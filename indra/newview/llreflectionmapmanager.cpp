/**
 * @file llreflectionmapmanager.cpp
 * @brief LLReflectionMapManager class implementation
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2022, Linden Research, Inc.
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

#include "llreflectionmapmanager.h"
#include "alprobereadiness.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <fmt/format.h>

#include "llviewercamera.h"
#include "llspatialpartition.h"
#include "llviewerregion.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llenvironment.h"
#include "llstartup.h"
#include "llviewermenufile.h"
#include "llnotificationsutil.h"

#if LL_DARWIN
#include "../almetal/almetalpbrpreview.h"
#include "../almetal/almetalresource.h"
#include "../almetal/almetalstagetiming.h"
#include "../almetal/almetalworld.h"
#include <optional>
#endif

#if LL_WINDOWS
#pragma warning (push)
#pragma warning (disable : 4702) // compiler complains unreachable code
#endif
#define TINYEXR_USE_MINIZ 0
#include <zlib.h>
#include <tinyexr.h>
#if LL_WINDOWS
#pragma warning (pop)
#endif

LLPointer<LLImageGL> gEXRImage;

void load_exr(const std::string& filename)
{
    // reset reflection maps when previewing a new HDRI
    gPipeline.mReflectionMapManager.reset();
    gPipeline.mReflectionMapManager.initReflectionMaps();

    float* out; // width * height * RGBA
    int width;
    int height;
    const char* err = NULL; // or nullptr in C++11

    int ret =  LoadEXRWithLayer(&out, &width, &height, filename.c_str(), /* layername */ nullptr, &err);
    if (ret == TINYEXR_SUCCESS)
    {
        // The native HDRI sky owns this image directly. Convert the decoded
        // RGBA32F values to the authored RGBA16F storage and generate the
        // complete mip family without allocating a parallel GL texture.
        gEXRImage = new LLImageGL(width, height, 4, true);
        gEXRImage->setExplicitFormat(
            GL_RGBA16F, GL_RGBA, GL_FLOAT);
        gEXRImage->setHasMipMaps(true);
        gEXRImage->setUseMipMaps(true);
        const bool uploaded = gEXRImage->createMetalTexture(
            0, reinterpret_cast<const U8*>(out));

        free(out); // release memory of image data
        if (!uploaded)
        {
            LL_WARNS("HDRI")
                << "Could not publish the loaded HDRI Metal texture" << LL_ENDL;
            gEXRImage = nullptr;
        }

    }
    else
    {
        LLSD notif_args;
        notif_args["WHAT"] = filename;
        notif_args["REASON"] = "Unknown";
        if (err)
        {
            notif_args["REASON"] = std::string(err);
            FreeEXRErrorMessage(err); // release memory of error message.
        }
        LLNotificationsUtil::add("CannotLoad", notif_args);
    }
}

void hdri_preview()
{
    LLFilePickerReplyThread::startPicker(
        [](const std::vector<std::string>& filenames, LLFilePicker::ELoadFilter load_filter, LLFilePicker::ESaveFilter save_filter)
        {
            if (LLAppViewer::instance()->quitRequested())
            {
                return;
            }
            if (filenames.size() > 0)
            {
                load_exr(filenames[0]);
            }
        },
        LLFilePicker::FFLOAD_HDRI,
        true);
}

extern bool gCubeSnapshot;
extern bool gSnapshot;
extern bool gSnapshotOffMainView;
extern bool gTeleportDisplay;

static U32 sUpdateCount = 0;

#if !LL_RELEASE_FOR_DOWNLOAD
namespace
{
    // Register the tail of the ReflectionProbes block for debug validation at shader load.
    // Only the scalars are listed: the leading arrays are what fixes their offsets, so a
    // packing surprise anywhere ahead of them shows up here, and these are the members with no
    // other check -- the C++ struct's own size is the only thing pinning _ssrTailPad.
    const bool s_probe_layout_registered = []
    {
#define LL_PROBE_LAYOUT(m) \
        { -1, #m, (U32)offsetof(LLReflectionMapManager::ReflectionProbeData, m), false }
        LLGLSLShader::registerEngineBlockLayout("ReflectionProbes",
        {
            LL_PROBE_LAYOUT(refmapCount),
            LL_PROBE_LAYOUT(heroShape),
            LL_PROBE_LAYOUT(heroMaxLod),
            LL_PROBE_LAYOUT(heroProbeCount),
            LL_PROBE_LAYOUT(iterationCount),
            LL_PROBE_LAYOUT(rayStep),
            LL_PROBE_LAYOUT(distanceBias),
            LL_PROBE_LAYOUT(depthRejectBias),
            LL_PROBE_LAYOUT(glossySampleCount),
            LL_PROBE_LAYOUT(adaptiveStepMultiplier),
        });
#undef LL_PROBE_LAYOUT
        return true;
    }();
}
#endif // !LL_RELEASE_FOR_DOWNLOAD

static void touch_default_probe(LLReflectionMap* probe)
{
    if (LLViewerCamera::getInstance())
    {
        LLVector3 origin = LLViewerCamera::getInstance()->getOrigin();
        origin.mV[2] += 64.f;

        probe->mOrigin.load3(origin.mV);
    }
}

#if LL_DARWIN
struct LLReflectionMapManager::MetalDefaultProbeMirror
{
    std::array<U32, LL_SH_PACKED_TEXELS * 4> mStagedSH{};
    std::array<U32, LL_SH_PACKED_TEXELS * 4> mPublishedSH{};
    bool mStagedSHValid = false;
    U64 mNativeStagedSHGeneration = 0;
    U64 mNativeStagedSHSequence = 0;

    std::unique_ptr<ALMetalTexture> mPublishedRadiance;
    std::unique_ptr<ALMetalTexture> mNativePendingRadiance;
    U64 mNativePendingRadianceGeneration = 0;
    U64 mNativePendingRadianceSequence = 0;

    bool hasResources() const
    {
        return mPublishedRadiance != nullptr ||
               mNativePendingRadiance != nullptr;
    }

    void release(bool keep_published_default = false)
    {
        mStagedSH = {};
        if (!keep_published_default)
        {
            mPublishedSH = {};
            mPublishedRadiance.reset();
        }
        mStagedSHValid = false;
        mNativeStagedSHGeneration = 0;
        mNativeStagedSHSequence = 0;
        mNativePendingRadiance.reset();
        mNativePendingRadianceGeneration = 0;
        mNativePendingRadianceSequence = 0;
    }
};

struct LLReflectionMapManager::MetalProbeArrayMirror
{
    std::vector<U64> mSlotGenerations;
    std::vector<U64> mPublishedGenerations;
    std::vector<U64> mPublishedSHGenerations;
    const ALMetalDevice* mDevice = nullptr;
    std::unique_ptr<ALMetalTexture> mPublishedRadiance;
    std::unique_ptr<ALMetalTexture> mPublishedSH;
    std::unique_ptr<ALMetalTexture> mNativeSourceCube;
    std::unique_ptr<ALMetalTexture> mNativeFilteredCube;
    std::unique_ptr<ALMetalTexture> mNativeStagedSH;
    std::unique_ptr<ALMetalBuffer> mNativePackedSH;
    std::unique_ptr<ALMetalCommandBuffer> mNativeSHPending;
    U32 mCubeCount = 0;
    U64 mNativeNextSequence = 0;
    U32 mNativeCaptureCube = 0;
    U64 mNativeCaptureGeneration = 0;
    U64 mNativeCaptureSequence = 0;
    U32 mNativeValidFaces = 0;
    U32 mNativePendingSHCube = 0;
    U64 mNativePendingSHGeneration = 0;
    U64 mNativePendingSHSequence = 0;
    U32 mNativeReadySHCube = 0;
    U64 mNativeReadySHGeneration = 0;
    U64 mNativeReadySHSequence = 0;
    bool mNativeCaptureRadiance = false;
    bool mNativeCaptureRealtime = false;
    bool mNativeSHPublishedThisPass = false;

    bool hasResources() const
    {
        return mPublishedRadiance != nullptr ||
               mPublishedSH != nullptr ||
               mNativeSourceCube != nullptr ||
               mNativeFilteredCube != nullptr ||
               mNativeStagedSH != nullptr ||
               mNativePackedSH != nullptr ||
               mNativeSHPending != nullptr;
    }

