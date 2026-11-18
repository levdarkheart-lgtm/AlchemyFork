/**
 * @file alworld_lighting_raster_tiles_metal_test.mm
 * @brief Actual production BASE/forward raster geometry parity and opt-in costs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Original1eb44 deferred soften/reflectionProbe and ordered alpha ownership is
 * retained. Native raster tile dimensions are new allocation geometry, with no
 * shader, probe selection, pixel quality or pass-order change.
 * Source-only until integrated against the matching world/resource ABI.
 */
#include "../../almetal/almetalworld.h"
#include "../../almetal/almetalprobecandidates.h"
#include "../../almetal/almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace lighting_tiles_base
{
using Pixel = std::array<std::uint16_t,4>;
struct DepthStencil
{
    std::vector<std::uint32_t> depth;
    std::vector<std::uint8_t> stencil;
};
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device = ALMetalDevice::create(&error);
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    Fixture()
    {
        REQUIRE([[NSBundle mainBundle] pathForResource:@"almetal_builtin" ofType:@"metallib"] != nil);
        INFO(error); REQUIRE(device);
        data = ALMetalWorldFrameData::create(*device,&error); REQUIRE(data);
        renderer = ALMetalWorldGBufferRenderer::create(*device,&error); INFO(error); REQUIRE(renderer);
        renderer->setFrameData(data);
        REQUIRE(renderer->supportsProbeLightingFusion());
    }
    ~Fixture() { if (device) device->waitUntilIdle(); }
    std::vector<Pixel> colors(const ALMetalRenderTarget& target)
    {
        const auto& descriptor = target.descriptor();
        REQUIRE(target.colorTexture(0));
        REQUIRE(target.colorTexture(0)->descriptor().format == ALMetalPixelFormat::rgba16_float);
        std::vector<Pixel> result(std::size_t(descriptor.width)*descriptor.height);
        REQUIRE(target.colorTexture(0)->readback(*device,0,0,{},result.data(),descriptor.width*8,&error));
        return result;
    }
    DepthStencil depthStencil(const ALMetalTexture& texture)
    {
        const auto width = texture.width(), height = texture.height();
        const auto depth_row = (width*4u+255u)&~255u;
        const auto stencil_row = (width+255u)&~255u;
        const std::size_t stencil_offset = std::size_t(depth_row)*height;
        auto command = device->makeCommandBuffer("Candidate lighting depth/stencil correctness readback",&error);
        REQUIRE(command);
        id<MTLDevice> native = (__bridge id<MTLDevice>)device->nativeHandle();
        id<MTLBuffer> staging = [native newBufferWithLength:stencil_offset+std::size_t(stencil_row)*height
            options:MTLResourceStorageModeShared]; REQUIRE(staging);
        id<MTLBlitCommandEncoder> blit = [(__bridge id<MTLCommandBuffer>)command->nativeHandle() blitCommandEncoder];
        REQUIRE(blit);
        id<MTLTexture> source = (__bridge id<MTLTexture>)texture.nativeHandle();
        [blit copyFromTexture:source sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(width,height,1) toBuffer:staging destinationOffset:0
            destinationBytesPerRow:depth_row destinationBytesPerImage:std::size_t(depth_row)*height
            options:MTLBlitOptionDepthFromDepthStencil];
        [blit copyFromTexture:source sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(width,height,1) toBuffer:staging destinationOffset:stencil_offset
            destinationBytesPerRow:stencil_row destinationBytesPerImage:std::size_t(stencil_row)*height
            options:MTLBlitOptionStencilFromDepthStencil];
        [blit endEncoding]; REQUIRE(command->commitAndWait(&error));
        DepthStencil result;
        result.depth.resize(std::size_t(width)*height); result.stencil.resize(std::size_t(width)*height);
        const auto* bytes = static_cast<const std::uint8_t*>(staging.contents);
        for (unsigned y=0;y<height;++y)
        {
            std::memcpy(result.depth.data()+std::size_t(y)*width,bytes+std::size_t(y)*depth_row,width*4);
            std::memcpy(result.stencil.data()+std::size_t(y)*width,
                bytes+stencil_offset+std::size_t(y)*stencil_row,width);
        }
        return result;
    }
};

struct Cell
{
    bool pbr, reverse, infinite, hdr, probe;
    unsigned width, height;
};

struct Inputs
{
    Fixture& f;
    Cell cell;
    std::array<std::unique_ptr<ALMetalTexture>,4> gbuffer_colors;
    std::unique_ptr<ALMetalRenderTarget> gbuffer;
    std::unique_ptr<ALMetalTexture> light_map, radiance, irradiance;
    ALMetalWorldReflectionProbes probes;
    ALMetalWorldBaseLightingScene base;
    ALMetalWorldLightMapScene map;
    Inputs(Fixture& fixture, Cell sample, unsigned identity) : f(fixture), cell(sample)
    {
        const unsigned width=cell.width,height=cell.height;
        const float near=.5f,far=64.f;
        base.projection[0]=base.projection[5]=1.f;
        base.projection[11]=-1.f;
        if (cell.reverse)
        {
            base.projection[10]=cell.infinite?0.f:near/(far-near);
            base.projection[14]=cell.infinite?near:far*near/(far-near);
        }
        else
        {
            base.projection[10]=cell.infinite?-1.f:-(far+near)/(far-near);
            base.projection[14]=cell.infinite?-2.f*near:-2.f*far*near/(far-near);
        }
        auto& constants=base.constants;
        constants.inverse_projection[0]=constants.inverse_projection[5]=1.f;
        constants.inverse_projection[14]=-1.f;
        constants.inverse_projection[11]=1.f/base.projection[14];
        constants.inverse_projection[15]=base.projection[10]/base.projection[14];
        constants.config[0]=cell.reverse;
        constants.light_direction[2]=1.f;
        constants.lighting[0]=constants.lighting[1]=1.f;
        constants.surface[1]=1.f; // rgba16_unorm normal packing
        for (unsigned c=0;c<3;++c)
        {
            constants.atmos_amblit[c]=.10f+.02f*c;
            constants.atmos_sunlit[c]=constants.atmos_sunlit_linear[c]=.30f;
        }
        base.projection_valid=true;
        base.hdr=cell.hdr;
        base.carry_depth=base.borrow_depth=base.depth_stencil_shader_read=true;
        base.diagnostic_view.view_kind=cell.probe?3:1;
        std::copy_n(base.projection,16,base.diagnostic_view.projection);

        ALMetalRenderTargetDescriptor target;
        target.width=width;target.height=height;target.color_attachment_count=4;
        target.colors[0].format=ALMetalPixelFormat::rgba8_unorm_srgb;
        target.colors[1].format=ALMetalPixelFormat::rgba8_unorm;
        target.colors[2].format=ALMetalPixelFormat::rgba16_unorm;
        target.colors[3].format=cell.hdr?ALMetalPixelFormat::rg11b10_float:ALMetalPixelFormat::rgba8_unorm;
        target.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
        target.depth_stencil_shader_read=true;
        target.label="Candidate full-lighting finite receiver plane";
        for (unsigned slot=0;slot<4;++slot)
        {
            ALMetalTextureDescriptor d;
            d.width=width;d.height=height;d.format=target.colors[slot].format;
            d.usage=ALMetalTextureUsage::shader_read|ALMetalTextureUsage::render_target;
            gbuffer_colors[slot]=ALMetalTexture::create(*f.device,d,&f.error); REQUIRE(gbuffer_colors[slot]);
            target.colors[slot].texture=gbuffer_colors[slot].get();
        }
        gbuffer=ALMetalRenderTarget::create(*f.device,target,&f.error);INFO(f.error);REQUIRE(gbuffer);
        const double ndc_z=(double(base.projection[10])*(-4.0)+base.projection[14])/4.0;
        ALMetalRenderPassConfig clear;
        clear.clear_depth=cell.reverse?ndc_z:(ndc_z+1.0)*.5;
        clear.clear_stencil=0x5a;
        clear.depth_store=clear.stencil_store=ALMetalStoreAction::store;
        auto pass=gbuffer->makeRenderPass(clear,&f.error); REQUIRE(pass);
        auto upload=f.device->makeCommandBuffer("Authored candidate lighting G-buffer/probe inputs",&f.error); REQUIRE(upload);
        auto encoder=ALMetalRenderEncoder::begin(*upload,*pass,"Finite receiver depth and stencil",&f.error); REQUIRE(encoder);
        REQUIRE(encoder->end(&f.error));
        const std::size_t pixels=std::size_t(width)*height;
        std::vector<std::uint8_t> albedo(pixels*4),specular(pixels*4);
        std::vector<std::uint16_t> normals(pixels*4);
        for (unsigned y=0;y<height;++y)for (unsigned x=0;x<width;++x)
        {
            const std::size_t offset=(std::size_t(y)*width+x)*4;
            albedo[offset]=std::uint8_t(96+x%32);albedo[offset+1]=144;albedo[offset+2]=184;
            albedo[offset+3]=cell.pbr?255:0; // legacy0 is shaded, not a fullbright early-out
            specular[offset]=cell.pbr?80:64;specular[offset+1]=cell.pbr?128:48;
            specular[offset+2]=cell.pbr?0:32;specular[offset+3]=cell.pbr?0:96;
            normals[offset]=normals[offset+1]=32768;
            normals[offset+2]=cell.pbr?65535:16384;
            normals[offset+3]=cell.pbr?43908:0;
        }
        REQUIRE(gbuffer_colors[0]->upload(*upload,0,0,{},albedo.data(),width*4,&f.error));
        REQUIRE(gbuffer_colors[1]->upload(*upload,0,0,{},specular.data(),width*4,&f.error));
        REQUIRE(gbuffer_colors[2]->upload(*upload,0,0,{},normals.data(),width*8,&f.error));
        ALMetalTextureDescriptor d;
        d.width=width;d.height=height;d.format=ALMetalPixelFormat::rg8_unorm;
        light_map=ALMetalTexture::create(*f.device,d,&f.error);REQUIRE(light_map);
        std::vector<std::uint8_t> white(pixels*2,255);
        REQUIRE(light_map->upload(*upload,0,0,{},white.data(),width*2,&f.error));
        d={};d.type=ALMetalTextureType::texture_cube_array;d.array_length=47;
        d.format=ALMetalPixelFormat::rgba8_unorm;d.usage=ALMetalTextureUsage::shader_read;
        radiance=ALMetalTexture::create(*f.device,d,&f.error);REQUIRE(radiance);
        d={};d.width=4;d.height=47;d.format=ALMetalPixelFormat::rgba32_uint;
        irradiance=ALMetalTexture::create(*f.device,d,&f.error);REQUIRE(irradiance);
        std::array<std::uint32_t,47*16> sh{};
        constexpr std::uint16_t palette[3][3]={{0x3800,0x3400,0x3000},
            {0x3000,0x3800,0x3400},{0x3400,0x3000,0x3800}};
        for (unsigned layer=0;layer<47;++layer)
        {
            const std::array<std::uint8_t,4> color{std::uint8_t(24+layer*7),
                std::uint8_t(176-layer*5),std::uint8_t(40+(layer%5)*24),255};
            for (unsigned face=0;face<6;++face)
                REQUIRE(radiance->upload(*upload,0,layer*6+face,{},color.data(),4,&f.error));
            // Real packed 27-half SH ABI, with distinct authored layer colors
            // and signed low bands. No private captured asset data is embedded.
            std::array<std::uint16_t,32> halves{};
            std::copy_n(palette[layer%3],3,halves.begin());
            halves[6]=0x2400;halves[7]=0xa000;halves[8]=0x2000;
            for (unsigned word=0;word<16;++word)
                sh[layer*16+word]=std::uint32_t(halves[word*2])|(std::uint32_t(halves[word*2+1])<<16);
        }
        REQUIRE(irradiance->upload(*upload,0,0,{},sh.data(),64,&f.error));
        REQUIRE(upload->commitAndWait(&f.error));
        probes.radiance_texture=radiance.get();probes.irradiance_texture=irradiance.get();
        auto& pack=probes.constants;pack.config[0]=47.f;
        for (unsigned i=0;i<47;++i)
        {
            pack.ref_sphere[i][0]=i?(-3.f+1.f*((i-1)%7)):0.f;
            pack.ref_sphere[i][1]=i?(-3.f+1.f*((i-1)/7)):0.f;
            pack.ref_sphere[i][2]=-4.f;pack.ref_sphere[i][3]=i?.85f:128.f;
            pack.ref_index[i][0]=i;pack.ref_index[i][1]=-1;
            pack.ref_params[i][0]=1.f;pack.ref_params[i][1]=1.f;
            pack.ref_params[i][2]=1.f;pack.ref_params[i][3]=0.f;
        }
        pack.ref_params[1][1]=.75f+.01f*identity; // distinct valid generation for every cell
        for (auto& bucket:pack.ref_bucket)bucket[0]=1;
        base.gbuffer=gbuffer.get();base.reflection_probes=&probes;
        base.light_map=cell.probe?nullptr:light_map.get();
        map.gbuffer=gbuffer.get();
        std::copy_n(constants.inverse_projection,16,map.constants.inverse_projection);
        map.constants.light_direction[2]=1.f;map.constants.screen[0]=float(width);map.constants.screen[1]=float(height);
        map.constants.config[0]=cell.reverse;map.constants.config[3]=1;
    }
    std::unique_ptr<ALMetalRenderTarget> render(bool prepared, ALMetalRasterTileSize tile=ALMetalRasterTileSize::automatic)
    {
        auto scene=base;scene.projection_valid=prepared;scene.lighting_raster_tile_size=tile;
        if (!cell.probe) return f.renderer->renderBaseLighting(scene,&f.error);
        ALMetalProbeLightingPath path=ALMetalProbeLightingPath::none;
        auto output=f.renderer->renderProbeLighting(map,scene,&f.error,&path);
        CHECK(path==ALMetalProbeLightingPath::memoryless_tile);
        return output;
    }
    void checkBounds()
    {
        const auto certificate=alMetalCertifyBaseProbeMapping(base.projection,
            base.constants.inverse_projection,cell.reverse,cell.width,cell.height);
        REQUIRE(certificate.certified);
        ALMetalProbeCandidateView view;
        std::copy_n(base.projection,16,view.projection);
        view.width=cell.width;view.height=cell.height;
        view.raster_mapping_certified=true;view.pixel_guard=certificate.pixel_guard;
        std::array<ALMetalProbeCandidateSphere,47> spheres{};
        for (unsigned i=0;i<47;++i)
            std::copy_n(probes.constants.ref_sphere[i],4,spheres[i].center_radius);
        const auto packet=alMetalBuildProbeCandidates(spheres,47.f,view);
        REQUIRE(packet.config[3]==1);
        unsigned bounded=0;
        for (unsigned i=1;i<47;++i)
        {
            const auto& r=packet.bounds[i];
            bounded+=r.x_min>0||r.y_min>0||r.x_max<cell.width||r.y_max<cell.height;
        }
        CHECK(bounded==46); // reject an all-full/fallback-only fixture
    }
};

