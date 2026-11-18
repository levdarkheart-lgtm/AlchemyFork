/**
 * @file alnativeframeplan.cpp
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "llviewerprecompiledheaders.h"
#include "alnativeframeplan.h"
#include "llmemory.h"
#include "llimagegl.h"

#include <algorithm>
#include <ctime>
#include <cmath>
#include <limits>

namespace
{
thread_local ALNativeFramePlan::MainScope* current_main = nullptr;
thread_local unsigned optional_depth = 0;
// These are scene-owner frame history, never touched by GPU/worker callbacks.
std::uint64_t frame_serial = 0;
std::uint64_t previous_required_cpu = 0;
std::array<ALNativeFramePlan::MainCompletionPhaseCensus, 2> completion_census{};

std::uint64_t threadCpuTime() noexcept
{
    timespec value{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) != 0 || value.tv_sec < 0) return 0;
    return static_cast<std::uint64_t>(value.tv_sec) * 1000000000ULL + value.tv_nsec;
}
}

void ALNativeFramePlan::observeMainCompletion(
    const ALMetalFrameScheduler* scheduler, MainCompletionPhase phase) noexcept
{
    const auto index = static_cast<std::size_t>(phase);
    if (index >= completion_census.size()) return;
    auto& census = completion_census[index];
    ++census.samples;
    if (!scheduler)
    {
        ++census.unavailable_samples;
        census.available = false;
        census.latest = {};
        census.last_submitted_serial = 0;
        census.last_submission_retired = false;
        return;
    }
    census.latest = scheduler->stats();
    census.last_submitted_serial = scheduler->lastSubmittedSerial();
    census.last_submission_retired = scheduler->isSubmissionComplete(census.last_submitted_serial);
    census.available = true;
    census.peak_in_flight = std::max(census.peak_in_flight, census.latest.in_flight);
    if (census.latest.maximum_in_flight &&
        census.latest.in_flight >= census.latest.maximum_in_flight)
        ++census.full_samples;
}

std::array<ALNativeFramePlan::MainCompletionPhaseCensus, 2>
ALNativeFramePlan::mainCompletionCensus() noexcept
{
    return completion_census;
}

ALNativeFramePlan::MainScope::MainScope(ALNativeFrameBudget::FrameInputs inputs)
{
    if (current_main) return;
    mCpuStart = threadCpuTime();
    begin(inputs);
}

ALNativeFramePlan::MainScope::MainScope(double interval_seconds, std::uint64_t minimum_headroom,
                                      bool main_pressure_continuity)
{
    if (current_main) return;
    mCpuStart = threadCpuTime();
    ALNativeFrameBudget::FrameInputs inputs;
    inputs.main_pressure_continuity = main_pressure_continuity;
    if (std::isfinite(interval_seconds) && interval_seconds > 0.0 &&
        interval_seconds < static_cast<double>(std::numeric_limits<std::uint64_t>::max()) / 1.e9)
        inputs.frame_interval_ns = static_cast<std::uint64_t>(interval_seconds * 1.e9);
    inputs.minimum_headroom_bytes = minimum_headroom;
    const auto memory = LLMemory::getMemoryBudgetSnapshot();
    const auto bytes = [](U32 kb)
    {
        return kb == std::numeric_limits<U32>::max()
            ? ALNativeFrameBudget::unknown_bytes : static_cast<std::uint64_t>(kb) * 1024;
    };
    inputs.os_available_bytes = bytes(memory.os_available_kb.value());
    inputs.process_headroom_bytes = bytes(memory.process_headroom_kb.value());
    if (auto* device = LLImageGL::getMetalDevice())
    {
        inputs.native_allocated_bytes = device->currentAllocatedSize();
        inputs.native_budget_bytes = device->info().recommended_working_set_size;
        const auto mode = device->resourcePlanningMode();
        if (mode != ALMetalResourcePlanMode::disabled)
        {
            const auto owned = device->uniqueResourceStats();
            inputs.native_owned_accounting = true;
            inputs.native_registered_use_tracking = mode == ALMetalResourcePlanMode::enabled;
            inputs.native_owned_reported_bytes = owned.owned_metal_reported_bytes;
            inputs.native_owned_unknown_declared_bytes = owned.owned_unknown_declared_bytes;
        }
    }
    begin(inputs);
}

void ALNativeFramePlan::MainScope::begin(ALNativeFrameBudget::FrameInputs inputs)
{
    inputs.frame = ++frame_serial;
    mBudgetFrame = inputs.frame;
    inputs.required_main_cpu_ns = previous_required_cpu;
    ALNativeFrameBudget::instance().beginFrame(inputs);
    current_main = this;
    mActive = true;
}

ALNativeFramePlan::MainScope::~MainScope()
{
    if (!mActive) return;
    recordRequiredWork();
    ALNativeFrameBudget::instance().closeMainObservations(mBudgetFrame);
    current_main = nullptr;
}

void ALNativeFramePlan::MainScope::recordRequiredWork()
{
    if (!mActive || !mCpuStart) return;
    const auto now = threadCpuTime();
    if (now < mCpuStart) return;
    const auto elapsed = now - mCpuStart;
    previous_required_cpu = elapsed - std::min(elapsed, mOptionalCpu);
    ALNativeFrameBudget::instance().recordRequiredMainWork(previous_required_cpu);
}

ALNativeFramePlan::OptionalScope::OptionalScope(ALNativeFrameBudget::Lease credit) noexcept
    : mCredit(std::move(credit)), mMain(current_main), mCpuStart(threadCpuTime()),
      mOutermost(optional_depth++ == 0), mSubmission(mCredit)
{}

ALNativeFramePlan::OptionalScope::~OptionalScope()
{
    if (mOutermost && mMain && mCpuStart)
    {
        const auto now = threadCpuTime();
        if (now >= mCpuStart) mMain->mOptionalCpu += now - mCpuStart;
    }
    --optional_depth;
}
