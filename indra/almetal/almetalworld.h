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

struct ALMetalWorldSimpleBatch
{
    static constexpr std::size_t max_diffuse_textures = 32;

    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t normal_offset = 0;
    std::size_t texcoord_offset = 0;
    std::size_t color_offset = 0;
    std::size_t weight_offset = 0;
    std::uint32_t vertex_count = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;
    std::array<const ALMetalTexture*, max_diffuse_textures> diffuse_textures{};
    std::uint32_t texture_count = 0;
    const float* skin_palette = nullptr;
    std::size_t skin_palette_float_count = 0;
    std::uint32_t skin_joint_count = 0;
    bool alpha_mask = false;
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

// The deliberately narrow world bootstrap owner: simple and alpha-mask
// geometry into a native four-attachment G-buffer plus depth. It is not
// presented yet. The viewer runs one bounded fail-closed probe so world
// integration can advance without doubling these passes every migration frame.
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
