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
#include <cstring>
#include <vector>

namespace
{
using HalfImage = std::array<std::uint16_t, 8>;
using BytePixel = std::array<std::uint8_t, 4>;
using ByteImage = std::vector<BytePixel>;

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalSamplerState> mirror;
    std::unique_ptr<ALMetalRenderPipeline> copy;
    std::unique_ptr<ALMetalRenderPipeline> copy_rgba8;
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
        pipeline.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        copy_rgba8 = ALMetalRenderPipeline::create(*device, pipeline, &error); REQUIRE(copy_rgba8);
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
    std::unique_ptr<ALMetalRenderTarget> asymmetricSource()
    {
        ALMetalRenderTargetDescriptor d;
        d.width = d.height = 2;
        d.colors[0].format = ALMetalPixelFormat::rgba16_float;
        auto value = ALMetalRenderTarget::create(*device, d, &error); REQUIRE(value);
        // Row zero is the original GL lower row, not the top of a screenshot.
        // Distinct RGB and alpha rows make a flipped/warped alpha source fail.
        const std::array<float, 16> authored{
            .125f,.25f,.5f,1.f/512.f, .5f,.125f,.25f,3.f/512.f,
            .25f,.5f,.125f,.25f, .125f,.25f,.5f,.75f,
        };
        std::array<std::uint16_t, 16> bits{};
        for (unsigned i=0;i<bits.size();++i)
        {
            const __fp16 half=authored[i];
            std::memcpy(&bits[i],&half,sizeof(half));
        }
        auto command=device->makeCommandBuffer("Asymmetric raw coverage rows",&error); REQUIRE(command);
        REQUIRE(const_cast<ALMetalTexture*>(value->colorTexture(0))->upload(*command,0,0,{},bits.data(),16,&error));
        REQUIRE(command->commitAndWait(&error));
        std::array<std::uint16_t,16> stored{};
        REQUIRE(value->colorTexture(0)->readback(*device,0,0,{},stored.data(),16,&error));
        REQUIRE(stored==bits);
        return value;
    }
    ByteImage read(const ALMetalRenderTarget& value, unsigned width=10, unsigned height=1)
    {
        REQUIRE(value.descriptor().width == width);
        REQUIRE(value.descriptor().height == height);
        REQUIRE(value.descriptor().colors[0].format == ALMetalPixelFormat::bgra8_unorm);
        ByteImage pixels(width*height);
        REQUIRE(value.colorTexture(0)->readback(*device, 0, 0, {}, pixels.data(), width*4, &error));
        return pixels;
    }
    std::unique_ptr<ALMetalRenderTarget> resample(
        const ALMetalRenderTarget& source, ALMetalPixelFormat format,
        unsigned width, unsigned height)
    {
        // Original1eb copyCutoutCoverageToAlpha samples raw RGBA16F Screen
        // at the final raw viewport using BilinearMirror, then stores U8.
        // Call the existing native resample shader; no alpha algorithm copy.
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = width; descriptor.height = height;
        descriptor.colors[0].format = format;
        auto value = ALMetalRenderTarget::create(*device, descriptor, &error); REQUIRE(value);
        auto command = device->makeCommandBuffer("Original float final-copy control", &error); REQUIRE(command);
        ALMetalRenderPassConfig config;
        config.colors[0].load = ALMetalLoadAction::dont_care;
        auto pass = value->makeRenderPass(config, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Float coverage final viewport", &error); REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(format==ALMetalPixelFormat::bgra8_unorm?*copy:*copy_rgba8, &error));
        REQUIRE(encoder->setViewport({0, 0, double(width), double(height), 0, 1}, &error));
        REQUIRE(encoder->setScissor({0, 0, width, height}, &error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
        REQUIRE(encoder->setFragmentTexture(0, source.colorTexture(0), &error));
        REQUIRE(encoder->setFragmentSampler(0, mirror.get(), &error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        return value;
    }
    ByteImage originalFinalCopy(const ALMetalRenderTarget& source, unsigned width=10, unsigned height=1)
    {
        auto value=resample(source,ALMetalPixelFormat::bgra8_unorm,width,height);
        return read(*value,width,height);
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
            display.cutout_premultiplied = true;
            auto post = f.renderer->renderDisplayPost(display, &f.error); INFO(f.error); REQUIRE(post);
            CHECK(post->descriptor().colors[0].format ==
                  (hdr?ALMetalPixelFormat::rgb10a2_unorm:ALMetalPixelFormat::rgba8_unorm));
            ALMetalWorldPresentationScene presentation;
            presentation.source = post.get(); presentation.output_width = 10; presentation.output_height = 1;
            presentation.cutout_coverage_texture=source->colorTexture(0);
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

TEST_CASE("Cutout float coverage keeps raw rows and leaves final RGB untouched",
          "[almetal][cutout-alpha-resample]")
{
    @autoreleasepool
    {
        Fixture f; auto raw=f.asymmetricSource();
        const auto original=f.originalFinalCopy(*raw,10,5);
        // The asymmetric lower row is below one8-bit unit; the upper row
        // has broad fractional coverage. This is a positive Y orientation
        // control, independent of display/reference/lens sampling.
        REQUIRE(original[4][3]==1);
        REQUIRE(original[4*10+4][3]>100);
        for(bool hdr:{false,true})
        {
            CAPTURE(hdr);
            ALMetalWorldDisplayPostScene display;
            display.source=raw.get(); display.hdr=hdr;
            display.cutout_premultiplied=true;
            auto post=f.renderer->renderDisplayPost(display,&f.error); REQUIRE(post);
            CHECK(post->descriptor().colors[0].format ==
                  (hdr?ALMetalPixelFormat::rgb10a2_unorm:ALMetalPixelFormat::rgba8_unorm));
            ALMetalWorldPresentationScene scene;
            scene.source=post.get(); scene.output_width=10; scene.output_height=5;
            auto neutral=f.renderer->renderPresentationPost(scene,&f.error); REQUIRE(neutral);
            const auto neutral_pixels=f.read(*neutral,10,5);
            for(unsigned reference:{0u,1u,2u})
            {
                CAPTURE(reference);
                scene.reference_mode=reference; scene.reference_texture=post->colorTexture(0);
                scene.reference_position=.4f;
                scene.lens_distortion_amount=.8f;
                scene.lens_distortion_k[0]=-.2f; scene.lens_distortion_k[1]=.05f;
                scene.lens_distortion_scale=.85f;
                scene.lens_distortion_squeeze[0]=1.2f; scene.lens_distortion_squeeze[1]=.9f;
                scene.lens_distortion_center[0]=.1f; scene.lens_distortion_center[1]=-.1f;
                scene.lens_distortion_tangential[0]=.02f; scene.lens_distortion_tangential[1]=-.01f;
                scene.vignette_amount=.4f; scene.cvd_mode=1; scene.cvd_amount=.5f;
                scene.grain_amount=.3f; scene.grain_animated=false;
                scene.dither_amount=.4f; scene.dither_animated=false;
                scene.preview_mode=3; scene.viewport_origin[0]=11; scene.viewport_origin[1]=7;
                scene.cutout_coverage_texture=nullptr;
                auto ordinary=f.renderer->renderPresentationPost(scene,&f.error); REQUIRE(ordinary);
                const auto ordinary_pixels=f.read(*ordinary,10,5);
                scene.cutout_coverage_texture=raw->colorTexture(0);
                auto cutout=f.renderer->renderPresentationPost(scene,&f.error); REQUIRE(cutout);
                const auto actual=f.read(*cutout,10,5);
                unsigned color_changes=0,alpha_changes=0;
                for(unsigned p=0;p<actual.size();++p)
                {
                    for(unsigned c=0;c<3;++c)
                    {
                        CHECK(actual[p][c]==ordinary_pixels[p][c]);
                        color_changes+=actual[p][c]!=neutral_pixels[p][c];
                    }
                    CHECK(actual[p][3]==original[p][3]);
                    alpha_changes+=actual[p][3]!=ordinary_pixels[p][3];
                }
                CHECK(color_changes>0); CHECK(alpha_changes>0);
            }
        }
    }
}

TEST_CASE("Cutout float coverage rejects incompatible inputs without replacing output",
          "[almetal][cutout-alpha-resample]")
{
    @autoreleasepool
    {
        Fixture f; auto raw=f.asymmetricSource();
        ALMetalWorldDisplayPostScene display; display.source=raw.get(); display.cutout_premultiplied=true;
        auto post=f.renderer->renderDisplayPost(display,&f.error); REQUIRE(post);
        ALMetalWorldPresentationScene scene; scene.source=post.get(); scene.output_width=10; scene.output_height=5;
        scene.cutout_coverage_texture=raw->colorTexture(0);
        auto valid=f.renderer->renderPresentationPost(scene,&f.error); REQUIRE(valid);
        const auto previous=f.read(*valid,10,5);
        for(unsigned invalid=0;invalid<6;++invalid)
        {
            CAPTURE(invalid);
            ALMetalTextureDescriptor d; d.width=d.height=2;
            d.format=ALMetalPixelFormat::rgba16_float;
            d.storage=ALMetalStorageMode::private_gpu; d.usage=ALMetalTextureUsage::shader_read;
            switch(invalid)
            {
            case 0:d.width=1;break;
            case 1:d.format=ALMetalPixelFormat::rgba8_unorm;break;
            case 2:d.mipmap_level_count=2;break;
            case 3:d.type=ALMetalTextureType::texture_2d_array;d.array_length=2;break;
            case 4:d.type=ALMetalTextureType::texture_2d_multisample;d.sample_count=2;d.usage=ALMetalTextureUsage::render_target;break;
            case 5:d.usage=ALMetalTextureUsage::render_target;break;
            }
            auto incompatible=ALMetalTexture::create(*f.device,d,&f.error); INFO(f.error); REQUIRE(incompatible);
            scene.cutout_coverage_texture=incompatible.get(); f.error.clear();
            CHECK_FALSE(f.renderer->renderPresentationPost(scene,&f.error));
            CHECK_FALSE(f.error.empty()); CHECK(f.read(*valid,10,5)==previous);
        }
        // Absence is still a legitimate ordinary presentation, not an
        // implicit request to find a previous raw coverage owner.
        scene.cutout_coverage_texture=nullptr;
        auto ordinary=f.renderer->renderPresentationPost(scene,&f.error); REQUIRE(ordinary);
        const auto ordinary_pixels=f.read(*ordinary,10,5);
        CHECK(ordinary_pixels!=previous);
    }
}

TEST_CASE("HDR cutout keeps original ten-bit RGB and independent float alpha boundaries",
          "[almetal][cutout-alpha-resample]")
{
    @autoreleasepool
    {
        Fixture f; auto raw=f.asymmetricSource();
        ALMetalWorldDisplayPostScene display;
        display.source=raw.get(); display.hdr=true; display.cutout_premultiplied=true;
        auto post=f.renderer->renderDisplayPost(display,&f.error); REQUIRE(post);
        REQUIRE(post->descriptor().colors[0].format==ALMetalPixelFormat::rgb10a2_unorm);
        constexpr unsigned width=17,height=9;
        // Original colorCorrect lands in HDR PostPing RGB10_A2, then final
        // color samples that target. The late alpha copy instead samples raw
        // F16 Screen. Exercise both existing native resample controls without
        // recreating grading, alpha filtering or transfer functions on CPU.
        const auto original_rgb=f.originalFinalCopy(*post,width,height);
        const auto original_alpha=f.originalFinalCopy(*raw,width,height);
        auto premature_u8=f.resample(*post,ALMetalPixelFormat::rgba8_unorm,2,2);
        const auto wrong_rgb=f.originalFinalCopy(*premature_u8,width,height);
        ALMetalWorldPresentationScene scene;
        scene.source=post.get(); scene.cutout_coverage_texture=raw->colorTexture(0);
        scene.output_width=width; scene.output_height=height;
        auto output=f.renderer->renderPresentationPost(scene,&f.error); REQUIRE(output);
        const auto actual=f.read(*output,width,height);
        unsigned premature_differences=0;
        for(unsigned p=0;p<actual.size();++p)
        {
            for(unsigned c=0;c<3;++c)
            {
                CHECK(actual[p][c]==original_rgb[p][c]);
                premature_differences+=actual[p][c]!=wrong_rgb[p][c];
            }
            CHECK(actual[p][3]==original_alpha[p][3]);
        }
        CHECK(premature_differences>0);
        // Final presentation takes normalized display color, not raw F16
        // Screen RGB. The latter is solely the optional coverage producer.
        scene.source=raw.get(); f.error.clear();
        CHECK_FALSE(f.renderer->renderPresentationPost(scene,&f.error));
        CHECK_FALSE(f.error.empty());
        CHECK(f.read(*output,width,height)==actual);
    }
}

TEST_CASE("Cutout float coverage supports batch cancel retry and encoded source retirement",
          "[almetal][cutout-alpha-resample]")
{
    @autoreleasepool
    {
        Fixture f; auto raw=f.asymmetricSource();
        const auto original=f.originalFinalCopy(*raw,10,5);
        ALMetalWorldDisplayPostScene display; display.source=raw.get(); display.cutout_premultiplied=true;
        auto post=f.renderer->renderDisplayPost(display,&f.error); REQUIRE(post);
        ALMetalWorldPresentationScene scene; scene.source=post.get(); scene.output_width=10; scene.output_height=5;
        scene.cutout_coverage_texture=raw->colorTexture(0);
        REQUIRE(f.renderer->beginCommandBatch("Canceled cutout raw coverage",&f.error));
        auto canceled=f.renderer->renderPresentationPost(scene,&f.error); REQUIRE(canceled);
        f.renderer->cancelCommandBatch(); canceled.reset();
        REQUIRE(f.renderer->beginCommandBatch("Retained cutout raw coverage retry",&f.error));
        auto output=f.renderer->renderPresentationPost(scene,&f.error); REQUIRE(output);
        // The actual encoded command must retain both sampled native images
        // after their C++ wrappers retire, before this batch is submitted.
        raw.reset(); post.reset();
        REQUIRE(f.renderer->commitCommandBatch(&f.error));
        f.renderer.reset();
        const auto actual=f.read(*output,10,5);
        for(unsigned p=0;p<actual.size();++p) CHECK(actual[p][3]==original[p][3]);
    }
}
