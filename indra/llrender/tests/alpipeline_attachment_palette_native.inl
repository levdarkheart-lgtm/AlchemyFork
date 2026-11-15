/**
 * @file alpipeline_attachment_palette_native.inl
 * @brief Actual rigged capture and shared immutable palette publication.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included at the end of the actual LLVB/source/capture fixture translation unit.
 */
namespace
{
// Skeleton simulation and matrix packing are explicit typed inputs here.
// Record sealing, LLVB publication, source factories, capture, adoption,
// validation, arena publication and Metal encoding are production bodies.
struct AttachmentPalettePublication : MaterialGroupPublication
{
    U32 joints;
    std::vector<std::unique_ptr<Inputs::LLVOAvatar>> avatars;
    AttachmentPalettePublication(U32 family,U32 joint_count,U32 draws=1,bool unique=false)
        : MaterialGroupPublication(family),joints(joint_count)
    {
        Inputs::gSavedSettings.native_resources=false;flavor.native_resources=false;
        for(U32 at=0;at<(unique?draws:1u);++at)
        {auto avatar=std::make_unique<Inputs::LLVOAvatar>();avatar->palette.mMatrixPalette.resize(joints);avatars.push_back(std::move(avatar));}
        pose(0.f);
        for(U32 at=0;at<draws;++at)
        {
            auto& info=materialDraw();
            info.mVertexBuffer=buffer(true,LLVertexBuffer::MAP_WEIGHT4|(family==2?LLVertexBuffer::MAP_TANGENT:0));
            LLStrider<LLVector4> weights;REQUIRE(info.mVertexBuffer->getWeight4Strider(weights));
            for(U32 vertex=0;vertex<3;++vertex)weights[vertex].set(F32(joints-1)+.5f,0.f,0.f,0.f);
            if(family==2)
            {LLStrider<LLVector4a> tangents;REQUIRE(info.mVertexBuffer->getTangentStrider(tangents));
             for(U32 vertex=0;vertex<3;++vertex)tangents[vertex].set(1.f,0.f,0.f,1.f);}
            info.mVertexBuffer->unmapBuffer();
            info.mAvatar=avatars[unique?at:0].get();info.mSkinInfo=info.mAvatar;
        }
    }
    ~AttachmentPalettePublication(){infos.clear();}
    void pose(F32 translation,bool distinct_values=true)
    {
        for(U32 at=0;at<avatars.size();++at)
        {
            auto& palette=avatars[at]->palette;palette.invalidate();palette.mGLMp.assign(4+joints*12,0.f);
            // Both distinct poses and distinct identities with equal bytes
            // are measured; the latter can favor the raw content-hash path.
            palette.mGLMp[0]=distinct_values?F32(at)*.0001f:0.f;
            for(U32 joint=0;joint<joints;++joint)
            {const auto offset=4+joint*12;palette.mGLMp[offset]=1.f;palette.mGLMp[offset+5]=1.f;
             palette.mGLMp[offset+10]=1.f;palette.mGLMp[offset+3]=translation;}
        }
    }
    U64 seals() const
    {U64 total=0;for(const auto& avatar:avatars)total+=avatar->palette.seals;return total;}
    void captureRigged(Inputs::LLPipeline& pipeline,bool record=true,bool post=false,bool glow=false)
    {
        auto& capture=*pipeline.mMetalWorldSimpleCapture;capture.clearDraws();capture.complete=true;
        configure(capture,false,false);capture.rigged=true;capture.world_post_deferred_active=post;capture.pbr_glow=glow;
        capture.world_resources=std::make_shared<Inputs::WorldResources>();
        for(const auto& info:infos)pipeline.captureMetalPublishedWorldBatch(*info,U32(flavor.family),false);
        REQUIRE(capture.complete);REQUIRE(capture.batches.size()==infos.size());
        if(!record)for(auto& batch:capture.batches)batch.skin_palette_record=nullptr; // Exact former raw-span path.
    }
    std::shared_ptr<const ALMetalWorldViewInputs> adopt(Inputs::LLPipeline& pipeline,U64 epoch,
        std::vector<ALMetalWorldVisibilityQuery> queries={})
    {
        auto& capture=*pipeline.mMetalWorldSimpleCapture;auto view=scene(capture);
        view.visibility_queries=queries.data();view.visibility_query_count=queries.size();
        return ALMetalWorldViewInputs::adopt(view,std::move(capture.batches),std::move(capture.source_references),
            std::move(capture.draw_order),std::move(capture.source_cohorts),std::move(queries),
            std::move(capture.cpu_sources),capture.world_resources,epoch,&error);
    }
    void reclaim(std::shared_ptr<const ALMetalWorldViewInputs>& inputs,Inputs::LLPipeline& pipeline)
    {
        auto& capture=*pipeline.mMetalWorldSimpleCapture;std::vector<ALMetalWorldVisibilityQuery> queries;
        REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,capture.source_references,
            capture.draw_order,capture.source_cohorts,queries));
        capture.clearDraws();capture.world_resources=std::make_shared<Inputs::WorldResources>();
    }
    std::vector<ALMetalWorldSpotShadowBatch> shadows() const
    {
        std::vector<ALMetalWorldSpotShadowBatch> result;result.reserve(infos.size());
        for(const auto& info:infos)
        {
            ALMetalWorldSpotShadowBatch batch;const auto& buffer=*info->mVertexBuffer;
            batch.vertices=buffer.getMetalVertexBuffer();batch.indices=buffer.getMetalIndexBuffer();
            batch.position_offset=buffer.getOffset(LLVertexBuffer::TYPE_VERTEX);batch.weight_offset=buffer.getOffset(LLVertexBuffer::TYPE_WEIGHT4);
            batch.vertex_count=buffer.getNumVerts();batch.index_count=info->mCount;batch.double_sided=true;
            batch.skin_palette_record=info->mAvatar->palette.getMetalAttachmentPalette();
            REQUIRE(batch.skin_palette_record);batch.skin_palette=batch.skin_palette_record->data();
            batch.skin_palette_float_count=batch.skin_palette_record->size();batch.skin_joint_count=joints;
            std::memcpy(batch.modelview_projection,model.getF32ptr(),sizeof(batch.modelview_projection));result.push_back(std::move(batch));
        }
        return result;
    }
    std::unique_ptr<ALMetalTexture> shadowTarget()
    {
        ALMetalTextureDescriptor descriptor;descriptor.width=descriptor.height=side;descriptor.array_length=2;
        descriptor.type=ALMetalTextureType::texture_2d_array;descriptor.format=ALMetalPixelFormat::r32_float;
        descriptor.usage=ALMetalTextureUsage::render_target|ALMetalTextureUsage::shader_read;
        auto result=ALMetalTexture::create(*device,descriptor,&error);INFO(error);REQUIRE(result);return result;
    }
};
}

