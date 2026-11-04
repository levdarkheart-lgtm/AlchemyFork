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

#include "lldrawpoolalpha.h"

#include "llglheaders.h"
#include "llviewercontrol.h"
#include "llcriticaldamp.h"
#include "llrender.h"

#include "llcubemap.h"
#include "llsky.h"
#include "lldrawable.h"
#include "llface.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"    // For debugging
#include "llviewerobjectlist.h" // For debugging
#include "llviewerwindow.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llviewerregion.h"
#include "lldrawpoolwater.h"
#include "llspatialpartition.h"
#include "llglcommonfunc.h"
#include "llvoavatar.h"

#include "llenvironment.h"

bool LLDrawPoolAlpha::sShowDebugAlpha = false;

#define current_shader (LLGLSLShader::sCurBoundShaderPtr)

LLVector4 LLDrawPoolAlpha::sWaterPlane;

// minimum alpha before discarding a fragment
static const F32 MINIMUM_ALPHA = 0.004f; // ~ 1/255

// static
void LLDrawPoolAlpha::setCoverageCutoff(LLGLSLShader* shader)
{
    shader->setMinimumAlpha(MINIMUM_ALPHA);
}

LLDrawPoolAlpha::LLDrawPoolAlpha(U32 type) :
        LLRenderPass(type), target_shader(NULL),
        mColorSFactor(LLRender::BF_UNDEF), mColorDFactor(LLRender::BF_UNDEF),
        mAlphaSFactor(LLRender::BF_UNDEF), mAlphaDFactor(LLRender::BF_UNDEF)
{

}

LLDrawPoolAlpha::~LLDrawPoolAlpha()
{
}


