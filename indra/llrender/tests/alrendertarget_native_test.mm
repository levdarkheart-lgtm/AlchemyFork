/**
 * @file alrendertarget_native_test.mm
 * @brief Original render-target ownership invariants on actual Metal resources.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project.
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llimagegl.h"
#include "../llgltexture.h"
#include "../llrendertarget.h"
#include "../llglstates.h"
#include "../../almetal/almetalpipeline.h"
#import <Metal/Metal.h>
#import <objc/runtime.h>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <vector>
#include <limits>

namespace
{
using Depth = ALRenderTargetDesc::Depth;
using Mips = ALRenderTargetDesc::Mips;
using EnsureResult = LLRenderTarget::EnsureResult;
constexpr ALRenderTargetDesc GBUFFER{.width=6,.height=5,.color={GL_RGBA8,GL_RGBA16F},.depth=Depth::D24,.stencil=true};
struct NamedDesc { const char* what; ALRenderTargetDesc desc; };

id<MTLTexture> textureFactory(id,SEL,MTLTextureDescriptor*) __attribute__((ns_returns_retained));
struct TextureFailure
{
    static TextureFailure* active;
    Method method=nullptr; IMP original=nullptr; id device=nil; unsigned calls=0;
    explicit TextureFailure(const ALMetalDevice& owner):device((__bridge id)owner.nativeHandle())
    {
        REQUIRE_FALSE(active);
        method=class_getInstanceMethod(object_getClass(device),@selector(newTextureWithDescriptor:));
        REQUIRE(method);original=method_getImplementation(method);REQUIRE(original);
        active=this;method_setImplementation(method,reinterpret_cast<IMP>(&textureFactory));
    }
    ~TextureFailure(){method_setImplementation(method,original);active=nullptr;}
};
TextureFailure* TextureFailure::active=nullptr;
id<MTLTexture> textureFactory(id device,SEL selector,MTLTextureDescriptor* descriptor)
{
    auto& state=*TextureFailure::active;
    if(device==state.device && ++state.calls==1) return nil;
    return reinterpret_cast<decltype(&textureFactory)>(state.original)(device,selector,descriptor);
}

U64 bytesOf(const ALRenderTargetDesc& desc)
{
    U64 bytes=0, pixels=0;
    U32 w=desc.width,h=desc.height;
    const U32 levels=desc.mips==Mips::None?1:std::bit_width(std::max(w,h));
    for (U32 i=0;i<levels;++i) { pixels+=U64(w)*h;w=std::max(1u,w/2);h=std::max(1u,h/2); }
    for (U32 i=0;i<desc.colorCount();++i)
        bytes+=pixels*(desc.color[i]==GL_RGBA16F?8:desc.color[i]==GL_R8?1:desc.color[i]==GL_RGBA32UI?16:4);
    // Metal physically maps both retained logical depth choices to D32F.
    if (desc.depth!=Depth::None) bytes+=U64(desc.width)*desc.height*(desc.stencil?8:4);
    return bytes;
}
void requireReleased(const LLRenderTarget& value)
{
    CHECK(value.getNumTextures()==0); CHECK_FALSE(value.hasDepthAttachment());
    CHECK_FALSE(value.sharesDepth()); CHECK_FALSE(value.isComplete());
    CHECK(value.desc()==ALRenderTargetDesc{});
}
void requireStorage(const LLRenderTarget& value,const ALRenderTargetDesc& desc)
{
    REQUIRE(value.getNumTextures()==desc.colorCount());
    const U32 levels=desc.mips==Mips::None?1:std::bit_width(std::max(desc.width,desc.height));
    for (U32 i=0;i<desc.colorCount();++i)
    {
        const auto* texture=value.getMetalColorTexture(i); REQUIRE(texture);
        CHECK(value.getInternalFormat(i)==desc.color[i]);
        CHECK(texture->descriptor().width==desc.width); CHECK(texture->descriptor().height==desc.height);
        CHECK(texture->descriptor().mipmap_level_count==levels);
        id<MTLTexture> actual=(__bridge id<MTLTexture>)texture->nativeHandle();
        CHECK(actual.width==desc.width);CHECK(actual.height==desc.height);CHECK(actual.mipmapLevelCount==levels);
        switch(desc.color[i])
        {
            case GL_RGBA8:CHECK(texture->descriptor().format==ALMetalPixelFormat::rgba8_unorm);CHECK(actual.pixelFormat==MTLPixelFormatRGBA8Unorm);break;
            case GL_RGBA16F:CHECK(texture->descriptor().format==ALMetalPixelFormat::rgba16_float);CHECK(actual.pixelFormat==MTLPixelFormatRGBA16Float);break;
            case GL_RGB10_A2:CHECK(texture->descriptor().format==ALMetalPixelFormat::rgb10a2_unorm);CHECK(actual.pixelFormat==MTLPixelFormatRGB10A2Unorm);break;
            case GL_R32F:CHECK(texture->descriptor().format==ALMetalPixelFormat::r32_float);CHECK(actual.pixelFormat==MTLPixelFormatR32Float);break;
            case GL_R8:CHECK(texture->descriptor().format==ALMetalPixelFormat::r8_unorm);CHECK(actual.pixelFormat==MTLPixelFormatR8Unorm);break;
            default:FAIL("Missing physical format oracle for the original target description");
        }
    }
    if (desc.depth!=Depth::None)
    {
        REQUIRE(value.getMetalDepthTexture());
        CHECK(value.getMetalDepthTexture()->descriptor().width==desc.width);
        CHECK(value.getMetalDepthTexture()->descriptor().height==desc.height);
        CHECK(value.getMetalDepthTexture()->descriptor().format==(desc.stencil?
            ALMetalPixelFormat::depth32_float_stencil8:ALMetalPixelFormat::depth32_float));
        CHECK(((__bridge id<MTLTexture>)value.getMetalDepthTexture()->nativeHandle()).pixelFormat==
            (desc.stencil?MTLPixelFormatDepth32Float_Stencil8:MTLPixelFormatDepth32Float));
    }
    else CHECK(value.hasDepthAttachment()==value.sharesDepth());
}

std::string label(const ALMetalTexture* texture)
{
    REQUIRE(texture);
    NSString* value = ((__bridge id<MTLTexture>)texture->nativeHandle()).label;
    return value.UTF8String ? value.UTF8String : "";
}
struct NativeTargets
{
    ALRenderCapabilities previous_capabilities = gRenderCapabilities;
    ALMetalDevice* previous_device = LLImageGL::getMetalDevice();
    bool previous_authority = LLImageGL::isMetalTextureAuthority();
    bool previous_reverse = LLRender::sReverseZ;
    LLColor4 previous_clear = gGL.getClearColor();
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalShaderLibrary> library;
    LLRenderTarget owner, sharer, other, target;
    std::string error;
    NativeTargets()
    {
        device=ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        gRenderCapabilities.initializeMetal(device->info());
        LLImageGL::setMetalDevice(device.get()); LLImageGL::setMetalTextureAuthority(true);
    }
    ~NativeTargets()
    {
        while (LLRenderTarget::getCurrentBoundTarget()) LLRenderTarget::getCurrentBoundTarget()->flush();
        sharer.release(); other.release(); target.release(); owner.release();
        LLRender::sReverseZ=previous_reverse;
        gGL.setClearColor(previous_clear);
        LLImageGL::setMetalDevice(previous_device); LLImageGL::setMetalTextureAuthority(previous_authority);
        gRenderCapabilities=previous_capabilities;
    }
    std::vector<U16> halfWords(const LLRenderTarget& value)
    {
        std::vector<U16> words(value.getWidth()*value.getHeight()*4);
        REQUIRE(value.getMetalColorTexture()->readback(*device,0,0,{},words.data(),value.getWidth()*8,&error));
        return words;
    }
    std::vector<U8> rgba(const LLRenderTarget& value,U32 attachment=0)
    {
        std::vector<U8> result(value.getWidth()*value.getHeight()*4);
        REQUIRE(value.getMetalColorTexture(attachment)->readback(*device,0,0,{},result.data(),value.getWidth()*4,&error));
        return result;
    }
    F32 depth(const LLRenderTarget& value)
    {
        std::vector<F32> values(value.getWidth()*value.getHeight());
        REQUIRE(value.getMetalDepthTexture()->readback(*device,0,0,{},values.data(),value.getWidth()*sizeof(F32),&error));
        return values[values.size()/2];
    }
    template<class T> std::vector<T> integers(const LLRenderTarget& value,U32 attachment)
    {
        const U32 count=value.getWidth()*value.getHeight();
        std::vector<T> result(count*4,0);
        for (U32 i=0;i<count;++i) result[i*4+3]=1;
        const auto* texture=value.getMetalColorTexture(attachment); REQUIRE(texture);
        if (texture->descriptor().format==ALMetalPixelFormat::rg16_sint)
        {
            std::vector<S16> packed(count*2);
            REQUIRE(texture->readback(*device,0,0,{},packed.data(),value.getWidth()*4,&error));
            for (U32 i=0;i<count;++i) {result[i*4]=packed[i*2];result[i*4+1]=packed[i*2+1];}
        }
        else
        {
            const U32 components=texture->descriptor().format==ALMetalPixelFormat::r32_uint?1:4;
            std::vector<U32> packed(count*components);
            REQUIRE(texture->readback(*device,0,0,{},packed.data(),value.getWidth()*components*4,&error));
            for (U32 i=0;i<count;++i) for(U32 c=0;c<components;++c) result[i*4+c]=packed[i*components+c];
        }
        return result;
    }
    template<class T> void every(const std::vector<T>& values,const std::array<T,4>& expected)
    { for(std::size_t i=0;i<values.size();++i) REQUIRE(values[i]==expected[i%4]); }
    void draw(LLRenderTarget& value,bool integer)
    {
        if (!library)
        {
            library=ALMetalShaderLibrary::compileSource(*device,R"(
#include <metal_stdlib>
using namespace metal;
vertex float4 target_vertex(uint i [[vertex_id]]) {
    return float4(float2((i<<1)&2,i&2)*2.0-1.0,0.0,1.0);
}
struct Integer { uint4 a [[color(0)]]; int4 b [[color(1)]]; uint4 c [[color(2)]]; };
fragment Integer target_integer(float4 pos [[position]]) {
    uint2 p=uint2(pos.xy);
    return {uint4(0x01000001u+p.y*1000u+p.x,0,0,0),
        int4(-1-int(p.x),int(p.y)*300-1000,0,0),uint4(p.x,p.y,0xFFFFFFFFu-p.x,0x80000000u+p.y)};
}
struct Colors { float4 a [[color(0)]]; float4 b [[color(1)]]; float4 c [[color(2)]]; float4 d [[color(3)]]; };
fragment Colors target_colors() { return {float4(1,0,0,1),float4(0,1,0,1),float4(0,0,1,1),float4(1,1,0,1)}; }
)","Original target integer and MRT output oracles",&error); INFO(error); REQUIRE(library);
        }
        ALMetalRenderPipelineDescriptor pd;pd.library=library.get();pd.vertex_function="target_vertex";
        pd.fragment_function=integer?"target_integer":"target_colors";pd.color_attachment_count=value.getNumTextures();
        for(U32 i=0;i<value.getNumTextures();++i) pd.colors[i].format=value.getMetalColorTexture(i)->descriptor().format;
        auto pipeline=ALMetalRenderPipeline::create(*device,pd,&error);INFO(error);REQUIRE(pipeline);
        auto command=device->makeCommandBuffer("Native target draw",&error);REQUIRE(command);
        ALMetalRenderPassConfig pc;pc.bind_depth_stencil=false;
        for(auto& color:pc.colors)color.load=ALMetalLoadAction::load;
        auto pass=value.getMetalRenderTarget()->makeRenderPass(pc,&error);REQUIRE(pass);
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Native target draw",&error);REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline,&error));
        REQUIRE(encoder->setViewport({0,0,double(value.getWidth()),double(value.getHeight()),0,1},&error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,&error));
        REQUIRE(encoder->end(&error));REQUIRE(command->commitAndWait(&error));
    }
    void requireLabels(const LLRenderTarget& value,const std::string& expected,bool depth)
    {
        REQUIRE(value.getMetalRenderTarget());
        CHECK(value.getMetalRenderTarget()->descriptor().label==expected);
        for (U32 i=0;i<value.getNumTextures();++i)
            CHECK(label(value.getMetalColorTexture(i))==(expected.empty()?"":expected+".color"+std::to_string(i)));
        if (depth) CHECK(label(value.getMetalDepthTexture())==(expected.empty()?"":expected+".depth"));
    }
};
}

// Original1eb44 ensure(desc,depth_from) attaches a new depth build while the
// sharer's existing color storage and pixels survive unchanged.
TEST_CASE_METHOD(NativeTargets,"Native render target depth rebase preserves owned color identity and contents",
                 "[almetal][native-render-target][target-rebase]")
{
    const U32 baseline=LLRenderTarget::sBytesAllocated;
    const ALRenderTargetDesc color{.width=6,.height=5,.color={GL_RGBA16F}};
    REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);
    REQUIRE(sharer.ensure(color,owner)==EnsureResult::Built);
    REQUIRE(sharer.clearAttachments(GL_COLOR_BUFFER_BIT,LLColor4(1,0,1,1),0,0));
    const auto pixels=halfWords(sharer);
    every<U16>(pixels,{0x3c00,0,0x3c00,0x3c00});
    const auto* color_identity=sharer.getMetalColorTexture();
    const auto native_identity=color_identity->nativeHandle();
    const auto old_serial=owner.getDepthSerial();
    owner.release(); REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);
    CHECK(owner.getDepthSerial()!=old_serial);
    REQUIRE(sharer.ensure(color,owner)==EnsureResult::Built);
    CHECK(sharer.getMetalColorTexture()==color_identity);
    CHECK(sharer.getMetalColorTexture()->nativeHandle()==native_identity);
    CHECK(halfWords(sharer)==pixels);
    CHECK(LLRenderTarget::sBytesAllocated-baseline==6u*5u*(4u+8u+8u+8u));
    CHECK(sharer.ensure(color,owner)==EnsureResult::Kept);
    const ALRenderTargetDesc builds[]={
        {.width=6,.height=5,.color={GL_RGBA8,GL_RGBA16F},.depth=Depth::D32F,.stencil=true},
        {.width=6,.height=5,.color={GL_RGBA8,GL_RGBA16F},.depth=Depth::D32F},
        {.width=6,.height=5,.color={GL_RGBA8,GL_RGBA16F},.depth=Depth::D24,.stencil=true},
        {.width=6,.height=5,.color={GL_RGBA8,GL_RGB10_A2},.depth=Depth::D24,.stencil=true},
        {.width=6,.height=5,.color={GL_RGBA8,GL_RGB10_A2},.depth=Depth::D24,.stencil=true}};
    F64 stored_depth=.25;
    for(const auto& build:builds)
    {
        if(owner.desc()==build)owner.release();
        REQUIRE(owner.ensure(build)==EnsureResult::Built);
        REQUIRE(sharer.ensure(color,owner)==EnsureResult::Built);
        CHECK(sharer.getMetalColorTexture()==color_identity);
        CHECK(sharer.getMetalColorTexture()->nativeHandle()==native_identity);
        CHECK(halfWords(sharer)==pixels);CHECK(sharer.desc()==color);
        CHECK(sharer.getMetalDepthTexture()->nativeHandle()==owner.getMetalDepthTexture()->nativeHandle());
        stored_depth+=.125;
        REQUIRE(owner.clearAttachments(GL_DEPTH_BUFFER_BIT,LLColor4::transparent,stored_depth,0));
        CHECK(std::abs(depth(sharer)-F32(stored_depth))<1.e-4f);
        CHECK(sharer.ensure(color,owner)==EnsureResult::Kept);
    }
    const ALRenderTargetDesc resized_owner{.width=9,.height=7,.color={GL_RGBA8},.depth=Depth::D24,.stencil=true};
    const ALRenderTargetDesc resized_sharer{.width=9,.height=7,.color={GL_RGBA16F}};
    REQUIRE(owner.ensure(resized_owner)==EnsureResult::Built);
    REQUIRE(sharer.ensure(resized_sharer,owner)==EnsureResult::Built);requireStorage(sharer,resized_sharer);
    REQUIRE(owner.clearAttachments(GL_DEPTH_BUFFER_BIT,LLColor4::transparent,.875,0));
    CHECK(std::abs(depth(sharer)-.875f)<1.e-4f);
    CHECK(LLRenderTarget::sBytesAllocated-baseline==bytesOf(resized_owner)+bytesOf(resized_sharer));
}

// Original labels name the owner across rename, rebuild and swap; a sharer
// must never relabel the depth resource owned by another target.
TEST_CASE_METHOD(NativeTargets,"Native render target labels follow rename clear and swapped owned resources",
                 "[almetal][native-render-target][target-label]")
{
    owner.setLabel("native.owner"); REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);
    owner.setLabel("native.renamed"); requireLabels(owner,"native.renamed",true);
    const ALRenderTargetDesc color{.width=6,.height=5,.color={GL_RGBA16F}};
    sharer.setLabel("native.sharer"); REQUIRE(sharer.ensure(color,owner)==EnsureResult::Built);
    sharer.setLabel("native.sharer.renamed"); requireLabels(sharer,"native.sharer.renamed",false);
    CHECK(label(owner.getMetalDepthTexture())=="native.renamed.depth");
    const ALRenderTargetDesc exposure{.width=2,.height=1,.color={GL_RGBA16F}};
    target.setLabel("native.current"); other.setLabel("native.last");
    REQUIRE(target.ensure(exposure)==EnsureResult::Built); REQUIRE(other.ensure(exposure)==EnsureResult::Built);
    const auto native_identity=target.getMetalColorTexture()->nativeHandle();
    target.swapFBORefs(other);
    CHECK(other.getMetalColorTexture()->nativeHandle()==native_identity);
    requireLabels(target,"native.current",false); requireLabels(other,"native.last",false);
    target.setLabel(""); requireLabels(target,"",false);
}

TEST_CASE_METHOD(NativeTargets,"Adding one native color preserves existing color and owned depth",
                 "[almetal][native-render-target][attachment-growth]")
{
    const ALRenderTargetDesc initial{.width=6,.height=5,.color={GL_RGBA16F},.depth=Depth::D32F};
    REQUIRE(target.ensure(initial)==EnsureResult::Built);
    REQUIRE(target.clearAttachments(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT,LLColor4(1,0,1,1),.25,0));
    const auto pixels=halfWords(target);
    every<U16>(pixels,{0x3c00,0,0x3c00,0x3c00});
    const auto color=target.getMetalColorTexture()->nativeHandle();
    const auto depth=target.getMetalDepthTexture()->nativeHandle();
    const auto serial=target.getDepthSerial();
    REQUIRE(target.addColorAttachment(GL_RGBA8));
    CHECK(target.getMetalColorTexture()->nativeHandle()==color);
    CHECK(halfWords(target)==pixels);
    CHECK(target.getMetalDepthTexture()->nativeHandle()==depth);
    CHECK(target.getDepthSerial()==serial);
    std::vector<F32> stored(6*5);
    REQUIRE(target.getMetalDepthTexture()->readback(*device,0,0,{},stored.data(),6*sizeof(F32),&error));
    CHECK(std::all_of(stored.begin(),stored.end(),[](F32 value){return value==.25f;}));
}

TEST_CASE_METHOD(NativeTargets,"Allocating native depth preserves an existing color attachment",
                 "[almetal][native-render-target][attachment-growth]")
{
    REQUIRE(target.ensure({.width=6,.height=5,.color={GL_RGBA16F}})==EnsureResult::Built);
    REQUIRE(target.clearAttachments(GL_COLOR_BUFFER_BIT,LLColor4(1,0,1,1),0,0));
    const auto pixels=halfWords(target);
    every<U16>(pixels,{0x3c00,0,0x3c00,0x3c00});
    const auto color=target.getMetalColorTexture()->nativeHandle();
    REQUIRE(target.allocateDepth());
    CHECK(target.getMetalColorTexture()->nativeHandle()==color);
    CHECK(halfWords(target)==pixels);
    CHECK(target.ownsDepthAttachment());
}

TEST_CASE_METHOD(NativeTargets,"Native target ensure keeps matching storage contents and device-clamped extent",
                 "[almetal][native-render-target][target-ensure]")
{
    const U32 baseline=LLRenderTarget::sBytesAllocated;
    REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);requireStorage(owner,GBUFFER);
    const auto* identity=owner.getMetalRenderTarget();
    const auto color=owner.getMetalColorTexture()->nativeHandle();
    REQUIRE(owner.clearAttachments(GL_COLOR_BUFFER_BIT,LLColor4(1,0,1,1),0,0));
    const auto contents=rgba(owner);
    CHECK(owner.ensure(GBUFFER)==EnsureResult::Kept);
    CHECK(owner.getMetalRenderTarget()==identity);CHECK(owner.getMetalColorTexture()->nativeHandle()==color);
    CHECK(rgba(owner)==contents);CHECK(LLRenderTarget::sBytesAllocated-baseline==bytesOf(GBUFFER));
    const U32 maximum=device->info().max_texture_2d_dimension;
    const ALRenderTargetDesc too_wide{.width=maximum+3,.height=1,.color={GL_R8}};
    REQUIRE(other.ensure(too_wide)==EnsureResult::Built);CHECK(other.getWidth()==maximum);
    CHECK(other.ensure(too_wide)==EnsureResult::Kept);
    REQUIRE(target.allocate(4,4,GL_RGBA8,true,true,ALTextureSlot::TT_TEXTURE,LLRenderTarget::MIPS_AUTO,LLRenderTarget::DEPTH_FMT_32F));
    REQUIRE(target.addColorAttachment(GL_RGBA16F));
    const ALRenderTargetDesc allocated{.width=4,.height=4,.color={GL_RGBA8,GL_RGBA16F},.depth=Depth::D32F,.stencil=true,.mips=Mips::Auto};
    CHECK(target.desc()==allocated);CHECK(target.ensure(allocated)==EnsureResult::Kept);
}

TEST_CASE_METHOD(NativeTargets,"Native target ensure rebuilds each changed storage field with exact physical accounting",
                 "[almetal][native-render-target][target-ensure]")
{
    const U32 baseline=LLRenderTarget::sBytesAllocated;
    REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);
        const NamedDesc changes[] = {
            { "width", { .width = 9, .height = 5, .color = { GL_RGBA8, GL_RGBA16F }, .depth = Depth::D24, .stencil = true } },
            { "height", { .width = 9, .height = 7, .color = { GL_RGBA8, GL_RGBA16F }, .depth = Depth::D24, .stencil = true } },
            { "a colour format", { .width = 9, .height = 7, .color = { GL_RGBA8, GL_RGB10_A2 }, .depth = Depth::D24, .stencil = true } },
            { "a colour attachment more", { .width = 9, .height = 7, .color = { GL_RGBA8, GL_RGB10_A2, GL_R32F }, .depth = Depth::D24, .stencil = true } },
            { "a colour attachment fewer", { .width = 9, .height = 7, .color = { GL_RGBA8 }, .depth = Depth::D24, .stencil = true } },
            { "the depth format", { .width = 9, .height = 7, .color = { GL_RGBA8 }, .depth = Depth::D32F, .stencil = true } },
            { "stencil", { .width = 9, .height = 7, .color = { GL_RGBA8 }, .depth = Depth::D32F } },
            { "mips", { .width = 9, .height = 7, .color = { GL_RGBA8 }, .depth = Depth::D32F, .mips = Mips::Manual } },
            { "no depth", { .width = 9, .height = 7, .color = { GL_RGBA8 }, .mips = Mips::Manual } },
            { "no colour", { .width = 9, .height = 7, .depth = Depth::D24 } },
        };
    for(const auto& [what,desc]:changes)
    {
        CAPTURE(what);REQUIRE(owner.ensure(desc)==EnsureResult::Built);
        CHECK(owner.desc()==desc);requireStorage(owner,desc);
        CHECK(LLRenderTarget::sBytesAllocated-baseline==bytesOf(desc));
        CHECK(owner.ensure(desc)==EnsureResult::Kept);
    }
}

TEST_CASE_METHOD(NativeTargets,"Native target failed ensure releases all storage and restores allocation accounting",
                 "[almetal][native-render-target][target-failure]")
{
    const U32 baseline=LLRenderTarget::sBytesAllocated;
    REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);
    {
        TextureFailure failure(*device);
        const auto native_bytes=alMetalResourceAllocationStats().texture_bytes_live;
        CHECK(owner.ensure({.width=9,.height=7,.color={GL_RGBA8},.depth=Depth::D32F})==EnsureResult::Failed);
        CHECK(failure.calls==1);requireReleased(owner);
        CHECK(LLRenderTarget::sBytesAllocated==baseline);
        CHECK(alMetalResourceAllocationStats().texture_bytes_live<native_bytes);
    }
    const ALRenderTargetDesc refused[]={
        {.color={GL_RGBA8}}, {.width=4,.height=4},
        {.width=4,.height=4,.color={GL_RGBA8,0,GL_RGBA8}},
        {.width=4,.height=4,.color={GL_RGBA8},.stencil=true},
        {.width=4,.height=4,.color={GL_R32UI},.mips=Mips::Auto},
        {.width=4,.height=4,.depth=Depth::D24,.mips=Mips::Auto}};
    for(const auto& desc:refused)
    {
        REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);
        CHECK(owner.ensure(desc)==EnsureResult::Failed);requireReleased(owner);
        CHECK(LLRenderTarget::sBytesAllocated==baseline);
    }
    REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);requireStorage(owner,GBUFFER);
}

TEST_CASE_METHOD(NativeTargets,"Native depth sharing retains an old allocation through owner destruction then rebases despite zero legacy names",
                 "[almetal][native-render-target][target-depth-lease]")
{
    const ALRenderTargetDesc depth_owner{.width=8,.height=8,.color={GL_RGBA8},.depth=Depth::D32F};
    const ALRenderTargetDesc color{.width=8,.height=8,.color={GL_RGBA8}};
    void* old_identity=nullptr;U64 old_serial=0;
    {
        LLRenderTarget previous;
        REQUIRE(previous.ensure(depth_owner)==EnsureResult::Built);
        REQUIRE(sharer.ensure(color,previous)==EnsureResult::Built);
        REQUIRE(previous.clearAttachments(GL_DEPTH_BUFFER_BIT,LLColor4::transparent,.25,0));
        old_identity=previous.getMetalDepthTexture()->nativeHandle();old_serial=previous.getDepthSerial();
        CHECK(previous.getDepth()==0);CHECK(depth(sharer)==.25f);
    }
    CHECK(sharer.getMetalDepthTexture()->nativeHandle()==old_identity);
    CHECK(depth(sharer)==.25f);
    REQUIRE(owner.ensure(depth_owner)==EnsureResult::Built);
    CHECK(owner.getDepth()==0);CHECK(owner.getDepthSerial()!=old_serial);
    REQUIRE(sharer.ensure(color,owner)==EnsureResult::Built);
    CHECK(sharer.getMetalDepthTexture()->nativeHandle()==owner.getMetalDepthTexture()->nativeHandle());
    REQUIRE(owner.clearAttachments(GL_DEPTH_BUFFER_BIT,LLColor4::transparent,.75,0));
    CHECK(depth(sharer)==.75f);CHECK(sharer.ensure(color,owner)==EnsureResult::Kept);
}

TEST_CASE_METHOD(NativeTargets,"Native ensure recognizes manual sharing and detaches it when own color alone is requested",
                 "[almetal][native-render-target]")
{
    const ALRenderTargetDesc color{.width=6,.height=5,.color={GL_RGBA16F}};
    REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);
    REQUIRE(sharer.allocate(6,5,GL_RGBA16F));owner.shareDepthBuffer(sharer);
    CHECK(sharer.sharesDepth());CHECK(sharer.ensure(color,owner)==EnsureResult::Kept);
    CHECK(sharer.ensure(color)==EnsureResult::Built);CHECK_FALSE(sharer.sharesDepth());
    CHECK_FALSE(sharer.hasDepthAttachment());CHECK(sharer.ensure(color)==EnsureResult::Kept);
}

TEST_CASE_METHOD(NativeTargets,"Native unshareable descriptions fail released while depth-only sharers remain complete",
                 "[almetal][native-render-target]")
{
    const U32 baseline=LLRenderTarget::sBytesAllocated;
    const ALRenderTargetDesc color{.width=6,.height=5,.color={GL_RGBA16F}};
    REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);
    const ALRenderTargetDesc refused[]={
        {.width=7,.height=5,.color={GL_RGBA16F}},
        {.width=6,.height=5,.color={GL_RGBA16F},.depth=Depth::D24},
        {.width=6,.height=5,.color={GL_RGBA16F},.stencil=true},
        {.color={GL_RGBA16F}}, {.width=6,.height=5,.mips=Mips::Auto}};
    for(const auto& desc:refused)
    {
        REQUIRE(sharer.ensure(color,owner)==EnsureResult::Built);
        CHECK(sharer.ensure(desc,owner)==EnsureResult::Failed);requireReleased(sharer);
        CHECK(LLRenderTarget::sBytesAllocated-baseline==bytesOf(GBUFFER));
    }
    REQUIRE(sharer.ensure(color,owner)==EnsureResult::Built);
    CHECK(sharer.ensure(color,sharer)==EnsureResult::Failed);requireReleased(sharer);
    REQUIRE(sharer.ensure(color,owner)==EnsureResult::Built);
    REQUIRE(owner.ensure({.width=6,.height=5,.color={GL_RGBA8}})==EnsureResult::Built);
    CHECK(sharer.ensure(color,owner)==EnsureResult::Failed);requireReleased(sharer);
    REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);
    const ALRenderTargetDesc depth_only{.width=6,.height=5};
    REQUIRE(sharer.ensure(depth_only,owner)==EnsureResult::Built);
    CHECK(sharer.isComplete());CHECK(sharer.ensure(depth_only,owner)==EnsureResult::Kept);
    CHECK(LLRenderTarget::sBytesAllocated-baseline==bytesOf(GBUFFER));
}

TEST_CASE_METHOD(NativeTargets,"Native target swaps carry shared depth and ensure restores each producer association",
                 "[almetal][native-render-target]")
{
    const ALRenderTargetDesc color{.width=6,.height=5,.color={GL_RGBA16F}};
    REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);REQUIRE(other.ensure(GBUFFER)==EnsureResult::Built);
    REQUIRE(sharer.ensure(color,owner)==EnsureResult::Built);REQUIRE(target.ensure(color,other)==EnsureResult::Built);
    sharer.swapFBORefs(target);
    CHECK(sharer.getMetalDepthTexture()->nativeHandle()==other.getMetalDepthTexture()->nativeHandle());
    CHECK(target.getMetalDepthTexture()->nativeHandle()==owner.getMetalDepthTexture()->nativeHandle());
    CHECK(sharer.ensure(color,owner)==EnsureResult::Built);CHECK(target.ensure(color,other)==EnsureResult::Built);
    CHECK(sharer.getMetalDepthTexture()->nativeHandle()==owner.getMetalDepthTexture()->nativeHandle());
    CHECK(target.getMetalDepthTexture()->nativeHandle()==other.getMetalDepthTexture()->nativeHandle());
    CHECK(sharer.ensure(color,owner)==EnsureResult::Kept);CHECK(target.ensure(color,other)==EnsureResult::Kept);
}

TEST_CASE_METHOD(NativeTargets,"Native integer attachments preserve clear rounding saturation and every drawn integer bit",
                 "[almetal][native-render-target][target-integer]")
{
    REQUIRE(target.allocate(6,5,GL_R32UI,true));
    REQUIRE(target.addColorAttachment(GL_RG16I));REQUIRE(target.addColorAttachment(GL_RGBA32UI));
    const std::array<U32,3> formats{GL_R32UI,GL_RG16I,GL_RGBA32UI};
    const std::array<ALMetalPixelFormat,3> physical{ALMetalPixelFormat::r32_uint,ALMetalPixelFormat::rg16_sint,ALMetalPixelFormat::rgba32_uint};
    for(U32 i=0;i<3;++i) {CHECK(target.getInternalFormat(i)==formats[i]);CHECK(target.getMetalColorTexture(i)->descriptor().format==physical[i]);}
    REQUIRE(target.clearAttachments(GL_COLOR_BUFFER_BIT,LLColor4(7,9,11,13),0,0));
    every<U32>(integers<U32>(target,0),{7,0,0,1});
    every<S32>(integers<S32>(target,1),{7,9,0,1});
    every<U32>(integers<U32>(target,2),{7,9,11,13});
    REQUIRE(target.clearAttachments(GL_DEPTH_BUFFER_BIT,LLColor4(-5.4f,-6.6f,1.e10f,.4f),0,0));
    every<U32>(integers<U32>(target,0),{7,0,0,1});
    REQUIRE(target.clearAttachments(GL_COLOR_BUFFER_BIT,LLColor4(-5.4f,-6.6f,1.e10f,.4f),0,0));
    every<U32>(integers<U32>(target,0),{0,0,0,1});
    every<S32>(integers<S32>(target,1),{-5,-7,0,1});
    every<U32>(integers<U32>(target,2),{0,0,0xFFFFFFFFu,0});
    draw(target,true);
    const auto uints=integers<U32>(target,0),wide=integers<U32>(target,2);
    const auto ints=integers<S32>(target,1);
    for(U32 y=0;y<5;++y)for(U32 x=0;x<6;++x)
    {
        const U32 i=(y*6+x)*4;CAPTURE(x,y);
        REQUIRE(uints[i]==0x01000001u+y*1000u+x);
        REQUIRE(ints[i]==-1-S32(x));REQUIRE(ints[i+1]==S32(y)*300-1000);
        REQUIRE(wide[i]==x);REQUIRE(wide[i+1]==y);
        REQUIRE(wide[i+2]==0xFFFFFFFFu-x);REQUIRE(wide[i+3]==0x80000000u+y);
    }
}

TEST_CASE_METHOD(NativeTargets,"Native mixed float integer targets honor narrow clear selection",
                 "[almetal][native-render-target]")
{
    REQUIRE(target.allocate(6,5,GL_RGBA8));REQUIRE(target.addColorAttachment(GL_R32UI));
    REQUIRE(target.clearAttachments(GL_COLOR_BUFFER_BIT,LLColor4(1,0,1,1),0,0));
    every<U8>(rgba(target),{255,0,255,255});every<U32>(integers<U32>(target,1),{1,0,0,1});
    const LLColor4 previous=gGL.getClearColor();
    target.bindTarget();target.setDrawBuffers(1);gGL.setClearColor(LLColor4::transparent);target.clear();
    every<U8>(rgba(target),{0,0,0,0});every<U32>(integers<U32>(target,1),{1,0,0,1});
    target.flush();gGL.setClearColor(previous);
}

TEST_CASE_METHOD(NativeTargets,"Native borrowed image attachment retains producer format pixels accounting and label",
                 "[almetal][native-render-target]")
{
    const U32 baseline=LLRenderTarget::sBytesAllocated;
    LLPointer<LLImageGL> image=new LLImageGL(6,5,1,false);
    image->setExplicitFormat(GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT);
    const std::array<U32,6*5> zeros{};
    REQUIRE(image->createMetalTexture(0,reinterpret_cast<const U8*>(zeros.data())));
    auto rendered=image->createMetalRenderTexture(&error);INFO(error);REQUIRE(rendered);
    rendered->setLabel("native.image.owner");
    REQUIRE(image->adoptMetalRenderTextureAuthoritative(std::move(rendered),LLGLTexture::LOCAL));
    target.setLabel("native.lent");target.setColorAttachment(image);
    REQUIRE(target.isComplete());CHECK(target.getInternalFormat()==GL_R32UI);
    CHECK(target.getMetalColorTexture()->nativeHandle()==image->getMetalTexture()->nativeHandle());
    CHECK(LLRenderTarget::sBytesAllocated==baseline);
    REQUIRE(target.clearAttachments(GL_COLOR_BUFFER_BIT,LLColor4(5,6,7,8),0,0));
    every<U32>(integers<U32>(target,0),{5,0,0,1});
    target.setLabel("native.lent.renamed");
    CHECK(target.getMetalRenderTarget()->descriptor().label=="native.lent.renamed");
    CHECK(label(image->getMetalTexture())=="native.image.owner");
    target.releaseColorAttachment();CHECK(target.getInternalFormat()==0);requireReleased(target);
    CHECK(label(image->getMetalTexture())=="native.image.owner");
}

TEST_CASE_METHOD(NativeTargets,"Native bound-target copy resamples authored pixels outside ambient scissor",
                 "[almetal][native-render-target]")
{
    REQUIRE(owner.allocate(4,4,GL_RGBA8));REQUIRE(target.allocate(2,2,GL_RGBA8));
    std::array<U8,4*4*4> source{};
    for(U32 y=0;y<4;++y)for(U32 x=0;x<4;++x)
    {const U32 i=(y*4+x)*4;source[i]=source[i+1]=source[i+2]=x==0?255:0;source[i+3]=255;}
    auto command=device->makeCommandBuffer("Original white-column copy oracle",&error);REQUIRE(command);
    REQUIRE(const_cast<ALMetalTexture*>(owner.getMetalColorTexture())->upload(*command,0,0,{},source.data(),16,&error));
    REQUIRE(command->commitAndWait(&error));
    owner.bindTarget();
    {
        LLGLSScissor corner(0,0,1,1);
        target.copyFromBound(0,0,4,4,0,0,2,2,GL_LINEAR);
    }
    CHECK(LLRenderTarget::getCurrentBoundTarget()==&owner);owner.flush();
    const auto result=rgba(target);
    CHECK(result[0]>=127);CHECK(result[0]<=128);CHECK(result[4]==0);
    CHECK(result[(2*2-1)*4+2]==0);
}

TEST_CASE_METHOD(NativeTargets,"Native MRT output slots preserve the original four fragment color oracles",
                 "[almetal][native-render-target]")
{
    // The removed no-caller GL output remap is replaced by the native
    // producer's explicit output layout, as all real Metal passes select.
    const ALRenderTargetDesc mrt{.width=4,.height=4,.color={GL_RGBA8,GL_RGBA8,GL_RGBA8,GL_RGBA8}};
    REQUIRE(target.ensure(mrt)==EnsureResult::Built);draw(target,false);
    const std::array<std::array<U8,4>,4> expected{{{255,0,0,255},{0,255,0,255},{0,0,255,255},{255,255,0,255}}};
    for(U32 i=0;i<4;++i)every<U8>(rgba(target,i),expected[i]);
}

TEST_CASE_METHOD(NativeTargets,"Native labels persist through storage rebuild and remain available without GL debug state",
                 "[almetal][native-render-target]")
{
    const bool previous=gDebugGL;
    struct Restore {bool value;~Restore(){gDebugGL=value;}} restore{previous};
    gDebugGL=false;
    owner.setLabel("native.gbuffer");REQUIRE(owner.ensure(GBUFFER)==EnsureResult::Built);
    requireLabels(owner,"native.gbuffer",true);
    REQUIRE(owner.ensure({.width=9,.height=7,.color={GL_RGBA8,GL_RGBA16F,GL_RGB10_A2},.depth=Depth::D32F,.stencil=true})==EnsureResult::Built);
    requireLabels(owner,"native.gbuffer",true);
    owner.setLabel("native.renamed");requireLabels(owner,"native.renamed",true);
    CHECK(owner.getLabel()=="native.renamed");owner.setLabel("");requireLabels(owner,"",true);
}

TEST_CASE_METHOD(NativeTargets,"Native target attachment growth failure preserves old target and retries without lost bytes",
                 "[almetal][native-render-target][target-failure]")
{
    REQUIRE(target.ensure({.width=6,.height=5,.color={GL_RGBA16F},.depth=Depth::D32F})==EnsureResult::Built);
    REQUIRE(target.clearAttachments(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT,LLColor4(1,0,1,1),.25,0));
    const auto* identity=target.getMetalRenderTarget();const auto* color=target.getMetalColorTexture();
    const auto depth_identity=target.getMetalDepthTexture()->nativeHandle();const U64 serial=target.getDepthSerial();
    const auto pixels=halfWords(target);const U32 bytes=LLRenderTarget::sBytesAllocated;
    const auto native_bytes=alMetalResourceAllocationStats().texture_bytes_live;
    {
        TextureFailure failure(*device);CHECK_FALSE(target.addColorAttachment(GL_RGBA8));CHECK(failure.calls==1);
        CHECK(target.getMetalRenderTarget()==identity);CHECK(target.getMetalColorTexture()==color);
        CHECK(target.getMetalDepthTexture()->nativeHandle()==depth_identity);CHECK(target.getDepthSerial()==serial);
        CHECK(halfWords(target)==pixels);CHECK(depth(target)==.25f);
        CHECK(LLRenderTarget::sBytesAllocated==bytes);CHECK(alMetalResourceAllocationStats().texture_bytes_live==native_bytes);
    }
    REQUIRE(target.addColorAttachment(GL_RGBA8));CHECK(target.getMetalColorTexture()==color);
    CHECK(target.getMetalDepthTexture()->nativeHandle()==depth_identity);CHECK(target.getDepthSerial()==serial);
    CHECK(LLRenderTarget::sBytesAllocated==bytes+6u*5u*4u);
    REQUIRE(other.ensure({.width=6,.height=5,.color={GL_RGBA16F}})==EnsureResult::Built);
    REQUIRE(other.clearAttachments(GL_COLOR_BUFFER_BIT,LLColor4(1,0,1,1),0,0));
    const auto other_pixels=halfWords(other);const auto* other_identity=other.getMetalRenderTarget();
    const U32 before_depth=LLRenderTarget::sBytesAllocated;
    {
        TextureFailure failure(*device);CHECK_FALSE(other.allocateDepth());CHECK(failure.calls==1);
        CHECK(other.getMetalRenderTarget()==other_identity);CHECK_FALSE(other.hasDepthAttachment());
        CHECK(halfWords(other)==other_pixels);CHECK(LLRenderTarget::sBytesAllocated==before_depth);
    }
    REQUIRE(other.allocateDepth());CHECK(halfWords(other)==other_pixels);
    CHECK(LLRenderTarget::sBytesAllocated==before_depth+6u*5u*4u);
}

TEST_CASE_METHOD(NativeTargets,"Native target writable-format policy reaches actual Metal usage flags",
                 "[almetal][native-render-target]")
{
    const std::array<U32,7> accepted{GL_R8,GL_RG8,GL_RGBA8,GL_R32UI,GL_RG16I,GL_RGBA16F,GL_RGBA32UI};
    for(U32 format:accepted)
    {
        CAPTURE(format);REQUIRE(target.allocate(2,2,format));
        CHECK(alMetalHasUsage(target.getMetalColorTexture()->descriptor().usage,ALMetalTextureUsage::shader_write));
        CHECK(((__bridge id<MTLTexture>)target.getMetalColorTexture()->nativeHandle()).usage&MTLTextureUsageShaderWrite);
    }
    for(U32 format:{U32(GL_RGB8),U32(GL_SRGB8_ALPHA8)})
    {
        CAPTURE(format);REQUIRE(target.allocate(2,2,format));
        CHECK_FALSE(alMetalHasUsage(target.getMetalColorTexture()->descriptor().usage,ALMetalTextureUsage::shader_write));
        CHECK_FALSE(((__bridge id<MTLTexture>)target.getMetalColorTexture()->nativeHandle()).usage&MTLTextureUsageShaderWrite);
    }
}

TEST_CASE_METHOD(NativeTargets,"Native attachment transaction rejects incompatible or externally managed graphs without consuming them",
                 "[almetal][native-render-target][target-failure]")
{
    ALMetalRenderTargetDescriptor desc;desc.width=desc.height=2;
    desc.colors[0].format=ALMetalPixelFormat::rgba8_unorm;
    auto previous=ALMetalRenderTarget::create(*device,desc,&error);REQUIRE(previous);
    const auto* color=previous->colorTexture(0);const auto identity=color->nativeHandle();
    auto preserving=desc;preserving.colors[0].texture=color;
    const auto native_bytes=alMetalResourceAllocationStats().texture_bytes_live;
    for(U32 alteration=0;alteration<5;++alteration)
    {
        auto invalid=preserving;
        switch(alteration)
        {
            case 0:invalid.width=3;break;
            case 1:invalid.mipmap_level_count=2;break;
            case 2:invalid.colors[0].format=ALMetalPixelFormat::rgba16_float;break;
            case 3:invalid.sample_count=2;break;
            case 4:invalid.colors[0].shader_write=true;break;
        }
        CHECK_FALSE(ALMetalRenderTarget::rebuildPreservingAttachments(*device,invalid,*previous,&error));
        CHECK(previous->colorTexture(0)==color);CHECK(previous->colorTexture(0)->nativeHandle()==identity);
        CHECK(alMetalResourceAllocationStats().texture_bytes_live==native_bytes);
    }
    auto pool=ALMetalRenderTargetPool::create(*device,4096);REQUIRE(pool);
    auto pooled=pool->acquire(desc,&error);REQUIRE(pooled);
    auto pool_desc=desc;pool_desc.colors[0].texture=pooled->colorTexture(0);
    CHECK_FALSE(ALMetalRenderTarget::rebuildPreservingAttachments(*device,pool_desc,*pooled,&error));
    CHECK(pooled->colorTexture(0));
    ALMetalTextureDescriptor td;td.width=td.height=2;td.format=ALMetalPixelFormat::depth32_float;
    td.usage=ALMetalTextureUsage::render_target|ALMetalTextureUsage::shader_read;
    auto depth=ALMetalTexture::create(*device,td,&error);REQUIRE(depth);
    auto parent=ALMetalRenderTarget::create(*device,desc,&error);REQUIRE(parent);
    auto parent_graph=ALMetalRenderTarget::attachBorrowedDepth(*device,std::move(parent),*depth,&error);REQUIRE(parent_graph);
    auto parent_desc=parent_graph->descriptor();
    CHECK_FALSE(ALMetalRenderTarget::rebuildPreservingAttachments(*device,parent_desc,*parent_graph,&error));
    CHECK(parent_graph->colorTexture(0));CHECK(parent_graph->depthStencilTexture()==depth.get());
}
