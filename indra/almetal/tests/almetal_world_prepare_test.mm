/** Native world preparation correctness and complete-path cost tests. */
#include "almetal_world_prepare_fixture.h"

TEST_CASE("almetal: prepared world consumes every G-buffer family with exact direct pixels", "[almetal][world-prepare]")
{
    @autoreleasepool
    {
        Fixture f;
        for(unsigned family=0;family<10;++family)
        {
            CAPTURE(family);
            auto s=f.scene();auto b=f.batch(family);s.batches=&b;s.batch_count=1;
            if(family<7)s.hdr=true;
            auto direct=f.renderer->render(s,&f.error);INFO(f.error);REQUIRE(direct);const auto expected=f.read(*direct);
            auto ready=f.prepare(s,{b});CHECK(ready->stats().jobs==1);CHECK(ready->stats().prepared_count==1);
            auto prepared=f.renderer->renderPrepared(*ready,&f.error);INFO(f.error);REQUIRE(prepared);
            const auto actual=f.read(*prepared);CHECK(actual==expected);
            CHECK(std::any_of(actual.depth.begin(),actual.depth.end(),[](auto x){return x!=0x3f800000u;}));
        }
    }
}

TEST_CASE("almetal: small mixed prepared views preserve query and special-state boundaries", "[almetal][world-prepare]")
{
    @autoreleasepool
    {
        Fixture f;
        for(unsigned mode=0;mode<4;++mode)
        {
            CAPTURE(mode);
            auto s=f.scene();s.reverse_z=mode&1;s.wireframe=mode==2;s.stamp_opaque_coverage=mode==3;
            s.join_adjacent_static_draws=true;
            std::vector<ALMetalWorldSimpleBatch> batches;
            for(unsigned family=0;family<10;++family)batches.push_back(f.batch(family));
            batches.back().impostor_debug_outline=mode==2;batches.back().impostor_debug_line_width=mode==2?2.f:0.f;
            std::vector<ALMetalWorldVisibilityQuery> queries(3);
            for(unsigned q=0;q<queries.size();++q)
            {
                identity(queries[q].modelview_projection);queries[q].center[2]=.75f;
                queries[q].half_extent[0]=queries[q].half_extent[1]=queries[q].half_extent[2]=.05f;
                queries[q].batch_offset=q==0?0:q==1?5:batches.size();queries[q].fan=7;queries[q].count_samples=1;
            }
            s.batches=batches.data();s.batch_count=batches.size();s.visibility_queries=queries.data();s.visibility_query_count=queries.size();
            std::shared_ptr<ALMetalVisibilityResultBatch> direct_queries,prepared_queries;
            auto direct=f.renderer->render(s,&f.error,&direct_queries);INFO(f.error);REQUIRE(direct);REQUIRE(direct_queries);
            const auto expected=f.read(*direct);std::array<std::uint64_t,3> results{};
            for(unsigned q=0;q<3;++q)REQUIRE(direct_queries->wait(q,results[q],&f.error));
            auto ready=f.prepare(s,batches,queries);CHECK(ready->stats().jobs==2);CHECK(ready->stats().prepared_count==10);
            auto prepared=f.renderer->renderPrepared(*ready,&f.error,&prepared_queries);INFO(f.error);REQUIRE(prepared);REQUIRE(prepared_queries);
            CHECK(f.read(*prepared)==expected);
            for(unsigned q=0;q<3;++q){std::uint64_t value=0;REQUIRE(prepared_queries->wait(q,value,&f.error));CHECK(value==results[q]);}
            CHECK(results.front()>0);
        }
    }
}

