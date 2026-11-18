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

// Why an eligible forward consumer retained the ordinary shader, or a shared
// mask publication declined. These describe CPU decisions, never GPU cost.
enum class ALMetalProbeCandidateReason : std::size_t
{
    none, not_eligible, projection_unavailable, probe_environment_unavailable,
    sky_only_environment, frame_data_unavailable, pipeline_unavailable,
    unsupported_inputs, invalid_bounds, all_full, byte_budget,
    completion_token, library_capability, admission_denied, inputs_changed,
    residency, allocation, retention, exception, native_failure, count
};
inline constexpr std::size_t alMetalProbeCandidateReasonCount =
    static_cast<std::size_t>(ALMetalProbeCandidateReason::count);
inline constexpr std::array<const char*, alMetalProbeCandidateReasonCount>
    alMetalProbeCandidateReasonNames{
        "none", "not_eligible", "projection_unavailable", "probe_environment_unavailable",
        "sky_only_environment", "frame_data_unavailable", "pipeline_unavailable",
        "unsupported_inputs", "invalid_bounds", "all_full", "byte_budget",
        "completion_token", "library_capability", "admission_denied", "inputs_changed",
        "residency", "allocation", "retention", "exception", "native_failure"};

struct ALMetalWorldProbeCandidateDecision
{
    bool valid = false, packet_usable = false, masks_bound = false;
    ALMetalProbeCandidateReason reason = ALMetalProbeCandidateReason::not_eligible;
    std::uint64_t source_frame_index = 0;
    std::uint32_t source_view_kind = 0, source_view_index = 0, probe_count = 0;
    // Nondefault bounds only; empty is a subset of bounded. Full bounds may
    // be conservative or genuinely cover the view, not a correctness defect.
    std::uint32_t full_rects = 0, bounded_rects = 0, empty_rects = 0;
    std::uint64_t eligible_batches = 0, prepared_batches = 0;
    std::uint64_t pipeline_fallback_batches = 0;
};

// Cumulative arena-backed common-constant bindings in accepted G-buffer
// renders. Reuses stay within one render/command; these are not GPU timings.
struct ALMetalWorldConstantStats
{
    std::uint64_t bindings = 0;
    std::uint64_t reused_bindings = 0;
    // Accepted legacy/PBR G-buffer draw commands using sealed native tables
    // versus dynamic packing (including absent or mismatched source records).
    // Terrain/other families are excluded; these are not GPU invocation counts.
    std::uint64_t material_source_draws = 0;
    std::uint64_t material_source_fallback_draws = 0;
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
    // Successful CPU-encoding observation, independent of legacy command
    // timing. Async telemetry samples at most once/second per renderer/view
    // kind; it retains the actual face. Counts are not GPU completion/coverage.
    std::uint64_t source_frame_index = 0, sample_sequence = 0;
    std::uint32_t source_view_kind = 0, source_view_index = 0;
    std::uint32_t probe_count = 0, shadow_flags = 0;
    std::array<ALMetalWorldEncodedDrawCensus, bucket_count> draws{};
    // Actual successful descriptors with bool3/bool4 true, per existing
    // bucket. These count encoded batches, not requested specializations.
    std::array<std::uint64_t, bucket_count> high_specialized_batches{};
    std::array<std::uint64_t, bucket_count> diffuse_pair_specialized_batches{};
    std::uint64_t unshadowed_simple_batches = 0;
    std::uint64_t unshadowed_diffuse_pair_batches = 0;
};

// Latest successful color-writing PBR-alpha draws per view. Descriptor
// selection is observed after encoding; counts are not GPU invocations.
struct ALMetalWorldPBRShadowPSOCensus
{
    static constexpr std::size_t bucket_count = 12;
    // Actual vertex function x RGB blend x optional bool0 undefined/false/true.
    static constexpr std::array<const char*, bucket_count> bucket_names{
        "rigid.standard.undefined", "rigid.standard.false", "rigid.standard.true",
        "rigid.custom.undefined", "rigid.custom.false", "rigid.custom.true",
        "skinned.standard.undefined", "skinned.standard.false", "skinned.standard.true",
        "skinned.custom.undefined", "skinned.custom.false", "skinned.custom.true"};
    bool valid = false, shadow_pack_present = false;
    std::uint64_t source_frame_index = 0, sample_sequence = 0;
    std::uint32_t source_view_kind = 0, source_view_index = 0;
    // Source scene pack metadata; an unshadowed draw can bind its fallback.
    std::uint32_t probe_count = 0, shadow_flags = 0;
    std::array<ALMetalWorldEncodedDrawCensus, bucket_count> draws{};
};