TEST_CASE("Actual attachment captures share one palette through main probes and native shadow cameras",
          "[almetal][pipeline-buffer-publication][attachment-palette][PPD-243]")
{
    for(U32 family:{0u,1u,2u})for(U32 joints:{1u,15u,110u})
    {
        CAPTURE(family,joints);AttachmentPalettePublication f(family,joints,4);
        auto data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(data);data->beginFrame(1);
        auto library=data->builtinLibrary(&f.error);REQUIRE(library);
        auto main=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);auto probe=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
        REQUIRE(main);REQUIRE(probe);main->setFrameData(data);probe->setFrameData(data);
        Inputs::LLPipeline pipeline;std::array<GroupPublication::Image,3> images;
        for(U32 view=0;view<3;++view)
        {
            f.captureRigged(pipeline);auto& capture=*pipeline.mMetalWorldSimpleCapture;
            const auto* record=f.avatars[0]->palette.record.get();REQUIRE(record);
            for(const auto& batch:capture.batches)
            {CHECK(batch.skin_palette_record==record);CHECK(batch.skin_palette==record->data());}
            auto inputs=f.adopt(pipeline,1);INFO(f.error);REQUIRE(inputs);
            for(const auto& batch:std::span(inputs->scene().batches,inputs->scene().batch_count))
                CHECK(batch.skin_palette==record->data());
            auto target=(view?probe:main)->renderOwnedInputs(*inputs,&f.error);INFO(f.error);REQUIRE(target);
            images[view]=f.read(*target);f.reclaim(inputs,pipeline);
        }
        CHECK(images[0]==images[1]);CHECK(images[0]==images[2]);CHECK(f.seals()==1);
        auto batches=f.shadows();auto destination=f.shadowTarget();
        ALMetalWorldSpotShadowScene shadow;shadow.batches=batches.data();shadow.batch_count=batches.size();
        shadow.width=shadow.height=Publication::side;shadow.bulk_draws=false;
        // These are the actual native shadow API's directional depth-clamp
        // and spotlight states, not a claim to exercise the viewer collector.
        for(U32 camera=0;camera<2;++camera)
        {shadow.depth_clamp=camera==0;REQUIRE(main->renderSpotShadow(*destination,camera,shadow,&f.error));}
        std::vector<F32> depth(Publication::side*Publication::side);
        for(U32 camera=0;camera<2;++camera)
        {REQUIRE(destination->readback(*f.device,0,camera,{},depth.data(),Publication::side*sizeof(F32),&f.error));
         CHECK(std::any_of(depth.begin(),depth.end(),[](F32 value){return value<1.f;}));}
        CHECK(data->stats().record_publications==1);CHECK(data->stats().record_reuses>=19);CHECK(f.seals()==1);
        const auto serial=Inputs::sNativeSceneSerial;f.pose(1.1f);f.captureRigged(pipeline);
        CHECK(Inputs::sNativeSceneSerial==serial);CHECK(f.seals()==2);
        auto inputs=f.adopt(pipeline,1);REQUIRE(inputs);auto changed=main->renderOwnedInputs(*inputs,&f.error);REQUIRE(changed);
        CHECK_FALSE(f.read(*changed)==images[0]);f.reclaim(inputs,pipeline);
    }
}

