/** Shared native visibility correctness and complete-path rejection fixtures. */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldvisibility.h"
#include "../almetalworldcohort.h"
#include "../almetalworldmaterialdata.h"
#include "../almetalresource.h"
#include <bit>

namespace
{
ALMetalWorldConservativeBounds triangleBounds(bool qualified=true)
{
    ALMetalWorldConservativeBounds b;b.minimum[0]=b.minimum[1]=-.8f;
    b.maximum[0]=b.maximum[1]=.8f;b.minimum[2]=b.maximum[2]=.25f;b.qualified=qualified;return b;
}
ALMetalWorldVisibilityMember member(ALMetalWorldBoundsHandle handle,const float* matrix,unsigned count=3)
{
    ALMetalWorldVisibilityMember value;std::memcpy(value.modelview_projection,matrix,16*sizeof(float));
    value.slot=handle.slot;value.identity=handle.identity;value.revision=handle.revision;
    value.index_count=count;value.enabled=1;return value;
}
std::shared_ptr<ALMetalWorldVisibility> visibility(Fixture& f)
{
    auto value=ALMetalWorldVisibility::create(*f.device,&f.error);INFO(f.error);REQUIRE(value);return value;
}
std::vector<ALMetalWorldVisibilityMember> worldMembers(ALMetalWorldBoundsHandle handle,
    const std::vector<ALMetalWorldSimpleBatch>& batches)
{
    std::vector<ALMetalWorldVisibilityMember> values;
    for(const auto& batch:batches)values.push_back(member(handle,batch.modelview_projection,batch.index_count));
    return values;
}
struct VisibilityCohortCpu {std::vector<ALMetalWorldSimpleBatch> sources;};
ALMetalWorldViewMatrices visibilityMatrices(const ALMetalWorldSimpleBatch& source)
{
    ALMetalWorldViewMatrices result;
    std::memcpy(result.modelview_projection,source.modelview_projection,64);
    std::memcpy(result.modelview,source.modelview,64);
    std::memcpy(result.normal_matrix,source.normal_matrix,48);
    std::memcpy(result.rigged_normal_matrix,source.rigged_normal_matrix,48);
    return result;
}
ALMetalWorldSimpleBatch withVisibilityMatrices(ALMetalWorldSimpleBatch source,const ALMetalWorldViewMatrices& view)
{
    std::memcpy(source.modelview_projection,view.modelview_projection,64);
    std::memcpy(source.modelview,view.modelview,64);
    std::memcpy(source.normal_matrix,view.normal_matrix,48);
    std::memcpy(source.rigged_normal_matrix,view.rigged_normal_matrix,48);
    return source; // Fixed source UV is deliberately independent of the view.
}
struct RetainedVisibilityFixture : Fixture
{
    ALMetalWorldMaterialDataCache material_cache;
    std::shared_ptr<VisibilityCohortCpu> cpu=std::make_shared<VisibilityCohortCpu>();
    std::shared_ptr<Resources> native;
    std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> records;
    std::shared_ptr<const ALMetalWorldSourceCohort> cohort;
    explicit RetainedVisibilityFixture(unsigned family):material_cache(*device)
    {
        const std::array<std::uint8_t,16> pattern{255,30,20,255,20,255,30,255,30,20,255,255,220,180,30,255};
        auto diffuse=texture(ALMetalPixelFormat::rgba8_unorm_srgb,2,pattern.data(),8);
        auto alternate=pattern;std::swap_ranges(alternate.begin(),alternate.begin()+8,alternate.begin()+8);
        auto indexed_diffuse=texture(ALMetalPixelFormat::rgba8_unorm_srgb,2,alternate.data(),8);
        cpu->sources.reserve(3);records.reserve(3);
        for(unsigned row=0;row<3;++row)
        {
            auto mesh=geometry;
            for(unsigned corner=0;corner<4;++corner)
            {
                mesh.positions[corner][0]+=row==1?4.f:0.f;
                mesh.positions[corner][3]=std::bit_cast<float>(family?1u:0u);
                mesh.uv[corner][0]=corner==1?.8f:.1f;mesh.uv[corner][1]=corner==2?.8f:.1f;
            }
            auto source=batch(family);source.vertices=buffer(&mesh,sizeof(mesh));
            source.texcoord1_offset=source.texcoord2_offset=offsetof(Geometry,uv);
            source.diffuse_textures[0]=diffuse;source.texture_matrix[12]=row*.125f;
            if(family)
            {
                source.texture_count=2;source.diffuse_textures[1]=indexed_diffuse;
                if(family==1)
                {source.material_slot_count=2;const auto slot=source.materialSlots()[0];source.materialSlots()[1]=slot;source.material_alpha_mode=2;}
                else
                {source.pbr_slot_count=2;const auto slot=source.pbrSlots()[0];source.pbrSlots()[1]=slot;source.pbrSlots()[1].roughness_factor=.7f;source.alpha_mask=true;}
                source.alpha_cutoff=.2f;
                REQUIRE(ALMetalWorldMaterialSource::publish(*device,source,material_cache,&error));
                for(const auto& buffer:{source.native_material_source->materialLease(),source.native_material_source->pbrLease(false),source.native_material_source->pbrLease(true)})
                    if(buffer)resources->buffers.push_back(buffer);
            }
            cpu->sources.push_back(std::move(source));
            auto bounds=triangleBounds();bounds.minimum[0]+=row==1?4.f:0.f;bounds.maximum[0]+=row==1?4.f:0.f;
            auto record=ALMetalWorldCohortSource::seal({{100+row,family,1},&cpu->sources.back()},&error,row==2?nullptr:&bounds);
            INFO(error);REQUIRE(record);records.push_back(std::move(record));
        }
        native=std::make_shared<Resources>(*resources);
        cohort=ALMetalWorldSourceCohort::seal(records,cpu,native,7,&error);INFO(error);REQUIRE(cohort);
        CHECK(records[0]->hasBoundsPublication());CHECK_FALSE(records[2]->hasBoundsPublication());
    }
    ALMetalWorldSourceCohortView view() const
    {
        ALMetalWorldSourceCohortView value;value.cohort=cohort;value.cpu_owner=cpu;value.native_owner=native;
        value.view=visibilityMatrices(cpu->sources.front());return value;
    }
};
void checkVisibilityCounts(const ALMetalWorldVisibilityArguments& arguments,std::span<const unsigned> counts)
{
    for(std::size_t index=0;index<counts.size();++index)
    {
        CAPTURE(index);ALMetalWorldIndexedArguments argument;REQUIRE(arguments.readCompletedArgumentForTesting(index,argument));
        CHECK(argument.index_count==counts[index]);CHECK(argument.instance_count==1);CHECK(argument.index_start==0);
        CHECK(argument.base_vertex==0);CHECK(argument.base_instance==0);
    }
}
std::vector<ALMetalWorldSourceCohortView> unequalVisibilityGroups(RetainedVisibilityFixture& f,unsigned count)
{
    std::vector<ALMetalWorldSourceCohortView> groups;groups.reserve(count);
    std::size_t logical=1;
    for(unsigned index=0;index<count;++index)
    {
        auto group=f.view();const auto rows=1+index%3;
        group.cohort=ALMetalWorldSourceCohort::seal(std::span(f.records).first(rows),f.cpu,f.native,7+index,&f.error);
        INFO(f.error);REQUIRE(group.cohort);
        group.logical_begin=logical;group.base_before=index+1;logical+=rows+1;
        groups.push_back(std::move(group));
    }
    return groups;
}
}

