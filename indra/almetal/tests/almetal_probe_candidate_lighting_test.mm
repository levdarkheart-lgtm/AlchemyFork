/**
 * @file almetal_probe_candidate_lighting_test.mm
 * @brief Full native BASE/probe lighting parity over prepared candidate masks.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalworld.h"
#include "../almetalprobecandidates.h"
#include "../almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#import <Metal/Metal.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace
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
        d={};d.type=ALMetalTextureType::texture_cube_array;d.array_length=20;
        d.format=ALMetalPixelFormat::rgba8_unorm;d.usage=ALMetalTextureUsage::shader_read;
        radiance=ALMetalTexture::create(*f.device,d,&f.error);REQUIRE(radiance);
        d={};d.width=4;d.height=20;d.format=ALMetalPixelFormat::rgba32_uint;
        irradiance=ALMetalTexture::create(*f.device,d,&f.error);REQUIRE(irradiance);
        std::array<std::uint32_t,20*16> sh{};
        constexpr std::uint16_t palette[3][3]={{0x3800,0x3400,0x3000},
            {0x3000,0x3800,0x3400},{0x3400,0x3000,0x3800}};
        for (unsigned layer=0;layer<20;++layer)
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
        auto& pack=probes.constants;pack.config[0]=20.f;
        for (unsigned i=0;i<20;++i)
        {
            pack.ref_sphere[i][0]=i?(-2.4f+1.2f*((i-1)%5)):0.f;
            pack.ref_sphere[i][1]=i?(-1.8f+1.2f*((i-1)/5)):0.f;
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
    std::unique_ptr<ALMetalRenderTarget> render(bool prepared)
    {
        auto scene=base;scene.projection_valid=prepared;
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
        std::array<ALMetalProbeCandidateSphere,20> spheres{};
        for (unsigned i=0;i<20;++i)
            std::copy_n(probes.constants.ref_sphere[i],4,spheres[i].center_radius);
        const auto packet=alMetalBuildProbeCandidates(spheres,20.f,view);
        REQUIRE(packet.config[3]==1);
        unsigned bounded=0;
        for (unsigned i=1;i<20;++i)
        {
            const auto& r=packet.bounds[i];
            bounded+=r.x_min>0||r.y_min>0||r.x_max<cell.width||r.y_max<cell.height;
        }
        CHECK(bounded==19); // reject an all-full/fallback-only fixture
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

TEST_CASE("Prepared probe candidates preserve complete native main and fused-probe lighting",
          "[almetal][probe-candidate-lighting]")
{
    @autoreleasepool
    {
        Fixture f;
        const std::array<Cell,8> cells{{
            {false,false,false,false,false,96,64},
            {true,false,false,true,false,97,65},
            {false,true,false,true,false,96,64},
            {true,true,true,false,false,97,65},
            {true,false,false,true,true,96,64},
            {false,false,true,false,true,97,65},
            {true,true,false,false,true,96,64},
            {false,true,true,true,true,97,65}}};
        for (unsigned index=0;index<cells.size();++index)
        {
            const auto& cell=cells[index];
            CAPTURE(index,cell.pbr,cell.reverse,cell.infinite,cell.hdr,cell.probe,cell.width,cell.height);
            f.data->beginFrame(index+1);
            Inputs input(f,cell,index);input.checkBounds();
            const auto original=f.depthStencil(*input.gbuffer->depthStencilTexture());
            REQUIRE(std::all_of(original.stencil.begin(),original.stencil.end(),[](auto value){return value==0x5a;}));
            const auto before=f.data->stats();
            auto generic=input.render(false);INFO(f.error);REQUIRE(generic);
            const auto expected=f.colors(*generic);requirePositiveFinite(expected);
            CHECK(f.data->stats().candidate_preparations==before.candidate_preparations);
            auto prepared=input.render(true);INFO(f.error);REQUIRE(prepared);
            CHECK(f.colors(*prepared)==expected);
            CHECK(f.data->stats().candidate_preparations>before.candidate_preparations);
            const auto after=f.data->stats();
            auto reused=input.render(true);INFO(f.error);REQUIRE(reused);
            CHECK(f.colors(*reused)==expected);
            CHECK(f.data->stats().candidate_preparations==after.candidate_preparations);
            CHECK(f.data->stats().candidate_reuses>after.candidate_reuses);
            REQUIRE(prepared->depthStencilTexture());
            const auto depth=f.depthStencil(*prepared->depthStencilTexture());
            CHECK(depth.depth==original.depth);CHECK(depth.stencil==original.stencil);
            const auto retained=f.depthStencil(*input.gbuffer->depthStencilTexture());
            CHECK(retained.depth==original.depth);CHECK(retained.stencil==original.stencil);
            // The removed local volume intersects the lit receiver plane.
            // Require a real color response, then independently retain exact
            // generic/prepared parity for this changed published generation.
            input.probes.constants.ref_sphere[1][0]=1000.f;
            auto changed_generic=input.render(false);INFO(f.error);REQUIRE(changed_generic);
            const auto changed=f.colors(*changed_generic);
            requirePositiveFinite(changed);
            CHECK(changed!=expected);
            auto changed_prepared=input.render(true);INFO(f.error);REQUIRE(changed_prepared);
            CHECK(f.colors(*changed_prepared)==changed);
            CHECK(f.data->stats().candidate_preparations>after.candidate_preparations);
            CHECK(f.data->stats().candidate_failures==before.candidate_failures);
        }
    }
}
