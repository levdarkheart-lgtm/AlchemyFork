/**
 * @file almetal_indexed_join_test.mm
 * @brief Native deferred-index draw barriers, output and IBO lifetime.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalshadowmaterials.h"
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace
{
struct alignas(16) JoinWorld
{
    float mvp[16]{}, mv[16]{}, normal[3][4]{}, rigged[3][4]{};
    float texture[16]{}, material[4]{};
    std::uint32_t config[4]{};
    float tail[5][4]{};
};
static_assert(sizeof(JoinWorld) == 400);
enum class JoinBoundary
{
    none, cached, offset, bytes, texture_view, pipeline, scissor,
    disabled_query, active_query, invalid, toggle, interop, lifetime, nonindexed, instanced
};
struct JoinPixels
{
    std::array<std::vector<std::uint8_t>, 4> colors;
    std::vector<std::uint32_t> depth;
    std::vector<std::uint8_t> stencil;
    bool operator==(const JoinPixels&) const = default;
};
}

TEST_CASE("almetal: indexed triangle joining respects native barriers and owns pending IBO",
          "[almetal][indexed-join]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error); REQUIRE(device);
        REQUIRE([[NSBundle mainBundle] pathForResource:@"almetal_builtin" ofType:@"metallib"].length != 0);
        auto library = ALMetalShaderLibrary::loadBuiltin(*device, &error);
        INFO(error); REQUIRE(library); // Packaged shader, no runtime MSL clone.
        constexpr unsigned size = 64, row = 256, plane = row * size;
        ALMetalRenderPipelineDescriptor pd;
        pd.library = library.get(); pd.label = "Indexed join ordinary MRT PSO";
        pd.vertex_function = "almetal_world_simple_vertex";
        pd.fragment_function = "almetal_world_simple_fragment";
        pd.color_attachment_count = 4;
        for (unsigned i = 0; i < 4; ++i) pd.colors[i].format = ALMetalPixelFormat::rgba8_unorm;
        pd.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        auto pipeline = ALMetalRenderPipeline::create(*device, pd, &error);
        REQUIRE(pipeline);
        pd.colors[0].write_mask = ALMetalColorWriteMask::red;
        pd.label = "Indexed join red-only MRT PSO";
        auto red_only = ALMetalRenderPipeline::create(*device, pd, &error);
        REQUIRE(red_only);
        ALMetalDepthStencilDescriptor dd; dd.label = "Indexed join depth and stencil";
        dd.depth_compare = ALMetalCompareFunction::always;
        dd.stencil_enabled = true;
        dd.front_stencil.depth_stencil_pass = ALMetalStencilOperation::increment_clamp;
        dd.back_stencil = dd.front_stencil;
        auto depth = ALMetalDepthStencilState::create(*device, dd, &error);
        REQUIRE(depth);
        ALMetalSamplerDescriptor sd; sd.label = "Indexed join fixed sampler";
        sd.support_argument_buffers = true;
        auto sampler = ALMetalSamplerState::create(*device, sd, &error);
        REQUIRE(sampler);
        auto materials = ALMetalShadowMaterials::create(*device, *library, &error);
        REQUIRE(materials); REQUIRE(materials->supported());
        ALMetalTextureDescriptor td; td.label="Indexed join encoded-grey texture";
        td.format = ALMetalPixelFormat::rgba8_unorm_srgb;
        auto texture = ALMetalTexture::create(*device, td, &error);
        REQUIRE(texture);
        auto upload = device->makeCommandBuffer("Indexed join source upload", &error);
        REQUIRE(upload);
        const std::array<std::uint8_t, 4> grey{128,128,128,255};
        REQUIRE(texture->upload(*upload, 0, 0, {}, grey.data(), 4, &error));
        REQUIRE(upload->commitAndWait(&error));

        JoinWorld world;
        for (unsigned i : {0u,5u,10u,15u}) world.mvp[i] = world.mv[i] = world.texture[i] = 1.f;
        for (unsigned i = 0; i < 3; ++i) world.normal[i][i] = world.rigged[i][i] = 1.f;
        world.config[0] = world.config[1] = 1; // Native depth + one populated texture.
        world.material[1] = 1.f;
        const std::array<std::array<float,4>,6> positions{{
            {{-1,-1,.25f,1}},{{3,-1,.25f,1}},{{-1,3,.25f,1}},
            {{-1,-1,.25f,1}},{{3,-1,.25f,1}},{{-1,3,.25f,1}}}};
        constexpr std::size_t normals = sizeof(positions), uv = normals + 6*16, color = uv + 6*8;
        std::array<std::uint8_t, color + 9*4> vertex_bytes{};
        std::memcpy(vertex_bytes.data(), positions.data(), sizeof(positions));
        for (unsigned i = 0; i < 6; ++i)
        {
            const std::array<float,4> n{0,0,1,0}; const std::array<float,2> t{.5f,.5f};
            std::memcpy(vertex_bytes.data()+normals+i*16,n.data(),16);
            std::memcpy(vertex_bytes.data()+uv+i*8,t.data(),8);
        }
        std::array<std::array<std::uint8_t,4>,9> colors{};
        for (unsigned i=0;i<9;++i) colors[i] = i<3 ? std::array<std::uint8_t,4>{255,128,0,255}
            : i<6 ? std::array<std::uint8_t,4>{0,255,0,255} : std::array<std::uint8_t,4>{0,0,255,255};
        std::memcpy(vertex_bytes.data()+color, colors.data(), sizeof(colors));
        const auto buffer = [&](const void* bytes, std::size_t length)
        {
            ALMetalBufferDescriptor d; d.label="Indexed join immutable input"; d.length=length;
            auto result=ALMetalBuffer::create(*device,d,&error); REQUIRE(result);
            REQUIRE(result->write(0,bytes,length,&error)); return result;
        };
        auto vertices=buffer(vertex_bytes.data(),vertex_bytes.size());
        auto constants=buffer(&world,sizeof(world));
        struct TimingRestore { bool old=ALMetalDevice::commandTimingEnabled();
            ~TimingRestore(){ALMetalDevice::setCommandTimingEnabled(old);} } timing_restore;
        ALMetalDevice::setCommandTimingEnabled(true);

        // One fresh target/encoder per arm. Only the selected boundary differs.
        const auto render = [&](int joining, JoinBoundary boundary, ALMetalIndexType type)
        {
            CAPTURE(joining, static_cast<unsigned>(boundary), static_cast<unsigned>(type));
            ALMetalRenderTargetDescriptor rt; rt.label="Indexed join fresh MRT target"; rt.width=rt.height=size; rt.color_attachment_count=4;
            for (unsigned i=0;i<4;++i) rt.colors[i].format=ALMetalPixelFormat::rgba8_unorm;
            rt.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
            auto target=ALMetalRenderTarget::create(*device,rt,&error); REQUIRE(target);
            ALMetalRenderPassConfig pc; pc.depth_store=pc.stencil_store=ALMetalStoreAction::store;
            pc.clear_depth=0.; pc.clear_stencil=3;
            auto pass=target->makeRenderPass(pc,&error); REQUIRE(pass);
            auto command=device->makeCommandBuffer("Indexed joining wrapper test",&error); REQUIRE(command);
            id<MTLDevice> native_device=(__bridge id<MTLDevice>)device->nativeHandle();
            id<MTLBuffer> query=[native_device newBufferWithLength:8 options:MTLResourceStorageModeShared];
            REQUIRE(query!=nil); std::memset(query.contents,0,8);
            ((__bridge MTLRenderPassDescriptor*)pass->nativeHandle()).visibilityResultBuffer=query;
            std::array<std::uint16_t,9> i16{0,1,2,3,4,5,0,1,2};
            std::array<std::uint32_t,9> i32{0,1,2,3,4,5,0,1,2};
            if(boundary==JoinBoundary::active_query)
                for(unsigned k=3;k<6;++k) i16[k]=i32[k]=3; // Queried second triangle is degenerate.
            const bool wide=type==ALMetalIndexType::uint32;
            auto indices=buffer(wide?static_cast<const void*>(i32.data()):i16.data(),wide?sizeof(i32):sizeof(i16));
            const ALMetalTexture* source=texture.get();
            const std::array<ALMetalShadowMaterialRequest,1> requests{{{&source,1,sampler.get()}}};
            auto material_pass=materials->prepare(*command,requests,&error); REQUIRE(material_pass);
            const auto before=ALMetalDevice::encodedDrawCallCount();
            auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Indexed join test encoder",&error); REQUIRE(encoder);
            const auto bind = [&]
            {
                REQUIRE(encoder->setPipeline(*pipeline,&error));
                REQUIRE(encoder->setDepthStencilState(depth.get(),&error));
                REQUIRE(encoder->setStencilReferenceValue(7,&error));
                REQUIRE(encoder->setViewport({0,0,size,size,0,1},&error));
                REQUIRE(encoder->setScissor({0,0,size,size},&error));
                REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&error));
                REQUIRE(encoder->setTriangleFillMode(ALMetalTriangleFillMode::fill,&error));
                REQUIRE(encoder->setVertexBuffer(0,*vertices,0,&error));
                REQUIRE(encoder->setVertexBuffer(1,*vertices,normals,&error));
                REQUIRE(encoder->setVertexBuffer(2,*vertices,uv,&error));
                REQUIRE(encoder->setVertexBuffer(3,*vertices,color,&error));
                REQUIRE(encoder->setVertexBuffer(4,*constants,0,&error));
                REQUIRE(encoder->setFragmentSampler(0,sampler.get(),&error));
                for(unsigned slot=0;slot<32;++slot) REQUIRE(encoder->setFragmentTexture(slot,texture.get(),&error));
            };
            bind();
            REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::disabled,0,&error));
            if(joining>=0) REQUIRE(encoder->setIndexedTriangleJoining(joining!=0,&error));
            REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle,3,type,*indices,0,1,&error));
            if(boundary==JoinBoundary::cached) bind();
            if(boundary==JoinBoundary::offset) REQUIRE(encoder->setVertexBuffer(3,*vertices,color+12,&error));
            if(boundary==JoinBoundary::bytes)
            {
                auto blue=colors; for(unsigned i=3;i<6;++i) blue[i]={0,0,255,255};
                REQUIRE(encoder->setVertexBytes(3,blue.data(),sizeof(blue),&error));
            }
            if(boundary==JoinBoundary::texture_view) REQUIRE(encoder->setFragmentTextureLinear(0,texture.get(),&error));
            if(boundary==JoinBoundary::pipeline) REQUIRE(encoder->setPipeline(*red_only,&error));
            if(boundary==JoinBoundary::scissor) REQUIRE(encoder->setScissor({0,0,size/2,size},&error));
            if(boundary==JoinBoundary::disabled_query) REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::disabled,0,&error));
            if(boundary==JoinBoundary::active_query) REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::boolean,0,&error));
            if(boundary==JoinBoundary::invalid)
            {
                CHECK_FALSE(encoder->drawIndexed(ALMetalPrimitiveType::triangle,3,type,*indices,indices->length(),1,&error));
                CHECK_FALSE(encoder->setVertexBuffer(31,*vertices,0,&error));
                CHECK_FALSE(encoder->setVertexBytes(3,nullptr,0,&error)); error.clear();
            }
            if(boundary==JoinBoundary::toggle)
            {
                REQUIRE(encoder->setIndexedTriangleJoining(false,&error));
                REQUIRE(encoder->setIndexedTriangleJoining(true,&error));
            }
            if(boundary==JoinBoundary::interop) REQUIRE(material_pass->declareResources(*encoder,&error));
            if(boundary==JoinBoundary::nonindexed) REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,&error));
            const bool lifetime=boundary==JoinBoundary::lifetime;
            if(!lifetime)
            {
                REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle,3,type,*indices,3*(wide?4:2),boundary==JoinBoundary::instanced?2:1,&error));
                if(boundary==JoinBoundary::interop)
                    REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle,3,type,*indices,6*(wide?4:2),1,&error));
            }
            else
            {
                __weak id<MTLBuffer> witness=(__bridge id<MTLBuffer>)indices->nativeHandle();
                indices.reset(); CHECK(witness!=nil);
                encoder.reset(); // Destructor must flush before endEncoding.
            }
            if(encoder) REQUIRE(encoder->end(&error));
            const auto draws=ALMetalDevice::encodedDrawCallCount()-before;
            const bool can_join=joining>0 && (boundary==JoinBoundary::none||boundary==JoinBoundary::cached||boundary==JoinBoundary::invalid);
            CHECK(draws==(lifetime?1u:(boundary==JoinBoundary::interop||boundary==JoinBoundary::nonindexed)?3u:can_join?1u:2u));
            ALMetalBufferDescriptor staging_desc; staging_desc.label="Indexed join depth stencil readback"; staging_desc.length=2*plane;
            auto staging=ALMetalBuffer::create(*device,staging_desc,&error); REQUIRE(staging);
            id<MTLCommandBuffer> native_command=(__bridge id<MTLCommandBuffer>)command->nativeHandle();
            id<MTLBlitCommandEncoder> blit=[native_command blitCommandEncoder]; REQUIRE(blit!=nil);
            id<MTLTexture> ds=(__bridge id<MTLTexture>)target->depthStencilTexture()->nativeHandle();
            id<MTLBuffer> destination=(__bridge id<MTLBuffer>)staging->nativeHandle();
            [blit copyFromTexture:ds sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                sourceSize:MTLSizeMake(size,size,1) toBuffer:destination destinationOffset:0
                destinationBytesPerRow:row destinationBytesPerImage:plane options:MTLBlitOptionDepthFromDepthStencil];
            [blit copyFromTexture:ds sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                sourceSize:MTLSizeMake(size,size,1) toBuffer:destination destinationOffset:plane
                destinationBytesPerRow:row destinationBytesPerImage:plane options:MTLBlitOptionStencilFromDepthStencil];
            [blit endEncoding]; REQUIRE(command->commitAndWait(&error));
            const auto query_result=*static_cast<const std::uint64_t*>(query.contents);
            CHECK(query_result==0u); // Pending visible first triangle must stay OUTSIDE the query.
            JoinPixels result; result.depth.resize(size*size); result.stencil.resize(size*size);
            for(unsigned slot=0;slot<4;++slot)
            {
                result.colors[slot].resize(size*size*4);
                REQUIRE(target->colorTexture(slot)->readback(*device,0,0,{},result.colors[slot].data(),row,&error));
            }
            REQUIRE(staging->read(0,result.depth.data(),plane,&error));
            for(unsigned y=0;y<size;++y) REQUIRE(staging->read(plane+y*row,result.stencil.data()+y*size,size,&error));
            const unsigned p=(size/2)*size+size/2;
            CHECK(result.depth[p]==0x3e800000u); // Actual stored .25f, not a tolerance.
            if(lifetime) CHECK(result.colors[0][4*p]>0);
            else if(boundary==JoinBoundary::offset||boundary==JoinBoundary::bytes) CHECK(result.colors[0][4*p+2]>0);
            else if(boundary!=JoinBoundary::pipeline && boundary!=JoinBoundary::scissor && boundary!=JoinBoundary::interop)
                CHECK(result.colors[0][4*p+1]>0);
            return result;
        };
        for(auto type:{ALMetalIndexType::uint16,ALMetalIndexType::uint32})
        {
            const auto original=render(-1,JoinBoundary::none,type);
            CHECK(std::all_of(original.depth.begin(),original.depth.end(),[](auto x){return x==0x3e800000u;}));
            CHECK(std::all_of(original.stencil.begin(),original.stencil.end(),[](auto x){return x==5u;}));
            CHECK(render(0,JoinBoundary::none,type)==original);
            CHECK(render(1,JoinBoundary::none,type)==original);
            CHECK(render(1,JoinBoundary::cached,type)==original);
            CHECK(render(1,JoinBoundary::invalid,type)==original);
            for(auto boundary:{JoinBoundary::offset,JoinBoundary::bytes,JoinBoundary::texture_view,
                JoinBoundary::pipeline,JoinBoundary::scissor,JoinBoundary::disabled_query,
                JoinBoundary::active_query,JoinBoundary::toggle,JoinBoundary::interop,JoinBoundary::lifetime,
                JoinBoundary::nonindexed,JoinBoundary::instanced})
            {
                const auto off=render(0,boundary,type), on=render(1,boundary,type);
                CHECK(on==off);
                if(boundary==JoinBoundary::offset||boundary==JoinBoundary::bytes||boundary==JoinBoundary::texture_view||
                   boundary==JoinBoundary::pipeline||boundary==JoinBoundary::scissor||boundary==JoinBoundary::interop||boundary==JoinBoundary::lifetime||
                   boundary==JoinBoundary::nonindexed||boundary==JoinBoundary::instanced)
                    CHECK(on!=original);
            }
        }
    }
}
