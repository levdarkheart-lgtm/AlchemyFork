/**
 * @file almetal_pbr_probe_formats_test.mm
 * @brief Published HDR/SDR default probes feed the actual material preview.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetalpbrpreview.h"
#include "almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace
{
constexpr unsigned SIDE = 8;
using Pixels = std::array<std::uint8_t, SIDE * SIDE * 4>;
using SH = std::array<std::uint32_t, 16>;

struct Geometry
{
    float positions[3][4]{{-1,-1,0,1}, {3,-1,0,1}, {-1,3,0,1}};
    float normals[3][4]{{0,0,1,0}, {0,0,1,0}, {0,0,1,0}};
    float texcoords[3][2]{{.5f,.5f}, {.5f,.5f}, {.5f,.5f}};
    std::uint8_t colors[3][4]{{255,255,255,255},
                            {255,255,255,255}, {255,255,255,255}};
    alignas(16) float tangents[3][4]{{1,0,0,1}, {1,0,0,1}, {1,0,0,1}};
};

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalPBRPreviewRenderer> preview;
    std::unique_ptr<ALMetalWorldGBufferRenderer> projector;
    std::unique_ptr<ALMetalBuffer> vertices, indices;
    std::unique_ptr<ALMetalTexture> base, normal, orm, emissive;
    ALMetalTextureDescriptor output;
    ALMetalPBRPreviewBatch batch;
    ALMetalPBRPreviewEnvironment environment;
    ALMetalPBRPreviewScene scene;

    template<class T> std::unique_ptr<ALMetalBuffer> buffer(const T& values)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length = sizeof(values);
        descriptor.storage = ALMetalStorageMode::shared;
        auto result = ALMetalBuffer::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result);
        REQUIRE(result->write(0, &values, sizeof(values), &error));
        return result;
    }

    std::unique_ptr<ALMetalTexture> texture(
        ALMetalPixelFormat format, std::array<std::uint8_t, 4> rgba)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.width = descriptor.height = 1;
        descriptor.format = format;
        auto result = ALMetalTexture::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result);
        auto command = device->makeCommandBuffer("Preview material input", &error);
        REQUIRE(command);
        REQUIRE(result->upload(*command, 0, 0, {}, rgba.data(), 4, &error));
        REQUIRE(command->commitAndWait(&error));
        return result;
    }

    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        std::cout << "PROBE_PREVIEW_GPU " << device->info().name << '\n';
        preview = ALMetalPBRPreviewRenderer::create(*device, &error);
        INFO(error); REQUIRE(preview);
        vertices = buffer(Geometry{});
        indices = buffer(std::array<std::uint16_t, 3>{0,1,2});
        base = texture(ALMetalPixelFormat::rgba8_unorm_srgb, {255,255,255,255});
        normal = texture(ALMetalPixelFormat::rgba8_unorm, {128,128,255,255});
        orm = texture(ALMetalPixelFormat::rgba8_unorm, {255,255,255,255});
        emissive = texture(ALMetalPixelFormat::rgba8_unorm_srgb, {0,0,0,255});
        batch.vertices = vertices.get(); batch.vertex_count = 3;
        batch.position_offset = offsetof(Geometry, positions);
        batch.normal_offset = offsetof(Geometry, normals);
        batch.texcoord_offset = offsetof(Geometry, texcoords);
        batch.color_offset = offsetof(Geometry, colors);
        batch.tangent_offset = offsetof(Geometry, tangents);
        batch.indices = indices.get(); batch.index_count = 3;
        batch.base_color_texture = base.get(); batch.normal_texture = normal.get();
        batch.orm_texture = orm.get(); batch.emissive_texture = emissive.get();
        batch.double_sided = true;
        batch.sun_color[0] = batch.sun_color[1] = batch.sun_color[2] = 0.f;
        for (unsigned axis = 0; axis < 4; ++axis)
        {
            batch.modelview_projection[axis * 4 + axis] = 1.f;
            batch.modelview[axis * 4 + axis] = 1.f;
            if (axis < 3) batch.normal_matrix[axis][axis] = 1.f;
        }
        batch.modelview[14] = -2.f;
        output.width = output.height = SIDE;
        output.format = ALMetalPixelFormat::rgba8_unorm;
        scene.batches = &batch; scene.batch_count = 1;
        scene.destination_descriptor = &output;
        scene.environment = &environment;
        scene.tonemap_enabled = false;
    }

    std::unique_ptr<ALMetalTexture> cube(ALMetalPixelFormat format, bool alpha)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.type = ALMetalTextureType::texture_cube;
        descriptor.format = format;
        descriptor.width = descriptor.height = 4;
        descriptor.mipmap_level_count = 2;
        auto result = ALMetalTexture::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result);
        // Constant blue is exactly representable in all three formats. No
        // image decoder, sRGB transfer, or R11 CPU approximation is involved.
        std::vector<std::uint8_t> bytes(16 * result->bytesPerPixel());
        for (unsigned texel = 0; texel < 16; ++texel)
        {
            if (format == ALMetalPixelFormat::rgba16_float)
            {
                const std::array<std::uint16_t, 4> value{
                    0, 0, 0x3c00, std::uint16_t(alpha ? 0x3c00 : 0)};
                std::memcpy(bytes.data() + texel * 8, value.data(), 8);
            }
            else if (format == ALMetalPixelFormat::rg11b10_float)
            {
                const std::uint32_t blue_one = 0x1e0u << 22u;
                std::memcpy(bytes.data() + texel * 4, &blue_one, 4);
            }
            else
            {
                const std::array<std::uint8_t, 4> value{0, 0, 255,
                    std::uint8_t(alpha ? 255 : 0)};
                std::memcpy(bytes.data() + texel * 4, value.data(), 4);
            }
        }
        auto command = device->makeCommandBuffer("Preview blue probe upload", &error);
        REQUIRE(command);
        for (unsigned face = 0; face < 6; ++face)
            REQUIRE(result->upload(*command, 0, face, {}, bytes.data(),
                                   4 * result->bytesPerPixel(), &error));
        REQUIRE(command->commitAndWait(&error));
        return result;
    }

    SH coefficients(ALMetalTexture& cube)
    {
        if (!projector)
        {
            projector = ALMetalWorldGBufferRenderer::create(*device, 1, &error);
            INFO(error); REQUIRE(projector);
        }
        ALMetalTextureDescriptor descriptor;
        descriptor.format = ALMetalPixelFormat::rgba32_uint;
        descriptor.width = 4; descriptor.height = 1;
        descriptor.usage = ALMetalTextureUsage::shader_read |
                           ALMetalTextureUsage::shader_write;
        auto destination = ALMetalTexture::create(*device, descriptor, &error);
        REQUIRE(destination);
        std::unique_ptr<ALMetalBuffer> words;
        std::unique_ptr<ALMetalCommandBuffer> pending;
        REQUIRE(projector->projectReflectionProbeSH(
            cube, 0x3f, 0, 4, *destination, 0, words, pending, &error));
        REQUIRE(pending); REQUIRE(pending->waitUntilCompleted(&error));
        SH result{};
        REQUIRE(words->read(0, result.data(), sizeof(result)));
        SH texture_words{};
        REQUIRE(destination->readback(*device, 0, 0, {}, texture_words.data(),
                                      sizeof(texture_words), &error));
        CHECK(result == texture_words);
        // The blue l=0 coefficient occupies the low half of packed word one.
        CHECK((result[1] & 0xffffu) != 0u);
        return result;
    }

    bool render(ALMetalTexture& cube, const SH& sh, bool diffuse, Pixels& pixels)
    {
        environment.radiance_texture = &cube;
        std::memcpy(environment.sh_coefficients, sh.data(), sizeof(sh));
        environment.radiance_scale = diffuse ? 0.f : 1.f;
        environment.irradiance_scale = diffuse ? 1.f : 0.f;
        batch.metallic_factor = diffuse ? 0.f : 1.f;
        error.clear();
        auto result = preview->render(scene, &error);
        INFO(error); CHECK(result);
        if (!result) return false;
        REQUIRE(result->readback(*device, 0, 0, {}, pixels.data(), SIDE * 4, &error));
        for (unsigned pixel = 0; pixel < SIDE * SIDE; ++pixel)
        {
            CHECK(pixels[pixel * 4] == 0);
            CHECK(pixels[pixel * 4 + 1] == 0);
            CHECK(pixels[pixel * 4 + 2] > 0);
            CHECK(pixels[pixel * 4 + 3] == 255);
        }
        return true;
    }
};
}

TEST_CASE("PBR previews consume published HDR and SDR probe formats",
          "[almetal][preview][probe-formats]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto reference = fixture.cube(ALMetalPixelFormat::rgba16_float, true);
        const SH reference_sh = fixture.coefficients(*reference);
        std::array<Pixels, 2> expected;
        REQUIRE(fixture.render(*reference, reference_sh, false, expected[0]));
        REQUIRE(fixture.render(*reference, reference_sh, true, expected[1]));
        for (const auto format : {ALMetalPixelFormat::rgba16_float,
                                 ALMetalPixelFormat::rg11b10_float,
                                 ALMetalPixelFormat::rgba8_unorm})
        {
            CAPTURE(format);
            // No radiance consumer may use the alpha channel: R11 has none.
            auto cube = fixture.cube(format, false);
            const SH sh = fixture.coefficients(*cube);
            CHECK(sh == reference_sh);
            for (unsigned diffuse = 0; diffuse < 2; ++diffuse)
            {
                Pixels actual{};
                if (fixture.render(*cube, sh, diffuse != 0, actual))
                    CHECK(actual == expected[diffuse]);
            }
        }
    }
}

TEST_CASE("PBR previews still reject incompatible probe storage",
          "[almetal][preview][probe-formats]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto wrong_format = fixture.cube(ALMetalPixelFormat::rgba8_unorm_srgb, true);
        fixture.environment.radiance_texture = wrong_format.get();
        CHECK_FALSE(fixture.preview->render(fixture.scene, &fixture.error));
        CHECK(fixture.error == "Metal PBR preview environment is invalid");
        ALMetalTextureDescriptor descriptor;
        descriptor.width = descriptor.height = 4;
        descriptor.format = ALMetalPixelFormat::rgba16_float;
        auto wrong_type = ALMetalTexture::create(*fixture.device, descriptor, &fixture.error);
        REQUIRE(wrong_type);
        fixture.environment.radiance_texture = wrong_type.get();
        fixture.error.clear();
        CHECK_FALSE(fixture.preview->render(fixture.scene, &fixture.error));
        CHECK(fixture.error == "Metal PBR preview environment is invalid");
    }
}
