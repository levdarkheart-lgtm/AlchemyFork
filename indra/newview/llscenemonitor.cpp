/**
 * @file llscenemonitor.cpp
 * @brief monitor the scene loading process.
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
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
#include "llscenemonitor.h"
#include "llviewerwindow.h"
#include "llviewerdisplay.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"
#include "llglstates.h"
#include "llui.h"
#include "llstartup.h"
#include "llappviewer.h"
#include "llwindow.h"
#include "llpointer.h"
#include "llspatialpartition.h"
#include "llagent.h"
#include "pipeline.h"
#include "llviewerparcelmgr.h"
#include "llviewerpartsim.h"

#include <utility>

LLSceneMonitorView* gSceneMonitorView = NULL;

// Keep the original cadence, avatar/time freeze and trace recording. Native
// capture takes only this frame's completed pre-UI window backing; comparison
// and filtering run on their own GPU command, with nonblocking result polling.
LLSceneMonitor::LLSceneMonitor() = default;

LLSceneMonitor::~LLSceneMonitor()
{
    mDiffState = VIEWER_QUITTING;
    reset();
}

void LLSceneMonitor::reset()
{
    mFrames = {};
    mPendingDiff.reset();
    mDiff.reset();
    mNativeMonitor.reset();
    mNativeDevice = nullptr;
    mCaptureWidth = mCaptureHeight = 0;
    mLastCaptureFrame = ~U32(0);
    mForceCapture = true;
    if (mDiffState != VIEWER_QUITTING) mDiffState = WAITING_FOR_NEXT_DIFF;
    mMonitorRecording.reset();
    mSceneLoadRecording.reset();
    mRecordingTimer.reset();
    unfreezeScene();
    // Teleport resets call this without toggling mEnabled. Reacquire pauses
    // for the current scene rather than leaving an active monitor unfrozen.
    if (mEnabled && mDiffState != VIEWER_QUITTING) freezeScene();
}

void LLSceneMonitor::setDebugViewerVisible(bool visible)
{
    mDebugViewerVisible = visible;
}

void LLSceneMonitor::freezeAvatar(LLCharacter* avatarp)
{
    if(mEnabled)
    {
        mAvatarPauseHandles.push_back(avatarp->requestPause());
    }
}

namespace
{
constexpr std::array<U32, 4> frozen_render_types{
    LLPipeline::RENDER_TYPE_SKY, LLPipeline::RENDER_TYPE_WL_SKY,
    LLPipeline::RENDER_TYPE_WATER, LLPipeline::RENDER_TYPE_CLOUDS};
}

void LLSceneMonitor::freezeScene()
{
    if (!mEnabled || mSceneFrozen) return;
    mPreviousFreezeTime = gSavedSettings.getBOOL("FreezeTime");
    mPreviousParticleLimit = LLViewerPartSim::getMaxPartCount();
    for (size_t i = 0; i < frozen_render_types.size(); ++i)
        mPreviousRenderTypes[i] = gPipeline.hasRenderType(frozen_render_types[i]);
    mSceneFrozen = true;
    for (LLCharacter* character : LLCharacter::sInstances) freezeAvatar(character);
    gSavedSettings.setBOOL("FreezeTime", true);
    gPipeline.clearRenderTypeMask(LLPipeline::RENDER_TYPE_SKY, LLPipeline::RENDER_TYPE_WL_SKY,
        LLPipeline::RENDER_TYPE_WATER, LLPipeline::RENDER_TYPE_CLOUDS, LLPipeline::END_RENDER_TYPES);
    LLViewerPartSim::getInstance()->enable(false);
}

void LLSceneMonitor::unfreezeScene()
{
    mAvatarPauseHandles.clear();
    if (!mSceneFrozen) return;
    mSceneFrozen = false;
    if (mDiffState == VIEWER_QUITTING) return;
    gSavedSettings.setBOOL("FreezeTime", mPreviousFreezeTime);
    for (size_t i = 0; i < frozen_render_types.size(); ++i)
    {
        if (mPreviousRenderTypes[i])
            gPipeline.setRenderTypeMask(frozen_render_types[i], LLPipeline::END_RENDER_TYPES);
        else
            gPipeline.clearRenderTypeMask(frozen_render_types[i], LLPipeline::END_RENDER_TYPES);
    }
    LLViewerPartSim::setMaxPartCount(mPreviousParticleLimit);
}

void LLSceneMonitor::capture()
{
    static LLCachedControl<bool> monitor_enabled(gSavedSettings, "SceneLoadingMonitorEnabled");
    static LLCachedControl<F32> scene_load_sample_time(gSavedSettings, "SceneLoadingMonitorSampleTime");
    const bool enabled = monitor_enabled || mDebugViewerVisible;
    if (mEnabled != enabled)
    {
        mEnabled = enabled;
        reset();
    }
    if (!mEnabled) return;
    if (mMonitorRecording.getSum(*LLViewerCamera::getVelocityStat()) > 0.1f ||
        mMonitorRecording.getSum(*LLViewerCamera::getAngularVelocityStat()) > 0.05f)
    {
        reset();
    }
    if ((!mForceCapture && mRecordingTimer.getElapsedTimeF32() <= scene_load_sample_time()) ||
        mLastCaptureFrame == gFrameCount || mDiffState == WAIT_ON_RESULT) return;

    auto frame = gPipeline.acquireMetalSceneMonitorFrame();
    if (!frame) return; // No prior-frame/cleared/loading background substitutes.
    const U32 width = gViewerWindow->getWorldViewWidthRaw();
    const U32 height = gViewerWindow->getWorldViewHeightRaw();
    if (width == 0 || height == 0 || width > frame->context.width || height > frame->context.height) return;
    if ((mFrames[1] && !(mFrames[1]->context == frame->context)) ||
        mCaptureWidth != width || mCaptureHeight != height)
    {
        mFrames = {};
        mDiff.reset();
    }
    mCaptureWidth = width; mCaptureHeight = height;
    mFrames[0] = std::move(mFrames[1]);
    mFrames[1] = std::move(frame);
    mForceCapture = false;
    mLastCaptureFrame = gFrameCount;
    mSceneLoadRecording.resume();
    mMonitorRecording.resume();
    mDiffState = NEED_DIFF;
}

bool LLSceneMonitor::needsUpdate() const { return mDiffState == NEED_DIFF; }

void LLSceneMonitor::compare()
{
    if (mDiffState != NEED_DIFF) return;
    mDiffState = WAITING_FOR_NEXT_DIFF;
    if (!mFrames[0] || !mFrames[1]) return;
    ALMetalDevice* device = getViewerMetalDevice();
    if (!device || device != mFrames[1]->context.device) return;
    std::string error;
    if (!mNativeMonitor || mNativeDevice != device)
    {
        mNativeMonitor = ALMetalSceneMonitor::create(*device, &error);
        mNativeDevice = device;
    }
    ALMetalSceneDifferenceConfig config;
    config.capture_width = mCaptureWidth; config.capture_height = mCaptureHeight;
    config.diff_width = gViewerWindow->getWindowWidthRaw();
    config.diff_height = gViewerWindow->getWindowHeightRaw();
    config.sample_ratio = mDiffPixelRatio; config.tolerance = mDiffTolerance;
    if (mNativeMonitor)
        mPendingDiff = mNativeMonitor->submit(mFrames[0], mFrames[1], config, &error);
    if (!mPendingDiff)
    {
        LL_WARNS_ONCE("SceneMonitor") << "Native scene difference is unavailable: " << error << LL_ENDL;
        return;
    }
    mDiffState = WAIT_ON_RESULT;
}

// The native computation is independent of showing its debug image. The
// floater only draws the latest completed, filtered result through ordinary UI.
void LLSceneMonitor::calcDiffAggregate()
{
    const ALMetalTexture* texture = getDiffTarget();
    if (!texture) return;
    LLGLDepthTest depth(true, false, GL_ALWAYS);
    LLGLSLShader* previous = LLGLSLShader::sCurBoundShaderPtr;
    gUIProgram.bind();
    auto* slot = gGL.getTextureSlot(0);
    if (slot->bindNative(const_cast<ALMetalTexture*>(texture), ALTextureSlot::TT_TEXTURE,
                         ALSamplers::PointClamp))
    {
        gGL.color4f(1.f, 1.f, 1.f, 1.f);
        gl_rect_2d_simple_tex(texture->width(), texture->height());
        gGL.flush();
    }
    slot->unbind();
    if (previous) previous->bind(); else gUIProgram.unbind();
}

static LLTrace::EventStatHandle<> sFramePixelDiff("FramePixelDifference");
void LLSceneMonitor::fetchQueryResult()
{
    if (mDiffState != WAIT_ON_RESULT || !mPendingDiff || LLAppViewer::instance()->quitRequested()) return;
    if (mPendingDiff->status() == ALMetalSceneDifference::Status::pending) return;
    U64 count = 0;
    F32 difference = 0.f;
    std::string error;
    mDiffState = WAITING_FOR_NEXT_DIFF;
    if (!mPendingDiff->result(count, difference, &error))
    {
        mPendingDiff.reset();
        LL_WARNS("SceneMonitor") << "Native scene difference failed: " << error << LL_ENDL;
        return; // An unavailable measurement must never be recorded as zero.
    }
    mDiffResult = difference;
    mDiff = std::move(mPendingDiff);
    LL_DEBUGS("SceneMonitor") << "Frame difference: " << mDiffResult << LL_ENDL;
    record(sFramePixelDiff, mDiffResult);
    static LLCachedControl<F32> sample_time_control(gSavedSettings, "SceneLoadingMonitorSampleTime");
    static LLCachedControl<F32> diff_threshold(gSavedSettings, "SceneLoadingMonitorPixelDiffThreshold");
    const F32Seconds sample_time = (F32Seconds)sample_time_control();
    if (mRecordingTimer.getElapsedTimeF32() > sample_time)
    {
        if (mDiffResult > diff_threshold())
        {
            mSceneLoadRecording.extend();
            llassert_always(mSceneLoadRecording.getResults().getLastRecording().getDuration() > sample_time);
        }
        else mSceneLoadRecording.nextPeriod();
        mRecordingTimer.reset();
    }
}

//dump results to a file _scene_xmonitor_results.csv
void LLSceneMonitor::dumpToFile(const std::string &file_name)
{
    if (!hasResults()) return;

    LL_INFOS("SceneMonitor") << "Saving scene load stats to " << file_name << LL_ENDL;
    try
    {
        llofstream os(file_name.c_str());

        os << std::setprecision(10);

        LLTrace::PeriodicRecording& scene_load_recording = mSceneLoadRecording.getResults();
        const auto frame_count = scene_load_recording.getNumRecordedPeriods();

        F64Seconds frame_time;

        os << "Stat";
        for (S32 frame = 1; frame <= frame_count; frame++)
        {
            frame_time += scene_load_recording.getPrevRecording(frame_count - frame).getDuration();
            os << ", " << frame_time.value();
        }
        os << '\n';

        os << "Sample period(s)";
        for (S32 frame = 1; frame <= frame_count; frame++)
        {
            frame_time = scene_load_recording.getPrevRecording(frame_count - frame).getDuration();
            os << ", " << frame_time.value();
        }
        os << '\n';


        typedef LLTrace::StatType<LLTrace::CountAccumulator> trace_count;
        for (auto& it : trace_count::instance_snapshot())
        {
            std::ostringstream row;
            row << std::setprecision(10);

            row << it.getName();

            const char* unit_label = it.getUnitLabel();
            if (unit_label[0])
            {
                row << "(" << unit_label << ")";
            }

            S32 samples = 0;

            for (S32 frame = 1; frame <= frame_count; frame++)
            {
                LLTrace::Recording& recording = scene_load_recording.getPrevRecording(frame_count - frame);
                samples += recording.getSampleCount(it);
                row << ", " << recording.getSum(it);
            }

            row << '\n';

            if (samples > 0)
            {
                os << row.str();
            }
        }

        typedef LLTrace::StatType<LLTrace::EventAccumulator> trace_event;

        for (auto& it : trace_event::instance_snapshot())
        {
            std::ostringstream row;
            row << std::setprecision(10);
            row << it.getName();

            const char* unit_label = it.getUnitLabel();
            if (unit_label[0])
            {
                row << "(" << unit_label << ")";
            }

            S32 samples = 0;

            for (S32 frame = 1; frame <= frame_count; frame++)
            {
                LLTrace::Recording& recording = scene_load_recording.getPrevRecording(frame_count - frame);
                samples += recording.getSampleCount(it);
                F64 mean = recording.getMean(it);
                if (llisnan(mean))
                {
                    row << ", n/a";
                }
                else
                {
                    row << ", " << mean;
                }
            }

            row << '\n';

            if (samples > 0)
            {
                os << row.str();
            }
        }

        typedef LLTrace::StatType<LLTrace::SampleAccumulator> trace_sample;

        for (auto& it : trace_sample::instance_snapshot())
        {
            std::ostringstream row;
            row << std::setprecision(10);
            row << it.getName();

            const char* unit_label = it.getUnitLabel();
            if (unit_label[0])
            {
                row << "(" << unit_label << ")";
            }

            S32 samples = 0;

            for (S32 frame = 1; frame <= frame_count; frame++)
            {
                LLTrace::Recording& recording = scene_load_recording.getPrevRecording(frame_count - frame);
                samples += recording.getSampleCount(it);
                F64 mean = recording.getMean(it);
                if (llisnan(mean))
                {
                    row << ", n/a";
                }
                else
                {
                    row << ", " << mean;
                }
            }

            row << '\n';

            if (samples > 0)
            {
                os << row.str();
            }
        }

        os.flush();
        os.close();
    }
    catch (const std::ios_base::failure &e)
    {
        LL_WARNS() << "Unable to dump scene monitor results: " << e.what() << LL_ENDL;
    }
}

//-------------------------------------------------------------------------------------------------------------
//definition of class LLSceneMonitorView
//-------------------------------------------------------------------------------------------------------------
LLSceneMonitorView::LLSceneMonitorView(const LLRect& rect)
    :   LLFloater(LLSD())
{
    setRect(rect);
    setVisible(false);

    setCanMinimize(false);
    setCanCollapse(false);
    setCanClose(true);

    sTeleportFinishConnection = LLViewerParcelMgr::getInstance()->setTeleportFinishedCallback(boost::bind(&LLSceneMonitorView::onTeleportFinished, this));
}

LLSceneMonitorView::~LLSceneMonitorView()
{
    sTeleportFinishConnection.disconnect();
}

void LLSceneMonitorView::onClose(bool app_quitting)
{
    setVisible(false);
}

void LLSceneMonitorView::onClickCloseBtn(bool app_quitting)
{
    setVisible(false);
}

void LLSceneMonitorView::onTeleportFinished()
{
    if(isInVisibleChain())
    {
        LLSceneMonitor::getInstance()->reset();
    }
}

void LLSceneMonitorView::onVisibilityChange(bool visible)
{
    // Showing the view makes the monitor if it has to, since the view is what turns its capture on. Hiding it tells
    // only a monitor that exists: a new one starts hidden, and a disconnect deletes it.
    if (visible || LLSceneMonitor::instanceExists())
    {
        LLSceneMonitor::getInstance()->setDebugViewerVisible(visible);
    }
}

void LLSceneMonitorView::draw()
{
    // A disconnect deletes the monitor; drawing it would make a new one.
    if (!LLSceneMonitor::instanceExists())
    {
        return;
    }

    const ALMetalTexture* target = LLSceneMonitor::getInstance()->getDiffTarget();
    if(!target)
    {
        return;
    }

    S32 height = target->height();
    S32 width = target->width();

    LLRect new_rect;
    new_rect.setLeftTopAndSize(getRect().mLeft, getRect().mTop, width, height);
    setRect(new_rect);

    //draw background
    gGL.getTextureSlot(0)->unbind();
    gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0, LLColor4(0.f, 0.f, 0.f, 0.25f));

    LLSceneMonitor::getInstance()->calcDiffAggregate();

    //show some texts
    LLColor4 color = LLColor4::white;
    S32 line_height = LLFontGL::getFontMonospace()->getLineHeight();

    S32 lines = 0;
    std::string num_str = llformat("Frame difference: %.6f", LLSceneMonitor::getInstance()->getDiffResult());
    LLFontGL::getFontMonospace()->renderUTF8(num_str, 0, 5, getRect().getHeight() - line_height * lines, color, LLFontGL::LEFT, LLFontGL::TOP);
    lines++;

    num_str = llformat("Pixel tolerance: (R+G+B) < %.4f", LLSceneMonitor::getInstance()->getDiffTolerance());
    LLFontGL::getFontMonospace()->renderUTF8(num_str, 0, 5, getRect().getHeight() - line_height * lines, color, LLFontGL::LEFT, LLFontGL::TOP);
    lines++;

    num_str = llformat("Sampling time: %.3f seconds", gSavedSettings.getF32("SceneLoadingMonitorSampleTime"));
    LLFontGL::getFontMonospace()->renderUTF8(num_str, 0, 5, getRect().getHeight() - line_height * lines, color, LLFontGL::LEFT, LLFontGL::TOP);
    lines++;

    num_str = llformat("Scene Loading time: %.3f seconds", (F32)LLSceneMonitor::getInstance()->getRecording()->getResults().getDuration().value());
    LLFontGL::getFontMonospace()->renderUTF8(num_str, 0, 5, getRect().getHeight() - line_height * lines, color, LLFontGL::LEFT, LLFontGL::TOP);
    lines++;

    LLView::draw();
}

