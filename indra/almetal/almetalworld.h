/**
 * @file almetalworld.h
 * @brief Native Metal world G-buffer bootstrap pass.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#pragma once

#include "almetalpipeline.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

struct ALMetalWorldMaterialSlot
{
    const ALMetalTexture* normal_texture = nullptr;
    const ALMetalTexture* specular_texture = nullptr;
    float specular_color[4] = {1.f, 1.f, 1.f, 0.5f};
    float env_intensity = 0.f;
    float alpha_cutoff = 0.5f;
    float fullbright = 0.f;
};

struct ALMetalWorldPBRSlot
{
    const ALMetalTexture* normal_texture = nullptr;
    const ALMetalTexture* orm_texture = nullptr;
    const ALMetalTexture* emissive_texture = nullptr;
    float texture_transform[4][8] = {
        {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
        {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
    };
    float emissive_color[4]{};
    float roughness_factor = 1.f;
    float metallic_factor = 1.f;
    float alpha_cutoff = -1.f;
};

struct ALMetalWorldSimpleBatch
{
    static constexpr std::size_t max_diffuse_textures = 32;
    static constexpr std::size_t max_material_slots = 8;

    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t normal_offset = 0;
    std::size_t texcoord_offset = 0;
    std::size_t texcoord1_offset = 0;
    std::size_t texcoord2_offset = 0;
    std::size_t color_offset = 0;
    std::size_t tangent_offset = 0;
    std::size_t weight_offset = 0;
    std::size_t cloth_offset = 0;
    std::uint32_t vertex_count = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;
    std::array<const ALMetalTexture*, max_diffuse_textures> diffuse_textures{};
    std::uint32_t texture_count = 0;
    const ALMetalTexture* normal_texture = nullptr;
    std::array<ALMetalWorldMaterialSlot, max_material_slots> material_slots{};
    std::uint32_t material_slot_count = 0;
    std::uint32_t material_alpha_mode = 0;
    std::array<ALMetalWorldPBRSlot, max_material_slots> pbr_slots{};
    std::uint32_t pbr_slot_count = 0;
    const float* skin_palette = nullptr;
    std::size_t skin_palette_float_count = 0;
    std::uint32_t skin_joint_count = 0;
    bool alpha_mask = false;
    bool double_sided = false;
    bool avatar_rigid = false;
    bool avatar_skinning = false;
    bool avatar_cloth = false;
    bool terrain = false;
    bool use_vertex_color = true;
    bool alpha_cutoff_bias = true;
    float alpha_cutoff = 0.f;
    float modelview_projection[16]{};
    float normal_matrix[3][4]{};
    float rigged_normal_matrix[3][4]{};
    float texture_matrix[16] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
    };
    float avatar_wind[4]{};
    float avatar_sin_wave[4]{};
    float avatar_gravity[4]{};
    float avatar_cloth_depth[4]{};
    const ALMetalTexture* terrain_height_texture = nullptr;
    const ALMetalTexture* terrain_composition_texture = nullptr;
    const ALMetalTexture* terrain_alpha_ramp_texture = nullptr;
    const ALMetalTexture* terrain_parcel_overlay_texture = nullptr;
    std::uint16_t terrain_tessellation_factors[6]{};
    float terrain_grid_scale = 1.f;
    float terrain_region_scale = 256.f;
    float terrain_detail_scale = 1.f / 12.f;
    float terrain_detail_phase[2]{};
    float terrain_triplanar_blend = 8.f;
    bool terrain_smoothing = false;
    bool terrain_triplanar = true;
    bool terrain_tessellation_pinned = false;
};

struct ALMetalWorldGBufferScene
{
    const ALMetalWorldSimpleBatch* batches = nullptr;
    std::size_t batch_count = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool normal_hdr = false;
    bool hdr = false;
    bool reverse_z = false;
    float clear_color[4] = {1.f, 0.f, 1.f, 1.f};
};

// The ordered native world G-buffer owner for supported object, material and
// system-avatar families.
// It writes four colour attachments plus depth and is not presented yet. The
// viewer runs one bounded fail-closed probe so world integration can advance
// without doubling these passes every migration frame.
class ALMetalWorldGBufferRenderer final
{
public:
    ALMetalWorldGBufferRenderer(const ALMetalWorldGBufferRenderer&) = delete;
    ALMetalWorldGBufferRenderer& operator=(const ALMetalWorldGBufferRenderer&) = delete;
    ~ALMetalWorldGBufferRenderer();

    static std::unique_ptr<ALMetalWorldGBufferRenderer> create(
        const ALMetalDevice& device,
        std::string* error = nullptr);

    std::unique_ptr<ALMetalRenderTarget> render(
        const ALMetalWorldGBufferScene& scene,
        std::string* error = nullptr);

private:
    struct Impl;
    explicit ALMetalWorldGBufferRenderer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
