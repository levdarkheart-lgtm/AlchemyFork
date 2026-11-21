/**
 * @file almetalworldvisibility.h
 * @brief Conservative CPU classification for published native source bounds.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include <cstddef>
#include <cstdint>

struct ALMetalWorldConservativeBounds
{
    float minimum[4]{}, maximum[4]{};
    // Producer proof for this exact immutable geometry generation. Animated,
    // cloth, rigged or otherwise unproved extents must leave this false.
    bool qualified = false;
};
enum class ALMetalWorldCpuVisibilityResult : std::uint8_t
{
    unknown, outside, partial, inside
};
struct ALMetalWorldCpuVisibilityStats
{
    // Accepted G-buffer encoding only, not final GPU completion. When selected,
    // visible_draws + culled_draws equals the original logical draw count.
    std::uint64_t views=0,groups=0,coarse_tests=0;
    std::uint64_t coarse_inside=0,coarse_outside=0,coarse_partial=0,coarse_unknown=0;
    std::uint64_t fine_tests=0,visible_draws=0,culled_draws=0;
    // unknown_draws is a subset of visible COHORT rows; raw/ref rows are always
    // retained and accounted separately, without inspecting their geometry.
    std::uint64_t unknown_draws=0,raw_visible_draws=0;
};
ALMetalWorldCpuVisibilityResult alMetalClassifyWorldBoundsCPU(
    const ALMetalWorldConservativeBounds&, const float* modelview_projection) noexcept;
