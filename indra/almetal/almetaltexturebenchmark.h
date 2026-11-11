/**
 * @file almetaltexturebenchmark.h
 * @brief The Developer menu's fixed texture-sampling workload, owned by Metal.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#pragma once

#include "almetaldevice.h"
#include <cstdint>
#include <memory>
#include <span>
#include <string>

class ALMetalTexture;

struct ALMetalTextureBenchmarkResult
{
    ALMetalCommandGPUTime gpu_time;
    std::uint64_t samples_drawn = 0;
    std::uint64_t sampled_bytes = 0;
    double gigabytes_per_second = 0.0; // Decimal GB, four bytes per passing sample.
};

class ALMetalTextureBenchmarkRun final
{
public:
    enum class Status { pending, ready, failed };
    ~ALMetalTextureBenchmarkRun();
    ALMetalTextureBenchmarkRun(const ALMetalTextureBenchmarkRun&) = delete;
    ALMetalTextureBenchmarkRun& operator=(const ALMetalTextureBenchmarkRun&) = delete;
    bool commit(std::string* error = nullptr);
    // No submission or wait. Only completed GPU timestamps and visibility
    // counts can produce a result; every other status clears output.
    Status poll(ALMetalTextureBenchmarkResult& output, std::string* error = nullptr) const;
    // Explicit manual benchmark only. Bounded to thirty seconds; CPU elapsed
    // time is a timeout, never a substitute for GPU execution time.
    bool wait(ALMetalTextureBenchmarkResult& output, std::string* error = nullptr) const;
    const ALMetalTexture* outputTexture() const noexcept;
    std::shared_ptr<const ALMetalCommandCompletion> completion() const noexcept;
private:
    struct Impl;
    explicit ALMetalTextureBenchmarkRun(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
    friend class ALMetalTextureBenchmark;
};

class ALMetalTextureBenchmark final
{
public:
    // Original1eb44 llglsandbox.cpp: 32 1024-square RGBA8 sources and
    // targets, 64 passes through all sources into target zero, two runs.
    static constexpr std::uint32_t resolution = 1024;
    static constexpr std::uint32_t texture_count = 32;
    static constexpr std::uint32_t iterations = 64;
    static constexpr std::uint32_t run_count = 2;
    static constexpr std::size_t source_bytes = resolution * resolution * 4;
    ~ALMetalTextureBenchmark();
    ALMetalTextureBenchmark(const ALMetalTextureBenchmark&) = delete;
    ALMetalTextureBenchmark& operator=(const ALMetalTextureBenchmark&) = delete;
    // Each source receives the same caller-provided random RGBA8 bytes.
    // Uploads and all target clears complete outside measured commands.
    // Reject an active timing diagnostic or Metal debugger capture rather
    // than changing its state or contaminating its measurements.
    static std::unique_ptr<ALMetalTextureBenchmark> create(
        const ALMetalDevice& device, std::span<const std::uint8_t> pixels,
        std::string* error = nullptr);
    // Dedicated, initially unsubmitted command. One run at a time on this
    // owner's targets. Runs and native submissions retain their resources
    // independently of the owner; destruction never waits for the GPU.
    std::unique_ptr<ALMetalTextureBenchmarkRun> encodeRun(std::string* error = nullptr);
private:
    struct Impl;
    explicit ALMetalTextureBenchmark(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
