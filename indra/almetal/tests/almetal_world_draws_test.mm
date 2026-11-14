/**
 * @file almetal_world_draws_test.mm
 * @brief Ordered compact-source parity and CPU/native ownership boundaries.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldmaterialdata.h"

#include <cmath>

namespace
{
struct DrawDescriptions
{
    std::vector<ALMetalWorldSimpleBatch> sources;
};

ALMetalWorldViewMatrices matrices(const ALMetalWorldSimpleBatch& batch)
{
    ALMetalWorldViewMatrices value;
    std::memcpy(value.modelview_projection, batch.modelview_projection, sizeof(value.modelview_projection));
    std::memcpy(value.modelview, batch.modelview, sizeof(value.modelview));
    std::memcpy(value.normal_matrix, batch.normal_matrix, sizeof(value.normal_matrix));
    std::memcpy(value.rigged_normal_matrix, batch.rigged_normal_matrix, sizeof(value.rigged_normal_matrix));
    std::memcpy(value.texture_matrix, batch.texture_matrix, sizeof(value.texture_matrix));
    return value;
}

// A full-batch baseline exists only in the fixture. The production facade
// consumes source fields and these five per-view arrays without rebuilding it.
void applyMatrices(ALMetalWorldSimpleBatch& batch, const ALMetalWorldViewMatrices& view)
{
    std::memcpy(batch.modelview_projection, view.modelview_projection, sizeof(view.modelview_projection));
    std::memcpy(batch.modelview, view.modelview, sizeof(view.modelview));
    std::memcpy(batch.normal_matrix, view.normal_matrix, sizeof(view.normal_matrix));
    std::memcpy(batch.rigged_normal_matrix, view.rigged_normal_matrix, sizeof(view.rigged_normal_matrix));
    std::memcpy(batch.texture_matrix, view.texture_matrix, sizeof(view.texture_matrix));
}

void poisonMatrices(ALMetalWorldSimpleBatch& batch)
{
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    std::fill_n(batch.modelview_projection, 16, nan);
    std::fill_n(batch.modelview, 16, nan);
    for (auto& row : batch.normal_matrix) std::fill_n(row, 4, nan);
    for (auto& row : batch.rigged_normal_matrix) std::fill_n(row, 4, nan);
    std::fill_n(batch.texture_matrix, 16, nan);
}

ALMetalWorldSourceReference reference(const ALMetalWorldSimpleBatch& source,
                                     const ALMetalWorldViewMatrices& view)
{
    ALMetalWorldSourceReference result;
    result.source = &source; result.view = view;
    return result;
}

struct DrawResult
{
    Pixels pixels;
    std::vector<std::uint64_t> queries;
};

DrawResult renderDraws(Fixture& f, ALMetalWorldGBufferScene scene,
    const std::vector<ALMetalWorldSimpleBatch>& raw,
    const std::vector<ALMetalWorldSourceReference>& refs,
    const std::vector<ALMetalWorldDrawIndex>& order,
    const std::vector<ALMetalWorldVisibilityQuery>& queries,
    const std::shared_ptr<const void>& cpu_owner, bool prepared, std::uint64_t frame,
    bool owned = false)
{
    f.data->beginFrame(frame);
    scene.batches = raw.data(); scene.batch_count = raw.size();
    scene.references = refs.data(); scene.reference_count = refs.size();
    scene.draw_order = order.data(); scene.draw_order_count = order.size();
    scene.visibility_queries = queries.data(); scene.visibility_query_count = queries.size();
    scene.cpu_source_owner = cpu_owner; scene.source_owner = f.resources;
    std::unique_ptr<ALMetalRenderTarget> target;
    std::shared_ptr<ALMetalVisibilityResultBatch> query_results;
    if (prepared || owned)
    {
        auto input = ALMetalWorldViewInputs::adopt(scene, raw, refs, order, queries,
            cpu_owner, f.resources, frame, &f.error);
        INFO(f.error); REQUIRE(input);
        if (owned) target = f.renderer->renderOwnedInputs(*input, &f.error, &query_results);
        else
        {
            auto work = ALMetalWorldPreparation::create(input, 2, {}, &f.error);
            INFO(f.error); REQUIRE(work);
            for (std::size_t job = 0; job < work->jobCount(); ++job) REQUIRE(work->runJob(job));
            auto ready = work->finish(&f.error); INFO(f.error); REQUIRE(ready);
            CHECK(ready->stats().input_count == raw.size() + refs.size());
            CHECK(ready->stats().prepared_count == raw.size() + refs.size());
            target = f.renderer->renderPrepared(*ready, &f.error, &query_results);
        }
    }
    else target = f.renderer->render(scene, &f.error, &query_results);
    INFO(f.error); REQUIRE(target);
    DrawResult result{f.read(*target), std::vector<std::uint64_t>(queries.size())};
    if (!queries.empty())
    {
        REQUIRE(query_results);
        for (std::size_t index = 0; index < queries.size(); ++index)
            REQUIRE(query_results->wait(index, result.queries[index], &f.error));
    }
    return result;
}

std::vector<ALMetalWorldVisibilityQuery> orderedQueries(std::size_t count)
{
    std::vector<ALMetalWorldVisibilityQuery> result(count + 1);
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        auto& query = result[index]; identity(query.modelview_projection);
        query.center[0] = query.center[1] = -.5f; query.center[2] = .75f;
        query.half_extent[0] = query.half_extent[1] = query.half_extent[2] = .08f;
        query.batch_offset = index; query.fan = 7; query.count_samples = 1;
    }
    return result;
}

void waitForDrawResources(const std::weak_ptr<Resources>& weak)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!weak.expired() && std::chrono::steady_clock::now() < end)
        std::this_thread::yield();
    CHECK(weak.expired());
}
}

TEST_CASE("almetal: compact draws resolve immutable fields and all five matrix arrays",
          "[almetal][world-draws]")
{
    ALMetalWorldSimpleBatch batch;
    ALMetalWorldViewMatrices view;
    ALMetalWorldDraw raw(batch), compact(batch, &view);
    CHECK(&raw.source() == &batch); CHECK_FALSE(raw.view());
    CHECK(&compact.source() == &batch); CHECK(compact.view() == &view);
    CHECK(raw.modelviewProjection() == batch.modelview_projection);
    CHECK(raw.modelview() == batch.modelview);
    CHECK(raw.normalMatrix() == &batch.normal_matrix[0][0]);
    CHECK(raw.riggedNormalMatrix() == &batch.rigged_normal_matrix[0][0]);
    CHECK(raw.textureMatrix() == batch.texture_matrix);
    CHECK(compact.modelviewProjection() == view.modelview_projection);
    CHECK(compact.modelview() == view.modelview);
    CHECK(compact.normalMatrix() == &view.normal_matrix[0][0]);
    CHECK(compact.riggedNormalMatrix() == &view.rigged_normal_matrix[0][0]);
    CHECK(compact.textureMatrix() == view.texture_matrix);
    for (unsigned index = 0; index < 16; ++index)
        CHECK(view.texture_matrix[index] == (index % 5 == 0 ? 1.f : 0.f));
    CHECK(alMetalWorldDrawIndexFits(alMetalWorldDrawIndexMask));
    CHECK_FALSE(alMetalWorldDrawIndexFits(std::size_t{alMetalWorldDrawIndexMask} + 1));
    CHECK_FALSE(alMetalWorldDrawIsReference(19));
    CHECK(alMetalWorldDrawIsReference(alMetalWorldDrawReferenceBit | 19));
    CHECK(alMetalWorldDrawStorageIndex(alMetalWorldDrawReferenceBit | 19) == 19);
}

TEST_CASE("almetal: mixed compact sources preserve native MRT depth stencil UV and ordered queries",
          "[almetal][world-draws]")
{
    @autoreleasepool
    {
        Fixture f; ALMetalWorldMaterialDataCache cache(*f.device);
        auto geometry = f.geometry;
        const float uv[4][2]{{0,0},{1,0},{.5f,1},{.5f,.5f}};
        std::memcpy(geometry.uv, uv, sizeof(uv));
        const auto* vertices = f.buffer(&geometry, sizeof(geometry));
        std::array<std::uint8_t, 4 * 4 * 4> texels;
        for (unsigned y = 0; y < 4; ++y) for (unsigned x = 0; x < 4; ++x)
        {
            const auto p = (y * 4 + x) * 4;
            texels[p] = 30 + x * 60; texels[p+1] = 25 + y * 65;
            texels[p+2] = 220 - x * 35; texels[p+3] = 255;
        }
        const auto* diffuse = f.texture(ALMetalPixelFormat::rgba8_unorm_srgb, 4, texels.data(), 16);
        std::uint64_t serial = 2;
        for (bool mask : {false, true})
        {
            auto cpu = std::make_shared<DrawDescriptions>(); cpu->sources.reserve(2);
            std::vector<ALMetalWorldSimpleBatch> baseline;
            for (unsigned index = 0; index < 4; ++index)
            {
                auto batch = f.batch(index == 0 ? 3 : index == 1 ? 1 : index == 2 ? 0 : 2);
                batch.vertices = vertices; batch.diffuse_textures[0] = diffuse;
                batch.texcoord1_offset = batch.texcoord2_offset = offsetof(Fixture::Geometry, uv);
                auto view = matrices(batch);
                view.modelview_projection[0] = view.modelview_projection[5] = .4f;
                view.modelview_projection[12] = index % 2 ? .5f : -.5f;
                view.modelview_projection[13] = index < 2 ? -.5f : .5f;
                view.modelview[12] = .13f; view.modelview[13] = -.07f;
                view.normal_matrix[0][0] = .8f; view.normal_matrix[0][1] = .6f;
                view.normal_matrix[1][0] = -.6f; view.normal_matrix[1][1] = .8f;
                view.texture_matrix[0] = .55f; view.texture_matrix[5] = .6f;
                view.texture_matrix[12] = .23f; view.texture_matrix[13] = .1f;
                applyMatrices(batch, view);
                if (index == 1 && mask) batch.material_alpha_mode = 2;
                if (index == 3 && mask) batch.alpha_mask = true;
                if (index == 1 || index == 3)
                {
                    REQUIRE(ALMetalWorldMaterialSource::publish(*f.device, batch, cache, &f.error));
                    cpu->sources.push_back(batch);
                }
                baseline.push_back(batch);
            }
            std::vector<ALMetalWorldSimpleBatch> raw{baseline[0], baseline[2]};
            std::vector<ALMetalWorldSourceReference> refs{
                reference(cpu->sources[0], matrices(baseline[1])),
                reference(cpu->sources[1], matrices(baseline[3]))};
            const std::vector<ALMetalWorldDrawIndex> order{0, alMetalWorldDrawReferenceBit, 1,
                alMetalWorldDrawReferenceBit | 1};
            for (auto& source : cpu->sources) poisonMatrices(source);
            const auto queries = orderedQueries(4);
            for (unsigned format = 0; format < 8; ++format)
            {
                CAPTURE(mask, format);
                auto scene = f.scene(); scene.hdr = format & 1;
                scene.normal_hdr = format & 2; scene.reverse_z = format & 4;
                scene.join_adjacent_static_draws = true;
                const auto expected = renderDraws(f, scene, baseline, {}, {}, queries, {}, false, ++serial);
                REQUIRE(expected.queries.front() > 0);
                const auto clear = scene.reverse_z ? 0u : 0x3f800000u;
                REQUIRE(std::count_if(expected.pixels.depth.begin(), expected.pixels.depth.end(),
                    [&](auto value) { return value != clear; }) > 40);
                for (unsigned path = 0; path < 3; ++path)
                {
                    CAPTURE(path); // Borrowed scene, typed owned scalar, prepared.
                    const auto actual = renderDraws(f, scene, raw, refs, order, queries, cpu,
                        path == 2, ++serial, path == 1);
                    CHECK(actual.pixels == expected.pixels);
                    CHECK(actual.queries == expected.queries);
                }
            }
        }
    }
}

TEST_CASE("almetal: raw special records retain their position around compact sources",
          "[almetal][world-draws]")
{
    @autoreleasepool
    {
        Fixture f; auto cpu = std::make_shared<DrawDescriptions>();
        cpu->sources.push_back(f.batch(1));
        auto view = matrices(cpu->sources[0]);
        view.modelview_projection[0] = view.modelview_projection[5] = .45f;
        view.modelview_projection[12] = .4f;
        auto source_baseline = cpu->sources[0]; applyMatrices(source_baseline, view);
        auto terrain = f.batch(7), impostor = f.batch(9);
        impostor.impostor_center[0] = -.5f; impostor.impostor_center[1] = .5f;
        impostor.impostor_left[0] = impostor.impostor_up[1] = .2f;
        std::vector<ALMetalWorldSimpleBatch> raw{terrain, impostor};
        const std::vector<ALMetalWorldSimpleBatch> baseline{terrain, source_baseline, impostor};
        const std::vector<ALMetalWorldSourceReference> refs{reference(cpu->sources[0], view)};
        const std::vector<ALMetalWorldDrawIndex> order{0, alMetalWorldDrawReferenceBit, 1};
        auto scene = f.scene(); auto queries = orderedQueries(3);
        const auto expected = renderDraws(f, scene, baseline, {}, {}, queries, {}, false, 2);
        for (unsigned path = 0; path < 3; ++path)
        {
            CAPTURE(path);
            const auto actual = renderDraws(f, scene, raw, refs, order, queries, cpu,
                path == 2, path + 3, path == 1);
            CHECK(actual.pixels == expected.pixels); CHECK(actual.queries == expected.queries);
        }
    }
}

TEST_CASE("almetal: compact view validation rejects malformed mappings owners and overrides",
          "[almetal][world-draws]")
{
    @autoreleasepool
    {
        Fixture f; auto cpu = std::make_shared<DrawDescriptions>();
        cpu->sources.push_back(f.batch(0));
        auto raw = f.batch(3); auto ref = reference(cpu->sources[0], matrices(cpu->sources[0]));
        std::array<ALMetalWorldDrawIndex, 2> order{0, alMetalWorldDrawReferenceBit};
        auto valid = f.scene(); valid.batches = &raw; valid.batch_count = 1;
        valid.references = &ref; valid.reference_count = 1;
        valid.draw_order = order.data(); valid.draw_order_count = order.size();
        valid.cpu_source_owner = cpu; valid.source_owner = f.resources;
        REQUIRE(alMetalValidateWorldView(valid, &f.error));
        REQUIRE(f.renderer->render(valid, &f.error));
        for (unsigned malformed = 0; malformed < 14; ++malformed)
        {
            CAPTURE(malformed);
            auto scene = valid; auto local_ref = ref; auto local_order = order;
            auto unsupported = f.batch(malformed == 10 ? 4 : 3);
            scene.references = &local_ref; scene.draw_order = local_order.data();
            switch (malformed)
            {
                case 0: scene.references = nullptr; break;
                case 1: scene.draw_order = nullptr; break;
                case 2: local_ref.source = nullptr; break;
                case 3: local_order[1] = 0; break;
                case 4: scene.draw_order_count = 1; break;
                case 5: local_order[0] = 1; break;
                case 6: local_order[1] = alMetalWorldDrawReferenceBit | 1; break;
                case 7: scene.draw_order_count = 0; break;
                case 8: scene.cpu_source_owner.reset(); break;
                case 9: scene.source_owner.reset(); break;
                case 10: local_ref.source = &unsupported; break;
                case 11: local_ref.source = &unsupported; break;
                case 12: local_ref.view.modelview_projection[0] = std::numeric_limits<float>::quiet_NaN(); break;
                case 13: local_ref.view.modelview[15] = 0; break;
            }
            f.error.clear(); CHECK_FALSE(f.renderer->render(scene, &f.error));
        }
        // Every matrix family is checked through the actual draw facade.
        for (unsigned array = 0; array < 5; ++array)
        {
            CAPTURE(array);
            auto bad = ref;
            float* values[] = {bad.view.modelview_projection, bad.view.modelview,
                &bad.view.normal_matrix[0][0], &bad.view.rigged_normal_matrix[0][0], bad.view.texture_matrix};
            values[array][0] = std::numeric_limits<float>::quiet_NaN();
            auto scene = valid; scene.references = &bad;
            CHECK_FALSE(f.renderer->render(scene, &f.error));
        }
        auto queries = orderedQueries(2); queries.back().batch_offset = 3;
        auto query_scene = valid; query_scene.visibility_queries = queries.data();
        query_scene.visibility_query_count = queries.size();
        CHECK_FALSE(f.renderer->render(query_scene, &f.error));
        CHECK_FALSE(ALMetalWorldViewInputs::adopt(valid, {raw}, {}, f.resources, 1, &f.error));
        CHECK_FALSE(ALMetalWorldViewInputs::adopt(valid, {raw}, {ref}, {0,0}, {}, cpu, f.resources, 1, &f.error));
        CHECK_FALSE(ALMetalWorldViewInputs::adopt(valid, {raw}, {ref}, {0,alMetalWorldDrawReferenceBit}, {}, {}, f.resources, 1, &f.error));
        CHECK_FALSE(ALMetalWorldViewInputs::adopt(valid, {raw}, {ref}, {0,alMetalWorldDrawReferenceBit}, {}, cpu, {}, 1, &f.error));
        // The detached helper is safe even before an owner's whole-view check.
        ALMetalWorldPreparedRecord record; std::array<std::byte, 400> bytes;
        auto invalid_local = valid;
        invalid_local.draw_order = nullptr;
        CHECK_FALSE(alMetalPrepareWorldRecord(invalid_local, 0, record, bytes, &f.error));
    }
}

TEST_CASE("almetal: typed owned input keeps draw validation and does not certify copied public scenes",
          "[almetal][world-draws]")
{
    @autoreleasepool
    {
        Fixture f; auto cpu = std::make_shared<DrawDescriptions>();
        cpu->sources.push_back(f.batch(1));
        const auto source_ref = reference(cpu->sources[0], matrices(cpu->sources[0]));
        auto inputs = ALMetalWorldViewInputs::adopt(f.scene(), {f.batch(3)}, {source_ref},
            {0, alMetalWorldDrawReferenceBit}, {}, cpu, f.resources, 2, &f.error);
        INFO(f.error); REQUIRE(inputs);
        auto owned = f.renderer->renderOwnedInputs(*inputs, &f.error);
        INFO(f.error); REQUIRE(owned); const auto expected = f.read(*owned);
        // A copied Scene is still public borrowed input, even when its owners
        // and all original arrays came from a valid private input packet.
        for (unsigned mutation = 0; mutation < 6; ++mutation)
        {
            CAPTURE(mutation);
            auto public_copy = inputs->scene();
            std::array<ALMetalWorldDrawIndex,2> order{0, alMetalWorldDrawReferenceBit};
            public_copy.draw_order = order.data();
            switch (mutation)
            {
                case 0: public_copy.draw_order_count = 1; break;
                case 1: order[1] = 0; break;
                case 2: public_copy.source_owner.reset(); break;
                case 3: public_copy.cpu_source_owner.reset(); break;
                case 4: public_copy.references = nullptr; break;
                case 5: public_copy.batches = nullptr; break;
            }
            f.error.clear(); CHECK_FALSE(f.renderer->render(public_copy, &f.error));
        }
        auto unchanged = f.renderer->renderOwnedInputs(*inputs, &f.error);
        INFO(f.error); REQUIRE(unchanged); CHECK(f.read(*unchanged) == expected);
        for (unsigned bad = 0; bad < 3; ++bad)
        {
            CAPTURE(bad);
            auto descriptions = std::make_shared<DrawDescriptions>();
            descriptions->sources.push_back(f.batch(0));
            auto ref = reference(descriptions->sources[0], matrices(descriptions->sources[0]));
            if (bad == 0) ref.view.modelview_projection[0] = std::numeric_limits<float>::quiet_NaN();
            if (bad == 1) ref.view.modelview[15] = 0;
            if (bad == 2) descriptions->sources[0].index_offset = f.indices->length() + sizeof(std::uint16_t);
            // Adoption certifies private storage/order/ownership, not native
            // draw ranges or view matrices. Owned scalar must still reject.
            auto invalid = ALMetalWorldViewInputs::adopt(f.scene(), {}, {ref},
                {alMetalWorldDrawReferenceBit}, {}, descriptions, f.resources, 3, &f.error);
            INFO(f.error); REQUIRE(invalid);
            CHECK_FALSE(f.renderer->renderOwnedInputs(*invalid, &f.error));
        }
        auto raw_bad = f.batch(0); raw_bad.index_offset = f.indices->length() + sizeof(std::uint16_t);
        auto invalid_raw = ALMetalWorldViewInputs::adopt(f.scene(), {raw_bad}, {}, f.resources, 4, &f.error);
        REQUIRE(invalid_raw); CHECK_FALSE(f.renderer->renderOwnedInputs(*invalid_raw, &f.error));
    }
}

TEST_CASE("almetal: compact preparation cancellation preserves readers and reclaims all capture capacity",
          "[almetal][world-draws]")
{
    @autoreleasepool
    {
        Fixture f; auto cpu = std::make_shared<DrawDescriptions>();
        cpu->sources.push_back(f.batch(1)); std::weak_ptr<DrawDescriptions> weak = cpu;
        std::vector<ALMetalWorldSimpleBatch> raw; raw.reserve(5); raw.push_back(f.batch(3));
        std::vector<ALMetalWorldSourceReference> refs; refs.reserve(7);
        refs.push_back(reference(cpu->sources[0], matrices(cpu->sources[0])));
        std::vector<ALMetalWorldDrawIndex> order; order.reserve(10); order = {0, alMetalWorldDrawReferenceBit};
        auto queries = orderedQueries(2); queries.reserve(6);
        const std::array<std::size_t,4> capacities{raw.capacity(), refs.capacity(), order.capacity(), queries.capacity()};
        auto inputs = ALMetalWorldViewInputs::adopt(f.scene(), std::move(raw), std::move(refs), std::move(order),
            std::move(queries), cpu, f.resources, 7, &f.error); INFO(f.error); REQUIRE(inputs);
        cpu.reset(); CHECK_FALSE(weak.expired());
        CHECK(inputs->scene().drawCount() == 2); CHECK(inputs->scene().batch_count == 1);
        CHECK(inputs->scene().draw(1).view() == &inputs->scene().references[0].view);
        auto epoch = std::make_shared<std::atomic<std::uint64_t>>(7);
        auto work = ALMetalWorldPreparation::create(inputs, 2, epoch, &f.error); REQUIRE(work);
        REQUIRE(work->jobCount() == 2); REQUIRE(work->runJob(0));
        epoch->store(8); CHECK_FALSE(work->runJob(1)); CHECK_FALSE(work->finish(&f.error));
        std::vector<ALMetalWorldSimpleBatch> returned_raw;
        std::vector<ALMetalWorldSourceReference> returned_refs;
        std::vector<ALMetalWorldDrawIndex> returned_order;
        std::vector<ALMetalWorldVisibilityQuery> returned_queries;
        CHECK_FALSE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs, returned_raw, returned_refs, returned_order, returned_queries));
        CHECK(inputs); CHECK_FALSE(weak.expired()); work->cancel(); work.reset();
        // Ticket cancellation affects its prepared result, not the immutable
        // input packet. A required local scalar retry remains available.
        REQUIRE(f.renderer->renderOwnedInputs(*inputs, &f.error));
        CHECK_FALSE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs, returned_raw, returned_queries));
        returned_order.push_back(42);
        CHECK_FALSE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs, returned_raw, returned_refs, returned_order, returned_queries));
        CHECK(returned_order == std::vector<ALMetalWorldDrawIndex>{42}); returned_order.clear();
        REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs, returned_raw, returned_refs, returned_order, returned_queries));
        CHECK_FALSE(inputs); CHECK(weak.expired());
        CHECK(returned_raw.empty()); CHECK(returned_refs.empty()); CHECK(returned_order.empty()); CHECK(returned_queries.empty());
        CHECK(returned_raw.capacity() == capacities[0]); CHECK(returned_refs.capacity() == capacities[1]);
        CHECK(returned_order.capacity() == capacities[2]); CHECK(returned_queries.capacity() == capacities[3]);
        // Empty-but-reserved compact storage cannot suppress a later raw view.
        returned_raw.push_back(f.batch(0));
        auto raw_input = ALMetalWorldViewInputs::adopt(f.scene(), std::move(returned_raw),
            std::move(returned_refs), std::move(returned_order), std::move(returned_queries),
            {}, f.resources, 9, &f.error); REQUIRE(raw_input);
        CHECK(raw_input->scene().drawCount() == 1);
        REQUIRE(f.renderer->renderOwnedInputs(*raw_input, &f.error));
    }
}

TEST_CASE("almetal: compact CPU descriptions retire before pending native commands",
          "[almetal][world-draws]")
{
    @autoreleasepool
    {
        for (bool prepared : {false, true}) for (bool abandon : {false, true})
        {
            CAPTURE(prepared, abandon);
            Fixture f; auto cpu = std::make_shared<DrawDescriptions>(); cpu->sources.push_back(f.batch(2));
            auto view = matrices(cpu->sources[0]);
            view.modelview_projection[0] = .75f; view.texture_matrix[12] = .25f;
            auto baseline = cpu->sources[0]; applyMatrices(baseline, view);
            auto scene = f.scene(); scene.hdr = true;
            const auto expected = renderDraws(f, scene, {baseline}, {}, {}, {}, {}, false, 2).pixels;
            std::weak_ptr<DrawDescriptions> weak_cpu = cpu; std::weak_ptr<Resources> weak_native = f.resources;
            std::vector<ALMetalWorldSimpleBatch> raw; raw.reserve(4);
            std::vector<ALMetalWorldSourceReference> refs; refs.reserve(8); refs.push_back(reference(cpu->sources[0], view));
            std::vector<ALMetalWorldDrawIndex> order; order.reserve(8); order.push_back(alMetalWorldDrawReferenceBit);
            std::vector<ALMetalWorldVisibilityQuery> queries; queries.reserve(4);
            poisonMatrices(cpu->sources[0]);
            auto input = ALMetalWorldViewInputs::adopt(scene, std::move(raw), std::move(refs), std::move(order),
                std::move(queries), cpu, f.resources, 3, &f.error); REQUIRE(input);
            std::shared_ptr<ALMetalPreparedWorldView const> ready;
            if (prepared)
            {
                auto work = ALMetalWorldPreparation::create(input, 1, {}, &f.error); REQUIRE(work);
                REQUIRE(work->runJob(0)); ready = work->finish(&f.error); REQUIRE(ready);
            }
            id<MTLSharedEvent> gate = [(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent]; REQUIRE(gate);
            struct OpenGate { id<MTLSharedEvent> gate; ~OpenGate() { gate.signaledValue = 1; } } open{gate};
            if (!abandon)
            {
                auto blocker = f.device->makeCommandBuffer("Hold native tuple while CPU packet retires", &f.error); REQUIRE(blocker);
                [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];
                REQUIRE(blocker->commit(&f.error));
            }
            f.data->beginFrame(3);
            REQUIRE(f.renderer->beginCommandBatch("Compact packet lifetime", &f.error));
            auto target = prepared ? f.renderer->renderPrepared(*ready, &f.error)
                                   : f.renderer->renderOwnedInputs(*input, &f.error);
            INFO(f.error); REQUIRE(target);
            auto completion = f.renderer->commandBatchCompletion(); REQUIRE(completion); CHECK_FALSE(completion->isComplete());
            if (!abandon) REQUIRE(f.renderer->commitCommandBatch(&f.error));
            ready.reset(); cpu.reset(); f.resources.reset();
            CHECK_FALSE(weak_cpu.expired());
            REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(input, raw, refs, order, queries));
            CHECK(weak_cpu.expired()); CHECK_FALSE(weak_native.expired());
            // Returned capture memory is free for reuse before GPU completion.
            raw.resize(4); for (auto& b : raw) poisonMatrices(b);
            refs.resize(8); for (auto& r : refs) { r.source = nullptr; r.view.modelview[0] = std::numeric_limits<float>::quiet_NaN(); }
            order.assign(8, 0xffffffffu); queries.resize(4);
            if (abandon) f.renderer->cancelCommandBatch();
            else
            {
                CHECK_FALSE(completion->isComplete()); gate.signaledValue = 1;
                REQUIRE(f.device->waitUntilIdle(&f.error)); CHECK(f.read(*target) == expected);
            }
            waitForDrawResources(weak_native);
        }
    }
}

TEST_CASE("almetal: native adjacent joins span raw and compact references without changing query boundaries",
          "[almetal][world-draws]")
{
    @autoreleasepool
    {
        Fixture f;
        struct TimingRestore
        {
            bool old = ALMetalDevice::commandTimingEnabled();
            ~TimingRestore() { ALMetalDevice::setCommandTimingEnabled(old); }
        } timing;
        ALMetalDevice::setCommandTimingEnabled(true);
        const std::array<std::uint16_t, 6> indices{0,1,2,0,1,2};
        const auto* joined_indices = f.buffer(indices.data(), sizeof(indices));
        auto first = f.batch(0); first.indices = joined_indices;
        auto second = first; second.index_offset = 3 * sizeof(std::uint16_t);
        auto cpu = std::make_shared<DrawDescriptions>(); cpu->sources.push_back(second);
        const std::vector<ALMetalWorldSourceReference> refs{reference(cpu->sources[0], matrices(second))};
        const std::vector<ALMetalWorldDrawIndex> order{0, alMetalWorldDrawReferenceBit};
        auto scene = f.scene(); scene.join_adjacent_static_draws = true;
        const auto before = ALMetalDevice::encodedDrawCallCount();
        const auto expected = renderDraws(f, scene, {first, second}, {}, {}, {}, {}, false, 2);
        REQUIRE(ALMetalDevice::encodedDrawCallCount() - before == 1);
        std::uint64_t serial = 3;
        for (unsigned path = 0; path < 3; ++path)
        {
            CAPTURE(path);
            const auto start = ALMetalDevice::encodedDrawCallCount();
            const auto actual = renderDraws(f, scene, {first}, refs, order, {}, cpu,
                path == 2, ++serial, path == 1);
            CHECK(actual.pixels == expected.pixels);
            CHECK(ALMetalDevice::encodedDrawCallCount() - start == 1);
        }
        const auto queries = orderedQueries(2);
        const auto query_start = ALMetalDevice::encodedDrawCallCount();
        const auto query_expected = renderDraws(f, scene, {first, second}, {}, {}, queries, {}, false, ++serial);
        const auto query_draws = ALMetalDevice::encodedDrawCallCount() - query_start;
        REQUIRE(query_draws > 1);
        for (unsigned path = 0; path < 3; ++path)
        {
            CAPTURE(path);
            const auto start = ALMetalDevice::encodedDrawCallCount();
            const auto actual = renderDraws(f, scene, {first}, refs, order, queries, cpu,
                path == 2, ++serial, path == 1);
            CHECK(actual.pixels == query_expected.pixels); CHECK(actual.queries == query_expected.queries);
            CHECK(ALMetalDevice::encodedDrawCallCount() - start == query_draws);
        }
    }
}

// A component benchmark of the complete owned input path, not viewer FPS or
// the LL producer frontier. Geometry/textures/PSOs are already native and are
// created outside timing. Both modes use the same scalar encoder, query work,
// quality and frame lifecycle; only capture representation differs. No native
// material/resource Source factory is treated as a real viewer publication.
TEST_CASE("almetal: complete compact source packet costs retain small and adverse cases",
          "[almetal][.world-draws-cost]")
{
    @autoreleasepool
    {
        Fixture f;
        auto probes = ALMetalWorldGBufferRenderer::create(*f.device, &f.error);
        INFO(f.error); REQUIRE(probes); probes->setFrameData(f.data);
        std::array<Fixture::Geometry, 8> tiles;
        for (unsigned tile = 0; tile < tiles.size(); ++tile)
        {
            tiles[tile] = f.geometry;
            const float x = (static_cast<float>(tile % 4) - 1.5f) * .47f;
            const float y = tile < 4 ? -.45f : .45f;
            for (unsigned vertex = 0; vertex < 4; ++vertex)
            {
                tiles[tile].positions[vertex][0] = f.geometry.positions[vertex][0] * .24f + x;
                tiles[tile].positions[vertex][1] = f.geometry.positions[vertex][1] * .38f + y;
                tiles[tile].colors[vertex][0] = 50 + tile * 25;
                tiles[tile].colors[vertex][1] = 225 - tile * 20;
            }
        }
        const auto* vertices = f.buffer(tiles.data(), sizeof(tiles));
        const auto cpu_ns = []
        {
            timespec value{}; REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) == 0);
            return static_cast<std::uint64_t>(value.tv_sec) * 1'000'000'000ULL + value.tv_nsec;
        };
        const auto summarize = [](auto values)
        {
            std::sort(values.begin(), values.end());
            return std::array<double, 2>{(values[5] + values[6]) * .5, values.back()};
        };
        struct Capture
        {
            std::vector<ALMetalWorldSimpleBatch> raw;
            std::vector<ALMetalWorldSourceReference> refs;
            std::vector<ALMetalWorldDrawIndex> order;
            std::vector<ALMetalWorldVisibilityQuery> queries;
            void releaseCapacity()
            {
                decltype(raw){}.swap(raw); decltype(refs){}.swap(refs);
                decltype(order){}.swap(order); decltype(queries){}.swap(queries);
            }
        };
        struct Costs
        {
            std::array<double, 12> cpu{}, encode{}, complete{}, source_setup_cpu{};
            std::uint64_t published_bytes = 0, encoded_draws = 0;
        };
        std::uint64_t serial = 1;
        struct TimingRestore
        {
            bool old = ALMetalDevice::commandTimingEnabled();
            ~TimingRestore() { ALMetalDevice::setCommandTimingEnabled(old); }
        } timing;
        ALMetalDevice::setCommandTimingEnabled(true);
        for (unsigned family : {0u, 1u, 2u})
        for (unsigned workload = 0; workload < 5; ++workload)
        {
            const std::array<unsigned, 5> main_counts{1,4,16,512,512};
            const unsigned views = workload == 4 ? 3 : 1;
            const std::array<unsigned, 3> counts{main_counts[workload],128,128};
            const unsigned variants = std::min(8u, counts[0]);
            const auto publish_descriptions = [&](unsigned generation)
            {
                auto owner = std::make_shared<DrawDescriptions>(); owner->sources.reserve(variants);
                for (unsigned variant = 0; variant < variants; ++variant)
                {
                    auto batch = f.batch(family); batch.vertices = vertices;
                    const auto base = variant * sizeof(Fixture::Geometry);
                    batch.position_offset += base; batch.normal_offset += base;
                    batch.texcoord_offset += base; batch.color_offset += base; batch.tangent_offset += base;
                    // The texture coordinates share the same region-space
                    // geometry buffer, including the legacy extra UV lanes.
                    batch.texcoord1_offset = batch.texcoord2_offset = base + offsetof(Fixture::Geometry, uv);
                    if (family == 0) batch.use_vertex_color = (variant + generation) % 2 == 0;
                    if (family == 1)
                    {
                        batch.materialSlots()[0].env_intensity = .05f + ((variant + generation) % 8) * .1f;
                        batch.materialSlots()[0].specular_color[0] = .1f + variant * .08f;
                    }
                    if (family == 2)
                    {
                        batch.pbrSlots()[0].roughness_factor = .1f + ((variant + generation) % 8) * .1f;
                        batch.pbrSlots()[0].emissive_color[0] = variant * .04f;
                    }
                    owner->sources.push_back(std::move(batch));
                }
                return owner;
            };
            for (unsigned phase = 0; phase < 3; ++phase)
            {
                // Warm: one shared immutable CPU publication, reported apart
                // from amortized capture cost. Cold: fresh source records and
                // capacities each outer frame. Churn: new material generation
                // each outer frame while capture capacity is reusable. Source
                // construction is charged to BOTH modes in cold/churn rows.
                const auto once_start = cpu_ns();
                auto warm_sources = phase == 1 ? publish_descriptions(0) : nullptr;
                const auto one_time_setup_ns = phase == 1 ? cpu_ns() - once_start : 0;
                std::array<std::array<Capture,3>,2> captures;
                std::array<Costs,2> costs;
                for (unsigned sample = 0; sample < 14; ++sample)
                {
                    std::array<Pixels,3> paired_pixels;
                    std::array<std::vector<std::uint64_t>,3> paired_queries;
                    for (unsigned turn = 0; turn < 2; ++turn)
                    {
                        // Alternating paired order avoids systematically
                        // assigning first-use effects to one representation.
                        const unsigned compact = (sample + turn) % 2;
                        std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                        std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> query_results;
                        const auto started = std::chrono::steady_clock::now();
                        const auto cpu_started = cpu_ns();
                        if (phase == 0) for (auto& capture : captures[compact]) capture.releaseCapacity();
                        const auto setup_start = cpu_ns();
                        auto descriptions = warm_sources ? warm_sources : publish_descriptions(phase == 2 ? sample + 1 : 0);
                        const auto setup_ns = warm_sources ? 0 : cpu_ns() - setup_start;
                        // Exactly one frame-data advance for main plus both
                        // probe transactions. Both renderers share this data.
                        f.data->beginFrame(++serial);
                        const auto before = f.data->stats();
                        const auto before_draws = ALMetalDevice::encodedDrawCallCount();
                        for (unsigned view = 0; view < views; ++view)
                        {
                            auto& capture = captures[compact][view];
                            auto scene = f.scene(); scene.hdr = true;
                            scene.join_adjacent_static_draws = false;
                            // Region-space geometry has ONE camera per view,
                            // shared by all draws, never invented per-object
                            // transforms to manufacture constant churn.
                            auto view_matrices = matrices(descriptions->sources[0]);
                            const float camera = sample * .002f + view * .03f;
                            view_matrices.modelview_projection[12] = camera;
                            view_matrices.modelview[12] = camera;
                            for (unsigned draw = 0; draw < counts[view]; ++draw)
                            {
                                const auto& source = descriptions->sources[draw % variants];
                                if (compact)
                                {
                                    capture.refs.push_back(reference(source, view_matrices));
                                    capture.order.push_back(alMetalWorldDrawReferenceBit | draw);
                                }
                                else
                                {
                                    capture.raw.push_back(source);
                                    applyMatrices(capture.raw.back(), view_matrices);
                                }
                            }
                            capture.queries.resize(3);
                            for (unsigned q = 0; q < capture.queries.size(); ++q)
                            {
                                auto& query = capture.queries[q]; identity(query.modelview_projection);
                                query.center[0] = -.7f + camera; query.center[1] = -.45f; query.center[2] = .75f;
                                query.half_extent[0] = query.half_extent[1] = query.half_extent[2] = .08f;
                                query.batch_offset = q == 0 ? 0 : q == 1 ? counts[view] / 2 : counts[view];
                                query.fan = 7; query.count_samples = 1;
                            }
                            std::shared_ptr<const void> cpu_owner;
                            if (compact) cpu_owner = descriptions;
                            auto inputs = ALMetalWorldViewInputs::adopt(scene, std::move(capture.raw),
                                std::move(capture.refs), std::move(capture.order), std::move(capture.queries),
                                std::move(cpu_owner), f.resources, serial, &f.error);
                            INFO(f.error); REQUIRE(inputs);
                            auto& renderer = view == 0 ? *f.renderer : *probes;
                            REQUIRE(renderer.beginCommandBatch("Compact complete packet cost", &f.error));
                            targets[view] = renderer.renderOwnedInputs(*inputs, &f.error, &query_results[view]);
                            INFO(f.error); REQUIRE(targets[view]); REQUIRE(query_results[view]);
                            REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs, capture.raw,
                                capture.refs, capture.order, capture.queries));
                            REQUIRE(renderer.commitCommandBatch(&f.error));
                        }
                        descriptions.reset();
                        const auto encoded = std::chrono::steady_clock::now();
                        // One explicit native completion wait, after all view
                        // transactions. Query reads settle actual GPU results.
                        REQUIRE(f.device->waitUntilIdle(&f.error));
                        std::array<std::vector<std::uint64_t>,3> queries;
                        for (unsigned view = 0; view < views; ++view)
                        {
                            queries[view].resize(3);
                            for (unsigned q = 0; q < 3; ++q)
                                REQUIRE(query_results[view]->wait(q, queries[view][q], &f.error));
                        }
                        const auto completed_cpu = cpu_ns();
                        const auto completed = std::chrono::steady_clock::now();
                        const auto publication_bytes = f.data->stats().published_bytes - before.published_bytes;
                        const auto draws = ALMetalDevice::encodedDrawCallCount() - before_draws;
                        // Read all native MRT/depth/stencil surfaces outside
                        // timings during BOTH warmups, under matched cameras.
                        if (sample < 2)
                            for (unsigned view = 0; view < views; ++view)
                            {
                                const auto pixels = f.read(*targets[view]);
                                if (!turn) paired_pixels[view] = pixels;
                                else CHECK(pixels == paired_pixels[view]);
                            }
                        for (unsigned view = 0; view < views; ++view)
                        {
                            if (!turn) paired_queries[view] = queries[view];
                            else CHECK(queries[view] == paired_queries[view]);
                        }
                        if (sample >= 2)
                        {
                            auto& row = costs[compact]; const auto index = sample - 2;
                            row.cpu[index] = (completed_cpu - cpu_started) / 1e6;
                            row.encode[index] = std::chrono::duration<double,std::milli>(encoded - started).count();
                            row.complete[index] = std::chrono::duration<double,std::milli>(completed - started).count();
                            row.source_setup_cpu[index] = setup_ns / 1e6;
                            row.published_bytes += publication_bytes; row.encoded_draws += draws;
                        }
                    }
                }
                std::size_t total_draws = counts[0];
                if (views == 3) total_draws += counts[1] + counts[2];
                for (unsigned compact = 0; compact < 2; ++compact)
                {
                    const auto& row = costs[compact];
                    const auto cpu = summarize(row.cpu), encode = summarize(row.encode);
                    const auto complete = summarize(row.complete), setup = summarize(row.source_setup_cpu);
                    std::cout << "world_draws_cost family=" << family << " main_count=" << counts[0]
                        << " views=" << views << " probe_count=" << (views == 3 ? 128 : 0)
                        << " phase=" << (phase == 0 ? "cold" : phase == 1 ? "warm" : "churn")
                        << " mode=" << (compact ? "source_ref_scalar" : "raw_copy_scalar")
                        << " samples=12 warmups=2 paired_alternating=1 input_entry=typed_owned cpu_prototypes=" << variants
                        << " cpu_prototype_inline_bytes=" << variants * sizeof(ALMetalWorldSimpleBatch)
                        << " raw_bytes_per_outer=" << (compact ? 0 : total_draws * sizeof(ALMetalWorldSimpleBatch))
                        << " ref_bytes_per_outer=" << (compact ? total_draws * sizeof(ALMetalWorldSourceReference) : 0)
                        << " order_bytes_per_outer=" << (compact ? total_draws * sizeof(ALMetalWorldDrawIndex) : 0)
                        << " query_bytes_per_outer=" << views * 3 * sizeof(ALMetalWorldVisibilityQuery)
                        << " cpu_median_ms=" << cpu[0] << " cpu_max_ms=" << cpu[1]
                        << " encode_median_ms=" << encode[0] << " encode_max_ms=" << encode[1]
                        << " complete_median_ms=" << complete[0] << " complete_max_ms=" << complete[1]
                        << " source_setup_cpu_median_ms=" << setup[0] << " source_setup_cpu_max_ms=" << setup[1]
                        << " shared_one_time_source_setup_cpu_ms=" << one_time_setup_ns / 1e6
                        << " sampled_native_publication_bytes=" << row.published_bytes
                        << " sampled_native_encoded_draws=" << row.encoded_draws
                        << " native_source_factories=0 ll_producer_frontier=excluded"
                           " fixture_native_resources=precreated pso_compilation=warmup_excluded"
                           " cold_scope=cpu_source_and_capture_capacity churn_scope=cpu_material_generation"
                           " cpu_clock=owner_thread_including_query_settle encode_clock=host_through_reclaim_submit"
                           " complete_clock=host_through_native_completion_query_settle\n";
                }
            }
        }
    }
}
