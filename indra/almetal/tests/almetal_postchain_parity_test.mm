/**
 * @file almetal_postchain_parity_test.mm
 * @brief Original final-viewport post effects over reduced native world input.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "../almetalworld.h"
#include "../almetalretainedbackground.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <memory>
#include <limits>
#include <vector>

namespace
{
using Pixel = std::array<std::uint8_t, 4>;
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error);
        INFO(error); REQUIRE(renderer);
        auto frame = ALMetalWorldFrameData::create(*device, &error);
        REQUIRE(frame); frame->beginFrame(1);
        renderer->setFrameData(std::move(frame));
    }
    std::shared_ptr<ALMetalRenderTarget> source(unsigned width, unsigned height)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = width; descriptor.height = height;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        descriptor.depth_stencil_shader_read = true;
        descriptor.label = "Original viewport constant-color world input";
        std::shared_ptr<ALMetalRenderTarget> result = ALMetalRenderTarget::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result);
        std::vector<Pixel> pixels(width * height, {240,80,16,255});
        auto command = device->makeCommandBuffer("Post-chain parity authored pixels", &error);
        REQUIRE(command);
        ALMetalRenderPassConfig clear;
        clear.colors[0].load = ALMetalLoadAction::dont_care;
        clear.depth_load = ALMetalLoadAction::clear;
        clear.depth_store = clear.stencil_store = ALMetalStoreAction::store;
        clear.clear_depth = .375;
        auto pass = result->makeRenderPass(clear, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Post-chain parity authored depth", &error);
        REQUIRE(encoder); REQUIRE(encoder->end(&error));
        REQUIRE(const_cast<ALMetalTexture*>(result->colorTexture(0))->upload(
            *command, 0, 0, {}, pixels.data(), width * 4, &error));
        REQUIRE(command->commitAndWait(&error));
        return result;
    }
    // Final effects run at the original window viewport; the authoritative
    // retained-background owner then normalizes the independent world depth.
    std::vector<Pixel> window(ALMetalWorldPresentationScene scene,
                              std::shared_ptr<ALMetalRenderTarget> depth,
                              unsigned width, unsigned height)
    {
        scene.output_width = width;
        scene.output_height = height;
        std::shared_ptr<ALMetalRenderTarget> presentation = renderer->renderPresentationPost(scene, &error);
        INFO(error); REQUIRE(presentation);
        REQUIRE(presentation->descriptor().width == width);
        REQUIRE(presentation->descriptor().height == height);
        ALMetalRetainedBackground background;
        ALMetalRetainedBackground::Context context{device.get(), this, 1, width, height, false};
        REQUIRE(background.publish(context, 1, presentation, depth, &error));
        auto normalized = background.find(context); INFO(error); REQUIRE(normalized);
        std::vector<float> normalized_depth(width * height);
        REQUIRE(normalized->depthTexture()->readback(*device, 0, 0, {}, normalized_depth.data(), width * 4, &error));
        for (float value : normalized_depth) CHECK(value == .375f);
        std::vector<Pixel> pixels(width * height);
        REQUIRE(normalized->colorTexture()->readback(*device, 0, 0, {}, pixels.data(), width * 4, &error));
        return pixels;
    }
};
}

TEST_CASE("Native final print noise retains the original viewport pixel origin",
          "[almetal][postchain-parity][presentation-origin]")
{
    Fixture f;
    auto source = f.source(8, 6);
    ALMetalWorldPresentationScene scene;
    scene.source = source.get();
    scene.grain_amount = .5f;
    scene.grain_animated = false;
    scene.grain_range = .5f;
    scene.output_width = 32; scene.output_height = 24;
    auto whole = f.renderer->renderPresentationPost(scene, &f.error); INFO(f.error); REQUIRE(whole);
    std::vector<Pixel> reference(32 * 24);
    REQUIRE(whole->colorTexture(0)->readback(*f.device, 0, 0, {}, reference.data(), 32 * 4, &f.error));
    scene.output_width = 16;
    scene.viewport_origin[0] = 8; scene.viewport_origin[1] = 5;
    auto shifted = f.renderer->renderPresentationPost(scene, &f.error); REQUIRE(shifted);
    std::vector<Pixel> actual(16 * 24);
    REQUIRE(shifted->colorTexture(0)->readback(*f.device, 0, 0, {}, actual.data(), 16 * 4, &f.error));
    unsigned changed = 0;
    // Raw row zero in these offscreen targets is the GL lower row. Positive
    // viewport Y therefore selects a later raw row of the zero-origin image.
    for (unsigned y = 0; y < 24 - 5; ++y)
        for (unsigned x = 0; x < 16; ++x)
        {
            CAPTURE(x, y);
            CHECK(actual[y*16+x] == reference[(y+5)*32+x+8]);
            changed += actual[y*16+x] != reference[y*32+x];
        }
    CHECK(changed > 16 * 10);

    const auto commands = ALMetalDevice::committedCommandBufferCount();
    scene.output_height = 0;
    CHECK_FALSE(f.renderer->renderPresentationPost(scene, &f.error));
    scene.output_height = 24;
    scene.viewport_origin[0] = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(f.renderer->renderPresentationPost(scene, &f.error));
    CHECK(ALMetalDevice::committedCommandBufferCount() == commands);
}

TEST_CASE("Native final lens mask is evaluated at window pixels before UI",
          "[almetal][postchain-parity][presentation-resolution]")
{
    Fixture f;
    constexpr unsigned width = 32, height = 24;
    // The same aspect ratio isolates the ordering defect from aspect-policy
    // differences. Constant source color isolates the original hard mask from
    // texture filtering and CAS arithmetic. Reference: original1eb44bbd,
    // renderFinalize final viewport + postEffectUtilsF lensDistortMask.
    for (const auto dimensions : {std::array<unsigned,2>{32,24}, {8,6}, {64,48}, {8,8}})
    {
        CAPTURE(dimensions);
        auto source = f.source(dimensions[0], dimensions[1]);
        ALMetalWorldPresentationScene scene;
        scene.source = source.get();
        scene.lens_distortion_amount = 1.f;
        scene.lens_distortion_k[0] = 0.3f;
        scene.lens_distortion_k[1] = 0.05f;
        scene.lens_distortion_scale = 1.2f;
        const auto actual = f.window(scene, source, width, height);
        unsigned inside = 0, outside = 0;
        for (unsigned y = 0; y < height; ++y)
        {
            for (unsigned x = 0; x < width; ++x)
            {
                const float qx = ((x + .5f) / width - .5f) * (float(width) / height);
                const float qy = (y + .5f) / height - .5f;
                const float r2 = qx*qx + qy*qy;
                const float radial = 1.f + r2 * (.3f + r2 * .05f);
                const float u = qx * radial * 1.2f / (float(width) / height) + .5f;
                const float v = qy * radial * 1.2f + .5f;
                const bool mask = u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f;
                inside += mask; outside += !mask;
                const Pixel expected = mask ? Pixel{16,80,240,255} : Pixel{0,0,0,255};
                CAPTURE(x, y, mask, actual[y*width+x], expected);
                for (unsigned c = 0; c < 4; ++c)
                    CHECK(std::abs(int(actual[y*width+x][c]) - int(expected[c])) <= 1);
            }
        }
        CHECK(inside > width * height / 2);
        CHECK(outside > width);
    }
}