    void release(bool keep_published_default = false)
    {
        if (keep_published_default)
        {
            // Retain only the completed default's logical publication. Every
            // slot gets a fresh epoch: a pending pre-reset SH result or a
            // recycled nondefault slot cannot become current by coincidence.
            for (U64& generation : mSlotGenerations)
            {
                if (++generation == 0) ++generation;
            }
            std::fill(mPublishedGenerations.begin(),
                      mPublishedGenerations.end(), 0);
            std::fill(mPublishedSHGenerations.begin(),
                      mPublishedSHGenerations.end(), 0);
            mPublishedGenerations[0] = mSlotGenerations[0];
            mPublishedSHGenerations[0] = mSlotGenerations[0];
        }
        else
        {
            mSlotGenerations.clear();
            mPublishedGenerations.clear();
            mPublishedSHGenerations.clear();
            mDevice = nullptr;
            mPublishedRadiance.reset();
            mPublishedSH.reset();
            mCubeCount = 0;
            mNativeNextSequence = 0;
        }
        mNativeSourceCube.reset();
        mNativeFilteredCube.reset();
        mNativeStagedSH.reset();
        mNativePackedSH.reset();
        mNativeSHPending.reset();
        mNativeCaptureCube = 0;
        mNativeCaptureGeneration = 0;
        mNativeCaptureSequence = 0;
        mNativeValidFaces = 0;
        mNativePendingSHCube = 0;
        mNativePendingSHGeneration = 0;
        mNativePendingSHSequence = 0;
        mNativeReadySHCube = 0;
        mNativeReadySHGeneration = 0;
        mNativeReadySHSequence = 0;
        mNativeCaptureRadiance = false;
        mNativeCaptureRealtime = false;
        mNativeSHPublishedThisPass = false;
    }
};
#endif

LLReflectionMapManager::LLReflectionMapManager()
{
    mDynamicProbeCount = LL_MAX_REFLECTION_PROBE_COUNT;
    initCubeFree();

#if LL_DARWIN
    mMetalDefaultProbe = std::make_unique<MetalDefaultProbeMirror>();
    mMetalProbeArray = std::make_unique<MetalProbeArrayMirror>();
#endif
}

LLReflectionMapManager::~LLReflectionMapManager() = default;

#if LL_DARWIN
void LLReflectionMapManager::releaseMetalDefaultProbe()
{
    if (mMetalDefaultProbe)
    {
        mMetalDefaultProbe->release();
    }
}

void LLReflectionMapManager::releaseMetalProbeArray()
{
    if (mMetalProbeArray)
    {
        mMetalProbeArray->release();
    }
}

void LLReflectionMapManager::invalidateMetalProbeSlot(U32 cube_index)
{
    MetalProbeArrayMirror& mirror = *mMetalProbeArray;
    if (cube_index < mirror.mSlotGenerations.size())
    {
        ++mirror.mSlotGenerations[cube_index];
        if (mirror.mSlotGenerations[cube_index] == 0)
        {
            ++mirror.mSlotGenerations[cube_index];
        }
    }
}

bool LLReflectionMapManager::reconcileMetalProbeArray()
{
    MetalProbeArrayMirror& mirror = *mMetalProbeArray;
    ALMetalDevice* device = getViewerMetalDevice();
    if (!device)
    {
        return false;
    }

    const U32 cube_count = mReflectionProbeCount + 2;
    if ((mirror.mDevice && mirror.mDevice != device) ||
        mirror.mCubeCount != cube_count ||
        mirror.mSlotGenerations.size() != cube_count)
    {
        // The readback queue and its metadata must be discarded together:
        // retaining one across a cube-layout or device change could attach an
        // old row to a newly assigned probe slot.
        MetalDefaultProbeMirror& default_mirror = *mMetalDefaultProbe;
        default_mirror.mNativePendingRadiance.reset();
        default_mirror.mNativePendingRadianceGeneration = 0;
        default_mirror.mNativePendingRadianceSequence = 0;
        mirror.release();
        mirror.mDevice = device;
        mirror.mCubeCount = cube_count;
        mirror.mSlotGenerations.assign(cube_count, 1);
        mirror.mPublishedGenerations.assign(cube_count, 0);
        mirror.mPublishedSHGenerations.assign(cube_count, 0);
    }

    // The retained GLSL path keeps rendering while the first default probe
    // is incomplete: refmapCount is zero, but probe slot zero remains a
    // legal, black/zero contribution and direct + atmospheric ambient light
    // still reach the framebuffer.  Give Metal the same coherent bootstrap
    // resource pair.  These are deliberately not marked as a published
    // logical generation; the first real twelve-face transaction replaces
    // them atomically below.
    if (!mirror.mPublishedRadiance || !mirror.mPublishedSH)
    {
        if (mProbeResolution == 0 || !llfinite(mMaxProbeLOD) ||
            mMaxProbeLOD < 0.f || cube_count == 0)
        {
            return false;
        }
        const U32 mip_count =
            static_cast<U32>(std::lround(mMaxProbeLOD)) + 1u;

        ALMetalTextureDescriptor radiance_descriptor;
        radiance_descriptor.type = ALMetalTextureType::texture_cube_array;
        radiance_descriptor.format = mHDR
            ? ALMetalPixelFormat::rgba16_float
            : ALMetalPixelFormat::rgba8_unorm;
        radiance_descriptor.width = mProbeResolution;
        radiance_descriptor.height = mProbeResolution;
        radiance_descriptor.mipmap_level_count = mip_count;
        radiance_descriptor.array_length = cube_count;
        radiance_descriptor.storage = ALMetalStorageMode::private_gpu;
        radiance_descriptor.usage = ALMetalTextureUsage::shader_read;
        radiance_descriptor.label =
            "Alchemy warming reflection-probe radiance";
        auto warming_radiance = ALMetalTexture::create(
            *device, radiance_descriptor, nullptr);

        ALMetalTextureDescriptor sh_descriptor;
        sh_descriptor.format = ALMetalPixelFormat::rgba32_uint;
        sh_descriptor.width = LL_SH_PACKED_TEXELS;
        sh_descriptor.height = cube_count;
        sh_descriptor.storage = ALMetalStorageMode::private_gpu;
        sh_descriptor.usage = ALMetalTextureUsage::shader_read;
        sh_descriptor.label = "Alchemy warming reflection-probe SH";
        auto warming_sh = ALMetalTexture::create(
            *device, sh_descriptor, nullptr);

        std::string error;
        auto command = warming_radiance && warming_sh
            ? device->makeCommandBuffer(
                  "Alchemy warming reflection-probe initialization",
                  &error)
            : nullptr;
        bool initialized = command != nullptr;
        const size_t radiance_bytes_per_pixel = warming_radiance
            ? warming_radiance->bytesPerPixel() : 0;
        std::vector<U8> black_face(
            static_cast<size_t>(mProbeResolution) * mProbeResolution *
                radiance_bytes_per_pixel,
            0);
        for (U32 mip = 0; initialized && mip < mip_count; ++mip)
        {
            const U32 width = llmax(1u, mProbeResolution >> mip);
            const size_t bytes_per_row =
                static_cast<size_t>(width) * radiance_bytes_per_pixel;
            for (U32 face = 0; initialized && face < 6; ++face)
            {
                initialized = warming_radiance->upload(
                    *command, mip, face, {}, black_face.data(),
                    bytes_per_row, &error);
            }
        }
        std::vector<U32> zero_sh(
            static_cast<size_t>(LL_SH_PACKED_TEXELS) * cube_count * 4u,
            0u);
        initialized = initialized && warming_sh->upload(
            *command, 0, 0, {}, zero_sh.data(),
            static_cast<size_t>(LL_SH_PACKED_TEXELS) * 4u * sizeof(U32),
            &error) &&
            command->commitAndWait(&error);
        if (!initialized)
        {
            LL_WARNS("Metal")
                << "Could not initialize the zero-probe Metal warm-up pack: "
                << error << LL_ENDL;
            return false;
        }
        mirror.mPublishedRadiance = std::move(warming_radiance);
        mirror.mPublishedSH = std::move(warming_sh);
        std::fill(mirror.mPublishedGenerations.begin(),
                  mirror.mPublishedGenerations.end(), 0);
        std::fill(mirror.mPublishedSHGenerations.begin(),
                  mirror.mPublishedSHGenerations.end(), 0);
    }
    return true;
}

void LLReflectionMapManager::collectNativeMetalProbe()
{
    MetalProbeArrayMirror& mirror = *mMetalProbeArray;
    if (!mirror.mNativeSHPending)
    {
        return;
    }
    const ALMetalCommandStatus status =
        mirror.mNativeSHPending->status();
    if (status != ALMetalCommandStatus::completed &&
        status != ALMetalCommandStatus::error)
    {
        return;
    }

    const U32 pending_cube = mirror.mNativePendingSHCube;
    const U64 pending_generation = mirror.mNativePendingSHGeneration;
    const U64 pending_sequence = mirror.mNativePendingSHSequence;
    const bool generation_valid =
        mirror.mNativePendingSHCube < mirror.mSlotGenerations.size() &&
        mirror.mSlotGenerations[mirror.mNativePendingSHCube] ==
            mirror.mNativePendingSHGeneration;
    bool default_sh_ready = false;
    if (status == ALMetalCommandStatus::completed && generation_valid)
    {
        mirror.mNativeReadySHCube = mirror.mNativePendingSHCube;
        mirror.mNativeReadySHGeneration =
            mirror.mNativePendingSHGeneration;
        mirror.mNativeReadySHSequence = mirror.mNativePendingSHSequence;
        if (mirror.mNativePendingSHCube == 0 && mirror.mNativePackedSH)
        {
            MetalDefaultProbeMirror& default_mirror = *mMetalDefaultProbe;
            if (mirror.mNativePackedSH->read(
                    0, default_mirror.mStagedSH.data(),
                    sizeof(default_mirror.mStagedSH)))
            {
                default_mirror.mStagedSHValid = true;
                default_mirror.mNativeStagedSHGeneration =
                    mirror.mNativePendingSHGeneration;
                default_mirror.mNativeStagedSHSequence =
                    mirror.mNativePendingSHSequence;
                default_sh_ready = true;
            }
        }
    }
    else if (status == ALMetalCommandStatus::error)
    {
        LL_WARNS("Metal")
            << "Could not complete native reflection-probe SH: "
            << mirror.mNativeSHPending->errorDescription() << LL_ENDL;
    }

    if (pending_cube == 0)
    {
        MetalDefaultProbeMirror& default_mirror = *mMetalDefaultProbe;
        const bool matching_deferred_radiance =
            default_mirror.mNativePendingRadiance &&
            default_mirror.mNativePendingRadianceGeneration ==
                pending_generation &&
            default_mirror.mNativePendingRadianceSequence ==
                pending_sequence;
        if (matching_deferred_radiance && default_sh_ready)
        {
            // The radiance copy was submitted after this SH command on the
            // viewer's single Metal queue. Publishing the retained texture
            // now is nonblocking: any later preview draw is ordered after the
            // copy, while the CPU coefficients are already readable and from
            // the same generation and sequence.
            default_mirror.mPublishedRadiance =
                std::move(default_mirror.mNativePendingRadiance);
            default_mirror.mPublishedSH = default_mirror.mStagedSH;
            default_mirror.mStagedSHValid = false;
            default_mirror.mNativeStagedSHGeneration = 0;
            default_mirror.mNativeStagedSHSequence = 0;
            static bool logged_deferred_default = false;
            if (!logged_deferred_default)
            {
                logged_deferred_default = true;
                LL_INFOS("Metal")
                    << "Published native default reflection-probe radiance "
                       "after deferred matching SH completion"
                    << LL_ENDL;
            }
        }
        if (matching_deferred_radiance)
        {
            // Completion without a readable matching row, an errored command
            // or a recycled cube generation can never form a coherent pair.
            default_mirror.mNativePendingRadiance.reset();
            default_mirror.mNativePendingRadianceGeneration = 0;
            default_mirror.mNativePendingRadianceSequence = 0;
        }
    }
    mirror.mNativeSHPending.reset();
    mirror.mNativePendingSHCube = 0;
    mirror.mNativePendingSHGeneration = 0;
    mirror.mNativePendingSHSequence = 0;
}

bool LLReflectionMapManager::beginNativeMetalProbeFaceCapture(
    LLReflectionMap* probe, U32 face, bool radiance_pass,
    bool realtime_capture,
    std::string* error)
{
    if (!probe || probe->mCubeIndex < 0 || face >= 6 ||
        !reconcileMetalProbeArray())
    {
        if (error)
        {
            *error = "Metal reflection-probe capture request is invalid";
        }
        return false;
    }
    MetalProbeArrayMirror& mirror = *mMetalProbeArray;
    const U32 cube = static_cast<U32>(probe->mCubeIndex);
    if (cube >= mirror.mSlotGenerations.size())
    {
        if (error)
        {
            *error = "Metal reflection-probe cube slot is out of range";
        }
        return false;
    }
    const U64 generation = mirror.mSlotGenerations[cube];
    if (face == 0)
    {
        if (!radiance_pass)
        {
            // The scheduled updater advances one face per frame while the
            // realtime updater renders a whole cube in that same frame. They
            // share this scratch transaction, so realtime must not preempt a
            // scheduled twelve-face generation. The scheduled lane may
            // replace a stale/failed sequence when its serialized updater
            // begins the next logical probe.
            if (realtime_capture &&
                mirror.mNativeCaptureSequence != 0 &&
                !mirror.mNativeCaptureRealtime)
            {
                if (error)
                {
                    *error =
                        "Metal scheduled reflection-probe capture owns the native scratch transaction";
                }
                return false;
            }
            ++mirror.mNativeNextSequence;
            if (mirror.mNativeNextSequence == 0)
            {
                ++mirror.mNativeNextSequence;
            }
            mirror.mNativeCaptureSequence =
                mirror.mNativeNextSequence;
        }
        else if (mirror.mNativeCaptureCube != cube ||
                 mirror.mNativeCaptureGeneration != generation ||
                 mirror.mNativeCaptureRadiance ||
                 mirror.mNativeCaptureRealtime != realtime_capture ||
                 mirror.mNativeCaptureSequence == 0 ||
                 mirror.mNativeValidFaces != 0x3fu ||
                 !mirror.mNativeSHPublishedThisPass)
        {
            if (error)
            {
                *error =
                    "Metal reflection-probe radiance pass has no complete matching irradiance/SH capture sequence";
            }
            return false;
        }
        mirror.mNativeCaptureCube = cube;
        mirror.mNativeCaptureGeneration = generation;
        mirror.mNativeCaptureRadiance = radiance_pass;
        mirror.mNativeCaptureRealtime = realtime_capture;
        mirror.mNativeValidFaces = 0;
        if (!radiance_pass)
        {
            mirror.mNativeSHPublishedThisPass = false;
        }
    }
    else if (mirror.mNativeCaptureCube != cube ||
             mirror.mNativeCaptureGeneration != generation ||
             mirror.mNativeCaptureRadiance != radiance_pass ||
             mirror.mNativeCaptureRealtime != realtime_capture ||
             mirror.mNativeCaptureSequence == 0)
    {
        if (error)
        {
            *error =
                "Metal reflection-probe face does not match the active capture sequence";
        }
        return false;
    }

    const U32 preceding_faces = face == 0 ? 0u : ((1u << face) - 1u);
    if (mirror.mNativeValidFaces != preceding_faces)
    {
        if (error)
        {
            *error =
                "Metal reflection-probe capture is missing an earlier face";
        }
        return false;
    }
    return true;
}

bool LLReflectionMapManager::captureNativeMetalProbeFace(
    ALMetalWorldGBufferRenderer& renderer,
    const ALMetalRenderTarget& source,
    LLReflectionMap* probe, U32 face, bool radiance_pass,
    bool realtime_capture,
    std::string* error)
{
    if (!probe || probe->mCubeIndex < 0 || face >= 6 ||
        !reconcileMetalProbeArray())
    {
        if (error)
        {
            *error = "Metal reflection-probe capture request is invalid";
        }
        return false;
    }
    MetalProbeArrayMirror& mirror = *mMetalProbeArray;
    const U32 cube = static_cast<U32>(probe->mCubeIndex);
    if (cube >= mirror.mSlotGenerations.size())
    {
        if (error)
        {
            *error = "Metal reflection-probe cube slot is out of range";
        }
        return false;
    }
    const U64 generation = mirror.mSlotGenerations[cube];
    const U32 preceding_faces = face == 0 ? 0u : ((1u << face) - 1u);
    if (mirror.mNativeCaptureCube != cube ||
        mirror.mNativeCaptureGeneration != generation ||
        mirror.mNativeCaptureRadiance != radiance_pass ||
        mirror.mNativeCaptureRealtime != realtime_capture ||
        mirror.mNativeCaptureSequence == 0 ||
        mirror.mNativeValidFaces != preceding_faces)
    {
        if (error)
        {
            *error =
                "Metal reflection-probe face was not admitted to the active capture sequence";
        }
        return false;
    }

    const U32 max_probe_lod =
        static_cast<U32>(std::lround(mMaxProbeLOD));
    if (!renderer.updateReflectionProbeFace(
            source, face, mProbeResolution, max_probe_lod,
            mirror.mNativeSourceCube, mirror.mNativeValidFaces,
            error))
    {
        return false;
    }
    if (face != 5)
    {
        return true;
    }
    if (mirror.mNativeValidFaces != 0x3fu)
    {
        if (error)
        {
            *error = "Metal reflection-probe capture did not retain all six faces";
        }
        return false;
    }

    ALMetalDevice* device = getViewerMetalDevice();
    if (!device || device != mirror.mDevice)
    {
        if (error)
        {
            *error = "Metal reflection-probe device changed during capture";
        }
        return false;
    }
    if (!radiance_pass)
    {
        const ALMetalTextureDescriptor* current = mirror.mNativeStagedSH
            ? &mirror.mNativeStagedSH->descriptor() : nullptr;
        const bool recreate = !current ||
            current->type != ALMetalTextureType::texture_2d ||
            current->format != ALMetalPixelFormat::rgba32_uint ||
            current->width != LL_SH_PACKED_TEXELS ||
            current->height != 1 ||
            !alMetalHasUsage(current->usage,
                             ALMetalTextureUsage::shader_read) ||
            !alMetalHasUsage(current->usage,
                             ALMetalTextureUsage::shader_write);
        std::unique_ptr<ALMetalTexture> replacement;
        ALMetalTexture* destination = mirror.mNativeStagedSH.get();
        if (recreate)
        {
            ALMetalTextureDescriptor descriptor;
            descriptor.format = ALMetalPixelFormat::rgba32_uint;
            descriptor.width = LL_SH_PACKED_TEXELS;
            descriptor.height = 1;
            descriptor.storage = ALMetalStorageMode::private_gpu;
            descriptor.usage = ALMetalTextureUsage::shader_read |
                               ALMetalTextureUsage::shader_write;
            descriptor.label =
                "Alchemy staged native reflection-probe SH";
            replacement = ALMetalTexture::create(
                *device, descriptor, error);
            destination = replacement.get();
        }
        if (!destination)
        {
            if (error && error->empty())
            {
                *error = "Metal reflection-probe SH destination is unavailable";
            }
            return false;
        }
        S32 sh_mip = 0;
        const U32 sh_resolution = shProjectionRes(sh_mip);
        if (!renderer.projectReflectionProbeSH(
                *mirror.mNativeSourceCube, mirror.mNativeValidFaces,
                static_cast<U32>(sh_mip), sh_resolution,
                *destination, 0, mirror.mNativePackedSH,
                mirror.mNativeSHPending, error))
        {
            return false;
        }
        if (replacement)
        {
            mirror.mNativeStagedSH = std::move(replacement);
        }
        mirror.mNativePendingSHCube = cube;
        mirror.mNativePendingSHGeneration = generation;
        mirror.mNativePendingSHSequence = mirror.mNativeCaptureSequence;
        mirror.mNativeSHPublishedThisPass = true;
        return true;
    }

    // A realtime probe renders all six irradiance faces in one frame and all
    // six radiance faces in the next.  The SH projection committed at the end
    // of the irradiance frame can therefore still be in flight when the next
    // frame begins.  Poll once more after the six radiance faces have been
    // submitted, then rely on the single viewer Metal queue for the remaining
    // dependency: the publication blit below is ordered after the matching SH
    // projection without stalling the CPU for GPU completion.
    collectNativeMetalProbe();
    const bool sh_ready = mirror.mNativeReadySHCube == cube &&
        mirror.mNativeReadySHGeneration == generation &&
        mirror.mNativeReadySHSequence == mirror.mNativeCaptureSequence &&
        mirror.mNativeStagedSH;
    const ALMetalCommandStatus pending_status = mirror.mNativeSHPending
        ? mirror.mNativeSHPending->status()
        : ALMetalCommandStatus::not_enqueued;
    const bool sh_queued = mirror.mNativePendingSHCube == cube &&
        mirror.mNativePendingSHGeneration == generation &&
        mirror.mNativePendingSHSequence == mirror.mNativeCaptureSequence &&
        mirror.mNativeStagedSH &&
        (pending_status == ALMetalCommandStatus::committed ||
         pending_status == ALMetalCommandStatus::scheduled ||
         pending_status == ALMetalCommandStatus::completed);
    if (!sh_ready && !sh_queued)
    {
        if (error)
        {
            *error = llformat(
                "Metal reflection-probe SH is neither ready nor queued "
                "for cube %u generation %llu sequence %llu lane %s; "
                "ready %u/%llu/%llu, pending %u/%llu/%llu status %d, "
                "staged %s",
                cube,
                static_cast<unsigned long long>(generation),
                static_cast<unsigned long long>(
                    mirror.mNativeCaptureSequence),
                realtime_capture ? "realtime" : "scheduled",
                mirror.mNativeReadySHCube,
                static_cast<unsigned long long>(
                    mirror.mNativeReadySHGeneration),
                static_cast<unsigned long long>(
                    mirror.mNativeReadySHSequence),
                mirror.mNativePendingSHCube,
                static_cast<unsigned long long>(
                    mirror.mNativePendingSHGeneration),
                static_cast<unsigned long long>(
                    mirror.mNativePendingSHSequence),
                static_cast<int>(pending_status),
                mirror.mNativeStagedSH ? "yes" : "no");
        }
        return false;
    }

    if (!renderer.filterReflectionProbeRadiance(
            *mirror.mNativeSourceCube, mirror.mNativeValidFaces,
            max_probe_lod, mirror.mNativeFilteredCube, error))
    {
        return false;
    }
    const ALMetalPixelFormat format = mHDR
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rgba8_unorm;
    const U32 mip_count =
        static_cast<U32>(std::lround(mMaxProbeLOD)) + 1u;
    const ALMetalTextureDescriptor* current = mirror.mPublishedRadiance
        ? &mirror.mPublishedRadiance->descriptor() : nullptr;
    const bool recreate = !current ||
        current->type != ALMetalTextureType::texture_cube_array ||
        current->format != format || current->width != mProbeResolution ||
        current->height != mProbeResolution ||
        current->mipmap_level_count != mip_count ||
        current->array_length != mirror.mCubeCount;
    std::unique_ptr<ALMetalTexture> replacement;
    ALMetalTexture* destination = mirror.mPublishedRadiance.get();
    if (recreate)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.type = ALMetalTextureType::texture_cube_array;
        descriptor.format = format;
        descriptor.width = mProbeResolution;
        descriptor.height = mProbeResolution;
        descriptor.mipmap_level_count = mip_count;
        descriptor.array_length = mirror.mCubeCount;
        descriptor.storage = ALMetalStorageMode::private_gpu;
        descriptor.usage = ALMetalTextureUsage::shader_read;
        descriptor.label =
            "Alchemy live native reflection-probe radiance";
        replacement = ALMetalTexture::create(*device, descriptor, error);
        destination = replacement.get();
    }
    if (!destination)
    {
        return false;
    }