void LLDrawPoolAlpha::prerender()
{
    mShaderLevel = LLViewerShaderMgr::instance()->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

S32 LLDrawPoolAlpha::getNumPostDeferredPasses()
{
    return 1;
}

// set some common parameters on the given shader to prepare for alpha rendering
static void prepare_alpha_shader(LLGLSLShader* shader, bool deferredEnvironment, F32 water_sign)
{
    static LLCachedControl<F32> displayGamma(gSavedSettings, "RenderDeferredDisplayGamma");
    F32 gamma = displayGamma;

    // Does this deferred shader need environment uniforms set such as sun_dir, etc. ?
    // NOTE: We don't actually need a gbuffer since we are doing forward rendering (for transparency) post deferred rendering
    // TODO: bindDeferredShader() probably should have the updating of the environment uniforms factored out into updateShaderEnvironmentUniforms()
    // i.e. shaders\class1\deferred\alphaF.glsl
    if (deferredEnvironment)
    {
        shader->mCanBindFast = false;
    }

    shader->bind();
    shader->uniform1f(LLShaderMgr::DISPLAY_GAMMA, (gamma > 0.1f) ? 1.0f / gamma : (1.0f / 2.2f));

    if (LLPipeline::sRenderingHUDs || LLPipeline::sImpostorRender)
    { // for HUD attachments, only the pre-water pass is executed and we never want to clip anything
      //
      // An impostor bake is in the same position for the opposite reason: generateImpostor
      // clears RENDER_TYPE_ALPHA_PRE_WATER, so only the post-water pool runs and it clips
      // everything below the plane -- with the pre-water pool disabled, nothing draws it
      // back. An impostored avatar standing waist-deep lost every blended attachment below
      // the waterline in a clean horizontal cut, while its opaque parts (which never clip)
      // stayed. The bake has no water in it to sort against, so clipping has nothing to do.
        LLVector4 near_clip(0, 0, -1, 0);
        shader->uniform1f(LLShaderMgr::WATER_WATERSIGN, 1.f);
        shader->uniform4fv(LLShaderMgr::WATER_WATERPLANE, 1, near_clip.mV);
    }
    else
    {
        shader->uniform1f(LLShaderMgr::WATER_WATERSIGN, water_sign);
        shader->uniform4fv(LLShaderMgr::WATER_WATERPLANE, 1, LLDrawPoolAlpha::sWaterPlane.mV);
    }

    // Same cutoff in a bake as anywhere else.
    //
    // The 10% impostor threshold existed because the coverage mask used to flatten every
    // covered texel to fully opaque: a 5%-opacity fragment would have been promoted to solid,
    // so discarding it early was the lesser evil. Now that the bake accumulates real coverage
    // and the composite divides it back out, that fragment contributes 0.05 and composites
    // correctly -- and the threshold is just a rule that deletes faint glass, veils and gauze
    // the moment an avatar impostors, then restores them when it stops.
    shader->setMinimumAlpha(MINIMUM_ALPHA);

    // The alpha pool's faces carry coverage in their vertex alpha, which the masked fullbright
    // program tests with the texture's when the pool draws with it (fullbrightF.glsl).
    shader->uniform1f(LLShaderMgr::VERTEX_ALPHA_COVERAGE, 1.f);

    //also prepare rigged variant
    if (shader->mRiggedVariant && shader->mRiggedVariant != shader)
    {
        prepare_alpha_shader(shader->mRiggedVariant, deferredEnvironment, water_sign);
    }
}

extern bool gCubeSnapshot;

void LLDrawPoolAlpha::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

#if LL_DARWIN
    // Avatar impostors close their opaque-forward phase here. The retained
    // main-world owner instead remains open through its ordered alpha/water
    // sequence and is encoded at renderGeomPostDeferred's boundary.
    gPipeline.beginMetalImpostorAlphaPhase();
#endif

    if (LLPipeline::isWaterClip() && getType() == LLDrawPool::POOL_ALPHA_PRE_WATER)
    { // don't render alpha objects on the other side of the water plane if water is opaque
        return;
    }

#if LL_DARWIN
    const bool metal_pre_water =
        getType() == LLDrawPool::POOL_ALPHA_PRE_WATER;
    const bool metal_post_water =
        getType() == LLDrawPool::POOL_ALPHA_POST_WATER;
    if (metal_pre_water)
    {
        gPipeline.beginMetalWorldPreWaterAlphaPhase();
    }
    else if (metal_post_water)
    {
        gPipeline.beginMetalWorldPostWaterAlphaPhase();
    }
#endif

    F32 water_sign = 1.f;

    if (getType() == LLDrawPool::POOL_ALPHA_PRE_WATER)
    {
        water_sign = -1.f;
    }

    if (LLPipeline::sUnderWaterRender)
    {
        water_sign *= -1.f;
    }

    // prepare shaders
    llassert(LLPipeline::sRenderDeferred);

    emissive_shader = &gDeferredEmissiveProgram;
    prepare_alpha_shader(emissive_shader, false, water_sign);

    pbr_emissive_shader = &gPBRGlowProgram;
    prepare_alpha_shader(pbr_emissive_shader, false, water_sign);


    fullbright_shader   =
        (LLPipeline::sImpostorRender) ? &gDeferredFullbrightAlphaMaskProgram :
        (LLPipeline::sRenderingHUDs) ? &gHUDFullbrightAlphaMaskAlphaProgram :
        &gDeferredFullbrightAlphaMaskAlphaProgram;
    fullbright_shader = fullbright_shader->selectVariant();
    prepare_alpha_shader(fullbright_shader, true, water_sign);

    simple_shader   =
        (LLPipeline::sImpostorRender) ? &gDeferredAlphaImpostorProgram :
        (LLPipeline::sRenderingHUDs) ? &gHUDAlphaProgram :
        &gDeferredAlphaProgram;

    // select the classic-lighting clone before preparation so the water plane and minimum
    // alpha uniforms land on the program that actually binds
    simple_shader = simple_shader->selectVariant();

    prepare_alpha_shader(simple_shader, true, water_sign); //prime simple shader (loads shadow relevant uniforms)

    // prepare_alpha_shader() recurses into the rigged variant, so priming the bases
    // covers every corner
    for (int i = 0; i < LLMaterial::SHADER_COUNT; ++i)
    {
        prepare_alpha_shader(gDeferredMaterialProgram[i].selectVariant(), true, water_sign);
    }

    pbr_shader =
        (LLPipeline::sRenderingHUDs) ? &gHUDPBRAlphaProgram :
        (LLPipeline::sImpostorRender) ? &gDeferredPBRAlphaImpostorProgram :
        &gDeferredPBRAlphaProgram;

    pbr_shader = pbr_shader->selectVariant();

    prepare_alpha_shader(pbr_shader, true, water_sign);

    // explicitly unbind here so render loop doesn't make assumptions about the last shader
    // already being setup for rendering
    LLGLSLShader::unbind();

    if (!LLPipeline::sRenderingHUDs)
    {
        // first pass, render rigged objects only and render to depth buffer
        forwardRender(true);
    }

    // second pass, regular forward alpha rendering
    forwardRender();

    // final pass, render to depth for depth of field effects
    if (!LLPipeline::sImpostorRender && LLPipeline::RenderDepthOfField && !gCubeSnapshot && !LLPipeline::sRenderingHUDs && getType() == LLDrawPool::POOL_ALPHA_POST_WATER)
    {
        //update depth buffer sampler
        simple_shader = fullbright_shader = gDeferredFullbrightAlphaMaskProgram.selectVariant();

        simple_shader->bind();
        simple_shader->setMinimumAlpha(0.33f);
        simple_shader->uniform1f(LLShaderMgr::VERTEX_ALPHA_COVERAGE, 1.f);

        // mask off color buffer writes as we're only writing to depth buffer
        gGL.setColorMask(false, false);

        // If the face is more than 90% transparent, then don't update the Depth buffer for Dof
        // We don't want the nearly invisible objects to cause of DoF effects
        renderAlpha(getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX | LLVertexBuffer::MAP_TANGENT | LLVertexBuffer::MAP_TEXCOORD1 | LLVertexBuffer::MAP_TEXCOORD2,
            true); // <--- discard mostly transparent faces

        gGL.setColorMask(true, false);
    }

#if LL_DARWIN
    if (metal_pre_water)
    {
        gPipeline.endMetalWorldPreWaterAlphaPhase();
    }
    else if (metal_post_water)
    {
        gPipeline.endMetalWorldPostWaterAlphaPhase();
    }
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

    LLEnvironment& environment = LLEnvironment::instance();
    const F32 water_height = environment.getWaterHeight();
    bool above_water = post_water;
    if (LLPipeline::sUnderWaterRender)
    {
        above_water = !above_water;
    }

    const auto capture_pass = [&](bool rigged)
    {
        LLCullResult::sg_iterator at = rigged
            ? gPipeline.beginRiggedAlphaGroups()
            : gPipeline.beginAlphaGroups();
        LLCullResult::sg_iterator end = rigged
            ? gPipeline.endRiggedAlphaGroups()
            : gPipeline.endAlphaGroups();

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

            const S32 partition_type =
                group->getSpatialPartition()->mPartitionType;
            const bool particle =
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
                if (gltf && gltf->mAlphaMode ==
                                LLGLTFMaterial::ALPHA_MODE_BLEND)
                {
                    gPipeline.captureMetalImpostorPBRAlphaBatch(
                        *info, particle, false);
                }
                else if (info->mMaterial)
                {
                    gPipeline.captureMetalImpostorLegacyMaterialAlphaBatch(
                        *info, particle, false);
                }
                else
                {
                    gPipeline.captureMetalImpostorFlatAlphaBatch(
                        *info, particle, false);
                }

                if (post_water && !LLPipeline::alphaWritesCoverage() &&
                    info->mVertexBuffer &&
                    info->mVertexBuffer->hasDataType(
                        LLVertexBuffer::TYPE_EMISSIVE))
                {
                    (info->mGLTFMaterial.notNull()
                         ? pbr_emissive : legacy_emissive).push_back(info);
                }
            }

            // The retained pool replays emissive geometry after the sorted
            // colour draws for each spatial group. Keep that exact boundary
            // in the native transaction; unlike GL, no shader excursion is
            // required to do it.
            for (LLDrawInfo* info : legacy_emissive)
            {
                gPipeline.captureMetalWorldPostWaterLegacyGlowBatch(
                    *info, particle);
            }
            for (LLDrawInfo* info : pbr_emissive)
            {
                gPipeline.captureMetalWorldPostWaterPBRGlowBatch(
                    *info, particle);
            }
        }
    };

    // Match renderPostDeferred: rigged transparent geometry writes depth
    // first, followed by the ordinary sorted transparent sweep.
    capture_pass(true);
    capture_pass(false);

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

