/**
 * @file almetal_terrain_pass_publication_test.inl
 * @brief Native terrain pass sharing, exact resources and complete path costs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included by the registered owned scalar world input fixture.
 */
#include "almetal_world_inputs_fixture.h"
#include "../almetalworldmaterialdata.h"
#include <optional>

namespace
{
namespace TerrainPassPublicationTest
{
struct TerrainFixture
{
    Fixture f;
    unsigned patches,grid;
    const ALMetalBuffer* controls=nullptr;
    const ALMetalTexture* detail=nullptr;
    const ALMetalTexture* paint=nullptr;
    const ALMetalTexture* overlay=nullptr;
    explicit TerrainFixture(unsigned count):patches(count),grid(count<=1?1:count<=4?2:count<=16?4:count<=64?8:16)
    {
        REQUIRE(patches>0);REQUIRE(grid*grid>=patches);
        // Patches occupy distinct region-space rectangles. A single common
        // camera maps the complete region into the same viewport in both arms.
        std::vector<std::array<float,16>> points(patches);
        for(unsigned patch=0;patch<patches;++patch)
        {
            const float x=static_cast<float>(patch%grid)*2.f,y=static_cast<float>(patch/grid)*2.f;
            points[patch]={x,y,0,1,x+2,y,0,1,x+2,y+2,0,1,x,y+2,0,1};
        }
        controls=f.buffer(points.data(),points.size()*sizeof(points[0]));
        const std::array<std::uint8_t,16> pixels{
            220,40,20,255,20,180,60,255,30,60,210,255,170,150,30,255};
        detail=f.texture(ALMetalPixelFormat::rgba8_unorm_srgb,2,pixels.data(),8);
        const std::array<std::uint8_t,4> paint_pixel{255,0,0,0},overlay_pixel{80,140,200,96};
        paint=f.texture(ALMetalPixelFormat::rgba8_unorm,1,paint_pixel.data(),4);
        overlay=f.texture(ALMetalPixelFormat::rgba8_unorm_srgb,1,overlay_pixel.data(),4);
    }
    std::vector<ALMetalWorldSimpleBatch> batches(bool pbr,float shift=0.f,unsigned limit=0)
    {
        const auto count=limit?limit:patches;
        std::vector<ALMetalWorldSimpleBatch> result;result.reserve(count);
        for(unsigned patch=0;patch<count;++patch)
        {
            auto batch=f.batch(pbr?8:7);batch.vertices=controls;
            batch.position_offset=patch*16*sizeof(float);
            batch.modelview_projection[0]=batch.modelview_projection[5]=.8f/static_cast<float>(grid);
            batch.modelview_projection[12]=-.8f+shift;batch.modelview_projection[13]=-.8f;
            batch.terrain_region_scale=static_cast<float>(grid)*2.f;batch.terrain_detail_scale=.5f;
            std::fill_n(batch.diffuse_textures.begin(),4,detail);
            if(pbr)for(unsigned slot=0;slot<4;++slot)
            {
                auto& value=batch.pbrSlots()[slot];value.emissive_texture=detail;
                value.roughness_factor=.2f+.15f*slot;value.metallic_factor=.1f*slot;
                value.emissive_color[0]=.02f*slot;value.emissive_color[1]=.01f*slot;
                batch.terrain_uv_offset[slot][0]=.125f*slot;
            }
            result.push_back(std::move(batch));
        }
        return result;
    }
    ALMetalWorldGBufferScene scene(bool reverse=false,bool hdr=false,bool normal_hdr=true)
    {
        auto scene=f.scene();scene.reverse_z=reverse;scene.hdr=hdr;scene.normal_hdr=normal_hdr;
        scene.source_owner=f.resources;return scene;
    }
    std::vector<ALMetalWorldVisibilityQuery> queries()
    {
        std::vector<ALMetalWorldVisibilityQuery> result(2);
        for(unsigned i=0;i<result.size();++i)
        {
            auto& query=result[i];identity(query.modelview_projection);
            query.center[0]=query.center[1]=.5f;query.center[2]=.75f;
            query.half_extent[0]=query.half_extent[1]=query.half_extent[2]=.08f;
            query.batch_offset=i?patches:patches/2;query.fan=7;query.count_samples=1;
        }
        return result;
    }
};
std::vector<ALMetalWorldSimpleBatch> install(
    const TerrainFixture& fixture,const std::shared_ptr<const ALMetalWorldTerrainPass>& pass,bool verify=true)
{
    std::vector<ALMetalWorldSimpleBatch> result;result.reserve(fixture.patches);
    for(unsigned patch=0;patch<fixture.patches;++patch)
    {
        ALMetalWorldSimpleBatch batch;batch.vertices=fixture.controls;batch.vertex_count=4;
        batch.position_offset=patch*16*sizeof(float);
        std::fill_n(batch.terrain_tessellation_factors,6,std::uint16_t{0x4400});
        if(patch&1u)batch.terrain_tessellation_factors[4]=0x4000;
        pass->install(batch);
        if(verify)
        {
            REQUIRE(pass->matches(batch));
            CHECK(batch.vertices==fixture.controls);CHECK(batch.position_offset==patch*16*sizeof(float));
            CHECK(batch.terrain_tessellation_factors[4]==((patch&1u)?0x4000:0x4400));
        }
        result.push_back(std::move(batch));
    }
    return result;
}
void vary(TerrainFixture& fixture,std::vector<ALMetalWorldSimpleBatch>& batches,unsigned variant)
{
    for(unsigned patch=0;patch<batches.size();++patch)
    {
        auto& batch=batches[patch];
        if(patch&1u)batch.terrain_tessellation_factors[4]=0x4000;
        if(variant&1u)
        {
            batch.terrain_smoothing=true;batch.terrain_triplanar=false;
            batch.terrain_detail_phase[0]=.125f;batch.terrain_detail_phase[1]=.25f;
            batch.terrain_parcel_overlay_texture=fixture.overlay;
            batch.terrain_overlay_wrap=true;batch.terrain_overlay_srgb=false;
        }
        if(variant&2u)
        {
            batch.terrain_hex=true;batch.terrain_blend_wrap=true;
            for(unsigned i=0;i<8;++i)for(unsigned j=0;j<4;++j)
            {batch.terrain_hex_fraction[i][j]=.25f;batch.terrain_hex_base[i][j]=static_cast<std::int32_t>(i)-3;}
            std::fill(batch.terrain_diffuse_srgb.begin(),batch.terrain_diffuse_srgb.end(),false);
            if(batch.terrain_pbr)
            {
                batch.terrain_alpha_ramp_texture=nullptr;batch.terrain_paint_texture=fixture.paint;batch.terrain_paint_type=1;
                batch.terrain_pbr_detail=0;batch.terrain_flat_normals=true;
                std::fill(batch.terrain_emissive_srgb.begin(),batch.terrain_emissive_srgb.end(),false);
            }
        }
        if(batch.terrain_pbr && variant==1)
        {
            batch.alpha_mask=true;batch.terrain_pbr_detail=-2;
            for(auto& slot:batch.pbrSlots()) {slot.alpha_cutoff=.2f;slot.base_color_factor[3]=.75f;}
        }
    }
}
struct Rendered
{
    Pixels image;
    std::array<std::uint64_t,2> queries{};
    bool operator==(const Rendered&) const=default;
};
Rendered render(TerrainFixture& fixture,ALMetalWorldGBufferScene scene,
    const std::vector<ALMetalWorldSimpleBatch>& batches,bool owned_input)
{
    auto queries=fixture.queries();scene.batches=batches.data();scene.batch_count=batches.size();
    scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
    std::shared_ptr<ALMetalVisibilityResultBatch> results;
    std::unique_ptr<ALMetalRenderTarget> target;
    if(owned_input)
    {
        auto ready=fixture.f.adopt(scene,batches,queries);
        target=fixture.f.renderer->renderOwnedInputs(*ready,&fixture.f.error,&results);
    }
    else target=fixture.f.renderer->render(scene,&fixture.f.error,&results);
    INFO(fixture.f.error);REQUIRE(target);REQUIRE(results);
    Rendered output;output.image=fixture.f.read(*target);
    for(unsigned at=0;at<2;++at)REQUIRE(results->wait(at,output.queries[at],&fixture.f.error));
    return output;
}
// A native-only out-of-band image lease lets cost samples retire every CPU
// target before settlement. It is never a typed target-pool reader or source
// certificate, and it performs no readback until after both timing endpoints.
struct NativePixels
{
    std::array<id<MTLTexture>,4> color{};
    id<MTLTexture> depth=nil;
    std::array<std::size_t,4> bpp{};
    explicit NativePixels(const ALMetalRenderTarget& target)
    {
        REQUIRE(target.descriptor().width==side);REQUIRE(target.descriptor().height==side);
        for(unsigned slot=0;slot<4;++slot)
        {const auto* texture=target.colorTexture(slot);REQUIRE(texture);color[slot]=(__bridge id<MTLTexture>)texture->nativeHandle();bpp[slot]=texture->bytesPerPixel();}
        REQUIRE(target.depthStencilTexture());depth=(__bridge id<MTLTexture>)target.depthStencilTexture()->nativeHandle();
    }
    Pixels read(Fixture& f) const
    {
        constexpr std::size_t row=256,plane=row*side;
        id<MTLDevice> device=(__bridge id<MTLDevice>)f.device->nativeHandle();
        id<MTLBuffer> output=[device newBufferWithLength:plane*6 options:MTLResourceStorageModeShared];REQUIRE(output);
        auto command=[(__bridge id<MTLCommandQueue>)f.device->nativeCommandQueue() commandBuffer];REQUIRE(command);
        auto blit=[command blitCommandEncoder];REQUIRE(blit);
        const auto copy=[&](id<MTLTexture> texture,unsigned at,MTLBlitOption option)
        {
            [blit copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                sourceSize:MTLSizeMake(side,side,1) toBuffer:output destinationOffset:at*plane
                destinationBytesPerRow:row destinationBytesPerImage:plane options:option];
        };
        for(unsigned slot=0;slot<4;++slot)copy(color[slot],slot,MTLBlitOptionNone);
        copy(depth,4,MTLBlitOptionDepthFromDepthStencil);copy(depth,5,MTLBlitOptionStencilFromDepthStencil);
        [blit endEncoding];[command commit];[command waitUntilCompleted];REQUIRE(command.status==MTLCommandBufferStatusCompleted);
        const auto* bytes=static_cast<const std::uint8_t*>(output.contents);Pixels result;
        for(unsigned slot=0;slot<4;++slot)
        {
            result.colors[slot].resize(side*side*bpp[slot]);
            for(unsigned y=0;y<side;++y)std::memcpy(result.colors[slot].data()+y*side*bpp[slot],bytes+slot*plane+y*row,side*bpp[slot]);
        }
        result.depth.resize(side*side);result.stencil.resize(side*side);
        for(unsigned y=0;y<side;++y)
        {std::memcpy(result.depth.data()+y*side,bytes+4*plane+y*row,side*sizeof(std::uint32_t));
         std::memcpy(result.stencil.data()+y*side,bytes+5*plane+y*row,side);}
        return result;
    }
};
struct QueueGate
{
    id<MTLSharedEvent> event;
    std::unique_ptr<ALMetalCommandBuffer> blocker;
    explicit QueueGate(Fixture& f)
    {
        event=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(event);
        blocker=f.device->makeCommandBuffer("Terrain pass publication GPU gate",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:event value:1];
        REQUIRE(blocker->commit(&f.error));
    }
    ~QueueGate(){event.signaledValue=1;}
    void release(Fixture& f){event.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));}
};
}
}