void requirePositiveFinite(const std::vector<Pixel>& pixels)
{
    unsigned positive=0;
    for (const auto& pixel:pixels)
    {
        bool rgb=false;
        for (unsigned c=0;c<4;++c) REQUIRE((pixel[c]&0x7c00u)!=0x7c00u);
        for (unsigned c=0;c<3;++c)rgb|=(pixel[c]&0x7fffu)!=0;
        positive+=rgb;
    }
    CHECK(positive==pixels.size());
}
}

namespace lighting_tiles_forward
{
using Pixel = std::array<std::uint16_t,4>;
struct Planes
{
    std::vector<Pixel> color;
    std::vector<std::uint32_t> depth;
    std::vector<std::uint8_t> stencil;
    bool operator==(const Planes&) const = default;
};
void identity(float* value)
{
    std::fill_n(value,16,0.f);
    value[0]=value[5]=value[10]=value[15]=1.f;
}
struct Cell { bool skinned,shadow,reverse,infinite,hdr; unsigned width,height; };
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device=ALMetalDevice::create(&error);
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    Fixture()
    {
        REQUIRE([[NSBundle mainBundle] pathForResource:@"almetal_builtin" ofType:@"metallib"] != nil);
        INFO(error); REQUIRE(device);
        data=ALMetalWorldFrameData::create(*device,&error); REQUIRE(data);
        renderer=ALMetalWorldGBufferRenderer::create(*device,&error); INFO(error); REQUIRE(renderer);
        renderer->setFrameData(data);
        data->beginFrame(1);
    }
    ~Fixture() { if(device) device->waitUntilIdle(); }
    std::unique_ptr<ALMetalBuffer> buffer(const void* bytes,std::size_t length)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length=length; descriptor.storage=ALMetalStorageMode::shared;
        auto result=ALMetalBuffer::create(*device,descriptor,&error); REQUIRE(result);
        REQUIRE(result->write(0,bytes,length,&error)); return result;
    }
    Planes read(const ALMetalRenderTarget& target)
    {
        const auto width=target.descriptor().width,height=target.descriptor().height;
        REQUIRE(target.colorTexture(0)); REQUIRE(target.depthStencilTexture());
        REQUIRE(target.colorTexture(0)->descriptor().format==ALMetalPixelFormat::rgba16_float);
        Planes result;
        result.color.resize(std::size_t(width)*height);
        REQUIRE(target.colorTexture(0)->readback(*device,0,0,{},result.color.data(),width*8,&error));
        const std::size_t depth_row=(width*4u+255u)&~255u,stencil_row=(width+255u)&~255u;
        const auto stencil_offset=depth_row*height;
        auto command=device->makeCommandBuffer("Probe candidate alpha exact plane readback",&error); REQUIRE(command);
        id<MTLDevice> native=(__bridge id<MTLDevice>)device->nativeHandle();
        id<MTLBuffer> destination=[native newBufferWithLength:stencil_offset+stencil_row*height options:MTLResourceStorageModeShared]; REQUIRE(destination);
        id<MTLBlitCommandEncoder> encoder=[(__bridge id<MTLCommandBuffer>)command->nativeHandle() blitCommandEncoder]; REQUIRE(encoder);
        id<MTLTexture> source=(__bridge id<MTLTexture>)target.depthStencilTexture()->nativeHandle();
        [encoder copyFromTexture:source sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(width,height,1) toBuffer:destination destinationOffset:0
            destinationBytesPerRow:depth_row destinationBytesPerImage:depth_row*height options:MTLBlitOptionDepthFromDepthStencil];
        [encoder copyFromTexture:source sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(width,height,1) toBuffer:destination destinationOffset:stencil_offset
            destinationBytesPerRow:stencil_row destinationBytesPerImage:stencil_row*height options:MTLBlitOptionStencilFromDepthStencil];
        [encoder endEncoding]; REQUIRE(command->commitAndWait(&error));
        result.depth.resize(std::size_t(width)*height); result.stencil.resize(std::size_t(width)*height);
        const auto* bytes=static_cast<const std::uint8_t*>(destination.contents);
        for(unsigned y=0;y<height;++y)
        {
            std::memcpy(result.depth.data()+std::size_t(y)*width,bytes+y*depth_row,width*4);
            std::memcpy(result.stencil.data()+std::size_t(y)*width,bytes+stencil_offset+y*stencil_row,width);
        }
        return result;
    }
};
struct Inputs
{
    Fixture& f;
    Cell cell;
    std::unique_ptr<ALMetalBuffer> vertices,indices;
    std::array<std::unique_ptr<ALMetalTexture>,3> diffuse;
    std::array<std::unique_ptr<ALMetalTexture>,4> material_maps;
    std::unique_ptr<ALMetalTexture> radiance,irradiance,shadow;
    std::array<ALMetalWorldSimpleBatch,2> batches;
    std::array<float,16> palette{{0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0}};
    ALMetalWorldReflectionProbes probes;
    ALMetalWorldAlphaEnvironment environment;
    ALMetalWorldLegacyAlphaLighting lighting;
    ALMetalWorldDirectionalShadows shadows;
    ALMetalWorldOpaquePostDeferredScene scene;
    std::array<std::uint32_t,47*16> packedSH{};
    explicit Inputs(Fixture& fixture,Cell value):f(fixture),cell(value)
    {
        const std::array<std::array<float,4>,8> positions{{
            {{-3.4f,-2.6f,-4.f,0.f}},{{1.4f,-2.6f,-4.f,0.f}},
            {{1.4f,2.6f,-4.f,0.f}},{{-3.4f,2.6f,-4.f,0.f}},
            {{-1.4f,-2.3f,-3.75f,0.f}},{{3.4f,-2.3f,-3.75f,0.f}},
            {{3.4f,2.3f,-3.75f,0.f}},{{-1.4f,2.3f,-3.75f,0.f}}}};
        std::array<std::array<float,4>,8> encoded=positions,normals{},weights{},tangents{};
        std::array<std::array<float,2>,8> uv{};
        std::array<std::array<std::uint8_t,4>,8> colors{};
        for(unsigned i=0;i<8;++i)
        {
            encoded[i][3]=std::bit_cast<float>(std::uint32_t(i/4));
            normals[i]={{0,0,1,0}}; weights[i]={{.5f,0,0,0}};tangents[i]={{1,0,0,1}};
            uv[i]={{float(i&1u),float((i>>1u)&1u)}};
            colors[i]={{255,std::uint8_t(i<4?224:255),255,std::uint8_t(i<4?144:112)}};
        }
        constexpr std::size_t normal_offset=sizeof(encoded),uv_offset=normal_offset+sizeof(normals);
        constexpr std::size_t color_offset=uv_offset+sizeof(uv),tangent_offset=(color_offset+sizeof(colors)+15)&~std::size_t(15);
        constexpr std::size_t weight_offset=tangent_offset+sizeof(tangents);
        std::array<std::uint8_t,weight_offset+sizeof(weights)> bytes{};
        std::memcpy(bytes.data(),encoded.data(),sizeof(encoded));
        std::memcpy(bytes.data()+normal_offset,normals.data(),sizeof(normals));
        std::memcpy(bytes.data()+uv_offset,uv.data(),sizeof(uv));
        std::memcpy(bytes.data()+color_offset,colors.data(),sizeof(colors));
        std::memcpy(bytes.data()+weight_offset,weights.data(),sizeof(weights));
        std::memcpy(bytes.data()+tangent_offset,tangents.data(),sizeof(tangents));
        vertices=f.buffer(bytes.data(),bytes.size());
        const std::array<std::uint16_t,12> index{{0,1,2,0,2,3,4,5,6,4,6,7}};
        indices=f.buffer(index.data(),sizeof(index));
        const float near=.5f,far=64.f;
        auto* p=scene.forward_probe_projection;
        p[0]=p[5]=1.f; p[11]=-1.f;
        p[10]=cell.reverse?(cell.infinite?0.f:near/(far-near)):(cell.infinite?-1.f:-(far+near)/(far-near));
        p[14]=cell.reverse?(cell.infinite?near:far*near/(far-near)):(cell.infinite?-2.f*near:-2.f*far*near/(far-near));
        scene.forward_probe_projection_valid=true;
        for(unsigned i=0;i<2;++i)
        {
            auto& batch=batches[i];
            batch.vertices=vertices.get(); batch.indices=indices.get(); batch.vertex_count=8;
            batch.position_offset=0; batch.normal_offset=normal_offset; batch.texcoord_offset=uv_offset;
            batch.color_offset=color_offset;batch.tangent_offset=tangent_offset; batch.index_offset=i*6*sizeof(std::uint16_t); batch.index_count=6;
            batch.texture_count=2; batch.pre_water_alpha=true;
            batch.pre_water_simple=true; batch.pre_water_shadows=cell.shadow;
            identity(batch.modelview); std::copy_n(p,16,batch.modelview_projection);
            for(unsigned axis=0;axis<3;++axis)batch.normal_matrix[axis][axis]=batch.rigged_normal_matrix[axis][axis]=1.f;
            if(cell.skinned)
            {batch.weight_offset=weight_offset;batch.skin_palette=palette.data();batch.skin_palette_float_count=palette.size();batch.skin_joint_count=1;}
        }
        auto upload=f.device->makeCommandBuffer("Authored transparent probe candidate inputs",&f.error); REQUIRE(upload);
        ALMetalTextureDescriptor descriptor;
        descriptor.format=ALMetalPixelFormat::rgba8_unorm_srgb;
        const std::array<std::array<std::uint8_t,4>,3> diffuse_colors{{{{224,128,64,224}},{{80,184,240,208}},{{192,64,224,192}}}};
        for(unsigned i=0;i<3;++i)
        {
            diffuse[i]=ALMetalTexture::create(*f.device,descriptor,&f.error); REQUIRE(diffuse[i]);
            REQUIRE(diffuse[i]->upload(*upload,0,0,{},diffuse_colors[i].data(),4,&f.error));
            for(auto& batch:batches)batch.diffuse_textures[i]=diffuse[i].get();
        }
        descriptor={};descriptor.type=ALMetalTextureType::texture_cube_array;descriptor.array_length=47;
        descriptor.format=ALMetalPixelFormat::rgba8_unorm;
        radiance=ALMetalTexture::create(*f.device,descriptor,&f.error); REQUIRE(radiance);
        for(unsigned layer=0;layer<47;++layer)
        {
            const std::array<std::uint8_t,4> color{{std::uint8_t(32+(layer%7)*24),std::uint8_t(192-(layer%5)*24),std::uint8_t(48+(layer%6)*24),255}};
            for(unsigned face=0;face<6;++face)REQUIRE(radiance->upload(*upload,0,layer*6+face,{},color.data(),4,&f.error));
            constexpr std::uint16_t bands[3][3]={{0x3800,0x3400,0x3000},{0x3000,0x3800,0x3400},{0x3400,0x3000,0x3800}};
            std::array<std::uint16_t,32> halves{};std::copy_n(bands[layer%3],3,halves.begin());
            halves[6]=0x2400;halves[7]=0xa000;halves[8]=0x2000;
            for(unsigned word=0;word<16;++word)packedSH[layer*16+word]=std::uint32_t(halves[word*2])|(std::uint32_t(halves[word*2+1])<<16);
        }
        descriptor={};descriptor.format=ALMetalPixelFormat::rgba32_uint;descriptor.width=4;descriptor.height=47;
        irradiance=ALMetalTexture::create(*f.device,descriptor,&f.error);REQUIRE(irradiance);
        REQUIRE(irradiance->upload(*upload,0,0,{},packedSH.data(),64,&f.error));
        descriptor={};descriptor.type=ALMetalTextureType::texture_2d_array;descriptor.array_length=4;
        descriptor.width=descriptor.height=8;descriptor.format=ALMetalPixelFormat::r32_float;
        shadow=ALMetalTexture::create(*f.device,descriptor,&f.error); REQUIRE(shadow);
        std::array<float,64> values{};
        for(unsigned cascade=0;cascade<4;++cascade)
        {
            for(unsigned i=0;i<values.size();++i)values[i]=cascade==0?((i+i/8)&1u?.25f:1.f):1.f;
            REQUIRE(shadow->upload(*upload,0,cascade,{},values.data(),8*sizeof(float),&f.error));
            auto* matrix=shadows.constants.shadow_matrix[cascade];matrix[12]=matrix[13]=.53f;matrix[14]=.5f;matrix[15]=1.f;
            shadows.constants.shadow_clip[cascade]=10.f+8.f*cascade;
        }
        REQUIRE(upload->commitAndWait(&f.error));
        shadows.depth_texture=shadow.get();shadows.constants.config[0]=2;shadows.constants.config[1]=0xe;
        shadows.constants.config[3]=2; // Production High2; ordinary runtime shadow path, not probe FC0.
        shadows.constants.resolution[0]=shadows.constants.resolution[1]=8.f;
        shadows.constants.resolution[2]=float(cell.width);shadows.constants.resolution[3]=float(cell.height);
        shadows.constants.parameters[2]=200.f;shadows.constants.parameters[3]=32.f;
        probes.radiance_texture=radiance.get();probes.irradiance_texture=irradiance.get();
        spatial();
        environment.water_plane[2]=1.f;environment.water_plane[3]=-100.f;environment.water_fog[0]=1.f;environment.water_fog[2]=-1.f;
        environment.eye_to_sky[0][0]=environment.eye_to_sky[1][1]=environment.eye_to_sky[2][2]=1.f;
        environment.glow_max_y[0]=1.f;environment.glow_max_y[2]=-1.f;environment.glow_max_y[3]=1.f;
        environment.light_direction[2]=1.f;
        environment.atmospheric[1]=environment.atmospheric[2]=environment.atmospheric[3]=1.f;
        environment.atmos_sunlit[3]=environment.atmos_sunlit_linear[3]=1.f;
        for(unsigned c=0;c<3;++c){environment.atmos_sunlit_linear[c]=.20f+.05f*c;environment.atmos_sunlit[c]=.25f;environment.atmos_amblit[c]=.1f;}
        environment.environment[0]=1.f;
        lighting.sun_direction[2]=1.f;lighting.parameters[0]=128.f;lighting.parameters[1]=.004f;lighting.parameters[2]=1.f;
        scene.width=cell.width;scene.height=cell.height;scene.reverse_z=cell.reverse;scene.hdr=cell.hdr;
        scene.batches=batches.data();scene.batch_count=batches.size();scene.reflection_probes=&probes;
        scene.alpha_environment=&environment;scene.alpha_lighting=&lighting;scene.alpha_directional_shadows=cell.shadow?&shadows:nullptr;
    }
    void family(unsigned kind)
    {
        auto upload=f.device->makeCommandBuffer("Authored full production alpha material maps",&f.error); REQUIRE(upload);
        const std::array<std::array<std::uint8_t,4>,4> values{{
            {{128,128,255,255}},{{128,96,64,192}},{{255,128,64,255}},{{16,24,32,255}}}};
        for(unsigned i=0;i<material_maps.size();++i)
        {
            ALMetalTextureDescriptor d;d.format=ALMetalPixelFormat::rgba8_unorm;
            material_maps[i]=ALMetalTexture::create(*f.device,d,&f.error);REQUIRE(material_maps[i]);
            REQUIRE(material_maps[i]->upload(*upload,0,0,{},values[i].data(),4,&f.error));
        }
        REQUIRE(upload->commitAndWait(&f.error));
        for(auto& batch:batches)
        {
            batch.pre_water_simple=kind==0;batch.pre_water_material=kind==1;batch.pre_water_pbr=kind==2;
            batch.material_slot_count=kind==1?2:0;batch.pbr_slot_count=kind==2?2:0;
            batch.material_alpha_mode = kind == 1 ? 1u : 0u;
            batch.texcoord1_offset = batch.texcoord_offset;
            batch.texcoord2_offset = batch.texcoord_offset;
            for(unsigned slot=0;slot<2;++slot)
            {
                if(kind==1)
                {
                    auto& material=batch.materialSlots()[slot];
                    material.normal_texture=material_maps[0].get();material.specular_texture=material_maps[1].get();
                    material.alpha_cutoff=-1.f;material.env_intensity=.35f;material.specular_color[3]=.6f;
                }
                if(kind==2)
                {
                    auto& material=batch.pbrSlots()[slot];
                    material.normal_texture=material_maps[0].get();material.orm_texture=material_maps[2].get();
                    material.emissive_texture=material_maps[3].get();material.emissive_color[0]=.05f;
                    material.roughness_factor=.6f;material.metallic_factor=.25f;material.alpha_cutoff=-1.f;
                }
            }
        }
    }
    std::unique_ptr<ALMetalRenderTarget> encode(const ALMetalRenderTarget& gbuffer,
        const ALMetalRenderTarget& composed, bool prepared, ALMetalRasterTileSize tile,
        const std::vector<ALMetalWorldSimpleBatch>* ordered=nullptr)
    {
        auto request=scene;request.gbuffer=&gbuffer;request.composed_lighting=&composed;
        request.forward_probe_projection_valid=prepared;request.lighting_raster_tile_size=tile;
        if(ordered){request.batches=ordered->data();request.batch_count=ordered->size();}
        return f.renderer->renderOpaquePostDeferred(request,&f.error);
    }
    void spatial()
    {
        probes.constants={}; auto& p=probes.constants;p.config[0]=47;
        for(unsigned i=0;i<47;++i)
        {
            p.ref_sphere[i][0]=i?(-3.f+1.f*((i-1)%7)):0.f;
            p.ref_sphere[i][1]=i?(-3.f+1.f*((i-1)/7)):0.f;
            p.ref_sphere[i][2]=-4.f;p.ref_sphere[i][3]=i?.85f:128.f;
            p.ref_index[i][0]=i;p.ref_index[i][1]=-1;
            p.ref_params[i][0]=p.ref_params[i][1]=p.ref_params[i][2]=1.f;
        }
        for(auto& bucket:p.ref_bucket)bucket[0]=1;
    }
    void postWaterOnly()
    {
        for(auto& batch:batches)
        {
            batch.pre_water_alpha=false;
            batch.post_water_alpha=true;
        }
        // The renderer reverses this sign for the post-water camera-side
        // sweep. Keep these authored receivers on its visible side.
        environment.water_fog[2]=1.f;
    }
    std::unique_ptr<ALMetalRenderTarget> seeded(bool gbuffer)
    {
        ALMetalRenderTargetDescriptor descriptor;descriptor.width=cell.width;descriptor.height=cell.height;
        descriptor.color_attachment_count=gbuffer?4:1;descriptor.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
        for(unsigned i=0;i<descriptor.color_attachment_count;++i)descriptor.colors[i].format=ALMetalPixelFormat::rgba16_float;
        auto result=ALMetalRenderTarget::create(*f.device,descriptor,&f.error);REQUIRE(result);
        ALMetalRenderPassConfig config;config.clear_depth=cell.reverse?0.:1.;config.clear_stencil=0x5a;
        config.depth_store=config.stencil_store=ALMetalStoreAction::store;
        for(unsigned i=0;i<descriptor.color_attachment_count;++i)config.colors[i].clear={.03125,.0625,.125,.5};
        auto pass=result->makeRenderPass(config,&f.error);REQUIRE(pass);
        auto command=f.device->makeCommandBuffer("Independent seeded alpha color/depth/stencil",&f.error);REQUIRE(command);
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Seed alpha inputs",&f.error);REQUIRE(encoder);
        REQUIRE(encoder->end(&f.error));REQUIRE(command->commitAndWait(&f.error));return result;
    }
    struct Rendered { Planes planes;ALMetalWorldPostDiagnostics diagnostics; };
    Rendered render(bool prepared, ALMetalRasterTileSize tile=ALMetalRasterTileSize::automatic)
    {
        // Each arm starts from independently owned attachments. No stale blended
        // destination/depth/stencil from a preceding arm can hide a discrepancy.
        auto gbuffer=seeded(true),composed=seeded(false);auto request=scene;
        request.gbuffer=gbuffer.get();request.composed_lighting=composed.get();request.forward_probe_projection_valid=prepared;request.lighting_raster_tile_size=tile;
        auto result=f.renderer->renderOpaquePostDeferred(request,&f.error);INFO(f.error);REQUIRE(result);
        const auto retained=f.read(*gbuffer);
        CHECK(std::all_of(retained.stencil.begin(),retained.stencil.end(),[](auto value){return value==0x5a;}));
        const auto far=std::bit_cast<std::uint32_t>(cell.reverse?0.f:1.f);
        CHECK(std::all_of(retained.depth.begin(),retained.depth.end(),[far](auto value){return value==far;}));
        return {f.read(*result),f.renderer->opaquePostDiagnostics()};
    }
};
void positive(const Planes& planes,bool writes_depth=true,bool reverse=false)
{
    const Pixel seed{{0x2800,0x2c00,0x3000,0x3800}};
    std::size_t changed=0,positive_changed=0;
    for(const auto& pixel:planes.color)
    {
        for(auto value:pixel)REQUIRE((value&0x7c00u)!=0x7c00u);
        changed+=pixel!=seed;
        bool positive_rgb=false;
        for(unsigned c=0;c<3;++c)positive_rgb|=(pixel[c]&0x8000u)==0&&(pixel[c]&0x7fffu)!=0;
        positive_changed+=pixel!=seed&&positive_rgb;
    }
    CHECK(changed>planes.color.size()/10);
    CHECK(positive_changed>planes.color.size()/10);
    // The existing opaque-post owner clears output stencil after copying the
    // retained input depth. The input's0x5a is checked separately above.
    CHECK(std::all_of(planes.stencil.begin(),planes.stencil.end(),[](auto value){return value==0;}));
    if(writes_depth)
        CHECK(std::any_of(planes.depth.begin(),planes.depth.end(),[first=planes.depth.front()](auto value){return value!=first;}));
    else
    {
        // Ordinary rigid post-water alpha tests depth without writing it.
        // Rigged alpha and the pre-water pool retain their depth-write owner.
        const auto far=std::bit_cast<std::uint32_t>(reverse?0.f:1.f);
        CHECK(std::all_of(planes.depth.begin(),planes.depth.end(),[far](auto value){return value==far;}));
    }
}
void encoded(const Inputs::Rendered& value,bool shadow,unsigned count)
{
    CHECK(value.diagnostics.probe_candidate_simple_batches==(shadow?0u:count));
    CHECK(value.diagnostics.probe_candidate_simple_shadow_batches==(shadow?count:0u));
}
}

