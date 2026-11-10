/**
 * @file llviewerdisplay.cpp
 * @brief LLViewerDisplay class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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

#include "llviewerdisplay.h"

#include "alfarplane.h"
#include "alfloaterprogressview.h"
#include "alframeend.h"
#include "alhuddepth.h"
#include "almetalbenchmark.h"
#include "alprobereadiness.h"
#if LL_DARWIN
#include "../almetal/almetaldevice.h"
#include "../almetal/almetalframecapture.h"
#include "../almetal/almetalstagetiming.h"
#endif
#include "aluniformbuffer.h"
#include "fsyspath.h"
#include "hexdump.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h"
#include "llcoord.h"
#include "llcriticaldamp.h"
#include "llcubemap.h"
#include "lldir.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolbump.h"
#include "lldynamictexture.h"
#include "llenvironment.h"
#include "llfeaturemanager.h"
#include "llfloaterreg.h"
#include "llfloatertools.h"
#include "llfocusmgr.h"
#include "llgl.h"
#include "llglheaders.h"
#include "llgltfmateriallist.h"
#include "llhudmanager.h"
#include "llimagepng.h"
#include "llmachineid.h"
#include "llmemory.h"
#include "llparcel.h"
#include "llperfstats.h"
#include "llrender.h"
#include "llscenemonitor.h"
#include "llsdjson.h"
#include "llselectmgr.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llstartup.h"
#include "llstartup.h"
#include "lltooldraganddrop.h"
#include "lltoolfocus.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "lltracker.h"
#include "lltrans.h"
#include "llui.h"
#include "lluuid.h"
#include "llversioninfo.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexture.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvograss.h"
#include "llworld.h"
#include "pipeline.h"

#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
// [RLVa:KB] - Checked: 2011-05-22 (RLVa-1.3.1a)
#include "llvisualeffect.h"
#include "rlvactions.h"
#include "rlvlocks.h"
// [/RLVa:KB]


extern LLPointer<LLViewerTexture> gStartTexture;
extern bool gShiftFrame;

LLPointer<LLViewerTexture> gDisconnectedImagep = nullptr;

// used to toggle renderer back on after teleport
bool         gTeleportDisplay = false;
LLFrameTimer gTeleportDisplayTimer;
LLFrameTimer gTeleportArrivalTimer;
bool gForceRenderLandFence = false;
bool gDisplaySwapBuffers = false;
bool gDepthDirty = false;
bool gSnapshot = false;
bool gSnapshotOffMainView = false;
bool gCubeSnapshot = false;
bool gSnapshotNoPost = false;
bool gShaderProfileFrame = false;

// This is how long the sim will try to teleport you before giving up.
constexpr F32 TELEPORT_EXPIRY = 15.0f;
// Additional time (in seconds) to wait per attachment
constexpr F32 TELEPORT_EXPIRY_PER_ATTACHMENT = 3.f;

U32 gRecentFrameCount = 0; // number of 'recent' frames
LLFrameTimer gRecentFPSTime;
LLFrameTimer gRecentMemoryTime;
LLFrameTimer gRecentVOCacheTime;
LLFrameTimer gAssetStorageLogTime;

// Rendering stuff
void render_ui(F32 zoom_factor = 1.f, int subfield = 0);
void swap();
void render_hud_attachments();
void render_ui_3d();
void render_ui_2d();
void render_disconnected_background();

void getProfileStatsContext(LLSD& stats);
std::string getProfileStatsFilename();

void display_startup()
{
    if (   !gViewerWindow
        || !gViewerWindow->getActive()
        || !gViewerWindow->getWindow()->getVisible()
        || gViewerWindow->getWindow()->getMinimized()
        || gNonInteractive)
    {
        return;
    }

    gPipeline.updateGL();

    // Written as branch to appease GCC which doesn't like different
    // pointer types across ternary ops
    //
    if (!LLViewerFetchedTexture::sWhiteImagep.isNull())
    {
        ALTextureSlot::setWhiteImage(LLViewerFetchedTexture::sWhiteImagep->getGLTexture());
    }

    LLGLSDefault gls_default;

    // Required for HTML update in login screen
    static S32 frame_count = 0;

    LLGLState::checkStates();

#if LL_DARWIN
    // Startup/login is wholly owned by the native drawable when coverage is
    // complete, so successful native batches may replace their GL copies.
    gGL.setMetalUIReplacementEnabled(true);
#endif

    if (frame_count++ > 1) // make sure we have rendered a frame first
    {
        LLViewerDynamicTexture::updateAllInstances();
    }
    else
    {
        LL_DEBUGS("Window") << "First display_startup frame" << LL_ENDL;
    }

    LLGLState::checkStates();

    gGL.clear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT); // | GL_STENCIL_BUFFER_BIT);
    LLGLSUIDefault gls_ui;
    gPipeline.disableLights();

    if (gViewerWindow)
    gViewerWindow->setup2DRender();
    if (gViewerWindow)
    gViewerWindow->draw();
    gGL.flush();

    LLVertexBuffer::unbind();

    LLGLState::checkStates();

    if (gViewerWindow && gViewerWindow->getWindow())
    {
#if LL_DARWIN
        const bool native_coverage = gGL.metalUIFrameHasCompleteCoverage();
        const bool native_surface = gGL.setMetalPresentationOpaque(true);
        const bool native_presentation = native_coverage && native_surface;
        // The native surface is the only visible presentation owner. Missing
        // coverage is exposed as a native defect; never reveal the GL drawable
        // behind it as a startup/login fallback.
        if (native_presentation)
        {
            LL_INFOS_ONCE("Metal")
                << "Complete startup/login frames are presented by the "
                   "opaque native Metal surface without a GL swap"
                << LL_ENDL;
        }
        else
        {
            LL_WARNS_ONCE("Metal")
                << "A startup/login frame was not completely reproduced by "
                   "Metal; the opaque native surface remains authoritative"
                << LL_ENDL;
        }
#else
        gViewerWindow->getWindow()->swapBuffers();
#endif
        // Finish the login frame: uniform rings and queries key their reuse on
        // completed GPU frames even when native coverage is incomplete.
        gGL.endFrame();
    }
}

void display_update_camera(bool without_render_state = false)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Update Camera");
    // TODO: cut draw distance down if customizing avatar?
    // TODO: cut draw distance on per-parcel basis?

    // Cut draw distance in half when customizing avatar,
    // but on the viewer only.
    const bool reverse_z = LLRender::sReverseZ;
    F32 final_far = ALFarPlane::clampDrawDistance(gAgentCamera.mDrawDistance, reverse_z);
    if (gCubeSnapshot)
    {
        if (gPipeline.mHeroProbeManager.isMirrorPass())
        {
            // mirrors are literal reflections and get their own render distance
            static LLCachedControl<F32> hero_probe_distance(gSavedSettings, "RenderHeroProbeDistance", 8.f);
            final_far = hero_probe_distance();
        }
        else
        {
            static LLCachedControl<F32> reflection_probe_draw_distance(gSavedSettings, "RenderReflectionProbeDrawDistance", 64.f);
            final_far = reflection_probe_draw_distance();
        }
    }
    else if (CAMERA_MODE_CUSTOMIZE_AVATAR == gAgentCamera.getCameraMode())
    {
        final_far *= 0.5f;
    }
    // When system memory is critically low or recovering, shrink draw distance.
    else if (const F32 mem_factor = LLMemory::getSystemMemoryBudgetFactor(); mem_factor > 1.f)
    {
        final_far = llmax(32.f, final_far / mem_factor);
    }
    LLViewerCamera::getInstance()->setFar(final_far);

    // The camera's far plane above stays the draw distance. The projection's only says how depth is stored
    // (alfarplane.h), and setup3DRender() below applies it.
    static LLCachedControl<S32> force_projection(gSavedSettings, "AlchemyRenderFarPlaneForce", ALFarPlane::FORCE_AUTOMATIC);
    LLViewerCamera* camera = LLViewerCamera::getInstance();
    camera->setProjectionFar(ALFarPlane::projectionFar(reverse_z, gCubeSnapshot, force_projection(), gAgentCamera.mDrawDistance));
    LLVOAvatar::sRenderDistance = llclamp(final_far, 16.f, 256.f);
    if (without_render_state)
    {
        const LLRect& world = gViewerWindow->getWorldViewRectRaw();
        camera->setPerspectiveWithoutRenderState(
            world.mLeft, world.mBottom, world.getWidth(),
            world.getHeight(), camera->getNear(),
            camera->getProjectionFar());
    }
    else
    {
        gViewerWindow->setup3DRender();
    }

    if (!gCubeSnapshot)
    {
        // Update land visibility too
        LLWorld::getInstance()->setLandFarClip(final_far);
    }
}

// Write some stats to LL_INFOS()
void display_stats()
{
    LL_PROFILE_ZONE_SCOPED;
    constexpr F32 FPS_LOG_FREQUENCY = 10.f;
    if (gRecentFPSTime.getElapsedTimeF32() >= FPS_LOG_FREQUENCY)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("DS - FPS");
        LLTrace::Recording& recording = LLTrace::get_frame_recording().getLastRecording();
        F64 normalized_session_jitter = recording.getLastValue(LLStatViewer::NOTRMALIZED_FRAMETIME_JITTER_SESSION);
        F64 normalized_period_jitter = recording.getLastValue(LLStatViewer::NORMALIZED_FRAMTIME_JITTER_PERIOD);
        F32 fps = gRecentFrameCount / FPS_LOG_FREQUENCY;
        LL_INFOS() << llformat("FPS: %.02f SESSION JITTER: %.4f PERIOD JITTER: %.4f", fps, normalized_session_jitter, normalized_period_jitter) << LL_ENDL;
        gRecentFrameCount = 0;
        gRecentFPSTime.reset();
    }
    static LLCachedControl<F32> mem_log_freq(gSavedSettings, "MemoryLogFrequency", 600.f);
    if (mem_log_freq > 0.f && gRecentMemoryTime.getElapsedTimeF32() >= mem_log_freq)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("DS - Memory");
        gMemoryAllocated = U64Bytes(LLMemory::getCurrentRSS());
        U32Megabytes memory = gMemoryAllocated;
        LL_INFOS() << "MEMORY: " << memory << LL_ENDL;
        LLMemory::logMemoryInfo(true) ;
        gRecentMemoryTime.reset();
    }
    constexpr F32 ASSET_STORAGE_LOG_FREQUENCY = 60.f;
    static LLCachedControl<F32> vocache_log_freq(gSavedSettings, "AlchemyVOCacheLogFrequency", 0.f);
    if (vocache_log_freq > 0.f && gRecentVOCacheTime.getElapsedTimeF32() >= vocache_log_freq)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("DS - VOCache");
        LLWorld::getInstance()->logObjectCacheInfo();
        gRecentVOCacheTime.reset();
    }
    if (gAssetStorageLogTime.getElapsedTimeF32() >= ASSET_STORAGE_LOG_FREQUENCY)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("DS - Asset Storage");
        gAssetStorageLogTime.reset();
        gAssetStorage->logAssetStorageInfo();
    }
}

static void update_tp_display(bool minimized)
{
    static LLCachedControl<F32> teleport_arrival_delay(gSavedSettings, "TeleportArrivalDelay");
    static LLCachedControl<F32> teleport_local_delay(gSavedSettings, "TeleportLocalDelay");

    S32 attach_count = 0;
    if (isAgentAvatarValid())
    {
        attach_count = gAgentAvatarp->getAttachmentCount();
    }
    F32 teleport_save_time = TELEPORT_EXPIRY + TELEPORT_EXPIRY_PER_ATTACHMENT * attach_count;
    F32 teleport_elapsed = gTeleportDisplayTimer.getElapsedTimeF32();
    F32 teleport_percent = teleport_elapsed * (100.f / teleport_save_time);
    if (gAgent.getTeleportState() != LLAgent::TELEPORT_START && teleport_percent > 100.f)
    {
        // Give up.  Don't keep the UI locked forever.
        LL_WARNS("Teleport") << "Giving up on teleport. elapsed time " << teleport_elapsed << " exceeds max time " << teleport_save_time << LL_ENDL;
        gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
        gAgent.setTeleportMessage(std::string());
    }

    ALFloaterProgressView* pProgFloater = LLFloaterReg::getTypedInstance<ALFloaterProgressView>("progress_view");

    // Make sure the TP progress panel gets hidden in case the viewer window
    // is minimized *during* a TP. HB
    if (minimized)
    {
        pProgFloater->setShowProgress(false);
    }

    const std::string& message = gAgent.getTeleportMessage();
    switch (gAgent.getTeleportState())
    {
        case LLAgent::TELEPORT_PENDING:
        {
            gTeleportDisplayTimer.reset();
            const std::string& msg = LLAgent::sTeleportProgressMessages["pending"];
            if (!minimized)
            {
                pProgFloater->setShowProgress(true);
                pProgFloater->setProgressPercent(llmin(teleport_percent, 0.0f));
                pProgFloater->setProgressString(msg);
            }
            gAgent.setTeleportMessage(msg);
            break;
        }

        case LLAgent::TELEPORT_START:
        {
            // Transition to REQUESTED.  Viewer has sent some kind
            // of TeleportRequest to the source simulator
            gTeleportDisplayTimer.reset();
            const std::string& msg = LLAgent::sTeleportProgressMessages["requesting"];
            LL_INFOS("Teleport") << "A teleport request has been sent, setting state to TELEPORT_REQUESTED" << LL_ENDL;
            gAgent.setTeleportState(LLAgent::TELEPORT_REQUESTED);
            gAgent.setTeleportMessage(msg);
            if (!minimized)
            {
                pProgFloater->setShowProgress(true);
                pProgFloater->setProgressPercent(llmin(teleport_percent, 0.0f));
                pProgFloater->setProgressString(msg);
                //pProgFloater->setProgressMessage(gAgent.mMOTD);
            }
            break;
        }

        case LLAgent::TELEPORT_REQUESTED:
            // Waiting for source simulator to respond
            if (!minimized)
            {
                pProgFloater->setProgressPercent(llmin(teleport_percent, 37.5f));
                pProgFloater->setProgressString(message);
            }
            break;

        case LLAgent::TELEPORT_MOVING:
            // Viewer has received destination location from source simulator
            if (!minimized)
            {
                pProgFloater->setProgressPercent(llmin(teleport_percent, 75.f));
                pProgFloater->setProgressString(message);
            }
            break;

        case LLAgent::TELEPORT_START_ARRIVAL:
            // Transition to ARRIVING.  Viewer has received avatar update, etc.,
            // from destination simulator
            gTeleportArrivalTimer.reset();
            LL_INFOS("Teleport") << "Changing state to TELEPORT_ARRIVING" << LL_ENDL;
            gAgent.setTeleportState(LLAgent::TELEPORT_ARRIVING);
            gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages["arriving"]);
            gAgent.sheduleTeleportIM();
            gTextureList.mForceResetTextureStats = true;
            gAgentCamera.resetView(true, true);
            if (!minimized)
            {
                pProgFloater->setProgressCancelButtonVisible(false, LLTrans::getString("Cancel"));
                pProgFloater->setProgressPercent(75.f);
            }
            break;

        case LLAgent::TELEPORT_ARRIVING:
        // Make the user wait while content "pre-caches"
        {
            F32 arrival_fraction = (gTeleportArrivalTimer.getElapsedTimeF32() / teleport_arrival_delay());
            if (arrival_fraction > 1.f)
            {
                arrival_fraction = 1.f;
                //LLFirstUse::useTeleport();
                LL_INFOS("Teleport") << "arrival_fraction is " << arrival_fraction << " changing state to TELEPORT_NONE" << LL_ENDL;
                gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
            }
            if (!minimized)
            {
                pProgFloater->setProgressCancelButtonVisible(false, LLTrans::getString("Cancel"));
                pProgFloater->setProgressPercent(arrival_fraction * 25.f + 75.f);
                pProgFloater->setProgressString(message);
            }
            break;
        }

        case LLAgent::TELEPORT_LOCAL:
        // Short delay when teleporting in the same sim (progress screen active but not shown - did not
        // fall-through from TELEPORT_START)
        {
            if (gTeleportDisplayTimer.getElapsedTimeF32() > teleport_local_delay())
            {
                //LLFirstUse::useTeleport();
                LL_INFOS("Teleport") << "State is local and gTeleportDisplayTimer " << gTeleportDisplayTimer.getElapsedTimeF32()
                                     << " exceeds teleport_local_delete " << teleport_local_delay
                                     << "; setting state to TELEPORT_NONE"
                                     << LL_ENDL;
                gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
            }
            break;
        }

        case LLAgent::TELEPORT_NONE:
            // No teleport in progress
            pProgFloater->setShowProgress(false);
            gTeleportDisplay = false;
    }
}

bool display_window_hidden()
{
    return !gViewerWindow->getActive()
        || !gViewerWindow->getWindow()->getVisible()
        || gViewerWindow->getWindow()->getMinimized()
        || gNonInteractive;
}

// The frames ahead of a frame display() showed and left for the main loop to end (display_end_frame), 0 for none.
static U32 sFrameEndOwed = 0;

// Runs the main loop's queued work in the time the frame's end is about to wait for the GPU, as much as the frames'
// ends before show there is to spare (ALPacingSlack). After the swap and before the frame's fence, so that the frame's
// end waits only for what is left, and with no poll between the items, since on a threaded driver a poll is that wait.
// Out of display(), whose scope the work was never written for: it is what idle() runs, posted for the main thread.
static void fill_pacing_slack()
{
    ALPacingSlack& slack = gGL.frameFences().slack();
    const F64 budget = slack.budget();
    F64 worked = 0.0;
    if (gPipeline.pacingSlackWork() && budget > slack.reserve())
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("frame pacing slack work");
        // The main thread is the queue's one consumer: work it sees queued is there for it to run.
        worked = slack.fill(
            []
            {
                if (gMainloopWork.size() == 0)
                {
                    return false;
                }
                gMainloopWork.runOne();
                return true;
            },
            []() -> F64 { return LLTimer::getTotalSeconds(); });
    }
    LL_PROFILE_PLOT("frame pacing slack budget ms", budget * 1000.0);
    LL_PROFILE_PLOT("frame pacing slack work ms", worked * 1000.0);
}

#if LL_DARWIN
static ALMetalBenchmarkRecorder::Provenance sMetalBenchmarkProvenance;
static bool sMetalBenchmarkCompletedSuccessfully = false;
static bool sMetalStageTimingFormalReady = false;
static std::unique_ptr<ALMetalFrameCapture> sMetalFrameCapture;
static bool sMetalFrameCaptureMainEligible = false;
static U64 sMetalFrameCaptureEligibleFrame = 0;

static bool metal_selected_capture_ready(ALMetalFrameCapture::Mode mode) noexcept
{
    return sMetalFrameCaptureMainEligible &&
        sMetalFrameCaptureEligibleFrame == gFrameCount &&
        sMetalBenchmarkCompletedSuccessfully && sMetalFrameCapture &&
        sMetalFrameCapture->mode() == mode &&
        sMetalFrameCapture->selectedPassReady();
}

bool metal_base_capture_ready() noexcept
{
    return metal_selected_capture_ready(ALMetalFrameCapture::Mode::main_base);
}

bool metal_hero_forward_capture_ready() noexcept
{
    return metal_selected_capture_ready(ALMetalFrameCapture::Mode::hero_forward);
}

static bool begin_metal_selected_capture(ALMetalFrameCapture::Mode mode) noexcept
{
    try
    {
        if (!metal_selected_capture_ready(mode)) return false;
        std::string error;
        if (!sMetalFrameCapture->beginSelectedPass(gFrameCount, &error))
        {
            LL_WARNS("MetalCapture") << error << LL_ENDL;
            return false;
        }
        return true;
    }
    catch (...)
    {
        sMetalFrameCapture.reset();
        return false;
    }
}

bool begin_metal_base_capture() noexcept
{
    return begin_metal_selected_capture(ALMetalFrameCapture::Mode::main_base);
}

bool begin_metal_hero_forward_capture() noexcept
{
    return begin_metal_selected_capture(ALMetalFrameCapture::Mode::hero_forward);
}

static void end_metal_selected_capture(bool completed) noexcept
{
    try
    {
        if (!sMetalFrameCapture) return;
        std::string error;
        if (!sMetalFrameCapture->finishSelectedPass(gFrameCount, &error, completed))
            LL_WARNS("MetalCapture") << error << LL_ENDL;
    }
    catch (...)
    {
        sMetalFrameCapture.reset();
    }
}

void end_metal_base_capture(bool completed) noexcept
{
    end_metal_selected_capture(completed);
}

void end_metal_hero_forward_capture(bool completed) noexcept
{
    end_metal_selected_capture(completed);
}

static void advance_metal_frame_capture(bool normal_display)
{
    static bool initialized = false;
    auto& capture = sMetalFrameCapture;
    if (initialized && !capture) return;
    try
    {
        if (!gViewerWindow || !gViewerWindow->getWindow()) return;
        ALMetalDevice* device = gViewerWindow->getWindow()->getMetalDevice();
        if (!device) return;
        if (!initialized)
        {
            initialized = true;
            std::string error;
            capture = ALMetalFrameCapture::createIfRequested(*device, &error);
            if (!error.empty())
                LL_WARNS("MetalCapture") << error << LL_ENDL;
        }
        if (!capture) return;
        const bool world_ready = LLStartUp::getStartupState() >= STATE_STARTED &&
            !gTeleportDisplay && !gDisconnected &&
            !LLAppViewer::instance()->logoutRequestSent() && !display_window_hidden();
        if (normal_display)
        {
            sMetalFrameCaptureMainEligible =
                world_ready && sMetalBenchmarkCompletedSuccessfully;
            sMetalFrameCaptureEligibleFrame = gFrameCount;
        }
        if (normal_display && !world_ready &&
            capture->stats().state == ALMetalFrameCapture::State::capturing)
        {
            // Abandon only our diagnostic interval if the normal world stops.
            // Nested cube/snapshot entries must not truncate a main interval.
            capture.reset();
            return;
        }
        const auto before = capture->stats().state;
        std::string error;
        if (!capture->advanceDisplayInterval(gFrameCount,
                normal_display && world_ready && sMetalBenchmarkCompletedSuccessfully,
                &error))
        {
            LL_WARNS("MetalCapture") << error << LL_ENDL;
            capture.reset();
            return;
        }
        const auto after = capture->stats();
        if (after.state != before &&
            (after.state == ALMetalFrameCapture::State::capturing ||
             after.state == ALMetalFrameCapture::State::stopped))
            LL_INFOS("MetalCapture") << "Native diagnostic interval state="
                << static_cast<unsigned>(after.state) << ", start_frame="
                << after.start_frame << ", stop_frame=" << after.stop_frame
                << "; capture-enabled runs are diagnostic only" << LL_ENDL;
    }
    catch (...)
    {
        capture.reset();
    }
}

struct MetalStageTimingSession
{
    bool initialized = false, finished = false;
    unsigned requested = 0, started = 0, cooldown = 0;
    std::unique_ptr<ALMetalStageTimingWindow> active;
    // Outlives display(): ordinary probes run after display_end_frame().
    // It is destroyed before its active window, including at process teardown.
    std::unique_ptr<ALMetalStageTimingScope> main_scope;
    std::vector<std::unique_ptr<ALMetalStageTimingWindow>> sealed;
    std::vector<ALMetalStageTimingResult> reports;
};

static MetalStageTimingSession& metal_stage_timing_session()
{
    static MetalStageTimingSession session;
    return session;
}

static const char* metal_stage_timing_status(ALMetalStageTimingStatus status)
{
    switch (status)
    {
        case ALMetalStageTimingStatus::complete: return "complete";
        case ALMetalStageTimingStatus::unsubmitted_at_seal: return "unsubmitted_at_seal";
        case ALMetalStageTimingStatus::not_encoded: return "not_encoded";
        case ALMetalStageTimingStatus::gpu_error: return "gpu_error";
        case ALMetalStageTimingStatus::counter_unavailable: return "counter_unavailable";
        case ALMetalStageTimingStatus::calibration_unavailable: return "calibration_unavailable";
    }
    return "unknown";
}

static void stop_metal_stage_timing(MetalStageTimingSession& session) noexcept
{
    session.main_scope.reset();
    session.active.reset();
    session.sealed.clear();
    session.reports.clear();
    session.finished = true;
}

static void advance_metal_stage_timing(bool normal_display) noexcept
{
    if (!normal_display) return;
    auto& session = metal_stage_timing_session();
    try
    {
        if (!session.initialized)
        {
            session.initialized = true;
            const char* value = std::getenv("ALCHEMY_METAL_STAGE_TIMING_FRAMES");
            if (!value || !*value) return;
            if (std::strcmp(value, "1") == 0) session.requested = 1;
            else if (std::strcmp(value, "2") == 0) session.requested = 2;
            else if (std::strcmp(value, "3") == 0) session.requested = 3;
            else
            {
                LL_WARNS("MetalStageTiming") << "Stage timing expects 1, 2 or 3 intervals" << LL_ENDL;
                session.finished = true;
                return;
            }
            if (const char* capture = std::getenv(ALMetalFrameCapture::directory_environment);
                capture && *capture)
            {
                LL_WARNS("MetalStageTiming")
                    << "Stage timing disabled during a debugger-capture request" << LL_ENDL;
                session.finished = true;
                return;
            }
        }
        if (!session.requested || session.finished) return;

        // End only at the next normal entry. Neither nested displays nor the
        // final CSV frame's trailing ordinary-probe work can arm a window.
        session.main_scope.reset();
        if (session.active)
        {
            std::string error;
            if (!session.active->seal(&error)) throw std::runtime_error(error);
            session.sealed.push_back(std::move(session.active));
        }
        bool all_finished = true;
        for (auto& window : session.sealed)
        {
            std::vector<ALMetalStageTimingResult> ready;
            std::string error;
            if (!window->poll(ready, &error)) throw std::runtime_error(error);
            for (auto& result : ready) session.reports.push_back(std::move(result));
            all_finished = all_finished && window->isFinished();
        }
        if (session.started == session.requested && all_finished)
        {
            std::size_t prepared = 0, rejected = 0, complete = 0;
            for (const auto& window : session.sealed)
            {
                prepared += window->preparedPassCount();
                rejected += window->rejectedPrepareCount();
            }
            // Logging is deferred until every selected interval has ended;
            // it is not part of the formal samples or the selected windows.
            for (const auto& result : session.reports)
            {
                complete += result.status == ALMetalStageTimingStatus::complete;
                std::ostringstream row;
                row << std::setprecision(17)
                    << "Native stage timing pass: frame=" << result.tag.frame
                    << " view=" << result.tag.view_kind << " face=" << result.tag.face
                    << " pass=" << result.pass_index << " sample=" << result.first_sample
                    << " status=" << metal_stage_timing_status(result.status)
                    << " label=" << std::quoted(result.tag.label);
                if (result.elapsed)
                    row << " vertex_ns=" << result.elapsed->vertex_ns
                        << " fragment_ns=" << result.elapsed->fragment_ns;
                if (result.raw_samples)
                    for (std::size_t i = 0; i < result.raw_samples->size(); ++i)
                        row << " raw" << i << '=' << (*result.raw_samples)[i];
                if (result.calibration)
                    row << " cpu_before_ns=" << result.calibration->cpu_before_ns
                        << " cpu_after_ns=" << result.calibration->cpu_after_ns
                        << " gpu_before=" << result.calibration->gpu_before_ticks
                        << " gpu_after=" << result.calibration->gpu_after_ticks;
                if (result.gpu_error_code) row << " gpu_error=" << result.gpu_error_code;
                LL_INFOS("MetalStageTiming") << row.str() << LL_ENDL;
            }
            LL_INFOS("MetalStageTiming") << "Native stage timing finished: intervals="
                << session.started << " prepared=" << prepared << " rejected=" << rejected
                << " reports=" << session.reports.size() << " complete=" << complete
                << "; elapsed stage intervals overlap; not GPU busy time or formal FPS"
                << LL_ENDL;
            stop_metal_stage_timing(session);
            return;
        }
        if (session.started == session.requested) return;
        if (sMetalBenchmarkCompletedSuccessfully && !sMetalStageTimingFormalReady)
            throw std::runtime_error("Stage timing requires at least300 exact ready-probe samples with provenance");
        const bool ready = !gHeadlessClient && !gPipeline.windowResizePending() &&
            LLStartUp::getStartupState() >= STATE_STARTED &&
            !gTeleportDisplay && !gDisconnected &&
            !LLAppViewer::instance()->logoutRequestSent() && !display_window_hidden() &&
            sMetalStageTimingFormalReady && !ALMetalDevice::commandTimingEnabled();
        if (!ready)
        {
            if (session.started) throw std::runtime_error("World eligibility ended during stage timing");
            return;
        }
        if (session.cooldown < ALMetalFrameCapture::cooldown_frames)
        {
            ++session.cooldown;
            return;
        }
        if (!gViewerWindow || !gViewerWindow->getWindow()) return;
        ALMetalDevice* device = gViewerWindow->getWindow()->getMetalDevice();
        if (!device) return;
        std::string error;
        session.active = ALMetalStageTimingWindow::create(*device,
            ALMetalStageTimingWindow::maximum_passes, &error);
        if (!session.active) throw std::runtime_error(error);
        ++session.started;
        // The main scope is installed after display increments gFrameCount.
    }
    catch (const std::exception& exception)
    {
        stop_metal_stage_timing(session);
        try
        {
            LL_WARNS("MetalStageTiming") << "Optional native stage timing disabled: " << exception.what()
                << LL_ENDL;
        }
        catch (...) {}
    }
    catch (...)
    {
        stop_metal_stage_timing(session);
        try { LL_WARNS("MetalStageTiming") << "Optional native stage timing disabled after an unknown diagnostic failure" << LL_ENDL; }
        catch (...) {}
    }
}

static void begin_metal_stage_timing_main(bool normal_display) noexcept
{
    if (!normal_display) return;
    auto& session = metal_stage_timing_session();
    if (!session.active || session.main_scope) return;
    try
    {
        if (gHeadlessClient || LLStartUp::getStartupState() < STATE_STARTED ||
            gTeleportDisplay || gDisconnected ||
            LLAppViewer::instance()->logoutRequestSent() || display_window_hidden())
        {
            stop_metal_stage_timing(session);
            LL_WARNS("MetalStageTiming") << "Stage timing lost world eligibility before main rendering" << LL_ENDL;
            return;
        }
        session.main_scope = std::make_unique<ALMetalStageTimingScope>(
            session.active.get(), gFrameCount, 1, -1);
    }
    catch (...) { stop_metal_stage_timing(session); }
}

static ALMetalBenchmarkRecorder::Camera benchmark_camera(const LLCamera& source)
{
    ALMetalBenchmarkRecorder::Camera camera;
    std::copy_n(source.getOrigin().mV, 3, camera.origin.begin());
    std::copy_n(source.getAtAxis().mV, 3, camera.at.begin());
    std::copy_n(source.getLeftAxis().mV, 3, camera.left.begin());
    std::copy_n(source.getUpAxis().mV, 3, camera.up.begin());
    std::copy_n(source.getModelview().getF32ptr(), 16, camera.modelview.begin());
    std::copy_n(source.getProjection().getF32ptr(), 16, camera.projection.begin());
    camera.vertical_fov_radians = source.getView();
    camera.aspect = source.getAspect();
    camera.near_clip = source.getNear();
    camera.draw_far_clip = source.getFar();
    return camera;
}

static ALMetalBenchmarkRecorder::ViewState benchmark_view_state()
{
    ALMetalBenchmarkRecorder::ViewState view;
    const LLViewerCamera& camera = LLViewerCamera::instance();
    view.viewer_camera = benchmark_camera(camera);
    view.pass_current_camera = benchmark_camera(LLViewerCamera::getCurrent());
    const LLVector3d global = gAgentCamera.getCameraPositionGlobal();
    std::copy_n(global.mdV, 3, view.agent_camera_global.begin());
    std::copy_n(gAgentCamera.getFocusGlobal().mdV, 3, view.agent_focus_global.begin());
    std::copy_n(gAgentCamera.getFocusTargetGlobal().mdV, 3, view.agent_focus_target_global.begin());
    std::copy_n(gAgentCamera.getCameraUpVector().mV, 3, view.agent_camera_up.begin());
    S32 viewport[4];
    gGL.getViewport(viewport);
    std::copy_n(viewport, 4, view.viewport.begin());
    view.projection_far_clip = camera.getProjectionFar();
    view.requested_draw_distance = gAgentCamera.mDrawDistance;
    view.camera_id = LLViewerCamera::sCurCameraID;
    view.agent_camera_mode = static_cast<U32>(gAgentCamera.getCameraMode());
    view.agent_camera_preset = static_cast<U32>(gAgentCamera.getCameraPreset());
    view.occlusion_mode = static_cast<U32>(LLPipeline::sUseOcclusion);
    view.reverse_z = LLRender::sReverseZ;
    view.main_world_pass = LLPipeline::isMainWorldPass();
    return view;
}

static ALMetalBenchmarkRecorder::Environment benchmark_environment()
{
    ALMetalBenchmarkRecorder::Environment state;
    if (!LLEnvironment::instanceExists()) return state;
    LLEnvironment& environment = LLEnvironment::instance();
    state.captured = true;
    state.selected_environment = static_cast<S32>(environment.getSelectedEnvironment());
    // Do not resolve the local slot through parcel/region defaults: this must
    // distinguish a completed local asset load from the effective fallback.
    const LLSettingsSky::ptr_t local = environment.getEnvironmentFixedSky(
        LLEnvironment::ENV_LOCAL, false);
    if (local)
    {
        state.local_sky_available = true;
        state.local_sky_asset_id = local->getAssetId();
    }
    const LLSettingsSky::ptr_t current = environment.getCurrentSky();
    if (current)
    {
        state.current_sky_available = true;
        state.current_sky_asset_id = current->getAssetId();
        const LLVector3 sun = current->getSunDirection();
        std::copy_n(sun.mV, 3, state.sun_direction.begin());
        state.sun_direction_available = true;
    }
    return state;
}

static void benchmark_camera_restoration(
    ALMetalBenchmarkRecorder::Restoration& result,
    const ALMetalBenchmarkRecorder::ViewState& before)
{
    const auto after = benchmark_view_state();
    ALMetalBenchmarkRecorder::Restoration restored;
    restored.checked = true;
    restored.viewer_camera_restored = before.viewer_camera == after.viewer_camera &&
        before.projection_far_clip == after.projection_far_clip;
    restored.pass_current_camera_restored = before.pass_current_camera == after.pass_current_camera;
    // These are the explicit controller fields read above; HUD zoom animates
    // intentionally and is not a claim about restoring the whole controller.
    restored.agent_camera_fields_restored = before.agent_camera_global == after.agent_camera_global &&
        before.agent_focus_global == after.agent_focus_global &&
        before.agent_focus_target_global == after.agent_focus_target_global &&
        before.agent_camera_up == after.agent_camera_up &&
        before.agent_camera_mode == after.agent_camera_mode &&
        before.agent_camera_preset == after.agent_camera_preset &&
        before.requested_draw_distance == after.requested_draw_distance;
    restored.camera_id_restored = before.camera_id == after.camera_id;
    if (!result.checked) result = restored;
    else
    {
        result.viewer_camera_restored &= restored.viewer_camera_restored;
        result.pass_current_camera_restored &= restored.pass_current_camera_restored;
        result.agent_camera_fields_restored &= restored.agent_camera_fields_restored;
        result.camera_id_restored &= restored.camera_id_restored;
    }
}

static ALMetalBenchmarkRecorder::NativePresentationState
benchmark_presentation_state(
    LLPipeline::MetalWorldPresentationState state)
{
    using PipelineState = LLPipeline::MetalWorldPresentationState;
    using BenchmarkState = ALMetalBenchmarkRecorder::NativePresentationState;
    switch (state)
    {
        case PipelineState::Disabled:
            return BenchmarkState::disabled;
        case PipelineState::NotAttempted:
            return BenchmarkState::not_attempted;
        case PipelineState::CaptureStarted:
            return BenchmarkState::capture_started;
        case PipelineState::CaptureFailed:
            return BenchmarkState::capture_failed;
        case PipelineState::SurfaceUnavailable:
            return BenchmarkState::surface_unavailable;
        case PipelineState::SurfaceSubmitFailed:
            return BenchmarkState::surface_submit_failed;
        case PipelineState::Submitted:
            return BenchmarkState::submitted;
    }
    return BenchmarkState::disabled;
}

struct MetalBenchmarkSession
{
    std::shared_ptr<ALMetalBenchmarkRecorder> recorder;
    std::uint64_t previous_native_draw_count = 0;
    bool reported_failure = false;
    bool stopped = false;
    bool stage_timing_config_eligible = false;
};

static void stop_interrupted_metal_benchmark_session(MetalBenchmarkSession& session) noexcept
{
    if (session.recorder && session.recorder->finished() && !session.stopped)
    {
        session.stopped = true;
        sMetalBenchmarkCompletedSuccessfully = false;
        sMetalStageTimingFormalReady = false;
        LLRender::setMetalCommandTimingEnabled(false);
        gGL.queries().setDiagnosticsEnabled(false);
        ALProbeReadiness::requestObservation(false);
        try
        {
            LL_WARNS("MetalBenchmark")
                << "Metal benchmark collection ended outside a normal display; "
                   "capture is not admissible" << LL_ENDL;
        }
        catch (...) {}

    }
}

struct MetalBenchmarkScopeCleanup
{
    MetalBenchmarkSession& session;
    ~MetalBenchmarkScopeCleanup() noexcept
    { stop_interrupted_metal_benchmark_session(session); }
};

static MetalBenchmarkSession& metal_benchmark_session()
{
    static bool initialized = false;
    static MetalBenchmarkSession session;
    if (!initialized)
    {
        initialized = true;
        std::string error;
        const LLVersionInfo& version = LLVersionInfo::instance();
        auto config = ALMetalBenchmarkRecorder::configFromEnvironment(
            version.getSourceRevision(), version.getBuildConfig(), &error);
        if (!config)
        {
            if (!error.empty())
            {
                LL_WARNS("MetalBenchmark")
                    << "Metal benchmark collection was requested but is "
                       "disabled: " << error << LL_ENDL;
            }
            return session;
        }
        LL_INFOS("MetalBenchmark")
            << "Metal benchmark collection armed for pseudonymous scenario "
            << config->scenario << ", variant " << config->variant << ", "
            << config->warmup_frames << " warm-up frames and "
            << config->warmup_seconds << " warm-up seconds, followed by "
            << config->sample_frames << " consecutive samples"
            << (config->require_exact_native
                    ? "; every sample requires exact native presentation"
                    : "; non-exact native presentation states are accepted")
            << LL_ENDL;
        const bool collect_provenance = config->collect_provenance;
        session.stage_timing_config_eligible = config->require_exact_native &&
            config->require_ready_probes && collect_provenance && config->sample_frames >= 300;
        try
        {
            session.recorder = std::make_shared<ALMetalBenchmarkRecorder>(
                std::move(*config));
            LLRender::setMetalCommandTimingEnabled(true);
            gGL.queries().setDiagnosticsEnabled(collect_provenance);
            session.previous_native_draw_count = ALMetalDevice::encodedDrawCallCount();
            ALProbeReadiness::requestObservation(
                session.recorder->requiresReadyProbes());
        }
        catch (...)
        {
            session.recorder.reset();
            LLRender::setMetalCommandTimingEnabled(false);
            gGL.queries().setDiagnosticsEnabled(false);
            ALProbeReadiness::requestObservation(false);
            LL_WARNS("MetalBenchmark")
                << "Metal benchmark initialization failed; collection disabled"
                << LL_ENDL;
        }
    }
    stop_interrupted_metal_benchmark_session(session);
    return session;
}

static void record_metal_benchmark_frame(
    bool for_snapshot, bool gl_presented, MetalBenchmarkSession& session,
    ALMetalBenchmarkRecorder::DisplayObservation& observation)
{
    const auto& recorder = session.recorder;
    if (!recorder || recorder->finished())
    {
        return;
    }

    const bool native_exact = gPipeline.metalWorldPresentationCurrent();
    const ALMetalBenchmarkRecorder::NativePresentationState
        native_presentation_state = benchmark_presentation_state(
            gPipeline.metalWorldPresentationStateCurrent());
    ALMetalBenchmarkRecorder::Frame frame;
    frame.timestamp = ALMetalBenchmarkRecorder::Clock::now();
    frame.frame_index = gFrameCount;
    frame.drawable_width = static_cast<U32>(
        llmax(gViewerWindow->getWindowWidthRaw(), 0));
    frame.drawable_height = static_cast<U32>(
        llmax(gViewerWindow->getWindowHeightRaw(), 0));
    frame.quality_profile =
        gSavedSettings.getU32("RenderQualityPerformance");
    frame.texture_discard_bias = LLViewerTexture::sDesiredDiscardBias;
    const std::uint64_t native_draw_count =
        ALMetalDevice::encodedDrawCallCount();
    frame.draw_calls = native_draw_count - session.previous_native_draw_count;
    session.previous_native_draw_count = native_draw_count;
#if LL_DARWIN
    frame.metal_command_buffers_committed =
        LLRender::metalCommandBuffersCommitted();
    const LLRender::MetalCommandTiming timing =
        LLRender::metalCommandTiming();
    frame.metal_timing_submitted = timing.submitted;
    frame.metal_timing_completed = timing.completed;
    frame.metal_timing_valid = timing.timed;
    frame.metal_gpu_time_nanoseconds = timing.gpu_time_nanoseconds;
#endif
    frame.native_presentation_state = native_presentation_state;
    frame.native_exact = native_exact;
    frame.gl_fallback = gl_presented && !native_exact &&
        native_presentation_state !=
            ALMetalBenchmarkRecorder::NativePresentationState::disabled;
    frame.eligible = LLStartUp::getStartupState() >= STATE_STARTED &&
        !for_snapshot && !gSnapshot && !gTeleportDisplay && !gDisconnected &&
        !LLAppViewer::instance()->logoutRequestSent() &&
        !display_window_hidden() && (gl_presented || native_exact);
    if (recorder->requiresReadyProbes())
    {
        frame.probe_readiness = ALProbeReadiness::observe(gFrameCount);
    }
    if (ALQueryPool::diagnosticsEnabled())
    {
        sMetalBenchmarkProvenance.frame_end_view = benchmark_view_state();
        sMetalBenchmarkProvenance.main_world_queries = gGL.queries().diagnostics();
        frame.provenance = sMetalBenchmarkProvenance;
    }

    std::string error;
    const ALMetalBenchmarkRecorder::Result result =
        observation.record(frame, &error);
    if (result == ALMetalBenchmarkRecorder::Result::complete)
    {
        session.stopped = true;
        sMetalBenchmarkCompletedSuccessfully = true;
        sMetalStageTimingFormalReady = session.stage_timing_config_eligible &&
            recorder->samplesWritten() >= 300 && recorder->requiresReadyProbes();
        LLRender::setMetalCommandTimingEnabled(false);
        gGL.queries().setDiagnosticsEnabled(false);
        ALProbeReadiness::requestObservation(false);
        LL_INFOS("MetalBenchmark")
            << "Metal benchmark collection completed with "
            << recorder->samplesWritten()
            << " consecutive post-warm-up frame samples" << LL_ENDL;
    }
    else if (result == ALMetalBenchmarkRecorder::Result::failed &&
             !session.reported_failure)
    {
        session.stopped = true;
        sMetalBenchmarkCompletedSuccessfully = false;
        sMetalStageTimingFormalReady = false;
        LLRender::setMetalCommandTimingEnabled(false);
        gGL.queries().setDiagnosticsEnabled(false);
        ALProbeReadiness::requestObservation(false);
        session.reported_failure = true;
        LL_WARNS("MetalBenchmark")
            << "Metal benchmark collection stopped without admissible "
               "completion: " << error << LL_ENDL;
    }
}
#endif

void display_end_frame()
{
    const U32 frames_ahead = sFrameEndOwed;
    if (frames_ahead == 0)
    {
        return;
    }
    sFrameEndOwed = 0;
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("End Frame");
    {
        LLPerfStats::RecordSceneTime T(LLPerfStats::StatType_t::RENDER_IDLE);
        fill_pacing_slack();
    }
    LLPerfStats::RecordSceneTime T(LLPerfStats::StatType_t::RENDER_DISPLAY);
    gGL.endFrame(frames_ahead);
}

// Paint the display!
void display(bool rebuild, F32 zoom_factor, int subfield, bool for_snapshot)
{
#if LL_DARWIN
    auto& benchmark_session = metal_benchmark_session();
    // Declared first so cleanup runs after the observation guard invalidates
    // an early-return sequence, including nested-display failures.
    MetalBenchmarkScopeCleanup benchmark_cleanup{benchmark_session};
    ALMetalBenchmarkRecorder::DisplayObservation benchmark_observation(
        benchmark_session.recorder && !benchmark_session.recorder->finished()
            ? benchmark_session.recorder : nullptr);
    // Start before any frame commands, and stop at the next normal display
    // entry. Ordinary probes run after display_end_frame, outside display().
    const bool metal_normal_display =
        !for_snapshot && !gSnapshot && !gCubeSnapshot &&
        !LLPipeline::sRenderingHUDs && zoom_factor == 1.f && subfield == 0;
    advance_metal_frame_capture(metal_normal_display);
    advance_metal_stage_timing(metal_normal_display);
    std::optional<ALMetalStageTimingScope> stage_snapshot_guard;
    if (!metal_normal_display && ALMetalStageTimingScope::current())
        stage_snapshot_guard.emplace(nullptr, 0, 0, -1);
#endif
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Render");
    LL_PROFILE_GPU_ZONE("Render");

    LLPerfStats::RecordSceneTime T (LLPerfStats::StatType_t::RENDER_DISPLAY); // render time capture - This is the main stat for overall rendering.

#if LL_DARWIN
    if (ALQueryPool::diagnosticsEnabled()) sMetalBenchmarkProvenance = {};
#endif

    // The frame the last pass showed is the main loop's to end (display_end_frame); one left unended ends here, unpaced.
    llassert(sFrameEndOwed == 0);
    if (sFrameEndOwed != 0)
    {
        sFrameEndOwed = 0;
        gGL.endFrame();
    }

#if LL_DARWIN
    // Keep the Metal layer opaque before every world frame, including
    // resize-only and incomplete frames. The previous native drawable remains
    // visible until a complete current transaction replaces it; GL rendering
    // may still supply migration-side effects but never presentation fallback.
    if (LLStartUp::getStartupState() >= STATE_PRECACHE)
    {
        gGL.setMetalUIReplacementEnabled(false);
        gGL.setMetalPresentationOpaque(true);
        static bool logged_native_only_presentation = false;
        if (!logged_native_only_presentation)
        {
            logged_native_only_presentation = true;
            LL_INFOS("Metal") << "World presentation is native-only; incomplete "
                                 "frames retain the previous Metal drawable and "
                                 "never expose a GL swap"
                               << LL_ENDL;
        }
    }
#endif

    // What the settings and the window changed since the last frame, applied once, here, whatever the frame goes on to
    // draw: the login screen's frames apply them as the world's do.
    if (gPipeline.windowResizePending())
    { //skip render on frames where window has been resized
        LL_DEBUGS("Window") << "Resizing window" << LL_ENDL;
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Resize Window");
#if LL_DARWIN
        // Metal is the sole visible presentation owner. Keep the preceding
        // complete drawable visible while applyRenderInputs reconciles the
        // new size; do not clear or swap the migration GL drawable.
        gGL.setMetalPresentationOpaque(true);
#else
        gGL.clear(GL_COLOR_BUFFER_BIT);
        gViewerWindow->getWindow()->swapBuffers();
#endif
        gGL.endFrame();
        gPipeline.applyRenderInputs();
        return;
    }
    gPipeline.applyRenderInputs();

    gSnapshot = for_snapshot;

    if (LLPipeline::sRenderDeferred)
    { //hack to make sky show up in deferred snapshots
        for_snapshot = false;
    }

    LLGLSDefault gls_default;
    LLGLDepthTest gls_depth(GL_TRUE, GL_TRUE, GL_LEQUAL);

    LLVertexBuffer::unbind();

    LLGLState::checkStates();

    gPipeline.disableLights();

    // Don't draw if the window is hidden or minimized.
    // In fact, must explicitly check the minimized state before drawing.
    // Attempting to draw into a minimized window causes a GL error. JC
    if (display_window_hidden())
    {
        // Clean up memory the pools may have allocated
        if (rebuild)
        {
            stop_glerror();
            gPipeline.rebuildPools();
            stop_glerror();
        }

        // Process hud objects to prevent overflow when backgrounded or minimized.
        LLHUDObject::renderAllForTimer();

        stop_glerror();
        gViewerWindow->returnEmptyPicks();
        stop_glerror();

        // We still need to update the teleport progress (to get changes done
        // in TP states, else the sim does not get the messages signaling the
        // agent's arrival). This fixes BUG-230616. HB
        if (gTeleportDisplay)
        {
            // true = minimized, do not show/update the TP screen. HB
            update_tp_display(true);
        }

        // Run texture subsystem to discard memory while backgrounded
        if (!gNonInteractive)
        {
            LL_PROFILE_ZONE_NAMED("Update Images");

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Class");
                LLViewerTexture::updateClass();
            }

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Image Update Bump");
                gBumpImageList.updateImages();  // must be called before gTextureList version so that it's textures are thrown out first.
            }

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("List");
                F32 max_image_decode_time = 0.050f * gFrameIntervalSeconds.value();          // 50 ms/second decode time
                max_image_decode_time     = llclamp(max_image_decode_time, 0.002f, 0.005f);  // min 2ms/frame, max 5ms/frame)
                gTextureList.updateImages(max_image_decode_time);
            }

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("GLTF Materials Cleanup");
                // remove dead gltf materials
                gGLTFMaterialList.flushMaterials();
            }

            // A pass that draws nothing still ends a frame, unpaced (ALFrameEnd): the uploads other contexts hand over
            // publish, the queries are read, and what the GPU used is reused, as on any frame. A session without a
            // window has no GL to end one in.
            const ALFrameEnd end = ALFrameEnd::make(true, false, gPipeline.maxFramesAhead());
            gGL.endFrame(end.mFramesAhead);
        }
        return;
    }

    gViewerWindow->checkSettings();

    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Picking");
        gViewerWindow->performPick();
    }

    LLAppViewer::instance()->pingMainloopTimeout("Display:CheckStates");
    LLGLState::checkStates();

    //////////////////////////////////////////////////////////

    //
    // Bail out if we're in the startup state and don't want to try to
    // render the world.
    //
    if (LLStartUp::getStartupState() < STATE_PRECACHE)
    {
        LLAppViewer::instance()->pingMainloopTimeout("Display:Startup");
        display_startup();
        return;
    }

    if (gShaderProfileFrame)
    {
        LLGLSLShader::initProfile();
    }

    //LLGLState::verify(false);

    /////////////////////////////////////////////////
    //
    // Update GL Texture statistics (used for discard logic?)
    //

    LLAppViewer::instance()->pingMainloopTimeout("Display:TextureStats");
    stop_glerror();

    LLImageGL::updateStats(gFrameTimeSeconds);

    static LLCachedControl<S32> avatar_name_tag_mode(gSavedSettings, "AvatarNameTagMode", 1);
    static LLCachedControl<S32> name_tag_show_group_titles(gSavedSettings, "GroupTitlesTagMode", 2 /*all group tags*/);
    LLVOAvatar::sRenderName = avatar_name_tag_mode;
    LLVOAvatar::sRenderGroupTitles = avatar_name_tag_mode > 0 ? name_tag_show_group_titles : 0;

    gPipeline.mBackfaceCull = true;
    gFrameCount++;
