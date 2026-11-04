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
#include "llglslshader.h"
#include "llgl.h"

#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewercamera.h"
#include "pipeline.h"
#include "llsky.h"
#include "llvowlsky.h"
#include "llsettingsvo.h"
#include "llviewercontrol.h"
#include "llappviewer.h" // gFrameIntervalSeconds

#include "llglstates.h"

#include <optional>

extern bool gCubeSnapshot;

static LLGLSLShader* cloud_shader = NULL;
static LLGLSLShader* sky_shader   = NULL;
static LLGLSLShader* sun_shader   = NULL;
static LLGLSLShader* moon_shader  = NULL;

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
    sky_shader = &gDeferredWLSkyProgram;
    cloud_shader = &gDeferredWLCloudProgram;

    sun_shader = &gDeferredWLSunProgram;

    moon_shader = &gDeferredWLMoonProgram;
}

void LLDrawPoolWLSky::endDeferredPass(S32 pass)
{
    sky_shader   = nullptr;
    cloud_shader = nullptr;
    sun_shader   = nullptr;
    moon_shader  = nullptr;
}

void LLDrawPoolWLSky::renderDome(const LLVector3& camPosLocal, F32 camHeightLocal, LLGLSLShader * shader) const
{
    llassert_always(NULL != shader);

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.pushMatrix();

    //chop off translation
    {
        gGL.translatef(camPosLocal.mV[0], camPosLocal.mV[1], camPosLocal.mV[2]);
    }


    // the windlight sky dome works most conveniently in a coordinate system
    // where Y is up, so permute our basis vectors accordingly.
    gGL.rotatef(120.f, 1.f / F_SQRT3, 1.f / F_SQRT3, 1.f / F_SQRT3);

    gGL.scalef(0.333f, 0.333f, 0.333f);

    gGL.translatef(0.f,-camHeightLocal, 0.f);

    // Draw WL Sky
    shader->uniform3f(LLShaderMgr::WL_CAMPOSLOCAL, 0.f, camHeightLocal, 0.f);

    gSky.mVOWLSkyp->drawDome();

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.popMatrix();
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

// Whether any of the water lies in front of the depth the world left, the sky drawing last: the frustum can hold the
// sea (LLPipeline::skyBehindWanted) while land hides all of it. The field, counted by the pipeline's query,
// writing nothing: flat, and with waves raised by A as well, which counts a crest over a wall the flat field misses.
// The flat copy stays for a camera between the plane and A above it, as on a deck in waves, which sees the raised one
// from below or not at all. False when there is no program to draw them with, or while the occlusion targets are
// held, as a shader profile counting samples holds them (LLGLSLShader::initProfile).
static bool query_water_in_view()
{
    LLGLSLShader& field = gWaterFieldProgram;
    if (!field.isComplete() || !gGL.queries().canBegin(ALQueryKind::Occlusion))
    {
        return false;
    }
    field.bind();
    gPipeline.mWaterSurface.bind(field);
    {
        LLGLDepthTest depth(GL_TRUE, GL_FALSE);
        LLGLSColorMask no_color(false, false);
        LLGLDisable no_cull(GL_CULL_FACE);
        ALQueryScope query(gGL.queries(), ALQueryKind::Occlusion);
        gPipeline.mSkyBehindQuery = query.ticket();
        field.uniform1f(LLShaderMgr::WATER_RAISE, 0.f);
        gPipeline.mWaterSurface.draw(field, false);
        const F32 raise = gPipeline.mWaterSurface.maxDisplacement();
        if (raise > 0.f)
        {
            field.uniform1f(LLShaderMgr::WATER_RAISE, raise);
            gPipeline.mWaterSurface.draw(field, false);
        }
    }
    field.unbind();
    gPipeline.mSkyBehindQueryFrame = gFrameCount;
    return gPipeline.mSkyBehindQuery.isValid();
}

void LLDrawPoolWLSky::renderSkyHazeDeferred(const LLVector3& camPosLocal, F32 camHeightLocal) const
{
    if (!gSky.mVOSkyp)
    {
        return;
    }

    LLVector3 const & origin = LLViewerCamera::getInstance()->getOrigin();

    if (gPipeline.canUseWindLightShaders() && gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SKY))
    {
        const bool hdri_sky = use_hdri_sky();
#if LL_DARWIN
        bool native_hdri_capture = false;
        bool native_split_capture = false;
        bool native_windlight_capture = false;
#endif
        static LLCachedControl<F32> hdri_exposure(
            gSavedSettings, "RenderHDRIExposure", 0.0f);
        static LLCachedControl<F32> hdri_rotation(
            gSavedSettings, "RenderHDRIRotation", 0.f);
        static LLCachedControl<F32> hdri_split(
            gSavedSettings, "RenderHDRISplitScreen", 1.f);
        LLMatrix3 hdri_rotation_matrix;
        hdri_rotation_matrix.setRot(
            0.f, static_cast<F32>(hdri_rotation) * DEG_TO_RAD, 0.f);
        // The sky again, behind everything, for the haze pass to converge far water on (hazeF.glsl): the main view's
        // only, and only when water is in it; an HDRI sky is colour already shaded, which the haze cannot take the
        // same way. Whether any water shows is asked first, before the sky binds its textures.
        // From the pool until the haze has read it, acquired before the sky binds its textures.
        LLRenderTarget* const behind = !hdri_sky && gPipeline.skyBehindWanted() ? gPipeline.acquireSkyBehind() : nullptr;
        const bool            sky_behind = behind != nullptr;
        if (!gCubeSnapshot && gPipeline.mRT == &gPipeline.mMainRT)
        {
            LL_PROFILE_PLOT("sky behind", (int64_t)sky_behind);
        }
        const bool queried = sky_behind && query_water_in_view();

        if (hdri_sky)
        {
            sky_shader = &gEnvironmentMapProgram;
            sky_shader->bind();
            S32 idx = sky_shader->enableTexture(LLShaderMgr::ENVIRONMENT_MAP);
            if (idx > -1)
            {
                gGL.getTextureSlot(idx)->bindSampled(gEXRImage, ALSamplers::TrilinearClamp);
            }

            sky_shader->uniform1f(LLShaderMgr::SKY_HDR_SCALE, powf(2.f, hdri_exposure));
            sky_shader->uniformMatrix3fv(
                LLShaderMgr::DEFERRED_ENV_MAT, 1, GL_FALSE,
                (F32*)hdri_rotation_matrix.mMatrix);
            sky_shader->uniform1f(LLShaderMgr::HDRI_SPLIT_SCREEN, gCubeSnapshot ? 1.f : hdri_split);
#if LL_DARWIN
            // The full-HDRI case owns every visible sky layer: the pool's
            // later star/body/effect/cloud draws all opt out through
            // use_hdri_sky(). A partial split is captured below once the
            // complementary WindLight inputs have been resolved.
            if (!gCubeSnapshot && hdri_split >= 1.f)
            {
                gPipeline.beginMetalWorldHDRISky(
                    gEXRImage.get(), powf(2.f, hdri_exposure),
                    hdri_rotation_matrix,
                    camHeightLocal);
                native_hdri_capture = true;
            }
#endif
        }
        else
        {
            sky_shader->bind();
        }

        sky_shader->uniform1i(LLShaderMgr::CUBE_SNAPSHOT, gCubeSnapshot ? 1 : 0);

        LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky();

        LLViewerTexture* rainbow_tex = gSky.mVOSkyp->getRainbowTex();
        LLViewerTexture* halo_tex  = gSky.mVOSkyp->getHaloTex();

        sky_shader->bindTexture(LLShaderMgr::RAINBOW_MAP, rainbow_tex, ALSamplers::AnisoWrap);
        sky_shader->bindTexture(LLShaderMgr::HALO_MAP, halo_tex, ALSamplers::AnisoWrap);

        F32 moisture_level  = (float)psky->getSkyMoistureLevel();
        F32 droplet_radius  = (float)psky->getSkyDropletRadius();
        F32 ice_level       = (float)psky->getSkyIceLevel();

        // hobble halos and rainbows when there's no light source to generate them
        if (!psky->getIsSunUp() && !psky->getIsMoonUp())
        {
            moisture_level = 0.0f;
            ice_level      = 0.0f;
        }

        sky_shader->uniform1f(LLShaderMgr::MOISTURE_LEVEL, moisture_level);
        sky_shader->uniform1f(LLShaderMgr::DROPLET_RADIUS, droplet_radius);
        sky_shader->uniform1f(LLShaderMgr::ICE_LEVEL, ice_level);

        sky_shader->uniform1f(LLShaderMgr::SUN_MOON_GLOW_FACTOR, psky->getSunMoonGlowFactor());

        sky_shader->uniform1i(LLShaderMgr::SUN_UP_FACTOR, psky->getIsSunUp() ? 1 : 0);

#if LL_DARWIN
        if (!hdri_sky)
        {
            gPipeline.beginMetalWorldWindLightSky(
                rainbow_tex, halo_tex, camHeightLocal,
                moisture_level, droplet_radius, ice_level,
                psky->getSunMoonGlowFactor(),
                psky->getDensityMultiplier(),
                windlight_sky_hdr_scale(psky));
            native_windlight_capture = true;
        }
        else if (!gCubeSnapshot && hdri_split > 0.f && hdri_split < 1.f)
        {
            gPipeline.beginMetalWorldSplitSky(
                gEXRImage.get(), powf(2.f, hdri_exposure),
                hdri_rotation_matrix, hdri_split,
                rainbow_tex, halo_tex, camHeightLocal,
                moisture_level, droplet_radius, ice_level,
                psky->getSunMoonGlowFactor(),
                psky->getDensityMultiplier(),
                windlight_sky_hdr_scale(psky));
            native_split_capture = true;
        }
#endif

        /// Render the skydome
        renderDome(origin, camHeightLocal, sky_shader);
#if LL_DARWIN
        if (native_hdri_capture)
        {
            gPipeline.endMetalWorldHDRISky();
        }
        if (native_split_capture)
        {
            gPipeline.endMetalWorldSplitSky();
        }
        if (native_windlight_capture)
        {
            gPipeline.endMetalWorldWindLightSky();
        }
#endif

        // Again into the sky behind, with the same program and uniforms and no depth, so it covers what the world hides.
        // The GPU clears and draws it only if the query saw water, with no readback: a frame late, the far water would
        // show WindLight's seam for a frame whenever the camera turned onto the sea. The target is held either way; with
        // no water in view no pixel is marked as water, so the haze never reads what it holds.
        if (sky_behind)
        {
            LL_PROFILE_GPU_ZONE("sky behind");
            std::optional<ALConditionalRender> if_water;
            if (queried)
            {
                if_water.emplace(gGL.queries(), gPipeline.mSkyBehindQuery);
            }
            behind->bindTarget();
            behind->clear();
            // skyF writes its colour to frag_data[3] with the emissive buffer, and to frag_data[0] without it.
            behind->setDrawOutput(gPipeline.shaderInputs().mGlobal.mEmissive ? 3 : 0);
            {
                LLGLDepthTest no_depth(GL_FALSE, GL_FALSE);
                LLGLDisable no_blend(GL_BLEND);
                renderDome(origin, camHeightLocal, sky_shader);
            }
            behind->flush();
        }

        sky_shader->unbind();
    }
}

