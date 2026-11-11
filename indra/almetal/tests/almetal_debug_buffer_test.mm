/**
 * @file almetal_debug_buffer_test.mm
 * @brief Physical GPU checks for Develop > Render Buffer's raw texture view.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "../almetaldevice.h"
#include "../almetalresource.h"
#include "../almetalworld.h"

#import <Metal/Metal.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
using Pixel = std::array<float, 4>;
constexpr std::array output_formats{
    ALMetalPixelFormat::rgba8_unorm, ALMetalPixelFormat::rgb10a2_unorm};

// The oracle is the original 1eb44bbd1b postDeferredVisualizeBuffers.glsl:
// textureLod -> RGBA output, with no display transfer, grading or tonemapping.
// Expectations below are authored texels, simple interpolants and the standard
// sRGB transfer function, not another copy of the native fragment shader.
float decodeSRGB(std::uint8_t encoded)
{
    const float value = encoded / 255.f;
    return value <= 0.04045f ? value / 12.92f
        : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;

    Fixture()
    {
        device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error);
        REQUIRE(renderer);
        auto data = ALMetalWorldFrameData::create(*device, &error);
        REQUIRE(data);
        data->beginFrame(1);
        renderer->setFrameData(std::move(data));
    }

    std::unique_ptr<ALMetalTexture> texture(ALMetalTextureDescriptor descriptor)
    {
        auto result = ALMetalTexture::create(*device, descriptor, &error);
        INFO(error);
        REQUIRE(result);
        return result;
    }

    void upload(ALMetalTexture& texture, const void* bytes,
                std::size_t row_bytes, std::uint32_t mip = 0)
    {
        auto command = device->makeCommandBuffer("Debug buffer authored input", &error);
        REQUIRE(command);
        REQUIRE(texture.upload(*command, mip, 0, {}, bytes, row_bytes, &error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);
    }

    std::unique_ptr<ALMetalRenderTarget> target(const ALMetalTexture& color)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = color.descriptor().width;
        descriptor.height = color.descriptor().height;
        descriptor.colors[0].format = color.descriptor().format;
        descriptor.colors[0].texture = &color;
        auto result = ALMetalRenderTarget::create(*device, descriptor, &error);
        INFO(error);
        REQUIRE(result);
        return result;
    }

    std::vector<std::uint32_t> words(const ALMetalRenderTarget& target)
    {
        std::vector<std::uint32_t> result(
            target.descriptor().width * target.descriptor().height);
        REQUIRE(target.colorTexture(0)->readback(*device, 0, 0, {}, result.data(),
            target.descriptor().width * sizeof(std::uint32_t), &error));
        INFO(error);
        return result;
    }

    std::vector<std::uint16_t> halfWords(const ALMetalRenderTarget& target)
    {
        std::vector<std::uint16_t> result(
            target.descriptor().width * target.descriptor().height * 4);
        REQUIRE(target.colorTexture(0)->readback(*device, 0, 0, {}, result.data(),
            target.descriptor().width * 4 * sizeof(std::uint16_t), &error));
        INFO(error);
        return result;
    }

    std::vector<std::uint32_t> render(const ALMetalWorldDebugBufferScene& scene)
    {
        auto result = renderer->renderDebugBuffer(scene, &error);
        INFO(error);
        REQUIRE(result);
        CHECK(result->descriptor().width == scene.width);
        CHECK(result->descriptor().height == scene.height);
        CHECK(result->descriptor().color_attachment_count == 1);
        CHECK(result->descriptor().colors[0].format == scene.output_format);
        std::vector<std::uint32_t> pixels(scene.width * scene.height);
        REQUIRE(result->colorTexture(0)->readback(*device, 0, 0, {},
            pixels.data(), scene.width * sizeof(std::uint32_t), &error));
        INFO(error);
        return pixels;
    }
};

void checkPixel(std::uint32_t packed, const Pixel& expected,
                ALMetalPixelFormat format, int rgb_tolerance = 1)
{
    for (std::uint32_t channel = 0; channel < 4; ++channel)
    {
        const bool ten_bit = format == ALMetalPixelFormat::rgb10a2_unorm;
        const auto maximum = ten_bit ? (channel == 3 ? 3u : 1023u) : 255u;
        const auto shift = channel * (ten_bit ? 10u : 8u);
        const auto actual = (packed >> shift) & maximum;
        const auto quantized = static_cast<int>(std::round(
            std::clamp(expected[channel], 0.f, 1.f) * maximum));
        CAPTURE(channel, actual, quantized);
        const int tolerance = channel < 3 ? rgb_tolerance
            : (ten_bit ? 0 : 1);
        CHECK(std::abs(static_cast<int>(actual) - quantized) <= tolerance);
    }
}

std::array<Pixel, 5> isolatedFilterControl(Fixture& fixture,
                                          const ALMetalTexture& source)
{
    // Isolate hardware filtering from the production fullscreen vertex stage,
    // fragment shader and normalized destination packing. Fixed UV samples
    // are written directly into a float buffer by a small independent kernel.
    const char* text = R"metal(
        #include <metal_stdlib>
        using namespace metal;
        kernel void sample_filter_control(texture2d<float> source [[texture(0)]],
            device float4* result [[buffer(0)]], uint index [[thread_position_in_grid]])
        {
            constexpr sampler bilinear(coord::normalized,
                address::mirrored_repeat, filter::linear, mip_filter::nearest);
            const float2 uv((float(index) + 0.5f) / 5.0f, 0.5f);
            result[index] = source.sample(bilinear, uv, level(0.0f));
        }
    )metal";
    id<MTLDevice> device = (__bridge id<MTLDevice>)fixture.device->nativeHandle();
    NSError* error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:
        [NSString stringWithUTF8String:text] options:nil error:&error];
    INFO((error ? error.localizedDescription.UTF8String : ""));
    REQUIRE(library);
    id<MTLFunction> function = [library newFunctionWithName:@"sample_filter_control"];
    REQUIRE(function);
    id<MTLComputePipelineState> pipeline = [device
        newComputePipelineStateWithFunction:function error:&error];
    INFO((error ? error.localizedDescription.UTF8String : ""));
    REQUIRE(pipeline);
    id<MTLBuffer> output = [device newBufferWithLength:5 * sizeof(Pixel)
        options:MTLResourceStorageModeShared];
    REQUIRE(output);
    auto command = fixture.device->makeCommandBuffer("Isolated filter precision control",
                                                     &fixture.error);
    REQUIRE(command);
    id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
    id<MTLComputeCommandEncoder> encoder = [native computeCommandEncoder];
    REQUIRE(encoder);
    [encoder setComputePipelineState:pipeline];
    [encoder setTexture:(__bridge id<MTLTexture>)source.nativeHandle() atIndex:0];
    [encoder setBuffer:output offset:0 atIndex:0];
    [encoder dispatchThreads:MTLSizeMake(5, 1, 1)
        threadsPerThreadgroup:MTLSizeMake(5, 1, 1)];
    [encoder endEncoding];
    REQUIRE(command->commitAndWait(&fixture.error));
    std::array<Pixel, 5> result;
    const auto* values = static_cast<const float*>(output.contents);
    for (std::size_t pixel = 0; pixel < result.size(); ++pixel)
        std::copy_n(values + pixel * 4, 4, result[pixel].begin());
    return result;
}
}

TEST_CASE("almetal: luminance debug owner retains the spatial sky and ground meter",
          "[almetal][debug-buffer][debug-owner]")
{
    @autoreleasepool
    {
        Fixture fixture;
        constexpr std::uint32_t width = 128, height = 72;
        ALMetalTextureDescriptor descriptor;
        descriptor.width = width;
        descriptor.height = height;
        descriptor.format = ALMetalPixelFormat::rgba16_float;
        descriptor.usage = ALMetalTextureUsage::shader_read |
                           ALMetalTextureUsage::render_target;
        auto source_texture = fixture.texture(descriptor);
        std::vector<std::uint16_t> authored(width * height * 4);
        std::vector<std::uint16_t> flags(width * height * 4, 0);
        for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x = 0; x < width; ++x)
        {
            const auto pixel = (y * width + x) * 4;
            const bool sky = x >= width / 2;
            for (std::size_t channel = 0; channel < 3; ++channel)
                authored[pixel + channel] = sky ? 0x4000 : 0x3400; // 2 or 0.25
            authored[pixel + 3] = 0x3c00;
            flags[pixel + 3] = sky ? 0 : 21845; // SKIP_ATMOS or HAS_ATMOS
        }
        fixture.upload(*source_texture, authored.data(), width * 8);
        auto source = fixture.target(*source_texture);
        descriptor.format = ALMetalPixelFormat::rgba16_unorm;
        auto normal = fixture.texture(descriptor);
        fixture.upload(*normal, flags.data(), width * 8);

        ALMetalWorldExposureScene scene;
        scene.source = source.get();
        scene.normal = normal.get();
        scene.delta_seconds = 1.f;
        scene.metering_mode = 0;
        auto ordinary = fixture.renderer->renderExposure(scene, &fixture.error);
        INFO(fixture.error);
        REQUIRE(ordinary);
        std::unique_ptr<ALMetalRenderTarget> retained;
        scene.meter_output = &retained;
        auto exposure = fixture.renderer->renderExposure(scene, &fixture.error);
        INFO(fixture.error);
        REQUIRE(exposure);
        REQUIRE(retained);
        CHECK(exposure->descriptor().width == 1);
        CHECK(exposure->descriptor().height == 1);
        CHECK(retained->descriptor().width == width);
        CHECK(retained->descriptor().height == height);
        CHECK(retained->descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
        CHECK(fixture.halfWords(*exposure) == fixture.halfWords(*ordinary));

        auto standalone = fixture.renderer->renderExposureMeter(
            *source, *normal, &fixture.error);
        INFO(fixture.error);
        REQUIRE(standalone);
        const auto pixels = fixture.halfWords(*retained);
        CHECK(pixels == fixture.halfWords(*standalone));
        // Original luminanceF: (mean ground luminance, mean sky luminance,
        // sky fraction, 1). Uniform monochrome patches make the luminance
        // analytic and expose accidentally publishing the adapted 1x1 EV.
        for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x = 0; x < width; ++x)
        {
            const auto pixel = (y * width + x) * 4;
            const bool sky = x >= width / 2;
            CAPTURE(x, y, sky);
            CHECK(pixels[pixel] == (sky ? 0 : 0x3400));
            CHECK(pixels[pixel + 1] == (sky ? 0x4000 : 0));
            CHECK(pixels[pixel + 2] == (sky ? 0x3c00 : 0));
            CHECK(pixels[pixel + 3] == 0x3c00);
        }

        scene.normal = nullptr;
        CHECK_FALSE(fixture.renderer->renderExposure(scene, &fixture.error));
        CHECK_FALSE(retained);
    }
}

TEST_CASE("almetal: SMAA debug intermediates publish only with the complete output",
          "[almetal][debug-buffer][debug-owner]")
{
    @autoreleasepool
    {
        Fixture fixture;
        constexpr std::uint32_t width = 16, height = 12;
        ALMetalTextureDescriptor descriptor;
        descriptor.width = width;
        descriptor.height = height;
        descriptor.usage = ALMetalTextureUsage::shader_read |
                           ALMetalTextureUsage::render_target;
        auto color = fixture.texture(descriptor);
        std::vector<std::uint8_t> authored(width * height * 4);
        for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x = 0; x < width; ++x)
        {
            const auto pixel = (y * width + x) * 4;
            const bool white = x > y;
            for (std::size_t channel = 0; channel < 3; ++channel)
                authored[pixel + channel] = white ? 255 : 0;
            authored[pixel + 3] = 255;
        }
        fixture.upload(*color, authored.data(), width * 4);
        auto source = fixture.target(*color);
        ALMetalWorldSMAAScene scene;
        scene.source = source.get();
        scene.quality = 2;
        auto ordinary = fixture.renderer->renderSMAA(scene, &fixture.error);
        INFO(fixture.error);
        REQUIRE(ordinary);

        ALMetalWorldSMAAEdgeScene edge_scene;
        edge_scene.source = source.get();
        edge_scene.quality = scene.quality;
        auto edges = fixture.renderer->renderSMAAEdges(edge_scene, &fixture.error);
        REQUIRE(edges);
        ALMetalWorldSMAAWeightScene weight_scene;
        weight_scene.edges = edges.get();
        weight_scene.quality = scene.quality;
        auto weights = fixture.renderer->renderSMAABlendWeights(weight_scene, &fixture.error);
        REQUIRE(weights);

        ALMetalWorldSMAAIntermediates retained;
        scene.intermediates = &retained;
        auto output = fixture.renderer->renderSMAA(scene, &fixture.error);
        INFO(fixture.error);
        REQUIRE(output);
        REQUIRE(retained.edges);
        REQUIRE(retained.weights);
        for (const auto* target : {retained.edges.get(), retained.weights.get()})
        {
            CHECK(target->descriptor().width == width);
            CHECK(target->descriptor().height == height);
            CHECK(target->descriptor().colors[0].format == ALMetalPixelFormat::rgba8_unorm);
        }
        const auto edge_pixels = fixture.words(*retained.edges);
        const auto weight_pixels = fixture.words(*retained.weights);
        CHECK(edge_pixels == fixture.words(*edges));
        CHECK(weight_pixels == fixture.words(*weights));
        CHECK(std::any_of(edge_pixels.begin(), edge_pixels.end(),
                          [](auto value) { return value != 0; }));
        CHECK(std::any_of(weight_pixels.begin(), weight_pixels.end(),
                          [](auto value) { return value != 0; }));
        CHECK(fixture.words(*output) == fixture.words(*ordinary));

        scene.quality = 4;
        CHECK_FALSE(fixture.renderer->renderSMAA(scene, &fixture.error));
        CHECK_FALSE(retained.edges);
        CHECK_FALSE(retained.weights);
    }
}

TEST_CASE("almetal: debug buffers preserve raw RGBA and source format semantics",
          "[almetal][debug-buffer]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALMetalTextureDescriptor descriptor;
        descriptor.width = descriptor.height = 2;
        // Distinct rows and alpha values expose image inversion and accidental
        // alpha replacement as well as decoding of an ordinary linear texture.
        const std::array<std::uint8_t, 16> rgba{{
            8, 64, 128, 0, 192, 32, 255, 85,
            255, 128, 11, 170, 16, 224, 48, 255}};
        for (const bool srgb : {false, true})
        {
            descriptor.format = srgb ? ALMetalPixelFormat::rgba8_unorm_srgb
                                     : ALMetalPixelFormat::rgba8_unorm;
            auto source = fixture.texture(descriptor);
            fixture.upload(*source, rgba.data(), 8);
            for (const auto output_format : output_formats)
            {
                CAPTURE(srgb, output_format);
                ALMetalWorldDebugBufferScene scene;
                scene.source = source.get();
                scene.width = scene.height = 2;
                scene.output_format = output_format;
                const auto pixels = fixture.render(scene);
                for (std::size_t pixel = 0; pixel < pixels.size(); ++pixel)
                {
                    Pixel expected;
                    for (std::size_t channel = 0; channel < 4; ++channel)
                        expected[channel] = srgb && channel < 3
                            ? decodeSRGB(rgba[pixel * 4 + channel])
                            : rgba[pixel * 4 + channel] / 255.f;
                    checkPixel(pixels[pixel], expected, output_format);
                }
            }
        }

        // A packed deferred normal attachment is an ordinary UNORM data
        // texture: do not normalize it, decode it as a normal, or apply sRGB.
        descriptor.format = ALMetalPixelFormat::rgba16_unorm;
        auto normals = fixture.texture(descriptor);
        const std::array<std::uint16_t, 16> raw_normals{{
            8192, 32768, 49151, 0, 65535, 4096, 16384, 21845,
            32768, 49151, 8192, 43690, 16384, 0, 65535, 65535}};
        fixture.upload(*normals, raw_normals.data(), 16);
        for (const auto output_format : output_formats)
        {
            ALMetalWorldDebugBufferScene scene;
            scene.source = normals.get();
            scene.width = scene.height = 2;
            scene.output_format = output_format;
            const auto pixels = fixture.render(scene);
            for (std::size_t pixel = 0; pixel < pixels.size(); ++pixel)
            {
                Pixel expected;
                for (std::size_t channel = 0; channel < 4; ++channel)
                    expected[channel] = raw_normals[pixel * 4 + channel] / 65535.f;
                checkPixel(pixels[pixel], expected, output_format);
            }
        }

        // Half-float -0.5, 0.25, 2.0, 1/3. The destination clips negative
        // and HDR values, just like the original normalized post target;
        // a tonemapper would visibly reduce the authored blue channel.
        descriptor.width = descriptor.height = 1;
        descriptor.format = ALMetalPixelFormat::rgba16_float;
        auto hdr = fixture.texture(descriptor);
        const std::array<std::uint16_t, 4> half{{0xb800, 0x3400, 0x4000, 0x3555}};
        fixture.upload(*hdr, half.data(), sizeof(half));
        for (const auto output_format : output_formats)
        {
            ALMetalWorldDebugBufferScene scene;
            scene.source = hdr.get();
            scene.width = scene.height = 1;
            scene.output_format = output_format;
            checkPixel(fixture.render(scene)[0], {0.f, 0.25f, 1.f, 1.f / 3.f},
                       output_format);
        }

        // Single-channel luminance/exposure is raw R, not a grayscale
        // conversion. Missing channels retain texture sampling defaults.
        descriptor.format = ALMetalPixelFormat::r32_float;
        auto luminance = fixture.texture(descriptor);
        const float meter = 0.375f;
        fixture.upload(*luminance, &meter, sizeof(meter));
        ALMetalWorldDebugBufferScene scene;
        scene.source = luminance.get();
        scene.width = scene.height = 1;
        scene.mip_level = 8.f; // The live exposure meter allocates only mip0.
        checkPixel(fixture.render(scene)[0], {meter, 0.f, 0.f, 1.f},
                   scene.output_format);
    }
}

TEST_CASE("almetal: debug buffer resampling decodes before bilinear filtering",
          "[almetal][debug-buffer]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALMetalTextureDescriptor descriptor;
        descriptor.width = 2;
        descriptor.format = ALMetalPixelFormat::rgba8_unorm_srgb;
        auto source = fixture.texture(descriptor);
        const std::array<std::uint8_t, 8> black_white{{0, 0, 0, 0, 255, 255, 255, 255}};
        fixture.upload(*source, black_white.data(), 8);
        const auto control = isolatedFilterControl(fixture, *source);
        const std::array values{0.f, 0.1f, 0.5f, 0.9f, 1.f};
        for (std::size_t pixel = 0; pixel < control.size(); ++pixel)
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CAPTURE(pixel, channel, control[pixel][channel]);
            // The M1 Max control is consistent with 1/256 filter weights
            // and, for RGB only, a 12-bit normalized linear component:
            // its midpoint is 2048/4095, while alpha is exactly 0.5.
            // Bound those two rounding steps independently. Endpoints and
            // midpoint alpha remain strict; these are observed precision
            // bounds, not universal Metal implementation guarantees.
            const float weight_rounding = pixel == 1 || pixel == 3
                ? 0.5f / 256.f : 0.f;
            const float component_rounding = channel < 3 && pixel > 0 && pixel < 4
                ? 0.5f / 4095.f : 0.f;
            const float tolerance = weight_rounding + component_rounding + 0.000001f;
            CHECK(std::abs(control[pixel][channel] - values[pixel]) <= tolerance);
        }
        for (const auto output_format : output_formats)
        {
            ALMetalWorldDebugBufferScene scene;
            scene.source = source.get();
            scene.width = 5;
            scene.height = 1;
            scene.output_format = output_format;
            const auto pixels = fixture.render(scene);
            // Full-screen normalized UVs sample between two authored texel
            // centers. Mirror repeats the edge texel for the outer samples;
            // ordinary repeat would contaminate both edges. This path cannot
            // distinguish mirror from clamp beyond [0,1] without changing its
            // original fixed full-screen UV contract.
            for (std::size_t pixel = 0; pixel < pixels.size(); ++pixel)
            {
                // Only fractional RGB10 samples need this bound: half a
                // 1/256 filtering step plus destination quantization is at
                // most three 10-bit codes. Authored texels, mip fixtures,
                // alpha, endpoints and midpoint retain their strict checks.
                const int tolerance = output_format == ALMetalPixelFormat::rgb10a2_unorm &&
                    (pixel == 1 || pixel == 3) ? 3 : 1;
                checkPixel(pixels[pixel], {values[pixel], values[pixel],
                           values[pixel], values[pixel]}, output_format, tolerance);
                // Agreement with the independent float-buffer control also
                // rules out a production UV or transfer-function error.
                checkPixel(pixels[pixel], control[pixel], output_format);
            }
        }
    }
}

TEST_CASE("almetal: debug buffer explicit mip clamps and selects nearest level",
          "[almetal][debug-buffer]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALMetalTextureDescriptor descriptor;
        descriptor.width = descriptor.height = 4;
        descriptor.mipmap_level_count = 3;
        auto source = fixture.texture(descriptor);
        const std::array<std::array<std::uint8_t, 4>, 3> colors{{
            {{255, 0, 0, 0}}, {{0, 255, 0, 85}}, {{0, 0, 255, 170}}}};
        for (std::uint32_t mip = 0; mip < 3; ++mip)
        {
            const auto size = 4u >> mip;
            std::vector<std::uint8_t> input(size * size * 4);
            for (std::size_t pixel = 0; pixel < size * size; ++pixel)
                std::copy(colors[mip].begin(), colors[mip].end(),
                          input.begin() + pixel * 4);
            fixture.upload(*source, input.data(), size * 4, mip);
        }
        struct Sample { float mip; std::size_t level; };
        const std::array<Sample, 7> samples{{
            {-8.f, 0}, {0.f, 0}, {0.25f, 0}, {0.75f, 1},
            {1.25f, 1}, {1.75f, 2}, {8.f, 2}}};
        for (const auto output_format : output_formats)
        for (const auto sample : samples)
        {
            CAPTURE(output_format, sample.mip);
            ALMetalWorldDebugBufferScene scene;
            scene.source = source.get();
            scene.width = 3;
            scene.height = 2;
            scene.mip_level = sample.mip;
            scene.output_format = output_format;
            Pixel expected;
            for (std::size_t channel = 0; channel < 4; ++channel)
                expected[channel] = colors[sample.level][channel] / 255.f;
            for (const auto pixel : fixture.render(scene))
                checkPixel(pixel, expected, output_format);
        }
    }
}

TEST_CASE("almetal: debug buffer rejects incompatible inputs without GPU submission",
          "[almetal][debug-buffer]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALMetalTextureDescriptor descriptor;
        auto source = fixture.texture(descriptor);
        ALMetalWorldDebugBufferScene valid;
        valid.source = source.get();
        valid.width = valid.height = 1;
        const auto reject = [&](const ALMetalWorldDebugBufferScene& scene)
        {
            const auto committed = ALMetalDevice::committedCommandBufferCount();
            fixture.error.clear();
            CHECK_FALSE(fixture.renderer->renderDebugBuffer(scene, &fixture.error));
            CHECK_FALSE(fixture.error.empty());
            CHECK(ALMetalDevice::committedCommandBufferCount() == committed);
        };
        auto scene = valid;
        scene.source = nullptr;
        reject(scene);
        scene = valid;
        scene.width = 0;
        reject(scene);
        scene = valid;
        scene.height = 0;
        reject(scene);
        for (const auto output_format : {ALMetalPixelFormat::invalid,
             ALMetalPixelFormat::rgba8_unorm_srgb, ALMetalPixelFormat::rgba16_float})
        {
            scene = valid;
            scene.output_format = output_format;
            reject(scene);
        }
        for (const auto mip : {std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity()})
        {
            scene = valid;
            scene.mip_level = mip;
            reject(scene);
        }
        for (const auto input_format : {ALMetalPixelFormat::rgba8_uint,
             ALMetalPixelFormat::rgba8_sint, ALMetalPixelFormat::depth32_float})
        {
            descriptor.format = input_format;
            auto incompatible = fixture.texture(descriptor);
            scene = valid;
            scene.source = incompatible.get();
            reject(scene);
        }
        descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        for (const auto type : {ALMetalTextureType::texture_cube,
                               ALMetalTextureType::texture_2d_array})
        {
            descriptor.type = type;
            auto incompatible = fixture.texture(descriptor);
            scene = valid;
            scene.source = incompatible.get();
            reject(scene);
        }
        descriptor.type = ALMetalTextureType::texture_2d;
        descriptor.usage = ALMetalTextureUsage::render_target;
        auto nonsampleable = fixture.texture(descriptor);
        scene = valid;
        scene.source = nonsampleable.get();
        reject(scene);
    }
}