TEST_CASE("almetal: repeated prepared common ranges restore after terrain and query boundaries", "[almetal][world-prepare]")
{
    @autoreleasepool
    {
        Fixture f;
        // Distinct triangles live in one region/agent-space buffer. Repeated
        // draws genuinely share matrices; per-object transforms do not hide
        // the common-constant reuse that the live viewer frequently exhibits.
        std::array<Fixture::Geometry,11> geometry;
        for(unsigned tile=0;tile<geometry.size();++tile)
        {
            geometry[tile]=f.geometry;
            const float x=(static_cast<float>(tile%4)-1.5f)*.45f;
            const float y=(static_cast<int>(tile/4)-1)*.6f;
            for(unsigned vertex=0;vertex<4;++vertex)
            {
                geometry[tile].positions[vertex][0]=f.geometry.positions[vertex][0]*.24f+x;
                geometry[tile].positions[vertex][1]=f.geometry.positions[vertex][1]*.24f+y;
                geometry[tile].colors[vertex][0]=static_cast<std::uint8_t>(55+tile*21);
                geometry[tile].colors[vertex][1]=static_cast<std::uint8_t>(225-tile*17);
                geometry[tile].colors[vertex][2]=static_cast<std::uint8_t>(75+tile*13);
            }
        }
        const auto* vertices=f.buffer(geometry.data(),sizeof(geometry));
        const auto tile_batch=[&](unsigned tile,unsigned family,bool vertex_color)
        {
            auto batch=f.batch(family);
            const auto base=tile*sizeof(Fixture::Geometry);
            batch.vertices=vertices;
            batch.position_offset+=base;batch.normal_offset+=base;
            batch.texcoord_offset+=base;batch.color_offset+=base;batch.tangent_offset+=base;
            batch.use_vertex_color=vertex_color;
            return batch;
        };
        for(bool reverse:{false,true})
        {
            CAPTURE(reverse);
            auto s=f.scene();s.reverse_z=reverse;
            auto terrain=f.batch(7),pbr_terrain=f.batch(8);
            for(auto* batch:{&terrain,&pbr_terrain})
            {
                batch->modelview_projection[0]=batch->modelview_projection[5]=.12f;
                batch->modelview_projection[12]=-.98f;
                batch->modelview_projection[13]=-.98f;
                batch->modelview_projection[14]=.75f;
            }
            pbr_terrain.modelview_projection[12]=.72f;
            auto impostor=f.batch(9);
            impostor.impostor_center[0]=impostor.impostor_center[1]=.95f;
            impostor.impostor_center[2]=.75f;
            impostor.impostor_left[0]=impostor.impostor_up[1]=.03f;
            // A,A,terrain,A,impostor,A,A | A,B,B,terrain,B,A,A.
            // The job split crosses equal A values. Specials retain the last
            // simple value; changed B and the final A require new ranges.
            std::vector<ALMetalWorldSimpleBatch> batches{
                tile_batch(0,0,true),tile_batch(1,1,true),terrain,
                tile_batch(2,2,true),impostor,tile_batch(3,3,true),tile_batch(4,0,true),
                tile_batch(5,1,true),tile_batch(6,0,false),tile_batch(7,2,false),pbr_terrain,
                tile_batch(8,1,false),tile_batch(9,0,true),tile_batch(10,2,true)};
            std::vector<ALMetalWorldVisibilityQuery> queries(4);
            const std::array<std::size_t,4> offsets{0,3,9,batches.size()};
            for(unsigned q=0;q<queries.size();++q)
            {
                identity(queries[q].modelview_projection);
                queries[q].center[0]=queries[q].center[1]=-.6f;
                queries[q].center[2]=.75f;
                queries[q].half_extent[0]=queries[q].half_extent[1]=queries[q].half_extent[2]=.03f;
                queries[q].batch_offset=offsets[q];queries[q].fan=7;queries[q].count_samples=1;
            }
            s.batches=batches.data();s.batch_count=batches.size();
            s.visibility_queries=queries.data();s.visibility_query_count=queries.size();
            std::shared_ptr<ALMetalVisibilityResultBatch> direct_queries,prepared_queries;
            auto direct=f.renderer->render(s,&f.error,&direct_queries);INFO(f.error);REQUIRE(direct);REQUIRE(direct_queries);
            const auto expected=f.read(*direct);
            std::array<std::uint64_t,4> results{};
            for(unsigned q=0;q<queries.size();++q)REQUIRE(direct_queries->wait(q,results[q],&f.error));
            REQUIRE(results.front()>0);
            auto ready=f.prepare(s,batches,queries,2);
            CHECK(ready->stats().jobs==2);
            CHECK(ready->stats().prepared_count==batches.size());
            CHECK(ready->stats().shared_common_records==8);
            CHECK(ready->stats().uncompacted_constant_bytes-ready->stats().constant_bytes==8*400);
            // Independently measure the real native query publication cost.
            // Query offsets order CPU replay but are absent from the shader
            // constants, so an empty scene can publish the identical values.
            auto query_only_scene=s;
            auto query_only=queries;
            for(auto& query:query_only)query.batch_offset=0;
            query_only_scene.batches=nullptr;query_only_scene.batch_count=0;
            query_only_scene.visibility_queries=query_only.data();
            const auto query_before=f.data->stats().published_bytes;
            auto query_target=f.renderer->render(query_only_scene,&f.error);INFO(f.error);REQUIRE(query_target);
            const auto query_bytes=f.data->stats().published_bytes-query_before;
            REQUIRE(query_bytes>0);
            const auto before=f.data->stats();
            auto prepared=f.renderer->renderPrepared(*ready,&f.error,&prepared_queries);INFO(f.error);REQUIRE(prepared);REQUIRE(prepared_queries);
            const auto published=f.data->stats().published_bytes-before.published_bytes;
            REQUIRE(published>=query_bytes);
            const auto geometry_bytes=published-query_bytes;
            // No palettes are present. Compare the actual geometry slab
            // publication separately from the independently measured queries.
            CHECK(geometry_bytes==ready->stats().constant_bytes);
            CHECK(geometry_bytes<ready->stats().uncompacted_constant_bytes);
            CHECK(f.read(*prepared)==expected);
            for(unsigned q=0;q<queries.size();++q)
            {
                std::uint64_t value=0;REQUIRE(prepared_queries->wait(q,value,&f.error));CHECK(value==results[q]);
            }
            const auto clear=reverse?0u:0x3f800000u;
            CHECK(std::count_if(expected.depth.begin(),expected.depth.end(),[&](auto value){return value!=clear;})>30);
        }
    }
}

