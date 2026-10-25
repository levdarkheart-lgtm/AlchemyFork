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

struct ALMetalPBRPreviewScene
{
    const ALMetalPBRPreviewBatch* batches = nullptr;
    std::size_t batch_count = 0;
    const ALMetalTextureDescriptor* destination_descriptor = nullptr;

    // colorCorrect applies these after the complete linear HDR scene has
    // been composited, rather than independently to each material batch.
    float exposure = 1.f;
    float tonemap_mix = 1.f;
    std::uint32_t tonemap_type = 1;
    float tonemap_params[4]{};

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
};

// A narrow owner for LLGLTFPreviewTexture's generated sphere. The GL render
// remains authoritative during migration; an incomplete material or geometry
// mirror rejects the entire native swatch rather than publishing mixed state.
class ALMetalPBRPreviewRenderer final
{
public:
    ALMetalPBRPreviewRenderer(const ALMetalPBRPreviewRenderer&) = delete;
    ALMetalPBRPreviewRenderer& operator=(const ALMetalPBRPreviewRenderer&) = delete;
    ~ALMetalPBRPreviewRenderer();

    static std::unique_ptr<ALMetalPBRPreviewRenderer> create(
        const ALMetalDevice& device,
        std::string* error = nullptr);

    std::unique_ptr<ALMetalTexture> render(
        const ALMetalPBRPreviewScene& scene,
        std::string* error = nullptr);

private:
    struct Impl;
    explicit ALMetalPBRPreviewRenderer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
