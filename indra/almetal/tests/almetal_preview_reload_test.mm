/**
 * @file almetal_preview_reload_test.mm
 * @brief Preview shader reload preserves published native texture ownership.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "../almetaldevice.h"
#include "../almetalpbrpreview.h"
#include "../almetalresource.h"
#include "../almetalsculptpreview.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <vector>

namespace
{
// Original1eb44's purge rebuilds the global object/avatar/physics/PBR-alpha
// programs consumed by previews. The Metal owners are independent caches;
// consume the announced revision without taking ownership of published images.
std::vector<std::uint8_t> pixels(const ALMetalDevice& device,
                                const ALMetalTexture& texture)
{
    std::string error;
    std::vector<std::uint8_t> bytes(texture.descriptor().width *
                                    texture.descriptor().height * 4);
    const bool read = texture.readback(device, 0, 0, {}, bytes.data(),
                                       texture.descriptor().width * 4, &error);
    INFO(error); REQUIRE(read);
    return bytes;
}

ALMetalTextureDescriptor destination(ALMetalPixelFormat format)
{
    ALMetalTextureDescriptor descriptor;
    descriptor.width = descriptor.height = 8;
    descriptor.format = format;
    descriptor.storage = ALMetalStorageMode::private_gpu;
    descriptor.usage = ALMetalTextureUsage::shader_read;
    return descriptor;
}

template<class T>
std::unique_ptr<ALMetalBuffer> buffer(const ALMetalDevice& device,
                                     const T& values)
{
    std::string error;
    ALMetalBufferDescriptor descriptor;
    descriptor.length = sizeof(values);
    descriptor.storage = ALMetalStorageMode::shared;
    auto result = ALMetalBuffer::create(device, descriptor, &error);
    INFO(error); REQUIRE(result);
    REQUIRE(result->write(0, &values, sizeof(values), &error));
    return result;
}
}

TEST_CASE("Preview shader revisions are lazy and preserve lit preview outputs",
          "[almetal][shader-reload][preview-reload]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        auto renderer = ALMetalLitPreviewRenderer::create(*device, &error);
        INFO(error); REQUIRE(renderer);
        auto unused = ALMetalLitPreviewRenderer::create(*device, &error);
        INFO(error); REQUIRE(unused);
        const auto before = ALMetalShaderLibrary::builtinReloadGeneration();
        CHECK(renderer->shaderGeneration() == before);
        CHECK(unused->shaderGeneration() == before);

        const std::array<float, 9> positions{-1,-1,.5f, 3,-1,.5f, -1,3,.5f};
        auto vertices = buffer(*device, positions);
        ALMetalLitPreviewBatch batch;
        batch.geometry = ALMetalLitPreviewGeometry::unindexed_flat_triangles;
        batch.vertices = vertices.get(); batch.vertex_count = 3;
        batch.diffuse_color[0] = .25f; batch.diffuse_color[1] = .5f;
        batch.diffuse_color[2] = .75f; batch.diffuse_color[3] = 1.f;
        for (unsigned axis = 0; axis < 4; ++axis)
            batch.modelview_projection[axis * 4 + axis] = 1.f;
        std::array<ALMetalTextureDescriptor, 2> descriptors{
            destination(ALMetalPixelFormat::rgba8_unorm),
            destination(ALMetalPixelFormat::rgba8_unorm_srgb)};
        std::array<std::unique_ptr<ALMetalTexture>, 2> retained;
        std::array<void*, 2> identities{};
        for (unsigned index = 0; index < descriptors.size(); ++index)
        {
            batch.destination_descriptor = &descriptors[index];
            retained[index] = renderer->render(batch, &error);
            INFO(error); REQUIRE(retained[index]);
            identities[index] = retained[index]->nativeHandle();
        }
        // No CPU wait for the old commands before replacing their PSO owner.
        ALMetalShaderLibrary::requestBuiltinReload();
        const auto requested = ALMetalShaderLibrary::builtinReloadGeneration();
        CHECK(requested > before);
        CHECK(renderer->shaderGeneration() == before);
        CHECK(unused->shaderGeneration() == before);
        CHECK_FALSE(renderer->render(ALMetalLitPreviewScene{}, &error));
        CHECK(renderer->shaderGeneration() == before);
        error.clear();

        for (unsigned index = 0; index < descriptors.size(); ++index)
        {
            batch.destination_descriptor = &descriptors[index];
            auto continued = renderer->render(batch, &error);
            INFO(error); REQUIRE(continued);
            CHECK(renderer->shaderGeneration() == requested);
            CHECK(retained[index]->nativeHandle() == identities[index]);
            CHECK(continued->nativeHandle() != identities[index]);
            const auto original = pixels(*device, *retained[index]);
            CHECK(pixels(*device, *continued) == original);
            REQUIRE(original.size() == 8 * 8 * 4);
            for (std::size_t pixel = 0; pixel < original.size(); pixel += 4)
            {
                if (index == 0)
                {
                    CHECK(original[pixel] == 64);
                    CHECK(original[pixel + 1] == 128);
                    CHECK(original[pixel + 2] == 191);
                }
                CHECK(original[pixel + 3] == 255);
            }
        }
        CHECK(unused->shaderGeneration() == before);
        auto newly_used = unused->render(batch, &error); INFO(error); REQUIRE(newly_used);
        CHECK(unused->shaderGeneration() == requested);
        CHECK(pixels(*device, *newly_used) == pixels(*device, *retained[1]));
    }
}

TEST_CASE("PBR preview reload regenerates its LUT and keeps published images live",
          "[almetal][shader-reload][preview-reload]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        auto renderer = ALMetalPBRPreviewRenderer::create(*device, &error);
        INFO(error); REQUIRE(renderer);
        const auto before = renderer->shaderGeneration();
        CHECK(before == ALMetalShaderLibrary::builtinReloadGeneration());
        CHECK(renderer->brdfLutGeneration() == 0);

        // Actual SoA glTF inputs: full-cover triangle and authored red emission
        // provide an exact oracle; a second material exercises the BRDF LUT.
        struct Geometry
        {
            float positions[3][4]{{-1,-1,0,1}, {3,-1,0,1}, {-1,3,0,1}};
            float normals[3][4]{{0,0,1,0}, {0,0,1,0}, {0,0,1,0}};
            float texcoords[3][2]{{.5f,.5f}, {.5f,.5f}, {.5f,.5f}};
            std::uint8_t colors[3][4]{{255,255,255,255},
                                      {255,255,255,255}, {255,255,255,255}};
            alignas(16) float tangents[3][4]{{1,0,0,1}, {1,0,0,1}, {1,0,0,1}};
        } geometry;
        auto vertices = buffer(*device, geometry);
        const std::array<std::uint16_t, 3> triangle{0,1,2};
        auto indices = buffer(*device, triangle);
        const auto texture = [&](ALMetalPixelFormat format,
                                  std::array<std::uint8_t, 4> rgba)
        {
            auto descriptor = destination(format);
            descriptor.width = descriptor.height = 1;
            auto result = ALMetalTexture::create(*device, descriptor, &error);
            INFO(error); REQUIRE(result);
            auto command = device->makeCommandBuffer("Preview reload input", &error);
            REQUIRE(command);
            REQUIRE(result->upload(*command, 0, 0, {}, rgba.data(), 4, &error));
            REQUIRE(command->commitAndWait(&error));
            return result;
        };
        auto base = texture(ALMetalPixelFormat::rgba8_unorm_srgb, {0,0,0,255});
        auto white_base = texture(ALMetalPixelFormat::rgba8_unorm_srgb, {255,255,255,255});
        auto normal = texture(ALMetalPixelFormat::rgba8_unorm, {128,128,255,255});
        auto orm = texture(ALMetalPixelFormat::rgba8_unorm, {0,255,0,255});
        auto metal_orm = texture(ALMetalPixelFormat::rgba8_unorm, {255,255,255,255});
        auto emissive = texture(ALMetalPixelFormat::rgba8_unorm_srgb, {255,0,0,255});
        auto cube_descriptor = destination(ALMetalPixelFormat::rgba16_float);
        cube_descriptor.width = cube_descriptor.height = 1;
        cube_descriptor.type = ALMetalTextureType::texture_cube;
        auto cube = ALMetalTexture::create(*device, cube_descriptor, &error);
        INFO(error); REQUIRE(cube);
        auto upload = device->makeCommandBuffer("Preview reload environment", &error);
        REQUIRE(upload);
        const std::array<std::uint16_t, 4> white{0x3c00,0x3c00,0x3c00,0x3c00};
        for (unsigned face = 0; face < 6; ++face)
            REQUIRE(cube->upload(*upload, 0, face, {}, white.data(), 8, &error));
        REQUIRE(upload->commitAndWait(&error));

        ALMetalPBRPreviewBatch batch;
        batch.vertices = vertices.get(); batch.vertex_count = 3;
        batch.position_offset = offsetof(Geometry, positions);
        batch.normal_offset = offsetof(Geometry, normals);
        batch.texcoord_offset = offsetof(Geometry, texcoords);
        batch.color_offset = offsetof(Geometry, colors);
        batch.tangent_offset = offsetof(Geometry, tangents);
        batch.indices = indices.get(); batch.index_count = 3;
        batch.base_color_texture = base.get(); batch.normal_texture = normal.get();
        batch.orm_texture = orm.get(); batch.emissive_texture = emissive.get();
        batch.emissive_color[0] = batch.emissive_color[1] = batch.emissive_color[2] = 1.f;
        batch.sun_color[0] = batch.sun_color[1] = batch.sun_color[2] = 0.f;
        batch.metallic_factor = 0.f; batch.roughness_factor = 1.f;
        batch.double_sided = true;
        for (unsigned axis = 0; axis < 4; ++axis)
        {
            batch.modelview_projection[axis * 4 + axis] = 1.f;
            batch.modelview[axis * 4 + axis] = 1.f;
            if (axis < 3) batch.normal_matrix[axis][axis] = 1.f;
        }
        batch.modelview[14] = -2.f;
        auto descriptor = destination(ALMetalPixelFormat::rgba8_unorm);
        ALMetalPBRPreviewEnvironment environment;
        environment.radiance_texture = cube.get();
        environment.radiance_scale = 0.f;
        ALMetalPBRPreviewScene scene;
        scene.batches = &batch; scene.batch_count = 1;
        scene.destination_descriptor = &descriptor; scene.environment = &environment;
        scene.tonemap_enabled = false;
        auto retained = renderer->render(scene, &error); INFO(error); REQUIRE(retained);
        auto* identity = retained->nativeHandle();
        CHECK(renderer->brdfLutGeneration() == before);
        environment.radiance_scale = 1.f;
        batch.base_color_texture = white_base.get(); batch.orm_texture = metal_orm.get();
        batch.metallic_factor = 1.f;
        batch.emissive_color[0] = batch.emissive_color[1] = batch.emissive_color[2] = 0.f;
        auto retained_metal = renderer->render(scene, &error);
        INFO(error); REQUIRE(retained_metal);
        auto* metal_identity = retained_metal->nativeHandle();
        // Keep the old submitted shade command and its LUT alive across reload.
        ALMetalShaderLibrary::requestBuiltinReload();
        const auto requested = ALMetalShaderLibrary::builtinReloadGeneration();
        CHECK(renderer->shaderGeneration() == before);
        CHECK(renderer->brdfLutGeneration() == before);
        auto continued = renderer->render(scene, &error); INFO(error); REQUIRE(continued);
        CHECK(renderer->shaderGeneration() == requested);
        CHECK(renderer->brdfLutGeneration() == requested);
        CHECK(retained->nativeHandle() == identity);
        CHECK(retained_metal->nativeHandle() == metal_identity);
        const auto authored = pixels(*device, *retained);
        const auto metal = pixels(*device, *retained_metal);
        CHECK(pixels(*device, *continued) == metal);
        for (std::size_t pixel = 0; pixel < authored.size(); pixel += 4)
        {
            CHECK(authored[pixel] == 255); CHECK(authored[pixel + 1] == 0);
            CHECK(authored[pixel + 2] == 0); CHECK(authored[pixel + 3] == 255);
            CHECK(metal[pixel] > 0); CHECK(metal[pixel + 1] > 0);
            CHECK(metal[pixel + 2] > 0); CHECK(metal[pixel + 3] == 255);
        }
        // Ordinary frames do not reload/recompute the already submitted LUT.
        auto repeated = renderer->render(scene, &error); INFO(error); REQUIRE(repeated);
        CHECK(renderer->shaderGeneration() == requested);
        CHECK(renderer->brdfLutGeneration() == requested);
        CHECK(pixels(*device, *repeated) == metal);
    }
}
