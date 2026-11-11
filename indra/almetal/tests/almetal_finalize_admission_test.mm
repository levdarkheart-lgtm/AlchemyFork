/**
 * @file almetal_finalize_admission_test.mm
 * @brief Native SDR optics, pre-print measurements and committed still owners.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "../almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
constexpr unsigned SIDE = 12, SAMPLE = 4;
using Bytes = std::array<std::uint8_t, 4>;
using Halves = std::array<std::uint16_t, 4>;
std::uint16_t half(float value)
{
    const __fp16 converted = value; std::uint16_t result;
    std::memcpy(&result, &converted, sizeof(result)); return result;
}
float real(std::uint16_t bits)
{
    __fp16 value; std::memcpy(&value, &bits, sizeof(value)); return value;
}
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
        auto arena = ALMetalWorldFrameData::create(*device, &error); REQUIRE(arena);
        arena->beginFrame(1); renderer->setFrameData(std::move(arena));
    }
    std::unique_ptr<ALMetalRenderTarget> target(ALMetalPixelFormat format)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = descriptor.height = SIDE;
        descriptor.colors[0].format = format;
        auto result = ALMetalRenderTarget::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result); return result;
    }
    template<typename T> void upload(ALMetalRenderTarget& target, const std::vector<T>& pixels)
    {
        REQUIRE(pixels.size() == SIDE * SIDE);
        auto command = device->makeCommandBuffer("Native finalization authored pixels", &error); REQUIRE(command);
        REQUIRE(const_cast<ALMetalTexture*>(target.colorTexture(0))->upload(
            *command, 0, 0, {}, pixels.data(), SIDE * sizeof(T), &error));
        REQUIRE(command->commitAndWait(&error));
    }
    template<typename T> std::vector<T> read(const ALMetalRenderTarget& target, unsigned side = SIDE)
    {
        REQUIRE(target.descriptor().width == side);
        REQUIRE(target.descriptor().height == side);
        REQUIRE(target.colorTexture(0)->bytesPerPixel() == sizeof(T));
        std::vector<T> result(side * side);
        REQUIRE(target.colorTexture(0)->readback(*device, 0, 0, {}, result.data(), side * sizeof(T), &error));
        return result;
    }
    std::vector<Bytes> normalizedReference(const ALMetalRenderTarget& target)
    {
        const auto format = target.descriptor().colors[0].format;
        REQUIRE((format == ALMetalPixelFormat::rgba8_unorm || format == ALMetalPixelFormat::rgb10a2_unorm));
        auto normalized = renderer->renderDisplaySample(target, SIDE, SIDE, &error);
        INFO(error); REQUIRE(normalized);
        return read<Bytes>(*normalized);
    }
    std::unique_ptr<ALMetalRenderTarget> farDepth()
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = descriptor.height = SIDE;
        descriptor.color_attachment_count = 0;
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        descriptor.depth_stencil_shader_read = true;
        auto result = ALMetalRenderTarget::create(*device, descriptor, &error); REQUIRE(result);
        ALMetalRenderPassConfig clear;
        clear.clear_depth = 0.; clear.depth_store = ALMetalStoreAction::store;
        auto command = device->makeCommandBuffer("Native finalization far depth", &error); REQUIRE(command);
        auto pass = result->makeRenderPass(clear, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Authored depth", &error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        return result;
    }
};
unsigned changed(const std::vector<Bytes>& a, const std::vector<Bytes>& b)
{
    REQUIRE(a.size() == b.size()); unsigned count = 0;
    for (unsigned i = 0; i < a.size(); ++i)
        count += a[i][0] != b[i][0] || a[i][1] != b[i][1] || a[i][2] != b[i][2];
    return count;
}
void point_samples(const std::vector<Bytes>& actual, const std::vector<Bytes>& source)
{
    REQUIRE(actual.size() == SAMPLE * SAMPLE); REQUIRE(source.size() == SIDE * SIDE);
    // Odd 3:1 decimation lands exactly on source texel centers, avoiding a
    // tie at a nearest-sampling boundary. Raw row zero remains the lower row.
    for (unsigned y = 0; y < SAMPLE; ++y) for (unsigned x = 0; x < SAMPLE; ++x)
        CHECK(actual[y * SAMPLE + x] == source[(y * 3 + 1) * SIDE + x * 3 + 1]);
    CHECK(actual.front() != actual[(SAMPLE - 1) * SAMPLE]);
}
}

TEST_CASE("Native SDR DoF remains linear until display and scopes sample before print",
          "[almetal][native-finalize-admission]")
{
    @autoreleasepool
    {
        Fixture f;
        auto source = f.target(ALMetalPixelFormat::rgba16_float);
        std::vector<Halves> input(SIDE * SIDE);
        for (unsigned y = 0; y < SIDE; ++y) for (unsigned x = 0; x < SIDE; ++x)
            input[y * SIDE + x] = {half(x < SIDE / 2 ? .125f : 4.f),
                half(y < SIDE / 2 ? .25f : 2.f), half((x + y) % 3 ? .5f : 1.5f),
                half((x ^ y) & 1 ? .25f : .75f)};
        f.upload(*source, input);
        // Sparse scene-pixel tools must still see unclipped sharp linear
        // radiance and the original lower-row coordinates.
        for (const auto coordinate : {std::array<unsigned, 2>{0, 0}, {11, 11}, {3, 9}})
        {
            float pixel[4]{};
            REQUIRE(f.renderer->readSceneColorPixel(*source, coordinate[0], coordinate[1], pixel, &f.error));
            const auto& expected = input[coordinate[1] * SIDE + coordinate[0]];
            for (unsigned c = 0; c < 4; ++c) CHECK(pixel[c] == real(expected[c]));
        }
        auto depth = f.farDepth();
        for (bool reverse : {false, true})
        {
            CAPTURE(reverse);
            ALMetalWorldDepthOfFieldScene dof;
            dof.source = source.get(); dof.depth_texture = depth->depthStencilTexture();
            dof.inverse_projection[0] = dof.inverse_projection[5] = dof.inverse_projection[15] = 1.f;
            dof.inverse_projection[10] = reverse ? 2.f : 1.f;
            dof.inverse_projection[14] = reverse ? -3.f : -2.f;
            dof.focal_distance = -2.f; dof.blur_constant = 24.f / 1.414f;
            dof.tan_pixel_angle = dof.magnification = 1.f;
            dof.max_cof = 4.f; dof.rgba16_blur = true; dof.reverse_z = reverse;
            auto blurred = f.renderer->renderDepthOfField(dof, &f.error); INFO(f.error); REQUIRE(blurred);
            REQUIRE(blurred->descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
            const auto linear = f.read<Halves>(*blurred);
            unsigned linear_changes = 0, bright = 0;
            for (unsigned i = 0; i < linear.size(); ++i)
            {
                for (unsigned c = 0; c < 3; ++c) REQUIRE(std::isfinite(real(linear[i][c])));
                CHECK(linear[i][3] == input[i][3]);
                linear_changes += linear[i][0] != input[i][0] || linear[i][1] != input[i][1] || linear[i][2] != input[i][2];
                bright += real(linear[i][0]) > 1.f;
            }
            CHECK(linear_changes > SIDE * SIDE / 3); CHECK(bright > 0);
            ALMetalWorldDisplayPostScene display; display.source = blurred.get(); display.hdr = false;
            auto post = f.renderer->renderDisplayPost(display, &f.error); INFO(f.error); REQUIRE(post);
            REQUIRE(post->descriptor().colors[0].format == ALMetalPixelFormat::rgba8_unorm);
            display.source = source.get();
            auto sharp = f.renderer->renderDisplayPost(display, &f.error); REQUIRE(sharp);
            const auto pixels = f.read<Bytes>(*post);
            CHECK(changed(pixels, f.read<Bytes>(*sharp)) > SIDE * SIDE / 3);
            auto scope = f.renderer->renderDisplaySample(*post, SAMPLE, SAMPLE, &f.error); REQUIRE(scope);
            point_samples(f.read<Bytes>(*scope, SAMPLE), pixels);
            auto reference = f.renderer->capturePresentationReference(*post, &f.error); REQUIRE(reference);
            const auto reference_pixels = f.normalizedReference(*reference);
            // This fixture checks pre-print color/lifetime, not the separate
            // original RGB10_A2 selection/conversion contract. Reference
            // alpha follows that separate storage contract: 2-bit alpha
            // cannot retain authored display-post quarter-opacity values.
            REQUIRE(reference_pixels.size() == pixels.size());
            for (unsigned i = 0; i < pixels.size(); ++i)
                for (unsigned c = 0; c < 3; ++c) CHECK(reference_pixels[i][c] == pixels[i][c]);
        }
    }
}

TEST_CASE("Native pre-print still and scope owners survive drops cancellation and later print failure",
          "[almetal][native-finalize-admission]")
{
    @autoreleasepool
    {
        Fixture f;
        auto invalid_source = f.target(ALMetalPixelFormat::rgba16_float);
        f.error.clear();
        CHECK_FALSE(f.renderer->capturePresentationReference(*invalid_source, &f.error));
        CHECK_FALSE(f.error.empty());
        invalid_source.reset();
        for (bool ten_bit : {false, true})
        {
            CAPTURE(ten_bit);
            const auto format = ten_bit ? ALMetalPixelFormat::rgb10a2_unorm : ALMetalPixelFormat::rgba8_unorm;
            auto source = f.target(format);
            std::vector<Bytes> expected(SIDE * SIDE);
            std::vector<std::uint32_t> packed(SIDE * SIDE);
            for (unsigned y = 0; y < SIDE; ++y) for (unsigned x = 0; x < SIDE; ++x)
            {
                const unsigned r = x / 3, g = y / 3, b = (x + y) % 4, a = (x ^ y) % 4;
                expected[y * SIDE + x] = {std::uint8_t(r * 85), std::uint8_t(g * 85), std::uint8_t(b * 85), std::uint8_t(a * 85)};
                packed[y * SIDE + x] = r * 341 | ((g * 341) << 10) | ((b * 341) << 20) | (a << 30);
            }
            if (ten_bit) f.upload(*source, packed); else f.upload(*source, expected);
            auto frozen = f.renderer->capturePresentationReference(*source, &f.error); REQUIRE(frozen);
            // Authored thirds are exactly representable in both supported
            // storage families. The format-parity companion requires the
            // original always-10-bit selection and its packed conversion.
            if (ten_bit) CHECK(f.read<std::uint32_t>(*frozen) == packed);
            CHECK(f.normalizedReference(*frozen) == expected);

            // Original capture precedes the final print/window draw. A
            // committed offscreen still remains valid if that later owner
            // rejects an unrelated invalid output-size pair.
            ALMetalWorldPresentationScene invalid;
            invalid.source = frozen.get(); invalid.output_height = 5;
            f.error.clear(); CHECK_FALSE(f.renderer->renderPresentationPost(invalid, &f.error));
            CHECK_FALSE(f.error.empty());
            if (ten_bit) CHECK(f.read<std::uint32_t>(*frozen) == packed);
            CHECK(f.normalizedReference(*frozen) == expected);
            frozen.reset();

            REQUIRE(f.renderer->beginCommandBatch("Canceled reference transaction", &f.error));
            auto canceled = f.renderer->capturePresentationReference(*source, &f.error); REQUIRE(canceled);
            f.renderer->cancelCommandBatch(); canceled.reset();
            CHECK_FALSE(f.renderer->commandBatchActive());
            REQUIRE(f.renderer->beginCommandBatch("Reference and scope retained source", &f.error));
            frozen = f.renderer->capturePresentationReference(*source, &f.error); REQUIRE(frozen);
            auto scope = f.renderer->renderDisplaySample(*frozen, SAMPLE, SAMPLE, &f.error); REQUIRE(scope);
            const auto reference_format = frozen->descriptor().colors[0].format;
            source.reset(); frozen.reset();
            // A fresh same-description allocation/upload runs before the
            // pending batch. It cannot overwrite an encoded input lease.
            auto pressure = f.target(format);
            if (ten_bit) f.upload(*pressure, std::vector<std::uint32_t>(SIDE * SIDE, 0));
            else f.upload(*pressure, std::vector<Bytes>(SIDE * SIDE, Bytes{}));
            std::unique_ptr<ALMetalRenderTarget> reference_pressure;
            if (reference_format != format)
            {
                reference_pressure = f.target(reference_format);
                f.upload(*reference_pressure, std::vector<std::uint32_t>(SIDE * SIDE, 0));
            }
            REQUIRE(f.renderer->commitCommandBatch(&f.error));
            point_samples(f.read<Bytes>(*scope, SAMPLE), expected);
        }
    }
}
