/**
 * @file almetal_borrowed_target_test.mm
 * @brief Shared scene depth with independently pooled color ownership.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "../almetaldevice.h"
#include "../almetalpipeline.h"
#include "../almetalresource.h"

#import <Metal/Metal.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace
{
    ALMetalRenderTargetDescriptor colorDescriptor()
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = 4;
        descriptor.height = 4;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.label = "Borrowed-depth pooled color";
        return descriptor;
    }

    ALMetalRenderTargetDescriptor depthDescriptor()
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = 4;
        descriptor.height = 4;
        descriptor.color_attachment_count = 0;
        descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        descriptor.depth_stencil_shader_read = true;
        descriptor.label = "Borrowed scene depth";
        return descriptor;
    }
}

TEST_CASE("almetal: borrowed depth retains pooled color across frame wrappers",
          "[almetal][borrowed-target]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto pool = ALMetalRenderTargetPool::create(*device, 1024 * 1024);
        REQUIRE(pool);
        auto scene = ALMetalRenderTarget::create(
            *device, depthDescriptor(), &error);
        REQUIRE(scene);
        const ALMetalTexture* first_color = nullptr;
        std::uint64_t created_after_first = 0;

        for (std::uint32_t frame = 0; frame < 3; ++frame)
        {
            CAPTURE(frame);
            auto color = pool->acquire(colorDescriptor(), &error);
            INFO(error);
            REQUIRE(color);
            if (frame == 0)
            {
                first_color = color->colorTexture(0);
                created_after_first =
                    alMetalResourceAllocationStats().textures_created;
            }
            CHECK(color->colorTexture(0) == first_color);
            auto target = ALMetalRenderTarget::attachBorrowedDepth(
                *device, std::move(color),
                *scene->depthStencilTexture(), &error);
            INFO(error);
            REQUIRE(target);
            CHECK(target->colorTexture(0) == first_color);
            CHECK(target->depthStencilTexture() ==
                  scene->depthStencilTexture());
            CHECK(target->descriptor().depth_stencil_shader_read);
            CHECK(alMetalResourceAllocationStats().textures_created ==
                  created_after_first);
            CHECK(pool->cachedTargetCount() == 0);

            ALMetalRenderPassConfig config;
            config.colors[0].clear = {0.0, 1.0, 0.0, 1.0};
            config.depth_store = ALMetalStoreAction::store;
            config.stencil_store = ALMetalStoreAction::store;
            config.clear_depth = 0.25 * (frame + 1);
            config.clear_stencil = 19 + frame;
            auto pass = target->makeRenderPass(config, &error);
            auto command = device->makeCommandBuffer(
                "Borrowed-depth frame clear", &error);
            REQUIRE(pass);
            REQUIRE(command);
            auto encoder = ALMetalRenderEncoder::begin(
                *command, *pass, "Borrowed-depth frame encoder", &error);
            REQUIRE(encoder);
            REQUIRE(encoder->end(&error));
            REQUIRE(command->commitAndWait(&error));
            INFO(error);

            std::array<float, 16> depth{};
            REQUIRE(scene->depthStencilTexture()->readback(
                *device, 0, 0, {}, depth.data(), 4 * sizeof(float),
                &error));
            for (const float value : depth)
            {
                CHECK(std::abs(value - config.clear_depth) < 1e-6);
            }
            std::array<std::uint8_t, 64> pixels{};
            REQUIRE(target->colorTexture(0)->readback(
                *device, 0, 0, {}, pixels.data(), 16, &error));
            for (std::size_t pixel = 0; pixel < 16; ++pixel)
            {
                CHECK(pixels[pixel * 4] == 0);
                CHECK(pixels[pixel * 4 + 1] == 255);
                CHECK(pixels[pixel * 4 + 2] == 0);
                CHECK(pixels[pixel * 4 + 3] == 255);
            }
            target.reset();
            CHECK(pool->cachedTargetCount() == 1);
            CHECK(pool->cachedBytes() == 4 * 4 * 4);
        }
        CHECK(pool->stats().reuses == 2);
        CHECK(alMetalResourceAllocationStats().textures_created ==
              created_after_first);
    }
}

TEST_CASE("almetal: queued borrowed-depth work survives wrapper retirement",
          "[almetal][borrowed-target]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto pool = ALMetalRenderTargetPool::create(*device, 1024 * 1024);
        auto scene = ALMetalRenderTarget::create(
            *device, depthDescriptor(), &error);
        REQUIRE(pool);
        REQUIRE(scene);
        auto target = ALMetalRenderTarget::attachBorrowedDepth(
            *device, pool->acquire(colorDescriptor(), &error),
            *scene->depthStencilTexture(), &error);
        REQUIRE(target);
        ALMetalRenderPassConfig config;
        config.colors[0].clear = {0.0, 0.0, 1.0, 1.0};
        config.depth_store = ALMetalStoreAction::store;
        config.stencil_store = ALMetalStoreAction::store;
        config.clear_depth = 0.375;
        config.clear_stencil = 91;
        auto pass = target->makeRenderPass(config, &error);
        auto command = device->makeCommandBuffer(
            "Queued borrowed-depth lifetime proof", &error);
        REQUIRE(pass);
        REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(
            *command, *pass, "Queued borrowed-depth encoder", &error);
        REQUIRE(encoder);
        REQUIRE(encoder->end(&error));
        encoder.reset();
        pass.reset();

        constexpr std::size_t row_bytes = 256;
        constexpr std::size_t plane_bytes = 4 * row_bytes;
        ALMetalBufferDescriptor staging_descriptor;
        staging_descriptor.length = 3 * plane_bytes;
        staging_descriptor.label = "Borrowed-depth lifetime staging";
        auto staging = ALMetalBuffer::create(
            *device, staging_descriptor, &error);
        REQUIRE(staging);
        id<MTLCommandBuffer> native_command =
            (__bridge id<MTLCommandBuffer>)command->nativeHandle();
        id<MTLBlitCommandEncoder> blit =
            [native_command blitCommandEncoder];
        REQUIRE(blit != nil);
        id<MTLTexture> native_depth = (__bridge id<MTLTexture>)
            target->depthStencilTexture()->nativeHandle();
        id<MTLTexture> native_color = (__bridge id<MTLTexture>)
            target->colorTexture(0)->nativeHandle();
        id<MTLBuffer> native_staging =
            (__bridge id<MTLBuffer>)staging->nativeHandle();
        [blit copyFromTexture:native_depth sourceSlice:0 sourceLevel:0
                 sourceOrigin:MTLOriginMake(0, 0, 0)
                   sourceSize:MTLSizeMake(4, 4, 1)
                     toBuffer:native_staging destinationOffset:0
        destinationBytesPerRow:row_bytes
      destinationBytesPerImage:plane_bytes
                      options:MTLBlitOptionDepthFromDepthStencil];
        [blit copyFromTexture:native_depth sourceSlice:0 sourceLevel:0
                 sourceOrigin:MTLOriginMake(0, 0, 0)
                   sourceSize:MTLSizeMake(4, 4, 1)
                     toBuffer:native_staging destinationOffset:plane_bytes
        destinationBytesPerRow:row_bytes
      destinationBytesPerImage:plane_bytes
                      options:MTLBlitOptionStencilFromDepthStencil];
        [blit copyFromTexture:native_color sourceSlice:0 sourceLevel:0
                 sourceOrigin:MTLOriginMake(0, 0, 0)
                   sourceSize:MTLSizeMake(4, 4, 1)
                     toBuffer:native_staging
            destinationOffset:2 * plane_bytes
        destinationBytesPerRow:row_bytes
      destinationBytesPerImage:plane_bytes
                      options:MTLBlitOptionNone];
        [blit endEncoding];
        blit = nil;
        native_depth = nil;
        native_color = nil;

        // External depth outlives the wrapper, as the API requires. Retire
        // every source wrapper and the pool before submitting: only native
        // command resource retention may keep these encoded reads valid.
        target.reset();
        CHECK(pool->cachedTargetCount() == 1);
        scene.reset();
        pool.reset();
        REQUIRE(command->commitAndWait(&error));
        INFO(error);
        const auto* bytes = static_cast<const std::uint8_t*>(
            staging->mappedBytes());
        REQUIRE(bytes);
        for (std::size_t y = 0; y < 4; ++y)
        {
            for (std::size_t x = 0; x < 4; ++x)
            {
                float depth = 0.f;
                std::memcpy(&depth, bytes + y * row_bytes + x * 4,
                            sizeof(depth));
                CHECK(std::abs(depth - 0.375f) < 1e-6f);
                CHECK(bytes[plane_bytes + y * row_bytes + x] == 91);
                const auto* color = bytes +
                    2 * plane_bytes + y * row_bytes + x * 4;
                CHECK(color[0] == 0);
                CHECK(color[1] == 0);
                CHECK(color[2] == 255);
                CHECK(color[3] == 255);
            }
        }
    }
}

TEST_CASE("almetal: borrowed-depth attachment rejects incompatible owners",
          "[almetal][borrowed-target]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        REQUIRE(device);
        auto scene = ALMetalRenderTarget::create(
            *device, depthDescriptor(), &error);
        REQUIRE(scene);
        CHECK_FALSE(ALMetalRenderTarget::attachBorrowedDepth(
            *device, nullptr, *scene->depthStencilTexture(), &error));

        auto with_depth = colorDescriptor();
        with_depth.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        CHECK_FALSE(ALMetalRenderTarget::attachBorrowedDepth(
            *device, ALMetalRenderTarget::create(*device, with_depth, &error),
            *scene->depthStencilTexture(), &error));

        auto wrong_size = colorDescriptor();
        wrong_size.width = 8;
        CHECK_FALSE(ALMetalRenderTarget::attachBorrowedDepth(
            *device, ALMetalRenderTarget::create(*device, wrong_size, &error),
            *scene->depthStencilTexture(), &error));

        auto colors = ALMetalRenderTarget::create(
            *device, colorDescriptor(), &error);
        REQUIRE(colors);
        auto external = colorDescriptor();
        external.colors[0].texture = colors->colorTexture(0);
        auto borrowed = ALMetalRenderTarget::create(*device, external, &error);
        REQUIRE(borrowed);
        CHECK_FALSE(ALMetalRenderTarget::attachBorrowedDepth(
            *device, std::move(borrowed),
            *scene->depthStencilTexture(), &error));
        CHECK_FALSE(ALMetalRenderTarget::attachBorrowedDepth(
            *device, ALMetalRenderTarget::create(
                *device, colorDescriptor(), &error),
            *colors->colorTexture(0), &error));
    }
}
