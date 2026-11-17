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
#include <initializer_list>

namespace
{
    using Policy = ALMetalQuietCapturePolicy;
    using Action = Policy::Action;
    constexpr auto second = Policy::second;
    template<typename P>
    bool rearm(P& policy,const Policy::Input& arrival)
    {
        // Keep the new behavior test executable against the original policy:
        // an unavailable rearm API cannot satisfy the new scene contract.
        if constexpr(requires { policy.rearmAfterInvalidation(arrival); })
            return policy.rearmAfterInvalidation(arrival);
        return false;
    }
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
        void late()
        {
            Action action=Action::none;
            for(unsigned i=0;i<1000 && action==Action::none;++i)
                action=tick(second/10);
            REQUIRE((action==Action::late_decline || action==Action::late_fallback));
            REQUIRE(policy.stats().attempts==2);
        }
        Policy::Input arrival() const
        {
            auto value=input;value.now_ns+=second;value.frame+=1;
            value.world_ready=value.normal_display=value.cadence_valid=true;
            value.world_invalidated=false;return value;
        }
    };
}

TEST_CASE("almetal: explicit post-teleport arrival rearms an independent scene baseline",
          "[almetal][quiet-capture-policy]")
{
    Clock clock;clock.early();
    clock.input.world_invalidated=true;
    clock.input.previous_interval_captured=true; // An interrupted old capture is not new-scene cadence.
    REQUIRE(clock.tick()==Action::invalidated);
    auto arrival=clock.input;arrival.world_invalidated=false;
    arrival.now_ns+=second;arrival.frame+=1;
    REQUIRE(rearm(clock.policy,arrival));
    CHECK(clock.policy.stats().scene_generation==2);
    CHECK(clock.policy.stats().total_attempts==1);
    clock.input=arrival;
    REQUIRE(clock.policy.update(clock.input)==Action::none);
    CHECK(clock.policy.stats().world_start_ns==arrival.now_ns);
    CHECK(clock.policy.stats().attempts==0);
    CHECK(clock.policy.stats().baseline_periods==0);
    CHECK(clock.policy.stats().previous_window_mean_period_ns==0);
    clock.input.previous_interval_captured=false;
    for(unsigned i=0;i<39;++i)REQUIRE(clock.tick()==Action::none);
    CHECK(clock.tick()==Action::early);
    CHECK(clock.policy.stats().baseline_mean_period_ns==second/20);
    CHECK(clock.policy.stats().attempts==1);
    CHECK(clock.policy.stats().total_attempts==2);
}

TEST_CASE("almetal: Home and club consume at most four capture attempts with no third generation",
          "[almetal][quiet-capture-policy]")
{
    Clock clock;clock.early();clock.late();
    CHECK(clock.policy.stats().scene_generation==1);
    CHECK(clock.policy.stats().total_attempts==2);
    clock.input.world_invalidated=true;
    REQUIRE(clock.tick()==Action::invalidated);
    REQUIRE(rearm(clock.policy,clock.arrival()));
    clock.input=clock.arrival();clock.early();clock.late();
    CHECK(clock.policy.stats().scene_generation==2);
    CHECK(clock.policy.stats().total_attempts==4);
    for(unsigned i=0;i<100;++i)CHECK(clock.tick()==Action::none);
    clock.input.world_invalidated=true;
    REQUIRE(clock.tick()==Action::invalidated);
    CHECK_FALSE(rearm(clock.policy,clock.arrival()));
    CHECK(clock.policy.stats().total_attempts==4);
    CHECK(clock.policy.stats().scene_generation==2);
}