    const ALMetalTextureDescriptor* current_sh = mirror.mPublishedSH
        ? &mirror.mPublishedSH->descriptor() : nullptr;
    const bool recreate_sh = !current_sh ||
        current_sh->type != ALMetalTextureType::texture_2d ||
        current_sh->format != ALMetalPixelFormat::rgba32_uint ||
        current_sh->width != LL_SH_PACKED_TEXELS ||
        current_sh->height != mirror.mCubeCount ||
        !alMetalHasUsage(current_sh->usage,
                         ALMetalTextureUsage::shader_read);
    std::unique_ptr<ALMetalTexture> sh_replacement;
    ALMetalTexture* sh_destination = mirror.mPublishedSH.get();
    if (recreate_sh)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.format = ALMetalPixelFormat::rgba32_uint;
        descriptor.width = LL_SH_PACKED_TEXELS;
        descriptor.height = mirror.mCubeCount;
        descriptor.storage = ALMetalStorageMode::private_gpu;
        descriptor.usage = ALMetalTextureUsage::shader_read;
        descriptor.label =
            "Alchemy live native reflection-probe SH";
        sh_replacement = ALMetalTexture::create(
            *device, descriptor, error);
        sh_destination = sh_replacement.get();
    }
    if (!sh_destination)
    {
        return false;
    }

    MetalDefaultProbeMirror& default_mirror = *mMetalDefaultProbe;
    const bool publish_default = cube == 0 &&
        default_mirror.mStagedSHValid &&
        default_mirror.mNativeStagedSHGeneration == generation &&
        default_mirror.mNativeStagedSHSequence ==
            mirror.mNativeCaptureSequence;
    const bool defer_default = cube == 0 && !publish_default;
    std::unique_ptr<ALMetalTexture> default_replacement;
    std::unique_ptr<ALMetalTexture> default_deferred;
    ALMetalTexture* default_destination =
        default_mirror.mPublishedRadiance.get();
    if (publish_default || defer_default)
    {
        const ALMetalTextureDescriptor* default_current =
            publish_default && default_destination
                ? &default_destination->descriptor() : nullptr;
        if (defer_default || !default_current ||
            default_current->type != ALMetalTextureType::texture_cube ||
            default_current->format != format ||
            default_current->width != mProbeResolution ||
            default_current->height != mProbeResolution ||
            default_current->mipmap_level_count != mip_count)
        {
            ALMetalTextureDescriptor descriptor;
            descriptor.type = ALMetalTextureType::texture_cube;
            descriptor.format = format;
            descriptor.width = mProbeResolution;
            descriptor.height = mProbeResolution;
            descriptor.mipmap_level_count = mip_count;
            descriptor.storage = ALMetalStorageMode::private_gpu;
            descriptor.usage = ALMetalTextureUsage::shader_read;
            descriptor.label = defer_default
                ? "Alchemy deferred native default reflection-probe radiance"
                : "Alchemy native default reflection-probe radiance";
            if (defer_default)
            {
                default_deferred = ALMetalTexture::create(
                    *device, descriptor, error);
                default_destination = default_deferred.get();
            }
            else
            {
                default_replacement = ALMetalTexture::create(
                    *device, descriptor, error);
                default_destination = default_replacement.get();
            }
        }
        if (!default_destination)
        {
            return false;
        }
    }

    auto command = device->makeCommandBuffer(
        "Alchemy native reflection-probe array publication", error);
    bool copied = command != nullptr;
    for (U32 mip = 0; copied && mip < mip_count; ++mip)
    {
        for (U32 face_index = 0; copied && face_index < 6; ++face_index)
        {
            ALMetalTextureCopy copy;
            copy.source_mip_level = mip;
            copy.source_slice = face_index;
            copy.destination_mip_level = mip;
            copy.destination_slice = cube * 6 + face_index;
            copied = destination->copyFrom(
                *command, *mirror.mNativeFilteredCube, copy, error);
            if (copied && (publish_default || defer_default))
            {
                copy.destination_slice = face_index;
                copied = default_destination->copyFrom(
                    *command, *mirror.mNativeFilteredCube, copy, error);
            }
        }
    }
    if (copied)
    {
        ALMetalTextureCopy sh_copy;
        sh_copy.source_region = {0, 0, LL_SH_PACKED_TEXELS, 1};
        sh_copy.destination_y = cube;
        copied = sh_destination->copyFrom(
            *command, *mirror.mNativeStagedSH, sh_copy, error);
    }
    copied = copied && command->commit(error);
    if (!copied)
    {
        return false;
    }
    if (replacement)
    {
        mirror.mPublishedRadiance = std::move(replacement);
        std::fill(mirror.mPublishedGenerations.begin(),
                  mirror.mPublishedGenerations.end(), 0);
    }
    if (sh_replacement)
    {
        mirror.mPublishedSH = std::move(sh_replacement);
        std::fill(mirror.mPublishedSHGenerations.begin(),
                  mirror.mPublishedSHGenerations.end(), 0);
    }
    mirror.mPublishedGenerations[cube] = generation;
    mirror.mPublishedSHGenerations[cube] = generation;
    if (publish_default)
    {
        if (default_replacement)
        {
            default_mirror.mPublishedRadiance =
                std::move(default_replacement);
        }
        default_mirror.mPublishedSH = default_mirror.mStagedSH;
        default_mirror.mStagedSHValid = false;
        default_mirror.mNativeStagedSHGeneration = 0;
        default_mirror.mNativeStagedSHSequence = 0;
    }
    else if (defer_default)
    {
        default_mirror.mNativePendingRadiance =
            std::move(default_deferred);
        default_mirror.mNativePendingRadianceGeneration = generation;
        default_mirror.mNativePendingRadianceSequence =
            mirror.mNativeCaptureSequence;
        // The coherent native pair is now retained and will promote when its
        // 64-byte shared SH copy completes.
    }
    mirror.mNativeReadySHCube = 0;
    mirror.mNativeReadySHGeneration = 0;
    mirror.mNativeReadySHSequence = 0;
    // Release the shared scratch transaction only after the coherent
    // radiance + SH pair is ordered on the Metal queue.
    mirror.mNativeCaptureSequence = 0;
    return true;
}