void LLDrawPoolWLSky::renderStarsDeferred(const LLVector3& camPosLocal) const
{
    if (!gSky.mVOSkyp || use_hdri_sky())
    {
        return;
    }

    LLGLEnable blend(GL_BLEND);

    // Pre-multiplied additive blend: the shader multiplies the star's RGB by
    // its shape/alpha before output, so the framebuffer operation is a simple
    // screen-space add rather than an alpha-blend.
    gGL.setSceneBlendType(LLRender::BT_ADD);

    F32 star_alpha = LLEnvironment::instance().getCurrentSky()->getStarBrightness() / 500.0f;

    // If start_brightness is not set, exit
    if(star_alpha < 0.001f)
    {
        LL_DEBUGS("SKY") << "star_brightness below threshold." << LL_ENDL;
        return;
    }
#if LL_DARWIN
    if (!gSky.mVOWLSkyp->hasStars())
    {
        return;
    }
#endif

    gDeferredStarProgram.bind();

    gGL.pushMatrix();
    gGL.translatef(camPosLocal.mV[0], camPosLocal.mV[1], camPosLocal.mV[2]);
    // Subtle rotation so fixed patterns drift over long time scales.
    const F32 star_rotation = (F32)fmod(gFrameTimeSeconds * 0.01, 360.0);
    gGL.rotatef(star_rotation, 0.f, 0.f, 1.f);

    gDeferredStarProgram.uniform1f(LLShaderMgr::CUSTOM_ALPHA, star_alpha);

    // Screen resolution for GPU-side pixel-sized billboarding.
    LLRenderTarget* deferred_target = &gPipeline.mRT->deferredScreen;
    gDeferredStarProgram.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
                                   (GLfloat)deferred_target->getWidth(),
                                   (GLfloat)deferred_target->getHeight());

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
    LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky();
    gDeferredStarProgram.uniform3fv(LLShaderMgr::DEFERRED_MOON_DIR, 1, in_stars(psky->getMoonDirection()).mV);
    gDeferredStarProgram.uniform1f(LLShaderMgr::MOON_BRIGHTNESS,
                                   psky->getIsMoonUp() ? (F32)psky->getMoonBrightness() : 0.0f);

    // Wrapped time for scintillation noise. 86400s is long enough that a
    // typical session never hits the wrap boundary (where the non-periodic
    // noise would show a one-frame jump), and short enough that 32-bit float
    // precision stays good for the scaled time term in the shader.
    sStarTime = (F32)fmod(LLFrameTimer::getUptimeSeconds() * 0.5, 86400.0);

    gDeferredStarProgram.uniform1f(LLShaderMgr::WATER_TIME, sStarTime);

    // The moon draws after the stars and fades low in the sky, so the stars leave out what its quad and texture cover
    // (starsF.glsl). Its corners lie around the camera in agent space.
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
        gDeferredStarProgram.uniform3fv(LLShaderMgr::MOON_QUAD_CENTER, 1, moon_quad_center.mV);
        gDeferredStarProgram.uniform3fv(LLShaderMgr::MOON_QUAD_U, 1, moon_quad_u.mV);
        gDeferredStarProgram.uniform3fv(LLShaderMgr::MOON_QUAD_V, 1, moon_quad_v.mV);
        gDeferredStarProgram.bindTexture(LLShaderMgr::DIFFUSE_MAP, moon_tex, ALSamplers::AnisoClamp);
    }
    else
    {
        gDeferredStarProgram.uniform3f(LLShaderMgr::MOON_QUAD_CENTER, 0.f, 0.f, 0.f);
    }