TEST_CASE("almetal: world preparation owns poses and invalidates complete tickets on cancellation", "[almetal][world-prepare]")
{
    @autoreleasepool
    {
        Fixture f;auto s=f.scene();
        std::vector<ALMetalWorldSimpleBatch> batches{f.batch(4),f.batch(6)};
        auto inputs=ALMetalWorldViewInputs::adopt(s,batches,{},f.resources,9,&f.error);INFO(f.error);REQUIRE(inputs);
        CHECK(inputs->scene().batches[0].skin_palette!=f.attachment_pose.data());
        CHECK(inputs->scene().batches[1].skin_palette!=f.avatar_pose.data());
        f.attachment_pose.fill(std::numeric_limits<float>::quiet_NaN());f.avatar_pose.fill(std::numeric_limits<float>::quiet_NaN());
        auto epoch=std::make_shared<std::atomic<std::uint64_t>>(9);
        auto ticket=ALMetalWorldPreparation::create(inputs,2,epoch,&f.error);REQUIRE(ticket);
        CHECK_FALSE(ticket->finish(&f.error));f.error.clear();
        REQUIRE(ticket->runJob(0));epoch->store(10);CHECK_FALSE(ticket->runJob(1));CHECK_FALSE(ticket->finish(&f.error));
        epoch->store(9);ticket=ALMetalWorldPreparation::create(inputs,2,epoch,&f.error);REQUIRE(ticket);
        REQUIRE(ticket->runJob(0));REQUIRE(ticket->runJob(1));auto ready=ticket->finish(&f.error);INFO(f.error);REQUIRE(ready);
        const auto constants=ready->stats().constant_bytes;
        const auto shares=ready->stats().shared_common_records;
        CHECK(ticket->finish(&f.error)==ready);
        CHECK(ready->stats().constant_bytes==constants);CHECK(ready->stats().shared_common_records==shares);
        epoch->store(10);CHECK_FALSE(ticket->finish(&f.error));
        CHECK_FALSE(f.renderer->renderPrepared(*ready,&f.error));
        ticket->cancel();CHECK_FALSE(f.renderer->renderPrepared(*ready,&f.error));
        // Fresh preparation still succeeds after the viewer's mutable pose arrays changed.
        ticket=ALMetalWorldPreparation::create(inputs,1,{},&f.error);REQUIRE(ticket);REQUIRE(ticket->runJob(0));
        ready=ticket->finish(&f.error);REQUIRE(ready);REQUIRE(f.renderer->renderPrepared(*ready,&f.error));
        auto unowned=ALMetalWorldViewInputs::adopt(s,batches,{},nullptr,1,&f.error);CHECK_FALSE(unowned);
        auto invalid=std::vector<ALMetalWorldSimpleBatch>{f.batch(0),f.batch(0)};invalid[1].index_offset=1000000;
        auto invalid_inputs=ALMetalWorldViewInputs::adopt(s,std::move(invalid),{},f.resources,1,&f.error);REQUIRE(invalid_inputs);
        auto invalid_ticket=ALMetalWorldPreparation::create(invalid_inputs,2,{},&f.error);REQUIRE(invalid_ticket);
        REQUIRE(invalid_ticket->runJob(0));CHECK_FALSE(invalid_ticket->runJob(1));CHECK_FALSE(invalid_ticket->finish(&f.error));
    }
}

