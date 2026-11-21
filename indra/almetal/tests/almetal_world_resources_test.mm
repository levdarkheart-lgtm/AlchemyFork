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
    std::vector<ALMetalWorldVisibilityQuery> queries, bool owned_input, std::uint64_t frame)
{
    f.data->beginFrame(frame);
    scene.source_owner = f.resources;
    scene.batches = batches.data(); scene.batch_count = batches.size();
    scene.visibility_queries = queries.data(); scene.visibility_query_count = queries.size();
    std::shared_ptr<ALMetalVisibilityResultBatch> results;
    std::unique_ptr<ALMetalRenderTarget> target;
    if (owned_input)
    {
        auto ready = f.adopt(scene, std::move(batches), std::move(queries));
        target = renderer.renderOwnedInputs(*ready, &f.error, &results);
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

}

TEST_CASE("almetal: world scalar resources preserve visible indexed and query boundaries",
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
                const auto owned_input = renderResourceView(f, *f.renderer, scene,
                    {first, second}, resourceQueries(count), true, ++frame);
                CHECK(owned_input.pixels == expected.pixels);
                CHECK(owned_input.queries == expected.queries);
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
            const auto current_anisotropy = f.renderer->anisotropy();
            REQUIRE(f.renderer->setAnisotropy(current_anisotropy == 4 ? 8 : 4, &f.error));
            auto scene = f.scene(); scene.hdr = true;
            const auto direct = renderResourceView(f, *f.renderer, scene, {first, second}, {}, false, ++frame);
            const auto edited_sampler = renderResourceView(f, *f.renderer, scene, {first, second}, {}, true, ++frame);
            CHECK(edited_sampler.pixels == direct.pixels);
        }
    }
}

TEST_CASE("almetal: scalar world commands retain exact native generations through settlement",
          "[almetal][world-resources]")
{
    @autoreleasepool
    {
        for (unsigned family : {1u, 2u}) for (bool owned_input : {false, true})
        for (bool abandon : {false, true})
        {
            CAPTURE(family, owned_input, abandon);
            WorldResourceFixture f;
            ALMetalWorldMaterialDataCache material_cache(*f.device);
            auto batch = f.material(family, 8);
            publishResourceBatch(f, batch, material_cache);
            auto scene = f.scene(); scene.hdr = true;
            const auto expected = renderResourceView(f, *f.renderer, scene, {batch}, {}, false, 2).pixels;
            std::weak_ptr<Resources> weak_native = f.resources;
            std::weak_ptr<const ALMetalWorldResourceSource> weak_cpu = batch.native_resource_source;
            std::weak_ptr<const ALMetalBuffer> weak_uniform = family == 1
                ? batch.native_material_source->materialLease() : batch.native_material_source->pbrLease(true);
            scene.batches = &batch; scene.batch_count = 1; scene.source_owner = f.resources;
            id<MTLSharedEvent> gate = [(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent]; REQUIRE(gate);
            struct ReleaseGate { id<MTLSharedEvent> value; ~ReleaseGate() { value.signaledValue = 1; } } release{gate};
            if (!abandon)
            {
                auto blocker = f.device->makeCommandBuffer("Hold scalar world native generations", &f.error); REQUIRE(blocker);
                [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];
                REQUIRE(blocker->commit(&f.error));
            }
            REQUIRE(f.renderer->beginCommandBatch("Scalar native generation settlement", &f.error));
            std::unique_ptr<ALMetalRenderTarget> target;
            if (owned_input)
            {
                auto inputs = ALMetalWorldViewInputs::adopt(scene, {batch}, {}, f.resources, 3, &f.error); REQUIRE(inputs);
                target = f.renderer->renderOwnedInputs(*inputs, &f.error);
            }
            else target = f.renderer->render(scene, &f.error);
            INFO(f.error); REQUIRE(target);
            auto completion = f.renderer->commandBatchCompletion(); REQUIRE(completion); CHECK_FALSE(completion->isComplete());
            if (!abandon) REQUIRE(f.renderer->commitCommandBatch(&f.error));
            batch = {}; scene = {}; f.resources.reset(); material_cache.clear();
            CHECK(weak_cpu.expired()); // No command needs the CPU resource-source description.
            CHECK_FALSE(weak_native.expired()); CHECK_FALSE(weak_uniform.expired());
            if (abandon) f.renderer->cancelCommandBatch();
            else
            {
                CHECK_FALSE(completion->isComplete()); gate.signaledValue = 1;
                REQUIRE(f.device->waitUntilIdle(&f.error)); CHECK(f.read(*target) == expected);
            }
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while ((!weak_native.expired() || !weak_uniform.expired()) &&
                   std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
            CHECK(completion->isComplete()); CHECK(weak_native.expired()); CHECK(weak_uniform.expired());
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
                const auto owned_input = renderResourceView(f, *f.renderer, scene, {batch}, {}, true, ++frame);
                CHECK(owned_input.pixels == expected.pixels);
            }
        }
    }
}

TEST_CASE("almetal: scalar resource publication rejects unowned generations without replacing prior proof",
          "[almetal][world-resources]")
{
    @autoreleasepool
    {
        WorldResourceFixture f;
        ALMetalWorldMaterialDataCache material_cache(*f.device);
        for (unsigned family : {1u, 2u})
        {
            auto batch = f.material(family, 8);
            publishResourceBatch(f, batch, material_cache);
            const auto prior = batch.native_resource_source;
            auto unowned = batch; unowned.diffuse_textures[0] = f.diffuse[7];
            ALMetalWorldResourceFallback failure = ALMetalWorldResourceFallback::none;
            CHECK_FALSE(ALMetalWorldResourceSource::publish(*f.device, unowned, {}, &f.error, &failure));
            CHECK(failure == ALMetalWorldResourceFallback::validation);
            CHECK(unowned.native_resource_source == prior);
            auto invalid = batch; invalid.pbr_slot_count = 9;
            auto scene = f.scene(); scene.batches = &invalid; scene.batch_count = 1;
            CHECK_FALSE(f.renderer->render(scene, &f.error));
            auto inputs = ALMetalWorldViewInputs::adopt(scene, {invalid}, {}, f.resources, 1, &f.error);
            REQUIRE(inputs); CHECK_FALSE(f.renderer->renderOwnedInputs(*inputs, &f.error));
        }
    }
}

TEST_CASE("almetal: scalar resource bindings preserve adjacent native indexed joining",
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
            for (bool owned_input : {false, true})
            {
                const auto draws_before = ALMetalDevice::encodedDrawCallCount();

                const auto actual = renderResourceView(f, *f.renderer, scene, {first, second}, {}, owned_input, ++frame);
                CHECK(actual.pixels == expected.pixels);
                CHECK(ALMetalDevice::encodedDrawCallCount() - draws_before == 1);
            }
        }
    }
}