TEST_CASE("almetal: shared GPU visibility fails visible for unknown stale and unclippable bounds", "[almetal][world-visibility]")
{
    @autoreleasepool
    {
        Fixture f;auto v=visibility(f);float matrix[16];identity(matrix);
        auto known=v->publishBounds(1,1,triangleBounds());
        auto unknown=v->publishBounds(2,1,triangleBounds(false));
        auto stale=v->publishBounds(3,1,triangleBounds());v->publishBounds(3,2,triangleBounds());
        auto invalid_bounds=triangleBounds();invalid_bounds.minimum[0]=std::numeric_limits<float>::quiet_NaN();
        auto nonfinite=v->publishBounds(4,1,invalid_bounds);
        std::vector<ALMetalWorldVisibilityMember> values{
            member(known,matrix),member(known,matrix),member(unknown,matrix),member(stale,matrix),
            member({},matrix),member(nonfinite,matrix),member(known,matrix),member(known,matrix),member(known,matrix)};
        values[1].modelview_projection[12]=4;values[2].modelview_projection[12]=4;
        values[3].modelview_projection[12]=4;values[4].modelview_projection[12]=4;
        values[5].modelview_projection[12]=4;values[6].modelview_projection[15]=-1;
        values[7].modelview_projection[14]=4;values[7].depth_clamp=1;
        values[8].modelview_projection[14]=4;
        auto view=v->makeView(values,&f.error);REQUIRE(view);
        auto command=f.device->makeCommandBuffer("Shared visibility qualification",&f.error);REQUIRE(command);
        auto args=ALMetalWorldVisibility::encode(*command,*view,&f.error);INFO(f.error);REQUIRE(args);
        ALMetalWorldIndexedArguments argument;CHECK_FALSE(args->readCompletedArgumentForTesting(0,argument));
        REQUIRE(command->commitAndWait(&f.error));
        const std::array<unsigned,9> expected{3,0,3,3,3,3,3,3,0};
        for(unsigned index=0;index<expected.size();++index)
        {
            CAPTURE(index);REQUIRE(args->readCompletedArgumentForTesting(index,argument));
            CHECK(argument.index_count==expected[index]);CHECK(argument.instance_count==1);
            CHECK(argument.index_start==0);CHECK(argument.base_vertex==0);CHECK(argument.base_instance==0);
        }
        CHECK(args->appliesTo(0,3,matrix));CHECK_FALSE(args->appliesTo(0,6,matrix));
        float moved[16];std::memcpy(moved,matrix,sizeof(moved));moved[12]=.1f;
        CHECK_FALSE(args->appliesTo(0,3,moved));CHECK_FALSE(args->appliesTo(0,3,matrix,true));
        auto conflicting=triangleBounds();conflicting.minimum[0]=-.9f;
        CHECK(v->publishBounds(1,1,conflicting).identity==0);
        CHECK(v->stats().stale_publications==1);
    }
}

TEST_CASE("almetal: shared bounds updates retain old generations through late completion and abandonment", "[almetal][world-visibility]")
{
    @autoreleasepool
    {
        Fixture f;auto v=visibility(f);float matrix[16];identity(matrix);
        auto outside=triangleBounds();outside.minimum[0]=4;outside.maximum[0]=5;
        auto first_handle=v->publishBounds(10,1,outside);
        auto first_view=v->makeView({member(first_handle,matrix)},&f.error);REQUIRE(first_view);
        auto first=f.device->makeCommandBuffer("Late old visibility generation",&f.error);REQUIRE(first);
        auto a=ALMetalWorldVisibility::encode(*first,*first_view,&f.error);INFO(f.error);REQUIRE(a);
        auto new_handle=v->publishBounds(10,2,triangleBounds());CHECK(new_handle.slot==first_handle.slot);
        auto second_view=v->makeView({member(new_handle,matrix)},&f.error);REQUIRE(second_view);
        auto second=f.device->makeCommandBuffer("New visibility generation",&f.error);REQUIRE(second);
        auto b=ALMetalWorldVisibility::encode(*second,*second_view,&f.error);INFO(f.error);REQUIRE(b);
        CHECK(a->boundsGeneration()!=b->boundsGeneration());
        REQUIRE(second->commitAndWait(&f.error));REQUIRE(first->commitAndWait(&f.error));
        ALMetalWorldIndexedArguments argument;REQUIRE(a->readCompletedArgumentForTesting(0,argument));CHECK(argument.index_count==0);
        REQUIRE(b->readCompletedArgumentForTesting(0,argument));CHECK(argument.index_count==3);
        auto abandoned=f.device->makeCommandBuffer("Abandoned shared visibility",&f.error);REQUIRE(abandoned);
        auto abandoned_args=ALMetalWorldVisibility::encode(*abandoned,*second_view,&f.error);REQUIRE(abandoned_args);
        auto completion=abandoned->completionToken();abandoned.reset();CHECK(completion->isComplete());
        CHECK_FALSE(abandoned_args->readCompletedArgumentForTesting(0,argument));
        CHECK(v->retire(new_handle));CHECK_FALSE(v->retire(new_handle));
        auto replacement=v->publishBounds(11,1,outside);CHECK(replacement.slot==new_handle.slot);
        auto stale_view=v->makeView({member(new_handle,matrix)},&f.error);REQUIRE(stale_view);
        auto recovery=f.device->makeCommandBuffer("Retired handle fails visible",&f.error);REQUIRE(recovery);
        auto recovered=ALMetalWorldVisibility::encode(*recovery,*stale_view,&f.error);REQUIRE(recovered);
        REQUIRE(recovery->commitAndWait(&f.error));REQUIRE(recovered->readCompletedArgumentForTesting(0,argument));CHECK(argument.index_count==3);
        CHECK(v->stats().generation_reuses>0);
        // The encoded command owns bounds/member/argument resources even after
        // every CPU visibility owner and view has been destroyed.
        auto final=f.device->makeCommandBuffer("Destroyed visibility owner",&f.error);REQUIRE(final);
        auto final_args=ALMetalWorldVisibility::encode(*final,*stale_view,&f.error);REQUIRE(final_args);
        final_args.reset();stale_view.reset();first_view.reset();second_view.reset();a.reset();b.reset();
        recovered.reset();abandoned_args.reset();v.reset();
        REQUIRE(final->commitAndWait(&f.error));
    }
}

TEST_CASE("almetal: shared world visibility preserves small mixed draw order and query results", "[almetal][world-visibility]")
{
    @autoreleasepool
    {
        Fixture f;auto v=visibility(f);auto bounds=v->publishBounds(20,1,triangleBounds());
        for(unsigned mode=0;mode<4;++mode)
        {
            CAPTURE(mode);auto s=f.scene();s.reverse_z=mode&1;s.wireframe=mode==2;s.stamp_opaque_coverage=mode==3;
            s.join_adjacent_static_draws=true;
            std::vector<ALMetalWorldSimpleBatch> batches;
            for(unsigned family=0;family<10;++family)batches.push_back(f.batch(family));
            batches[0].modelview_projection[12]=4; // Actual offscreen static draw.
            std::vector<ALMetalWorldVisibilityQuery> queries(3);
            for(unsigned q=0;q<3;++q)
            {
                identity(queries[q].modelview_projection);queries[q].center[2]=.75f;
                queries[q].half_extent[0]=queries[q].half_extent[1]=queries[q].half_extent[2]=.05f;
                queries[q].batch_offset=q==0?0:q==1?4:batches.size();queries[q].fan=7;queries[q].count_samples=1;
            }
            s.batches=batches.data();s.batch_count=batches.size();s.visibility_queries=queries.data();s.visibility_query_count=queries.size();
            std::shared_ptr<ALMetalVisibilityResultBatch> direct_queries,native_queries;
            auto direct=f.renderer->render(s,&f.error,&direct_queries);INFO(f.error);REQUIRE(direct);REQUIRE(direct_queries);
            const auto expected=f.read(*direct);std::array<std::uint64_t,3> results{};
            for(unsigned q=0;q<3;++q)REQUIRE(direct_queries->wait(q,results[q],&f.error));
            auto values=worldMembers(bounds,batches);
            // Deliberately present an unsafe culling claim for rigged/body
            // families: the consumer must leave their exact native path live.
            auto outside=triangleBounds();outside.minimum[0]=4;outside.maximum[0]=5;
            auto unsafe=v->publishBounds(21,1,outside);
            for(unsigned index:{4u,5u,6u})values[index]=member(unsafe,batches[index].modelview_projection,batches[index].index_count);
            s.native_visibility=v->makeView(std::move(values),&f.error);REQUIRE(s.native_visibility);
            auto prepared=f.prepare(s,batches,queries);
            auto actual=f.renderer->renderPrepared(*prepared,&f.error,&native_queries);INFO(f.error);REQUIRE(actual);REQUIRE(native_queries);
            CHECK(f.read(*actual)==expected);
            for(unsigned q=0;q<3;++q){std::uint64_t value=0;REQUIRE(native_queries->wait(q,value,&f.error));CHECK(value==results[q]);}
            CHECK(results.front()>0);
        }
        CHECK(v->stats().views==4);CHECK(v->stats().argument_candidates>0);
    }
}

