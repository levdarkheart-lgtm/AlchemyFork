/**
 * @file almetalresource.h
 * @brief Typed Metal buffers, textures, render targets and transfer paths.
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

#include "almetaldevice.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

enum class ALMetalStorageMode
{
    shared,
    private_gpu,
    memoryless,
};

enum class ALMetalHazardTrackingMode
{
    tracked,
    untracked,
};

enum class ALMetalCpuCacheMode
{
    default_cache,
    write_combined,
};

struct ALMetalBufferDescriptor
{
    std::size_t length = 0;
    ALMetalStorageMode storage = ALMetalStorageMode::shared;
    ALMetalHazardTrackingMode hazard_tracking = ALMetalHazardTrackingMode::tracked;
    ALMetalCpuCacheMode cpu_cache = ALMetalCpuCacheMode::default_cache;
    std::string label;
};

class ALMetalBuffer final
{
public:
    ALMetalBuffer(const ALMetalBuffer&) = delete;
    ALMetalBuffer& operator=(const ALMetalBuffer&) = delete;
    ALMetalBuffer(ALMetalBuffer&&) = delete;
    ALMetalBuffer& operator=(ALMetalBuffer&&) = delete;
    ~ALMetalBuffer();

    static std::unique_ptr<ALMetalBuffer> create(
        const ALMetalDevice& device,
        const ALMetalBufferDescriptor& descriptor,
        std::string* error = nullptr);

    void* nativeHandle() const noexcept;
    std::size_t length() const noexcept;
    ALMetalStorageMode storageMode() const noexcept;
    void* mappedBytes() const noexcept;

    bool write(std::size_t offset, const void* bytes, std::size_t length,
               std::string* error = nullptr);
    bool read(std::size_t offset, void* bytes, std::size_t length,
              std::string* error = nullptr) const;

    // Shared buffers are updated directly before submission. Private buffers
    // receive a retained staging buffer and a blit on the supplied command.
    bool upload(ALMetalCommandBuffer& command_buffer,
                std::size_t offset,
                const void* bytes,
                std::size_t length,
                std::string* error = nullptr);

    // Explicitly synchronous and intended for screenshots, picking, tests and
    // other exceptional readback paths rather than the steady-state frame.
    bool readback(const ALMetalDevice& device,
                  std::size_t offset,
                  void* bytes,
                  std::size_t length,
                  std::string* error = nullptr) const;

private:
    struct Impl;
    ALMetalBuffer(std::unique_ptr<Impl> impl, ALMetalBufferDescriptor descriptor);
    std::unique_ptr<Impl> mImpl;
    ALMetalBufferDescriptor mDescriptor;
};

enum class ALMetalPixelFormat
{
    invalid,
    r8_unorm,
    r8_sint,
    r8_uint,
    rg8_unorm,
    rg8_sint,
    rg8_uint,
    rgba8_unorm,
    rgba8_unorm_srgb,
    rgba8_sint,
    rgba8_uint,
    bgra8_unorm,
    bgra8_unorm_srgb,
    r16_unorm,
    r16_float,
    r16_sint,
    r16_uint,
    rg16_float,
    rg16_sint,
    rg16_uint,
    rgba16_unorm,
    rgba16_float,
    rgba16_sint,
    rgba16_uint,
    r32_float,
    r32_sint,
    r32_uint,
    rg32_float,
    rg32_sint,
    rg32_uint,
    rgba32_float,
    rgba32_sint,
    rgba32_uint,
    rgb10a2_unorm,
    rgb10a2_uint,
    rg11b10_float,
    bc1_rgba,
    bc1_rgba_srgb,
    bc2_rgba,
    bc2_rgba_srgb,
    bc3_rgba,
    bc3_rgba_srgb,
    depth32_float,
    depth32_float_stencil8,
};

enum class ALMetalTextureSwizzle
{
    zero,
    one,
    red,
    green,
    blue,
    alpha,
};

enum class ALMetalTextureType
{
    texture_2d,
    texture_2d_array,
    texture_cube,
    texture_2d_multisample,
};

enum class ALMetalTextureUsage : std::uint32_t
{
    none = 0,
    shader_read = 1u << 0,
    shader_write = 1u << 1,
    render_target = 1u << 2,
};

constexpr ALMetalTextureUsage operator|(ALMetalTextureUsage lhs, ALMetalTextureUsage rhs)
{
    return static_cast<ALMetalTextureUsage>(
        static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr ALMetalTextureUsage operator&(ALMetalTextureUsage lhs, ALMetalTextureUsage rhs)
{
    return static_cast<ALMetalTextureUsage>(
        static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr bool alMetalHasUsage(ALMetalTextureUsage value, ALMetalTextureUsage flag)
{
    return (value & flag) != ALMetalTextureUsage::none;
}

struct ALMetalTextureDescriptor
{
    ALMetalTextureType type = ALMetalTextureType::texture_2d;
    ALMetalPixelFormat format = ALMetalPixelFormat::rgba8_unorm;
    std::uint32_t width = 1;
    std::uint32_t height = 1;
    std::uint32_t array_length = 1;
    std::uint32_t mipmap_level_count = 1;
    std::uint32_t sample_count = 1;
    ALMetalStorageMode storage = ALMetalStorageMode::private_gpu;
    ALMetalHazardTrackingMode hazard_tracking = ALMetalHazardTrackingMode::tracked;
    ALMetalTextureUsage usage = ALMetalTextureUsage::shader_read;
    std::array<ALMetalTextureSwizzle, 4> swizzle = {
        ALMetalTextureSwizzle::red,
        ALMetalTextureSwizzle::green,
        ALMetalTextureSwizzle::blue,
        ALMetalTextureSwizzle::alpha,
    };
    std::string label;
};

struct ALMetalRegion2D
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

class ALMetalTexture final
{
public:
    ALMetalTexture(const ALMetalTexture&) = delete;
    ALMetalTexture& operator=(const ALMetalTexture&) = delete;
    ALMetalTexture(ALMetalTexture&&) = delete;
    ALMetalTexture& operator=(ALMetalTexture&&) = delete;
    ~ALMetalTexture();

    static std::unique_ptr<ALMetalTexture> create(
        const ALMetalDevice& device,
        const ALMetalTextureDescriptor& descriptor,
        std::string* error = nullptr);

    void* nativeHandle() const noexcept;
    const ALMetalTextureDescriptor& descriptor() const noexcept;
    std::uint32_t width(std::uint32_t mip_level = 0) const noexcept;
    std::uint32_t height(std::uint32_t mip_level = 0) const noexcept;
    std::size_t bytesPerPixel() const noexcept;
    std::uint32_t blockWidth() const noexcept;
    std::uint32_t blockHeight() const noexcept;
    std::size_t bytesPerBlock() const noexcept;
    bool isCompressed() const noexcept;

    // Uploads one uncompressed 2D region. Source rows may be tightly packed;
    // the backend supplies Metal's required staging-row alignment.
    bool upload(ALMetalCommandBuffer& command_buffer,
                std::uint32_t mip_level,
                std::uint32_t slice,
                ALMetalRegion2D region,
                const void* bytes,
                std::size_t bytes_per_row,
                std::string* error = nullptr);

    // Generates every level after level zero in this texture's allocated mip
    // chain. The texture must be uncompressed and have more than one level.
    bool generateMipmaps(ALMetalCommandBuffer& command_buffer,
                         std::string* error = nullptr);

    // Synchronous counterpart for exceptional CPU consumers. Destination row
    // pitch is caller-selected and need not have Metal's blit alignment.
    bool readback(const ALMetalDevice& device,
                  std::uint32_t mip_level,
                  std::uint32_t slice,
                  ALMetalRegion2D region,
                  void* bytes,
                  std::size_t bytes_per_row,
                  std::string* error = nullptr) const;

private:
    struct Impl;
    ALMetalTexture(std::unique_ptr<Impl> impl, ALMetalTextureDescriptor descriptor);
    std::unique_ptr<Impl> mImpl;
    ALMetalTextureDescriptor mDescriptor;

    friend class ALMetalRenderTarget;
};

enum class ALMetalLoadAction
{
    dont_care,
    load,
    clear,
};

enum class ALMetalStoreAction
{
    dont_care,
    store,
    multisample_resolve,
    store_and_multisample_resolve,
};

struct ALMetalClearColor
{
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    double alpha = 0.0;
};

struct ALMetalRenderTargetColorDescriptor
{
    ALMetalPixelFormat format = ALMetalPixelFormat::rgba16_float;
    bool shader_read = true;
    bool shader_write = false;
    std::string label;
};

constexpr std::size_t AL_METAL_MAX_COLOR_ATTACHMENTS = 8;

struct ALMetalRenderTargetDescriptor
{
    std::uint32_t width = 1;
    std::uint32_t height = 1;
    std::uint32_t sample_count = 1;
    std::uint32_t color_attachment_count = 1;
    std::array<ALMetalRenderTargetColorDescriptor, AL_METAL_MAX_COLOR_ATTACHMENTS> colors{};
    ALMetalPixelFormat depth_stencil_format = ALMetalPixelFormat::invalid;
    bool depth_stencil_shader_read = false;
    bool memoryless_multisample = true;
    bool memoryless_depth_stencil = false;
    std::string label;
};

struct ALMetalRenderPassColorConfig
{
    ALMetalLoadAction load = ALMetalLoadAction::clear;
    ALMetalStoreAction store = ALMetalStoreAction::store;
    ALMetalClearColor clear{};
};

struct ALMetalRenderPassConfig
{
    std::array<ALMetalRenderPassColorConfig, AL_METAL_MAX_COLOR_ATTACHMENTS> colors{};
    ALMetalLoadAction depth_load = ALMetalLoadAction::clear;
    ALMetalStoreAction depth_store = ALMetalStoreAction::dont_care;
    double clear_depth = 1.0;
    ALMetalLoadAction stencil_load = ALMetalLoadAction::clear;
    ALMetalStoreAction stencil_store = ALMetalStoreAction::dont_care;
    std::uint32_t clear_stencil = 0;
};

class ALMetalRenderPass final
{
public:
    ALMetalRenderPass(const ALMetalRenderPass&) = delete;
    ALMetalRenderPass& operator=(const ALMetalRenderPass&) = delete;
    ALMetalRenderPass(ALMetalRenderPass&&) = delete;
    ALMetalRenderPass& operator=(ALMetalRenderPass&&) = delete;
    ~ALMetalRenderPass();

    void* nativeHandle() const noexcept;

private:
    struct Impl;
    explicit ALMetalRenderPass(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;

    friend class ALMetalRenderTarget;
};

class ALMetalRenderTarget final
{
public:
    ALMetalRenderTarget(const ALMetalRenderTarget&) = delete;
    ALMetalRenderTarget& operator=(const ALMetalRenderTarget&) = delete;
    ALMetalRenderTarget(ALMetalRenderTarget&&) = delete;
    ALMetalRenderTarget& operator=(ALMetalRenderTarget&&) = delete;
    ~ALMetalRenderTarget();

    static std::unique_ptr<ALMetalRenderTarget> create(
        const ALMetalDevice& device,
        const ALMetalRenderTargetDescriptor& descriptor,
        std::string* error = nullptr);

    const ALMetalRenderTargetDescriptor& descriptor() const noexcept;
    const ALMetalTexture* colorTexture(std::uint32_t index) const noexcept;
    const ALMetalTexture* multisampleColorTexture(std::uint32_t index) const noexcept;
    const ALMetalTexture* depthStencilTexture() const noexcept;

    std::unique_ptr<ALMetalRenderPass> makeRenderPass(
        const ALMetalRenderPassConfig& config,
        std::string* error = nullptr) const;

private:
    struct Impl;
    ALMetalRenderTarget(std::unique_ptr<Impl> impl, ALMetalRenderTargetDescriptor descriptor);
    std::unique_ptr<Impl> mImpl;
    ALMetalRenderTargetDescriptor mDescriptor;
};