#if LL_DARWIN
    gPipeline.beginMetalWorldWindLightStars(
        star_alpha, sStarTime, in_stars(psky->getMoonDirection()),
        psky->getIsMoonUp() ? (F32)psky->getMoonBrightness() : 0.f,
        moon_quad_center, moon_quad_u, moon_quad_v,
        moon_tex, mask_moon);
#endif
    gSky.mVOWLSkyp->drawStars();
#if LL_DARWIN
    gPipeline.endMetalWorldWindLightStars();
#endif

    if (mask_moon)
    {
        gDeferredStarProgram.disableTexture(LLShaderMgr::DIFFUSE_MAP);
    }

    gDeferredStarProgram.unbind();

    gGL.popMatrix();
}

void LLDrawPoolWLSky::renderMeteorsDeferred(const LLVector3& camPosLocal) const
{
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

    LLGLEnable blend(GL_BLEND);
    gGL.setSceneBlendType(LLRender::BT_ADD);

    gDeferredMeteorProgram.bind();

    gGL.pushMatrix();
    gGL.translatef(camPosLocal.mV[0], camPosLocal.mV[1], camPosLocal.mV[2]);

    LLRenderTarget* deferred_target = &gPipeline.mRT->deferredScreen;
    gDeferredMeteorProgram.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
                                     (GLfloat)deferred_target->getWidth(),
                                     (GLfloat)deferred_target->getHeight());
    gDeferredMeteorProgram.uniform1f(LLShaderMgr::METEOR_WIDTH_PIXELS, 1.5f);

