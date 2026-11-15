/**
 * @file alpipeline_cohort_visibility_cost_native.inl
 * @brief Actual ordinary/direct/retained-visibility complete-path costs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after alpipeline_buffer_publication_metal_test.cpp's fixtures.
 */

static void visibilityCostMesh(Inputs::LLDrawInfo& info,U32 count,bool outside)
{
    const U32 cells=count==768?16:32;REQUIRE(count==cells*cells*3);
    const U32 mask=info.mVertexBuffer->getTypeMask();LLPointer<LLVertexBuffer> mesh=new LLVertexBuffer(mask);
    REQUIRE(mesh->allocateBuffer(count,count));std::memset(mesh->getMappedData(),0,mesh->getSize());
    std::vector<LLVector4a> positions(count),normals(count),tangents(count);
    std::vector<LLVector2> uv(count);std::vector<LLColor4U> colors(count,LLColor4U(255,0,0,255));std::vector<U16> indices(count);
    const auto slot=std::bit_cast<float>((info.mMaterialSlotList.size()>1||info.mGLTFMaterialList.size()>1)?1u:0u);
    for(U32 row=0;row<cells;++row)for(U32 column=0;column<cells;++column)
    {
        // Each cell owns three different positions and three fresh indices.
        // Gaps between adjacent cells prevent even boundary positions being
        // shared; the whole mesh retains the original local AABB.
        // Offset interior grid lines from the16px fixture's sample centers;
        // an exact32x32 grid with gaps would place every pixel in a gap.
        const F32 phase=cells==16?.25f:-.25f;
        const F32 x0=column?F32(column+phase)/cells:0.f,x1=column+1==cells?1.f:F32(column+phase+.9f)/cells;
        const F32 y0=row?F32(row+phase)/cells:0.f,y1=row+1==cells?1.f:F32(row+phase+.9f)/cells;
        const std::array<LLVector2,3> corners{LLVector2(x0,y0),LLVector2(x1,y0),LLVector2(x0,y1)};
        for(U32 corner=0;corner<3;++corner)
        {
            const U32 index=(row*cells+column)*3+corner;
            const F32 x=corners[corner].mV[0]==1.f?(outside?3.9f:-.1f):(outside?3.f:-1.f)+.9f*corners[corner].mV[0];
            positions[index].set(x,-1.f+2.f*corners[corner].mV[1],0.f,slot);
            normals[index].set(0,0,1,0);tangents[index].set(1,0,0,1);uv[index]=corners[corner];indices[index]=U16(index);
        }
    }
    mesh->setPositionData(positions.data());mesh->setNormalData(normals.data());mesh->setTexCoord0Data(uv.data());
    mesh->setColorData(colors.data());if(mask&LLVertexBuffer::MAP_TANGENT)mesh->setTangentData(tangents.data());
    if(mask&LLVertexBuffer::MAP_TEXCOORD1)mesh->setTexCoord1Data(uv.data());
    mesh->setIndexData(indices.data());mesh->unmapBuffer();
    REQUIRE(mesh->hasPublishedMetalVertexBuffer());REQUIRE(mesh->hasPublishedMetalIndexBuffer());
    info.mVertexBuffer=mesh;info.mStart=0;info.mEnd=U16(count-1);info.mOffset=0;info.mCount=count;
}

