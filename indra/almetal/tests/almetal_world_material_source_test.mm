/**
 * @file almetal_world_material_source_test.mm
 * @brief Native pixels, publication cost and lifetime of sealed material data.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldmaterialdata.h"
#include <time.h>

namespace
{
ALMetalWorldSimpleBatch materialBatch(Fixture& f, unsigned family, bool indexed)
{
    auto batch = f.batch(family);
    batch.texcoord1_offset = batch.texcoord2_offset = batch.texcoord_offset;
    if (!indexed) return batch;
    batch.texture_count = 2;
    batch.diffuse_textures[1] = f.color;
    if (family == 1)
    {
        batch.material_slot_count = 2;
        auto& slots = batch.materialSlots();
        slots[1] = slots[0];
        slots[1].env_intensity = .65f;
        slots[1].specular_color[1] = .2f;
    }
    else
    {
        batch.pbr_slot_count = 2;
        auto& slots = batch.pbrSlots();
        slots[1] = slots[0];
        slots[1].roughness_factor = .8f;
        slots[1].metallic_factor = .15f;
        slots[1].emissive_color[0] = .25f;
    }
    auto geometry = f.geometry;
    const std::int32_t texture_index = 1;
    for (auto& position : geometry.positions)
        std::memcpy(&position[3], &texture_index, sizeof(texture_index));
    batch.vertices = f.buffer(&geometry, sizeof(geometry));
    return batch;
}

std::shared_ptr<const ALMetalBuffer> tableLease(
    const ALMetalWorldMaterialSource& source, unsigned family, bool normal_hdr)
{
    return family == 1 ? source.materialLease() : source.pbrLease(normal_hdr);
}

struct MaterialRender
{
    Pixels pixels;
    std::uint64_t published_bytes = 0;
};

MaterialRender renderMaterial(Fixture& f, ALMetalWorldGBufferScene scene,
    const ALMetalWorldSimpleBatch& batch, bool prepared, std::uint64_t frame)
{
    f.data->beginFrame(frame);
    scene.batches = &batch;
    scene.batch_count = 1;
    const auto before = f.data->stats().published_bytes;
    std::unique_ptr<ALMetalRenderTarget> target;
    if (prepared)
    {
        auto ready = f.prepare(scene, {batch});
        target = f.renderer->renderPrepared(*ready, &f.error);
    }
    else target = f.renderer->render(scene, &f.error);
    INFO(f.error);
    REQUIRE(target);
    const auto published = f.data->stats().published_bytes - before;
    return {f.read(*target), published};
}

std::shared_ptr<const ALMetalPreparedWorldView> tryPrepareMaterial(
    Fixture& f, ALMetalWorldGBufferScene scene, const ALMetalWorldSimpleBatch& batch)
{
    auto inputs = ALMetalWorldViewInputs::adopt(scene, {batch}, {}, f.resources, 1, &f.error);
    if (!inputs) return {};
    auto ticket = ALMetalWorldPreparation::create(inputs, 1, {}, &f.error);
    if (!ticket || !ticket->runJob(0)) return {};
    return ticket->finish(&f.error);
}

void waitForMaterialRelease(const std::weak_ptr<const ALMetalBuffer>& weak)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!weak.expired() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    CHECK(weak.expired());
}
}

TEST_CASE("almetal: sealed material tables reuse native bytes across direct and prepared views",
          "[almetal][world-material-source]")
{
    @autoreleasepool
    {
        Fixture f;
        ALMetalWorldMaterialDataCache cache(*f.device);
        std::uint64_t frame = 1;
        for (unsigned family : {1u, 2u})
        for (bool indexed : {false, true})
        {
            CAPTURE(family, indexed);
            auto batch = materialBatch(f, family, indexed);
            auto source = ALMetalWorldMaterialSource::publish(*f.device, batch, cache, &f.error);
            INFO(f.error);
            REQUIRE(source);
            REQUIRE(source->matches(batch));
            REQUIRE(batch.native_material_source == source);
            auto equivalent = batch;
            equivalent.native_material_source.reset();
            const auto before_reuse = cache.stats();
            auto reused = ALMetalWorldMaterialSource::publish(*f.device, equivalent, cache, &f.error);
            REQUIRE(reused);
            CHECK(cache.stats().publications == before_reuse.publications);
            CHECK(cache.stats().published_bytes == before_reuse.published_bytes);
            CHECK(cache.stats().hits > before_reuse.hits);
            for (bool normal_hdr : {false, true})
            {
                CAPTURE(normal_hdr);
                auto table = tableLease(*source, family, normal_hdr);
                REQUIRE(table);
                CHECK(table == tableLease(*reused, family, normal_hdr));
                REQUIRE(table->mappedBytes());
                const auto* first = static_cast<const std::byte*>(table->mappedBytes());
                const std::vector<std::byte> original_bytes(first, first + table->length());
                for (unsigned view = 0; view < 3; ++view)
                {
                    CAPTURE(view);
                    const bool prepared = view != 0;
                    auto view_batch = batch;
                    view_batch.modelview_projection[12] = view == 2 ? .15f : 0.f;
                    auto scene = f.scene();
                    scene.hdr = true;
                    scene.normal_hdr = normal_hdr;
                    scene.reverse_z = view == 2;
                    REQUIRE(source->matches(view_batch));
                    auto manual = view_batch;
                    manual.native_material_source.reset();
                    const auto expected = renderMaterial(f, scene, manual, prepared, ++frame);
                    const auto actual = renderMaterial(f, scene, view_batch, prepared, ++frame);
                    CHECK(actual.pixels == expected.pixels);
                    CHECK(std::any_of(actual.pixels.depth.begin(), actual.pixels.depth.end(),
                        [reverse = scene.reverse_z](auto value)
                        { return value != (reverse ? 0u : 0x3f800000u); }));
                    // This measures real FrameData publication, including the
                    // prepared slab; a synthetic cache hit cannot satisfy it.
                    CHECK(expected.published_bytes == actual.published_bytes + table->length());
                    CHECK(cache.stats().publications == before_reuse.publications);
                    CHECK(cache.stats().published_bytes == before_reuse.published_bytes);
                    CHECK(std::memcmp(table->mappedBytes(), original_bytes.data(), table->length()) == 0);
                }
                REQUIRE(f.renderer->setAnisotropy(4, &f.error));
                CHECK(source->matches(batch));
                const auto sampler_before = cache.stats().published_bytes;
                auto scene = f.scene(); scene.normal_hdr = normal_hdr; scene.hdr = true;
                auto manual = batch; manual.native_material_source.reset();
                const auto sampled = renderMaterial(f, scene, batch, false, ++frame);
                const auto sampled_manual = renderMaterial(f, scene, manual, false, ++frame);
                CHECK(sampled.pixels == sampled_manual.pixels);
                CHECK(cache.stats().published_bytes == sampler_before);
            }
        }
    }
}

TEST_CASE("almetal: sealed material edits preserve pending native pixels and weak lease retirement",
          "[almetal][world-material-source]")
{
    @autoreleasepool
    {
        for (unsigned family : {1u, 2u})
        for (bool abandon : {false, true})
        {
            CAPTURE(family, abandon);
            Fixture f;
            ALMetalWorldMaterialDataCache cache(*f.device);
            auto batch = materialBatch(f, family, false);
            auto scene = f.scene(); scene.hdr = true;
            const auto expected = renderMaterial(f, scene, batch, false, 2).pixels;
            // These references remain in the producer's optional storage.
            // Successful sealing must not certify that mutable allocation.
            auto& old_material = batch.materialSlots();
            auto& old_pbr = batch.pbrSlots();
            auto source = ALMetalWorldMaterialSource::publish(*f.device, batch, cache, &f.error);
            REQUIRE(source);
            auto old_buffer = tableLease(*source, family, scene.normal_hdr);
            REQUIRE(old_buffer);
            std::weak_ptr<const ALMetalBuffer> weak = old_buffer;
            const auto* bytes = static_cast<const std::byte*>(old_buffer->mappedBytes());
            REQUIRE(bytes);
            const std::vector<std::byte> original_bytes(bytes, bytes + old_buffer->length());
            old_material[0].env_intensity = .99f;
            old_pbr[0].roughness_factor = .05f;
            CHECK(source->matches(batch));
            CHECK(std::memcmp(old_buffer->mappedBytes(), original_bytes.data(), old_buffer->length()) == 0);

            id<MTLSharedEvent> gate = [(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];
            REQUIRE(gate);
            struct ReleaseGate { id<MTLSharedEvent> value; ~ReleaseGate() { value.signaledValue = 1; } } release{gate};
            if (!abandon)
            {
                auto blocker = f.device->makeCommandBuffer("Hold old native material generation", &f.error);
                REQUIRE(blocker);
                [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];
                REQUIRE(blocker->commit(&f.error));
            }
            REQUIRE(f.renderer->beginCommandBatch("Material generation retirement", &f.error));
            scene.batches = &batch; scene.batch_count = 1;
            auto target = f.renderer->render(scene, &f.error);
            INFO(f.error); REQUIRE(target);
            auto completion = f.renderer->commandBatchCompletion(); REQUIRE(completion);
            CHECK_FALSE(completion->isComplete());
            if (!abandon) REQUIRE(f.renderer->commitCommandBatch(&f.error));

            auto changed = batch;
            if (family == 1) changed.materialSlots()[0].env_intensity = .8f;
            else changed.pbrSlots()[0].roughness_factor = .9f;
            CHECK_FALSE(source->matches(changed));
            CHECK_FALSE(changed.native_material_source);
            auto replacement = ALMetalWorldMaterialSource::publish(*f.device, changed, cache, &f.error);
            REQUIRE(replacement);
            CHECK(tableLease(*replacement, family, scene.normal_hdr) != old_buffer);
            CHECK(std::memcmp(old_buffer->mappedBytes(), original_bytes.data(), old_buffer->length()) == 0);
            batch.native_material_source.reset(); source.reset(); old_buffer.reset();
            if (!abandon)
            {
                cache.clear();
                CHECK(cache.stats().entries == 0);
            }
            else CHECK(cache.stats().entries > 0);
            CHECK_FALSE(weak.expired());
            if (abandon)
            {
                f.renderer->cancelCommandBatch();
                waitForMaterialRelease(weak);
            }
            else
            {
                CHECK_FALSE(completion->isComplete());
                gate.signaledValue = 1;
                REQUIRE(f.device->waitUntilIdle(&f.error));
                CHECK(f.read(*target) == expected);
                waitForMaterialRelease(weak);
                auto edited_scene = f.scene(); edited_scene.hdr = true;
                const auto edited = renderMaterial(f, edited_scene, changed, true, 3).pixels;
                CHECK_FALSE(edited == expected);
                auto manual = changed; manual.native_material_source.reset();
                CHECK(renderMaterial(f, edited_scene, manual, true, 4).pixels == edited);
            }
        }
    }
}

TEST_CASE("almetal: stale material certificates preserve native fallback and malformed rejection",
          "[almetal][world-material-source]")
{
    @autoreleasepool
    {
        Fixture f;
        ALMetalWorldMaterialDataCache cache(*f.device);
        std::uint64_t frame = 10;
        for (unsigned family : {1u, 2u})
        {
            CAPTURE(family);
            auto batch = materialBatch(f, family, false);
            auto source = ALMetalWorldMaterialSource::publish(*f.device, batch, cache, &f.error);
            REQUIRE(source);
            if (family == 1)
            {
                // Forward/impostor BLEND prototypes share this factory. Their
                // valid stable table does not make BLEND a G-buffer family.
                auto blend = batch; blend.material_alpha_mode = 1;
                auto blend_source = ALMetalWorldMaterialSource::publish(*f.device, blend, cache, &f.error);
                REQUIRE(blend_source); REQUIRE(blend_source->matches(blend));
                auto scene = f.scene(); scene.batches = &blend; scene.batch_count = 1;
                CHECK_FALSE(f.renderer->render(scene, &f.error));
                CHECK_FALSE(tryPrepareMaterial(f, scene, blend));
                blend.material_alpha_mode = 4;
                const auto before = cache.stats().published_bytes;
                CHECK_FALSE(ALMetalWorldMaterialSource::publish(*f.device, blend, cache, &f.error));
                CHECK(blend.native_material_source == blend_source);
                CHECK(cache.stats().published_bytes == before);
            }
            for (unsigned edit = 0; edit < 3; ++edit)
            {
                CAPTURE(edit);
                auto stale = batch;
                if (family == 1)
                {
                    if (edit == 0) stale.materialSlots()[0].specular_color[0] = .9f;
                    if (edit == 1)
                    {
                        stale.materialSlots()[0].normal_texture = nullptr;
                        stale.materialSlots()[0].specular_texture = nullptr;
                    }
                    if (edit == 2) stale.material_alpha_mode = 2;
                }
                else
                {
                    if (edit == 0) stale.pbrSlots()[0].emissive_color[0] = .7f;
                    if (edit == 1) stale.pbrSlots()[0].texture_transform[0][2] = .5f;
                    if (edit == 2) stale.alpha_mask = true;
                }
                stale.native_material_source = source;
                CHECK_FALSE(source->matches(stale));
                auto manual = stale; manual.native_material_source.reset();
                for (bool prepared : {false, true})
                {
                    auto scene = f.scene(); scene.hdr = true;
                    const auto expected = renderMaterial(f, scene, manual, prepared, ++frame);
                    const auto actual = renderMaterial(f, scene, stale, prepared, ++frame);
                    CHECK(actual.pixels == expected.pixels);
                    CHECK(actual.published_bytes == expected.published_bytes);
                }
            }
            for (unsigned defect = 0; defect < 6; ++defect)
            {
                CAPTURE(defect);
                auto invalid = batch;
                if (defect == 0) invalid.texture_count = 2;
                if (defect == 1)
                {
                    if (family == 1) invalid.material_slot_count = 9;
                    else invalid.pbr_slot_count = 9;
                }
                if (defect == 2)
                {
                    if (family == 1) invalid.material_alpha_mode = 1;
                    else invalid.pbrSlots()[0].roughness_factor = 1.1f;
                }
                if (defect == 3)
                {
                    if (family == 1)
                    {
                        invalid.texture_count = invalid.material_slot_count = 2;
                        invalid.diffuse_textures[1] = f.color;
                        invalid.materialSlots()[1] = invalid.materialSlots()[0];
                        invalid.materialSlots()[1].normal_texture = nullptr;
                    }
                    else invalid.pbrSlots()[0].normal_texture = nullptr;
                }
                if (defect == 4)
                {
                    if (family == 1) invalid.materialSlots()[0].env_intensity = std::numeric_limits<float>::infinity();
                    else invalid.pbrSlots()[0].alpha_cutoff = std::numeric_limits<float>::quiet_NaN();
                }
                if (defect == 5) invalid.vertices = nullptr;
                invalid.native_material_source = source;
                // A material certificate does not certify dynamic geometry.
                // Even a valid certificate must still run normal preflight.
                if (defect == 5) CHECK(source->matches(invalid));
                else CHECK_FALSE(source->matches(invalid));
                auto scene = f.scene(); scene.batches = &invalid; scene.batch_count = 1;
                CHECK_FALSE(f.renderer->render(scene, &f.error));
                CHECK_FALSE(tryPrepareMaterial(f, scene, invalid));
                invalid.native_material_source.reset();
                CHECK_FALSE(f.renderer->render(scene, &f.error));
                CHECK_FALSE(tryPrepareMaterial(f, scene, invalid));
            }
        }
    }
}

// Complete owner-side work, including capture construction, adoption, real
// credited GCD preparation, native encoding, CPU storage retirement and submit.
// The final interval additionally waits for all three views on the native GPU.
// Hidden because these controlled costs are not a viewer FPS measurement.
TEST_CASE("almetal: three-view stable material complete path cost",
          "[almetal][.world-material-source-cost]")
{
    @autoreleasepool
    {
        Fixture f;
        auto probes = ALMetalWorldGBufferRenderer::create(*f.device, &f.error);
        INFO(f.error); REQUIRE(probes);
        probes->setFrameData(f.data);
        ALMetalWorldMaterialDataCache cache(*f.device);
        ALNativeFrameBudget budget;
        std::uint64_t serial = 1;
        const auto thread_cpu_ns = []
        {
            timespec time{};
            REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) == 0);
            return static_cast<std::uint64_t>(time.tv_sec) * 1'000'000'000ULL + time.tv_nsec;
        };
        for (unsigned family : {1u, 2u})
        {
            std::array<ALMetalWorldSimpleBatch, 8> raw, stable;
            for (unsigned variant = 0; variant < raw.size(); ++variant)
            {
                raw[variant] = materialBatch(f, family, false);
                if (family == 1)
                {
                    raw[variant].materialSlots()[0].env_intensity = .05f + variant * .1f;
                    raw[variant].materialSlots()[0].specular_color[0] = .1f + variant * .1f;
                }
                else
                {
                    raw[variant].pbrSlots()[0].roughness_factor = .1f + variant * .1f;
                    raw[variant].pbrSlots()[0].emissive_color[0] = variant * .05f;
                }
                stable[variant] = raw[variant];
            }
            const auto publication_before = cache.stats();
            const auto publication_cpu_start = thread_cpu_ns();
            for (auto& prototype : stable)
            {
                auto source = ALMetalWorldMaterialSource::publish(*f.device, prototype, cache, &f.error);
                INFO(f.error); REQUIRE(source);
            }
            const auto publication_cpu_ns = thread_cpu_ns() - publication_cpu_start;
            const auto publication_after = cache.stats();
            std::cout << "world_material_source_setup family=" << family
                << " variants=8 publication_cpu_ms=" << publication_cpu_ns / 1e6
                << " native_publication_bytes=" << publication_after.published_bytes - publication_before.published_bytes
                << " native_publications=" << publication_after.publications - publication_before.publications
                << " renderer_owners=2 shared_frame_data=1 probe_views=2 probe_draws=128"
                   " encode_clock=host_wall_including_worker_join cpu_clock=owner_thread_plus_worker_kernel\n";

            std::array<Pixels, 3> reference_pixels;
            for (unsigned main_count : {128u, 1024u, 4096u})
            for (unsigned mode = 0; mode < 3; ++mode)
            {
                constexpr unsigned samples = 12;
                std::array<double, samples> encode_ms{}, cpu_ms{}, complete_ms{};
                std::array<std::vector<ALMetalWorldSimpleBatch>, 3> capture_batches;
                std::array<std::vector<ALMetalWorldVisibilityQuery>, 3> capture_queries;
                const std::array<unsigned, 3> counts{main_count, 128, 128};
                for (unsigned view = 0; view < 3; ++view) capture_batches[view].reserve(counts[view]);
                std::uint64_t measured_publication_bytes = 0;
                unsigned measured_worker_jobs = 0;
                for (unsigned sample = 0; sample < samples + 2; ++sample)
                {
                    std::array<std::unique_ptr<ALMetalRenderTarget>, 3> targets;
                    const auto start = std::chrono::steady_clock::now();
                    const auto owner_cpu_start = thread_cpu_ns();
                    std::uint64_t worker_cpu_ns = 0;
                    unsigned worker_jobs = 0;
                    // One publication/admission epoch for main plus both
                    // probes, even though they have separate renderer owners.
                    f.data->beginFrame(++serial);
                    ALNativeFrameBudget::FrameInputs frame;
                    frame.frame = serial;
                    frame.frame_interval_ns = 16'666'667;
                    budget.beginFrame(frame);
                    const auto before_bytes = f.data->stats().published_bytes;
                    for (unsigned view = 0; view < 3; ++view)
                    {
                        auto& batches = capture_batches[view];
                        const auto& prototypes = mode == 0 ? raw : stable;
                        auto scene = f.scene(); scene.hdr = true;
                        const float camera_x = .01f * static_cast<float>(sample) + .04f * view;
                        for (unsigned draw = 0; draw < counts[view]; ++draw)
                        {
                            batches.push_back(prototypes[draw % prototypes.size()]);
                            // Region-space geometry keeps one common camera
                            // per view. No unique per-object transform cost.
                            batches.back().modelview_projection[12] = camera_x;
                        }
                        scene.batches = batches.data(); scene.batch_count = batches.size();
                        scene.source_owner = f.resources;
                        auto& renderer = view == 0 ? *f.renderer : *probes;
                        REQUIRE(renderer.beginCommandBatch("Three-view material cost", &f.error));
                        if (mode == 0) targets[view] = renderer.render(scene, &f.error);
                        else
                        {
                            auto inputs = ALMetalWorldViewInputs::adopt(scene, std::move(batches),
                                std::move(capture_queries[view]), f.resources, serial, &f.error);
                            INFO(f.error); REQUIRE(inputs);
                            std::shared_ptr<const ALMetalPreparedWorldView> prepared;
                            if (mode == 2 && counts[view] >= 1024)
                            {
                                auto work = ALMetalWorldPreparation::create(inputs, 2, {}, &f.error);
                                REQUIRE(work);
                                auto credit = work->jobCount() > 1
                                    ? budget.tryAcquire(ALNativeFrameBudget::Work::preparation).credit : nullptr;
                                if (credit)
                                {
                                    struct Range
                                    {
                                        ALMetalWorldPreparation* work;
                                        bool succeeded = false;
                                        bool clock_valid = false;
                                        std::uint64_t cpu_ns = 0;
                                    } range{work.get()};
                                    auto group = dispatch_group_create();
                                    dispatch_group_async_f(group, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                                        &range, [](void* context)
                                        {
                                            auto& range = *static_cast<Range*>(context);
                                            timespec begin{}, end{};
                                            const bool began = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &begin) == 0;
                                            range.succeeded = range.work->runJob(1);
                                            range.clock_valid = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end) == 0 && began;
                                            range.cpu_ns = static_cast<std::uint64_t>(end.tv_sec - begin.tv_sec) * 1'000'000'000ULL
                                                + end.tv_nsec - begin.tv_nsec;
                                        });
                                    const bool owner_succeeded = work->runJob(0);
                                    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
#if !OS_OBJECT_USE_OBJC
                                    dispatch_release(group);
#endif
                                    REQUIRE(owner_succeeded); REQUIRE(range.succeeded); REQUIRE(range.clock_valid);
                                    worker_cpu_ns += range.cpu_ns; ++worker_jobs;
                                }
                                else
                                    for (std::size_t job = 0; job < work->jobCount(); ++job) REQUIRE(work->runJob(job));
                                prepared = work->finish(&f.error);
                                INFO(f.error); REQUIRE(prepared);
                                work.reset(); credit.reset();
                                targets[view] = renderer.renderPrepared(*prepared, &f.error);
                            }
                            else
                            {
                                scene = inputs->scene(); scene.source_owner = inputs->resourceLease();
                                targets[view] = renderer.render(scene, &f.error);
                            }
                            INFO(f.error); REQUIRE(targets[view]);
                            prepared.reset();
                            REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs, batches, capture_queries[view]));
                            CHECK(batches.empty()); CHECK(capture_queries[view].empty());
                        }
                        INFO(f.error); REQUIRE(targets[view]);
                        REQUIRE(renderer.commitCommandBatch(&f.error));
                        batches.clear();
                    }
                    const auto owner_cpu_ns = thread_cpu_ns() - owner_cpu_start;
                    const auto encoded = std::chrono::steady_clock::now();
                    REQUIRE(f.device->waitUntilIdle(&f.error));
                    const auto completed = std::chrono::steady_clock::now();
                    CHECK(cache.stats().published_bytes == publication_after.published_bytes);
                    if (sample == 1)
                    {
                        // Untimed second warmup verifies all three output
                        // attachments/coverage owners before reporting costs.
                        for (unsigned view = 0; view < 3; ++view)
                        {
                            const auto actual = f.read(*targets[view]);
                            if (mode == 0) reference_pixels[view] = actual;
                            else CHECK(actual == reference_pixels[view]);
                        }
                    }
                    if (sample >= 2)
                    {
                        const auto index = sample - 2;
                        encode_ms[index] = std::chrono::duration<double, std::milli>(encoded - start).count();
                        cpu_ms[index] = (owner_cpu_ns + worker_cpu_ns) / 1e6;
                        complete_ms[index] = std::chrono::duration<double, std::milli>(completed - start).count();
                        measured_publication_bytes += f.data->stats().published_bytes - before_bytes;
                        measured_worker_jobs += worker_jobs;
                    }
                }
                const auto summary = [](auto values)
                {
                    std::sort(values.begin(), values.end());
                    return std::array<double, 2>{(values[5] + values[6]) * .5, values.back()};
                };
                const auto encode = summary(encode_ms), cpu = summary(cpu_ms), complete = summary(complete_ms);
                std::cout << "world_material_source_three_view family=" << family << " main_count=" << main_count
                    << " mode=" << (mode == 0 ? "raw_direct" : mode == 1 ? "stable_scalar" : "stable_adaptive")
                    << " samples=12 warmups=2 encode_median_ms=" << encode[0] << " encode_max_ms=" << encode[1]
                    << " cpu_median_ms=" << cpu[0] << " cpu_max_ms=" << cpu[1]
                    << " complete_median_ms=" << complete[0] << " complete_max_ms=" << complete[1]
                    << " sampled_publication_bytes_total=" << measured_publication_bytes
                    << " credited_worker_jobs=" << measured_worker_jobs << '\n';
            }
        }
    }
}
