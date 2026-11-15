/**
 * @file alpipeline_gpu_cohort_cost_native.inl
 * @brief Independent ordinary/direct/GPU cohort complete-path comparisons.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after the actual production capture/producer fixtures.
 */

template<bool include_direct_bound,bool heavier_mesh=false,bool gated=false>
static void actualNativeCohortBankCosts()
{
    static_assert(!heavier_mesh||include_direct_bound);
    static_assert(!gated||include_direct_bound);
    constexpr U32 samples=12,warmups=2,modes=include_direct_bound?4:3,indices_per_source=heavier_mesh?768:3;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(10);
    const auto cpu_ns=[]
    {timespec value{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0);return U64(value.tv_sec)*1'000'000'000ULL+value.tv_nsec;};
    const auto median=[](auto values)
    {std::sort(values.begin(),values.end());return (values[5]+values[6])*.5;};
    struct Sample
    {
        double cpu=0,encode=0,complete=0,gpu=0;
        U64 source_publications=0,group_builds=0,source_builds=0,records=0,descriptors=0,getters=0,dependencies=0,images=0;
        U64 raw_draws=0,raw_bytes=0,reference_bytes=0,order_bytes=0,cohort_bytes=0,candidate_bytes=0,regional_bytes=0;
        U64 cpu_pins=0,native_pins=0,cohort_draws=0,replacements=0,native_frame_bytes=0,gpu_ready=0;
        U64 compile_admitted=0,compile_denied=0,compile_duplicates=0,compile_bytes=0,compile_stability_deferred=0;
        U64 accepted_cohort_draws=0,bank_requested=0,bank_draws=0,bank_fallback=0,execute_calls=0;
        U64 bank_hits=0,bank_builds=0,bank_admission_requests=0,bank_admission_denials=0,bank_argument_bytes=0;
        U64 page_cohort_requests=0,page_cohort_hits=0,page_cohort_builds=0,page_cohort_failures=0;
        U64 page_admission_requests=0,page_admission_denials=0,page_admitted_bytes=0,page_view_bytes=0;
        U64 page_arena_views=0,page_arena_bytes=0;
        U64 icb_live_bytes=0,index_retained_bytes=0,cached_banks=0,live_banks=0,page_live_bytes=0,cached_cohort_rows=0,cached_cohort_bytes=0;
        U64 cached_cohort_native_asset_bytes=0,cached_cohort_native_asset_limit=0;
        U64 all_scene_admitted=0,all_scene_deferred=0,all_scene_bytes=0;
        U64 direct_builds=0,direct_hits=0,direct_encoded_calls=0,direct_binding_bytes=0,direct_binding_limit=0,direct_binding_work_bytes=0;
        U64 direct_policy_bytes=0,direct_slot_allocations=0,direct_slot_hits=0,direct_view_bytes=0,live_direct_slots=0,live_direct_slot_bytes=0;
    };
    struct Arm
    {
        Inputs::LLPipeline pipeline;
        std::vector<Inputs::LLDrawInfo*> sources;
        std::array<U64,4> geometry{}; // vertices, indices, summed native wrapper extents; not unique physical bytes.
        std::vector<ALMetalWorldVisibilityQuery> queries;
        std::shared_ptr<ALNativePublication::State> group=std::make_shared<ALNativePublication::State>();
        std::shared_ptr<ALNativePublication::Observer> dependencies;
        std::shared_ptr<Inputs::ALSceneGroupPublicationNative::Layout> layout;
        ALNativeCohortCompilationAdmission compilation;
        ALNativeFrameBudget budget;
        std::unique_ptr<ALMetalWorldMaterialDataCache> material_cache;
        std::shared_ptr<ALMetalWorldFrameData> data;
        std::unique_ptr<ALMetalWorldGBufferRenderer> main,probe;
        U64 native_epoch=0;
        ~Arm(){group->invalidate();for(auto* source:sources)source->state->invalidate();}
    };
    struct MaterialCacheScope
    {
        Arm& arm;
        std::unique_ptr<ALMetalWorldMaterialDataCache> previous;
        explicit MaterialCacheScope(Arm& value):arm(value)
        {previous.swap(Inputs::ALScenePublicationNative::material_data_cache);arm.material_cache.swap(Inputs::ALScenePublicationNative::material_data_cache);}
        ~MaterialCacheScope()
        {arm.material_cache.swap(Inputs::ALScenePublicationNative::material_data_cache);previous.swap(Inputs::ALScenePublicationNative::material_data_cache);}
    };
    for(U32 family:{1u,2u})
    {
        MaterialGroupPublication f(family);Inputs::gSavedSettings.native_resources=false;Inputs::gSavedSettings.native_visibility=false;
        auto library_data=ALMetalWorldFrameData::create(*f.device,&f.error);INFO(f.error);REQUIRE(library_data);
        auto library=library_data->builtinLibrary(&f.error);INFO(f.error);REQUIRE(library);
        const auto counts=heavier_mesh?std::vector<U32>{512}:std::vector<U32>{1,4,16,512};
        for(U32 count:counts)
        {
            std::array<Arm,modes> arms;
            for(auto& arm:arms)
            {
                for(U32 source=0;source<count;++source)
                {
                    auto& info=f.materialDraw();
                    if constexpr(heavier_mesh)visibilityCostMesh(info,indices_per_source,false);
                    const auto vertex=info.mVertexBuffer->getMetalVertexBufferLease(),indices=info.mVertexBuffer->getMetalIndexBufferLease();
                    REQUIRE(vertex);REQUIRE(indices);REQUIRE(info.mVertexBuffer->getNumVerts()==indices_per_source);
                    REQUIRE(info.mVertexBuffer->getNumIndices()==indices_per_source);REQUIRE(info.mCount==indices_per_source);
                    arm.geometry[0]+=info.mVertexBuffer->getNumVerts();arm.geometry[1]+=info.mCount;
                    arm.geometry[2]+=vertex->length();arm.geometry[3]+=indices->length();
                    arm.sources.push_back(&info);
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
            for(U32 views:{1u,3u})for(U32 condition=0;condition<3;++condition)
            {
                CAPTURE(family,count,views,condition);
                for(auto& arm:arms)
                {arm.group->invalidate();arm.layout.reset();arm.dependencies.reset();for(auto* source:arm.sources)source->state->invalidate();}
                std::array<std::array<Sample,samples>,modes> costs{};
                for(U32 sample=0;sample<samples+warmups;++sample)
                {
                    ++Inputs::gFrameCount;
                    for(auto& arm:arms)
                    {ALNativeFrameBudget::FrameInputs input;input.frame=Inputs::gFrameCount;arm.budget.beginFrame(input);}
                    std::array<std::array<GroupPublication::Image,3>,modes> pixels;
                    std::array<std::array<std::array<std::uint64_t,2>,3>,modes> query_counts{};
                    for(U32 position=0;position<modes;++position)
                    {
                        INFO("Bounded cohort mesh fixture exceeded its ten-minute workload cap");
                        REQUIRE(std::chrono::steady_clock::now()<deadline);
                        const U32 mode=(sample+position)%modes;auto& arm=arms[mode];CAPTURE(mode,sample);
                        MaterialCacheScope material_scope(arm);
                        auto& capture=*arm.pipeline.mMetalWorldSimpleCapture;
                        const auto before_reads=f.reads();const auto before_serial=Inputs::sNativeSceneSerial;
                        const auto before_groups=Inputs::ALSceneGroupPublicationNative::statistics;
                        const auto before_compile=Inputs::ALSceneGroupCompilationNative::statistics;
                        const auto before_records=Inputs::ALScenePublicationNative::cohort_source_publications;
                        const auto before_descriptors=Inputs::ALScenePublicationNative::resource_source_publications;
                        const auto before_native=arm.data->stats().published_bytes;
                        const auto before_budget=arm.budget.snapshot();
                        const std::array<ALMetalWorldCohortStats,2> before_cohort{arm.main->worldCohortStats(),arm.probe->worldCohortStats()};
                        const std::array<ALMetalWorldSourceCommandDrawStats,2> before_draw{arm.main->worldSourceCommandStats(),arm.probe->worldSourceCommandStats()};
                        // Main/probe share this arm's context. Observe it once,
                        // never sum the same page/cache counters twice.
                        const auto before_cache=arm.main->worldSourceCommandStats().cache;
                        const auto before_pages=arm.main->worldSourceStats().pages;
                        std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                        std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                        std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> results;
                        Sample value;const auto begin=std::chrono::steady_clock::now();const auto begin_cpu=cpu_ns();
                        arm.data->beginFrame(++arm.native_epoch);
                        if(condition==0)
                        {
                            for(auto* source:arm.sources)source->state->invalidate();
                            if(mode){arm.group->invalidate();arm.layout.reset();arm.dependencies.reset();}
                        }
                        if(condition==2)
                        {
                            const auto tint=sample%2?LLColor4U(0,255,0,255):LLColor4U(0,0,255,255);
                            const std::array<LLColor4U,3> colors{tint,tint,tint};std::vector<LLColor4U> mesh_colors;
                            if constexpr(heavier_mesh)mesh_colors.assign(indices_per_source,tint);
                            for(U32 source=0;source<count;source+=16)
                            {auto& buffer=*arm.sources[source]->mVertexBuffer;
                             buffer.setColorData(heavier_mesh?mesh_colors.data():colors.data());buffer.unmapBuffer();}
                        }
                        for(U32 view=0;view<views;++view)
                        {
                            // Distinct copied per-view matrices; camera state
                            // is the fixture's explicit identity frontier.
                            REQUIRE(f.region.update(f.model,LLVector3(.01f*view,0.f,0.f)));
                            // PPD-228: GroupSources banks retain ordinary raw
                            // fallback; SourceRows is an independent request.
                            capture.clearDraws();capture.complete=true;f.configure(capture,false,false);
                            capture.world_resources=std::make_shared<Inputs::WorldResources>();
                            Inputs::LLCullResult cull;
                            struct Pending{U32 type,raw_begin,count;};
                            std::unordered_map<const void*,std::vector<Pending>> pending;
                            const auto current=[&]
                            {return Inputs::ALSceneGroupPublicationNative::currentMaterial(arm.group,f.region.snapshot(),f.flavor,
                                Inputs::sNativeSceneSources,Inputs::gFrameCount);};
                            auto packet=mode?current():std::shared_ptr<const NativeGroup>{};
                            if(packet)arm.compilation.publicationCompleted();
                            if(packet)REQUIRE(cull.pushNativeMaterialRange(f.type,packet,count,f.region.snapshot()));
                            else for(auto* source:arm.sources)cull.pushDrawInfo(f.type,source);
                            if(mode&&!packet)pending[arm.group.get()].push_back({f.type,0,count});
                            LLVertexBuffer::flushBuffers();
                            if(mode&&!packet)
                            {
                                arm.compilation.notePending(arm.budget.snapshot().frame);
                                struct Candidate
                                {
                                    const void* group;U32 type;
                                    std::shared_ptr<ALNativePublication::State> publisher;
                                    ALNativeCohortCompilationAdmission* admission;
                                    Inputs::ALScenePublicationNative::WorldFlavor flavor;
                                };
                                std::vector<Candidate> candidates;const auto frame=arm.budget.snapshot().frame;
                                if(arm.compilation.lastConsideredFrame()&&frame<=*arm.compilation.lastConsideredFrame())
                                    ++Inputs::ALSceneGroupCompilationNative::statistics.duplicate_views;
                                else
                                {
                                    ++Inputs::ALSceneGroupCompilationNative::statistics.candidates;
                                    candidates.push_back({arm.group.get(),f.type,arm.group,&arm.compilation,f.flavor});
                                }
                                std::sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b)
                                {return a.admission->priority(a.publisher->identity())<b.admission->priority(b.publisher->identity());});
                                auto sealed=candidates.empty()?std::shared_ptr<const NativeGroup>{}:current();
                                if(sealed)arm.compilation.publicationCompleted();
                                else if(!candidates.empty())sealed=Inputs::ALSceneGroupCompilationNative::compile(
                                    arm.compilation,arm.group,arm.budget,count,family,[&]
                                    {return Inputs::ALSceneGroupPublicationNative::sealMaterial(arm.group,arm.dependencies,arm.layout,
                                        f.model,f.region.snapshot(),arm.sources,f.flavor,Inputs::sNativeSceneSources,
                                        Inputs::sNativeSceneSerial,Inputs::gFrameCount);},gated);
                                if(sealed)
                                {
                                    const auto at=pending.find(arm.group.get());REQUIRE(at!=pending.end());REQUIRE(at->second.size()==1);
                                    const auto& span=at->second.front();REQUIRE(span.count==arm.sources.size());
                                    std::vector<Inputs::LLDrawInfo*> expected(arm.sources.begin(),arm.sources.end());
                                    REQUIRE(cull.replaceNativeMaterialRange(f.type,sealed,span.raw_begin,expected,f.region.snapshot()));
                                    ++value.replacements;
                                }
                            }
                            {CullScope scope(cull);f.capture(arm.pipeline);}INFO(f.error);REQUIRE(capture.complete);
                            REQUIRE(capture.logicalDrawCount()==count);REQUIRE(capture.flushDrawOrder());
                            value.raw_draws+=capture.batches.size();
                            value.raw_bytes+=capture.batches.size()*sizeof(ALMetalWorldSimpleBatch);
                            value.reference_bytes+=capture.source_references.size()*sizeof(ALMetalWorldSourceReference);
                            value.order_bytes+=capture.draw_order.size()*sizeof(ALMetalWorldDrawIndex);
                            value.cohort_bytes+=capture.source_cohorts.size()*sizeof(ALMetalWorldSourceCohortView);
                            value.cohort_draws+=capture.source_cohort_draw_count;
                            value.cpu_pins+=capture.source_cohorts.size()+(capture.cpu_sources?capture.cpu_sources->owners.size():0);
                            value.native_pins+=capture.source_cohorts.size()+capture.world_resources->owners.size();
                            if(capture.source_rows)
                            {value.candidate_bytes+=capture.source_rows->candidates.size()*sizeof(ALMetalWorldSourceView::Candidate);
                             value.regional_bytes+=capture.source_rows->regions.size()*sizeof(ALMetalWorldSourceRegion);}
                            CHECK_FALSE(capture.source_rows); // PPD-228: explicit diagnostic only.
                            if(!capture.source_cohorts.empty())
                            {CHECK(capture.source_references.empty());CHECK((!capture.source_rows||capture.source_rows->candidates.empty()));}
                            arm.queries.resize(2);
                            for(U32 query=0;query<2;++query)
                            {
                                auto& q=arm.queries[query];q={};std::memcpy(q.modelview_projection,f.model.getF32ptr(),sizeof(q.modelview_projection));
                                q.center[0]=.35f;q.center[1]=-.5f;q.center[2]=.75f;
                                q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.08f;q.batch_offset=query?count:count/2;q.fan=7;q.count_samples=1;
                            }
                            auto scene=f.scene(capture);scene.visibility_queries=arm.queries.data();scene.visibility_query_count=arm.queries.size();
                            scene.material_argument_buffers=false;scene.source_command_banks=mode>=2;
                            scene.source_direct_command_banks=mode==3;scene.source_command_minimum_run=2;
                            auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(capture.batches),std::move(capture.source_references),
                                std::move(capture.draw_order),std::move(capture.source_cohorts),std::move(arm.queries),
                                std::move(capture.cpu_sources),capture.world_resources,arm.native_epoch,&f.error);INFO(f.error);REQUIRE(inputs);scene={};
                            auto& renderer=view?*arm.probe:*arm.main;
                            REQUIRE(renderer.beginCommandBatch("Actual ordinary/direct/GPU cohort complete cost",&f.error));
                            targets[view]=renderer.renderOwnedInputs(*inputs,&f.error,&results[view]);INFO(f.error);REQUIRE(targets[view]);REQUIRE(results[view]);
                            completions[view]=renderer.commandBatchCompletion();
                            REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,capture.source_references,
                                capture.draw_order,capture.source_cohorts,arm.queries));
                            capture.clearDraws();capture.world_resources=std::make_shared<Inputs::WorldResources>();
                            cull.clear();packet.reset();REQUIRE(renderer.commitCommandBatch(&f.error));
                        }
                        const auto encoded=std::chrono::steady_clock::now();REQUIRE(f.device->waitUntilIdle(&f.error));
                        for(U32 view=0;view<views;++view)for(U32 query=0;query<2;++query)
                        {REQUIRE(results[view]->wait(query,query_counts[mode][view][query],&f.error));CHECK(query_counts[mode][view][query]>0);}
                        // Native completion handlers retire the metadata
                        // credits, independently of query publication. Include
                        // that actual retirement in this sample's settle cost.
                        const auto retire_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
                        while(arm.budget.snapshot().scene_publications_in_flight&&std::chrono::steady_clock::now()<retire_deadline)
                            std::this_thread::yield();
                        gGL.frameFences().release();const auto completed=std::chrono::steady_clock::now();
                        value.cpu=(cpu_ns()-begin_cpu)/1e6;value.encode=std::chrono::duration<double,std::milli>(encoded-begin).count();
                        value.complete=std::chrono::duration<double,std::milli>(completed-begin).count();
                        for(U32 view=0;view<views;++view)
                        {ALMetalCommandGPUTime timing;if(completions[view]&&completions[view]->gpuTime(timing)==ALMetalCommandTimingStatus::ready)
                            {value.gpu+=timing.elapsed_nanoseconds/1e6;++value.gpu_ready;}}
                        const auto reads=f.reads();const auto groups=Inputs::ALSceneGroupPublicationNative::statistics;
                        const auto compilation=Inputs::ALSceneGroupCompilationNative::statistics;const auto budget=arm.budget.snapshot();
                        value.source_publications=Inputs::sNativeSceneSerial-before_serial;value.group_builds=groups.builds-before_groups.builds;
                        value.source_builds=groups.cpu_source_builds-before_groups.cpu_source_builds;
                        value.records=Inputs::ALScenePublicationNative::cohort_source_publications-before_records;
                        value.descriptors=Inputs::ALScenePublicationNative::resource_source_publications-before_descriptors;
                        value.getters=reads[0]-before_reads[0];value.dependencies=reads[1]-before_reads[1];value.images=reads[2]-before_reads[2];
                        value.native_frame_bytes=arm.data->stats().published_bytes-before_native;
                        value.compile_admitted=compilation.admitted-before_compile.admitted;value.compile_denied=compilation.denied-before_compile.denied;
                        value.compile_duplicates=compilation.duplicate_views-before_compile.duplicate_views;value.compile_bytes=compilation.admitted_bytes-before_compile.admitted_bytes;
                        value.compile_stability_deferred=compilation.stability_deferred-before_compile.stability_deferred;
                        if(gated&&value.compile_stability_deferred&&!value.compile_admitted&&!value.compile_denied)
                        {CHECK(value.compile_bytes==0);CHECK(compilation.requested_bytes==before_compile.requested_bytes);}
                        const std::array<ALMetalWorldCohortStats,2> cohorts{arm.main->worldCohortStats(),arm.probe->worldCohortStats()};
                        const std::array<ALMetalWorldSourceCommandDrawStats,2> draws{arm.main->worldSourceCommandStats(),arm.probe->worldSourceCommandStats()};
                        for(U32 owner=0;owner<2;++owner)
                        {value.accepted_cohort_draws+=cohorts[owner].draws-before_cohort[owner].draws;
                         value.bank_requested+=draws[owner].requested_draws-before_draw[owner].requested_draws;
                         value.bank_draws+=draws[owner].bank_draws-before_draw[owner].bank_draws;
                         value.bank_fallback+=draws[owner].fallback_draws-before_draw[owner].fallback_draws;
                         value.execute_calls+=draws[owner].execute_calls-before_draw[owner].execute_calls;}
                        const auto cache=arm.main->worldSourceCommandStats().cache;const auto pages=arm.main->worldSourceStats().pages;
                        value.bank_hits=cache.hits-before_cache.hits;value.bank_builds=cache.builds-before_cache.builds;
                        value.bank_admission_requests=cache.admission_requests-before_cache.admission_requests;
                        value.bank_admission_denials=cache.admission_denials-before_cache.admission_denials;
                        value.bank_argument_bytes=cache.charged_logical_argument_bytes-before_cache.charged_logical_argument_bytes;
                        value.icb_live_bytes=cache.live_icb_allocated_bytes;value.index_retained_bytes=cache.unique_retained_index_bytes;
                        value.cached_banks=cache.cached_banks;value.live_banks=cache.live_banks;
                        value.page_cohort_requests=pages.cohort_requests-before_pages.cohort_requests;value.page_cohort_hits=pages.cohort_hits-before_pages.cohort_hits;
                        value.page_cohort_builds=pages.cohort_builds-before_pages.cohort_builds;value.page_cohort_failures=pages.cohort_failures-before_pages.cohort_failures;
                        value.page_admission_requests=pages.admission_requests-before_pages.admission_requests;
                        value.page_admission_denials=pages.admission_denials-before_pages.admission_denials;
                        value.page_admitted_bytes=pages.admitted_bytes-before_pages.admitted_bytes;value.page_view_bytes=pages.view_bytes-before_pages.view_bytes;
                        value.page_arena_views=pages.arena_views-before_pages.arena_views;value.page_arena_bytes=pages.arena_view_bytes-before_pages.arena_view_bytes;
                        value.page_live_bytes=pages.live_metadata_bytes;value.cached_cohort_rows=pages.cached_cohort_rows;
                        value.cached_cohort_bytes=pages.cached_cohort_metadata_bytes;
                        value.cached_cohort_native_asset_bytes=pages.cached_cohort_native_asset_bytes;
                        value.cached_cohort_native_asset_limit=pages.cached_cohort_native_asset_limit;
                        value.all_scene_admitted=budget.scene_publications_this_frame-before_budget.scene_publications_this_frame;
                        value.all_scene_deferred=budget.deferred-before_budget.deferred;value.all_scene_bytes=budget.scene_bytes_this_frame-before_budget.scene_bytes_this_frame;
                        value.direct_builds=cache.direct_builds-before_cache.direct_builds;value.direct_hits=cache.direct_hits-before_cache.direct_hits;
                        value.direct_encoded_calls=cache.direct_encoded_calls-before_cache.direct_encoded_calls;value.direct_binding_bytes=cache.unique_retained_binding_bytes;
                        value.direct_binding_limit=cache.retained_binding_limit_bytes;value.direct_binding_work_bytes=cache.charged_logical_binding_bytes-before_cache.charged_logical_binding_bytes;
                        value.direct_policy_bytes=pages.direct_policy_bytes-before_pages.direct_policy_bytes;
                        value.direct_slot_allocations=pages.direct_slot_allocations-before_pages.direct_slot_allocations;
                        value.direct_slot_hits=pages.direct_slot_hits-before_pages.direct_slot_hits;value.direct_view_bytes=pages.direct_view_bytes-before_pages.direct_view_bytes;
                        value.live_direct_slots=pages.live_direct_slots;value.live_direct_slot_bytes=pages.live_direct_slot_bytes;
                        CHECK(value.accepted_cohort_draws==value.cohort_draws);
                        CHECK(value.bank_draws+value.bank_fallback==value.bank_requested);
                        if(mode==3&&value.bank_draws)CHECK(value.direct_builds+value.direct_hits>0);
                        CHECK(budget.scene_publications_in_flight==0);CHECK(budget.scene_bytes_in_flight==0);
                        if(mode<2){CHECK(value.bank_requested==0);CHECK(value.bank_draws==0);CHECK(value.execute_calls==0);}
                        if(mode>=2&&value.cohort_draws==U64(count)*views)
                        {
                            CHECK(value.bank_requested==U64(count)*views);
                            if(count==1){CHECK(value.bank_draws==0);CHECK(value.bank_fallback==views);CHECK(value.execute_calls==0);}
                            else if(!value.page_admission_denials&&!value.bank_admission_denials)
                            {CHECK(value.bank_draws==U64(count)*views);CHECK(value.execute_calls==2*views);
                             if(mode==2){CHECK(value.page_arena_views==views);CHECK(value.page_arena_bytes==U64(288)*views);}}
                        }
                        if(sample>=warmups&&mode&&condition==1&&value.cohort_draws==U64(count)*views)
                        {CHECK(value.getters==0);CHECK(value.dependencies==0);CHECK(value.images==0);CHECK(value.compile_admitted==0);}
                        if(gated&&sample>=warmups&&mode&&condition==1)
                        {
                            CHECK(value.cohort_draws==U64(count)*views);CHECK(value.compile_stability_deferred==0);
                            CHECK(value.compile_admitted==0);CHECK(value.compile_denied==0);CHECK(value.compile_bytes==0);
                            CHECK(compilation.requested_bytes==before_compile.requested_bytes);
                        }
                        if(sample>=warmups)costs[mode][sample-warmups]=value;
                        if(sample==warmups-1||heavier_mesh)for(U32 view=0;view<views;++view)
                        {
                            pixels[mode][view]=f.read(*targets[view]);
                            if constexpr(heavier_mesh)CHECK(std::any_of(pixels[mode][view].depth.begin(),pixels[mode][view].depth.end(),[](F32 depth){return depth<1.f;}));
                        }
                        std::cout<<(gated?"NATIVE_STABLE_DIRECT_BOUND_COHORT_RAW family=":heavier_mesh?"NATIVE_DIRECT_BOUND_COHORT_MESH_RAW family=":include_direct_bound?"NATIVE_DIRECT_BOUND_COHORT_RAW family=":"NATIVE_GPU_COHORT_RAW family=")<<(family==1?"legacy":"pbr")<<" draws_per_view="<<count<<" views="<<views
                            <<" condition="<<condition<<" mode="<<mode<<" sample="<<sample<<" warmup="<<(sample<warmups)
                            <<" vertices_per_source="<<indices_per_source<<" indices_per_source="<<indices_per_source
                            <<" candidate_vertices="<<arm.geometry[0]<<" candidate_indices="<<arm.geometry[1]
                            <<" vertex_buffer_wrapper_extent_bytes="<<arm.geometry[2]<<" index_buffer_wrapper_extent_bytes="<<arm.geometry[3]
                            <<" position_payload_bytes="<<arm.geometry[0]*16<<" index_payload_bytes="<<arm.geometry[1]*sizeof(U16)
                            <<" owner_cpu_ms="<<value.cpu<<" capture_adopt_encode_reclaim_ms="<<value.encode<<" completion_and_query_settle_ms="<<value.complete
                            <<" gpu_sum_ms="<<value.gpu<<" gpu_ready_commands="<<value.gpu_ready
                            <<" source_and_group_publications="<<value.source_publications<<" group_builds="<<value.group_builds<<" source_builds="<<value.source_builds
                            <<" static_records="<<value.records<<" producer_resource_descriptors="<<value.descriptors
                            <<" source_getters="<<value.getters<<" dependency_walks="<<value.dependencies<<" image_getters="<<value.images
                            <<" compile_admitted="<<value.compile_admitted<<" compile_denied="<<value.compile_denied<<" compile_stability_deferred="<<value.compile_stability_deferred
                            <<" require_stable_revision="<<(gated&&mode!=0)<<" compile_duplicate_views="<<value.compile_duplicates
                            <<" compile_logical_work_bytes="<<value.compile_bytes<<" same_view_replacements="<<value.replacements
                            <<" required_raw_draws="<<value.raw_draws<<" captured_cohort_draws="<<value.cohort_draws<<" accepted_cohort_draws="<<value.accepted_cohort_draws
                            <<" bank_requested="<<value.bank_requested<<" bank_draws="<<value.bank_draws<<" fallback="<<value.bank_fallback<<" execute_calls="<<value.execute_calls
                            <<" bank_hits="<<value.bank_hits<<" bank_builds="<<value.bank_builds<<" bank_admission_requests="<<value.bank_admission_requests
                            <<" bank_admission_denials="<<value.bank_admission_denials<<" bank_logical_argument_bytes="<<value.bank_argument_bytes
                            <<" page_cohort_requests="<<value.page_cohort_requests<<" page_cohort_hits="<<value.page_cohort_hits
                            <<" page_cohort_builds="<<value.page_cohort_builds<<" page_cohort_failures="<<value.page_cohort_failures
                            <<" page_admission_requests="<<value.page_admission_requests<<" page_admission_denials="<<value.page_admission_denials
                            <<" page_admitted_logical_bytes="<<value.page_admitted_bytes<<" page_view_bytes="<<value.page_view_bytes
                            <<" page_arena_views="<<value.page_arena_views<<" page_arena_cpu_copy_bytes="<<value.page_arena_bytes
                            <<" live_icb_allocated_bytes="<<value.icb_live_bytes<<" unique_retained_index_bytes="<<value.index_retained_bytes
                            <<" cached_banks="<<value.cached_banks<<" live_banks="<<value.live_banks<<" live_page_metadata_bytes="<<value.page_live_bytes
                            <<" cached_compositions="<<pages.cached_compositions
                            <<" cached_cohort_rows="<<value.cached_cohort_rows<<" cached_cohort_cpu_metadata_bytes="<<value.cached_cohort_bytes
                            <<" cached_cohort_native_asset_bytes="<<value.cached_cohort_native_asset_bytes
                            <<" cached_cohort_native_asset_limit="<<value.cached_cohort_native_asset_limit
                            <<" all_scene_admitted="<<value.all_scene_admitted<<" all_scene_deferred="<<value.all_scene_deferred<<" all_scene_logical_bytes="<<value.all_scene_bytes
                            <<" raw_value_bytes="<<value.raw_bytes<<" reference_value_bytes="<<value.reference_bytes<<" order_value_bytes="<<value.order_bytes
                            <<" cohort_value_bytes="<<value.cohort_bytes<<" expanded_candidate_bytes="<<value.candidate_bytes<<" sourceview_regional_bytes="<<value.regional_bytes
                            <<" native_frame_bytes="<<value.native_frame_bytes<<" cpu_description_pins="<<value.cpu_pins<<" native_generation_pins="<<value.native_pins
                            <<" direct_bound_mode="<<(mode==3)<<" direct_bank_builds="<<value.direct_builds<<" direct_bank_hits="<<value.direct_hits
                            <<" direct_encoded_calls="<<value.direct_encoded_calls<<" unique_retained_binding_bytes="<<value.direct_binding_bytes
                            <<" retained_binding_limit_bytes="<<value.direct_binding_limit<<" direct_charged_logical_binding_bytes="<<value.direct_binding_work_bytes
                            <<" direct_policy_published_bytes="<<value.direct_policy_bytes<<" direct_slot_allocations="<<value.direct_slot_allocations
                            <<" direct_slot_hits="<<value.direct_slot_hits<<" direct_view_copy_bytes="<<value.direct_view_bytes
                            <<" live_direct_slots="<<value.live_direct_slots<<" live_direct_slot_bytes="<<value.live_direct_slot_bytes<<'\n';
                    }
                    for(U32 view=0;view<views;++view)
                        for(U32 mode=1;mode<modes;++mode)
                        {CHECK(query_counts[0][view]==query_counts[mode][view]);
                         if(sample==warmups-1||heavier_mesh)CHECK(pixels[0][view]==pixels[mode][view]);}
                }
                for(U32 mode=0;mode<modes;++mode)
                {
                    std::array<double,samples> cpu{},encode{},complete{},gpu{};Sample direct_totals;
                    for(U32 sample=0;sample<samples;++sample)
                    {
                        const auto& value=costs[mode][sample];cpu[sample]=value.cpu;encode[sample]=value.encode;complete[sample]=value.complete;gpu[sample]=value.gpu;
                        direct_totals.raw_draws+=value.raw_draws;direct_totals.cohort_draws+=value.cohort_draws;
                        direct_totals.compile_admitted+=value.compile_admitted;direct_totals.compile_denied+=value.compile_denied;
                        direct_totals.compile_stability_deferred+=value.compile_stability_deferred;
                        direct_totals.direct_builds+=value.direct_builds;direct_totals.direct_hits+=value.direct_hits;direct_totals.direct_encoded_calls+=value.direct_encoded_calls;
                        direct_totals.direct_slot_allocations+=value.direct_slot_allocations;direct_totals.direct_slot_hits+=value.direct_slot_hits;
                        direct_totals.direct_binding_work_bytes+=value.direct_binding_work_bytes;
                        direct_totals.direct_policy_bytes+=value.direct_policy_bytes;direct_totals.direct_view_bytes+=value.direct_view_bytes;
                        direct_totals.direct_binding_bytes=std::max(direct_totals.direct_binding_bytes,value.direct_binding_bytes);
                        direct_totals.live_direct_slot_bytes=std::max(direct_totals.live_direct_slot_bytes,value.live_direct_slot_bytes);
                    }
                    std::cout<<(gated?"NATIVE_STABLE_DIRECT_BOUND_COHORT_COST family=":heavier_mesh?"NATIVE_DIRECT_BOUND_COHORT_MESH_COST family=":include_direct_bound?"NATIVE_DIRECT_BOUND_COHORT_COST family=":"NATIVE_GPU_COHORT_COST family=")<<(family==1?"legacy":"pbr")<<" draws_per_view="<<count<<" views="<<views
                        <<" vertices_per_source="<<indices_per_source<<" indices_per_source="<<indices_per_source
                        <<" vertices_per_view="<<arms[mode].geometry[0]<<" indices_per_view="<<arms[mode].geometry[1]
                        <<" vertex_buffer_wrapper_extent_bytes="<<arms[mode].geometry[2]<<" index_buffer_wrapper_extent_bytes="<<arms[mode].geometry[3]
                        <<" position_payload_bytes="<<arms[mode].geometry[0]*16<<" index_payload_bytes="<<arms[mode].geometry[1]*sizeof(U16)
                        <<" condition="<<(condition==0?"cold":condition==1?"warm":"churn")<<" mode="<<(mode==0?"ordinary_native":mode==1?"direct_cohort":mode==2?"gpu_cohort":"direct_bound_cohort")
                        <<" samples="<<samples<<" warmups="<<warmups<<" owner_cpu_median_ms="<<median(cpu)<<" owner_cpu_max_ms="<<*std::max_element(cpu.begin(),cpu.end())
                        <<" capture_adopt_encode_reclaim_median_ms="<<median(encode)<<" capture_adopt_encode_reclaim_max_ms="<<*std::max_element(encode.begin(),encode.end())
                        <<" completion_and_query_settle_median_ms="<<median(complete)<<" completion_and_query_settle_max_ms="<<*std::max_element(complete.begin(),complete.end())
                        <<" gpu_sum_median_ms="<<median(gpu)<<" gpu_sum_max_ms="<<*std::max_element(gpu.begin(),gpu.end())
                        <<" measured_required_raw_draws="<<direct_totals.raw_draws<<" measured_captured_cohort_draws="<<direct_totals.cohort_draws
                        <<" measured_compile_admitted="<<direct_totals.compile_admitted<<" measured_compile_denied="<<direct_totals.compile_denied
                        <<" measured_compile_stability_deferred="<<direct_totals.compile_stability_deferred<<" require_stable_revision="<<(gated&&mode!=0)
                        <<" measured_direct_bank_builds="<<direct_totals.direct_builds<<" measured_direct_bank_hits="<<direct_totals.direct_hits
                        <<" measured_direct_encoded_calls="<<direct_totals.direct_encoded_calls<<" measured_direct_slot_allocations="<<direct_totals.direct_slot_allocations
                        <<" measured_direct_charged_logical_binding_bytes="<<direct_totals.direct_binding_work_bytes
                        <<" measured_direct_slot_hits="<<direct_totals.direct_slot_hits<<" measured_direct_policy_bytes="<<direct_totals.direct_policy_bytes
                        <<" measured_direct_view_copy_bytes="<<direct_totals.direct_view_bytes<<" max_unique_retained_binding_bytes="<<direct_totals.direct_binding_bytes
                        <<" max_live_direct_slot_bytes="<<direct_totals.live_direct_slot_bytes
                        <<" page_bank_hit_unit=ordered_composition implicit_scalar_source_rows=0"
                        <<" update_stride=16 independent_group_source_admission=1 independent_framedata_renderers_material_cache=1 paired_logical_frame=1"
                        <<" native_epoch_once_per_arm_frame=1 shared_exact_library=1 distinct_view_matrix_payloads=1 typed_identity_camera_frontier=1"
                        <<" dirty_raw_admission_before_seal=1 same_view_verified_replacement=1 cpu_and_gpu_scene_admission=1 synthetic_idle_budget_inputs=1"
                        <<" count1_short_run_fallback_retained=1 denied_frames_retained=1 shared_context_counters_observed_once=1"
                        <<" retained_native_index_footprint_not_new_allocation=1 binding_slot_footprints_overlap_do_not_sum=1"
                        <<" fixture_indices_per_source="<<indices_per_source<<" unique_vertex_indices_per_source="<<indices_per_source
                        <<" same_local_mesh_aabb=1 repeated_three_vertex_indices=0"
                        <<" full_vertex_color_mutation="<<heavier_mesh<<" wrapper_extents_are_not_physical_unique_bytes=1"
                        <<" geometry_texture_drawinfo_creation_excluded=1 whole_viewer_fps_claim=0\n";
                }
            }
        }
    }
}

TEST_CASE("Actual retained cohorts compare ordinary direct and native bank complete costs",
          "[almetal][.native-gpu-cohort-cost]")
{actualNativeCohortBankCosts<false>();}

TEST_CASE("Actual retained cohorts compare ordinary direct pointer-bank and direct-bound bank complete costs",
          "[almetal][.native-direct-bound-cohort-cost]")
{actualNativeCohortBankCosts<true>();}

TEST_CASE("Actual heavy mesh retained cohorts compare ordinary direct pointer-bank and direct-bound complete costs",
          "[almetal][.native-direct-bound-cohort-mesh-cost]")
{actualNativeCohortBankCosts<true,true>();}

TEST_CASE("Actual stable retained cohorts compare ordinary direct pointer-bank and direct-bound complete costs",
          "[almetal][.native-stable-direct-bound-cohort-cost]")
{actualNativeCohortBankCosts<true,false,true>();}
