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
#include "../almetalsurface.h"

#import <Metal/Metal.h>

#include <SDL3/SDL.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>

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
        REQUIRE(command->waitUntilCompleted(&error));
        INFO(error);

        // The Metal view must go away before its SDL window.
        surface.reset();
    }
}