#if LL_DARWIN
    // display entry observes the preceding frame index; the actual world
    // commands below belong to this newly incremented main display.
    if (metal_normal_display && sMetalFrameCaptureMainEligible)
        sMetalFrameCaptureEligibleFrame = gFrameCount;
    begin_metal_stage_timing_main(metal_normal_display);
#endif
    gRecentFrameCount++;
    if (gFocusMgr.getAppHasFocus())
    {
        gForegroundFrameCount++;
    }

    //////////////////////////////////////////////////////////
    //
    // Display start screen if we're teleporting, and skip render
    //

    if (gTeleportDisplay)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Teleport Display");
        LLAppViewer::instance()->pingMainloopTimeout("Display:Teleport");
        // Note: false = not minimized, do update the TP screen. HB
        update_tp_display(false);
// [SL:KB] - Patch: Appearance-TeleportAttachKill | Checked: Catznip-4.0
        LLViewerParcelMgr::getInstance()->onTeleportDone();
// [/SL:KB]
    }
    else if(LLAppViewer::instance()->logoutRequestSent())
    {
        LLAppViewer::instance()->pingMainloopTimeout("Display:Logout");
        F32 percent_done = gLogoutTimer.getElapsedTimeF32() * 100.f / gLogoutMaxTime;
        if (percent_done > 100.f)
        {
            percent_done = 100.f;
        }

        if( LLApp::isExiting() )
        {
            percent_done = 100.f;
        }

        gViewerWindow->setProgressPercent( percent_done );
        gViewerWindow->setProgressMessage(std::string());
    }
    //////////////////////////
    //
    // Prepare for the next frame
    //

    /////////////////////////////
    //
    // Update the camera
    //
    //

    LLAppViewer::instance()->pingMainloopTimeout("Display:Camera");
    if (LLViewerCamera::instanceExists())
    {
        LLViewerCamera::getInstance()->setZoomParameters(zoom_factor, subfield);
        LLViewerCamera::getInstance()->setNear(MIN_NEAR_PLANE);
    }

    //////////////////////////
    //
    // clear the next buffer
    // (must follow dynamic texture writing since that uses the frame buffer)
    //

    if (gDisconnected)
    {
        LLAppViewer::instance()->pingMainloopTimeout("Display:Disconnected");
        render_ui();
        swap();
    }

    //////////////////////////
    //
    // Set rendering options
    //
    //
    LLAppViewer::instance()->pingMainloopTimeout("Display:RenderSetup");
    stop_glerror();

    ///////////////////////////////////////
    //
    // Slam lighting parameters back to our defaults.
    // Note that these are not the same as GL defaults...

    stop_glerror();
    gGL.setAmbientLightColor(LLColor4::white);
    stop_glerror();

    /////////////////////////////////////
    //
    // Render
    //
    // Actually push all of our triangles to the screen.
    //

    // do render-to-texture stuff here
    if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_DYNAMIC_TEXTURES))
    {
        LLAppViewer::instance()->pingMainloopTimeout("Display:DynamicTextures");
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Update Dynamic Textures");
        if (LLViewerDynamicTexture::updateAllInstances())
        {
            gGL.setColorMask(true, true);
            gGL.clear(GL_DEPTH_BUFFER_BIT);
        }
    }

    gViewerWindow->setup3DViewport();

    gPipeline.resetFrameStats();    // Reset per-frame statistics.

    // Whether the world's frame reaches the screen: a capture holds its swap back, and a viewer on its way out draws
    // no world.
    bool presented = false;
    if (!gDisconnected && !LLApp::isExiting())
    {
        // The water's field around the main camera, before the mirrors, the shadow fit and the probes read it.
        gPipeline.mWaterSurface.update();

        // Render mirrors and associated hero probes before we render the rest of the scene.
        // This ensures the scene state in the hero probes are exactly the same as the rest of the scene before we render it.
        if (gPipeline.resourceInputs().mMirrors && !gSnapshot)
        {
            LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Update hero probes");
            LL_PROFILE_GPU_ZONE("hero manager")
#if LL_DARWIN
            std::optional<ALMetalBenchmarkRecorder::ViewState> before_hero;
            if (ALQueryPool::diagnosticsEnabled()) before_hero = benchmark_view_state();
#endif
            gPipeline.mHeroProbeManager.update();
            gPipeline.mHeroProbeManager.renderProbes();
#if LL_DARWIN
            if (before_hero) benchmark_camera_restoration(
                sMetalBenchmarkProvenance.hero_restoration, *before_hero);
#endif
        }

        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("display - 1");
        LLAppViewer::instance()->pingMainloopTimeout("Display:Update");
        if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
        { //don't draw hud objects in this frame
            gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
        }

        if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES))
        { //don't draw hud particles in this frame
            gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
        }

        stop_glerror();
        display_update_camera();
        stop_glerror();

        {
            LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Env Update");
            // update all the sky/atmospheric/water settings
            LLEnvironment::instance().update(LLViewerCamera::getInstance());
        }

        // *TODO: merge these two methods
        {
            LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("HUD Update");
            LLHUDManager::getInstance()->updateEffects();
            LLHUDObject::updateAll();
            stop_glerror();
        }

        {
            LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Update Geom");
            const F32 max_geom_update_time = llmin(0.005f*10.f*gFrameIntervalSeconds.value(), 0.005f); // 50 ms/second update time, max 5ms/frame
            gPipeline.createObjects(max_geom_update_time);
            gPipeline.processPartitionQ();
            gPipeline.updateGeom(max_geom_update_time);
            stop_glerror();
        }

        gPipeline.updateGL();

        stop_glerror();

        LLAppViewer::instance()->pingMainloopTimeout("Display:Cull");

        //Increment drawable frame counter
        LLDrawable::incrementVisible();

        LLSpatialGroup::sNoDelete = true;
        ALTextureSlot::setWhiteImage(LLViewerFetchedTexture::sWhiteImagep->getGLTexture());

        S32 occlusion = LLPipeline::sUseOcclusion;
        if (gDepthDirty)
        { //depth buffer is invalid, don't overwrite occlusion state
            LLPipeline::sUseOcclusion = llmin(occlusion, 1);
        }
        // The queries draw through gOcclusionCubeProgram. One that did not build would draw them through the stand-in,
        // which passes no sample, so that every group queried read as occluded: the frame culls by no occlusion.
        bool occlusion_program_available =
            gOcclusionCubeProgram.isComplete();
