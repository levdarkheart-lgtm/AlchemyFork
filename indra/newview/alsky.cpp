/**
 * @file alsky.h
 * @brief The sky: the sun and the moon, the sky's textures, the stars and the meteors, and what the passes that draw
 *        it are given.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
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
#include "alsky.h"
#include "alskycube.h"
#include "alfarplane.h"
#include "llagent.h"
#include "llappviewer.h"
#include "llenvironment.h"
#include "llimage.h"
#include "llrender.h"
#include "llsettingssky.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewertexture.h"
#include "llvoavatarself.h"
#include "pipeline.h"
#include "workqueue.h"
#include <cstring>
#include <utility>

extern bool gCubeSnapshot;
extern LLPointer<LLImageGL> gEXRImage;

namespace
{
    // The bodies' disks, as fractions of ALSkyLayers::BODY_FACTOR's metre.
    constexpr F32 SUN_DISK_RADIUS  = 0.5f;
    constexpr F32 MOON_DISK_RADIUS = SUN_DISK_RADIUS * 0.9f;

    // How long the fog colour and the environment cube hold before a changed sky is taken again.
    constexpr F64 ENVIRONMENT_UPDATE_SECONDS = 0.25;

    // The global generator, for the meteors.
    F32 globalRandom() { return ll_frand(); }

    // The most meteors in flight: the user-facing RenderMeteorMaxCount setting is clamped to the composite's array.
    constexpr U32 kMeteorHardMax = ALSkyLayers::MAX_METEORS;

    // Pick a random unit vector perpendicular to `n`. Uses a deterministic
    // orthogonal basis built from `n` and a fixed "up" swap for robustness
    // at the poles, then rotates around `n` by a uniform angle.
    template <typename Rand> LLVector3 randomTangentTo(const LLVector3& n, Rand& draw)
    {
        const LLVector3 fallback = (fabsf(n.mV[2]) < 0.9f)
                                       ? LLVector3(0.f, 0.f, 1.f)
                                       : LLVector3(1.f, 0.f, 0.f);
        LLVector3 a = (n % fallback);   // cross
        a.normVec();
        LLVector3 b = (n % a);
        b.normVec();
        const F32 rot = F_TWO_PI * draw();
        return a * cosf(rot) + b * sinf(rot);
    }

    // A texture pair the sky blends between, as the sun's and the clouds' draws took one: the first, and the next
    // only while they differ and both are there, with how far the sky has blended toward it.
    struct TexturePair
    {
        LLViewerTexture* mFirst = nullptr;
        LLViewerTexture* mNext  = nullptr;
        F32              mBlend = 0.f;
    };

    bool resident(LLViewerTexture* texture)
    {
        return texture && texture->getGLTexture() && texture->getGLTexture()->getMetalTexture();
    }

    TexturePair texturePair(LLViewerTexture* first, LLViewerTexture* next, F32 blend)
    {
        if (!resident(first)) first = nullptr;
        if (!resident(next)) next = nullptr;
        if (first && (!next || first == next))
        {
            return { first, nullptr, 0.f };
        }
        if (next && !first)
        {
            return { next, nullptr, 0.f };
        }
        if (first && next)
        {
            return { first, next, blend };
        }
        return {};
    }

    // @a texture for @a id: null for none, fetched again where it is not the one held.
    void fetch(LLPointer<LLViewerFetchedTexture>& texture, const LLUUID& id)
    {
        if (id.isNull())
        {
            texture = nullptr;
        }
        else if (texture.isNull() || texture->getID() != id)
        {
            texture = LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT, true, LLGLTexture::BOOST_UI);
        }
    }
}

F32 LLHeavenBody::angularHalfSize() const
{
    // Corners 0..3 are (-right, +up), (-right, -up), (+right, +up), (+right, -up) (ALSkyLayers::bodyCorners).
    const F32 dist = ((mCorners[0] + mCorners[3]) * 0.5f).length();
    if (dist <= 0.f)
    {
        return 0.f;
    }
    const F32 half_width  = (mCorners[2] - mCorners[0]).length() * 0.5f;
    const F32 half_height = (mCorners[0] - mCorners[1]).length() * 0.5f;
    return 0.5f * (half_width + half_height) / dist;
}

ALSky::ALSky()
    : mSun(SUN_DISK_RADIUS)
    , mMoon(MOON_DISK_RADIUS)
    , mBumpSunDir(0.f, 0.f, 1.f)
{
}

void ALSky::takeSky()
{
    const LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky();
    if (!psky)
    {
        return;
    }

    // The bodies' quads about the camera, turned as the sky turns them: the stars' moon mask, the composite and the
    // lens flare's occlusion probe read them.
    mSun.setCorners(
        ALSkyLayers::bodyCorners(psky->getSunRotation(), HEAVENLY_BODY_DIST, mSunScale, mSun.getDiskRadius()));
    mMoon.setCorners(
        ALSkyLayers::bodyCorners(psky->getMoonRotation(), HEAVENLY_BODY_DIST, mMoonScale, mMoon.getDiskRadius()));

    fetchTextures();
}

void ALSky::update()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_ENVIRONMENT;

    LLEnvironment&             environment = LLEnvironment::instance();
    const LLSettingsSky::ptr_t psky        = environment.getCurrentSky();
    if (!psky)
    {
        return;
    }

    if (!mReady)
    {
        // The directions and the lighting worked once before anything is drawn.
        psky->update();

        mStarLifetime = std::make_shared<U8>(0);
        requestStars();
        // RenderStarCount is for tuning, so it takes effect without a restart, once its field lands.
        mStarCountConnection = gSavedSettings.getControl("RenderStarCount")->getSignal()->connect(
            [this](LLControlVariable*, const LLSD&, const LLSD&) { requestStars(); });

        mReady = true;
    }

    takeSky();
}

void ALSky::forceSkyUpdate()
{
    mEnvironmentDirty = true;
    mEnvironmentRetry = false;
}

void ALSky::setSunDirectionCFR(const LLVector3& sun_dir_cfr)
{
    // Push the sun "South" as it approaches directly overhead so that we can always see bump mapping
    // on the upward facing faces of cubes.
    // Same as dot product with the up direction + clamp.
    F32 sunDot = llmax(0.f, sun_dir_cfr.mV[2]);
    sunDot *= sunDot;

    // Create normalized vector that has the sunDir pushed south about an hour and change.
    LLVector3 adjustedDir = (sun_dir_cfr + LLVector3(0.f, -0.70711f, 0.70711f)) * 0.5f;

    // Blend between normal sun dir and adjusted sun dir based on how close we are
    // to having the sun overhead.
    mBumpSunDir = adjustedDir * sunDot + sun_dir_cfr * (1.0f - sunDot);
    mBumpSunDir.normalize();
}

void ALSky::setSunTextures(const LLUUID& sun_texture, const LLUUID& sun_texture_next)
{
    mSunTextureId = { sun_texture, sun_texture_next };
}

void ALSky::setMoonTextures(const LLUUID& moon_texture, const LLUUID& moon_texture_next)
{
    mMoonTextureId = { moon_texture, moon_texture_next };
}

void ALSky::setCloudNoiseTextures(const LLUUID& cloud_noise_texture, const LLUUID& cloud_noise_texture_next)
{
    mCloudNoiseTextureId = { cloud_noise_texture, cloud_noise_texture_next };
}

void ALSky::setBloomTextures(const LLUUID& bloom_texture, const LLUUID& bloom_texture_next)
{
    const LLUUID fallback = bloom_texture.isNull() ? LLSettingsSky::GetDefaultBloomTextureId() : bloom_texture;
    mBloomTextureId = { fallback, bloom_texture_next.isNull() ? fallback : bloom_texture_next };
}

void ALSky::fetchTextures()
{
    const LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky();
    // The latched input, so the bloom's textures come and go with the bloom the programs draw.
    const bool bloom = gPipeline.shaderInputs().mLocal.mSunBloom;
    for (size_t i = 0; i < 2; ++i)
    {
        fetch(mSunTexture[i], mSunTextureId[i]);
        fetch(mMoonTexture[i], mMoonTextureId[i]);
        fetch(mCloudNoiseTexture[i], mCloudNoiseTextureId[i]);
        fetch(mBloomTexture[i], bloom ? mBloomTextureId[i] : LLUUID::null);
    }
    // A sky need not name the dome's two maps, which every view that draws the dome samples: one it leaves out is the
    // default's.
    const LLUUID rainbow = psky->getRainbowTextureId();
    const LLUUID halo    = psky->getHaloTextureId();
    fetch(mRainbowMap, rainbow.isNull() ? LLSettingsSky::GetDefaultRainbowTextureId() : rainbow);
    fetch(mHaloMap, halo.isNull() ? LLSettingsSky::GetDefaultHaloTextureId() : halo);

    // The sun's, the moon's and the bloom's at full size, as the drawn bodies are always wanted.
    for (auto* textures : { &mSunTexture, &mMoonTexture, &mBloomTexture })
    {
        for (const LLPointer<LLViewerFetchedTexture>& texture : *textures)
        {
            if (texture)
            {
                texture->addTextureStats((F32)MAX_IMAGE_AREA);
            }
        }
    }
}

void ALSky::requestStars()
{
    // RenderStarCount scales every tier alike, so the mix of bright primaries, mid stars and faint dust stays the same
    // when the total is dialled down for performance.
    const LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky();
    if (!psky)
    {
        return;
    }
    const U32 request     = ++mStarRequest;
    const U32 count       = (U32)llmax(gSavedSettings.getS32("RenderStarCount"), 0);
    const F32 dome_radius = psky->getDomeRadius();

    // Tens of milliseconds at the default count, and more toward ALStarField::MAX_COUNT: off the main thread, from the
    // count and the radius alone.
    auto lay_out = [count, dome_radius]()
    {
        LL_PROFILE_ZONE_NAMED("sky stars lay out");
        StarField field;
        try
        {
            field.mStars  = ALStarField::generate(count, dome_radius);
            field.mLayout = ALStarField::layOut(field.mStars);
        }
        catch (const std::exception& e)
        {
            LL_WARNS("Sky") << "The star field could not be laid out, so no stars are drawn: " << e.what() << LL_ENDL;
            field = {};
        }
        return field;
    };

    const std::weak_ptr<U8> lifetime = mStarLifetime;
    LL::WorkQueue::ptr_t main_queue    = LL::WorkQueue::getInstance("mainloop");
    LL::WorkQueue::ptr_t general_queue = LL::WorkQueue::getInstance("General");
    if (!main_queue || !general_queue ||
        !main_queue->postTo(general_queue, std::move(lay_out),
                            [this, lifetime, request](StarField field) { if (lifetime.lock()) landStars(request, std::move(field)); }))
    {
        LL_WARNS("Sky") << "No queue to lay the star field out on, so no stars are drawn" << LL_ENDL;
    }
}

void ALSky::landStars(U32 request, StarField&& field)
{
    if (request != mStarRequest)
    {
        return;
    }
    mStars      = std::move(field.mStars);
    mStarLayout = std::move(field.mLayout);
    // The buffer is made from the new field when next drawn, and tried again where the last could not be made.
    mStarsVerts       = nullptr;
    mStarsBuildFailed = false;
}

void ALSky::beginView(bool draws_sky)
{
    mView       = {};
    mViewLayers = 0;
    if (!draws_sky || !mReady)
    {
        return;
    }

    const LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky();

    mView.mProbe    = gCubeSnapshot;
    mView.mMirror   = gPipeline.mHeroProbeManager.isMirrorPass();
    mView.mRadiance = gPipeline.mReflectionMapManager.isRadiancePass();
    mView.mHdri     = useHDRISky();

    // The sun, and its bloom while the latched input draws it (its textures are fetched only then); the moon, its first
    // texture alone.
    mView.mSun   = psky->getIsSunUp() && texturePair(mSunTexture[0], mSunTexture[1], 0.f).mFirst;
    mView.mBloom =
        gPipeline.shaderInputs().mLocal.mSunBloom && texturePair(mBloomTexture[0], mBloomTexture[1], 0.f).mFirst;
    mView.mMoon  = psky->getIsMoonUp() && resident(mMoonTexture[0]);

    // The night: the stars' brightness gates the meteors and the aurora, and the moon washes the aurora out.
    static LLCachedControl<bool> meteors_enabled(gSavedSettings, "RenderMeteorsEnabled", true);
    static LLCachedControl<F32> aurora_intensity(gSavedSettings, "RenderAuroraIntensity", 0.6f);
    mView.mStarAlpha = psky->getStarBrightness() / 500.0f;
    mView.mMeteors   = meteors_enabled;
    const F32 moon_wash = psky->getIsMoonUp() ? (F32)psky->getMoonBrightness() : 0.0f;
    mView.mAurora = aurora_intensity > 0.f ? aurora_intensity * llmax(0.0f, 1.0f - moon_wash * 0.85f) : 0.f;

    // The clouds, which take their first noise texture to draw.
    mView.mClouds = gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_CLOUDS) && resident(mCloudNoiseTexture[0]);

    mViewLayers = ALSkyLayers::layersFor(mView);
}

void ALSky::tickMeteors(F32 dt_seconds)
{
    LL_PROFILE_ZONE_SCOPED;

    if (dt_seconds <= 0.0f) return;

    // Age existing meteors and drop any that have run their course.
    for (MeteorState& m : mMeteors)
    {
        m.age += dt_seconds;
    }
    mMeteors.erase(
        std::remove_if(mMeteors.begin(), mMeteors.end(),
                       [](const MeteorState& m) { return m.age >= m.lifetime; }),
        mMeteors.end());

    static LLCachedControl<S32> max_count_setting(gSavedSettings, "RenderMeteorMaxCount", 16);
    static LLCachedControl<F32> spawn_secs_setting(gSavedSettings, "RenderMeteorSpawnInterval", 4.0f);
    const U32 max_count  = (U32)llclamp((S32)max_count_setting, 0, (S32)kMeteorHardMax);
    const F32 spawn_secs = llmax(0.05f, (F32)spawn_secs_setting);

    // Poisson-ish spawn: per-frame probability = dt / mean_interval. At 60 FPS
    // with a 4s mean, this produces a new meteor roughly every 240 frames.
    if (mMeteors.size() < max_count
        && ll_frand() < dt_seconds / spawn_secs)
    {
        const F32 dome_radius = LLEnvironment::instance().getCurrentSky()->getDomeRadius();

        // Start point anywhere on the upper hemisphere, but prefer a bit above
        // the horizon so meteors don't immediately pop out of view.
        LLVector3 start_dir = ALStarField::uniformUpperHemisphere(globalRandom);
        // Lift horizon-hugging starts slightly so short streaks stay visible.
        if (start_dir.mV[2] < 0.1f)
        {
            start_dir.mV[2] = 0.1f;
            start_dir.normVec();
        }

        // Travel direction is a random tangent in the sky tangent plane at
        // start_dir. This gives great-circle-like paths.
        LLVector3 travel_dir = randomTangentTo(start_dir, globalRandom);

        // Classify each new meteor into a visual archetype. The weighted mix
        // makes showers feel alive at high spawn rates — most meteors are
        // faint and quick, but the occasional fireball with exotic chemistry
        // colors draws the eye. The cumulative thresholds below are the
        // probability cutoffs for each class.
        const F32 roll = ll_frand();
        F32 width_scale;
        F32 path_mult  = 1.0f;       // multiplier on both path and trail length
        F32 life_mult  = 1.0f;       // multiplier on lifetime (shorter = faster)
        F32 peak;
        LLColor3 color;

        if (roll < 0.55f)
        {
            // Common: faint, thin, cool red-orange. These dominate by count
            // (matching real meteor distributions) and give the eye a quiet
            // background of activity.
            width_scale = 0.55f + 0.25f * ll_frand();
            peak        = 0.35f + 0.25f * ll_frand();
            life_mult   = 0.75f + 0.25f * ll_frand();
            const F32 tk = 2500.0f + 1500.0f * ll_frand();
            color = ALStarField::blackBodyColor(tk);
        }
        else if (roll < 0.85f)
        {
            // Standard: yellow-white, the "average" visible meteor.
            width_scale = 0.85f + 0.35f * ll_frand();
            peak        = 0.6f + 0.3f * ll_frand();
            const F32 tk = 4500.0f + 2500.0f * ll_frand();
            color = ALStarField::blackBodyColor(tk);
        }
        else if (roll < 0.96f)
        {
            // Fast: hot blue-white, travels farther in the same time so
            // apparent angular velocity is higher.
            width_scale = 0.7f + 0.3f * ll_frand();
            peak        = 0.8f + 0.2f * ll_frand();
            path_mult   = 1.25f + 0.35f * ll_frand();
            life_mult   = 0.6f + 0.25f * ll_frand();
            const F32 tk = 8000.0f + 7000.0f * ll_frand();
            color = ALStarField::blackBodyColor(tk);
        }
        else
        {
            // Fireball: thick, bright, persistent. Some roll to an exotic
            // chemistry color from ablation (Mg, Na, N, Cu) rather than the
            // usual blackbody curve.
            width_scale = 1.8f + 1.4f * ll_frand();
            peak        = 0.9f + 0.1f * ll_frand();
            path_mult   = 1.3f + 0.4f * ll_frand();
            life_mult   = 1.1f + 0.4f * ll_frand();

            const F32 chem = ll_frand();
            if (chem < 0.22f)       color = LLColor3(0.35f, 1.00f, 0.45f);  // magnesium green
            else if (chem < 0.42f)  color = LLColor3(1.00f, 0.55f, 0.20f);  // sodium orange
            else if (chem < 0.58f)  color = LLColor3(1.00f, 0.35f, 0.30f);  // nitrogen red
            else if (chem < 0.72f)  color = LLColor3(0.40f, 0.55f, 1.00f);  // copper/Mg blue
            else
            {
                // Hot white bolide, no exotic chemistry.
                const F32 tk = 9000.0f + 6000.0f * ll_frand();
                color = ALStarField::blackBodyColor(tk);
            }
        }

        MeteorState m;
        m.origin_world      = start_dir * dome_radius;
        m.direction_world   = travel_dir;
        // Base lifetime 1.1..2.5s. Fast class scales this down, fireballs up,
        // so the full range across classes is roughly 0.7..3.7s.
        m.lifetime          = (1.1f + 1.4f * ll_frand()) * life_mult;
        const F32 path_angle  = (0.21f + 0.31f * ll_frand()) * path_mult;   // radians
        const F32 trail_angle = path_angle * (0.35f + 0.25f * ll_frand());
        m.path_length_world  = dome_radius * path_angle;
        m.trail_length_world = dome_radius * trail_angle;
        m.color              = color;
        m.peak_intensity     = peak;
        m.width_scale        = width_scale;
        m.age                = 0.0f;

        mMeteors.push_back(m);
    }
}

void ALSky::meteors(std::vector<ALSkyLayers::Meteor>& out) const
{
    out.clear();
    for (const MeteorState& m : mMeteors)
    {
        if (out.size() >= kMeteorHardMax) break;

        // Envelope: quick fade in, steady, slower fade out. These ratios read
        // as a meteor "blooming" and then trailing off.
        const F32 frac = m.age / llmax(m.lifetime, 1e-4f);
        F32 alpha = 1.0f;
        if (frac < 0.10f)      alpha = frac / 0.10f;
        else if (frac > 0.80f) alpha = (1.0f - frac) / 0.20f;
        alpha = llclamp(alpha, 0.0f, 1.0f) * m.peak_intensity;
        if (alpha <= 0.0f) continue;

        // Head progresses linearly along the travel direction. Tail is a fixed
        // length behind the head regardless of age — the trail doesn't grow,
        // it just moves with the head.
        const LLVector3 head = m.origin_world + m.direction_world * (m.path_length_world * frac);
        const LLVector3 tail = head - m.direction_world * m.trail_length_world;

        // Pack per-frame envelope alpha into the RGB channels (in sRGB space — a
        // mild nonlinearity vs. linear-space premultiply, but the envelope is
        // smooth enough that the difference isn't visible). This frees the
        // color's alpha slot to carry the per-meteor width_scale, which the
        // fragment shader needs to normalize its cross-streak Gaussian so
        // thicker meteors are actually thicker (not just bigger empty quads).
        LLColor4 color4;
        color4.mV[VRED]   = m.color.mV[VRED]   * alpha;
        color4.mV[VGREEN] = m.color.mV[VGREEN] * alpha;
        color4.mV[VBLUE]  = m.color.mV[VBLUE]  * alpha;
        // Width_scale range is [0, 4]; pack as /4 so it fits in 8-bit unsigned.
        color4.mV[VALPHA] = llclamp(m.width_scale * 0.25f, 0.0f, 1.0f);

        ALSkyLayers::Meteor meteor;
        meteor.mMidpoint = (head + tail) * 0.5f;
        meteor.mHalf     = (head - tail) * 0.5f;
        meteor.mColour   = LLColor4U(color4);
        meteor.mWidth    = m.width_scale;
        out.push_back(meteor);
    }
}

bool ALSky::useHDRISky()
{
    static LLCachedControl<F32> hdri_split(gSavedSettings, "RenderHDRISplitScreen", 1.f);
    static LLCachedControl<bool> irradiance_only(gSavedSettings, "RenderHDRIIrradianceOnly", false);

    return gCubeSnapshot && (!irradiance_only || !gPipeline.mReflectionMapManager.isRadiancePass()) ? gEXRImage.notNull() : // always use HDRI for reflection probes when available
        gEXRImage.notNull() ? hdri_split > 0.f : // fallback to EEP sky when split screen is zero
        false; // no HDRI available, always use EEP sky
}

ALSky::DomeUniforms ALSky::domeUniforms() const
{
    LLEnvironment&             environment = LLEnvironment::instance();
    const LLSettingsSky::ptr_t psky        = environment.getCurrentSky();

    DomeUniforms dome;
    dome.mMoisture = (F32)psky->getSkyMoistureLevel();
    dome.mDroplet  = (F32)psky->getSkyDropletRadius();
    dome.mIce      = (F32)psky->getSkyIceLevel();

    // hobble halos and rainbows when there's no light source to generate them
    if (!psky->getIsSunUp() && !psky->getIsMoonUp())
    {
        dome.mMoisture = 0.0f;
        dome.mIce      = 0.0f;
    }

    dome.mGlowFactor = psky->getSunMoonGlowFactor();
    dome.mDensity    = psky->getDensityMultiplier();
    dome.mLightNorm  = environment.getClampedLightNorm();
    dome.mCamHeight  = environment.getCamHeight();
    dome.mDomeRadius = psky->getDomeRadius();
    return dome;
}

bool ALSky::EnvironmentKey::alike(const EnvironmentKey& other) const
{
    return mMaps == other.mMaps && ALSkyCube::termsAlike(mTerms, other.mTerms);
}

ALSky::EnvironmentKey ALSky::environmentKey() const
{
    // The members windlightDome reads of the Environment block (LLSettingsVOSky::applySpecial and
    // LLEnvironment::packAtmosphericTerms work them from the sky), and of its loose uniforms.
    const LLEnvironment::EnvironmentUBOData& block = LLEnvironment::instance().mEnvUBOData;
    const DomeUniforms dome = domeUniforms();

    EnvironmentKey key;
    size_t next = 0;
    const auto take = [&](const F32* values, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            key.mTerms[next++] = values[i];
        }
    };
    take(block.blue_horizon, 3);
    take(&block.haze_horizon, 1);
    take(block.glow, 3);
    take(&block.max_y, 1);
    take(&block.cloud_shadow, 1);
    take(block.haze_extinction, 3);
    take(block.blue_weight, 3);
    take(block.haze_weight, 3);
    take(block.sky_light, 3);
    take(block.sky_ambient, 3);
    take(block.sky_light_atten, 3);
    take(block.sky_ambient_below, 3);
    take(&dome.mMoisture, 1);
    take(&dome.mDroplet, 1);
    take(&dome.mIce, 1);
    take(&dome.mGlowFactor, 1);
    take(&dome.mDensity, 1);
    take(dome.mLightNorm.mV, 3);
    take(&dome.mCamHeight, 1);
    take(&dome.mDomeRadius, 1);
    llassert(next == key.mTerms.size());

    // A map still loading is baked as far as it has come, and again as it comes in.
    for (const auto& [slot, map] : { std::pair{ 0, mRainbowMap.get() }, std::pair{ 2, mHaloMap.get() } })
    {
        key.mMaps[slot] = map && map->getGLTexture() ? reinterpret_cast<uintptr_t>(map->getGLTexture()->getMetalTexture()) : 0;
        key.mMaps[slot + 1] = map ? static_cast<uintptr_t>(map->getDiscardLevel() + 1) : 0;
    }
    return key;
}

void ALSky::addSunMoonBeacons()
{
    if (!gAgentAvatarp || !mReady) return;

    static LLUICachedControl<bool> show_sun_beacon("sunbeacon", false);
    static LLUICachedControl<bool> show_moon_beacon("moonbeacon", false);

    const LLEnvironment& environment = LLEnvironment::instance();
    if (show_sun_beacon)
    {
        renderSunMoonBeacons(gAgentAvatarp->getPositionAgent(), environment.getSunDirection(),
                             LLColor4(1.f, 0.5f, 0.f, 0.5f));
    }
    if (show_moon_beacon)
    {
        renderSunMoonBeacons(gAgentAvatarp->getPositionAgent(), environment.getMoonDirection(),
                             LLColor4(1.f, 0.f, 0.8f, 0.5f));
    }
}


ALSky::~ALSky() { cleanup(); }

void ALSky::followEnvironment()
{
    if (mReady) takeSky();
}

void ALSky::cleanup()
{
    mStarCountConnection.disconnect();
    mStarLifetime.reset();
    ++mStarRequest;
    mStars.clear();
    mStarLayout = {};
    mStarsVerts = nullptr;
    mStarsBuildFailed = false;
    for (auto* textures : { &mSunTexture, &mMoonTexture, &mCloudNoiseTexture, &mBloomTexture }) textures->fill(nullptr);
    mRainbowMap = nullptr;
    mHaloMap = nullptr;
    mMeteors.clear();
    mMeteorsInFlight.clear();
    mMeteorData.clear();
    mView = {};
    mViewLayers = 0;
    mReady = false;
    forceSkyUpdate();
}

void ALSky::releaseNative()
{
    mStarsBuildFailed = false;
    mStarsVerts = nullptr;
    forceSkyUpdate();
}

void ALSky::restoreNative() { forceSkyUpdate(); }

bool ALSky::buildStars()
{
    if (mStarsVerts.notNull()) return mStarsVerts->prepareMetalBuffers(false);
    const U32 count = mStarLayout.vertices();
    if (!count || mStarsBuildFailed) return false;
    LLPointer<LLVertexBuffer> candidate = new LLVertexBuffer(STAR_VERTEX_DATA_MASK);
    LLStrider<LLVector4a> vertices;
    if (!candidate->allocateBuffer(count, 0) || !candidate->getVertexStrider(vertices))
    {
        mStarsBuildFailed = true;
        LL_WARNS("Sky") << "Could not allocate the star field's native buffer" << LL_ENDL;
        return false;
    }
    ALStarField::fillVertices(mStars, mStarLayout, vertices);
    candidate->unmapBuffer();
    mStarsVerts = candidate;
    return mStarsVerts->prepareMetalBuffers(false);
}

ALSky::NativeView::NativeView(NativeView&& other) noexcept { *this = std::move(other); }
ALSky::NativeView& ALSky::NativeView::operator=(NativeView&& other) noexcept
{
    if (this != &other)
    {
        scene = other.scene;
        stars = other.stars;
        draws = std::move(other.draws);
        textures = std::move(other.textures);
        vertices = std::move(other.vertices);
        fixup();
        other.reset();
    }
    return *this;
}
void ALSky::NativeView::fixup()
{
    stars.draws = draws.empty() ? nullptr : draws.data();
    stars.draw_count = draws.size();
}
void ALSky::NativeView::reset()
{
    scene = {};
    stars = {};
    draws.clear();
    textures.clear();
    vertices = nullptr;
}

bool ALSky::environmentUpdateDue()
{
    if (!mReady) return false;
    const EnvironmentKey key = environmentKey();
    const F64 now = LLFrameTimer::getTotalSeconds();
    const bool due = now - mEnvironmentUpdated >= ENVIRONMENT_UPDATE_SECONDS;
    if (mEnvironmentDirty ? (mEnvironmentRetry && !due) : (key.alike(mEnvironmentKey) || !due)) return false;
    mAtmospherics.updateFog(LLEnvironment::instance().getClampedLightDirection());
    mEnvironmentUpdated = now;
    mPendingEnvironmentKey = key;
    mEnvironmentPending = true;
    return true;
}
void ALSky::environmentUpdated(bool success)
{
    if (!mEnvironmentPending) return;
    mEnvironmentRetry = !success;
    if (success)
    {
        mEnvironmentKey = mPendingEnvironmentKey;
        mEnvironmentDirty = false;
    }
    else mEnvironmentDirty = true;
    mEnvironmentPending = false;
}

bool ALSky::captureNativeView(NativeView& out, U32 width, U32 height, bool emissive, bool hdr, bool reverse_z)
{
    out.reset();
    if (!mReady || !width || !height) return false;
    const LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();
    if (!sky) return false;
    try
    {
        const auto nativeImage = [&out](LLImageGL* image) -> const ALMetalTexture*
        {
            if (!image || !image->getMetalTexture()) return nullptr;
            out.textures.emplace_back(image);
            return image->getMetalTexture();
        };
        const auto native = [&nativeImage](LLViewerTexture* texture) -> const ALMetalTexture*
        {
            return texture ? nativeImage(texture->getGLTexture()) : nullptr;
        };
        ALMetalWorldModernSkyScene& scene = out.scene;
        ALMetalWorldModernSkyConstants& constants = scene.constants;
        scene.width = width;
        scene.height = height;
        constants.config[0] = mViewLayers;
        constants.config[1] = emissive;
        constants.config[2] = reverse_z;
        const LLEnvironment& environment = LLEnvironment::instance();
        const auto& block = environment.getEnvironmentUBOData();
        const DomeUniforms dome = domeUniforms();
        auto& d = constants.dome;
        d.camera_local[1] = dome.mCamHeight;
        d.camera_local[3] = dome.mDomeRadius;
        std::memcpy(d.light_density, dome.mLightNorm.mV, sizeof(F32) * 3);
        d.light_density[3] = dome.mDensity;
        std::memcpy(d.blue_horizon, block.blue_horizon, sizeof(block.blue_horizon));
        d.blue_horizon[3] = block.haze_horizon;
        std::memcpy(d.glow, block.glow, sizeof(block.glow));
        d.glow[3] = block.max_y;
        std::memcpy(d.haze_extinction, block.haze_extinction, sizeof(block.haze_extinction));
        std::memcpy(d.blue_weight, block.blue_weight, sizeof(block.blue_weight));
        std::memcpy(d.haze_weight, block.haze_weight, sizeof(block.haze_weight));
        std::memcpy(d.sky_light, block.sky_light, sizeof(block.sky_light));
        std::memcpy(d.sky_ambient, block.sky_ambient, sizeof(block.sky_ambient));
        std::memcpy(d.sky_light_atten, block.sky_light_atten, sizeof(block.sky_light_atten));
        std::memcpy(d.sky_ambient_below, block.sky_ambient_below, sizeof(block.sky_ambient_below));
        d.sky_ambient_below[3] = block.cloud_shadow;
        d.optics[0] = dome.mMoisture;
        d.optics[1] = dome.mDroplet;
        d.optics[2] = dome.mIce;
        d.optics[3] = dome.mGlowFactor;
        scene.rainbow_texture = native(mRainbowMap);
        scene.halo_texture = native(mHaloMap);
        if (!scene.rainbow_texture) d.optics[0] = 0.f;
        if (!scene.halo_texture) d.optics[2] = 0.f;

        auto& c = constants.clouds;
        std::memcpy(c.camera_local, d.camera_local, sizeof(c.camera_local));
        std::memcpy(c.light_density, d.light_density, sizeof(c.light_density));
        std::memcpy(c.blue_horizon, d.blue_horizon, sizeof(c.blue_horizon));
        std::memcpy(c.glow, d.glow, sizeof(c.glow));
        std::memcpy(c.haze_extinction, d.haze_extinction, sizeof(c.haze_extinction));
        std::memcpy(c.blue_weight, d.blue_weight, sizeof(c.blue_weight));
        std::memcpy(c.haze_weight, d.haze_weight, sizeof(c.haze_weight));
        std::memcpy(c.sky_sunlight, block.sky_sunlight, sizeof(block.sky_sunlight));
        std::memcpy(c.atmos_ambient, block.atmos_ambient, sizeof(block.atmos_ambient));
        std::memcpy(c.sky_light_atten, block.sky_light_atten, sizeof(block.sky_light_atten));
        std::memcpy(c.cloud_sunlight, block.cloud_sunlight, sizeof(block.cloud_sunlight));
        std::memcpy(c.cloud_color_shadow, block.cloud_color, sizeof(block.cloud_color));
        c.cloud_color_shadow[3] = block.cloud_shadow;
        std::memcpy(c.cloud_pos_density1, block.cloud_pos_density1, sizeof(block.cloud_pos_density1));
        std::memcpy(c.cloud_pos_density2, block.cloud_pos_density2, sizeof(block.cloud_pos_density2));
        const F32 blend = (F32)sky->getBlendFactor();
        const TexturePair cloud = texturePair(mCloudNoiseTexture[0], mCloudNoiseTexture[1],
            sky->getCloudScrollRate().isExactlyZero() ? 0.f : blend);
        c.parameters[0] = block.cloud_scale;
        c.parameters[1] = (F32)sky->getCloudVariance();
        c.parameters[2] = cloud.mBlend;
        c.parameters[3] = dome.mGlowFactor;
        scene.cloud_texture = native(cloud.mFirst);
        scene.cloud_next_texture = cloud.mNext ? native(cloud.mNext) : scene.cloud_texture;
        const auto& camera = LLViewerCamera::getCurrent();
        LLMatrix4a inverseProjection;
        if (!inverseProjection.setInverse(camera.getProjection())) return false;
        std::memcpy(constants.inverse_projection, inverseProjection.getF32ptr(), sizeof(constants.inverse_projection));
        const auto eyeToSky = ALFarPlane::eyeToSkyFrame(camera.getModelview());
        for (U32 axis = 0; axis < 3; ++axis)
            std::memcpy(constants.eye_to_sky[axis], eyeToSky.data() + 3 * axis, sizeof(F32) * 3);
        static LLCachedControl<bool> autoAdjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
        static LLCachedControl<F32> autoAdjustScale(gSavedSettings, "RenderSkyAutoAdjustHDRScale", 2.f);
        constants.transfer[2] = sky->getReflectionProbeAmbiance() != 0.f ? sqrtf(sky->getGamma()) * 2.f
            : sky->canAutoAdjust() && autoAdjust ? (F32)autoAdjustScale : 1.f;
        constants.transfer[3] = sky->canAutoAdjust() && !autoAdjust ? 1.1f : 1.f;
        if (mViewLayers & ALSkyLayers::HDRI)
        {
            static LLCachedControl<F32> exposure(gSavedSettings, "RenderHDRIExposure", 0.f);
            static LLCachedControl<F32> rotation(gSavedSettings, "RenderHDRIRotation", 0.f);
            static LLCachedControl<F32> split(gSavedSettings, "RenderHDRISplitScreen", 1.f);
            scene.hdri_texture = nativeImage(gEXRImage);
            LLMatrix3 turn;
            turn.setRot(0.f, (F32)rotation * DEG_TO_RAD, 0.f);
            for (U32 axis = 0; axis < 3; ++axis)
                std::memcpy(constants.hdri_rotation[axis], turn.mMatrix[axis], sizeof(F32) * 3);
            constants.transfer[0] = powf(2.f, (F32)exposure);
            constants.transfer[1] = gCubeSnapshot ? 1.f : (F32)split;
            return scene.hdri_texture != nullptr;
        }
        const auto quad = [](F32 (&destination)[3][4], const std::array<LLVector3,3>& source)
        {
            for (U32 i = 0; i < 3; ++i) std::memcpy(destination[i], source[i].mV, sizeof(F32) * 3);
        };
        const TexturePair sun = texturePair(mSunTexture[0], mSunTexture[1], blend);
        const TexturePair bloom = texturePair(mBloomTexture[0], mBloomTexture[1], blend);
        if (mViewLayers & ALSkyLayers::SUN)
            quad(constants.sun_quad, ALSkyLayers::rayQuad(mSun.corners(), LLVector3(0.f,0.f,-ALSkyLayers::SUN_DROP)));
        if (mViewLayers & ALSkyLayers::BLOOM)
            quad(constants.bloom_quad, ALSkyLayers::rayQuad(ALSkyLayers::bloomCorners(mSun.corners()),
                LLVector3(0.f,0.f,-ALSkyLayers::SUN_DROP)));
        if (mViewLayers & ALSkyLayers::MOON)
            quad(constants.moon_quad, ALSkyLayers::rayQuad(mMoon.corners()));
        scene.sun_texture = native(sun.mFirst);
        scene.sun_next_texture = sun.mNext ? native(sun.mNext) : scene.sun_texture;
        scene.moon_texture = native(mMoonTexture[0]);
        scene.bloom_texture = native(bloom.mFirst);
        scene.bloom_next_texture = bloom.mNext ? native(bloom.mNext) : scene.bloom_texture;
        constants.body[0] = sun.mBlend;
        constants.body[1] = (F32)sky->getMoonBrightness();
        constants.body[2] = ALSkyLayers::moonFade(sky->getMoonDirection().mV[VZ]);
        constants.body[3] = bloom.mBlend;
        LLColor3 bloomColor = sky->getSunDiffuse();
        bloomColor.clamp();
        std::memcpy(constants.bloom_color, bloomColor.mV, sizeof(F32) * 3);

        const LLVector3& origin = camera.getOrigin();
        LLMatrix4a skyView;
        skyView.setMul(LLMatrix4a::translation(origin.mV[VX], origin.mV[VY], origin.mV[VZ]), camera.getModelview());
        LLMatrix4a skyMvp;
        skyMvp.setMul(skyView, camera.getProjection());
        const F32 dt = llmin((F32)gFrameIntervalSeconds.value(), 0.1f);
        if (mViewLayers & ALSkyLayers::METEORS)
        {
            tickMeteors(dt);
            meteors(mMeteorsInFlight);
            const U32 count = ALSkyLayers::packMeteors(mMeteorsInFlight, skyMvp, (F32)width, (F32)height,
                ALSkyLayers::METEOR_WIDTH_PIXELS, mMeteorData);
            if (count)
            {
                constants.config[3] = count;
                for (size_t i = 0; i < mMeteorData.size(); ++i)
                    std::memcpy(scene.meteor_data[i], mMeteorData[i].mV, sizeof(F32) * 4);
            }
            else constants.config[0] &= ~ALSkyLayers::METEORS;
        }
        if (mViewLayers & ALSkyLayers::AURORA)
        {
            mAuroraTime = fmod(mAuroraTime + dt, 86400.0);
            constants.aurora[0] = mView.mAurora;
            constants.aurora[1] = (F32)mAuroraTime;
        }
        if ((mViewLayers & ALSkyLayers::STARS) && buildStars())
        {
            const F32 rotation = (F32)fmod(gFrameTimeSeconds * 0.01, 360.0);
            const LLMatrix4a turn = LLMatrix4a::rotation(rotation * DEG_TO_RAD, LLVector4a(0.f,0.f,1.f));
            LLMatrix4a starView;
            starView.setMul(turn, skyView);
            LLMatrix4a starMvp;
            starMvp.setMul(starView, camera.getProjection());
            LLMatrix4a inverseTurn = turn;
            inverseTurn.transpose();
            const auto toStars = [&inverseTurn](const LLVector3& direction)
            {
                LLVector4a result;
                inverseTurn.rotate(LLVector4a(direction.mV[VX],direction.mV[VY],direction.mV[VZ]), result);
                return LLVector3(result.getF32ptr());
            };
            auto& stars = out.stars;
            stars.vertices = mStarsVerts->getMetalVertexBuffer();
            stars.position_offset = mStarsVerts->getOffset(LLVertexBuffer::TYPE_VERTEX);
            stars.vertex_count = mStarLayout.vertices();
            stars.width = width;
            stars.height = height;
            stars.emissive = emissive;
            stars.hdr = hdr;
            stars.reverse_z = reverse_z;
            stars.star_alpha = mView.mStarAlpha;
            stars.time = (F32)fmod(LLFrameTimer::getUptimeSeconds() * 0.5, 86400.0);
            const LLVector3 moonDirection = toStars(sky->getMoonDirection());
            std::memcpy(stars.moon_direction, moonDirection.mV, sizeof(F32) * 3);
            stars.moon_brightness = sky->getIsMoonUp() ? (F32)sky->getMoonBrightness() : 0.f;
            std::memcpy(stars.modelview_projection, starMvp.getF32ptr(), sizeof(stars.modelview_projection));
            if (sky->getIsMoonUp() && scene.moon_texture)
            {
                const auto& corners = mMoon.corners();
                const LLVector3 center = (corners[0] + corners[3]) * 0.5f;
                const F32 distance = center.length();
                if (distance > 0.f)
                {
                    LLVector3 halfU = (corners[2] - corners[0]) * (0.5f / distance);
                    LLVector3 halfV = (corners[0] - corners[1]) * (0.5f / distance);
                    if (halfU.lengthSquared() > 0.f && halfV.lengthSquared() > 0.f)
                    {
                        halfU /= halfU.lengthSquared();
                        halfV /= halfV.lengthSquared();
                        const LLVector3 centerStar = toStars(center / distance);
                        const LLVector3 uStar = toStars(halfU), vStar = toStars(halfV);
                        std::memcpy(stars.moon_quad_center, centerStar.mV, sizeof(F32) * 3);
                        std::memcpy(stars.moon_quad_u, uStar.mV, sizeof(F32) * 3);
                        std::memcpy(stars.moon_quad_v, vStar.mV, sizeof(F32) * 3);
                        stars.moon_texture = scene.moon_texture;
                    }
                }
            }
            const F32 moonPower = stars.moon_brightness * llmax(0.f, moonDirection.mV[VZ]);
            const U32 faintest = emissive && hdr ? 0 : ALStarField::faintestVisible(mView.mStarAlpha, moonPower, emissive);
            const U32 quads = ALStarField::quadsDrawn(mStarLayout, faintest);
            if (quads) out.draws.push_back({0, quads * ALStarField::QUAD_VERTICES, true});
            ALStarField::pointDraws(mStarLayout, ALStarField::pointFrustum(starMvp,width,height), faintest, mStarDraws);
            for (size_t i = 0; i < mStarDraws.mFirst.size(); ++i)
                out.draws.push_back({(U32)mStarDraws.mFirst[i], (U32)mStarDraws.mCount[i], false});
            out.vertices = mStarsVerts;
            out.fixup();
        }
        return true;
    }
    catch (const std::bad_alloc&)
    {
        out.reset();
        LL_WARNS("Sky") << "Could not capture the native sky's view inputs" << LL_ENDL;
        return false;
    }
}
