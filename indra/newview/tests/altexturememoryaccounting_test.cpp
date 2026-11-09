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
#include <limits>

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

TEST_CASE("ALTextureMemoryAccounting does not charge device-only allocations to the image target",
          "[texture][pressure]")
{
    // Device allocations exceed the small image target, but neither their
    // own recommendation nor the actual image allowance is exhausted.
    const auto pressure = ALTextureMemoryAccounting::evaluate(
        2'000.f, 2'969.6f, 5'000.0, 59'392.0);
    CHECK(pressure.over_pct < -0.1f);
    CHECK(pressure.free_mb == Approx(969.6f).margin(0.001f));
}

TEST_CASE("ALTextureMemoryAccounting keeps genuine image overage effective",
          "[texture][pressure]")
{
    const auto pressure = ALTextureMemoryAccounting::evaluate(
        3'000.f, 2'969.6f, 5'000.0, 59'392.0);
    CHECK(pressure.over_pct > 0.01f);
    CHECK(pressure.over_pct < 0.02f);
    CHECK(pressure.free_mb == Approx(-30.4f).margin(0.001f));
}

TEST_CASE("ALTextureMemoryAccounting keeps genuine global overage effective",
          "[texture][pressure]")
{
    const auto pressure = ALTextureMemoryAccounting::evaluate(
        1'000.f, 2'969.6f, 60'000.0, 59'392.0);
    CHECK(pressure.over_pct == Approx(0.0102370689655f));
    CHECK(pressure.free_mb == -608.f);
}

TEST_CASE("ALTextureMemoryAccounting requires both scopes to clear strict recovery hysteresis",
          "[texture][pressure]")
{
    // Existing controller recovery is STRICTLY over_pct < -0.1f. The
    // comfortable scope must never hide either near-full or boundary scope.
    CHECK_FALSE(ALTextureMemoryAccounting::evaluate(
        100.f, 1'000.f, 901.0, 1'000.0).over_pct < -0.1f);
    CHECK_FALSE(ALTextureMemoryAccounting::evaluate(
        901.f, 1'000.f, 100.0, 1'000.0).over_pct < -0.1f);
    CHECK_FALSE(ALTextureMemoryAccounting::evaluate(
        100.f, 1'000.f, 900.0, 1'000.0).over_pct < -0.1f);
    CHECK_FALSE(ALTextureMemoryAccounting::evaluate(
        900.f, 1'000.f, 100.0, 1'000.0).over_pct < -0.1f);
    CHECK(ALTextureMemoryAccounting::evaluate(
        899.f, 1'000.f, 899.0, 1'000.0).over_pct < -0.1f);
}

