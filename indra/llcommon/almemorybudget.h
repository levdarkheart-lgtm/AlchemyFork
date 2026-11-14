/**
 * @file almemorybudget.h
 * @brief Process limits and independently scoped available-memory operands.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace ALMemoryBudget
{
// LLMemory's public counters use U32 KiB. Keep the all-ones value reserved for
// an unavailable sample, including when a setting exceeds their finite range.
inline constexpr std::uint32_t unknown_kb = std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t maximum_known_kb = unknown_kb - 1;
inline constexpr std::uint64_t kib_per_gib = 1024ULL * 1024ULL;

struct ProcessLimit
{
    std::uint32_t limit_kb;
    bool automatic;
    bool invalid_setting;
    bool physical_memory_known;
};

inline ProcessLimit resolveProcessLimitKB(double configured_gib,
                                         std::uint64_t physical_kb) noexcept
{
    const bool valid = std::isfinite(configured_gib) && configured_gib >= 0.0;
    const bool physical_known = physical_kb != 0 && physical_kb < unknown_kb;
    if (valid && configured_gib > 0.0)
    {
        const double maximum_gib = static_cast<double>(maximum_known_kb) / kib_per_gib;
        const auto limit = configured_gib >= maximum_gib
            ? maximum_known_kb
            : static_cast<std::uint32_t>(configured_gib * kib_per_gib);
        return {std::max<std::uint32_t>(limit, 1), false, false, physical_known};
    }

    // Reserve at least 4 GiB or one quarter of installed RAM for the OS and
    // other processes. On small devices retain at least half for the viewer.
    // This is an admission ceiling, not a request to allocate this memory.
    std::uint64_t limit = 16ULL * kib_per_gib;
    if (physical_known)
    {
        const auto reserve = std::max<std::uint64_t>(4ULL * kib_per_gib, physical_kb / 4);
        const auto after_reserve = physical_kb > reserve ? physical_kb - reserve : 0;
        limit = std::max(physical_kb / 2, after_reserve);
    }
    return {static_cast<std::uint32_t>(std::clamp<std::uint64_t>(
                limit, 1, maximum_known_kb)), true, !valid, physical_known};
}

inline std::uint32_t processHeadroomKB(std::uint32_t limit_kb,
                                     std::uint64_t allocated_kb) noexcept
{
    if (limit_kb == unknown_kb) return unknown_kb;
    return allocated_kb < limit_kb
        ? static_cast<std::uint32_t>(limit_kb - allocated_kb) : 0;
}

inline std::uint32_t availableKB(std::uint32_t os_available_kb,
                                 std::uint32_t process_headroom_kb,
                                 std::uint32_t commit_available_kb = unknown_kb) noexcept
{
    // Unknown values sort after every known value. An unavailable OS query
    // cannot hide an exhausted configured process limit (or Windows commit).
    return std::min({os_available_kb, process_headroom_kb, commit_available_kb});
}
}
