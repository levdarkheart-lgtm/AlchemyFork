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

void LLDrawPoolGLTFPBR::renderDeferred(S32 pass)
{
    llassert(!LLPipeline::sRenderingHUDs);

    // GL_FRAMEBUFFER_SRGB is enabled for the whole deferred pass in renderGeomDeferred.

    // Only the alpha-mask pass binds a program that can discard. Its materials are all
    // masked and the opaque pass's never are, so the opaque pass's programs drop the
    // test and keep their depth writes ahead of shading.
    const bool alpha_mask = mRenderType == LLPipeline::RENDER_TYPE_PASS_GLTF_PBR_ALPHA_MASK;
    LLGLSLShader& scalar_program = alpha_mask ? gDeferredPBRAlphaMaskProgram : gDeferredPBROpaqueProgram;
    LLGLSLShader& indexed_program = alpha_mask ? gDeferredPBRAlphaMaskIndexedProgram : gDeferredPBROpaqueIndexedProgram;

    // Indexed (multi-material) batching applies to the static opaque and alpha-mask
    // passes. The indexed programs write the GBuffer the same way for both; in the
    // alpha-mask one the per-slot gltf_minimum_alpha array drives the discard.
    // Only skip multi-material infos in the scalar sweep once BOTH the indexed
    // program and its rigged variant are complete -- otherwise scalar would skip
    // them and the indexed sweep would bind an incomplete program.
    bool indexed = (mRenderType == LLPipeline::RENDER_TYPE_PASS_GLTF_PBR ||
                    mRenderType == LLPipeline::RENDER_TYPE_PASS_GLTF_PBR_ALPHA_MASK) &&
                   LLGLSLShader::sIndexedGLTFChannels >= 2 &&
                   indexed_program.isComplete() &&
                   indexed_program.mRiggedVariant &&
                   indexed_program.mRiggedVariant->isComplete();

    LLGLSLShader* opaque = scalar_program.selectVariant();

#if LL_DARWIN
    gPipeline.beginMetalWorldPBRPass(false, alpha_mask);
#endif
    opaque->bind();
    if (indexed)
    { // multi-material infos are drawn separately below; render only scalar here
        pushGLTFBatchesScalar(mRenderType);
    }
    else
    {
        pushGLTFBatches(mRenderType);
    }
#if LL_DARWIN
    gPipeline.endMetalWorldGBufferPass();
    gPipeline.beginMetalWorldPBRPass(true, alpha_mask);
#endif

    opaque->bind(true);
    if (indexed)
    {
        pushRiggedGLTFBatchesScalar(mRenderType + 1);
    }
    else
    {
        pushRiggedGLTFBatches(mRenderType + 1);
    }
#if LL_DARWIN
    gPipeline.endMetalWorldGBufferPass();
#endif

    if (indexed)
    {
        LLGLSLShader* opaque_indexed = indexed_program.selectVariant();
#if LL_DARWIN
        gPipeline.beginMetalWorldPBRPass(false, alpha_mask);
#endif
        opaque_indexed->bind();
        pushGLTFBatchesIndexed(mRenderType);
#if LL_DARWIN
        gPipeline.endMetalWorldGBufferPass();
        gPipeline.beginMetalWorldPBRPass(true, alpha_mask);
#endif

        opaque_indexed->bind(true); // rigged variant
        pushRiggedGLTFBatchesIndexed(mRenderType + 1);
#if LL_DARWIN
        gPipeline.endMetalWorldGBufferPass();
#endif
    }
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

