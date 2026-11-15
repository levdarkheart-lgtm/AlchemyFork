/**
 * @file alpipeline_many_cohort_cost_native.inl
 * @brief Many-small-cohort complete-path costs through the actual producer.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after alpipeline_buffer_publication_metal_test.cpp's fixtures.
 */

TEST_CASE("Actual many small mixed cohorts retain loading and cache pressure costs",
          "[almetal][.native-many-cohort-cost]")
{
    constexpr U32 loading_frames=8,warmup_frames=64,measured_frames=12;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(10);
    const auto cpu_ns=[]
    {timespec value{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0);return U64(value.tv_sec)*1'000'000'000ULL+value.tv_nsec;};
    const auto median=[](auto values)
    {std::sort(values.begin(),values.end());return (values[5]+values[6])*.5;};
    struct Sample
    {
        double cpu=0,encode=0,complete=0,gpu=0;
        U64 captured_groups=0,captured_draws=0,accepted_draws=0,raw_draws=0,replacements=0;
        U64 requested=0,bound=0,fallback=0,executions=0,hits=0,builds=0,evictions=0;
        U64 compile_admitted=0,compile_denied=0,page_hits=0,page_builds=0,page_failures=0;
    };
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
    for(U32 group_count:{64u,256u})for(U32 sources_per_group:{2u,4u})for(U32 views:{1u,3u})
    {
        CAPTURE(group_count,sources_per_group,views);
        MaterialGroupPublication f(1);
        Inputs::gSavedSettings.native_resources=false;Inputs::gSavedSettings.native_visibility=false;
        std::array<Inputs::ALScenePublicationNative::WorldFlavor,2> flavors;
        for(U32 family=1;family<=2;++family)
        {flavors[family-1].family=static_cast<Inputs::ALScenePublicationNative::WorldFamily>(family);
         flavors[family-1].native_materials=true;flavors[family-1].native_resources=false;}
        const auto select_family=[&](U32 family)
        {f.flavor=flavors[family-1];f.type=family==1?Inputs::LLRenderPass::PASS_MATERIAL:Inputs::LLRenderPass::PASS_GLTF_PBR;};
        auto library_data=ALMetalWorldFrameData::create(*f.device,&f.error);INFO(f.error);REQUIRE(library_data);
        auto library=library_data->builtinLibrary(&f.error);INFO(f.error);REQUIRE(library);
        std::array<Arm,3> arms;
        for(auto& arm:arms)
        {
            // Independent producer identities and LLDrawInfo owners prevent
            // either cohort arm from warming the ordinary arm's source cache.
            // Creation/upload of these input assets is the excluded frontier;
            // their first native source/group publication is timed below.
            for(U32 index=0;index<group_count;++index)
            {
                auto group=std::make_unique<Group>();group->family=1+index%2;select_family(group->family);
                group->type=f.type;group->flavor=f.flavor;
                for(U32 source=0;source<sources_per_group;++source)group->sources.push_back(&f.materialDraw(source%2));
                arm.groups.push_back(std::move(group));
            }
            arm.material_cache=std::make_unique<ALMetalWorldMaterialDataCache>(*f.device);
            arm.data=ALMetalWorldFrameData::create(*f.device,&f.error);INFO(f.error);REQUIRE(arm.data);
            auto* budget=&arm.budget;
            arm.data->setSourcePublicationAdmission(
                [budget,pending_since=std::optional<ALNativeFrameBudget::Frame>{}](const ALMetalWorldSourcePublicationWork& work)mutable->std::shared_ptr<const void>
                {
                    if(!pending_since)pending_since=budget->snapshot().frame;
                    auto decision=budget->tryAcquire(ALNativeFrameBudget::Work::scene_publication,work.chargedBytes(),*pending_since);
                    if(decision)pending_since.reset();return decision.credit;
                });
            arm.main=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
            arm.probe=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
            INFO(f.error);REQUIRE(arm.main);REQUIRE(arm.probe);
            REQUIRE(arm.main->shaderLibrary()==library);REQUIRE(arm.probe->shaderLibrary()==library);
            arm.main->setFrameData(arm.data);arm.probe->setFrameData(arm.data);
        }
        std::array<std::array<Sample,measured_frames>,3> measured{};
        for(U32 frame=0;frame<loading_frames+warmup_frames+measured_frames;++frame)
        {
            const bool loading=frame<loading_frames;
            const bool warmup=!loading&&frame<loading_frames+warmup_frames;
            const U32 visible_groups=loading?(frame+1)*group_count/loading_frames:group_count;
            const U32 visible_draws=visible_groups*sources_per_group;
            const bool compare_pixels=frame==loading_frames-1||frame==loading_frames+warmup_frames-1||(!loading&&!warmup);
            ++Inputs::gFrameCount;
            for(auto& arm:arms)
            {ALNativeFrameBudget::FrameInputs input;input.frame=Inputs::gFrameCount;arm.budget.beginFrame(input);}
            std::array<std::array<GroupPublication::Image,3>,3> pixels;
            std::array<std::array<std::vector<std::uint64_t>,3>,3> query_counts;
            for(U32 position=0;position<3;++position)
            {
                INFO("Bounded many-cohort fixture exceeded its ten-minute workload cap");
                REQUIRE(std::chrono::steady_clock::now()<deadline);
                const U32 mode=(frame+position)%3;auto& arm=arms[mode];CAPTURE(mode,frame,visible_groups);
                MaterialCacheScope material_scope(arm);auto& capture=*arm.pipeline.mMetalWorldSimpleCapture;
                const auto before_reads=f.reads();const auto before_serial=Inputs::sNativeSceneSerial;
                const auto before_groups=Inputs::ALSceneGroupPublicationNative::statistics;
                const auto before_compile=Inputs::ALSceneGroupCompilationNative::statistics;
                const auto before_records=Inputs::ALScenePublicationNative::cohort_source_publications;
                const auto before_descriptors=Inputs::ALScenePublicationNative::resource_source_publications;
                const auto before_native=arm.data->stats().published_bytes;const auto before_budget=arm.budget.snapshot();
                const std::array<ALMetalWorldCohortStats,2> before_cohort{arm.main->worldCohortStats(),arm.probe->worldCohortStats()};
                const std::array<ALMetalWorldSourceCommandDrawStats,2> before_draw{arm.main->worldSourceCommandStats(),arm.probe->worldSourceCommandStats()};
                // These caches are shared by main/probe within one arm.
                // Observe them once, never sum their duplicate snapshots.
                const auto before_cache=arm.main->worldSourceCommandStats().cache;
                const auto before_pages=arm.main->worldSourceStats().pages;
                std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> results;
                U64 raw_bytes=0,reference_bytes=0,order_bytes=0,cohort_bytes=0,candidate_bytes=0,regional_bytes=0,cpu_pins=0,native_pins=0,gpu_ready=0;
                Sample value;const auto begin=std::chrono::steady_clock::now();const auto begin_cpu=cpu_ns();
                arm.data->beginFrame(++arm.native_epoch);
                for(U32 view=0;view<views;++view)
                {
                    // Preserve the typed fixture's identity-camera frontier,
                    // with distinct actual copied matrix payloads per view.
                    REQUIRE(f.region.update(f.model,LLVector3(.01f*view,0.f,0.f)));
                    capture.clearDraws();capture.complete=true;select_family(1);f.configure(capture,false,mode==2);
                    capture.world_resources=std::make_shared<Inputs::WorldResources>();Inputs::LLCullResult cull;
                    struct PendingGroupRange{U32 type,raw_begin,count;};
                    std::unordered_map<Group*,std::vector<PendingGroupRange>> pending_group_ranges;
                    const auto current=[&](Group& group,const ALNativeRegionModel::Snapshot& region)
                    {return Inputs::ALSceneGroupPublicationNative::currentMaterial(group.state,region,group.flavor,
                        Inputs::sNativeSceneSources,Inputs::gFrameCount);};
                    // Match the real deferred pool order: all Legacy groups,
                    // then PBR groups. Loading admits equal numbers of both.
                    for(U32 family=1;family<=2;++family)for(U32 index=0;index<visible_groups;++index)
                    {
                        auto& group=*arm.groups[index];if(group.family!=family)continue;
                        const auto region=f.region.snapshot();auto packet=mode?current(group,region):std::shared_ptr<const NativeGroup>{};
                        if(packet)group.compilation.publicationCompleted();
                        if(packet)REQUIRE(cull.pushNativeMaterialRange(group.type,packet,sources_per_group,region));
                        else
                        {
                            const auto raw_begin=U32(cull.endRenderMap(group.type)-cull.beginRenderMap(group.type));
                            for(auto* source:group.sources)cull.pushDrawInfo(group.type,source);
                            if(mode)pending_group_ranges[&group].push_back({group.type,raw_begin,sources_per_group});
                        }
                    }
                    LLVertexBuffer::flushBuffers();
                    if(mode)
                    {
                        struct ColdCohort
                        {
                            Group* group;U32 type;
                            std::shared_ptr<ALNativePublication::State> publisher;
                            ALNativeCohortCompilationAdmission* admission;
                            Inputs::ALScenePublicationNative::WorldFlavor flavor;
                        };
                        std::vector<ColdCohort> cold;const auto budget_frame=arm.budget.snapshot().frame;
                        const auto consume=[&](Group& group,U32 type,const std::shared_ptr<const NativeGroup>& packet,
                            const ALNativeRegionModel::Snapshot& region,U32 family)
                        {
                            const auto pending=pending_group_ranges.find(&group);
                            if(!packet||!packet->cohort||pending==pending_group_ranges.end())return;
                            const auto span=std::find_if(pending->second.begin(),pending->second.end(),
                                [type](const auto& value){return value.type==type;});
                            if(span==pending->second.end()||group.sources.size()!=span->count||packet->cohort->size()!=span->count||
                               packet->cohort->family()!=family||packet->cohort->regionIdentity()!=region.identity||
                               !packet->cohort->matchesOwners(packet,packet->native_tuple))return;
                            std::vector<Inputs::LLDrawInfo*> expected;expected.reserve(group.sources.size());
                            for(auto* source:group.sources)expected.push_back(source);
                            REQUIRE(cull.replaceNativeMaterialRange(type,packet,span->raw_begin,expected,region));
                            ++value.replacements;
                        };
                        // Match postSort's owner scan, full candidate value
                        // copies and per-group pending-map/vector allocation.
                        // These are timed cold costs, not free fixture setup.
                        for(U32 index=0;index<visible_groups;++index)
                        {
                            auto& group=*arm.groups[index];
                            if(!group.state->dirty()&&!pending_group_ranges.contains(&group))continue;
                            const auto region=f.region.snapshot();auto packet=current(group,region);
                            if(packet&&packet->cohort&&packet->cohort->size()==group.sources.size()&&
                               packet->cohort->family()==group.family&&packet->cohort->regionIdentity()==region.identity&&
                               packet->cohort->matchesOwners(packet,packet->native_tuple))
                            {group.compilation.publicationCompleted();consume(group,group.type,packet,region,group.family);continue;}
                            auto& admission=group.compilation;admission.notePending(budget_frame);
                            if(admission.lastConsideredFrame()&&budget_frame<=*admission.lastConsideredFrame())
                                ++Inputs::ALSceneGroupCompilationNative::statistics.duplicate_views;
                            else{++Inputs::ALSceneGroupCompilationNative::statistics.candidates;
                                cold.push_back({&group,group.type,group.state,&admission,group.flavor});}
                        }
                        std::sort(cold.begin(),cold.end(),[](const auto& a,const auto& b)
                        {return a.admission->priority(a.publisher->identity())<b.admission->priority(b.publisher->identity());});
                        for(const auto& candidate:cold)
                        {
                            auto& group=*candidate.group;const auto region=f.region.snapshot();
                            auto packet=Inputs::ALSceneGroupPublicationNative::currentMaterial(candidate.publisher,region,
                                candidate.flavor,Inputs::sNativeSceneSources,Inputs::gFrameCount);
                            if(packet&&packet->cohort&&packet->cohort->size()==group.sources.size()&&
                               packet->cohort->family()==static_cast<U32>(candidate.flavor.family)&&
                               packet->cohort->regionIdentity()==region.identity&&packet->cohort->matchesOwners(packet,packet->native_tuple))
                            {candidate.admission->publicationCompleted();consume(group,candidate.type,packet,region,static_cast<U32>(candidate.flavor.family));continue;}
                            packet=Inputs::ALSceneGroupCompilationNative::compile(*candidate.admission,candidate.publisher,arm.budget,
                                group.sources.size(),static_cast<U32>(candidate.flavor.family),[&]
                                {return Inputs::ALSceneGroupPublicationNative::sealMaterial(candidate.publisher,group.dependencies,group.layout,
                                    f.model,region,group.sources,candidate.flavor,Inputs::sNativeSceneSources,
                                    Inputs::sNativeSceneSerial,Inputs::gFrameCount);});
                            consume(group,candidate.type,packet,region,static_cast<U32>(candidate.flavor.family));
                        }
                    }
                    {
                        CullScope scope(cull);
                        for(U32 family=1;family<=2;++family)
                        {
                            select_family(family);
                            // Only the pass flags change here. Re-running
                            // beginSourceRows would erase the earlier pass.
                            capture.material_normal_map=f.flavor.normal_map;capture.material_specular_map=f.flavor.specular_map;
                            capture.material_alpha_mode=f.flavor.material_alpha_mode;capture.alpha_mask=f.flavor.alpha_mask;
                            capture.pbr=family==2;f.capture(arm.pipeline);
                        }
                    }
                    INFO(f.error);REQUIRE(capture.complete);REQUIRE(capture.logicalDrawCount()==visible_draws);REQUIRE(capture.flushDrawOrder());
                    value.captured_groups+=capture.source_cohorts.size();value.captured_draws+=capture.source_cohort_draw_count;
                    value.raw_draws+=capture.batches.size();raw_bytes+=capture.batches.size()*sizeof(ALMetalWorldSimpleBatch);
                    reference_bytes+=capture.source_references.size()*sizeof(ALMetalWorldSourceReference);order_bytes+=capture.draw_order.size()*sizeof(ALMetalWorldDrawIndex);
                    cohort_bytes+=capture.source_cohorts.size()*sizeof(ALMetalWorldSourceCohortView);
                    cpu_pins+=capture.source_cohorts.size()+(capture.cpu_sources?capture.cpu_sources->owners.size():0);
                    native_pins+=capture.source_cohorts.size()+capture.world_resources->owners.size();
                    if(capture.source_rows)
                    {candidate_bytes+=capture.source_rows->candidates.size()*sizeof(ALMetalWorldSourceView::Candidate);
                     regional_bytes+=capture.source_rows->regions.size()*sizeof(ALMetalWorldSourceRegion);}
                    // A denied group may legitimately expand the old raw
                    // source-row path. Retain and report that cost/fallback.
                    CHECK(capture.source_references.empty());
                    if(capture.source_cohort_draw_count==visible_draws)CHECK((!capture.source_rows||capture.source_rows->candidates.empty()));
                    arm.queries.resize(visible_groups);
                    for(U32 query=0;query<visible_groups;++query)
                    {
                        auto& q=arm.queries[query];q={};std::memcpy(q.modelview_projection,f.model.getF32ptr(),sizeof(q.modelview_projection));
                        q.center[0]=.35f;q.center[1]=-.5f;q.center[2]=.75f;q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.08f;
                        q.batch_offset=(query+1)*sources_per_group;q.fan=7;q.count_samples=1;
                    }
                    auto scene=f.scene(capture);scene.visibility_queries=arm.queries.data();scene.visibility_query_count=arm.queries.size();
                    scene.material_argument_buffers=false;scene.source_command_banks=mode==2;scene.source_command_minimum_run=2;
                    auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(capture.batches),std::move(capture.source_references),
                        std::move(capture.draw_order),std::move(capture.source_cohorts),std::move(arm.queries),std::move(capture.cpu_sources),
                        capture.world_resources,arm.native_epoch,&f.error);INFO(f.error);REQUIRE(inputs);scene={};
                    auto& renderer=view?*arm.probe:*arm.main;
                    REQUIRE(renderer.beginCommandBatch("Actual many-small-cohort complete cost",&f.error));
                    targets[view]=renderer.renderOwnedInputs(*inputs,&f.error,&results[view]);INFO(f.error);REQUIRE(targets[view]);REQUIRE(results[view]);
                    completions[view]=renderer.commandBatchCompletion();
                    REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,capture.source_references,capture.draw_order,capture.source_cohorts,arm.queries));
                    capture.clearDraws();capture.world_resources=std::make_shared<Inputs::WorldResources>();cull.clear();
                    REQUIRE(renderer.commitCommandBatch(&f.error));
                }
                const auto encoded=std::chrono::steady_clock::now();REQUIRE(f.device->waitUntilIdle(&f.error));
                for(U32 view=0;view<views;++view)
                {
                    query_counts[mode][view].resize(visible_groups);
                    for(U32 query=0;query<visible_groups;++query)
                    {REQUIRE(results[view]->wait(query,query_counts[mode][view][query],&f.error));CHECK(query_counts[mode][view][query]>0);}
                }
                const auto retire_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
                while(arm.budget.snapshot().scene_publications_in_flight&&std::chrono::steady_clock::now()<retire_deadline)std::this_thread::yield();
                gGL.frameFences().release();const auto completed=std::chrono::steady_clock::now();
                value.cpu=(cpu_ns()-begin_cpu)/1e6;value.encode=std::chrono::duration<double,std::milli>(encoded-begin).count();
                value.complete=std::chrono::duration<double,std::milli>(completed-begin).count();
                for(U32 view=0;view<views;++view)
                {ALMetalCommandGPUTime timing;if(completions[view]&&completions[view]->gpuTime(timing)==ALMetalCommandTimingStatus::ready)
                    {value.gpu+=timing.elapsed_nanoseconds/1e6;++gpu_ready;}}
                const auto reads=f.reads();const auto groups=Inputs::ALSceneGroupPublicationNative::statistics;
                const auto compilation=Inputs::ALSceneGroupCompilationNative::statistics;const auto budget=arm.budget.snapshot();
                const std::array<ALMetalWorldCohortStats,2> cohorts{arm.main->worldCohortStats(),arm.probe->worldCohortStats()};
                const std::array<ALMetalWorldSourceCommandDrawStats,2> draws{arm.main->worldSourceCommandStats(),arm.probe->worldSourceCommandStats()};
                for(U32 owner=0;owner<2;++owner)
                {value.accepted_draws+=cohorts[owner].draws-before_cohort[owner].draws;
                 value.requested+=draws[owner].requested_draws-before_draw[owner].requested_draws;
                 value.bound+=draws[owner].bank_draws-before_draw[owner].bank_draws;
                 value.fallback+=draws[owner].fallback_draws-before_draw[owner].fallback_draws;
                 value.executions+=draws[owner].execute_calls-before_draw[owner].execute_calls;}
                const auto cache=arm.main->worldSourceCommandStats().cache;const auto pages=arm.main->worldSourceStats().pages;
                value.hits=cache.hits-before_cache.hits;value.builds=cache.builds-before_cache.builds;value.evictions=cache.evictions-before_cache.evictions;
                value.page_hits=pages.cohort_hits-before_pages.cohort_hits;value.page_builds=pages.cohort_builds-before_pages.cohort_builds;
                value.page_failures=pages.cohort_failures-before_pages.cohort_failures;
                value.compile_admitted=compilation.admitted-before_compile.admitted;value.compile_denied=compilation.denied-before_compile.denied;
                CHECK(value.accepted_draws==value.captured_draws);CHECK(value.bound+value.fallback==value.requested);
                CHECK(budget.scene_publications_in_flight==0);CHECK(budget.scene_bytes_in_flight==0);
                if(mode!=2){CHECK(value.requested==0);CHECK(value.bound==0);CHECK(value.executions==0);}
                if(mode==0){CHECK(value.captured_groups==0);CHECK(value.raw_draws==U64(visible_draws)*views);}
                // PPD218: all64groups fit the unchanged native caches. After
                // the declared warmups, unrelated row publications must not
                // keep invalidating their exact table/membership proofs.
                if(mode==2&&group_count==64&&!loading&&!warmup)
                {
                    CHECK(value.requested==U64(visible_draws)*views);
                    CHECK(value.bound==value.requested);CHECK(value.fallback==0);
                    CHECK(value.hits==U64(group_count)*views);CHECK(value.builds==0);CHECK(value.evictions==0);
                    CHECK(value.page_hits==U64(group_count)*views);CHECK(value.page_builds==0);CHECK(value.page_failures==0);
                    CHECK(cache.admission_denials==before_cache.admission_denials);
                    CHECK(pages.admission_denials==before_pages.admission_denials);
                }
                // The256group distribution can exceed the unchanged caps;
                // retain every actual rebuild, denial and direct fallback.
                if(!loading&&!warmup)measured[mode][frame-loading_frames-warmup_frames]=value;
                if(compare_pixels)for(U32 view=0;view<views;++view)pixels[mode][view]=f.read(*targets[view]);
                std::cout<<"NATIVE_MANY_COHORT_RAW groups="<<group_count<<" visible_groups="<<visible_groups<<" sources_per_group="<<sources_per_group
                    <<" legacy_groups="<<visible_groups/2<<" pbr_groups="<<visible_groups/2<<" views="<<views<<" queries_per_view="<<visible_groups
                    <<" phase="<<(loading?"loading":warmup?"warmup":"measured")<<" frame="<<frame<<" mode="<<mode
                    <<" owner_cpu_ms="<<value.cpu<<" capture_adopt_encode_reclaim_ms="<<value.encode<<" completion_and_query_settle_ms="<<value.complete
                    <<" gpu_sum_ms="<<value.gpu<<" gpu_ready_commands="<<gpu_ready<<" pixel_parity_checked="<<compare_pixels
                    <<" source_and_group_publications="<<Inputs::sNativeSceneSerial-before_serial<<" group_builds="<<groups.builds-before_groups.builds
                    <<" source_builds="<<groups.cpu_source_builds-before_groups.cpu_source_builds
                    <<" static_records="<<Inputs::ALScenePublicationNative::cohort_source_publications-before_records
                    <<" producer_resource_descriptors="<<Inputs::ALScenePublicationNative::resource_source_publications-before_descriptors
                    <<" source_getters="<<reads[0]-before_reads[0]<<" dependency_walks="<<reads[1]-before_reads[1]<<" image_getters="<<reads[2]-before_reads[2]
                    <<" compile_admitted="<<value.compile_admitted<<" compile_denied="<<value.compile_denied
                    <<" compile_duplicate_views="<<compilation.duplicate_views-before_compile.duplicate_views
                    <<" compile_logical_work_bytes="<<compilation.admitted_bytes-before_compile.admitted_bytes<<" same_view_replacements="<<value.replacements
                    <<" captured_cohort_groups="<<value.captured_groups<<" captured_cohort_draws="<<value.captured_draws<<" accepted_cohort_draws="<<value.accepted_draws
                    <<" raw_draws="<<value.raw_draws<<" bank_requested="<<value.requested<<" bank_draws="<<value.bound<<" fallback="<<value.fallback
                    <<" execute_calls="<<value.executions<<" bank_hits="<<value.hits<<" bank_builds="<<value.builds<<" bank_evictions="<<value.evictions
                    <<" bank_admission_requests="<<cache.admission_requests-before_cache.admission_requests
                    <<" bank_admission_denials="<<cache.admission_denials-before_cache.admission_denials
                    <<" bank_logical_argument_bytes="<<cache.charged_logical_argument_bytes-before_cache.charged_logical_argument_bytes
                    <<" page_cohort_requests="<<pages.cohort_requests-before_pages.cohort_requests<<" page_cohort_hits="<<value.page_hits
                    <<" page_cohort_builds="<<value.page_builds<<" page_cohort_failures="<<value.page_failures
                    <<" page_admission_requests="<<pages.admission_requests-before_pages.admission_requests
                    <<" page_admission_denials="<<pages.admission_denials-before_pages.admission_denials
                    <<" page_admitted_logical_bytes="<<pages.admitted_bytes-before_pages.admitted_bytes
                    <<" copied_page_bytes="<<pages.copied_page_bytes-before_pages.copied_page_bytes<<" copied_bank_bytes="<<pages.copied_bank_bytes-before_pages.copied_bank_bytes
                    <<" page_allocations="<<pages.page_allocations-before_pages.page_allocations<<" bank_allocations="<<pages.bank_allocations-before_pages.bank_allocations
                    <<" standalone_view_bytes="<<pages.view_bytes-before_pages.view_bytes<<" arena_views="<<pages.arena_views-before_pages.arena_views
                    <<" arena_view_cpu_copy_bytes="<<pages.arena_view_bytes-before_pages.arena_view_bytes
                    <<" native_frame_published_bytes="<<arm.data->stats().published_bytes-before_native
                    <<" live_icb_allocated_bytes="<<cache.live_icb_allocated_bytes<<" unique_retained_index_bytes="<<cache.unique_retained_index_bytes
                    <<" cached_banks="<<cache.cached_banks<<" live_banks="<<cache.live_banks<<" cached_cohorts="<<pages.cached_cohorts
                    <<" cached_cohort_rows="<<pages.cached_cohort_rows<<" cached_cohort_metadata_bytes="<<pages.cached_cohort_metadata_bytes
                    <<" cached_cohort_native_asset_bytes="<<pages.cached_cohort_native_asset_bytes<<" cached_cohort_native_asset_limit="<<pages.cached_cohort_native_asset_limit
                    <<" live_page_metadata_bytes="<<pages.live_metadata_bytes<<" all_scene_admitted="<<budget.scene_publications_this_frame-before_budget.scene_publications_this_frame
                    <<" all_scene_deferred="<<budget.deferred-before_budget.deferred<<" all_scene_logical_bytes="<<budget.scene_bytes_this_frame-before_budget.scene_bytes_this_frame
                    <<" raw_value_bytes="<<raw_bytes<<" reference_value_bytes="<<reference_bytes<<" order_value_bytes="<<order_bytes<<" cohort_value_bytes="<<cohort_bytes
                    <<" expanded_candidate_bytes="<<candidate_bytes<<" sourceview_regional_bytes="<<regional_bytes<<" cpu_description_pins="<<cpu_pins
                    <<" native_generation_pins="<<native_pins;
                for(std::size_t reason=0;reason<cache.failures.size();++reason)
                {
                    U64 fallback=0;for(U32 owner=0;owner<2;++owner)
                        fallback+=draws[owner].fallback_reasons[reason]-before_draw[owner].fallback_reasons[reason];
                    const auto* name=alMetalWorldSourceCommandFailureName(static_cast<ALMetalWorldSourceCommandFailure>(reason));
                    std::cout<<" bank_fallback_"<<name<<'='<<fallback
                        <<" bank_cache_failure_"<<name<<'='<<cache.failures[reason]-before_cache.failures[reason];
                }
                std::cout<<'\n';
            }
            for(U32 view=0;view<views;++view)
            {CHECK(query_counts[0][view]==query_counts[1][view]);CHECK(query_counts[0][view]==query_counts[2][view]);
             if(compare_pixels){CHECK(pixels[0][view]==pixels[1][view]);CHECK(pixels[0][view]==pixels[2][view]);}}
        }
        for(U32 mode=0;mode<3;++mode)
        {
            std::array<double,measured_frames> cpu{},encode{},complete{},gpu{};Sample totals;
            for(U32 sample=0;sample<measured_frames;++sample)
            {
                const auto& value=measured[mode][sample];cpu[sample]=value.cpu;encode[sample]=value.encode;complete[sample]=value.complete;gpu[sample]=value.gpu;
                totals.requested+=value.requested;totals.bound+=value.bound;totals.fallback+=value.fallback;totals.executions+=value.executions;
                totals.hits+=value.hits;totals.builds+=value.builds;totals.evictions+=value.evictions;
                totals.compile_admitted+=value.compile_admitted;totals.compile_denied+=value.compile_denied;
                totals.page_hits+=value.page_hits;totals.page_builds+=value.page_builds;totals.page_failures+=value.page_failures;
            }
            std::cout<<"NATIVE_MANY_COHORT_COST groups="<<group_count<<" sources_per_group="<<sources_per_group<<" draws_per_view="<<group_count*sources_per_group
                <<" legacy_groups="<<group_count/2<<" pbr_groups="<<group_count/2<<" views="<<views<<" queries_per_view="<<group_count
                <<" mode="<<(mode==0?"ordinary_native":mode==1?"direct_cohort":"gpu_cohort")<<" loading_frames="<<loading_frames
                <<" warmup_frames="<<warmup_frames<<" measured_frames="<<measured_frames
                <<" owner_cpu_median_ms="<<median(cpu)<<" owner_cpu_max_ms="<<*std::max_element(cpu.begin(),cpu.end())
                <<" capture_adopt_encode_reclaim_median_ms="<<median(encode)<<" capture_adopt_encode_reclaim_max_ms="<<*std::max_element(encode.begin(),encode.end())
                <<" completion_and_query_settle_median_ms="<<median(complete)<<" completion_and_query_settle_max_ms="<<*std::max_element(complete.begin(),complete.end())
                <<" gpu_sum_median_ms="<<median(gpu)<<" gpu_sum_max_ms="<<*std::max_element(gpu.begin(),gpu.end())
                <<" measured_bank_requested="<<totals.requested<<" measured_bank_draws="<<totals.bound<<" measured_fallback="<<totals.fallback
                <<" measured_execute_calls="<<totals.executions<<" measured_bank_hits="<<totals.hits<<" measured_bank_builds="<<totals.builds
                <<" measured_bank_evictions="<<totals.evictions<<" measured_page_hits="<<totals.page_hits<<" measured_page_builds="<<totals.page_builds
                <<" measured_page_failures="<<totals.page_failures<<" measured_compile_admitted="<<totals.compile_admitted<<" measured_compile_denied="<<totals.compile_denied
                <<" progressive_loading_eighths=1 loading_warmup_denials_retained=1 query_at_each_group_boundary=1 pass_order_legacy_then_pbr=1"
                <<" cache_limits_unchanged=1 all_banks_warm_assumption=0 independent_group_source_admission=1 independent_framedata_renderers_material_cache=1"
                <<" paired_logical_frame=1 native_epoch_once_per_arm_frame=1 shared_exact_library=1 distinct_view_matrix_payloads=1 typed_identity_camera_frontier=1"
                <<" dirty_raw_admission_before_seal=1 same_view_verified_replacement=1 cpu_and_gpu_scene_admission=1 synthetic_idle_budget_inputs=1"
                <<" shared_context_counters_observed_once=1 retained_native_index_footprint_not_new_allocation=1"
                <<" geometry_texture_drawinfo_creation_excluded=1 loading_means_first_native_publication_not_asset_download=1 whole_viewer_fps_claim=0\n";
        }
    }
}
