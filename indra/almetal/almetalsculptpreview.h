/**
 * @file almetalsculptpreview.h
 * @brief Native Metal sculpt-image preview render pass.
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

struct ALMetalSculptPreviewBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t normal_offset = 0;
    std::uint32_t vertex_count = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;
    const ALMetalTexture* destination_template = nullptr;
    float modelview_projection[16]{};
    float normal_matrix[3][4]{};
    float light_direction[3][4]{};
    float light_diffuse[3][4]{};
    float diffuse_color[4] = {1.f, 1.f, 1.f, 1.f};
};

// The sculpt upload preview is a deliberately narrow dynamic-texture owner:
// an indexed position/normal mesh over a constant background. Keeping it here
// avoids recreating the old generic framebuffer-copy abstraction in Metal.
class ALMetalSculptPreviewRenderer final
{
public:
    ALMetalSculptPreviewRenderer(const ALMetalSculptPreviewRenderer&) = delete;
    ALMetalSculptPreviewRenderer& operator=(const ALMetalSculptPreviewRenderer&) = delete;
    ~ALMetalSculptPreviewRenderer();

    static std::unique_ptr<ALMetalSculptPreviewRenderer> create(
        const ALMetalDevice& device,
        std::string* error = nullptr);

    std::unique_ptr<ALMetalTexture> render(
        const ALMetalSculptPreviewBatch& batch,
        std::string* error = nullptr);

private:
    struct Impl;
    explicit ALMetalSculptPreviewRenderer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
