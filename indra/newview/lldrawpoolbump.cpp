/**
 * @file lldrawpoolbump.cpp
 * @brief LLDrawPoolBump class implementation
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "lldrawpoolbump.h"

#include "llstl.h"
#include "llviewercontrol.h"
#include "lldir.h"
#include "m3math.h"
#include "m4math.h"
#include "v4math.h"
#include "llglheaders.h"
#include "llrender.h"

#include "llcubemap.h"
#include "lldrawable.h"
#include "llface.h"
#include "llstartup.h"
#include "lltextureentry.h"
#include "llviewercamera.h"
#include "llviewerwindow.h"
#include "llviewertexturelist.h"
#include "pipeline.h"
#include "llspatialpartition.h"
#include "llviewershadermgr.h"
#include "llmodel.h"

#if LL_DARWIN
#include "../almetal/almetaltextureops.h"
#endif

extern bool gCubeSnapshot;

//#include "llimagebmp.h"
//#include "../tools/imdebug/imdebug.h"

// static
LLStandardBumpmap gStandardBumpmapList[TEM_BUMPMAP_COUNT];

// static
U32 LLStandardBumpmap::sStandardBumpmapCount = 0;

// static
LLBumpImageList gBumpImageList;

const S32 STD_BUMP_LATEST_FILE_VERSION = 1;

const U32 VERTEX_MASK_SHINY = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_NORMAL | LLVertexBuffer::MAP_COLOR;

U32 LLDrawPoolBump::sVertexMask = VERTEX_MASK_SHINY;

#if LL_DARWIN
namespace
{
    const ALMetalDevice* sMetalNormalMapDevice = nullptr;
    std::unique_ptr<ALMetalNormalMapGenerator> sMetalNormalMapGenerator;
}
#endif

// static
void LLStandardBumpmap::restoreResources()
{
    addstandard();
}

// static
void LLStandardBumpmap::addstandard()
{
    if(!gTextureList.isInitialized())
    {
        //Note: loading pre-configuration sometimes triggers this call.
        //But it is safe to return here because bump images will be reloaded during initialization later.
        return ;
    }

    if (LLStartUp::getStartupState() < STATE_SEED_CAP_GRANTED)
    {
        // Not ready, need caps for images
        return;
    }

    // Startup resource rebuilding can populate this list before normal viewer
    // initialization, so clear it before adding the standard bump maps again.
    //llassert( LLStandardBumpmap::sStandardBumpmapCount == 0 );
    clear();
    LL_INFOS() << "Adding standard bumpmaps." << LL_ENDL;
    gStandardBumpmapList[LLStandardBumpmap::sStandardBumpmapCount++] = LLStandardBumpmap("None");       // BE_NO_BUMP
    gStandardBumpmapList[LLStandardBumpmap::sStandardBumpmapCount++] = LLStandardBumpmap("Brightness"); // BE_BRIGHTNESS
    gStandardBumpmapList[LLStandardBumpmap::sStandardBumpmapCount++] = LLStandardBumpmap("Darkness");   // BE_DARKNESS

    std::string file_name = gDirUtilp->getExpandedFilename( LL_PATH_APP_SETTINGS, "std_bump.ini" );
    LLFILE* file = LLFile::fopen( file_name, LLFILE_MODE("rt") );     /*Flawfinder: ignore*/
    if( !file )
    {
        LL_WARNS() << "Could not open std_bump <" << file_name << ">" << LL_ENDL;
        return;
    }

    S32 file_version = 0;

    S32 fields_read = fscanf( file, "LLStandardBumpmap version %d", &file_version );
    if( fields_read != 1 )
    {
        LL_WARNS() << "Bad LLStandardBumpmap header" << LL_ENDL;
        return;
    }

    if( file_version > STD_BUMP_LATEST_FILE_VERSION )
    {
        LL_WARNS() << "LLStandardBumpmap has newer version (" << file_version << ") than viewer (" << STD_BUMP_LATEST_FILE_VERSION << ")" << LL_ENDL;
        return;
    }

    while( !feof(file) && (LLStandardBumpmap::sStandardBumpmapCount < (U32)TEM_BUMPMAP_COUNT) )
    {
        // *NOTE: This buffer size is hard coded into scanf() below.
        char label[2048] = "";  /* Flawfinder: ignore */
        char bump_image_id[2048] = "";  /* Flawfinder: ignore */
        fields_read = fscanf(   /* Flawfinder: ignore */
            file, "\n%2047s %2047s", label, bump_image_id);
        if( EOF == fields_read )
        {
            break;
        }
        if( fields_read != 2 )
        {
            LL_WARNS() << "Bad LLStandardBumpmap entry" << LL_ENDL;
            return;
        }

//      LL_INFOS() << "Loading bumpmap: " << bump_image_id << " from viewerart" << LL_ENDL;
        gStandardBumpmapList[LLStandardBumpmap::sStandardBumpmapCount].mLabel = label;
        gStandardBumpmapList[LLStandardBumpmap::sStandardBumpmapCount].mImage =
            LLViewerTextureManager::getFetchedTexture(LLUUID(bump_image_id));
        gStandardBumpmapList[LLStandardBumpmap::sStandardBumpmapCount].mImage->setBoostLevel(LLGLTexture::LOCAL) ;
        gStandardBumpmapList[LLStandardBumpmap::sStandardBumpmapCount].mImage->setLoadedCallback(LLBumpImageList::onSourceStandardLoaded, 0, true, false, NULL, NULL );
        gStandardBumpmapList[LLStandardBumpmap::sStandardBumpmapCount].mImage->forceToSaveRawImage(0, 30.f) ;
        LLStandardBumpmap::sStandardBumpmapCount++;
    }

    fclose( file );
}

