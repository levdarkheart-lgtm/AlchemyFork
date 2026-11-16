/**
 * @file almetalworldencodetask.h
 * @brief One exclusive owned main G-buffer encode with caller-side joining.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetalworldprepare.h"
#include "almetalstagetiming.h"
#include "../llcommon/alnativeframebudget.h"
#include <dispatch/dispatch.h>
#include <chrono>
#include <thread>

// PPD307: this transfers the SAME renderer to one worker for a short interval.
// Its caller may only gather independent CPU values until finish() joins it.
// No command admission, commit, query-pool call or viewer callback lives here.
class ALMetalMainWorldEncodingTask final
{
public:
    struct Outcome
    {
        std::unique_ptr<ALMetalRenderTarget> target;
        std::shared_ptr<ALMetalVisibilityResultBatch> visibility_results;
        std::string error;
        std::uint64_t started_ns = 0, finished_ns = 0;
        bool threw = false, worker_is_owner = false;
    };
    ALMetalMainWorldEncodingTask() = default;
    ALMetalMainWorldEncodingTask(const ALMetalMainWorldEncodingTask&) = delete;
    ALMetalMainWorldEncodingTask& operator=(const ALMetalMainWorldEncodingTask&) = delete;
    ~ALMetalMainWorldEncodingTask()
    {
        if (mGroup)
        {
            join();
            // An owner exception cannot abandon a running reader or publish
            // an unknown prefix. Ordinary successful finish leaves commit
            // and visibility settlement at their original owner boundary.
            mRenderer->cancelCommandBatch();
        }
    }
    bool start(ALMetalWorldGBufferRenderer& renderer,
               const std::shared_ptr<const ALMetalWorldViewInputs>& inputs,
               ALNativeFrameBudget::Lease reservation)
    {
        // Submission TLS is captured at the original owner batch creation.
        // Explicit diagnostic windows are owner-thread objects and decline.
        if (mRenderer || !inputs || !reservation ||
            ALMetalStageTimingScope::current() || !renderer.commandBatchActive()) return false;
        auto group = dispatch_group_create();
        if (!group) return false;
        mRenderer = &renderer; mInputs = inputs; mReservation = std::move(reservation);
        mSubmissionOwner = ALMetalSubmissionScope::current();
        mOwner = std::this_thread::get_id(); mGroup = group;
        dispatch_group_async_f(group, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), this, &run);
        return true;
    }
    Outcome finish() noexcept
    {
        if (!mGroup) return {};
        join();
        if (mOutcome.threw) mRenderer->cancelCommandBatch();
        return std::move(mOutcome);
    }
private:
    static std::uint64_t now() noexcept
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    static void run(void* opaque) noexcept
    {
        auto& task = *static_cast<ALMetalMainWorldEncodingTask*>(opaque);
        task.mOutcome.started_ns = now();
        task.mOutcome.worker_is_owner = std::this_thread::get_id() == task.mOwner;
        try
        {
            ALMetalSubmissionScope owner(task.mSubmissionOwner);
            task.mOutcome.target = task.mRenderer->renderOwnedInputs(
                *task.mInputs, &task.mOutcome.error, &task.mOutcome.visibility_results);
        }
        catch (...) { task.mOutcome.threw = true; }
        task.mOutcome.finished_ns = now();
    }
    void join() noexcept
    {
        dispatch_group_wait(mGroup, DISPATCH_TIME_FOREVER);
        dispatch_release(mGroup); mGroup = nullptr;
        mInputs.reset(); mSubmissionOwner.reset(); mReservation.reset();
    }
    ALMetalWorldGBufferRenderer* mRenderer = nullptr;
    std::shared_ptr<const ALMetalWorldViewInputs> mInputs;
    std::shared_ptr<const void> mSubmissionOwner;
    ALNativeFrameBudget::Lease mReservation;
    std::thread::id mOwner;
    dispatch_group_t mGroup = nullptr;
    Outcome mOutcome;
};
