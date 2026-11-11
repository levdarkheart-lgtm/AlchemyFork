/**
 * @file almetalpbrpreview.h
 * @brief Native Metal glTF material swatch renderer.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#pragma once

#include "almetalpipeline.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

struct ALMetalPBRPreviewBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t normal_offset = 0;
    std::size_t texcoord_offset = 0;
    std::size_t color_offset = 0;
    std::size_t tangent_offset = 0;
    std::uint32_t vertex_count = 0;

    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;

    const ALMetalTexture* base_color_texture = nullptr;
    const ALMetalTexture* normal_texture = nullptr;
    const ALMetalTexture* orm_texture = nullptr;
    const ALMetalTexture* emissive_texture = nullptr;

    float modelview_projection[16]{};
    float modelview[16]{};
    float normal_matrix[3][4]{};
    float texture_transform[4][8] = {
        {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    };
    float emissive_color[4]{};
    float sun_direction[4] = {0.f, 0.f, 1.f, 0.f};
    float sun_color[4] = {1.f, 1.f, 1.f, 0.f};
    float metallic_factor = 1.f;
    float roughness_factor = 1.f;
    float alpha_cutoff = 0.004f;
    bool double_sided = false;
};

// One coherent generation of the GL owner's default reflection probe,
// mirrored without stalling either API. The radiance cube and packed SH row
// are published together; callers must not substitute one from a different
// generation.
struct ALMetalPBRPreviewEnvironment
{
    const ALMetalTexture* radiance_texture = nullptr;
    std::uint32_t sh_coefficients[4][4]{};
    float probe_origin[4]{};
    float env_matrix[3][4] = {
        {1.f, 0.f, 0.f, 0.f},
        {0.f, 1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f, 0.f},
    };
    // atmos_amblit.rgb and sky_ambient_scale, before the fragment's
    // normal-dependent ambientLighting and sRGB-to-linear conversion.
    float ambient[4]{};
    float irradiance_scale = 1.f;
    float radiance_scale = 1.f;
    float max_lod = 0.f;
};

struct ALMetalPBRPreviewScene
{
    const ALMetalPBRPreviewBatch* batches = nullptr;
    std::size_t batch_count = 0;
    const ALMetalTextureDescriptor* destination_descriptor = nullptr;
    const ALMetalPBRPreviewEnvironment* environment = nullptr;

    // The retained preview selects the CLASSIC_MODE PBR-alpha variant from
    // the active sky. Keep that lighting contract explicit here: it changes
    // atmospheric color space, probe diffuse participation and the direct
    // light recombination, not just the later color-correction pass.
    bool classic_mode = false;

    // colorCorrect applies these after the complete linear scene has been
    // composited, rather than independently to each material batch. HDR
    // previews apply exposure/tonemapping unless their clean-plate/legacy
    // configuration explicitly disables it. Non-HDR previews encode the
    // same scene without exposure or tone mapping and may dither the 8-bit
    // destination.
    bool tonemap_enabled = true;
    float exposure = 1.f;
    float tonemap_mix = 1.f;
    std::uint32_t tonemap_type = 1;
    float tonemap_params[4]{};
    bool legacy_gamma = false;
    float legacy_gamma_exponent = 1.f;
    float dither_amount = 0.f;
    bool dither_animated = false;
    std::uint32_t frame_id = 0;

    // The material preview deliberately requests an unadapted exposure
    // (generateExposure(..., use_history=false)). Fractions are normalized
    // percentile positions in the generated histogram.
    bool meter_exposure = false;
    float exposure_compensation = 0.f;
    float exposure_min_ev = -3.f;
    float exposure_max_ev = 4.f;
    float exposure_low_fraction = 0.1f;
    float exposure_high_fraction = 0.95f;
    std::uint32_t exposure_metering_mode = 1;

    // HDR bloom and its optional cross-filter streaks are generated from the
    // same pre-exposure linear target as GL.
    bool bloom_enabled = false;
    std::uint32_t bloom_mip_count = 5;
    std::uint32_t bloom_resolution_scale = 0;
    float bloom_threshold = 1.f;
    float bloom_knee = 0.5f;
    float bloom_firefly_clamp = 24.f;
    float bloom_scatter = 0.7f;
    float bloom_alpha_glow_boost = 2.f;
    float bloom_strength = 0.325f;
    bool bloom_halation = false;
    float bloom_halation_strength = 0.f;
    float bloom_halation_tint[3] = {1.f, 0.35f, 0.15f};
    float bloom_halation_weights[3] = {1.f, 1.f, 1.f};
    bool cross_filter_enabled = false;
    float cross_filter_strength = 0.f;
    std::uint32_t cross_filter_points = 4;
    float cross_filter_angle = 0.f;
    float cross_filter_length = 1.f;
    float cross_filter_falloff = 1.5f;
    float cross_filter_chromatic = 0.f;

    // colorCorrect's lateral chromatic aberration is applied to the complete
    // linear HDR scene before bloom, exposure and tone mapping. These are the
    // authored controls; the renderer bakes the squared strength, reciprocal
    // falloff and angle sin/cos exactly as pipeline.cpp does.
    float chromatic_aberration_strength = 0.f;
    float chromatic_aberration_falloff = 1.f;
    float chromatic_aberration_angle = 0.f;
    float chromatic_aberration_offset_r[2] = {-1.f, 0.f};
    float chromatic_aberration_offset_b[2] = {1.f, 0.f};
    float chromatic_aberration_anisotropy = 0.f;
};

// A narrow owner for LLGLTFPreviewTexture's generated sphere. A complete native
// result owns post processing and publication; configurations or inputs not yet
// represented natively retain their established preview path during migration.
// Incomplete native state is never mixed into a retained result.
class ALMetalPBRPreviewRenderer final
{
public:
    ALMetalPBRPreviewRenderer(const ALMetalPBRPreviewRenderer&) = delete;
    ALMetalPBRPreviewRenderer& operator=(const ALMetalPBRPreviewRenderer&) = delete;
    ~ALMetalPBRPreviewRenderer();

    static std::unique_ptr<ALMetalPBRPreviewRenderer> create(
        const ALMetalDevice& device,
        std::string* error = nullptr);

    // Revisions actually loaded/generated by this owner. A purge is consumed
    // at the next render; the internal shader-derived LUT is then regenerated
    // on that render's command before shading. Zero means no LUT submitted.
    std::uint64_t shaderGeneration() const noexcept;
    std::uint64_t brdfLutGeneration() const noexcept;

    std::unique_ptr<ALMetalTexture> render(
        const ALMetalPBRPreviewScene& scene,
        std::string* error = nullptr);

private:
    struct Impl;
    explicit ALMetalPBRPreviewRenderer(std::unique_ptr<Impl> impl);
    bool refreshShaders(std::string* error);
    std::unique_ptr<Impl> mImpl;
};
