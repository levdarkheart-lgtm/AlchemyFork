/**
 * @file alprobeupdateadmission_test.cpp
 * @brief Fresh probe groups keep separate ages and cannot steal every progress turn.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../alprobeupdateadmission.h"
#include "alnativeframebudget.h"
#include <catch2/catch_test_macros.hpp>

namespace
{
using Admission = ALProbeUpdateAdmission;
using Kind = Admission::Kind;
ALNativeFrameBudget::FrameInputs busyFrame(std::uint64_t frame)
{
    ALNativeFrameBudget::FrameInputs value;
    value.frame = frame;
    value.frame_interval_ns = 50'000'000;
    value.required_main_cpu_ns = 45'000'000;
    return value;
}
}

TEST_CASE("Scheduled probe age survives realtime admission and wins its next progress opportunity",
          "[native-frame-budget][probe-admission][PPD-142]")
{
    Admission fairness;
    ALNativeFrameBudget budget;
    ALNativeFrameBudget::Lease realtime;
    for (std::uint64_t frame = 1; frame <= 9; ++frame)
    {
        budget.beginFrame(busyFrame(frame));
        fairness.request(Kind::scheduled, frame);
        fairness.request(Kind::realtime, frame);
        CHECK(fairness.oldest(Kind::scheduled) == 1);
        CHECK(fairness.mayStart(Kind::realtime)); // original order on equal-age first demand
        auto decision = budget.tryAcquire(ALNativeFrameBudget::Work::ordinary_probe, 0,
                                          fairness.oldest(Kind::realtime));
        if (frame < 9) CHECK_FALSE(decision);
        else
        {
            REQUIRE(decision);
            realtime = std::move(decision.credit);
            fairness.started(Kind::realtime);
        }
    }
    CHECK(fairness.waiting(Kind::scheduled));
    CHECK(fairness.oldest(Kind::scheduled) == 1);
    // The second coherent half uses the retained group; it does not consume
    // another admission or erase the waiting scheduled request's age.
    budget.beginFrame(busyFrame(10));
    realtime.reset();
    fairness.request(Kind::realtime, 10); // even a new realtime candidate cannot steal this turn
    CHECK_FALSE(fairness.mayStart(Kind::realtime));
    REQUIRE(fairness.mayStart(Kind::scheduled));
    auto scheduled = budget.tryAcquire(ALNativeFrameBudget::Work::ordinary_probe, 0,
                                       fairness.oldest(Kind::scheduled));
    REQUIRE(scheduled);
    CHECK(scheduled.reason == ALNativeFrameBudget::Reason::progress);
    fairness.started(Kind::scheduled);
    CHECK_FALSE(fairness.waiting(Kind::scheduled));
    CHECK(fairness.waiting(Kind::realtime));
    CHECK(fairness.oldest(Kind::realtime) == 10);
}

TEST_CASE("Older scheduled requests cannot be overtaken by continually renewed realtime candidates",
          "[probe-admission][PPD-142]")
{
    Admission fairness;
    fairness.request(Kind::scheduled, 0); // startup zero is a valid age
    for (std::uint64_t frame = 1; frame < 32; ++frame)
    {
        fairness.request(Kind::realtime, frame);
        CHECK_FALSE(fairness.mayStart(Kind::realtime));
        CHECK(fairness.mayStart(Kind::scheduled));
        CHECK(fairness.oldest(Kind::scheduled) == 0);
        fairness.cancel(Kind::realtime);
    }
    fairness.started(Kind::scheduled);
    fairness.request(Kind::scheduled, 33);
    fairness.request(Kind::realtime, 33);
    CHECK(fairness.mayStart(Kind::realtime));
}

TEST_CASE("Probe group cancellation and completion release only their own pending request",
          "[probe-admission]")
{
    Admission fairness;
    fairness.request(Kind::realtime, 2);
    fairness.request(Kind::scheduled, 3);
    CHECK(fairness.mayStart(Kind::realtime));
    fairness.started(Kind::realtime);
    CHECK(fairness.mayStart(Kind::scheduled));
    fairness.request(Kind::realtime, 4);
    CHECK_FALSE(fairness.mayStart(Kind::realtime));
    fairness.cancel(Kind::scheduled);
    CHECK(fairness.mayStart(Kind::realtime));
    fairness.reset();
    CHECK_FALSE(fairness.waiting(Kind::scheduled));
    CHECK_FALSE(fairness.waiting(Kind::realtime));
}