TEST_CASE("almetal: scene rearm requires fresh normal arrival after every invalidation observation",
          "[almetal][quiet-capture-policy]")
{
    Clock clock;clock.early();clock.input.world_invalidated=true;
    REQUIRE(clock.tick()==Action::invalidated);
    const auto stale=clock.arrival();
    CHECK(clock.tick(2*second)==Action::none); // Still teleporting; advance the floor.
    CHECK_FALSE(rearm(clock.policy,stale));
    const auto arrival=clock.arrival();
    auto invalid=arrival;invalid.world_ready=false;
    CHECK_FALSE(rearm(clock.policy,invalid));
    invalid=arrival;invalid.normal_display=false;
    CHECK_FALSE(rearm(clock.policy,invalid));
    invalid=arrival;invalid.cadence_valid=false;
    CHECK_FALSE(rearm(clock.policy,invalid));
    invalid=arrival;invalid.world_invalidated=true;
    CHECK_FALSE(rearm(clock.policy,invalid));
    invalid=arrival;invalid.now_ns=clock.input.now_ns;
    CHECK_FALSE(rearm(clock.policy,invalid));
    invalid=arrival;invalid.frame=clock.input.frame;
    CHECK_FALSE(rearm(clock.policy,invalid));
    CHECK(clock.policy.stats().scene_generation==1);
    CHECK(clock.policy.stats().total_attempts==1);
    REQUIRE(rearm(clock.policy,arrival));
    CHECK_FALSE(rearm(clock.policy,arrival)); // Repeated rearm cannot spend another generation.
    REQUIRE(clock.policy.update(arrival)==Action::none);
    CHECK(clock.policy.stats().normal_intervals==1);
    invalid=arrival;invalid.now_ns+=second;
    CHECK(clock.policy.update(invalid)==Action::none); // Duplicate display ID.
    CHECK(clock.policy.stats().normal_intervals==1);
    CHECK(clock.policy.stats().world_start_ns==arrival.now_ns);
}

TEST_CASE("almetal: scene rearm cannot revive native failures expiry or an unstarted session",
          "[almetal][quiet-capture-policy]")
{
    SECTION("no reported invalidation")
    {
        Clock clock;clock.early();CHECK_FALSE(rearm(clock.policy,clock.arrival()));
        clock.late();CHECK_FALSE(rearm(clock.policy,clock.arrival()));
        CHECK(clock.policy.stats().total_attempts==2);
    }
    SECTION("failed capture remains failed even after a teleport")
    {
        Clock clock;clock.early();clock.policy.markCaptureFailed();
        clock.input.world_invalidated=true;
        CHECK(clock.tick()==Action::none);
        CHECK_FALSE(rearm(clock.policy,clock.arrival()));
        CHECK(clock.policy.stats().capture_failed);
        CHECK(clock.policy.stats().total_attempts==1);
    }
    SECTION("failure while cancelling an invalidated capture remains global")
    {
        Clock clock;clock.early();clock.input.world_invalidated=true;
        REQUIRE(clock.tick()==Action::invalidated);clock.policy.markCaptureFailed();
        CHECK_FALSE(rearm(clock.policy,clock.arrival()));
        CHECK(clock.policy.stats().last_action==Action::invalidated);
        CHECK(clock.policy.stats().capture_failed);
    }
    SECTION("expiry has distinct terminal ownership")
    {
        Clock clock;clock.early();clock.input.normal_display=clock.input.cadence_valid=false;
        REQUIRE(clock.tick(298*second)==Action::expired);
        CHECK_FALSE(rearm(clock.policy,clock.arrival()));
        CHECK(clock.policy.stats().last_action==Action::expired);
        CHECK_FALSE(clock.policy.stats().capture_failed);
    }
    SECTION("world readiness and normal display are required before any scene")
    {
        Clock clock;clock.input.world_ready=false;clock.input.world_invalidated=true;
        CHECK(clock.policy.update(clock.input)==Action::none);
        CHECK_FALSE(rearm(clock.policy,clock.arrival()));
        clock.input.world_invalidated=false;clock.input.world_ready=true;
        clock.input.normal_display=false;
        for(unsigned i=0;i<5;++i)CHECK(clock.tick(second)==Action::none);
        CHECK_FALSE(clock.policy.stats().started);
        CHECK(clock.policy.stats().total_attempts==0);
        CHECK_FALSE(rearm(clock.policy,clock.arrival()));
    }
}

TEST_CASE("almetal: scene generation configuration is bounded and can retain one-scene behavior",
          "[almetal][quiet-capture-policy]")
{
    for(const auto limit:{0u,3u})
    {
        Policy::Config config;config.maximum_scene_generations=limit;
        Clock clock;clock.policy=Policy(config);
        CHECK(clock.policy.update(clock.input)==Action::none);
        CHECK(clock.policy.stats().terminal);
        CHECK_FALSE(rearm(clock.policy,clock.arrival()));
        CHECK(clock.policy.stats().total_attempts==0);
    }
    Clock clock;Policy::Config config;config.maximum_scene_generations=1;
    clock.policy=Policy(config);clock.early();clock.input.world_invalidated=true;
    REQUIRE(clock.tick()==Action::invalidated);
    CHECK_FALSE(rearm(clock.policy,clock.arrival()));
    CHECK(clock.policy.stats().scene_generation==1);
    CHECK(clock.policy.stats().total_attempts==1);
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
