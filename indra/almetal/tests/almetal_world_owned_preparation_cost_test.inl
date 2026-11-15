/**
 * @file almetal_world_owned_preparation_cost_test.inl
 * @brief Equal owned-input frontiers for scalar and CPU-prepared world costs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after almetal_world_prepare_fixture.h, in its native test TU.
 */
#include "../almetalworldmaterialdata.h"
#include <atomic>
#include <time.h>

namespace
{
namespace OwnedPreparationCost
{
std::uint64_t cpuClock(bool& success) noexcept
{
    timespec value{};
    success=clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0;
    return success?std::uint64_t(value.tv_sec)*1'000'000'000ULL+std::uint64_t(value.tv_nsec):0;
}
std::uint64_t ownerClock()
{bool success=false;const auto result=cpuClock(success);REQUIRE(success);return result;}
struct Arm
{
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::unique_ptr<ALMetalWorldGBufferRenderer> main,probe;
    ALNativeFrameBudget budget;
    std::shared_ptr<std::atomic<std::uint64_t>> current=std::make_shared<std::atomic<std::uint64_t>>(0);
    std::vector<ALMetalWorldSimpleBatch> batches;
    std::vector<ALMetalWorldVisibilityQuery> queries;
    std::array<float,180> body_pose{};
    std::uint64_t epoch=0;
};
struct Sample
{
    double owner_cpu=0,worker_cpu=0,capture_adopt=0,preparation=0,encode=0,complete=0,gpu=0;
    std::uint64_t worker_views=0,fallback_views=0,reclaimed=0,prepared_records=0,constant_bytes=0,uncompacted=0;
    std::uint64_t shared_common=0,native_bytes=0,raw_pose_bytes=0,credits=0,denials=0,commits=0,gpu_ready=0;
};
struct Worker
{
    ALMetalWorldPreparation* preparation=nullptr;
    bool result=false,clock_ok=false;
    std::uint64_t cpu_ns=0;
};
void runWorker(void* context) noexcept
{
    auto& worker=*static_cast<Worker*>(context);bool first=false,last=false;
    const auto begin=cpuClock(first);worker.result=worker.preparation->runJob(1);
    const auto end=cpuClock(last);worker.cpu_ns=end-begin;worker.clock_ok=first&&last&&end>=begin;
}
std::vector<ALMetalWorldSimpleBatch> sources(Fixture& fixture,unsigned count,bool mixed,
    ALMetalWorldMaterialDataCache& materials,const std::shared_ptr<const ALMetalAttachmentPaletteRecord>& attachment)
{
    constexpr unsigned tiles=11;
    std::array<Fixture::Geometry,tiles> geometry;
    for(unsigned tile=0;tile<tiles;++tile)
    {
        geometry[tile]=fixture.geometry;
        for(unsigned vertex=0;vertex<4;++vertex)
        {
            geometry[tile].positions[vertex][0]=fixture.geometry.positions[vertex][0]*.2f+(int(tile%4)-1.5f)*.45f;
            geometry[tile].positions[vertex][1]=fixture.geometry.positions[vertex][1]*.2f+(int(tile/4)-1)*.6f;
            geometry[tile].positions[vertex][2]=.15f+.035f*tile;
            geometry[tile].colors[vertex][0]=static_cast<std::uint8_t>(55+tile*17);
            geometry[tile].colors[vertex][1]=static_cast<std::uint8_t>(225-tile*13);
            geometry[tile].colors[vertex][2]=static_cast<std::uint8_t>(75+tile*11);
        }
    }
    const auto* vertices=fixture.buffer(geometry.data(),sizeof(geometry));
    std::array<ALMetalWorldSimpleBatch,7> prototypes;
    for(unsigned family=0;family<7;++family)
    {
        prototypes[family]=fixture.batch(family);
        if(family==4)
        {
            prototypes[family].skin_palette_record=attachment.get();
            prototypes[family].skin_palette=attachment->data();
        }
        if(family==1 || family==2)
        {
            auto owner=ALMetalWorldMaterialSource::publish(*fixture.device,prototypes[family],materials,&fixture.error);
            INFO(fixture.error);REQUIRE(owner);
            for(const auto& buffer:{owner->materialLease(),owner->pbrLease(false),owner->pbrLease(true)})
                if(buffer && std::none_of(fixture.resources->buffers.begin(),fixture.resources->buffers.end(),
                    [&](const auto& previous){return previous.get()==buffer.get() && !previous.owner_before(buffer) && !buffer.owner_before(previous);}))
                    fixture.resources->buffers.push_back(buffer);
        }
    }
    std::vector<ALMetalWorldSimpleBatch> result;result.reserve(count);
    for(unsigned at=0;at<count;++at)
    {
        const unsigned family=mixed?at%7:1+at%2;
        auto batch=prototypes[family];const auto offset=std::size_t(at%tiles)*sizeof(Fixture::Geometry);batch.vertices=vertices;
        batch.position_offset+=offset;batch.normal_offset+=offset;batch.texcoord_offset+=offset;
        batch.texcoord1_offset+=offset;batch.texcoord2_offset+=offset;batch.color_offset+=offset;batch.tangent_offset+=offset;
        if(family==4 || family==6)batch.weight_offset+=offset;
        result.push_back(std::move(batch));
    }
    return result;
}
}
}

