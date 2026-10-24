/**
 * @file almetalframe.h
 * @brief Bounded Metal frames-in-flight and command completion ownership.
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

#include <cstdint>
#include <memory>
#include <string>

struct ALMetalFrameStats
{
    std::uint64_t begun = 0;
    std::uint64_t submitted = 0;
    std::uint64_t completed = 0;
    std::uint64_t failed = 0;
    std::uint32_t in_flight = 0;
    std::uint32_t maximum_in_flight = 0;
};

class ALMetalFrame final
{
public:
    ALMetalFrame(const ALMetalFrame&) = delete;
    ALMetalFrame& operator=(const ALMetalFrame&) = delete;
    ALMetalFrame(ALMetalFrame&&) = delete;
    ALMetalFrame& operator=(ALMetalFrame&&) = delete;
    ~ALMetalFrame();

    ALMetalCommandBuffer& commandBuffer() noexcept;
    const ALMetalCommandBuffer& commandBuffer() const noexcept;
    std::uint64_t serial() const noexcept;
    std::uint32_t slot() const noexcept;
    bool wasSubmitted() const noexcept;

private:
    struct Impl;
    explicit ALMetalFrame(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;

    friend class ALMetalFrameScheduler;
};

class ALMetalFrameScheduler final
{
public:
    ALMetalFrameScheduler(const ALMetalFrameScheduler&) = delete;
    ALMetalFrameScheduler& operator=(const ALMetalFrameScheduler&) = delete;
    ALMetalFrameScheduler(ALMetalFrameScheduler&&) = delete;
    ALMetalFrameScheduler& operator=(ALMetalFrameScheduler&&) = delete;
    ~ALMetalFrameScheduler();

    static std::unique_ptr<ALMetalFrameScheduler> create(
        const ALMetalDevice& device,
        std::uint32_t maximum_in_flight = 3,
        std::string* error = nullptr);

    // Waits for an available frame slot. A finite timeout is useful for
    // diagnostics and shutdown; UINT32_MAX waits without a deadline.
    std::unique_ptr<ALMetalFrame> beginFrame(
        const std::string& label,
        std::uint32_t timeout_milliseconds = UINT32_MAX,
        std::string* error = nullptr);

    // Adds completion accounting and commits the frame's command buffer.
    // Presentation, when required, must already have been encoded on it.
    bool submit(ALMetalFrame& frame, std::string* error = nullptr);

    bool waitUntilIdle(std::uint32_t timeout_milliseconds = UINT32_MAX,
                       std::string* error = nullptr) const;
    ALMetalFrameStats stats() const noexcept;
    std::string lastError() const;

private:
    struct Impl;
    explicit ALMetalFrameScheduler(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
