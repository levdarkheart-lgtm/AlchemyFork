// Copyright (c) 2026, Alchemy Viewer Project.
// SPDX-License-Identifier: LGPL-2.1-or-later

/** @file alprobereadiness_test.cpp
 * Bounded readiness and exact, non-retaining population identity tests.
 */
#include "linden_common.h"
#include <catch2/catch_test_macros.hpp>
#include "../alprobereadiness.h"

namespace
{
ALProbeReadiness::Snapshot ready_snapshot(std::uint64_t frame)
{
    ALProbeReadiness::Snapshot s;
    s.frame_index = frame;
    s.enabled = s.layout_ready = s.default_ready = s.native_pairs_required = true;
    s.budget = s.dynamic_count = 4;
    s.resolution = 128;
    s.assigned = s.member_count = s.main_visible = 2;
    s.members[0] = {101, 0, 3, true};
    s.members[1] = {202, 2, 7, true};
    return s;
}

TEST_CASE("Probe readiness publication is explicit and exact-frame only", "[ALProbeReadiness]")
{
    ALProbeReadiness::requestObservation(false);
    CHECK_FALSE(ALProbeReadiness::observationRequested());
    auto s = ready_snapshot(1);
    ALProbeReadiness::publish(7, s);
    CHECK_FALSE(ALProbeReadiness::observe(7));
    ALProbeReadiness::requestObservation(true);
    CHECK_FALSE(ALProbeReadiness::observe(7));
    ALProbeReadiness::publish(7, s);
    s.members[0].owner = 999;
    auto observed = ALProbeReadiness::observe(7);
    REQUIRE(observed);
    CHECK(observed->frame_index == 7);
    CHECK(observed->members[0].owner == 101);
    CHECK_FALSE(ALProbeReadiness::observe(6));
    CHECK_FALSE(ALProbeReadiness::observe(8));
    ALProbeReadiness::requestObservation(true); // Session rearm drops old data.
    CHECK_FALSE(ALProbeReadiness::observe(7));
    ALProbeReadiness::requestObservation(false);
}

TEST_CASE("Probe readiness needs 120 consecutive stable snapshots and permits refresh", "[ALProbeReadiness]")
{
    ALProbeReadiness::Gate gate;
    for (std::uint64_t i = 1; i <= 120; ++i)
    {
        auto s = ready_snapshot(i);
        s.refreshing = i % 2;
        s.radiance_pass = i % 3;
        s.updating_face = i % 6;
        const auto status = gate.observe(i, &s);
        CHECK(status.ready);
        CHECK(status.consecutive_ready_frames == i);
        CHECK(status.admitted() == (i == 120));
    }
    auto s = ready_snapshot(121);
    CHECK(gate.observe(121, &s).consecutive_ready_frames == 120);
    // Only assigned slots are required; the other budget slots are unassigned.
    CHECK(gate.status().admitted());
}

TEST_CASE("Probe readiness rejects every initialization and publication blocker", "[ALProbeReadiness]")
{
    for (unsigned variant = 0; variant < 17; ++variant)
    {
        ALProbeReadiness::Gate gate;
        auto s = ready_snapshot(1);
        switch (variant)
        {
        case 0: s.enabled = false; break;
        case 1: s.layout_ready = false; break;
        case 2: s.reset = true; break;
        case 3: s.paused = true; break;
        case 4: s.reset_fade_pending = true; break;
        case 5: s.default_ready = false; break;
        case 6: s.updating_incomplete = true; break;
        case 7: s.dynamic_count = 3; break;
        case 8: s.resolution = 0; break;
        case 9: s.queued_create = 1; break;
        case 10: s.queued_kill = 1; break;
        case 11: s.incomplete = 1; break;
        case 12: s.fading = 1; break;
        case 13: s.unpublished_pairs = 1; break;
        case 14: s.members[1].generation = 0; break;
        case 15: s.members[0].slot = 1; break;
        case 16: s.invalid = true; break;
        }
        INFO("blocker " << variant);
        CHECK_FALSE(gate.observe(1, &s).admitted());
        CHECK_FALSE(gate.status().ready);
        s = ready_snapshot(2);
        CHECK(gate.observe(2, &s).consecutive_ready_frames == 1);
    }
    auto external = ready_snapshot(1);
    external.native_pairs_required = false;
    external.members[0].generation = external.members[1].generation = 0;
    ALProbeReadiness::Gate external_gate;
    CHECK(external_gate.observe(1, &external).ready);
}

TEST_CASE("Probe readiness fails closed on stale malformed or missing membership", "[ALProbeReadiness]")
{
    for (unsigned variant = 0; variant < 10; ++variant)
    {
        ALProbeReadiness::Gate gate;
        auto s = ready_snapshot(1);
        REQUIRE(gate.observe(1, &s).ready);
        s = ready_snapshot(2);
        switch (variant)
        {
        case 0: s.frame_index = 1; break;
        case 1: s.assigned = s.member_count = 257; break;
        case 2: s.member_count = 1; break;
        case 3: s.members[1].slot = 0; break;
        case 4: s.members[1].slot = 4; break;
        case 5: s.members[1].owner = s.members[0].owner; break;
        case 6: s.members[1].owner = 0; break;
        case 7: s.main_visible = 1; break;
        case 8: s.incomplete = 3; break;
        case 9: s.budget = 257; break;
        }
        INFO("malformed " << variant);
        const auto status = gate.observe(2, &s);
        CHECK_FALSE(status.valid);
        CHECK_FALSE(status.admitted());
        s = ready_snapshot(3);
        CHECK(gate.observe(3, &s).consecutive_ready_frames == 1);
    }
    ALProbeReadiness::Gate gate;
    auto s = ready_snapshot(1);
    gate.observe(1, &s);
    CHECK_FALSE(gate.observe(2, nullptr).fresh);
    s = ready_snapshot(3);
    CHECK(gate.observe(3, &s).consecutive_ready_frames == 1);
}

TEST_CASE("Probe readiness restarts on exact owner slot generation visibility or layout changes", "[ALProbeReadiness]")
{
    for (unsigned variant = 0; variant < 6; ++variant)
    {
        ALProbeReadiness::Gate gate;
        for (std::uint64_t i = 1; i <= 120; ++i)
        {
            auto s = ready_snapshot(i);
            gate.observe(i, &s);
        }
        REQUIRE(gate.status().admitted());
        auto s = ready_snapshot(121);
        switch (variant)
        {
        case 0: s.members[1].owner = 303; break;
        case 1: s.members[1].slot = 3; break;
        case 2: ++s.members[1].generation; break;
        case 3: s.members[1].main_visible = false; --s.main_visible; break;
        case 4: s.resolution = 256; break;
        case 5: s.budget = s.dynamic_count = 5; break;
        }
        const auto status = gate.observe(121, &s);
        CHECK(status.ready);
        CHECK(status.membership_changed);
        CHECK(status.consecutive_ready_frames == 1);
        CHECK_FALSE(status.admitted());
    }
    ALProbeReadiness::Gate gap;
    auto s = ready_snapshot(1);
    gap.observe(1, &s);
    s = ready_snapshot(3);
    CHECK(gap.observe(3, &s).consecutive_ready_frames == 1);
    gap.reset();
    CHECK_FALSE(gap.status().fresh);
}
}