// Latest successful opt-in water PSO observation, not GPU invocations.
struct ALMetalWorldWaterShadowPSOCensus
{
    bool valid = false, mirror_clip = false, underwater = false;
    bool uses_shadows = false;
    std::uint64_t source_frame_index = 0, sample_sequence = 0;
    std::uint32_t source_view_kind = 0, source_view_index = 0;
    std::uint32_t probe_count = 0, shadow_flags = 0;
    // Actual selected descriptor: flat/wave x optional bool0 undefined/false/true.
    std::array<std::uint64_t, 6> draws{};
};

// Successful CPU encodings over this renderer's lifetime (including reloads).
// A borrowed outer command batch can still fail later; these are not GPU completions.
struct ALMetalWorldProbeCandidateActivity
{
    std::uint64_t encoded_calls = 0;
    // Disjoint pre-water/post-water owners; only simple-alpha draws (standard or custom blend).
    std::array<std::uint64_t, 2> eligible_batches{}, prepared_batches{};
    // The same disjoint pre/post owners for the material and PBR consumers.
    // Existing eligible/prepared_batches above retain their simple-only meaning.
    std::array<std::uint64_t, 2> material_eligible_batches{}, material_prepared_batches{};
    std::array<std::uint64_t, 2> pbr_eligible_batches{}, pbr_prepared_batches{};
    // Avatar/body alpha is a separate ordered owner, not a pre/post-water pool.
    // Depth-only/color-disabled replays do not need probe candidates.
    std::uint64_t avatar_eligible_batches = 0, avatar_prepared_batches = 0;
    std::uint64_t shiny_eligible_batches = 0, shiny_prepared_batches = 0;
    // Separate compositor calls, published only after successful commit.
    // Array owners: shiny0, standalone legacy-material-alpha1. Not completion.
    std::uint64_t impostor_encoded_calls = 0;
    std::array<std::uint64_t, 2> impostor_eligible_batches{}, impostor_prepared_batches{};
    // Eligible calls without a usable shared mask, grouped by their final
    // preparation decision; separate from per-draw missing-variant fallback.
    std::array<std::uint64_t, alMetalProbeCandidateReasonCount> unavailable_calls{};
    std::uint64_t pipeline_fallback_batches = 0;
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
    // Last successful render: disjoint unshadowed/shadowed draws using the
    // prepared simple-alpha entry points, independent of timing. Not GPU work.
    std::uint64_t probe_candidate_simple_batches = 0;
    std::uint64_t probe_candidate_simple_shadow_batches = 0;
    std::uint64_t probe_candidate_material_batches = 0;
    std::uint64_t probe_candidate_pbr_batches = 0;
    std::uint64_t probe_candidate_avatar_batches = 0;
    std::uint64_t probe_candidate_shiny_batches = 0;
    std::uint64_t probe_candidate_frame_index = 0;
    std::uint32_t probe_candidate_view_kind = 0, probe_candidate_view_index = 0;
    ALMetalWorldProbeCandidateActivity probe_candidate_activity;
    ALMetalWorldProbeCandidateDecision probe_candidate_decision;
    std::uint64_t diagnostic_initial_family_frame = 0;
    std::uint32_t diagnostic_initial_family_view_kind = 0;
    std::uint32_t diagnostic_initial_family_view_index = 0;
    // Latest successfully encoded call per main(1), hero(6), ordinary(6)
    // view. Each entry has its own tuple; these are not synchronized frames.
    std::array<ALMetalWorldSimpleShadowPSOCensus, 13> simple_shadow_psos{};
    std::array<ALMetalWorldPBRShadowPSOCensus, 13> pbr_shadow_psos{};
    ALMetalWorldWaterShadowPSOCensus water_shadow_pso;
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
