/**
 * @file almetalterrainpaint.h
 * @brief Native Metal terrain paintmap bake.
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

struct ALMetalTerrainPaintBatch
{
    const ALMetalBuffer* vertices = nullptr;
    std::size_t position_offset = 0;
    std::size_t texcoord_offset = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
    std::uint32_t index_count = 0;
    const ALMetalTexture* alpha_ramp = nullptr;
    const ALMetalTexture* destination_template = nullptr;
    float modelview_projection[16]{};
    float precision = 255.f;
};

// The terrain editor creates this bake only on demand, but the shader library,
// pipeline and sampler remain cached across bakes. Each render produces a new
// shader-readable texture so production can publish the completed native bake
// atomically while retaining the previous texture on failure.
class ALMetalTerrainPaintRenderer final
{
public:
    ALMetalTerrainPaintRenderer(const ALMetalTerrainPaintRenderer&) = delete;
    ALMetalTerrainPaintRenderer& operator=(const ALMetalTerrainPaintRenderer&) = delete;
    ~ALMetalTerrainPaintRenderer();

    static std::unique_ptr<ALMetalTerrainPaintRenderer> create(
        const ALMetalDevice& device,
        std::string* error = nullptr);

    std::unique_ptr<ALMetalTexture> render(
        const ALMetalTerrainPaintBatch& batch,
        std::string* error = nullptr);

    std::uint64_t shaderReloadGeneration() const noexcept;

private:
    struct Impl;
    explicit ALMetalTerrainPaintRenderer(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
