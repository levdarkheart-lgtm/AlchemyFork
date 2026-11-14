/**
 * @file almetal_frame_admission_test.cpp
 * @brief Fake native receipts exercise real admission/retirement policy.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalframeadmission.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <vector>
#include <utility>

namespace
{
using Budget = ALMetalFrameAdmission;
using Outcome = Budget::Outcome;
using State = Budget::CompletionState;
struct FakeNativeCompletions
{
    std::vector<Budget::CompletionReceipt> receipts;
    void submit(Budget& budget, Budget::Ticket ticket)
    {
        REQUIRE(budget.submitted(ticket));
        receipts.push_back({ticket, State::pending});
    }
    void signal(Budget::Ticket ticket, State state)
    {
        const auto receipt = std::find_if(receipts.begin(), receipts.end(),
            [ticket](const auto& value) { return value.ticket == ticket; });
        REQUIRE(receipt != receipts.end());
        receipt->state = state;
    }
    Budget::Retirement poll(Budget& budget) { return budget.prune(receipts); }
};
}

TEST_CASE("almetal: shadow admission bounds frame groups rather than six same-frame command buffers",
          "[almetal][frame-admission]")
{
    Budget budget;
    FakeNativeCompletions native;
    std::array<Budget::Ticket, 6> first{}, second{};
    for (unsigned index = 0; index < 6; ++index)
    {
        auto decision = budget.reserve(10); REQUIRE(decision);
        first[index] = decision.reservation; native.submit(budget, decision.reservation);
    }
    CHECK(budget.pendingFrameCount() == 1);
    CHECK(budget.pendingSubmissionCount() == 6);
    CHECK(budget.reserve(10).outcome == Outcome::same_frame_limit);
    for (unsigned index = 0; index < 6; ++index)
    {
        auto decision = budget.reserve(11); REQUIRE(decision);
        second[index] = decision.reservation; native.submit(budget, decision.reservation);
    }
    CHECK(budget.pendingFrameCount() == 2);
    CHECK(budget.pendingSubmissionCount() == 12);
    const auto blocked = budget.reserve(12);
    CHECK(blocked.outcome == Outcome::wait_for_prior_frame);
    CHECK(blocked.blocking_frame == 10);
    CHECK(budget.submittedTickets(blocked.blocking_frame) ==
          std::vector<Budget::Ticket>(first.begin(), first.end()));
    native.signal(first[0], State::completed);
    auto retired = native.poll(budget);
    CHECK(retired.completed == std::vector<Budget::Ticket>{first[0]});
    CHECK(budget.reserve(12).outcome == Outcome::wait_for_prior_frame);
    for (unsigned index = 1; index < 6; ++index) native.signal(first[index], State::completed);
    retired = native.poll(budget);
    CHECK(retired.completed.size() == 5);
    CHECK(budget.pendingFrameCount() == 1);
    CHECK(budget.reserve(12).outcome == Outcome::admitted);
}

TEST_CASE("almetal: completed same-frame submissions do not reset the invocation bound",
          "[almetal][frame-admission]")
{
    Budget budget;
    FakeNativeCompletions native;
    for (unsigned index = 0; index < 6; ++index)
    {
        auto decision = budget.reserve(20); REQUIRE(decision);
        native.submit(budget, decision.reservation);
        native.signal(decision.reservation, State::completed);
        native.poll(budget);
    }
    CHECK(budget.pendingFrameCount() == 0);
    CHECK(budget.latestFrameInvocationCount() == 6);
    CHECK(budget.reserve(20).outcome == Outcome::same_frame_limit);
    CHECK(budget.reserve(19).outcome == Outcome::stale_frame);
    CHECK(budget.reserve(21).outcome == Outcome::admitted);
}

TEST_CASE("almetal: uncommitted frame reservations cancel without manufacturing GPU completion",
          "[almetal][frame-admission]")
{
    Budget budget;
    FakeNativeCompletions native;
    const auto reserved = budget.reserve(30); REQUIRE(reserved);
    const auto submitted = budget.reserve(31); REQUIRE(submitted);
    native.submit(budget, submitted.reservation);
    CHECK(budget.reservationCount() == 1);
    const auto blocked = budget.reserve(32);
    CHECK(blocked.outcome == Outcome::unsubmitted_prior_frame);
    CHECK(blocked.blocking_frame == 30);
    const std::array fake_completed{Budget::CompletionReceipt{reserved.reservation, State::completed}};
    CHECK(budget.prune(fake_completed).ignored_receipts == 1);
    CHECK(budget.reserve(32).outcome == Outcome::unsubmitted_prior_frame);
    CHECK_FALSE(budget.cancel(submitted.reservation));
    REQUIRE(budget.cancel(reserved.reservation));
    CHECK_FALSE(budget.cancel(reserved.reservation));
    for (unsigned retry = 0; retry < 20; ++retry)
    {
        const auto current = budget.reserve(32); REQUIRE(current);
        CHECK(budget.latestFrameInvocationCount() == 1);
        REQUIRE(budget.cancel(current.reservation));
        CHECK(budget.latestFrameInvocationCount() == 0);
    }
    const auto current = budget.reserve(32); REQUIRE(current);
    native.submit(budget, current.reservation);
    CHECK_FALSE(budget.submitted(current.reservation));
}

TEST_CASE("almetal: completion pruning handles out-of-order frames and returns native failures",
          "[almetal][frame-admission]")
{
    Budget budget;
    FakeNativeCompletions native;
    const auto oldest = budget.reserve(40); REQUIRE(oldest);
    native.submit(budget, oldest.reservation);
    const auto newer = budget.reserve(41); REQUIRE(newer);
    native.submit(budget, newer.reservation);
    CHECK(budget.reserve(42).outcome == Outcome::wait_for_prior_frame);
    native.signal(newer.reservation, State::completed); // oldest is still pending
    auto retired = native.poll(budget);
    CHECK(retired.completed == std::vector<Budget::Ticket>{newer.reservation});
    CHECK(budget.pendingFrameCount() == 1);
    const auto latest = budget.reserve(42); REQUIRE(latest);
    native.submit(budget, latest.reservation);
    native.signal(oldest.reservation, State::failed);
    retired = native.poll(budget);
    CHECK(retired.failed == std::vector<Budget::Ticket>{oldest.reservation});
    CHECK(retired.completed.empty());
    CHECK(budget.pendingSubmissionCount() == 1);
    CHECK(budget.submittedTickets(40).empty());
    CHECK(budget.submittedTickets(42) == std::vector<Budget::Ticket>{latest.reservation});
    CHECK(budget.reserve(41).outcome == Outcome::stale_frame);
}

TEST_CASE("almetal: zero admission limits clamp to one and frame zero remains a valid tag",
          "[almetal][frame-admission]")
{
    Budget budget(0, 0);
    FakeNativeCompletions native;
    const auto first = budget.reserve(0); REQUIRE(first);
    CHECK(budget.reserve(0).outcome == Outcome::same_frame_limit);
    CHECK(budget.reserve(1).outcome == Outcome::unsubmitted_prior_frame);
    native.submit(budget, first.reservation);
    const auto blocked = budget.reserve(1);
    CHECK(blocked.outcome == Outcome::wait_for_prior_frame);
    CHECK(blocked.blocking_frame == 0);
    native.signal(first.reservation, State::completed);
    CHECK(native.poll(budget).completed.size() == 1);
    CHECK(budget.reserve(1).outcome == Outcome::admitted);
    CHECK_FALSE(budget.cancel(0)); CHECK_FALSE(budget.submitted(0));
    const std::array unknown{Budget::CompletionReceipt{999, State::failed}};
    const auto ignored = budget.prune(unknown);
    CHECK(ignored.ignored_receipts == 1);
    CHECK(ignored.failed.empty());
}

TEST_CASE("almetal: renderer reload moves pending frame identities without dropping admission state",
          "[almetal][frame-admission]")
{
    Budget old;
    FakeNativeCompletions native;
    const auto first = old.reserve(50); REQUIRE(first);
    native.submit(old, first.reservation);
    const auto second = old.reserve(51); REQUIRE(second);
    native.submit(old, second.reservation);
    Budget replacement;
    replacement = std::move(old); // native command owners move alongside this
    CHECK(replacement.pendingFrameCount() == 2);
    const auto blocked = replacement.reserve(52);
    CHECK(blocked.outcome == Outcome::wait_for_prior_frame);
    CHECK(blocked.blocking_frame == 50);
    CHECK(replacement.submittedTickets(50) == std::vector<Budget::Ticket>{first.reservation});
    const std::array unknown_state{Budget::CompletionReceipt{
        first.reservation, static_cast<State>(99)}};
    CHECK(replacement.prune(unknown_state).ignored_receipts == 1);
    CHECK(replacement.pendingFrameCount() == 2);
    native.signal(first.reservation, State::completed);
    CHECK(native.poll(replacement).completed == std::vector<Budget::Ticket>{first.reservation});
    CHECK(replacement.reserve(52).outcome == Outcome::admitted);
    CHECK(replacement.pendingSubmissionCount() == 1);
    CHECK(replacement.reservationCount() == 1);
}
