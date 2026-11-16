/**
 * @file almetal_quiet_capture_policy_test.cpp
 * @brief Quiet capture timing, interruption and fixed-budget policy.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalquietcapturepolicy.h"
#include <catch2/catch_test_macros.hpp>

namespace
{
    using Policy = ALMetalQuietCapturePolicy;
    using Action = Policy::Action;
    constexpr auto second = Policy::second;
    struct Clock
    {
        Policy policy;
        Policy::Input input{second, 1, true, true, true, false, false};
        Action tick(std::uint64_t period = second / 20)
        {
            input.now_ns += period;
            ++input.frame;
            return policy.update(input);
        }
        void early()
        {
            REQUIRE(policy.update(input) == Action::none);
            for (unsigned i = 0; i < 39; ++i) REQUIRE(tick() == Action::none);
            REQUIRE(tick() == Action::early);
            REQUIRE(policy.stats().attempts == 1);
        }
    };
}

TEST_CASE("almetal: quiet captures compare uncaptured cadence then consume only two attempts",
          "[almetal][quiet-capture-policy]")
{
    Clock clock;
    clock.early();
    CHECK(clock.policy.stats().baseline_periods == 40);
    CHECK(clock.policy.stats().baseline_mean_period_ns == second / 20);
    clock.input.previous_interval_captured = true;
    CHECK(clock.tick(2 * second) == Action::none); // Exclude capture instrumentation.
    clock.input.previous_interval_captured = false;
    Action action = Action::none;
    for (unsigned i = 0; i < 700 && action == Action::none; ++i)
        action = clock.tick(second / 10);
    CHECK(action == Action::late_decline);
    CHECK(clock.input.now_ns - clock.policy.stats().world_start_ns >= 60 * second);
    CHECK(clock.policy.stats().previous_window_mean_period_ns == second / 10);
    CHECK(clock.policy.stats().last_window_mean_period_ns == second / 10);
    CHECK(clock.policy.stats().attempts == 2);
    for (unsigned i = 0; i < 100; ++i) CHECK(clock.tick() == Action::none);
    CHECK(clock.policy.stats().attempts == 2);
    // Even after the second attempt, cancel an interval if the owner teleports.
    clock.input.world_invalidated = true;
    CHECK(clock.tick() == Action::invalidated);
    CHECK(clock.tick() == Action::none);
}

TEST_CASE("almetal: quiet capture has an explicit late fallback without enough baseline samples",
          "[almetal][quiet-capture-policy]")
{
    Clock clock;
    REQUIRE(clock.policy.update(clock.input) == Action::none);
    CHECK(clock.tick(3 * second) == Action::none);
    CHECK(clock.tick(second) == Action::none);
    CHECK(clock.tick(second) == Action::early); // Four distinct entries, age five seconds.
    CHECK(clock.policy.stats().baseline_periods == 0);
    CHECK(clock.policy.stats().baseline_mean_period_ns == 0);
    for (unsigned age = 6; age < 90; ++age) CHECK(clock.tick(second) == Action::none);
    CHECK(clock.tick(second) == Action::late_fallback);
    CHECK(clock.policy.stats().attempts == 2);
    CHECK(clock.policy.stats().last_action == Action::late_fallback);
}

TEST_CASE("almetal: quiet decline requires two adjacent slow windows with stable mean period",
          "[almetal][quiet-capture-policy]")
{
    Clock clock;
    Policy::Config config;
    config.late_min_age_ns = 20 * second;
    clock.policy = Policy(config);
    clock.early();
    for (unsigned i = 0; i < 100; ++i) CHECK(clock.tick(second / 10) == Action::none);
    for (unsigned i = 0; i < 50; ++i) CHECK(clock.tick(second / 5) == Action::none);
    CHECK(clock.policy.stats().attempts == 1); // Still declining: adjacent means differ by 100%.
    CHECK(clock.policy.stats().previous_window_mean_period_ns == second / 10);
    CHECK(clock.policy.stats().last_window_mean_period_ns == second / 5);
    for (unsigned i = 0; i < 49; ++i) CHECK(clock.tick(second / 5) == Action::none);
    CHECK(clock.tick(second / 5) == Action::late_decline);
}

TEST_CASE("almetal: nested duplicate and suspended displays do not manufacture quiet cadence",
          "[almetal][quiet-capture-policy]")
{
    Clock clock;
    clock.early();
    const auto entries = clock.policy.stats().normal_intervals;
    clock.input.now_ns += second;
    CHECK(clock.policy.update(clock.input) == Action::none); // Duplicate frame.
    CHECK(clock.policy.stats().normal_intervals == entries);
    clock.input.normal_display = false;
    CHECK(clock.tick(9 * second) == Action::none); // Nested entries don't sample.
    CHECK(clock.policy.stats().normal_intervals == entries);
    clock.input.cadence_valid = false; // Hidden/unfocused: discard prior rolling window.
    CHECK(clock.tick(20 * second) == Action::none);
    CHECK(clock.policy.stats().cadence_suspensions == 1);
    clock.input.normal_display = clock.input.cadence_valid = true;
    CHECK(clock.tick() == Action::none); // Resumption is an anchor, not a giant period.
    CHECK(clock.policy.stats().last_window_mean_period_ns == 0);
    for (unsigned i = 0; i < 200; ++i) CHECK(clock.tick() == Action::none);
    CHECK(clock.policy.stats().last_window_mean_period_ns == second / 20);
    CHECK(clock.policy.stats().attempts == 1);
}

TEST_CASE("almetal: quiet sequence fails closed on invalidation failure and hard timeout",
          "[almetal][quiet-capture-policy]")
{
    SECTION("world loss before first eligible world does not start a sequence")
    {
        Clock clock;
        clock.input.world_ready = false;
        clock.input.world_invalidated = true;
        CHECK(clock.policy.update(clock.input) == Action::none);
        CHECK_FALSE(clock.policy.stats().started);
    }
    SECTION("teleport terminates after first world")
    {
        Clock clock;
        clock.early();
        clock.input.world_invalidated = true;
        CHECK(clock.tick() == Action::invalidated);
        CHECK(clock.tick(100 * second) == Action::none);
        CHECK(clock.policy.stats().attempts == 1);
    }
    SECTION("failed native attempt is never retried")
    {
        Clock clock;
        clock.early();
        clock.policy.markCaptureFailed();
        CHECK(clock.tick(100 * second) == Action::none);
        CHECK(clock.policy.stats().capture_failed);
        CHECK(clock.policy.stats().attempts == 1);
    }
    SECTION("maximum age also expires while the window is hidden")
    {
        Clock clock;
        clock.early();
        clock.input.normal_display = clock.input.cadence_valid = false;
        CHECK(clock.tick(298 * second) == Action::expired);
        CHECK(clock.policy.stats().attempts == 1);
        CHECK(clock.tick(second) == Action::none);
    }
    SECTION("invalid configuration never arms")
    {
        Policy::Config config;
        config.maximum_age_ns = 301 * second;
        Policy policy(config);
        Policy::Input input{second, 1, true, true, true, false, false};
        CHECK(policy.update(input) == Action::none);
        CHECK(policy.stats().terminal);
    }
}