namespace lighting_tiles_check
{
using Tile = ALMetalRasterTileSize;

void observed(const ALMetalWorldLightingTileStats& before,
              const ALMetalWorldLightingTileStats& after,
              Tile tile, bool base, std::uint64_t minimum = 1)
{
    const auto base_passes = after.base_passes - before.base_passes;
    const auto forward_passes = after.forward_passes - before.forward_passes;
    const auto total = base_passes + forward_passes;
    REQUIRE((base ? base_passes : forward_passes) >= minimum);
    CHECK((base ? forward_passes : base_passes) == 0);
    CHECK(after.tile16_passes - before.tile16_passes +
          after.tile32_passes - before.tile32_passes +
          after.other_passes - before.other_passes == total);
    if (tile == Tile::automatic)
    {
        CHECK(after.requested_tile16_passes == before.requested_tile16_passes);
        CHECK(after.effective_tile16_passes == before.effective_tile16_passes);
        CHECK((base ? after.last_base_requested_size : after.last_forward_requested_size) == 0);
        CHECK((base ? after.last_base_effective_size : after.last_forward_effective_size) == 0);
    }
    if (tile == Tile::tile16)
    {
        CHECK(after.requested_tile16_passes - before.requested_tile16_passes == total);
        CHECK(after.effective_tile16_passes - before.effective_tile16_passes == total);
        CHECK((base ? after.last_base_requested_size : after.last_forward_requested_size) == 16);
        CHECK((base ? after.last_base_effective_size : after.last_forward_effective_size) == 16);
        CHECK(after.tile16_passes - before.tile16_passes == total);
        CHECK((base ? after.last_base_width : after.last_forward_width) == 16);
        CHECK((base ? after.last_base_height : after.last_forward_height) == 16);
    }
}

bool enabled(const char* name)
{
    const char* value = std::getenv(name);
    return value && value[0] && value[0] != '0';
}

template<class Fixture>
void validationSentinel(Fixture& f, bool cost)
{
    // Same public factory and exact offline library as all world factories.
    // This is process/factory validation evidence, not exhaustive inspection
    // of every private world PSO. Keep it outside warm-up/timed observations.
    const auto library = f.renderer->shaderLibrary();
    REQUIRE(library);
    ALMetalRenderPipelineDescriptor descriptor;
    descriptor.library = library.get();
    descriptor.vertex_function = "almetal_resample_vertex";
    descriptor.fragment_function = "almetal_resample_fragment";
    descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
    auto sentinel = ALMetalRenderPipeline::create(*f.device, descriptor, &f.error);
    INFO(f.error); REQUIRE(sentinel);
    auto native = (__bridge id<MTLRenderPipelineState>)sentinel->nativeHandle();
    REQUIRE(native);
    std::cout << "world_raster_tiles_validation_sentinel actual=" << long(native.shaderValidation)
              << " cost=" << cost << " same_factory_default=1 same_offline_library=1 exhaustive_world_PSO_census=0\n";
    if (cost) REQUIRE(native.shaderValidation == MTLShaderValidationDisabled);
    else if (enabled("MTL_SHADER_VALIDATION"))
        REQUIRE(native.shaderValidation == MTLShaderValidationEnabled);
}

bool supported(ALMetalDevice& device)
{
    return [(__bridge id<MTLDevice>)device.nativeHandle() supportsFamily:MTLGPUFamilyApple4];
}
} // namespace lighting_tiles_check

