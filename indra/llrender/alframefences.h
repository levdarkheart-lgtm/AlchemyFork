/**
 * @file alframefences.h
 * @brief One fence a frame, and which frames the GPU has finished.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
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
 * $/LicenseInfo$
 */

#pragma once

#include "alpacingslack.h"
#include "stdtypes.h"

class ALMetalFrameScheduler;

/// Logical viewer frames mapped to the Metal scheduler's ordered submission
/// timeline. A frame is complete once every command buffer submitted before
/// its end has completed. Resource rings record the logical frame that last
/// used a region and can reuse it once this timeline reports completion.
///
/// The scheduler already bounds frames in flight. Optional frames-ahead pacing
/// retains the viewer's explicit latency/slack accounting without creating a
/// second GPU fence object or a fallback graphics context. Non-copyable.
class ALFrameFences
{
public:
    /// The logical frames whose submission serials may be pending at once.
    static constexpr U32 FRAMES = 8;

    ALFrameFences() = default;
    ALFrameFences(const ALFrameFences&) = delete;
    ALFrameFences& operator=(const ALFrameFences&) = delete;

    /// Install the window-owned scheduler. Changing or detaching a live
    /// scheduler drains the submissions already recorded against it.
    void setMetalScheduler(ALMetalFrameScheduler* scheduler);

    /// The open frame: a command issued now completes with it.
    U32 frame() const { return mFrame; }

    /// Whether every command issued in `frame` has finished, as far as the frames' ends have shown. No GL call. Frame
    /// 0, and every frame up to a release(), is complete.
    bool isComplete(U32 frame) const { return frame <= mComplete; }

    /// Waits up to `timeout_ns` for an ended frame to complete, polling first.
    /// Waiting on the still-open frame is invalid: its Metal command buffer has
    /// not been committed and must not be treated as reusable storage.
    bool wait(U32 frame, U64 timeout_ns, bool* stalled = nullptr);

    /// Waits for `frame` to complete however long the GPU takes, for a user that must reuse what the frame used: a slow
    /// GPU, however slow, is waited out. A wait that runs past LONG_WAIT_SECONDS is logged, once, so that a GPU that has
    /// hung shows in the log of the report the main loop's watchdog makes of it. `stalled` as for wait().
    void waitFinished(U32 frame, bool* stalled = nullptr);

    /// How long waitFinished() waits before it logs that it is waiting.
    static constexpr U32 LONG_WAIT_SECONDS = 10;

    /// Records the scheduler's latest committed submission for the open frame;
    /// with `frames_ahead` above 0 waits for the frame that many before it to
    /// complete; then polls earlier frames and opens the next frame.
    ///
    /// `frames_ahead` is how many frames the GPU may still be working on when the CPU begins the next: with 1, the frame
    /// before the one ending has finished. That wait paces the CPU and is no stall: it has a zone and a count of its
    /// own, and is for the frames a user sees, never a capture's. 0 waits for none but the frame FRAMES before, whose
    /// fence must make room. Either way the frame's end is noted in slack().
    void endFrame(U32 frames_ahead = 0);

    /// Drains recorded submissions and forgets the logical timeline. Every
    /// frame so far, including the open one, is complete afterwards.
    void release();

    /// How long the paced frames' ends have waited for the GPU, and what work before the next one's wait may have of it.
    ALPacingSlack& slack() { return mSlack; }
    const ALPacingSlack& slack() const { return mSlack; }

    /// The waits that had to wait, the frames' ends' pacing waits apart.
    U32 getStalls() const { return mStalls; }
    /// The frames' ends whose pacing wait had to wait for the GPU.
    U32 getPacingWaits() const { return mPacingWaits; }
    /// Scheduler completion queries and waits made.
    U32 getCalls() const { return mCalls; }

private:
    // Poll ended frames oldest first, stopping at the first pending submission.
    void poll();
    // Wait for an ended frame as the frame end paces the CPU. The seconds it
    // took; `behind` says whether the GPU was still processing it.
    F64 pace(U32 frame, bool& behind);
    // Every frame up to `frame` is complete: forget their serials.
    void completeThrough(U32 frame);

    ALMetalFrameScheduler* mScheduler = nullptr;
    U64   mSubmissions[FRAMES] = {}; // latest committed serial at each logical frame end
    U32   mFrame    = 1;         // the open frame
    U32   mComplete = 0;         // the newest frame shown complete, with every one before it
    U32   mStalls   = 0;
    U32   mPacingWaits = 0;
    U32   mCalls    = 0;
    U32   mFrameCalls = 0;       // timeline queries since the last endFrame()
    ALPacingSlack mSlack;
};
