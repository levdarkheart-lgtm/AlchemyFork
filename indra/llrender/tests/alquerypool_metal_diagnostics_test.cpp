/** Real native visibility results exercise diagnostic phase deduplication. */
#include <catch2/catch_test_macros.hpp>

#include "alquerypool.h"
#include "almetaldevice.h"
#include "almetalresource.h"
#include "almetalworld.h"

#include <array>

TEST_CASE("Native visibility diagnostics count tickets across pending and GPU-ready phases",
          "[alquerypool][diagnostics][metal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);
    auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
    REQUIRE(renderer);
    auto data = ALMetalWorldFrameData::create(*device, &error);
    REQUIRE(data);
    data->beginFrame(1);
    renderer->setFrameData(data);

    ALQueryPool pool;
    struct DiagnosticsOffOnExit
    {
        ALQueryPool& pool;
        ~DiagnosticsOffOnExit() { pool.setDiagnosticsEnabled(false); }
    } cleanup{pool};
    pool.setDiagnosticsEnabled(true);
    REQUIRE(pool.beginVisibilityCapture(true));
    std::array<ALQueryTicket, 2> tickets;
    for (auto& ticket : tickets)
    {
        ticket = pool.begin(ALQueryKind::OcclusionExact);
        REQUIRE(ticket.isValid());
        pool.end(ticket);
    }
    pool.endVisibilityCapture();

    std::array<ALMetalWorldVisibilityQuery, 2> queries{};
    for (auto& query : queries)
    {
        for (std::size_t i = 0; i < 4; ++i) query.modelview_projection[i * 4 + i] = 1.f;
        query.center[3] = 1.f;
        query.half_extent[0] = query.half_extent[1] = query.half_extent[2] = 0.25f;
        query.fan = 7;
        query.count_samples = 1;
    }
    queries[1].center[0] = 4.f;
    ALMetalWorldGBufferScene scene;
    scene.width = scene.height = 32;
    scene.visibility_queries = queries.data();
    scene.visibility_query_count = queries.size();
    REQUIRE(renderer->beginCommandBatch("Query diagnostic phase regression", &error));
    std::shared_ptr<ALMetalVisibilityResultBatch> results;
    auto target = renderer->render(scene, &error, &results);
    INFO(error);
    REQUIRE(target);
    REQUIRE(results);
    REQUIRE(results->status() == ALMetalVisibilityResultBatch::Status::pending);
    REQUIRE(pool.attachVisibilityResult(tickets[0], results, 0));
    REQUIRE(pool.attachVisibilityResult(tickets[0], results, 0));
    REQUIRE(pool.attachVisibilityResult(tickets[1], results, 1));
    CHECK_FALSE(pool.attachVisibilityResult(tickets[1], results, 2));
    for (const auto& ticket : tickets)
    {
        U64 value = 99;
        CHECK(pool.result(ticket, value) == ALQueryPool::Status::Pending);
        CHECK(value == 0);
        CHECK(pool.result(ticket, value) == ALQueryPool::Status::Pending);
    }
    CHECK(pool.diagnostics().acquired == 2);
    CHECK(pool.diagnostics().attachment_calls == 3);
    CHECK(pool.diagnostics().attached_unique == 2);
    CHECK(pool.diagnostics().pending_polls == 4);
    CHECK(pool.diagnostics().pending_unique == 2);
    CHECK(pool.diagnostics().duplicate_phase_polls == 2);

    REQUIRE(renderer->commitCommandBatch(&error));
    std::uint64_t value = 0;
    REQUIRE(results->wait(0, value, &error));
    REQUIRE(value > 0);
    for (std::size_t i = 0; i < tickets.size(); ++i)
    {
        U64 count = 99;
        CHECK(pool.result(tickets[i], count) == ALQueryPool::Status::Ready);
        CHECK((count == 0) == (i == 1));
        CHECK(pool.result(tickets[i], count) == ALQueryPool::Status::Ready);
    }
    const auto completed = pool.diagnostics();
    CHECK(completed.ready_nonzero_polls == 2);
    CHECK(completed.ready_nonzero_unique == 1);
    CHECK(completed.ready_zero_polls == 2);
    CHECK(completed.ready_zero_unique == 1);
    CHECK(completed.expired_unique == 0);
    CHECK(completed.duplicate_phase_polls == 4);
    for (U32 i = 0; i < ALQueryPool::RESULT_FRAMES; ++i) pool.endFrame();
    CHECK(pool.diagnostics().retired == 2);
    CHECK(pool.diagnostics().retired_without_terminal_observation == 0);
}
