/**
 * @file almetal_world_source_pages_test.mm
 * @brief Source page reflection, sparse COW and native reader retirement.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_source_pages_fixture.h"
#include "../almetalworldcohort.h"
#include "../almetalworldsourcecommands.h"
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

TEST_CASE("almetal: co-resident cohort rows retain independent memberships through denial rewrite and retirement",
          "[almetal][world-source-pages][world-cohort]")
{
    @autoreleasepool
    {
        CohortPageFixture f;ALMetalWorldSourcePageLimits limits;limits.maximum_rows=2;auto pages=f.pages(limits);
        const auto sealOne=[&](std::size_t first)
        {
            auto certificate=ALMetalWorldSourceCohort::seal(std::span(f.descriptions->keys).subspan(first,1),
                f.descriptions,f.resources,7,&f.error,&CohortPageFixture::resolve);REQUIRE(certificate);return certificate;
        };
        auto first=f.cohortView();first.cohort=sealOne(0);auto second=f.cohortView();second.cohort=sealOne(1);
        auto command_a=f.device->makeCommandBuffer("Independent source row A",&f.error);REQUIRE(command_a);
        auto a=pages->freezeCohortForCommand(*command_a,first,f.globals,&f.error);REQUIRE(a);
        const auto membership_a=a->commandMembership();auto command_b=f.device->makeCommandBuffer("Co-resident source row B",&f.error);REQUIRE(command_b);
        auto b=pages->freezeCohortForCommand(*command_b,second,f.globals,&f.error);REQUIRE(b);
        CHECK(a->rowId(0)!=b->rowId(0));REQUIRE(a->rowId(0)/128==b->rowId(0)/128);
        CHECK(pages->stats().cached_cohorts==2); // A's row/table did not change.
        auto reference_a=f.encode(*a,f.descriptions->sources[0],std::move(command_a));REQUIRE(reference_a.command->commitAndWait(&f.error));
        auto reference_b=f.encode(*b,f.descriptions->sources[1],std::move(command_b));REQUIRE(reference_b.command->commitAndWait(&f.error));
        const auto expected_a=f.read(*reference_a.target),expected_b=f.read(*reference_b.target);
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate {id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold both old co-resident rows",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        auto pending_a=f.encode(*a,f.descriptions->sources[0]);auto pending_b=f.encode(*b,f.descriptions->sources[1]);
        REQUIRE(pending_a.command->commit(&f.error));REQUIRE(pending_b.command->commit(&f.error));
        auto changed_cpu=std::make_shared<PageCohortCpu>();changed_cpu->sources.push_back(f.descriptions->sources[1]);
        changed_cpu->sources[0].materialSlots()[0].env_intensity=.85f;
        REQUIRE(ALMetalWorldMaterialSource::publish(*f.device,changed_cpu->sources[0],f.cache,&f.error));
        auto changed_native=std::make_shared<Resources>(*f.resources);
        const auto& material=*changed_cpu->sources[0].native_material_source;
        for(const auto& buffer:{material.materialLease(),material.pbrLease(false),material.pbrLease(true)})
            if(buffer)changed_native->buffers.push_back(buffer);
        changed_cpu->keys.push_back({{2,1,2},&changed_cpu->sources[0]});
        auto changed_certificate=ALMetalWorldSourceCohort::seal(changed_cpu->keys,changed_cpu,changed_native,7,&f.error,&CohortPageFixture::resolve);
        REQUIRE(changed_certificate);auto changed=second;changed.cohort=changed_certificate;changed.cpu_owner=changed_cpu;changed.native_owner=changed_native;
        unsigned denied_calls=0;pages->setPublicationAdmission([&](const auto&)->std::shared_ptr<const void>{++denied_calls;return {};});
        auto denied=f.device->makeCommandBuffer("Denied dirty B preserves A",&f.error);REQUIRE(denied);
        const auto before=pages->stats();CHECK_FALSE(pages->freezeCohortForCommand(*denied,changed,f.globals,&f.error));REQUIRE(denied_calls==1);
        auto warm_command=f.device->makeCommandBuffer("A remains warm during B denial",&f.error);REQUIRE(warm_command);
        auto warm=pages->freezeCohortForCommand(*warm_command,first,f.globals,&f.error);REQUIRE(warm);
        CHECK(warm->commandMembership()==membership_a);CHECK(denied_calls==1);CHECK(pages->stats().row_publications==before.row_publications);
        pages->setPublicationAdmission({});pages->invalidate(2,2);CHECK(pages->stats().cached_cohorts==1);
        auto replacement_command=f.device->makeCommandBuffer("Rewrite only B at same allocation generation",&f.error);REQUIRE(replacement_command);
        auto replacement=pages->freezeCohortForCommand(*replacement_command,changed,f.globals,&f.error);REQUIRE(replacement);
        CHECK(replacement->rowId(0)==b->rowId(0));CHECK(replacement->allocationGeneration(0)==b->allocationGeneration(0));
        CHECK(pages->stats().cached_cohorts==2);CHECK(pages->stats().copied_page_bytes>before.copied_page_bytes);
        auto current_command=f.device->makeCommandBuffer("A selects current copied page",&f.error);REQUIRE(current_command);
        auto current_a=pages->freezeCohortForCommand(*current_command,first,f.globals,&f.error);REQUIRE(current_a);
        CHECK(current_a->commandMembership()==membership_a);
        auto current_pixels=f.encode(*current_a,f.descriptions->sources[0],std::move(current_command));REQUIRE(current_pixels.command->commit(&f.error));
        auto changed_pixels=f.encode(*replacement,changed_cpu->sources[0],std::move(replacement_command));REQUIRE(changed_pixels.command->commit(&f.error));
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));
        CHECK(f.read(*pending_a.target)==expected_a);CHECK(f.read(*pending_b.target)==expected_b);
        CHECK(f.read(*current_pixels.target)==expected_a);CHECK_FALSE(f.read(*changed_pixels.target)==expected_b);
        pages->withdraw(2);CHECK(pages->stats().cached_cohorts==1);
        auto restore_command=f.device->makeCommandBuffer("Cache-only B withdrawal preserves A",&f.error);REQUIRE(restore_command);
        auto restored=pages->freezeCohortForCommand(*restore_command,changed,f.globals,&f.error);REQUIRE(restored);
        auto next_a=f.device->makeCommandBuffer("Unchanged A after B cache restore",&f.error);REQUIRE(next_a);
        auto still_a=pages->freezeCohortForCommand(*next_a,first,f.globals,&f.error);REQUIRE(still_a);
        CHECK(still_a->commandMembership()==membership_a);CHECK(pages->stats().cached_cohorts==2);
        // Clearing/destroying entries must unlink their reverse nodes while
        // actual views independently keep old native pages/tables usable.
        pages->clear();CHECK(pages->stats().cached_cohorts==0);pages->withdraw(2);
        auto rebuild=f.device->makeCommandBuffer("Reuse row heads after clear",&f.error);REQUIRE(rebuild);
        auto rebuilt=pages->freezeCohortForCommand(*rebuild,first,f.globals,&f.error);REQUIRE(rebuilt);
        CHECK(rebuilt->commandMembership()!=membership_a);pages.reset();
        auto after_destroy=f.encode(*rebuilt,f.descriptions->sources[0],std::move(rebuild));REQUIRE(after_destroy.command->commitAndWait(&f.error));
        CHECK(f.read(*after_destroy.target)==expected_a);
    }
}

TEST_CASE("almetal: bounded many small Legacy and PBR cohorts converge without sibling table invalidation",
          "[almetal][world-source-pages][world-cohort][world-source-admission]")
{
    @autoreleasepool
    {
        CohortPageFixture f;auto pbr=f.batch(2);REQUIRE(ALMetalWorldMaterialSource::publish(*f.device,pbr,f.cache,&f.error));
        for(const auto& buffer:{pbr.native_material_source->pbrLease(false),pbr.native_material_source->pbrLease(true)})
            if(buffer)f.resources->buffers.push_back(buffer);
        std::vector<std::shared_ptr<PageCohortCpu>> cpu;
        std::vector<ALMetalWorldSourceCohortView> groups;
        for(unsigned group=0;group<64;++group)
        {
            auto backing=std::make_shared<PageCohortCpu>();backing->sources.reserve(2);backing->keys.reserve(2);
            const auto family=group%2?2u:1u;
            for(unsigned row=0;row<2;++row)backing->sources.push_back(family==1?f.descriptions->sources[0]:pbr);
            for(unsigned row=0;row<2;++row)backing->keys.push_back({{100+group*2+row,family,1},&backing->sources[row]});
            auto certificate=ALMetalWorldSourceCohort::seal(backing->keys,backing,f.resources,7,&f.error,&CohortPageFixture::resolve);
            REQUIRE(certificate);auto selected=f.cohortView();selected.cohort=certificate;selected.cpu_owner=backing;
            cpu.push_back(std::move(backing));groups.push_back(std::move(selected));
        }
        ALMetalWorldSourcePageLimits limits;limits.maximum_rows=128;auto pages=f.pages(limits);
        ALNativeFrameBudget budget;pages->setPublicationAdmission([&](const auto& work)->std::shared_ptr<const void>
        {return budget.tryAcquire(ALNativeFrameBudget::Work::scene_publication,work.chargedBytes(),budget.snapshot().frame).credit;});
        std::vector<std::shared_ptr<const ALMetalWorldSourceCommandMembership>> stable(64);
        for(unsigned frame=1;frame<=10;++frame)
        {
            ALNativeFrameBudget::FrameInputs input;input.frame=frame;budget.beginFrame(input);
            const auto before=pages->stats();std::vector<std::shared_ptr<const ALMetalWorldSourceFrameView>> views;
            for(unsigned group=0;group<64;++group)
            {
                auto command=f.device->makeCommandBuffer("Progressive independent small cohort publication",&f.error);REQUIRE(command);
                auto view=pages->freezeCohortForCommand(*command,groups[group],f.globals,&f.error);
                if(frame>=8)REQUIRE(view);
                if(view)
                {
                    if(stable[group])CHECK(view->commandMembership()==stable[group]);
                    else stable[group]=view->commandMembership();
                    views.push_back(std::move(view));
                }
            }
            if(frame>=8)
            {
                CHECK(pages->stats().cohort_hits-before.cohort_hits==64);CHECK(pages->stats().cohort_builds==before.cohort_builds);
                CHECK(pages->stats().admission_requests==before.admission_requests);CHECK(pages->stats().admission_denials==before.admission_denials);
                CHECK(pages->stats().row_publications==before.row_publications);CHECK(pages->stats().cached_cohorts==64);
                CHECK(pages->stats().cached_cohort_rows==128);CHECK(pages->stats().cached_cohort_metadata_bytes<=limits.maximum_cached_cohort_metadata_bytes);
                CHECK(pages->stats().rows==128);
            }
            CHECK(budget.snapshot().scene_publications_in_flight==0);CHECK(budget.snapshot().scene_bytes_in_flight==0);
        }
        pages->clear();CHECK(pages->stats().cached_cohorts==0);CHECK(pages->stats().cached_cohort_rows==0);
        CHECK(pages->stats().cached_cohort_metadata_bytes==0);
    }
}

namespace
{
PageFixture::Encoded encodeOrderedPass(CohortPageFixture& f,
    std::span<const std::shared_ptr<const ALMetalWorldSourceFrameView>> frames,
    std::span<const ALMetalWorldSimpleBatch* const> sources,
    std::unique_ptr<ALMetalCommandBuffer> command,
    std::shared_ptr<const ALMetalBuffer>& queries,ALMetalWorldSourceCommands* commands=nullptr,
    const ALMetalWorldSourceCommands::Batch* bank=nullptr)
{
    REQUIRE(command);REQUIRE_FALSE(frames.empty());REQUIRE_FALSE(sources.empty());
    const bool aggregate=frames.size()==1;const auto ranges=aggregate?frames[0]->cohortRanges():std::span<const ALMetalWorldSourceCohortRange>{};
    const auto count=aggregate?ranges.size():frames.size();REQUIRE(count==sources.size());
    ALMetalRenderPipelineDescriptor pd;pd.library=f.library.get();pd.support_indirect_commands=true;
    const bool pbr=sources[0]->pbr_slot_count!=0;
    pd.vertex_function=pbr?"almetal_world_pbr_source_rows_vertex":"almetal_world_material_source_rows_vertex";
    pd.fragment_function=pbr?"almetal_world_pbr_source_rows_fragment":"almetal_world_material_source_rows_fragment";
    pd.color_attachment_count=4;for(unsigned slot=0;slot<4;++slot)pd.colors[slot].format=ALMetalPixelFormat::rgba8_unorm;
    pd.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
    auto pipeline=ALMetalRenderPipeline::create(*f.device,pd,&f.error);REQUIRE(pipeline);
    ALMetalRenderTargetDescriptor td;td.width=td.height=side;td.color_attachment_count=4;
    for(unsigned slot=0;slot<4;++slot)td.colors[slot].format=ALMetalPixelFormat::rgba8_unorm;
    td.depth_stencil_format=pd.depth_stencil_format;PageFixture::Encoded output;
    output.target=ALMetalRenderTarget::create(*f.device,td,&f.error);REQUIRE(output.target);output.command=std::move(command);
    ALMetalBufferDescriptor q;q.length=count*sizeof(std::uint64_t);q.storage=ALMetalStorageMode::shared;
    queries=ALMetalBuffer::create(*f.device,q,&f.error);REQUIRE(queries);std::memset(queries->mappedBytes(),0,q.length);
    ALMetalRenderPassConfig config;config.depth_store=config.stencil_store=ALMetalStoreAction::store;
    auto pass=output.target->makeRenderPass(config,&f.error);REQUIRE(pass);
    ((__bridge MTLRenderPassDescriptor*)pass->nativeHandle()).visibilityResultBuffer=(__bridge id<MTLBuffer>)queries->nativeHandle();
    auto encoder=ALMetalRenderEncoder::begin(*output.command,*pass,"Ordered source pass query boundaries",&f.error);REQUIRE(encoder);
    ALMetalDepthStencilDescriptor dd;auto depth=ALMetalDepthStencilState::create(*f.device,dd,&f.error);REQUIRE(depth);
    REQUIRE(encoder->setPipeline(*pipeline,&f.error));REQUIRE(encoder->setDepthStencilState(depth.get(),&f.error));
    REQUIRE(encoder->setViewport({0,0,side,side,0,1},&f.error));REQUIRE(encoder->setScissor({0,0,side,side},&f.error));
    REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&f.error));
    for(std::size_t group=0;group<count;++group)
    {
        REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::counting,group*sizeof(std::uint64_t),&f.error));
        const auto& frame=*frames[aggregate?0:group];
        if(commands && bank)REQUIRE(commands->executeRange(*encoder,*output.command,*bank,ranges[group].first,ranges[group].count,&f.error));
        else
        {
            REQUIRE(frame.bind(*encoder,*output.command,&f.error));const auto& source=*sources[group];
            const auto first=aggregate?ranges[group].first:0,draws=aggregate?ranges[group].count:frame.size();
            for(std::size_t draw=0;draw<draws;++draw)
                REQUIRE(encoder->drawIndexedWithBaseInstance(ALMetalPrimitiveType::triangle,source.index_count,source.index_type,
                    *source.indices,source.index_offset,frame.rowId(first+draw),&f.error));
        }
    }
    REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::disabled,0,&f.error));REQUIRE(encoder->end(&f.error));return output;
}
std::vector<std::uint64_t> passQueries(const ALMetalBuffer& buffer)
{
    std::vector<std::uint64_t> values(buffer.length()/sizeof(std::uint64_t));
    std::memcpy(values.data(),buffer.mappedBytes(),buffer.length());return values;
}
}

TEST_CASE("almetal: ordered native passes share mixed-region publication and unchanged command ranges with exact query output",
          "[almetal][world-source-pages][world-cohort][world-source-pass]")
{
    @autoreleasepool
    {
        for(unsigned family:{1u,2u})
        {
            CAPTURE(family);CohortPageFixture f(family,2);auto pages=f.pages();
            std::array<ALMetalWorldSourceCohortView,2> groups;
            for(unsigned group=0;group<2;++group)
            {
                groups[group]=f.cohortView();groups[group].cohort=ALMetalWorldSourceCohort::seal(
                    std::span(f.descriptions->keys).subspan(group,1),f.descriptions,f.resources,7+group,&f.error,&CohortPageFixture::resolve);
                REQUIRE(groups[group].cohort);groups[group].logical_begin=3+group*4;
                groups[group].view.modelview_projection[12]=group?.4f:-.4f;
            }
            auto reference_command=f.device->makeCommandBuffer("Separate regional view reference",&f.error);REQUIRE(reference_command);
            std::array<std::shared_ptr<const ALMetalWorldSourceFrameView>,2> separate;
            for(unsigned group=0;group<2;++group)
            {separate[group]=pages->freezeCohortForCommand(*reference_command,groups[group],f.globals,&f.error);REQUIRE(separate[group]);}
            const std::array<const ALMetalWorldSimpleBatch*,2> sources{&f.descriptions->sources[0],&f.descriptions->sources[1]};
            std::shared_ptr<const ALMetalBuffer> expected_queries;
            auto reference=encodeOrderedPass(f,separate,sources,std::move(reference_command),expected_queries);
            REQUIRE(reference.command->commitAndWait(&f.error));const auto expected=f.read(*reference.target);
            const auto counts=passQueries(*expected_queries);REQUIRE(counts[0]>0);REQUIRE(counts[1]>0);
            auto arena=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(arena);arena->beginFrame(1);
            auto command=f.device->makeCommandBuffer("One mixed-region ordered native pass",&f.error);REQUIRE(command);
            const auto before=arena->stats();auto frame=pages->freezeCohortsForCommand(*command,groups,f.globals,&f.error,arena.get());INFO(f.error);REQUIRE(frame);
            REQUIRE(frame->size()==2);REQUIRE(frame->cohortRanges().size()==2);
            CHECK(frame->cohortRanges()[0].logical_begin==3);CHECK(frame->cohortRanges()[1].logical_begin==7);
            CHECK(frame->cohortRanges()[0].first==0);CHECK(frame->cohortRanges()[1].first==1);
            CHECK(arena->stats().published_bytes-before.published_bytes==544); //2*176, aligned globals512+32
            auto commands=ALMetalWorldSourceCommands::create(*f.device,{},&f.error);REQUIRE(commands);
            auto bank=commands->prepareCohortForCommand(*command,frame,&f.error);INFO(f.error);REQUIRE(bank);CHECK(bank->size()==2);
            const std::array<std::shared_ptr<const ALMetalWorldSourceFrameView>,1> aggregate{frame};
            std::shared_ptr<const ALMetalBuffer> actual_queries;
            auto actual=encodeOrderedPass(f,aggregate,sources,std::move(command),actual_queries,commands.get(),bank.get());
            REQUIRE(actual.command->commitAndWait(&f.error));CHECK(f.read(*actual.target)==expected);CHECK(passQueries(*actual_queries)==counts);
            const auto old=pages->stats();const auto membership=frame->commandMembership();
            for(auto& group:groups)group.view.modelview_projection[13]=.125f;
            auto next=f.device->makeCommandBuffer("Warm pass other camera",&f.error);REQUIRE(next);
            auto warm=pages->freezeCohortsForCommand(*next,groups,f.globals,&f.error,arena.get());REQUIRE(warm);
            CHECK(warm->commandMembership()==membership);CHECK(pages->stats().requests==old.requests);
            CHECK(pages->stats().row_publications==old.row_publications);CHECK(pages->stats().arena_views==old.arena_views+1);
            auto reused=commands->prepareCohortForCommand(*next,warm,&f.error);REQUIRE(reused);CHECK(commands->stats().hits==1);
        }
    }
}

TEST_CASE("almetal: sixty-four and256 cohort compositions share one view without per-source warm expansion",
          "[almetal][world-source-pages][world-cohort][world-source-pass]")
{
    @autoreleasepool
    {
        for(unsigned count:{64u,256u})
        {
            CAPTURE(count);CohortPageFixture f;auto pages=f.pages();
            std::vector<std::shared_ptr<PageCohortCpu>> owners;
            std::vector<ALMetalWorldSourceCohortView> groups;
            for(unsigned group=0;group<count;++group)
            {
                auto cpu=std::make_shared<PageCohortCpu>();cpu->sources.assign(2,f.descriptions->sources[0]);
                for(unsigned row=0;row<2;++row)cpu->keys.push_back({{100+group*2+row,1,1},&cpu->sources[row]});
                auto selected=f.cohortView();selected.cpu_owner=cpu;selected.logical_begin=group*2;
                selected.cohort=ALMetalWorldSourceCohort::seal(cpu->keys,cpu,f.resources,7,&f.error,&CohortPageFixture::resolve);REQUIRE(selected.cohort);
                owners.push_back(std::move(cpu));groups.push_back(std::move(selected));
            }
            auto arena=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(arena);arena->beginFrame(1);
            auto command=f.device->makeCommandBuffer("Cold large ordered cohort composition",&f.error);REQUIRE(command);
            auto cold=pages->freezeCohortsForCommand(*command,groups,f.globals,&f.error,arena.get());INFO(f.error);REQUIRE(cold);
            const auto first=pages->stats();CHECK(first.cached_compositions==1);CHECK(first.cached_cohorts==count);
            CHECK(first.cached_cohort_rows==count*2);CHECK(first.cached_cohort_metadata_bytes<=8*1024*1024);
            CHECK(first.arena_views==1);CHECK(first.arena_view_bytes==288);CHECK(cold->cohortRanges().size()==count);
            const auto before=arena->stats();const auto allocated=alMetalResourceAllocationStats();
            auto next=f.device->makeCommandBuffer("Warm large ordered cohort composition",&f.error);REQUIRE(next);
            auto warm=pages->freezeCohortsForCommand(*next,groups,f.globals,&f.error,arena.get());REQUIRE(warm);
            CHECK(warm->commandMembership()==cold->commandMembership());CHECK(pages->stats().requests==first.requests);
            CHECK(pages->stats().row_publications==first.row_publications);CHECK(pages->stats().admission_requests==first.admission_requests);
            CHECK(pages->stats().arena_views==first.arena_views+1);CHECK(arena->stats().published_bytes-before.published_bytes==288);
            CHECK(alMetalResourceAllocationStats().buffers_created==allocated.buffers_created);
            for(unsigned group=0;group<count;++group)
            {CHECK(warm->cohortRanges()[group].first==group*2);CHECK(warm->cohortRanges()[group].count==2);}
            // Failure after staging the composition/watch array must not evict
            // this usable membership; a reordered set is valid but uncached.
            auto reordered=groups;std::reverse(reordered.begin(),reordered.end());
            for(unsigned group=0;group<count;++group)reordered[group].logical_begin=group*2;
            pages->failNextCohortCacheInsertionForTesting();auto retry=f.device->makeCommandBuffer("Composition cache OOM",&f.error);REQUIRE(retry);
            auto uncached=pages->freezeCohortsForCommand(*retry,reordered,f.globals,&f.error,arena.get());REQUIRE(uncached);
            CHECK(pages->stats().cached_compositions==1);CHECK(uncached->commandMembership()!=cold->commandMembership());
            auto again=f.device->makeCommandBuffer("Original pass remains warm after optional cache OOM",&f.error);REQUIRE(again);
            auto stable=pages->freezeCohortsForCommand(*again,groups,f.globals,&f.error,arena.get());REQUIRE(stable);
            CHECK(stable->commandMembership()==cold->commandMembership());
        }
    }
}

TEST_CASE("almetal: ordered pass transform ambiguity and malformed membership fail before native row writes",
          "[almetal][world-source-pages][world-cohort][world-source-pass]")
{
    @autoreleasepool
    {
        CohortPageFixture f;auto pages=f.pages();auto first=f.cohortView();
        first.cohort=ALMetalWorldSourceCohort::seal(std::span(f.descriptions->keys).first(1),f.descriptions,f.resources,7,&f.error,&CohortPageFixture::resolve);REQUIRE(first.cohort);
        auto second=first;second.logical_begin=1;second.view.modelview_projection[12]=.25f;
        std::array groups{first,second};auto command=f.device->makeCommandBuffer("Reject incompatible pass regional mapping",&f.error);REQUIRE(command);
        CHECK_FALSE(pages->freezeCohortsForCommand(*command,groups,f.globals,&f.error));CHECK(pages->stats().row_publications==0);
        // A repeated row under different region identities is equally
        // ambiguous for the shader's one rowID->region mapping.
        groups[1].cohort=ALMetalWorldSourceCohort::seal(std::span(f.descriptions->keys).first(1),f.descriptions,f.resources,8,&f.error,&CohortPageFixture::resolve);
        REQUIRE(groups[1].cohort);CHECK_FALSE(pages->freezeCohortsForCommand(*command,groups,f.globals,&f.error));
        CHECK(pages->stats().row_publications==0);CHECK(pages->stats().page_allocations==0);
        groups[1]=first;groups[1].logical_begin=0;CHECK_FALSE(pages->freezeCohortsForCommand(*command,groups,f.globals,&f.error));
        groups[1]=first;groups[1].logical_begin=1;groups[1].native_owner=std::make_shared<unsigned>(1);
        CHECK_FALSE(pages->freezeCohortsForCommand(*command,groups,f.globals,&f.error));CHECK(pages->stats().cached_compositions==0);
    }
}

TEST_CASE("almetal: ordered pass generations preserve pending pixels and retire every native tuple without CPU graphs",
          "[almetal][world-source-pages][world-cohort][world-source-pass]")
{
    @autoreleasepool
    {
        CohortPageFixture f;auto pages=f.pages();
        std::array<std::shared_ptr<PageCohortCpu>,2> owners;
        std::array<std::shared_ptr<Resources>,2> native;
        std::array<ALMetalWorldSourceCohortView,2> groups;
        std::array<const ALMetalWorldSimpleBatch*,2> sources;
        for(unsigned group=0;group<2;++group)
        {
            owners[group]=std::make_shared<PageCohortCpu>();owners[group]->sources.push_back(f.descriptions->sources[group]);
            owners[group]->keys.push_back({{group+1,1,1},&owners[group]->sources[0]});native[group]=std::make_shared<Resources>(*f.resources);
            groups[group]=f.cohortView();groups[group].cpu_owner=owners[group];groups[group].native_owner=native[group];
            groups[group].cohort=ALMetalWorldSourceCohort::seal(owners[group]->keys,owners[group],native[group],7+group,&f.error,&CohortPageFixture::resolve);
            REQUIRE(groups[group].cohort);groups[group].logical_begin=group;groups[group].view.modelview_projection[12]=group?.4f:-.4f;
            sources[group]=&owners[group]->sources[0];
        }
        auto reference_command=f.device->makeCommandBuffer("Original ordered pass reference",&f.error);REQUIRE(reference_command);
        std::array<std::shared_ptr<const ALMetalWorldSourceFrameView>,2> separate;
        for(unsigned group=0;group<2;++group)
        {separate[group]=pages->freezeCohortForCommand(*reference_command,groups[group],f.globals,&f.error);REQUIRE(separate[group]);}
        std::shared_ptr<const ALMetalBuffer> reference_queries;
        auto reference=encodeOrderedPass(f,separate,sources,std::move(reference_command),reference_queries);
        REQUIRE(reference.command->commitAndWait(&f.error));const auto expected=f.read(*reference.target);separate={};
        auto arena=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(arena);arena->beginFrame(1);
        auto command=f.device->makeCommandBuffer("Old ordered native pass remains uncommitted",&f.error);REQUIRE(command);
        auto old=pages->freezeCohortsForCommand(*command,groups,f.globals,&f.error,arena.get());REQUIRE(old);
        const auto old_membership=old->commandMembership();std::shared_ptr<const ALMetalBuffer> old_queries;
        auto pending=encodeOrderedPass(f,std::span(&old,1),sources,std::move(command),old_queries);
        // New immutable CPU/native owners for just B; old tuple contents and
        // private snapshots remain untouched while its command is queued.
        auto changed_cpu=std::make_shared<PageCohortCpu>();changed_cpu->sources.push_back(owners[1]->sources[0]);
        changed_cpu->sources[0].materialSlots()[0].env_intensity=.9f;
        REQUIRE(ALMetalWorldMaterialSource::publish(*f.device,changed_cpu->sources[0],f.cache,&f.error));
        auto changed_native=std::make_shared<Resources>(*native[1]);
        changed_native->buffers.push_back(changed_cpu->sources[0].native_material_source->materialLease());
        changed_cpu->keys.push_back({{2,1,2},&changed_cpu->sources[0]});auto changed=groups;
        changed[1].cpu_owner=changed_cpu;changed[1].native_owner=changed_native;
        changed[1].cohort=ALMetalWorldSourceCohort::seal(changed_cpu->keys,changed_cpu,changed_native,8,&f.error,&CohortPageFixture::resolve);REQUIRE(changed[1].cohort);
        unsigned denials=0;pages->setPublicationAdmission([&](const auto&)->std::shared_ptr<const void>{++denials;return {};});
        const auto before=pages->stats();auto denied=f.device->makeCommandBuffer("Denied changed pass",&f.error);REQUIRE(denied);
        CHECK_FALSE(pages->freezeCohortsForCommand(*denied,changed,f.globals,&f.error,arena.get()));CHECK(denials==1);
        CHECK(pages->stats().row_publications==before.row_publications);
        auto unchanged=f.device->makeCommandBuffer("Original pass survives denied work",&f.error);REQUIRE(unchanged);
        auto warm=pages->freezeCohortsForCommand(*unchanged,groups,f.globals,&f.error,arena.get());REQUIRE(warm);
        CHECK(warm->commandMembership()==old_membership);CHECK(denials==1);warm.reset();unchanged.reset();
        pages->setPublicationAdmission({});auto next=f.device->makeCommandBuffer("Publish changed B with old pass readers",&f.error);REQUIRE(next);
        auto current=pages->freezeCohortsForCommand(*next,changed,f.globals,&f.error,arena.get());REQUIRE(current);
        CHECK(current->commandMembership()!=old_membership);CHECK(current->rowId(0)==old->rowId(0));
        CHECK(current->allocationGeneration(0)==old->allocationGeneration(0));CHECK(pages->stats().copied_page_bytes>before.copied_page_bytes);
        auto changed_sources=sources;changed_sources[1]=&changed_cpu->sources[0];std::shared_ptr<const ALMetalBuffer> changed_queries;
        auto replacement=encodeOrderedPass(f,std::span(&current,1),changed_sources,std::move(next),changed_queries);
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate {id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold both pass generations",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        REQUIRE(pending.command->commit(&f.error));REQUIRE(replacement.command->commit(&f.error));
        std::array<std::weak_ptr<PageCohortCpu>,2> cpu_weak{owners[0],owners[1]};
        std::array<std::weak_ptr<Resources>,2> native_weak{native[0],native[1]};
        std::weak_ptr<PageCohortCpu> changed_cpu_weak=changed_cpu;std::weak_ptr<Resources> changed_native_weak=changed_native;
        groups={};changed={};owners={};native={};changed_cpu.reset();changed_native.reset();old.reset();current.reset();
        CHECK(cpu_weak[0].expired());CHECK(cpu_weak[1].expired());CHECK(changed_cpu_weak.expired());
        CHECK_FALSE(native_weak[0].expired());CHECK_FALSE(native_weak[1].expired());CHECK_FALSE(changed_native_weak.expired());
        pages->trimUnusedCohorts();CHECK(pages->stats().cached_compositions==0);
        arena.reset();gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));
        CHECK(f.read(*pending.target)==expected);CHECK_FALSE(f.read(*replacement.target)==expected);
        waitPageResources(native_weak[0]);waitPageResources(native_weak[1]);waitPageResources(changed_native_weak);
    }
}

TEST_CASE("almetal: mapping-only cold pass publication is admitted before allocation and owned by its actual command",
          "[almetal][world-source-pages][world-cohort][world-source-pass][world-source-admission]")
{
    @autoreleasepool
    {
        CohortPageFixture f;auto pages=f.pages();std::array<ALMetalWorldSourceCohortView,2> groups;
        for(unsigned group=0;group<2;++group)
        {
            groups[group]=f.cohortView();groups[group].logical_begin=group;
            groups[group].view.modelview_projection[12]=group?.4f:-.4f;
            groups[group].cohort=ALMetalWorldSourceCohort::seal(std::span(f.descriptions->keys).subspan(group,1),
                f.descriptions,f.resources,7+group,&f.error,&CohortPageFixture::resolve);REQUIRE(groups[group].cohort);
        }
        auto original_command=f.device->makeCommandBuffer("Original warm rows and tables for mapping admission",&f.error);REQUIRE(original_command);
        auto original=pages->freezeCohortsForCommand(*original_command,groups,f.globals,&f.error);REQUIRE(original);
        const auto membership=original->commandMembership();const auto native_rows=pages->stats().row_publications;
        const auto anotherComposition=[&]
        {
            auto result=groups;
            result[0].cohort=ALMetalWorldSourceCohort::seal(std::span(f.descriptions->keys).first(1),f.descriptions,
                f.resources,7,&f.error,&CohortPageFixture::resolve);REQUIRE(result[0].cohort);return result;
        };
        auto incoming=anotherComposition();ALMetalWorldSourcePublicationWork observed;unsigned calls=0;
        pages->setPublicationAdmission([&](const auto& work)->std::shared_ptr<const void>{++calls;observed=work;return {};});
        const auto before=pages->stats();const auto allocations=alMetalResourceAllocationStats();
        auto denied=f.device->makeCommandBuffer("Deny mapping-only composition",&f.error);REQUIRE(denied);
        CHECK_FALSE(pages->freezeCohortsForCommand(*denied,incoming,f.globals,&f.error));REQUIRE(calls==1);
        CHECK(observed.encoded_mapping_bytes==2*sizeof(std::uint32_t));CHECK(observed.chargedBytes()==observed.encoded_mapping_bytes);
        CHECK(observed.allocation_bytes==observed.encoded_mapping_bytes);
        CHECK(observed.encoded_row_bytes==0);CHECK(observed.encoded_table_bytes==0);CHECK(observed.encoded_bank_range_bytes==0);
        CHECK(observed.copied_page_bytes==0);CHECK(observed.copied_bank_bytes==0);CHECK(observed.zeroed_page_bytes==0);CHECK(observed.zeroed_bank_bytes==0);
        CHECK(alMetalResourceAllocationStats().buffers_created==allocations.buffers_created);
        CHECK(pages->stats().encoded_mapping_bytes==before.encoded_mapping_bytes);CHECK(pages->stats().row_publications==native_rows);
        CHECK(pages->stats().cached_compositions==1);
        auto warm_command=f.device->makeCommandBuffer("Warm old composition bypasses denied publication",&f.error);REQUIRE(warm_command);
        auto warm=pages->freezeCohortsForCommand(*warm_command,groups,f.globals,&f.error);REQUIRE(warm);
        CHECK(warm->commandMembership()==membership);CHECK(calls==1);
        ALNativeFrameBudget budget;ALNativeFrameBudget::FrameInputs input;input.frame=1;budget.beginFrame(input);
        pages->setPublicationAdmission([&](const auto& work)->std::shared_ptr<const void>
        {++calls;observed=work;return budget.tryAcquire(ALNativeFrameBudget::Work::scene_publication,work.chargedBytes(),1).credit;});
        const auto waitCredit=[&]
        {
            const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
            while(budget.snapshot().scene_publications_in_flight&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
            REQUIRE(budget.snapshot().scene_publications_in_flight==0);CHECK(budget.snapshot().scene_bytes_in_flight==0);
        };
        auto abandoned=f.device->makeCommandBuffer("Abandon admitted mapping-only work",&f.error);REQUIRE(abandoned);
        auto held=pages->freezeCohortsForCommand(*abandoned,incoming,f.globals,&f.error);REQUIRE(held);
        CHECK(observed.chargedBytes()==2*sizeof(std::uint32_t));CHECK(budget.snapshot().scene_bytes_in_flight==observed.chargedBytes());
        CHECK(pages->stats().encoded_mapping_bytes-before.encoded_mapping_bytes==observed.encoded_mapping_bytes);
        CHECK(pages->stats().row_publications==native_rows);abandoned.reset();waitCredit();REQUIRE(held);
        // A second fresh certificate still needs a new retained map, even
        // though all rows, tables and their GPU argument addresses are reused.
        input.frame=2;budget.beginFrame(input);auto next_groups=anotherComposition();
        auto command=f.device->makeCommandBuffer("Commit admitted mapping-only work",&f.error);REQUIRE(command);
        auto frame=pages->freezeCohortsForCommand(*command,next_groups,f.globals,&f.error);REQUIRE(frame);
        CHECK(observed.chargedBytes()==observed.encoded_mapping_bytes);REQUIRE(budget.snapshot().scene_publications_in_flight==1);
        const std::array<const ALMetalWorldSimpleBatch*,2> sources{&f.descriptions->sources[0],&f.descriptions->sources[1]};
        std::shared_ptr<const ALMetalBuffer> queries;auto encoded=encodeOrderedPass(f,std::span(&frame,1),sources,std::move(command),queries);
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate {id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold map publication receipt",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        REQUIRE(encoded.command->commit(&f.error));CHECK(budget.snapshot().scene_bytes_in_flight==observed.encoded_mapping_bytes);
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));waitCredit();REQUIRE(frame);
        const auto counts=passQueries(*queries);CHECK(counts[0]>0);CHECK(counts[1]>0);
        CHECK(budget.snapshot().uploads_in_flight==0);CHECK(budget.snapshot().optional_views_in_flight==0);
    }
}

TEST_CASE("almetal: direct cohort slots preserve exact per-command cameras and reuse only retired readers",
          "[almetal][world-source-pages][world-cohort][world-direct-bindings]")
{
    @autoreleasepool
    {
        for(unsigned family:{1u,2u})
        {
            CAPTURE(family);CohortPageFixture f(family,4);auto pages=f.pages();auto cohort=f.cohortView();
            auto command=f.device->makeCommandBuffer("First fixed direct cohort view",&f.error);REQUIRE(command);
            auto first=pages->freezeCohortsForCommand(*command,std::span(&cohort,1),f.globals,&f.error,nullptr,true);INFO(f.error);REQUIRE(first);
            REQUIRE(first->directBindings());auto original=first->directBindingSlot();REQUIRE(original);
            const auto membership=first->commandMembership();REQUIRE(membership);REQUIRE(membership->directBindings().size()==4);
            const auto cold=pages->stats();CHECK(cold.direct_policy_bytes==4*96);CHECK(cold.direct_slot_allocations==1);
            auto copy=cohort;copy.view.modelview_projection[12]=.25f;auto flags=f.globals;flags.flags[2]=1;
            auto other=f.device->makeCommandBuffer("Second incomplete direct cohort view",&f.error);REQUIRE(other);
            auto second=pages->freezeCohortsForCommand(*other,std::span(&copy,1),flags,&f.error,nullptr,true);INFO(f.error);REQUIRE(second);
            CHECK(second->commandMembership()==membership);CHECK(second->directBindingSlot()!=original);CHECK(second->directNormalHDR());
            CHECK(pages->stats().direct_policy_bytes==cold.direct_policy_bytes);CHECK(pages->stats().row_publications==cold.row_publications);
            const auto* old_matrix=static_cast<const ALMetalWorldSourceRegion*>([(__bridge id<MTLBuffer>)original->nativeBuffer() contents]);
            const auto* new_matrix=static_cast<const ALMetalWorldSourceRegion*>([(__bridge id<MTLBuffer>)second->directBindingSlot()->nativeBuffer() contents]);
            REQUIRE(old_matrix);REQUIRE(new_matrix);CHECK(old_matrix->modelview_projection[12]==0);CHECK(new_matrix->modelview_projection[12]==.25f);
            const std::weak_ptr<const ALMetalWorldSourceFrameView> weak=first;
            REQUIRE(command->retainUntilCompletion(first,&f.error));first.reset();CHECK_FALSE(weak.expired());
            auto completion=command->completionToken();command.reset();CHECK(completion->isComplete());CHECK(weak.expired());
            // Keeping slot STORAGE (as a cached ICB does) is not a reader.
            auto recovered=f.device->makeCommandBuffer("Recycled fixed direct cohort view",&f.error);REQUIRE(recovered);
            auto third=pages->freezeCohortsForCommand(*recovered,std::span(&copy,1),flags,&f.error,nullptr,true);REQUIRE(third);
            CHECK(third->directBindingSlot()==original);CHECK(pages->stats().direct_slot_allocations==2);
            CHECK(pages->stats().direct_slot_hits==1);CHECK(pages->stats().direct_policy_bytes==4*96);
            CHECK(pages->stats().direct_view_bytes==3*(sizeof(ALMetalWorldSourceRegion)+sizeof(ALMetalWorldSourceGlobals)));
            for(const auto& source:f.descriptions->sources)CHECK_FALSE(source.native_resource_source);
        }
    }
}

TEST_CASE("almetal: direct cohort fixed-slot pressure protects uncommitted and GPU readers without waits",
          "[almetal][world-source-pages][world-cohort][world-direct-bindings]")
{
    @autoreleasepool
    {
        CohortPageFixture f;auto pages=f.pages();auto cohort=f.cohortView();
        std::vector<std::unique_ptr<ALMetalCommandBuffer>> commands;
        std::vector<std::shared_ptr<const ALMetalWorldSourceFrameView>> frames;
        for(unsigned index=0;index<128;++index)
        {
            auto command=f.device->makeCommandBuffer("Live fixed-address direct slot",&f.error);REQUIRE(command);
            auto frame=pages->freezeCohortsForCommand(*command,std::span(&cohort,1),f.globals,&f.error,nullptr,true);INFO(f.error);REQUIRE(frame);
            REQUIRE(command->retainUntilCompletion(frame,&f.error));commands.push_back(std::move(command));frames.push_back(std::move(frame));
        }
        const auto full=pages->stats();CHECK(full.live_direct_slots==128);CHECK(full.direct_policy_bytes==2*96);
        auto denied=f.device->makeCommandBuffer("Direct slot pressure has required fallback",&f.error);REQUIRE(denied);
        CHECK_FALSE(pages->freezeCohortsForCommand(*denied,std::span(&cohort,1),f.globals,&f.error,nullptr,true));
        CHECK(pages->stats().direct_slot_allocations==128);CHECK(pages->stats().live_direct_slot_bytes==full.live_direct_slot_bytes);
        auto storage=frames[0]->directBindingSlot();auto completion=commands[0]->completionToken();commands[0].reset();CHECK(completion->isComplete());
        CHECK_FALSE(pages->freezeCohortsForCommand(*denied,std::span(&cohort,1),f.globals,&f.error,nullptr,true)); // CPU reader remains.
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate{id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Retain a direct slot through actual GPU completion",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        REQUIRE(commands[1]->commit(&f.error));frames[1].reset();CHECK_FALSE(commands[1]->completionToken()->isComplete());
        frames[0].reset();auto recovered=pages->freezeCohortsForCommand(*denied,std::span(&cohort,1),f.globals,&f.error,nullptr,true);REQUIRE(recovered);
        CHECK(recovered->directBindingSlot()==storage);CHECK(pages->stats().direct_slot_allocations==128);
        CHECK(pages->stats().direct_policy_bytes==full.direct_policy_bytes);
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));
    }
}

TEST_CASE("almetal: cold direct UV policy is admitted separately from reused row tables and warm view writes",
          "[almetal][world-source-pages][world-cohort][world-direct-bindings][world-source-admission]")
{
    @autoreleasepool
    {
        CohortPageFixture f;for(auto& source:f.descriptions->sources)
            REQUIRE(ALMetalWorldResourceSource::publish(*f.device,source,f.resources->textures,&f.error));
        f.seal();auto pages=f.pages();auto cohort=f.cohortView();
        auto control_command=f.device->makeCommandBuffer("Old row-mode tables stay reusable",&f.error);REQUIRE(control_command);
        auto control=pages->freezeCohortForCommand(*control_command,cohort,f.globals,&f.error);REQUIRE(control);
        ALNativeFrameBudget budget;ALNativeFrameBudget::FrameInputs input;input.frame=1;budget.beginFrame(input);
        ALMetalWorldSourcePublicationWork observed;bool allow=false;unsigned callbacks=0;
        pages->setPublicationAdmission([&](const auto& work)->std::shared_ptr<const void>
        {++callbacks;observed=work;return allow?budget.tryAcquire(ALNativeFrameBudget::Work::scene_publication,work.chargedBytes(),1).credit:nullptr;});
        auto command=f.device->makeCommandBuffer("Deny cold immutable direct policy",&f.error);REQUIRE(command);
        const auto before=pages->stats();const auto allocations=alMetalResourceAllocationStats();
        CHECK_FALSE(pages->freezeCohortsForCommand(*command,std::span(&cohort,1),f.globals,&f.error,nullptr,true));
        CHECK(callbacks==1);CHECK(observed.encoded_row_bytes==2*96);CHECK(observed.chargedBytes()==2*96);
        CHECK(pages->stats().direct_policy_bytes==0);CHECK(pages->stats().direct_slot_allocations==0);
        CHECK(pages->stats().row_publications==before.row_publications);CHECK(alMetalResourceAllocationStats().buffers_created==allocations.buffers_created);
        allow=true;auto admitted=pages->freezeCohortsForCommand(*command,std::span(&cohort,1),f.globals,&f.error,nullptr,true);INFO(f.error);REQUIRE(admitted);
        CHECK(callbacks==2);CHECK(budget.snapshot().scene_bytes_in_flight==2*96);CHECK(pages->stats().direct_policy_bytes==2*96);
        auto token=command->completionToken();command.reset();CHECK(token->isComplete());CHECK(budget.snapshot().scene_bytes_in_flight==0);
        admitted.reset();auto warm_command=f.device->makeCommandBuffer("Warm fixed-address direct view",&f.error);REQUIRE(warm_command);
        auto warm=pages->freezeCohortsForCommand(*warm_command,std::span(&cohort,1),f.globals,&f.error,nullptr,true);REQUIRE(warm);
        CHECK(callbacks==2);CHECK(pages->stats().direct_slot_allocations==1);CHECK(pages->stats().row_publications==before.row_publications);
    }
}
