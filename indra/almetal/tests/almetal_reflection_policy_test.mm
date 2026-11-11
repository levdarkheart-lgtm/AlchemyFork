/**
 * @file almetal_reflection_policy_test.mm
 * @brief Actual native probe consumers at the original default-only boundary.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace
{
constexpr unsigned SIDE = 8;
using HalfPixels = std::vector<std::uint16_t>;
using Color = std::array<float, 4>;
constexpr Color DEFAULT_COLOR{.125f, .375f, .75f, 1.f};
constexpr std::array<Color, 3> COLORS{{DEFAULT_COLOR,
    {.0625f, 1.f, .125f, 1.f}, {1.f, .0625f, .125f, 1.f}}};
float decode(std::uint16_t bits)
{
    __fp16 value; std::memcpy(&value, &bits, 2); return value;
}
std::uint16_t encode(float value)
{
    const __fp16 half = value; std::uint16_t bits;
    std::memcpy(&bits, &half, 2); return bits;
}
struct Pack
{
    std::unique_ptr<ALMetalTexture> radiance, sh;
    ALMetalWorldReflectionProbes probes;
};
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalTexture> light_map;
    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error);
        INFO(error); REQUIRE(renderer);
        auto arena = ALMetalWorldFrameData::create(*device, &error); REQUIRE(arena);
        arena->beginFrame(1); renderer->setFrameData(std::move(arena));
        ALMetalTextureDescriptor descriptor;
        descriptor.width = descriptor.height = SIDE;
        descriptor.format = ALMetalPixelFormat::rg8_unorm;
        light_map = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(light_map);
        const std::array<std::uint8_t, SIDE * SIDE * 2> lit = [] {
            std::array<std::uint8_t, SIDE * SIDE * 2> result; result.fill(255); return result;
        }();
        auto command = device->makeCommandBuffer("Reflection policy light map", &error); REQUIRE(command);
        REQUIRE(light_map->upload(*command, 0, 0, {}, lit.data(), SIDE * 2, &error));
        REQUIRE(command->commitAndWait(&error));
    }
    Pack pack(bool default_only)
    {
        Pack result;
        const unsigned count = default_only ? 1 : 3;
        ALMetalTextureDescriptor descriptor;
        descriptor.type = ALMetalTextureType::texture_cube_array;
        descriptor.format = ALMetalPixelFormat::rgba16_float;
        descriptor.width = descriptor.height = 1; descriptor.array_length = count;
        result.radiance = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(result.radiance);
        descriptor.type = ALMetalTextureType::texture_2d;
        descriptor.format = ALMetalPixelFormat::rgba32_uint;
        descriptor.width = 4; descriptor.height = count; descriptor.array_length = 1;
        result.sh = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(result.sh);
        std::vector<std::uint32_t> coefficients(count * 16, 0);
        auto command = device->makeCommandBuffer("Warmed reflection pack", &error); REQUIRE(command);
        for (unsigned index = 0; index < count; ++index)
        {
            std::array<std::uint16_t, 4> pixel;
            for (unsigned c = 0; c < 4; ++c) pixel[c] = encode(COLORS[index][c]);
            for (unsigned face = 0; face < 6; ++face)
                REQUIRE(result.radiance->upload(*command, 0, index * 6 + face, {}, pixel.data(), 8, &error));
            coefficients[index * 16] = pixel[0] | (std::uint32_t(pixel[1]) << 16);
            coefficients[index * 16 + 1] = pixel[2]; // L0 only; all higher orders are zero.
            result.probes.constants.ref_sphere[index][3] = 100.f;
            result.probes.constants.ref_params[index][0] = 1.f;
            result.probes.constants.ref_params[index][1] = 1.f;
            result.probes.constants.ref_params[index][2] = 1.f;
            result.probes.constants.ref_params[index][3] = -100.f;
            result.probes.constants.ref_index[index][0] = index;
            result.probes.constants.ref_index[index][1] = -1;
            result.probes.constants.ref_index[index][3] = index == 2 ? 1 : 0;
        }
        REQUIRE(result.sh->upload(*command, 0, 0, {}, coefficients.data(), 4 * 16, &error));
        REQUIRE(command->commitAndWait(&error)); INFO(error);
        result.probes.radiance_texture = result.radiance.get();
        result.probes.irradiance_texture = result.sh.get();
        for (unsigned axis = 0; axis < 3; ++axis)
            result.probes.constants.env_matrix[axis][axis] = 1.f;
        result.probes.constants.config[0] = float(count);
        return result;
    }
    std::vector<std::uint8_t> snapshot(const Pack& pack)
    {
        const unsigned count = pack.radiance->descriptor().array_length;
        std::vector<std::uint8_t> bytes(count * (6 * 8 + 4 * 16));
        for (unsigned slice = 0; slice < count * 6; ++slice)
            REQUIRE(pack.radiance->readback(*device, 0, slice, {}, bytes.data() + slice * 8, 8, &error));
        REQUIRE(pack.sh->readback(*device, 0, 0, {}, bytes.data() + count * 6 * 8, 4 * 16, &error));
        return bytes;
    }
    std::unique_ptr<ALMetalRenderTarget> gbuffer(bool pbr, bool reversed_normal = false)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = descriptor.height = SIDE; descriptor.color_attachment_count = 4;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
        descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.colors[2].format = ALMetalPixelFormat::rgba16_unorm;
        descriptor.colors[3].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        descriptor.depth_stencil_shader_read = true;
        auto result = ALMetalRenderTarget::create(*device, descriptor, &error); REQUIRE(result);
        ALMetalRenderPassConfig clear;
        clear.colors[0].clear = {.25, .25, .25, 0};
        clear.colors[1].clear = pbr ? ALMetalClearColor{1, 0, 1, 0} : ALMetalClearColor{1, 1, 1, 1};
        const double oct = reversed_normal ? 1. : .5;
        clear.colors[2].clear = {oct, oct, pbr ? 32896. / 65535. : 1., pbr ? .67 : .34};
        clear.clear_depth = .25; clear.depth_store = ALMetalStoreAction::store;
        clear.stencil_store = ALMetalStoreAction::store;
        auto command = device->makeCommandBuffer("Authored native reflection GBuffer", &error); REQUIRE(command);
        auto pass = result->makeRenderPass(clear, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Clear authored material planes", &error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        return result;
    }
    ALMetalWorldBaseLightingScene scene(const ALMetalRenderTarget& gbuffer, const Pack& pack, bool classic = false)
    {
        ALMetalWorldBaseLightingScene result;
        result.gbuffer = &gbuffer; result.light_map = light_map.get(); result.reflection_probes = &pack.probes;
        for (unsigned axis = 0; axis < 4; ++axis) result.constants.inverse_projection[axis * 5] = 1.f;
        result.constants.light_direction[2] = 1.f;
        result.constants.surface[1] = 1.f;
        result.constants.config[1] = classic;
        return result;
    }
    HalfPixels render(const ALMetalWorldBaseLightingScene& scene)
    {
        auto target = renderer->renderBaseLighting(scene, &error); INFO(error); REQUIRE(target);
        REQUIRE(target->descriptor().width == SIDE); REQUIRE(target->descriptor().height == SIDE);
        REQUIRE(target->descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
        HalfPixels result(SIDE * SIDE * 4);
        REQUIRE(target->colorTexture(0)->readback(*device, 0, 0, {}, result.data(), SIDE * 8, &error));
        REQUIRE(std::all_of(result.begin(), result.end(), [](auto v) { return std::isfinite(decode(v)); }));
        return result;
    }
};
unsigned changed(const HalfPixels& a, const HalfPixels& b)
{
    REQUIRE(a.size() == b.size()); unsigned result = 0;
    for (unsigned p = 0; p < a.size(); p += 4)
        if (a[p] != b[p] || a[p + 1] != b[p + 1] || a[p + 2] != b[p + 2]) ++result;
    return result;
}
bool positive_rgb(const HalfPixels& pixels)
{
    for (unsigned p = 0; p < pixels.size(); p += 4)
        for (unsigned c = 0; c < 3; ++c)
            if (decode(pixels[p + c]) > .01f) return true;
    return false;
}
}

TEST_CASE("Native base reflection policy consumes default only without rewriting warmed packs",
          "[almetal][reflection-policy]")
{
    @autoreleasepool
    {
        Fixture f; auto warmed = f.pack(false); auto control = f.pack(true);
        const auto bytes = f.snapshot(warmed);
        const auto native_radiance = warmed.radiance->nativeHandle();
        const auto native_sh = warmed.sh->nativeHandle();
        const auto constants = warmed.probes.constants;
        for (bool pbr : {false, true}) for (bool classic : {false, true})
        {
            CAPTURE(pbr, classic);
            auto gbuffer = f.gbuffer(pbr);
            auto scene = f.scene(*gbuffer, warmed, classic);
            const auto full = f.render(scene);
            warmed.probes.constants.config[0] = 1;
            const auto selected_default = f.render(scene);
            const auto independent_default = f.render(f.scene(*gbuffer, control, classic));
            CHECK(selected_default == independent_default);
            CHECK(changed(full, selected_default) > SIDE * SIDE / 2);
            CHECK(positive_rgb(selected_default));
            warmed.probes.constants.config[0] = 3;
            CHECK(f.render(scene) == full);
        }
        CHECK(std::memcmp(&constants, &warmed.probes.constants, sizeof(constants)) == 0);
        CHECK(warmed.radiance->nativeHandle() == native_radiance);
        CHECK(warmed.sh->nativeHandle() == native_sh);
        CHECK(f.snapshot(warmed) == bytes);
    }
}

TEST_CASE("Native default-only base reflections still finish with SSR then hero",
          "[almetal][reflection-policy]")
{
    @autoreleasepool
    {
        Fixture f; auto warmed = f.pack(false); warmed.probes.constants.config[0] = 1;
        ALMetalTextureDescriptor descriptor;
        descriptor.type = ALMetalTextureType::texture_cube;
        descriptor.width = descriptor.height = 1; descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        auto hero_texture = ALMetalTexture::create(*f.device, descriptor, &f.error); REQUIRE(hero_texture);
        descriptor.type = ALMetalTextureType::texture_2d;
        auto history_color = ALMetalTexture::create(*f.device, descriptor, &f.error); REQUIRE(history_color);
        descriptor.format = ALMetalPixelFormat::r32_float;
        auto history_depth = ALMetalTexture::create(*f.device, descriptor, &f.error); REQUIRE(history_depth);
        auto upload = f.device->makeCommandBuffer("Reflection finish positive controls", &f.error); REQUIRE(upload);
        const std::array<std::uint8_t, 4> red{255, 0, 0, 255}, green{0, 255, 0, 255};
        const float depth = .5f;
        for (unsigned face = 0; face < 6; ++face)
            REQUIRE(hero_texture->upload(*upload, 0, face, {}, green.data(), 4, &f.error));
        REQUIRE(history_color->upload(*upload, 0, 0, {}, red.data(), 4, &f.error));
        REQUIRE(history_depth->upload(*upload, 0, 0, {}, &depth, sizeof(depth), &f.error));
        REQUIRE(upload->commitAndWait(&f.error));
        ALMetalWorldHeroProbe hero; hero.radiance_texture = hero_texture.get();
        hero.constants.hero_sphere[3] = 100.f; hero.constants.clip_plane[3] = 1.f;
        hero.constants.config[0] = 1; hero.constants.config[1] = 1;
        ALMetalWorldScreenSpaceReflections ssr;
        ssr.scene_color = history_color.get(); ssr.scene_depth = history_depth.get();
        for (unsigned axis = 0; axis < 4; ++axis)
        {
            ssr.constants.projection[axis * 5] = 1.f;
            ssr.constants.inverse_projection[axis * 5] = 1.f;
            ssr.constants.inverse_modelview_delta[axis * 5] = 1.f;
        }
        ssr.constants.inverse_projection[14] = -.4f;
        ssr.constants.screen[0] = ssr.constants.screen[1] = ssr.constants.screen[2] = SIDE;
        ssr.constants.trace[0] = 4; ssr.constants.trace[1] = .1f;
        ssr.constants.trace[2] = .2f; ssr.constants.trace[3] = .001f;
        ssr.constants.sampling[0] = 1; ssr.constants.sampling[1] = 1.6f; ssr.constants.config[1] = 1;
        for (bool pbr : {false, true})
        {
            CAPTURE(pbr);
            auto gbuffer = f.gbuffer(pbr, true); auto scene = f.scene(*gbuffer, warmed);
            const auto ordinary = f.render(scene);
            scene.screen_space_reflections = &ssr; const auto reflected = f.render(scene);
            CHECK(changed(ordinary, reflected) > 0);
            unsigned red_hits = 0;
            for (unsigned p = 0; p < ordinary.size(); p += 4)
                red_hits += decode(reflected[p]) > decode(ordinary[p]) &&
                            decode(reflected[p + 2]) < decode(ordinary[p + 2]);
            CHECK(red_hits > 0);
            scene.screen_space_reflections = nullptr; scene.hero_probe = &hero;
            const auto hero_only = f.render(scene);
            CHECK(changed(ordinary, hero_only) > SIDE * SIDE / 2);
            unsigned green_hits = 0;
            for (unsigned p = 0; p < ordinary.size(); p += 4)
                green_hits += decode(hero_only[p + 1]) > decode(ordinary[p + 1]) &&
                              decode(hero_only[p + 2]) < decode(ordinary[p + 2]);
            CHECK(green_hits > SIDE * SIDE / 2);
            scene.screen_space_reflections = &ssr;
            CHECK(f.render(scene) == hero_only); // Full hero override comes after the successful red SSR hit.
        }
    }
}


TEST_CASE("Default-only native water probe helper excludes warmed locals without discarding default",
          "[almetal][reflection-policy]")
{
    @autoreleasepool
    {
        Fixture f; auto warmed = f.pack(false); auto control = f.pack(true);
        const auto packed_before = f.snapshot(warmed);
        // This is explicitly helper-level GPU coverage, not a water scene or
        // viewer invocation. Compile the exact production shader source with
        // a test entrypoint that calls its existing shared helper. No probe
        // traversal/weighting/filter algorithm is duplicated here. Actual
        // water-surface pixel/ownership coverage remains in almetal_gpu_test.
        const auto source_path = std::filesystem::path(__FILE__).parent_path().parent_path() /
            "shaders" / "almetal_builtin.metal";
        std::ifstream file(source_path);
        REQUIRE(file.good());
        std::string source((std::istreambuf_iterator<char>(file)), {});
        REQUIRE_FALSE(source.empty());
        source += R"metal(
kernel void test_water_quality_projection(
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(0)]],
    device float4* output [[buffer(1)]], constant uint& side [[buffer(2)]],
    texturecube_array<float> radiance [[texture(0)]], sampler probe_sampler [[sampler(0)]],
    uint2 pixel [[thread_position_in_grid]])
{
    if (any(pixel >= uint2(side))) return;
    const float3 position = float3((float2(pixel) + .5) / float(side) * 2.0 - 1.0, -.5);
    const float3 value = almetal_sample_water_probes(
        position, float3(0, 0, 1), 1.0, radiance, probe_sampler, probes);
    output[pixel.y * side + pixel.x] = float4(value, 1.0);
}
)metal";
        auto library = ALMetalShaderLibrary::compileSource(*f.device, source,
            "Actual native water helper diagnostic", &f.error); INFO(f.error); REQUIRE(library);
        ALMetalComputePipelineDescriptor pipeline_descriptor;
        pipeline_descriptor.library = library.get(); pipeline_descriptor.function = "test_water_quality_projection";
        auto pipeline = ALMetalComputePipeline::create(*f.device, pipeline_descriptor, &f.error);
        INFO(f.error); REQUIRE(pipeline);
        ALMetalSamplerDescriptor sampler_descriptor;
        sampler_descriptor.min_filter = sampler_descriptor.mag_filter = ALMetalSamplerFilter::linear;
        auto sampler = ALMetalSamplerState::create(*f.device, sampler_descriptor, &f.error); REQUIRE(sampler);
        const auto sample = [&](const Pack& pack)
        {
            ALMetalBufferDescriptor descriptor; descriptor.length = sizeof(pack.probes.constants);
            auto constants = ALMetalBuffer::create(*f.device, descriptor, &f.error); REQUIRE(constants);
            REQUIRE(constants->write(0, &pack.probes.constants, descriptor.length, &f.error));
            descriptor.length = SIDE * SIDE * sizeof(Color);
            auto output = ALMetalBuffer::create(*f.device, descriptor, &f.error); REQUIRE(output);
            std::vector<Color> result(SIDE * SIDE, Color{NAN, NAN, NAN, NAN});
            REQUIRE(output->write(0, result.data(), descriptor.length, &f.error));
            auto command = f.device->makeCommandBuffer("Native water helper quality", &f.error); REQUIRE(command);
            auto encoder = ALMetalComputeEncoder::begin(*command, "Probe quality projection", &f.error); REQUIRE(encoder);
            REQUIRE(encoder->setPipeline(*pipeline, &f.error));
            REQUIRE(encoder->setBuffer(0, *constants, 0, &f.error));
            REQUIRE(encoder->setBuffer(1, *output, 0, &f.error));
            REQUIRE(encoder->setBytes(2, &SIDE, sizeof(SIDE), &f.error));
            REQUIRE(encoder->setTexture(0, pack.radiance.get(), &f.error));
            REQUIRE(encoder->setSampler(0, sampler.get(), &f.error));
            REQUIRE(encoder->dispatch(SIDE, SIDE, 1, 4, 4, 1, &f.error));
            REQUIRE(encoder->end(&f.error)); REQUIRE(command->commitAndWait(&f.error));
            REQUIRE(output->read(0, result.data(), descriptor.length, &f.error));
            for (const auto& pixel : result) for (float channel : pixel) REQUIRE(std::isfinite(channel));
            return result;
        };
        const auto local = sample(warmed);
        warmed.probes.constants.config[0] = 1;
        const auto selected = sample(warmed), independent_default = sample(control);
        CHECK(selected == independent_default);
        unsigned differs = 0;
        for (unsigned index = 0; index < selected.size(); ++index)
        {
            // The production mix computes color*weight*(1/weight). Its
            // float32 result can straddle the authored constant by one ULP;
            // the independent default-pack comparison above remains exact.
            for (unsigned c = 0; c < 3; ++c)
            {
                CHECK(selected[index][c] >= std::nextafter(DEFAULT_COLOR[c], -INFINITY));
                CHECK(selected[index][c] <= std::nextafter(DEFAULT_COLOR[c], INFINITY));
            }
            CHECK(selected[index][3] == 1.f);
            differs += local[index] != selected[index];
        }
        CHECK(differs > SIDE * SIDE / 2);
        warmed.probes.constants.config[0] = 3;
        CHECK(sample(warmed) == local);
        CHECK(f.snapshot(warmed) == packed_before);
    }
}