TEST_CASE("almetal: world preparation pins exact attachment records without copying their values",
          "[almetal][world-prepare][attachment-palette]")
{
    @autoreleasepool
    {
        Fixture f;
        auto record=ALMetalAttachmentPaletteRecord::create(f.attachment_pose.data(),f.attachment_pose.size(),1,&f.error);
        INFO(f.error);REQUIRE(record);
        auto batch=f.batch(4);
        batch.skin_palette_record=record.get();batch.skin_palette=record->data();
        const auto* values=record->data();
        auto scene=f.scene();
        auto inputs=ALMetalWorldViewInputs::adopt(scene,{batch,batch},{},f.resources,7,&f.error);
        INFO(f.error);REQUIRE(inputs);
        CHECK(inputs->scene().batches[0].skin_palette==values);
        CHECK(inputs->scene().batches[1].skin_palette==values);
        CHECK(inputs->scene().batches[0].skin_palette_record==record.get());
        auto native_owner=inputs->resourceLease();
        CHECK(native_owner==f.resources);
        std::weak_ptr<const ALMetalAttachmentPaletteRecord> weak=record;
        record.reset();CHECK_FALSE(weak.expired());
        f.attachment_pose.fill(std::numeric_limits<float>::quiet_NaN());
        auto epoch=std::make_shared<std::atomic<std::uint64_t>>(7);
        auto ticket=ALMetalWorldPreparation::create(inputs,2,epoch,&f.error);REQUIRE(ticket);
        REQUIRE(ticket->runJob(0));REQUIRE(ticket->runJob(1));
        auto ready=ticket->finish(&f.error);INFO(f.error);REQUIRE(ready);
        epoch->store(8);CHECK_FALSE(f.renderer->renderPrepared(*ready,&f.error));
        ticket->cancel();ready.reset();ticket.reset();inputs.reset();
        // The native-only resource aggregate is intentionally insufficient to
        // keep a CPU palette record alive after the joined packet is discarded.
        CHECK(weak.expired());
        CHECK(native_owner==f.resources);
    }
}

