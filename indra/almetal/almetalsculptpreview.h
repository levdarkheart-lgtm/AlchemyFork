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

struct ALMetalLitPreviewBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t normal_offset = 0;
    std::size_t texcoord_offset = 0;
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
    const ALMetalTexture* destination_template = nullptr;
    float modelview_projection[16]{};
    float normal_matrix[3][4]{};
    float light_direction[3][4]{};
    float light_diffuse[3][4]{};
    float diffuse_color[4] = {1.f, 1.f, 1.f, 1.f};
    float edge_color[4] = {0.4f, 0.4f, 0.4f, 1.f};
};

struct ALMetalLitPreviewScene
{
    const ALMetalLitPreviewBatch* batches = nullptr;
    std::size_t batch_count = 0;
    const ALMetalTexture* destination_template = nullptr;
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