TEST_CASE("almetal: one immutable terrain pass preserves classic and PBR patch output",
          "[almetal][terrain-pass-publication]")
{
    using namespace TerrainPassPublicationTest;
    @autoreleasepool
    {
        TerrainFixture t(4);std::uint64_t serial=2;
        for(bool pbr:{false,true})for(unsigned variant=0;variant<4;++variant)
        {
            auto raw=t.batches(pbr);vary(t,raw,variant);
            auto pass=ALMetalWorldTerrainPass::create(raw.front(),t.f.resources->textures,&t.f.error);INFO(t.f.error);REQUIRE(pass);
            CHECK(ALMetalWorldTerrainPass::constantByteSize()==896);REQUIRE(pass->resourceLease());
            auto published=install(t,pass);
            for(bool reverse:{false,true})for(bool hdr:{false,true})for(bool normal:{false,true})
            {
                CAPTURE(pbr,variant,reverse,hdr,normal);auto scene=t.scene(reverse,hdr,normal);
                t.f.data->beginFrame(serial++);
                const auto expected=render(t,scene,raw,false);
                CHECK(std::any_of(expected.image.depth.begin(),expected.image.depth.end(),[&](auto value){return value!=(reverse?0u:0x3f800000u);}));
                const auto before=t.f.data->stats();
                CHECK(render(t,scene,published,false)==expected);
                CHECK(t.f.data->stats().terrain_publications==before.terrain_publications+1);
                CHECK(t.f.data->stats().terrain_published_bytes==before.terrain_published_bytes+896);
                CHECK(render(t,scene,raw,true)==expected);
                auto queries=t.queries();
                auto ready=t.f.adopt(scene,published,queries);
                CHECK(ready->scene().batch_count==published.size());
                std::shared_ptr<ALMetalVisibilityResultBatch> result;
                auto target=t.f.renderer->renderOwnedInputs(*ready,&t.f.error,&result);INFO(t.f.error);REQUIRE(target);REQUIRE(result);
                CHECK(t.f.read(*target)==expected.image);
                for(unsigned at=0;at<2;++at){std::uint64_t count=0;REQUIRE(result->wait(at,count,&t.f.error));CHECK(count==expected.queries[at]);}
            }
        }
    }
}

