/**
 * @file almetal_cutout_alpha_resample_test.mm
 * @brief Original float-coverage final-copy boundary during native resizing.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetalworld.h"
#include <Foundation/Foundation.h>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>

namespace
{
using HalfImage = std::array<std::uint16_t, 8>;
using BytePixel = std::array<std::uint8_t, 4>;
using ByteImage = std::array<BytePixel, 10>;

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalSamplerState> mirror;
    std::unique_ptr<ALMetalRenderPipeline> copy;
    Fixture()
    {
        REQUIRE([[NSBundle mainBundle] pathForResource:@"almetal_builtin" ofType:@"metallib"] != nil);
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        library = ALMetalShaderLibrary::loadBuiltin(*device, &error); INFO(error); REQUIRE(library);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error); INFO(error); REQUIRE(renderer);
        ALMetalSamplerDescriptor sampler;
        sampler.min_filter = sampler.mag_filter = ALMetalSamplerFilter::linear;
        sampler.address_s = sampler.address_t = sampler.address_r = ALMetalSamplerAddressMode::mirror_repeat;
        mirror = ALMetalSamplerState::create(*device, sampler, &error); REQUIRE(mirror);
        ALMetalRenderPipelineDescriptor pipeline;
        pipeline.library = library.get();
        pipeline.vertex_function = "almetal_resample_vertex";
        pipeline.fragment_function = "almetal_resample_fragment";
        pipeline.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
        copy = ALMetalRenderPipeline::create(*device, pipeline, &error); INFO(error); REQUIRE(copy);
    }
    ~Fixture() { if (device) device->waitUntilIdle(); }

    std::unique_ptr<ALMetalRenderTarget> source()
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = 2; descriptor.height = 1;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
        auto value = ALMetalRenderTarget::create(*device, descriptor, &error); REQUIRE(value);
        // Exactly representable half values 1/512 and 3/512. RGB is black,
        // isolating coverage from the separate display RGB transfer.
        const HalfImage pixels{0, 0, 0, 0x1800, 0, 0, 0, 0x1e00};
        ALMetalTextureDescriptor upload;
        upload.width = 2; upload.height = 1; upload.format = ALMetalPixelFormat::rgba16_float;
        auto authored = ALMetalTexture::create(*device, upload, &error); REQUIRE(authored);
        auto command = device->makeCommandBuffer("Authored float cutout coverage", &error); REQUIRE(command);
        REQUIRE(authored->upload(*command, 0, 0, {}, pixels.data(), 16, &error));
        REQUIRE(value->copyColorFrom(*command, 0, *authored, {}, &error));
        REQUIRE(command->commitAndWait(&error));
        HalfImage stored{};
        REQUIRE(value->colorTexture(0)->readback(*device, 0, 0, {}, stored.data(), 16, &error));
        REQUIRE(stored == pixels);
        return value;
    }
    ByteImage read(const ALMetalRenderTarget& value)
    {
        REQUIRE(value.descriptor().width == 10);
        REQUIRE(value.descriptor().height == 1);
        REQUIRE(value.descriptor().colors[0].format == ALMetalPixelFormat::bgra8_unorm);
        ByteImage pixels{};
        REQUIRE(value.colorTexture(0)->readback(*device, 0, 0, {}, pixels.data(), 40, &error));
        return pixels;
    }
    ByteImage originalFinalCopy(const ALMetalRenderTarget& source)
    {
        // Original1eb copyCutoutCoverageToAlpha samples raw RGBA16F Screen
        // at the final raw viewport using BilinearMirror, then stores U8.
        // Call the existing native resample shader; no alpha algorithm copy.
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = 10; descriptor.height = 1;
        descriptor.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
        auto value = ALMetalRenderTarget::create(*device, descriptor, &error); REQUIRE(value);
        auto command = device->makeCommandBuffer("Original float final-copy control", &error); REQUIRE(command);
        ALMetalRenderPassConfig config;
        config.colors[0].load = ALMetalLoadAction::dont_care;
        auto pass = value->makeRenderPass(config, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Float coverage final viewport", &error); REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*copy, &error));
        REQUIRE(encoder->setViewport({0, 0, 10, 1, 0, 1}, &error));
        REQUIRE(encoder->setScissor({0, 0, 10, 1}, &error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
        REQUIRE(encoder->setFragmentTexture(0, source.colorTexture(0), &error));
        REQUIRE(encoder->setFragmentSampler(0, mirror.get(), &error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        return read(*value);
    }
};
}

TEST_CASE("Native resized cutout alpha keeps the original float final-copy precision",
          "[almetal][cutout-alpha-resample]")
{
    @autoreleasepool
    {
        Fixture f;
        auto source = f.source();
        const auto original = f.originalFinalCopy(*source);
        // Pixel4 samples x=.45, hence source weight .4: exact float alpha
        // maps to .896484375 U8 units. Prequantized 0/1 maps to .4 instead.
        REQUIRE(original[4][3] == 1);
        for (bool hdr : {false, true})
        {
            CAPTURE(hdr);
            ALMetalWorldDisplayPostScene display;
            display.source = source.get(); display.hdr = hdr;
            display.cutout_premultiplied = true; display.preserve_alpha = hdr;
            auto post = f.renderer->renderDisplayPost(display, &f.error); INFO(f.error); REQUIRE(post);
            ALMetalWorldPresentationScene presentation;
            presentation.source = post.get(); presentation.output_width = 10; presentation.output_height = 1;
            auto final = f.renderer->renderPresentationPost(presentation, &f.error); INFO(f.error); REQUIRE(final);
            const auto actual = f.read(*final);
            for (unsigned x = 0; x < 10; ++x)
            {
                CAPTURE(x, actual[x][3], original[x][3]);
                CHECK(actual[x][3] == original[x][3]);
            }
        }
    }
}
