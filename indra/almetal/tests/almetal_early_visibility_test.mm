/**
 * @file almetal_early_visibility_test.mm
 * @brief Completed early compute, late native consumption and weak ownership.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldcohort.h"
#include "../almetalworldmaterialdata.h"

namespace
{
struct EarlyCpu
{
    std::vector<ALMetalWorldSimpleBatch> rows;
};
struct EarlyGroup
{
    std::shared_ptr<EarlyCpu> cpu;
    std::shared_ptr<Resources> native;
    ALMetalWorldSourceCohortView view;
};
struct EarlyGate
{
    id<MTLSharedEvent> event;
    explicit EarlyGate(const ALMetalDevice& device)
    {
        event = [(__bridge id<MTLDevice>)device.nativeHandle() newSharedEvent]; REQUIRE(event);
        auto command = [(__bridge id<MTLCommandQueue>)device.nativeCommandQueue() commandBuffer]; REQUIRE(command);
        [command encodeWaitForEvent:event value:1]; [command commit];
    }
    void release() noexcept { event.signaledValue = 1; }
    ~EarlyGate() { release(); }
};
EarlyGroup group(Fixture& f, unsigned family, std::uint64_t key, std::uint64_t region = 9)
{
    EarlyGroup result;
    result.cpu = std::make_shared<EarlyCpu>();
    result.native = std::make_shared<Resources>(*f.resources);
    result.cpu->rows.resize(3);
    ALMetalWorldMaterialDataCache materials(*f.device);
    std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> proofs;
    for (unsigned row = 0; row < 3; ++row)
    {
        auto geometry = f.geometry;
        const float shift = row == 1 ? 5.f : 0.f;
        for (auto& point : geometry.positions) point[0] += shift;
        geometry.uv[0][0] = 0.f; geometry.uv[0][1] = 0.f;
        geometry.uv[1][0] = 1.f; geometry.uv[1][1] = 0.f;
        geometry.uv[2][0] = .5f; geometry.uv[2][1] = 1.f;
        ALMetalBufferDescriptor descriptor; descriptor.length = sizeof(geometry);
        std::shared_ptr<ALMetalBuffer> vertices = ALMetalBuffer::create(*f.device, descriptor, &f.error); REQUIRE(vertices);
        REQUIRE(vertices->write(0, &geometry, sizeof(geometry), &f.error));
        result.native->buffers.push_back(vertices);
        auto& batch = result.cpu->rows[row]; batch = f.batch(family); batch.vertices = vertices.get();
        if (family)
        {
            // Legacy materials select MASK through their material mode and
            // slot cutoff; generic alpha_mask is the separate PBR contract.
            batch.alpha_cutoff = .6f;
            if (family == 1)
            {
                batch.material_alpha_mode = 2;
                batch.materialSlots()[0].alpha_cutoff = .6f;
                batch.texcoord1_offset = batch.texcoord2_offset = batch.texcoord_offset;
            }
            else
            {
                batch.alpha_mask = true;
                batch.pbrSlots()[0].alpha_cutoff = .6f;
            }
            ALMetalTextureDescriptor td; td.width = td.height = 2;
            td.format = ALMetalPixelFormat::rgba8_unorm_srgb;
            std::shared_ptr<ALMetalTexture> mask = ALMetalTexture::create(*f.device, td, &f.error); REQUIRE(mask);
            const std::uint8_t pixels[16]{255,64,32,0, 32,255,64,255, 64,32,255,255, 255,128,64,0};
            auto upload = f.device->makeCommandBuffer("Early visibility material mask", &f.error); REQUIRE(upload);
            REQUIRE(mask->upload(*upload, 0, 0, {}, pixels, 8, &f.error));
            REQUIRE(upload->commitAndWait(&f.error));
            batch.diffuse_textures[0] = mask.get(); result.native->textures.push_back(std::move(mask));
            CAPTURE(family, row, batch.material_alpha_mode, batch.alpha_mask);
            auto published_material = ALMetalWorldMaterialSource::publish(*f.device, batch, materials, &f.error);
            INFO(f.error); REQUIRE(published_material);
            auto published_resources = ALMetalWorldResourceSource::publish(*f.device, batch, result.native->textures, &f.error);
            INFO(f.error); REQUIRE(published_resources);
            for (const auto& uniform : {batch.native_material_source->materialLease(),
                    batch.native_material_source->pbrLease(false), batch.native_material_source->pbrLease(true)})
                if (uniform) result.native->buffers.push_back(uniform);
        }
        ALMetalWorldConservativeBounds bounds;
        bounds.minimum[0] = -.8f + shift; bounds.maximum[0] = .8f + shift;
        bounds.minimum[1] = -.8f; bounds.maximum[1] = .8f;
        bounds.minimum[2] = bounds.maximum[2] = .25f;
        bounds.qualified = row != 2; // Unknown geometry always keeps its direct draw.
        auto proof = ALMetalWorldCohortSource::seal({{key + row, family, 1}, &batch}, &f.error, &bounds);
        INFO(f.error); REQUIRE(proof); proofs.push_back(std::move(proof));
    }
    result.view.cohort = ALMetalWorldSourceCohort::seal(proofs, result.cpu, result.native, region, &f.error);
    INFO(f.error); REQUIRE(result.view.cohort);
    result.view.cpu_owner = result.cpu; result.view.native_owner = result.native;
    identity(result.view.view.modelview_projection); identity(result.view.view.modelview);
    for (unsigned axis = 0; axis < 3; ++axis)
        result.view.view.normal_matrix[axis][axis] = result.view.view.rigged_normal_matrix[axis][axis] = 1.f;
    return result;
}
std::shared_ptr<const ALMetalWorldVisibilityView> earlyView(
    ALMetalWorldVisibility& visibility, std::span<const ALMetalWorldSourceCohortView> cohorts, std::string& error)
{
    std::size_t logical = 0;
    for (const auto& view : cohorts) logical = std::max(logical, view.logical_begin + view.cohort->size());
    auto result = visibility.makeView({}, {}, cohorts, logical, &error);
    INFO(error); REQUIRE(result); return result;
}
struct EarlyOutput
{
    Pixels pixels;
    std::vector<std::uint64_t> queries;
    bool operator==(const EarlyOutput&) const = default;
};
EarlyOutput render(Fixture& f, std::vector<ALMetalWorldSourceCohortView> cohorts,
    std::shared_ptr<const ALMetalWorldEarlyVisibilityArguments> early, unsigned format)
{
    auto scene = f.scene(); scene.hdr = format & 1; scene.normal_hdr = format & 2; scene.reverse_z = format & 4;
    scene.material_argument_buffers = true; scene.early_visibility = std::move(early);
    std::vector<ALMetalWorldSimpleBatch> raw(4, f.batch(0));
    if (scene.reverse_z)
        for (auto& batch : raw) { batch.modelview_projection[10] = -.5f; batch.modelview_projection[14] = .5f; }
    std::vector<ALMetalWorldVisibilityQuery> queries(7);
    const std::size_t offsets[7]{0, 2, 4, 6, 8, 10, 13};
    for (unsigned index = 0; index < queries.size(); ++index)
    {
        auto& query = queries[index]; identity(query.modelview_projection);
        if (scene.reverse_z) { query.modelview_projection[10] = -.5f; query.modelview_projection[14] = .5f; }
        query.center[0] = .4f; query.center[1] = -.4f; query.center[2] = -.5f;
        query.half_extent[0] = query.half_extent[1] = query.half_extent[2] = .05f;
        query.batch_offset = offsets[index]; query.fan = 7; query.count_samples = 1;
    }
    auto input = ALMetalWorldViewInputs::adopt(scene, std::move(raw), {}, {}, std::move(cohorts),
        std::move(queries), {}, f.resources, 1, &f.error);
    INFO(f.error); REQUIRE(input);
    std::shared_ptr<ALMetalVisibilityResultBatch> results;
    auto target = f.renderer->renderOwnedInputs(*input, &f.error, &results); INFO(f.error); REQUIRE(target);
    EarlyOutput output{f.read(*target), {}};
    REQUIRE(results); output.queries.resize(7);
    for (unsigned index = 0; index < output.queries.size(); ++index)
        REQUIRE(results->wait(index, output.queries[index], &f.error));
    return output;
}
}

TEST_CASE("almetal: completed early cohorts remap raw gaps and preserve native masks unknown bounds and queries",
          "[almetal][early-world-visibility]")
{
    @autoreleasepool
    {
        Fixture f;
        auto visibility = ALMetalWorldVisibility::create(*f.device, &f.error); REQUIRE(visibility);
        std::array<EarlyGroup, 3> groups{group(f, 0, 10), group(f, 1, 20), group(f, 2, 30)};
        for (unsigned format : {0u, 3u, 4u, 7u})
        {
            CAPTURE(format);
            std::vector<ALMetalWorldSourceCohortView> early, late;
            for (unsigned at = 0; at < groups.size(); ++at)
            {
                auto view = groups[at].view;
                if (format & 4) { view.view.modelview_projection[10] = -.5f; view.view.modelview_projection[14] = .5f; }
                view.logical_begin = at * 3; view.base_before = 0; early.push_back(view);
                view.logical_begin = at * 4 + 1; view.base_before = at + 1; late.push_back(view);
            }
            auto view = earlyView(*visibility, early, f.error);
            auto producer = f.device->makeCommandBuffer("Actual early visibility producer A", &f.error); REQUIRE(producer);
            auto ticket = ALMetalWorldVisibility::encodeEarly(*producer, *view, &f.error); INFO(f.error); REQUIRE(ticket);
            REQUIRE(producer->commitAndWait(&f.error)); view.reset();
            const auto expected = render(f, late, {}, format);
            const auto before = f.renderer->worldEarlyVisibilityStats();
            CHECK(render(f, late, ticket, format) == expected);
            const auto after = f.renderer->worldEarlyVisibilityStats();
            CHECK(after.requested_views == before.requested_views + 1);
            CHECK(after.adopted_views == before.adopted_views + 1);
            CHECK(after.requested_cohort_draws == before.requested_cohort_draws + 9);
            CHECK(after.indirect_draws == before.indirect_draws + 6);
            CHECK(after.fallback_draws == before.fallback_draws + 3);
            CHECK(std::any_of(expected.queries.begin(), expected.queries.end(), [](auto samples) { return samples != 0; }));
        }
    }
}

TEST_CASE("almetal: early adoption declines pending abandoned and changed owners without relaxing original arguments",
          "[almetal][early-world-visibility]")
{
    @autoreleasepool
    {
        Fixture f; auto visibility = ALMetalWorldVisibility::create(*f.device, &f.error); REQUIRE(visibility);
        auto g = group(f, 0, 100); std::array<ALMetalWorldSourceCohortView, 1> groups{g.view};
        auto view = earlyView(*visibility, groups, f.error);
        auto producer = f.device->makeCommandBuffer("Unsubmitted early producer", &f.error); REQUIRE(producer);
        ALMetalWorldVisibilityMember raw; raw.enabled = 1; raw.index_count = 3;
        auto raw_view = visibility->makeView({raw}, &f.error); REQUIRE(raw_view);
        CHECK_FALSE(ALMetalWorldVisibility::encodeEarly(*producer, *raw_view, &f.error));
        auto ticket = ALMetalWorldVisibility::encodeEarly(*producer, *view, &f.error); REQUIRE(ticket);
        auto later = f.device->makeCommandBuffer("Uncommitted later reader", &f.error); REQUIRE(later);
        CHECK_FALSE(ticket->adoptCompleted(*later, groups, &f.error));
        producer.reset(); // Abandonment is not successful GPU completion.
        CHECK_FALSE(ticket->adoptCompleted(*later, groups, &f.error)); ticket.reset();
        {
            auto pending = f.device->makeCommandBuffer("Gated early producer", &f.error); REQUIRE(pending);
            EarlyGate gate(*f.device);
            auto delayed = ALMetalWorldVisibility::encodeEarly(*pending, *view, &f.error); REQUIRE(delayed);
            REQUIRE(pending->commit(&f.error));
            CHECK_FALSE(delayed->adoptCompleted(*later, groups, &f.error));
            gate.release(); REQUIRE(pending->waitUntilCompleted(&f.error));
            auto accepted = delayed->adoptCompleted(*later, groups, &f.error); REQUIRE(accepted);
            CHECK(accepted->appliesToCohort(0, 3, groups[0]));
            CHECK_FALSE(accepted->appliesToCohort(0, 4, groups[0]));
            CHECK_FALSE(accepted->appliesToCohort(2, 3, groups[0])); // Unknown bounds.
            for (unsigned change = 0; change < 7; ++change)
            {
                auto changed = groups;
                if (change == 0) changed[0].view.modelview_projection[12] = .25f;
                if (change == 1) changed[0].view.modelview[12] = .25f;
                if (change == 2) changed[0].cpu_owner = std::make_shared<int>(7);
                if (change == 3) changed[0].native_owner = std::make_shared<int>(8);
                if (change == 4) changed[0] = group(f, 0, 100, 10).view;
                if (change == 5) changed[0].cpu_owner = std::shared_ptr<const void>(g.cpu.get(), [](const void*) {});
                if (change == 6) changed[0].native_owner = std::shared_ptr<const void>(g.native.get(), [](const void*) {});
                CHECK_FALSE(delayed->adoptCompleted(*later, changed, &f.error));
            }
            auto remapped = groups; remapped[0].logical_begin = 5; remapped[0].base_before = 5;
            auto moved = delayed->adoptCompleted(*later, remapped, &f.error); REQUIRE(moved);
            CHECK_FALSE(moved->appliesToCohort(0, 3, remapped[0]));
            CHECK(moved->appliesToCohort(5, 3, remapped[0]));
        }
        // The existing same-command facade still refuses a later command.
        auto original = f.device->makeCommandBuffer("Existing same-command arguments", &f.error); REQUIRE(original);
        auto old = ALMetalWorldVisibility::encode(*original, *view, &f.error); REQUIRE(old);
        REQUIRE(original->commitAndWait(&f.error));
        ALMetalRenderTargetDescriptor td; td.width = td.height = 8;
        auto target = ALMetalRenderTarget::create(*f.device, td, &f.error); REQUIRE(target);
        auto pass = target->makeRenderPass({}, &f.error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*later, *pass, "Later command cannot borrow old arguments", &f.error); REQUIRE(encoder);
        CHECK_FALSE(old->drawIndexed(*encoder, *later, 0, ALMetalIndexType::uint16,
            *f.indices, 0, 3, &f.error));
        REQUIRE(encoder->end(&f.error));
    }
}

TEST_CASE("almetal: early bounds-only retirement drops CPU graphs and geometry before later adoption",
          "[almetal][early-world-visibility]")
{
    @autoreleasepool
    {
        Fixture f; auto visibility = ALMetalWorldVisibility::create(*f.device, &f.error); REQUIRE(visibility);
        auto cpu = std::make_shared<EarlyCpu>(); cpu->rows.push_back(f.batch(0));
        auto native = std::make_shared<Resources>();
        ALMetalBufferDescriptor bd; bd.length = sizeof(f.geometry);
        std::shared_ptr<ALMetalBuffer> vertex = ALMetalBuffer::create(*f.device, bd, &f.error); REQUIRE(vertex);
        REQUIRE(vertex->write(0, &f.geometry, sizeof(f.geometry), &f.error));
        const std::uint16_t indices[3]{0, 1, 2}; bd.length = sizeof(indices);
        std::shared_ptr<ALMetalBuffer> index = ALMetalBuffer::create(*f.device, bd, &f.error); REQUIRE(index);
        REQUIRE(index->write(0, indices, sizeof(indices), &f.error));
        cpu->rows[0].vertices = vertex.get(); cpu->rows[0].indices = index.get();
        native->buffers.push_back(vertex); native->buffers.push_back(index);
        ALMetalWorldConservativeBounds bounds; bounds.minimum[0] = bounds.minimum[1] = -.8f;
        bounds.maximum[0] = bounds.maximum[1] = .8f; bounds.minimum[2] = bounds.maximum[2] = .25f; bounds.qualified = true;
        auto source = ALMetalWorldCohortSource::seal({{900, 0, 1}, &cpu->rows[0]}, &f.error, &bounds); REQUIRE(source);
        auto cohort = ALMetalWorldSourceCohort::seal(std::span(&source, 1), cpu, native, 7, &f.error); REQUIRE(cohort);
        ALMetalWorldSourceCohortView group; group.cohort = cohort; group.cpu_owner = cpu; group.native_owner = native;
        identity(group.view.modelview_projection); identity(group.view.modelview);
        auto view = earlyView(*visibility, std::span(&group, 1), f.error);
        auto producer = f.device->makeCommandBuffer("Bounds-only early CPU lifetime", &f.error); REQUIRE(producer);
        auto ticket = ALMetalWorldVisibility::encodeEarly(*producer, *view, &f.error); REQUIRE(ticket);
        std::weak_ptr<EarlyCpu> cpu_weak = cpu; std::weak_ptr<Resources> native_weak = native;
        std::weak_ptr<const ALMetalBuffer> vertex_weak = vertex, index_weak = index;
        view.reset(); group.cpu_owner.reset(); group.native_owner.reset(); cpu.reset(); native.reset(); vertex.reset(); index.reset();
        CHECK(cpu_weak.expired()); CHECK(native_weak.expired());
        CHECK(vertex_weak.expired()); CHECK(index_weak.expired());
        REQUIRE(producer->commitAndWait(&f.error));
        auto later = f.device->makeCommandBuffer("Expired owners stay direct", &f.error); REQUIRE(later);
        CHECK_FALSE(ticket->adoptCompleted(*later, std::span(&group, 1), &f.error));
    }
}

TEST_CASE("almetal: later GPU reader protects early argument slot after producer completion",
          "[almetal][early-world-visibility]")
{
    @autoreleasepool
    {
        Fixture f; auto visibility = ALMetalWorldVisibility::create(*f.device, &f.error); REQUIRE(visibility);
        auto a = group(f, 0, 200), b = group(f, 0, 300);
        b.view.logical_begin = 3;
        std::array<ALMetalWorldSourceCohortView, 2> groups{a.view, b.view};
        auto view = earlyView(*visibility, groups, f.error);
        auto producer = f.device->makeCommandBuffer("Slot producer A", &f.error); REQUIRE(producer);
        auto early = ALMetalWorldVisibility::encodeEarly(*producer, *view, &f.error); REQUIRE(early);
        REQUIRE(producer->commitAndWait(&f.error));
        auto later = f.device->makeCommandBuffer("Slot reader B", &f.error); REQUIRE(later);
        auto read = early->adoptCompleted(*later, groups, &f.error); REQUIRE(read);
        EarlyGate gate(*f.device);
        REQUIRE(later->commit(&f.error)); early.reset(); read.reset();
        const auto before = visibility->stats(); REQUIRE(before.live_cohort_descriptor_bytes != 0);
        auto other = f.device->makeCommandBuffer("B pending requires another argument slot", &f.error); REQUIRE(other);
        auto next = ALMetalWorldVisibility::encodeEarly(*other, *view, &f.error); REQUIRE(next);
        CHECK(visibility->stats().cohort_descriptor_allocated_bytes > before.cohort_descriptor_allocated_bytes);
        next.reset(); other.reset();
        gate.release(); REQUIRE(later->waitUntilCompleted(&f.error));
        const auto allocated = visibility->stats().cohort_descriptor_allocated_bytes;
        auto reuse = f.device->makeCommandBuffer("B retired permits slot reuse", &f.error); REQUIRE(reuse);
        auto final = ALMetalWorldVisibility::encodeEarly(*reuse, *view, &f.error); REQUIRE(final);
        CHECK(visibility->stats().cohort_descriptor_allocated_bytes == allocated);
    }
}
