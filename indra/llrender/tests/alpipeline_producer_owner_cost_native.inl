/**
 * @file alpipeline_producer_owner_cost_native.inl
 * @brief Producer drain through required capture, real render and retirement.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>

void alTestWithAutoreleasePool(void (*)(void*), void*);
namespace producer_owner_cost
{
void require(bool value,const std::string& message)
{ if(!value)throw std::runtime_error(message); }
U64 cpu()
{ timespec value{};require(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0,"thread clock unavailable");
  return U64(value.tv_sec)*1000000000ULL+value.tv_nsec; }
struct Arm
{
    std::shared_ptr<DrainGroup> group=std::make_shared<DrainGroup>();
    std::shared_ptr<ALNativePublication::State> cohort_state=std::make_shared<ALNativePublication::State>();
    std::shared_ptr<ALNativePublication::Observer> dependencies;
    std::shared_ptr<Inputs::ALSceneGroupPublicationNative::Layout> layout;
    ALNativeCohortCompilationAdmission admission;
    std::unique_ptr<ALMetalWorldMaterialDataCache> materials;
    std::shared_ptr<ALMetalWorldFrameData> data;
    std::array<std::unique_ptr<ALMetalWorldGBufferRenderer>,3> renderers;
    std::array<Inputs::LLPipeline,3> pipelines;
    std::array<Inputs::LLCullResult,3> culls;
};
struct Sample
{
    U64 wall=0,thread=0,producer=0,group=0,encode=0,gpu=0;
    U64 source_hits=0,source_builds=0,cohort_draws=0;
    std::array<ALMetalCommandGPUTime,3> intervals{};
};
using Image=std::vector<U8>;
Image read(DrainFixture& fixture,const ALMetalRenderTarget& target)
{
    Image result;
    const auto width=target.descriptor().width,height=target.descriptor().height;
    for(U32 attachment=0;attachment<5;++attachment)
    {
        const auto* texture=attachment<4?target.colorTexture(attachment):target.depthStencilTexture();
        require(texture!=nullptr,"missing precheck attachment");
        const auto row=width*(attachment<4?texture->bytesPerPixel():sizeof(float));
        const auto at=result.size();result.resize(at+row*height);
        require(texture->readback(*fixture.device,0,0,{},result.data()+at,row,&fixture.error),fixture.error);
    }
    return result;
}
}

TEST_CASE("Actual producer preparation and selective CPU routing include capture and native completion costs",
          "[almetal][.producer-owner-cost]")
{
    using namespace producer_owner_cost;
    const auto* enabled=std::getenv("ALCHEMY_PRODUCER_OWNER_COST");
    if(!enabled||std::strcmp(enabled,"1"))SKIP("Explicit producer owner cost opt-in required");
    const auto* cell_text=std::getenv("ALCHEMY_PRODUCER_OWNER_COST_CELL");
    const auto* directory=std::getenv("ALCHEMY_PRODUCER_OWNER_COST_DIR");
    REQUIRE(cell_text);REQUIRE(directory);
    const std::string cell_string=cell_text;REQUIRE(cell_string.size()==1);
    const U32 cell=U32(cell_string[0]-'0');REQUIRE(cell<=5);
    const U32 count=cell==0?1:cell==1?16:64,views=cell==4?3:1;
    const bool heavy=cell>=2;
    DrainFixture fixture(2*count);fixture.policy.heavy_bounds=true;
    REQUIRE(fixture.device->resourcePlanningMode()==ALMetalResourcePlanMode::enabled);
    const U32 producer_pass=ProducerRecipeFrontier::LLRenderPass::PASS_MATERIAL;
    const U32 consumer_pass=Inputs::LLRenderPass::PASS_MATERIAL;
    std::array<Arm,2> arms;
    const auto all_sources=fixture.group->draws;
    LLMatrix4a model=LLMatrix4a::identity();ALNativeRegionModel region;REQUIRE(region.update(model,LLVector3(0,0,0)));
    Inputs::gSavedSettings.native_groups=Inputs::gSavedSettings.native_resources=Inputs::gSavedSettings.native_visibility=false;
    auto library_data=ALMetalWorldFrameData::create(*fixture.device,&fixture.error);REQUIRE(library_data);
    auto library=library_data->builtinLibrary(&fixture.error);REQUIRE(library);
    for(U32 mode=0;mode<2;++mode)
    {
        auto& arm=arms[mode];
        for(U32 i=0;i<count;++i)
        {
            auto source=all_sources[mode*count+i];source->mModelMatrix=&model;
            if(heavy)visibilityCostMesh(*source,3072,cell==3||(cell==4&&i%2));
            arm.group->draws.push_back(std::move(source));
        }
        arm.group->rebuild();for(auto& row:arm.group->work->values)row.pass=producer_pass;
        arm.materials=std::make_unique<ALMetalWorldMaterialDataCache>(*fixture.device);
        arm.data=ALMetalWorldFrameData::create(*fixture.device,&fixture.error);REQUIRE(arm.data);
        for(U32 view=0;view<views;++view)
        { arm.renderers[view]=ALMetalWorldGBufferRenderer::create(*fixture.device,1,library,&fixture.error);
          REQUIRE(arm.renderers[view]);arm.renderers[view]->setFrameData(arm.data); }
    }
    std::filesystem::path output=directory;
    REQUIRE(std::filesystem::create_directories(output));
    std::ofstream raw(output/"raw.csv");REQUIRE(raw);
    raw<<"phase,block,position,mode,wall_ns,thread_cpu_ns,producer_ns,group_ns,encode_ns,gpu_ns,source_hits,source_builds,cohort_draws";
    for(U32 i=0;i<views;++i)raw<<",gpu"<<i<<"_start,gpu"<<i<<"_end";raw<<'\n';
    std::ofstream metadata(output/"metadata.json");REQUIRE(metadata);
    metadata<<"{\"cell\":"<<cell<<",\"sources\":"<<count<<",\"indices_per_source\":"<<(heavy?3072:3)
        <<",\"views\":"<<views<<",\"same_identity_camera_views\":true,\"width\":16,\"height\":16,"
        <<"\"resource_plan_mode\":2,\"native_visibility\":false,\"old_diagnostics\":false,"
        <<"\"measured_per_arm\":300,\"warm_per_arm\":20,\"scope\":\"producer drain, bounds, group admission/seal, ordered capture, render, actual command completion and autorelease retirement; excludes mesh upload and original scene discovery; typed weak/shared pins\","
        <<"\"precheck_planes\":\"four color attachments plus depth and query; stencil covered by separate source routing native tests\"}\n";
    auto emit=[&](const char* phase,int block,U32 position,U32 mode,const Sample& sample)
    {
        raw<<phase<<','<<block<<','<<position<<','<<mode<<','<<sample.wall<<','<<sample.thread<<','
            <<sample.producer<<','<<sample.group<<','<<sample.encode<<','<<sample.gpu<<','<<sample.source_hits<<','<<sample.source_builds<<','<<sample.cohort_draws;
        raw<<std::setprecision(17);for(U32 view=0;view<views;++view)raw<<','<<sample.intervals[view].start_seconds<<','<<sample.intervals[view].end_seconds;raw<<'\n';raw.flush();
        require(bool(raw),"producer owner raw output failed");
    };
    auto retain_planes=[&](const char* phase,U32 mode,const std::array<Image,3>& planes,
                           const std::array<std::vector<U64>,3>& counts)
    {
        for(U32 view=0;view<views;++view)
        {
            const auto stem=std::string(phase)+"-arm"+std::to_string(mode)+"-view"+std::to_string(view);
            std::ofstream image(output/(stem+".planes"),std::ios::binary);
            image.write(reinterpret_cast<const char*>(planes[view].data()),planes[view].size());require(bool(image),"plane retention failed");
            std::ofstream query(output/(stem+".queries"),std::ios::binary);
            query.write(reinterpret_cast<const char*>(counts[view].data()),counts[view].size()*sizeof(U64));require(bool(query),"query retention failed");
        }
    };
    U64 frame=fixture.inputs.frame;
    F32 churn_value=.25f;
    auto observe=[&](U32 mode,bool retain,bool churn,std::array<Image,3>* pixels,
                     std::array<std::vector<U64>,3>* query_values)
    {
        auto& arm=arms[mode];fixture.group=arm.group;
        fixture.inputs.frame=++frame;fixture.budget.beginFrame(fixture.inputs);
        auto previous=std::move(Inputs::ALScenePublicationNative::material_data_cache);
        Inputs::ALScenePublicationNative::material_data_cache=std::move(arm.materials);
        struct Restore
        {
            Arm& arm;std::unique_ptr<ALMetalWorldMaterialDataCache> previous;
            ~Restore(){arm.materials=std::move(Inputs::ALScenePublicationNative::material_data_cache);
                Inputs::ALScenePublicationNative::material_data_cache=std::move(previous);Inputs::sCull=nullptr;}
        } restore{arm,std::move(previous)};
        Sample sample;std::array<std::unique_ptr<ALMetalRenderTarget>,3> retained;
        std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> retained_queries;
        auto body=[&]
        {
            if(churn)
            { auto& source=*arm.group->draws.back();source.mSpecColor.mV[0]=churn_value;
              source.state->invalidate();arm.group->state->invalidate();arm.group->rebuild();
              for(auto& row:arm.group->work->values)row.pass=producer_pass;arm.cohort_state->invalidate(); }
            auto phase=std::chrono::steady_clock::now();
            if(mode)
            {
                const std::array groups{arm.group.get()};
                Inputs::ALSceneProducerDrainNative::drain(groups.begin(),groups.end(),fixture.policy,
                    Inputs::sNativeSceneSources,fixture.serial,U32(frame),fixture.budget,
                    [](DrainGroup* group){return group->shared_from_this();},
                    [](Inputs::LLDrawInfo* source){return source?source->weak_from_this().lock():std::shared_ptr<Inputs::LLDrawInfo>{};},
                    ProducerRecipeFrontier::ALSceneProducerDrainNative::ordinaryFlavor);
            }
            sample.producer=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-phase).count();
            phase=std::chrono::steady_clock::now();
            std::shared_ptr<const NativeGroup> packet;
            auto flavor=Inputs::ALScenePublicationNative::WorldFlavor{};flavor.family=Inputs::ALScenePublicationNative::WorldFamily::legacy;
            flavor.native_resources=false;flavor.cohort_bounds=true;
            std::vector<Inputs::LLDrawInfo*> sources;
            if(mode&&arm.group->mMetalSourceCapabilityComplete)
            {
                sources.reserve(count);for(const auto& source:arm.group->draws)sources.push_back(source.get());
                packet=Inputs::ALSceneGroupPublicationNative::currentMaterial(arm.cohort_state,region.snapshot(),flavor,
                    Inputs::sNativeSceneSources,U32(frame));
                if(!packet)packet=Inputs::ALSceneGroupCompilationNative::compile(arm.admission,arm.cohort_state,
                    fixture.budget,count,1,[&]{return Inputs::ALSceneGroupPublicationNative::sealMaterial(
                        arm.cohort_state,arm.dependencies,arm.layout,model,region.snapshot(),sources,flavor,
                        Inputs::sNativeSceneSources,fixture.serial,U32(frame));});
                require(bool(packet),"heavy selected cohort failed to publish");
            }
            sample.group=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-phase).count();
            arm.data->beginFrame(frame);
            for(U32 view=0;view<views;++view)
            {
                auto& capture=*arm.pipelines[view].mMetalWorldSimpleCapture;capture.clearDraws();capture.complete=true;
                capture.world_resources=std::make_shared<Inputs::WorldResources>();
                auto& cull=arm.culls[view];cull.clear();Inputs::sCull=&cull;
                if(packet)require(cull.pushNativeMaterialRange(consumer_pass,packet,count,region.snapshot()),"cohort cull admission failed");
                else for(const auto& source:arm.group->draws)cull.pushDrawInfo(consumer_pass,source.get());
                arm.pipelines[view].captureMetalWorldMaterialPass(consumer_pass);
                require(capture.complete&&capture.logicalDrawCount()==count,"required capture failed");
                sample.source_hits+=capture.scene_source_hits;sample.source_builds+=capture.scene_source_builds;
                capture.scene_source_hits=capture.scene_source_builds=0;
                sample.cohort_draws+=capture.source_cohort_draw_count;
                ALMetalWorldGBufferScene scene;scene.width=scene.height=16;scene.source_routing=mode!=0;
                std::fill_n(scene.clear_color,4,0.f);
                scene.batches=capture.batches.data();scene.batch_count=capture.batches.size();
                scene.references=capture.source_references.data();scene.reference_count=capture.source_references.size();
                scene.draw_order=capture.draw_order.data();scene.draw_order_count=capture.draw_order.size();
                scene.cohorts=capture.source_cohorts.data();scene.cohort_count=capture.source_cohorts.size();
                scene.source_owner=capture.world_resources;scene.cpu_source_owner=capture.cpu_sources;
                std::array<ALMetalWorldVisibilityQuery,2> queries{};queries[0].batch_offset=0;queries[1].batch_offset=count;
                for(auto& query:queries)
                { std::memcpy(query.modelview_projection,model.getF32ptr(),sizeof(query.modelview_projection));
                  query.center[0]=.35f;query.center[1]=-.5f;query.center[2]=.75f;
                  query.half_extent[0]=query.half_extent[1]=query.half_extent[2]=.08f;query.fan=7;query.count_samples=1; }
                scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
                auto& renderer=*arm.renderers[view];
                const auto encode_begin=std::chrono::steady_clock::now();
                require(renderer.beginCommandBatch("Producer complete-owner observation",&fixture.error),fixture.error);
                auto target=renderer.render(scene,&fixture.error,&retained_queries[view]);require(bool(target),fixture.error);
                auto completion=renderer.commandBatchCompletion();require(bool(completion),"missing actual owner receipt");
                require(renderer.commitCommandBatch(&fixture.error),fixture.error);
                sample.encode+=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-encode_begin).count();
                require(completion->waitUntilCompleted(&fixture.error),fixture.error);
                require(completion->gpuTime(sample.intervals[view])==ALMetalCommandTimingStatus::ready,"missing actual owner GPU interval");
                sample.gpu+=sample.intervals[view].elapsed_nanoseconds;
                const auto actual=renderer.worldSourceCommandStats();
                require((mode&&packet&&cell==3)||actual.last_pipeline_validation==2,"actual fragment validation not disabled");
                capture.clearDraws();capture.world_resources.reset();
                completion.reset();
                if(retain)retained[view]=std::move(target);
                else{retained_queries[view].reset();target.reset();}
            }
        };
        const auto wall=std::chrono::steady_clock::now();const auto thread=cpu();
        alTestWithAutoreleasePool(+[](void* context){(*static_cast<decltype(body)*>(context))();},&body);
        sample.thread=cpu()-thread;sample.wall=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-wall).count();
        if(retain)for(U32 view=0;view<views;++view)
        { (*pixels)[view]=read(fixture,*retained[view]);for(U32 q=0;q<2;++q){U64 value=0;
            require(retained_queries[view]->wait(q,value,&fixture.error),fixture.error);(*query_values)[view].push_back(value);} }
        return sample;
    };
    std::array<std::array<Image,3>,2> images;std::array<std::array<std::vector<U64>,3>,2> queries;
    for(U32 mode=0;mode<2;++mode)
    { const auto sample=observe(mode,true,false,&images[mode],&queries[mode]);emit("precheck",-1,mode,mode,sample);
      retain_planes("initial",mode,images[mode],queries[mode]); }
    REQUIRE(images[0]==images[1]);REQUIRE(queries[0]==queries[1]);
    REQUIRE(std::any_of(queries[0][0].begin(),queries[0][0].end(),[](U64 value){return value>0;}));
    if(cell!=3)
    {
        bool drawn=false;for(U32 i=0;i<16*16;++i)drawn|=images[0][0][i*4]==255;
        REQUIRE(drawn);
    }
    if(cell==5)
    {
        const auto original=images[0];
        churn_value=.75f;
        for(U32 mode=0;mode<2;++mode)
        { queries[mode]={};const auto sample=observe(mode,true,true,&images[mode],&queries[mode]);
          emit("churn-precheck",-1,mode,mode,sample);retain_planes("changed",mode,images[mode],queries[mode]); }
        REQUIRE(images[0]==images[1]);REQUIRE(queries[0]==queries[1]);REQUIRE(images[0]!=original);
    }
    const auto* precheck=std::getenv("ALCHEMY_PRODUCER_OWNER_PRECHECK_ONLY");if(precheck&&std::strcmp(precheck,"1")==0)return;
    for(int block=-10;block<150;++block)for(U32 position=0;position<4;++position)
    {
        const U32 mode=U32(((block+10)%2)?(position==0||position==3):(position==1||position==2));
        churn_value=block%32==0?.25f:.75f;
        const auto sample=observe(mode,false,cell==5&&position<2&&block%16==0,nullptr,nullptr);
        emit(block<0?"warm":"measured",block,position,mode,sample);
    }
}
