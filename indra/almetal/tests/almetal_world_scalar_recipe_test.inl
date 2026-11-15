/**
 * @file almetal_world_scalar_recipe_test.inl
 * @brief Ordinary scalar cohort recipes preserve native output and lifetimes.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after the real CohortFixture and native preparation fixture.
 */
namespace
{
void scalarRecipePublish(CohortFixture& f, ALMetalWorldSimpleBatch& batch)
{
    if (!batch.material_slot_count && !batch.pbr_slot_count) return;
    REQUIRE(ALMetalWorldMaterialSource::publish(*f.device, batch, f.cache, &f.error));
    for (const auto& buffer : {batch.native_material_source->materialLease(),
             batch.native_material_source->pbrLease(false), batch.native_material_source->pbrLease(true)})
        if (buffer && std::none_of(f.resources->buffers.begin(), f.resources->buffers.end(),
                [&](const auto& prior) { return prior == buffer; }))
            f.resources->buffers.push_back(buffer);
}

// Each source has visible geometry in the same region-space buffer. Source
// order and a common camera, rather than per-object transforms, match the
// ordinary cohort boundary whose owner-side classification is being cached.
void scalarRecipeTiles(CohortFixture& f, unsigned family, unsigned normal_spec = 3,
                      bool masked = false, bool double_sided = false, unsigned legacy_alpha_mode = 0)
{
    constexpr unsigned count = 4;
    std::array<Fixture::Geometry, count> geometry;
    for (unsigned tile = 0; tile < count; ++tile)
    {
        geometry[tile] = f.geometry;
        for (unsigned vertex = 0; vertex < 4; ++vertex)
        {
            geometry[tile].positions[vertex][0] =
                f.geometry.positions[vertex][0] * .24f + (float(tile) - 1.5f) * .46f;
            geometry[tile].positions[vertex][1] = f.geometry.positions[vertex][1] * .6f;
            geometry[tile].uv[vertex][0] = .2f;
            geometry[tile].uv[vertex][1] = .25f;
        }
    }
    const auto* vertices = f.buffer(geometry.data(), sizeof(geometry));
    f.cpu->sources.clear(); f.cpu->keys.clear();
    f.cpu->sources.reserve(count); f.cpu->keys.reserve(count);
    for (unsigned tile = 0; tile < count; ++tile)
    {
        auto batch = f.batch(family);
        const auto offset = tile * sizeof(Fixture::Geometry);
        batch.vertices = vertices;
        batch.position_offset += offset; batch.normal_offset += offset;
        batch.texcoord_offset += offset; batch.color_offset += offset;
        batch.tangent_offset += offset;
        batch.texcoord1_offset = batch.texcoord2_offset = batch.texcoord_offset;
        if (family == 1)
        {
            batch.material_alpha_mode = legacy_alpha_mode;
            auto& slot = batch.materialSlots()[0];
            slot.normal_texture = normal_spec & 1 ? f.flat : nullptr;
            slot.specular_texture = normal_spec & 2 ? f.orm : nullptr;
            slot.alpha_cutoff = .5f;
        }
        if (family == 2)
        {
            batch.alpha_mask = masked;
            batch.double_sided = double_sided;
            batch.pbrSlots()[0].alpha_cutoff = masked ? .5f : -1.f;
        }
        scalarRecipePublish(f, batch);
        f.cpu->sources.push_back(std::move(batch));
    }
    for (unsigned tile = 0; tile < count; ++tile)
        f.cpu->keys.push_back({{tile + 1, family, 1}, &f.cpu->sources[tile]});
}

std::vector<ALMetalWorldSimpleBatch> scalarRecipeRaw(
    const ALMetalWorldSourceCohortView& group)
{
    std::vector<ALMetalWorldSimpleBatch> raw;
    raw.reserve(group.cohort->size());
    for (std::size_t row = 0; row < group.cohort->size(); ++row)
    {
        raw.push_back(group.cohort->source(row));
        applyCohortMatrices(raw.back(), group.view);
    }
    return raw;
}

std::vector<std::uint64_t> scalarRecipeQueries(
    const std::shared_ptr<ALMetalVisibilityResultBatch>& result, std::size_t count,
    std::string& error)
{
    REQUIRE(result);
    std::vector<std::uint64_t> values(count);
    for (std::size_t query = 0; query < count; ++query)
        REQUIRE(result->wait(query, values[query], &error));
    return values;
}
}

