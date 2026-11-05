/**
 * @file lldrawpoolwlsky.cpp
 * @brief LLDrawPoolWLSky class implementation
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright © 2026, Rye <rye@alchemyviewer.org>
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

#include "lldrawpoolwlsky.h"

#include "llerror.h"
#include "llface.h"
#include "llimage.h"
#include "llrender.h"
#include "llenvironment.h"

#include "llviewerregion.h"
#include "llviewercamera.h"
#include "pipeline.h"
#include "llsky.h"
#include "llvowlsky.h"
#include "llsettingsvo.h"
#include "llviewercontrol.h"
#include "llappviewer.h" // gFrameIntervalSeconds

#include <optional>

extern bool gCubeSnapshot;

static float sStarTime;

LLDrawPoolWLSky::LLDrawPoolWLSky(void) :
    LLDrawPool(POOL_WL_SKY)
{
}

LLDrawPoolWLSky::~LLDrawPoolWLSky()
{
}

LLViewerTexture *LLDrawPoolWLSky::getDebugTexture()
{
    return NULL;
}

void LLDrawPoolWLSky::beginDeferredPass(S32 pass)
{
    (void)pass;
}

void LLDrawPoolWLSky::endDeferredPass(S32 pass)
{
    (void)pass;
}

extern LLPointer<LLImageGL> gEXRImage;

static bool use_hdri_sky()
{
    static LLCachedControl<F32> hdri_split(gSavedSettings, "RenderHDRISplitScreen", 1.f);
    static LLCachedControl<bool> irradiance_only(gSavedSettings, "RenderHDRIIrradianceOnly", false);

    return gCubeSnapshot && (!irradiance_only || !gPipeline.mReflectionMapManager.isRadiancePass()) ? gEXRImage.notNull() : // always use HDRI for reflection probes when available
        gEXRImage.notNull() ? hdri_split > 0.f : // fallback to EEP sky when split screen is zero
        false; // no HDRI available, always use EEP sky

}

// LLSettingsVOSky::applySpecial owns this exact loose-uniform policy. Keep the
// native soften transfer in lockstep with the value the authoritative GLSL
// sky and soften programs see for the main world view.
static F32 windlight_sky_hdr_scale(const LLSettingsSky::ptr_t& sky)
{
    static LLCachedControl<bool> should_auto_adjust(
        gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
    static LLCachedControl<F32> auto_adjust_hdr_scale(
        gSavedSettings, "RenderSkyAutoAdjustHDRScale", 2.f);
    if (sky->getReflectionProbeAmbiance() != 0.f)
    {
        return sqrtf(sky->getGamma()) * 2.f;
    }
    if (sky->canAutoAdjust() && should_auto_adjust)
    {
        return auto_adjust_hdr_scale;
    }
    return 1.f;
}

#if LL_DARWIN
static LLMatrix4a metal_sky_camera_modelview(
    const LLVector3& cam_pos_local)
{
    LLMatrix4a modelview = LLViewerCamera::getCurrent().getModelview();
    modelview.setMul(
        LLMatrix4a::translation(
            cam_pos_local.mV[VX], cam_pos_local.mV[VY],
            cam_pos_local.mV[VZ]),
        modelview);
    return modelview;
}

static LLMatrix4a metal_sky_modelview_projection(
    const LLVector3& cam_pos_local, F32 cam_height_local,
    bool dome_transform)
{
    LLMatrix4a modelview = metal_sky_camera_modelview(cam_pos_local);
    if (dome_transform)
    {
        // Mirror renderDome's row-vector transform order exactly without
        // touching gGL's matrix stack: basis permutation, uniform dome
        // scale, then the local-height offset.
        modelview.setMul(
            LLMatrix4a::rotation(
                120.f * DEG_TO_RAD,
                LLVector4a(
                    1.f / F_SQRT3, 1.f / F_SQRT3,
                    1.f / F_SQRT3)),
            modelview);
        modelview.setMul(
            LLMatrix4a::scaling(0.333f, 0.333f, 0.333f),
            modelview);
        modelview.setMul(
            LLMatrix4a::translation(0.f, -cam_height_local, 0.f),
            modelview);
    }

    LLMatrix4a modelview_projection;
    modelview_projection.setMul(
        modelview, LLViewerCamera::getCurrent().getProjection());
    return modelview_projection;
}

static LLMatrix4a metal_star_modelview_projection(
    const LLVector3& cam_pos_local, F32 rotation_radians)
{
    LLMatrix4a modelview = metal_sky_camera_modelview(cam_pos_local);
    modelview.setMul(
        LLMatrix4a::rotation(
            rotation_radians, LLVector4a(0.f, 0.f, 1.f)),
        modelview);
    LLMatrix4a modelview_projection;
    modelview_projection.setMul(
        modelview, LLViewerCamera::getCurrent().getProjection());
    return modelview_projection;
}

void LLDrawPoolWLSky::captureMetalReflectionProbe()
{
    if (!gCubeSnapshot || !gSky.mVOSkyp || !gSky.mVOWLSkyp ||
        !gPipeline.canUseWindLightShaders() ||
        !gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SKY))
    {
        return;
    }

    // The retained pool normally refreshes this owner immediately before
    // drawing. Direct native enumeration happens earlier, so it owns the
    // same geometry refresh without binding a GL program or target.
    gSky.mVOSkyp->updateGeometry(gSky.mVOSkyp->mDrawable);

    const LLVector3& origin = LLViewerCamera::getInstance()->getOrigin();
    const F32 cam_height_local = LLEnvironment::instance().getCamHeight();
    const LLMatrix4a dome_mvp = metal_sky_modelview_projection(
        origin, cam_height_local, true);

    static LLCachedControl<F32> hdri_exposure(
        gSavedSettings, "RenderHDRIExposure", 0.0f);
    static LLCachedControl<F32> hdri_rotation(
        gSavedSettings, "RenderHDRIRotation", 0.f);
    if (use_hdri_sky())
    {
        LLMatrix3 hdri_rotation_matrix;
        hdri_rotation_matrix.setRot(
            0.f, static_cast<F32>(hdri_rotation) * DEG_TO_RAD, 0.f);
        gPipeline.beginMetalWorldHDRISky(
            gEXRImage.get(), powf(2.f, hdri_exposure),
            hdri_rotation_matrix, cam_height_local);
        gSky.mVOWLSkyp->captureMetalDome(dome_mvp);
        gPipeline.endMetalWorldHDRISky();
        return;
    }

    const LLSettingsSky::ptr_t psky =
        LLEnvironment::instance().getCurrentSky();
    if (!psky)
    {
        return;
    }

    LLViewerTexture* rainbow_tex = gSky.mVOSkyp->getRainbowTex();
    LLViewerTexture* halo_tex = gSky.mVOSkyp->getHaloTex();
    F32 moisture_level = static_cast<F32>(psky->getSkyMoistureLevel());
    const F32 droplet_radius =
        static_cast<F32>(psky->getSkyDropletRadius());
    F32 ice_level = static_cast<F32>(psky->getSkyIceLevel());
    if (!psky->getIsSunUp() && !psky->getIsMoonUp())
    {
        moisture_level = 0.f;
        ice_level = 0.f;
    }

    gPipeline.beginMetalWorldWindLightSky(
        rainbow_tex, halo_tex, cam_height_local,
        moisture_level, droplet_radius, ice_level,
        psky->getSunMoonGlowFactor(), psky->getDensityMultiplier(),
        windlight_sky_hdr_scale(psky));
    gSky.mVOWLSkyp->captureMetalDome(dome_mvp);
    gPipeline.endMetalWorldWindLightSky();

    const LLMatrix4a body_mvp = metal_sky_modelview_projection(
        origin, cam_height_local, false);
    gPipeline.beginMetalWorldWindLightHeavenlyBodies();
    const bool can_render_bodies =
        gPipeline.shadersLoaded() && gPipeline.canUseWindLightShaders();
    const bool draw_sun = gPipeline.mHeroProbeManager.isMirrorPass();
    LLFace* face = gSky.mVOSkyp->mFace[LLVOSky::FACE_SUN];
    if (can_render_bodies && draw_sun &&
        gSky.mVOSkyp->getSun().getDraw() && face &&
        face->getGeomCount())
    {
        LLViewerTexture* texture_a =
            face->getTexture(LLRender::DIFFUSE_MAP);
        LLViewerTexture* texture_b =
            face->getTexture(LLRender::ALTERNATE_DIFFUSE_MAP);
        if (texture_a || texture_b)
        {
            gPipeline.captureMetalWorldWindLightHeavenlyBody(
                face, texture_a, texture_b,
                static_cast<F32>(psky->getBlendFactor()), 0.f, false,
                body_mvp);
        }
    }

    face = gSky.mVOSkyp->mFace[LLVOSky::FACE_MOON];
    if (can_render_bodies && gSky.mVOSkyp->getMoon().getDraw() &&
        face && face->getTexture(LLRender::DIFFUSE_MAP) &&
        face->getGeomCount())
    {
        LLViewerTexture* texture_a =
            face->getTexture(LLRender::DIFFUSE_MAP);
        LLViewerTexture* texture_b =
            face->getTexture(LLRender::ALTERNATE_DIFFUSE_MAP);
        if (texture_a || texture_b)
        {
            gPipeline.captureMetalWorldWindLightHeavenlyBody(
                face, texture_a, texture_b,
                static_cast<F32>(psky->getMoonBrightness()),
                psky->getMoonDirection().mV[VZ], true, body_mvp);
        }
    }
    gPipeline.endMetalWorldWindLightHeavenlyBodies();

    // Probe irradiance deliberately excludes clouds; the retained pool and
    // this direct producer both include them only in the radiance face.
    if (!gPipeline.mReflectionMapManager.isRadiancePass() ||
        !gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_CLOUDS) ||
        !gSky.mVOSkyp->getCloudNoiseTex())
    {
        return;
    }

    LLPointer<LLViewerTexture> cloud_noise =
        gSky.mVOSkyp->getCloudNoiseTex();
    LLPointer<LLViewerTexture> cloud_noise_next =
        gSky.mVOSkyp->getCloudNoiseTexNext();
    F32 cloud_variance = static_cast<F32>(psky->getCloudVariance());
    F32 blend_factor = static_cast<F32>(psky->getBlendFactor());
    if (psky->getCloudScrollRate().isExactlyZero())
    {
        blend_factor = 0.f;
    }
    if (cloud_noise &&
        (!cloud_noise_next || cloud_noise == cloud_noise_next))
    {
        cloud_noise_next = cloud_noise;
        blend_factor = 0.f;
    }
    else if (cloud_noise_next && !cloud_noise)
    {
        cloud_noise = cloud_noise_next;
        blend_factor = 0.f;
    }

    gPipeline.beginMetalWorldWindLightClouds(
        cloud_noise, cloud_noise_next, cam_height_local,
        cloud_variance, blend_factor, psky->getSunMoonGlowFactor(),
        psky->getDensityMultiplier());
    gSky.mVOWLSkyp->captureMetalDome(dome_mvp);
    gPipeline.endMetalWorldWindLightClouds();
}
#endif

void LLDrawPoolWLSky::renderSkyHazeDeferred(const LLVector3& camPosLocal, F32 camHeightLocal) const
{
#if LL_DARWIN
    if (!gSky.mVOSkyp || !gSky.mVOWLSkyp || gCubeSnapshot ||
        !gPipeline.canUseWindLightShaders() ||
        !gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SKY))
    {
        return;
    }

    const LLSettingsSky::ptr_t psky =
        LLEnvironment::instance().getCurrentSky();
    if (!psky)
    {
        return;
    }

    const LLMatrix4a dome_mvp = metal_sky_modelview_projection(
        camPosLocal, camHeightLocal, true);
    static LLCachedControl<F32> hdri_exposure(
        gSavedSettings, "RenderHDRIExposure", 0.0f);
    static LLCachedControl<F32> hdri_rotation(
        gSavedSettings, "RenderHDRIRotation", 0.f);
    static LLCachedControl<F32> hdri_split(
        gSavedSettings, "RenderHDRISplitScreen", 1.f);
    LLMatrix3 hdri_rotation_matrix;
    hdri_rotation_matrix.setRot(
        0.f, static_cast<F32>(hdri_rotation) * DEG_TO_RAD, 0.f);

    if (use_hdri_sky())
    {
        if (hdri_split >= 1.f)
        {
            gPipeline.beginMetalWorldHDRISky(
                gEXRImage.get(), powf(2.f, hdri_exposure),
                hdri_rotation_matrix, camHeightLocal);
            gSky.mVOWLSkyp->captureMetalDome(dome_mvp);
            gPipeline.endMetalWorldHDRISky();
        }
        else if (hdri_split > 0.f)
        {
            LLViewerTexture* rainbow_tex = gSky.mVOSkyp->getRainbowTex();
            LLViewerTexture* halo_tex = gSky.mVOSkyp->getHaloTex();
            F32 moisture_level = static_cast<F32>(psky->getSkyMoistureLevel());
            const F32 droplet_radius =
                static_cast<F32>(psky->getSkyDropletRadius());
            F32 ice_level = static_cast<F32>(psky->getSkyIceLevel());
            if (!psky->getIsSunUp() && !psky->getIsMoonUp())
            {
                moisture_level = 0.f;
                ice_level = 0.f;
            }
            gPipeline.beginMetalWorldSplitSky(
                gEXRImage.get(), powf(2.f, hdri_exposure),
                hdri_rotation_matrix, hdri_split,
                rainbow_tex, halo_tex, camHeightLocal,
                moisture_level, droplet_radius, ice_level,
                psky->getSunMoonGlowFactor(),
                psky->getDensityMultiplier(),
                windlight_sky_hdr_scale(psky));
            gSky.mVOWLSkyp->captureMetalDome(dome_mvp);
            gPipeline.endMetalWorldSplitSky();
        }
        return;
    }

    LLViewerTexture* rainbow_tex = gSky.mVOSkyp->getRainbowTex();
    LLViewerTexture* halo_tex = gSky.mVOSkyp->getHaloTex();
    F32 moisture_level = static_cast<F32>(psky->getSkyMoistureLevel());
    const F32 droplet_radius =
        static_cast<F32>(psky->getSkyDropletRadius());
    F32 ice_level = static_cast<F32>(psky->getSkyIceLevel());
    if (!psky->getIsSunUp() && !psky->getIsMoonUp())
    {
        moisture_level = 0.f;
        ice_level = 0.f;
    }
    gPipeline.beginMetalWorldWindLightSky(
        rainbow_tex, halo_tex, camHeightLocal,
        moisture_level, droplet_radius, ice_level,
        psky->getSunMoonGlowFactor(), psky->getDensityMultiplier(),
        windlight_sky_hdr_scale(psky));
    gSky.mVOWLSkyp->captureMetalDome(dome_mvp);
    gPipeline.endMetalWorldWindLightSky();
#else
    (void)camPosLocal;
    (void)camHeightLocal;
#endif
}

void LLDrawPoolWLSky::renderStarsDeferred(const LLVector3& camPosLocal) const
{
#if LL_DARWIN
    if (!gSky.mVOSkyp || !gSky.mVOWLSkyp || use_hdri_sky() ||
        !gSky.mVOWLSkyp->hasStars())
    {
        return;
    }

    const LLSettingsSky::ptr_t psky =
        LLEnvironment::instance().getCurrentSky();
    if (!psky)
    {
        return;
    }
    const F32 star_alpha = psky->getStarBrightness() / 500.0f;

    if (star_alpha < 0.001f)
    {
        LL_DEBUGS("SKY") << "star_brightness below threshold." << LL_ENDL;
        return;
    }

    // Subtle rotation so fixed patterns drift over long time scales.
    const F32 star_rotation =
        static_cast<F32>(fmod(gFrameTimeSeconds * 0.01, 360.0));

    // The stars' frame is the agent's turned by the rotation above, and the fragment shader works in it
    // (vary_world_dir), so agent directions are turned back by the rotation's transpose.
    LLMatrix4a to_stars = LLMatrix4a::rotation(star_rotation * DEG_TO_RAD, LLVector4a(0.f, 0.f, 1.f));
    to_stars.transpose();
    const auto in_stars = [&to_stars](const LLVector3& v)
    {
        LLVector4a out;
        to_stars.rotate(LLVector4a(v.mV[VX], v.mV[VY], v.mV[VZ]), out);
        return LLVector3(out.getF32ptr());
    };

    // Moon glare inputs, in the stars' frame so the glare stays on the moon however far the stars have turned.
    // Wrapped time for scintillation noise. 86400s is long enough that a
    // typical session never hits the wrap boundary (where the non-periodic
    // noise would show a one-frame jump), and short enough that 32-bit float
    // precision stays good for the scaled time term in the shader.
    sStarTime = (F32)fmod(LLFrameTimer::getUptimeSeconds() * 0.5, 86400.0);

    // The moon draws after the stars and fades low in the sky, so the stars leave out what its quad and texture cover
    // in the native star compositor. Its corners lie around the camera in agent space.
    LLFace* moon_face = gSky.mVOSkyp->mFace[LLVOSky::FACE_MOON];
    LLViewerTexture* moon_tex = moon_face ? moon_face->getTexture(LLRender::DIFFUSE_MAP) : nullptr;
    const LLHeavenBody& moon = gSky.mVOSkyp->getMoon();
    const bool mask_moon = moon.getDraw() && moon_tex && moon_face->getGeomCount();
    LLVector3 moon_quad_center;
    LLVector3 moon_quad_u;
    LLVector3 moon_quad_v;
    if (mask_moon)
    {
        // Corners 0..3 are (-right, +up), (-right, -up), (+right, +up), (+right, -up) (LLVOSky::updateHeavenlyBodyGeometry).
        const LLVector3 centre = (moon.corner(0) + moon.corner(3)) * 0.5f;
        const F32 dist = centre.length();
        LLVector3 half_u = (moon.corner(2) - moon.corner(0)) * (0.5f / dist);
        LLVector3 half_v = (moon.corner(0) - moon.corner(1)) * (0.5f / dist);
        half_u /= half_u.lengthSquared();
        half_v /= half_v.lengthSquared();

        moon_quad_center = in_stars(centre / dist);
        moon_quad_u = in_stars(half_u);
        moon_quad_v = in_stars(half_v);
    }

    gPipeline.beginMetalWorldWindLightStars(
        star_alpha, sStarTime, in_stars(psky->getMoonDirection()),
        psky->getIsMoonUp() ? (F32)psky->getMoonBrightness() : 0.f,
        moon_quad_center, moon_quad_u, moon_quad_v,
        moon_tex, mask_moon);
    gSky.mVOWLSkyp->captureMetalStars(
        metal_star_modelview_projection(
            camPosLocal, star_rotation * DEG_TO_RAD));
    gPipeline.endMetalWorldWindLightStars();
#else
    (void)camPosLocal;
#endif
}

void LLDrawPoolWLSky::renderMeteorsDeferred(const LLVector3& camPosLocal) const
{
#if LL_DARWIN
    if (!gSky.mVOSkyp || use_hdri_sky() || !gSky.mVOWLSkyp) return;

    static LLCachedControl<bool> meteors_enabled(gSavedSettings, "RenderMeteorsEnabled", true);
    if (!meteors_enabled) return;

    // Gate on the same star-brightness threshold as stars — meteors are a
    // night-sky phenomenon and should disappear when daytime suppresses stars.
    // Also freezes the CPU-side meteor state so we don't accumulate work off-screen.
    F32 star_alpha = LLEnvironment::instance().getCurrentSky()->getStarBrightness() / 500.0f;
    if (star_alpha < 0.001f) return;

    // Tick and upload state once per frame before drawing. gFrameIntervalSeconds
    // is zero when paused (dt <= 0 bails out of the tick), so the meteor list
    // naturally freezes with the rest of the world.
    F32 dt = (F32)gFrameIntervalSeconds.value();
    if (dt > 0.1f) dt = 0.1f;   // clamp after long frames so a new meteor doesn't snap to end-of-life
    gSky.mVOWLSkyp->tickMeteors(dt);
    gSky.mVOWLSkyp->updateMeteorGeometry();

    gPipeline.beginMetalWorldWindLightMeteors(1.5f);
    gSky.mVOWLSkyp->captureMetalMeteors(
        metal_sky_modelview_projection(camPosLocal, 0.f, false));
    gPipeline.endMetalWorldWindLightMeteors();
#else
    (void)camPosLocal;
#endif
}

void LLDrawPoolWLSky::renderAuroraDeferred(const LLVector3& camPosLocal, F32 camHeightLocal) const
{
#if LL_DARWIN
    if (!gSky.mVOSkyp || use_hdri_sky() || !gSky.mVOWLSkyp) return;

    static LLCachedControl<F32> aurora_intensity(gSavedSettings, "RenderAuroraIntensity", 0.6f);
    if (aurora_intensity <= 0.0f) return;

    // Aurora is a night phenomenon — gate on star brightness for consistency
    // with the rest of the night sky (stars, meteors).
    LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky();
    const F32 star_alpha = psky->getStarBrightness() / 500.0f;
    if (star_alpha < 0.001f) return;

    // Suppress aurora when the moon is up and bright (scattered moonlight
    // would wash out the faint emission anyway).
    const F32 moon_wash = psky->getIsMoonUp() ? (F32)psky->getMoonBrightness() : 0.0f;
    const F32 intensity = aurora_intensity * llmax(0.0f, 1.0f - moon_wash * 0.85f);
    if (intensity <= 0.001f) return;

    // Accumulated in F64: an F32 near the wrap holds the time only to 8 ms, so adding a frame's
    // dt to it rounds each step to whole multiples of that, and the aurora moves unevenly.
    static F64 s_aurora_time = 0.0;
    F32 dt = (F32)gFrameIntervalSeconds.value();
    if (dt > 0.1f) dt = 0.1f;
    s_aurora_time = fmod(s_aurora_time + dt, 86400.0);

    // Re-use the WL sky dome mesh — a ready-made hemisphere. Aurora shader
    // discards the zenith cap and below-horizon region.
    gPipeline.beginMetalWorldWindLightAurora(
        intensity, (F32)s_aurora_time, camHeightLocal);
    gSky.mVOWLSkyp->captureMetalDome(
        metal_sky_modelview_projection(
            camPosLocal, camHeightLocal, true));
    gPipeline.endMetalWorldWindLightAurora();
#else
    (void)camPosLocal;
    (void)camHeightLocal;
#endif
}

void LLDrawPoolWLSky::renderSkyCloudsDeferred(
    const LLVector3& camPosLocal, F32 camHeightLocal) const
{
#if LL_DARWIN
    if (use_hdri_sky() || !gSky.mVOSkyp || !gSky.mVOWLSkyp ||
        !gPipeline.canUseWindLightShaders() ||
        !gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_CLOUDS))
    {
        return;
    }

    const LLSettingsSky::ptr_t psky =
        LLEnvironment::instance().getCurrentSky();
    LLPointer<LLViewerTexture> cloud_noise =
        gSky.mVOSkyp->getCloudNoiseTex();
    LLPointer<LLViewerTexture> cloud_noise_next =
        gSky.mVOSkyp->getCloudNoiseTexNext();
    if (!psky || !cloud_noise)
    {
        return;
    }

    const F32 cloud_variance =
        static_cast<F32>(psky->getCloudVariance());
    F32 blend_factor = static_cast<F32>(psky->getBlendFactor());
    if (psky->getCloudScrollRate().isExactlyZero())
    {
        blend_factor = 0.f;
    }
    if (!cloud_noise_next || cloud_noise == cloud_noise_next)
    {
        cloud_noise_next = cloud_noise;
        blend_factor = 0.f;
    }

    gPipeline.beginMetalWorldWindLightClouds(
        cloud_noise, cloud_noise_next, camHeightLocal,
        cloud_variance, blend_factor,
        psky->getSunMoonGlowFactor(), psky->getDensityMultiplier());
    gSky.mVOWLSkyp->captureMetalDome(
        metal_sky_modelview_projection(
            camPosLocal, camHeightLocal, true));
    gPipeline.endMetalWorldWindLightClouds();
#else
    (void)camPosLocal;
    (void)camHeightLocal;
#endif
}

void LLDrawPoolWLSky::renderHeavenlyBodies()
{
#if LL_DARWIN
    if (!gSky.mVOSkyp || use_hdri_sky()) return;
    const LLSettingsSky::ptr_t psky =
        LLEnvironment::instance().getCurrentSky();
    if (!psky) return;

    const F32 blend_factor = static_cast<F32>(psky->getBlendFactor());
    const bool can_render_bodies =
        gPipeline.shadersLoaded() && gPipeline.canUseWindLightShaders();
    const LLVector3& origin = LLViewerCamera::getInstance()->getOrigin();
    const LLMatrix4a body_mvp = metal_sky_modelview_projection(
        origin, LLEnvironment::instance().getCamHeight(), false);
    gPipeline.beginMetalWorldWindLightHeavenlyBodies();

    // Environment probes leave the sun disc out. The punctual sun term owns the sun's light and
    // gates it with the sun's shadow; captured into a probe as well, the disc came back through
    // every environment lobe -- counted twice on a lit surface, and a sun highlight on one in
    // shade. Mirrors keep it: they are a picture of the scene, not lighting for it.
    const bool draw_sun = !gCubeSnapshot || gPipeline.mHeroProbeManager.isMirrorPass();

    LLFace* face = gSky.mVOSkyp->mFace[LLVOSky::FACE_SUN];
    if (can_render_bodies && draw_sun &&
        gSky.mVOSkyp->getSun().getDraw() && face && face->getGeomCount())
    {
        LLViewerTexture* tex_a =
            face->getTexture(LLRender::DIFFUSE_MAP);
        LLViewerTexture* tex_b =
            face->getTexture(LLRender::ALTERNATE_DIFFUSE_MAP);
        if (tex_a || tex_b)
        {
            gPipeline.captureMetalWorldWindLightHeavenlyBody(
                face, tex_a, tex_b, blend_factor, 0.f, false, body_mvp);
        }
    }

    face = gSky.mVOSkyp->mFace[LLVOSky::FACE_MOON];

    if (can_render_bodies && gSky.mVOSkyp->getMoon().getDraw() &&
        face && face->getTexture(LLRender::DIFFUSE_MAP) &&
        face->getGeomCount())
    {
        LLViewerTexture* tex_a = face->getTexture(LLRender::DIFFUSE_MAP);
        LLViewerTexture* tex_b = face->getTexture(LLRender::ALTERNATE_DIFFUSE_MAP);
        if (tex_a || tex_b)
        {
            const F32 moon_brightness =
                static_cast<F32>(psky->getMoonBrightness());
            gPipeline.captureMetalWorldWindLightHeavenlyBody(
                face, tex_a, tex_b, moon_brightness,
                psky->getMoonDirection().mV[VZ], true, body_mvp);
        }
    }

    gPipeline.endMetalWorldWindLightHeavenlyBodies();
#endif
}

void LLDrawPoolWLSky::renderDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    (void)pass;
    if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SKY) || gSky.mVOSkyp.isNull())
    {
        return;
    }

    // TODO: remove gSky.mVOSkyp and fold sun/moon into LLVOWLSky
    gSky.mVOSkyp->updateGeometry(gSky.mVOSkyp->mDrawable);

    // Reflection and hero faces are enumerated directly into their native
    // transaction before retained draw-pool traversal reaches this point.
    if (gCubeSnapshot || !gPipeline.canUseWindLightShaders())
    {
        return;
    }

    const F32 camHeightLocal = LLEnvironment::instance().getCamHeight();
    const LLVector3& origin = LLViewerCamera::getInstance()->getOrigin();

    renderSkyHazeDeferred(origin, camHeightLocal);
    renderStarsDeferred(origin);
    renderHeavenlyBodies();
    renderMeteorsDeferred(origin);
    renderAuroraDeferred(origin, camHeightLocal);

    if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_CLOUDS))
    {
        renderSkyCloudsDeferred(origin, camHeightLocal);
    }
}



LLViewerTexture* LLDrawPoolWLSky::getTexture()
{
    return NULL;
}

void LLDrawPoolWLSky::resetDrawOrders()
{
}
