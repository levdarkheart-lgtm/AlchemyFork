/**
 * @file almetaltextureops.h
 * @brief Cached GPU texture transforms used by production resource owners.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#pragma once

#include "almetalresource.h"

#include <cstdint>
#include <memory>
#include <string>

// Build a filterable 2D-array representation of a tightly packed unorm
// volume. Metal has no filterable 3D-texture path in the viewer backend; the
// caller samples adjacent array slices explicitly. RGB input is expanded to
// RGBA with an opaque alpha channel. component_bytes accepts 1 or 2 only.
std::unique_ptr<ALMetalTexture> alMetalCreateUnormVolumeArray(
    const ALMetalDevice& device,
    std::uint32_t edge,
    std::uint32_t components,
    std::uint32_t component_bytes,
    const void* pixels,
    const std::string& label,
    std::string* error = nullptr);

class ALMetalTextureResampler final
{
public:
    ALMetalTextureResampler(const ALMetalTextureResampler&) = delete;
    ALMetalTextureResampler& operator=(const ALMetalTextureResampler&) = delete;
    ~ALMetalTextureResampler();

    static std::unique_ptr<ALMetalTextureResampler> create(
        const ALMetalDevice& device,
        std::string* error = nullptr);

    // Encode a bilinear, clamp-to-edge resize from source level zero to the
    // complete destination. When flip_y is true, dimensions must match and
    // the exact opposite source row is copied without filtering. Both textures
    // remain owned by the caller.
    bool encode(ALMetalCommandBuffer& command_buffer,
                const ALMetalTexture& source,
                const ALMetalTexture& destination,
                bool flip_y,
                std::string* error = nullptr);

    bool encode(ALMetalCommandBuffer& command_buffer,
                const ALMetalTexture& source,
                const ALMetalTexture& destination,
                std::string* error = nullptr)
    {
        return encode(command_buffer, source, destination, false, error);
    }

    static bool supports(ALMetalPixelFormat format) noexcept;

private:
    struct Impl;
    explicit ALMetalTextureResampler(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};

class ALMetalNormalMapGenerator final
{
public:
    ALMetalNormalMapGenerator(const ALMetalNormalMapGenerator&) = delete;
    ALMetalNormalMapGenerator& operator=(
        const ALMetalNormalMapGenerator&) = delete;
    ~ALMetalNormalMapGenerator();

    static std::unique_ptr<ALMetalNormalMapGenerator> create(
        const ALMetalDevice& device,
        std::string* error = nullptr);

    // Reproduce the viewer's brightness/darkness legacy bump-map generator.
    // The caller owns both textures and may generate destination mipmaps on
    // the same command buffer after this render pass.
    bool encode(ALMetalCommandBuffer& command_buffer,
                const ALMetalTexture& source,
                const ALMetalTexture& destination,
                float normal_scale,
                bool darkness,
                std::string* error = nullptr);

private:
    struct Impl;
    explicit ALMetalNormalMapGenerator(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
