/**
 * @file almetal_world_cohort_test.mm
 * @brief Sealed ordinary cohorts, mixed ordered preparation and exact lifetimes.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldcohort.h"
#include "../almetalworldmaterialdata.h"
#include <cmath>

namespace
{
struct CohortCpu
{
    std::vector<ALMetalWorldSimpleBatch> sources;
    std::vector<ALMetalWorldKeyedSource> keys;
};
ALMetalWorldViewMatrices cohortMatrices(const ALMetalWorldSimpleBatch& source)
{
    ALMetalWorldViewMatrices view;
    std::memcpy(view.modelview_projection,source.modelview_projection,64);
    std::memcpy(view.modelview,source.modelview,64);
    std::memcpy(view.normal_matrix,source.normal_matrix,48);
    std::memcpy(view.rigged_normal_matrix,source.rigged_normal_matrix,48);
    return view;
}
struct CohortFixture : Fixture
{
    ALMetalWorldMaterialDataCache cache;
    std::shared_ptr<CohortCpu> cpu=std::make_shared<CohortCpu>();
    explicit CohortFixture(unsigned family=1,unsigned count=2,bool resource_descriptor=true):cache(*device)
    {
        cpu->sources.reserve(count);cpu->keys.reserve(count);
        for(unsigned i=0;i<count;++i)
        {
            auto source=batch(family);source.texcoord1_offset=source.texcoord2_offset=offsetof(Geometry,uv);
            source.texture_matrix[12]=.125f*i;
            if(family==2)source.double_sided=i%2!=0;
            if(family)
            {
                REQUIRE(ALMetalWorldMaterialSource::publish(*device,source,cache,&error));
                if(resource_descriptor)REQUIRE(ALMetalWorldResourceSource::publish(*device,source,resources->textures,&error));
                // The actual producer's complete native tuple includes uniforms,
                // not only the geometry/textures in the old component fixture.
                for(const auto& value:{source.native_material_source->materialLease(),
                    source.native_material_source->pbrLease(false),source.native_material_source->pbrLease(true)})
                    if(value && std::none_of(resources->buffers.begin(),resources->buffers.end(),
                        [&](const auto& prior){return prior==value;}))resources->buffers.push_back(value);
            }
            cpu->sources.push_back(std::move(source));
        }
        for(unsigned i=0;i<count;++i)cpu->keys.push_back({{i+1,family,1},&cpu->sources[i]});
    }
    std::shared_ptr<const ALMetalWorldSourceCohort> seal()
    {
        auto result=ALMetalWorldSourceCohort::seal(cpu->keys,cpu,resources,7,&error);
        INFO(error);REQUIRE(result);return result;
    }
    ALMetalWorldSourceCohortView view(const std::shared_ptr<const ALMetalWorldSourceCohort>& certificate)
    {
        ALMetalWorldSourceCohortView result;result.cohort=certificate;
        result.cpu_owner=cpu;result.native_owner=resources;result.view=cohortMatrices(cpu->sources.front());return result;
    }
};
std::vector<ALMetalWorldVisibilityQuery> cohortQueries(std::size_t count)
{
    std::vector<ALMetalWorldVisibilityQuery> result(count+1);
    for(std::size_t i=0;i<result.size();++i)
    {
        auto& query=result[i];identity(query.modelview_projection);
        query.center[0]=query.center[1]=-.5f;query.center[2]=.75f;
        query.half_extent[0]=query.half_extent[1]=query.half_extent[2]=.08f;
        query.batch_offset=i;query.fan=7;query.count_samples=1;
    }
    return result;
}
void applyCohortMatrices(ALMetalWorldSimpleBatch& source,const ALMetalWorldViewMatrices& view,bool fixed_uv=true)
{
    std::memcpy(source.modelview_projection,view.modelview_projection,64);
    std::memcpy(source.modelview,view.modelview,64);std::memcpy(source.normal_matrix,view.normal_matrix,48);
    std::memcpy(source.rigged_normal_matrix,view.rigged_normal_matrix,48);
    if(!fixed_uv)std::memcpy(source.texture_matrix,view.texture_matrix,64);
}
}

TEST_CASE("almetal: ordinary cohort seals private static values and exact weak producer identities",
          "[almetal][world-cohort]")
{
    @autoreleasepool
    {
        CohortFixture f(2);auto certificate=f.seal();
        CHECK(certificate->size()==2);CHECK(certificate->family()==2);CHECK(certificate->regionIdentity()==7);
        CHECK_FALSE(certificate->uniformDoubleSided());CHECK(certificate->ownedBytes()==certificate->metadataBytes());
        CHECK(certificate->ownedBytes()>2*sizeof(ALMetalWorldSimpleBatch));
        REQUIRE(certificate->matchesOwners(f.cpu,f.resources));
        CHECK_FALSE(certificate->matchesOwners(std::make_shared<CohortCpu>(),f.resources));
        CHECK_FALSE(certificate->matchesOwners(f.cpu,std::make_shared<Resources>()));
        // PPD-192: the first member can share the enclosing object's address.
        // This negative case needs a genuinely different stored address.
        std::shared_ptr<const void> wrong_alias(f.cpu,&f.cpu->keys);
        REQUIRE(wrong_alias.get()!=f.cpu.get());
        CHECK_FALSE(certificate->matchesOwners(wrong_alias,f.resources));
        const auto original_key=certificate->key(0);const auto original_count=certificate->source(0).index_count;
        const auto original_uv=certificate->source(0).texture_matrix[12];
        f.cpu->keys[0].key.identity=999;f.cpu->sources[0].index_count=0;
        f.cpu->sources[0].texture_matrix[12]=std::numeric_limits<float>::quiet_NaN();
        f.cpu->sources[0].pbrSlots()[0].roughness_factor=.99f;
        CHECK(certificate->key(0).identity==original_key.identity);
        CHECK(certificate->source(0).index_count==original_count);
        CHECK(certificate->source(0).texture_matrix[12]==original_uv);
        CHECK(certificate->source(0).native_material_source->matches(certificate->source(0)));
        CHECK(&certificate->source(0)!=&f.cpu->sources[0]);
        auto view=f.view(certificate);identity(view.view.modelview_projection);identity(view.view.modelview);
        view.view.texture_matrix[12]=std::numeric_limits<float>::quiet_NaN();
        CHECK(ALMetalWorldSourceCohort::validView(view.view)); // Fixed source UV overrides this unused array.
        ALMetalWorldDraw draw(certificate->source(0),&view.view,certificate->source(0).texture_matrix);
        CHECK(draw.textureMatrix()==certificate->source(0).texture_matrix);
        view.view.modelview[15]=0;CHECK_FALSE(ALMetalWorldSourceCohort::validView(view.view));
        std::weak_ptr<CohortCpu> weak=f.cpu;f.cpu.reset();view.cpu_owner.reset();wrong_alias.reset();
        CHECK(weak.expired());CHECK_FALSE(certificate->matchesOwners({},f.resources));
        std::weak_ptr<Resources> weak_native=f.resources;f.resources.reset();view.native_owner.reset();
        CHECK(weak_native.expired());CHECK_FALSE(certificate->matchesOwners({},{}));
    }
}

TEST_CASE("almetal: ordinary cohort rejects unsupported static rows before issuing a certificate",
          "[almetal][world-cohort]")
{
    @autoreleasepool
    {
        CohortFixture f;auto original=f.cpu->sources;
        for(unsigned failure=0;failure<7;++failure)
        {
            CAPTURE(failure);f.cpu->sources=original;
            for(std::size_t i=0;i<f.cpu->sources.size();++i)f.cpu->keys[i].prototype=&f.cpu->sources[i];
            auto& source=f.cpu->sources[0];
            if(failure==0)source.index_count=1;
            if(failure==1)source.texture_matrix[0]=std::numeric_limits<float>::quiet_NaN();
            if(failure==2)source.skin_palette=f.attachment_pose.data();
            if(failure==3)source.normal_texture=f.flat;
            if(failure==4)source.native_material_source.reset();
            if(failure==5)source.diffuse_textures[0]=f.orm; // A present stale resource descriptor still rejects.
            if(failure==6)source.vertices=nullptr;
            CHECK_FALSE(ALMetalWorldSourceCohort::seal(f.cpu->keys,f.cpu,f.resources,7,&f.error));
        }
        f.cpu->sources=original;
        for(std::size_t i=0;i<f.cpu->sources.size();++i)f.cpu->keys[i].prototype=&f.cpu->sources[i];
        CHECK_FALSE(ALMetalWorldSourceCohort::seal(f.cpu->keys,f.cpu,f.resources,0,&f.error));
        CHECK_FALSE(ALMetalWorldSourceCohort::seal(f.cpu->keys,{},f.resources,7,&f.error));
        CHECK_FALSE(ALMetalWorldSourceCohort::seal(f.cpu->keys,f.cpu,{},7,&f.error));
        // Old prototype cameras are not static source fields.
        for(auto& source:f.cpu->sources)
        {
            std::fill_n(source.modelview_projection,16,std::numeric_limits<float>::quiet_NaN());
            std::fill_n(source.modelview,16,std::numeric_limits<float>::quiet_NaN());
        }
        REQUIRE(f.seal());
    }
}

TEST_CASE("almetal: cached SIMPLE records contain no owning native generations and reject mixed families",
          "[almetal][world-cohort]")
{
    @autoreleasepool
    {
        CohortFixture simple(0),material(1),pbr(2);
        auto plain=ALMetalWorldCohortSource::seal(simple.cpu->keys[0],&simple.error);REQUIRE(plain);
        auto legacy=ALMetalWorldCohortSource::seal(material.cpu->keys[0],&material.error);REQUIRE(legacy);
        auto modern=ALMetalWorldCohortSource::seal(pbr.cpu->keys[0],&pbr.error);REQUIRE(modern);
        CHECK(plain->family()==0);CHECK_FALSE(plain->source().native_material_source);CHECK_FALSE(plain->source().native_resource_source);
        CHECK_FALSE(plain->source().impostor_texture_owner);CHECK_FALSE(plain->source().terrain_texture_owner);
        auto cohort=simple.seal();CHECK(cohort->family()==0);
        for(const auto& other:{legacy,modern})for(bool plain_first:{false,true})
        {
            const std::array records=plain_first?std::array{plain,other}:std::array{other,plain};
            CHECK_FALSE(ALMetalWorldSourceCohort::seal(records,simple.cpu,simple.resources,7,&simple.error));
        }
        for(unsigned bad=0;bad<11;++bad)
        {
            auto source=plain->source();CAPTURE(bad);
            if(bad==0)source.native_material_source=legacy->source().native_material_source;
            if(bad==1)source.native_resource_source=legacy->source().native_resource_source;
            if(bad==2)source.impostor_texture_owner=std::make_shared<int>(1);
            if(bad==3)source.terrain_texture_owner=std::make_shared<int>(1);
            if(bad==4)source.alpha_mask=true;
            if(bad==5)source.normal_texture=simple.flat;
            if(bad==6)source.water_haze=true;
            if(bad==7)source.avatar_alpha=true;
            if(bad==8)source.material_slot_count=1;
            if(bad==9)source.pbr_slot_count=1;
            if(bad==10)source.texture_matrix[0]=std::numeric_limits<float>::quiet_NaN();
            CHECK_FALSE(ALMetalWorldCohortSource::seal({plain->key(),&source},&simple.error));
        }
        // A strong idle CPU record is allowed only because all geometry and
        // diffuse bindings are non-owning. Keep the private record alive while
        // its exact native tuple and original CPU publication disappear.
        std::weak_ptr<const ALMetalBuffer> weak_vertices;
        std::weak_ptr<const ALMetalTexture> weak_diffuse;
        for(const auto& buffer:simple.resources->buffers)if(buffer.get()==simple.vertices)weak_vertices=buffer;
        for(const auto& texture:simple.resources->textures)if(texture.get()==simple.color)weak_diffuse=texture;
        REQUIRE_FALSE(weak_vertices.expired());REQUIRE_FALSE(weak_diffuse.expired());
        cohort.reset();simple.cpu.reset();simple.resources.reset();REQUIRE(simple.device->waitUntilIdle(&simple.error));
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while((!weak_vertices.expired() || !weak_diffuse.expired()) && std::chrono::steady_clock::now()<end)std::this_thread::yield();
        CHECK(weak_vertices.expired());CHECK(weak_diffuse.expired());
        CHECK(plain->family()==0);CHECK(plain->key().identity==1); // No native pointer dereference after expiry.
    }
}

TEST_CASE("almetal: partially changed cohorts reuse immutable source generations without prototype copies",
          "[almetal][world-cohort]")
{
    @autoreleasepool
    {
        CohortFixture f(1,3);
        std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> records;
        for(const auto& keyed:f.cpu->keys)
        {
            auto record=ALMetalWorldCohortSource::seal(keyed,&f.error);INFO(f.error);REQUIRE(record);
            CHECK(record->ownedBytes()>sizeof(ALMetalWorldSimpleBatch));records.push_back(std::move(record));
        }
        auto original=ALMetalWorldSourceCohort::seal(records,f.cpu,f.resources,7,&f.error);REQUIRE(original);
        auto group=f.view(original);auto scene=f.scene();scene.cohorts=&group;scene.cohort_count=1;
        auto old_input=ALMetalWorldViewInputs::adopt(scene,{}, {},{},std::vector{group},{},{},{},1,&f.error);REQUIRE(old_input);
        auto before=f.renderer->renderOwnedInputs(*old_input,&f.error);INFO(f.error);REQUIRE(before);
        const auto expected=f.read(*before);
        auto changed=records[0]->source();auto edited_geometry=f.geometry;
        for(auto& position:edited_geometry.positions)position[2]=.05f;
        auto native=std::make_shared<Resources>(*f.resources);
        ALMetalBufferDescriptor descriptor;descriptor.length=sizeof(edited_geometry);descriptor.storage=ALMetalStorageMode::shared;
        std::shared_ptr<ALMetalBuffer> vertices=ALMetalBuffer::create(*f.device,descriptor,&f.error);REQUIRE(vertices);
        REQUIRE(vertices->write(0,&edited_geometry,sizeof(edited_geometry),&f.error));
        native->buffers.push_back(vertices);changed.vertices=vertices.get();changed.texture_matrix[12]=.75f;
        changed.materialSlots()[0].env_intensity=.9f;
        REQUIRE(ALMetalWorldMaterialSource::publish(*f.device,changed,f.cache,&f.error));
        REQUIRE(ALMetalWorldResourceSource::publish(*f.device,changed,native->textures,&f.error));
        native->buffers.push_back(changed.native_material_source->materialLease());
        auto key=records[0]->key();++key.revision;
        auto replacement=ALMetalWorldCohortSource::seal({key,&changed},&f.error);REQUIRE(replacement);
        auto next_records=records;next_records[0]=replacement;
        auto cpu=std::make_shared<CohortCpu>();
        auto next=ALMetalWorldSourceCohort::seal(next_records,cpu,native,7,&f.error);REQUIRE(next);
        CHECK(&original->source(0)!=&next->source(0));CHECK(next->key(0).revision==2);
        for(unsigned unchanged=1;unchanged<3;++unchanged)
        {
            CHECK(&original->source(unchanged)==&next->source(unchanged));
            CHECK(&next->source(unchanged)==&records[unchanged]->source());
        }
        CHECK(next->source(0).vertices==vertices.get());CHECK(original->source(0).vertices==f.vertices);
        // Held caller aliases cannot mutate either sealed generation. The
        // generic API still validates the now malformed caller-owned input.
        f.cpu->sources[0].index_count=1;f.cpu->keys[0].key.identity=999;
        changed.index_count=1;changed.texture_matrix[12]=std::numeric_limits<float>::quiet_NaN();
        CHECK_FALSE(ALMetalWorldSourceCohort::seal(f.cpu->keys,f.cpu,f.resources,7,&f.error));
        auto reused=ALMetalWorldSourceCohort::seal(next_records,cpu,native,7,&f.error);REQUIRE(reused);
        CHECK(&reused->source(0)==&replacement->source());CHECK(reused->source(0).index_count==3);
        CHECK(reused->source(0).texture_matrix[12]==.75f);
        auto next_group=group;next_group.cohort=next;next_group.cpu_owner=cpu;next_group.native_owner=native;
        scene.cohorts=&next_group;
        auto new_input=ALMetalWorldViewInputs::adopt(scene,{}, {},{},std::vector{next_group},{},{},{},2,&f.error);REQUIRE(new_input);
        auto after=f.renderer->renderOwnedInputs(*new_input,&f.error);INFO(f.error);REQUIRE(after);
        CHECK_FALSE(f.read(*after)==expected);
        auto old_again=f.renderer->renderOwnedInputs(*old_input,&f.error);REQUIRE(old_again);CHECK(f.read(*old_again)==expected);
        auto missing=next_records;missing[1].reset();
        CHECK_FALSE(ALMetalWorldSourceCohort::seal(missing,cpu,native,7,&f.error));
        auto pbr=f.batch(2);REQUIRE(ALMetalWorldMaterialSource::publish(*f.device,pbr,f.cache,&f.error));
        REQUIRE(ALMetalWorldResourceSource::publish(*f.device,pbr,native->textures,&f.error));
        auto pbr_record=ALMetalWorldCohortSource::seal({{50,2,1},&pbr},&f.error);REQUIRE(pbr_record);
        missing=next_records;missing[1]=pbr_record;
        CHECK_FALSE(ALMetalWorldSourceCohort::seal(missing,cpu,native,7,&f.error));
    }
}

TEST_CASE("almetal: mixed raw reference cohort queries prepare and reclaim complete owned storage",
          "[almetal][world-cohort]")
{
    @autoreleasepool
    {
        for(unsigned family:{0u,1u,2u})for(bool normal_hdr:{false,true})
        {
            CAPTURE(family,normal_hdr);CohortFixture f(family,2,false);auto certificate=f.seal();
            for(const auto& source:f.cpu->sources)
            {
                CHECK_FALSE(source.native_resource_source);
                if(family)REQUIRE(source.native_material_source->matches(source));
            }
            std::vector<ALMetalWorldSimpleBatch> raw{f.batch(0),f.batch(4)};
            auto refs_owner=std::make_shared<CohortCpu>();refs_owner->sources.push_back(f.batch(0));
            ALMetalWorldSourceReference reference;reference.source=&refs_owner->sources[0];
            reference.view=cohortMatrices(refs_owner->sources[0]);reference.view.modelview_projection[12]=.1f;
            std::vector<ALMetalWorldSourceReference> refs{reference};
            std::vector<ALMetalWorldDrawIndex> order{0,alMetalWorldDrawReferenceBit,1};
            auto group=f.view(certificate);group.logical_begin=group.base_before=1;
            group.view.modelview_projection[12]=-.1f;group.view.texture_matrix[12]=.8f;
            std::vector<ALMetalWorldSourceCohortView> groups;groups.reserve(4);groups.push_back(group);
            auto queries=cohortQueries(5);auto scene=f.scene();scene.normal_hdr=normal_hdr;
            scene.batches=raw.data();scene.batch_count=raw.size();scene.references=refs.data();scene.reference_count=refs.size();
            scene.draw_order=order.data();scene.draw_order_count=order.size();scene.cohorts=groups.data();scene.cohort_count=groups.size();
            scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
            scene.cpu_source_owner=refs_owner;scene.source_owner=f.resources;
            REQUIRE(alMetalValidateWorldView(scene,&f.error));REQUIRE(scene.drawCount()==5);
            ALMetalWorldDrawCursor cursor(scene);
            CHECK(&cursor.draw().source()==&raw[0]);CHECK_FALSE(cursor.cohort());cursor.advance();
            CHECK(cursor.cohort()==&groups[0]);CHECK(&cursor.draw().source()==&certificate->source(0));cursor.advance();
            CHECK(&cursor.draw().source()==&certificate->source(1));cursor.advance();
            CHECK_FALSE(cursor.cohort());CHECK(&cursor.draw().source()==reference.source);cursor.advance();
            CHECK(&cursor.draw().source()==&raw[1]);
            ALMetalWorldDrawCursor skip(scene);skip.advance(3);CHECK(&skip.draw().source()==reference.source);
            CHECK(scene.draw(2).textureMatrix()==certificate->source(1).texture_matrix);
            CHECK(scene.draw(3).textureMatrix()==refs[0].view.texture_matrix);
            std::vector<ALMetalWorldSimpleBatch> baseline{raw[0],certificate->source(0),certificate->source(1),*reference.source,raw[1]};
            applyCohortMatrices(baseline[1],group.view);applyCohortMatrices(baseline[2],group.view);
            applyCohortMatrices(baseline[3],reference.view,false);
            auto ordinary=scene;ordinary.batches=baseline.data();ordinary.batch_count=baseline.size();ordinary.references=nullptr;ordinary.reference_count=0;
            ordinary.draw_order=nullptr;ordinary.draw_order_count=0;ordinary.cohorts=nullptr;ordinary.cohort_count=0;
            std::shared_ptr<ALMetalVisibilityResultBatch> expected_queries;
            auto expected_target=f.renderer->render(ordinary,&f.error,&expected_queries);INFO(f.error);REQUIRE(expected_target);REQUIRE(expected_queries);
            const auto expected=f.read(*expected_target);
            auto inputs=ALMetalWorldViewInputs::adopt(scene,raw,refs,order,std::move(groups),queries,refs_owner,f.resources,5,&f.error);
            INFO(f.error);REQUIRE(inputs);scene={};ordinary={};group={};groups.clear();

            auto ready=inputs;INFO(f.error);REQUIRE(ready);CHECK(ready->scene().drawCount()==5);
            std::shared_ptr<ALMetalVisibilityResultBatch> result;
            auto target=f.renderer->renderOwnedInputs(*ready,&f.error,&result);INFO(f.error);REQUIRE(target);REQUIRE(result);
            CHECK(f.read(*target)==expected);
            std::uint64_t visible=0;REQUIRE(result->wait(0,visible,&f.error));CHECK(visible>0);
            std::shared_ptr<ALMetalVisibilityResultBatch> scalar_queries;
            auto scalar=f.renderer->renderOwnedInputs(*inputs,&f.error,&scalar_queries);REQUIRE(scalar);REQUIRE(scalar_queries);
            CHECK(f.read(*scalar)==expected);
            for(std::size_t query=0;query<queries.size();++query)
            {
                std::uint64_t a=0,b=0,c=0;REQUIRE(result->wait(query,a,&f.error));REQUIRE(scalar_queries->wait(query,b,&f.error));
                REQUIRE(expected_queries->wait(query,c,&f.error));CHECK(a==b);CHECK(a==c);
            }
            std::vector<ALMetalWorldSimpleBatch> out_raw;std::vector<ALMetalWorldSourceReference> out_refs;
            std::vector<ALMetalWorldDrawIndex> out_order;std::vector<ALMetalWorldSourceCohortView> out_groups;
            std::vector<ALMetalWorldVisibilityQuery> out_queries;
            CHECK_FALSE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,out_raw,out_refs,out_order,out_groups,out_queries));
            ready.reset();
            CHECK_FALSE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,out_raw,out_queries));
            CHECK_FALSE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,out_raw,out_refs,out_order,out_queries));
            REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,out_raw,out_refs,out_order,out_groups,out_queries));
            CHECK_FALSE(inputs);CHECK(out_raw.empty());CHECK(out_refs.empty());CHECK(out_order.empty());CHECK(out_groups.empty());CHECK(out_queries.empty());
            CHECK(out_groups.capacity()>=4);CHECK(out_queries.capacity()>=queries.size());
        }
    }
}

TEST_CASE("almetal: cohort input ownership rejects malformed public storage",
          "[almetal][world-cohort]")
{
    @autoreleasepool
    {
        CohortFixture f;auto certificate=f.seal();auto group=f.view(certificate);auto scene=f.scene();
        scene.cohorts=&group;scene.cohort_count=1;scene.source_owner=f.resources;
        REQUIRE(alMetalValidateWorldView(scene,&f.error));
        CHECK_FALSE(ALMetalWorldViewInputs::adopt(scene,{}, {},f.resources,1,&f.error));
        CHECK_FALSE(ALMetalWorldViewInputs::adopt(scene,{}, {},{}, {},{},f.resources,1,&f.error));
        for(unsigned failure=0;failure<5;++failure)
        {
            auto bad=group;CAPTURE(failure);
            if(failure==0)bad.cpu_owner=std::make_shared<CohortCpu>();
            if(failure==1)bad.native_owner=std::make_shared<Resources>();
            if(failure==2)bad.logical_begin=1;
            if(failure==3)bad.base_before=1;
            if(failure==4)bad.view.modelview_projection[0]=std::numeric_limits<float>::quiet_NaN();
            scene.cohorts=&bad;CHECK_FALSE(alMetalValidateWorldView(scene,&f.error));
            CHECK_FALSE(f.renderer->render(scene,&f.error));
        }
        scene.cohorts=&group;
        auto input=ALMetalWorldViewInputs::adopt(scene,{}, {},{},std::vector{group},{},{},{},3,&f.error);INFO(f.error);REQUIRE(input);
        // Cohort-only adoption needs no unrelated aggregate CPU owner or
        // caller resource owner: each exact native tuple is pinned separately.
        REQUIRE(input->resourceLease());
        REQUIRE(f.renderer->renderOwnedInputs(*input,&f.error));
    }
}

TEST_CASE("almetal: cohort CPU descriptions expire while native-only command generations remain pending",
          "[almetal][world-cohort]")
{
    @autoreleasepool
    {
        for(unsigned family:{1u,2u})for(bool owned_input:{false,true})for(bool abandon:{false,true})
        {
            CAPTURE(family,owned_input,abandon);
            // Keep the legacy scalar descriptor-retirement gate while also
            // exercising descriptor-free Legacy/PBR pending native tuples.
            const bool resource_descriptor=family==1 && !owned_input;
            CohortFixture f(family,2,resource_descriptor);auto certificate=f.seal();
            CHECK(bool(certificate->source(0).native_resource_source)==resource_descriptor);
            std::weak_ptr<const ALMetalWorldCohortSource> weak_record;
            if(owned_input)
            {
                std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> records;
                for(const auto& source:f.cpu->keys)
                {auto record=ALMetalWorldCohortSource::seal(source,&f.error);REQUIRE(record);records.push_back(std::move(record));}
                weak_record=records.front();certificate=ALMetalWorldSourceCohort::seal(records,f.cpu,f.resources,7,&f.error);REQUIRE(certificate);
            }
            auto group=f.view(certificate);
            auto scene=f.scene();scene.cohorts=&group;scene.cohort_count=1;
            auto inputs=ALMetalWorldViewInputs::adopt(scene,{}, {},{},std::vector{group},{},{},{},7,&f.error);REQUIRE(inputs);scene={};
            auto warm=f.renderer->renderOwnedInputs(*inputs,&f.error);INFO(f.error);REQUIRE(warm);
            const auto expected=f.read(*warm);warm.reset();REQUIRE(f.device->waitUntilIdle(&f.error));
            id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
            struct OpenGate {id<MTLSharedEvent> value;~OpenGate(){value.signaledValue=1;}} open{gate};
            if(!abandon)
            {
                auto blocker=f.device->makeCommandBuffer("Hold cohort exact native tuple",&f.error);REQUIRE(blocker);
                [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
            }
            REQUIRE(f.renderer->beginCommandBatch("Cohort CPU retirement test",&f.error));
            std::unique_ptr<ALMetalRenderTarget> target;
            if(owned_input)
            {

                auto ready=inputs;REQUIRE(ready);target=f.renderer->renderOwnedInputs(*ready,&f.error);
            }
            else target=f.renderer->render(inputs->scene(),&f.error);
            INFO(f.error);REQUIRE(target);if(!abandon)REQUIRE(f.renderer->commitCommandBatch(&f.error));
            std::weak_ptr<CohortCpu> weak_cpu=f.cpu;std::weak_ptr<Resources> weak_native=f.resources;
            std::weak_ptr<const ALMetalWorldSourceCohort> weak_certificate=certificate;
            std::weak_ptr<const ALMetalWorldMaterialSource> weak_material=certificate->source(0).native_material_source;
            std::weak_ptr<const ALMetalWorldResourceSource> weak_resource=certificate->source(0).native_resource_source;
            std::vector<ALMetalWorldSimpleBatch> raw;std::vector<ALMetalWorldSourceReference> refs;
            std::vector<ALMetalWorldDrawIndex> order;std::vector<ALMetalWorldSourceCohortView> cohorts;
            std::vector<ALMetalWorldVisibilityQuery> queries;
            group={};certificate.reset();f.cpu.reset();f.resources.reset();
            REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,raw,refs,order,cohorts,queries));
            CHECK(weak_cpu.expired());CHECK(weak_certificate.expired());CHECK(weak_material.expired());CHECK(weak_resource.expired());
            if(owned_input)CHECK(weak_record.expired());
            CHECK_FALSE(weak_native.expired());
            if(abandon)f.renderer->cancelCommandBatch();
            else{gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(f.read(*target)==expected);}
            const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
            while(!weak_native.expired() && std::chrono::steady_clock::now()<end)std::this_thread::yield();
            CHECK(weak_native.expired());
        }
    }
}

#include "almetal_world_scalar_recipe_test.inl"
