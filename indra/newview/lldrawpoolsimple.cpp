/**
 * @file lldrawpoolsimple.cpp
 * @brief LLDrawPoolSimple class implementation
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

#include "lldrawpoolsimple.h"

#include "llviewercamera.h"
#include "lldrawable.h"
#include "llface.h"
#include "llsky.h"
#include "pipeline.h"
#include "llspatialpartition.h"
#include "llviewershadermgr.h"
#include "llrender.h"

void LLDrawPoolGlow::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    // This pass writes only glow, into the alpha channel. In a cutout frame
    // that channel is the subject's coverage, and the coverage seeded after
    // the opaque pools (LLPipeline::seedCutoutCoverageFromDepth) writes over
    // whatever this put there, so it would be work for nothing.
    //
    // RenderGlow off is decided here rather than at registration, so the faces stay registered and
    // a toggle takes effect on the next frame.
    if (!LLPipeline::sRenderGlow || LLPipeline::cullingToCutoutSubject())
    {
        return;
    }
#if LL_DARWIN
    for (bool rigged : { false, true })
    {
        const U32 type = rigged
            ? LLRenderPass::PASS_GLOW_RIGGED
            : LLRenderPass::PASS_GLOW;
        gPipeline.beginMetalWorldLegacyGlowPass(rigged);
        auto* at = gPipeline.beginRenderMap(type);
        auto* end = gPipeline.endRenderMap(type);
        while (at != end)
        {
            LLDrawInfo* info = *at;
            LLCullResult::increment_iterator(at, end);
            if (info)
            {
                gPipeline.captureMetalWorldLegacyGlowBatch(
                    *info, info->mMaterialSlotList.size() > 1);
            }
        }
        gPipeline.endMetalWorldGBufferPass();
    }
#endif
}

LLDrawPoolSimple::LLDrawPoolSimple() :
    LLRenderPass(POOL_SIMPLE)
{
}

LLDrawPoolAlphaMask::LLDrawPoolAlphaMask() :
    LLRenderPass(POOL_ALPHA_MASK)
{
}

LLDrawPoolFullbrightAlphaMask::LLDrawPoolFullbrightAlphaMask() :
    LLRenderPass(POOL_FULLBRIGHT_ALPHA_MASK)
{
}

#if LL_DARWIN
static void capture_metal_gbuffer_batches(
    U32 type, bool rigged, bool alpha_mask, bool batch_textures,
    bool skip_indexed_materials = false)
{
    gPipeline.beginMetalWorldGBufferPass(rigged, alpha_mask);
    auto* at = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    while (at != end)
    {
        LLDrawInfo* info = *at;
        LLCullResult::increment_iterator(at, end);
        if (!info ||
            (skip_indexed_materials &&
             info->mMaterialSlotList.size() > 1))
        {
            continue;
        }
        gPipeline.captureMetalWorldGBufferBatch(*info, batch_textures);
    }
    gPipeline.endMetalWorldGBufferPass();
}
#endif

//===============================
//DEFERRED IMPLEMENTATION
//===============================

S32 LLDrawPoolSimple::getNumDeferredPasses()
{
    return 1;
}

void LLDrawPoolSimple::renderDeferred(S32)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
#if LL_DARWIN
    capture_metal_gbuffer_batches(
        LLRenderPass::PASS_SIMPLE, false, false, true);
    capture_metal_gbuffer_batches(
        LLRenderPass::PASS_SIMPLE_RIGGED, true, false, true);
#endif
}

void LLDrawPoolAlphaMask::renderDeferred(S32)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
#if LL_DARWIN
    capture_metal_gbuffer_batches(
        LLRenderPass::PASS_ALPHA_MASK, false, true, true, true);
    capture_metal_gbuffer_batches(
        LLRenderPass::PASS_ALPHA_MASK_RIGGED, true, true, true, true);
#endif
}

// grass drawpool
LLDrawPoolGrass::LLDrawPoolGrass() :
 LLRenderPass(POOL_GRASS)
{

}

void LLDrawPoolGrass::renderDeferred(S32)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
#if LL_DARWIN
    capture_metal_gbuffer_batches(
        LLRenderPass::PASS_GRASS, false, true, false);
#endif
}


// Fullbright drawpool
LLDrawPoolFullbright::LLDrawPoolFullbright() :
    LLRenderPass(POOL_FULLBRIGHT)
{
}

void LLDrawPoolFullbright::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    LLGLSLShader* shader = nullptr;
    if (LLPipeline::sRenderingHUDs)
    {
        shader = &gHUDFullbrightProgram;
    }
    else
    {
        shader = gDeferredFullbrightProgram.selectVariant();
    }

    gGL.setSceneBlendType(LLRender::BT_ALPHA);

    // render static
    shader->bind();
#if LL_DARWIN
    gPipeline.beginMetalImpostorFullbrightPass(false, false);
#endif
    pushBatches(LLRenderPass::PASS_FULLBRIGHT, true, true);
#if LL_DARWIN
    gPipeline.endMetalWorldGBufferPass();
#endif

    if (!LLPipeline::sRenderingHUDs)
    {
        // render rigged
        shader->bind(true);
#if LL_DARWIN
        gPipeline.beginMetalImpostorFullbrightPass(true, false);
#endif
        pushRiggedBatches(LLRenderPass::PASS_FULLBRIGHT_RIGGED, true, true);
#if LL_DARWIN
        gPipeline.endMetalWorldGBufferPass();
#endif
    }
}

void LLDrawPoolFullbrightAlphaMask::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    LLGLSLShader* shader = nullptr;
    if (LLPipeline::sRenderingHUDs)
    {
        shader = &gHUDFullbrightAlphaMaskProgram;
    }
    else
    {
        shader = gDeferredFullbrightAlphaMaskProgram.selectVariant();
    }

    LLGLDisable blend(GL_BLEND);

    // Outside the alpha pool LLFace stores shininess in the vertex alpha, which the mask must not
    // test (fullbrightF.glsl); the alpha pool, drawing with the same program, sets it back.

    // render static
    shader->bind();
    shader->uniform1f(LLShaderMgr::VERTEX_ALPHA_COVERAGE, 0.f);
#if LL_DARWIN
    gPipeline.beginMetalImpostorFullbrightPass(false, true);
#endif
    pushMaskBatches(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK, true, true);
#if LL_DARWIN
    gPipeline.endMetalWorldGBufferPass();
#endif

    if (!LLPipeline::sRenderingHUDs)
    {
        // render rigged
        shader->bind(true);
        LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::VERTEX_ALPHA_COVERAGE, 0.f);
#if LL_DARWIN
        gPipeline.beginMetalImpostorFullbrightPass(true, true);
#endif
        pushRiggedMaskBatches(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK_RIGGED, true, true);
#if LL_DARWIN
        gPipeline.endMetalWorldGBufferPass();
#endif
    }
}