void LLDrawPoolAlpha::forwardRender(bool rigged)
{
    gPipeline.enableLightsDynamic();

    LLGLSPipelineAlpha gls_pipeline_alpha;

    //enable writing to alpha for emissive effects
    gGL.setColorMask(true, true);

    bool write_depth = rigged
        || getType() == LLDrawPoolAlpha::POOL_ALPHA_PRE_WATER; // needed for accurate water fog


    LLGLDepthTest depth(GL_TRUE, write_depth ? GL_TRUE : GL_FALSE);

    // In a bake, the destination alpha accumulates COVERAGE -- src.a + (1-src.a)*dst.a -- so
    // the impostor stores colour premultiplied by coverage and the composite recovers the
    // surface colour by dividing it back out (deferred/impostorF.glsl). Blending over the
    // cleared black background is what darkened hair and translucent edges, and treating them
    // as opaque afterwards is what froze that darkening into a halo.
    //
    // The accumulation starts from what generateImpostor's coverage stamp left: 1 wherever
    // opaque geometry sits (so it stays 1, correctly), 0 over background (so it becomes this
    // surface's coverage). Depth writing is untouched by any of that -- the stamp runs before
    // this pass, so blended geometry occludes normally again.
    //
    // A snapshot cutout's main pass keeps coverage the same way, seeded by a pass of its
    // own (LLPipeline::seedCutoutCoverageFromDepth), so its subject can be lifted off the
    // frame.
    //
    // Everywhere else the alpha channel carries glow, and this suppresses source alpha so a
    // blended surface does not erase the glow already accumulated behind it.
    mColorSFactor = LLRender::BF_SOURCE_ALPHA;           // } regular alpha blend
    mColorDFactor = LLRender::BF_ONE_MINUS_SOURCE_ALPHA; // }
    mAlphaSFactor = LLPipeline::alphaWritesCoverage()          // } glow suppression /
                        ? LLRender::BF_ONE                      // } bake coverage
                        : LLRender::BF_ZERO;                    // }
    mAlphaDFactor = LLRender::BF_ONE_MINUS_SOURCE_ALPHA;       // }
    gGL.blendFunc(mColorSFactor, mColorDFactor, mAlphaSFactor, mAlphaDFactor);

    // If the face is more than 90% transparent, then don't update the Depth buffer for Dof
    // We don't want the nearly invisible objects to cause of DoF effects
    renderAlpha(getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX | LLVertexBuffer::MAP_TANGENT | LLVertexBuffer::MAP_TEXCOORD1 | LLVertexBuffer::MAP_TEXCOORD2, false, rigged);

    gGL.setColorMask(true, false);

    if (!rigged && (LLPipeline::sRenderingHUDs || getType() == LLDrawPoolAlpha::POOL_ALPHA_POST_WATER))
    { //render "highlight alpha" on final non-rigged pass for non-HUDs (HUDs only run pre-water alpha pass)
        // NOTE -- hacky call here protected by !rigged instead of alongside "forwardRender"
        // so renderDebugAlpha is executed while gls_pipeline_alpha and depth GL state
        // variables above are still in scope
        renderDebugAlpha();
    }
}

