/**
 * @file almetal_shadow_source_benchmark.mm
 * @brief Matched legacy-owned versus retained-source native shadow cost.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalshadowprepare.h"
#include "../almetalshadowindirect.h"
#include "llmath.h"
#include "llmatrix4a.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <limits>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace
{
using Clock = std::chrono::steady_clock;
using Batch = ALMetalWorldSpotShadowBatch;
using Source = ALMetalShadowSourceRecord;
using Instance = ALMetalShadowPrepareInstance;
using Context = ALMetalShadowPrepareContext;
constexpr std::uint32_t width = 96, height = 32, joints = 64;
constexpr std::size_t weight_offset = 64, uv_offset = 128, color_offset = 160;
struct Case
{
    const char* name;
    std::uint32_t views;
    std::size_t draws, sources, slots;
    bool single_rigged_pbr_family = false;
};
constexpr std::array<Case, 4> cases{{
    {"light_1v_32d_4s_1t", 1, 32, 4, 1},
    {"reused_4v_256d_8s_8t", 4, 256, 8, 8},
    {"reused_6v_1024d_16s_8t", 6, 1024, 16, 8},
    {"large_6v_8192d_16s_8t", 6, 8192, 16, 8, true},
}};
double ms(Clock::time_point first, Clock::time_point last)
{ return std::chrono::duration<double, std::milli>(last - first).count(); }
bool enabled(const char* name)
{
    const auto* value = std::getenv(name);
    return value && value[0] && value[0] != '0';
}
struct Sample
{
    double prep_encode_ms = 0, commit_ms = 0, wait_ms = 0, release_ms = 0, gpu_ms = 0;
    double gpu_span_ms = 0, wall_ms = 0;
    std::size_t commands = 0, prepared_jobs = 0;
    std::uint64_t certified = 0;
    double cpu() const { return prep_encode_ms + commit_ms + release_ms; }
};
struct PacketOwner
{
    std::vector<std::shared_ptr<const Source>> sources;
    std::vector<std::shared_ptr<const ALMetalAttachmentPaletteRecord>> palettes;
    std::unordered_set<const Source*> source_ids;
    std::unordered_set<const ALMetalAttachmentPaletteRecord*> palette_ids;
    const Source* retain(const std::shared_ptr<const Source>& source)
    {
        const auto [at, inserted] = source_ids.insert(source.get());
        if (inserted)
        {
            try { sources.push_back(source); }
            catch (...) { source_ids.erase(at); throw; }
        }
        return source.get();
    }
    const ALMetalAttachmentPaletteRecord* retain(
        const std::shared_ptr<const ALMetalAttachmentPaletteRecord>& palette)
    {
        const auto [at, inserted] = palette_ids.insert(palette.get());
        if (inserted)
        {
            try { palettes.push_back(palette); }
            catch (...) { palette_ids.erase(at); throw; }
        }
        return palette.get();
    }
};
struct CancelBatch
{
    ALMetalWorldGBufferRenderer& renderer;
    ~CancelBatch() { if (renderer.commandBatchActive()) renderer.cancelCommandBatch(); }
};
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::shared_ptr<ALMetalWorldFrameData> arena;
    std::shared_ptr<const ALMetalBuffer> vertices, indices16, indices32;
    std::shared_ptr<const ALMetalTexture> alpha;
    std::array<std::unique_ptr<ALMetalTexture>, 2> targets;
    std::uint64_t epoch = 0;

    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error); INFO(error); REQUIRE(renderer);
        arena = ALMetalWorldFrameData::create(*device, &error); REQUIRE(arena);
        renderer->setFrameData(arena);
        const std::array<std::array<float, 4>, 4> positions{{
            {{-.75f,-.75f,0.f,1.f}}, {{.75f,-.75f,0.f,1.f}},
            {{-.75f,.75f,0.f,1.f}}, {{.75f,.75f,0.f,1.f}}}};
        const std::array<std::array<float, 4>, 4> weights{{
            {{.5f,0.f,0.f,0.f}}, {{.5f,0.f,0.f,0.f}},
            {{.5f,0.f,0.f,0.f}}, {{.5f,0.f,0.f,0.f}}}};
        const std::array<std::array<float, 2>, 4> uv{{
            {{0.f,1.f}}, {{1.f,1.f}}, {{0.f,0.f}}, {{1.f,0.f}}}};
        std::array<std::uint8_t, 176> bytes{};
        std::memcpy(bytes.data(), positions.data(), sizeof(positions));
        std::memcpy(bytes.data() + weight_offset, weights.data(), sizeof(weights));
        std::memcpy(bytes.data() + uv_offset, uv.data(), sizeof(uv));
        std::fill(bytes.begin() + color_offset, bytes.end(), std::uint8_t{255});
        ALMetalBufferDescriptor bd; bd.length = bytes.size();
        auto vb = ALMetalBuffer::create(*device, bd, &error); REQUIRE(vb);
        REQUIRE(vb->write(0, bytes.data(), bytes.size(), &error)); vertices = std::move(vb);
        const std::array<std::uint16_t, 6> short_ids{{0,1,2,1,3,2}};
        const std::array<std::uint32_t, 6> long_ids{{0,1,2,1,3,2}};
        bd.length = sizeof(short_ids);
        auto ib16 = ALMetalBuffer::create(*device, bd, &error); REQUIRE(ib16);
        REQUIRE(ib16->write(0, short_ids.data(), sizeof(short_ids), &error)); indices16 = std::move(ib16);
        bd.length = sizeof(long_ids);
        auto ib32 = ALMetalBuffer::create(*device, bd, &error); REQUIRE(ib32);
        REQUIRE(ib32->write(0, long_ids.data(), sizeof(long_ids), &error)); indices32 = std::move(ib32);
        ALMetalTextureDescriptor td; td.width = 2; td.height = 1;
        td.format = ALMetalPixelFormat::rgba8_unorm; td.usage = ALMetalTextureUsage::shader_read;
        auto image = ALMetalTexture::create(*device, td, &error); REQUIRE(image);
        const std::array<std::uint8_t, 8> pixels{{255,255,255,0,255,255,255,255}};
        auto upload = device->makeCommandBuffer("Shadow source benchmark alpha", &error); REQUIRE(upload);
        REQUIRE(image->upload(*upload, 0, 0, {}, pixels.data(), 8, &error));
        REQUIRE(upload->commitAndWait(&error)); alpha = std::move(image);
        td.type = ALMetalTextureType::texture_2d_array; td.width = width; td.height = height;
        td.array_length = 6; td.format = ALMetalPixelFormat::r32_float;
        td.usage = ALMetalTextureUsage::render_target | ALMetalTextureUsage::shader_read;
        for (auto& target : targets) { target = ALMetalTexture::create(*device, td, &error); REQUIRE(target); }
    }

    Batch prototype(const Case& c, std::size_t source,
                    const std::shared_ptr<const ALMetalAttachmentPaletteRecord>& pose)
    {
        Batch batch;
        batch.vertices = vertices.get(); batch.vertex_count = 4;
        batch.indices = (source / 2 % 2 ? indices32 : indices16).get();
        batch.index_type = source / 2 % 2 ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
        batch.index_count = 6; batch.double_sided = true;
        batch.texcoord_offset = uv_offset; batch.color_offset = color_offset;
        const auto kind = c.single_rigged_pbr_family ? 2 : source % 3;
        batch.coverage = kind == 0 ? ALMetalWorldSpotShadowCoverage::opaque :
            kind == 1 ? ALMetalWorldSpotShadowCoverage::legacy_mask : ALMetalWorldSpotShadowCoverage::pbr_mask;
        if (kind)
        {
            batch.texture_count = c.slots;
            for (std::size_t slot = 0; slot < c.slots; ++slot)
            {
                batch.coverageData().textures[slot] = alpha.get();
                batch.coverageData().alpha_cutoffs[slot] = .35f + float(slot % 4) * .1f;
                batch.coverageData().pbr_base_color_transforms[slot][0][0] = 1.f;
                batch.coverageData().pbr_base_color_transforms[slot][0][1] = 1.f;
                batch.coverageData().pbr_base_color_transforms[slot][1][0] = float(slot) / 32.f;
            }
        }
        if (c.single_rigged_pbr_family || source % 2)
        {
            batch.weight_offset = weight_offset;
            batch.skin_palette_record = pose; batch.skin_palette = pose->data();
            batch.skin_palette_float_count = pose->size(); batch.skin_joint_count = pose->jointCount();
        }
        return batch;
    }

    std::array<float, 16> modelFor(std::size_t draw, std::size_t source_index)
    {
        auto value = alMetalShadowIdentity;
        value[0] = .3f;
        value[12] = (float(source_index % 3) - 1.f) * .625f + float(draw % 8) / 1024.f;
        return value;
    }

    Sample frame(const Case& c, bool retained, unsigned pose_epoch)
    {
        const auto before = renderer->shadowIndirectStats();
        Sample result;
        const auto started = Clock::now();
        auto prep_started = started;
        Clock::time_point release_started;
        @autoreleasepool
        {
            arena->beginFrame(++epoch);
            CancelBatch cancel{*renderer};
            std::vector<float> values(4 + joints * 12, 0.f);
            for (unsigned joint = 0; joint < joints; ++joint)
                values[4 + joint * 12] = values[9 + joint * 12] = values[14 + joint * 12] = 1.f;
            // Move the rigged draw toward the light so its changed pose is
            // visible even where a static source overlaps the same region.
            values[15] = -float(pose_epoch % 2) / 8.f;
            auto pose = ALMetalAttachmentPaletteRecord::create(values.data(), values.size(), joints, &error);
            if (!pose) throw std::runtime_error(error);
            std::vector<Batch> prototypes;
            prototypes.reserve(c.sources);
            for (std::size_t at = 0; at < c.sources; ++at) prototypes.push_back(prototype(c, at, pose));
            std::vector<std::shared_ptr<const Source>> sources;
            if (retained)
            {
                sources.reserve(c.sources);
                for (std::size_t at = 0; at < c.sources; ++at)
                {
                    const auto& p = prototypes[at];
                    std::vector<std::shared_ptr<const ALMetalTexture>> textures(p.texture_count, alpha);
                    auto source = Source::create(at + 1, p, vertices,
                        p.index_type == ALMetalIndexType::uint32 ? indices32 : indices16,
                        std::move(textures), &error);
                    if (!source) throw std::runtime_error(error);
                    sources.push_back(std::move(source));
                }
            }
            std::vector<std::shared_ptr<PacketOwner>> owners;
            std::vector<std::shared_ptr<const ALMetalCommandCompletion>> completions;
            owners.reserve(c.views); completions.reserve(c.views);
            std::vector<Instance> instances;
            instances.reserve(retained && c.draws >= 8192 ? c.draws : 0);
            std::vector<Batch> batches;
            batches.reserve(c.draws);
            for (std::uint32_t view = 0; view < c.views; ++view)
            {
                // Match production's independently committed, frame-budgeted
                // views. Neither arm waits before encoding the following view.
                if (!renderer->beginShadowFrameBatch(epoch, "Streamed shadow source cost", &error))
                    throw std::runtime_error(error);
                auto owner = std::make_shared<PacketOwner>();
                batches.clear(); batches.resize(c.draws);
                Context context;
                context.view[14] = float(view) / 256.f;
                const LLMatrix4a view_matrix(context.view.data()), projection(context.projection.data());
                if (retained && c.draws >= 8192)
                {
                    instances.clear();
                    for (std::size_t draw = 0; draw < c.draws; ++draw)
                    {
                        const auto at = draw % c.sources;
                        Instance value;
                        value.source = owner->retain(sources[at]);
                        value.source_generation = sources[at]->generation();
                        value.epoch = epoch; value.has_model = true;
                        value.model = modelFor(draw, at);
                        if (sources[at]->requiresPalette()) value.palette = owner->retain(pose);
                        instances.push_back(value);
                    }
                    ALMetalShadowPrepareStats stats;
                    if (!alMetalPrepareShadowBatches(instances, context, batches, epoch, nullptr, true, &stats, &error))
                        throw std::runtime_error(error);
                    result.prepared_jobs += stats.jobs;
                }
                else
                {
                    for (std::size_t draw = 0; draw < c.draws; ++draw)
                    {
                        const auto at = draw % c.sources;
                        auto& batch = batches[draw];
                        if (retained)
                        {
                            const auto* source = owner->retain(sources[at]);
                            const auto& prototype = source->prototype();
                            const auto* palette = source->requiresPalette() ? owner->retain(pose) : nullptr;
                            static_cast<ALMetalWorldSpotShadowValues&>(batch) = prototype;
                            // Small families use ordinary backend validation;
                            // cached immutable values avoid input/output passes
                            // and a public-field source-certificate comparison.
                            batch.borrowPreparedInputs(nullptr, palette, prototype.coverageDataIdentity());
                            if (palette)
                            {
                                batch.skin_palette = palette->data();
                                batch.skin_palette_float_count = palette->size();
                                batch.skin_joint_count = palette->jointCount();
                            }
                        }
                        else batch = prototypes[at];
                        const auto model_value = modelFor(draw, at);
                        LLMatrix4a model(model_value.data()), modelview, mvp;
                        modelview.setMul(model, view_matrix); mvp.setMul(modelview, projection);
                        std::memcpy(batch.modelview_projection, mvp.getF32ptr(), sizeof(batch.modelview_projection));
                    }
                }
                ALMetalWorldSpotShadowScene scene;
                // Both arms hold a common owner until callbacks drop their
                // references, charging final destruction to this same sample.
                // The retained arm pins records once per ACTUAL command.
                scene.source_owner = owner;
                scene.batches = batches.data(); scene.batch_count = batches.size();
                scene.width = width; scene.height = height;
                scene.material_buffers = false; scene.indirect_draws = false;
                if (!renderer->renderSpotShadow(*targets[retained], view, scene, &error))
                    throw std::runtime_error(error);
                auto completion = renderer->commandBatchCompletion();
                if (!completion) throw std::runtime_error("Missing native source benchmark completion");
                owners.push_back(owner); completions.push_back(std::move(completion));
                const auto encoded = Clock::now(); result.prep_encode_ms += ms(prep_started, encoded);
                if (!renderer->commitCommandBatch(&error)) throw std::runtime_error(error);
                const auto committed = Clock::now(); result.commit_ms += ms(encoded, committed);
                prep_started = committed; ++result.commands;
            }
            const auto wait_started = Clock::now();
            result.prep_encode_ms += ms(prep_started, wait_started);
            const auto settled = [&] {
                for (std::size_t at = 0; at < completions.size(); ++at)
                    if (!completions[at]->isComplete() || owners[at].use_count() != 1) return false;
                return true;
            };
            const auto deadline = wait_started + std::chrono::seconds(10);
            while (!settled() && Clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            if (!settled()) throw std::runtime_error("Source benchmark GPU/owner completion timed out");
            release_started = Clock::now();
            result.wait_ms = ms(wait_started, release_started);
            double first_gpu = std::numeric_limits<double>::infinity(), last_gpu = 0;
            for (const auto& completion : completions)
            {
                ALMetalCommandGPUTime gpu;
                if (completion->gpuTime(gpu) != ALMetalCommandTimingStatus::ready)
                    throw std::runtime_error("Source benchmark GPU command failed or has no valid timestamps");
                result.gpu_ms += double(gpu.elapsed_nanoseconds) / 1e6;
                first_gpu = std::min(first_gpu, gpu.start_seconds);
                last_gpu = std::max(last_gpu, gpu.end_seconds);
            }
            result.gpu_span_ms = (last_gpu - first_gpu) * 1000;
            arena->beginFrame(++epoch);
            batches.clear(); owners.clear(); sources.clear(); prototypes.clear(); pose.reset(); completions.clear();
        }
        const auto finished = Clock::now();
        result.release_ms = ms(release_started, finished); result.wall_ms = ms(started, finished);
        result.certified = renderer->shadowIndirectStats().certified_source_validations -
            before.certified_source_validations;
        REQUIRE(result.certified == (retained && c.draws >= 8192 ? c.draws * c.views : 0));
        REQUIRE(result.commands == c.views);
        REQUIRE(result.prepared_jobs == (retained && c.draws >= 8192 ? c.views * 2 : 0));
        return result;
    }

    std::vector<float> pixels(bool retained, const Case& c)
    {
        std::vector<float> data(width * height * c.views);
        for (std::uint32_t view = 0; view < c.views; ++view)
            REQUIRE(targets[retained]->readback(*device, 0, view, {},
                data.data() + view * width * height, width * sizeof(float), &error));
        return data;
    }
    void parity()
    {
        for (const auto& c : cases)
        {
            CAPTURE(c.name);
            std::vector<float> old;
            for (unsigned pose = 0; pose < 2; ++pose)
            {
                frame(c, false, pose); frame(c, true, pose);
                const auto raw = pixels(false, c), retained = pixels(true, c);
                REQUIRE(raw == retained);
                REQUIRE(std::all_of(raw.begin(), raw.end(), [](float d) { return std::isfinite(d); }));
                // Three separate screen regions keep opaque draws from
                // hiding a missing covered-family producer.
                std::array<std::size_t, 3> visible{};
                for (unsigned y = 0; y < height; ++y)
                    for (unsigned x = 0; x < width; ++x)
                        visible[x / 32] += raw[y * width + x] < .99f;
                for (auto count : visible) REQUIRE(count > 0);
                if (!c.single_rigged_pbr_family)
                { REQUIRE(visible[1] < visible[0]); REQUIRE(visible[2] < visible[0]); }
                if (!old.empty()) REQUIRE(raw != old);
                old = raw;
            }
        }
    }
};
double percentile(std::vector<double> values, double fraction)
{
    std::sort(values.begin(), values.end());
    return values[std::max<std::size_t>(1, std::size_t(std::ceil(values.size() * fraction))) - 1];
}
}

TEST_CASE("almetal: raw and retained source cost fixtures preserve native output",
          "[almetal][shadow-source-cost-parity]")
{
    @autoreleasepool { Fixture fixture; fixture.parity(); }
}

TEST_CASE("almetal: opt-in complete raw versus retained shadow source CPU cost",
          "[.][almetal][shadow-source-cost]")
{
    if (!enabled("ALCHEMY_SHADOW_SOURCE_BENCHMARK"))
        SKIP("Set ALCHEMY_SHADOW_SOURCE_BENCHMARK=1 for the explicit timing run");
#ifndef NDEBUG
    SKIP("Source timing requires an optimized build");
#endif
    REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER")); REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
    REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED")); REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
    @autoreleasepool
    {
        Fixture fixture; fixture.parity();
        std::cout << std::setprecision(12)
            << "SHADOW_SOURCE_SCOPE mode=same_binary_legacy_owned_vs_retained_cached joints=" << joints
            << " includes=pose_prototypes_source_seal_inputs_batch_matrices_render_commit_release_pool_drain"
               " native_GPU_envelope_ms=sum_per_view_commands native_GPU_span_ms=logical_frame_span"
               " GPU_wait_ms=excluded_from_CPU logical_wall_ms=CPU_plus_wait"
               " excludes=live_LLDrawInfo_key_admission_cull_pose_solve FPS_claim=0"
               " raw_control=current_owned_API_COW_explicit_model_shared_view_context"
               " v2_control_change=removed_artificial_per_draw_three_matrix_input_temporary"
               " source_publication=cold_each_arm_all_declared_sources"
               " retained_path=owner_values_below8192_compact_dispatch_at8192"
               " native_commands=per_view_frame_budgeted source_certificate=large_only"
               " retained_ownership=aggregate_per_actual_command_pointer_dedup_source_pose_once"
               " v3_change=source_kernel_default_and_per_view_streaming"
               " large_case=one_rigged_PBR_mask_family8192_selected_from_fixed_compact_trial"
               " source_reuse=controlled16_sources_not_live_LLDrawInfo_distribution\n";
        unsigned sample_id = 0;
        for (const auto& c : cases)
        {
            for (unsigned warm = 0; warm < 2; ++warm)
            { fixture.frame(c, false, warm); fixture.frame(c, true, warm); }
            std::array<std::vector<Sample>, 2> results;
            for (unsigned block = 0; block < 10; ++block)
            for (unsigned repetition = 0; repetition < 5; ++repetition)
            for (unsigned arm = 0; arm < 4; ++arm)
            {
                const bool retained = (arm == 1 || arm == 2) != bool(block % 2);
                const auto sample = fixture.frame(c, retained, block * 5 + repetition);
                results[retained].push_back(sample);
                std::cout << "SHADOW_SOURCE_SAMPLE case=" << c.name
                    << " variant=" << (retained ? "retained" : "raw") << " sample=" << sample_id++
                    << " block=" << block << " repetition=" << repetition << " arm=" << arm
                    << " views=" << c.views << " draws=" << c.draws << " sources=" << c.sources
                    << " family=" << (c.single_rigged_pbr_family ? "rigged_pbr_mask" : "mixed")
                    << " slots=" << c.slots << " cpu_total_ms=" << sample.cpu()
                    << " prep_encode_ms=" << sample.prep_encode_ms << " commit_ms=" << sample.commit_ms
                    << " wait_ms=" << sample.wait_ms << " release_ms=" << sample.release_ms
                    << " gpu_envelope_ms=" << sample.gpu_ms << " gpu_span_ms=" << sample.gpu_span_ms
                    << " logical_wall_ms=" << sample.wall_ms << " commands=" << sample.commands
                    << " prepared_jobs=" << sample.prepared_jobs << " certified=" << sample.certified << '\n';
            }
            for (unsigned arm = 0; arm < 2; ++arm)
            {
                REQUIRE(results[arm].size() == 100);
                std::vector<double> cpu, gpu, span, wall;
                for (const auto& sample : results[arm])
                {
                    cpu.push_back(sample.cpu()); gpu.push_back(sample.gpu_ms);
                    span.push_back(sample.gpu_span_ms); wall.push_back(sample.wall_ms);
                }
                std::cout << "SHADOW_SOURCE_SUMMARY case=" << c.name
                    << " variant=" << (arm ? "retained" : "raw") << " samples=100"
                    << " p50_cpu_ms=" << percentile(cpu, .5) << " p95_cpu_ms=" << percentile(cpu, .95)
                    << " p50_gpu_envelope_ms=" << percentile(gpu, .5)
                    << " p95_gpu_envelope_ms=" << percentile(gpu, .95)
                    << " p50_gpu_span_ms=" << percentile(span, .5) << " p95_gpu_span_ms=" << percentile(span, .95)
                    << " p50_logical_wall_ms=" << percentile(wall, .5)
                    << " p95_logical_wall_ms=" << percentile(wall, .95) << '\n';
            }
        }
    }
}