TEST_CASE("Actual attachment publication preserves raw fallback and rejects mismatched record proofs",
          "[almetal][pipeline-buffer-publication][attachment-palette]")
{
    for(U32 family:{0u,1u,2u})
    {
        CAPTURE(family);AttachmentPalettePublication f(family,15);Inputs::LLPipeline pipeline;
        auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);REQUIRE(renderer);
        std::array<GroupPublication::Image,2> images;
        for(U32 mode=0;mode<2;++mode)
        {
            f.captureRigged(pipeline,mode!=0);const auto* original=pipeline.mMetalWorldSimpleCapture->batches[0].skin_palette;
            auto input=f.adopt(pipeline,mode+1);INFO(f.error);REQUIRE(input);
            CHECK((input->scene().batches[0].skin_palette==original)==(mode!=0));
            auto target=renderer->renderOwnedInputs(*input,&f.error);REQUIRE(target);images[mode]=f.read(*target);f.reclaim(input,pipeline);
        }
        CHECK(images[0]==images[1]);
        for(U32 mismatch=0;mismatch<4;++mismatch)
        {
            f.captureRigged(pipeline);auto& batch=pipeline.mMetalWorldSimpleCapture->batches[0];
            auto values=f.avatars[0]->palette.mGLMp;values[7]+=.125f;
            auto other=ALMetalAttachmentPaletteRecord::create(values.data(),values.size(),15,&f.error);REQUIRE(other);
            if(mismatch==0)batch.skin_palette_record=other.get();
            if(mismatch==1)--batch.skin_palette_float_count;
            if(mismatch==2)--batch.skin_joint_count;
            if(mismatch==3)batch.avatar_skinning=true;
            CHECK_FALSE(f.adopt(pipeline,3));
        }
        // Forward and PBR-glow published capture keep the same palette proof.
        // Their complete lighting/output cases belong to the backend fixture.
        f.captureRigged(pipeline,true,true,family==2);
        CHECK(pipeline.mMetalWorldSimpleCapture->batches[0].skin_palette_record==f.avatars[0]->palette.record.get());
    }
}