TEST_CASE("almetal: terrain publication isolates authored slots and falls back for changed common data",
          "[almetal][terrain-pass-publication]")
{
    using namespace TerrainPassPublicationTest;
    @autoreleasepool
    {
        TerrainFixture t(4);auto raw=t.batches(true);vary(t,raw,1);
        auto& held=raw.front().pbrSlots()[0];
        auto pass=ALMetalWorldTerrainPass::create(raw.front(),t.f.resources->textures,&t.f.error);INFO(t.f.error);REQUIRE(pass);
        const auto expected=render(t,t.scene(),raw,false);
        held.roughness_factor=.95f;held.base_color_factor[0]=.1f;
        auto published=install(t,pass);
        CHECK(render(t,t.scene(),published,false)==expected);
        published[0].terrain_detail_scale*=1.5f;CHECK_FALSE(pass->matches(published[0]));
        auto fallback=published;for(auto& batch:fallback)batch.terrain_pass.reset();
        CHECK(render(t,t.scene(),published,false)==render(t,t.scene(),fallback,false));
        published[0].terrain_region_scale=std::numeric_limits<float>::quiet_NaN();
        auto scene=t.scene();scene.batches=published.data();scene.batch_count=published.size();
        CHECK_FALSE(t.f.renderer->render(scene,&t.f.error));
        auto missing=t.f.resources->textures;
        std::erase_if(missing,[&](const auto& texture){return texture.get()==raw.front().terrain_height_texture;});
        CHECK_FALSE(ALMetalWorldTerrainPass::create(raw.front(),missing,&t.f.error));
        auto ordinary=t.f.batch(0);CHECK_FALSE(ALMetalWorldTerrainPass::create(ordinary,t.f.resources->textures,&t.f.error));
        auto invalid=install(t,pass);scene.batches=invalid.data();scene.batch_count=invalid.size();
        invalid[0].position_offset=t.controls->length();REQUIRE(pass->matches(invalid[0]));
        CHECK_FALSE(t.f.renderer->render(scene,&t.f.error));
        invalid=install(t,pass);scene.batches=invalid.data();invalid[0].terrain_tessellation_factors[0]=0;
        REQUIRE(pass->matches(invalid[0]));CHECK_FALSE(t.f.renderer->render(scene,&t.f.error));
    }
}

