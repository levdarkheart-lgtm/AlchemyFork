/**
 * @file almetal_cutout_coverage_test.mm
 * @brief Original cutout far-depth pin and separate shared coverage behavior.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetalworld.h"
#include <Foundation/Foundation.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>

namespace
{
constexpr unsigned SIDE = 4;
using Pixel = std::array<std::uint16_t, 4>;
using Image = std::array<std::uint16_t, SIDE * SIDE * 4>;
constexpr std::array<double, 4> SOURCE{.25, .5, 1.5, .5};
std::uint16_t half(float value)
{
    const __fp16 converted = value;
    std::uint16_t bits;
    std::memcpy(&bits, &converted, sizeof(bits));
    return bits;
}
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    Fixture()
    {
        REQUIRE([[NSBundle mainBundle] pathForResource:@"almetal_builtin" ofType:@"metallib"] != nil);
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error);
        INFO(error); REQUIRE(renderer);
        auto frames = ALMetalWorldFrameData::create(*device, &error); REQUIRE(frames);
        frames->beginFrame(1);
        renderer->setFrameData(std::move(frames));
    }
    ~Fixture() { if (device) device->waitUntilIdle(); }
    std::unique_ptr<ALMetalRenderTarget> target(bool gbuffer, bool hdr, float raw_depth)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = descriptor.height = SIDE;
        if (gbuffer)
        {
            descriptor.color_attachment_count = 4;
            descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
            descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
            descriptor.colors[2].format = ALMetalPixelFormat::rgba16_unorm;
            descriptor.colors[3].format = hdr ? ALMetalPixelFormat::rg11b10_float
                                             : ALMetalPixelFormat::rgba8_unorm;
            descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
            descriptor.depth_stencil_shader_read = true;
        }
        else descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
        auto result = ALMetalRenderTarget::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result);
        ALMetalRenderPassConfig clear;
        clear.colors[0].clear = {SOURCE[0], SOURCE[1], SOURCE[2], SOURCE[3]};
        clear.clear_depth = raw_depth;
        clear.depth_store = clear.stencil_store = ALMetalStoreAction::store;
        auto command = device->makeCommandBuffer("Cutout authored source/depth", &error); REQUIRE(command);
        auto pass = result->makeRenderPass(clear, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Cutout input clear", &error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        return result;
    }
    Image pixels(const ALMetalRenderTarget& target)
    {
        REQUIRE(target.descriptor().width == SIDE); REQUIRE(target.descriptor().height == SIDE);
        REQUIRE(target.descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
        Image result{};
        REQUIRE(target.colorTexture(0)->readback(*device, 0, 0, {}, result.data(), SIDE * 8, &error));
        return result;
    }
    void checkDepth(const ALMetalRenderTarget& target, float expected)
    {
        REQUIRE(target.depthStencilTexture());
        std::array<float, SIDE * SIDE> actual{};
        REQUIRE(target.depthStencilTexture()->readback(*device, 0, 0, {}, actual.data(), SIDE * sizeof(float), &error));
        CHECK(std::all_of(actual.begin(), actual.end(), [expected](float d) { return d == expected; }));
    }
    void checkPixels(const Image& actual, Pixel expected)
    {
        unsigned errors = 0;
        for (unsigned i = 0; i < actual.size(); ++i)
            errors += actual[i] != expected[i % 4];
        INFO("First RGBA half words: " << actual[0] << ", " << actual[1] << ", " << actual[2] << ", " << actual[3]);
        CHECK(errors == 0);
    }
};
}

TEST_CASE("Native cutout coverage follows the original authored far-depth pin",
          "[almetal][cutout-depth-pin]")
{
    @autoreleasepool
    {
        Fixture f;
        // Original1eb drawFarPlaneQuad passes forward NDC0.99999f through
        // identity debugV, then GL's [-1,1] window conversion. Reverse-Z uses
        // GL_ZERO_TO_ONE and its separately authored0.000005f literal. Do not
        // derive the latter by subtracting the rounded forward value from1.
        constexpr float forward_pin = (0.99999f + 1.f) * .5f;
        constexpr float reverse_pin = 0.000005f;
        STATIC_REQUIRE(std::bit_cast<std::uint32_t>(forward_pin) == 0x3f7fffacu);
        STATIC_REQUIRE(std::bit_cast<std::uint32_t>(reverse_pin) == 0x36a7c5acu);
        for (bool hdr : {false, true}) for (bool reverse : {false, true})
        {
            const float pin = reverse ? reverse_pin : forward_pin;
            const std::array<float, 6> depths{
                .5f,
                std::nextafter(pin, reverse ? 1.f : 0.f),
                pin,
                std::nextafter(pin, reverse ? 0.f : 1.f),
                reverse ? .000003f : .999997f,
                reverse ? 0.f : 1.f,
            };
            for (float depth : depths)
            {
                CAPTURE(hdr, reverse, depth, pin);
                auto gbuffer = f.target(true, hdr, depth);
                auto composed = f.target(false, hdr, depth);
                f.checkDepth(*gbuffer, depth);
                ALMetalWorldOpaquePostDeferredScene scene;
                scene.gbuffer = gbuffer.get(); scene.composed_lighting = composed.get();
                scene.width = scene.height = SIDE; scene.hdr = hdr; scene.reverse_z = reverse;
                auto unseeded = f.renderer->renderOpaquePostDeferred(scene, &f.error);
                INFO(f.error); REQUIRE(unseeded);
                f.checkPixels(f.pixels(*unseeded), {half(.25f), half(.5f), half(1.5f), half(.5f)});
                scene.seed_cutout_coverage = true;
                auto seeded = f.renderer->renderOpaquePostDeferred(scene, &f.error);
                INFO(f.error); REQUIRE(seeded);
                // Original complementary depth functions: background <= (>=
                // reversed), opaque > (< reversed), evaluated at the pin.
                const bool opaque = reverse ? depth > pin : depth < pin;
                f.checkPixels(f.pixels(*seeded), opaque
                    ? Pixel{half(.25f), half(.5f), half(1.5f), half(1.f)}
                    : Pixel{});
                f.checkDepth(*seeded, depth);

                // A separate live opaque-coverage owner uses exact far0/1.
                // Keep that behavior: globally changing the shared coverage
                // vertex would incorrectly pass the cutout case alone.
                ALMetalWorldGBufferScene shared;
                shared.width = shared.height = SIDE;
                shared.normal_hdr = true; shared.hdr = hdr; shared.reverse_z = reverse;
                shared.stamp_opaque_coverage = true;
                REQUIRE(f.renderer->compositeImpostorPostDeferred(*gbuffer, shared, &f.error));
                std::array<std::uint8_t, SIDE * SIDE * 4> albedo{};
                REQUIRE(gbuffer->colorTexture(0)->readback(*f.device, 0, 0, {}, albedo.data(), SIDE * 4, &f.error));
                const bool shared_opaque = reverse ? depth > 0.f : depth < 1.f;
                for (unsigned p = 0; p < SIDE * SIDE; ++p)
                    CHECK(albedo[p * 4 + 3] == (shared_opaque ? 255 : 128));
                f.checkDepth(*gbuffer, depth);
            }
        }
    }
}