void LLDrawPoolAlpha::renderDebugAlpha()
{
    if (sShowDebugAlpha && !gCubeSnapshot)
    {
        LLViewerFetchedTexture* smoke =
            LLViewerFetchedTexture::getSmokeImage();
        const auto draw_debug_batches =
            [this, smoke](U32 type, bool rigged,
                          const LLColor4& color)
        {
            const LLVOAvatar* last_avatar = nullptr;
            U64 last_mesh_id = 0;
            bool skip_last_skin = false;
            auto* begin = gPipeline.beginRenderMap(type);
            auto* end = gPipeline.endRenderMap(type);
            for (auto* iterator = begin; iterator != end; )
            {
                LLDrawInfo* params = *iterator;
                LLCullResult::increment_iterator(iterator, end);
                if (!params ||
                    (rigged && !uploadMatrixPalette(
                        params->mAvatar, params->mSkinInfo,
                        last_avatar, last_mesh_id, skip_last_skin)))
                {
                    continue;
                }
#if LL_DARWIN
                applyModelMatrix(*params);
                gPipeline.captureMetalDebugAlphaBatch(
                    *params, smoke, color);
#endif
                pushUntexturedBatch(*params);
            }
        };

        gHighlightProgram.bind();
        const LLColor4 red(1, 0, 0, 1);
        const LLColor4 blue(0, 0, 1, 1);
        const LLColor4 green(0, 1, 0, 1);
        gGL.diffuseColor4fv(red.mV);
        gGL.getTextureSlot(0)->bindFast(smoke, ALSamplers::AnisoWrap);

        renderAlphaHighlight();

        draw_debug_batches(LLRenderPass::PASS_ALPHA_MASK, false, red);
        draw_debug_batches(LLRenderPass::PASS_ALPHA_INVISIBLE, false, red);

        // Material alpha mask
        gGL.diffuseColor4fv(blue.mV);
        draw_debug_batches(
            LLRenderPass::PASS_MATERIAL_ALPHA_MASK, false, blue);
        draw_debug_batches(LLRenderPass::PASS_NORMMAP_MASK, false, blue);
        draw_debug_batches(LLRenderPass::PASS_SPECMAP_MASK, false, blue);
        draw_debug_batches(LLRenderPass::PASS_NORMSPEC_MASK, false, blue);
        draw_debug_batches(
            LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK, false, blue);
        draw_debug_batches(
            LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK, false, blue);

        gGL.diffuseColor4fv(green.mV);
        draw_debug_batches(LLRenderPass::PASS_INVISIBLE, false, green);

        gHighlightProgram.riggedVariant()->bind();
        gGL.diffuseColor4fv(red.mV);

        draw_debug_batches(
            LLRenderPass::PASS_ALPHA_MASK_RIGGED, true, red);
        draw_debug_batches(
            LLRenderPass::PASS_ALPHA_INVISIBLE_RIGGED, true, red);

        // Material alpha mask
        gGL.diffuseColor4fv(blue.mV);
        draw_debug_batches(
            LLRenderPass::PASS_MATERIAL_ALPHA_MASK_RIGGED, true, blue);
        draw_debug_batches(
            LLRenderPass::PASS_NORMMAP_MASK_RIGGED, true, blue);
        draw_debug_batches(
            LLRenderPass::PASS_SPECMAP_MASK_RIGGED, true, blue);
        draw_debug_batches(
            LLRenderPass::PASS_NORMSPEC_MASK_RIGGED, true, blue);
        draw_debug_batches(
            LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK_RIGGED, true, blue);
        draw_debug_batches(
            LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK_RIGGED, true, blue);

        gGL.diffuseColor4fv(green.mV);
        draw_debug_batches(
            LLRenderPass::PASS_INVISIBLE_RIGGED, true, green);
        LLGLSLShader::sCurBoundShaderPtr->unbind();
    }
}

void LLDrawPoolAlpha::renderAlphaHighlight()
{
    for (int pass = 0; pass < 2; ++pass)
    { //two passes, one rigged and one not
        const LLVOAvatar* lastAvatar = nullptr;
        U64 lastMeshId = 0;
        bool skipLastSkin = false;

        LLCullResult::sg_iterator begin = pass == 0 ? gPipeline.beginAlphaGroups() : gPipeline.beginRiggedAlphaGroups();
        LLCullResult::sg_iterator end = pass == 0 ? gPipeline.endAlphaGroups() : gPipeline.endRiggedAlphaGroups();

        for (LLCullResult::sg_iterator i = begin; i != end; ++i)
        {
            LLSpatialGroup* group = *i;
            if (group->getSpatialPartition()->mRenderByGroup &&
                !group->isDead())
            {
                LLSpatialGroup::drawmap_elem_t& draw_info = group->mDrawMap[LLRenderPass::PASS_ALPHA+pass]; // <-- hacky + pass to use PASS_ALPHA_RIGGED on second pass

                for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(); k != draw_info.end(); ++k)
                {
                    LLDrawInfo& params = **k;

                    bool rigged = (params.mAvatar != nullptr);
                    gHighlightProgram.bind(rigged);

                    if (rigged)
                    {
                        if (!uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
                        { // failed to upload matrix palette, skip rendering
                            continue;
                        }
                    }

                    gGL.diffuseColor4f(1, 0, 0, 1);
                    LLRenderPass::applyModelMatrix(params);
#if LL_DARWIN
                    gPipeline.captureMetalDebugAlphaBatch(
                        params, LLViewerFetchedTexture::getSmokeImage(),
                        LLColor4::red);
#endif
                    params.mVertexBuffer->setBuffer();
                    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
                }
            }
        }
    }

    // make sure static version of highlight shader is bound before returning
    gHighlightProgram.bind();
}

inline bool IsFullbright(LLDrawInfo& params)
{
    return params.mFullbright;
}

inline bool IsMaterial(LLDrawInfo& params)
{
    return params.mMaterial != nullptr;
}

inline bool IsEmissive(LLDrawInfo& params)
{
    return params.mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_EMISSIVE);
}

inline void Draw(LLDrawInfo* draw, U32 mask)
{
    draw->mVertexBuffer->setBuffer();
    LLRenderPass::applyModelMatrix(*draw);
    draw->mVertexBuffer->drawRange(LLRender::TRIANGLES, draw->mStart, draw->mEnd, draw->mCount, draw->mOffset);
}

