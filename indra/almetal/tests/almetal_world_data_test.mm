/**
 * @file almetal_world_data_test.mm
 * @brief Draw-payload ownership regressions and opt-in CPU publication timings.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "../almetaldevice.h"
#include "../almetalworld.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

TEST_CASE("almetal: lazy world material payloads preserve defaults and value ownership",
          "[almetal][world-data]")
{
    ALMetalWorldSimpleBatch batch;
    const auto& empty = std::as_const(batch);
    CHECK(empty.materialSlots().size() == ALMetalWorldSimpleBatch::max_material_slots);
    CHECK(empty.pbrSlots().size() == ALMetalWorldSimpleBatch::max_material_slots);
    for (const auto& slot : empty.materialSlots())
    {
        CHECK(slot.normal_texture == nullptr);
        CHECK(slot.specular_texture == nullptr);
        CHECK(slot.specular_color[0] == 1.f);
        CHECK(slot.specular_color[3] == 0.5f);
        CHECK(slot.alpha_cutoff == 0.5f);
        CHECK(slot.env_intensity == 0.f);
    }
    for (const auto& slot : empty.pbrSlots())
    {
        CHECK(slot.normal_texture == nullptr);
        CHECK(slot.orm_texture == nullptr);
        CHECK(slot.emissive_texture == nullptr);
        CHECK(slot.base_color_factor[3] == 1.f);
        CHECK(slot.roughness_factor == 1.f);
        CHECK(slot.metallic_factor == 1.f);
        CHECK(slot.alpha_cutoff == -1.f);
        for (const auto& transform : slot.texture_transform)
        {
            CHECK(transform[0] == 1.f);
            CHECK(transform[1] == 1.f);
            CHECK(transform[2] == 0.f);
        }
    }
    batch.index_count = 15;
    batch.material_slot_count = 3;
    batch.materialSlots()[2].specular_color[1] = 0.125f;
    batch.materialSlots()[2].alpha_cutoff = 0.75f;
    batch.pbr_slot_count = 2;
    batch.pbrSlots()[1].texture_transform[2][6] = 0.375f;
    batch.pbrSlots()[1].roughness_factor = 0.25f;

    ALMetalWorldSimpleBatch copied(batch);
    batch.materialSlots()[2].alpha_cutoff = 0.1f;
    batch.pbrSlots()[1].roughness_factor = 0.9f;
    CHECK(std::as_const(copied).materialSlots()[2].alpha_cutoff == 0.75f);
    CHECK(std::as_const(copied).pbrSlots()[1].roughness_factor == 0.25f);
    CHECK(copied.index_count == 15);
    CHECK(copied.material_slot_count == 3);
    CHECK(copied.pbr_slot_count == 2);

    ALMetalWorldSimpleBatch moved(std::move(copied));
    CHECK(std::as_const(moved).materialSlots()[2].specular_color[1] == 0.125f);
    CHECK(std::as_const(moved).pbrSlots()[1].texture_transform[2][6] == 0.375f);
    ALMetalWorldSimpleBatch assigned;
    assigned = moved;
    moved.pbrSlots()[1].texture_transform[2][6] = 0.6f;
    CHECK(std::as_const(assigned).pbrSlots()[1].texture_transform[2][6] == 0.375f);

    // Capture vectors relocate records while retaining the optional payloads.
    std::vector<ALMetalWorldSimpleBatch> records;
    records.push_back(std::move(assigned));
    records.reserve(64);
    CHECK(std::as_const(records.front()).materialSlots()[2].alpha_cutoff == 0.75f);
    CHECK(std::as_const(records.front()).pbrSlots()[1].roughness_factor == 0.25f);
    records.front() = ALMetalWorldSimpleBatch{};
    CHECK(records.front().index_count == 0);
    CHECK(records.front().material_slot_count == 0);
    CHECK(records.front().pbr_slot_count == 0);
    CHECK(std::as_const(records.front()).materialSlots()[2].alpha_cutoff == 0.5f);
    CHECK(std::as_const(records.front()).pbrSlots()[1].roughness_factor == 1.f);
    copied = ALMetalWorldSimpleBatch{}; // Reset a moved-from record too.
    CHECK(std::as_const(copied).pbrSlots()[1].alpha_cutoff == -1.f);
}

TEST_CASE("almetal: lazy shadow coverage payloads remain independent across copies",
          "[almetal][world-data]")
{
    ALMetalWorldSpotShadowBatch batch;
    const auto& empty = std::as_const(batch).coverageData();
    for (std::size_t index = 0; index < ALMetalWorldSpotShadowBatch::max_textures; ++index)
    {
        CHECK(empty.textures[index] == nullptr);
        CHECK(empty.alpha_cutoffs[index] == 0.f);
    }
    CHECK(empty.pbr_base_color_transforms[7][1][3] == 0.f);
    CHECK(batch.coverage == ALMetalWorldSpotShadowCoverage::opaque);
    CHECK(batch.texture_matrix[0] == 1.f);
    CHECK(batch.texture_matrix[15] == 1.f);

    batch.index_count = 12;
    batch.texture_count = 4;
    batch.coverage = ALMetalWorldSpotShadowCoverage::pbr_mask;
    batch.coverageData().alpha_cutoffs[3] = 0.625f;
    batch.coverageData().pbr_base_color_transforms[2][1][3] = 0.875f;
    ALMetalWorldSpotShadowBatch copied = batch;
    batch.coverageData().alpha_cutoffs[3] = 0.25f;
    CHECK(std::as_const(copied).coverageData().alpha_cutoffs[3] == 0.625f);
    CHECK(std::as_const(copied).coverageData().pbr_base_color_transforms[2][1][3] == 0.875f);
    CHECK(copied.index_count == 12);
    CHECK(copied.coverage == ALMetalWorldSpotShadowCoverage::pbr_mask);

    ALMetalWorldSpotShadowBatch moved = std::move(copied);
    CHECK(std::as_const(moved).coverageData().alpha_cutoffs[3] == 0.625f);
    ALMetalWorldSpotShadowBatch assigned;
    assigned = moved;
    moved.coverageData().alpha_cutoffs[3] = 0.9f;
    CHECK(std::as_const(assigned).coverageData().alpha_cutoffs[3] == 0.625f);
    std::vector<ALMetalWorldSpotShadowBatch> records;
    records.push_back(std::move(assigned));
    records.reserve(64);
    CHECK(std::as_const(records.front()).coverageData().alpha_cutoffs[3] == 0.625f);
    records.front() = ALMetalWorldSpotShadowBatch{};
    CHECK(records.front().texture_count == 0);
    CHECK(records.front().coverage == ALMetalWorldSpotShadowCoverage::opaque);
    CHECK(std::as_const(records.front()).coverageData().alpha_cutoffs[3] == 0.f);
    copied = ALMetalWorldSpotShadowBatch{};
    CHECK(std::as_const(copied).coverageData().pbr_base_color_transforms[2][1][3] == 0.f);
}

namespace
{
    struct PublicationTiming
    {
        double nanoseconds_per_operation = 0.;
        ALMetalWorldFrameDataStats before;
        ALMetalWorldFrameDataStats after;
    };

    PublicationTiming timePublications(ALMetalWorldFrameData& arena,
        const ALMetalDevice& device, std::uint64_t generation,
        std::size_t length, std::size_t publishes_per_operation,
        bool deduplicate, std::size_t operations)
    {
        std::string error;
        arena.beginFrame(generation);
        auto command = device.makeCommandBuffer("Opt-in frame-data CPU timing", &error);
        REQUIRE(command);
        std::vector<std::uint8_t> bytes(length, 0x35);
        ALMetalWorldDataSlice slice;
        PublicationTiming result;
        result.before = arena.stats();
        bool succeeded = true;
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t operation = 0; operation < operations; ++operation)
        {
            for (std::size_t publish = 0; publish < publishes_per_operation; ++publish)
            {
                if (!arena.publish(*command, bytes.data(), bytes.size(),
                                   slice, &error, deduplicate))
                {
                    succeeded = false;
                    break;
                }
            }
            if (!succeeded) break;
        }
        const auto end = std::chrono::steady_clock::now();
        INFO(error);
        REQUIRE(succeeded);
        result.after = arena.stats();
        result.nanoseconds_per_operation =
            std::chrono::duration<double, std::nano>(end - start).count() / operations;
        // No GPU commands are encoded in this CPU-only timing. Discarding the
        // wrapper marks the page's completion token abandoned for warm reuse.
        return result;
    }

    template<class Batch>
    void timeCommonRecordConstruction(const char* name)
    {
        constexpr std::size_t operations = 20000;
        constexpr std::size_t repetitions = 7;
        std::vector<Batch> records;
        records.reserve(operations);
        for (std::size_t index = 0; index < operations; ++index) records.emplace_back();
        std::array<double, repetitions> times{};
        std::uint64_t observed = 0;
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
        {
            records.clear();
            const auto start = std::chrono::steady_clock::now();
            for (std::size_t index = 0; index < operations; ++index) records.emplace_back();
            const auto end = std::chrono::steady_clock::now();
            // Observe common initialized fields after timing, without forcing
            // optional coverage/material payload allocation.
            for (const auto& batch : records)
                observed += batch.index_count + static_cast<unsigned>(batch.texture_matrix[0]);
            times[repetition] =
                std::chrono::duration<double, std::nano>(end - start).count() / operations;
        }
        CHECK(observed == operations * repetitions);
        std::sort(times.begin(), times.end());
        std::cout << "FRAME_DATA_CPU case=" << name << " sizeof_record=" << sizeof(Batch)
                  << " operations=" << operations << " repetitions=" << repetitions
                  << " median_ns=" << times[repetitions / 2]
                  << " min_ns=" << times.front() << " max_ns=" << times.back()
                  << " comparison=current_common_construction_only\n";
    }
}

TEST_CASE("almetal: opt-in frame-data publication CPU measurements",
          "[.][almetal][frame-data-benchmark]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        REQUIRE(device);
        auto arena = ALMetalWorldFrameData::create(*device, &error);
        REQUIRE(arena);
        const char* source = std::getenv("ALCHEMY_BENCHMARK_SOURCE");
        const char* build = std::getenv("ALCHEMY_BENCHMARK_BUILD");
        std::cout << "FRAME_DATA_CPU source=" << (source ? source : "unspecified")
                  << " build=" << (build ? build : "unspecified")
                  << " clock=steady units=ns_per_logical_operation"
                     " native_bindings=excluded GPU=excluded\n";
        struct Case { const char* name; std::size_t bytes; std::size_t publishes; bool deduplicate; };
        const std::array<Case, 4> cases{{
            {"opaque80_unique", 80, 1, false},
            {"PBR1296_pair_two_publications", 1296, 2, true},
            {"PBR1296_pair_one_publication", 1296, 1, true},
            {"palette5296_repeated", 5296, 1, true},
        }};
        std::uint64_t generation = 1;
        constexpr std::size_t operations = 20000;
        constexpr std::size_t repetitions = 7;
        for (const auto& scenario : cases)
        {
            // Warm allocation and the per-frame working set before timing.
            timePublications(*arena, *device, generation++, scenario.bytes,
                             scenario.publishes, scenario.deduplicate, operations);
            std::array<double, repetitions> times{};
            ALMetalWorldFrameDataStats before, after;
            for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
            {
                const auto measured = timePublications(*arena, *device,
                    generation++, scenario.bytes, scenario.publishes,
                    scenario.deduplicate, operations);
                times[repetition] = measured.nanoseconds_per_operation;
                if (repetition == 0) before = measured.before;
                after = measured.after;
            }
            std::sort(times.begin(), times.end());
            std::cout << "FRAME_DATA_CPU case=" << scenario.name
                      << " operations=" << operations << " repetitions=" << repetitions
                      << " publishes_per_operation=" << scenario.publishes
                      << " median_ns=" << times[repetitions / 2]
                      << " min_ns=" << times.front() << " max_ns=" << times.back()
                      << " published_bytes=" << after.published_bytes - before.published_bytes
                      << " reused_bytes=" << after.reused_bytes - before.reused_bytes
                      << " pages_created=" << after.pages_created - before.pages_created
                      << " pages_reused=" << after.pages_reused - before.pages_reused << '\n';
        }
        timeCommonRecordConstruction<ALMetalWorldSimpleBatch>("simple_common_constructor");
        timeCommonRecordConstruction<ALMetalWorldSpotShadowBatch>("shadow_common_constructor");
        std::cout << "FRAME_DATA_CPU limitation=production_arena_publication_only;"
                     "two_stage_cases_compare_logical_publication_counts;"
                     "not_viewer_FPS_or_GPU_frame_time\n";
    }
}
