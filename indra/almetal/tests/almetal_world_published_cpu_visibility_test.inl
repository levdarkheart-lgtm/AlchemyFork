/**
 * @file almetal_world_published_cpu_visibility_test.inl
 * @brief Published CPU frustum proof, logical order and native reader lifetime.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included by the existing native world-visibility fixture translation unit.
 */
#include <cmath>

namespace
{
namespace PublishedCpuVisibilityTest
{
struct Output
{
    Pixels pixels;
    std::vector<std::uint64_t> queries;
    bool operator==(const Output&) const=default;
};
Output render(Fixture& f,const ALMetalWorldGBufferScene& scene)
{
    std::shared_ptr<ALMetalVisibilityResultBatch> results;
    auto target=f.renderer->render(scene,&f.error,&results);INFO(f.error);REQUIRE(target);
    Output output{f.read(*target),{}};
    if(scene.visibility_query_count)
    {REQUIRE(results);output.queries.resize(scene.visibility_query_count);
     for(unsigned at=0;at<output.queries.size();++at)REQUIRE(results->wait(at,output.queries[at],&f.error));}
    return output;
}
std::vector<ALMetalWorldVisibilityQuery> queries(unsigned count)
{
    std::vector<ALMetalWorldVisibilityQuery> values(count+1);
    for(unsigned at=0;at<values.size();++at)
    {
        auto& q=values[at];identity(q.modelview_projection);q.center[0]=.7f;q.center[1]=-.7f;q.center[2]=-.5f;
        q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.04f;q.batch_offset=at;q.fan=7;q.count_samples=1;
    }
    return values;
}
ALMetalWorldConservativeBounds meshBounds(const Fixture::Geometry& mesh)
{
    ALMetalWorldConservativeBounds value;value.qualified=true;
    for(unsigned axis=0;axis<3;++axis)
    {
        value.minimum[axis]=std::numeric_limits<float>::infinity();value.maximum[axis]=-value.minimum[axis];
        // These are the exact three vertices addressed by Fixture::indices.
        for(unsigned vertex=0;vertex<3;++vertex)
        {value.minimum[axis]=std::min(value.minimum[axis],mesh.positions[vertex][axis]);
         value.maximum[axis]=std::max(value.maximum[axis],mesh.positions[vertex][axis]);
         value.qualified=value.qualified&&std::isfinite(mesh.positions[vertex][axis]);}
    }
    return value;
}
struct StressGroup
{
    std::shared_ptr<VisibilityCohortCpu> cpu=std::make_shared<VisibilityCohortCpu>();
    std::shared_ptr<const ALMetalWorldCohortSource> record;
    ALMetalWorldSourceCohortView view;
};
StressGroup stressGroup(Fixture& f,const Fixture::Geometry& mesh,std::uint64_t identity_value,
    const ALMetalWorldConservativeBounds* explicit_bounds=nullptr)
{
    StressGroup value;auto source=f.batch(0);source.vertices=f.buffer(&mesh,sizeof(mesh));
    value.cpu->sources.push_back(std::move(source));const auto actual=meshBounds(mesh);
    value.record=ALMetalWorldCohortSource::seal({{identity_value,0,1},&value.cpu->sources.front()},&f.error,
        explicit_bounds?explicit_bounds:&actual);INFO(f.error);REQUIRE(value.record);
    value.view.cohort=ALMetalWorldSourceCohort::seal(std::span(&value.record,1),value.cpu,f.resources,19,&f.error);
    INFO(f.error);REQUIRE(value.view.cohort);value.view.cpu_owner=value.cpu;value.view.native_owner=f.resources;
    value.view.view=visibilityMatrices(value.cpu->sources.front());return value;
}
}

TEST_CASE("almetal: published CPU visibility preserves interleaved raw reference terrain and query order",
          "[almetal][world-published-cpu-visibility][PPD-271]")
{
    using namespace PublishedCpuVisibilityTest;
    @autoreleasepool
    {
        for(unsigned family:{0u,1u,2u})
        {
            CAPTURE(family);RetainedVisibilityFixture f(family);auto group=f.view();group.logical_begin=group.base_before=1;
            group.view.texture_matrix[12]=std::numeric_limits<float>::quiet_NaN(); // Unused source-view UV.
            auto reference_cpu=std::make_shared<VisibilityCohortCpu>();reference_cpu->sources.push_back(f.batch(0));
            ALMetalWorldSourceReference reference;reference.source=&reference_cpu->sources.front();reference.view=visibilityMatrices(*reference.source);
            reference.view.modelview_projection[12]=.15f;
            auto cpu_owner=std::make_shared<std::pair<std::shared_ptr<VisibilityCohortCpu>,std::shared_ptr<VisibilityCohortCpu>>>(f.cpu,reference_cpu);
            std::vector<ALMetalWorldSimpleBatch> raw{f.batch(0),f.batch(7),f.batch(3)};
            raw[0].modelview_projection[0]=raw[0].modelview_projection[5]=.4f;
            raw[2].modelview_projection[0]=raw[2].modelview_projection[5]=.3f;raw[2].modelview_projection[12]=-.5f;
            const std::array<ALMetalWorldDrawIndex,4> order{0,alMetalWorldDrawReferenceBit,1,2};
            std::vector<ALMetalWorldSimpleBatch> flat{raw[0]};
            for(const auto& row:f.cpu->sources)flat.push_back(withVisibilityMatrices(row,group.view));
            flat.push_back(withVisibilityMatrices(*reference.source,reference.view));flat.push_back(raw[1]);flat.push_back(raw[2]);
            auto q=queries(7);
            for(unsigned format=0;format<8;++format)
            {
                CAPTURE(format);auto scene=f.scene();scene.hdr=format&1;scene.normal_hdr=format&2;scene.reverse_z=format&4;
                // PPD281: reverse-Z queries already consume native [0,w]
                // clip depth. Reflect the negative world-Z box in its MVP so
                // it stays inside that range and in front of the clear depth.
                for(auto& query:q)query.modelview_projection[10]=scene.reverse_z?-1.f:1.f;
                scene.batches=flat.data();scene.batch_count=flat.size();scene.source_owner=f.native;
                scene.visibility_queries=q.data();scene.visibility_query_count=q.size();
                const auto expected=render(f,scene);REQUIRE(expected.queries.front()>0);
                scene.batches=raw.data();scene.batch_count=raw.size();scene.references=&reference;scene.reference_count=1;
                scene.draw_order=order.data();scene.draw_order_count=order.size();scene.cohorts=&group;scene.cohort_count=1;scene.cpu_source_owner=cpu_owner;
                const auto before=f.renderer->worldSourceRoutingStats();scene.source_routing=true;
                CHECK(render(f,scene)==expected);const auto after=f.renderer->worldSourceRoutingStats();
                // A partially visible qualified union stays on the scalar route.
                CHECK(after.cohorts==before.cohorts+1);CHECK(after.scalar==before.scalar+1);
                CHECK(after.cpu==before.cpu);
            }
        }
    }
}

TEST_CASE("almetal: published CPU classifier is conservative at clip eye and floating cancellation boundaries",
          "[almetal][world-published-cpu-visibility][PPD-271]")
{
    using namespace PublishedCpuVisibilityTest;
    @autoreleasepool
    {
        Fixture f;float matrix[16];identity(matrix);const auto bounds=meshBounds(f.geometry);
        CHECK(alMetalClassifyWorldBoundsCPU(bounds,matrix)==ALMetalWorldCpuVisibilityResult::inside);
        matrix[12]=4;CHECK(alMetalClassifyWorldBoundsCPU(bounds,matrix)==ALMetalWorldCpuVisibilityResult::outside);
        auto unknown=bounds;unknown.qualified=false;CHECK(alMetalClassifyWorldBoundsCPU(unknown,matrix)==ALMetalWorldCpuVisibilityResult::unknown);
        identity(matrix);matrix[0]=std::numeric_limits<float>::quiet_NaN();CHECK(alMetalClassifyWorldBoundsCPU(bounds,matrix)==ALMetalWorldCpuVisibilityResult::unknown);
        identity(matrix);matrix[0]=std::numeric_limits<float>::denorm_min();CHECK(alMetalClassifyWorldBoundsCPU(bounds,matrix)==ALMetalWorldCpuVisibilityResult::unknown);
        identity(matrix);matrix[3]=1;matrix[15]=0;CHECK(alMetalClassifyWorldBoundsCPU(bounds,matrix)==ALMetalWorldCpuVisibilityResult::unknown);
        struct Case {Fixture::Geometry mesh;std::array<float,16> mvp;};
        std::vector<Case> cases;
        const auto add=[&](const Fixture::Geometry& mesh)
        {Case value{mesh,{}};identity(value.mvp.data());cases.push_back(value);};
        add(f.geometry); // Strictly inside: renderer must avoid per-row fine tests.
        add(f.geometry);cases.back().mvp[12]=4;
        for(float x:{std::nextafter(1.8f,0.f),1.8f,std::nextafter(1.8f,3.f)})
        {add(f.geometry);cases.back().mvp[12]=x;}
        for(float z:{std::nextafter(-1.25f,-2.f),-1.25f,std::nextafter(-1.25f,0.f),.75f})
        {add(f.geometry);cases.back().mvp[14]=z;}
        add(f.geometry);cases.back().mvp[3]=1;cases.back().mvp[15]=0;cases.back().mvp[0]=cases.back().mvp[5]=.1f;cases.back().mvp[10]=0;
        add(f.geometry);cases.back().mvp[0]=cases.back().mvp[5]=cases.back().mvp[10]=1.e-7f;cases.back().mvp[15]=1.e-7f;
        auto large=f.geometry;
        for(unsigned corner=0;corner<4;++corner)large.positions[corner][0]=16'777'216.f+(corner==1?2.f:0.f);
        add(large);cases.back().mvp[0]=.5f;cases.back().mvp[12]=-8'388'608.f;
        for(unsigned corner=0;corner<4;++corner)large.positions[corner][0]=100'000'000.f+(corner==1?8.f:0.f);
        add(large);cases.back().mvp[0]=.1f;cases.back().mvp[12]=-10'000'000.f;
        unsigned at=0;
        for(const auto& test:cases)
        {
            CAPTURE(at);auto group=stressGroup(f,test.mesh,1000+at++);
            std::copy(test.mvp.begin(),test.mvp.end(),group.view.view.modelview_projection);
            auto scene=f.scene();scene.hdr=true;scene.reverse_z=at%2;scene.source_owner=f.resources;
            scene.cohorts=&group.view;scene.cohort_count=1;
            const auto reference=render(f,scene);scene.source_routing=true;const auto before=f.renderer->worldCpuVisibilityStats();
            CHECK(render(f,scene)==reference);const auto after=f.renderer->worldCpuVisibilityStats();
            const auto classification=alMetalClassifyWorldBoundsCPU(group.record->bounds(),test.mvp.data());
            if(classification!=ALMetalWorldCpuVisibilityResult::outside)
            {CHECK(after.coarse_tests==before.coarse_tests);CHECK(after.fine_tests==before.fine_tests);}
            else
            {CHECK(after.coarse_outside==before.coarse_outside+1);CHECK(after.culled_draws==before.culled_draws+1);}
            if(classification==ALMetalWorldCpuVisibilityResult::outside)
            {scene.source_routing=false;scene.cohorts=nullptr;scene.cohort_count=0;CHECK(render(f,scene)==reference);}
        }
    }
}

TEST_CASE("almetal: unqualified members prevent coarse rejection of their visible geometry",
          "[almetal][world-published-cpu-visibility][PPD-271]")
{
    using namespace PublishedCpuVisibilityTest;
    @autoreleasepool
    {
        Fixture f;auto cpu=std::make_shared<VisibilityCohortCpu>();
        std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> records;
        cpu->sources.reserve(3);
        for(unsigned row=0;row<3;++row)
        {
            auto mesh=f.geometry;if(row<2)for(auto& point:mesh.positions)point[0]+=4.f+row;
            auto batch=f.batch(0);batch.vertices=f.buffer(&mesh,sizeof(mesh));cpu->sources.push_back(std::move(batch));
            const auto bounds=meshBounds(mesh);auto record=ALMetalWorldCohortSource::seal({{2000+row,0,1},&cpu->sources.back()},&f.error,row<2?&bounds:nullptr);
            INFO(f.error);REQUIRE(record);records.push_back(std::move(record));
        }
        ALMetalWorldSourceCohortView group;group.cohort=ALMetalWorldSourceCohort::seal(records,cpu,f.resources,27,&f.error);REQUIRE(group.cohort);
        group.cpu_owner=cpu;group.native_owner=f.resources;group.view=visibilityMatrices(cpu->sources.front());
        // PPD273: this union describes the TWO qualified members only.
        // Their outside result cannot reject the unqualified visible third.
        CHECK(group.cohort->qualifiedBoundsUnion().qualified);CHECK(group.cohort->qualifiedBoundsCount()==2);
        CHECK(alMetalClassifyWorldBoundsCPU(group.cohort->qualifiedBoundsUnion(),group.view.modelview_projection)==ALMetalWorldCpuVisibilityResult::outside);
        auto scene=f.scene();scene.source_owner=f.resources;scene.cohorts=&group;scene.cohort_count=1;
        const auto expected=render(f,scene);scene.source_routing=true;const auto before=f.renderer->worldCpuVisibilityStats();
        CHECK(render(f,scene)==expected);const auto after=f.renderer->worldCpuVisibilityStats();
        CHECK(after.culled_draws==before.culled_draws+2);CHECK(after.visible_draws==before.visible_draws+1);CHECK(after.unknown_draws==before.unknown_draws+1);
    }
}

TEST_CASE("almetal: CPU visibility releases CPU certificates while queued draws pin old native generations",
          "[almetal][world-published-cpu-visibility][PPD-271]")
{
    using namespace PublishedCpuVisibilityTest;
    @autoreleasepool
    {
        RetainedVisibilityFixture f(2);auto group=f.view();auto scene=f.scene();scene.source_owner=f.native;
        scene.cohorts=&group;scene.cohort_count=1;scene.source_routing=true;
        const auto expected=render(f,scene);
        REQUIRE(f.renderer->beginCommandBatch("Abandon published CPU visibility",&f.error));
        auto abandoned=f.renderer->render(scene,&f.error);REQUIRE(abandoned);auto abandoned_completion=f.renderer->commandBatchCompletion();
        f.renderer->cancelCommandBatch();CHECK(abandoned_completion->isComplete());
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate{id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold published CPU visibility reader",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        REQUIRE(f.renderer->beginCommandBatch("Old published CPU visibility native generation",&f.error));
        auto target=f.renderer->render(scene,&f.error);INFO(f.error);REQUIRE(target);auto completion=f.renderer->commandBatchCompletion();REQUIRE(completion);
        REQUIRE(f.renderer->commitCommandBatch(&f.error));CHECK_FALSE(completion->isComplete());
        const std::weak_ptr<VisibilityCohortCpu> weak_cpu=f.cpu;const std::weak_ptr<const ALMetalWorldSourceCohort> weak_cohort=f.cohort;
        const std::weak_ptr<const ALMetalWorldMaterialSource> weak_material=f.cpu->sources[0].native_material_source;
        const std::weak_ptr<Resources> weak_native=f.native;
        // A new actual native vertex generation and bounds publication may be
        // authored while the old native command is pending. No old CPU proof
        // is retained by that command or by CPU classification.
        auto next_mesh=f.geometry;for(auto& point:next_mesh.positions){point[0]*=.5f;point[2]=-.25f;}
        auto next=stressGroup(f,next_mesh,3000);next.view.view.modelview_projection[12]=.15f;
        scene={};group={};f.records.clear();f.cohort.reset();f.cpu.reset();f.native.reset();
        CHECK(weak_cpu.expired());CHECK(weak_cohort.expired());CHECK(weak_material.expired());CHECK_FALSE(weak_native.expired());
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(completion->isComplete());CHECK(f.read(*target)==expected.pixels);
        const auto retired=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!weak_native.expired()&&std::chrono::steady_clock::now()<retired)std::this_thread::yield();CHECK(weak_native.expired());
        auto current=f.scene();current.cohorts=&next.view;current.cohort_count=1;current.source_owner=f.resources;
        const auto next_reference=render(f,current);current.source_routing=true;CHECK(render(f,current)==next_reference);
    }
}
}

TEST_CASE("almetal: populated CPU routing counters and output survive shader reload",
          "[almetal][world-published-cpu-visibility][shader-reload][PPD-551]")
{
    using namespace PublishedCpuVisibilityTest;
    @autoreleasepool
    {
        RetainedVisibilityFixture f(0);
        auto group = f.view();
        auto scene = f.scene();
        scene.source_owner = f.native;
        scene.cohorts = &group;
        scene.cohort_count = 1;
        scene.source_routing = true;
        const auto visible = render(f, scene);
        group.view.modelview_projection[12] = 5.f;
        const auto outside = render(f, scene);
        const auto before = f.renderer->worldSourceRoutingStats();
        REQUIRE(before.cohorts > 0);
        REQUIRE(before.scalar > 0);
        REQUIRE(before.cpu > 0);
        REQUIRE(f.renderer->reloadShaders(&f.error));
        const auto after = f.renderer->worldSourceRoutingStats();
        CHECK(after.cohorts == before.cohorts);
        CHECK(after.scalar == before.scalar);
        CHECK(after.cpu == before.cpu);
        CHECK(render(f, scene) == outside);
        group.view.modelview_projection[12] = 0.f;
        CHECK(render(f, scene) == visible);
    }
}