TEST_CASE("almetal: owned scalar serial and credited world preparation complete costs",
          "[almetal][.world-owned-prepare-cost][PPD-263]")
{
    @autoreleasepool
    {
        using OwnedPreparationCost::Sample;
        Fixture fixture;auto library=fixture.data->builtinLibrary(&fixture.error);REQUIRE(library);
        fixture.renderer.reset();fixture.data.reset();
        auto attachment=ALMetalAttachmentPaletteRecord::create(fixture.attachment_pose.data(),fixture.attachment_pose.size(),1,&fixture.error);
        REQUIRE(attachment);ALMetalWorldMaterialDataCache materials(*fixture.device);
        constexpr unsigned samples=12,warmups=2;
        struct Restore
        {bool timing=ALMetalDevice::commandTimingEnabled();~Restore(){ALMetalDevice::setCommandTimingEnabled(timing);}} restore;
        ALMetalDevice::setCommandTimingEnabled(true);
        const auto median=[](std::array<double,samples> values){std::sort(values.begin(),values.end());return (values[5]+values[6])*.5;};
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(10);
        // Nine shape/view combinations, each with available and genuinely
        // occupied shared CPU credits:18 conditions/54 arm summaries.
        for(bool mixed:{false,true})for(unsigned count:{128u,1024u,4096u})for(unsigned views:{1u,3u})
        {
            if(views==3 && ((!mixed && count!=4096) || (mixed && count==128)))continue;
            const auto prototypes=OwnedPreparationCost::sources(fixture,count,mixed,materials,attachment);
            for(bool occupied:{false,true})
            {
                CAPTURE(mixed,count,views,occupied);std::array<OwnedPreparationCost::Arm,3> arms;
                for(auto& arm:arms)
                {
                    arm.data=ALMetalWorldFrameData::create(*fixture.device,&fixture.error);REQUIRE(arm.data);
                    arm.main=ALMetalWorldGBufferRenderer::create(*fixture.device,1,library,&fixture.error);
                    arm.probe=ALMetalWorldGBufferRenderer::create(*fixture.device,1,library,&fixture.error);
                    INFO(fixture.error);REQUIRE(arm.main);REQUIRE(arm.probe);
                    arm.main->setFrameData(arm.data);arm.probe->setFrameData(arm.data);
                }
                std::array<std::array<Sample,samples>,3> recorded{};
                for(unsigned sample=0;sample<samples+warmups;++sample)
                {
                    std::array<std::array<Pixels,3>,3> pixels;
                    std::array<std::array<std::array<std::uint64_t,3>,3>,3> query_values{};
                    for(unsigned position=0;position<3;++position)
                    {
                        REQUIRE(std::chrono::steady_clock::now()<deadline);
                        const unsigned mode=(sample+position)%3;auto& arm=arms[mode];CAPTURE(sample,mode);
                        ALNativeFrameBudget::FrameInputs plan;plan.frame=++arm.epoch;plan.frame_interval_ns=16'666'667;arm.budget.beginFrame(plan);
                        std::array<ALNativeFrameBudget::Lease,2> held;
                        if(occupied)for(auto& credit:held)
                        {credit=arm.budget.tryAcquire(ALNativeFrameBudget::Work::preparation).credit;REQUIRE(credit);}
                        const auto budget_before=arm.budget.snapshot();const auto native_before=arm.data->stats().published_bytes;
                        const auto commits=ALMetalDevice::committedCommandBufferCount();
                        std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;
                        std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> results;
                        std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;
                        Sample value;const auto started=std::chrono::steady_clock::now();const auto cpu_started=OwnedPreparationCost::ownerClock();
                        arm.current->store(arm.epoch,std::memory_order_release);arm.data->beginFrame(arm.epoch);
                        for(unsigned view=0;view<views;++view)
                        {
                            const auto capture_begin=std::chrono::steady_clock::now();arm.batches.assign(prototypes.begin(),prototypes.end());
                            auto scene=fixture.scene();scene.hdr=true;scene.normal_hdr=view%2!=0;scene.reverse_z=view%2!=0;
                            const float x=.0125f*float(view+1),y=view%2?-.0075f:.0075f;
                            arm.body_pose=fixture.avatar_pose;
                            for(unsigned joint=0;joint<15;++joint)arm.body_pose[joint*4+3]=x*.25f;
                            for(auto& batch:arm.batches)
                            {
                                batch.modelview_projection[12]=batch.modelview[12]=x;
                                batch.modelview_projection[13]=batch.modelview[13]=y;
                                if(batch.avatar_skinning){batch.skin_palette=arm.body_pose.data();value.raw_pose_bytes+=sizeof(arm.body_pose);}
                            }
                            arm.queries.resize(3);
                            for(unsigned query=0;query<3;++query)
                            {
                                auto& q=arm.queries[query];q={};identity(q.modelview_projection);q.modelview_projection[12]=x;q.modelview_projection[13]=y;
                                q.center[0]=.7f;q.center[1]=-.65f;q.center[2]=.75f;
                                q.half_extent[0]=q.half_extent[1]=q.half_extent[2]=.05f;
                                q.batch_offset=query==0?0:query==1?count/2:count;q.fan=7;q.count_samples=1;
                            }
                            scene.batches=arm.batches.data();scene.batch_count=arm.batches.size();
                            scene.visibility_queries=arm.queries.data();scene.visibility_query_count=arm.queries.size();
                            auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(arm.batches),std::move(arm.queries),fixture.resources,arm.epoch,&fixture.error);
                            INFO(fixture.error);REQUIRE(inputs);scene={};
                            if(mixed)
                            {
                                CHECK(inputs->scene().batches[6].skin_palette!=arm.body_pose.data());
                                CHECK(inputs->scene().batches[6].skin_palette_float_count==180);
                                CHECK(inputs->scene().batches[4].skin_palette==attachment->data());
                            }
                            value.capture_adopt+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-capture_begin).count();
                            std::shared_ptr<const ALMetalPreparedWorldView> prepared;
                            if(mode)
                            {
                                const auto preparation_begin=std::chrono::steady_clock::now();
                                auto work=ALMetalWorldPreparation::create(inputs,mode==1?1:2,arm.current,&fixture.error);REQUIRE(work);
                                ALNativeFrameBudget::Lease credit;
                                if(mode==2)credit=arm.budget.tryAcquire(ALNativeFrameBudget::Work::preparation).credit;
                                if(credit)
                                {
                                    OwnedPreparationCost::Worker worker;worker.preparation=work.get();auto group=dispatch_group_create();
                                    dispatch_group_async_f(group,dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),&worker,&OwnedPreparationCost::runWorker);
                                    const bool first=work->runJob(0);dispatch_group_wait(group,DISPATCH_TIME_FOREVER);
#if !OS_OBJECT_USE_OBJC
                                    dispatch_release(group);
#endif
                                    REQUIRE(first);REQUIRE(worker.result);REQUIRE(worker.clock_ok);value.worker_cpu+=worker.cpu_ns/1e6;++value.worker_views;
                                }
                                else
                                {
                                    if(mode==2)++value.fallback_views;
                                    for(std::size_t job=0;job<work->jobCount();++job)REQUIRE(work->runJob(job));
                                }
                                prepared=work->finish(&fixture.error);INFO(fixture.error);REQUIRE(prepared);
                                value.prepared_records+=prepared->stats().prepared_count;value.constant_bytes+=prepared->stats().constant_bytes;
                                value.uncompacted+=prepared->stats().uncompacted_constant_bytes;value.shared_common+=prepared->stats().shared_common_records;
                                credit.reset();work.reset();
                                value.preparation+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-preparation_begin).count();
                            }
                            const auto encode_begin=std::chrono::steady_clock::now();auto& renderer=view?*arm.probe:*arm.main;
                            REQUIRE(renderer.beginCommandBatch("Equal owned world preparation cost",&fixture.error));
                            targets[view]=prepared?renderer.renderPrepared(*prepared,&fixture.error,&results[view]):renderer.renderOwnedInputs(*inputs,&fixture.error,&results[view]);
                            INFO(fixture.error);REQUIRE(targets[view]);REQUIRE(results[view]);completions[view]=renderer.commandBatchCompletion();REQUIRE(completions[view]);
                            prepared.reset();REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,arm.batches,arm.queries));++value.reclaimed;
                            REQUIRE(renderer.commitCommandBatch(&fixture.error));
                            value.encode+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-encode_begin).count();
                        }
                        REQUIRE(fixture.device->waitUntilIdle(&fixture.error));
                        for(unsigned view=0;view<views;++view)for(unsigned query=0;query<3;++query)
                            REQUIRE(results[view]->wait(query,query_values[mode][view][query],&fixture.error));
                        const auto finished=std::chrono::steady_clock::now();value.owner_cpu=(OwnedPreparationCost::ownerClock()-cpu_started)/1e6;
                        value.complete=std::chrono::duration<double,std::milli>(finished-started).count();
                        value.native_bytes=arm.data->stats().published_bytes-native_before;
                        const auto budget_after=arm.budget.snapshot();value.credits=budget_after.admitted-budget_before.admitted;value.denials=budget_after.deferred-budget_before.deferred;
                        value.commits=ALMetalDevice::committedCommandBufferCount()-commits;
                        CHECK(value.commits==views+1);CHECK(value.reclaimed==views);CHECK(budget_after.preparation_in_flight==(occupied?2:0));
                        CHECK(value.worker_views==((mode==2&&!occupied)?views:0));CHECK(value.fallback_views==((mode==2&&occupied)?views:0));
                        CHECK(value.credits==value.worker_views);CHECK(value.denials==value.fallback_views);
                        CHECK(value.prepared_records==(mode?std::uint64_t(count)*views:0));
                        for(auto& credit:held)credit.reset();CHECK(arm.budget.snapshot().preparation_in_flight==0);
                        for(unsigned view=0;view<views;++view){ALMetalCommandGPUTime time;
                            if(completions[view]->gpuTime(time)==ALMetalCommandTimingStatus::ready){value.gpu+=time.elapsed_nanoseconds/1e6;++value.gpu_ready;}}
                        if(sample>=warmups)recorded[mode][sample-warmups]=value;
                        if(sample==warmups-1 || sample==samples+warmups-1)
                            for(unsigned view=0;view<views;++view)
                            {
                                pixels[mode][view]=fixture.read(*targets[view]);const auto clear=view%2?0u:0x3f800000u;
                                const auto covered=std::count_if(pixels[mode][view].depth.begin(),pixels[mode][view].depth.end(),[&](auto value){return value!=clear;});
                                CHECK(covered>25);CHECK(covered<side*side);
                            }
                        std::cout<<"WORLD_OWNED_PREP_RAW draws="<<count<<" mixed_seven_families="<<mixed<<" views="<<views<<" occupied_preparation_credits="<<occupied
                            <<" mode="<<mode<<" sample="<<sample<<" warmup="<<(sample<warmups)<<" owner_cpu_ms="<<value.owner_cpu<<" worker_kernel_cpu_ms="<<value.worker_cpu
                            <<" owner_plus_worker_cpu_ms="<<value.owner_cpu+value.worker_cpu<<" capture_adopt_wall_ms="<<value.capture_adopt<<" prepare_join_wall_ms="<<value.preparation
                            <<" render_reclaim_commit_wall_ms="<<value.encode<<" complete_settle_wall_ms="<<value.complete<<" gpu_command_sum_ms="<<value.gpu<<" gpu_ready="<<value.gpu_ready
                            <<" credited_worker_views="<<value.worker_views<<" denied_serial_fallback_views="<<value.fallback_views<<" cpu_packets_reclaimed="<<value.reclaimed
                            <<" prepared_constant_bytes="<<value.constant_bytes<<" uncompacted_constant_bytes="<<value.uncompacted<<" shared_common_records="<<value.shared_common
                            <<" native_frame_bytes="<<value.native_bytes<<" raw_body_pose_references_bytes="<<value.raw_pose_bytes<<" committed_commands="<<value.commits<<" settlement_barrier_commands=1\n";
                    }
                    for(unsigned view=0;view<views;++view)for(unsigned mode=1;mode<3;++mode)
                    {
                        CHECK(query_values[mode][view]==query_values[0][view]);
                        if(sample==warmups-1 || sample==samples+warmups-1)CHECK(pixels[mode][view]==pixels[0][view]);
                    }
                }
                for(unsigned mode=0;mode<3;++mode)
                {
                    std::array<double,samples> owner,worker,total,encode,complete,gpu;
                    for(unsigned sample=0;sample<samples;++sample){const auto& value=recorded[mode][sample];owner[sample]=value.owner_cpu;worker[sample]=value.worker_cpu;
                        total[sample]=value.owner_cpu+value.worker_cpu;encode[sample]=value.encode;complete[sample]=value.complete;gpu[sample]=value.gpu;}
                    std::cout<<"WORLD_OWNED_PREP_COST draws="<<count<<" mixed_seven_families="<<mixed<<" views="<<views<<" occupied_preparation_credits="<<occupied
                        <<" mode="<<(mode==0?"scalar_owned":mode==1?"prepared_serial":"prepared_requested_two")<<" samples="<<samples<<" warmups="<<warmups
                        <<" owner_cpu_median_ms="<<median(owner)<<" owner_cpu_max_ms="<<*std::max_element(owner.begin(),owner.end())
                        <<" worker_kernel_cpu_median_ms="<<median(worker)<<" owner_plus_worker_cpu_median_ms="<<median(total)
                        <<" encode_wall_median_ms="<<median(encode)<<" encode_wall_max_ms="<<*std::max_element(encode.begin(),encode.end())
                        <<" complete_wall_median_ms="<<median(complete)<<" complete_wall_max_ms="<<*std::max_element(complete.begin(),complete.end())
                        <<" gpu_command_median_ms="<<median(gpu)<<" gpu_command_max_ms="<<*std::max_element(gpu.begin(),gpu.end())
                        <<" capture_frontier=typed_native_batch_values ll_capture=0 asset_source_construction_excluded=1 stable_native_material_sources=1"
                        <<" raw_body_pose_adoption_included=1 independent_arm_frame_data=1 exact_shared_builtin_library=1 geometry_tiles=11"
                        <<" unique_raw_body_pose_bytes_per_view="<<(mixed?sizeof(fixture.avatar_pose):0)
                        <<" worker_cpu_scope=runJob_callback gcd_runtime_cpu_not_counted=1 budget_fixture_pressure_setup_excluded=1 product_worker_threshold_applied=0 whole_viewer_fps_claim=0\n";
                }
            }
        }
    }
}