TEST_CASE("Actual attachment CPU palette can retire before its encoded native generation",
          "[almetal][pipeline-buffer-publication][attachment-palette][native-tuple]")
{
    AttachmentPalettePublication f(0,110);Inputs::LLPipeline pipeline;
    auto data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(data);data->beginFrame(1);
    auto library=data->builtinLibrary(&f.error);REQUIRE(library);
    auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);REQUIRE(renderer);renderer->setFrameData(data);
    f.captureRigged(pipeline);std::weak_ptr<const ALMetalAttachmentPaletteRecord> old=f.avatars[0]->palette.record;
    auto inputs=f.adopt(pipeline,1);REQUIRE(inputs);REQUIRE(renderer->beginCommandBatch("Actual attachment old palette",&f.error));
    auto target=renderer->renderOwnedInputs(*inputs,&f.error);INFO(f.error);REQUIRE(target);
    const auto completion=renderer->commandBatchCompletion();REQUIRE(completion);CHECK_FALSE(completion->isComplete());
    f.reclaim(inputs,pipeline);f.pose(1.1f);data->beginFrame(2);CHECK(old.expired());
    REQUIRE(renderer->commitCommandBatch(&f.error));REQUIRE(f.device->waitUntilIdle(&f.error));CHECK(completion->isComplete());
    const auto previous=f.read(*target);f.captureRigged(pipeline);inputs=f.adopt(pipeline,2);REQUIRE(inputs);
    auto current=renderer->renderOwnedInputs(*inputs,&f.error);REQUIRE(current);CHECK_FALSE(f.read(*current)==previous);f.reclaim(inputs,pipeline);
}