bool LLDrawPoolAlpha::TexSetup(LLDrawInfo* draw, bool use_material, LLGLSLShader* shader, ALSampler diffuse_key)
{
    llassert(shader);
    llassert(shader == current_shader); // the caller must have bound what it is binding for

    bool tex_setup = false;

    if (draw->mGLTFMaterial)
    {
        // A GLTF material binds its own maps through LLGLTFMaterial::bind; only the texture
        // matrix is left to us.
        if (draw->mTextureMatrix && (!gCubeSnapshot || gPipeline.mHeroProbeManager.isMirrorPass()))
        {
            tex_setup = true;
            gGL.matrixMode(LLRender::MM_TEXTURE0);
            gGL.loadMatrix((GLfloat*)draw->mTextureMatrix->mMatrix);
            gPipeline.countTextureMatrixOp(*draw->mTextureMatrix);
        }

        return tex_setup;
    }

    // Legacy Blinn-Phong maps. Bound unconditionally against neutral stand-ins when this draw
    // has none, because a program that DECLARES these samplers must not be left reading
    // whatever the previous draw put on those units.
    //
    // Which programs declare them is read from the program itself -- bindTexture resolves the
    // uniform through the shader and is a no-op when the channel is absent -- rather than by
    // testing `shader == simple_shader || shader == simple_shader->mRiggedVariant`. That
    // comparison silently changed meaning whenever a variant was added, and it could not see
    // a material program at all.
    const bool material_maps = !LLPipeline::sRenderingHUDs && use_material;

    // Decode the diffuse for the forward writers converted to shade in linear (see
    // LLGLSLShader::mLinearDiffuse) -- gDeferredAlphaProgram and the BLEND material programs.
    // The FOR_IMPOSTOR alpha program and the HUD programs leave it false and keep the sRGB
    // texel. Normal and specular are data / read-side-decoded and stay AnisoWrap.
    //
    // OR the bit onto the caller's key rather than replacing it. That key already carries the
    // addressing this draw needs -- particle groups pass AnisoClamp, because their quads span
    // the full [0,1] and wrap filtering bleeds the opposite edge in -- and replacing it would
    // silently cost them that clamp.
    if (shader->mLinearDiffuse)
    {
        diffuse_key = diffuse_key | ALSampler::SRGBDecode;
    }

    LLViewerTexture* normal_map = material_maps ? draw->mNormalMap.get() : nullptr;
    LLViewerTexture* specular_map = material_maps ? draw->mSpecularMap.get() : nullptr;

    shader->bindTexture(LLShaderMgr::BUMP_MAP,
                        normal_map ? normal_map : LLViewerFetchedTexture::sFlatNormalImagep.get(),
                        ALSamplers::AnisoWrap);
    // Spec decodes too now: legacy Blinn-Phong spec is sRGB colour, so filtering it in linear
    // is the same win as diffuse. The shader re-encodes for the deferred store; forward lights
    // with it linear. Normal is data and stays AnisoWrap.
    shader->bindTexture(LLShaderMgr::SPECULAR_MAP,
                        specular_map ? specular_map : LLViewerFetchedTexture::sWhiteImagep.get(),
                        ALSamplers::AnisoWrapSRGB);

    if (draw->mTextureList.size() > 1)
    {
        // Indexed batch: clamped to what this program declares, and null slots stood in for.
        bindIndexedTextures(*draw, shader);
    }
    else
    { //not batching textures or batch has only 1 texture -- might need a texture matrix
        if (draw->mTexture.notNull())
        {
            if (use_material)
            {
                shader->bindTexture(LLShaderMgr::DIFFUSE_MAP, draw->mTexture, diffuse_key);
            }
            else
            {
                gGL.getTextureSlot(0)->bindFast(draw->mTexture, diffuse_key);
            }

            if (draw->mTextureMatrix && (!gCubeSnapshot || gPipeline.mHeroProbeManager.isMirrorPass()))
            {
                tex_setup = true;
                gGL.matrixMode(LLRender::MM_TEXTURE0);
                gGL.loadMatrix((GLfloat*)draw->mTextureMatrix->mMatrix);
                gPipeline.countTextureMatrixOp(*draw->mTextureMatrix);
            }
        }
        else
        {
            gGL.getTextureSlot(0)->unbindFast();
        }
    }

    return tex_setup;
}

void LLDrawPoolAlpha::popTextureMatrix(bool tex_setup)
{
    if (tex_setup)
    {
        gGL.matrixMode(LLRender::MM_TEXTURE0);
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
}

void LLDrawPoolAlpha::drawEmissive(LLDrawInfo* draw)
{
    LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, 1.f);
    draw->mVertexBuffer->setBuffer();
    draw->mVertexBuffer->drawRange(LLRender::TRIANGLES, draw->mStart, draw->mEnd, draw->mCount, draw->mOffset);
}


void LLDrawPoolAlpha::renderEmissives(
    std::vector<LLDrawInfo*>& emissives, bool particle_or_hud_particle)
{
    emissive_shader->bind();
    emissive_shader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, 1.f);

    for (LLDrawInfo* draw : emissives)
    {
#if LL_DARWIN
        gPipeline.captureMetalWorldPostWaterLegacyGlowBatch(
            *draw, particle_or_hud_particle);
#endif
        bool tex_setup = TexSetup(draw, false, emissive_shader);
        drawEmissive(draw);
        popTextureMatrix(tex_setup);
    }
}

void LLDrawPoolAlpha::renderPbrEmissives(
    std::vector<LLDrawInfo*>& emissives, bool particle_or_hud_particle)
{
    pbr_emissive_shader->bind();

    for (LLDrawInfo* draw : emissives)
    {
#if LL_DARWIN
        gPipeline.captureMetalWorldPostWaterPBRGlowBatch(
            *draw, particle_or_hud_particle);
#endif
        llassert(draw->mGLTFMaterial);
        LLGLDisable cull_face(draw->mGLTFMaterial->mDoubleSided ? GL_CULL_FACE : 0);
        draw->mGLTFMaterial->bind(draw->mTexture);
        setup_texture_matrix(*draw);
        draw->mVertexBuffer->setBuffer();
        draw->mVertexBuffer->drawRange(LLRender::TRIANGLES, draw->mStart, draw->mEnd, draw->mCount, draw->mOffset);
        teardown_texture_matrix(*draw);
    }
}

