/**
 * @file lldrawpoolpbropaque.cpp
 * @brief LLDrawPoolGLTFPBR class implementation
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2022, Linden Research, Inc.
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
#include "lldrawpoolpbropaque.h"
#include "llfetchedgltfmaterial.h"
#include "llviewershadermgr.h"
#include "pipeline.h"

LLDrawPoolGLTFPBR::LLDrawPoolGLTFPBR(U32 type) :
    LLRenderPass(type)
{
    if (type == LLDrawPool::POOL_GLTF_PBR_ALPHA_MASK)
    {
        mRenderType = LLPipeline::RENDER_TYPE_PASS_GLTF_PBR_ALPHA_MASK;
    }
    else
    {
        mRenderType = LLPipeline::RENDER_TYPE_PASS_GLTF_PBR;
    }
}

S32 LLDrawPoolGLTFPBR::getNumDeferredPasses()
{
    return 1;
}

#if LL_DARWIN
static void capture_metal_pbr_batches(
    U32 type, bool rigged, bool alpha_mask)
{
    gPipeline.beginMetalWorldPBRPass(rigged, alpha_mask);
    auto* at = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    while (at != end)
    {
        LLDrawInfo* info = *at;
        LLCullResult::increment_iterator(at, end);
        if (info)
        {
            gPipeline.captureMetalWorldPBRBatch(
                *info, info->mGLTFMaterialList.size() > 1);
        }
    }
    gPipeline.endMetalWorldGBufferPass();
}

static void capture_metal_pbr_glow_batches(U32 type, bool rigged)
{
    gPipeline.beginMetalWorldPBRGlowPass(rigged);
    auto* at = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    while (at != end)
    {
        LLDrawInfo* info = *at;
        LLCullResult::increment_iterator(at, end);
        if (info)
        {
            gPipeline.captureMetalWorldPBRBatch(
                *info, info->mGLTFMaterialList.size() > 1);
        }
    }
    gPipeline.endMetalWorldGBufferPass();
}
#endif

void LLDrawPoolGLTFPBR::renderDeferred(S32)
{
    llassert(!LLPipeline::sRenderingHUDs);
    const bool alpha_mask = mRenderType == LLPipeline::RENDER_TYPE_PASS_GLTF_PBR_ALPHA_MASK;
#if LL_DARWIN
    capture_metal_pbr_batches(mRenderType, false, alpha_mask);
    capture_metal_pbr_batches(mRenderType + 1, true, alpha_mask);
#endif
}

S32 LLDrawPoolGLTFPBR::getNumPostDeferredPasses()
{
    return 1;
}

void LLDrawPoolGLTFPBR::renderPostDeferred(S32)
{
    if (LLPipeline::sRenderingHUDs)
    {
#if LL_DARWIN
        if (!gGL.isMetalUITextureCaptureAuthoritative())
        {
            return;
        }
        LLGLDisable blend(GL_BLEND);
        // genDrawInfo deliberately excludes HUDs from multi-material PBR
        // batching. Preserve this scalar material's original HUD contract;
        // the geometry still uses its actual U16/U32 index buffer/range.
        auto* at = gPipeline.beginRenderMap(mRenderType);
        auto* end = gPipeline.endRenderMap(mRenderType);
        while (at != end)
        {
            LLDrawInfo* info = *at;
            LLCullResult::increment_iterator(at, end);
            if (!info)
            {
                continue;
            }
            const LLFetchedGLTFMaterial* material = info->mGLTFMaterial.get();
            const F32 minimum_alpha = material &&
                material->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_MASK
                ? material->mAlphaCutoff : -1.f;
            applyModelMatrix(*info);
            captureMetalHUDBatch(
                *info, false, false, false, false, minimum_alpha,
                false, false, true);
        }
#endif
    }
    // Alpha-masked PBR does not contribute the opaque emissive replay.
    else if (mRenderType == LLPipeline::RENDER_TYPE_PASS_GLTF_PBR && LLPipeline::sRenderGlow)
    {
#if LL_DARWIN
        capture_metal_pbr_glow_batches(
            LLRenderPass::PASS_GLTF_GLOW, false);
        capture_metal_pbr_glow_batches(
            LLRenderPass::PASS_GLTF_GLOW_RIGGED, true);
#endif
    }
}

