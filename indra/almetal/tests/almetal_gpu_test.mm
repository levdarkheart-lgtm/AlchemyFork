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
#include "../almetalresource.h"
#include "../almetalsurface.h"

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

TEST_CASE("almetal: an SDL CAMetalLayer resizes, supplies a drawable and presents it", "[almetal]")
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
        SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_METAL_BOOLEAN, true);
        SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
        std::unique_ptr<SDL_Window, SDLWindowDeleter> window(SDL_CreateWindowWithProperties(properties));
        SDL_DestroyProperties(properties);
        INFO(SDL_GetError());
        REQUIRE(window);

        auto surface = ALMetalSurface::create(window.get(), *device, {}, &error);
        INFO(error);
        REQUIRE(surface);
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
        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = (__bridge id<MTLTexture>)drawable->nativeTexture();
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.02, 0.04, 0.08, 1.0);
        id<MTLRenderCommandEncoder> encoder = [nativeCommand(*command) renderCommandEncoderWithDescriptor:pass];
        REQUIRE(encoder);
        [encoder endEncoding];

        REQUIRE(surface->present(*command, *drawable, &error));
        CHECK(drawable->wasPresented());
        REQUIRE(command->commit(&error));
        REQUIRE(command->waitUntilCompleted(&error));
        INFO(error);

        // The Metal view must go away before its SDL window.
        surface.reset();
    }
}
