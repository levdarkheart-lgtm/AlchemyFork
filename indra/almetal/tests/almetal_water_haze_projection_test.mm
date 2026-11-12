/**
 * @file almetal_water_haze_projection_test.mm
 * @brief Screen-depth sampling by the actual perspective water haze shaders.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetalworld.h"
#include <Foundation/Foundation.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
constexpr std::uint32_t SIDE = 64;
using Pixel = std::array<float, 4>;
using Image = std::vector<Pixel>;

// Buffer ABI of ALMetalWorldWaterHazeConstants. No vertex, interpolation,
// coordinate reconstruction or fog implementation is duplicated by this test.
struct alignas(16) HazeConstants
{
    float modelview_projection[16]{};
    float inverse_projection[16]{};
    float cell_square[4]{};
    float default_height[4]{};
    float rings[4]{};
    float water_plane[4]{};
    float fog_color[4]{};
    float fog[4]{};
    std::int32_t patch_rect[4]{};
    std::uint32_t config[4]{};
};
static_assert(sizeof(HazeConstants) == 256);

void identity(float* matrix)
{
    for (unsigned i = 0; i < 16; ++i) matrix[i] = i % 5 == 0 ? 1.f : 0.f;
}

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> field_pipeline, screen_pipeline;
    std::unique_ptr<ALMetalTexture> cell_map;
    std::unique_ptr<ALMetalSamplerState> point_sampler;

    Fixture()
    {
        REQUIRE([[NSBundle mainBundle] pathForResource:@"almetal_builtin" ofType:@"metallib"] != nil);
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        library = ALMetalShaderLibrary::loadBuiltin(*device, &error); INFO(error); REQUIRE(library);
        const auto pipeline = [&](const char* vertex)
        {
            ALMetalRenderPipelineDescriptor descriptor;
            descriptor.library = library.get();
            descriptor.vertex_function = vertex;
            descriptor.fragment_function = "almetal_world_water_haze_fragment";
            descriptor.colors[0].format = ALMetalPixelFormat::rgba32_float;
            descriptor.label = vertex;
            auto result = ALMetalRenderPipeline::create(*device, descriptor, &error);
            INFO(error); REQUIRE(result); return result;
        };
        field_pipeline = pipeline("almetal_world_water_haze_field_vertex");
        screen_pipeline = pipeline("almetal_world_water_haze_fullscreen_vertex");

        ALMetalTextureDescriptor cell;
        cell.width = cell.height = 1;
        cell.format = ALMetalPixelFormat::rg32_float;
        cell_map = ALMetalTexture::create(*device, cell, &error); REQUIRE(cell_map);
        const std::array<float, 2> authored_cell{0.f, 1.f};
        auto command = device->makeCommandBuffer("Haze projected authored water cell", &error); REQUIRE(command);
        REQUIRE(cell_map->upload(*command, 0, 0, {}, authored_cell.data(), sizeof(authored_cell), &error));
        REQUIRE(command->commitAndWait(&error));

        ALMetalSamplerDescriptor sampler;
        sampler.min_filter = sampler.mag_filter = ALMetalSamplerFilter::nearest;
        sampler.address_s = sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
        point_sampler = ALMetalSamplerState::create(*device, sampler, &error); REQUIRE(point_sampler);
    }

    ~Fixture() { if (device) device->waitUntilIdle(); }

    std::unique_ptr<ALMetalTexture> depth(bool reverse)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.width = descriptor.height = SIDE;
        descriptor.format = ALMetalPixelFormat::r32_float;
        auto result = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(result);
        std::array<float, SIDE * SIDE> values;
        for (unsigned y = 0; y < SIDE; ++y) for (unsigned x = 0; x < SIDE; ++x)
        {
            // An asymmetric sloped floor with an authored inset. Incorrect
            // screen coordinates must change the sampled depth, not merely
            // redistribute a uniform fog value across the two field triangles.
            float d = .1f + .55f * float(x) / float(SIDE - 1) + .15f * float(y) / float(SIDE - 1);
            if (x > 19 && x < 39 && y > 11 && y < 47) d += .12f;
            values[y * SIDE + x] = reverse ? 1.f - d : d;
        }
        auto command = device->makeCommandBuffer("Haze projected authored scene depth", &error); REQUIRE(command);
        REQUIRE(result->upload(*command, 0, 0, {}, values.data(), SIDE * sizeof(float), &error));
        REQUIRE(command->commitAndWait(&error));
        return result;
    }

    HazeConstants constants(bool reverse, float yaw, float tilt)
    {
        HazeConstants value;
        identity(value.modelview_projection);
        identity(value.inverse_projection);
        const float c = std::cos(yaw), s = std::sin(yaw);
        value.modelview_projection[0] = c;
        value.modelview_projection[1] = s;
        value.modelview_projection[4] = -s;
        value.modelview_projection[5] = c;
        // A perspective homography of the flat water quad. Clip W varies
        // between its corners, unlike every prior orthographic haze fixture.
        value.modelview_projection[3] = tilt * c;
        value.modelview_projection[7] = tilt * s;
        if (reverse)
        {
            value.modelview_projection[2] = .5f * value.modelview_projection[3];
            value.modelview_projection[6] = .5f * value.modelview_projection[7];
            value.modelview_projection[14] = .5f;
            // Equivalent reconstructed floor positions from reversed depth.
            value.inverse_projection[10] = -2.f;
            value.inverse_projection[14] = 1.f;
        }
        value.cell_square[0] = value.cell_square[1] = -1.5f;
        value.cell_square[2] = 3.f; value.cell_square[3] = 1.f;
        value.water_plane[2] = 1.f; value.water_plane[3] = -2.f;
        value.fog_color[0] = .17f; value.fog_color[1] = .42f; value.fog_color[2] = .83f;
        value.fog[0] = 0.f; value.fog[1] = 7.f;
        value.config[0] = reverse ? 1u : 0u;
        return value;
    }

    Image render(const HazeConstants& constants, const ALMetalTexture& scene_depth, bool fullscreen)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = descriptor.height = SIDE;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba32_float;
        auto target = ALMetalRenderTarget::create(*device, descriptor, &error); REQUIRE(target);
        ALMetalRenderPassConfig config;
        config.colors[0].clear = {-1., -1., -1., -1.};
        config.colors[0].store = ALMetalStoreAction::store;
        auto pass = target->makeRenderPass(config, &error); REQUIRE(pass);
        auto command = device->makeCommandBuffer("Actual water haze projection", &error); REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Actual water haze projected field", &error);
        REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(fullscreen ? *screen_pipeline : *field_pipeline, &error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
        REQUIRE(encoder->setViewport({0., 0., double(SIDE), double(SIDE), 0., 1.}, &error));
        REQUIRE(encoder->setScissor({0, 0, SIDE, SIDE}, &error));
        REQUIRE(encoder->setVertexBytes(0, &constants, sizeof(constants), &error));
        REQUIRE(encoder->setVertexTexture(0, cell_map.get(), &error));
        REQUIRE(encoder->setFragmentBytes(0, &constants, sizeof(constants), &error));
        REQUIRE(encoder->setFragmentTexture(33, &scene_depth, &error));
        REQUIRE(encoder->setFragmentSampler(1, point_sampler.get(), &error));
        REQUIRE(encoder->draw(fullscreen ? ALMetalPrimitiveType::triangle : ALMetalPrimitiveType::triangle_strip,
                              0, fullscreen ? 3u : 4u, 1, &error));
        REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));
        Image image(SIDE * SIDE);
        REQUIRE(target->colorTexture(0)->readback(*device, 0, 0, {}, image.data(), SIDE * sizeof(Pixel), &error));
        return image;
    }
};
}

TEST_CASE("Native water haze samples the current screen pixel across perspective field triangles",
          "[almetal][water-haze-projection]")
{
    @autoreleasepool
    {
        Fixture fixture;
        for (bool reverse : {false, true})
        {
            const auto depth = fixture.depth(reverse);
            for (const auto& camera : {std::array<float, 2>{0.f, 0.f}, {.65f, .25f}, {-.9f, .25f}})
            {
                const auto constants = fixture.constants(reverse, camera[0], camera[1]);
                // The production fullscreen vertex has W=1 everywhere and
                // supplies the independent screen-coordinate control. Both
                // paths execute the identical production depth/fog fragment.
                const auto control = fixture.render(constants, *depth, true);
                const auto field = fixture.render(constants, *depth, false);
                unsigned covered = 0, different = 0;
                float maximum_error = 0.f, control_range = 0.f;
                for (unsigned y = 2; y + 2 < SIDE; ++y) for (unsigned x = 2; x + 2 < SIDE; ++x)
                {
                    const unsigned p = y * SIDE + x;
                    // Exclude the quad boundary, including clipped edges.
                    if (field[p][3] < 0.f || field[p - 1][3] < 0.f || field[p + 1][3] < 0.f ||
                        field[p - SIDE][3] < 0.f || field[p + SIDE][3] < 0.f) continue;
                    ++covered;
                    float difference = 0.f;
                    for (unsigned channel = 0; channel < 4; ++channel)
                    {
                        REQUIRE(std::isfinite(field[p][channel]));
                        REQUIRE(std::isfinite(control[p][channel]));
                        difference = std::max(difference, std::abs(field[p][channel] - control[p][channel]));
                    }
                    maximum_error = std::max(maximum_error, difference);
                    different += difference > .00002f;
                    control_range = std::max(control_range, std::abs(control[p][3] - control[SIDE * SIDE / 2][3]));
                }
                CAPTURE(reverse, camera[0], camera[1], covered, different, maximum_error, control_range);
                CHECK(covered > 1000);
                CHECK(control_range > .01f); // A constant-depth/constant-output fixture cannot pass.
                CHECK(maximum_error < .00002f);
                CHECK(different == 0);
            }
        }
    }
}