#if LL_DARWIN
        occlusion_program_available = occlusion_program_available ||
            (gOcclusionCubeProgram.isNativeConfigured() &&
             gPipeline.isMetalVisibilityCullingAvailable());
#endif
        if (!occlusion_program_available)
        {
            LLPipeline::sUseOcclusion = 0;
        }
        gDepthDirty = false;

        LLGLState::checkStates();

        // The queries of the frames the frame fences have shown finished were read at the last frame's end
        // (LLRender::endFrame), so nothing here asks GL for a result in the middle of the frame.
        gPipeline.readAttachmentTimes();

        static LLCullResult result;
        LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
        LLPipeline::sUnderWaterRender = LLViewerCamera::getInstance()->cameraUnderWater();
#if LL_DARWIN
        if (ALQueryPool::diagnosticsEnabled() && LLPipeline::isMainWorldPass())
        {
            sMetalBenchmarkProvenance.main_view = benchmark_view_state();
            sMetalBenchmarkProvenance.main_environment = benchmark_environment();
            sMetalBenchmarkProvenance.main_view_captured = true;
        }
#endif
        gPipeline.updateCull(*LLViewerCamera::getInstance(), result);
        stop_glerror();

        LLGLState::checkStates();

        LLAppViewer::instance()->pingMainloopTimeout("Display:Swap");

        {
            LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("display - 2")
            gGL.setColorMask(true, true);
            gGL.setClearColor(LLColor4::transparent);

            LLGLState::checkStates();

            if (!for_snapshot)
            {
                if (gFrameCount > 1 && !for_snapshot)
                { //for some reason, ATI 4800 series will error out if you
                  //try to generate a shadow before the first frame is through
                    gPipeline.generateSunShadow(*LLViewerCamera::getInstance());
                }

                LLVertexBuffer::unbind();

                LLGLState::checkStates();

                const LLCamera saved_camera = LLViewerCamera::getCurrent();

                LLVOAvatar::updateImpostors();

                LLViewerCamera::setCurrent(saved_camera);
                gGL.matrixMode(LLRender::MM_PROJECTION);
                gGL.loadMatrix(saved_camera.getProjection());
                gGL.matrixMode(LLRender::MM_MODELVIEW);
                gGL.loadMatrix(saved_camera.getModelview());
                gViewerWindow->setup3DViewport();

                LLGLState::checkStates();
            }
            gGL.clear(GL_DEPTH_BUFFER_BIT);
        }

        //////////////////////////////////////
        //
        // Update images, using the image stats generated during object update/culling
        //
        // Can put objects onto the retextured list.
        //
        // Doing this here gives hardware occlusion queries extra time to complete
        LLAppViewer::instance()->pingMainloopTimeout("Display:UpdateImages");

        {
            LL_PROFILE_ZONE_NAMED("Update Images");

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Class");
                LLViewerTexture::updateClass();
            }

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Image Update Bump");
                gBumpImageList.updateImages();  // must be called before gTextureList version so that it's textures are thrown out first.
            }

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("List");
                F32 max_image_decode_time = 0.050f*gFrameIntervalSeconds.value(); // 50 ms/second decode time
                max_image_decode_time = llclamp(max_image_decode_time, 0.002f, 0.005f ); // min 2ms/frame, max 5ms/frame)
                gTextureList.updateImages(max_image_decode_time);
            }

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("GLTF Materials Cleanup");
                //remove dead gltf materials
                gGLTFMaterialList.flushMaterials();
            }
        }

        LLGLState::checkStates();

        ///////////////////////////////////
        //
        // StateSort
        //
        // Responsible for taking visible objects, and adding them to the appropriate draw orders.
        // In the case of alpha objects, z-sorts them first.
        // Also creates special lists for outlines and selected face rendering.
        //
        LLAppViewer::instance()->pingMainloopTimeout("Display:StateSort");
        {
            LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("display - 4")
            LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
            gPipeline.stateSort(*LLViewerCamera::getInstance(), result);
            stop_glerror();

            if (rebuild)
            {
                //////////////////////////////////////
                //
                // rebuildPools
                //
                //
                gPipeline.rebuildPools();
                stop_glerror();
            }
        }

        LLSceneMonitor::getInstance()->fetchQueryResult();

        LLGLState::checkStates();

        LLPipeline::sUseOcclusion = occlusion;

        {
            LLAppViewer::instance()->pingMainloopTimeout("Display:Sky");
            LL_PROFILE_ZONE_NAMED_CATEGORY_ENVIRONMENT("update sky");
            gSky.updateSky();
        }

        if(gUseWireframe)
        {
            gGL.setClearColor(LLColor4(0.5f, 0.5f, 0.5f, 0.f));
            gGL.clear(GL_COLOR_BUFFER_BIT);
        }

        LLAppViewer::instance()->pingMainloopTimeout("Display:RenderStart");

        //// render frontmost floater opaque for occlusion culling purposes
        //LLFloater* frontmost_floaterp = gFloaterView->getFrontmost();
        //// assumes frontmost floater with focus is opaque
        //if (frontmost_floaterp && gFocusMgr.childHasKeyboardFocus(frontmost_floaterp))
        //{
        //  gGL.matrixMode(LLRender::MM_MODELVIEW);
        //  gGL.pushMatrix();
        //  {
        //      gGL.getTextureSlot(0)->unbind();

        //      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
        //      gGL.loadIdentity();

        //      LLRect floater_rect = frontmost_floaterp->calcScreenRect();
        //      // deflate by one pixel so rounding errors don't occlude outside of floater extents
        //      floater_rect.stretch(-1);
        //      LLRectf floater_3d_rect((F32)floater_rect.mLeft / (F32)gViewerWindow->getWindowWidthScaled(),
        //                              (F32)floater_rect.mTop / (F32)gViewerWindow->getWindowHeightScaled(),
        //                              (F32)floater_rect.mRight / (F32)gViewerWindow->getWindowWidthScaled(),
        //                              (F32)floater_rect.mBottom / (F32)gViewerWindow->getWindowHeightScaled());
        //      floater_3d_rect.translate(-0.5f, -0.5f);
        //      gGL.translatef(0.f, 0.f, -LLViewerCamera::getInstance()->getNear());
        //      gGL.scalef(LLViewerCamera::getInstance()->getNear() * LLViewerCamera::getInstance()->getAspect() / sinf(LLViewerCamera::getInstance()->getView()), LLViewerCamera::getInstance()->getNear() / sinf(LLViewerCamera::getInstance()->getView()), 1.f);
        //      gGL.color4fv(LLColor4::white.mV);
        //      gGL.begin(LLVertexBuffer::QUADS);
        //      {
        //          gGL.vertex3f(floater_3d_rect.mLeft, floater_3d_rect.mBottom, 0.f);
        //          gGL.vertex3f(floater_3d_rect.mLeft, floater_3d_rect.mTop, 0.f);
        //          gGL.vertex3f(floater_3d_rect.mRight, floater_3d_rect.mTop, 0.f);
        //          gGL.vertex3f(floater_3d_rect.mRight, floater_3d_rect.mBottom, 0.f);
        //      }
        //      gGL.end();
        //      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        //  }
        //  gGL.popMatrix();
        //}

        LLPipeline::sUnderWaterRender = LLViewerCamera::getInstance()->cameraUnderWater();

        LLGLState::checkStates();

        stop_glerror();

        gGL.setColorMask(true, true);

        gPipeline.mRT->deferredScreen.bindTarget();
        if (gUseWireframe)
        {
            constexpr F32 g = 0.5f;
            gGL.setClearColor(LLColor4(g, g, g, 1.f));
        }
        else
        {
            gGL.setClearColor(LLColor4::magenta);
        }
        gPipeline.mRT->deferredScreen.clear();

        gGL.setColorMask(true, false);

        LLAppViewer::instance()->pingMainloopTimeout("Display:RenderGeom");

        if (!(LLAppViewer::instance()->logoutRequestSent() &&
              LLAppViewer::instance()->hasSavedFinalSnapshot()))
        {
            LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("display - 5")
            LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

            static LLCachedControl<bool> render_depth_pre_pass(gSavedSettings, "RenderDepthPrePass", false);
            if (render_depth_pre_pass)
            {
                LLGLDepthTest depth(GL_TRUE, GL_TRUE);
                LLGLEnable cull_face(GL_CULL_FACE);
                gGL.setColorMask(false, false);

                constexpr U32 types[] = {
                    LLRenderPass::PASS_SIMPLE,
                    LLRenderPass::PASS_FULLBRIGHT,
                    LLRenderPass::PASS_SHINY
                };

                U32 num_types = LL_ARRAY_SIZE(types);
                gOcclusionProgram.bind();
                for (U32 i = 0; i < num_types; i++)
                {
                    gPipeline.renderObjects(types[i], false, false);
                }

                gOcclusionProgram.unbind();

            }

            gGL.setColorMask(true, true);
            gPipeline.renderGeomDeferred(*LLViewerCamera::getInstance(), true);
        }

        {
            LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Texture Unbind");
            for (S32 i = 0; i < gGLManager.mNumTextureImageUnits; i++)
            { //dummy cleanup of any currently bound textures
                gGL.getTextureSlot(i)->unbind();
            }
        }

        LLAppViewer::instance()->pingMainloopTimeout("Display:RenderFlush");

        LLRenderTarget &rt = (gPipeline.sRenderDeferred ? gPipeline.mRT->deferredScreen : gPipeline.mRT->screen);
        rt.flush();

        if (LLPipeline::sRenderDeferred)
        {
            gPipeline.renderDeferredLighting();
        }

        LLPipeline::sUnderWaterRender = false;

        {
            //capture the frame buffer.
            LLSceneMonitor::getInstance()->capture();
        }

        LLAppViewer::instance()->pingMainloopTimeout("Display:RenderUI");
        if (!for_snapshot)
        {
            render_ui();
#if LL_DARWIN
            // Application-owned GL is still traversed during conversion, but
            // it is no longer a presentation fallback.
            gDisplaySwapBuffers = false;
#endif
            presented = gDisplaySwapBuffers;
            swap();
        }


        LLSpatialGroup::sNoDelete = false;
        gPipeline.clearReferences();
    }

    LLAppViewer::instance()->pingMainloopTimeout("Display:FrameStats");

    stop_glerror();

    // Close the frame's queries, lay this frame's fence and poll the frames before, and close out the UBO
    // streaming rings' per-frame counters (slices, wraps, GPU stalls) and let them resize. Straight
    // after the swap: the frame's one poll waits on a threaded driver's own thread here, if
    // anywhere, which is behind the swap already (see ALFrameFences). And here rather than in
    // resetFrameStats(): a resize deletes and remaps a ring and invalidates every outstanding slice,
    // which must land past this frame's draws, not before its heaviest passes. See
    // ALUniformBuffer::endFrame. A frame that reached the screen also waits for the one
    // AlchemyRenderMaxFramesAhead before it, every frame, so the CPU keeps an even pace that far
    // ahead of the GPU, with the main loop's work in that wait first; that end is the main loop's,
    // straight after display() returns (display_end_frame, ALFrameEnd). A capture's frames, and
    // every frame with pacing off, end here unpaced.
    const ALFrameEnd end = ALFrameEnd::make(false, presented, gPipeline.maxFramesAhead());
    if (end.mAfterDisplay)
    {
        sFrameEndOwed = end.mFramesAhead;
    }
    else
    {
        gGL.endFrame(end.mFramesAhead);
    }

    display_stats();

