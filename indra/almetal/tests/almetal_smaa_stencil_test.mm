/**
 * @file almetal_smaa_stencil_test.mm
 * @brief Original SMAA hardware stencil admission and alternate-image metrics.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetalworld.h"
#include "almetaltextureops.h"
#include <Foundation/Foundation.h>
#include <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace
{
using Bytes = std::vector<std::uint8_t>;
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> probe_pipeline;
    std::unique_ptr<ALMetalDepthStencilState> probe_state;
    std::unique_ptr<ALMetalBuffer> probe_vertices;
    Fixture()
    {
        REQUIRE([[NSBundle mainBundle] pathForResource:@"almetal_builtin" ofType:@"metallib"] != nil);
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error);
        INFO(error); REQUIRE(renderer);
        auto frames = ALMetalWorldFrameData::create(*device, &error); REQUIRE(frames);
        frames->beginFrame(1); renderer->setFrameData(std::move(frames));
        library = ALMetalShaderLibrary::loadBuiltin(*device, &error); REQUIRE(library);
        ALMetalRenderPipelineDescriptor p;
        p.library = library.get(); p.vertex_function = "almetal_test_vertex";
        p.fragment_function = "almetal_test_fragment";
        p.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        p.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        probe_pipeline = ALMetalRenderPipeline::create(*device, p, &error); REQUIRE(probe_pipeline);
        ALMetalDepthStencilDescriptor d;
        d.depth_compare = ALMetalCompareFunction::always; d.depth_write_enabled = false;
        d.stencil_enabled = true; d.front_stencil.compare = ALMetalCompareFunction::equal;
        d.front_stencil.read_mask = 0xff; d.front_stencil.write_mask = 0;
        d.back_stencil = d.front_stencil;
        probe_state = ALMetalDepthStencilState::create(*device, d, &error); REQUIRE(probe_state);
        struct Vertex { float position[2]; float color[4]; };
        const std::array<Vertex, 3> vertices{{
            {{-1.f,-1.f},{1.f,1.f,1.f,1.f}},
            {{ 3.f,-1.f},{1.f,1.f,1.f,1.f}},
            {{-1.f, 3.f},{1.f,1.f,1.f,1.f}},
        }};
        ALMetalBufferDescriptor b; b.length = sizeof(vertices); b.storage = ALMetalStorageMode::shared;
        probe_vertices = ALMetalBuffer::create(*device, b, &error); REQUIRE(probe_vertices);
        REQUIRE(probe_vertices->write(0, vertices.data(), sizeof(vertices), &error));
    }
    ~Fixture() { if (device) device->waitUntilIdle(); }
    std::unique_ptr<ALMetalRenderTarget> source(unsigned w, unsigned h, const Bytes& rgba, bool hdr=false)
    {
        REQUIRE(rgba.size() == w*h*4);
        ALMetalRenderTargetDescriptor d; d.width=w; d.height=h;
        d.colors[0].format = hdr ? ALMetalPixelFormat::rgb10a2_unorm : ALMetalPixelFormat::rgba8_unorm;
        auto t = ALMetalRenderTarget::create(*device,d,&error); REQUIRE(t);
        Bytes packed = rgba;
        if(hdr) for(unsigned i=0;i<w*h;++i)
        {
            std::uint32_t word = 3u<<30;
            for(unsigned c=0;c<3;++c) word |= ((rgba[i*4+c]*1023u+127u)/255u)<<(10*c);
            std::memcpy(packed.data()+i*4,&word,4);
        }
        auto command=device->makeCommandBuffer("SMAA authored display upload",&error); REQUIRE(command);
        REQUIRE(const_cast<ALMetalTexture*>(t->colorTexture(0))->upload(*command,0,0,{},packed.data(),w*4,&error));
        REQUIRE(command->commitAndWait(&error)); return t;
    }
    std::unique_ptr<ALMetalTexture> depth(unsigned w,unsigned h)
    {
        ALMetalTextureDescriptor d; d.width=w; d.height=h; d.format=ALMetalPixelFormat::depth32_float;
        d.storage=ALMetalStorageMode::private_gpu; d.usage=ALMetalTextureUsage::shader_read;
        auto t=ALMetalTexture::create(*device,d,&error); REQUIRE(t);
        std::vector<float> pixels(w*h);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) pixels[y*w+x]=x>=w/2?.8f:.2f;
        auto command=device->makeCommandBuffer("SMAA predication upload",&error); REQUIRE(command);
        REQUIRE(t->upload(*command,0,0,{},pixels.data(),w*sizeof(float),&error));
        REQUIRE(command->commitAndWait(&error)); return t;
    }
    Bytes read(const ALMetalRenderTarget& target)
    {
        const auto& d=target.descriptor();
        REQUIRE((d.colors[0].format==ALMetalPixelFormat::rgba8_unorm || d.colors[0].format==ALMetalPixelFormat::rgb10a2_unorm));
        Bytes pixels(d.width*d.height*4,0xcd);
        REQUIRE(target.colorTexture(0)->readback(*device,0,0,{},pixels.data(),d.width*4,&error)); return pixels;
    }
    std::unique_ptr<ALMetalRenderTarget> encodeProbe(const ALMetalRenderTarget& edges, ALMetalCommandBuffer& command)
    {
        REQUIRE(edges.depthStencilTexture());
        REQUIRE(edges.descriptor().depth_stencil_format==ALMetalPixelFormat::depth32_float_stencil8);
        ALMetalRenderTargetDescriptor d; d.width=edges.descriptor().width; d.height=edges.descriptor().height;
        d.colors[0].format=ALMetalPixelFormat::rgba8_unorm;
        d.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
        d.depth_stencil_texture=edges.depthStencilTexture();
        auto target=ALMetalRenderTarget::create(*device,d,&error); REQUIRE(target);
        ALMetalRenderPassConfig p; p.colors[0].clear={0,0,0,0};
        p.depth_load=ALMetalLoadAction::dont_care;
        p.stencil_load=ALMetalLoadAction::load; p.stencil_store=ALMetalStoreAction::store;
        auto pass=target->makeRenderPass(p,&error); REQUIRE(pass);
        auto encoder=ALMetalRenderEncoder::begin(command,*pass,"SMAA actual stencil-equal proof",&error); REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*probe_pipeline,&error));
        REQUIRE(encoder->setDepthStencilState(probe_state.get(),&error));
        REQUIRE(encoder->setStencilReferenceValue(1,&error));
        REQUIRE(encoder->setVertexBuffer(0,*probe_vertices,0,&error));
        REQUIRE(encoder->setViewport({0,0,double(d.width),double(d.height),0,1},&error));
        REQUIRE(encoder->setScissor({0,0,d.width,d.height},&error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,&error)); REQUIRE(encoder->end(&error));
        return target;
    }
    Bytes mask(const ALMetalRenderTarget& edges)
    {
        auto command=device->makeCommandBuffer("SMAA retained stencil observation",&error); REQUIRE(command);
        auto target=encodeProbe(edges,*command); REQUIRE(command->commitAndWait(&error)); return read(*target);
    }
    void clearMask(const ALMetalRenderTarget& edges)
    {
        ALMetalRenderPassConfig p; p.colors[0].load=ALMetalLoadAction::load;
        p.depth_load=ALMetalLoadAction::dont_care;
        p.stencil_load=ALMetalLoadAction::clear; p.stencil_store=ALMetalStoreAction::store; p.clear_stencil=0;
        auto command=device->makeCommandBuffer("SMAA independent zero-mask control",&error); REQUIRE(command);
        auto pass=edges.makeRenderPass(p,&error); REQUIRE(pass);
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"SMAA mask clear only",&error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
    }
};
Bytes patterned(unsigned w,unsigned h)
{
    Bytes pixels(w*h*4);
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x)
    {
        const unsigned v=(x+2*y>=w?144u:16u)+(x*11+y*17+x*y*3)%48;
        const unsigned p=(y*w+x)*4;
        pixels[p]=v; pixels[p+1]=v/2; pixels[p+2]=255-v; pixels[p+3]=255;
    }
    return pixels;
}
void zero(const Bytes& pixels) { CHECK(std::all_of(pixels.begin(),pixels.end(),[](auto x){return x==0;})); }
}

TEST_CASE("SMAA stencil preserves all native image paths", "[almetal][smaa-stencil]")
{
    @autoreleasepool
    {
        Fixture f; constexpr unsigned w=13,h=11;
        auto predication=f.depth(w,h);
        const auto sample_pixels=patterned(7,5);
        auto sample=alMetalCreateUnormTexture2D(*f.device,7,5,4,1,sample_pixels.data(),"SMAA alternate unequal image",&f.error); REQUIRE(sample);
        for(bool hdr:{false,true})
        {
            auto source=f.source(w,h,patterned(w,h),hdr);
            const auto input=f.read(*source); unsigned changed=0;
            for(unsigned quality=0;quality<4;++quality) for(bool pred:{false,true}) for(bool alt:{false,true})
            {
                CAPTURE(hdr,quality,pred,alt);
                ALMetalWorldSMAAScene scene; scene.source=source.get(); scene.quality=quality;
                scene.edge_source_texture=alt?sample.get():nullptr;
                scene.predication_texture=pred?predication.get():nullptr;
                ALMetalWorldSMAAIntermediates ordinary,masked;
                scene.intermediates=&ordinary;
                auto a=f.renderer->renderSMAA(scene,&f.error); INFO(f.error); REQUIRE(a);
                REQUIRE(ordinary.edges); REQUIRE(ordinary.weights);
                scene.use_stencil=true; scene.intermediates=&masked;
                auto b=f.renderer->renderSMAA(scene,&f.error); INFO(f.error); REQUIRE(b);
                REQUIRE(masked.edges); REQUIRE(masked.weights);
                CHECK(f.read(*ordinary.edges)==f.read(*masked.edges));
                CHECK(f.read(*ordinary.weights)==f.read(*masked.weights));
                const auto output=f.read(*b); CHECK(f.read(*a)==output);
                changed+=output!=input;
                CHECK_FALSE(ordinary.edges->depthStencilTexture());
                CHECK_FALSE(masked.weights->depthStencilTexture());
                REQUIRE(masked.edges->depthStencilTexture());
                CHECK(masked.edges->descriptor().depth_stencil_format==ALMetalPixelFormat::depth32_float_stencil8);
            }
            CHECK(changed>0);
        }
    }
}

TEST_CASE("SMAA initial threshold writes real stencil and weights test it", "[almetal][smaa-stencil]")
{
    @autoreleasepool
    {
        Fixture f; constexpr unsigned w=9,h=7;
        Bytes pixels(w*h*4,51);
        for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x)
        {
            const unsigned p=(y*w+x)*4; const unsigned v=x<3?0u:x==3?51u:255u;
            pixels[p]=pixels[p+1]=pixels[p+2]=v; pixels[p+3]=255;
        }
        // A separate diagonal step supplies nonzero area/search weights;
        // the suppressed/flat control texels above remain undisturbed.
        for(unsigned y=0;y<3;++y)
        {
            const unsigned p=(y*w+6+y)*4;
            pixels[p]=pixels[p+1]=pixels[p+2]=0;
        }
        auto source=f.source(w,h,pixels);
        ALMetalWorldSMAAEdgeScene scene; scene.source=source.get(); scene.use_stencil=true;
        auto edges=f.renderer->renderSMAAEdges(scene,&f.error); INFO(f.error); REQUIRE(edges);
        const auto edge_pixels=f.read(*edges);
        const unsigned suppressed=(3*w+3)*4,flat=(3*w)*4,active=(3*w+4)*4;
        CHECK(edge_pixels[suppressed]==0); CHECK(edge_pixels[suppressed+1]==0);
        CHECK(edge_pixels[active]==255);
        auto before=f.mask(*edges);
        CHECK(before[suppressed]==255); CHECK(before[flat]==0); CHECK(before[active]==255);
        ALMetalWorldSMAAWeightScene weight; weight.edges=edges.get();
        auto weights=f.renderer->renderSMAABlendWeights(weight,&f.error); REQUIRE(weights);
        const auto normal=f.read(*weights);
        CHECK(std::any_of(normal.begin(),normal.end(),[](auto x){return x!=0;}));
        CHECK(f.mask(*edges)==before);
        f.clearMask(*edges); CHECK(f.read(*edges)==edge_pixels); zero(f.mask(*edges));
        auto blocked=f.renderer->renderSMAABlendWeights(weight,&f.error); REQUIRE(blocked); zero(f.read(*blocked));
        scene.use_stencil=false;
        auto unmasked_edges=f.renderer->renderSMAAEdges(scene,&f.error); REQUIRE(unmasked_edges);
        weight.edges=unmasked_edges.get();
        auto unmasked=f.renderer->renderSMAABlendWeights(weight,&f.error); REQUIRE(unmasked);
        CHECK(f.read(*unmasked)==normal);
    }
}

TEST_CASE("SMAA alternate image offsets use output dimensions", "[almetal][smaa-stencil]")
{
    @autoreleasepool
    {
        Fixture f; constexpr unsigned w=20,h=8;
        Bytes ramp(5*3*4);
        for(unsigned y=0;y<3;++y) for(unsigned x=0;x<5;++x)
        {
            const unsigned p=(y*5+x)*4; const auto v=std::uint8_t(std::min(x*64u,255u));
            ramp[p]=ramp[p+1]=ramp[p+2]=v; ramp[p+3]=255;
        }
        auto alternate=alMetalCreateUnormTexture2D(*f.device,5,3,4,1,ramp.data(),"SMAA analytic ramp",&f.error); REQUIRE(alternate);
        auto source=f.source(w,h,Bytes(w*h*4,255)); auto depth=f.depth(w,h);
        for(bool stencil:{false,true})
        {
            CAPTURE(stencil);
            ALMetalWorldSMAAEdgeScene scene; scene.source=source.get(); scene.edge_source_texture=alternate.get();
            scene.use_stencil=stencil; scene.quality=0;
            auto edges=f.renderer->renderSMAAEdges(scene,&f.error); REQUIRE(edges); zero(f.read(*edges));
            if(stencil) zero(f.mask(*edges));
            // Output-sized x offset spans only 1/4 of an alternate texel:
            // max delta64/255/4 < .15. A source-sized offset spans a whole
            // texel and would wrongly mark this smooth magnified ramp.
            scene.predication_texture=depth.get(); scene.quality=3; scene.predication_strength=.8f;
            auto predicated=f.renderer->renderSMAAEdges(scene,&f.error); REQUIRE(predicated);
            const auto actual=f.read(*predicated);
            for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x)
            {
                const unsigned p=(y*w+x)*4;
                CHECK(actual[p]==(x==w/2?255u:0u));
                CHECK(actual[p+1]==0); CHECK(actual[p+2]==0); CHECK(actual[p+3]==0);
            }
            if(stencil)
            {
                const auto mask=f.mask(*predicated);
                for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) CHECK(mask[(y*w+x)*4]==(x==w/2?255u:0u));
            }
        }
        // Same ramp at its own resolution has real threshold-crossing edges.
        auto small=f.source(5,3,ramp); ALMetalWorldSMAAEdgeScene scene; scene.source=small.get(); scene.use_stencil=true;
        auto active=f.renderer->renderSMAAEdges(scene,&f.error); REQUIRE(active);
        const auto observed=f.mask(*active); CHECK(std::any_of(observed.begin(),observed.end(),[](auto x){return x==255;}));
    }
}

TEST_CASE("SMAA stencil owners survive encoding and failed transactions retry", "[almetal][smaa-stencil]")
{
    @autoreleasepool
    {
        Fixture f; constexpr unsigned w=9,h=7;
        auto source=f.source(w,h,patterned(w,h));
        ALMetalWorldSMAAIntermediates intermediates;
        ALMetalWorldSMAAScene scene; scene.source=source.get(); scene.use_stencil=true; scene.intermediates=&intermediates;
        auto output=f.renderer->renderSMAA(scene,&f.error); REQUIRE(output);
        const auto expected=f.mask(*intermediates.edges);
        REQUIRE(std::any_of(expected.begin(),expected.end(),[](auto x){return x==255;}));
        scene.quality=4; CHECK_FALSE(f.renderer->renderSMAA(scene,&f.error));
        CHECK_FALSE(intermediates.edges); CHECK_FALSE(intermediates.weights);
        scene.quality=0;
        REQUIRE(f.renderer->beginCommandBatch("SMAA cancellation control",&f.error));
        auto canceled=f.renderer->renderSMAA(scene,&f.error); REQUIRE(canceled);
        f.renderer->cancelCommandBatch(); canceled.reset(); intermediates={};
        REQUIRE(f.renderer->beginCommandBatch("SMAA retry",&f.error));
        auto retry=f.renderer->renderSMAA(scene,&f.error); REQUIRE(retry);
        REQUIRE(f.renderer->commitCommandBatch(&f.error));
        CHECK(f.mask(*intermediates.edges)==expected);
        // Encode a hardware observation and readback while all owners are
        // alive. Then retire their wrappers and renderer before submission;
        // the actual native command must retain the mask and proof resources.
        auto command=f.device->makeCommandBuffer("SMAA encoded mask lease",&f.error); REQUIRE(command);
        auto proof=f.encodeProbe(*intermediates.edges,*command);
        constexpr unsigned row=256;
        ALMetalBufferDescriptor b; b.length=row*h; b.storage=ALMetalStorageMode::shared;
        auto readback=ALMetalBuffer::create(*f.device,b,&f.error); REQUIRE(readback);
        id<MTLBlitCommandEncoder> blit=[(__bridge id<MTLCommandBuffer>)command->nativeHandle() blitCommandEncoder]; REQUIRE(blit);
        [blit copyFromTexture:(__bridge id<MTLTexture>)proof->colorTexture(0)->nativeHandle()
                 sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(w,h,1)
                    toBuffer:(__bridge id<MTLBuffer>)readback->nativeHandle() destinationOffset:0
         destinationBytesPerRow:row destinationBytesPerImage:row*h];
        [blit endEncoding];
        proof.reset(); intermediates={}; output.reset(); retry.reset(); source.reset(); f.renderer.reset();
        REQUIRE(command->commitAndWait(&f.error));
        Bytes actual(w*h*4,0xcd);
        for(unsigned y=0;y<h;++y) REQUIRE(readback->read(y*row,actual.data()+y*w*4,w*4,&f.error));
        CHECK(actual==expected);
    }
}