TEST_CASE("almetal: shared shadow arguments preserve scalar caster and query boundaries", "[almetal][world-visibility]")
{
    @autoreleasepool
    {
        Fixture f;auto v=visibility(f);auto bounds=v->publishBounds(30,1,triangleBounds());
        ALMetalTextureDescriptor descriptor;descriptor.type=ALMetalTextureType::texture_2d_array;
        descriptor.format=ALMetalPixelFormat::r32_float;descriptor.width=descriptor.height=side;descriptor.array_length=1;
        descriptor.storage=ALMetalStorageMode::private_gpu;descriptor.usage=ALMetalTextureUsage::shader_read|ALMetalTextureUsage::render_target;
        auto target=ALMetalTexture::create(*f.device,descriptor,&f.error);REQUIRE(target);
        std::vector<ALMetalWorldSpotShadowBatch> batches(3);
        for(unsigned index=0;index<3;++index)
        {
            auto& b=batches[index];b.vertices=f.vertices;b.indices=f.indices;b.vertex_count=4;b.index_count=3;
            identity(b.modelview_projection);
            if(index==0)b.modelview_projection[12]=4;
            if(index==1)
            {
                b.coverage=ALMetalWorldSpotShadowCoverage::texture_mask;b.texture_count=1;
                b.coverageData().textures[0]=f.color;b.coverageData().alpha_cutoffs[0]=.2f;
                b.texcoord_offset=offsetof(Fixture::Geometry,uv);b.color_offset=offsetof(Fixture::Geometry,colors);
            }
        }
        std::vector<ALMetalWorldVisibilityQuery> queries(2);
        for(unsigned q=0;q<2;++q)
        {
            identity(queries[q].modelview_projection);queries[q].center[2]=.75f;
            queries[q].half_extent[0]=queries[q].half_extent[1]=queries[q].half_extent[2]=.05f;
            queries[q].batch_offset=q?batches.size():0;queries[q].fan=7;queries[q].count_samples=1;
        }
        ALMetalWorldSpotShadowScene s;s.width=s.height=side;s.batches=batches.data();s.batch_count=batches.size();
        s.visibility_queries=queries.data();s.visibility_query_count=queries.size();s.source_owner=f.resources;s.bulk_draws=false;
        std::shared_ptr<ALMetalVisibilityResultBatch> direct_queries,native_queries;
        REQUIRE(f.renderer->renderSpotShadow(*target,0,s,&f.error,&direct_queries));INFO(f.error);REQUIRE(direct_queries);
        std::vector<float> expected(side*side),actual(side*side);
        REQUIRE(target->readback(*f.device,0,0,{},expected.data(),side*sizeof(float),&f.error));
        std::array<std::uint64_t,2> results{};for(unsigned q=0;q<2;++q)REQUIRE(direct_queries->wait(q,results[q],&f.error));
        std::vector<ALMetalWorldVisibilityMember> values;
        for(const auto& b:batches)values.push_back(member(bounds,b.modelview_projection,b.index_count));
        s.native_visibility=v->makeView(std::move(values),&f.error);REQUIRE(s.native_visibility);
        REQUIRE(f.renderer->renderSpotShadow(*target,0,s,&f.error,&native_queries));INFO(f.error);REQUIRE(native_queries);
        REQUIRE(target->readback(*f.device,0,0,{},actual.data(),side*sizeof(float),&f.error));CHECK(actual==expected);
        for(unsigned q=0;q<2;++q){std::uint64_t value=0;REQUIRE(native_queries->wait(q,value,&f.error));CHECK(value==results[q]);}
        CHECK(std::any_of(actual.begin(),actual.end(),[](float x){return x<1.f;}));
    }
}

TEST_CASE("almetal: complete shared visibility small mixed path cost matrix", "[almetal][.world-visibility-cost]")
{
    @autoreleasepool
    {
        Fixture f;std::uint64_t frame=1;auto v=visibility(f);auto handle=v->publishBounds(40,1,triangleBounds());
        for(unsigned count:{16u,128u,1024u})for(unsigned outside_percent:{0u,50u})
        {
            std::vector<ALMetalWorldSimpleBatch> batches;
            for(unsigned index=0;index<count;++index)
            {auto b=f.batch(index%4);if(index<count*outside_percent/100)b.modelview_projection[12]=4;batches.push_back(std::move(b));}
            auto s=f.scene();s.batches=batches.data();s.batch_count=batches.size();
            const auto run=[&](bool gpu)
            {
                std::array<double,12> ms{};
                for(unsigned sample=0;sample<14;++sample)
                {
                    const auto start=std::chrono::steady_clock::now();
                    f.data->beginFrame(++frame);
                    s.native_visibility=gpu?v->makeView(worldMembers(handle,batches),&f.error):nullptr;
                    auto prepared=f.prepare(s,batches,{},1);
                    auto target=f.renderer->renderPrepared(*prepared,&f.error);INFO(f.error);REQUIRE(target);
                    REQUIRE(f.device->waitUntilIdle(&f.error));
                    if(sample>=2)ms[sample-2]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                }
                std::sort(ms.begin(),ms.end());return std::array<double,2>{(ms[5]+ms[6])*.5,ms[11]};
            };
            const auto direct=run(false),gpu=run(true);
            std::cout << "world_visibility_complete_path count=" << count << " outside_percent=" << outside_percent
                << " direct_median_ms=" << direct[0] << " direct_max_ms=" << direct[1]
                << " gpu_median_ms=" << gpu[0] << " gpu_max_ms=" << gpu[1] << '\n';
        }
    }
}

TEST_CASE("almetal: retained visibility preserves mixed raw reference cohort and query order",
          "[almetal][world-visibility][world-cohort]")
{
    @autoreleasepool
    {
        for(unsigned family:{0u,1u,2u})
        {
            CAPTURE(family);RetainedVisibilityFixture f(family);auto v=visibility(f);
            auto raw_bounds=v->publishBounds(500,1,triangleBounds());auto group=f.view();group.logical_begin=group.base_before=1;
            // This unused group UV must never replace the fixed source UV.
            group.view.texture_matrix[12]=std::numeric_limits<float>::quiet_NaN();
            auto reference_cpu=std::make_shared<VisibilityCohortCpu>();reference_cpu->sources.push_back(f.batch(0));
            reference_cpu->sources[0].modelview_projection[12]=.15f;
            ALMetalWorldSourceReference reference;reference.source=&reference_cpu->sources[0];
            reference.view=visibilityMatrices(*reference.source);
            auto cpu_owner=std::make_shared<std::pair<std::shared_ptr<VisibilityCohortCpu>,std::shared_ptr<VisibilityCohortCpu>>>(f.cpu,reference_cpu);
            std::vector<ALMetalWorldSimpleBatch> raw{f.batch(0),f.batch(3)};
            raw[0].modelview_projection[0]=raw[0].modelview_projection[5]=.4f;
            raw[1].modelview_projection[0]=raw[1].modelview_projection[5]=.3f;raw[1].modelview_projection[12]=-.5f;
            const std::array<ALMetalWorldDrawIndex,3> order{0,alMetalWorldDrawReferenceBit,1};
            std::vector<ALMetalWorldSimpleBatch> baseline{raw[0]};
            for(const auto& source:f.cpu->sources)baseline.push_back(withVisibilityMatrices(source,group.view));
            baseline.push_back(withVisibilityMatrices(*reference.source,reference.view));baseline.push_back(raw[1]);
            std::vector<ALMetalWorldVisibilityQuery> queries(6);
            const std::array<std::size_t,6> offsets{0,1,2,4,5,6};
            for(unsigned index=0;index<queries.size();++index)
            {
                auto& query=queries[index];identity(query.modelview_projection);query.center[0]=.7f;query.center[1]=-.7f;query.center[2]=.75f;
                query.half_extent[0]=query.half_extent[1]=query.half_extent[2]=.05f;query.batch_offset=offsets[index];query.fan=7;query.count_samples=1;
            }
            for(unsigned format=0;format<8;++format)
            {
                CAPTURE(format);auto scene=f.scene();scene.hdr=format&1;scene.normal_hdr=format&2;scene.reverse_z=format&4;
                scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();scene.batches=baseline.data();scene.batch_count=baseline.size();
                std::shared_ptr<ALMetalVisibilityResultBatch> direct_queries,actual_queries;
                auto direct=f.renderer->render(scene,&f.error,&direct_queries);INFO(f.error);REQUIRE(direct);REQUIRE(direct_queries);
                const auto expected=f.read(*direct);std::array<std::uint64_t,6> expected_queries{};
                for(unsigned query=0;query<queries.size();++query)REQUIRE(direct_queries->wait(query,expected_queries[query],&f.error));
                scene.batches=raw.data();scene.batch_count=raw.size();scene.references=&reference;scene.reference_count=1;
                scene.draw_order=order.data();scene.draw_order_count=order.size();scene.cohorts=&group;scene.cohort_count=1;
                scene.cpu_source_owner=cpu_owner;scene.source_owner=f.native;
                std::vector<ALMetalWorldVisibilityMember> base{
                    member(raw_bounds,raw[0].modelview_projection),member(raw_bounds,reference.view.modelview_projection),member(raw_bounds,raw[1].modelview_projection)};
                const auto before=v->stats();scene.native_visibility=v->makeView(std::move(base),std::span(&group,1),baseline.size(),&f.error);
                INFO(f.error);REQUIRE(scene.native_visibility);CHECK(scene.native_visibility->size()==baseline.size());
                auto actual=f.renderer->render(scene,&f.error,&actual_queries);INFO(f.error);REQUIRE(actual);REQUIRE(actual_queries);
                CHECK(f.read(*actual)==expected);
                for(unsigned query=0;query<queries.size();++query)
                {std::uint64_t count=0;REQUIRE(actual_queries->wait(query,count,&f.error));CHECK(count==expected_queries[query]);}
                CHECK(expected_queries.front()>0);
                const auto stats=v->stats();CHECK(stats.cohort_views-before.cohort_views==1);CHECK(stats.cohort_groups-before.cohort_groups==1);
                CHECK(stats.expanded_member_bytes-before.expanded_member_bytes==3*sizeof(ALMetalWorldVisibilityMember));
                if(format)
                {CHECK(stats.cohort_row_hits-before.cohort_row_hits==1);CHECK(stats.cohort_row_builds==before.cohort_row_builds);
                 CHECK(stats.cohort_row_upload_bytes==before.cohort_row_upload_bytes);
                 CHECK(stats.cohort_row_allocated_bytes==before.cohort_row_allocated_bytes);}
                else
                {
                    CHECK(stats.cohort_row_builds-before.cohort_row_builds==1);CHECK(stats.cohort_row_upload_bytes-before.cohort_row_upload_bytes==3*48);
                    CHECK(stats.cohort_row_allocated_bytes-before.cohort_row_allocated_bytes>=3*48);
                    CHECK(stats.live_cohort_row_bytes==stats.cohort_row_allocated_bytes-before.cohort_row_allocated_bytes);
                    std::cout<<"retained_visibility_native_rows family="<<family<<" logical_upload_bytes="<<3*48
                        <<" actual_native_allocated_bytes="<<stats.live_cohort_row_bytes<<'\n';
                }
                CHECK(stats.live_cohort_row_bytes<=8*1024*1024);
            }
        }
    }
}

