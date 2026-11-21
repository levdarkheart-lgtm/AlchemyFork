/**
 * @file almetal_world_source_rows_test.mm
 * @brief Actual G-buffer source-ID pulling parity and fallback boundaries.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldmaterialdata.h"
#include "../almetalworldresources.h"
#include "../almetalworldsourceview.h"
#include <cstdlib>
#include <filesystem>

namespace
{
struct SourceRowsFixture : Fixture
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

    SourceRowsFixture()
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
                mesh.colors[vertex][1] = 170 + slot * 10;
                mesh.colors[vertex][2] = 80 + slot * 20;
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
            single.colors[corner][1] = 180; single.colors[corner][2] = 96;
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

struct SourceDescriptions { std::vector<ALMetalWorldSimpleBatch> sources; };

void publishSourceRowsBatch(SourceRowsFixture& f, ALMetalWorldSimpleBatch& batch,
                           ALMetalWorldMaterialDataCache& cache)
{
    REQUIRE(ALMetalWorldMaterialSource::publish(*f.device, batch, cache, &f.error));
    auto source = ALMetalWorldResourceSource::publish(*f.device, batch, f.resources->textures, &f.error);
    INFO(f.error); REQUIRE(source); REQUIRE(source->matches(batch));
}
ALMetalWorldSourceRegion sourceRegion(const ALMetalWorldSimpleBatch& batch)
{
    ALMetalWorldSourceRegion region;
    std::memcpy(region.modelview_projection, batch.modelview_projection, sizeof(region.modelview_projection));
    std::memcpy(region.modelview, batch.modelview, sizeof(region.modelview));
    std::memcpy(region.normal_matrix, batch.normal_matrix, sizeof(region.normal_matrix));
    return region;
}
ALMetalWorldSourceReference sourceReference(const ALMetalWorldSimpleBatch& batch)
{
    ALMetalWorldSourceReference reference; reference.source = &batch;
    std::memcpy(reference.view.modelview_projection, batch.modelview_projection, sizeof(reference.view.modelview_projection));
    std::memcpy(reference.view.modelview, batch.modelview, sizeof(reference.view.modelview));
    std::memcpy(reference.view.normal_matrix, batch.normal_matrix, sizeof(reference.view.normal_matrix));
    std::memcpy(reference.view.rigged_normal_matrix, batch.rigged_normal_matrix, sizeof(reference.view.rigged_normal_matrix));
    std::memcpy(reference.view.texture_matrix, batch.texture_matrix, sizeof(reference.view.texture_matrix));
    return reference;
}
struct SourceRowsOutput { Pixels pixels; std::vector<std::uint64_t> queries; };
SourceRowsOutput renderSourceRows(SourceRowsFixture& f, ALMetalWorldGBufferScene scene,
    const std::vector<ALMetalWorldSimpleBatch>& raw,
    const std::vector<ALMetalWorldSourceReference>& refs,
    const std::vector<ALMetalWorldDrawIndex>& order,
    const std::vector<ALMetalWorldVisibilityQuery>& queries,
    const std::shared_ptr<const void>& cpu_owner, unsigned mode, std::uint64_t frame)
{
    f.data->beginFrame(frame);
    scene.batches = raw.data(); scene.batch_count = raw.size();
    scene.references = refs.data(); scene.reference_count = refs.size();
    scene.draw_order = order.data(); scene.draw_order_count = order.size();
    scene.visibility_queries = queries.data(); scene.visibility_query_count = queries.size();
    scene.cpu_source_owner = cpu_owner; scene.source_owner = f.resources;
    std::shared_ptr<ALMetalVisibilityResultBatch> results;
    std::unique_ptr<ALMetalRenderTarget> target;
    if (!mode) target = f.renderer->render(scene, &f.error, &results);
    else
    {
        auto input = ALMetalWorldViewInputs::adopt(scene, raw, refs, order, queries, cpu_owner, f.resources, frame, &f.error);
        INFO(f.error); REQUIRE(input);
        target = f.renderer->renderOwnedInputs(*input, &f.error, &results);
    }
    INFO(f.error); REQUIRE(target);
    SourceRowsOutput output{f.read(*target), {}};
    if (!queries.empty())
    {
        REQUIRE(results); REQUIRE(results->resultCount() == queries.size());
        output.queries.resize(queries.size());
        for (std::size_t query = 0; query < queries.size(); ++query)
            REQUIRE(results->wait(query, output.queries[query], &f.error));
    }
    return output;
}
void checkSourceRowsBound(const ALMetalWorldSourceDrawStats& before,
                          const ALMetalWorldSourceDrawStats& after, std::uint64_t count)
{
    CHECK(after.requested - before.requested == count);
    CHECK(after.bound - before.bound == count);
    CHECK(after.fallback == before.fallback);
    CHECK(after.setup_failures == before.setup_failures);
}
std::vector<ALMetalWorldVisibilityQuery> sourceRowsQueries()
{
    std::vector<ALMetalWorldVisibilityQuery> queries(5);
    for (unsigned index = 0; index < queries.size(); ++index)
    {
        auto& query = queries[index]; identity(query.modelview_projection);
        query.center[0] = -.5f; query.center[1] = -.1f; query.center[2] = .75f;
        query.half_extent[0] = query.half_extent[1] = query.half_extent[2] = .08f;
        query.batch_offset = index; query.fan = 7; query.count_samples = 1;
    }
    return queries;
}
}

TEST_CASE("almetal: source-row renderer preserves indexed material slots and ordered mixed G-buffer draws",
          "[almetal][world-source-rows][world-source-row-renderer]")
{
    @autoreleasepool
    {
        SourceRowsFixture f; ALMetalWorldMaterialDataCache cache(*f.device);
        std::uint64_t frame = 2;
        for (unsigned family : {1u, 2u})
        for (unsigned slots : {1u, 8u})
        for (bool mask : {false, true})
        {
            CAPTURE(family, slots, mask);
            auto cpu = std::make_shared<SourceDescriptions>(); cpu->sources.reserve(2);
            for (unsigned source = 0; source < 2; ++source)
            {
                auto batch = f.material(family, slots, mask);
                batch.modelview_projection[0] = source && family == 2 ? -.45f : .45f;
                batch.modelview_projection[5] = .85f;
                batch.modelview_projection[12] = source ? .5f : -.5f;
                batch.modelview[12] = source ? -.17f : .13f;
                batch.normal_matrix[0][0] = .8f; batch.normal_matrix[0][1] = .6f;
                batch.normal_matrix[1][0] = -.6f; batch.normal_matrix[1][1] = .8f;
                batch.double_sided = source && family == 2;
                if (source)
                    for (unsigned slot = 0; slot < slots; ++slot)
                        batch.diffuse_textures[slot] = f.diffuse[(slot + 1) % f.slots];
                publishSourceRowsBatch(f, batch, cache);
                cpu->sources.push_back(std::move(batch));
            }
            auto first = f.batch(0), middle = f.batch(3);
            for (auto* batch : {&first, &middle})
            {
                batch->modelview_projection[0] = batch->modelview_projection[5] = .15f;
                batch->modelview_projection[13] = batch == &first ? -.85f : .85f;
            }
            const std::vector<ALMetalWorldSimpleBatch> raw{first, middle};
            const std::vector<ALMetalWorldSimpleBatch> baseline{first, cpu->sources[0], middle, cpu->sources[1]};
            const std::vector<ALMetalWorldSourceReference> refs{
                sourceReference(cpu->sources[0]), sourceReference(cpu->sources[1])};
            const std::vector<ALMetalWorldDrawIndex> order{0, alMetalWorldDrawReferenceBit, 1,
                alMetalWorldDrawReferenceBit | 1};
            auto metadata = std::make_shared<ALMetalWorldSourceView>();
            // Deliberately reverse region placement relative to both source
            // order and bank-local IDs. No shader may use rowID as regionID.
            metadata->regions = {sourceRegion(cpu->sources[1]), sourceRegion(cpu->sources[0])};
            const std::uint64_t flavor = family | (std::uint64_t(slots) << 8) | (std::uint64_t(mask) << 16);
            metadata->candidates = {{{1001,flavor,1},1,1}, {{1002,flavor,1},3,0}};
            const auto queries = sourceRowsQueries();
            for (unsigned format = 0; format < 8; ++format)
            {
                CAPTURE(format);
                auto scene = f.scene(); scene.hdr = format & 1; scene.normal_hdr = format & 2;
                scene.reverse_z = format & 4; scene.join_adjacent_static_draws = true;
                scene.mirror_clip = format == 3 || format == 7;
                if (scene.mirror_clip)
                {
                    scene.mirror_clip_plane[0] = 1; scene.mirror_clip_plane[3] = 0;
                }
                const auto expected = renderSourceRows(f, scene, baseline, {}, {}, queries, {}, 0, ++frame);
                REQUIRE(expected.queries.front() > 0);
                const auto clear = scene.reverse_z ? 0u : 0x3f800000u;
                REQUIRE(std::count_if(expected.pixels.depth.begin(), expected.pixels.depth.end(),
                    [&](auto value) { return value != clear; }) > 8);
                scene.source_rows = metadata;
                for (unsigned mode = 0; mode < 2; ++mode)
                {
                    CAPTURE(mode);
                    const auto before = f.renderer->worldSourceStats();
                    const auto actual = renderSourceRows(f, scene, raw, refs, order, queries, cpu, mode, ++frame);
                    CHECK(actual.pixels == expected.pixels); CHECK(actual.queries == expected.queries);
                    checkSourceRowsBound(before, f.renderer->worldSourceStats(), 2);
                }
            }
        }
    }
}

TEST_CASE("almetal: invalid optional source-row metadata preserves required native rendering",
          "[almetal][world-source-rows][world-source-row-renderer]")
{
    @autoreleasepool
    {
        SourceRowsFixture f; ALMetalWorldMaterialDataCache cache(*f.device);
        auto batch = f.material(1, 8); publishSourceRowsBatch(f, batch, cache);
        auto good = std::make_shared<ALMetalWorldSourceView>();
        good->regions = {sourceRegion(batch)};
        good->candidates = {{{2001,1,1},0,0}};
        auto scene = f.scene(); scene.hdr = true;
        const auto expected = renderSourceRows(f, scene, {batch}, {}, {}, {}, {}, 0, 2);
        scene.source_rows = good;
        const auto before = f.renderer->worldSourceStats();
        const auto warm = renderSourceRows(f, scene, {batch}, {}, {}, {}, {}, 1, 3);
        CHECK(warm.pixels == expected.pixels); checkSourceRowsBound(before, f.renderer->worldSourceStats(), 1);
        std::uint64_t frame = 4;
        for (unsigned invalid = 0; invalid < 7; ++invalid)
        {
            CAPTURE(invalid);
            auto bad = std::make_shared<ALMetalWorldSourceView>(*good);
            switch (invalid)
            {
                case 0: bad->candidates[0].draw_index = 1; break;
                case 1: bad->candidates[0].region = 1; break;
                case 2: bad->candidates[0].key.identity = 0; break;
                case 3: bad->candidates.push_back(bad->candidates[0]); break;
                case 4: bad->regions.clear(); break;
                case 5: bad->regions[0].modelview_projection[0] = std::numeric_limits<float>::quiet_NaN(); break;
                case 6: bad->regions[0].modelview[12] += .25f; break;
            }
            scene.source_rows = bad;
            const auto stats = f.renderer->worldSourceStats();
            const auto actual = renderSourceRows(f, scene, {batch}, {}, {}, {}, {}, 1, ++frame);
            CHECK(actual.pixels == expected.pixels);
            const auto after = f.renderer->worldSourceStats();
            CHECK(after.bound == stats.bound);
            CHECK(after.fallback > stats.fallback);
            CHECK(after.setup_failures > stats.setup_failures);
        }
        // PPD175: a finite per-view UV override is a valid ordinary compact
        // draw but cannot use a row encoding the immutable prototype's UV.
        auto cpu = std::make_shared<SourceDescriptions>(); cpu->sources.push_back(batch);
        auto ref = sourceReference(cpu->sources[0]);
        ref.view.texture_matrix[0] = .35f; ref.view.texture_matrix[5] = .65f;
        ref.view.texture_matrix[12] = .23f; ref.view.texture_matrix[13] = .11f;
        auto transformed = batch;
        std::memcpy(transformed.texture_matrix, ref.view.texture_matrix, sizeof(transformed.texture_matrix));
        scene.source_rows.reset();
        const auto uv_expected = renderSourceRows(f, scene, {transformed}, {}, {}, {}, {}, 0, ++frame);
        CHECK_FALSE(uv_expected.pixels == expected.pixels);
        scene.source_rows = good;
        const auto stats = f.renderer->worldSourceStats();
        const auto uv_actual = renderSourceRows(f, scene, {}, {ref}, {alMetalWorldDrawReferenceBit}, {}, cpu, 1, ++frame);
        CHECK(uv_actual.pixels == uv_expected.pixels);
        const auto after = f.renderer->worldSourceStats();
        CHECK(after.bound == stats.bound);
        CHECK(after.fallback - stats.fallback == 1);
        CHECK(after.setup_failures == stats.setup_failures + 1);
    }
}

TEST_CASE("almetal: source-row request gracefully falls back with the previous compiled scalar library",
          "[almetal][.world-source-rows-previous-shader]")
{
    @autoreleasepool
    {
        const char* value = std::getenv("ALCHEMY_WORLD_RESOURCE_BASELINE_LIBRARY"); REQUIRE(value);
        const std::filesystem::path path(value);
        REQUIRE(path.is_absolute()); REQUIRE(std::filesystem::is_regular_file(path));
        SourceRowsFixture f; ALMetalWorldMaterialDataCache cache(*f.device);
        auto library = ALMetalShaderLibrary::loadFile(*f.device, path.string(), &f.error); REQUIRE(library);
        REQUIRE_FALSE(library->hasFunction("almetal_world_material_source_rows_vertex"));
        REQUIRE_FALSE(library->hasFunction("almetal_world_pbr_source_rows_fragment"));
        std::uint64_t frame = 2;
        for (unsigned family : {1u, 2u})
        {
            auto batch = f.material(family, 8); publishSourceRowsBatch(f, batch, cache);
            auto metadata = std::make_shared<ALMetalWorldSourceView>(); metadata->regions = {sourceRegion(batch)};
            metadata->candidates = {{{3001,family,1},0,0}};
            auto scene = f.scene(); scene.hdr = true;
            REQUIRE(f.renderer->reloadShaders(&f.error));
            const auto expected = renderSourceRows(f, scene, {batch}, {}, {}, {}, {}, 0, ++frame);
            REQUIRE(f.renderer->reloadShaders(path.string(), &f.error));
            scene.source_rows = metadata;
            const auto before = f.renderer->worldSourceStats();
            const auto actual = renderSourceRows(f, scene, {batch}, {}, {}, {}, {}, 1, ++frame);
            CHECK(actual.pixels == expected.pixels);
            const auto after = f.renderer->worldSourceStats();
            CHECK(after.requested - before.requested == 1);
            CHECK(after.bound == before.bound);
            CHECK(after.fallback - before.fallback == 1);
            CHECK(after.setup_failures > before.setup_failures);
        }
    }
}