void LLDrawPoolAlpha::renderRiggedEmissives(
    std::vector<LLDrawInfo*>& emissives, bool particle_or_hud_particle)
{
    LLGLDepthTest depth(GL_TRUE, GL_FALSE); //disable depth writes since "emissive" is additive so sorting doesn't matter
    LLGLSLShader* shader = emissive_shader->riggedVariant();
    shader->bind();
    shader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, 1.f);

    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    for (LLDrawInfo* draw : emissives)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("Emissives");

        if (uploadMatrixPalette(draw->mAvatar, draw->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
        {
#if LL_DARWIN
            gPipeline.captureMetalWorldPostWaterLegacyGlowBatch(
                *draw, particle_or_hud_particle);
#endif
            bool tex_setup = TexSetup(draw, false, shader);
            drawEmissive(draw);
            popTextureMatrix(tex_setup);
        }
    }
}

void LLDrawPoolAlpha::renderRiggedPbrEmissives(
    std::vector<LLDrawInfo*>& emissives, bool particle_or_hud_particle)
{
    LLGLDepthTest depth(GL_TRUE, GL_FALSE); //disable depth writes since "emissive" is additive so sorting doesn't matter
    pbr_emissive_shader->bind(true);

    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    for (LLDrawInfo* draw : emissives)
    {
        if (!uploadMatrixPalette(draw->mAvatar, draw->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
        { // failed to upload matrix palette, skip rendering
            continue;
        }

#if LL_DARWIN
        gPipeline.captureMetalWorldPostWaterPBRGlowBatch(
            *draw, particle_or_hud_particle);
#endif

        LLGLDisable cull_face(draw->mGLTFMaterial->mDoubleSided ? GL_CULL_FACE : 0);
        draw->mGLTFMaterial->bind(draw->mTexture);
        setup_texture_matrix(*draw);
        draw->mVertexBuffer->setBuffer();
        draw->mVertexBuffer->drawRange(LLRender::TRIANGLES, draw->mStart, draw->mEnd, draw->mCount, draw->mOffset);
        teardown_texture_matrix(*draw);
    }
}

void LLDrawPoolAlpha::renderAlpha(U32 mask, bool depth_only, bool rigged)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    bool initialized_lighting = false;
    bool light_enabled = true;

    // The last shader bound through bindDeferredShaderFast. Used to decide whether restoring
    // after the emissive excursion needs the deferred path (which re-binds the shadow maps)
    // or a plain bind().
    LLGLSLShader* deferred_bound_shader = nullptr;

    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    const LLGLSLShader* lastAvatarShader = nullptr;
    bool skipLastSkin = false;

    LLCullResult::sg_iterator begin;
    LLCullResult::sg_iterator end;

    if (rigged)
    {
        begin = gPipeline.beginRiggedAlphaGroups();
        end = gPipeline.endRiggedAlphaGroups();
    }
    else
    {
        begin = gPipeline.beginAlphaGroups();
        end = gPipeline.endAlphaGroups();
    }

    LLEnvironment& env = LLEnvironment::instance();
    F32 water_height = env.getWaterHeight();

    bool above_water = getType() == LLDrawPool::POOL_ALPHA_POST_WATER;
    if (LLPipeline::sUnderWaterRender)
    {
        above_water = !above_water;
    }


    for (LLCullResult::sg_iterator i = begin; i != end; ++i)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("renderAlpha - group");
        LLSpatialGroup* group = *i;
        llassert(group);
        llassert(group->getSpatialPartition());

        if (group->getSpatialPartition()->mRenderByGroup &&
            !group->isDead())
        {

            LLSpatialBridge* bridge = group->getSpatialPartition()->asBridge();
            const LLVector4a* ext = bridge ? bridge->getSpatialExtents() : group->getExtents();

            if (!LLPipeline::sRenderingHUDs) // ignore above/below water for HUD render
            {
                if (above_water)
                { // reject any spatial groups that have no part above water
                    if (ext[1].getF32ptr()[2] < water_height)
                    {
                        continue;
                    }
                }
                else
                { // reject any spatial groups that he no part below water
                    if (ext[0].getF32ptr()[2] > water_height)
                    {
                        continue;
                    }
                }
            }

            static std::vector<LLDrawInfo*> emissives;
            static std::vector<LLDrawInfo*> rigged_emissives;
            static std::vector<LLDrawInfo*> pbr_emissives;
            static std::vector<LLDrawInfo*> pbr_rigged_emissives;

            emissives.resize(0);
            rigged_emissives.resize(0);
            pbr_emissives.resize(0);
            pbr_rigged_emissives.resize(0);

            bool is_particle_or_hud_particle = group->getSpatialPartition()->mPartitionType == LLViewerRegion::PARTITION_PARTICLE
                                                      || group->getSpatialPartition()->mPartitionType == LLViewerRegion::PARTITION_HUD_PARTICLE;

            bool disable_cull = is_particle_or_hud_particle;
            LLGLDisable cull(disable_cull ? GL_CULL_FACE : 0);

            LLSpatialGroup::drawmap_elem_t& draw_info = rigged ? group->mDrawMap[LLRenderPass::PASS_ALPHA_RIGGED] : group->mDrawMap[LLRenderPass::PASS_ALPHA];

            for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(); k != draw_info.end(); ++k)
            {
                LLDrawInfo& params = **k;
                if ((bool)params.mAvatar != rigged)
                {
                    continue;
                }

                LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("ra - push batch");

                LLRenderPass::applyModelMatrix(params);

                LLMaterial* mat = NULL;
                LLGLTFMaterial *gltf_mat = params.mGLTFMaterial;

                LLGLDisable cull_face(gltf_mat && gltf_mat->mDoubleSided ? GL_CULL_FACE : 0);

                if (gltf_mat && gltf_mat->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_BLEND)
                {
#if LL_DARWIN
                    gPipeline.captureMetalImpostorPBRAlphaBatch(
                        params, is_particle_or_hud_particle, depth_only);
#endif
                    target_shader = pbr_shader;
                    if (params.mAvatar != nullptr)
                    {
                        target_shader = target_shader->riggedVariant();
                    }

                    // shader must be bound before LLGLTFMaterial::bind
                    if (current_shader != target_shader)
                    {
                        gPipeline.bindDeferredShaderFast(*target_shader);
                        deferred_bound_shader = target_shader;
                    }

                    params.mGLTFMaterial->bind(params.mTexture);
                }
                else
                {
                    // NOT flattened for impostors, unlike the GLTF path, however much the two
                    // look like they should match.
                    //
                    // A legacy BLEND material derives its OPACITY from lighting:
                    // materialF ends with al = max(diffcol.a, glare) * vertex_color.a,
                    // where glare accumulates reflection-probe environment and per-light
                    // specular. A shiny surface is opaque because it is shiny. Routing it to
                    // the flat impostor program -- which has no glare term and cannot have one
                    // without doing the lighting it exists to avoid -- drops those surfaces to
                    // their bare diffuse alpha, so they turn patchily see-through and blend
                    // against their own back faces.
                    //
                    // GLTF alpha has no such coupling (pbralphaF: a = basecolor.a *
                    // vertex_color.a), which is why FOR_IMPOSTOR is safe there and not here.
                    // The asymmetry is in the content model, not in the impostor code. Fixing
                    // it properly means a materialF FOR_IMPOSTOR variant that still runs
                    // the lighting for glare but emits flat albedo -- correct on both counts,
                    // at the cost of a permutation across the material program set.
                    //
                    // So legacy BLEND stays double-lit in a bake, which is a colour error we
                    // have always had, rather than a transparency error we would be adding.
                    mat = LLPipeline::sRenderingHUDs ? nullptr : params.mMaterial;

#if LL_DARWIN
                    if (mat)
                    {
                        gPipeline.captureMetalImpostorLegacyMaterialAlphaBatch(
                            params, is_particle_or_hud_particle, depth_only);
                    }
                    else
                    {
                        gPipeline.captureMetalImpostorFlatAlphaBatch(
                            params, is_particle_or_hud_particle, depth_only);
                    }
#endif

                    if (params.mFullbright)
                    {
                        // Turn off lighting if it hasn't already been so.
                        if (light_enabled || !initialized_lighting)
                        {
                            initialized_lighting = true;
                            target_shader = fullbright_shader;

                            light_enabled = false;
                        }
                    }
                    // Turn on lighting if it isn't already.
                    else if (!light_enabled || !initialized_lighting)
                    {
                        initialized_lighting = true;
                        target_shader = simple_shader;
                        light_enabled = true;
                    }

                    if (LLPipeline::sRenderingHUDs)
                    {
                        target_shader = fullbright_shader;
                    }
                    else if (mat)
                    {
                        U32 mask = params.mShaderMask;

                        llassert(mask < LLMaterial::SHADER_COUNT);
                        target_shader = gDeferredMaterialProgram[mask].selectVariant();
                    }
                    else if (!params.mFullbright)
                    {
                        target_shader = simple_shader;
                    }
                    else
                    {
                        target_shader = fullbright_shader;
                    }

                    if (params.mAvatar != nullptr)
                    {
                        llassert(target_shader->mRiggedVariant || !target_shader->isComplete());
                        target_shader = target_shader->riggedVariant();
                    }

                    if (current_shader != target_shader)
                    {// If we need shaders, and we're not ALREADY using the proper shader, then bind it
                    // (this way we won't rebind shaders unnecessarily).
                        gPipeline.bindDeferredShaderFast(*target_shader);
                        deferred_bound_shader = target_shader;

                        if (params.mFullbright)
                        { // make sure the bind the exposure map for fullbright shaders so they can cancel out exposure
                            S32 channel = target_shader->enableTexture(LLShaderMgr::EXPOSURE_MAP);
                            if (channel > -1)
                            {
                                gGL.getTextureSlot(channel)->bind(&gPipeline.mExposureMap);
                            }
                        }
                    }

                    LLVector4 spec_color(1, 1, 1, 1);
                    F32 env_intensity = 0.0f;
                    F32 brightness = 1.0f;

                    // We have a material.  Supply the appropriate data here.
                    if (mat)
                    {
                        spec_color = params.mSpecColor;
                        env_intensity = params.mEnvIntensity;
                        brightness = params.mFullbright ? 1.f : 0.f;
                    }

                    // target_shader, not current_shader: these belong to the program this draw
                    // was batched for. They are the same pointer here -- the branch above just
                    // bound it -- but saying so is what keeps them the same.
                    llassert(current_shader == target_shader);
                    target_shader->uniform4f(LLShaderMgr::SPECULAR_COLOR, spec_color.mV[VRED], spec_color.mV[VGREEN], spec_color.mV[VBLUE], spec_color.mV[VALPHA]);
                    target_shader->uniform1f(LLShaderMgr::ENVIRONMENT_INTENSITY, env_intensity);
                    target_shader->uniform1f(LLShaderMgr::EMISSIVE_BRIGHTNESS, brightness);
                }

                if (params.mAvatar && !uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, lastAvatarShader, skipLastSkin))
                {
                    continue;
                }

                bool tex_setup = TexSetup(&params, (mat != nullptr), target_shader,
                                          is_particle_or_hud_particle ? ALSamplers::AnisoClamp
                                                                      : ALSamplers::AnisoWrap);

                {
                    gGL.blendFunc((LLRender::eBlendFactor) params.mBlendFuncSrc, (LLRender::eBlendFactor) params.mBlendFuncDst, mAlphaSFactor, mAlphaDFactor);

                    bool reset_minimum_alpha = false;
                    if (!LLPipeline::sImpostorRender &&
                        params.mBlendFuncDst != LLRender::BF_SOURCE_ALPHA &&
                        params.mBlendFuncSrc != LLRender::BF_SOURCE_ALPHA)
                    { // this draw call has a custom blend function that may require rendering of "invisible" fragments
                        target_shader->setMinimumAlpha(0.f);
                        reset_minimum_alpha = true;
                    }

#if LL_DARWIN
                    if (LLPipeline::sRenderingHUDs)
                    {
                        const bool pbr_hud_alpha = target_shader == pbr_shader;
                        captureMetalHUDBatch(
                            params, true, false,
                            !pbr_hud_alpha, pbr_hud_alpha,
                            reset_minimum_alpha ? 0.f : MINIMUM_ALPHA,
                            is_particle_or_hud_particle);
                    }
#endif

                    params.mVertexBuffer->setBuffer();
                    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
                    stop_glerror();

                    if (reset_minimum_alpha)
                    {
                        target_shader->setMinimumAlpha(MINIMUM_ALPHA);
                    }
                }

                // If this alpha mesh has glow, then draw it a second time to add the destination-alpha (=glow).  Interleaving these state-changing calls is expensive, but glow must be drawn Z-sorted with alpha.
                if (getType() != LLDrawPool::POOL_ALPHA_PRE_WATER &&
                    params.mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_EMISSIVE))
                {
                    if (params.mAvatar != nullptr)
                    {
                        if (params.mGLTFMaterial.isNull())
                        {
                            rigged_emissives.push_back(&params);
                        }
                        else
                        {
                            pbr_rigged_emissives.push_back(&params);
                        }
                    }
                    else
                    {
                        if (params.mGLTFMaterial.isNull())
                        {
                            emissives.push_back(&params);
                        }
                        else
                        {
                            pbr_emissives.push_back(&params);
                        }
                    }
                }

                popTextureMatrix(tex_setup);
            }

            // render emissive faces into alpha channel for bloom effects
            //
            // Skipped in an impostor bake and a cutout frame. These batches write ONLY alpha
            // (their blend mode leaves colour untouched), and in both that channel is
            // coverage -- glow accumulated into it would read back as extra opacity. Nothing
            // is lost in a bake: the coverage stamp overwrote this glow before the impostor
            // was ever sampled, so it has never reached the screen. It also saves the whole
            // excursion, including the shader rebind that follows it.
            if (!depth_only && !LLPipeline::alphaWritesCoverage())
            {
                gPipeline.enableLightsDynamic();

                // install glow-accumulating blend mode
                // don't touch color, add to alpha (glow)
                gGL.blendFunc(LLRender::BF_ZERO, LLRender::BF_ONE, LLRender::BF_ONE, LLRender::BF_ONE);

                bool rebind = false;
                LLGLSLShader* lastShader = current_shader;

                // The emissive programs declare the indexed-texture array (tex0..tex20) as
                // plain sampler2D, and that range covers the units the alpha shader's shadow
                // maps sit on -- 8..13 in practice. No explicit release is needed for the
                // excursion: LLGLSLShader::bind() drops the compare-sampler units when it
                // binds any program that declares no shadow samplers, emissives included.
                // The restore below still has to come back through the deferred path.

                if (!emissives.empty())
                {
                    light_enabled = true;
                    renderEmissives(emissives, is_particle_or_hud_particle);
                    rebind = true;
                }

                if (!pbr_emissives.empty())
                {
                    light_enabled = true;
                    renderPbrEmissives(
                        pbr_emissives, is_particle_or_hud_particle);
                    rebind = true;
                }

                if (!rigged_emissives.empty())
                {
                    light_enabled = true;
                    renderRiggedEmissives(
                        rigged_emissives, is_particle_or_hud_particle);
                    rebind = true;
                }

                if (!pbr_rigged_emissives.empty())
                {
                    light_enabled = true;
                    renderRiggedPbrEmissives(
                        pbr_rigged_emissives,
                        is_particle_or_hud_particle);
                    rebind = true;
                }

                // restore our alpha blend mode
                gGL.blendFunc(mColorSFactor, mColorDFactor, mAlphaSFactor, mAlphaDFactor);

                if (lastShader && rebind)
                {
                    // Restore through the deferred path when that is how this shader was
                    // bound, so the shadow maps released above come back. A plain bind()
                    // would leave them unbound and silently drop shadowing on every alpha
                    // draw after the first emissive batch.
                    if (lastShader == deferred_bound_shader)
                    {
                        gPipeline.bindDeferredShaderFast(*lastShader);
                    }
                    else
                    {
                        lastShader->bind();
                    }
                }
            }
        }
    }

    gGL.setSceneBlendType(LLRender::BT_ALPHA);

    // Release the shadow maps at the pass boundary. LLGLSLShader::bind() would do this on
    // the next non-declaring program switch anyway; doing it here keeps the maps from
    // riding along to whatever runs between passes.
    //
    // Deliberately NOT unbindDeferredShader: that unbinds by a single shader's channel
    // layout and asserts each unit still holds the type that shader expects. By the time the
    // pass ends the bound shader may be a different one -- an emissive variant, or a restored
    // earlier shader -- so the units reflect somebody else's layout and the check trips.
    // unbindShadowMaps tracks units directly and needs no such assumption.
    gPipeline.unbindShadowMaps();

    LLVertexBuffer::unbind();

    if (!light_enabled)
    {
        gPipeline.enableLightsDynamic();
    }
}
