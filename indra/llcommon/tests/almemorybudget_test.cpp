/**
 * @file almemorybudget_test.cpp
 * @brief Process/OS policy separation and automatic native-memory ceilings.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"

#include "almemorybudget.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <limits>

namespace
{
constexpr auto gib = ALMemoryBudget::kib_per_gib;
constexpr std::uint32_t mib = 1024;
}

TEST_CASE("Memory budget automatic ceilings reserve OS room across Apple memory sizes",
          "[memory-budget][memory-pressure]")
{
    constexpr std::array<std::array<std::uint32_t, 2>, 6> cases{{
        {4, 2}, {8, 4}, {16, 12}, {32, 24}, {64, 48}, {128, 96}}};
    for (const auto& [physical, expected] : cases)
    {
        CAPTURE(physical, expected);
        const auto policy = ALMemoryBudget::resolveProcessLimitKB(0.0, physical * gib);
        CHECK(policy.limit_kb == expected * gib);
        CHECK(policy.automatic);
        CHECK_FALSE(policy.invalid_setting);
        CHECK(policy.physical_memory_known);
        CHECK(policy.limit_kb < physical * gib);
    }
}

TEST_CASE("Memory budget positive process limits remain intentional user policy",
          "[memory-budget][memory-pressure]")
{
    for (const double configured : {1.0, 12.5, 16.0, 48.0, 96.0})
    {
        CAPTURE(configured);
        const auto policy = ALMemoryBudget::resolveProcessLimitKB(configured, 64 * gib);
        CHECK(policy.limit_kb == static_cast<std::uint32_t>(configured * gib));
        CHECK_FALSE(policy.automatic);
        CHECK_FALSE(policy.invalid_setting);
    }
    // Explicit limits are not silently rewritten to match detected RAM.
    CHECK(ALMemoryBudget::resolveProcessLimitKB(16.0, 8 * gib).limit_kb == 16 * gib);
}

TEST_CASE("Memory budget reproduces the captured 16 GiB false system-pressure operand",
          "[memory-budget][memory-pressure][PPD-111]")
{
    constexpr std::array<std::array<double, 2>, 5> captured{{
        {15934.45, 449.55}, {16890.92, 0.0}, {14902.05, 1481.95},
        {15885.61, 498.39}, {15575.52, 808.48}}};
    const auto explicit_limit = ALMemoryBudget::resolveProcessLimitKB(16.0, 64 * gib);
    const auto automatic = ALMemoryBudget::resolveProcessLimitKB(0.0, 64 * gib);
    constexpr auto os_available = static_cast<std::uint32_t>(30 * gib);
    for (const auto& [allocated_mib, old_available_mib] : captured)
    {
        CAPTURE(allocated_mib, old_available_mib);
        const auto allocated = static_cast<std::uint64_t>(allocated_mib * mib);
        const auto explicit_headroom = ALMemoryBudget::processHeadroomKB(
            explicit_limit.limit_kb, allocated);
        CHECK(explicit_headroom / 1024.0 ==
              Catch::Approx(old_available_mib).margin(1.0 / mib));
        // The OS operand remains independently available and unchanged.
        CHECK(os_available == 30 * gib);
        const auto auto_headroom = ALMemoryBudget::processHeadroomKB(
            automatic.limit_kb, allocated);
        CHECK(ALMemoryBudget::availableKB(os_available, auto_headroom) == os_available);
        CHECK(auto_headroom > 512 * mib);
        // An intentionally configured 16 GiB limit must still protect itself.
        CHECK(ALMemoryBudget::availableKB(os_available, explicit_headroom) == explicit_headroom);
    }
}

TEST_CASE("Memory budget real OS pressure and explicit process exhaustion remain effective",
          "[memory-budget][memory-pressure]")
{
    const auto automatic = ALMemoryBudget::resolveProcessLimitKB(0.0, 64 * gib);
    const auto headroom = ALMemoryBudget::processHeadroomKB(automatic.limit_kb, 17 * gib);
    CHECK(ALMemoryBudget::availableKB(128 * mib, headroom) == 128 * mib);
    CHECK(ALMemoryBudget::availableKB(0, headroom) == 0);
    CHECK(ALMemoryBudget::availableKB(static_cast<std::uint32_t>(30 * gib), 0) == 0);
    CHECK(ALMemoryBudget::availableKB(128 * mib, headroom, 64 * mib) == 64 * mib);
}

TEST_CASE("Memory budget unknown samples never hide a known exhausted scope",
          "[memory-budget][memory-pressure]")
{
    using namespace ALMemoryBudget;
    CHECK(availableKB(unknown_kb, 0) == 0);
    CHECK(availableKB(128 * mib, unknown_kb) == 128 * mib);
    CHECK(availableKB(unknown_kb, unknown_kb, 64 * mib) == 64 * mib);
    CHECK(availableKB(unknown_kb, unknown_kb) == unknown_kb);
    CHECK(processHeadroomKB(unknown_kb, 17 * gib) == unknown_kb);
}

TEST_CASE("Memory budget unknown RAM and invalid settings use a finite conservative fallback",
          "[memory-budget][memory-pressure]")
{
    using namespace ALMemoryBudget;
    for (const auto physical : {std::uint64_t(0), std::uint64_t(unknown_kb),
                               std::numeric_limits<std::uint64_t>::max()})
    {
        const auto policy = resolveProcessLimitKB(0.0, physical);
        CHECK(policy.limit_kb == 16 * gib);
        CHECK(policy.automatic);
        CHECK_FALSE(policy.physical_memory_known);
    }
    for (const double invalid : {-1.0, std::numeric_limits<double>::quiet_NaN(),
                                 std::numeric_limits<double>::infinity(),
                                 -std::numeric_limits<double>::infinity()})
    {
        const auto policy = resolveProcessLimitKB(invalid, 64 * gib);
        CHECK(policy.limit_kb == 48 * gib);
        CHECK(policy.automatic);
        CHECK(policy.invalid_setting);
    }
}

TEST_CASE("Memory budget saturates arithmetic without reusing the unknown sentinel",
          "[memory-budget][memory-pressure]")
{
    using namespace ALMemoryBudget;
    CHECK(processHeadroomKB(16 * gib, 16 * gib) == 0);
    CHECK(processHeadroomKB(16 * gib, std::numeric_limits<std::uint64_t>::max()) == 0);
    const auto enormous = resolveProcessLimitKB(std::numeric_limits<double>::max(), 64 * gib);
    CHECK(enormous.limit_kb == maximum_known_kb);
    CHECK(enormous.limit_kb != unknown_kb);
    CHECK_FALSE(enormous.automatic);
    CHECK(processHeadroomKB(enormous.limit_kb, 0) == maximum_known_kb);
    CHECK(resolveProcessLimitKB(1e-12, 64 * gib).limit_kb == 1);
}
