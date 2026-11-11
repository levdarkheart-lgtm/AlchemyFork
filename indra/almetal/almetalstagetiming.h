/**
 * @file almetalstagetiming.h
 * @brief Explicit bounded native stage-timestamp diagnostics.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */
#pragma once

#include "almetaldevice.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class ALMetalRenderPass;
class ALMetalStageTimingWindow;

// Passive, thread-local caller context. It does not change graphics state,
// create a window, query a device or allocate. The caller keeps window alive
// until this scope is destroyed. Nested scopes restore their predecessor.
class ALMetalStageTimingScope final
{
public:
    ALMetalStageTimingScope(ALMetalStageTimingWindow* window, std::uint64_t frame,
                           std::uint32_t view_kind, std::int32_t face) noexcept;
    ~ALMetalStageTimingScope();
    ALMetalStageTimingScope(const ALMetalStageTimingScope&) = delete;
    ALMetalStageTimingScope& operator=(const ALMetalStageTimingScope&) = delete;
    static const ALMetalStageTimingScope* current() noexcept;
    ALMetalStageTimingWindow* window() const noexcept { return mWindow; }
    std::uint64_t frame() const noexcept { return mFrame; }
    std::uint32_t viewKind() const noexcept { return mViewKind; }
    std::int32_t face() const noexcept { return mFace; }
private:
    ALMetalStageTimingWindow* mWindow;
    const ALMetalStageTimingScope* mPrevious;
    std::uint64_t mFrame;
    std::uint32_t mViewKind;
    std::int32_t mFace;
};

struct ALMetalStageTimingTag
{
    std::uint64_t frame = 0;
    std::uint32_t view_kind = 0; // Caller-owned metadata: main1, hero2, ordinary3.
    std::int32_t face = -1;
    std::string label;
};

enum class ALMetalStageTimingStatus
{
    complete,
    unsubmitted_at_seal,
    not_encoded,
    gpu_error,
    counter_unavailable,
    calibration_unavailable
};

struct ALMetalStageTimingCalibration
{
    std::uint64_t cpu_before_ns = 0, cpu_after_ns = 0;
    std::uint64_t gpu_before_ticks = 0, gpu_after_ticks = 0;
};

struct ALMetalStageTimingElapsed
{
    double vertex_ns = 0;
    double fragment_ns = 0;
    double compute_ns = 0;
};

struct ALMetalStageTimingPipeline
{
    std::string vertex, fragment, compute;
    std::uintptr_t native_pipeline = 0; // Report-local PSO identity, never dereferenced.
    bool operator==(const ALMetalStageTimingPipeline&) const = default;
};

struct ALMetalStageTimingCapabilities
{
    bool stage_boundary = false, draw_boundary = false, dispatch_boundary = false;
    bool timestamps = false, statistics = false;
};

struct ALMetalStageTimingResult
{
    ALMetalStageTimingTag tag;
    std::size_t pass_index = 0;
    std::size_t first_sample = 0; // Local to this record's command sample buffer.
    ALMetalStageTimingStatus status = ALMetalStageTimingStatus::counter_unavailable;
    std::int64_t gpu_error_code = 0;
    bool compute = false;
    // Executable identities used in this physical encoder, NOT independently
    // timed shaders. A mixed encoder's duration cannot be assigned to each.
    std::vector<ALMetalStageTimingPipeline> pipelines;
    std::size_t omitted_pipelines = 0;
    std::optional<std::array<std::uint64_t, 4>> raw_samples;
    std::optional<ALMetalStageTimingCalibration> calibration;
    // Present ONLY for complete. These are elapsed stage intervals, never a
    // busy-time union, additive total, utilization or shader-line measurement.
    std::optional<ALMetalStageTimingElapsed> elapsed;
};

// Explicit caller-owned diagnostic window. Nothing is global or environment
// driven: the normal renderer has no window and performs no counter work.
// Calls, including poll(), belong to the owning CPU thread. No GPU waits,
// submissions, worker callbacks or per-draw sampling are performed here.
class ALMetalStageTimingWindow final
{
public:
    static constexpr std::size_t maximum_passes = 256;
    class PreparedPass final
    {
    public:
        ~PreparedPass();
        PreparedPass(const PreparedPass&) = delete;
        PreparedPass& operator=(const PreparedPass&) = delete;
        // Consume once, immediately before starting the actual native encoder
        // on this SAME uncommitted command. Null means use the original pass.
        void* consume(ALMetalCommandBuffer& command, std::string* error = nullptr) noexcept;
        // If native counter-encoder creation fails after consume(), call this
        // BEFORE falling back to the original descriptor. No unwritten/stale
        // counter range may then be resolved; the command's buffer lease stays.
        void abandon() noexcept;
        std::size_t firstSampleIndex() const noexcept;
        void recordPipeline(const std::string& vertex, const std::string& fragment,
                            const std::string& compute = {}, std::uintptr_t native_pipeline = 0) noexcept;
    private:
        struct Impl;
        explicit PreparedPass(std::unique_ptr<Impl> impl);
        std::unique_ptr<Impl> mImpl;
        friend class ALMetalStageTimingWindow;
    };

    static std::unique_ptr<ALMetalStageTimingWindow> create(
        const ALMetalDevice& device, std::size_t max_passes = maximum_passes,
        std::string* error = nullptr, bool frame_profile = false) noexcept;
    static ALMetalStageTimingCapabilities capabilities(const ALMetalDevice& device) noexcept;
    ~ALMetalStageTimingWindow();
    ALMetalStageTimingWindow(const ALMetalStageTimingWindow&) = delete;
    ALMetalStageTimingWindow& operator=(const ALMetalStageTimingWindow&) = delete;

    // Copies the original native descriptor; never mutates it. Failure is
    // diagnostic-only and leaves the original pass usable. One four-index
    // range is reserved per physical normal encoder or parallel parent only.
    std::unique_ptr<PreparedPass> prepare(ALMetalCommandBuffer& command,
        const ALMetalRenderPass& pass, const ALMetalStageTimingTag& tag,
        std::string* error = nullptr) noexcept;
    // Manual profile only: sample the physical compute encoder's start/end.
    // Existing render-stage diagnostics retain their original admission scope.
    std::unique_ptr<PreparedPass> prepareCompute(ALMetalCommandBuffer& command,
        const ALMetalStageTimingTag& tag, std::string* error = nullptr) noexcept;
    // Ends admission. Not-yet-committed commands are explicitly unavailable,
    // even if their caller commits later. Their sample buffers remain owned by
    // their command, but this window will never resolve those samples.
    bool seal(std::string* error = nullptr) noexcept;
    // On success replaces ready with new terminal reports; failure leaves
    // ready unchanged. Only committed, successfully
    // completed command ranges are resolved. No waits or new commits occur.
    bool poll(std::vector<ALMetalStageTimingResult>& ready,
              std::string* error = nullptr) noexcept;
    bool isSealed() const noexcept;
    bool isFinished() const noexcept;
    std::size_t preparedPassCount() const noexcept;
    std::size_t pendingPassCount() const noexcept;
    std::size_t rejectedPrepareCount() const noexcept;
private:
    struct Impl;
    explicit ALMetalStageTimingWindow(std::unique_ptr<Impl> impl);
    std::unique_ptr<PreparedPass> prepareImpl(ALMetalCommandBuffer& command,
        const ALMetalRenderPass* pass, const ALMetalStageTimingTag& tag,
        bool compute, std::string* error) noexcept;
    std::unique_ptr<Impl> mImpl;
};
