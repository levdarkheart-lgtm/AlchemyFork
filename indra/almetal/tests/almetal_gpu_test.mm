/**
 * @file almetal_gpu_test.mm
 * @brief Real-device tests for Metal commands and SDL drawable presentation.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "../almetaldevice.h"
#include "../almetalframe.h"
#include "../almetalpbrpreview.h"
#include "../almetalpipeline.h"
#include "../almetalresource.h"
#include "../almetalsculptpreview.h"
#include "../almetalsurface.h"
#include "../almetalterrainpaint.h"
#include "../almetaltextureops.h"
#include "../almetaluirenderer.h"
#include "../almetalworld.h"

#import <Metal/Metal.h>

#include <SDL3/SDL.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
    id<MTLDevice> nativeDevice(const ALMetalDevice& device)
    {
        return (__bridge id<MTLDevice>)device.nativeHandle();
    }

    id<MTLCommandBuffer> nativeCommand(const ALMetalCommandBuffer& command)
    {
        return (__bridge id<MTLCommandBuffer>)command.nativeHandle();
    }

    struct SDLVideoScope
    {
        SDLVideoScope()
        {
            REQUIRE(SDL_InitSubSystem(SDL_INIT_VIDEO));
        }
        ~SDLVideoScope()
        {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
    };

    struct SDLWindowDeleter
    {
        void operator()(SDL_Window* window) const
        {
            if (window)
            {
                SDL_DestroyWindow(window);
            }
        }
    };
}

TEST_CASE("almetal: the selected device is built-in unified-memory Apple Silicon", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        CHECK_FALSE(device->info().name.empty());
        CHECK(device->info().registry_id != 0);
        CHECK(device->info().unified_memory);
        CHECK_FALSE(device->info().removable);
        CHECK_FALSE(device->info().headless);
        CHECK(device->info().recommended_working_set_size > 0);
        CHECK(device->info().max_buffer_length > 0);
        CHECK(device->info().highest_apple_family >= 1);
        CHECK(device->nativeCommandQueue() != nullptr);
        CHECK(device->waitUntilIdle(&error));
        INFO(error);
    }
}

#ifdef AL_METAL_BUILTIN_LIBRARY_PATH
TEST_CASE("almetal: offline built-in library exposes every production entry point", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        auto library = ALMetalShaderLibrary::loadFile(
            *device, AL_METAL_BUILTIN_LIBRARY_PATH, &error);
        INFO(error);
        REQUIRE(library);

        constexpr std::array functions{
            "almetal_test_vertex",
            "almetal_test_fragment",
            "almetal_test_compute",
            "almetal_test_sample_texture",
            "almetal_test_sample_cube_array",
            "almetal_resample_vertex",
            "almetal_resample_fragment",
            "almetal_world_simple_vertex",
            "almetal_world_simple_skinned_vertex",
            "almetal_world_bump_vertex",
            "almetal_world_bump_skinned_vertex",
            "almetal_world_material_vertex",
            "almetal_world_material_skinned_vertex",
            "almetal_world_pbr_vertex",
            "almetal_world_pbr_skinned_vertex",
            "almetal_world_avatar_rigid_vertex",
            "almetal_world_avatar_skinned_vertex",
            "almetal_world_avatar_cloth_skinned_vertex",
            "almetal_world_impostor_vertex",
            "almetal_world_coverage_vertex",
            "almetal_world_simple_fragment",
            "almetal_world_alpha_mask_fragment",
            "almetal_world_bump_fragment",
            "almetal_world_material_fragment",
            "almetal_world_pbr_fragment",
            "almetal_world_fullbright_fragment",
            "almetal_world_shiny_vertex",
            "almetal_world_shiny_skinned_vertex",
            "almetal_world_shiny_fragment",
            "almetal_world_legacy_alpha_vertex",
            "almetal_world_legacy_alpha_skinned_vertex",
            "almetal_world_legacy_alpha_fragment",
            "almetal_world_emboss_vertex",
            "almetal_world_emboss_skinned_vertex",
            "almetal_world_emboss_fragment",
            "almetal_world_water_exclusion_vertex",
            "almetal_world_water_exclusion_fragment",
            "almetal_world_water_haze_field_vertex",
            "almetal_world_water_haze_fullscreen_vertex",
            "almetal_world_water_haze_fragment",
            "almetal_world_pre_water_fullbright_fragment",
            "almetal_world_pre_water_simple_fragment",
            "almetal_world_pre_water_simple_shadow_fragment",
            "almetal_world_pre_water_material_fragment",
            "almetal_world_pre_water_pbr_fragment",
            "almetal_world_legacy_glow_vertex",
            "almetal_world_legacy_glow_skinned_vertex",
            "almetal_world_legacy_glow_fragment",
            "almetal_world_pbr_glow_vertex",
            "almetal_world_pbr_glow_skinned_vertex",
            "almetal_world_pbr_glow_fragment",
            "almetal_world_flat_alpha_fragment",
            "almetal_world_pbr_alpha_fragment",
            "almetal_world_impostor_fragment",
            "almetal_world_coverage_fragment",
            "almetal_world_terrain_vertex",
            "almetal_world_terrain_fragment",
            "almetal_world_pbr_terrain_fragment",
            "almetal_world_local_light_vertex",
            "almetal_world_local_light_fragment",
            "almetal_world_projector_light_fragment",
            "almetal_world_lighting_composition_fragment",
            "almetal_world_light_map_fragment",
            "almetal_world_light_map_blur_fragment",
            "almetal_world_hdri_sky_vertex",
            "almetal_world_hdri_sky_fragment",
            "almetal_world_windlight_sky_vertex",
            "almetal_world_windlight_sky_fragment",
            "almetal_world_star_vertex",
            "almetal_world_star_fragment",
            "almetal_world_heavenly_body_vertex",
            "almetal_world_heavenly_body_fragment",
            "almetal_world_meteor_vertex",
            "almetal_world_meteor_fragment",
            "almetal_world_aurora_vertex",
            "almetal_world_aurora_fragment",
            "almetal_world_cloud_vertex",
            "almetal_world_cloud_fragment",
            "almetal_world_base_light_fragment",
            "almetal_terrain_paint_vertex",
            "almetal_terrain_paint_fragment",
            "almetal_lit_preview_vertex",
            "almetal_lit_preview_flat_vertex",
            "almetal_lit_preview_skinned_vertex",
            "almetal_lit_preview_fragment",
            "almetal_lit_preview_textured_vertex",
            "almetal_lit_preview_skinned_textured_vertex",
            "almetal_lit_preview_textured_fragment",
            "almetal_lit_preview_avatar_vertex",
            "almetal_lit_preview_avatar_textured_vertex",
            "almetal_lit_preview_avatar_unlit_textured_vertex",
            "almetal_lit_preview_avatar_skinned_vertex",
            "almetal_lit_preview_avatar_skinned_textured_vertex",
            "almetal_lit_preview_avatar_skinned_unlit_textured_vertex",
            "almetal_lit_preview_avatar_skinned_unlit_cloth_textured_vertex",
            "almetal_lit_preview_deferred_rigged_textured_vertex",
            "almetal_lit_preview_deferred_rigged_fragment",
            "almetal_lit_preview_legacy_material_rigged_fragment",
            "almetal_lit_preview_pbr_rigged_textured_vertex",
            "almetal_lit_preview_pbr_blend_rigged_fragment",
            "almetal_lit_preview_pbr_glow_rigged_textured_vertex",
            "almetal_lit_preview_pbr_glow_rigged_fragment",
            "almetal_lit_preview_fullbright_mask_rigged_textured_vertex",
            "almetal_lit_preview_fullbright_rigged_fragment",
            "almetal_lit_preview_fullbright_mask_rigged_fragment",
            "almetal_lit_preview_avatar_fragment",
            "almetal_lit_preview_avatar_unlit_fragment",
            "almetal_lit_preview_edge_u16_vertex",
            "almetal_lit_preview_edge_u32_vertex",
            "almetal_lit_preview_skinned_edge_u16_vertex",
            "almetal_lit_preview_skinned_edge_u32_vertex",
            "almetal_ui_vertex",
            "almetal_ui_fragment",
            "almetal_alpha_mask_fragment",
            "almetal_alpha_mask_solid_fragment",
            "almetal_solid_vertex",
            "almetal_solid_fragment",
            "almetal_pbr_preview_vertex",
            "almetal_pbr_preview_fragment",
            "almetal_pbr_preview_brdf_lut",
            "almetal_pbr_preview_exposure",
            "almetal_pbr_preview_post_fragment",
            "almetal_pbr_preview_bloom_extract_fragment",
            "almetal_pbr_preview_bloom_downsample_fragment",
            "almetal_pbr_preview_bloom_upsample_fragment",
            "almetal_pbr_preview_cross_filter_fragment",
        };
        for (const char* function : functions)
        {
            CAPTURE(function);
            CHECK(library->hasFunction(function));
        }
    }
}
#endif

TEST_CASE("almetal: a blit command copies shared storage and reports completion", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        constexpr std::size_t size = 4096;
        std::array<std::uint8_t, size> expected{};
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            expected[i] = static_cast<std::uint8_t>((i * 131u + 17u) & 0xffu);
        }

        id<MTLBuffer> source = [nativeDevice(*device)
            newBufferWithBytes:expected.data()
                      length:expected.size()
                     options:MTLResourceStorageModeShared];
        id<MTLBuffer> destination = [nativeDevice(*device)
            newBufferWithLength:expected.size()
                         options:MTLResourceStorageModeShared];
        REQUIRE(source);
        REQUIRE(destination);
        std::memset(destination.contents, 0, size);

        auto command = device->makeCommandBuffer("Alchemy Metal blit test", &error);
        INFO(error);
        REQUIRE(command);
        id<MTLBlitCommandEncoder> encoder = [nativeCommand(*command) blitCommandEncoder];
        REQUIRE(encoder);
        [encoder copyFromBuffer:source
                   sourceOffset:0
                       toBuffer:destination
              destinationOffset:0
                           size:size];
        [encoder endEncoding];

        CHECK(command->commitAndWait(&error));
        INFO(error);
        CHECK(command->status() == ALMetalCommandStatus::completed);
        CHECK(std::memcmp(destination.contents, expected.data(), size) == 0);
    }
}

TEST_CASE("almetal: a render pass clears a texture and CPU readback observes it", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        MTLTextureDescriptor* description = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                      width:8
                                     height:8
                                  mipmapped:NO];
        description.storageMode = MTLStorageModeShared;
        description.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> texture = [nativeDevice(*device) newTextureWithDescriptor:description];
        REQUIRE(texture);

        auto command = device->makeCommandBuffer("Alchemy Metal clear/readback test", &error);
        INFO(error);
        REQUIRE(command);
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.25, 0.5, 0.75, 1.0);
        id<MTLRenderCommandEncoder> encoder = [nativeCommand(*command) renderCommandEncoderWithDescriptor:pass];
        REQUIRE(encoder);
        [encoder endEncoding];
        CHECK(command->commitAndWait(&error));
        INFO(error);

        std::array<std::uint8_t, 8 * 8 * 4> pixels{};
        [texture getBytes:pixels.data()
              bytesPerRow:8 * 4
               fromRegion:MTLRegionMake2D(0, 0, 8, 8)
              mipmapLevel:0];
        // BGRA storage for the requested (R,G,B,A) clear, with one LSB of
        // tolerance for the implementation's UNORM rounding mode.
        CHECK(std::abs(static_cast<int>(pixels[0]) - 191) <= 1);
        CHECK(std::abs(static_cast<int>(pixels[1]) - 128) <= 1);
        CHECK(std::abs(static_cast<int>(pixels[2]) - 64) <= 1);
        CHECK(pixels[3] == 255);
    }
}

TEST_CASE("almetal: typed shared and private buffers preserve staged ranges", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        ALMetalBufferDescriptor shared_descriptor;
        shared_descriptor.length = 256;
        shared_descriptor.storage = ALMetalStorageMode::shared;
        shared_descriptor.label = "Alchemy shared buffer test";
        auto shared = ALMetalBuffer::create(*device, shared_descriptor, &error);
        INFO(error);
        REQUIRE(shared);
        REQUIRE(shared->mappedBytes());

        std::array<std::uint8_t, 31> small_source{};
        for (std::size_t i = 0; i < small_source.size(); ++i)
        {
            small_source[i] = static_cast<std::uint8_t>(i * 7u + 3u);
        }
        REQUIRE(shared->write(17, small_source.data(), small_source.size(), &error));
        std::array<std::uint8_t, 31> small_result{};
        REQUIRE(shared->read(17, small_result.data(), small_result.size(), &error));
        CHECK(small_result == small_source);

        ALMetalBufferDescriptor private_descriptor;
        private_descriptor.length = 8192;
        private_descriptor.storage = ALMetalStorageMode::private_gpu;
        private_descriptor.label = "Alchemy private buffer test";
        auto private_buffer = ALMetalBuffer::create(*device, private_descriptor, &error);
        INFO(error);
        REQUIRE(private_buffer);
        CHECK(private_buffer->mappedBytes() == nullptr);

        std::array<std::uint8_t, 1024> source{};
        for (std::size_t i = 0; i < source.size(); ++i)
        {
            source[i] = static_cast<std::uint8_t>((i * 29u + 11u) & 0xffu);
        }
        auto upload = device->makeCommandBuffer("Alchemy private buffer upload", &error);
        INFO(error);
        REQUIRE(upload);
        REQUIRE(private_buffer->upload(*upload, 257, source.data(), source.size(), &error));
        REQUIRE(upload->commitAndWait(&error));
        INFO(error);

        std::array<std::uint8_t, 1024> result{};
        REQUIRE(private_buffer->readback(*device, 257, result.data(), result.size(), &error));
        INFO(error);
        CHECK(result == source);
        CHECK_FALSE(private_buffer->readback(*device, 8000, result.data(), result.size(), &error));
    }
}

TEST_CASE("almetal: private texture transfer handles unaligned CPU row pitches", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        constexpr std::uint32_t width = 17;
        constexpr std::uint32_t height = 9;
        constexpr std::size_t source_pitch = width * 4 + 12;
        constexpr std::size_t result_pitch = width * 4 + 20;
        std::vector<std::uint8_t> source(source_pitch * height, 0xcd);
        for (std::uint32_t y = 0; y < height; ++y)
        {
            for (std::uint32_t x = 0; x < width; ++x)
            {
                std::uint8_t* pixel = source.data() + y * source_pitch + x * 4;
                pixel[0] = static_cast<std::uint8_t>(x * 9u);
                pixel[1] = static_cast<std::uint8_t>(y * 17u);
                pixel[2] = static_cast<std::uint8_t>(x + y * 3u);
                pixel[3] = 255;
            }
        }

        ALMetalTextureDescriptor descriptor;
        descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.width = width;
        descriptor.height = height;
        descriptor.storage = ALMetalStorageMode::private_gpu;
        descriptor.usage = ALMetalTextureUsage::shader_read | ALMetalTextureUsage::shader_write;
        descriptor.label = "Alchemy odd-pitch private texture";
        auto texture = ALMetalTexture::create(*device, descriptor, &error);
        INFO(error);
        REQUIRE(texture);

        auto command = device->makeCommandBuffer("Alchemy private texture upload", &error);
        INFO(error);
        REQUIRE(command);
        REQUIRE(texture->upload(*command, 0, 0, {}, source.data(), source_pitch, &error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);

        std::vector<std::uint8_t> result(result_pitch * height, 0xee);
        REQUIRE(texture->readback(*device, 0, 0, {}, result.data(), result_pitch, &error));
        INFO(error);
        for (std::uint32_t y = 0; y < height; ++y)
        {
            CHECK(std::memcmp(result.data() + y * result_pitch,
                              source.data() + y * source_pitch,
                              width * 4) == 0);
            CHECK(result[y * result_pitch + width * 4] == 0xee);
        }
    }
}

TEST_CASE("almetal: texture swizzles and generated mipmaps survive private upload", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        ALMetalTextureDescriptor descriptor;
        descriptor.format = ALMetalPixelFormat::r8_unorm;
        descriptor.width = 4;
        descriptor.height = 4;
        descriptor.mipmap_level_count = 3;
        descriptor.storage = ALMetalStorageMode::private_gpu;
        descriptor.swizzle = {
            ALMetalTextureSwizzle::zero,
            ALMetalTextureSwizzle::zero,
            ALMetalTextureSwizzle::zero,
            ALMetalTextureSwizzle::red,
        };
        descriptor.label = "Alchemy alpha swizzle texture";
        auto texture = ALMetalTexture::create(*device, descriptor, &error);
        INFO(error);
        REQUIRE(texture);
        CHECK(texture->blockWidth() == 1);
        CHECK(texture->blockHeight() == 1);
        CHECK(texture->bytesPerBlock() == 1);
        CHECK_FALSE(texture->isCompressed());

        auto library = ALMetalShaderLibrary::loadBuiltin(*device, &error);
        INFO(error);
        REQUIRE(library);
        ALMetalComputePipelineDescriptor pipeline_descriptor;
        pipeline_descriptor.library = library.get();
        pipeline_descriptor.function = "almetal_test_sample_texture";
        pipeline_descriptor.label = "Alchemy texture swizzle pipeline";
        auto pipeline = ALMetalComputePipeline::create(*device, pipeline_descriptor, &error);
        INFO(error);
        REQUIRE(pipeline);

        ALMetalBufferDescriptor result_descriptor;
        result_descriptor.length = sizeof(float) * 4;
        result_descriptor.storage = ALMetalStorageMode::shared;
        result_descriptor.label = "Alchemy texture sample result";
        auto result = ALMetalBuffer::create(*device, result_descriptor, &error);
        INFO(error);
        REQUIRE(result);

        const std::array<std::uint8_t, 16> source = {
            64, 64, 64, 64,
            64, 64, 64, 64,
            64, 64, 64, 64,
            64, 64, 64, 64,
        };
        auto command = device->makeCommandBuffer("Alchemy swizzle and mip test", &error);
        INFO(error);
        REQUIRE(command);
        REQUIRE(texture->upload(*command, 0, 0, {}, source.data(), 4, &error));
        REQUIRE(texture->generateMipmaps(*command, &error));

        auto encoder = ALMetalComputeEncoder::begin(*command, "Alchemy texture sample", &error);
        INFO(error);
        REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline, &error));
        REQUIRE(encoder->setTexture(0, texture.get(), &error));
        REQUIRE(encoder->setBuffer(0, *result, 0, &error));
        REQUIRE(encoder->dispatch(1, 1, 1, 1, 1, 1, &error));
        REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);

        std::array<float, 4> sample{};
        REQUIRE(result->read(0, sample.data(), sizeof(sample), &error));
        CHECK(sample[0] == 0.0f);
        CHECK(sample[1] == 0.0f);
        CHECK(sample[2] == 0.0f);
        CHECK(std::abs(sample[3] - (64.0f / 255.0f)) < 0.001f);

        std::uint8_t final_mip = 0;
        REQUIRE(texture->readback(*device, 2, 0, {}, &final_mip, 1, &error));
        INFO(error);
        CHECK(final_mip == 64);
    }
}

TEST_CASE("almetal: a cube-array texture addresses every cube face and samples one array element", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        ALMetalTextureDescriptor descriptor;
        descriptor.type = ALMetalTextureType::texture_cube_array;
        descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.width = 1;
        descriptor.height = 1;
        descriptor.array_length = 2;
        descriptor.storage = ALMetalStorageMode::private_gpu;
        descriptor.usage = ALMetalTextureUsage::shader_read;
        descriptor.label = "Alchemy cube-array resource test";
        auto texture = ALMetalTexture::create(*device, descriptor, &error);
        INFO(error);
        REQUIRE(texture);

        auto upload = device->makeCommandBuffer(
            "Alchemy cube-array resource upload", &error);
        INFO(error);
        REQUIRE(upload);
        for (std::uint32_t slice = 0; slice < 12; ++slice)
        {
            const std::array<std::uint8_t, 4> pixel{{
                static_cast<std::uint8_t>(slice * 17u),
                static_cast<std::uint8_t>(255u - slice * 13u),
                static_cast<std::uint8_t>(slice * 7u),
                255,
            }};
            REQUIRE(texture->upload(
                *upload, 0, slice, {}, pixel.data(), 4, &error));
        }
        REQUIRE(upload->commitAndWait(&error));
        INFO(error);

        std::array<std::uint8_t, 4> face{};
        REQUIRE(texture->readback(
            *device, 0, 10, {}, face.data(), 4, &error));
        INFO(error);
        CHECK(face[0] == 170);
        CHECK(face[1] == 125);
        CHECK(face[2] == 70);
        CHECK(face[3] == 255);

        auto library = ALMetalShaderLibrary::loadBuiltin(*device, &error);
        INFO(error);
        REQUIRE(library);
        ALMetalComputePipelineDescriptor pipeline_descriptor;
        pipeline_descriptor.library = library.get();
        pipeline_descriptor.function = "almetal_test_sample_cube_array";
        pipeline_descriptor.label = "Alchemy cube-array sample pipeline";
        auto pipeline = ALMetalComputePipeline::create(
            *device, pipeline_descriptor, &error);
        INFO(error);
        REQUIRE(pipeline);

        ALMetalBufferDescriptor result_descriptor;
        result_descriptor.length = sizeof(float) * 4;
        result_descriptor.storage = ALMetalStorageMode::shared;
        result_descriptor.label = "Alchemy cube-array sample result";
        auto result = ALMetalBuffer::create(
            *device, result_descriptor, &error);
        INFO(error);
        REQUIRE(result);

        auto sample = device->makeCommandBuffer(
            "Alchemy cube-array resource sample", &error);
        INFO(error);
        REQUIRE(sample);
        auto encoder = ALMetalComputeEncoder::begin(
            *sample, "Alchemy cube-array sample encoder", &error);
        INFO(error);
        REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline, &error));
        REQUIRE(encoder->setTexture(0, texture.get(), &error));
        REQUIRE(encoder->setBuffer(0, *result, 0, &error));
        REQUIRE(encoder->dispatch(1, 1, 1, 1, 1, 1, &error));
        REQUIRE(encoder->end(&error));
        REQUIRE(sample->commitAndWait(&error));
        INFO(error);

        std::array<float, 4> sampled{};
        REQUIRE(result->read(
            0, sampled.data(), sizeof(sampled), &error));
        CHECK(std::abs(sampled[0] - 170.0f / 255.0f) < 0.001f);
        CHECK(std::abs(sampled[1] - 125.0f / 255.0f) < 0.001f);
        CHECK(std::abs(sampled[2] - 70.0f / 255.0f) < 0.001f);
        CHECK(sampled[3] == 1.0f);

        auto invalid_upload = device->makeCommandBuffer(
            "Alchemy invalid cube-array upload", &error);
        INFO(error);
        REQUIRE(invalid_upload);
        CHECK_FALSE(texture->upload(
            *invalid_upload, 0, 12, {}, face.data(), 4, &error));
        CHECK(error == "Metal texture slice is out of range");

        ALMetalTextureDescriptor invalid_descriptor = descriptor;
        invalid_descriptor.type = ALMetalTextureType::texture_cube;
        CHECK_FALSE(ALMetalTexture::create(
            *device, invalid_descriptor, &error));
        CHECK(error ==
              "Only an array Metal texture accepts array_length greater than one");
    }
}

TEST_CASE("almetal: a typed texture blit copies one mip into a new mip chain", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        ALMetalTextureDescriptor source_descriptor;
        source_descriptor.format = ALMetalPixelFormat::r8_unorm;
        source_descriptor.width = 4;
        source_descriptor.height = 4;
        source_descriptor.mipmap_level_count = 3;
        source_descriptor.storage = ALMetalStorageMode::private_gpu;
        source_descriptor.usage = ALMetalTextureUsage::shader_read;
        source_descriptor.label = "Alchemy texture-copy source";
        auto source = ALMetalTexture::create(*device, source_descriptor, &error);
        INFO(error);
        REQUIRE(source);

        ALMetalTextureDescriptor destination_descriptor = source_descriptor;
        destination_descriptor.width = 2;
        destination_descriptor.height = 2;
        destination_descriptor.mipmap_level_count = 2;
        destination_descriptor.label = "Alchemy texture-copy destination";
        auto destination = ALMetalTexture::create(*device, destination_descriptor, &error);
        INFO(error);
        REQUIRE(destination);

        const std::array<std::uint8_t, 4> expected = {11, 22, 33, 44};
        auto command = device->makeCommandBuffer("Alchemy typed texture-copy test", &error);
        INFO(error);
        REQUIRE(command);
        REQUIRE(source->upload(*command, 1, 0, {}, expected.data(), 2, &error));
        ALMetalTextureCopy copy;
        copy.source_mip_level = 1;
        REQUIRE(destination->copyFrom(*command, *source, copy, &error));
        REQUIRE(destination->generateMipmaps(*command, &error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);

        std::array<std::uint8_t, 4> result{};
        REQUIRE(destination->readback(*device, 0, 0, {}, result.data(), 2, &error));
        INFO(error);
        CHECK(result == expected);

        ALMetalTextureDescriptor mismatched_descriptor = destination_descriptor;
        mismatched_descriptor.format = ALMetalPixelFormat::rg8_unorm;
        auto mismatched = ALMetalTexture::create(*device, mismatched_descriptor, &error);
        INFO(error);
        REQUIRE(mismatched);
        auto rejected = device->makeCommandBuffer("Alchemy rejected texture-copy test", &error);
        REQUIRE(rejected);
        CHECK_FALSE(mismatched->copyFrom(*rejected, *source, {}, &error));
        CHECK(error == "Metal texture copy requires matching pixel formats");
    }
}

TEST_CASE("almetal: bilinear texture resampling stays entirely on the GPU", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        ALMetalTextureDescriptor source_descriptor;
        source_descriptor.format = ALMetalPixelFormat::r8_unorm;
        source_descriptor.width = 4;
        source_descriptor.height = 4;
        source_descriptor.storage = ALMetalStorageMode::private_gpu;
        source_descriptor.usage = ALMetalTextureUsage::shader_read;
        source_descriptor.label = "Alchemy texture-resample source";
        auto source = ALMetalTexture::create(*device, source_descriptor, &error);
        INFO(error);
        REQUIRE(source);

        ALMetalTextureDescriptor destination_descriptor = source_descriptor;
        destination_descriptor.width = 2;
        destination_descriptor.height = 2;
        destination_descriptor.usage = ALMetalTextureUsage::shader_read |
                                       ALMetalTextureUsage::render_target;
        destination_descriptor.label = "Alchemy texture-resample destination";
        auto destination = ALMetalTexture::create(*device, destination_descriptor, &error);
        INFO(error);
        REQUIRE(destination);

        // Each output pixel samples the center of one constant 2x2 quadrant,
        // so the exact expected bytes are independent of UNORM rounding.
        const std::array<std::uint8_t, 16> source_pixels = {
             16,  16,  64,  64,
             16,  16,  64,  64,
            128, 128, 240, 240,
            128, 128, 240, 240,
        };
        const std::array<std::uint8_t, 4> expected = {16, 64, 128, 240};

        auto resampler = ALMetalTextureResampler::create(*device, &error);
        INFO(error);
        REQUIRE(resampler);
        auto command = device->makeCommandBuffer(
            "Alchemy bilinear texture-resample test", &error);
        INFO(error);
        REQUIRE(command);
        REQUIRE(source->upload(
            *command, 0, 0, {}, source_pixels.data(), 4, &error));
        REQUIRE(resampler->encode(*command, *source, *destination, &error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);

        std::array<std::uint8_t, 4> result{};
        REQUIRE(destination->readback(
            *device, 0, 0, {}, result.data(), 2, &error));
        INFO(error);
        CHECK(result == expected);
        CHECK(ALMetalTextureResampler::supports(
            ALMetalPixelFormat::rgba8_unorm_srgb));

        ALMetalTextureDescriptor srgb_source_descriptor;
        srgb_source_descriptor.format = ALMetalPixelFormat::rgba8_unorm_srgb;
        srgb_source_descriptor.width = 4;
        srgb_source_descriptor.height = 4;
        srgb_source_descriptor.storage = ALMetalStorageMode::private_gpu;
        srgb_source_descriptor.usage = ALMetalTextureUsage::shader_read;
        srgb_source_descriptor.label = "Alchemy raw-sRGB resample source";
        auto srgb_source = ALMetalTexture::create(
            *device, srgb_source_descriptor, &error);
        INFO(error);
        REQUIRE(srgb_source);

        ALMetalTextureDescriptor srgb_destination_descriptor = srgb_source_descriptor;
        srgb_destination_descriptor.width = 2;
        srgb_destination_descriptor.height = 2;
        srgb_destination_descriptor.usage = ALMetalTextureUsage::shader_read |
                                            ALMetalTextureUsage::render_target;
        srgb_destination_descriptor.label = "Alchemy raw-sRGB resample destination";
        auto srgb_destination = ALMetalTexture::create(
            *device, srgb_destination_descriptor, &error);
        INFO(error);
        REQUIRE(srgb_destination);

        std::array<std::uint8_t, 4 * 4 * 4> srgb_pixels{};
        for (std::size_t y = 0; y < 4; ++y)
        {
            for (std::size_t x = 0; x < 4; ++x)
            {
                const std::uint8_t value = (x & 1u) ? 255 : 0;
                const std::size_t offset = (y * 4 + x) * 4;
                srgb_pixels[offset + 0] = value;
                srgb_pixels[offset + 1] = value;
                srgb_pixels[offset + 2] = value;
                srgb_pixels[offset + 3] = 255;
            }
        }
        auto srgb_command = device->makeCommandBuffer(
            "Alchemy encoded-sRGB texture-resample test", &error);
        INFO(error);
        REQUIRE(srgb_command);
        REQUIRE(srgb_source->upload(
            *srgb_command, 0, 0, {}, srgb_pixels.data(), 4 * 4, &error));
        REQUIRE(resampler->encode(
            *srgb_command, *srgb_source, *srgb_destination, &error));
        REQUIRE(srgb_command->commitAndWait(&error));
        INFO(error);

        std::array<std::uint8_t, 2 * 2 * 4> srgb_result{};
        REQUIRE(srgb_destination->readback(
            *device, 0, 0, {}, srgb_result.data(), 2 * 4, &error));
        INFO(error);
        for (std::size_t pixel = 0; pixel < 4; ++pixel)
        {
            CHECK(std::abs(static_cast<int>(srgb_result[pixel * 4 + 0]) - 128) <= 1);
            CHECK(std::abs(static_cast<int>(srgb_result[pixel * 4 + 1]) - 128) <= 1);
            CHECK(std::abs(static_cast<int>(srgb_result[pixel * 4 + 2]) - 128) <= 1);
            CHECK(srgb_result[pixel * 4 + 3] == 255);
        }
    }
}

TEST_CASE("almetal: terrain paint bake consumes indexed viewer buffers and generates mips", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto renderer = ALMetalTerrainPaintRenderer::create(*device, &error);
        INFO(error);
        REQUIRE(renderer);

        constexpr std::size_t position_offset = 0;
        constexpr std::size_t texcoord_offset = sizeof(float) * 4 * 4;
        std::array<std::uint8_t, texcoord_offset + sizeof(float) * 2 * 4> vertex_bytes{};
        const float positions[4][4] = {
            {-1.f, -1.f, 0.f, 1.f},
            { 1.f, -1.f, 0.f, 1.f},
            {-1.f,  1.f, 0.f, 1.f},
            { 1.f,  1.f, 0.f, 1.f},
        };
        const float texcoords[4][2] = {
            {0.5f, 0.5f}, {0.5f, 0.5f},
            {0.5f, 0.5f}, {0.5f, 0.5f},
        };
        std::memcpy(vertex_bytes.data() + position_offset, positions, sizeof(positions));
        std::memcpy(vertex_bytes.data() + texcoord_offset, texcoords, sizeof(texcoords));

        ALMetalBufferDescriptor vertex_descriptor;
        vertex_descriptor.length = vertex_bytes.size();
        vertex_descriptor.storage = ALMetalStorageMode::shared;
        vertex_descriptor.label = "Alchemy terrain paint vertex test";
        auto vertices = ALMetalBuffer::create(*device, vertex_descriptor, &error);
        INFO(error);
        REQUIRE(vertices);
        REQUIRE(vertices->write(0, vertex_bytes.data(), vertex_bytes.size(), &error));

        const std::array<std::uint32_t, 6> index_values = {0, 1, 2, 1, 3, 2};
        ALMetalBufferDescriptor index_descriptor;
        index_descriptor.length = sizeof(index_values);
        index_descriptor.storage = ALMetalStorageMode::shared;
        index_descriptor.label = "Alchemy terrain paint index test";
        auto indices = ALMetalBuffer::create(*device, index_descriptor, &error);
        INFO(error);
        REQUIRE(indices);
        REQUIRE(indices->write(0, index_values.data(), sizeof(index_values), &error));

        ALMetalTextureDescriptor alpha_descriptor;
        alpha_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        alpha_descriptor.width = 1;
        alpha_descriptor.height = 1;
        alpha_descriptor.storage = ALMetalStorageMode::private_gpu;
        alpha_descriptor.usage = ALMetalTextureUsage::shader_read;
        alpha_descriptor.label = "Alchemy terrain alpha-ramp test";
        auto alpha_ramp = ALMetalTexture::create(*device, alpha_descriptor, &error);
        INFO(error);
        REQUIRE(alpha_ramp);
        const std::array<std::uint8_t, 4> alpha_pixel = {0, 0, 0, 128};
        auto upload = device->makeCommandBuffer("Alchemy terrain alpha upload", &error);
        INFO(error);
        REQUIRE(upload);
        REQUIRE(alpha_ramp->upload(*upload, 0, 0, {}, alpha_pixel.data(), 4, &error));
        REQUIRE(upload->commit(&error));

        ALMetalTextureDescriptor template_descriptor;
        template_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        template_descriptor.width = 8;
        template_descriptor.height = 8;
        template_descriptor.mipmap_level_count = 4;
        template_descriptor.storage = ALMetalStorageMode::private_gpu;
        template_descriptor.usage = ALMetalTextureUsage::shader_read;
        template_descriptor.label = "Alchemy terrain paint destination template";
        auto destination_template = ALMetalTexture::create(
            *device, template_descriptor, &error);
        INFO(error);
        REQUIRE(destination_template);

        ALMetalTerrainPaintBatch batch;
        batch.vertices = vertices.get();
        batch.position_offset = position_offset;
        batch.texcoord_offset = texcoord_offset;
        batch.indices = indices.get();
        batch.index_type = ALMetalIndexType::uint32;
        batch.index_count = index_values.size();
        batch.alpha_ramp = alpha_ramp.get();
        batch.destination_template = destination_template.get();
        batch.precision = 255.f;
        for (int i = 0; i < 4; ++i)
        {
            batch.modelview_projection[i * 4 + i] = 1.f;
        }

        auto result = renderer->render(batch, &error);
        INFO(error);
        REQUIRE(result);
        CHECK(alMetalHasUsage(result->descriptor().usage,
                              ALMetalTextureUsage::shader_read));
        CHECK(alMetalHasUsage(result->descriptor().usage,
                              ALMetalTextureUsage::render_target));
        CHECK(result->descriptor().mipmap_level_count == 4);

        std::array<std::uint8_t, 8 * 8 * 4> level_zero{};
        REQUIRE(result->readback(
            *device, 0, 0, {}, level_zero.data(), 8 * 4, &error));
        INFO(error);
        for (std::size_t pixel = 0; pixel < 8 * 8; ++pixel)
        {
            CHECK(level_zero[pixel * 4 + 0] == 64);
            CHECK(level_zero[pixel * 4 + 1] == 64);
            CHECK(level_zero[pixel * 4 + 2] == 63);
            CHECK(level_zero[pixel * 4 + 3] == 255);
        }

        std::array<std::uint8_t, 4> final_mip{};
        REQUIRE(result->readback(
            *device, 3, 0, {}, final_mip.data(), 4, &error));
        INFO(error);
        CHECK(final_mip == std::array<std::uint8_t, 4>{64, 64, 63, 255});

        // A Metal render target numbers row zero from the top, while the GL
        // framebuffer copy numbers it from the bottom. Vary the composition
        // coordinate vertically and verify the renderer stores GL's lower
        // row first so the two mirrors remain byte-for-byte interchangeable.
        const float vertical_texcoords[4][2] = {
            {1.25f, 0.5f}, {1.25f, 0.5f},
            {1.75f, 0.5f}, {1.75f, 0.5f},
        };
        REQUIRE(vertices->write(
            texcoord_offset, vertical_texcoords, sizeof(vertical_texcoords), &error));
        ALMetalTextureDescriptor gradient_descriptor = alpha_descriptor;
        gradient_descriptor.width = 2;
        gradient_descriptor.label = "Alchemy terrain alpha orientation test";
        auto gradient = ALMetalTexture::create(*device, gradient_descriptor, &error);
        INFO(error);
        REQUIRE(gradient);
        const std::array<std::uint8_t, 8> gradient_pixels = {
            0, 0, 0, 0,
            0, 0, 0, 255,
        };
        auto gradient_upload = device->makeCommandBuffer(
            "Alchemy terrain alpha orientation upload", &error);
        INFO(error);
        REQUIRE(gradient_upload);
        REQUIRE(gradient->upload(
            *gradient_upload, 0, 0, {}, gradient_pixels.data(), 8, &error));
        REQUIRE(gradient_upload->commit(&error));

        batch.alpha_ramp = gradient.get();
        auto oriented = renderer->render(batch, &error);
        INFO(error);
        REQUIRE(oriented);
        std::array<std::uint8_t, 8 * 8 * 4> oriented_pixels{};
        REQUIRE(oriented->readback(
            *device, 0, 0, {}, oriented_pixels.data(), 8 * 4, &error));
        INFO(error);
        const std::size_t lower_gl_row = 4 * 4;
        CHECK(oriented_pixels[lower_gl_row + 0] == 0);
        CHECK(oriented_pixels[lower_gl_row + 1] == 0);
        CHECK(oriented_pixels[lower_gl_row + 2] == 255);
        CHECK(oriented_pixels[lower_gl_row + 3] == 255);
        const std::size_t upper_gl_row = (7 * 8 + 4) * 4;
        CHECK(oriented_pixels[upper_gl_row + 0] == 0);
        CHECK(oriented_pixels[upper_gl_row + 1] == 0);
        CHECK(oriented_pixels[upper_gl_row + 2] == 0);
        CHECK(oriented_pixels[upper_gl_row + 3] == 255);
    }
}

TEST_CASE("almetal: world simple pass writes the native four-attachment G-buffer", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error);
        REQUIRE(renderer);

        const std::array<std::array<float, 4>, 3> positions{{
            {{-0.8f, -0.8f, 0.25f, 1.f}},
            {{ 0.8f, -0.8f, 0.25f, 1.f}},
            {{ 0.0f,  0.8f, 0.25f, 1.f}},
        }};
        const std::array<std::array<float, 4>, 3> normals{{
            {{0.f, 0.f, 1.f, 0.f}},
            {{0.f, 0.f, 1.f, 0.f}},
            {{0.f, 0.f, 1.f, 0.f}},
        }};
        const std::array<std::array<float, 2>, 3> texcoords{{
            {{0.f, 0.f}}, {{0.f, 0.f}}, {{0.f, 0.f}},
        }};
        const std::array<std::array<std::uint8_t, 4>, 3> colors{{
            {{128, 255, 255, 128}},
            {{128, 255, 255, 128}},
            {{128, 255, 255, 128}},
        }};
        const std::array<std::array<float, 4>, 3> tangents{{
            {{1.f, 0.f, 0.f, 1.f}},
            {{1.f, 0.f, 0.f, 1.f}},
            {{1.f, 0.f, 0.f, 1.f}},
        }};
        const std::array<std::array<float, 4>, 3> weights{{
            {{0.5f, 0.f, 0.f, 0.f}},
            {{0.5f, 0.f, 0.f, 0.f}},
            {{0.5f, 0.f, 0.f, 0.f}},
        }};
        const std::array<std::array<float, 2>, 3> emboss_texcoords{{
            {{0.5f, 0.f}}, {{0.5f, 0.f}}, {{0.5f, 0.f}},
        }};
        const std::array<std::array<std::uint8_t, 4>, 3> emissive{{
            {{0, 0, 0, 128}},
            {{0, 0, 0, 128}},
            {{0, 0, 0, 128}},
        }};
        const std::array<float, 3> avatar_weights{{0.f, 0.f, 0.f}};
        const std::array<std::array<float, 4>, 3> avatar_clothing{{
            {{0.25f, 0.f, 0.f, 1.f}},
            {{0.25f, 0.f, 0.f, 1.f}},
            {{0.25f, 0.f, 0.f, 1.f}},
        }};
        constexpr std::size_t normal_offset = sizeof(positions);
        constexpr std::size_t texcoord_offset =
            normal_offset + sizeof(normals);
        constexpr std::size_t color_offset =
            texcoord_offset + sizeof(texcoords);
        constexpr std::size_t tangent_offset =
            (color_offset + sizeof(colors) + 15) & ~std::size_t{15};
        constexpr std::size_t weight_offset =
            tangent_offset + sizeof(tangents);
        constexpr std::size_t emboss_texcoord_offset =
            weight_offset + sizeof(weights);
        constexpr std::size_t emissive_offset =
            emboss_texcoord_offset + sizeof(emboss_texcoords);
        constexpr std::size_t vertex_storage_size =
            (emissive_offset + sizeof(emissive) + 15) & ~std::size_t{15};
        constexpr std::size_t avatar_weight_offset =
            texcoord_offset + sizeof(texcoords);
        constexpr std::size_t avatar_cloth_offset =
            (avatar_weight_offset + sizeof(avatar_weights) + 15) &
            ~std::size_t{15};
        std::array<std::uint8_t, vertex_storage_size> vertex_bytes{};
        std::memcpy(vertex_bytes.data(), positions.data(), sizeof(positions));
        std::memcpy(vertex_bytes.data() + normal_offset,
                    normals.data(), sizeof(normals));
        std::memcpy(vertex_bytes.data() + texcoord_offset,
                    texcoords.data(), sizeof(texcoords));
        std::memcpy(vertex_bytes.data() + color_offset,
                    colors.data(), sizeof(colors));
        std::memcpy(vertex_bytes.data() + tangent_offset,
                    tangents.data(), sizeof(tangents));
        std::memcpy(vertex_bytes.data() + weight_offset,
                    weights.data(), sizeof(weights));
        std::memcpy(vertex_bytes.data() + emboss_texcoord_offset,
                    emboss_texcoords.data(), sizeof(emboss_texcoords));
        std::memcpy(vertex_bytes.data() + emissive_offset,
                    emissive.data(), sizeof(emissive));
        std::array<std::uint8_t,
                   avatar_cloth_offset + sizeof(avatar_clothing)>
            avatar_vertex_bytes{};
        std::memcpy(avatar_vertex_bytes.data(),
                    positions.data(), sizeof(positions));
        std::memcpy(avatar_vertex_bytes.data() + normal_offset,
                    normals.data(), sizeof(normals));
        std::memcpy(avatar_vertex_bytes.data() + texcoord_offset,
                    texcoords.data(), sizeof(texcoords));
        std::memcpy(avatar_vertex_bytes.data() + avatar_weight_offset,
                    avatar_weights.data(), sizeof(avatar_weights));
        std::memcpy(avatar_vertex_bytes.data() + avatar_cloth_offset,
                    avatar_clothing.data(), sizeof(avatar_clothing));

        ALMetalBufferDescriptor vertex_descriptor;
        vertex_descriptor.length = vertex_bytes.size();
        vertex_descriptor.storage = ALMetalStorageMode::shared;
        vertex_descriptor.label = "Alchemy world simple test vertices";
        auto vertices = ALMetalBuffer::create(
            *device, vertex_descriptor, &error);
        INFO(error);
        REQUIRE(vertices);
        REQUIRE(vertices->write(
            0, vertex_bytes.data(), vertex_bytes.size(), &error));
        ALMetalBufferDescriptor avatar_vertex_descriptor = vertex_descriptor;
        avatar_vertex_descriptor.length = avatar_vertex_bytes.size();
        avatar_vertex_descriptor.label =
            "Alchemy world avatar test vertices";
        auto avatar_vertices = ALMetalBuffer::create(
            *device, avatar_vertex_descriptor, &error);
        INFO(error);
        REQUIRE(avatar_vertices);
        REQUIRE(avatar_vertices->write(
            0, avatar_vertex_bytes.data(), avatar_vertex_bytes.size(),
            &error));

        const std::array<std::uint16_t, 3> index_values{{0, 1, 2}};
        ALMetalBufferDescriptor index_descriptor;
        index_descriptor.length = sizeof(index_values);
        index_descriptor.storage = ALMetalStorageMode::shared;
        index_descriptor.label = "Alchemy world simple test indices";
        auto indices = ALMetalBuffer::create(
            *device, index_descriptor, &error);
        INFO(error);
        REQUIRE(indices);
        REQUIRE(indices->write(
            0, index_values.data(), sizeof(index_values), &error));

        ALMetalTextureDescriptor diffuse_descriptor;
        diffuse_descriptor.format = ALMetalPixelFormat::rgba8_unorm_srgb;
        diffuse_descriptor.width = 1;
        diffuse_descriptor.height = 1;
        diffuse_descriptor.storage = ALMetalStorageMode::private_gpu;
        diffuse_descriptor.usage = ALMetalTextureUsage::shader_read;
        diffuse_descriptor.label = "Alchemy world simple sRGB diffuse";
        auto diffuse = ALMetalTexture::create(
            *device, diffuse_descriptor, &error);
        INFO(error);
        REQUIRE(diffuse);
        const std::array<std::uint8_t, 4> diffuse_pixel{{128, 64, 32, 255}};
        diffuse_descriptor.label =
            "Alchemy world simple indexed sRGB diffuse";
        auto indexed_diffuse = ALMetalTexture::create(
            *device, diffuse_descriptor, &error);
        INFO(error);
        REQUIRE(indexed_diffuse);
        const std::array<std::uint8_t, 4> indexed_diffuse_pixel{{
            0, 255, 0, 255,
        }};
        diffuse_descriptor.label =
            "Alchemy world tree half-alpha sRGB diffuse";
        auto tree_diffuse = ALMetalTexture::create(
            *device, diffuse_descriptor, &error);
        INFO(error);
        REQUIRE(tree_diffuse);
        const std::array<std::uint8_t, 4> tree_diffuse_pixel{{
            128, 64, 32, 128,
        }};
        ALMetalTextureDescriptor normal_descriptor = diffuse_descriptor;
        normal_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        normal_descriptor.label = "Alchemy world bump tangent normal";
        auto bump_normal = ALMetalTexture::create(
            *device, normal_descriptor, &error);
        INFO(error);
        REQUIRE(bump_normal);
        const std::array<std::uint8_t, 4> bump_normal_pixel{{
            255, 128, 128, 128,
        }};
        normal_descriptor.label = "Alchemy world flat tangent normal";
        auto flat_normal = ALMetalTexture::create(
            *device, normal_descriptor, &error);
        INFO(error);
        REQUIRE(flat_normal);
        const std::array<std::uint8_t, 4> flat_normal_pixel{{
            128, 128, 255, 255,
        }};
        normal_descriptor.width = 2;
        normal_descriptor.label = "Alchemy world two-tap emboss bump";
        auto emboss_bump = ALMetalTexture::create(
            *device, normal_descriptor, &error);
        INFO(error);
        REQUIRE(emboss_bump);
        const std::array<std::uint8_t, 8> emboss_bump_pixels{{
            0, 0, 0, 128,
            0, 0, 0, 192,
        }};
        normal_descriptor.width = 1;
        ALMetalTextureDescriptor material_specular_descriptor =
            diffuse_descriptor;
        material_specular_descriptor.label =
            "Alchemy world material white sRGB specular";
        auto material_specular_texture = ALMetalTexture::create(
            *device, material_specular_descriptor, &error);
        INFO(error);
        REQUIRE(material_specular_texture);
        const std::array<std::uint8_t, 4> material_specular_pixel{{
            255, 255, 255, 128,
        }};
        ALMetalTextureDescriptor pbr_orm_descriptor = normal_descriptor;
        pbr_orm_descriptor.label = "Alchemy world PBR ORM";
        auto pbr_orm_texture = ALMetalTexture::create(
            *device, pbr_orm_descriptor, &error);
        INFO(error);
        REQUIRE(pbr_orm_texture);
        const std::array<std::uint8_t, 4> pbr_orm_pixel{{
            128, 128, 64, 255,
        }};
        ALMetalTextureDescriptor pbr_base_descriptor = diffuse_descriptor;
        pbr_base_descriptor.width = 2;
        pbr_base_descriptor.label =
            "Alchemy world PBR transformed sRGB base color";
        auto pbr_base_texture = ALMetalTexture::create(
            *device, pbr_base_descriptor, &error);
        INFO(error);
        REQUIRE(pbr_base_texture);
        const std::array<std::uint8_t, 8> pbr_base_pixels{{
            255, 0, 0, 255,
            0, 255, 0, 128,
        }};
        auto upload = device->makeCommandBuffer(
            "Alchemy world simple diffuse upload", &error);
        INFO(error);
        REQUIRE(upload);
        REQUIRE(diffuse->upload(
            *upload, 0, 0, {}, diffuse_pixel.data(), 4, &error));
        REQUIRE(indexed_diffuse->upload(
            *upload, 0, 0, {}, indexed_diffuse_pixel.data(), 4, &error));
        REQUIRE(tree_diffuse->upload(
            *upload, 0, 0, {}, tree_diffuse_pixel.data(), 4, &error));
        REQUIRE(bump_normal->upload(
            *upload, 0, 0, {}, bump_normal_pixel.data(), 4, &error));
        REQUIRE(flat_normal->upload(
            *upload, 0, 0, {}, flat_normal_pixel.data(), 4, &error));
        REQUIRE(emboss_bump->upload(
            *upload, 0, 0, {}, emboss_bump_pixels.data(), 8, &error));
        REQUIRE(material_specular_texture->upload(
            *upload, 0, 0, {}, material_specular_pixel.data(), 4, &error));
        REQUIRE(pbr_orm_texture->upload(
            *upload, 0, 0, {}, pbr_orm_pixel.data(), 4, &error));
        REQUIRE(pbr_base_texture->upload(
            *upload, 0, 0, {}, pbr_base_pixels.data(), 8, &error));
        REQUIRE(upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldSimpleBatch batch;
        batch.vertices = vertices.get();
        batch.position_offset = 0;
        batch.normal_offset = normal_offset;
        batch.texcoord_offset = texcoord_offset;
        batch.color_offset = color_offset;
        batch.vertex_count = positions.size();
        batch.indices = indices.get();
        batch.index_count = index_values.size();
        batch.diffuse_textures[0] = diffuse.get();
        batch.texture_count = 1;
        for (std::size_t index = 0; index < 4; ++index)
        {
            batch.modelview_projection[index * 4 + index] = 1.f;
        }
        for (std::size_t index = 0; index < 3; ++index)
        {
            batch.normal_matrix[index][index] = 1.f;
        }

        ALMetalWorldGBufferScene scene;
        scene.batches = &batch;
        scene.batch_count = 1;
        scene.width = 32;
        scene.height = 32;
        scene.normal_hdr = true;
        scene.hdr = true;

        batch.color_offset = vertex_bytes.size();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error ==
              "Metal world simple geometry range exceeds its buffers");
        batch.color_offset = color_offset;
        error.clear();

        auto result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(result);
        CHECK(result->descriptor().color_attachment_count == 4);
        CHECK(result->descriptor().colors[0].format ==
              ALMetalPixelFormat::rgba8_unorm_srgb);
        CHECK(result->descriptor().colors[1].format ==
              ALMetalPixelFormat::rgba8_unorm);
        CHECK(result->descriptor().colors[2].format ==
              ALMetalPixelFormat::rgba16_unorm);
        CHECK(result->descriptor().colors[3].format ==
              ALMetalPixelFormat::rg11b10_float);
        REQUIRE(result->depthStencilTexture());

        std::array<std::uint8_t, 32 * 32 * 4> albedo{};
        REQUIRE(result->colorTexture(0)->readback(
            *device, 0, 0, {}, albedo.data(), 32 * 4, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> specular{};
        REQUIRE(result->colorTexture(1)->readback(
            *device, 0, 0, {}, specular.data(), 32 * 4, &error));
        INFO(error);
        std::array<std::uint16_t, 32 * 32 * 4> normal{};
        REQUIRE(result->colorTexture(2)->readback(
            *device, 0, 0, {}, normal.data(), 32 * 8, &error));
        INFO(error);

        const std::size_t center = (16 * 32 + 16) * 4;
        // Both diffuse and tint are decoded with the exact sRGB transfer;
        // attachment zero then performs the hardware sRGB encode on store.
        CHECK(std::abs(static_cast<int>(albedo[center + 0]) - 61) <= 1);
        CHECK(std::abs(static_cast<int>(albedo[center + 1]) - 64) <= 1);
        CHECK(std::abs(static_cast<int>(albedo[center + 2]) - 32) <= 1);
        CHECK(albedo[center + 3] == 0);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(std::abs(static_cast<int>(specular[center + channel]) -
                           128) <= 1);
        }
        CHECK(std::abs(static_cast<int>(normal[center + 0]) - 32768) <= 1);
        CHECK(std::abs(static_cast<int>(normal[center + 1]) - 32768) <= 1);
        CHECK(std::abs(static_cast<int>(normal[center + 2]) - 32896) <= 1);
        CHECK(std::abs(static_cast<int>(normal[center + 3]) - 22282) <= 2);

        std::array<float, 32 * 32> forward_depth{};
        REQUIRE(result->depthStencilTexture()->readback(
            *device, 0, 0, {}, forward_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        CHECK(std::abs(forward_depth[16 * 32 + 16] - 0.625f) < 0.0001f);

        // Forward GL clip depth is remapped from [-w,+w]. A reverse-Z world
        // projection already emits Metal's [0,+w] convention and must not be
        // remapped a second time.
        scene.reverse_z = true;
        auto reverse_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(reverse_result);
        std::array<float, 32 * 32> reverse_depth{};
        REQUIRE(reverse_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, reverse_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        CHECK(std::abs(reverse_depth[16 * 32 + 16] - 0.25f) < 0.0001f);

        // The impostor bake turns every opaque depth-covered texel into full
        // albedo coverage before forward alpha accumulates over it. Stamp it
        // on the GPU with strict far-plane depth tests: equal clear depth
        // remains transparent in both forward and reverse Z.
        scene.stamp_opaque_coverage = true;
        scene.reverse_z = false;
        std::fill(std::begin(scene.clear_color),
                  std::end(scene.clear_color), 0.f);
        auto coverage_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(coverage_result);
        std::array<std::uint8_t, 32 * 32 * 4> coverage_albedo{};
        REQUIRE(coverage_result->colorTexture(0)->readback(
            *device, 0, 0, {}, coverage_albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(coverage_albedo[center + 0] == albedo[center + 0]);
        CHECK(coverage_albedo[center + 1] == albedo[center + 1]);
        CHECK(coverage_albedo[center + 2] == albedo[center + 2]);
        CHECK(coverage_albedo[center + 3] == 255);
        CHECK(coverage_albedo[3] == 0);

        scene.reverse_z = true;
        auto reverse_coverage_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(reverse_coverage_result);
        coverage_albedo.fill(0);
        REQUIRE(reverse_coverage_result->colorTexture(0)->readback(
            *device, 0, 0, {}, coverage_albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(coverage_albedo[center + 3] == 255);
        CHECK(coverage_albedo[3] == 0);

        scene.stamp_opaque_coverage = false;
        scene.clear_color[0] = 1.f;
        scene.clear_color[1] = 0.f;
        scene.clear_color[2] = 1.f;
        scene.clear_color[3] = 1.f;

        // TYPE_TEXTURE_INDEX aliases the fourth 32-bit word of the packed
        // position stream. Verify the Metal vertex function reads that exact
        // per-vertex word with the position's 16-byte stride and selects the
        // corresponding entry from the viewer's 32-texture ladder.
        constexpr std::int32_t indexed_texture = 1;
        for (std::size_t vertex = 0; vertex < positions.size(); ++vertex)
        {
            std::memcpy(vertex_bytes.data() + vertex * sizeof(positions[0]) +
                            sizeof(float) * 3,
                        &indexed_texture, sizeof(indexed_texture));
        }
        REQUIRE(vertices->write(
            0, vertex_bytes.data(), vertex_bytes.size(), &error));
        batch.diffuse_textures[1] = indexed_diffuse.get();
        batch.texture_count = 2;
        scene.reverse_z = false;
        auto indexed_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(indexed_result);
        std::array<std::uint8_t, 32 * 32 * 4> indexed_albedo{};
        REQUIRE(indexed_result->colorTexture(0)->readback(
            *device, 0, 0, {}, indexed_albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(indexed_albedo[center + 0] == 0);
        CHECK(indexed_albedo[center + 1] == 255);
        CHECK(indexed_albedo[center + 2] == 0);
        CHECK(indexed_albedo[center + 3] == 0);

        // The legacy bump owner consumes the mirrored tangent stream and its
        // generated linear normal map. This +X tangent-space normal must turn
        // the encoded view normal away from the geometric +Z value while
        // retaining the vertex-alpha specular/gloss contract.
        batch.tangent_offset =
            (vertex_bytes.size() + 15) & ~std::size_t{15};
        batch.normal_texture = bump_normal.get();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error ==
              "Metal world simple geometry range exceeds its buffers");
        batch.tangent_offset = tangent_offset;
        error.clear();
        auto bump_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(bump_result);
        std::array<std::uint8_t, 32 * 32 * 4> bump_specular{};
        std::array<std::uint16_t, 32 * 32 * 4> bump_normal_output{};
        REQUIRE(bump_result->colorTexture(1)->readback(
            *device, 0, 0, {}, bump_specular.data(), 32 * 4, &error));
        REQUIRE(bump_result->colorTexture(2)->readback(
            *device, 0, 0, {}, bump_normal_output.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(bump_normal_output[center + 0] > 65000);
        CHECK(std::abs(static_cast<int>(bump_normal_output[center + 1]) -
                       32768) < 512);
        CHECK(std::abs(static_cast<int>(bump_normal_output[center + 2]) -
                       32896) <= 2);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(std::abs(static_cast<int>(bump_specular[center + channel]) -
                           128) <= 1);
        }

        batch.alpha_mask = true;
        batch.alpha_cutoff = 0.5f;
        auto masked_bump_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(masked_bump_result);
        std::array<std::uint16_t, 32 * 32 * 4> masked_bump_normal{};
        REQUIRE(masked_bump_result->colorTexture(2)->readback(
            *device, 0, 0, {}, masked_bump_normal.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(masked_bump_normal[center + 0] > 65000);

        batch.normal_texture = nullptr;
        batch.tangent_offset = 0;

        // The alpha-mask owner uses combined texture/vertex coverage with the
        // legacy half-byte bias, but clears specular/environment while
        // retaining the same encoded albedo and atmospheric normal flag.
        batch.alpha_mask = true;
        batch.alpha_cutoff = 0.5f;
        auto masked_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(masked_result);
        std::array<std::uint8_t, 32 * 32 * 4> masked_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4> masked_specular{};
        std::array<std::uint16_t, 32 * 32 * 4> masked_normal{};
        REQUIRE(masked_result->colorTexture(0)->readback(
            *device, 0, 0, {}, masked_albedo.data(), 32 * 4, &error));
        REQUIRE(masked_result->colorTexture(1)->readback(
            *device, 0, 0, {}, masked_specular.data(), 32 * 4, &error));
        REQUIRE(masked_result->colorTexture(2)->readback(
            *device, 0, 0, {}, masked_normal.data(), 32 * 8, &error));
        INFO(error);
        CHECK(masked_albedo[center + 0] == 0);
        CHECK(masked_albedo[center + 1] == 255);
        CHECK(masked_albedo[center + 2] == 0);
        CHECK(masked_specular[center + 0] == 0);
        CHECK(masked_specular[center + 1] == 0);
        CHECK(masked_specular[center + 2] == 0);
        CHECK(masked_specular[center + 3] == 0);
        CHECK(std::abs(static_cast<int>(masked_normal[center + 0]) -
                       32768) <= 1);
        CHECK(std::abs(static_cast<int>(masked_normal[center + 1]) -
                       32768) <= 1);
        CHECK(masked_normal[center + 2] == 0);
        CHECK(std::abs(static_cast<int>(masked_normal[center + 3]) -
                       22282) <= 2);

        // Tree cutouts share the alpha-mask pipeline but intentionally ignore
        // the streamed vertex tint and compare alpha without the generic
        // diffuse-mask half-byte bias. A cutoff just above 128/255 therefore
        // discards here, then passes when the generic bias is restored.
        batch.diffuse_textures[1] = tree_diffuse.get();
        batch.use_vertex_color = false;
        batch.alpha_cutoff_bias = false;
        batch.alpha_cutoff = 0.503f;
        auto tree_discarded_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(tree_discarded_result);
        std::array<std::uint8_t, 32 * 32 * 4> tree_discarded_albedo{};
        REQUIRE(tree_discarded_result->colorTexture(0)->readback(
            *device, 0, 0, {}, tree_discarded_albedo.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(tree_discarded_albedo[center + 0] == 255);
        CHECK(tree_discarded_albedo[center + 1] == 0);
        CHECK(tree_discarded_albedo[center + 2] == 255);

        batch.alpha_cutoff_bias = true;
        auto tree_biased_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(tree_biased_result);
        std::array<std::uint8_t, 32 * 32 * 4> tree_biased_albedo{};
        REQUIRE(tree_biased_result->colorTexture(0)->readback(
            *device, 0, 0, {}, tree_biased_albedo.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(tree_biased_albedo[center + 0]) -
                       128) <= 1);
        CHECK(std::abs(static_cast<int>(tree_biased_albedo[center + 1]) -
                       64) <= 1);
        CHECK(std::abs(static_cast<int>(tree_biased_albedo[center + 2]) -
                       32) <= 1);

        batch.diffuse_textures[1] = indexed_diffuse.get();
        batch.use_vertex_color = true;
        batch.alpha_cutoff = 0.6f;
        auto discarded_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(discarded_result);
        std::array<std::uint8_t, 32 * 32 * 4> discarded_albedo{};
        REQUIRE(discarded_result->colorTexture(0)->readback(
            *device, 0, 0, {}, discarded_albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(discarded_albedo[center + 0] == 255);
        CHECK(discarded_albedo[center + 1] == 0);
        CHECK(discarded_albedo[center + 2] == 255);
        CHECK(discarded_albedo[center + 3] == 255);

        // Plain legacy materials add authored specular/gloss, environment,
        // fullbright and three diffuse-alpha modes to the same geometry path.
        // The position.w slot index selects the second material exactly as the
        // indexed GL program does.
        batch.alpha_mask = false;
        batch.alpha_cutoff = 0.f;
        batch.diffuse_textures[1] = indexed_diffuse.get();
        batch.material_slot_count = 2;
        batch.material_alpha_mode = 1;
        batch.material_slots[1].specular_color[0] = 0.25f;
        batch.material_slots[1].specular_color[1] = 0.5f;
        batch.material_slots[1].specular_color[2] = 0.75f;
        batch.material_slots[1].specular_color[3] = 0.25f;
        batch.material_slots[1].env_intensity = 0.375f;
        batch.material_slots[1].alpha_cutoff = 0.5f;
        batch.material_slots[1].fullbright = 0.25f;
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world simple batch is invalid");

        batch.material_alpha_mode = 0;
        error.clear();
        auto material_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(material_result);
        std::array<std::uint8_t, 32 * 32 * 4> material_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4> material_specular{};
        std::array<std::uint16_t, 32 * 32 * 4> material_normal{};
        REQUIRE(material_result->colorTexture(0)->readback(
            *device, 0, 0, {}, material_albedo.data(), 32 * 4, &error));
        REQUIRE(material_result->colorTexture(1)->readback(
            *device, 0, 0, {}, material_specular.data(), 32 * 4, &error));
        REQUIRE(material_result->colorTexture(2)->readback(
            *device, 0, 0, {}, material_normal.data(), 32 * 8, &error));
        INFO(error);
        CHECK(material_albedo[center + 0] == 0);
        CHECK(material_albedo[center + 1] == 255);
        CHECK(material_albedo[center + 2] == 0);
        CHECK(std::abs(static_cast<int>(material_albedo[center + 3]) -
                       64) <= 1);
        CHECK(std::abs(static_cast<int>(material_specular[center + 0]) -
                       64) <= 1);
        CHECK(std::abs(static_cast<int>(material_specular[center + 1]) -
                       128) <= 1);
        CHECK(std::abs(static_cast<int>(material_specular[center + 2]) -
                       191) <= 1);
        CHECK(std::abs(static_cast<int>(material_specular[center + 3]) -
                       64) <= 1);
        CHECK(std::abs(static_cast<int>(material_normal[center + 0]) -
                       32768) <= 1);
        CHECK(std::abs(static_cast<int>(material_normal[center + 1]) -
                       32768) <= 1);
        CHECK(std::abs(static_cast<int>(material_normal[center + 2]) -
                       24576) <= 2);
        CHECK(std::abs(static_cast<int>(material_normal[center + 3]) -
                       22282) <= 2);

        // The three remaining legacy material families share the same slot
        // constants but independently opt into the tangent/UV1 normal map and
        // UV2 sRGB specular map. Exercise normal-only, specular-only and both
        // before the rigged variant below reuses the combined contract.
        batch.tangent_offset = tangent_offset;
        batch.texcoord1_offset = texcoord_offset;
        batch.texcoord2_offset = texcoord_offset;
        batch.material_slots[0].normal_texture = bump_normal.get();
        batch.material_slots[1].normal_texture = bump_normal.get();
        auto material_normal_map_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(material_normal_map_result);
        std::array<std::uint8_t, 32 * 32 * 4>
            material_normal_map_specular{};
        std::array<std::uint16_t, 32 * 32 * 4>
            material_normal_map_normal{};
        REQUIRE(material_normal_map_result->colorTexture(1)->readback(
            *device, 0, 0, {}, material_normal_map_specular.data(),
            32 * 4, &error));
        REQUIRE(material_normal_map_result->colorTexture(2)->readback(
            *device, 0, 0, {}, material_normal_map_normal.data(),
            32 * 8, &error));
        INFO(error);
        CHECK(material_normal_map_normal[center + 0] > 65000);
        CHECK(std::abs(static_cast<int>(
                           material_normal_map_specular[center + 3]) -
                       32) <= 1);
        CHECK(std::abs(static_cast<int>(
                           material_normal_map_normal[center + 2]) -
                       24576) <= 2);

        batch.material_slots[0].normal_texture = nullptr;
        batch.material_slots[1].normal_texture = nullptr;
        batch.material_slots[0].specular_texture =
            material_specular_texture.get();
        batch.material_slots[1].specular_texture =
            material_specular_texture.get();
        auto material_specular_map_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(material_specular_map_result);
        std::array<std::uint8_t, 32 * 32 * 4>
            material_specular_map_specular{};
        std::array<std::uint16_t, 32 * 32 * 4>
            material_specular_map_normal{};
        REQUIRE(material_specular_map_result->colorTexture(1)->readback(
            *device, 0, 0, {}, material_specular_map_specular.data(),
            32 * 4, &error));
        REQUIRE(material_specular_map_result->colorTexture(2)->readback(
            *device, 0, 0, {}, material_specular_map_normal.data(),
            32 * 8, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           material_specular_map_specular[center + 0]) -
                       64) <= 1);
        CHECK(std::abs(static_cast<int>(
                           material_specular_map_specular[center + 1]) -
                       128) <= 1);
        CHECK(std::abs(static_cast<int>(
                           material_specular_map_specular[center + 2]) -
                       191) <= 1);
        CHECK(std::abs(static_cast<int>(
                           material_specular_map_specular[center + 3]) -
                       64) <= 1);
        CHECK(std::abs(static_cast<int>(
                           material_specular_map_normal[center + 0]) -
                       32768) <= 1);
        CHECK(std::abs(static_cast<int>(
                           material_specular_map_normal[center + 2]) -
                       12336) <= 3);

        batch.material_slots[0].normal_texture = bump_normal.get();
        batch.material_slots[1].normal_texture = bump_normal.get();
        batch.texcoord2_offset = vertex_bytes.size();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error ==
              "Metal world simple geometry range exceeds its buffers");
        batch.texcoord2_offset = texcoord_offset;
        error.clear();
        auto material_normal_specular_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(material_normal_specular_result);
        std::array<std::uint8_t, 32 * 32 * 4>
            material_normal_specular_specular{};
        std::array<std::uint16_t, 32 * 32 * 4>
            material_normal_specular_normal{};
        REQUIRE(material_normal_specular_result->colorTexture(1)->readback(
            *device, 0, 0, {}, material_normal_specular_specular.data(),
            32 * 4, &error));
        REQUIRE(material_normal_specular_result->colorTexture(2)->readback(
            *device, 0, 0, {}, material_normal_specular_normal.data(),
            32 * 8, &error));
        INFO(error);
        CHECK(material_normal_specular_normal[center + 0] > 65000);
        CHECK(std::abs(static_cast<int>(
                           material_normal_specular_specular[center + 3]) -
                       32) <= 1);
        CHECK(std::abs(static_cast<int>(
                           material_normal_specular_normal[center + 2]) -
                       12336) <= 3);

        batch.material_alpha_mode = 2;
        batch.material_slots[1].alpha_cutoff = 1.01f;
        auto material_masked_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(material_masked_result);
        std::array<std::uint8_t, 32 * 32 * 4> material_masked_albedo{};
        REQUIRE(material_masked_result->colorTexture(0)->readback(
            *device, 0, 0, {}, material_masked_albedo.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(material_masked_albedo[center + 0] == 255);
        CHECK(material_masked_albedo[center + 1] == 0);
        CHECK(material_masked_albedo[center + 2] == 255);

        batch.material_alpha_mode = 3;
        batch.diffuse_textures[1] = tree_diffuse.get();
        auto material_emissive_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(material_emissive_result);
        std::array<std::uint8_t, 32 * 32 * 4> material_emissive_albedo{};
        REQUIRE(material_emissive_result->colorTexture(0)->readback(
            *device, 0, 0, {}, material_emissive_albedo.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(material_emissive_albedo[center + 0]) -
                       61) <= 1);
        CHECK(std::abs(static_cast<int>(material_emissive_albedo[center + 1]) -
                       64) <= 1);
        CHECK(std::abs(static_cast<int>(material_emissive_albedo[center + 2]) -
                       32) <= 1);
        CHECK(std::abs(static_cast<int>(material_emissive_albedo[center + 3]) -
                       128) <= 1);

        batch.material_slot_count = 0;
        batch.material_alpha_mode = 0;
        batch.diffuse_textures[1] = indexed_diffuse.get();

        // Reproduce the world owner's rebased object skinning. Joint-local
        // and origin translations each contribute +0.25 clip X, so the
        // indexed green triangle moves by exactly half the target width.
        const std::array<float, 16> skin_palette{{
            0.25f, 0.f, 0.f, 0.f,
            1.f, 0.f, 0.f, 0.25f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
        }};
        batch.alpha_mask = false;
        batch.alpha_cutoff = 0.f;
        batch.weight_offset = weight_offset;
        batch.skin_palette = skin_palette.data();
        batch.skin_palette_float_count = skin_palette.size() - 1;
        batch.skin_joint_count = 1;
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world simple skinning data is invalid");
        batch.skin_palette_float_count = skin_palette.size();
        for (std::size_t column = 0; column < 3; ++column)
        {
            batch.rigged_normal_matrix[column][column] = 1.f;
        }
        error.clear();
        auto skinned_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(skinned_result);
        std::array<std::uint8_t, 32 * 32 * 4> skinned_albedo{};
        REQUIRE(skinned_result->colorTexture(0)->readback(
            *device, 0, 0, {}, skinned_albedo.data(), 32 * 4, &error));
        INFO(error);
        const std::size_t translated_center = (16 * 32 + 24) * 4;
        CHECK(skinned_albedo[translated_center + 0] == 0);
        CHECK(skinned_albedo[translated_center + 1] == 255);
        CHECK(skinned_albedo[translated_center + 2] == 0);
        CHECK(skinned_albedo[translated_center + 3] == 0);

        batch.tangent_offset = tangent_offset;
        batch.normal_texture = bump_normal.get();
        auto skinned_bump_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(skinned_bump_result);
        std::array<std::uint16_t, 32 * 32 * 4> skinned_bump_normal{};
        REQUIRE(skinned_bump_result->colorTexture(2)->readback(
            *device, 0, 0, {}, skinned_bump_normal.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(skinned_bump_normal[translated_center + 0] > 65000);

        batch.alpha_mask = true;
        batch.alpha_cutoff = 0.5f;
        auto skinned_bump_mask_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(skinned_bump_mask_result);
        std::array<std::uint16_t, 32 * 32 * 4> skinned_bump_mask_normal{};
        REQUIRE(skinned_bump_mask_result->colorTexture(2)->readback(
            *device, 0, 0, {}, skinned_bump_mask_normal.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(skinned_bump_mask_normal[translated_center + 0] > 65000);

        batch.normal_texture = nullptr;
        batch.tangent_offset = 0;
        auto skinned_mask_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(skinned_mask_result);
        std::array<std::uint8_t, 32 * 32 * 4> skinned_mask_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4> skinned_mask_specular{};
        REQUIRE(skinned_mask_result->colorTexture(0)->readback(
            *device, 0, 0, {}, skinned_mask_albedo.data(), 32 * 4, &error));
        REQUIRE(skinned_mask_result->colorTexture(1)->readback(
            *device, 0, 0, {}, skinned_mask_specular.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(skinned_mask_albedo[translated_center + 0] == 0);
        CHECK(skinned_mask_albedo[translated_center + 1] == 255);
        CHECK(skinned_mask_albedo[translated_center + 2] == 0);
        CHECK(skinned_mask_specular[translated_center + 0] == 0);
        CHECK(skinned_mask_specular[translated_center + 1] == 0);
        CHECK(skinned_mask_specular[translated_center + 2] == 0);
        CHECK(skinned_mask_specular[translated_center + 3] == 0);

        // The same material fragment contract consumes the rebased rigged
        // vertex output; no separate approximation or CPU skinning path is
        // allowed for the rigged material owner.
        batch.alpha_mask = false;
        batch.tangent_offset = tangent_offset;
        batch.material_slot_count = 2;
        batch.material_alpha_mode = 0;
        batch.material_slots[1].fullbright = 0.25f;
        auto skinned_material_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(skinned_material_result);
        std::array<std::uint8_t, 32 * 32 * 4> skinned_material_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4> skinned_material_specular{};
        std::array<std::uint16_t, 32 * 32 * 4> skinned_material_normal{};
        REQUIRE(skinned_material_result->colorTexture(0)->readback(
            *device, 0, 0, {}, skinned_material_albedo.data(), 32 * 4,
            &error));
        REQUIRE(skinned_material_result->colorTexture(1)->readback(
            *device, 0, 0, {}, skinned_material_specular.data(), 32 * 4,
            &error));
        REQUIRE(skinned_material_result->colorTexture(2)->readback(
            *device, 0, 0, {}, skinned_material_normal.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(skinned_material_albedo[translated_center + 0] == 0);
        CHECK(skinned_material_albedo[translated_center + 1] == 255);
        CHECK(skinned_material_albedo[translated_center + 2] == 0);
        CHECK(std::abs(static_cast<int>(
                           skinned_material_albedo[translated_center + 3]) -
                       64) <= 1);
        CHECK(std::abs(static_cast<int>(
                           skinned_material_specular[translated_center + 3]) -
                       32) <= 1);
        CHECK(skinned_material_normal[translated_center + 0] > 65000);
        CHECK(std::abs(static_cast<int>(
                           skinned_material_normal[translated_center + 2]) -
                       12336) <= 3);

        // The glTF PBR owner uses four independently transformed maps and
        // packs filtered ORM plus the geometric normal into the same target.
        // Exercise the indexed static path first, including a KHR base-color
        // offset that selects the green texel from a red/green texture.
        batch.material_slot_count = 0;
        batch.material_alpha_mode = 0;
        batch.pbr_slot_count = 2;
        batch.alpha_mask = false;
        batch.double_sided = true;
        batch.tangent_offset = tangent_offset;
        batch.diffuse_textures[0] = pbr_base_texture.get();
        batch.diffuse_textures[1] = pbr_base_texture.get();
        for (std::size_t slot = 0; slot < batch.pbr_slot_count; ++slot)
        {
            ALMetalWorldPBRSlot& pbr = batch.pbr_slots[slot];
            pbr.normal_texture = bump_normal.get();
            pbr.orm_texture = pbr_orm_texture.get();
            pbr.emissive_texture = diffuse.get();
            pbr.texture_transform[0][4] = 0.75f;
            pbr.emissive_color[0] = 0.5f;
            pbr.emissive_color[1] = 1.f;
            pbr.roughness_factor = 0.5f;
            pbr.metallic_factor = 0.5f;
            pbr.alpha_cutoff = -1.f;
        }
        batch.skin_palette = nullptr;
        batch.skin_palette_float_count = 0;
        batch.skin_joint_count = 0;
        batch.weight_offset = 0;

        // Blend the glTF FOR_IMPOSTOR contract into the retained opaque bake:
        // flat linear base colour, true coverage, attachment-zero only. Static
        // alpha keeps depth read-only; rigged alpha writes it so later layers
        // self-occlude, matching LLDrawPoolAlpha::forwardRender.
        ALMetalWorldSimpleBatch alpha_batch = batch;
        alpha_batch.texture_count = 1;
        alpha_batch.pbr_slot_count = 1;
        alpha_batch.modelview_projection[12] = 0.8f;
        ALMetalWorldGBufferScene alpha_scene = scene;
        alpha_scene.batches = &alpha_batch;
        alpha_scene.batch_count = 1;
        alpha_scene.hdr = true;
        alpha_scene.normal_hdr = true;
        alpha_scene.reverse_z = false;
        alpha_batch.pbr_slots[0].alpha_cutoff = 0.5f;
        CHECK_FALSE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        CHECK(error == "Metal world PBR alpha material is invalid");
        alpha_batch.pbr_slots[0].alpha_cutoff = -1.f;
        error.clear();
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> alpha_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4> alpha_orm{};
        std::array<float, 32 * 32> alpha_depth{};
        REQUIRE(coverage_result->colorTexture(0)->readback(
            *device, 0, 0, {}, alpha_albedo.data(), 32 * 4, &error));
        REQUIRE(coverage_result->colorTexture(1)->readback(
            *device, 0, 0, {}, alpha_orm.data(), 32 * 4, &error));
        REQUIRE(coverage_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, alpha_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        const std::size_t alpha_right = (16 * 32 + 24) * 4;
        CHECK(alpha_albedo[alpha_right + 0] == 0);
        CHECK(std::abs(static_cast<int>(alpha_albedo[alpha_right + 1]) -
                       137) <= 2);
        CHECK(alpha_albedo[alpha_right + 2] == 0);
        CHECK(std::abs(static_cast<int>(alpha_albedo[alpha_right + 3]) -
                       64) <= 1);
        CHECK(alpha_albedo[center + 0] == coverage_albedo[center + 0]);
        CHECK(alpha_albedo[center + 1] == coverage_albedo[center + 1]);
        CHECK(alpha_albedo[center + 2] == coverage_albedo[center + 2]);
        CHECK(alpha_albedo[center + 3] == 255);
        CHECK(alpha_orm[alpha_right + 0] == 0);
        CHECK(alpha_orm[alpha_right + 1] == 0);
        CHECK(alpha_orm[alpha_right + 2] == 0);
        CHECK(alpha_orm[alpha_right + 3] == 0);
        CHECK(std::abs(alpha_depth[16 * 32 + 24] - 1.f) < 0.0001f);

        REQUIRE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        INFO(error);
        REQUIRE(coverage_result->colorTexture(0)->readback(
            *device, 0, 0, {}, alpha_albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(alpha_albedo[alpha_right + 1]) -
                       177) <= 2);
        CHECK(std::abs(static_cast<int>(alpha_albedo[alpha_right + 3]) -
                       112) <= 1);

        const std::array<float, 16> alpha_skin_palette{{
            0.f, 0.f, 0.f, 0.f,
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
        }};
        alpha_batch.modelview_projection[12] = -0.8f;
        alpha_batch.weight_offset = weight_offset;
        alpha_batch.skin_palette = alpha_skin_palette.data();
        alpha_batch.skin_palette_float_count = alpha_skin_palette.size();
        alpha_batch.skin_joint_count = 1;
        for (std::size_t column = 0; column < 3; ++column)
        {
            alpha_batch.rigged_normal_matrix[column][column] = 1.f;
        }
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        INFO(error);
        REQUIRE(coverage_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, alpha_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        CHECK(std::abs(alpha_depth[16 * 32 + 8] - 0.625f) < 0.0001f);

        alpha_scene.reverse_z = true;
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *reverse_coverage_result, alpha_scene, &error));
        INFO(error);
        REQUIRE(reverse_coverage_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, alpha_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        CHECK(std::abs(alpha_depth[16 * 32 + 8] - 0.25f) < 0.0001f);

        // The legacy simple and fullbright impostor programs converge on the
        // same flat diffuse-times-tint contract. Exercise that pipeline with
        // static read-only depth and the object-skinned depth-writing variant.
        ALMetalWorldSimpleBatch flat_alpha_batch = alpha_batch;
        flat_alpha_batch.pbr_slot_count = 0;
        flat_alpha_batch.double_sided = false;
        flat_alpha_batch.tangent_offset = 0;
        flat_alpha_batch.diffuse_textures[0] = indexed_diffuse.get();
        flat_alpha_batch.skin_palette = nullptr;
        flat_alpha_batch.skin_palette_float_count = 0;
        flat_alpha_batch.skin_joint_count = 0;
        flat_alpha_batch.weight_offset = 0;
        flat_alpha_batch.modelview_projection[12] = 0.8f;
        flat_alpha_batch.modelview_projection[13] = 0.8f;
        alpha_scene.batches = &flat_alpha_batch;
        alpha_scene.reverse_z = false;
        flat_alpha_batch.diffuse_textures[0] = nullptr;
        CHECK_FALSE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        CHECK(error ==
              "Metal world impostor post-deferred texture is invalid");
        flat_alpha_batch.diffuse_textures[0] = indexed_diffuse.get();
        error.clear();
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        INFO(error);
        REQUIRE(coverage_result->colorTexture(0)->readback(
            *device, 0, 0, {}, alpha_albedo.data(), 32 * 4, &error));
        REQUIRE(coverage_result->colorTexture(1)->readback(
            *device, 0, 0, {}, alpha_orm.data(), 32 * 4, &error));
        REQUIRE(coverage_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, alpha_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        const std::size_t alpha_top = (24 * 32 + 24) * 4;
        CHECK(alpha_albedo[alpha_top + 0] == 0);
        CHECK(std::abs(static_cast<int>(alpha_albedo[alpha_top + 1]) -
                       188) <= 2);
        CHECK(alpha_albedo[alpha_top + 2] == 0);
        CHECK(std::abs(static_cast<int>(alpha_albedo[alpha_top + 3]) -
                       128) <= 1);
        CHECK(alpha_orm[alpha_top + 0] == 0);
        CHECK(alpha_orm[alpha_top + 1] == 0);
        CHECK(alpha_orm[alpha_top + 2] == 0);
        CHECK(alpha_orm[alpha_top + 3] == 0);
        CHECK(std::abs(alpha_depth[24 * 32 + 24] - 1.f) < 0.0001f);

        flat_alpha_batch.modelview_projection[13] = -0.8f;
        flat_alpha_batch.weight_offset = weight_offset;
        flat_alpha_batch.skin_palette = alpha_skin_palette.data();
        flat_alpha_batch.skin_palette_float_count =
            alpha_skin_palette.size();
        flat_alpha_batch.skin_joint_count = 1;
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        INFO(error);
        REQUIRE(coverage_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, alpha_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        CHECK(std::abs(alpha_depth[8 * 32 + 28] - 0.625f) < 0.0001f);

        // Fullbright and fullbright-mask run earlier in post-deferred than
        // blended alpha. They write depth, then the bake seeds full coverage
        // from that depth before partial coverage begins. Exercise the masked
        // static owner and that phase-boundary stamp together.
        ALMetalWorldSimpleBatch fullbright_batch = flat_alpha_batch;
        fullbright_batch.impostor_fullbright = true;
        fullbright_batch.alpha_mask = true;
        fullbright_batch.alpha_cutoff = 1.1f;
        fullbright_batch.skin_palette = nullptr;
        fullbright_batch.skin_palette_float_count = 0;
        fullbright_batch.skin_joint_count = 0;
        fullbright_batch.weight_offset = 0;
        fullbright_batch.modelview_projection[12] = -0.8f;
        fullbright_batch.modelview_projection[13] = 0.8f;
        alpha_scene.batches = &fullbright_batch;
        alpha_scene.stamp_opaque_coverage = true;
        CHECK_FALSE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        CHECK(error ==
              "Metal world impostor post-deferred batch is invalid");
        fullbright_batch.alpha_cutoff = 0.5f;
        error.clear();
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        INFO(error);
        REQUIRE(coverage_result->colorTexture(0)->readback(
            *device, 0, 0, {}, alpha_albedo.data(), 32 * 4, &error));
        REQUIRE(coverage_result->colorTexture(1)->readback(
            *device, 0, 0, {}, alpha_orm.data(), 32 * 4, &error));
        REQUIRE(coverage_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, alpha_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        const std::size_t fullbright_top = (24 * 32 + 8) * 4;
        CHECK(alpha_albedo[fullbright_top + 0] == 0);
        CHECK(alpha_albedo[fullbright_top + 1] == 255);
        CHECK(alpha_albedo[fullbright_top + 2] == 0);
        CHECK(alpha_albedo[fullbright_top + 3] == 255);
        CHECK(alpha_orm[fullbright_top + 0] == 0);
        CHECK(alpha_orm[fullbright_top + 1] == 0);
        CHECK(alpha_orm[fullbright_top + 2] == 0);
        CHECK(alpha_orm[fullbright_top + 3] == 0);
        CHECK(std::abs(alpha_depth[24 * 32 + 8] - 0.625f) < 0.0001f);

        // The main-world owner starts from independently produced lighting
        // color and G-buffer depth. It copies both on-GPU, then replays this
        // opaque-forward batch into a single final-color attachment without
        // disturbing pixels outside the geometry.
        ALMetalRenderTargetDescriptor final_light_descriptor;
        final_light_descriptor.width = 32;
        final_light_descriptor.height = 32;
        final_light_descriptor.colors[0].format =
            ALMetalPixelFormat::rgba16_float;
        final_light_descriptor.label =
            "Alchemy opaque post-deferred test lighting";
        auto final_light = ALMetalRenderTarget::create(
            *device, final_light_descriptor, &error);
        INFO(error);
        REQUIRE(final_light);
        ALMetalRenderPassConfig final_light_clear;
        final_light_clear.colors[0].load = ALMetalLoadAction::clear;
        final_light_clear.colors[0].store = ALMetalStoreAction::store;
        final_light_clear.colors[0].clear = {0.25, 0.125, 0.0625, 0.5};
        auto final_light_pass = final_light->makeRenderPass(
            final_light_clear, &error);
        auto final_light_command = device->makeCommandBuffer(
            "Alchemy opaque post-deferred test lighting clear", &error);
        auto final_light_encoder = final_light_pass && final_light_command
            ? ALMetalRenderEncoder::begin(
                  *final_light_command, *final_light_pass,
                  "Alchemy opaque post-deferred test clear encoder",
                  &error)
            : nullptr;
        INFO(error);
        REQUIRE(final_light_encoder);
        REQUIRE(final_light_encoder->end(&error));
        REQUIRE(final_light_command->commitAndWait(&error));

        ALMetalWorldOpaquePostDeferredScene opaque_forward_scene;
        opaque_forward_scene.gbuffer = coverage_result.get();
        opaque_forward_scene.composed_lighting = final_light.get();
        opaque_forward_scene.batches = &fullbright_batch;
        opaque_forward_scene.batch_count = 1;
        opaque_forward_scene.width = 32;
        opaque_forward_scene.height = 32;
        opaque_forward_scene.hdr = true;
        fullbright_batch.diffuse_textures[0] = nullptr;
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error ==
              "Metal world opaque post-deferred texture is invalid");
        fullbright_batch.diffuse_textures[0] = indexed_diffuse.get();
        error.clear();
        auto opaque_forward = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(opaque_forward);
        CHECK(opaque_forward->descriptor().color_attachment_count == 1);
        CHECK(opaque_forward->descriptor().colors[0].format ==
              ALMetalPixelFormat::rgba16_float);
        CHECK(opaque_forward->descriptor().depth_stencil_format ==
              ALMetalPixelFormat::depth32_float_stencil8);
        std::array<std::uint16_t, 32 * 32 * 4> opaque_forward_pixels{};
        std::array<float, 32 * 32> opaque_forward_depth{};
        REQUIRE(opaque_forward->colorTexture(0)->readback(
            *device, 0, 0, {}, opaque_forward_pixels.data(), 32 * 8,
            &error));
        REQUIRE(opaque_forward->depthStencilTexture()->readback(
            *device, 0, 0, {}, opaque_forward_depth.data(),
            32 * sizeof(float), &error));
        INFO(error);
        CHECK(opaque_forward_pixels[0] == 0x3400);
        CHECK(opaque_forward_pixels[1] == 0x3000);
        CHECK(opaque_forward_pixels[2] == 0x2c00);
        CHECK(opaque_forward_pixels[3] == 0x3800);
        CHECK(opaque_forward_pixels[fullbright_top + 0] == 0);
        CHECK(opaque_forward_pixels[fullbright_top + 1] == 0x3c00);
        CHECK(opaque_forward_pixels[fullbright_top + 2] == 0);
        CHECK(opaque_forward_pixels[fullbright_top + 3] >= 0x3800);
        CHECK(opaque_forward_pixels[fullbright_top + 3] <= 0x3808);
        CHECK(std::abs(
                  opaque_forward_depth[24 * 32 + 8] - 0.625f) < 0.0001f);
        CHECK(std::abs(opaque_forward_depth[0] - 1.f) < 0.0001f);

        // Water exclusion runs before opaque-forward geometry, tests the
        // retained scene depth without writing it, and marks only stencil
        // bit zero. Feed an all-zero water-height cell under a camera and
        // triangle that are both above the surface, then prove the mark by
        // drawing red only where that bit survived the transaction.
        ALMetalTextureDescriptor water_cell_descriptor;
        water_cell_descriptor.format = ALMetalPixelFormat::rg32_float;
        water_cell_descriptor.width = 1;
        water_cell_descriptor.height = 1;
        water_cell_descriptor.storage = ALMetalStorageMode::private_gpu;
        water_cell_descriptor.usage = ALMetalTextureUsage::shader_read;
        water_cell_descriptor.label =
            "Alchemy water-exclusion test height field";
        auto water_cell_map = ALMetalTexture::create(
            *device, water_cell_descriptor, &error);
        INFO(error);
        REQUIRE(water_cell_map);
        const std::array<float, 2> water_cell{{0.f, 1.f}};
        auto water_upload = device->makeCommandBuffer(
            "Alchemy water-exclusion test height upload", &error);
        REQUIRE(water_upload);
        REQUIRE(water_cell_map->upload(
            *water_upload, 0, 0, {}, water_cell.data(),
            sizeof(water_cell), &error));
        REQUIRE(water_upload->commitAndWait(&error));

        const std::array<float, 16> identity{{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        }};
        ALMetalWorldSimpleBatch water_exclusion_batch;
        water_exclusion_batch.vertices = vertices.get();
        water_exclusion_batch.vertex_count = positions.size();
        water_exclusion_batch.indices = indices.get();
        water_exclusion_batch.index_count = index_values.size();
        water_exclusion_batch.water_exclusion = true;
        std::memcpy(water_exclusion_batch.modelview_projection,
                    identity.data(), sizeof(identity));
        std::memcpy(water_exclusion_batch.modelview,
                    identity.data(), sizeof(identity));
        std::array<ALMetalWorldSimpleBatch, 2> final_batches{{
            water_exclusion_batch,
            fullbright_batch,
        }};
        ALMetalWorldWaterExclusionSurface water_surface;
        water_surface.cell_map = water_cell_map.get();
        std::memcpy(water_surface.eye_to_agent,
                    identity.data(), sizeof(identity));
        water_surface.cell_square[0] = -1.f;
        water_surface.cell_square[1] = -1.f;
        water_surface.cell_square[2] = 2.f;
        water_surface.cell_square[3] = 1.f;
        water_surface.camera_default_height[2] = 1.f;
        water_surface.camera_default_height[3] = 0.f;
        opaque_forward_scene.batches = final_batches.data();
        opaque_forward_scene.batch_count = final_batches.size();
        opaque_forward_scene.water_exclusion_surface = nullptr;
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error ==
              "Metal world water-exclusion surface is invalid");
        opaque_forward_scene.water_exclusion_surface = &water_surface;
        error.clear();
        auto water_excluded = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(water_excluded);

        auto stencil_library = ALMetalShaderLibrary::loadBuiltin(
            *device, &error);
        INFO(error);
        REQUIRE(stencil_library);
        ALMetalRenderPipelineDescriptor stencil_pipeline_descriptor;
        stencil_pipeline_descriptor.library = stencil_library.get();
        stencil_pipeline_descriptor.vertex_function =
            "almetal_test_vertex";
        stencil_pipeline_descriptor.fragment_function =
            "almetal_test_fragment";
        stencil_pipeline_descriptor.colors[0].format =
            ALMetalPixelFormat::rgba16_float;
        stencil_pipeline_descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        stencil_pipeline_descriptor.label =
            "Alchemy water-exclusion stencil proof pipeline";
        auto stencil_pipeline = ALMetalRenderPipeline::create(
            *device, stencil_pipeline_descriptor, &error);
        INFO(error);
        REQUIRE(stencil_pipeline);
        ALMetalDepthStencilDescriptor stencil_test_descriptor;
        stencil_test_descriptor.depth_compare =
            ALMetalCompareFunction::always;
        stencil_test_descriptor.depth_write_enabled = false;
        stencil_test_descriptor.stencil_enabled = true;
        stencil_test_descriptor.front_stencil.compare =
            ALMetalCompareFunction::equal;
        stencil_test_descriptor.front_stencil.read_mask = 1u;
        stencil_test_descriptor.front_stencil.write_mask = 0u;
        stencil_test_descriptor.back_stencil =
            stencil_test_descriptor.front_stencil;
        stencil_test_descriptor.label =
            "Alchemy water-exclusion stencil proof state";
        auto stencil_test_state = ALMetalDepthStencilState::create(
            *device, stencil_test_descriptor, &error);
        INFO(error);
        REQUIRE(stencil_test_state);
        struct StencilProofVertex
        {
            float position[2];
            float color[4];
        };
        const std::array<StencilProofVertex, 3> stencil_vertices{{
            {{-0.8f, -0.8f}, {1.f, 0.f, 0.f, 1.f}},
            {{ 0.8f, -0.8f}, {1.f, 0.f, 0.f, 1.f}},
            {{ 0.0f,  0.8f}, {1.f, 0.f, 0.f, 1.f}},
        }};
        ALMetalBufferDescriptor stencil_vertex_descriptor;
        stencil_vertex_descriptor.length = sizeof(stencil_vertices);
        stencil_vertex_descriptor.storage = ALMetalStorageMode::shared;
        stencil_vertex_descriptor.label =
            "Alchemy water-exclusion stencil proof vertices";
        auto stencil_vertex_buffer = ALMetalBuffer::create(
            *device, stencil_vertex_descriptor, &error);
        INFO(error);
        REQUIRE(stencil_vertex_buffer);
        REQUIRE(stencil_vertex_buffer->write(
            0, stencil_vertices.data(), sizeof(stencil_vertices), &error));
        ALMetalRenderPassConfig stencil_proof_config;
        stencil_proof_config.colors[0].load = ALMetalLoadAction::load;
        stencil_proof_config.colors[0].store = ALMetalStoreAction::store;
        stencil_proof_config.depth_load = ALMetalLoadAction::load;
        stencil_proof_config.depth_store = ALMetalStoreAction::store;
        stencil_proof_config.stencil_load = ALMetalLoadAction::load;
        stencil_proof_config.stencil_store = ALMetalStoreAction::store;
        auto stencil_proof_pass = water_excluded->makeRenderPass(
            stencil_proof_config, &error);
        auto stencil_proof_command = device->makeCommandBuffer(
            "Alchemy water-exclusion stencil proof", &error);
        auto stencil_proof_encoder =
            stencil_proof_pass && stencil_proof_command
            ? ALMetalRenderEncoder::begin(
                  *stencil_proof_command, *stencil_proof_pass,
                  "Alchemy water-exclusion stencil proof encoder",
                  &error)
            : nullptr;
        INFO(error);
        REQUIRE(stencil_proof_encoder);
        REQUIRE(stencil_proof_encoder->setPipeline(
            *stencil_pipeline, &error));
        REQUIRE(stencil_proof_encoder->setDepthStencilState(
            stencil_test_state.get(), &error));
        REQUIRE(stencil_proof_encoder->setStencilReferenceValue(
            1u, &error));
        REQUIRE(stencil_proof_encoder->setVertexBuffer(
            0, *stencil_vertex_buffer, 0, &error));
        REQUIRE(stencil_proof_encoder->setViewport(
            {0.0, 0.0, 32.0, 32.0, 0.0, 1.0}, &error));
        REQUIRE(stencil_proof_encoder->setScissor(
            {0, 0, 32, 32}, &error));
        REQUIRE(stencil_proof_encoder->setCullMode(
            ALMetalCullMode::none, &error));
        REQUIRE(stencil_proof_encoder->draw(
            ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        REQUIRE(stencil_proof_encoder->end(&error));
        REQUIRE(stencil_proof_command->commitAndWait(&error));
        INFO(error);
        std::array<std::uint16_t, 32 * 32 * 4> stencil_pixels{};
        REQUIRE(water_excluded->colorTexture(0)->readback(
            *device, 0, 0, {}, stencil_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        const std::size_t stencil_center = (16 * 32 + 16) * 4;
        CHECK(stencil_pixels[stencil_center + 0] == 0x3c00);
        CHECK(stencil_pixels[stencil_center + 1] == 0);
        CHECK(stencil_pixels[stencil_center + 2] == 0);
        CHECK(stencil_pixels[stencil_center + 3] == 0x3c00);
        CHECK(stencil_pixels[0] == 0x3400);
        CHECK(stencil_pixels[1] == 0x3000);
        CHECK(stencil_pixels[2] == 0x2c00);
        CHECK(stencil_pixels[3] == 0x3800);

        // Water haze is an ordered procedural marker rather than a mesh
        // batch. Above water it reuses the live field, reads the retained
        // scene depth, and keeps pixels marked by water exclusion untouched.
        // Underwater it uses the same fog equation over a full-screen
        // triangle without depending on a field texture or stencil.
        ALMetalWorldWaterHaze water_haze;
        water_haze.cell_map = water_cell_map.get();
        std::memcpy(water_haze.modelview_projection,
                    identity.data(), sizeof(identity));
        std::memcpy(water_haze.inverse_projection,
                    identity.data(), sizeof(identity));
        water_haze.cell_square[0] = -1.f;
        water_haze.cell_square[1] = -1.f;
        water_haze.cell_square[2] = 2.f;
        water_haze.cell_square[3] = 1.f;
        water_haze.rings[0] = 1.f;
        water_haze.rings[1] = 1.f;
        water_haze.rings[2] = 4.f;
        water_haze.rings[3] = 0.f;
        water_haze.water_plane[2] = -1.f;
        water_haze.fog_color[0] = 0.5f;
        water_haze.fog_color[1] = 0.25f;
        water_haze.fog_color[2] = 0.125f;
        water_haze.fog_color[3] = 1.f;
        water_haze.fog[0] = 1.f;
        water_haze.fog[1] = 1.f;
        water_haze.draws[0] = {0u, 4u, 1u, true};
        water_haze.draws[1] = {1u, 9u, 12u, false};
        water_haze.draws[2] = {2u, 9u, 4u, false};
        water_haze.draw_count = 3;
        water_haze.above_water = true;
        ALMetalWorldSimpleBatch water_haze_marker;
        water_haze_marker.water_haze = true;
        std::array<ALMetalWorldSimpleBatch, 2> water_haze_batches{{
            water_exclusion_batch,
            water_haze_marker,
        }};
        opaque_forward_scene.batches = water_haze_batches.data();
        opaque_forward_scene.batch_count = water_haze_batches.size();
        opaque_forward_scene.water_exclusion_surface = &water_surface;
        opaque_forward_scene.water_haze = nullptr;
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error == "Metal world water-haze inputs are invalid");
        opaque_forward_scene.water_haze = &water_haze;
        std::array<ALMetalWorldSimpleBatch, 2> misordered_haze{{
            water_haze_marker,
            fullbright_batch,
        }};
        opaque_forward_scene.batches = misordered_haze.data();
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error == "Metal world water-haze ordering is invalid");
        opaque_forward_scene.batches = water_haze_batches.data();
        error.clear();
        auto above_water_haze = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(above_water_haze);
        std::array<std::uint16_t, 32 * 32 * 4> water_haze_pixels{};
        std::array<float, 32 * 32> water_haze_depth{};
        REQUIRE(above_water_haze->colorTexture(0)->readback(
            *device, 0, 0, {}, water_haze_pixels.data(), 32 * 8,
            &error));
        REQUIRE(above_water_haze->depthStencilTexture()->readback(
            *device, 0, 0, {}, water_haze_depth.data(),
            32 * sizeof(float), &error));
        INFO(error);
        CHECK(water_haze_pixels[stencil_center + 0] == 0x3400);
        CHECK(water_haze_pixels[stencil_center + 1] == 0x3000);
        CHECK(water_haze_pixels[stencil_center + 2] == 0x2c00);
        CHECK(water_haze_pixels[stencil_center + 3] == 0x3800);
        CHECK(water_haze_pixels[0] != 0x3400);
        CHECK(water_haze_pixels[3] < 0x3800);
        CHECK(std::abs(
                  water_haze_depth[stencil_center / 4] - 0.625f) <
              0.0001f);
        CHECK(std::abs(water_haze_depth[0] - 1.f) < 0.0001f);

        water_haze.above_water = false;
        water_haze.cell_map = nullptr;
        water_haze.draw_count = 0;
        opaque_forward_scene.batches = &water_haze_marker;
        opaque_forward_scene.batch_count = 1;
        opaque_forward_scene.water_exclusion_surface = nullptr;
        auto underwater_haze = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(underwater_haze);
        REQUIRE(underwater_haze->colorTexture(0)->readback(
            *device, 0, 0, {}, water_haze_pixels.data(), 32 * 8,
            &error));
        REQUIRE(underwater_haze->depthStencilTexture()->readback(
            *device, 0, 0, {}, water_haze_depth.data(),
            32 * sizeof(float), &error));
        INFO(error);
        CHECK(water_haze_pixels[stencil_center + 0] != 0x3400);
        CHECK(water_haze_pixels[stencil_center + 3] < 0x3800);
        CHECK(std::abs(
                  water_haze_depth[stencil_center / 4] - 0.625f) <
              0.0001f);

        const std::array<float, 2> reverse_water_cell{{0.5f, 1.f}};
        auto reverse_water_upload = device->makeCommandBuffer(
            "Alchemy reverse water-haze height upload", &error);
        REQUIRE(reverse_water_upload);
        REQUIRE(water_cell_map->upload(
            *reverse_water_upload, 0, 0, {}, reverse_water_cell.data(),
            sizeof(reverse_water_cell), &error));
        REQUIRE(reverse_water_upload->commitAndWait(&error));
        water_haze.above_water = true;
        water_haze.cell_map = water_cell_map.get();
        water_haze.draw_count = 3;
        opaque_forward_scene.gbuffer = reverse_coverage_result.get();
        opaque_forward_scene.reverse_z = true;
        auto reverse_water_haze = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(reverse_water_haze);
        REQUIRE(reverse_water_haze->colorTexture(0)->readback(
            *device, 0, 0, {}, water_haze_pixels.data(), 32 * 8,
            &error));
        REQUIRE(reverse_water_haze->depthStencilTexture()->readback(
            *device, 0, 0, {}, water_haze_depth.data(),
            32 * sizeof(float), &error));
        INFO(error);
        CHECK(water_haze_pixels[stencil_center + 0] != 0x3400);
        CHECK(water_haze_pixels[stencil_center + 3] < 0x3800);
        CHECK(std::abs(
                  water_haze_depth[stencil_center / 4] - 0.25f) <
              0.0001f);

        // Pre-water alpha follows water haze in the final-color owner. The
        // first native family is the exact fogged fullbright program: source
        // over RGB, destination alpha multiplied by uncovered coverage, and
        // writable depth for both static and rigged geometry.
        ALMetalWorldAlphaEnvironment alpha_environment;
        alpha_environment.water_plane[2] = 1.f;
        alpha_environment.water_plane[3] = -1.f;
        alpha_environment.eye_to_sky[0][0] = 1.f;
        alpha_environment.eye_to_sky[1][1] = 1.f;
        alpha_environment.eye_to_sky[2][2] = 1.f;
        alpha_environment.glow_max_y[0] = 1.f;
        alpha_environment.glow_max_y[2] = -1.f;
        alpha_environment.glow_max_y[3] = 1.f;
        alpha_environment.light_direction[2] = 1.f;
        alpha_environment.atmospheric[1] = 1.f;
        alpha_environment.atmospheric[2] = 1.f;
        alpha_environment.atmospheric[3] = 1.f;
        alpha_environment.water_fog[0] = 1.f;
        alpha_environment.water_fog[2] = -1.f;

        ALMetalWorldSimpleBatch pre_water_alpha = fullbright_batch;
        pre_water_alpha.impostor_fullbright = false;
        pre_water_alpha.alpha_mask = false;
        pre_water_alpha.alpha_cutoff = 0.f;
        pre_water_alpha.pre_water_alpha = true;
        pre_water_alpha.pre_water_fullbright = true;
        std::fill(std::begin(pre_water_alpha.modelview),
                  std::end(pre_water_alpha.modelview), 0.f);
        pre_water_alpha.modelview[14] = 0.25f;
        pre_water_alpha.modelview[15] = 1.f;
        std::fill(std::begin(pre_water_alpha.modelview_projection),
                  std::end(pre_water_alpha.modelview_projection), 0.f);
        for (std::size_t diagonal = 0; diagonal < 4; ++diagonal)
        {
            pre_water_alpha.modelview_projection[
                diagonal * 4 + diagonal] = 1.f;
        }
        pre_water_alpha.modelview_projection[12] = -0.8f;
        pre_water_alpha.modelview_projection[14] = -0.25f;
        pre_water_alpha.skin_palette = nullptr;
        pre_water_alpha.skin_palette_float_count = 0;
        pre_water_alpha.skin_joint_count = 0;
        pre_water_alpha.weight_offset = 0;

        ALMetalWorldSimpleBatch pre_water_alpha_rigged =
            pre_water_alpha;
        pre_water_alpha_rigged.modelview_projection[12] = 0.8f;
        pre_water_alpha_rigged.weight_offset = weight_offset;
        pre_water_alpha_rigged.skin_palette =
            alpha_skin_palette.data();
        pre_water_alpha_rigged.skin_palette_float_count =
            alpha_skin_palette.size();
        pre_water_alpha_rigged.skin_joint_count = 1;
        for (std::size_t diagonal = 0; diagonal < 3; ++diagonal)
        {
            pre_water_alpha_rigged.rigged_normal_matrix
                [diagonal][diagonal] = 1.f;
        }
        std::array<ALMetalWorldSimpleBatch, 2> pre_water_batches{{
            pre_water_alpha,
            pre_water_alpha_rigged,
        }};
        opaque_forward_scene.batches = pre_water_batches.data();
        opaque_forward_scene.batch_count = pre_water_batches.size();
        opaque_forward_scene.gbuffer = coverage_result.get();
        opaque_forward_scene.reverse_z = false;
        opaque_forward_scene.water_exclusion_surface = nullptr;
        opaque_forward_scene.water_haze = nullptr;
        opaque_forward_scene.alpha_environment = nullptr;
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error ==
              "Metal world pre-water alpha environment is invalid");
        opaque_forward_scene.alpha_environment = &alpha_environment;
        error.clear();
        auto pre_water_result = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(pre_water_result);
        std::array<std::uint16_t, 32 * 32 * 4> pre_water_pixels{};
        std::array<float, 32 * 32> pre_water_depth{};
        REQUIRE(pre_water_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        REQUIRE(pre_water_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, pre_water_depth.data(),
            32 * sizeof(float), &error));
        INFO(error);
        const std::size_t pre_water_left = (16 * 32 + 6) * 4;
        const std::size_t pre_water_right = (16 * 32 + 26) * 4;
        for (const std::size_t pixel : {pre_water_left,
                                        pre_water_right})
        {
            CHECK(pre_water_pixels[pixel + 0] >= 0x2ff0);
            CHECK(pre_water_pixels[pixel + 0] <= 0x3000);
            CHECK(pre_water_pixels[pixel + 1] >= 0x3880);
            CHECK(pre_water_pixels[pixel + 1] <= 0x3890);
            CHECK(pre_water_pixels[pixel + 2] >= 0x27f0);
            CHECK(pre_water_pixels[pixel + 2] <= 0x2800);
            CHECK(pre_water_pixels[pixel + 3] >= 0x33f0);
            CHECK(pre_water_pixels[pixel + 3] <= 0x3400);
            CHECK(std::abs(pre_water_depth[pixel / 4] - 0.5f) <
                  0.0001f);
        }

        const auto half_to_float = [](std::uint16_t value)
        {
            const float magnitude = ((value >> 10) & 0x1fu) == 0
                ? std::ldexp(static_cast<float>(value & 0x3ffu), -24)
                : std::ldexp(
                      static_cast<float>((value & 0x3ffu) + 0x400u),
                      static_cast<int>((value >> 10) & 0x1fu) - 25);
            return (value & 0x8000u) != 0 ? -magnitude : magnitude;
        };
        const float coverage = 128.f / 255.f;
        const float uncovered = 1.f - coverage;

        // A constant eye-space position makes the reference water-fog line
        // integral independent of raster position. Compare the actual half
        // output against the GLSL equation, then do the same for atmospheric
        // extinction and additive haze below.
        alpha_environment.water_fog_color[0] = 0.2f;
        alpha_environment.water_fog_color[1] = 0.4f;
        alpha_environment.water_fog_color[2] = 0.6f;
        alpha_environment.water_fog[1] = 2.f;
        auto fogged_pre_water = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(fogged_pre_water);
        REQUIRE(fogged_pre_water->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        constexpr float fog_base = 0.98f;
        const float fog_t1 = -2.f * std::pow(fog_base, 1.f);
        const float fog_t2 = 2.f - 1.f;
        const float fog_t3 = std::pow(fog_base, fog_t2 * 0.25f) - 1.f;
        const float fog_light = std::pow(
            std::min(fog_t1 / fog_t2 * fog_t3, 1.f), 1.f / 1.7f);
        const float fog_transmission = std::pow(fog_base, 0.5f);
        const std::array<float, 4> expected_water_fog{{
            (0.2f * fog_light) * coverage + 0.25f * uncovered,
            (fog_transmission + 0.4f * fog_light) * coverage +
                0.125f * uncovered,
            (0.6f * fog_light) * coverage + 0.0625f * uncovered,
            0.5f * uncovered,
        }};
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(std::abs(
                half_to_float(pre_water_pixels[pre_water_left + channel]) -
                expected_water_fog[channel]) < 0.002f);
        }

        const auto srgb_to_linear = [](float value)
        {
            return value <= 0.04045f
                ? value / 12.92f
                : std::pow((value + 0.055f) / 1.055f, 2.4f);
        };
        alpha_environment.water_plane[3] = 1.f;
        alpha_environment.water_fog[1] = 0.f;
        alpha_environment.water_fog[2] = 1.f;
        alpha_environment.atmospheric[0] = 1.f;
        alpha_environment.haze_extinction[0] = 2.f;
        alpha_environment.haze_extinction[1] = 2.f;
        alpha_environment.haze_extinction[2] = 2.f;
        alpha_environment.haze_additive[0] = 0.1f;
        alpha_environment.haze_additive[1] = 0.2f;
        alpha_environment.haze_additive[2] = 0.3f;
        auto atmospheric_pre_water = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(atmospheric_pre_water);
        REQUIRE(atmospheric_pre_water->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        const float attenuation = std::exp(-2.f * 0.25f);
        const std::array<float, 3> haze_additive{{0.1f, 0.2f, 0.3f}};
        std::array<float, 4> expected_atmosphere{{
            0.f, attenuation, 0.f, 0.5f * uncovered,
        }};
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const float additive = srgb_to_linear(
                haze_additive[channel] * (1.f - attenuation) * 2.f);
            expected_atmosphere[channel] =
                (expected_atmosphere[channel] + additive) * coverage +
                std::array<float, 3>{{0.25f, 0.125f, 0.0625f}}[channel] *
                    uncovered;
        }
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(std::abs(
                half_to_float(pre_water_pixels[pre_water_left + channel]) -
                expected_atmosphere[channel]) < 0.002f);
        }

        // Flip the clip side: the same geometry must leave both color and
        // depth at their copied inputs.
        alpha_environment.water_fog[2] = -1.f;
        auto clipped_pre_water = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(clipped_pre_water);
        REQUIRE(clipped_pre_water->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        REQUIRE(clipped_pre_water->depthStencilTexture()->readback(
            *device, 0, 0, {}, pre_water_depth.data(),
            32 * sizeof(float), &error));
        INFO(error);
        CHECK(pre_water_pixels[pre_water_left + 0] == 0x3400);
        CHECK(pre_water_pixels[pre_water_left + 1] == 0x3000);
        CHECK(pre_water_pixels[pre_water_left + 2] == 0x2c00);
        CHECK(pre_water_pixels[pre_water_left + 3] == 0x3800);
        CHECK(std::abs(pre_water_depth[pre_water_left / 4] - 0.625f) <
              0.0001f);
        alpha_environment.water_plane[3] = -1.f;
        alpha_environment.water_fog[2] = -1.f;
        alpha_environment.atmospheric[0] = 0.f;
        alpha_environment.haze_extinction[0] = 0.f;
        alpha_environment.haze_extinction[1] = 0.f;
        alpha_environment.haze_extinction[2] = 0.f;
        alpha_environment.haze_additive[0] = 0.f;
        alpha_environment.haze_additive[1] = 0.f;
        alpha_environment.haze_additive[2] = 0.f;

        std::array<ALMetalWorldSimpleBatch, 2> ordered_haze_alpha{{
            water_haze_marker,
            pre_water_alpha,
        }};
        opaque_forward_scene.batches = ordered_haze_alpha.data();
        opaque_forward_scene.batch_count = ordered_haze_alpha.size();
        opaque_forward_scene.water_haze = &water_haze;
        auto ordered_haze_alpha_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(ordered_haze_alpha_result);
        REQUIRE(ordered_haze_alpha_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, pre_water_depth.data(),
            32 * sizeof(float), &error));
        INFO(error);
        CHECK(std::abs(pre_water_depth[pre_water_left / 4] - 0.5f) <
              0.0001f);

        // Alpha cannot precede the water-haze marker. The valid production
        // order is haze followed by one or more alpha batches.
        std::array<ALMetalWorldSimpleBatch, 2> alpha_before_haze{{
            pre_water_alpha,
            water_haze_marker,
        }};
        opaque_forward_scene.batches = alpha_before_haze.data();
        opaque_forward_scene.batch_count = alpha_before_haze.size();
        opaque_forward_scene.water_haze = &water_haze;
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error == "Metal world water-haze ordering is invalid");

        opaque_forward_scene.batches = pre_water_batches.data();
        opaque_forward_scene.batch_count = pre_water_batches.size();
        opaque_forward_scene.water_haze = nullptr;
        opaque_forward_scene.gbuffer = reverse_coverage_result.get();
        opaque_forward_scene.reverse_z = true;
        pre_water_batches[0].modelview_projection[14] = 0.25f;
        pre_water_batches[1].modelview_projection[14] = 0.25f;
        error.clear();
        auto reverse_pre_water = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(reverse_pre_water);
        REQUIRE(reverse_pre_water->depthStencilTexture()->readback(
            *device, 0, 0, {}, pre_water_depth.data(),
            32 * sizeof(float), &error));
        INFO(error);
        CHECK(std::abs(pre_water_depth[pre_water_left / 4] - 0.5f) <
              0.0001f);
        CHECK(std::abs(pre_water_depth[pre_water_right / 4] - 0.5f) <
              0.0001f);

        opaque_forward_scene.batches = &fullbright_batch;
        opaque_forward_scene.batch_count = 1;
        opaque_forward_scene.gbuffer = coverage_result.get();
        opaque_forward_scene.reverse_z = false;
        opaque_forward_scene.water_exclusion_surface = nullptr;
        opaque_forward_scene.water_haze = nullptr;
        opaque_forward_scene.alpha_environment = nullptr;

        // The two glow owners share alpha-only output but not blending:
        // opaque PBR glow replaces destination alpha, then legacy glow adds
        // its diffuse-alpha-times-emissive coverage. Replay both static and
        // rigged variants at retained equal depth and prove RGB/depth survive.
        ALMetalWorldSimpleBatch pbr_glow_batch;
        pbr_glow_batch.vertices = vertices.get();
        pbr_glow_batch.position_offset = 0;
        pbr_glow_batch.texcoord_offset = texcoord_offset;
        pbr_glow_batch.emissive_offset = emissive_offset;
        pbr_glow_batch.vertex_count = positions.size();
        pbr_glow_batch.indices = indices.get();
        pbr_glow_batch.index_count = index_values.size();
        pbr_glow_batch.diffuse_textures[0] = indexed_diffuse.get();
        pbr_glow_batch.texture_count = 1;
        pbr_glow_batch.pbr_slots[0].emissive_texture =
            indexed_diffuse.get();
        pbr_glow_batch.pbr_slots[0].emissive_color[1] = 0.5f;
        pbr_glow_batch.pbr_slots[0].base_color_factor[3] = 1.f;
        pbr_glow_batch.pbr_slots[0].alpha_cutoff = -1.f;
        pbr_glow_batch.pbr_slot_count = 1;
        pbr_glow_batch.pbr_glow = true;
        std::memcpy(pbr_glow_batch.modelview_projection,
                    identity.data(), sizeof(identity));

        ALMetalWorldSimpleBatch pbr_glow_rigged = pbr_glow_batch;
        pbr_glow_rigged.weight_offset = weight_offset;
        pbr_glow_rigged.skin_palette = alpha_skin_palette.data();
        pbr_glow_rigged.skin_palette_float_count =
            alpha_skin_palette.size();
        pbr_glow_rigged.skin_joint_count = 1;
        std::array<ALMetalWorldSimpleBatch, 2> pbr_glow_batches{{
            pbr_glow_batch,
            pbr_glow_rigged,
        }};
        opaque_forward_scene.batches = pbr_glow_batches.data();
        opaque_forward_scene.batch_count = pbr_glow_batches.size();

        pbr_glow_batches[0].emissive_offset = vertex_bytes.size();
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error ==
              "Metal world opaque post-deferred batch is invalid");
        pbr_glow_batches[0].emissive_offset = emissive_offset;
        pbr_glow_batches[0].pbr_slots[0].emissive_texture = nullptr;
        error.clear();
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error == "Metal world PBR glow slot is invalid");
        pbr_glow_batches[0].pbr_slots[0].emissive_texture =
            indexed_diffuse.get();
        error.clear();
        auto pbr_glow_result = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(pbr_glow_result);
        std::array<std::uint16_t, 32 * 32 * 4> glow_pixels{};
        std::array<float, 32 * 32> glow_depth{};
        REQUIRE(pbr_glow_result->colorTexture(0)->readback(
            *device, 0, 0, {}, glow_pixels.data(), 32 * 8, &error));
        REQUIRE(pbr_glow_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, glow_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        CHECK(glow_pixels[center + 0] == 0x3400);
        CHECK(glow_pixels[center + 1] == 0x3000);
        CHECK(glow_pixels[center + 2] == 0x2c00);
        CHECK(glow_pixels[center + 3] >= 0x3400);
        CHECK(glow_pixels[center + 3] <= 0x3410);
        CHECK(std::abs(glow_depth[center / 4] - 0.625f) < 0.0001f);

        ALMetalWorldSimpleBatch legacy_glow_batch = pbr_glow_batch;
        legacy_glow_batch.diffuse_textures[0] = tree_diffuse.get();
        legacy_glow_batch.pbr_slot_count = 0;
        legacy_glow_batch.pbr_glow = false;
        legacy_glow_batch.legacy_glow = true;
        ALMetalWorldSimpleBatch legacy_glow_rigged = legacy_glow_batch;
        legacy_glow_rigged.weight_offset = weight_offset;
        legacy_glow_rigged.skin_palette = alpha_skin_palette.data();
        legacy_glow_rigged.skin_palette_float_count =
            alpha_skin_palette.size();
        legacy_glow_rigged.skin_joint_count = 1;
        std::array<ALMetalWorldSimpleBatch, 4> glow_batches{{
            pbr_glow_batch,
            pbr_glow_rigged,
            legacy_glow_batch,
            legacy_glow_rigged,
        }};
        opaque_forward_scene.batches = glow_batches.data();
        opaque_forward_scene.batch_count = glow_batches.size();
        auto glow_result = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(glow_result);
        REQUIRE(glow_result->colorTexture(0)->readback(
            *device, 0, 0, {}, glow_pixels.data(), 32 * 8, &error));
        REQUIRE(glow_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, glow_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        CHECK(glow_pixels[center + 0] == 0x3400);
        CHECK(glow_pixels[center + 1] == 0x3000);
        CHECK(glow_pixels[center + 2] == 0x2c00);
        CHECK(glow_pixels[center + 3] > 0x3a00);
        CHECK(glow_pixels[center + 3] < 0x3a20);
        CHECK(std::abs(glow_depth[center / 4] - 0.625f) < 0.0001f);
        CHECK(glow_pixels[0] == 0x3400);
        CHECK(glow_pixels[1] == 0x3000);
        CHECK(glow_pixels[2] == 0x2c00);
        CHECK(glow_pixels[3] == 0x3800);

        opaque_forward_scene.batches = &fullbright_batch;
        opaque_forward_scene.batch_count = 1;

        // Fullbright shiny is opaque in the impostor bake, but its RGB is
        // the exact legacy reflection-probe result: decoded diffuse times
        // linear tint mixed with the selected live cubemap at vertex-alpha
        // intensity. Use two single-texel cubes with distinct colours to
        // prove that refIndex selects the requested array element rather
        // than silently sampling cube zero.
        ALMetalTextureDescriptor probe_descriptor;
        probe_descriptor.type = ALMetalTextureType::texture_cube_array;
        probe_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        probe_descriptor.width = 1;
        probe_descriptor.height = 1;
        probe_descriptor.array_length = 2;
        probe_descriptor.storage = ALMetalStorageMode::private_gpu;
        probe_descriptor.usage = ALMetalTextureUsage::shader_read;
        probe_descriptor.label =
            "Alchemy world shiny reflection-probe array";
        auto probe_texture = ALMetalTexture::create(
            *device, probe_descriptor, &error);
        INFO(error);
        REQUIRE(probe_texture);
        auto probe_upload = device->makeCommandBuffer(
            "Alchemy world shiny reflection-probe upload", &error);
        INFO(error);
        REQUIRE(probe_upload);
        for (std::uint32_t slice = 0; slice < 12; ++slice)
        {
            const std::array<std::uint8_t, 4> pixel = slice < 6
                ? std::array<std::uint8_t, 4>{{255, 0, 0, 255}}
                : std::array<std::uint8_t, 4>{{0, 0, 255, 255}};
            REQUIRE(probe_texture->upload(
                *probe_upload, 0, slice, {}, pixel.data(), 4, &error));
        }
        REQUIRE(probe_upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldReflectionProbes reflection_probes;
        reflection_probes.radiance_texture = probe_texture.get();
        reflection_probes.constants.ref_sphere[0][3] = 100.f;
        reflection_probes.constants.ref_params[0][1] = 1.f;
        reflection_probes.constants.ref_params[0][2] = 1.f;
        reflection_probes.constants.ref_index[0][0] = 0;
        reflection_probes.constants.ref_index[0][1] = -1;
        reflection_probes.constants.ref_sphere[1][3] = 100.f;
        reflection_probes.constants.ref_params[1][1] = 1.f;
        reflection_probes.constants.ref_params[1][2] = 1.f;
        reflection_probes.constants.ref_params[1][3] = -100.f;
        reflection_probes.constants.ref_index[1][0] = 1;
        reflection_probes.constants.ref_index[1][1] = -1;
        reflection_probes.constants.ref_index[1][3] = 1;
        reflection_probes.constants.config[0] = 2.f;
        reflection_probes.constants.env_matrix[0][0] = 1.f;
        reflection_probes.constants.env_matrix[1][1] = 1.f;
        reflection_probes.constants.env_matrix[2][2] = 1.f;

        ALMetalTextureDescriptor irradiance_descriptor;
        irradiance_descriptor.format = ALMetalPixelFormat::rgba32_uint;
        irradiance_descriptor.width = 4;
        irradiance_descriptor.height = 2;
        irradiance_descriptor.storage = ALMetalStorageMode::private_gpu;
        irradiance_descriptor.usage = ALMetalTextureUsage::shader_read;
        irradiance_descriptor.label =
            "Alchemy world simple-alpha irradiance test";
        auto irradiance_texture = ALMetalTexture::create(
            *device, irradiance_descriptor, &error);
        INFO(error);
        REQUIRE(irradiance_texture);
        std::array<std::uint32_t, 4 * 2 * 4> zero_irradiance{};
        auto irradiance_upload = device->makeCommandBuffer(
            "Alchemy world simple-alpha irradiance upload", &error);
        REQUIRE(irradiance_upload);
        REQUIRE(irradiance_texture->upload(
            *irradiance_upload, 0, 0, {}, zero_irradiance.data(),
            4 * 4 * sizeof(std::uint32_t), &error));
        REQUIRE(irradiance_upload->commitAndWait(&error));
        INFO(error);
        reflection_probes.irradiance_texture = irradiance_texture.get();

        // The lit/simple pre-water family adds the exact forward ambient,
        // directional and punctual model on top of the fullbright coverage,
        // fog and depth contract. A zero-ambiance SH pack leaves the live
        // Environment ambient unchanged, so this fixture isolates the
        // modern directional term while exercising both static and rigged
        // pipelines.
        ALMetalWorldLegacyAlphaLighting simple_alpha_lighting;
        simple_alpha_lighting.sun_direction[2] = 1.f;
        simple_alpha_lighting.parameters[0] = 128.f;
        simple_alpha_lighting.parameters[1] = 0.004f;
        simple_alpha_lighting.parameters[2] = 1.f;
        alpha_environment.atmos_sunlit_linear[0] = 1.f;
        alpha_environment.atmos_sunlit_linear[1] = 0.5f;
        alpha_environment.atmos_sunlit_linear[2] = 0.25f;
        alpha_environment.atmos_sunlit[3] = 1.f;
        alpha_environment.atmos_sunlit_linear[3] = 1.f;
        alpha_environment.atmos_amblit[0] = 0.f;
        alpha_environment.atmos_amblit[1] = 0.f;
        alpha_environment.atmos_amblit[2] = 0.f;
        alpha_environment.water_fog[1] = 0.f;

        ALMetalWorldSimpleBatch simple_pre_water = pre_water_alpha;
        simple_pre_water.pre_water_fullbright = false;
        simple_pre_water.pre_water_simple = true;
        simple_pre_water.pre_water_shadows = false;
        ALMetalWorldSimpleBatch simple_pre_water_rigged =
            pre_water_alpha_rigged;
        simple_pre_water_rigged.pre_water_fullbright = false;
        simple_pre_water_rigged.pre_water_simple = true;
        simple_pre_water_rigged.pre_water_shadows = false;
        std::array<ALMetalWorldSimpleBatch, 2> simple_pre_water_batches{{
            simple_pre_water,
            simple_pre_water_rigged,
        }};
        opaque_forward_scene.batches = simple_pre_water_batches.data();
        opaque_forward_scene.batch_count = simple_pre_water_batches.size();
        opaque_forward_scene.gbuffer = coverage_result.get();
        opaque_forward_scene.reverse_z = false;
        opaque_forward_scene.water_exclusion_surface = nullptr;
        opaque_forward_scene.water_haze = nullptr;
        opaque_forward_scene.alpha_environment = &alpha_environment;
        opaque_forward_scene.reflection_probes = &reflection_probes;
        opaque_forward_scene.alpha_lighting = nullptr;
        opaque_forward_scene.alpha_directional_shadows = nullptr;
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error ==
              "Metal world pre-water alpha lighting is invalid");
        opaque_forward_scene.alpha_lighting = &simple_alpha_lighting;
        reflection_probes.irradiance_texture = nullptr;
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error ==
              "Metal world pre-water alpha lighting is invalid");
        reflection_probes.irradiance_texture = irradiance_texture.get();
        error.clear();
        auto simple_pre_water_result = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(simple_pre_water_result);
        REQUIRE(simple_pre_water_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        REQUIRE(simple_pre_water_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, pre_water_depth.data(),
            32 * sizeof(float), &error));
        INFO(error);
        for (const std::size_t pixel : {pre_water_left,
                                        pre_water_right})
        {
            CHECK(std::abs(
                half_to_float(pre_water_pixels[pixel + 0]) - 0.125f) <
                0.002f);
            CHECK(std::abs(
                half_to_float(pre_water_pixels[pixel + 1]) - 0.3125f) <
                0.002f);
            CHECK(std::abs(
                half_to_float(pre_water_pixels[pixel + 2]) - 0.03125f) <
                0.002f);
            CHECK(std::abs(
                half_to_float(pre_water_pixels[pixel + 3]) - 0.25f) <
                0.002f);
            CHECK(std::abs(pre_water_depth[pixel / 4] - 0.5f) <
                  0.0001f);
        }

        simple_alpha_lighting.parameters[3] = 1.f;
        alpha_environment.atmos_sunlit[0] = 0.f;
        alpha_environment.atmos_sunlit[1] = 1.f;
        alpha_environment.atmos_sunlit[2] = 0.f;
        auto classic_simple_result = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(classic_simple_result);
        REQUIRE(classic_simple_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        const float classic_source_green =
            srgb_to_linear(0.7f * 1.35f) * 1.1f;
        CHECK(std::abs(
            half_to_float(pre_water_pixels[pre_water_left + 1]) -
            (classic_source_green * coverage + 0.125f * uncovered)) <
            0.002f);

        simple_alpha_lighting.parameters[3] = 0.f;
        alpha_environment.atmos_sunlit_linear[0] = 0.f;
        alpha_environment.atmos_sunlit_linear[1] = 0.f;
        alpha_environment.atmos_sunlit_linear[2] = 0.f;
        ALMetalWorldLegacyAlphaLight& local_light =
            simple_alpha_lighting.local_lights[0];
        local_light.position[2] = 10.f;
        local_light.diffuse[1] = 0.5f;
        local_light.parameters[0] = 100.f;
        local_light.parameters[2] = 1.f;
        auto local_simple_result = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(local_simple_result);
        REQUIRE(local_simple_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(half_to_float(pre_water_pixels[pre_water_left + 1]) >
              0.0625f);
        local_light = {};
        alpha_environment.atmos_sunlit_linear[0] = 1.f;
        alpha_environment.atmos_sunlit_linear[1] = 0.5f;
        alpha_environment.atmos_sunlit_linear[2] = 0.25f;

        ALMetalTextureDescriptor hero_descriptor;
        hero_descriptor.type = ALMetalTextureType::texture_cube;
        hero_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        hero_descriptor.width = 1;
        hero_descriptor.height = 1;
        hero_descriptor.storage = ALMetalStorageMode::private_gpu;
        hero_descriptor.usage = ALMetalTextureUsage::shader_read;
        hero_descriptor.label =
            "Alchemy world hero-probe test cube";
        auto hero_texture = ALMetalTexture::create(
            *device, hero_descriptor, &error);
        INFO(error);
        REQUIRE(hero_texture);
        auto hero_upload = device->makeCommandBuffer(
            "Alchemy world hero-probe black upload", &error);
        INFO(error);
        REQUIRE(hero_upload);
        const std::array<std::uint8_t, 4> black_hero{{0, 0, 0, 255}};
        for (std::uint32_t face = 0; face < 6; ++face)
        {
            REQUIRE(hero_texture->upload(
                *hero_upload, 0, face, {}, black_hero.data(), 4,
                &error));
        }
        REQUIRE(hero_upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldHeroProbe hero_probe;
        hero_probe.radiance_texture = hero_texture.get();
        hero_probe.constants.clip_plane[3] = 1.f;
        hero_probe.constants.config[1] = 1;

        ALMetalTextureDescriptor ssr_color_descriptor;
        ssr_color_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        ssr_color_descriptor.width = 1;
        ssr_color_descriptor.height = 2;
        ssr_color_descriptor.storage = ALMetalStorageMode::private_gpu;
        ssr_color_descriptor.usage = ALMetalTextureUsage::shader_read;
        ssr_color_descriptor.label =
            "Alchemy world SSR scene-color test";
        auto ssr_color = ALMetalTexture::create(
            *device, ssr_color_descriptor, &error);
        INFO(error);
        REQUIRE(ssr_color);
        ALMetalTextureDescriptor ssr_depth_descriptor =
            ssr_color_descriptor;
        ssr_depth_descriptor.format = ALMetalPixelFormat::r32_float;
        ssr_depth_descriptor.label =
            "Alchemy world SSR scene-depth test";
        auto ssr_depth = ALMetalTexture::create(
            *device, ssr_depth_descriptor, &error);
        INFO(error);
        REQUIRE(ssr_depth);
        ALMetalTextureDescriptor ssr_normal_descriptor =
            ssr_color_descriptor;
        ssr_normal_descriptor.height = 1;
        ssr_normal_descriptor.label =
            "Alchemy world SSR reverse-facing normal test";
        auto ssr_normal = ALMetalTexture::create(
            *device, ssr_normal_descriptor, &error);
        INFO(error);
        REQUIRE(ssr_normal);
        auto ssr_upload = device->makeCommandBuffer(
            "Alchemy world SSR scene-history upload", &error);
        INFO(error);
        REQUIRE(ssr_upload);
        // The bridge uploads glReadPixels verbatim: row zero is GL's lower
        // row. Keep the upper row black so a second Y flip in the Metal
        // sampler makes the lower-half assertion below fail visibly.
        const std::array<std::uint8_t, 8> red_lower_ssr{{
            255, 0, 0, 255,
              0, 0, 0, 255,
        }};
        const std::array<std::uint8_t, 4> reverse_normal{{128, 128, 0, 255}};
        const std::array<float, 2> ssr_depth_values{{0.5f, 0.5f}};
        REQUIRE(ssr_color->upload(
            *ssr_upload, 0, 0, {}, red_lower_ssr.data(), 4, &error));
        REQUIRE(ssr_depth->upload(
            *ssr_upload, 0, 0, {}, ssr_depth_values.data(),
            sizeof(float), &error));
        REQUIRE(ssr_normal->upload(
            *ssr_upload, 0, 0, {}, reverse_normal.data(), 4, &error));
        REQUIRE(ssr_upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldScreenSpaceReflections screen_space_reflections;
        screen_space_reflections.scene_color = ssr_color.get();
        screen_space_reflections.scene_depth = ssr_depth.get();
        for (std::size_t index = 0; index < 4; ++index)
        {
            screen_space_reflections.constants.projection[index * 4 + index] = 1.f;
            screen_space_reflections.constants.inverse_projection[index * 4 + index] = 1.f;
            screen_space_reflections.constants.inverse_modelview_delta[index * 4 + index] = 1.f;
        }
        screen_space_reflections.constants.inverse_projection[14] = -1.5f;
        screen_space_reflections.constants.screen[0] = 32.f;
        screen_space_reflections.constants.screen[1] = 32.f;
        screen_space_reflections.constants.screen[2] = 32.f;
        screen_space_reflections.constants.trace[0] = 4.f;
        screen_space_reflections.constants.trace[1] = 0.1f;
        screen_space_reflections.constants.trace[2] = 0.2f;
        screen_space_reflections.constants.trace[3] = 0.001f;
        screen_space_reflections.constants.sampling[0] = 1.f;
        screen_space_reflections.constants.sampling[1] = 1.6f;
        screen_space_reflections.constants.config[1] = 1;

        // Legacy BLEND materials keep their authored diffuse alpha unless a
        // sun, reflection-probe or punctual highlight raises it. Build a
        // clean retained target so every assertion below observes the Metal
        // fragment directly rather than a preceding alpha layer.
        ALMetalWorldSimpleBatch legacy_seed_batch = flat_alpha_batch;
        legacy_seed_batch.skin_palette = nullptr;
        legacy_seed_batch.skin_palette_float_count = 0;
        legacy_seed_batch.skin_joint_count = 0;
        legacy_seed_batch.weight_offset = 0;
        std::fill(std::begin(legacy_seed_batch.modelview_projection),
                  std::end(legacy_seed_batch.modelview_projection), 0.f);
        for (std::size_t index = 0; index < 4; ++index)
        {
            legacy_seed_batch.modelview_projection[index * 4 + index] = 1.f;
        }
        legacy_seed_batch.modelview_projection[12] = 3.f;
        ALMetalWorldGBufferScene legacy_seed_scene = scene;
        legacy_seed_scene.batches = &legacy_seed_batch;
        legacy_seed_scene.batch_count = 1;
        legacy_seed_scene.reverse_z = false;
        legacy_seed_scene.stamp_opaque_coverage = false;
        legacy_seed_scene.reflection_probes = nullptr;
        legacy_seed_scene.legacy_alpha_lighting = nullptr;
        std::fill(std::begin(legacy_seed_scene.clear_color),
                  std::end(legacy_seed_scene.clear_color), 0.f);
        auto make_legacy_target = [&]()
        {
            error.clear();
            return renderer->render(legacy_seed_scene, &error);
        };
        auto read_legacy_alpha = [&](ALMetalRenderTarget& target)
        {
            std::array<std::uint8_t, 32 * 32 * 4> pixels{};
            REQUIRE(target.colorTexture(0)->readback(
                *device, 0, 0, {}, pixels.data(), 32 * 4, &error));
            INFO(error);
            return pixels[center + 3];
        };
        auto read_legacy_alpha_at = [&](ALMetalRenderTarget& target,
                                        std::size_t x, std::size_t y)
        {
            std::array<std::uint8_t, 32 * 32 * 4> pixels{};
            REQUIRE(target.colorTexture(0)->readback(
                *device, 0, 0, {}, pixels.data(), 32 * 4, &error));
            INFO(error);
            return pixels[(y * 32 + x) * 4 + 3];
        };
        auto read_legacy_depth = [&](ALMetalRenderTarget& target)
        {
            std::array<float, 32 * 32> pixels{};
            REQUIRE(target.depthStencilTexture()->readback(
                *device, 0, 0, {}, pixels.data(), 32 * sizeof(float),
                &error));
            INFO(error);
            return pixels[center / 4];
        };

        ALMetalWorldSimpleBatch legacy_alpha_batch = flat_alpha_batch;
        legacy_alpha_batch.diffuse_textures[0] = tree_diffuse.get();
        legacy_alpha_batch.texture_count = 1;
        legacy_alpha_batch.material_slot_count = 1;
        legacy_alpha_batch.material_alpha_mode = 1;
        legacy_alpha_batch.tangent_offset = tangent_offset;
        legacy_alpha_batch.texcoord1_offset = texcoord_offset;
        legacy_alpha_batch.texcoord2_offset = texcoord_offset;
        legacy_alpha_batch.pbr_slot_count = 0;
        legacy_alpha_batch.skin_palette = nullptr;
        legacy_alpha_batch.skin_palette_float_count = 0;
        legacy_alpha_batch.skin_joint_count = 0;
        legacy_alpha_batch.weight_offset = 0;
        std::fill(std::begin(legacy_alpha_batch.modelview_projection),
                  std::end(legacy_alpha_batch.modelview_projection), 0.f);
        std::fill(std::begin(legacy_alpha_batch.modelview),
                  std::end(legacy_alpha_batch.modelview), 0.f);
        for (std::size_t index = 0; index < 4; ++index)
        {
            legacy_alpha_batch.modelview_projection[index * 4 + index] = 1.f;
            legacy_alpha_batch.modelview[index * 4 + index] = 1.f;
        }
        legacy_alpha_batch.modelview[14] = -2.f;
        legacy_alpha_batch.material_slots[0].specular_color[0] = 1.f;
        legacy_alpha_batch.material_slots[0].specular_color[1] = 1.f;
        legacy_alpha_batch.material_slots[0].specular_color[2] = 1.f;
        legacy_alpha_batch.material_slots[0].specular_color[3] = 1.f;
        legacy_alpha_batch.material_slots[0].normal_texture = nullptr;
        legacy_alpha_batch.material_slots[0].specular_texture = nullptr;
        legacy_alpha_batch.material_slots[0].env_intensity = 1.f;
        legacy_alpha_batch.material_slots[0].fullbright = 0.f;

        ALMetalWorldLegacyAlphaLighting legacy_lighting;
        legacy_lighting.sun_direction[2] = 1.f;
        legacy_lighting.sun_color[0] = 1.f;
        legacy_lighting.parameters[0] = 128.f;
        legacy_lighting.parameters[1] = 0.004f;
        legacy_lighting.parameters[2] = 1.f;

        ALMetalWorldGBufferScene legacy_alpha_scene = legacy_seed_scene;
        legacy_alpha_scene.batches = &legacy_alpha_batch;
        legacy_alpha_scene.reflection_probes = &reflection_probes;
        legacy_alpha_scene.legacy_alpha_lighting = nullptr;
        auto legacy_target = make_legacy_target();
        INFO(error);
        REQUIRE(legacy_target);
        CHECK_FALSE(renderer->compositeImpostorPostDeferred(
            *legacy_target, legacy_alpha_scene, &error));
        CHECK(error == "Metal world legacy alpha lighting is invalid");
        error.clear();
        legacy_alpha_scene.legacy_alpha_lighting = &legacy_lighting;
        legacy_alpha_scene.reflection_probes = nullptr;
        CHECK_FALSE(renderer->compositeImpostorPostDeferred(
            *legacy_target, legacy_alpha_scene, &error));
        CHECK(error ==
              "Metal world legacy alpha reflection probes are invalid");
        error.clear();
        legacy_alpha_scene.reflection_probes = &reflection_probes;

        std::array<std::uint8_t, 32 * 32 * 4> legacy_orm_before{};
        std::array<std::uint16_t, 32 * 32 * 4> legacy_normal_before{};
        std::array<std::uint32_t, 32 * 32> legacy_emissive_before{};
        REQUIRE(legacy_target->colorTexture(1)->readback(
            *device, 0, 0, {}, legacy_orm_before.data(), 32 * 4,
            &error));
        REQUIRE(legacy_target->colorTexture(2)->readback(
            *device, 0, 0, {}, legacy_normal_before.data(), 32 * 8,
            &error));
        REQUIRE(legacy_target->colorTexture(3)->readback(
            *device, 0, 0, {}, legacy_emissive_before.data(), 32 * 4,
            &error));
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *legacy_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(read_legacy_alpha(*legacy_target)) -
                       128) <= 1);
        CHECK(std::abs(read_legacy_depth(*legacy_target) - 1.f) < 0.0001f);
        std::array<std::uint8_t, 32 * 32 * 4> legacy_orm_after{};
        std::array<std::uint16_t, 32 * 32 * 4> legacy_normal_after{};
        std::array<std::uint32_t, 32 * 32> legacy_emissive_after{};
        REQUIRE(legacy_target->colorTexture(1)->readback(
            *device, 0, 0, {}, legacy_orm_after.data(), 32 * 4,
            &error));
        REQUIRE(legacy_target->colorTexture(2)->readback(
            *device, 0, 0, {}, legacy_normal_after.data(), 32 * 8,
            &error));
        REQUIRE(legacy_target->colorTexture(3)->readback(
            *device, 0, 0, {}, legacy_emissive_after.data(), 32 * 4,
            &error));
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(legacy_orm_after[center + channel] ==
                  legacy_orm_before[center + channel]);
            CHECK(legacy_normal_after[center + channel] ==
                  legacy_normal_before[center + channel]);
        }
        CHECK(legacy_emissive_after[center / 4] ==
              legacy_emissive_before[center / 4]);

        // A full-weight box hero replaces the ordinary bright probe with a
        // black sharp cube, returning coverage to authored diffuse alpha.
        // The sphere branch reaches the same result at its centre, while a
        // rejected mirror clip plane leaves the ordinary probe untouched.
        legacy_lighting.sun_color[0] = 0.f;
        ALMetalWorldHeroProbe invalid_hero;
        invalid_hero.constants.config[1] = 1;
        legacy_alpha_scene.hero_probe = &invalid_hero;
        auto invalid_hero_target = make_legacy_target();
        INFO(error);
        REQUIRE(invalid_hero_target);
        CHECK_FALSE(renderer->compositeImpostorPostDeferred(
            *invalid_hero_target, legacy_alpha_scene, &error));
        CHECK(error == "Metal world impostor hero probe is invalid");
        error.clear();
        legacy_alpha_scene.hero_probe = &hero_probe;
        auto hero_box_target = make_legacy_target();
        INFO(error);
        REQUIRE(hero_box_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *hero_box_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*hero_box_target)) - 64) <= 1);

        hero_probe.constants.config[0] = 1;
        hero_probe.constants.hero_sphere[2] = -2.f;
        hero_probe.constants.hero_sphere[3] = 100.f;
        auto hero_sphere_target = make_legacy_target();
        INFO(error);
        REQUIRE(hero_sphere_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *hero_sphere_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*hero_sphere_target)) - 64) <= 1);

        hero_probe.constants.clip_plane[3] = -1.f;
        auto hero_clipped_target = make_legacy_target();
        INFO(error);
        REQUIRE(hero_clipped_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *hero_clipped_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*hero_clipped_target)) - 128) <= 1);
        hero_probe.constants.config[0] = 0;
        hero_probe.constants.clip_plane[3] = 1.f;
        legacy_alpha_scene.hero_probe = nullptr;
        legacy_lighting.sun_color[0] = 1.f;

        // The transparent legacy finish blends a successful screen-space
        // hit into both radiance taps before the hero override. Disable the
        // ordinary cube contribution, flip the test normal toward the view,
        // and require the red history hit to raise quarter coverage to one
        // half. A partial pack must fail closed before encoding.
        ALMetalWorldScreenSpaceReflections invalid_ssr;
        invalid_ssr.constants.config[1] = 1;
        legacy_alpha_scene.screen_space_reflections = &invalid_ssr;
        auto invalid_ssr_target = make_legacy_target();
        INFO(error);
        REQUIRE(invalid_ssr_target);
        CHECK_FALSE(renderer->compositeImpostorPostDeferred(
            *invalid_ssr_target, legacy_alpha_scene, &error));
        CHECK(error ==
              "Metal world impostor screen-space reflections are invalid");
        error.clear();

        const float saved_default_radiance_scale =
            reflection_probes.constants.ref_params[0][1];
        const float saved_radiance_scale =
            reflection_probes.constants.ref_params[1][1];
        reflection_probes.constants.ref_params[0][1] = 0.f;
        reflection_probes.constants.ref_params[1][1] = 0.f;
        legacy_alpha_batch.material_slots[0].normal_texture =
            ssr_normal.get();
        legacy_alpha_scene.screen_space_reflections = nullptr;
        auto no_ssr_target = make_legacy_target();
        INFO(error);
        REQUIRE(no_ssr_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *no_ssr_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*no_ssr_target)) - 64) <= 1);

        legacy_alpha_scene.screen_space_reflections =
            &screen_space_reflections;
        auto ssr_target = make_legacy_target();
        INFO(error);
        REQUIRE(ssr_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *ssr_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha_at(*ssr_target, 16, 8)) -
                       128) <= 2);

        // Transparent legacy SSR has no opaque roughness cutoff: GLSL
        // traces one mirror ray at full gloss and applies the authored
        // glossiness only to the final hit weight. A quarter-gloss hit is
        // therefore still strong enough to saturate this environment glare.
        legacy_alpha_batch.material_slots[0].specular_color[3] = 0.25f;
        auto low_gloss_ssr_target = make_legacy_target();
        INFO(error);
        REQUIRE(low_gloss_ssr_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *low_gloss_ssr_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(read_legacy_alpha_at(
                           *low_gloss_ssr_target, 16, 8)) -
                       128) <= 2);
        legacy_alpha_batch.material_slots[0].specular_color[3] = 1.f;
        legacy_alpha_scene.screen_space_reflections = nullptr;
        legacy_alpha_batch.material_slots[0].normal_texture = nullptr;
        reflection_probes.constants.ref_params[0][1] =
            saved_default_radiance_scale;
        reflection_probes.constants.ref_params[1][1] =
            saved_radiance_scale;

        // With the probe term disabled, an aligned red sun saturates glare
        // and raises half-diffuse/half-vertex coverage from one quarter to
        // one half. Tangent-space +X and an orthogonal green specular map
        // each remove that same red highlight, proving both authored maps
        // participate in the coverage calculation.
        legacy_alpha_batch.material_slots[0].env_intensity = 0.f;
        auto sun_target = make_legacy_target();
        INFO(error);
        REQUIRE(sun_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *sun_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(read_legacy_alpha(*sun_target)) -
                       128) <= 1);

        // A live raw-depth cascade applies the same forward-Z receiver
        // comparison as shadowUtil.glsl before the sun glint raises
        // coverage. The constant matrix puts every receiver at (0.5, 0.5,
        // 0.5); a stored 0.25 therefore shadows it and restores the authored
        // quarter coverage. Marking that cascade unbuilt returns fully lit.
        ALMetalTextureDescriptor shadow_descriptor;
        shadow_descriptor.type = ALMetalTextureType::texture_2d_array;
        shadow_descriptor.format = ALMetalPixelFormat::r32_float;
        shadow_descriptor.width = 8;
        shadow_descriptor.height = 8;
        shadow_descriptor.array_length = 4;
        shadow_descriptor.storage = ALMetalStorageMode::private_gpu;
        shadow_descriptor.usage = ALMetalTextureUsage::shader_read;
        shadow_descriptor.label =
            "Alchemy world legacy-alpha directional-shadow test";
        auto shadow_texture = ALMetalTexture::create(
            *device, shadow_descriptor, &error);
        INFO(error);
        REQUIRE(shadow_texture);
        auto shadow_upload = device->makeCommandBuffer(
            "Alchemy world legacy-alpha directional-shadow upload",
            &error);
        INFO(error);
        REQUIRE(shadow_upload);
        const std::array<float, 8 * 8> shadowed_depth = []
        {
            std::array<float, 8 * 8> value{};
            value.fill(0.25f);
            return value;
        }();
        const std::array<float, 8 * 8> lit_depth = []
        {
            std::array<float, 8 * 8> value{};
            value.fill(1.f);
            return value;
        }();
        for (std::uint32_t cascade = 0; cascade < 4; ++cascade)
        {
            const auto& depth = cascade == 0
                ? shadowed_depth : lit_depth;
            REQUIRE(shadow_texture->upload(
                *shadow_upload, 0, cascade, {}, depth.data(),
                8 * sizeof(float), &error));
        }
        REQUIRE(shadow_upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldDirectionalShadows directional_shadows;
        directional_shadows.depth_texture = shadow_texture.get();
        for (std::size_t cascade = 0; cascade < 4; ++cascade)
        {
            float* matrix = directional_shadows.constants
                                .shadow_matrix[cascade];
            matrix[12] = 0.5f;
            matrix[13] = 0.5f;
            matrix[14] = 0.5f;
            matrix[15] = 1.f;
        }
        directional_shadows.constants.shadow_clip[0] = 4.f;
        directional_shadows.constants.shadow_clip[1] = 8.f;
        directional_shadows.constants.shadow_clip[2] = 12.f;
        directional_shadows.constants.shadow_clip[3] = 16.f;
        directional_shadows.constants.resolution[0] = 8.f;
        directional_shadows.constants.resolution[1] = 8.f;
        directional_shadows.constants.resolution[2] = 32.f;
        directional_shadows.constants.resolution[3] = 32.f;
        directional_shadows.constants.parameters[2] = 200.f;
        directional_shadows.constants.parameters[3] = 32.f;
        directional_shadows.constants.config[0] = 4;
        directional_shadows.constants.config[1] = 0xeu;
        directional_shadows.constants.config[3] = 1;

        ALMetalWorldSimpleBatch shadowed_simple_pre_water =
            simple_pre_water;
        shadowed_simple_pre_water.pre_water_shadows = true;
        opaque_forward_scene.batches = &shadowed_simple_pre_water;
        opaque_forward_scene.batch_count = 1;
        opaque_forward_scene.gbuffer = coverage_result.get();
        opaque_forward_scene.reverse_z = false;
        opaque_forward_scene.reflection_probes = &reflection_probes;
        opaque_forward_scene.alpha_environment = &alpha_environment;
        opaque_forward_scene.alpha_lighting = &simple_alpha_lighting;
        opaque_forward_scene.alpha_directional_shadows = nullptr;
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error ==
              "Metal world pre-water alpha shadows are invalid");
        opaque_forward_scene.alpha_directional_shadows =
            &directional_shadows;
        error.clear();
        auto shadowed_simple_result = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(shadowed_simple_result);
        REQUIRE(shadowed_simple_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(std::abs(half_to_float(
                  pre_water_pixels[pre_water_left + 1]) - 0.0625f) <
              0.002f);

        directional_shadows.constants.config[1] = 0xfu;
        auto unbuilt_simple_result = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(unbuilt_simple_result);
        REQUIRE(unbuilt_simple_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(std::abs(half_to_float(
                  pre_water_pixels[pre_water_left + 1]) - 0.3125f) <
              0.002f);
        opaque_forward_scene.alpha_directional_shadows = nullptr;
        directional_shadows.constants.config[1] = 0xeu;
        legacy_alpha_scene.directional_shadows = &directional_shadows;

        auto shadow_target = make_legacy_target();
        INFO(error);
        REQUIRE(shadow_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *shadow_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*shadow_target)) - 64) <= 1);

        directional_shadows.constants.config[3] = 3;
        auto pcss_shadow_target = make_legacy_target();
        INFO(error);
        REQUIRE(pcss_shadow_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *pcss_shadow_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*pcss_shadow_target)) - 64) <= 1);

        directional_shadows.constants.config[1] = 0xfu;
        auto unbuilt_shadow_target = make_legacy_target();
        INFO(error);
        REQUIRE(unbuilt_shadow_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *unbuilt_shadow_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*unbuilt_shadow_target)) -
                       128) <= 1);

        directional_shadows.constants.config[1] = 0xeu;
        directional_shadows.constants.config[2] = 1;
        directional_shadows.constants.config[3] = 1;
        auto reverse_shadow_target = make_legacy_target();
        INFO(error);
        REQUIRE(reverse_shadow_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *reverse_shadow_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*reverse_shadow_target)) -
                       128) <= 1);
        legacy_alpha_scene.directional_shadows = nullptr;

        legacy_alpha_batch.material_slots[0].normal_texture =
            bump_normal.get();
        auto normal_map_target = make_legacy_target();
        INFO(error);
        REQUIRE(normal_map_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *normal_map_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*normal_map_target)) - 64) <= 1);

        legacy_alpha_batch.material_slots[0].normal_texture = nullptr;
        legacy_alpha_batch.material_slots[0].specular_texture =
            indexed_diffuse.get();
        auto specular_map_target = make_legacy_target();
        INFO(error);
        REQUIRE(specular_map_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *specular_map_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*specular_map_target)) - 64) <= 1);

        legacy_alpha_batch.material_slots[0].specular_texture = nullptr;
        legacy_alpha_batch.material_slots[0].fullbright = 1.f;
        auto emissive_target = make_legacy_target();
        INFO(error);
        REQUIRE(emissive_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *emissive_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*emissive_target)) - 64) <= 1);

        // Punctual glare follows the same legacy attenuation and spot/omni
        // contract as deferredUtil.glsl. Disable the sun, place an omni light
        // one metre above the eye-space surface and require raised coverage.
        legacy_alpha_batch.material_slots[0].fullbright = 0.f;
        legacy_lighting.sun_color[0] = 0.f;
        legacy_lighting.local_lights[0].position[2] = -0.75f;
        legacy_lighting.local_lights[0].diffuse[0] = 1.f;
        legacy_lighting.local_lights[0].parameters[0] = 4.f;
        legacy_lighting.local_lights[0].parameters[1] = 0.f;
        legacy_lighting.local_lights[0].parameters[2] = 1.f;
        auto local_light_target = make_legacy_target();
        INFO(error);
        REQUIRE(local_light_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *local_light_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*local_light_target)) - 128) <= 1);

        // Object-skinned legacy alpha uses the same probe/material inputs but
        // writes depth, while the static variant above kept depth read-only.
        legacy_lighting.local_lights[0] = {};
        legacy_alpha_batch.material_slots[0].env_intensity = 1.f;
        legacy_alpha_batch.skin_palette = alpha_skin_palette.data();
        legacy_alpha_batch.skin_palette_float_count =
            alpha_skin_palette.size();
        legacy_alpha_batch.skin_joint_count = 1;
        legacy_alpha_batch.weight_offset = weight_offset;
        auto rigged_legacy_target = make_legacy_target();
        INFO(error);
        REQUIRE(rigged_legacy_target);
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *rigged_legacy_target, legacy_alpha_scene, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           read_legacy_alpha(*rigged_legacy_target)) -
                       128) <= 1);
        CHECK(std::abs(read_legacy_depth(*rigged_legacy_target) - 0.625f) <
              0.0001f);
        legacy_alpha_batch.skin_palette = nullptr;
        legacy_alpha_batch.skin_palette_float_count = 0;
        legacy_alpha_batch.skin_joint_count = 0;
        legacy_alpha_batch.weight_offset = 0;

        // The main-world legacy-material owner keeps materialF's lit RGB,
        // unlike the impostor fragment above, while retaining the same
        // glare-dependent coverage. Start with its fullbright mix so the
        // expected source is exactly decoded diffuse times decoded vertex
        // tint, then execute the same draw through static and rigged
        // pipelines over the retained final-color/depth target.
        ALMetalWorldSimpleBatch pre_water_material = legacy_alpha_batch;
        pre_water_material.pre_water_alpha = true;
        pre_water_material.pre_water_material = true;
        pre_water_material.pre_water_shadows = false;
        pre_water_material.material_slots[0].specular_color[3] = 0.f;
        pre_water_material.material_slots[0].env_intensity = 0.f;
        pre_water_material.material_slots[0].fullbright = 1.f;
        ALMetalWorldSimpleBatch pre_water_material_rigged =
            pre_water_material;
        pre_water_material_rigged.skin_palette = alpha_skin_palette.data();
        pre_water_material_rigged.skin_palette_float_count =
            alpha_skin_palette.size();
        pre_water_material_rigged.skin_joint_count = 1;
        pre_water_material_rigged.weight_offset = weight_offset;

        opaque_forward_scene.batches = &pre_water_material;
        opaque_forward_scene.batch_count = 1;
        opaque_forward_scene.gbuffer = coverage_result.get();
        opaque_forward_scene.reverse_z = false;
        opaque_forward_scene.reflection_probes = &reflection_probes;
        opaque_forward_scene.hero_probe = nullptr;
        opaque_forward_scene.screen_space_reflections = nullptr;
        opaque_forward_scene.alpha_environment = &alpha_environment;
        opaque_forward_scene.alpha_lighting = nullptr;
        opaque_forward_scene.alpha_directional_shadows = nullptr;
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error == "Metal world pre-water alpha lighting is invalid");
        opaque_forward_scene.alpha_lighting = &simple_alpha_lighting;
        error.clear();
        auto material_fullbright_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(material_fullbright_result);
        REQUIRE(material_fullbright_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        REQUIRE(material_fullbright_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, pre_water_depth.data(),
            32 * sizeof(float), &error));
        INFO(error);
        const float material_coverage = coverage * coverage;
        const float material_uncovered = 1.f - material_coverage;
        const std::array<float, 3> material_source{{
            srgb_to_linear(128.f / 255.f) *
                srgb_to_linear(128.f / 255.f),
            srgb_to_linear(64.f / 255.f),
            srgb_to_linear(32.f / 255.f),
        }};
        const std::array<float, 3> retained_color{{
            0.25f, 0.125f, 0.0625f,
        }};
        const std::size_t material_pixel = center;
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            CHECK(std::abs(
                half_to_float(pre_water_pixels[
                    material_pixel + channel]) -
                (material_source[channel] * material_coverage +
                 retained_color[channel] * material_uncovered)) <
                0.002f);
        }
        CHECK(std::abs(
            half_to_float(pre_water_pixels[material_pixel + 3]) -
            0.5f * material_uncovered) < 0.002f);
        CHECK(std::abs(pre_water_depth[material_pixel / 4] - 0.625f) <
              0.0001f);

        opaque_forward_scene.batches = &pre_water_material_rigged;
        auto material_rigged_result = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(material_rigged_result);
        REQUIRE(material_rigged_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            CHECK(std::abs(
                half_to_float(pre_water_pixels[
                    material_pixel + channel]) -
                (material_source[channel] * material_coverage +
                 retained_color[channel] * material_uncovered)) <
                0.002f);
        }

        // Restore lit material behavior. A shadowed cascade removes the
        // modern directional term without changing authored coverage; the
        // all-unbuilt mask restores the fully lit result.
        pre_water_material.material_slots[0].fullbright = 0.f;
        pre_water_material.pre_water_shadows = true;
        opaque_forward_scene.batches = &pre_water_material;
        directional_shadows.constants.config[1] = 0xeu;
        directional_shadows.constants.config[2] = 0;
        directional_shadows.constants.config[3] = 1;
        opaque_forward_scene.alpha_directional_shadows =
            &directional_shadows;
        auto shadowed_material_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(shadowed_material_result);
        REQUIRE(shadowed_material_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            CHECK(std::abs(
                half_to_float(pre_water_pixels[
                    material_pixel + channel]) -
                retained_color[channel] * material_uncovered) < 0.002f);
        }
        directional_shadows.constants.config[1] = 0xfu;
        auto unbuilt_material_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(unbuilt_material_result);
        REQUIRE(unbuilt_material_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(half_to_float(
                  pre_water_pixels[material_pixel + 1]) >
              retained_color[1] * material_uncovered);
        opaque_forward_scene.alpha_directional_shadows = nullptr;
        directional_shadows.constants.config[1] = 0xeu;

        // The material fragment's fixed resource contract consumes the
        // same coherent hero/SSR finish as materialF.glsl. Invalid partial
        // packs fail before encoding. With direct light disabled, the
        // ordinary bright probe raises blue radiance and glare; the valid
        // black hero override removes both.
        pre_water_material.pre_water_shadows = false;
        pre_water_material.material_slots[0].specular_color[3] = 1.f;
        pre_water_material.material_slots[0].env_intensity = 1.f;
        alpha_environment.atmos_sunlit_linear[0] = 0.f;
        alpha_environment.atmos_sunlit_linear[1] = 0.f;
        alpha_environment.atmos_sunlit_linear[2] = 0.f;
        opaque_forward_scene.hero_probe = &invalid_hero;
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error ==
              "Metal world opaque post-deferred hero probe is invalid");
        opaque_forward_scene.hero_probe = nullptr;
        opaque_forward_scene.screen_space_reflections = &invalid_ssr;
        error.clear();
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error ==
              "Metal world opaque post-deferred screen-space reflections are invalid");
        opaque_forward_scene.screen_space_reflections = nullptr;
        error.clear();
        auto ordinary_material_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(ordinary_material_result);
        REQUIRE(ordinary_material_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        const float ordinary_material_blue = half_to_float(
            pre_water_pixels[material_pixel + 2]);
        const float ordinary_material_alpha = half_to_float(
            pre_water_pixels[material_pixel + 3]);

        opaque_forward_scene.hero_probe = &hero_probe;
        auto hero_material_result = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(hero_material_result);
        REQUIRE(hero_material_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(ordinary_material_blue > half_to_float(
                  pre_water_pixels[material_pixel + 2]));
        CHECK(ordinary_material_alpha < half_to_float(
                  pre_water_pixels[material_pixel + 3]));
        opaque_forward_scene.hero_probe = nullptr;

        // A tangent-space reverse normal makes the transparent one-ray SSR
        // fixture hit its red lower-row history. Disable ordinary radiance
        // so the comparison isolates normal-map sampling, screen Y
        // orientation and the paired color/depth finish.
        const float material_default_radiance_scale =
            reflection_probes.constants.ref_params[0][1];
        const float material_radiance_scale =
            reflection_probes.constants.ref_params[1][1];
        reflection_probes.constants.ref_params[0][1] = 0.f;
        reflection_probes.constants.ref_params[1][1] = 0.f;
        pre_water_material.material_slots[0].normal_texture =
            ssr_normal.get();
        auto no_ssr_material_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(no_ssr_material_result);
        REQUIRE(no_ssr_material_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        const float no_ssr_material_red = half_to_float(
            pre_water_pixels[material_pixel + 0]);
        opaque_forward_scene.screen_space_reflections =
            &screen_space_reflections;
        auto ssr_material_result = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(ssr_material_result);
        REQUIRE(ssr_material_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(half_to_float(pre_water_pixels[material_pixel + 0]) >
              no_ssr_material_red);
        opaque_forward_scene.screen_space_reflections = nullptr;
        pre_water_material.material_slots[0].normal_texture = nullptr;
        reflection_probes.constants.ref_params[0][1] =
            material_default_radiance_scale;
        reflection_probes.constants.ref_params[1][1] =
            material_radiance_scale;

        // Local-light RGB uses the full legacy diffuse/specular model, not
        // the simple-alpha diffuse-only helper. With authored gloss and the
        // environment disabled it must add visible green over the retained
        // background while also preserving the coverage contract.
        pre_water_material.material_slots[0].env_intensity = 0.f;
        pre_water_material.material_slots[0].specular_color[3] = 0.f;
        simple_alpha_lighting.local_lights[0].position[2] = 10.f;
        simple_alpha_lighting.local_lights[0].diffuse[1] = 0.5f;
        simple_alpha_lighting.local_lights[0].parameters[0] = 100.f;
        simple_alpha_lighting.local_lights[0].parameters[2] = 1.f;
        auto local_material_result = renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(local_material_result);
        REQUIRE(local_material_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(half_to_float(pre_water_pixels[material_pixel + 1]) >
              retained_color[1] * material_uncovered);
        simple_alpha_lighting.local_lights[0] = {};
        alpha_environment.atmos_sunlit_linear[0] = 1.f;
        alpha_environment.atmos_sunlit_linear[1] = 0.5f;
        alpha_environment.atmos_sunlit_linear[2] = 0.25f;

        // The glTF BLEND owner stays in the main-world forward slice rather
        // than falling back to its flat impostor bake. It consumes the same
        // four-map material, probe/BRDF, directional-shadow and seven-light
        // contracts as pbralphaF.glsl. Exercise the source-over static and
        // rigged pipelines first with an emissive-only source so the result
        // remains independent of direct-light and radiance-probe terms.
        ALMetalWorldSimpleBatch pre_water_pbr = legacy_alpha_batch;
        pre_water_pbr.pre_water_alpha = true;
        pre_water_pbr.pre_water_material = false;
        pre_water_pbr.pre_water_pbr = true;
        pre_water_pbr.pre_water_shadows = false;
        pre_water_pbr.material_slot_count = 0;
        pre_water_pbr.material_alpha_mode = 0;
        pre_water_pbr.pbr_slot_count = 1;
        pre_water_pbr.texture_count = 1;
        pre_water_pbr.diffuse_textures[0] = tree_diffuse.get();
        pre_water_pbr.double_sided = true;
        ALMetalWorldPBRSlot& pre_water_pbr_slot =
            pre_water_pbr.pbr_slots[0];
        pre_water_pbr_slot.normal_texture = flat_normal.get();
        pre_water_pbr_slot.orm_texture = pbr_orm_texture.get();
        pre_water_pbr_slot.emissive_texture = diffuse.get();
        pre_water_pbr_slot.emissive_color[1] = 1.f;
        pre_water_pbr_slot.roughness_factor = 1.f;
        pre_water_pbr_slot.metallic_factor = 0.f;
        pre_water_pbr_slot.alpha_cutoff = -1.f;
        ALMetalWorldSimpleBatch pre_water_pbr_rigged = pre_water_pbr;
        pre_water_pbr_rigged.skin_palette = alpha_skin_palette.data();
        pre_water_pbr_rigged.skin_palette_float_count =
            alpha_skin_palette.size();
        pre_water_pbr_rigged.skin_joint_count = 1;
        pre_water_pbr_rigged.weight_offset = weight_offset;

        opaque_forward_scene.batches = &pre_water_pbr;
        opaque_forward_scene.batch_count = 1;
        opaque_forward_scene.gbuffer = coverage_result.get();
        opaque_forward_scene.reverse_z = false;
        opaque_forward_scene.reflection_probes = &reflection_probes;
        opaque_forward_scene.hero_probe = nullptr;
        opaque_forward_scene.screen_space_reflections = nullptr;
        opaque_forward_scene.alpha_environment = &alpha_environment;
        opaque_forward_scene.alpha_lighting = &simple_alpha_lighting;
        opaque_forward_scene.alpha_directional_shadows = nullptr;
        pre_water_pbr_slot.normal_texture = nullptr;
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error == "Metal world pre-water PBR slot is invalid");
        pre_water_pbr_slot.normal_texture = flat_normal.get();
        pre_water_pbr_slot.alpha_cutoff = 0.5f;
        error.clear();
        CHECK_FALSE(renderer->renderOpaquePostDeferred(
            opaque_forward_scene, &error));
        CHECK(error == "Metal world pre-water PBR slot is invalid");
        pre_water_pbr_slot.alpha_cutoff = -1.f;

        const float pbr_default_radiance_scale =
            reflection_probes.constants.ref_params[0][1];
        const float pbr_radiance_scale =
            reflection_probes.constants.ref_params[1][1];
        reflection_probes.constants.ref_params[0][1] = 0.f;
        reflection_probes.constants.ref_params[1][1] = 0.f;
        alpha_environment.atmos_sunlit_linear[0] = 0.f;
        alpha_environment.atmos_sunlit_linear[1] = 0.f;
        alpha_environment.atmos_sunlit_linear[2] = 0.f;
        error.clear();
        auto static_pbr_alpha_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(static_pbr_alpha_result);
        REQUIRE(static_pbr_alpha_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        REQUIRE(static_pbr_alpha_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, pre_water_depth.data(),
            32 * sizeof(float), &error));
        INFO(error);
        std::array<float, 4> static_pbr_pixel{};
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            static_pbr_pixel[channel] = half_to_float(
                pre_water_pixels[material_pixel + channel]);
            CHECK(std::isfinite(static_pbr_pixel[channel]));
        }
        CHECK(static_pbr_pixel[1] >
              retained_color[1] * material_uncovered);
        const float static_pbr_depth =
            pre_water_depth[material_pixel / 4];

        opaque_forward_scene.batches = &pre_water_pbr_rigged;
        auto rigged_pbr_alpha_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(rigged_pbr_alpha_result);
        REQUIRE(rigged_pbr_alpha_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        REQUIRE(rigged_pbr_alpha_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, pre_water_depth.data(),
            32 * sizeof(float), &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(std::abs(
                half_to_float(pre_water_pixels[
                    material_pixel + channel]) -
                static_pbr_pixel[channel]) < 0.002f);
        }
        CHECK(std::abs(pre_water_depth[material_pixel / 4] -
                       static_pbr_depth) < 0.0001f);

        // A built cascade removes the aligned modern sun term; marking all
        // four cascades unbuilt restores it. This proves the PBR owner uses
        // the demanded live shadow state instead of silently rendering an
        // unshadowed approximation.
        opaque_forward_scene.batches = &pre_water_pbr;
        pre_water_pbr_slot.emissive_color[1] = 0.f;
        pre_water_pbr.pre_water_shadows = true;
        alpha_environment.atmos_sunlit_linear[0] = 1.f;
        alpha_environment.atmos_sunlit_linear[1] = 0.5f;
        alpha_environment.atmos_sunlit_linear[2] = 0.25f;
        directional_shadows.constants.config[1] = 0xeu;
        opaque_forward_scene.alpha_directional_shadows =
            &directional_shadows;
        auto shadowed_pbr_alpha_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(shadowed_pbr_alpha_result);
        REQUIRE(shadowed_pbr_alpha_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        const float shadowed_pbr_green = half_to_float(
            pre_water_pixels[material_pixel + 1]);
        directional_shadows.constants.config[1] = 0xfu;
        auto unbuilt_pbr_alpha_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(unbuilt_pbr_alpha_result);
        REQUIRE(unbuilt_pbr_alpha_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(half_to_float(pre_water_pixels[material_pixel + 1]) >
              shadowed_pbr_green);

        // With directional and probe radiance disabled, an aligned green
        // punctual light must still raise the PBR diffuse lobe.
        pre_water_pbr.pre_water_shadows = false;
        opaque_forward_scene.alpha_directional_shadows = nullptr;
        alpha_environment.atmos_sunlit_linear[0] = 0.f;
        alpha_environment.atmos_sunlit_linear[1] = 0.f;
        alpha_environment.atmos_sunlit_linear[2] = 0.f;
        simple_alpha_lighting.local_lights[0].position[2] = 10.f;
        simple_alpha_lighting.local_lights[0].diffuse[1] = 2.f;
        simple_alpha_lighting.local_lights[0].parameters[0] = 100.f;
        simple_alpha_lighting.local_lights[0].parameters[2] = 1.f;
        auto local_pbr_alpha_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(local_pbr_alpha_result);
        REQUIRE(local_pbr_alpha_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(half_to_float(pre_water_pixels[material_pixel + 1]) >
              retained_color[1] * material_uncovered);
        simple_alpha_lighting.local_lights[0] = {};

        // Finish the glossy radiance lobe through the same ordinary, SSR,
        // then hero order as pbralphaF.glsl. A black hero removes the blue
        // ordinary cube; with ordinary radiance disabled, the paired red
        // scene-history hit raises only the SSR-enabled result.
        reflection_probes.constants.ref_params[0][1] =
            pbr_default_radiance_scale;
        reflection_probes.constants.ref_params[1][1] =
            pbr_radiance_scale;
        pre_water_pbr_slot.roughness_factor = 0.f;
        pre_water_pbr_slot.metallic_factor = 1.f;
        auto ordinary_pbr_alpha_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(ordinary_pbr_alpha_result);
        REQUIRE(ordinary_pbr_alpha_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        const float ordinary_pbr_blue = half_to_float(
            pre_water_pixels[material_pixel + 2]);
        opaque_forward_scene.hero_probe = &hero_probe;
        auto hero_pbr_alpha_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(hero_pbr_alpha_result);
        REQUIRE(hero_pbr_alpha_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(ordinary_pbr_blue > half_to_float(
                  pre_water_pixels[material_pixel + 2]));
        opaque_forward_scene.hero_probe = nullptr;

        reflection_probes.constants.ref_params[0][1] = 0.f;
        reflection_probes.constants.ref_params[1][1] = 0.f;
        pre_water_pbr_slot.normal_texture = ssr_normal.get();
        auto no_ssr_pbr_alpha_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(no_ssr_pbr_alpha_result);
        REQUIRE(no_ssr_pbr_alpha_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        const float no_ssr_pbr_red = half_to_float(
            pre_water_pixels[material_pixel + 0]);
        opaque_forward_scene.screen_space_reflections =
            &screen_space_reflections;
        auto ssr_pbr_alpha_result =
            renderer->renderOpaquePostDeferred(
                opaque_forward_scene, &error);
        INFO(error);
        REQUIRE(ssr_pbr_alpha_result);
        REQUIRE(ssr_pbr_alpha_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pre_water_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(half_to_float(pre_water_pixels[material_pixel + 0]) >
              no_ssr_pbr_red);
        opaque_forward_scene.screen_space_reflections = nullptr;
        pre_water_pbr_slot.normal_texture = flat_normal.get();
        pre_water_pbr_slot.roughness_factor = 1.f;
        pre_water_pbr_slot.metallic_factor = 0.f;
        alpha_environment.atmos_sunlit_linear[0] = 1.f;
        alpha_environment.atmos_sunlit_linear[1] = 0.5f;
        alpha_environment.atmos_sunlit_linear[2] = 0.25f;
        reflection_probes.constants.ref_params[0][1] =
            pbr_default_radiance_scale;
        reflection_probes.constants.ref_params[1][1] =
            pbr_radiance_scale;
        directional_shadows.constants.config[1] = 0xeu;

        ALMetalWorldSimpleBatch shiny_batch = fullbright_batch;
        shiny_batch.impostor_fullbright = false;
        shiny_batch.impostor_shiny = true;
        shiny_batch.alpha_mask = false;
        for (std::size_t index = 0; index < 4; ++index)
        {
            shiny_batch.modelview[index * 4 + index] = 1.f;
        }
        alpha_scene.batches = &shiny_batch;
        alpha_scene.stamp_opaque_coverage = false;
        alpha_scene.reflection_probes = nullptr;
        CHECK_FALSE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        CHECK(error ==
              "Metal world shiny impostor reflection probes are invalid");
        error.clear();
        alpha_scene.reflection_probes = &reflection_probes;
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        INFO(error);
        REQUIRE(coverage_result->colorTexture(0)->readback(
            *device, 0, 0, {}, alpha_albedo.data(), 32 * 4, &error));
        REQUIRE(coverage_result->colorTexture(1)->readback(
            *device, 0, 0, {}, alpha_orm.data(), 32 * 4, &error));
        REQUIRE(coverage_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, alpha_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        CHECK(alpha_albedo[fullbright_top + 0] == 0);
        CHECK(std::abs(static_cast<int>(
                           alpha_albedo[fullbright_top + 1]) - 188) <= 2);
        CHECK(std::abs(static_cast<int>(
                           alpha_albedo[fullbright_top + 2]) - 99) <= 2);
        CHECK(alpha_albedo[fullbright_top + 3] == 255);
        CHECK(alpha_orm[fullbright_top + 0] == 0);
        CHECK(alpha_orm[fullbright_top + 1] == 0);
        CHECK(alpha_orm[fullbright_top + 2] == 0);
        CHECK(alpha_orm[fullbright_top + 3] == 0);
        CHECK(std::abs(alpha_depth[24 * 32 + 8] - 0.625f) < 0.0001f);

        // fullbrightShinyF also finishes its legacy radiance tap with the
        // hero probe at glossiness one. Replace blue cube radiance with red
        // and require the reflected channel to move without changing the
        // diffuse green contribution or opaque coverage.
        auto red_hero_upload = device->makeCommandBuffer(
            "Alchemy world hero-probe red upload", &error);
        INFO(error);
        REQUIRE(red_hero_upload);
        const std::array<std::uint8_t, 4> red_hero{{255, 0, 0, 255}};
        for (std::uint32_t face = 0; face < 6; ++face)
        {
            REQUIRE(hero_texture->upload(
                *red_hero_upload, 0, face, {}, red_hero.data(), 4,
                &error));
        }
        REQUIRE(red_hero_upload->commitAndWait(&error));
        INFO(error);
        alpha_scene.hero_probe = &hero_probe;
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        INFO(error);
        REQUIRE(coverage_result->colorTexture(0)->readback(
            *device, 0, 0, {}, alpha_albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           alpha_albedo[fullbright_top + 0]) - 99) <= 2);
        CHECK(std::abs(static_cast<int>(
                           alpha_albedo[fullbright_top + 1]) - 188) <= 2);
        CHECK(alpha_albedo[fullbright_top + 2] == 0);
        CHECK(alpha_albedo[fullbright_top + 3] == 255);
        alpha_scene.hero_probe = nullptr;

        // The rigged owner uses the same probe traversal after applying its
        // rebased palette to eye position and normal, and remains an opaque
        // depth-writing pass.
        shiny_batch.modelview_projection[12] = 0.8f;
        shiny_batch.weight_offset = weight_offset;
        shiny_batch.skin_palette = alpha_skin_palette.data();
        shiny_batch.skin_palette_float_count = alpha_skin_palette.size();
        shiny_batch.skin_joint_count = 1;
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        INFO(error);
        REQUIRE(coverage_result->colorTexture(0)->readback(
            *device, 0, 0, {}, alpha_albedo.data(), 32 * 4, &error));
        REQUIRE(coverage_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, alpha_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        const std::size_t shiny_top_right = (24 * 32 + 24) * 4;
        CHECK(alpha_albedo[shiny_top_right + 0] == 0);
        CHECK(std::abs(static_cast<int>(
                           alpha_albedo[shiny_top_right + 1]) - 188) <= 2);
        CHECK(std::abs(static_cast<int>(
                           alpha_albedo[shiny_top_right + 2]) - 99) <= 2);
        CHECK(alpha_albedo[shiny_top_right + 3] == 255);
        CHECK(std::abs(alpha_depth[24 * 32 + 24] - 0.625f) < 0.0001f);

        // Restore the original fullbright source colour before the chained
        // emboss assertions below; shiny was deliberately tested at the same
        // static pixel so its probe mix could be compared directly.
        alpha_scene.batches = &fullbright_batch;
        alpha_scene.reflection_probes = nullptr;
        alpha_scene.stamp_opaque_coverage = true;
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        INFO(error);

        // The post-bump owner samples one bump texture at its two precomputed
        // UV streams, then applies GL's BT_MULT_X2 blend to RGB only. A
        // half-strength emboss result dims linear green to its sRGB 188 byte
        // while preserving full coverage, every other attachment and depth.
        ALMetalWorldSimpleBatch emboss_batch = fullbright_batch;
        emboss_batch.impostor_fullbright = false;
        emboss_batch.impostor_emboss = true;
        emboss_batch.alpha_mask = false;
        emboss_batch.texcoord1_offset = emboss_texcoord_offset;
        emboss_batch.diffuse_textures[0] = emboss_bump.get();
        emboss_batch.texture_count = 1;
        emboss_batch.texture_matrix[12] = 0.25f;
        alpha_scene.batches = &emboss_batch;
        alpha_scene.stamp_opaque_coverage = false;
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        INFO(error);
        REQUIRE(coverage_result->colorTexture(0)->readback(
            *device, 0, 0, {}, alpha_albedo.data(), 32 * 4, &error));
        REQUIRE(coverage_result->colorTexture(1)->readback(
            *device, 0, 0, {}, alpha_orm.data(), 32 * 4, &error));
        REQUIRE(coverage_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, alpha_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           alpha_albedo[fullbright_top + 1]) - 188) <= 2);
        CHECK(alpha_albedo[fullbright_top + 3] == 255);
        CHECK(alpha_orm[fullbright_top + 0] == 0);
        CHECK(alpha_orm[fullbright_top + 1] == 0);
        CHECK(alpha_orm[fullbright_top + 2] == 0);
        CHECK(alpha_orm[fullbright_top + 3] == 0);
        CHECK(std::abs(alpha_depth[24 * 32 + 8] - 0.625f) < 0.0001f);

        emboss_batch.weight_offset = weight_offset;
        emboss_batch.skin_palette = alpha_skin_palette.data();
        emboss_batch.skin_palette_float_count =
            alpha_skin_palette.size();
        emboss_batch.skin_joint_count = 1;
        REQUIRE(renderer->compositeImpostorPostDeferred(
            *coverage_result, alpha_scene, &error));
        INFO(error);
        REQUIRE(coverage_result->colorTexture(0)->readback(
            *device, 0, 0, {}, alpha_albedo.data(), 32 * 4, &error));
        REQUIRE(coverage_result->depthStencilTexture()->readback(
            *device, 0, 0, {}, alpha_depth.data(), 32 * sizeof(float),
            &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                           alpha_albedo[fullbright_top + 1]) - 137) <= 2);
        CHECK(alpha_albedo[fullbright_top + 3] == 255);
        CHECK(std::abs(alpha_depth[24 * 32 + 8] - 0.625f) < 0.0001f);

        scene.hdr = false;

        batch.tangent_offset =
            (vertex_bytes.size() + 15) & ~std::size_t{15};
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error ==
              "Metal world simple geometry range exceeds its buffers");
        batch.tangent_offset = tangent_offset;
        error.clear();
        auto pbr_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(pbr_result);
        std::array<std::uint8_t, 32 * 32 * 4> pbr_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4> pbr_orm{};
        std::array<std::uint16_t, 32 * 32 * 4> pbr_normal{};
        std::array<std::uint8_t, 32 * 32 * 4> pbr_emissive{};
        REQUIRE(pbr_result->colorTexture(0)->readback(
            *device, 0, 0, {}, pbr_albedo.data(), 32 * 4, &error));
        REQUIRE(pbr_result->colorTexture(1)->readback(
            *device, 0, 0, {}, pbr_orm.data(), 32 * 4, &error));
        REQUIRE(pbr_result->colorTexture(2)->readback(
            *device, 0, 0, {}, pbr_normal.data(), 32 * 8, &error));
        REQUIRE(pbr_result->colorTexture(3)->readback(
            *device, 0, 0, {}, pbr_emissive.data(), 32 * 4, &error));
        INFO(error);
        CHECK(pbr_albedo[center + 0] == 0);
        CHECK(pbr_albedo[center + 1] == 255);
        CHECK(pbr_albedo[center + 2] == 0);
        CHECK(pbr_albedo[center + 3] == 0);
        CHECK(std::abs(static_cast<int>(pbr_orm[center + 0]) - 128) <= 1);
        CHECK(std::abs(static_cast<int>(pbr_orm[center + 1]) - 64) <= 1);
        CHECK(std::abs(static_cast<int>(pbr_orm[center + 2]) - 32) <= 1);
        CHECK(pbr_orm[center + 3] <= 1);
        CHECK(pbr_normal[center + 0] > 65000);
        CHECK(std::abs(static_cast<int>(pbr_normal[center + 2]) -
                       32896) <= 2);
        CHECK(std::abs(static_cast<int>(pbr_normal[center + 3]) -
                       43908) <= 2);
        CHECK(std::abs(static_cast<int>(pbr_emissive[center + 0]) - 28) <= 1);
        CHECK(std::abs(static_cast<int>(pbr_emissive[center + 1]) - 13) <= 1);
        CHECK(pbr_emissive[center + 2] == 0);

        constexpr float half_pi = 1.5707963267948966f;
        batch.pbr_slots[0].texture_transform[1][2] = half_pi;
        batch.pbr_slots[1].texture_transform[1][2] = half_pi;
        auto rotated_pbr_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(rotated_pbr_result);
        std::array<std::uint16_t, 32 * 32 * 4> rotated_pbr_normal{};
        REQUIRE(rotated_pbr_result->colorTexture(2)->readback(
            *device, 0, 0, {}, rotated_pbr_normal.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(rotated_pbr_normal[center + 0]) -
                       32768) < 512);
        CHECK(rotated_pbr_normal[center + 1] < 512);
        batch.pbr_slots[0].texture_transform[1][2] = 0.f;
        batch.pbr_slots[1].texture_transform[1][2] = 0.f;

        batch.pbr_slot_count = 1;
        batch.texture_count = 1;
        auto scalar_pbr_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(scalar_pbr_result);
        std::array<std::uint8_t, 32 * 32 * 4> scalar_pbr_albedo{};
        REQUIRE(scalar_pbr_result->colorTexture(0)->readback(
            *device, 0, 0, {}, scalar_pbr_albedo.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(scalar_pbr_albedo[center + 0] == 0);
        CHECK(scalar_pbr_albedo[center + 1] == 255);
        CHECK(scalar_pbr_albedo[center + 2] == 0);
        batch.pbr_slot_count = 2;
        batch.texture_count = 2;

        // A masked PBR texel uses base texture alpha times streamed alpha and
        // the same half-byte equality bias as the GL owner.
        batch.alpha_mask = true;
        batch.pbr_slots[0].alpha_cutoff = 0.6f;
        batch.pbr_slots[1].alpha_cutoff = 0.6f;
        auto pbr_masked_out = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(pbr_masked_out);
        std::array<std::uint8_t, 32 * 32 * 4> pbr_masked_albedo{};
        REQUIRE(pbr_masked_out->colorTexture(0)->readback(
            *device, 0, 0, {}, pbr_masked_albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(pbr_masked_albedo[center + 0] == 255);
        CHECK(pbr_masked_albedo[center + 1] == 0);
        CHECK(pbr_masked_albedo[center + 2] == 255);
        batch.pbr_slots[0].alpha_cutoff = 0.253f;
        batch.pbr_slots[1].alpha_cutoff = 0.253f;
        auto pbr_masked_in = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(pbr_masked_in);
        REQUIRE(pbr_masked_in->colorTexture(0)->readback(
            *device, 0, 0, {}, pbr_masked_albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(pbr_masked_albedo[center + 0] == 0);
        CHECK(pbr_masked_albedo[center + 1] == 255);
        CHECK(pbr_masked_albedo[center + 2] == 0);

        // Reuse the same complete PBR contract through object skinning.
        batch.alpha_mask = false;
        batch.pbr_slots[0].alpha_cutoff = -1.f;
        batch.pbr_slots[1].alpha_cutoff = -1.f;
        batch.weight_offset = weight_offset;
        batch.skin_palette = skin_palette.data();
        batch.skin_palette_float_count = skin_palette.size();
        batch.skin_joint_count = 1;
        auto skinned_pbr_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(skinned_pbr_result);
        std::array<std::uint8_t, 32 * 32 * 4> skinned_pbr_albedo{};
        REQUIRE(skinned_pbr_result->colorTexture(0)->readback(
            *device, 0, 0, {}, skinned_pbr_albedo.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(skinned_pbr_albedo[translated_center + 0] == 0);
        CHECK(skinned_pbr_albedo[translated_center + 1] == 255);
        CHECK(skinned_pbr_albedo[translated_center + 2] == 0);

        // System-avatar eyes and body meshes have no streamed color. Rigid
        // parts use the live matrix stack; weighted parts use the legacy
        // scalar joint/blend stream and exact 45-float4 eye-space palette.
        batch = ALMetalWorldSimpleBatch{};
        batch.vertices = avatar_vertices.get();
        batch.position_offset = 0;
        batch.normal_offset = normal_offset;
        batch.texcoord_offset = texcoord_offset;
        batch.vertex_count = positions.size();
        batch.indices = indices.get();
        batch.index_count = index_values.size();
        batch.diffuse_textures[0] = diffuse.get();
        batch.texture_count = 1;
        batch.alpha_mask = true;
        batch.use_vertex_color = false;
        batch.alpha_cutoff_bias = false;
        batch.alpha_cutoff = 0.2f;
        batch.avatar_rigid = true;
        for (std::size_t index = 0; index < 4; ++index)
        {
            batch.modelview_projection[index * 4 + index] = 1.f;
        }
        for (std::size_t index = 0; index < 3; ++index)
        {
            batch.normal_matrix[index][index] = 1.f;
        }
        scene.hdr = true;

        batch.avatar_skinning = true;
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world simple batch is invalid");
        batch.avatar_skinning = false;
        error.clear();
        auto avatar_rigid_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(avatar_rigid_result);
        std::array<std::uint8_t, 32 * 32 * 4> avatar_rigid_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4> avatar_rigid_specular{};
        REQUIRE(avatar_rigid_result->colorTexture(0)->readback(
            *device, 0, 0, {}, avatar_rigid_albedo.data(), 32 * 4,
            &error));
        REQUIRE(avatar_rigid_result->colorTexture(1)->readback(
            *device, 0, 0, {}, avatar_rigid_specular.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(avatar_rigid_albedo[center + 0]) -
                       128) <= 1);
        CHECK(std::abs(static_cast<int>(avatar_rigid_albedo[center + 1]) -
                       64) <= 1);
        CHECK(std::abs(static_cast<int>(avatar_rigid_albedo[center + 2]) -
                       32) <= 1);
        CHECK(avatar_rigid_specular[center + 0] == 0);
        CHECK(avatar_rigid_specular[center + 1] == 0);
        CHECK(avatar_rigid_specular[center + 2] == 0);
        CHECK(avatar_rigid_specular[center + 3] == 0);

        std::array<float, 45 * 4> avatar_palette{};
        avatar_palette[0] = 1.f;
        avatar_palette[3] = 0.75f;
        avatar_palette[15 * 4 + 1] = 1.f;
        avatar_palette[30 * 4 + 2] = 1.f;
        batch.avatar_rigid = false;
        batch.avatar_skinning = true;
        batch.weight_offset = avatar_weight_offset;
        batch.skin_palette = avatar_palette.data();
        batch.skin_palette_float_count = avatar_palette.size() - 1;
        batch.skin_joint_count = 15;
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world avatar skinning data is invalid");
        batch.skin_palette_float_count = avatar_palette.size();
        error.clear();
        auto avatar_skinned_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(avatar_skinned_result);
        std::array<std::uint8_t, 32 * 32 * 4> avatar_skinned_albedo{};
        REQUIRE(avatar_skinned_result->colorTexture(0)->readback(
            *device, 0, 0, {}, avatar_skinned_albedo.data(), 32 * 4,
            &error));
        INFO(error);
        const std::size_t avatar_translated = (16 * 32 + 24) * 4;
        CHECK(avatar_skinned_albedo[center + 0] == 255);
        CHECK(avatar_skinned_albedo[center + 1] == 0);
        CHECK(avatar_skinned_albedo[center + 2] == 255);
        CHECK(std::abs(static_cast<int>(
                           avatar_skinned_albedo[avatar_translated + 0]) -
                       128) <= 1);
        CHECK(std::abs(static_cast<int>(
                           avatar_skinned_albedo[avatar_translated + 1]) -
                       64) <= 1);

        // With a unit wind strength and phase PI, the owner's minimax sine
        // produces a unit primary displacement. The clothing X vector moves
        // this triangle another quarter clip unit; its X wind also turns the
        // stored normal through the same exaggerated legacy perturbation.
        batch.avatar_cloth = true;
        batch.cloth_offset = avatar_cloth_offset;
        batch.avatar_wind[0] = 1.f;
        batch.avatar_wind[3] = 1.f;
        batch.avatar_sin_wave[3] = 3.141592f;
        auto avatar_cloth_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(avatar_cloth_result);
        std::array<std::uint8_t, 32 * 32 * 4> avatar_cloth_albedo{};
        std::array<std::uint16_t, 32 * 32 * 4> avatar_cloth_normal{};
        REQUIRE(avatar_cloth_result->colorTexture(0)->readback(
            *device, 0, 0, {}, avatar_cloth_albedo.data(), 32 * 4,
            &error));
        REQUIRE(avatar_cloth_result->colorTexture(2)->readback(
            *device, 0, 0, {}, avatar_cloth_normal.data(), 32 * 8,
            &error));
        INFO(error);
        const std::size_t avatar_cloth_translated = (16 * 32 + 28) * 4;
        CHECK(avatar_cloth_albedo[avatar_translated + 0] == 255);
        CHECK(avatar_cloth_albedo[avatar_translated + 1] == 0);
        CHECK(avatar_cloth_albedo[avatar_translated + 2] == 255);
        CHECK(std::abs(static_cast<int>(
                           avatar_cloth_albedo[
                               avatar_cloth_translated + 0]) - 128) <= 1);
        CHECK(avatar_cloth_normal[avatar_cloth_translated + 0] > 40000);
        CHECK(std::abs(static_cast<int>(
                           avatar_cloth_normal[
                               avatar_cloth_translated + 1]) - 32768) <= 2);
    }
}

TEST_CASE("almetal: avatar impostor replays a coherent baked G-buffer", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error);
        REQUIRE(renderer);

        const auto make_texture = [&device, &error](
            ALMetalPixelFormat format, const char* label)
        {
            ALMetalTextureDescriptor descriptor;
            descriptor.format = format;
            descriptor.width = 1;
            descriptor.height = 1;
            descriptor.storage = ALMetalStorageMode::private_gpu;
            descriptor.usage = ALMetalTextureUsage::shader_read;
            descriptor.label = label;
            auto texture = ALMetalTexture::create(
                *device, descriptor, &error);
            INFO(error);
            REQUIRE(texture);
            return texture;
        };

        auto albedo = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb,
            "Alchemy impostor baked albedo");
        auto specular = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy impostor baked specular ORM");
        auto normal = make_texture(
            ALMetalPixelFormat::rgba16_unorm,
            "Alchemy impostor baked normal");
        auto emissive = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy impostor baked emissive");

        const std::array<std::uint8_t, 4> albedo_pixel{{
            128, 64, 32, 128,
        }};
        const std::array<std::uint8_t, 4> specular_pixel{{
            17, 34, 51, 68,
        }};
        const std::array<std::uint16_t, 4> normal_pixel{{
            65535, 32768, 16384, 43908,
        }};
        const std::array<std::uint8_t, 4> emissive_pixel{{
            11, 22, 33, 255,
        }};
        auto upload = device->makeCommandBuffer(
            "Alchemy impostor G-buffer upload", &error);
        INFO(error);
        REQUIRE(upload);
        REQUIRE(albedo->upload(
            *upload, 0, 0, {}, albedo_pixel.data(), 4, &error));
        REQUIRE(specular->upload(
            *upload, 0, 0, {}, specular_pixel.data(), 4, &error));
        REQUIRE(normal->upload(
            *upload, 0, 0, {}, normal_pixel.data(), 8, &error));
        REQUIRE(emissive->upload(
            *upload, 0, 0, {}, emissive_pixel.data(), 4, &error));
        REQUIRE(upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldSimpleBatch batch;
        batch.impostor = true;
        batch.impostor_textures = {
            albedo.get(), specular.get(), normal.get(), emissive.get(),
        };
        batch.impostor_left[0] = 0.8f;
        batch.impostor_up[1] = 0.8f;
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            batch.modelview_projection[axis * 4 + axis] = 1.f;
            if (axis < 3)
            {
                batch.impostor_normal_rotation[axis][axis] = 1.f;
            }
        }

        ALMetalWorldGBufferScene scene;
        scene.batches = &batch;
        scene.batch_count = 1;
        scene.width = 32;
        scene.height = 32;
        scene.normal_hdr = true;
        scene.hdr = false;

        auto result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(result);
        REQUIRE(device->waitUntilIdle(&error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> output_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4> output_specular{};
        std::array<std::uint16_t, 32 * 32 * 4> output_normal{};
        std::array<std::uint8_t, 32 * 32 * 4> output_emissive{};
        REQUIRE(result->colorTexture(0)->readback(
            *device, 0, 0, {}, output_albedo.data(), 32 * 4, &error));
        REQUIRE(result->colorTexture(1)->readback(
            *device, 0, 0, {}, output_specular.data(), 32 * 4, &error));
        REQUIRE(result->colorTexture(2)->readback(
            *device, 0, 0, {}, output_normal.data(), 32 * 8, &error));
        REQUIRE(result->colorTexture(3)->readback(
            *device, 0, 0, {}, output_emissive.data(), 32 * 4, &error));
        INFO(error);

        const auto srgb_to_linear = [](float encoded)
        {
            return encoded <= 0.04045f
                ? encoded / 12.92f
                : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
        };
        const auto linear_to_srgb = [](float linear)
        {
            return linear <= 0.0031308f
                ? linear * 12.92f
                : 1.055f * std::pow(linear, 1.f / 2.4f) - 0.055f;
        };
        const std::size_t center = (16 * 32 + 16) * 4;
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const float encoded = albedo_pixel[channel] / 255.f;
            const float recovered = srgb_to_linear(encoded) /
                                    (albedo_pixel[3] / 255.f);
            const int expected = static_cast<int>(std::lround(
                std::clamp(linear_to_srgb(recovered), 0.f, 1.f) * 255.f));
            CHECK(std::abs(static_cast<int>(
                      output_albedo[center + channel]) - expected) <= 1);
        }
        CHECK(output_albedo[center + 3] == 0);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(output_specular[center + channel] ==
                  specular_pixel[channel]);
        }
        CHECK(std::abs(static_cast<int>(output_normal[center + 0]) -
                       65535) <= 2);
        CHECK(std::abs(static_cast<int>(output_normal[center + 1]) -
                       32768) <= 2);
        CHECK(std::abs(static_cast<int>(output_normal[center + 2]) -
                       16384) <= 2);
        CHECK(std::abs(static_cast<int>(output_normal[center + 3]) -
                       43908) <= 2);
        CHECK(output_emissive[center + 0] == emissive_pixel[0]);
        CHECK(output_emissive[center + 1] == emissive_pixel[1]);
        CHECK(output_emissive[center + 2] == emissive_pixel[2]);
        CHECK(output_emissive[center + 3] == 0);

        // The viewer publishes a completed native impostor bake by moving
        // all four resolved attachments directly into the retained per-avatar
        // mirror. Prove that this ownership handoff stays sampleable without
        // a GL/PBO round trip and can immediately feed the world replay.
        scene.stamp_opaque_coverage = true;
        auto native_bake = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(native_bake);
        std::array<std::unique_ptr<ALMetalTexture>, 4> published_textures;
        for (std::size_t attachment = 0;
             attachment < published_textures.size(); ++attachment)
        {
            published_textures[attachment] =
                native_bake->takeColorTexture(attachment);
            REQUIRE(published_textures[attachment]);
            CHECK(native_bake->colorTexture(attachment) == nullptr);
            batch.impostor_textures[attachment] =
                published_textures[attachment].get();
        }
        scene.stamp_opaque_coverage = false;
        auto direct_replay = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(direct_replay);
        REQUIRE(device->waitUntilIdle(&error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> direct_albedo{};
        REQUIRE(direct_replay->colorTexture(0)->readback(
            *device, 0, 0, {}, direct_albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(direct_albedo[center + 0] == output_albedo[center + 0]);
        CHECK(direct_albedo[center + 1] == output_albedo[center + 1]);
        CHECK(direct_albedo[center + 2] == output_albedo[center + 2]);
        CHECK(direct_albedo[center + 3] == 0);

        // Rotate the baked +X normal to +Y. The PBR flag and the blue
        // geometric-normal payload remain byte-for-byte material data.
        std::memset(batch.impostor_normal_rotation, 0,
                    sizeof(batch.impostor_normal_rotation));
        batch.impostor_normal_rotation[0][1] = 1.f;
        batch.impostor_normal_rotation[1][0] = -1.f;
        batch.impostor_normal_rotation[2][2] = 1.f;
        auto rotated = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(rotated);
        REQUIRE(device->waitUntilIdle(&error));
        output_normal.fill(0);
        REQUIRE(rotated->colorTexture(2)->readback(
            *device, 0, 0, {}, output_normal.data(), 32 * 8, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(output_normal[center + 0]) -
                       32768) <= 2);
        CHECK(std::abs(static_cast<int>(output_normal[center + 1]) -
                       65535) <= 2);
        CHECK(std::abs(static_cast<int>(output_normal[center + 2]) -
                       16384) <= 2);
        CHECK(std::abs(static_cast<int>(output_normal[center + 3]) -
                       43908) <= 2);

        batch.impostor_textures[3] = nullptr;
        error.clear();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world impostor batch is invalid");
    }
}

TEST_CASE("almetal: native world G-buffer feeds exact deferred point lights", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error);
        REQUIRE(renderer);

        const auto make_texture = [&device, &error](
            ALMetalPixelFormat format, const char* label)
        {
            ALMetalTextureDescriptor descriptor;
            descriptor.format = format;
            descriptor.width = 1;
            descriptor.height = 1;
            descriptor.storage = ALMetalStorageMode::private_gpu;
            descriptor.usage = ALMetalTextureUsage::shader_read;
            descriptor.label = label;
            auto texture = ALMetalTexture::create(
                *device, descriptor, &error);
            INFO(error);
            REQUIRE(texture);
            return texture;
        };

        auto albedo = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb,
            "Alchemy local-light G-buffer albedo");
        auto specular = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy local-light G-buffer specular ORM");
        auto normal = make_texture(
            ALMetalPixelFormat::rgba16_unorm,
            "Alchemy local-light G-buffer normal");
        auto emissive = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy local-light G-buffer emissive");

        const std::array<std::uint8_t, 4> albedo_pixel{{
            128, 96, 64, 255,
        }};
        const std::array<std::uint8_t, 4> legacy_specular{{
            0, 0, 0, 0,
        }};
        const std::array<std::uint16_t, 4> legacy_normal{{
            32768, 32768, 0, 22282,
        }};
        const std::array<std::uint8_t, 4> black{{0, 0, 0, 0}};
        auto upload = device->makeCommandBuffer(
            "Alchemy local-light G-buffer upload", &error);
        INFO(error);
        REQUIRE(upload);
        REQUIRE(albedo->upload(
            *upload, 0, 0, {}, albedo_pixel.data(), 4, &error));
        REQUIRE(specular->upload(
            *upload, 0, 0, {}, legacy_specular.data(), 4, &error));
        REQUIRE(normal->upload(
            *upload, 0, 0, {}, legacy_normal.data(), 8, &error));
        REQUIRE(emissive->upload(
            *upload, 0, 0, {}, black.data(), 4, &error));
        REQUIRE(upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldSimpleBatch batch;
        batch.impostor = true;
        batch.impostor_textures = {
            albedo.get(), specular.get(), normal.get(), emissive.get(),
        };
        batch.impostor_center[2] = -0.5f;
        batch.impostor_left[0] = 0.75f;
        batch.impostor_up[1] = 0.75f;
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            batch.modelview_projection[axis * 4 + axis] = 1.f;
            if (axis < 3)
            {
                batch.impostor_normal_rotation[axis][axis] = 1.f;
            }
        }

        ALMetalWorldGBufferScene gbuffer_scene;
        gbuffer_scene.batches = &batch;
        gbuffer_scene.batch_count = 1;
        gbuffer_scene.width = 32;
        gbuffer_scene.height = 32;
        gbuffer_scene.normal_hdr = true;
        std::fill(std::begin(gbuffer_scene.clear_color),
                  std::end(gbuffer_scene.clear_color), 0.f);
        auto gbuffer = renderer->render(gbuffer_scene, &error);
        INFO(error);
        REQUIRE(gbuffer);

        std::array<ALMetalWorldLocalLight, 2> lights{};
        lights[0].position_radius[2] = 0.5f;
        lights[0].position_radius[3] = 2.f;
        lights[0].color_falloff[0] = 0.8f;
        lights[0].color_falloff[1] = 0.4f;
        lights[0].color_falloff[2] = 0.3f;
        lights[0].color_falloff[3] = 0.5f;
        lights[1] = lights[0];

        ALMetalWorldLocalLightingScene lighting_scene;
        lighting_scene.gbuffer = gbuffer.get();
        lighting_scene.lights = lights.data();
        lighting_scene.light_count = 1;
        lighting_scene.specular_exponent = 128.f;
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            lighting_scene.inverse_projection[axis * 4 + axis] = 1.f;
        }

        auto legacy = renderer->renderLocalLights(lighting_scene, &error);
        INFO(error);
        REQUIRE(legacy);
        REQUIRE(legacy->descriptor().colors[0].format ==
                ALMetalPixelFormat::rgba8_unorm);
        std::array<std::uint8_t, 32 * 32 * 4> legacy_pixels{};
        REQUIRE(legacy->colorTexture(0)->readback(
            *device, 0, 0, {}, legacy_pixels.data(), 32 * 4, &error));
        INFO(error);
        const std::size_t center = (16 * 32 + 16) * 4;
        const std::size_t outside = (2 * 32 + 2) * 4;
        CHECK(legacy_pixels[center + 0] > legacy_pixels[center + 1]);
        CHECK(legacy_pixels[center + 1] > legacy_pixels[center + 2]);
        CHECK(legacy_pixels[center + 2] > 0);
        CHECK(legacy_pixels[center + 3] == 0);
        CHECK(legacy_pixels[outside + 0] == 0);
        CHECK(legacy_pixels[outside + 1] == 0);
        CHECK(legacy_pixels[outside + 2] == 0);

        const auto srgb_to_linear = [](float encoded)
        {
            return encoded <= 0.04045f
                ? encoded / 12.92f
                : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
        };
        const float center_ndc = 2.f * (16.5f / 32.f) - 1.f;
        const float light_distance = std::sqrt(
            1.f + 2.f * center_ndc * center_ndc);
        const float normalized_distance = light_distance / 2.f;
        float attenuation = 1.f - std::clamp(
            (normalized_distance + 0.5f) / 1.5f, 0.f, 1.f);
        attenuation = attenuation * attenuation * 2.f;
        const float normal_light = 1.f / light_distance;
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const float expected_linear = srgb_to_linear(
                albedo_pixel[channel] / 255.f) *
                lights[0].color_falloff[channel] *
                normal_light * attenuation;
            const int expected = static_cast<int>(std::lround(
                std::clamp(expected_linear, 0.f, 1.f) * 255.f));
            CHECK(std::abs(static_cast<int>(
                      legacy_pixels[center + channel]) - expected) <= 1);
        }

        lighting_scene.lights = nullptr;
        lighting_scene.light_count = 0;
        auto unlit = renderer->renderLocalLights(lighting_scene, &error);
        INFO(error);
        REQUIRE(unlit);
        std::array<std::uint8_t, 32 * 32 * 4> unlit_pixels{};
        REQUIRE(unlit->colorTexture(0)->readback(
            *device, 0, 0, {}, unlit_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(unlit_pixels[center + 0] == 0);
        CHECK(unlit_pixels[center + 1] == 0);
        CHECK(unlit_pixels[center + 2] == 0);
        lighting_scene.lights = lights.data();
        lighting_scene.light_count = 1;

        // Reverse-Z stores zero-to-one clip depth directly. Use the matching
        // projection/inverse pair and require the same reconstructed surface
        // and local-light result as the forward-Z path.
        batch.modelview_projection[10] = -1.f;
        gbuffer_scene.reverse_z = true;
        auto reverse_gbuffer = renderer->render(gbuffer_scene, &error);
        INFO(error);
        REQUIRE(reverse_gbuffer);
        lighting_scene.gbuffer = reverse_gbuffer.get();
        lighting_scene.inverse_projection[10] = -1.f;
        lighting_scene.reverse_z = true;
        auto reverse = renderer->renderLocalLights(lighting_scene, &error);
        INFO(error);
        REQUIRE(reverse);
        std::array<std::uint8_t, 32 * 32 * 4> reverse_pixels{};
        REQUIRE(reverse->colorTexture(0)->readback(
            *device, 0, 0, {}, reverse_pixels.data(), 32 * 4, &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            CHECK(std::abs(static_cast<int>(
                      reverse_pixels[center + channel]) -
                  static_cast<int>(legacy_pixels[center + channel])) <= 1);
        }
        batch.modelview_projection[10] = 1.f;
        gbuffer_scene.reverse_z = false;
        lighting_scene.gbuffer = gbuffer.get();
        lighting_scene.inverse_projection[10] = 1.f;
        lighting_scene.reverse_z = false;

        lighting_scene.classic_mode = true;
        auto classic = renderer->renderLocalLights(lighting_scene, &error);
        INFO(error);
        REQUIRE(classic);
        std::array<std::uint8_t, 32 * 32 * 4> classic_pixels{};
        REQUIRE(classic->colorTexture(0)->readback(
            *device, 0, 0, {}, classic_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(classic_pixels[center + 0] < legacy_pixels[center + 0]);
        CHECK(classic_pixels[center + 1] < legacy_pixels[center + 1]);

        lighting_scene.classic_mode = false;
        lighting_scene.light_count = 2;
        auto doubled = renderer->renderLocalLights(lighting_scene, &error);
        INFO(error);
        REQUIRE(doubled);
        std::array<std::uint8_t, 32 * 32 * 4> doubled_pixels{};
        REQUIRE(doubled->colorTexture(0)->readback(
            *device, 0, 0, {}, doubled_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(doubled_pixels[center + 0] > legacy_pixels[center + 0]);
        CHECK(doubled_pixels[center + 1] > legacy_pixels[center + 1]);
        CHECK(doubled_pixels[center + 2] > legacy_pixels[center + 2]);

        // The projector owner uses one draw per cookie so every light keeps
        // its own sRGB mip chain. Its transitional shadow input is the pair
        // of raw GL spot-depth maps; Metal owns the exact filter and fade.
        ALMetalTextureDescriptor cookie_descriptor;
        cookie_descriptor.format = ALMetalPixelFormat::rgba8_unorm_srgb;
        cookie_descriptor.width = 1;
        cookie_descriptor.height = 1;
        cookie_descriptor.storage = ALMetalStorageMode::private_gpu;
        cookie_descriptor.usage = ALMetalTextureUsage::shader_read;
        cookie_descriptor.label = "Alchemy projector colored sRGB cookie";
        auto cookie = ALMetalTexture::create(
            *device, cookie_descriptor, &error);
        INFO(error);
        REQUIRE(cookie);

        ALMetalTextureDescriptor shadow_descriptor;
        shadow_descriptor.type = ALMetalTextureType::texture_2d_array;
        shadow_descriptor.format = ALMetalPixelFormat::r32_float;
        shadow_descriptor.width = 32;
        shadow_descriptor.height = 32;
        shadow_descriptor.array_length = 2;
        shadow_descriptor.storage = ALMetalStorageMode::private_gpu;
        shadow_descriptor.usage = ALMetalTextureUsage::shader_read;
        shadow_descriptor.label =
            "Alchemy projector raw spot-shadow depths";
        auto projector_shadows = ALMetalTexture::create(
            *device, shadow_descriptor, &error);
        INFO(error);
        REQUIRE(projector_shadows);

        const std::array<std::uint8_t, 4> colored_cookie{{
            128, 192, 255, 255,
        }};
        const std::array<float, 32 * 32> projector_shadowed_depth = []
        {
            std::array<float, 32 * 32> value{};
            value.fill(0.f);
            return value;
        }();
        const std::array<float, 32 * 32> projector_lit_depth = []
        {
            std::array<float, 32 * 32> value{};
            value.fill(1.f);
            return value;
        }();
        auto projector_upload = device->makeCommandBuffer(
            "Alchemy projector input upload", &error);
        INFO(error);
        REQUIRE(projector_upload);
        REQUIRE(cookie->upload(
            *projector_upload, 0, 0, {}, colored_cookie.data(), 4,
            &error));
        REQUIRE(projector_shadows->upload(
            *projector_upload, 0, 0, {}, projector_shadowed_depth.data(),
            32 * sizeof(float), &error));
        REQUIRE(projector_shadows->upload(
            *projector_upload, 0, 1, {}, projector_lit_depth.data(),
            32 * sizeof(float),
            &error));
        REQUIRE(projector_upload->commitAndWait(&error));
        INFO(error);

        std::array<ALMetalWorldProjectorLight, 2> projectors{};
        ALMetalWorldProjectorLight& projector = projectors[0];
        projector.cookie_texture = cookie.get();
        std::copy(std::begin(lights[0].position_radius),
                  std::end(lights[0].position_radius),
                  projector.position_radius);
        std::copy(std::begin(lights[0].color_falloff),
                  std::end(lights[0].color_falloff),
                  projector.color_falloff);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            projector.color_falloff[channel] *= 4.f;
        }
        projector.projector_matrix[0] = 0.5f;
        projector.projector_matrix[5] = 0.5f;
        projector.projector_matrix[12] = 0.5f;
        projector.projector_matrix[13] = 0.5f;
        projector.projector_matrix[14] = 1.f;
        projector.projector_matrix[15] = 1.f;
        projector.projector_normal[2] = -1.f;
        projector.projector_normal[3] = 2.f;
        projector.projector_origin[2] = 0.5f;
        projector.projector_origin[3] = 2.f;
        projector.sampling[0] = 1.f;
        projector.shadow_channel = 1;
        projectors[1] = projector;

        ALMetalWorldDirectionalShadows projector_shadow_input;
        projector_shadow_input.depth_texture = projector_shadows.get();
        for (std::size_t slice = 0; slice < 2; ++slice)
        {
            float* matrix = projector_shadow_input.constants
                                .shadow_matrix[slice];
            matrix[0] = 0.5f;
            matrix[5] = 0.5f;
            matrix[12] = 0.5f;
            matrix[13] = 0.5f;
            matrix[14] = 0.5f;
            matrix[15] = 1.f;
        }
        projector_shadow_input.constants.shadow_clip[2] = 1.f;
        projector_shadow_input.constants.shadow_clip[3] = 2.f;
        projector_shadow_input.constants.resolution[0] = 32.f;
        projector_shadow_input.constants.resolution[1] = 32.f;
        projector_shadow_input.constants.resolution[2] = 32.f;
        projector_shadow_input.constants.resolution[3] = 32.f;
        projector_shadow_input.constants.parameters[2] = 32.f;
        projector_shadow_input.constants.parameters[3] = 32.f;
        projector_shadow_input.constants.config[0] = 2;
        projector_shadow_input.constants.config[3] = 1;

        ALMetalWorldProjectorLightingScene projector_scene;
        projector_scene.gbuffer = gbuffer.get();
        projector_scene.lights = projectors.data();
        projector_scene.light_count = 1;
        projector_scene.projector_shadows = &projector_shadow_input;
        projector_scene.specular_exponent = 128.f;
        projector_scene.normal_hdr = true;
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            projector_scene.inverse_projection[axis * 4 + axis] = 1.f;
        }
        auto projected = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(projected);
        std::array<std::uint8_t, 32 * 32 * 4> projected_pixels{};
        REQUIRE(projected->colorTexture(0)->readback(
            *device, 0, 0, {}, projected_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(projected_pixels[center + 0] > 0);
        CHECK(projected_pixels[center + 1] > 0);
        CHECK(projected_pixels[center + 2] > 0);
        CHECK(projected_pixels[center + 3] == 0);
        CHECK(projected_pixels[outside + 0] == 0);
        CHECK(projected_pixels[outside + 1] == 0);
        CHECK(projected_pixels[outside + 2] == 0);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const float expected_linear = srgb_to_linear(
                albedo_pixel[channel] / 255.f) *
                projector.color_falloff[channel] *
                srgb_to_linear(colored_cookie[channel] / 255.f) *
                normal_light * attenuation;
            const int expected = static_cast<int>(std::lround(
                std::clamp(expected_linear, 0.f, 1.f) * 255.f));
            CHECK(std::abs(static_cast<int>(
                      projected_pixels[center + channel]) - expected) <= 1);
        }

        projector.shadow_channel = 0;
        auto shadowed = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(shadowed);
        std::array<std::uint8_t, 32 * 32 * 4> shadowed_pixels{};
        REQUIRE(shadowed->colorTexture(0)->readback(
            *device, 0, 0, {}, shadowed_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(shadowed_pixels[center + 0] == 0);
        CHECK(shadowed_pixels[center + 1] == 0);
        CHECK(shadowed_pixels[center + 2] == 0);

        ALMetalWorldProjectorLightingScene missing_projector_shadows =
            projector_scene;
        missing_projector_shadows.projector_shadows = nullptr;
        error.clear();
        CHECK_FALSE(renderer->renderProjectorLights(
            missing_projector_shadows, &error));
        CHECK(error ==
              "Metal world projector shadows are incompatible");
        error.clear();

        // The same raw map is lit when the receiver itself is outside the
        // spot frustum; normal offset does not control this early decision.
        projector_shadow_input.constants.shadow_matrix[0][12] = 2.f;
        auto outside_shadow_frustum = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(outside_shadow_frustum);
        std::array<std::uint8_t, 32 * 32 * 4>
            outside_shadow_frustum_pixels{};
        REQUIRE(outside_shadow_frustum->colorTexture(0)->readback(
            *device, 0, 0, {}, outside_shadow_frustum_pixels.data(),
            32 * 4, &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            CHECK(std::abs(static_cast<int>(
                      outside_shadow_frustum_pixels[center + channel]) -
                  static_cast<int>(projected_pixels[center + channel])) <= 1);
        }
        projector_shadow_input.constants.shadow_matrix[0][12] = 0.5f;

        projector_shadow_input.constants.config[1] = 1u;
        auto unbuilt_projector_shadow = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(unbuilt_projector_shadow);
        std::array<std::uint8_t, 32 * 32 * 4>
            unbuilt_projector_shadow_pixels{};
        REQUIRE(unbuilt_projector_shadow->colorTexture(0)->readback(
            *device, 0, 0, {}, unbuilt_projector_shadow_pixels.data(),
            32 * 4, &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            CHECK(std::abs(static_cast<int>(
                      unbuilt_projector_shadow_pixels[center + channel]) -
                  static_cast<int>(projected_pixels[center + channel])) <= 1);
        }
        projector_shadow_input.constants.config[1] = 0u;

        projector_shadow_input.constants.config[3] = 3u;
        auto pcss_projector_shadow = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(pcss_projector_shadow);
        std::array<std::uint8_t, 32 * 32 * 4>
            pcss_projector_shadow_pixels{};
        REQUIRE(pcss_projector_shadow->colorTexture(0)->readback(
            *device, 0, 0, {}, pcss_projector_shadow_pixels.data(),
            32 * 4, &error));
        INFO(error);
        CHECK(pcss_projector_shadow_pixels[center + 0] == 0);
        CHECK(pcss_projector_shadow_pixels[center + 1] == 0);
        CHECK(pcss_projector_shadow_pixels[center + 2] == 0);
        projector_shadow_input.constants.config[3] = 1u;

        projector.sampling[1] = 0.5f;
        auto fading_shadow = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(fading_shadow);
        std::array<std::uint8_t, 32 * 32 * 4> fading_pixels{};
        REQUIRE(fading_shadow->colorTexture(0)->readback(
            *device, 0, 0, {}, fading_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            CHECK(std::abs(
                      static_cast<int>(fading_pixels[center + channel]) * 2 -
                      static_cast<int>(projected_pixels[center + channel]))
                  <= 2);
        }

        projector.shadow_channel = 1;
        projector.sampling[1] = 0.f;
        const std::array<std::uint16_t, 4> backface_normal{{
            65535, 65535, 0, 22282,
        }};
        auto backface_upload = device->makeCommandBuffer(
            "Alchemy projector ambiance backface upload", &error);
        INFO(error);
        REQUIRE(backface_upload);
        REQUIRE(normal->upload(
            *backface_upload, 0, 0, {}, backface_normal.data(), 8,
            &error));
        REQUIRE(backface_upload->commitAndWait(&error));
        INFO(error);
        auto backface_gbuffer = renderer->render(gbuffer_scene, &error);
        INFO(error);
        REQUIRE(backface_gbuffer);
        projector_scene.gbuffer = backface_gbuffer.get();
        auto no_ambient_projector = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(no_ambient_projector);
        std::array<std::uint8_t, 32 * 32 * 4>
            no_ambient_projector_pixels{};
        REQUIRE(no_ambient_projector->colorTexture(0)->readback(
            *device, 0, 0, {}, no_ambient_projector_pixels.data(),
            32 * 4, &error));
        INFO(error);
        projector.projector_plane[3] = 1.f;
        auto ambient_projector = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(ambient_projector);
        std::array<std::uint8_t, 32 * 32 * 4>
            ambient_projector_pixels{};
        REQUIRE(ambient_projector->colorTexture(0)->readback(
            *device, 0, 0, {}, ambient_projector_pixels.data(),
            32 * 4, &error));
        INFO(error);
        CHECK(ambient_projector_pixels[center + 0] >
              no_ambient_projector_pixels[center + 0]);
        CHECK(ambient_projector_pixels[center + 1] >
              no_ambient_projector_pixels[center + 1]);
        projector.projector_plane[3] = 0.f;
        projector_scene.gbuffer = gbuffer.get();

        projector.projector_matrix[12] = 2.f;
        auto outside_cookie = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(outside_cookie);
        std::array<std::uint8_t, 32 * 32 * 4>
            outside_cookie_pixels{};
        REQUIRE(outside_cookie->colorTexture(0)->readback(
            *device, 0, 0, {}, outside_cookie_pixels.data(),
            32 * 4, &error));
        INFO(error);
        CHECK(outside_cookie_pixels[center + 0] == 0);
        CHECK(outside_cookie_pixels[center + 1] == 0);
        CHECK(outside_cookie_pixels[center + 2] == 0);
        projector.projector_matrix[12] = 0.5f;

        projector_scene.classic_mode = true;
        auto classic_projector = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(classic_projector);
        std::array<std::uint8_t, 32 * 32 * 4> classic_projector_pixels{};
        REQUIRE(classic_projector->colorTexture(0)->readback(
            *device, 0, 0, {}, classic_projector_pixels.data(),
            32 * 4, &error));
        INFO(error);
        CHECK(classic_projector_pixels[center + 1] <
              projected_pixels[center + 1]);
        projector_scene.classic_mode = false;

        const std::array<float, 32 * 32> reverse_shadowed_depth = []
        {
            std::array<float, 32 * 32> value{};
            value.fill(1.f);
            return value;
        }();
        const std::array<float, 32 * 32> reverse_lit_depth = []
        {
            std::array<float, 32 * 32> value{};
            value.fill(0.f);
            return value;
        }();
        auto reverse_shadow_upload = device->makeCommandBuffer(
            "Alchemy reverse-Z projector shadow upload", &error);
        INFO(error);
        REQUIRE(reverse_shadow_upload);
        REQUIRE(projector_shadows->upload(
            *reverse_shadow_upload, 0, 0, {},
            reverse_shadowed_depth.data(), 32 * sizeof(float), &error));
        REQUIRE(projector_shadows->upload(
            *reverse_shadow_upload, 0, 1, {}, reverse_lit_depth.data(),
            32 * sizeof(float), &error));
        REQUIRE(reverse_shadow_upload->commitAndWait(&error));
        INFO(error);
        projector_shadow_input.constants.config[2] = 1u;
        projector_scene.gbuffer = reverse_gbuffer.get();
        projector_scene.inverse_projection[10] = -1.f;
        projector_scene.reverse_z = true;
        auto reverse_projector = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(reverse_projector);
        std::array<std::uint8_t, 32 * 32 * 4>
            reverse_projector_pixels{};
        REQUIRE(reverse_projector->colorTexture(0)->readback(
            *device, 0, 0, {}, reverse_projector_pixels.data(),
            32 * 4, &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            CHECK(std::abs(static_cast<int>(
                      reverse_projector_pixels[center + channel]) -
                  static_cast<int>(projected_pixels[center + channel])) <= 1);
        }
        projector_scene.gbuffer = gbuffer.get();
        projector_scene.inverse_projection[10] = 1.f;
        projector_scene.reverse_z = false;
        projector_shadow_input.constants.config[2] = 0u;
        auto forward_shadow_upload = device->makeCommandBuffer(
            "Alchemy forward-Z projector shadow restore", &error);
        INFO(error);
        REQUIRE(forward_shadow_upload);
        REQUIRE(projector_shadows->upload(
            *forward_shadow_upload, 0, 0, {},
            projector_shadowed_depth.data(),
            32 * sizeof(float), &error));
        REQUIRE(projector_shadows->upload(
            *forward_shadow_upload, 0, 1, {}, projector_lit_depth.data(),
            32 * sizeof(float), &error));
        REQUIRE(forward_shadow_upload->commitAndWait(&error));
        INFO(error);

        projector_scene.light_count = 2;
        auto doubled_projector = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(doubled_projector);
        std::array<std::uint8_t, 32 * 32 * 4>
            doubled_projector_pixels{};
        REQUIRE(doubled_projector->colorTexture(0)->readback(
            *device, 0, 0, {}, doubled_projector_pixels.data(),
            32 * 4, &error));
        INFO(error);
        CHECK(doubled_projector_pixels[center + 0] >
              projected_pixels[center + 0]);
        CHECK(doubled_projector_pixels[center + 1] >
              projected_pixels[center + 1]);
        projector_scene.light_count = 1;

        // A legacy environment-intensity payload keeps the fragment alive
        // outside its direct receiver. Its reflected ray intersects the
        // emitting plane and samples the cookie's specular LOD independently.
        const std::array<std::uint8_t, 4> reflective_specular{{
            255, 255, 255, 255,
        }};
        const std::array<std::uint16_t, 4> reflective_normal{{
            32768, 32768, 32768, 22282,
        }};
        auto reflective_upload = device->makeCommandBuffer(
            "Alchemy reflective projector G-buffer upload", &error);
        INFO(error);
        REQUIRE(reflective_upload);
        REQUIRE(specular->upload(
            *reflective_upload, 0, 0, {}, reflective_specular.data(),
            4, &error));
        REQUIRE(normal->upload(
            *reflective_upload, 0, 0, {}, reflective_normal.data(),
            8, &error));
        REQUIRE(reflective_upload->commitAndWait(&error));
        INFO(error);
        auto reflective_gbuffer = renderer->render(
            gbuffer_scene, &error);
        INFO(error);
        REQUIRE(reflective_gbuffer);
        projector_scene.gbuffer = reflective_gbuffer.get();
        projector.projector_matrix[12] = -0.02f;
        projector.projector_plane[2] = 3.f;
        auto reflected_projector = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(reflected_projector);
        std::array<std::uint8_t, 32 * 32 * 4>
            reflected_projector_pixels{};
        REQUIRE(reflected_projector->colorTexture(0)->readback(
            *device, 0, 0, {}, reflected_projector_pixels.data(),
            32 * 4, &error));
        INFO(error);
        CHECK(reflected_projector_pixels[center + 0] > 0);
        CHECK(reflected_projector_pixels[center + 1] > 0);
        CHECK(reflected_projector_pixels[center + 3] == 0);
        projector.projector_matrix[12] = 0.5f;
        projector.projector_plane[2] = 0.f;
        projector_scene.gbuffer = gbuffer.get();

        // Switch the same native G-buffer owner to the packed PBR material
        // flag/roughness contract. The local-light pass must use the shared
        // GGX split and generated BRDF energy-compensation table rather than
        // interpreting these bytes as legacy specular/glossiness.
        const std::array<std::uint8_t, 4> pbr_orm{{
            255, 128, 0, 0,
        }};
        const std::array<std::uint16_t, 4> pbr_normal{{
            32768, 32768, 0, 43908,
        }};
        auto pbr_upload = device->makeCommandBuffer(
            "Alchemy local-light PBR G-buffer upload", &error);
        INFO(error);
        REQUIRE(pbr_upload);
        REQUIRE(specular->upload(
            *pbr_upload, 0, 0, {}, pbr_orm.data(), 4, &error));
        REQUIRE(normal->upload(
            *pbr_upload, 0, 0, {}, pbr_normal.data(), 8, &error));
        REQUIRE(pbr_upload->commitAndWait(&error));
        INFO(error);
        auto pbr_gbuffer = renderer->render(gbuffer_scene, &error);
        INFO(error);
        REQUIRE(pbr_gbuffer);
        batch.modelview_projection[10] = -1.f;
        gbuffer_scene.reverse_z = true;
        auto pbr_reverse_gbuffer = renderer->render(
            gbuffer_scene, &error);
        INFO(error);
        REQUIRE(pbr_reverse_gbuffer);
        batch.modelview_projection[10] = 1.f;
        gbuffer_scene.reverse_z = false;
        lighting_scene.gbuffer = pbr_gbuffer.get();
        lighting_scene.light_count = 1;
        auto pbr = renderer->renderLocalLights(lighting_scene, &error);
        INFO(error);
        REQUIRE(pbr);
        std::array<std::uint8_t, 32 * 32 * 4> pbr_pixels{};
        REQUIRE(pbr->colorTexture(0)->readback(
            *device, 0, 0, {}, pbr_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(pbr_pixels[center + 0] > 0);
        CHECK(pbr_pixels[center + 1] > 0);
        CHECK(pbr_pixels[center + 2] > 0);
        CHECK(pbr_pixels[center + 3] == 0);
        CHECK(pbr_pixels[center + 0] != legacy_pixels[center + 0]);

        projector_scene.gbuffer = pbr_gbuffer.get();
        auto pbr_projector = renderer->renderProjectorLights(
            projector_scene, &error);
        INFO(error);
        REQUIRE(pbr_projector);
        std::array<std::uint8_t, 32 * 32 * 4>
            pbr_projector_pixels{};
        REQUIRE(pbr_projector->colorTexture(0)->readback(
            *device, 0, 0, {}, pbr_projector_pixels.data(),
            32 * 4, &error));
        INFO(error);
        CHECK(pbr_projector_pixels[center + 0] > 0);
        CHECK(pbr_projector_pixels[center + 1] > 0);
        CHECK(pbr_projector_pixels[center + 2] > 0);
        CHECK(pbr_projector_pixels[center + 3] == 0);
        CHECK(pbr_projector_pixels[center + 0] !=
              projected_pixels[center + 0]);
        projector_scene.gbuffer = gbuffer.get();

        // Fill only the clear-depth pixels of both forward- and reverse-Z
        // retained G-buffers with a full HDRI dome. A one-texel panorama
        // makes the expected transfer independent of direction while the
        // indexed strip still exercises the production geometry contract.
        const std::array<std::array<float, 4>, 4> sky_vertices{{
            {{-1.f, -1.f, 0.f, 1.f}},
            {{ 1.f, -1.f, 0.f, 1.f}},
            {{-1.f,  1.f, 0.f, 1.f}},
            {{ 1.f,  1.f, 0.f, 1.f}},
        }};
        const std::array<std::uint16_t, 4> sky_indices{{0, 1, 2, 3}};
        ALMetalBufferDescriptor sky_vertex_descriptor;
        sky_vertex_descriptor.length = sizeof(sky_vertices);
        sky_vertex_descriptor.storage = ALMetalStorageMode::shared;
        sky_vertex_descriptor.label = "Alchemy HDRI sky test vertices";
        auto sky_vertex_buffer = ALMetalBuffer::create(
            *device, sky_vertex_descriptor, &error);
        INFO(error);
        REQUIRE(sky_vertex_buffer);
        REQUIRE(sky_vertex_buffer->write(
            0, sky_vertices.data(), sizeof(sky_vertices), &error));
        ALMetalBufferDescriptor sky_index_descriptor;
        sky_index_descriptor.length = sizeof(sky_indices);
        sky_index_descriptor.storage = ALMetalStorageMode::shared;
        sky_index_descriptor.label = "Alchemy HDRI sky test indices";
        auto sky_index_buffer = ALMetalBuffer::create(
            *device, sky_index_descriptor, &error);
        INFO(error);
        REQUIRE(sky_index_buffer);
        REQUIRE(sky_index_buffer->write(
            0, sky_indices.data(), sizeof(sky_indices), &error));

        ALMetalTextureDescriptor hdri_descriptor;
        hdri_descriptor.format = ALMetalPixelFormat::rgba16_float;
        hdri_descriptor.width = 1;
        hdri_descriptor.height = 1;
        hdri_descriptor.storage = ALMetalStorageMode::private_gpu;
        hdri_descriptor.usage = ALMetalTextureUsage::shader_read;
        hdri_descriptor.label = "Alchemy full HDRI sky test panorama";
        auto hdri_texture = ALMetalTexture::create(
            *device, hdri_descriptor, &error);
        INFO(error);
        REQUIRE(hdri_texture);
        const std::array<std::uint16_t, 4> hdri_pixel{{
            0x3800, 0x3400, 0x3a00, 0x3c00,
        }}; // linear (0.5, 0.25, 0.75, 1.0)
        auto hdri_upload = device->makeCommandBuffer(
            "Alchemy full HDRI sky test upload", &error);
        INFO(error);
        REQUIRE(hdri_upload);
        REQUIRE(hdri_texture->upload(
            *hdri_upload, 0, 0, {}, hdri_pixel.data(), 8, &error));
        REQUIRE(hdri_upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldHDRISkyBatch sky_batch;
        sky_batch.vertices = sky_vertex_buffer.get();
        sky_batch.vertex_count = 4;
        sky_batch.indices = sky_index_buffer.get();
        sky_batch.index_count = 4;
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            sky_batch.modelview_projection[axis * 4 + axis] = 1.f;
        }
        ALMetalWorldHDRISkyScene sky_scene;
        sky_scene.batches = &sky_batch;
        sky_scene.batch_count = 1;
        sky_scene.environment_texture = hdri_texture.get();
        sky_scene.environment_rotation[0][0] = 1.f;
        sky_scene.environment_rotation[1][1] = 1.f;
        sky_scene.environment_rotation[2][2] = 1.f;
        sky_scene.exposure_scale = 1.f;
        sky_scene.width = 32;
        sky_scene.height = 32;
        sky_scene.normal_hdr = true;

        std::array<std::uint8_t, 32 * 32 * 4> before_sky_albedo{};
        REQUIRE(pbr_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, before_sky_albedo.data(), 32 * 4,
            &error));
        INFO(error);
        REQUIRE(renderer->compositeHDRISky(
            *pbr_gbuffer, sky_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> sky_albedo{};
        std::array<std::uint16_t, 32 * 32 * 4> sky_normal{};
        REQUIRE(pbr_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, sky_albedo.data(), 32 * 4, &error));
        REQUIRE(pbr_gbuffer->colorTexture(2)->readback(
            *device, 0, 0, {}, sky_normal.data(), 32 * 8, &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(sky_albedo[center + channel] ==
                  before_sky_albedo[center + channel]);
        }
        CHECK(std::abs(static_cast<int>(sky_albedo[outside + 0]) - 188)
              <= 1);
        CHECK(std::abs(static_cast<int>(sky_albedo[outside + 1]) - 137)
              <= 1);
        CHECK(std::abs(static_cast<int>(sky_albedo[outside + 2]) - 225)
              <= 1);
        CHECK(sky_albedo[outside + 3] == 255);
        CHECK(sky_normal[outside + 0] == 0);
        CHECK(sky_normal[outside + 1] == 0);
        CHECK(sky_normal[outside + 2] == 0);
        CHECK(sky_normal[outside + 3] == 65535);

        sky_scene.reverse_z = true;
        REQUIRE(renderer->compositeHDRISky(
            *pbr_reverse_gbuffer, sky_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> reverse_sky_albedo{};
        REQUIRE(pbr_reverse_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, reverse_sky_albedo.data(), 32 * 4,
            &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(reverse_sky_albedo[outside + channel] ==
                  sky_albedo[outside + channel]);
        }
        sky_scene.reverse_z = false;

        ALMetalWorldHDRISkyScene invalid_sky_scene = sky_scene;
        invalid_sky_scene.environment_texture = nullptr;
        error.clear();
        CHECK_FALSE(renderer->compositeHDRISky(
            *pbr_gbuffer, invalid_sky_scene, &error));
        CHECK(error == "Metal world HDRI sky scene is incomplete");
        error.clear();

        // Replace the same clear-depth pixels with skyF.glsl's non-HDRI
        // WindLight branch. Zero optic maps isolate the atmospheric terms;
        // the family-specific all-zero normal marker distinguishes this
        // display-encoded transfer from the linear HDRI transaction.
        auto rainbow = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy WindLight sky rainbow optic");
        auto halo = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy WindLight sky halo optic");
        const std::array<std::uint8_t, 4> zero_optic{{0, 0, 0, 0}};
        auto optic_upload = device->makeCommandBuffer(
            "Alchemy WindLight sky optic upload", &error);
        INFO(error);
        REQUIRE(optic_upload);
        REQUIRE(rainbow->upload(
            *optic_upload, 0, 0, {}, zero_optic.data(), 4, &error));
        REQUIRE(halo->upload(
            *optic_upload, 0, 0, {}, zero_optic.data(), 4, &error));
        REQUIRE(optic_upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldWindLightSkyScene windlight_scene;
        windlight_scene.batches = &sky_batch;
        windlight_scene.batch_count = 1;
        windlight_scene.rainbow_texture = rainbow.get();
        windlight_scene.halo_texture = halo.get();
        windlight_scene.constants.light_density[1] = 1.f;
        windlight_scene.constants.light_density[3] = 0.001f;
        windlight_scene.constants.blue_horizon[0] = 0.4f;
        windlight_scene.constants.blue_horizon[1] = 0.3f;
        windlight_scene.constants.blue_horizon[2] = 0.2f;
        windlight_scene.constants.blue_horizon[3] = 0.1f;
        windlight_scene.constants.glow[0] = 1.f;
        windlight_scene.constants.glow[2] = -1.f;
        windlight_scene.constants.glow[3] = 1000.f;
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            windlight_scene.constants.haze_extinction[channel] = 0.5f;
            windlight_scene.constants.blue_weight[channel] = 1.f;
            windlight_scene.constants.haze_weight[channel] = 1.f;
            windlight_scene.constants.sky_light[channel] = 0.5f;
            windlight_scene.constants.sky_ambient[channel] = 0.2f;
            windlight_scene.constants.sky_light_atten[channel] = 0.1f;
            windlight_scene.constants.sky_ambient_below[channel] = 0.1f;
        }
        windlight_scene.constants.sky_ambient_below[3] = 0.2f;
        windlight_scene.constants.optics[1] = 10.f;
        windlight_scene.sky_hdr_scale = 2.f;
        windlight_scene.width = 32;
        windlight_scene.height = 32;
        windlight_scene.normal_hdr = true;

        REQUIRE(renderer->compositeWindLightSky(
            *pbr_gbuffer, windlight_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> windlight_albedo{};
        std::array<std::uint16_t, 32 * 32 * 4> windlight_normal{};
        REQUIRE(pbr_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, windlight_albedo.data(), 32 * 4,
            &error));
        REQUIRE(pbr_gbuffer->colorTexture(2)->readback(
            *device, 0, 0, {}, windlight_normal.data(), 32 * 8,
            &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(windlight_albedo[center + channel] ==
                  before_sky_albedo[center + channel]);
        }
        CHECK(windlight_albedo[outside + 0] > 0);
        CHECK(windlight_albedo[outside + 1] > 0);
        CHECK(windlight_albedo[outside + 2] > 0);
        CHECK(windlight_albedo[outside + 3] == 255);
        CHECK(windlight_normal[outside + 0] == 0);
        CHECK(windlight_normal[outside + 1] == 0);
        CHECK(windlight_normal[outside + 2] == 0);
        CHECK(windlight_normal[outside + 3] == 0);

        // Replay one production-layout star quad over the atmospheric dome.
        // Rendering the same star over a black WindLight transaction isolates
        // its stored RGB contribution. The colored-dome result must then be
        // the raw encoded-byte sum (within one quantization step), not an sRGB
        // decode/blend/re-encode result.
        struct StarBufferData
        {
            std::array<std::array<float, 4>, 6> positions{};
            std::array<std::array<std::uint8_t, 4>, 6> colors{};
            std::array<std::array<float, 2>, 6> texcoords{};
        } star_data;
        const float star_coordinate =
            2.f * (2.5f / 32.f) - 1.f;
        const std::array<std::array<float, 2>, 6> star_corners{{
            {{-1.f, -1.f}}, {{ 1.f, -1.f}}, {{ 1.f,  1.f}},
            {{-1.f, -1.f}}, {{ 1.f,  1.f}}, {{-1.f,  1.f}},
        }};
        for (std::size_t vertex = 0; vertex < 6; ++vertex)
        {
            star_data.positions[vertex] = {
                star_coordinate, star_coordinate, 0.5f, 1.f,
            };
            star_data.colors[vertex] = {220, 180, 140, 128};
            star_data.texcoords[vertex] = star_corners[vertex];
        }
        ALMetalBufferDescriptor star_buffer_descriptor;
        star_buffer_descriptor.length = sizeof(star_data);
        star_buffer_descriptor.storage = ALMetalStorageMode::shared;
        star_buffer_descriptor.label =
            "Alchemy WindLight star production-layout vertices";
        auto star_buffer = ALMetalBuffer::create(
            *device, star_buffer_descriptor, &error);
        INFO(error);
        REQUIRE(star_buffer);
        REQUIRE(star_buffer->write(
            0, &star_data, sizeof(star_data), &error));

        ALMetalWorldStarBatch star_batch;
        star_batch.vertices = star_buffer.get();
        star_batch.position_offset = offsetof(
            StarBufferData, positions);
        star_batch.color_offset = offsetof(StarBufferData, colors);
        star_batch.texcoord_offset = offsetof(
            StarBufferData, texcoords);
        star_batch.vertex_count = 6;
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            star_batch.modelview_projection[axis * 4 + axis] = 1.f;
        }
        ALMetalWorldStarScene star_scene;
        star_scene.batches = &star_batch;
        star_scene.batch_count = 1;
        star_scene.star_alpha = 1.f;
        star_scene.time = 0.f;
        star_scene.width = 32;
        star_scene.height = 32;
        star_scene.normal_hdr = true;
        REQUIRE(renderer->compositeWindLightStars(
            *pbr_gbuffer, star_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> star_albedo{};
        std::array<std::uint16_t, 32 * 32 * 4> star_normal{};
        REQUIRE(pbr_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, star_albedo.data(), 32 * 4, &error));
        REQUIRE(pbr_gbuffer->colorTexture(2)->readback(
            *device, 0, 0, {}, star_normal.data(), 32 * 8, &error));
        INFO(error);
        CHECK(star_albedo[outside + 0] >
              windlight_albedo[outside + 0]);
        CHECK(star_albedo[outside + 1] >
              windlight_albedo[outside + 1]);
        CHECK(star_normal[outside + 0] == 0);
        CHECK(star_normal[outside + 1] == 65535);
        CHECK(star_normal[outside + 2] == 0);
        CHECK(star_normal[outside + 3] == 0);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(star_albedo[center + channel] ==
                  windlight_albedo[center + channel]);
        }

        auto star_source_gbuffer = renderer->render(
            gbuffer_scene, &error);
        INFO(error);
        REQUIRE(star_source_gbuffer);
        ALMetalWorldWindLightSkyScene black_windlight_scene =
            windlight_scene;
        std::fill(std::begin(
                      black_windlight_scene.constants.blue_horizon),
                  std::end(
                      black_windlight_scene.constants.blue_horizon),
                  0.f);
        REQUIRE(renderer->compositeWindLightSky(
            *star_source_gbuffer, black_windlight_scene, &error));
        REQUIRE(renderer->compositeWindLightStars(
            *star_source_gbuffer, star_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> star_source_albedo{};
        REQUIRE(star_source_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, star_source_albedo.data(),
            32 * 4, &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const int raw_sum = std::min(
                255, static_cast<int>(
                         windlight_albedo[outside + channel]) +
                     static_cast<int>(
                         star_source_albedo[outside + channel]));
            CHECK(std::abs(static_cast<int>(
                      star_albedo[outside + channel]) - raw_sum) <= 1);
        }

        // The live moon texture alpha suppresses the whole star quad when its
        // direction intersects the moon quad. Missing mask storage fails
        // before encoding rather than drawing through the moon.
        auto opaque_moon = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy WindLight star opaque moon mask");
        const std::array<std::uint8_t, 4> opaque_moon_pixel{{
            255, 255, 255, 255,
        }};
        auto moon_upload = device->makeCommandBuffer(
            "Alchemy WindLight star moon-mask upload", &error);
        INFO(error);
        REQUIRE(moon_upload);
        REQUIRE(opaque_moon->upload(
            *moon_upload, 0, 0, {}, opaque_moon_pixel.data(), 4,
            &error));
        REQUIRE(moon_upload->commitAndWait(&error));
        INFO(error);
        auto masked_star_gbuffer = renderer->render(
            gbuffer_scene, &error);
        INFO(error);
        REQUIRE(masked_star_gbuffer);
        REQUIRE(renderer->compositeWindLightSky(
            *masked_star_gbuffer, windlight_scene, &error));
        star_scene.moon_mask = true;
        star_scene.moon_texture = opaque_moon.get();
        const float star_length = std::sqrt(
            2.f * star_coordinate * star_coordinate + 0.25f);
        star_scene.moon_quad_center[0] =
            star_coordinate / star_length;
        star_scene.moon_quad_center[1] =
            star_coordinate / star_length;
        star_scene.moon_quad_center[2] = 0.5f / star_length;
        star_scene.moon_quad_u[0] = 1.f;
        star_scene.moon_quad_v[1] = 1.f;
        REQUIRE(renderer->compositeWindLightStars(
            *masked_star_gbuffer, star_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> masked_star_albedo{};
        REQUIRE(masked_star_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, masked_star_albedo.data(),
            32 * 4, &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(masked_star_albedo[outside + channel] ==
                  windlight_albedo[outside + channel]);
        }
        star_scene.moon_texture = nullptr;
        error.clear();
        CHECK_FALSE(renderer->compositeWindLightStars(
            *masked_star_gbuffer, star_scene, &error));
        CHECK(error ==
              "Metal world WindLight star scene is incomplete");
        error.clear();
        star_scene.moon_mask = false;
        std::fill(std::begin(star_scene.moon_quad_center),
                  std::end(star_scene.moon_quad_center), 0.f);
        std::fill(std::begin(star_scene.moon_quad_u),
                  std::end(star_scene.moon_quad_u), 0.f);
        std::fill(std::begin(star_scene.moon_quad_v),
                  std::end(star_scene.moon_quad_v), 0.f);

        windlight_scene.reverse_z = true;
        REQUIRE(renderer->compositeWindLightSky(
            *pbr_reverse_gbuffer, windlight_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4>
            reverse_windlight_albedo{};
        REQUIRE(pbr_reverse_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, reverse_windlight_albedo.data(),
            32 * 4, &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(reverse_windlight_albedo[outside + channel] ==
                  windlight_albedo[outside + channel]);
        }
        star_scene.reverse_z = true;
        REQUIRE(renderer->compositeWindLightStars(
            *pbr_reverse_gbuffer, star_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4>
            reverse_star_albedo{};
        REQUIRE(pbr_reverse_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, reverse_star_albedo.data(),
            32 * 4, &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(reverse_star_albedo[outside + channel] ==
                  star_albedo[outside + channel]);
        }
        star_scene.reverse_z = false;
        windlight_scene.reverse_z = false;

        // Source-over the indexed sun/moon quads after the stars. The sun's
        // two live environment textures blend before its alpha blends into
        // the raw display-encoded vintage storage, exactly as the GL sky pool
        // does with framebuffer sRGB disabled.
        struct HeavenlyBodyBufferData
        {
            std::array<std::array<float, 4>, 4> positions{};
            std::array<std::array<float, 2>, 4> texcoords{};
        } body_data;
        const float body_extent = 4.f / 32.f;
        body_data.positions = {{
            {{star_coordinate - body_extent,
              star_coordinate + body_extent, 0.5f, 1.f}},
            {{star_coordinate - body_extent,
              star_coordinate - body_extent, 0.5f, 1.f}},
            {{star_coordinate + body_extent,
              star_coordinate + body_extent, 0.5f, 1.f}},
            {{star_coordinate + body_extent,
              star_coordinate - body_extent, 0.5f, 1.f}},
        }};
        body_data.texcoords = {{
            {{0.f, 1.f}}, {{0.f, 0.f}},
            {{1.f, 1.f}}, {{1.f, 0.f}},
        }};
        const std::array<std::uint16_t, 6> body_indices{{
            0, 2, 1, 1, 2, 3,
        }};
        ALMetalBufferDescriptor body_vertex_descriptor;
        body_vertex_descriptor.length = sizeof(body_data);
        body_vertex_descriptor.storage = ALMetalStorageMode::shared;
        body_vertex_descriptor.label =
            "Alchemy WindLight heavenly-body production-layout vertices";
        auto body_vertex_buffer = ALMetalBuffer::create(
            *device, body_vertex_descriptor, &error);
        INFO(error);
        REQUIRE(body_vertex_buffer);
        REQUIRE(body_vertex_buffer->write(
            0, &body_data, sizeof(body_data), &error));
        ALMetalBufferDescriptor body_index_descriptor;
        body_index_descriptor.length = sizeof(body_indices);
        body_index_descriptor.storage = ALMetalStorageMode::shared;
        body_index_descriptor.label =
            "Alchemy WindLight heavenly-body production-layout indices";
        auto body_index_buffer = ALMetalBuffer::create(
            *device, body_index_descriptor, &error);
        INFO(error);
        REQUIRE(body_index_buffer);
        REQUIRE(body_index_buffer->write(
            0, body_indices.data(), sizeof(body_indices), &error));

        auto sun_a = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy WindLight sun texture A");
        auto sun_b = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy WindLight sun texture B");
        const std::array<std::uint8_t, 4> sun_a_pixel{{
            200, 40, 20, 128,
        }};
        const std::array<std::uint8_t, 4> sun_b_pixel{{
            40, 200, 100, 128,
        }};
        auto sun_upload = device->makeCommandBuffer(
            "Alchemy WindLight sun texture upload", &error);
        INFO(error);
        REQUIRE(sun_upload);
        REQUIRE(sun_a->upload(
            *sun_upload, 0, 0, {}, sun_a_pixel.data(), 4, &error));
        REQUIRE(sun_b->upload(
            *sun_upload, 0, 0, {}, sun_b_pixel.data(), 4, &error));
        REQUIRE(sun_upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldHeavenlyBodyBatch body_batch;
        body_batch.vertices = body_vertex_buffer.get();
        body_batch.position_offset = offsetof(
            HeavenlyBodyBufferData, positions);
        body_batch.texcoord_offset = offsetof(
            HeavenlyBodyBufferData, texcoords);
        body_batch.indices = body_index_buffer.get();
        body_batch.index_type = ALMetalIndexType::uint16;
        body_batch.vertex_count = 4;
        body_batch.index_count = 6;
        body_batch.texture_a = sun_a.get();
        body_batch.texture_b = sun_b.get();
        body_batch.parameters[0] = 0.25f;
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            body_batch.modelview_projection[axis * 4 + axis] = 1.f;
        }
        ALMetalWorldHeavenlyBodyScene body_scene;
        body_scene.batches = &body_batch;
        body_scene.batch_count = 1;
        body_scene.width = 32;
        body_scene.height = 32;
        body_scene.normal_hdr = true;

        REQUIRE(renderer->compositeWindLightHeavenlyBodies(
            *pbr_gbuffer, body_scene, &error));
        INFO(error);
        body_scene.reverse_z = true;
        REQUIRE(renderer->compositeWindLightHeavenlyBodies(
            *pbr_reverse_gbuffer, body_scene, &error));
        INFO(error);
        body_scene.reverse_z = false;
        std::array<std::uint8_t, 32 * 32 * 4> sun_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4> reverse_sun_albedo{};
        std::array<std::uint16_t, 32 * 32 * 4> sun_normal{};
        REQUIRE(pbr_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, sun_albedo.data(), 32 * 4, &error));
        REQUIRE(pbr_reverse_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, reverse_sun_albedo.data(),
            32 * 4, &error));
        REQUIRE(pbr_gbuffer->colorTexture(2)->readback(
            *device, 0, 0, {}, sun_normal.data(), 32 * 8, &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(reverse_sun_albedo[outside + channel] ==
                  sun_albedo[outside + channel]);
            CHECK(sun_albedo[center + channel] ==
                  star_albedo[center + channel]);
        }
        CHECK(sun_normal[outside + 0] == star_normal[outside + 0]);
        CHECK(sun_normal[outside + 1] == star_normal[outside + 1]);
        CHECK(sun_normal[outside + 2] == star_normal[outside + 2]);
        CHECK(sun_normal[outside + 3] == star_normal[outside + 3]);

        auto sun_source_gbuffer = renderer->render(
            gbuffer_scene, &error);
        INFO(error);
        REQUIRE(sun_source_gbuffer);
        REQUIRE(renderer->compositeWindLightSky(
            *sun_source_gbuffer, black_windlight_scene, &error));
        REQUIRE(renderer->compositeWindLightHeavenlyBodies(
            *sun_source_gbuffer, body_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> sun_source_albedo{};
        REQUIRE(sun_source_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, sun_source_albedo.data(),
            32 * 4, &error));
        INFO(error);
        const float sun_alpha = 128.f / 255.f;
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const int raw_source_over = std::min(
                255, static_cast<int>(std::lround(
                    static_cast<float>(
                        star_albedo[outside + channel]) *
                    (1.f - sun_alpha))) +
                    static_cast<int>(
                        sun_source_albedo[outside + channel]));
            CHECK(std::abs(static_cast<int>(
                      sun_albedo[outside + channel]) -
                  raw_source_over) <= 2);
        }

        // The moon uses only texture A, discards the transparent surround,
        // scales RGB by moon brightness and applies the legacy z-direction
        // horizon fade to alpha before source-over blending.
        auto moon_texture = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy WindLight moon texture");
        auto transparent_moon = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy WindLight transparent moon surround");
        const std::array<std::uint8_t, 4> moon_pixel{{
            160, 80, 40, 128,
        }};
        const std::array<std::uint8_t, 4> transparent_moon_pixel{{
            255, 255, 255, 2,
        }};
        auto body_upload = device->makeCommandBuffer(
            "Alchemy WindLight moon texture upload", &error);
        INFO(error);
        REQUIRE(body_upload);
        REQUIRE(moon_texture->upload(
            *body_upload, 0, 0, {}, moon_pixel.data(), 4, &error));
        REQUIRE(transparent_moon->upload(
            *body_upload, 0, 0, {}, transparent_moon_pixel.data(),
            4, &error));
        REQUIRE(body_upload->commitAndWait(&error));
        INFO(error);

        auto moon_gbuffer = renderer->render(gbuffer_scene, &error);
        INFO(error);
        REQUIRE(moon_gbuffer);
        REQUIRE(renderer->compositeWindLightSky(
            *moon_gbuffer, black_windlight_scene, &error));
        std::array<std::uint8_t, 32 * 32 * 4> moon_before{};
        REQUIRE(moon_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, moon_before.data(), 32 * 4, &error));
        body_batch.moon = true;
        body_batch.texture_a = transparent_moon.get();
        body_batch.texture_b = transparent_moon.get();
        body_batch.parameters[0] = 0.5f;
        body_batch.parameters[1] = 0.25f;
        REQUIRE(renderer->compositeWindLightHeavenlyBodies(
            *moon_gbuffer, body_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> transparent_moon_albedo{};
        REQUIRE(moon_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, transparent_moon_albedo.data(),
            32 * 4, &error));
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(transparent_moon_albedo[outside + channel] ==
                  moon_before[outside + channel]);
        }

        body_batch.texture_a = moon_texture.get();
        body_batch.texture_b = moon_texture.get();
        REQUIRE(renderer->compositeWindLightHeavenlyBodies(
            *moon_gbuffer, body_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> moon_albedo{};
        REQUIRE(moon_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, moon_albedo.data(), 32 * 4, &error));
        INFO(error);
        const float moon_fade = 0.25f * 0.25f * 4.f;
        const float moon_alpha = 128.f / 255.f * moon_fade;
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const int expected = static_cast<int>(std::lround(
                static_cast<float>(moon_pixel[channel]) * 0.5f *
                moon_alpha));
            CHECK(std::abs(static_cast<int>(
                      moon_albedo[outside + channel]) - expected) <= 1);
            CHECK(moon_albedo[center + channel] ==
                  moon_before[center + channel]);
        }

        body_batch.texture_a = nullptr;
        error.clear();
        CHECK_FALSE(renderer->compositeWindLightHeavenlyBodies(
            *moon_gbuffer, body_scene, &error));
        CHECK(error ==
              "Metal world WindLight heavenly-body batch is invalid");
        error.clear();
        body_batch.moon = false;
        body_batch.texture_a = sun_a.get();
        body_batch.texture_b = sun_b.get();
        body_batch.parameters[0] = 0.25f;
        body_batch.parameters[1] = 0.f;

        // Replay one production-layout meteor streak after the bodies. Its
        // CPU-premultiplied packed color is linearized in the vertex stage,
        // shaped along/across the streak and additively accumulated in the
        // same raw encoded storage convention as stars.
        struct MeteorBufferData
        {
            std::array<std::array<float, 4>, 6> positions{};
            std::array<std::array<float, 4>, 6> normals{};
            std::array<std::array<std::uint8_t, 4>, 6> colors{};
            std::array<std::array<float, 2>, 6> texcoords{};
        } meteor_data;
        const std::array<std::array<float, 2>, 6> meteor_coordinates{{
            {{-1.f, -1.f}}, {{ 1.f, -1.f}}, {{ 1.f,  1.f}},
            {{-1.f, -1.f}}, {{ 1.f,  1.f}}, {{-1.f,  1.f}},
        }};
        for (std::size_t vertex = 0; vertex < 6; ++vertex)
        {
            meteor_data.positions[vertex] = {
                star_coordinate, star_coordinate, 0.5f, 1.f,
            };
            meteor_data.normals[vertex] = {
                body_extent, 0.f, 0.f, 0.f,
            };
            meteor_data.colors[vertex] = {180, 120, 60, 64};
            meteor_data.texcoords[vertex] = meteor_coordinates[vertex];
        }
        ALMetalBufferDescriptor meteor_buffer_descriptor;
        meteor_buffer_descriptor.length = sizeof(meteor_data);
        meteor_buffer_descriptor.storage = ALMetalStorageMode::shared;
        meteor_buffer_descriptor.label =
            "Alchemy WindLight meteor production-layout vertices";
        auto meteor_buffer = ALMetalBuffer::create(
            *device, meteor_buffer_descriptor, &error);
        INFO(error);
        REQUIRE(meteor_buffer);
        REQUIRE(meteor_buffer->write(
            0, &meteor_data, sizeof(meteor_data), &error));
        ALMetalWorldMeteorBatch meteor_batch;
        meteor_batch.vertices = meteor_buffer.get();
        meteor_batch.position_offset = offsetof(
            MeteorBufferData, positions);
        meteor_batch.normal_offset = offsetof(
            MeteorBufferData, normals);
        meteor_batch.color_offset = offsetof(
            MeteorBufferData, colors);
        meteor_batch.texcoord_offset = offsetof(
            MeteorBufferData, texcoords);
        meteor_batch.vertex_count = 6;
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            meteor_batch.modelview_projection[axis * 4 + axis] = 1.f;
        }
        ALMetalWorldMeteorScene meteor_scene;
        meteor_scene.batches = &meteor_batch;
        meteor_scene.batch_count = 1;
        meteor_scene.width_pixels = 1.5f;
        meteor_scene.width = 32;
        meteor_scene.height = 32;
        meteor_scene.normal_hdr = true;

        REQUIRE(renderer->compositeWindLightMeteors(
            *pbr_gbuffer, meteor_scene, &error));
        INFO(error);
        meteor_scene.reverse_z = true;
        REQUIRE(renderer->compositeWindLightMeteors(
            *pbr_reverse_gbuffer, meteor_scene, &error));
        INFO(error);
        meteor_scene.reverse_z = false;
        std::array<std::uint8_t, 32 * 32 * 4> meteor_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4> reverse_meteor_albedo{};
        REQUIRE(pbr_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, meteor_albedo.data(), 32 * 4,
            &error));
        REQUIRE(pbr_reverse_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, reverse_meteor_albedo.data(),
            32 * 4, &error));
        INFO(error);
        CHECK(meteor_albedo[outside + 0] > sun_albedo[outside + 0]);
        CHECK(meteor_albedo[outside + 1] > sun_albedo[outside + 1]);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(reverse_meteor_albedo[outside + channel] ==
                  meteor_albedo[outside + channel]);
            CHECK(meteor_albedo[center + channel] ==
                  sun_albedo[center + channel]);
        }

        auto meteor_source_gbuffer = renderer->render(
            gbuffer_scene, &error);
        INFO(error);
        REQUIRE(meteor_source_gbuffer);
        REQUIRE(renderer->compositeWindLightSky(
            *meteor_source_gbuffer, black_windlight_scene, &error));
        REQUIRE(renderer->compositeWindLightMeteors(
            *meteor_source_gbuffer, meteor_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4>
            meteor_source_albedo{};
        REQUIRE(meteor_source_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, meteor_source_albedo.data(),
            32 * 4, &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const int raw_sum = std::min(
                255, static_cast<int>(sun_albedo[outside + channel]) +
                     static_cast<int>(
                         meteor_source_albedo[outside + channel]));
            CHECK(std::abs(static_cast<int>(
                      meteor_albedo[outside + channel]) - raw_sum) <= 1);
        }
        meteor_scene.width_pixels = 0.f;
        error.clear();
        CHECK_FALSE(renderer->compositeWindLightMeteors(
            *meteor_source_gbuffer, meteor_scene, &error));
        CHECK(error == "Metal world WindLight meteor scene is incomplete");
        error.clear();
        meteor_scene.width_pixels = 1.5f;

        // The procedural aurora reuses the indexed dome. Give its physical
        // fixture a positive local Z so screen pixels span elevation and
        // azimuth while the identity MVP still covers the full target.
        const std::array<std::array<float, 4>, 4> aurora_vertices{{
            {{-1.f, -1.f, 1.f, 1.f}},
            {{ 1.f, -1.f, 1.f, 1.f}},
            {{-1.f,  1.f, 1.f, 1.f}},
            {{ 1.f,  1.f, 1.f, 1.f}},
        }};
        ALMetalBufferDescriptor aurora_vertex_descriptor;
        aurora_vertex_descriptor.length = sizeof(aurora_vertices);
        aurora_vertex_descriptor.storage = ALMetalStorageMode::shared;
        aurora_vertex_descriptor.label =
            "Alchemy WindLight aurora dome vertices";
        auto aurora_vertex_buffer = ALMetalBuffer::create(
            *device, aurora_vertex_descriptor, &error);
        INFO(error);
        REQUIRE(aurora_vertex_buffer);
        REQUIRE(aurora_vertex_buffer->write(
            0, aurora_vertices.data(), sizeof(aurora_vertices), &error));
        ALMetalWorldHDRISkyBatch aurora_batch = sky_batch;
        aurora_batch.vertices = aurora_vertex_buffer.get();
        ALMetalWorldAuroraScene aurora_scene;
        aurora_scene.batches = &aurora_batch;
        aurora_scene.batch_count = 1;
        aurora_scene.intensity = 1.f;
        aurora_scene.time = 123.f;
        aurora_scene.width = 32;
        aurora_scene.height = 32;
        aurora_scene.normal_hdr = true;

        REQUIRE(renderer->compositeWindLightAurora(
            *pbr_gbuffer, aurora_scene, &error));
        INFO(error);
        aurora_scene.reverse_z = true;
        REQUIRE(renderer->compositeWindLightAurora(
            *pbr_reverse_gbuffer, aurora_scene, &error));
        INFO(error);
        aurora_scene.reverse_z = false;
        std::array<std::uint8_t, 32 * 32 * 4> aurora_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4>
            reverse_aurora_albedo{};
        REQUIRE(pbr_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, aurora_albedo.data(), 32 * 4,
            &error));
        REQUIRE(pbr_reverse_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, reverse_aurora_albedo.data(),
            32 * 4, &error));
        INFO(error);
        CHECK(reverse_aurora_albedo == aurora_albedo);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(aurora_albedo[center + channel] ==
                  meteor_albedo[center + channel]);
        }

        auto aurora_source_gbuffer = renderer->render(
            gbuffer_scene, &error);
        INFO(error);
        REQUIRE(aurora_source_gbuffer);
        REQUIRE(renderer->compositeWindLightSky(
            *aurora_source_gbuffer, black_windlight_scene, &error));
        std::array<std::uint8_t, 32 * 32 * 4>
            aurora_source_before{};
        REQUIRE(aurora_source_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, aurora_source_before.data(),
            32 * 4, &error));
        REQUIRE(renderer->compositeWindLightAurora(
            *aurora_source_gbuffer, aurora_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4>
            aurora_source_albedo{};
        REQUIRE(aurora_source_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, aurora_source_albedo.data(),
            32 * 4, &error));
        INFO(error);
        bool found_aurora = false;
        for (std::size_t pixel = 0; pixel < 32 * 32; ++pixel)
        {
            const std::size_t byte = pixel * 4;
            if (aurora_source_albedo[byte + 0] ==
                    aurora_source_before[byte + 0] &&
                aurora_source_albedo[byte + 1] ==
                    aurora_source_before[byte + 1] &&
                aurora_source_albedo[byte + 2] ==
                    aurora_source_before[byte + 2])
            {
                continue;
            }
            found_aurora = true;
            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                const int raw_sum = std::min(
                    255, static_cast<int>(meteor_albedo[byte + channel]) +
                         static_cast<int>(
                             aurora_source_albedo[byte + channel]) -
                         static_cast<int>(
                             aurora_source_before[byte + channel]));
                CHECK(std::abs(static_cast<int>(
                          aurora_albedo[byte + channel]) - raw_sum) <= 1);
            }
            break;
        }
        CHECK(found_aurora);
        aurora_scene.intensity = 0.f;
        error.clear();
        CHECK_FALSE(renderer->compositeWindLightAurora(
            *aurora_source_gbuffer, aurora_scene, &error));
        CHECK(error == "Metal world WindLight aurora scene is incomplete");
        error.clear();
        aurora_scene.intensity = 1.f;

        // Clouds are the final non-HDRI WindLight layer. Their retained dome
        // adds the production float2 UV plane and interpolates the two live
        // wrap-filtered noise textures before source-over composition.
        struct CloudBufferData
        {
            std::array<std::array<float, 4>, 4> positions{};
            std::array<std::array<float, 2>, 4> texcoords{};
        } cloud_data;
        cloud_data.positions = aurora_vertices;
        cloud_data.texcoords = {{
            {{0.f, 0.f}}, {{1.f, 0.f}},
            {{0.f, 1.f}}, {{1.f, 1.f}},
        }};
        ALMetalBufferDescriptor cloud_vertex_descriptor;
        cloud_vertex_descriptor.length = sizeof(cloud_data);
        cloud_vertex_descriptor.storage = ALMetalStorageMode::shared;
        cloud_vertex_descriptor.label =
            "Alchemy WindLight cloud production-layout vertices";
        auto cloud_vertex_buffer = ALMetalBuffer::create(
            *device, cloud_vertex_descriptor, &error);
        INFO(error);
        REQUIRE(cloud_vertex_buffer);
        REQUIRE(cloud_vertex_buffer->write(
            0, &cloud_data, sizeof(cloud_data), &error));

        auto cloud_noise_a = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy WindLight cloud noise A");
        auto cloud_noise_b = make_texture(
            ALMetalPixelFormat::rgba8_unorm,
            "Alchemy WindLight cloud noise B");
        const std::array<std::uint8_t, 4> cloud_noise_a_pixel{{
            204, 0, 0, 255,
        }};
        const std::array<std::uint8_t, 4> cloud_noise_b_pixel{{
            51, 0, 0, 255,
        }};
        auto cloud_upload = device->makeCommandBuffer(
            "Alchemy WindLight cloud noise upload", &error);
        INFO(error);
        REQUIRE(cloud_upload);
        REQUIRE(cloud_noise_a->upload(
            *cloud_upload, 0, 0, {}, cloud_noise_a_pixel.data(),
            4, &error));
        REQUIRE(cloud_noise_b->upload(
            *cloud_upload, 0, 0, {}, cloud_noise_b_pixel.data(),
            4, &error));
        REQUIRE(cloud_upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldCloudBatch cloud_batch;
        cloud_batch.vertices = cloud_vertex_buffer.get();
        cloud_batch.position_offset = offsetof(CloudBufferData, positions);
        cloud_batch.texcoord_offset = offsetof(CloudBufferData, texcoords);
        cloud_batch.indices = sky_index_buffer.get();
        cloud_batch.index_count = 4;
        cloud_batch.vertex_count = 4;
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            cloud_batch.modelview_projection[axis * 4 + axis] = 1.f;
        }
        ALMetalWorldCloudScene cloud_scene;
        cloud_scene.batches = &cloud_batch;
        cloud_scene.batch_count = 1;
        cloud_scene.noise_texture = cloud_noise_a.get();
        cloud_scene.next_noise_texture = cloud_noise_b.get();
        cloud_scene.constants.light_density[1] = 1.f;
        cloud_scene.constants.light_density[3] = 0.001f;
        cloud_scene.constants.blue_horizon[0] = 0.4f;
        cloud_scene.constants.blue_horizon[1] = 0.3f;
        cloud_scene.constants.blue_horizon[2] = 0.2f;
        cloud_scene.constants.blue_horizon[3] = 0.1f;
        cloud_scene.constants.glow[0] = 1.f;
        cloud_scene.constants.glow[2] = -1.f;
        cloud_scene.constants.glow[3] = 1000.f;
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            cloud_scene.constants.haze_extinction[channel] = 0.5f;
            cloud_scene.constants.blue_weight[channel] = 1.f;
            cloud_scene.constants.haze_weight[channel] = 1.f;
            cloud_scene.constants.sky_sunlight[channel] = 0.5f;
            cloud_scene.constants.atmos_ambient[channel] = 0.2f;
            cloud_scene.constants.sky_light_atten[channel] = 0.1f;
            cloud_scene.constants.cloud_sunlight[channel] = 0.4f;
            cloud_scene.constants.cloud_color_shadow[channel] =
                0.6f - static_cast<float>(channel) * 0.1f;
        }
        cloud_scene.constants.cloud_color_shadow[3] = 0.25f;
        cloud_scene.constants.cloud_pos_density1[2] = 0.2f;
        cloud_scene.constants.cloud_pos_density2[2] = 0.5f;
        cloud_scene.constants.parameters[0] = 1.f;
        cloud_scene.constants.parameters[1] = 0.f;
        cloud_scene.constants.parameters[2] = 0.25f;
        cloud_scene.constants.parameters[3] = 0.5f;
        cloud_scene.width = 32;
        cloud_scene.height = 32;
        cloud_scene.normal_hdr = true;

        REQUIRE(renderer->compositeWindLightClouds(
            *pbr_gbuffer, cloud_scene, &error));
        INFO(error);
        cloud_scene.reverse_z = true;
        REQUIRE(renderer->compositeWindLightClouds(
            *pbr_reverse_gbuffer, cloud_scene, &error));
        INFO(error);
        cloud_scene.reverse_z = false;
        std::array<std::uint8_t, 32 * 32 * 4> cloud_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4>
            reverse_cloud_albedo{};
        REQUIRE(pbr_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, cloud_albedo.data(), 32 * 4,
            &error));
        REQUIRE(pbr_reverse_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, reverse_cloud_albedo.data(),
            32 * 4, &error));
        INFO(error);
        CHECK(reverse_cloud_albedo == cloud_albedo);
        CHECK(cloud_albedo[outside + 0] !=
              aurora_albedo[outside + 0]);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(cloud_albedo[center + channel] ==
                  aurora_albedo[center + channel]);
        }

        auto cloud_source_gbuffer = renderer->render(
            gbuffer_scene, &error);
        INFO(error);
        REQUIRE(cloud_source_gbuffer);
        REQUIRE(renderer->compositeWindLightSky(
            *cloud_source_gbuffer, black_windlight_scene, &error));
        REQUIRE(renderer->compositeWindLightClouds(
            *cloud_source_gbuffer, cloud_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4>
            cloud_source_albedo{};
        REQUIRE(cloud_source_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, cloud_source_albedo.data(),
            32 * 4, &error));
        INFO(error);
        constexpr float cloud_alpha = 0.36399375f;
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const int expected = static_cast<int>(std::lround(
                static_cast<float>(aurora_albedo[outside + channel]) *
                (1.f - cloud_alpha))) +
                static_cast<int>(cloud_source_albedo[outside + channel]);
            CHECK(std::abs(static_cast<int>(
                      cloud_albedo[outside + channel]) - expected) <= 2);
        }

        auto cloud_a_gbuffer = renderer->render(gbuffer_scene, &error);
        auto cloud_b_gbuffer = renderer->render(gbuffer_scene, &error);
        INFO(error);
        REQUIRE(cloud_a_gbuffer);
        REQUIRE(cloud_b_gbuffer);
        REQUIRE(renderer->compositeWindLightSky(
            *cloud_a_gbuffer, black_windlight_scene, &error));
        REQUIRE(renderer->compositeWindLightSky(
            *cloud_b_gbuffer, black_windlight_scene, &error));
        cloud_scene.next_noise_texture = cloud_noise_a.get();
        cloud_scene.constants.parameters[2] = 0.f;
        REQUIRE(renderer->compositeWindLightClouds(
            *cloud_a_gbuffer, cloud_scene, &error));
        cloud_scene.noise_texture = cloud_noise_b.get();
        cloud_scene.next_noise_texture = cloud_noise_b.get();
        REQUIRE(renderer->compositeWindLightClouds(
            *cloud_b_gbuffer, cloud_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> cloud_a_albedo{};
        std::array<std::uint8_t, 32 * 32 * 4> cloud_b_albedo{};
        REQUIRE(cloud_a_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, cloud_a_albedo.data(), 32 * 4,
            &error));
        REQUIRE(cloud_b_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, cloud_b_albedo.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(cloud_a_albedo[outside + 0] >
              cloud_source_albedo[outside + 0]);
        CHECK(cloud_b_albedo[outside + 0] <
              cloud_source_albedo[outside + 0]);
        cloud_scene.noise_texture = cloud_noise_a.get();
        cloud_scene.next_noise_texture = cloud_noise_b.get();
        cloud_scene.constants.parameters[2] = 0.25f;

        ALMetalWorldCloudScene invalid_cloud_scene = cloud_scene;
        invalid_cloud_scene.noise_texture = nullptr;
        error.clear();
        CHECK_FALSE(renderer->compositeWindLightClouds(
            *cloud_source_gbuffer, invalid_cloud_scene, &error));
        CHECK(error == "Metal world WindLight cloud scene is incomplete");
        error.clear();

        ALMetalWorldWindLightSkyScene invalid_windlight_scene =
            windlight_scene;
        invalid_windlight_scene.halo_texture = nullptr;
        error.clear();
        CHECK_FALSE(renderer->compositeWindLightSky(
            *pbr_gbuffer, invalid_windlight_scene, &error));
        CHECK(error == "Metal world WindLight sky scene is incomplete");
        error.clear();

        // Partial HDRI is one fail-closed transaction: WindLight first owns
        // every clear-depth pixel, then HDRI overwrites exactly the left-hand
        // pixel centers selected by skyF.glsl's split inequality.
        auto split_gbuffer = renderer->render(gbuffer_scene, &error);
        INFO(error);
        REQUIRE(split_gbuffer);
        REQUIRE(renderer->compositeWindLightSky(
            *split_gbuffer, windlight_scene, &error));
        sky_scene.horizontal_fraction = 0.5f;
        REQUIRE(renderer->compositeHDRISky(
            *split_gbuffer, sky_scene, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> split_albedo{};
        std::array<std::uint16_t, 32 * 32 * 4> split_normal{};
        REQUIRE(split_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, split_albedo.data(), 32 * 4,
            &error));
        REQUIRE(split_gbuffer->colorTexture(2)->readback(
            *device, 0, 0, {}, split_normal.data(), 32 * 8,
            &error));
        INFO(error);
        const std::size_t split_left = (2 * 32 + 2) * 4;
        const std::size_t split_right = (2 * 32 + 29) * 4;
        const std::size_t split_last_hdri = (2 * 32 + 15) * 4;
        const std::size_t split_first_windlight = (2 * 32 + 16) * 4;
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(split_albedo[split_left + channel] ==
                  sky_albedo[split_left + channel]);
            CHECK(split_albedo[split_right + channel] ==
                  windlight_albedo[split_right + channel]);
            CHECK(split_albedo[center + channel] ==
                  before_sky_albedo[center + channel]);
        }
        CHECK(split_normal[split_left + 3] == 65535);
        CHECK(split_normal[split_right + 3] == 0);
        CHECK(split_normal[split_last_hdri + 3] == 65535);
        CHECK(split_normal[split_first_windlight + 3] == 0);

        batch.modelview_projection[10] = -1.f;
        gbuffer_scene.reverse_z = true;
        auto reverse_split_gbuffer = renderer->render(
            gbuffer_scene, &error);
        INFO(error);
        REQUIRE(reverse_split_gbuffer);
        windlight_scene.reverse_z = true;
        REQUIRE(renderer->compositeWindLightSky(
            *reverse_split_gbuffer, windlight_scene, &error));
        sky_scene.reverse_z = true;
        REQUIRE(renderer->compositeHDRISky(
            *reverse_split_gbuffer, sky_scene, &error));
        std::array<std::uint8_t, 32 * 32 * 4> reverse_split_albedo{};
        REQUIRE(reverse_split_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, reverse_split_albedo.data(),
            32 * 4, &error));
        INFO(error);
        CHECK(reverse_split_albedo == split_albedo);
        batch.modelview_projection[10] = 1.f;
        gbuffer_scene.reverse_z = false;
        windlight_scene.reverse_z = false;
        sky_scene.reverse_z = false;

        sky_scene.horizontal_fraction = 0.f;
        REQUIRE(renderer->compositeHDRISky(
            *split_gbuffer, sky_scene, &error));
        std::array<std::uint8_t, 32 * 32 * 4> zero_split_albedo{};
        REQUIRE(split_gbuffer->colorTexture(0)->readback(
            *device, 0, 0, {}, zero_split_albedo.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(zero_split_albedo == split_albedo);

        sky_scene.horizontal_fraction = 1.1f;
        error.clear();
        CHECK_FALSE(renderer->compositeHDRISky(
            *split_gbuffer, sky_scene, &error));
        CHECK(error == "Metal world HDRI sky scene is incomplete");
        error.clear();
        sky_scene.horizontal_fraction = 1.f;

        // Restore the HDRI family before the existing light-map and base
        // checks so each transfer is independently identified by its marker.
        REQUIRE(renderer->compositeHDRISky(
            *pbr_gbuffer, sky_scene, &error));
        sky_scene.reverse_z = true;
        REQUIRE(renderer->compositeHDRISky(
            *pbr_reverse_gbuffer, sky_scene, &error));
        sky_scene.reverse_z = false;
        INFO(error);

        // The retained native G-buffer now owns the two-channel deferred
        // light map as well. With no shadows or SSAO both geometry and the
        // cleared sky are fully lit. Enabling the directional owner with no
        // built cascades still applies the exact shading-normal facing cut.
        ALMetalWorldLightMapScene light_map_scene;
        light_map_scene.gbuffer = pbr_gbuffer.get();
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            light_map_scene.constants
                .inverse_projection[axis * 4 + axis] = 1.f;
        }
        light_map_scene.constants.inverse_projection[14] = -1.f;
        light_map_scene.constants.light_direction[2] = 1.f;
        light_map_scene.constants.screen[0] = 32.f;
        light_map_scene.constants.screen[1] = 32.f;
        light_map_scene.constants.config[3] = 1;
        auto native_light_map = renderer->renderLightMap(
            light_map_scene, &error);
        INFO(error);
        REQUIRE(native_light_map);
        std::array<std::uint8_t, 32 * 32 * 2> native_light_pixels{};
        REQUIRE(native_light_map->colorTexture(0)->readback(
            *device, 0, 0, {}, native_light_pixels.data(), 32 * 2,
            &error));
        INFO(error);
        const std::size_t light_center = (16 * 32 + 16) * 2;
        const std::size_t light_outside = (2 * 32 + 2) * 2;
        CHECK(native_light_pixels[light_center + 0] == 255);
        CHECK(native_light_pixels[light_center + 1] == 255);
        CHECK(native_light_pixels[light_outside + 0] == 255);
        CHECK(native_light_pixels[light_outside + 1] == 255);

        light_map_scene.constants.config[1] = 1;
        light_map_scene.constants.light_direction[2] = -1.f;
        auto back_facing_light_map = renderer->renderLightMap(
            light_map_scene, &error);
        INFO(error);
        REQUIRE(back_facing_light_map);
        std::array<std::uint8_t, 32 * 32 * 2> back_facing_pixels{};
        REQUIRE(back_facing_light_map->colorTexture(0)->readback(
            *device, 0, 0, {}, back_facing_pixels.data(), 32 * 2,
            &error));
        INFO(error);
        CHECK(back_facing_pixels[light_center + 0] == 0);
        CHECK(back_facing_pixels[light_center + 1] == 255);
        CHECK(back_facing_pixels[light_outside + 0] == 255);

        // A constant raw-depth cascade maps the receiver to depth 0.5. Its
        // stored 0.25 is shadowing under forward-Z, while declaring the same
        // cascade unbuilt restores fully lit without sampling it.
        ALMetalTextureDescriptor world_shadow_descriptor;
        world_shadow_descriptor.type =
            ALMetalTextureType::texture_2d_array;
        world_shadow_descriptor.format = ALMetalPixelFormat::r32_float;
        world_shadow_descriptor.width = 8;
        world_shadow_descriptor.height = 8;
        world_shadow_descriptor.array_length = 4;
        world_shadow_descriptor.storage = ALMetalStorageMode::private_gpu;
        world_shadow_descriptor.usage = ALMetalTextureUsage::shader_read;
        world_shadow_descriptor.label =
            "Alchemy world light-map directional-shadow test";
        auto world_shadow_texture = ALMetalTexture::create(
            *device, world_shadow_descriptor, &error);
        INFO(error);
        REQUIRE(world_shadow_texture);
        const std::array<float, 8 * 8> shadowed_depth = []
        {
            std::array<float, 8 * 8> value{};
            value.fill(0.25f);
            return value;
        }();
        const std::array<float, 8 * 8> fully_lit_depth = []
        {
            std::array<float, 8 * 8> value{};
            value.fill(1.f);
            return value;
        }();
        auto world_shadow_upload = device->makeCommandBuffer(
            "Alchemy world light-map directional-shadow upload", &error);
        INFO(error);
        REQUIRE(world_shadow_upload);
        for (std::uint32_t cascade = 0; cascade < 4; ++cascade)
        {
            const auto& values = cascade == 0
                ? shadowed_depth : fully_lit_depth;
            REQUIRE(world_shadow_texture->upload(
                *world_shadow_upload, 0, cascade, {}, values.data(),
                8 * sizeof(float), &error));
        }
        REQUIRE(world_shadow_upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldDirectionalShadows world_shadows;
        world_shadows.depth_texture = world_shadow_texture.get();
        for (std::size_t cascade = 0; cascade < 4; ++cascade)
        {
            float* matrix = world_shadows.constants
                                .shadow_matrix[cascade];
            matrix[12] = 0.5f;
            matrix[13] = 0.5f;
            matrix[14] = 0.5f;
            matrix[15] = 1.f;
        }
        world_shadows.constants.shadow_clip[0] = 4.f;
        world_shadows.constants.shadow_clip[1] = 8.f;
        world_shadows.constants.shadow_clip[2] = 12.f;
        world_shadows.constants.shadow_clip[3] = 16.f;
        world_shadows.constants.resolution[0] = 8.f;
        world_shadows.constants.resolution[1] = 8.f;
        world_shadows.constants.resolution[2] = 32.f;
        world_shadows.constants.resolution[3] = 32.f;
        world_shadows.constants.parameters[2] = 200.f;
        world_shadows.constants.parameters[3] = 32.f;
        world_shadows.constants.config[0] = 4;
        world_shadows.constants.config[1] = 0xeu;
        world_shadows.constants.config[3] = 1;
        light_map_scene.directional_shadows = &world_shadows;
        light_map_scene.constants.light_direction[2] = 1.f;
        auto shadowed_light_map = renderer->renderLightMap(
            light_map_scene, &error);
        INFO(error);
        REQUIRE(shadowed_light_map);
        std::array<std::uint8_t, 32 * 32 * 2> shadowed_light_pixels{};
        REQUIRE(shadowed_light_map->colorTexture(0)->readback(
            *device, 0, 0, {}, shadowed_light_pixels.data(), 32 * 2,
            &error));
        INFO(error);
        CHECK(shadowed_light_pixels[light_center + 0] == 0);
        CHECK(shadowed_light_pixels[light_center + 1] == 255);
        world_shadows.constants.config[1] = 0xfu;
        auto unbuilt_light_map = renderer->renderLightMap(
            light_map_scene, &error);
        INFO(error);
        REQUIRE(unbuilt_light_map);
        std::array<std::uint8_t, 32 * 32 * 2> unbuilt_light_pixels{};
        REQUIRE(unbuilt_light_map->colorTexture(0)->readback(
            *device, 0, 0, {}, unbuilt_light_pixels.data(), 32 * 2,
            &error));
        INFO(error);
        CHECK(unbuilt_light_pixels[light_center + 0] == 255);
        light_map_scene.directional_shadows = nullptr;
        light_map_scene.constants.config[1] = 0;

        light_map_scene.gbuffer = pbr_reverse_gbuffer.get();
        light_map_scene.constants.inverse_projection[10] = -1.f;
        light_map_scene.constants.config[0] = 1;
        auto reverse_light_map = renderer->renderLightMap(
            light_map_scene, &error);
        INFO(error);
        REQUIRE(reverse_light_map);
        std::array<std::uint8_t, 32 * 32 * 2> reverse_light_pixels{};
        REQUIRE(reverse_light_map->colorTexture(0)->readback(
            *device, 0, 0, {}, reverse_light_pixels.data(), 32 * 2,
            &error));
        INFO(error);
        CHECK(reverse_light_pixels[light_center + 0] ==
              native_light_pixels[light_center + 0]);
        CHECK(reverse_light_pixels[light_center + 1] ==
              native_light_pixels[light_center + 1]);
        light_map_scene.gbuffer = pbr_gbuffer.get();
        light_map_scene.constants.inverse_projection[10] = 1.f;
        light_map_scene.constants.config[0] = 0;

        // SSAO consumes the exact 128-square reflected-noise contract and
        // applies the same twelve-tap bilateral blur on each axis. A stepped
        // retained G-buffer must contain occlusion, and a non-zero blur must
        // redistribute at least one occluded texel without touching R.
        ALMetalTextureDescriptor noise_descriptor;
        noise_descriptor.format = ALMetalPixelFormat::rg32_float;
        noise_descriptor.width = 128;
        noise_descriptor.height = 128;
        noise_descriptor.storage = ALMetalStorageMode::private_gpu;
        noise_descriptor.usage = ALMetalTextureUsage::shader_read;
        noise_descriptor.label = "Alchemy world SSAO reflected noise";
        auto noise_texture = ALMetalTexture::create(
            *device, noise_descriptor, &error);
        INFO(error);
        REQUIRE(noise_texture);
        std::array<float, 128 * 128 * 2> noise_values{};
        for (std::size_t texel = 0; texel < 128 * 128; ++texel)
        {
            constexpr float directions[4][2] = {
                {1.f, 0.f}, {0.f, 1.f},
                {-1.f, 0.f}, {0.f, -1.f},
            };
            noise_values[texel * 2 + 0] =
                directions[texel & 3u][0];
            noise_values[texel * 2 + 1] =
                directions[texel & 3u][1];
        }
        auto noise_upload = device->makeCommandBuffer(
            "Alchemy world SSAO reflected-noise upload", &error);
        INFO(error);
        REQUIRE(noise_upload);
        REQUIRE(noise_texture->upload(
            *noise_upload, 0, 0, {}, noise_values.data(),
            128 * 2 * sizeof(float), &error));
        REQUIRE(noise_upload->commitAndWait(&error));
        INFO(error);

        std::array<ALMetalWorldSimpleBatch, 2> occlusion_batches{{
            batch, batch,
        }};
        occlusion_batches[0].impostor_left[0] = 0.9f;
        occlusion_batches[0].impostor_up[1] = 0.9f;
        occlusion_batches[1].impostor_center[0] = 0.25f;
        occlusion_batches[1].impostor_center[2] = -0.9f;
        occlusion_batches[1].impostor_left[0] = 0.2f;
        occlusion_batches[1].impostor_up[1] = 0.6f;
        ALMetalWorldGBufferScene occlusion_scene = gbuffer_scene;
        occlusion_scene.batches = occlusion_batches.data();
        occlusion_scene.batch_count = occlusion_batches.size();
        auto occlusion_gbuffer = renderer->render(
            occlusion_scene, &error);
        INFO(error);
        REQUIRE(occlusion_gbuffer);

        light_map_scene.gbuffer = occlusion_gbuffer.get();
        light_map_scene.noise_texture = noise_texture.get();
        light_map_scene.constants.ssao[0] = 96.f;
        light_map_scene.constants.ssao[1] = 96.f;
        light_map_scene.constants.ssao[2] = 4.f;
        light_map_scene.constants.ssao[3] = 0.25f;
        for (std::size_t tap = 0; tap < 4; ++tap)
        {
            const float offset = static_cast<float>(tap);
            const float weight = std::exp(
                -(offset * offset) / 8.f);
            light_map_scene.constants.blur_taps[tap][0] = weight;
            light_map_scene.constants.blur_taps[tap][1] = weight;
            light_map_scene.constants.blur_taps[tap][2] = offset;
        }
        light_map_scene.constants.blur[1] = 0.1f;
        light_map_scene.constants.config[2] = 1;
        auto unblurred_ssao = renderer->renderLightMap(
            light_map_scene, &error);
        INFO(error);
        REQUIRE(unblurred_ssao);
        std::array<std::uint8_t, 32 * 32 * 2> unblurred_ssao_pixels{};
        REQUIRE(unblurred_ssao->colorTexture(0)->readback(
            *device, 0, 0, {}, unblurred_ssao_pixels.data(), 32 * 2,
            &error));
        INFO(error);
        light_map_scene.constants.blur[0] = 1.5f;
        auto blurred_ssao = renderer->renderLightMap(
            light_map_scene, &error);
        INFO(error);
        REQUIRE(blurred_ssao);
        std::array<std::uint8_t, 32 * 32 * 2> blurred_ssao_pixels{};
        REQUIRE(blurred_ssao->colorTexture(0)->readback(
            *device, 0, 0, {}, blurred_ssao_pixels.data(), 32 * 2,
            &error));
        INFO(error);
        bool found_occlusion = false;
        bool found_blur_change = false;
        std::uint8_t minimum_unblurred = 255;
        std::uint8_t minimum_blurred = 255;
        std::size_t changed_pixels = 0;
        for (std::size_t pixel = 0; pixel < 32 * 32; ++pixel)
        {
            CHECK(blurred_ssao_pixels[pixel * 2 + 0] ==
                  unblurred_ssao_pixels[pixel * 2 + 0]);
            found_occlusion = found_occlusion ||
                unblurred_ssao_pixels[pixel * 2 + 1] < 255;
            minimum_unblurred = std::min(
                minimum_unblurred,
                unblurred_ssao_pixels[pixel * 2 + 1]);
            minimum_blurred = std::min(
                minimum_blurred,
                blurred_ssao_pixels[pixel * 2 + 1]);
            if (blurred_ssao_pixels[pixel * 2 + 1] !=
                unblurred_ssao_pixels[pixel * 2 + 1])
            {
                found_blur_change = true;
                ++changed_pixels;
            }
        }
        CAPTURE(minimum_unblurred, minimum_blurred, changed_pixels);
        CHECK(found_occlusion);
        CHECK(found_blur_change);

        ALMetalWorldLightMapScene invalid_light_map_scene;
        error.clear();
        CHECK_FALSE(renderer->renderLightMap(
            invalid_light_map_scene, &error));
        CHECK(error == "Metal world light-map scene is incomplete");
        error.clear();

        // Consume the same retained PBR G-buffer through the base deferred
        // owner. One constant ordinary probe proves that its packed SH row
        // and roughness-selected radiance cube are both live inputs; the
        // borrowed RG light map then proves direct shadow and SSAO response.
        ALMetalTextureDescriptor radiance_descriptor;
        radiance_descriptor.type = ALMetalTextureType::texture_cube_array;
        radiance_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        radiance_descriptor.width = 1;
        radiance_descriptor.height = 1;
        radiance_descriptor.array_length = 1;
        radiance_descriptor.storage = ALMetalStorageMode::private_gpu;
        radiance_descriptor.usage = ALMetalTextureUsage::shader_read;
        radiance_descriptor.label = "Alchemy base-light probe radiance";
        auto radiance = ALMetalTexture::create(
            *device, radiance_descriptor, &error);
        INFO(error);
        REQUIRE(radiance);

        ALMetalTextureDescriptor irradiance_descriptor;
        irradiance_descriptor.format = ALMetalPixelFormat::rgba32_uint;
        irradiance_descriptor.width = 4;
        irradiance_descriptor.height = 1;
        irradiance_descriptor.storage = ALMetalStorageMode::private_gpu;
        irradiance_descriptor.usage = ALMetalTextureUsage::shader_read;
        irradiance_descriptor.label = "Alchemy base-light packed SH row";
        auto irradiance = ALMetalTexture::create(
            *device, irradiance_descriptor, &error);
        INFO(error);
        REQUIRE(irradiance);

        ALMetalTextureDescriptor light_map_descriptor;
        light_map_descriptor.format = ALMetalPixelFormat::rg8_unorm;
        light_map_descriptor.width = 32;
        light_map_descriptor.height = 32;
        light_map_descriptor.storage = ALMetalStorageMode::private_gpu;
        light_map_descriptor.usage = ALMetalTextureUsage::shader_read;
        light_map_descriptor.label = "Alchemy base-light borrowed light map";
        auto light_map = ALMetalTexture::create(
            *device, light_map_descriptor, &error);
        INFO(error);
        REQUIRE(light_map);

        const std::array<std::uint8_t, 4> green_radiance{{
            0, 128, 0, 255,
        }};
        std::array<std::uint32_t, 16> packed_sh{};
        packed_sh[0] = 0x3c003c00u;
        packed_sh[1] = 0x00003c00u;
        std::array<std::uint8_t, 32 * 32 * 2> full_light_map{};
        for (std::size_t pixel = 0; pixel < 32 * 32; ++pixel)
        {
            full_light_map[pixel * 2 + 0] = 255;
            full_light_map[pixel * 2 + 1] = 255;
        }
        auto base_upload = device->makeCommandBuffer(
            "Alchemy base-light inputs upload", &error);
        INFO(error);
        REQUIRE(base_upload);
        for (std::uint32_t face = 0; face < 6; ++face)
        {
            REQUIRE(radiance->upload(
                *base_upload, 0, face, {}, green_radiance.data(), 4,
                &error));
        }
        REQUIRE(irradiance->upload(
            *base_upload, 0, 0, {}, packed_sh.data(), 4 * 16, &error));
        REQUIRE(light_map->upload(
            *base_upload, 0, 0, {}, full_light_map.data(), 32 * 2,
            &error));
        REQUIRE(base_upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldReflectionProbes base_probes;
        base_probes.radiance_texture = radiance.get();
        base_probes.irradiance_texture = irradiance.get();
        base_probes.constants.ref_sphere[0][3] = 100.f;
        base_probes.constants.ref_params[0][0] = 1.f;
        base_probes.constants.ref_params[0][1] = 1.f;
        base_probes.constants.ref_params[0][2] = 1.f;
        base_probes.constants.ref_index[0][0] = 0;
        base_probes.constants.ref_index[0][1] = -1;
        base_probes.constants.env_matrix[0][0] = 1.f;
        base_probes.constants.env_matrix[1][1] = 1.f;
        base_probes.constants.env_matrix[2][2] = 1.f;
        base_probes.constants.config[0] = 1.f;
        base_probes.constants.config[1] = 0.f;

        ALMetalWorldBaseLightingScene base_scene;
        base_scene.gbuffer = pbr_gbuffer.get();
        base_scene.light_map = light_map.get();
        base_scene.reflection_probes = &base_probes;
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            base_scene.constants.inverse_projection[axis * 4 + axis] = 1.f;
        }
        base_scene.constants.light_direction[2] = 1.f;
        base_scene.constants.atmos_sunlit_linear[0] = 1.f;
        base_scene.constants.atmos_sunlit_linear[1] = 1.f;
        base_scene.constants.atmos_sunlit_linear[2] = 1.f;
        base_scene.constants.lighting[0] = 1.f;
        base_scene.constants.lighting[1] = 1.f;
        base_scene.constants.lighting[2] = 0.25f;
        base_scene.constants.lighting[3] = 0.25f;
        base_scene.constants.surface[0] = 128.f;
        base_scene.constants.surface[1] = 1.f;
        base_scene.constants.ssao_effect[0][0] = 1.f;
        base_scene.constants.ssao_effect[1][1] = 1.f;
        base_scene.constants.ssao_effect[2][2] = 1.f;
        base_scene.constants.config[2] = 1;

        auto base_lit = renderer->renderBaseLighting(base_scene, &error);
        INFO(error);
        REQUIRE(base_lit);
        std::array<std::uint8_t, 32 * 32 * 4> base_pixels{};
        REQUIRE(base_lit->colorTexture(0)->readback(
            *device, 0, 0, {}, base_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(base_pixels[center + 0] > 0);
        CHECK(base_pixels[center + 1] > 0);
        CHECK(base_pixels[center + 2] > 0);
        CHECK(base_pixels[center + 3] == 0);
        CHECK(base_pixels[outside + 0] == 0);
        CHECK(base_pixels[outside + 1] == 0);
        CHECK(base_pixels[outside + 2] == 0);

        // GL writes this base result into screen first, then adds ordinary
        // and projector lights with ONE/ONE blending. The native owner keeps
        // that boundary separate from post-deferred geometry and output
        // transforms while reproducing the exact three-target sum.
        ALMetalWorldLightingCompositionScene composition_scene;
        composition_scene.base_lighting = base_lit.get();
        composition_scene.local_lighting = legacy.get();
        composition_scene.projector_lighting = projected.get();
        auto composed = renderer->composeLighting(
            composition_scene, &error);
        INFO(error);
        REQUIRE(composed);
        REQUIRE(composed->descriptor().colors[0].format ==
                ALMetalPixelFormat::rgba8_unorm);
        std::array<std::uint8_t, 32 * 32 * 4> composed_pixels{};
        REQUIRE(composed->colorTexture(0)->readback(
            *device, 0, 0, {}, composed_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const int expected = std::min(
                static_cast<int>(base_pixels[center + channel]) +
                    static_cast<int>(legacy_pixels[center + channel]) +
                    static_cast<int>(projected_pixels[center + channel]),
                255);
            CHECK(std::abs(static_cast<int>(
                      composed_pixels[center + channel]) - expected) <= 1);
            CHECK(composed_pixels[outside + channel] == 0);
        }
        CHECK(composed_pixels[center + 3] == 0);
        CHECK(composed_pixels[outside + 3] == 0);

        // Merely seeing clear depth and a normal alpha of one is not enough:
        // the base owner above discarded the sky until this explicit native
        // transaction bit is set. Once set, the sRGB attachment decodes the
        // vintage HDRI exactly once and transfers its linear color unlit.
        base_scene.constants.sky[0] = 1;
        auto base_with_hdri = renderer->renderBaseLighting(
            base_scene, &error);
        INFO(error);
        REQUIRE(base_with_hdri);
        std::array<std::uint8_t, 32 * 32 * 4> base_hdri_pixels{};
        REQUIRE(base_with_hdri->colorTexture(0)->readback(
            *device, 0, 0, {}, base_hdri_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                  base_hdri_pixels[outside + 0]) - 128) <= 1);
        CHECK(std::abs(static_cast<int>(
                  base_hdri_pixels[outside + 1]) - 64) <= 1);
        CHECK(std::abs(static_cast<int>(
                  base_hdri_pixels[outside + 2]) - 191) <= 1);
        CHECK(base_hdri_pixels[outside + 3] == 0);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(base_hdri_pixels[center + channel] ==
                  base_pixels[center + channel]);
        }

        // Layer the stars again before base transfer. Their additive green
        // normal channel is the reference SKIP_ATMOS value while red, blue
        // and alpha retain the WindLight family marker.
        REQUIRE(renderer->compositeWindLightSky(
            *pbr_gbuffer, windlight_scene, &error));
        REQUIRE(renderer->compositeWindLightStars(
            *pbr_gbuffer, star_scene, &error));
        INFO(error);
        base_scene.constants.sky[2] = 1;
        base_scene.constants.sky_transfer[0] = 2.f;
        auto base_with_windlight = renderer->renderBaseLighting(
            base_scene, &error);
        INFO(error);
        REQUIRE(base_with_windlight);
        std::array<std::uint8_t, 32 * 32 * 4>
            base_windlight_pixels{};
        REQUIRE(base_with_windlight->colorTexture(0)->readback(
            *device, 0, 0, {}, base_windlight_pixels.data(),
            32 * 4, &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const float expected_linear = std::clamp(
                srgb_to_linear(
                    star_albedo[outside + channel] / 255.f) * 2.f,
                0.f, 1.f);
            const int expected = static_cast<int>(std::lround(
                expected_linear * 255.f));
            CHECK(std::abs(static_cast<int>(
                      base_windlight_pixels[outside + channel]) -
                  expected) <= 1);
            CHECK(base_windlight_pixels[center + channel] ==
                  base_pixels[center + channel]);
        }
        CHECK(base_windlight_pixels[outside + 3] == 0);

        // Family 2 decodes the partial split per pixel: the HDRI marker stays
        // linear and unscaled, while the WindLight marker receives the exact
        // display decode and sky_hdr_scale.
        base_scene.gbuffer = split_gbuffer.get();
        base_scene.constants.sky[2] = 2;
        auto base_with_split = renderer->renderBaseLighting(
            base_scene, &error);
        INFO(error);
        REQUIRE(base_with_split);
        std::array<std::uint8_t, 32 * 32 * 4> base_split_pixels{};
        REQUIRE(base_with_split->colorTexture(0)->readback(
            *device, 0, 0, {}, base_split_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
                  base_split_pixels[split_left + 0]) - 128) <= 1);
        CHECK(std::abs(static_cast<int>(
                  base_split_pixels[split_left + 1]) - 64) <= 1);
        CHECK(std::abs(static_cast<int>(
                  base_split_pixels[split_left + 2]) - 191) <= 1);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const float expected_linear = std::clamp(
                srgb_to_linear(
                    windlight_albedo[split_right + channel] / 255.f) *
                    2.f,
                0.f, 1.f);
            const int expected = static_cast<int>(std::lround(
                expected_linear * 255.f));
            CHECK(std::abs(static_cast<int>(
                      base_split_pixels[split_right + channel]) -
                  expected) <= 1);
            CHECK(base_split_pixels[center + channel] ==
                  base_pixels[center + channel]);
        }
        CHECK(base_split_pixels[split_left + 3] == 0);
        CHECK(base_split_pixels[split_right + 3] == 0);
        base_scene.gbuffer = pbr_gbuffer.get();

        REQUIRE(renderer->compositeHDRISky(
            *pbr_gbuffer, sky_scene, &error));
        INFO(error);
        base_scene.constants.sky[0] = 0;
        base_scene.constants.sky[2] = 0;
        base_scene.constants.sky_transfer[0] = 1.f;

        // Finish the ordinary probe result the same way as the deferred GLSL
        // owner. A 0.8-gloss PBR surface reads the hero probe at LOD 0.8:
        // its two-level red-to-blue cube therefore raises blue over the
        // ordinary green probe. The declared heroMaxLod also proves that an
        // incomplete glossiest-quarter mip family fails before encoding.
        const std::array<std::uint8_t, 4> glossy_pbr_orm{{
            255, 51, 255, 0,
        }};
        const std::array<std::uint16_t, 4> glossy_pbr_normal{{
            32768, 32768, 32896, 43908,
        }};
        auto glossy_upload = device->makeCommandBuffer(
            "Alchemy glossy base-light G-buffer upload", &error);
        INFO(error);
        REQUIRE(glossy_upload);
        REQUIRE(specular->upload(
            *glossy_upload, 0, 0, {}, glossy_pbr_orm.data(), 4,
            &error));
        REQUIRE(normal->upload(
            *glossy_upload, 0, 0, {}, glossy_pbr_normal.data(), 8,
            &error));
        REQUIRE(glossy_upload->commitAndWait(&error));
        INFO(error);
        auto glossy_gbuffer = renderer->render(gbuffer_scene, &error);
        INFO(error);
        REQUIRE(glossy_gbuffer);

        auto restore_pbr_upload = device->makeCommandBuffer(
            "Alchemy restore PBR base-light G-buffer upload", &error);
        INFO(error);
        REQUIRE(restore_pbr_upload);
        REQUIRE(specular->upload(
            *restore_pbr_upload, 0, 0, {}, pbr_orm.data(), 4, &error));
        REQUIRE(normal->upload(
            *restore_pbr_upload, 0, 0, {}, pbr_normal.data(), 8,
            &error));
        REQUIRE(restore_pbr_upload->commitAndWait(&error));
        INFO(error);

        base_scene.gbuffer = glossy_gbuffer.get();
        base_scene.constants.lighting[0] = 0.f;
        base_scene.constants.lighting[1] = 0.f;
        auto glossy_ordinary = renderer->renderBaseLighting(
            base_scene, &error);
        INFO(error);
        REQUIRE(glossy_ordinary);
        std::array<std::uint8_t, 32 * 32 * 4> glossy_ordinary_pixels{};
        REQUIRE(glossy_ordinary->colorTexture(0)->readback(
            *device, 0, 0, {}, glossy_ordinary_pixels.data(), 32 * 4,
            &error));
        INFO(error);

        ALMetalTextureDescriptor base_hero_descriptor;
        base_hero_descriptor.type = ALMetalTextureType::texture_cube;
        base_hero_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        base_hero_descriptor.width = 2;
        base_hero_descriptor.height = 2;
        base_hero_descriptor.mipmap_level_count = 2;
        base_hero_descriptor.storage = ALMetalStorageMode::private_gpu;
        base_hero_descriptor.usage = ALMetalTextureUsage::shader_read;
        base_hero_descriptor.label =
            "Alchemy base-light hero-probe mip family";
        auto base_hero_texture = ALMetalTexture::create(
            *device, base_hero_descriptor, &error);
        INFO(error);
        REQUIRE(base_hero_texture);
        const std::array<std::uint8_t, 2 * 2 * 4> red_hero_mip{{
            255, 0, 0, 255, 255, 0, 0, 255,
            255, 0, 0, 255, 255, 0, 0, 255,
        }};
        const std::array<std::uint8_t, 4> blue_hero_mip{{
            0, 0, 255, 255,
        }};
        auto base_hero_upload = device->makeCommandBuffer(
            "Alchemy base-light hero-probe upload", &error);
        INFO(error);
        REQUIRE(base_hero_upload);
        for (std::uint32_t face = 0; face < 6; ++face)
        {
            REQUIRE(base_hero_texture->upload(
                *base_hero_upload, 0, face, {}, red_hero_mip.data(),
                2 * 4, &error));
            REQUIRE(base_hero_texture->upload(
                *base_hero_upload, 1, face, {}, blue_hero_mip.data(),
                4, &error));
        }
        REQUIRE(base_hero_upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldHeroProbe base_hero;
        base_hero.radiance_texture = base_hero_texture.get();
        base_hero.constants.clip_plane[3] = 1.f;
        base_hero.constants.config[1] = 1;
        base_hero.constants.config[2] = 4;
        base_scene.hero_probe = &base_hero;
        auto glossy_hero = renderer->renderBaseLighting(base_scene, &error);
        INFO(error);
        REQUIRE(glossy_hero);
        std::array<std::uint8_t, 32 * 32 * 4> glossy_hero_pixels{};
        REQUIRE(glossy_hero->colorTexture(0)->readback(
            *device, 0, 0, {}, glossy_hero_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(glossy_hero_pixels[center + 2] >
              glossy_ordinary_pixels[center + 2]);

        base_hero.constants.config[2] = 8;
        error.clear();
        CHECK_FALSE(renderer->renderBaseLighting(base_scene, &error));
        CHECK(error == "Metal world base-light hero probe is invalid");
        error.clear();
        base_hero.constants.config[2] = 4;
        base_scene.hero_probe = nullptr;

        // The opaque SSR finish uses the same 0.567 gloss threshold and GGX
        // dominant ray as reflectionProbeF. Turn the surface toward the view,
        // arrange a first-step red history hit, and require that hit to raise
        // red over the ordinary-probe-only result. Incomplete paired history
        // remains fail-closed.
        const std::array<std::uint8_t, 4> glossy_legacy_specular{{
            255, 255, 255, 204,
        }};
        const std::array<std::uint16_t, 4> reverse_legacy_normal{{
            65535, 65535, 65535, 22282,
        }};
        auto ssr_gbuffer_upload = device->makeCommandBuffer(
            "Alchemy base-light SSR G-buffer upload", &error);
        INFO(error);
        REQUIRE(ssr_gbuffer_upload);
        REQUIRE(specular->upload(
            *ssr_gbuffer_upload, 0, 0, {},
            glossy_legacy_specular.data(), 4,
            &error));
        REQUIRE(normal->upload(
            *ssr_gbuffer_upload, 0, 0, {}, reverse_legacy_normal.data(), 8,
            &error));
        REQUIRE(ssr_gbuffer_upload->commitAndWait(&error));
        INFO(error);
        auto ssr_gbuffer = renderer->render(gbuffer_scene, &error);
        INFO(error);
        REQUIRE(ssr_gbuffer);

        auto ssr_restore_upload = device->makeCommandBuffer(
            "Alchemy restore base-light SSR inputs", &error);
        INFO(error);
        REQUIRE(ssr_restore_upload);
        REQUIRE(specular->upload(
            *ssr_restore_upload, 0, 0, {}, pbr_orm.data(), 4, &error));
        REQUIRE(normal->upload(
            *ssr_restore_upload, 0, 0, {}, pbr_normal.data(), 8,
            &error));
        REQUIRE(ssr_restore_upload->commitAndWait(&error));
        INFO(error);

        base_scene.gbuffer = ssr_gbuffer.get();
        auto no_ssr = renderer->renderBaseLighting(base_scene, &error);
        INFO(error);
        REQUIRE(no_ssr);
        std::array<std::uint8_t, 32 * 32 * 4> no_ssr_pixels{};
        REQUIRE(no_ssr->colorTexture(0)->readback(
            *device, 0, 0, {}, no_ssr_pixels.data(), 32 * 4, &error));
        INFO(error);

        ALMetalWorldScreenSpaceReflections invalid_base_ssr;
        invalid_base_ssr.constants.config[1] = 1;
        base_scene.screen_space_reflections = &invalid_base_ssr;
        error.clear();
        CHECK_FALSE(renderer->renderBaseLighting(base_scene, &error));
        CHECK(error ==
              "Metal world base-light screen-space reflections are invalid");
        error.clear();

        ALMetalTextureDescriptor base_ssr_color_descriptor;
        base_ssr_color_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        base_ssr_color_descriptor.width = 1;
        base_ssr_color_descriptor.height = 1;
        base_ssr_color_descriptor.storage = ALMetalStorageMode::private_gpu;
        base_ssr_color_descriptor.usage = ALMetalTextureUsage::shader_read;
        base_ssr_color_descriptor.label =
            "Alchemy base-light SSR scene color";
        auto base_ssr_color = ALMetalTexture::create(
            *device, base_ssr_color_descriptor, &error);
        INFO(error);
        REQUIRE(base_ssr_color);
        ALMetalTextureDescriptor base_ssr_depth_descriptor =
            base_ssr_color_descriptor;
        base_ssr_depth_descriptor.format = ALMetalPixelFormat::r32_float;
        base_ssr_depth_descriptor.label =
            "Alchemy base-light SSR scene depth";
        auto base_ssr_depth = ALMetalTexture::create(
            *device, base_ssr_depth_descriptor, &error);
        INFO(error);
        REQUIRE(base_ssr_depth);
        const std::array<std::uint8_t, 4> red_ssr{{255, 0, 0, 255}};
        const float ssr_hit_depth = 0.5f;
        auto base_ssr_upload = device->makeCommandBuffer(
            "Alchemy base-light SSR history upload", &error);
        INFO(error);
        REQUIRE(base_ssr_upload);
        REQUIRE(base_ssr_color->upload(
            *base_ssr_upload, 0, 0, {}, red_ssr.data(), 4, &error));
        REQUIRE(base_ssr_depth->upload(
            *base_ssr_upload, 0, 0, {}, &ssr_hit_depth, sizeof(float),
            &error));
        REQUIRE(base_ssr_upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldScreenSpaceReflections base_ssr;
        base_ssr.scene_color = base_ssr_color.get();
        base_ssr.scene_depth = base_ssr_depth.get();
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            base_ssr.constants.projection[axis * 4 + axis] = 1.f;
            base_ssr.constants.inverse_projection[axis * 4 + axis] = 1.f;
            base_ssr.constants.inverse_modelview_delta[axis * 4 + axis] =
                1.f;
        }
        base_ssr.constants.inverse_projection[14] = -0.4f;
        base_ssr.constants.screen[0] = 32.f;
        base_ssr.constants.screen[1] = 32.f;
        base_ssr.constants.screen[2] = 32.f;
        base_ssr.constants.trace[0] = 4.f;
        base_ssr.constants.trace[1] = 0.1f;
        base_ssr.constants.trace[2] = 0.2f;
        base_ssr.constants.trace[3] = 0.001f;
        base_ssr.constants.sampling[0] = 1.f;
        base_ssr.constants.sampling[1] = 1.6f;
        base_ssr.constants.config[1] = 1;
        base_scene.screen_space_reflections = &base_ssr;
        auto with_ssr = renderer->renderBaseLighting(base_scene, &error);
        INFO(error);
        REQUIRE(with_ssr);
        std::array<std::uint8_t, 32 * 32 * 4> with_ssr_pixels{};
        REQUIRE(with_ssr->colorTexture(0)->readback(
            *device, 0, 0, {}, with_ssr_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(with_ssr_pixels[center + 0] > no_ssr_pixels[center + 0]);
        base_scene.screen_space_reflections = nullptr;
        base_scene.gbuffer = pbr_gbuffer.get();
        base_scene.constants.lighting[0] = 1.f;
        base_scene.constants.lighting[1] = 1.f;

        std::array<std::uint8_t, 32 * 32 * 2> occluded_light_map{};
        auto occluded_upload = device->makeCommandBuffer(
            "Alchemy occluded base-light map upload", &error);
        INFO(error);
        REQUIRE(occluded_upload);
        REQUIRE(light_map->upload(
            *occluded_upload, 0, 0, {}, occluded_light_map.data(),
            32 * 2, &error));
        REQUIRE(occluded_upload->commitAndWait(&error));
        INFO(error);
        base_scene.constants.config[3] = 1;
        auto base_occluded = renderer->renderBaseLighting(
            base_scene, &error);
        INFO(error);
        REQUIRE(base_occluded);
        std::array<std::uint8_t, 32 * 32 * 4> base_occluded_pixels{};
        REQUIRE(base_occluded->colorTexture(0)->readback(
            *device, 0, 0, {}, base_occluded_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(base_occluded_pixels[center + 0] <
              base_pixels[center + 0]);
        CHECK(base_occluded_pixels[center + 1] <
              base_pixels[center + 1]);
        CHECK(base_occluded_pixels[center + 2] <
              base_pixels[center + 2]);

        auto restored_upload = device->makeCommandBuffer(
            "Alchemy restored base-light map upload", &error);
        INFO(error);
        REQUIRE(restored_upload);
        REQUIRE(light_map->upload(
            *restored_upload, 0, 0, {}, full_light_map.data(),
            32 * 2, &error));
        REQUIRE(restored_upload->commitAndWait(&error));
        INFO(error);

        base_scene.constants.config[3] = 0;
        base_scene.gbuffer = pbr_reverse_gbuffer.get();
        base_scene.constants.inverse_projection[10] = -1.f;
        base_scene.constants.config[0] = 1;
        auto base_reverse = renderer->renderBaseLighting(
            base_scene, &error);
        INFO(error);
        REQUIRE(base_reverse);
        std::array<std::uint8_t, 32 * 32 * 4> base_reverse_pixels{};
        REQUIRE(base_reverse->colorTexture(0)->readback(
            *device, 0, 0, {}, base_reverse_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            CHECK(std::abs(static_cast<int>(
                      base_reverse_pixels[center + channel]) -
                  static_cast<int>(base_pixels[center + channel])) <= 1);
        }

        base_scene.gbuffer = gbuffer.get();
        base_scene.constants.inverse_projection[10] = 1.f;
        base_scene.constants.config[0] = 0;
        base_scene.constants.atmos_amblit[0] = 0.25f;
        base_scene.constants.atmos_amblit[1] = 0.25f;
        base_scene.constants.atmos_amblit[2] = 0.25f;
        auto base_legacy = renderer->renderBaseLighting(
            base_scene, &error);
        INFO(error);
        REQUIRE(base_legacy);
        std::array<std::uint8_t, 32 * 32 * 4> base_legacy_pixels{};
        REQUIRE(base_legacy->colorTexture(0)->readback(
            *device, 0, 0, {}, base_legacy_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(base_legacy_pixels[center + 0] > 0);
        CHECK(base_legacy_pixels[center + 1] > 0);
        CHECK(base_legacy_pixels[center + 2] > 0);

        base_scene.constants.config[1] = 1;
        base_scene.constants.atmos_sunlit[0] = 1.f;
        base_scene.constants.atmos_sunlit[1] = 1.f;
        base_scene.constants.atmos_sunlit[2] = 1.f;
        auto base_classic = renderer->renderBaseLighting(
            base_scene, &error);
        INFO(error);
        REQUIRE(base_classic);
        std::array<std::uint8_t, 32 * 32 * 4> base_classic_pixels{};
        REQUIRE(base_classic->colorTexture(0)->readback(
            *device, 0, 0, {}, base_classic_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(base_classic_pixels[center + 0] > 0);
        CHECK(base_classic_pixels[center + 1] > 0);
        CHECK(base_classic_pixels[center + 2] > 0);

        ALMetalWorldBaseLightingScene invalid_base_scene;
        error.clear();
        CHECK_FALSE(renderer->renderBaseLighting(
            invalid_base_scene, &error));
        CHECK(error == "Metal world base-light scene is incomplete");

        // The same owner must preserve the HDR screen format. A zero R11G11B10
        // emissive plane is sufficient because local point lights do not add
        // the emissive attachment, but its format is part of the G-buffer ABI.
        auto hdr_emissive = make_texture(
            ALMetalPixelFormat::rg11b10_float,
            "Alchemy local-light HDR G-buffer emissive");
        const std::uint32_t zero_emissive = 0;
        auto hdr_upload = device->makeCommandBuffer(
            "Alchemy local-light HDR emissive upload", &error);
        INFO(error);
        REQUIRE(hdr_upload);
        REQUIRE(hdr_emissive->upload(
            *hdr_upload, 0, 0, {}, &zero_emissive,
            sizeof(zero_emissive), &error));
        REQUIRE(hdr_upload->commitAndWait(&error));
        INFO(error);
        batch.impostor_textures[3] = hdr_emissive.get();
        gbuffer_scene.hdr = true;
        auto hdr_gbuffer = renderer->render(gbuffer_scene, &error);
        INFO(error);
        REQUIRE(hdr_gbuffer);
        sky_scene.hdr = true;
        REQUIRE(renderer->compositeHDRISky(
            *hdr_gbuffer, sky_scene, &error));
        INFO(error);
        auto hdr_split_gbuffer = renderer->render(
            gbuffer_scene, &error);
        INFO(error);
        REQUIRE(hdr_split_gbuffer);
        windlight_scene.hdr = true;
        REQUIRE(renderer->compositeWindLightSky(
            *hdr_split_gbuffer, windlight_scene, &error));
        sky_scene.horizontal_fraction = 0.5f;
        REQUIRE(renderer->compositeHDRISky(
            *hdr_split_gbuffer, sky_scene, &error));
        INFO(error);
        std::array<std::uint32_t, 32 * 32> hdr_split_emissive{};
        std::array<std::uint16_t, 32 * 32 * 4> hdr_split_normal{};
        REQUIRE(hdr_split_gbuffer->colorTexture(3)->readback(
            *device, 0, 0, {}, hdr_split_emissive.data(), 32 * 4,
            &error));
        REQUIRE(hdr_split_gbuffer->colorTexture(2)->readback(
            *device, 0, 0, {}, hdr_split_normal.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(hdr_split_emissive[split_left / 4] != 0);
        CHECK(hdr_split_emissive[split_right / 4] != 0);
        CHECK(hdr_split_normal[split_left + 3] == 65535);
        CHECK(hdr_split_normal[split_right + 3] == 0);
        sky_scene.horizontal_fraction = 1.f;
        base_scene.gbuffer = hdr_gbuffer.get();
        base_scene.hdr = true;
        base_scene.hero_probe = nullptr;
        base_scene.screen_space_reflections = nullptr;
        base_scene.constants.inverse_projection[10] = 1.f;
        base_scene.constants.config[0] = 0;
        base_scene.constants.config[1] = 0;
        base_scene.constants.config[3] = 0;
        base_scene.constants.sky[0] = 1;
        base_scene.constants.sky[1] = 1;
        auto hdr_base = renderer->renderBaseLighting(base_scene, &error);
        INFO(error);
        REQUIRE(hdr_base);
        std::array<std::uint16_t, 32 * 32 * 4> hdr_base_pixels{};
        REQUIRE(hdr_base->colorTexture(0)->readback(
            *device, 0, 0, {}, hdr_base_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(hdr_base_pixels[outside + 0] >= 0x37e0);
        CHECK(hdr_base_pixels[outside + 0] <= 0x3820);
        CHECK(hdr_base_pixels[outside + 1] >= 0x33e0);
        CHECK(hdr_base_pixels[outside + 1] <= 0x3420);
        CHECK(hdr_base_pixels[outside + 2] >= 0x39c0);
        CHECK(hdr_base_pixels[outside + 2] <= 0x3a40);
        CHECK(hdr_base_pixels[outside + 3] == 0);

        base_scene.gbuffer = hdr_split_gbuffer.get();
        base_scene.constants.sky[2] = 2;
        base_scene.constants.sky_transfer[0] = 2.f;
        auto hdr_split_base = renderer->renderBaseLighting(
            base_scene, &error);
        INFO(error);
        REQUIRE(hdr_split_base);
        std::array<std::uint16_t, 32 * 32 * 4>
            hdr_split_base_pixels{};
        REQUIRE(hdr_split_base->colorTexture(0)->readback(
            *device, 0, 0, {}, hdr_split_base_pixels.data(),
            32 * 8, &error));
        INFO(error);
        CHECK(hdr_split_base_pixels[split_left + 0] >= 0x37e0);
        CHECK(hdr_split_base_pixels[split_left + 0] <= 0x3820);
        CHECK(hdr_split_base_pixels[split_left + 1] >= 0x33e0);
        CHECK(hdr_split_base_pixels[split_left + 1] <= 0x3420);
        CHECK(hdr_split_base_pixels[split_left + 2] >= 0x39c0);
        CHECK(hdr_split_base_pixels[split_left + 2] <= 0x3a40);
        CHECK(hdr_split_base_pixels[split_right + 0] > 0);
        CHECK(hdr_split_base_pixels[split_right + 1] > 0);
        CHECK(hdr_split_base_pixels[split_right + 2] > 0);
        base_scene.gbuffer = hdr_gbuffer.get();
        base_scene.constants.sky[2] = 0;
        base_scene.constants.sky_transfer[0] = 1.f;

        // Exercise the HDR WindLight variant: it retains display-encoded
        // color in R11G11B10F, then the base owner decodes and scales it.
        REQUIRE(renderer->compositeWindLightSky(
            *hdr_gbuffer, windlight_scene, &error));
        star_scene.hdr = true;
        REQUIRE(renderer->compositeWindLightStars(
            *hdr_gbuffer, star_scene, &error));
        std::array<std::uint32_t, 32 * 32> hdr_body_before{};
        REQUIRE(hdr_gbuffer->colorTexture(3)->readback(
            *device, 0, 0, {}, hdr_body_before.data(), 32 * 4,
            &error));
        body_scene.hdr = true;
        REQUIRE(renderer->compositeWindLightHeavenlyBodies(
            *hdr_gbuffer, body_scene, &error));
        INFO(error);
        std::array<std::uint32_t, 32 * 32> hdr_body_after{};
        REQUIRE(hdr_gbuffer->colorTexture(3)->readback(
            *device, 0, 0, {}, hdr_body_after.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(hdr_body_after[outside / 4] !=
              hdr_body_before[outside / 4]);
        CHECK(hdr_body_after[center / 4] ==
              hdr_body_before[center / 4]);
        meteor_scene.hdr = true;
        REQUIRE(renderer->compositeWindLightMeteors(
            *hdr_gbuffer, meteor_scene, &error));
        INFO(error);
        std::array<std::uint32_t, 32 * 32> hdr_meteor_after{};
        REQUIRE(hdr_gbuffer->colorTexture(3)->readback(
            *device, 0, 0, {}, hdr_meteor_after.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(hdr_meteor_after[outside / 4] !=
              hdr_body_after[outside / 4]);
        CHECK(hdr_meteor_after[center / 4] ==
              hdr_body_after[center / 4]);
        aurora_scene.hdr = true;
        REQUIRE(renderer->compositeWindLightAurora(
            *hdr_gbuffer, aurora_scene, &error));
        INFO(error);
        std::array<std::uint32_t, 32 * 32> hdr_aurora_after{};
        REQUIRE(hdr_gbuffer->colorTexture(3)->readback(
            *device, 0, 0, {}, hdr_aurora_after.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(hdr_aurora_after != hdr_meteor_after);
        CHECK(hdr_aurora_after[center / 4] ==
              hdr_meteor_after[center / 4]);
        cloud_scene.hdr = true;
        REQUIRE(renderer->compositeWindLightClouds(
            *hdr_gbuffer, cloud_scene, &error));
        INFO(error);
        std::array<std::uint32_t, 32 * 32> hdr_cloud_after{};
        REQUIRE(hdr_gbuffer->colorTexture(3)->readback(
            *device, 0, 0, {}, hdr_cloud_after.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(hdr_cloud_after[outside / 4] !=
              hdr_aurora_after[outside / 4]);
        CHECK(hdr_cloud_after[center / 4] ==
              hdr_aurora_after[center / 4]);
        std::array<std::uint16_t, 32 * 32 * 4> hdr_windlight_normal{};
        REQUIRE(hdr_gbuffer->colorTexture(2)->readback(
            *device, 0, 0, {}, hdr_windlight_normal.data(),
            32 * 8, &error));
        INFO(error);
        CHECK(hdr_windlight_normal[outside + 0] == 0);
        CHECK(hdr_windlight_normal[outside + 1] == 65535);
        CHECK(hdr_windlight_normal[outside + 2] == 0);
        CHECK(hdr_windlight_normal[outside + 3] == 0);
        base_scene.constants.sky[2] = 1;
        base_scene.constants.sky_transfer[0] = 2.f;
        auto hdr_windlight_base = renderer->renderBaseLighting(
            base_scene, &error);
        INFO(error);
        REQUIRE(hdr_windlight_base);
        std::array<std::uint16_t, 32 * 32 * 4>
            hdr_windlight_pixels{};
        REQUIRE(hdr_windlight_base->colorTexture(0)->readback(
            *device, 0, 0, {}, hdr_windlight_pixels.data(),
            32 * 8, &error));
        INFO(error);
        CHECK(hdr_windlight_pixels[outside + 0] > 0);
        CHECK(hdr_windlight_pixels[outside + 1] > 0);
        CHECK(hdr_windlight_pixels[outside + 2] > 0);
        CHECK(hdr_windlight_pixels[outside + 3] == 0);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(hdr_windlight_pixels[center + channel] ==
                  hdr_base_pixels[center + channel]);
        }
        base_scene.constants.sky[2] = 0;
        base_scene.constants.sky_transfer[0] = 1.f;
        lighting_scene.gbuffer = hdr_gbuffer.get();
        lighting_scene.hdr = true;
        auto hdr = renderer->renderLocalLights(lighting_scene, &error);
        INFO(error);
        REQUIRE(hdr);
        CHECK(hdr->descriptor().colors[0].format ==
              ALMetalPixelFormat::rgba16_float);
        std::array<std::uint16_t, 32 * 32 * 4> hdr_pixels{};
        REQUIRE(hdr->colorTexture(0)->readback(
            *device, 0, 0, {}, hdr_pixels.data(), 32 * 8, &error));
        INFO(error);
        CHECK(hdr_pixels[center + 0] > 0);
        CHECK(hdr_pixels[center + 1] > 0);
        CHECK(hdr_pixels[center + 2] > 0);
        CHECK(hdr_pixels[center + 3] == 0);

        ALMetalWorldLightingCompositionScene hdr_composition_scene;
        hdr_composition_scene.base_lighting = hdr.get();
        hdr_composition_scene.local_lighting = hdr.get();
        hdr_composition_scene.projector_lighting = hdr.get();
        hdr_composition_scene.hdr = true;
        auto hdr_composed = renderer->composeLighting(
            hdr_composition_scene, &error);
        INFO(error);
        REQUIRE(hdr_composed);
        REQUIRE(hdr_composed->descriptor().colors[0].format ==
                ALMetalPixelFormat::rgba16_float);
        std::array<std::uint16_t, 32 * 32 * 4> hdr_composed_pixels{};
        REQUIRE(hdr_composed->colorTexture(0)->readback(
            *device, 0, 0, {}, hdr_composed_pixels.data(), 32 * 8,
            &error));
        INFO(error);
        CHECK(hdr_composed_pixels[center + 0] > hdr_pixels[center + 0]);
        CHECK(hdr_composed_pixels[center + 1] > hdr_pixels[center + 1]);
        CHECK(hdr_composed_pixels[center + 2] > hdr_pixels[center + 2]);
        CHECK(hdr_composed_pixels[center + 3] == 0);

        composition_scene.local_lighting = hdr.get();
        error.clear();
        CHECK_FALSE(renderer->composeLighting(
            composition_scene, &error));
        CHECK(error ==
              "Metal world lighting composition targets are incompatible");

        ALMetalWorldLightingCompositionScene invalid_composition_scene;
        error.clear();
        CHECK_FALSE(renderer->composeLighting(
            invalid_composition_scene, &error));
        CHECK(error ==
              "Metal world lighting composition is incomplete");

        ALMetalWorldProjectorLightingScene invalid_projector_scene;
        error.clear();
        CHECK_FALSE(renderer->renderProjectorLights(
            invalid_projector_scene, &error));
        CHECK(error ==
              "Metal world projector-light scene is incomplete");

        ALMetalWorldLocalLightingScene invalid_scene;
        error.clear();
        CHECK_FALSE(renderer->renderLocalLights(invalid_scene, &error));
        CHECK(error == "Metal world local-light scene is incomplete");
    }
}

TEST_CASE("almetal: classic terrain tessellates its height field into the world G-buffer", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error);
        REQUIRE(renderer);

        const std::array<std::array<float, 4>, 4> control_points{{
            {{0.f, 0.f, 0.f, 1.f}},
            {{2.f, 0.f, 0.f, 1.f}},
            {{2.f, 2.f, 0.f, 1.f}},
            {{0.f, 2.f, 0.f, 1.f}},
        }};
        ALMetalBufferDescriptor vertex_descriptor;
        vertex_descriptor.length = sizeof(control_points);
        vertex_descriptor.storage = ALMetalStorageMode::shared;
        vertex_descriptor.label = "Alchemy terrain test control points";
        auto vertices = ALMetalBuffer::create(
            *device, vertex_descriptor, &error);
        INFO(error);
        REQUIRE(vertices);
        REQUIRE(vertices->write(
            0, control_points.data(), sizeof(control_points), &error));

        ALMetalTextureDescriptor detail_descriptor;
        detail_descriptor.format = ALMetalPixelFormat::rgba8_unorm_srgb;
        detail_descriptor.width = 1;
        detail_descriptor.height = 1;
        detail_descriptor.storage = ALMetalStorageMode::private_gpu;
        detail_descriptor.usage = ALMetalTextureUsage::shader_read;
        detail_descriptor.label = "Alchemy terrain red sRGB detail";
        auto detail = ALMetalTexture::create(
            *device, detail_descriptor, &error);
        INFO(error);
        REQUIRE(detail);

        ALMetalTextureDescriptor ramp_descriptor = detail_descriptor;
        ramp_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        ramp_descriptor.label = "Alchemy terrain opaque alpha ramp";
        auto ramp = ALMetalTexture::create(*device, ramp_descriptor, &error);
        INFO(error);
        REQUIRE(ramp);

        ALMetalTextureDescriptor overlay_descriptor = detail_descriptor;
        overlay_descriptor.label = "Alchemy terrain encoded parcel overlay";
        auto overlay = ALMetalTexture::create(
            *device, overlay_descriptor, &error);
        INFO(error);
        REQUIRE(overlay);

        ALMetalTextureDescriptor height_descriptor;
        height_descriptor.format = ALMetalPixelFormat::r32_float;
        height_descriptor.width = 8;
        height_descriptor.height = 8;
        height_descriptor.storage = ALMetalStorageMode::private_gpu;
        height_descriptor.usage = ALMetalTextureUsage::shader_read;
        height_descriptor.label = "Alchemy terrain flat height map";
        auto height = ALMetalTexture::create(
            *device, height_descriptor, &error);
        INFO(error);
        REQUIRE(height);

        ALMetalTextureDescriptor composition_descriptor = height_descriptor;
        composition_descriptor.format = ALMetalPixelFormat::rg16_float;
        composition_descriptor.label = "Alchemy terrain composition map";
        auto composition = ALMetalTexture::create(
            *device, composition_descriptor, &error);
        INFO(error);
        REQUIRE(composition);

        const std::array<std::uint8_t, 4> detail_pixel{{255, 0, 0, 255}};
        const std::array<std::uint8_t, 4> ramp_pixel{{0, 0, 0, 255}};
        const std::array<std::uint8_t, 4> overlay_pixel{{128, 128, 128, 255}};
        std::array<float, 64> height_pixels{};
        for (std::size_t row = 0; row < 8; ++row)
        {
            for (std::size_t column = 0; column < 8; ++column)
            {
                // Texel (x,y) holds grid x + 2*y: one and two metres of
                // rise per metre, so both surface modes must emit the same
                // non-axis-aligned slope.
                height_pixels[row * 8 + column] =
                    (static_cast<float>(column) - 2.f) +
                    2.f * (static_cast<float>(row) - 2.f);
            }
        }
        const std::array<std::uint16_t, 128> composition_pixels{};
        auto upload = device->makeCommandBuffer(
            "Alchemy terrain test texture upload", &error);
        INFO(error);
        REQUIRE(upload);
        REQUIRE(detail->upload(
            *upload, 0, 0, {}, detail_pixel.data(), 4, &error));
        REQUIRE(ramp->upload(
            *upload, 0, 0, {}, ramp_pixel.data(), 4, &error));
        REQUIRE(overlay->upload(
            *upload, 0, 0, {}, overlay_pixel.data(), 4, &error));
        REQUIRE(height->upload(
            *upload, 0, 0, {}, height_pixels.data(), 8 * sizeof(float),
            &error));
        REQUIRE(composition->upload(
            *upload, 0, 0, {}, composition_pixels.data(),
            8 * 2 * sizeof(std::uint16_t), &error));
        REQUIRE(upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldSimpleBatch batch;
        batch.terrain = true;
        batch.vertices = vertices.get();
        batch.vertex_count = control_points.size();
        batch.texture_count = 4;
        for (std::size_t texture = 0; texture < 4; ++texture)
        {
            batch.diffuse_textures[texture] = detail.get();
        }
        batch.terrain_height_texture = height.get();
        batch.terrain_composition_texture = composition.get();
        batch.terrain_alpha_ramp_texture = ramp.get();
        std::fill_n(
            batch.terrain_tessellation_factors, 6, std::uint16_t{0x4400});
        batch.terrain_grid_scale = 1.f;
        batch.terrain_region_scale = 4.f;
        batch.terrain_detail_scale = 1.f;
        batch.terrain_triplanar_blend = 8.f;
        batch.terrain_smoothing = true;
        batch.terrain_triplanar = true;
        batch.terrain_tessellation_pinned = true;
        batch.modelview_projection[0] = 0.8f;
        batch.modelview_projection[5] = 0.8f;
        // Flatten clip Z so the test can exercise heights beyond the
        // canonical clip volume without changing the expected footprint.
        batch.modelview_projection[10] = 0.f;
        batch.modelview_projection[12] = -0.8f;
        batch.modelview_projection[13] = -0.8f;
        batch.modelview_projection[15] = 1.f;
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            batch.normal_matrix[axis][axis] = 1.f;
        }

        ALMetalWorldGBufferScene scene;
        scene.batches = &batch;
        scene.batch_count = 1;
        scene.width = 32;
        scene.height = 32;
        scene.normal_hdr = true;
        scene.hdr = true;

        batch.terrain_tessellation_factors[0] = 0;
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world terrain batch is invalid");
        batch.terrain_tessellation_factors[0] = 0x4400;
        error.clear();

        batch.terrain_parcel_overlay_texture = ramp.get();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world terrain batch is invalid");
        batch.terrain_parcel_overlay_texture = nullptr;
        error.clear();

        auto result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(result);
        REQUIRE(device->waitUntilIdle(&error));
        INFO(error);

        std::array<std::uint8_t, 32 * 32 * 4> albedo{};
        REQUIRE(result->colorTexture(0)->readback(
            *device, 0, 0, {}, albedo.data(), 32 * 4, &error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> specular{};
        REQUIRE(result->colorTexture(1)->readback(
            *device, 0, 0, {}, specular.data(), 32 * 4, &error));
        INFO(error);
        std::array<std::uint16_t, 32 * 32 * 4> normal{};
        REQUIRE(result->colorTexture(2)->readback(
            *device, 0, 0, {}, normal.data(), 32 * 8, &error));
        INFO(error);

        const std::size_t center = (16 * 32 + 16) * 4;
        CHECK(albedo[center + 0] == 255);
        CHECK(albedo[center + 1] == 0);
        CHECK(albedo[center + 2] == 0);
        CHECK(albedo[center + 3] == 0);
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            CHECK(specular[center + channel] == 0);
        }
        CHECK(std::abs(static_cast<int>(normal[center + 0]) - 24576) <= 1);
        CHECK(std::abs(static_cast<int>(normal[center + 1]) - 16384) <= 1);
        CHECK(normal[center + 2] == 0);
        CHECK(std::abs(static_cast<int>(normal[center + 3]) - 22282) <= 2);

        // The live parcel overlay's Metal mirror is sRGB, so sampling decodes
        // before blending and the sRGB G-buffer store encodes it back to the
        // original byte. This guards against a missing or double decode.
        batch.terrain_parcel_overlay_texture = overlay.get();
        auto overlay_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(overlay_result);
        REQUIRE(device->waitUntilIdle(&error));
        INFO(error);
        albedo.fill(0);
        REQUIRE(overlay_result->colorTexture(0)->readback(
            *device, 0, 0, {}, albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(albedo[center + 0]) - 128) <= 1);
        CHECK(std::abs(static_cast<int>(albedo[center + 1]) - 128) <= 1);
        CHECK(std::abs(static_cast<int>(albedo[center + 2]) - 128) <= 1);
        CHECK(albedo[center + 3] == 0);
        batch.terrain_parcel_overlay_texture = nullptr;

        const std::size_t outside = (1 * 32 + 1) * 4;
        CHECK(albedo[outside + 0] == 255);
        CHECK(albedo[outside + 1] == 0);
        CHECK(albedo[outside + 2] == 255);
        CHECK(albedo[outside + 3] == 255);

        // Exercise the world-stable hex lattice through every triplanar
        // projection. The single-texel detail remains red through all cell
        // rotations and blends, while the nontrivial places cover both the
        // direct and mirrored-side placement paths.
        batch.terrain_hex = true;
        batch.terrain_triplanar_blend = 1.f;
        const float hex_fraction[2][4] = {
            {0.125f, 0.75f, 0.5f, 0.25f},
            {0.875f, 0.375f, 0.625f, 0.9375f},
        };
        const std::int32_t hex_base[2][4] = {
            {17, -31, std::numeric_limits<std::int32_t>::min(), -1048577},
            {-2147483647, 2147483646, 0, 0},
        };
        std::memcpy(batch.terrain_hex_fraction,
                    hex_fraction, sizeof(hex_fraction));
        std::memcpy(batch.terrain_hex_base, hex_base, sizeof(hex_base));

        batch.terrain_hex_fraction[1][3] = 1.f;
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world terrain batch is invalid");
        batch.terrain_hex_fraction[1][3] = hex_fraction[1][3];
        error.clear();

        auto hex_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(hex_result);
        REQUIRE(device->waitUntilIdle(&error));
        INFO(error);
        albedo.fill(0);
        REQUIRE(hex_result->colorTexture(0)->readback(
            *device, 0, 0, {}, albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(albedo[center + 0] == 255);
        CHECK(albedo[center + 1] == 0);
        CHECK(albedo[center + 2] == 0);
        CHECK(albedo[center + 3] == 0);

        // The collision-surface evaluator and dynamic fractional-odd
        // pipeline reproduce the same analytic plane through a different
        // production variant.
        batch.terrain_hex = false;
        batch.terrain_smoothing = false;
        batch.terrain_triplanar = false;
        batch.terrain_tessellation_pinned = false;
        std::fill_n(
            batch.terrain_tessellation_factors, 6, std::uint16_t{0x4200});
        auto dynamic_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(dynamic_result);
        REQUIRE(device->waitUntilIdle(&error));
        INFO(error);
        normal.fill(0);
        REQUIRE(dynamic_result->colorTexture(2)->readback(
            *device, 0, 0, {}, normal.data(), 32 * 8, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(normal[center + 0]) - 24576) <= 1);
        CHECK(std::abs(static_cast<int>(normal[center + 1]) - 16384) <= 1);
        CHECK(normal[center + 2] == 0);
        CHECK(std::abs(static_cast<int>(normal[center + 3]) - 22282) <= 2);
    }
}

TEST_CASE("almetal: PBR terrain preserves material tiers, paint ownership and masking", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error);
        REQUIRE(renderer);

        const std::array<std::array<float, 4>, 4> control_points{{
            {{0.f, 0.f, 0.f, 1.f}},
            {{2.f, 0.f, 0.f, 1.f}},
            {{2.f, 2.f, 0.f, 1.f}},
            {{0.f, 2.f, 0.f, 1.f}},
        }};
        ALMetalBufferDescriptor vertex_descriptor;
        vertex_descriptor.length = sizeof(control_points);
        vertex_descriptor.storage = ALMetalStorageMode::shared;
        vertex_descriptor.label = "Alchemy PBR terrain test control points";
        auto vertices = ALMetalBuffer::create(
            *device, vertex_descriptor, &error);
        INFO(error);
        REQUIRE(vertices);
        REQUIRE(vertices->write(
            0, control_points.data(), sizeof(control_points), &error));

        const auto make_texture = [&device, &error](
            ALMetalPixelFormat format, std::uint32_t width,
            std::uint32_t height, const char* label)
        {
            ALMetalTextureDescriptor descriptor;
            descriptor.format = format;
            descriptor.width = width;
            descriptor.height = height;
            descriptor.storage = ALMetalStorageMode::private_gpu;
            descriptor.usage = ALMetalTextureUsage::shader_read;
            descriptor.label = label;
            auto texture = ALMetalTexture::create(
                *device, descriptor, &error);
            INFO(error);
            REQUIRE(texture);
            return texture;
        };

        auto base0 = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 1, 1,
            "Alchemy PBR terrain material zero base");
        auto base1 = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 1, 1,
            "Alchemy PBR terrain material one base");
        auto base2 = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 1, 1,
            "Alchemy PBR terrain material two base");
        auto base3 = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 1, 1,
            "Alchemy PBR terrain material three base");
        auto normal_map = make_texture(
            ALMetalPixelFormat::rgba8_unorm, 1, 1,
            "Alchemy PBR terrain normal");
        auto orm_map = make_texture(
            ALMetalPixelFormat::rgba8_unorm, 1, 1,
            "Alchemy PBR terrain ORM");
        auto emissive_map = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 1, 1,
            "Alchemy PBR terrain emissive");
        auto paint_map = make_texture(
            ALMetalPixelFormat::rgba8_unorm, 1, 1,
            "Alchemy PBR terrain paint map");
        auto alpha_ramp = make_texture(
            ALMetalPixelFormat::rgba8_unorm, 1, 1,
            "Alchemy PBR terrain alpha ramp");
        auto height = make_texture(
            ALMetalPixelFormat::r32_float, 8, 8,
            "Alchemy PBR terrain flat height map");
        auto composition = make_texture(
            ALMetalPixelFormat::rg16_float, 8, 8,
            "Alchemy PBR terrain composition map");

        const std::array<std::uint8_t, 4> base0_pixel{{
            255, 255, 255, 128,
        }};
        const std::array<std::uint8_t, 4> base1_pixel{{
            0, 255, 0, 255,
        }};
        const std::array<std::uint8_t, 4> base2_pixel{{
            255, 0, 0, 255,
        }};
        const std::array<std::uint8_t, 4> base3_pixel{{
            0, 0, 255, 255,
        }};
        const std::array<std::uint8_t, 4> normal_pixel{{
            128, 128, 255, 255,
        }};
        const std::array<std::uint8_t, 4> orm_pixel{{
            64, 128, 192, 255,
        }};
        const std::array<std::uint8_t, 4> emissive_pixel{{
            255, 255, 255, 255,
        }};
        const std::array<std::uint8_t, 4> zero_pixel{};
        std::array<float, 64> height_pixels{};
        const std::array<std::uint16_t, 128> composition_pixels{};
        auto upload = device->makeCommandBuffer(
            "Alchemy PBR terrain texture upload", &error);
        INFO(error);
        REQUIRE(upload);
        REQUIRE(base0->upload(
            *upload, 0, 0, {}, base0_pixel.data(), 4, &error));
        REQUIRE(base1->upload(
            *upload, 0, 0, {}, base1_pixel.data(), 4, &error));
        REQUIRE(base2->upload(
            *upload, 0, 0, {}, base2_pixel.data(), 4, &error));
        REQUIRE(base3->upload(
            *upload, 0, 0, {}, base3_pixel.data(), 4, &error));
        REQUIRE(normal_map->upload(
            *upload, 0, 0, {}, normal_pixel.data(), 4, &error));
        REQUIRE(orm_map->upload(
            *upload, 0, 0, {}, orm_pixel.data(), 4, &error));
        REQUIRE(emissive_map->upload(
            *upload, 0, 0, {}, emissive_pixel.data(), 4, &error));
        REQUIRE(paint_map->upload(
            *upload, 0, 0, {}, zero_pixel.data(), 4, &error));
        REQUIRE(alpha_ramp->upload(
            *upload, 0, 0, {}, zero_pixel.data(), 4, &error));
        REQUIRE(height->upload(
            *upload, 0, 0, {}, height_pixels.data(), 8 * sizeof(float),
            &error));
        REQUIRE(composition->upload(
            *upload, 0, 0, {}, composition_pixels.data(),
            8 * 2 * sizeof(std::uint16_t), &error));
        REQUIRE(upload->commitAndWait(&error));
        INFO(error);

        ALMetalWorldSimpleBatch batch;
        batch.terrain = true;
        batch.terrain_pbr = true;
        batch.vertices = vertices.get();
        batch.vertex_count = control_points.size();
        batch.texture_count = 4;
        batch.pbr_slot_count = 4;
        batch.diffuse_textures[0] = base0.get();
        batch.diffuse_textures[1] = base1.get();
        batch.diffuse_textures[2] = base2.get();
        batch.diffuse_textures[3] = base3.get();
        for (std::size_t material = 0; material < 4; ++material)
        {
            ALMetalWorldPBRSlot& slot = batch.pbr_slots[material];
            slot.normal_texture = normal_map.get();
            slot.orm_texture = orm_map.get();
            slot.emissive_texture = emissive_map.get();
            batch.terrain_uv_transform[material][0] = 1.f;
            batch.terrain_uv_transform[material][3] = 1.f;
            batch.terrain_normal_axes[material][0] = 1.f;
            batch.terrain_normal_axes[material][3] = 1.f;
        }
        batch.pbr_slots[0].base_color_factor[0] = 0.5f;
        batch.pbr_slots[0].base_color_factor[2] = 0.25f;
        batch.pbr_slots[0].roughness_factor = 0.5f;
        batch.pbr_slots[0].metallic_factor = 0.5f;
        batch.pbr_slots[0].emissive_color[0] = 0.25f;
        batch.pbr_slots[0].emissive_color[1] = 0.5f;
        batch.pbr_slots[0].emissive_color[2] = 1.f;
        batch.terrain_height_texture = height.get();
        batch.terrain_composition_texture = composition.get();
        batch.terrain_paint_texture = paint_map.get();
        batch.terrain_paint_type = 1;
        batch.terrain_pbr_detail = 0;
        std::fill_n(
            batch.terrain_tessellation_factors, 6, std::uint16_t{0x4400});
        batch.terrain_grid_scale = 1.f;
        batch.terrain_region_scale = 4.f;
        batch.terrain_detail_scale = 1.f;
        batch.terrain_triplanar_blend = 8.f;
        batch.terrain_smoothing = true;
        batch.terrain_tessellation_pinned = true;
        batch.modelview_projection[0] = 0.8f;
        batch.modelview_projection[5] = 0.8f;
        batch.modelview_projection[10] = 0.f;
        batch.modelview_projection[12] = -0.8f;
        batch.modelview_projection[13] = -0.8f;
        batch.modelview_projection[15] = 1.f;
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            batch.terrain_modelview[axis * 4 + axis] = 1.f;
            if (axis < 3)
            {
                batch.normal_matrix[axis][axis] = 1.f;
            }
        }

        ALMetalWorldGBufferScene scene;
        scene.batches = &batch;
        scene.batch_count = 1;
        scene.width = 32;
        scene.height = 32;
        scene.normal_hdr = true;
        scene.hdr = true;

        batch.terrain_pbr_detail = -5;
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world terrain batch is invalid");
        batch.terrain_pbr_detail = 0;
        error.clear();

        batch.terrain_uv_transform[0][0] =
            std::numeric_limits<float>::quiet_NaN();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world terrain batch is invalid");
        batch.terrain_uv_transform[0][0] = 1.f;
        error.clear();

        batch.terrain_alpha_ramp_texture = alpha_ramp.get();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world terrain batch is invalid");
        batch.terrain_alpha_ramp_texture = nullptr;
        error.clear();

        auto result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(result);
        REQUIRE(device->waitUntilIdle(&error));
        INFO(error);
        std::array<std::uint8_t, 32 * 32 * 4> albedo{};
        std::array<std::uint8_t, 32 * 32 * 4> orm{};
        std::array<std::uint16_t, 32 * 32 * 4> normal{};
        std::array<std::uint8_t, 32 * 32 * 4> emissive{};
        REQUIRE(result->colorTexture(0)->readback(
            *device, 0, 0, {}, albedo.data(), 32 * 4, &error));
        REQUIRE(result->colorTexture(1)->readback(
            *device, 0, 0, {}, orm.data(), 32 * 4, &error));
        REQUIRE(result->colorTexture(2)->readback(
            *device, 0, 0, {}, normal.data(), 32 * 8, &error));
        REQUIRE(result->colorTexture(3)->readback(
            *device, 0, 0, {}, emissive.data(), 32 * 4, &error));
        INFO(error);

        const std::size_t center = (16 * 32 + 16) * 4;
        CHECK(std::abs(static_cast<int>(albedo[center + 0]) - 188) <= 1);
        CHECK(albedo[center + 1] == 255);
        CHECK(std::abs(static_cast<int>(albedo[center + 2]) - 137) <= 1);
        CHECK(albedo[center + 3] == 0);
        CHECK(std::abs(static_cast<int>(orm[center + 0]) - 64) <= 1);
        CHECK(std::abs(static_cast<int>(orm[center + 1]) - 64) <= 1);
        CHECK(std::abs(static_cast<int>(orm[center + 2]) - 96) <= 1);
        CHECK(orm[center + 3] <= 1);
        CHECK(std::abs(static_cast<int>(normal[center + 0]) - 32896) <= 4);
        CHECK(std::abs(static_cast<int>(normal[center + 1]) - 32896) <= 4);
        CHECK(std::abs(static_cast<int>(normal[center + 2]) - 32896) <= 2);
        CHECK(std::abs(static_cast<int>(normal[center + 3]) - 43908) <= 2);
        CHECK(std::any_of(
            emissive.begin() + center, emissive.begin() + center + 4,
            [](std::uint8_t value) { return value != 0; }));

        // The runtime detail selector reproduces all five source shader
        // permutations without multiplying the Metal pipeline count.
        for (std::int32_t detail = -4; detail < 0; ++detail)
        {
            CAPTURE(detail);
            batch.terrain_pbr_detail = detail;
            auto tier_result = renderer->render(scene, &error);
            INFO(error);
            REQUIRE(tier_result);
            REQUIRE(device->waitUntilIdle(&error));
            INFO(error);
            orm.fill(0);
            normal.fill(0);
            emissive.fill(0);
            REQUIRE(tier_result->colorTexture(1)->readback(
                *device, 0, 0, {}, orm.data(), 32 * 4, &error));
            REQUIRE(tier_result->colorTexture(2)->readback(
                *device, 0, 0, {}, normal.data(), 32 * 8, &error));
            REQUIRE(tier_result->colorTexture(3)->readback(
                *device, 0, 0, {}, emissive.data(), 32 * 4, &error));
            INFO(error);
            const int expected_ao = detail >= -1 ? 64 : 255;
            const int expected_roughness = detail >= -3 ? 64 : 255;
            const int expected_metal = detail >= -3 ? 96 : 0;
            const int expected_normal = detail >= -2 ? 32896 : 32768;
            CHECK(std::abs(static_cast<int>(orm[center + 0]) -
                           expected_ao) <= 1);
            CHECK(std::abs(static_cast<int>(orm[center + 1]) -
                           expected_roughness) <= 1);
            CHECK(std::abs(static_cast<int>(orm[center + 2]) -
                           expected_metal) <= 1);
            CHECK(std::abs(static_cast<int>(normal[center + 0]) -
                           expected_normal) <= 4);
            CHECK(std::all_of(
                emissive.begin() + center, emissive.begin() + center + 4,
                [](std::uint8_t value) { return value == 0; }));
        }
        batch.terrain_pbr_detail = 0;

        // The height-ramp mode's all-zero ramp selects material three. A
        // fully opaque parcel overlay then replaces that linear-space blend.
        batch.terrain_paint_type = 0;
        batch.terrain_paint_texture = nullptr;
        batch.terrain_alpha_ramp_texture = alpha_ramp.get();
        auto ramp_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(ramp_result);
        REQUIRE(device->waitUntilIdle(&error));
        albedo.fill(0);
        REQUIRE(ramp_result->colorTexture(0)->readback(
            *device, 0, 0, {}, albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(albedo[center + 0] == 0);
        CHECK(albedo[center + 1] == 0);
        CHECK(albedo[center + 2] == 255);

        batch.terrain_parcel_overlay_texture = base2.get();
        auto overlay_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(overlay_result);
        REQUIRE(device->waitUntilIdle(&error));
        albedo.fill(0);
        REQUIRE(overlay_result->colorTexture(0)->readback(
            *device, 0, 0, {}, albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(albedo[center + 0] == 255);
        CHECK(albedo[center + 1] == 0);
        CHECK(albedo[center + 2] == 0);
        batch.terrain_parcel_overlay_texture = nullptr;

        // Constant maps make the expected material invariant while every
        // material-specific lattice origin and triplanar projection executes.
        batch.terrain_hex = true;
        batch.terrain_triplanar = true;
        batch.terrain_triplanar_blend = 1.f;
        for (std::size_t element = 0; element < 8; ++element)
        {
            for (std::size_t component = 0; component < 4; ++component)
            {
                batch.terrain_hex_fraction[element][component] =
                    0.0625f * static_cast<float>(component + 1);
                batch.terrain_hex_base[element][component] =
                    static_cast<std::int32_t>(
                        element * 131 + component * 17) - 509;
            }
        }
        batch.terrain_hex_base[7][3] =
            std::numeric_limits<std::int32_t>::min();
        auto hex_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(hex_result);
        REQUIRE(device->waitUntilIdle(&error));
        albedo.fill(0);
        REQUIRE(hex_result->colorTexture(0)->readback(
            *device, 0, 0, {}, albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(albedo[center + 0] == 0);
        CHECK(albedo[center + 1] == 0);
        CHECK(albedo[center + 2] == 255);

        batch.terrain_hex_fraction[7][3] = 1.f;
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world terrain batch is invalid");
        batch.terrain_hex_fraction[7][3] = 0.25f;
        error.clear();

        batch.terrain_hex = false;
        batch.terrain_triplanar = false;
        batch.terrain_flat_normals = true;
        auto flat_result = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(flat_result);

        // Paint-map material zero carries half alpha. The blended cutoff uses
        // the same half-byte bias as the OpenGL owner.
        batch.terrain_flat_normals = false;
        batch.terrain_paint_type = 1;
        batch.terrain_alpha_ramp_texture = nullptr;
        batch.terrain_paint_texture = paint_map.get();
        batch.alpha_mask = true;
        batch.pbr_slots[0].alpha_cutoff = 0.75f;
        auto masked_out = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(masked_out);
        REQUIRE(device->waitUntilIdle(&error));
        albedo.fill(0);
        REQUIRE(masked_out->colorTexture(0)->readback(
            *device, 0, 0, {}, albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(albedo[center + 0] == 255);
        CHECK(albedo[center + 1] == 0);
        CHECK(albedo[center + 2] == 255);

        batch.alpha_mask = false;
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal world PBR terrain mask state is invalid");
        batch.alpha_mask = true;
        batch.pbr_slots[0].alpha_cutoff = 0.5f;
        error.clear();
        auto masked_in = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(masked_in);
        REQUIRE(device->waitUntilIdle(&error));
        albedo.fill(0);
        REQUIRE(masked_in->colorTexture(0)->readback(
            *device, 0, 0, {}, albedo.data(), 32 * 4, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(albedo[center + 0]) - 188) <= 1);
        CHECK(albedo[center + 1] == 255);
        CHECK(std::abs(static_cast<int>(albedo[center + 2]) - 137) <= 1);
    }
}

TEST_CASE("almetal: lit preview renders multiple indexed depth-tested batches", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto renderer = ALMetalLitPreviewRenderer::create(*device, &error);
        INFO(error);
        REQUIRE(renderer);

        // Two coincident triangles exercise indexed SoA fetch and depth. The
        // nearer triangle has an X normal/red light; the farther one has a Y
        // normal/green light and must not overwrite it.
        const std::array<std::array<float, 4>, 6> positions{{
            {{-0.8f, -0.8f,  0.5f, 1.f}},
            {{ 0.8f, -0.8f,  0.5f, 1.f}},
            {{ 0.0f,  0.8f,  0.5f, 1.f}},
            {{-0.8f, -0.8f, -0.5f, 1.f}},
            {{ 0.8f, -0.8f, -0.5f, 1.f}},
            {{ 0.0f,  0.8f, -0.5f, 1.f}},
        }};
        const std::array<std::array<float, 4>, 6> normals{{
            {{0.f, 1.f, 0.f, 0.f}}, {{0.f, 1.f, 0.f, 0.f}},
            {{0.f, 1.f, 0.f, 0.f}}, {{1.f, 0.f, 0.f, 0.f}},
            {{1.f, 0.f, 0.f, 0.f}}, {{1.f, 0.f, 0.f, 0.f}},
        }};
        const std::array<std::array<float, 2>, 6> texcoords{{
            {{0.f, 0.f}}, {{0.f, 0.f}}, {{0.f, 0.f}},
            {{0.f, 0.f}}, {{0.f, 0.f}}, {{0.f, 0.f}},
        }};
        const std::array<std::array<float, 4>, 6> skin_weights{{
            {{0.5f, 0.f, 0.f, 0.f}}, {{0.5f, 0.f, 0.f, 0.f}},
            {{0.5f, 0.f, 0.f, 0.f}}, {{0.5f, 0.f, 0.f, 0.f}},
            {{0.5f, 0.f, 0.f, 0.f}}, {{0.5f, 0.f, 0.f, 0.f}},
        }};
        const std::array<float, 6> avatar_weights{{
            0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
        }};
        const std::array<std::array<float, 4>, 6> avatar_clothing{{
            {{0.5f, 0.f, 0.f, 1.f}}, {{0.5f, 0.f, 0.f, 1.f}},
            {{0.5f, 0.f, 0.f, 1.f}}, {{0.5f, 0.f, 0.f, 1.f}},
            {{0.5f, 0.f, 0.f, 1.f}}, {{0.5f, 0.f, 0.f, 1.f}},
        }};
        const std::array<std::array<std::uint8_t, 4>, 6> rigged_colors{{
            {{128, 255, 255, 128}}, {{128, 255, 255, 128}},
            {{128, 255, 255, 128}}, {{128, 255, 255, 128}},
            {{128, 255, 255, 128}}, {{128, 255, 255, 128}},
        }};
        const std::array<std::array<std::uint8_t, 4>, 6> emissive_colors{{
            {{0, 0, 0, 128}}, {{0, 0, 0, 128}},
            {{0, 0, 0, 128}}, {{0, 0, 0, 128}},
            {{0, 0, 0, 128}}, {{0, 0, 0, 128}},
        }};
        constexpr std::size_t rigged_color_alignment_padding = 8;
        std::array<std::uint8_t,
                   sizeof(positions) + sizeof(normals) + sizeof(texcoords) +
                       sizeof(skin_weights) + sizeof(avatar_weights) +
                       sizeof(avatar_clothing) +
                       rigged_color_alignment_padding + sizeof(rigged_colors) +
                       sizeof(emissive_colors)>
            vertex_bytes{};
        std::memcpy(vertex_bytes.data(), positions.data(), sizeof(positions));
        std::memcpy(vertex_bytes.data() + sizeof(positions), normals.data(), sizeof(normals));
        std::memcpy(vertex_bytes.data() + sizeof(positions) + sizeof(normals),
                    texcoords.data(), sizeof(texcoords));
        const std::size_t skin_weight_offset =
            sizeof(positions) + sizeof(normals) + sizeof(texcoords);
        std::memcpy(vertex_bytes.data() + skin_weight_offset,
                    skin_weights.data(), sizeof(skin_weights));
        const std::size_t avatar_cloth_offset =
            skin_weight_offset + sizeof(skin_weights);
        std::memcpy(vertex_bytes.data() + avatar_cloth_offset,
                    avatar_clothing.data(), sizeof(avatar_clothing));
        const std::size_t avatar_weight_offset =
            avatar_cloth_offset + sizeof(avatar_clothing);
        std::memcpy(vertex_bytes.data() + avatar_weight_offset,
                    avatar_weights.data(), sizeof(avatar_weights));
        const std::size_t rigged_color_offset =
            avatar_weight_offset + sizeof(avatar_weights) +
            rigged_color_alignment_padding;
        std::memcpy(vertex_bytes.data() + rigged_color_offset,
                    rigged_colors.data(), sizeof(rigged_colors));
        const std::size_t emissive_color_offset =
            rigged_color_offset + sizeof(rigged_colors);
        std::memcpy(vertex_bytes.data() + emissive_color_offset,
                    emissive_colors.data(), sizeof(emissive_colors));
        const std::array<std::uint16_t, 9> indices{{
            0, 1, 2, 3, 4, 5,
            // The same near triangle with reversed winding verifies that the
            // explicit edge path preserves GL polygon-mode back-face culling.
            5, 4, 3,
        }};

        ALMetalBufferDescriptor vertex_descriptor;
        vertex_descriptor.length = vertex_bytes.size();
        vertex_descriptor.storage = ALMetalStorageMode::shared;
        vertex_descriptor.label = "Alchemy sculpt preview test vertices";
        auto vertices = ALMetalBuffer::create(*device, vertex_descriptor, &error);
        INFO(error);
        REQUIRE(vertices);
        REQUIRE(vertices->write(0, vertex_bytes.data(), vertex_bytes.size(), &error));

        ALMetalBufferDescriptor index_descriptor;
        index_descriptor.length = sizeof(indices);
        index_descriptor.storage = ALMetalStorageMode::shared;
        index_descriptor.label = "Alchemy sculpt preview test indices";
        auto index_buffer = ALMetalBuffer::create(*device, index_descriptor, &error);
        INFO(error);
        REQUIRE(index_buffer);
        REQUIRE(index_buffer->write(0, indices.data(), sizeof(indices), &error));

        // Convex decomposition meshes are CPU-owned, tightly-packed float3
        // triangle lists. Put the farther hull first so the output differs if
        // the required all-hull depth prepass is accidentally omitted.
        const std::array<std::array<float, 3>, 6> hull_positions{{
            {{-0.8f, -0.8f,  0.5f}},
            {{ 0.8f, -0.8f,  0.5f}},
            {{ 0.0f,  0.8f,  0.5f}},
            {{-0.8f, -0.8f, -0.5f}},
            {{ 0.8f, -0.8f, -0.5f}},
            {{ 0.0f,  0.8f, -0.5f}},
        }};
        ALMetalBufferDescriptor hull_descriptor;
        hull_descriptor.length = sizeof(hull_positions);
        hull_descriptor.storage = ALMetalStorageMode::shared;
        hull_descriptor.label = "Alchemy flat convex-hull preview vertices";
        auto hull_vertices = ALMetalBuffer::create(
            *device, hull_descriptor, &error);
        INFO(error);
        REQUIRE(hull_vertices);
        REQUIRE(hull_vertices->write(
            0, hull_positions.data(), sizeof(hull_positions), &error));

        ALMetalTextureDescriptor destination_descriptor;
        destination_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        destination_descriptor.width = 32;
        destination_descriptor.height = 32;
        destination_descriptor.storage = ALMetalStorageMode::private_gpu;
        destination_descriptor.usage = ALMetalTextureUsage::shader_read;
        destination_descriptor.label = "Alchemy sculpt preview destination template";
        auto destination_template = ALMetalTexture::create(
            *device, destination_descriptor, &error);
        INFO(error);
        REQUIRE(destination_template);

        ALMetalTextureDescriptor diffuse_descriptor;
        diffuse_descriptor.format = ALMetalPixelFormat::rgba8_unorm_srgb;
        diffuse_descriptor.width = 1;
        diffuse_descriptor.height = 1;
        diffuse_descriptor.storage = ALMetalStorageMode::private_gpu;
        diffuse_descriptor.usage = ALMetalTextureUsage::shader_read;
        diffuse_descriptor.label = "Alchemy textured preview encoded-color source";
        auto diffuse_texture = ALMetalTexture::create(
            *device, diffuse_descriptor, &error);
        INFO(error);
        REQUIRE(diffuse_texture);
        const std::array<std::uint8_t, 4> diffuse_pixel{{128, 64, 32, 255}};
        auto upload = device->makeCommandBuffer(
            "Alchemy textured preview upload", &error);
        INFO(error);
        REQUIRE(upload);
        REQUIRE(diffuse_texture->upload(
            *upload, 0, 0, {}, diffuse_pixel.data(), 4, &error));
        REQUIRE(upload->commitAndWait(&error));
        INFO(error);

        diffuse_descriptor.label =
            "Alchemy textured preview half-alpha encoded-color source";
        auto half_alpha_diffuse_texture = ALMetalTexture::create(
            *device, diffuse_descriptor, &error);
        INFO(error);
        REQUIRE(half_alpha_diffuse_texture);
        const std::array<std::uint8_t, 4> half_alpha_diffuse_pixel{{
            128, 64, 32, 128,
        }};
        auto half_alpha_upload = device->makeCommandBuffer(
            "Alchemy half-alpha textured preview upload", &error);
        INFO(error);
        REQUIRE(half_alpha_upload);
        REQUIRE(half_alpha_diffuse_texture->upload(
            *half_alpha_upload, 0, 0, {}, half_alpha_diffuse_pixel.data(), 4,
            &error));
        REQUIRE(half_alpha_upload->commitAndWait(&error));
        INFO(error);

        diffuse_descriptor.width = 2;
        diffuse_descriptor.label =
            "Alchemy PBR preview UV-transform source";
        auto pbr_transform_texture = ALMetalTexture::create(
            *device, diffuse_descriptor, &error);
        INFO(error);
        REQUIRE(pbr_transform_texture);
        const std::array<std::uint8_t, 8> pbr_transform_pixels{{
            128, 0, 0, 255,
            0, 128, 0, 255,
        }};
        auto pbr_transform_upload = device->makeCommandBuffer(
            "Alchemy PBR preview UV-transform upload", &error);
        INFO(error);
        REQUIRE(pbr_transform_upload);
        REQUIRE(pbr_transform_texture->upload(
            *pbr_transform_upload, 0, 0, {}, pbr_transform_pixels.data(), 8,
            &error));
        REQUIRE(pbr_transform_upload->commitAndWait(&error));
        INFO(error);

        diffuse_descriptor.label =
            "Alchemy PBR preview emissive UV-transform source";
        auto pbr_emissive_texture = ALMetalTexture::create(
            *device, diffuse_descriptor, &error);
        INFO(error);
        REQUIRE(pbr_emissive_texture);
        const std::array<std::uint8_t, 8> pbr_emissive_pixels{{
            0, 0, 0, 255,
            255, 255, 255, 255,
        }};
        auto pbr_emissive_upload = device->makeCommandBuffer(
            "Alchemy PBR preview emissive upload", &error);
        INFO(error);
        REQUIRE(pbr_emissive_upload);
        REQUIRE(pbr_emissive_texture->upload(
            *pbr_emissive_upload, 0, 0, {}, pbr_emissive_pixels.data(), 8,
            &error));
        REQUIRE(pbr_emissive_upload->commitAndWait(&error));
        INFO(error);

        ALMetalLitPreviewBatch batch;
        batch.vertices = vertices.get();
        batch.position_offset = 0;
        batch.normal_offset = sizeof(positions);
        batch.texcoord_offset = sizeof(positions) + sizeof(normals);
        batch.vertex_count = positions.size();
        batch.indices = index_buffer.get();
        batch.index_count = indices.size();
        batch.destination_template = destination_template.get();
        for (std::size_t index = 0; index < 4; ++index)
        {
            batch.modelview_projection[index * 4 + index] = 1.f;
        }
        for (std::size_t index = 0; index < 3; ++index)
        {
            batch.normal_matrix[index][index] = 1.f;
        }
        batch.light_direction[0][0] = 1.f;
        batch.light_direction[1][1] = 1.f;
        batch.light_diffuse[0][0] = 1.f;
        batch.light_diffuse[1][1] = 1.f;
        batch.diffuse_color[0] = 0.9f;
        batch.diffuse_color[1] = 0.9f;
        batch.diffuse_color[2] = 0.9f;
        batch.diffuse_color[3] = 1.f;

        batch.normal_offset = vertex_bytes.size();
        CHECK_FALSE(renderer->render(batch, &error));
        CHECK(error == "Metal lit preview geometry range exceeds its buffers");
        batch.normal_offset = sizeof(positions);
        error.clear();

        ALMetalLitPreviewBatch batches[3] = {batch, batch, batch};
        batches[0].index_count = 3;
        batches[1].index_offset = sizeof(std::uint16_t) * 3;
        batches[1].index_count = 3;
        batches[1].texture = diffuse_texture.get();
        std::memset(batches[0].light_direction, 0,
                    sizeof(batches[0].light_direction));
        std::memset(batches[0].light_diffuse, 0,
                    sizeof(batches[0].light_diffuse));
        batches[0].light_direction[0][1] = 1.f;
        batches[0].light_diffuse[0][1] = 1.f;
        std::memset(batches[1].light_direction, 0,
                    sizeof(batches[1].light_direction));
        std::memset(batches[1].light_diffuse, 0,
                    sizeof(batches[1].light_diffuse));
        batches[1].light_direction[0][0] = 1.f;
        batches[1].light_diffuse[0][0] = 1.f;
        batches[1].show_edges = true;
        batches[1].edge_color[0] = 1.f;
        batches[1].edge_color[1] = 0.f;
        batches[1].edge_color[2] = 0.f;
        batches[1].edge_color[3] = 1.f;
        batches[2].index_offset = sizeof(std::uint16_t) * 6;
        batches[2].index_count = 3;
        batches[2].show_edges = true;
        batches[2].edge_color[0] = 0.f;
        batches[2].edge_color[1] = 1.f;
        batches[2].edge_color[2] = 0.f;
        batches[2].edge_color[3] = 1.f;

        batches[1].index_count = 2;
        CHECK_FALSE(renderer->render(batches[1], &error));
        CHECK(error == "Metal lit preview edges require complete triangle indices");
        batches[1].index_count = 3;
        error.clear();

        batches[1].texcoord_offset = vertex_bytes.size();
        CHECK_FALSE(renderer->render(batches[1], &error));
        CHECK(error == "Metal lit preview geometry range exceeds its buffers");
        batches[1].texcoord_offset = sizeof(positions) + sizeof(normals);
        error.clear();

        ALMetalLitPreviewScene scene;
        scene.batches = batches;
        scene.batch_count = 3;
        scene.destination_descriptor = &destination_template->descriptor();
        scene.clear_color[0] = 0.169f;
        scene.clear_color[1] = 0.169f;
        scene.clear_color[2] = 0.169f;
        auto rendered = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(rendered);
        CHECK(alMetalHasUsage(rendered->descriptor().usage,
                              ALMetalTextureUsage::render_target));
        std::array<std::uint8_t, 32 * 32 * 4> pixels{};
        REQUIRE(rendered->readback(*device, 0, 0, {}, pixels.data(), 32 * 4, &error));
        INFO(error);

        const std::size_t center = (16 * 32 + 16) * 4;
        // The sRGB source is intentionally sampled through its linear UNORM
        // view, matching legacy AnisoWrap/GL_SKIP_DECODE. 128 * 0.9 = 115.2;
        // hardware sRGB decode would instead produce roughly 50 here.
        CHECK(std::abs(static_cast<int>(pixels[center + 0]) - 115) <= 1);
        CHECK(pixels[center + 1] <= 1);
        CHECK(pixels[center + 2] <= 1);
        CHECK(pixels[center + 3] == 255);
        std::size_t red_edge_pixels = 0;
        for (std::size_t offset = 0; offset < pixels.size(); offset += 4)
        {
            if (pixels[offset + 0] >= 254 && pixels[offset + 1] <= 1 &&
                pixels[offset + 2] <= 1 && pixels[offset + 3] == 255)
            {
                ++red_edge_pixels;
            }
        }
        CHECK(red_edge_pixels == 76);
        std::size_t back_face_edge_pixels = 0;
        for (std::size_t offset = 0; offset < pixels.size(); offset += 4)
        {
            if (pixels[offset + 0] <= 1 && pixels[offset + 1] >= 254 &&
                pixels[offset + 2] <= 1 && pixels[offset + 3] == 255)
            {
                ++back_face_edge_pixels;
            }
        }
        CHECK(back_face_edge_pixels == 0);
        const std::size_t corner = 4;
        CHECK(std::abs(static_cast<int>(pixels[corner + 0]) - 43) <= 1);
        CHECK(std::abs(static_cast<int>(pixels[corner + 1]) - 43) <= 1);
        CHECK(std::abs(static_cast<int>(pixels[corner + 2]) - 43) <= 1);
        CHECK(pixels[corner + 3] == 255);

        // The ordinary model-physics overlay clears depth after the model,
        // retains its color, then source-alpha blends the physics mesh. Reuse
        // the farther triangle: it was rejected above, so seeing it here also
        // proves that the explicit pass boundary cleared depth.
        std::array<ALMetalLitPreviewBatch, 4> overlay_batches{{
            batches[0], batches[1], batches[2], batch,
        }};
        ALMetalLitPreviewBatch& overlay = overlay_batches[3];
        overlay.index_offset = 0;
        overlay.index_count = 3;
        overlay.texture = nullptr;
        overlay.show_edges = false;
        overlay.clear_depth_before = true;
        overlay.alpha_blending = true;
        std::memset(overlay.light_direction, 0,
                    sizeof(overlay.light_direction));
        std::memset(overlay.light_diffuse, 0,
                    sizeof(overlay.light_diffuse));
        overlay.light_direction[0][1] = 1.f;
        overlay.light_diffuse[0][2] = 1.f;
        overlay.diffuse_color[0] = 1.f;
        overlay.diffuse_color[1] = 1.f;
        overlay.diffuse_color[2] = 1.f;
        overlay.diffuse_color[3] = 0.5f;

        scene.batches = overlay_batches.data();
        scene.batch_count = overlay_batches.size();
        auto rendered_overlay = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(rendered_overlay);
        std::array<std::uint8_t, 32 * 32 * 4> overlay_pixels{};
        REQUIRE(rendered_overlay->readback(
            *device, 0, 0, {}, overlay_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(overlay_pixels[center + 0]) - 58) <= 1);
        CHECK(overlay_pixels[center + 1] <= 1);
        CHECK(std::abs(static_cast<int>(overlay_pixels[center + 2]) - 128) <= 1);
        CHECK(std::abs(static_cast<int>(overlay_pixels[center + 3]) - 191) <= 1);

        ALMetalLitPreviewBatch far_hull;
        far_hull.vertices = hull_vertices.get();
        far_hull.vertex_count = 3;
        far_hull.geometry =
            ALMetalLitPreviewGeometry::unindexed_flat_triangles;
        far_hull.depth_prepass = true;
        far_hull.clear_depth_before = true;
        far_hull.alpha_blending = true;
        far_hull.destination_template = destination_template.get();
        far_hull.diffuse_color[0] = 0.f;
        far_hull.diffuse_color[1] = 1.f;
        far_hull.diffuse_color[2] = 0.f;
        far_hull.diffuse_color[3] = 0.5f;
        for (std::size_t index = 0; index < 4; ++index)
        {
            far_hull.modelview_projection[index * 4 + index] = 1.f;
        }
        ALMetalLitPreviewBatch near_hull = far_hull;
        near_hull.position_offset = sizeof(float) * 3 * 3;
        near_hull.clear_depth_before = false;
        near_hull.diffuse_color[0] = 1.f;
        near_hull.diffuse_color[1] = 0.f;

        far_hull.vertex_count = 2;
        CHECK_FALSE(renderer->render(far_hull, &error));
        CHECK(error == "Metal flat preview requires an unindexed triangle list");
        far_hull.vertex_count = 3;
        error.clear();

        std::array<ALMetalLitPreviewBatch, 2> hull_batches{{
            far_hull, near_hull,
        }};
        ALMetalLitPreviewScene hull_scene;
        hull_scene.batches = hull_batches.data();
        hull_scene.batch_count = hull_batches.size();
        hull_scene.destination_descriptor = &destination_template->descriptor();
        hull_scene.clear_color[0] = 0.169f;
        hull_scene.clear_color[1] = 0.169f;
        hull_scene.clear_color[2] = 0.169f;
        auto rendered_hulls = renderer->render(hull_scene, &error);
        INFO(error);
        REQUIRE(rendered_hulls);
        std::array<std::uint8_t, 32 * 32 * 4> hull_pixels{};
        REQUIRE(rendered_hulls->readback(
            *device, 0, 0, {}, hull_pixels.data(), 32 * 4, &error));
        INFO(error);
        // Only the nearer red hull contributes. If the far green hull were
        // blended before depth became complete, green would be about 75.
        CHECK(std::abs(static_cast<int>(hull_pixels[center + 0]) - 149) <= 1);
        CHECK(std::abs(static_cast<int>(hull_pixels[center + 1]) - 22) <= 1);
        CHECK(std::abs(static_cast<int>(hull_pixels[center + 2]) - 22) <= 1);
        CHECK(std::abs(static_cast<int>(hull_pixels[center + 3]) - 191) <= 1);

        // Joint diagnostics use a compact shared float3 stream for lines and
        // one renderer-owned unit sphere. Exercise both geometry modes and
        // the legacy read-only GL_GREATER pass used for occluded bones.
        const std::array<std::array<float, 3>, 2> line_positions{{
            {{-0.75f, 0.f, 0.f}}, {{0.75f, 0.f, 0.f}},
        }};
        ALMetalBufferDescriptor line_descriptor;
        line_descriptor.length = sizeof(line_positions);
        line_descriptor.storage = ALMetalStorageMode::shared;
        line_descriptor.label = "Alchemy diagnostic preview line vertices";
        auto line_vertices = ALMetalBuffer::create(
            *device, line_descriptor, &error);
        INFO(error);
        REQUIRE(line_vertices);
        REQUIRE(line_vertices->write(
            0, line_positions.data(), sizeof(line_positions), &error));

        ALMetalLitPreviewBatch line_batch;
        line_batch.vertices = line_vertices.get();
        line_batch.vertex_count = line_positions.size();
        line_batch.geometry =
            ALMetalLitPreviewGeometry::unindexed_flat_lines;
        line_batch.destination_template = destination_template.get();
        line_batch.diffuse_color[0] = 0.f;
        line_batch.diffuse_color[1] = 0.f;
        line_batch.diffuse_color[2] = 1.f;
        line_batch.diffuse_color[3] = 1.f;
        for (std::size_t index = 0; index < 4; ++index)
        {
            line_batch.modelview_projection[index * 4 + index] = 1.f;
        }
        line_batch.vertex_count = 1;
        CHECK_FALSE(renderer->render(line_batch, &error));
        CHECK(error == "Metal flat-line preview requires complete line segments");
        line_batch.vertex_count = line_positions.size();
        error.clear();
        auto rendered_line = renderer->render(line_batch, &error);
        INFO(error);
        REQUIRE(rendered_line);
        std::array<std::uint8_t, 32 * 32 * 4> line_pixels{};
        REQUIRE(rendered_line->readback(
            *device, 0, 0, {}, line_pixels.data(), 32 * 4, &error));
        INFO(error);
        std::size_t blue_line_pixels = 0;
        for (std::size_t offset = 0; offset < line_pixels.size(); offset += 4)
        {
            if (line_pixels[offset + 0] <= 1 &&
                line_pixels[offset + 1] <= 1 &&
                line_pixels[offset + 2] >= 254 &&
                line_pixels[offset + 3] == 255)
            {
                ++blue_line_pixels;
            }
        }
        CHECK(blue_line_pixels >= 20);

        ALMetalLitPreviewBatch sphere_batch;
        sphere_batch.geometry = ALMetalLitPreviewGeometry::unit_sphere;
        sphere_batch.destination_template = destination_template.get();
        sphere_batch.diffuse_color[0] = 0.f;
        sphere_batch.diffuse_color[1] = 1.f;
        sphere_batch.diffuse_color[2] = 1.f;
        sphere_batch.diffuse_color[3] = 1.f;
        sphere_batch.modelview_projection[0] = 0.25f;
        sphere_batch.modelview_projection[5] = 0.25f;
        sphere_batch.modelview_projection[10] = 0.25f;
        sphere_batch.modelview_projection[14] = 0.5f;
        sphere_batch.modelview_projection[15] = 1.f;

        ALMetalLitPreviewBatch near_surface = batch;
        near_surface.index_offset = sizeof(std::uint16_t) * 3;
        near_surface.index_count = 3;
        near_surface.texture = nullptr;
        near_surface.show_edges = false;
        sphere_batch.depth_mode =
            ALMetalLitPreviewDepthMode::greater_read_only;
        std::array<ALMetalLitPreviewBatch, 2> diagnostic_batches{{
            near_surface, sphere_batch,
        }};
        ALMetalLitPreviewScene diagnostic_scene;
        diagnostic_scene.batches = diagnostic_batches.data();
        diagnostic_scene.batch_count = diagnostic_batches.size();
        diagnostic_scene.destination_descriptor =
            &destination_template->descriptor();
        auto rendered_diagnostics = renderer->render(
            diagnostic_scene, &error);
        INFO(error);
        REQUIRE(rendered_diagnostics);
        std::array<std::uint8_t, 32 * 32 * 4> diagnostic_pixels{};
        REQUIRE(rendered_diagnostics->readback(
            *device, 0, 0, {}, diagnostic_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(diagnostic_pixels[center + 0] <= 1);
        CHECK(diagnostic_pixels[center + 1] >= 254);
        CHECK(diagnostic_pixels[center + 2] >= 254);
        CHECK(diagnostic_pixels[center + 3] == 255);

        // Reproduce the viewer's rebased one-joint object-skinning palette.
        // The joint contributes +0.25 X in its local translation and the
        // separate origin contributes another +0.25 X, moving the triangle
        // by exactly +0.5 in clip space. The textured and edge pipelines both
        // consume the same packed integer-index/fractional-weight stream.
        const std::array<float, 16> skin_palette{{
            0.25f, 0.f, 0.f, 0.f,
            1.f, 0.f, 0.f, 0.25f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
        }};
        ALMetalLitPreviewBatch skinned = batch;
        skinned.index_offset = 0;
        skinned.index_count = 3;
        skinned.texture = diffuse_texture.get();
        skinned.show_edges = true;
        skinned.weight_offset = skin_weight_offset;
        skinned.skin_palette = skin_palette.data();
        skinned.skin_palette_float_count = skin_palette.size();
        skinned.skin_joint_count = 1;
        skinned.edge_color[0] = 1.f;
        skinned.edge_color[1] = 0.f;
        skinned.edge_color[2] = 0.f;
        skinned.edge_color[3] = 1.f;

        auto rendered_skinned = renderer->render(skinned, &error);
        INFO(error);
        REQUIRE(rendered_skinned);
        std::array<std::uint8_t, 32 * 32 * 4> skinned_pixels{};
        REQUIRE(rendered_skinned->readback(
            *device, 0, 0, {}, skinned_pixels.data(), 32 * 4, &error));
        INFO(error);
        const std::size_t translated_interior = (16 * 32 + 24) * 4;
        CHECK(skinned_pixels[translated_interior + 0] <= 1);
        CHECK(std::abs(static_cast<int>(
            skinned_pixels[translated_interior + 1]) - 58) <= 1);
        CHECK(skinned_pixels[translated_interior + 2] <= 1);
        CHECK(skinned_pixels[translated_interior + 3] == 255);

        // Deferred rigged attachments decode both diffuse and vertex tint,
        // multiply them in linear space, and re-encode for the hint texture.
        // 128 sRGB multiplied by 128 sRGB becomes 61 sRGB, unlike an encoded
        // byte multiply (64). Accepted deferred coverage is opaque.
        ALMetalLitPreviewBatch deferred_rigged = skinned;
        deferred_rigged.show_edges = false;
        deferred_rigged.deferred_rigged = true;
        deferred_rigged.color_offset = rigged_color_offset;
        auto rendered_deferred_rigged = renderer->render(
            deferred_rigged, &error);
        INFO(error);
        REQUIRE(rendered_deferred_rigged);
        std::array<std::uint8_t, 32 * 32 * 4> deferred_rigged_pixels{};
        REQUIRE(rendered_deferred_rigged->readback(
            *device, 0, 0, {}, deferred_rigged_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
            deferred_rigged_pixels[translated_interior + 0]) - 61) <= 1);
        CHECK(deferred_rigged_pixels[translated_interior + 1] == 64);
        CHECK(deferred_rigged_pixels[translated_interior + 2] == 32);
        CHECK(deferred_rigged_pixels[translated_interior + 3] == 255);

        deferred_rigged.alpha_cutoff = 0.6f;
        auto rendered_deferred_mask = renderer->render(
            deferred_rigged, &error);
        INFO(error);
        REQUIRE(rendered_deferred_mask);
        std::array<std::uint8_t, 32 * 32 * 4> deferred_mask_pixels{};
        REQUIRE(rendered_deferred_mask->readback(
            *device, 0, 0, {}, deferred_mask_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(deferred_mask_pixels[translated_interior + 0] == 38);
        CHECK(deferred_mask_pixels[translated_interior + 1] == 51);
        CHECK(deferred_mask_pixels[translated_interior + 2] == 77);
        CHECK(deferred_mask_pixels[translated_interior + 3] == 255);

        // Legacy-material attachment 0 uses the same linear diffuse/tint
        // product, but its mask tests texture alpha alone. Vertex alpha is
        // unrelated to deferred coverage, which is stamped from depth.
        ALMetalLitPreviewBatch legacy_material = deferred_rigged;
        legacy_material.deferred_rigged = false;
        legacy_material.legacy_material_rigged = true;
        legacy_material.alpha_cutoff = 0.75f;
        auto rendered_legacy_material = renderer->render(
            legacy_material, &error);
        INFO(error);
        REQUIRE(rendered_legacy_material);
        std::array<std::uint8_t, 32 * 32 * 4> legacy_material_pixels{};
        REQUIRE(rendered_legacy_material->readback(
            *device, 0, 0, {}, legacy_material_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
            legacy_material_pixels[translated_interior + 0]) - 61) <= 1);
        CHECK(legacy_material_pixels[translated_interior + 1] == 64);
        CHECK(legacy_material_pixels[translated_interior + 2] == 32);
        CHECK(legacy_material_pixels[translated_interior + 3] == 255);

        legacy_material.texture = half_alpha_diffuse_texture.get();
        legacy_material.alpha_cutoff = 0.6f;
        auto rendered_legacy_material_discard = renderer->render(
            legacy_material, &error);
        INFO(error);
        REQUIRE(rendered_legacy_material_discard);
        std::array<std::uint8_t, 32 * 32 * 4>
            legacy_material_discard_pixels{};
        REQUIRE(rendered_legacy_material_discard->readback(
            *device, 0, 0, {}, legacy_material_discard_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(legacy_material_discard_pixels[translated_interior + 0] == 38);
        CHECK(legacy_material_discard_pixels[translated_interior + 1] == 51);
        CHECK(legacy_material_discard_pixels[translated_interior + 2] == 77);
        CHECK(legacy_material_discard_pixels[translated_interior + 3] == 255);

        // Opaque/masked PBR writes the same attachment-0 base-color product,
        // but its MASK cutoff uses texture alpha times the streamed
        // baseColorFactor alpha. The U8 vertex color is that streamed factor.
        ALMetalLitPreviewBatch pbr = deferred_rigged;
        pbr.deferred_rigged = false;
        pbr.pbr_rigged = true;
        pbr.alpha_cutoff = 0.f;
        auto rendered_pbr = renderer->render(pbr, &error);
        INFO(error);
        REQUIRE(rendered_pbr);
        std::array<std::uint8_t, 32 * 32 * 4> pbr_pixels{};
        REQUIRE(rendered_pbr->readback(
            *device, 0, 0, {}, pbr_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
            pbr_pixels[translated_interior + 0]) - 61) <= 1);
        CHECK(pbr_pixels[translated_interior + 1] == 64);
        CHECK(pbr_pixels[translated_interior + 2] == 32);
        CHECK(pbr_pixels[translated_interior + 3] == 255);

        // Blended PBR in an impostor bake is intentionally flat albedo, not
        // forward lighting: pbralphaF writes base colour and true coverage.
        // Reproduce the sRGB target's linear-space blend and the alpha pool's
        // src.a + (1-src.a)*dst.a coverage recurrence. Both texture and
        // streamed baseColorFactor alpha are 128/255 here.
        ALMetalLitPreviewBatch pbr_blend = pbr;
        pbr_blend.pbr_blend = true;
        pbr_blend.texture = half_alpha_diffuse_texture.get();
        pbr_blend.alpha_cutoff = 0.004f;
        ALMetalLitPreviewScene pbr_blend_scene;
        pbr_blend_scene.batches = &pbr_blend;
        pbr_blend_scene.batch_count = 1;
        pbr_blend_scene.destination_descriptor =
            &destination_template->descriptor();
        pbr_blend_scene.clear_color[0] = 0.169f;
        pbr_blend_scene.clear_color[1] = 0.2f;
        pbr_blend_scene.clear_color[2] = 0.3f;
        pbr_blend_scene.clear_color[3] = 0.f;
        auto rendered_pbr_blend = renderer->render(
            pbr_blend_scene, &error);
        INFO(error);
        REQUIRE(rendered_pbr_blend);
        std::array<std::uint8_t, 32 * 32 * 4> pbr_blend_pixels{};
        REQUIRE(rendered_pbr_blend->readback(
            *device, 0, 0, {}, pbr_blend_pixels.data(), 32 * 4, &error));
        INFO(error);
        const auto srgb_to_linear = [](float encoded)
        {
            return encoded <= 0.04045f
                ? encoded / 12.92f
                : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
        };
        const auto linear_to_srgb = [](float linear)
        {
            return linear <= 0.0031308f
                ? linear * 12.92f
                : 1.055f * std::pow(linear, 1.f / 2.4f) - 0.055f;
        };
        const float half = 128.f / 255.f;
        const float blend_coverage = half * half;
        const std::array<float, 3> texture_encoded{{
            128.f / 255.f, 64.f / 255.f, 32.f / 255.f,
        }};
        const std::array<float, 3> tint_encoded{{half, 1.f, 1.f}};
        const std::array<float, 3> clear_encoded{{
            43.f / 255.f, 51.f / 255.f, 77.f / 255.f,
        }};
        for (std::size_t channel = 0; channel < 3; ++channel)
        {
            const float source = srgb_to_linear(texture_encoded[channel]) *
                                 srgb_to_linear(tint_encoded[channel]);
            const float destination =
                srgb_to_linear(clear_encoded[channel]);
            const int expected = static_cast<int>(std::lround(
                linear_to_srgb(source * blend_coverage +
                    destination * (1.f - blend_coverage)) * 255.f));
            CHECK(std::abs(static_cast<int>(
                pbr_blend_pixels[translated_interior + channel]) -
                expected) <= 1);
        }
        CHECK(std::abs(static_cast<int>(
            pbr_blend_pixels[translated_interior + 3]) -
            static_cast<int>(std::lround(blend_coverage * 255.f))) <= 1);

        // Opaque PBR glow replaces the coverage stamp's alpha with emissive
        // luminance. Its texture transform is independent from base colour:
        // offset U=0.75 selects the white emissive texel, whose 0.75 factor
        // and 128/255 streamed TE glow produce exactly 96/255.
        ALMetalLitPreviewBatch pbr_glow = pbr;
        pbr_glow.pbr_glow = true;
        pbr_glow.pbr_emissive_texture = pbr_emissive_texture.get();
        pbr_glow.emissive_offset = emissive_color_offset;
        pbr_glow.pbr_emissive_transform[4] = 0.75f;
        pbr_glow.pbr_emissive_color[0] = 0.25f;
        pbr_glow.pbr_emissive_color[1] = 0.5f;
        pbr_glow.pbr_emissive_color[2] = 0.75f;
        auto rendered_pbr_glow = renderer->render(pbr_glow, &error);
        INFO(error);
        REQUIRE(rendered_pbr_glow);
        std::array<std::uint8_t, 32 * 32 * 4> pbr_glow_pixels{};
        REQUIRE(rendered_pbr_glow->readback(
            *device, 0, 0, {}, pbr_glow_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
            pbr_glow_pixels[translated_interior + 0]) - 61) <= 1);
        CHECK(pbr_glow_pixels[translated_interior + 1] == 64);
        CHECK(pbr_glow_pixels[translated_interior + 2] == 32);
        CHECK(std::abs(static_cast<int>(
            pbr_glow_pixels[translated_interior + 3]) - 96) <= 1);

        pbr_glow.pbr_emissive_texture = nullptr;
        error.clear();
        CHECK_FALSE(renderer->render(pbr_glow, &error));
        CHECK(error == "Metal PBR rigged preview data is invalid");

        pbr_glow.pbr_emissive_texture = pbr_emissive_texture.get();
        pbr_glow.pbr_blend = true;
        error.clear();
        CHECK_FALSE(renderer->render(pbr_glow, &error));
        CHECK(error == "Metal PBR rigged preview data is invalid");

        pbr.alpha_cutoff = 0.6f;
        auto rendered_pbr_mask = renderer->render(pbr, &error);
        INFO(error);
        REQUIRE(rendered_pbr_mask);
        std::array<std::uint8_t, 32 * 32 * 4> pbr_mask_pixels{};
        REQUIRE(rendered_pbr_mask->readback(
            *device, 0, 0, {}, pbr_mask_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(pbr_mask_pixels[translated_interior + 0] == 38);
        CHECK(pbr_mask_pixels[translated_interior + 1] == 51);
        CHECK(pbr_mask_pixels[translated_interior + 2] == 77);
        CHECK(pbr_mask_pixels[translated_interior + 3] == 255);

        // Mirror the PBR vertex shader's texture animation followed by the
        // handed KHR transform. UV 0 becomes 0.5 under texture_matrix0, then
        // scale 0.5 plus offset 0.5 selects the green texel at U=0.75. The
        // reversed triangle verifies material-driven double-sided culling.
        pbr.texture = pbr_transform_texture.get();
        pbr.alpha_cutoff = 0.f;
        pbr.index_offset = sizeof(std::uint16_t) * 6;
        pbr.index_count = 3;
        pbr.pbr_double_sided = true;
        pbr.pbr_base_color_transform[0] = 0.5f;
        pbr.pbr_base_color_transform[1] = 1.f;
        pbr.pbr_base_color_transform[4] = 0.5f;
        pbr.texture_matrix[12] = 0.5f;
        auto rendered_pbr_transform = renderer->render(pbr, &error);
        INFO(error);
        REQUIRE(rendered_pbr_transform);
        std::array<std::uint8_t, 32 * 32 * 4> pbr_transform_output{};
        REQUIRE(rendered_pbr_transform->readback(
            *device, 0, 0, {}, pbr_transform_output.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(pbr_transform_output[translated_interior + 0] <= 1);
        CHECK(std::abs(static_cast<int>(
            pbr_transform_output[translated_interior + 1]) - 128) <= 1);
        CHECK(pbr_transform_output[translated_interior + 2] <= 1);
        CHECK(pbr_transform_output[translated_interior + 3] == 255);

        pbr.pbr_base_color_transform[0] =
            std::numeric_limits<float>::quiet_NaN();
        error.clear();
        CHECK_FALSE(renderer->render(pbr, &error));
        CHECK(error == "Metal PBR rigged preview data is invalid");

        // The post-deferred fullbright mask uses texture alpha alone: outside
        // the alpha pool the U8 vertex alpha is legacy shininess. A 0.75
        // cutoff therefore keeps the opaque texel despite the 0.5 vertex
        // alpha. The dedicated vertex path must not require a normal stream.
        ALMetalLitPreviewBatch fullbright_mask = deferred_rigged;
        fullbright_mask.deferred_rigged = false;
        fullbright_mask.fullbright_mask_rigged = true;
        fullbright_mask.alpha_cutoff = 0.75f;
        fullbright_mask.normal_offset = vertex_bytes.size();
        auto rendered_fullbright_mask = renderer->render(
            fullbright_mask, &error);
        INFO(error);
        REQUIRE(rendered_fullbright_mask);
        std::array<std::uint8_t, 32 * 32 * 4> fullbright_mask_pixels{};
        REQUIRE(rendered_fullbright_mask->readback(
            *device, 0, 0, {}, fullbright_mask_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
            fullbright_mask_pixels[translated_interior + 0]) - 61) <= 1);
        CHECK(fullbright_mask_pixels[translated_interior + 1] == 64);
        CHECK(fullbright_mask_pixels[translated_interior + 2] == 32);
        CHECK(fullbright_mask_pixels[translated_interior + 3] == 255);

        fullbright_mask.texture = half_alpha_diffuse_texture.get();
        fullbright_mask.alpha_cutoff = 0.6f;
        auto rendered_fullbright_mask_discard = renderer->render(
            fullbright_mask, &error);
        INFO(error);
        REQUIRE(rendered_fullbright_mask_discard);
        std::array<std::uint8_t, 32 * 32 * 4>
            fullbright_mask_discard_pixels{};
        REQUIRE(rendered_fullbright_mask_discard->readback(
            *device, 0, 0, {}, fullbright_mask_discard_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(fullbright_mask_discard_pixels[translated_interior + 0] == 38);
        CHECK(fullbright_mask_discard_pixels[translated_interior + 1] == 51);
        CHECK(fullbright_mask_discard_pixels[translated_interior + 2] == 77);
        CHECK(fullbright_mask_discard_pixels[translated_interior + 3] == 255);

        // Ordinary post-deferred fullbright attachments blend in linear
        // space against the decoded sRGB destination. Both texture and
        // vertex alpha contribute to coverage, while the RGB-only GL color
        // mask preserves the destination alpha. With two half-alpha inputs,
        // the exact coverage is roughly one quarter.
        ALMetalLitPreviewBatch fullbright = fullbright_mask;
        fullbright.fullbright_mask_rigged = false;
        fullbright.fullbright_rigged = true;
        fullbright.alpha_cutoff = 0.f;
        auto rendered_fullbright = renderer->render(fullbright, &error);
        INFO(error);
        REQUIRE(rendered_fullbright);
        std::array<std::uint8_t, 32 * 32 * 4> fullbright_pixels{};
        REQUIRE(rendered_fullbright->readback(
            *device, 0, 0, {}, fullbright_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(
            fullbright_pixels[translated_interior + 0]) - 45) <= 1);
        CHECK(std::abs(static_cast<int>(
            fullbright_pixels[translated_interior + 1]) - 55) <= 1);
        CHECK(std::abs(static_cast<int>(
            fullbright_pixels[translated_interior + 2]) - 69) <= 1);
        CHECK(fullbright_pixels[translated_interior + 3] == 255);

        fullbright.fullbright_mask_rigged = true;
        error.clear();
        CHECK_FALSE(renderer->render(fullbright, &error));
        CHECK(error == "Metal fullbright mask rigged preview data is invalid");

        deferred_rigged.color_offset = vertex_bytes.size();
        error.clear();
        CHECK_FALSE(renderer->render(deferred_rigged, &error));
        CHECK(error == "Metal deferred rigged preview data is invalid");

        // Keep local mesh detail out of the region-scale add. A single
        // pre-MVP addition at 2^24 collapses the triangle's positive-X half;
        // the rebased two-term transform preserves this interior sample.
        std::array<float, 16> precision_palette = skin_palette;
        constexpr float large_origin = 16777216.f;
        precision_palette[0] = large_origin;
        precision_palette[7] = 0.f;
        ALMetalLitPreviewBatch precision_skinned = skinned;
        precision_skinned.skin_palette = precision_palette.data();
        precision_skinned.texture = nullptr;
        precision_skinned.show_edges = false;
        precision_skinned.modelview_projection[12] = -large_origin;
        auto rendered_precision = renderer->render(precision_skinned, &error);
        INFO(error);
        REQUIRE(rendered_precision);
        std::array<std::uint8_t, 32 * 32 * 4> precision_pixels{};
        REQUIRE(rendered_precision->readback(
            *device, 0, 0, {}, precision_pixels.data(), 32 * 4, &error));
        INFO(error);
        std::size_t preserved_local_pixels = 0;
        for (std::size_t pixel = 0; pixel < precision_pixels.size(); pixel += 4)
        {
            if (precision_pixels[pixel + 0] <= 1 &&
                std::abs(static_cast<int>(precision_pixels[pixel + 1]) - 230) <= 1 &&
                precision_pixels[pixel + 2] <= 1 &&
                precision_pixels[pixel + 3] == 255)
            {
                ++preserved_local_pixels;
            }
        }
        CHECK(preserved_local_pixels > 50);

        // Exercise the legacy system-avatar scalar weight convention used by
        // clothing/body previews. Weight 0.5 blends joint 0 identity with a
        // joint 1 transform translated +0.5 X, producing +0.25 X. The chosen
        // sample lies inside only the translated triangle.
        std::array<float, 45 * 4> avatar_palette{};
        for (std::size_t joint = 0; joint < 15; ++joint)
        {
            avatar_palette[joint * 4 + 0] = 1.f;
            avatar_palette[(15 + joint) * 4 + 1] = 1.f;
            avatar_palette[(30 + joint) * 4 + 2] = 1.f;
        }
        avatar_palette[1 * 4 + 3] = 0.5f;

        ALMetalLitPreviewBatch avatar_skinned = batch;
        avatar_skinned.index_offset = 0;
        avatar_skinned.index_count = 3;
        avatar_skinned.weight_offset = avatar_weight_offset;
        avatar_skinned.skin_palette = avatar_palette.data();
        avatar_skinned.skin_palette_float_count = avatar_palette.size();
        avatar_skinned.skin_joint_count = 15;
        avatar_skinned.avatar_skinning = true;
        std::memset(avatar_skinned.light_direction, 0,
                    sizeof(avatar_skinned.light_direction));
        std::memset(avatar_skinned.light_diffuse, 0,
                    sizeof(avatar_skinned.light_diffuse));
        avatar_skinned.light_direction[0][1] = 1.f;
        avatar_skinned.light_diffuse[0][0] = 1.f;
        std::fill(std::begin(avatar_skinned.diffuse_color),
                  std::end(avatar_skinned.diffuse_color), 1.f);

        auto rendered_avatar = renderer->render(avatar_skinned, &error);
        INFO(error);
        REQUIRE(rendered_avatar);
        std::array<std::uint8_t, 32 * 32 * 4> avatar_pixels{};
        REQUIRE(rendered_avatar->readback(
            *device, 0, 0, {}, avatar_pixels.data(), 32 * 4, &error));
        INFO(error);
        const std::size_t avatar_translated_interior = (16 * 32 + 24) * 4;
        CHECK(avatar_pixels[avatar_translated_interior + 0] >= 254);
        CHECK(avatar_pixels[avatar_translated_interior + 1] <= 1);
        CHECK(avatar_pixels[avatar_translated_interior + 2] <= 1);
        CHECK(avatar_pixels[avatar_translated_interior + 3] == 255);

        avatar_skinned.texture = diffuse_texture.get();
        auto rendered_avatar_textured = renderer->render(
            avatar_skinned, &error);
        INFO(error);
        REQUIRE(rendered_avatar_textured);
        std::array<std::uint8_t, 32 * 32 * 4> avatar_textured_pixels{};
        REQUIRE(rendered_avatar_textured->readback(
            *device, 0, 0, {}, avatar_textured_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(avatar_textured_pixels[avatar_translated_interior + 0] > 0);
        CHECK(avatar_textured_pixels[avatar_translated_interior + 3] == 255);

        ALMetalLitPreviewBatch avatar_skinned_unlit = avatar_skinned;
        avatar_skinned_unlit.avatar_unlit = true;
        auto rendered_avatar_skinned_unlit = renderer->render(
            avatar_skinned_unlit, &error);
        INFO(error);
        REQUIRE(rendered_avatar_skinned_unlit);
        std::array<std::uint8_t, 32 * 32 * 4> avatar_skinned_unlit_pixels{};
        REQUIRE(rendered_avatar_skinned_unlit->readback(
            *device, 0, 0, {}, avatar_skinned_unlit_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(avatar_skinned_unlit_pixels[avatar_translated_interior + 0] == 128);
        CHECK(avatar_skinned_unlit_pixels[avatar_translated_interior + 1] == 64);
        CHECK(avatar_skinned_unlit_pixels[avatar_translated_interior + 2] == 32);
        CHECK(avatar_skinned_unlit_pixels[avatar_translated_interior + 3] == 255);

        // Match avatarSkinV.glsl's optional clothing stream. With zero phase,
        // frequency, gravity and view-depth terms, wind strength one plus the
        // source shader's minimax sine approximation makes clothing.x move
        // every vertex slightly more than +0.5 before the palette adds its
        // existing +0.25 translation.
        const std::size_t avatar_cloth_interior = (16 * 32 + 28) * 4;
        CHECK(avatar_skinned_unlit_pixels[avatar_cloth_interior + 0] == 38);
        CHECK(avatar_skinned_unlit_pixels[avatar_cloth_interior + 1] == 51);
        CHECK(avatar_skinned_unlit_pixels[avatar_cloth_interior + 2] == 77);
        CHECK(avatar_skinned_unlit_pixels[avatar_cloth_interior + 3] == 255);
        ALMetalLitPreviewBatch avatar_cloth = avatar_skinned_unlit;
        avatar_cloth.cloth_offset = avatar_cloth_offset;
        avatar_cloth.avatar_cloth = true;
        avatar_cloth.avatar_wind[3] = 1.f;
        auto rendered_avatar_cloth = renderer->render(avatar_cloth, &error);
        INFO(error);
        REQUIRE(rendered_avatar_cloth);
        std::array<std::uint8_t, 32 * 32 * 4> avatar_cloth_pixels{};
        REQUIRE(rendered_avatar_cloth->readback(
            *device, 0, 0, {}, avatar_cloth_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(avatar_cloth_pixels[avatar_cloth_interior + 0] == 128);
        CHECK(avatar_cloth_pixels[avatar_cloth_interior + 1] == 64);
        CHECK(avatar_cloth_pixels[avatar_cloth_interior + 2] == 32);
        CHECK(avatar_cloth_pixels[avatar_cloth_interior + 3] == 255);

        avatar_cloth.cloth_offset = vertex_bytes.size();
        error.clear();
        CHECK_FALSE(renderer->render(avatar_cloth, &error));
        CHECK(error == "Metal avatar cloth preview data is invalid");

        // Rigid avatar parts use the same one-light/wrapped-back response and
        // alpha-tested texture path without a palette or weight stream.
        ALMetalLitPreviewBatch rigid_avatar = batch;
        rigid_avatar.index_offset = 0;
        rigid_avatar.index_count = 3;
        rigid_avatar.avatar_lighting = true;
        rigid_avatar.modelview_projection[12] = 0.25f;
        std::memset(rigid_avatar.light_direction, 0,
                    sizeof(rigid_avatar.light_direction));
        std::memset(rigid_avatar.light_diffuse, 0,
                    sizeof(rigid_avatar.light_diffuse));
        rigid_avatar.light_direction[0][1] = 1.f;
        rigid_avatar.light_diffuse[0][2] = 1.f;
        std::fill(std::begin(rigid_avatar.diffuse_color),
                  std::end(rigid_avatar.diffuse_color), 1.f);
        auto rendered_rigid_avatar = renderer->render(rigid_avatar, &error);
        INFO(error);
        REQUIRE(rendered_rigid_avatar);
        std::array<std::uint8_t, 32 * 32 * 4> rigid_avatar_pixels{};
        REQUIRE(rendered_rigid_avatar->readback(
            *device, 0, 0, {}, rigid_avatar_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(rigid_avatar_pixels[avatar_translated_interior + 0] <= 1);
        CHECK(rigid_avatar_pixels[avatar_translated_interior + 1] <= 1);
        CHECK(rigid_avatar_pixels[avatar_translated_interior + 2] >= 254);
        CHECK(rigid_avatar_pixels[avatar_translated_interior + 3] == 255);

        rigid_avatar.texture = diffuse_texture.get();
        auto rendered_rigid_avatar_textured = renderer->render(
            rigid_avatar, &error);
        INFO(error);
        REQUIRE(rendered_rigid_avatar_textured);

        rigid_avatar.avatar_lighting = false;
        rigid_avatar.avatar_unlit = true;
        auto rendered_rigid_avatar_unlit = renderer->render(
            rigid_avatar, &error);
        INFO(error);
        REQUIRE(rendered_rigid_avatar_unlit);
        std::array<std::uint8_t, 32 * 32 * 4> rigid_avatar_unlit_pixels{};
        REQUIRE(rendered_rigid_avatar_unlit->readback(
            *device, 0, 0, {}, rigid_avatar_unlit_pixels.data(), 32 * 4,
            &error));
        INFO(error);
        CHECK(rigid_avatar_unlit_pixels[avatar_translated_interior + 0] == 128);
        CHECK(rigid_avatar_unlit_pixels[avatar_translated_interior + 1] == 64);
        CHECK(rigid_avatar_unlit_pixels[avatar_translated_interior + 2] == 32);
        CHECK(rigid_avatar_unlit_pixels[avatar_translated_interior + 3] == 255);

        ALMetalLitPreviewScene avatar_hint_scene;
        avatar_hint_scene.batches = &rigid_avatar;
        avatar_hint_scene.batch_count = 1;
        avatar_hint_scene.background_texture = diffuse_texture.get();
        avatar_hint_scene.destination_descriptor =
            &destination_template->descriptor();
        auto rendered_avatar_hint = renderer->render(avatar_hint_scene, &error);
        INFO(error);
        REQUIRE(rendered_avatar_hint);
        std::array<std::uint8_t, 32 * 32 * 4> avatar_hint_pixels{};
        REQUIRE(rendered_avatar_hint->readback(
            *device, 0, 0, {}, avatar_hint_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(avatar_hint_pixels[corner + 0] == 128);
        CHECK(avatar_hint_pixels[corner + 1] == 64);
        CHECK(avatar_hint_pixels[corner + 2] == 32);
        CHECK(avatar_hint_pixels[corner + 3] == 255);

        rigid_avatar.show_edges = true;
        CHECK_FALSE(renderer->render(rigid_avatar, &error));
        CHECK(error == "Metal avatar preview mode is invalid");
        rigid_avatar.show_edges = false;
        rigid_avatar.avatar_lighting = true;
        CHECK_FALSE(renderer->render(rigid_avatar, &error));
        CHECK(error == "Metal avatar preview mode is invalid");
        rigid_avatar.avatar_lighting = false;
        rigid_avatar.texture = nullptr;
        CHECK_FALSE(renderer->render(rigid_avatar, &error));
        CHECK(error == "Metal avatar preview mode is invalid");
        rigid_avatar.texture = diffuse_texture.get();
        rigid_avatar.skin_palette = avatar_palette.data();
        rigid_avatar.skin_palette_float_count = avatar_palette.size();
        rigid_avatar.skin_joint_count = 15;
        CHECK_FALSE(renderer->render(rigid_avatar, &error));
        CHECK(error == "Metal avatar preview mode is invalid");

        avatar_skinned.skin_joint_count = 14;
        CHECK_FALSE(renderer->render(avatar_skinned, &error));
        CHECK(error == "Metal lit preview skinning data is invalid");
        avatar_skinned.skin_joint_count = 15;
        avatar_skinned.show_edges = true;
        error.clear();
        CHECK_FALSE(renderer->render(avatar_skinned, &error));
        CHECK(error == "Metal lit preview skinning data is invalid");

        skinned.skin_joint_count = 0;
        CHECK_FALSE(renderer->render(skinned, &error));
        CHECK(error == "Metal lit preview skinning data is invalid");

        ALMetalTextureDescriptor srgb_destination =
            destination_template->descriptor();
        srgb_destination.format = ALMetalPixelFormat::rgba8_unorm_srgb;
        srgb_destination.label = "Alchemy sRGB preview destination descriptor";
        scene.destination_descriptor = &srgb_destination;
        error.clear();
        auto rendered_srgb = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(rendered_srgb);
        CHECK(rendered_srgb->descriptor().format ==
              ALMetalPixelFormat::rgba8_unorm_srgb);
    }
}

TEST_CASE("almetal: BC texture transfer honors block rows and edge dimensions", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        ALMetalTextureDescriptor descriptor;
        descriptor.format = ALMetalPixelFormat::bc1_rgba;
        descriptor.width = 7;
        descriptor.height = 5;
        descriptor.storage = ALMetalStorageMode::private_gpu;
        descriptor.label = "Alchemy BC1 transfer texture";
        auto texture = ALMetalTexture::create(*device, descriptor, &error);
        INFO(error);
        REQUIRE(texture);
        CHECK(texture->isCompressed());
        CHECK(texture->blockWidth() == 4);
        CHECK(texture->blockHeight() == 4);
        CHECK(texture->bytesPerBlock() == 8);
        CHECK(texture->bytesPerPixel() == 0);

        constexpr std::size_t row_pitch = 2 * 8;
        std::array<std::uint8_t, row_pitch * 2> source{};
        for (std::size_t index = 0; index < source.size(); ++index)
        {
            source[index] = static_cast<std::uint8_t>(index * 13u + 5u);
        }
        auto command = device->makeCommandBuffer("Alchemy BC1 upload", &error);
        INFO(error);
        REQUIRE(command);
        REQUIRE(texture->upload(*command, 0, 0, {}, source.data(), row_pitch, &error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);

        std::array<std::uint8_t, row_pitch * 2> result{};
        REQUIRE(texture->readback(*device, 0, 0, {}, result.data(), row_pitch, &error));
        INFO(error);
        CHECK(result == source);
    }
}

TEST_CASE("almetal: memoryless MSAA target resolves while depth remains transient", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        ALMetalRenderTargetDescriptor target_descriptor;
        target_descriptor.width = 13;
        target_descriptor.height = 7;
        target_descriptor.sample_count = 4;
        target_descriptor.color_attachment_count = 1;
        target_descriptor.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
        target_descriptor.colors[0].label = "Alchemy MSAA color";
        target_descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        target_descriptor.memoryless_multisample = true;
        target_descriptor.memoryless_depth_stencil = true;
        target_descriptor.label = "Alchemy MSAA target";
        auto target = ALMetalRenderTarget::create(*device, target_descriptor, &error);
        INFO(error);
        REQUIRE(target);
        REQUIRE(target->colorTexture(0));
        REQUIRE(target->multisampleColorTexture(0));
        REQUIRE(target->depthStencilTexture());
        CHECK(target->multisampleColorTexture(0)->descriptor().storage == ALMetalStorageMode::memoryless);
        CHECK(target->depthStencilTexture()->descriptor().storage == ALMetalStorageMode::memoryless);

        ALMetalRenderPassConfig pass_config;
        pass_config.colors[0].load = ALMetalLoadAction::clear;
        pass_config.colors[0].store = ALMetalStoreAction::multisample_resolve;
        pass_config.colors[0].clear = {0.2, 0.4, 0.6, 1.0};
        pass_config.depth_load = ALMetalLoadAction::clear;
        pass_config.depth_store = ALMetalStoreAction::dont_care;
        pass_config.clear_depth = 0.0;
        auto pass = target->makeRenderPass(pass_config, &error);
        INFO(error);
        REQUIRE(pass);

        auto command = device->makeCommandBuffer("Alchemy MSAA resolve test", &error);
        INFO(error);
        REQUIRE(command);
        id<MTLRenderCommandEncoder> encoder = [nativeCommand(*command)
            renderCommandEncoderWithDescriptor:(__bridge MTLRenderPassDescriptor*)pass->nativeHandle()];
        REQUIRE(encoder);
        [encoder endEncoding];
        REQUIRE(command->commitAndWait(&error));
        INFO(error);

        std::array<std::uint8_t, 13 * 7 * 4> pixels{};
        REQUIRE(target->colorTexture(0)->readback(*device, 0, 0, {}, pixels.data(), 13 * 4, &error));
        INFO(error);
        CHECK(std::abs(static_cast<int>(pixels[0]) - 153) <= 1);
        CHECK(std::abs(static_cast<int>(pixels[1]) - 102) <= 1);
        CHECK(std::abs(static_cast<int>(pixels[2]) - 51) <= 1);
        CHECK(pixels[3] == 255);
    }
}

TEST_CASE("almetal: frame scheduler bounds work and recovers abandoned slots", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto scheduler = ALMetalFrameScheduler::create(*device, 2, &error);
        INFO(error);
        REQUIRE(scheduler);

        auto first = scheduler->beginFrame("Alchemy scheduled frame one", 100, &error);
        auto abandoned = scheduler->beginFrame("Alchemy abandoned frame", 100, &error);
        REQUIRE(first);
        REQUIRE(abandoned);
        auto blocked = scheduler->beginFrame("Alchemy blocked frame", 1, &error);
        CHECK_FALSE(blocked);

        abandoned.reset();
        auto second = scheduler->beginFrame("Alchemy scheduled frame two", 100, &error);
        INFO(error);
        REQUIRE(second);
        CHECK(first->slot() != second->slot());
        REQUIRE(scheduler->submit(*first, &error));
        REQUIRE(scheduler->submit(*second, &error));
        REQUIRE(scheduler->waitUntilIdle(1000, &error));
        INFO(error);

        const ALMetalFrameStats stats = scheduler->stats();
        CHECK(stats.maximum_in_flight == 2);
        CHECK(stats.begun == 3);
        CHECK(stats.submitted == 2);
        CHECK(stats.completed == 2);
        CHECK(stats.failed == 0);
        CHECK(stats.in_flight == 0);
    }
}

TEST_CASE("almetal: bundled MSL compute pipeline dispatches through typed bindings", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto library = ALMetalShaderLibrary::loadBuiltin(*device, &error);
        INFO(error);
        REQUIRE(library);
        CHECK(library->hasFunction("almetal_test_compute"));

        ALMetalComputePipelineDescriptor pipeline_descriptor;
        pipeline_descriptor.library = library.get();
        pipeline_descriptor.function = "almetal_test_compute";
        pipeline_descriptor.label = "Alchemy typed compute test";
        auto pipeline = ALMetalComputePipeline::create(*device, pipeline_descriptor, &error);
        INFO(error);
        REQUIRE(pipeline);
        CHECK(pipeline->threadExecutionWidth() > 0);

        constexpr std::uint32_t count = 257;
        std::array<std::uint32_t, count> values{};
        for (std::uint32_t index = 0; index < count; ++index)
        {
            values[index] = index * 5u + 1u;
        }
        ALMetalBufferDescriptor buffer_descriptor;
        buffer_descriptor.length = sizeof(values);
        buffer_descriptor.storage = ALMetalStorageMode::shared;
        buffer_descriptor.label = "Alchemy compute values";
        auto buffer = ALMetalBuffer::create(*device, buffer_descriptor, &error);
        INFO(error);
        REQUIRE(buffer);
        REQUIRE(buffer->write(0, values.data(), sizeof(values), &error));

        auto command = device->makeCommandBuffer("Alchemy typed compute dispatch", &error);
        INFO(error);
        REQUIRE(command);
        auto encoder = ALMetalComputeEncoder::begin(*command, "Alchemy compute encoder", &error);
        INFO(error);
        REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline, &error));
        REQUIRE(encoder->setBuffer(0, *buffer, 0, &error));
        const std::uint32_t addend = 19;
        REQUIRE(encoder->setBytes(1, &addend, sizeof(addend), &error));
        const std::uint32_t group_width = std::min(
            pipeline->threadExecutionWidth(), pipeline->maximumTotalThreadsPerThreadgroup());
        REQUIRE(encoder->dispatch(count, 1, 1, group_width, 1, 1, &error));
        REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);

        std::array<std::uint32_t, count> result{};
        REQUIRE(buffer->read(0, result.data(), sizeof(result), &error));
        for (std::uint32_t index = 0; index < count; ++index)
        {
            CHECK(result[index] == values[index] * 3u + addend);
        }
    }
}

TEST_CASE("almetal: bundled MSL render pipeline draws a typed triangle", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto library = ALMetalShaderLibrary::loadBuiltin(*device, &error);
        INFO(error);
        REQUIRE(library);
        CHECK(library->hasFunction("almetal_test_vertex"));
        CHECK(library->hasFunction("almetal_test_fragment"));

        ALMetalRenderTargetDescriptor target_descriptor;
        target_descriptor.width = 32;
        target_descriptor.height = 32;
        target_descriptor.color_attachment_count = 1;
        target_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        target_descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        target_descriptor.label = "Alchemy typed triangle target";
        auto target = ALMetalRenderTarget::create(*device, target_descriptor, &error);
        INFO(error);
        REQUIRE(target);

        ALMetalRenderPipelineDescriptor pipeline_descriptor;
        pipeline_descriptor.library = library.get();
        pipeline_descriptor.vertex_function = "almetal_test_vertex";
        pipeline_descriptor.fragment_function = "almetal_test_fragment";
        pipeline_descriptor.color_attachment_count = 1;
        pipeline_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        pipeline_descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        pipeline_descriptor.label = "Alchemy typed triangle pipeline";
        auto pipeline = ALMetalRenderPipeline::create(*device, pipeline_descriptor, &error);
        INFO(error);
        REQUIRE(pipeline);

        ALMetalDepthStencilDescriptor depth_descriptor;
        depth_descriptor.depth_compare = ALMetalCompareFunction::less_equal;
        depth_descriptor.depth_write_enabled = true;
        depth_descriptor.label = "Alchemy typed triangle depth";
        auto depth_state = ALMetalDepthStencilState::create(*device, depth_descriptor, &error);
        INFO(error);
        REQUIRE(depth_state);

        ALMetalSamplerDescriptor sampler_descriptor;
        sampler_descriptor.max_anisotropy = 8;
        sampler_descriptor.label = "Alchemy linear repeat sampler";
        auto sampler = ALMetalSamplerState::create(*device, sampler_descriptor, &error);
        INFO(error);
        REQUIRE(sampler);

        struct Vertex
        {
            float position[2];
            float color[4];
        };
        const std::array<Vertex, 3> vertices{{
            {{-0.9f, -0.9f}, {0.125f, 0.5f, 0.875f, 1.0f}},
            {{ 0.9f, -0.9f}, {0.125f, 0.5f, 0.875f, 1.0f}},
            {{ 0.0f,  0.9f}, {0.125f, 0.5f, 0.875f, 1.0f}},
        }};
        ALMetalBufferDescriptor vertex_descriptor;
        vertex_descriptor.length = sizeof(vertices);
        vertex_descriptor.storage = ALMetalStorageMode::shared;
        vertex_descriptor.label = "Alchemy typed triangle vertices";
        auto vertex_buffer = ALMetalBuffer::create(*device, vertex_descriptor, &error);
        INFO(error);
        REQUIRE(vertex_buffer);
        REQUIRE(vertex_buffer->write(0, vertices.data(), sizeof(vertices), &error));

        ALMetalRenderPassConfig pass_config;
        pass_config.colors[0].load = ALMetalLoadAction::clear;
        pass_config.colors[0].store = ALMetalStoreAction::store;
        pass_config.colors[0].clear = {0.0, 0.0, 0.0, 1.0};
        pass_config.depth_load = ALMetalLoadAction::clear;
        pass_config.depth_store = ALMetalStoreAction::dont_care;
        pass_config.clear_depth = 1.0;
        auto pass = target->makeRenderPass(pass_config, &error);
        INFO(error);
        REQUIRE(pass);

        auto command = device->makeCommandBuffer("Alchemy typed triangle draw", &error);
        INFO(error);
        REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Alchemy triangle encoder", &error);
        INFO(error);
        REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline, &error));
        REQUIRE(encoder->setDepthStencilState(depth_state.get(), &error));
        REQUIRE(encoder->setVertexBuffer(0, *vertex_buffer, 0, &error));
        REQUIRE(encoder->setFragmentSampler(0, sampler.get(), &error));
        REQUIRE(encoder->setViewport({0.0, 0.0, 32.0, 32.0, 0.0, 1.0}, &error));
        REQUIRE(encoder->setScissor({0, 0, 32, 32}, &error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
        REQUIRE(encoder->setFrontFacingWinding(ALMetalWinding::counter_clockwise, &error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);

        std::array<std::uint8_t, 32 * 32 * 4> pixels{};
        REQUIRE(target->colorTexture(0)->readback(*device, 0, 0, {}, pixels.data(), 32 * 4, &error));
        INFO(error);
        const std::size_t center = (16 * 32 + 16) * 4;
        CHECK(std::abs(static_cast<int>(pixels[center + 0]) - 32) <= 1);
        CHECK(std::abs(static_cast<int>(pixels[center + 1]) - 128) <= 1);
        CHECK(std::abs(static_cast<int>(pixels[center + 2]) - 223) <= 1);
        CHECK(pixels[center + 3] == 255);
    }
}

TEST_CASE("almetal: typed depth-stencil state preserves masked stencil semantics", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto library = ALMetalShaderLibrary::loadBuiltin(*device, &error);
        INFO(error);
        REQUIRE(library);

        ALMetalRenderTargetDescriptor target_descriptor;
        target_descriptor.width = 32;
        target_descriptor.height = 32;
        target_descriptor.color_attachment_count = 1;
        target_descriptor.colors[0].format =
            ALMetalPixelFormat::rgba8_unorm;
        target_descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        target_descriptor.label = "Alchemy typed stencil target";
        auto target = ALMetalRenderTarget::create(
            *device, target_descriptor, &error);
        INFO(error);
        REQUIRE(target);

        ALMetalRenderPipelineDescriptor pipeline_descriptor;
        pipeline_descriptor.library = library.get();
        pipeline_descriptor.vertex_function = "almetal_test_vertex";
        pipeline_descriptor.fragment_function = "almetal_test_fragment";
        pipeline_descriptor.color_attachment_count = 1;
        pipeline_descriptor.colors[0].format =
            ALMetalPixelFormat::rgba8_unorm;
        pipeline_descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        pipeline_descriptor.label = "Alchemy typed stencil color pipeline";
        auto color_pipeline = ALMetalRenderPipeline::create(
            *device, pipeline_descriptor, &error);
        INFO(error);
        REQUIRE(color_pipeline);
        pipeline_descriptor.colors[0].write_mask =
            ALMetalColorWriteMask::none;
        pipeline_descriptor.label = "Alchemy typed stencil mark pipeline";
        auto mark_pipeline = ALMetalRenderPipeline::create(
            *device, pipeline_descriptor, &error);
        INFO(error);
        REQUIRE(mark_pipeline);

        ALMetalDepthStencilDescriptor mark_descriptor;
        mark_descriptor.depth_compare = ALMetalCompareFunction::always;
        mark_descriptor.depth_write_enabled = false;
        mark_descriptor.stencil_enabled = true;
        mark_descriptor.front_stencil.depth_stencil_pass =
            ALMetalStencilOperation::replace;
        mark_descriptor.front_stencil.read_mask = 0x1u;
        mark_descriptor.front_stencil.write_mask = 0x1u;
        mark_descriptor.back_stencil = mark_descriptor.front_stencil;
        mark_descriptor.label = "Alchemy typed stencil mark";
        auto mark_state = ALMetalDepthStencilState::create(
            *device, mark_descriptor, &error);
        INFO(error);
        REQUIRE(mark_state);

        ALMetalDepthStencilDescriptor test_descriptor;
        test_descriptor.depth_compare = ALMetalCompareFunction::always;
        test_descriptor.depth_write_enabled = false;
        test_descriptor.stencil_enabled = true;
        test_descriptor.front_stencil.compare =
            ALMetalCompareFunction::equal;
        test_descriptor.front_stencil.read_mask = 0x1u;
        test_descriptor.front_stencil.write_mask = 0u;
        test_descriptor.back_stencil = test_descriptor.front_stencil;
        test_descriptor.label = "Alchemy typed stencil test";
        auto test_state = ALMetalDepthStencilState::create(
            *device, test_descriptor, &error);
        INFO(error);
        REQUIRE(test_state);

        struct Vertex
        {
            float position[2];
            float color[4];
        };
        const auto make_vertices = [](const std::array<float, 4>& color)
        {
            return std::array<Vertex, 3>{{
                {{-0.9f, -0.9f}, {color[0], color[1], color[2], color[3]}},
                {{ 0.9f, -0.9f}, {color[0], color[1], color[2], color[3]}},
                {{ 0.0f,  0.9f}, {color[0], color[1], color[2], color[3]}},
            }};
        };
        const auto green_vertices = make_vertices({0.f, 1.f, 0.f, 1.f});
        const auto red_vertices = make_vertices({1.f, 0.f, 0.f, 1.f});
        ALMetalBufferDescriptor buffer_descriptor;
        buffer_descriptor.length = sizeof(green_vertices);
        buffer_descriptor.storage = ALMetalStorageMode::shared;
        buffer_descriptor.label = "Alchemy typed stencil vertices";
        auto green_buffer = ALMetalBuffer::create(
            *device, buffer_descriptor, &error);
        auto red_buffer = ALMetalBuffer::create(
            *device, buffer_descriptor, &error);
        INFO(error);
        REQUIRE(green_buffer);
        REQUIRE(red_buffer);
        REQUIRE(green_buffer->write(
            0, green_vertices.data(), sizeof(green_vertices), &error));
        REQUIRE(red_buffer->write(
            0, red_vertices.data(), sizeof(red_vertices), &error));

        ALMetalRenderPassConfig pass_config;
        pass_config.colors[0].load = ALMetalLoadAction::clear;
        pass_config.colors[0].store = ALMetalStoreAction::store;
        pass_config.colors[0].clear = {0.0, 0.0, 0.0, 1.0};
        pass_config.depth_load = ALMetalLoadAction::clear;
        pass_config.depth_store = ALMetalStoreAction::dont_care;
        pass_config.clear_depth = 1.0;
        pass_config.stencil_load = ALMetalLoadAction::clear;
        pass_config.stencil_store = ALMetalStoreAction::store;
        pass_config.clear_stencil = 0;
        auto pass = target->makeRenderPass(pass_config, &error);
        INFO(error);
        REQUIRE(pass);

        auto command = device->makeCommandBuffer(
            "Alchemy typed stencil draw", &error);
        INFO(error);
        REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(
            *command, *pass, "Alchemy typed stencil encoder", &error);
        INFO(error);
        REQUIRE(encoder);
        REQUIRE(encoder->setViewport(
            {0.0, 0.0, 32.0, 32.0, 0.0, 1.0}, &error));
        REQUIRE(encoder->setScissor({0, 0, 32, 32}, &error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));

        // Reference 3 is masked to bit zero, exactly like the water mask's
        // independent EXCLUDE_BIT and SURFACE_BIT transactions.
        REQUIRE(encoder->setPipeline(*mark_pipeline, &error));
        REQUIRE(encoder->setDepthStencilState(mark_state.get(), &error));
        REQUIRE(encoder->setStencilReferenceValue(3u, &error));
        REQUIRE(encoder->setVertexBuffer(0, *green_buffer, 0, &error));
        REQUIRE(encoder->draw(
            ALMetalPrimitiveType::triangle, 0, 3, 1, &error));

        REQUIRE(encoder->setPipeline(*color_pipeline, &error));
        REQUIRE(encoder->setDepthStencilState(test_state.get(), &error));
        REQUIRE(encoder->setStencilReferenceValue(1u, &error));
        REQUIRE(encoder->setVertexBuffer(0, *green_buffer, 0, &error));
        REQUIRE(encoder->draw(
            ALMetalPrimitiveType::triangle, 0, 3, 1, &error));

        // The zero reference must fail over the marked triangle. If stencil
        // comparison or the read mask is lost, this red draw overwrites it.
        REQUIRE(encoder->setStencilReferenceValue(0u, &error));
        REQUIRE(encoder->setVertexBuffer(0, *red_buffer, 0, &error));
        REQUIRE(encoder->draw(
            ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);

        std::array<std::uint8_t, 32 * 32 * 4> pixels{};
        REQUIRE(target->colorTexture(0)->readback(
            *device, 0, 0, {}, pixels.data(), 32 * 4, &error));
        INFO(error);
        const std::size_t center = (16 * 32 + 16) * 4;
        CHECK(pixels[center + 0] == 0);
        CHECK(pixels[center + 1] == 255);
        CHECK(pixels[center + 2] == 0);
        CHECK(pixels[center + 3] == 255);
        CHECK(pixels[0] == 0);
        CHECK(pixels[1] == 0);
        CHECK(pixels[2] == 0);
        CHECK(pixels[3] == 255);
    }
}

TEST_CASE("almetal: a native quad patch tessellates through the typed render encoder", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        const std::string source = R"metal(
#include <metal_stdlib>
using namespace metal;

struct TessellatedVertex
{
    float4 position [[position]];
    float2 uv;
};

[[patch(quad, 4)]]
vertex TessellatedVertex almetal_test_tessellated_quad(
    const device float2* control_points [[buffer(0)]],
    uint patch_id [[patch_id]],
    float2 uv [[position_in_patch]])
{
    const uint base = patch_id * 4;
    const float2 bottom = mix(control_points[base], control_points[base + 1], uv.x);
    const float2 top = mix(control_points[base + 3], control_points[base + 2], uv.x);

    TessellatedVertex result;
    result.position = float4(mix(bottom, top, uv.y), 0.0f, 1.0f);
    result.uv = uv;
    return result;
}

fragment float4 almetal_test_tessellated_color(TessellatedVertex input [[stage_in]])
{
    return float4(input.uv, 0.25f, 1.0f);
}
)metal";
        auto library = ALMetalShaderLibrary::compileSource(
            *device, source, "Alchemy tessellation test library", &error);
        INFO(error);
        REQUIRE(library);

        ALMetalRenderPipelineDescriptor pipeline_descriptor;
        pipeline_descriptor.library = library.get();
        pipeline_descriptor.vertex_function = "almetal_test_tessellated_quad";
        pipeline_descriptor.fragment_function = "almetal_test_tessellated_color";
        pipeline_descriptor.color_attachment_count = 1;
        pipeline_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        pipeline_descriptor.tessellation.enabled = true;
        pipeline_descriptor.tessellation.partition_mode =
            ALMetalTessellationPartitionMode::integer;
        pipeline_descriptor.tessellation.maximum_factor = 16;
        pipeline_descriptor.tessellation.factor_scale_enabled = true;
        pipeline_descriptor.tessellation.factor_step_function =
            ALMetalTessellationFactorStepFunction::per_patch;
        pipeline_descriptor.tessellation.output_winding =
            ALMetalTessellationOutputWinding::counter_clockwise;
        pipeline_descriptor.label = "Alchemy typed quad tessellation pipeline";

        ALMetalRenderPipelineDescriptor invalid_descriptor = pipeline_descriptor;
        invalid_descriptor.tessellation.maximum_factor = 65;
        CHECK_FALSE(ALMetalRenderPipeline::create(*device, invalid_descriptor, &error));
        CHECK_FALSE(error.empty());
        error.clear();

        auto pipeline = ALMetalRenderPipeline::create(*device, pipeline_descriptor, &error);
        INFO(error);
        REQUIRE(pipeline);
        CHECK(pipeline->descriptor().tessellation.enabled);
        CHECK(pipeline->descriptor().tessellation.maximum_factor == 16);

        ALMetalRenderTargetDescriptor target_descriptor;
        target_descriptor.width = 32;
        target_descriptor.height = 32;
        target_descriptor.color_attachment_count = 1;
        target_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        target_descriptor.label = "Alchemy tessellation target";
        auto target = ALMetalRenderTarget::create(*device, target_descriptor, &error);
        INFO(error);
        REQUIRE(target);

        const std::array<std::array<float, 2>, 4> control_points{{
            {{-0.8f, -0.8f}},
            {{ 0.8f, -0.8f}},
            {{ 0.8f,  0.8f}},
            {{-0.8f,  0.8f}},
        }};
        ALMetalBufferDescriptor control_point_descriptor;
        control_point_descriptor.length = sizeof(control_points);
        control_point_descriptor.storage = ALMetalStorageMode::shared;
        control_point_descriptor.label = "Alchemy quad control points";
        auto control_point_buffer = ALMetalBuffer::create(
            *device, control_point_descriptor, &error);
        INFO(error);
        REQUIRE(control_point_buffer);
        REQUIRE(control_point_buffer->write(
            0, control_points.data(), sizeof(control_points), &error));

        // Four outer and two inner half-precision factors, each set to 4.0.
        std::array<std::uint16_t, 128> tessellation_factors{};
        std::fill_n(tessellation_factors.begin(), 6, std::uint16_t{0x4400});
        ALMetalBufferDescriptor factor_descriptor;
        factor_descriptor.length = sizeof(tessellation_factors);
        factor_descriptor.storage = ALMetalStorageMode::shared;
        factor_descriptor.label = "Alchemy quad tessellation factors";
        auto factor_buffer = ALMetalBuffer::create(*device, factor_descriptor, &error);
        INFO(error);
        REQUIRE(factor_buffer);
        REQUIRE(factor_buffer->write(
            0, tessellation_factors.data(), sizeof(tessellation_factors), &error));

        ALMetalRenderPassConfig pass_config;
        pass_config.colors[0].load = ALMetalLoadAction::clear;
        pass_config.colors[0].store = ALMetalStoreAction::store;
        pass_config.colors[0].clear = {0.0, 0.0, 0.0, 1.0};
        auto pass = target->makeRenderPass(pass_config, &error);
        INFO(error);
        REQUIRE(pass);

        auto command = device->makeCommandBuffer("Alchemy typed quad tessellation", &error);
        INFO(error);
        REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(
            *command, *pass, "Alchemy tessellation encoder", &error);
        INFO(error);
        REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline, &error));
        REQUIRE(encoder->setVertexBuffer(0, *control_point_buffer, 0, &error));
        REQUIRE(encoder->setViewport({0.0, 0.0, 32.0, 32.0, 0.0, 1.0}, &error));
        REQUIRE(encoder->setScissor({0, 0, 32, 32}, &error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));

        CHECK_FALSE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        CHECK_FALSE(encoder->drawPatches(4, 0, 1, 1, 0, &error));
        CHECK_FALSE(encoder->setTessellationFactorBuffer(
            *factor_buffer, factor_buffer->length(), 0, &error));
        CHECK_FALSE(encoder->setTessellationFactorBuffer(*factor_buffer, 2, 0, &error));
        CHECK_FALSE(encoder->setTessellationFactorBuffer(*factor_buffer, 0, 2, &error));
        CHECK_FALSE(encoder->setTessellationFactorScale(
            std::numeric_limits<float>::quiet_NaN(), &error));
        error.clear();

        REQUIRE(encoder->setTessellationFactorBuffer(*factor_buffer, 0, 0, &error));
        REQUIRE(encoder->setTessellationFactorScale(1.0f, &error));
        REQUIRE(encoder->drawPatches(4, 0, 1, 1, 0, &error));
        REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);

        std::array<std::uint8_t, 32 * 32 * 4> pixels{};
        REQUIRE(target->colorTexture(0)->readback(
            *device, 0, 0, {}, pixels.data(), 32 * 4, &error));
        INFO(error);
        const std::size_t center = (16 * 32 + 16) * 4;
        CHECK(std::abs(static_cast<int>(pixels[center + 0]) - 128) <= 9);
        CHECK(std::abs(static_cast<int>(pixels[center + 1]) - 128) <= 9);
        CHECK(std::abs(static_cast<int>(pixels[center + 2]) - 64) <= 1);
        CHECK(pixels[center + 3] == 255);

        const std::size_t outside = (1 * 32 + 1) * 4;
        CHECK(pixels[outside + 0] == 0);
        CHECK(pixels[outside + 1] == 0);
        CHECK(pixels[outside + 2] == 0);
        CHECK(pixels[outside + 3] == 255);
    }
}

TEST_CASE("almetal: PBR material preview preserves glTF texture semantics", "[almetal]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto renderer = ALMetalPBRPreviewRenderer::create(*device, &error);
        INFO(error);
        REQUIRE(renderer);

        constexpr std::size_t position_offset = 0;
        constexpr std::size_t normal_offset =
            position_offset + sizeof(float) * 4 * 3;
        constexpr std::size_t texcoord_offset =
            normal_offset + sizeof(float) * 4 * 3;
        constexpr std::size_t color_offset =
            texcoord_offset + sizeof(float) * 2 * 3;
        constexpr std::size_t tangent_offset =
            (color_offset + sizeof(std::uint8_t) * 4 * 3 + 15) & ~std::size_t{15};
        std::array<std::uint8_t,
                   tangent_offset + sizeof(float) * 4 * 3> vertex_bytes{};
        const float positions[3][4] = {
            {-1.f, -1.f, 0.f, 1.f},
            { 3.f, -1.f, 0.f, 1.f},
            {-1.f,  3.f, 0.f, 1.f},
        };
        const float normals[3][4] = {
            {0.f, 0.f, 1.f, 0.f},
            {0.f, 0.f, 1.f, 0.f},
            {0.f, 0.f, 1.f, 0.f},
        };
        const float texcoords[3][2] = {
            {0.25f, 0.5f}, {0.25f, 0.5f}, {0.25f, 0.5f},
        };
        const std::uint8_t colors[3][4] = {
            {255, 255, 255, 255},
            {255, 255, 255, 255},
            {255, 255, 255, 255},
        };
        const float tangents[3][4] = {
            {1.f, 0.f, 0.f, 1.f},
            {1.f, 0.f, 0.f, 1.f},
            {1.f, 0.f, 0.f, 1.f},
        };
        std::memcpy(vertex_bytes.data() + position_offset,
                    positions, sizeof(positions));
        std::memcpy(vertex_bytes.data() + normal_offset,
                    normals, sizeof(normals));
        std::memcpy(vertex_bytes.data() + texcoord_offset,
                    texcoords, sizeof(texcoords));
        std::memcpy(vertex_bytes.data() + color_offset,
                    colors, sizeof(colors));
        std::memcpy(vertex_bytes.data() + tangent_offset,
                    tangents, sizeof(tangents));

        ALMetalBufferDescriptor vertex_descriptor;
        vertex_descriptor.length = vertex_bytes.size();
        vertex_descriptor.storage = ALMetalStorageMode::shared;
        vertex_descriptor.label = "Alchemy PBR preview test vertices";
        auto vertex_buffer = ALMetalBuffer::create(
            *device, vertex_descriptor, &error);
        INFO(error);
        REQUIRE(vertex_buffer);
        REQUIRE(vertex_buffer->write(
            0, vertex_bytes.data(), vertex_bytes.size(), &error));

        const std::array<std::uint16_t, 3> indices{{0, 1, 2}};
        ALMetalBufferDescriptor index_descriptor;
        index_descriptor.length = sizeof(indices);
        index_descriptor.storage = ALMetalStorageMode::shared;
        index_descriptor.label = "Alchemy PBR preview test indices";
        auto index_buffer = ALMetalBuffer::create(
            *device, index_descriptor, &error);
        INFO(error);
        REQUIRE(index_buffer);
        REQUIRE(index_buffer->write(
            0, indices.data(), sizeof(indices), &error));

        auto make_texture = [&](ALMetalPixelFormat format,
                                std::uint32_t width,
                                const std::uint8_t* bytes,
                                std::size_t bytes_per_row,
                                const char* label)
        {
            ALMetalTextureDescriptor descriptor;
            descriptor.format = format;
            descriptor.width = width;
            descriptor.height = 1;
            descriptor.storage = ALMetalStorageMode::private_gpu;
            descriptor.usage = ALMetalTextureUsage::shader_read;
            descriptor.label = label;
            auto texture = ALMetalTexture::create(*device, descriptor, &error);
            INFO(error);
            REQUIRE(texture);
            auto upload = device->makeCommandBuffer(label, &error);
            INFO(error);
            REQUIRE(upload);
            REQUIRE(texture->upload(
                *upload, 0, 0, {}, bytes, bytes_per_row, &error));
            REQUIRE(upload->commitAndWait(&error));
            INFO(error);
            return texture;
        };

        const std::array<std::uint8_t, 4> opaque_black{{0, 0, 0, 255}};
        const std::array<std::uint8_t, 4> opaque_white{{255, 255, 255, 255}};
        const std::array<std::uint8_t, 4> half_alpha_black{{0, 0, 0, 128}};
        const std::array<std::uint8_t, 4> flat_normal{{128, 128, 255, 255}};
        const std::array<std::uint8_t, 4> zero_ambient_orm{{0, 255, 0, 255}};
        const std::array<std::uint8_t, 4> glossy_metal_orm{{255, 0, 255, 255}};
        const std::array<std::uint8_t, 4> rough_metal_orm{{255, 255, 255, 255}};
        const std::array<std::uint8_t, 4> black_emissive{{0, 0, 0, 255}};
        const std::array<std::uint8_t, 8> red_green_emissive{{
            255, 0, 0, 255, 0, 255, 0, 255,
        }};
        auto base = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 1,
            opaque_black.data(), 4, "Alchemy PBR preview test base");
        auto white_base = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 1,
            opaque_white.data(), 4,
            "Alchemy PBR preview direct-light test base");
        auto cutout_base = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 1,
            half_alpha_black.data(), 4,
            "Alchemy PBR preview test cutout base");
        auto normal = make_texture(
            ALMetalPixelFormat::rgba8_unorm, 1,
            flat_normal.data(), 4, "Alchemy PBR preview test normal");
        auto orm = make_texture(
            ALMetalPixelFormat::rgba8_unorm, 1,
            zero_ambient_orm.data(), 4, "Alchemy PBR preview test ORM");
        auto glossy_metal = make_texture(
            ALMetalPixelFormat::rgba8_unorm, 1,
            glossy_metal_orm.data(), 4,
            "Alchemy PBR preview glossy-metal test ORM");
        auto rough_metal = make_texture(
            ALMetalPixelFormat::rgba8_unorm, 1,
            rough_metal_orm.data(), 4,
            "Alchemy PBR preview rough-metal test ORM");
        auto emissive = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 2,
            red_green_emissive.data(), 8,
            "Alchemy PBR preview test emissive");
        auto unlit_emissive = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 1,
            black_emissive.data(), 4,
            "Alchemy PBR preview direct-light test emissive");

        ALMetalTextureDescriptor environment_descriptor;
        environment_descriptor.type = ALMetalTextureType::texture_cube;
        environment_descriptor.format = ALMetalPixelFormat::rgba16_float;
        environment_descriptor.width = 1;
        environment_descriptor.height = 1;
        environment_descriptor.storage = ALMetalStorageMode::private_gpu;
        environment_descriptor.usage = ALMetalTextureUsage::shader_read;
        environment_descriptor.label =
            "Alchemy PBR preview test environment";
        auto environment_texture = ALMetalTexture::create(
            *device, environment_descriptor, &error);
        INFO(error);
        REQUIRE(environment_texture);
        const std::array<std::uint16_t, 4> black_environment{{0, 0, 0, 0}};
        auto environment_upload = device->makeCommandBuffer(
            "Alchemy PBR preview test environment upload", &error);
        INFO(error);
        REQUIRE(environment_upload);
        for (std::uint32_t face = 0; face < 6; ++face)
        {
            REQUIRE(environment_texture->upload(
                *environment_upload, 0, face, {},
                black_environment.data(), sizeof(black_environment),
                &error));
        }
        REQUIRE(environment_upload->commitAndWait(&error));
        INFO(error);

        ALMetalTextureDescriptor filtered_environment_descriptor =
            environment_descriptor;
        filtered_environment_descriptor.width = 2;
        filtered_environment_descriptor.height = 2;
        filtered_environment_descriptor.mipmap_level_count = 2;
        filtered_environment_descriptor.label =
            "Alchemy PBR preview filtered test environment";
        auto filtered_environment = ALMetalTexture::create(
            *device, filtered_environment_descriptor, &error);
        INFO(error);
        REQUIRE(filtered_environment);
        auto filtered_environment_upload = device->makeCommandBuffer(
            "Alchemy PBR preview filtered environment upload", &error);
        INFO(error);
        REQUIRE(filtered_environment_upload);
        for (std::uint32_t face = 0; face < 6; ++face)
        {
            std::array<std::uint16_t, 2 * 2 * 4> mip0{};
            for (std::size_t texel = 0; texel < 4; ++texel)
            {
                // Metal cube slice 4 is +Z. The smooth lookup at this test's
                // centre must select it, rather than any blue neighbour.
                mip0[texel * 4 + (face == 4 ? 0 : 2)] = 0x3c00;
                mip0[texel * 4 + 3] = 0x3c00;
            }
            const std::array<std::uint16_t, 4> mip1{{
                0, 0x3c00, 0, 0x3c00,
            }};
            REQUIRE(filtered_environment->upload(
                *filtered_environment_upload, 0, face, {},
                mip0.data(), 2 * 4 * sizeof(std::uint16_t), &error));
            REQUIRE(filtered_environment->upload(
                *filtered_environment_upload, 1, face, {},
                mip1.data(), 4 * sizeof(std::uint16_t), &error));
        }
        REQUIRE(filtered_environment_upload->commitAndWait(&error));
        INFO(error);

        ALMetalTextureDescriptor destination_descriptor;
        destination_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        destination_descriptor.width = 32;
        destination_descriptor.height = 32;
        destination_descriptor.storage = ALMetalStorageMode::private_gpu;
        destination_descriptor.usage = ALMetalTextureUsage::shader_read;
        destination_descriptor.label = "Alchemy PBR preview test output";

        ALMetalPBRPreviewBatch batch;
        batch.vertices = vertex_buffer.get();
        batch.position_offset = position_offset;
        batch.normal_offset = normal_offset;
        batch.texcoord_offset = texcoord_offset;
        batch.color_offset = color_offset;
        batch.tangent_offset = tangent_offset;
        batch.vertex_count = 3;
        batch.indices = index_buffer.get();
        batch.index_type = ALMetalIndexType::uint16;
        batch.index_count = 3;
        batch.base_color_texture = base.get();
        batch.normal_texture = normal.get();
        batch.orm_texture = orm.get();
        batch.emissive_texture = emissive.get();
        const float identity[16] = {
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        std::memcpy(batch.modelview_projection, identity, sizeof(identity));
        std::memcpy(batch.modelview, identity, sizeof(identity));
        batch.modelview[14] = -2.f;
        batch.normal_matrix[0][0] = 1.f;
        batch.normal_matrix[1][1] = 1.f;
        batch.normal_matrix[2][2] = 1.f;
        batch.emissive_color[0] = 1.f;
        batch.emissive_color[1] = 1.f;
        batch.emissive_color[2] = 1.f;
        batch.sun_color[0] = 0.f;
        batch.sun_color[1] = 0.f;
        batch.sun_color[2] = 0.f;
        batch.metallic_factor = 0.f;
        batch.roughness_factor = 1.f;
        batch.alpha_cutoff = 0.f;
        batch.double_sided = true;

        ALMetalPBRPreviewScene scene;
        ALMetalPBRPreviewEnvironment environment;
        environment.radiance_texture = environment_texture.get();
        scene.batches = &batch;
        scene.batch_count = 1;
        scene.destination_descriptor = &destination_descriptor;
        scene.environment = &environment;
        scene.exposure = 1.f;
        scene.tonemap_mix = 0.f;
        scene.tonemap_type = 1;

        auto read_center = [&](const ALMetalTexture& texture)
        {
            std::array<std::uint8_t, 32 * 32 * 4> pixels{};
            REQUIRE(texture.readback(
                *device, 0, 0, {}, pixels.data(), 32 * 4, &error));
            INFO(error);
            const std::size_t center = (16 * 32 + 16) * 4;
            return std::array<std::uint8_t, 4>{{
                pixels[center], pixels[center + 1],
                pixels[center + 2], pixels[center + 3],
            }};
        };

        auto red_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(red_output);
        const auto red = read_center(*red_output);
        CHECK(red[0] == 255);
        CHECK(red[1] == 0);
        CHECK(red[2] == 0);
        CHECK(red[3] == 255);

        batch.texture_transform[3][4] = 0.5f;
        auto green_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(green_output);
        const auto green = read_center(*green_output);
        CHECK(green[0] == 0);
        CHECK(green[1] == 255);
        CHECK(green[2] == 0);
        CHECK(green[3] == 255);

        destination_descriptor.format =
            ALMetalPixelFormat::rgba8_unorm_srgb;
        auto srgb_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(srgb_output);
        CHECK(srgb_output->descriptor().format ==
              ALMetalPixelFormat::rgba8_unorm_srgb);
        const auto srgb_green = read_center(*srgb_output);
        CHECK(srgb_green[0] == 0);
        CHECK(srgb_green[1] == 255);
        CHECK(srgb_green[2] == 0);
        CHECK(srgb_green[3] == 255);
        destination_descriptor.format = ALMetalPixelFormat::rgba8_unorm;

        // At normal incidence with perceptual roughness 1, this is the GL
        // forward path's height-correlated GGX plus its generated 1024-sample
        // BRDF-table energy compensation, its real sun-disc widening and the
        // shared punctual scale of 3. The table's half-float quantization and
        // encoded output allow a two-byte envelope around the analytic 132.
        batch.base_color_texture = white_base.get();
        batch.emissive_texture = unlit_emissive.get();
        batch.sun_color[0] = 0.25f;
        batch.sun_color[1] = 0.25f;
        batch.sun_color[2] = 0.25f;
        auto direct_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(direct_output);
        const auto direct = read_center(*direct_output);
        CHECK(std::abs(static_cast<int>(direct[0]) - 132) <= 2);
        CHECK(std::abs(static_cast<int>(direct[1]) - 132) <= 2);
        CHECK(std::abs(static_cast<int>(direct[2]) - 132) <= 2);
        CHECK(direct[3] == 255);
        batch.base_color_texture = base.get();
        batch.emissive_texture = emissive.get();
        batch.sun_color[0] = 0.f;
        batch.sun_color[1] = 0.f;
        batch.sun_color[2] = 0.f;

        // A coherent environment carries the cube's face orientation and
        // roughness-filtered mip chain. The centre reflection points at +Z:
        // smooth metal reads red from slice 4/mip 0, while rough metal reads
        // the green final mip shared by every face.
        environment.radiance_texture = filtered_environment.get();
        environment.max_lod = 1.f;
        batch.base_color_texture = white_base.get();
        batch.emissive_texture = unlit_emissive.get();
        batch.orm_texture = glossy_metal.get();
        batch.metallic_factor = 1.f;
        auto smooth_environment_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(smooth_environment_output);
        const auto smooth_environment = read_center(
            *smooth_environment_output);
        CHECK(smooth_environment[0] > 200);
        CHECK(smooth_environment[1] < 4);
        CHECK(smooth_environment[2] < 4);

        batch.orm_texture = rough_metal.get();
        auto rough_environment_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(rough_environment_output);
        const auto rough_environment = read_center(*rough_environment_output);
        CHECK(rough_environment[0] < 4);
        CHECK(rough_environment[1] > 60);
        CHECK(rough_environment[2] < 4);

        // The packed SH row is consumed in the same coefficient order as
        // reflectionProbeF.glsl. A green unit L0 coefficient therefore adds
        // diffuse green while a black radiance cube contributes no specular.
        environment.radiance_texture = environment_texture.get();
        environment.max_lod = 0.f;
        environment.sh_coefficients[0][0] = 0x3c000000u;
        batch.metallic_factor = 0.f;
        auto sh_environment_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(sh_environment_output);
        const auto sh_environment = read_center(*sh_environment_output);
        CHECK(sh_environment[0] < 4);
        CHECK(sh_environment[1] > 100);
        CHECK(sh_environment[2] < 4);

        environment.sh_coefficients[0][0] = 0;
        batch.base_color_texture = base.get();
        batch.emissive_texture = emissive.get();
        batch.orm_texture = orm.get();
        batch.metallic_factor = 0.f;

        // colorCorrect's lateral chromatic aberration samples R and B away
        // from the centre tap while leaving G and coverage at the centre. Feed
        // it a spatial step at one quarter of the frame (away from the clean
        // optical centre), and prove both displaced channels change while the
        // centre channels remain bit-identical.
        std::array<std::uint8_t, 64 * 4> chromatic_emissive_bytes{};
        for (std::size_t texel = 0; texel < 64; ++texel)
        {
            const bool left = texel < 16;
            chromatic_emissive_bytes[texel * 4 + 0] = left ? 0 : 255;
            chromatic_emissive_bytes[texel * 4 + 1] = 32;
            chromatic_emissive_bytes[texel * 4 + 2] = left ? 255 : 0;
            chromatic_emissive_bytes[texel * 4 + 3] = 255;
        }
        auto chromatic_emissive = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 64,
            chromatic_emissive_bytes.data(), 64 * 4,
            "Alchemy PBR preview chromatic-aberration test emissive");
        const float screen_texcoords[3][2] = {
            {0.f, 0.5f}, {2.f, 0.5f}, {0.f, 0.5f},
        };
        REQUIRE(vertex_buffer->write(
            texcoord_offset, screen_texcoords, sizeof(screen_texcoords),
            &error));
        batch.texture_transform[3][4] = 0.f;
        batch.emissive_texture = chromatic_emissive.get();
        auto chromatic_baseline = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(chromatic_baseline);
        scene.chromatic_aberration_strength = 1.f;
        scene.chromatic_aberration_falloff = 1.f;
        scene.chromatic_aberration_angle = 0.f;
        auto chromatic_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(chromatic_output);
        std::array<std::uint8_t, 32 * 32 * 4> baseline_pixels{};
        std::array<std::uint8_t, 32 * 32 * 4> chromatic_pixels{};
        REQUIRE(chromatic_baseline->readback(
            *device, 0, 0, {}, baseline_pixels.data(), 32 * 4, &error));
        REQUIRE(chromatic_output->readback(
            *device, 0, 0, {}, chromatic_pixels.data(), 32 * 4, &error));
        int red_delta = 0;
        int blue_delta = 0;
        bool green_unchanged = true;
        bool alpha_unchanged = true;
        for (std::size_t pixel = 0; pixel < 32 * 32; ++pixel)
        {
            red_delta = std::max(
                red_delta,
                std::abs(static_cast<int>(chromatic_pixels[pixel * 4]) -
                         static_cast<int>(baseline_pixels[pixel * 4])));
            blue_delta = std::max(
                blue_delta,
                std::abs(static_cast<int>(chromatic_pixels[pixel * 4 + 2]) -
                         static_cast<int>(baseline_pixels[pixel * 4 + 2])));
            green_unchanged = green_unchanged &&
                chromatic_pixels[pixel * 4 + 1] ==
                    baseline_pixels[pixel * 4 + 1];
            alpha_unchanged = alpha_unchanged &&
                chromatic_pixels[pixel * 4 + 3] ==
                    baseline_pixels[pixel * 4 + 3];
        }
        CHECK(red_delta > 8);
        CHECK(blue_delta > 8);
        CHECK(green_unchanged);
        CHECK(alpha_unchanged);
        scene.chromatic_aberration_strength = 0.f;
        batch.emissive_texture = emissive.get();
        REQUIRE(vertex_buffer->write(
            texcoord_offset, texcoords, sizeof(texcoords), &error));

        // The GL owner alpha-composites linear HDR first, then applies the
        // full-frame post pass. Half-covered unit red therefore encodes 0.5
        // linear (~188), rather than blending an encoded 255 down to 128.
        batch.texture_transform[3][4] = 0.f;
        batch.base_color_texture = cutout_base.get();
        batch.alpha_cutoff = 0.f;
        auto blended_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(blended_output);
        const auto blended = read_center(*blended_output);
        CHECK(std::abs(static_cast<int>(blended[0]) - 188) <= 1);
        CHECK(blended[1] == 0);
        CHECK(blended[2] == 0);
        CHECK(std::abs(static_cast<int>(blended[3]) - 128) <= 1);

        // The preview asks generateExposure for an unadapted one-frame value.
        // Pinning min/max to EV 1 makes its dynamic scale exactly 0.5.
        batch.base_color_texture = base.get();
        scene.meter_exposure = true;
        scene.exposure_min_ev = 1.f;
        scene.exposure_max_ev = 1.f;
        auto exposed_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(exposed_output);
        const auto exposed = read_center(*exposed_output);
        CHECK(std::abs(static_cast<int>(exposed[0]) - 188) <= 1);
        CHECK(exposed[1] == 0);
        CHECK(exposed[2] == 0);
        CHECK(exposed[3] == 255);

        // With a full target of unit red, the meter's exact luminance is
        // 0.2126. Placing that at the 0.18 key produces scale 0.18/0.2126.
        scene.exposure_min_ev = -3.f;
        scene.exposure_max_ev = 4.f;
        auto metered_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(metered_output);
        const auto metered = read_center(*metered_output);
        const float expected_linear = 0.18f / 0.2126f;
        const float expected_encoded =
            1.055f * std::pow(expected_linear, 1.f / 2.4f) - 0.055f;
        const int expected_byte = static_cast<int>(
            std::lround(expected_encoded * 255.f));
        CHECK(std::abs(static_cast<int>(metered[0]) - expected_byte) <= 1);
        CHECK(metered[1] == 0);
        CHECK(metered[2] == 0);
        CHECK(metered[3] == 255);
        scene.meter_exposure = false;

        // A uniform 0.25-red HDR target with a threshold above the scene and
        // alpha-glow boost 0.5 extracts 0.25 at every level. Three pyramid
        // levels add back to 0.75, bringing the post input to unit red.
        batch.emissive_color[0] = 0.25f;
        auto quarter_red_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(quarter_red_output);
        const auto quarter_red = read_center(*quarter_red_output);
        CHECK(std::abs(static_cast<int>(quarter_red[0]) - 137) <= 1);
        scene.bloom_enabled = true;
        scene.bloom_mip_count = 3;
        scene.bloom_threshold = 100.f;
        scene.bloom_knee = 0.f;
        scene.bloom_alpha_glow_boost = 0.5f;
        scene.bloom_strength = 1.f;
        scene.bloom_scatter = 1.f;
        auto bloomed_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(bloomed_output);
        const auto bloomed = read_center(*bloomed_output);
        CHECK(bloomed[0] == 255);
        CHECK(bloomed[1] == 0);
        CHECK(bloomed[2] == 0);
        CHECK(bloomed[3] == 255);

        // A uniform field remains uniform through every normalized four-tap
        // streak pass. Four arms therefore sum four copies of the 0.25 bloom
        // extract; the final composite scales both the pyramid and streaks by
        // bloom strength, matching colorCorrectF.glsl.
        scene.bloom_strength = 0.25f;
        auto quarter_bloom_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(quarter_bloom_output);
        const auto quarter_bloom = read_center(*quarter_bloom_output);
        CHECK(std::abs(static_cast<int>(quarter_bloom[0]) - 177) <= 2);
        scene.cross_filter_enabled = true;
        scene.cross_filter_strength = 1.f;
        scene.cross_filter_points = 4;
        scene.cross_filter_angle = 13.f;
        scene.cross_filter_length = 1.f;
        scene.cross_filter_falloff = 1.5f;
        scene.cross_filter_chromatic = 1.f;
        auto streaked_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(streaked_output);
        const auto streaked = read_center(*streaked_output);
        CHECK(std::abs(static_cast<int>(streaked[0]) - 216) <= 2);
        CHECK(streaked[1] == 0);
        CHECK(streaked[2] == 0);
        CHECK(streaked[3] == 255);
        scene.cross_filter_enabled = false;
        scene.cross_filter_strength = 0.f;

        // Halation travels independently in alpha and is tinted only at the
        // pre-exposure composite. A 0.05 uniform excess sums to 0.15 here.
        scene.bloom_strength = 0.f;
        scene.bloom_halation = true;
        scene.bloom_halation_strength = 1.f;
        scene.bloom_threshold = 0.2f;
        scene.bloom_halation_tint[0] = 0.f;
        scene.bloom_halation_tint[1] = 1.f;
        scene.bloom_halation_tint[2] = 0.f;
        scene.bloom_halation_weights[0] = 1.f;
        scene.bloom_halation_weights[1] = 0.f;
        scene.bloom_halation_weights[2] = 0.f;
        auto halation_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(halation_output);
        const auto halation = read_center(*halation_output);
        CHECK(std::abs(static_cast<int>(halation[0]) - 137) <= 1);
        CHECK(std::abs(static_cast<int>(halation[1]) - 108) <= 2);
        CHECK(halation[2] == 0);
        CHECK(halation[3] == 255);
        batch.emissive_color[0] = 1.f;
        scene.bloom_enabled = false;
        scene.bloom_strength = 0.325f;
        scene.bloom_halation = false;
        scene.bloom_halation_strength = 0.f;

        batch.base_color_texture = cutout_base.get();
        batch.alpha_cutoff = 0.75f;
        auto cutout_output = renderer->render(scene, &error);
        INFO(error);
        REQUIRE(cutout_output);
        const auto cutout = read_center(*cutout_output);
        CHECK(cutout[0] == 0);
        CHECK(cutout[1] == 0);
        CHECK(cutout[2] == 0);
        CHECK(cutout[3] == 0);

        batch.base_color_texture = base.get();
        batch.alpha_cutoff = 0.f;
        scene.tonemap_mix = 1.f;
        for (std::uint32_t type = 2; type <= 6; ++type)
        {
            scene.tonemap_type = type;
            std::fill(std::begin(scene.tonemap_params),
                      std::end(scene.tonemap_params), 0.f);
            if (type == 2 || type == 3 || type == 4)
            {
                scene.tonemap_params[0] = 1.f;
            }
            else if (type == 6)
            {
                std::fill(std::begin(scene.tonemap_params),
                          std::end(scene.tonemap_params), 1.f);
            }
            auto mapped_output = renderer->render(scene, &error);
            INFO(error);
            CAPTURE(type);
            REQUIRE(mapped_output);
            CHECK(read_center(*mapped_output)[3] == 255);
        }

        scene.tonemap_type = 6;
        std::fill(std::begin(scene.tonemap_params),
                  std::end(scene.tonemap_params), 0.f);
        error.clear();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal PBR preview post state is invalid");
        scene.tonemap_type = 7;
        error.clear();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal PBR preview post state is invalid");
        scene.tonemap_type = 1;
        scene.tonemap_mix = 0.f;
        scene.bloom_mip_count = 2;
        error.clear();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal PBR preview post state is invalid");
        scene.bloom_mip_count = 3;
        scene.cross_filter_points = 1;
        error.clear();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal PBR preview post state is invalid");
        scene.cross_filter_points = 4;
        scene.chromatic_aberration_falloff = 0.25f;
        error.clear();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal PBR preview post state is invalid");
        scene.chromatic_aberration_falloff = 1.f;
        scene.environment = nullptr;
        error.clear();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal PBR preview scene is incomplete");
        scene.environment = &environment;
        environment.radiance_texture = nullptr;
        error.clear();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal PBR preview environment is invalid");
        environment.radiance_texture = environment_texture.get();
        batch.normal_texture = nullptr;
        error.clear();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal PBR preview batch is invalid");
        batch.normal_texture = normal.get();
        batch.tangent_offset += sizeof(float);
        error.clear();
        CHECK_FALSE(renderer->render(scene, &error));
        CHECK(error == "Metal PBR preview batch is invalid");
    }
}

TEST_CASE("almetal: production UI renderer consumes the viewer SoA layout", "[almetal]")
{
    @autoreleasepool
    {
        static_assert(sizeof(ALMetalUIMatrices) == 368);

        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto scheduler = ALMetalFrameScheduler::create(*device, 3, &error);
        INFO(error);
        REQUIRE(scheduler);
        auto renderer = ALMetalUIRenderer::create(*device, *scheduler, &error);
        INFO(error);
        REQUIRE(renderer);

        // Exactly LLVertexBuffer's production structure-of-arrays layout:
        // float4 positions, then float2 texcoords, then byte colors.
        constexpr std::size_t position_offset = 0;
        constexpr std::size_t texcoord_offset = sizeof(float) * 4 * 3;
        constexpr std::size_t color_offset = texcoord_offset + sizeof(float) * 2 * 3;
        std::array<std::uint8_t, color_offset + 4 * 3> bytes{};
        const float positions[3][4] = {
            {-0.8f, -0.8f, 0.f, 1.f},
            { 0.8f, -0.8f, 0.f, 1.f},
            { 0.0f,  0.8f, 0.f, 1.f},
        };
        const float texcoords[3][2] = {{0.f, 0.f}, {0.f, 0.f}, {0.f, 0.f}};
        const std::uint8_t colors[3][4] = {
            {255, 32, 16, 255}, {255, 32, 16, 255}, {255, 32, 16, 255},
        };
        std::memcpy(bytes.data() + position_offset, positions, sizeof(positions));
        std::memcpy(bytes.data() + texcoord_offset, texcoords, sizeof(texcoords));
        std::memcpy(bytes.data() + color_offset, colors, sizeof(colors));

        ALMetalBufferDescriptor buffer_descriptor;
        buffer_descriptor.length = bytes.size();
        buffer_descriptor.storage = ALMetalStorageMode::shared;
        buffer_descriptor.label = "Alchemy production UI test SoA";
        auto buffer = ALMetalBuffer::create(*device, buffer_descriptor, &error);
        INFO(error);
        REQUIRE(buffer);
        REQUIRE(buffer->write(0, bytes.data(), bytes.size(), &error));

        ALMetalTextureDescriptor texture_descriptor;
        texture_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        texture_descriptor.width = 1;
        texture_descriptor.height = 1;
        texture_descriptor.mipmap_level_count = 1;
        texture_descriptor.storage = ALMetalStorageMode::private_gpu;
        texture_descriptor.usage = ALMetalTextureUsage::shader_read;
        texture_descriptor.label = "Alchemy production UI test white";
        auto texture = ALMetalTexture::create(*device, texture_descriptor, &error);
        INFO(error);
        REQUIRE(texture);
        const std::array<std::uint8_t, 4> white = {255, 255, 255, 255};
        auto upload = device->makeCommandBuffer("Alchemy production UI test upload", &error);
        REQUIRE(upload);
        REQUIRE(texture->upload(*upload, 0, 0, {}, white.data(), 4, &error));
        REQUIRE(upload->commitAndWait(&error));
        INFO(error);

        ALMetalUIBatch batch;
        batch.vertices = buffer.get();
        batch.position_offset = position_offset;
        batch.texcoord_offset = texcoord_offset;
        batch.color_offset = color_offset;
        batch.texture = texture.get();
        batch.sampler.min_filter = ALMetalSamplerFilter::nearest;
        batch.sampler.mag_filter = ALMetalSamplerFilter::nearest;
        batch.sampler.mip_filter = ALMetalSamplerMipFilter::none;
        batch.sampler.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
        batch.sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
        batch.sampler.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
        batch.blend.enabled = false;
        batch.viewport = {0.0, 0.0, 32.0, 32.0, 0.0, 1.0};
        batch.scissor = {0, 0, 32, 32};
        batch.target_width = 32;
        batch.target_height = 32;
        batch.vertex_count = 3;
        for (int i = 0; i < 4; ++i)
        {
            batch.matrices.modelview[i * 4 + i] = 1.f;
            batch.matrices.projection[i * 4 + i] = 1.f;
            batch.matrices.modelview_projection[i * 4 + i] = 1.f;
            batch.matrices.inverse_projection[i * 4 + i] = 1.f;
            batch.matrices.texture0[i * 4 + i] = 1.f;
        }

        REQUIRE(renderer->draw(batch, &error));
        INFO(error);
        REQUIRE(renderer->hasOpenFrame());
        REQUIRE(renderer->endFrame(&error));
        INFO(error);
        REQUIRE(scheduler->waitUntilIdle(1000, &error));
        INFO(error);

        const ALMetalTexture* output = renderer->outputTexture();
        REQUIRE(output);
        std::array<std::uint8_t, 32 * 32 * 4> pixels{};
        REQUIRE(output->readback(*device, 0, 0, {}, pixels.data(), 32 * 4, &error));
        INFO(error);
        const std::size_t center = (16 * 32 + 16) * 4;
        CHECK(pixels[center + 0] == 16);  // BGRA target
        CHECK(pixels[center + 1] == 32);
        CHECK(pixels[center + 2] == 255);
        CHECK(pixels[center + 3] == 255);
        CHECK(pixels[0] == 0);
        CHECK(pixels[1] == 0);
        CHECK(pixels[2] == 0);
        CHECK(pixels[3] == 0);

        const ALMetalUIRendererStats stats = renderer->stats();
        CHECK(stats.frames == 1);
        CHECK(stats.batches == 1);
        CHECK(stats.vertices == 3);
        CHECK(stats.pipeline_states == 1);
        CHECK(stats.sampler_states == 1);

        auto rgba_renderer = ALMetalUIRenderer::createForTexture(
            *device, *scheduler, ALMetalPixelFormat::rgba8_unorm, &error);
        INFO(error);
        REQUIRE(rgba_renderer);
        REQUIRE(rgba_renderer->draw(batch, &error));
        INFO(error);
        CHECK(rgba_renderer->takeOutputTexture() == nullptr);
        REQUIRE(rgba_renderer->endFrame(&error));
        INFO(error);
        REQUIRE(scheduler->waitUntilIdle(1000, &error));
        INFO(error);

        auto owned_output = rgba_renderer->takeOutputTexture();
        REQUIRE(owned_output);
        CHECK(rgba_renderer->outputTexture() == nullptr);
        CHECK(owned_output->descriptor().format == ALMetalPixelFormat::rgba8_unorm);
        CHECK(alMetalHasUsage(owned_output->descriptor().usage,
                              ALMetalTextureUsage::render_target));
        CHECK(alMetalHasUsage(owned_output->descriptor().usage,
                              ALMetalTextureUsage::shader_read));
        pixels.fill(0);
        REQUIRE(owned_output->readback(*device, 0, 0, {}, pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(pixels[center + 0] == 255); // RGBA target
        CHECK(pixels[center + 1] == 32);
        CHECK(pixels[center + 2] == 16);
        CHECK(pixels[center + 3] == 255);

        // Ownership transfer must leave the reusable encoder capable of
        // allocating its next offscreen target lazily.
        REQUIRE(rgba_renderer->draw(batch, &error));
        INFO(error);
        REQUIRE(rgba_renderer->endFrame(&error));
        INFO(error);
        REQUIRE(scheduler->waitUntilIdle(1000, &error));
        INFO(error);
        REQUIRE(rgba_renderer->outputTexture());

        // The appearance compositor uses the same vertex layout with the
        // alpha-mask shader, including untextured clear/color rectangles.
        auto alpha_renderer = ALMetalUIRenderer::createForTexture(
            *device, *scheduler, ALMetalPixelFormat::rgba8_unorm, &error);
        INFO(error);
        REQUIRE(alpha_renderer);
        batch.shader = ALMetalUIShader::alpha_mask_textured;
        batch.minimum_alpha = 1.1f;
        REQUIRE(alpha_renderer->draw(batch, &error));
        REQUIRE(alpha_renderer->endFrame(&error));
        REQUIRE(scheduler->waitUntilIdle(1000, &error));
        const ALMetalTexture* alpha_output = alpha_renderer->outputTexture();
        REQUIRE(alpha_output);
        pixels.fill(255);
        REQUIRE(alpha_output->readback(
            *device, 0, 0, {}, pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(pixels[center + 0] == 0);
        CHECK(pixels[center + 1] == 0);
        CHECK(pixels[center + 2] == 0);
        CHECK(pixels[center + 3] == 0);

        auto discarded_alpha_output = alpha_renderer->takeOutputTexture();
        REQUIRE(discarded_alpha_output);
        const float upper_positions[3][4] = {
            {-0.8f, 0.1f, 0.f, 1.f},
            { 0.8f, 0.1f, 0.f, 1.f},
            { 0.0f, 0.9f, 0.f, 1.f},
        };
        REQUIRE(buffer->write(position_offset, upper_positions,
                              sizeof(upper_positions), &error));
        batch.shader = ALMetalUIShader::alpha_mask_solid;
        batch.texture = nullptr;
        batch.minimum_alpha = 0.5f;
        batch.blend.enabled = false;
        REQUIRE(alpha_renderer->draw(batch, &error));

        auto faded_bytes = bytes;
        std::memcpy(faded_bytes.data() + position_offset,
                    upper_positions, sizeof(upper_positions));
        std::uint8_t faded_colors[3][4] = {
            {255, 32, 16, 128}, {255, 32, 16, 128}, {255, 32, 16, 128},
        };
        std::memcpy(faded_bytes.data() + color_offset,
                    faded_colors, sizeof(faded_colors));
        buffer_descriptor.length = faded_bytes.size();
        buffer_descriptor.label = "Alchemy alpha-mask multiply SoA";
        auto faded_buffer = ALMetalBuffer::create(
            *device, buffer_descriptor, &error);
        INFO(error);
        REQUIRE(faded_buffer);
        REQUIRE(faded_buffer->write(
            0, faded_bytes.data(), faded_bytes.size(), &error));

        batch.vertices = faded_buffer.get();
        batch.blend.enabled = true;
        batch.blend.source_rgb = ALMetalBlendFactor::destination_alpha;
        batch.blend.destination_rgb = ALMetalBlendFactor::zero;
        batch.blend.source_alpha = ALMetalBlendFactor::destination_alpha;
        batch.blend.destination_alpha = ALMetalBlendFactor::zero;
        batch.blend.write_mask = ALMetalColorWriteMask::alpha;
        REQUIRE(alpha_renderer->draw(batch, &error));
        REQUIRE(alpha_renderer->endFrame(&error));
        REQUIRE(scheduler->waitUntilIdle(1000, &error));
        alpha_output = alpha_renderer->outputTexture();
        REQUIRE(alpha_output);
        pixels.fill(0);
        REQUIRE(alpha_output->readback(
            *device, 0, 0, {}, pixels.data(), 32 * 4, &error));
        INFO(error);
        const std::size_t gl_upper_row = (26 * 32 + 16) * 4;
        const std::size_t gl_lower_row = (4 * 32 + 16) * 4;
        CHECK(pixels[gl_upper_row + 0] == 255);
        CHECK(pixels[gl_upper_row + 1] == 32);
        CHECK(pixels[gl_upper_row + 2] == 16);
        CHECK(pixels[gl_upper_row + 3] == 128);
        CHECK(pixels[gl_lower_row + 0] == 0);
        CHECK(pixels[gl_lower_row + 1] == 0);
        CHECK(pixels[gl_lower_row + 2] == 0);
        CHECK(pixels[gl_lower_row + 3] == 0);
    }
}

TEST_CASE("almetal: a CAMetalLayer overlay coexists with an SDL OpenGL window", "[almetal]")
{
    @autoreleasepool
    {
        SDLVideoScope video;
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        SDL_PropertiesID properties = SDL_CreateProperties();
        REQUIRE(properties != 0);
        SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Alchemy Metal surface test");
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 64);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 48);
        SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
        SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
        std::unique_ptr<SDL_Window, SDLWindowDeleter> window(SDL_CreateWindowWithProperties(properties));
        SDL_DestroyProperties(properties);
        INFO(SDL_GetError());
        REQUIRE(window);
        CHECK((SDL_GetWindowFlags(window.get()) & SDL_WINDOW_OPENGL) != 0);
        CHECK((SDL_GetWindowFlags(window.get()) & SDL_WINDOW_METAL) == 0);

        auto surface = ALMetalSurface::create(window.get(), *device, {}, &error);
        INFO(error);
        REQUIRE(surface);
        CHECK((SDL_GetWindowFlags(window.get()) & SDL_WINDOW_OPENGL) != 0);
        CHECK((SDL_GetWindowFlags(window.get()) & SDL_WINDOW_METAL) == 0);
        CHECK(surface->nativeLayer() != nullptr);
        CHECK(surface->info().width >= 64);
        CHECK(surface->info().height >= 48);
        CHECK(surface->info().maximum_drawable_count == 3);
        CHECK(surface->info().opaque);
        REQUIRE(surface->setOpaque(false, &error));
        CHECK_FALSE(surface->info().opaque);
        REQUIRE(surface->setOpaque(true, &error));
        CHECK(surface->info().opaque);
        const std::uint64_t original_generation = surface->info().generation;
        REQUIRE(surface->resize(80, 52, &error));
        CHECK(surface->info().width == 80);
        CHECK(surface->info().height == 52);
        CHECK(surface->info().generation == original_generation + 1);

        auto drawable = surface->nextDrawable(&error);
        if (!drawable)
        {
            // Some WindowServer versions withhold drawable allocation from a
            // hidden view. Briefly showing the tiny test window is still the
            // real SDL/CAMetalLayer path and avoids treating that policy as a
            // backend failure.
            REQUIRE(SDL_ShowWindow(window.get()));
            REQUIRE(SDL_SyncWindow(window.get()));
            drawable = surface->nextDrawable(&error);
        }
        INFO(error);
        REQUIRE(drawable);
        CHECK(drawable->width() == 80);
        CHECK(drawable->height() == 52);
        CHECK(drawable->generation() == surface->info().generation);

        auto command = device->makeCommandBuffer("Alchemy Metal drawable test", &error);
        INFO(error);
        REQUIRE(command);
        ALMetalRenderPassColorConfig pass_config;
        pass_config.clear = {0.02, 0.04, 0.08, 1.0};
        auto pass = ALMetalRenderPass::createForColorAttachment(
            *device, drawable->nativeTexture(), ALMetalPixelFormat::bgra8_unorm,
            pass_config, &error);
        INFO(error);
        REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(
            *command, *pass, "Alchemy Metal drawable clear", &error);
        INFO(error);
        REQUIRE(encoder);
        REQUIRE(encoder->end(&error));

        REQUIRE(surface->present(*command, *drawable, &error));
        CHECK(drawable->wasPresented());
        REQUIRE(command->commit(&error));
        REQUIRE(command->waitUntilCompleted(&error));
        INFO(error);

        // A viewer frame without UI batches must still replace the previous
        // overlay with a transparent drawable; otherwise hiding all UI would
        // leave the last presented controls frozen above the GL world.
        auto scheduler = ALMetalFrameScheduler::create(*device, 3, &error);
        INFO(error);
        REQUIRE(scheduler);
        auto renderer = ALMetalUIRenderer::createForSurface(
            *device, *scheduler, *surface, &error);
        INFO(error);
        REQUIRE(renderer);
        REQUIRE(renderer->endFrame(&error));
        INFO(error);
        REQUIRE(scheduler->waitUntilIdle(1000, &error));
        INFO(error);
        const ALMetalUIRendererStats empty_stats = renderer->stats();
        CHECK(empty_stats.frames == 1);
        CHECK(empty_stats.batches == 0);
        CHECK(empty_stats.vertices == 0);
        CHECK(renderer->outputTexture() == nullptr);
        renderer.reset();

        // The Metal view must go away before its SDL window.
        surface.reset();
    }
}
