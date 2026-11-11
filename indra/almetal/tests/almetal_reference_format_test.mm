/**
 * @file almetal_reference_format_test.mm
 * @brief Original RGB10_A2 still storage and native cross-mode presentation.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetalworld.h"
#include <Foundation/Foundation.h>
#include <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace
{
using RGBA=std::array<std::uint8_t,4>;
std::uint32_t pack(RGBA color)
{
    const auto rgb=[](std::uint32_t value){return (value*1023u+127u)/255u;};
    return rgb(color[0]) | (rgb(color[1])<<10) | (rgb(color[2])<<20) |
        (((std::uint32_t(color[3])*3u+127u)/255u)<<30);
}
RGBA unpack_bgra(std::uint32_t color)
{
    const auto rgb=[](std::uint32_t value){return std::uint8_t((value*255u+511u)/1023u);};
    return {rgb((color>>20)&1023u),rgb((color>>10)&1023u),rgb(color&1023u),std::uint8_t((color>>30)*85u)};
}
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    bool saved_render_info=ALMetalDevice::renderInfo().enabled;
    Fixture()
    {
        REQUIRE([[NSBundle mainBundle] pathForResource:@"almetal_builtin" ofType:@"metallib"]!=nil);
        device=ALMetalDevice::create(&error);INFO(error);REQUIRE(device);
        renderer=ALMetalWorldGBufferRenderer::create(*device,1,&error);INFO(error);REQUIRE(renderer);
    }
    ~Fixture(){if(device)device->waitUntilIdle();ALMetalDevice::beginRenderInfoFrame(saved_render_info);}
    std::unique_ptr<ALMetalRenderTarget> target(ALMetalPixelFormat format,unsigned w,unsigned h,const void* data=nullptr,unsigned attachments=1)
    {
        ALMetalRenderTargetDescriptor descriptor;descriptor.width=w;descriptor.height=h;descriptor.color_attachment_count=attachments;
        for(unsigned i=0;i<attachments;++i)descriptor.colors[i].format=format;
        auto value=ALMetalRenderTarget::create(*device,descriptor,&error);INFO(error);REQUIRE(value);
        if(data)
        {
            auto command=device->makeCommandBuffer("Reference authored upload",&error);REQUIRE(command);
            ALMetalTextureDescriptor upload;upload.width=w;upload.height=h;upload.format=format;
            auto authored=ALMetalTexture::create(*device,upload,&error);REQUIRE(authored);
            REQUIRE(authored->upload(*command,0,0,{},data,w*4,&error));
            REQUIRE(value->copyColorFrom(*command,0,*authored,{},&error));REQUIRE(command->commitAndWait(&error));
        }
        return value;
    }
    template<class T>std::vector<T> read(const ALMetalRenderTarget& target)
    {
        const auto& descriptor=target.descriptor();std::vector<T> result(descriptor.width*descriptor.height);
        REQUIRE(target.colorTexture(0)->readback(*device,0,0,{},result.data(),descriptor.width*sizeof(T),&error));return result;
    }
    std::unique_ptr<ALMetalRenderTarget> present(const ALMetalRenderTarget& source,const ALMetalTexture& reference,unsigned mode)
    {
        ALMetalWorldPresentationScene scene;scene.source=&source;scene.reference_texture=&reference;scene.reference_mode=mode;
        auto value=renderer->renderPresentationPost(scene,&error);INFO(error);return value;
    }
};
void compare_packed(const std::vector<std::uint32_t>& actual,const std::vector<std::uint32_t>& expected)
{
    REQUIRE(actual.size()==expected.size());unsigned errors=0;
    for(unsigned i=0;i<actual.size();++i)if(actual[i]!=expected[i])
    {if(errors<4)UNSCOPED_INFO("packed pixel "<<i<<": "<<actual[i]<<" expected "<<expected[i]);++errors;}
    CHECK(errors==0);
}
}

TEST_CASE("Native still converts every SDR component to original RGB10 A2 storage", "[almetal][reference-format]")
{
    @autoreleasepool
    {
        Fixture f;constexpr unsigned w=256,h=4;std::vector<RGBA> pixels(w*h);std::vector<std::uint32_t> expected(w*h);
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)
        {
            pixels[y*w+x]={std::uint8_t(x),std::uint8_t(x*37+y*19),std::uint8_t(255-x+y*63),std::uint8_t(x+y*67)};
            expected[y*w+x]=pack(pixels[y*w+x]);
        }
        auto source=f.target(ALMetalPixelFormat::rgba8_unorm,w,h,pixels.data());
        ALMetalDevice::beginRenderInfoFrame(true);
        auto reference=f.renderer->capturePresentationReference(*source,&f.error);INFO(f.error);REQUIRE(reference);
        CHECK(reference->descriptor().colors[0].format==ALMetalPixelFormat::rgb10a2_unorm);
        CHECK([(__bridge id<MTLTexture>)reference->colorTexture(0)->nativeHandle() pixelFormat]==MTLPixelFormatRGB10A2Unorm);
        compare_packed(f.read<std::uint32_t>(*reference),expected);
        const auto counts=ALMetalDevice::renderInfo();CHECK(counts.draw_calls==1);CHECK(counts.input_elements==3);
        CHECK(counts.draw_calls_known);CHECK(counts.input_elements_known);
        CHECK(f.read<RGBA>(*source)==pixels);
    }
}
TEST_CASE("Native still preserves raw ten-bit copies and batch cancellation", "[almetal][reference-format]")
{
    @autoreleasepool
    {
        Fixture f;constexpr unsigned w=32,h=8;std::vector<std::uint32_t> pixels(w*h);
        for(unsigned i=0;i<pixels.size();++i)pixels[i]=((i*43+7)&1023u)|(((i*83+49)&1023u)<<10)|(((1023-i*7)&1023u)<<20)|((i&3u)<<30);
        auto source=f.target(ALMetalPixelFormat::rgb10a2_unorm,w,h,pixels.data());
        ALMetalDevice::beginRenderInfoFrame(true);
        auto reference=f.renderer->capturePresentationReference(*source,&f.error);REQUIRE(reference);
        compare_packed(f.read<std::uint32_t>(*reference),pixels);CHECK(ALMetalDevice::renderInfo().draw_calls==0);
        REQUIRE(f.renderer->beginCommandBatch("Reference cancelled capture",&f.error));
        auto cancelled=f.renderer->capturePresentationReference(*source,&f.error);REQUIRE(cancelled);
        f.renderer->cancelCommandBatch();CHECK_FALSE(f.renderer->commandBatchActive());cancelled.reset();
        REQUIRE(f.renderer->beginCommandBatch("Reference valid capture retry",&f.error));
        auto retry=f.renderer->capturePresentationReference(*source,&f.error);REQUIRE(retry);
        REQUIRE(f.renderer->commitCommandBatch(&f.error));compare_packed(f.read<std::uint32_t>(*retry),pixels);
        compare_packed(f.read<std::uint32_t>(*reference),pixels);
        const RGBA sdr_color{43,211,127,63};std::vector<RGBA> sdr_pixels(w*h,sdr_color);
        auto sdr=f.target(ALMetalPixelFormat::rgba8_unorm,w,h,sdr_pixels.data());
        REQUIRE(f.renderer->beginCommandBatch("Reference cancelled SDR conversion",&f.error));
        auto discarded=f.renderer->capturePresentationReference(*sdr,&f.error);REQUIRE(discarded);
        f.renderer->cancelCommandBatch();CHECK_FALSE(f.renderer->commandBatchActive());discarded.reset();
        CHECK(ALMetalDevice::renderInfo().draw_calls==1); // Encoded, even though not submitted.
        REQUIRE(f.renderer->beginCommandBatch("Reference SDR conversion retry",&f.error));
        auto converted=f.renderer->capturePresentationReference(*sdr,&f.error);REQUIRE(converted);
        REQUIRE(f.renderer->commitCommandBatch(&f.error));
        compare_packed(f.read<std::uint32_t>(*converted),std::vector<std::uint32_t>(w*h,pack(sdr_color)));
        CHECK(ALMetalDevice::renderInfo().draw_calls==2);CHECK(ALMetalDevice::renderInfo().input_elements==6);
        compare_packed(f.read<std::uint32_t>(*reference),pixels);
    }
}
TEST_CASE("Native reference presentation accepts normalized formats across display modes", "[almetal][reference-format]")
{
    @autoreleasepool
    {
        Fixture f;constexpr unsigned w=8,h=4;const RGBA s8{17,211,60,198},r8{230,31,77,63};
        const std::uint32_t s10=1019u|(293u<<10)|(11u<<20)|(2u<<30),r10=900u|(36u<<10)|(622u<<20)|(1u<<30);
        std::vector<RGBA> source8(w*h,s8),reference8(w*h,r8);std::vector<std::uint32_t> source10(w*h,s10),reference10(w*h,r10);
        auto a=f.target(ALMetalPixelFormat::rgba8_unorm,w,h,source8.data());auto b=f.target(ALMetalPixelFormat::rgb10a2_unorm,w,h,source10.data());
        auto c=f.target(ALMetalPixelFormat::rgba8_unorm,w,h,reference8.data());auto d=f.target(ALMetalPixelFormat::rgb10a2_unorm,w,h,reference10.data());
        for(bool source_hdr:{false,true})for(bool reference_hdr:{false,true})for(unsigned mode:{1u,2u})
        {
            CAPTURE(source_hdr,reference_hdr,mode);
            auto output=f.present(source_hdr?*b:*a,*(reference_hdr?d:c)->colorTexture(0),mode);REQUIRE(output);
            const auto actual=f.read<RGBA>(*output);const RGBA expected_reference=reference_hdr?unpack_bgra(r10):RGBA{r8[2],r8[1],r8[0],r8[3]};
            const RGBA expected_source=source_hdr?unpack_bgra(s10):RGBA{s8[2],s8[1],s8[0],s8[3]};
            CHECK(actual[(h/2)*w+1]==expected_reference);CHECK(actual[(h/2)*w+w-2]==expected_source);
        }
        const auto held=f.read<std::uint32_t>(*d);
        for(unsigned variant=0;variant<5;++variant)
        {
            ALMetalTextureDescriptor invalid;invalid.width=w;invalid.height=h;
            if(variant==0)invalid.format=ALMetalPixelFormat::rgba16_float;
            if(variant==1)invalid.width=w/2;
            if(variant==2)invalid.mipmap_level_count=2;
            if(variant==3)invalid.usage=ALMetalTextureUsage::render_target;
            if(variant==4){invalid.type=ALMetalTextureType::texture_2d_array;invalid.array_length=2;}
            auto texture=ALMetalTexture::create(*f.device,invalid,&f.error);REQUIRE(texture);f.error.clear();
            CHECK_FALSE(f.present(*a,*texture,1));CHECK(f.error=="Metal world presentation reference is incompatible");
        }
        auto half=f.target(ALMetalPixelFormat::rgba16_float,w,h);auto mrt=f.target(ALMetalPixelFormat::rgba8_unorm,w,h,nullptr,2);
        for(auto* invalid:{half.get(),mrt.get()})
        {f.error.clear();CHECK_FALSE(f.renderer->capturePresentationReference(*invalid,&f.error));CHECK(f.error=="Metal world presentation-reference source is incompatible");}
        compare_packed(f.read<std::uint32_t>(*d),held);
    }
}
TEST_CASE("Native reference commands retain converted storage and pipeline owners", "[almetal][reference-format]")
{
    @autoreleasepool
    {
        Fixture f;constexpr unsigned w=8,h=4;const RGBA color{224,32,80,63};std::vector<RGBA> pixels(w*h,color);
        auto source=f.target(ALMetalPixelFormat::rgba8_unorm,w,h,pixels.data());
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct ReleaseGate{id<MTLSharedEvent> value;~ReleaseGate(){value.signaledValue=1;}} release{gate};
        auto blocker=f.device->makeCommandBuffer("Reference lifetime gate",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        REQUIRE(f.renderer->beginCommandBatch("Retained reference conversion and presentation",&f.error));
        auto reference=f.renderer->capturePresentationReference(*source,&f.error);REQUIRE(reference);
        auto output=f.present(*source,*reference->colorTexture(0),1);REQUIRE(output);
        REQUIRE(f.renderer->commitCommandBatch(&f.error));
        source.reset();reference.reset();f.renderer.reset();
        CHECK(blocker->status()!=ALMetalCommandStatus::completed);
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));
        const auto actual=f.read<RGBA>(*output);
        CHECK(actual[(h/2)*w+1]==unpack_bgra(pack(color)));
        CHECK(actual[(h/2)*w+w-2]==RGBA{color[2],color[1],color[0],color[3]});
    }
}
