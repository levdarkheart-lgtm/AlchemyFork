/**
 * @file almetal_spatial_presentation_test.mm
 * @brief MetalFX integration at the perceptual world/presentation boundary.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "../almetalworld.h"
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <dispatch/dispatch.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <utility>

namespace
{
using RGBA = std::array<std::uint8_t, 4>;

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::shared_ptr<ALMetalWorldFrameData> frame_data;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;

    Fixture()
    {
        device = ALMetalDevice::create(&error, ALMetalResourcePlanMode::enabled);
        INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error); REQUIRE(renderer);
        frame_data = ALMetalWorldFrameData::create(*device, &error);
        INFO(error); REQUIRE(frame_data);
        frame_data->beginFrame(1);
        renderer->setFrameData(frame_data);
    }

    ~Fixture()
    {
        if (renderer) renderer->cancelCommandBatch();
        if (device) device->waitUntilIdle();
    }

    std::unique_ptr<ALMetalRenderTarget> target(
        unsigned width, unsigned height, ALMetalPixelFormat format)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = width;
        descriptor.height = height;
        descriptor.colors[0].format = format;
        descriptor.label = "Spatial presentation integration fixture";
        auto result = ALMetalRenderTarget::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result);
        return result;
    }

    std::unique_ptr<ALMetalRenderTarget> constant(
        unsigned width, unsigned height, RGBA color,
        ALMetalPixelFormat format = ALMetalPixelFormat::rgba8_unorm)
    {
        auto result = target(width, height, format);
        std::vector<std::uint32_t> pixels(std::size_t(width) * height);
        std::uint32_t packed = 0;
        if (format == ALMetalPixelFormat::rgba8_unorm)
        {
            for (unsigned channel = 0; channel < 4; ++channel)
                packed |= std::uint32_t(color[channel]) << (channel * 8);
        }
        else
        {
            REQUIRE(format == ALMetalPixelFormat::rgb10a2_unorm);
            for (unsigned channel = 0; channel < 3; ++channel)
                packed |= ((std::uint32_t(color[channel]) * 1023 + 127) / 255)
                    << (channel * 10);
            packed |= ((std::uint32_t(color[3]) * 3 + 127) / 255) << 30;
        }
        std::fill(pixels.begin(), pixels.end(), packed);
        auto command = device->makeCommandBuffer("Spatial fixture upload", &error);
        INFO(error); REQUIRE(command);
        const std::size_t row_bytes = (std::size_t(width) * 4 + 255) & ~std::size_t(255);
        auto native_device = (__bridge id<MTLDevice>)device->nativeHandle();
        id<MTLBuffer> staging = [native_device newBufferWithLength:row_bytes * height
            options:MTLResourceStorageModeShared];
        REQUIRE(staging);
        for (unsigned y = 0; y < height; ++y)
            std::memcpy(static_cast<std::uint8_t*>(staging.contents) + y * row_bytes,
                pixels.data() + std::size_t(y) * width, width * 4);
        auto native_command = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
        id<MTLBlitCommandEncoder> blit = [native_command blitCommandEncoder];
        REQUIRE(blit);
        [blit copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:row_bytes
            sourceBytesPerImage:row_bytes * height sourceSize:MTLSizeMake(width, height, 1)
            toTexture:(__bridge id<MTLTexture>)result->colorTexture(0)->nativeHandle()
            destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blit endEncoding];
        INFO(error); REQUIRE(command->commitAndWait(&error));
        return result;
    }

    std::vector<RGBA> read(const ALMetalRenderTarget& target)
    {
        REQUIRE(target.descriptor().colors[0].format == ALMetalPixelFormat::bgra8_unorm);
        const auto width = target.descriptor().width;
        std::vector<RGBA> result(std::size_t(width) * target.descriptor().height);
        REQUIRE(target.colorTexture(0)->readback(*device, 0, 0, {},
            result.data(), width * 4, &error));
        INFO(error); return result;
    }

    void expectPixel(const RGBA& bgra, RGBA rgba, int tolerance = 2)
    {
        for (unsigned channel = 0; channel < 4; ++channel)
        {
            const unsigned source = channel < 3 ? 2 - channel : 3;
            CAPTURE(channel, bgra[channel], rgba[source]);
            CHECK(std::abs(int(bgra[channel]) - int(rgba[source])) <= tolerance);
        }
    }

    void expectConstant(const ALMetalRenderTarget& output, RGBA color)
    {
        for (const auto& pixel : read(output)) expectPixel(pixel, color);
    }

    std::unique_ptr<ALMetalRenderTarget> present(
        const ALMetalRenderTarget& source, ALMetalWorldSpatialUpscaleResult& scaled)
    {
        ALMetalWorldPresentationScene scene;
        scene.source = &source;
        scene.upscaled_color = scaled.color->colorTexture(0);
        scene.output_width = scaled.color->descriptor().width;
        scene.output_height = scaled.color->descriptor().height;
        auto result = renderer->renderPresentationPost(scene, &error, std::move(scaled.presentation));
        INFO(error); REQUIRE(result); return result;
    }
};

// Hold two real committed main post commands pending without relying on GPU
// speed. The bounded watchdog and destructor both unblock teardown on failure.
struct PendingGate
{
    id<MTLSharedEvent> event;
    std::shared_ptr<std::atomic<unsigned>> state = std::make_shared<std::atomic<unsigned>>(0);
    explicit PendingGate(Fixture& fixture)
    {
        event = [(__bridge id<MTLDevice>)fixture.device->nativeHandle() newSharedEvent];
        REQUIRE(event);
        auto command = fixture.device->makeCommandBuffer("Spatial pending-main queue gate", &fixture.error);
        REQUIRE(command);
        [(__bridge id<MTLCommandBuffer>)command->nativeHandle() encodeWaitForEvent:event value:1];
        REQUIRE(command->commit(&fixture.error));
        const auto observed = state;
        id<MTLSharedEvent> retained = event;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC),
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                unsigned expected = 0;
                if (observed->compare_exchange_strong(expected, 2)) retained.signaledValue = 1;
            });
    }
    void release()
    {
        unsigned expected = 0;
        state->compare_exchange_strong(expected, 1);
        event.signaledValue = 1;
    }
    ~PendingGate() { release(); }
};
}

TEST_CASE("Spatial world output preserves perceptual RGB and owns opaque alpha",
          "[metal][spatial-presentation]")
{
    @autoreleasepool
    {
        Fixture fixture;
        const auto format = GENERATE(ALMetalPixelFormat::rgba8_unorm,
                                     ALMetalPixelFormat::rgb10a2_unorm);
        constexpr RGBA color{64, 128, 192, 85};
        auto source = fixture.constant(32, 24, color, format);
        REQUIRE(fixture.renderer->prepareSpatialUpscale(*source, 64, 48, &fixture.error));
        INFO(fixture.error);

        // Preparation may select a perceptual float copy for a packed display
        // source; it must not add a gamma/linear-light conversion in either path.
        auto scaled = fixture.renderer->renderSpatialUpscale(*source, 64, 48, &fixture.error);
        INFO(fixture.error); REQUIRE(scaled);
        REQUIRE(scaled.color->descriptor().width == 64);
        REQUIRE(scaled.color->descriptor().height == 48);
        REQUIRE(scaled.color->descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
        auto output = fixture.present(*source, scaled);
        REQUIRE(output->descriptor().width == 64);
        REQUIRE(output->descriptor().height == 48);
        fixture.expectConstant(*output, {64, 128, 192, 255});
        CHECK(fixture.renderer->spatialUpscaleStats().encoded_calls == 1);

        // Ordinary native presentation still samples the internal source and
        // preserves its authored alpha. Opaque FX alpha is a separate contract.
        ALMetalWorldPresentationScene native;
        native.source = source.get();
        native.output_width = 64;
        native.output_height = 48;
        auto native_output = fixture.renderer->renderPresentationPost(native, &fixture.error);
        INFO(fixture.error); REQUIRE(native_output);
        fixture.expectConstant(*native_output, color);
    }
}

TEST_CASE("Spatial presentation uses internal reference extents and upscaled current pixels",
          "[metal][spatial-presentation]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto internal = fixture.constant(32, 24, {16, 32, 48, 85});
        auto current = fixture.constant(32, 24, {64, 128, 192, 170});
        auto reference_source = fixture.constant(32, 24, {224, 32, 80, 85});
        auto reference = fixture.renderer->capturePresentationReference(*reference_source, &fixture.error);
        INFO(fixture.error); REQUIRE(reference);
        auto scaled = fixture.renderer->renderSpatialUpscale(*current, 64, 48, &fixture.error);
        INFO(fixture.error); REQUIRE(scaled);
        ALMetalWorldPresentationScene scene;
        scene.source = internal.get();
        scene.upscaled_color = scaled.color->colorTexture(0);
        scene.output_width = 64;
        scene.output_height = 48;
        scene.reference_texture = reference->colorTexture(0);
        scene.reference_position = .5f;
        for (const unsigned mode : {1u, 2u})
        {
            CAPTURE(mode);
            scene.reference_mode = mode;
            auto output = fixture.renderer->renderPresentationPost(scene, &fixture.error,
                std::move(scaled.presentation));
            INFO(fixture.error); REQUIRE(output);
            const auto pixels = fixture.read(*output);
            fixture.expectPixel(pixels[24 * 64 + 8], {224, 32, 80, 255});
            // A distinct original source prevents silently selecting the
            // internal image instead of the actual full-resolution FX result.
            fixture.expectPixel(pixels[24 * 64 + 56], {64, 128, 192, 255});
        }

        auto wrong_reference = fixture.constant(64, 48, {224, 32, 80, 85});
        scene.reference_texture = wrong_reference->colorTexture(0);
        fixture.error.clear();
        CHECK_FALSE(fixture.renderer->renderPresentationPost(scene, &fixture.error));
        CHECK_FALSE(fixture.error.empty());
    }
}

TEST_CASE("Spatial presentation rejects incompatible output and cutout before submission",
          "[metal][spatial-presentation]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto source = fixture.constant(32, 24, {64, 128, 192, 255});
        auto scaled = fixture.renderer->renderSpatialUpscale(*source, 64, 48, &fixture.error);
        INFO(fixture.error); REQUIRE(scaled);
        auto wrong_size = fixture.target(63, 48, ALMetalPixelFormat::rgba16_float);
        auto wrong_format = fixture.target(64, 48, ALMetalPixelFormat::rgba8_unorm);
        auto coverage = fixture.target(32, 24, ALMetalPixelFormat::rgba16_float);
        ALMetalWorldPresentationScene valid;
        valid.source = source.get();
        valid.upscaled_color = scaled.color->colorTexture(0);
        valid.output_width = 64;
        valid.output_height = 48;
        const auto submissions = ALMetalDevice::committedCommandBufferCount();
        for (const unsigned invalid_case : {0u, 1u, 2u, 3u, 4u, 5u})
        {
            CAPTURE(invalid_case);
            auto scene = valid;
            if (invalid_case == 0) scene.upscaled_color = wrong_size->colorTexture(0);
            if (invalid_case == 1) scene.upscaled_color = wrong_format->colorTexture(0);
            if (invalid_case == 2) scene.cutout_coverage_texture = coverage->colorTexture(0);
            if (invalid_case == 3) scene.output_width = scene.output_height = 0;
            std::unique_ptr<ALMetalRenderTarget> supplied;
            if (invalid_case == 4) supplied = fixture.target(63, 48, ALMetalPixelFormat::bgra8_unorm);
            if (invalid_case == 5) supplied = fixture.target(64, 48, ALMetalPixelFormat::rgba8_unorm);
            fixture.error.clear();
            CHECK_FALSE(fixture.renderer->renderPresentationPost(scene, &fixture.error, std::move(supplied)));
            CHECK_FALSE(fixture.error.empty());
            CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);
        }
    }
}

TEST_CASE("Spatial main batches reuse pre-acquired outputs while prior work is pending",
          "[metal][spatial-presentation]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto source = fixture.constant(32, 24, {64, 128, 192, 85});
        // Warm the exact main-batch descriptor set and release both owners to
        // the pool. This includes conversion storage when the vendor needs it.
        REQUIRE(fixture.renderer->beginMainFrameBatch(1, "Spatial warm main post", &fixture.error));
        auto warm = fixture.renderer->renderSpatialUpscale(*source, 64, 48, &fixture.error);
        REQUIRE(warm);
        auto warm_output = fixture.present(*source, warm);
        REQUIRE(fixture.renderer->commitCommandBatch(&fixture.error));
        fixture.expectConstant(*warm_output, {64, 128, 192, 255});
        warm = {}; warm_output.reset();
        const auto allocated = alMetalResourceAllocationStats().textures_created;
        const auto before_pool = fixture.renderer->renderTargetPoolStats();
        PendingGate gate(fixture);
        std::array<std::shared_ptr<const ALMetalCommandCompletion>, 2> pending;
        const void* first_final_backing = nullptr;
        std::unique_ptr<ALMetalRenderTarget> last_output;
        for (unsigned index = 0; index < 2; ++index)
        {
            fixture.frame_data->beginFrame(index + 2);
            REQUIRE(fixture.renderer->beginMainFrameBatch(index + 2,
                "Spatial ordinary main opaque and optical post", &fixture.error));
            auto scaled = fixture.renderer->renderSpatialUpscale(*source, 64, 48, &fixture.error);
            INFO(fixture.error); REQUIRE(scaled);
            auto output = fixture.present(*source, scaled);
            if (!index) first_final_backing = output->colorTexture(0)->nativeHandle();
            else CHECK(output->colorTexture(0)->nativeHandle() == first_final_backing);
            pending[index] = fixture.renderer->commandBatchCompletion();
            REQUIRE(pending[index]);
            REQUIRE(fixture.renderer->commitCommandBatch(&fixture.error)); INFO(fixture.error);
            scaled = {};
            if (index) last_output = std::move(output);
            // First output returns to the pool after its command commits.
            // Ordered queue reuse must not require its GPU work to complete.
        }
        REQUIRE(gate.state->load() == 0);
        CHECK_FALSE(pending[0]->isComplete()); CHECK_FALSE(pending[1]->isComplete());
        CHECK(alMetalResourceAllocationStats().textures_created == allocated);
        CHECK(fixture.renderer->renderTargetPoolStats().reuses >= before_pool.reuses + 4);
        gate.release();
        for (const auto& completion : pending)
            REQUIRE(completion->waitUntilCompleted(&fixture.error));
        fixture.expectConstant(*last_output, {64, 128, 192, 255});
    }
}

TEST_CASE("Spatial used presentation PSO is staged before shared shader reload publishes",
          "[metal][spatial-presentation][shader-reload]")
{
    @autoreleasepool
    {
        Fixture fixture;
        const auto format = GENERATE(ALMetalPixelFormat::rgba8_unorm,
                                     ALMetalPixelFormat::rgb10a2_unorm);
        auto source = fixture.constant(32, 24, {64, 128, 192, 85}, format);
        auto scaled = fixture.renderer->renderSpatialUpscale(*source, 64, 48, &fixture.error);
        REQUIRE(scaled);
        auto first = fixture.present(*source, scaled);
        fixture.expectConstant(*first, {64, 128, 192, 255});
        const auto generation = fixture.renderer->shaderGeneration();
        const auto scaler_stats = fixture.renderer->spatialUpscaleStats();
        const auto library = fixture.renderer->shaderLibrary();
        std::array owners{fixture.renderer.get()};
        fixture.renderer->failNextShaderReloadForTesting();
        CHECK_FALSE(ALMetalWorldGBufferRenderer::reloadSharedShaders(
            *fixture.frame_data, owners, &fixture.error));
        CHECK(fixture.renderer->shaderGeneration() == generation);
        CHECK(fixture.renderer->shaderLibrary() == library);
        CHECK(fixture.renderer->spatialUpscaleStats().encoded_calls == scaler_stats.encoded_calls);
        REQUIRE(ALMetalWorldGBufferRenderer::reloadSharedShaders(
            *fixture.frame_data, owners, &fixture.error)); INFO(fixture.error);
        CHECK(fixture.renderer->shaderGeneration() == generation + 1);
        CHECK(fixture.renderer->spatialUpscaleStats().creation_attempts == scaler_stats.creation_attempts);
        CHECK(fixture.renderer->spatialUpscaleStats().encoded_calls == scaler_stats.encoded_calls);
        // Do not prepare/encode another MetalFX image after publication. The
        // previously used final PSO must already exist in the new shader set.
        ALMetalWorldPresentationScene scene;
        scene.source = source.get(); scene.upscaled_color = scaled.color->colorTexture(0);
        scene.output_width = 64; scene.output_height = 48;
        auto provided = fixture.target(64, 48, ALMetalPixelFormat::bgra8_unorm);
        auto output = fixture.renderer->renderPresentationPost(scene, &fixture.error, std::move(provided));
        INFO(fixture.error); REQUIRE(output);
        fixture.expectConstant(*output, {64, 128, 192, 255});
    }
}

TEST_CASE("Spatial post chains retain pending resized images and recover after cancellation",
          "[metal][spatial-presentation]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto first_source = fixture.constant(32, 24, {64, 128, 192, 85});
        auto second_source = fixture.constant(36, 28, {192, 64, 128, 170});
        const auto submissions = ALMetalDevice::committedCommandBufferCount();
        REQUIRE(fixture.renderer->beginCommandBatch("Pending spatial post chains", &fixture.error));
        auto first_scaled = fixture.renderer->renderSpatialUpscale(*first_source, 64, 48, &fixture.error);
        INFO(fixture.error); REQUIRE(first_scaled);
        auto first_output = fixture.present(*first_source, first_scaled);
        auto second_scaled = fixture.renderer->renderSpatialUpscale(*second_source, 72, 56, &fixture.error);
        INFO(fixture.error); REQUIRE(second_scaled);
        auto second_output = fixture.present(*second_source, second_scaled);
        CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);
        first_scaled = {};
        second_scaled = {};
        first_source.reset();
        second_source.reset();
        // External frame rotation cannot recycle resources retained by this
        // still-unsubmitted command, even after the caller releases wrappers.
        fixture.frame_data->beginFrame(2);
        REQUIRE(fixture.renderer->commitCommandBatch(&fixture.error));
        INFO(fixture.error);
        CHECK(ALMetalDevice::committedCommandBufferCount() == submissions + 1);
        fixture.expectConstant(*first_output, {64, 128, 192, 255});
        fixture.expectConstant(*second_output, {192, 64, 128, 255});

        auto canceled_source = fixture.constant(32, 24, {8, 16, 24, 0});
        const auto before_cancel = ALMetalDevice::committedCommandBufferCount();
        REQUIRE(fixture.renderer->beginCommandBatch("Canceled spatial post chain", &fixture.error));
        auto canceled_scaled = fixture.renderer->renderSpatialUpscale(*canceled_source, 64, 48, &fixture.error);
        INFO(fixture.error); REQUIRE(canceled_scaled);
        auto canceled_output = fixture.present(*canceled_source, canceled_scaled);
        fixture.renderer->cancelCommandBatch();
        CHECK_FALSE(fixture.renderer->commandBatchActive());
        CHECK(ALMetalDevice::committedCommandBufferCount() == before_cancel);
        // Canceled outputs are never read or published. A new transaction
        // with the same descriptor must produce the newly supplied image.
        canceled_output.reset();
        canceled_scaled = {};
        auto recovered_source = fixture.constant(32, 24, {96, 160, 224, 85});
        REQUIRE(fixture.renderer->beginCommandBatch("Recovered spatial post chain", &fixture.error));
        auto recovered_scaled = fixture.renderer->renderSpatialUpscale(*recovered_source, 64, 48, &fixture.error);
        INFO(fixture.error); REQUIRE(recovered_scaled);
        auto recovered_output = fixture.present(*recovered_source, recovered_scaled);
        REQUIRE(fixture.renderer->commitCommandBatch(&fixture.error));
        INFO(fixture.error);
        fixture.expectConstant(*recovered_output, {96, 160, 224, 255});
    }
}