TEST_CASE("almetal: retained visibility maps fail-visible rows and exact revised regional matrices",
          "[almetal][world-visibility][world-cohort]")
{
    @autoreleasepool
    {
        RetainedVisibilityFixture f(1);auto v=visibility(f);auto group=f.view();group.logical_begin=group.base_before=1;
        float matrix[16];identity(matrix);auto known=v->publishBounds(600,1,triangleBounds());
        auto old=v->publishBounds(601,1,triangleBounds());v->publishBounds(601,2,triangleBounds());
        auto stale=member(old,matrix);stale.modelview_projection[12]=4.f;
        const std::vector<ALMetalWorldVisibilityMember> base{member(known,matrix),stale};
        auto first=v->makeView(base,std::span(&group,1),5,&f.error);INFO(f.error);REQUIRE(first);
        auto command=f.device->makeCommandBuffer("Retained logical argument overlay",&f.error);REQUIRE(command);
        auto original=ALMetalWorldVisibility::encode(*command,*first,&f.error);INFO(f.error);REQUIRE(original);
        REQUIRE(command->commitAndWait(&f.error));checkVisibilityCounts(*original,std::array<unsigned,5>{3,3,0,3,3});
        CHECK_FALSE(original->appliesTo(1,3,group.view.modelview_projection)); // Generic qualification is raw-only.
        CHECK_FALSE(original->appliesTo(3,3,group.view.modelview_projection));
        CHECK_FALSE(original->appliesTo(1,6,group.view.modelview_projection));CHECK_FALSE(original->appliesTo(1,3,group.view.modelview_projection,true));
        CHECK(original->appliesToCohort(1,3,group));CHECK(original->appliesToCohort(2,3,group));
        CHECK_FALSE(original->appliesToCohort(0,3,group));CHECK_FALSE(original->appliesToCohort(4,3,group));
        CHECK_FALSE(original->appliesToCohort(3,3,group));CHECK_FALSE(original->appliesToCohort(1,6,group));
        auto other=group;other.cohort=ALMetalWorldSourceCohort::seal(f.records,f.cpu,f.native,7,&f.error);REQUIRE(other.cohort);
        REQUIRE(other.cohort!=group.cohort);CHECK_FALSE(original->appliesToCohort(1,3,other)); // Equal values are not a certificate identity.
        for(unsigned mismatch=0;mismatch<7;++mismatch)
        {
            CAPTURE(mismatch);auto current=group;
            if(mismatch==0)current.cpu_owner=std::make_shared<VisibilityCohortCpu>(*f.cpu);
            if(mismatch==1)current.native_owner=std::make_shared<Resources>(*f.native);
            // These deliberately nonowning impostors have the same stored
            // address but a different control block; real owners stay alive.
            if(mismatch==2)current.cpu_owner=std::shared_ptr<const void>(f.cpu.get(),[](const void*){});
            if(mismatch==3)current.native_owner=std::shared_ptr<const void>(f.native.get(),[](const void*){});
            if(mismatch==4)current.logical_begin+=1;if(mismatch==5)current.base_before+=1;
            if(mismatch==6)current.view.modelview_projection[12]=.125f;
            CHECK_FALSE(original->appliesToCohort(1,3,current));
        }
        // Same immutable cohort, new per-view region MVP: only the copied
        // 80-byte group dispatch state changes; persistent rows stay warm.
        auto moved=group;moved.view.modelview_projection[12]=4.f;const auto before=v->stats();
        auto next=v->makeView(base,std::span(&moved,1),5,&f.error);REQUIRE(next);
        auto moved_command=f.device->makeCommandBuffer("Moved retained regional matrix",&f.error);REQUIRE(moved_command);
        auto moved_args=ALMetalWorldVisibility::encode(*moved_command,*next,&f.error);REQUIRE(moved_args);
        REQUIRE(moved_command->commitAndWait(&f.error));checkVisibilityCounts(*moved_args,std::array<unsigned,5>{3,0,0,3,3});
        CHECK_FALSE(original->appliesTo(1,3,moved.view.modelview_projection));
        CHECK(v->stats().cohort_row_hits-before.cohort_row_hits==1);CHECK(v->stats().cohort_row_upload_bytes==before.cohort_row_upload_bytes);
        // A new immutable bounds revision and region identity can qualify the
        // formerly unknown row. All three actual offscreen rows now cull.
        auto revised_records=f.records;auto bounds=triangleBounds();
        revised_records[2]=ALMetalWorldCohortSource::seal({{102,1,2},&f.cpu->sources[2]},&f.error,&bounds);REQUIRE(revised_records[2]);
        auto revised=ALMetalWorldSourceCohort::seal(revised_records,f.cpu,f.native,8,&f.error);REQUIRE(revised);CHECK(revised->regionIdentity()==8);
        auto revised_group=moved;revised_group.cohort=revised;
        auto revised_view=v->makeView(base,std::span(&revised_group,1),5,&f.error);REQUIRE(revised_view);
        auto revised_command=f.device->makeCommandBuffer("Revised retained immutable bounds",&f.error);REQUIRE(revised_command);
        auto revised_args=ALMetalWorldVisibility::encode(*revised_command,*revised_view,&f.error);REQUIRE(revised_args);
        REQUIRE(revised_command->commitAndWait(&f.error));checkVisibilityCounts(*revised_args,std::array<unsigned,5>{3,0,0,0,3});
        CHECK(revised_args->appliesToCohort(1,3,revised_group));
        auto same_matrix_revision=revised_group;same_matrix_revision.view=group.view;
        CHECK_FALSE(original->appliesToCohort(1,3,same_matrix_revision));
        checkVisibilityCounts(*original,std::array<unsigned,5>{3,3,0,3,3}); // Prior rows were not rewritten.
        CHECK(v->stats().cohort_row_builds==2);
        auto nonfinite=triangleBounds();nonfinite.minimum[0]=std::numeric_limits<float>::quiet_NaN();
        revised_records[2]=ALMetalWorldCohortSource::seal({{102,1,3},&f.cpu->sources[2]},&f.error,&nonfinite);REQUIRE(revised_records[2]);
        revised_group.cohort=ALMetalWorldSourceCohort::seal(revised_records,f.cpu,f.native,8,&f.error);REQUIRE(revised_group.cohort);
        auto bad_bounds_view=v->makeView(base,std::span(&revised_group,1),5,&f.error);REQUIRE(bad_bounds_view);
        auto bad_bounds_command=f.device->makeCommandBuffer("Unproved finite extent fails visible",&f.error);REQUIRE(bad_bounds_command);
        auto bad_bounds_args=ALMetalWorldVisibility::encode(*bad_bounds_command,*bad_bounds_view,&f.error);REQUIRE(bad_bounds_args);
        REQUIRE(bad_bounds_command->commitAndWait(&f.error));checkVisibilityCounts(*bad_bounds_args,std::array<unsigned,5>{3,0,0,3,3});
        for(unsigned failure=0;failure<7;++failure)
        {
            CAPTURE(failure);auto invalid=group;std::size_t count=5;
            if(failure==0)invalid.cpu_owner.reset();if(failure==1)invalid.native_owner=std::make_shared<Resources>();
            if(failure==2)invalid.logical_begin=2;if(failure==3)invalid.base_before=3;if(failure==4)count=6;
            if(failure==5)invalid.view.modelview_projection[0]=std::numeric_limits<float>::quiet_NaN();
            if(failure==6)invalid.view.modelview[15]=0;
            CHECK_FALSE(v->makeView(base,std::span(&invalid,1),count,&f.error));
        }
        // PPD220: an old qualified offscreen member must not suppress a new
        // visible generation merely because count and MVP still match.
        auto old_group=f.view();auto old_view=v->makeView({},std::span(&old_group,1),3,&f.error);REQUIRE(old_view);
        auto changed_cpu=std::make_shared<VisibilityCohortCpu>(*f.cpu);auto changed_mesh=f.geometry;
        for(auto& position:changed_mesh.positions){position[2]=-.5f;position[3]=std::bit_cast<float>(1u);}
        changed_cpu->sources[1].vertices=f.buffer(&changed_mesh,sizeof(changed_mesh));
        auto changed_native=std::make_shared<Resources>(*f.resources);auto changed_records=f.records;
        auto changed_bounds=triangleBounds();changed_bounds.minimum[2]=changed_bounds.maximum[2]=-.5f;
        changed_records[1]=ALMetalWorldCohortSource::seal({{101,1,2},&changed_cpu->sources[1]},&f.error,&changed_bounds);REQUIRE(changed_records[1]);
        auto changed_group=old_group;changed_group.cpu_owner=changed_cpu;changed_group.native_owner=changed_native;
        changed_group.cohort=ALMetalWorldSourceCohort::seal(changed_records,changed_cpu,changed_native,7,&f.error);REQUIRE(changed_group.cohort);
        auto scene=f.scene();scene.cohorts=&old_group;scene.cohort_count=1;scene.source_owner=f.native;
        auto old_target=f.renderer->render(scene,&f.error);INFO(f.error);REQUIRE(old_target);const auto old_pixels=f.read(*old_target);
        scene.cohorts=&changed_group;scene.source_owner=changed_native;
        auto direct=f.renderer->render(scene,&f.error);INFO(f.error);REQUIRE(direct);const auto expected=f.read(*direct);CHECK_FALSE(expected==old_pixels);
        scene.native_visibility=old_view;
        auto actual=f.renderer->render(scene,&f.error);INFO(f.error);REQUIRE(actual);CHECK(f.read(*actual)==expected);
    }
}