#if LL_DARWIN
    record_metal_benchmark_frame(
        for_snapshot, presented, benchmark_session, benchmark_observation);
#endif

    LLAppViewer::instance()->pingMainloopTimeout("Display:Done");

    gShiftFrame = false;

    if (gShaderProfileFrame)
    {
        gShaderProfileFrame = false;
        LLSD stats = LLSD::emptyMap();
        getProfileStatsContext(stats);
        LLGLSLShader::finishProfile(stats);

        auto report_name = getProfileStatsFilename();
        llofstream outf(report_name);
        if (! outf)
        {
            LL_WARNS() << "Couldn't write to " << std::quoted(report_name) << LL_ENDL;
        }
        else
        {
            outf << LlsdToJson(stats);
            LL_INFOS() << "(also dumped to " << std::quoted(report_name) << ")" << LL_ENDL;
        }
    }
}

void getProfileStatsContext(LLSD& stats)
{
    // populate the context with info from LLFloaterAbout
    LLSD& context = stats["context"];
    context = LLAppViewer::instance()->getViewerInfo();

    // then add a few more things
    unsigned char unique_id[MAC_ADDRESS_BYTES]{};
    LLMachineID::getUniqueID(unique_id, sizeof(unique_id));
    context["machine"] = stringize(LL::hexdump(unique_id, sizeof(unique_id)));
    context["grid"] = LLGridManager::instance().getGrid();
    LLViewerRegion* region = gAgent.getRegion();
    if (region)
    {
        context["regionid"] = stringize(region->getRegionID());
    }
    LLParcel* parcel = LLViewerParcelMgr::instance().getAgentParcel();
    if (parcel)
    {
        context["parcel"] = parcel->getName();
        context["parcelid"] = parcel->getLocalID();
    }
    context["time"] = LLDate::now().toHTTPDateString("%Y-%m-%dT%H:%M:%S");
}

