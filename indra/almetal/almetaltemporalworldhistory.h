/**
 * @file almetaltemporalworldhistory.h
 * @brief Submission-owned previous main-world transforms and deformation poses.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include "almetalworld.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Shift a copied column-major world projection/MVP in input-pixel units.
// Logical camera, picking and UI matrices remain untouched. Invalid extents or
// non-finite offsets are a no-op; production preflight validates them earlier.
void alMetalApplyTemporalJitter(float matrix[16], float x, float y,
                                std::uint32_t width, std::uint32_t height) noexcept;

// Water/haze reconstruct sampled scene depth as well as rasterizing a field.
// Jitter the copied field MVP and its depth inverse together. The inverse is
// for projection alone, not MVP; J acts on clip coordinates in both cases.
// Logical water state and independent views remain unchanged.
void alMetalApplyTemporalWaterJitter(ALMetalWorldWaterHaze& field, float x, float y,
                                    std::uint32_t width, std::uint32_t height) noexcept;

// Expand a copied culling projection by one input pixel on every edge, covering
// all subpixel jitter phases. This may also be used while a requested Temporal
// view preflights fallback; admitting the small guard band changes no raster or
// logical-camera state.
void alMetalExpandTemporalCullProjection(float matrix[16], std::uint32_t width,
                                         std::uint32_t height) noexcept;

// One serialized render owner. Only actual Temporal main-world submissions call
// begin/stage/commit. UI redraws, repeated presentation, probes and independent
// captures do not participate. No Metal commands, waits or simulation pointers
// are retained by this CPU owner.
class ALMetalTemporalWorldHistory final
{
public:
    struct FrameInput
    {
        std::uint64_t world_id = 0;
        std::uint32_t input_width = 0, input_height = 0;
        std::uint32_t output_width = 0, output_height = 0;
        std::uint32_t mode = 0;
        // Zero keeps the existing MetalFX recommendation. Other main-view
        // algorithms may provide their documented phase count, owned by this
        // same submission transaction rather than a second jitter sequence.
        std::uint32_t jitter_phase_count = 0;
        // Column-major, unjittered viewer clip matrix. Native rasterization
        // negates clip Y; storage UV is original clip.xy / w * .5 + .5.
        float camera_mvp[16]{};
        std::array<double, 3> origin{};
        bool reverse_z = true;
        bool force_reset = false;
    };
    struct Limits
    {
        std::size_t draws = 65536;
        // Bounds owned pose arrays/maps and deep palette bytes in each frame.
        // At most one committed and one pending frame are retained.
        std::size_t bytes_per_frame = 64u * 1024u * 1024u;
    };
    struct Stats
    {
        std::uint64_t submitted_worlds = 0, abandoned_worlds = 0, resets = 0;
        std::size_t committed_draws = 0, pending_draws = 0;
        std::size_t committed_bytes = 0, pending_bytes = 0;
        std::size_t valid_previous_draws = 0, rejected_previous_draws = 0;
        std::uint32_t jitter_phase = 0, jitter_phase_count = 0;
    };

    ALMetalTemporalWorldHistory();
    explicit ALMetalTemporalWorldHistory(Limits);
    ~ALMetalTemporalWorldHistory();
    ALMetalTemporalWorldHistory(const ALMetalTemporalWorldHistory&) = delete;
    ALMetalTemporalWorldHistory& operator=(const ALMetalTemporalWorldHistory&) = delete;

    // Merely preparing or abandoning a world does not advance authoritative
    // previous state or the Halton phase. Descriptor/origin/cut resets are
    // pending decisions until successful submission.
    bool beginFrame(const FrameInput&, std::string* error = nullptr);
    bool stage(const ALMetalWorldGBufferScene&,
               const ALMetalWorldOpaquePostDeferredScene*,
               std::string* error = nullptr);
    bool stage(const ALMetalWorldGBufferScene& opaque,
               const ALMetalWorldOpaquePostDeferredScene& forward,
               std::string* error = nullptr)
    { return stage(opaque, &forward, error); }
    // Call immediately after the outer required-world command commits, without
    // waiting for its completion notification. Existing ordered queue semantics
    // protect GPU history; errors call invalidate() on this serialized owner.
    bool commit(std::uint64_t world_id, std::string* error = nullptr) noexcept;
    void abandon(std::uint64_t world_id = 0) noexcept;
    void invalidate() noexcept;

    bool pending() const noexcept;
    bool resetRequired() const noexcept;
    std::uint64_t pendingWorld() const noexcept;
    std::uint64_t committedWorld() const noexcept;
    // Apple's Temporal starting policy: ceil(8 * max(output/input)^2),
    // calculated from the actual rounded extents and bounded to 128 phases.
    static std::uint32_t recommendedJitterPhaseCount(std::uint32_t input_width,
        std::uint32_t input_height, std::uint32_t output_width, std::uint32_t output_height) noexcept;
    std::uint32_t jitterPhaseCount() const noexcept;
    std::array<float, 2> jitterPixels() const noexcept;
    const std::array<float, 16>& clipToPrevious() const noexcept;
    const std::vector<ALMetalWorldTemporalPreviousDraw>& previousOpaque() const noexcept;
    const std::vector<ALMetalWorldTemporalPreviousDraw>& previousForward() const noexcept;
    Stats stats() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
