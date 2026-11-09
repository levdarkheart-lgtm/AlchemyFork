/**
 * @file almetal_gbuffer_constants_test.mm
 * @brief Consecutive G-buffer constants keep exact draw state and slot ownership.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "../almetalworld.h"
#include "../almetalpipeline.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
    constexpr std::uint32_t image_size = 32;
    constexpr std::size_t common_constant_bytes = 400;

    struct VertexData
    {
        std::array<std::array<float, 4>, 24> positions{};
        std::array<std::array<float, 4>, 24> normals{};
        std::array<std::array<float, 2>, 24> texcoords{};
        std::array<std::array<std::uint8_t, 4>, 24> colors{};
    };

    struct Image
    {
        std::array<std::vector<std::uint8_t>, 4> colors;
        std::vector<float> depth;
    };

    struct Fixture
    {
        std::string error;
        std::unique_ptr<ALMetalDevice> device;
        std::unique_ptr<ALMetalWorldGBufferRenderer> reference;
        std::unique_ptr<ALMetalWorldGBufferRenderer> cached;
        std::shared_ptr<ALMetalWorldFrameData> arena;
        std::unique_ptr<ALMetalBuffer> vertices, indices, terrain_vertices;
        std::unique_ptr<ALMetalTexture> red, green, height, composition;
        std::array<ALMetalWorldSimpleBatch, 8> batches;
        ALMetalWorldSimpleBatch terrain;

        template<class T>
        std::unique_ptr<ALMetalBuffer> makeBuffer(const T& bytes)
        {
            ALMetalBufferDescriptor descriptor;
            descriptor.length = sizeof(bytes);
            descriptor.storage = ALMetalStorageMode::shared;
            auto buffer = ALMetalBuffer::create(*device, descriptor, &error);
            INFO(error);
            REQUIRE(buffer);
            REQUIRE(buffer->write(0, &bytes, sizeof(bytes), &error));
            return buffer;
        }

        std::unique_ptr<ALMetalTexture> makeTexture(
            ALMetalPixelFormat format, std::uint32_t size = 1)
        {
            ALMetalTextureDescriptor descriptor;
            descriptor.format = format;
            descriptor.width = size;
            descriptor.height = size;
            descriptor.usage = ALMetalTextureUsage::shader_read;
            auto texture = ALMetalTexture::create(*device, descriptor, &error);
            INFO(error);
            REQUIRE(texture);
            return texture;
        }

        Fixture()
        {
            device = ALMetalDevice::create(&error);
            INFO(error);
            REQUIRE(device);
            reference = ALMetalWorldGBufferRenderer::create(*device, &error);
            cached = ALMetalWorldGBufferRenderer::create(*device, &error);
            arena = ALMetalWorldFrameData::create(*device, &error);
            REQUIRE(reference);
            REQUIRE(cached);
            REQUIRE(arena);
            cached->setFrameData(arena);
            arena->beginFrame(1);

            VertexData data;
            std::array<std::uint16_t, 24> index_values{};
            for (std::size_t triangle = 0; triangle < 8; ++triangle)
            {
                const float x = -0.75f + 0.5f * (triangle % 4);
                const float y = -0.5f + static_cast<float>(triangle / 4);
                data.positions[triangle * 3] = {x - 0.2f, y - 0.35f, 0.25f, 0.f};
                data.positions[triangle * 3 + 1] = {x + 0.2f, y - 0.35f, 0.25f, 0.f};
                data.positions[triangle * 3 + 2] = {x, y + 0.35f, 0.25f, 0.f};
            }
            for (std::size_t index = 0; index < data.positions.size(); ++index)
            {
                data.normals[index] = {0.f, 0.f, 1.f, 0.f};
                data.colors[index] = {255, 255, 255, 255};
                index_values[index] = static_cast<std::uint16_t>(index);
            }
            vertices = makeBuffer(data);
            indices = makeBuffer(index_values);
            red = makeTexture(ALMetalPixelFormat::rgba8_unorm_srgb);
            green = makeTexture(ALMetalPixelFormat::rgba8_unorm_srgb);
            height = makeTexture(ALMetalPixelFormat::r32_float, 2);
            composition = makeTexture(ALMetalPixelFormat::rg16_float, 2);
            auto upload = device->makeCommandBuffer("G-buffer constants fixtures", &error);
            REQUIRE(upload);
            const std::array<std::uint8_t, 4> red_pixel{255, 0, 0, 255};
            const std::array<std::uint8_t, 4> green_pixel{0, 255, 0, 255};
            const std::array<float, 4> flat_height{};
            const std::array<std::uint16_t, 8> flat_composition{};
            REQUIRE(red->upload(*upload, 0, 0, {}, red_pixel.data(), 4, &error));
            REQUIRE(green->upload(*upload, 0, 0, {}, green_pixel.data(), 4, &error));
            REQUIRE(height->upload(*upload, 0, 0, {}, flat_height.data(), 8, &error));
            REQUIRE(composition->upload(*upload, 0, 0, {},
                flat_composition.data(), 8, &error));
            REQUIRE(upload->commitAndWait(&error));

            for (std::size_t index = 0; index < batches.size(); ++index)
            {
                auto& batch = batches[index];
                batch.vertices = vertices.get();
                batch.normal_offset = offsetof(VertexData, normals);
                batch.texcoord_offset = offsetof(VertexData, texcoords);
                batch.color_offset = offsetof(VertexData, colors);
                batch.vertex_count = data.positions.size();
                batch.indices = indices.get();
                batch.index_offset = index * 3 * sizeof(std::uint16_t);
                batch.index_count = 3;
                batch.texture_count = 1;
                batch.diffuse_textures[0] = index % 2 ? green.get() : red.get();
                for (std::size_t axis = 0; axis < 4; ++axis)
                    batch.modelview_projection[axis * 4 + axis] =
                        batch.modelview[axis * 4 + axis] = 1.f;
                for (std::size_t axis = 0; axis < 3; ++axis)
                    batch.normal_matrix[axis][axis] = 1.f;
            }

            const std::array<std::array<float, 4>, 4> controls{{
                {{0.f, 0.f, 0.f, 1.f}}, {{1.f, 0.f, 0.f, 1.f}},
                {{1.f, 1.f, 0.f, 1.f}}, {{0.f, 1.f, 0.f, 1.f}},
            }};
            terrain_vertices = makeBuffer(controls);
            terrain.terrain = true;
            terrain.vertices = terrain_vertices.get();
            terrain.vertex_count = 4;
            terrain.texture_count = 4;
            for (std::size_t slot = 0; slot < 4; ++slot)
                terrain.diffuse_textures[slot] = red.get();
            terrain.terrain_height_texture = height.get();
            terrain.terrain_composition_texture = composition.get();
            terrain.terrain_alpha_ramp_texture = green.get();
            terrain.terrain_grid_scale = 1.f;
            terrain.terrain_region_scale = 4.f;
            terrain.terrain_detail_scale = 1.f;
            terrain.terrain_triplanar_blend = 1.f;
            terrain.terrain_tessellation_pinned = true;
            std::fill_n(terrain.terrain_tessellation_factors, 6, std::uint16_t{0x3c00});
            for (std::size_t axis = 0; axis < 4; ++axis)
                terrain.modelview_projection[axis * 4 + axis] = 1.f;
            for (std::size_t axis = 0; axis < 3; ++axis)
                terrain.normal_matrix[axis][axis] = 1.f;
            // It still overwrites vertex slot4 but contributes no pixels.
            terrain.modelview_projection[12] = 100.f;
        }

        ALMetalWorldGBufferScene scene(const ALMetalWorldSimpleBatch* draws,
                                      std::size_t count) const
        {
            ALMetalWorldGBufferScene result;
            result.batches = draws;
            result.batch_count = count;
            result.width = result.height = image_size;
            std::fill_n(result.clear_color, 4, 0.f);
            return result;
        }

        Image read(const ALMetalRenderTarget& target)
        {
            Image image;
            for (std::size_t attachment = 0; attachment < 4; ++attachment)
            {
                const auto* texture = target.colorTexture(attachment);
                const auto row_bytes = image_size * texture->bytesPerPixel();
                image.colors[attachment].resize(image_size * row_bytes);
                REQUIRE(texture->readback(*device, 0, 0, {},
                    image.colors[attachment].data(), row_bytes, &error));
            }
            image.depth.resize(image_size * image_size);
            REQUIRE(target.depthStencilTexture()->readback(*device, 0, 0, {},
                image.depth.data(), image_size * sizeof(float), &error));
            return image;
        }

        Image render(ALMetalWorldGBufferRenderer& renderer,
                     const ALMetalWorldGBufferScene& scene)
        {
            auto target = renderer.render(scene, &error);
            INFO(error);
            REQUIRE(target);
            return read(*target);
        }
    };

    void checkImage(const Image& expected, const Image& actual)
    {
        for (std::size_t attachment = 0; attachment < 4; ++attachment)
        {
            CAPTURE(attachment);
            CHECK(actual.colors[attachment] == expected.colors[attachment]);
        }
        CHECK(actual.depth == expected.depth);
    }

    void checkRedAndGreen(const Image& image)
    {
        std::size_t red = 0, green = 0;
        for (std::size_t offset = 0; offset < image.colors[0].size(); offset += 4)
        {
            red += image.colors[0][offset] == 255 &&
                image.colors[0][offset + 1] == 0 && image.colors[0][offset + 2] == 0;
            green += image.colors[0][offset] == 0 &&
                image.colors[0][offset + 1] == 255 && image.colors[0][offset + 2] == 0;
        }
        CHECK(red > 0);
        CHECK(green > 0);
    }
}

TEST_CASE("almetal: consecutive G-buffer constants preserve exact values and texture changes",
          "[almetal][gbuffer-constants]")
{
    @autoreleasepool
    {
        Fixture fixture;
        for (const bool reverse : {false, true})
        {
            for (const bool hdr : {false, true})
            {
                CAPTURE(reverse, hdr);
                auto scene = fixture.scene(fixture.batches.data(), fixture.batches.size());
                scene.reverse_z = reverse;
                scene.hdr = scene.normal_hdr = hdr;
                const auto expected = fixture.render(*fixture.reference, scene);
                checkRedAndGreen(expected);
                const auto before = fixture.arena->stats();
                const auto binds_before = fixture.cached->constantStats();
                checkImage(expected, fixture.render(*fixture.cached, scene));
                CHECK(fixture.cached->constantStats().bindings - binds_before.bindings == 8);
                CHECK(fixture.cached->constantStats().reused_bindings - binds_before.reused_bindings == 7);
                CHECK(fixture.arena->stats().published_bytes - before.published_bytes ==
                      common_constant_bytes);

                // A,A,B,B,C,A,D,D: identity/range/texture changes do not alter
                // constants; matrix/config changes must start new publications.
                auto changed = fixture.batches;
                changed[2].modelview_projection[14] = 0.125f;
                changed[3].modelview_projection[14] = 0.125f;
                changed[4].alpha_mask = true;
                changed[4].alpha_cutoff = 0.5f;
                changed[4].use_vertex_color = false;
                changed[6].texture_matrix[12] = changed[7].texture_matrix[12] = 0.25f;
                changed[6].normal_matrix[2][0] = changed[7].normal_matrix[2][0] = 1.f;
                changed[6].normal_matrix[2][2] = changed[7].normal_matrix[2][2] = 0.f;
                scene.batches = changed.data();
                const auto expected_changed = fixture.render(*fixture.reference, scene);
                const auto changed_before = fixture.arena->stats();
                const auto changed_binds_before = fixture.cached->constantStats();
                checkImage(expected_changed, fixture.render(*fixture.cached, scene));
                CHECK(fixture.cached->constantStats().bindings - changed_binds_before.bindings == 8);
                CHECK(fixture.cached->constantStats().reused_bindings - changed_binds_before.reused_bindings == 3);
                CHECK(fixture.arena->stats().published_bytes - changed_before.published_bytes ==
                      5 * common_constant_bytes);
            }
        }

        // Keep preflight fail-closed, even after an otherwise reusable prefix.
        auto invalid = fixture.batches;
        invalid.back().modelview_projection[0] = std::numeric_limits<float>::quiet_NaN();
        auto scene = fixture.scene(invalid.data(), invalid.size());
        const auto before = fixture.arena->stats();
        CHECK_FALSE(fixture.cached->render(scene, &fixture.error));
        CHECK(fixture.error == "Metal world simple batch is invalid");
        CHECK(fixture.arena->stats().published_bytes == before.published_bytes);
    }
}

TEST_CASE("almetal: cached G-buffer constants rebind after terrain and stay local to render",
          "[almetal][gbuffer-constants]")
{
    @autoreleasepool
    {
        Fixture fixture;
        for (const bool reverse : {false, true})
        {
            CAPTURE(reverse);
            auto terrain_scene = fixture.scene(&fixture.terrain, 1);
            terrain_scene.reverse_z = reverse;
            const auto terrain_before = fixture.arena->stats();
            fixture.render(*fixture.cached, terrain_scene);
            const auto terrain_bytes = fixture.arena->stats().published_bytes -
                terrain_before.published_bytes;
            REQUIRE(terrain_bytes > common_constant_bytes);

            const std::array<ALMetalWorldSimpleBatch, 3> draws{
                fixture.batches[0], fixture.terrain, fixture.batches[1]};
            auto scene = fixture.scene(draws.data(), draws.size());
            scene.reverse_z = reverse;
            const auto expected = fixture.render(*fixture.reference, scene);
            checkRedAndGreen(expected);
            const auto before = fixture.arena->stats();
            checkImage(expected, fixture.render(*fixture.cached, scene));
            CHECK(fixture.arena->stats().published_bytes - before.published_bytes ==
                  terrain_bytes + common_constant_bytes);

            // Same frame and native command, two render calls: neither may
            // inherit the other call's cached slice or suppress publication.
            scene.batches = fixture.batches.data();
            scene.batch_count = 2;
            const auto expected_pair = fixture.render(*fixture.reference, scene);
            const auto batch_before = fixture.arena->stats();
            REQUIRE(fixture.cached->beginCommandBatch("G-buffer local constants", &fixture.error));
            auto first = fixture.cached->render(scene, &fixture.error);
            auto second = fixture.cached->render(scene, &fixture.error);
            REQUIRE(first);
            REQUIRE(second);
            CHECK(fixture.arena->stats().published_bytes - batch_before.published_bytes ==
                  2 * common_constant_bytes);
            REQUIRE(fixture.cached->commitCommandBatch(&fixture.error));
            REQUIRE(fixture.device->waitUntilIdle(&fixture.error));
            checkImage(expected_pair, fixture.read(*first));
            checkImage(expected_pair, fixture.read(*second));
        }
    }
}

namespace
{
    // A byte-identical layout model for the private common G-buffer payload.
    // This benchmark measures publication/binding/encoding, not construction
    // of the production matrices or complete viewer render() preparation.
    struct alignas(16) TimedWorldConstants
    {
        float modelview_projection[16]{};
        float modelview[16]{};
        float normal_matrix[3][4]{};
        float rigged_normal_matrix[3][4]{};
        float texture_matrix[16]{};
        float material[4]{};
        std::uint32_t depth_convention[4]{};
        float avatar_wind[4]{};
        float avatar_sin_wave[4]{};
        float avatar_gravity[4]{};
        float avatar_cloth_depth[4]{};
        float mirror_clip_plane[4]{0.f, 0.f, 0.f, 1.f};
    };
    static_assert(sizeof(TimedWorldConstants) == common_constant_bytes);

    struct EncodingCase
    {
        const char* name;
        std::vector<TimedWorldConstants> constants;
        std::size_t expected_publications;
        bool clobber_slot;
        std::array<double, 7> before{}, after{};
    };

    struct EncodingFixture
    {
        std::string error;
        std::unique_ptr<ALMetalDevice> device;
        std::shared_ptr<ALMetalWorldFrameData> arena;
        std::unique_ptr<ALMetalShaderLibrary> library;
        std::unique_ptr<ALMetalRenderPipeline> pipeline;
        std::unique_ptr<ALMetalRenderTarget> target;
        std::unique_ptr<ALMetalRenderPass> pass;
        std::unique_ptr<ALMetalSamplerState> sampler;
        std::array<std::unique_ptr<ALMetalTexture>, 2> textures;
        std::uint64_t generation = 1;

        EncodingFixture()
        {
            device = ALMetalDevice::create(&error);
            REQUIRE(device);
            arena = ALMetalWorldFrameData::create(*device, &error);
            REQUIRE(arena);
            const std::string source = R"metal(
#include <metal_stdlib>
using namespace metal;
struct V { float4 position [[position]]; float4 color; };
vertex V encoding_vertex(uint vertex_index [[vertex_id]],
                         constant float4* payload [[buffer(4)]])
{
    const float2 positions[3] = {float2(-1,-1), float2(1,-1), float2(0,1)};
    V result;
    result.position = float4(positions[vertex_index], 0, 1);
    result.color = payload[0] + payload[24];
    return result;
}
fragment float4 encoding_fragment(V input [[stage_in]],
    texture2d<float> texture [[texture(0)]], sampler sample [[sampler(0)]])
{
    return input.color + texture.sample(sample, float2(0.5));
}
)metal";
            library = ALMetalShaderLibrary::compileSource(
                *device, source, "Opt-in common constants encoding library", &error);
            INFO(error);
            REQUIRE(library);
            ALMetalRenderPipelineDescriptor pipeline_descriptor;
            pipeline_descriptor.library = library.get();
            pipeline_descriptor.vertex_function = "encoding_vertex";
            pipeline_descriptor.fragment_function = "encoding_fragment";
            pipeline_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
            pipeline_descriptor.label = "Common constants CPU encoding benchmark";
            pipeline = ALMetalRenderPipeline::create(*device, pipeline_descriptor, &error);
            REQUIRE(pipeline);
            ALMetalRenderTargetDescriptor target_descriptor;
            target_descriptor.width = target_descriptor.height = 1;
            target_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
            target = ALMetalRenderTarget::create(*device, target_descriptor, &error);
            REQUIRE(target);
            pass = target->makeRenderPass({}, &error);
            REQUIRE(pass);
            sampler = ALMetalSamplerState::create(*device, {}, &error);
            REQUIRE(sampler);
            ALMetalTextureDescriptor texture_descriptor;
            texture_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
            texture_descriptor.usage = ALMetalTextureUsage::shader_read;
            for (auto& texture : textures)
            {
                texture = ALMetalTexture::create(*device, texture_descriptor, &error);
                REQUIRE(texture);
            }
        }

        double encode(const EncodingCase& scenario, bool reuse)
        {
            // Resource creation, pass setup, frame retirement and publication
            // warm-up are excluded. The native command is NEVER submitted.
            arena->beginFrame(generation++);
            auto command = device->makeCommandBuffer("Opt-in constants CPU encoding", &error);
            REQUIRE(command);
            auto encoder = ALMetalRenderEncoder::begin(*command, *pass,
                "Opt-in constants CPU encoder", &error);
            REQUIRE(encoder);
            REQUIRE(encoder->setPipeline(*pipeline, &error));
            REQUIRE(encoder->setViewport({0, 0, 1, 1, 0, 1}, &error));
            REQUIRE(encoder->setScissor({0, 0, 1, 1}, &error));
            REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
            REQUIRE(encoder->setFragmentSampler(0, sampler.get(), &error));
            TimedWorldConstants previous{}, other{};
            other.modelview_projection[12] = 13.f;
            ALMetalWorldDataSlice slice;
            bool have_previous = false, succeeded = true;
            const auto before = arena->stats();
            const auto begin = std::chrono::steady_clock::now();
            for (std::size_t index = 0; index < scenario.constants.size(); ++index)
            {
                const auto& value = scenario.constants[index];
                if (scenario.clobber_slot && index % 8 == 7)
                    succeeded = encoder->setVertexBytes(4, &other, sizeof(other), &error);
                if (succeeded && (!reuse || !have_previous ||
                    std::memcmp(&previous, &value, sizeof(value)) != 0))
                {
                    succeeded = arena->publish(*command, &value, sizeof(value),
                                                slice, &error, false);
                    if (succeeded && reuse)
                    {
                        std::memcpy(&previous, &value, sizeof(value));
                        have_previous = true;
                    }
                }
                succeeded = succeeded &&
                    encoder->setVertexBuffer(4, *slice.buffer, slice.offset, &error) &&
                    encoder->setFragmentTexture(0, textures[index % 2].get(), &error) &&
                    encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error);
                if (!succeeded) break;
            }
            const auto end = std::chrono::steady_clock::now();
            INFO(error);
            REQUIRE(succeeded);
            REQUIRE(encoder->end(&error));
            CHECK(arena->stats().published_bytes - before.published_bytes ==
                (reuse ? scenario.expected_publications : scenario.constants.size()) *
                    common_constant_bytes);
            CHECK_FALSE(command->isCommitted());
            return std::chrono::duration<double, std::micro>(end - begin).count();
        }
    };
}

TEST_CASE("almetal: opt-in consecutive common constants CPU encoding measurements",
          "[.][almetal][gbuffer-constants-benchmark]")
{
    @autoreleasepool
    {
        constexpr std::size_t operations = 10000;
        constexpr std::size_t repetitions = 7;
        EncodingFixture fixture;
        std::array<EncodingCase, 4> cases{{
            {"repeated_textures_alternate", {}, 1, false},
            {"unique_matrices", {}, operations, false},
            {"mixed_four_draw_groups", {}, operations / 4, false},
            {"repeated_slot4_bytes_clobber", {}, 1, true},
        }};
        for (std::size_t kind = 0; kind < cases.size(); ++kind)
        {
            auto& scenario = cases[kind];
            scenario.constants.resize(operations);
            for (std::size_t index = 0; index < operations; ++index)
            {
                auto& value = scenario.constants[index];
                for (std::size_t axis = 0; axis < 4; ++axis)
                    value.modelview_projection[axis * 4 + axis] =
                        value.modelview[axis * 4 + axis] =
                        value.texture_matrix[axis * 4 + axis] = 1.f;
                value.depth_convention[1] = 1;
                if (kind == 1) value.modelview_projection[12] = index * 0.00001f;
                if (kind == 2)
                {
                    value.modelview_projection[12] = (index / 4) * 0.00001f;
                    value.material[3] = ((index / 4) % 2) ? 1.f : 0.f;
                    value.texture_matrix[12] = (index / 4) % 3 * 0.125f;
                }
            }
            for (unsigned warm = 0; warm < 2; ++warm)
            {
                fixture.encode(scenario, false);
                fixture.encode(scenario, true);
            }
        }
        for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
            for (std::size_t position = 0; position < cases.size(); ++position)
            {
                const auto index = (position + repetition) % cases.size();
                auto& scenario = cases[index];
                if ((index + repetition) % 2 == 0)
                {
                    scenario.before[repetition] = fixture.encode(scenario, false);
                    scenario.after[repetition] = fixture.encode(scenario, true);
                }
                else
                {
                    scenario.after[repetition] = fixture.encode(scenario, true);
                    scenario.before[repetition] = fixture.encode(scenario, false);
                }
            }
        std::cout << std::fixed << std::setprecision(3);
        for (const auto& scenario : cases)
        {
            auto before = scenario.before, after = scenario.after;
            for (std::size_t repetition = 0; repetition < repetitions; ++repetition)
                std::cout << "COMMON_CONSTANTS_CPU_RAW case=" << scenario.name
                          << " repetition=" << repetition
                          << " before_us=" << before[repetition]
                          << " after_us=" << after[repetition] << '\n';
            std::sort(before.begin(), before.end());
            std::sort(after.begin(), after.end());
            std::cout << "COMMON_CONSTANTS_CPU case=" << scenario.name
                      << " operations=" << operations << " repetitions=" << repetitions
                      << " before_median_us=" << before[repetitions / 2]
                      << " after_median_us=" << after[repetitions / 2]
                      << " before_range_us=" << before.front() << '/' << before.back()
                      << " after_range_us=" << after.front() << '/' << after.back()
                      << " GPU_submission=none viewer_FPS=unmeasured\n";
        }
    }
}
