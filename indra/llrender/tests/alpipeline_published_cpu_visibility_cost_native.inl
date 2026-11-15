/**
 * @file alpipeline_published_cpu_visibility_cost_native.inl
 * @brief Complete actual-producer costs of published CPU cohort visibility.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included at the end of the existing actual-owner fixture translation unit.
 */
namespace
{
namespace PublishedCpuVisibilityCost
{
constexpr U32 samples=12,modes=3;
struct Shape {U32 groups,sources,indices;};
struct Group
{
    U32 family=0,type=0;
    Inputs::ALScenePublicationNative::WorldFlavor flavor;
    std::vector<Inputs::LLDrawInfo*> sources;
    std::shared_ptr<ALNativePublication::State> state=std::make_shared<ALNativePublication::State>();
    std::shared_ptr<ALNativePublication::Observer> dependencies;
    std::shared_ptr<Inputs::ALSceneGroupPublicationNative::Layout> layout;
    ALNativeCohortCompilationAdmission compilation;
};
struct Arm
{
    Inputs::LLPipeline pipeline;
    std::vector<std::unique_ptr<Group>> groups;
    std::vector<ALMetalWorldVisibilityQuery> queries;
    ALNativeFrameBudget budget;
    std::unique_ptr<ALMetalWorldMaterialDataCache> material_cache;
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::unique_ptr<ALMetalWorldGBufferRenderer> main,probe;
    U64 native_epoch=0;
    ~Arm()
    {for(auto& group:groups){group->state->invalidate();for(auto* source:group->sources)source->state->invalidate();}}
};
struct MaterialCacheScope
{
    Arm& arm;std::unique_ptr<ALMetalWorldMaterialDataCache> previous;
    explicit MaterialCacheScope(Arm& value):arm(value)
    {previous.swap(Inputs::ALScenePublicationNative::material_data_cache);arm.material_cache.swap(Inputs::ALScenePublicationNative::material_data_cache);}
    ~MaterialCacheScope()
    {arm.material_cache.swap(Inputs::ALScenePublicationNative::material_data_cache);previous.swap(Inputs::ALScenePublicationNative::material_data_cache);}
};
struct Sample
{
    double cpu=0,encode=0,complete=0,gpu=0;
    U64 gpu_ready=0,raw_draws=0,cohort_groups=0,cohort_draws=0,replacements=0;
    U64 cpu_views=0,cpu_groups=0,cpu_coarse_tests=0,cpu_fine_tests=0,cpu_visible=0,cpu_culled=0,cpu_unknown=0,cpu_raw_visible=0;
    U64 scalar_builds=0,recipe_reads=0,common_reuses=0,recipe_metadata_bytes=0;
    U64 publications=0,group_builds=0,static_records=0,resource_descriptors=0;
    U64 getters=0,dependencies=0,image_getters=0,compile_admitted=0,compile_denied=0,compile_failed=0,compile_bytes=0;
    U64 raw_bytes=0,reference_bytes=0,order_bytes=0,cohort_bytes=0,native_bytes=0,cpu_pins=0,native_pins=0;
};
U64 cpuNanoseconds()
{timespec value{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0);return U64(value.tv_sec)*1'000'000'000ULL+value.tv_nsec;}
double median(std::array<double,samples> values)
{std::sort(values.begin(),values.end());return (values[5]+values[6])*.5;}

void run()
{
    constexpr bool many=true;
    const std::array<Shape,4> shapes{{{64,1,768},{64,2,768},{64,4,768},{64,2,3}}};
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(10);
    for(const auto shape:shapes)for(U32 views:{1u,3u})for(U32 condition:{0u,1u,2u})for(U32 outside_percent:{0u,50u})
    {
        CAPTURE(shape.groups,shape.sources,shape.indices,views,condition,outside_percent);
        // Exactly one NativeFont owner. Geometry/texture/material authoring is
        // excluded; both arms' FIRST source/group/recipe publication is timed.
        MaterialGroupPublication f(1);
        Inputs::gSavedSettings.native_resources=false;Inputs::gSavedSettings.native_visibility=false;
        std::array<Inputs::ALScenePublicationNative::WorldFlavor,2> flavors;
        for(U32 family=1;family<=2;++family)
        {auto& flavor=flavors[family-1];flavor.family=static_cast<Inputs::ALScenePublicationNative::WorldFamily>(family);
         flavor.native_materials=true;flavor.native_resources=false;}
        const auto selectFamily=[&](U32 family)
        {f.flavor=flavors[family-1];f.type=family==1?Inputs::LLRenderPass::PASS_MATERIAL:Inputs::LLRenderPass::PASS_GLTF_PBR;};
        auto library_data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(library_data);
        auto library=library_data->builtinLibrary(&f.error);INFO(f.error);REQUIRE(library);
        std::array<Arm,modes> arms;
        for(auto& arm:arms)
        {
            for(U32 index=0;index<shape.groups;++index)
            {
                auto group=std::make_unique<Group>();group->family=1+index%2;
                selectFamily(group->family);group->type=f.type;group->flavor=f.flavor;group->flavor.cohort_bounds=&arm==&arms[2];
                for(U32 source=0;source<shape.sources;++source)
                {
                    // Three adjacent indexed sources share common policy;
                    // every fourth ordinary source deliberately splits it.
                    auto& info=f.materialDraw(source%4!=3);
                    // Half of ONE-source groups really are outside; pair
                    // family groups so this does not cull only PBR. Larger
                    // groups interleave inside/outside sources within a cohort.
                    const bool outside=outside_percent && (shape.sources==1?(index/2)%2:source%2);
                    if(shape.indices>3)visibilityCostMesh(info,shape.indices,outside);
                    else if(outside)
                    {
                        std::array<LLVector4a,3> points;const auto slot=std::bit_cast<float>(source%4!=3?1u:0u);
                        points[0].set(3.f,-1.f,0.f,slot);points[1].set(3.9f,-1.f,0.f,slot);points[2].set(3.f,1.f,0.f,slot);
                        info.mVertexBuffer->setPositionData(points.data());info.mVertexBuffer->unmapBuffer();
                    }
                    // The fully visible control is strictly within the side
                    // planes, so it exercises the O(groups) coarse-inside
                    // route instead of touching every viewport edge. This
                    // asset transform is identical in all three arms.
                    LLStrider<LLVector3> positions;REQUIRE(info.mVertexBuffer->getVertexStrider(positions));
                    for(U32 vertex=0;vertex<shape.indices;++vertex)
                    {positions[vertex].mV[0]*=.75f;positions[vertex].mV[1]*=.75f;}
                    info.mVertexBuffer->unmapBuffer();
                    REQUIRE(info.mCount==shape.indices);
                    group->sources.push_back(&info);
                }
                arm.groups.push_back(std::move(group));
            }
            arm.material_cache=std::make_unique<ALMetalWorldMaterialDataCache>(*f.device);
            arm.data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(arm.data);
            auto* budget=&arm.budget;
            arm.data->setSourcePublicationAdmission(
                [budget,pending_since=std::optional<ALNativeFrameBudget::Frame>{}](const ALMetalWorldSourcePublicationWork& work)mutable->std::shared_ptr<const void>
                {if(!pending_since)pending_since=budget->snapshot().frame;
                 auto decision=budget->tryAcquire(ALNativeFrameBudget::Work::scene_publication,work.chargedBytes(),*pending_since);
                 if(decision)pending_since.reset();return decision.credit;});
            arm.main=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
            arm.probe=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
            INFO(f.error);REQUIRE(arm.main);REQUIRE(arm.probe);
            REQUIRE(arm.main->shaderLibrary()==library);REQUIRE(arm.probe->shaderLibrary()==library);
            arm.main->setFrameData(arm.data);arm.probe->setFrameData(arm.data);
        }
        // Many stable/churn cases establish real publication residency through
        // the unchanged quotas, not an untimed preseal or enlarged budget.
        // Cold always starts from raw. Every loading/settling sample is kept.
        const U32 loading=many&&condition!=0?8:0,settling=many&&condition!=0?64:2;
        const U32 measured_begin=loading+settling;
        std::array<std::array<Sample,samples>,modes> measured{};
        for(U32 frame=0;frame<measured_begin+samples;++frame)
        {
            const bool loading_now=frame<loading,measuring=frame>=measured_begin;
            const U32 visible_groups=loading_now?(frame+1)*shape.groups/loading:shape.groups;
            const U32 visible_draws=visible_groups*shape.sources;
            const bool compare_pixels=frame+1==measured_begin||frame+1==measured_begin+samples;
            ++Inputs::gFrameCount;
            for(auto& arm:arms){ALNativeFrameBudget::FrameInputs input;input.frame=Inputs::gFrameCount;arm.budget.beginFrame(input);}
            std::array<std::array<GroupPublication::Image,3>,modes> pixels;
            std::array<std::array<std::vector<U64>,3>,modes> queries;
            for(U32 position=0;position<modes;++position)
            {
                INFO("Scalar recipe complete cost exceeded its ten-minute workload cap");REQUIRE(std::chrono::steady_clock::now()<deadline);
                const U32 mode=(frame+position)%modes;auto& arm=arms[mode];CAPTURE(mode,frame,visible_groups);
                MaterialCacheScope material_scope(arm);auto& capture=*arm.pipeline.mMetalWorldSimpleCapture;
                const auto before_reads=f.reads();const auto before_serial=Inputs::sNativeSceneSerial;
                const auto before_groups=Inputs::ALSceneGroupPublicationNative::statistics;
                const auto before_compile=Inputs::ALSceneGroupCompilationNative::statistics;
                const auto before_records=Inputs::ALScenePublicationNative::cohort_source_publications;
                const auto before_resources=Inputs::ALScenePublicationNative::resource_source_publications;
                const auto before_native=arm.data->stats().published_bytes;
                const std::array<ALMetalWorldCpuVisibilityStats,2> before_cpu{arm.main->worldCpuVisibilityStats(),arm.probe->worldCpuVisibilityStats()};
                const std::array<ALMetalWorldCohortStats,2> before_render{arm.main->worldCohortStats(),arm.probe->worldCohortStats()};
                std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> results;
                Sample value;const auto begin=std::chrono::steady_clock::now();const auto begin_cpu=cpuNanoseconds();
                arm.data->beginFrame(++arm.native_epoch);
                if(condition==0)for(auto& group:arm.groups)
                {for(auto* source:group->sources)source->state->invalidate();
                 if(mode){group->state->invalidate();group->layout.reset();group->dependencies.reset();}}
                const bool churn_now=condition==2&&frame+2>=measured_begin;
                if(churn_now)
                {
                    const auto tint=frame%2?LLColor4U(0,255,0,255):LLColor4U(0,0,255,255);
                    const std::vector<LLColor4U> colors(shape.indices,tint);U32 ordinal=0;
                    for(auto& group:arm.groups)for(auto* source:group->sources)
                    {if(ordinal++%16==0){source->mVertexBuffer->setColorData(colors.data());source->mVertexBuffer->unmapBuffer();}}
                }
                for(U32 view=0;view<views;++view)
                {
                    REQUIRE(f.region.update(f.model,LLVector3(.01f*view,0.f,0.f)));
                    capture.clearDraws();capture.complete=true;selectFamily(1);f.configure(capture,false,false);
                    capture.world_resources=std::make_shared<Inputs::WorldResources>();Inputs::LLCullResult cull;
                    struct Pending{U32 type,raw_begin,count;};
                    std::unordered_map<Group*,std::vector<Pending>> pending;
                    const auto current=[&](Group& group,const ALNativeRegionModel::Snapshot& region)
                    {return Inputs::ALSceneGroupPublicationNative::currentMaterial(group.state,region,group.flavor,
                        Inputs::sNativeSceneSources,Inputs::gFrameCount);};
                    for(U32 family=1;family<=2;++family)for(U32 index=0;index<visible_groups;++index)
                    {
                        auto& group=*arm.groups[index];if(group.family!=family)continue;
                        const auto region=f.region.snapshot();auto packet=mode?current(group,region):std::shared_ptr<const NativeGroup>{};
                        if(packet)group.compilation.publicationCompleted();
                        if(packet)REQUIRE(cull.pushNativeMaterialRange(group.type,packet,shape.sources,region));
                        else
                        {const auto raw_begin=U32(cull.endRenderMap(group.type)-cull.beginRenderMap(group.type));
                         for(auto* source:group.sources)cull.pushDrawInfo(group.type,source);
                         if(mode)pending[&group].push_back({group.type,raw_begin,shape.sources});}
                    }
                    LLVertexBuffer::flushBuffers();
                    if(mode)
                    {
                        struct Candidate
                        {Group* group;U32 type;std::shared_ptr<ALNativePublication::State> publisher;
                         ALNativeCohortCompilationAdmission* admission;Inputs::ALScenePublicationNative::WorldFlavor flavor;};
                        std::vector<Candidate> cold;const auto frame_id=arm.budget.snapshot().frame;
                        const auto consume=[&](Group& group,U32 type,const std::shared_ptr<const NativeGroup>& packet,
                            const ALNativeRegionModel::Snapshot& region,U32 family)
                        {
                            const auto at=pending.find(&group);if(!packet||!packet->cohort||at==pending.end())return;
                            const auto span=std::find_if(at->second.begin(),at->second.end(),[type](const auto& p){return p.type==type;});
                            if(span==at->second.end()||group.sources.size()!=span->count||packet->cohort->size()!=span->count||
                               packet->cohort->family()!=family||packet->cohort->regionIdentity()!=region.identity||
                               !packet->cohort->matchesOwners(packet,packet->native_tuple))return;
                            std::vector<Inputs::LLDrawInfo*> expected;expected.reserve(group.sources.size());
                            for(auto* source:group.sources)expected.push_back(source);
                            REQUIRE(cull.replaceNativeMaterialRange(type,packet,span->raw_begin,expected,region));++value.replacements;
                        };
                        for(U32 index=0;index<visible_groups;++index)
                        {
                            auto& group=*arm.groups[index];if(!group.state->dirty()&&!pending.contains(&group))continue;
                            const auto region=f.region.snapshot();auto packet=current(group,region);
                            if(packet&&packet->cohort&&packet->cohort->size()==group.sources.size()&&
                               packet->cohort->family()==group.family&&packet->cohort->regionIdentity()==region.identity&&
                               packet->cohort->matchesOwners(packet,packet->native_tuple))
                            {group.compilation.publicationCompleted();consume(group,group.type,packet,region,group.family);continue;}
                            group.compilation.notePending(frame_id);
                            if(group.compilation.lastConsideredFrame()&&frame_id<=*group.compilation.lastConsideredFrame())
                                ++Inputs::ALSceneGroupCompilationNative::statistics.duplicate_views;
                            else{++Inputs::ALSceneGroupCompilationNative::statistics.candidates;
                                cold.push_back({&group,group.type,group.state,&group.compilation,group.flavor});}
                        }
                        std::sort(cold.begin(),cold.end(),[](const auto& a,const auto& b)
                        {return a.admission->priority(a.publisher->identity())<b.admission->priority(b.publisher->identity());});
                        for(const auto& candidate:cold)
                        {
                            auto& group=*candidate.group;const auto region=f.region.snapshot();auto packet=current(group,region);
                            if(packet&&packet->cohort&&packet->cohort->size()==group.sources.size()&&
                               packet->cohort->family()==group.family&&packet->cohort->regionIdentity()==region.identity&&
                               packet->cohort->matchesOwners(packet,packet->native_tuple))
                            {candidate.admission->publicationCompleted();consume(group,candidate.type,packet,region,group.family);continue;}
                            packet=Inputs::ALSceneGroupCompilationNative::compile(*candidate.admission,candidate.publisher,arm.budget,
                                group.sources.size(),group.family,[&]
                                {return Inputs::ALSceneGroupPublicationNative::sealMaterial(candidate.publisher,group.dependencies,group.layout,
                                    f.model,region,group.sources,candidate.flavor,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);});
                            consume(group,candidate.type,packet,region,group.family);
                        }
                    }
                    {CullScope scope(cull);for(U32 family=1;family<=2;++family)
                        {selectFamily(family);
                         capture.material_normal_map=f.flavor.normal_map;capture.material_specular_map=f.flavor.specular_map;
                         capture.material_alpha_mode=f.flavor.material_alpha_mode;capture.alpha_mask=f.flavor.alpha_mask;capture.pbr=family==2;f.capture(arm.pipeline);}}
                    INFO(f.error);REQUIRE(capture.complete);REQUIRE(capture.logicalDrawCount()==visible_draws);REQUIRE(capture.flushDrawOrder());
                    value.raw_draws+=capture.batches.size();value.cohort_groups+=capture.source_cohorts.size();value.cohort_draws+=capture.source_cohort_draw_count;
                    value.raw_bytes+=capture.batches.size()*sizeof(ALMetalWorldSimpleBatch);
                    value.reference_bytes+=capture.source_references.size()*sizeof(ALMetalWorldSourceReference);
                    value.order_bytes+=capture.draw_order.size()*sizeof(ALMetalWorldDrawIndex);value.cohort_bytes+=capture.source_cohorts.size()*sizeof(ALMetalWorldSourceCohortView);
                    value.cpu_pins+=capture.source_cohorts.size()+(capture.cpu_sources?capture.cpu_sources->owners.size():0);
                    value.native_pins+=capture.source_cohorts.size()+capture.world_resources->owners.size();
                    CHECK(capture.source_references.empty());CHECK_FALSE(capture.source_rows);
                    arm.queries.resize(visible_groups+1);
                    for(U32 query=0;query<arm.queries.size();++query)
                    {auto& q=arm.queries[query];q={};std::memcpy(q.modelview_projection,f.model.getF32ptr(),sizeof(q.modelview_projection));
                     // PPD269: both left/right triangles are real occluders.
                     // Put the required-positive box in front, within clip.
                     q.center[0]=.35f;q.center[1]=-.5f;q.center[2]=-.5f;q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.08f;
                     q.batch_offset=query?query*shape.sources:shape.sources/2;q.fan=7;q.count_samples=1;}
                    auto scene=f.scene(capture);scene.visibility_queries=arm.queries.data();scene.visibility_query_count=arm.queries.size();
                    scene.material_argument_buffers=false;scene.source_command_banks=false;scene.source_direct_command_banks=false;scene.cpu_cohort_visibility=mode==2;
                    auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(capture.batches),std::move(capture.source_references),
                        std::move(capture.draw_order),std::move(capture.source_cohorts),std::move(arm.queries),std::move(capture.cpu_sources),
                        capture.world_resources,arm.native_epoch,&f.error);INFO(f.error);REQUIRE(inputs);scene={};
                    auto& renderer=view?*arm.probe:*arm.main;REQUIRE(renderer.beginCommandBatch("Actual raw direct and published CPU cohort visibility cost",&f.error));
                    targets[view]=renderer.renderOwnedInputs(*inputs,&f.error,&results[view]);INFO(f.error);REQUIRE(targets[view]);REQUIRE(results[view]);
                    completions[view]=renderer.commandBatchCompletion();
                    REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,capture.source_references,capture.draw_order,capture.source_cohorts,arm.queries));
                    capture.clearDraws();capture.world_resources=std::make_shared<Inputs::WorldResources>();cull.clear();REQUIRE(renderer.commitCommandBatch(&f.error));
                }
                const auto encoded=std::chrono::steady_clock::now();REQUIRE(f.device->waitUntilIdle(&f.error));
                for(U32 view=0;view<views;++view)
                {queries[mode][view].resize(visible_groups+1);for(U32 query=0;query<=visible_groups;++query)
                    {REQUIRE(results[view]->wait(query,queries[mode][view][query],&f.error));CHECK(queries[mode][view][query]>0);}}
                gGL.frameFences().release();const auto completed=std::chrono::steady_clock::now();
                value.cpu=(cpuNanoseconds()-begin_cpu)/1e6;value.encode=std::chrono::duration<double,std::milli>(encoded-begin).count();
                value.complete=std::chrono::duration<double,std::milli>(completed-begin).count();
                for(U32 view=0;view<views;++view)
                {ALMetalCommandGPUTime time;if(completions[view]&&completions[view]->gpuTime(time)==ALMetalCommandTimingStatus::ready)
                    {value.gpu+=time.elapsed_nanoseconds/1e6;++value.gpu_ready;}}
                const auto after_reads=f.reads();const auto after_groups=Inputs::ALSceneGroupPublicationNative::statistics;
                const auto after_compile=Inputs::ALSceneGroupCompilationNative::statistics;const auto budget=arm.budget.snapshot();
                value.getters=after_reads[0]-before_reads[0];value.dependencies=after_reads[1]-before_reads[1];value.image_getters=after_reads[2]-before_reads[2];
                value.publications=Inputs::sNativeSceneSerial-before_serial;value.group_builds=after_groups.builds-before_groups.builds;
                value.static_records=Inputs::ALScenePublicationNative::cohort_source_publications-before_records;
                value.resource_descriptors=Inputs::ALScenePublicationNative::resource_source_publications-before_resources;
                value.compile_admitted=after_compile.admitted-before_compile.admitted;value.compile_denied=after_compile.denied-before_compile.denied;
                value.compile_failed=after_compile.failed-before_compile.failed;value.compile_bytes=after_compile.admitted_bytes-before_compile.admitted_bytes;
                value.native_bytes=arm.data->stats().published_bytes-before_native;
                const std::array<ALMetalWorldCohortStats,2> after_render{arm.main->worldCohortStats(),arm.probe->worldCohortStats()};U64 accepted=0;
                const std::array<ALMetalWorldCpuVisibilityStats,2> after_cpu{arm.main->worldCpuVisibilityStats(),arm.probe->worldCpuVisibilityStats()};
                for(U32 owner=0;owner<2;++owner)
                {accepted+=after_render[owner].draws-before_render[owner].draws;
                 value.scalar_builds+=after_render[owner].scalar_common_builds-before_render[owner].scalar_common_builds;
                 value.recipe_reads+=after_render[owner].scalar_recipe_reads-before_render[owner].scalar_recipe_reads;
                 value.common_reuses+=after_render[owner].scalar_common_reuses-before_render[owner].scalar_common_reuses;
                 value.cpu_views+=after_cpu[owner].views-before_cpu[owner].views;value.cpu_groups+=after_cpu[owner].groups-before_cpu[owner].groups;
                 value.cpu_coarse_tests+=after_cpu[owner].coarse_tests-before_cpu[owner].coarse_tests;value.cpu_fine_tests+=after_cpu[owner].fine_tests-before_cpu[owner].fine_tests;
                 value.cpu_visible+=after_cpu[owner].visible_draws-before_cpu[owner].visible_draws;value.cpu_culled+=after_cpu[owner].culled_draws-before_cpu[owner].culled_draws;
                 value.cpu_unknown+=after_cpu[owner].unknown_draws-before_cpu[owner].unknown_draws;value.cpu_raw_visible+=after_cpu[owner].raw_visible_draws-before_cpu[owner].raw_visible_draws;}
                // Capacity census only, AFTER both timers; do not add a warm
                // per-source metadata walk to the actual producer/renderer path.
                for(const auto& group:arm.groups)if(auto raw=group->state->publication())
                {auto packet=std::static_pointer_cast<const NativeGroup>(raw);if(packet->cohort)value.recipe_metadata_bytes+=packet->cohort->scalarCommonMetadataBytes();}
                CHECK(accepted+value.cpu_culled==value.cohort_draws);CHECK(value.raw_draws+value.cohort_draws==U64(visible_draws)*views);
                CHECK(value.resource_descriptors==0);CHECK(budget.scene_publications_in_flight==0);CHECK(budget.scene_bytes_in_flight==0);
                CHECK(value.compile_failed==0);CHECK(value.group_builds==value.compile_admitted);CHECK(value.gpu_ready==views);
                CHECK(value.recipe_reads+value.cpu_culled==value.cohort_draws);CHECK(value.scalar_builds+value.common_reuses+value.cpu_culled==U64(visible_draws)*views);
                if(mode==2){CHECK(value.cpu_views==views);CHECK(value.cpu_visible+value.cpu_culled==U64(visible_draws)*views);CHECK(value.cpu_raw_visible==value.raw_draws);}
                else{CHECK(value.cpu_views==0);CHECK(value.cpu_culled==0);}
                if(!mode){CHECK(value.cohort_draws==0);CHECK(value.recipe_reads==0);CHECK(value.common_reuses==0);CHECK(value.compile_admitted==0);}
                if(measuring&&condition==0&&mode)
                {CHECK(value.compile_denied>0);CHECK(value.raw_draws>0);} // Unchanged16-publication quota,64groups.
                if(measuring&&condition==1&&mode)
                {CHECK(value.cohort_draws==U64(visible_draws)*views);CHECK(value.compile_admitted==0);CHECK(value.compile_denied==0);
                 CHECK(value.getters==0);CHECK(value.dependencies==0);CHECK(value.image_getters==0);CHECK(value.static_records==0);
                 if(mode==2)CHECK(value.cpu_culled==(outside_percent?U64(visible_draws/2)*views:0));}
                if(measuring)measured[mode][frame-measured_begin]=value;
                if(compare_pixels)for(U32 view=0;view<views;++view)pixels[mode][view]=f.read(*targets[view]);
                std::cout<<"NATIVE_PUBLISHED_CPU_VISIBILITY_RAW indices_per_source="<<shape.indices<<" outside_percent="<<outside_percent<<" groups="<<shape.groups<<" visible_groups="<<visible_groups
                    <<" sources_per_group="<<shape.sources<<" views="<<views<<" condition="<<condition<<" mode="<<mode<<" frame="<<frame
                    <<" phase="<<(loading_now?"loading":measuring?"measured":"warmup")<<" owner_cpu_ms="<<value.cpu
                    <<" capture_adopt_encode_reclaim_ms="<<value.encode<<" completion_query_settle_ms="<<value.complete<<" gpu_sum_ms="<<value.gpu<<" gpu_ready="<<value.gpu_ready
                    <<" raw_draws="<<value.raw_draws<<" captured_groups="<<value.cohort_groups<<" captured_draws="<<value.cohort_draws<<" accepted_draws="<<accepted
                    <<" cpu_visibility_views="<<value.cpu_views<<" cpu_groups="<<value.cpu_groups<<" cpu_coarse_tests="<<value.cpu_coarse_tests<<" cpu_fine_tests="<<value.cpu_fine_tests
                    <<" cpu_visible_draws="<<value.cpu_visible<<" cpu_culled_draws="<<value.cpu_culled<<" cpu_unknown_visible_draws="<<value.cpu_unknown<<" cpu_raw_visible_draws="<<value.cpu_raw_visible
                    <<" scalar_common_builds="<<value.scalar_builds<<" scalar_recipe_reads="<<value.recipe_reads<<" scalar_common_reuses="<<value.common_reuses
                    <<" scalar_common_metadata_bytes="<<value.recipe_metadata_bytes<<" source_getters="<<value.getters<<" dependency_walks="<<value.dependencies<<" image_getters="<<value.image_getters
                    <<" source_group_publications="<<value.publications<<" group_builds="<<value.group_builds<<" static_source_records="<<value.static_records<<" producer_resource_descriptors="<<value.resource_descriptors
                    <<" compile_admitted="<<value.compile_admitted<<" compile_denied="<<value.compile_denied<<" compile_failed="<<value.compile_failed<<" compile_logical_bytes="<<value.compile_bytes
                    <<" same_view_replacements="<<value.replacements<<" raw_value_bytes="<<value.raw_bytes<<" reference_value_bytes="<<value.reference_bytes<<" order_value_bytes="<<value.order_bytes
                    <<" cohort_value_bytes="<<value.cohort_bytes<<" native_published_bytes="<<value.native_bytes<<" cpu_description_pins="<<value.cpu_pins<<" native_generation_pins="<<value.native_pins
                    <<" modified_source_count="<<(churn_now?(shape.groups*shape.sources+15)/16:0)<<" pixel_parity_checked="<<compare_pixels<<'\n';
            }
            // PPD273: BOTH retained controls, including actual CPU culling,
            // must preserve the ordinary reference pixels and logical queries.
            for(U32 view=0;view<views;++view)for(U32 mode=1;mode<modes;++mode)
            {CHECK(queries[0][view]==queries[mode][view]);if(compare_pixels)CHECK(pixels[0][view]==pixels[mode][view]);}
        }
        for(U32 mode=0;mode<modes;++mode)
        {
            std::array<double,samples> cpu{},encode{},complete{},gpu{};Sample total;
            for(U32 sample=0;sample<samples;++sample)
            {const auto& v=measured[mode][sample];cpu[sample]=v.cpu;encode[sample]=v.encode;complete[sample]=v.complete;gpu[sample]=v.gpu;
             total.cpu_views+=v.cpu_views;total.cpu_visible+=v.cpu_visible;total.cpu_culled+=v.cpu_culled;total.cpu_unknown+=v.cpu_unknown;
             total.cpu_coarse_tests+=v.cpu_coarse_tests;total.cpu_fine_tests+=v.cpu_fine_tests;total.cpu_raw_visible+=v.cpu_raw_visible;
             total.raw_draws+=v.raw_draws;total.cohort_draws+=v.cohort_draws;total.scalar_builds+=v.scalar_builds;total.recipe_reads+=v.recipe_reads;
             total.common_reuses+=v.common_reuses;total.compile_admitted+=v.compile_admitted;total.compile_denied+=v.compile_denied;total.compile_failed+=v.compile_failed;
             total.compile_bytes+=v.compile_bytes;total.group_builds+=v.group_builds;total.static_records+=v.static_records;total.native_bytes+=v.native_bytes;
             total.recipe_metadata_bytes=std::max(total.recipe_metadata_bytes,v.recipe_metadata_bytes);}
            std::cout<<"NATIVE_PUBLISHED_CPU_VISIBILITY_COST indices_per_source="<<shape.indices<<" outside_percent="<<outside_percent<<" groups="<<shape.groups<<" sources_per_group="<<shape.sources<<" views="<<views
                <<" condition="<<(condition==0?"cold":condition==1?"warm":"churn_1of16")<<" mode="<<(mode==0?"ordinary_native":mode==1?"retained_direct":"published_cpu_visibility_requested")
                <<" measured_samples="<<samples<<" loading_frames="<<loading<<" warmup_frames="<<settling
                <<" owner_cpu_median_ms="<<median(cpu)<<" owner_cpu_max_ms="<<*std::max_element(cpu.begin(),cpu.end())
                <<" capture_adopt_encode_reclaim_median_ms="<<median(encode)<<" capture_adopt_encode_reclaim_max_ms="<<*std::max_element(encode.begin(),encode.end())
                <<" completion_query_settle_median_ms="<<median(complete)<<" completion_query_settle_max_ms="<<*std::max_element(complete.begin(),complete.end())
                <<" gpu_sum_median_ms="<<median(gpu)<<" gpu_sum_max_ms="<<*std::max_element(gpu.begin(),gpu.end())
                <<" measured_cpu_views="<<total.cpu_views<<" measured_cpu_coarse_tests="<<total.cpu_coarse_tests<<" measured_cpu_fine_tests="<<total.cpu_fine_tests
                <<" measured_cpu_visible_draws="<<total.cpu_visible<<" measured_cpu_culled_draws="<<total.cpu_culled<<" measured_cpu_unknown_visible_draws="<<total.cpu_unknown
                <<" measured_raw_draws="<<total.raw_draws<<" measured_cohort_draws="<<total.cohort_draws<<" measured_scalar_common_builds="<<total.scalar_builds
                <<" measured_scalar_recipe_reads="<<total.recipe_reads<<" measured_scalar_common_reuses="<<total.common_reuses<<" max_scalar_common_metadata_bytes="<<total.recipe_metadata_bytes
                <<" measured_compile_admitted="<<total.compile_admitted<<" measured_compile_denied="<<total.compile_denied<<" measured_compile_failed="<<total.compile_failed
                <<" measured_compile_logical_bytes="<<total.compile_bytes<<" measured_group_builds="<<total.group_builds<<" measured_static_source_records="<<total.static_records
                <<" measured_native_published_bytes="<<total.native_bytes
                <<" cpu_visibility_activation_is_actual_counter=1 cold_actual_geometry_bounds_scan_and_cohort_metadata_timed=1 gpu_visibility_banks_rows_tables_off=1"
                <<" dirty_raw_admission_pending_map_full_candidate_sort_timed=1 same_view_verified_replacement=1 actual_cpu_scene_admission=1 denied_samples_preserve_raw=1"
                <<" independent_source_group_admission_framedata_renderers_material_cache=1 one_nativefont_host=1 shared_exact_library=1 paired_outer_frame=1 native_epoch_once_per_arm_frame=1"
                <<" distinct_view_region_payloads=1 identity_camera_frontier=1 queries_group_boundaries_plus_first_interior=1 pass_order_legacy_then_pbr=1"
                <<" common_policy_pattern=three_indexed_one_ordinary_per_group require_stable_revision=0 synthetic_idle_budget_inputs=1"
                <<" geometry_texture_drawinfo_creation_excluded=1 asset_xy_scale=0.75 target_pool_retirement_excluded=1 unique_vertex_indices=1 no_repeated_triangle_index_mesh=1 scalar_metadata_capacity_census_after_timer=1 idle_barrier_commands_per_arm=1 actual_outside_source_count="<<(outside_percent?shape.groups*shape.sources/2:0)<<" actual_outside_percent="<<outside_percent<<" whole_viewer_fps_claim=0\n";
        }
    }
}
}
}

TEST_CASE("Actual published CPU visibility includes bounds publication and complete small-group mesh costs",
          "[almetal][.native-published-cpu-visibility-cost]")
{PublishedCpuVisibilityCost::run();}
