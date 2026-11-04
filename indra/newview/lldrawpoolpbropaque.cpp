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

void LLDrawPoolGLTFPBR::renderPostDeferred(S32 pass)
{
    if (LLPipeline::sRenderingHUDs)
    {
        gHUDPBROpaqueProgram.bind();
        pushGLTFBatches(mRenderType);
    }
    // HACK -- don't render glow except for the non-alpha masked implementation. RenderGlow off is
    // decided here, as in LLDrawPoolGlow, rather than at registration.
    else if (mRenderType == LLPipeline::RENDER_TYPE_PASS_GLTF_PBR && LLPipeline::sRenderGlow)
    {
        gGL.setColorMask(false, true);

        // Multi-material (indexed) glow batches render with the indexed program; the
        // scalar sweep skips them. Require both the static and rigged indexed glow
        // programs to be complete; otherwise fall back to scalar for everything
        // (slot-0 glow, the pre-batching behavior).
        bool glow_indexed = LLGLSLShader::sIndexedGLTFChannels >= 2 &&
                            gPBRGlowIndexedProgram.isComplete() &&
                            gPBRGlowIndexedProgram.mRiggedVariant &&
                            gPBRGlowIndexedProgram.mRiggedVariant->isComplete();

        gPBRGlowProgram.bind();
#if LL_DARWIN
        gPipeline.beginMetalWorldPBRGlowPass(false);
#endif
        if (glow_indexed)
        {
            pushGLTFBatchesScalar(LLRenderPass::PASS_GLTF_GLOW);
        }
        else
        {
            pushGLTFBatches(LLRenderPass::PASS_GLTF_GLOW);
        }
#if LL_DARWIN
        gPipeline.endMetalWorldGBufferPass();
#endif

        gPBRGlowProgram.bind(true);
#if LL_DARWIN
        gPipeline.beginMetalWorldPBRGlowPass(true);
#endif
        if (glow_indexed)
        {
            pushRiggedGLTFBatchesScalar(LLRenderPass::PASS_GLTF_GLOW_RIGGED);
        }
        else
        {
            pushRiggedGLTFBatches(LLRenderPass::PASS_GLTF_GLOW_RIGGED);
        }
#if LL_DARWIN
        gPipeline.endMetalWorldGBufferPass();
#endif

        if (glow_indexed)
        {
            gPBRGlowIndexedProgram.bind();
#if LL_DARWIN
            gPipeline.beginMetalWorldPBRGlowPass(false);
#endif
            pushGLTFBatchesIndexed(LLRenderPass::PASS_GLTF_GLOW, GLTF_MAPS_GLOW);
#if LL_DARWIN
            gPipeline.endMetalWorldGBufferPass();
#endif

            gPBRGlowIndexedProgram.bind(true); // rigged variant
#if LL_DARWIN
            gPipeline.beginMetalWorldPBRGlowPass(true);
#endif
            pushRiggedGLTFBatchesIndexed(LLRenderPass::PASS_GLTF_GLOW_RIGGED, GLTF_MAPS_GLOW);
#if LL_DARWIN
            gPipeline.endMetalWorldGBufferPass();
#endif
        }

        gGL.setColorMask(true, false);
    }
}

