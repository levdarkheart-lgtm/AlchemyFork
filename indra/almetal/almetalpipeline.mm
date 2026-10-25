/**
 * @file almetalpipeline.mm
 * @brief Native MSL libraries, immutable pipeline state and command encoders.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "almetalpipeline.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#ifndef AL_METAL_BUILTIN_SOURCE_PATH
#define AL_METAL_BUILTIN_SOURCE_PATH ""
#endif

namespace
{
    void setError(std::string* destination, const std::string& message)
    {
        if (destination)
        {
            *destination = message;
        }
    }

    std::string fromNSString(NSString* value)
    {
        const char* utf8 = value.UTF8String;
        return utf8 ? std::string(utf8) : std::string();
    }

    std::string describeError(NSError* error)
    {
        if (!error)
        {
            return "Metal operation failed without an NSError";
        }
        return fromNSString([NSString stringWithFormat:@"%@ (%@:%ld)",
                             error.localizedDescription,
                             error.domain,
                             static_cast<long>(error.code)]);
    }

    NSString* toNSString(const std::string& value)
    {
        return value.empty() ? nil : [NSString stringWithUTF8String:value.c_str()];
    }

    MTLPixelFormat pixelFormat(ALMetalPixelFormat format)
    {
        switch (format)
        {
            case ALMetalPixelFormat::r8_unorm:               return MTLPixelFormatR8Unorm;
            case ALMetalPixelFormat::r8_sint:                return MTLPixelFormatR8Sint;
            case ALMetalPixelFormat::r8_uint:                return MTLPixelFormatR8Uint;
            case ALMetalPixelFormat::rg8_unorm:              return MTLPixelFormatRG8Unorm;
            case ALMetalPixelFormat::rg8_sint:               return MTLPixelFormatRG8Sint;
            case ALMetalPixelFormat::rg8_uint:               return MTLPixelFormatRG8Uint;
            case ALMetalPixelFormat::rgba8_unorm:            return MTLPixelFormatRGBA8Unorm;
            case ALMetalPixelFormat::rgba8_unorm_srgb:       return MTLPixelFormatRGBA8Unorm_sRGB;
            case ALMetalPixelFormat::rgba8_sint:             return MTLPixelFormatRGBA8Sint;
            case ALMetalPixelFormat::rgba8_uint:             return MTLPixelFormatRGBA8Uint;
            case ALMetalPixelFormat::bgra8_unorm:            return MTLPixelFormatBGRA8Unorm;
            case ALMetalPixelFormat::bgra8_unorm_srgb:       return MTLPixelFormatBGRA8Unorm_sRGB;
            case ALMetalPixelFormat::r16_unorm:              return MTLPixelFormatR16Unorm;
            case ALMetalPixelFormat::r16_float:              return MTLPixelFormatR16Float;
            case ALMetalPixelFormat::r16_sint:               return MTLPixelFormatR16Sint;
            case ALMetalPixelFormat::r16_uint:               return MTLPixelFormatR16Uint;
            case ALMetalPixelFormat::rg16_float:             return MTLPixelFormatRG16Float;
            case ALMetalPixelFormat::rg16_sint:              return MTLPixelFormatRG16Sint;
            case ALMetalPixelFormat::rg16_uint:              return MTLPixelFormatRG16Uint;
            case ALMetalPixelFormat::rgba16_unorm:           return MTLPixelFormatRGBA16Unorm;
            case ALMetalPixelFormat::rgba16_float:           return MTLPixelFormatRGBA16Float;
            case ALMetalPixelFormat::rgba16_sint:            return MTLPixelFormatRGBA16Sint;
            case ALMetalPixelFormat::rgba16_uint:            return MTLPixelFormatRGBA16Uint;
            case ALMetalPixelFormat::r32_float:              return MTLPixelFormatR32Float;
            case ALMetalPixelFormat::r32_sint:               return MTLPixelFormatR32Sint;
            case ALMetalPixelFormat::r32_uint:               return MTLPixelFormatR32Uint;
            case ALMetalPixelFormat::rg32_float:             return MTLPixelFormatRG32Float;
            case ALMetalPixelFormat::rg32_sint:              return MTLPixelFormatRG32Sint;
            case ALMetalPixelFormat::rg32_uint:              return MTLPixelFormatRG32Uint;
            case ALMetalPixelFormat::rgba32_float:           return MTLPixelFormatRGBA32Float;
            case ALMetalPixelFormat::rgba32_sint:            return MTLPixelFormatRGBA32Sint;
            case ALMetalPixelFormat::rgba32_uint:            return MTLPixelFormatRGBA32Uint;
            case ALMetalPixelFormat::rgb10a2_unorm:          return MTLPixelFormatRGB10A2Unorm;
            case ALMetalPixelFormat::rgb10a2_uint:           return MTLPixelFormatRGB10A2Uint;
            case ALMetalPixelFormat::rg11b10_float:          return MTLPixelFormatRG11B10Float;
            case ALMetalPixelFormat::bc1_rgba:               return MTLPixelFormatBC1_RGBA;
            case ALMetalPixelFormat::bc1_rgba_srgb:          return MTLPixelFormatBC1_RGBA_sRGB;
            case ALMetalPixelFormat::bc2_rgba:               return MTLPixelFormatBC2_RGBA;
            case ALMetalPixelFormat::bc2_rgba_srgb:          return MTLPixelFormatBC2_RGBA_sRGB;
            case ALMetalPixelFormat::bc3_rgba:               return MTLPixelFormatBC3_RGBA;
            case ALMetalPixelFormat::bc3_rgba_srgb:          return MTLPixelFormatBC3_RGBA_sRGB;
            case ALMetalPixelFormat::depth32_float:          return MTLPixelFormatDepth32Float;
            case ALMetalPixelFormat::depth32_float_stencil8: return MTLPixelFormatDepth32Float_Stencil8;
            case ALMetalPixelFormat::invalid:                return MTLPixelFormatInvalid;
        }
        return MTLPixelFormatInvalid;
    }

    MTLBlendFactor blendFactor(ALMetalBlendFactor factor)
    {
        switch (factor)
        {
            case ALMetalBlendFactor::zero:                        return MTLBlendFactorZero;
            case ALMetalBlendFactor::one:                         return MTLBlendFactorOne;
            case ALMetalBlendFactor::source_color:                return MTLBlendFactorSourceColor;
            case ALMetalBlendFactor::one_minus_source_color:      return MTLBlendFactorOneMinusSourceColor;
            case ALMetalBlendFactor::source_alpha:                return MTLBlendFactorSourceAlpha;
            case ALMetalBlendFactor::one_minus_source_alpha:      return MTLBlendFactorOneMinusSourceAlpha;
            case ALMetalBlendFactor::destination_color:           return MTLBlendFactorDestinationColor;
            case ALMetalBlendFactor::one_minus_destination_color: return MTLBlendFactorOneMinusDestinationColor;
            case ALMetalBlendFactor::destination_alpha:           return MTLBlendFactorDestinationAlpha;
            case ALMetalBlendFactor::one_minus_destination_alpha: return MTLBlendFactorOneMinusDestinationAlpha;
        }
        return MTLBlendFactorOne;
    }

    MTLBlendOperation blendOperation(ALMetalBlendOperation operation)
    {
        switch (operation)
        {
            case ALMetalBlendOperation::add:              return MTLBlendOperationAdd;
            case ALMetalBlendOperation::subtract:         return MTLBlendOperationSubtract;
            case ALMetalBlendOperation::reverse_subtract: return MTLBlendOperationReverseSubtract;
            case ALMetalBlendOperation::minimum:          return MTLBlendOperationMin;
            case ALMetalBlendOperation::maximum:          return MTLBlendOperationMax;
        }
        return MTLBlendOperationAdd;
    }

    MTLColorWriteMask colorWriteMask(ALMetalColorWriteMask value)
    {
        const auto bits = static_cast<std::uint8_t>(value);
        MTLColorWriteMask mask = MTLColorWriteMaskNone;
        if ((bits & static_cast<std::uint8_t>(ALMetalColorWriteMask::red)) != 0)
        {
            mask |= MTLColorWriteMaskRed;
        }
        if ((bits & static_cast<std::uint8_t>(ALMetalColorWriteMask::green)) != 0)
        {
            mask |= MTLColorWriteMaskGreen;
        }
        if ((bits & static_cast<std::uint8_t>(ALMetalColorWriteMask::blue)) != 0)
        {
            mask |= MTLColorWriteMaskBlue;
        }
        if ((bits & static_cast<std::uint8_t>(ALMetalColorWriteMask::alpha)) != 0)
        {
            mask |= MTLColorWriteMaskAlpha;
        }
        return mask;
    }

    MTLCompareFunction compareFunction(ALMetalCompareFunction function)
    {
        switch (function)
        {
            case ALMetalCompareFunction::never:         return MTLCompareFunctionNever;
            case ALMetalCompareFunction::less:          return MTLCompareFunctionLess;
            case ALMetalCompareFunction::equal:         return MTLCompareFunctionEqual;
            case ALMetalCompareFunction::less_equal:    return MTLCompareFunctionLessEqual;
            case ALMetalCompareFunction::greater:       return MTLCompareFunctionGreater;
            case ALMetalCompareFunction::not_equal:     return MTLCompareFunctionNotEqual;
            case ALMetalCompareFunction::greater_equal: return MTLCompareFunctionGreaterEqual;
            case ALMetalCompareFunction::always:        return MTLCompareFunctionAlways;
        }
        return MTLCompareFunctionAlways;
    }

    MTLStencilOperation stencilOperation(ALMetalStencilOperation operation)
    {
        switch (operation)
        {
            case ALMetalStencilOperation::keep:
                return MTLStencilOperationKeep;
            case ALMetalStencilOperation::zero:
                return MTLStencilOperationZero;
            case ALMetalStencilOperation::replace:
                return MTLStencilOperationReplace;
            case ALMetalStencilOperation::increment_clamp:
                return MTLStencilOperationIncrementClamp;
            case ALMetalStencilOperation::decrement_clamp:
                return MTLStencilOperationDecrementClamp;
            case ALMetalStencilOperation::invert:
                return MTLStencilOperationInvert;
            case ALMetalStencilOperation::increment_wrap:
                return MTLStencilOperationIncrementWrap;
            case ALMetalStencilOperation::decrement_wrap:
                return MTLStencilOperationDecrementWrap;
        }
        return MTLStencilOperationKeep;
    }

    MTLStencilDescriptor* stencilDescriptor(
        const ALMetalStencilDescriptor& descriptor)
    {
        MTLStencilDescriptor* native_descriptor =
            [[MTLStencilDescriptor alloc] init];
        native_descriptor.stencilCompareFunction =
            compareFunction(descriptor.compare);
        native_descriptor.stencilFailureOperation =
            stencilOperation(descriptor.stencil_failure);
        native_descriptor.depthFailureOperation =
            stencilOperation(descriptor.depth_failure);
        native_descriptor.depthStencilPassOperation =
            stencilOperation(descriptor.depth_stencil_pass);
        native_descriptor.readMask = descriptor.read_mask;
        native_descriptor.writeMask = descriptor.write_mask;
        return native_descriptor;
    }

    MTLSamplerMinMagFilter samplerFilter(ALMetalSamplerFilter filter)
    {
        return filter == ALMetalSamplerFilter::nearest
            ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
    }

    MTLSamplerMipFilter samplerMipFilter(ALMetalSamplerMipFilter filter)
    {
        switch (filter)
        {
            case ALMetalSamplerMipFilter::none:    return MTLSamplerMipFilterNotMipmapped;
            case ALMetalSamplerMipFilter::nearest: return MTLSamplerMipFilterNearest;
            case ALMetalSamplerMipFilter::linear:  return MTLSamplerMipFilterLinear;
        }
        return MTLSamplerMipFilterNotMipmapped;
    }

    MTLSamplerAddressMode samplerAddressMode(ALMetalSamplerAddressMode mode)
    {
        switch (mode)
        {
            case ALMetalSamplerAddressMode::clamp_to_edge: return MTLSamplerAddressModeClampToEdge;
            case ALMetalSamplerAddressMode::repeat:        return MTLSamplerAddressModeRepeat;
            case ALMetalSamplerAddressMode::mirror_repeat: return MTLSamplerAddressModeMirrorRepeat;
            case ALMetalSamplerAddressMode::clamp_to_zero: return MTLSamplerAddressModeClampToZero;
        }
        return MTLSamplerAddressModeClampToEdge;
    }

    MTLPrimitiveType primitiveType(ALMetalPrimitiveType primitive)
    {
        switch (primitive)
        {
            case ALMetalPrimitiveType::point:          return MTLPrimitiveTypePoint;
            case ALMetalPrimitiveType::line:           return MTLPrimitiveTypeLine;
            case ALMetalPrimitiveType::line_strip:     return MTLPrimitiveTypeLineStrip;
            case ALMetalPrimitiveType::triangle:       return MTLPrimitiveTypeTriangle;
            case ALMetalPrimitiveType::triangle_strip: return MTLPrimitiveTypeTriangleStrip;
        }
        return MTLPrimitiveTypeTriangle;
    }

    MTLCullMode cullMode(ALMetalCullMode mode)
    {
        switch (mode)
        {
            case ALMetalCullMode::none:  return MTLCullModeNone;
            case ALMetalCullMode::front: return MTLCullModeFront;
            case ALMetalCullMode::back:  return MTLCullModeBack;
        }
        return MTLCullModeNone;
    }

    MTLWinding winding(ALMetalWinding value)
    {
        return value == ALMetalWinding::clockwise ? MTLWindingClockwise : MTLWindingCounterClockwise;
    }

    MTLTessellationPartitionMode tessellationPartitionMode(
        ALMetalTessellationPartitionMode mode)
    {
        switch (mode)
        {
            case ALMetalTessellationPartitionMode::power_of_two:
                return MTLTessellationPartitionModePow2;
            case ALMetalTessellationPartitionMode::integer:
                return MTLTessellationPartitionModeInteger;
            case ALMetalTessellationPartitionMode::fractional_odd:
                return MTLTessellationPartitionModeFractionalOdd;
            case ALMetalTessellationPartitionMode::fractional_even:
                return MTLTessellationPartitionModeFractionalEven;
        }
        return MTLTessellationPartitionModeInteger;
    }

    MTLTessellationFactorStepFunction tessellationFactorStepFunction(
        ALMetalTessellationFactorStepFunction function)
    {
        switch (function)
        {
            case ALMetalTessellationFactorStepFunction::constant:
                return MTLTessellationFactorStepFunctionConstant;
            case ALMetalTessellationFactorStepFunction::per_patch:
                return MTLTessellationFactorStepFunctionPerPatch;
            case ALMetalTessellationFactorStepFunction::per_instance:
                return MTLTessellationFactorStepFunctionPerInstance;
            case ALMetalTessellationFactorStepFunction::per_patch_and_instance:
                return MTLTessellationFactorStepFunctionPerPatchAndPerInstance;
        }
        return MTLTessellationFactorStepFunctionConstant;
    }

    MTLWinding tessellationOutputWinding(ALMetalTessellationOutputWinding value)
    {
        return value == ALMetalTessellationOutputWinding::clockwise
            ? MTLWindingClockwise : MTLWindingCounterClockwise;
    }

    bool validBindingIndex(std::uint32_t index, std::uint32_t limit,
                           const char* kind, std::string* error)
    {
        if (index < limit)
        {
            return true;
        }
        setError(error, std::string("Metal ") + kind + " binding index exceeds the API limit");
        return false;
    }
}

struct ALMetalShaderLibrary::Impl
{
    id<MTLDevice> device = nil;
    id<MTLLibrary> library = nil;
};

ALMetalShaderLibrary::ALMetalShaderLibrary(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalShaderLibrary::~ALMetalShaderLibrary() = default;

std::unique_ptr<ALMetalShaderLibrary> ALMetalShaderLibrary::loadBuiltin(
    const ALMetalDevice& device,
    std::string* error)
{
    @autoreleasepool
    {
        NSString* bundled = [[NSBundle mainBundle] pathForResource:@"almetal_builtin" ofType:@"metallib"];
        if (bundled.length != 0)
        {
            return loadFile(device, fromNSString(bundled), error);
        }

        NSString* bundled_source = [[NSBundle mainBundle] pathForResource:@"almetal_builtin" ofType:@"metal"];
        NSString* source_path = bundled_source.length != 0
            ? bundled_source : toNSString(AL_METAL_BUILTIN_SOURCE_PATH);
        NSError* read_error = nil;
        NSString* source = source_path.length == 0 ? nil
            : [NSString stringWithContentsOfFile:source_path
                                        encoding:NSUTF8StringEncoding
                                           error:&read_error];
        if (source)
        {
            return compileSource(device, fromNSString(source), "Alchemy built-in Metal library", error);
        }
        setError(error, read_error
            ? describeError(read_error)
            : "Could not locate almetal_builtin.metallib or almetal_builtin.metal");
        return nullptr;
    }
}

std::unique_ptr<ALMetalShaderLibrary> ALMetalShaderLibrary::loadFile(
    const ALMetalDevice& device,
    const std::string& path,
    std::string* error)
{
    @autoreleasepool
    {
        if (!device.nativeHandle() || path.empty())
        {
            setError(error, "Cannot load a Metal library without a device and path");
            return nullptr;
        }
        NSError* native_error = nil;
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device.nativeHandle();
        NSURL* url = [NSURL fileURLWithPath:toNSString(path)];
        id<MTLLibrary> library = [native_device newLibraryWithURL:url error:&native_error];
        if (!library)
        {
            setError(error, describeError(native_error));
            return nullptr;
        }
        auto impl = std::make_unique<Impl>();
        impl->device = native_device;
        impl->library = library;
        return std::unique_ptr<ALMetalShaderLibrary>(new ALMetalShaderLibrary(std::move(impl)));
    }
}

std::unique_ptr<ALMetalShaderLibrary> ALMetalShaderLibrary::compileSource(
    const ALMetalDevice& device,
    const std::string& source,
    const std::string& label,
    std::string* error)
{
    @autoreleasepool
    {
        if (!device.nativeHandle() || source.empty())
        {
            setError(error, "Cannot compile an empty Metal shader source");
            return nullptr;
        }
        MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
        options.mathMode = MTLMathModeFast;
        NSError* native_error = nil;
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device.nativeHandle();
        id<MTLLibrary> library = [native_device newLibraryWithSource:toNSString(source)
                                                             options:options
                                                               error:&native_error];
        if (!library)
        {
            setError(error, describeError(native_error));
            return nullptr;
        }
        library.label = toNSString(label);
        auto impl = std::make_unique<Impl>();
        impl->device = native_device;
        impl->library = library;
        return std::unique_ptr<ALMetalShaderLibrary>(new ALMetalShaderLibrary(std::move(impl)));
    }
}

void* ALMetalShaderLibrary::nativeHandle() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->library : nullptr;
}

bool ALMetalShaderLibrary::hasFunction(const std::string& name) const
{
    if (!mImpl || name.empty())
    {
        return false;
    }
    @autoreleasepool
    {
        return [mImpl->library newFunctionWithName:toNSString(name)] != nil;
    }
}

struct ALMetalRenderPipeline::Impl
{
    id<MTLDevice> device = nil;
    id<MTLRenderPipelineState> state = nil;
};

ALMetalRenderPipeline::ALMetalRenderPipeline(
    std::unique_ptr<Impl> impl,
    ALMetalRenderPipelineDescriptor descriptor)
: mImpl(std::move(impl)),
  mDescriptor(std::move(descriptor))
{
    // The compiled state is independent of the source library. Do not retain
    // a caller-owned C++ pointer in the diagnostic descriptor.
    mDescriptor.library = nullptr;
}

ALMetalRenderPipeline::~ALMetalRenderPipeline() = default;

std::unique_ptr<ALMetalRenderPipeline> ALMetalRenderPipeline::create(
    const ALMetalDevice& device,
    const ALMetalRenderPipelineDescriptor& descriptor,
    std::string* error)
{
    @autoreleasepool
    {
        if (!descriptor.library || !descriptor.library->mImpl ||
            descriptor.library->mImpl->device != (__bridge id<MTLDevice>)device.nativeHandle())
        {
            setError(error, "Metal render pipeline requires a shader library from the same device");
            return nullptr;
        }
        if (descriptor.vertex_function.empty() ||
            descriptor.color_attachment_count > AL_METAL_MAX_COLOR_ATTACHMENTS ||
            descriptor.sample_count == 0 ||
            (descriptor.tessellation.enabled &&
             (descriptor.tessellation.maximum_factor == 0 ||
              descriptor.tessellation.maximum_factor > 64)))
        {
            setError(error, "Metal render pipeline descriptor is incomplete");
            return nullptr;
        }
        id<MTLFunction> vertex = [descriptor.library->mImpl->library
            newFunctionWithName:toNSString(descriptor.vertex_function)];
        id<MTLFunction> fragment = descriptor.fragment_function.empty() ? nil
            : [descriptor.library->mImpl->library newFunctionWithName:toNSString(descriptor.fragment_function)];
        if (!vertex || (!descriptor.fragment_function.empty() && !fragment))
        {
            setError(error, "Metal render pipeline could not resolve its vertex or fragment function");
            return nullptr;
        }

        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device.nativeHandle();
        if (![native_device supportsTextureSampleCount:descriptor.sample_count])
        {
            setError(error, "Metal device does not support the render pipeline sample count");
            return nullptr;
        }

        MTLRenderPipelineDescriptor* native_descriptor = [[MTLRenderPipelineDescriptor alloc] init];
        native_descriptor.label = toNSString(descriptor.label);
        native_descriptor.vertexFunction = vertex;
        native_descriptor.fragmentFunction = fragment;
        native_descriptor.rasterSampleCount = descriptor.sample_count;
        if (descriptor.tessellation.enabled)
        {
            native_descriptor.tessellationPartitionMode =
                tessellationPartitionMode(descriptor.tessellation.partition_mode);
            native_descriptor.maxTessellationFactor = descriptor.tessellation.maximum_factor;
            native_descriptor.tessellationFactorScaleEnabled =
                descriptor.tessellation.factor_scale_enabled;
            native_descriptor.tessellationFactorFormat = MTLTessellationFactorFormatHalf;
            native_descriptor.tessellationControlPointIndexType =
                MTLTessellationControlPointIndexTypeNone;
            native_descriptor.tessellationFactorStepFunction =
                tessellationFactorStepFunction(descriptor.tessellation.factor_step_function);
            native_descriptor.tessellationOutputWindingOrder =
                tessellationOutputWinding(descriptor.tessellation.output_winding);
        }
        for (std::uint32_t index = 0; index < descriptor.color_attachment_count; ++index)
        {
            const auto& color = descriptor.colors[index];
            const MTLPixelFormat format = pixelFormat(color.format);
            if (format == MTLPixelFormatInvalid)
            {
                setError(error, "Metal render pipeline color attachment has an invalid format");
                return nullptr;
            }
            MTLRenderPipelineColorAttachmentDescriptor* native_color = native_descriptor.colorAttachments[index];
            native_color.pixelFormat = format;
            native_color.blendingEnabled = color.blending_enabled;
            native_color.sourceRGBBlendFactor = blendFactor(color.source_rgb);
            native_color.destinationRGBBlendFactor = blendFactor(color.destination_rgb);
            native_color.rgbBlendOperation = blendOperation(color.rgb_operation);
            native_color.sourceAlphaBlendFactor = blendFactor(color.source_alpha);
            native_color.destinationAlphaBlendFactor = blendFactor(color.destination_alpha);
            native_color.alphaBlendOperation = blendOperation(color.alpha_operation);
            native_color.writeMask = colorWriteMask(color.write_mask);
        }
        if (descriptor.depth_stencil_format != ALMetalPixelFormat::invalid)
        {
            native_descriptor.depthAttachmentPixelFormat = pixelFormat(descriptor.depth_stencil_format);
            if (descriptor.depth_stencil_format == ALMetalPixelFormat::depth32_float_stencil8)
            {
                native_descriptor.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
            }
        }

        NSError* native_error = nil;
        id<MTLRenderPipelineState> state = [native_device
            newRenderPipelineStateWithDescriptor:native_descriptor error:&native_error];
        if (!state)
        {
            setError(error, describeError(native_error));
            return nullptr;
        }
        auto impl = std::make_unique<Impl>();
        impl->device = native_device;
        impl->state = state;
        return std::unique_ptr<ALMetalRenderPipeline>(
            new ALMetalRenderPipeline(std::move(impl), descriptor));
    }
}

void* ALMetalRenderPipeline::nativeHandle() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->state : nullptr;
}

const ALMetalRenderPipelineDescriptor& ALMetalRenderPipeline::descriptor() const noexcept
{
    return mDescriptor;
}

struct ALMetalComputePipeline::Impl
{
    id<MTLDevice> device = nil;
    id<MTLComputePipelineState> state = nil;
};

ALMetalComputePipeline::ALMetalComputePipeline(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalComputePipeline::~ALMetalComputePipeline() = default;

std::unique_ptr<ALMetalComputePipeline> ALMetalComputePipeline::create(
    const ALMetalDevice& device,
    const ALMetalComputePipelineDescriptor& descriptor,
    std::string* error)
{
    @autoreleasepool
    {
        if (!descriptor.library || !descriptor.library->mImpl || descriptor.function.empty() ||
            descriptor.library->mImpl->device != (__bridge id<MTLDevice>)device.nativeHandle())
        {
            setError(error, "Metal compute pipeline requires a function and library from the same device");
            return nullptr;
        }
        id<MTLFunction> function = [descriptor.library->mImpl->library
            newFunctionWithName:toNSString(descriptor.function)];
        if (!function)
        {
            setError(error, "Metal compute pipeline could not resolve its function");
            return nullptr;
        }
        NSError* native_error = nil;
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device.nativeHandle();
        id<MTLComputePipelineState> state = [native_device
            newComputePipelineStateWithFunction:function error:&native_error];
        if (!state)
        {
            setError(error, describeError(native_error));
            return nullptr;
        }
        auto impl = std::make_unique<Impl>();
        impl->device = native_device;
        impl->state = state;
        return std::unique_ptr<ALMetalComputePipeline>(new ALMetalComputePipeline(std::move(impl)));
    }
}

void* ALMetalComputePipeline::nativeHandle() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->state : nullptr;
}

std::uint32_t ALMetalComputePipeline::threadExecutionWidth() const noexcept
{
    return mImpl ? static_cast<std::uint32_t>(mImpl->state.threadExecutionWidth) : 0;
}

std::uint32_t ALMetalComputePipeline::maximumTotalThreadsPerThreadgroup() const noexcept
{
    return mImpl ? static_cast<std::uint32_t>(mImpl->state.maxTotalThreadsPerThreadgroup) : 0;
}

struct ALMetalDepthStencilState::Impl
{
    id<MTLDevice> device = nil;
    id<MTLDepthStencilState> state = nil;
};

ALMetalDepthStencilState::ALMetalDepthStencilState(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalDepthStencilState::~ALMetalDepthStencilState() = default;

std::unique_ptr<ALMetalDepthStencilState> ALMetalDepthStencilState::create(
    const ALMetalDevice& device,
    const ALMetalDepthStencilDescriptor& descriptor,
    std::string* error)
{
    @autoreleasepool
    {
        if (!device.nativeHandle())
        {
            setError(error, "Cannot create Metal depth state without a device");
            return nullptr;
        }
        MTLDepthStencilDescriptor* native_descriptor = [[MTLDepthStencilDescriptor alloc] init];
        native_descriptor.label = toNSString(descriptor.label);
        native_descriptor.depthCompareFunction = compareFunction(descriptor.depth_compare);
        native_descriptor.depthWriteEnabled = descriptor.depth_write_enabled;
        if (descriptor.stencil_enabled)
        {
            native_descriptor.frontFaceStencil =
                stencilDescriptor(descriptor.front_stencil);
            native_descriptor.backFaceStencil =
                stencilDescriptor(descriptor.back_stencil);
        }
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device.nativeHandle();
        id<MTLDepthStencilState> state = [native_device newDepthStencilStateWithDescriptor:native_descriptor];
        if (!state)
        {
            setError(error, "Metal device could not create depth-stencil state");
            return nullptr;
        }
        auto impl = std::make_unique<Impl>();
        impl->device = native_device;
        impl->state = state;
        return std::unique_ptr<ALMetalDepthStencilState>(new ALMetalDepthStencilState(std::move(impl)));
    }
}

void* ALMetalDepthStencilState::nativeHandle() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->state : nullptr;
}

struct ALMetalSamplerState::Impl
{
    id<MTLDevice> device = nil;
    id<MTLSamplerState> state = nil;
};

ALMetalSamplerState::ALMetalSamplerState(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalSamplerState::~ALMetalSamplerState() = default;

std::unique_ptr<ALMetalSamplerState> ALMetalSamplerState::create(
    const ALMetalDevice& device,
    const ALMetalSamplerDescriptor& descriptor,
    std::string* error)
{
    @autoreleasepool
    {
        if (!device.nativeHandle() || descriptor.max_anisotropy == 0 || descriptor.max_anisotropy > 16)
        {
            setError(error, "Metal sampler requires a device and anisotropy in the range 1...16");
            return nullptr;
        }
        MTLSamplerDescriptor* native_descriptor = [[MTLSamplerDescriptor alloc] init];
        // MTLSamplerDescriptor rejects nil rather than treating it as an
        // unlabeled object. An empty public label is valid and intentionally
        // common for descriptors assembled from production GL sampler state.
        if (!descriptor.label.empty())
        {
            native_descriptor.label = toNSString(descriptor.label);
        }
        native_descriptor.minFilter = samplerFilter(descriptor.min_filter);
        native_descriptor.magFilter = samplerFilter(descriptor.mag_filter);
        native_descriptor.mipFilter = samplerMipFilter(descriptor.mip_filter);
        native_descriptor.sAddressMode = samplerAddressMode(descriptor.address_s);
        native_descriptor.tAddressMode = samplerAddressMode(descriptor.address_t);
        native_descriptor.rAddressMode = samplerAddressMode(descriptor.address_r);
        native_descriptor.maxAnisotropy = descriptor.max_anisotropy;
        native_descriptor.normalizedCoordinates = descriptor.normalized_coordinates;
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device.nativeHandle();
        id<MTLSamplerState> state = [native_device newSamplerStateWithDescriptor:native_descriptor];
        if (!state)
        {
            setError(error, "Metal device could not create sampler state");
            return nullptr;
        }
        auto impl = std::make_unique<Impl>();
        impl->device = native_device;
        impl->state = state;
        return std::unique_ptr<ALMetalSamplerState>(new ALMetalSamplerState(std::move(impl)));
    }
}

void* ALMetalSamplerState::nativeHandle() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->state : nullptr;
}

struct ALMetalRenderEncoder::Impl
{
    id<MTLDevice> device = nil;
    id<MTLRenderCommandEncoder> encoder = nil;
    bool ended = false;
    bool pipeline_set = false;
    bool tessellation_enabled = false;
    bool tessellation_factor_buffer_set = false;
};

ALMetalRenderEncoder::ALMetalRenderEncoder(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalRenderEncoder::~ALMetalRenderEncoder()
{
    if (mImpl && mImpl->encoder && !mImpl->ended)
    {
        [mImpl->encoder endEncoding];
        mImpl->ended = true;
    }
}

std::unique_ptr<ALMetalRenderEncoder> ALMetalRenderEncoder::begin(
    ALMetalCommandBuffer& command_buffer,
    const ALMetalRenderPass& render_pass,
    const std::string& label,
    std::string* error)
{
    @autoreleasepool
    {
        if (command_buffer.isCommitted() || !command_buffer.nativeHandle() || !render_pass.nativeHandle())
        {
            setError(error, "Metal render encoding requires an uncommitted command and render pass");
            return nullptr;
        }
        id<MTLCommandBuffer> command = (__bridge id<MTLCommandBuffer>)command_buffer.nativeHandle();
        id<MTLRenderCommandEncoder> encoder = [command
            renderCommandEncoderWithDescriptor:(__bridge MTLRenderPassDescriptor*)render_pass.nativeHandle()];
        if (!encoder)
        {
            setError(error, "Metal command buffer could not create a render encoder");
            return nullptr;
        }
        encoder.label = toNSString(label);
        auto impl = std::make_unique<Impl>();
        impl->device = (__bridge id<MTLDevice>)command_buffer.nativeDevice();
        impl->encoder = encoder;
        return std::unique_ptr<ALMetalRenderEncoder>(new ALMetalRenderEncoder(std::move(impl)));
    }
}

bool ALMetalRenderEncoder::setPipeline(const ALMetalRenderPipeline& pipeline, std::string* error)
{
    if (!mImpl || mImpl->ended || !pipeline.mImpl || pipeline.mImpl->device != mImpl->device)
    {
        setError(error, "Metal render pipeline is unavailable or belongs to another device");
        return false;
    }
    [mImpl->encoder setRenderPipelineState:pipeline.mImpl->state];
    mImpl->pipeline_set = true;
    mImpl->tessellation_enabled = pipeline.mDescriptor.tessellation.enabled;
    mImpl->tessellation_factor_buffer_set = false;
    return true;
}

bool ALMetalRenderEncoder::setDepthStencilState(const ALMetalDepthStencilState* state,
                                                std::string* error)
{
    if (!mImpl || mImpl->ended || (state && (!state->mImpl || state->mImpl->device != mImpl->device)))
    {
        setError(error, "Metal depth state is unavailable or belongs to another device");
        return false;
    }
    [mImpl->encoder setDepthStencilState:state ? state->mImpl->state : nil];
    return true;
}

bool ALMetalRenderEncoder::setStencilReferenceValue(
    std::uint32_t value, std::string* error)
{
    if (!mImpl || mImpl->ended)
    {
        setError(error, "Metal render encoder has ended");
        return false;
    }
    [mImpl->encoder setStencilReferenceValue:value];
    return true;
}

bool ALMetalRenderEncoder::setVertexBuffer(std::uint32_t index, const ALMetalBuffer& buffer,
                                           std::size_t offset, std::string* error)
{
    if (!mImpl || mImpl->ended || !validBindingIndex(index, 31, "vertex buffer", error) ||
        offset >= buffer.length())
    {
        if (offset >= buffer.length()) setError(error, "Metal vertex buffer offset is out of range");
        return false;
    }
    id<MTLBuffer> native_buffer = (__bridge id<MTLBuffer>)buffer.nativeHandle();
    if (!native_buffer || native_buffer.device != mImpl->device)
    {
        setError(error, "Metal vertex buffer belongs to another device");
        return false;
    }
    [mImpl->encoder setVertexBuffer:native_buffer offset:offset atIndex:index];
    return true;
}

bool ALMetalRenderEncoder::setFragmentBuffer(std::uint32_t index, const ALMetalBuffer& buffer,
                                             std::size_t offset, std::string* error)
{
    if (!mImpl || mImpl->ended || !validBindingIndex(index, 31, "fragment buffer", error) ||
        offset >= buffer.length())
    {
        if (offset >= buffer.length()) setError(error, "Metal fragment buffer offset is out of range");
        return false;
    }
    id<MTLBuffer> native_buffer = (__bridge id<MTLBuffer>)buffer.nativeHandle();
    if (!native_buffer || native_buffer.device != mImpl->device)
    {
        setError(error, "Metal fragment buffer belongs to another device");
        return false;
    }
    [mImpl->encoder setFragmentBuffer:native_buffer offset:offset atIndex:index];
    return true;
}

bool ALMetalRenderEncoder::setVertexBytes(std::uint32_t index, const void* bytes, std::size_t length,
                                          std::string* error)
{
    if (!mImpl || mImpl->ended || !bytes || length == 0 || length > 4096 ||
        !validBindingIndex(index, 31, "vertex bytes", error))
    {
        if (!bytes || length == 0 || length > 4096) setError(error, "Metal inline vertex data must be 1...4096 bytes");
        return false;
    }
    [mImpl->encoder setVertexBytes:bytes length:length atIndex:index];
    return true;
}

bool ALMetalRenderEncoder::setFragmentBytes(std::uint32_t index, const void* bytes, std::size_t length,
                                            std::string* error)
{
    if (!mImpl || mImpl->ended || !bytes || length == 0 || length > 4096 ||
        !validBindingIndex(index, 31, "fragment bytes", error))
    {
        if (!bytes || length == 0 || length > 4096) setError(error, "Metal inline fragment data must be 1...4096 bytes");
        return false;
    }
    [mImpl->encoder setFragmentBytes:bytes length:length atIndex:index];
    return true;
}

bool ALMetalRenderEncoder::setVertexTexture(std::uint32_t index, const ALMetalTexture* texture,
                                            std::string* error)
{
    if (!mImpl || mImpl->ended || !validBindingIndex(index, 128, "vertex texture", error)) return false;
    id<MTLTexture> native_texture = texture ? (__bridge id<MTLTexture>)texture->nativeHandle() : nil;
    if (native_texture && native_texture.device != mImpl->device)
    {
        setError(error, "Metal vertex texture belongs to another device");
        return false;
    }
    [mImpl->encoder setVertexTexture:native_texture atIndex:index];
    return true;
}

bool ALMetalRenderEncoder::setFragmentTexture(std::uint32_t index, const ALMetalTexture* texture,
                                              std::string* error)
{
    if (!mImpl || mImpl->ended || !validBindingIndex(index, 128, "fragment texture", error)) return false;
    id<MTLTexture> native_texture = texture ? (__bridge id<MTLTexture>)texture->nativeHandle() : nil;
    if (native_texture && native_texture.device != mImpl->device)
    {
        setError(error, "Metal fragment texture belongs to another device");
        return false;
    }
    [mImpl->encoder setFragmentTexture:native_texture atIndex:index];
    return true;
}

bool ALMetalRenderEncoder::setFragmentTextureLinear(std::uint32_t index,
                                                    const ALMetalTexture* texture,
                                                    std::string* error)
{
    if (!mImpl || mImpl->ended ||
        !validBindingIndex(index, 128, "fragment texture", error))
    {
        return false;
    }
    id<MTLTexture> native_texture = texture
        ? (__bridge id<MTLTexture>)texture->nativeLinearHandle() : nil;
    if (texture && !native_texture)
    {
        setError(error, "Metal could not provide the linear fragment texture view");
        return false;
    }
    if (native_texture && native_texture.device != mImpl->device)
    {
        setError(error, "Metal fragment texture belongs to another device");
        return false;
    }
    [mImpl->encoder setFragmentTexture:native_texture atIndex:index];
    return true;
}

bool ALMetalRenderEncoder::setVertexSampler(std::uint32_t index, const ALMetalSamplerState* sampler,
                                            std::string* error)
{
    if (!mImpl || mImpl->ended || !validBindingIndex(index, 16, "vertex sampler", error) ||
        (sampler && (!sampler->mImpl || sampler->mImpl->device != mImpl->device)))
    {
        if (sampler && (!sampler->mImpl || sampler->mImpl->device != mImpl->device))
            setError(error, "Metal vertex sampler belongs to another device");
        return false;
    }
    [mImpl->encoder setVertexSamplerState:sampler ? sampler->mImpl->state : nil atIndex:index];
    return true;
}

bool ALMetalRenderEncoder::setFragmentSampler(std::uint32_t index, const ALMetalSamplerState* sampler,
                                              std::string* error)
{
    if (!mImpl || mImpl->ended || !validBindingIndex(index, 16, "fragment sampler", error) ||
        (sampler && (!sampler->mImpl || sampler->mImpl->device != mImpl->device)))
    {
        if (sampler && (!sampler->mImpl || sampler->mImpl->device != mImpl->device))
            setError(error, "Metal fragment sampler belongs to another device");
        return false;
    }
    [mImpl->encoder setFragmentSamplerState:sampler ? sampler->mImpl->state : nil atIndex:index];
    return true;
}

bool ALMetalRenderEncoder::setViewport(const ALMetalViewport& viewport, std::string* error)
{
    if (!mImpl || mImpl->ended || viewport.width <= 0.0 || viewport.height <= 0.0 ||
        viewport.near_depth < 0.0 || viewport.far_depth > 1.0 || viewport.near_depth > viewport.far_depth)
    {
        setError(error, "Metal viewport has invalid dimensions or depth range");
        return false;
    }
    [mImpl->encoder setViewport:MTLViewport{viewport.x, viewport.y, viewport.width, viewport.height,
                                            viewport.near_depth, viewport.far_depth}];
    return true;
}

bool ALMetalRenderEncoder::setScissor(const ALMetalScissorRect& scissor, std::string* error)
{
    if (!mImpl || mImpl->ended || scissor.width == 0 || scissor.height == 0)
    {
        setError(error, "Metal scissor rectangle has zero area");
        return false;
    }
    [mImpl->encoder setScissorRect:MTLScissorRect{scissor.x, scissor.y, scissor.width, scissor.height}];
    return true;
}

bool ALMetalRenderEncoder::setCullMode(ALMetalCullMode mode, std::string* error)
{
    if (!mImpl || mImpl->ended)
    {
        setError(error, "Metal render encoder has ended");
        return false;
    }
    [mImpl->encoder setCullMode:cullMode(mode)];
    return true;
}

bool ALMetalRenderEncoder::setFrontFacingWinding(ALMetalWinding value, std::string* error)
{
    if (!mImpl || mImpl->ended)
    {
        setError(error, "Metal render encoder has ended");
        return false;
    }
    [mImpl->encoder setFrontFacingWinding:winding(value)];
    return true;
}

bool ALMetalRenderEncoder::setDepthClipMode(
    ALMetalDepthClipMode mode, std::string* error)
{
    if (!mImpl || mImpl->ended)
    {
        setError(error, "Metal render encoder has ended");
        return false;
    }
    [mImpl->encoder setDepthClipMode:
        mode == ALMetalDepthClipMode::clamp
            ? MTLDepthClipModeClamp : MTLDepthClipModeClip];
    return true;
}

bool ALMetalRenderEncoder::setDepthBias(float constant_bias,
                                        float slope_scale,
                                        float clamp,
                                        std::string* error)
{
    if (!mImpl || mImpl->ended || !std::isfinite(constant_bias) ||
        !std::isfinite(slope_scale) || !std::isfinite(clamp) || clamp < 0.f)
    {
        setError(error, "Metal depth bias is invalid or encoder has ended");
        return false;
    }
    [mImpl->encoder setDepthBias:constant_bias
                      slopeScale:slope_scale
                            clamp:clamp];
    return true;
}

bool ALMetalRenderEncoder::setTessellationFactorBuffer(const ALMetalBuffer& buffer,
                                                        std::size_t offset,
                                                        std::size_t instance_stride,
                                                        std::string* error)
{
    if (!mImpl || mImpl->ended || !mImpl->pipeline_set || !mImpl->tessellation_enabled ||
        offset >= buffer.length() || offset % 4 != 0 || instance_stride % 4 != 0)
    {
        setError(error, "Metal tessellation factors require an active tessellation pipeline and 4-byte-aligned range");
        return false;
    }
    id<MTLBuffer> native_buffer = (__bridge id<MTLBuffer>)buffer.nativeHandle();
    if (!native_buffer || native_buffer.device != mImpl->device)
    {
        setError(error, "Metal tessellation factor buffer belongs to another device");
        return false;
    }
    [mImpl->encoder setTessellationFactorBuffer:native_buffer
                                          offset:offset
                                  instanceStride:instance_stride];
    mImpl->tessellation_factor_buffer_set = true;
    return true;
}

bool ALMetalRenderEncoder::setTessellationFactorScale(float scale, std::string* error)
{
    if (!mImpl || mImpl->ended || !mImpl->pipeline_set || !mImpl->tessellation_enabled ||
        !std::isnormal(scale) || scale <= 0.f)
    {
        setError(error, "Metal tessellation factor scale requires an active pipeline and a positive normal value");
        return false;
    }
    [mImpl->encoder setTessellationFactorScale:scale];
    return true;
}

bool ALMetalRenderEncoder::draw(ALMetalPrimitiveType primitive, std::uint32_t vertex_start,
                                std::uint32_t vertex_count, std::uint32_t instance_count,
                                std::string* error)
{
    if (!mImpl || mImpl->ended || !mImpl->pipeline_set || mImpl->tessellation_enabled ||
        vertex_count == 0 || instance_count == 0)
    {
        setError(error, "Metal draw requires an active encoder, pipeline, vertices and instances");
        return false;
    }
    [mImpl->encoder drawPrimitives:primitiveType(primitive)
                       vertexStart:vertex_start
                       vertexCount:vertex_count
                     instanceCount:instance_count];
    return true;
}

bool ALMetalRenderEncoder::drawIndexed(ALMetalPrimitiveType primitive, std::uint32_t index_count,
                                       ALMetalIndexType index_type, const ALMetalBuffer& index_buffer,
                                       std::size_t index_buffer_offset, std::uint32_t instance_count,
                                       std::string* error)
{
    const std::size_t index_size = index_type == ALMetalIndexType::uint16 ? 2 : 4;
    if (!mImpl || mImpl->ended || !mImpl->pipeline_set || mImpl->tessellation_enabled ||
        index_count == 0 || instance_count == 0 ||
        index_buffer_offset > index_buffer.length() ||
        static_cast<std::size_t>(index_count) > (index_buffer.length() - index_buffer_offset) / index_size)
    {
        setError(error, "Metal indexed draw has invalid state or index range");
        return false;
    }
    id<MTLBuffer> native_buffer = (__bridge id<MTLBuffer>)index_buffer.nativeHandle();
    if (!native_buffer || native_buffer.device != mImpl->device)
    {
        setError(error, "Metal index buffer belongs to another device");
        return false;
    }
    [mImpl->encoder drawIndexedPrimitives:primitiveType(primitive)
                               indexCount:index_count
                                indexType:index_type == ALMetalIndexType::uint16 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
                              indexBuffer:native_buffer
                        indexBufferOffset:index_buffer_offset
                            instanceCount:instance_count];
    return true;
}

bool ALMetalRenderEncoder::drawPatches(std::uint32_t control_point_count,
                                       std::uint32_t patch_start,
                                       std::uint32_t patch_count,
                                       std::uint32_t instance_count,
                                       std::uint32_t base_instance,
                                       std::string* error)
{
    if (!mImpl || mImpl->ended || !mImpl->pipeline_set || !mImpl->tessellation_enabled ||
        !mImpl->tessellation_factor_buffer_set || control_point_count == 0 ||
        control_point_count > 32 || patch_count == 0 || instance_count == 0)
    {
        setError(error, "Metal patch draw has invalid tessellation state or dimensions");
        return false;
    }
    [mImpl->encoder drawPatches:control_point_count
                     patchStart:patch_start
                     patchCount:patch_count
               patchIndexBuffer:nil
         patchIndexBufferOffset:0
                  instanceCount:instance_count
                   baseInstance:base_instance];
    return true;
}

bool ALMetalRenderEncoder::end(std::string* error)
{
    if (!mImpl || mImpl->ended)
    {
        setError(error, "Metal render encoder already ended");
        return false;
    }
    [mImpl->encoder endEncoding];
    mImpl->ended = true;
    return true;
}

struct ALMetalComputeEncoder::Impl
{
    id<MTLDevice> device = nil;
    id<MTLComputeCommandEncoder> encoder = nil;
    id<MTLComputePipelineState> pipeline = nil;
    bool ended = false;
};

ALMetalComputeEncoder::ALMetalComputeEncoder(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalComputeEncoder::~ALMetalComputeEncoder()
{
    if (mImpl && mImpl->encoder && !mImpl->ended)
    {
        [mImpl->encoder endEncoding];
        mImpl->ended = true;
    }
}

std::unique_ptr<ALMetalComputeEncoder> ALMetalComputeEncoder::begin(
    ALMetalCommandBuffer& command_buffer,
    const std::string& label,
    std::string* error)
{
    @autoreleasepool
    {
        if (command_buffer.isCommitted() || !command_buffer.nativeHandle())
        {
            setError(error, "Metal compute encoding requires an uncommitted command buffer");
            return nullptr;
        }
        id<MTLCommandBuffer> command = (__bridge id<MTLCommandBuffer>)command_buffer.nativeHandle();
        id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
        if (!encoder)
        {
            setError(error, "Metal command buffer could not create a compute encoder");
            return nullptr;
        }
        encoder.label = toNSString(label);
        auto impl = std::make_unique<Impl>();
        impl->device = (__bridge id<MTLDevice>)command_buffer.nativeDevice();
        impl->encoder = encoder;
        return std::unique_ptr<ALMetalComputeEncoder>(new ALMetalComputeEncoder(std::move(impl)));
    }
}

bool ALMetalComputeEncoder::setPipeline(const ALMetalComputePipeline& pipeline, std::string* error)
{
    if (!mImpl || mImpl->ended || !pipeline.mImpl || pipeline.mImpl->device != mImpl->device)
    {
        setError(error, "Metal compute pipeline is unavailable or belongs to another device");
        return false;
    }
    [mImpl->encoder setComputePipelineState:pipeline.mImpl->state];
    mImpl->pipeline = pipeline.mImpl->state;
    return true;
}

bool ALMetalComputeEncoder::setBuffer(std::uint32_t index, const ALMetalBuffer& buffer,
                                      std::size_t offset, std::string* error)
{
    if (!mImpl || mImpl->ended || !validBindingIndex(index, 31, "compute buffer", error) ||
        offset >= buffer.length())
    {
        if (offset >= buffer.length()) setError(error, "Metal compute buffer offset is out of range");
        return false;
    }
    id<MTLBuffer> native_buffer = (__bridge id<MTLBuffer>)buffer.nativeHandle();
    if (!native_buffer || native_buffer.device != mImpl->device)
    {
        setError(error, "Metal compute buffer belongs to another device");
        return false;
    }
    [mImpl->encoder setBuffer:native_buffer offset:offset atIndex:index];
    return true;
}

bool ALMetalComputeEncoder::setBytes(std::uint32_t index, const void* bytes, std::size_t length,
                                     std::string* error)
{
    if (!mImpl || mImpl->ended || !bytes || length == 0 || length > 4096 ||
        !validBindingIndex(index, 31, "compute bytes", error))
    {
        if (!bytes || length == 0 || length > 4096) setError(error, "Metal inline compute data must be 1...4096 bytes");
        return false;
    }
    [mImpl->encoder setBytes:bytes length:length atIndex:index];
    return true;
}

bool ALMetalComputeEncoder::setTexture(std::uint32_t index, const ALMetalTexture* texture,
                                       std::string* error)
{
    if (!mImpl || mImpl->ended || !validBindingIndex(index, 128, "compute texture", error)) return false;
    id<MTLTexture> native_texture = texture ? (__bridge id<MTLTexture>)texture->nativeHandle() : nil;
    if (native_texture && native_texture.device != mImpl->device)
    {
        setError(error, "Metal compute texture belongs to another device");
        return false;
    }
    [mImpl->encoder setTexture:native_texture atIndex:index];
    return true;
}

bool ALMetalComputeEncoder::setSampler(std::uint32_t index, const ALMetalSamplerState* sampler,
                                       std::string* error)
{
    if (!mImpl || mImpl->ended || !validBindingIndex(index, 16, "compute sampler", error) ||
        (sampler && (!sampler->mImpl || sampler->mImpl->device != mImpl->device)))
    {
        if (sampler && (!sampler->mImpl || sampler->mImpl->device != mImpl->device))
            setError(error, "Metal compute sampler belongs to another device");
        return false;
    }
    [mImpl->encoder setSamplerState:sampler ? sampler->mImpl->state : nil atIndex:index];
    return true;
}

bool ALMetalComputeEncoder::dispatch(std::uint32_t width, std::uint32_t height, std::uint32_t depth,
                                     std::uint32_t threads_per_group_width,
                                     std::uint32_t threads_per_group_height,
                                     std::uint32_t threads_per_group_depth,
                                     std::string* error)
{
    if (!mImpl || mImpl->ended || !mImpl->pipeline ||
        width == 0 || height == 0 || depth == 0 ||
        threads_per_group_width == 0 || threads_per_group_height == 0 || threads_per_group_depth == 0)
    {
        setError(error, "Metal compute dispatch requires a pipeline and non-zero dimensions");
        return false;
    }
    const std::uint64_t total_threads = static_cast<std::uint64_t>(threads_per_group_width) *
                                        threads_per_group_height * threads_per_group_depth;
    if (total_threads > mImpl->pipeline.maxTotalThreadsPerThreadgroup)
    {
        setError(error, "Metal compute threadgroup exceeds the pipeline limit");
        return false;
    }
    [mImpl->encoder dispatchThreads:MTLSizeMake(width, height, depth)
              threadsPerThreadgroup:MTLSizeMake(threads_per_group_width,
                                                threads_per_group_height,
                                                threads_per_group_depth)];
    return true;
}

bool ALMetalComputeEncoder::end(std::string* error)
{
    if (!mImpl || mImpl->ended)
    {
        setError(error, "Metal compute encoder already ended");
        return false;
    }
    [mImpl->encoder endEncoding];
    mImpl->ended = true;
    return true;
}