#if LL_DARWIN
    gPipeline.beginMetalWorldWindLightMeteors(1.5f);
#endif
    gSky.mVOWLSkyp->drawMeteors();
#if LL_DARWIN
    gPipeline.endMetalWorldWindLightMeteors();
#endif

    gDeferredMeteorProgram.unbind();
    gGL.popMatrix();
}

void LLDrawPoolWLSky::renderAuroraDeferred(const LLVector3& camPosLocal, F32 camHeightLocal) const
{
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

    LLGLEnable blend(GL_BLEND);
    gGL.setSceneBlendType(LLRender::BT_ADD);

    gDeferredAuroraProgram.bind();

    // Accumulated in F64: an F32 near the wrap holds the time only to 8 ms, so adding a frame's
    // dt to it rounds each step to whole multiples of that, and the aurora moves unevenly.
    static F64 s_aurora_time = 0.0;
    F32 dt = (F32)gFrameIntervalSeconds.value();
    if (dt > 0.1f) dt = 0.1f;
    s_aurora_time = fmod(s_aurora_time + dt, 86400.0);

    gDeferredAuroraProgram.uniform1f(LLShaderMgr::AURORA_INTENSITY, intensity);
    gDeferredAuroraProgram.uniform1f(LLShaderMgr::AURORA_TIME, (F32)s_aurora_time);

    // Re-use the WL sky dome mesh — a ready-made hemisphere. Aurora shader
    // discards the zenith cap and below-horizon region.
#if LL_DARWIN
    gPipeline.beginMetalWorldWindLightAurora(
        intensity, (F32)s_aurora_time, camHeightLocal);
#endif
    renderDome(camPosLocal, camHeightLocal, &gDeferredAuroraProgram);
#if LL_DARWIN
    gPipeline.endMetalWorldWindLightAurora();
#endif

    gDeferredAuroraProgram.unbind();
}