// static
void LLStandardBumpmap::clear()
{
    LL_INFOS() << "Clearing standard bumpmaps." << LL_ENDL;
    for( U32 i = 0; i < LLStandardBumpmap::sStandardBumpmapCount; i++ )
    {
        gStandardBumpmapList[i].mLabel.assign("");
        gStandardBumpmapList[i].mImage = NULL;
    }
    sStandardBumpmapCount = 0;
}

////////////////////////////////////////////////////////////////

LLDrawPoolBump::LLDrawPoolBump()
:  LLRenderPass(LLDrawPool::POOL_BUMP)
{
}
void LLDrawPoolBump::prerender()
{
    mShaderLevel = LLViewerShaderMgr::instance()->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

// static
S32 LLDrawPoolBump::numBumpPasses()
{
    return 1;
}


void LLDrawPoolBump::renderGroup(LLSpatialGroup* group, U32 type, bool texture = true)
{
    LLSpatialGroup::drawmap_elem_t& draw_info = group->mDrawMap[type];

    for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(); k != draw_info.end(); ++k)
    {
        LLDrawInfo& params = **k;

        applyModelMatrix(params);

        params.mVertexBuffer->setBuffer();
        params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
    }
}


// static
LLViewerTexture* LLDrawPoolBump::resolveBumpMap(
    const LLDrawInfo& params)
{
    return resolveBumpMap(params.mBump, params.mTexture);
}

// static
LLViewerTexture* LLDrawPoolBump::resolveBumpMap(
    U8 bump_code, LLViewerTexture* texture)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    // Texture atlases do not support legacy bump textures. This lookup owns
    // no GL state; generated brightness/darkness images publish through the
    // native texture path on Darwin.
    LLViewerFetchedTexture* tex =
        LLViewerTextureManager::staticCastToFetchedTexture(texture);
    if (!tex)
    {
        return nullptr;
    }

    switch (bump_code)
    {
    case BE_NO_BUMP:
        return nullptr;
    case BE_BRIGHTNESS:
    case BE_DARKNESS:
        return gBumpImageList.getBrightnessDarknessImage(tex, bump_code);
    default:
        if (bump_code < LLStandardBumpmap::sStandardBumpmapCount)
        {
            gBumpImageList.addTextureStats(
                bump_code, tex->getID(), tex->getMaxVirtualSize());
            return gStandardBumpmapList[bump_code].mImage;
        }
        return nullptr;
    }
}

S32 LLDrawPoolBump::getNumDeferredPasses()
{
    return 1;
}

// Whether a bump face's texture can fall below its cutoff. Without an alpha channel the texture reads 1 and the face
// is never discarded; a texture whose channels are not yet known is taken to have one.
bool LLDrawPoolBump::bumpCanDiscard(const LLDrawInfo& params)
{
    if (params.mAlphaMaskCutoff <= 0.f)
    {
        return false;
    }
    const LLViewerTexture* tex = params.mTexture.get();
    return !(tex && tex->hasGPUTexture() && tex->getComponents() == 3);
}

