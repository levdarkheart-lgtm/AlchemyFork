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

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class ALMetalSurface;

enum class ALMetalUIShader
{
    textured,
    vertex_color,
    uniform_color,
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
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;
    const ALMetalTexture* texture = nullptr;
    // Sample a view-compatible non-sRGB view so legacy callers that selected
    // GL_SKIP_DECODE_EXT keep filtering and multiplying encoded storage bytes.
    bool sample_linear_storage = false;
    ALMetalSamplerDescriptor sampler{};
    ALMetalUIMatrices matrices{};
    ALMetalUIBlendState blend{};
    ALMetalUIDepthState depth{};
    ALMetalCullMode cull_mode = ALMetalCullMode::none;
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

enum class ALMetalHUDShader
{
    fullbright,
    fullbright_alpha_mask,
    fullbright_alpha,
    pbr_alpha,
    highlight,
    highlight_skinned,
    debug,
    debug_skinned,
};

// One indexed static HUD-attachment draw. Unlike immediate-mode UI, volume
// geometry owns an index buffer and may select one of the viewer's packed
// diffuse texture ladder slots through position.w. Legacy fullbright HUD
// programs operate in display-encoded space. The PBR alpha permutation
// instead decodes its glTF color maps, shades in linear light and encodes the
// result for this display target, matching the two authoritative GLSL paths.
struct ALMetalHUDIndexedBatch
{
    static constexpr std::size_t max_textures = 32;

    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t texcoord_offset = 0;
    std::size_t color_offset = 0;
    std::size_t weight_offset = 0;
    std::uint32_t vertex_count = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;
    std::array<const ALMetalTexture*, max_textures> textures{};
    std::uint32_t texture_count = 0;
    const ALMetalTexture* emissive_texture = nullptr;
    const float* skin_palette = nullptr;
    std::size_t skin_palette_float_count = 0;
    std::uint32_t skin_joint_count = 0;
    float highlight_color[4] = {1.f, 1.f, 1.f, 1.f};
    float pbr_base_color_transform[8] = {
        1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    float pbr_emissive_transform[8] = {
        1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    float pbr_emissive_color[4]{};
    ALMetalSamplerDescriptor sampler{};
    ALMetalUIMatrices matrices{};
    ALMetalUIBlendState blend{};
    ALMetalUIDepthState depth{};
    ALMetalViewport viewport{};
    ALMetalScissorRect scissor{};
    ALMetalHUDShader shader = ALMetalHUDShader::fullbright;
    float minimum_alpha = 0.004f;
    // Semantic GL polygon-offset terms. The encoder translates these to
    // Metal's depth-bias sign for the active forward/reverse-Z convention.
    float depth_bias_slope = 0.f;
    float depth_bias_units = 0.f;
    bool double_sided = false;
    std::uint32_t target_width = 0;
    std::uint32_t target_height = 0;
};

// One face range for the selection owner's private RGBA8 id/depth target.
// The color contract is byte exact with selectionIdF.glsl: RG is the low/high
// id, B is visible/hidden/not-drawn coverage and A is priority. The target is
// deliberately not composited by this stage; the optimized contour/glow
// passes consume it later in the same ordered world/UI transaction.
struct ALMetalSelectionIDBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t texcoord_offset = 0;
    std::size_t weight_offset = 0;
    std::uint32_t vertex_count = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;
    const ALMetalTexture* texture = nullptr;
    ALMetalSamplerDescriptor sampler{};
    const float* skin_palette = nullptr;
    std::size_t skin_palette_float_count = 0;
    std::uint32_t skin_joint_count = 0;
    float modelview_projection[16]{};
    float texture_matrix[16] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
    };
    float base_color_transform[8] = {
        1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    float alpha_cutoff = -1.f;
    std::uint32_t id = 0;
    std::uint32_t priority = 0;
    bool show_hidden = false;
    bool outlined = true;
    bool keep_cut = false;
    bool double_sided = false;
};

struct ALMetalSelectionIDScene
{
    const ALMetalSelectionIDBatch* batches = nullptr;
    std::size_t batch_count = 0;
    float inverse_projection[16]{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool reverse_z = false;
    // World selection compares against the immutable retained scene depth.
    // HUD selection has no readable window depth and marks every face visible.
    bool use_scene_depth = true;
};

// Inputs for the optimized screen-space contour consumer of the private
// selection id/depth target. The palette is byte-identical to the legacy
// RGBA8 texture: visible and hidden rows alternate for each 256 ids. The
// optional wireframe, hover glow and contrast-shift backdrop use the same
// private id/depth data.
struct ALMetalSelectionContourScene
{
    const std::uint8_t* palette_rgba8 = nullptr;
    std::size_t palette_bytes = 0;
    std::uint32_t palette_width = 0;
    std::uint32_t palette_height = 0;
    float inverse_projection[16]{};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t outline_width = 0;
    std::uint32_t inner_width = 0;
    std::uint32_t rim_width = 0;
    // A glow radius of zero disables hover glow. Contour can be disabled for
    // a hover-only frame while retaining the shared edge transaction.
    std::uint32_t glow_radius = 0;
    float glow_brightness = 0.f;
    // Wire batches reuse the exact selection face contract above, but only
    // id/show_hidden and the geometry, alpha, skinning and culling fields are
    // consumed. The renderer expands every indexed triangle to a clipped
    // near-plane-safe barycentric triangle pair on the GPU.
    const ALMetalSelectionIDBatch* wire_batches = nullptr;
    std::size_t wire_batch_count = 0;
    std::uint32_t wire_width = 0;
    float wire_depth_slope = 0.f;
    float wire_depth_units = 0.f;
    bool wire_hud = false;
    bool contour_enabled = true;
    bool contrast_shift = false;
    bool reverse_z = false;
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
        std::string* error = nullptr,
        bool linear_storage_view = false);

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
    // Localized HUD depth brackets operate on the copied destination depth,
    // while restore samples the immutable retained backing selected above.
    // Restore uses the caller's physical depth comparison so semantic LESS
    // remains conditional for either conventional or reverse-Z depth.
    // Rectangles use Metal's upper-left target coordinates and must be
    // non-empty and fully contained by the retained depth dimensions.
    bool clearFrameDepthRects(
        const ALMetalScissorRect* rects,
        std::size_t count,
        float depth,
        std::string* error = nullptr);
    bool restoreFrameDepthRects(
        const ALMetalScissorRect* rects,
        std::size_t count,
        ALMetalCompareFunction compare,
        std::string* error = nullptr);
    // Snapshot one output pixel at the current ordered draw boundary, then
    // resume the same frame. Coordinates address the retained offscreen
    // texture directly; row zero is the GL-compatible lower row. The returned
    // private texture is populated by the still-open frame command and must
    // not be consumed until that frame has been submitted.
    std::unique_ptr<ALMetalTexture> copyFrameColorPixel(
        std::uint32_t x,
        std::uint32_t y,
        std::string* error = nullptr);
    // Exceptional synchronous consumer for appearance morph-mask and bake
    // generation. Finish and submit every draw ordered before this boundary,
    // read the selected RGBA8 or RGBA8-sRGB storage bytes with row zero in the
    // retained GL-row convention, then retain that complete output as the next
    // frame's background so later draws resume rather than clear it. This
    // deliberately splits the transaction and waits; recurring diagnostics
    // use the asynchronous readback ring instead.
    bool readFrameColorRGBA8(
        ALMetalRegion2D region,
        void* bytes,
        std::size_t bytes_per_row,
        std::string* error = nullptr);
    bool draw(const ALMetalUIBatch& batch, std::string* error = nullptr);
    bool drawHUDIndexed(
        const ALMetalHUDIndexedBatch& batch,
        std::string* error = nullptr);
    // Encodes the private selection face-id/depth pass between already ordered
    // UI/HUD draws, then resumes the main color/depth target with load actions.
    // This requires the offscreen world color and depth backings selected via
    // setFrameBackground/setFrameDepth. It never publishes partial selection.
    bool drawSelectionIDs(
        const ALMetalSelectionIDScene& scene,
        std::string* error = nullptr);
    // Consumes the most recent same-size selection id/depth target, builds
    // jump and tile metadata, then source-alpha composites the contour into
    // the resumed ordered UI target without modifying its alpha or depth.
    bool drawSelectionContour(
        const ALMetalSelectionContourScene& scene,
        std::string* error = nullptr);
    bool endFrame(std::string* error = nullptr);
    // Offscreen exact-world path: finish the retained texture, append its
    // lower-left-row to CAMetalLayer presentation pass to the same command
    // buffer, and submit once. The offscreen output remains available for
    // ownership handoff after a successful commit. Any drawable or encoding
    // failure abandons the complete command buffer without publishing.
    bool endFrameAndPresent(
        ALMetalSurface& surface,
        std::string* error = nullptr);

    bool hasOpenFrame() const noexcept;
    const ALMetalTexture* outputTexture() const noexcept;
    const ALMetalTexture* outputDepthTexture() const noexcept;
    const ALMetalTexture* selectionIDTexture() const noexcept;
    const ALMetalTexture* selectionDepthTexture() const noexcept;
    // Completes the offscreen renderer-to-viewer ownership handoff. Returns
    // null for a surface-backed renderer or while a frame is still open. The
    // next offscreen draw lazily allocates a fresh target.
    std::unique_ptr<ALMetalTexture> takeOutputTexture() noexcept;
    ALMetalUIRendererStats stats() const noexcept;

private:
    struct Impl;
    explicit ALMetalUIRenderer(std::unique_ptr<Impl> impl);
    bool finishFrame(
        ALMetalSurface* presentation_surface,
        std::string* error);
    std::unique_ptr<Impl> mImpl;
};