TEST_CASE("almetal: native terrain readers retain resources without retaining their CPU pass",
          "[almetal][terrain-pass-publication]")
{
    using namespace TerrainPassPublicationTest;
    for(bool owned_input:{false,true})
    @autoreleasepool
    {
        CAPTURE(owned_input);TerrainFixture t(4);
        auto geometry=std::make_shared<Resources>();geometry->buffers=t.f.resources->buffers;
        std::weak_ptr<Resources> weak_geometry=geometry;
        auto geometry_scene=t.scene();geometry_scene.source_owner=geometry;
        auto old_raw=t.batches(true),new_raw=t.batches(true,.35f);
        const auto expected_old=render(t,geometry_scene,old_raw,false),expected_new=render(t,geometry_scene,new_raw,false);
        REQUIRE_FALSE(expected_old.image==expected_new.image);
        auto old_pass=ALMetalWorldTerrainPass::create(old_raw.front(),t.f.resources->textures,&t.f.error);
        auto new_pass=ALMetalWorldTerrainPass::create(new_raw.front(),t.f.resources->textures,&t.f.error);REQUIRE(old_pass);REQUIRE(new_pass);
        std::weak_ptr<const ALMetalWorldTerrainPass> weak_old=old_pass,weak_new=new_pass;
        auto old_batches=install(t,old_pass),new_batches=install(t,new_pass);
        // PPD264: public optional owner replacement cannot remove the private
        // certificate's selected native texture-generation leases.
        for(auto& batch:old_batches)batch.terrain_texture_owner.reset();
        for(auto& batch:new_batches)batch.terrain_texture_owner.reset();
        auto old_inputs=ALMetalWorldViewInputs::adopt(geometry_scene,std::move(old_batches),{},geometry,1,&t.f.error);
        auto new_inputs=ALMetalWorldViewInputs::adopt(geometry_scene,std::move(new_batches),{},geometry,2,&t.f.error);REQUIRE(old_inputs);REQUIRE(new_inputs);
        geometry_scene.source_owner.reset();
        std::weak_ptr<const ALMetalTexture> selected_texture;
        for(const auto& texture:t.f.resources->textures)if(texture.get()==t.detail)selected_texture=texture;
        REQUIRE_FALSE(selected_texture.expired());
        auto library=t.f.data->builtinLibrary(&t.f.error);REQUIRE(library);
        auto probe=ALMetalWorldGBufferRenderer::create(*t.f.device,1,library,&t.f.error);REQUIRE(probe);probe->setFrameData(t.f.data);
        QueueGate gate(t.f);
        REQUIRE(t.f.renderer->beginCommandBatch("Pending immutable terrain old pass",&t.f.error));
        auto old=owned_input?t.f.renderer->renderOwnedInputs(*old_inputs,&t.f.error):t.f.renderer->render(old_inputs->scene(),&t.f.error);
        INFO(t.f.error);REQUIRE(old);auto old_completion=t.f.renderer->commandBatchCompletion();REQUIRE(old_completion);
        REQUIRE(t.f.renderer->commitCommandBatch(&t.f.error));
        t.f.data->beginFrame(2);
        REQUIRE(probe->beginCommandBatch("Pending immutable terrain new pass",&t.f.error));
        auto current=owned_input?probe->renderOwnedInputs(*new_inputs,&t.f.error):probe->render(new_inputs->scene(),&t.f.error);
        INFO(t.f.error);REQUIRE(current);auto new_completion=probe->commandBatchCompletion();REQUIRE(new_completion);
        REQUIRE(probe->commitCommandBatch(&t.f.error));
        old_inputs.reset();new_inputs.reset();old_pass.reset();new_pass.reset();
        CHECK(weak_old.expired());CHECK(weak_new.expired());
        geometry.reset();t.f.resources.reset();
        CHECK_FALSE(weak_geometry.expired());CHECK_FALSE(selected_texture.expired());
        CHECK_FALSE(old_completion->isComplete());CHECK_FALSE(new_completion->isComplete());
        gate.release(t.f);
        CHECK(t.f.read(*old)==expected_old.image);CHECK(t.f.read(*current)==expected_new.image);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while((!weak_geometry.expired()||!selected_texture.expired())&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        CHECK(weak_geometry.expired());CHECK(selected_texture.expired());
    }
}

TEST_CASE("almetal: terrain pass identity handles scene keys abandonment and arena retirement",
          "[almetal][terrain-pass-publication]")
{
    using namespace TerrainPassPublicationTest;
    @autoreleasepool
    {
        TerrainFixture t(1);auto raw=t.batches(true);auto scene=t.scene();
        auto pass=ALMetalWorldTerrainPass::create(raw.front(),t.f.resources->textures,&t.f.error);REQUIRE(pass);
        std::weak_ptr<const ALMetalWorldTerrainPass> weak=pass;
        std::array<std::byte,896> expected{};REQUIRE(pass->copyConstants(scene,expected));
        ALMetalBufferDescriptor descriptor;descriptor.length=expected.size();
        auto output=ALMetalBuffer::create(*t.f.device,descriptor,&t.f.error);REQUIRE(output);
        t.f.data->beginFrame(10);const auto before=t.f.data->stats();
        auto first=t.f.device->makeCommandBuffer("Terrain pass abandoned first publisher",&t.f.error);
        auto second=t.f.device->makeCommandBuffer("Terrain pass actual cached reader",&t.f.error);REQUIRE(first);REQUIRE(second);
        ALMetalWorldDataSlice original,repeated,other_scene;
        REQUIRE(t.f.data->publishTerrainPass(*first,pass,scene,original,&t.f.error));
        REQUIRE(t.f.data->publishTerrainPass(*second,pass,scene,repeated,&t.f.error));
        CHECK(original.buffer==repeated.buffer);CHECK(original.offset==repeated.offset);CHECK(original.length==896);
        CHECK(t.f.data->stats().terrain_publications==before.terrain_publications+1);
        CHECK(t.f.data->stats().terrain_reuses==before.terrain_reuses+1);
        auto reversed=scene;reversed.reverse_z=true;
        REQUIRE(t.f.data->publishTerrainPass(*first,pass,reversed,other_scene,&t.f.error));
        CHECK(other_scene.offset!=original.offset);CHECK(t.f.data->stats().terrain_publications==before.terrain_publications+2);
        CHECK(t.f.data->stats().terrain_published_bytes==before.terrain_published_bytes+2*896);
        REQUIRE(output->copyFrom(*second,*repeated.buffer,repeated.offset,0,repeated.length,&t.f.error));
        first.reset();pass.reset();CHECK(weak.expired());
        t.f.data->beginFrame(11);CHECK(t.f.data->stats().retired_pages==1);
        auto current_pass=ALMetalWorldTerrainPass::create(raw.front(),t.f.resources->textures,&t.f.error);REQUIRE(current_pass);
        auto abandoned=t.f.device->makeCommandBuffer("Terrain pass next-frame canceled reader",&t.f.error);REQUIRE(abandoned);
        ALMetalWorldDataSlice next;
        REQUIRE(t.f.data->publishTerrainPass(*abandoned,current_pass,scene,next,&t.f.error));
        CHECK(next.buffer!=original.buffer);
        abandoned.reset();REQUIRE(second->commitAndWait(&t.f.error));
        std::array<std::byte,896> actual{};REQUIRE(output->read(0,actual.data(),actual.size(),&t.f.error));CHECK(actual==expected);
        t.f.data->beginFrame(12);CHECK(t.f.data->stats().retired_pages==0);
        const auto created=t.f.data->stats().pages_created;
        auto recycled=t.f.device->makeCommandBuffer("Terrain pass completion releases arena pages",&t.f.error);REQUIRE(recycled);
        REQUIRE(t.f.data->publishTerrainPass(*recycled,current_pass,scene,next,&t.f.error));
        CHECK(t.f.data->stats().pages_created==created);
    }
}

TEST_CASE("almetal: complete raw and shared terrain pass path costs",
          "[almetal][.terrain-pass-publication-cost]")
{
    using namespace TerrainPassPublicationTest;
    constexpr unsigned samples=12,warmups=2;
    struct Sample{double cpu=0,encode=0,settle=0,gpu=0;std::uint64_t published=0,terrain_bytes=0,publications=0,reuses=0,gpu_ready=0,source_draws=0,fallback_draws=0;};
    const auto cpu_ns=[]
    {timespec now{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&now)==0);
     return std::uint64_t(now.tv_sec)*1'000'000'000ULL+std::uint64_t(now.tv_nsec);};
    const auto median=[](std::array<double,samples> values)
    {std::sort(values.begin(),values.end());return (values[samples/2-1]+values[samples/2])*.5;};
    for(bool pbr:{false,true})for(unsigned patches:{1u,2u,4u,16u,64u,256u})for(unsigned views:{1u,3u})
    @autoreleasepool
    {
        TerrainFixture t(patches);
        struct Arm
        {
            std::shared_ptr<ALMetalWorldFrameData> data;
            std::unique_ptr<ALMetalWorldGBufferRenderer> main,probe;
            std::uint64_t epoch=0;
        };
        std::array<Arm,2> arms;
        for(auto& arm:arms)
        {
            arm.data=ALMetalWorldFrameData::create(*t.f.device,&t.f.error);REQUIRE(arm.data);
            auto library=arm.data->builtinLibrary(&t.f.error);REQUIRE(library);
            arm.main=ALMetalWorldGBufferRenderer::create(*t.f.device,1,library,&t.f.error);
            arm.probe=ALMetalWorldGBufferRenderer::create(*t.f.device,1,library,&t.f.error);REQUIRE(arm.main);REQUIRE(arm.probe);
            arm.main->setFrameData(arm.data);arm.probe->setFrameData(arm.data);
        }
        std::array<std::array<Sample,samples>,2> measurements;
        for(unsigned sample=0;sample<samples+warmups;++sample)
        {
            std::array<std::array<Pixels,3>,2> images;
            std::array<std::array<std::array<std::uint64_t,2>,3>,2> query_values{};
            for(unsigned order=0;order<2;++order)
            {
                const auto mode=(order+sample)%2;auto& arm=arms[mode];CAPTURE(pbr,patches,views,sample,mode);
                const auto before=arm.data->stats();
                std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> results;
                std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                std::array<std::optional<NativePixels>,3> parity;
                const bool parity_sample=sample==warmups-1||sample==samples+warmups-1;
                Sample value;const auto start=std::chrono::steady_clock::now();const auto cpu_start=cpu_ns();
                arm.data->beginFrame(++arm.epoch);
                for(unsigned view=0;view<views;++view)
                {
                    const auto shift=.01f*view+.002f*(sample&1u);
                    std::vector<ALMetalWorldSimpleBatch> batches;
                    std::shared_ptr<const ALMetalWorldTerrainPass> pass;
                    if(mode)
                    {
                        // Build the same common values once for this view,
                        // then install into real per-patch geometry records.
                        auto common=t.batches(pbr,shift,1);
                        pass=ALMetalWorldTerrainPass::create(common.front(),t.f.resources->textures,&t.f.error);INFO(t.f.error);REQUIRE(pass);
                        batches=install(t,pass,false);
                    }
                    else
                    {
                        batches=t.batches(pbr,shift);
                        for(unsigned patch=0;patch<batches.size();++patch)
                            if(patch&1u)batches[patch].terrain_tessellation_factors[4]=0x4000;
                    }
                    auto queries=t.queries();auto scene=t.scene();
                    auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(batches),std::move(queries),t.f.resources,arm.epoch,&t.f.error);INFO(t.f.error);REQUIRE(inputs);
                    auto& renderer=view?*arm.probe:*arm.main;
                    REQUIRE(renderer.beginCommandBatch("Complete terrain pass publication cost",&t.f.error));
                    targets[view]=renderer.renderOwnedInputs(*inputs,&t.f.error,&results[view]);INFO(t.f.error);REQUIRE(targets[view]);REQUIRE(results[view]);
                    completions[view]=renderer.commandBatchCompletion();
                    std::vector<ALMetalWorldSimpleBatch> recycled;
                    std::vector<ALMetalWorldVisibilityQuery> recycled_queries;
                    REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,recycled,recycled_queries));
                    recycled.clear();recycled_queries.clear();pass.reset();
                    REQUIRE(renderer.commitCommandBatch(&t.f.error));
                }
                for(unsigned view=0;view<views;++view)
                {if(parity_sample)parity[view].emplace(*targets[view]);targets[view].reset();}
                const auto encoded=std::chrono::steady_clock::now();REQUIRE(t.f.device->waitUntilIdle(&t.f.error));
                for(unsigned view=0;view<views;++view)for(unsigned query=0;query<2;++query)
                    REQUIRE(results[view]->wait(query,query_values[mode][view][query],&t.f.error));
                const auto settled=std::chrono::steady_clock::now();
                value.cpu=(cpu_ns()-cpu_start)/1e6;value.encode=std::chrono::duration<double,std::milli>(encoded-start).count();
                value.settle=std::chrono::duration<double,std::milli>(settled-start).count();
                for(unsigned view=0;view<views;++view)
                {ALMetalCommandGPUTime gpu;if(completions[view]->gpuTime(gpu)==ALMetalCommandTimingStatus::ready)
                    {value.gpu+=gpu.elapsed_nanoseconds/1e6;++value.gpu_ready;}}
                const auto after=arm.data->stats();value.published=after.published_bytes-before.published_bytes;
                value.terrain_bytes=after.terrain_published_bytes-before.terrain_published_bytes;
                value.publications=after.terrain_publications-before.terrain_publications;value.reuses=after.terrain_reuses-before.terrain_reuses;
                value.source_draws=after.terrain_source_draws-before.terrain_source_draws;
                value.fallback_draws=after.terrain_source_fallback_draws-before.terrain_source_fallback_draws;
                if(mode){CHECK(value.publications==views);CHECK(value.terrain_bytes==views*896);CHECK(value.source_draws==views*patches);CHECK(value.fallback_draws==0);}
                if(sample>=warmups)measurements[mode][sample-warmups]=value;
                if(parity_sample)
                    for(unsigned view=0;view<views;++view)images[mode][view]=parity[view]->read(t.f);
                std::cout<<"TERRAIN_PASS_RAW pbr="<<pbr<<" patches="<<patches<<" views="<<views<<" mode="<<mode<<" sample="<<sample
                    <<" warmup="<<(sample<warmups)<<" pass_frontier=native_completed_inputs asset_upload_excluded=1 cpu_target_retirement_timed=1 parity_readback_excluded=1"
                    <<" owner_cpu_ms="<<value.cpu<<" capture_adopt_encode_reclaim_ms="<<value.encode<<" commit_settle_ms="<<value.settle
                    <<" gpu_command_sum_ms="<<value.gpu<<" gpu_ready="<<value.gpu_ready<<" published_logical_bytes="<<value.published
                    <<" terrain_published_bytes="<<value.terrain_bytes<<" terrain_publications="<<value.publications<<" terrain_reuses="<<value.reuses
                    <<" source_draws="<<value.source_draws<<" fallback_draws="<<value.fallback_draws<<'\n';
            }
            for(unsigned view=0;view<views;++view)
            {CHECK(query_values[0][view]==query_values[1][view]);
             if(sample==warmups-1||sample==samples+warmups-1)CHECK(images[0][view]==images[1][view]);}
        }
        for(unsigned mode=0;mode<2;++mode)
        {
            std::array<double,samples> cpu,encode,settle,gpu;
            for(unsigned sample=0;sample<samples;++sample)
            {const auto& value=measurements[mode][sample];cpu[sample]=value.cpu;encode[sample]=value.encode;settle[sample]=value.settle;gpu[sample]=value.gpu;}
            std::cout<<"TERRAIN_PASS_COST pbr="<<pbr<<" patches="<<patches<<" views="<<views<<" mode="<<mode<<" samples="<<samples<<" warmups="<<warmups
                <<" pass_frontier=native_completed_inputs asset_upload_excluded=1 source_geometry=unique_region_space_control_points cpu_target_retirement_timed=1 parity_readback_excluded=1"
                <<" owner_cpu_median_ms="<<median(cpu)<<" owner_cpu_max_ms="<<*std::max_element(cpu.begin(),cpu.end())
                <<" encode_reclaim_median_ms="<<median(encode)<<" encode_reclaim_max_ms="<<*std::max_element(encode.begin(),encode.end())
                <<" commit_settle_median_ms="<<median(settle)<<" commit_settle_max_ms="<<*std::max_element(settle.begin(),settle.end())
                <<" gpu_command_sum_median_ms="<<median(gpu)<<" gpu_command_sum_max_ms="<<*std::max_element(gpu.begin(),gpu.end())<<'\n';
        }
    }
}

