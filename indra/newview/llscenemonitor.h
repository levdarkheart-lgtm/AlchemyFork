/**
 * @file llscenemonitor.h
 * @brief monitor the process of scene loading
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

#ifndef LL_LLSCENE_MONITOR_H
#define LL_LLSCENE_MONITOR_H

#include "llsingleton.h"
#include "llmath.h"
#include "llfloater.h"
#include "llcharacter.h"
#include "lltracerecording.h"
#include "almetalscenemonitor.h"
#include <array>

class LLCharacter;
class LLSceneMonitor : public LLSingleton<LLSceneMonitor>
{
    LLSINGLETON(LLSceneMonitor);
    ~LLSceneMonitor();
    LOG_CLASS(LLSceneMonitor);
public:

    void freezeAvatar(LLCharacter* avatarp);
    void setDebugViewerVisible(bool visible);

    void capture(); //capture the main frame buffer
    void compare(); //compare the stored two buffers.
    void fetchQueryResult();
    void calcDiffAggregate();
    void setDiffTolerance(F32 tol) {mDiffTolerance = tol;}

    const ALMetalTexture* getDiffTarget() const
    { return mDiff ? mDiff->filteredTexture() : nullptr; }
    F32  getDiffTolerance() const {return mDiffTolerance;}
    F32  getDiffResult() const { return mDiffResult;}
    bool isEnabled()const {return mEnabled;}
    bool needsUpdate() const;

    const LLTrace::ExtendablePeriodicRecording* getRecording() const {return &mSceneLoadRecording;}
    void dumpToFile(const std::string &file_name);
    bool hasResults() const { return mSceneLoadRecording.getResults().getDuration() != S32Seconds(0);}

    void reset();

private:
    void freezeScene();
    void unfreezeScene();

    bool mEnabled = false, mDebugViewerVisible = false;

    enum EDiffState
    {
        WAITING_FOR_NEXT_DIFF,
        NEED_DIFF,
        WAIT_ON_RESULT,
        VIEWER_QUITTING
    } mDiffState = WAITING_FOR_NEXT_DIFF;

    std::array<ALMetalRetainedBackground::Lease, 2> mFrames;
    std::unique_ptr<ALMetalSceneMonitor> mNativeMonitor;
    std::unique_ptr<ALMetalSceneDifference> mPendingDiff, mDiff;
    ALMetalDevice* mNativeDevice = nullptr;
    U32 mCaptureWidth = 0, mCaptureHeight = 0;
    U32 mLastCaptureFrame = ~U32(0);
    bool mForceCapture = true;
    F32 mDiffResult = 0.f, mDiffTolerance = 0.1f, mDiffPixelRatio = 0.5f;

    // Only restore state this monitor actually froze. In particular, a user
    // who already froze time or disabled a render type must keep that choice.
    bool mSceneFrozen = false, mPreviousFreezeTime = false;
    S32 mPreviousParticleLimit = 0;
    std::array<bool, 4> mPreviousRenderTypes{};

    std::vector<LLAnimPauseRequest>         mAvatarPauseHandles;

    LLTimer                                 mRecordingTimer;
    LLTrace::ExtendablePeriodicRecording    mSceneLoadRecording;
    LLTrace::Recording                      mMonitorRecording;
};

class LLSceneMonitorView final : public LLFloater
{
public:
    AL_VIEW_TYPE(LLSceneMonitorView, LLFloater);

    LLSceneMonitorView(const LLRect& rect);
    ~LLSceneMonitorView();
    virtual void draw() override;

    virtual void onVisibilityChange(bool visible) override;

protected:
    virtual void onClose(bool app_quitting=false) override;
    virtual void onClickCloseBtn(bool app_quitting=false) override;
    void onTeleportFinished();
    boost::signals2::connection sTeleportFinishConnection;
};

extern LLSceneMonitorView* gSceneMonitorView;

#endif