TEST_CASE("almetal: scalar cohort recipes preserve ordinary shader families and output formats",
          "[almetal][world-cohort][world-scalar-recipe]")
{
    @autoreleasepool
    {
        CohortFixture f(0, 0, false);
        struct Variant { unsigned family, normal_spec, legacy_alpha_mode; bool mask, double_sided; };
        std::vector<Variant> variants{{0, 0, 0, false, false}};
        for (unsigned maps = 0; maps < 4; ++maps)
            for (unsigned alpha_mode : {0u, 2u, 3u}) variants.push_back({1, maps, alpha_mode, false, false});
        for (bool mask : {false, true}) for (bool cull : {false, true})
            variants.push_back({2, 3, 0, mask, cull});
        std::uint64_t frame = 1;
        for (const auto& variant : variants)
        {
            scalarRecipeTiles(f, variant.family, variant.normal_spec, variant.mask, variant.double_sided, variant.legacy_alpha_mode);
            auto certificate = f.seal(); auto group = f.view(certificate);
            CHECK(certificate->scalarCommonMetadataBytes() >= certificate->size());
            CHECK_FALSE(certificate->sameScalarCommonAsPrevious(0));
            CHECK_FALSE(certificate->sameScalarCommonAsPrevious(certificate->size()));
            for (std::size_t row = 0; row < certificate->size(); ++row)
            {
                const auto recipe = certificate->scalarRecipe(row);
                CHECK(recipe.family == variant.family);
                CHECK(recipe.normal_map == (variant.family == 1 && bool(variant.normal_spec & 1)));
                CHECK(recipe.specular_map == (variant.family == 1 && bool(variant.normal_spec & 2)));
                if (row) CHECK(certificate->sameScalarCommonAsPrevious(row));
            }
            auto batches = scalarRecipeRaw(group);
            auto queries = cohortQueries(batches.size());
            for (unsigned format = 0; format < 8; ++format)
            {
                CAPTURE(variant.family, variant.normal_spec, variant.legacy_alpha_mode, variant.mask, variant.double_sided, format);
                auto scene = f.scene();
                scene.hdr = format & 1; scene.normal_hdr = format & 2; scene.reverse_z = format & 4;
                scene.visibility_queries = queries.data(); scene.visibility_query_count = queries.size();
                scene.batches = batches.data(); scene.batch_count = batches.size(); scene.source_owner = f.resources;
                f.data->beginFrame(++frame);
                auto before = f.renderer->worldCohortStats();
                std::shared_ptr<ALMetalVisibilityResultBatch> raw_queries;
                auto raw = f.renderer->render(scene, &f.error, &raw_queries); INFO(f.error); REQUIRE(raw);
                const auto expected = f.read(*raw);
                const auto expected_queries = scalarRecipeQueries(raw_queries, queries.size(), f.error);
                REQUIRE(expected_queries.front() > 0);
                CHECK(std::any_of(expected.depth.begin(), expected.depth.end(), [&](auto value)
                    { return value != (scene.reverse_z ? 0u : 0x3f800000u); }));
                auto after = f.renderer->worldCohortStats();
                CHECK(after.scalar_recipe_reads == before.scalar_recipe_reads);
                CHECK(after.scalar_common_builds - before.scalar_common_builds == batches.size());
                CHECK(after.scalar_common_reuses == before.scalar_common_reuses);
                scene.batches = nullptr; scene.batch_count = 0; scene.cohorts = &group; scene.cohort_count = 1;
                auto inputs = ALMetalWorldViewInputs::adopt(scene, {}, {}, {}, std::vector{group}, queries,
                    {}, f.resources, frame, &f.error); INFO(f.error); REQUIRE(inputs);
                f.data->beginFrame(++frame); before = f.renderer->worldCohortStats();
                std::shared_ptr<ALMetalVisibilityResultBatch> recipe_queries;
                auto actual = f.renderer->renderOwnedInputs(*inputs, &f.error, &recipe_queries); INFO(f.error); REQUIRE(actual);
                after = f.renderer->worldCohortStats();
                CHECK(after.scalar_recipe_reads - before.scalar_recipe_reads == batches.size());
                CHECK(after.scalar_common_builds - before.scalar_common_builds == 1);
                CHECK(after.scalar_common_reuses - before.scalar_common_reuses == batches.size() - 1);
                CHECK(f.read(*actual) == expected);
                CHECK(scalarRecipeQueries(recipe_queries, queries.size(), f.error) == expected_queries);
            }
        }
    }
}

