/**
 * @file almetalshadowindirect.h
 * @brief Completion-retired native indirect commands for static opaque shadows.
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

struct ALMetalShadowIndirectDraw
{
    const ALMetalBuffer* positions = nullptr;
    const ALMetalBuffer* indices = nullptr;
    const ALMetalBuffer* constants = nullptr;
    std::size_t position_offset = 0;
    std::size_t index_offset = 0;
    std::size_t constant_offset = 0;
    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
};

struct ALMetalShadowIndirectStats
{
    std::uint64_t slot_allocations = 0;
    std::uint64_t slot_reuses = 0;
    std::uint64_t prepared_draws = 0;
    std::uint64_t executions = 0;
    std::uint64_t executed_draws = 0;
    std::uint64_t resource_declarations = 0;
    std::size_t slot_count = 0;
    std::uint64_t direct_prepared_draws = 0;
    std::uint64_t direct_executions = 0;
    std::uint64_t direct_executed_draws = 0;
    std::uint64_t direct_slot_allocations = 0;
    std::uint64_t direct_slot_reuses = 0;
    std::uint64_t direct_coalesced_draws = 0;
    // Static non-avatar covered input commands and adjacent commands removed.
    // Separate from the opaque direct-packet counter above.
    std::uint64_t covered_input_draws = 0;
    std::uint64_t covered_coalesced_draws = 0;
    std::uint64_t source_records = 0;
    std::uint64_t encoded_records = 0;
    std::uint64_t early_coalesced_records = 0;
    std::size_t direct_slot_count = 0;
};

// Single-CPU-owner helper. It changes neither draw admission nor render state.
// Callers execute ordered ranges at the original scalar-family boundaries.
class ALMetalShadowIndirectCommands final
{
public:
    static constexpr std::size_t maximum_draw_count = 16384;
    static constexpr std::size_t constant_size = 80;

    class Batch final
    {
    public:
        ~Batch();
        Batch(const Batch&) = delete;
        Batch& operator=(const Batch&) = delete;
        std::size_t drawCount() const noexcept;
        bool isIndirect() const noexcept;
    private:
        struct Impl;
        explicit Batch(std::unique_ptr<Impl> impl);
        std::unique_ptr<Impl> mImpl;
        friend class ALMetalShadowIndirectCommands;
    };

    static std::unique_ptr<ALMetalShadowIndirectCommands> create(
        const ALMetalDevice& device, std::string* error = nullptr);
    ~ALMetalShadowIndirectCommands();
    ALMetalShadowIndirectCommands(const ALMetalShadowIndirectCommands&) = delete;
    ALMetalShadowIndirectCommands& operator=(const ALMetalShadowIndirectCommands&) = delete;

    bool supported() const noexcept;
    std::unique_ptr<Batch> prepare(
        ALMetalCommandBuffer& command,
        std::span<const ALMetalShadowIndirectDraw> draws,
        std::string* error = nullptr);
    bool execute(ALMetalRenderEncoder& encoder, const Batch& batch,
                 std::size_t first, std::size_t count,
                 std::string* error = nullptr);
    // Ordinary native draws keep the same immutable packet/slab contract
    // without requiring ICB-compatible pipeline state or indirect resources.
    std::unique_ptr<Batch> prepareDirect(
        ALMetalCommandBuffer& command,
        std::span<const ALMetalShadowIndirectDraw> draws,
        std::string* error = nullptr);
    bool executeDirect(ALMetalRenderEncoder& encoder, const Batch& batch,
                       std::size_t first, std::size_t count,
                       std::string* error = nullptr);
    ALMetalShadowIndirectStats stats() const noexcept;

private:
    struct Impl;
    explicit ALMetalShadowIndirectCommands(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
