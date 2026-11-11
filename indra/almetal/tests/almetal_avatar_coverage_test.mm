/**
 * @file almetal_avatar_coverage_test.mm
 * @brief Native impostor coverage and RLVa silhouette stamping.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "almetaldevice.h"
#include "almetalpipeline.h"
#include "almetalresource.h"
#include "almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#import <Metal/Metal.h>
#import <dispatch/dispatch.h>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
    constexpr std::uint32_t side = 8;
    struct Pixels
    {
        std::array<std::vector<std::uint8_t>, 4> color;
        std::array<float, side * side> depth;
    };

    Pixels snapshot(const ALMetalDevice& device, const ALMetalRenderTarget& target)
    {
        Pixels result;
        std::string error;
        for (unsigned attachment = 0; attachment < 4; ++attachment)
        {
            const unsigned bytes = attachment == 2 &&
                target.descriptor().colors[2].format == ALMetalPixelFormat::rgba16_unorm ? 8 : 4;
            result.color[attachment].resize(side * side * bytes);
            REQUIRE(target.colorTexture(attachment)->readback(device, 0, 0, {},
                result.color[attachment].data(), side * bytes, &error));
        }
        REQUIRE(target.depthStencilTexture()->readback(device, 0, 0, {},
            result.depth.data(), side * sizeof(float), &error));
        return result;
    }

    std::unique_ptr<ALMetalRenderTarget> authoredTarget(
        const ALMetalDevice& device, const ALMetalShaderLibrary& library,
        bool reverse, bool hdr = false, bool normal_hdr = false, float geometry_depth = .5f)
    {
        std::string error;
        ALMetalRenderTargetDescriptor d;
        d.width = d.height = side;
        d.color_attachment_count = 4;
        d.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
        d.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
        d.colors[2].format = normal_hdr ? ALMetalPixelFormat::rgba16_unorm : ALMetalPixelFormat::rgb10a2_unorm;
        d.colors[3].format = hdr ? ALMetalPixelFormat::rg11b10_float : ALMetalPixelFormat::rgba8_unorm;
        d.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        auto target = ALMetalRenderTarget::create(device, d, &error);
        INFO(error); REQUIRE(target);
        ALMetalRenderPipelineDescriptor pipeline;
        pipeline.library = &library;
        pipeline.vertex_function = "almetal_world_coverage_vertex";
        pipeline.fragment_function = "almetal_world_coverage_fragment";
        pipeline.color_attachment_count = 4;
        pipeline.depth_stencil_format = d.depth_stencil_format;
        pipeline.label = "Native avatar coverage authored depth";
        for (unsigned i = 0; i < 4; ++i)
        {
            pipeline.colors[i].format = d.colors[i].format;
            pipeline.colors[i].write_mask = ALMetalColorWriteMask::none;
        }
        auto pso = ALMetalRenderPipeline::create(device, pipeline, &error);
        REQUIRE(pso);
        ALMetalDepthStencilDescriptor depth;
        depth.depth_compare = ALMetalCompareFunction::always;
        depth.depth_write_enabled = true;
        auto state = ALMetalDepthStencilState::create(device, depth, &error);
        REQUIRE(state);
        ALMetalRenderPassConfig config;
        for (unsigned i = 0; i < 4; ++i)
            config.colors[i].clear = {.3, .2, .6, .125};
        config.clear_depth = reverse ? 0.0 : 1.0;
        config.depth_store = config.stencil_store = ALMetalStoreAction::store;
        auto pass = target->makeRenderPass(config, &error);
        REQUIRE(pass);
        auto command = device.makeCommandBuffer("Native avatar depth-mask producer", &error);
        REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Native avatar authored edge mask", &error);
        REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pso, &error));
        REQUIRE(encoder->setDepthStencilState(state.get(), &error));
        REQUIRE(encoder->setViewport({0, 0, side, side, geometry_depth, geometry_depth}, &error));
        REQUIRE(encoder->setScissor({1, 2, 5, 4}, &error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
        const std::uint32_t forward = 0;
        REQUIRE(encoder->setVertexBytes(0, &forward, sizeof(forward), &error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));
        return target;
    }
}

TEST_CASE("Native impostor coverage stamps an empty forward scene without altering its material or depth",
          "[almetal][native_avatar_coverage]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error); REQUIRE(device);
    auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
    INFO(error); REQUIRE(renderer);
    auto library = ALMetalShaderLibrary::loadBuiltin(*device, &error);
    REQUIRE(library);
    for (bool reverse : {false, true})
    {
        CAPTURE(reverse);
        auto target = authoredTarget(*device, *library, reverse);
        const auto before = snapshot(*device, *target);
        ALMetalWorldGBufferScene scene;
        scene.width = scene.height = side;
        scene.reverse_z = reverse;
        scene.stamp_opaque_coverage = true;
        REQUIRE(renderer->compositeImpostorPostDeferred(*target, scene, &error));
        const auto after = snapshot(*device, *target);
        for (unsigned attachment = 1; attachment < 4; ++attachment)
            CHECK(after.color[attachment] == before.color[attachment]);
        CHECK(after.depth == before.depth);
        for (unsigned y = 0; y < side; ++y)
            for (unsigned x = 0; x < side; ++x)
            {
                CAPTURE(x, y);
                const unsigned at = (y * side + x) * 4;
                for (unsigned channel = 0; channel < 3; ++channel)
                    CHECK(after.color[0][at + channel] == before.color[0][at + channel]);
                CHECK(after.color[0][at + 3] ==
                    (x >= 1 && x < 6 && y >= 2 && y < 6 ? 255 : before.color[0][at + 3]));
            }
    }
}

TEST_CASE("Native avatar silhouette stamp preserves original fill pin and masked attachments",
          "[almetal][native_avatar_coverage]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    REQUIRE(device);
    auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
    INFO(error); REQUIRE(renderer);
    auto library = ALMetalShaderLibrary::loadBuiltin(*device, &error);
    REQUIRE(library);
    // Exact LLColor4::silhouette from the original CPU policy. Linear .05 is
    // encoded by the real sRGB target, as the original gDebug stamp was.
    const std::array<float, 4> silhouette{.05f, .05f, .05f, 1.f};
    const std::array<float, 4> pink{1.f, .5f, .8f, 1.f}; // LLColor4::pink, complexity-only branch.
    for (bool reverse : {false, true})
    for (bool hdr : {false, true})
    for (bool normal_hdr : {false, true})
    for (unsigned fill_mode = 0; fill_mode < 3; ++fill_mode)
    for (unsigned boundary = 0; boundary < 3; ++boundary)
    {
        CAPTURE(reverse, hdr, normal_hdr, fill_mode, boundary);
        const auto& fill = fill_mode == 1 ? silhouette : pink;
        const float pin = reverse ? .000005f : .999995f;
        const float geometry_depth = boundary == 0 ? .5f :
            boundary == 1 ? pin : (reverse ? .000003f : .999997f);
        auto target = authoredTarget(*device, *library, reverse, hdr, normal_hdr, geometry_depth);
        const auto before = snapshot(*device, *target);
        ALMetalWorldGBufferScene scene;
        scene.width = scene.height = side;
        scene.reverse_z = reverse; scene.hdr = hdr; scene.normal_hdr = normal_hdr;
        scene.stamp_opaque_coverage = true;
        scene.opaque_coverage_depth = pin;
        scene.opaque_coverage_fill = fill_mode ? fill.data() : nullptr;
        REQUIRE(renderer->compositeImpostorPostDeferred(*target, scene, &error));
        const auto after = snapshot(*device, *target);
        for (unsigned attachment = 1; attachment < 4; ++attachment)
            CHECK(after.color[attachment] == before.color[attachment]);
        CHECK(after.depth == before.depth);
        unsigned covered = 0;
        for (unsigned pixel = 0; pixel < side * side; ++pixel)
        {
            CAPTURE(pixel);
            const bool passes = reverse ? pin < before.depth[pixel] : pin > before.depth[pixel];
            covered += passes ? 1 : 0;
            for (unsigned channel = 0; channel < 3; ++channel)
            {
                const unsigned actual = after.color[0][pixel * 4 + channel];
                if (passes && fill_mode)
                {
                    const auto encoded = static_cast<unsigned>(std::lround(
                        (1.055 * std::pow(double(fill[channel]), 1.0 / 2.4) - .055) * 255.0));
                    CHECK(std::abs(int(actual) - int(encoded)) <= 1);
                }
                else
                    CHECK(actual == before.color[0][pixel * 4 + channel]);
            }
            CHECK(after.color[0][pixel * 4 + 3] == (passes ? 255 : before.color[0][pixel * 4 + 3]));
        }
        CHECK(covered == (boundary == 0 ? 20 : 0));
    }
}

TEST_CASE("Native avatar coverage rejects invalid policy before changing the target",
          "[almetal][native_avatar_coverage]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error); REQUIRE(device);
    auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error); REQUIRE(renderer);
    auto library = ALMetalShaderLibrary::loadBuiltin(*device, &error); REQUIRE(library);
    auto target = authoredTarget(*device, *library, false);
    const auto before = snapshot(*device, *target);
    ALMetalWorldGBufferScene scene;
    scene.width = scene.height = side;
    scene.stamp_opaque_coverage = true;
    for (float invalid : {-1.f, 2.f, std::numeric_limits<float>::quiet_NaN()})
    {
        scene.opaque_coverage_depth = invalid;
        CHECK_FALSE(renderer->compositeImpostorPostDeferred(*target, scene, &error));
        CHECK_FALSE(error.empty());
    }
    scene.opaque_coverage_depth.reset();
    const std::array<float, 4> invalid_fill{0, std::numeric_limits<float>::infinity(), 0, 1};
    scene.opaque_coverage_fill = invalid_fill.data();
    CHECK_FALSE(renderer->compositeImpostorPostDeferred(*target, scene, &error));
    scene.opaque_coverage_fill = nullptr;
    scene.stamp_opaque_coverage = false;
    CHECK_FALSE(renderer->compositeImpostorPostDeferred(*target, scene, &error));
    const auto after = snapshot(*device, *target);
    CHECK(after.color == before.color);
    CHECK(after.depth == before.depth);
}

TEST_CASE("Native avatar coverage copies CPU policy and retains encoded owners until GPU completion",
          "[almetal][native_avatar_coverage]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); REQUIRE(device);
        auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error); REQUIRE(renderer);
        auto library = ALMetalShaderLibrary::loadBuiltin(*device, &error); REQUIRE(library);
        auto target = authoredTarget(*device, *library, false);
        const auto before = snapshot(*device, *target);
        auto output = ALMetalTexture::retainSharedBacking(*target->colorTexture(0), &error);
        REQUIRE(output);
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device->nativeHandle();
        id<MTLSharedEvent> event = [native_device newSharedEvent];
        REQUIRE(event);
        struct Signal
        {
            id<MTLSharedEvent> event;
            std::shared_ptr<std::atomic<unsigned>> phase = std::make_shared<std::atomic<unsigned>>(0);
            ~Signal() { release(); }
            void release() { phase->exchange(1); event.signaledValue = 1; }
        } signal{event};
        const auto phase = signal.phase;
        // A test-only watchdog releases an accidental CPU wait so this cannot
        // leave the command queue blocked before RAII can unwind.
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC),
            dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
                unsigned expected = 0;
                if (phase->compare_exchange_strong(expected, 2)) event.signaledValue = 1;
            });
        auto gate = device->makeCommandBuffer("Native avatar coverage completion gate", &error);
        REQUIRE(gate);
        id<MTLCommandBuffer> native_gate = (__bridge id<MTLCommandBuffer>)gate->nativeHandle();
        [native_gate encodeWaitForEvent:event value:1];
        REQUIRE(gate->commit(&error));
        {
            std::array<float, 4> fill{.05f, .05f, .05f, 1.f};
            ALMetalWorldGBufferScene scene;
            scene.width = scene.height = side;
            scene.stamp_opaque_coverage = true;
            scene.opaque_coverage_depth = .999995f;
            scene.opaque_coverage_fill = fill.data();
            REQUIRE(renderer->compositeImpostorPostDeferred(*target, scene, &error));
            fill.fill(1.f); // Submitted constants must already be immutable.
        }
        target.reset(); renderer.reset(); library.reset();
        CHECK(phase->load() != 2);
        signal.release();
        std::array<std::uint8_t, side * side * 4> actual{};
        REQUIRE(output->readback(*device, 0, 0, {}, actual.data(), side * 4, &error));
        for (unsigned y = 0; y < side; ++y)
            for (unsigned x = 0; x < side; ++x)
            {
                const unsigned at = (y * side + x) * 4;
                const bool filled = x >= 1 && x < 6 && y >= 2 && y < 6;
                for (unsigned channel = 0; channel < 3; ++channel)
                    CHECK(actual[at + channel] == (filled ? 63 : before.color[0][at + channel]));
                CHECK(actual[at + 3] == (filled ? 255 : before.color[0][at + 3]));
            }
    }
}