std::string getProfileStatsFilename()
{
    std::ostringstream basebuff;
    // viewer build
    basebuff << "profile.v" << LLVersionInfo::instance().getBuild();
    // machine ID: zero-initialize unique_id in case LLMachineID fails
    unsigned char unique_id[MAC_ADDRESS_BYTES]{};
    LLMachineID::getUniqueID(unique_id, sizeof(unique_id));
    basebuff << ".m" << LL::hexdump(unique_id, sizeof(unique_id));
    // region ID
    LLViewerRegion *region = gAgent.getRegion();
    basebuff << ".r" << (region? region->getRegionID() : LLUUID());
    // local parcel ID
    LLParcel* parcel = LLViewerParcelMgr::instance().getAgentParcel();
    basebuff << ".p" << (parcel? parcel->getLocalID() : 0);
    // date/time -- omit seconds for now
    auto now = LLDate::now();
    basebuff << ".t" << LLDate::now().toHTTPDateString("%Y-%m-%dT%H-%M-");
    // put this candidate file in our logs directory
    auto base = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, basebuff.str());
    S32 sec;
    now.split(nullptr, nullptr, nullptr, nullptr, nullptr, &sec);
    // Loop over finished filename, incrementing sec until we find one that
    // doesn't yet exist. Should rarely loop (only if successive calls within
    // same second), may produce (e.g.) sec==61, but avoids collisions and
    // preserves chronological filename sort order.
    std::string name;
    do
    {
        // base + missing 2-digit seconds, append ".json"
        // post-increment sec in case we have to try again
        name = stringize(base, std::setw(2), std::setfill('0'), sec++, ".json");
    } while (LLFile::exists(name));
    return name;
}

