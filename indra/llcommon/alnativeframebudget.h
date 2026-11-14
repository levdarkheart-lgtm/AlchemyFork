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
    Decision tryAcquire(Work, std::uint64_t logical_bytes = 0, Frame oldest_frame = 0);
    // Downstream debt constrains additional starts; active jobs and responders
    // are never cancelled. At least the established decoder minimum survives.
    unsigned decodeCeiling(unsigned minimum, unsigned current, unsigned maximum) const;
    Snapshot snapshot() const;
    // Shutdown must not depend on future frame ticks or admission receipts.
    void beginDrain();
private:
    std::shared_ptr<State> mState;
};
