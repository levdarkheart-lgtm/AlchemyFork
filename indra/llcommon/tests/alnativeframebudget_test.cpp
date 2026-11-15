/**
 * @file alnativeframebudget_test.cpp
 * @brief Shared streaming/probe admission and actual owner-retirement contracts.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "../alnativeframebudget.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
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
Budget::Limits smallScene()
{
    auto limits = small();
    limits.scene_publications_per_frame = 2;
    limits.scene_publications_in_flight = 2;
    limits.scene_bytes_per_frame = 16;
    limits.scene_bytes_in_flight = 32;
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

TEST_CASE("Native source publication credits retain independent scene debt through final retirement",
          "[native-frame-budget][scene-publication]")
{
    Budget budget(smallScene());budget.beginFrame(frame(1));
    auto first = budget.tryAcquire(Budget::Work::scene_publication, 8, 1);
    auto upload = budget.tryAcquire(Budget::Work::upload, 4);
    auto probe = budget.tryAcquire(Budget::Work::ordinary_probe);
    auto preparation = budget.tryAcquire(Budget::Work::preparation);
    REQUIRE(first);REQUIRE(upload);REQUIRE(probe);REQUIRE(preparation);
    CHECK(first.credit->work() == Budget::Work::scene_publication);
    CHECK(first.credit->logicalBytes() == 8);
    auto completion_owner = first.credit;first.credit.reset();
    auto second = budget.tryAcquire(Budget::Work::scene_publication, 8, 1);REQUIRE(second);
    auto state = budget.snapshot();
    CHECK(state.scene_publications_in_flight == 2);CHECK(state.scene_bytes_in_flight == 16);
    CHECK(state.scene_publications_this_frame == 2);CHECK(state.scene_bytes_this_frame == 16);
    CHECK(state.uploads_in_flight == 1);CHECK(state.upload_bytes_in_flight == 4);
    CHECK(state.uploads_this_frame == 1);CHECK(state.upload_bytes_this_frame == 4);
    CHECK(state.optional_views_in_flight == 1);CHECK(state.optional_views_this_frame == 1);
    CHECK(state.preparation_in_flight == 1);CHECK(state.streaming.pending_publications == 0);
    second.credit.reset();
    CHECK(budget.snapshot().scene_publications_in_flight == 1);
    CHECK(budget.snapshot().scene_bytes_in_flight == 8);
    CHECK(budget.tryAcquire(Budget::Work::scene_publication, 1, 1).reason == Budget::Reason::frame_work);
    budget.beginFrame(frame(2));state = budget.snapshot();
    CHECK(state.scene_publications_this_frame == 0);CHECK(state.scene_bytes_this_frame == 0);
    CHECK(state.scene_publications_in_flight == 1);CHECK(state.scene_bytes_in_flight == 8);
    auto replacement = budget.tryAcquire(Budget::Work::scene_publication, 16, 2);REQUIRE(replacement);
    CHECK(budget.snapshot().scene_bytes_in_flight == 24);
    completion_owner.reset();
    CHECK(budget.snapshot().scene_bytes_in_flight == 16);
    CHECK(budget.snapshot().uploads_in_flight == 1);CHECK(budget.snapshot().optional_views_in_flight == 1);
    replacement.credit.reset();state = budget.snapshot();
    CHECK(state.scene_publications_in_flight == 0);CHECK(state.scene_bytes_in_flight == 0);
    CHECK(state.scene_publications_this_frame == 1);CHECK(state.scene_bytes_this_frame == 16);
}

TEST_CASE("Native source publication enforces separate frame and active byte ceilings",
          "[native-frame-budget][scene-publication]")
{
    auto limits = smallScene();limits.scene_publications_per_frame = limits.scene_publications_in_flight = 8;
    limits.scene_bytes_in_flight = 24;Budget budget(limits);budget.beginFrame(frame(1));
    auto first = budget.tryAcquire(Budget::Work::scene_publication, 10);REQUIRE(first);
    CHECK(budget.tryAcquire(Budget::Work::scene_publication, 7).reason == Budget::Reason::frame_work);
    auto second = budget.tryAcquire(Budget::Work::scene_publication, 6);REQUIRE(second);
    CHECK(budget.snapshot().scene_bytes_this_frame == 16);
    budget.beginFrame(frame(2));
    CHECK(budget.tryAcquire(Budget::Work::scene_publication, 9).reason == Budget::Reason::in_flight);
    auto third = budget.tryAcquire(Budget::Work::scene_publication, 8);REQUIRE(third);
    CHECK(budget.snapshot().scene_bytes_in_flight == 24);
    CHECK(budget.snapshot().uploads_in_flight == 0);CHECK(budget.snapshot().upload_bytes_in_flight == 0);
    CHECK(budget.decodeCeiling(2, 4, 6) == 2); // actual scene bytes, no fabricated image upload.
    auto upload = budget.tryAcquire(Budget::Work::upload, 8);REQUIRE(upload);
    first.credit.reset();second.credit.reset();
    CHECK(budget.snapshot().scene_bytes_in_flight == 8);
    auto fourth = budget.tryAcquire(Budget::Work::scene_publication, 8);REQUIRE(fourth);
    CHECK(budget.snapshot().scene_bytes_this_frame == 16);
    CHECK(budget.snapshot().scene_bytes_in_flight == 16);
    CHECK(budget.snapshot().upload_bytes_in_flight == 8);
}

TEST_CASE("Aged dirty source work progresses during busy or pressured frames without borrowing other lanes",
          "[native-frame-budget][scene-publication]")
{
    Budget budget(smallScene());auto busy = frame(10);busy.required_main_cpu_ns = 45'000'000;
    budget.beginFrame(busy);
    CHECK(budget.tryAcquire(Budget::Work::scene_publication, 8, 10).reason == Budget::Reason::frame_work);
    auto source = budget.tryAcquire(Budget::Work::scene_publication, 8, 7);REQUIRE(source);
    CHECK(source.reason == Budget::Reason::progress);
    CHECK(budget.tryAcquire(Budget::Work::scene_publication, 8, 7).reason == Budget::Reason::frame_work);
    auto upload = budget.tryAcquire(Budget::Work::upload, 4, 10);
    auto ordinary = budget.tryAcquire(Budget::Work::ordinary_probe, 0, 7);
    auto hero = budget.tryAcquire(Budget::Work::hero_probe, 0, 7);
    REQUIRE(upload);REQUIRE(ordinary);REQUIRE(hero);
    CHECK(budget.snapshot().uploads_this_frame == 1);CHECK(budget.snapshot().optional_views_this_frame == 2);
    source.credit.reset();
    CHECK_FALSE(budget.tryAcquire(Budget::Work::scene_publication, 1, 7)); // one progress batch per frame, even after abandon.
    auto pressured = frame(11);pressured.os_available_bytes = 0;budget.beginFrame(pressured);
    CHECK(budget.tryAcquire(Budget::Work::scene_publication, 8, 11).reason == Budget::Reason::memory_pressure);
    auto aged = budget.tryAcquire(Budget::Work::scene_publication, 8, 8);REQUIRE(aged);
    CHECK(aged.reason == Budget::Reason::progress);CHECK(budget.snapshot().scene_publications_this_frame == 1);
    budget.beginFrame(frame(12));
    auto fresh = budget.tryAcquire(Budget::Work::scene_publication, 8, 12);REQUIRE(fresh);
    CHECK(fresh.reason == Budget::Reason::admitted);
    CHECK(budget.tryAcquire(Budget::Work::scene_publication, 1, 0).reason == Budget::Reason::in_flight);
    CHECK(budget.snapshot().scene_bytes_in_flight == 16);
}

TEST_CASE("An oversized source batch progresses alone but can never exceed the hard scene metadata ceiling",
          "[native-frame-budget][scene-publication]")
{
    auto limits = smallScene();limits.scene_bytes_per_frame = 8;Budget budget(limits);budget.beginFrame(frame(1));
    auto small = budget.tryAcquire(Budget::Work::scene_publication, 4);REQUIRE(small);
    CHECK_FALSE(budget.tryAcquire(Budget::Work::scene_publication, 24));
    small.credit.reset();CHECK_FALSE(budget.tryAcquire(Budget::Work::scene_publication, 24));
    budget.beginFrame(frame(2));
    auto oversized = budget.tryAcquire(Budget::Work::scene_publication, 24);REQUIRE(oversized);
    CHECK(oversized.reason == Budget::Reason::progress);
    CHECK(budget.snapshot().scene_bytes_this_frame == 24);CHECK(budget.snapshot().scene_bytes_in_flight == 24);
    CHECK(budget.tryAcquire(Budget::Work::scene_publication, 1).reason == Budget::Reason::in_flight);
    budget.beginFrame(frame(3));
    CHECK(budget.tryAcquire(Budget::Work::scene_publication, 1).reason == Budget::Reason::in_flight);
    auto queued_owner = oversized.credit;oversized.credit.reset();
    CHECK_FALSE(budget.tryAcquire(Budget::Work::scene_publication, 1));
    auto upload = budget.tryAcquire(Budget::Work::upload, 4);REQUIRE(upload);
    auto probe = budget.tryAcquire(Budget::Work::ordinary_probe);REQUIRE(probe);
    queued_owner.reset();
    CHECK(budget.snapshot().scene_bytes_in_flight == 0);
    CHECK(budget.tryAcquire(Budget::Work::scene_publication, 33).reason == Budget::Reason::in_flight);
    auto recovered = budget.tryAcquire(Budget::Work::scene_publication, 8);REQUIRE(recovered);
    CHECK(budget.snapshot().upload_bytes_in_flight == 4);CHECK(budget.snapshot().optional_views_in_flight == 1);
}

TEST_CASE("Source publication startup bounds checked sums and stale frame plans cannot wrap or refill debt",
          "[native-frame-budget][scene-publication]")
{
    Budget startup(smallScene());
    auto first = startup.tryAcquire(Budget::Work::scene_publication, 8);REQUIRE(first);
    auto second = startup.tryAcquire(Budget::Work::scene_publication, 8);REQUIRE(second);
    CHECK(startup.decodeCeiling(2, 4, 6) == 2);
    CHECK_FALSE(startup.tryAcquire(Budget::Work::scene_publication, 1));
    first.credit.reset();second.credit.reset();
    CHECK(startup.decodeCeiling(2, 4, 6) == 6);
    CHECK(startup.tryAcquire(Budget::Work::scene_publication, 1).reason == Budget::Reason::frame_work);
    startup.beginFrame(frame(3));
    {auto source = startup.tryAcquire(Budget::Work::scene_publication, 8);REQUIRE(source);}
    {auto source = startup.tryAcquire(Budget::Work::scene_publication, 8);REQUIRE(source);}
    startup.beginFrame(frame(3));startup.beginFrame(frame(2));
    CHECK(startup.snapshot().frame == 3);CHECK(startup.snapshot().scene_bytes_this_frame == 16);
    CHECK_FALSE(startup.tryAcquire(Budget::Work::scene_publication, 1));
    CHECK(startup.decodeCeiling(2, 4, 6) == 6); // retired work is no longer decoder debt.

    auto limits = smallScene();limits.scene_bytes_per_frame = limits.scene_bytes_in_flight = Budget::unknown_bytes;
    Budget checked(limits);auto maximum = checked.tryAcquire(Budget::Work::scene_publication, Budget::unknown_bytes);REQUIRE(maximum);
    CHECK(checked.snapshot().scene_bytes_in_flight == Budget::unknown_bytes);
    CHECK(checked.decodeCeiling(2, 4, 6) == 2);
    CHECK_FALSE(checked.tryAcquire(Budget::Work::scene_publication, 1));
    maximum.credit.reset();CHECK(checked.snapshot().scene_bytes_in_flight == 0);
    CHECK(checked.decodeCeiling(2, 4, 6) == 6);
    CHECK(checked.snapshot().scene_bytes_this_frame == Budget::unknown_bytes);
    CHECK(checked.tryAcquire(Budget::Work::scene_publication, 1).reason == Budget::Reason::frame_work);
    checked.beginFrame(frame(1));CHECK(checked.tryAcquire(Budget::Work::scene_publication, 1));
}

TEST_CASE("Actual source credit debt reduces decoder starts without crossing the finite floor",
          "[native-frame-budget][scene-publication]")
{
    Budget budget;budget.beginFrame(frame(1));std::vector<Budget::Lease> sources;
    CHECK(budget.decodeCeiling(2, 4, 6) == 6);
    sources.push_back(budget.tryAcquire(Budget::Work::scene_publication, 1).credit);REQUIRE(sources.back());
    CHECK(budget.decodeCeiling(2, 4, 6) == 5);
    auto preparation = budget.tryAcquire(Budget::Work::preparation);REQUIRE(preparation);
    CHECK(budget.decodeCeiling(2, 4, 6) == 4);
    for(unsigned i = 0; i < 3; ++i)
    {sources.push_back(budget.tryAcquire(Budget::Work::scene_publication, 1).credit);REQUIRE(sources.back());}
    CHECK(budget.decodeCeiling(2, 4, 6) == 2);CHECK(budget.decodeCeiling(0, 4, 3) == 1);
    CHECK(budget.decodeCeiling(7, 4, 3) == 7);
    auto state = budget.snapshot();CHECK(state.uploads_in_flight == 0);CHECK(state.upload_bytes_in_flight == 0);
    CHECK(state.streaming.pending_publications == 0);CHECK(state.optional_views_in_flight == 0);
    sources.clear();CHECK(budget.decodeCeiling(2, 4, 6) == 5);
    preparation.credit.reset();CHECK(budget.decodeCeiling(2, 4, 6) == 6);
    budget.beginFrame(frame(2));
    auto large = budget.tryAcquire(Budget::Work::scene_publication, Budget::Limits{}.scene_bytes_in_flight);REQUIRE(large);
    CHECK(budget.snapshot().scene_publications_in_flight == 1);CHECK(budget.decodeCeiling(2, 4, 6) == 2);
    large.credit.reset();CHECK(budget.decodeCeiling(2, 4, 6) == 6);
}

TEST_CASE("Scene credits survive policy destruction and draining admits no new source publication",
          "[native-frame-budget][scene-publication]")
{
    Budget::Lease command_owner;
    {
        Budget budget(smallScene());budget.beginFrame(frame(1));
        auto source = budget.tryAcquire(Budget::Work::scene_publication, 8);REQUIRE(source);
        command_owner = source.credit;source.credit.reset();budget.beginDrain();
        CHECK(budget.tryAcquire(Budget::Work::scene_publication, 1).reason == Budget::Reason::draining);
        budget.beginFrame(frame(2));CHECK_FALSE(budget.tryAcquire(Budget::Work::scene_publication, 1));
        CHECK(budget.snapshot().scene_bytes_in_flight == 8);
        CHECK(budget.tryAcquire(Budget::Work::upload, 1)); // existing shutdown upload behavior is retained.
        CHECK(budget.decodeCeiling(2, 4, 6) == 6);
    }
    command_owner.reset();
    SUCCEED("Actual command credit retires without borrowing the destroyed policy owner");
}

TEST_CASE("Optional cohort stability requires distinct outer frames without consuming credits",
          "[native-frame-budget][cohort-compilation][cohort-stability]")
{
    using Owner=ALNativeCohortCompilationAdmission;
    Budget budget(smallScene());Owner owner;const Owner::ProducerStamp producer{17,1};
    budget.beginFrame(frame(0));const auto untouched=budget.snapshot();
    CHECK_FALSE(owner.deferUnstableRevision(0)); // default remains immediate
    REQUIRE(owner.deferUnstableRevision(0,producer));CHECK(owner.pendingSince()==0);
    CHECK(owner.lastConsideredFrame()==0);CHECK_FALSE(owner.lastAdmittedFrame());
    for(unsigned view=0;view<4;++view)CHECK(owner.deferUnstableRevision(0,producer));
    CHECK_FALSE(owner.tryAcquire(budget,8));
    CHECK(budget.snapshot().admitted==untouched.admitted);CHECK(budget.snapshot().deferred==untouched.deferred);
    CHECK(budget.snapshot().scene_publications_this_frame==0);
    budget.beginFrame(frame(1));REQUIRE_FALSE(owner.deferUnstableRevision(1,producer));
    {
        auto credit=owner.tryAcquire(budget,8);REQUIRE(credit);CHECK(owner.lastAdmittedFrame()==1);
        CHECK(owner.deferUnstableRevision(1,producer));CHECK_FALSE(owner.tryAcquire(budget,8));
        owner.publicationCompleted();CHECK_FALSE(owner.pending());
    }
    CHECK(budget.snapshot().scene_publications_in_flight==0);
    // Cache-only loss does not change the producer stamp or require two more
    // observations. A skipped outer frame is distinct; adjacency is not needed.
    budget.beginFrame(frame(8));REQUIRE_FALSE(owner.deferUnstableRevision(8,producer));
    auto reused=owner.tryAcquire(budget,8);REQUIRE(reused);CHECK(owner.pendingSince()==8);
    CHECK(owner.lastConsideredFrame()==8);CHECK(owner.deferUnstableRevision(7,Owner::ProducerStamp{99,1}));
    CHECK(owner.lastConsideredFrame()==8);CHECK(owner.pendingSince()==8);
    owner.publicationCompleted();CHECK_FALSE(owner.pending());
    CHECK(owner.deferUnstableRevision(7,Owner::ProducerStamp{99,1}));
    CHECK_FALSE(owner.pending());CHECK(owner.lastConsideredFrame()==8); // PPD239: no false old pending age.
}

TEST_CASE("Cohort stability resets for a new revision or publisher and ignores stale observations",
          "[native-frame-budget][cohort-compilation][cohort-stability]")
{
    using Owner=ALNativeCohortCompilationAdmission;
    Budget budget(smallScene());Owner owner;
    budget.beginFrame(frame(0));CHECK(owner.deferUnstableRevision(0,Owner::ProducerStamp{41,1}));
    budget.beginFrame(frame(1));CHECK(owner.deferUnstableRevision(1,Owner::ProducerStamp{41,2}));
    CHECK(owner.pendingSince()==0);CHECK(owner.lastConsideredFrame()==1);
    CHECK(owner.deferUnstableRevision(0,Owner::ProducerStamp{41,1}));
    budget.beginFrame(frame(2));REQUIRE_FALSE(owner.deferUnstableRevision(2,Owner::ProducerStamp{41,2}));
    // Observation alone does not grant service or move consideration. A
    // regressing observation cannot replace this newer stamp's evidence.
    CHECK(owner.deferUnstableRevision(1,Owner::ProducerStamp{41,3}));
    CHECK(owner.lastConsideredFrame()==1);CHECK_FALSE(owner.lastAdmittedFrame());
    REQUIRE_FALSE(owner.deferUnstableRevision(2,Owner::ProducerStamp{41,2}));
    {auto granted=owner.tryAcquire(budget,8);REQUIRE(granted);}
    budget.beginFrame(frame(3));CHECK(owner.deferUnstableRevision(3,Owner::ProducerStamp{42,2}));
    CHECK(owner.pendingSince()==0);CHECK(owner.lastAdmittedFrame()==2);
    budget.beginFrame(frame(4));REQUIRE_FALSE(owner.deferUnstableRevision(4,Owner::ProducerStamp{42,2}));
    {auto granted=owner.tryAcquire(budget,8);REQUIRE(granted);owner.publicationCompleted();}
    budget.beginFrame(frame(5));CHECK(owner.deferUnstableRevision(5,Owner::ProducerStamp{0,2}));
    CHECK(budget.snapshot().scene_publications_this_frame==0);
    budget.beginFrame(frame(6));REQUIRE_FALSE(owner.deferUnstableRevision(6,Owner::ProducerStamp{42,2}));
    budget.beginDrain();CHECK_FALSE(owner.tryAcquire(budget,8));CHECK(owner.pendingSince()==5);
    CHECK(budget.snapshot().scene_publications_in_flight==0);
}

TEST_CASE("Stable failed CPU cohorts retain fairness with later page and bank publication",
          "[native-frame-budget][cohort-compilation][cohort-stability][PPD-214]")
{
    using Owner=ALNativeCohortCompilationAdmission;
    for(bool pressure:{false,true})
    {
        CAPTURE(pressure);Budget budget(smallScene());std::array<Owner,65> cpu;
        bool pages=false,commands=false;std::optional<Budget::Frame> command_pending;
        Budget::Lease page_completion,command_completion;unsigned services=0,unstable=0;
        for(Budget::Frame id=0;id<24;++id)
        {
            auto input=frame(id);if(pressure)input.os_available_bytes=0;else input.required_main_cpu_ns=45'000'000;
            budget.beginFrame(input);std::array<unsigned,65> order;
            for(unsigned i=0;i<order.size();++i){order[i]=i;cpu[i].notePending(id);}
            std::sort(order.begin(),order.end(),[&](unsigned a,unsigned b){return cpu[a].priority(a)<cpu[b].priority(b);});
            for(unsigned i:order)
            {
                // Half the optional owners never stabilize. They must not
                // reserve a busy progress turn or manufacture budget denials.
                const Owner::ProducerStamp stamp{U64(i)+1,i%2?id+1:1};
                const auto before=budget.snapshot();
                if(cpu[i].deferUnstableRevision(id,stamp))
                {
                    ++unstable;CHECK(budget.snapshot().admitted==before.admitted);
                    CHECK(budget.snapshot().deferred==before.deferred);continue;
                }
                auto admitted=cpu[i].tryAcquire(budget,8);if(admitted){++services;CHECK(cpu[i].pendingSince()==0);}
                // No completed publication: local credit retires on failure,
                // preserving original pending age and last-service fairness.
            }
            if(!pages)
            {
                auto credit=budget.tryAcquire(Budget::Work::scene_publication,8,0);
                if(credit){pages=true;page_completion=credit.credit;command_pending=id;}
            }
            if(pages&&!commands)
            {
                auto credit=budget.tryAcquire(Budget::Work::scene_publication,8,*command_pending);
                if(credit){commands=true;command_completion=credit.credit;}
            }
            if(commands){page_completion.reset();command_completion.reset();}
            CHECK(budget.snapshot().scene_publications_this_frame<=1);
            CHECK(budget.snapshot().scene_publications_in_flight<=2);
        }
        REQUIRE(pages);REQUIRE(commands);CHECK(services>0);CHECK(unstable>24);
        CHECK(budget.snapshot().scene_publications_in_flight==0);
    }
}

TEST_CASE("Cohort compilation is considered once across main and probe and owns no idle credit",
          "[native-frame-budget][cohort-compilation]")
{
    Budget budget(smallScene());budget.beginFrame(frame(0));
    ALNativeCohortCompilationAdmission owner;
    owner.notePending(0);
    REQUIRE(owner.pendingSince());CHECK(*owner.pendingSince() == 0);
    {
        auto main = owner.tryAcquire(budget, 8);REQUIRE(main);
        CHECK(main.credit->work() == Budget::Work::scene_publication);
        CHECK(budget.snapshot().scene_bytes_in_flight == 8);
        // A failed compile retires its local CPU credit, not its pending age.
    }
    CHECK(budget.snapshot().scene_publications_in_flight == 0);
    CHECK(owner.pending());CHECK(owner.lastAdmittedFrame() == 0);
    const auto after_main = budget.snapshot();
    CHECK_FALSE(owner.tryAcquire(budget, 8)); // ordinary/hero probe, same outer frame
    CHECK(budget.snapshot().admitted == after_main.admitted);
    CHECK(budget.snapshot().deferred == after_main.deferred);
    CHECK(budget.snapshot().scene_publications_this_frame == 1);

    budget.beginFrame(frame(1));owner.notePending(1); // another dirty revision
    CHECK(owner.pendingSince() == 0);
    {
        auto retry = owner.tryAcquire(budget, 8);REQUIRE(retry);
        owner.publicationCompleted();
        CHECK_FALSE(owner.pending());
        CHECK(budget.snapshot().scene_publications_in_flight == 1);
    }
    CHECK(budget.snapshot().scene_publications_in_flight == 0);
    CHECK(owner.lastAdmittedFrame() == 1);
    CHECK(owner.lastConsideredFrame() == 1);
    // Caller warm lookup does not enqueue or request admission at all.
    for (Budget::Frame id = 2; id <= 4; ++id)
    {
        budget.beginFrame(frame(id));
        if (owner.pending()) FAIL("Completed warm owner must not request compilation");
        CHECK(budget.snapshot().scene_publications_this_frame == 0);
        CHECK(budget.snapshot().admitted == after_main.admitted + 1);
    }
    owner.notePending(4);
    CHECK(owner.pendingSince() == 4);
    { auto changed = owner.tryAcquire(budget, 8);REQUIRE(changed);owner.publicationCompleted(); }
    // Even successful publication followed by another same-frame change is
    // not a second compilation opportunity for the other view.
    owner.notePending(4);
    CHECK_FALSE(owner.tryAcquire(budget, 8));
    CHECK(budget.snapshot().scene_publications_this_frame == 1);
}

TEST_CASE("Failed cohort publication requeues behind older unserved owners with stable ties",
          "[native-frame-budget][cohort-compilation]")
{
    auto limits = smallScene();limits.scene_publications_per_frame = 1;
    Budget budget(limits);
    std::array<ALNativeCohortCompilationAdmission, 3> owners;
    const std::array<std::uint64_t, 3> identities{30, 10, 20};
    for (auto& owner : owners) owner.notePending(1);
    const std::array<std::size_t, 6> expected{1, 2, 0, 1, 2, 0};
    for (std::size_t iteration = 0; iteration < expected.size(); ++iteration)
    {
        budget.beginFrame(frame(10 + iteration));
        std::array<std::size_t, 3> order{0, 1, 2};
        std::sort(order.begin(), order.end(), [&](auto a, auto b)
        { return owners[a].priority(identities[a]) < owners[b].priority(identities[b]); });
        CHECK(order.front() == expected[iteration]);
        unsigned admitted = 0;
        for (const auto index : order)
        {
            const auto old_service = owners[index].lastAdmittedFrame();
            auto decision = owners[index].tryAcquire(budget, 8);
            if (decision)
            {
                ++admitted;CHECK(index == expected[iteration]);
                CHECK(owners[index].lastAdmittedFrame() == 10 + iteration);
                // Deliberately fail every publication. Never clear pending.
            }
            else CHECK(owners[index].lastAdmittedFrame() == old_service);
        }
        CHECK(admitted == 1);
        CHECK(budget.snapshot().scene_publications_this_frame == 1);
        CHECK(budget.snapshot().scene_publications_in_flight == 0);
        for (const auto& owner : owners) CHECK(owner.pendingSince() == 1);
    }
}

TEST_CASE("Aged cohort owners use the existing pressure progress lane without starvation by failed service",
          "[native-frame-budget][cohort-compilation]")
{
    Budget budget(smallScene());
    std::array<ALNativeCohortCompilationAdmission, 2> owners;
    owners[0].notePending(1);owners[1].notePending(2);
    for (Budget::Frame id = 3; id <= 5; ++id)
    {
        auto input = frame(id);input.required_main_cpu_ns = 45'000'000;
        budget.beginFrame(input);
        std::array<std::size_t, 2> order{0, 1};
        std::sort(order.begin(), order.end(), [&](auto a, auto b)
        { return owners[a].priority(a) < owners[b].priority(b); });
        unsigned admitted = 0;
        for (const auto index : order)
        {
            auto decision = owners[index].tryAcquire(budget, 8);
            if (!decision) continue;
            ++admitted;CHECK(decision.reason == Budget::Reason::progress);
            CHECK(index == (id == 4 ? 0 : 1));
        }
        CHECK(admitted == (id == 3 ? 0 : 1));
    }
    CHECK(owners[0].pendingSince() == 1);CHECK(owners[1].pendingSince() == 2);
    CHECK(owners[0].lastAdmittedFrame() == 4);CHECK(owners[1].lastAdmittedFrame() == 5);
}

TEST_CASE("Hard cohort denial leaves service priority intact and cannot retry in a later view",
          "[native-frame-budget][cohort-compilation]")
{
    Budget budget(smallScene());budget.beginFrame(frame(5));
    ALNativeCohortCompilationAdmission large, other;
    large.notePending(1);other.notePending(2);
    auto rejected = large.tryAcquire(budget, 33);
    CHECK_FALSE(rejected);CHECK(rejected.reason == Budget::Reason::in_flight);
    CHECK_FALSE(large.lastAdmittedFrame());
    CHECK(large.priority(4).unserved_since == 1);
    CHECK(budget.snapshot().scene_publications_this_frame == 0);
    { auto unaffected = other.tryAcquire(budget, 8);REQUIRE(unaffected); }
    CHECK_FALSE(large.tryAcquire(budget, 8));
    CHECK_FALSE(large.lastAdmittedFrame());
    large.notePending(5);CHECK(large.pendingSince() == 1);
    budget.beginFrame(frame(6));
    {
        auto sole_large = large.tryAcquire(budget, 24);REQUIRE(sole_large);
        CHECK(sole_large.reason == Budget::Reason::progress);
        CHECK(budget.snapshot().scene_bytes_in_flight == 24);
    }
    CHECK(large.pendingSince() == 1);CHECK(large.lastAdmittedFrame() == 6);
    CHECK(budget.snapshot().scene_bytes_in_flight == 0);
    budget.beginDrain();budget.beginFrame(frame(7));
    CHECK(large.tryAcquire(budget, 8).reason == Budget::Reason::draining);
    CHECK(large.lastAdmittedFrame() == 6);CHECK(large.pendingSince() == 1);
}

TEST_CASE("Saturated cohort work is rejected before admission and aggregate byte overflow stays bounded",
          "[native-frame-budget][cohort-compilation]")
{
    auto limits = smallScene();
    limits.scene_bytes_per_frame = limits.scene_bytes_in_flight = Budget::unknown_bytes;
    Budget budget(limits);budget.beginFrame(frame(7));
    ALNativeCohortCompilationAdmission overflow, remainder;
    overflow.notePending(1);
    CHECK_FALSE(overflow.tryAcquire(budget, Budget::unknown_bytes));
    CHECK_FALSE(overflow.lastAdmittedFrame());CHECK(overflow.pendingSince() == 1);
    CHECK(budget.snapshot().admitted == 0);CHECK(budget.snapshot().scene_bytes_in_flight == 0);
    CHECK_FALSE(overflow.tryAcquire(budget, 1));
    budget.beginFrame(frame(8));
    {
        auto maximum = overflow.tryAcquire(budget, Budget::unknown_bytes - 1);REQUIRE(maximum);
        CHECK(budget.snapshot().scene_bytes_in_flight == Budget::unknown_bytes - 1);
        CHECK(remainder.tryAcquire(budget, 2).reason == Budget::Reason::in_flight);
        CHECK_FALSE(remainder.lastAdmittedFrame());
    }
    CHECK(budget.snapshot().scene_bytes_in_flight == 0);
    CHECK(budget.snapshot().scene_bytes_this_frame == Budget::unknown_bytes - 1);
    budget.beginFrame(frame(9));
    { auto recovered = remainder.tryAcquire(budget, 2);REQUIRE(recovered); }
    CHECK(remainder.pendingSince() == 8);CHECK(remainder.lastAdmittedFrame() == 9);
}

TEST_CASE("Busy CPU compilation cannot starve later native page and command publication",
          "[native-frame-budget][cohort-compilation][scene-publication][PPD-214]")
{
    for(unsigned count:{1u,2u,65u})for(bool pressure:{false,true})for(bool dirty_again:{false,true})
    {
        CAPTURE(count,pressure,dirty_again);auto limits=smallScene();Budget budget(limits);
        std::vector<ALNativeCohortCompilationAdmission> cpu(count);
        for(auto& owner:cpu)owner.notePending(0);
        bool pages=false,commands=false;Budget::Frame page_frame=0,command_frame=0;
        std::optional<Budget::Frame> command_pending;
        Budget::Lease page_completion,command_completion;
        unsigned cpu_services=0;
        for(Budget::Frame id=1;id<=16;++id)
        {
            auto input=frame(id);
            if(pressure)input.os_available_bytes=0;else input.required_main_cpu_ns=45'000'000;
            budget.beginFrame(input);
            std::vector<unsigned> order(count);for(unsigned i=0;i<count;++i)order[i]=i;
            std::sort(order.begin(),order.end(),[&](unsigned a,unsigned b){return cpu[a].priority(a)<cpu[b].priority(b);});
            for(unsigned i:order)
            {
                auto compilation=cpu[i].tryAcquire(budget,8);
                if(!compilation)continue;
                ++cpu_services;CHECK(compilation.reason==Budget::Reason::progress);
                // Both permanent failed compiles and newly dirty successful
                // owners must share service with the later renderer phase.
                if(dirty_again){cpu[i].publicationCompleted();cpu[i].notePending(id);}
            }
            // This is the actual native callback's default category and age
            // contract: a warm page needs no ticket; its later command bank
            // keeps the first denied frame until successful admission.
            if(!pages)
            {
                auto publication=budget.tryAcquire(Budget::Work::scene_publication,8,0);
                if(publication){pages=true;page_frame=id;page_completion=publication.credit;command_pending=id;}
            }
            if(pages&&!commands)
            {
                auto publication=budget.tryAcquire(Budget::Work::scene_publication,8,*command_pending);
                if(publication){commands=true;command_frame=id;command_completion=publication.credit;command_pending.reset();}
            }
            if(commands){page_completion.reset();command_completion.reset();}
            const auto state=budget.snapshot();CHECK(state.scene_publications_this_frame<=1);
            CHECK(state.scene_publications_in_flight<=2);CHECK(state.scene_bytes_in_flight<=32);
            CHECK(state.uploads_this_frame==0);CHECK(state.optional_views_this_frame==0);
        }
        REQUIRE(pages);REQUIRE(commands);CHECK(page_frame<=limits.maximum_deferral_frames+1);
        CHECK(command_frame<=page_frame+limits.maximum_deferral_frames+1);CHECK(cpu_services>0);
        CHECK(budget.snapshot().scene_publications_in_flight==0);CHECK(budget.snapshot().scene_bytes_in_flight==0);
    }
}

TEST_CASE("Equal-age phase service cannot be reset by arbitrarily many new CPU owners",
          "[native-frame-budget][cohort-compilation][scene-publication][PPD-214]")
{
    Budget budget(smallScene());std::array<ALNativeCohortCompilationAdmission,64> cpu;
    for(auto& owner:cpu)owner.notePending(0);
    unsigned cpu_services=0,native_services=0;
    for(Budget::Frame id=10;id<42;++id)
    {
        auto input=frame(id);input.required_main_cpu_ns=45'000'000;budget.beginFrame(input);
        unsigned served=0;
        for(auto& owner:cpu)if(auto lease=owner.tryAcquire(budget,8)){++cpu_services;++served;}
        ALNativeCohortCompilationAdmission newcomer;newcomer.notePending(0);
        CHECK_FALSE(newcomer.tryAcquire(budget,8)); // No per-owner reset of phase history.
        if(auto lease=budget.tryAcquire(Budget::Work::scene_publication,8,0)){++native_services;++served;}
        CHECK(served==1);CHECK(budget.snapshot().scene_publications_in_flight==0);
        CHECK(budget.snapshot().scene_publications_this_frame==1);
    }
    CHECK(cpu_services==16);CHECK(native_services==16);CHECK(budget.snapshot().retired==32);
}

TEST_CASE("Older native command requests survive younger pages in the same late phase",
          "[native-frame-budget][cohort-compilation][scene-publication][PPD-214]")
{
    Budget budget(smallScene());ALNativeCohortCompilationAdmission cpu;cpu.notePending(0);
    const std::array<Budget::Frame,3> pending{2,1,0};
    std::array<bool,3> complete{};std::vector<unsigned> native_order;
    for(Budget::Frame id=10;id<18;++id)
    {
        auto input=frame(id);input.required_main_cpu_ns=45'000'000;budget.beginFrame(input);
        {auto failed_compilation=cpu.tryAcquire(budget,8);}
        for(unsigned context=0;context<complete.size();++context)
        {
            if(complete[context])continue;
            auto publication=budget.tryAcquire(Budget::Work::scene_publication,8,pending[context]);
            if(publication){complete[context]=true;native_order.push_back(context);}
        }
        CHECK(budget.snapshot().scene_publications_this_frame<=1);
        CHECK(budget.snapshot().scene_publications_in_flight==0);
    }
    CHECK((native_order==std::vector<unsigned>{2,1,0}));
}

TEST_CASE("Vanished publication phases and skipped frames cannot reserve progress forever",
          "[native-frame-budget][cohort-compilation][scene-publication][PPD-214]")
{
    for(bool skip_frames:{false,true})
    {
        Budget budget(smallScene());ALNativeCohortCompilationAdmission cpu;cpu.notePending(0);
        auto input=frame(10);input.required_main_cpu_ns=45'000'000;budget.beginFrame(input);
        {auto first=cpu.tryAcquire(budget,8);REQUIRE(first);}
        CHECK_FALSE(budget.tryAcquire(Budget::Work::scene_publication,8,0));
        input.frame=skip_frames?100:11;budget.beginFrame(input);
        if(skip_frames){auto recovered=cpu.tryAcquire(budget,8);REQUIRE(recovered);}
        else
        {
            CHECK_FALSE(cpu.tryAcquire(budget,8)); // One recent late-phase opportunity.
            budget.beginFrame(input);auto stale=input;stale.frame=10;budget.beginFrame(stale);
            CHECK(budget.snapshot().scene_publications_this_frame==0);
            // No metadata callback renews the reservation in frame11.
            input.frame=12;budget.beginFrame(input);auto recovered=cpu.tryAcquire(budget,8);REQUIRE(recovered);
        }
    }
    // Conversely, a single remaining default metadata caller retains its
    // established one-aged-item-per-pressure-frame behavior.
    Budget metadata(smallScene());
    for(Budget::Frame id=10;id<16;++id)
    {auto input=frame(id);input.os_available_bytes=0;metadata.beginFrame(input);
     auto only=metadata.tryAcquire(Budget::Work::scene_publication,8,0);REQUIRE(only);}
    Budget same_phase(smallScene());ALNativeCohortCompilationAdmission earlier;earlier.notePending(0);
    auto input=frame(10);input.required_main_cpu_ns=45'000'000;same_phase.beginFrame(input);
    {auto first=earlier.tryAcquire(same_phase,8);REQUIRE(first);}
    CHECK_FALSE(same_phase.tryAcquire(Budget::Work::scene_publication,8,0));
    input.frame=11;same_phase.beginFrame(input);
    CHECK_FALSE(same_phase.tryAcquire(Budget::Work::scene_publication,8,2)); // The older context has disappeared.
    input.frame=12;same_phase.beginFrame(input);
    {auto next=same_phase.tryAcquire(Budget::Work::scene_publication,8,2);REQUIRE(next);}
}

TEST_CASE("Publication hints recheck active byte capacity and never waive idle limits or draining",
          "[native-frame-budget][cohort-compilation][scene-publication][PPD-214]")
{
    Budget budget(smallScene());ALNativeCohortCompilationAdmission cpu;cpu.notePending(0);
    auto input=frame(10);input.required_main_cpu_ns=45'000'000;budget.beginFrame(input);
    {auto compilation=cpu.tryAcquire(budget,8);REQUIRE(compilation);}
    CHECK_FALSE(budget.tryAcquire(Budget::Work::scene_publication,24,0)); // Legal sole oversized hint.
    budget.recordRequiredMainWork(0); // Actual main work can clear the old busy estimate before probes.
    auto held=budget.tryAcquire(Budget::Work::scene_publication,8,10);REQUIRE(held);
    input.frame=11;budget.beginFrame(input);
    {auto fits=cpu.tryAcquire(budget,8);REQUIRE(fits);} // 24 cannot run alone while held is active.
    CHECK_FALSE(budget.tryAcquire(Budget::Work::scene_publication,24,0));
    held.credit.reset();input.frame=12;budget.beginFrame(input);
    {auto first=cpu.tryAcquire(budget,8);REQUIRE(first);}
    CHECK_FALSE(budget.tryAcquire(Budget::Work::scene_publication,24,0));
    input.frame=13;budget.beginFrame(input);CHECK_FALSE(cpu.tryAcquire(budget,8));
    {auto sole=budget.tryAcquire(Budget::Work::scene_publication,24,0);REQUIRE(sole);CHECK(sole.reason==Budget::Reason::progress);}
    CHECK(budget.snapshot().scene_bytes_in_flight==0);

    Budget idle(smallScene());ALNativeCohortCompilationAdmission idle_cpu;idle_cpu.notePending(0);
    idle.beginFrame(frame(1));auto c=idle_cpu.tryAcquire(idle,8);REQUIRE(c);
    auto n=idle.tryAcquire(Budget::Work::scene_publication,8,0);REQUIRE(n);
    CHECK_FALSE(idle.tryAcquire(Budget::Work::scene_publication,1,0));
    CHECK(idle.snapshot().scene_publications_this_frame==2);CHECK(idle.snapshot().scene_bytes_this_frame==16);
    c.credit.reset();n.credit.reset();CHECK(idle.snapshot().scene_publications_in_flight==0);
    idle.beginDrain();idle.beginFrame(frame(2));
    CHECK(idle_cpu.tryAcquire(idle,8).reason==Budget::Reason::draining);
    CHECK(idle.tryAcquire(Budget::Work::scene_publication,8,0).reason==Budget::Reason::draining);

    Budget invalid(smallScene());ALNativeCohortCompilationAdmission valid_cpu;valid_cpu.notePending(0);
    input.frame=20;invalid.beginFrame(input);{auto first=valid_cpu.tryAcquire(invalid,8);REQUIRE(first);}
    CHECK_FALSE(invalid.tryAcquire(Budget::Work::scene_publication,Budget::unknown_bytes,0));
    CHECK_FALSE(invalid.tryAcquire(Budget::Work::scene_publication,33,0));
    input.frame=21;invalid.beginFrame(input);{auto unaffected=valid_cpu.tryAcquire(invalid,8);REQUIRE(unaffected);}
}