// WIP simplified copy of display() that does minimal work
void display_cube_face()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Render Cube Face");
    LL_PROFILE_GPU_ZONE("display cube face");

    llassert(!gSnapshot);
    llassert(!gTeleportDisplay);
    llassert(LLStartUp::getStartupState() >= STATE_PRECACHE);
    llassert(!LLAppViewer::instance()->logoutRequestSent());
    bool rebuild = false;
#if LL_DARWIN
    const bool native_probe_face =
        gPipeline.isMetalProbeFaceCaptureActive();
#else
    constexpr bool native_probe_face = false;
#endif

    std::unique_ptr<LLGLSDefault> gls_default;
    std::unique_ptr<LLGLDepthTest> gls_depth;
    if (!native_probe_face)
    {
        gls_default = std::make_unique<LLGLSDefault>();
        gls_depth = std::make_unique<LLGLDepthTest>(
            GL_TRUE, GL_TRUE, GL_LEQUAL);
        LLVertexBuffer::unbind();
        gPipeline.disableLights();
    }

    gPipeline.mBackfaceCull = true;

    if (!native_probe_face)
    {
        gViewerWindow->setup3DViewport();
    }

    if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
    { //don't draw hud objects in this frame
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
    }

    if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES))
    { //don't draw hud particles in this frame
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
    }

    display_update_camera(native_probe_face);

    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Env Update");
        // update all the sky/atmospheric/water settings
        LLEnvironment::instance().update(LLViewerCamera::getInstance());
    }

    LLSpatialGroup::sNoDelete = true;

    S32 occlusion = LLPipeline::sUseOcclusion;
    LLPipeline::sUseOcclusion = 0; // occlusion data is from main camera point of view, don't read or write it during cube snapshots
    //gDepthDirty = true; //let "real" render pipe know it can't trust the depth buffer for occlusion data

    static LLCullResult result;
    LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
    LLPipeline::sUnderWaterRender = LLViewerCamera::getInstance()->cameraUnderWater();
    gPipeline.updateCull(*LLViewerCamera::getInstance(), result);

    if (!native_probe_face)
    {
        gGL.setColorMask(true, true);
        gGL.setClearColor(LLColor4::transparent);
    }
#if LL_DARWIN
    if (native_probe_face)
    {
        // Establish the native transaction before its directional shadows.
        // This also resolves the Metal renderer/device and planned capture
        // dimensions without consulting the retained GL G-buffer target.
        gPipeline.beginMetalWorldGBufferCapture();
    }
#endif
    gPipeline.generateSunShadow(*LLViewerCamera::getInstance());

    if (!native_probe_face)
    {
        gGL.clear(GL_DEPTH_BUFFER_BIT); // | GL_STENCIL_BUFFER_BIT);
    }

    {
        LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
        gPipeline.stateSort(*LLViewerCamera::getInstance(), result);

        if (rebuild)
        {
            //////////////////////////////////////
            //
            // rebuildPools
            //
            //
            gPipeline.rebuildPools();
            stop_glerror();
        }
    }

    LLPipeline::sUseOcclusion = occlusion;

    LLAppViewer::instance()->pingMainloopTimeout("Display:RenderStart");

    LLPipeline::sUnderWaterRender = LLViewerCamera::getInstance()->cameraUnderWater();

    if (!native_probe_face)
    {
        gGL.setColorMask(true, true);
        gPipeline.mRT->deferredScreen.bindTarget();
        if (gUseWireframe)
        {
            gGL.setClearColor(LLColor4(0.5f, 0.5f, 0.5f, 1.f));
        }
        else
        {
            gGL.setClearColor(LLColor4::magenta);
        }
        gPipeline.mRT->deferredScreen.clear();
    }

    LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

    gPipeline.renderGeomDeferred(*LLViewerCamera::getInstance());

    if (!native_probe_face)
    {
        gPipeline.mRT->deferredScreen.flush();
    }

    gPipeline.renderDeferredLighting();

    LLPipeline::sUnderWaterRender = false;

    // Finalize scene
    //gPipeline.renderFinalize();

    LLSpatialGroup::sNoDelete = false;
    gPipeline.clearReferences();
}