TEST_CASE("almetal: retained visibility commands release CPU cohorts and preserve pending native generations",
          "[almetal][world-visibility][world-cohort]")
{
    @autoreleasepool
    {
        RetainedVisibilityFixture f(2);auto v=visibility(f);auto group=f.view();auto scene=f.scene();
        scene.cohorts=&group;scene.cohort_count=1;scene.source_owner=f.native;
        auto reference=f.renderer->render(scene,&f.error);INFO(f.error);REQUIRE(reference);const auto expected=f.read(*reference);
        auto view=v->makeView({},std::span(&group,1),3,&f.error);INFO(f.error);REQUIRE(view);
        auto abandoned=f.device->makeCommandBuffer("Abandon retained native row arguments",&f.error);REQUIRE(abandoned);
        auto abandoned_args=ALMetalWorldVisibility::encode(*abandoned,*view,&f.error);REQUIRE(abandoned_args);
        auto abandoned_completion=abandoned->completionToken();abandoned.reset();CHECK(abandoned_completion->isComplete());
        ALMetalWorldIndexedArguments argument;CHECK_FALSE(abandoned_args->readCompletedArgumentForTesting(0,argument));abandoned_args.reset();
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate{id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold retained visibility render",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        REQUIRE(f.renderer->beginCommandBatch("Pending retained visibility and GBuffer",&f.error));scene.native_visibility=view;
        auto target=f.renderer->render(scene,&f.error);INFO(f.error);REQUIRE(target);
        auto completion=f.renderer->commandBatchCompletion();REQUIRE(completion);REQUIRE(f.renderer->commitCommandBatch(&f.error));CHECK_FALSE(completion->isComplete());
        const std::weak_ptr<VisibilityCohortCpu> weak_cpu=f.cpu;const std::weak_ptr<const ALMetalWorldSourceCohort> weak_cohort=f.cohort;
        const std::weak_ptr<const ALMetalWorldMaterialSource> weak_material=f.cpu->sources[0].native_material_source;
        const std::weak_ptr<Resources> weak_native=f.native;
        scene={};group={};view.reset();f.cohort.reset();f.records.clear();f.cpu.reset();f.native.reset();v.reset();
        CHECK(weak_cpu.expired());CHECK(weak_cohort.expired());CHECK(weak_material.expired());CHECK_FALSE(weak_native.expired());
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(completion->isComplete());CHECK(f.read(*target)==expected);
        const auto retirement=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!weak_native.expired()&&std::chrono::steady_clock::now()<retirement)std::this_thread::yield();
        CHECK(weak_native.expired());
    }
}

