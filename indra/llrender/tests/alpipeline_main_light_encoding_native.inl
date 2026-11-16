/**
 * @file alpipeline_main_light_encoding_native.inl
 * @brief Actual static producers, owner point values and exclusive native main encoding.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "llcamera.h"
namespace
{
namespace MainLightEncodingTest
{
using Values = MainLightValuesTest::MetalLocalLightValues;
using ActualEarlyVisibility::Arm;
using ActualEarlyVisibility::MaterialScope;
using ActualEarlyVisibility::initialize;
using ActualEarlyVisibility::requireWork;
using ActualEarlyVisibility::now;
using ActualEarlyVisibility::wait;
using ActualStaticVisibilityCommands::threadCPU;
constexpr F32 DEFERRED_LIGHT_FALLOFF = .5f;
F32 light_fade_brightness(F32 resolved_brightness) { return resolved_brightness; }
#include "../../newview/light_screen_rect_native.inl"
struct LLVOVolume
{
    bool spotlight = false;
    bool isAttachment() const { return false; }
    bool isLightSpotlight() const { return spotlight; }
    F32 getLightRadius() const { return .25f; }
    LLColor3 getLightLinearColor() const { return {.1f,.075f,.05f}; }
    F32 getLightFalloff(F32 factor) const { return factor * .5f; }
};
struct LLDrawable
{
    LLVOVolume* volume = nullptr;
    LLVector3 position;
    LLVOVolume* getVOVolume() const { return volume; }
    const LLVector3& getPositionAgent() const { return position; }
};
// Match the production LLPointer conversion boundary without giving this
// handle ownership of stack/vector fixture objects.
struct DrawableReference
{
    LLDrawable* value = nullptr;
    DrawableReference(LLDrawable* input = nullptr) : value(input) {}
    operator LLDrawable*() const noexcept { return value; }
};
struct Light { DrawableReference drawable; F32 fade = 1.f; };
struct Owner
{
    using light_set_t = std::vector<Light>;
    light_set_t mNearbyLights;
    std::vector<LLVOVolume> volumes;
    std::vector<LLDrawable> drawables;
    LLCamera camera;
    LLMatrix4a modelview = LLMatrix4a::identity();
    bool sRenderAttachedLights = true;
    S32 local_light_count = 256;
    F32 light_scale = 1.f;
    U32 light_width = 16, light_height = 16;
    explicit Owner(U32 count) : volumes(count), drawables(count)
    {
        mNearbyLights.reserve(count);
        for (U32 i = 0; i < count; ++i)
        {
            drawables[i].volume = &volumes[i];
            drawables[i].position.set(3.f + .025f * (i % 5), .05f * (S32(i % 7) - 3), .05f * (S32(i % 3) - 1));
            mNearbyLights.push_back({&drawables[i],1.f});
        }
    }
    bool candidate() const
    {
#include "../../newview/pipeline_local_light_candidate_native.inl"
        return point_candidate;
    }
    void gather(Values& values)
    {
        values.setKey(Inputs::gFrameCount, light_width, light_height, local_light_count,
            light_scale, sRenderAttachedLights, modelview, camera.getProjection(), camera.getOrigin());
#include "../../newview/pipeline_local_light_values_native.inl"
    }
};
using LightPixels = std::array<U16,16*16*4>;
struct Sample : ActualEarlyVisibility::Sample
{
    LightPixels light_pixels{};
    double owner_cpu_ms=0, gpu_ms=0, gather_ms=0, encode_ms=0, join_ms=0, overlap_ms=0;
    U64 started=0, denied=0, point_lights=0, packet_bytes=0, credit_after_join=0;
    U64 publication_debt=0;
};
void frame(std::array<Arm,2>& arms)
{
    ++Inputs::gFrameCount;
    for (auto& arm : arms)
    { ALNativeFrameBudget::FrameInputs input; input.frame=Inputs::gFrameCount; arm.budget.beginFrame(input); }
}
Sample measure(MaterialGroupPublication& f, Arm& arm, U32 sources, Owner& owner,
               bool overlap, U32 condition, ALMetalRenderTarget& base)
{
    MaterialScope scope(arm); Sample sample;
    auto& renderer=*arm.renderers[0]; auto& capture=*arm.pipeline.mMetalWorldSimpleCapture;
    struct Cancel { ALMetalWorldGBufferRenderer& renderer; ~Cancel(){renderer.cancelCommandBatch();} } cancel{renderer};
    std::array<ActualEarlyVisibility::Receipt,3> receipts;
    std::unique_ptr<ALMetalRenderTarget> target;
    std::shared_ptr<ALMetalVisibilityResultBatch> results;
    const auto commands=ALMetalDevice::committedCommandBufferCount();
    const auto before_group=Inputs::ALSceneGroupPublicationNative::statistics.builds;
    const auto before_compile=Inputs::ALSceneGroupCompilationNative::statistics;
    const auto begin=now(), cpu_begin=threadCPU();
    arm.data->beginFrame(Inputs::gFrameCount);
    if (condition==0)
    {
        for(auto& group:arm.groups)
        {group->state->invalidate();group->layout.reset();group->dependencies.reset();
         for(auto* source:group->sources)source->state->invalidate();}
    }
    capture.clearDraws(); capture.complete=true;
    ActualEarlyVisibility::selectFamily(f,*arm.groups.front());f.configure(capture,false,false);
    capture.world_resources=std::make_shared<Inputs::WorldResources>();
    ActualEarlyVisibility::capture(f,arm,sources,sample);
    auto scene=f.scene(capture);scene.visibility_queries=arm.queries.data();scene.visibility_query_count=arm.queries.size();
    scene.source_command_banks=scene.source_direct_command_banks=scene.source_gpu_visibility=false;
    scene.material_argument_buffers=scene.cpu_cohort_visibility=false;
    auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(capture.batches),std::move(capture.source_references),
        std::move(capture.draw_order),std::move(capture.source_cohorts),std::move(arm.queries),
        std::move(capture.cpu_sources),capture.world_resources,Inputs::gFrameCount,&f.error);
    requireWork(bool(inputs),"Main light actual producer adoption failed");scene={};
    requireWork(renderer.beginCommandBatch("Actual main encode and owner point values",&f.error),"Main command admission failed");
    {
        Values serial_values;
        std::unique_ptr<Values> packet;
        auto* values=&serial_values;
        ALMetalMainWorldEncodingTask task;
        bool started=false;
        if(overlap && owner.candidate())
        {
            const auto capacity=std::min<std::size_t>(std::max(owner.local_light_count,0),owner.mNearbyLights.size());
            const auto bytes=Values::requiredCPUBytes(capacity,sizeof(task));
            auto reservation=arm.budget.tryAcquire(ALNativeFrameBudget::Work::preparation,bytes).credit;
            if(reservation)
            {
                packet=std::make_unique<Values>();packet->cpu_reservation=std::move(reservation);
                packet->lights.reserve(capacity);packet->inside_lights.reserve(capacity);
                sample.packet_bytes=packet->ownedCPUBytes(sizeof(task));
                requireWork(sample.packet_bytes<=bytes,"Actual main point capacity exceeds reserved CPU bytes");
                started=task.start(renderer,inputs,packet->cpu_reservation);
            }
            else ++sample.denied;
        }
        if(started)
        {
            ++sample.started;const auto gather_begin=now();owner.gather(*packet);const auto gather_end=now();
            auto outcome=task.finish();const auto joined=now();
            sample.gather_ms=double(gather_end-gather_begin)/1e6;
            sample.encode_ms=double(outcome.finished_ns-outcome.started_ns)/1e6;
            sample.join_ms=double(joined-gather_end)/1e6;
            const auto low=std::max(gather_begin,outcome.started_ns),high=std::min(gather_end,outcome.finished_ns);
            sample.overlap_ms=high>low?double(high-low)/1e6:0;
            requireWork(!outcome.threw&&!outcome.worker_is_owner,"Native main task did not execute safely off owner");
            target=std::move(outcome.target);results=std::move(outcome.visibility_results);f.error=std::move(outcome.error);
            sample.credit_after_join=arm.budget.snapshot().preparation_bytes_in_flight;
            requireWork(sample.credit_after_join==packet->cpu_reservation->logicalBytes(),"Main packet lost its reservation at CPU join");
            values=packet.get();
        }
        else
        {
            target=renderer.renderOwnedInputs(*inputs,&f.error,&results);
        }
        requireWork(bool(target)&&bool(results),"Actual main native rendering failed");
        receipts[0]=renderer.commandBatchCompletion();
        requireWork(renderer.commitCommandBatch(&f.error),"Actual main native commit failed");
        requireWork(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,capture.source_references,
            capture.draw_order,capture.source_cohorts,arm.queries),"Actual main CPU packet reclamation failed");
        ++sample.cpu_packets_reclaimed;capture.clearDraws();capture.world_resources=std::make_shared<Inputs::WorldResources>();
        // A typed base clear stands in for the unchanged serial base-light
        // producer. It is timed in both arms, and is not a base-shader claim.
        ALMetalRenderPassConfig clear;clear.colors[0].clear={.125,.25,.5,0};
        auto pass=base.makeRenderPass(clear,&f.error);
        auto command=f.device->makeCommandBuffer("Identical serial base-light producer frontier",&f.error);
        requireWork(bool(pass)&&bool(command),"Base-light frontier setup failed");
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Typed base-light clear",&f.error);
        requireWork(bool(encoder)&&encoder->end(&f.error),"Base-light frontier clear failed");
        receipts[1]=command->completionToken();requireWork(command->commit(&f.error),"Base-light frontier commit failed");
        // Selected serial preparation stays AFTER the main command commit
        // and unchanged serial base producer, at its original dependency.
        // Moving it before commit would exaggerate overlap's GPU head start.
        if(!started)
        {const auto gather_begin=now();owner.gather(serial_values);sample.gather_ms=double(now()-gather_begin)/1e6;}
        requireWork(values->matches(Inputs::gFrameCount,16,16,owner.local_light_count,owner.light_scale,
            owner.sRenderAttachedLights,owner.modelview,owner.camera.getProjection(),owner.camera.getOrigin()),"Main point packet key changed");
        sample.point_lights=values->lights.size();
        requireWork(renderer.beginCommandBatch("Identical downstream native point-light consumption",&f.error),"Point command admission failed");
        ALMetalWorldLocalLightingScene local;local.gbuffer=target.get();local.lights=values->lights.data();
        local.light_count=values->lights.size();local.bounded_light_count=values->bounded_count;
        local.additive_pass_count=values->additive_pass_count;local.specular_exponent=128.f;
        LLMatrix4a inverse;inverse.setInverse(owner.camera.getProjection());
        std::memcpy(local.inverse_projection,inverse.getF32ptr(),sizeof(local.inverse_projection));
        requireWork(renderer.accumulateLocalLights(base,local,&f.error),"Downstream native point-light consumption failed");
        receipts[2]=renderer.commandBatchCompletion();
        requireWork(renderer.commitCommandBatch(&f.error),"Point-light commit failed");
    } // CPU point arrays die before their packet credit, before timed settlement.
    requireWork(arm.budget.snapshot().preparation_bytes_in_flight==0,"Main point packet bytes did not retire");
    const auto encoded=now(),cpu_end=threadCPU();
    for(const auto& receipt:receipts)wait(receipt);
    sample.query_results[0].resize(results->resultCount());
    for(std::size_t q=0;q<results->resultCount();++q)
        requireWork(results->wait(q,sample.query_results[0][q],&f.error),"Actual main query settlement failed");
    results.reset();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(arm.budget.snapshot().scene_publications_in_flight&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    sample.publication_debt=arm.budget.snapshot().scene_publications_in_flight;
    requireWork(sample.publication_debt==0,"Actual main native publication debt did not retire");
    gGL.frameFences().release();const auto settled=now();
    sample.owner_wall_ms=double(encoded-begin)/1e6;sample.owner_cpu_ms=double(cpu_end-cpu_begin)/1e6;
    sample.settled_wall_ms=double(settled-begin)/1e6;
    sample.command_count=ALMetalDevice::committedCommandBufferCount()-commands;
    sample.group_builds=Inputs::ALSceneGroupPublicationNative::statistics.builds-before_group;
    const auto after_compile=Inputs::ALSceneGroupCompilationNative::statistics;
    sample.compile_admitted=after_compile.admitted-before_compile.admitted;
    sample.compile_denied=after_compile.denied-before_compile.denied;
    sample.stability_deferred=after_compile.stability_deferred-before_compile.stability_deferred;
    sample.pixels[0]=f.read(*target);
    requireWork(base.colorTexture(0)->readback(*f.device,0,0,{},sample.light_pixels.data(),16*8,&f.error),"Point target parity readback failed");
    for(const auto& receipt:receipts)
    { ALMetalCommandGPUTime gpu;requireWork(receipt->gpuTime(gpu)==ALMetalCommandTimingStatus::ready,"Native main GPU receipt unavailable");
      sample.gpu_ms+=double(gpu.elapsed_nanoseconds)/1e6;++sample.gpu_receipts_ready; }
    CHECK(sample.command_count==3);CHECK(sample.point_lights==owner.mNearbyLights.size());
    for(auto count:sample.query_results[0])CHECK(count>0);
    return sample;
}
std::unique_ptr<ALMetalRenderTarget> baseTarget(MaterialGroupPublication& f)
{
    ALMetalRenderTargetDescriptor d;d.width=d.height=16;d.colors[0].format=ALMetalPixelFormat::rgba16_float;
    auto target=ALMetalRenderTarget::create(*f.device,d,&f.error);REQUIRE(target);return target;
}
void parity(const std::array<Sample,2>& values)
{
    CHECK(values[0].pixels[0]==values[1].pixels[0]);
    CHECK(values[0].query_results[0]==values[1].query_results[0]);
    CHECK(values[0].light_pixels==values[1].light_pixels);
}
}
}

TEST_CASE("Actual main point eligibility declines empty and limited all-projector work",
          "[main-world-encoding][almetal][pipeline-buffer-publication]")
{
    using namespace MainLightEncodingTest;
    Owner empty(0);CHECK_FALSE(empty.candidate());
    Owner lights(3);for(auto& volume:lights.volumes)volume.spotlight=true;
    CHECK_FALSE(lights.candidate());lights.volumes.back().spotlight=false;
    lights.local_light_count=2;CHECK_FALSE(lights.candidate());
    lights.local_light_count=3;CHECK(lights.candidate());
    lights.local_light_count=0;CHECK_FALSE(lights.candidate());
}

TEST_CASE("Actual main encoder declines before mutation and cancels on owner unwinding",
          "[main-world-encoding][almetal][pipeline-buffer-publication]")
{
    std::string error;auto device=ALMetalDevice::create(&error);REQUIRE(device);
    auto renderer=ALMetalWorldGBufferRenderer::create(*device,1,&error);REQUIRE(renderer);
    ALMetalWorldGBufferScene scene;scene.width=scene.height=16;
    auto owner=std::make_shared<int>(17);std::weak_ptr<int> weak_owner=owner;
    auto inputs=ALMetalWorldViewInputs::adopt(scene,{}, {}, owner,1,&error);REQUIRE(inputs);
    ALNativeFrameBudget budget;auto credit=budget.tryAcquire(ALNativeFrameBudget::Work::preparation,1024).credit;REQUIRE(credit);
    const auto committed=ALMetalDevice::committedCommandBufferCount();
    { ALMetalMainWorldEncodingTask task;CHECK_FALSE(task.start(*renderer,inputs,credit)); }
    {
        ALMetalSubmissionScope submission(owner);REQUIRE(renderer->beginCommandBatch("Main decline preservation",&error));
        { ALMetalMainWorldEncodingTask task;CHECK_FALSE(task.start(*renderer,inputs,{})); }
        { ALMetalStageTimingScope diagnostic(nullptr,1,1,-1);ALMetalMainWorldEncodingTask task;
          CHECK_FALSE(task.start(*renderer,inputs,credit)); }
        CHECK(renderer->commandBatchActive());renderer->cancelCommandBatch();
        try
        {
            REQUIRE(renderer->beginCommandBatch("Main owner unwind",&error));
            ALMetalMainWorldEncodingTask task;REQUIRE(task.start(*renderer,inputs,credit));
            throw std::runtime_error("Authored owner gather failure");
        }
        catch(const std::runtime_error&) {}
        CHECK_FALSE(renderer->commandBatchActive());
    }
    CHECK(ALMetalDevice::committedCommandBufferCount()==committed);
    inputs.reset();owner.reset();credit.reset();CHECK(weak_owner.expired());
    CHECK(budget.snapshot().preparation_bytes_in_flight==0);
    // Adoption shape-validates; native geometry validation still owns a
    // malformed row. A failed task result must never replay or submit it.
    std::vector<ALMetalWorldSimpleBatch> invalid(1);
    // Nonempty adoption requires an explicit lifetime owner before native
    // geometry validation. The shell owns no backing: the row intentionally
    // remains malformed so the real renderer must reject it after dispatch.
    auto bad=ALMetalWorldViewInputs::adopt(scene,std::move(invalid),{},
        std::make_shared<int>(18),2,&error);REQUIRE(bad);
    credit=budget.tryAcquire(ALNativeFrameBudget::Work::preparation,1024).credit;REQUIRE(credit);
    REQUIRE(renderer->beginCommandBatch("Native malformed-row main failure",&error));
    ALMetalMainWorldEncodingTask task;REQUIRE(task.start(*renderer,bad,credit));
    auto result=task.finish();CHECK_FALSE(result.target);CHECK_FALSE(result.error.empty());
    CHECK(ALMetalDevice::committedCommandBufferCount()==committed);renderer->cancelCommandBatch();
}

TEST_CASE("Actual main producer worker preserves point pixels queries and packet-held CPU reservation",
          "[main-world-encoding][almetal][pipeline-buffer-publication]")
{
    using namespace MainLightEncodingTest;
    MaterialGroupPublication f(1);Inputs::gSavedSettings.native_groups=true;
    Inputs::gSavedSettings.native_resources=Inputs::gSavedSettings.native_visibility=false;
    auto data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(data);auto library=data->builtinLibrary(&f.error);REQUIRE(library);
    std::array<Arm,2> arms;std::array<std::unique_ptr<ALMetalRenderTarget>,2> bases;
    for(U32 mode=0;mode<2;++mode){initialize(f,arms[mode],2,4,false,false,library);bases[mode]=baseTarget(f);}
    const auto timing=ALMetalDevice::commandTimingEnabled();ALMetalDevice::setCommandTimingEnabled(true);
    struct Restore {bool value;~Restore(){ALMetalDevice::setCommandTimingEnabled(value);}} restore{timing};
    for(U32 step=0;step<3;++step)
    {
        frame(arms);std::array<Sample,2> samples;
        for(U32 mode=0;mode<2;++mode)
        {Owner owner(4);samples[mode]=measure(f,arms[mode],4,owner,mode,1,*bases[mode]);}
        parity(samples);CHECK(samples[1].started==1);CHECK(samples[1].credit_after_join==samples[1].packet_bytes);
        CHECK(samples[1].denied==0);CHECK(samples[1].overlap_ms>=0);
    }
}

TEST_CASE("Actual main producer point gather and exclusive native encoding compare complete frontier costs",
          "[almetal][.native-main-light-encoding-cost]")
{
    using namespace MainLightEncodingTest;
    const auto enabled=[](const char* key){const auto* value=std::getenv(key);return value&&*value&&*value!='0';};
    if(!enabled("ALCHEMY_MAIN_LIGHT_ENCODING_COST"))SKIP("Set ALCHEMY_MAIN_LIGHT_ENCODING_COST=1 for actual owner/native frontier costs");
#ifndef NDEBUG
    SKIP("Main point overlap costs require optimized code");
#endif
    REQUIRE_FALSE(enabled("MTL_DEBUG_LAYER"));REQUIRE_FALSE(enabled("MTL_SHADER_VALIDATION"));
    REQUIRE_FALSE(enabled("MTL_CAPTURE_ENABLED"));REQUIRE_FALSE(enabled("MTL_HUD_ENABLED"));
    const auto timing=ALMetalDevice::commandTimingEnabled();ALMetalDevice::setCommandTimingEnabled(true);
    struct Restore {bool value;~Restore(){ALMetalDevice::setCommandTimingEnabled(value);}} restore{timing};
    for(const auto shape:{std::array<U32,2>{2,1},{2,2},{2,4},{64,4}})
    for(U32 points:{0u,1u,4u,32u,128u})for(U32 condition:{0u,1u})
    {
        MaterialGroupPublication f(1);Inputs::gSavedSettings.native_groups=true;
        Inputs::gSavedSettings.native_resources=Inputs::gSavedSettings.native_visibility=false;
        auto data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(data);auto library=data->builtinLibrary(&f.error);REQUIRE(library);
        std::array<Arm,2> arms;std::array<std::unique_ptr<ALMetalRenderTarget>,2> bases;
        for(U32 mode=0;mode<2;++mode){initialize(f,arms[mode],shape[0],shape[1],false,false,library);bases[mode]=baseTarget(f);}
        std::cout<<"MAIN_LIGHT_SCOPE groups="<<shape[0]<<" sources_per_group="<<shape[1]<<" points="<<points<<" condition="<<condition
            <<" samples=8 startup=2 mode0=selected_serial mode1=exclusive_main_owner_point_values defaultOFF=1 synthetic_point_inputs=1"
              " real_LLCamera_frustum_and_production_screenrect=1 real_default16_publication_budget=1 no_untimed_preseal=1"
              " includes=actual_source_cull_capture_publication_adoption_candidatecheck_budget_packetallocation_dispatch_gather_nativeencode_commit_pointconsumer_query_GPUsettlement_CPUreclaim_creditretirement"
              " base_producer=identical_typed_clear_not_base_shader excludes=initial_assets_live_octree_camera_dependencywalker_common_renderer_factory_parityreadback_targetwrapper"
              " GPU_sum_not_criticalpath=1 FPS_claim=0\n";
        for(U32 sample=0;sample<10;++sample)
        {
            frame(arms);std::array<Sample,2> paired;
            for(U32 position=0;position<2;++position)
            {
                const U32 mode=(sample+position)%2;Owner owner(points);
                auto value=measure(f,arms[mode],shape[1],owner,mode,condition,*bases[mode]);
                std::cout<<"MAIN_LIGHT_COST groups="<<shape[0]<<" sources_per_group="<<shape[1]<<" points="<<points
                    <<" condition="<<condition<<" sample="<<sample<<" mode="<<mode
                    <<" owner_wall_ms="<<value.owner_wall_ms<<" owner_cpu_ms="<<value.owner_cpu_ms<<" settled_wall_ms="<<value.settled_wall_ms
                    <<" gpu_ms="<<value.gpu_ms<<" gather_ms="<<value.gather_ms<<" encode_ms="<<value.encode_ms<<" join_ms="<<value.join_ms<<" overlap_ms="<<value.overlap_ms
                    <<" started="<<value.started<<" denied="<<value.denied<<" packet_bytes="<<value.packet_bytes<<" credit_after_join="<<value.credit_after_join
                    <<" point_lights="<<value.point_lights<<" raw_draws="<<value.raw_draws<<" cohort_draws="<<value.cohort_draws<<" cohort_groups="<<value.cohort_groups
                    <<" publication_debt_groups="<<value.publication_debt_groups<<" compile_admitted="<<value.compile_admitted<<" compile_denied="<<value.compile_denied
                    <<" stability_deferred="<<value.stability_deferred<<" commands="<<value.command_count<<" GPU_receipts="<<value.gpu_receipts_ready<<'\n';
                if(points==0)CHECK(value.started==0);
                paired[mode]=std::move(value);
            }
            parity(paired);
        }
    }
}