void LLDrawPoolBump::renderDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    (void)pass;

#if LL_DARWIN
    for (int i = 0; i < 2; ++i)
    {
        const bool rigged = i == 1;

        const U32 type = rigged
            ? LLRenderPass::PASS_BUMP_RIGGED
            : LLRenderPass::PASS_BUMP;
        auto* begin = gPipeline.beginRenderMap(type);
        auto* end = gPipeline.endRenderMap(type);
        if (begin == end)
        {
            continue;
        }

        // Preserve the retained pool's opaque-first then alpha-mask order,
        // but enumerate both sets directly into the Metal G-buffer.
        static std::vector<LLDrawInfo*> masked;
        masked.clear();
        for (bool alpha_mask : { false, true })
        {
            U8 lastBump = 255;
            LLViewerTexture* lastBumpTex = nullptr;
            LLViewerTexture* lastBumpMap = nullptr;

            gPipeline.beginMetalWorldGBufferPass(rigged, alpha_mask);
            const auto capture = [&](LLDrawInfo& params)
            {
                if (params.mBump != lastBump || params.mTexture.get() != lastBumpTex)
                {
                    lastBump = params.mBump;
                    lastBumpTex = params.mTexture.get();
                    lastBumpMap = LLDrawPoolBump::resolveBumpMap(params);
                }
                gPipeline.captureMetalWorldBumpBatch(params, lastBumpMap);
            };

            if (!alpha_mask)
            {
                for (auto* at = begin; at != end; )
                {
                    LLDrawInfo& params = **at;
                    LLCullResult::increment_iterator(at, end);

                    if (bumpCanDiscard(params))
                    {
                        masked.push_back(&params);
                        continue;
                    }
                    capture(params);
                }
            }
            else
            {
                for (LLDrawInfo* params : masked)
                {
                    capture(*params);
                }
            }

            gPipeline.endMetalWorldGBufferPass();
        }
    }

#endif
}


void LLDrawPoolBump::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    (void)pass;

    if (LLPipeline::sRenderingHUDs)
    {
#if LL_DARWIN
        // HUD post-deferred replay is part of the authoritative offscreen
        // Metal transaction. If that transaction is unavailable, fail closed
        // instead of restoring an application OpenGL draw.
        if (!gGL.isMetalUITextureCaptureAuthoritative())
        {
            return;
        }

        mRigged = false;
        {
            LLGLEnable blend(GL_BLEND);
            auto* at = gPipeline.beginRenderMap(
                LLRenderPass::PASS_FULLBRIGHT_SHINY);
            auto* end = gPipeline.endRenderMap(
                LLRenderPass::PASS_FULLBRIGHT_SHINY);
            while (at != end)
            {
                LLDrawInfo* info = *at;
                LLCullResult::increment_iterator(at, end);
                if (!info)
                {
                    continue;
                }
                applyModelMatrix(*info);
                captureMetalHUDBatch(
                    *info, mShaderLevel > 1,
                    false, false, false, 0.f, false, true);
            }
        }

        {
            LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
            LLGLEnable blend(GL_BLEND);
            gGL.setSceneBlendType(LLRender::BT_MULT_X2);
            auto* at = gPipeline.beginRenderMap(
                LLRenderPass::PASS_POST_BUMP);
            auto* end = gPipeline.endRenderMap(
                LLRenderPass::PASS_POST_BUMP);
            while (at != end)
            {
                LLDrawInfo* info = *at;
                LLCullResult::increment_iterator(at, end);
                if (!info)
                {
                    continue;
                }
                LLViewerTexture* bump = resolveBumpMap(*info);
                if (!bump)
                {
                    continue;
                }
                applyModelMatrix(*info);
                captureMetalHUDEmbossBatch(*info, bump);
            }
            gGL.setSceneBlendType(LLRender::BT_ALPHA);
        }
#endif
        return;
    }

