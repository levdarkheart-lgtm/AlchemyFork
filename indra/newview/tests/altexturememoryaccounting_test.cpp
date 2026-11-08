/**
 * @file altexturememoryaccounting_test.cpp
 * @brief Unit tests for native texture/global resource pressure separation.
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

#include "linden_common.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../altexturememoryaccounting.h"

using Catch::Approx;

TEST_CASE("ALTextureMemoryAccounting preserves the original GL VRAM estimate",
          "[texture][pressure]")
{
    const auto usage = ALTextureMemoryAccounting::classify(
        false, 2'560.0, 198.0 * 2.0, 4'860.0, 59'392.0);

    CHECK(usage.texture_pressure_mb == Approx(2'956.0));
    CHECK(usage.global_pressure_mb == 0.0);
    CHECK(usage.global_budget_mb == 0.0);
}

TEST_CASE("ALTextureMemoryAccounting keeps Metal device resources out of the texture allowance",
          "[texture][pressure]")
{
    const auto usage = ALTextureMemoryAccounting::classify(
        true, 2'996.0, 198.0 * 2.0, 4'860.0, 59'392.0);

    CHECK(usage.texture_pressure_mb == Approx(2'996.0));
    CHECK(usage.global_pressure_mb == Approx(4'860.0));
    CHECK(usage.global_budget_mb == Approx(59'392.0));
    CHECK(ALTextureMemoryAccounting::overageFraction(
              usage.global_pressure_mb, usage.global_budget_mb) < -0.9);
}

TEST_CASE("ALTextureMemoryAccounting reports independent texture and global overages",
          "[texture][pressure]")
{
    CHECK(ALTextureMemoryAccounting::overageFraction(3'000.0, 2'969.6) ==
          Approx(0.0102370689655));
    CHECK(ALTextureMemoryAccounting::overageFraction(60'000.0, 59'392.0) ==
          Approx(0.0102370689655));
    CHECK(ALTextureMemoryAccounting::overageFraction(1.0, 0.0) == 0.0);
}
