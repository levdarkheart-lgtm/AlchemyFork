/**
 * @file almetal_probe_candidate_transparency_test.mm
 * @brief Strict full-renderer simple-alpha parity with virtual-eye probe masks.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalworld.h"
#include "../almetalprobecandidates.h"
#include <catch2/catch_test_macros.hpp>
#import <Metal/Metal.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
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
    std::unique_ptr<ALMetalDevice> device=ALMetalDevice::create(&error);
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    Fixture()
    {
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
    std::unique_ptr<ALMetalTexture> radiance,irradiance,shadow;
    std::array<ALMetalWorldSimpleBatch,2> batches;
    std::array<float,16> palette{{0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0}};
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
        std::array<std::array<float,4>,8> encoded=positions,normals{},weights{};
        std::array<std::array<float,2>,8> uv{};
        std::array<std::array<std::uint8_t,4>,8> colors{};
        for(unsigned i=0;i<8;++i)
        {
            encoded[i][3]=std::bit_cast<float>(std::uint32_t(i/4));
            normals[i]={{0,0,1,0}}; weights[i]={{.5f,0,0,0}};
            uv[i]={{float(i&1u),float((i>>1u)&1u)}};
            colors[i]={{255,std::uint8_t(i<4?224:255),255,std::uint8_t(i<4?144:112)}};
        }
        constexpr std::size_t normal_offset=sizeof(encoded),uv_offset=normal_offset+sizeof(normals);
        constexpr std::size_t color_offset=uv_offset+sizeof(uv),weight_offset=(color_offset+sizeof(colors)+15)&~std::size_t(15);
        std::array<std::uint8_t,weight_offset+sizeof(weights)> bytes{};
        std::memcpy(bytes.data(),encoded.data(),sizeof(encoded));
        std::memcpy(bytes.data()+normal_offset,normals.data(),sizeof(normals));
        std::memcpy(bytes.data()+uv_offset,uv.data(),sizeof(uv));
        std::memcpy(bytes.data()+color_offset,colors.data(),sizeof(colors));
        std::memcpy(bytes.data()+weight_offset,weights.data(),sizeof(weights));
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
            batch.color_offset=color_offset; batch.index_offset=i*6*sizeof(std::uint16_t); batch.index_count=6;
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
void positive(const Planes& planes)
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
    CHECK(std::any_of(planes.depth.begin(),planes.depth.end(),[first=planes.depth.front()](auto value){return value!=first;}));
}
void encoded(const Inputs::Rendered& value,bool shadow,unsigned count)
{
    CHECK(value.diagnostics.probe_candidate_simple_batches==(shadow?0u:count));
    CHECK(value.diagnostics.probe_candidate_simple_shadow_batches==(shadow?count:0u));
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
        CHECK(custom.planes==custom_generic.planes);encoded(custom,false,0);
        CHECK(f.data->stats().candidate_preparations==custom_before.candidate_preparations);
        CHECK(f.data->stats().candidate_reuses==custom_before.candidate_reuses);
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