// The viewport pixels the HUD can cover, a rect per attachment point with something on it, overlapping ones merged
// (ALHUDDepth::mergeRect). A point's box is in the frame the HUD's model matrix takes in (LLVOAvatar::getHUDBBox),
// with no rotation or offset of its own.
static void hud_screen_rects(std::vector<LLRect>& rects)
{
    rects.clear();
    // None without what restore_scene_depth puts the scene's depth back from.
    LLMatrix4a proj;
    LLMatrix4a model;
    if (!gPipeline.mRT || !gPipeline.mScreenTriangleVB || !isAgentAvatarValid() || !get_hud_matrices(proj, model))
    {
        return;
    }

    LLMatrix4a mvp;
    mvp.setMul(model, proj);

    for (const auto& [id, attachment] : gAgentAvatarp->mAttachmentPoints)
    {
        if (!attachment || !attachment->getIsHUDAttachment() || attachment->mAttachedObjects.empty())
        {
            continue;
        }
        const LLBBox bbox = LLVOAvatar::getHUDBBox(attachment);
        const LLRect rect = ALHUDDepth::boxRect(mvp, bbox.getMinLocal(), bbox.getMaxLocal(), gGLViewport[2], gGLViewport[3]);
        if (!rect.isEmpty())
        {
            ALHUDDepth::mergeRect(rects, rect);
        }
    }
}

// Writes the scene's depth back under the HUD, as the final blit left it (ALHUDDepth::restore), within `rects`, which
// hud_screen_rects gives only where it can.
static void restore_scene_depth(const std::vector<LLRect>& rects)
{
    if (rects.empty())
    {
        return;
    }
    ALHUDDepth::restore(gCopyDepthProgram, gPipeline.mRT->deferredScreen, *gPipeline.mScreenTriangleVB, rects,
                        gGLViewport[0], gGLViewport[1]);
}

void render_hud_attachments()
{
#if LL_DARWIN
    std::optional<ALMetalBenchmarkRecorder::ViewState> before_hud;
    if (ALQueryPool::diagnosticsEnabled() && LLPipeline::isMainWorldPass())
        before_hud = benchmark_view_state();
#endif
    LLPerfStats::RecordSceneTime T ( LLPerfStats::StatType_t::RENDER_HUDS); // render time capture - Primary contributor to HUDs (though these end up in render batches)
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.pushMatrix();
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.pushMatrix();

    const LLCamera saved_camera = LLViewerCamera::getCurrent();

    // clamp target zoom level to reasonable values
//  gAgentCamera.mHUDTargetZoom = llclamp(gAgentCamera.mHUDTargetZoom, 0.1f, 1.f);
// [RLVa:KB] - Checked: 2010-08-22 (RLVa-1.2.1a) | Modified: RLVa-1.0.0c
    gAgentCamera.mHUDTargetZoom = llclamp(gAgentCamera.mHUDTargetZoom, (!gRlvAttachmentLocks.hasLockedHUD()) ? 0.1f : 0.85f, 1.f);
// [/RLVa:KB]

    // smoothly interpolate current zoom level
    gAgentCamera.mHUDCurZoom = lerp(gAgentCamera.mHUDCurZoom, gAgentCamera.getAgentHUDTargetZoom(), LLSmoothInterpolation::getInterpolant(0.03f));

    if (LLPipeline::sShowHUDAttachments && !gDisconnected && setup_hud_matrices())
    {
        LLPipeline::sRenderingHUDs = true;
        LLCamera hud_cam = LLViewerCamera::getCurrent();

        static LLCachedControl<bool> render_hud_particles(gSavedSettings, "RenderHUDParticles", false);
        bool render_particles = gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES) && render_hud_particles;

        //only render hud objects
        gPipeline.pushRenderTypeMask();

        // turn off everything
        gPipeline.andRenderTypeMask(LLPipeline::END_RENDER_TYPES);
        // turn on HUD
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
        // turn on HUD particles
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);

        // if particles are off, turn off hud-particles as well
        if (!render_particles)
        {
            // turn back off HUD particles
            gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
        }

        bool has_ui = gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);
        if (has_ui)
        {
            gPipeline.toggleRenderDebugFeature(LLPipeline::RENDER_DEBUG_FEATURE_UI);
        }

        S32 use_occlusion = LLPipeline::sUseOcclusion;
        LLPipeline::sUseOcclusion = 0;

        //cull, sort, and render hud objects
        static LLCullResult result;
        LLSpatialGroup::sNoDelete = true;

        LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
        gPipeline.updateCull(hud_cam, result);

        // Toggle render types
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_BUMP);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_SIMPLE);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_VOLUME);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_ALPHA);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_ALPHA_PRE_WATER);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_ALPHA_MASK);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_FULLBRIGHT_ALPHA_MASK);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_FULLBRIGHT);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_GLTF_PBR);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_GLTF_PBR_ALPHA_MASK);

        // Toggle render passes
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_ALPHA);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_BUMP);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_MATERIAL);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_SHINY);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_INVISIBLE);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_INVISI_SHINY);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_GLTF_PBR);
        gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_GLTF_PBR_ALPHA_MASK);

        gPipeline.stateSort(hud_cam, result);

        // The HUD is drawn over the window's framebuffer, whose depth the final blit filled with the scene's for the
        // 3D UI around it. get_hud_matrices squeezes the HUD into a band of that depth in front of the scene, but a
        // world surface at the near plane still reached into it, and the band widens with the HUD's own depth: a
        // HUD with a prim parked far along its axis put its rear within reach of surfaces half a metre away. So
        // under the HUD the depth is cleared, the HUD tests against itself alone, and the scene's is put back after
        // it for the 3D UI that follows, which the world and the HUD both hide.
        static std::vector<LLRect> hud_rects;
        hud_screen_rects(hud_rects);
        ALHUDDepth::clear(gCopyDepthProgram, hud_rects, gGLViewport[0], gGLViewport[1]);

        gPipeline.renderGeomPostDeferred(hud_cam);

        LLSpatialGroup::sNoDelete = false;
        //gPipeline.clearReferences();

        render_hud_elements();

        restore_scene_depth(hud_rects);

        //restore type mask
        gPipeline.popRenderTypeMask();

        if (has_ui)
        {
            gPipeline.toggleRenderDebugFeature(LLPipeline::RENDER_DEBUG_FEATURE_UI);
        }
        LLPipeline::sUseOcclusion = use_occlusion;
        LLPipeline::sRenderingHUDs = false;
    }
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.popMatrix();
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.popMatrix();

    LLViewerCamera::setCurrent(saved_camera);
#if LL_DARWIN
    if (before_hud) benchmark_camera_restoration(
        sMetalBenchmarkProvenance.hud_restoration, *before_hud);
#endif
}

LLRect get_whole_screen_region()
{
    LLRect whole_screen = gViewerWindow->getWorldViewRectScaled();

    // apply camera zoom transform (for high res screenshots)
    F32 zoom_factor = LLViewerCamera::getInstance()->getZoomFactor();
    S16 sub_region = LLViewerCamera::getInstance()->getZoomSubRegion();
    if (zoom_factor > 1.f)
    {
        S32 num_horizontal_tiles = llceil(zoom_factor);
        S32 tile_width = ll_round((F32)gViewerWindow->getWorldViewWidthScaled() / zoom_factor);
        S32 tile_height = ll_round((F32)gViewerWindow->getWorldViewHeightScaled() / zoom_factor);
        int tile_y = sub_region / num_horizontal_tiles;
        int tile_x = sub_region - (tile_y * num_horizontal_tiles);

        whole_screen.setLeftTopAndSize(tile_x * tile_width, gViewerWindow->getWorldViewHeightScaled() - (tile_y * tile_height), tile_width, tile_height);
    }
    return whole_screen;
}

bool get_hud_matrices(const LLRect& screen_region, LLMatrix4a& proj, LLMatrix4a& model)
{
    if (isAgentAvatarValid() && gAgentAvatarp->hasHUDAttachment())
    {
        F32 zoom_level = gAgentCamera.mHUDCurZoom;
        LLBBox hud_bbox = gAgentAvatarp->getHUDBBox();

        F32 hud_depth = llmax(1.f, hud_bbox.getExtentLocal().mV[VX] * 1.1f);
        // al_ortho reverses the depth row under reverse-Z (its [3][2] becomes +1), so only
        // the flatten-hack [2][2] constant mirrors: window = 1 + 0.005*z == 1 - window_fwd,
        // same per-unit spacing and clip budget, reversed direction.
        proj = al_ortho(-0.5f * LLViewerCamera::getInstance()->getAspect(), 0.5f * LLViewerCamera::getInstance()->getAspect(), -0.5f, 0.5f, 0.f, hud_depth);
        proj.mMatrix[2].getF32ptr()[2] = LLRender::sReverseZ ? 0.005f : -0.01f;

        F32 aspect_ratio = LLViewerCamera::getInstance()->getAspect();

        F32 scale_x = (F32)gViewerWindow->getWorldViewWidthScaled() / (F32)screen_region.getWidth();
        F32 scale_y = (F32)gViewerWindow->getWorldViewHeightScaled() / (F32)screen_region.getHeight();

        // the region scaled up to the view, then moved to its centre, ahead of the ortho
        LLMatrix4a mat;
        mat.setMul(LLMatrix4a::scaling(scale_x, scale_y, 1.f),
                   LLMatrix4a::translation(clamp_rescale((F32)(screen_region.getCenterX() - screen_region.mLeft), 0.f, (F32)gViewerWindow->getWorldViewWidthScaled(), 0.5f * scale_x * aspect_ratio, -0.5f * scale_x * aspect_ratio),
                                           clamp_rescale((F32)(screen_region.getCenterY() - screen_region.mBottom), 0.f, (F32)gViewerWindow->getWorldViewHeightScaled(), 0.5f * scale_y, -0.5f * scale_y),
                                           0.f));
        proj.setMul(mat, proj);

        // the HUD zoomed and moved in front of the camera, then into the GL frame
        mat.setMul(LLMatrix4a::scaling(zoom_level, zoom_level, zoom_level),
                   LLMatrix4a::translation(-hud_bbox.getCenterLocal().mV[VX] + (hud_depth * 0.5f), 0.f, 0.f));
        model.setMul(mat, LLMatrix4a(OGL_TO_CFR_ROTATION));

        return true;
    }
    else
    {
        return false;
    }
}

bool get_hud_matrices(LLMatrix4a& proj, LLMatrix4a& model)
{
    LLRect whole_screen = get_whole_screen_region();
    return get_hud_matrices(whole_screen, proj, model);
}

bool setup_hud_matrices()
{
    LLRect whole_screen = get_whole_screen_region();
    return setup_hud_matrices(whole_screen);
}

bool setup_hud_matrices(const LLRect& screen_region)
{
    LLMatrix4a proj, model;
    bool result = get_hud_matrices(screen_region, proj, model);
    if (!result) return result;

    // set up transform to keep HUD objects in front of camera
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.loadMatrix(proj);

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.loadMatrix(model);

    // the HUD's camera: at the HUD's origin looking along +x, with the
    // HUD's matrices and the frustum they give
    LLCamera hud_cam = *LLViewerCamera::getInstance();
    hud_cam.setOrigin(-1.f, 0.f, 0.f);
    hud_cam.setAxes(LLVector3(1.f, 0.f, 0.f), LLVector3(0.f, 1.f, 0.f), LLVector3(0.f, 0.f, 1.f));
    hud_cam.setProjection(proj);
    hud_cam.setModelview(model);
    LLViewerCamera::updateFrustumPlanes(hud_cam, true);
    LLViewerCamera::setCurrent(hud_cam);
    return true;
}

static void render_ui_over_world();

void render_ui(F32 zoom_factor, int subfield)
{
    LLPerfStats::RecordSceneTime T ( LLPerfStats::StatType_t::RENDER_UI ); // render time capture - Primary UI stat can have HUD time overlap (TODO)
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    LL_PROFILE_GPU_ZONE("ui");
    LLGLState::checkStates();

    // the world camera, as the scene just rendered with it
    const LLCamera saved_camera = LLViewerCamera::getCurrent();

    if (!gSnapshot)
    {
        gGL.pushMatrix();
        gGL.loadMatrix(LLViewerCamera::getInstance()->getModelview());
        LLViewerCamera::setCurrent(*LLViewerCamera::getInstance());
    }

    if(LLSceneMonitor::getInstance()->needsUpdate())
    {
        gGL.pushMatrix();
        gViewerWindow->setup2DRender();
        LLSceneMonitor::getInstance()->compare();
        gViewerWindow->setup3DRender();
        gGL.popMatrix();
    }

    // apply gamma correction and post effects
    gPipeline.renderFinalize();

    // A cutout frame is its subject and nothing else. Nothing drawn over the
    // frame belongs to that, and the coverage the subject is lifted out by
    // knows nothing of what would be drawn here.
    if (!LLPipeline::sCutoutRender)
    {
#if LL_DARWIN
        const bool metal_world_ui_probe =
            gPipeline.beginMetalWorldUICompositionCapture();
#endif
        render_ui_over_world();
#if LL_DARWIN
        if (metal_world_ui_probe)
        {
            // Retire any final immediate-mode batch while the offscreen
            // capture is still active. GL still receives every draw.
            gGL.flush();
            gPipeline.endMetalWorldUICompositionCapture();
        }
#endif
    }

    if (!gSnapshot)
    {
        LLViewerCamera::setCurrent(saved_camera);
        gGL.popMatrix();
    }
}

