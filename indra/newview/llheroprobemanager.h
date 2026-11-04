/**
 * @file llheroprobemanager.h
 * @brief LLHeroProbeManager class declaration
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

#pragma once

#include "llreflectionmap.h"
#include "lldrawable.h"

class LLSpatialGroup;
class LLViewerObject;

// number of reflection probes to keep in vram
#define LL_MAX_HERO_PROBE_COUNT 2

struct HeroProbeData
{
    LLMatrix4 heroBox;
    LLVector4 heroSphere;
    GLint     heroShape;
    GLfloat   heroMaxLod;     // the LOD radiance generation maps to roughness 1
    GLint     heroProbeCount; // 1 while there is a mirror to reflect, else 0
};

class alignas(16) LLHeroProbeManager
{
public:
    enum class DetailLevel
    {
        STATIC_ONLY = 0,
        STATIC_AND_DYNAMIC,
        REALTIME = 2
    };

    // allocate an environment map of the given resolution
    LLHeroProbeManager();
    ~LLHeroProbeManager();

    // release native hero-probe ownership and logical probe state
    void cleanup();

    // Whether initReflectionMaps has built logical/native ownership that
    // cleanup invalidates.
    bool holdsResources() const { return mResourcesInitialized; }

    // maintain reflection probes
    void update();

    void renderProbes();

    // debug display, called from llspatialpartition if reflection
    // probe debug display is active
    void renderDebug();

    // call once at startup to allocate cubemap arrays
    void initReflectionMaps();

    // perform occlusion culling on all active reflection probes
    void doOcclusion();

    void reset();

    bool registerViewerObject(LLVOVolume *drawablep);
    void unregisterViewerObject(LLVOVolume* drawablep);

    bool isMirrorPass() const { return mRenderingMirror; }

    LLVector3 mMirrorPosition;
    LLVector3     mMirrorNormal;
    HeroProbeData mHeroData;

private:
    friend class LLPipeline;
    friend class LLReflectionMapManager;

    // update UBO used for rendering (call only once per render pipe flush)
    void updateUniforms();

    LLPlane mCurrentClipPlane;


    // update the specified face of the specified probe
    void updateProbeFace(LLReflectionMap* probe, U32 face, bool is_dynamic, F32 near_clip);
    void generateRadiance(LLReflectionMap *probe);

    // list of active reflection maps
    std::vector<LLPointer<LLReflectionMap>> mProbes;

    LLPointer<LLReflectionMap> mDefaultProbe;  // default reflection probe to fall back to for pixels with no probe influences (should always be at cube index 0)

    // number of reflection probes to use for rendering
    U32 mReflectionProbeCount = 0;

    // resolution of reflection probes
    U32 mProbeResolution = 1024;

    // HDR mode reconciled into the native hero resource plan.
    bool mHDR = true;

    // maximum LoD of reflection probes (mip levels - 1)
    F32 mMaxProbeLOD = 6.f;

    // Monotonic content identity for the native hero-radiance mip family.
    // Readiness is separate because reconciled storage has no generated
    // radiance until a complete face transaction publishes.
    U64 mMetalRadianceGeneration = 0;
    U64 mMetalRadianceEpoch = 1;
    bool mMetalRadianceReady = false;
    bool mMetalRadianceCaptureComplete = false;
    bool mResourcesInitialized = false;

    F32 mHeroProbeStrength = 1.f;
    bool mIsInTransition = false;

    // if true, reset all probe render state on the next update (for teleports and sky changes)
    bool mReset = false;

    bool mRenderingMirror = false;

    std::vector<LLPointer<LLVOVolume>>                       mHeroVOList;
    LLPointer<LLVOVolume>                                 mNearestHero;

    // Part of a hacky workaround to fix #3331.
    bool mInitialized = false;
};

