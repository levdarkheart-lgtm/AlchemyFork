/**
 * @file almetal_water_normal_filter_test.mm
 * @brief Original water normal-map filtering through the actual water owner.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetalworld.h"
#include <Foundation/Foundation.h>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstring>
#include <vector>

namespace
{
constexpr unsigned SIDE = 32;
using Pixel = std::array<std::uint16_t, 4>;
using Image = std::vector<Pixel>;
const std::array<float, 4> NORMAL_A{.75f, .5f, 1.f, 1.f};
const std::array<float, 4> NORMAL_B{.5f, .75f, 1.f, 1.f};
Pixel half(const std::array<float, 4>& source)
{
    Pixel value;
    for (unsigned c = 0; c < 4; ++c)
    {
        const __fp16 h = source[c];
        std::memcpy(&value[c], &h, sizeof(h));
    }
    return value;
}
void identity(float* matrix)
{
    for (unsigned i = 0; i < 16; ++i) matrix[i] = i % 5 == 0 ? 1.f : 0.f;
}
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalTexture> cells;
    std::unique_ptr<ALMetalRenderTarget> lighting;
    Image background;
    Fixture()
    {
        REQUIRE([[NSBundle mainBundle] pathForResource:@"almetal_builtin" ofType:@"metallib"] != nil);
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error); INFO(error); REQUIRE(renderer);
        auto frames = ALMetalWorldFrameData::create(*device, &error); REQUIRE(frames);
        frames->beginFrame(1); renderer->setFrameData(std::move(frames));
        ALMetalTextureDescriptor cell;
        cell.width = cell.height = 1; cell.format = ALMetalPixelFormat::rg32_float;
        cells = ALMetalTexture::create(*device, cell, &error); REQUIRE(cells);
        const std::array<float, 2> height{0.f, 1.f};
        auto command = device->makeCommandBuffer("Water authored height", &error); REQUIRE(command);
        REQUIRE(cells->upload(*command, 0, 0, {}, height.data(), sizeof(height), &error));
        REQUIRE(command->commitAndWait(&error));
        ALMetalRenderTargetDescriptor target;
        target.width = target.height = SIDE;
        target.colors[0].format = ALMetalPixelFormat::rgba16_float;
        lighting = ALMetalRenderTarget::create(*device, target, &error); REQUIRE(lighting);
        background.resize(SIDE * SIDE);
        for (unsigned y = 0; y < SIDE; ++y) for (unsigned x = 0; x < SIDE; ++x)
            background[y * SIDE + x] = half({float(x) / (SIDE - 1), float(y) / (SIDE - 1), .25f, .75f});
        ALMetalTextureDescriptor texture;
        texture.width = texture.height = SIDE; texture.format = ALMetalPixelFormat::rgba16_float;
        auto upload = ALMetalTexture::create(*device, texture, &error); REQUIRE(upload);
        command = device->makeCommandBuffer("Water authored refraction gradient", &error); REQUIRE(command);
        REQUIRE(upload->upload(*command, 0, 0, {}, background.data(), SIDE * sizeof(Pixel), &error));
        REQUIRE(lighting->copyColorFrom(*command, 0, *upload, {}, &error));
        REQUIRE(command->commitAndWait(&error));
    }
    ~Fixture() { if (device) device->waitUntilIdle(); }
    std::unique_ptr<ALMetalRenderTarget> gbuffer(bool hdr, bool reverse)
    {
        ALMetalRenderTargetDescriptor target;
        target.width = target.height = SIDE; target.color_attachment_count = 4;
        target.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
        target.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
        target.colors[2].format = ALMetalPixelFormat::rgba16_unorm;
        target.colors[3].format = hdr ? ALMetalPixelFormat::rg11b10_float : ALMetalPixelFormat::rgba8_unorm;
        target.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        target.depth_stencil_shader_read = true;
        auto value = ALMetalRenderTarget::create(*device, target, &error); REQUIRE(value);
        auto command = device->makeCommandBuffer("Water authored far Gbuffer", &error); REQUIRE(command);
        ALMetalRenderPassConfig config; config.clear_depth = reverse ? 0.0 : 1.0;
        config.depth_store = config.stencil_store = ALMetalStoreAction::store;
        auto pass = value->makeRenderPass(config, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Water far clear", &error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        return value;
    }
    std::unique_ptr<ALMetalTexture> normals(unsigned levels, bool alternate_mips, bool all_b = false, bool checker = false)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.width = descriptor.height = 8; descriptor.mipmap_level_count = levels;
        descriptor.format = ALMetalPixelFormat::rgba16_float;
        auto value = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(value);
        auto command = device->makeCommandBuffer("Water authored normal levels", &error); REQUIRE(command);
        for (unsigned level = 0; level < levels; ++level)
        {
            const unsigned side = 8u >> level;
            Image pixels(side * side, half(all_b || (alternate_mips && level != 0) ? NORMAL_B : NORMAL_A));
            if (checker) for (unsigned y = 0; y < side; ++y) for (unsigned x = 0; x < side; ++x)
                pixels[y * side + x] = half((x + y) % 2 ? NORMAL_A : NORMAL_B);
            REQUIRE(value->upload(*command, level, 0, {}, pixels.data(), side * sizeof(Pixel), &error));
        }
        REQUIRE(command->commitAndWait(&error));
        return value;
    }
    Image read(const ALMetalRenderTarget& value)
    {
        REQUIRE(value.descriptor().width == SIDE); REQUIRE(value.descriptor().height == SIDE);
        REQUIRE(value.descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
        Image pixels(SIDE * SIDE);
        REQUIRE(value.colorTexture(0)->readback(*device, 0, 0, {}, pixels.data(), SIDE * sizeof(Pixel), &error));
        return pixels;
    }
    std::vector<Image> readNormals(const ALMetalTexture& value)
    {
        std::vector<Image> levels;
        for (unsigned level = 0; level < value.descriptor().mipmap_level_count; ++level)
        {
            const unsigned side = value.descriptor().width >> level;
            levels.emplace_back(side * side);
            REQUIRE(value.readback(*device, level, 0, {}, levels.back().data(), side * sizeof(Pixel), &error));
        }
        return levels;
    }
    ALMetalWorldWaterSurface water(const ALMetalTexture& normal, bool reverse, float extent)
    {
        ALMetalWorldWaterSurface value;
        auto& field = value.field;
        field.cell_map = cells.get(); field.above_water = true;
        identity(field.modelview_projection); field.modelview_projection[0] = field.modelview_projection[5] = 1.f / extent;
        field.modelview_projection[14] = reverse ? .5f : 0.f;
        field.cell_square[0] = field.cell_square[1] = -extent; field.cell_square[2] = extent * 2; field.cell_square[3] = 1;
        field.rings[0] = extent; field.rings[1] = 1; field.rings[2] = 4;
        field.water_plane[2] = 1; field.water_plane[3] = -1; field.fog[0] = 1;
        field.draws[0] = {0u, 4u, 1u, true};
        field.draws[1] = {1u, 9u, 12u, false};
        field.draws[2] = {2u, 9u, 4u, false}; field.draw_count = 3;
        identity(value.modelview); value.modelview[14] = -1;
        value.normal_maps = {&normal, &normal}; value.eye_position[2] = -1;
        value.water_far[0] = 10000; value.parameters[1] = .1f; value.parameters[2] = 1;
        value.underwater = true; value.refracts = true;
        return value;
    }
    Image render(const ALMetalRenderTarget& input, ALMetalWorldWaterSurface water, bool hdr, bool reverse)
    {
        std::array<ALMetalWorldSimpleBatch, 2> markers;
        markers[0].water_haze = true; markers[1].water_surface = true;
        ALMetalWorldWaterHaze haze;
        identity(haze.inverse_projection);
        haze.water_plane[2] = 1.f; haze.water_plane[3] = -1.f;
        haze.fog[0] = 1.f; // Defined zero-density underwater pre-surface haze.
        ALMetalWorldOpaquePostDeferredScene scene;
        scene.gbuffer = &input; scene.composed_lighting = lighting.get(); scene.batches = markers.data(); scene.batch_count = markers.size();
        scene.width = scene.height = SIDE; scene.hdr = hdr; scene.reverse_z = reverse; scene.water_haze = &haze; scene.water_surface = &water;
        auto result = renderer->renderOpaquePostDeferred(scene, &error); INFO(error); REQUIRE(result);
        return read(*result);
    }
};
}

TEST_CASE("Native water normal mip policy selects base points or anisotropic levels",
          "[almetal][water-normal-filter]")
{
    @autoreleasepool
    {
        Fixture f;
        auto mixed = f.normals(4, true), a = f.normals(4, false), b = f.normals(4, false, true);
        const auto before = f.readNormals(*mixed); const void* native = mixed->nativeHandle();
        for (bool hdr : {false, true}) for (bool reverse : {false, true}) for (bool second_map : {false, true})
        {
            CAPTURE(hdr, reverse, second_map);
            auto input = f.gbuffer(hdr, reverse);
            auto water = f.water(*mixed, reverse, 1024.f);
            water.normal_maps[second_map ? 0 : 1] = a.get(); water.parameters[0] = second_map ? 1.f : 0.f;
            const auto mipped = f.render(*input, water, hdr, reverse);
            water.normal_maps_mipped = false;
            const auto point = f.render(*input, water, hdr, reverse);
            water.normal_maps_mipped = true;
            CHECK(bool(f.render(*input, water, hdr, reverse) == mipped));
            water.normal_maps = {a.get(), a.get()}; const auto independent_base = f.render(*input, water, hdr, reverse);
            water.normal_maps = {b.get(), b.get()}; const auto independent_mip = f.render(*input, water, hdr, reverse);
            CHECK(bool(independent_base != independent_mip));
            CHECK(bool(point == independent_base));
            CHECK(bool(mipped == independent_mip));
            CHECK(bool(point != mipped));
        }
        CHECK(mixed->nativeHandle() == native); CHECK(bool(f.readNormals(*mixed) == before));
        CHECK(bool(f.read(*f.lighting) == f.background));
    }
}

TEST_CASE("Native water normal point policy also preserves original magnification filtering",
          "[almetal][water-normal-filter]")
{
    @autoreleasepool
    {
        Fixture f;
        auto checker = f.normals(1, false, false, true), constant = f.normals(1, false);
        auto input = f.gbuffer(false, false);
        auto water = f.water(*checker, false, 1.f);
        const auto linear = f.render(*input, water, false, false);
        water.normal_maps_mipped = false;
        const auto point = f.render(*input, water, false, false);
        CHECK(bool(point != linear));
        water.normal_maps = {constant.get(), constant.get()};
        const auto constant_point = f.render(*input, water, false, false);
        water.normal_maps_mipped = true;
        CHECK(bool(f.render(*input, water, false, false) == constant_point));
        CHECK(bool(constant_point != f.background));
    }
}