TEST_CASE("almetal: retained visibility publication shares real frame admission and required direct fallback",
          "[almetal][world-visibility][world-cohort][world-source-admission]")
{
    @autoreleasepool
    {
        RetainedVisibilityFixture f(1);auto v=visibility(f);auto group=f.view();auto scene=f.scene();
        std::array<ALMetalWorldVisibilityQuery,2> queries;
        for(unsigned query=0;query<queries.size();++query)
        {
            auto& q=queries[query];identity(q.modelview_projection);q.center[0]=.7f;q.center[1]=-.7f;q.center[2]=.75f;
            q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.05f;q.batch_offset=query?3:0;q.fan=7;q.count_samples=1;
        }
        scene.cohorts=&group;scene.cohort_count=1;scene.source_owner=f.native;
        scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
        std::shared_ptr<ALMetalVisibilityResultBatch> reference_queries,actual_queries;
        auto reference=f.renderer->render(scene,&f.error,&reference_queries);INFO(f.error);REQUIRE(reference);REQUIRE(reference_queries);
        const auto expected=f.read(*reference);std::array<std::uint64_t,2> expected_queries{};
        for(unsigned query=0;query<queries.size();++query)REQUIRE(reference_queries->wait(query,expected_queries[query],&f.error));
        REQUIRE(expected_queries.front()>0);
        auto view=v->makeView({},std::span(&group,1),3,&f.error);REQUIRE(view);
        ALNativeFrameBudget budget;ALNativeFrameBudget::FrameInputs frame;frame.frame=1;budget.beginFrame(frame);
        std::vector<ALNativeFrameBudget::Lease> occupied;
        for(unsigned index=0;index<16;++index)
        {auto grant=budget.tryAcquire(ALNativeFrameBudget::Work::scene_publication,1,1);REQUIRE(grant);occupied.push_back(grant.credit);}
        std::vector<ALNativeFrameBudget::Frame> observed_ages;
        f.data->setSourcePublicationAdmission(
            [&budget,&observed_ages,pending_since=std::optional<ALNativeFrameBudget::Frame>{}](const ALMetalWorldSourcePublicationWork& work)mutable->std::shared_ptr<const void>
            {
                if(!pending_since)pending_since=budget.snapshot().frame;observed_ages.push_back(*pending_since);
                auto grant=budget.tryAcquire(ALNativeFrameBudget::Work::scene_publication,work.chargedBytes(),*pending_since);
                if(grant)pending_since.reset();return grant.credit;
            });
        auto denied=f.device->makeCommandBuffer("Real frame quota denies retained publication",&f.error);REQUIRE(denied);
        const auto before=v->stats();const auto allocations=alMetalResourceAllocationStats();
        CHECK_FALSE(ALMetalWorldVisibility::encode(*denied,*view,&f.error,f.data.get()));
        CHECK(v->stats().last_fallback==ALMetalWorldVisibilityFailure::admission);
        CHECK(v->stats().cohort_row_builds==before.cohort_row_builds);CHECK(v->stats().cohort_row_upload_bytes==before.cohort_row_upload_bytes);
        CHECK(v->stats().cohort_row_allocated_bytes==before.cohort_row_allocated_bytes);
        CHECK(alMetalResourceAllocationStats().buffers_created==allocations.buffers_created);
        scene.native_visibility=view;auto direct_fallback=f.renderer->render(scene,&f.error,&actual_queries);
        INFO(f.error);REQUIRE(direct_fallback);REQUIRE(actual_queries);CHECK(f.read(*direct_fallback)==expected);
        for(unsigned query=0;query<queries.size();++query)
        {std::uint64_t count=0;REQUIRE(actual_queries->wait(query,count,&f.error));CHECK(count==expected_queries[query]);}
        CHECK(v->stats().admission_denials-before.admission_denials==2);CHECK(v->stats().cohort_row_builds==before.cohort_row_builds);
        occupied.clear();CHECK(budget.snapshot().scene_publications_in_flight==0);
        frame.frame=2;budget.beginFrame(frame);f.data->beginFrame(2);
        auto admitted=f.device->makeCommandBuffer("Admit deferred retained publication",&f.error);REQUIRE(admitted);
        auto arguments=ALMetalWorldVisibility::encode(*admitted,*view,&f.error,f.data.get());INFO(f.error);REQUIRE(arguments);
        REQUIRE(observed_ages.size()==3);CHECK(observed_ages.back()==1); // The callback's private pending age survived both denials.
        CHECK(v->stats().admitted_logical_bytes-before.admitted_logical_bytes==3*48);
        CHECK(budget.snapshot().scene_publications_in_flight==1);CHECK(budget.snapshot().scene_bytes_in_flight==3*48);
        const auto abandoned_completion=admitted->completionToken();admitted.reset();CHECK(abandoned_completion->isComplete());
        CHECK(budget.snapshot().scene_publications_in_flight==0);CHECK(budget.snapshot().scene_bytes_in_flight==0);
        ALMetalWorldIndexedArguments argument;CHECK_FALSE(arguments->readCompletedArgumentForTesting(0,argument));arguments.reset();
        const auto warm=v->stats();const auto callbacks=observed_ages.size();
        for(unsigned views:{1u,3u})for(unsigned camera=0;camera<views;++camera)
        {
            auto copied=group;copied.view.modelview_projection[13]=camera*.01f;
            auto warm_view=v->makeView({},std::span(&copied,1),3,&f.error);REQUIRE(warm_view);
            auto command=f.device->makeCommandBuffer("Required warm view needs no publication ticket",&f.error);REQUIRE(command);
            auto current=ALMetalWorldVisibility::encode(*command,*warm_view,&f.error,f.data.get());REQUIRE(current);
            CHECK(budget.snapshot().scene_publications_in_flight==0);REQUIRE(command->commitAndWait(&f.error));
            checkVisibilityCounts(*current,std::array<unsigned,3>{3,0,3});
        }
        CHECK(observed_ages.size()==callbacks);CHECK(v->stats().admission_requests==warm.admission_requests);
        CHECK(v->stats().cohort_row_upload_bytes==warm.cohort_row_upload_bytes);CHECK(v->stats().cohort_row_allocated_bytes==warm.cohort_row_allocated_bytes);
        // A genuinely new private generation owns one new ticket until its
        // actual native completion, independently of cached warm views.
        frame.frame=3;budget.beginFrame(frame);auto fresh=group;
        fresh.cohort=ALMetalWorldSourceCohort::seal(f.records,f.cpu,f.native,7,&f.error);REQUIRE(fresh.cohort);
        auto fresh_view=v->makeView({},std::span(&fresh,1),3,&f.error);REQUIRE(fresh_view);
        auto completed=f.device->makeCommandBuffer("Completion retires retained publication credit",&f.error);REQUIRE(completed);
        auto completed_args=ALMetalWorldVisibility::encode(*completed,*fresh_view,&f.error,f.data.get());REQUIRE(completed_args);
        CHECK(budget.snapshot().scene_publications_in_flight==1);REQUIRE(completed->commitAndWait(&f.error));
        checkVisibilityCounts(*completed_args,std::array<unsigned,3>{3,0,3});
        const auto retirement=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(budget.snapshot().scene_publications_in_flight&&std::chrono::steady_clock::now()<retirement)std::this_thread::yield();
        CHECK(budget.snapshot().scene_publications_in_flight==0);CHECK(budget.snapshot().scene_bytes_in_flight==0);
        // An admission callback may reenter the publisher. The captured plan
        // must reject before native allocation and release its unused grant.
        frame.frame=4;budget.beginFrame(frame);auto reentrant_group=group;
        reentrant_group.cohort=ALMetalWorldSourceCohort::seal(f.records,f.cpu,f.native,7,&f.error);REQUIRE(reentrant_group.cohort);
        auto reentrant_view=v->makeView({},std::span(&reentrant_group,1),3,&f.error);REQUIRE(reentrant_view);
        f.data->setSourcePublicationAdmission([&](const auto& work)->std::shared_ptr<const void>
        {v->publishBounds(999,1,triangleBounds());return budget.tryAcquire(ALNativeFrameBudget::Work::scene_publication,work.chargedBytes(),4).credit;});
        auto stale=f.device->makeCommandBuffer("Reentrant bounds publication retires captured plan",&f.error);REQUIRE(stale);
        const auto stable_builds=v->stats().cohort_row_builds,stable_upload=v->stats().cohort_row_upload_bytes;
        const auto stable_allocations=alMetalResourceAllocationStats();
        CHECK_FALSE(ALMetalWorldVisibility::encode(*stale,*reentrant_view,&f.error,f.data.get()));
        CHECK(v->stats().last_fallback==ALMetalWorldVisibilityFailure::stale_plan);
        CHECK(v->stats().cohort_row_builds==stable_builds);CHECK(v->stats().cohort_row_upload_bytes==stable_upload);
        CHECK(alMetalResourceAllocationStats().buffers_created==stable_allocations.buffers_created);
        CHECK(budget.snapshot().scene_publications_in_flight==0);CHECK(budget.snapshot().scene_bytes_in_flight==0);
        f.data->setSourcePublicationAdmission({});
    }
}

TEST_CASE("almetal: sparse visibility leaves raw holes direct without placeholder membership",
          "[almetal][world-visibility][world-cohort]")
{
    @autoreleasepool
    {
        RetainedVisibilityFixture f(1);auto v=visibility(f);auto group=f.view();group.logical_begin=group.base_before=2;
        float matrix[16];identity(matrix);auto handle=v->publishBounds(700,1,triangleBounds());
        auto disabled=member(handle,matrix);disabled.enabled=0;auto outside=member(handle,matrix);outside.modelview_projection[12]=4.f;
        const auto before=v->stats();
        auto view=v->makeView({disabled,member(handle,matrix),outside},{0,1,6},std::span(&group,1),8,&f.error);INFO(f.error);REQUIRE(view);
        auto command=f.device->makeCommandBuffer("Sparse qualified membership and direct holes",&f.error);REQUIRE(command);
        auto args=ALMetalWorldVisibility::encode(*command,*view,&f.error);INFO(f.error);REQUIRE(args);REQUIRE(command->commitAndWait(&f.error));
        CHECK(v->stats().expanded_member_bytes-before.expanded_member_bytes==2*sizeof(ALMetalWorldVisibilityMember));
        CHECK(v->stats().cohort_candidates-before.cohort_candidates==2);
        for(unsigned index:{0u,5u,7u})
        {
            CAPTURE(index);ALMetalWorldIndexedArguments argument;
            CHECK_FALSE(args->appliesTo(index,3,matrix));CHECK_FALSE(args->readCompletedArgumentForTesting(index,argument));
        }
        const std::array<unsigned,5> indices{1,2,3,4,6},counts{3,3,0,3,0};
        for(unsigned row=0;row<indices.size();++row)
        {ALMetalWorldIndexedArguments argument;REQUIRE(args->readCompletedArgumentForTesting(indices[row],argument));CHECK(argument.index_count==counts[row]);}
        CHECK(args->appliesTo(1,3,matrix));CHECK_FALSE(args->appliesTo(2,3,matrix));
        CHECK(args->appliesToCohort(2,3,group));CHECK_FALSE(args->appliesToCohort(4,3,group));
        for(const auto& invalid:std::array<std::vector<std::uint32_t>,3>{{{1,1},{1,8},{1,3}}})
            CHECK_FALSE(v->makeView({member(handle,matrix),member(handle,matrix)},invalid,std::span(&group,1),8,&f.error));
        f.error="previous optional error";
        CHECK_FALSE(v->makeView({disabled},{0},{},8,&f.error));CHECK(f.error.empty());
        std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> unknown;
        for(unsigned row=0;row<3;++row)
        {auto record=ALMetalWorldCohortSource::seal({{200+row,1,1},&f.cpu->sources[row]},&f.error);REQUIRE(record);unknown.push_back(std::move(record));}
        auto unqualified=f.view();unqualified.cohort=ALMetalWorldSourceCohort::seal(unknown,f.cpu,f.native,7,&f.error);REQUIRE(unqualified.cohort);
        f.error="previous empty-work error";
        CHECK_FALSE(v->makeView({}, {},std::span(&unqualified,1),3,&f.error));CHECK(f.error.empty());
        // A raw token may not overlap even a cohort filtered out as unproved.
        CHECK_FALSE(v->makeView({member(handle,matrix)},{0},std::span(&unqualified,1),3,&f.error));
    }
}

