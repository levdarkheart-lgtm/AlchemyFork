/**
 * @file almetal_world_source_pages_test.mm
 * @brief Source page reflection, sparse COW and native reader retirement.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldsourcepages.h"
#include "../almetalworldmaterialdata.h"

namespace
{
void waitPageResources(const std::weak_ptr<Resources>& weak)
{
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!weak.expired()&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
    CHECK(weak.expired());
}
struct PageDescription {ALMetalWorldSimpleBatch source;};
struct PageFixture : Fixture
{
    std::unique_ptr<ALMetalShaderLibrary> library;
    ALMetalWorldMaterialDataCache cache;
    std::shared_ptr<PageDescription> cpu;
    ALMetalWorldSourceRegion region;
    ALMetalWorldSourceGlobals globals;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;
    std::unique_ptr<ALMetalDepthStencilState> depth;
    PageFixture():cache(*device)
    {
        library=ALMetalShaderLibrary::loadBuiltin(*device,&error);INFO(error);REQUIRE(library);
        cpu=std::make_shared<PageDescription>();cpu->source=batch(1);
        cpu->source.texcoord1_offset=cpu->source.texcoord2_offset=offsetof(Geometry,uv);
        publish(cpu->source);
        identity(region.modelview_projection);identity(region.modelview);
        for(unsigned axis=0;axis<3;++axis)region.normal_matrix[axis][axis]=1;
    }
    void publish(ALMetalWorldSimpleBatch& source)
    {
        REQUIRE(ALMetalWorldMaterialSource::publish(*device,source,cache,&error));
        REQUIRE(ALMetalWorldResourceSource::publish(*device,source,resources->textures,&error));
    }
    std::shared_ptr<ALMetalWorldSourcePages> pages(ALMetalWorldSourcePageLimits limits={})
    {
        auto value=ALMetalWorldSourcePages::create(*device,*library,1,limits,&error);
        INFO(error);REQUIRE(value);REQUIRE(value->supported());return value;
    }
    ALMetalWorldSourceRequest request(std::uint64_t id=1,std::uint64_t revision=1)
    {
        ALMetalWorldSourceRequest value;value.key={id,1,revision};value.source=&cpu->source;
        value.cpu_owner=cpu;value.native.owner=resources;
        for(const auto& buffer:resources->buffers)
        {
            if(buffer.get()==vertices)value.native.vertices=buffer;
            if(buffer.get()==indices)value.native.indices=buffer;
        }
        return value;
    }
    std::shared_ptr<const ALMetalWorldSourceFrameView> freeze(
        const std::shared_ptr<ALMetalWorldSourcePages>& pages,std::span<const ALMetalWorldSourceRequest> requests)
    {
        auto value=pages->freeze(requests,std::span(&region,1),globals,&error);
        INFO(error);REQUIRE(value);return value;
    }
    struct Encoded
    {
        std::unique_ptr<ALMetalRenderTarget> target;
        std::unique_ptr<ALMetalCommandBuffer> command;
    };
    Encoded encode(const ALMetalWorldSourceFrameView& view,const ALMetalWorldSimpleBatch& source)
    {
        if(!pipeline)
        {
            ALMetalRenderPipelineDescriptor descriptor;descriptor.library=library.get();
            descriptor.vertex_function="almetal_world_material_source_rows_vertex";
            descriptor.fragment_function="almetal_world_material_source_rows_fragment";
            descriptor.color_attachment_count=4;
            for(unsigned slot=0;slot<4;++slot)descriptor.colors[slot].format=ALMetalPixelFormat::rgba8_unorm;
            descriptor.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
            pipeline=ALMetalRenderPipeline::create(*device,descriptor,&error);INFO(error);REQUIRE(pipeline);
            ALMetalDepthStencilDescriptor dd;depth=ALMetalDepthStencilState::create(*device,dd,&error);REQUIRE(depth);
        }
        ALMetalRenderTargetDescriptor descriptor;descriptor.width=descriptor.height=side;descriptor.color_attachment_count=4;
        for(unsigned slot=0;slot<4;++slot)descriptor.colors[slot].format=ALMetalPixelFormat::rgba8_unorm;
        descriptor.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
        Encoded value;value.target=ALMetalRenderTarget::create(*device,descriptor,&error);REQUIRE(value.target);
        ALMetalRenderPassConfig config;config.depth_store=config.stencil_store=ALMetalStoreAction::store;
        auto pass=value.target->makeRenderPass(config,&error);REQUIRE(pass);
        value.command=device->makeCommandBuffer("Source page reader test",&error);REQUIRE(value.command);
        auto encoder=ALMetalRenderEncoder::begin(*value.command,*pass,"Source page reader encoder",&error);REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline,&error));REQUIRE(encoder->setDepthStencilState(depth.get(),&error));
        REQUIRE(encoder->setViewport({0,0,side,side,0,1},&error));REQUIRE(encoder->setScissor({0,0,side,side},&error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&error));REQUIRE(view.bind(*encoder,*value.command,&error));
        // Rebinding the same immutable view must not repeat the native union
        // or grow completion pins, but still restores overwritten bindings.
        REQUIRE(view.bind(*encoder,*value.command,&error));
        REQUIRE(encoder->drawIndexedWithBaseInstance(ALMetalPrimitiveType::triangle,source.index_count,
            source.index_type,*source.indices,source.index_offset,view.rowId(0),&error));
        REQUIRE(encoder->end(&error));return value;
    }
};
}

TEST_CASE("almetal: reflected source pages copy only a dirty reader page and reuse idle metadata",
          "[almetal][world-source-pages]")
{
    @autoreleasepool
    {
        PageFixture f;auto pages=f.pages();
        CHECK(pages->belongsTo(*f.device,*f.library,1));CHECK_FALSE(pages->belongsTo(*f.device,*f.library,2));
        auto other_library=ALMetalShaderLibrary::compileSource(*f.device,
            "#include <metal_stdlib>\nusing namespace metal;kernel void unrelated(device uint* x [[buffer(0)]]){x[0]=1;}",
            "Unrelated source-page ABI",&f.error);REQUIRE(other_library);
        CHECK_FALSE(pages->belongsTo(*f.device,*other_library,1));
        std::vector<ALMetalWorldSourceRequest> requests;
        for(unsigned source=0;source<257;++source)requests.push_back(f.request(source+1));
        auto first=f.freeze(pages,requests);const auto initial=pages->stats();
        CHECK(initial.row_publications==257);CHECK(initial.page_allocations==3);
        CHECK(initial.encoded_row_bytes>0);CHECK(initial.live_metadata_bytes>0);
        auto second=f.freeze(pages,requests);const auto repeated=pages->stats();
        CHECK(repeated.hits-initial.hits==257);CHECK(repeated.row_publications==initial.row_publications);
        CHECK(repeated.copied_page_bytes==0);CHECK(repeated.copied_bank_bytes==0);
        auto changed=std::make_shared<PageDescription>(*f.cpu);
        changed->source.materialSlots()[0].env_intensity=.8f;f.publish(changed->source);
        pages->invalidate(129,2);requests[128].key.revision=2;
        requests[128].source=&changed->source;requests[128].cpu_owner=changed;
        auto updated=f.freeze(pages,requests);const auto dirty=pages->stats();
        CHECK(dirty.row_publications-repeated.row_publications==1);
        CHECK(dirty.page_allocations-repeated.page_allocations==1);
        CHECK(dirty.bank_allocations-repeated.bank_allocations==1);
        CHECK(dirty.copied_page_bytes>0);CHECK(dirty.copied_bank_bytes>0);
        CHECK(updated->rowId(128)==first->rowId(128));
        first.reset();second.reset();updated.reset();
        changed=std::make_shared<PageDescription>(*changed);
        changed->source.materialSlots()[0].env_intensity=.6f;f.publish(changed->source);
        pages->invalidate(129,3);requests[128].key.revision=3;
        requests[128].source=&changed->source;requests[128].cpu_owner=changed;
        auto idle=f.freeze(pages,requests);const auto in_place=pages->stats();
        CHECK(in_place.row_publications-dirty.row_publications==1);
        CHECK(in_place.page_allocations==dirty.page_allocations);
        CHECK(in_place.copied_page_bytes==dirty.copied_page_bytes);
        CHECK(in_place.copied_bank_bytes==dirty.copied_bank_bytes);
        // Per-view dense region order is not an immutable source-row field.
        std::array<ALMetalWorldSourceRegion,2> regions{f.region,f.region};regions[1].modelview_projection[12]=.25f;
        for(auto& request:requests)request.region=1;
        auto different_region=pages->freeze(requests,regions,f.globals,&f.error);REQUIRE(different_region);
        CHECK(pages->stats().row_publications==in_place.row_publications);
    }
}

TEST_CASE("almetal: source ID churn recycles bounded rows while pending old GPU bytes and generations survive",
          "[almetal][world-source-pages]")
{
    @autoreleasepool
    {
        PageFixture f;ALMetalWorldSourcePageLimits limits;limits.maximum_rows=2;auto pages=f.pages(limits);
        auto request=f.request();auto old=f.freeze(pages,std::span(&request,1));
        const auto old_id=old->rowId(0),old_generation=old->allocationGeneration(0);
        auto baseline=f.encode(*old,f.cpu->source);REQUIRE(baseline.command->commitAndWait(&f.error));
        const auto expected=f.read(*baseline.target);
        REQUIRE(std::any_of(expected.depth.begin(),expected.depth.end(),[](auto value){return value!=0x3f800000u;}));
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate {id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold old source page through online churn",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        auto pending=f.encode(*old,f.cpu->source);REQUIRE(pending.command->commit(&f.error));
        std::weak_ptr<const ALMetalWorldSourceFrameView> weak_view=old;old.reset();
        std::uint32_t newest_generation=0;
        std::shared_ptr<PageDescription> latest;
        for(unsigned id=2;id<=11;++id)
        {
            latest=std::make_shared<PageDescription>(*f.cpu);
            latest->source.materialSlots()[0].specular_color[0]=.5f+(id%10)*.04f;
            latest->source.materialSlots()[0].env_intensity=.05f*id;f.publish(latest->source);
            auto next_request=f.request(id);next_request.source=&latest->source;next_request.cpu_owner=latest;
            auto next=f.freeze(pages,std::span(&next_request,1));
            CHECK(pages->stats().rows<=2);
            if(next->rowId(0)==old_id)newest_generation=next->allocationGeneration(0);
        }
        REQUIRE(newest_generation>old_generation);CHECK_FALSE(weak_view.expired());
        CHECK(pages->stats().copied_page_bytes>0);CHECK(pages->stats().copied_bank_bytes>0);
        // A removed producer's identity index must not invalidate the newer
        // source now occupying its old numeric slot.
        auto current_request=f.request(11);current_request.source=&latest->source;current_request.cpu_owner=latest;
        const auto before=pages->stats();pages->invalidate(1,99);
        auto current=f.freeze(pages,std::span(&current_request,1));
        CHECK(pages->stats().row_publications==before.row_publications);
        auto newest=f.encode(*current,latest->source);REQUIRE(newest.command->commit(&f.error));current.reset();
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(f.read(*pending.target)==expected);
        CHECK_FALSE(f.read(*newest.target)==expected);
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!weak_view.expired()&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
        CHECK(weak_view.expired());
        // One view with more distinct sources than capacity fails completely.
        std::array<ALMetalWorldSourceRequest,3> oversized{f.request(20),f.request(21),f.request(22)};
        CHECK_FALSE(pages->freeze(oversized,std::span(&f.region,1),f.globals,&f.error));
        auto recovery=f.request(23);REQUIRE(f.freeze(pages,std::span(&recovery,1)));
    }
}

TEST_CASE("almetal: native source readers drop CPU descriptions and retire exact scopes on abandonment or completion",
          "[almetal][world-source-pages]")
{
    @autoreleasepool
    {
        for(bool abandon:{false,true})
        {
            CAPTURE(abandon);PageFixture f;auto pages=f.pages();auto request=f.request();
            auto view=f.freeze(pages,std::span(&request,1));
            // Warm the shader/PSO before closing a queue gate.
            auto warm=f.encode(*view,f.cpu->source);REQUIRE(warm.command->commitAndWait(&f.error));
            const auto expected=f.read(*warm.target);
            std::weak_ptr<PageDescription> cpu=f.cpu;std::weak_ptr<Resources> native=f.resources;
            id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
            struct OpenGate {id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
            if(!abandon)
            {
                auto blocker=f.device->makeCommandBuffer("Hold exact native source lease",&f.error);REQUIRE(blocker);
                [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
            }
            auto pending=f.encode(*view,f.cpu->source);if(!abandon)REQUIRE(pending.command->commit(&f.error));
            request={};f.cpu.reset();f.resources.reset();view.reset();
            CHECK(cpu.expired());CHECK_FALSE(native.expired());
            if(abandon)pending.command.reset();
            else{gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(f.read(*pending.target)==expected);}
            waitPageResources(native);
            // Idle page/root metadata must not retain C++ backing generations.
            CHECK(native.expired());CHECK(pages->stats().live_metadata_bytes>0);
        }
    }
}

TEST_CASE("almetal: source publication rejects stale proofs malformed scopes special states and bounded failures",
          "[almetal][world-source-pages]")
{
    @autoreleasepool
    {
        PageFixture f;auto pages=f.pages();auto request=f.request();
        auto valid=f.freeze(pages,std::span(&request,1));valid.reset();
        pages->withdraw(1);auto same_revision=f.freeze(pages,std::span(&request,1));same_revision.reset();
        pages->invalidate(1,2);CHECK_FALSE(pages->freeze(std::span(&request,1),std::span(&f.region,1),f.globals,&f.error));
        request.key.revision=2;auto next=f.freeze(pages,std::span(&request,1));next.reset();
        for(unsigned bad=0;bad<9;++bad)
        {
            CAPTURE(bad);auto invalid=request;auto source=f.cpu->source;invalid.source=&source;
            switch(bad)
            {
                case 0:invalid.native.owner.reset();break;
                case 1:invalid.native.vertices=invalid.native.indices;break;
                case 2:invalid.region=1;break;
                case 3:source.pre_water_material=true;break;
                case 4:source.legacy_glow=true;break;
                case 5:source.normal_texture=f.flat;break;
                case 6:source.native_resource_source.reset();break;
                case 7:source.texture_matrix[0]=std::numeric_limits<float>::quiet_NaN();break;
                case 8:source.post_water_depth_only=true;break;
            }
            CHECK_FALSE(pages->freeze(std::span(&invalid,1),std::span(&f.region,1),f.globals,&f.error));
        }
        auto borrowed=request;borrowed.cpu_owner.reset();REQUIRE(f.freeze(pages,std::span(&borrowed,1)));
        std::array<ALMetalWorldSourceRegion,2> regions{f.region,f.region};
        auto duplicate=request;duplicate.region=1;
        std::array<ALMetalWorldSourceRequest,2> conflicting{request,duplicate};
        CHECK_FALSE(pages->freeze(conflicting,regions,f.globals,&f.error));
        auto bad_region=f.region;bad_region.modelview[15]=0;
        CHECK_FALSE(pages->freeze(std::span(&request,1),std::span(&bad_region,1),f.globals,&f.error));
        pages->failNextPublicationForTesting();CHECK_FALSE(pages->freeze(std::span(&request,1),std::span(&f.region,1),f.globals,&f.error));
        REQUIRE(f.freeze(pages,std::span(&request,1)));
        ALMetalWorldSourcePageLimits limits;limits.maximum_metadata_bytes=256;
        auto bounded=f.pages(limits);CHECK_FALSE(bounded->freeze(std::span(&request,1),std::span(&f.region,1),f.globals,&f.error));
        CHECK(bounded->stats().live_metadata_bytes<=limits.maximum_metadata_bytes);
    }
}
