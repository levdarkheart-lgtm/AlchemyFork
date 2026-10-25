/**
 * @file almetalsculptpreview.h
 * @brief Native Metal solid-lit upload preview render pass.
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

enum class ALMetalLitPreviewGeometry
{
    indexed_lit,
    unindexed_flat_triangles,
    unindexed_flat_lines,
    unit_sphere,
};

enum class ALMetalLitPreviewDepthMode
{
    less_equal_write,
    greater_read_only,
};

struct ALMetalLitPreviewBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t normal_offset = 0;
    std::size_t texcoord_offset = 0;
    std::size_t color_offset = 0;
    // Optional object-skinning input. The weight stream uses Alchemy's packed
    // float4 convention (integer joint index + fractional influence). The
    // palette is one rebase-origin float4 followed by three float4 columns
    // per joint, matching LLVOAvatar::MatrixPaletteCache::mGLMp.
    std::size_t weight_offset = 0;
    // Optional legacy clothing displacement stream and constants, matching
    // avatarSkinV.glsl's AVATAR_CLOTH path. It is valid only for an unlit,
    // scalar-weighted system-avatar batch.
    std::size_t cloth_offset = 0;
    const float* skin_palette = nullptr;
    std::size_t skin_palette_float_count = 0;
    std::uint32_t skin_joint_count = 0;
    // Select the legacy system-avatar one-light/wrapped-back lighting and
    // alpha-tested texture fragment. Rigid avatar parts use this without a
    // palette; scalar-weight body meshes select it implicitly below.
    bool avatar_lighting = false;
    // Select the deferred avatar-preview albedo writer: encoded-space diffuse
    // RGB, alpha-tested at 0.2, with opaque coverage for accepted fragments.
    // It is valid only with a texture. With avatar_skinning it consumes the
    // scalar palette; without a palette it draws a rigid avatar part.
    bool avatar_unlit = false;
    // Legacy avatar body meshes use one float per vertex: integer joint i plus
    // a fractional blend toward min(i + 1, 14). Their fixed palette is 45
    // float4 rows, laid out as three runs of 15 by
    // LLViewerJointMesh::buildJointMatrixPalette. Object skinning above keeps
    // its packed four-influence convention; the two modes are mutually
    // exclusive even though they share the palette upload storage.
    bool avatar_skinning = false;
    bool avatar_cloth = false;
    // Select an opaque or alpha-masked deferred rigged-attachment albedo
    // writer. It consumes object skinning, encoded U8 vertex tint and an
    // encoded diffuse texture; alpha_cutoff zero represents the opaque pass.
    bool deferred_rigged = false;
    // Select the post-deferred fullbright alpha-mask attachment writer. This
    // uses object skinning and the same encoded tint/diffuse inputs, but the
    // GL owner tests texture alpha alone because vertex alpha stores legacy
    // shininess outside the alpha pool.
    bool fullbright_mask_rigged = false;
    // Select the post-deferred, source-alpha blended fullbright attachment
    // writer. The visual-hint target stores encoded bytes in linear RGBA8,
    // so its fragment performs the GL sRGB framebuffer blend explicitly.
    bool fullbright_rigged = false;
    std::uint32_t vertex_count = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;
    // When present, the batch samples this texture through the preview's
    // encoded-space anisotropic wrap sampler. A null texture selects the
    // solid-lit pipeline and does not require texture coordinates.
    const ALMetalTexture* texture = nullptr;
    // Re-emit the three edges of every indexed triangle as one-pixel Metal
    // line primitives after its fill. The edge vertex path reads the same
    // index buffer directly, so no CPU expansion or transient buffer is
    // required.
    bool show_edges = false;
    // Start a new render pass which retains color but clears depth before
    // drawing this batch. Model previews use this boundary before their
    // optional physics overlay, matching the legacy depth-buffer clear.
    bool clear_depth_before = false;
    // Match the legacy source-alpha/one-minus-source-alpha preview overlay.
    // Blending is immutable pipeline state, so the renderer selects a cached
    // blended pipeline rather than changing state on the encoder.
    bool alpha_blending = false;
    // Flat geometry is used by generated convex hulls and the upload
    // preview's diagnostic lines/spheres. Unit spheres use renderer-owned,
    // immutable geometry; the other modes consume tightly-packed float3s.
    ALMetalLitPreviewGeometry geometry =
        ALMetalLitPreviewGeometry::indexed_lit;
    // Most preview geometry updates depth with GL_LEQUAL semantics. Bone and
    // collision diagnostics immediately repeat each primitive through a
    // read-only GL_GREATER pass so hidden portions remain visible.
    ALMetalLitPreviewDepthMode depth_mode =
        ALMetalLitPreviewDepthMode::less_equal_write;
    // The legacy hull path first writes every hull to depth with color writes
    // disabled, then blends the colored hulls against that completed depth.
    // This is distinct from the ordinary physics-mesh overlay, which has no
    // depth prepass.
    bool depth_prepass = false;
    const ALMetalTexture* destination_template = nullptr;
    const ALMetalTextureDescriptor* destination_descriptor = nullptr;
    float modelview_projection[16]{};
    float normal_matrix[3][4]{};
    float light_direction[3][4]{};
    float light_diffuse[3][4]{};
    float diffuse_color[4] = {1.f, 1.f, 1.f, 1.f};
    float avatar_wind[4]{};
    float avatar_sin_wave[4]{};
    float avatar_gravity[4]{};
    float avatar_cloth_depth[4]{};
    float alpha_cutoff = 0.f;
    float edge_color[4] = {0.4f, 0.4f, 0.4f, 1.f};
};

struct ALMetalLitPreviewScene
{
    const ALMetalLitPreviewBatch* batches = nullptr;
    std::size_t batch_count = 0;
    // Optional full-target encoded-space background. This is sampled through
    // a clamp sampler before any depth-tested preview geometry and does not
    // touch depth, matching a UI image drawn ahead of an avatar hint.
    const ALMetalTexture* background_texture = nullptr;
    const ALMetalTexture* destination_template = nullptr;
    // Dynamic GL preview textures discard their Metal mirror after the GL
    // framebuffer copy. Keep only its validated allocation descriptor so a
    // later native preview frame can allocate a fresh, non-stale target.
    const ALMetalTextureDescriptor* destination_descriptor = nullptr;
    float clear_color[4] = {0.15f, 0.20f, 0.30f, 1.0f};
};

// A deliberately narrow shared pass for solid or diffuse-textured upload
// previews: indexed position/normal meshes over a constant background. It is
// not a generic framebuffer-copy abstraction; each concrete preview still
// owns its scene.
class ALMetalLitPreviewRenderer final
{
public:
    ALMetalLitPreviewRenderer(const ALMetalLitPreviewRenderer&) = delete;
    ALMetalLitPreviewRenderer& operator=(const ALMetalLitPreviewRenderer&) = delete;
    ~ALMetalLitPreviewRenderer();

    static std::unique_ptr<ALMetalLitPreviewRenderer> create(
        const ALMetalDevice& device,
        std::string* error = nullptr);

    std::unique_ptr<ALMetalTexture> render(
        const ALMetalLitPreviewBatch& batch,
        std::string* error = nullptr);
    std::unique_ptr<ALMetalTexture> render(
        const ALMetalLitPreviewScene& scene,
        std::string* error = nullptr);

private:
    struct Impl;
    explicit ALMetalLitPreviewRenderer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
