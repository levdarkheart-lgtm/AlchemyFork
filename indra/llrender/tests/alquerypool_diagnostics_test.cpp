/** Benchmark-only visibility diagnostics preserve ticket/retirement semantics. */
#include <catch2/catch_test_macros.hpp>

#include "alquerypool.h"

namespace
{
struct DiagnosticsOffOnExit
{
    ALQueryPool& pool;
    ~DiagnosticsOffOnExit() { pool.setDiagnosticsEnabled(false); }
};

ALQueryTicket closed_ticket(ALQueryPool& pool)
{
    const auto ticket = pool.begin(ALQueryKind::Occlusion);
    pool.end(ticket);
    return ticket;
}
}

TEST_CASE("Query diagnostics separate armed main tickets, duplicate polls and retirement",
          "[alquerypool][diagnostics]")
{
    ALQueryPool pool;
    DiagnosticsOffOnExit cleanup{pool};
    pool.setDiagnosticsEnabled(false);
    REQUIRE(pool.beginVisibilityCapture(true));
    const auto before_arming = closed_ticket(pool);
    REQUIRE(before_arming.isValid());
    pool.endVisibilityCapture();

    pool.setDiagnosticsEnabled(true);
    const U64 epoch = pool.diagnostics().epoch;
    REQUIRE(epoch != 0);
    U64 value = 99;
    CHECK(pool.result(before_arming, value) == ALQueryPool::Status::Expired);
    CHECK(pool.diagnostics().untracked_polls == 1);
    CHECK(pool.diagnostics().expired_unique == 0);

    // The default scope is intentionally unclassified, rather than assuming
    // tests, future probe callers or pre-arming tickets are main-camera work.
    REQUIRE(pool.beginVisibilityCapture());
    const auto unclassified = closed_ticket(pool);
    REQUIRE(unclassified.isValid());
    pool.endVisibilityCapture();
    CHECK(pool.diagnostics().acquired == 0);

    REQUIRE(pool.beginVisibilityCapture(true));
    const auto observed = closed_ticket(pool);
    const auto unread = closed_ticket(pool);
    REQUIRE(observed.isValid());
    REQUIRE(unread.isValid());
    CHECK(pool.diagnostics().acquired == 2);
    CHECK_FALSE(pool.attachVisibilityResult(observed, nullptr, 0));
    CHECK(pool.diagnostics().attachment_calls == 0);
    CHECK(pool.result(observed, value) == ALQueryPool::Status::Expired);
    CHECK(value == 0);
    CHECK(pool.result(observed, value) == ALQueryPool::Status::Expired);
    CHECK(pool.diagnostics().expired_polls == 2);
    CHECK(pool.diagnostics().expired_unique == 1);
    CHECK(pool.diagnostics().duplicate_phase_polls == 1);
    pool.discard(unread);
    pool.discard(unread);
    CHECK(pool.diagnostics().discarded_unique == 1);
    pool.endVisibilityCapture();

    for (U32 i = 0; i < ALQueryPool::RESULT_FRAMES; ++i) pool.endFrame();
    const auto retired = pool.diagnostics();
    CHECK(retired.retired == 2);
    CHECK(retired.retired_without_terminal_observation == 1);
    CHECK(pool.result(observed, value) == ALQueryPool::Status::Expired);
    CHECK(pool.diagnostics().untracked_polls == 2);
    CHECK(pool.diagnostics().expired_unique == 1);
    pool.release();
    CHECK(pool.diagnostics().retired == 2);

    pool.recordMainWorldGroupTransition(true);
    pool.recordMainWorldGroupTransition(false);
    pool.recordMainWorldGroupSkip(ALQueryPool::GroupSkip::spatial_cull);
    pool.recordMainWorldGroupSkip(ALQueryPool::GroupSkip::state_sort);
    CHECK(pool.diagnostics().group_occluded_transitions == 1);
    CHECK(pool.diagnostics().group_visible_transitions == 1);
    CHECK(pool.diagnostics().spatial_group_skips == 1);
    CHECK(pool.diagnostics().state_sort_group_skips == 1);

    pool.setDiagnosticsEnabled(false);
    pool.recordMainWorldGroupSkip(ALQueryPool::GroupSkip::state_sort);
    CHECK(pool.diagnostics().state_sort_group_skips == 1);
    pool.setDiagnosticsEnabled(true);
    CHECK(pool.diagnostics().epoch != epoch);
    CHECK(pool.diagnostics().acquired == 0);
    CHECK(pool.diagnostics().expired_polls == 0);
    CHECK(pool.diagnostics().state_sort_group_skips == 0);
}

TEST_CASE("Rearming query diagnostics does not relabel live old tickets",
          "[alquerypool][diagnostics]")
{
    ALQueryPool pool;
    DiagnosticsOffOnExit cleanup{pool};
    pool.setDiagnosticsEnabled(true);
    REQUIRE(pool.beginVisibilityCapture(true));
    const auto old = closed_ticket(pool);
    REQUIRE(old.isValid());
    pool.endVisibilityCapture();
    const U64 old_epoch = pool.diagnostics().epoch;
    pool.setDiagnosticsEnabled(false);
    pool.setDiagnosticsEnabled(true);
    CHECK(pool.diagnostics().epoch != old_epoch);
    U64 value = 99;
    CHECK(pool.result(old, value) == ALQueryPool::Status::Expired);
    CHECK(pool.diagnostics().untracked_polls == 1);
    CHECK(pool.diagnostics().expired_polls == 0);
    CHECK(pool.diagnostics().expired_unique == 0);
    REQUIRE(pool.beginVisibilityCapture(true));
    const auto current = closed_ticket(pool);
    REQUIRE(current.isValid());
    pool.endVisibilityCapture();
    pool.release();
    CHECK(pool.diagnostics().acquired == 1);
    CHECK(pool.diagnostics().retired == 1);
    CHECK(pool.diagnostics().retired_without_terminal_observation == 1);
}