TEST_CASE("almetal: scalar common runs restore exact values across queries raw draws and terrain",
          "[almetal][world-cohort][world-scalar-recipe]")
{
    @autoreleasepool
    {
        CohortFixture f(0, 0, false);
        const std::array<std::uint8_t, 16> pattern{
            240, 30, 20, 255, 20, 230, 40, 255,
            30, 50, 240, 255, 220, 190, 20, 255};
        const auto* varied = f.texture(ALMetalPixelFormat::rgba8_unorm_srgb, 2, pattern.data(), 8);
        std::uint64_t frame = 1;
        for (unsigned family : {0u, 1u, 2u}) for (unsigned change = 0; change < 5; ++change)
        {
            CAPTURE(family, change);
            scalarRecipeTiles(f, family);
            for (auto& batch : f.cpu->sources) batch.diffuse_textures[0] = varied;
            auto& changed = f.cpu->sources[2];
            if (change == 0) changed.texture_matrix[12] = .5f;
            if (change == 1) changed.use_vertex_color = false;
            if (change == 2) changed.alpha_cutoff_bias = false;
            if (change == 3) changed.alpha_cutoff = .4f;
            if (change == 4)
            {
                changed.texture_count = 2; changed.diffuse_textures[1] = f.color;
                if (family == 1)
                {
                    changed.material_slot_count = 2;
                    auto& slots = changed.materialSlots(); slots[1] = slots[0]; slots[1].env_intensity = .8f;
                }
                if (family == 2)
                {
                    changed.pbr_slot_count = 2;
                    auto& slots = changed.pbrSlots(); slots[1] = slots[0]; slots[1].metallic_factor = .1f;
                }
                Fixture::Geometry indexed;
                std::memcpy(&indexed, static_cast<const std::byte*>(changed.vertices->mappedBytes()) +
                    2 * sizeof(Fixture::Geometry), sizeof(indexed));
                const std::uint32_t slot = 1;
                for (auto& position : indexed.positions) std::memcpy(position + 3, &slot, sizeof(slot));
                changed.vertices = f.buffer(&indexed, sizeof(indexed)); changed.position_offset = 0;
                changed.normal_offset = offsetof(Fixture::Geometry, normals);
                changed.texcoord_offset = changed.texcoord1_offset = changed.texcoord2_offset = offsetof(Fixture::Geometry, uv);
                changed.color_offset = offsetof(Fixture::Geometry, colors); changed.tangent_offset = offsetof(Fixture::Geometry, tangents);
            }
            // A new numeric certificate is needed only when authored slots or
            // its certified texture-count facts changed. Descriptor tables are
            // intentionally absent from this ordinary direct-shader exercise.
            for (auto& batch : f.cpu->sources) scalarRecipePublish(f, batch);
            auto certificate = f.seal();
            REQUIRE(certificate->sameScalarCommonAsPrevious(1));
            CHECK_FALSE(certificate->sameScalarCommonAsPrevious(2));
            CHECK_FALSE(certificate->sameScalarCommonAsPrevious(3));
            auto first = f.view(certificate); first.base_before = first.logical_begin = 1;
            auto second = first; second.base_before = 2; second.logical_begin = 6;
            second.view.modelview_projection[13] = .12f;
            std::vector groups{first, second};
            std::vector<ALMetalWorldSimpleBatch> raw{f.batch(0), f.batch(7), f.batch(0)};
            // Tiny special/raw draws leave all four cohort tiles observable.
            for (auto* batch : {&raw.front(), &raw.back()})
            { batch->modelview_projection[0] = batch->modelview_projection[5] = .05f; batch->modelview_projection[13] = -.9f; }
            raw[1].modelview_projection[0] = raw[1].modelview_projection[5] = .04f;
            raw[1].modelview_projection[12] = raw[1].modelview_projection[13] = -.98f;
            auto baseline = std::vector<ALMetalWorldSimpleBatch>{raw.front()};
            auto first_raw = scalarRecipeRaw(first), second_raw = scalarRecipeRaw(second);
            baseline.insert(baseline.end(), first_raw.begin(), first_raw.end()); baseline.push_back(raw[1]);
            baseline.insert(baseline.end(), second_raw.begin(), second_raw.end()); baseline.push_back(raw.back());
            auto queries = cohortQueries(baseline.size());
            auto scene = f.scene(); scene.source_owner = f.resources;
            scene.visibility_queries = queries.data(); scene.visibility_query_count = queries.size();
            scene.batches = baseline.data(); scene.batch_count = baseline.size();
            f.data->beginFrame(++frame);
            std::shared_ptr<ALMetalVisibilityResultBatch> baseline_queries;
            auto expected_target = f.renderer->render(scene, &f.error, &baseline_queries); INFO(f.error); REQUIRE(expected_target);
            const auto expected = f.read(*expected_target);
            const auto expected_queries = scalarRecipeQueries(baseline_queries, queries.size(), f.error);
            REQUIRE(expected_queries.front() > 0);
            scene.batches = raw.data(); scene.batch_count = raw.size();
            scene.cohorts = groups.data(); scene.cohort_count = groups.size();
            auto inputs = ALMetalWorldViewInputs::adopt(scene, raw, {}, {}, groups, queries, {}, f.resources, frame, &f.error);
            INFO(f.error); REQUIRE(inputs);
            const auto before = f.renderer->worldCohortStats();
            f.data->beginFrame(++frame);
            std::shared_ptr<ALMetalVisibilityResultBatch> result;
            auto target = f.renderer->renderOwnedInputs(*inputs, &f.error, &result); INFO(f.error); REQUIRE(target);
            const auto after = f.renderer->worldCohortStats();
            CHECK(after.scalar_recipe_reads - before.scalar_recipe_reads == 8);
            CHECK(after.scalar_common_builds - before.scalar_common_builds == 8);
            CHECK(after.scalar_common_reuses - before.scalar_common_reuses == 2);
            CHECK(f.read(*target) == expected);
            CHECK(scalarRecipeQueries(result, queries.size(), f.error) == expected_queries);
            auto work = ALMetalWorldPreparation::create(inputs, 2, {}, &f.error); INFO(f.error); REQUIRE(work);
            for (std::size_t job = 0; job < work->jobCount(); ++job) REQUIRE(work->runJob(job));
            auto ready = work->finish(&f.error); INFO(f.error); REQUIRE(ready);
            std::shared_ptr<ALMetalVisibilityResultBatch> prepared_queries;
            auto prepared = f.renderer->renderPrepared(*ready, &f.error, &prepared_queries); INFO(f.error); REQUIRE(prepared);
            CHECK(f.read(*prepared) == expected);
            CHECK(scalarRecipeQueries(prepared_queries, queries.size(), f.error) == expected_queries);
        }
    }
}

