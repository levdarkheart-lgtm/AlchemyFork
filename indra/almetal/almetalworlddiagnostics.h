/**
 * @file almetalworlddiagnostics.h
 * @brief Opt-in census of the encoded post-deferred scene, not GPU timings.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

struct ALMetalWorldOpaquePostDeferredScene;

// Cumulative arena-backed common-constant bindings in accepted G-buffer
// renders. Reuses stay within one render/command; these are not GPU timings.
struct ALMetalWorldConstantStats
{
    std::uint64_t bindings = 0;
    std::uint64_t reused_bindings = 0;
};

enum class ALMetalWorldPostFamily : std::size_t
{
    fullbright, shiny, emboss, alpha_simple, alpha_fullbright,
    alpha_material, alpha_pbr, alpha_avatar, water_exclusion,
    water_haze, atmospheric_haze, water_surface, glow, count
};

// Encoded indexed draw commands, not unique geometry or GPU invocations.
struct ALMetalWorldEncodedDrawCensus
{
    std::uint64_t batches = 0, vertices = 0, indices = 0;
};

struct ALMetalWorldSimpleShadowPSOCensus
{
    // Per selected descriptor: ordinary/avatar x standard/custom RGB blend x
    // optional constant 0 undefined/false/true. Avatar is identified from the
    // actual vertex function, not a duplicated pipeline selector predicate.
    static constexpr std::size_t bucket_count = 12;
    static constexpr std::array<const char*, bucket_count> bucket_names{
        "ordinary.standard.undefined", "ordinary.standard.false", "ordinary.standard.true",
        "ordinary.custom.undefined", "ordinary.custom.false", "ordinary.custom.true",
        "avatar.standard.undefined", "avatar.standard.false", "avatar.standard.true",
        "avatar.custom.undefined", "avatar.custom.false", "avatar.custom.true"};
    bool valid = false, shadow_pack_present = false;
    std::uint64_t source_frame_index = 0, sample_sequence = 0;
    std::uint32_t source_view_kind = 0, source_view_index = 0;
    std::uint32_t probe_count = 0, shadow_flags = 0;
    std::array<ALMetalWorldEncodedDrawCensus, bucket_count> draws{};
};

struct ALMetalWorldPostDiagnostics
{
    static constexpr std::size_t family_count =
        static_cast<std::size_t>(ALMetalWorldPostFamily::count);
    static constexpr std::size_t segment_count = 4;
    static constexpr std::array<const char*, family_count> family_names{
        "fullbright", "shiny", "emboss", "alpha_simple", "alpha_fullbright",
        "alpha_material", "alpha_pbr", "alpha_avatar", "water_exclusion",
        "water_haze", "atmospheric_haze", "water_surface", "glow"};
    bool valid = false;
    std::uint64_t sample_sequence = 0;
    std::uint64_t encoded_calls = 0;
    std::uint64_t source_frame_index = 0;
    std::uint32_t source_view_kind = 0, source_view_index = 0;
    std::uint32_t width = 0, height = 0, flags = 0, probe_count = 0;
    std::uint64_t batch_count = 0, initial_batch_count = 0;
    std::uint64_t auxiliary_items = 0;
    // Last successful optional encoder diagnostic, independent of the sparse census.
    // Zero encoders means unsplit. Nonzero counts actual initial family encoders.
    std::uint64_t diagnostic_initial_family_encoders = 0;
    std::uint64_t diagnostic_initial_family_frame = 0;
    std::uint32_t diagnostic_initial_family_view_kind = 0;
    std::uint32_t diagnostic_initial_family_view_index = 0;
    // Latest successfully encoded call per main(1), hero(6), ordinary(6)
    // view. Each entry has its own tuple; these are not synchronized frames.
    std::array<ALMetalWorldSimpleShadowPSOCensus, 13> simple_shadow_psos{};
    std::array<std::uint64_t, family_count> batches{}, indices{};
    std::array<std::uint64_t, family_count> initial_batches{};
    // Initial, water-haze, atmospheric-haze, water-surface encoder segments.
    // Later draws keep using the most recently opened encoder.
    std::array<std::array<std::uint64_t, family_count>, segment_count> segment_batches{};
    std::uint32_t tail_segment = 0;
    std::uint64_t ordered_geometry_hash = 0;
    std::uint64_t unordered_geometry_hash = 0;
    // Descriptor dimensions/formats/mips, not asset contents or asset IDs.
    std::uint64_t texture_descriptor_hash = 0;
    // Native object identities: useful only within one process.
    std::uint64_t process_resource_hash = 0;
};

// Describe already-admitted CPU data without changing it or querying the GPU.
// Equality of these noncryptographic signatures is diagnostic, not parity proof.
ALMetalWorldPostDiagnostics alMetalDescribeOpaquePost(
    const ALMetalWorldOpaquePostDeferredScene& scene,
    std::uint64_t sequence) noexcept;
