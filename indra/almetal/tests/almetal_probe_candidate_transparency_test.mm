/**
 * @file almetal_probe_candidate_transparency_test.mm
 * @brief Strict full-renderer simple-alpha parity with virtual-eye probe masks.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalworld.h"
#include "../almetalprobecandidates.h"
#include "../almetalpipelinecache.h"
#include <catch2/catch_test_macros.hpp>
#import <Metal/Metal.h>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
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
    std::shared_ptr<ALMetalDevice> device;
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    Fixture(std::shared_ptr<ALMetalDevice> selected_device = {},
            std::shared_ptr<const ALMetalShaderLibrary> library = {},
            bool make_renderer = true)
        : device(std::move(selected_device))
    {
        if (!device) device=ALMetalDevice::create(&error);
        INFO(error); REQUIRE(device);
        data=ALMetalWorldFrameData::create(*device,&error); REQUIRE(data);
        if(make_renderer)
        {
            renderer=library?ALMetalWorldGBufferRenderer::create(*device,1,std::move(library),&error)
                            :ALMetalWorldGBufferRenderer::create(*device,&error);
            INFO(error); REQUIRE(renderer);
            renderer->setFrameData(data);
        }
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
    Planes read(const ALMetalRenderTarget& target,bool color=true)
    {
        const auto width=target.descriptor().width,height=target.descriptor().height;
        REQUIRE(target.colorTexture(0)); REQUIRE(target.depthStencilTexture());
        Planes result;
        if(color)
        {
            REQUIRE(target.colorTexture(0)->descriptor().format==ALMetalPixelFormat::rgba16_float);
            result.color.resize(std::size_t(width)*height);
            REQUIRE(target.colorTexture(0)->readback(*device,0,0,{},result.color.data(),width*8,&error));
        }
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
    std::array<std::uint8_t,3> diffuse_alpha{};
    std::array<std::unique_ptr<ALMetalTexture>,4> material_maps;
    std::unique_ptr<ALMetalTexture> radiance,irradiance,shadow;
    std::array<ALMetalWorldSimpleBatch,2> batches;
    std::array<float,16> palette{{0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0}};
    std::array<float,45*4> avatar_palette{};
    std::size_t avatar_weight_offset=0,avatar_cloth_offset=0;
    ALMetalWorldReflectionProbes probes;
    ALMetalWorldAlphaEnvironment environment;
    ALMetalWorldLegacyAlphaLighting lighting;
    ALMetalWorldDirectionalShadows shadows;
    ALMetalWorldOpaquePostDeferredScene scene;
    std::array<std::uint32_t,40*16> packedSH{};
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
            normals[i]={{0,0,1,0}}; weights[i]={{.5f,0,0,0}}; tangents[i]={{1,0,0,1}};
            uv[i]={{float(i&1u),float((i>>1u)&1u)}};
            colors[i]={{255,std::uint8_t(i<4?224:255),255,std::uint8_t(i<4?144:112)}};
        }
        constexpr std::size_t normal_offset=sizeof(encoded),uv_offset=normal_offset+sizeof(normals);
        constexpr std::size_t color_offset=uv_offset+sizeof(uv),weight_offset=(color_offset+sizeof(colors)+15)&~std::size_t(15);
        constexpr std::size_t tangent_offset=weight_offset+sizeof(weights);
        std::array<float,8> avatar_weights{};
        std::array<std::array<float,4>,8> clothing{};
        for(auto& value:clothing)value={{.1f,.05f,0.f,.25f}};
        constexpr std::size_t avatar_weights_offset=tangent_offset+sizeof(tangents);
        constexpr std::size_t clothing_offset=avatar_weights_offset+sizeof(avatar_weights);
        std::array<std::uint8_t,clothing_offset+sizeof(clothing)> bytes{};
        std::memcpy(bytes.data(),encoded.data(),sizeof(encoded));
        std::memcpy(bytes.data()+normal_offset,normals.data(),sizeof(normals));
        std::memcpy(bytes.data()+uv_offset,uv.data(),sizeof(uv));
        std::memcpy(bytes.data()+color_offset,colors.data(),sizeof(colors));
        std::memcpy(bytes.data()+weight_offset,weights.data(),sizeof(weights));
        std::memcpy(bytes.data()+tangent_offset,tangents.data(),sizeof(tangents));
        std::memcpy(bytes.data()+avatar_weights_offset,avatar_weights.data(),sizeof(avatar_weights));
        std::memcpy(bytes.data()+clothing_offset,clothing.data(),sizeof(clothing));
        avatar_weight_offset=avatar_weights_offset;avatar_cloth_offset=clothing_offset;
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
            batch.color_offset=color_offset; batch.tangent_offset=tangent_offset;
            batch.texcoord1_offset=batch.texcoord2_offset=uv_offset;
            batch.index_offset=i*6*sizeof(std::uint16_t); batch.index_count=6;
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
            diffuse_alpha[i]=diffuse_colors[i][3];
            for(auto& batch:batches)batch.diffuse_textures[i]=diffuse[i].get();
        }
        descriptor={};descriptor.type=ALMetalTextureType::texture_cube_array;descriptor.array_length=40;
        descriptor.format=ALMetalPixelFormat::rgba8_unorm;
        radiance=ALMetalTexture::create(*f.device,descriptor,&f.error); REQUIRE(radiance);
        for(unsigned layer=0;layer<40;++layer)
        {
            const std::array<std::uint8_t,4> color{{std::uint8_t(32+(layer%7)*24),std::uint8_t(192-(layer%5)*24),std::uint8_t(48+(layer%6)*24),255}};
            for(unsigned face=0;face<6;++face)REQUIRE(radiance->upload(*upload,0,layer*6+face,{},color.data(),4,&f.error));
            constexpr std::uint16_t bands[3][3]={{0x3800,0x3400,0x3000},{0x3000,0x3800,0x3400},{0x3400,0x3000,0x3800}};
            std::array<std::uint16_t,32> halves{};std::copy_n(bands[layer%3],3,halves.begin());
            halves[6]=0x2400;halves[7]=0xa000;halves[8]=0x2000;
            for(unsigned word=0;word<16;++word)packedSH[layer*16+word]=std::uint32_t(halves[word*2])|(std::uint32_t(halves[word*2+1])<<16);
        }
        descriptor={};descriptor.format=ALMetalPixelFormat::rgba32_uint;descriptor.width=4;descriptor.height=40;
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
    void spatial()
    {
        probes.constants={}; auto& p=probes.constants;p.config[0]=20;
        for(unsigned i=0;i<40;++i)
        {
            p.ref_sphere[i][0]=i?(-2.4f+1.2f*((i-1)%5)):0.f;
            p.ref_sphere[i][1]=i?(-1.8f+1.2f*((i-1)/5)):0.f;
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
    void avatar(unsigned variant)
    {
        for(unsigned joint=0;joint<15;++joint)
        {
            avatar_palette[joint*4]=1.f;
            avatar_palette[15*4+joint*4+1]=1.f;
            avatar_palette[30*4+joint*4+2]=1.f;
        }
        for(auto& batch:batches)
        {
            batch.pre_water_alpha=batch.post_water_alpha=false;
            batch.avatar_alpha=true;batch.avatar_rigid=variant==0;batch.avatar_skinning=variant!=0;
            batch.avatar_cloth=variant==2;batch.pre_water_minimum_alpha=.2f;
            batch.texture_count=1; // System-avatar vertices always select slot0.
            batch.skin_palette=variant?avatar_palette.data():nullptr;
            batch.skin_palette_float_count=variant?avatar_palette.size():0;
            batch.skin_joint_count=variant?15:0;batch.weight_offset=variant?avatar_weight_offset:0;
            batch.cloth_offset=variant==2?avatar_cloth_offset:0;
            batch.avatar_wind[2]=.2f;batch.avatar_wind[3]=.1f;
            batch.avatar_sin_wave[0]=batch.avatar_sin_wave[1]=batch.avatar_sin_wave[2]=1.f;
            batch.avatar_sin_wave[3]=.4f;batch.avatar_cloth_depth[2]=1.f;
        }
    }
    void shiny()
    {
        for(auto& batch:batches)
        {
            batch.pre_water_alpha=batch.post_water_alpha=false;
            batch.pre_water_simple=batch.pre_water_shadows=false;
            batch.impostor_shiny=true;
        }
        scene.alpha_directional_shadows=nullptr;
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
    Rendered render(bool prepared)
    {
        // Each arm starts from independently owned attachments. No stale blended
        // destination/depth/stencil from a preceding arm can hide a discrepancy.
        auto gbuffer=seeded(true),composed=seeded(false);auto request=scene;
        request.gbuffer=gbuffer.get();request.composed_lighting=composed.get();request.forward_probe_projection_valid=prepared;
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

struct ImpostorPlanes
{
    std::array<std::vector<std::uint8_t>,4> colors;
    Planes planes;
    bool operator==(const ImpostorPlanes&) const = default;
};
ImpostorPlanes readImpostor(Fixture& f,const ALMetalRenderTarget& target)
{
    ImpostorPlanes result;result.planes=f.read(target,false);
    for(unsigned i=0;i<4;++i)
    {
        const auto* texture=target.colorTexture(i);REQUIRE(texture);
        const auto row=texture->width()*texture->bytesPerPixel();REQUIRE(row>0);
        result.colors[i].resize(std::size_t(row)*texture->height());
        REQUIRE(texture->readback(*f.device,0,0,{},result.colors[i].data(),row,&f.error));
    }
    return result;
}
struct ImpostorResult {ImpostorPlanes before,after;};

ImpostorResult renderImpostor(Fixture& f,Inputs& input,bool prepared)
{
    ALMetalRenderTargetDescriptor descriptor;descriptor.width=input.cell.width;descriptor.height=input.cell.height;
    descriptor.color_attachment_count=4;descriptor.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
    descriptor.colors[0].format=ALMetalPixelFormat::rgba8_unorm_srgb;
    descriptor.colors[1].format=ALMetalPixelFormat::rgba8_unorm;
    descriptor.colors[2].format=ALMetalPixelFormat::rgba16_unorm;
    descriptor.colors[3].format=input.cell.hdr?ALMetalPixelFormat::rg11b10_float:ALMetalPixelFormat::rgba8_unorm;
    auto target=ALMetalRenderTarget::create(*f.device,descriptor,&f.error);REQUIRE(target);
    ALMetalRenderPassConfig config;config.clear_depth=input.cell.reverse?0.:1.;config.clear_stencil=0x5a;
    config.depth_store=config.stencil_store=ALMetalStoreAction::store;
    for(auto& color:config.colors)color.clear={.03125,.0625,.125,.5};
    auto pass=target->makeRenderPass(config,&f.error);REQUIRE(pass);
    auto command=f.device->makeCommandBuffer("Independent impostor attachments",&f.error);REQUIRE(command);
    auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Independent impostor clear",&f.error);REQUIRE(encoder);
    REQUIRE(encoder->end(&f.error));REQUIRE(command->commitAndWait(&f.error));
    ImpostorResult result;result.before=readImpostor(f,*target);
    ALMetalWorldGBufferScene scene;scene.batches=input.batches.data();scene.batch_count=input.batches.size();
    scene.width=input.cell.width;scene.height=input.cell.height;scene.hdr=input.cell.hdr;scene.normal_hdr=true;scene.reverse_z=input.cell.reverse;
    scene.reflection_probes=&input.probes;scene.legacy_alpha_lighting=&input.lighting;
    scene.directional_shadows=input.cell.shadow?&input.shadows:nullptr;
    std::copy_n(input.scene.forward_probe_projection,16,scene.forward_probe_projection);
    scene.forward_probe_projection_valid=prepared;
    REQUIRE(f.renderer->compositeImpostorPostDeferred(*target,scene,&f.error));INFO(f.error);
    result.after=readImpostor(f,*target);
    CHECK(result.after.colors[0]!=result.before.colors[0]);
    for(unsigned i=1;i<4;++i)CHECK(result.after.colors[i]==result.before.colors[i]);
    CHECK(result.after.planes.stencil==result.before.planes.stencil);
    return result;
}
}



TEST_CASE("Prepared body and cloth preserve frozen-library outputs",
          "[almetal][probe-candidate-avatar][.probe-candidate-previous-library]")
{
    @autoreleasepool
    {
        const char* baseline=std::getenv("ALCHEMY_PROBE_CANDIDATE_BASELINE_LIBRARY");
        REQUIRE(baseline);REQUIRE(std::filesystem::is_regular_file(baseline));
        Fixture f;
        for(unsigned variant=0;variant<3;++variant)
        {
            Inputs input(f,{false,true,true,false,true,97,65});input.avatar(variant);
            auto generic=input.render(false);auto prepared=input.render(true);CHECK(prepared.planes==generic.planes);
            CHECK(prepared.diagnostics.probe_candidate_avatar_batches==2);
            REQUIRE(f.renderer->reloadShaders(std::string(baseline),&f.error));
            auto previous=input.render(false);CHECK(previous.planes==generic.planes);
            std::array<ALMetalWorldGBufferRenderer*,1> owners{{f.renderer.get()}};
            REQUIRE(ALMetalWorldGBufferRenderer::reloadSharedShaders(*f.data,owners,&f.error));
            CHECK(input.render(true).planes==generic.planes);
        }
    }
}

TEST_CASE("Empty forward batch spans do not prepare masks or publish consumer activity",
          "[almetal][probe-candidate-transparency][probe-candidate-policy]")
{
    @autoreleasepool
    {
        Fixture f;Inputs input(f,{false,false,false,false,true,96,64});
        input.scene.batches=nullptr;input.scene.batch_count=0;
        const auto before=f.data->stats();
        const auto activity=f.renderer->opaquePostDiagnostics().probe_candidate_activity;
        auto generic=input.render(false);auto projected=input.render(true);
        CHECK(projected.planes==generic.planes);
        encoded(projected,false,0);
        CHECK(f.data->stats().candidate_preparations==before.candidate_preparations);
        CHECK(f.data->stats().candidate_reuses==before.candidate_reuses);
        const auto publication=f.data->stats();
        // Neither empty view may request the 49,216-byte forward pack,
        // whether a request would copy new bytes or hit content deduplication.
        CHECK(publication.published_bytes-before.published_bytes+
              publication.reused_bytes-before.reused_bytes<
              sizeof(ALMetalWorldReflectionProbeConstants));
        const auto& after=projected.diagnostics.probe_candidate_activity;
        CHECK(after.encoded_calls==activity.encoded_calls+2);
        CHECK(after.eligible_batches==activity.eligible_batches);
        CHECK(after.prepared_batches==activity.prepared_batches);
    }
}

TEST_CASE("Prepared avatar masks preserve rigid body cloth and depth-only ownership",
          "[almetal][probe-candidate-transparency][probe-candidate-avatar]")
{
    @autoreleasepool
    {
        Fixture f;
        for(unsigned variant=0;variant<3;++variant)for(bool shadow:{false,true})for(bool reverse:{false,true})
        {
            CAPTURE(variant,shadow,reverse);
            Inputs input(f,{false,shadow,reverse,false,true,97,65});input.avatar(variant);
            const auto before=f.renderer->opaquePostDiagnostics().probe_candidate_activity;
            auto generic=input.render(false);positive(generic.planes);
            auto prepared=input.render(true);CHECK(prepared.planes==generic.planes);
            encoded(prepared,shadow,0);CHECK(prepared.diagnostics.probe_candidate_avatar_batches==2);
            CHECK(prepared.diagnostics.probe_candidate_activity.avatar_eligible_batches==before.avatar_eligible_batches+4);
            CHECK(prepared.diagnostics.probe_candidate_activity.avatar_prepared_batches==before.avatar_prepared_batches+2);
            input.probes.constants.ref_params[1][0]=.25f;
            auto changed=input.render(false);CHECK(changed.planes.color!=generic.planes.color);
            auto changed_prepared=input.render(true);CHECK(changed_prepared.planes==changed.planes);
            CHECK(changed_prepared.diagnostics.probe_candidate_avatar_batches==2);
            if(variant)
                for(unsigned joint=0;joint<15;++joint)input.avatar_palette[joint*4+3]=.25f;
            else for(auto& batch:input.batches)
            {
                batch.modelview[12]=.25f;batch.modelview_projection[12]=.25f;
            }
            auto posed=input.render(false);CHECK(posed.planes.color!=changed.planes.color);
            auto posed_prepared=input.render(true);CHECK(posed_prepared.planes==posed.planes);
            CHECK(posed_prepared.diagnostics.probe_candidate_avatar_batches==2);
            for(auto& batch:input.batches)batch.avatar_diffuse_srgb=false;
            auto raw=input.render(false);CHECK(raw.planes.color!=posed.planes.color);
            auto raw_prepared=input.render(true);CHECK(raw_prepared.planes==raw.planes);
            CHECK(raw_prepared.diagnostics.probe_candidate_avatar_batches==2);
            for(auto& batch:input.batches){batch.avatar_color_write=false;batch.avatar_depth_write=true;}
            const auto stats=f.data->stats();
            const auto activity=f.renderer->opaquePostDiagnostics().probe_candidate_activity;
            auto depth_generic=input.render(false);auto depth_prepared=input.render(true);
            CHECK(depth_prepared.planes==depth_generic.planes);
            CHECK(depth_prepared.diagnostics.probe_candidate_avatar_batches==0);
            CHECK(depth_prepared.diagnostics.probe_candidate_activity.avatar_eligible_batches==activity.avatar_eligible_batches);
            CHECK(depth_prepared.diagnostics.probe_candidate_activity.avatar_prepared_batches==activity.avatar_prepared_batches);
            CHECK(f.data->stats().candidate_preparations==stats.candidate_preparations);
            CHECK(f.data->stats().candidate_reuses==stats.candidate_reuses);
        }
    }
}













TEST_CASE("Prepared probe masks preserve ordered rigid and skinned simple alpha",
          "[almetal][probe-candidate-transparency]")
{
    @autoreleasepool
    {
        Fixture f;
        constexpr std::array<Cell,8> cells{{
            {false,false,false,false,false,96,64},{false,true,true,false,true,97,65},
            {true,false,false,true,true,97,65},{true,true,true,true,false,96,64},
            {false,false,true,true,true,97,65},{false,true,false,true,false,96,64},
            {true,false,true,false,false,96,64},{true,true,false,false,true,97,65}}};
        for(unsigned i=0;i<cells.size();++i)
        {
            CAPTURE(i,cells[i].skinned,cells[i].shadow,cells[i].reverse,cells[i].infinite,cells[i].hdr);
            f.data->beginFrame(i+1);Inputs input(f,cells[i]);
            auto generic=input.render(false);positive(generic.planes);encoded(generic,cells[i].shadow,0);
            const auto before=f.data->stats();auto prepared=input.render(true);
            CHECK(prepared.planes==generic.planes);encoded(prepared,cells[i].shadow,2);
            CHECK(f.data->stats().candidate_preparations+f.data->stats().candidate_reuses>
                  before.candidate_preparations+before.candidate_reuses);
            const auto after=f.data->stats();auto reused=input.render(true);
            CHECK(reused.planes==generic.planes);encoded(reused,cells[i].shadow,2);
            CHECK(f.data->stats().candidate_preparations==after.candidate_preparations);
            CHECK(f.data->stats().candidate_reuses>after.candidate_reuses);
            input.probes.constants.ref_sphere[1][0]=1000.f;
            auto changed=input.render(false);positive(changed.planes);CHECK(changed.planes.color!=generic.planes.color);
            auto changed_prepared=input.render(true);CHECK(changed_prepared.planes==changed.planes);encoded(changed_prepared,cells[i].shadow,2);
            CHECK(f.data->stats().candidate_failures==before.candidate_failures);
            if(cells[i].skinned)
            {
                input.palette[0]=.25f; // Current rebased pose; mask geometry remains unchanged.
                auto posed_generic=input.render(false);positive(posed_generic.planes);
                CHECK(posed_generic.planes.color!=changed.planes.color);
                auto posed_prepared=input.render(true);
                CHECK(posed_prepared.planes==posed_generic.planes);encoded(posed_prepared,cells[i].shadow,2);
            }
            if(i==0)
            {
                std::swap(input.batches[0],input.batches[1]);
                auto reordered=input.render(false);CHECK(reordered.planes.color!=changed.planes.color);
                auto reordered_prepared=input.render(true);
                CHECK(reordered_prepared.planes==reordered.planes);encoded(reordered_prepared,false,2);
            }
        }
    }
}

TEST_CASE("Prepared probe masks select post-water-only rigid and skinned simple alpha",
          "[almetal][probe-candidate-transparency]")
{
    @autoreleasepool
    {
        Fixture f;
        constexpr std::array<Cell,8> cells{{
            {false,false,false,false,false,96,64},{false,true,true,false,true,97,65},
            {true,false,false,true,true,97,65},{true,true,true,true,false,96,64},
            {false,false,true,true,true,97,65},{false,true,false,true,false,96,64},
            {true,false,true,false,false,96,64},{true,true,false,false,true,97,65}}};
        for(unsigned i=0;i<cells.size();++i)
        {
            CAPTURE(i,cells[i].skinned,cells[i].shadow,cells[i].reverse,cells[i].infinite,cells[i].hdr);
            f.data->beginFrame(i+1);Inputs input(f,cells[i]);input.postWaterOnly();
            REQUIRE(std::all_of(input.batches.begin(),input.batches.end(),[](const auto& batch) {
                return !batch.pre_water_alpha && batch.post_water_alpha && !batch.post_water_depth_only;
            }));
            auto generic=input.render(false);
            positive(generic.planes,cells[i].skinned,cells[i].reverse);encoded(generic,cells[i].shadow,0);
            const auto before=f.data->stats();auto prepared=input.render(true);
            CHECK(prepared.planes==generic.planes);encoded(prepared,cells[i].shadow,2);
            CHECK(f.data->stats().candidate_preparations+f.data->stats().candidate_reuses>
                  before.candidate_preparations+before.candidate_reuses);
            const auto after=f.data->stats();auto reused=input.render(true);
            CHECK(reused.planes==generic.planes);encoded(reused,cells[i].shadow,2);
            CHECK(f.data->stats().candidate_preparations==after.candidate_preparations);
            CHECK(f.data->stats().candidate_reuses>after.candidate_reuses);
            input.probes.constants.ref_sphere[1][0]=1000.f;
            auto changed=input.render(false);
            positive(changed.planes,cells[i].skinned,cells[i].reverse);
            CHECK(changed.planes.color!=generic.planes.color);
            auto changed_prepared=input.render(true);
            CHECK(changed_prepared.planes==changed.planes);encoded(changed_prepared,cells[i].shadow,2);
            CHECK(f.data->stats().candidate_failures==before.candidate_failures);
            if(cells[i].skinned)
            {
                input.palette[0]=.25f;
                auto posed_generic=input.render(false);
                positive(posed_generic.planes,true,cells[i].reverse);
                CHECK(posed_generic.planes.color!=changed.planes.color);
                auto posed_prepared=input.render(true);
                CHECK(posed_prepared.planes==posed_generic.planes);encoded(posed_prepared,cells[i].shadow,2);
            }
            if(i==0)
            {
                std::swap(input.batches[0],input.batches[1]);
                auto reordered=input.render(false);CHECK(reordered.planes.color!=changed.planes.color);
                auto reordered_prepared=input.render(true);
                CHECK(reordered_prepared.planes==reordered.planes);encoded(reordered_prepared,false,2);
            }
        }
    }
}

TEST_CASE("Virtual probe lookup preserves alpha with mismatched and unsupported projections",
          "[almetal][probe-candidate-transparency]")
{
    @autoreleasepool
    {
        Fixture f;Inputs input(f,{false,true,false,false,true,97,65});
        // Deliberately keep raster MVP fixed while changing the actual eye
        // position: the lookup projects eye_position rather than raster XY.
        for(auto& batch:input.batches)batch.modelview[12]=.375f;
        auto generic=input.render(false);positive(generic.planes);auto prepared=input.render(true);
        CHECK(prepared.planes==generic.planes);encoded(prepared,true,2);
        for(auto& batch:input.batches){batch.modelview[12]=0.f;batch.modelview[14]=5.f;}
        // Actual eye positions are now behind the virtual camera, but the
        // authored raster geometry remains visible. Per-fragment fallback is
        // required; preparation/selection may remain active for this view.
        generic=input.render(false);positive(generic.planes);prepared=input.render(true);
        CHECK(prepared.planes==generic.planes);encoded(prepared,true,2);
        input.scene.forward_probe_projection[0]=1.e8f;
        const auto before=f.data->stats();generic=input.render(false);prepared=input.render(true);
        CHECK(prepared.planes==generic.planes);encoded(prepared,true,0);
        CHECK(f.data->stats().candidate_preparations==before.candidate_preparations);
    }
}

TEST_CASE("Prepared alpha keeps manual neighbors default fallback and accepted cap",
          "[almetal][probe-candidate-transparency]")
{
    @autoreleasepool
    {
        Fixture f;Inputs input(f,{true,false,true,true,true,97,65});
        for(unsigned layout=0;layout<4;++layout)
        {
            CAPTURE(layout);input.spatial();auto& p=input.probes.constants;
            if(layout==0)
            {
                p.ref_index[2][3]=1;p.ref_index[3][3]=-1;identity(p.ref_box[3]);
                p.ref_box[3][0]=p.ref_box[3][5]=p.ref_box[3][10]=2.f;p.ref_box[3][14]=8.f;
                p.ref_index[2][1]=0;p.ref_index[2][2]=5;
                p.ref_neighbor[0]=3;p.ref_neighbor[1]=1;p.ref_neighbor[2]=3;p.ref_neighbor[3]=-1;p.ref_neighbor[4]=256;
            }
            if(layout==1)
            {
                p.config[0]=40;
                for(unsigned i=1;i<39;++i){p.ref_sphere[i][0]=p.ref_sphere[i][1]=0.f;p.ref_sphere[i][3]=128.f;}
                p.ref_sphere[39][0]=1000.f;p.ref_sphere[39][3]=.1f; // nonfull packet despite32 accepted cap
            }
            if(layout==2)p.config[0]=0.f;
            if(layout==3)for(unsigned i=1;i<20;++i)p.ref_sphere[i][0]=1000.f;
            auto generic=input.render(false);positive(generic.planes);auto prepared=input.render(true);
            CHECK(prepared.planes==generic.planes);encoded(prepared,false,layout==2?0:2);
        }
    }
}

TEST_CASE("Alpha mask denial and reuse preserve current lighting and fallback",
          "[almetal][probe-candidate-transparency]")
{
    @autoreleasepool
    {
        Fixture f;Inputs input(f,{false,false,false,false,true,96,64});
        auto generic=input.render(false);auto first=input.render(true);CHECK(first.planes==generic.planes);encoded(first,false,2);
        unsigned admissions=0;
        f.data->setSourcePublicationAdmission([&](const ALMetalWorldSourcePublicationWork&)->std::shared_ptr<const void>{++admissions;return {};});
        auto initial=input.probes.constants;
        input.probes.constants.ref_sphere[1][0]=1000.f;
        auto changed_generic=input.render(false);const auto before=f.data->stats();auto denied=input.render(true);
        CHECK(denied.planes==changed_generic.planes);encoded(denied,false,0);CHECK(admissions>0);
        CHECK(f.data->stats().candidate_preparations==before.candidate_preparations);
        CHECK(f.data->stats().candidate_failures==before.candidate_failures);
        input.probes.constants=initial;admissions=0;
        input.probes.constants.ref_params[1][0]=.25f; // SH-lighting-only mutation: same exact mask dependencies
        auto lighting_generic=input.render(false);CHECK(lighting_generic.planes.color!=generic.planes.color);
        const auto reuse_before=f.data->stats();auto hit=input.render(true);
        CHECK(hit.planes==lighting_generic.planes);encoded(hit,false,2);CHECK(admissions==0);
        CHECK(f.data->stats().candidate_preparations==reuse_before.candidate_preparations);
        CHECK(f.data->stats().candidate_reuses>reuse_before.candidate_reuses);
        f.data->setSourcePublicationAdmission({});
        for(auto& batch:input.batches){batch.pre_water_source_rgb=ALMetalBlendFactor::one;batch.pre_water_destination_rgb=ALMetalBlendFactor::one;}
        auto custom_generic=input.render(false);const auto custom_before=f.data->stats();auto custom=input.render(true);
        CHECK(custom.planes==custom_generic.planes);encoded(custom,false,2);
        CHECK(f.data->stats().candidate_preparations==custom_before.candidate_preparations);
        CHECK(f.data->stats().candidate_reuses>custom_before.candidate_reuses);
    }
}

TEST_CASE("Prepared alpha retains probe filtering and the three-texture selector",
          "[almetal][probe-candidate-transparency]")
{
    @autoreleasepool
    {
        Fixture f;Inputs input(f,{false,true,true,false,true,97,65});
        input.shadows.constants.config[3]=2u|ALMetalWorldDirectionalShadowConstants::probe_single_tap;
        input.scene.diagnostic_view_kind=3;input.scene.diagnostic_view_index=2;
        auto pair_generic=input.render(false);positive(pair_generic.planes);
        auto pair=input.render(true);CHECK(pair.planes==pair_generic.planes);encoded(pair,true,2);
        // The first row composes the optional probe/High2/pair choices. The
        // next row uses the generic selector and a genuinely different slot2.
        // All three bound images are authored, not duplicate fallback dummies.
        for(auto& batch:input.batches)batch.texture_count=3;
        const std::uint32_t slot=2;
        for(unsigned vertex=4;vertex<8;++vertex)
            REQUIRE(input.vertices->write(vertex*4*sizeof(float)+3*sizeof(float),&slot,sizeof(slot),&f.error));
        auto triple_generic=input.render(false);positive(triple_generic.planes);
        CHECK(triple_generic.planes.color!=pair_generic.planes.color);
        auto triple=input.render(true);CHECK(triple.planes==triple_generic.planes);encoded(triple,true,2);
    }
}

TEST_CASE("Cumulative prepared alpha activity survives later same-frame generic calls",
          "[almetal][probe-candidate-transparency]")
{
    @autoreleasepool
    {
        Fixture f;Inputs input(f,{false,false,true,false,true,97,65});input.postWaterOnly();
        input.scene.diagnostic_frame_index=515;
        input.scene.diagnostic_view_kind=1;input.scene.diagnostic_view_index=0;
        const auto before=f.renderer->opaquePostDiagnostics().probe_candidate_activity;
        const auto activity=[&](const Inputs::Rendered& rendered,std::uint64_t calls,
                                std::uint64_t eligible,std::uint64_t prepared) {
            const auto& value=rendered.diagnostics.probe_candidate_activity;
            CHECK(value.encoded_calls==before.encoded_calls+calls);
            CHECK(value.eligible_batches[0]==before.eligible_batches[0]);
            CHECK(value.eligible_batches[1]==before.eligible_batches[1]+eligible);
            CHECK(value.prepared_batches[0]==before.prepared_batches[0]);
            CHECK(value.prepared_batches[1]==before.prepared_batches[1]+prepared);
            CHECK(rendered.diagnostics.probe_candidate_frame_index==515);
            CHECK(rendered.diagnostics.probe_candidate_view_kind==1);
            CHECK(rendered.diagnostics.probe_candidate_view_index==0);
        };
        auto prepared=input.render(true);positive(prepared.planes,false,true);encoded(prepared,false,2);
        activity(prepared,1,2,2);
        auto generic=input.render(false);CHECK(generic.planes==prepared.planes);encoded(generic,false,0);
        activity(generic,2,4,2);
        for(auto& batch:input.batches)
        {
            batch.pre_water_source_rgb=ALMetalBlendFactor::one;
            batch.pre_water_destination_rgb=ALMetalBlendFactor::one;
            // Fullbright has no probe consumer. Lit custom blends are now
            // covered separately and must not stand in for this fallback.
            batch.pre_water_simple=false;batch.pre_water_fullbright=true;
        }
        auto custom=input.render(true);positive(custom.planes,false,true);encoded(custom,false,0);
        activity(custom,3,4,2);
        std::array<ALMetalWorldGBufferRenderer*,1> owners{{f.renderer.get()}};
        REQUIRE(ALMetalWorldGBufferRenderer::reloadSharedShaders(*f.data,owners,&f.error));
        const auto reloaded=f.renderer->opaquePostDiagnostics().probe_candidate_activity;
        CHECK(reloaded.encoded_calls==custom.diagnostics.probe_candidate_activity.encoded_calls);
        CHECK(reloaded.eligible_batches==custom.diagnostics.probe_candidate_activity.eligible_batches);
        CHECK(reloaded.prepared_batches==custom.diagnostics.probe_candidate_activity.prepared_batches);
        CHECK(reloaded.unavailable_calls==custom.diagnostics.probe_candidate_activity.unavailable_calls);
        CHECK(reloaded.pipeline_fallback_batches==custom.diagnostics.probe_candidate_activity.pipeline_fallback_batches);
        auto after_reload=input.render(true);CHECK(after_reload.planes==custom.planes);encoded(after_reload,false,0);
        activity(after_reload,4,4,2);
        // A preflight failure must not publish activity, even when its
        // diagnostic frame is identical.
        auto gbuffer=input.seeded(true),composed=input.seeded(false);
        input.batches[0].pre_water_alpha=true; // Invalid: both water-stage owners.
        auto invalid=input.scene;invalid.gbuffer=gbuffer.get();invalid.composed_lighting=composed.get();
        CHECK_FALSE(f.renderer->renderOpaquePostDeferred(invalid,&f.error));
        CHECK(f.error.find("family contract")!=std::string::npos);
        const auto after=f.renderer->opaquePostDiagnostics().probe_candidate_activity;
        CHECK(after.encoded_calls==after_reload.diagnostics.probe_candidate_activity.encoded_calls);
        CHECK(after.eligible_batches==after_reload.diagnostics.probe_candidate_activity.eligible_batches);
        CHECK(after.prepared_batches==after_reload.diagnostics.probe_candidate_activity.prepared_batches);
        CHECK(after.unavailable_calls==after_reload.diagnostics.probe_candidate_activity.unavailable_calls);
    }
}

TEST_CASE("Forward probe decisions retain distinct conservative and admission fallbacks",
          "[almetal][probe-candidate-transparency]")
{
    @autoreleasepool
    {
        using Reason = ALMetalProbeCandidateReason;
        Fixture f; Inputs input(f,{false,true,true,false,true,97,65}); input.postWaterOnly();
        input.scene.diagnostic_frame_index=771;
        input.scene.diagnostic_view_kind=1; input.scene.diagnostic_view_index=0;
        const auto inspect=[&](const Inputs::Rendered& result,Reason reason,bool masks) {
            const auto& value=result.diagnostics.probe_candidate_decision;
            CHECK(value.valid); CHECK(value.reason==reason); CHECK(value.masks_bound==masks);
            CHECK(value.source_frame_index==771); CHECK(value.source_view_kind==1);
            CHECK(value.source_view_index==0); CHECK(value.probe_count==20);
            CHECK(value.eligible_batches==2); CHECK(value.prepared_batches==(masks?2u:0u));
            CHECK(value.pipeline_fallback_batches==0);
            if(reason!=Reason::none)
                CHECK(result.diagnostics.probe_candidate_activity.unavailable_calls[
                    static_cast<std::size_t>(reason)]>0);
        };
        auto generic=input.render(false); positive(generic.planes,false,true);
        inspect(generic,Reason::projection_unavailable,false);
        auto prepared=input.render(true); CHECK(prepared.planes==generic.planes);
        inspect(prepared,Reason::none,true);
        CHECK(prepared.diagnostics.probe_candidate_decision.packet_usable);
        CHECK(prepared.diagnostics.probe_candidate_decision.bounded_rects==19);
        CHECK(prepared.diagnostics.probe_candidate_decision.full_rects==0);
        // Every local sphere contains the eye: full bounds are legitimate,
        // and the ordinary shader must preserve their lighting contributions.
        for(unsigned index=1;index<20;++index)input.probes.constants.ref_sphere[index][3]=128.f;
        auto full_generic=input.render(false); auto full=input.render(true);
        CHECK(full.planes==full_generic.planes); inspect(full,Reason::all_full,false);
        CHECK(full.diagnostics.probe_candidate_decision.packet_usable);
        CHECK(full.diagnostics.probe_candidate_decision.full_rects==19);
        CHECK(full.diagnostics.probe_candidate_decision.bounded_rects==0);
        input.spatial(); input.probes.constants.ref_sphere[1][0]+=.125f; // New immutable key.
        unsigned denied=0;
        f.data->setSourcePublicationAdmission([&](const ALMetalWorldSourcePublicationWork&)
            ->std::shared_ptr<const void> {++denied;return {};});
        auto denied_generic=input.render(false); auto unavailable=input.render(true);
        REQUIRE(denied==1); CHECK(unavailable.planes==denied_generic.planes);
        inspect(unavailable,Reason::admission_denied,false);
        CHECK(unavailable.diagnostics.probe_candidate_decision.bounded_rects==19);
        f.data->setSourcePublicationAdmission({});
        auto restored=input.render(true); CHECK(restored.planes==denied_generic.planes);
        inspect(restored,Reason::none,true);
        CHECK(f.data->stats().candidate_failures==0);
    }
}

TEST_CASE("Prepared lighting keeps body and simple consumers with generic other families",
          "[almetal][probe-candidate-transparency][probe-candidate-policy]")
{
    @autoreleasepool
    {
        Fixture f;
        Inputs body(f,{false,true,false,false,true,97,65});body.avatar(2);
        auto body_generic=body.render(false);positive(body_generic.planes);
        CHECK(body_generic.diagnostics.probe_candidate_avatar_batches==0);
        auto body_prepared=body.render(true);CHECK(body_prepared.planes==body_generic.planes);
        CHECK(body_prepared.diagnostics.probe_candidate_avatar_batches==2);
        body.probes.constants.ref_params[1][0]=.25f;
        auto body_changed=body.render(false);CHECK(body_changed.planes.color!=body_generic.planes.color);
        CHECK(body.render(true).planes==body_changed.planes);

        Inputs simple(f,{false,true,false,false,true,97,65});
        auto simple_generic=simple.render(false);positive(simple_generic.planes);
        auto simple_prepared=simple.render(true);CHECK(simple_prepared.planes==simple_generic.planes);
        encoded(simple_prepared,true,2);

        const auto unchanged_preparation=[&](const auto& before)
        {
            const auto after=f.data->stats();
            CHECK(after.candidate_preparations==before.candidate_preparations);
            CHECK(after.candidate_reuses==before.candidate_reuses);
            CHECK(after.candidate_declines==before.candidate_declines);
        };
        for(unsigned family:{1u,2u})for(bool custom:{false,true})
        {
            CAPTURE(family,custom);
            Inputs input(f,{true,true,false,false,true,97,65});input.family(family);input.postWaterOnly();
            if(custom)for(auto& batch:input.batches)
            {batch.pre_water_source_rgb=ALMetalBlendFactor::one;batch.pre_water_destination_rgb=ALMetalBlendFactor::one;}
            auto generic=input.render(false);positive(generic.planes);
            const auto before=f.data->stats();auto selected=input.render(true);
            CHECK(selected.planes==generic.planes);unchanged_preparation(before);
            CHECK_FALSE(selected.diagnostics.probe_candidate_decision.masks_bound);
            for(auto& value:input.probes.constants.ref_params){value[0]*=.25f;value[1]*=.5f;}
            auto changed=input.render(false);CHECK(changed.planes.color!=generic.planes.color);
            CHECK(input.render(true).planes==changed.planes);
        }
        Inputs shiny(f,{false,false,false,false,true,97,65});shiny.shiny();
        auto shiny_generic=shiny.render(false);positive(shiny_generic.planes);
        auto before=f.data->stats();auto shiny_selected=shiny.render(true);
        CHECK(shiny_selected.planes==shiny_generic.planes);unchanged_preparation(before);
        CHECK_FALSE(shiny_selected.diagnostics.probe_candidate_decision.masks_bound);

        // A shared body mask must not select an unhelpful custom consumer.
        Inputs material(f,{false,true,false,false,true,97,65});material.family(1);
        Inputs pbr(f,{false,true,false,false,true,97,65});pbr.family(2);
        std::array<ALMetalWorldSimpleBatch,3> mixed{{body.batches[0],material.batches[1],pbr.batches[1]}};
        for(unsigned i=1;i<3;++i)
        {mixed[i].pre_water_source_rgb=ALMetalBlendFactor::one;mixed[i].pre_water_destination_rgb=ALMetalBlendFactor::one;}
        body.scene.batches=mixed.data();body.scene.batch_count=mixed.size();
        auto mixed_generic=body.render(false);auto mixed_prepared=body.render(true);
        CHECK(mixed_prepared.planes==mixed_generic.planes);
        CHECK(mixed_prepared.diagnostics.probe_candidate_decision.masks_bound);
        CHECK(mixed_prepared.diagnostics.probe_candidate_avatar_batches==1);
        CHECK(mixed_prepared.diagnostics.probe_candidate_decision.prepared_batches==1);

        // The compositor's new families remain generic before mask work.
        auto impostor_generic=renderImpostor(f,shiny,false);
        before=f.data->stats();
        auto impostor_selected=renderImpostor(f,shiny,true);
        CHECK(impostor_selected.after==impostor_generic.after);unchanged_preparation(before);
    }
}

TEST_CASE("Forward lighting isolates reused probe packs across pending views and commands",
          "[almetal][probe-candidate-transparency][lighting-data][PPD-567]")
{
    @autoreleasepool
    {
        Fixture f;
        Inputs input(f, {false, false, false, false, true, 97, 65});
        // Both shiny and simple-alpha consume the forward pack at fragment
        // slot 1. The above-water slot-3 consumer has a separate test below.
        // Their outer quarters do not overlap, so both bindings have an
        // independent visible response to the two authored packs below.
        auto& shiny = input.batches[0];
        shiny.pre_water_alpha = shiny.pre_water_simple = false;
        shiny.pre_water_shadows = false;
        shiny.impostor_shiny = true;

        std::array<ALMetalWorldReflectionProbeConstants, 2> packs{
            input.probes.constants, input.probes.constants};
        for (auto& parameters : packs[1].ref_params)
        {
            parameters[0] *= .125f; // Simple-alpha SH response.
            parameters[1] *= .25f;  // Shiny radiance response.
        }
        std::array<Planes, 2> expected;
        for (unsigned view = 0; view < expected.size(); ++view)
        {
            input.probes.constants = packs[view];
            expected[view] = input.render(false).planes;
            positive(expected[view]);
        }
        const auto changed_region = [&](unsigned first_x, unsigned last_x)
        {
            for (unsigned y = input.cell.height / 4; y < 3 * input.cell.height / 4; ++y)
                for (unsigned x = first_x; x < last_x; ++x)
                    for (unsigned channel = 0; channel < 3; ++channel)
                    {
                        const auto index = std::size_t(y) * input.cell.width + x;
                        if (expected[0].color[index][channel] != expected[1].color[index][channel])
                            return true;
                    }
            return false;
        };
        REQUIRE(changed_region(0, input.cell.width / 4));
        REQUIRE(changed_region(3 * input.cell.width / 4, input.cell.width));

        struct PendingView
        {
            std::unique_ptr<ALMetalRenderTarget> gbuffer, composed, output;
        };
        std::uint64_t generation = 100;
        for (bool external_pages : {true, false})
            for (bool separate_commands : {false, true})
                for (bool prepared : {false, true})
                {
                    CAPTURE(external_pages, separate_commands, prepared);
                    auto other = ALMetalWorldGBufferRenderer::create(
                        *f.device, 1, f.renderer->shaderLibrary(), &f.error);
                    INFO(f.error); REQUIRE(other);
                    const auto pages = external_pages
                        ? f.data : std::shared_ptr<ALMetalWorldFrameData>{};
                    f.renderer->setFrameData(pages);
                    other->setFrameData(pages);
                    f.data->beginFrame(generation++);
                    std::array<PendingView, 2> pending;
                    // Seed both sets before either owner has an unsubmitted
                    // transaction; no readback or queue wait runs between views.
                    for (auto& view : pending)
                    {
                        view.gbuffer = input.seeded(true);
                        view.composed = input.seeded(false);
                    }
                    REQUIRE(f.renderer->beginCommandBatch("Pending forward probe view A", &f.error));
                    if (separate_commands)
                        REQUIRE(other->beginCommandBatch("Pending forward probe view B", &f.error));
                    const auto first_completion = f.renderer->commandBatchCompletion();
                    const auto second_completion = separate_commands
                        ? other->commandBatchCompletion() : first_completion;
                    REQUIRE(first_completion); REQUIRE(second_completion);
                    CHECK_FALSE(first_completion->isComplete());
                    CHECK_FALSE(second_completion->isComplete());

                    for (unsigned view = 0; view < pending.size(); ++view)
                    {
                        // Reuse the identical caller address with different
                        // lighting while the prior GPU view is still pending.
                        input.probes.constants = packs[view];
                        auto scene = input.scene;
                        scene.gbuffer = pending[view].gbuffer.get();
                        scene.composed_lighting = pending[view].composed.get();
                        scene.forward_probe_projection_valid = prepared;
                        auto& owner = separate_commands && view ? *other : *f.renderer;
                        const auto before = f.data->stats();
                        pending[view].output = owner.renderOpaquePostDeferred(scene, &f.error);
                        INFO(f.error); REQUIRE(pending[view].output);
                        const auto diagnostics = owner.opaquePostDiagnostics();
                        const bool candidates = external_pages && prepared;
                        CHECK(diagnostics.probe_candidate_decision.masks_bound == candidates);
                        CHECK(diagnostics.probe_candidate_simple_batches == (candidates ? 1u : 0u));
                        if (prepared && !external_pages)
                            CHECK(diagnostics.probe_candidate_decision.reason ==
                                  ALMetalProbeCandidateReason::frame_data_unavailable);
                        if (external_pages)
                        {
                            // All other repeated constants in this two-draw
                            // scene total less than one full probe block. An
                            // extra content-dedup request for that same block
                            // would alone exhaust this observable byte bound.
                            const auto after = f.data->stats();
                            CHECK(after.reused_bytes - before.reused_bytes < sizeof(packs[0]));
                            f.data->beginFrame(generation++);
                        }
                    }
                    input.probes.constants = {}; // Destroy the borrowed values before either commit.
                    if (external_pages)
                        CHECK(f.data->stats().retired_pages >= 2);
                    CHECK_FALSE(first_completion->isComplete());
                    CHECK_FALSE(second_completion->isComplete());
                    // Creation order is not submission order. Independent
                    // command views must keep their exact immutable slices.
                    if (separate_commands)
                        REQUIRE(other->commitCommandBatch(&f.error));
                    REQUIRE(f.renderer->commitCommandBatch(&f.error));
                    REQUIRE(f.device->waitUntilIdle(&f.error));
                    CHECK(first_completion->isComplete());
                    CHECK(second_completion->isComplete());
                    for (unsigned view = 0; view < pending.size(); ++view)
                        CHECK(f.read(*pending[view].output) == expected[view]);
                }
    }
}

TEST_CASE("Above-water probe binding restores alpha and isolates pending view packs",
          "[almetal][probe-candidate-transparency][lighting-data][PPD-567][PPD-582]")
{
    @autoreleasepool
    {
        Fixture f;
        Inputs input(f, {false, false, false, false, true, 97, 65});
        auto& shiny = input.batches[0];
        shiny.pre_water_alpha = shiny.pre_water_simple = false;
        shiny.pre_water_shadows = false;
        shiny.impostor_shiny = true;
        auto& alpha = input.batches[1];
        alpha.pre_water_alpha = false;
        alpha.post_water_alpha = true;
        // Replace destination color so a visible right-hand alpha receiver
        // must match its independent no-water reference exactly. Water and
        // shiny cannot hide a failure to restore alpha's probe buffer 1.
        alpha.pre_water_source_rgb = ALMetalBlendFactor::one;
        alpha.pre_water_destination_rgb = ALMetalBlendFactor::zero;
        input.environment.water_fog[2] = 1.f;

        ALMetalTextureDescriptor descriptor;
        descriptor.format = ALMetalPixelFormat::rg32_float;
        auto cells = ALMetalTexture::create(*f.device, descriptor, &f.error);
        REQUIRE(cells);
        descriptor.format = ALMetalPixelFormat::rgba16_float;
        auto normal = ALMetalTexture::create(*f.device, descriptor, &f.error);
        REQUIRE(normal);
        auto upload = f.device->makeCommandBuffer("Pending-view water inputs", &f.error);
        REQUIRE(upload);
        const std::array<float, 2> height{0.f, 1.f};
        const Pixel flat_normal{0x3800, 0x3800, 0x3c00, 0x3c00};
        REQUIRE(cells->upload(*upload, 0, 0, {}, height.data(), sizeof(height), &f.error));
        REQUIRE(normal->upload(*upload, 0, 0, {}, flat_normal.data(), sizeof(flat_normal), &f.error));
        REQUIRE(upload->commitAndWait(&f.error));

        ALMetalWorldWaterSurface water;
        auto& field = water.field;
        field.cell_map = cells.get();
        field.above_water = true;
        identity(field.modelview_projection);
        field.modelview_projection[0] = field.modelview_projection[5] = .25f;
        field.modelview_projection[14] = .9f; // Behind the authored alpha receivers.
        field.cell_square[0] = field.cell_square[1] = -4.f;
        field.cell_square[2] = 8.f;
        field.cell_square[3] = 1.f;
        field.rings[0] = 4.f;
        field.rings[1] = 1.f;
        field.rings[2] = 4.f;
        field.water_plane[2] = field.fog[0] = 1.f;
        field.draws[0] = {0u, 4u, 1u, true};
        field.draws[1] = {1u, 9u, 12u, false};
        field.draws[2] = {2u, 9u, 4u, false};
        field.draw_count = 3;
        identity(water.modelview);
        water.modelview[14] = -5.f;
        water.eye_position[2] = 5.f;
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            water.normal_matrix[axis][axis] = 1.f;
            water.normal_scale_glint[axis] = 2.f;
        }
        water.normal_scale_glint[3] = 1.f;
        water.optics[1] = 1.f;
        water.optics[2] = .7f;
        water.light_direction[2] = 1.f;
        water.normal_maps = {normal.get(), normal.get()};
        water.water_far[0] = 10000.f;
        water.parameters[2] = 1.f;
        water.underwater = false;

        ALMetalWorldSimpleBatch water_marker;
        water_marker.water_surface = true;
        const std::array<ALMetalWorldSimpleBatch, 3> batches{
            shiny, water_marker, alpha};
        input.scene.water_surface = &water;
        std::array<ALMetalWorldReflectionProbeConstants, 2> packs{
            input.probes.constants, input.probes.constants};
        for (auto& parameters : packs[1].ref_params)
        {
            parameters[0] *= .125f;
            parameters[1] *= .25f;
        }
        std::array<Planes, 2> expected, water_only, alpha_only;
        for (unsigned view = 0; view < expected.size(); ++view)
        {
            input.probes.constants = packs[view];
            input.scene.batches = &water_marker;
            input.scene.batch_count = 1;
            water_only[view] = input.render(false).planes;
            input.scene.batches = &alpha;
            input.scene.water_surface = nullptr;
            alpha_only[view] = input.render(false).planes;
            input.scene.batches = batches.data();
            input.scene.batch_count = batches.size();
            input.scene.water_surface = &water;
            expected[view] = input.render(false).planes;
        }
        const Pixel seed{0x2800, 0x2c00, 0x3000, 0x3800};
        unsigned changed_water = 0, changed_alpha = 0;
        for (unsigned y = 0; y < input.cell.height; ++y)
            for (unsigned x = 0; x < input.cell.width; ++x)
            {
                const auto pixel = std::size_t(y) * input.cell.width + x;
                if (y < input.cell.height / 8)
                {
                    for (unsigned view = 0; view < expected.size(); ++view)
                        CHECK(expected[view].color[pixel] == water_only[view].color[pixel]);
                    changed_water += water_only[0].color[pixel] != water_only[1].color[pixel];
                }
                if (x >= 3 * input.cell.width / 4 &&
                    y >= input.cell.height / 4 && y < 3 * input.cell.height / 4 &&
                    alpha_only[0].color[pixel] != seed)
                {
                    for (unsigned view = 0; view < expected.size(); ++view)
                        CHECK(expected[view].color[pixel] == alpha_only[view].color[pixel]);
                    changed_alpha += alpha_only[0].color[pixel] != alpha_only[1].color[pixel];
                }
            }
        REQUIRE(changed_water > 0);
        REQUIRE(changed_alpha > 0);

        struct PendingView
        {
            std::unique_ptr<ALMetalRenderTarget> gbuffer, composed, output;
        };
        std::uint64_t generation = 1000;
        for (bool external_pages : {true, false})
            for (bool separate_commands : {false, true})
                for (bool prepared : {false, true})
                {
                    CAPTURE(external_pages, separate_commands, prepared);
                    auto other = ALMetalWorldGBufferRenderer::create(
                        *f.device, 1, f.renderer->shaderLibrary(), &f.error);
                    INFO(f.error); REQUIRE(other);
                    const auto pages = external_pages
                        ? f.data : std::shared_ptr<ALMetalWorldFrameData>{};
                    f.renderer->setFrameData(pages);
                    other->setFrameData(pages);
                    f.data->beginFrame(generation++);
                    std::array<PendingView, 2> pending;
                    for (auto& view : pending)
                    {
                        view.gbuffer = input.seeded(true);
                        view.composed = input.seeded(false);
                    }
                    REQUIRE(f.renderer->beginCommandBatch("Pending water-probe view A", &f.error));
                    if (separate_commands)
                        REQUIRE(other->beginCommandBatch("Pending water-probe view B", &f.error));
                    const auto first = f.renderer->commandBatchCompletion();
                    const auto second = separate_commands ? other->commandBatchCompletion() : first;
                    REQUIRE(first); REQUIRE(second);
                    for (unsigned view = 0; view < pending.size(); ++view)
                    {
                        CHECK_FALSE(first->isComplete());
                        CHECK_FALSE(second->isComplete());
                        input.probes.constants = packs[view];
                        auto scene = input.scene;
                        scene.gbuffer = pending[view].gbuffer.get();
                        scene.composed_lighting = pending[view].composed.get();
                        scene.forward_probe_projection_valid = prepared;
                        auto& owner = separate_commands && view ? *other : *f.renderer;
                        const auto before = f.data->stats();
                        std::uint32_t water_draws = 0;
                        pending[view].output = owner.renderOpaquePostDeferred(scene, &f.error, &water_draws);
                        INFO(f.error); REQUIRE(pending[view].output);
                        CHECK(water_draws == field.draw_count);
                        const auto diagnostics = owner.opaquePostDiagnostics();
                        CHECK(diagnostics.probe_candidate_simple_batches ==
                              (external_pages && prepared ? 1u : 0u));
                        if (external_pages)
                        {
                            // Shiny, water and alpha must rebind one immutable
                            // full pack, not repeat its content-dedup request.
                            const auto after = f.data->stats();
                            CHECK(after.reused_bytes - before.reused_bytes < sizeof(packs[0]));
                            f.data->beginFrame(generation++);
                        }
                    }
                    input.probes.constants = {};
                    if (external_pages)
                        CHECK(f.data->stats().retired_pages >= 2);
                    CHECK_FALSE(first->isComplete());
                    CHECK_FALSE(second->isComplete());
                    if (separate_commands)
                        REQUIRE(other->commitCommandBatch(&f.error));
                    REQUIRE(f.renderer->commitCommandBatch(&f.error));
                    REQUIRE(f.device->waitUntilIdle(&f.error));
                    CHECK(first->isComplete());
                    CHECK(second->isComplete());
                    for (unsigned view = 0; view < pending.size(); ++view)
                        CHECK(f.read(*pending[view].output) == expected[view]);
                }
    }
}
