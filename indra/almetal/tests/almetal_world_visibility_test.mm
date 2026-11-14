/** Shared native visibility correctness and complete-path rejection fixtures. */
#include "almetal_world_prepare_fixture.h"
#include "../almetalworldvisibility.h"

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
        Fixture f;auto v=visibility(f);auto handle=v->publishBounds(40,1,triangleBounds());
        for(unsigned count:{16u,128u,1024u})for(unsigned outside_percent:{0u,50u})
        {
            std::vector<ALMetalWorldSimpleBatch> batches;
            for(unsigned index=0;index<count;++index)
            {auto b=f.batch(index%4);if(index%100<outside_percent)b.modelview_projection[12]=4;batches.push_back(std::move(b));}
            auto s=f.scene();s.batches=batches.data();s.batch_count=batches.size();
            const auto run=[&](bool gpu)
            {
                std::array<double,12> ms{};
                for(unsigned sample=0;sample<14;++sample)
                {
                    const auto start=std::chrono::steady_clock::now();
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
