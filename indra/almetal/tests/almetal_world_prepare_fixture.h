#pragma once
/**
 * @file almetal_world_prepare_test.mm
 * @brief Ordered owned_input/direct native world parity and input lifetime tests.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalworldprepare.h"
#include "alnativeframebudget.h"
#include <Metal/Metal.h>
#include <dispatch/dispatch.h>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <chrono>
#include <iostream>
#include <limits>
#include <thread>

namespace
{
constexpr unsigned side = 32;
void identity(float* matrix)
{
    std::fill_n(matrix, 16, 0.f);
    for (unsigned axis=0; axis<4; ++axis) matrix[axis*5]=1.f;
}
struct Resources
{
    std::vector<std::shared_ptr<const ALMetalBuffer>> buffers;
    std::vector<std::shared_ptr<const ALMetalTexture>> textures;
};
struct Pixels
{
    std::array<std::vector<std::uint8_t>,4> colors;
    std::vector<std::uint32_t> depth;
    std::vector<std::uint8_t> stencil;
    bool operator==(const Pixels&) const = default;
};
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::shared_ptr<Resources> resources=std::make_shared<Resources>();
    struct Geometry
    {
        float positions[4][4]{{-.8f,-.8f,.25f,1},{.8f,-.8f,.25f,1},{0,.8f,.25f,1},{0,0,.25f,1}};
        float normals[4][4]{{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}};
        float uv[4][2]{};
        std::uint8_t colors[4][4]{{255,180,128,255},{255,180,128,255},{255,180,128,255},{255,180,128,255}};
        float tangents[4][4]{{1,0,0,1},{1,0,0,1},{1,0,0,1},{1,0,0,1}};
        float weights[4][4]{{.5f,0,0,0},{.5f,0,0,0},{.5f,0,0,0},{.5f,0,0,0}};
        float avatar_weights[4]{};
        float cloth[4][4]{};
    } geometry;
    const ALMetalBuffer *vertices=nullptr,*indices=nullptr,*terrain_vertices=nullptr;
    const ALMetalTexture *color=nullptr,*flat=nullptr,*orm=nullptr,*impostor_normal=nullptr,*height=nullptr,*composition=nullptr;
    std::array<float,16> attachment_pose{{0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0}};
    std::array<float,180> avatar_pose{};

    Fixture()
    {
        device=ALMetalDevice::create(&error);INFO(error);REQUIRE(device);
        renderer=ALMetalWorldGBufferRenderer::create(*device,&error);INFO(error);REQUIRE(renderer);
        data=ALMetalWorldFrameData::create(*device,&error);INFO(error);REQUIRE(data);
        data->beginFrame(1);renderer->setFrameData(data);
        vertices=buffer(&geometry,sizeof(geometry));
        const std::array<std::uint16_t,3> values{{0,1,2}};indices=buffer(values.data(),sizeof(values));
        const float control[4][4]{{0,0,0,1},{2,0,0,1},{2,2,0,1},{0,2,0,1}};
        terrain_vertices=buffer(control,sizeof(control));
        const std::uint8_t rgba[4]{180,80,40,255},normal[4]{128,128,255,255},orm_pixel[4]{255,128,64,255};
        const std::uint16_t normal16[4]{32768,32768,65535,65535};
        const std::array<float,64> height_pixels{};
        const std::array<std::uint16_t,128> composition_pixels{};
        color=texture(ALMetalPixelFormat::rgba8_unorm_srgb,1,rgba,4);
        flat=texture(ALMetalPixelFormat::rgba8_unorm,1,normal,4);
        orm=texture(ALMetalPixelFormat::rgba8_unorm,1,orm_pixel,4);
        impostor_normal=texture(ALMetalPixelFormat::rgba16_unorm,1,normal16,8);
        height=texture(ALMetalPixelFormat::r32_float,8,height_pixels.data(),8*sizeof(float));
        composition=texture(ALMetalPixelFormat::rg16_float,8,composition_pixels.data(),8*4);
        for(unsigned joint=0;joint<15;++joint)
        {
            avatar_pose[joint*4]=1.f;
            avatar_pose[(15+joint)*4+1]=1.f;
            avatar_pose[(30+joint)*4+2]=1.f;
        }
    }
    const ALMetalBuffer* buffer(const void* bytes,std::size_t length)
    {
        ALMetalBufferDescriptor descriptor;descriptor.length=length;descriptor.storage=ALMetalStorageMode::shared;
        std::shared_ptr<ALMetalBuffer> value=ALMetalBuffer::create(*device,descriptor,&error);
        INFO(error);REQUIRE(value);REQUIRE(value->write(0,bytes,length,&error));
        resources->buffers.push_back(value);return value.get();
    }
    const ALMetalTexture* texture(ALMetalPixelFormat format,unsigned width,const void* bytes,std::size_t row)
    {
        ALMetalTextureDescriptor descriptor;descriptor.width=descriptor.height=width;descriptor.format=format;
        descriptor.storage=ALMetalStorageMode::private_gpu;descriptor.usage=ALMetalTextureUsage::shader_read;
        std::shared_ptr<ALMetalTexture> value=ALMetalTexture::create(*device,descriptor,&error);INFO(error);REQUIRE(value);
        auto command=device->makeCommandBuffer("Prepared world fixture upload",&error);REQUIRE(command);
        REQUIRE(value->upload(*command,0,0,{},bytes,row,&error));REQUIRE(command->commitAndWait(&error));
        resources->textures.push_back(value);return value.get();
    }
    ALMetalWorldSimpleBatch batch(unsigned family)
    {
        ALMetalWorldSimpleBatch b;b.vertices=vertices;b.indices=indices;b.vertex_count=4;b.index_count=3;
        b.normal_offset=offsetof(Geometry,normals);b.texcoord_offset=offsetof(Geometry,uv);
        b.color_offset=offsetof(Geometry,colors);b.tangent_offset=offsetof(Geometry,tangents);
        b.texture_count=1;b.diffuse_textures[0]=color;identity(b.modelview_projection);identity(b.modelview);
        for(unsigned axis=0;axis<3;++axis)b.normal_matrix[axis][axis]=b.rigged_normal_matrix[axis][axis]=1.f;
        if(family==1)
        {
            b.material_slot_count=1;b.material_alpha_mode=0;
            auto& slot=b.materialSlots()[0];slot.normal_texture=flat;slot.specular_texture=orm;
            slot.specular_color[0]=.35f;slot.env_intensity=.2f;
        }
        if(family==2)
        {
            b.pbr_slot_count=1;auto& slot=b.pbrSlots()[0];slot.normal_texture=flat;slot.orm_texture=orm;
            slot.emissive_texture=color;slot.roughness_factor=.4f;slot.metallic_factor=.6f;
        }
        if(family==3)b.normal_texture=flat;
        if(family==4)
        {
            b.weight_offset=offsetof(Geometry,weights);b.skin_palette=attachment_pose.data();
            b.skin_palette_float_count=attachment_pose.size();b.skin_joint_count=1;
        }
        if(family==5 || family==6)
        {
            b.avatar_rigid=family==5;b.avatar_skinning=family==6;b.alpha_mask=true;
            b.use_vertex_color=false;b.alpha_cutoff_bias=false;b.alpha_cutoff=.2f;
            if(family==6)
            {
                b.weight_offset=offsetof(Geometry,avatar_weights);b.skin_palette=avatar_pose.data();
                b.skin_palette_float_count=avatar_pose.size();b.skin_joint_count=15;
            }
        }
        if(family==7 || family==8)
        {
            b=ALMetalWorldSimpleBatch{};b.terrain=true;b.terrain_pbr=family==8;
            b.vertices=terrain_vertices;b.vertex_count=4;b.texture_count=4;
            std::fill_n(b.diffuse_textures.begin(),4,color);b.terrain_height_texture=height;
            b.terrain_composition_texture=composition;b.terrain_alpha_ramp_texture=flat;
            b.terrain_region_scale=4;b.terrain_detail_scale=1;
            std::fill_n(b.terrain_tessellation_factors,6,std::uint16_t{0x4400});
            b.modelview_projection[0]=b.modelview_projection[5]=.8f;
            b.modelview_projection[12]=b.modelview_projection[13]=-.8f;
            b.modelview_projection[14]=.25f;b.modelview_projection[15]=1;
            identity(b.terrain_modelview);
            for(unsigned axis=0;axis<3;++axis)b.normal_matrix[axis][axis]=1;
            if(b.terrain_pbr)
            {
                b.pbr_slot_count=4;
                for(unsigned slot=0;slot<4;++slot)
                {
                    auto& p=b.pbrSlots()[slot];p.normal_texture=flat;p.orm_texture=orm;p.emissive_texture=color;
                    b.terrain_uv_transform[slot][0]=b.terrain_uv_transform[slot][3]=1;
                    b.terrain_normal_axes[slot][0]=1;
                }
            }
        }
        if(family==9)
        {
            b=ALMetalWorldSimpleBatch{};b.impostor=true;
            b.impostor_textures={color,orm,impostor_normal,flat};
            b.impostor_left[0]=b.impostor_up[1]=.75f;b.impostor_center[2]=.25f;
            identity(b.modelview_projection);identity(b.modelview);
            for(unsigned axis=0;axis<3;++axis)b.impostor_normal_rotation[axis][axis]=1;
        }
        return b;
    }
    ALMetalWorldGBufferScene scene()
    {
        ALMetalWorldGBufferScene value;value.width=value.height=side;value.normal_hdr=true;
        value.clear_color[0]=.1f;value.clear_color[1]=.2f;value.clear_color[2]=.3f;return value;
    }
    Pixels read(ALMetalRenderTarget& target)
    {
        Pixels p;
        for(unsigned slot=0;slot<4;++slot)
        {
            const auto format=target.colorTexture(slot)->descriptor().format;
            const std::size_t bpp=format==ALMetalPixelFormat::rgba16_unorm?8:4;
            p.colors[slot].resize(side*side*bpp);
            REQUIRE(target.colorTexture(slot)->readback(*device,0,0,{},p.colors[slot].data(),side*bpp,&error));
        }
        p.depth.resize(side*side);p.stencil.resize(side*side);
        REQUIRE(target.depthStencilTexture()->readback(*device,0,0,{},p.depth.data(),side*4,&error));
        constexpr std::size_t row=256;
        id<MTLDevice> native=(__bridge id<MTLDevice>)device->nativeHandle();
        id<MTLBuffer> plane=[native newBufferWithLength:row*side options:MTLResourceStorageModeShared];REQUIRE(plane);
        auto command=[(__bridge id<MTLCommandQueue>)device->nativeCommandQueue() commandBuffer];
        auto blit=[command blitCommandEncoder];REQUIRE(blit);
        [blit copyFromTexture:(__bridge id<MTLTexture>)target.depthStencilTexture()->nativeHandle()
            sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(side,side,1)
            toBuffer:plane destinationOffset:0 destinationBytesPerRow:row destinationBytesPerImage:row*side
            options:MTLBlitOptionStencilFromDepthStencil];
        [blit endEncoding];[command commit];[command waitUntilCompleted];
        REQUIRE(command.status==MTLCommandBufferStatusCompleted);
        for(unsigned y=0;y<side;++y)std::memcpy(p.stencil.data()+y*side,static_cast<const std::uint8_t*>(plane.contents)+y*row,side);
        return p;
    }
    std::shared_ptr<const ALMetalWorldViewInputs> adopt(ALMetalWorldGBufferScene s,
        std::vector<ALMetalWorldSimpleBatch> batches,std::vector<ALMetalWorldVisibilityQuery> queries={})
    {
        auto inputs=ALMetalWorldViewInputs::adopt(s,std::move(batches),std::move(queries),resources,7,&error);
        INFO(error);REQUIRE(inputs);return inputs;
    }

};
}
