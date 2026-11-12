/**
 * @file alquerypool_metal_lifecycle_test.cpp
 * @brief Native visibility ticket generations, resource leases and ring reuse.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alquerypool.h"
#include <catch2/catch_test_macros.hpp>
#include "almetaldevice.h"
#include "almetalresource.h"
#include "almetalworld.h"
#include <array>
#include <memory>
#include <vector>

TEST_CASE("Released visibility tickets cannot end or discard a new capture generation",
          "[alquerypool][native-query-lifecycle][query-generation]")
{
    ALQueryPool pool;
    REQUIRE(pool.beginVisibilityCapture());
    const auto old = pool.begin(ALQueryKind::Occlusion);
    REQUIRE(old.isValid());
    pool.release();
    REQUIRE(pool.beginVisibilityCapture());
    const auto next = pool.begin(ALQueryKind::Occlusion);
    REQUIRE(next.isValid());
    CHECK_FALSE(old == next);
    pool.end(old);
    CHECK_FALSE(pool.canBegin(ALQueryKind::Occlusion));
    pool.discard(old);
    CHECK_FALSE(pool.canBegin(ALQueryKind::Occlusion));
    pool.end(next);
    CHECK(pool.canBegin(ALQueryKind::Occlusion));
    U64 value = 99;
    CHECK(pool.result(old, value) == ALQueryPool::Status::Expired);
    CHECK(value == 0);
    pool.release();
}


namespace
{
struct NativeVisibility
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::shared_ptr<ALMetalWorldFrameData> data;
    ALQueryPool pool;

    struct Batch
    {
        std::unique_ptr<ALMetalRenderTarget> target;
        std::shared_ptr<ALMetalVisibilityResultBatch> results;
    };
    NativeVisibility()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error); REQUIRE(renderer);
        data = ALMetalWorldFrameData::create(*device, &error); REQUIRE(data);
        data->beginFrame(1); renderer->setFrameData(data);
    }
    Batch encode(U32 count = 2)
    {
        std::vector<ALMetalWorldVisibilityQuery> queries(count);
        for (auto& query : queries)
        {
            for (std::size_t i = 0; i < 4; ++i) query.modelview_projection[i * 4 + i] = 1.f;
            query.center[3] = 1.f;
            query.half_extent[0] = query.half_extent[1] = query.half_extent[2] = .25f;
            query.fan = 7;
        }
        for (U32 i = 1; i < count; i += 2) queries[i].center[0] = 4.f;
        ALMetalWorldGBufferScene scene;
        scene.width = scene.height = 32;
        scene.visibility_queries = queries.data();
        scene.visibility_query_count = queries.size();
        REQUIRE(renderer->beginCommandBatch("Native visibility lifetime regression", &error));
        Batch batch;
        batch.target = renderer->render(scene, &error, &batch.results);
        INFO(error); REQUIRE(batch.target); REQUIRE(batch.results);
        REQUIRE(batch.results->resultCount() == count);
        REQUIRE(batch.results->status() == ALMetalVisibilityResultBatch::Status::pending);
        return batch;
    }
    void commit(const Batch& batch)
    {
        REQUIRE(renderer->commitCommandBatch(&error));
        std::uint64_t visible = 0;
        REQUIRE(batch.results->wait(0, visible, &error)); INFO(error);
        REQUIRE(visible == 1); // Boolean native visibility, not a synthetic count.
        std::uint64_t clipped = 99;
        REQUIRE(batch.results->result(1, clipped)); REQUIRE(clipped == 0);
    }
};
}

TEST_CASE("Released native visibility tickets cannot attach or mutate a later GPU result",
          "[alquerypool][native-query-lifecycle][query-release-gpu]")
{
    NativeVisibility f;
    REQUIRE(f.pool.beginVisibilityCapture());
    const auto old = f.pool.begin(ALQueryKind::Occlusion);
    REQUIRE(old.isValid()); f.pool.end(old);
    auto batch = f.encode();
    REQUIRE(f.pool.attachVisibilityResult(old, batch.results, 0));
    const auto submissions = ALMetalDevice::committedCommandBufferCount();
    f.pool.release();
    CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);
    REQUIRE(f.pool.beginVisibilityCapture());
    const auto next = f.pool.begin(ALQueryKind::Occlusion);
    REQUIRE(next.isValid()); CHECK_FALSE(old == next);
    bool attached = false;
    SECTION("A stale end cannot close the current capture")
    {
        f.pool.end(old);
        CHECK_FALSE(f.pool.canBegin(ALQueryKind::Occlusion));
    }
    SECTION("A stale discard cannot cancel the current capture")
    {
        f.pool.discard(old);
        CHECK_FALSE(f.pool.canBegin(ALQueryKind::Occlusion));
    }
    SECTION("A stale attachment cannot replace the current clipped result")
    {
        f.pool.end(next);
        REQUIRE(f.pool.attachVisibilityResult(next, batch.results, 1));
        attached = true;
        CHECK_FALSE(f.pool.attachVisibilityResult(old, batch.results, 0));
    }
    if (!attached)
    {
        f.pool.end(next);
        CHECK(f.pool.attachVisibilityResult(next, batch.results, 1));
    }
    f.pool.endVisibilityCapture();
    f.commit(batch);
    // The native command and query readback remain valid after their renderer
    // and local result wrappers go away; the pool owns the attached batch.
    std::weak_ptr<ALMetalVisibilityResultBatch> lease = batch.results;
    f.renderer.reset(); batch.target.reset(); batch.results.reset();
    CHECK_FALSE(lease.expired());
    U64 value = 99;
    CHECK(f.pool.result(next, value) == ALQueryPool::Status::Ready); CHECK(value == 0);
    value = 99;
    CHECK(f.pool.result(old, value) == ALQueryPool::Status::Expired); CHECK(value == 0);
    CHECK_FALSE(f.pool.attachVisibilityResult(old, lease.lock(), 0));
    f.pool.release();
    CHECK(lease.expired());
}

TEST_CASE("Native visibility results survive their history window and expire before slot reuse",
          "[alquerypool][native-query-lifecycle][query-ring-gpu]")
{
    NativeVisibility f;
    REQUIRE(f.pool.beginVisibilityCapture());
    const auto visible = f.pool.begin(ALQueryKind::Occlusion);
    REQUIRE(visible.isValid()); f.pool.end(visible);
    const auto clipped = f.pool.begin(ALQueryKind::OcclusionExact);
    REQUIRE(clipped.isValid()); f.pool.end(clipped);
    f.pool.endVisibilityCapture();
    auto batch = f.encode();
    REQUIRE(f.pool.attachVisibilityResult(visible, batch.results, 0));
    REQUIRE(f.pool.attachVisibilityResult(clipped, batch.results, 1));
    f.commit(batch);
    std::weak_ptr<ALMetalVisibilityResultBatch> lease = batch.results;
    f.renderer.reset(); batch.target.reset(); batch.results.reset();
    for (U32 age = 0; age < ALQueryPool::RESULT_FRAMES; ++age)
    {
        U64 value = 99;
        REQUIRE(f.pool.result(visible, value) == ALQueryPool::Status::Ready); CHECK(value == 1);
        REQUIRE(f.pool.result(clipped, value) == ALQueryPool::Status::Ready); CHECK(value == 0);
        CHECK_FALSE(lease.expired());
        f.pool.endFrame();
    }
    U64 value = 99;
    CHECK(f.pool.result(visible, value) == ALQueryPool::Status::Expired); CHECK(value == 0);
    value = 99;
    CHECK_FALSE(f.pool.wait(clipped, value)); CHECK(value == 0);
    CHECK(lease.expired());
    REQUIRE(f.pool.beginVisibilityCapture());
    const auto next = f.pool.begin(ALQueryKind::Occlusion);
    REQUIRE(next.isValid()); CHECK_FALSE(next == visible);
    f.pool.end(visible); f.pool.discard(visible);
    CHECK_FALSE(f.pool.canBegin(ALQueryKind::Occlusion));
    f.pool.end(next); f.pool.endVisibilityCapture();
}

TEST_CASE("Native visibility polls and waits never submit an uncommitted query command",
          "[alquerypool][native-query-lifecycle][query-wait-gpu]")
{
    NativeVisibility f;
    REQUIRE(f.pool.beginVisibilityCapture());
    const auto ticket = f.pool.begin(ALQueryKind::Occlusion);
    REQUIRE(ticket.isValid()); f.pool.end(ticket); f.pool.endVisibilityCapture();
    auto batch = f.encode();
    REQUIRE(f.pool.attachVisibilityResult(ticket, batch.results, 0));
    const auto submissions = ALMetalDevice::committedCommandBufferCount();
    U64 value = 99;
    CHECK(f.pool.result(ticket, value) == ALQueryPool::Status::Pending); CHECK(value == 0);
    CHECK(f.pool.getAskedReads() == 0);
    value = 99;
    CHECK_FALSE(f.pool.wait(ticket, value)); CHECK(value == 0);
    CHECK(f.pool.getAskedReads() == 1);
    CHECK(f.pool.getStalls() == 0);
    CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);
    f.commit(batch);
    REQUIRE(f.pool.wait(ticket, value)); CHECK(value == 1);
    CHECK(f.pool.getAskedReads() == 2);
    CHECK(f.pool.getStalls() == 0); // The explicit test oracle already waited.
}

TEST_CASE("Ending a native visibility capture discards an unfinished query without leaking its scope",
          "[alquerypool][native-query-lifecycle][query-capture]")
{
    ALQueryPool pool;
    SECTION("Explicit capture end")
    {
        REQUIRE(pool.beginVisibilityCapture());
        const auto old = pool.begin(ALQueryKind::Occlusion);
        REQUIRE(old.isValid()); pool.endVisibilityCapture();
        U64 value = 99;
        CHECK(pool.result(old, value) == ALQueryPool::Status::Expired); CHECK(value == 0);
        REQUIRE(pool.beginVisibilityCapture());
        const auto next = pool.begin(ALQueryKind::Occlusion);
        REQUIRE(next.isValid()); CHECK_FALSE(old == next);
        pool.end(old); pool.discard(old);
        CHECK_FALSE(pool.canBegin(ALQueryKind::Occlusion));
        pool.end(next); pool.endVisibilityCapture();
    }
    SECTION("Viewer frame end")
    {
        REQUIRE(pool.beginVisibilityCapture());
        const auto old = pool.begin(ALQueryKind::Occlusion);
        REQUIRE(old.isValid()); pool.endFrame();
        CHECK_FALSE(pool.visibilityCaptureActive());
        U64 value = 99;
        CHECK(pool.result(old, value) == ALQueryPool::Status::Expired); CHECK(value == 0);
        REQUIRE(pool.beginVisibilityCapture());
        const auto next = pool.begin(ALQueryKind::Occlusion);
        REQUIRE(next.isValid()); CHECK_FALSE(old == next);
        pool.end(old); pool.discard(old);
        CHECK_FALSE(pool.canBegin(ALQueryKind::Occlusion));
        pool.end(next); pool.endVisibilityCapture();
    }
}

TEST_CASE("Native visibility batches preserve each ticket kind and GPU result slot",
          "[alquerypool][native-query-lifecycle][query-batch-gpu]")
{
    NativeVisibility f;
    constexpr U32 count = 32;
    std::array<ALQueryTicket, count> tickets;
    REQUIRE(f.pool.beginVisibilityCapture());
    for (U32 i = 0; i < count; ++i)
    {
        tickets[i] = f.pool.begin(i % 2 == 0 ? ALQueryKind::Occlusion : ALQueryKind::OcclusionExact);
        REQUIRE(tickets[i].isValid());
        CHECK_FALSE(f.pool.begin(ALQueryKind::Occlusion).isValid());
        f.pool.end(tickets[i]);
        for (U32 before = 0; before < i; ++before) CHECK_FALSE(tickets[before] == tickets[i]);
    }
    f.pool.endVisibilityCapture();
    auto batch = f.encode(count);
    for (U32 i = 0; i < count; ++i)
        REQUIRE(f.pool.attachVisibilityResult(tickets[i], batch.results, i));
    CHECK_FALSE(f.pool.attachVisibilityResult(tickets[0], batch.results, count));
    CHECK(f.pool.getNameCount() == count);
    f.commit(batch);
    for (U32 i = 0; i < count; ++i)
    {
        U64 value = 99;
        REQUIRE(f.pool.result(tickets[i], value) == ALQueryPool::Status::Ready);
        CHECK(value == (i % 2 == 0 ? 1 : 0));
    }
}

TEST_CASE("Sequential shadow and main visibility captures preserve earlier tickets and isolate aborts",
          "[alquerypool][native-query-lifecycle][query-view-phases-gpu]")
{
    NativeVisibility f;
    constexpr U32 shadow_views = 4;
    constexpr U32 queries_per_view = 2;
    constexpr U32 shadow_queries = shadow_views * queries_per_view;
    constexpr U32 accepted_queries = shadow_queries + queries_per_view;
    constexpr U32 total_queries = accepted_queries + queries_per_view;
    std::array<ALQueryTicket, total_queries> tickets;
    struct DiagnosticsOffOnExit
    {
        ALQueryPool& pool;
        ~DiagnosticsOffOnExit() { pool.setDiagnosticsEnabled(false); }
    } cleanup{f.pool};
    f.pool.setDiagnosticsEnabled(true);

    // All view phases use one pool frame. A rejected nested capture must not
    // change the current phase's owner or close its open ticket.
    for (U32 view = 0; view < shadow_views; ++view)
    {
        CAPTURE(view);
        REQUIRE(f.pool.beginVisibilityCapture(false));
        for (U32 query = 0; query < queries_per_view; ++query)
        {
            const U32 index = view * queries_per_view + query;
            tickets[index] = f.pool.begin(
                query == 0 ? ALQueryKind::Occlusion : ALQueryKind::OcclusionExact);
            REQUIRE(tickets[index].isValid());
            CHECK_FALSE(f.pool.beginVisibilityCapture(true));
            CHECK_FALSE(f.pool.canBegin(ALQueryKind::Occlusion));
            f.pool.end(tickets[index]);
        }
        f.pool.endVisibilityCapture();
        CHECK_FALSE(f.pool.visibilityCaptureActive());
        CHECK(f.pool.getNameCount() == (view + 1) * queries_per_view);
        CHECK(f.pool.diagnostics().acquired == 0);
    }

    REQUIRE(f.pool.beginVisibilityCapture(true));
    for (U32 query = 0; query < queries_per_view; ++query)
    {
        const U32 index = shadow_queries + query;
        tickets[index] = f.pool.begin(ALQueryKind::Occlusion);
        REQUIRE(tickets[index].isValid());
        CHECK_FALSE(f.pool.beginVisibilityCapture(false));
        // A closed ticket from an earlier view cannot end the main query.
        f.pool.end(tickets[0]);
        CHECK_FALSE(f.pool.canBegin(ALQueryKind::Occlusion));
        f.pool.end(tickets[index]);
    }
    f.pool.endVisibilityCapture();
    CHECK(f.pool.diagnostics().acquired == queries_per_view);

    // Model a later view that aborts after collecting closed tickets. Ending
    // its scope alone does not discard those tickets; its owner must do so.
    REQUIRE(f.pool.beginVisibilityCapture(false));
    for (U32 query = 0; query < queries_per_view; ++query)
    {
        const U32 index = accepted_queries + query;
        tickets[index] = f.pool.begin(ALQueryKind::Occlusion);
        REQUIRE(tickets[index].isValid());
        f.pool.end(tickets[index]);
    }
    f.pool.endVisibilityCapture();
    CHECK(f.pool.getNameCount() == total_queries);
    CHECK(f.pool.diagnostics().acquired == queries_per_view);

    // The shared producer supplies real pending GPU values. Attachment is
    // deliberately after all capture scopes ended, as the frontend handoff is.
    auto batch = f.encode(total_queries);
    for (U32 index = 0; index < accepted_queries; ++index)
        REQUIRE(f.pool.attachVisibilityResult(tickets[index], batch.results, index));
    REQUIRE(f.pool.attachVisibilityResult(
        tickets[accepted_queries], batch.results, accepted_queries));
    for (U32 index = accepted_queries; index < total_queries; ++index)
    {
        f.pool.discard(tickets[index]);
        U64 value = 99;
        CHECK(f.pool.result(tickets[index], value) == ALQueryPool::Status::Expired);
        CHECK(value == 0);
        CHECK_FALSE(f.pool.attachVisibilityResult(tickets[index], batch.results, index));
    }
    for (U32 index = 0; index < accepted_queries; ++index)
    {
        U64 value = 99;
        CHECK(f.pool.result(tickets[index], value) == ALQueryPool::Status::Pending);
        CHECK(value == 0);
    }

    f.commit(batch);
    std::weak_ptr<ALMetalVisibilityResultBatch> lease = batch.results;
    f.renderer.reset();
    batch.target.reset();
    batch.results.reset();
    CHECK_FALSE(lease.expired());
    for (U32 index = 0; index < accepted_queries; ++index)
    {
        CAPTURE(index);
        U64 value = 99;
        REQUIRE(f.pool.result(tickets[index], value) == ALQueryPool::Status::Ready);
        CHECK(value == (index % 2 == 0 ? 1 : 0));
    }
    for (U32 index = accepted_queries; index < total_queries; ++index)
    {
        U64 value = 99;
        CHECK(f.pool.result(tickets[index], value) == ALQueryPool::Status::Expired);
        CHECK(value == 0);
        CHECK_FALSE(f.pool.attachVisibilityResult(tickets[index], lease.lock(), index));
    }
    CHECK(f.pool.getAskedReads() == 0);
    CHECK(f.pool.getStalls() == 0);
    f.pool.endFrame();
    f.pool.release();
    CHECK(lease.expired());
}