bool LLReflectionMapManager::fillMetalDefaultProbe(
    ALMetalPBRPreviewEnvironment& environment) const
{
    const MetalDefaultProbeMirror& mirror = *mMetalDefaultProbe;
    if (!mirror.mPublishedRadiance || mDefaultProbe.isNull() ||
        mDefaultProbe->mCubeIndex != 0)
    {
        return false;
    }

    environment.radiance_texture = mirror.mPublishedRadiance.get();
    memcpy(environment.sh_coefficients, mirror.mPublishedSH.data(),
           sizeof(environment.sh_coefficients));
    memcpy(environment.probe_origin, mProbeData.refSphere[0].mV,
           sizeof(environment.probe_origin));
    environment.irradiance_scale = mProbeData.refParams[0].mV[0];
    environment.radiance_scale = mProbeData.refParams[0].mV[1];
    environment.max_lod = mMaxProbeLOD;
    return true;
}

bool LLReflectionMapManager::fillMetalReflectionProbes(
    ALMetalWorldReflectionProbes& probes)
{
    if (!reconcileMetalProbeArray())
    {
        return false;
    }
    const MetalProbeArrayMirror& mirror = *mMetalProbeArray;
    const S32 count = mProbeData.refmapCount;
    if (!mirror.mPublishedRadiance || count < 0 ||
        count > static_cast<S32>(
            ALMetalWorldReflectionProbeConstants::max_probe_count))
    {
        LL_INFOS_ONCE("Metal")
            << "Metal reflection-probe pack is warming: radiance="
            << static_cast<bool>(mirror.mPublishedRadiance)
            << ", probe_count=" << count << LL_ENDL;
        return false;
    }
    bool complete_sh = mirror.mPublishedSH != nullptr;
    for (S32 index = 0; index < count; ++index)
    {
        const S32 cube = mProbeData.refIndex[index][0];
        if (cube < 0 ||
            static_cast<size_t>(cube) >=
                mirror.mPublishedGenerations.size() ||
            mirror.mPublishedGenerations[cube] == 0 ||
            mirror.mPublishedGenerations[cube] !=
                mirror.mSlotGenerations[cube])
        {
            const bool cube_in_range = cube >= 0 &&
                static_cast<size_t>(cube) <
                    mirror.mPublishedGenerations.size() &&
                static_cast<size_t>(cube) <
                    mirror.mSlotGenerations.size();
            LL_INFOS_ONCE("Metal")
                << "Metal reflection-probe pack excluded a non-current "
                   "logical probe: logical_index="
                << index << ", cube=" << cube
                << ", in_range=" << cube_in_range
                << ", published_generation="
                << (cube_in_range
                        ? mirror.mPublishedGenerations[cube] : 0)
                << ", slot_generation="
                << (cube_in_range ? mirror.mSlotGenerations[cube] : 0)
                << LL_ENDL;
            return false;
        }
        complete_sh = complete_sh &&
            static_cast<size_t>(cube) <
                mirror.mPublishedSHGenerations.size() &&
            mirror.mPublishedSHGenerations[cube] ==
                mirror.mSlotGenerations[cube];
    }

    if (!complete_sh)
    {
        LL_INFOS_ONCE("Metal")
            << "Metal reflection-probe radiance is current but its SH pack "
               "is still warming for "
            << count << " logical probe(s)" << LL_ENDL;
    }

    probes = {};
    probes.radiance_texture = mirror.mPublishedRadiance.get();
    probes.irradiance_texture = complete_sh
        ? mirror.mPublishedSH.get() : nullptr;
    return fillMetalReflectionProbeConstants(probes.constants);
}

bool LLReflectionMapManager::fillMetalReflectionProbeConstants(
    ALMetalWorldReflectionProbeConstants& constants) const
{
    const S32 count = mProbeData.refmapCount;
    if (count < 0 ||
        count > static_cast<S32>(
            ALMetalWorldReflectionProbeConstants::max_probe_count))
    {
        return false;
    }

    static_assert(sizeof(LLMatrix4) == sizeof(float) * 16);
    constants = {};
    memcpy(constants.ref_box, mProbeData.refBox,
           sizeof(constants.ref_box));
    memcpy(constants.ref_sphere, mProbeData.refSphere,
           sizeof(constants.ref_sphere));
    memcpy(constants.ref_params, mProbeData.refParams,
           sizeof(constants.ref_params));
    memcpy(constants.ref_index, mProbeData.refIndex,
           sizeof(constants.ref_index));
    memcpy(constants.ref_neighbor, mProbeData.refNeighbor,
           sizeof(constants.ref_neighbor));
    memcpy(constants.ref_bucket, mProbeData.refBucket,
           sizeof(constants.ref_bucket));

    // setEnvMat uploads the transpose of the current view rotation. Spell
    // the resulting columns out for MSL's column-major multiply.
    const F32* matrix =
        LLViewerCamera::getCurrent().getModelview().getF32ptr();
    constants.env_matrix[0][0] = matrix[0];
    constants.env_matrix[0][1] = matrix[4];
    constants.env_matrix[0][2] = matrix[8];
    constants.env_matrix[1][0] = matrix[1];
    constants.env_matrix[1][1] = matrix[5];
    constants.env_matrix[1][2] = matrix[9];
    constants.env_matrix[2][0] = matrix[2];
    constants.env_matrix[2][1] = matrix[6];
    constants.env_matrix[2][2] = matrix[10];
    constants.config[0] = static_cast<F32>(count);
    constants.config[1] = mMaxProbeLOD;
    return true;
}
#endif

void LLReflectionMapManager::initCubeFree()
{
    // start at 1 because index 0 is reserved for mDefaultProbe
    for (U32 i = 1; i < mDynamicProbeCount; ++i)
    {
        mCubeFree.push_back(i);
    }
}

// Order probes for the cube-slot handout below: placed probes first, then by distance.
//
// This order decides which probes exist in VRAM at all, so it is where artist intent has to be
// honoured. Distance alone did the opposite: mDistance is (distance to origin - radius), so a
// large influence volume reads as nearer than a small one the camera is equally far inside --
// an automatic probe covering a 16m octree node has a radius of about 13.9, and a manual probe
// on a 2m prim standing in the same spot has 1. The placed probe lost the slot every time.
//
// A probe's radius is not a statement about its importance when the octree chose it and is
// exactly that when a person did, so priority leads and distance orders within each group.
// Automatic probes are a fallback for space nobody placed a probe in; they should be what gives
// when there is not enough room for everything.
struct CompareProbeDistance
{
    bool operator()(const LLPointer<LLReflectionMap>& lhs, const LLPointer<LLReflectionMap>& rhs) const
    {
        if (lhs->mPriority != rhs->mPriority)
        {
            return lhs->mPriority > rhs->mPriority;
        }

        return lhs->mDistance < rhs->mDistance;
    }
};

static F32 update_score(LLReflectionMap* p)
{
    return (F32)(gFrameTimeSeconds - p->mLastUpdateTime) - p->mDistance*0.1f;
}

// return true if a is higher priority for an update than b
static bool check_priority(LLReflectionMap* a, LLReflectionMap* b)
{
    if (a->mCubeIndex == -1)
    { // not a candidate for updating
        return false;
    }
    else if (b->mCubeIndex == -1)
    { // b is not a candidate for updating, a is higher priority by default
        return true;
    }
    else if (!a->mComplete && !b->mComplete)
    { //neither probe is complete, generate the placed one first, then use distance
        if (a->mPriority != b->mPriority)
        {
            return a->mPriority > b->mPriority;
        }
        return a->mDistance < b->mDistance;
    }
    else if (a->mComplete && b->mComplete)
    { //both probes are complete, use update_score metric
        return update_score(a) > update_score(b);
    }

    // a or b is not complete,
    if (sUpdateCount % 3 == 0)
    { // every third update, allow complete probes to cut in line in front of non-complete probes to avoid spammy probe generators from deadlocking scheduler (SL-20258))
        return !b->mComplete;
    }

    // prioritize incomplete probe
    return b->mComplete;
}

