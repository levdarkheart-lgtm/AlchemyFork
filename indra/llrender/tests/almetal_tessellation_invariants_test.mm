/**
 * @file almetal_tessellation_invariants_test.mm
 * @brief Fractional-odd quad tessellation: actual coverage, winding and emitted topology.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "../../almetal/almetalpipeline.h"
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <vector>

namespace
{
constexpr unsigned side=64;
using Point=std::array<float,2>;
using Corners=std::array<Point,4>;
constexpr Corners ccw{{{-1,-1},{1,-1},{1,1},{-1,1}}};
constexpr Corners cw{{{-1,-1},{-1,1},{1,1},{1,-1}}};
using Vertex=std::array<int,2>;
using Triangle=std::array<Vertex,3>;
constexpr int coordinate_scale=10000;

// Metal's primitive_id is the patch ID for tessellation, not the emitted
// triangle ID. Instead reconstruct each actual rasterized triangle's three
// domain vertices from its hardware barycentrics and their derivatives.
// No expected CPU grid or primitive-count formula participates in this oracle.
constexpr const char* source=R"metal(
#include <metal_stdlib>
using namespace metal;
struct Out {float4 position [[position]];float2 uv;};
[[patch(quad,4)]]
vertex Out patch_vertex(const device float2* corners [[buffer(0)]],
                        float2 uv [[position_in_patch]])
{
    float2 bottom=mix(corners[0],corners[1],uv.x);
    float2 top=mix(corners[3],corners[2],uv.x);
    return {float4(mix(bottom,top,uv.y),0,1),uv};
}
// This separate non-rasterizing function observes generated domain vertices
// even when they belong to degenerate triangles along a subdivided edge.
[[patch(quad,4)]]
vertex void capture_vertices(float2 uv [[position_in_patch]],
    device atomic_uint* count [[buffer(0)]],device float2* vertices [[buffer(1)]])
{
    uint index=atomic_fetch_add_explicit(count,1,memory_order_relaxed);
    if (index<4096) vertices[index]=uv;
}
struct Result {float4 ab [[color(0)]];float4 c [[color(1)]];};
fragment Result triangle_vertices(Out in [[stage_in]],float3 bary [[barycentric_coord]])
{
    float2 dx=dfdx(bary.xy),dy=dfdy(bary.xy);
    float2 ux=dfdx(in.uv),uy=dfdy(in.uv);
    float det=dx.x*dy.y-dx.y*dy.x;
    float2 bx=(dy.y*ux-dx.y*uy)/det;
    float2 by=(-dy.x*ux+dx.x*uy)/det;
    float2 origin=in.uv-bary.x*bx-bary.y*by;
    return {float4(origin+bx,origin+by),float4(origin,0,1)};
}
)metal";

struct Pixels
{
    std::vector<std::array<float,4>> ab,c;
    std::set<Triangle> triangles() const
    {
        std::set<Triangle> result;
        for (unsigned pixel=0;pixel<ab.size();++pixel)
        {
            if (c[pixel][3]==0) continue;
            Triangle triangle;
            const std::array<Point,3> vertices{{
                {ab[pixel][0],ab[pixel][1]},
                {ab[pixel][2],ab[pixel][3]},
                {c[pixel][0],c[pixel][1]}}};
            for (unsigned v=0;v<3;++v)
                for (unsigned axis=0;axis<2;++axis)
                {
                    REQUIRE(std::isfinite(vertices[v][axis]));
                    REQUIRE(vertices[v][axis]>=-1e-4f);
                    REQUIRE(vertices[v][axis]<=1.0001f);
                    triangle[v][axis]=static_cast<int>(std::lround(vertices[v][axis]*coordinate_scale));
                }
            std::sort(triangle.begin(),triangle.end());
            result.insert(triangle);
        }
        return result;
    }
    void covered(bool expected) const
    {
        REQUIRE(c.size()==side*side);
        for (unsigned pixel=0;pixel<c.size();++pixel)
        {
            CAPTURE(pixel);
            CHECK(c[pixel][3]==(expected?1.f:0.f));
        }
    }
};
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;
    Fixture()
    {
        device=ALMetalDevice::create(&error);INFO(error);REQUIRE(device);
        id<MTLDevice> native=(__bridge id<MTLDevice>)device->nativeHandle();
        // Honest native capability, required on the Apple Silicon target.
        REQUIRE(native.supportsShaderBarycentricCoordinates);
        library=ALMetalShaderLibrary::compileSource(*device,source,"Native tessellation topology oracle",&error);
        INFO(error);REQUIRE(library);
        ALMetalRenderPipelineDescriptor d;
        d.library=library.get();d.vertex_function="patch_vertex";d.fragment_function="triangle_vertices";
        d.color_attachment_count=2;
        d.colors[0].format=d.colors[1].format=ALMetalPixelFormat::rgba32_float;
        d.tessellation.enabled=true;
        d.tessellation.partition_mode=ALMetalTessellationPartitionMode::fractional_odd;
        d.tessellation.maximum_factor=16;
        d.tessellation.output_winding=ALMetalTessellationOutputWinding::counter_clockwise;
        pipeline=ALMetalRenderPipeline::create(*device,d,&error);INFO(error);REQUIRE(pipeline);
    }
    std::set<Vertex> capture(const std::array<float,6>& values)
    {
        id<MTLDevice> native=(__bridge id<MTLDevice>)device->nativeHandle();
        id<MTLLibrary> lib=(__bridge id<MTLLibrary>)library->nativeHandle();
        MTLRenderPipelineDescriptor* pd=[[MTLRenderPipelineDescriptor alloc] init];
        pd.vertexFunction=[lib newFunctionWithName:@"capture_vertices"];
        pd.rasterizationEnabled=NO;
        pd.maxTessellationFactor=16;
        pd.tessellationFactorFormat=MTLTessellationFactorFormatHalf;
        pd.tessellationPartitionMode=MTLTessellationPartitionModeFractionalOdd;
        pd.tessellationOutputWindingOrder=MTLWindingCounterClockwise;
        NSError* native_error=nil;
        id<MTLRenderPipelineState> state=[native newRenderPipelineStateWithDescriptor:pd error:&native_error];
        INFO((native_error?native_error.localizedDescription.UTF8String:""));REQUIRE(state);
        auto buffer=[&](std::size_t length)
        {
            ALMetalBufferDescriptor d;d.length=length;
            auto result=ALMetalBuffer::create(*device,d,&error);REQUIRE(result);return result;
        };
        std::array<__fp16,6> factors;
        std::transform(values.begin(),values.end(),factors.begin(),[](float f){return __fp16(f);});
        auto tess=buffer(sizeof(factors));REQUIRE(tess->write(0,factors.data(),sizeof(factors),&error));
        const unsigned zero=0;
        auto count=buffer(sizeof(zero));REQUIRE(count->write(0,&zero,sizeof(zero),&error));
        auto vertices=buffer(4096*sizeof(Point));
        auto command=device->makeCommandBuffer("Capture actual post-tessellation coordinates",&error);REQUIRE(command);
        id<MTLCommandBuffer> native_command=(__bridge id<MTLCommandBuffer>)command->nativeHandle();
        MTLRenderPassDescriptor* pass=[MTLRenderPassDescriptor renderPassDescriptor];
        pass.renderTargetWidth=pass.renderTargetHeight=1;
        pass.defaultRasterSampleCount=1;
        id<MTLRenderCommandEncoder> encoder=[native_command renderCommandEncoderWithDescriptor:pass];REQUIRE(encoder);
        [encoder setRenderPipelineState:state];
        [encoder setVertexBuffer:(__bridge id<MTLBuffer>)count->nativeHandle() offset:0 atIndex:0];
        [encoder setVertexBuffer:(__bridge id<MTLBuffer>)vertices->nativeHandle() offset:0 atIndex:1];
        [encoder setTessellationFactorBuffer:(__bridge id<MTLBuffer>)tess->nativeHandle() offset:0 instanceStride:0];
        [encoder drawPatches:4 patchStart:0 patchCount:1 patchIndexBuffer:nil patchIndexBufferOffset:0 instanceCount:1 baseInstance:0];
        [encoder endEncoding];REQUIRE(command->commitAndWait(&error));INFO(error);
        const auto* number=static_cast<const unsigned*>(count->mappedBytes());REQUIRE(number);
        REQUIRE(*number>0);REQUIRE(*number<=4096);
        const auto* output=static_cast<const Point*>(vertices->mappedBytes());REQUIRE(output);
        std::set<Vertex> result;
        for (unsigned i=0;i<*number;++i)
        {
            Vertex v;
            for (unsigned axis=0;axis<2;++axis)
            {
                REQUIRE(std::isfinite(output[i][axis]));
                REQUIRE(output[i][axis]>=0);REQUIRE(output[i][axis]<=1);
                v[axis]=static_cast<int>(std::lround(output[i][axis]*coordinate_scale));
            }
            result.insert(v);
        }
        return result;
    }
    Pixels draw(const Corners& corners,const std::array<float,6>& values)
    {
        std::array<__fp16,6> factors;
        std::transform(values.begin(),values.end(),factors.begin(),[](float f){return __fp16(f);});
        auto buffer=[&](const void* bytes,std::size_t size)
        {
            ALMetalBufferDescriptor d;d.length=size;
            auto result=ALMetalBuffer::create(*device,d,&error);REQUIRE(result);
            REQUIRE(result->write(0,bytes,size,&error));return result;
        };
        auto points=buffer(corners.data(),sizeof(corners));
        auto tess=buffer(factors.data(),sizeof(factors));
        ALMetalRenderTargetDescriptor d;
        d.width=d.height=side;d.color_attachment_count=2;
        d.colors[0].format=d.colors[1].format=ALMetalPixelFormat::rgba32_float;
        auto target=ALMetalRenderTarget::create(*device,d,&error);INFO(error);REQUIRE(target);
        ALMetalRenderPassConfig config;
        for (unsigned i=0;i<2;++i)
        {
            config.colors[i].load=ALMetalLoadAction::clear;
            config.colors[i].store=ALMetalStoreAction::store;
            config.colors[i].clear={0,0,0,0};
        }
        auto pass=target->makeRenderPass(config,&error);REQUIRE(pass);
        auto command=device->makeCommandBuffer("Native fractional-odd tessellation invariant",&error);REQUIRE(command);
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Actual patch topology",&error);REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline,&error));
        REQUIRE(encoder->setVertexBuffer(0,*points,0,&error));
        REQUIRE(encoder->setTessellationFactorBuffer(*tess,0,0,&error));
        REQUIRE(encoder->setViewport({0,0,side,side,0,1},&error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::back,&error));
        REQUIRE(encoder->setFrontFacingWinding(ALMetalWinding::counter_clockwise,&error));
        REQUIRE(encoder->drawPatches(4,0,1,1,0,&error));
        REQUIRE(encoder->end(&error));REQUIRE(command->commitAndWait(&error));INFO(error);
        Pixels result;result.ab.resize(side*side);result.c.resize(side*side);
        REQUIRE(target->colorTexture(0)->readback(*device,0,0,{},result.ab.data(),side*sizeof(result.ab[0]),&error));
        REQUIRE(target->colorTexture(1)->readback(*device,0,0,{},result.c.data(),side*sizeof(result.c[0]),&error));
        return result;
    }
};
}

TEST_CASE("Native fractional-odd quad creation and level-one winding preserve coverage",
          "[almetal][tessellation-invariants]")
{
    @autoreleasepool
    {
        Fixture f;
        CHECK(f.pipeline->descriptor().tessellation.enabled);
        CHECK(f.pipeline->descriptor().tessellation.partition_mode==ALMetalTessellationPartitionMode::fractional_odd);
        f.draw(ccw,{1,1,1,1,1,1}).covered(true);
        f.draw(cw,{1,1,1,1,1,1}).covered(false);
    }
}
TEST_CASE("Native fractional-odd subdivisions leave no raster cracks",
          "[almetal][tessellation-invariants]")
{
    @autoreleasepool
    {
        Fixture f;
        for (float level:{4.f,2.5f,7.25f})
        {
            CAPTURE(level);
            f.draw(ccw,{level,level,level,level,level,level}).covered(true);
        }
    }
}
TEST_CASE("Native raster barycentrics expose exactly two and eighteen generated triangles",
          "[almetal][tessellation-invariants]")
{
    @autoreleasepool
    {
        Fixture f;
        const auto one=f.draw(ccw,{1,1,1,1,1,1});one.covered(true);
        CHECK(one.triangles().size()==2);
        const auto three=f.draw(ccw,{3,3,3,3,3,3});three.covered(true);
        CHECK(three.triangles().size()==18);
    }
}
TEST_CASE("Native outer tessellation factors split only their corresponding domain edges",
          "[almetal][tessellation-invariants]")
{
    @autoreleasepool
    {
        Fixture f;
        // Original GLSL and Metal both order factors u=0,v=0,u=1,v=1.
        for (unsigned raised=0;raised<4;++raised)
        {
            std::array<float,6> factors{1,1,1,1,1,1};factors[raised]=3;
            const auto pixels=f.draw(ccw,factors);pixels.covered(true);
            const auto generated=f.capture(factors);
            REQUIRE_FALSE(generated.empty());
            for (unsigned edge=0;edge<4;++edge)
            {
                std::set<Vertex> points;
                for (const auto& point:generated)
                    if (point[edge%2]==(edge<2?0:coordinate_scale)) points.insert(point);
                CAPTURE(raised,edge);
                CHECK(points.size()==(edge==raised?4:2));
            }
        }
    }
}
