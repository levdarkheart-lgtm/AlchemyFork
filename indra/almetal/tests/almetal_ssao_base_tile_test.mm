/**
 * @file almetal_ssao_base_tile_test.mm
 * @brief Final SSAO blur/base-light boundary equivalence and complete cost.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetaldevice.h"
#include "../almetalpipeline.h"
#include "../almetalresource.h"
#include "../almetalworld.h"
#include "almetal_world_prepare_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdlib>

namespace
{
struct SSAOTileFixture
{
    Fixture f;
    std::shared_ptr<ALMetalTexture> environment, noise, cascades, probe_radiance, probe_sh;
    ALMetalWorldDirectionalShadows shadows;
    ALMetalWorldReflectionProbes probes;

    SSAOTileFixture()
    {
        ALMetalTextureDescriptor d;
        d.type = ALMetalTextureType::texture_cube;
        d.format = ALMetalPixelFormat::rgba8_unorm;
        d.usage = ALMetalTextureUsage::shader_read;
        environment = ALMetalTexture::create(*f.device, d, &f.error);
        REQUIRE(environment);
        // Match the native reflection-policy fixture: one cube per probe,
        // six face slices per cube, and four rgba32_uint SH texels per row.
        d.type = ALMetalTextureType::texture_cube_array;
        d.format = ALMetalPixelFormat::rgba16_float;
        d.array_length = 2;
        probe_radiance = ALMetalTexture::create(*f.device, d, &f.error);
        REQUIRE(probe_radiance);
        d.type = ALMetalTextureType::texture_2d;
        d.format = ALMetalPixelFormat::rgba32_uint;
        d.width = 4; d.height = 2; d.array_length = 1;
        probe_sh = ALMetalTexture::create(*f.device, d, &f.error);
        REQUIRE(probe_sh);
        d.format = ALMetalPixelFormat::rg16_float;
        d.width = d.height = 128;
        noise = ALMetalTexture::create(*f.device, d, &f.error);
        REQUIRE(noise);
        d.type = ALMetalTextureType::texture_2d_array;
        d.format = ALMetalPixelFormat::r32_float;
        d.width = d.height = 8;
        d.array_length = 4;
        cascades = ALMetalTexture::create(*f.device, d, &f.error);
        REQUIRE(cascades);
        auto upload = f.device->makeCommandBuffer("Main SSAO fixed inputs", &f.error);
        REQUIRE(upload);
        const std::array<std::uint8_t, 4> color{{64, 96, 160, 255}};
        for (unsigned face = 0; face < 6; ++face)
            REQUIRE(environment->upload(*upload, 0, face, {}, color.data(), 4, &f.error));
        // Exactly represented half RGB: default (.125,.375,.75), local
        // (.0625,1,.125). The first three packed half coefficients are L0;
        // all higher SH orders remain zero, as in the native probe fixtures.
        const std::array<std::array<std::uint16_t, 4>, 2> probe_colors{{
            {{0x3000, 0x3600, 0x3a00, 0x3c00}}, {{0x2c00, 0x3c00, 0x3000, 0x3c00}}}};
        std::array<std::uint32_t, 2 * 16> coefficients{};
        for (unsigned index = 0; index < 2; ++index)
        {
            const auto& pixel = probe_colors[index];
            for (unsigned face = 0; face < 6; ++face)
                REQUIRE(probe_radiance->upload(*upload, 0, index * 6 + face,
                    {}, pixel.data(), 8, &f.error));
            coefficients[index * 16] = pixel[0] | (std::uint32_t(pixel[1]) << 16);
            coefficients[index * 16 + 1] = pixel[2];
            probes.constants.ref_sphere[index][3] = 100.f;
            probes.constants.ref_params[index][0] = probes.constants.ref_params[index][1] =
                probes.constants.ref_params[index][2] = 1.f;
            probes.constants.ref_params[index][3] = -100.f;
            probes.constants.ref_index[index][0] = index;
            probes.constants.ref_index[index][1] = -1;
            probes.constants.ref_index[index][3] = index == 1 ? 1 : 0;
        }
        REQUIRE(probe_sh->upload(*upload, 0, 0, {}, coefficients.data(), 4 * 16, &f.error));
        probes.radiance_texture = probe_radiance.get();
        probes.irradiance_texture = probe_sh.get();
        for (unsigned axis = 0; axis < 3; ++axis)
            probes.constants.env_matrix[axis][axis] = 1.f;
        probes.constants.config[0] = 2.f;
        // Signed half-float unit vectors rotate the eight real SSAO taps.
        std::vector<std::uint16_t> random(128 * 128 * 2);
        for (unsigned y = 0; y < 128; ++y)
        for (unsigned x = 0; x < 128; ++x)
        {
            random[(y * 128 + x) * 2] = (x & 1) ? 0xbc00 : 0x3c00;
            random[(y * 128 + x) * 2 + 1] = 0;
        }
        REQUIRE(noise->upload(*upload, 0, 0, {}, random.data(), 128 * 4, &f.error));
        std::array<float, 64> shadow_depth{};
        for (unsigned y = 0; y < 8; ++y)
        for (unsigned x = 0; x < 8; ++x)
            shadow_depth[y * 8 + x] = (x + y) & 1 ? .25f : .75f;
        for (unsigned slice = 0; slice < 4; ++slice)
            REQUIRE(cascades->upload(*upload, 0, slice, {}, shadow_depth.data(),
                8 * sizeof(float), &f.error));
        REQUIRE(upload->commitAndWait(&f.error));
        shadows.depth_texture = cascades.get();
        for (auto& matrix : shadows.constants.shadow_matrix)
        {
            matrix[0] = matrix[5] = .35f;
            matrix[12] = matrix[13] = matrix[14] = .5f;
            matrix[15] = 1.f;
        }
        const float clips[4]{4, 8, 12, 16};
        std::copy_n(clips, 4, shadows.constants.shadow_clip);
        shadows.constants.resolution[0] = shadows.constants.resolution[1] = 8.f;
        shadows.constants.parameters[2] = 200.f;
        shadows.constants.config[0] = 4;
        shadows.constants.config[3] = 2;
    }

    static void projection(float* matrix, bool reverse)
    {
        identity(matrix);
        // Equivalent forward/reverse perspective reconstruction. A depth
        // discontinuity changes 1/w, exercising the bilateral plane test.
        matrix[10] = 0.f;
        matrix[11] = reverse ? 1.f : -.5f;
        matrix[14] = -1.f;
        matrix[15] = reverse ? .5f : 1.f;
    }

    std::unique_ptr<ALMetalRenderTarget> gbuffer(unsigned width, unsigned height,
        bool reverse, bool hdr, bool normal_hdr, unsigned family)
    {
        std::array<ALMetalWorldSimpleBatch, 2> batches;
        for (unsigned layer = 0; layer < batches.size(); ++layer)
        {
            auto geometry = f.geometry;
            const float scale = layer == 0 ? 1.2f : .48f;
            const float offset = layer == 0 ? 0.f : .28f;
            for (unsigned vertex = 0; vertex < 4; ++vertex)
            {
                geometry.positions[vertex][0] = geometry.positions[vertex][0] * scale + offset;
                geometry.positions[vertex][1] = geometry.positions[vertex][1] * scale;
                geometry.positions[vertex][2] = layer == 0 ? .3f : -.4f;
                geometry.normals[vertex][0] = layer == 0 ? 0.f : .4f;
                geometry.normals[vertex][2] = layer == 0 ? 1.f : .916515f;
            }
            batches[layer] = f.batch(family == 0 ? (layer == 0 ? 1 : 2) : family);
            batches[layer].vertices = f.buffer(&geometry, sizeof(geometry));
            if (reverse)
            {
                // Reverse vertices already use native [0,w] clip depth.
                batches[layer].modelview_projection[10] = -.5f;
                batches[layer].modelview_projection[14] = .5f;
            }
        }
        auto scene = f.scene();
        scene.width = width;
        scene.height = height;
        scene.batches = batches.data();
        scene.batch_count = batches.size();
        scene.reverse_z = reverse;
        scene.hdr = hdr;
        scene.normal_hdr = normal_hdr;
        // Uncovered pixels have the explicit HDRI sky marker. The separate
        // all-sky case below also checks a nonblack sky transfer.
        scene.clear_color[0] = scene.clear_color[1] = scene.clear_color[2] = 0.f;
        scene.clear_color[3] = 1.f;
        auto result = f.renderer->render(scene, &f.error);
        INFO(f.error);
        REQUIRE(result);
        return result;
    }

    ALMetalWorldLightMapScene lightMap(const ALMetalRenderTarget& input,
        bool reverse, bool normal_hdr, bool shadow = true)
    {
        ALMetalWorldLightMapScene s;
        s.gbuffer = &input;
        s.noise_texture = noise.get();
        if (shadow) s.directional_shadows = &shadows;
        projection(s.constants.inverse_projection, reverse);
        s.constants.light_direction[2] = 1.f;
        s.constants.ssao[0] = 24.f;
        s.constants.ssao[1] = 12.f;
        s.constants.ssao[2] = 4.f;
        s.constants.ssao[3] = .25f;
        for (unsigned tap = 0; tap < 4; ++tap)
        {
            const float offset = static_cast<float>(tap);
            s.constants.blur_taps[tap][0] = s.constants.blur_taps[tap][1] =
                std::exp(-offset * offset / 8.f);
            s.constants.blur_taps[tap][2] = offset;
        }
        s.constants.blur[0] = 1.5f;
        s.constants.blur[1] = .25f;
        s.constants.screen[0] = static_cast<float>(input.descriptor().width);
        s.constants.screen[1] = static_cast<float>(input.descriptor().height);
        s.constants.config[0] = reverse;
        s.constants.config[1] = shadow;
        s.constants.config[2] = 1;
        s.constants.config[3] = normal_hdr;
        shadows.constants.resolution[2] = s.constants.screen[0];
        shadows.constants.resolution[3] = s.constants.screen[1];
        shadows.constants.parameters[3] = s.constants.screen[1];
        shadows.constants.config[2] = reverse;
        return s;
    }

    ALMetalWorldBaseLightingScene base(const ALMetalRenderTarget& input,
        bool reverse, bool hdr, bool normal_hdr, bool shadow = true,
        bool probe_environment = false)
    {
        ALMetalWorldBaseLightingScene s;
        s.gbuffer = &input;
        if (probe_environment) s.reflection_probes = &probes;
        else s.legacy_environment = environment.get();
        s.hdr = hdr;
        projection(s.constants.inverse_projection, reverse);
        s.constants.light_direction[2] = 1.f;
        s.constants.environment_config[0] = !probe_environment;
        for (unsigned channel = 0; channel < 3; ++channel)
        {
            s.constants.atmos_sunlit[channel] = .7f;
            s.constants.atmos_sunlit_linear[channel] = .7f;
            s.constants.atmos_amblit[channel] = .3f;
            s.constants.ssao_effect[channel][channel] = 1.f;
        }
        s.constants.lighting[0] = s.constants.lighting[1] = 1.f;
        s.constants.lighting[2] = .5f;
        s.constants.lighting[3] = .25f;
        s.constants.surface[1] = normal_hdr ? 1.f : 0.f;
        s.constants.config[0] = reverse;
        s.constants.config[2] = shadow;
        s.constants.config[3] = 1;
        s.constants.sky[0] = 1;
        s.constants.sky[1] = hdr;
        s.carry_depth = s.borrow_depth = s.depth_stencil_shader_read = true;
        s.diagnostic_view.view_kind = 1;
        return s;
    }

    std::vector<std::uint16_t> read(const ALMetalRenderTarget& output)
    {
        const auto width = output.descriptor().width;
        std::vector<std::uint16_t> pixels(width * output.descriptor().height * 4);
        REQUIRE(output.colorTexture(0)->readback(*f.device, 0, 0, {},
            pixels.data(), width * 8, &f.error));
        return pixels;
    }

    std::vector<std::uint8_t> readMap(const ALMetalRenderTarget& output)
    {
        const auto width = output.descriptor().width;
        std::vector<std::uint8_t> pixels(width * output.descriptor().height * 2);
        REQUIRE(output.colorTexture(0)->readback(*f.device, 0, 0, {},
            pixels.data(), width * 2, &f.error));
        return pixels;
    }

    std::unique_ptr<ALMetalRenderTarget> reference(ALMetalWorldGBufferRenderer& renderer,
        const ALMetalWorldLightMapScene& lm, ALMetalWorldBaseLightingScene base)
    {
        if (base.light_map) return renderer.renderBaseLighting(base, &f.error);
        auto map = renderer.renderLightMap(lm, &f.error);
        INFO(f.error);
        REQUIRE(map);
        base.light_map = map->colorTexture(0);
        return renderer.renderBaseLighting(base, &f.error);
    }
};

void settleSSAO(const std::shared_ptr<const ALMetalCommandCompletion>& completion)
{
    REQUIRE(completion);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!completion->isComplete() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    REQUIRE(completion->isComplete());
}

double medianSSAO(std::vector<double> values)
{
    REQUIRE(!values.empty());
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() & 1 ? values[middle] : (values[middle - 1] + values[middle]) * .5;
}
}

TEST_CASE("almetal: main final SSAO tile blur preserves the complete RG8 lighting chain",
          "[almetal][main-ssao-fusion]")
{
    @autoreleasepool
    {
        SSAOTileFixture p;
        auto& f = p.f;
        REQUIRE(f.renderer->supportsMainSSAOFusion());
        struct Case
        {
            unsigned width, height, family;
            bool reverse, hdr, normal_hdr, classic, shadow;
            float blur, strength;
        };
        const std::array<Case, 7> cases{{
            {7, 5, 1, false, false, false, false, false, 1.5f, .5f},
            {31, 19, 1, true, true, true, true, true, 1.5f, .5f},
            {31, 19, 2, false, true, false, false, true, 1.5f, .5f},
            {64, 48, 2, true, false, true, true, false, 1.5f, .5f},
            {64, 48, 0, false, true, true, false, true, 2.25f, .85f},
            {64, 48, 0, true, true, false, false, true, 0.f, .25f},
            {1, 1, 2, false, false, true, false, false, 1.5f, .5f},
        }};
        for (const auto& c : cases)
        {
            CAPTURE(c.width, c.height, c.family, c.reverse, c.hdr,
                c.normal_hdr, c.classic, c.shadow, c.blur, c.strength);
            auto input = p.gbuffer(c.width, c.height, c.reverse, c.hdr, c.normal_hdr, c.family);
            auto lm = p.lightMap(*input, c.reverse, c.normal_hdr, c.shadow);
            auto base = p.base(*input, c.reverse, c.hdr, c.normal_hdr, c.shadow);
            lm.constants.blur[0] = c.blur;
            base.constants.config[1] = c.classic;
            base.constants.lighting[2] = c.strength;
            base.constants.environment_config[1] = c.family == 0;
            auto separate = p.reference(*f.renderer, lm, base);
            INFO(f.error);
            REQUIRE(separate);
            const auto expected = p.read(*separate);
            ALMetalMainSSAOLightingPath path = ALMetalMainSSAOLightingPath::none;
            auto fused = f.renderer->renderMainSSAOLighting(lm, base, &f.error, &path);
            INFO(f.error);
            REQUIRE(fused);
            CHECK(path == ALMetalMainSSAOLightingPath::memoryless_final_blur);
            CHECK(p.read(*fused) == expected);
            CHECK(fused->depthStencilTexture() == input->depthStencilTexture());
            CHECK(fused->descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
            for (std::size_t pixel = 0; pixel < expected.size() / 4; ++pixel)
                CHECK(expected[pixel * 4 + 3] == 0);
            if (c.width > 1)
                CHECK(std::any_of(expected.begin(), expected.end(),
                    [](auto value) { return (value & 0x7fffu) != 0; }));
        }

        // This is not a white/flat-input equivalence test: the retained
        // near/far geometry must generate AO, and the actual two-axis blur
        // must change at least one stored occlusion byte without changing R.
        auto input = p.gbuffer(64, 48, false, true, true, 0);
        auto lm = p.lightMap(*input, false, true);
        auto blurred = f.renderer->renderLightMap(lm, &f.error);
        REQUIRE(blurred);
        const auto blur_pixels = p.readMap(*blurred);
        lm.constants.blur[0] = 0.f;
        auto raw = f.renderer->renderLightMap(lm, &f.error);
        REQUIRE(raw);
        const auto raw_pixels = p.readMap(*raw);
        bool occlusion = false, changed = false;
        for (std::size_t pixel = 0; pixel < raw_pixels.size() / 2; ++pixel)
        {
            CHECK(raw_pixels[pixel * 2] == blur_pixels[pixel * 2]);
            occlusion = occlusion || raw_pixels[pixel * 2 + 1] < 255;
            changed = changed || raw_pixels[pixel * 2 + 1] != blur_pixels[pixel * 2 + 1];
        }
        CHECK(occlusion);
        CHECK(changed);
    }
}

TEST_CASE("almetal: final SSAO fusion retains sky alpha and separate input fallbacks",
          "[almetal][main-ssao-fusion]")
{
    @autoreleasepool
    {
        SSAOTileFixture p;
        auto& f = p.f;
        REQUIRE(f.renderer->supportsMainSSAOFusion());
        auto input = p.gbuffer(31, 19, false, true, true, 0);
        auto other = p.gbuffer(31, 19, false, true, true, 1);
        const auto lm = p.lightMap(*input, false, true);
        const auto base = p.base(*input, false, true, true);
        enum class Fallback { ssao_off, non_main, projection, reverse, shadow, normal, gbuffer };
        for (const auto reason : {Fallback::ssao_off, Fallback::non_main, Fallback::projection,
             Fallback::reverse, Fallback::shadow, Fallback::normal, Fallback::gbuffer})
        {
            CAPTURE(static_cast<unsigned>(reason));
            auto map_scene = lm;
            auto base_scene = base;
            switch (reason)
            {
            case Fallback::ssao_off: map_scene.constants.config[2] = base_scene.constants.config[3] = 0; break;
            case Fallback::non_main: base_scene.diagnostic_view.view_kind = 3; break;
            case Fallback::projection: base_scene.constants.inverse_projection[12] += .03125f; break;
            case Fallback::reverse: base_scene.constants.config[0] = 1; break;
            case Fallback::shadow: base_scene.constants.config[2] = 0; break;
            case Fallback::normal: base_scene.constants.surface[1] = 0.f; break;
            case Fallback::gbuffer: base_scene.gbuffer = other.get(); break;
            }
            auto reference = p.reference(*f.renderer, map_scene, base_scene);
            INFO(f.error);
            REQUIRE(reference);
            ALMetalMainSSAOLightingPath path = ALMetalMainSSAOLightingPath::none;
            auto result = f.renderer->renderMainSSAOLighting(map_scene, base_scene, &f.error, &path);
            INFO(f.error);
            REQUIRE(result);
            CHECK(path == ALMetalMainSSAOLightingPath::separate_passes);
            CHECK(p.read(*result) == p.read(*reference));
        }
        auto external_map = f.renderer->renderLightMap(lm, &f.error);
        REQUIRE(external_map);
        auto external_base = base;
        external_base.light_map = external_map->colorTexture(0);
        auto external_reference = f.renderer->renderBaseLighting(external_base, &f.error);
        REQUIRE(external_reference);
        ALMetalMainSSAOLightingPath path;
        // A caller-supplied final map is authoritative; the unused producer
        // description is not inspected or rebuilt in this existing route.
        auto external = f.renderer->renderMainSSAOLighting({}, external_base, &f.error, &path);
        REQUIRE(external);
        CHECK(path == ALMetalMainSSAOLightingPath::separate_passes);
        CHECK(p.read(*external) == p.read(*external_reference));

        for (unsigned malformed = 0; malformed < 4; ++malformed)
        {
            auto map_scene = lm;
            auto base_scene = base;
            if (malformed == 0) map_scene.noise_texture = nullptr;
            if (malformed == 1) map_scene.constants.screen[0] += 1.f;
            if (malformed == 2) base_scene.constants.lighting[2] = std::numeric_limits<float>::quiet_NaN();
            if (malformed == 3) base_scene.gbuffer = nullptr;
            path = ALMetalMainSSAOLightingPath::memoryless_final_blur;
            f.error.clear();
            CHECK_FALSE(f.renderer->renderMainSSAOLighting(map_scene, base_scene, &f.error, &path));
            CHECK(path == ALMetalMainSSAOLightingPath::none);
            CHECK_FALSE(f.error.empty());
        }

        for (const bool reverse : {false, true})
        for (const bool hdr : {false, true})
        {
            auto sky_input = p.gbuffer(7, 5, reverse, hdr, true, 1);
            ALMetalRenderPassConfig clear;
            clear.clear_depth = reverse ? 0.0 : 1.0;
            clear.depth_store = clear.stencil_store = ALMetalStoreAction::store;
            clear.colors[0].clear = {.125, .25, .5, 1};
            clear.colors[2].clear = {0, 0, 0, 1};
            clear.colors[3].clear = {.125, .25, .5, 0};
            auto pass = sky_input->makeRenderPass(clear, &f.error);
            auto command = f.device->makeCommandBuffer("Main SSAO explicit sky input", &f.error);
            REQUIRE(pass);
            REQUIRE(command);
            auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Main SSAO sky clear", &f.error);
            REQUIRE(encoder);
            REQUIRE(encoder->end(&f.error));
            REQUIRE(command->commitAndWait(&f.error));
            auto sky_lm = p.lightMap(*sky_input, reverse, true);
            auto sky_base = p.base(*sky_input, reverse, hdr, true);
            auto reference = p.reference(*f.renderer, sky_lm, sky_base);
            REQUIRE(reference);
            auto output = f.renderer->renderMainSSAOLighting(sky_lm, sky_base, &f.error, &path);
            INFO(f.error);
            REQUIRE(output);
            CHECK(path == ALMetalMainSSAOLightingPath::memoryless_final_blur);
            const auto pixels = p.read(*output);
            CHECK(pixels == p.read(*reference));
            CHECK((pixels[0] & 0x7fffu) != 0);
            for (std::size_t pixel = 0; pixel < pixels.size() / 4; ++pixel)
                CHECK(pixels[pixel * 4 + 3] == 0);
        }
    }
}

TEST_CASE("almetal: main SSAO tile lighting preserves active reflection-probe SH and radiance",
          "[almetal][main-ssao-fusion][main-ssao-probes]")
{
    @autoreleasepool
    {
        SSAOTileFixture p;
        auto& f = p.f;
        REQUIRE(f.renderer->supportsMainSSAOFusion());
        auto input = p.gbuffer(31, 19, true, true, true, 0);
        const auto lm = p.lightMap(*input, true, true);
        auto base = p.base(*input, true, true, true, true, true);
        base.constants.environment_config[1] = 1; // Expose lighting beneath PBR emissive.
        auto separate = p.reference(*f.renderer, lm, base);
        INFO(f.error); REQUIRE(separate);
        const auto expected = p.read(*separate);

        // Local index 1 is a containing manual sphere, reached from bucket
        // start 1 with no neighbor relay. Removing it must change actual RGB,
        // proving this is an active probe consumer rather than a default flag.
        auto default_only = p.probes;
        default_only.constants.config[0] = 1.f;
        auto default_base = base; default_base.reflection_probes = &default_only;
        auto default_output = p.reference(*f.renderer, lm, default_base);
        REQUIRE(default_output);
        const auto default_pixels = p.read(*default_output);
        std::size_t changed = 0;
        for (std::size_t pixel = 0; pixel < expected.size() / 4; ++pixel)
            changed += expected[pixel * 4] != default_pixels[pixel * 4] ||
                expected[pixel * 4 + 1] != default_pixels[pixel * 4 + 1] ||
                expected[pixel * 4 + 2] != default_pixels[pixel * 4 + 2];
        REQUIRE(changed > 0);
        ALMetalMainSSAOLightingPath path = ALMetalMainSSAOLightingPath::none;
        auto fused = f.renderer->renderMainSSAOLighting(lm, base, &f.error, &path);
        INFO(f.error); REQUIRE(fused);
        CHECK(path == ALMetalMainSSAOLightingPath::memoryless_final_blur);
        CHECK(p.read(*fused) == expected);
        CHECK(fused->depthStencilTexture() == input->depthStencilTexture());
    }
}

TEST_CASE("almetal: a failed combined SSAO chain cannot commit an encoded prefix",
          "[almetal][main-ssao-fusion][main-ssao-prefix]")
{
    @autoreleasepool
    {
        SSAOTileFixture p;
        auto& f = p.f;
        auto input = p.gbuffer(31, 19, false, true, true, 0);
        const auto light_map = p.lightMap(*input, false, true);
        const auto base = p.base(*input, false, true, true);
        for (const bool main_view : {false, true})
        {
            CAPTURE(main_view);
            auto invalid_base = base;
            invalid_base.diagnostic_view.view_kind = main_view ? 1 : 3;
            invalid_base.constants.surface[0] = 0.f;
            REQUIRE(f.renderer->beginCommandBatch("Rejected complete SSAO chain", &f.error));
            const auto committed = ALMetalDevice::committedCommandBufferCount();
            auto prefix = f.renderer->renderLightMap(light_map, &f.error);
            REQUIRE(prefix);
            ALMetalMainSSAOLightingPath path = ALMetalMainSSAOLightingPath::memoryless_final_blur;
            CHECK_FALSE(f.renderer->renderMainSSAOLighting(
                light_map, invalid_base, &f.error, &path));
            CHECK(path == ALMetalMainSSAOLightingPath::none);
            CHECK_FALSE(f.renderer->commitCommandBatch(&f.error));
            CHECK_FALSE(f.renderer->commandBatchActive());
            CHECK(ALMetalDevice::committedCommandBufferCount() == committed);
        }
    }
}

TEST_CASE("almetal: complete main SSAO separate and final-tile lighting costs",
          "[almetal][.main-ssao-fusion-cost]")
{
    const char* enabled = std::getenv("ALCHEMY_MAIN_SSAO_FUSION_COST");
    if (!enabled || std::string(enabled) != "1")
        SKIP("Set ALCHEMY_MAIN_SSAO_FUSION_COST=1 for the complete paired GPU cost gate");
    // A retained G-buffer frontier, not a vertical-only microbenchmark.
    // Each timed command contains raw SSAO/shadows, horizontal blur, final
    // blur and actual base lighting, with identical settings and outputs.
    // Geometry/upload are excluded equally. Target retirement is included;
    // parity readback is outside both the command and CPU timing intervals.
    for (const auto extent : {std::array<unsigned, 2>{{1280, 720}},
                              std::array<unsigned, 2>{{1920, 1080}}})
    for (const bool shadow : {false, true})
    for (const bool probe_environment : {false, true})
    @autoreleasepool
    {
        SSAOTileFixture p;
        auto& f = p.f;
        std::array<std::unique_ptr<ALMetalWorldGBufferRenderer>, 2> owners;
        std::array<std::shared_ptr<ALMetalWorldFrameData>, 2> data;
        for (unsigned mode = 0; mode < 2; ++mode)
        {
            data[mode] = ALMetalWorldFrameData::create(*f.device, &f.error);
            REQUIRE(data[mode]);
            owners[mode] = ALMetalWorldGBufferRenderer::create(
                *f.device, 1, f.renderer->shaderLibrary(), &f.error);
            REQUIRE(owners[mode]);
            owners[mode]->setFrameData(data[mode]);
            REQUIRE(owners[mode]->supportsMainSSAOFusion());
        }
        auto input = p.gbuffer(extent[0], extent[1], true, true, true, 0);
        const auto lm = p.lightMap(*input, true, true, shadow);
        const auto base = p.base(*input, true, true, true, shadow, probe_environment);
        const char* environment_mode = probe_environment ? "probe" : "legacy";
        std::array<std::vector<double>, 2> gpu_samples, cpu_samples, settle_samples;
        constexpr unsigned warmups = 2, measured = 8;
        for (unsigned sample = 0; sample < warmups + measured; ++sample)
        {
            std::array<std::vector<std::uint16_t>, 2> colors;
            for (unsigned order = 0; order < 2; ++order)
            {
                const unsigned mode = order ^ (sample & 1u);
                auto& renderer = *owners[mode];
                const auto resources_before = alMetalResourceAllocationStats();
                const auto started = std::chrono::steady_clock::now();
                data[mode]->beginFrame(sample + 1);
                REQUIRE(renderer.beginCommandBatch("Complete main SSAO lighting cost", &f.error));
                std::unique_ptr<ALMetalRenderTarget> output, map;
                if (mode)
                {
                    ALMetalMainSSAOLightingPath selected;
                    output = renderer.renderMainSSAOLighting(lm, base, &f.error, &selected);
                    INFO(f.error);
                    REQUIRE(output);
                    REQUIRE(selected == ALMetalMainSSAOLightingPath::memoryless_final_blur);
                }
                else
                {
                    map = renderer.renderLightMap(lm, &f.error);
                    REQUIRE(map);
                    auto view_base = base;
                    view_base.light_map = map->colorTexture(0);
                    output = renderer.renderBaseLighting(view_base, &f.error);
                    INFO(f.error);
                    REQUIRE(output);
                }
                // Keep a native parity reference, not a typed target-pool
                // reader. Both arms retire their CPU targets before settling.
                id<MTLTexture> parity_texture =
                    (__bridge id<MTLTexture>)output->colorTexture(0)->nativeHandle();
                auto completion = renderer.commandBatchCompletion();
                REQUIRE(renderer.commitCommandBatch(&f.error));
                output.reset();
                map.reset();
                const auto encoded = std::chrono::steady_clock::now();
                settleSSAO(completion);
                const auto settled = std::chrono::steady_clock::now();
                ALMetalCommandGPUTime gpu;
                REQUIRE(completion->gpuTime(gpu) == ALMetalCommandTimingStatus::ready);
                const auto resources_after = alMetalResourceAllocationStats();
                const double cpu_ms = std::chrono::duration<double, std::milli>(encoded - started).count();
                const double settled_ms = std::chrono::duration<double, std::milli>(settled - started).count();
                const double gpu_ms = gpu.elapsed_nanoseconds / 1e6;
                if (sample >= warmups)
                {
                    gpu_samples[mode].push_back(gpu_ms);
                    cpu_samples[mode].push_back(cpu_ms);
                    settle_samples[mode].push_back(settled_ms);
                }
                const auto width = extent[0], height = extent[1];
                const std::size_t row = (width * 8 + 255u) & ~std::size_t{255u};
                id<MTLDevice> native = (__bridge id<MTLDevice>)f.device->nativeHandle();
                id<MTLBuffer> pixels = [native newBufferWithLength:row * height options:MTLResourceStorageModeShared];
                REQUIRE(pixels);
                auto parity = [(__bridge id<MTLCommandQueue>)f.device->nativeCommandQueue() commandBuffer];
                auto blit = [parity blitCommandEncoder];
                REQUIRE(blit);
                [blit copyFromTexture:parity_texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
                    sourceSize:MTLSizeMake(width, height, 1) toBuffer:pixels destinationOffset:0
                    destinationBytesPerRow:row destinationBytesPerImage:row * height];
                [blit endEncoding];
                [parity commit];
                [parity waitUntilCompleted];
                REQUIRE(parity.status == MTLCommandBufferStatusCompleted);
                colors[mode].resize(width * height * 4);
                for (unsigned y = 0; y < height; ++y)
                    std::memcpy(colors[mode].data() + y * width * 4,
                        static_cast<const std::uint8_t*>(pixels.contents) + y * row, width * 8);
                std::cout << "main_ssao_lighting_cost width=" << width << " height=" << height
                    << " shadow=" << shadow << " environment=" << environment_mode
                    << " mode=" << mode << " sample=" << sample
                    << " phase=" << (sample == 0 ? "cold" : sample < warmups ? "warmup" : "warm")
                    << " cpu_encode_commit_retire_ms=" << cpu_ms << " settled_wall_ms=" << settled_ms
                    << " complete_gpu_ms=" << gpu_ms
                    << " expected_render_passes=" << (mode ? 3 : 4)
                    << " textures_created=" << resources_after.textures_created - resources_before.textures_created
                    << " live_wrapper_logical_texture_bytes=" << resources_after.texture_bytes_live << '\n';
            }
            CHECK(colors[0] == colors[1]);
        }
        const double old_gpu = medianSSAO(gpu_samples[0]), new_gpu = medianSSAO(gpu_samples[1]);
        const double old_cpu = medianSSAO(cpu_samples[0]), new_cpu = medianSSAO(cpu_samples[1]);
        const double old_settle = medianSSAO(settle_samples[0]), new_settle = medianSSAO(settle_samples[1]);
        std::cout << "main_ssao_lighting_cost_summary width=" << extent[0] << " height=" << extent[1]
            << " shadow=" << shadow << " environment=" << environment_mode << " samples=" << measured
            << " old_gpu_median_ms=" << old_gpu << " new_gpu_median_ms=" << new_gpu
            << " gpu_ratio=" << new_gpu / old_gpu
            << " old_cpu_median_ms=" << old_cpu << " new_cpu_median_ms=" << new_cpu
            << " old_settled_median_ms=" << old_settle << " new_settled_median_ms=" << new_settle
            << " measured_gpu_win=" << (new_gpu < old_gpu) << '\n';
        // This guard rejects >5% regression; passing it is not a speedup
        // claim. Report the measured sign and magnitude separately above.
        REQUIRE(old_gpu > 0.0);
        CHECK(new_gpu <= old_gpu * 1.05);
    }
}
