/**
 * @file alnativeframeplan_test.cpp
 * @brief Actual outer-frame and optional submission-owner scope contracts.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "../alnativeframeplan.h"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <memory>
#include <utility>

namespace
{
using Budget = ALNativeFrameBudget;
using ALNativeFramePlan::MainScope;
using ALNativeFramePlan::OptionalScope;

Budget::FrameInputs frameInputs()
{
    Budget::FrameInputs inputs;
    // The owner supplies the serial. Zero interval deliberately disables the
    // CPU threshold: these tests assert state/lifetime, never elapsed timing.
    inputs.frame = std::numeric_limits<Budget::Frame>::max();
    return inputs;
}
}

TEST_CASE("Native outer frame scope advances once and nested scopes cannot refill admission",
          "[native-frame-plan]")
{
    auto& budget = Budget::instance();
    const auto previous = budget.snapshot().frame;
    Budget::Frame first = 0;
    {
        MainScope outer(frameInputs());
        first = budget.snapshot().frame;
        CHECK(first == previous + 1);
        CHECK(budget.snapshot().initialized);
        CHECK(budget.snapshot().main_observations_open);
        REQUIRE(budget.recordRequiredMainGpuWait(first, 100, 0));
        CHECK_FALSE(budget.snapshot().memory_pressure);
        for (unsigned at = 0; at < Budget::Limits{}.uploads_per_frame; ++at)
        {
            auto upload = budget.tryAcquire(Budget::Work::upload, 1);
            REQUIRE(upload);
        }
        const auto exhausted = budget.snapshot();
        auto conflicting = frameInputs();
        conflicting.os_available_bytes = 0;
        {
            MainScope nested(conflicting);
            nested.recordRequiredWork();
            CHECK(budget.snapshot().frame == first);
            CHECK(budget.snapshot().uploads_this_frame == exhausted.uploads_this_frame);
            CHECK_FALSE(budget.snapshot().memory_pressure);
            CHECK(budget.snapshot().main_observations_open);
            CHECK(budget.snapshot().required_main_gpu_wait_observations == 1);
            const auto denied = budget.tryAcquire(Budget::Work::upload, 1);
            CHECK_FALSE(denied);
            CHECK(denied.reason == Budget::Reason::frame_work);
        }
        // Destroying an inactive nested scope must leave the outer TLS owner
        // active, including for a later nested display/snapshot recursion.
        MainScope later_nested(frameInputs());
        CHECK(budget.snapshot().frame == first);
        CHECK(budget.snapshot().main_observations_open);
        CHECK(budget.snapshot().uploads_this_frame == exhausted.uploads_this_frame);
        outer.recordRequiredWork();
    }
    CHECK_FALSE(budget.snapshot().main_observations_open);
    CHECK_FALSE(budget.recordRequiredMainGpuWait(first, 1, 0));
    MainScope next(frameInputs());
    CHECK(budget.snapshot().frame == first + 1);
    CHECK(budget.snapshot().uploads_this_frame == 0);
    CHECK(budget.snapshot().main_observations_open);
    CHECK(budget.snapshot().required_main_gpu_wait_observations == 0);
    CHECK(budget.tryAcquire(Budget::Work::upload, 1));
}

TEST_CASE("Native optional groups restore submission TLS and retire only their own credits",
          "[native-frame-plan]")
{
    auto& budget = Budget::instance();
    const auto previous_owner = ALMetalSubmissionScope::current();
    auto surrounding_owner = std::make_shared<int>(17);
    {
        ALMetalSubmissionScope surrounding(surrounding_owner);
        {
            MainScope main(frameInputs());
            const auto frame = budget.snapshot().frame;
            const auto retired = budget.snapshot().retired;
            auto ordinary = budget.tryAcquire(Budget::Work::ordinary_probe, 0, frame);
            REQUIRE(ordinary);
            std::weak_ptr<const Budget::Credit> ordinary_lifetime = ordinary.credit;
            const auto* ordinary_owner = ordinary.credit.get();
            {
                OptionalScope outer(std::move(ordinary.credit));
                CHECK(ALMetalSubmissionScope::current().get() == ordinary_owner);
                CHECK(budget.snapshot().optional_views_in_flight == 1);
                auto hero = budget.tryAcquire(Budget::Work::hero_probe, 0, frame);
                REQUIRE(hero);
                std::weak_ptr<const Budget::Credit> hero_lifetime = hero.credit;
                const auto* hero_owner = hero.credit.get();
                {
                    OptionalScope inner(std::move(hero.credit));
                    CHECK(ALMetalSubmissionScope::current().get() == hero_owner);
                    CHECK(budget.snapshot().optional_views_in_flight == 2);
                    CHECK_FALSE(ordinary_lifetime.expired());
                }
                CHECK(hero_lifetime.expired());
                CHECK_FALSE(ordinary_lifetime.expired());
                CHECK(budget.snapshot().optional_views_in_flight == 1);
                CHECK(ALMetalSubmissionScope::current().get() == ordinary_owner);
                {
                    OptionalScope empty(Budget::Lease{});
                    CHECK_FALSE(ALMetalSubmissionScope::current());
                    CHECK(budget.snapshot().optional_views_in_flight == 1);
                }
                CHECK(ALMetalSubmissionScope::current().get() == ordinary_owner);
            }
            CHECK(ordinary_lifetime.expired());
            CHECK(budget.snapshot().optional_views_in_flight == 0);
            CHECK(budget.snapshot().retired == retired + 2);
            CHECK(ALMetalSubmissionScope::current().get() == surrounding_owner.get());
            // The real display-end call occurs after optional group teardown.
            // Recording it cannot revive an expired credit or refill the frame.
            main.recordRequiredWork();
            CHECK(budget.snapshot().frame == frame);
            CHECK(budget.snapshot().optional_views_this_frame == 2);
            CHECK(budget.snapshot().optional_views_in_flight == 0);
            CHECK(ALMetalSubmissionScope::current().get() == surrounding_owner.get());
        }
        // Optional groups also work without a live MainScope and restore the
        // surrounding command owner without borrowing the destroyed frame.
        auto detached = budget.tryAcquire(Budget::Work::preparation);
        REQUIRE(detached);
        std::weak_ptr<const Budget::Credit> lifetime = detached.credit;
        {
            OptionalScope outside_frame(std::move(detached.credit));
            CHECK_FALSE(lifetime.expired());
        }
        CHECK(lifetime.expired());
        CHECK(ALMetalSubmissionScope::current().get() == surrounding_owner.get());
    }
    CHECK(ALMetalSubmissionScope::current() == previous_owner);
}

TEST_CASE("Outer frame lifecycle continuity carries only the closed predecessor GPU hint",
          "[native-frame-plan][main-gpu-history][PPD-409]")
{
    auto& budget = Budget::instance();
    auto input = frameInputs();
    { MainScope reset(input); } // Isolate the singleton from prior test history.
    input.main_pressure_continuity = true;
    Budget::Frame previous = 0;
    {
        MainScope first(input);
        previous = budget.snapshot().frame;
        REQUIRE(budget.recordRequiredMainGpuWait(previous, 800, 100));
    }
    {
        MainScope early(input);
        CHECK(budget.snapshot().previous_main_gpu_wait_valid);
        CHECK(budget.snapshot().previous_main_gpu_wait_frame == previous);
        CHECK(budget.snapshot().previous_main_gpu_wait_non_cpu_ns == 700);
        CHECK(budget.snapshot().required_main_gpu_wait_observations == 0);
        CHECK(budget.snapshot().required_main_gpu_pressure_ns == 700);
        auto reset_nested = input;
        reset_nested.main_pressure_continuity = false;
        { MainScope nested(reset_nested); }
        CHECK(budget.snapshot().previous_main_gpu_wait_non_cpu_ns == 700);
    }
    {
        MainScope quiet(input);
        CHECK(budget.snapshot().previous_main_gpu_wait_valid);
        CHECK(budget.snapshot().previous_main_gpu_wait_non_cpu_ns == 0);
        REQUIRE(budget.recordRequiredMainGpuWait(budget.snapshot().frame, 900, 0));
    }
    input.main_pressure_continuity = false; // Startup/teleport/disconnect owner boundary.
    { MainScope reset(input); CHECK_FALSE(budget.snapshot().previous_main_gpu_wait_valid); }
    input.main_pressure_continuity = true;
    { MainScope resumed(input); CHECK_FALSE(budget.snapshot().previous_main_gpu_wait_valid); }
}