// helper class to seed octree with probes
void LLReflectionMapManager::update()
{
#if LL_DARWIN
    // Poll the native SH projection before any eligibility return so a
    // completed coherent generation cannot be stranded when probe updates
    // pause temporarily.
    collectNativeMetalProbe();
#endif

    // Not until the resources are reconciled for probes, which a shader reload that stops short of it leaves undone.
    if (!LLPipeline::sReflectionProbesEnabled || !gPipeline.resourcePlan().reflectionProbes() || gTeleportDisplay ||
        LLStartUp::getStartupState() < STATE_PRECACHE)
    {
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LL_PROFILE_GPU_ZONE("reflection manager update");
    llassert(!gCubeSnapshot); // assert a snapshot is not in progress
    if (LLAppViewer::instance()->logoutRequestSent())
    {
        return;
    }

    if (mPaused && gFrameTimeSeconds > mResumeTime)
    {
        resume();
    }

    mResetFade = llmin((F32)(mResetFade + gFrameIntervalSeconds * 2.f), 1.f);

    {
        U32 probe_count_temp = mDynamicProbeCount;
        if (mRenderReflectionProbeDynamicAllocation > -1)
        {
            if (mRenderReflectionProbeLevel == 0)
            {
                mDynamicProbeCount = 1;
            }
            else if (mRenderReflectionProbeLevel == 1)
            {
                mDynamicProbeCount = (U32)mProbes.size();
            }
            else if (mRenderReflectionProbeLevel == 2)
            {
                mDynamicProbeCount = llmax((U32)mProbes.size(), 128);
            }
            else
            {
                mDynamicProbeCount = 256;
            }

            if (mRenderReflectionProbeDynamicAllocation > 1)
            {
                // Round mDynamicProbeCount to the nearest increment of 16
                mDynamicProbeCount = ((mDynamicProbeCount + mRenderReflectionProbeDynamicAllocation / 2) / mRenderReflectionProbeDynamicAllocation) * 16;
                mDynamicProbeCount = llclamp(mDynamicProbeCount, 1, mRenderReflectionProbeCount);
            }
            else
            {
                mDynamicProbeCount = llclamp(mDynamicProbeCount + mRenderReflectionProbeDynamicAllocation, 1, mRenderReflectionProbeCount);
            }
        }
        else
        {
            mDynamicProbeCount = mRenderReflectionProbeCount;
        }

        mDynamicProbeCount = llmin(mDynamicProbeCount, LL_MAX_REFLECTION_PROBE_COUNT);

        if (mDynamicProbeCount != probe_count_temp)
            mResetFade = 1.f;
    }

    initReflectionMaps();

    llassert(mProbes[0] == mDefaultProbe);

    LLVector4a camera_pos;
    camera_pos.load3(LLViewerCamera::instance().getOrigin().mV);

    // process kill list
    for (auto& probe : mKillList)
    {
        auto const & iter = std::find(mProbes.begin(), mProbes.end(), probe);
        if (iter != mProbes.end())
        {
            deleteProbe((U32)(iter - mProbes.begin()));
        }
    }

    mKillList.clear();

    // process create list
    for (auto& probe : mCreateList)
    {
        mProbes.push_back(probe);
    }

    mCreateList.clear();

    if (mProbes.empty())
    {
        return;
    }


    bool did_update = false;

    bool realtime = mRenderReflectionProbeDetail >= (S32)LLReflectionMapManager::DetailLevel::REALTIME;

    LLReflectionMap* closestDynamic = nullptr;

    LLReflectionMap* oldestProbe = nullptr;
    LLReflectionMap* oldestOccluded = nullptr;

    // Occlusion state only means anything for as long as the queries that produce it keep
    // running, and they stop whenever LLPipeline::sUseOcclusion drops out of its "query"
    // level -- the UseOcclusion setting, a feature-table veto, or simply turning on wireframe.
    // Nothing used to clear mOccluded when that happened, so every probe that was occluded at
    // that instant stayed occluded for the rest of the session: dropped by getReflectionMaps,
    // skipped by the neighbour packer, and unable to recover, because recovering needs a query
    // result and no query would ever run again. The one probe that escaped was whichever one
    // the camera stood inside, since that path answers false before issuing anything.
    if (LLPipeline::sUseOcclusion <= 1)
    {
        for (auto& probe : mProbes)
        {
            probe->clearOcclusion();
        }
    }

    // Make probes track the viewer objects they are attached to, then work out which automatic
    // probes a manual probe has made redundant.
    //
    // An automatic probe is a fallback for space nobody placed a probe in. Where a placed probe
    // has swallowed its whole influence volume it can no longer change a single pixel -- see
    // eclipses() for what "swallowed" has to mean for that to be true of each volume shape --
    // so the cube slot and the place in the update queue it holds are pure waste. Only probes
    // that are actually rendering get to evict anything, or a probe still generating would
    // leave a hole where the one it displaced used to be.
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmu - manual eclipse");

        // Slack, applied in whichever direction preserves the current state: a probe sitting on
        // the boundary of a manual probe that is moving would otherwise flip every frame, and
        // each flip costs it a full twelve-pass regeneration.
        const F32 ECLIPSE_HYSTERESIS = 0.5f;

        std::vector<LLReflectionMap*> manual_probes;

        for (auto& probe : mProbes)
        {
            probe->syncToViewerObject();

            if (probe != mDefaultProbe && probe->mViewerObject && probe->mComplete &&
                probe->mCubeIndex != -1 && probe->mFadeIn >= 1.f && probe->isRelevant())
            {
                manual_probes.push_back(probe);
            }
        }

        for (auto& probe : mProbes)
        {
            if (probe == mDefaultProbe || probe->mViewerObject)
            { // only automatic probes are ever displaced
                continue;
            }

            F32 margin = probe->mInsideManualProbe ? ECLIPSE_HYSTERESIS : -ECLIPSE_HYSTERESIS;

            bool eclipsed = false;
            for (auto* manual : manual_probes)
            {
                if (manual->eclipses(probe, margin))
                {
                    eclipsed = true;
                    break;
                }
            }

            probe->mInsideManualProbe = eclipsed;
        }
    }

    // A probe that stopped being relevant partway through its twelve passes is dropped rather than
    // finished, by the same rule the scheduling loop below uses to skip one. Every remaining pass
    // renders the whole scene for a probe that is about to lose its cube slot in the release pass,
    // or, when whatever registered it is gone, to be deleted outright. It comes after the eclipse
    // pass above so that it judges on this frame's mInsideManualProbe, as the scheduling loop does:
    // judged on last frame's, a probe whose eclipser had just gone would be abandoned here, found
    // relevant again a few lines later, and start its twelve passes over.
    if (mUpdatingProbe != nullptr && mUpdatingProbe != mDefaultProbe && !mUpdatingProbe->isRelevant())
    {
        abandonProbeUpdate();
    }

    if (mUpdatingProbe != nullptr)
    {
        did_update = true;
        doProbeUpdate();
    }

    // Update distance to camera for all probes.
    //
    // This has to happen before the sort below, before the cube slot handout that follows it,
    // and before check_priority's tie-break -- all three consume mDistance. It used to be done
    // inside the scheduling loop further down, one frame after the sort that reads it, and only
    // for probes that loop did not skip: an irrelevant probe, or any probe at all while paused,
    // kept whatever value it last held and sorted on that. A newly created probe holds -1, which
    // is nearer than anything real.
    for (auto& probe : mProbes)
    {
        if (probe == mDefaultProbe)
        {
            // 64m for the purposes of prioritization once it has been generated once, and a
            // hard boost while it has not
            probe->mDistance = probe->mComplete ? 64.f : -4096.f;
            continue;
        }

        if (!probe->isRelevant())
        { // sorts to the very back, so the release pass below reclaims its cube slot -- that
          // pass only ever looks past mReflectionProbeCount, so a probe that stopped mattering
          // while close to the camera used to hold its slot indefinitely
            probe->mDistance = FLT_MAX;
            continue;
        }

        LLVector4a d;
        d.setSub(camera_pos, probe->mOrigin);
        probe->mDistance = d.getLength3().getF32() - probe->mRadius;
    }

    std::sort(mProbes.begin()+1, mProbes.end(), CompareProbeDistance());
    llassert(mProbes[0] == mDefaultProbe);
    llassert(mProbes[0]->mCubeIndex == 0);

    // make sure we're assigning cube slots to the closest probes

    // first free any cube indices for distant probes, and for probes that can no longer affect
    // anything. The second case was unreachable before: relevance had no bearing on the sort, so
    // an irrelevant probe near the camera sat inside the budget and held its slot indefinitely,
    // while still being complete enough for getReflectionMaps to keep handing it to the shader.
    // Index 0 is the default probe and is never released.
    for (U32 i = 1; i < mProbes.size(); ++i)
    {
        LLReflectionMap* probe = mProbes[i];
        llassert(probe != nullptr);

        if (probe && probe->mCubeIndex != -1 && mUpdatingProbe != probe &&
            (i >= mReflectionProbeCount || !probe->isRelevant()))
        { // free this index
#if LL_DARWIN
            invalidateMetalProbeSlot(static_cast<U32>(probe->mCubeIndex));
#endif
            mCubeFree.push_back(probe->mCubeIndex);

            probe->mCubeIndex = -1;
            probe->mComplete = false;
            probe->mFadeIn = 0;
            // Cleared with the rest of the render state. A probe that gets a slot back later
            // would otherwise start out culled by a query taken back when it still had
            // something to show, and need two more occlusion passes to say so -- on top of the
            // twelve it needs to regenerate.
            probe->clearOcclusion();
        }
    }

    // next distribute the free indices
    U32 count = llmin(mReflectionProbeCount, (U32)mProbes.size());

    for (U32 i = 1; i < count && !mCubeFree.empty(); ++i)
    {
        // find the closest probe that needs a cube index
        LLReflectionMap* probe = mProbes[i];

        // Relevance checked here as well as in the release pass above. When there are fewer
        // probes than slots every probe is inside the budget, so without this an irrelevant one
        // would be handed a slot and have it taken back on the next frame, forever.
        if (probe->mCubeIndex == -1 && probe->isRelevant())
        {
            S32 idx = allocateCubeIndex();
            llassert(idx > 0); //if we're still in this loop, mCubeFree should not be empty and allocateCubeIndex should be returning good indices
            probe->mCubeIndex = idx;
        }
    }

    for (unsigned int i = 0; i < mProbes.size(); ++i)
    {
        LLReflectionMap* probe = mProbes[i];
        if (probe->getNumRefs() == 1)
        { // no references held outside manager, delete this probe
            deleteProbe(i);
            --i;
            continue;
        }

        if (probe != mDefaultProbe &&
            (!probe->isRelevant() || mPaused))
        { // skip irrelevant probes (or all non-default probes if paused)
            continue;
        }

        if (probe->mComplete)
        {
            probe->autoAdjustOrigin();
            probe->mFadeIn = llmin((F32) (probe->mFadeIn + gFrameIntervalSeconds), 1.f);

            // Rebuild the neighbour graph once the influence volume has drifted away from the
            // one it was built against.
            //
            // The shader stops its scan at the first probe covering a pixel and then considers
            // only that probe's neighbours, which is sound only while the graph is current: two
            // probes that both contain a point necessarily intersect, so the second is reachable
            // from the first. The graph was only ever rebuilt when a probe finished a full
            // twelve-pass update, so a probe riding a moving object described where it used to
            // be, and a pixel it covered could be skipped outright because whichever probe the
            // scan started from had never heard of it. Rebuilding here also fixes the other
            // side, since the search phase writes into both lists.
            if (probe->neighborsAreStale())
            {
                updateNeighbors(probe);
            }
        }
        if (probe->mOccluded && probe->mComplete)
        {
            if (oldestOccluded == nullptr)
            {
                oldestOccluded = probe;
            }
            else if (probe->mLastUpdateTime < oldestOccluded->mLastUpdateTime)
            {
                oldestOccluded = probe;
            }
        }
        else
        {
            if (!did_update &&
                i < mReflectionProbeCount &&
                (oldestProbe == nullptr ||
                    check_priority(probe, oldestProbe)))
            {
               oldestProbe = probe;
            }
        }

        if (realtime &&
            closestDynamic == nullptr &&
            probe->mCubeIndex != -1 &&
            probe->getIsDynamic())
        {
            closestDynamic = probe;
        }

        if (mRenderReflectionProbeLevel == 0)
        {
            // only update default probe when coverage is set to none
            llassert(probe == mDefaultProbe);
            break;
        }
    }

    if (realtime && closestDynamic != nullptr)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmu - realtime");
        // update the closest dynamic probe realtime
        // should do a full irradiance pass on "odd" frames and a radiance pass on "even" frames
        closestDynamic->autoAdjustOrigin();

        // store and override the value of "isRadiancePass" -- parts of the render pipe rely on "isRadiancePass" to set
        // lighting values etc
        bool radiance_pass = isRadiancePass();
        const bool realtime_radiance_pass = mRealtimeRadiancePass;
        bool native_realtime_pass_complete = true;
        mRadiancePass = realtime_radiance_pass;
        for (U32 i = 0; i < 6; ++i)
        {
            native_realtime_pass_complete =
                updateProbeFace(closestDynamic, i, true) &&
                native_realtime_pass_complete;
        }
        // A native probe pass is transactional. Keep the same half selected
        // when any face fails so the next frame retries it instead of
        // treating the retained GL work as a usable fallback generation.
        if (native_realtime_pass_complete)
        {
            mRealtimeRadiancePass = !mRealtimeRadiancePass;
        }

        // Reserve the following frame for the dependent radiance half when
        // an otherwise-idle realtime irradiance pass has queued native SH.
        // Starting a scheduled face zero below would replace the shared
        // scratch owner before the coherent pair could publish.
        if (!native_realtime_pass_complete ||
            !realtime_radiance_pass)
        {
            did_update = true;
        }

        // restore "isRadiancePass"
        mRadiancePass = radiance_pass;
    }

    static LLCachedControl<F32> sUpdatePeriod(gSavedSettings, "RenderDefaultProbeUpdatePeriod", 2.f);
    if ((gFrameTimeSeconds - mDefaultProbe->mLastUpdateTime) < sUpdatePeriod)
    {
        if (mRenderReflectionProbeLevel == 0)
        { // when probes are disabled don't update the default probe more often than the prescribed update period
            oldestProbe = nullptr;
        }
    }
    else if (mRenderReflectionProbeLevel > 0)
    { // when probes are enabled don't update the default probe less often than the prescribed update period
      oldestProbe = mDefaultProbe;
    }

    // switch to updating the next oldest probe
    if (!did_update && oldestProbe != nullptr)
    {
        LLReflectionMap* probe = oldestProbe;
        llassert(probe->mCubeIndex != -1);

        probe->autoAdjustOrigin();

        sUpdateCount++;
        mUpdatingProbe = probe;
        doProbeUpdate();
    }

    if (oldestOccluded)
    {
        // as far as this occluded probe is concerned, an origin/radius update is as good as a full update
        oldestOccluded->autoAdjustOrigin();
        oldestOccluded->mLastUpdateTime = gFrameTimeSeconds;
    }
}

void LLReflectionMapManager::refreshSettings()
{
    mRenderReflectionProbeDetail = gSavedSettings.getS32("RenderReflectionProbeDetail");
    mRenderReflectionProbeLevel = gSavedSettings.getS32("RenderReflectionProbeLevel");
    mRenderReflectionProbeCount = gSavedSettings.getU32("RenderReflectionProbeCount");
    mRenderReflectionProbeDynamicAllocation = gSavedSettings.getS32("RenderReflectionProbeDynamicAllocation");
}

LLReflectionMap* LLReflectionMapManager::addProbe(LLSpatialGroup* group)
{
    if (!LLPipeline::sReflectionProbesEnabled)
    {
        return nullptr;
    }

    LLReflectionMap* probe = new LLReflectionMap();
    probe->mGroup = group;

    if (mDefaultProbe.isNull())
    {  //safety check to make sure default probe is always first probe added
        mDefaultProbe = new LLReflectionMap();
        mProbes.push_back(mDefaultProbe);
    }

    llassert(mProbes[0] == mDefaultProbe);

    if (group)
    {
        probe->mOrigin = group->getOctreeNode()->getCenter();
    }

    if (gCubeSnapshot)
    { //snapshot is in progress, mProbes is being iterated over, defer insertion until next update
        mCreateList.push_back(probe);
    }
    else
    {
        mProbes.push_back(probe);
    }

    return probe;
}

U32 LLReflectionMapManager::probeCount()
{
    return mDynamicProbeCount;
}

U32 LLReflectionMapManager::probeMemory()
{
    // The SH coefficient strip is 9 x probes x RGBA16F -- tens of kilobytes, below the
    // resolution this reports in.
    return (mDynamicProbeCount * 6 * (mProbeResolution * mProbeResolution) * 4) / 1024 / 1024;
}

struct CompareProbeDepth
{
    bool operator()(const LLReflectionMap* lhs, const LLReflectionMap* rhs)
    {
        return lhs->mMinDepth < rhs->mMinDepth;
    }
};

