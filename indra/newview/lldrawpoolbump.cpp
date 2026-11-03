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
#include "llsky.h"
#include "llstartup.h"
#include "lltextureentry.h"
#include "llviewercamera.h"
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
const U32 VERTEX_MASK_BUMP = LLVertexBuffer::MAP_VERTEX |LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXCOORD1;

U32 LLDrawPoolBump::sVertexMask = VERTEX_MASK_SHINY;


static LLGLSLShader* shader = NULL;
static S32 cube_channel = -1;
static S32 diffuse_channel = -1;
static S32 bump_channel = -1;
static bool shiny = false;

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
    shiny = false;
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


//static
void LLDrawPoolBump::bindCubeMap(LLGLSLShader* shader, S32 shader_level, S32& diffuse_channel, S32& cube_channel)
{
    LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
    if( cube_map && !LLPipeline::sReflectionProbesEnabled )
    {
        if (shader )
        {
            const LLMatrix4a& modelview = LLViewerCamera::getCurrent().getModelview();
            const LLMatrix4 mat = modelview.toMatrix4();
            LLVector3 vec = LLVector3(gShinyOrigin) * mat;
            LLVector4 vec4(vec, gShinyOrigin.mV[3]);
            shader->uniform4fv(LLViewerShaderMgr::SHINY_ORIGIN, 1, vec4.mV);
            if (shader_level > 1)
            {
                cube_map->setMatrix(1, modelview);
                // Make sure that texture coord generation happens for tex unit 1, as that's the one we use for
                // the cube map in the one pass shiny shaders
                cube_channel = shader->enableTexture(LLViewerShaderMgr::ENVIRONMENT_MAP);
                cube_map->enableTexture(cube_channel);
                diffuse_channel = shader->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
            }
            else
            {
                cube_map->setMatrix(0, modelview);
                cube_channel = shader->enableTexture(LLViewerShaderMgr::ENVIRONMENT_MAP);
                diffuse_channel = -1;
                cube_map->enable(cube_channel);
            }
            gGL.getTextureSlot(cube_channel)->bind(cube_map, ALSamplers::AnisoClamp);
        }
        else
        {
            cube_channel = 0;
            diffuse_channel = -1;
            gGL.getTextureSlot(0)->unbind();
            cube_map->enable(0);
            cube_map->setMatrix(0, LLViewerCamera::getCurrent().getModelview());
            gGL.getTextureSlot(0)->bind(cube_map, ALSamplers::AnisoClamp);
        }
    }
}

//static
void LLDrawPoolBump::unbindCubeMap(LLGLSLShader* shader, S32 shader_level, S32& diffuse_channel, S32& cube_channel)
{
    LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
    if( cube_map && !LLPipeline::sReflectionProbesEnabled)
    {
        if (shader_level > 1)
        {
            shader->disableTexture(LLViewerShaderMgr::ENVIRONMENT_MAP, ALTextureSlot::TT_CUBE_MAP);

            if (LLViewerShaderMgr::instance()->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT) > 0)
            {
                if (diffuse_channel != 0)
                {
                    shader->disableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
                }
            }
        }
        // Moved below shader->disableTexture call to avoid false alarms from auto-re-enable of textures on stage 0
        // MAINT-755
        cube_map->disable();
        cube_map->restoreMatrix();
    }
}

void LLDrawPoolBump::beginFullbrightShiny()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    sVertexMask = VERTEX_MASK_SHINY | LLVertexBuffer::MAP_TEXCOORD0;

    // Second pass: environment map
    shader = gDeferredFullbrightShinyProgram.selectVariant();
    if (LLPipeline::sRenderingHUDs)
    {
        shader = gHUDFullbrightShinyProgram.selectVariant();
    }

    if (mRigged)
    {
        llassert(shader->mRiggedVariant || !shader->isComplete());
        shader = shader->riggedVariant();
    }

    // bind exposure map so fullbright shader can cancel out exposure
    S32 channel = shader->enableTexture(LLShaderMgr::EXPOSURE_MAP);
    if (channel > -1)
    {
        gGL.getTextureSlot(channel)->bind(&gPipeline.mExposureMap);
    }

    LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;

    if (cube_map && !LLPipeline::sReflectionProbesEnabled)
    {
        // Make sure that texture coord generation happens for tex unit 1, as that's the one we use for
        // the cube map in the one pass shiny shaders
        gGL.getTextureSlot(1)->unbind();
        cube_channel = shader->enableTexture(LLViewerShaderMgr::ENVIRONMENT_MAP);
        cube_map->enableTexture(cube_channel);
        diffuse_channel = shader->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);

        gGL.getTextureSlot(cube_channel)->bind(cube_map, ALSamplers::AnisoClamp);
    }

    {
        const LLMatrix4 mat = LLViewerCamera::getCurrent().getModelview().toMatrix4();
        shader->bind();

        LLVector3 vec = LLVector3(gShinyOrigin) * mat;
        LLVector4 vec4(vec, gShinyOrigin.mV[3]);
        shader->uniform4fv(LLViewerShaderMgr::SHINY_ORIGIN, 1, vec4.mV);

        if (LLPipeline::sReflectionProbesEnabled)
        {
            gPipeline.bindReflectionProbes(*shader);
        }
        else
        {
            gPipeline.setEnvMat(*shader);
        }
    }

    if (mShaderLevel > 1)
    { //indexed texture rendering, channel 0 is always diffuse
        diffuse_channel = 0;
    }

    shiny = true;
}

