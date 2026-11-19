/**
 * PPD494: actual transparent-zero accumulation, carried depth and continuation.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_sky_modernization_fixture.h"

namespace
{
using HalfPixel = std::array<std::uint16_t, 4>;

void checkZero(sky_test::Fixture& f, const ALMetalRenderTarget& target)
{
    const auto pixels = f.read<HalfPixel>(*target.colorTexture(0));
    REQUIRE_FALSE(pixels.empty());
    CHECK(std::all_of(pixels.begin(), pixels.end(), [](const auto& pixel)
        { return pixel == HalfPixel{}; }));
}

bool increased(const std::vector<HalfPixel>& after, const std::vector<HalfPixel>& before)
{
    if (after.size() != before.size()) return false;
    for (std::size_t at = 0; at < after.size(); ++at)
        for (unsigned rgb = 0; rgb < 3; ++rgb)
            if (sky_test::half(after[at][rgb]) > sky_test::half(before[at][rgb])) return true;
    return false;
}

ALMetalRenderTargetDescriptor malformedDescriptor(unsigned variant)
{
    ALMetalRenderTargetDescriptor d;
    d.width = d.height = 4;
    d.color_attachment_count = 4;
    d.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
    d.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
    d.colors[2].format = ALMetalPixelFormat::rgb10a2_unorm;
    d.colors[3].format = ALMetalPixelFormat::rgba8_unorm;
    d.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
    d.depth_stencil_shader_read = true;
    switch (variant)
    {
    case 0: d.color_attachment_count = 1; break;
    case 1: d.colors[0].format = ALMetalPixelFormat::rgba8_unorm; break;
    case 2: d.colors[1].format = ALMetalPixelFormat::r8_unorm; break;
    case 3: d.colors[2].format = ALMetalPixelFormat::rgba8_unorm; break;
    case 4: d.colors[3].format = ALMetalPixelFormat::rgba16_float; break;
    case 5: d.depth_stencil_format = ALMetalPixelFormat::depth32_float; break;
    case 6: d.depth_stencil_shader_read = false; break;
    case 7: d.depth_stencil_format = ALMetalPixelFormat::invalid; break;
    case 8: d.colors[0].shader_read = false; break;
    case 9: d.sample_count = 4; break;
    }
    return d;
}
}

TEST_CASE("almetal: zero lighting clears exact RGBA and preserves carried G-buffer stores",
          "[almetal][zero-lighting]")
{
    @autoreleasepool
    {
        sky_test::Fixture f;
        for (bool hdr : {false, true}) for (bool normal_hdr : {false, true})
        for (bool reverse : {false, true})
        {
            CAPTURE(hdr, normal_hdr, reverse);
            auto gbuffer = f.gbuffer(true, reverse, normal_hdr, hdr);
            f.stampStencil(*gbuffer);
            const auto depth = f.read<std::uint32_t>(*gbuffer->depthStencilTexture());
            const auto stencil = f.stencil(*gbuffer->depthStencilTexture());
            std::array<std::vector<std::uint8_t>, 4> colors;
            for (unsigned i = 0; i < colors.size(); ++i) colors[i] = f.bytes(*gbuffer->colorTexture(i));
            auto target = f.renderer->renderZeroLighting(*gbuffer, &f.error);
            INFO(f.error); REQUIRE(target);
            CHECK(target->descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
            CHECK(target->descriptor().color_attachment_count == 1);
            CHECK(target->depthStencilTexture() == gbuffer->depthStencilTexture());
            CHECK(target->descriptor().depth_stencil_shader_read);
            checkZero(f, *target);
            CHECK(f.read<std::uint32_t>(*target->depthStencilTexture()) == depth);
            CHECK(f.stencil(*target->depthStencilTexture()) == stencil);
            for (unsigned i = 0; i < colors.size(); ++i) CHECK(f.bytes(*gbuffer->colorTexture(i)) == colors[i]);
        }
    }
}

TEST_CASE("almetal: zero lighting remains a positive point and projector accumulation owner",
          "[almetal][zero-lighting]")
{
    @autoreleasepool
    {
        sky_test::Fixture f;
        const auto* cookie = f.flat({.8f, .6f, .4f, 1.f});
        for (bool hdr : {false, true}) for (bool reverse : {false, true})
        {
            CAPTURE(hdr, reverse);
            auto gbuffer = f.gbuffer(true, reverse, true, hdr);
            f.stampStencil(*gbuffer);
            const auto depth = f.read<std::uint32_t>(*gbuffer->depthStencilTexture());
            const auto stencil = f.stencil(*gbuffer->depthStencilTexture());
            auto target = f.renderer->renderZeroLighting(*gbuffer, &f.error);
            INFO(f.error); REQUIRE(target);
            const auto zero = f.read<HalfPixel>(*target->colorTexture(0));
            ALMetalWorldLocalLight point;
            point.position_radius[2] = .5f; point.position_radius[3] = 3.f;
            point.color_falloff[0] = .08f; point.color_falloff[1] = .06f; point.color_falloff[2] = .04f;
            ALMetalWorldLocalLightingScene local;
            local.gbuffer = gbuffer.get(); local.lights = &point; local.light_count = 1;
            local.hdr = hdr; local.reverse_z = reverse; local.additive_pass_count = 2;
            sky_test::identity(local.inverse_projection); local.inverse_projection[14] = -1.f;
            const bool local_encoded = f.renderer->accumulateLocalLights(*target, local, &f.error);
            INFO(f.error); REQUIRE(local_encoded);
            const auto point_pixels = f.read<HalfPixel>(*target->colorTexture(0));
            CHECK(increased(point_pixels, zero));

            ALMetalWorldProjectorLight spot;
            spot.cookie_texture = cookie;
            spot.position_radius[2] = .5f; spot.position_radius[3] = 3.f;
            spot.color_falloff[0] = .12f; spot.color_falloff[1] = .08f; spot.color_falloff[2] = .06f;
            spot.projector_matrix[0] = spot.projector_matrix[5] = .25f;
            spot.projector_matrix[12] = spot.projector_matrix[13] = .5f;
            spot.projector_matrix[14] = spot.projector_matrix[15] = 1.f;
            spot.projector_normal[2] = -1.f; spot.projector_normal[3] = 2.f;
            spot.projector_origin[2] = .5f; spot.projector_origin[3] = 2.f;
            spot.sampling[0] = 1.f;
            ALMetalWorldProjectorLightingScene projector;
            projector.gbuffer = gbuffer.get(); projector.lights = &spot; projector.light_count = 1;
            projector.hdr = hdr; projector.normal_hdr = true; projector.reverse_z = reverse;
            projector.additive_pass_count = 2;
            std::copy_n(local.inverse_projection, 16, projector.inverse_projection);
            const bool projector_encoded = f.renderer->accumulateProjectorLights(*target, projector, &f.error);
            INFO(f.error); REQUIRE(projector_encoded);
            CHECK(increased(f.read<HalfPixel>(*target->colorTexture(0)), point_pixels));
            CHECK(f.read<std::uint32_t>(*target->depthStencilTexture()) == depth);
            CHECK(f.stencil(*target->depthStencilTexture()) == stencil);
            // Reuse/recycling must not turn the next disabled-atmosphere
            // invocation into a retained previous-light color owner.
            target.reset();
            target = f.renderer->renderZeroLighting(*gbuffer, &f.error);
            REQUIRE(target); checkZero(f, *target);
        }
    }
}

TEST_CASE("almetal: zero lighting rejects malformed inputs and cancels a borrowed prefix",
          "[almetal][zero-lighting]")
{
    @autoreleasepool
    {
        sky_test::Fixture f;
        for (unsigned variant = 0; variant < 10; ++variant)
        {
            CAPTURE(variant);
            auto invalid = ALMetalRenderTarget::create(*f.device, malformedDescriptor(variant), &f.error);
            INFO(f.error); REQUIRE(invalid);
            const auto committed = ALMetalDevice::committedCommandBufferCount();
            CHECK_FALSE(f.renderer->renderZeroLighting(*invalid, &f.error));
            CHECK_FALSE(f.error.empty()); f.error.clear();
            CHECK(ALMetalDevice::committedCommandBufferCount() == committed);
        }
        auto valid = f.gbuffer(true, true, true, true);
        f.stampStencil(*valid);
        const auto depth = f.read<std::uint32_t>(*valid->depthStencilTexture());
        const auto stencil = f.stencil(*valid->depthStencilTexture());
        auto invalid = ALMetalRenderTarget::create(*f.device, malformedDescriptor(0), &f.error);
        REQUIRE(invalid);
        for (bool fail : {false, true})
        {
            CAPTURE(fail);
            REQUIRE(f.renderer->beginCommandBatch("Zero-light prefix cancellation", &f.error));
            bool settled = true; unsigned settlements = 0;
            REQUIRE(f.renderer->observeCommandBatchSettlement([&](bool ok)
                { settled = ok; ++settlements; }, &f.error));
            const auto committed = ALMetalDevice::committedCommandBufferCount();
            auto target = f.renderer->renderZeroLighting(*valid, &f.error);
            INFO(f.error); REQUIRE(target);
            const auto completion = f.renderer->commandBatchCompletion(); REQUIRE(completion);
            CHECK_FALSE(completion->isComplete());
            if (fail)
            {
                CHECK_FALSE(f.renderer->renderZeroLighting(*invalid, &f.error));
                CHECK_FALSE(f.renderer->commitCommandBatch(&f.error));
            }
            else f.renderer->cancelCommandBatch();
            CHECK_FALSE(settled); CHECK(settlements == 1);
            CHECK(completion->isComplete());
            CHECK(ALMetalDevice::committedCommandBufferCount() == committed);
            target.reset();
            auto retry = f.renderer->renderZeroLighting(*valid, &f.error);
            INFO(f.error); REQUIRE(retry); checkZero(f, *retry);
            CHECK(f.read<std::uint32_t>(*retry->depthStencilTexture()) == depth);
            CHECK(f.stencil(*retry->depthStencilTexture()) == stencil);
        }
    }
}