TEST_CASE("Production BASE raster tiles preserve generic and prepared legacy/PBR lighting",
          "[almetal][world-lighting-raster-tiles]")
{
    @autoreleasepool
    {
        using namespace lighting_tiles_base;
        using lighting_tiles_check::Tile;
        Fixture f;
        if (!lighting_tiles_check::supported(*f.device)) SKIP("Explicit raster tile device unavailable");
        lighting_tiles_check::validationSentinel(f, false);
        REQUIRE(ALMetalWorldBaseLightingScene{}.lighting_raster_tile_size == Tile::automatic);
        constexpr std::array<Cell, 8> cells{{
            {false,false,false,false,false,96,64}, {true,false,false,true,false,97,65},
            {false,true,false,true,false,97,65}, {true,true,true,false,false,96,64},
            {false,false,true,true,false,96,64}, {true,false,true,false,false,97,65},
            {false,true,true,false,false,96,64}, {true,true,false,true,false,97,65}}};
        for (unsigned i = 0; i < cells.size(); ++i)
        {
            CAPTURE(i, cells[i].pbr, cells[i].reverse, cells[i].hdr, cells[i].infinite);
            f.data->beginFrame(i + 1);
            Inputs input(f, cells[i], i);
            input.checkBounds();
            const auto retained = f.depthStencil(*input.gbuffer->depthStencilTexture());
            for (bool prepared : {false, true})
            {
                CAPTURE(prepared);
                const auto before = f.renderer->lightingTileStats();
                const auto candidates_before = f.data->stats();
                auto automatic = input.render(prepared, Tile::automatic);
                INFO(f.error); REQUIRE(automatic);
                const auto after_automatic = f.renderer->lightingTileStats();
                lighting_tiles_check::observed(before, after_automatic, Tile::automatic, true);
                auto explicit16 = input.render(prepared, Tile::tile16);
                INFO(f.error); REQUIRE(explicit16);
                lighting_tiles_check::observed(after_automatic, f.renderer->lightingTileStats(), Tile::tile16, true);
                const auto candidates_after = f.data->stats();
                if (prepared)
                    CHECK(candidates_after.candidate_preparations + candidates_after.candidate_reuses >
                          candidates_before.candidate_preparations + candidates_before.candidate_reuses);
                else
                    CHECK(candidates_after.candidate_preparations + candidates_after.candidate_reuses ==
                          candidates_before.candidate_preparations + candidates_before.candidate_reuses);
                const auto expected = f.colors(*automatic);
                requirePositiveFinite(expected);
                CHECK(f.colors(*explicit16) == expected);
                const auto automatic_depth = f.depthStencil(*automatic->depthStencilTexture());
                const auto explicit_depth = f.depthStencil(*explicit16->depthStencilTexture());
                CHECK(explicit_depth.depth == automatic_depth.depth);
                CHECK(explicit_depth.stencil == automatic_depth.stencil);
                CHECK(explicit_depth.depth == retained.depth);
                CHECK(explicit_depth.stencil == retained.stencil);
            }
            const auto before_change = input.render(false, Tile::automatic);
            REQUIRE(before_change);
            const auto original_color = f.colors(*before_change);
            input.probes.constants.ref_sphere[1][0] = 1000.f;
            auto changed = input.render(false, Tile::automatic);
            auto changed16 = input.render(true, Tile::tile16);
            INFO(f.error); REQUIRE(changed); REQUIRE(changed16);
            CHECK(f.colors(*changed) != original_color);
            CHECK(f.colors(*changed16) == f.colors(*changed));
            const auto unchanged_input = f.depthStencil(*input.gbuffer->depthStencilTexture());
            CHECK(unchanged_input.depth == retained.depth);
            CHECK(unchanged_input.stencil == retained.stencil);
        }
    }
}

