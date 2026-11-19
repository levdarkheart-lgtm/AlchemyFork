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
    enum class Work { upload, ordinary_probe, hero_probe, preparation, scene_publication, native_maintenance };
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
        // Initial bounded policy for native texture reduction copies, separate
        // from raw image uploads and scene metadata. Logical destination bytes
        // include every reduced mip, not another physical resident RAM total.
        unsigned maintenance_per_frame = 2;
        unsigned maintenance_in_flight = 2;
        std::uint64_t maintenance_bytes_per_frame = 16ULL * 1024 * 1024;
        std::uint64_t maintenance_bytes_in_flight = 32ULL * 1024 * 1024;
        unsigned optional_views_per_frame = 2;
        unsigned preparation_jobs = 2;
        // Logical retained CPU packet capacities while the next producer
        // allocates; exclude already-owned native resources/physical RAM.
        std::uint64_t preparation_bytes_in_flight = 32ULL * 1024 * 1024;
        Frame maximum_deferral_frames = 8;
    };
    struct FrameInputs
    {
        Frame frame = 0;
        // The outer viewer owner certifies uninterrupted normal-world
        // lifecycle across this frame. Startup, teleport, disconnect/logout
        // and reset callers leave this false; unknown contexts fail closed.
        bool main_pressure_continuity = false;
        std::uint64_t frame_interval_ns = 0;
        std::uint64_t required_main_cpu_ns = 0;
        std::uint64_t native_allocated_bytes = 0;
        // Unique viewer-owned backing leases: Metal-reported allocation sizes
        // plus declared sizes ONLY for records without that reported size.
        // This overlaps the driver total and must never be added to it.
        std::uint64_t native_owned_reported_bytes = 0;
        std::uint64_t native_owned_unknown_declared_bytes = 0;
        bool native_owned_accounting = false;
        // Registered typed-use tracking is enabled. Opaque/native-only uses
        // and external allocations remain unknown; this is not exhaustive
        // completion coverage or physical residency.
        bool native_registered_use_tracking = false;
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
        // Soft CPU decode-ahead accounting, independent of GPU publication credits.
        std::uint64_t decode_estimates_in_flight = 0;
        std::uint64_t decode_estimated_bytes_in_flight = 0;
        std::uint64_t decode_unknown_estimates_in_flight = 0;
        bool decode_estimate_saturated = false;
        bool decode_ahead_headroom_known = false;
        std::uint64_t decode_ahead_target_bytes = 0;
        std::uint64_t decode_ahead_release_bytes = 0;
        std::uint64_t decode_ahead_pressure_bytes = 0;
        bool decode_ahead_pressure_saturated = false;
        bool decode_ahead_pressure = false;
        std::uint64_t native_driver_allocated_bytes = 0;
        std::uint64_t native_owned_reported_bytes = 0;
        std::uint64_t native_owned_unknown_declared_bytes = 0;
        std::uint64_t native_admission_allocated_bytes = 0;
        std::uint64_t native_budget_bytes = 0;
        bool native_owned_accounting = false, native_registered_use_tracking = false;
        bool native_allocation_estimate_saturated = false;
        // Current-frame admission operands, not GPU execution/busy time.
        // Wait CPU is already included in required_main_cpu_ns. Only the
        // measured non-CPU remainder is an additional pressure component.
        std::uint64_t required_main_cpu_ns = 0;
        std::uint64_t required_main_gpu_wait_wall_ns = 0;
        std::uint64_t required_main_gpu_wait_thread_cpu_ns = 0;
        std::uint64_t required_main_gpu_wait_non_cpu_ns = 0;
        std::uint64_t required_main_gpu_wait_observations = 0; // Includes unknown CPU clocks.
        std::uint64_t required_main_gpu_wait_unknown_observations = 0;
        // A one-predecessor admission hint, NOT another measured wait in this
        // frame. Only a closed, consecutive frame on the same owner may seed
        // it, with continuity certified at both frame boundaries. Unknown
        // CPU clocks contribute no inferred non-CPU duration to this hint.
        Frame previous_main_gpu_wait_frame = 0;
        std::uint64_t previous_main_gpu_wait_non_cpu_ns = 0;
        bool previous_main_gpu_wait_valid = false;
        // max(previous hint, current proven non-CPU waits), never their sum.
        std::uint64_t required_main_gpu_pressure_ns = 0;
        std::uint64_t required_main_pressure_ns = 0;
        bool required_main_gpu_wait_saturated = false;
        bool required_main_pressure_saturated = false;
        bool main_observations_open = false;
        StreamingBacklog streaming;
        std::uint64_t upload_bytes_in_flight = 0;
        unsigned uploads_in_flight = 0;
        std::uint64_t scene_bytes_in_flight = 0;
        unsigned scene_publications_in_flight = 0;
        unsigned maintenance_in_flight = 0;
        std::uint64_t maintenance_bytes_in_flight = 0;
        unsigned preparation_in_flight = 0;
        std::uint64_t preparation_bytes_in_flight = 0;
        unsigned optional_views_in_flight = 0;
        unsigned uploads_this_frame = 0;
        unsigned scene_publications_this_frame = 0;
        unsigned maintenance_this_frame = 0;
        unsigned optional_views_this_frame = 0;
        std::uint64_t upload_bytes_this_frame = 0;
        std::uint64_t scene_bytes_this_frame = 0;
        std::uint64_t maintenance_bytes_this_frame = 0;
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
    // Accounting only: this never rejects an accepted decode. Keep it through
    // the responder; it does not claim to own all retained raw/codec memory.
    class DecodeEstimate final
    {
    public:
        DecodeEstimate() noexcept = default;
        DecodeEstimate(DecodeEstimate&&) noexcept;
        DecodeEstimate& operator=(DecodeEstimate&&) noexcept;
        DecodeEstimate(const DecodeEstimate&) = delete;
        DecodeEstimate& operator=(const DecodeEstimate&) = delete;
        ~DecodeEstimate();
        void reset() noexcept;
    private:
        friend class ALNativeFrameBudget;
        DecodeEstimate(std::shared_ptr<State>, std::uint64_t bytes, bool known);
        std::shared_ptr<State> mState;
        std::uint64_t mBytes = 0;
        bool mKnown = true;
    };
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
    DecodeEstimate beginDecodeEstimate(std::uint64_t bytes, bool known = true);

    // Duplicate/nested or stale frame starts cannot refill the current plan.
    void beginFrame(const FrameInputs&);
    void recordRequiredMainWork(std::uint64_t cpu_nanoseconds);
    // Report ONLY the actual older-required-main-group admission wait. Capture
    // frame before waiting; utility/probe/presentation waits are not inputs.
    // The accepted beginFrame caller owns observations until explicit close.
    // Wrong thread/frame, closed, uninitialized and draining reports return
    // false without changing counters or admission. nullopt records unknown
    // CPU-clock incidence and wall time, but invents no blocked-time operand.
    bool recordRequiredMainGpuWait(Frame frame, std::uint64_t wall_nanoseconds,
                                  std::optional<std::uint64_t> thread_cpu_nanoseconds);
    // Retains final pressure/counters for later phases; the next beginFrame
    // resets observations. Only this frame's observation owner may close it.
    bool closeMainObservations(Frame frame);
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
    // Optional service_order changes only ordering between already-aged
    // requests. It never replaces the genuine dirty age used for progress.
    Decision tryAcquire(Work, std::uint64_t logical_bytes = 0, Frame oldest_frame = 0,
                        ScenePublicationKind = ScenePublicationKind::native_metadata,
                        std::optional<Frame> service_order = std::nullopt);
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