void LLDrawPoolBump::renderFullbrightShiny()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    {
        LLGLEnable blend_enable(GL_BLEND);

        if (mShaderLevel > 1)
        {
            if (mRigged)
            {
                LLRenderPass::pushRiggedBatches(LLRenderPass::PASS_FULLBRIGHT_SHINY_RIGGED, true, true);
            }
            else
            {
                LLRenderPass::pushBatches(LLRenderPass::PASS_FULLBRIGHT_SHINY, true, true);
            }
        }
        else
        {
            if (mRigged)
            {
                LLRenderPass::pushRiggedBatches(LLRenderPass::PASS_FULLBRIGHT_SHINY_RIGGED);
            }
            else
            {
                LLRenderPass::pushBatches(LLRenderPass::PASS_FULLBRIGHT_SHINY);
            }
        }
    }
}

void LLDrawPoolBump::endFullbrightShiny()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
    if( cube_map && !LLPipeline::sReflectionProbesEnabled )
    {
        cube_map->disable();
        if (shader->mFeatures.hasReflectionProbes)
        {
            gPipeline.unbindReflectionProbes(*shader);
        }
        shader->unbind();
    }

    diffuse_channel = -1;
    cube_channel = 0;
    shiny = false;
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
bool LLDrawPoolBump::bindBumpMap(LLDrawInfo& params, S32 channel,
                                 LLViewerTexture** resolved)
{
    U8 bump_code = params.mBump;

    return bindBumpMap(bump_code, params.mTexture, channel, resolved);
}

//static
bool LLDrawPoolBump::bindBumpMap(LLFace* face, S32 channel)
{
    const LLTextureEntry* te = face->getTextureEntry();
    if (te)
    {
        U8 bump_code = te->getBumpmap();
        return bindBumpMap(bump_code, face->getTexture(), channel);
    }

    return false;
}

//static
bool LLDrawPoolBump::bindBumpMap(U8 bump_code, LLViewerTexture* texture,
                                 S32 channel, LLViewerTexture** resolved)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    if (resolved)
    {
        *resolved = nullptr;
    }
    //Note: texture atlas does not support bump texture now.
    LLViewerFetchedTexture* tex = LLViewerTextureManager::staticCastToFetchedTexture(texture) ;
    if(!tex)
    {
        //if the texture is not a fetched texture
        return false;
    }

    LLViewerTexture* bump = NULL;

    switch( bump_code )
    {
    case BE_NO_BUMP:
        break;
    case BE_BRIGHTNESS:
    case BE_DARKNESS:
        bump = gBumpImageList.getBrightnessDarknessImage( tex, bump_code );
        break;

    default:
        if( bump_code < LLStandardBumpmap::sStandardBumpmapCount )
        {
            bump = gStandardBumpmapList[bump_code].mImage;
            gBumpImageList.addTextureStats(bump_code, tex->getID(), tex->getMaxVirtualSize());
        }
        break;
    }

    if (bump)
    {
        if (resolved)
        {
            *resolved = bump;
        }
        if (channel == -2)
        {
            gGL.getTextureSlot(1)->bindFast(bump, ALSamplers::AnisoWrap);
            gGL.getTextureSlot(0)->bindFast(bump, ALSamplers::AnisoWrap);
        }
        else
        {
            // NOTE: do not use bindFast here (see SL-16222)
            gGL.getTextureSlot(channel)->bindSampled(bump, ALSamplers::AnisoWrap);
        }

        return true;
    }

    return false;
}

//static
void LLDrawPoolBump::beginBump()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    sVertexMask = VERTEX_MASK_BUMP;
    // Optional second pass: emboss bump map
    stop_glerror();

    shader = gObjectBumpProgram.selectVariant();

    if (mRigged)
    {
        llassert(shader->mRiggedVariant || !shader->isComplete());
        shader = shader->riggedVariant();
    }

    shader->bind();

    gGL.setSceneBlendType(LLRender::BT_MULT_X2);
    stop_glerror();
}