TEST_CASE("Production alpha raster tiles preserve prepared High2 and generic material/PBR paths",
          "[almetal][world-lighting-raster-tiles]")
{
    @autoreleasepool
    {
        using namespace lighting_tiles_forward;
        using lighting_tiles_check::Tile;
        Fixture f;
        if (!lighting_tiles_check::supported(*f.device)) SKIP("Explicit raster tile device unavailable");
        lighting_tiles_check::validationSentinel(f, false);
        REQUIRE(ALMetalWorldOpaquePostDeferredScene{}.lighting_raster_tile_size == Tile::automatic);
        constexpr std::array<Cell, 8> cells{{
            {false,false,false,false,false,96,64}, {false,true,true,false,true,97,65},
            {true,false,false,true,true,97,65}, {true,true,true,true,false,96,64},
            {false,false,true,true,true,97,65}, {false,true,false,true,false,96,64},
            {true,false,true,false,false,96,64}, {true,true,false,false,true,97,65}}};
        for (unsigned family = 0; family < 3; ++family)
        for (unsigned i = 0; i < (family == 0 ? 8u : 4u); ++i)
        {
            const bool post_water = (i & 1u) != 0;
            CAPTURE(family, i, post_water, cells[i].skinned, cells[i].shadow, cells[i].reverse);
            f.data->beginFrame(1 + family * 8 + i);
            Inputs input(f, cells[i]);
            if (family) input.family(family);
            if (post_water) input.postWaterOnly();
            for (bool prepared : {false, true})
            {
                CAPTURE(prepared);
                const auto before = f.renderer->lightingTileStats();
                const auto automatic = input.render(prepared, Tile::automatic);
                const auto after_automatic = f.renderer->lightingTileStats();
                lighting_tiles_check::observed(before, after_automatic, Tile::automatic, false);
                const auto explicit16 = input.render(prepared, Tile::tile16);
                lighting_tiles_check::observed(after_automatic, f.renderer->lightingTileStats(), Tile::tile16, false);
                positive(automatic.planes, !post_water || cells[i].skinned, cells[i].reverse);
                CHECK(explicit16.planes == automatic.planes);
                const unsigned candidate_draws = prepared && family == 0 ? 2 : 0;
                encoded(automatic, cells[i].shadow, candidate_draws);
                encoded(explicit16, cells[i].shadow, candidate_draws);
            }
            if (family == 0 && i == 0)
            {
                const auto original = input.render(true, Tile::automatic);
                input.probes.constants.ref_sphere[1][0] = 1000.f;
                const auto moved = input.render(true, Tile::automatic);
                CHECK(moved.planes.color != original.planes.color);
                CHECK(input.render(true, Tile::tile16).planes == moved.planes);
                std::swap(input.batches[0], input.batches[1]);
                const auto reversed = input.render(true, Tile::automatic);
                CHECK(reversed.planes.color != moved.planes.color);
                CHECK(input.render(true, Tile::tile16).planes == reversed.planes);
            }
            if (family == 0 && cells[i].skinned)
            {
                const auto original = input.render(true, Tile::automatic);
                input.palette[0] = .25f;
                const auto posed = input.render(true, Tile::automatic);
                CHECK(posed.planes.color != original.planes.color);
                CHECK(input.render(true, Tile::tile16).planes == posed.planes);
            }
        }
    }
}

TEST_CASE("Forward raster tile request survives the atmospheric encoder restart",
          "[almetal][world-lighting-raster-tiles]")
{
    @autoreleasepool
    {
        using namespace lighting_tiles_forward;
        using lighting_tiles_check::Tile;
        Fixture f;
        if (!lighting_tiles_check::supported(*f.device)) SKIP("Explicit raster tile device unavailable");
        lighting_tiles_check::validationSentinel(f, false);
        Inputs input(f, {false,true,false,false,true,97,65});
        ALMetalWorldAtmosphericHaze haze;
        haze.inverse_projection[0] = haze.inverse_projection[5] = 1.f;
        haze.inverse_projection[14] = -1.f;
        haze.inverse_projection[11] = 1.f / input.scene.forward_probe_projection[14];
        haze.inverse_projection[15] = input.scene.forward_probe_projection[10] /
                                      input.scene.forward_probe_projection[14];
        input.scene.atmospheric_haze = &haze;
        // Pre-water eye-Z=-4 and post-water eye-Z=-3.75 straddle this
        // plane. Initial sign-1 and the renderer's flipped post sign+1
        // therefore preserve both contributions (PPD403).
        input.environment.water_plane[3] = 3.875f;
        std::array<ALMetalWorldSimpleBatch, 3> ordered{input.batches[0], {}, input.batches[1]};
        ordered[1].atmospheric_haze = true;
        ordered[2].pre_water_alpha = false;
        ordered[2].post_water_alpha = true;
        input.scene.batches = ordered.data();
        input.scene.batch_count = ordered.size();
        // This checks physical pass re-entry and retained alpha output, not
        // a new analytic oracle for the independently tested haze equations.
        const auto before = f.renderer->lightingTileStats();
        const auto automatic = input.render(true, Tile::automatic);
        const auto after = f.renderer->lightingTileStats();
        lighting_tiles_check::observed(before, after, Tile::automatic, false, 2);
        const auto explicit16 = input.render(true, Tile::tile16);
        lighting_tiles_check::observed(after, f.renderer->lightingTileStats(), Tile::tile16, false, 2);
        positive(automatic.planes);
        CHECK(explicit16.planes == automatic.planes);
        input.scene.batch_count = 2; // Keep the same pre draw and haze marker.
        const auto no_post = input.render(true, Tile::automatic);
        CHECK(no_post.planes.color != automatic.planes.color);
        input.scene.batch_count = ordered.size();
    }
}

namespace lighting_tiles_cost
{
using Clock = std::chrono::steady_clock;
using Tile = ALMetalRasterTileSize;
struct Observation
{
    bool ok = false;
    std::string error;
    ALMetalCommandGPUTime gpu;
    std::uint64_t owner_wall_ns = 0;
    ALMetalWorldLightingTileStats before, after;
    std::uint64_t candidate_preparations = 0, candidate_reuses = 0;
};

template<class Fixture, class Encode>
Observation measure(Fixture& f, unsigned frame, Tile tile, Encode&& encode)
{
    Observation result;
    result.before = f.renderer->lightingTileStats();
    const auto candidates_before = f.data->stats();
    const auto started = Clock::now();
    f.data->beginFrame(frame);
    if (!f.renderer->beginCommandBatch("Actual world raster tile complete owner cost", &result.error))
        return result;
    auto output = encode(tile);
    auto completion = f.renderer->commandBatchCompletion();
    if (!output || !completion || !f.renderer->commitCommandBatch(&result.error))
    {
        f.renderer->cancelCommandBatch();
        return result;
    }
    if (!f.device->waitUntilIdle(&result.error) || !completion->isComplete() ||
        completion->gpuTime(result.gpu) != ALMetalCommandTimingStatus::ready)
    {
        if (result.error.empty()) result.error = "Actual world cost lacks completed GPU timestamps";
        return result;
    }
    // Include target/receipt retirement after the exact consuming command.
    output.reset();
    completion.reset();
    result.owner_wall_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - started).count();
    result.after = f.renderer->lightingTileStats();
    const auto candidates_after = f.data->stats();
    result.candidate_preparations = candidates_after.candidate_preparations - candidates_before.candidate_preparations;
    result.candidate_reuses = candidates_after.candidate_reuses - candidates_before.candidate_reuses;
    result.ok = true;
    return result;
}