TEST_CASE("almetal: world attachment record proof rejects unrelated valid objects and layouts",
          "[almetal][world-prepare][attachment-palette]")
{
    @autoreleasepool
    {
        Fixture f;
        auto record=ALMetalAttachmentPaletteRecord::create(f.attachment_pose.data(),f.attachment_pose.size(),1,&f.error);
        auto other=ALMetalAttachmentPaletteRecord::create(f.attachment_pose.data(),f.attachment_pose.size(),1,&f.error);
        REQUIRE(record);REQUIRE(other);
        auto good=f.batch(4);good.skin_palette_record=record.get();good.skin_palette=record->data();
        auto scene=f.scene();
        const auto reject=[&](auto change)
        {
            auto wrong=good;change(wrong);
            scene.batches=&wrong;scene.batch_count=1;
            CHECK_FALSE(f.renderer->render(scene,&f.error));
            CHECK_FALSE(ALMetalWorldViewInputs::adopt(scene,{wrong},{},f.resources,7,&f.error));
        };
        reject([&](auto& b){b.skin_palette_record=other.get();});
        reject([](auto& b){b.skin_palette=nullptr;});
        reject([&](auto& b){b.skin_palette=f.attachment_pose.data();});
        reject([](auto& b){--b.skin_palette_float_count;});
        reject([](auto& b){++b.skin_joint_count;});
        reject([](auto& b){b.avatar_skinning=true;});
        reject([](auto& b){b.avatar_rigid=true;});
        reject([](auto& b){b.avatar_cloth=true;});
        reject([](auto& b){b.avatar_alpha=true;});
        reject([](auto& b){b.terrain=true;});
        reject([](auto& b){b.impostor=true;});
        // An unrelated attachment certificate never vouches for the separate
        // 180-float system-body ABI, even when both objects are valid.
        auto body=f.batch(6);body.skin_palette_record=record.get();
        scene.batches=&body;scene.batch_count=1;
        CHECK_FALSE(f.renderer->render(scene,&f.error));
        CHECK_FALSE(ALMetalWorldViewInputs::adopt(scene,{body},{},f.resources,7,&f.error));
        // Static geometry validation remains active after exact record proof.
        auto invalid=good;invalid.weight_offset=f.resources->buffers.front()->length();
        scene.batches=&invalid;
        CHECK_FALSE(f.renderer->render(scene,&f.error));
        auto inputs=ALMetalWorldViewInputs::adopt(scene,{invalid},{},f.resources,7,&f.error);REQUIRE(inputs);
        auto ticket=ALMetalWorldPreparation::create(inputs,1,{},&f.error);REQUIRE(ticket);
        CHECK_FALSE(ticket->runJob(0));CHECK_FALSE(ticket->finish(&f.error));
    }
}

TEST_CASE("almetal: denied shared preparation credit preserves local complete output", "[almetal][world-prepare]")
{
    @autoreleasepool
    {
        Fixture f;auto s=f.scene();
        std::vector<ALMetalWorldSimpleBatch> batches{f.batch(0),f.batch(1)};
        s.batches=batches.data();s.batch_count=batches.size();
        auto direct=f.renderer->render(s,&f.error);INFO(f.error);REQUIRE(direct);
        const auto expected=f.read(*direct);
        auto& budget=ALNativeFrameBudget::instance();
        ALNativeFrameBudget::FrameInputs plan;plan.frame=budget.snapshot().frame+1;
        plan.frame_interval_ns=16666667;budget.beginFrame(plan);
        auto first=budget.tryAcquire(ALNativeFrameBudget::Work::preparation).credit;
        auto second=budget.tryAcquire(ALNativeFrameBudget::Work::preparation).credit;
        REQUIRE(first);REQUIRE(second);
        const auto before=budget.snapshot();
        auto ready=f.prepare(s,batches,{},2);CHECK(ready->stats().jobs==2);
        const auto after=budget.snapshot();
        CHECK(after.deferred==before.deferred+1);
        CHECK(after.preparation_in_flight==before.preparation_in_flight);
        auto prepared=f.renderer->renderPrepared(*ready,&f.error);INFO(f.error);REQUIRE(prepared);
        CHECK(f.read(*prepared)==expected);
    }
}

