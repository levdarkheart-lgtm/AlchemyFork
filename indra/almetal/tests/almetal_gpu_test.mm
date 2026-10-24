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
#include "../almetalpipeline.h"
#include "../almetalresource.h"
#include "../almetalsurface.h"
#include "../almetalterrainpaint.h"
#include "../almetaltextureops.h"
#include "../almetaluirenderer.h"

#import <Metal/Metal.h>

#include <SDL3/SDL.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstring>
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