TEST_CASE("Actual attachment palette complete costs retain equal-byte identity tradeoffs",
          "[almetal][.native-attachment-palette-cost]")
{
    constexpr U32 samples=12,warmups=2;
    struct Sample
    {
        double cpu=0,encode=0,settle=0,gpu=0;
        U64 seals=0,source_publications=0,published=0,reused=0,record_publications=0,record_reuses=0,gpu_ready=0;
    };
    struct Arm
    {
        Inputs::LLPipeline pipeline;
        std::shared_ptr<ALMetalWorldFrameData> data;
        std::unique_ptr<ALMetalWorldGBufferRenderer> main,probe;
        std::unique_ptr<ALMetalTexture> shadow_target;
        U64 epoch=0;
    };
    const auto cpu_ns=[]
    {timespec value{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0);
     return U64(value.tv_sec)*1'000'000'000ULL+U64(value.tv_nsec);};
    const auto median=[](std::array<double,samples> values)
    {std::sort(values.begin(),values.end());return (values[samples/2-1]+values[samples/2])*.5;};
    struct TimingRestore
    {bool previous=ALMetalDevice::commandTimingEnabled();~TimingRestore(){ALMetalDevice::setCommandTimingEnabled(previous);}} timing_restore;
    ALMetalDevice::setCommandTimingEnabled(true);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(10);
    // Palette shape/record sharing is independent of material shading. The
    // normal cases above exercise all three families; this cost isolates the
    // default ordinary rigged SIMPLE path, with no experimental viewer flags.
    for(U32 joints:{1u,110u})for(U32 draws:{4u,64u})for(U32 distribution=0;distribution<3;++distribution)
    for(U32 views:{1u,3u})for(bool churn:{false,true})for(U32 shadow_views:{0u,2u})
    {
        CAPTURE(joints,draws,distribution,views,churn,shadow_views);
        // PPD246: NativeFont is a singleton owner. Share this host's immutable
        // geometry/source publications, with independent per-arm frame data
        // and renderers. Reseal producer records inside EACH timed arm below.
        AttachmentPalettePublication f(0,joints,draws,distribution!=0);
        f.pose(0.f,distribution==2);std::array<Arm,2> arms;
        for(auto& arm:arms)
        {
            arm.data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(arm.data);
            auto library=arm.data->builtinLibrary(&f.error);REQUIRE(library);
            arm.main=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);arm.probe=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
            REQUIRE(arm.main);REQUIRE(arm.probe);arm.main->setFrameData(arm.data);arm.probe->setFrameData(arm.data);
            arm.shadow_target=f.shadowTarget();
        }
        std::array<std::array<Sample,samples>,2> measurements;
        for(U32 sample=0;sample<samples+warmups;++sample)
        {
            ++Inputs::gFrameCount; // Same real outer-frame domain for both arms.
            std::array<std::array<GroupPublication::Image,3>,2> images;
            std::array<std::array<std::array<U64,2>,3>,2> query_values{};
            for(U32 position=0;position<2;++position)
            {
                REQUIRE(std::chrono::steady_clock::now()<deadline);
                const U32 mode=(sample+position)%2;auto& arm=arms[mode];CAPTURE(sample,mode);
                const auto before_data=arm.data->stats();const auto before_seals=f.seals();const auto before_source=Inputs::sNativeSceneSerial;
                std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> results;
                std::array<std::shared_ptr<const ALMetalCommandCompletion>,5> completions;
                std::shared_ptr<const void> native_owner;
                Sample value;const auto start=std::chrono::steady_clock::now();const auto cpu_start=cpu_ns();
                arm.data->beginFrame(++arm.epoch);
                // The paired arms share a viewer frame, but both must pay
                // actual Record::create rather than reuse the first arm's
                // CPU proof. Palette bytes remain unchanged in stable cases.
                for(auto& avatar:f.avatars)avatar->palette.invalidate();
                if(churn)f.pose(sample%2?.02f:0.f,distribution==2);
                for(U32 view=0;view<views;++view)
                {
                    std::array<F32,16> matrix{};matrix[0]=matrix[5]=matrix[10]=matrix[15]=1.f;matrix[12]=.01f*view;f.model.loadu(matrix.data());
                    // Both arms include the actual existing producer sealing
                    // and capture. Clearing only the new borrowed proof in
                    // mode0 selects the former raw adopt/hash/bind behavior.
                    f.captureRigged(arm.pipeline,mode!=0);
                    const auto* original=arm.pipeline.mMetalWorldSimpleCapture->batches[0].skin_palette;
                    std::vector<ALMetalWorldVisibilityQuery> queries(2);
                    for(U32 query=0;query<2;++query)
                    {auto& q=queries[query];std::memcpy(q.modelview_projection,f.model.getF32ptr(),sizeof(q.modelview_projection));
                     q.center[0]=.35f;q.center[1]=-.5f;q.center[2]=.75f;q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.08f;
                     q.batch_offset=query?draws:draws/2;q.fan=7;q.count_samples=1;}
                    auto inputs=f.adopt(arm.pipeline,arm.epoch,std::move(queries));INFO(f.error);REQUIRE(inputs);
                    CHECK((inputs->scene().batches[0].skin_palette==original)==(mode!=0));native_owner=inputs->resourceLease();
                    auto& renderer=view?*arm.probe:*arm.main;REQUIRE(renderer.beginCommandBatch("Actual attachment complete-path cost",&f.error));
                    targets[view]=renderer.renderOwnedInputs(*inputs,&f.error,&results[view]);INFO(f.error);REQUIRE(targets[view]);REQUIRE(results[view]);
                    completions[view]=renderer.commandBatchCompletion();f.reclaim(inputs,arm.pipeline);REQUIRE(renderer.commitCommandBatch(&f.error));
                }
                if(shadow_views)
                {
                    // Existing shadow identity publication is unchanged in
                    // BOTH arms. Main-only cases expose equal-byte adverse
                    // behavior without this later shared consumer masking it.
                    auto batches=f.shadows();ALMetalWorldSpotShadowScene scene;
                    scene.batches=batches.data();scene.batch_count=batches.size();scene.source_owner=native_owner;
                    scene.width=scene.height=Publication::side;scene.bulk_draws=false;
                    for(U32 camera=0;camera<shadow_views;++camera)
                    {scene.depth_clamp=camera==0;REQUIRE(arm.main->beginCommandBatch("Actual attachment shadow reuse cost",&f.error));
                     REQUIRE(arm.main->renderSpotShadow(*arm.shadow_target,camera,scene,&f.error));
                     completions[views+camera]=arm.main->commandBatchCompletion();REQUIRE(arm.main->commitCommandBatch(&f.error));}
                }
                const auto encoded=std::chrono::steady_clock::now();REQUIRE(f.device->waitUntilIdle(&f.error));
                for(U32 view=0;view<views;++view)for(U32 query=0;query<2;++query)
                {REQUIRE(results[view]->wait(query,query_values[mode][view][query],&f.error));CHECK(query_values[mode][view][query]>0);}
                gGL.frameFences().release();const auto settled=std::chrono::steady_clock::now();
                value.cpu=(cpu_ns()-cpu_start)/1e6;value.encode=std::chrono::duration<double,std::milli>(encoded-start).count();
                value.settle=std::chrono::duration<double,std::milli>(settled-start).count();
                for(U32 command=0;command<views+shadow_views;++command)
                {ALMetalCommandGPUTime gpu;if(completions[command]&&completions[command]->gpuTime(gpu)==ALMetalCommandTimingStatus::ready)
                    {value.gpu+=gpu.elapsed_nanoseconds/1e6;++value.gpu_ready;}}
                const auto after=arm.data->stats();value.published=after.published_bytes-before_data.published_bytes;value.reused=after.reused_bytes-before_data.reused_bytes;
                value.record_publications=after.record_publications-before_data.record_publications;value.record_reuses=after.record_reuses-before_data.record_reuses;
                value.seals=f.seals()-before_seals;value.source_publications=Inputs::sNativeSceneSerial-before_source;
                CHECK(value.seals==f.avatars.size());if(sample>=warmups)CHECK(value.source_publications==0);
                if(mode)CHECK(value.record_publications==f.avatars.size());
                if(sample>=warmups)measurements[mode][sample-warmups]=value;
                if(sample==warmups-1||sample==samples+warmups-1)
                    for(U32 view=0;view<views;++view)images[mode][view]=f.read(*targets[view]);
                std::cout<<"NATIVE_ATTACHMENT_PALETTE_RAW joints="<<joints<<" draws="<<draws<<" views="<<views<<" shadow_api_views="<<shadow_views
                    <<" distribution="<<distribution<<" unique_producer_records="<<f.avatars.size()<<" churn="<<churn<<" mode="<<mode
                    <<" sample="<<sample<<" warmup="<<(sample<warmups)<<" skeleton_packing=typed_input_frontier"
                    <<" source_geometry=shared_immutable frame_data=independent_arm producer_record=resealed_per_arm"
                    <<" capture_adopt_validate_encode_retire_cpu_ms="<<value.cpu<<" capture_adopt_encode_reclaim_ms="<<value.encode
                    <<" commit_and_query_settle_ms="<<value.settle<<" gpu_sum_ms="<<value.gpu<<" gpu_ready="<<value.gpu_ready
                    <<" producer_record_seals="<<value.seals<<" source_publications="<<value.source_publications
                    <<" frame_data_published_logical_bytes="<<value.published<<" frame_data_reused_logical_bytes="<<value.reused
                    <<" record_publications="<<value.record_publications<<" record_reuses="<<value.record_reuses<<'\n';
            }
            for(U32 view=0;view<views;++view)
            {CHECK(query_values[0][view]==query_values[1][view]);
             if(sample==warmups-1||sample==samples+warmups-1)CHECK(images[0][view]==images[1][view]);}
        }
        for(U32 mode=0;mode<2;++mode)
        {
            std::array<double,samples> cpu,encode,settle,gpu;U64 bytes=0,publications=0,reuses=0;
            for(U32 sample=0;sample<samples;++sample)
            {const auto& value=measurements[mode][sample];cpu[sample]=value.cpu;encode[sample]=value.encode;settle[sample]=value.settle;gpu[sample]=value.gpu;
             bytes+=value.published;publications+=value.record_publications;reuses+=value.record_reuses;}
            std::cout<<"NATIVE_ATTACHMENT_PALETTE_COST joints="<<joints<<" draws="<<draws<<" views="<<views<<" shadow_api_views="<<shadow_views
                <<" distribution="<<distribution<<" unique_producer_records="<<f.avatars.size()<<" churn="<<churn<<" mode="<<mode
                <<" skeleton_packing=typed_input_frontier source_geometry=shared_immutable frame_data=independent_arm producer_record=resealed_per_arm"
                <<" samples="<<samples<<" warmups="<<warmups<<" cpu_median_ms="<<median(cpu)<<" cpu_max_ms="<<*std::max_element(cpu.begin(),cpu.end())
                <<" encode_reclaim_median_ms="<<median(encode)<<" encode_reclaim_max_ms="<<*std::max_element(encode.begin(),encode.end())
                <<" commit_settle_median_ms="<<median(settle)<<" commit_settle_max_ms="<<*std::max_element(settle.begin(),settle.end())
                <<" gpu_sum_median_ms="<<median(gpu)<<" gpu_sum_max_ms="<<*std::max_element(gpu.begin(),gpu.end())
                <<" frame_data_published_logical_bytes="<<bytes<<" record_publications="<<publications<<" record_reuses="<<reuses<<'\n';
        }
    }
}
