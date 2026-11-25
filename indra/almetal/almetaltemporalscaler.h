/**
 * @file almetaltemporalscaler.h
 * @brief One main-world classic MetalFX temporal history stream.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#pragma once

#include "almetalresource.h"

#include <cstdint>
#include <memory>
#include <string>

struct ALMetalTemporalScalerDescriptor
{
    std::uint32_t input_width = 0, input_height = 0;
    std::uint32_t output_width = 0, output_height = 0;
    ALMetalPixelFormat color_format = ALMetalPixelFormat::rgba16_float;
    ALMetalPixelFormat depth_format = ALMetalPixelFormat::r32_float;
    ALMetalPixelFormat motion_format = ALMetalPixelFormat::rg16_float;
    ALMetalPixelFormat output_format = ALMetalPixelFormat::rgba16_float;
    bool reactive_mask = true;
    bool operator==(const ALMetalTemporalScalerDescriptor&) const = default;
};

struct ALMetalTemporalScalerRequirements
{
    ALMetalTextureUsage color_usage = ALMetalTextureUsage::none;
    ALMetalTextureUsage depth_usage = ALMetalTextureUsage::none;
    ALMetalTextureUsage motion_usage = ALMetalTextureUsage::none;
    ALMetalTextureUsage reactive_usage = ALMetalTextureUsage::none;
    ALMetalTextureUsage output_usage = ALMetalTextureUsage::none;
    std::uint64_t native_color_usage = 0, native_depth_usage = 0;
    std::uint64_t native_motion_usage = 0, native_reactive_usage = 0;
    std::uint64_t native_output_usage = 0;
};

struct ALMetalTemporalScalerSupport
{
    bool supported = false, reactive_mask = false;
    // SDK methods named supportedInputContent*Scale return output/input
    // factors on this device (M1 Max: 1..3), not fractional input/output.
    float minimum_upscale_factor = 0.f, maximum_upscale_factor = 0.f;
};

struct ALMetalTemporalScalerInputs
{
    const ALMetalTexture* color = nullptr;
    const ALMetalTexture* depth = nullptr;
    const ALMetalTexture* motion = nullptr;
    // R8Unorm at input resolution; zero uses history normally, one rejects it.
    const ALMetalTexture* reactive = nullptr;
    const ALMetalTexture* output = nullptr;
};

struct ALMetalTemporalScalerFrame
{
    // Input pixels, top-left framebuffer origin, Y down. Motion is unjittered
    // previous minus current pixel position, so both native motion scales are 1.
    float jitter_x = 0.f, jitter_y = 0.f;
    float pre_exposure = 1.f;
    bool depth_reversed = true;
    bool reset = false;
};

enum class ALMetalTemporalScalerFailure
{
    none, unsupported_device, invalid_descriptor, unsupported_format,
    creation_failed, unsupported_usage, incompatible_texture,
    incompatible_command, resource_tracking_failed, retention_failed,
    encoding_failed, transaction_pending, invalid_transaction,
};

struct ALMetalTemporalScalerStats
{
    std::uint64_t creation_attempts = 0, creation_successes = 0;
    std::uint64_t prepare_reuses = 0, encoded_calls = 0, submitted_calls = 0;
    std::uint64_t abandoned_calls = 0, reset_calls = 0, gpu_failures = 0;
    std::uint32_t pending_submissions = 0;
    bool history_valid = false;
};

// Serialized CPU owner, one existing device/queue and ONE continuous history.
// A descriptor change replaces the stream, never rotates among cached histories.
// Superseded scalers and their hidden resources remain retained by their commands.
class ALMetalTemporalScaler final
{
public:
    using Ticket = std::uint64_t;
    static ALMetalTemporalScalerSupport support(const ALMetalDevice&) noexcept;
    static std::unique_ptr<ALMetalTemporalScaler> create(
        const ALMetalDevice&, std::string* error = nullptr);
    ~ALMetalTemporalScaler();
    ALMetalTemporalScaler(const ALMetalTemporalScaler&) = delete;
    ALMetalTemporalScaler& operator=(const ALMetalTemporalScaler&) = delete;

    bool prepare(const ALMetalTemporalScalerDescriptor&,
                 ALMetalTemporalScalerRequirements&, std::string* error = nullptr);
    // Preflight first, before applying raster jitter. Failure after rasterization
    // requires cancelling that world's publication, not unjittered linear fallback.
    // No command creation, queue submission, completion wait or readback occurs.
    bool encode(ALMetalCommandBuffer&, const ALMetalTemporalScalerDescriptor&,
                const ALMetalTemporalScalerInputs&, const ALMetalTemporalScalerFrame&,
                Ticket&, std::string* error = nullptr);
    // Call only after the encompassing command commits successfully. This is the
    // history commit point; GPU completion is not required. The same queue orders
    // subsequent uses. Call abandon after any unsubmitted transaction failure.
    bool submitted(Ticket, std::string* error = nullptr);
    void abandon(Ticket) noexcept;
    // Main-view discontinuity, mode switch, skipped world or recovery. Does not
    // touch already submitted work. The next encode explicitly resets history.
    void invalidate() noexcept;
    ALMetalTemporalScalerFailure lastFailure() const noexcept;
    ALMetalTemporalScalerStats stats() const noexcept;

private:
    struct Impl;
    explicit ALMetalTemporalScaler(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};
