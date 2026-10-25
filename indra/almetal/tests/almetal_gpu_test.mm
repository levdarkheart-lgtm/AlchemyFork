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
            "almetal_resample_vertex",
            "almetal_resample_fragment",
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
        const std::array<std::uint8_t, 4> half_alpha_black{{0, 0, 0, 128}};
        const std::array<std::uint8_t, 4> flat_normal{{128, 128, 255, 255}};
        const std::array<std::uint8_t, 4> zero_ambient_orm{{0, 255, 0, 255}};
        const std::array<std::uint8_t, 8> red_green_emissive{{
            255, 0, 0, 255, 0, 255, 0, 255,
        }};
        auto base = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 1,
            opaque_black.data(), 4, "Alchemy PBR preview test base");
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
        auto emissive = make_texture(
            ALMetalPixelFormat::rgba8_unorm_srgb, 2,
            red_green_emissive.data(), 8,
            "Alchemy PBR preview test emissive");

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
        scene.batches = &batch;
        scene.batch_count = 1;
        scene.destination_descriptor = &destination_descriptor;
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