TEST_CASE("ALTextureMemoryAccounting reports the limiting absolute free headroom",
          "[texture][pressure]")
{
    CHECK(ALTextureMemoryAccounting::evaluate(
        500.f, 1'000.f, 950.0, 1'000.0).free_mb == 50.f);
    const auto image_limited = ALTextureMemoryAccounting::evaluate(
        1'100.f, 1'000.f, 1'050.0, 1'000.0);
    CHECK(image_limited.free_mb == -100.f);
    CHECK(image_limited.over_pct == Approx(0.1f));
    const auto device_limited = ALTextureMemoryAccounting::evaluate(
        100.f, 1'000.f, 1'200.0, 1'000.0);
    CHECK(device_limited.free_mb == -200.f);
    CHECK(device_limited.over_pct == Approx(0.2f));
}

TEST_CASE("ALTextureMemoryAccounting ignores an unknown global recommendation",
          "[texture][pressure]")
{
    const auto comfortable = ALTextureMemoryAccounting::evaluate(
        1'000.f, 2'969.6f, 90'000.0, 0.0);
    CHECK(comfortable.over_pct < -0.1f);
    CHECK(comfortable.free_mb == Approx(1'969.6f).margin(0.001f));
    const auto image_over = ALTextureMemoryAccounting::evaluate(
        3'000.f, 2'969.6f, 90'000.0, 0.0);
    CHECK(image_over.over_pct > 0.f);
    CHECK(image_over.free_mb < 0.f);
}

TEST_CASE("ALTextureMemoryAccounting preserves exact legacy F32 controller arithmetic",
          "[texture][pressure]")
{
    // Compatibility oracle is the existing GL controller after ll_round and
    // F32 conversion. Do not silently replace its subtract/divide with F64.
    for (const float used : {0.f, 1.f, 768.f, 2'048.f, 3'000.f, 100'000'000.f})
    for (const float target : {768.f, 1'638.4f, 2'969.6f, 100'000.f})
    {
        CAPTURE(used, target);
        const float legacy_over = (used - target) / target;
        const float legacy_free = target - used;
        const auto pressure = ALTextureMemoryAccounting::evaluate(
            used, target, 90'000.0, 0.0);
        CHECK(pressure.over_pct == legacy_over);
        CHECK(pressure.free_mb == legacy_free);
    }
}

TEST_CASE("ALTextureMemoryAccounting reserves non-image storage at diagnostic magnitudes without rounding images",
          "[texture][pressure][capacity]")
{
    // Representative observed magnitudes, with fractional synthetic inputs
    // to catch premature rounding. This is not a new runtime observation.
    const auto capacity = ALTextureMemoryAccounting::automaticCapacity(
        59'392.0, 4'876.125, 3'053.375);
    CHECK(capacity.non_image_estimate_mb == 1'822.75);
    CHECK(capacity.automatic_capacity_mb == 57'569.25);
    CHECK(capacity.automatic_capacity_mb < 59'392.0);
}

TEST_CASE("ALTextureMemoryAccounting clamps image estimates above device allocation",
          "[texture][pressure][capacity]")
{
    const auto capacity = ALTextureMemoryAccounting::automaticCapacity(
        10'000.0, 1'000.0, 2'000.0);
    CHECK(capacity.non_image_estimate_mb == 0.0);
    CHECK(capacity.automatic_capacity_mb == 10'000.0);
}

TEST_CASE("ALTextureMemoryAccounting admits no automatic image capacity when non-images exhaust the recommendation",
          "[texture][pressure][capacity]")
{
    for (double device : {12'000.0, 13'000.0})
    {
        CAPTURE(device);
        const auto capacity = ALTextureMemoryAccounting::automaticCapacity(
            10'000.0, device, 2'000.0);
        CHECK(capacity.non_image_estimate_mb >= 10'000.0);
        CHECK(capacity.automatic_capacity_mb == 0.0);
    }
}

TEST_CASE("ALTextureMemoryAccounting rejects unknown or invalid automatic-capacity inputs",
          "[texture][pressure][capacity]")
{
    CHECK(ALTextureMemoryAccounting::automaticCapacity(
        0.0, 5'000.0, 3'000.0).automatic_capacity_mb == 0.0);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (double invalid : {nan, inf, -inf, -1.0})
    for (unsigned field : {0u, 1u, 2u})
    {
        CAPTURE(invalid, field);
        double inputs[3] = {10'000.0, 5'000.0, 3'000.0};
        inputs[field] = invalid;
        const auto capacity = ALTextureMemoryAccounting::automaticCapacity(
            inputs[0], inputs[1], inputs[2]);
        CHECK(capacity.non_image_estimate_mb == 0.0);
        CHECK(capacity.automatic_capacity_mb == 0.0);
    }
}

TEST_CASE("ALTextureMemoryAccounting does not grow capacity as image and total allocations grow equally",
          "[texture][pressure][capacity]")
{
    for (double image : {0.0, 1'000.0, 3'000.0, 10'000.0})
    {
        CAPTURE(image);
        const auto capacity = ALTextureMemoryAccounting::automaticCapacity(
            59'392.0, image + 1'800.0, image);
        CHECK(capacity.non_image_estimate_mb == 1'800.0);
        CHECK(capacity.automatic_capacity_mb == 57'592.0);
    }
}

TEST_CASE("ALTextureMemoryAccounting pressure rises monotonically while global allocation remains below recommendation",
          "[texture][pressure][capacity]")
{
    float previous = -std::numeric_limits<float>::infinity();
    for (float image : {1'000.f, 3'000.f, 6'000.f, 7'000.f})
    {
        CAPTURE(image);
        const double device = static_cast<double>(image) + 2'000.0;
        const auto capacity = ALTextureMemoryAccounting::automaticCapacity(
            10'000.0, device, image);
        REQUIRE(device < 10'000.0);
        REQUIRE(capacity.automatic_capacity_mb == 8'000.0);
        // Existing divisor1/headroom applied ONCE to capacity8000 gives
        // target6400. The helper must not itself apply that policy again.
        const auto pressure = ALTextureMemoryAccounting::evaluate(
            image, 6'400.f, device, 10'000.0);
        CHECK(pressure.over_pct > previous);
        previous = pressure.over_pct;
    }
    CHECK(previous > 0.f); // preserved headroom can trigger before global R
}

TEST_CASE("ALTextureMemoryAccounting global guard blocks recovery at and above the recommendation",
          "[texture][pressure][capacity]")
{
    for (double device : {10'000.0, 10'001.0})
    {
        CAPTURE(device);
        // A comfortable manual image target cannot override genuine global
        // pressure. Equal R blocks strict recovery; above R is overcommit.
        const auto pressure = ALTextureMemoryAccounting::evaluate(
            1'000.f, 8'000.f, device, 10'000.0);
        CHECK_FALSE(pressure.over_pct < -0.1f);
        CHECK(pressure.over_pct >= 0.f);
        CHECK(pressure.free_mb <= 0.f);
        if (device > 10'000.0) CHECK(pressure.over_pct > 0.f);
    }
}

TEST_CASE("ALTextureMemoryAccounting cannot create recovery by increasing logical image accounting alone",
          "[texture][pressure][capacity]")
{
    float previous = -std::numeric_limits<float>::infinity();
    for (float image : {1'000.f, 3'000.f, 5'000.f, 7'000.f, 9'000.f})
    {
        CAPTURE(image);
        // With fixed physical allocation, a revised logical estimate raises
        // the capacity too. Its normalized pressure must still never fall.
        const auto capacity = ALTextureMemoryAccounting::automaticCapacity(
            10'000.0, 8'000.0, image);
        const float target = static_cast<float>(capacity.automatic_capacity_mb) * 0.8f;
        const auto pressure = ALTextureMemoryAccounting::evaluate(
            image, target, 8'000.0, 10'000.0);
        CHECK(pressure.over_pct >= previous);
        previous = pressure.over_pct;
    }
    CHECK(previous > 0.f);
}
