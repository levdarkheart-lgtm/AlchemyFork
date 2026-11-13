/**
 * @file almetal_attachment_palette_benchmark.mm
 * @brief Complete attachment-palette preparation and shadow encoding timings.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetaldevice.h"
#include "../almetalworld.h"

#import <Foundation/Foundation.h>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;
using Record = ALMetalAttachmentPaletteRecord;
constexpr std::uint32_t width = 32, height = 16;
constexpr std::size_t position_bytes = 4 * 4 * sizeof(float);
constexpr std::size_t weight_offset = position_bytes;
constexpr std::size_t texcoord_offset = weight_offset + 4 * 4 * sizeof(float);
constexpr std::size_t color_offset = texcoord_offset + 4 * 2 * sizeof(float);
constexpr std::size_t vertex_bytes = color_offset + 4 * 4;

struct Scenario
{
    const char* name;
    std::size_t draws, poses;
    std::uint32_t joints, views;
    bool gate_repeated;
};
constexpr std::array<Scenario, 6> scenarios{{
    {"light_1v_32d_4p_32j", 32, 4, 32, 1, false},
    {"unique_1v_32d_32p_32j", 32, 32, 32, 1, false},
    {"repeated_4v_256d_8p_64j", 256, 8, 64, 4, true},
    {"repeated_6v_1024d_16p_110j", 1024, 16, 110, 6, true},
    {"adverse_unique_4v_256d_256p_64j", 256, 256, 64, 4, false},
    {"adverse_unique_6v_1024d_1024p_110j", 1024, 1024, 110, 6, false},
}};

double milliseconds(Clock::time_point first, Clock::time_point last)
{
    return std::chrono::duration<double, std::milli>(last - first).count();
}

bool enabled(const char* name)
{
    const char* value = std::getenv(name);
    return value && value[0] && value[0] != '0';
}

struct Sample
{
    double cpu_ms = 0, seal_ms = 0, commit_ms = 0, wait_ms = 0, input_release_ms = 0;
    std::uint64_t published_bytes = 0, reused_bytes = 0;
    std::uint64_t record_publications = 0, record_reuses = 0;
};

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::shared_ptr<ALMetalWorldFrameData> arena;
    std::unique_ptr<ALMetalBuffer> vertices, indices16, indices32;
    std::unique_ptr<ALMetalTexture> alpha, raw_target, record_target;
    std::vector<std::vector<float>> poses;
    std::vector<std::shared_ptr<const Record>> records;
    std::vector<ALMetalWorldSpotShadowBatch> batches;
    std::uint64_t epoch = 1;

    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error); REQUIRE(renderer);
        arena = ALMetalWorldFrameData::create(*device, &error); REQUIRE(arena);
        renderer->setFrameData(arena);
        const std::array<std::array<float, 4>, 4> points{{
            {{-.75f, -.75f, 0.f, 1.f}}, {{.75f, -.75f, 0.f, 1.f}},
            {{-.75f, .75f, 0.f, 1.f}}, {{.75f, .75f, 0.f, 1.f}},
        }};
        const std::array<std::array<float, 2>, 4> uv{{
            {{0.f, 1.f}}, {{1.f, 1.f}}, {{0.f, 0.f}}, {{1.f, 0.f}},
        }};
        std::array<std::uint8_t, vertex_bytes> data{};
        std::memcpy(data.data(), points.data(), sizeof(points));
        std::memcpy(data.data() + texcoord_offset, uv.data(), sizeof(uv));
        std::fill(data.begin() + color_offset, data.end(), std::uint8_t{255});
        ALMetalBufferDescriptor bd;
        bd.storage = ALMetalStorageMode::shared;
        bd.length = data.size(); bd.label = "Attachment palette cost vertex planes";
        vertices = ALMetalBuffer::create(*device, bd, &error); REQUIRE(vertices);
        REQUIRE(vertices->write(0, data.data(), data.size(), &error));
        const std::array<std::uint16_t, 6> short_ids{{0, 1, 2, 1, 3, 2}};
        const std::array<std::uint32_t, 6> long_ids{{0, 1, 2, 1, 3, 2}};
        bd.length = sizeof(short_ids);
        indices16 = ALMetalBuffer::create(*device, bd, &error); REQUIRE(indices16);
        REQUIRE(indices16->write(0, short_ids.data(), sizeof(short_ids), &error));
        bd.length = sizeof(long_ids);
        indices32 = ALMetalBuffer::create(*device, bd, &error); REQUIRE(indices32);
        REQUIRE(indices32->write(0, long_ids.data(), sizeof(long_ids), &error));
        ALMetalTextureDescriptor td;
        td.width = 2; td.height = 1; td.format = ALMetalPixelFormat::rgba8_unorm;
        td.usage = ALMetalTextureUsage::shader_read;
        alpha = ALMetalTexture::create(*device, td, &error); REQUIRE(alpha);
        const std::array<std::uint8_t, 8> alpha_values{{255,255,255,0,255,255,255,255}};
        auto upload = device->makeCommandBuffer("Attachment cost alpha preflight", &error);
        REQUIRE(upload);
        REQUIRE(alpha->upload(*upload, 0, 0, {}, alpha_values.data(), 8, &error));
        REQUIRE(upload->commitAndWait(&error));
        td.type = ALMetalTextureType::texture_2d_array;
        td.width = width; td.height = height; td.array_length = 6;
        td.format = ALMetalPixelFormat::r32_float;
        td.storage = ALMetalStorageMode::private_gpu;
        td.usage = ALMetalTextureUsage::render_target | ALMetalTextureUsage::shader_read;
        raw_target = ALMetalTexture::create(*device, td, &error); REQUIRE(raw_target);
        record_target = ALMetalTexture::create(*device, td, &error); REQUIRE(record_target);
    }

    void prepare(const Scenario& s, unsigned pose_epoch)
    {
        poses.resize(s.poses);
        for (std::size_t p = 0; p < poses.size(); ++p)
        {
            auto& values = poses[p]; values.assign(4 + s.joints * 12, 0.f);
            // Simulated producer output is common input to both arms. Its
            // computation is not timed; immutable sealing/copy/validation is.
            values[2] = (pose_epoch % 2) * .0625f + float(p) / 65536.f;
            for (std::uint32_t joint = 0; joint < s.joints; ++joint)
            {
                const std::size_t base = 4 + joint * 12;
                values[base] = values[base + 5] = values[base + 10] = 1.f;
                values[base + 3] = float(int(joint % 3) - 1) / 2048.f;
                values[base + 7] = float(int(joint % 5) - 2) / 2048.f;
                values[base + 11] = float(joint) / 16384.f;
            }
        }
        std::array<std::array<float, 4>, 4> weights{};
        for (unsigned v = 0; v < weights.size(); ++v)
            weights[v] = {float(v % s.joints) + .25f,
                float(s.joints / 2) + .25f, float(s.joints - 1) + .5f, 0.f};
        REQUIRE(vertices->write(weight_offset, weights.data(), sizeof(weights), &error));
    }

    Sample frame(const Scenario& s, bool use_records, int family = -1)
    {
        Sample result;
        const auto before = arena->stats();
        const auto started = Clock::now();
        Clock::time_point release_started;
        @autoreleasepool
        {
            arena->beginFrame(++epoch);
            if (!renderer->beginCommandBatch("Attachment full shadow cost frame", &error))
                throw std::runtime_error(error);
            const auto seal_started = Clock::now();
            records.clear();
            if (use_records)
            {
                records.reserve(poses.size());
                for (const auto& pose : poses)
                {
                    auto record = Record::create(pose.data(), pose.size(), s.joints, &error);
                    if (!record) throw std::runtime_error(error);
                    records.push_back(std::move(record));
                }
            }
            const auto sealed = Clock::now();
            result.seal_ms = milliseconds(seal_started, sealed);
            for (std::uint32_t view = 0; view < s.views; ++view)
            {
                batches.clear(); batches.reserve(s.draws);
                for (std::size_t draw = 0; draw < s.draws; ++draw)
                {
                    auto& b = batches.emplace_back();
                    const auto p = draw % s.poses;
                    const unsigned kind = family < 0 ? unsigned(draw % 3) : unsigned(family);
                    b.vertices = vertices.get(); b.vertex_count = 4;
                    b.weight_offset = weight_offset;
                    b.indices = draw % 2 ? indices32.get() : indices16.get();
                    b.index_type = draw % 2 ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
                    b.index_count = 6; b.double_sided = true;
                    b.skin_palette = use_records ? records[p]->data() : poses[p].data();
                    b.skin_palette_float_count = use_records ? records[p]->size() : poses[p].size();
                    b.skin_joint_count = s.joints;
                    if (use_records) b.skin_palette_record = records[p];
                    for (unsigned axis = 0; axis < 4; ++axis) b.modelview_projection[axis * 4 + axis] = 1.f;
                    b.modelview_projection[14] = float(view) / 256.f;
                    if (family < 0)
                    {
                        b.modelview_projection[0] = .3f;
                        b.modelview_projection[12] = (float(kind) - 1.f) * .625f;
                    }
                    if (kind != 0)
                    {
                        b.texcoord_offset = texcoord_offset; b.color_offset = color_offset;
                        b.coverage = kind == 1 ? ALMetalWorldSpotShadowCoverage::legacy_mask
                                              : ALMetalWorldSpotShadowCoverage::pbr_mask;
                        b.texture_count = 1;
                        b.coverageData().textures[0] = alpha.get();
                        b.coverageData().alpha_cutoffs[0] = .5f;
                        b.coverageData().pbr_base_color_transforms[0][0][0] = 1.f;
                        b.coverageData().pbr_base_color_transforms[0][0][1] = 1.f;
                    }
                }
                ALMetalWorldSpotShadowScene scene;
                scene.batches = batches.data(); scene.batch_count = batches.size();
                scene.width = width; scene.height = height;
                scene.material_buffers = false;
                if (!renderer->renderSpotShadow(use_records ? *record_target : *raw_target,
                        view, scene, &error))
                    throw std::runtime_error(error);
            }
            auto completion = renderer->commandBatchCompletion();
            if (!completion) throw std::runtime_error("Missing benchmark command completion");
            const auto encoded = Clock::now();
            result.cpu_ms = milliseconds(started, encoded);
            if (!renderer->commitCommandBatch(&error)) throw std::runtime_error(error);
            const auto committed = Clock::now();
            result.commit_ms = milliseconds(encoded, committed);
            const auto deadline = committed + std::chrono::seconds(10);
            while (!completion->isComplete() && Clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            const auto completed = Clock::now();
            REQUIRE(completion->isComplete());
            result.wait_ms = milliseconds(committed, completed);
            release_started = Clock::now();
            // Charge this frame's arena-index retirement and input ownership
            // destruction to its own arm, not to whichever variant runs next.
            arena->beginFrame(++epoch);
            batches.clear(); records.clear(); completion.reset();
        }
        // Include the per-frame Objective-C autorelease pool drain in this
        // arm's release cost, after all frame-local ownership has retired.
        result.input_release_ms = milliseconds(release_started, Clock::now());
        const auto after = arena->stats();
        result.published_bytes = after.published_bytes - before.published_bytes;
        result.reused_bytes = after.reused_bytes - before.reused_bytes;
        result.record_publications = after.record_publications - before.record_publications;
        result.record_reuses = after.record_reuses - before.record_reuses;
        REQUIRE(result.record_publications == (use_records ? s.poses : 0));
        REQUIRE(result.record_reuses == (use_records ? s.draws * s.views - s.poses : 0));
        return result;
    }

    std::vector<float> pixels(bool record, std::uint32_t views)
    {
        std::vector<float> result(width * height * views);
        for (std::uint32_t view = 0; view < views; ++view)
            REQUIRE((record ? record_target : raw_target)->readback(*device, 0, view, {},
                result.data() + view * width * height, width * sizeof(float), &error));
        return result;
    }

    void preflight()
    {
        for (const auto& s : scenarios)
        {
            CAPTURE(s.name);
            for (unsigned pose_epoch = 0; pose_epoch < 2; ++pose_epoch)
            {
                prepare(s, pose_epoch);
                frame(s, false); frame(s, true);
                const auto raw = pixels(false, s.views), recorded = pixels(true, s.views);
                REQUIRE(std::memcmp(raw.data(), recorded.data(), raw.size() * sizeof(float)) == 0);
                REQUIRE(std::all_of(raw.begin(), raw.end(), [](float d) { return std::isfinite(d); }));
                REQUIRE(std::count_if(raw.begin(), raw.end(), [](float d) { return d < .99f; }) > 0);
            }
        }
        // Isolate each source family: a missing covered family must not be
        // hidden behind an opaque draw at the same pixels.
        const Scenario isolated{"isolated_family_controls", 8, 4, 32, 1, false};
        std::size_t opaque_pixels = 0;
        for (int family = 0; family < 3; ++family)
        {
            std::vector<float> previous;
            for (unsigned pose_epoch = 0; pose_epoch < 2; ++pose_epoch)
            {
                prepare(isolated, pose_epoch);
                frame(isolated, false, family); frame(isolated, true, family);
                const auto raw = pixels(false, 1), recorded = pixels(true, 1);
                REQUIRE(std::memcmp(raw.data(), recorded.data(), raw.size() * sizeof(float)) == 0);
                const auto covered = std::size_t(std::count_if(raw.begin(), raw.end(), [](float d) { return d < .99f; }));
                REQUIRE(covered > 0);
                if (family == 0) opaque_pixels = covered;
                else REQUIRE(covered < opaque_pixels);
                if (!previous.empty()) REQUIRE(raw != previous);
                previous = raw;
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

TEST_CASE("almetal: attachment palette cost fixture preserves production shadow depth",
          "[almetal][attachment-palette-cost-parity]")
{
    @autoreleasepool { Fixture fixture; fixture.preflight(); }
}

TEST_CASE("almetal: opt-in complete attachment palette and shadow CPU cost",
          "[.][almetal][attachment-palette-cost]")
{
    @autoreleasepool
    {
#ifndef NDEBUG
        SKIP("Attachment palette timing requires an optimized build");
#endif
        REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER"));
        REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
        REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED"));
        REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
        Fixture fixture;
        fixture.preflight();
        std::cout << std::setprecision(12);
        std::cout << "ATTACHMENT_PALETTE_COST source="
                  << (std::getenv("ALCHEMY_BENCHMARK_SOURCE") ? std::getenv("ALCHEMY_BENCHMARK_SOURCE") : "unspecified")
                  << " build=Release mode=headless_production_shadow_raw_vs_record_same_binary"
                     " excludes=unchanged_world_cull_pose_solve_view_matrix_calculation_PSO_creation"
                     " commit_wait_and_input_release=separate GPU_ms=not_claimed"
                     " raw_control=current_raw_path_including_new_null_record_branches\n";
        unsigned sample_id = 0;
        for (const auto& s : scenarios)
        {
            for (unsigned warm = 0; warm < 4; ++warm)
            {
                fixture.prepare(s, warm);
                fixture.frame(s, false); fixture.frame(s, true);
            }
            std::array<std::vector<double>, 2> times, prep_times;
            std::array<std::vector<double>, 2> commit_times, wait_times, release_times;
            for (unsigned block = 0; block < 10; ++block)
            {
                const std::array<bool, 4> order = block % 2
                    ? std::array<bool, 4>{{true, false, false, true}}
                    : std::array<bool, 4>{{false, true, true, false}};
                for (unsigned repetition = 0; repetition < 15; ++repetition)
                {
                    // Both variants receive the same producer bytes in each
                    // ABBA/BAAB quartet. Every record is sealed anew per frame.
                    fixture.prepare(s, block * 15 + repetition);
                    for (unsigned arm = 0; arm < order.size(); ++arm)
                    {
                        const bool record = order[arm];
                        const auto sample = fixture.frame(s, record);
                        const double cpu_total_ms = sample.cpu_ms + sample.commit_ms + sample.input_release_ms;
                        times[record].push_back(cpu_total_ms);
                        prep_times[record].push_back(sample.cpu_ms);
                        commit_times[record].push_back(sample.commit_ms);
                        wait_times[record].push_back(sample.wait_ms);
                        release_times[record].push_back(sample.input_release_ms);
                        std::cout << "ATTACHMENT_PALETTE_SAMPLE case=" << s.name
                            << " variant=" << (record ? "record" : "raw")
                            << " sample=" << sample_id++ << " block=" << block
                            << " repetition=" << repetition << " arm=" << arm
                            << " views=" << s.views << " draws_per_view=" << s.draws
                            << " producer_poses=" << s.poses << " joints=" << s.joints
                            << " sealed_records=" << (record ? s.poses : 0)
                            << " cpu_total_ms=" << cpu_total_ms
                            << " prep_encode_ms=" << sample.cpu_ms << " seal_ms=" << sample.seal_ms
                            << " commit_ms=" << sample.commit_ms << " wait_ms=" << sample.wait_ms
                            << " input_release_ms=" << sample.input_release_ms
                            << " published_bytes=" << sample.published_bytes
                            << " reused_bytes=" << sample.reused_bytes
                            << " record_publications=" << sample.record_publications
                            << " record_reuses=" << sample.record_reuses << '\n';
                    }
                }
            }
            for (unsigned arm = 0; arm < 2; ++arm)
            {
                REQUIRE(times[arm].size() == 300);
                std::cout << "ATTACHMENT_PALETTE_SUMMARY case=" << s.name
                    << " variant=" << (arm ? "record" : "raw")
                    << " frames=" << times[arm].size()
                    << " p50_cpu_total_ms=" << percentile(times[arm], .5)
                    << " p95_cpu_total_ms=" << percentile(times[arm], .95)
                    << " p50_prep_encode_ms=" << percentile(prep_times[arm], .5)
                    << " p95_prep_encode_ms=" << percentile(prep_times[arm], .95)
                    << " p50_commit_ms=" << percentile(commit_times[arm], .5)
                    << " p95_commit_ms=" << percentile(commit_times[arm], .95)
                    << " p50_wait_ms=" << percentile(wait_times[arm], .5)
                    << " p95_wait_ms=" << percentile(wait_times[arm], .95)
                    << " p50_input_release_ms=" << percentile(release_times[arm], .5)
                    << " repeated_gate=" << s.gate_repeated << '\n';
            }
        }
        std::cout << "ATTACHMENT_PALETTE_LIMIT headless_mechanism_only;not_in_world_frames_or_FPS;"
                     "R32_raster_depth_parity_only;private_D32_not_read_back;"
                     "input_release_is_not_total_native_retirement\n";
    }
}
