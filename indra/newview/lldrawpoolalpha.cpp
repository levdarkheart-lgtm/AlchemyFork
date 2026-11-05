/**
 * @file lldrawpoolalpha.cpp
 * @brief LLDrawPoolAlpha class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; version 2.1.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "lldrawpoolalpha.h"

#include "llcubemap.h"
#include "lldrawable.h"
#include "llenvironment.h"
#include "llface.h"
#include "llspatialpartition.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewertexture.h"
#include "llvoavatar.h"
#include "pipeline.h"

extern bool gCubeSnapshot;

bool LLDrawPoolAlpha::sShowDebugAlpha = false;
LLVector4 LLDrawPoolAlpha::sWaterPlane;

namespace
{
constexpr F32 MINIMUM_ALPHA = 0.004f;
}

LLDrawPoolAlpha::LLDrawPoolAlpha(U32 type)
: LLRenderPass(type)
{
}

LLDrawPoolAlpha::~LLDrawPoolAlpha() = default;

S32 LLDrawPoolAlpha::getNumPostDeferredPasses()
{
    return 1;
}

void LLDrawPoolAlpha::prerender()
{
}

void LLDrawPoolAlpha::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    (void)pass;

#if LL_DARWIN
    // The alpha pool is now a direct native producer. No GLSL program is
    // prepared and no GL draw is issued merely to trigger a Metal hook.
    gPipeline.beginMetalImpostorAlphaPhase();

    const bool pre_water =
        getType() == LLDrawPool::POOL_ALPHA_PRE_WATER;
    const bool post_water =
        getType() == LLDrawPool::POOL_ALPHA_POST_WATER;
    if ((!pre_water && !post_water) ||
        (pre_water && LLPipeline::isWaterClip()))
    {
        return;
    }

    if (LLPipeline::sRenderingHUDs &&
        !gGL.isMetalUITextureCaptureAuthoritative())
    {
        // The visible UI owner is Metal-only. An unavailable native capture
        // must not resurrect the retired GL HUD draw.
        return;
    }

    if (pre_water)
    {
        gPipeline.beginMetalWorldPreWaterAlphaPhase();
    }
    else
    {
        gPipeline.beginMetalWorldPostWaterAlphaPhase();
    }

    // Preserve the retained ordering: rigged alpha first writes depth, then
    // the ordinary sorted sweep. HUD alpha has no rigged pass.
    if (!LLPipeline::sRenderingHUDs)
    {
        captureMetalBatches(true, false);
    }
    captureMetalBatches(false, false);

    if (LLPipeline::sRenderingHUDs || post_water)
    {
        renderDebugAlpha();
    }

    // The DoF owner consumes a second post-water, non-rigged depth-only
    // replay with the historical 0.33 coverage floor for flat alpha. Native
    // material and PBR batches retain their material coverage semantics.
    if (!LLPipeline::sImpostorRender && LLPipeline::RenderDepthOfField &&
        !gCubeSnapshot && !LLPipeline::sRenderingHUDs && post_water)
    {
        captureMetalBatches(false, true);
    }

    if (pre_water)
    {
        gPipeline.endMetalWorldPreWaterAlphaPhase();
    }
    else
    {
        gPipeline.endMetalWorldPostWaterAlphaPhase();
    }
#endif
}

void LLDrawPoolAlpha::captureMetalBatches(bool rigged, bool depth_only)
{
#if LL_DARWIN
    LLCullResult::sg_iterator at = rigged
        ? gPipeline.beginRiggedAlphaGroups()
        : gPipeline.beginAlphaGroups();
    LLCullResult::sg_iterator end = rigged
        ? gPipeline.endRiggedAlphaGroups()
        : gPipeline.endAlphaGroups();

    const F32 water_height = LLEnvironment::instance().getWaterHeight();
    bool above_water =
        getType() == LLDrawPool::POOL_ALPHA_POST_WATER;
    if (LLPipeline::sUnderWaterRender)
    {
        above_water = !above_water;
    }

    for (; at != end; ++at)
    {
        LLSpatialGroup* group = *at;
        if (!group || !group->getSpatialPartition() ||
            !group->getSpatialPartition()->mRenderByGroup ||
            group->isDead())
        {
            continue;
        }

        LLSpatialBridge* bridge =
            group->getSpatialPartition()->asBridge();
        const LLVector4a* extents = bridge
            ? bridge->getSpatialExtents() : group->getExtents();
        if (!LLPipeline::sRenderingHUDs)
        {
            if (above_water)
            {
                if (extents[1].getF32ptr()[2] < water_height)
                {
                    continue;
                }
            }
            else if (extents[0].getF32ptr()[2] > water_height)
            {
                continue;
            }
        }

        const S32 partition_type =
            group->getSpatialPartition()->mPartitionType;
        const bool particle_or_hud_particle =
            partition_type == LLViewerRegion::PARTITION_PARTICLE ||
            partition_type == LLViewerRegion::PARTITION_HUD_PARTICLE;
        const U32 draw_type = rigged
            ? LLRenderPass::PASS_ALPHA_RIGGED
            : LLRenderPass::PASS_ALPHA;
        LLSpatialGroup::drawmap_elem_t& draws =
            group->mDrawMap[draw_type];

        std::vector<LLDrawInfo*> legacy_emissive;
        std::vector<LLDrawInfo*> pbr_emissive;
        for (LLDrawInfo* info : draws)
        {
            if (!info || static_cast<bool>(info->mAvatar) != rigged)
            {
                continue;
            }

            LLGLTFMaterial* gltf = info->mGLTFMaterial.get();
            const bool pbr_alpha = gltf &&
                gltf->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_BLEND;

            if (LLPipeline::sRenderingHUDs)
            {
                LLRenderPass::applyModelMatrix(*info);
                const bool custom_zero_cutoff =
                    !LLPipeline::sImpostorRender &&
                    info->mBlendFuncSrc != LLRender::BF_SOURCE_ALPHA &&
                    info->mBlendFuncDst != LLRender::BF_SOURCE_ALPHA;
                captureMetalHUDBatch(
                    *info, true, false, !pbr_alpha, pbr_alpha,
                    custom_zero_cutoff ? 0.f : MINIMUM_ALPHA,
                    particle_or_hud_particle);
            }
            else if (pbr_alpha)
            {
                gPipeline.captureMetalImpostorPBRAlphaBatch(
                    *info, particle_or_hud_particle, depth_only);
            }
            else if (info->mMaterial)
            {
                gPipeline.captureMetalImpostorLegacyMaterialAlphaBatch(
                    *info, particle_or_hud_particle, depth_only);
            }
            else
            {
                gPipeline.captureMetalImpostorFlatAlphaBatch(
                    *info, particle_or_hud_particle, depth_only);
            }

            if (!depth_only &&
                getType() == LLDrawPool::POOL_ALPHA_POST_WATER &&
                !LLPipeline::alphaWritesCoverage() && info->mVertexBuffer &&
                info->mVertexBuffer->hasDataType(
                    LLVertexBuffer::TYPE_EMISSIVE))
            {
                (info->mGLTFMaterial.notNull()
                     ? pbr_emissive : legacy_emissive).push_back(info);
            }
        }

        // Glow replay is group-local and follows the sorted colour batches.
        for (LLDrawInfo* info : legacy_emissive)
        {
            gPipeline.captureMetalWorldPostWaterLegacyGlowBatch(
                *info, particle_or_hud_particle);
        }
        for (LLDrawInfo* info : pbr_emissive)
        {
            gPipeline.captureMetalWorldPostWaterPBRGlowBatch(
                *info, particle_or_hud_particle);
        }
    }
#else
    (void)rigged;
    (void)depth_only;
#endif
}

void LLDrawPoolAlpha::captureMetalReflectionProbe()
{
#if LL_DARWIN
    const bool pre_water =
        getType() == LLDrawPool::POOL_ALPHA_PRE_WATER;
    const bool post_water =
        getType() == LLDrawPool::POOL_ALPHA_POST_WATER;
    if ((!pre_water && !post_water) ||
        (pre_water && LLPipeline::isWaterClip()))
    {
        return;
    }

    if (pre_water)
    {
        gPipeline.beginMetalWorldPreWaterAlphaPhase();
    }
    else
    {
        gPipeline.beginMetalWorldPostWaterAlphaPhase();
    }
    captureMetalBatches(true, false);
    captureMetalBatches(false, false);
    if (pre_water)
    {
        gPipeline.endMetalWorldPreWaterAlphaPhase();
    }
    else
    {
        gPipeline.endMetalWorldPostWaterAlphaPhase();
    }
#endif
}

void LLDrawPoolAlpha::renderDebugAlpha()
{
#if LL_DARWIN
    if (!sShowDebugAlpha || gCubeSnapshot)
    {
        return;
    }

    LLViewerFetchedTexture* smoke =
        LLViewerFetchedTexture::getSmokeImage();
    const auto capture_group_alpha = [&](bool rigged)
    {
        LLCullResult::sg_iterator at = rigged
            ? gPipeline.beginRiggedAlphaGroups()
            : gPipeline.beginAlphaGroups();
        LLCullResult::sg_iterator end = rigged
            ? gPipeline.endRiggedAlphaGroups()
            : gPipeline.endAlphaGroups();
        const U32 type = rigged
            ? LLRenderPass::PASS_ALPHA_RIGGED
            : LLRenderPass::PASS_ALPHA;
        for (; at != end; ++at)
        {
            LLSpatialGroup* group = *at;
            if (!group || !group->getSpatialPartition() ||
                !group->getSpatialPartition()->mRenderByGroup ||
                group->isDead())
            {
                continue;
            }
            for (LLDrawInfo* info : group->mDrawMap[type])
            {
                if (!info || static_cast<bool>(info->mAvatar) != rigged)
                {
                    continue;
                }
                LLRenderPass::applyModelMatrix(*info);
                gPipeline.captureMetalDebugAlphaBatch(
                    *info, smoke, LLColor4::red);
            }
        }
    };
    const auto capture_render_map =
        [&](U32 type, bool rigged, const LLColor4& color)
    {
        auto* at = gPipeline.beginRenderMap(type);
        auto* end = gPipeline.endRenderMap(type);
        while (at != end)
        {
            LLDrawInfo* info = *at;
            LLCullResult::increment_iterator(at, end);
            if (!info || static_cast<bool>(info->mAvatar) != rigged)
            {
                continue;
            }
            LLRenderPass::applyModelMatrix(*info);
            gPipeline.captureMetalDebugAlphaBatch(*info, smoke, color);
        }
    };

    capture_group_alpha(false);
    capture_group_alpha(true);

    const LLColor4 red(1.f, 0.f, 0.f, 1.f);
    const LLColor4 blue(0.f, 0.f, 1.f, 1.f);
    const LLColor4 green(0.f, 1.f, 0.f, 1.f);
    capture_render_map(LLRenderPass::PASS_ALPHA_MASK, false, red);
    capture_render_map(LLRenderPass::PASS_ALPHA_INVISIBLE, false, red);
    capture_render_map(
        LLRenderPass::PASS_MATERIAL_ALPHA_MASK, false, blue);
    capture_render_map(LLRenderPass::PASS_NORMMAP_MASK, false, blue);
    capture_render_map(LLRenderPass::PASS_SPECMAP_MASK, false, blue);
    capture_render_map(LLRenderPass::PASS_NORMSPEC_MASK, false, blue);
    capture_render_map(
        LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK, false, blue);
    capture_render_map(
        LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK, false, blue);
    capture_render_map(LLRenderPass::PASS_INVISIBLE, false, green);

    capture_render_map(
        LLRenderPass::PASS_ALPHA_MASK_RIGGED, true, red);
    capture_render_map(
        LLRenderPass::PASS_ALPHA_INVISIBLE_RIGGED, true, red);
    capture_render_map(
        LLRenderPass::PASS_MATERIAL_ALPHA_MASK_RIGGED, true, blue);
    capture_render_map(
        LLRenderPass::PASS_NORMMAP_MASK_RIGGED, true, blue);
    capture_render_map(
        LLRenderPass::PASS_SPECMAP_MASK_RIGGED, true, blue);
    capture_render_map(
        LLRenderPass::PASS_NORMSPEC_MASK_RIGGED, true, blue);
    capture_render_map(
        LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK_RIGGED, true, blue);
    capture_render_map(
        LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK_RIGGED, true, blue);
    capture_render_map(
        LLRenderPass::PASS_INVISIBLE_RIGGED, true, green);
#endif
}
