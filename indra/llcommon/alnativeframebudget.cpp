/**
 * @file alnativeframebudget.cpp
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "alnativeframebudget.h"
#include <algorithm>
#include <array>
#include <mutex>

struct ALNativeFrameBudget::State
{
    explicit State(Limits supplied) : limits(supplied)
    {
        limits.uploads_per_frame = std::max(limits.uploads_per_frame, 1u);
        limits.uploads_in_flight = std::max(limits.uploads_in_flight, 1u);
        limits.upload_bytes_per_frame = std::max<std::uint64_t>(limits.upload_bytes_per_frame, 1);
        limits.upload_bytes_in_flight = std::max<std::uint64_t>(limits.upload_bytes_in_flight, 1);
        limits.scene_publications_per_frame = std::max(limits.scene_publications_per_frame, 1u);
        limits.scene_publications_in_flight = std::max(limits.scene_publications_in_flight, 1u);
        limits.scene_bytes_per_frame = std::max<std::uint64_t>(limits.scene_bytes_per_frame, 1);
        limits.scene_bytes_in_flight = std::max<std::uint64_t>(limits.scene_bytes_in_flight, 1);
        limits.maintenance_per_frame = std::max(limits.maintenance_per_frame, 1u);
        limits.maintenance_in_flight = std::max(limits.maintenance_in_flight, 1u);
        limits.maintenance_bytes_per_frame = std::max<std::uint64_t>(limits.maintenance_bytes_per_frame, 1);
        limits.maintenance_bytes_in_flight = std::max<std::uint64_t>(limits.maintenance_bytes_in_flight, 1);
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
    bool oversized_scene_in_flight = false;
    bool oversized_maintenance_in_flight = false;
    struct ScenePending
    {
        Frame oldest = 0;
        std::uint64_t bytes = 0;
    };
    struct SceneHistory
    {
        std::optional<ScenePending> previous_denial, current_denial;
        std::optional<Frame> last_service, serviced_oldest;
    };
    std::array<SceneHistory, 2> scene_history;

    // Ignore frame usage here: a later phase denied by today's earlier work
    // must be visible to next frame's first caller. Actual active capacity
    // and sole-oversized eligibility are still required for a useful hint.
    bool sceneCanWait(std::uint64_t bytes) const noexcept
    {
        return bytes != unknown_bytes && bytes <= limits.scene_bytes_in_flight &&
            !oversized_scene_in_flight &&
            stats.scene_publications_in_flight < limits.scene_publications_in_flight &&
            stats.scene_bytes_in_flight <= limits.scene_bytes_in_flight - bytes &&
            (bytes <= limits.scene_bytes_per_frame || stats.scene_publications_in_flight == 0);
    }
    void sceneDenied(std::size_t kind, std::uint64_t bytes, Frame oldest) noexcept
    {
        if (oldest > stats.frame || !sceneCanWait(bytes)) return;
        // A duplicate after this age bucket already received today's credit
        // must not create a phantom older reservation for the following frame.
        const auto& history = scene_history[kind];
        if (history.last_service == stats.frame && history.serviced_oldest == oldest) return;
        auto& pending = scene_history[kind].current_denial;
        if (!pending || oldest < pending->oldest || (oldest == pending->oldest && bytes < pending->bytes))
            pending = ScenePending{oldest, bytes};
    }
    std::optional<ScenePending> sceneWaiting(std::size_t kind) const noexcept
    {
        std::optional<ScenePending> oldest;
        for (const auto& pending : {scene_history[kind].previous_denial, scene_history[kind].current_denial})
        {
            if (!pending || pending->oldest > stats.frame ||
                stats.frame - pending->oldest < limits.maximum_deferral_frames || !sceneCanWait(pending->bytes)) continue;
            if (!oldest || pending->oldest < oldest->oldest ||
                (pending->oldest == oldest->oldest && pending->bytes < oldest->bytes)) oldest = pending;
        }
        return oldest;
    }
    bool sceneTurn(std::size_t kind, Frame oldest) const noexcept
    {
        // Within a phase, a newer earlier callback cannot overtake an older
        // recently denied Pages/Commands request. An absent old request can
        // cost at most one frame because only fresh denials renew the hint.
        if (const auto same = sceneWaiting(kind); same && same->oldest < oldest) return false;
        const auto other_kind = 1 - kind;
        const auto other = sceneWaiting(other_kind);
        if (!other) return true;
        const auto mine = std::max(oldest, scene_history[kind].last_service.value_or(0));
        const auto theirs = std::max(other->oldest, scene_history[other_kind].last_service.value_or(0));
        // Stable ties prefer metadata. After a grant its service frame makes
        // the other pending phase older, even if thousands of new CPU owners
        // all carry the same original dirty age.
        return mine < theirs || (mine == theirs && kind < other_kind);
    }
};

ALNativeFrameBudget::ALNativeFrameBudget() : ALNativeFrameBudget(Limits{}) {}
ALNativeFrameBudget::ALNativeFrameBudget(Limits limits) : mState(std::make_shared<State>(limits)) {}

ALNativeFrameBudget& ALNativeFrameBudget::instance()
{
    static ALNativeFrameBudget budget;
    return budget;
}

void ALNativeCohortCompilationAdmission::notePending(Frame frame) noexcept
{
    if (!mPendingSince) mPendingSince = frame;
}

ALNativeCohortCompilationAdmission::Priority
ALNativeCohortCompilationAdmission::priority(std::uint64_t stable_tie) const noexcept
{
    return {mPendingSince
        ? std::max(*mPendingSince, mLastAdmitted.value_or(0))
        : std::numeric_limits<Frame>::max(), stable_tie};
}

bool ALNativeCohortCompilationAdmission::deferUnstableRevision(
    Frame frame, std::optional<ProducerStamp> producer) noexcept
{
    if (!producer) return false;
    if ((mLastConsidered && frame <= *mLastConsidered) ||
        (mLastObservedFrame && frame < *mLastObservedFrame)) return true;
    notePending(frame);
    if (!producer->identity || !producer->revision)
    {
        mLastConsidered = frame;
        return true;
    }
    const bool same = mObservedProducer && *mObservedProducer == *producer;
    if (!same)
    {
        mObservedProducer = producer;
        mFirstObservedFrame = frame;
    }
    mLastObservedFrame = frame;
    if (same && mFirstObservedFrame && frame > *mFirstObservedFrame) return false;
    mLastConsidered = frame;
    return true;
}

ALNativeFrameBudget::Decision ALNativeCohortCompilationAdmission::tryAcquire(
    ALNativeFrameBudget& budget, std::uint64_t logical_bytes)
{
    const auto frame = budget.snapshot().frame;
    notePending(frame);
    // Main/probe recursion cannot reconsider even a failed or denied owner.
    // Refuse older frame domains too; callers must not reuse this state with
    // an unrelated budget whose serial restarts below its previous service.
    if (mLastConsidered && frame <= *mLastConsidered)
        return {{}, ALNativeFrameBudget::Reason::frame_work};
    mLastConsidered = frame;
    if (logical_bytes == ALNativeFrameBudget::unknown_bytes)
        return {{}, ALNativeFrameBudget::Reason::frame_work};
    auto decision = budget.tryAcquire(ALNativeFrameBudget::Work::scene_publication,
        logical_bytes, *mPendingSince, ALNativeFrameBudget::ScenePublicationKind::cohort_compilation);
    if (decision) mLastAdmitted = frame;
    return decision;
}

void ALNativeCohortCompilationAdmission::publicationCompleted() noexcept
{
    mPendingSince.reset();
    // Same-revision cache eviction is not producer mutation. Retain scalar
    // observations as well as consideration/service history; an actual new
    // producer identity/revision must establish its own two-frame evidence.
}

void ALNativeFrameBudget::beginFrame(const FrameInputs& inputs)
{
    std::lock_guard lock(mState->mutex);
    auto& state = *mState;
    if (state.stats.initialized && inputs.frame <= state.stats.frame) return;
    const bool consecutive = state.stats.initialized && inputs.frame - state.stats.frame == 1;
    for (auto& history : state.scene_history)
    {
        history.previous_denial = consecutive ? history.current_denial : std::nullopt;
        history.current_denial.reset();
    }
    state.inputs = inputs;
    state.stats.initialized = true;
    state.stats.frame = inputs.frame;
    state.stats.uploads_this_frame = 0;
    state.stats.upload_bytes_this_frame = 0;
    state.stats.scene_publications_this_frame = 0;
    state.stats.scene_bytes_this_frame = 0;
    state.stats.maintenance_this_frame = 0;
    state.stats.maintenance_bytes_this_frame = 0;
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
    case Work::native_maintenance:
        --stats.maintenance_in_flight;
        stats.maintenance_bytes_in_flight -= mBytes;
        if(mBytes>mState->limits.maintenance_bytes_per_frame || mBytes>mState->limits.maintenance_bytes_in_flight)
            mState->oversized_maintenance_in_flight=false;
        break;
    case Work::scene_publication:
        --stats.scene_publications_in_flight;
        stats.scene_bytes_in_flight -= mBytes;
        if (mBytes > mState->limits.scene_bytes_per_frame) mState->oversized_scene_in_flight = false;
        break;
    default: --stats.optional_views_in_flight; break;
    }
    ++stats.retired;
}

ALNativeFrameBudget::Decision ALNativeFrameBudget::tryAcquire(
    Work work, std::uint64_t bytes, Frame oldest_frame, ScenePublicationKind scene_kind)
{
    std::lock_guard lock(mState->mutex);
    auto& state = *mState;
    auto& stats = state.stats;
    const auto& limits = state.limits;
    const bool aged = stats.frame >= oldest_frame &&
        stats.frame - oldest_frame >= limits.maximum_deferral_frames;
    Reason reason = Reason::admitted;
    const auto kind = static_cast<std::size_t>(scene_kind);
    const auto deny = [&](Reason why) -> Decision
    {
        if (work == Work::scene_publication && !stats.draining && kind < state.scene_history.size())
            state.sceneDenied(kind, bytes, oldest_frame);
        ++stats.deferred;
        return {{}, why};
    };
    if (stats.draining && work != Work::upload) return deny(Reason::draining);
    if(work==Work::native_maintenance)
    {
        if(bytes==unknown_bytes)return deny(Reason::frame_work);
        if(state.oversized_maintenance_in_flight || stats.maintenance_in_flight>=limits.maintenance_in_flight)
            return deny(Reason::in_flight);
        const bool oversized=bytes>limits.maintenance_bytes_per_frame || bytes>limits.maintenance_bytes_in_flight;
        // Reducing an already resident large texture must not wedge forever
        // behind a smaller byte allowance. Exactly ONE oversize destination
        // may progress alone; its exact debt remains exclusive until retired.
        if(oversized && (stats.maintenance_in_flight || stats.maintenance_this_frame))
            return deny(stats.maintenance_in_flight?Reason::in_flight:Reason::frame_work);
        if(!oversized && stats.maintenance_bytes_in_flight>limits.maintenance_bytes_in_flight-bytes)
            return deny(Reason::in_flight);
        if(stats.maintenance_this_frame>=limits.maintenance_per_frame ||
           (!oversized && stats.maintenance_bytes_this_frame>limits.maintenance_bytes_per_frame-bytes))
            return deny(Reason::frame_work);
        // Unlike optional refresh, pressure reduction is useful UNDER memory
        // pressure. Keep immediate one-item progress without borrowing image
        // upload/probe credit or bypassing live maintenance capacity.
        if(stats.main_busy || stats.memory_pressure)
        {
            if(stats.maintenance_this_frame)return deny(stats.memory_pressure?Reason::memory_pressure:Reason::frame_work);
            reason=Reason::progress;
        }
        else if(oversized)reason=Reason::progress;
        if(bytes>unknown_bytes-stats.maintenance_bytes_in_flight || bytes>unknown_bytes-stats.maintenance_bytes_this_frame)
            return deny(Reason::in_flight);
    }
    else if (work == Work::scene_publication)
    {
        if (kind >= state.scene_history.size()) return deny(Reason::frame_work);
        // Native scene metadata is always bounded, including startup before
        // the first outer frame plan. Its hard in-flight cap is never waived.
        if (state.oversized_scene_in_flight ||
            stats.scene_publications_in_flight >= limits.scene_publications_in_flight ||
            bytes > limits.scene_bytes_in_flight ||
            stats.scene_bytes_in_flight > limits.scene_bytes_in_flight - bytes)
            return deny(Reason::in_flight);
        if (stats.scene_publications_this_frame >= limits.scene_publications_per_frame)
            return deny(Reason::frame_work);
        // A batch larger than the normal per-frame allowance may progress
        // alone, after all previous scene credits retire, while remaining
        // within the hard in-flight/actual allocator bounds. It consumes the
        // entire scene lane for this frame and stays exclusive until final
        // retirement; it borrows no upload/probe credit.
        const bool sole_oversized = bytes > limits.scene_bytes_per_frame &&
            stats.scene_publications_this_frame == 0 && stats.scene_publications_in_flight == 0;
        if ((bytes > limits.scene_bytes_per_frame ||
             stats.scene_bytes_this_frame > limits.scene_bytes_per_frame - bytes) && !sole_oversized)
            return deny(Reason::frame_work);
        // An aged first batch is a finite progress lane under actual frame or
        // memory pressure, still subject to both scene caps above. A fresh
        // dirty batch does not add publication work to an already busy frame.
        if (stats.main_busy || stats.memory_pressure)
        {
            if (!aged || stats.scene_publications_this_frame != 0 || !state.sceneTurn(kind, oldest_frame))
                return deny(stats.memory_pressure ? Reason::memory_pressure : Reason::frame_work);
            reason = Reason::progress;
        }
        else if (sole_oversized) reason = Reason::progress;
        // Addition is checked before allocating/changing any counters. Unlike
        // a display estimate, live debt may never saturate or wrap silently.
        if (bytes > unknown_bytes - stats.scene_bytes_in_flight)
            return deny(Reason::in_flight);
        if (bytes > unknown_bytes - stats.scene_bytes_this_frame)
            return deny(Reason::frame_work);
    }
    else if (stats.initialized && !stats.draining)
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
    if(work==Work::native_maintenance)
    {
        ++stats.maintenance_in_flight;++stats.maintenance_this_frame;
        stats.maintenance_bytes_in_flight+=bytes;stats.maintenance_bytes_this_frame+=bytes;
        if(bytes>limits.maintenance_bytes_per_frame || bytes>limits.maintenance_bytes_in_flight)
            state.oversized_maintenance_in_flight=true;
    }
    else if (work == Work::upload)
    {
        ++stats.uploads_in_flight;
        stats.upload_bytes_in_flight += bytes;
        ++stats.uploads_this_frame;
        stats.upload_bytes_this_frame = bytes > unknown_bytes - stats.upload_bytes_this_frame
            ? unknown_bytes : stats.upload_bytes_this_frame + bytes;
    }
    else if (work == Work::preparation) ++stats.preparation_in_flight;
    else if (work == Work::scene_publication)
    {
        ++stats.scene_publications_in_flight;
        stats.scene_bytes_in_flight += bytes;
        ++stats.scene_publications_this_frame;
        stats.scene_bytes_this_frame += bytes;
        if (bytes > limits.scene_bytes_per_frame) state.oversized_scene_in_flight = true;
        auto& history = state.scene_history[kind];
        history.last_service = stats.frame;history.serviced_oldest = oldest_frame;
        if (history.current_denial && history.current_denial->oldest == oldest_frame)
            history.current_denial.reset();
    }
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
    if (stats.draining) return maximum;
    if (!stats.initialized)
    {
        // Preserve existing startup upload/preparation behavior, while an
        // actual scene receipt already owns bounded downstream work even
        // before the first outer frame plan has been published.
        if (stats.scene_publications_in_flight >= state.limits.scene_publications_in_flight ||
            stats.scene_bytes_in_flight >= state.limits.scene_bytes_in_flight)
            return minimum;
        return maximum - std::min(maximum - minimum, stats.scene_publications_in_flight);
    }
    const bool downstream_full = stats.streaming.queued_requests >= state.limits.uploads_in_flight ||
        stats.streaming.queued_input_bytes >= state.limits.upload_bytes_in_flight ||
        stats.streaming.pending_publications >= state.limits.uploads_in_flight ||
        stats.uploads_in_flight >= state.limits.uploads_in_flight ||
        stats.upload_bytes_in_flight >= state.limits.upload_bytes_in_flight ||
        stats.scene_publications_in_flight >= state.limits.scene_publications_in_flight ||
        stats.scene_bytes_in_flight >= state.limits.scene_bytes_in_flight ||
        stats.streaming.pressure_requests != 0;
    if (stats.main_busy || stats.memory_pressure || downstream_full) return minimum;
    const auto active_native_debt = std::uint64_t(stats.preparation_in_flight) + stats.scene_publications_in_flight;
    return maximum - static_cast<unsigned>(std::min<std::uint64_t>(maximum - minimum, active_native_debt));
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
