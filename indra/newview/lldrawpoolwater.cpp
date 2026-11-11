/**
 * @file lldrawpoolwater.cpp
 * @brief LLDrawPoolWater class implementation
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
#include "lldrawpoolwater.h"

#include "llviewercontrol.h"

#include "llagent.h"
#include "llviewertexturelist.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llenvironment.h"
#include "llsettingswater.h"
#include "alfarplane.h"
#include "alwaterfield.h"
#if LL_DARWIN
#include "../almetal/almetalworld.h"
#include "llimagegl.h"
#endif

extern bool gCubeSnapshot;

// Whether the native water of the view drawn now refracts the retained scene
// snapshot (ALRenderResourcePlan::refractsWater). Not in an ordinary probe's
// face, which clips the scene at the water (LLPipeline::isWaterClip), so its
// snapshot holds nothing beneath the surface.
static bool water_refracts()
{
    const bool refracts = ALRenderResourcePlan::refractsWater(gPipeline.renderPack(), LLPipeline::sRenderTransparentWater);
    llassert(refracts == (LLPipeline::sRenderTransparentWater && !LLPipeline::isWaterClip()));
    return refracts;
}

LLDrawPoolWater::LLDrawPoolWater() : LLDrawPool(POOL_WATER)
{
}

void LLDrawPoolWater::setNormalMaps(const LLUUID& normalMapId, const LLUUID& nextNormalMapId)
{
    LLSettingsWater::ptr_t pwater = LLEnvironment::instance().getCurrentWater();
    mWaterNormp[0] = LLViewerTextureManager::getFetchedTexture(!normalMapId.isNull() ? normalMapId : pwater->GetDefaultWaterNormalAssetId());
    mWaterNormp[1] = LLViewerTextureManager::getFetchedTexture(!nextNormalMapId.isNull() ? nextNormalMapId : (!normalMapId.isNull() ? normalMapId : pwater->GetDefaultWaterNormalAssetId()));
    mWaterNormp[0]->addTextureStats(1024.f*1024.f);
    mWaterNormp[1]->addTextureStats(1024.f*1024.f);
}

void LLDrawPoolWater::prerender()
{
    mShaderLevel = LLViewerShaderMgr::instance()->getShaderLevel(LLViewerShaderMgr::SHADER_WATER);
}

void LLDrawPoolWater::captureMetalReflectionProbe()
{
#if LL_DARWIN
    if (!gCubeSnapshot || !gPipeline.mWaterSurface.exists() ||
        !gPipeline.mWaterSurface.drawnFrom(
            *LLViewerCamera::getInstance()))
    {
        return;
    }
    if (!captureMetalWaterSurface(true))
    {
        // Water is visible in this face, so an unavailable native input must
        // fail the whole native face instead of publishing a waterless cube.
        gPipeline.captureMetalWorldWaterSurface(
            ALMetalWorldWaterSurface{}, true);
    }
#endif
}

bool LLDrawPoolWater::captureMetalWaterSurface(bool direct_probe)
{
#if LL_DARWIN
    LLEnvironment& environment = LLEnvironment::instance();
    LLSettingsWater::ptr_t pwater = environment.getCurrentWater();
    if (!pwater || mShaderLevel <= 2)
    {
        return false;
    }

    LLViewerTexture* tex_a = mWaterNormp[0];
    LLViewerTexture* tex_b = mWaterNormp[1];
    F32 blend_factor = static_cast<F32>(pwater->getBlendFactor());
    LLViewerTexture* bound_a = nullptr;
    LLViewerTexture* bound_b = nullptr;
    if (tex_a && (!tex_b || tex_a == tex_b))
    {
        bound_a = tex_a;
        blend_factor = 0.f;
    }
    else if (tex_b && !tex_a)
    {
        bound_a = tex_b;
        blend_factor = 0.f;
    }
    else if (tex_b != tex_a)
    {
        bound_a = tex_a;
        bound_b = tex_b;
    }
    if (!bound_a)
    {
        return false;
    }

    LLImageGL* normal_a = bound_a->getGLTexture();
    LLImageGL* normal_b = bound_b ? bound_b->getGLTexture() : normal_a;
    const ALMetalTexture* metal_a = normal_a
        ? normal_a->getMetalTexture() : nullptr;
    const ALMetalTexture* metal_b = normal_b
        ? normal_b->getMetalTexture() : nullptr;
    if (!metal_a || !metal_b)
    {
        return false;
    }

    LLViewerCamera& camera = *LLViewerCamera::getInstance();
    const bool underwater = camera.cameraUnderWater();
    const LLVector3 eye = camera.getOrigin();
    const LLVector3d eye_global = gAgent.getPosGlobalFromAgent(eye);
    const ALWaterField::WavePhases phases = ALWaterField::wavePhases(
        eye_global.mdV[VX], eye_global.mdV[VY],
        LLFrameTimer::getUptimeSeconds(), pwater->getWave1Dir(),
        pwater->getWave2Dir());
    const ALWaterField::LatticePlaces lattice =
        ALWaterField::latticePlaces(
            eye_global.mdV[VX], eye_global.mdV[VY]);

    const ALWaterSurface& surface = gPipeline.mWaterSurface;
    LLVector2 rim_lo;
    LLVector2 rim_hi;
    const bool have_rim = surface.rim(eye, rim_lo, rim_hi);
    const ALFarPlane::WaterFar water_far = ALFarPlane::waterFar(
        camera.getProjectionFar(), have_rim, rim_lo, rim_hi);
    const bool waves = surface.wavesDrawn() && !gCubeSnapshot &&
        surface.patchesInView(camera);
    const bool main_view = gPipeline.mRT == &gPipeline.mMainRT;
    static LLCachedControl<bool> occlusion_cull(
        gSavedSettings, "AlchemyRenderWaterOcclusionCull", true);
    static LLCachedControl<F32> glint_roughness(
        gSavedSettings, "AlchemyRenderWaterGlintRoughness", 0.35f);
    LLVector3 light_dir = environment.getLightDirection();
    light_dir.normalize();

    const bool refracts = water_refracts();
    const bool excluded = underwater &&
        gPipeline.beginRenderMap(LLRenderPass::PASS_INVISIBLE) !=
            gPipeline.endRenderMap(LLRenderPass::PASS_INVISIBLE);
    const bool opaque_beside =
        LLPipeline::sRenderTransparentWater && !refracts;

    ALMetalWorldWaterSurface metal_water;
    LLMatrix4a normal_matrix;
    if (!normal_matrix.setNormalMatrix(camera.getModelview()) ||
        !surface.fillMetalWaterHaze(metal_water.field, camera, true))
    {
        return false;
    }
    metal_water.normal_maps[0] = metal_a;
    metal_water.normal_maps[1] = metal_b;
    static LLCachedControl<bool> normal_maps_mipped(
        gSavedSettings, "RenderWaterMipNormal", true);
    metal_water.normal_maps_mipped = normal_maps_mipped();
    std::memcpy(metal_water.modelview,
                camera.getModelview().getF32ptr(),
                sizeof(metal_water.modelview));
    std::memcpy(metal_water.normal_matrix,
                normal_matrix.getF32ptr(),
                sizeof(metal_water.normal_matrix));

    // Match bindDeferredShader's transpose=true env_mat upload for the
    // probe-disabled legacy sky cube path without reading a GL matrix.
    const F32* modelview = camera.getModelview().getF32ptr();
    metal_water.environment_matrix[0][0] = modelview[0];
    metal_water.environment_matrix[0][1] = modelview[4];
    metal_water.environment_matrix[0][2] = modelview[8];
    metal_water.environment_matrix[1][0] = modelview[1];
    metal_water.environment_matrix[1][1] = modelview[5];
    metal_water.environment_matrix[1][2] = modelview[9];
    metal_water.environment_matrix[2][0] = modelview[2];
    metal_water.environment_matrix[2][1] = modelview[6];
    metal_water.environment_matrix[2][2] = modelview[10];
    std::memcpy(metal_water.eye_position, eye.mV, sizeof(F32) * 3);
    std::memcpy(metal_water.big_phase, phases.mBig.mV,
                sizeof(F32) * 2);
    std::memcpy(metal_water.little_phase, phases.mLittle.mV,
                sizeof(metal_water.little_phase));
    std::memcpy(metal_water.sweep_phase, phases.mSweep.mV,
                sizeof(F32) * 2);
    metal_water.water_far[0] = water_far.mWaveClamp;
    metal_water.water_far[1] = water_far.mEdgeFade;
    const LLVector4 means = surface.mapMeans(bound_a, bound_b);
    std::memcpy(metal_water.map_mean, means.mV,
                sizeof(metal_water.map_mean));
    std::memcpy(metal_water.lattice_fraction, lattice.mFrac.mV,
                sizeof(metal_water.lattice_fraction));
    std::memcpy(metal_water.lattice_base, lattice.mBase,
                sizeof(metal_water.lattice_base));
    const LLVector3 normal_scale = pwater->getNormalScale();
    std::memcpy(metal_water.normal_scale_glint, normal_scale.mV,
                sizeof(F32) * 3);
    metal_water.normal_scale_glint[3] = llclamp(
        static_cast<F32>(glint_roughness), 0.05f, 1.f);
    metal_water.optics[0] = pwater->getFresnelScale();
    metal_water.optics[1] = pwater->getFresnelOffset();
    metal_water.optics[2] =
        fmaxf(0.f, pwater->getBlurMultiplier()) * 2.f;
    metal_water.rim[0] = rim_lo.mV[VX];
    metal_water.rim[1] = rim_lo.mV[VY];
    metal_water.rim[2] = rim_hi.mV[VX];
    metal_water.rim[3] = rim_hi.mV[VY];
    std::memcpy(metal_water.light_direction, light_dir.mV,
                sizeof(F32) * 3);
    metal_water.parameters[0] = blend_factor;
    metal_water.parameters[1] = underwater
        ? pwater->getScaleBelow() : pwater->getScaleAbove();
    metal_water.parameters[2] = 1.f;
    metal_water.parameters[3] = static_cast<F32>(
        gPipeline.shaderInputs().mGlobal.mWaterHextile);
    metal_water.underwater = underwater;
    metal_water.refracts = refracts;
    metal_water.has_excluded_surface = excluded;
    metal_water.uses_shadows = !underwater &&
        LLViewerShaderMgr::instance()->getShaderLevel(
            LLViewerShaderMgr::SHADER_DEFERRED) > 1 &&
        gPipeline.shaderInputs().mGlobal.mShadowDetail > 0;
    const bool metal_occlusion = main_view && occlusion_cull && LLRender::sCompute;
    if (waves && !surface.fillMetalWaterWaves(
            metal_water, camera, bound_a, bound_b,
            metal_occlusion))
    {
        return false;
    }

    gPipeline.captureMetalWorldWaterSurface(
        metal_water, direct_probe);
    return true;
#else
    (void)direct_probe;
    return false;
#endif
}

S32 LLDrawPoolWater::getNumPostDeferredPasses()
{
    // Nor in a cutout frame's main pass. The water is drawn from the field, not culled, so the cutout's cull cannot
    // leave it out: drawn, it would cover a wading subject and lay its glint's glow into the coverage. With no pass
    // here, the exclusion and the haze over the surface are not run either.
    if (!gPipeline.mWaterSurface.exists() || LLPipeline::cullingToCutoutSubject())
    {
        return 0;
    }

    // Not where the view misses the whole sea, as looking up, nor from so high above it that it has faded into the sky:
    // nothing would be drawn, and the screen's copy and the depth pyramid would be made for nothing.
    return gPipeline.mWaterSurface.drawnFrom(*LLViewerCamera::getInstance()) ? 1 : 0;
}

void LLDrawPoolWater::beginPostDeferredPass(S32 pass)
{
    (void)pass;
}

void LLDrawPoolWater::endPostDeferredPass(S32 pass)
{
    (void)pass;
}

void LLDrawPoolWater::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    (void)pass;
#if LL_DARWIN
    // Water is a direct native producer. Its retained color/depth snapshot,
    // depth pyramid, wave tessellation, exclusion semantics and surface draw
    // are all encoded by ALMetalWorldGBufferRenderer at this ordered marker.
    // Missing inputs fail the native transaction closed; no GL surface draw
    // or transitional GL scratch target is allowed to mask the failure.
    if (!captureMetalWaterSurface(false))
    {
        gPipeline.captureMetalWorldWaterSurface(
            ALMetalWorldWaterSurface{}, false);
    }
#endif
}