void heavyOverlap(ALMetalWorldReflectionProbeConstants& probes)
{
    REQUIRE(probes.config[0] == 47.f);
    for (unsigned i = 0; i < 47; ++i)
    {
        REQUIRE(probes.ref_params[i][0] == 1.f);
        REQUIRE(probes.ref_index[i][1] == -1);
        REQUIRE(probes.ref_index[i][3] == 0);
    }
    // Authored stress, not a captured Home distribution. These31 volumes
    // contain all authored BASE and seven-layer alpha receivers. Either one
    // of the remaining15 bounded locals fills the unchanged32 cap, or the
    // original automatic default fallback adds the32nd semantic SH entry.
    // Retain every other pack field, layer, binding, mask and local32..46.
    for (unsigned i = 1; i <= 31; ++i)
    {
        probes.ref_sphere[i][0] = probes.ref_sphere[i][1] = 0.f;
        probes.ref_sphere[i][2] = -4.f;
        probes.ref_sphere[i][3] = 6.f;
    }
}

template<class Fixture, class Encode>
void run(Fixture& f, unsigned cell, bool base, bool prepared, unsigned configured_probes, bool heavy_overlap, Encode&& encode)
{
    constexpr unsigned warmups = 10, blocks = 150;
    unsigned frame = 1;
    // Output/depth/stencil parity is a separate actual-production-renderer
    // correctness selector; each cost cell must also pass the immediate pair
    // supplied by its caller before reaching this function.
    for (unsigned warm = 0; warm < warmups; ++warm)
    for (unsigned arm = 0; arm < 2; ++arm)
    {
        const auto result = measure(f, frame++, arm ? Tile::tile16 : Tile::automatic, encode);
        INFO(result.error); REQUIRE(result.ok);
    }
    double last_end = 0;
    std::cout << "world_raster_tiles_csv,cell,block,slot,arm,order,gpu_start_s,gpu_end_s,gpu_ns,owner_wall_ns,"
        "base_passes,forward_passes,requested16,effective16,actual16,actual32,actual_other,last_width,last_height,prepared,configured_probes,authored_heavy_overlap,candidate_preparations,candidate_reuses\n";
    std::cout << std::setprecision(17);
    for (unsigned block = 0; block < blocks; ++block)
    {
        const std::array<unsigned,4> order = block & 1u
            ? std::array<unsigned,4>{1,0,0,1} : std::array<unsigned,4>{0,1,1,0};
        for (unsigned slot = 0; slot < order.size(); ++slot)
        {
            const unsigned arm = order[slot];
            const Tile tile = arm ? Tile::tile16 : Tile::automatic;
            const auto result = measure(f, frame++, tile, encode);
            INFO(result.error); REQUIRE(result.ok);
            REQUIRE(result.gpu.start_seconds >= last_end);
            REQUIRE(result.gpu.end_seconds > result.gpu.start_seconds);
            last_end = result.gpu.end_seconds;
            lighting_tiles_check::observed(result.before, result.after, tile, base);
            if (prepared) REQUIRE(result.candidate_preparations + result.candidate_reuses > 0);
            else REQUIRE(result.candidate_preparations + result.candidate_reuses == 0);
            const auto& a = result.before; const auto& b = result.after;
            std::cout << "world_raster_tiles_csv," << cell << ',' << block << ',' << slot << ',' << arm << ','
                << (block & 1u ? "BAAB" : "ABBA") << ',' << result.gpu.start_seconds << ','
                << result.gpu.end_seconds << ',' << result.gpu.elapsed_nanoseconds << ',' << result.owner_wall_ns << ','
                << b.base_passes-a.base_passes << ',' << b.forward_passes-a.forward_passes << ','
                << b.requested_tile16_passes-a.requested_tile16_passes << ','
                << b.effective_tile16_passes-a.effective_tile16_passes << ','
                << b.tile16_passes-a.tile16_passes << ',' << b.tile32_passes-a.tile32_passes << ','
                << b.other_passes-a.other_passes << ',' << (base ? b.last_base_width : b.last_forward_width) << ','
                << (base ? b.last_base_height : b.last_forward_height) << ',' << prepared << ',' << configured_probes << ',' << heavy_overlap << ','
                << result.candidate_preparations << ',' << result.candidate_reuses << '\n';
        }
    }
    std::cout.flush();
}
} // namespace lighting_tiles_cost

TEST_CASE("Actual BASE and ordered-alpha raster tile complete owner cost",
          "[.][almetal][world-lighting-raster-tiles-cost]")
{
    if (!lighting_tiles_check::enabled("ALCHEMY_WORLD_LIGHTING_RASTER_TILES_COST"))
        SKIP("Explicit source-qualified world raster tile cost opt-in is required");
#ifndef NDEBUG
    SKIP("World raster tile costs require an optimized build");
#endif
    REQUIRE_FALSE(lighting_tiles_check::enabled("MTL_DEBUG_LAYER"));
    REQUIRE_FALSE(lighting_tiles_check::enabled("MTL_SHADER_VALIDATION"));
    REQUIRE_FALSE(lighting_tiles_check::enabled("MTL_CAPTURE_ENABLED"));
    REQUIRE_FALSE(lighting_tiles_check::enabled("MTL_HUD_ENABLED"));
    REQUIRE_FALSE(ALMetalDevice::commandTimingEnabled());
    // One explicitly selected cell per process.6/7 are optional one-probe
    // early-state negatives;8..10 are authored heavy overlap. None run as
    // an automatic matrix.
    unsigned cell = 0;
    if (const char* text = std::getenv("ALCHEMY_WORLD_LIGHTING_RASTER_TILES_COST_CELL"))
    {
        std::size_t used = 0;
        const auto parsed = std::stoul(text, &used);
        REQUIRE(used == std::strlen(text)); REQUIRE(parsed < 11);
        cell = unsigned(parsed);
    }
    std::cout << "world_raster_tiles_scope cell=" << cell
        << " prepared=" << (cell < 4 || cell >= 8)
        << " configured_probes=" << (cell == 6 || cell == 7 ? 1 : 47)
        << " authored_heavy_overlap=" << (cell >= 8)
        << " authored_stress_semantic_SH_entries_per_covered_fragment=" << (cell >= 8 ? "32" : "not_applicable")
        << " actual_Home_distribution_claim=0"
        << " dimensions=1024x576 actual_production_fragments=1 shared_current_library=1"
        << " includes=frame_arena_target_acquisition_current_constant_resource_preparation_actual_world_passes_encode_commit_GPU_completion_output_receipt_retirement"
        << " excludes=device_renderer_PSO_creation_fixed_authored_Gbuffer_textures_input_seeding_LL_scene_collection_culling_sorting"
        << " waitUntilIdle_barrier_in_wall=1 barrier_excluded_from_exact_command_GPU_interval=1"
        << " cold_source_publication_not_measured=1 no_FS_helper_or_tile_kernel=1 no_outlier_removal=1 FPS_claim=0\n";
    @autoreleasepool
    {
        using lighting_tiles_check::Tile;
        if (cell < 2 || (cell >= 4 && cell <= 6) || cell == 8 || cell == 9)
        {
            using namespace lighting_tiles_base;
            Fixture f; REQUIRE(lighting_tiles_check::supported(*f.device));
            lighting_tiles_check::validationSentinel(f, true);
            const bool prepared = cell < 2 || cell >= 8;
            const bool pbr = cell == 1 || cell == 5 || cell == 9;
            Inputs input(f, {pbr,false,false,true,false,1024,576}, 0);
            if (cell == 6) input.probes.constants.config[0] = 1.f;
            if (cell >= 8) lighting_tiles_cost::heavyOverlap(input.probes.constants);
            auto automatic = input.render(prepared, Tile::automatic);
            auto explicit16 = input.render(prepared, Tile::tile16);
            INFO(f.error); REQUIRE(automatic); REQUIRE(explicit16);
            requirePositiveFinite(f.colors(*automatic));
            REQUIRE(f.colors(*automatic) == f.colors(*explicit16));
            const auto depth = f.depthStencil(*automatic->depthStencilTexture());
            const auto depth16 = f.depthStencil(*explicit16->depthStencilTexture());
            REQUIRE(depth.depth == depth16.depth); REQUIRE(depth.stencil == depth16.stencil);
            auto encode = [&](Tile tile) {
                auto request = input.base; request.projection_valid = prepared;
                request.lighting_raster_tile_size = tile;
                return f.renderer->renderBaseLighting(request, &f.error);
            };
            automatic.reset(); explicit16.reset();
            lighting_tiles_cost::run(f, cell, true, prepared, unsigned(input.probes.constants.config[0]), cell >= 8, encode);
        }
        else
        {
            using namespace lighting_tiles_forward;
            Fixture f; REQUIRE(lighting_tiles_check::supported(*f.device));
            lighting_tiles_check::validationSentinel(f, true);
            Inputs input(f, {false,true,false,false,true,1024,576});
            const bool prepared = cell < 4 || cell == 10;
            if (cell == 7) input.probes.constants.config[0] = 1.f;
            if (cell == 10) lighting_tiles_cost::heavyOverlap(input.probes.constants);
            input.postWaterOnly();
            // Fullscreen eye-space quads; preserve the packed integer texture
            // indices in position.w and every remaining authored stream.
            std::array<std::array<float,4>,8> positions{{
                {{-4.2f,-4.2f,-4,0}},{{4.2f,-4.2f,-4,0}},{{4.2f,4.2f,-4,0}},{{-4.2f,4.2f,-4,0}},
                {{-4.f,-4.f,-3.75f,0}},{{4.f,-4.f,-3.75f,0}},{{4.f,4.f,-3.75f,0}},{{-4.f,4.f,-3.75f,0}}}};
            for (unsigned i=0;i<positions.size();++i) positions[i][3]=std::bit_cast<float>(std::uint32_t(i/4));
            REQUIRE(input.vertices->write(0,positions.data(),sizeof(positions),&f.error));
            std::vector<ALMetalWorldSimpleBatch> ordered;
            const unsigned draws = cell == 3 ? 2 : 7;
            ordered.reserve(draws);
            for (unsigned i=0;i<draws;++i)
            {
                ordered.push_back(input.batches[i%2]);
                if (cell == 3)
                {
                    auto& batch=ordered.back();batch.modelview[0]=batch.modelview[5]=.15f;
                    for(unsigned column=0;column<4;++column)for(unsigned row=0;row<4;++row)
                    {
                        float value=0;
                        for(unsigned k=0;k<4;++k)value+=input.scene.forward_probe_projection[row+4*k]*batch.modelview[k+4*column];
                        batch.modelview_projection[row+4*column]=value;
                    }
                }
            }
            auto gbuffer=input.seeded(true), composed=input.seeded(false);
            auto encode = [&](Tile tile) { return input.encode(*gbuffer,*composed,prepared,tile,&ordered); };
            auto automatic=encode(Tile::automatic), explicit16=encode(Tile::tile16);
            INFO(f.error); REQUIRE(automatic); REQUIRE(explicit16);
            const auto expected=f.read(*automatic), actual=f.read(*explicit16);
            if (cell != 3) positive(expected,false,false);
            else
            {
                // Physical geometry occupies about2.5% of the target. Keep
                // the inherited10% full-geometry guard unchanged elsewhere.
                const Pixel seed{{0x2800,0x2c00,0x3000,0x3800}};
                std::size_t changed = 0;
                for (const auto& pixel : expected.color)
                {
                    for (auto value : pixel) REQUIRE((value & 0x7c00u) != 0x7c00u);
                    changed += pixel != seed;
                }
                REQUIRE(changed > expected.color.size()/100);
                REQUIRE(changed < expected.color.size()/10);
                REQUIRE(std::all_of(expected.depth.begin(),expected.depth.end(),[](auto value){return value==0x3f800000u;}));
                REQUIRE(std::all_of(expected.stencil.begin(),expected.stencil.end(),[](auto value){return value==0;}));
            }
            REQUIRE(actual==expected);
            const auto diagnostics=f.renderer->opaquePostDiagnostics();
            REQUIRE(diagnostics.probe_candidate_simple_shadow_batches==(prepared?draws:0));
            REQUIRE(diagnostics.probe_candidate_simple_batches==0);
            automatic.reset(); explicit16.reset();
            lighting_tiles_cost::run(f,cell,false,prepared,unsigned(input.probes.constants.config[0]),cell == 10,encode);
        }
    }
}