TEST_CASE("almetal: packed retained visibility preserves unequal groups sparse holes and ordered queries",
          "[almetal][world-visibility][world-cohort]")
{
    @autoreleasepool
    {
        for(unsigned group_count:{64u,256u})
        {
            CAPTURE(group_count);RetainedVisibilityFixture f(group_count==64?1:2);auto v=visibility(f);
            auto groups=unequalVisibilityGroups(f,group_count);
            const auto logical=groups.back().logical_begin+groups.back().cohort->size()+1;
            float matrix[16];identity(matrix);auto known=v->publishBounds(800,1,triangleBounds());
            auto view=v->makeView({member(known,matrix),member(known,matrix)},
                {0,static_cast<std::uint32_t>(logical-1)},groups,logical,&f.error);INFO(f.error);REQUIRE(view);
            auto command=f.device->makeCommandBuffer("One packed retained visibility dispatch",&f.error);REQUIRE(command);
            const auto before=v->stats();auto arguments=ALMetalWorldVisibility::encode(*command,*view,&f.error);
            if([(__bridge id<MTLDevice>)f.device->nativeHandle() argumentBuffersSupport]!=MTLArgumentBuffersTier2)
            {CHECK_FALSE(arguments);CHECK(v->stats().last_fallback==ALMetalWorldVisibilityFailure::cohort_batch_unsupported);continue;}
            INFO(f.error);REQUIRE(arguments);REQUIRE(command->commitAndWait(&f.error));
            ALMetalWorldIndexedArguments result;
            REQUIRE(arguments->readCompletedArgumentForTesting(0,result));CHECK(result.index_count==3);
            REQUIRE(arguments->readCompletedArgumentForTesting(logical-1,result));CHECK(result.index_count==3);
            for(const auto& group:groups)
            {
                for(std::size_t row=0;row<group.cohort->size();++row)
                {
                    CAPTURE(group.logical_begin,row);REQUIRE(arguments->readCompletedArgumentForTesting(group.logical_begin+row,result));
                    CHECK(result.index_count==(row==1?0:3));CHECK(result.instance_count==1);CHECK(result.index_start==0);
                    CHECK(result.base_vertex==0);CHECK(result.base_instance==0);
                }
                const auto hole=group.logical_begin+group.cohort->size();
                if(hole!=logical-1)CHECK_FALSE(arguments->readCompletedArgumentForTesting(hole,result));
            }
            const auto cold=v->stats();CHECK(cold.cohort_dispatches-before.cohort_dispatches==1);
            CHECK(cold.batched_cohort_dispatches-before.batched_cohort_dispatches==1);
            CHECK(cold.cohort_descriptor_upload_bytes-before.cohort_descriptor_upload_bytes==96*group_count);
            CHECK(cold.cohort_descriptor_allocated_bytes>=96*group_count);
            CHECK(cold.live_cohort_descriptor_bytes<=cold.cohort_descriptor_limit_bytes);
            CHECK(cold.live_cohort_row_bytes<=8*1024*1024);
            // Every unrepresented raw slot keeps its direct draw. Query offsets
            // span actual logical source order rather than packed compute order.
            std::vector<ALMetalWorldSimpleBatch> raw(group_count+1,f.cpu->sources[0]);
            std::array<ALMetalWorldVisibilityQuery,3> queries;
            for(unsigned index=0;index<queries.size();++index)
            {
                auto& q=queries[index];identity(q.modelview_projection);q.center[0]=.7f;q.center[1]=-.7f;q.center[2]=.75f;
                q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.05f;q.fan=7;q.count_samples=1;
                q.batch_offset=index==0?0:(index==1?groups[group_count/2].logical_begin:logical);
            }
            auto scene=f.scene();scene.batches=raw.data();scene.batch_count=raw.size();scene.cohorts=groups.data();scene.cohort_count=groups.size();
            scene.source_owner=f.native;scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
            std::shared_ptr<ALMetalVisibilityResultBatch> direct_queries,actual_queries;
            auto direct=f.renderer->render(scene,&f.error,&direct_queries);INFO(f.error);REQUIRE(direct);REQUIRE(direct_queries);
            const auto expected=f.read(*direct);scene.native_visibility=view;
            auto actual=f.renderer->render(scene,&f.error,&actual_queries);INFO(f.error);REQUIRE(actual);REQUIRE(actual_queries);
            CHECK(f.read(*actual)==expected);
            for(unsigned index=0;index<queries.size();++index)
            {
                std::uint64_t expected_count=0,actual_count=0;
                REQUIRE(direct_queries->wait(index,expected_count,&f.error));REQUIRE(actual_queries->wait(index,actual_count,&f.error));
                CHECK(actual_count==expected_count);if(index==0)CHECK(expected_count>0);
            }
            const auto warm=v->stats();CHECK(warm.cohort_row_builds==cold.cohort_row_builds);
            CHECK(warm.cohort_row_upload_bytes==cold.cohort_row_upload_bytes);
            CHECK(warm.batched_cohort_dispatches-cold.batched_cohort_dispatches==1);
            CHECK(warm.cohort_row_hits-cold.cohort_row_hits==group_count);
        }
    }
}

TEST_CASE("almetal: packed cohort descriptors preserve pending cameras and reclaim abandoned slot debt",
          "[almetal][world-visibility][world-cohort]")
{
    @autoreleasepool
    {
        RetainedVisibilityFixture f(1);auto v=visibility(f);auto groups=unequalVisibilityGroups(f,2);
        const auto logical=groups.back().logical_begin+groups.back().cohort->size()+1;
        auto first=v->makeView({}, {},groups,logical,&f.error);REQUIRE(first);
        auto old=f.device->makeCommandBuffer("Old packed cohort camera",&f.error);REQUIRE(old);
        auto old_args=ALMetalWorldVisibility::encode(*old,*first,&f.error);INFO(f.error);REQUIRE(old_args);
        const auto allocated=v->stats().cohort_descriptor_allocated_bytes;
        auto old_completion=old->completionToken();old.reset();CHECK(old_completion->isComplete());old_args.reset();
        // Abandon is synchronous: the same quiescent descriptor allocation can
        // be reused while no command or borrowed arguments can still read it.
        old=f.device->makeCommandBuffer("Reused packed cohort slot",&f.error);REQUIRE(old);
        old_args=ALMetalWorldVisibility::encode(*old,*first,&f.error);REQUIRE(old_args);
        CHECK(v->stats().cohort_descriptor_allocated_bytes==allocated);
        auto moved=groups;moved[0].view.modelview_projection[12]=4;
        // The second group also gets a genuinely new geometry/bounds row,
        // rather than only changing the per-view descriptor's camera.
        auto changed_cpu=std::make_shared<VisibilityCohortCpu>(*f.cpu);auto mesh=f.geometry;
        for(auto& position:mesh.positions){position[2]=-.5f;position[3]=std::bit_cast<float>(1u);}
        changed_cpu->sources[1].vertices=f.buffer(&mesh,sizeof(mesh));
        auto changed_native=std::make_shared<Resources>(*f.resources);auto changed_records=f.records;
        auto changed_bounds=triangleBounds();changed_bounds.minimum[2]=changed_bounds.maximum[2]=-.5f;
        changed_records[1]=ALMetalWorldCohortSource::seal({{101,1,2},&changed_cpu->sources[1]},&f.error,&changed_bounds);REQUIRE(changed_records[1]);
        moved[1].cohort=ALMetalWorldSourceCohort::seal(std::span(changed_records).first(2),changed_cpu,changed_native,8,&f.error);REQUIRE(moved[1].cohort);
        moved[1].cpu_owner=changed_cpu;moved[1].native_owner=changed_native;
        auto current=v->makeView({}, {},moved,logical,&f.error);REQUIRE(current);
        auto next=f.device->makeCommandBuffer("New packed cohort camera",&f.error);REQUIRE(next);
        auto next_args=ALMetalWorldVisibility::encode(*next,*current,&f.error);INFO(f.error);REQUIRE(next_args);
        CHECK(v->stats().cohort_descriptor_allocated_bytes>allocated);
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate{id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold both packed descriptor generations",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        REQUIRE(old->commit(&f.error));REQUIRE(next->commit(&f.error));
        CHECK_FALSE(old->completionToken()->isComplete());CHECK_FALSE(next->completionToken()->isComplete());
        const auto live=v->stats().live_cohort_descriptor_bytes;
        v.reset();groups.clear();moved.clear();first.reset();current.reset();f.cohort.reset();f.records.clear();
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));
        ALMetalWorldIndexedArguments argument;
        for(unsigned index:{1u,3u,4u})
        {
            REQUIRE(old_args->readCompletedArgumentForTesting(index,argument));CHECK(argument.index_count==(index==4?0:3));
            REQUIRE(next_args->readCompletedArgumentForTesting(index,argument));CHECK(argument.index_count==(index==1?0:3));
        }
        CHECK(live>=2*4096);
    }
}

