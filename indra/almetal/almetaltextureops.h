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

#include <memory>
#include <string>

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
    // complete destination. Both textures remain owned by the caller.
    bool encode(ALMetalCommandBuffer& command_buffer,
                const ALMetalTexture& source,
                const ALMetalTexture& destination,
                std::string* error = nullptr);

    static bool supports(ALMetalPixelFormat format) noexcept;

private:
    struct Impl;
    explicit ALMetalTextureResampler(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
