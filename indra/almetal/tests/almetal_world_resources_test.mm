/**
 * @file almetal_world_resources_test.mm
 * @brief Actual world resource-table output, lifetime and complete-path costs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldmaterialdata.h"
#include "../almetalworldresources.h"
#include <cstdlib>
#include <filesystem>
#include <time.h>

namespace
{
struct WorldResourceFixture : Fixture
{
    static constexpr unsigned slots = 8;
    struct Mesh
    {
        float positions[slots * 3][4]{};
        float normals[slots * 3][4]{};
        float uv[slots * 3][2]{};
        std::uint8_t colors[slots * 3][4]{};
        float tangents[slots * 3][4]{};
    };
    const ALMetalBuffer* tiled_vertices = nullptr;
    const ALMetalBuffer* tiled_indices = nullptr;
    const ALMetalBuffer* single_vertices = nullptr;
    std::array<const ALMetalTexture*, slots> diffuse{}, normals{}, specular{}, orm_maps{}, emissive{};

    WorldResourceFixture()
    {
        Mesh mesh;
        std::array<std::uint16_t, slots * 3> index{};
        for (unsigned slot = 0; slot < slots; ++slot)
        {
            const float center_x = -.75f + .5f * (slot % 4);
            const float center_y = slot < 4 ? -.5f : .5f;
            for (unsigned corner = 0; corner < 3; ++corner)
            {
                const unsigned vertex = slot * 3 + corner;
                mesh.positions[vertex][0] = geometry.positions[corner][0] * .24f + center_x;
                mesh.positions[vertex][1] = geometry.positions[corner][1] * .42f + center_y;
                mesh.positions[vertex][2] = .25f;
                const std::int32_t packed_slot = slot;
                std::memcpy(&mesh.positions[vertex][3], &packed_slot, sizeof(packed_slot));
                mesh.normals[vertex][2] = 1.f;
                mesh.uv[vertex][0] = corner == 1 ? 1.75f : -.25f;
                mesh.uv[vertex][1] = corner == 2 ? 1.5f : -.2f;
                std::fill_n(mesh.colors[vertex], 4, std::uint8_t{255});
                mesh.tangents[vertex][0] = mesh.tangents[vertex][3] = 1.f;
                index[vertex] = vertex;
            }
            diffuse[slot] = patternedTexture(slot, 0);
            normals[slot] = patternedTexture(slot, 1);
            specular[slot] = patternedTexture(slot, 2);
            orm_maps[slot] = patternedTexture(slot, 3);
            emissive[slot] = patternedTexture(slot, 4);
            // A wrong argument-table texture view must produce a different
            // value: normal/ORM assets intentionally carry sRGB metadata.
            REQUIRE(normals[slot]->nativeLinearHandle() != normals[slot]->nativeHandle());
            REQUIRE(orm_maps[slot]->nativeLinearHandle() != orm_maps[slot]->nativeHandle());
        }
        tiled_vertices = buffer(&mesh, sizeof(mesh));
        tiled_indices = buffer(index.data(), sizeof(index));
        auto single = geometry;
        for (unsigned corner = 0; corner < 4; ++corner)
        {
            single.positions[corner][3] = 0.f;
            single.uv[corner][0] = corner == 1 ? 1.75f : -.25f;
            single.uv[corner][1] = corner == 2 ? 1.5f : -.2f;
            std::fill_n(single.colors[corner], 4, std::uint8_t{255});
        }
        single_vertices = buffer(&single, sizeof(single));
    }

    const ALMetalTexture* patternedTexture(unsigned slot, unsigned role)
    {
        std::array<std::uint8_t, 16> pixels{};
        for (unsigned texel = 0; texel < 4; ++texel)
        {
            auto* value = pixels.data() + texel * 4;
            value[0] = 30 + slot * 23 + texel * 4;
            value[1] = 215 - slot * 17 - texel * 5;
            value[2] = 65 + slot * 11 + texel * 8;
            value[3] = slot % 3 == 0 ? 80 : 240;
            if (role == 1)
            {
                value[0] = 105 + slot * 4 + texel * 2;
                value[1] = 120 + texel * 5;
                value[2] = 235;
                value[3] = 255;
            }
            if (role == 3)
            {
                value[0] = 240;
                value[1] = 65 + slot * 20 + texel * 4;
                value[2] = 210 - slot * 17;
                value[3] = 255;
            }
        }
        return texture(ALMetalPixelFormat::rgba8_unorm_srgb, 2, pixels.data(), 8);
    }

    ALMetalWorldSimpleBatch material(unsigned family, unsigned count, bool masked = false)
    {
        REQUIRE((family == 1 || family == 2));
        REQUIRE((count == 1 || count == slots));
        auto batch = Fixture::batch(family);
        batch.vertices = count == 1 ? single_vertices : tiled_vertices;
        if (count == slots)
        {
            batch.indices = tiled_indices;
            batch.vertex_count = slots * 3;
            batch.index_count = slots * 3;
            batch.normal_offset = offsetof(Mesh, normals);
            batch.texcoord_offset = offsetof(Mesh, uv);
            batch.color_offset = offsetof(Mesh, colors);
            batch.tangent_offset = offsetof(Mesh, tangents);
        }
        batch.texcoord1_offset = batch.texcoord2_offset = batch.texcoord_offset;
        batch.texture_count = count;
        batch.material_slot_count = family == 1 ? count : 0;
        batch.material_alpha_mode = family == 1 && masked ? 2 : 0;
        batch.pbr_slot_count = family == 2 ? count : 0;
        batch.alpha_mask = family == 2 && masked;
        for (unsigned slot = 0; slot < count; ++slot)
        {
            batch.diffuse_textures[slot] = diffuse[slot];
            if (family == 1)
            {
                auto& value = batch.materialSlots()[slot];
                value.normal_texture = normals[slot];
                value.specular_texture = specular[slot];
                value.specular_color[0] = .1f + .1f * slot;
                value.env_intensity = .05f * slot;
                value.alpha_cutoff = .5f;
            }
            else
            {
                auto& value = batch.pbrSlots()[slot];
                value.normal_texture = normals[slot];
                value.orm_texture = orm_maps[slot];
                value.emissive_texture = emissive[slot];
                value.roughness_factor = .2f + .08f * slot;
                value.metallic_factor = .8f - .08f * slot;
                value.emissive_color[0] = .1f + .03f * slot;
                value.emissive_color[1] = .15f;
                value.alpha_cutoff = masked ? .5f : -1.f;
            }
        }
        return batch;
    }
};

struct WorldResourceOutput
{
    Pixels pixels;
    std::vector<std::uint64_t> queries;
};

WorldResourceOutput renderResourceView(WorldResourceFixture& f,
    ALMetalWorldGBufferRenderer& renderer, ALMetalWorldGBufferScene scene,
    std::vector<ALMetalWorldSimpleBatch> batches,
    std::vector<ALMetalWorldVisibilityQuery> queries, bool prepared, std::uint64_t frame)
{
    f.data->beginFrame(frame);
    scene.source_owner = f.resources;
    scene.batches = batches.data(); scene.batch_count = batches.size();
    scene.visibility_queries = queries.data(); scene.visibility_query_count = queries.size();
    std::shared_ptr<ALMetalVisibilityResultBatch> results;
    std::unique_ptr<ALMetalRenderTarget> target;
    if (prepared)
    {
        auto ready = f.prepare(scene, std::move(batches), std::move(queries));
        target = renderer.renderPrepared(*ready, &f.error, &results);
    }
    else target = renderer.render(scene, &f.error, &results);
    INFO(f.error); REQUIRE(target);
    WorldResourceOutput output{f.read(*target), {}};
    if (results)
    {
        output.queries.resize(results->resultCount());
        for (std::size_t query = 0; query < output.queries.size(); ++query)
            REQUIRE(results->wait(query, output.queries[query], &f.error));
    }
    return output;
}

std::vector<ALMetalWorldVisibilityQuery> resourceQueries(unsigned count)
{
    std::vector<ALMetalWorldVisibilityQuery> queries(3);
    for (unsigned index = 0; index < queries.size(); ++index)
    {
        auto& query = queries[index];
        identity(query.modelview_projection);
        query.center[0] = count == 1 ? 0.f : -.25f;
        query.center[1] = count == 1 ? -.1f : -.5f;
        query.center[2] = .75f;
        // At the 32 px fixture size these centers lie between pixel centers.
        // Cover the adjacent samples while keeping the box behind geometry.
        query.half_extent[0] = query.half_extent[1] = query.half_extent[2] = .08f;
        query.fan = 7; query.count_samples = 1;
        query.batch_offset = index;
    }
    return queries;
}

void publishResourceBatch(WorldResourceFixture& f, ALMetalWorldSimpleBatch& batch,
    ALMetalWorldMaterialDataCache& material_cache)
{
    REQUIRE(ALMetalWorldMaterialSource::publish(*f.device, batch, material_cache, &f.error));
    auto source = ALMetalWorldResourceSource::publish(*f.device, batch, f.resources->textures, &f.error);
    INFO(f.error); REQUIRE(source); REQUIRE(source->matches(batch));
    REQUIRE(source == batch.native_resource_source);
}

void checkResourcePath(const ALMetalWorldResourceStats& before,
    const ALMetalWorldResourceStats& after, unsigned draws)
{
    CHECK(after.requested_draws - before.requested_draws == draws);
    CHECK(after.bound_draws - before.bound_draws == draws);
    CHECK(after.fallback_draws == before.fallback_draws);
    CHECK(after.buffer_bind_calls - before.buffer_bind_calls >= draws);
    CHECK(after.declared_resources > before.declared_resources);
}
}

TEST_CASE("almetal: world resource tables preserve visible indexed and query boundaries",
          "[almetal][world-resources]")
{
    @autoreleasepool
    {
        WorldResourceFixture f;
        ALMetalWorldMaterialDataCache material_cache(*f.device);
        std::uint64_t frame = 1;
        for (unsigned family : {1u, 2u})
        for (unsigned count : {1u, 8u})
        for (bool masked : {false, true})
        {
            CAPTURE(family, count, masked);
            auto first = f.material(family, count, masked);
            publishResourceBatch(f, first, material_cache);
            auto second = first;
            for (unsigned slot = 0; slot < count; ++slot)
                second.diffuse_textures[slot] = f.diffuse[(slot + 1) % f.slots];
            CHECK_FALSE(first.native_resource_source->matches(second));
            publishResourceBatch(f, second, material_cache);
            for (unsigned format = 0; format < 8; ++format)
            {
                CAPTURE(format);
                auto scene = f.scene();
                scene.normal_hdr = format & 1;
                scene.hdr = format & 2;
                scene.reverse_z = format & 4;
                const auto expected = renderResourceView(f, *f.renderer, scene,
                    {first, second}, resourceQueries(count), false, ++frame);
                REQUIRE(expected.queries.size() == 3);
                CHECK(expected.queries.front() > 0);
                const auto prepared = renderResourceView(f, *f.renderer, scene,
                    {first, second}, resourceQueries(count), true, ++frame);
                CHECK(prepared.pixels == expected.pixels);
                CHECK(prepared.queries == expected.queries);
                scene.material_argument_buffers = true;
                for (bool prepare : {false, true})
                {
                    const auto before = f.renderer->worldResourceStats();
                    const auto actual = renderResourceView(f, *f.renderer, scene,
                        {first, second}, resourceQueries(count), prepare, ++frame);
                    CHECK(actual.pixels == expected.pixels);
                    CHECK(actual.queries == expected.queries);
                    checkResourcePath(before, f.renderer->worldResourceStats(), 2);
                }
                if (!masked && format == 0)
                {
                    // Both commands have exactly equal depth. The later
                    // source's resource bindings must survive that boundary.
                    const auto last = renderResourceView(f, *f.renderer, scene,
                        {second}, {}, false, ++frame);
                    const auto original = renderResourceView(f, *f.renderer, scene,
                        {first}, {}, false, ++frame);
                    CHECK(last.pixels == expected.pixels);
                    CHECK_FALSE(original.pixels == expected.pixels);
                }
            }
            const auto before_sampler = f.renderer->worldResourceStats();
            const auto current_anisotropy = f.renderer->anisotropy();
            REQUIRE(f.renderer->setAnisotropy(current_anisotropy == 4 ? 8 : 4, &f.error));
            auto scene = f.scene(); scene.hdr = true;
            const auto direct = renderResourceView(f, *f.renderer, scene, {first, second}, {}, false, ++frame);
            scene.material_argument_buffers = true;
            const auto edited_sampler = renderResourceView(f, *f.renderer, scene, {first, second}, {}, true, ++frame);
            CHECK(edited_sampler.pixels == direct.pixels);
            CHECK(f.renderer->worldResourceStats().table_creates > before_sampler.table_creates);
        }
    }
}

TEST_CASE("almetal: world resource generations survive pending GPU work and retire on settlement",
          "[almetal][world-resources]")
{
    @autoreleasepool
    {
        for (unsigned family : {1u, 2u})
        for (bool abandon : {false, true})
        {
            CAPTURE(family, abandon);
            WorldResourceFixture f;
            ALMetalWorldMaterialDataCache material_cache(*f.device);
            auto batch = f.material(family, 8);
            publishResourceBatch(f, batch, material_cache);
            auto scene = f.scene(); scene.hdr = true;
            const auto expected = renderResourceView(f, *f.renderer, scene, {batch}, {}, false, 2).pixels;
            auto old_source = batch.native_resource_source;
            std::weak_ptr<const ALMetalWorldResourceSource> weak_source = old_source;
            std::weak_ptr<const ALMetalTexture> weak_texture;
            for (const auto& lease : f.resources->textures)
                if (lease.get() == f.diffuse[0]) weak_texture = lease;
            REQUIRE_FALSE(weak_texture.expired());
            // Upload before closing the GPU gate; no readback/upload may wait
            // behind our deliberately pending command.
            const auto* fresh = f.patternedTexture(7, 0);
            auto changed = batch; changed.diffuse_textures[0] = fresh;
            if (family == 1) changed.materialSlots()[0].env_intensity = .8f;
            else changed.pbrSlots()[0].roughness_factor = .9f;
            CHECK_FALSE(old_source->matches(changed));
            publishResourceBatch(f, changed, material_cache);
            CHECK(changed.native_resource_source != old_source);

            // Geometry ownership deliberately excludes textures. The resource
            // certificate/table must supply their exact C++ generation leases.
            auto geometry = std::make_shared<Resources>();
            geometry->buffers = f.resources->buffers;
            scene.source_owner = geometry;
            scene.material_argument_buffers = true;
            scene.batches = &batch; scene.batch_count = 1;
            id<MTLSharedEvent> gate = [(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];
            REQUIRE(gate);
            struct ReleaseGate { id<MTLSharedEvent> value; ~ReleaseGate() { value.signaledValue = 1; } } release{gate};
            if (!abandon)
            {
                auto blocker = f.device->makeCommandBuffer("Hold world resource generation", &f.error);
                REQUIRE(blocker);
                [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];
                REQUIRE(blocker->commit(&f.error));
            }
            REQUIRE(f.renderer->beginCommandBatch("World resource generation settlement", &f.error));
            const auto before = f.renderer->worldResourceStats();
            std::unique_ptr<ALMetalRenderTarget> target;
            if (family == 2)
            {
                auto inputs = ALMetalWorldViewInputs::adopt(scene, {batch}, {}, geometry, 3, &f.error);
                REQUIRE(inputs);
                auto work = ALMetalWorldPreparation::create(inputs, 1, {}, &f.error);
                REQUIRE(work); REQUIRE(work->runJob(0));
                auto ready = work->finish(&f.error); REQUIRE(ready);
                target = f.renderer->renderPrepared(*ready, &f.error);
            }
            else target = f.renderer->render(scene, &f.error);
            INFO(f.error); REQUIRE(target);
            checkResourcePath(before, f.renderer->worldResourceStats(), 1);
            auto completion = f.renderer->commandBatchCompletion(); REQUIRE(completion);
            CHECK_FALSE(completion->isComplete());
            if (!abandon)
            {
                REQUIRE(f.renderer->commitCommandBatch(&f.error));
                REQUIRE(f.renderer->setAnisotropy(8, &f.error));
            }
            f.renderer->clearWorldResourceLookup();
            batch.native_resource_source.reset(); old_source.reset();
            f.resources->textures.clear();
            CHECK_FALSE(weak_source.expired());
            CHECK_FALSE(weak_texture.expired());
            if (abandon) f.renderer->cancelCommandBatch();
            else
            {
                CHECK_FALSE(completion->isComplete());
                gate.signaledValue = 1;
                REQUIRE(f.device->waitUntilIdle(&f.error));
                CHECK(f.read(*target) == expected);
                CHECK(completion->isComplete());
            }
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while ((!weak_source.expired() || !weak_texture.expired()) &&
                   std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
            CHECK(weak_source.expired()); CHECK(weak_texture.expired());

            // A new context/sampler generation must encode the edited source,
            // while the completed old pixels remain owned by the old target.
            scene = f.scene(); scene.hdr = true; scene.material_argument_buffers = true;
            const auto warm = renderResourceView(f, *f.renderer, scene, {changed}, {}, false, 4);
            if (!abandon)
            {
                // Reload initializes unrelated lookup textures with ordered
                // commitAndWait uploads; never hold our synthetic GPU gate.
                const auto shader = f.renderer->shaderGeneration();
                REQUIRE(f.renderer->reloadShaders(&f.error));
                CHECK(f.renderer->shaderGeneration() > shader);
            }
            scene.material_argument_buffers = false;
            const auto manual = renderResourceView(f, *f.renderer, scene, {changed}, {}, false, 5);
            scene.material_argument_buffers = true;
            const auto new_before = f.renderer->worldResourceStats();
            const auto actual = renderResourceView(f, *f.renderer, scene, {changed}, {}, true, 6);
            CHECK(actual.pixels == manual.pixels);
            CHECK(actual.pixels == warm.pixels);
            CHECK_FALSE(actual.pixels == expected);
            checkResourcePath(new_before, f.renderer->worldResourceStats(), 1);
            if (!abandon) CHECK(f.renderer->worldResourceStats().table_creates > new_before.table_creates);
        }
    }
}

TEST_CASE("almetal: rigged world resource pipelines preserve single material output",
          "[almetal][world-resources]")
{
    @autoreleasepool
    {
        WorldResourceFixture f;
        ALMetalWorldMaterialDataCache material_cache(*f.device);
        std::uint64_t frame = 1;
        for (unsigned family : {1u, 2u})
        {
            auto batch = f.material(family, 1);
            batch.weight_offset = offsetof(Fixture::Geometry, weights);
            batch.skin_palette = f.attachment_pose.data();
            batch.skin_palette_float_count = f.attachment_pose.size();
            batch.skin_joint_count = 1;
            publishResourceBatch(f, batch, material_cache);
            for (unsigned format = 0; format < 8; ++format)
            {
                CAPTURE(family, format);
                auto scene = f.scene(); scene.normal_hdr = format & 1;
                scene.hdr = format & 2; scene.reverse_z = format & 4;
                const auto expected = renderResourceView(f, *f.renderer, scene, {batch}, {}, false, ++frame);
                CHECK(std::any_of(expected.pixels.depth.begin(), expected.pixels.depth.end(),
                    [reverse = scene.reverse_z](auto value) { return value != (reverse ? 0u : 0x3f800000u); }));
                const auto prepared = renderResourceView(f, *f.renderer, scene, {batch}, {}, true, ++frame);
                CHECK(prepared.pixels == expected.pixels);
                scene.material_argument_buffers = true;
                for (bool prepare : {false, true})
                {
                    const auto before = f.renderer->worldResourceStats();
                    const auto actual = renderResourceView(f, *f.renderer, scene, {batch}, {}, prepare, ++frame);
                    CHECK(actual.pixels == expected.pixels);
                    checkResourcePath(before, f.renderer->worldResourceStats(), 1);
                }
            }
        }
    }
}

TEST_CASE("almetal: optional world resource failures preserve exact direct rendering",
          "[almetal][world-resources]")
{
    @autoreleasepool
    {
        WorldResourceFixture f;
        ALMetalWorldMaterialDataCache material_cache(*f.device);
        std::uint64_t frame = 1;
        for (unsigned family : {1u, 2u})
        {
            auto batch = f.material(family, 8, true);
            publishResourceBatch(f, batch, material_cache);
            auto scene = f.scene(); scene.hdr = true;
            const auto expected = renderResourceView(f, *f.renderer, scene, {batch}, {}, false, ++frame);
            scene.material_argument_buffers = true;
            // Warm once so fault injection must also override a fast hit.
            renderResourceView(f, *f.renderer, scene, {batch}, {}, false, ++frame);
            for (unsigned failure = 0; failure < 4; ++failure)
            for (bool prepared : {false, true})
            {
                CAPTURE(family, failure, prepared);
                auto candidate = batch;
                if (failure == 0) candidate.native_resource_source.reset();
                if (failure == 1)
                {
                    candidate.diffuse_textures[0] = f.diffuse[7];
                    CHECK_FALSE(candidate.native_resource_source->matches(candidate));
                }
                if (failure == 2) f.renderer->configureWorldResources({0, 0});
                if (failure == 3) f.renderer->failNextWorldResourcePreparationForTesting();
                const auto before = f.renderer->worldResourceStats();
                auto direct_scene = scene; direct_scene.material_argument_buffers = false;
                const auto reference = failure == 1
                    ? renderResourceView(f, *f.renderer, direct_scene, {candidate}, {}, prepared, ++frame) : expected;
                const auto actual = renderResourceView(f, *f.renderer, scene, {candidate}, {}, prepared, ++frame);
                CHECK(actual.pixels == reference.pixels);
                const auto after = f.renderer->worldResourceStats();
                CHECK(after.requested_draws - before.requested_draws == 1);
                CHECK(after.bound_draws == before.bound_draws);
                CHECK(after.fallback_draws - before.fallback_draws == 1);
                constexpr std::array reasons{ALMetalWorldResourceFallback::missing_source,
                    ALMetalWorldResourceFallback::unmatched_source, ALMetalWorldResourceFallback::capacity,
                    ALMetalWorldResourceFallback::injected};
                const auto reason = static_cast<std::size_t>(reasons[failure]);
                CHECK(after.last_fallback == reasons[failure]);
                CHECK(after.fallback_reasons[reason] - before.fallback_reasons[reason] == 1);
                if (failure == 2) f.renderer->configureWorldResources({});
            }
            auto unowned = batch; unowned.diffuse_textures[0] = f.diffuse[7];
            ALMetalWorldResourceFallback failure = ALMetalWorldResourceFallback::none;
            CHECK_FALSE(ALMetalWorldResourceSource::publish(*f.device, unowned, {}, &f.error, &failure));
            CHECK(failure == ALMetalWorldResourceFallback::validation);
            CHECK(unowned.native_resource_source == batch.native_resource_source);
            if (family == 1)
                for (unsigned maps = 0; maps < 3; ++maps)
                {
                    CAPTURE(maps);
                    auto optional_maps = batch;
                    for (unsigned slot = 0; slot < optional_maps.material_slot_count; ++slot)
                    {
                        if (!(maps & 1)) optional_maps.materialSlots()[slot].normal_texture = nullptr;
                        if (!(maps & 2)) optional_maps.materialSlots()[slot].specular_texture = nullptr;
                    }
                    publishResourceBatch(f, optional_maps, material_cache);
                    auto direct_scene = scene; direct_scene.material_argument_buffers = false;
                    const auto reference = renderResourceView(f, *f.renderer, direct_scene, {optional_maps}, {}, false, ++frame);
                    for (bool prepared : {false, true})
                    {
                        const auto resource_before = f.renderer->worldResourceStats();
                        const auto actual = renderResourceView(f, *f.renderer, scene, {optional_maps}, {}, prepared, ++frame);
                        CHECK(actual.pixels == reference.pixels);
                        checkResourcePath(resource_before, f.renderer->worldResourceStats(), 1);
                    }
                }
            auto invalid = batch; invalid.pbr_slot_count = 9;
            scene.batches = &invalid; scene.batch_count = 1;
            const auto before = f.renderer->worldResourceStats();
            CHECK_FALSE(f.renderer->render(scene, &f.error));
            CHECK(f.renderer->worldResourceStats().bound_draws == before.bound_draws);
        }
    }
}

TEST_CASE("almetal: argument resource declarations preserve adjacent native indexed joining",
          "[almetal][world-resources]")
{
    @autoreleasepool
    {
        WorldResourceFixture f;
        ALMetalWorldMaterialDataCache material_cache(*f.device);
        struct TimingRestore
        {
            bool old = ALMetalDevice::commandTimingEnabled();
            ~TimingRestore() { ALMetalDevice::setCommandTimingEnabled(old); }
        } timing;
        ALMetalDevice::setCommandTimingEnabled(true);
        std::array<std::uint16_t, WorldResourceFixture::slots * 3 * 2> indices;
        for (unsigned index = 0; index < indices.size(); ++index) indices[index] = index % (f.slots * 3);
        const auto* joined_indices = f.buffer(indices.data(), sizeof(indices));
        std::uint64_t frame = 1;
        for (unsigned family : {1u, 2u})
        {
            auto first = f.material(family, 8);
            first.indices = joined_indices;
            publishResourceBatch(f, first, material_cache);
            auto second = first; second.index_offset = first.index_count * sizeof(std::uint16_t);
            auto scene = f.scene(); scene.hdr = true; scene.join_adjacent_static_draws = true;
            const auto before = ALMetalDevice::encodedDrawCallCount();
            const auto expected = renderResourceView(f, *f.renderer, scene, {first, second}, {}, false, ++frame);
            REQUIRE(ALMetalDevice::encodedDrawCallCount() - before == 1);
            scene.material_argument_buffers = true;
            for (bool prepared : {false, true})
            {
                const auto draws_before = ALMetalDevice::encodedDrawCallCount();
                const auto resource_before = f.renderer->worldResourceStats();
                const auto actual = renderResourceView(f, *f.renderer, scene, {first, second}, {}, prepared, ++frame);
                CHECK(actual.pixels == expected.pixels);
                CHECK(ALMetalDevice::encodedDrawCallCount() - draws_before == 1);
                checkResourcePath(resource_before, f.renderer->worldResourceStats(), 2);
            }
        }
    }
}

TEST_CASE("almetal: world resources preserve the previous compiled scalar shader",
          "[almetal][.world-resources-previous-shader]")
{
    @autoreleasepool
    {
        const char* baseline = std::getenv("ALCHEMY_WORLD_RESOURCE_BASELINE_LIBRARY");
        REQUIRE(baseline);
        const std::filesystem::path path(baseline);
        REQUIRE(path.is_absolute()); REQUIRE(std::filesystem::is_regular_file(path));
        WorldResourceFixture f;
        auto previous_library = ALMetalShaderLibrary::loadFile(*f.device, path.string(), &f.error);
        INFO(f.error); REQUIRE(previous_library);
        // Root independently records the staged69008 path/hash. Also reject
        // accidentally selecting our new library in this explicit old-code
        // comparison: it would repeat the same shared-helper comparison.
        REQUIRE_FALSE(previous_library->hasFunction("almetal_world_material_resources_fragment"));
        REQUIRE_FALSE(previous_library->hasFunction("almetal_world_pbr_resources_fragment"));
        auto previous = ALMetalWorldGBufferRenderer::create(*f.device, &f.error);
        REQUIRE(previous); previous->setFrameData(f.data);
        REQUIRE(previous->reloadShaders(path.string(), &f.error));
        REQUIRE(previous->setAnisotropy(4, &f.error));
        REQUIRE(f.renderer->setAnisotropy(4, &f.error));
        ALMetalWorldMaterialDataCache material_cache(*f.device);
        std::uint64_t frame = 1;
        for (unsigned family : {1u, 2u})
        for (unsigned count : {1u, 8u})
        for (bool masked : {false, true})
        {
            CAPTURE(family, count, masked);
            auto first = f.material(family, count, masked);
            publishResourceBatch(f, first, material_cache);
            auto second = first;
            for (unsigned slot = 0; slot < count; ++slot)
                second.diffuse_textures[slot] = f.diffuse[(slot + 1) % f.slots];
            publishResourceBatch(f, second, material_cache);
            for (unsigned format = 0; format < 8; ++format)
            {
                CAPTURE(format);
                auto scene = f.scene(); scene.normal_hdr = format & 1;
                scene.hdr = format & 2; scene.reverse_z = format & 4;
                const auto old = renderResourceView(f, *previous, scene,
                    {first, second}, resourceQueries(count), false, ++frame);
                const auto direct = renderResourceView(f, *f.renderer, scene,
                    {first, second}, resourceQueries(count), false, ++frame);
                CHECK(direct.pixels == old.pixels);
                CHECK(direct.queries == old.queries);
                scene.material_argument_buffers = true;
                for (bool prepared : {false, true})
                {
                    const auto before = f.renderer->worldResourceStats();
                    const auto actual = renderResourceView(f, *f.renderer, scene,
                        {first, second}, resourceQueries(count), prepared, ++frame);
                    CHECK(actual.pixels == old.pixels);
                    CHECK(actual.queries == old.queries);
                    checkResourcePath(before, f.renderer->worldResourceStats(), 2);
                }
            }
        }
        std::cout << "world_resources_previous_shader baseline_entrypoints_absent=1"
                     " family_slot_mask_format_tuples=64 old_direct_new_direct_new_argument=1\n";
    }
}

TEST_CASE("almetal: world resource complete path cold warm and churn costs",
          "[almetal][.world-resources-cost]")
{
    @autoreleasepool
    {
        WorldResourceFixture f;
        auto probes = ALMetalWorldGBufferRenderer::create(*f.device, &f.error);
        REQUIRE(probes); probes->setFrameData(f.data);
        REQUIRE(f.renderer->setAnisotropy(4, &f.error));
        REQUIRE(probes->setAnisotropy(4, &f.error));
        ALMetalWorldMaterialDataCache material_cache(*f.device);
        std::uint64_t serial = 1;
        const auto cpu_ns = []
        {
            timespec time{}; REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) == 0);
            return static_cast<std::uint64_t>(time.tv_sec) * 1'000'000'000ULL + time.tv_nsec;
        };
        const auto total_stats = [&]
        {
            auto result = f.renderer->worldResourceStats();
            const auto other = probes->worldResourceStats();
            result.requests += other.requests; result.cache_hits += other.cache_hits;
            result.table_creates += other.table_creates; result.published_bytes += other.published_bytes;
            result.argument_encoder_creations += other.argument_encoder_creations;
            result.declared_resources += other.declared_resources; result.buffer_bind_calls += other.buffer_bind_calls;
            result.bound_draws += other.bound_draws; result.fallback_draws += other.fallback_draws;
            return result;
        };
        const auto exact_leases = [&](const ALMetalWorldSimpleBatch& batch)
        {
            std::vector<std::shared_ptr<const ALMetalTexture>> leases;
            const auto add = [&](const ALMetalTexture* texture)
            {
                if (std::any_of(leases.begin(), leases.end(), [&](const auto& value) { return value.get() == texture; })) return;
                const auto at = std::find_if(f.resources->textures.begin(), f.resources->textures.end(),
                    [&](const auto& value) { return value.get() == texture; });
                REQUIRE(at != f.resources->textures.end()); leases.push_back(*at);
            };
            for (unsigned slot = 0; slot < batch.texture_count; ++slot)
            {
                add(batch.diffuse_textures[slot]);
                if (batch.material_slot_count)
                { add(batch.materialSlots()[slot].normal_texture); add(batch.materialSlots()[slot].specular_texture); }
                else
                { add(batch.pbrSlots()[slot].normal_texture); add(batch.pbrSlots()[slot].orm_texture); add(batch.pbrSlots()[slot].emissive_texture); }
            }
            return leases;
        };
        const auto resource_key_count = [](const auto& sources, unsigned used)
        {
            // Within one view the family, sampler and other maps are fixed.
            // Count actual diffuse native-view tuples, not uniform variants.
            std::vector<std::array<void*, 8>> keys;
            for (unsigned variant = 0; variant < used; ++variant)
            {
                std::array<void*, 8> key{};
                for (unsigned slot = 0; slot < sources[variant].texture_count; ++slot)
                    key[slot] = sources[variant].diffuse_textures[slot]->nativeHandle();
                if (std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(key);
            }
            return keys.size();
        };
        constexpr unsigned samples = 12, warmups = 2;
        // New allocations have identical texels, but genuinely distinct native
        // generations. Upload and owner-span construction are outside timing.
        std::array<std::array<const ALMetalTexture*, 8>, samples + warmups> churn;
        for (auto& generation : churn)
            for (unsigned variant = 0; variant < generation.size(); ++variant)
                generation[variant] = f.patternedTexture(variant, 0);

        for (unsigned family : {1u, 2u})
        for (unsigned slots : {1u, 8u})
        for (bool distinct_resources : {false, true})
        {
            std::array<ALMetalWorldSimpleBatch, 8> prototypes;
            for (unsigned variant = 0; variant < prototypes.size(); ++variant)
            {
                prototypes[variant] = f.material(family, slots);
                if (distinct_resources)
                    for (unsigned slot = 0; slot < slots; ++slot)
                        prototypes[variant].diffuse_textures[slot] = f.diffuse[(slot + variant) % f.slots];
                if (family == 1) prototypes[variant].materialSlots()[0].env_intensity = .05f * variant;
                else prototypes[variant].pbrSlots()[0].roughness_factor = .1f + .1f * variant;
            }
            const auto uniform_before = material_cache.stats().published_bytes;
            const auto uniform_start = cpu_ns();
            for (auto& prototype : prototypes)
                REQUIRE(ALMetalWorldMaterialSource::publish(*f.device, prototype, material_cache, &f.error));
            const auto uniform_cpu = cpu_ns() - uniform_start;
            const auto uniform_after = material_cache.stats().published_bytes;
            std::array<std::vector<std::shared_ptr<const ALMetalTexture>>, 8> base_leases;
            for (unsigned variant = 0; variant < prototypes.size(); ++variant)
                base_leases[variant] = exact_leases(prototypes[variant]);
            std::array<std::array<std::vector<std::shared_ptr<const ALMetalTexture>>, 8>, samples + warmups> churn_leases;
            for (unsigned sample = 0; sample < churn.size(); ++sample)
                for (unsigned variant = 0; variant < prototypes.size(); ++variant)
                {
                    auto changed = prototypes[variant];
                    changed.diffuse_textures[0] = churn[sample][distinct_resources ? variant : 0];
                    churn_leases[sample][variant] = exact_leases(changed);
                }
            std::cout << "world_resources_setup family=" << family << " slots=" << slots
                << " resource_pattern=" << (distinct_resources ? "distinct_texture_tuples" : "shared_texture_tuple")
                << " uniform_publication_cpu_ms=" << uniform_cpu / 1e6
                << " uniform_publication_bytes=" << uniform_after - uniform_before
                << " source_variants_available=8 resource_keys_available_per_view=" << resource_key_count(prototypes, 8)
                << " churn_texture_generations_precreated=112 owner_spans_prebuilt=1"
                   " renderer_owners=2 shared_frame_data=1 encode_clock=host_wall cpu_clock=owner_thread\n";
            constexpr std::array main_counts{1u, 4u, 16u, 512u, 512u};
            for (unsigned shape = 0; shape < main_counts.size(); ++shape)
            for (unsigned pattern = 0; pattern < 3; ++pattern)
            {
                std::array<Pixels, 3> reference;
                for (bool arguments : {false, true})
                {
                    const unsigned main_count = main_counts[shape];
                    const unsigned views = shape == main_counts.size() - 1 ? 3 : 1;
                    const unsigned used_sources = std::min<unsigned>(prototypes.size(), main_count);
                    const std::array<unsigned, 3> counts{main_count, 128, 128};
                    auto sources = prototypes;
                    const auto keys_per_view = resource_key_count(sources, used_sources);
                    f.renderer->clearWorldResourceLookup(); probes->clearWorldResourceLookup();
                    const auto setup_start = cpu_ns();
                    if (arguments && pattern == 1)
                        for (unsigned variant = 0; variant < used_sources; ++variant)
                            REQUIRE(ALMetalWorldResourceSource::publish(*f.device, sources[variant], base_leases[variant], &f.error));
                    const auto resource_setup_cpu = cpu_ns() - setup_start;
                    std::array<std::vector<ALMetalWorldSimpleBatch>, 3> capture;
                    std::array<std::vector<ALMetalWorldVisibilityQuery>, 3> queries;
                    for (unsigned view = 0; view < views; ++view) capture[view].reserve(counts[view]);
                    std::array<double, samples> encode_ms{}, cpu_ms{}, complete_ms{}, tail_ms{};
                    std::uint64_t source_cpu_total = 0, table_bytes = 0, table_creates = 0, hits = 0, binds = 0, declared = 0;
                    for (unsigned sample = 0; sample < samples + warmups; ++sample)
                    {
                        std::array<std::unique_ptr<ALMetalRenderTarget>, 3> targets;
                        const auto before = total_stats();
                        const auto begin = std::chrono::steady_clock::now();
                        const auto owner_cpu_start = cpu_ns();
                        ALMetalWorldResourceStats view_work;
                        // This is the outer frame: all view transactions share
                        // one epoch and its native data-page lifetime.
                        f.data->beginFrame(++serial);
                        if (arguments && pattern == 0)
                        {
                            for (unsigned variant = 0; variant < used_sources; ++variant)
                                sources[variant].native_resource_source.reset();
                            f.renderer->clearWorldResourceLookup(); probes->clearWorldResourceLookup();
                        }
                        if (pattern == 2)
                            for (unsigned variant = 0; variant < used_sources; ++variant)
                                sources[variant].diffuse_textures[0] = churn[sample][distinct_resources ? variant : 0];
                        const auto source_cpu_start = cpu_ns();
                        if (arguments && pattern != 1)
                            for (unsigned variant = 0; variant < used_sources; ++variant)
                                REQUIRE(ALMetalWorldResourceSource::publish(*f.device, sources[variant],
                                    pattern == 2 ? churn_leases[sample][variant] : base_leases[variant], &f.error));
                        const auto source_cpu = cpu_ns() - source_cpu_start;
                        for (unsigned view = 0; view < views; ++view)
                        {
                            auto& batches = capture[view];
                            const float camera = .001f * sample + .04f * view;
                            for (unsigned draw = 0; draw < counts[view]; ++draw)
                            {
                                batches.push_back(sources[draw % sources.size()]);
                                batches.back().modelview_projection[12] = camera;
                                batches.back().modelview[12] = camera;
                            }
                            auto scene = f.scene(); scene.hdr = true;
                            scene.reverse_z = sample & 1;
                            scene.material_argument_buffers = arguments;
                            scene.source_owner = f.resources;
                            auto inputs = ALMetalWorldViewInputs::adopt(scene, std::move(batches),
                                std::move(queries[view]), f.resources, serial, &f.error);
                            INFO(f.error); REQUIRE(inputs);
                            scene = inputs->scene(); scene.source_owner = inputs->resourceLease();
                            auto& renderer = view == 0 ? *f.renderer : *probes;
                            const auto view_before = renderer.worldResourceStats();
                            REQUIRE(renderer.beginCommandBatch("Complete world resource view", &f.error));
                            targets[view] = renderer.render(scene, &f.error);
                            INFO(f.error); REQUIRE(targets[view]);
                            REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs, batches, queries[view]));
                            REQUIRE(renderer.commitCommandBatch(&f.error));
                            const auto view_after = renderer.worldResourceStats();
                            // Context counters may be shared between renderer
                            // owners. Serial per-view deltas count each event
                            // once; summing outer context snapshots would not.
                            view_work.published_bytes += view_after.published_bytes - view_before.published_bytes;
                            view_work.table_creates += view_after.table_creates - view_before.table_creates;
                            view_work.cache_hits += view_after.cache_hits - view_before.cache_hits;
                            view_work.buffer_bind_calls += view_after.buffer_bind_calls - view_before.buffer_bind_calls;
                            view_work.declared_resources += view_after.declared_resources - view_before.declared_resources;
                        }
                        const auto owner_cpu = cpu_ns() - owner_cpu_start;
                        const auto encoded = std::chrono::steady_clock::now();
                        REQUIRE(f.device->waitUntilIdle(&f.error));
                        const auto completed = std::chrono::steady_clock::now();
                        const auto after = total_stats();
                        if (sample == 1)
                        {
                            CHECK(resource_key_count(sources, used_sources) == keys_per_view);
                            for (unsigned view = 0; view < views; ++view)
                            {
                                const auto actual = f.read(*targets[view]);
                                if (!arguments) reference[view] = actual;
                                else CHECK(actual == reference[view]);
                            }
                        }
                        if (sample >= warmups)
                        {
                            const unsigned at = sample - warmups;
                            encode_ms[at] = std::chrono::duration<double, std::milli>(encoded - begin).count();
                            cpu_ms[at] = owner_cpu / 1e6;
                            complete_ms[at] = std::chrono::duration<double, std::milli>(completed - begin).count();
                            tail_ms[at] = std::chrono::duration<double, std::milli>(completed - encoded).count();
                            source_cpu_total += source_cpu;
                            table_bytes += view_work.published_bytes;
                            table_creates += view_work.table_creates;
                            hits += view_work.cache_hits;
                            binds += view_work.buffer_bind_calls;
                            declared += view_work.declared_resources;
                            CHECK(after.argument_encoder_creations == before.argument_encoder_creations);
                            if (arguments)
                            {
                                CHECK(after.bound_draws - before.bound_draws == main_count + (views - 1) * 128);
                                CHECK(after.fallback_draws == before.fallback_draws);
                                if (pattern == 1) CHECK(view_work.table_creates == 0);
                            }
                            CHECK(material_cache.stats().published_bytes == uniform_after);
                        }
                    }
                    const auto summary = [](auto values)
                    {
                        std::sort(values.begin(), values.end());
                        return std::array<double, 2>{(values[5] + values[6]) * .5, values.back()};
                    };
                    const auto encode = summary(encode_ms), cpu = summary(cpu_ms), complete = summary(complete_ms), tail = summary(tail_ms);
                    std::cout << "world_resources_complete_path family=" << family << " slots=" << slots
                        << " main_count=" << main_count << " views=" << views
                        << " resource_pattern=" << (distinct_resources ? "distinct_texture_tuples" : "shared_texture_tuple")
                        << " source_variants_used=" << used_sources << " resource_keys_per_view=" << keys_per_view
                        << " pattern=" << (pattern == 0 ? "cold" : pattern == 1 ? "warm" : "churn")
                        << " mode=" << (arguments ? "argument_resources" : "native_direct")
                        << " samples=12 warmups=2 source_setup_cpu_ms=" << resource_setup_cpu / 1e6
                        << " sampled_source_cpu_ms=" << source_cpu_total / 1e6
                        << " encode_median_ms=" << encode[0] << " encode_max_ms=" << encode[1]
                        << " cpu_median_ms=" << cpu[0] << " cpu_max_ms=" << cpu[1]
                        << " complete_median_ms=" << complete[0] << " complete_max_ms=" << complete[1]
                        << " gpu_wait_tail_median_ms=" << tail[0] << " gpu_wait_tail_max_ms=" << tail[1]
                        << " sampled_table_creates=" << table_creates << " sampled_table_publication_bytes=" << table_bytes
                        << " sampled_cache_hits=" << hits << " sampled_buffer_binds=" << binds
                        << " sampled_declared_resources=" << declared << '\n';
                }
            }
        }
    }
}