#if LL_DARWIN
    // The world owner is native-only: enumerate the same static then rigged
    // shiny/emboss draw maps without binding a shader, texture unit, blend
    // state, matrix stack, vertex array, or issuing an OpenGL draw.
    for (bool rigged : { false, true })
    {
        mRigged = rigged;

        const U32 shiny_type = rigged
            ? LLRenderPass::PASS_FULLBRIGHT_SHINY_RIGGED
            : LLRenderPass::PASS_FULLBRIGHT_SHINY;
        gPipeline.beginMetalImpostorShinyPass(rigged);
        auto* shiny_at = gPipeline.beginRenderMap(shiny_type);
        auto* shiny_end = gPipeline.endRenderMap(shiny_type);
        while (shiny_at != shiny_end)
        {
            LLDrawInfo* info = *shiny_at;
            LLCullResult::increment_iterator(shiny_at, shiny_end);
            if (info)
            {
                gPipeline.captureMetalWorldGBufferBatch(*info, true);
            }
        }
        gPipeline.endMetalWorldGBufferPass();

        const U32 emboss_type = rigged
            ? LLRenderPass::PASS_POST_BUMP_RIGGED
            : LLRenderPass::PASS_POST_BUMP;
        gPipeline.beginMetalImpostorEmbossPass(rigged);
        auto* emboss_at = gPipeline.beginRenderMap(emboss_type);
        auto* emboss_end = gPipeline.endRenderMap(emboss_type);
        while (emboss_at != emboss_end)
        {
            LLDrawInfo* info = *emboss_at;
            LLCullResult::increment_iterator(emboss_at, emboss_end);
            if (info)
            {
                gPipeline.captureMetalWorldBumpBatch(
                    *info, LLDrawPoolBump::resolveBumpMap(*info));
            }
        }
        gPipeline.endMetalWorldGBufferPass();
    }
#endif
}


////////////////////////////////////////////////////////////////
// List of bump-maps created from other textures.


//const LLUUID TEST_BUMP_ID("3d33eaf2-459c-6f97-fd76-5fce3fc29447");

void LLBumpImageList::init()
{
    llassert( mBrightnessEntries.size() == 0 );
    llassert( mDarknessEntries.size() == 0 );

    LLStandardBumpmap::restoreResources();
}

void LLBumpImageList::clear()
{
    LL_INFOS() << "Clearing dynamic bumpmaps." << LL_ENDL;
    // these will be re-populated on-demand
    mBrightnessEntries.clear();
    mDarknessEntries.clear();

    LLStandardBumpmap::clear();
}

void LLBumpImageList::shutdown()
{
    clear();
}

void LLBumpImageList::restoreResources()
{
    if(!gTextureList.isInitialized())
    {
        //safe to return here because bump images will be reloaded during initialization later.
        return ;
    }

    LLStandardBumpmap::restoreResources();
    // Images will be recreated as they are needed.
}


LLBumpImageList::~LLBumpImageList()
{
    // Shutdown should have already been called.
    llassert( mBrightnessEntries.size() == 0 );
    llassert( mDarknessEntries.size() == 0 );
}


// Note: Does nothing for entries in gStandardBumpmapList that are not actually standard bump images (e.g. none, brightness, and darkness)
void LLBumpImageList::addTextureStats(U8 bump, const LLUUID& base_image_id, F32 virtual_size)
{
    bump &= TEM_BUMP_MASK;
    LLViewerFetchedTexture* bump_image = gStandardBumpmapList[bump].mImage;
    if( bump_image )
    {
        bump_image->addTextureStats(virtual_size);
    }
}


void LLBumpImageList::updateImages()
{
    llassert(LLCoros::on_main_thread_main_coro()); // This code is not thread safe

    const auto has_backing = [](LLViewerTexture* image)
    {
        return image && image->hasGPUTexture();
    };

    for (bump_image_map_t::iterator iter = mBrightnessEntries.begin(); iter != mBrightnessEntries.end(); )
    {
        LLViewerTexture* image = iter->second;
        if( image )
        {
            bool destroy = true;
            if (has_backing(image))
            {
                if( image->getBoundRecently() )
                {
                    destroy = false;
                }
                else
                {
                    image->destroyGPUTexture();
                }
            }

            if( destroy )
            {
                //LL_INFOS() << "*** Destroying bright " << (void*)image << LL_ENDL;
                iter = mBrightnessEntries.erase(iter);   // deletes the image thanks to reference counting
                continue;
            }
        }
        ++iter;
    }

    for (bump_image_map_t::iterator iter = mDarknessEntries.begin(); iter != mDarknessEntries.end(); )
    {
        bump_image_map_t::iterator curiter = iter++;
        LLViewerTexture* image = curiter->second;
        if( image )
        {
            bool destroy = true;
            if (has_backing(image))
            {
                if( image->getBoundRecently() )
                {
                    destroy = false;
                }
                else
                {
                    image->destroyGPUTexture();
                }
            }

            if( destroy )
            {
                //LL_INFOS() << "*** Destroying dark " << (void*)image << LL_ENDL;;
                mDarknessEntries.erase(curiter);  // deletes the image thanks to reference counting
            }
        }
    }
}