TEST_CASE("almetal: ordinary source proofs reject a real unrelated terrain owner",
          "[almetal][terrain-pass-publication][PPD-265]")
{
    using namespace TerrainPassPublicationTest;
    @autoreleasepool
    {
        TerrainFixture t(1);auto terrain=t.batches(true);
        auto pass=ALMetalWorldTerrainPass::create(terrain.front(),t.f.resources->textures,&t.f.error);INFO(t.f.error);REQUIRE(pass);
        auto material=t.f.batch(2);
        ALMetalWorldMaterialDataCache cache(*t.f.device);
        REQUIRE(ALMetalWorldMaterialSource::publish(*t.f.device,material,cache,&t.f.error));
        REQUIRE(material.native_material_source->matches(material));
        REQUIRE_FALSE(material.terrain);REQUIRE_FALSE(material.terrain_texture_owner);
        const ALMetalWorldKeyedSource source{{1,2,1},&material};
        auto ordinary=ALMetalWorldCohortSource::seal(source,&t.f.error);INFO(t.f.error);REQUIRE(ordinary);
        // Keep every previously valid material/geometry field and the old
        // optional owner empty. Only the new strong publication field changes.
        material.terrain_pass=pass;
        REQUIRE(material.native_material_source->matches(material));
        CHECK_FALSE(ALMetalWorldCohortSource::seal(source,&t.f.error));
    }
}