void LLDrawPoolWLSky::renderSkyCloudsDeferred(const LLVector3& camPosLocal, F32 camHeightLocal, LLGLSLShader* cloudshader) const
{
    if (use_hdri_sky())
    {
        return;
    }

    if (gPipeline.canUseWindLightShaders() && gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_CLOUDS) && gSky.mVOSkyp && gSky.mVOSkyp->getCloudNoiseTex())
    {
        LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky();

        LLGLEnable blend(GL_BLEND);
        gGL.setSceneBlendType(LLRender::BT_ALPHA);

        cloudshader->bind();

        LLPointer<LLViewerTexture> cloud_noise      = gSky.mVOSkyp->getCloudNoiseTex();
        LLPointer<LLViewerTexture> cloud_noise_next = gSky.mVOSkyp->getCloudNoiseTexNext();

        gGL.getTextureSlot(0)->unbind();
        gGL.getTextureSlot(1)->unbind();

        F32 cloud_variance = psky ? (F32)psky->getCloudVariance() : 0.0f;
        F32 blend_factor   = psky ? (F32)psky->getBlendFactor() : 0.0f;

        if (psky->getCloudScrollRate().isExactlyZero())
        {
            blend_factor = 0.f;
        }

        // if we even have sun disc textures to work with...
        if (cloud_noise || cloud_noise_next)
        {
            if (cloud_noise && (!cloud_noise_next || (cloud_noise == cloud_noise_next)))
            {
                // Bind current and next sun textures
                cloudshader->bindTexture(LLShaderMgr::CLOUD_NOISE_MAP, cloud_noise, ALSamplers::AnisoWrap);
                cloud_noise_next = cloud_noise;
                blend_factor = 0;
            }
            else if (cloud_noise_next && !cloud_noise)
            {
                cloudshader->bindTexture(LLShaderMgr::CLOUD_NOISE_MAP, cloud_noise_next, ALSamplers::AnisoWrap);
                cloud_noise = cloud_noise_next;
                blend_factor = 0;
            }
            else if (cloud_noise_next != cloud_noise)
            {
                cloudshader->bindTexture(LLShaderMgr::CLOUD_NOISE_MAP, cloud_noise, ALSamplers::AnisoWrap);
                cloudshader->bindTexture(LLShaderMgr::CLOUD_NOISE_MAP_NEXT, cloud_noise_next, ALSamplers::AnisoWrap);
            }
        }

        cloudshader->uniform1f(LLShaderMgr::BLEND_FACTOR, blend_factor);
        cloudshader->uniform1f(LLShaderMgr::CLOUD_VARIANCE, cloud_variance);
        cloudshader->uniform1f(LLShaderMgr::SUN_MOON_GLOW_FACTOR, psky->getSunMoonGlowFactor());

        /// Render the skydome
#if LL_DARWIN
        gPipeline.beginMetalWorldWindLightClouds(
            cloud_noise, cloud_noise_next, camHeightLocal,
            cloud_variance, blend_factor,
            psky->getSunMoonGlowFactor(), psky->getDensityMultiplier());
#endif
        renderDome(camPosLocal, camHeightLocal, cloudshader);
#if LL_DARWIN
        gPipeline.endMetalWorldWindLightClouds();
#endif

        cloudshader->unbind();

        gGL.getTextureSlot(0)->unbind();
        gGL.getTextureSlot(1)->unbind();
    }
}

