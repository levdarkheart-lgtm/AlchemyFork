/**
 * @file almetalprobecandidates.h
 * @brief Conservative per-view bounds over an exact published probe pack.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

struct ALMetalProbeCandidateSphere
{
    // Eye-space center/radius and the corresponding ref_index[index].w.
    float center_radius[4]{};
    std::int32_t priority = 0;
};

struct ALMetalProbeCandidateView
{
    // Original column-major eye->clip projection, BEFORE the native vertex Y
    // inversion. Stored world texture row Y = (clipY / clipW + 1) * height/2.
    float projection[16]{};
    std::uint32_t width = 0, height = 0;
    // The owner must certify this projection/viewport against the actual
    // consumer's reconstructed/interpolated eye position and raster position.
    // Separate alpha MVP/MV or ill-conditioned inverse matrices are NOT
    // certified merely because their nominal camera projection is identical.
    bool raster_mapping_certified = false;
    // Outward pixel error envelope, including caller reconstruction/raster
    // error. At least two pixels; unavailable/unbounded error must not certify.
    double pixel_guard = 2.0;
};

struct ALMetalProbeCandidateRect
{
    std::uint32_t x_min = 0, y_min = 0;
    std::uint32_t x_max = 0, y_max = 0; // exclusive integer pixel boundaries
};

// Direct GPU packet: no pointers, native owners, dynamic allocation or reordered
// indices. Consumers may reject only outside bounds[index], and must retain
// original bucket/neighbor/order/default/priority/cap selection semantics.
struct alignas(16) ALMetalProbeCandidatePacket
{
    static constexpr std::size_t capacity = 256;
    std::array<ALMetalProbeCandidateRect, capacity> bounds{};
    // effective count (at least default0), width, height, usable (0/1).
    // usable0 requires the unchanged generic shader path, never an empty list.
    std::uint32_t config[4]{};
};
static_assert(sizeof(ALMetalProbeCandidateRect) == 16);
static_assert(offsetof(ALMetalProbeCandidatePacket, config) == 4096);
static_assert(sizeof(ALMetalProbeCandidatePacket) == 4112);
static_assert(std::is_trivially_copyable_v<ALMetalProbeCandidatePacket>);
static_assert(std::is_standard_layout_v<ALMetalProbeCandidatePacket>);

// Copy sphere/priority values from the SAME immutable pack bound to the view.
// Count must match config.x exactly and be finite/integral/in[0,256]. Even count0
// needs source slot0, which is always full-view. Only automatic spheres (w0)
// receive bounded rectangles; every manual/box/uncertain case is full-view.
// Publication identity/revision, view identity and completion lifetime remain
// the caller's ownership contract; this pure math helper does not certify them.
ALMetalProbeCandidatePacket alMetalBuildProbeCandidates(
    std::span<const ALMetalProbeCandidateSphere> probes,
    float published_count, const ALMetalProbeCandidateView& view) noexcept;

struct ALMetalProbeBaseMappingCertificate
{
    bool certified = false;
    double pixel_guard = 0.0;
    double ndc_error[2]{};
    // Lower bound after multiplying the reconstructed eye position by its
    // common positive divisor; NOT physical clip W or an inverse-W bound.
    double minimum_projected_denominator = 0.0;
};

// Only the fullscreen deferred BASE reconstruction is eligible: exact bound
// inverse_projection, unit-W local-light vertex UVs, depth in[0,1], and the
// original pre-Y-flip projection. Requires P[X/Y/W][3] exactly zero, so the
// shader's common max(inverseW,1e-6) divisor cancels in projected XY ratios.
// Supports finite/infinite-far perspective and reverse depth through the
// explicit clip domain. Orthographic/affine, mismatched, nonfinite, overflowing
// or ill-conditioned inputs return uncertified. This is NOT an alpha/MVP or
// arbitrary interpolated-eye-position certificate.
//
// Returns an outward guard of 2..16 pixels, including a two-pixel budget for
// the known fullscreen UV/raster mapping plus a derived f32 numerical bound.
// Actual consumer/shader correctness remains a separate integration gate.
ALMetalProbeBaseMappingCertificate alMetalCertifyBaseProbeMapping(
    std::span<const float, 16> projection,
    std::span<const float, 16> inverse_projection,
    bool reverse_z, std::uint32_t width, std::uint32_t height) noexcept;
