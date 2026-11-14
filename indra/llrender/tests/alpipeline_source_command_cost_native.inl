/**
 * @file alpipeline_source_command_cost_native.inl
 * @brief Actual published-source capture through native command-bank retirement.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

TEST_CASE("Actual Legacy and PBR producers include native command-bank complete path costs",
          "[almetal][.native-source-command-cost]")
{
    constexpr U32 samples=12,warmups=2;
    const auto cpu_ns=[]
    {
        timespec value{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0);
        return U64(value.tv_sec)*1'000'000'000ULL+value.tv_nsec;
    };
    const auto median=[](auto values)
    {std::sort(values.begin(),values.end());return (values[5]+values[6])*.5;};
    struct Sample
    {
        double cpu=0,encode=0,complete=0,gpu=0;
        U64 source_publications=0,requested=0,bank_draws=0,fallback=0,execute_calls=0;
        U64 cache_requests=0,hits=0,builds=0,executions=0,logical_draws=0,evictions=0,argument_bytes=0;
        U64 icb_bytes=0,index_bytes=0,cached_banks=0,live_banks=0;
        U64 row_publications=0,row_bytes=0,page_copy_bytes=0,bank_copy_bytes=0,view_bytes=0,metadata_live_bytes=0;
        U64 material_publications=0,material_bytes=0,material_hits=0,native_frame_bytes=0;
        U64 raw_bytes=0,reference_bytes=0,order_bytes=0,candidate_bytes=0,region_bytes=0,cpu_pins=0,native_pins=0;
        U64 getters=0,dependencies=0,image_getters=0,gpu_ready=0;
    };
    for(U32 family:{1u,2u})
    {
        // Each producer has a real LLDrawInfo and independent LLVertexBuffer;
        // geometry/texture/DrawInfo construction is the explicit setup frontier.
        std::vector<std::unique_ptr<Inputs::LLFetchedGLTFMaterial>> material_owners;
        GroupPublication f;ALNativeRegionModel region;
        REQUIRE(region.update(f.model,LLVector3(0.f,0.f,0.f)));
        Inputs::gSavedSettings.native_resources=false;Inputs::gSavedSettings.native_visibility=false;
        auto data=ALMetalWorldFrameData::create(*f.device,&f.error);INFO(f.error);REQUIRE(data);
        auto library=data->builtinLibrary(&f.error);INFO(f.error);REQUIRE(library);
        auto main=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
        auto probes=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
        INFO(f.error);REQUIRE(main);REQUIRE(probes);
        REQUIRE(main->shaderLibrary()==library);REQUIRE(probes->shaderLibrary()==library);
        main->setFrameData(data);probes->setFrameData(data);
        // One actual shared cache observation; renderer cache snapshots must
        // never be summed when main/probe share the same native cache.
        auto banks=data->worldSourceCommands(&f.error);INFO(f.error);REQUIRE(banks);REQUIRE(banks->supported());
        U64 native_epoch=0;
        struct Arm
        {
            Inputs::LLPipeline pipeline;
            std::vector<Inputs::LLDrawInfo*> sources;
            std::vector<ALMetalWorldVisibilityQuery> queries;
        };
        for(U32 count:{1u,4u,16u,512u})
        {
            std::array<Arm,2> arms;
            for(auto& arm:arms)for(U32 source=0;source<count;++source)
            {
                auto& info=f.draw();
                if(family==2)
                {
                    info.mVertexBuffer=f.buffer(true,LLVertexBuffer::MAP_TANGENT);
                    std::array<LLVector4a,3> tangents;for(auto& tangent:tangents)tangent.set(1.f,0.f,0.f,1.f);
                    info.mVertexBuffer->setTangentData(tangents.data());info.mVertexBuffer->unmapBuffer();
                    auto material=std::make_unique<Inputs::LLFetchedGLTFMaterial>();
                    info.mGLTFMaterial.value=material.get();material_owners.push_back(std::move(material));
                }
                arm.sources.push_back(&info);
            }
            for(U32 views:{1u,3u})for(U32 condition=0;condition<3;++condition)
            {
                CAPTURE(family,count,views,condition);
                for(auto& arm:arms)for(auto* info:arm.sources)info->state->invalidate();
                std::array<std::array<Sample,samples>,2> costs;
                for(U32 sample=0;sample<samples+warmups;++sample)
                {
                    std::array<std::array<GroupPublication::Image,3>,2> pixels;
                    std::array<std::array<std::array<std::uint64_t,2>,3>,2> query_counts{};
                    ++Inputs::gFrameCount;
                    for(U32 position=0;position<2;++position)
                    {
                        const U32 mode=(sample+position)%2;auto& arm=arms[mode];CAPTURE(mode,sample);
                        auto& capture=*arm.pipeline.mMetalWorldSimpleCapture;
                        const auto before_reads=f.reads();const auto before_material=Inputs::ALScenePublicationNative::materialDataStats();
                        const auto before_serial=Inputs::sNativeSceneSerial;const auto before_bytes=data->stats().published_bytes;
                        const auto before_cache=banks->stats();const auto before_pages=main->worldSourceStats().pages;
                        const std::array<ALMetalWorldSourceCommandDrawStats,2> before_render{main->worldSourceCommandStats(),probes->worldSourceCommandStats()};
                        std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                        std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                        std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> results;
                        Sample value;
                        const auto begin=std::chrono::steady_clock::now();const auto begin_cpu=cpu_ns();
                        data->beginFrame(++native_epoch); // Exactly one native frame for main plus both probe views.
                        if(condition==0)for(auto* info:arm.sources)info->state->invalidate();
                        if(condition==2)
                        {
                            const LLColor4U tint=sample%2?LLColor4U(0,255,0,255):LLColor4U(0,0,255,255);
                            const std::array<LLColor4U,3> changed{tint,tint,tint};
                            for(U32 source=0;source<count;source+=16)
                            {auto& vb=*arm.sources[source]->mVertexBuffer;vb.setColorData(changed.data());vb.unmapBuffer();}
                        }
                        for(U32 view=0;view<views;++view)
                        {
                            capture.clearDraws();capture.complete=true;
                            capture.beginSourceRows(mode!=0);capture.compact_gbuffer_active=mode!=0;
                            if(mode)capture.registerSourceModel(&f.model,region.snapshot());
                            capture.world_resources=std::make_shared<Inputs::WorldResources>();
                            for(auto* info:arm.sources)arm.pipeline.captureMetalPublishedWorldBatch(*info,family,false);
                            REQUIRE(capture.complete);REQUIRE(capture.logicalDrawCount()==count);REQUIRE(capture.flushDrawOrder());
                            value.raw_bytes+=capture.batches.size()*sizeof(ALMetalWorldSimpleBatch);
                            value.reference_bytes+=capture.source_references.size()*sizeof(ALMetalWorldSourceReference);
                            value.order_bytes+=capture.draw_order.size()*sizeof(ALMetalWorldDrawIndex);
                            value.cpu_pins+=capture.cpu_sources?capture.cpu_sources->owners.size():0;
                            value.native_pins+=capture.world_resources->owners.size();
                            if(mode)
                            {
                                REQUIRE(capture.source_rows);REQUIRE(capture.source_rows->candidates.size()==count);
                                REQUIRE(capture.source_rows->regions.size()==1);
                                value.candidate_bytes+=count*sizeof(ALMetalWorldSourceView::Candidate);
                                value.region_bytes+=sizeof(ALMetalWorldSourceRegion);
                            }
                            else REQUIRE_FALSE(capture.source_rows);
                            arm.queries.resize(2);
                            for(U32 query=0;query<2;++query)
                            {
                                auto& q=arm.queries[query];q={};
                                std::memcpy(q.modelview_projection,f.model.getF32ptr(),sizeof(q.modelview_projection));
                                q.center[0]=.35f;q.center[1]=-.5f;q.center[2]=.75f;
                                q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.08f;
                                q.batch_offset=query?count:count/2;q.fan=7;q.count_samples=1;
                            }
                            auto scene=f.scene(capture);scene.visibility_queries=arm.queries.data();scene.visibility_query_count=arm.queries.size();
                            scene.material_argument_buffers=false;
                            scene.source_command_banks=mode!=0;scene.source_command_minimum_run=2;
                            auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(capture.batches),
                                std::move(capture.source_references),std::move(capture.draw_order),std::move(arm.queries),
                                std::move(capture.cpu_sources),capture.world_resources,native_epoch,&f.error);
                            INFO(f.error);REQUIRE(inputs);scene={};
                            auto& renderer=view==0?*main:*probes;
                            REQUIRE(renderer.beginCommandBatch("Actual Legacy/PBR source command-bank complete cost",&f.error));
                            targets[view]=renderer.renderOwnedInputs(*inputs,&f.error,&results[view]);INFO(f.error);
                            REQUIRE(targets[view]);REQUIRE(results[view]);completions[view]=renderer.commandBatchCompletion();
                            REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,
                                capture.source_references,capture.draw_order,arm.queries));
                            capture.clearDraws();capture.world_resources=std::make_shared<Inputs::WorldResources>();
                            REQUIRE(renderer.commitCommandBatch(&f.error));
                        }
                        const auto encoded=std::chrono::steady_clock::now();
                        REQUIRE(f.device->waitUntilIdle(&f.error));
                        for(U32 view=0;view<views;++view)for(U32 query=0;query<2;++query)
                        {
                            REQUIRE(results[view]->wait(query,query_counts[mode][view][query],&f.error));
                            CHECK(query_counts[mode][view][query]>0);
                        }
                        gGL.frameFences().release();
                        const auto completed=std::chrono::steady_clock::now();
                        value.cpu=(cpu_ns()-begin_cpu)/1e6;
                        value.encode=std::chrono::duration<double,std::milli>(encoded-begin).count();
                        value.complete=std::chrono::duration<double,std::milli>(completed-begin).count();
                        for(U32 view=0;view<views;++view)
                        {
                            ALMetalCommandGPUTime time;
                            if(completions[view]&&completions[view]->gpuTime(time)==ALMetalCommandTimingStatus::ready)
                            {value.gpu+=time.elapsed_nanoseconds/1e6;++value.gpu_ready;}
                        }
                        const auto after_reads=f.reads();const auto after_material=Inputs::ALScenePublicationNative::materialDataStats();
                        const auto after_cache=banks->stats();const auto after_pages=main->worldSourceStats().pages;
                        const std::array<ALMetalWorldSourceCommandDrawStats,2> after_render{main->worldSourceCommandStats(),probes->worldSourceCommandStats()};
                        for(U32 owner=0;owner<2;++owner)
                        {
                            value.requested+=after_render[owner].requested_draws-before_render[owner].requested_draws;
                            value.bank_draws+=after_render[owner].bank_draws-before_render[owner].bank_draws;
                            value.fallback+=after_render[owner].fallback_draws-before_render[owner].fallback_draws;
                            value.execute_calls+=after_render[owner].execute_calls-before_render[owner].execute_calls;
                        }
                        value.source_publications=Inputs::sNativeSceneSerial-before_serial;
                        value.native_frame_bytes=data->stats().published_bytes-before_bytes;
                        value.material_publications=after_material.publications-before_material.publications;
                        value.material_bytes=after_material.published_bytes-before_material.published_bytes;
                        value.material_hits=after_material.hits-before_material.hits;
                        value.getters=after_reads[0]-before_reads[0];value.dependencies=after_reads[1]-before_reads[1];value.image_getters=after_reads[2]-before_reads[2];
                        value.cache_requests=after_cache.requests-before_cache.requests;value.hits=after_cache.hits-before_cache.hits;
                        value.builds=after_cache.builds-before_cache.builds;value.executions=after_cache.executions-before_cache.executions;
                        value.logical_draws=after_cache.logical_draws-before_cache.logical_draws;value.evictions=after_cache.evictions-before_cache.evictions;
                        value.argument_bytes=after_cache.charged_logical_argument_bytes-before_cache.charged_logical_argument_bytes;
                        value.icb_bytes=after_cache.live_icb_allocated_bytes;value.index_bytes=after_cache.unique_retained_index_bytes;
                        value.cached_banks=after_cache.cached_banks;value.live_banks=after_cache.live_banks;
                        value.row_publications=after_pages.row_publications-before_pages.row_publications;
                        value.row_bytes=after_pages.encoded_row_bytes-before_pages.encoded_row_bytes;
                        value.page_copy_bytes=after_pages.copied_page_bytes-before_pages.copied_page_bytes;
                        value.bank_copy_bytes=after_pages.copied_bank_bytes-before_pages.copied_bank_bytes;
                        value.view_bytes=after_pages.view_bytes-before_pages.view_bytes;value.metadata_live_bytes=after_pages.live_metadata_bytes;
                        CHECK(value.requested==(mode?U64(count)*views:0));
                        CHECK(value.bank_draws==((mode&&count>=4)?U64(count)*views:0));
                        CHECK(value.fallback==((mode&&count==1)?views:0));
                        CHECK(value.execute_calls==((mode&&count>=4)?U64(2)*views:0));
                        CHECK(value.executions==value.execute_calls);CHECK(value.logical_draws==value.bank_draws);
                        if(mode&&count==1)
                            CHECK(after_render[0].last_fallback==ALMetalWorldSourceCommandFailure::short_run);
                        if(sample==warmups-1)for(U32 view=0;view<views;++view)pixels[mode][view]=f.read(*targets[view]);
                        if(sample>=warmups)
                        {
                            CHECK(value.source_publications==(condition==0?count:condition==1?0:(count+15)/16));
                            costs[mode][sample-warmups]=value;
                        }
                        // Preserve every startup and measured observation,
                        // including short-run fallback and adverse samples.
                        std::cout<<"NATIVE_SOURCE_COMMAND_RAW family="<<(family==1?"legacy":"pbr")<<" draws_per_view="<<count<<" views="<<views
                            <<" condition="<<condition<<" mode="<<mode<<" sample="<<sample<<" warmup="<<(sample<warmups)
                            <<" owner_cpu_ms="<<value.cpu<<" capture_adopt_encode_reclaim_ms="<<value.encode<<" completion_and_query_settle_ms="<<value.complete
                            <<" gpu_sum_ms="<<value.gpu<<" gpu_ready_commands="<<value.gpu_ready<<" source_publications="<<value.source_publications
                            <<" requested="<<value.requested<<" bank_draws="<<value.bank_draws<<" fallback="<<value.fallback<<" execute_calls="<<value.execute_calls
                            <<" cache_requests="<<value.cache_requests<<" cache_hits="<<value.hits<<" cache_builds="<<value.builds<<" cache_evictions="<<value.evictions
                            <<" logical_draws="<<value.logical_draws<<" encoded_logical_argument_bytes="<<value.argument_bytes
                            <<" live_icb_allocated_bytes="<<value.icb_bytes<<" unique_retained_index_bytes="<<value.index_bytes
                            <<" cached_banks="<<value.cached_banks<<" live_banks="<<value.live_banks
                            <<" row_publications="<<value.row_publications<<" encoded_row_bytes="<<value.row_bytes
                            <<" copied_page_bytes="<<value.page_copy_bytes<<" copied_pointer_bank_bytes="<<value.bank_copy_bytes
                            <<" required_view_bytes="<<value.view_bytes<<" source_metadata_live_bytes="<<value.metadata_live_bytes
                            <<" material_publications="<<value.material_publications<<" material_published_bytes="<<value.material_bytes<<" material_cache_hits="<<value.material_hits
                            <<" native_frame_published_bytes="<<value.native_frame_bytes<<" raw_value_bytes="<<value.raw_bytes<<" reference_value_bytes="<<value.reference_bytes
                            <<" order_value_bytes="<<value.order_bytes<<" candidate_value_bytes="<<value.candidate_bytes<<" regional_value_bytes="<<value.region_bytes
                            <<" cpu_description_owner_pins="<<value.cpu_pins<<" native_tuple_owner_pins="<<value.native_pins
                            <<" source_getters="<<value.getters<<" dependency_walks="<<value.dependencies<<" image_getters="<<value.image_getters
                            <<" shared_native_cache_observed_once=1\n";
                    }
                    if(sample==warmups-1)for(U32 view=0;view<views;++view)
                    {CHECK(pixels[0][view]==pixels[1][view]);CHECK(query_counts[0][view]==query_counts[1][view]);}
                    else for(U32 view=0;view<views;++view)CHECK(query_counts[0][view]==query_counts[1][view]);
                }
                for(U32 mode=0;mode<2;++mode)
                {
                    std::array<double,samples> cpu{},encode{},complete{},gpu{};
                    for(U32 sample=0;sample<samples;++sample)
                    {cpu[sample]=costs[mode][sample].cpu;encode[sample]=costs[mode][sample].encode;complete[sample]=costs[mode][sample].complete;gpu[sample]=costs[mode][sample].gpu;}
                    std::cout<<"NATIVE_SOURCE_COMMAND_COST family="<<(family==1?"legacy":"pbr")<<" draws_per_view="<<count<<" views="<<views
                        <<" condition="<<(condition==0?"cold_source_publication":condition==1?"warm":"churn")
                        <<" update_stride=16 updated_sources_per_frame="<<(condition==2?(count+15)/16:0)
                        <<" mode="<<(mode?"native_source_command_banks":"ordinary_native")<<" samples="<<samples<<" warmups="<<warmups
                        <<" owner_cpu_median_ms="<<median(cpu)<<" owner_cpu_max_ms="<<*std::max_element(cpu.begin(),cpu.end())
                        <<" capture_adopt_encode_reclaim_median_ms="<<median(encode)<<" capture_adopt_encode_reclaim_max_ms="<<*std::max_element(encode.begin(),encode.end())
                        <<" completion_and_query_settle_median_ms="<<median(complete)<<" completion_and_query_settle_max_ms="<<*std::max_element(complete.begin(),complete.end())
                        <<" gpu_sum_median_ms="<<median(gpu)<<" gpu_sum_max_ms="<<*std::max_element(gpu.begin(),gpu.end())
                        <<" unique_drawinfo_sources="<<count<<" separate_llvertexbuffer_objects="<<count<<" material_values_shared=1 regional_models=1 identity_camera=1"
                        <<" queries_per_view=2 baseline_argument_tables=0 shared_exact_library=1 shared_native_cache_observed_once=1"
                        <<" paired_logical_frame=1 separate_arm_native_epochs=1 app_admission_policy=0"
                        <<" mrt_depth_parity_sample=last_warmup query_parity_all_samples=1 shared_library_renderer_setup_excluded=1"
                        <<" geometry_texture_drawinfo_creation_excluded=1 viewer_dependency_frontier=typed_native_signals whole_viewer_fps_claim=0\n";
                }
            }
        }
    }
}
