/**
 * @file almetal_ssr_isolation_test.mm
 * @brief Native SSR history, original clear values and probe fallback.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
constexpr unsigned SIDE = 8;
using Halves = std::vector<std::uint16_t>;
using Color = std::array<double, 4>;
struct Camera
{
    std::array<float, 16> projection{}, inverse{};
    bool reverse;
    explicit Camera(bool reversed) : reverse(reversed)
    {
        // Authored finite camera input, matching LLMatrix4a::perspective
        // and al_reverse_z_transform's z'=(w-z)/2. This is neither an SSR
        // march nor its oracle, and needs no viewer/LLRender owner.
        const float near = .1f, far = 10.f, cotangent = 1.f / std::tan(.5f);
        const float a = reverse ? near / (far - near) : -(far + near) / (far - near);
        const float b = reverse ? far * near / (far - near) : -2.f * far * near / (far - near);
        projection[0] = projection[5] = cotangent;
        projection[10] = a; projection[11] = -1.f; projection[14] = b;
        inverse[0] = inverse[5] = 1.f / cotangent;
        inverse[11] = 1.f / b; inverse[14] = -1.f; inverse[15] = a / b;
    }
    float depth(float distance) const
    {
        const float ndc = (-distance * projection[10] + projection[14]) / distance;
        return reverse ? ndc : ndc * .5f + .5f;
    }
};
std::uint16_t half(float value)
{
    const __fp16 converted = value; std::uint16_t bits;
    std::memcpy(&bits, &converted, sizeof(bits)); return bits;
}
float real(std::uint16_t bits)
{
    __fp16 value; std::memcpy(&value, &bits, sizeof(value)); return value;
}
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalTexture> radiance, sh, light_map;
    ALMetalWorldReflectionProbes probes;

    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error);
        INFO(error); REQUIRE(renderer);
        auto arena = ALMetalWorldFrameData::create(*device, &error); REQUIRE(arena);
        arena->beginFrame(1); renderer->setFrameData(std::move(arena));
        ALMetalTextureDescriptor descriptor;
        descriptor.type = ALMetalTextureType::texture_cube_array;
        descriptor.width = descriptor.height = 1; descriptor.array_length = 1;
        descriptor.format = ALMetalPixelFormat::rgba16_float;
        radiance = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(radiance);
        descriptor.type = ALMetalTextureType::texture_2d;
        descriptor.width = 4; descriptor.height = 1;
        descriptor.format = ALMetalPixelFormat::rgba32_uint;
        sh = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(sh);
        descriptor.width = descriptor.height = SIDE;
        descriptor.format = ALMetalPixelFormat::rg8_unorm;
        light_map = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(light_map);
        const std::array<std::uint16_t, 4> blue{half(.125f), half(.375f), half(.75f), half(1.f)};
        std::array<std::uint32_t, 16> coefficients{};
        coefficients[0] = blue[0] | (std::uint32_t(blue[1]) << 16);
        coefficients[1] = blue[2];
        std::array<std::uint8_t, SIDE * SIDE * 2> lighting; lighting.fill(255);
        auto upload = device->makeCommandBuffer("SSR isolation blue default probe", &error); REQUIRE(upload);
        for (unsigned face = 0; face < 6; ++face)
            REQUIRE(radiance->upload(*upload, 0, face, {}, blue.data(), sizeof(blue), &error));
        REQUIRE(sh->upload(*upload, 0, 0, {}, coefficients.data(), sizeof(coefficients), &error));
        REQUIRE(light_map->upload(*upload, 0, 0, {}, lighting.data(), SIDE * 2, &error));
        REQUIRE(upload->commitAndWait(&error));
        probes.radiance_texture = radiance.get(); probes.irradiance_texture = sh.get();
        probes.constants.config[0] = 1.f;
        probes.constants.ref_sphere[0][3] = 100.f;
        probes.constants.ref_params[0][0] = probes.constants.ref_params[0][1] = 1.f;
        probes.constants.ref_params[0][2] = 1.f;
        probes.constants.ref_params[0][3] = -100.f;
        probes.constants.ref_index[0][1] = -1;
    }
    std::unique_ptr<ALMetalRenderTarget> target(
        ALMetalPixelFormat format, const Color& color, bool depth, float raw_depth)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = descriptor.height = SIDE;
        descriptor.colors[0].format = format;
        if (depth)
        {
            descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
            descriptor.depth_stencil_shader_read = true;
        }
        auto result = ALMetalRenderTarget::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result);
        ALMetalRenderPassConfig clear;
        clear.colors[0].clear = {color[0], color[1], color[2], color[3]};
        clear.clear_depth = raw_depth;
        clear.depth_store = clear.stencil_store = ALMetalStoreAction::store;
        auto command = device->makeCommandBuffer("SSR authored color and raw depth", &error); REQUIRE(command);
        auto pass = result->makeRenderPass(clear, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Authored planes", &error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        return result;
    }
    std::unique_ptr<ALMetalRenderTarget> gbuffer(bool pbr, bool hdr, const Camera& camera)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = descriptor.height = SIDE; descriptor.color_attachment_count = 4;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
        descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.colors[2].format = ALMetalPixelFormat::rgba16_unorm;
        descriptor.colors[3].format = hdr ? ALMetalPixelFormat::rg11b10_float
                                         : ALMetalPixelFormat::rgba8_unorm;
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        descriptor.depth_stencil_shader_read = true;
        auto result = ALMetalRenderTarget::create(*device, descriptor, &error); REQUIRE(result);
        ALMetalRenderPassConfig clear;
        clear.colors[0].clear = {.25, .25, .25, 0};
        clear.colors[1].clear = pbr ? ALMetalClearColor{1, 0, 1, 0} : ALMetalClearColor{1, 1, 1, 1};
        clear.colors[2].clear = {1, 1, pbr ? 32896. / 65535. : 1., pbr ? .67 : .34};
        clear.clear_depth = camera.depth(.5f);
        clear.depth_store = clear.stencil_store = ALMetalStoreAction::store;
        auto command = device->makeCommandBuffer("SSR isolation authored material", &error); REQUIRE(command);
        auto pass = result->makeRenderPass(clear, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Reflection receiver", &error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        return result;
    }
    ALMetalWorldBaseLightingScene scene(const ALMetalRenderTarget& gbuffer,
                                         bool hdr, const Camera& camera)
    {
        ALMetalWorldBaseLightingScene result;
        result.gbuffer = &gbuffer; result.light_map = light_map.get();
        result.reflection_probes = &probes; result.hdr = hdr;
        std::memcpy(result.constants.inverse_projection, camera.inverse.data(),
                    sizeof(result.constants.inverse_projection));
        result.constants.config[0] = camera.reverse;
        result.constants.light_direction[2] = 1.f;
        result.constants.surface[1] = 1.f;
        return result;
    }
    ALMetalWorldScreenSpaceReflections reflection(
        const ALMetalRenderTarget& history, const Camera& camera,
        const ALMetalWorldBaseLightingScene& base)
    {
        ALMetalWorldScreenSpaceReflections result;
        result.scene_color = history.colorTexture(0);
        result.scene_depth = history.colorTexture(1);
        std::copy(std::begin(base.constants.inverse_projection),
                  std::end(base.constants.inverse_projection), result.constants.inverse_projection);
        std::memcpy(result.constants.projection, camera.projection.data(),
                    sizeof(result.constants.projection));
        for (unsigned axis = 0; axis < 4; ++axis)
            result.constants.inverse_modelview_delta[axis * 5] = 1.f;
        result.constants.screen[0] = result.constants.screen[1] = result.constants.screen[2] = SIDE;
        result.constants.trace[0] = 4; result.constants.trace[1] = .1f;
        result.constants.trace[2] = .2f; result.constants.trace[3] = .001f;
        result.constants.sampling[0] = 1; result.constants.sampling[1] = 1.6f;
        result.constants.config[0] = camera.reverse; result.constants.config[1] = 1;
        return result;
    }
    Halves pixels(const ALMetalRenderTarget& target)
    {
        REQUIRE(target.descriptor().width == SIDE); REQUIRE(target.descriptor().height == SIDE);
        REQUIRE(target.descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
        Halves values(SIDE * SIDE * 4);
        REQUIRE(target.colorTexture(0)->readback(*device, 0, 0, {}, values.data(), SIDE * 8, &error));
        REQUIRE(std::all_of(values.begin(), values.end(), [](auto bits) { return std::isfinite(real(bits)); }));
        return values;
    }
    std::unique_ptr<ALMetalRenderTarget> render(const ALMetalWorldBaseLightingScene& scene)
    {
        auto result = renderer->renderBaseLighting(scene, &error); INFO(error); REQUIRE(result); return result;
    }
    void checkHistory(const ALMetalRenderTarget& history, bool hdr,
                      const Color& expected_color, float raw_depth)
    {
        REQUIRE(history.descriptor().color_attachment_count == 2);
        CHECK(history.descriptor().colors[0].format ==
              (hdr ? ALMetalPixelFormat::rg11b10_float : ALMetalPixelFormat::rgba8_unorm));
        CHECK(history.descriptor().colors[1].format == ALMetalPixelFormat::r32_float);
        std::array<float, SIDE * SIDE> depth{};
        REQUIRE(history.colorTexture(1)->readback(*device, 0, 0, {}, depth.data(), SIDE * sizeof(float), &error));
        for (float value : depth) CHECK(value == raw_depth);
        // Independent Metal attachment-store control, not a copied SSR
        // publisher: HDR preserves >1 in R11; SDR stores the RGBA8 boundary.
        auto control = target(history.descriptor().colors[0].format, expected_color, false, 0);
        std::array<std::uint32_t, SIDE * SIDE> actual{}, expected{};
        REQUIRE(history.colorTexture(0)->readback(*device, 0, 0, {}, actual.data(), SIDE * 4, &error));
        REQUIRE(control->colorTexture(0)->readback(*device, 0, 0, {}, expected.data(), SIDE * 4, &error));
        CHECK(actual == expected);
    }
};
unsigned colorHits(const Halves& baseline, const Halves& changed, unsigned channel)
{
    REQUIRE(baseline.size() == changed.size()); unsigned hits = 0;
    for (unsigned p = 0; p < baseline.size(); p += 4)
        hits += real(changed[p + channel]) > real(baseline[p + channel]) &&
                real(changed[p + 2]) < real(baseline[p + 2]);
    return hits;
}
}

TEST_CASE("Native SSR cleared history selects probes until a new ordinary pair publishes",
          "[almetal][ssr-isolation]")
{
    @autoreleasepool
    {
        Fixture f;
        for (bool hdr : {false, true}) for (bool reverse : {false, true})
        {
            CAPTURE(hdr, reverse);
            const Camera camera(reverse);
            const float raw_hit = camera.depth(.375f);
            const float raw_far = reverse ? 0.f : 1.f;
            const Color red{2, 0, 0, .25}, green{0, 2, 0, .75}, black{0, 0, 0, 0};
            auto ordinary_red = f.target(ALMetalPixelFormat::rgba16_float, red, true, raw_hit);
            auto empty_source = f.target(ALMetalPixelFormat::rgba16_float, black, true, raw_far);
            auto ordinary_green = f.target(ALMetalPixelFormat::rgba16_float, green, true, raw_hit);
            std::unique_ptr<ALMetalRenderTarget> history;
            REQUIRE(f.renderer->updateScreenSpaceReflectionHistory(*ordinary_red, hdr, history, &f.error));
            REQUIRE(history); f.checkHistory(*history, hdr, red, raw_hit);
            for (bool pbr : {false, true})
            {
                CAPTURE(pbr);
                auto gbuffer = f.gbuffer(pbr, hdr, camera);
                auto scene = f.scene(*gbuffer, hdr, camera);
                const auto probe_only = f.pixels(*f.render(scene));
                auto ssr = f.reflection(*history, camera, scene);
                scene.screen_space_reflections = &ssr;
                const auto red_result = f.pixels(*f.render(scene));
                CHECK(colorHits(probe_only, red_result, 0) > 0);
                // Already-submitted native work retains its input resources
                // after the CPU history wrapper and its borrowed tuple retire.
                auto submitted_red = f.render(scene);
                history.reset();
                ssr = {};
                scene.screen_space_reflections = nullptr;

                // Original clearSceneMap: transparent black color and the
                // reverse-Z latch's far depth, before isolation becomes active.
                // This is an authored low-level clear/state control. It does
                // not execute ALRenderIsolation or the production clear body;
                // actual caller admission/clear is covered separately in CPU.
                REQUIRE(f.renderer->updateScreenSpaceReflectionHistory(*empty_source, hdr, history, &f.error));
                f.checkHistory(*history, hdr, black, raw_far);
                CHECK(f.pixels(*submitted_red) == red_result);
                ssr = f.reflection(*history, camera, scene);
                scene.screen_space_reflections = &ssr;
                CHECK(f.pixels(*f.render(scene)) == probe_only);
                // Exiting an isolated view alone publishes nothing. The next
                // ordinary render must not resurrect the old red generation.
                CHECK(f.pixels(*f.render(scene)) == probe_only);
                scene.screen_space_reflections = nullptr;
                CHECK(f.pixels(*f.render(scene)) == probe_only);
                CHECK(f.pixels(*f.render(scene)) == probe_only);

                REQUIRE(f.renderer->updateScreenSpaceReflectionHistory(*ordinary_green, hdr, history, &f.error));
                f.checkHistory(*history, hdr, green, raw_hit);
                ssr = f.reflection(*history, camera, scene);
                scene.screen_space_reflections = &ssr;
                const auto green_result = f.pixels(*f.render(scene));
                CHECK(colorHits(probe_only, green_result, 1) > 0);
                CHECK(green_result != red_result);
                // Restore the real red publication for the next material.
                REQUIRE(f.renderer->updateScreenSpaceReflectionHistory(*ordinary_red, hdr, history, &f.error));
            }
        }
    }
}