// Note: the caller SHOULD NOT keep the pointer that this function returns.  It may be updated as more data arrives.
LLViewerTexture* LLBumpImageList::getBrightnessDarknessImage(LLViewerFetchedTexture* src_image, U8 bump_code )
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    llassert( (bump_code == BE_BRIGHTNESS) || (bump_code == BE_DARKNESS) );

    LLViewerTexture* bump = nullptr;

    bump_image_map_t* entries_list = nullptr;

    switch( bump_code )
    {
    case BE_BRIGHTNESS:
        entries_list = &mBrightnessEntries;
        break;
    case BE_DARKNESS:
        entries_list = &mDarknessEntries;
        break;
    default:
        llassert(0);
        return nullptr;
    }

    bump_image_map_t::iterator iter = entries_list->find(src_image->getID());
    if (iter != entries_list->end() && iter->second.notNull())
    {
        bump = iter->second;
    }

    bool native_missing = false;
#if LL_DARWIN
    native_missing = bump && !bump->getGLTexture()->getMetalTexture();
#endif
    if (bump == nullptr || native_missing ||
        src_image->getWidth() != bump->getWidth() ||
        src_image->getHeight() != bump->getHeight())
    {
        onSourceUpdated(src_image, (EBumpEffect) bump_code);
    }

    return (*entries_list)[src_image->getID()];
}


void LLBumpImageList::onSourceStandardLoaded( bool success, LLViewerFetchedTexture* src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, bool final, void* userdata)
{
    if (success && LLPipeline::sRenderDeferred)
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
        LLPointer<LLImageRaw> nrm_image = new LLImageRaw(src->getWidth(), src->getHeight(), 4);
        {
            generateNormalMapFromAlpha(src, nrm_image);
        }
        src_vi->setExplicitFormat(GL_RGBA8, GL_RGBA);
        {
#if LL_DARWIN
            const bool created = src_vi->createMetalTexture(
                src_vi->getDiscardLevel(), nrm_image, LLGLTexture::LOCAL);
#else
            const bool created = false;
#endif
            if (!created)
            {
                LL_WARNS() << "Failed to create bump image texture for image " << src_vi->getID() << LL_ENDL;
            }
        }
    }
}

void LLBumpImageList::generateNormalMapFromAlpha(LLImageRaw* src, LLImageRaw* nrm_image)
{
    LLImageDataCopyLock lock(src, nrm_image);

    U8* nrm_data = nrm_image->getData();
    S32 resX = src->getWidth();
    S32 resY = src->getHeight();

    const U8* src_data = src->getData();

    S32 src_cmp = src->getComponents();

    F32 norm_scale = gSavedSettings.getF32("RenderNormalMapScale");

    U32 idx = 0;
    //generate normal map from pseudo-heightfield
    for (S32 j = 0; j < resY; ++j)
    {
        for (S32 i = 0; i < resX; ++i)
        {
            S32 rX = (i+1)%resX;
            S32 rY = (j+1)%resY;
            S32 lX = (i-1)%resX;
            S32 lY = (j-1)%resY;

            if (lX < 0)
            {
                lX += resX;
            }
            if (lY < 0)
            {
                lY += resY;
            }

            F32 cH = (F32) src_data[(j*resX+i)*src_cmp+src_cmp-1];

            LLVector3 right = LLVector3(norm_scale, 0, (F32) src_data[(j*resX+rX)*src_cmp+src_cmp-1]-cH);
            LLVector3 left = LLVector3(-norm_scale, 0, (F32) src_data[(j*resX+lX)*src_cmp+src_cmp-1]-cH);
            LLVector3 up = LLVector3(0, -norm_scale, (F32) src_data[(lY*resX+i)*src_cmp+src_cmp-1]-cH);
            LLVector3 down = LLVector3(0, norm_scale, (F32) src_data[(rY*resX+i)*src_cmp+src_cmp-1]-cH);

            LLVector3 norm = right%down + down%left + left%up + up%right;

            norm.normVec();

            norm *= 0.5f;
            norm += LLVector3(0.5f,0.5f,0.5f);

            idx = (j*resX+i)*4;
            nrm_data[idx+0]= (U8) (norm.mV[0]*255);
            nrm_data[idx+1]= (U8) (norm.mV[1]*255);
            nrm_data[idx+2]= (U8) (norm.mV[2]*255);
            nrm_data[idx+3]= src_data[(j*resX+i)*src_cmp+src_cmp-1];
        }
    }
}

