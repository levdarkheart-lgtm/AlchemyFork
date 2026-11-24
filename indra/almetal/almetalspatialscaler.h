/**
 * @file almetalspatialscaler.h
 * @brief Cached native MetalFX spatial scaling on an existing command buffer.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#pragma once

#include "almetalresource.h"

#include <cstdint>
#include <memory>
#include <string>

// Post-display RGB remains perceptually encoded even when stored in a float
// texture. This helper does not perform a color-space conversion or tone map.
enum class ALMetalSpatialScalerColorMode { perceptual };

struct ALMetalSpatialScalerDescriptor
{
    std::uint32_t input_width = 0, input_height = 0;
    std::uint32_t output_width = 0, output_height = 0;
    ALMetalPixelFormat input_format = ALMetalPixelFormat::rgba16_float;
    ALMetalPixelFormat output_format = ALMetalPixelFormat::rgba16_float;
    ALMetalSpatialScalerColorMode color_mode =
        ALMetalSpatialScalerColorMode::perceptual;
    bool operator==(const ALMetalSpatialScalerDescriptor&) const = default;
};

struct ALMetalSpatialScalerRequirements
{
    ALMetalTextureUsage input_usage = ALMetalTextureUsage::none;
    ALMetalTextureUsage output_usage = ALMetalTextureUsage::none;
    std::uint64_t native_input_usage = 0, native_output_usage = 0;
    // MetalFX requires a private output. Both textures must be tracked,
    // non-sRGB, identity-swizzled single-sample 2D textures matching the key.
    ALMetalStorageMode output_storage = ALMetalStorageMode::private_gpu;
};

enum class ALMetalSpatialScalerFailure
{
    none,
    unsupported_device,
    invalid_descriptor,
    unsupported_format,
    creation_failed,
    unsupported_usage,
    incompatible_texture,
    incompatible_command,
    resource_tracking_failed,
    retention_failed,
    encoding_failed,
};

struct ALMetalSpatialScalerStats
{
    // Owner-thread counters only. Native factory success/encode return does
    // not establish command submission, GPU completion, timing or image parity.
    std::uint64_t creation_attempts = 0, creation_successes = 0;
    std::uint64_t cache_hits = 0, encoded_calls = 0;
    std::uint32_t cached_entries = 0, cached_failures = 0;
};

// One serialized CPU owner, bound to one Metal device and its existing queue.
// Four descriptor entries include failed creations, so a rejected format does
// not rebuild every frame. Replaced entries live through commands retaining
// them; repeated preparation/encoding creates no new scaler or GPU texture.
class ALMetalSpatialScaler final
{
public:
    static bool supported(const ALMetalDevice&) noexcept;
    static std::unique_ptr<ALMetalSpatialScaler> create(
        const ALMetalDevice&, std::string* error = nullptr);
    ~ALMetalSpatialScaler();
    ALMetalSpatialScaler(const ALMetalSpatialScaler&) = delete;
    ALMetalSpatialScaler& operator=(const ALMetalSpatialScaler&) = delete;

    // Query before allocating textures. Format support is established by the
    // real descriptor factory, not inferred from supportsDevice(). A failure
    // and its reason are cached for this descriptor just like a successful key.
    bool prepare(const ALMetalSpatialScalerDescriptor&,
                 ALMetalSpatialScalerRequirements&,
                 std::string* error = nullptr);

    // No queue creation, submission or wait. End any enclosing encoder first.
    // Caller retains pool leases/current generations exactly as for its other
    // passes. Native resource retention alone does not exclude pooled rewrites.
    // MetalFX owns hidden scratch, so this conservatively marks the command's
    // resource plan opaque while declaring the two external backing accesses.
    // On failure the caller must abandon the enclosing command; encoding may
    // already have started. Fallback belongs before encode(), after prepare().
    bool encode(ALMetalCommandBuffer&, const ALMetalSpatialScalerDescriptor&,
                const ALMetalTexture& input, const ALMetalTexture& output,
                std::string* error = nullptr);

    ALMetalSpatialScalerFailure lastFailure() const noexcept;
    ALMetalSpatialScalerStats stats() const noexcept;

private:
    struct Impl;
    explicit ALMetalSpatialScaler(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};
