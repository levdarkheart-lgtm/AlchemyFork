/**
 * @file almetaluirenderer.h
 * @brief Production immediate-mode UI batches encoded by native Metal.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#pragma once

#include "almetalframe.h"
#include "almetalpipeline.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class ALMetalSurface;

enum class ALMetalUIShader
{
    textured,
    solid_color,
    alpha_mask_textured,
    alpha_mask_solid,
};

// Metal has no triangle-fan or line-loop primitive. The renderer expands
// those two topologies through a vertex-id remap in the MSL vertex shader,
// without allocating or uploading transient index buffers.
enum class ALMetalUIPrimitive
{
    points,
    lines,
    line_strip,
    line_loop,
    triangles,
    triangle_strip,
    triangle_fan,
};

// Byte-for-byte counterpart of ALMetalMatrices in almetal_builtin.metal and
// LLRender::MatricesUBOData. Keeping the complete engine block means the same
// batch can move from the transitional target to a drawable without another
// matrix ABI.
struct alignas(16) ALMetalUIMatrices
{
    float modelview[16]{};
    float projection[16]{};
    float modelview_projection[16]{};
    float inverse_projection[16]{};
    float texture0[16]{};
    float normal[3][4]{};
};

struct ALMetalUIBlendState
{
    bool enabled = true;
    ALMetalBlendFactor source_rgb = ALMetalBlendFactor::source_alpha;
    ALMetalBlendFactor destination_rgb = ALMetalBlendFactor::one_minus_source_alpha;
    ALMetalBlendOperation rgb_operation = ALMetalBlendOperation::add;
    ALMetalBlendFactor source_alpha = ALMetalBlendFactor::source_alpha;
    ALMetalBlendFactor destination_alpha = ALMetalBlendFactor::one_minus_source_alpha;
    ALMetalBlendOperation alpha_operation = ALMetalBlendOperation::add;
    ALMetalColorWriteMask write_mask = ALMetalColorWriteMask::all;
};

// Physical depth state for 3D UI/HUD batches. The caller supplies the
// compare function already translated for the active forward/reverse-Z
// convention, just as the retained depth backing stores it.
struct ALMetalUIDepthState
{
    bool enabled = false;
    bool write_enabled = false;
    bool reverse_z = false;
    ALMetalCompareFunction compare = ALMetalCompareFunction::always;
};

struct ALMetalUIBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t texcoord_offset = 0;
    std::size_t color_offset = 0;
    const ALMetalTexture* texture = nullptr;
    ALMetalSamplerDescriptor sampler{};
    ALMetalUIMatrices matrices{};
    ALMetalUIBlendState blend{};
    ALMetalUIDepthState depth{};
    ALMetalViewport viewport{};
    ALMetalScissorRect scissor{};
    ALMetalUIShader shader = ALMetalUIShader::textured;
    ALMetalUIPrimitive primitive = ALMetalUIPrimitive::triangles;
    std::uint32_t vertex_count = 0;
    std::uint32_t text_shadow_mode = 0;
    float minimum_alpha = 0.004f;
    float solid_color[4] = {1.f, 1.f, 1.f, 1.f};
    std::uint32_t target_width = 0;
    std::uint32_t target_height = 0;
};

struct ALMetalUIRendererStats
{
    std::uint64_t frames = 0;
    std::uint64_t batches = 0;
    std::uint64_t vertices = 0;
    std::uint64_t pipeline_states = 0;
    std::uint64_t sampler_states = 0;
};

class ALMetalUIRenderer final
{
public:
    ALMetalUIRenderer(const ALMetalUIRenderer&) = delete;
    ALMetalUIRenderer& operator=(const ALMetalUIRenderer&) = delete;
    ALMetalUIRenderer(ALMetalUIRenderer&&) = delete;
    ALMetalUIRenderer& operator=(ALMetalUIRenderer&&) = delete;
    ~ALMetalUIRenderer();

    static std::unique_ptr<ALMetalUIRenderer> create(
        const ALMetalDevice& device,
        ALMetalFrameScheduler& scheduler,
        std::string* error = nullptr);

    // Offscreen path with an explicit destination format. RGBA is used by
    // viewer-owned render textures such as appearance bakes, while BGRA is
    // retained by create() and the window surface path.
    static std::unique_ptr<ALMetalUIRenderer> createForTexture(
        const ALMetalDevice& device,
        ALMetalFrameScheduler& scheduler,
        ALMetalPixelFormat format,
        std::string* error = nullptr);

    // Production path: encode the same batches directly into the window's
    // CAMetalLayer drawable and schedule presentation with the frame command
    // buffer. The surface remains owned by the platform window.
    static std::unique_ptr<ALMetalUIRenderer> createForSurface(
        const ALMetalDevice& device,
        ALMetalFrameScheduler& scheduler,
        ALMetalSurface& surface,
        std::string* error = nullptr);

    // Encodes into one command buffer per viewer frame. The test path uses a
    // persistent offscreen target; the production path acquires and presents
    // one window drawable. No CPU/GPU synchronization occurs here.
    //
    // A frame background is a one-shot, same-size/same-format native backing
    // copied by the renderer before its first UI batch. It lets the eventual
    // world transaction and ordinary UI share one drawable command without a
    // GL bridge. The caller retains the texture until endFrame returns.
    bool setFrameBackground(
        const ALMetalTexture* background,
        std::string* error = nullptr);
    // A frame depth backing is a one-shot, same-size, single-sample D32F or
    // D32F/S8 texture copied on-GPU before encoding. It is currently an
    // offscreen composition primitive: the drawable path remains color-only
    // until the complete world/HUD transaction is ready for presentation.
    // The caller retains the texture until endFrame returns.
    bool setFrameDepth(
        const ALMetalTexture* depth,
        std::string* error = nullptr);
    bool draw(const ALMetalUIBatch& batch, std::string* error = nullptr);
    bool endFrame(std::string* error = nullptr);

    bool hasOpenFrame() const noexcept;
    const ALMetalTexture* outputTexture() const noexcept;
    const ALMetalTexture* outputDepthTexture() const noexcept;
    // Completes the offscreen renderer-to-viewer ownership handoff. Returns
    // null for a surface-backed renderer or while a frame is still open. The
    // next offscreen draw lazily allocates a fresh target.
    std::unique_ptr<ALMetalTexture> takeOutputTexture() noexcept;
    ALMetalUIRendererStats stats() const noexcept;

private:
    struct Impl;
    explicit ALMetalUIRenderer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};

