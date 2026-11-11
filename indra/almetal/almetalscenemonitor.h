/**
 * @file almetalscenemonitor.h
 * @brief Owned asynchronous native scene-loading pixel differences.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#pragma once

#include "almetalretainedbackground.h"
#include <cstdint>
#include <memory>
#include <string>

struct ALMetalSceneDifferenceConfig
{
    // Original copyFromWindow captured this lower-left rectangle of the
    // display-sized backing, then point-resampled it into the full diff.
    std::uint32_t capture_width = 0, capture_height = 0;
    std::uint32_t diff_width = 0, diff_height = 0;
    float sample_ratio = 0.5f;
    float tolerance = 0.1f;
};

class ALMetalSceneDifference final
{
public:
    enum class Status { pending, ready, failed };
    ~ALMetalSceneDifference();
    ALMetalSceneDifference(const ALMetalSceneDifference&) = delete;
    ALMetalSceneDifference& operator=(const ALMetalSceneDifference&) = delete;
    Status status() const noexcept;
    // No wait: output arguments are untouched unless a completed result is valid.
    bool result(std::uint64_t& changed_samples, float& normalized_difference,
                std::string* error = nullptr) const;
    const ALMetalTexture* diffTexture() const noexcept;
    const ALMetalTexture* filteredTexture() const noexcept;
private:
    struct Impl;
    explicit ALMetalSceneDifference(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
    friend class ALMetalSceneMonitor;
};

class ALMetalSceneMonitor final
{
public:
    ~ALMetalSceneMonitor();
    static std::unique_ptr<ALMetalSceneMonitor> create(
        const ALMetalDevice& device, std::string* error = nullptr);
    // Inputs are exact committed pre-UI window backings. Pin their pool
    // leases through command completion even if the caller resets its ticket.
    std::unique_ptr<ALMetalSceneDifference> submit(
        ALMetalRetainedBackground::Lease previous,
        ALMetalRetainedBackground::Lease current,
        const ALMetalSceneDifferenceConfig& config,
        std::string* error = nullptr);
private:
    struct Impl;
    explicit ALMetalSceneMonitor(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
