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
        Fixture f;
        for(unsigned count:{16u,128u,1024u})
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
