/**
 * @file almetal_base_lighting_fastpath_test.mm
 * @brief Deferred fullbright early-out and its material/depth boundaries.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "../almetaldevice.h"
#include "../almetalpipeline.h"
#include "../almetalresource.h"
#include "../almetalworld.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>

namespace
{
    constexpr std::uint32_t size = 4;
    constexpr std::size_t pixel_count = size * size;
    constexpr std::array<std::uint8_t, 3> encoded_albedo{{255, 128, 64}};
    constexpr std::array<std::uint8_t, 4> environment_color{{32, 192, 64, 255}};

    float linear(float value)
    {
        return value <= 0.04045f ? value / 12.92f
            : std::pow((value + 0.055f) / 1.055f, 2.4f);
    }

    float halfValue(std::uint16_t bits)
    {
        const unsigned exponent = (bits >> 10) & 31u;
        const float magnitude = exponent == 0
            ? std::ldexp(static_cast<float>(bits & 1023u), -24)
            : std::ldexp(1.f + static_cast<float>(bits & 1023u) / 1024.f,
                         static_cast<int>(exponent) - 15);
        return (bits & 0x8000u) != 0 ? -magnitude : magnitude;
    }
}

TEST_CASE("almetal: base fullbright preserves linear output and exact material guards",
          "[almetal][base-light-fastpath][emissive-toggle]")
{
    for (const bool hdr : {false, true})
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error);
        REQUIRE(renderer);

        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = size;
        descriptor.height = size;
        descriptor.color_attachment_count = 4;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
        descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.colors[2].format = ALMetalPixelFormat::rgba16_unorm;
        descriptor.colors[3].format = hdr ? ALMetalPixelFormat::rg11b10_float
                                        : ALMetalPixelFormat::rgba8_unorm;
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        descriptor.depth_stencil_shader_read = true;
        descriptor.label = "Fullbright boundary G-buffer";
        std::array<std::unique_ptr<ALMetalTexture>, 4> colors;
        for (std::size_t slot = 0; slot < colors.size(); ++slot)
        {
            ALMetalTextureDescriptor color;
            color.width = size;
            color.height = size;
            color.format = descriptor.colors[slot].format;
            color.usage = ALMetalTextureUsage::shader_read |
                ALMetalTextureUsage::render_target;
            colors[slot] = ALMetalTexture::create(*device, color, &error);
            REQUIRE(colors[slot]);
            descriptor.colors[slot].texture = colors[slot].get();
        }
        auto gbuffer = ALMetalRenderTarget::create(*device, descriptor, &error);
        INFO(error);
        REQUIRE(gbuffer);

        ALMetalTextureDescriptor environment_descriptor;
        environment_descriptor.type = ALMetalTextureType::texture_cube;
        environment_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        environment_descriptor.storage = ALMetalStorageMode::private_gpu;
        environment_descriptor.usage = ALMetalTextureUsage::shader_read;
        environment_descriptor.label = "Fullbright boundary legacy environment";
        auto environment = ALMetalTexture::create(*device, environment_descriptor, &error);
        REQUIRE(environment);
        ALMetalTextureDescriptor light_descriptor;
        light_descriptor.width = size;
        light_descriptor.height = size;
        light_descriptor.format = ALMetalPixelFormat::rg8_unorm;
        light_descriptor.storage = ALMetalStorageMode::private_gpu;
        light_descriptor.usage = ALMetalTextureUsage::shader_read;
        light_descriptor.label = "Fullbright boundary light map";
        auto light_map = ALMetalTexture::create(*device, light_descriptor, &error);
        REQUIRE(light_map);
        auto upload = device->makeCommandBuffer("Fullbright fixed inputs", &error);
        REQUIRE(upload);
        for (std::uint32_t face = 0; face < 6; ++face)
        {
            REQUIRE(environment->upload(*upload, 0, face, {},
                environment_color.data(), sizeof(environment_color), &error));
        }
        std::array<std::uint8_t, pixel_count * 2> light{};
        light.fill(255);
        REQUIRE(light_map->upload(*upload, 0, 0, {}, light.data(), size * 2, &error));
        REQUIRE(upload->commitAndWait(&error));

        ALMetalWorldBaseLightingScene scene;
        scene.gbuffer = gbuffer.get();
        scene.light_map = light_map.get();
        scene.legacy_environment = environment.get();
        scene.hdr = hdr;
        scene.constants.environment_config[0] = 1;
        scene.constants.light_direction[2] = 1.f;
        // With no ambient/sun and no gloss, the original legacy expression
        // is mix(0, decoded_albedo, alpha), followed by its environment mix.
        // PBR has zero material AO, so its IBL is dark and its independent
        // emissive channel supplies an analytic classification check.
        for (std::size_t axis = 0; axis < 4; ++axis)
            scene.constants.inverse_projection[axis * 4 + axis] = 1.f;

        struct Case
        {
            const char* name;
            std::uint8_t alpha;
            std::uint16_t environment;
            bool pbr;
            bool clear_depth;
        };
        const std::array<Case, 5> cases{{
            {"fully fullbright", 255, 0, false, false},
            {"alpha 254 is still shaded", 254, 0, false, false},
            {"positive environment survives fullbright", 255, 32768, false, false},
            {"PBR alpha one is material coverage", 255, 0, true, false},
            {"clear depth remains outside geometry", 255, 0, false, true},
        }};
        for (const bool reverse_z : {false, true})
        {
            for (const bool suppress_emissive : {false, true})
            for (const bool classic : {false, true})
            {
                for (const Case& sample : cases)
                {
                    CAPTURE(sample.name, hdr, reverse_z, classic, suppress_emissive);
                    ALMetalRenderPassConfig clear;
                    clear.clear_depth = sample.clear_depth
                        ? (reverse_z ? 0.0 : 1.0)
                        : (reverse_z ? 0.75 : 0.25);
                    clear.depth_store = ALMetalStoreAction::store;
                    clear.stencil_store = ALMetalStoreAction::store;
                    clear.colors[3].clear = {0.25, 0.5, 1.0, 0.0};
                    auto pass = gbuffer->makeRenderPass(clear, &error);
                    auto command = device->makeCommandBuffer("Fullbright boundary inputs", &error);
                    REQUIRE(pass);
                    REQUIRE(command);
                    auto encoder = ALMetalRenderEncoder::begin(*command, *pass,
                        "Fullbright boundary G-buffer clear", &error);
                    REQUIRE(encoder);
                    REQUIRE(encoder->end(&error));

                    std::array<std::uint8_t, pixel_count * 4> albedo{};
                    std::array<std::uint8_t, pixel_count * 4> specular{};
                    std::array<std::uint16_t, pixel_count * 4> normal{};
                    for (std::size_t pixel = 0; pixel < pixel_count; ++pixel)
                    {
                        for (std::size_t channel = 0; channel < 3; ++channel)
                            albedo[pixel * 4 + channel] = encoded_albedo[channel];
                        albedo[pixel * 4 + 3] = sample.alpha;
                        specular[pixel * 4 + 1] = sample.pbr ? 255 : 0;
                        normal[pixel * 4] = 32768;
                        normal[pixel * 4 + 1] = 32768;
                        normal[pixel * 4 + 2] = sample.environment;
                        normal[pixel * 4 + 3] = sample.pbr ? 43908 : 0;
                    }
                    REQUIRE(colors[0]->upload(*command, 0, 0, {},
                        albedo.data(), size * 4, &error));
                    REQUIRE(colors[1]->upload(*command, 0, 0, {},
                        specular.data(), size * 4, &error));
                    REQUIRE(colors[2]->upload(*command, 0, 0, {},
                        normal.data(), size * 8, &error));
                    REQUIRE(command->commitAndWait(&error));
                    scene.constants.config[0] = reverse_z ? 1 : 0;
                    scene.constants.config[1] = classic ? 1 : 0;
                    scene.constants.environment_config[1] = suppress_emissive ? 1u : 0u;
                    auto output = renderer->renderBaseLighting(scene, &error);
                    INFO(error);
                    REQUIRE(output);
                    std::array<std::uint16_t, pixel_count * 4> pixels{};
                    REQUIRE(output->colorTexture(0)->readback(*device, 0, 0, {},
                        pixels.data(), size * 8, &error));
                    CHECK(output->descriptor().colors[0].format ==
                          ALMetalPixelFormat::rgba16_float);
                    for (std::size_t pixel = 0; pixel < pixel_count; ++pixel)
                    {
                        for (std::size_t channel = 0; channel < 3; ++channel)
                        {
                            float expected = linear(encoded_albedo[channel] / 255.f) *
                                (sample.alpha / 255.f);
                            if (sample.environment != 0)
                            {
                                const float weight = sample.environment / 65535.f;
                                const float encoded = encoded_albedo[channel] / 255.f;
                                expected = linear(encoded * (1.f - weight) +
                                    environment_color[channel] / 255.f * 2.f * weight);
                            }
                            if (sample.pbr)
                            {
                                expected = suppress_emissive ? 0.f
                                    : std::array<float, 3>{{0.25f, 0.5f, 1.f}}[channel];
                                // The G-buffer emissive storage still follows
                                // HDR. Its UNORM8 input quantizes before the
                                // now-float Screen; do not clamp that Screen.
                                if (!hdr) expected = std::round(expected * 255.f) / 255.f;
                            }
                            expected *= classic ? 1.1f : 1.f;
                            if (sample.clear_depth) expected = 0.f;
                            const float actual = halfValue(pixels[pixel * 4 + channel]);
                            CHECK(std::abs(actual - expected) < 0.001f);
                        }
                        CHECK(pixels[pixel * 4 + 3] == 0);
                    }
                }
            }
        }
    }
}
