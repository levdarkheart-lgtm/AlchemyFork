/**
 * @file altexture_mip_parity_native_test.mm
 * @brief Real-device checks of shared asset mips and colour/data views.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llgl.h"
#include "../llimagegl.h"
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalpipeline.h"
#include "../../almetal/almetalresource.h"

#import <Metal/Metal.h>

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>

namespace
{
struct NativeImages
{
    ALRenderCapabilities previous_capabilities = gRenderCapabilities;
    ALMetalDevice* previous_device = LLImageGL::getMetalDevice();
    bool previous_authority = LLImageGL::isMetalTextureAuthority();
    std::unique_ptr<ALMetalDevice> device;
    std::string error;

    NativeImages()
    {
        device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        gRenderCapabilities.initializeMetal(device->info());
        LLImageGL::setMetalDevice(device.get());
        LLImageGL::setMetalTextureAuthority(true);
    }
    ~NativeImages()
    {
        LLImageGL::setMetalDevice(previous_device);
        LLImageGL::setMetalTextureAuthority(previous_authority);
        gRenderCapabilities = previous_capabilities;
    }
};

// The same fetched asset may be colour in one material and data in another.
// Original LLImageGL::generateMipmaps clears the sampler and sets SKIP_DECODE,
// but a real Apple M1 Max CGL control (immutable SRGB8_ALPHA8, sampler zero,
// glGenerateMipmap) produces 188 rather than 128 in the varying red channel.
// The actual reference, not its raw-byte comment, is the mip parity oracle.
// Colour/data sampling independently selects decoded or raw views below.
constexpr std::array<U8, 16> checker{{
      0, 128, 192,  32, 255, 128, 192, 224,
    255, 128, 192, 224,   0, 128, 192,  32,
}};
constexpr std::array<U8, 4> encoded_average{{128, 128, 192, 128}};
constexpr std::array<U8, 4> reference_srgb_mip{{188, 128, 192, 128}};

LLPointer<LLImageGL> makeImage(NativeImages& f, bool srgb)
{
    LLPointer<LLImageGL> image = new LLImageGL(2, 2, 4, true);
    image->setNeedsAlphaAndPickMask(false);
    if (!srgb) image->setExplicitFormat(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    REQUIRE(image->createMetalTexture(0, checker.data()));
    REQUIRE(image->getMetalTexture());
    CHECK(image->getMetalTexture()->descriptor().format ==
        (srgb ? ALMetalPixelFormat::rgba8_unorm_srgb
              : ALMetalPixelFormat::rgba8_unorm));
    CHECK(image->getMetalTexture()->descriptor().mipmap_level_count == 2);
    CHECK_FALSE(image->getHasGLTexture());
    CHECK(image->getTexName() == 0);
    return image;
}

std::array<U8, 4> mipBytes(NativeImages& f, LLImageGL& image, const char* label)
{
    std::array<U8, 4> result{};
    REQUIRE(image.getMetalTexture()->readback(
        *f.device, 1, 0, {}, result.data(), 4, &f.error));
    std::cout << label << " mip1 RGBA=" << unsigned(result[0]) << ','
              << unsigned(result[1]) << ',' << unsigned(result[2]) << ','
              << unsigned(result[3]) << '\n';
    return result;
}

void checkMip(const std::array<U8, 4>& actual,
              const std::array<U8, 4>& expected)
{
    for (std::size_t channel = 0; channel < actual.size(); ++channel)
    {
        CAPTURE(channel, unsigned(actual[channel]), unsigned(expected[channel]));
        CHECK(std::abs(int(actual[channel]) - int(expected[channel])) <= 1);
    }
}

float decode(float value)
{
    return value <= 0.04045f ? value / 12.92f
        : std::pow((value + 0.055f) / 1.055f, 2.4f);
}
}

TEST_CASE("Shared native asset generated mips match verified Apple GL behaviour",
          "[texture-mip-parity][almetal][native-generated]")
{
    @autoreleasepool
    {
        NativeImages f;
        const auto linear = makeImage(f, false);
        checkMip(mipBytes(f, *linear, "linear-storage control"), encoded_average);

        const auto srgb = makeImage(f, true);
        checkMip(mipBytes(f, *srgb, "production sRGB-storage"), reference_srgb_mip);

        // A direct raw-view control on another real LLImageGL allocation
        // proves that the distinguishable 128-byte alternative would change
        // the verified reference behaviour. It is not a proposed producer fix.
        const auto raw_view = makeImage(f, true);
        id<MTLTexture> raw = (__bridge id<MTLTexture>)
            raw_view->getMetalTexture()->nativeLinearHandle();
        REQUIRE(raw);
        CHECK(raw.pixelFormat == MTLPixelFormatRGBA8Unorm);
        auto command = f.device->makeCommandBuffer("Encoded mip raw-view control", &f.error);
        REQUIRE(command);
        id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
        id<MTLBlitCommandEncoder> blit = [native blitCommandEncoder];
        REQUIRE(blit);
        [blit generateMipmapsForTexture:raw];
        [blit endEncoding];
        REQUIRE(command->commitAndWait(&f.error));
        checkMip(mipBytes(f, *raw_view, "raw-view regeneration control"), encoded_average);
    }
}

TEST_CASE("Shared native asset colour and data views decode independently before filtering",
          "[texture-mip-parity][almetal][native-views]")
{
    @autoreleasepool
    {
        NativeImages f;
        LLPointer<LLImageGL> image = new LLImageGL(2, 2, 4, true);
        image->setNeedsAlphaAndPickMask(false);
        const std::array<LLImageGL::MetalMipLevel, 2> levels{{
            {checker.data(), 2, 2}, {encoded_average.data(), 1, 1},
        }};
        REQUIRE(image->createMetalTexture(0, levels));
        ALMetalTexture* texture = image->getMetalTexture();
        REQUIRE(texture);
        id<MTLTexture> srgb = (__bridge id<MTLTexture>)texture->nativeHandle();
        id<MTLTexture> raw = (__bridge id<MTLTexture>)texture->nativeLinearHandle();
        REQUIRE(srgb);
        REQUIRE(raw);
        CHECK(srgb.pixelFormat == MTLPixelFormatRGBA8Unorm_sRGB);
        CHECK(raw.pixelFormat == MTLPixelFormatRGBA8Unorm);

        // This kernel only observes the real API's sampler transfer and
        // view bytes; it does not implement a second production renderer.
        const std::string source = R"metal(
            #include <metal_stdlib>
            using namespace metal;
            kernel void observe_views(texture2d<float> colour [[texture(0)]],
                                      texture2d<float> data [[texture(1)]],
                                      device float4* result [[buffer(0)]])
            {
                constexpr sampler s(coord::normalized, address::clamp_to_edge,
                                    filter::linear, mip_filter::nearest);
                result[0] = colour.sample(s, float2(0.5), level(0.0));
                result[1] = data.sample(s, float2(0.5), level(0.0));
                result[2] = colour.read(uint2(0), 1);
                result[3] = data.read(uint2(0), 1);
            }
        )metal";
        auto library = ALMetalShaderLibrary::compileSource(
            *f.device, source, "Asset colour/data view observation", &f.error);
        INFO(f.error);
        REQUIRE(library);
        id<MTLLibrary> native_library = (__bridge id<MTLLibrary>)library->nativeHandle();
        id<MTLFunction> function = [native_library newFunctionWithName:@"observe_views"];
        REQUIRE(function);
        id<MTLDevice> device = (__bridge id<MTLDevice>)f.device->nativeHandle();
        NSError* error = nil;
        id<MTLComputePipelineState> pipeline =
            [device newComputePipelineStateWithFunction:function error:&error];
        INFO((error ? error.localizedDescription.UTF8String : ""));
        REQUIRE(pipeline);
        id<MTLBuffer> result = [device newBufferWithLength:16 * sizeof(float)
                                                 options:MTLResourceStorageModeShared];
        REQUIRE(result);
        auto command = f.device->makeCommandBuffer("Observe encoded asset views", &f.error);
        REQUIRE(command);
        id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
        id<MTLComputeCommandEncoder> encoder = [native computeCommandEncoder];
        REQUIRE(encoder);
        [encoder setComputePipelineState:pipeline];
        [encoder setTexture:srgb atIndex:0];
        [encoder setTexture:raw atIndex:1];
        [encoder setBuffer:result offset:0 atIndex:0];
        [encoder dispatchThreadgroups:MTLSizeMake(1, 1, 1)
               threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
        [encoder endEncoding];
        REQUIRE(command->commitAndWait(&f.error));
        std::array<std::array<float, 4>, 4> samples{};
        std::memcpy(samples.data(), result.contents, sizeof(samples));
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CAPTURE(channel);
            const float average = channel == 0 || channel == 3
                ? 0.5f : float(encoded_average[channel]) / 255.f;
            const float decoded = channel == 0 || channel == 3
                ? 0.5f : decode(average);
            CHECK(std::abs(samples[0][channel] - decoded) < 0.002f);
            CHECK(std::abs(samples[1][channel] - average) < 0.002f);
            const float byte = float(encoded_average[channel]) / 255.f;
            CHECK(std::abs(samples[2][channel] - (channel == 3 ? byte : decode(byte))) < 0.002f);
            CHECK(std::abs(samples[3][channel] - byte) < 0.002f);
        }
        CHECK(samples[0][0] > decode(samples[1][0]) + 0.25f);
    }
}
