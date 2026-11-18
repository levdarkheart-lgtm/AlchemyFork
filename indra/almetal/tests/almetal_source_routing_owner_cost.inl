/** Actual-renderer E3 owner costs. Included after the shared fixture. */
TEST_CASE("almetal: E3 actual owner scalar versus applicable native visibility costs",
          "[.][almetal][source-applicability-owner-cost]")
{
    const char* selected=std::getenv("ALCHEMY_E3_OWNER_COST_CELL");
    const char* output_path=std::getenv("ALCHEMY_E3_OWNER_COST_CSV");
    REQUIRE(selected);REQUIRE(output_path);const std::string text=selected;
    REQUIRE(text.size()==1);REQUIRE(text[0]>='0');REQUIRE(text[0]<='6');
    const unsigned cell=text[0]-'0';
    std::ofstream csv(output_path);REQUIRE(csv);
    csv<<"cell,block,warm,position,arm,route,draws,indices,qualified_indices,bank_draws,visibility_dispatches,begin,end,gpu_ns,wall_ns,settlement_commands,owner_validation,compute_validation,resource_mode,page_requests,bank_requests,views,cpu_culled,frame\n";
    REQUIRE(std::string(std::getenv("MTL_SHADER_VALIDATION")?std::getenv("MTL_SHADER_VALIDATION"):"")=="0");
    REQUIRE(std::string(std::getenv("MTL_DEBUG_LAYER")?std::getenv("MTL_DEBUG_LAYER"):"")=="0");
    REQUIRE(std::string(std::getenv("MTL_CAPTURE_ENABLED")?std::getenv("MTL_CAPTURE_ENABLED"):"")=="0");
    @autoreleasepool
    {
        E3Fixture f;const unsigned count=cell==0?6:(cell==3?64:32),grid=cell==0?1:64;
        f.make(cell==3?2:1,false,false,count,grid,cell==6?2:(cell==2?1:0));f.extent=128;
        std::uint64_t total_indices=0,total_qualified=0;
        for(const auto& view:f.views)
        {
            const auto work=alMetalInspectWorldCohortVisibilityWork(view);REQUIRE(work.valid);
            total_indices+=work.indexed_elements;total_qualified+=work.qualified_indexed_elements;
            if(cell==3)
            {
                const auto minimum=ALMetalWorldSourceCommandPolicy{}.minimum_qualified_indices;
                REQUIRE(work.qualified_indexed_elements>=minimum);
                REQUIRE(work.outside_center_indexed_elements>=minimum/4);
            }
        }
        if(cell==3)
        {
            // Genuinely heavy mixed-unknown PBR input, not the retained failed
            // grid32 control whose qualified rows were below policy admission.
            REQUIRE(total_indices==64u*24576u);REQUIRE(total_qualified==55u*24576u);
            std::ofstream scope(std::string(output_path)+".scope.json");REQUIRE(scope);
            scope<<"{\"label\":\"heavy-PBR-mixed-unknown\",\"sources\":64,\"grid\":64,"
                "\"unique_vertices_per_source\":4225,\"indices_per_source\":24576,"
                "\"qualified_sources\":55,\"qualified_indices\":"<<total_qualified<<"}";REQUIRE(scope.good());
        }
        std::uint64_t next_frame=2;
        const auto demand=[&](bool accepted,const char* why)
        {if(!accepted)throw std::runtime_error(std::string(why)+": "+f.error);};
        auto render=[&](bool candidate,unsigned iteration,bool retain_outputs=false)
        {
            struct Result
            {
                std::unique_ptr<ALMetalRenderTarget> target,second_target;
                ALMetalCommandGPUTime gpu;std::uint64_t wall=0,bank_draws=0,dispatches=0,commits=0;
                unsigned route=0;std::uint32_t owner_validation=UINT32_MAX,compute_validation=UINT32_MAX;
                std::uint64_t page_requests=0,bank_requests=0,cpu_culled=0,frame=0;
            } result;
            if(cell==4)for(auto& view:f.views)view.view.modelview_projection[12]=(iteration%2)?.0625f:-.0625f;
            const auto before=f.renderer->worldSourceCommandStats();
            const auto pages_before=f.renderer->worldSourceStats().pages;
            const auto cpu_before=f.renderer->worldCpuVisibilityStats();
            const auto commits=ALMetalDevice::committedCommandBufferCount();
            const auto start=std::chrono::steady_clock::now();
            @autoreleasepool
            {
            result.frame=next_frame++;f.data->beginFrame(result.frame);
            if(cell==5)
            {
                // Exact source-publication identity churn, retaining the same
                // mesh backing. Neither shader reload nor PSO rebuild is hidden
                // inside this workload; physical COW has separate strict tests.
                static std::uint64_t serial=100000;
                auto previous=std::move(f.views);f.views.clear();f.views.reserve(previous.size());
                for(const auto& old:previous)
                {
                    std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> sources;
                    for(std::size_t row=0;row<old.cohort->size();++row)
                    {
                        auto key=old.cohort->key(row);key.identity=++serial;
                        auto source=ALMetalWorldCohortSource::seal({key,&old.cohort->source(row)},
                            &f.error,&old.cohort->bounds(row));demand(bool(source),"churn source publication");sources.push_back(std::move(source));
                    }
                    auto view=old;view.cohort=ALMetalWorldSourceCohort::seal(sources,f.cpu,f.native,
                        old.cohort->regionIdentity(),&f.error);demand(bool(view.cohort),"churn cohort publication");f.views.push_back(std::move(view));
                }
            }
            auto scene=f.scene();scene.width=scene.height=f.extent;scene.hdr=true;scene.normal_hdr=true;
            scene.cohorts=f.views.data();scene.cohort_count=f.views.size();scene.source_owner=f.native;scene.cpu_source_owner=f.cpu;
            // Invoke the ACTUAL production policy, with all old experiment
            // banks/rows/table controls left OFF in both arms. Scalar A pays no
            // route inspection, SourcePages, bank allocation or compute setup.
            scene.source_routing=candidate;scene.source_native_visibility=candidate;
            demand(f.renderer->beginCommandBatch("E3 complete actual G-buffer owner",&f.error),"begin owner");
            result.target=f.renderer->render(scene,&f.error);demand(bool(result.target),"render owner");
            if(cell==4)
            {
                for(auto& view:f.views)view.view.modelview_projection[12]+=.125f;
                result.second_target=f.renderer->render(scene,&f.error);demand(bool(result.second_target),"render second view");
            }
            auto completion=f.renderer->commandBatchCompletion();demand(bool(completion),"owner completion token");
            demand(f.renderer->commitCommandBatch(&f.error),"commit owner");
            demand(completion->waitUntilCompleted(&f.error),"wait actual owner"); // The actual submitted owner; no queue/settlement command.
            demand(completion->gpuTime(result.gpu)==ALMetalCommandTimingStatus::ready,"owner GPU timestamps");
            if(!retain_outputs){result.target.reset();result.second_target.reset();}
            completion.reset();
            }
            // Include transient native pool drainage in the owner wall; the
            // result retains only its actual output target generations.
            result.wall=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
            result.commits=ALMetalDevice::committedCommandBufferCount()-commits;
            demand(result.commits==1,"one actual owner command");
            const auto after=f.renderer->worldSourceCommandStats();
            result.route=after.routing_native>before.routing_native?2:(after.routing_cpu>before.routing_cpu?1:0);
            result.bank_draws=after.bank_draws-before.bank_draws;
            result.dispatches=after.cache.static_visibility_dispatches-before.cache.static_visibility_dispatches;
            result.page_requests=f.renderer->worldSourceStats().pages.cohort_requests-pages_before.cohort_requests;
            result.bank_requests=after.cache.requests-before.cache.requests;
            result.cpu_culled=f.renderer->worldCpuVisibilityStats().culled_draws-cpu_before.culled_draws;
            result.owner_validation=candidate&&cell==6?UINT32_MAX:after.last_pipeline_validation;
            if(result.route==2)result.compute_validation=after.cache.last_static_validation;
            const unsigned views=cell==4?2:1;const bool gpu=candidate&&cell!=0&&cell!=2&&cell!=6;
            demand(result.route==(gpu?2:(candidate&&cell==6?1:0)),"actual route");
            demand(result.bank_draws==(gpu?count*views:0),"actual native draws");demand(result.dispatches==(gpu?views:0),"actual visibility dispatches");
            // Root dfd11 records the actual ordinary scalar owner PSO too.
            // The CPU-outside owner legitimately selects no shader. Never
            // substitute a stale preceding scalar PSO mode for no work.
            if(candidate&&cell==6)demand(result.cpu_culled==count,"actual CPU rejects");
            else demand(result.owner_validation==2,"actual owner validation disabled");
            if(gpu)demand(result.compute_validation==2,"actual compute validation disabled");
            else{demand(result.page_requests==0,"scalar bypasses SourcePages");demand(result.bank_requests==0,"scalar bypasses banks");}
            return result;
        };
        auto a=render(false,0,true),b=render(true,0,true);
        const auto baseline=f.read(*a.target),candidate=f.read(*b.target);
        REQUIRE(baseline==candidate);
        if(cell==4)REQUIRE(f.read(*a.second_target)==f.read(*b.second_target));
        const auto clear=std::uint32_t{0x3f800000};
        const auto covered=std::count_if(baseline.depth.begin(),baseline.depth.end(),[&](auto value){return value!=clear;});
        if(cell==6)REQUIRE(covered==0);else REQUIRE(covered>0);
        if(std::getenv("ALCHEMY_E3_OWNER_PRECHECK_ONLY"))
        {
            std::cout<<"E3_OWNER_PRECHECK cell="<<cell<<" exact=1 scalar_pages="<<a.page_requests
                <<" candidate_pages="<<b.page_requests<<" ownerA="<<a.owner_validation<<" ownerB="<<b.owner_validation
                <<" computeB="<<b.compute_validation<<" native_draws="<<b.bank_draws
                <<" resource_mode="<<unsigned(f.device->resourcePlanningMode())<<'\n';
            return;
        }
        // One first-use observation per arm, retained separately from the
        // paired warm measurements. Optional B PSO/pages/bank setup is inside
        // these clocks; original scene/mesh/material publication is not.
        std::ofstream cold(std::string(output_path)+".cold.csv");REQUIRE(cold);
        cold<<"arm,route,begin,end,gpu_ns,wall_ns,owner_validation,compute_validation,page_requests,bank_requests\n";
        const auto save_cold=[&](char arm,const auto& value)
        {cold<<arm<<','<<value.route<<','<<std::setprecision(17)<<value.gpu.start_seconds<<','<<value.gpu.end_seconds
             <<','<<value.gpu.elapsed_nanoseconds<<','<<value.wall<<','<<value.owner_validation<<','<<value.compute_validation
             <<','<<value.page_requests<<','<<value.bank_requests<<'\n';};
        save_cold('A',a);save_cold('B',b);REQUIRE(cold.good());cold.close();
        a.target.reset();a.second_target.reset();b.target.reset();b.second_target.reset();
        double previous_end=0;
        for(unsigned block=0;block<160;++block)
        {
            const bool warm=block<10,abba=block%2==0;const bool order[]{!abba,abba,abba,!abba};
            for(unsigned position=0;position<4;++position)
            {
                const bool candidate=order[position];const auto result=render(candidate,block);
                REQUIRE(result.gpu.start_seconds>=previous_end);previous_end=result.gpu.end_seconds;
                csv<<cell<<','<<block<<','<<warm<<','<<position<<','<<(candidate?'B':'A')<<','<<result.route<<','
                   <<count<<','<<total_indices<<','<<total_qualified<<','<<result.bank_draws<<','<<result.dispatches<<','
                   <<std::setprecision(17)<<result.gpu.start_seconds<<','<<result.gpu.end_seconds<<','
                   <<result.gpu.elapsed_nanoseconds<<','<<result.wall<<",0,"<<result.owner_validation<<','
                   <<result.compute_validation<<','<<unsigned(f.device->resourcePlanningMode())<<','
                   <<result.page_requests<<','<<result.bank_requests<<','<<(cell==4?2:1)<<','<<result.cpu_culled<<','<<result.frame<<'\n';
            }
        }
        REQUIRE(csv.good());
    }
}
