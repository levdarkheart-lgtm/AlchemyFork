/**
 * @file alpipeline_body_palette_native.inl
 * @brief Actual view-palette composition/capture and complete owned-path costs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../../newview/alavatarpose.h"
#include "../../newview/pipeline_body_palette_capture_native.inl"

namespace BodyPaletteActual
{
using Pose = ALMetalAvatarPoseRecord;
using Record = ALMetalBodyPaletteRecord;
struct Mesh
{
    std::shared_ptr<const Pose> pose;
    LLMatrix4a view=LLMatrix4a::identity();
    mutable U64 compositions=0;
    bool buildJointMatrixPalette(F32* values,std::size_t count) const
    {++compositions;return pose && ALAvatarPose::compose(*pose,view,values,count);}
};
struct Host : GroupPublication
{
    std::vector<Mesh> meshes;
    std::vector<std::unique_ptr<std::array<F32,180>>> raw;
    U64 seals=0,pose_seals=0;
    bool distinct=false;
    Host(U32 count,bool distinct_inputs=false):meshes(distinct_inputs?count:1),distinct(distinct_inputs)
    {
        const U32 columns=U32(std::ceil(std::sqrt(F32(count))));
        for(U32 at=0;at<count;++at)
        {
            auto& info=draw();info.mVertexBuffer=buffer(true,LLVertexBuffer::MAP_WEIGHT|LLVertexBuffer::MAP_CLOTHWEIGHT);
            LLStrider<F32> weights;REQUIRE(info.mVertexBuffer->getWeightStrider(weights));
            LLStrider<LLVector4> clothing;REQUIRE(info.mVertexBuffer->getClothWeightStrider(clothing));
            LLStrider<LLVector3> positions;REQUIRE(info.mVertexBuffer->getVertexStrider(positions));
            const F32 edge=1.6f/columns,x=-.8f+(at%columns)*edge,y=-.8f+(at/columns)*edge;
            positions[0].set(x,y,.125f);positions[1].set(x+edge*.8f,y,.125f);positions[2].set(x,y+edge*.8f,.125f);
            for(U32 vertex=0;vertex<3;++vertex)
            {weights[vertex]=F32((at+vertex)%14)+.5f;clothing[vertex].set(.015625f,.03125f,.0078125f,.5f);}
            info.mVertexBuffer->unmapBuffer();
        }
        updatePose(0,1);
    }
    void updatePose(F32 translation,U64 frame,bool unique_values=false,U64 mesh_revision=1,U64 lifecycle_revision=1)
    {
        for(U32 at=0;at<meshes.size();++at)
        {
            Pose::Inputs input;input.joint_count=input.pivot_count=15;input.frame=frame;
            input.pose_revision=frame;input.mesh_revision=mesh_revision;input.lifecycle_revision=lifecycle_revision;
            for(U32 joint=0;joint<15;++joint)
            {auto& world=input.world_matrices[joint];world[0]=world[5]=world[10]=world[15]=1;
             world[12]=translation+(unique_values?F32(at)*.0001f:0.f);input.pivots[joint]={.0001f*joint,0,0};}
            meshes[at].pose=Pose::create(input,&error);REQUIRE(meshes[at].pose);++pose_seals;
        }
    }
    void setView(U32 view)
    {for(auto& mesh:meshes)mesh.view=LLMatrix4a::translation(.01f*view,-.005f*view,0.f);}
    U64 compositions() const
    {U64 count=0;for(const auto& mesh:meshes)count+=mesh.compositions;return count;}
    void capture(Inputs::LLPipeline& pipeline,bool certified,bool cloth_enabled=false)
    {
        auto& capture=*pipeline.mMetalWorldSimpleCapture;capture.clearDraws();capture.complete=true;
        capture.compact_gbuffer_active=false;capture.beginSourceRows(false);
        capture.world_resources=std::make_shared<Inputs::WorldResources>();raw.clear();
        for(U32 at=0;at<infos.size();++at)
        {
            auto& buffer=*infos[at]->mVertexBuffer;REQUIRE(buffer.prepareMetalBuffers(false));
            ALMetalWorldSimpleBatch batch;batch.vertices=buffer.getMetalVertexBuffer();batch.indices=buffer.getMetalIndexBuffer();
            batch.position_offset=buffer.getOffset(LLVertexBuffer::TYPE_VERTEX);batch.normal_offset=buffer.getOffset(LLVertexBuffer::TYPE_NORMAL);
            batch.texcoord_offset=buffer.getOffset(LLVertexBuffer::TYPE_TEXCOORD0);batch.weight_offset=buffer.getOffset(LLVertexBuffer::TYPE_WEIGHT);
            batch.vertex_count=buffer.getNumVerts();batch.index_count=infos[at]->mCount;
            batch.texture_count=1;batch.diffuse_textures[0]=white->getMetalTexture();batch.avatar_skinning=true;
            batch.alpha_mask=true;batch.use_vertex_color=false;batch.alpha_cutoff_bias=false;batch.alpha_cutoff=.2f;
            batch.avatar_cull_mode=ALMetalCullMode::none;batch.skin_joint_count=15;batch.skin_palette_float_count=180;
            for(U32 axis=0;axis<4;++axis)
            {batch.modelview_projection[axis*5]=batch.modelview[axis*5]=1;
             if(axis<3)batch.normal_matrix[axis][axis]=batch.rigged_normal_matrix[axis][axis]=1;}
            auto& mesh=meshes[distinct?at:0];
            if(certified)
            {
                // Actual production stack-compose -> sealed-record body. It
                // returns a new view-specific identity for EVERY captured
                // draw; sharing a source pose is not a fake record cache.
                auto record=ALMetalBodyPaletteCaptureNative::capture(mesh);REQUIRE(record);++seals;
                capture.retainCpu(record);batch.body_palette_record=record.get();batch.skin_palette=record->data();
            }
            else
            {
                // Exact former capture allocation. The old arm does NOT pay
                // new record construction before its adoption snapshot.
                auto values=std::make_unique<std::array<F32,180>>();
                REQUIRE(mesh.buildJointMatrixPalette(values->data(),values->size()));
                batch.skin_palette=values->data();raw.push_back(std::move(values));
            }
            if(cloth_enabled)
            {
                batch.avatar_cloth=true;batch.cloth_offset=buffer.getOffset(LLVertexBuffer::TYPE_CLOTHWEIGHT);
                batch.avatar_wind[0]=.25f;batch.avatar_wind[2]=.5f;batch.avatar_wind[3]=.375f;
                batch.avatar_sin_wave[0]=1;batch.avatar_sin_wave[1]=2;batch.avatar_sin_wave[2]=.5f;
                batch.avatar_gravity[2]=-1;batch.avatar_cloth_depth[2]=1;
            }
            capture.retainGeometry(buffer);capture.world_resources->retain(white->getMetalTextureLease());
            capture.batches.push_back(std::move(batch));
        }
    }
    std::shared_ptr<const ALMetalWorldViewInputs> adopt(Inputs::LLPipeline& pipeline,U64 epoch,bool reverse=false)
    {
        auto& capture=*pipeline.mMetalWorldSimpleCapture;auto value=scene(capture);value.reverse_z=reverse;value.hdr=value.normal_hdr=true;
        std::vector<ALMetalWorldVisibilityQuery> queries(2);
        for(U32 at=0;at<2;++at)
        {auto& query=queries[at];query.modelview_projection[0]=query.modelview_projection[5]=query.modelview_projection[10]=query.modelview_projection[15]=1;
         query.center[2]=reverse?.75f:-.75f;query.half_extent[0]=query.half_extent[1]=.4f;query.half_extent[2]=.0625f;
         query.batch_offset=at?infos.size():infos.size()/2;query.fan=7;query.count_samples=1;}
        return ALMetalWorldViewInputs::adopt(value,std::move(capture.batches),std::move(capture.source_references),
            std::move(capture.draw_order),std::move(capture.source_cohorts),std::move(queries),
            std::move(capture.cpu_sources),capture.world_resources,epoch,&error);
    }
    void reclaim(std::shared_ptr<const ALMetalWorldViewInputs>& input,Inputs::LLPipeline& pipeline)
    {
        auto& capture=*pipeline.mMetalWorldSimpleCapture;std::vector<ALMetalWorldVisibilityQuery> queries;
        REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(input,capture.batches,capture.source_references,
            capture.draw_order,capture.source_cohorts,queries));
        capture.clearDraws();capture.world_resources.reset();raw.clear();
    }
};
}

TEST_CASE("Actual body capture composes each view without geometry or material republication",
          "[almetal][pipeline-buffer-publication][body-palette][PPD-282]")
{
    using namespace BodyPaletteActual;
    Host f(4,true);Inputs::LLPipeline pipeline;
    auto data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(data);data->beginFrame(1);
    auto library=data->builtinLibrary(&f.error);REQUIRE(library);
    auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);REQUIRE(renderer);renderer->setFrameData(data);
    const U64 source_serial=Inputs::sNativeSceneSerial;
    std::vector<U64> revisions;for(const auto& info:f.infos)revisions.push_back(info->state->revision());
    for(bool clothed:{false,true})for(U32 view=0;view<3;++view)
    {
        CAPTURE(clothed,view);f.setView(view);std::array<GroupPublication::Image,2> images;
        std::array<std::array<U64,2>,2> queries;
        for(U32 mode=0;mode<2;++mode)
        {
            const auto before=f.compositions();f.capture(pipeline,mode!=0,clothed);CHECK(f.compositions()-before==f.infos.size());
            const auto* original=pipeline.mMetalWorldSimpleCapture->batches[0].skin_palette;
            if(mode)for(const auto& batch:pipeline.mMetalWorldSimpleCapture->batches)
            {CHECK(batch.matchesBodyPaletteRecord());CHECK(batch.body_palette_record->size()==180);}
            auto input=f.adopt(pipeline,1,view==2);INFO(f.error);REQUIRE(input);
            CHECK((input->scene().batches[0].skin_palette==original)==(mode!=0));
            std::shared_ptr<ALMetalVisibilityResultBatch> results;auto target=renderer->renderOwnedInputs(*input,&f.error,&results);
            INFO(f.error);REQUIRE(target);REQUIRE(results);images[mode]=f.read(*target);
            for(U32 at=0;at<2;++at){REQUIRE(results->wait(at,queries[mode][at],&f.error));CHECK(queries[mode][at]>0);}
            f.reclaim(input,pipeline);
        }
        CHECK(images[0]==images[1]);CHECK(queries[0]==queries[1]);
    }
    CHECK(Inputs::sNativeSceneSerial==source_serial);
    for(U32 at=0;at<f.infos.size();++at)CHECK(f.infos[at]->state->revision()==revisions[at]);
    // Same body layout, but exact pose, view, mesh binding and lifecycle each
    // create a fresh immutable identity. Different cameras are not reused.
    f.setView(0);f.capture(pipeline,true);auto old=pipeline.mMetalWorldSimpleCapture->batches[0].body_palette_record->owner();REQUIRE(old);
    std::cout<<"NATIVE_BODY_PALETTE_LAYOUT batch_bytes="<<sizeof(ALMetalWorldSimpleBatch)
        <<" shadow_batch_bytes="<<sizeof(ALMetalWorldSpotShadowBatch)
        <<" record_owned_bytes="<<old->ownedBytes()
        <<" raw_palette_value_bytes="<<180*sizeof(float)
        <<" record_allocator_control_block_excluded=1 static_source_caps_unchanged=1\n";
    const std::array<F32,180> original=[&]{std::array<F32,180> result;std::copy_n(old->data(),180,result.begin());return result;}();
    for(U32 change=0;change<4;++change)
    {
        f.updatePose(change==0?.125f:0.f,change+2,true,change==2?2:1,change==3?2:1);f.setView(change==1?2:0);
        f.capture(pipeline,true);const auto& batch=pipeline.mMetalWorldSimpleCapture->batches[0];
        CHECK(batch.body_palette_record!=old.get());CHECK(batch.matchesBodyPaletteRecord());
        if(change<2)CHECK_FALSE(std::equal(original.begin(),original.end(),batch.skin_palette));
        CHECK(std::equal(original.begin(),original.end(),old->data()));
    }
}

TEST_CASE("Actual body palette complete costs include camera composition and old raw snapshots",
          "[almetal][.native-body-palette-cost]")
{
    using namespace BodyPaletteActual;
    constexpr U32 samples=12,warmups=2;
    struct Sample
    {double cpu=0,encode=0,settle=0,gpu=0;U64 compositions=0,body_seals=0,pose_seals=0,published=0,reused=0,publications=0,hits=0,gpu_ready=0;};
    struct Arm
    {Inputs::LLPipeline pipeline;std::shared_ptr<ALMetalWorldFrameData> data;
     std::unique_ptr<ALMetalWorldGBufferRenderer> main,probe;U64 epoch=0;};
    const auto cpu=[] {timespec now{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&now)==0);return U64(now.tv_sec)*1'000'000'000ULL+U64(now.tv_nsec);};
    const auto median=[](std::array<double,samples> data){std::sort(data.begin(),data.end());return (data[5]+data[6])*.5;};
    struct TimingRestore{bool old=ALMetalDevice::commandTimingEnabled();~TimingRestore(){ALMetalDevice::setCommandTimingEnabled(old);}} restore;
    ALMetalDevice::setCommandTimingEnabled(true);const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(8);
    for(U32 draws:{1u,4u,64u})for(bool distinct:{false,true})for(U32 views:{1u,3u})for(bool churn:{false,true})
    {
        CAPTURE(draws,distinct,views,churn);Host f(draws,distinct);std::array<Arm,2> arms;
        // One NativeFont/UI host and immutable native geometry; CPU/world pose
        // and view composition are refreshed within EACH rotating timed arm.
        for(auto& arm:arms)
        {
            arm.data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(arm.data);auto library=arm.data->builtinLibrary(&f.error);REQUIRE(library);
            arm.main=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);arm.probe=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
            REQUIRE(arm.main);REQUIRE(arm.probe);arm.main->setFrameData(arm.data);arm.probe->setFrameData(arm.data);
        }
        std::array<std::array<Sample,samples>,2> measured;
        for(U32 sample=0;sample<samples+warmups;++sample)
        {
            ++Inputs::gFrameCount;std::array<std::array<GroupPublication::Image,3>,2> images;
            std::array<std::array<std::array<U64,2>,3>,2> query_values{};std::array<Sample,2> pair;
            for(U32 position=0;position<2;++position)
            {
                REQUIRE(std::chrono::steady_clock::now()<deadline);const U32 mode=(sample+position)%2;auto& arm=arms[mode];CAPTURE(sample,mode);
                const auto before=arm.data->stats();const auto composed=f.compositions(),sealed=f.seals,poses=f.pose_seals;
                std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> results;
                std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                Sample value;const auto started=std::chrono::steady_clock::now();const auto cpu_start=cpu();
                arm.data->beginFrame(++arm.epoch);f.updatePose(churn&&sample%2?.03125f:0.f,Inputs::gFrameCount);
                for(U32 view=0;view<views;++view)
                {
                    f.setView(view);f.capture(arm.pipeline,mode!=0);const auto* original=arm.pipeline.mMetalWorldSimpleCapture->batches[0].skin_palette;
                    auto input=f.adopt(arm.pipeline,arm.epoch);INFO(f.error);REQUIRE(input);
                    CHECK((input->scene().batches[0].skin_palette==original)==(mode!=0));
                    auto& renderer=view?*arm.probe:*arm.main;REQUIRE(renderer.beginCommandBatch("Actual body palette complete cost",&f.error));
                    targets[view]=renderer.renderOwnedInputs(*input,&f.error,&results[view]);INFO(f.error);REQUIRE(targets[view]);REQUIRE(results[view]);
                    completions[view]=renderer.commandBatchCompletion();f.reclaim(input,arm.pipeline);REQUIRE(renderer.commitCommandBatch(&f.error));
                }
                const auto encoded=std::chrono::steady_clock::now();REQUIRE(f.device->waitUntilIdle(&f.error));
                for(U32 view=0;view<views;++view)for(U32 at=0;at<2;++at)
                {REQUIRE(results[view]->wait(at,query_values[mode][view][at],&f.error));CHECK(query_values[mode][view][at]>0);}
                gGL.frameFences().release();const auto settled=std::chrono::steady_clock::now();value.cpu=(cpu()-cpu_start)/1e6;
                value.encode=std::chrono::duration<double,std::milli>(encoded-started).count();value.settle=std::chrono::duration<double,std::milli>(settled-started).count();
                for(U32 view=0;view<views;++view)
                {ALMetalCommandGPUTime gpu;if(completions[view]&&completions[view]->gpuTime(gpu)==ALMetalCommandTimingStatus::ready)
                    {value.gpu+=gpu.elapsed_nanoseconds/1e6;++value.gpu_ready;}}
                const auto after=arm.data->stats();value.compositions=f.compositions()-composed;value.body_seals=f.seals-sealed;value.pose_seals=f.pose_seals-poses;
                value.published=after.published_bytes-before.published_bytes;value.reused=after.reused_bytes-before.reused_bytes;
                value.publications=after.body_record_publications-before.body_record_publications;value.hits=after.body_record_reuses-before.body_record_reuses;
                CHECK(value.compositions==draws*views);CHECK(value.body_seals==(mode?draws*views:0));CHECK(value.pose_seals==f.meshes.size());
                CHECK(value.publications==(mode?draws*views:0));pair[mode]=value;
                if(sample>=warmups)measured[mode][sample-warmups]=value;
                if(sample==warmups-1||sample==samples+warmups-1)for(U32 view=0;view<views;++view)images[mode][view]=f.read(*targets[view]);
                std::cout<<"NATIVE_BODY_PALETTE_RAW draws="<<draws<<" views="<<views<<" distinct_equal_pose_inputs="<<distinct<<" churn="<<churn
                    <<" mode="<<mode<<" sample="<<sample<<" warmup="<<(sample<warmups)<<" palette_compositions_per_draw=1 candidate_record_identity_per_draw="<<mode
                    <<" shared_pose_input="<<!distinct<<" actual_capture_palette_body=1"
                    <<" mutable_skeleton=typed_pose_input geometry=shared_immutable frame_data=independent_arm target_wrapper_retirement=outside_timer"
                    <<" owner_cpu_ms="<<value.cpu<<" capture_adopt_encode_reclaim_ms="<<value.encode<<" commit_query_settle_ms="<<value.settle
                    <<" gpu_sum_ms="<<value.gpu<<" gpu_ready="<<value.gpu_ready<<" compositions="<<value.compositions<<" body_record_seals="<<value.body_seals
                    <<" world_pose_seals="<<value.pose_seals<<" body_record_publications="<<value.publications<<" body_record_reuses="<<value.hits
                    <<" frame_data_published_logical_bytes="<<value.published<<" frame_data_reused_logical_bytes="<<value.reused<<'\n';
            }
            CHECK(pair[1].published<=pair[0].published); // Equal-byte identities must not amplify native publication.
            for(U32 view=0;view<views;++view)
            {CHECK(query_values[0][view]==query_values[1][view]);if(sample==warmups-1||sample==samples+warmups-1)CHECK(images[0][view]==images[1][view]);}
        }
        for(U32 mode=0;mode<2;++mode)
        {
            std::array<double,samples> cpu_values,encode,settle,gpu;U64 publications=0,bytes=0;
            for(U32 sample=0;sample<samples;++sample)
            {const auto& value=measured[mode][sample];cpu_values[sample]=value.cpu;encode[sample]=value.encode;settle[sample]=value.settle;gpu[sample]=value.gpu;
             publications+=value.publications;bytes+=value.published;}
            std::cout<<"NATIVE_BODY_PALETTE_COST draws="<<draws<<" views="<<views<<" distinct_equal_pose_inputs="<<distinct<<" churn="<<churn<<" mode="<<mode
                <<" samples="<<samples<<" warmups="<<warmups<<" mutable_skeleton=typed_pose_input palette_compositions_per_draw=1 candidate_record_identity_per_draw="<<mode
                <<" shared_pose_input="<<!distinct
                <<" target_wrapper_retirement=outside_timer owner_cpu_median_ms="<<median(cpu_values)<<" owner_cpu_max_ms="<<*std::max_element(cpu_values.begin(),cpu_values.end())
                <<" encode_reclaim_median_ms="<<median(encode)<<" encode_reclaim_max_ms="<<*std::max_element(encode.begin(),encode.end())
                <<" settle_median_ms="<<median(settle)<<" settle_max_ms="<<*std::max_element(settle.begin(),settle.end())
                <<" gpu_sum_median_ms="<<median(gpu)<<" gpu_sum_max_ms="<<*std::max_element(gpu.begin(),gpu.end())
                <<" body_record_publications="<<publications<<" frame_data_published_logical_bytes="<<bytes<<'\n';
        }
    }
}
