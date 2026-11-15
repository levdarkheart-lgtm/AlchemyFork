/**
 * @file alnativeframebudget.h
 * @brief Shared native frame admission for streaming and optional scene work.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

// A native viewer policy, not a task executor. The main owner publishes one
// plan per outer frame. Worker/GPU owners retain credits through their actual
// lifetime; publication remains ordered by command commitment, not completion.
class ALNativeFrameBudget final
{
    struct State;
public:
    using Frame = std::uint64_t;
    static constexpr std::uint64_t unknown_bytes = std::numeric_limits<std::uint64_t>::max();
    enum class Work { upload, ordinary_probe, hero_probe, preparation, scene_publication };
    enum class ScenePublicationKind { native_metadata, cohort_compilation };
    enum class Reason { admitted, progress, frame_work, in_flight, memory_pressure, draining };
    struct Limits
    {
        unsigned uploads_per_frame = 16;
        unsigned uploads_in_flight = 32;
        std::uint64_t upload_bytes_per_frame = 32ULL * 1024 * 1024;
        std::uint64_t upload_bytes_in_flight = 64ULL * 1024 * 1024;
        unsigned scene_publications_per_frame = 16;
        unsigned scene_publications_in_flight = 32;
        std::uint64_t scene_bytes_per_frame = 8ULL * 1024 * 1024;
        std::uint64_t scene_bytes_in_flight = 32ULL * 1024 * 1024;
        unsigned optional_views_per_frame = 2;
        unsigned preparation_jobs = 2;
        Frame maximum_deferral_frames = 8;
    };
    struct FrameInputs
    {
        Frame frame = 0;
        std::uint64_t frame_interval_ns = 0;
        std::uint64_t required_main_cpu_ns = 0;
        std::uint64_t native_allocated_bytes = 0;
        std::uint64_t native_budget_bytes = 0; // zero: no supplied native bound
        std::uint64_t os_available_bytes = unknown_bytes;
        std::uint64_t process_headroom_bytes = unknown_bytes;
        std::uint64_t minimum_headroom_bytes = 512ULL * 1024 * 1024;
    };
    struct StreamingBacklog
    {
        std::uint64_t queued_requests = 0;
        std::uint64_t queued_input_bytes = 0; // logical input, not unique resident RAM
        Frame oldest_request_frame = 0;
        std::uint64_t pending_publications = 0;
        std::uint64_t pressure_requests = 0;
    };
    struct Snapshot
    {
        Frame frame = 0;
        bool initialized = false;
        bool draining = false;
        bool main_busy = false;
        bool memory_pressure = false;
        StreamingBacklog streaming;
        std::uint64_t upload_bytes_in_flight = 0;
        unsigned uploads_in_flight = 0;
        std::uint64_t scene_bytes_in_flight = 0;
        unsigned scene_publications_in_flight = 0;
        unsigned preparation_in_flight = 0;
        unsigned optional_views_in_flight = 0;
        unsigned uploads_this_frame = 0;
        unsigned scene_publications_this_frame = 0;
        unsigned optional_views_this_frame = 0;
        std::uint64_t upload_bytes_this_frame = 0;
        std::uint64_t scene_bytes_this_frame = 0;
        std::uint64_t admitted = 0;
        std::uint64_t deferred = 0;
        std::uint64_t progress_admissions = 0;
        std::uint64_t retired = 0;
    };
    class Credit final
    {
    public:
        ~Credit();
        Credit(const Credit&) = delete;
        Credit& operator=(const Credit&) = delete;
        Work work() const noexcept { return mWork; }
        std::uint64_t logicalBytes() const noexcept { return mBytes; }
    private:
        friend class ALNativeFrameBudget;
        Credit(std::shared_ptr<State>, Work, std::uint64_t);
        std::shared_ptr<State> mState;
        Work mWork;
        std::uint64_t mBytes;
        bool mAcquired = false;
    };
    using Lease = std::shared_ptr<const Credit>;
    struct Decision
    {
        Lease credit;
        Reason reason = Reason::frame_work;
        explicit operator bool() const noexcept { return bool(credit); }
    };

    ALNativeFrameBudget();
    explicit ALNativeFrameBudget(Limits);
    ALNativeFrameBudget(const ALNativeFrameBudget&) = delete;
    ALNativeFrameBudget& operator=(const ALNativeFrameBudget&) = delete;
    static ALNativeFrameBudget& instance();

    // Duplicate/nested or stale frame starts cannot refill the current plan.
    void beginFrame(const FrameInputs&);
    void recordRequiredMainWork(std::uint64_t cpu_nanoseconds);
    void updateStreaming(const StreamingBacklog&);
    void setPendingPublications(std::uint64_t);
    // Scene publication accounts one coalesced dirty source batch, separately
    // from image uploads and optional views. logical_bytes charges encoded and
    // copied/zeroed source metadata work; the native allocator independently
    // bounds actual resident metadata. Pass its actual oldest dirty frame.
    // Warm views with no dirty sources need no scene publication credit.
    // Busy progress is shared fairly by the early CPU compiler and later
    // native metadata publishers. Recent denials reserve only a finite next
    // frame opportunity; no requester, resource, job or credit is retained.
    // Existing callbacks default to native metadata. Idle quotas are shared
    // exactly as before, independently of this pressure-only service order.
    Decision tryAcquire(Work, std::uint64_t logical_bytes = 0, Frame oldest_frame = 0,
                        ScenePublicationKind = ScenePublicationKind::native_metadata);
    // Downstream debt constrains additional starts; active jobs and responders
    // are never cancelled. At least the established decoder minimum survives.
    unsigned decodeCeiling(unsigned minimum, unsigned current, unsigned maximum) const;
    Snapshot snapshot() const;
    // Shutdown must not depend on future frame ticks or admission receipts.
    void beginDrain();
private:
    std::shared_ptr<State> mState;
};

// Scene-owner state for OPTIONAL synchronous cohort compilation. The caller
// checks the immutable warm publication first, then orders pending owners by
// priority() before calling tryAcquire(). This is neither a queue nor an
// executor. One state belongs to one owner in one frame-budget domain; it
// stores no credit, native resource, producer publication or viewer pointer.
class ALNativeCohortCompilationAdmission final
{
public:
    using Frame = ALNativeFrameBudget::Frame;
    struct ProducerStamp
    {
        std::uint64_t identity = 0, revision = 0;
        bool operator==(const ProducerStamp&) const = default;
    };
    struct Priority
    {
        Frame unserved_since = 0;
        std::uint64_t stable_tie = 0;
        bool operator<(const Priority& other) const noexcept
        {
            return unserved_since < other.unserved_since ||
                (unserved_since == other.unserved_since && stable_tie < other.stable_tie);
        }
    };

    // Repeated dirty revisions do not restart pending age. Frame zero is a
    // valid startup frame, distinct from an owner with no pending work.
    void notePending(Frame) noexcept;
    bool pending() const noexcept { return mPendingSince.has_value(); }
    std::optional<Frame> pendingSince() const noexcept { return mPendingSince; }
    std::optional<Frame> lastConsideredFrame() const noexcept { return mLastConsidered; }
    std::optional<Frame> lastAdmittedFrame() const noexcept { return mLastAdmitted; }
    // Pending owners use max(original pending age, last ADMITTED service).
    // Thus failed compilation rejoins behind older unserved work; budget
    // denial changes no service priority. Nonpending owners sort last and
    // must be filtered by the caller. stable_tie must be stable and unique
    // among that caller's candidates (for example, publication identity).
    Priority priority(std::uint64_t stable_tie) const noexcept;
    // Optional owner-revision heuristic, before estimating/compiling work.
    // A first/new stamp defers without touching the shared budget. Only the
    // same stamp observed in a later outer frame becomes eligible. Unstable
    // consideration still consumes this owner's frame, including probes.
    // Ready callers proceed immediately to tryAcquire(); no credit is stored.
    // An absent stamp preserves the existing immediate-admission behavior.
    bool deferUnstableRevision(Frame, std::optional<ProducerStamp> = std::nullopt) noexcept;
    // At most one consideration per owner/outer frame, including denial or
    // failure. logical_bytes is checked compilation/copy/proof work, not
    // resident geometry/texture size or a wall-clock bound. unknown_bytes is
    // reserved for saturated/overflowed work and rejected without admission.
    // The returned credit is local to synchronous compile + publication;
    // never put it in idle publications or on later required GPU commands.
    ALNativeFrameBudget::Decision tryAcquire(ALNativeFrameBudget&, std::uint64_t logical_bytes);
    // Call ONLY after the complete immutable publication actually succeeds.
    // Admission alone, failure, or a new dirty revision does not reset age.
    // Per-frame consideration/service history intentionally survives reset.
    void publicationCompleted() noexcept;
private:
    std::optional<Frame> mPendingSince;
    std::optional<Frame> mLastConsidered;
    std::optional<Frame> mLastAdmitted;
    std::optional<ProducerStamp> mObservedProducer;
    std::optional<Frame> mFirstObservedFrame, mLastObservedFrame;
};
