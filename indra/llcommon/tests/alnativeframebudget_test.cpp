/**
 * @file alnativeframebudget_test.cpp
 * @brief Shared streaming/probe admission and actual owner-retirement contracts.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "../alnativeframebudget.h"
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <vector>

namespace
{
using Budget = ALNativeFrameBudget;
Budget::FrameInputs frame(Budget::Frame id)
{
    Budget::FrameInputs input;
    input.frame = id;
    input.frame_interval_ns = 50'000'000;
    input.required_main_cpu_ns = 20'000'000;
    return input;
}
Budget::Limits small()
{
    Budget::Limits limits;
    limits.uploads_per_frame = 2;
    limits.uploads_in_flight = 2;
    limits.upload_bytes_per_frame = 16;
    limits.upload_bytes_in_flight = 16;
    limits.maximum_deferral_frames = 3;
    return limits;
}
}

TEST_CASE("Native frame budget keeps upload credits until their final completion owner retires",
          "[native-frame-budget]")
{
    Budget budget(small());
    budget.beginFrame(frame(1));
    auto first = budget.tryAcquire(Budget::Work::upload, 8);
    auto second = budget.tryAcquire(Budget::Work::upload, 8);
    REQUIRE(first); REQUIRE(second);
    auto native_completion_owner = first.credit;
    first.credit.reset();
    budget.beginFrame(frame(2));
    CHECK_FALSE(budget.tryAcquire(Budget::Work::upload, 1));
    CHECK(budget.snapshot().upload_bytes_in_flight == 16);
    native_completion_owner.reset();
    CHECK(budget.snapshot().upload_bytes_in_flight == 8);
    auto replacement = budget.tryAcquire(Budget::Work::upload, 8);
    REQUIRE(replacement);
    second.credit.reset(); replacement.credit.reset();
    CHECK(budget.snapshot().uploads_in_flight == 0);
    CHECK(budget.snapshot().retired == 3);
}

TEST_CASE("Native frame budget progresses oversized uploads without permitting concurrent byte overflow",
          "[native-frame-budget]")
{
    Budget budget(small());
    budget.beginFrame(frame(7));
    auto oversized = budget.tryAcquire(Budget::Work::upload, 64);
    REQUIRE(oversized);
    CHECK_FALSE(budget.tryAcquire(Budget::Work::upload, 1));
    budget.beginFrame(frame(8));
    CHECK_FALSE(budget.tryAcquire(Budget::Work::upload, 1));
    oversized.credit.reset();
    CHECK(budget.tryAcquire(Budget::Work::upload, 1));
    Budget overflow;
    auto maximum = overflow.tryAcquire(Budget::Work::upload, Budget::unknown_bytes);
    REQUIRE(maximum);
    CHECK_FALSE(overflow.tryAcquire(Budget::Work::upload, 1));
}

TEST_CASE("Native frame budget shares main and streaming debt while guaranteeing aged view progress",
          "[native-frame-budget]")
{
    Budget budget(small());
    auto input = frame(10);
    input.required_main_cpu_ns = 45'000'000;
    budget.beginFrame(input);
    CHECK(budget.decodeCeiling(2, 4, 6) == 2);
    auto upload = budget.tryAcquire(Budget::Work::upload, 4, 10);
    REQUIRE(upload);
    CHECK(upload.reason == Budget::Reason::progress);
    CHECK_FALSE(budget.tryAcquire(Budget::Work::upload, 4, 10));
    CHECK_FALSE(budget.tryAcquire(Budget::Work::ordinary_probe, 0, 10));
    auto ordinary = budget.tryAcquire(Budget::Work::ordinary_probe, 0, 7);
    auto hero = budget.tryAcquire(Budget::Work::hero_probe, 0, 7);
    REQUIRE(ordinary); REQUIRE(hero);
    CHECK(ordinary.reason == Budget::Reason::progress);
    CHECK_FALSE(budget.tryAcquire(Budget::Work::ordinary_probe, 0, 7));
    budget.beginFrame(frame(11));
    Budget::StreamingBacklog backlog;
    backlog.queued_requests = 2;
    backlog.queued_input_bytes = 16;
    backlog.oldest_request_frame = 7;
    budget.updateStreaming(backlog);
    CHECK(budget.decodeCeiling(2, 4, 6) == 2);
    budget.updateStreaming({});
    CHECK(budget.decodeCeiling(2, 4, 6) == 6);
    auto preparation = budget.tryAcquire(Budget::Work::preparation);
    REQUIRE(preparation);
    CHECK(budget.decodeCeiling(2, 4, 6) == 5);
}

TEST_CASE("Nested frames cannot refill native work and draining does not require future ticks",
          "[native-frame-budget]")
{
    Budget budget(small());
    budget.beginFrame(frame(3));
    { auto a = budget.tryAcquire(Budget::Work::upload, 8); REQUIRE(a); }
    { auto b = budget.tryAcquire(Budget::Work::upload, 8); REQUIRE(b); }
    budget.beginFrame(frame(3));
    budget.beginFrame(frame(2));
    CHECK_FALSE(budget.tryAcquire(Budget::Work::upload, 1));
    CHECK(budget.snapshot().frame == 3);
    budget.beginDrain();
    CHECK(budget.tryAcquire(Budget::Work::upload, 1));
    CHECK(budget.decodeCeiling(2, 2, 6) == 6);
    CHECK_FALSE(budget.tryAcquire(Budget::Work::preparation));
    CHECK_FALSE(budget.tryAcquire(Budget::Work::ordinary_probe));
}

TEST_CASE("Native completion credits safely outlive the frame policy owner",
          "[native-frame-budget]")
{
    Budget::Lease completion;
    {
        Budget budget;
        budget.beginFrame(frame(1));
        completion = budget.tryAcquire(Budget::Work::upload, 1024).credit;
        REQUIRE(completion);
    }
    completion.reset();
    SUCCEED("Completion retirement does not borrow the destroyed policy owner");
}

TEST_CASE("A single large native upload constrains decoder starts until its completion owner releases",
          "[native-frame-budget][PPD-121]")
{
    Budget budget;
    budget.beginFrame(frame(1));
    CHECK(budget.decodeCeiling(2, 4, 6) == 6);
    auto upload = budget.tryAcquire(Budget::Work::upload, 64ULL * 1024 * 1024);
    REQUIRE(upload);
    const auto occupied = budget.snapshot();
    CHECK(occupied.streaming.queued_requests == 0);
    CHECK(occupied.streaming.queued_input_bytes == 0);
    CHECK(occupied.uploads_in_flight == 1);
    CHECK(occupied.uploads_in_flight < Budget::Limits{}.uploads_in_flight);
    CHECK(budget.decodeCeiling(2, 4, 6) == 2);
    CHECK_FALSE(budget.tryAcquire(Budget::Work::upload, 1));

    // Submission/publication does not release the credit held by its actual
    // completion owner. Reducing new decode starts cancels no accepted work.
    auto completion_owner = upload.credit;
    upload.credit.reset();
    budget.beginFrame(frame(2));
    CHECK(budget.decodeCeiling(2, 4, 6) == 2);
    CHECK(budget.snapshot().retired == 0);
    completion_owner.reset();
    CHECK(budget.snapshot().upload_bytes_in_flight == 0);
    CHECK(budget.snapshot().retired == 1);
    CHECK(budget.decodeCeiling(2, 2, 6) == 6);
    CHECK(budget.tryAcquire(Budget::Work::upload, 1));
}

TEST_CASE("An optional update group shares one credit across every selected face command",
          "[native-frame-budget]")
{
    Budget budget(small());
    auto input = frame(10);
    input.required_main_cpu_ns = 45'000'000;
    budget.beginFrame(input);
    auto ordinary = budget.tryAcquire(Budget::Work::ordinary_probe, 0, 7);
    auto hero = budget.tryAcquire(Budget::Work::hero_probe, 0, 7);
    REQUIRE(ordinary); REQUIRE(hero);
    std::vector<Budget::Lease> selected_faces(6, ordinary.credit);
    ordinary.credit.reset();
    CHECK(budget.snapshot().optional_views_in_flight == 2);
    CHECK(budget.snapshot().optional_views_this_frame == 2);
    for (unsigned face = 0; face < 5; ++face) selected_faces[face].reset();
    CHECK(budget.snapshot().optional_views_in_flight == 2);
    selected_faces.back().reset();
    CHECK(budget.snapshot().optional_views_in_flight == 1);
    hero.credit.reset();
    CHECK(budget.snapshot().optional_views_in_flight == 0);
    CHECK(budget.snapshot().retired == 2);
}
