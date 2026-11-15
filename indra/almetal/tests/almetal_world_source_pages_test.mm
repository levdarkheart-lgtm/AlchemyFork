/**
 * @file almetal_world_source_pages_test.mm
 * @brief Source page reflection, sparse COW and native reader retirement.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_source_pages_fixture.h"
#include "../almetalworldcohort.h"
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

namespace
{
struct PageCohortCpu
{
    std::vector<ALMetalWorldSimpleBatch> sources;
    std::vector<ALMetalWorldKeyedSource> keys;
};
struct CohortPageFixture : PageFixture
{
    std::shared_ptr<PageCohortCpu> descriptions=std::make_shared<PageCohortCpu>();
    std::shared_ptr<const ALMetalWorldSourceCohort> certificate;
    static std::shared_ptr<const ALMetalTexture> resolve(const std::shared_ptr<const void>& owner,
                                                        const ALMetalTexture* texture) noexcept
    {
        const auto& tuple=*std::static_pointer_cast<const Resources>(owner);
        const auto found=std::find_if(tuple.textures.begin(),tuple.textures.end(),[&](const auto& lease){return lease.get()==texture;});
        return found==tuple.textures.end()?nullptr:*found;
    }
    explicit CohortPageFixture(unsigned family=1,unsigned count=2)
    {
        descriptions->sources.reserve(count);descriptions->keys.reserve(count);
        for(unsigned i=0;i<count;++i)
        {
            auto source=batch(family);source.texcoord1_offset=source.texcoord2_offset=offsetof(Geometry,uv);
            source.texture_matrix[12]=i*.125f;
            if(family==1)source.materialSlots()[0].env_intensity=.1f*i;
            else {source.pbrSlots()[0].roughness_factor=.3f+.1f*i;source.double_sided=(i%2)!=0;}
            REQUIRE(ALMetalWorldMaterialSource::publish(*device,source,cache,&error));
            REQUIRE_FALSE(source.native_resource_source);
            for(const auto& value:{source.native_material_source->materialLease(),source.native_material_source->pbrLease(false),
                                  source.native_material_source->pbrLease(true)})
                if(value && std::none_of(resources->buffers.begin(),resources->buffers.end(),[&](const auto& old){return old==value;}))
                    resources->buffers.push_back(value);
            descriptions->sources.push_back(std::move(source));
        }
        seal();
    }
    void seal(std::uint64_t revision=1)
    {
        descriptions->keys.clear();
        for(std::size_t i=0;i<descriptions->sources.size();++i)
            descriptions->keys.push_back({{i+1,descriptions->sources[i].pbr_slot_count?2u:1u,revision},&descriptions->sources[i]});
        certificate=ALMetalWorldSourceCohort::seal(descriptions->keys,descriptions,resources,7,&error,&resolve);
        INFO(error);REQUIRE(certificate);
    }
    ALMetalWorldSourceCohortView cohortView()const
    {
        ALMetalWorldSourceCohortView value;value.cohort=certificate;value.cpu_owner=descriptions;value.native_owner=resources;
        identity(value.view.modelview_projection);identity(value.view.modelview);
        for(unsigned axis=0;axis<3;++axis)value.view.normal_matrix[axis][axis]=value.view.rigged_normal_matrix[axis][axis]=1;
        return value;
    }
};
}

TEST_CASE("almetal: cohort source publication preserves descriptor-free sources and warm immutable membership",
          "[almetal][world-source-pages][world-cohort]")
{
    @autoreleasepool
    {
        for(unsigned family:{1u,2u})
        {
            CAPTURE(family);CohortPageFixture f(family,4);auto pages=f.pages();auto cohort=f.cohortView();
            unsigned admission_calls=0;
            pages->setPublicationAdmission([&](const auto& work)->std::shared_ptr<const void>
            {++admission_calls;REQUIRE(work.chargedBytes()>0);return std::make_shared<unsigned>(1);});
            auto command=f.device->makeCommandBuffer("Cold completed cohort",&f.error);REQUIRE(command);
            auto cold=pages->freezeCohortForCommand(*command,cohort,f.globals,&f.error);INFO(f.error);REQUIRE(cold);
            const auto before=pages->stats();REQUIRE(cold->size()==4);REQUIRE(admission_calls==1);
            const auto membership=cold->commandMembership();REQUIRE(membership);REQUIRE(membership->calls().size()==4);
            CHECK(membership->nativeDevice()==f.device->nativeHandle());
            CHECK(membership->runs().size()==(family==1?1:4));
            for(std::size_t i=0;i<4;++i)
            {
                CHECK_FALSE(f.descriptions->sources[i].native_resource_source); // no mutation of direct publication
                CHECK(membership->calls()[i].key.identity==i+1);
                CHECK(membership->calls()[i].row_id==cold->rowId(i));
                CHECK(membership->calls()[i].native_indices==f.indices->nativeHandle());
            }
            // Matrix changes are a per-view operation, not source publication.
            cohort.view.modelview_projection[12]=.125f;f.globals.flags[0]=f.globals.flags[2]=1;
            auto next=f.device->makeCommandBuffer("Warm cohort another camera",&f.error);REQUIRE(next);
            auto warm=pages->freezeCohortForCommand(*next,cohort,f.globals,&f.error);INFO(f.error);REQUIRE(warm);
            const auto after=pages->stats();
            CHECK(warm->commandMembership()==membership);CHECK(after.cohort_hits==before.cohort_hits+1);
            CHECK(after.requests==before.requests);CHECK(after.device_checks==before.device_checks);
            CHECK(after.device_proof_hits==before.device_proof_hits);CHECK(after.row_publications==before.row_publications);
            CHECK(after.admission_requests==before.admission_requests);CHECK(admission_calls==1);
            CHECK(after.view_bytes-before.view_bytes==sizeof(ALMetalWorldSourceRegion)+sizeof(ALMetalWorldSourceGlobals));
            CHECK(after.cached_cohorts==1);CHECK(after.cached_cohort_rows==4);
            // Another exact shader/sampler context cannot consume this cache.
            auto other=ALMetalWorldSourcePages::create(*f.device,*f.library,4,{},&f.error);REQUIRE(other);
            auto other_command=f.device->makeCommandBuffer("Other sampler context cohort",&f.error);REQUIRE(other_command);
            auto other_view=other->freezeCohortForCommand(*other_command,cohort,f.globals,&f.error);REQUIRE(other_view);
            CHECK(other_view->commandMembership()!=membership);CHECK(other->stats().cohort_builds==1);
            auto bad=cohort;bad.cpu_owner=std::make_shared<unsigned>(1);
            CHECK_FALSE(pages->freezeCohortForCommand(*next,bad,f.globals,&f.error));
            bad=cohort;bad.native_owner=std::make_shared<unsigned>(1);
            CHECK_FALSE(pages->freezeCohortForCommand(*next,bad,f.globals,&f.error));
            bad=cohort;bad.view.modelview[15]=0;
            CHECK_FALSE(pages->freezeCohortForCommand(*next,bad,f.globals,&f.error));
        }
    }
}

TEST_CASE("almetal: cohort page content proofs reject same-slot changes and preserve pending native pixels",
          "[almetal][world-source-pages][world-cohort]")
{
    @autoreleasepool
    {
        CohortPageFixture f;ALMetalWorldSourcePageLimits limits;limits.maximum_rows=2;auto pages=f.pages(limits);
        auto cohort=f.cohortView();auto command=f.device->makeCommandBuffer("Original cohort pixels",&f.error);REQUIRE(command);
        auto first=pages->freezeCohortForCommand(*command,cohort,f.globals,&f.error);REQUIRE(first);
        auto reference=f.encode(*first,f.descriptions->sources[0],std::move(command));REQUIRE(reference.command->commitAndWait(&f.error));
        const auto expected=f.read(*reference.target);const auto old_membership=first->commandMembership();
        const auto old_row=first->rowId(0),old_generation=first->allocationGeneration(0);
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate {id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Keep old cohort page pending",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        auto pending=f.encode(*first,f.descriptions->sources[0]);REQUIRE(pending.command->commit(&f.error));
        const auto before=pages->stats();
        // A different publication writes the SAME allocation generation. A
        // page-allocation-only stamp would incorrectly accept the old cohort.
        auto changed=std::make_shared<PageDescription>();changed->source=f.descriptions->sources[0];
        changed->source.materialSlots()[0].env_intensity=.85f;f.publish(changed->source);
        auto request=f.request(1,2);request.source=&changed->source;request.cpu_owner=changed;
        auto replacement=f.freeze(pages,std::span(&request,1));REQUIRE(replacement->rowId(0)==old_row);
        REQUIRE(replacement->allocationGeneration(0)==old_generation);
        CHECK(pages->stats().cached_cohorts==0);CHECK(pages->stats().copied_page_bytes>before.copied_page_bytes);
        CHECK(pages->stats().copied_bank_bytes>before.copied_bank_bytes);
        auto stale=f.device->makeCommandBuffer("Reject stale cohort generation",&f.error);REQUIRE(stale);
        CHECK_FALSE(pages->freezeCohortForCommand(*stale,cohort,f.globals,&f.error));
        CHECK(pages->stats().cohort_hits==before.cohort_hits);
        first.reset();replacement.reset();
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(f.read(*pending.target)==expected);
        // Cache-only withdrawal permits reconstruction of the same immutable
        // authoritative revision, but must issue different membership proof.
        pages->clear();auto rebuilt=f.device->makeCommandBuffer("Rebuild cleared cohort",&f.error);REQUIRE(rebuilt);
        auto fresh=pages->freezeCohortForCommand(*rebuilt,cohort,f.globals,&f.error);INFO(f.error);REQUIRE(fresh);
        CHECK(fresh->commandMembership()!=old_membership);
        pages->withdraw(1);CHECK(pages->stats().cached_cohorts==0);
        auto restored=f.device->makeCommandBuffer("Restore cache-only withdrawn cohort",&f.error);REQUIRE(restored);
        auto again=pages->freezeCohortForCommand(*restored,cohort,f.globals,&f.error);INFO(f.error);REQUIRE(again);
        CHECK(again->commandMembership()!=fresh->commandMembership());
    }
}

TEST_CASE("almetal: cached cohort metadata does not retain CPU certificates or create idle page readers",
          "[almetal][world-source-pages][world-cohort]")
{
    @autoreleasepool
    {
        CohortPageFixture f;auto pages=f.pages();auto cohort=f.cohortView();
        auto command=f.device->makeCommandBuffer("Cohort native lifetime warmup",&f.error);REQUIRE(command);
        auto ready=pages->freezeCohortForCommand(*command,cohort,f.globals,&f.error);REQUIRE(ready);
        auto reference=f.encode(*ready,f.descriptions->sources[0],std::move(command));REQUIRE(reference.command->commitAndWait(&f.error));
        const auto expected=f.read(*reference.target);std::weak_ptr<const ALMetalWorldSourceFrameView> completed=ready;ready.reset();
        const auto retire_end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!completed.expired()&&std::chrono::steady_clock::now()<retire_end)std::this_thread::yield();REQUIRE(completed.expired());
        const auto idle=pages->stats();pages->withdraw(1);
        auto recold_command=f.device->makeCommandBuffer("Idle cache is not a page reader",&f.error);REQUIRE(recold_command);
        auto recold=pages->freezeCohortForCommand(*recold_command,cohort,f.globals,&f.error);REQUIRE(recold);
        CHECK(pages->stats().copied_page_bytes==idle.copied_page_bytes);
        CHECK(pages->stats().copied_bank_bytes==idle.copied_bank_bytes);recold.reset();recold_command.reset();
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate {id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold native-only cohort command",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        auto warm_command=f.device->makeCommandBuffer("Native-only warm cohort reader",&f.error);REQUIRE(warm_command);
        auto warm=pages->freezeCohortForCommand(*warm_command,cohort,f.globals,&f.error);REQUIRE(warm);
        auto pending=f.encode(*warm,f.descriptions->sources[0],std::move(warm_command));REQUIRE(pending.command->commit(&f.error));
        std::weak_ptr<PageCohortCpu> cpu=f.descriptions;std::weak_ptr<const ALMetalWorldSourceCohort> certificate=f.certificate;
        std::weak_ptr<Resources> native=f.resources;std::weak_ptr<const ALMetalWorldSourceFrameView> frame=warm;
        cohort={};f.descriptions.reset();f.certificate.reset();f.cpu.reset();f.resources.reset();warm.reset();
        CHECK(cpu.expired());CHECK(certificate.expired());CHECK_FALSE(native.expired());CHECK_FALSE(frame.expired());
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(f.read(*pending.target)==expected);
        waitPageResources(native);
        // Native-only table caching did not keep a C++ generation owner alive.
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!frame.expired()&&std::chrono::steady_clock::now()<end)std::this_thread::yield();CHECK(frame.expired());
    }
}

TEST_CASE("almetal: cohort publication admission denies unchanged metadata and retires only on the actual command",
          "[almetal][world-source-pages][world-source-admission][world-cohort]")
{
    @autoreleasepool
    {
        CohortPageFixture f;auto pages=f.pages();auto cohort=f.cohortView();
        ALNativeFrameBudget::Limits limits;limits.maximum_deferral_frames=3;
        ALNativeFrameBudget budget(limits);std::uint64_t oldest=0;unsigned calls=0;
        ALMetalWorldSourcePublicationWork observed;
        const auto begin=[&](std::uint64_t frame,bool pressure)
        {
            ALNativeFrameBudget::FrameInputs inputs;inputs.frame=frame;inputs.frame_interval_ns=16'666'667;
            if(pressure){inputs.required_main_cpu_ns=16'000'000;inputs.native_budget_bytes=inputs.native_allocated_bytes=1;}
            budget.beginFrame(inputs);
        };
        pages->setPublicationAdmission([&](const auto& work)->std::shared_ptr<const void>
        {
            ++calls;observed=work;if(!oldest)oldest=budget.snapshot().frame;
            auto result=budget.tryAcquire(ALNativeFrameBudget::Work::scene_publication,work.chargedBytes(),oldest);
            if(result)oldest=0;return result.credit;
        });
        begin(1,true);const auto allocations=alMetalResourceAllocationStats();
        auto denied=f.device->makeCommandBuffer("Deferred whole cohort",&f.error);REQUIRE(denied);
        CHECK_FALSE(pages->freezeCohortForCommand(*denied,cohort,f.globals,&f.error));
        CHECK(pages->stats().row_publications==0);CHECK(pages->stats().live_metadata_bytes==0);
        CHECK(pages->stats().cached_cohorts==0);CHECK(alMetalResourceAllocationStats().buffers_created==allocations.buffers_created);
        REQUIRE(calls==1);CHECK(oldest==1);REQUIRE(observed.chargedBytes()>0);
        begin(4,true);auto command=f.device->makeCommandBuffer("Aged whole cohort",&f.error);REQUIRE(command);
        auto ready=pages->freezeCohortForCommand(*command,cohort,f.globals,&f.error);INFO(f.error);REQUIRE(ready);
        CHECK(calls==2);CHECK(budget.snapshot().scene_publications_in_flight==1);
        auto encoded=f.encode(*ready,f.descriptions->sources[0],std::move(command));
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate {id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold cohort publication receipt",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        REQUIRE(encoded.command->commit(&f.error));const auto charged=observed.chargedBytes();
        CHECK(budget.snapshot().scene_bytes_in_flight==charged);
        auto next=f.device->makeCommandBuffer("Warm cohort no scene admission",&f.error);REQUIRE(next);
        auto warm=pages->freezeCohortForCommand(*next,cohort,f.globals,&f.error);REQUIRE(warm);
        CHECK(calls==2);CHECK(budget.snapshot().scene_publications_in_flight==1);
        CHECK(budget.snapshot().uploads_in_flight==0);CHECK(budget.snapshot().optional_views_in_flight==0);
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));
        const auto waitCredit=[&]
        {
            const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
            while(budget.snapshot().scene_publications_in_flight&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
            REQUIRE(budget.snapshot().scene_publications_in_flight==0);CHECK(budget.snapshot().scene_bytes_in_flight==0);
        };
        waitCredit();REQUIRE(ready);REQUIRE(warm); // retained views never own the receipt
        begin(5,false);pages->withdraw(1);
        auto abandoned=f.device->makeCommandBuffer("Abandon dirty cohort receipt",&f.error);REQUIRE(abandoned);
        auto republished=pages->freezeCohortForCommand(*abandoned,cohort,f.globals,&f.error);REQUIRE(republished);
        REQUIRE(budget.snapshot().scene_publications_in_flight==1);abandoned.reset();waitCredit();REQUIRE(republished);
    }
}

TEST_CASE("almetal: cohort idle asset budget deduplicates real texture roots and frame maintenance releases expired caches",
          "[almetal][world-source-pages][world-cohort]")
{
    @autoreleasepool
    {
        CohortPageFixture f;auto pages=f.pages();auto first=f.cohortView();
        auto command=f.device->makeCommandBuffer("Cached root asset union",&f.error);REQUIRE(command);
        auto frame=pages->freezeCohortForCommand(*command,first,f.globals,&f.error);REQUIRE(frame);
        const auto one=pages->stats();REQUIRE(one.cached_cohort_native_asset_bytes>0);
        CHECK(one.cached_cohort_native_asset_bytes<=one.cached_cohort_native_asset_limit);
        const auto expected_limit=std::clamp<std::uint64_t>(f.device->info().recommended_working_set_size/16,
            256ULL*1024*1024,4ULL*1024*1024*1024);
        CHECK(one.cached_cohort_native_asset_limit==expected_limit);
        // Another privately sealed CPU cohort can reuse exactly the same
        // rows/tables/native assets without charging their root bytes twice.
        auto second_certificate=ALMetalWorldSourceCohort::seal(f.descriptions->keys,f.descriptions,f.resources,7,&f.error,&CohortPageFixture::resolve);
        REQUIRE(second_certificate);auto second=first;second.cohort=second_certificate;
        auto second_command=f.device->makeCommandBuffer("Shared root assets another cohort",&f.error);REQUIRE(second_command);
        auto second_frame=pages->freezeCohortForCommand(*second_command,second,f.globals,&f.error);REQUIRE(second_frame);
        CHECK(pages->stats().cached_cohorts==2);CHECK(pages->stats().cached_cohort_native_asset_bytes==one.cached_cohort_native_asset_bytes);
        CHECK(pages->stats().row_publications==one.row_publications);
        // The owner maintenance boundary is independent of another GPU-cohort
        // request. Actual frozen command readers remain legal after pruning.
        auto table=frame->tableGenerationForTesting();REQUIRE_FALSE(table.expired());
        first={};second={};f.certificate.reset();second_certificate.reset();f.descriptions.reset();
        pages->trimUnusedCohorts();CHECK(pages->stats().cached_cohorts==0);CHECK(pages->stats().cached_cohort_native_asset_bytes==0);
        CHECK_FALSE(table.expired());auto encoded=f.encode(*frame,f.cpu->source,std::move(command));
        REQUIRE(encoded.command->commitAndWait(&f.error));
        const auto pixels=f.read(*encoded.target);
        CHECK(std::any_of(pixels.depth.begin(),pixels.depth.end(),[](auto value){return value!=0x3f800000u;}));
        // An explicit tiny asset cap disables only cache admission. Valid
        // native publication and command output remain available uncached.
        CohortPageFixture limited;ALMetalWorldSourcePageLimits cap;cap.maximum_cached_cohort_native_asset_bytes=1;
        auto uncached=limited.pages(cap);auto selected=limited.cohortView();
        auto small_command=limited.device->makeCommandBuffer("Valid uncached asset-cap view",&limited.error);REQUIRE(small_command);
        auto valid=uncached->freezeCohortForCommand(*small_command,selected,limited.globals,&limited.error);REQUIRE(valid);
        REQUIRE(valid->commandMembership());CHECK(uncached->stats().cached_cohorts==0);
        CHECK(uncached->stats().cached_cohort_native_asset_bytes==0);CHECK(uncached->stats().cached_cohort_native_asset_limit==1);
        auto result=limited.encode(*valid,limited.descriptions->sources[0],std::move(small_command));
        REQUIRE(result.command->commitAndWait(&limited.error));
    }
}

TEST_CASE("almetal: optional cohort cache allocation failure preserves a valid view and the previous warm cache",
          "[almetal][world-source-pages][world-cohort]")
{
    @autoreleasepool
    {
        CohortPageFixture f;ALMetalWorldSourcePageLimits limits;limits.maximum_cached_cohorts=1;auto pages=f.pages(limits);
        auto original=f.cohortView();auto command=f.device->makeCommandBuffer("Original one-entry cohort cache",&f.error);REQUIRE(command);
        auto first=pages->freezeCohortForCommand(*command,original,f.globals,&f.error);REQUIRE(first);
        const auto identity=first->commandMembership();const auto before=pages->stats();
        auto second_certificate=ALMetalWorldSourceCohort::seal(f.descriptions->keys,f.descriptions,f.resources,7,&f.error,&CohortPageFixture::resolve);
        REQUIRE(second_certificate);auto incoming=original;incoming.cohort=second_certificate;
        auto next=f.device->makeCommandBuffer("Uncached candidate after cache node OOM",&f.error);REQUIRE(next);
        pages->failNextCohortCacheInsertionForTesting();
        auto valid=pages->freezeCohortForCommand(*next,incoming,f.globals,&f.error);INFO(f.error);REQUIRE(valid);
        CHECK(valid->commandMembership()!=identity);CHECK(pages->stats().cached_cohorts==1);
        CHECK(pages->stats().cached_cohort_native_asset_bytes==before.cached_cohort_native_asset_bytes);
        auto warm_command=f.device->makeCommandBuffer("Old cohort survives incoming cache OOM",&f.error);REQUIRE(warm_command);
        auto warm=pages->freezeCohortForCommand(*warm_command,original,f.globals,&f.error);REQUIRE(warm);
        CHECK(warm->commandMembership()==identity);CHECK(pages->stats().cohort_hits==before.cohort_hits+1);
        CHECK(pages->stats().row_publications==before.row_publications);
        auto encoded=f.encode(*valid,f.descriptions->sources[0],std::move(next));REQUIRE(encoded.command->commitAndWait(&f.error));
        const auto pixels=f.read(*encoded.target);
        CHECK(std::any_of(pixels.depth.begin(),pixels.depth.end(),[](auto value){return value!=0x3f800000u;}));
    }
}

namespace
{
void checkCohortArenaBindRejected(CohortPageFixture& f,const ALMetalWorldSourceFrameView& view,
                                 ALMetalCommandBuffer& command)
{
    ALMetalRenderTargetDescriptor descriptor;descriptor.width=descriptor.height=side;descriptor.color_attachment_count=1;
    descriptor.colors[0].format=ALMetalPixelFormat::rgba8_unorm;
    auto target=ALMetalRenderTarget::create(*f.device,descriptor,&f.error);REQUIRE(target);
    auto pass=target->makeRenderPass({},&f.error);REQUIRE(pass);
    auto encoder=ALMetalRenderEncoder::begin(command,*pass,"Reject wrong cohort arena command",&f.error);REQUIRE(encoder);
    CHECK_FALSE(view.bind(*encoder,command,&f.error));REQUIRE(encoder->end(&f.error));
}
}

TEST_CASE("almetal: cohort views use one shared frame arena publication and preserve native output and binding caches",
          "[almetal][world-source-pages][world-cohort][world-cohort-arena]")
{
    @autoreleasepool
    {
        CohortPageFixture f;auto selected=f.cohortView();selected.view.modelview_projection[12]=-.25f;
        auto standalone=f.pages();auto standalone_command=f.device->makeCommandBuffer("Standalone cohort reference",&f.error);REQUIRE(standalone_command);
        const auto ordinary_before=alMetalResourceAllocationStats();
        auto immutable=standalone->freezeCohortForCommand(*standalone_command,selected,f.globals,&f.error);REQUIRE(immutable);
        const auto ordinary_buffers=alMetalResourceAllocationStats().buffers_created-ordinary_before.buffers_created;
        // Standalone semantics allow another command after the publication
        // command is abandoned; its buffers are immutable independent owners.
        standalone_command.reset();auto reference=f.encode(*immutable,f.descriptions->sources[0]);
        REQUIRE(reference.command->commitAndWait(&f.error));const auto expected=f.read(*reference.target);
        auto arena=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(arena);arena->beginFrame(100);
        auto pages=f.pages();auto command=f.device->makeCommandBuffer("Cold cohort arena publication",&f.error);REQUIRE(command);
        const auto before=alMetalResourceAllocationStats();const auto data_before=arena->stats();
        auto first=pages->freezeCohortForCommand(*command,selected,f.globals,&f.error,arena.get());INFO(f.error);REQUIRE(first);
        const auto published=pages->stats();const auto data_after=arena->stats();
        // The cold path also replaces two old buffers with one arena page.
        CHECK(alMetalResourceAllocationStats().buffers_created-before.buffers_created+1==ordinary_buffers);
        CHECK(data_after.pages_created-data_before.pages_created==1);
        CHECK(data_after.published_bytes-data_before.published_bytes==288);
        CHECK(published.arena_views==1);CHECK(published.arena_view_bytes==288);
        CHECK(published.view_bytes==alMetalWorldSourceMaximumRows*sizeof(std::uint32_t)); // only the once-published map
        auto foreign=f.device->makeCommandBuffer("Different live command cannot borrow arena view",&f.error);REQUIRE(foreign);
        checkCohortArenaBindRejected(f,*first,*foreign);
        auto result=f.encode(*first,f.descriptions->sources[0],std::move(command),true);
        REQUIRE(result.command->commitAndWait(&f.error));CHECK(f.read(*result.target)==expected);
        const auto membership=first->commandMembership();
        // Completed arenas may recycle even while an old FrameView keeps the
        // native buffer alive. Old views cannot turn that into a late bind.
        arena->beginFrame(101);const auto warm_before=alMetalResourceAllocationStats();const auto arena_before=arena->stats();
        std::vector<std::shared_ptr<const ALMetalWorldSourceFrameView>> retained;
        for(unsigned i=0;i<8;++i)
        {
            auto next=f.device->makeCommandBuffer("Warm allocation-free cohort arena publication",&f.error);REQUIRE(next);
            auto warm=pages->freezeCohortForCommand(*next,selected,f.globals,&f.error,arena.get());REQUIRE(warm);
            CHECK(warm->commandMembership()==membership);retained.push_back(std::move(warm));
        }
        CHECK(alMetalResourceAllocationStats().buffers_created==warm_before.buffers_created);
        CHECK(arena->stats().pages_reused-arena_before.pages_reused==1);
        CHECK(arena->stats().published_bytes-arena_before.published_bytes==8*288);
        CHECK(pages->stats().arena_view_bytes-published.arena_view_bytes==8*288);
        CHECK(pages->stats().view_bytes==published.view_bytes);
        auto late=f.device->makeCommandBuffer("Completed or abandoned arena view stays invalid",&f.error);REQUIRE(late);
        checkCohortArenaBindRejected(f,*first,*late);checkCohortArenaBindRejected(f,*retained.back(),*late);
    }
}

TEST_CASE("almetal: pending cohort arena data survives frame retirement and arena destruction without retaining CPU owners",
          "[almetal][world-source-pages][world-cohort][world-cohort-arena]")
{
    @autoreleasepool
    {
        CohortPageFixture f;auto pages=f.pages();auto selected=f.cohortView();
        auto setup=f.device->makeCommandBuffer("Warm source pipeline before arena queue gate",&f.error);REQUIRE(setup);
        auto baseline=pages->freezeCohortForCommand(*setup,selected,f.globals,&f.error);REQUIRE(baseline);
        auto reference=f.encode(*baseline,f.descriptions->sources[0],std::move(setup));REQUIRE(reference.command->commitAndWait(&f.error));
        const auto expected=f.read(*reference.target);
        auto arena=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(arena);arena->beginFrame(1);
        auto command=f.device->makeCommandBuffer("Pending native-only cohort arena view",&f.error);REQUIRE(command);
        auto view=pages->freezeCohortForCommand(*command,selected,f.globals,&f.error,arena.get());REQUIRE(view);
        auto pending=f.encode(*view,f.descriptions->sources[0],std::move(command),true);
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate {id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold pending cohort arena bytes",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        REQUIRE(pending.command->commit(&f.error));arena->beginFrame(2);REQUIRE(arena->stats().retired_pages==1);
        const auto pages_created=arena->stats().pages_created;
        auto another=f.device->makeCommandBuffer("Another frame while old arena page is pending",&f.error);REQUIRE(another);
        auto moved=selected;moved.view.modelview_projection[12]=.5f;
        auto next=pages->freezeCohortForCommand(*another,moved,f.globals,&f.error,arena.get());REQUIRE(next);
        CHECK(arena->stats().pages_created==pages_created+1);CHECK(arena->stats().pages_reused==0);
        std::weak_ptr<ALMetalWorldFrameData> weak_arena=arena;arena.reset();CHECK(weak_arena.expired());
        std::weak_ptr<PageCohortCpu> weak_cpu=f.descriptions;std::weak_ptr<const ALMetalWorldSourceCohort> weak_certificate=f.certificate;
        selected={};moved={};f.certificate.reset();f.descriptions.reset();CHECK(weak_cpu.expired());CHECK(weak_certificate.expired());
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(f.read(*pending.target)==expected);
        auto late=f.device->makeCommandBuffer("Destroyed arena view cannot be rebound after completion",&f.error);REQUIRE(late);
        checkCohortArenaBindRejected(f,*view,*late);
    }
}

TEST_CASE("almetal: abandoned arena commands release pages for reuse while stale views cannot read overwritten data",
          "[almetal][world-source-pages][world-cohort][world-cohort-arena]")
{
    @autoreleasepool
    {
        CohortPageFixture f;auto pages=f.pages();auto selected=f.cohortView();
        auto arena=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(arena);arena->beginFrame(1);
        auto command=f.device->makeCommandBuffer("Abandoned original arena cohort",&f.error);REQUIRE(command);
        auto old=pages->freezeCohortForCommand(*command,selected,f.globals,&f.error,arena.get());REQUIRE(old);
        auto completion=command->completionToken();REQUIRE(completion);command.reset();REQUIRE(completion->isComplete());
        const auto old_stats=arena->stats();arena->beginFrame(2);
        selected.view.modelview_projection[12]=.375f;auto next=f.device->makeCommandBuffer("Reused arena new camera",&f.error);REQUIRE(next);
        const auto before=alMetalResourceAllocationStats();
        auto current=pages->freezeCohortForCommand(*next,selected,f.globals,&f.error,arena.get());REQUIRE(current);
        CHECK(arena->stats().pages_created==old_stats.pages_created);CHECK(arena->stats().pages_reused==old_stats.pages_reused+1);
        CHECK(alMetalResourceAllocationStats().buffers_created==before.buffers_created);
        CHECK(current->commandMembership()==old->commandMembership());
        checkCohortArenaBindRejected(f,*old,*next);
        auto actual=f.encode(*current,f.descriptions->sources[0],std::move(next),true);REQUIRE(actual.command->commitAndWait(&f.error));
        const auto pixels=f.read(*actual.target);
        auto standalone=f.device->makeCommandBuffer("Immutable reference for reused arena camera",&f.error);REQUIRE(standalone);
        auto immutable=pages->freezeCohortForCommand(*standalone,selected,f.globals,&f.error);REQUIRE(immutable);
        auto reference=f.encode(*immutable,f.descriptions->sources[0],std::move(standalone));REQUIRE(reference.command->commitAndWait(&f.error));
        CHECK(f.read(*reference.target)==pixels);
    }
}