void LLDrawPoolWLSky::renderHeavenlyBodies()
{
    if (!gSky.mVOSkyp || use_hdri_sky()) return;

#if LL_DARWIN
    gPipeline.beginMetalWorldWindLightHeavenlyBodies();
#endif

    LLGLEnable blend(GL_BLEND);
    gGL.setSceneBlendType(LLRender::BT_ALPHA);

    LLVector3 const & origin = LLViewerCamera::getInstance()->getOrigin();
    gGL.pushMatrix();
    gGL.translatef(origin.mV[0], origin.mV[1], origin.mV[2]);

    LLFace * face = gSky.mVOSkyp->mFace[LLVOSky::FACE_SUN];

    F32 blend_factor = (F32)LLEnvironment::instance().getCurrentSky()->getBlendFactor();
    bool can_use_vertex_shaders = gPipeline.shadersLoaded();
    bool can_use_windlight_shaders = gPipeline.canUseWindLightShaders();

    // Environment probes leave the sun disc out. The punctual sun term owns the sun's light and
    // gates it with the sun's shadow; captured into a probe as well, the disc came back through
    // every environment lobe -- counted twice on a lit surface, and a sun highlight on one in
    // shade. Mirrors keep it: they are a picture of the scene, not lighting for it.
    const bool draw_sun = !gCubeSnapshot || gPipeline.mHeroProbeManager.isMirrorPass();

    if (draw_sun && gSky.mVOSkyp->getSun().getDraw() && face && face->getGeomCount())
    {
        LLPointer<LLViewerTexture> tex_a = face->getTexture(LLRender::DIFFUSE_MAP);
        LLPointer<LLViewerTexture> tex_b = face->getTexture(LLRender::ALTERNATE_DIFFUSE_MAP);

        gGL.getTextureSlot(0)->unbind();
        gGL.getTextureSlot(1)->unbind();

        // if we even have sun disc textures to work with...
        if (tex_a || tex_b)
        {
            // if and only if we have a texture defined, render the sun disc
            if (can_use_vertex_shaders && can_use_windlight_shaders)
            {
                sun_shader->bind();

                if (tex_a && (!tex_b || (tex_a == tex_b)))
                {
                    // Bind current and next sun textures
                    sun_shader->bindTexture(LLShaderMgr::DIFFUSE_MAP, tex_a, ALSamplers::AnisoClamp);
                    blend_factor = 0;
                }
                else if (tex_b && !tex_a)
                {
                    sun_shader->bindTexture(LLShaderMgr::DIFFUSE_MAP, tex_b, ALSamplers::AnisoClamp);
                    blend_factor = 0;
                }
                else if (tex_b != tex_a)
                {
                    sun_shader->bindTexture(LLShaderMgr::DIFFUSE_MAP, tex_a, ALSamplers::AnisoClamp);
                    sun_shader->bindTexture(LLShaderMgr::ALTERNATE_DIFFUSE_MAP, tex_b, ALSamplers::AnisoClamp);
                }

                LLColor4 color(gSky.mVOSkyp->getSun().getInterpColor());

                sun_shader->uniform4fv(LLShaderMgr::DIFFUSE_COLOR, 1, color.mV);
                sun_shader->uniform1f(LLShaderMgr::BLEND_FACTOR, blend_factor);

#if LL_DARWIN
                gPipeline.captureMetalWorldWindLightHeavenlyBody(
                    face, tex_a, tex_b, blend_factor, 0.f, false);
#endif
                face->renderIndexed();

                gGL.getTextureSlot(0)->unbind();
                gGL.getTextureSlot(1)->unbind();

                sun_shader->unbind();
            }
        }
    }

    face = gSky.mVOSkyp->mFace[LLVOSky::FACE_MOON];

    if (gSky.mVOSkyp->getMoon().getDraw() && face && face->getTexture(LLRender::DIFFUSE_MAP) && face->getGeomCount() && moon_shader)
    {
        LLViewerTexture* tex_a = face->getTexture(LLRender::DIFFUSE_MAP);
        LLViewerTexture* tex_b = face->getTexture(LLRender::ALTERNATE_DIFFUSE_MAP);

        LLColor4 color(gSky.mVOSkyp->getMoon().getInterpColor());

        if (can_use_vertex_shaders && can_use_windlight_shaders && (tex_a || tex_b))
        {
            moon_shader->bind();

            if (tex_a && (!tex_b || (tex_a == tex_b)))
            {
                // Bind current and next sun textures
                moon_shader->bindTexture(LLShaderMgr::DIFFUSE_MAP, tex_a, ALSamplers::AnisoClamp);
                //blend_factor = 0;
            }
            else if (tex_b && !tex_a)
            {
                moon_shader->bindTexture(LLShaderMgr::DIFFUSE_MAP, tex_b, ALSamplers::AnisoClamp);
                //blend_factor = 0;
            }
            else if (tex_b != tex_a)
            {
                moon_shader->bindTexture(LLShaderMgr::DIFFUSE_MAP, tex_a, ALSamplers::AnisoClamp);
                //moon_shader->bindTexture(LLShaderMgr::ALTERNATE_DIFFUSE_MAP, tex_b, ALSamplers::AnisoClamp);
            }

            LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky();

            F32 moon_brightness = (float)psky->getMoonBrightness();

            moon_shader->uniform1f(LLShaderMgr::MOON_BRIGHTNESS, moon_brightness);
            moon_shader->uniform4fv(LLShaderMgr::DIFFUSE_COLOR, 1, color.mV);
            //moon_shader->uniform1f(LLShaderMgr::BLEND_FACTOR, blend_factor);
            moon_shader->uniform3fv(LLShaderMgr::DEFERRED_MOON_DIR, 1, psky->getMoonDirection().mV); // shader: moon_dir

#if LL_DARWIN
            gPipeline.captureMetalWorldWindLightHeavenlyBody(
                face, tex_a, tex_b, moon_brightness,
                psky->getMoonDirection().mV[VZ], true);
#endif
            face->renderIndexed();

            gGL.getTextureSlot(0)->unbind();
            gGL.getTextureSlot(1)->unbind();

            moon_shader->unbind();
        }
    }

#if LL_DARWIN
    gPipeline.endMetalWorldWindLightHeavenlyBodies();
#endif
    gGL.popMatrix();
}

