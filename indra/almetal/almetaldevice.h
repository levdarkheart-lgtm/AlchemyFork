/**
 * @file almetaldevice.h
 * @brief Apple Silicon Metal device, queue and command-buffer ownership.
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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

struct ALMetalDeviceInfo
{
    std::string name;
    std::uint64_t registry_id = 0;
    std::uint64_t recommended_working_set_size = 0;
    std::uint64_t max_buffer_length = 0;
    std::uint64_t current_allocated_size = 0;
    std::uint32_t max_texture_2d_dimension = 0;
    std::uint32_t highest_apple_family = 0;
    std::uint32_t highest_mac_family = 0;
    std::uint32_t max_threads_per_threadgroup_width = 0;
    std::uint32_t max_threads_per_threadgroup_height = 0;
    std::uint32_t max_threads_per_threadgroup_depth = 0;
    bool unified_memory = false;
    bool low_power = false;
    bool removable = false;
    bool headless = false;
};

struct ALMetalCommandTiming
{
    std::uint64_t submitted = 0;
    std::uint64_t completed = 0;
    std::uint64_t timed = 0;
    std::uint64_t gpu_time_nanoseconds = 0;
};

enum class ALMetalCommandStatus
{
    not_enqueued,
    enqueued,
    committed,
    scheduled,
    completed,
    error,
    unknown,
};

enum class ALMetalCommandTimingStatus
{
    pending,
    ready,
    failed,
};

struct ALMetalCommandGPUTime
{
    double start_seconds = 0.0;
    double end_seconds = 0.0;
    std::uint64_t elapsed_nanoseconds = 0;
};

// Retains the native completion identity without retaining its C++ command
// wrapper. Resource arenas can poll this after a standalone wrapper has gone
// away; neither polling nor destruction waits for the GPU. An uncommitted
// command discarded by its wrapper is also complete for resource reuse.
class ALMetalCommandCompletion final
{
public:
    ~ALMetalCommandCompletion();
    bool isComplete() const noexcept;

    // Poll this command's native GPU execution interval after completion.
    // Queue delay and CPU encoding/commit time are excluded. The interval is
    // the whole command's GPU envelope, not GPU-busy or individual draw time.
    // This does not
    // submit, wait, install callbacks or depend on global timing telemetry.
    // Abandonment, native failure or unusable timestamps fail closed; output
    // is cleared unless ready. The ticket can outlive its command wrapper.
    ALMetalCommandTimingStatus gpuTime(ALMetalCommandGPUTime& output) const noexcept;

private:
    struct Impl;
    explicit ALMetalCommandCompletion(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
    friend class ALMetalCommandBuffer;
};

class ALMetalCommandBuffer final
{
public:
    ALMetalCommandBuffer(const ALMetalCommandBuffer&) = delete;
    ALMetalCommandBuffer& operator=(const ALMetalCommandBuffer&) = delete;
    ALMetalCommandBuffer(ALMetalCommandBuffer&&) = delete;
    ALMetalCommandBuffer& operator=(ALMetalCommandBuffer&&) = delete;
    ~ALMetalCommandBuffer();

    // Non-owning Objective-C handles. Only Objective-C++ encoder code should
    // bridge these to id<MTLCommandBuffer>/id<MTLDevice>; their lifetime is
    // bounded by this wrapper.
    void* nativeHandle() const noexcept;
    void* nativeDevice() const noexcept;

    ALMetalCommandStatus status() const noexcept;
    bool isCommitted() const noexcept;
    std::shared_ptr<const ALMetalCommandCompletion> completionToken() const;
    std::string errorDescription() const;

    // Retain indirect/argument-buffer resource owners through actual GPU
    // completion, including when their renderer and this wrapper are gone.
    // Owners added before submission are released on abandonment. The native
    // callback is installed only at commit, avoiding an uncommitted cycle.
    bool retainUntilCompletion(std::shared_ptr<const void> owner,
                               std::string* error = nullptr);

    bool commit(std::string* error = nullptr);
    bool waitUntilCompleted(std::string* error = nullptr);
    bool commitAndWait(std::string* error = nullptr);

private:
    struct Impl;
    explicit ALMetalCommandBuffer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;

    friend class ALMetalDevice;
};

class ALMetalDevice final
{
public:
    ALMetalDevice(const ALMetalDevice&) = delete;
    ALMetalDevice& operator=(const ALMetalDevice&) = delete;
    ALMetalDevice(ALMetalDevice&&) = delete;
    ALMetalDevice& operator=(ALMetalDevice&&) = delete;
    ~ALMetalDevice();

    // Fails closed when the process is not native arm64 or the selected Metal
    // device is not a built-in unified-memory Apple GPU.
    static std::unique_ptr<ALMetalDevice> create(std::string* error = nullptr);

    const ALMetalDeviceInfo& info() const noexcept;
    // Live driver-accounted bytes owned by this Metal device. Unlike the
    // creation-time value in info(), this is sampled at the call site and is
    // intended for resource-lifetime diagnostics and benchmark evidence.
    std::uint64_t currentAllocatedSize() const noexcept;
    void* nativeHandle() const noexcept;
    void* nativeCommandQueue() const noexcept;

    // Explicit Develop-menu crash-reporting test only. Deliberately passes
    // invalid client memory to Metal so the crash has a graphics API stack;
    // never enqueues GPU work and never returns to the viewer.
    [[noreturn]] void forceDriverCrash() const;

    // Process-wide count of command buffers committed through the viewer's
    // native Metal wrapper. Monotonic deltas provide low-overhead per-frame
    // submission telemetry; this is not a GPU-time measurement.
    static std::uint64_t committedCommandBufferCount() noexcept;

    // Process-wide cumulative census supplied only by explicitly armed
    // encoder collectors. Count direct draws and populated draw commands in
    // executed indirect ranges, not just indirect execution API calls.
    // Encoders aggregate locally and publish once, avoiding per-draw atomic
    // work. This counts encoded work even if its command is later discarded;
    // it is not a committed/completed or GPU-time counter.
    static std::uint64_t encodedDrawCallCount() noexcept;
    static void addEncodedDrawCalls(std::uint64_t count) noexcept;

    // Default-off development telemetry for explicit benchmark collection.
    // submitted/completed/timed and GPU duration are process-wide monotonic
    // counters for commands committed while enabled. GPU duration is read
    // only after native completion and is app-owned Metal work, not complete
    // frame GPU time or legacy-driver work.
    static void setCommandTimingEnabled(bool enabled) noexcept;
    static bool commandTimingEnabled() noexcept;
    static ALMetalCommandTiming commandTiming() noexcept;

    std::unique_ptr<ALMetalCommandBuffer> makeCommandBuffer(
        const std::string& label,
        std::string* error = nullptr) const;

    // Submit an empty command buffer and wait for it. This is an explicit
    // shutdown/testing primitive, not a per-frame synchronization path.
    bool waitUntilIdle(std::string* error = nullptr) const;

private:
    struct Impl;
    explicit ALMetalDevice(std::unique_ptr<Impl> impl, ALMetalDeviceInfo info);
    std::unique_ptr<Impl> mImpl;
    ALMetalDeviceInfo mInfo;
};
