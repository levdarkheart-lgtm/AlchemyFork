/**
 * @file llreflectionmapmanager.h
 * @brief LLReflectionMapManager class declaration
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

#include <memory>

class LLSpatialGroup;
class LLViewerObject;
#if LL_DARWIN
struct ALMetalPBRPreviewEnvironment;
struct ALMetalWorldReflectionProbes;
struct ALMetalWorldReflectionProbeConstants;
class ALMetalRenderTarget;
class ALMetalWorldGBufferRenderer;
#endif

// number of reflection probes to keep in vram
#define LL_MAX_REFLECTION_PROBE_COUNT 256

// Second-order SH: one DC term, three linear, five quadratic.
#define LL_SH_COEFF_COUNT 9

// RGBA32UI texels a native probe's coefficients pack into: 27 halves, eight
// to a texel.
#define LL_SH_PACKED_TEXELS 4

// reflection probe mininum scale
#define LL_REFLECTION_PROBE_MINIMUM_SCALE 1.f;

void renderReflectionProbe(LLReflectionMap* probe);

class alignas(16) LLReflectionMapManager
{
public:
    enum class DetailLevel
    {
        STATIC_ONLY = 0,
        STATIC_AND_DYNAMIC,
        REALTIME = 2
    };

    // General guidance for UBOs is to statically allocate all of these fields to make your life ever so slightly easier.
    // Then set a "max" value for the number of probes you'll ever have, and use that to index into the arrays.
    // We do this with refmapCount.  The shaders will just pick up on it there.
    // This data structure should _always_ match what's in class3/deferred/reflectionProbeF.glsl.
    // The shader can and will break otherwise.
    // -Geenz 2025-03-10
    struct ReflectionProbeData
    {
        // for box probes, matrix that transforms from camera space to a [-1, 1] cube representing the bounding box of
        // the box probe
        LLMatrix4 refBox[LL_MAX_REFLECTION_PROBE_COUNT];

        LLMatrix4 heroBox;

        // for sphere probes, origin (xyz) and radius (w) of refmaps in clip space
        LLVector4 refSphere[LL_MAX_REFLECTION_PROBE_COUNT];

        // extra parameters
        //  x - irradiance scale
        //  y - radiance scale
        //  z - fade in
        //  w - nearest view depth of the probe's bounding sphere (mMinDepth), which the list is
        //      sorted on: the shader's scan stops at the first probe beyond the pixel
        LLVector4 refParams[LL_MAX_REFLECTION_PROBE_COUNT];

        LLVector4 heroSphere;

        // indices used by probe:
        //  [i][0] - cubemap array index for this probe
        //  [i][1] - index into "refNeighbor" for probes that intersect this probe
        //  [i][2] - number of probes  that intersect this probe, or -1 for no neighbors
        //  [i][3] - priority (probe type stored in sign bit - positive for spheres, negative for boxes)
        GLint refIndex[LL_MAX_REFLECTION_PROBE_COUNT][4];

        // list of neighbor indices
        GLint refNeighbor[4096];

        GLint refBucket[256][4]; // lookup table for which index to start with for the given Z depth
        // numbrer of active refmaps
        GLint refmapCount;

        GLint heroShape;
        GLfloat heroMaxLod;
        GLint heroProbeCount;

        // Screen-space reflection march parameters. Frame-constant, and every SSR consumer is
        // already reading this block, so they ride along here instead of being re-pushed as
        // loose uniforms on every bindReflectionProbes. (noiseSine is NOT here: it advances
        // per bind.)
        F32 iterationCount;
        F32 rayStep;
        F32 distanceBias;
        F32 depthRejectBias;
        F32 glossySampleCount;
        F32 adaptiveStepMultiplier;
        F32 _ssrTailPad[2]; // round the block to a 16-byte multiple (std140)
    };

    // allocate an environment map of the given resolution
    LLReflectionMapManager();
    ~LLReflectionMapManager();

    // release native reflection resources and logical probe state
    void cleanup();

    // maintain reflection probes
    void update();

    void refreshSettings();

    // add a probe for the given spatial group
    LLReflectionMap* addProbe(LLSpatialGroup* group = nullptr);

    // Populate "maps" with the N most relevant Reflection Maps where N is no more than maps.size()
    // If less than maps.size() ReflectionMaps are available, will assign trailing elements to nullptr.
    //  maps -- presized array of Reflection Map pointers
    void getReflectionMaps(std::vector<LLReflectionMap*>& maps);

    // called by LLSpatialGroup constructor
    // If spatial group should receive a Reflection Probe, will create one for the specified spatial group
    LLReflectionMap* registerSpatialGroup(LLSpatialGroup* group);

    // presently hacked into LLViewerObject::setTE
    // Used by LLViewerObjects that are Reflection Probes
    // vobj must not be null
    // Guaranteed to not return null
    LLReflectionMap* registerViewerObject(LLViewerObject* vobj);

    // reset all state on the next update
    void reset();

    // pause all updates other than the default probe
    // duration - number of seconds to pause (default 10)
    void pause(F32 duration = 10.f);

    // unpause (see pause)
    void resume();

    // called on region crossing to "shift" probes into new coordinate frame
    void shift(const LLVector4a& offset);

    // debug display, called from llspatialpartition if reflection
    // probe debug display is active
    void renderDebug();

    // call once at startup to allocate cubemap arrays
    void initReflectionMaps();

    // Release native cube/SH resources while retaining logical probes so the
    // next reconciliation can assign fresh slots. Returns whether storage was held.
    bool freeResources();

    // True if currently updating a radiance map, false if currently updating an irradiance map
    bool isRadiancePass() { return mRadiancePass; }

    // perform occlusion culling on all active reflection probes
    void doOcclusion();

    // *HACK: "cull" all reflection probes except the default one. Only call
    // this if you don't intend to call updateUniforms directly. Call again
    // with false when done.
    void forceDefaultProbeAndUpdateUniforms(bool force = true);

#if LL_DARWIN
    // Fill a native preview environment only after a matching radiance cube
    // and SH row from one completed default-probe generation are available.
    bool fillMetalDefaultProbe(ALMetalPBRPreviewEnvironment& environment) const;

    // Fill the live native probe array only when every cube slot referenced
    // by the current selection pack has a Metal generation for its current
    // GL owner. Old generations remain usable while that same probe refreshes;
    // a recycled slot fails closed until its new owner has been mirrored.
    bool fillMetalReflectionProbes(ALMetalWorldReflectionProbes& probes) const;

    // Snapshot the live selection pack for native owners that inspect probe
    // volumes without sampling their radiance or irradiance generations.
    bool fillMetalReflectionProbeConstants(
        ALMetalWorldReflectionProbeConstants& constants) const;
#endif

    U32 probeCount();
    U32 probeMemory();

private:
    friend class LLPipeline;
    friend class LLHeroProbeManager;

    // initialize mCubeFree array to default values
    void initCubeFree();

    // Just does a bulk clear of all of the cubemaps.
    void clearCubeMaps();

    // delete the probe with the given index in mProbes
    void deleteProbe(U32 i);

    // stop generating mUpdatingProbe partway through, so the next probe starts from its first pass
    void abandonProbeUpdate();

    // get a free cube index
    // returns -1 if allocation failed
    S32 allocateCubeIndex();

    // update the neighbors of the given probe
    void updateNeighbors(LLReflectionMap* probe);

    // Refresh reflection-probe constants once per render-pipe flush. Metal
    // consumes the CPU-side block directly.
    void updateUniforms();

    // list of free cubemap indices
    std::list<S32> mCubeFree;

    // perform an update on the currently updating Probe
    void doProbeUpdate();

    // update the specified face of the specified probe
    bool updateProbeFace(
        LLReflectionMap* probe, U32 face,
        bool realtime_capture = false);

    // Face edge length of the native probe mip the SH irradiance projection
    // integrates over, and its mip level.
    U32 shProjectionRes(S32& mip) const;

#if LL_DARWIN
    struct MetalDefaultProbeMirror;
    struct MetalProbeArrayMirror;
    void releaseMetalDefaultProbe();
    bool reconcileMetalProbeArray();
    void invalidateMetalProbeSlot(U32 cube_index);
    void releaseMetalProbeArray();
    void collectNativeMetalProbe();
    bool beginNativeMetalProbeFaceCapture(
        LLReflectionMap* probe, U32 face, bool radiance_pass,
        bool realtime_capture,
        std::string* error = nullptr);
    bool captureNativeMetalProbeFace(
        ALMetalWorldGBufferRenderer& renderer,
        const ALMetalRenderTarget& source,
        LLReflectionMap* probe, U32 face, bool radiance_pass,
        bool realtime_capture,
        std::string* error = nullptr);
    std::unique_ptr<MetalDefaultProbeMirror> mMetalDefaultProbe;
    std::unique_ptr<MetalProbeArrayMirror> mMetalProbeArray;
#endif

    // list of active reflection maps
    std::vector<LLPointer<LLReflectionMap> > mProbes;

    // list of reflection maps to kill
    std::vector<LLPointer<LLReflectionMap> > mKillList;

    // list of reflection maps to create
    std::vector<LLPointer<LLReflectionMap> > mCreateList;

    // list of maps being used for rendering
    std::vector<LLReflectionMap*> mReflectionMaps;

    LLReflectionMap* mUpdatingProbe = nullptr;
    U32 mUpdatingFace = 0;

    // if true, we're generating the radiance map for the current probe, otherwise we're generating the irradiance map.
    // Update sequence should be to generate the irradiance map from render of the world that has no irradiance,
    // then generate the radiance map from a render of the world that includes irradiance.
    // This should avoid feedback loops and ensure that the colors in the radiance maps match the colors in the environment.
    bool mRadiancePass = false;

    // same as above, but for the realtime probe.
    // Realtime probes should update all six sides of the irradiance map on "odd" frames and all six sides of the
    // radiance map on "even" frames.
    bool mRealtimeRadiancePass = false;

    LLPointer<LLReflectionMap> mDefaultProbe;  // default reflection probe to fall back to for pixels with no probe influences (should always be at cube index 0)

    // number of reflection probes to use for rendering
    U32 mReflectionProbeCount;

    U32 mDynamicProbeCount;

    // cached settings from gSavedSettings
    S32 mRenderReflectionProbeDetail = -1;
    S32 mRenderReflectionProbeLevel = 3;
    U32 mRenderReflectionProbeCount = 256U;
    S32 mRenderReflectionProbeDynamicAllocation = -1;

    // resolution of reflection probes
    U32 mProbeResolution = 128;

    // Linear supersample factor of a capture: faces render at mProbeResolution times this and,
    // above 1, are Gaussian blurred and downsampled to the probe. ALProbeSuperSample, which
    // initReflectionMaps takes from the plan with the resolution, as the probe pack is sized.
    U32 mSuperSample = 4;
    U32 superSample() const { return mSuperSample; }

    // HDR mode reconciled into the native reflection resource plan.
    bool mHDR = true;

    // maximum LoD of reflection probes (mip levels - 1)
    F32 mMaxProbeLOD = 6.f;

    // amount to scale local lights during an irradiance map update (set during updateProbeFace and used by LLPipeline)
    F32 mLightScale = 1.f;

    // if true, reset all probe render state on the next update (for teleports and sky changes)
    bool mReset = false;

    float mResetFade = 1.f;

    // if true, only update the default probe
    bool mPaused = false;
    F64 mResumeTime = 0.0;

    ReflectionProbeData mProbeData;
};