TEST_CASE("almetal: immutable resource sources pin exact texture and registry generations",
          "[almetal][world-resources][metal-resource-plan-native]")
{
    @autoreleasepool
    {
        for (bool pbr : {false, true})
        {
            CAPTURE(pbr);
            std::string error;
            auto device = ALMetalDevice::create(&error, ALMetalResourcePlanMode::enabled); REQUIRE(device);
            ALMetalTextureDescriptor descriptor;
            descriptor.width = descriptor.height = 1;
            descriptor.format = ALMetalPixelFormat::rgba8_unorm_srgb;
            descriptor.usage = ALMetalTextureUsage::shader_read;
            std::shared_ptr<const ALMetalTexture> generation = ALMetalTexture::create(*device, descriptor, &error);
            INFO(error); REQUIRE(generation); REQUIRE(generation->resourceView());
            REQUIRE(generation->resourceView().canIssueGeneration());
            ALMetalWorldSimpleBatch batch;
            batch.texture_count = 1; batch.diffuse_textures[0] = generation.get();
            if (pbr)
            {
                batch.pbr_slot_count = 1;
                auto& slot = batch.pbrSlots()[0];
                slot.normal_texture = slot.orm_texture = slot.emissive_texture = generation.get();
            }
            else batch.material_slot_count = 1;
            ALMetalWorldMaterialDataCache materials(*device);
            REQUIRE(ALMetalWorldMaterialSource::publish(*device, batch, materials, &error));
            std::array<std::shared_ptr<const ALMetalTexture>, 1> leases{generation};
            auto source = ALMetalWorldResourceSource::publish(*device, batch, leases, &error);
            INFO(error); REQUIRE(source); REQUIRE(source->matches(batch));
            CHECK(source->generationLeases().size() == 1);
            CHECK(source->generationLeases().front() == generation);
            CHECK(ALMetalWorldResourceSource::publish(*device, batch, leases, &error) == source);
            CHECK_FALSE(generation->resourceView().canIssueGeneration());
            batch.native_resource_source.reset(); source.reset();
            CHECK(generation->resourceView().canIssueGeneration());

            source = ALMetalWorldResourceSource::publish(*device, batch, leases, &error); REQUIRE(source);
            std::weak_ptr<const ALMetalTexture> weak_generation = generation;
            generation.reset(); leases[0].reset();
            CHECK_FALSE(weak_generation.expired());
            batch.native_resource_source.reset(); source.reset();
            CHECK(weak_generation.expired());
        }
    }
}

