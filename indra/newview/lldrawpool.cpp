/**
 * @file lldrawpool.cpp
 * @brief LLDrawPool class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#include "lldrawpool.h"
#include "llrender.h"
#include "llviewercontrol.h"

#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolavatar.h"
#include "lldrawpoolbump.h"
#include "lldrawpoolmaterials.h"
#include "lldrawpoolpbropaque.h"
#include "lldrawpoolsimple.h"
#include "lldrawpoolsky.h"
#include "lldrawpooltree.h"
#include "lldrawpoolterrain.h"
#include "lldrawpoolwater.h"
#include "lldrawpoolwaterexclusion.h"
#include "llface.h"
#include "llviewerobjectlist.h" // For debug listing.
#include "pipeline.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "lldrawpoolwlsky.h"
#include "llglslshader.h"
#include "llglcommonfunc.h"
#include "llimagegl.h"
#include "llvoavatar.h"
#include "llviewershadermgr.h"
#include "llfetchedgltfmaterial.h"
#include "llviewertexture.h"

#if LL_DARWIN
#include "../almetal/almetaluirenderer.h"
#endif

extern bool gCubeSnapshot;

S32 LLDrawPool::sNumDrawPools = 0;

//=============================
// Draw Pool Implementation
//=============================
LLDrawPool *LLDrawPool::createPool(const U32 type, LLViewerTexture *tex0)
{
    LLDrawPool *poolp = NULL;
    switch (type)
    {
    case POOL_SIMPLE:
        poolp = new LLDrawPoolSimple();
        break;
    case POOL_GRASS:
        poolp = new LLDrawPoolGrass();
        break;
    case POOL_ALPHA_MASK:
        poolp = new LLDrawPoolAlphaMask();
        break;
    case POOL_FULLBRIGHT_ALPHA_MASK:
        poolp = new LLDrawPoolFullbrightAlphaMask();
        break;
    case POOL_FULLBRIGHT:
        poolp = new LLDrawPoolFullbright();
        break;
    case POOL_GLOW:
        poolp = new LLDrawPoolGlow();
        break;
    case POOL_ALPHA_PRE_WATER:
        poolp = new LLDrawPoolAlpha(LLDrawPool::POOL_ALPHA_PRE_WATER);
        break;
    case POOL_ALPHA_POST_WATER:
        poolp = new LLDrawPoolAlpha(LLDrawPool::POOL_ALPHA_POST_WATER);
        break;
    case POOL_AVATAR:
    case POOL_CONTROL_AV:
        poolp = new LLDrawPoolAvatar(type);
        break;
    case POOL_TREE:
        poolp = new LLDrawPoolTree(tex0);
        break;
    case POOL_TERRAIN:
        poolp = new LLDrawPoolTerrain(tex0);
        break;
    case POOL_SKY:
        poolp = new LLDrawPoolSky();
        break;
    case POOL_WATER:
        poolp = new LLDrawPoolWater();
        break;
    case POOL_BUMP:
        poolp = new LLDrawPoolBump();
        break;
    case POOL_MATERIALS:
        poolp = new LLDrawPoolMaterials();
        break;
    case POOL_WL_SKY:
        poolp = new LLDrawPoolWLSky();
        break;
    case POOL_GLTF_PBR:
        poolp = new LLDrawPoolGLTFPBR();
        break;
    case POOL_GLTF_PBR_ALPHA_MASK:
        poolp = new LLDrawPoolGLTFPBR(LLDrawPool::POOL_GLTF_PBR_ALPHA_MASK);
        break;
    case POOL_WATEREXCLUSION:
        poolp = new LLDrawPoolWaterExclusion();
        break;
    default:
        LL_ERRS() << "Unknown draw pool type!" << LL_ENDL;
        return NULL;
    }

    llassert(poolp->mType == type);
    return poolp;
}

LLDrawPool::LLDrawPool(const U32 type)
{
    mType = type;
    sNumDrawPools++;
    mId = sNumDrawPools;
    mShaderLevel = 0;
    mSkipRender = false;
}

LLDrawPool::~LLDrawPool()
{

}

LLViewerTexture *LLDrawPool::getDebugTexture()
{
    return NULL;
}

//virtual
void LLDrawPool::beginRenderPass( S32 pass )
{
}

//virtual
S32  LLDrawPool::getNumPasses()
{
    return 1;
}

//virtual
void LLDrawPool::beginDeferredPass(S32 pass)
{

}

//virtual
void LLDrawPool::endDeferredPass(S32 pass)
{

}

//virtual
S32 LLDrawPool::getNumDeferredPasses()
{
    return 0;
}

//virtual
void LLDrawPool::renderDeferred(S32 pass)
{

}

//virtual
void LLDrawPool::beginPostDeferredPass(S32 pass)
{

}

//virtual
void LLDrawPool::endPostDeferredPass(S32 pass)
{

}

//virtual
S32 LLDrawPool::getNumPostDeferredPasses()
{
    return 0;
}

//virtual
void LLDrawPool::renderPostDeferred(S32 pass)
{

}

//virtual
void LLDrawPool::endRenderPass( S32 pass )
{
    //make sure channel 0 is active channel
}

//virtual
bool LLDrawPool::hasShadowCasters() const
{
    return false;
}

//virtual
void LLDrawPool::addShadowCasters(ALSpotShadowCache::Casters& casters)
{
    casters.markChanging();
}

//=============================
// Face Pool Implementation
//=============================
LLFacePool::LLFacePool(const U32 type)
: LLDrawPool(type)
{
    resetDrawOrders();
}

LLFacePool::~LLFacePool()
{
    destroy();
}

void LLFacePool::destroy()
{
    if (!mReferences.empty())
    {
        LL_INFOS() << mReferences.size() << " references left on deletion of draw pool!" << LL_ENDL;
    }
}

void LLFacePool::dirtyTextures(const std::set<LLViewerFetchedTexture*>& textures)
{
}

void LLFacePool::enqueue(LLFace* facep)
{
    mDrawFace.push_back(facep);
}

// virtual
bool LLFacePool::addFace(LLFace *facep)
{
    addFaceReference(facep);
    return true;
}

// virtual
bool LLFacePool::removeFace(LLFace *facep)
{
    removeFaceReference(facep);

    vector_replace_with_last(mDrawFace, facep);

    return true;
}

// Not absolutely sure if we should be resetting all of the chained pools as well - djs
void LLFacePool::resetDrawOrders()
{
    mDrawFace.resize(0);
}

// virtual
void LLFacePool::addShadowCasters(ALSpotShadowCache::Casters& casters)
{
    if (!mDrawFace.empty())
    {
        casters.markChanging();
    }
}

LLViewerTexture *LLFacePool::getTexture()
{
    return NULL;
}

void LLFacePool::removeFaceReference(LLFace *facep)
{
    if (facep->getReferenceIndex() != -1)
    {
        if (facep->getReferenceIndex() != (S32)mReferences.size())
        {
            LLFace *back = mReferences.back();
            mReferences[facep->getReferenceIndex()] = back;
            back->setReferenceIndex(facep->getReferenceIndex());
        }
        mReferences.pop_back();
    }
    facep->setReferenceIndex(-1);
}

void LLFacePool::addFaceReference(LLFace *facep)
{
    if (-1 == facep->getReferenceIndex())
    {
        facep->setReferenceIndex(static_cast<S32>(mReferences.size()));
        mReferences.push_back(facep);
    }
}

bool LLFacePool::verify() const
{
    bool ok = true;

    for (std::vector<LLFace*>::const_iterator iter = mDrawFace.begin();
         iter != mDrawFace.end(); iter++)
    {
        const LLFace* facep = *iter;
        if (facep->getPool() != this)
        {
            LL_INFOS() << "Face in wrong pool!" << LL_ENDL;
            facep->printDebugInfo();
            ok = false;
        }
        else if (!facep->verify())
        {
            ok = false;
        }
    }

    return ok;
}

void LLFacePool::printDebugInfo() const
{
    LL_INFOS() << "Pool " << this << " Type: " << getType() << LL_ENDL;
}

bool LLFacePool::LLOverrideFaceColor::sOverrideFaceColor = false;

void LLFacePool::LLOverrideFaceColor::setColor(const LLColor4& color)
{
    gGL.diffuseColor4fv(color.mV);
}

void LLFacePool::LLOverrideFaceColor::setColor(const LLColor4U& color)
{
    gGL.diffuseColor4ubv(color.mV);
}

void LLFacePool::LLOverrideFaceColor::setColor(F32 r, F32 g, F32 b, F32 a)
{
    gGL.diffuseColor4f(r,g,b,a);
}


//=============================
// Render Pass Implementation
//=============================
LLRenderPass::LLRenderPass(const U32 type)
: LLDrawPool(type)
{

}

LLRenderPass::~LLRenderPass()
{

}

void LLRenderPass::renderGroup(LLSpatialGroup* group, U32 type, bool texture)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLSpatialGroup::drawmap_elem_t& draw_info = group->mDrawMap[type];

    for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(); k != draw_info.end(); ++k)
    {
        LLDrawInfo *pparams = *k;
        if (pparams)
        {
            pushBatch(*pparams, texture);
        }
    }
}

void LLRenderPass::renderRiggedGroup(LLSpatialGroup* group, U32 type, bool texture)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLSpatialGroup::drawmap_elem_t& draw_info = group->mDrawMap[type];
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(); k != draw_info.end(); ++k)
    {
        LLDrawInfo* pparams = *k;
        if (pparams)
        {
            if (uploadMatrixPalette(pparams->mAvatar, pparams->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
            {
                pushBatch(*pparams, texture);
            }
        }
    }
}

void LLRenderPass::pushBatches(U32 type, bool texture, bool batch_textures)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    if (texture)
    {
        auto* begin = gPipeline.beginRenderMap(type);
        auto* end = gPipeline.endRenderMap(type);
        for (LLCullResult::drawinfo_iterator i = begin; i != end; )
        {
            LLDrawInfo* pparams = *i;
            LLCullResult::increment_iterator(i, end);

            llassert(pparams); // figure out how null got here, it shouldn't be happening
            if (pparams)
            {
                pushBatch(*pparams, texture, batch_textures);
            }
        }
    }
    else
    {
        pushUntexturedBatches(type);
    }
}

void LLRenderPass::pushUntexturedBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo* pparams = *i;
        LLCullResult::increment_iterator(i, end);

        if (pparams)
        {
            pushUntexturedBatch(*pparams);
        }
    }
}

void LLRenderPass::pushRiggedBatches(U32 type, bool texture, bool batch_textures)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    if (texture)
    {
        const LLVOAvatar* lastAvatar = nullptr;
        U64 lastMeshId = 0;
        bool skipLastSkin = false;
        auto* begin = gPipeline.beginRenderMap(type);
        auto* end = gPipeline.endRenderMap(type);
        for (LLCullResult::drawinfo_iterator i = begin; i != end; )
        {
            LLDrawInfo* pparams = *i;
            LLCullResult::increment_iterator(i, end);

            if (pparams && uploadMatrixPalette(pparams->mAvatar, pparams->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
            {
                pushBatch(*pparams, texture, batch_textures);
            }
        }
    }
    else
    {
        pushUntexturedRiggedBatches(type);
    }
}

void LLRenderPass::pushUntexturedRiggedBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo* pparams = *i;
        LLCullResult::increment_iterator(i, end);

        if (pparams && uploadMatrixPalette(pparams->mAvatar, pparams->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
        {
            pushUntexturedBatch(*pparams);
        }
    }
}

void LLRenderPass::applyModelMatrix(const LLDrawInfo& params)
{
    applyModelMatrix(params.mModelMatrix);
}

void LLRenderPass::applyModelMatrix(const LLMatrix4a* model_matrix)
{
    if (model_matrix != gGLLastMatrix)
    {
        gGLLastMatrix = model_matrix;
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.loadMatrix(LLViewerCamera::getCurrent().getModelview());
        if (model_matrix)
        {
            gGL.multMatrix(model_matrix->getF32ptr());
        }
        gPipeline.mMatrixOpCount++;
    }
}

void LLRenderPass::bindIndexedTextures(const LLDrawInfo& params, const LLGLSLShader* shader)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    // Never wider than the program actually declares: a batch that outran the ladder would
    // bind past the last sampler, and the overflow vertices sample tex(N-1) regardless.
    const U32 declared = shader ? (U32)shader->mFeatures.mIndexedTextureChannels : 0;
    const U32 batch = llmin((U32)params.mTextureList.size(), declared);

    // The indexed ladder is all diffuse; the program is what knows whether it shades in
    // linear and wants the ladder decoded (mLinearDiffuse, from LINEAR_DIFFUSE).
    ALSampler key = ALSamplers::AnisoWrap;
    if (shader && shader->mLinearDiffuse)
    {
        key |= ALSampler::SRGBDecode;
    }

    for (U32 i = 0; i < batch; ++i)
    {
        LLViewerTexture* tex = params.mTextureList[i].get();
        gGL.getTextureSlot(i)->bindFast(tex ? tex : LLViewerFetchedTexture::sWhiteImagep.get(), key);
    }
}

#if LL_DARWIN
void LLRenderPass::captureMetalHUDBatch(
    const LLDrawInfo& params,
    bool batch_textures,
    bool alpha_mask,
    bool alpha_pool,
    bool pbr_alpha,
    F32 minimum_alpha,
    bool force_double_sided,
    bool fullbright_shiny,
    bool pbr_opaque)
{
    LLVertexBuffer* buffer = params.mVertexBuffer.get();
    ALMetalHUDIndexedBatch batch;
    batch.shader = pbr_opaque
        ? ALMetalHUDShader::pbr_opaque
        : pbr_alpha ? ALMetalHUDShader::pbr_alpha
        : fullbright_shiny
        ? ALMetalHUDShader::fullbright_shiny
        : alpha_pool
        ? ALMetalHUDShader::fullbright_alpha
        : alpha_mask ? ALMetalHUDShader::fullbright_alpha_mask
                     : ALMetalHUDShader::fullbright;
    batch.minimum_alpha = (alpha_pool || pbr_alpha || pbr_opaque)
        ? minimum_alpha : alpha_mask ? params.mAlphaMaskCutoff : 0.f;

    const bool complete_geometry = buffer && params.mCount &&
        buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) &&
        buffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0) &&
        buffer->hasDataType(LLVertexBuffer::TYPE_COLOR) &&
        buffer->getMetalVertexBuffer() &&
        buffer->getMetalIndexBuffer();
    if (complete_geometry)
    {
        batch.vertices = buffer->getMetalVertexBuffer();
        batch.position_offset = buffer->getOffset(
            LLVertexBuffer::TYPE_VERTEX);
        batch.texcoord_offset = buffer->getOffset(
            LLVertexBuffer::TYPE_TEXCOORD0);
        batch.color_offset = buffer->getOffset(
            LLVertexBuffer::TYPE_COLOR);
        batch.vertex_count = buffer->getNumVerts();
        batch.indices = buffer->getMetalIndexBuffer();
        batch.index_offset = static_cast<std::size_t>(params.mOffset) *
            buffer->getIndexStride();
        batch.index_type = buffer->getIndexStride() == 4
            ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
        batch.index_count = params.mCount;
    }

    LLMatrix4a modelview_projection;
    modelview_projection.setMul(
        gGL.getModelviewMatrix(), gGL.getProjectionMatrix());
    std::memcpy(batch.matrices.modelview_projection,
                modelview_projection.getF32ptr(),
                sizeof(batch.matrices.modelview_projection));
    for (U32 diagonal = 0; diagonal < 4; ++diagonal)
    {
        batch.matrices.texture0[diagonal * 4 + diagonal] = 1.f;
    }
    if (params.mTextureMatrix &&
        (!gCubeSnapshot || gPipeline.mHeroProbeManager.isMirrorPass()))
    {
        std::memcpy(batch.matrices.texture0,
                    params.mTextureMatrix->mMatrix,
                    sizeof(batch.matrices.texture0));
    }

    const auto metal_texture = [](LLViewerTexture* texture)
        -> ALMetalTexture*
    {
        if (texture)
        {
            // Direct native draws still count as visible texture use. Match
            // bindFast's activity/residency updates without binding a slot.
            texture->setActive();
        }
        LLImageGL* image = texture ? texture->getGLTexture() : nullptr;
        if (texture && (!image || !image->getMetalTexture()))
        {
            // Match bindFast/bindDefaultImage while a streamed HUD asset is
            // unpublished: request it and draw the normal placeholder. One
            // pending texture must not stall the complete world/UI frame.
            texture->forceImmediateUpdate();
            image = texture->getGLTexture();
            if (!image || !image->getMetalTexture())
            {
                if (image)
                {
                    image->forceUpdateBindStats();
                }
                LLViewerTexture* fallback =
                    LLViewerFetchedTexture::sDefaultImagep.get();
                image = fallback ? fallback->getGLTexture() : nullptr;
                if ((!image || !image->getMetalTexture()) &&
                    LLViewerTexture::sNullImagep.notNull())
                {
                    fallback = LLViewerTexture::sNullImagep.get();
                    image = fallback->getGLTexture();
                }
                if (fallback)
                {
                    fallback->setActive();
                }
            }
        }
        if (image)
        {
            image->forceUpdateBindStats();
        }
        return image ? image->getMetalTexture() : nullptr;
    };
    if ((pbr_alpha || pbr_opaque) && params.mGLTFMaterial.notNull())
    {
        LLFetchedGLTFMaterial* material = params.mGLTFMaterial.get();
        LLViewerTexture* base_color = params.mTexture.notNull()
            ? params.mTexture.get() : material->mBaseColorTexture.get();
        LLViewerTexture* emissive = params.mTexture.notNull()
            ? params.mTexture.get() : material->mEmissiveTexture.get();
        if (!base_color)
        {
            base_color = LLViewerFetchedTexture::sWhiteImagep.get();
        }
        if (!emissive)
        {
            emissive = LLViewerFetchedTexture::sWhiteImagep.get();
        }
        batch.texture_count = 1;
        batch.textures[0] = metal_texture(base_color);
        batch.emissive_texture = metal_texture(emissive);
        LLGLTFMaterial::TextureTransform::Pack packed;
        material->mTextureTransform[
            LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR].getPacked(packed);
        std::memcpy(batch.pbr_base_color_transform, packed,
                    sizeof(batch.pbr_base_color_transform));
        material->mTextureTransform[
            LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE].getPacked(packed);
        std::memcpy(batch.pbr_emissive_transform, packed,
                    sizeof(batch.pbr_emissive_transform));
        std::memcpy(batch.pbr_emissive_color,
                    material->mEmissiveColor.mV,
                    sizeof(material->mEmissiveColor.mV));
        batch.double_sided = force_double_sided || material->mDoubleSided;
    }
    else
    {
        batch.double_sided = force_double_sided;
        const bool indexed_textures =
            batch_textures && params.mTextureList.size() > 1;
        const std::size_t texture_count = indexed_textures
            ? params.mTextureList.size() : 1;
        batch.texture_count = static_cast<U32>(texture_count);
        const std::size_t copied_textures = std::min<std::size_t>(
            texture_count, ALMetalHUDIndexedBatch::max_textures);
        for (std::size_t index = 0; index < copied_textures; ++index)
        {
            LLViewerTexture* texture = indexed_textures
                ? params.mTextureList[index].get() : params.mTexture.get();
            if (!texture)
            {
                texture = LLViewerFetchedTexture::sWhiteImagep.get();
            }
            batch.textures[index] = metal_texture(texture);
        }
    }

    // Submit one exact native HUD batch. The caller owns the decision about
    // whether this is diagnostic mirroring or the authoritative draw.
    gGL.drawMetalHUDIndexedBatch(
        batch, force_double_sided ? ALSamplers::AnisoClamp
                                  : ALSamplers::AnisoWrap);
}

void LLRenderPass::captureMetalHUDEmbossBatch(
    const LLDrawInfo& params, LLViewerTexture* bump_texture)
{
    LLVertexBuffer* buffer = params.mVertexBuffer.get();
    ALMetalHUDIndexedBatch batch;
    batch.shader = ALMetalHUDShader::emboss;
    LLImageGL* bump_image = bump_texture
        ? bump_texture->getGLTexture() : nullptr;
    if (bump_image && !bump_image->getMetalTexture())
    {
        LLViewerTexture* fallback =
            LLViewerFetchedTexture::sDefaultImagep.get();
        bump_texture = fallback;
        bump_image = fallback ? fallback->getGLTexture() : nullptr;
    }
    if (!params.mCount || !buffer || !bump_image ||
        !bump_image->getMetalTexture() ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0) ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD1) ||
        !buffer->getMetalVertexBuffer() ||
        !buffer->getMetalIndexBuffer())
    {
        // Submit the invalid descriptor so the authoritative capture records
        // a fail-closed reason instead of silently omitting HUD geometry.
        gGL.drawMetalHUDIndexedBatch(batch);
        return;
    }

    bump_texture->setActive();
    bump_image->forceUpdateBindStats();
    batch.vertices = buffer->getMetalVertexBuffer();
    batch.position_offset = buffer->getOffset(
        LLVertexBuffer::TYPE_VERTEX);
    batch.texcoord_offset = buffer->getOffset(
        LLVertexBuffer::TYPE_TEXCOORD0);
    batch.texcoord1_offset = buffer->getOffset(
        LLVertexBuffer::TYPE_TEXCOORD1);
    batch.vertex_count = buffer->getNumVerts();
    batch.indices = buffer->getMetalIndexBuffer();
    batch.index_offset = static_cast<std::size_t>(params.mOffset) *
        buffer->getIndexStride();
    batch.index_type = buffer->getIndexStride() == 4
        ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
    batch.index_count = params.mCount;
    batch.textures[0] = bump_image->getMetalTexture();
    batch.texture_count = 1;
    batch.depth_bias_slope = -1.f;
    batch.depth_bias_units = -1.f;

    LLMatrix4a modelview_projection;
    modelview_projection.setMul(
        gGL.getModelviewMatrix(), gGL.getProjectionMatrix());
    std::memcpy(batch.matrices.modelview_projection,
                modelview_projection.getF32ptr(),
                sizeof(batch.matrices.modelview_projection));
    for (U32 diagonal = 0; diagonal < 4; ++diagonal)
    {
        batch.matrices.texture0[diagonal * 4 + diagonal] = 1.f;
    }
    if (params.mTextureMatrix &&
        (!gCubeSnapshot || gPipeline.mHeroProbeManager.isMirrorPass()))
    {
        std::memcpy(batch.matrices.texture0,
                    params.mTextureMatrix->mMatrix,
                    sizeof(batch.matrices.texture0));
    }

    gGL.drawMetalHUDIndexedBatch(batch);
}
#endif

void LLRenderPass::pushBatch(LLDrawInfo& params, bool texture, bool batch_textures)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    llassert(texture);

    if (!params.mCount)
    {
        return;
    }

    applyModelMatrix(params);

    // The bound program is what knows whether its diffuse wants the hardware decode
    // (mLinearDiffuse, from the LINEAR_DIFFUSE permutation), whichever bind path it takes
    // below. bindIndexedTextures derives the same key for the ladder.
    const LLGLSLShader* bound = LLGLSLShader::sCurBoundShaderPtr;
    ALSampler key = ALSamplers::AnisoWrap;
    if (bound && bound->mLinearDiffuse)
    {
        key |= ALSampler::SRGBDecode;
    }

    bool tex_setup = false;

    {
        if (batch_textures && params.mTextureList.size() > 1)
        {
            bindIndexedTextures(params, bound);
        }
        else
        { //not batching textures or batch has only 1 texture -- might need a texture matrix
            if (params.mTexture.notNull())
            {
                gGL.getTextureSlot(0)->bindFast(params.mTexture, key);
                if (params.mTextureMatrix && (!gCubeSnapshot || gPipeline.mHeroProbeManager.isMirrorPass()))
                {
                    tex_setup = true;
                    gGL.matrixMode(LLRender::MM_TEXTURE0);
                    gGL.loadMatrix((GLfloat*) params.mTextureMatrix->mMatrix);
                    gPipeline.countTextureMatrixOp(*params.mTextureMatrix);
                }
            }
            else
            {
                gGL.getTextureSlot(0)->unbindFast();
            }
        }
    }

#if LL_DARWIN
    gPipeline.captureMetalSpotShadowTexturedBatch(
        params, batch_textures);
    gPipeline.captureMetalWorldGBufferBatch(params, batch_textures);
#endif

    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);

    if (tex_setup)
    {
        gGL.matrixMode(LLRender::MM_TEXTURE0);
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
}

void LLRenderPass::pushUntexturedBatch(LLDrawInfo& params)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    if (!params.mCount)
    {
        return;
    }

    applyModelMatrix(params);

#if LL_DARWIN
    gPipeline.captureMetalSpotShadowOpaqueBatch(params);
#endif

    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
}

// Route a computed palette to the bound shader. mGLMp is [origin vec4 | count mat3x4]:
// the array upload starts PAST the origin, and the origin rides as its own uniform, which
// the shader adds back (skinTransformH / getObjectSkinnedTransform). The split is what keeps
// the palette translations -- and so every per-vertex operand -- avatar-local; see
// LLVOAvatar::updateSkinInfoMatrixPalette for why that matters.
static void apply_matrix_palette(const LLVOAvatar::MatrixPaletteCache& mpc, U32 count)
{
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    shader->uniformMatrix3x4fv(LLViewerShaderMgr::AVATAR_MATRIX, count, false, (GLfloat*)&(mpc.mGLMp[4]));
    shader->uniform3fv(LLViewerShaderMgr::SKIN_ORIGIN, 1, (GLfloat*)&(mpc.mGLMp[0]));
}

// static
bool LLRenderPass::uploadMatrixPalette(LLDrawInfo& params)
{
    // upload matrix palette to shader
    return uploadMatrixPalette(params.mAvatar, params.mSkinInfo);
}

//static
bool LLRenderPass::uploadMatrixPalette(LLVOAvatar* avatar, LLMeshSkinInfo* skinInfo)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    if (!avatar)
    {
        return false;
    }
    const LLVOAvatar::MatrixPaletteCache& mpc = avatar->updateSkinInfoMatrixPalette(skinInfo);
    U32 count = static_cast<U32>(mpc.mMatrixPalette.size());

    if (count == 0)
    {
        //skin info not loaded yet, don't render
        return false;
    }

    apply_matrix_palette(mpc, count);

    return true;
}

// Returns true if rendering should proceed
//static
bool LLRenderPass::uploadMatrixPalette(LLVOAvatar* avatar, LLMeshSkinInfo* skinInfo, const LLVOAvatar*& lastAvatar, U64& lastMeshId, bool& skipLastSkin)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    llassert(skinInfo);
    llassert(LLGLSLShader::sCurBoundShaderPtr);

    if (!avatar)
    {
        return false;
    }

    if (avatar == lastAvatar && skinInfo->mHash == lastMeshId)
    {
        return !skipLastSkin;
    }

    const LLVOAvatar::MatrixPaletteCache& mpc = avatar->updateSkinInfoMatrixPalette(skinInfo);
    U32 count = static_cast<U32>(mpc.mMatrixPalette.size());
    // skipLastSkin -> skin info not loaded yet, don't render
    skipLastSkin = !bool(count);
    lastAvatar = avatar;
    lastMeshId = skinInfo->mHash;

    if (!skipLastSkin)
    {
        apply_matrix_palette(mpc, count);
    }

    return !skipLastSkin;
}

// Returns true if rendering should proceed
//static
bool LLRenderPass::uploadMatrixPalette(LLVOAvatar* avatar, LLMeshSkinInfo* skinInfo, const LLVOAvatar*& lastAvatar, U64& lastMeshId, const LLGLSLShader*& lastAvatarShader, bool& skipLastSkin)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    llassert(skinInfo);
    llassert(LLGLSLShader::sCurBoundShaderPtr);

    if (!avatar)
    {
        return false;
    }

    if (avatar == lastAvatar && skinInfo->mHash == lastMeshId && lastAvatarShader == LLGLSLShader::sCurBoundShaderPtr)
    {
        return !skipLastSkin;
    }

    const LLVOAvatar::MatrixPaletteCache& mpc = avatar->updateSkinInfoMatrixPalette(skinInfo);
    U32 count = static_cast<U32>(mpc.mMatrixPalette.size());
    // skipLastSkin -> skin info not loaded yet, don't render
    skipLastSkin = !bool(count);
    lastAvatar = avatar;
    lastMeshId = skinInfo->mHash;
    lastAvatarShader = LLGLSLShader::sCurBoundShaderPtr;

    if (!skipLastSkin)
    {
        apply_matrix_palette(mpc, count);
    }

    return !skipLastSkin;
}

void add_shadow_texture(ALSpotShadowCache::Casters& casters, ALSpotShadowCache::Hash& draw, const LLViewerTexture* texture)
{
    draw.add(texture);
    if (!texture)
    {
        return;
    }

    const S8 type = texture->getType();
    if (type == LLViewerTexture::MEDIA_TEXTURE || type == LLViewerTexture::DYNAMIC_TEXTURE)
    {
        casters.markChanging();
    }

    const LLImageGL* image = texture->getGLTexture();
    draw.add(image);
    if (image)
    {
        draw.add(image->getContentStamp())
            .add((U64)(U32)image->getDiscardLevel())
            .add((U64)(U32)image->getCurrentWidth())
            .add((U64)(U32)image->getCurrentHeight());
    }
}

void setup_texture_matrix(LLDrawInfo& params)
{
    if (params.mTextureMatrix && (!gCubeSnapshot || gPipeline.mHeroProbeManager.isMirrorPass()))
    { //special case implementation of texture animation here because of special handling of textures for PBR batches
        gGL.matrixMode(LLRender::MM_TEXTURE0);
        gGL.loadMatrix((GLfloat*)params.mTextureMatrix->mMatrix);
        gPipeline.countTextureMatrixOp(*params.mTextureMatrix);
    }
}

void teardown_texture_matrix(LLDrawInfo& params)
{
    if (params.mTextureMatrix && (!gCubeSnapshot || gPipeline.mHeroProbeManager.isMirrorPass()))
    {
        gGL.matrixMode(LLRender::MM_TEXTURE0);
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
}

void LLRenderPass::pushGLTFBatches(U32 type, bool textured)
{
    if (textured)
    {
        pushGLTFBatches(type);
    }
    else
    {
        pushUntexturedGLTFBatches(type);
    }
}

void LLRenderPass::pushGLTFBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLFetchedGLTFMaterial* lastMat = nullptr;
    LLViewerTexture* lastTex = nullptr;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("pushGLTFBatch");
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        pushGLTFBatch(params, lastMat, lastTex);
    }
}

void LLRenderPass::pushUntexturedGLTFBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("pushGLTFBatch");
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        pushUntexturedGLTFBatch(params);
    }
}

// static
void LLRenderPass::pushGLTFBatch(LLDrawInfo& params, LLFetchedGLTFMaterial*& lastMat, LLViewerTexture*& lastTex)
{
    LLFetchedGLTFMaterial* mat = params.mGLTFMaterial.get();

    if (mat)
    {
        // params.mTexture is the media override (bind() applies it to base color
        // and emissive), so it is part of the cache key -- otherwise media faces
        // sharing a material would render with a stale base texture.
        LLViewerTexture* tex = params.mTexture.get();
        if (mat != lastMat || tex != lastTex)
        {
            mat->bind(params.mTexture);
            lastMat = mat;
            lastTex = tex;
        }
    }

    LLGLDisable cull_face(mat && mat->mDoubleSided ? GL_CULL_FACE : 0);

    setup_texture_matrix(params);

    applyModelMatrix(params);

#if LL_DARWIN
    gPipeline.captureMetalSpotShadowPBRBatch(params, false);
    gPipeline.captureMetalWorldPBRBatch(params, false);
#endif

    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);

    teardown_texture_matrix(params);
}

// static
void LLRenderPass::pushUntexturedGLTFBatch(LLDrawInfo& params)
{
    auto& mat = params.mGLTFMaterial;

    LLGLDisable cull_face(mat->mDoubleSided ? GL_CULL_FACE : 0);

    applyModelMatrix(params);

#if LL_DARWIN
    gPipeline.captureMetalSpotShadowOpaqueBatch(
        params, mat && mat->mDoubleSided);
#endif

    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
}

void LLRenderPass::pushRiggedGLTFBatches(U32 type, bool textured)
{
    if (textured)
    {
        pushRiggedGLTFBatches(type);
    }
    else
    {
        pushUntexturedRiggedGLTFBatches(type);
    }
}

void LLRenderPass::pushRiggedGLTFBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;
    LLFetchedGLTFMaterial* lastMat = nullptr;
    LLViewerTexture* lastTex = nullptr;

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("pushRiggedGLTFBatch");
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        pushRiggedGLTFBatch(params, lastAvatar, lastMeshId, skipLastSkin, lastMat, lastTex);
    }
}

void LLRenderPass::pushUntexturedRiggedGLTFBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("pushRiggedGLTFBatch");
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        pushUntexturedRiggedGLTFBatch(params, lastAvatar, lastMeshId, skipLastSkin);
    }
}


// static
void LLRenderPass::pushRiggedGLTFBatch(LLDrawInfo& params, const LLVOAvatar*& lastAvatar, U64& lastMeshId, bool& skipLastSkin, LLFetchedGLTFMaterial*& lastMat, LLViewerTexture*& lastTex)
{
    if (uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
    {
        pushGLTFBatch(params, lastMat, lastTex);
    }
}

// static
void LLRenderPass::pushUntexturedRiggedGLTFBatch(LLDrawInfo& params, const LLVOAvatar*& lastAvatar, U64& lastMeshId, bool& skipLastSkin)
{
    if (uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
    {
        pushUntexturedGLTFBatch(params);
    }
}