TEST_CASE("almetal: scalar recipes release CPU certificates while pending commands retain only native generations",
          "[almetal][world-cohort][world-scalar-recipe]")
{
    @autoreleasepool
    {
        for (unsigned family : {0u, 1u, 2u}) for (bool abandon : {false, true})
        {
            CAPTURE(family, abandon);
            CohortFixture f(family, 2, false);
            for (auto& source : f.cpu->sources) identity(source.texture_matrix);
            std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> records;
            for (const auto& source : f.cpu->keys)
            {
                auto record = ALMetalWorldCohortSource::seal(source, &f.error); INFO(f.error); REQUIRE(record);
                records.push_back(std::move(record));
            }
            auto certificate = ALMetalWorldSourceCohort::seal(records, f.cpu, f.resources, 7, &f.error);
            INFO(f.error); REQUIRE(certificate);
            auto group = f.view(certificate);
            auto scene = f.scene(); scene.cohorts = &group; scene.cohort_count = 1;
            auto inputs = ALMetalWorldViewInputs::adopt(scene, {}, {}, {}, std::vector{group}, {}, {}, {}, 2, &f.error);
            INFO(f.error); REQUIRE(inputs); scene = {};
            auto warm = f.renderer->renderOwnedInputs(*inputs, &f.error); INFO(f.error); REQUIRE(warm);
            const auto expected = f.read(*warm); warm.reset(); REQUIRE(f.device->waitUntilIdle(&f.error));
            f.data->beginFrame(2);
            id<MTLSharedEvent> gate = [(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent]; REQUIRE(gate);
            struct RecipeOpenGate { id<MTLSharedEvent> event; ~RecipeOpenGate() { event.signaledValue = 1; } } open{gate};
            if (!abandon)
            {
                auto blocker = f.device->makeCommandBuffer("Hold scalar recipe exact generations", &f.error); REQUIRE(blocker);
                [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];
                REQUIRE(blocker->commit(&f.error));
            }
            REQUIRE(f.renderer->beginCommandBatch("Scalar recipe CPU retirement", &f.error));
            const auto before = f.renderer->worldCohortStats();
            auto target = f.renderer->renderOwnedInputs(*inputs, &f.error); INFO(f.error); REQUIRE(target);
            const auto after = f.renderer->worldCohortStats();
            CHECK(after.scalar_recipe_reads - before.scalar_recipe_reads == 2);
            CHECK(after.scalar_common_builds - before.scalar_common_builds == 1);
            CHECK(after.scalar_common_reuses - before.scalar_common_reuses == 1);
            if (!abandon) REQUIRE(f.renderer->commitCommandBatch(&f.error));
            std::weak_ptr<CohortCpu> cpu = f.cpu;
            std::weak_ptr<Resources> native = f.resources;
            std::weak_ptr<const ALMetalWorldSourceCohort> cohort = certificate;
            std::weak_ptr<const ALMetalWorldCohortSource> record = records.front();
            std::weak_ptr<const ALMetalWorldMaterialSource> material = certificate->source(0).native_material_source;
            group = {}; certificate.reset(); records.clear(); f.cpu.reset(); f.resources.reset();
            std::vector<ALMetalWorldSimpleBatch> raw;
            std::vector<ALMetalWorldSourceReference> refs;
            std::vector<ALMetalWorldDrawIndex> order;
            std::vector<ALMetalWorldSourceCohortView> groups;
            std::vector<ALMetalWorldVisibilityQuery> queries;
            REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs, raw, refs, order, groups, queries));
            CHECK(cpu.expired()); CHECK(cohort.expired()); CHECK(record.expired()); CHECK(material.expired());
            REQUIRE_FALSE(native.expired());
            if (abandon) f.renderer->cancelCommandBatch();
            else
            {
                gate.signaledValue = 1; REQUIRE(f.device->waitUntilIdle(&f.error));
                CHECK(f.read(*target) == expected);
            }
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (!native.expired() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
            CHECK(native.expired());
        }
    }
}
