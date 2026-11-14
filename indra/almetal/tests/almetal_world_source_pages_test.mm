/**
 * @file almetal_world_source_pages_test.mm
 * @brief Source page reflection, sparse COW and native reader retirement.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_source_pages_fixture.h"
#include <cstdlib>
#include <filesystem>
#include <unordered_set>

TEST_CASE("almetal: source pages prove unique native buffer devices and reuse bounded weak proofs",
          "[almetal][world-source-pages]")
{
    @autoreleasepool
    {
        for(unsigned family:{1u,2u})
        {
            CAPTURE(family);PageFixture f;
            if(family==2){f.cpu->source=f.batch(2);f.publish(f.cpu->source);}
            ALMetalWorldSourcePageLimits limits;limits.maximum_rows=1;auto pages=f.pages(limits);
            const auto uniqueBuffers=[&](const ALMetalWorldSimpleBatch& source)
            {
                std::unordered_set<void*> handles{source.vertices->nativeHandle(),source.indices->nativeHandle()};
                const auto& material=*source.native_material_source;
                for(const auto& buffer:{material.materialLease(),material.pbrLease(false),material.pbrLease(true)})
                    if(buffer)handles.insert(buffer->nativeHandle());
                for(auto handle:handles)
                    CHECK([(__bridge id<MTLBuffer>)handle device]==(__bridge id<MTLDevice>)f.device->nativeHandle());
                return handles.size();
            };
            const auto expected=uniqueBuffers(f.cpu->source);
            // Repeated requests and seven geometry attributes still require
            // one native device query for each distinct buffer generation.
            std::array<ALMetalWorldSourceRequest,4> requests;requests.fill(f.request());
            auto first=f.freeze(pages,requests);const auto cold=pages->stats();
            CHECK(cold.device_checks==expected);CHECK(cold.device_proof_hits==0);
            CHECK(cold.device_rejections==0);CHECK(cold.device_proof_entries==expected);
            auto warm=f.freeze(pages,requests);const auto reused=pages->stats();
            CHECK(reused.device_checks==cold.device_checks);
            CHECK(reused.device_proof_hits-cold.device_proof_hits==expected);
            CHECK(reused.row_publications==cold.row_publications);
            CHECK(warm->matchesSourceCommand(0,requests[0].key,*f.cpu->source.indices));

            // Wrapper identity is not Metal device identity. Publish all new
            // geometry/index/numeric buffers through a second real wrapper
            // for the same native device and preserve actual scalar pixels.
            auto same_device=ALMetalDevice::create(&f.error);INFO(f.error);REQUIRE(same_device);
            REQUIRE(same_device.get()!=f.device.get());
            REQUIRE(same_device->nativeHandle()==f.device->nativeHandle());
            ALMetalWorldMaterialDataCache other_cache(*same_device);
            auto changed=std::make_shared<PageDescription>(*f.cpu);
            auto owner=std::make_shared<Resources>(*f.resources);
            const auto copyBuffer=[&](const ALMetalBuffer& original)
            {
                ALMetalBufferDescriptor descriptor;descriptor.length=original.length();descriptor.storage=ALMetalStorageMode::shared;
                std::shared_ptr<const ALMetalBuffer> value;
                auto mutable_value=ALMetalBuffer::create(*same_device,descriptor,&f.error);REQUIRE(mutable_value);
                REQUIRE(mutable_value->write(0,original.mappedBytes(),original.length(),&f.error));
                value=std::move(mutable_value);owner->buffers.push_back(value);return value;
            };
            auto new_vertices=copyBuffer(*f.cpu->source.vertices),new_indices=copyBuffer(*f.cpu->source.indices);
            changed->source.vertices=new_vertices.get();changed->source.indices=new_indices.get();
            changed->source.native_material_source.reset();
            REQUIRE(ALMetalWorldMaterialSource::publish(*same_device,changed->source,other_cache,&f.error));
            REQUIRE(ALMetalWorldResourceSource::publish(*f.device,changed->source,owner->textures,&f.error));
            auto request=f.request(1,2);request.source=&changed->source;request.cpu_owner=changed;
            request.native={owner,new_vertices,new_indices};
            const auto replacement_count=uniqueBuffers(changed->source);
            auto replacement=f.freeze(pages,std::span(&request,1));const auto replaced=pages->stats();
            CHECK(replaced.device_checks-reused.device_checks==replacement_count);
            CHECK(replaced.device_rejections==0);CHECK(replaced.device_proof_entries<=5);
            REQUIRE(replacement->matchesSourceCommand(0,request.key,*new_indices));
            CHECK_FALSE(replacement->matchesSourceCommand(0,request.key,*f.cpu->source.indices));
            // PageFixture's native pipeline is Legacy; PBR native output is
            // covered by the renderer's source-row format/parity fixture.
            if(family==1)
            {
                auto before=f.encode(*first,f.cpu->source),after=f.encode(*replacement,changed->source);
                REQUIRE(before.command->commitAndWait(&f.error));REQUIRE(after.command->commitAndWait(&f.error));
                CHECK(f.read(*before.target)==f.read(*after.target));
            }
            // Only the default built-in Apple GPU is constructible through
            // ALMetalDevice's supported API. No distinct-device negative is
            // manufactured by spoofing a foreign native object.
            INFO("Distinct native GPU negative not run: supported device factory selects the default built-in Apple GPU");
        }
    }
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

TEST_CASE("almetal: source rows reprove derived table generations after real context churn",
          "[almetal][world-source-pages]")
{
    @autoreleasepool
    {
        PageFixture f;auto pages=f.pages();auto request=f.request();
        auto first=f.freeze(pages,std::span(&request,1));
        const auto row_id=first->rowId(0),allocation=first->allocationGeneration(0);
        auto old_table=first->tableGenerationForTesting();REQUIRE_FALSE(old_table.expired());
        auto baseline=f.encode(*first,f.cpu->source);REQUIRE(baseline.command->commitAndWait(&f.error));
        const auto expected=f.read(*baseline.target);baseline.command.reset();
        ALMetalSamplerDescriptor descriptor;descriptor.support_argument_buffers=true;
        auto sampler=ALMetalSamplerState::create(*f.device,descriptor,&f.error);REQUIRE(sampler);
        std::vector<std::shared_ptr<ALMetalWorldResources>> contexts;
        for(unsigned index=0;index<5;++index)
        {
            auto context=ALMetalWorldResources::create(*f.device,*f.library,{},&f.error);REQUIRE(context);
            contexts.push_back(std::move(context));
        }
        const auto churn=[&]
        {
            // Keep these real contexts alive. More than four exact contexts
            // evicts ContextA from the Source's bounded strong table cache.
            for(const auto& context:contexts)
            {
                auto table=context->prepare(f.cpu->source.native_resource_source,*sampler,&f.error);
                INFO(f.error);REQUIRE(table);
            }
        };
        churn();
        CHECK(f.cpu->source.native_material_source->matches(f.cpu->source));
        CHECK(f.cpu->source.native_resource_source->matches(f.cpu->source));
        CHECK_FALSE(old_table.expired());first.reset();waitPageTable(old_table);
        const auto before=pages->stats();
        auto restored=f.freeze(pages,std::span(&request,1));
        auto restored_table=restored->tableGenerationForTesting();REQUIRE_FALSE(restored_table.expired());
        // This exercises actual ownership turnover, not a forced allocator
        // ABA. No assertion depends on native-object address reuse.
        CHECK((old_table.owner_before(restored_table)||restored_table.owner_before(old_table)));
        CHECK(pages->stats().row_publications==before.row_publications+1);
        CHECK(restored->rowId(0)==row_id);CHECK(restored->allocationGeneration(0)==allocation);
        auto replay=f.encode(*restored,f.cpu->source);REQUIRE(replay.command->commitAndWait(&f.error));
        CHECK(f.read(*replay.target)==expected);replay.command.reset();

        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate {id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold derived table through context churn",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        auto pending=f.encode(*restored,f.cpu->source);REQUIRE(pending.command->commit(&f.error));restored.reset();
        churn();CHECK_FALSE(restored_table.expired());
        const auto held=pages->stats();
        auto still_live=f.freeze(pages,std::span(&request,1));
        auto same_table=still_live->tableGenerationForTesting();
        CHECK_FALSE(same_table.owner_before(restored_table));CHECK_FALSE(restored_table.owner_before(same_table));
        CHECK(pages->stats().row_publications==held.row_publications);still_live.reset();
        // Remove the reinserted source-cache lease again; only the pending
        // GPU view now keeps that exact ContextA table alive.
        churn();CHECK_FALSE(restored_table.expired());
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(f.read(*pending.target)==expected);
        waitPageTable(restored_table);
        const auto settled=pages->stats();auto after_completion=f.freeze(pages,std::span(&request,1));
        CHECK(pages->stats().row_publications==settled.row_publications+1);
    }
}

TEST_CASE("almetal: source pages reject a source shader library missing direct row layout reflection",
          "[almetal][.world-source-pages-no-layout]")
{
    @autoreleasepool
    {
        const char* selected=std::getenv("ALCHEMY_SOURCE_ROWS_NO_LAYOUT_LIBRARY");
        REQUIRE(selected);REQUIRE(*selected);
        const std::filesystem::path path(selected);
        REQUIRE(path.is_absolute());REQUIRE(std::filesystem::is_regular_file(path));
        std::string error;auto device=ALMetalDevice::create(&error);REQUIRE(device);
        auto library=ALMetalShaderLibrary::loadFile(*device,path.string(),&error);INFO(error);REQUIRE(library);
        REQUIRE(library->hasFunction("almetal_world_material_source_rows_vertex"));
        REQUIRE(library->hasFunction("almetal_world_pbr_source_rows_vertex"));
        REQUIRE(library->hasFunction("almetal_world_material_source_rows_fragment"));
        REQUIRE(library->hasFunction("almetal_world_pbr_source_rows_fragment"));
        REQUIRE_FALSE(library->hasFunction("almetal_world_source_rows_layout"));
        auto pages=ALMetalWorldSourcePages::create(*device,*library,1,{},&error);
        CHECK_FALSE(pages);CHECK_FALSE(error.empty());
    }
}