TEST_CASE("almetal: recurring source ownership survives bounded cache collisions and settlement",
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
            constexpr unsigned source_count = 17; // Pigeonhole collision in either 16-entry owner cache.
            std::vector<ALMetalWorldSimpleBatch> sources;
            std::vector<std::weak_ptr<const ALMetalWorldMaterialSource>> material_sources;
            std::vector<std::weak_ptr<const ALMetalWorldResourceSource>> resource_sources;
            std::vector<std::weak_ptr<const ALMetalBuffer>> uniforms;
            for (unsigned source = 0; source < source_count; ++source)
            {
                auto batch = f.material(family, 1);
                if (family == 1) batch.materialSlots()[0].env_intensity = .02f * source;
                else batch.pbrSlots()[0].roughness_factor = .1f + .04f * source;
                publishResourceBatch(f, batch, material_cache);
                material_sources.emplace_back(batch.native_material_source);
                resource_sources.emplace_back(batch.native_resource_source);
                uniforms.emplace_back(family == 1 ? batch.native_material_source->materialLease()
                    : batch.native_material_source->pbrLease(true));
                sources.push_back(std::move(batch));
            }
            std::vector<ALMetalWorldSimpleBatch> batches;
            for (unsigned repetition = 0; repetition < 2; ++repetition)
                batches.insert(batches.end(), sources.begin(), sources.end());
            batches.push_back(sources.front());
            auto queries = resourceQueries(1);
            queries[1].batch_offset = source_count;
            queries[2].batch_offset = source_count * 2;
            auto scene = f.scene(); scene.hdr = true;
            const auto expected = renderResourceView(f, *f.renderer, scene, batches, queries, false, 2);
            REQUIRE(expected.queries.front() > 0);
            auto geometry = std::make_shared<Resources>();
            geometry->buffers = f.resources->buffers;
            // Numeric uniform sources own no texture generations. Their
            // ordinary scene owner still supplies that independent contract.
            scene.source_owner = f.resources;
            scene.batches = batches.data(); scene.batch_count = batches.size();
            scene.visibility_queries = queries.data(); scene.visibility_query_count = queries.size();
            std::weak_ptr<const ALMetalTexture> old_texture;
            for (const auto& texture : f.resources->textures)
                if (texture.get() == f.diffuse[0]) old_texture = texture;
            REQUIRE_FALSE(old_texture.expired());
            id<MTLSharedEvent> gate = [(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];
            REQUIRE(gate);
            struct ReleaseGate { id<MTLSharedEvent> value; ~ReleaseGate() { value.signaledValue = 1; } } release{gate};
            if (!abandon)
            {
                auto blocker = f.device->makeCommandBuffer("Recurring source ownership gate", &f.error); REQUIRE(blocker);
                [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];
                REQUIRE(blocker->commit(&f.error));
            }
            REQUIRE(f.renderer->beginCommandBatch("Recurring source ownership", &f.error));

            std::shared_ptr<ALMetalVisibilityResultBatch> results;
            std::unique_ptr<ALMetalRenderTarget> target;
            if (family == 2)
            {
                auto inputs = ALMetalWorldViewInputs::adopt(scene, batches, queries, scene.source_owner, 4, &f.error);
                INFO(f.error); REQUIRE(inputs);

                 auto ready = inputs; REQUIRE(ready);
                target = f.renderer->renderOwnedInputs(*ready, &f.error, &results);
            }
            else target = f.renderer->render(scene, &f.error, &results);
            INFO(f.error); REQUIRE(target); REQUIRE(results);
            auto completion = f.renderer->commandBatchCompletion(); REQUIRE(completion);
            if (!abandon) REQUIRE(f.renderer->commitCommandBatch(&f.error));
            sources.clear(); batches.clear();
            for (const auto& source : material_sources) CHECK_FALSE(source.expired());
            for (const auto& uniform : uniforms) CHECK_FALSE(uniform.expired());
            for (const auto& source : resource_sources) CHECK(source.expired());
            CHECK_FALSE(old_texture.expired());
            CHECK_FALSE(completion->isComplete());
            if (abandon) f.renderer->cancelCommandBatch();
            else
            {
                gate.signaledValue = 1;
                REQUIRE(f.device->waitUntilIdle(&f.error));
                CHECK(f.read(*target) == expected.pixels);
                REQUIRE(results->resultCount() == expected.queries.size());
                for (unsigned query = 0; query < expected.queries.size(); ++query)
                {
                    std::uint64_t samples = 0;
                    REQUIRE(results->wait(query, samples, &f.error));
                    CHECK(samples == expected.queries[query]);
                }
            }
            const auto retired = [&]
            {
                return std::all_of(material_sources.begin(), material_sources.end(), [](const auto& source) { return source.expired(); }) &&
                    std::all_of(resource_sources.begin(), resource_sources.end(), [](const auto& source) { return source.expired(); }) &&
                    std::all_of(uniforms.begin(), uniforms.end(), [](const auto& uniform) { return uniform.expired(); });
            };
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (!retired() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
            CHECK(retired());
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
                for (bool owned_input : {false, true})
                {

                    const auto actual = renderResourceView(f, *f.renderer, scene,
                        {first, second}, resourceQueries(count), owned_input, ++frame);
                    CHECK(actual.pixels == old.pixels);
                    CHECK(actual.queries == old.queries);
                }
            }
        }
        std::cout << "world_resources_previous_shader baseline_entrypoints_absent=1"
                     " family_slot_mask_format_tuples=64 old_raw_new_raw_new_owned=1\n";
    }
}