namespace lighting_tiles_water
{
constexpr unsigned SIDE = 32;
using Pixel = std::array<std::uint16_t, 4>;
using Image = std::vector<Pixel>;
const std::array<float, 4> NORMAL_A{.75f, .5f, 1.f, 1.f};
const std::array<float, 4> NORMAL_B{.5f, .75f, 1.f, 1.f};
Pixel half(const std::array<float, 4>& source)
{
    Pixel value;
    for (unsigned c = 0; c < 4; ++c)
    {
        const __fp16 h = source[c];
        std::memcpy(&value[c], &h, sizeof(h));
    }
    return value;
}
void identity(float* matrix)
{
    for (unsigned i = 0; i < 16; ++i) matrix[i] = i % 5 == 0 ? 1.f : 0.f;
}
struct DepthStencil {std::vector<std::uint32_t> depth;std::vector<std::uint8_t> stencil;};
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::shared_ptr<ALMetalWorldFrameData> frames;
    std::unique_ptr<ALMetalTexture> cells;
    std::unique_ptr<ALMetalRenderTarget> lighting;
    Image background;
    Fixture()
    {
        REQUIRE([[NSBundle mainBundle] pathForResource:@"almetal_builtin" ofType:@"metallib"] != nil);
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error); INFO(error); REQUIRE(renderer);
        frames = ALMetalWorldFrameData::create(*device, &error); REQUIRE(frames);
        frames->beginFrame(1); renderer->setFrameData(frames);
        ALMetalTextureDescriptor cell;
        cell.width = cell.height = 1; cell.format = ALMetalPixelFormat::rg32_float;
        cells = ALMetalTexture::create(*device, cell, &error); REQUIRE(cells);
        const std::array<float, 2> height{0.f, 1.f};
        auto command = device->makeCommandBuffer("Water authored height", &error); REQUIRE(command);
        REQUIRE(cells->upload(*command, 0, 0, {}, height.data(), sizeof(height), &error));
        REQUIRE(command->commitAndWait(&error));
        ALMetalRenderTargetDescriptor target;
        target.width = target.height = SIDE;
        target.colors[0].format = ALMetalPixelFormat::rgba16_float;
        lighting = ALMetalRenderTarget::create(*device, target, &error); REQUIRE(lighting);
        background.resize(SIDE * SIDE);
        for (unsigned y = 0; y < SIDE; ++y) for (unsigned x = 0; x < SIDE; ++x)
            background[y * SIDE + x] = half({float(x) / (SIDE - 1), float(y) / (SIDE - 1), .25f, .75f});
        ALMetalTextureDescriptor texture;
        texture.width = texture.height = SIDE; texture.format = ALMetalPixelFormat::rgba16_float;
        auto upload = ALMetalTexture::create(*device, texture, &error); REQUIRE(upload);
        command = device->makeCommandBuffer("Water authored refraction gradient", &error); REQUIRE(command);
        REQUIRE(upload->upload(*command, 0, 0, {}, background.data(), SIDE * sizeof(Pixel), &error));
        REQUIRE(lighting->copyColorFrom(*command, 0, *upload, {}, &error));
        REQUIRE(command->commitAndWait(&error));
    }
    ~Fixture() { if (device) device->waitUntilIdle(); }
    std::unique_ptr<ALMetalRenderTarget> gbuffer(bool hdr, bool reverse)
    {
        ALMetalRenderTargetDescriptor target;
        target.width = target.height = SIDE; target.color_attachment_count = 4;
        target.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
        target.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
        target.colors[2].format = ALMetalPixelFormat::rgba16_unorm;
        target.colors[3].format = hdr ? ALMetalPixelFormat::rg11b10_float : ALMetalPixelFormat::rgba8_unorm;
        target.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        target.depth_stencil_shader_read = true;
        auto value = ALMetalRenderTarget::create(*device, target, &error); REQUIRE(value);
        auto command = device->makeCommandBuffer("Water authored far Gbuffer", &error); REQUIRE(command);
        ALMetalRenderPassConfig config; config.clear_depth = reverse ? 0.0 : 1.0;
        config.depth_store = config.stencil_store = ALMetalStoreAction::store;
        auto pass = value->makeRenderPass(config, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Water far clear", &error); REQUIRE(encoder);
        REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        return value;
    }
    std::unique_ptr<ALMetalTexture> normals(unsigned levels, bool alternate_mips, bool all_b = false, bool checker = false)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.width = descriptor.height = 8; descriptor.mipmap_level_count = levels;
        descriptor.format = ALMetalPixelFormat::rgba16_float;
        auto value = ALMetalTexture::create(*device, descriptor, &error); REQUIRE(value);
        auto command = device->makeCommandBuffer("Water authored normal levels", &error); REQUIRE(command);
        for (unsigned level = 0; level < levels; ++level)
        {
            const unsigned side = 8u >> level;
            Image pixels(side * side, half(all_b || (alternate_mips && level != 0) ? NORMAL_B : NORMAL_A));
            if (checker) for (unsigned y = 0; y < side; ++y) for (unsigned x = 0; x < side; ++x)
                pixels[y * side + x] = half((x + y) % 2 ? NORMAL_A : NORMAL_B);
            REQUIRE(value->upload(*command, level, 0, {}, pixels.data(), side * sizeof(Pixel), &error));
        }
        REQUIRE(command->commitAndWait(&error));
        return value;
    }
    Image read(const ALMetalRenderTarget& value)
    {
        REQUIRE(value.descriptor().width == SIDE); REQUIRE(value.descriptor().height == SIDE);
        REQUIRE(value.descriptor().colors[0].format == ALMetalPixelFormat::rgba16_float);
        Image pixels(SIDE * SIDE);
        REQUIRE(value.colorTexture(0)->readback(*device, 0, 0, {}, pixels.data(), SIDE * sizeof(Pixel), &error));
        return pixels;
    }
    std::vector<Image> readNormals(const ALMetalTexture& value)
    {
        std::vector<Image> levels;
        for (unsigned level = 0; level < value.descriptor().mipmap_level_count; ++level)
        {
            const unsigned side = value.descriptor().width >> level;
            levels.emplace_back(side * side);
            REQUIRE(value.readback(*device, level, 0, {}, levels.back().data(), side * sizeof(Pixel), &error));
        }
        return levels;
    }
    DepthStencil depthStencil(const ALMetalTexture& texture)
    {
        const auto width = texture.width(), height = texture.height();
        const auto depth_row = (width*4u+255u)&~255u;
        const auto stencil_row = (width+255u)&~255u;
        const std::size_t stencil_offset = std::size_t(depth_row)*height;
        auto command = device->makeCommandBuffer("Candidate lighting depth/stencil correctness readback",&error);
        REQUIRE(command);
        id<MTLDevice> native = (__bridge id<MTLDevice>)device->nativeHandle();
        id<MTLBuffer> staging = [native newBufferWithLength:stencil_offset+std::size_t(stencil_row)*height
            options:MTLResourceStorageModeShared]; REQUIRE(staging);
        id<MTLBlitCommandEncoder> blit = [(__bridge id<MTLCommandBuffer>)command->nativeHandle() blitCommandEncoder];
        REQUIRE(blit);
        id<MTLTexture> source = (__bridge id<MTLTexture>)texture.nativeHandle();
        [blit copyFromTexture:source sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(width,height,1) toBuffer:staging destinationOffset:0
            destinationBytesPerRow:depth_row destinationBytesPerImage:std::size_t(depth_row)*height
            options:MTLBlitOptionDepthFromDepthStencil];
        [blit copyFromTexture:source sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(width,height,1) toBuffer:staging destinationOffset:stencil_offset
            destinationBytesPerRow:stencil_row destinationBytesPerImage:std::size_t(stencil_row)*height
            options:MTLBlitOptionStencilFromDepthStencil];
        [blit endEncoding]; REQUIRE(command->commitAndWait(&error));
        DepthStencil result;
        result.depth.resize(std::size_t(width)*height); result.stencil.resize(std::size_t(width)*height);
        const auto* bytes = static_cast<const std::uint8_t*>(staging.contents);
        for (unsigned y=0;y<height;++y)
        {
            std::memcpy(result.depth.data()+std::size_t(y)*width,bytes+std::size_t(y)*depth_row,width*4);
            std::memcpy(result.stencil.data()+std::size_t(y)*width,
                bytes+stencil_offset+std::size_t(y)*stencil_row,width);
        }
        return result;
    }
    ALMetalWorldWaterSurface water(const ALMetalTexture& normal, bool reverse, float extent)
    {
        ALMetalWorldWaterSurface value;
        auto& field = value.field;
        field.cell_map = cells.get(); field.above_water = true;
        identity(field.modelview_projection); field.modelview_projection[0] = field.modelview_projection[5] = 1.f / extent;
        field.modelview_projection[14] = reverse ? .5f : 0.f;
        field.cell_square[0] = field.cell_square[1] = -extent; field.cell_square[2] = extent * 2; field.cell_square[3] = 1;
        field.rings[0] = extent; field.rings[1] = 1; field.rings[2] = 4;
        field.water_plane[2] = 1; field.water_plane[3] = -1; field.fog[0] = 1;
        field.draws[0] = {0u, 4u, 1u, true};
        field.draws[1] = {1u, 9u, 12u, false};
        field.draws[2] = {2u, 9u, 4u, false}; field.draw_count = 3;
        identity(value.modelview); value.modelview[14] = -1;
        value.normal_maps = {&normal, &normal}; value.eye_position[2] = -1;
        value.water_far[0] = 10000; value.parameters[1] = .1f; value.parameters[2] = 1;
        value.underwater = true; value.refracts = true;
        return value;
    }
    std::unique_ptr<ALMetalRenderTarget> render(const ALMetalRenderTarget& input, ALMetalWorldWaterSurface water, bool hdr, bool reverse, ALMetalRasterTileSize tile)
    {
        std::array<ALMetalWorldSimpleBatch, 2> markers;
        markers[0].water_haze = true; markers[1].water_surface = true;
        ALMetalWorldWaterHaze haze;
        identity(haze.inverse_projection);
        haze.water_plane[2] = 1.f; haze.water_plane[3] = -1.f;
        haze.fog[0] = 1.f; // Defined zero-density underwater pre-surface haze.
        ALMetalWorldOpaquePostDeferredScene scene;
        scene.gbuffer = &input; scene.composed_lighting = lighting.get(); scene.batches = markers.data(); scene.batch_count = markers.size();
        scene.width = scene.height = SIDE; scene.hdr = hdr; scene.reverse_z = reverse; scene.water_haze = &haze; scene.water_surface = &water;
        scene.lighting_raster_tile_size = tile;
        auto result = renderer->renderOpaquePostDeferred(scene, &error); INFO(error); REQUIRE(result);
        return result;
    }
};
}