void LLReflectionMapManager::getReflectionMaps(std::vector<LLReflectionMap*>& maps)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;

    const LLMatrix4a& modelview = LLViewerCamera::getCurrent().getModelview();
    LLVector4a oa; // scratch space for transformed origin

    // Occlusion is measured from the main camera, so it says nothing about what a probe capture
    // rendering from somewhere else can see. llviewerdisplay stops the queries for the duration
    // of a snapshot for exactly that reason -- "don't read or write it during cube snapshots" --
    // but only the write half was ever honoured, and this is the read. Without it every capture
    // omits whatever the player happens not to be looking at, and bakes that into the probe. A
    // snapshot queries none (LLPipeline::doOcclusion) and keeps the main view's: its camera is the
    // main one, or a tile of it, so the probes that view hides are hidden from it too, and none of
    // them takes the slot of one it sees. A 360 capture's faces look elsewhere, and shade with
    // every probe.
    const bool honor_occlusion = !gCubeSnapshot && !gSnapshotOffMainView;

    U32 count = 0;
    U32 lastIdx = 0;
    for (U32 i = 0; count < maps.size() && i < mProbes.size(); ++i)
    {
        // Every probe that does not make the list gets its index cleared, not just the ones
        // without a cube slot. A probe that is occluded or not yet complete used to keep the
        // index it held on some earlier frame, and that slot now belongs to a different probe:
        // the neighbour packer rejects -1 but has no way to tell a stale index from a live one,
        // so a neighbour entry could point at an unrelated probe.
        if (mProbes[i]->mCubeIndex != -1 && mProbes[i]->mComplete &&
            !(honor_occlusion && mProbes[i]->mOccluded))
        {
            maps[count++] = mProbes[i];
            modelview.affineTransform(mProbes[i]->mOrigin, oa);
            mProbes[i]->mMinDepth = -oa.getF32ptr()[2] - mProbes[i]->mRadius;
            mProbes[i]->mMaxDepth = -oa.getF32ptr()[2] + mProbes[i]->mRadius;
        }
        else
        {
            mProbes[i]->mProbeIndex = -1;
        }
        lastIdx = i;
    }

    // set remaining probe indices to -1
    for (U32 i = lastIdx+1; i < mProbes.size(); ++i)
    {
        mProbes[i]->mProbeIndex = -1;
    }

    if (count > 1)
    {
        std::sort(maps.begin(), maps.begin() + count, CompareProbeDepth());
    }

    for (U32 i = 0; i < count; ++i)
    {
        maps[i]->mProbeIndex = i;
    }

    // null terminate list
    if (count < maps.size())
    {
        maps[count] = nullptr;
    }

    // Explicit benchmark evidence only; never retain or mutate a probe.
    if (ALProbeReadiness::observationRequested() &&
        LLPipeline::isMainWorldPass() && !gSnapshot &&
        !gSnapshotOffMainView && gPipeline.mRT == &gPipeline.mMainRT)
    {
        ALProbeReadiness::Snapshot snapshot;
        const auto& planned = gPipeline.resourcePlan().reflectionProbes();
        snapshot.enabled = LLPipeline::sReflectionProbesEnabled && planned.has_value();
        snapshot.budget = mReflectionProbeCount;
        snapshot.dynamic_count = mDynamicProbeCount;
        snapshot.resolution = mProbeResolution;
        snapshot.reset = mReset;
        snapshot.paused = mPaused;
        snapshot.reset_fade_pending = !std::isfinite(mResetFade) || mResetFade < 1.f;
        snapshot.default_ready = mDefaultProbe.notNull() &&
            mDefaultProbe->mCubeIndex == 0 && mDefaultProbe->mComplete;
        snapshot.updating_incomplete = mUpdatingProbe != nullptr && !mUpdatingProbe->mComplete;
        snapshot.refreshing = mUpdatingProbe != nullptr && mUpdatingProbe->mComplete;
        snapshot.radiance_pass = mRadiancePass;
        snapshot.updating_face = mUpdatingFace;
        if (mCreateList.size() > std::numeric_limits<U32>::max() ||
            mKillList.size() > std::numeric_limits<U32>::max()) snapshot.invalid = true;
        else
        {
            snapshot.queued_create = static_cast<U32>(mCreateList.size());
            snapshot.queued_kill = static_cast<U32>(mKillList.size());
        }
        snapshot.layout_ready = snapshot.enabled &&
            snapshot.budget > 0 && snapshot.budget <= ALProbeReadiness::capacity &&
            snapshot.budget == snapshot.dynamic_count &&
            mProbeResolution == planned->mResolution &&
            mSuperSample == planned->mSuperSample && mHDR == planned->mHDR;
        snapshot.native_pairs_required = true;
        const MetalProbeArrayMirror* mirror = mMetalProbeArray.get();
        snapshot.layout_ready = snapshot.layout_ready && mirror && mirror->mDevice &&
            mirror->mCubeCount == snapshot.budget + 2 &&
            mirror->mSlotGenerations.size() == mirror->mCubeCount &&
            mirror->mPublishedGenerations.size() == mirror->mCubeCount &&
            mirror->mPublishedSHGenerations.size() == mirror->mCubeCount;
        std::array<bool, ALProbeReadiness::capacity> visible{};
        snapshot.main_visible = count;
        for (U32 i = 0; i < count; ++i)
        {
            const S32 slot = maps[i]->mCubeIndex;
            if (slot < 0 || static_cast<U32>(slot) >= ALProbeReadiness::capacity) snapshot.invalid = true;
            else
            {
                if (visible[slot]) snapshot.invalid = true;
                visible[slot] = true;
            }
        }
        bool found_default = false;
        for (const auto& probe : mProbes)
        {
            if (probe.isNull()) { snapshot.invalid = true; continue; }
            if (probe->mCubeIndex < 0) continue; // Unassigned probes do not block readiness.
            ++snapshot.assigned;
            if (!probe->mComplete) ++snapshot.incomplete;
            if (!std::isfinite(probe->mFadeIn) || probe->mFadeIn < 1.f) ++snapshot.fading;
            found_default = found_default || probe == mDefaultProbe;
            ALProbeReadiness::Member member;
            member.owner = reinterpret_cast<std::uintptr_t>(probe.get());
            member.slot = probe->mCubeIndex;
            const bool valid_slot = member.slot >= 0 &&
                static_cast<U32>(member.slot) < ALProbeReadiness::capacity;
            if (!valid_slot) snapshot.invalid = true;
            else member.main_visible = visible[member.slot];
            const auto slot = static_cast<std::size_t>(member.slot);
            if (mirror && slot < mirror->mSlotGenerations.size())
                member.generation = mirror->mSlotGenerations[slot];
            if (probe->mComplete &&
                (!mirror || !mirror->mPublishedRadiance || !mirror->mPublishedSH ||
                 member.generation == 0 || slot >= mirror->mPublishedGenerations.size() ||
                 slot >= mirror->mPublishedSHGenerations.size() ||
                 mirror->mPublishedGenerations[slot] != member.generation ||
                 mirror->mPublishedSHGenerations[slot] != member.generation))
                ++snapshot.unpublished_pairs;
            if (snapshot.member_count < ALProbeReadiness::capacity)
                snapshot.members[snapshot.member_count++] = member;
            else snapshot.invalid = true;
        }
        if (!found_default || snapshot.assigned > snapshot.budget) snapshot.invalid = true;
        std::sort(snapshot.members.begin(), snapshot.members.begin() + snapshot.member_count,
            [](const auto& a, const auto& b) { return a.slot < b.slot; });
        for (U32 i = 1; i < snapshot.member_count; ++i)
            if (snapshot.members[i - 1].slot == snapshot.members[i].slot) snapshot.invalid = true;
        ALProbeReadiness::publish(gFrameCount, snapshot);
    }
}

LLReflectionMap* LLReflectionMapManager::registerSpatialGroup(LLSpatialGroup* group)
{
    if (!group)
    {
        return nullptr;
    }
    LLSpatialPartition* part = group->getSpatialPartition();
    if (!part || part->mPartitionType != LLViewerRegion::PARTITION_VOLUME)
    {
        return nullptr;
    }
    OctreeNode* node = group->getOctreeNode();
    F32 size = node->getSize().getF32ptr()[0];
    if (size < 15.f || size > 17.f)
    {
        return nullptr;
    }
    return addProbe(group);
}

LLReflectionMap* LLReflectionMapManager::registerViewerObject(LLViewerObject* vobj)
{
    if (!LLPipeline::sReflectionProbesEnabled)
    {
        return nullptr;
    }

    llassert(vobj != nullptr);

    LLReflectionMap* probe = new LLReflectionMap();
    probe->mViewerObject = vobj;
    // A placed probe, and it is one from the moment it exists. This used to be derived inside
    // autoAdjustOrigin, which is a placement pass and does not run on a probe until it has
    // already won a cube slot -- so a new manual probe carried an automatic probe's priority
    // through the sort that hands slots out, and in a region with none free it could never
    // reach the code that would have told the sort otherwise.
    probe->mPriority = 1;
    probe->mOrigin.load3(vobj->getPositionAgent().mV);

    if (gCubeSnapshot)
    { //snapshot is in progress, mProbes is being iterated over, defer insertion until next update
        mCreateList.push_back(probe);
    }
    else
    {
        mProbes.push_back(probe);
    }

    return probe;
}

S32 LLReflectionMapManager::allocateCubeIndex()
{
    if (!mCubeFree.empty())
    {
        S32 ret = mCubeFree.front();
        mCubeFree.pop_front();
        return ret;
    }

    return -1;
}

void LLReflectionMapManager::deleteProbe(U32 i)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LLReflectionMap* probe = mProbes[i];

    llassert(probe != mDefaultProbe);

    if (probe->mCubeIndex != -1)
    { // mark the cube index used by this probe as being free
#if LL_DARWIN
        invalidateMetalProbeSlot(static_cast<U32>(probe->mCubeIndex));
#endif
        mCubeFree.push_back(probe->mCubeIndex);
    }
    if (mUpdatingProbe == probe)
    {
        abandonProbeUpdate();
    }

    // remove from any Neighbors lists
    for (auto& other : probe->mNeighbors)
    {
        auto const & iter = std::find(other->mNeighbors.begin(), other->mNeighbors.end(), probe);
        llassert(iter != other->mNeighbors.end());
        other->mNeighbors.erase(iter);
    }

    // Probes are distance sorted, order matters.
    mProbes.erase(mProbes.begin() + i);
}

void LLReflectionMapManager::abandonProbeUpdate()
{
    mUpdatingProbe = nullptr;
    mUpdatingFace = 0;

    // The irradiance half runs first and the radiance half is what marks a probe complete. Left
    // set, this would start the next probe on its radiance half: it would skip the irradiance
    // projection altogether and be marked complete on whatever SH coefficients its slot's
    // previous owner left behind.
    mRadiancePass = false;
}


void LLReflectionMapManager::doProbeUpdate()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    llassert(mUpdatingProbe != nullptr);

    // Do not advance the logical transaction unless the native face was
    // published. The GL traversal still enumerates retained scene families
    // during the cutover, but it is no longer completion authority.
    if (!updateProbeFace(mUpdatingProbe, mUpdatingFace))
    {
        return;
    }

    bool debug_updates = gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_PROBE_UPDATES) && mUpdatingProbe->mViewerObject;

    if (++mUpdatingFace == 6)
    {
        if (debug_updates)
        {
            mUpdatingProbe->mViewerObject->setDebugText(llformat("%.1f", (F32)gFrameTimeSeconds), LLColor4(1, 1, 1, 1));
        }
        updateNeighbors(mUpdatingProbe);
        mUpdatingFace = 0;
        if (isRadiancePass())
        {
            mUpdatingProbe->mComplete = true;
            mUpdatingProbe = nullptr;
            mRadiancePass = false;
        }
        else
        {
            mRadiancePass = true;
        }
    }
    else if (debug_updates)
    {
        mUpdatingProbe->mViewerObject->setDebugText(llformat("%.1f", (F32)gFrameTimeSeconds), LLColor4(1, 1, 0, 1));
    }
}

// Publish one face of the native reflection-probe transaction. The first six
// faces project irradiance SH; the next six filter and publish radiance with
// that matching SH generation, preserving the original single-bounce order.
bool LLReflectionMapManager::updateProbeFace(
    LLReflectionMap* probe, U32 face, bool realtime_capture)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LL_PROFILE_GPU_ZONE("probe update");
#if LL_DARWIN
    std::optional<ALMetalStageTimingScope> stage_scope;
    if (const auto* parent = ALMetalStageTimingScope::current();
        parent && parent->window() && parent->frame() == gFrameCount)
        stage_scope.emplace(parent->window(), gFrameCount, 3, static_cast<std::int32_t>(face));