//static
void LLDrawPoolBump::renderBump(U32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
    LLGLEnable blend(GL_BLEND);
    gGL.diffuseColor4f(1,1,1,1);
    /// Get rid of z-fighting with non-bump pass.
    LLGLEnable polyOffset(GL_POLYGON_OFFSET_FILL);
    gGL.setPolygonOffset(-1.0f, -1.0f);
    pushBumpBatches(pass);
}

//static
void LLDrawPoolBump::endBump(U32 pass)
{
    LLGLSLShader::unbind();

    gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

S32 LLDrawPoolBump::getNumDeferredPasses()
{
    return 1;
}

// Whether a bump face's texture can fall below its cutoff. Without an alpha channel the texture reads 1 and the face
// is never discarded; a texture whose channels are not yet known is taken to have one.
static bool bump_can_discard(const LLDrawInfo& params)
{
    if (params.mAlphaMaskCutoff <= 0.f)
    {
        return false;
    }
    const LLViewerTexture* tex = params.mTexture.get();
    return !(tex && tex->hasGLTexture() && tex->getComponents() == 3);
}

void LLDrawPoolBump::renderDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    // The program's LINEAR_DIFFUSE makes pushBumpBatch decode the diffuse on the sampler;
    // the hoisted GL_FRAMEBUFFER_SRGB (renderGeomDeferred) re-encodes on store.

    shiny = true;
    for (int i = 0; i < 2; ++i)
    {
        bool rigged = i == 1;

        U32 type = rigged ? LLRenderPass::PASS_BUMP_RIGGED : LLRenderPass::PASS_BUMP;
        LLCullResult::drawinfo_iterator begin = gPipeline.beginRenderMap(type);
        LLCullResult::drawinfo_iterator end = gPipeline.endRenderMap(type);
        if (begin == end)
        {   // no bump geometry in this pass -- skip the shader bind and texture setup
            continue;
        }

        // The faces that cannot be discarded first, under the program without the test, so their depth is written
        // ahead of shading; the rest are set aside as the list is walked, once, and drawn after under the one with it.
        // A program with no faces to draw is never bound.
        static std::vector<LLDrawInfo*> masked;
        masked.clear();
        for (bool alpha_mask : { false, true })
        {
            LLGLSLShader* shader = nullptr;

            const LLVOAvatar* lastAvatar = nullptr;
            U64 lastMeshId = 0;
            bool skipLastSkin = false;

            // Faces are sorted by bumpmap then texture, so the alpha-mask cutoff and the
            // bump-image bind (an image lookup + texture bind) repeat across runs of faces.
            // Skip them when unchanged. (bindBumpMap's only side effect, addTextureStats, is
            // max-based on the source texture, so skipping a repeat is a no-op there too.)
            U8 lastBump = 255;
            LLViewerTexture* lastBumpTex = nullptr;
            LLViewerTexture* lastBumpMap = nullptr;
            F32 lastAlpha = -1.f;

#if LL_DARWIN
            gPipeline.beginMetalWorldGBufferPass(rigged, alpha_mask);
#endif

            const auto draw = [&](LLDrawInfo& params)
            {
                if (!shader)
                {
                    LLGLSLShader& program = alpha_mask ? gDeferredBumpAlphaMaskProgram : gDeferredBumpProgram;
                    program.selectVariant()->bind(rigged);
                    shader = LLGLSLShader::sCurBoundShaderPtr;
                    diffuse_channel = shader->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
                    bump_channel = shader->enableTexture(LLViewerShaderMgr::BUMP_MAP);
                    gGL.getTextureSlot(diffuse_channel)->unbind();
                    gGL.getTextureSlot(bump_channel)->unbind();
                }

                if (alpha_mask && params.mAlphaMaskCutoff != lastAlpha)
                {
                    lastAlpha = params.mAlphaMaskCutoff;
                    shader->setMinimumAlpha(lastAlpha);
                }

                if (params.mBump != lastBump || params.mTexture.get() != lastBumpTex)
                {
                    lastBump = params.mBump;
                    lastBumpTex = params.mTexture.get();
                    LLDrawPoolBump::bindBumpMap(
                        params, bump_channel, &lastBumpMap);
                }

                if (rigged)
                {
                    if (uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
                    {
                        pushBumpBatch(params, true, false, lastBumpMap);
                    }
                }
                else
                {
                    pushBumpBatch(params, true, false, lastBumpMap);
                }
            };

            if (!alpha_mask)
            {
                for (LLCullResult::drawinfo_iterator i = begin; i != end; )
                {
                    LLDrawInfo& params = **i;

                    LLCullResult::increment_iterator(i, end);

                    if (bump_can_discard(params))
                    {
                        masked.push_back(&params);
                        continue;
                    }
                    draw(params);
                }
            }
            else
            {
                for (LLDrawInfo* params : masked)
                {
                    draw(*params);
                }
            }

            if (shader)
            {
                shader->disableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
                shader->disableTexture(LLViewerShaderMgr::BUMP_MAP);
                shader->unbind();
            }
#if LL_DARWIN
            gPipeline.endMetalWorldGBufferPass();
#endif
        }
    }

    shiny = false;
}


void LLDrawPoolBump::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    S32 num_passes = LLPipeline::sRenderingHUDs ? 1 : 2; // skip rigged pass when rendering HUDs

    for (int i = 0; i < num_passes; ++i)
    { // two passes -- static and rigged
        mRigged = (i == 1);

        // render shiny
        beginFullbrightShiny();
#if LL_DARWIN
        gPipeline.beginMetalImpostorShinyPass(mRigged);
#endif
        renderFullbrightShiny();
#if LL_DARWIN
        gPipeline.endMetalWorldGBufferPass();
#endif
        endFullbrightShiny();

        //render bump
        beginBump();
#if LL_DARWIN
        gPipeline.beginMetalImpostorEmbossPass(mRigged);
#endif
        renderBump(LLRenderPass::PASS_POST_BUMP);
#if LL_DARWIN
        gPipeline.endMetalWorldGBufferPass();
#endif
        endBump();
    }
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
#if LL_DARWIN
        return image->getGLTexture()->getMetalTexture() != nullptr;
#else
        return image->hasGLTexture();
#endif
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
                    image->destroyGLTexture();
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
                    image->destroyGLTexture();
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
        ALMetalDevice* metal_device = LLImageGL::getMetalDevice();
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

void LLDrawPoolBump::pushBumpBatches(U32 type)
{
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    if (mRigged)
    { // nudge type enum and include skinweights for rigged pass
        type += 1;
    }

    LLCullResult::drawinfo_iterator begin = gPipeline.beginRenderMap(type);
    LLCullResult::drawinfo_iterator end = gPipeline.endRenderMap(type);

    for (LLCullResult::drawinfo_iterator i = begin; i != end; ++i)
    {
        LLDrawInfo& params = **i;

        LLViewerTexture* bump_texture = nullptr;
        if (LLDrawPoolBump::bindBumpMap(
                params, -2, &bump_texture))
        {
            if (mRigged)
            {
                if (!uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
                {
                    continue;
                }
            }
            pushBumpBatch(params, false, false, bump_texture);
        }
    }
}

void LLRenderPass::pushBumpBatch(LLDrawInfo& params, bool texture,
                                 bool batch_textures,
                                 LLViewerTexture* bump_texture)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    applyModelMatrix(params);

#if LL_DARWIN
    gPipeline.captureMetalWorldBumpBatch(params, bump_texture);
#endif

    // Same derivation as pushBatch: the bound program knows whether its diffuse wants the
    // hardware decode. The shadow pass reaches this too with an unflagged program, so its
    // binds stay plain AnisoWrap.
    const LLGLSLShader* bound = LLGLSLShader::sCurBoundShaderPtr;
    ALSampler key = ALSamplers::AnisoWrap;
    if (bound && bound->mLinearDiffuse)
    {
        key |= ALSampler::SRGBDecode;
    }

    bool tex_setup = false;

    if (batch_textures && params.mTextureList.size() > 1)
    {
        // Clamped to the bound program's ladder and null slots stood in for, same as every
        // other indexed pass -- a raw loop here would bind past the last declared sampler
        // whenever a batch was built at a wider ladder than the current program's.
        bindIndexedTextures(params, bound);
    }
    else
    { //not batching textures or batch has only 1 texture -- might need a texture matrix
        if (params.mTextureMatrix && (!gCubeSnapshot || gPipeline.mHeroProbeManager.isMirrorPass()))
        {
            // the non-shiny path used to load this twice -- a leftover from when the
            // second (bump) texture unit carried its own copy of the matrix
            gGL.matrixMode(LLRender::MM_TEXTURE0);
            gGL.loadMatrix((GLfloat*) params.mTextureMatrix->mMatrix);
            gPipeline.countTextureMatrixOp(*params.mTextureMatrix);

            tex_setup = true;
        }

        if (shiny && mShaderLevel > 1 && texture)
        {
            if (params.mTexture.notNull())
            {
                gGL.getTextureSlot(diffuse_channel)->bindFast(params.mTexture, key);
            }
            else
            {
                gGL.getTextureSlot(diffuse_channel)->unbind();
            }
        }
    }

    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);

    if (tex_setup)
    {
        // Both branches selected the texture matrix stack after activating unit 0; only the
        // activate differed, and it was doing nothing either way.
        gGL.matrixMode(LLRender::MM_TEXTURE0);
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
}