TEST_CASE("Production water-haze and water-surface restarts retain requested raster tiles",
          "[almetal][world-lighting-raster-tiles]")
{
    @autoreleasepool
    {
        using namespace lighting_tiles_water;
        using lighting_tiles_check::Tile;
        Fixture f;
        if (!lighting_tiles_check::supported(*f.device)) SKIP("Explicit raster tile device unavailable");
        lighting_tiles_check::validationSentinel(f, false);
        auto normal_a = f.normals(4, false), normal_b = f.normals(4, false, true);
        for (bool hdr : {false, true}) for (bool reverse : {false, true})
        {
            CAPTURE(hdr, reverse);
            auto input = f.gbuffer(hdr, reverse);
            const auto retained = f.depthStencil(*input->depthStencilTexture());
            auto water = f.water(*normal_a, reverse, 1024.f);
            const auto before = f.renderer->lightingTileStats();
            auto automatic = f.render(*input, water, hdr, reverse, Tile::automatic);
            const auto after = f.renderer->lightingTileStats();
            lighting_tiles_check::observed(before, after, Tile::automatic, false, 3);
            auto explicit16 = f.render(*input, water, hdr, reverse, Tile::tile16);
            lighting_tiles_check::observed(after, f.renderer->lightingTileStats(), Tile::tile16, false, 3);
            const auto expected = f.read(*automatic);
            REQUIRE(f.read(*explicit16) == expected);
            REQUIRE(expected != f.background);
            for (const auto& pixel : expected)
                for (auto value : pixel) REQUIRE((value & 0x7c00u) != 0x7c00u);
            const auto depth = f.depthStencil(*automatic->depthStencilTexture());
            const auto depth16 = f.depthStencil(*explicit16->depthStencilTexture());
            REQUIRE(depth.depth == depth16.depth); REQUIRE(depth.stencil == depth16.stencil);
            water.normal_maps = {normal_b.get(), normal_b.get()};
            auto changed = f.render(*input, water, hdr, reverse, Tile::automatic);
            auto changed16 = f.render(*input, water, hdr, reverse, Tile::tile16);
            REQUIRE(f.read(*changed) != expected);
            REQUIRE(f.read(*changed16) == f.read(*changed));
            const auto current = f.depthStencil(*input->depthStencilTexture());
            REQUIRE(current.depth == retained.depth); REQUIRE(current.stencil == retained.stencil);
        }
    }
}

TEST_CASE("Generic water retains exact frozen-library manual and default probe lighting",
          "[almetal][probe-candidate-water][.probe-candidate-previous-library]")
{
    @autoreleasepool
    {
        using namespace lighting_tiles_water;
        const char* baseline=std::getenv("ALCHEMY_PROBE_CANDIDATE_BASELINE_LIBRARY");
        REQUIRE(baseline);REQUIRE(std::filesystem::is_regular_file(baseline));
        Fixture f;
        auto normal=f.normals(4,false);
        ALMetalTextureDescriptor descriptor;descriptor.type=ALMetalTextureType::texture_cube_array;
        descriptor.array_length=4;descriptor.format=ALMetalPixelFormat::rgba8_unorm;
        auto radiance=ALMetalTexture::create(*f.device,descriptor,&f.error);REQUIRE(radiance);
        descriptor={};descriptor.width=4;descriptor.height=4;descriptor.format=ALMetalPixelFormat::rgba32_uint;
        auto irradiance=ALMetalTexture::create(*f.device,descriptor,&f.error);REQUIRE(irradiance);
        auto upload=f.device->makeCommandBuffer("Water manual/default radiance input",&f.error);REQUIRE(upload);
        const std::array<std::array<std::uint8_t,4>,4> colors{{{{224,32,16,255}},{{16,224,32,255}},{{32,16,224,255}},{{224,224,16,255}}}};
        for(unsigned layer=0;layer<4;++layer)for(unsigned face=0;face<6;++face)
            REQUIRE(radiance->upload(*upload,0,layer*6+face,{},colors[layer].data(),4,&f.error));
        const std::array<std::uint32_t,64> unused_sh{};
        REQUIRE(irradiance->upload(*upload,0,0,{},unused_sh.data(),64,&f.error));
        REQUIRE(upload->commitAndWait(&f.error));
        ALMetalWorldReflectionProbes probes;probes.radiance_texture=radiance.get();probes.irradiance_texture=irradiance.get();
        probes.constants.config[0]=4;
        for(unsigned i=0;i<4;++i)
        {
            probes.constants.ref_index[i][0]=i;probes.constants.ref_index[i][1]=-1;
            probes.constants.ref_index[i][3]=i==1||i==2?1:0;
            probes.constants.ref_sphere[i][3]=2000.f;
            probes.constants.ref_params[i][0]=probes.constants.ref_params[i][1]=probes.constants.ref_params[i][2]=1.f;
        }
        for(auto& bucket:probes.constants.ref_bucket)bucket[0]=1;
        ALMetalWorldAlphaEnvironment environment;
        environment.water_plane[2]=1.f;environment.water_plane[3]=-1.f;
        environment.water_fog[0]=1.f;environment.water_fog[2]=-1.f;
        environment.light_direction[2]=1.f;environment.atmospheric[1]=environment.atmospheric[2]=environment.atmospheric[3]=1.f;
        environment.atmos_sunlit[3]=environment.atmos_sunlit_linear[3]=1.f;
        ALMetalWorldLegacyAlphaLighting lighting;lighting.sun_direction[2]=1.f;
        lighting.parameters[0]=128.f;lighting.parameters[1]=.004f;lighting.parameters[2]=1.f;
        for(bool hdr:{false,true})for(bool reverse:{false,true})for(bool classic:{false,true})
        {
            CAPTURE(hdr,reverse,classic);
            auto input=f.gbuffer(hdr,reverse);auto surface=f.water(*normal,reverse,4.f);
            surface.underwater=false;surface.refracts=false;surface.field.default_height[0]=-.5f;surface.eye_position[2]=1.f;
            surface.normal_matrix[0][0]=surface.normal_matrix[1][1]=surface.normal_matrix[2][2]=1.f;
            surface.normal_scale_glint[0]=surface.normal_scale_glint[1]=surface.normal_scale_glint[2]=2.f;
            surface.normal_scale_glint[3]=1.f;surface.optics[1]=1.f;surface.optics[2]=.7f;surface.light_direction[2]=1.f;
            lighting.parameters[3]=classic?1.f:0.f;
            const auto render=[&]()
            {
                ALMetalWorldSimpleBatch marker;marker.water_surface=true;
                ALMetalWorldOpaquePostDeferredScene scene;scene.gbuffer=input.get();scene.composed_lighting=f.lighting.get();
                scene.batches=&marker;scene.batch_count=1;scene.width=scene.height=SIDE;scene.hdr=hdr;scene.reverse_z=reverse;
                scene.reflection_probes=&probes;scene.alpha_environment=&environment;scene.alpha_lighting=&lighting;scene.water_surface=&surface;
                scene.forward_probe_projection_valid=true;identity(scene.forward_probe_projection);
                auto output=f.renderer->renderOpaquePostDeferred(scene,&f.error);INFO(f.error);REQUIRE(output);
                return std::pair{f.read(*output),f.depthStencil(*output->depthStencilTexture())};
            };
            const auto stats=f.frames->stats();
            const auto current=render();CHECK(current.first!=f.background);
            for(const auto& pixel:current.first)for(auto word:pixel)REQUIRE((word&0x7c00u)!=0x7c00u);
            REQUIRE(f.renderer->reloadShaders(std::string(baseline),&f.error));
            const auto previous=render();CHECK(previous.first==current.first);
            CHECK(previous.second.depth==current.second.depth);CHECK(previous.second.stencil==current.second.stencil);
            probes.constants.ref_sphere[1][0]=1000000.f;
            const auto manual_removed=render();CHECK(manual_removed.first!=previous.first);
            probes.constants.ref_sphere[2][0]=1000000.f;
            const auto default_only=render();CHECK(default_only.first!=manual_removed.first);
            std::array<ALMetalWorldGBufferRenderer*,1> owners{{f.renderer.get()}};
            REQUIRE(ALMetalWorldGBufferRenderer::reloadSharedShaders(*f.frames,owners,&f.error));
            const auto current_default=render();CHECK(current_default.first==default_only.first);
            CHECK(current_default.second.depth==default_only.second.depth);CHECK(current_default.second.stencil==default_only.second.stencil);
            probes.constants.ref_sphere[1][0]=probes.constants.ref_sphere[2][0]=0.f;
            CHECK(render().first==current.first);
            CHECK(f.frames->stats().candidate_preparations==stats.candidate_preparations);
            CHECK(f.frames->stats().candidate_reuses==stats.candidate_reuses);
        }
        // This is real generic water/radiance, not an SH test. The declared SH
        // texture is valid but unused by the water helper. No mask is prepared.
    }
}