// Everything render_ui draws over the finished world frame: HUD elements and
// attachments, the interface, and the debug text.
static void render_ui_over_world()
{
    {
        LLGLState::checkStates();


        LL_PROFILE_ZONE_NAMED_CATEGORY_UI("HUD");
        render_hud_elements();
        LLGLState::checkStates();
// [RLVa:KB] - Checked: RLVa-2.2 (@setoverlay)
        if (RlvActions::hasBehaviour(RLV_BHVR_SETOVERLAY))
        {
            LLVfxManager::instance().runEffect(EVisualEffect::RlvOverlay);
        }
// [/RLVa:KB]
        render_hud_attachments();

        LLGLState::checkStates();

        LLGLSDefault gls_default;
        LLGLSUIDefault gls_ui;
        {
            gPipeline.disableLights();
        }

        bool render_ui = gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);
        if (render_ui)
        {
            if (!gDisconnected)
            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_UI("UI 3D");
                LLGLState::checkStates();
                render_ui_3d();
                LLGLState::checkStates();
            }
            else
            {
                render_disconnected_background();
            }
        }
        else
        {
            // Make sure particle effects disappear
            LLHUDObject::renderAllForTimer();
        }

        if (render_ui)
        {
            LL_PROFILE_ZONE_NAMED_CATEGORY_UI("UI 2D");
            LLHUDObject::renderAll();
            render_ui_2d();
        }

        gViewerWindow->setup2DRender();
        gViewerWindow->updateDebugText();
        gViewerWindow->drawDebugText();
    }
}

void swap()
{
    LLPerfStats::RecordSceneTime T ( LLPerfStats::StatType_t::RENDER_SWAP ); // render time capture - Swap buffer time - can signify excessive data transfer to/from GPU
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Swap");
    LL_PROFILE_GPU_ZONE("swap");
    bool swap_gl_drawable = gDisplaySwapBuffers;
#if LL_DARWIN
    if (LLStartUp::getStartupState() >= STATE_PRECACHE)
    {
        swap_gl_drawable = false;
    }
#endif
    if (swap_gl_drawable)
    {
        gViewerWindow->getWindow()->swapBuffers();
    }
    gDisplaySwapBuffers = true;
}

void renderCoordinateAxes()
{
    gGL.getTextureSlot(0)->unbind();
    gGL.begin(LLRender::LINES);
        gGL.color3f(1.0f, 0.0f, 0.0f);   // i direction = X-Axis = red
        gGL.vertex3f(0.0f, 0.0f, 0.0f);
        gGL.vertex3f(2.0f, 0.0f, 0.0f);
        gGL.vertex3f(3.0f, 0.0f, 0.0f);
        gGL.vertex3f(5.0f, 0.0f, 0.0f);
        gGL.vertex3f(6.0f, 0.0f, 0.0f);
        gGL.vertex3f(8.0f, 0.0f, 0.0f);
        // Make an X
        gGL.vertex3f(11.0f, 1.0f, 1.0f);
        gGL.vertex3f(11.0f, -1.0f, -1.0f);
        gGL.vertex3f(11.0f, 1.0f, -1.0f);
        gGL.vertex3f(11.0f, -1.0f, 1.0f);

        gGL.color3f(0.0f, 1.0f, 0.0f);   // j direction = Y-Axis = green
        gGL.vertex3f(0.0f, 0.0f, 0.0f);
        gGL.vertex3f(0.0f, 2.0f, 0.0f);
        gGL.vertex3f(0.0f, 3.0f, 0.0f);
        gGL.vertex3f(0.0f, 5.0f, 0.0f);
        gGL.vertex3f(0.0f, 6.0f, 0.0f);
        gGL.vertex3f(0.0f, 8.0f, 0.0f);
        // Make a Y
        gGL.vertex3f(1.0f, 11.0f, 1.0f);
        gGL.vertex3f(0.0f, 11.0f, 0.0f);
        gGL.vertex3f(-1.0f, 11.0f, 1.0f);
        gGL.vertex3f(0.0f, 11.0f, 0.0f);
        gGL.vertex3f(0.0f, 11.0f, 0.0f);
        gGL.vertex3f(0.0f, 11.0f, -1.0f);

        gGL.color3f(0.0f, 0.0f, 1.0f);   // Z-Axis = blue
        gGL.vertex3f(0.0f, 0.0f, 0.0f);
        gGL.vertex3f(0.0f, 0.0f, 2.0f);
        gGL.vertex3f(0.0f, 0.0f, 3.0f);
        gGL.vertex3f(0.0f, 0.0f, 5.0f);
        gGL.vertex3f(0.0f, 0.0f, 6.0f);
        gGL.vertex3f(0.0f, 0.0f, 8.0f);
        // Make a Z
        gGL.vertex3f(-1.0f, 1.0f, 11.0f);
        gGL.vertex3f(1.0f, 1.0f, 11.0f);
        gGL.vertex3f(1.0f, 1.0f, 11.0f);
        gGL.vertex3f(-1.0f, -1.0f, 11.0f);
        gGL.vertex3f(-1.0f, -1.0f, 11.0f);
        gGL.vertex3f(1.0f, -1.0f, 11.0f);
    gGL.end();
}


void draw_axes()
{
    LLGLSUIDefault gls_ui;
    gGL.getTextureSlot(0)->unbind();
    // A vertical white line at origin
    LLVector3 v = gAgent.getPositionAgent();
    gGL.begin(LLRender::LINES);
        gGL.color3f(1.0f, 1.0f, 1.0f);
        gGL.vertex3f(0.0f, 0.0f, 0.0f);
        gGL.vertex3f(0.0f, 0.0f, 40.0f);
    gGL.end();
    // Some coordinate axes
    gGL.pushMatrix();
        gGL.translatef( v.mV[VX], v.mV[VY], v.mV[VZ] );
        renderCoordinateAxes();
    gGL.popMatrix();
}

void render_ui_3d()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    LLGLSPipeline gls_pipeline;

    //////////////////////////////////////
    //
    // Render 3D UI elements
    // NOTE: zbuffer is cleared before we get here by LLDrawPoolHUD,
    //       so 3d elements requiring Z buffer are moved to LLDrawPoolHUD
    //

    /////////////////////////////////////////////////////////////
    //
    // Render 2.5D elements (2D elements in the world)
    // Stuff without z writes
    //

    // Debugging stuff goes before the UI.

    stop_glerror();

    gUIProgram.bind();
    gGL.color4f(1.f, 1.f, 1.f, 1.f);

    // Coordinate axes
    static LLCachedControl<bool> show_axes(gSavedSettings, "ShowAxes");
    if (show_axes())
    {
        draw_axes();
    }

    gViewerWindow->renderSelections(false, false, true); // Non HUD call in render_hud_elements

    if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
    {
        // Render debugging beacons.
        gObjectList.renderObjectBeacons();
        gObjectList.resetObjectBeacons();
        gSky.addSunMoonBeacons();
    }
    else
    {
        // Make sure particle effects disappear
        LLHUDObject::renderAllForTimer();
    }

    stop_glerror();
}

void render_ui_2d()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    LLGLSUIDefault gls_ui;

    /////////////////////////////////////////////////////////////
    //
    // Render 2D UI elements that overlay the world (no z compare)

    //  Disable wireframe mode below here, as this is HUD/menus
    gGL.setPolygonMode(GL_FILL);

    //  Menu overlays, HUD, etc
    gViewerWindow->setup2DRender();

    F32 zoom_factor = LLViewerCamera::getInstance()->getZoomFactor();
    S16 sub_region = LLViewerCamera::getInstance()->getZoomSubRegion();

    if (zoom_factor > 1.f)
    {
        //decompose subregion number to x and y values
        int pos_y = sub_region / llceil(zoom_factor);
        int pos_x = sub_region - (pos_y*llceil(zoom_factor));
        // offset for this tile
        LLFontGL::sCurOrigin.mX -= ll_round((F32)gViewerWindow->getWindowWidthScaled() * (F32)pos_x / zoom_factor);
        LLFontGL::sCurOrigin.mY -= ll_round((F32)gViewerWindow->getWindowHeightScaled() * (F32)pos_y / zoom_factor);
    }

    stop_glerror();

    // render outline for HUD
    if (isAgentAvatarValid() && gAgentCamera.mHUDCurZoom < 0.98f)
    {
        gUIProgram.bind();
        gGL.pushMatrix();
        S32 half_width = (gViewerWindow->getWorldViewWidthScaled() / 2);
        S32 half_height = (gViewerWindow->getWorldViewHeightScaled() / 2);
        gGL.scalef(LLUI::getScaleFactor().mV[VX], LLUI::getScaleFactor().mV[VY], 1.f);
        gGL.translatef((F32)half_width, (F32)half_height, 0.f);
        F32 zoom = gAgentCamera.mHUDCurZoom;
        gGL.scalef(zoom,zoom,1.f);
        gGL.color4fv(LLColor4::white.mV);
        gl_rect_2d(-half_width, half_height, half_width, -half_height, false);
        gGL.popMatrix();
        gUIProgram.unbind();
        stop_glerror();
    }

    gViewerWindow->draw();

    // reset current origin for font rendering, in case of tiling render
    LLFontGL::sCurOrigin.set(0, 0);
}

void render_disconnected_background()
{
    gUIProgram.bind();

    gGL.color4f(1.f, 1.f, 1.f, 1.f);
    if (!gDisconnectedImagep && gDisconnected)
    {
        LL_INFOS() << "Loading last bitmap..." << LL_ENDL;

        std::string temp_str;
        temp_str = gDirUtilp->getLindenUserDir() + gDirUtilp->getDirDelimiter() + LLStartUp::getScreenLastFilename();

        LLPointer<LLImagePNG> image_png = new LLImagePNG;
        if( !image_png->load(temp_str) )
        {
            //LL_INFOS() << "Bitmap load failed" << LL_ENDL;
            return;
        }

        LLPointer<LLImageRaw> raw = new LLImageRaw;
        if (!image_png->decode(raw, 0.0f))
        {
            LL_INFOS() << "Bitmap decode failed" << LL_ENDL;
            gDisconnectedImagep = NULL;
            return;
        }

        U8 *rawp = raw->getData();
        S32 npixels = (S32)image_png->getWidth()*(S32)image_png->getHeight();
        for (S32 i = 0; i < npixels; i++)
        {
            S32 sum = 0;
            sum = *rawp + *(rawp+1) + *(rawp+2);
            sum /= 3;
            *rawp = ((S32)sum*6 + *rawp)/7;
            rawp++;
            *rawp = ((S32)sum*6 + *rawp)/7;
            rawp++;
            *rawp = ((S32)sum*6 + *rawp)/7;
            rawp++;
        }


        raw->expandToPowerOfTwo();
        gDisconnectedImagep = LLViewerTextureManager::getLocalTexture(raw.get(), false);
        gStartTexture = gDisconnectedImagep;
        gGL.getTextureSlot(0)->unbind();
    }

    // Make sure the progress view always fills the entire window.
    S32 width = gViewerWindow->getWindowWidthScaled();
    S32 height = gViewerWindow->getWindowHeightScaled();

    if (gDisconnectedImagep)
    {
        LLGLSUIDefault gls_ui;
        gViewerWindow->setup2DRender();
        gGL.pushMatrix();
        {
            // scale ui to reflect UIScaleFactor
            // this can't be done in setup2DRender because it requires a
            // pushMatrix/popMatrix pair
            const LLVector2& display_scale = gViewerWindow->getDisplayScale();
            gGL.scalef(display_scale.mV[VX], display_scale.mV[VY], 1.f);

            gGL.getTextureSlot(0)->bindSampled(gDisconnectedImagep, ALSamplers::AnisoWrap);
            gGL.color4f(1.f, 1.f, 1.f, 1.f);
            gl_rect_2d_simple_tex(width, height);
            gGL.getTextureSlot(0)->unbind();
        }
        gGL.popMatrix();
    }
    gGL.flush();

    gUIProgram.unbind();
}

void display_cleanup()
{
    gDisconnectedImagep = nullptr;
}
