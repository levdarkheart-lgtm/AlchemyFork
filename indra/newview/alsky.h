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

#pragma once

#include "alskylayers.h"
#include "alstarfield.h"
#include "almetalworld.h"
#include "lllegacyatmospherics.h"
#include "llimagegl.h"
#include "llpointer.h"
#include "llquaternion.h"
#include "lluuid.h"
#include "llvertexbuffer.h"
#include "v3color.h"
#include "v3math.h"
#include "v4color.h"
#include <boost/signals2/connection.hpp>
#include <array>
#include <memory>
#include <vector>

class LLViewerFetchedTexture;
class LLViewerTexture;
const F32 HEAVENLY_BODY_DIST = HORIZON_DIST - 20.f;

class LLHeavenBody
{
public:
    explicit LLHeavenBody(F32 disk_radius) : mDiskRadius(disk_radius) {}
    F32 getDiskRadius() const { return mDiskRadius; }
    const ALSkyLayers::Corners& corners() const { return mCorners; }
    void setCorners(const ALSkyLayers::Corners& corners) { mCorners = corners; }
    F32 angularHalfSize() const;
private:
    F32 mDiskRadius;
    ALSkyLayers::Corners mCorners{};
};

// CPU sky state and immutable native inputs. The pipeline owns pass execution and the environment cube.
class ALSky
{
public:
    static constexpr U32 STAR_VERTEX_DATA_MASK = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXTURE_INDEX;
    struct NativeView
    {
        ALMetalWorldModernSkyScene scene;
        ALMetalWorldModernSkyStars stars;
        std::vector<ALMetalWorldModernSkyStarDraw> draws;
        std::vector<LLPointer<LLImageGL>> textures;
        LLPointer<LLVertexBuffer> vertices;
        NativeView() = default;
        NativeView(const NativeView&) = delete;
        NativeView& operator=(const NativeView&) = delete;
        NativeView(NativeView&& other) noexcept;
        NativeView& operator=(NativeView&& other) noexcept;
        void reset();
        void fixup();
    };
    ALSky();
    ~ALSky();
    ALSky(const ALSky&) = delete;
    ALSky& operator=(const ALSky&) = delete;
    void update();
    bool ready() const { return mReady; }
    void followEnvironment();
    void cleanup();
    void releaseNative();
    void restoreNative();
    void forceSkyUpdate();
    void setSunDirectionCFR(const LLVector3& sun_direction);
    void setSunTextures(const LLUUID& first, const LLUUID& next);
    void setMoonTextures(const LLUUID& first, const LLUUID& next);
    void setCloudNoiseTextures(const LLUUID& first, const LLUUID& next);
    void setBloomTextures(const LLUUID& first, const LLUUID& next);
    void setSunScale(F32 scale) { mSunScale = scale; }
    void setMoonScale(F32 scale) { mMoonScale = scale; }
    const LLHeavenBody& getSun() const { return mSun; }
    const LLHeavenBody& getMoon() const { return mMoon; }
    const LLVector3& getBumpSunDir() const { return mBumpSunDir; }
    LLColor4 getSkyFogColor() const { return mAtmospherics.getFogColor(); }
    void beginView(bool draws_sky);
    S32 viewLayers() const { return mViewLayers; }
    // Captures the current camera and the view policy begun above. Retain NativeView until encoding ends.
    bool captureNativeView(NativeView& out, U32 width, U32 height, bool emissive, bool hdr, bool reverse_z);
    static bool useHDRISky();
    void addSunMoonBeacons();
    // .25-second change/retry policy. A failed GPU bake never publishes its key as current.
    bool environmentUpdateDue();
    void environmentUpdated(bool success);
private:
    void fetchTextures();
    void takeSky();
    struct StarField { std::vector<ALStarField::Star> mStars; ALStarField::Layout mLayout; };
    void requestStars();
    void landStars(U32 request, StarField&& field);
    bool buildStars();
    void tickMeteors(F32 dt_seconds);
    void meteors(std::vector<ALSkyLayers::Meteor>& out) const;
    struct DomeUniforms
    {
        F32 mMoisture=0.f, mDroplet=0.f, mIce=0.f, mGlowFactor=0.f, mDensity=0.f;
        LLVector4 mLightNorm;
        F32 mCamHeight=0.f, mDomeRadius=0.f;
    };
    DomeUniforms domeUniforms() const;
    struct EnvironmentKey
    {
        std::array<F32,40> mTerms{};
        std::array<uintptr_t,4> mMaps{};
        bool alike(const EnvironmentKey& other) const;
    };
    EnvironmentKey environmentKey() const;
    static void renderSunMoonBeacons(const LLVector3& pos_agent, const LLVector3& direction, LLColor4 color);
    bool mReady=false;
    LLHeavenBody mSun, mMoon;
    F32 mSunScale=1.f, mMoonScale=1.f;
    LLVector3 mBumpSunDir;
    std::array<LLUUID,2> mSunTextureId, mMoonTextureId, mCloudNoiseTextureId, mBloomTextureId;
    std::array<LLPointer<LLViewerFetchedTexture>,2> mSunTexture, mMoonTexture, mCloudNoiseTexture, mBloomTexture;
    LLPointer<LLViewerFetchedTexture> mRainbowMap, mHaloMap;
    std::vector<ALStarField::Star> mStars;
    ALStarField::Layout mStarLayout;
    ALStarField::Draws mStarDraws;
    LLPointer<LLVertexBuffer> mStarsVerts;
    bool mStarsBuildFailed=false;
    U32 mStarRequest=0;
    std::shared_ptr<U8> mStarLifetime;
    boost::signals2::scoped_connection mStarCountConnection;
    struct MeteorState
    {
        LLVector3 origin_world, direction_world;
        F32 path_length_world, trail_length_world, age, lifetime;
        LLColor3 color;
        F32 peak_intensity, width_scale;
    };
    std::vector<MeteorState> mMeteors;
    std::vector<ALSkyLayers::Meteor> mMeteorsInFlight;
    std::vector<LLVector4> mMeteorData;
    F64 mAuroraTime=0.0;
    ALSkyLayers::Frame mView;
    S32 mViewLayers=0;
    LLAtmospherics mAtmospherics;
    EnvironmentKey mEnvironmentKey, mPendingEnvironmentKey;
    F64 mEnvironmentUpdated=0.0;
    bool mEnvironmentDirty=true, mEnvironmentRetry=false, mEnvironmentPending=false;
};