// static
void LLBumpImageList::onSourceUpdated(LLViewerTexture* src, EBumpEffect bump_code)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    const LLUUID& src_id = src->getID();

    bump_image_map_t& entries_list(bump_code == BE_BRIGHTNESS ? gBumpImageList.mBrightnessEntries : gBumpImageList.mDarknessEntries);
    bump_image_map_t::iterator iter = entries_list.find(src_id);

    if (iter == entries_list.end())
    { //make sure an entry exists for this image
        entries_list[src_id] = LLViewerTextureManager::getLocalTexture(true);
        iter = entries_list.find(src_id);
    }

    //---------------------------------------------------
    // immediately assign bump to a smart pointer in case some local smart pointer
    // accidentally releases it.
    LLPointer<LLViewerTexture> bump = iter->second;

    bool needs_update = bump->getWidth() != src->getWidth() ||
                        bump->getHeight() != src->getHeight();
#if LL_DARWIN
    needs_update = needs_update || !bump->getGLTexture()->getMetalTexture();
#endif
    if (needs_update) // bump not cached yet or has changed resolution/backing
    {
        //convert to normal map
        LL_PROFILE_ZONE_NAMED("bil - create normal map");

        bump->setExplicitFormat(GL_RGBA8, GL_RGBA);

        LLImageGL* dst_img = bump->getGLTexture();
        if (!dst_img->setSize(src->getWidth(), src->getHeight(), 4, 0))
        {
            LL_WARNS() << "Failed to setSize for image " << bump->getID() << LL_ENDL;
        }
        dst_img->setUseMipMaps(true);
        dst_img->setDiscardLevel(0);

#if LL_DARWIN
        ALMetalDevice* metal_device = getViewerMetalDevice();
        LLImageGL* source_image = src->getGLTexture();
        ALMetalTexture* metal_source = source_image
            ? source_image->getMetalTexture() : nullptr;
        if (!metal_device || !metal_source)
        {
            // A fetched source may publish on the next frame. Keep the local
            // entry without fallback storage so getBrightnessDarknessImage()
            // retries instead of sampling stale or empty pixels.
            return;
        }

        std::string error;
        if (sMetalNormalMapDevice != metal_device)
        {
            sMetalNormalMapGenerator =
                ALMetalNormalMapGenerator::create(*metal_device, &error);
            sMetalNormalMapDevice = metal_device;
        }

        auto metal_bump = dst_img->createMetalRenderTexture(&error);
        auto command = metal_bump
            ? metal_device->makeCommandBuffer(
                  "Alchemy legacy bump-map generation", &error)
            : nullptr;
        bool encoded = command && sMetalNormalMapGenerator &&
            sMetalNormalMapGenerator->encode(
                *command, *metal_source, *metal_bump,
                gSavedSettings.getF32("RenderNormalMapScale"),
                bump_code == BE_DARKNESS, &error) &&
            metal_bump->generateMipmaps(*command, &error) &&
            command->commit(&error);
        if (encoded &&
            !dst_img->adoptMetalRenderTextureAuthoritative(
                std::move(metal_bump), LLGLTexture::LOCAL))
        {
            error = "render-produced texture descriptor did not match the "
                    "native bump-map image";
            encoded = false;
        }
        if (!encoded)
        {
            LL_WARNS("Metal")
                << "Could not generate native legacy bump map: "
                << error << LL_ENDL;
            return;
        }

        LL_INFOS_ONCE("Metal")
            << "Native-only brightness/darkness bump-map generation is active"
            << LL_ENDL;
#else
        LL_WARNS_ONCE("Metal")
            << "Legacy bump-map generation requires the native Metal renderer"
            << LL_ENDL;
        return;
#endif
    }

    iter->second = bump; // derefs (and deletes) old image
    //---------------------------------------------------
}