#endif
    // hacky hot-swap of camera specific render targets
    gPipeline.mRT = &gPipeline.mAuxillaryRT;

#if LL_DARWIN
    const bool native_face_capture =
        gPipeline.beginMetalReflectionProbeFaceCapture(
            probe, face, isRadiancePass(), realtime_capture);
    if (!native_face_capture)
    {
        // Native publication is the only probe authority. Warming or missing
        // Metal resources defer this logical face; they do not trigger a
        // hidden GL scene render whose result cannot be published.
        gPipeline.mRT = &gPipeline.mMainRT;
        return false;
    }
    bool native_face_published = false;
#endif

    mLightScale = 1.f;
    static LLCachedControl<F32> max_local_light_ambiance(gSavedSettings, "RenderReflectionProbeMaxLocalLightAmbiance", 8.f);
    if (!isRadiancePass() && probe->getAmbiance() > max_local_light_ambiance)
    {
        mLightScale = max_local_light_ambiance / probe->getAmbiance();
    }

    if (probe == mDefaultProbe)
    {
        touch_default_probe(probe);

        gPipeline.pushRenderTypeMask();

        //only render sky, water, terrain, and clouds
        gPipeline.andRenderTypeMask(LLPipeline::RENDER_TYPE_SKY, LLPipeline::RENDER_TYPE_WL_SKY,
            LLPipeline::RENDER_TYPE_WATER, LLPipeline::RENDER_TYPE_VOIDWATER, LLPipeline::RENDER_TYPE_CLOUDS, LLPipeline::RENDER_TYPE_TERRAIN, LLPipeline::END_RENDER_TYPES);

        probe->update(mProbeResolution * mSuperSample, face);

        gPipeline.popRenderTypeMask();
    }
    else
    {
        llassert(mRenderReflectionProbeLevel > 0); // should never update a probe that's not the default probe if reflection coverage is none
        probe->update(mProbeResolution * mSuperSample, face);
    }

#if LL_DARWIN
    if (native_face_capture)
    {
        native_face_published =
            gPipeline.endMetalReflectionProbeFaceCapture();
    }
#endif

    gPipeline.mRT = &gPipeline.mMainRT;

#if LL_DARWIN
    // The native face producer performs its own mip generation, SH
    // projection, radiance filtering and coherent publication. Do not run
    // the retained GL post-processing path after it: an incomplete native
    // transaction keeps the preceding coherent Metal generation.
    return native_face_published;
#else
    return false;
#endif
}

U32 LLReflectionMapManager::shProjectionRes(S32& mip) const
{
    // Face edge length the SH projection integrates over. Irradiance is band-limited to nine
    // coefficients, so this only has to be fine enough not to alias the source before the
    // integral -- it is not an output resolution and does not bound reconstruction quality.
    // scripts/content_tools/check_sh_projection.py puts the 8x8 default within half a percent
    // of a 128x128 integration; 4x4 is the floor, where that error passes one percent. The
    // cost scales with the texel count, so 32 (the old fixed value) is sixteen times 8.
    static LLCachedControl<S32> projection_res(gSavedSettings, "ALProbeSHProjectionRes", 8);
    const U32 target = llmin((U32)llmax((S32)projection_res, 4), mProbeResolution);

    // The native radiance chain writes mips down to 2x2, so the walk always
    // reaches the requested target (which is clamped to at least 4x4).
    mip = 0;
    U32 res = mProbeResolution;
    while (res > target)
    {
        res >>= 1;
        ++mip;
    }
    return res;
}

void LLReflectionMapManager::reset()
{
    mReset = true;
}

void LLReflectionMapManager::pause(F32 duration)
{
    mPaused = true;
    mResumeTime = gFrameTimeSeconds + duration;
}

void LLReflectionMapManager::resume()
{
    mPaused = false;
}

void LLReflectionMapManager::shift(const LLVector4a& offset)
{
    for (auto& probe : mProbes)
    {
        probe->mOrigin.add(offset);

        // Carried along, because a rigid translation of every probe at once leaves the neighbour
        // graph describing exactly the same geometry. Without this a region crossing would mark
        // all of them stale on the same frame and rebuild the whole graph against itself.
        if (probe->mNeighborRadius >= 0.f)
        {
            probe->mNeighborOrigin.add(offset);
        }
    }
}

void LLReflectionMapManager::updateNeighbors(LLReflectionMap* probe)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;

    // Recorded whether or not there is a list to build. The default probe never has neighbours,
    // and without this it would report itself stale on every frame forever.
    probe->mNeighborOrigin = probe->mOrigin;
    probe->mNeighborRadius = probe->mRadius;

    if (mDefaultProbe == probe)
    {
        return;
    }

    //remove from existing neighbors
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmun - clear");

        for (auto& other : probe->mNeighbors)
        {
            auto const & iter = std::find(other->mNeighbors.begin(), other->mNeighbors.end(), probe);
            llassert(iter != other->mNeighbors.end()); // <--- bug davep if this ever happens, something broke badly
            other->mNeighbors.erase(iter);
        }

        probe->mNeighbors.clear();
    }

    // search for new neighbors
    if (probe->isRelevant())
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmun - search");
        for (auto& other : mProbes)
        {
            if (other != mDefaultProbe && other != probe)
            {
                if (other->isRelevant() && probe->intersects(other))
                {
                    probe->mNeighbors.push_back(other);
                    other->mNeighbors.push_back(probe);
                }
            }
        }
    }
}