TEST_CASE("almetal: packed cohort publication denial precedes row and descriptor allocation",
          "[almetal][world-visibility][world-cohort][world-source-admission]")
{
    @autoreleasepool
    {
        RetainedVisibilityFixture f(2);auto v=visibility(f);auto groups=unequalVisibilityGroups(f,64);
        const auto logical=groups.back().logical_begin+groups.back().cohort->size()+1;
        auto view=v->makeView({}, {},groups,logical,&f.error);REQUIRE(view);
        ALNativeFrameBudget budget;ALNativeFrameBudget::FrameInputs frame;frame.frame=1;budget.beginFrame(frame);
        bool allow=false;unsigned calls=0;std::size_t requested=0;
        f.data->setSourcePublicationAdmission([&](const ALMetalWorldSourcePublicationWork& work)->std::shared_ptr<const void>
        {
            ++calls;requested=work.chargedBytes();
            return allow?budget.tryAcquire(ALNativeFrameBudget::Work::scene_publication,requested,1).credit:nullptr;
        });
        auto denied=f.device->makeCommandBuffer("Denied packed cohort publication",&f.error);REQUIRE(denied);
        const auto allocations=alMetalResourceAllocationStats();
        CHECK_FALSE(ALMetalWorldVisibility::encode(*denied,*view,&f.error,f.data.get()));CHECK(calls==1);
        CHECK(v->stats().last_fallback==ALMetalWorldVisibilityFailure::admission);
        CHECK(v->stats().cohort_row_builds==0);CHECK(v->stats().cohort_descriptor_upload_bytes==0);
        CHECK(v->stats().live_cohort_descriptor_bytes==0);CHECK(alMetalResourceAllocationStats().buffers_created==allocations.buffers_created);
        allow=true;auto admitted=f.device->makeCommandBuffer("Admitted packed cohort publication",&f.error);REQUIRE(admitted);
        auto args=ALMetalWorldVisibility::encode(*admitted,*view,&f.error,f.data.get());INFO(f.error);REQUIRE(args);
        std::size_t rows=0;for(const auto& group:groups)rows+=group.cohort->size();CHECK(requested==rows*48);
        CHECK(budget.snapshot().scene_publications_in_flight==1);CHECK(budget.snapshot().scene_bytes_in_flight==rows*48);
        auto completion=admitted->completionToken();admitted.reset();CHECK(completion->isComplete());
        CHECK(budget.snapshot().scene_publications_in_flight==0);args.reset();
        const auto allocated=v->stats().cohort_descriptor_allocated_bytes;const auto before=v->stats();
        auto warm=f.device->makeCommandBuffer("Warm packed visibility has no cold ticket",&f.error);REQUIRE(warm);
        auto warm_args=ALMetalWorldVisibility::encode(*warm,*view,&f.error,f.data.get());REQUIRE(warm_args);
        CHECK(calls==2);CHECK(v->stats().cohort_row_upload_bytes==before.cohort_row_upload_bytes);
        CHECK(v->stats().cohort_descriptor_allocated_bytes==allocated);CHECK(budget.snapshot().scene_publications_in_flight==0);
        REQUIRE(warm->commitAndWait(&f.error));
    }
}

TEST_CASE("almetal: packed descriptor physical debt preserves pending and borrowed readers before recovery",
          "[almetal][world-visibility][world-cohort]")
{
    @autoreleasepool
    {
        RetainedVisibilityFixture f(1);auto v=visibility(f);auto group=f.view();
        group.cohort=ALMetalWorldSourceCohort::seal(std::span(f.records).first(1),f.cpu,f.native,7,&f.error);REQUIRE(group.cohort);
        // Many placements share ONE retained native row allocation. Descriptor
        // debt, not the 8MiB source-row cap or 32-slot count, is the constraint.
        constexpr unsigned placements=16384;std::vector<ALMetalWorldSourceCohortView> groups(placements,group);
        for(unsigned index=0;index<placements;++index){groups[index].logical_begin=index;groups[index].base_before=0;}
        auto view=v->makeView({},groups,placements,&f.error);INFO(f.error);REQUIRE(view);
        std::vector<std::unique_ptr<ALMetalCommandBuffer>> commands;
        std::vector<std::shared_ptr<ALMetalWorldVisibilityArguments>> arguments;
        for(unsigned attempt=0;attempt<5;++attempt)
        {
            auto command=f.device->makeCommandBuffer("Bound physical packed descriptor debt",&f.error);REQUIRE(command);
            auto result=ALMetalWorldVisibility::encode(*command,*view,&f.error);
            if(!result){CHECK(v->stats().last_fallback==ALMetalWorldVisibilityFailure::cohort_descriptors);break;}
            commands.push_back(std::move(command));arguments.push_back(std::move(result));
        }
        REQUIRE_FALSE(commands.empty());REQUIRE(commands.size()<=4);
        const auto full=v->stats();CHECK(full.cohort_row_builds==1);CHECK(full.live_cohort_row_bytes<=8*1024*1024);
        CHECK(full.live_cohort_descriptor_bytes<=full.cohort_descriptor_limit_bytes);
        REQUIRE(full.fallback_reasons[static_cast<std::size_t>(ALMetalWorldVisibilityFailure::cohort_descriptors)]>0);
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate{id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold packed descriptor readers pending",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        for(std::size_t index=1;index<commands.size();++index)
        {REQUIRE(commands[index]->commit(&f.error));CHECK_FALSE(commands[index]->completionToken()->isComplete());}
        auto abandoned=commands[0]->completionToken();commands[0].reset();CHECK(abandoned->isComplete());
        auto retry=f.device->makeCommandBuffer("Borrowed completed descriptors remain protected",&f.error);REQUIRE(retry);
        CHECK_FALSE(ALMetalWorldVisibility::encode(*retry,*view,&f.error));
        CHECK(v->stats().last_fallback==ALMetalWorldVisibilityFailure::cohort_descriptors);
        CHECK(v->stats().live_cohort_descriptor_bytes==full.live_cohort_descriptor_bytes);
        // Only after BOTH native completion/abandon and CPU reader release is
        // this allocation safe to overwrite. Other readers remain GPU-pending.
        arguments[0].reset();auto recovered=ALMetalWorldVisibility::encode(*retry,*view,&f.error);INFO(f.error);REQUIRE(recovered);
        CHECK(v->stats().cohort_descriptor_allocated_bytes==full.cohort_descriptor_allocated_bytes);
        CHECK(v->stats().live_cohort_descriptor_bytes==full.live_cohort_descriptor_bytes);
        REQUIRE(retry->commit(&f.error));gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));
        ALMetalWorldIndexedArguments result;
        REQUIRE(recovered->readCompletedArgumentForTesting(placements-1,result));CHECK(result.index_count==3);
        for(std::size_t index=1;index<arguments.size();++index)
        {REQUIRE(arguments[index]->readCompletedArgumentForTesting(placements-1,result));CHECK(result.index_count==3);}
    }
}