TEST_CASE("almetal: abandoning a terrain command retires its private generation leases",
          "[almetal][terrain-pass-publication][PPD-264]")
{
    using namespace TerrainPassPublicationTest;
    @autoreleasepool
    {
        TerrainFixture t(1);auto raw=t.batches(true);
        auto pass=ALMetalWorldTerrainPass::create(raw.front(),t.f.resources->textures,&t.f.error);REQUIRE(pass);
        std::weak_ptr<const ALMetalWorldTerrainPass> weak_pass=pass;
        std::weak_ptr<const ALMetalTexture> selected;
        for(const auto& texture:t.f.resources->textures)if(texture.get()==t.detail)selected=texture;
        REQUIRE_FALSE(selected.expired());
        auto geometry=std::make_shared<Resources>();geometry->buffers=t.f.resources->buffers;
        std::weak_ptr<Resources> weak_geometry=geometry;
        auto scene=t.scene();scene.source_owner=geometry;
        auto batches=install(t,pass);for(auto& batch:batches)batch.terrain_texture_owner.reset();
        auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(batches),{},geometry,1,&t.f.error);REQUIRE(inputs);
        REQUIRE(t.f.renderer->beginCommandBatch("Abandoned terrain private resource lease",&t.f.error));
        auto target=t.f.renderer->renderOwnedInputs(*inputs,&t.f.error);INFO(t.f.error);REQUIRE(target);
        auto completion=t.f.renderer->commandBatchCompletion();REQUIRE(completion);CHECK_FALSE(completion->isComplete());
        inputs.reset();pass.reset();scene.source_owner.reset();geometry.reset();t.f.resources.reset();
        CHECK(weak_pass.expired());CHECK_FALSE(selected.expired());CHECK_FALSE(weak_geometry.expired());
        t.f.renderer->cancelCommandBatch();CHECK(completion->isComplete());
        CHECK(selected.expired());CHECK(weak_geometry.expired());
    }
}
