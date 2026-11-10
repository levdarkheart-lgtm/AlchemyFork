/**
 * @file almetal_classic_avatar_pose_test.mm
 * @brief Classic scalar-weight skinning ignores vec3 position padding.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetaldevice.h"
#include "../almetalresource.h"
#include "../almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace
{
using V4 = std::array<float, 4>;
using Affine = std::array<V4, 3>;
constexpr unsigned columns=6, rows=5, prefix=2, count=prefix+columns*rows+1;
constexpr unsigned width=64, height=64;
constexpr std::size_t positions_offset=0;
constexpr std::size_t normals_offset=count*sizeof(V4);
constexpr std::size_t uv_offset=normals_offset+count*sizeof(V4);
constexpr std::size_t weights_offset=uv_offset+count*2*sizeof(float);
constexpr std::size_t cloth_offset=(weights_offset+count*sizeof(float)+15)&~std::size_t{15};
constexpr std::size_t bytes_count=cloth_offset+count*sizeof(V4);
using Snapshot = std::array<std::vector<std::uint8_t>, 5>;

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalBuffer> indices;
    std::unique_ptr<ALMetalTexture> diffuse;
    std::array<Affine,15> joints{};
    std::array<V4,45> palette{};
    std::array<V4,count> source{}, posed{}, normals{};
    std::array<float,count> weights{};
    unsigned index_count=0;
    Fixture()
    {
        device=ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer=ALMetalWorldGBufferRenderer::create(*device,&error); INFO(error); REQUIRE(renderer);
        for(unsigned joint=0;joint<15;++joint)
        {
            // Distinct, exactly representable affine coefficients. Scaling,
            // shear and all three translations vary across the full15 joints.
            auto& a=joints[joint];
            a[0]={1.f+(int(joint%3)-1)*.125f, (joint%2?.0625f:-.0625f), 0,
                  (int(joint)-7)*.0078125f};
            a[1]={(joint%2?-.03125f:.03125f),1.f+(int(joint%5)-2)*.0625f,0,
                  (int(joint%4)-1)*.015625f};
            a[2]={0,0,1.f+(int(joint%3)-1)*.0625f,.25f+joint*.00390625f};
            for(unsigned axis=0;axis<3;++axis) palette[15*axis+joint]=a[axis];
        }
        for(unsigned vertex=0;vertex<count;++vertex)
        {
            normals[vertex]={0,0,1,123};
            if(vertex<prefix || vertex>=prefix+columns*rows)
                source[vertex]={32,32,0,1}; // Unused prefix/suffix catch index/base errors.
            else
            {
                const unsigned local=vertex-prefix;
                source[vertex]={-.625f+(local%columns)*.25f,
                                -.5f+(local/columns)*.25f,0,1};
            }
            const unsigned joint=(vertex-prefix+15*count)%15;
            const float blend=joint==14 ? (vertex%2?.75f:0.f) : (vertex%2?.25f:.75f);
            weights[vertex]=joint+blend;
            // Independent original oracle: transform each endpoint using
            // authored 3x4 matrices with homogeneous W=1, THEN interpolate.
            // It does not unpack the native45-row palette/helper.
            for(unsigned axis=0;axis<3;++axis)
            {
                const auto transform=[&](unsigned j)
                {
                    const auto& row=joints[j][axis];
                    return row[0]*source[vertex][0]+row[1]*source[vertex][1]+
                           row[2]*source[vertex][2]+row[3];
                };
                const float a=transform(joint), b=transform(joint==14?14:joint+1);
                posed[vertex][axis]=a+(b-a)*blend;
            }
            posed[vertex][3]=1;
        }
        std::vector<std::uint16_t> ids{0,0,0}; // Nonzero index offset; guards are offscreen.
        for(unsigned y=0;y<rows-1;++y) for(unsigned x=0;x<columns-1;++x)
        {
            const std::uint16_t a=prefix+y*columns+x,b=a+1,c=a+columns,d=c+1;
            for(auto i:std::array<std::uint16_t,6>{a,b,c,b,d,c}) ids.push_back(i);
        }
        index_count=static_cast<unsigned>(ids.size()-3);
        ALMetalBufferDescriptor bd; bd.length=ids.size()*sizeof(ids[0]);
        bd.storage=ALMetalStorageMode::shared; bd.label="Classic pose nonzero-offset indices";
        indices=ALMetalBuffer::create(*device,bd,&error); REQUIRE(indices);
        REQUIRE(indices->write(0,ids.data(),bd.length,&error));
        ALMetalTextureDescriptor td; td.width=td.height=1;
        td.format=ALMetalPixelFormat::rgba8_unorm_srgb;
        td.usage=ALMetalTextureUsage::shader_read; td.label="Classic skin authored color";
        diffuse=ALMetalTexture::create(*device,td,&error); REQUIRE(diffuse);
        const std::array<std::uint8_t,4> skin{192,128,64,255};
        auto command=device->makeCommandBuffer("Classic pose texture upload",&error); REQUIRE(command);
        REQUIRE(diffuse->upload(*command,0,0,{},skin.data(),4,&error));
        REQUIRE(command->commitAndWait(&error));
    }
    std::unique_ptr<ALMetalBuffer> vertices(bool cpu_pose, unsigned padding)
    {
        auto positions=cpu_pose?posed:source;
        if(!cpu_pose) for(unsigned vertex=0;vertex<count;++vertex)
            positions[vertex][3]=padding==0?1.f:padding==1?0.f:(vertex%2?-17.f:4096.f);
        std::array<std::uint8_t,bytes_count> data{};
        std::memcpy(data.data()+positions_offset,positions.data(),sizeof(positions));
        std::memcpy(data.data()+normals_offset,normals.data(),sizeof(normals));
        std::memcpy(data.data()+weights_offset,weights.data(),sizeof(weights));
        const V4 clothing{.015625f,.03125f,.0078125f,.5f};
        for(unsigned vertex=0;vertex<count;++vertex)
            std::memcpy(data.data()+cloth_offset+vertex*sizeof(V4),clothing.data(),sizeof(clothing));
        ALMetalBufferDescriptor bd; bd.length=data.size(); bd.storage=ALMetalStorageMode::shared;
        bd.label="Classic15-joint scalar stream with padding canaries";
        auto result=ALMetalBuffer::create(*device,bd,&error); REQUIRE(result);
        REQUIRE(result->write(0,data.data(),data.size(),&error)); return result;
    }
    void wind(float* w,float* s,float* g,float* depth)
    {
        const V4 wind{.25f,.125f,.5f,.375f}, wave{1,2,.5f,.25f}, gravity{0,0,-1,0};
        std::memcpy(w,wind.data(),sizeof(wind)); std::memcpy(s,wave.data(),sizeof(wave));
        std::memcpy(g,gravity.data(),sizeof(gravity)); depth[2]=1;
    }
    Snapshot color(bool cpu_pose,unsigned padding,bool reverse,bool cloth)
    {
        auto vb=vertices(cpu_pose,padding);
        ALMetalWorldSimpleBatch b; b.vertices=vb.get(); b.vertex_count=count;
        b.normal_offset=normals_offset; b.texcoord_offset=uv_offset;
        b.indices=indices.get(); b.index_offset=3*sizeof(std::uint16_t); b.index_count=index_count;
        b.texture_count=1; b.diffuse_textures[0]=diffuse.get();
        b.avatar_rigid=cpu_pose; b.avatar_skinning=!cpu_pose;
        b.alpha_mask=true; b.use_vertex_color=false; b.alpha_cutoff_bias=false; b.alpha_cutoff=.2f;
        for(unsigned axis=0;axis<4;++axis)
        { b.modelview_projection[axis*4+axis]=1; b.modelview[axis*4+axis]=1;
          if(axis<3) b.normal_matrix[axis][axis]=1; }
        if(!cpu_pose)
        { b.weight_offset=weights_offset; b.skin_palette=palette[0].data();
          b.skin_palette_float_count=45*4; b.skin_joint_count=15; }
        if(cloth)
        { b.avatar_cloth=true; b.cloth_offset=cloth_offset;
          wind(b.avatar_wind,b.avatar_sin_wave,b.avatar_gravity,b.avatar_cloth_depth); }
        ALMetalWorldGBufferScene s; s.batches=&b; s.batch_count=1;
        s.width=width; s.height=height; s.hdr=s.normal_hdr=true; s.reverse_z=reverse;
        auto target=renderer->render(s,&error); INFO(error); REQUIRE(target);
        Snapshot pixels;
        for(unsigned a=0;a<4;++a)
        { const auto* t=target->colorTexture(a); const auto row=width*t->bytesPerPixel();
          pixels[a].resize(height*row); REQUIRE(t->readback(*device,0,0,{},pixels[a].data(),row,&error)); }
        pixels[4].resize(width*height*sizeof(float));
        REQUIRE(target->depthStencilTexture()->readback(*device,0,0,{},pixels[4].data(),width*sizeof(float),&error));
        unsigned covered=0; bool finite=true;
        for(unsigned p=0;p<width*height;++p)
        { float z; std::memcpy(&z,pixels[4].data()+p*sizeof(float),sizeof(float));
          finite=finite&&std::isfinite(z); if(z!=(reverse?0.f:1.f)) ++covered; }
        CHECK(finite);
        CHECK(covered>128); CHECK(covered<width*height);
        return pixels;
    }
    std::vector<std::uint8_t> shadow(bool cpu_pose,unsigned padding,bool reverse,bool cloth)
    {
        auto vb=vertices(cpu_pose,padding);
        ALMetalWorldSpotShadowBatch b; b.avatar=true; b.vertices=vb.get(); b.vertex_count=count;
        b.indices=indices.get(); b.index_offset=3*sizeof(std::uint16_t); b.index_count=index_count;
        b.texcoord_offset=uv_offset;
        for(unsigned axis=0;axis<4;++axis) b.modelview_projection[axis*4+axis]=1;
        if(!cpu_pose)
        { b.weight_offset=weights_offset; b.skin_palette=palette[0].data();
          b.skin_palette_float_count=45*4; b.skin_joint_count=15; }
        if(cloth)
        { b.avatar_cloth=true; b.normal_offset=normals_offset; b.cloth_offset=cloth_offset;
          wind(b.avatar_wind,b.avatar_sin_wave,b.avatar_gravity,b.avatar_cloth_depth); }
        ALMetalWorldSpotShadowScene s; s.batches=&b; s.batch_count=1;
        s.width=width; s.height=height; s.reverse_z=reverse;
        ALMetalTextureDescriptor td; td.width=width; td.height=height;
        td.type=ALMetalTextureType::texture_2d_array; td.array_length=1;
        td.format=ALMetalPixelFormat::r32_float;
        td.usage=ALMetalTextureUsage::render_target|ALMetalTextureUsage::shader_read;
        td.label="Classic avatar color/shadow padding crosscheck";
        auto target=ALMetalTexture::create(*device,td,&error); REQUIRE(target);
        const bool encoded=renderer->renderSpotShadow(*target,0,s,&error);
        INFO(error); REQUIRE(encoded);
        std::vector<std::uint8_t> pixels(width*height*sizeof(float));
        REQUIRE(target->readback(*device,0,0,{},pixels.data(),width*sizeof(float),&error));
        return pixels;
    }
};
}

TEST_CASE("almetal: classic15-joint poses ignore stored position padding in color and shadow",
          "[almetal][classic-avatar-pose]")
{
    Fixture f;
    for(bool reverse:{false,true})
    {
        CAPTURE(reverse);
        const auto expected=f.color(true,0,reverse,false);
        const auto original_w1=f.color(false,0,reverse,false);
        CHECK(bool(original_w1==expected));
        for(unsigned padding:{1u,2u})
        { CAPTURE(padding); CHECK(bool(f.color(false,padding,reverse,false)==expected)); }
        const auto clothed=f.color(false,0,reverse,true);
        CHECK(bool(clothed!=original_w1)); // Real nonzero wind; not a zero-motion cloth fixture.
        for(unsigned padding:{1u,2u}) CHECK(bool(f.color(false,padding,reverse,true)==clothed));
        const auto shadow_expected=f.shadow(true,0,reverse,false);
        const auto shadow_w1=f.shadow(false,0,reverse,false);
        CHECK(bool(shadow_w1==shadow_expected));
        for(unsigned padding:{1u,2u}) CHECK(bool(f.shadow(false,padding,reverse,false)==shadow_expected));
        const auto shadow_clothed=f.shadow(false,0,reverse,true);
        CHECK(bool(shadow_clothed!=shadow_w1));
        for(unsigned padding:{1u,2u}) CHECK(bool(f.shadow(false,padding,reverse,true)==shadow_clothed));
    }
}
