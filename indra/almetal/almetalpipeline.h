/**
 * @file almetalpipeline.h
 * @brief Native MSL libraries, immutable pipeline state and command encoders.
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

#include "almetalresource.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class ALMetalShaderLibrary final
{
public:
    ALMetalShaderLibrary(const ALMetalShaderLibrary&) = delete;
    ALMetalShaderLibrary& operator=(const ALMetalShaderLibrary&) = delete;
    ALMetalShaderLibrary(ALMetalShaderLibrary&&) = delete;
    ALMetalShaderLibrary& operator=(ALMetalShaderLibrary&&) = delete;
    ~ALMetalShaderLibrary();

    static std::unique_ptr<ALMetalShaderLibrary> loadBuiltin(
        const ALMetalDevice& device,
        std::string* error = nullptr);
    static std::unique_ptr<ALMetalShaderLibrary> loadFile(
        const ALMetalDevice& device,
        const std::string& path,
        std::string* error = nullptr);
    static std::unique_ptr<ALMetalShaderLibrary> compileSource(
        const ALMetalDevice& device,
        const std::string& source,
        const std::string& label,
        std::string* error = nullptr);

    void* nativeHandle() const noexcept;
    bool hasFunction(const std::string& name) const;

private:
    struct Impl;
    explicit ALMetalShaderLibrary(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;

    friend class ALMetalRenderPipeline;
    friend class ALMetalComputePipeline;
};

enum class ALMetalBlendFactor
{
    zero,
    one,
    source_color,
    one_minus_source_color,
    source_alpha,
    one_minus_source_alpha,
    destination_color,
    one_minus_destination_color,
    destination_alpha,
    one_minus_destination_alpha,
};

enum class ALMetalBlendOperation
{
    add,
    subtract,
    reverse_subtract,
    minimum,
    maximum,
};

enum class ALMetalColorWriteMask : std::uint8_t
{
    none = 0,
    red = 1u << 0,
    green = 1u << 1,
    blue = 1u << 2,
    alpha = 1u << 3,
    all = red | green | blue | alpha,
};

struct ALMetalColorPipelineDescriptor
{
    ALMetalPixelFormat format = ALMetalPixelFormat::invalid;
    bool blending_enabled = false;
    ALMetalBlendFactor source_rgb = ALMetalBlendFactor::one;
    ALMetalBlendFactor destination_rgb = ALMetalBlendFactor::zero;
    ALMetalBlendOperation rgb_operation = ALMetalBlendOperation::add;
    ALMetalBlendFactor source_alpha = ALMetalBlendFactor::one;
    ALMetalBlendFactor destination_alpha = ALMetalBlendFactor::zero;
    ALMetalBlendOperation alpha_operation = ALMetalBlendOperation::add;
    ALMetalColorWriteMask write_mask = ALMetalColorWriteMask::all;
};

enum class ALMetalTessellationPartitionMode
{
    power_of_two,
    integer,
    fractional_odd,
    fractional_even,
};

enum class ALMetalTessellationFactorStepFunction
{
    constant,
    per_patch,
    per_instance,
    per_patch_and_instance,
};

enum class ALMetalTessellationOutputWinding
{
    clockwise,
    counter_clockwise,
};

struct ALMetalTessellationDescriptor
{
    bool enabled = false;
    ALMetalTessellationPartitionMode partition_mode = ALMetalTessellationPartitionMode::integer;
    std::uint32_t maximum_factor = 1;
    bool factor_scale_enabled = false;
    ALMetalTessellationFactorStepFunction factor_step_function =
        ALMetalTessellationFactorStepFunction::constant;
    ALMetalTessellationOutputWinding output_winding =
        ALMetalTessellationOutputWinding::counter_clockwise;
};

struct ALMetalRenderPipelineDescriptor
{
    const ALMetalShaderLibrary* library = nullptr;
    std::string vertex_function;
    std::string fragment_function;
    std::uint32_t color_attachment_count = 1;
    std::array<ALMetalColorPipelineDescriptor, AL_METAL_MAX_COLOR_ATTACHMENTS> colors{};
    ALMetalPixelFormat depth_stencil_format = ALMetalPixelFormat::invalid;
    std::uint32_t sample_count = 1;
    bool support_indirect_commands = false;
    ALMetalTessellationDescriptor tessellation;
    std::string label;
};

class ALMetalRenderPipeline final
{
public:
    ALMetalRenderPipeline(const ALMetalRenderPipeline&) = delete;
    ALMetalRenderPipeline& operator=(const ALMetalRenderPipeline&) = delete;
    ALMetalRenderPipeline(ALMetalRenderPipeline&&) = delete;
    ALMetalRenderPipeline& operator=(ALMetalRenderPipeline&&) = delete;
    ~ALMetalRenderPipeline();

    static std::unique_ptr<ALMetalRenderPipeline> create(
        const ALMetalDevice& device,
        const ALMetalRenderPipelineDescriptor& descriptor,
        std::string* error = nullptr);

    void* nativeHandle() const noexcept;
    const ALMetalRenderPipelineDescriptor& descriptor() const noexcept;

private:
    struct Impl;
    ALMetalRenderPipeline(std::unique_ptr<Impl> impl, ALMetalRenderPipelineDescriptor descriptor);
    std::unique_ptr<Impl> mImpl;
    ALMetalRenderPipelineDescriptor mDescriptor;

    friend class ALMetalRenderEncoder;
};

struct ALMetalComputePipelineDescriptor
{
    const ALMetalShaderLibrary* library = nullptr;
    std::string function;
    std::string label;
};

class ALMetalComputePipeline final
{
public:
    ALMetalComputePipeline(const ALMetalComputePipeline&) = delete;
    ALMetalComputePipeline& operator=(const ALMetalComputePipeline&) = delete;
    ALMetalComputePipeline(ALMetalComputePipeline&&) = delete;
    ALMetalComputePipeline& operator=(ALMetalComputePipeline&&) = delete;
    ~ALMetalComputePipeline();

    static std::unique_ptr<ALMetalComputePipeline> create(
        const ALMetalDevice& device,
        const ALMetalComputePipelineDescriptor& descriptor,
        std::string* error = nullptr);

    void* nativeHandle() const noexcept;
    std::uint32_t threadExecutionWidth() const noexcept;
    std::uint32_t maximumTotalThreadsPerThreadgroup() const noexcept;

private:
    struct Impl;
    explicit ALMetalComputePipeline(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;

    friend class ALMetalComputeEncoder;
};

enum class ALMetalCompareFunction
{
    never,
    less,
    equal,
    less_equal,
    greater,
    not_equal,
    greater_equal,
    always,
};

enum class ALMetalStencilOperation
{
    keep,
    zero,
    replace,
    increment_clamp,
    decrement_clamp,
    invert,
    increment_wrap,
    decrement_wrap,
};

struct ALMetalStencilDescriptor
{
    ALMetalCompareFunction compare = ALMetalCompareFunction::always;
    ALMetalStencilOperation stencil_failure =
        ALMetalStencilOperation::keep;
    ALMetalStencilOperation depth_failure =
        ALMetalStencilOperation::keep;
    ALMetalStencilOperation depth_stencil_pass =
        ALMetalStencilOperation::keep;
    std::uint32_t read_mask = 0xffffffffu;
    std::uint32_t write_mask = 0xffffffffu;
};

struct ALMetalDepthStencilDescriptor
{
    ALMetalCompareFunction depth_compare = ALMetalCompareFunction::less_equal;
    bool depth_write_enabled = true;
    bool stencil_enabled = false;
    ALMetalStencilDescriptor front_stencil;
    ALMetalStencilDescriptor back_stencil;
    std::string label;
};

class ALMetalDepthStencilState final
{
public:
    ALMetalDepthStencilState(const ALMetalDepthStencilState&) = delete;
    ALMetalDepthStencilState& operator=(const ALMetalDepthStencilState&) = delete;
    ~ALMetalDepthStencilState();

    static std::unique_ptr<ALMetalDepthStencilState> create(
        const ALMetalDevice& device,
        const ALMetalDepthStencilDescriptor& descriptor,
        std::string* error = nullptr);

    void* nativeHandle() const noexcept;

private:
    struct Impl;
    explicit ALMetalDepthStencilState(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;

    friend class ALMetalRenderEncoder;
};

enum class ALMetalSamplerFilter { nearest, linear };
enum class ALMetalSamplerMipFilter { none, nearest, linear };
enum class ALMetalSamplerAddressMode { clamp_to_edge, repeat, mirror_repeat, clamp_to_zero };

struct ALMetalSamplerDescriptor
{
    ALMetalSamplerFilter min_filter = ALMetalSamplerFilter::linear;
    ALMetalSamplerFilter mag_filter = ALMetalSamplerFilter::linear;
    ALMetalSamplerMipFilter mip_filter = ALMetalSamplerMipFilter::linear;
    ALMetalSamplerAddressMode address_s = ALMetalSamplerAddressMode::repeat;
    ALMetalSamplerAddressMode address_t = ALMetalSamplerAddressMode::repeat;
    ALMetalSamplerAddressMode address_r = ALMetalSamplerAddressMode::repeat;
    std::uint32_t max_anisotropy = 1;
    bool normalized_coordinates = true;
    std::string label;
};

class ALMetalSamplerState final
{
public:
    ALMetalSamplerState(const ALMetalSamplerState&) = delete;
    ALMetalSamplerState& operator=(const ALMetalSamplerState&) = delete;
    ~ALMetalSamplerState();

    static std::unique_ptr<ALMetalSamplerState> create(
        const ALMetalDevice& device,
        const ALMetalSamplerDescriptor& descriptor,
        std::string* error = nullptr);

    void* nativeHandle() const noexcept;
    std::uint32_t maxAnisotropy() const noexcept;

private:
    struct Impl;
    explicit ALMetalSamplerState(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;

    friend class ALMetalRenderEncoder;
    friend class ALMetalComputeEncoder;
};

enum class ALMetalPrimitiveType { point, line, line_strip, triangle, triangle_strip };
enum class ALMetalIndexType { uint16, uint32 };
enum class ALMetalCullMode { none, front, back };
enum class ALMetalTriangleFillMode { fill, lines };
enum class ALMetalWinding { clockwise, counter_clockwise };
enum class ALMetalDepthClipMode { clip, clamp };
enum class ALMetalVisibilityResultMode { disabled, boolean, counting };

struct ALMetalViewport
{
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    double near_depth = 0.0;
    double far_depth = 1.0;
};

struct ALMetalScissorRect
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

class ALMetalRenderEncoder final
{
public:
    ALMetalRenderEncoder(const ALMetalRenderEncoder&) = delete;
    ALMetalRenderEncoder& operator=(const ALMetalRenderEncoder&) = delete;
    ~ALMetalRenderEncoder();

    static std::unique_ptr<ALMetalRenderEncoder> begin(
        ALMetalCommandBuffer& command_buffer,
        const ALMetalRenderPass& render_pass,
        const std::string& label,
        std::string* error = nullptr);

    bool setPipeline(const ALMetalRenderPipeline& pipeline, std::string* error = nullptr);
    bool setDepthStencilState(const ALMetalDepthStencilState* state, std::string* error = nullptr);
    bool setStencilReferenceValue(std::uint32_t value,
                                  std::string* error = nullptr);
    bool setVertexBuffer(std::uint32_t index, const ALMetalBuffer& buffer,
                         std::size_t offset = 0, std::string* error = nullptr);
    bool setFragmentBuffer(std::uint32_t index, const ALMetalBuffer& buffer,
                           std::size_t offset = 0, std::string* error = nullptr);
    bool setVertexBytes(std::uint32_t index, const void* bytes, std::size_t length,
                        std::string* error = nullptr);
    bool setFragmentBytes(std::uint32_t index, const void* bytes, std::size_t length,
                          std::string* error = nullptr);
    bool setVertexTexture(std::uint32_t index, const ALMetalTexture* texture,
                          std::string* error = nullptr);
    bool setFragmentTexture(std::uint32_t index, const ALMetalTexture* texture,
                            std::string* error = nullptr);
    // Bind a view that samples encoded sRGB storage as linear UNORM values.
    bool setFragmentTextureLinear(std::uint32_t index, const ALMetalTexture* texture,
                                  std::string* error = nullptr);
    bool setVertexSampler(std::uint32_t index, const ALMetalSamplerState* sampler,
                          std::string* error = nullptr);
    bool setFragmentSampler(std::uint32_t index, const ALMetalSamplerState* sampler,
                            std::string* error = nullptr);
    bool setViewport(const ALMetalViewport& viewport, std::string* error = nullptr);
    bool setScissor(const ALMetalScissorRect& scissor, std::string* error = nullptr);
    bool setCullMode(ALMetalCullMode mode, std::string* error = nullptr);
    bool setTriangleFillMode(ALMetalTriangleFillMode mode,
                             std::string* error = nullptr);
    bool setFrontFacingWinding(ALMetalWinding winding, std::string* error = nullptr);
    bool setDepthClipMode(ALMetalDepthClipMode mode,
                          std::string* error = nullptr);
    bool setDepthBias(float constant_bias, float slope_scale, float clamp,
                      std::string* error = nullptr);
    bool setVisibilityResultMode(
        ALMetalVisibilityResultMode mode,
        std::size_t offset = 0,
        std::string* error = nullptr);
    bool setTessellationFactorBuffer(const ALMetalBuffer& buffer, std::size_t offset = 0,
                                     std::size_t instance_stride = 0,
                                     std::string* error = nullptr);
    bool setTessellationFactorScale(float scale, std::string* error = nullptr);
    bool draw(ALMetalPrimitiveType primitive, std::uint32_t vertex_start,
              std::uint32_t vertex_count, std::uint32_t instance_count = 1,
              std::string* error = nullptr);
    bool drawIndexed(ALMetalPrimitiveType primitive, std::uint32_t index_count,
                     ALMetalIndexType index_type, const ALMetalBuffer& index_buffer,
                     std::size_t index_buffer_offset = 0, std::uint32_t instance_count = 1,
                     std::string* error = nullptr);
    bool drawPatches(std::uint32_t control_point_count, std::uint32_t patch_start,
                     std::uint32_t patch_count, std::uint32_t instance_count = 1,
                     std::uint32_t base_instance = 0, std::string* error = nullptr);
    bool end(std::string* error = nullptr);

private:
    struct Impl;
    explicit ALMetalRenderEncoder(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;

    // Narrow native bridge for the owned shadow-command helper. Indirect
    // execution must preserve the parent state and invalidate buffer caches.
    void* nativeHandleForIndirect() const noexcept;
    void* nativeDeviceForIndirect() const noexcept;
    void* nativeCommandBufferForIndirect() const noexcept;
    bool canExecuteIndirect() const noexcept;
    bool canExecuteStaticDraws() const noexcept;
    void invalidateIndirectBufferBindings() noexcept;
    void recordPrevalidatedDraws(std::uint32_t count) noexcept;
    friend class ALMetalShadowIndirectCommands;
};

class ALMetalComputeEncoder final
{
public:
    ALMetalComputeEncoder(const ALMetalComputeEncoder&) = delete;
    ALMetalComputeEncoder& operator=(const ALMetalComputeEncoder&) = delete;
    ~ALMetalComputeEncoder();

    static std::unique_ptr<ALMetalComputeEncoder> begin(
        ALMetalCommandBuffer& command_buffer,
        const std::string& label,
        std::string* error = nullptr);

    bool setPipeline(const ALMetalComputePipeline& pipeline, std::string* error = nullptr);
    bool setBuffer(std::uint32_t index, const ALMetalBuffer& buffer,
                   std::size_t offset = 0, std::string* error = nullptr);
    bool setBytes(std::uint32_t index, const void* bytes, std::size_t length,
                  std::string* error = nullptr);
    bool setTexture(std::uint32_t index, const ALMetalTexture* texture,
                    std::string* error = nullptr);
    bool setSampler(std::uint32_t index, const ALMetalSamplerState* sampler,
                    std::string* error = nullptr);
    bool dispatch(std::uint32_t width, std::uint32_t height, std::uint32_t depth,
                  std::uint32_t threads_per_group_width,
                  std::uint32_t threads_per_group_height = 1,
                  std::uint32_t threads_per_group_depth = 1,
                  std::string* error = nullptr);
    bool end(std::string* error = nullptr);

private:
    struct Impl;
    explicit ALMetalComputeEncoder(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
