/**
 * @file alnativeframeplan.h
 * @brief Outer viewer-frame and coherent native optional-work ownership.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include "alnativeframebudget.h"
#include "../almetal/almetalframe.h"

#include <array>

namespace ALNativeFramePlan
{
enum class MainCompletionPhase { frame_begin, before_ordinary_probes };
struct MainCompletionPhaseCensus
{
    std::uint64_t samples = 0, unavailable_samples = 0, full_samples = 0;
    std::uint32_t peak_in_flight = 0;
    bool available = false, last_submission_retired = false;
    std::uint64_t last_submitted_serial = 0;
    ALMetalFrameStats latest;
};

// Observe/read on the outer frame owner only. Each scheduler counter and its
// ordered retirement receipt is sampled independently, not as a coherent
// submitted-minus-completed snapshot. Retirement includes failed commands;
// it establishes neither successful rendering, display/scan-out nor GPU use.
// Null clears the latest known sample. No scheduler pointer is retained and
// observation does not wait for GPU work, submit, enable timing or admit work.
void observeMainCompletion(const ALMetalFrameScheduler* scheduler,
                           MainCompletionPhase phase) noexcept;
std::array<MainCompletionPhaseCensus, 2> mainCompletionCensus() noexcept;

// The outer doFrame owner publishes one plan before simulation/streaming and
// rendering. Snapshot/cube/impostor display recursion does not start a plan or
// refill credits. Thread CPU time excludes explicit optional groups, pacing
// waits and other worker threads; actual native completion retires GPU credit.
class MainScope final
{
public:
    explicit MainScope(ALNativeFrameBudget::FrameInputs inputs);
    MainScope(double observed_frame_interval_seconds, std::uint64_t minimum_headroom_bytes);
    ~MainScope();
    MainScope(const MainScope&) = delete;
    MainScope& operator=(const MainScope&) = delete;
    void recordRequiredWork();
private:
    friend class OptionalScope;
    void begin(ALNativeFrameBudget::FrameInputs inputs);
    std::uint64_t mCpuStart = 0;
    std::uint64_t mOptionalCpu = 0;
    ALNativeFrameBudget::Frame mBudgetFrame = 0;
    bool mActive = false;
};

// One credit represents an entire ordinary/hero update group, not one cube
// face. All faces selected by the existing detail/rate settings share this
// owner. Native commands created inside inherit it through actual completion.
class OptionalScope final
{
public:
    explicit OptionalScope(ALNativeFrameBudget::Lease credit) noexcept;
    ~OptionalScope();
    OptionalScope(const OptionalScope&) = delete;
    OptionalScope& operator=(const OptionalScope&) = delete;
private:
    ALNativeFrameBudget::Lease mCredit;
    MainScope* mMain = nullptr;
    std::uint64_t mCpuStart = 0;
    bool mOutermost = false;
    ALMetalSubmissionScope mSubmission;
};
}
