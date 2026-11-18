/**
 * @file almetalshadowvisibility.h
 * @brief Ordered Apple GPU shadow visibility and completion-owned compaction.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#pragma once
#include "almetalpipeline.h"
#include <span>

struct alignas(16) ALMetalShadowVisibilityInput
{
    float matrix[16]{};
    float minimum[4]{};
    float maximum[4]{};
    std::uint32_t config[4]{};
};
struct ALMetalShadowVisibilityRun { std::uint32_t first, count; };
struct alignas(16) ALMetalShadowVisibilityDraw
{
    std::uint64_t vertex_buffers[7]{};
    std::uint64_t fragment_buffers[2]{};
    std::uint64_t indices = 0;
    std::uint32_t index_count = 0;
    std::uint32_t index32 = 0;
};
static_assert(sizeof(ALMetalShadowVisibilityDraw) == 96);

class ALMetalShadowVisibility final
{
public:
    class Batch final
    {
    public:
        ~Batch();
        bool computeComplete() const noexcept;
        bool usedMetal4() const noexcept;
        void* executionRanges() const noexcept;
        const ALMetalResourcePlanning::View& executionRangesResourceView() const noexcept;
    private:
        struct Impl;
        std::shared_ptr<Impl> mImpl;
        explicit Batch(std::shared_ptr<Impl>);
        friend class ALMetalShadowVisibility;
    };
    static std::unique_ptr<ALMetalShadowVisibility> create(const ALMetalDevice&, std::string* error=nullptr);
    ~ALMetalShadowVisibility();
    bool metal4Supported() const noexcept;
    std::uint64_t failures() const noexcept;
    // Actual independent Metal4 queue intervals from commit feedback. These
    // exclude CPU encoding and the legacy render queue; neither polling nor
    // these counters adds GPU timestamp/counter commands to the workload.
    std::uint64_t metal4TimedDispatches() const noexcept;
    std::uint64_t metal4GPUTimeNanoseconds() const noexcept;
    std::shared_ptr<Batch> encode(ALMetalCommandBuffer&, void* destination_icb,
        std::span<const ALMetalShadowVisibilityInput>, std::span<const ALMetalShadowVisibilityRun>,
        std::span<void* const> native_buffers,
        std::span<const ALMetalShadowVisibilityDraw> draws,
        bool request_metal4, std::string* error=nullptr,
        std::span<const ALMetalResourcePlanning::View> captured_buffers = {},
        const ALMetalResourcePlanning::View& destination_backing = {});
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
    explicit ALMetalShadowVisibility(std::unique_ptr<Impl>);
};