static void actualRetainedCohortVisibilityCosts(bool heavier_mesh)
{
    constexpr U32 loading_frames=8,measured_frames=12;
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
        U64 visibility_views=0,visibility_groups=0,visibility_candidates=0,visibility_builds=0,visibility_hits=0,visibility_upload_bytes=0,expanded_visibility_bytes=0,captured_visibility_bytes=0,visibility_allocated_bytes=0;
        U64 visibility_admission_requests=0,visibility_admission_denials=0,visibility_admitted_bytes=0,visibility_fallbacks=0;
    };
    struct Group
    {
        U32 family=0,type=0;
        Inputs::ALScenePublicationNative::WorldFlavor flavor;
        std::vector<Inputs::LLDrawInfo*> sources;
        std::array<U64,4> geometry{}; // vertices, indices, summed native wrapper extents (not new allocation).
        std::shared_ptr<ALNativePublication::State> state=std::make_shared<ALNativePublication::State>();
        std::shared_ptr<ALNativePublication::Observer> dependencies;
        std::shared_ptr<Inputs::ALSceneGroupPublicationNative::Layout> layout;
        ALNativeCohortCompilationAdmission compilation;
    };
    struct Arm
    {
        Inputs::LLPipeline pipeline;
        std::vector<std::unique_ptr<Group>> groups;
        std::vector<std::array<U64,4>> geometry_prefix{{0,0,0,0}};
        std::vector<ALMetalWorldVisibilityQuery> queries;
        ALNativeFrameBudget budget;
        std::shared_ptr<ALMetalWorldVisibility> visibility;
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
        std::shared_ptr<ALMetalWorldVisibility> previous_visibility;
        ALMetalDevice* previous_device=nullptr;
        explicit MaterialCacheScope(Arm& value):arm(value)
        {previous.swap(Inputs::ALScenePublicationNative::material_data_cache);arm.material_cache.swap(Inputs::ALScenePublicationNative::material_data_cache);
         previous_visibility.swap(Inputs::sNativeSceneVisibility);arm.visibility.swap(Inputs::sNativeSceneVisibility);
         previous_device=Inputs::sNativeSceneVisibilityDevice;Inputs::sNativeSceneVisibilityDevice=LLImageGL::getMetalDevice();}
        ~MaterialCacheScope()
        {arm.material_cache.swap(Inputs::ALScenePublicationNative::material_data_cache);previous.swap(Inputs::ALScenePublicationNative::material_data_cache);
         arm.visibility.swap(Inputs::sNativeSceneVisibility);previous_visibility.swap(Inputs::sNativeSceneVisibility);
         Inputs::sNativeSceneVisibilityDevice=previous_device;}
    };
    const std::vector<std::pair<U32,U32>> shapes=heavier_mesh
        ?std::vector<std::pair<U32,U32>>{{1,512},{64,2},{256,4}}
        :std::vector<std::pair<U32,U32>>{{1,1},{1,4},{1,16},{1,512},{64,2},{256,4}};
    for(const auto shape:shapes)
    for(U32 first_family:{0u,1u,2u})for(U32 views:{1u,3u})for(U32 condition:{0u,1u,2u})for(U32 outside_percent:{0u,50u})
    {
        const auto group_count=shape.first,sources_per_group=shape.second;
        if(heavier_mesh && first_family==0)continue;
        if(group_count>1 && first_family!=1)continue;
        const U32 indices_per_source=heavier_mesh?(group_count==64?3072:768):3;
        const U32 warmup_frames=condition==0||group_count==1?2u:64u;
        // PPD-225: integer source counts cannot represent half of one draw.
        // Preserve the requested case and report its actual geometry fraction.
        const U32 actual_outside_per_group=outside_percent?sources_per_group/2:0;
        const double effective_outside_percent=100.0*actual_outside_per_group/sources_per_group;
        CAPTURE(group_count,sources_per_group,views,first_family,condition,outside_percent);
        MaterialGroupPublication f(1);
        Inputs::gSavedSettings.native_resources=false;Inputs::gSavedSettings.native_visibility=false;
        std::array<Inputs::ALScenePublicationNative::WorldFlavor,3> flavors;
        for(U32 family=0;family<=2;++family)
        {flavors[family].family=static_cast<Inputs::ALScenePublicationNative::WorldFamily>(family);
         flavors[family].native_materials=true;flavors[family].native_resources=false;}
        const auto select_family=[&](U32 family)
        {f.flavor=flavors[family];f.type=family==0?Inputs::LLRenderPass::PASS_SIMPLE:family==1?Inputs::LLRenderPass::PASS_MATERIAL:Inputs::LLRenderPass::PASS_GLTF_PBR;};
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
                auto group=std::make_unique<Group>();group->family=group_count==1?first_family:1+index%2;select_family(group->family);
                group->type=f.type;group->flavor=f.flavor;group->flavor.cohort_bounds=&arm==&arms[2];
                for(U32 source=0;source<sources_per_group;++source)
                {
                    auto& info=f.materialDraw(source%2);
                    // Partial-group source rejection is beyond the coarse CPU
                    // group frustum. Asset construction/upload stays excluded.
                    if(heavier_mesh)visibilityCostMesh(info,indices_per_source,outside_percent&&source%2);
                    else if(outside_percent && source%2)
                    {
                        std::array<LLVector4a,3> positions;
                        const auto slot=std::bit_cast<float>(source%2);
                        positions[0].set(3.f,-1.f,0.f,slot);positions[1].set(3.9f,-1.f,0.f,slot);positions[2].set(3.f,1.f,0.f,slot);
                        info.mVertexBuffer->setPositionData(positions.data());info.mVertexBuffer->unmapBuffer();
                    }
                    const auto vertex=info.mVertexBuffer->getMetalVertexBufferLease(),indices=info.mVertexBuffer->getMetalIndexBufferLease();
                    REQUIRE(vertex);REQUIRE(indices);REQUIRE(info.mVertexBuffer->getNumVerts()==indices_per_source);REQUIRE(info.mCount==indices_per_source);
                    group->geometry[0]+=info.mVertexBuffer->getNumVerts();group->geometry[1]+=info.mCount;
                    group->geometry[2]+=vertex->length();group->geometry[3]+=indices->length();
                    group->sources.push_back(&info);
                }
                auto prefix=arm.geometry_prefix.back();for(U32 field=0;field<prefix.size();++field)prefix[field]+=group->geometry[field];
                arm.geometry_prefix.push_back(prefix);
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
            const U32 visible_groups=loading?std::max(1u,(frame+1)*group_count/loading_frames):group_count;
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
                MaterialCacheScope material_scope(arm);Inputs::gSavedSettings.native_visibility=mode==2;Inputs::gSavedSettings.native_groups=mode!=0;
                const auto before_visibility=Inputs::sNativeSceneVisibility?Inputs::sNativeSceneVisibility->stats():ALMetalWorldVisibilityStats{};
                auto& capture=*arm.pipeline.mMetalWorldSimpleCapture;
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
                if(condition==0)
                    for(auto& group:arm.groups)
                    {
                        group->state->invalidate();group->layout.reset();group->dependencies.reset();
                        for(auto* source:group->sources)source->state->invalidate();
                    }
                if(condition==2 && frame>=loading_frames)
                {
                    const auto tint=frame%2?LLColor4U(0,255,0,255):LLColor4U(0,0,255,255);
                    const std::array<LLColor4U,3> colors{tint,tint,tint};std::vector<LLColor4U> mesh_colors;
                    if(heavier_mesh)mesh_colors.assign(indices_per_source,tint);U32 source_index=0;
                    for(U32 index=0;index<visible_groups;++index)for(auto* source:arm.groups[index]->sources)
                    {if((source_index++%16)==0){source->mVertexBuffer->setColorData(heavier_mesh?mesh_colors.data():colors.data());source->mVertexBuffer->unmapBuffer();}}
                }
                for(U32 view=0;view<views;++view)
                {
                    // Preserve the typed fixture's identity-camera frontier,
                    // with distinct actual copied matrix payloads per view.
                    REQUIRE(f.region.update(f.model,LLVector3(.01f*view,0.f,0.f)));
                    capture.clearDraws();capture.complete=true;select_family(1);f.configure(capture,false,false);capture.world_visibility_members.clear();
                    capture.world_resources=std::make_shared<Inputs::WorldResources>();Inputs::LLCullResult cull;
                    struct PendingGroupRange{U32 type,raw_begin,count;};
                    std::unordered_map<Group*,std::vector<PendingGroupRange>> pending_group_ranges;
                    const auto current=[&](Group& group,const ALNativeRegionModel::Snapshot& region)
                    {return group.family?Inputs::ALSceneGroupPublicationNative::currentMaterial(group.state,region,group.flavor,
                        Inputs::sNativeSceneSources,Inputs::gFrameCount):Inputs::ALSceneGroupPublicationNative::current(group.state,region,
                        Inputs::sNativeSceneSources,Inputs::gFrameCount);};
                    // Match the real deferred pool order: all Legacy groups,
                    // then PBR groups. Loading admits equal numbers of both.
                    for(U32 family=0;family<=2;++family)for(U32 index=0;index<visible_groups;++index)
                    {
                        auto& group=*arm.groups[index];if(group.family!=family)continue;
                        const auto region=f.region.snapshot();auto packet=mode?current(group,region):std::shared_ptr<const NativeGroup>{};
                        if(packet)group.compilation.publicationCompleted();
                        if(packet)REQUIRE((group.family?cull.pushNativeMaterialRange(group.type,packet,sources_per_group,region):cull.pushNativeSimpleRange(packet,sources_per_group,region)));
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
                            REQUIRE((family?cull.replaceNativeMaterialRange(type,packet,span->raw_begin,expected,region):cull.replaceNativeSimpleRange(packet,span->raw_begin,expected,region)));
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
                            auto packet=group.family?Inputs::ALSceneGroupPublicationNative::currentMaterial(candidate.publisher,region,
                                candidate.flavor,Inputs::sNativeSceneSources,Inputs::gFrameCount):Inputs::ALSceneGroupPublicationNative::current(candidate.publisher,region,
                                Inputs::sNativeSceneSources,Inputs::gFrameCount);
                            if(packet&&packet->cohort&&packet->cohort->size()==group.sources.size()&&
                               packet->cohort->family()==static_cast<U32>(candidate.flavor.family)&&
                               packet->cohort->regionIdentity()==region.identity&&packet->cohort->matchesOwners(packet,packet->native_tuple))
                            {candidate.admission->publicationCompleted();consume(group,candidate.type,packet,region,static_cast<U32>(candidate.flavor.family));continue;}
                            packet=Inputs::ALSceneGroupCompilationNative::compile(*candidate.admission,candidate.publisher,arm.budget,
                                group.sources.size(),static_cast<U32>(candidate.flavor.family),[&]
                                {return group.family?Inputs::ALSceneGroupPublicationNative::sealMaterial(candidate.publisher,group.dependencies,group.layout,
                                    f.model,region,group.sources,candidate.flavor,Inputs::sNativeSceneSources,
                                    Inputs::sNativeSceneSerial,Inputs::gFrameCount):Inputs::ALSceneGroupPublicationNative::seal(candidate.publisher,group.dependencies,group.layout,
                                    f.model,region,group.sources,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount,mode==2);});
                            consume(group,candidate.type,packet,region,static_cast<U32>(candidate.flavor.family));
                        }
                    }
                    {
                        CullScope scope(cull);
                        for(U32 family=0;family<=2;++family)
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
                    scene.material_argument_buffers=false;scene.source_command_banks=false;
                    if(mode==2)
                    {
                        if(!Inputs::sNativeSceneVisibility)Inputs::sNativeSceneVisibility=ALMetalWorldVisibility::create(*f.device,&f.error);
                        INFO(f.error);REQUIRE(Inputs::sNativeSceneVisibility);
                        value.captured_visibility_bytes+=capture.world_visibility_members.size()*sizeof(ALMetalWorldVisibilityMember);
                        scene.native_visibility=Inputs::sNativeSceneVisibility->makeView(std::move(capture.world_visibility_members),
                            std::move(capture.world_visibility_indices),capture.source_cohorts,scene.drawCount(),&f.error);
                        // A wholly unqualified or denied raw span has no
                        // optional view; all required direct draws survive.
                        if(!scene.native_visibility)CHECK(f.error.empty());
                    }
                    auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(capture.batches),std::move(capture.source_references),
                        std::move(capture.draw_order),std::move(capture.source_cohorts),std::move(arm.queries),std::move(capture.cpu_sources),
                        capture.world_resources,arm.native_epoch,&f.error);INFO(f.error);REQUIRE(inputs);scene={};
                    auto& renderer=view?*arm.probe:*arm.main;
                    REQUIRE(renderer.beginCommandBatch("Actual retained visibility complete cost",&f.error));
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
                const auto visibility=Inputs::sNativeSceneVisibility?Inputs::sNativeSceneVisibility->stats():ALMetalWorldVisibilityStats{};
                value.visibility_views=visibility.views-before_visibility.views;
                value.visibility_groups=visibility.cohort_groups-before_visibility.cohort_groups;
                value.visibility_candidates=visibility.cohort_candidates-before_visibility.cohort_candidates;
                value.visibility_builds=visibility.cohort_row_builds-before_visibility.cohort_row_builds;
                value.visibility_hits=visibility.cohort_row_hits-before_visibility.cohort_row_hits;
                value.visibility_upload_bytes=visibility.cohort_row_upload_bytes-before_visibility.cohort_row_upload_bytes;
                value.expanded_visibility_bytes=visibility.expanded_member_bytes-before_visibility.expanded_member_bytes;
                value.visibility_allocated_bytes=visibility.cohort_row_allocated_bytes-before_visibility.cohort_row_allocated_bytes;
                value.visibility_admission_requests=visibility.admission_requests-before_visibility.admission_requests;
                value.visibility_admission_denials=visibility.admission_denials-before_visibility.admission_denials;
                value.visibility_admitted_bytes=visibility.admitted_logical_bytes-before_visibility.admitted_logical_bytes;
                value.visibility_fallbacks=visibility.fallbacks-before_visibility.fallbacks;
                CHECK(value.captured_visibility_bytes<=value.raw_draws*sizeof(ALMetalWorldVisibilityMember)*(mode==2));
                CHECK(value.expanded_visibility_bytes<=value.captured_visibility_bytes);
                CHECK(visibility.live_cohort_row_bytes<=8*1024*1024);
                if(!loading&&!warmup&&condition==1&&mode && value.captured_draws==U64(visible_draws)*views)
                {CHECK(reads==before_reads);CHECK(value.visibility_upload_bytes==0);CHECK(value.visibility_admission_requests==0);}
                CHECK(value.bound==0);CHECK(value.requested==0);
                value.hits=cache.hits-before_cache.hits;value.builds=cache.builds-before_cache.builds;value.evictions=cache.evictions-before_cache.evictions;
                value.page_hits=pages.cohort_hits-before_pages.cohort_hits;value.page_builds=pages.cohort_builds-before_pages.cohort_builds;
                value.page_failures=pages.cohort_failures-before_pages.cohort_failures;
                value.compile_admitted=compilation.admitted-before_compile.admitted;value.compile_denied=compilation.denied-before_compile.denied;
                CHECK(value.accepted_draws==value.captured_draws);CHECK(value.bound+value.fallback==value.requested);
                CHECK(budget.scene_publications_in_flight==0);CHECK(budget.scene_bytes_in_flight==0);
                if(mode!=2){CHECK(value.requested==0);CHECK(value.bound==0);CHECK(value.executions==0);}
                if(mode==0){CHECK(value.captured_groups==0);CHECK(value.raw_draws==U64(visible_draws)*views);}
                // Retain denied compilation/raw fallback samples rather than
                // assuming every dirty group became a retained cohort.
                if(!loading&&!warmup)measured[mode][frame-loading_frames-warmup_frames]=value;
                if(compare_pixels)for(U32 view=0;view<views;++view)
                {
                    pixels[mode][view]=f.read(*targets[view]);
                    if(heavier_mesh)CHECK(std::any_of(pixels[mode][view].depth.begin(),pixels[mode][view].depth.end(),[](F32 depth){return depth<1.f;}));
                }
                const auto& geometry=arm.geometry_prefix[visible_groups];
                std::cout<<(heavier_mesh?"NATIVE_COHORT_VISIBILITY_MESH_RAW groups=":"NATIVE_COHORT_VISIBILITY_RAW groups=")<<group_count<<" visible_groups="<<visible_groups<<" sources_per_group="<<sources_per_group
                    <<" simple_groups="<<(group_count==1&&first_family==0?visible_groups:0)<<" legacy_groups="<<(group_count==1?(first_family==1?visible_groups:0):(visible_groups+1)/2)<<" pbr_groups="<<(group_count==1?(first_family==2?visible_groups:0):visible_groups/2)<<" views="<<views<<" queries_per_view="<<visible_groups
                    <<" first_family="<<first_family<<" condition="<<(condition==0?"cold":condition==1?"warm":"churn_1_of_16")
                    <<" requested_outside_percent="<<outside_percent<<" actual_outside_sources="<<visible_groups*actual_outside_per_group
                    <<" effective_outside_percent="<<effective_outside_percent
                    <<" vertices_per_source="<<indices_per_source<<" indices_per_source="<<indices_per_source
                    <<" candidate_vertices="<<geometry[0]<<" candidate_indices="<<geometry[1]
                    <<" vertex_buffer_wrapper_extent_bytes="<<geometry[2]<<" index_buffer_wrapper_extent_bytes="<<geometry[3]
                    <<" position_payload_bytes="<<geometry[0]*16<<" index_payload_bytes="<<geometry[1]*sizeof(U16)
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
                    <<" visibility_views="<<value.visibility_views<<" visibility_groups="<<value.visibility_groups<<" visibility_candidates="<<value.visibility_candidates
                    <<" visibility_row_builds="<<value.visibility_builds<<" visibility_row_hits="<<value.visibility_hits
                    <<" visibility_row_upload_bytes="<<value.visibility_upload_bytes<<" expanded_visibility_member_bytes="<<value.expanded_visibility_bytes
                    <<" captured_visibility_member_bytes="<<value.captured_visibility_bytes<<" visibility_native_allocated_bytes="<<value.visibility_allocated_bytes
                    <<" visibility_admission_requests="<<value.visibility_admission_requests<<" visibility_admission_denials="<<value.visibility_admission_denials
                    <<" visibility_admitted_logical_bytes="<<value.visibility_admitted_bytes<<" visibility_fallbacks="<<value.visibility_fallbacks
                    <<" live_visibility_row_bytes="<<visibility.live_cohort_row_bytes<<" cached_visibility_rows="<<visibility.cached_cohort_rows
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
                totals.visibility_groups+=value.visibility_groups;totals.visibility_candidates+=value.visibility_candidates;
                totals.visibility_builds+=value.visibility_builds;totals.visibility_hits+=value.visibility_hits;
                totals.visibility_upload_bytes+=value.visibility_upload_bytes;totals.expanded_visibility_bytes+=value.expanded_visibility_bytes;
                totals.captured_visibility_bytes+=value.captured_visibility_bytes;totals.visibility_allocated_bytes+=value.visibility_allocated_bytes;
                totals.visibility_admission_requests+=value.visibility_admission_requests;totals.visibility_admission_denials+=value.visibility_admission_denials;
                totals.visibility_admitted_bytes+=value.visibility_admitted_bytes;totals.visibility_fallbacks+=value.visibility_fallbacks;
                totals.page_hits+=value.page_hits;totals.page_builds+=value.page_builds;totals.page_failures+=value.page_failures;
            }
            const auto& geometry=arms[mode].geometry_prefix.back();
            std::cout<<(heavier_mesh?"NATIVE_COHORT_VISIBILITY_MESH_COST groups=":"NATIVE_COHORT_VISIBILITY_COST groups=")<<group_count<<" sources_per_group="<<sources_per_group<<" draws_per_view="<<group_count*sources_per_group
                <<" simple_groups="<<(group_count==1&&first_family==0?group_count:0)<<" legacy_groups="<<(group_count==1?(first_family==1?group_count:0):group_count/2)<<" pbr_groups="<<(group_count==1?(first_family==2?group_count:0):group_count/2)<<" views="<<views<<" queries_per_view="<<group_count
                <<" first_family="<<first_family<<" condition="<<(condition==0?"cold":condition==1?"warm":"churn_1_of_16")
                <<" requested_outside_percent="<<outside_percent<<" actual_outside_sources="<<group_count*actual_outside_per_group
                <<" effective_outside_percent="<<effective_outside_percent
                <<" vertices_per_source="<<indices_per_source<<" indices_per_source="<<indices_per_source
                <<" vertices_per_view="<<geometry[0]<<" indices_per_view="<<geometry[1]
                <<" vertex_buffer_wrapper_extent_bytes="<<geometry[2]<<" index_buffer_wrapper_extent_bytes="<<geometry[3]
                <<" position_payload_bytes="<<geometry[0]*16<<" index_payload_bytes="<<geometry[1]*sizeof(U16)
                <<" mode="<<(mode==0?"ordinary_native":mode==1?"direct_cohort":"cohort_visibility")<<" loading_frames="<<loading_frames
                <<" warmup_frames="<<warmup_frames<<" measured_frames="<<measured_frames
                <<" owner_cpu_median_ms="<<median(cpu)<<" owner_cpu_max_ms="<<*std::max_element(cpu.begin(),cpu.end())
                <<" capture_adopt_encode_reclaim_median_ms="<<median(encode)<<" capture_adopt_encode_reclaim_max_ms="<<*std::max_element(encode.begin(),encode.end())
                <<" completion_and_query_settle_median_ms="<<median(complete)<<" completion_and_query_settle_max_ms="<<*std::max_element(complete.begin(),complete.end())
                <<" gpu_sum_median_ms="<<median(gpu)<<" gpu_sum_max_ms="<<*std::max_element(gpu.begin(),gpu.end())
                <<" measured_bank_requested="<<totals.requested<<" measured_bank_draws="<<totals.bound<<" measured_fallback="<<totals.fallback
                <<" measured_execute_calls="<<totals.executions<<" measured_bank_hits="<<totals.hits<<" measured_bank_builds="<<totals.builds
                <<" measured_bank_evictions="<<totals.evictions<<" measured_page_hits="<<totals.page_hits<<" measured_page_builds="<<totals.page_builds
                <<" measured_page_failures="<<totals.page_failures<<" measured_compile_admitted="<<totals.compile_admitted<<" measured_compile_denied="<<totals.compile_denied
                <<" measured_visibility_groups="<<totals.visibility_groups<<" measured_visibility_candidates="<<totals.visibility_candidates
                <<" measured_visibility_builds="<<totals.visibility_builds<<" measured_visibility_hits="<<totals.visibility_hits
                <<" measured_visibility_upload_bytes="<<totals.visibility_upload_bytes<<" measured_expanded_member_bytes="<<totals.expanded_visibility_bytes
                <<" measured_captured_member_bytes="<<totals.captured_visibility_bytes<<" measured_visibility_allocated_bytes="<<totals.visibility_allocated_bytes
                <<" measured_visibility_admission_requests="<<totals.visibility_admission_requests<<" measured_visibility_admission_denials="<<totals.visibility_admission_denials
                <<" measured_visibility_admitted_logical_bytes="<<totals.visibility_admitted_bytes<<" measured_visibility_fallbacks="<<totals.visibility_fallbacks
                <<" command_banks=0 source_rows=0 material_argument_buffers=0 gpu_culling_eliminates_cpu_bindings=0"
                <<" progressive_loading_eighths=1 loading_warmup_denials_retained=1 query_at_each_group_boundary=1 pass_order_legacy_then_pbr=1"
                <<" native_visibility_live_row_cap_bytes=8388608 raw_argument_slot_cap=32 independent_group_source_admission=1 independent_framedata_renderers_material_cache=1"
                <<" paired_logical_frame=1 native_epoch_once_per_arm_frame=1 shared_exact_library=1 distinct_view_matrix_payloads=1 typed_identity_camera_frontier=1"
                <<" dirty_raw_admission_before_seal=1 same_view_verified_replacement=1 cpu_and_gpu_scene_admission=1 native_visibility_metadata_count_bytes_bounded=1 synthetic_idle_budget_inputs=1"
                <<" shared_context_counters_observed_once=1 retained_native_index_footprint_not_new_allocation=1"
                <<" fixture_indices_per_source="<<indices_per_source<<" unique_vertex_indices_per_source="<<indices_per_source
                <<" same_local_mesh_aabb=1 repeated_three_vertex_indices=0 wrapper_extents_are_not_physical_unique_bytes=1"
                <<" geometry_texture_drawinfo_creation_excluded=1 generation_bounds_scan_inside_capture_timer=1"
                <<" loading_means_first_native_publication_not_asset_download=1 whole_viewer_fps_claim=0\n";
        }
    }
}

TEST_CASE("Actual retained cohort visibility includes cold warm churn and many-group costs",
          "[almetal][.native-cohort-visibility-cost]")
{actualRetainedCohortVisibilityCosts(false);}

TEST_CASE("Actual retained cohort visibility compares unique heavier meshes through complete publication",
          "[almetal][.native-cohort-visibility-mesh-cost]")
{actualRetainedCohortVisibilityCosts(true);}