TEST_CASE("almetal: prepared input leases survive caller destruction until command settlement", "[almetal][world-prepare]")
{
    @autoreleasepool
    {
        Fixture f;auto s=f.scene();std::weak_ptr<Resources> weak=f.resources;
        auto inputs=ALMetalWorldViewInputs::adopt(s,{f.batch(0)},{},f.resources,1,&f.error);REQUIRE(inputs);
        auto ticket=ALMetalWorldPreparation::create(inputs,1,{},&f.error);REQUIRE(ticket);REQUIRE(ticket->runJob(0));
        auto ready=ticket->finish(&f.error);REQUIRE(ready);inputs.reset();ticket.reset();
        REQUIRE(f.renderer->beginCommandBatch("Prepared world lease completion",&f.error));
        auto target=f.renderer->renderPrepared(*ready,&f.error);INFO(f.error);REQUIRE(target);
        auto completion=f.renderer->commandBatchCompletion();REQUIRE(completion);CHECK_FALSE(completion->isComplete());
        ready.reset();f.resources.reset();CHECK_FALSE(weak.expired());
        REQUIRE(f.renderer->commitCommandBatch(&f.error));f.renderer.reset();
        REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(completion->isComplete());
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!weak.expired() && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        CHECK(weak.expired());
        // Empty/sky-only packets retain the original successful clear behavior.
        auto empty=ALMetalWorldViewInputs::adopt(s,{}, {},{},2,&f.error);REQUIRE(empty);
        auto empty_ticket=ALMetalWorldPreparation::create(empty,2,{},&f.error);REQUIRE(empty_ticket);
        CHECK(empty_ticket->jobCount()==1);REQUIRE(empty_ticket->runJob(0));REQUIRE(empty_ticket->finish(&f.error));
    }
}

// Optional rejection fixture: includes adoption/pose copies, ticket allocation,
// jobs, dependency join, full native publication/binds/draws, commit and GPU
// completion. These controlled timings cannot establish a viewer FPS gain.
TEST_CASE("almetal: complete prepared world path cost matrix", "[almetal][.world-prepare-cost]")
{
    @autoreleasepool
    {
        Fixture f;std::uint64_t frame=1;
        for(unsigned count:{16u,128u,512u,1024u,4096u})
        {
            std::vector<ALMetalWorldSimpleBatch> batches;
            for(unsigned i=0;i<count;++i)batches.push_back(f.batch(i%7));
            auto s=f.scene();s.batches=batches.data();s.batch_count=batches.size();
            const auto measure=[&](bool prepared,unsigned jobs)
            {
                constexpr unsigned samples=12;
                std::array<double,samples> ms{};
                for(unsigned sample=0;sample<samples+2;++sample)
                {
                    const auto start=std::chrono::steady_clock::now();
                    f.data->beginFrame(++frame);
                    auto frame_batches=batches;
                    auto frame_scene=s;
                    frame_scene.batches=frame_batches.data();frame_scene.batch_count=frame_batches.size();
                    std::unique_ptr<ALMetalRenderTarget> target;
                    if(prepared)
                    {
                        auto ready=f.prepare(frame_scene,std::move(frame_batches),{},jobs);
                        target=f.renderer->renderPrepared(*ready,&f.error);
                    }
                    else target=f.renderer->render(frame_scene,&f.error);
                    INFO(f.error);REQUIRE(target);REQUIRE(f.device->waitUntilIdle(&f.error));
                    if(sample>=2)ms[sample-2]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                }
                std::sort(ms.begin(),ms.end());
                return std::array<double,2>{(ms[5]+ms[6])*.5,ms[11]};
            };
            const auto direct=measure(false,1),serial=measure(true,1),two=measure(true,2);
            std::cout << "world_prepare_complete_path count=" << count
                << " direct_median_ms=" << direct[0] << " direct_max_ms=" << direct[1]
                << " prepared_serial_median_ms=" << serial[0] << " prepared_serial_max_ms=" << serial[1]
                << " prepared_two_median_ms=" << two[0] << " prepared_two_max_ms=" << two[1] << '\n';
        }
    }
}
