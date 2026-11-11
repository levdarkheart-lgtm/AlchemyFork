/**
 * @file almetal_lens_policy_admission_test.mm
 * @brief Original non-HDR flare and flare-lit dirt on the actual native owners.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "../almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace
{
using Pixel = std::array<std::uint8_t, 4>;
struct LensFixture
{
    static constexpr unsigned WIDTH = 24, HEIGHT = 16;
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    LensFixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error);
        INFO(error); REQUIRE(renderer);
        auto arena = ALMetalWorldFrameData::create(*device, &error);
        REQUIRE(arena); arena->beginFrame(1);
        renderer->setFrameData(std::move(arena));
    }
    std::unique_ptr<ALMetalRenderTarget> source(Pixel value)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = WIDTH; descriptor.height = HEIGHT;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.label = "Non-HDR lens policy authored source";
        auto target = ALMetalRenderTarget::create(*device, descriptor, &error);
        REQUIRE(target);
        const std::vector<Pixel> pixels(WIDTH * HEIGHT, value);
        auto upload = device->makeCommandBuffer("Non-HDR lens source upload", &error);
        REQUIRE(upload);
        REQUIRE(const_cast<ALMetalTexture*>(target->colorTexture(0))->upload(
            *upload, 0, 0, {}, pixels.data(), WIDTH * sizeof(Pixel), &error));
        REQUIRE(upload->commitAndWait(&error));
        return target;
    }
    std::unique_ptr<ALMetalRenderTarget> floatSource(std::array<std::uint16_t, 4> value)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = WIDTH; descriptor.height = HEIGHT;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
        auto target = ALMetalRenderTarget::create(*device, descriptor, &error);
        REQUIRE(target);
        const std::vector<std::array<std::uint16_t, 4>> pixels(WIDTH * HEIGHT, value);
        auto upload = device->makeCommandBuffer("Linear lens source upload", &error);
        REQUIRE(upload);
        REQUIRE(const_cast<ALMetalTexture*>(target->colorTexture(0))->upload(
            *upload, 0, 0, {}, pixels.data(), WIDTH * 8, &error));
        REQUIRE(upload->commitAndWait(&error));
        return target;
    }
    struct WorldSky
    {
        std::unique_ptr<ALMetalRenderTarget> gbuffer, linear;
    };
    WorldSky worldSky(bool reverse)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = WIDTH; descriptor.height = HEIGHT;
        descriptor.color_attachment_count = 4;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
        descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.colors[2].format = ALMetalPixelFormat::rgb10a2_unorm;
        descriptor.colors[3].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        descriptor.depth_stencil_shader_read = true;
        WorldSky world;
        world.gbuffer = ALMetalRenderTarget::create(*device, descriptor, &error);
        REQUIRE(world.gbuffer);
        ALMetalRenderTargetDescriptor light_descriptor;
        light_descriptor.width = WIDTH; light_descriptor.height = HEIGHT;
        light_descriptor.colors[0].format = ALMetalPixelFormat::rg8_unorm;
        auto light = ALMetalRenderTarget::create(*device, light_descriptor, &error);
        REQUIRE(light);
        ALMetalTextureDescriptor environment_descriptor;
        environment_descriptor.type = ALMetalTextureType::texture_cube;
        environment_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        environment_descriptor.usage = ALMetalTextureUsage::shader_read;
        auto environment = ALMetalTexture::create(*device, environment_descriptor, &error);
        REQUIRE(environment);
        auto command = device->makeCommandBuffer("Non-HDR real world sky inputs", &error);
        REQUIRE(command);
        const Pixel black{0, 0, 0, 0};
        for (unsigned face = 0; face < 6; ++face)
            REQUIRE(environment->upload(*command, 0, face, {}, black.data(), 4, &error));
        ALMetalRenderPassConfig clear;
        clear.colors[0].clear = {1, 1, 1, 1};
        clear.colors[2].clear = {0, 0, 0, 0};
        clear.depth_store = ALMetalStoreAction::store;
        clear.stencil_store = ALMetalStoreAction::store;
        clear.clear_depth = reverse ? 0. : 1.;
        auto pass = world.gbuffer->makeRenderPass(clear, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "World sky G-buffer", &error);
        REQUIRE(encoder); REQUIRE(encoder->end(&error));
        pass = light->makeRenderPass(clear, &error); REQUIRE(pass);
        encoder = ALMetalRenderEncoder::begin(*command, *pass, "World sky light map", &error);
        REQUIRE(encoder); REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));
        ALMetalWorldBaseLightingScene base;
        base.gbuffer = world.gbuffer.get(); base.light_map = light->colorTexture(0);
        base.hdr = false; base.legacy_environment = environment.get();
        base.constants.environment_config[0] = 1;
        for (unsigned axis = 0; axis < 4; ++axis)
            base.constants.inverse_projection[axis * 4 + axis] = 1.f;
        base.constants.config[0] = reverse ? 1u : 0u;
        base.constants.sky[0] = 1; base.constants.sky[2] = 1;
        base.constants.sky_transfer[0] = 4.f;
        auto lit = renderer->renderBaseLighting(base, &error);
        INFO(error); REQUIRE(lit);
        REQUIRE(lit->descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
        ALMetalWorldOpaquePostDeferredScene forward;
        forward.gbuffer = world.gbuffer.get(); forward.composed_lighting = lit.get();
        forward.width = WIDTH; forward.height = HEIGHT;
        forward.hdr = false; forward.reverse_z = reverse;
        world.linear = renderer->renderOpaquePostDeferred(forward, &error);
        INFO(error); REQUIRE(world.linear);
        REQUIRE(world.linear->descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
        std::vector<std::array<std::uint16_t, 4>> pixels(WIDTH * HEIGHT);
        REQUIRE(world.linear->colorTexture(0)->readback(*device, 0, 0, {},
            pixels.data(), WIDTH * 8, &error));
        for (const auto& pixel : pixels)
        {
            CHECK(pixel[0] == 0x4400); CHECK(pixel[1] == 0x4400); CHECK(pixel[2] == 0x4400);
        }
        return world;
    }
    std::unique_ptr<ALMetalRenderTarget> skyDepth(bool reverse)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = WIDTH; descriptor.height = HEIGHT;
        descriptor.color_attachment_count = 0;
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        descriptor.depth_stencil_shader_read = true;
        auto target = ALMetalRenderTarget::create(*device, descriptor, &error);
        REQUIRE(target);
        ALMetalRenderPassConfig clear;
        clear.depth_load = ALMetalLoadAction::clear;
        clear.depth_store = ALMetalStoreAction::store;
        clear.clear_depth = reverse ? 0. : 1.;
        auto pass = target->makeRenderPass(clear, &error); REQUIRE(pass);
        auto command = device->makeCommandBuffer("Non-HDR lens sky depth", &error);
        REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Lens sky", &error);
        REQUIRE(encoder); REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));
        return target;
    }
    std::vector<Pixel> display(const ALMetalWorldDisplayPostScene& scene)
    {
        auto target = renderer->renderDisplayPost(scene, &error);
        INFO(error); REQUIRE(target);
        REQUIRE(target->descriptor().width == WIDTH);
        REQUIRE(target->descriptor().height == HEIGHT);
        REQUIRE(target->descriptor().colors[0].format == ALMetalPixelFormat::rgba8_unorm);
        std::vector<Pixel> pixels(WIDTH * HEIGHT);
        REQUIRE(target->colorTexture(0)->readback(*device, 0, 0, {}, pixels.data(),
                                                WIDTH * sizeof(Pixel), &error));
        return pixels;
    }
};
}

TEST_CASE("almetal: non-HDR viewer lens inputs retain flare dirt and source alpha",
          "[almetal][native-lens-policy]")
{
    @autoreleasepool
    {
        LensFixture f;
        auto bright = f.source({255, 255, 255, 255});
        auto dark = f.floatSource({0, 0, 0, 0x3404});
        ALMetalWorldLensDirtScene plate;
        plate.width = LensFixture::WIDTH; plate.height = LensFixture::HEIGHT;
        plate.seed = 19.f; plate.grime = 1.1f; plate.mote_scale = 1.25f;
        plate.smudge = .8f; plate.scratches = 7; plate.toe = 1.4f; plate.gain = 1.2f;
        auto dirt = f.renderer->renderLensDirt(plate, &f.error);
        INFO(f.error); REQUIRE(dirt);
        for (const bool reverse : {false, true})
        {
            CAPTURE(reverse);
            auto depth = f.skyDepth(reverse);
            ALMetalWorldLensFlareStateScene state;
            state.source = bright.get(); state.depth_texture = depth->depthStencilTexture();
            state.delta_seconds = .25f; state.edge_visibility = 1.f;
            state.occlusion_radius = .1f; state.fade_time = .35f; state.reverse_z = reverse;
            // RGBA8 cannot exceed the original luminance gate2. This exact
            // black control catches any attempt to invent a lower threshold.
            auto normalized_state = f.renderer->renderLensFlareState(state, &f.error);
            REQUIRE(normalized_state);
            std::array<std::uint16_t, 8> normalized_pixels{};
            REQUIRE(normalized_state->colorTexture(0)->readback(*f.device, 0, 0, {},
                normalized_pixels.data(), sizeof(normalized_pixels), &f.error));
            CHECK(normalized_pixels[0] == 0); CHECK(normalized_pixels[1] == 0);
            CHECK(normalized_pixels[2] == 0);
            auto world = f.worldSky(reverse);
            state.source = world.linear.get();
            state.depth_texture = world.gbuffer->depthStencilTexture();
            auto history = f.renderer->renderLensFlareState(state, &f.error);
            INFO(f.error); REQUIRE(history);
            std::array<std::uint16_t, 8> history_pixels{};
            REQUIRE(history->colorTexture(0)->readback(*f.device, 0, 0, {},
                history_pixels.data(), sizeof(history_pixels), &f.error));
            // A positive finite half drive from the real non-HDR world is required;
            // no fabricated state texture can conceal a format admission bug.
            CHECK(history_pixels[0] > 0); CHECK(history_pixels[0] < 0x7c00u);

            ALMetalWorldDisplayPostScene scene;
            scene.source = dark.get(); scene.hdr = false;
            const auto baseline = f.display(scene);
            scene.lens_flare_state_texture = history->colorTexture(0);
            scene.lens_flare_strength = .25f;
            scene.lens_flare_streak_intensity = 0.f;
            scene.lens_flare_glow_radius = .5f; scene.lens_flare_glow_falloff = 1.f;
            const auto flared = f.display(scene);
            CHECK(std::count_if(flared.begin(), flared.end(),
                [](const Pixel& p) { return p[0] != 0 || p[1] != 0 || p[2] != 0; }) > 0);
            CHECK(flared != baseline);

            // Original colorCorrectF applies dirt to flare light even without
            // HDR bloom. A zero flare response is an independent negative control.
            scene.lens_dirt_texture = dirt->colorTexture(0);
            scene.lens_dirt_strength = 1.5f; scene.lens_dirt_flare_response = 0.f;
            CHECK(f.display(scene) == flared);
            scene.lens_dirt_flare_response = .75f;
            const auto dirty = f.display(scene);
            CHECK(dirty != flared);
            unsigned brighter = 0;
            for (unsigned pixel = 0; pixel < dirty.size(); ++pixel)
            {
                CHECK(baseline[pixel] == Pixel{0, 0, 0, 64});
                CHECK(flared[pixel][3] == 64); CHECK(dirty[pixel][3] == 64);
                for (unsigned c = 0; c < 3; ++c)
                {
                    CHECK(dirty[pixel][c] >= flared[pixel][c]);
                    brighter += dirty[pixel][c] > flared[pixel][c];
                }
            }
            CHECK(brighter > 0);
            // Viewer clean-plate/off policy sets these strengths to zero, as
            // exercised through the actual CPU method in allens_policy_test.
            scene.lens_flare_strength = 0.f; scene.lens_dirt_strength = 0.f;
            CHECK(f.display(scene) == baseline);
        }
    }
}
