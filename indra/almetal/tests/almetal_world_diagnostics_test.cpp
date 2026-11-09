/**
 * @file almetal_world_diagnostics_test.cpp
 * @brief Diagnostic pass-segment and signature interpretation boundaries.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalworld.h"
#include "../almetalworlddiagnostics.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <utility>

TEST_CASE("almetal: post census distinguishes initial encoder from later haze and alpha", "[almetal][world-diagnostics]")
{
    using F = ALMetalWorldPostFamily;
    const auto at = [](F kind) { return static_cast<std::size_t>(kind); };
    std::array<ALMetalWorldSimpleBatch, 5> batches;
    batches[0].index_count = 6;
    batches[1].pre_water_alpha = batches[1].pre_water_pbr = true;
    batches[1].index_count = 12;
    batches[2].atmospheric_haze = true;
    batches[3].pre_water_alpha = batches[3].pre_water_material = true;
    batches[3].index_count = 18;
    batches[4].impostor_shiny = true;
    ALMetalWorldOpaquePostDeferredScene scene;
    scene.batches = batches.data();
    scene.batch_count = batches.size();
    scene.width = 3456;
    scene.height = 2168;
    scene.hdr = scene.reverse_z = true;
    scene.diagnostic_frame_index = 500;
    scene.diagnostic_view_kind = 2;
    scene.diagnostic_view_index = 3;
    const auto first = alMetalDescribeOpaquePost(scene, 120);
    CHECK(first.valid);
    CHECK(first.sample_sequence == 120);
    CHECK(first.source_frame_index == 500);
    CHECK(first.source_view_kind == 2);
    CHECK(first.source_view_index == 3);
    CHECK(first.flags == 3);
    CHECK(first.initial_batch_count == 2);
    CHECK(first.initial_batches[at(F::alpha_pbr)] == 1);
    CHECK(first.initial_batches[at(F::shiny)] == 0);
    CHECK(first.batches[at(F::alpha_material)] == 1);
    CHECK(first.indices[at(F::alpha_material)] == 18);
    CHECK(first.batches[at(F::atmospheric_haze)] == 1);
    CHECK(first.segment_batches[0][at(F::alpha_pbr)] == 1);
    CHECK(first.segment_batches[2][at(F::alpha_material)] == 1);
    CHECK(first.segment_batches[2][at(F::shiny)] == 1);
    CHECK(first.tail_segment == 2);
    CHECK(first.auxiliary_items == 0);
    scene.composition_debug_height_count = 3;
    CHECK(alMetalDescribeOpaquePost(scene, 120).auxiliary_items == 3);
    scene.composition_debug_height_count = 0;
    const auto identical = alMetalDescribeOpaquePost(scene, 121);
    CHECK(identical.ordered_geometry_hash == first.ordered_geometry_hash);
    CHECK(identical.unordered_geometry_hash == first.unordered_geometry_hash);
    std::swap(batches[0], batches[1]);
    const auto reordered = alMetalDescribeOpaquePost(scene, 122);
    CHECK(reordered.ordered_geometry_hash != first.ordered_geometry_hash);
    CHECK(reordered.unordered_geometry_hash == first.unordered_geometry_hash);
    batches[0].post_water_depth_only = true;
    CHECK(alMetalDescribeOpaquePost(scene, 122).unordered_geometry_hash !=
          first.unordered_geometry_hash);
    batches[0].post_water_depth_only = false;
    batches[0].modelview_projection[12] += 1.f;
    const auto moved = alMetalDescribeOpaquePost(scene, 123);
    CHECK(moved.unordered_geometry_hash != first.unordered_geometry_hash);
    std::swap(batches[0], batches[2]);
    const auto haze_first = alMetalDescribeOpaquePost(scene, 124);
    CHECK(haze_first.initial_batch_count == 0);
}
