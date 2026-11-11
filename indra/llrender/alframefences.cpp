/**
 * @file alframefences.cpp
 * @brief Viewer-frame completion over the Metal submission timeline.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "alframefences.h"

#include "almetalframe.h"
#include "lltimer.h"

#include <string>

namespace
{
    constexpr U64 WAIT_NANOSECONDS = 1000000000ull;
}

void ALFrameFences::setMetalScheduler(ALMetalFrameScheduler* scheduler)
{
    if (mScheduler == scheduler)
    {
        return;
    }
    if (mScheduler)
    {
        release();
    }
    mScheduler = scheduler;
}

void ALFrameFences::poll()
{
    if (!mScheduler)
    {
        return;
    }
    while (mComplete + 1 < mFrame)
    {
        const U32 frame = mComplete + 1;
        const U64 submission = mSubmissions[frame % FRAMES];
        ++mCalls;
        ++mFrameCalls;
        if (!mScheduler->isSubmissionComplete(submission))
        {
            return;
        }
        completeThrough(frame);
    }
}

void ALFrameFences::completeThrough(U32 frame)
{
    for (U32 done = mComplete + 1; done <= frame && done < mFrame; ++done)
    {
        mSubmissions[done % FRAMES] = 0;
    }
    if (frame > mComplete)
    {
        mComplete = frame;
    }
}

bool ALFrameFences::wait(U32 frame, U64 timeout_ns, bool* stalled)
{
    if (stalled)
    {
        *stalled = false;
    }
    if (isComplete(frame))
    {
        return true;
    }
    if (!mScheduler)
    {
        LL_WARNS_ONCE("FrameFences")
            << "Cannot wait for a viewer frame without its Metal scheduler"
            << LL_ENDL;
        return false;
    }
    if (frame >= mFrame)
    {
        LL_WARNS_ONCE("FrameFences")
            << "Cannot wait for the open viewer frame before its Metal command buffer is submitted"
            << LL_ENDL;
        return false;
    }

    poll();
    if (isComplete(frame))
    {
        return true;
    }

    const U64 submission = mSubmissions[frame % FRAMES];
    ++mCalls;
    ++mFrameCalls;
    if (!mScheduler->isSubmissionComplete(submission))
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Metal frame completion stall");
        ++mStalls;
        if (stalled)
        {
            *stalled = true;
        }
    }
    ++mCalls;
    ++mFrameCalls;
    std::string error;
    if (!mScheduler->waitForSubmission(submission, timeout_ns, &error))
    {
        return false;
    }
    completeThrough(frame);
    return true;
}

void ALFrameFences::waitFinished(U32 frame, bool* stalled)
{
    if (stalled)
    {
        *stalled = false;
    }
    if (frame >= mFrame && !isComplete(frame))
    {
        // Reusing storage read by an uncommitted command buffer would corrupt
        // that frame. Native ring owners must grow or split their command
        // transaction rather than manufacture completion here.
        LL_ERRS("FrameFences")
            << "A Metal resource attempted to reuse storage from the still-open viewer frame"
            << LL_ENDL;
        return;
    }
    for (U32 tries = 1;; ++tries)
    {
        bool this_wait = false;
        const bool finished = wait(frame, WAIT_NANOSECONDS, &this_wait);
        if (stalled && this_wait)
        {
            *stalled = true;
        }
        if (finished)
        {
            return;
        }
        if (tries == LONG_WAIT_SECONDS)
        {
            LL_WARNS_ONCE("FrameFences")
                << "The GPU has not finished a Metal submission in "
                << LONG_WAIT_SECONDS << " seconds; still waiting for it"
                << LL_ENDL;
        }
    }
}

F64 ALFrameFences::pace(U32 frame, bool& behind)
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("Metal frame pacing wait");
    const F64 start = LLTimer::getTotalSeconds();
    behind = false;
    if (!mScheduler || frame >= mFrame)
    {
        return 0.0;
    }

    const U64 submission = mSubmissions[frame % FRAMES];
    ++mCalls;
    ++mFrameCalls;
    behind = !mScheduler->isSubmissionComplete(submission);
    if (behind)
    {
        for (U32 tries = 1;; ++tries)
        {
            ++mCalls;
            ++mFrameCalls;
            if (mScheduler->waitForSubmission(
                    submission, WAIT_NANOSECONDS, nullptr))
            {
                break;
            }
            if (tries == LONG_WAIT_SECONDS)
            {
                LL_WARNS_ONCE("FrameFences")
                    << "The GPU has not finished a paced Metal submission in "
                    << LONG_WAIT_SECONDS << " seconds; still waiting for it"
                    << LL_ENDL;
            }
        }
        ++mPacingWaits;
    }
    completeThrough(frame);
    return LLTimer::getTotalSeconds() - start;
}

void ALFrameFences::endFrame(U32 frames_ahead)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    if (!mScheduler)
    {
        LL_ERRS("FrameFences")
            << "Cannot end a native viewer frame without its Metal scheduler"
            << LL_ENDL;
        return;
    }

    if (mFrame > FRAMES)
    {
        waitFinished(mFrame - FRAMES);
    }

    const U32 slot = mFrame % FRAMES;
    llassert(mSubmissions[slot] == 0);
    mSubmissions[slot] = mScheduler->lastSubmittedSerial();

    F64 paced = 0.0;
    bool behind = false;
    if (frames_ahead > 0 && mFrame > frames_ahead &&
        !isComplete(mFrame - frames_ahead))
    {
        paced = pace(mFrame - frames_ahead, behind);
    }
    if (frames_ahead > 0)
    {
        mSlack.endPaced(paced, behind);
    }
    else
    {
        mSlack.endUnpaced();
    }

    poll();
    ++mFrame;

    LL_PROFILE_PLOT("Metal frame completion calls", (int64_t)mFrameCalls);
    LL_PROFILE_PLOT("Metal frame pacing wait ms", paced * 1000.0);
    mFrameCalls = 0;
}

void ALFrameFences::release()
{
    U64 latest = mScheduler ? mScheduler->lastSubmittedSerial() : 0;
    for (U64 submission : mSubmissions)
    {
        latest = llmax(latest, submission);
    }
    if (mScheduler && latest != 0 &&
        !mScheduler->isSubmissionComplete(latest))
    {
        std::string error;
        if (!mScheduler->waitForSubmission(latest, UINT64_MAX, &error))
        {
            LL_WARNS("FrameFences")
                << "Could not drain the Metal frame timeline during release: "
                << error << LL_ENDL;
        }
    }
    for (U64& submission : mSubmissions)
    {
        submission = 0;
    }
    mComplete = mFrame;
    ++mFrame;
    mSlack = ALPacingSlack();
}
