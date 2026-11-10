/**
 * @file almetalshadowmaterials.h
 * @brief Native cached material tables for covered shadow objects.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#pragma once

#include "almetalpipeline.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

struct ALMetalShadowMaterialRequest
{
    // Borrowed only during prepare(). Each texture is the exact shader view
    // selected by the ordinary setFragmentTexture path, including swizzles.
    const ALMetalTexture* const* textures = nullptr;
    std::uint32_t texture_count = 0;
    // Must have been created with supportArgumentBuffers=YES; quality unchanged.
    const ALMetalSamplerState* sampler = nullptr;
};

struct ALMetalShadowMaterialLimits
{
    // Includes slots evicted from lookup but still held by pending commands.
    std::size_t maximum_slots = 1024;
    std::size_t maximum_buffer_bytes = 4 * 1024 * 1024;
};

struct ALMetalShadowMaterialStats
{
    std::uint64_t prepared_passes = 0;
    std::uint64_t cache_hits = 0;
    std::uint64_t cache_misses = 0;
    std::uint64_t table_creates = 0;
    // Actual prepare-time native calls/slot inspections, not logical misses.
    // Factory-only ABI layout queries are excluded.
    std::uint64_t argument_encoder_creations = 0;
    std::uint64_t slot_scan_visits = 0;
    std::uint64_t slot_reuses = 0;
    std::uint64_t direct_requests = 0;
    // Unique misses selecting direct because storage is full OR because an
    // entirely one-use new working set exceeds the maximum slot count.
    std::uint64_t capacity_fallbacks = 0;
    std::uint64_t allocation_fallbacks = 0;
    std::uint64_t declared_resources = 0;
    std::uint64_t buffer_bind_calls = 0;
    std::size_t cached_tables = 0;
    std::size_t allocated_slots = 0;
    std::size_t allocated_buffer_bytes = 0;
    std::size_t leased_slots = 0;
    std::size_t command_referenced_tables = 0;
    std::size_t evicted_inflight_slots = 0;
};

// Single-CPU-owner cache. Share through ALMetalWorldFrameData across view
// owners. Nothing in this helper publishes to the frame draw-data arena.
class ALMetalShadowMaterials final
{
public:
    class Pass final
    {
    public:
        ~Pass();
        Pass(const Pass&) = delete;
        Pass& operator=(const Pass&) = delete;
        std::size_t requestCount() const noexcept;
        bool usesArgumentBuffer(std::size_t request) const noexcept;
        // One native encoder per Pass. Exactly once before draws on that
        // encoder. Declares all indirect
        // textures together. It does not replace command resource retention.
        bool declareResources(ALMetalRenderEncoder& encoder,
                              std::string* error = nullptr);
        // Requires the matching eight/32 material PSO and prior declaration.
        // Fragment buffer0 remains the caller's unchanged608B draw constants.
        bool bind(ALMetalRenderEncoder& encoder, std::size_t request,
                  std::string* error = nullptr);
    private:
        struct Impl;
        explicit Pass(std::unique_ptr<Impl> impl);
        std::unique_ptr<Impl> mImpl;
        friend class ALMetalShadowMaterials;
    };

    static std::unique_ptr<ALMetalShadowMaterials> create(
        const ALMetalDevice& device, const ALMetalShaderLibrary& library,
        std::string* error = nullptr,
        ALMetalShadowMaterialLimits limits = {});
    ~ALMetalShadowMaterials();
    ALMetalShadowMaterials(const ALMetalShadowMaterials&) = delete;
    ALMetalShadowMaterials& operator=(const ALMetalShadowMaterials&) = delete;
    bool supported() const noexcept;
    // Same-generation calls are no-ops. Retains tables used in the immediately
    // preceding generation; older cold tables lose cache ownership. Pending
    // commands hold their independent immutable leases until completion.
    void beginFrame(std::uint64_t generation);
    // Call after complete world source validation and before creating encoder.
    // Invalid resources/device/sampler/ABI fail; optional capacity/allocation
    // pressure selects the original direct PSO for the affected request.
    // Count1 always selects the existing single-texture direct PSO initially.
    std::unique_ptr<Pass> prepare(
        ALMetalCommandBuffer& command,
        std::span<const ALMetalShadowMaterialRequest> requests,
        std::string* error = nullptr);
    ALMetalShadowMaterialStats stats() const noexcept;

private:
    struct Impl;
    explicit ALMetalShadowMaterials(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