void LLDrawPoolWLSky::renderDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_SKY) || gSky.mVOSkyp.isNull())
    {
        return;
    }

    // Opt out of the deferred pass's hoisted GL_FRAMEBUFFER_SRGB (renderGeomDeferred): the
    // sky writers store display-encoded values into the albedo attachment raw and the
    // lighting pass's decoded read round-trips them -- the pass-through convention. An
    // encode here would double-transform the non-emissive sky path. Inert for the
    // HAS_EMISSIVE path, which writes the sky to the float emissive attachment instead.
    LLGLDisable srgb(GL_FRAMEBUFFER_SRGB);

    // TODO: remove gSky.mVOSkyp and fold sun/moon into LLVOWLSky
    gSky.mVOSkyp->updateGeometry(gSky.mVOSkyp->mDrawable);

    const F32 camHeightLocal = LLEnvironment::instance().getCamHeight();

    LLVector3 const & origin = LLViewerCamera::getInstance()->getOrigin();

    if (gPipeline.canUseWindLightShaders())
    {
        // Every layer lies on the far plane and writes no depth. The pool draws after all the
        // opaque geometry, so the depth test passes only where the world drew nothing, and the
        // layers cover one another in the order they are drawn: the stars behind the sun and
        // moon, the meteors and aurora in front of them, the clouds in front of everything.
        LLGLDisable cull(GL_CULL_FACE);
        LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);

        renderSkyHazeDeferred(origin, camHeightLocal);
        if (!gCubeSnapshot)
        {
            renderStarsDeferred(origin);
        }

        renderHeavenlyBodies();

        if (!gCubeSnapshot)
        {
            renderMeteorsDeferred(origin);
            renderAuroraDeferred(origin, camHeightLocal);
        }

        // Reset blend type after drawing effects that need BT_ADD
        gGL.setSceneBlendType(LLRender::BT_ALPHA);

        if (!gCubeSnapshot || gPipeline.mReflectionMapManager.isRadiancePass()) // don't draw clouds in irradiance maps to avoid popping
        {
            renderSkyCloudsDeferred(origin, camHeightLocal, cloud_shader);
        }
    }
}



LLViewerTexture* LLDrawPoolWLSky::getTexture()
{
    return NULL;
}

void LLDrawPoolWLSky::resetDrawOrders()
{
}
