/**
 * @file alnativeframebudget.cpp
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "alnativeframebudget.h"
#include <algorithm>
#include <mutex>

struct ALNativeFrameBudget::State
{
    explicit State(Limits supplied) : limits(supplied)
    {
        limits.uploads_per_frame = std::max(limits.uploads_per_frame, 1u);
        limits.uploads_in_flight = std::max(limits.uploads_in_flight, 1u);
        limits.upload_bytes_per_frame = std::max<std::uint64_t>(limits.upload_bytes_per_frame, 1);
        limits.upload_bytes_in_flight = std::max<std::uint64_t>(limits.upload_bytes_in_flight, 1);
        limits.optional_views_per_frame = std::max(limits.optional_views_per_frame, 1u);
        limits.preparation_jobs = std::max(limits.preparation_jobs, 1u);
        limits.maximum_deferral_frames = std::max<Frame>(limits.maximum_deferral_frames, 1);
    }
    mutable std::mutex mutex;
    Limits limits;
    FrameInputs inputs;
    Snapshot stats;
    unsigned ordinary_this_frame = 0;
    unsigned hero_this_frame = 0;
};

ALNativeFrameBudget::ALNativeFrameBudget() : ALNativeFrameBudget(Limits{}) {}
ALNativeFrameBudget::ALNativeFrameBudget(Limits limits) : mState(std::make_shared<State>(limits)) {}

ALNativeFrameBudget& ALNativeFrameBudget::instance()
{
    static ALNativeFrameBudget budget;
    return budget;
}

void ALNativeFrameBudget::beginFrame(const FrameInputs& inputs)
{
    std::lock_guard lock(mState->mutex);
    auto& state = *mState;
    if (state.stats.initialized && inputs.frame <= state.stats.frame) return;
    state.inputs = inputs;
    state.stats.initialized = true;
    state.stats.frame = inputs.frame;
    state.stats.uploads_this_frame = 0;
    state.stats.upload_bytes_this_frame = 0;
    state.stats.optional_views_this_frame = 0;
    state.ordinary_this_frame = state.hero_this_frame = 0;
    // Compare by division to avoid overflow in a caller-supplied interval.
    state.stats.main_busy = inputs.frame_interval_ns != 0 &&
        inputs.required_main_cpu_ns > inputs.frame_interval_ns - inputs.frame_interval_ns / 4;
    state.stats.memory_pressure =
        (inputs.native_budget_bytes != 0 && inputs.native_allocated_bytes >= inputs.native_budget_bytes) ||
        (inputs.os_available_bytes != unknown_bytes && inputs.os_available_bytes < inputs.minimum_headroom_bytes) ||
        (inputs.process_headroom_bytes != unknown_bytes && inputs.process_headroom_bytes < inputs.minimum_headroom_bytes);
}

void ALNativeFrameBudget::recordRequiredMainWork(std::uint64_t cpu_nanoseconds)
{
    std::lock_guard lock(mState->mutex);
    mState->inputs.required_main_cpu_ns = cpu_nanoseconds;
    const auto interval = mState->inputs.frame_interval_ns;
    mState->stats.main_busy = interval != 0 && cpu_nanoseconds > interval - interval / 4;
}

void ALNativeFrameBudget::updateStreaming(const StreamingBacklog& backlog)
{
    std::lock_guard lock(mState->mutex);
    // Publications are updated at their actual main-thread queue boundary.
    const auto publications = mState->stats.streaming.pending_publications;
    mState->stats.streaming = backlog;
    mState->stats.streaming.pending_publications = publications;
}

void ALNativeFrameBudget::setPendingPublications(std::uint64_t count)
{
    std::lock_guard lock(mState->mutex);
    mState->stats.streaming.pending_publications = count;
}

ALNativeFrameBudget::Credit::Credit(std::shared_ptr<State> state, Work work, std::uint64_t bytes)
    : mState(std::move(state)), mWork(work), mBytes(bytes) {}

ALNativeFrameBudget::Credit::~Credit()
{
    // shared_ptr control-block allocation may throw before admission below.
    if (!mAcquired) return;
    std::lock_guard lock(mState->mutex);
    auto& stats = mState->stats;
    switch (mWork)
    {
    case Work::upload:
        --stats.uploads_in_flight;
        stats.upload_bytes_in_flight -= mBytes;
        break;
    case Work::preparation: --stats.preparation_in_flight; break;
    default: --stats.optional_views_in_flight; break;
    }
    ++stats.retired;
}

ALNativeFrameBudget::Decision ALNativeFrameBudget::tryAcquire(
    Work work, std::uint64_t bytes, Frame oldest_frame)
{
    std::lock_guard lock(mState->mutex);
    auto& state = *mState;
    auto& stats = state.stats;
    const auto& limits = state.limits;
    const bool aged = stats.frame >= oldest_frame &&
        stats.frame - oldest_frame >= limits.maximum_deferral_frames;
    Reason reason = Reason::admitted;
    const auto deny = [&stats](Reason why) -> Decision
    {
        ++stats.deferred;
        return {{}, why};
    };
    if (stats.draining && work != Work::upload) return deny(Reason::draining);
    if (stats.initialized && !stats.draining)
    {
        if (work == Work::upload)
        {
            // An oversized individual image may progress only as the sole
            // in-flight upload; it cannot wedge forever behind a byte ceiling.
            if (stats.uploads_in_flight >= limits.uploads_in_flight ||
                (stats.uploads_in_flight != 0 &&
                 (bytes > limits.upload_bytes_in_flight ||
                  stats.upload_bytes_in_flight > limits.upload_bytes_in_flight - bytes)))
                return deny(Reason::in_flight);
            const bool frame_full = stats.uploads_this_frame >= limits.uploads_per_frame ||
                (stats.uploads_this_frame != 0 &&
                 (bytes > limits.upload_bytes_per_frame ||
                  stats.upload_bytes_this_frame > limits.upload_bytes_per_frame - bytes));
            if (frame_full) return deny(Reason::frame_work);
            if (stats.main_busy || stats.memory_pressure)
            {
                // One upload each frame is the forward-progress lane, while
                // additional traffic competes with main/probe work normally.
                if (stats.uploads_this_frame != 0)
                    return deny(stats.memory_pressure ? Reason::memory_pressure : Reason::frame_work);
                reason = Reason::progress;
            }
        }
        else if (work == Work::preparation)
        {
            if (stats.preparation_in_flight >= limits.preparation_jobs)
                return deny(Reason::in_flight);
        }
        else
        {
            // One optional credit covers a coherent ordinary/hero UPDATE
            // GROUP, never a face. The outer-frame owner acquires at most one
            // per kind and shares its credit across all selected commands.
            const auto maximum_active = static_cast<std::uint64_t>(limits.optional_views_per_frame) * 2;
            if (stats.optional_views_in_flight >= maximum_active) return deny(Reason::in_flight);
            if (stats.optional_views_this_frame >= limits.optional_views_per_frame) return deny(Reason::frame_work);
            if (stats.main_busy || stats.memory_pressure ||
                stats.streaming.queued_requests >= limits.uploads_in_flight ||
                stats.streaming.pressure_requests != 0)
            {
                const unsigned already_served = work == Work::ordinary_probe
                    ? state.ordinary_this_frame : state.hero_this_frame;
                if (!aged || already_served != 0)
                    return deny(stats.memory_pressure ? Reason::memory_pressure : Reason::frame_work);
                reason = Reason::progress;
            }
        }
    }
    if (work == Work::upload && bytes > unknown_bytes - stats.upload_bytes_in_flight)
        return deny(Reason::in_flight);
    // Allocate before changing any counters: allocation failure cannot leave a
    // phantom reservation that blocks future work or shutdown.
    std::shared_ptr<Credit> credit(new Credit(mState, work, bytes));
    if (work == Work::upload)
    {
        ++stats.uploads_in_flight;
        stats.upload_bytes_in_flight += bytes;
        ++stats.uploads_this_frame;
        stats.upload_bytes_this_frame = bytes > unknown_bytes - stats.upload_bytes_this_frame
            ? unknown_bytes : stats.upload_bytes_this_frame + bytes;
    }
    else if (work == Work::preparation) ++stats.preparation_in_flight;
    else
    {
        ++stats.optional_views_in_flight;
        ++stats.optional_views_this_frame;
        if (work == Work::ordinary_probe) ++state.ordinary_this_frame;
        else ++state.hero_this_frame;
    }
    ++stats.admitted;
    if (reason == Reason::progress) ++stats.progress_admissions;
    credit->mAcquired = true;
    return {std::move(credit), reason};
}

unsigned ALNativeFrameBudget::decodeCeiling(unsigned minimum, unsigned current, unsigned maximum) const
{
    (void)current;
    std::lock_guard lock(mState->mutex);
    const auto& state = *mState;
    const auto& stats = state.stats;
    minimum = std::max(minimum, 1u);
    maximum = std::max(maximum, minimum);
    if (!stats.initialized || stats.draining) return maximum;
    const bool downstream_full = stats.streaming.queued_requests >= state.limits.uploads_in_flight ||
        stats.streaming.queued_input_bytes >= state.limits.upload_bytes_in_flight ||
        stats.streaming.pending_publications >= state.limits.uploads_in_flight ||
        stats.uploads_in_flight >= state.limits.uploads_in_flight ||
        stats.upload_bytes_in_flight >= state.limits.upload_bytes_in_flight ||
        stats.streaming.pressure_requests != 0;
    if (stats.main_busy || stats.memory_pressure || downstream_full) return minimum;
    return maximum - std::min(maximum - minimum, stats.preparation_in_flight);
}

ALNativeFrameBudget::Snapshot ALNativeFrameBudget::snapshot() const
{
    std::lock_guard lock(mState->mutex);
    return mState->stats;
}

void ALNativeFrameBudget::beginDrain()
{
    std::lock_guard lock(mState->mutex);
    mState->stats.draining = true;
}