void LLReflectionMapManager::updateUniforms()
{
    if (!LLPipeline::sReflectionProbesEnabled)
    {
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LL_PROFILE_GPU_ZONE("rmmu - uniforms")


    // Have active manual probes live-track the object they're associated with, BEFORE anything
    // derives from their origin or radius.
    //
    // getReflectionMaps takes mMinDepth/mMaxDepth from these values and sorts the array on
    // mMinDepth, and refBucket -- which is the shader's lower bound for its scan -- is only
    // valid while that sort holds. Refreshing afterwards, as this used to, described where the
    // probe was in the bucket and uploaded where it is in refSphere, and a probe whose bucket
    // entry no longer covers it is one the shader scans straight past. Manual probes are the
    // only ones that get this refresh, so they were the only ones that could vanish from it.
    for (auto& probe : mProbes)
    {
        probe->syncToViewerObject();
    }

    mReflectionMaps.resize(mReflectionProbeCount);
    getReflectionMaps(mReflectionMaps);

    F32 minDepth[256];

    for (int i = 0; i < 256; ++i)
    {
        mProbeData.refBucket[i][0] = mReflectionProbeCount;
        mProbeData.refBucket[i][1] = mReflectionProbeCount;
        mProbeData.refBucket[i][2] = mReflectionProbeCount;
        mProbeData.refBucket[i][3] = mReflectionProbeCount;
        minDepth[i] = FLT_MAX;
    }

    // load modelview matrix into matrix 4a
    const LLMatrix4a& modelview = LLViewerCamera::getCurrent().getModelview();
    LLVector4a oa; // scratch space for transformed origin

    S32 count = 0;
    U32 nc = 0; // neighbor "cursor" - index into refNeighbor to start writing the next probe's list of neighbors

    LLEnvironment& environment = LLEnvironment::instance();
    LLSettingsSky::ptr_t psky = environment.getCurrentSky();

    static LLCachedControl<bool> should_auto_adjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
    F32 minimum_ambiance = psky->getReflectionProbeAmbiance(should_auto_adjust);

    bool is_ambiance_pass = gCubeSnapshot && !isRadiancePass();
    F32 ambscale = is_ambiance_pass ? 0.f : 1.f;
    ambscale *= mResetFade;
    ambscale = llmax(0, ambscale);
    F32 radscale = is_ambiance_pass ? 0.5f : 1.f;
    radscale *= mResetFade;
    radscale = llmax(0, radscale);

    for (auto* refmap : mReflectionMaps)
    {
        if (refmap == nullptr)
        {
            break;
        }

        if (refmap != mDefaultProbe)
        {
            // bucket search data
            // theory of operation:
            //      1. Determine minimum and maximum depth of each influence volume and store in mDepth (done in getReflectionMaps)
            //      2. Sort by minimum depth
            //      3. Prepare a bucket for each 1m of depth out to 256m
            //      4. For each bucket, store the index of the nearest probe that might influence pixels in that bucket
            //      5. In the shader, lookup the bucket for the pixel depth to get the index of the first probe that could possibly influence
            //          the current pixel.
            unsigned int depth_min = llclamp(llfloor(refmap->mMinDepth), 0, 255);
            unsigned int depth_max = llclamp(llfloor(refmap->mMaxDepth), 0, 255);
            for (U32 i = depth_min; i <= depth_max; ++i)
            {
                if (refmap->mMinDepth < minDepth[i])
                {
                    minDepth[i] = refmap->mMinDepth;
                    mProbeData.refBucket[i][0] = refmap->mProbeIndex;
                }
            }
        }

        llassert(refmap->mProbeIndex == count);
        llassert(mReflectionMaps[refmap->mProbeIndex] == refmap);

        llassert(refmap->mCubeIndex >= 0); // should always be  true, if not, getReflectionMaps is bugged

        modelview.affineTransform(refmap->mOrigin, oa);
        mProbeData.refSphere[count].set(oa.getF32ptr());
        mProbeData.refSphere[count].mV[3] = refmap->mRadius;

        mProbeData.refIndex[count][0] = refmap->mCubeIndex;
        llassert(nc % 4 == 0);
        mProbeData.refIndex[count][1] = nc / 4;
        mProbeData.refIndex[count][3] = refmap->mPriority;

        // for objects that are reflection probes, use the volume as the influence volume of the probe
        // only possibile influence volumes are boxes and spheres, so detect boxes and treat everything else as spheres
        if (refmap->getBox(mProbeData.refBox[count]))
        { // negate priority to indicate this probe has a box influence volume
            // Negated from at least 1, because -0 and 0 are the same integer: a box probe that
            // reached here with priority 0 would be read back as an automatic SPHERE, losing
            // both its shape and its precedence. A box influence volume only exists on a probe
            // attached to a viewer object, which is a manual probe by definition.
            mProbeData.refIndex[count][3] = -llmax(mProbeData.refIndex[count][3], 1);
        }

        mProbeData.refParams[count].set(
            llmax(minimum_ambiance, refmap->getAmbiance())*ambscale, // ambiance scale
            radscale, // radiance scale
            refmap->mFadeIn, // fade in weight
            refmap->mMinDepth); // the sort key; the shader's scan stops past the pixel's depth

        S32 ni = nc; // neighbor ("index") - index into refNeighbor to write indices for current reflection probe's neighbors
        {
            //LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmsu - refNeighbors");
            //pack neghbor list
            const U32 max_neighbors = 64;
            U32 neighbor_count = 0;

            for (auto& neighbor : refmap->mNeighbors)
            {
                if (ni >= 4096)
                { // out of space
                    break;
                }

                // mProbeIndex alone decides this. getReflectionMaps clears it on every probe it
                // leaves out, so it already carries occlusion, completeness and the cube slot,
                // and it is the only one of those that agrees with the list the shader will
                // actually index. Re-testing mOccluded here would disagree with it during a
                // cube snapshot, where occlusion measured from the main camera is ignored:
                // the probe would be in the scan list and unreachable as a neighbour.
                GLint idx = neighbor->mProbeIndex;
                if (idx == -1)
                {
                    continue;
                }

                // this neighbor may be sampled
                mProbeData.refNeighbor[ni++] = idx;

                neighbor_count++;
                if (neighbor_count >= max_neighbors)
                {
                    break;
                }
            }
        }

        if (nc == ni)
        {
            //no neighbors, tag as empty
            mProbeData.refIndex[count][1] = -1;
        }
        else
        {
            mProbeData.refIndex[count][2] = ni - nc;

            // move the cursor forward
            nc = ni;
            if (nc % 4 != 0)
            { // jump to next power of 4 for compatibility with ivec4
                nc += 4 - (nc % 4);
            }
        }


        count++;
    }

#if 0
    {
        // fill in gaps in refBucket
        S32 probe_idx = mReflectionProbeCount;

        for (int i = 0; i < 256; ++i)
        {
            if (i < count)
            { // for debugging, store depth of mReflectionsMaps[i]
                rpd.refBucket[i][1] = (S32) (mReflectionMaps[i]->mDepth * 10);
            }

            if (rpd.refBucket[i][0] == mReflectionProbeCount)
            {
                rpd.refBucket[i][0] = probe_idx;
            }
            else
            {
                probe_idx = rpd.refBucket[i][0];
            }
        }
    }
#endif

    mProbeData.refmapCount = count;

    gPipeline.mHeroProbeManager.updateUniforms();

    // Get the hero data.

    mProbeData.heroBox = gPipeline.mHeroProbeManager.mHeroData.heroBox;
    mProbeData.heroSphere = gPipeline.mHeroProbeManager.mHeroData.heroSphere;
    mProbeData.heroShape  = gPipeline.mHeroProbeManager.mHeroData.heroShape;
    mProbeData.heroMaxLod     = gPipeline.mHeroProbeManager.mHeroData.heroMaxLod;
    mProbeData.heroProbeCount = gPipeline.mHeroProbeManager.mHeroData.heroProbeCount;

    // SSR march parameters ride along in this block rather than being re-pushed loose on every
    // bindReflectionProbes; they are constant for the frame.
    mProbeData.iterationCount         = (F32)LLPipeline::RenderScreenSpaceReflectionIterations;
    mProbeData.rayStep                = LLPipeline::RenderScreenSpaceReflectionRayStep;
    mProbeData.distanceBias           = LLPipeline::RenderScreenSpaceReflectionDistanceBias;
    mProbeData.depthRejectBias        = LLPipeline::RenderScreenSpaceReflectionDepthRejectBias;
    mProbeData.glossySampleCount      = (F32)LLPipeline::RenderScreenSpaceReflectionGlossySamples;
    mProbeData.adaptiveStepMultiplier = LLPipeline::RenderScreenSpaceReflectionAdaptiveStepMultiplier;

#if 0
    if (!gCubeSnapshot)
    {
        for (auto& probe : mProbes)
        {
            LLViewerObject* vobj = probe->mViewerObject;
            if (vobj)
            {
                F32 time = (F32)(gFrameTimeSeconds - probe->mLastUpdateTime);
                vobj->setDebugText(llformat("%d/%d/%d/%.1f - %.1f/%.1f", probe->mCubeIndex, probe->mProbeIndex, (U32) probe->mNeighbors.size(), probe->mMinDepth, probe->mMaxDepth, time), time > 1.f ? LLColor4::white : LLColor4::green);
            }
        }
    }
#endif
}

void renderReflectionProbe(LLReflectionMap* probe)
{
    if (probe->isRelevant())
    {
        for (auto& neighbor : probe->mNeighbors)
        {
            if (probe->mViewerObject && neighbor->mViewerObject)
            {
                continue;
            }
            gPipeline.captureMetalReflectionProbeLink(
                probe->mOrigin, neighbor->mOrigin,
                LLColor4(1.f, 0.5f, 0.f, 1.f));
        }
        for (auto& neighbor : probe->mNeighbors)
        {
            if (probe->mViewerObject && neighbor->mViewerObject)
            {
                gPipeline.captureMetalReflectionProbeLink(
                    probe->mOrigin, neighbor->mOrigin,
                    LLColor4(1.f, 1.f, 0.f, 1.f));
            }
        }
    }
}

void LLReflectionMapManager::renderDebug()
{
    std::size_t link_count = 0;
    for (const LLPointer<LLReflectionMap>& probe : mProbes)
    {
        if (!probe->isRelevant())
        {
            continue;
        }
        if (probe->mNeighbors.size() >
            std::numeric_limits<std::size_t>::max() - link_count)
        {
            link_count = std::numeric_limits<std::size_t>::max();
            break;
        }
        link_count += probe->mNeighbors.size();
    }
    gPipeline.beginMetalReflectionProbeLinkCapture(link_count);

    for (auto& probe : mProbes)
    {
        renderReflectionProbe(probe);
    }

    gPipeline.endMetalReflectionProbeLinkCapture(link_count);
}

void LLReflectionMapManager::initReflectionMaps()
{
    // What the probe pack was planned for, which a face renders into at the probe resolution
    // times the supersample (ALRenderResourcePlan::reflectionProbes): the manager builds what it
    // reads from the same inputs, so the two cannot disagree. Nothing while probes are off.
    const std::optional<ALRenderResourcePlan::ProbeManager>& planned = gPipeline.resourcePlan().reflectionProbes();
    if (!planned)
    {
        return;
    }
    const U32 probe_resolution = planned->mResolution;

    // The supersample sizes the native face capture. It remains part of the
    // resource plan even though the retired GL downsample target is gone.
    if (planned->mSuperSample != mSuperSample)
    {
        mSuperSample = planned->mSuperSample;
    }

    // HDR picks the colour format of the authoritative native radiance array.
    const bool hdr = planned->mHDR;
    const bool hdr_changed = hdr != mHDR;
    if (hdr_changed)
    {
        mHDR = hdr;
    }

    const bool layout_changed =
        mReflectionProbeCount != mDynamicProbeCount ||
        mProbeResolution != probe_resolution || hdr_changed;
    if (layout_changed || mReset)
    {
        gEXRImage = nullptr;
        mReset = false;
        mReflectionProbeCount = mDynamicProbeCount;
        mProbeResolution = probe_resolution;
        mMaxProbeLOD = log2f((F32)mProbeResolution) - 1.f; // number of mips - 1

        // Original initReflectionMaps keeps the completed default on a
        // same-layout sky reset (SL-20498). Bridge the new twelve-face update
        // with that coherent pair rather than zero-probe bootstrap lighting.
        bool default_complete = false;
#if LL_DARWIN
        MetalProbeArrayMirror& mirror = *mMetalProbeArray;
        MetalDefaultProbeMirror& default_mirror = *mMetalDefaultProbe;
        const U32 cube_count = mReflectionProbeCount + 2;
        const U32 mip_count = llfinite(mMaxProbeLOD) && mMaxProbeLOD >= 0.f
            ? static_cast<U32>(std::lround(mMaxProbeLOD)) + 1u : 0u;
        const ALMetalPixelFormat format = mHDR
            ? ALMetalPixelFormat::rgba16_float
            : ALMetalPixelFormat::rgba8_unorm;
        const auto radiance_matches = [&](const ALMetalTexture* texture,
                                          ALMetalTextureType type,
                                          U32 count)
        {
            if (!texture) return false;
            const auto& d = texture->descriptor();
            return d.type == type && d.format == format &&
                d.width == mProbeResolution && d.height == mProbeResolution &&
                d.mipmap_level_count == mip_count && d.array_length == count &&
                alMetalHasUsage(d.usage, ALMetalTextureUsage::shader_read);
        };
        const auto* sh = mirror.mPublishedSH
            ? &mirror.mPublishedSH->descriptor() : nullptr;
        default_complete = !layout_changed && mDefaultProbe.notNull() &&
            mDefaultProbe->mComplete && mDefaultProbe->mCubeIndex == 0 &&
            mirror.mDevice == getViewerMetalDevice() && mirror.mDevice &&
            mirror.mCubeCount == cube_count && mip_count != 0 &&
            mirror.mSlotGenerations.size() == cube_count &&
            mirror.mPublishedGenerations.size() == cube_count &&
            mirror.mPublishedSHGenerations.size() == cube_count &&
            mirror.mSlotGenerations[0] != 0 &&
            mirror.mPublishedGenerations[0] == mirror.mSlotGenerations[0] &&
            mirror.mPublishedSHGenerations[0] == mirror.mSlotGenerations[0] &&
            radiance_matches(mirror.mPublishedRadiance.get(),
                ALMetalTextureType::texture_cube_array, cube_count) &&
            radiance_matches(default_mirror.mPublishedRadiance.get(),
                ALMetalTextureType::texture_cube, 1) &&
            sh && sh->type == ALMetalTextureType::texture_2d &&
            sh->format == ALMetalPixelFormat::rgba32_uint &&
            sh->width == LL_SH_PACKED_TEXELS && sh->height == cube_count &&
            alMetalHasUsage(sh->usage, ALMetalTextureUsage::shader_read);

        // Scratch commands keep their resources through native completion.
        // Their CPU readback/publication tickets are discarded here, while
        // fresh epochs reject every nondefault row until it is rebuilt.
        default_mirror.release(default_complete);
        mirror.release(default_complete);
        static U32 reported_resets = 0;
        if (reported_resets < 32u)
        {
            ++reported_resets;
            LL_INFOS("Metal")
                << "Native default-probe reset policy: layout_changed="
                << layout_changed << ", kept_completed_default="
                << default_complete << ", resolution=" << mProbeResolution
                << ", hdr=" << mHDR << LL_ENDL;
        }
#endif

        // reset probe state
        mUpdatingFace = 0;
        mUpdatingProbe = nullptr;
        mRadiancePass = false;
        mRealtimeRadiancePass = false;

        for (auto& probe : mProbes)
        {
            probe->mLastUpdateTime = 0.f;
            probe->mComplete = false;
            probe->mProbeIndex = -1;
            probe->mCubeIndex = -1;
            probe->mNeighbors.clear();
            probe->mNeighborRadius = -1.f; // list cleared, so it describes nothing
            probe->mFadeIn = 0;
            // Both of these decide whether a probe is allowed to render, so a reset that left
            // them standing could not clear a probe that was stuck on either -- and this is the
            // path a teleport, a sky change or a probe-count change goes through, which is
            // exactly where someone would expect to get their probes back.
            probe->clearOcclusion();
            probe->mInsideManualProbe = false;
        }

        mCubeFree.clear();
        initCubeFree();

        if (mDefaultProbe.isNull())
        {
            llassert(mProbes.empty()); // default probe MUST be the first probe created
            mDefaultProbe = new LLReflectionMap();
            mProbes.push_back(mDefaultProbe);
        }

        llassert(mProbes[0] == mDefaultProbe);

        mDefaultProbe->mCubeIndex = 0;
        mDefaultProbe->mDistance = 64.f;
        mDefaultProbe->mRadius = 4096.f;
        mDefaultProbe->mProbeIndex = 0;
        mDefaultProbe->mComplete = default_complete;

        touch_default_probe(mDefaultProbe);
    }

}

bool LLReflectionMapManager::freeResources()
{
    bool held = false;
#if LL_DARWIN
    held = (mMetalDefaultProbe && mMetalDefaultProbe->hasResources()) ||
           (mMetalProbeArray && mMetalProbeArray->hasResources());
#endif

    abandonProbeUpdate();

    // Probes own logical slots only; the manager owns both backend resources.
    for (auto& probe : mProbes)
    {
        probe->mCubeIndex  = -1;
        probe->mProbeIndex = -1;
        probe->mComplete   = false;
    }
    mCubeFree.clear();
    initCubeFree();

#if LL_DARWIN
    releaseMetalDefaultProbe();
    releaseMetalProbeArray();
#endif

    return held;
}

void LLReflectionMapManager::cleanup()
{
#if LL_DARWIN
    releaseMetalDefaultProbe();
    releaseMetalProbeArray();
#endif

    mProbes.clear();
    mKillList.clear();
    mCreateList.clear();

    mReflectionMaps.clear();
    abandonProbeUpdate();

    mDefaultProbe = nullptr;

    // note: also called on teleport (not just shutdown), so make sure we're in a good "starting" state
    initCubeFree();
}

void LLReflectionMapManager::doOcclusion()
{
    LLVector4a eye;
    eye.load3(LLViewerCamera::instance().getOrigin().mV);

    for (auto& probe : mProbes)
    {
        if (probe.isNull() || probe == mDefaultProbe)
        {
            continue;
        }

        // A probe holding no cube data, or one relevance has switched off, cannot reach the
        // frame whatever a query says about it -- and each query costs a box draw, so this was
        // up to a couple of hundred of them per frame spent on probes with nothing to show.
        // Cleared rather than left to go stale: a probe that becomes eligible again should be
        // culled by a query taken while it had something to show, not by one taken before.
        if (probe->mCubeIndex == -1 || !probe->isRelevant())
        {
            probe->clearOcclusion();
            continue;
        }

        probe->doOcclusion(eye);
    }
}

void LLReflectionMapManager::forceDefaultProbeAndUpdateUniforms(bool force)
{
    // Saved against the probe itself rather than against its position in mProbes. The list can
    // gain or lose entries between the two calls -- the create and kill lists are drained in
    // update(), and a probe can be dropped the moment nothing outside the manager holds it --
    // and an index-keyed restore then hands one probe's occlusion state to another. The
    // LLPointer also keeps every saved probe alive until the restore.
    static std::vector<std::pair<LLPointer<LLReflectionMap>, bool> > sSavedOcclusion;

    if (force)
    {
        llassert(sSavedOcclusion.empty());

        for (auto& probe : mProbes)
        {
            sSavedOcclusion.emplace_back(probe, probe->mOccluded);
            if (probe != mDefaultProbe)
            {
                probe->mOccluded = true;
            }
        }

        updateUniforms();
    }
    else
    {
        for (auto& saved : sSavedOcclusion)
        {
            saved.first->mOccluded = saved.second;
        }
        sSavedOcclusion.clear();
        sSavedOcclusion.shrink_to_fit();
    }
}
