/**
 * @file LLHeroProbeManager.cpp
 * @brief LLHeroProbeManager class implementation
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

#include "llheroprobemanager.h"
#include "llreflectionmapmanager.h"
#include "llviewercamera.h"
#include "llspatialpartition.h"
#include "llviewerregion.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llviewercontrol.h"
#include "llenvironment.h"
#include "llstartup.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llviewerwindow.h"
#include "llviewerjoystick.h"
#include "llviewermediafocus.h"

#if LL_DARWIN
#include "../almetal/almetalstagetiming.h"
#include <optional>
#endif

extern bool gCubeSnapshot;
extern bool gTeleportDisplay;

// get the next highest power of two of v (or v if v is already a power of two)
//defined in llvertexbuffer.cpp
extern U32 nhpo2(U32 v);

static void touch_default_probe(LLReflectionMap* probe)
{
    if (LLViewerCamera::getInstance())
    {
        LLVector3 origin = LLViewerCamera::getInstance()->getOrigin();
        origin.mV[2] += 64.f;

        probe->mOrigin.load3(origin.mV);
    }
}

LLHeroProbeManager::LLHeroProbeManager() = default;

LLHeroProbeManager::~LLHeroProbeManager()
{
    cleanup();

    mHeroVOList.clear();
}

// helper class to seed octree with probes
void LLHeroProbeManager::update()
{
    // Not until the resources are reconciled for mirrors, which a shader reload that stops short of it leaves undone.
    if (!gPipeline.resourcePlan().heroProbes() || gTeleportDisplay || LLStartUp::getStartupState() < STATE_PRECACHE)
    {
        return;
    }

    // Part of a hacky workaround to fix #3331.
    // For some reason clearing shaders will cause mirrors to actually work.
    // There's likely some deeper state issue that needs to be resolved.
    // - Geenz 2025-02-25
    static LLCachedControl<bool> shader_reload_workaround(gSavedSettings, "AlchemyRenderMirrorShaderReload", true);
    if (!mInitialized && shader_reload_workaround && LLStartUp::getStartupState() > STATE_PRECACHE)
    {
        // The hero probes are rebuilt from nothing now, and every program from its sources by the next frame's
        // application: which of the two the workaround needs is not known.
        reset();
        LLViewerShaderMgr::instance()->clearShaderCache();
        gPipeline.markRenderInputsDirty(ALRenderInputRequests::Change::Programs);
        mInitialized = true;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LL_PROFILE_GPU_ZONE("hero manager update");
    llassert(!gCubeSnapshot); // assert a snapshot is not in progress
    if (LLAppViewer::instance()->logoutRequestSent())
    {
        return;
    }

    initReflectionMaps();

    llassert(mProbes[0] == mDefaultProbe);

    LLVector4a probe_pos;
    LLVector3 camera_pos = LLViewerCamera::instance().mOrigin;
    bool       probe_present = false;
    LLQuaternion cameraOrientation = LLViewerCamera::instance().getQuaternion();
    LLVector3    cameraDirection   = LLVector3::z_axis * cameraOrientation;

    if (mHeroVOList.size() > 0)
    {
        // Find our nearest hero candidate.
        float last_distance = 99999.f;
        float camera_center_distance = 99999.f;
        mNearestHero = nullptr;
        for (auto vo : mHeroVOList)
        {
            if (vo && !vo->isDead() && vo->mDrawable.notNull() && vo->isReflectionProbe() && vo->getReflectionProbeIsBox())
            {
                float distance = (LLViewerCamera::instance().getOrigin() - vo->getPositionAgent()).magVec();
                float center_distance = cameraDirection * (vo->getPositionAgent() - camera_pos);

                if (distance > LLViewerCamera::instance().getFar())
                    continue;

                LLVector4a center;
                center.load3(vo->getPositionAgent().mV);
                LLVector4a size;

                size.load3(vo->getScale().mV);

                bool visible = LLViewerCamera::instance().AABBInFrustum(center, size);

                if (distance < last_distance && center_distance < camera_center_distance && visible)
                {
                    probe_present = true;
                    mNearestHero = vo;
                    last_distance = distance;
                    camera_center_distance = center_distance;
                }
            }
            else
            {
                unregisterViewerObject(vo);
            }
        }

        // Don't even try to do anything if we didn't find a single mirror present.
        if (!probe_present)
            return;

        if (mNearestHero != nullptr && !mNearestHero->isDead() && mNearestHero->mDrawable.notNull())
        {
            LLVector3 hero_pos = mNearestHero->getPositionAgent();
            LLVector3 face_normal = LLVector3(0, 0, 1);

            face_normal *= mNearestHero->mDrawable->getWorldRotation();
            face_normal.normalize();

            LLVector3 offset = camera_pos - hero_pos;
            LLVector3 project = face_normal * (offset * face_normal);
            LLVector3 reject  = offset - project;
            LLVector3 point   = (reject - project) + hero_pos;

            mCurrentClipPlane.setVec(hero_pos, face_normal);
            mMirrorPosition = hero_pos;
            mMirrorNormal   = face_normal;

            probe_pos.load3(point.mV);

            // Detect visible faces of a cube based on camera direction and distance

            // Define the cube faces
            static LLVector3 cubeFaces[6] = {
                LLVector3(1, 0, 0),
                LLVector3(-1, 0, 0),
                LLVector3(0, 1, 0),
                LLVector3(0, -1, 0),
                LLVector3(0, 0, 1),
                LLVector3(0, 0, -1)
            };

            mProbes[0]->mOrigin = probe_pos;
            mProbes[0]->mRadius = mNearestHero->getScale().magVec() * 0.5f;
        }
        else
        {
            mNearestHero = nullptr;
            mDefaultProbe->mViewerObject = nullptr;
        }

        mHeroProbeStrength = 1;
    }
    else
    {
        mNearestHero = nullptr;
        mDefaultProbe->mViewerObject = nullptr;
    }
}

void LLHeroProbeManager::renderProbes()
{
    if (!gPipeline.resourcePlan().heroProbes() || gTeleportDisplay || LLStartUp::getStartupState() < STATE_PRECACHE)
    {
        return;
    }

    // cubeSnapshot() uses the ordinary reflection-detail contract to decide
    // whether dynamic render types (avatars, control avatars and particles)
    // belong in a probe face.  There is no RenderHeroReflectionProbeDetail
    // setting; reading that misspelled key always selected the fallback and
    // made every mirror capture static-only.
    static LLCachedControl<S32> sDetail(gSavedSettings, "RenderReflectionProbeDetail", -1);
    static LLCachedControl<S32> sLevel(gSavedSettings, "RenderHeroReflectionProbeLevel", 3);
    static LLCachedControl<S32> sUpdateRate(gSavedSettings, "RenderHeroProbeUpdateRate", 0);

    // Nothing to render into until update() has built the default probe again after cleanup().
    F32 near_clip = 0.01f;
    if (!mProbes.empty() && mProbes[0].notNull() && mNearestHero != nullptr && !mNearestHero->isDead() &&
        !gTeleportDisplay && !gDisconnected && !LLAppViewer::instance()->logoutRequestSent())
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("hpmu - realtime");

        bool radiance_pass = gPipeline.mReflectionMapManager.isRadiancePass();

        gPipeline.mReflectionMapManager.mRadiancePass = true;
        mRenderingMirror = true;
        // Mirrors isMirrorPass() down into llrender, which cannot reach gPipeline. This is
        // the window in which the mirror-clip shader variants apply.
        LLRender::sMirrorPass = true;

        S32 rate = sUpdateRate;

        // rate must be divisor of 6 (1, 2, 3, or 6)
        if (rate < 1)
        {
            rate = 1;
        }
        else if (rate > 3)
        {
            rate = 6;
        }

        S32 face = gFrameCount % 6;

        if (!mProbes[0]->mOccluded)
        {
            LL_PROFILE_ZONE_NUM(gFrameCount % rate);
            LL_PROFILE_ZONE_NUM(rate);

#if LL_DARWIN
            mMetalRadianceCaptureComplete = true;
#endif
            const bool object_dynamic =
                mNearestHero->getReflectionProbeIsDynamic();
            const bool dynamic = object_dynamic && sDetail() > 0;
            LL_INFOS_ONCE("Metal")
                << "Native mirror dynamic-capture contract: object_dynamic="
                << object_dynamic << ", reflection_detail=" << sDetail()
                << ", dynamic_render_types=" << dynamic
                << LL_ENDL;
            for (U32 i = 0; i < 6; ++i)
            {
                if ((gFrameCount % rate) == (i % rate))
                { // update 6/rate faces per frame
                    LL_PROFILE_ZONE_NUM(i);
                    updateProbeFace(mProbes[0], i, dynamic, near_clip);
                }
            }
            generateRadiance(mProbes[0]);
        }

        mRenderingMirror = false;
        LLRender::sMirrorPass = false;

        gPipeline.mReflectionMapManager.mRadiancePass = radiance_pass;

        mProbes[0]->mViewerObject = mNearestHero;
        mProbes[0]->autoAdjustOrigin();
    }
}

// Do the reflection map update render passes.
// For every 12 calls of this function, one complete reflection probe radiance map and irradiance map is generated
// First six passes render the scene with direct lighting only into a scratch space cube map at the end of the cube map array and generate
// a simple mip chain (not convolution filter).
// At the end of these passes, an irradiance map is generated for this probe and placed into the irradiance cube map array at the index for this probe
// The next six passes render the scene with both radiance and irradiance into the same scratch space cube map and generate a simple mip chain.
// At the end of these passes, a radiance map is generated for this probe and placed into the radiance cube map array at the index for this probe.
// In effect this simulates single-bounce lighting.
void LLHeroProbeManager::updateProbeFace(LLReflectionMap* probe, U32 face, bool is_dynamic, F32 near_clip)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LL_PROFILE_GPU_ZONE("hero probe update");
#if LL_DARWIN
    std::optional<ALMetalStageTimingScope> stage_scope;
    if (const auto* parent = ALMetalStageTimingScope::current();
        parent && parent->window() && parent->frame() == gFrameCount)
        stage_scope.emplace(parent->window(), gFrameCount, 2, static_cast<std::int32_t>(face));
#endif

    // hacky hot-swap of camera specific render targets
    gPipeline.mRT = &gPipeline.mHeroProbeRT;

    const bool native_face_capture = mNearestHero.notNull() &&
        !mNearestHero->isDead() &&
        gPipeline.beginMetalHeroProbeFaceCapture(
            mNearestHero->getID(), mMetalRadianceEpoch, face,
            static_cast<U32>(std::lround(mMaxProbeLOD)));
    if (!native_face_capture)
    {
        // Native mirror authority fails closed while its renderer/resources
        // warm or are unavailable. Do not render a hidden GL face that cannot
        // become the published Metal hero probe.
        mMetalRadianceCaptureComplete = false;
        LL_WARNS_ONCE("Metal")
            << "Native mirror warm-up could not begin hero-probe face "
            << face << "; the ordinary world remains present while the "
               "mirror retries"
            << LL_ENDL;
        gPipeline.mRT = &gPipeline.mMainRT;
        return;
    }
    const std::optional<ALRenderResourcePlan::ProbeManager>& planned =
        gPipeline.resourcePlan().heroProbes();
    if (!planned || planned->mResolution == 0)
    {
        mMetalRadianceCaptureComplete = false;
        gPipeline.endMetalHeroProbeFaceCapture();
        gPipeline.mRT = &gPipeline.mMainRT;
        return;
    }
    const U32 face_resolution = planned->mResolution;
    probe->update(face_resolution, face, is_dynamic, near_clip);
    const bool face_published = gPipeline.endMetalHeroProbeFaceCapture();
    mMetalRadianceCaptureComplete = face_published &&
        mMetalRadianceCaptureComplete;
    if (!face_published)
    {
        LL_WARNS_ONCE("Metal")
            << "Native mirror warm-up did not publish hero-probe face "
            << face << "; the ordinary world remains present while the "
               "mirror retries"
            << LL_ENDL;
    }

    gPipeline.mRT = &gPipeline.mMainRT;
}

// Separate out radiance generation as a separate stage.
// This is to better enable independent control over how we generate radiance vs. having it coupled with processing the final face of the probe.
// Useful when we may not always be rendering a full set of faces of the probe.
void LLHeroProbeManager::generateRadiance(LLReflectionMap*)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    ++mMetalRadianceGeneration;
    if (mMetalRadianceGeneration == 0)
    {
        ++mMetalRadianceGeneration;
    }
    const bool owner_ready = mNearestHero.notNull() &&
        !mNearestHero->isDead();
    mMetalRadianceReady = mMetalRadianceCaptureComplete && owner_ready &&
        gPipeline.publishMetalHeroProbeRadiance(
            mNearestHero->getID(), mMetalRadianceGeneration,
            mHeroProbeStrength);
    if (!mMetalRadianceReady)
    {
        LL_WARNS_ONCE("Metal")
            << "Native mirror radiance is still warming: faces_complete="
            << mMetalRadianceCaptureComplete
            << ", owner_ready=" << owner_ready
            << "; the mirror remains unadvertised to the main world"
            << LL_ENDL;
    }
}

void LLHeroProbeManager::updateUniforms()
{
    // Count is the publication gate for every consumer. Clear it before the
    // mirrors-off return so a disabled resource plan cannot leave the last
    // native radiance owner advertised to the visible world.
    mHeroData.heroProbeCount = 0;
    if (!gPipeline.resourceInputs().mMirrors)
    {
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LL_PROFILE_GPU_ZONE("hpmu - uniforms")

    const LLMatrix4a& modelview = LLViewerCamera::getCurrent().getModelview();
    LLVector4a oa; // scratch space for transformed origin
    oa.set(0, 0, 0, 0);
    // With no mirror, the box and sphere below keep the last one's, and the cube array its
    // picture; 0 tells tapHeroProbe there is nothing to reflect.
    // Called every frame while mirrors are on, whether or not update() has
    // built the default probe since cleanup(). The original GL owner allocates
    // its hero cube before advertising count one. Metal must likewise avoid
    // advertising a mirror until its complete radiance cube is publishable.
    const bool radiance_ready =
#if LL_DARWIN
        mMetalRadianceReady;
#else
        true;
#endif
    if (!mProbes.empty() && mProbes[0].notNull() &&
        mNearestHero != nullptr && !mNearestHero->isDead() &&
        radiance_ready)
    {
        mHeroData.heroProbeCount = 1;
        if (mNearestHero->getReflectionProbeIsBox())
        {
            LLVector3 s = mNearestHero->getScale().scaledVec(LLVector3(0.5f, 0.5f, 0.5f));
            mProbes[0]->mRadius = s.magVec();
        }
        else
        {
            mProbes[0]->mRadius = mNearestHero->getScale().mV[0] * 0.5f;
        }

        modelview.affineTransform(mProbes[0]->mOrigin, oa);
        mHeroData.heroShape = 0;
        if (!mProbes[0]->getBox(mHeroData.heroBox))
        {
            mHeroData.heroShape = 1;
        }

        mHeroData.heroSphere.set(oa.getF32ptr());
        mHeroData.heroSphere.mV[3] = mProbes[0]->mRadius;
    }

    mHeroData.heroMaxLod = mMaxProbeLOD;
}

void LLHeroProbeManager::initReflectionMaps()
{
    U32 count = LL_MAX_HERO_PROBE_COUNT;

    // Reconcile the logical owner with the native hero resource plan. Native
    // cube/radiance storage lives in the Metal renderer; this manager retains
    // only probe scheduling and publication identity.
    const std::optional<ALRenderResourcePlan::ProbeManager>& planned = gPipeline.resourcePlan().heroProbes();
    if (!planned)
    {
        return;
    }
    if (mResourcesInitialized &&
        (planned->mHDR != mHDR ||
         planned->mResolution != mProbeResolution))
    {
        mReset = true;
    }

    if (!mResourcesInitialized || mReflectionProbeCount != count || mReset)
    {

        if (mReset)
        {
            cleanup();
        }

        mReset = false;
        mReflectionProbeCount = count;
        mProbeResolution      = planned->mResolution;
        mMaxProbeLOD = log2f((F32)mProbeResolution) - 1.f; // number of mips - 1

        mHDR = planned->mHDR;
        mResourcesInitialized = true;

        if (mDefaultProbe.isNull())
        {
            llassert(mProbes.empty()); // default probe MUST be the first probe created
            mDefaultProbe = new LLReflectionMap();
            mProbes.push_back(mDefaultProbe);
        }

        llassert(mProbes[0] == mDefaultProbe);

        // For hero probes, we treat this as the main mirror probe.

        mDefaultProbe->mCubeIndex = 0;
        mDefaultProbe->mDistance  = gSavedSettings.getF32("RenderHeroProbeDistance");
        mDefaultProbe->mRadius = 4096.f;
        mDefaultProbe->mProbeIndex = 0;
        touch_default_probe(mDefaultProbe);
    }
}

void LLHeroProbeManager::cleanup()
{
    // cleanup() is the authoritative transition to "no hero probe". The
    // original shader contract uses count zero to stop sampling the retained
    // cube; Metal has direct consumers of the same contract, so retire it
    // synchronously rather than waiting for the next uniform update.
    mHeroData.heroProbeCount = 0;
    mRenderingMirror = false;
    LLRender::sMirrorPass = false;

    ++mMetalRadianceEpoch;
    if (mMetalRadianceEpoch == 0)
    {
        ++mMetalRadianceEpoch;
    }
    ++mMetalRadianceGeneration;
    if (mMetalRadianceGeneration == 0)
    {
        ++mMetalRadianceGeneration;
    }
    mMetalRadianceReady = false;
    mMetalRadianceCaptureComplete = false;
    mResourcesInitialized = false;

    mProbes.clear();

    mDefaultProbe = nullptr;
    // The mirror is found again by the next update() that builds a probe for it.
    mNearestHero = nullptr;
}

void LLHeroProbeManager::doOcclusion()
{
    LLVector4a eye;
    eye.load3(LLViewerCamera::instance().getOrigin().mV);

    for (auto& probe : mProbes)
    {
        if (probe != nullptr)
        {
            probe->doOcclusion(eye);
        }
    }
}

void LLHeroProbeManager::reset()
{
    mReset = true;
}

bool LLHeroProbeManager::registerViewerObject(LLVOVolume* drawablep)
{
    llassert(drawablep != nullptr);

    if (std::find(mHeroVOList.begin(), mHeroVOList.end(), drawablep) == mHeroVOList.end())
    {
        // Probe isn't in our list for consideration.  Add it.
        mHeroVOList.push_back(drawablep);
        return true;
    }

    return false;
}

void LLHeroProbeManager::unregisterViewerObject(LLVOVolume* drawablep)
{
    std::vector<LLPointer<LLVOVolume>>::iterator found_itr = std::find(mHeroVOList.begin(), mHeroVOList.end(), drawablep);
    if (found_itr != mHeroVOList.end())
    {
        mHeroVOList.erase(found_itr);
    }
}
