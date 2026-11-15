/**
 * @file alpipeline_animated_gpu_records_native.inl
 * @brief Actual attachment source publication, late-pose capture and costs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after the existing attachment palette actual-owner fixtures.
 */
#include <optional>
#include <cstdlib>
namespace AnimatedGpuRecordActual
{
// Every triangle has three unique positions/indices. This is a small real
// mesh, rather than repeating one triangle to manufacture an index count.
void mesh(Inputs::LLDrawInfo& info)
{
    constexpr U32 columns=8,rows=4,count=columns*rows*3;
    const U32 mask=info.mVertexBuffer->getTypeMask();LLPointer<LLVertexBuffer> buffer=new LLVertexBuffer(mask);
    REQUIRE(buffer->allocateBuffer(count,count));std::memset(buffer->getMappedData(),0,buffer->getSize());
    std::vector<LLVector4a> positions(count),normals(count),tangents(count);
    std::vector<LLVector2> uv(count);std::vector<LLVector4a> weights(count);
    std::vector<LLColor4U> colors(count,LLColor4U(255,96,64,255));std::vector<U16> indices(count);
    for(U32 row=0;row<rows;++row)for(U32 column=0;column<columns;++column)
    {
        const F32 x0=F32(column)/columns,x1=F32(column+.9f)/columns;
        const F32 y0=F32(row)/rows,y1=F32(row+.9f)/rows;
        const std::array<LLVector2,3> corners{LLVector2(x0,y0),LLVector2(x1,y0),LLVector2(x0,y1)};
        for(U32 corner=0;corner<3;++corner)
        {
            const U32 at=(row*columns+column)*3+corner;
            positions[at].set(-.9f+.8f*corners[corner].mV[0],-.8f+1.6f*corners[corner].mV[1],.125f,0);
            normals[at].set(0,0,1,0);tangents[at].set(1,0,0,1);uv[at]=corners[corner];
            weights[at].set((column&1)?.75f:.25f,(column&1)?1.25f:1.75f,0,0);indices[at]=U16(at);
        }
    }
    buffer->setPositionData(positions.data());buffer->setNormalData(normals.data());buffer->setTexCoord0Data(uv.data());
    buffer->setColorData(colors.data());buffer->setWeight4Data(weights.data());
    if(mask&LLVertexBuffer::MAP_TANGENT)buffer->setTangentData(tangents.data());
    if(mask&LLVertexBuffer::MAP_TEXCOORD1)buffer->setTexCoord1Data(uv.data());
    if(mask&LLVertexBuffer::MAP_TEXCOORD2)
    {LLStrider<LLVector2> target;REQUIRE(buffer->getTexCoord2Strider(target));for(U32 at=0;at<count;++at)target[at]=uv[at];}
    if(mask&LLVertexBuffer::MAP_EMISSIVE)
    {LLStrider<LLColor4U> target;REQUIRE(buffer->getEmissiveStrider(target));for(U32 at=0;at<count;++at)target[at].set(0,0,0,255);}
    buffer->setIndexData(indices.data());buffer->unmapBuffer();
    REQUIRE(buffer->hasPublishedMetalVertexBuffer());REQUIRE(buffer->hasPublishedMetalIndexBuffer());
    info.mVertexBuffer=buffer;info.mStart=0;info.mEnd=U16(count-1);info.mCount=count;info.mOffset=0;
}
struct Host : AttachmentPalettePublication
{
    U32 count;
    Host(U32 family,U32 draws):AttachmentPalettePublication(family,2,draws*2),count(draws)
    {
        if(family==1)flavor.material_alpha_mode=2;
        else {flavor.alpha_mask=true;for(auto& material:materials)material->mAlphaMode=Inputs::LLGLTFMaterial::ALPHA_MODE_MASK;}
        for(auto& info:infos)mesh(*info);
    }
    void captureArm(Inputs::LLPipeline& pipeline,U32 mode,U32 source_offset=std::numeric_limits<U32>::max(),ALNativeFrameBudget* budget=nullptr,bool gpu_requested=true,bool stabilize_direct=false)
    {
        auto& capture=*pipeline.mMetalWorldSimpleCapture;capture.clearDraws();capture.complete=true;
        configure(capture,false,false);capture.rigged=true;capture.world_post_deferred_active=false;capture.pbr_glow=false;
        capture.beginAnimatedSources(mode!=0,mode!=0 && gpu_requested,budget,stabilize_direct);capture.world_resources=std::make_shared<Inputs::WorldResources>();
        const U32 first=source_offset==std::numeric_limits<U32>::max()?mode*count:source_offset;
        for(U32 at=first;at<first+count;++at)
            pipeline.captureMetalPublishedWorldBatch(*infos[at],U32(flavor.family),false);
        REQUIRE(capture.complete);REQUIRE(capture.logicalDrawCount()==count);REQUIRE(capture.flushDrawOrder());
    }
    std::shared_ptr<const ALMetalWorldViewInputs> adoptArm(Inputs::LLPipeline& pipeline,U64 epoch,U32 mode,bool gpu_visibility=true,bool gpu_commands=true)
    {
        auto& capture=*pipeline.mMetalWorldSimpleCapture;auto view=scene(capture);view.animated_gpu_records=mode!=0 && gpu_commands;
        view.animated_gpu_visibility=gpu_visibility;
        std::vector<ALMetalWorldVisibilityQuery> queries(3);
        for(U32 at=0;at<queries.size();++at)
        {
            auto& query=queries[at];query.modelview_projection[0]=query.modelview_projection[5]=query.modelview_projection[10]=query.modelview_projection[15]=1;
            query.center[0]=.5f;query.center[1]=-.5f;query.center[2]=-.75f;query.half_extent[0]=query.half_extent[1]=.125f;query.half_extent[2]=.0625f;
            query.batch_offset=at==0?0:at==1?count/2:count;query.fan=7;query.count_samples=1;
        }
        return ALMetalWorldViewInputs::adopt(view,std::move(capture.batches),std::move(capture.source_references),
            std::move(capture.draw_order),std::move(capture.source_cohorts),std::move(queries),
            std::move(capture.cpu_sources),capture.world_resources,epoch,&error);
    }
    void changeResources(U32 mode,U32 sample)
    {
        const LLColor4U tint=sample&1?LLColor4U(64,255,128,255):LLColor4U(255,96,64,255);
        const std::vector<LLColor4U> colors(96,tint);
        for(U32 at=0;at<count;at+=16)
        {auto& buffer=*infos[mode*count+at]->mVertexBuffer;buffer.setColorData(colors.data());buffer.unmapBuffer();}
    }
};
void beginBudgetFrame(ALNativeFrameBudget& budget,U64 frame)
{
    REQUIRE_FALSE(budget.snapshot().draining);
    ALNativeFrameBudget::FrameInputs input;input.frame=frame;budget.beginFrame(input);
}
void configureMetadataAdmission(ALMetalWorldFrameData& data,ALNativeFrameBudget& budget)
{
    data.setSourcePublicationAdmission([&budget,pending=std::optional<ALNativeFrameBudget::Frame>{}]
        (const ALMetalWorldSourcePublicationWork& work) mutable->std::shared_ptr<const void>
    {
        const auto frame=budget.snapshot().frame;
        if(!pending)pending=frame;
        auto decision=budget.tryAcquire(ALNativeFrameBudget::Work::scene_publication,work.chargedBytes(),*pending);
        if(decision){pending.reset();return decision.credit;}return {};
    });
}
}

TEST_CASE("Actual retained animated capture changes pose without rebuilding geometry or material sources",
          "[almetal][pipeline-buffer-publication][animated-gpu-records][PPD-287]")
{
    using namespace AnimatedGpuRecordActual;
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);Host f(family,4);ALNativeFrameBudget budget;Inputs::LLPipeline pipeline;
        auto data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(data);U64 native_epoch=1;data->beginFrame(native_epoch);
        configureMetadataAdmission(*data,budget);
        auto library=data->builtinLibrary(&f.error);REQUIRE(library);
        auto main=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error),probe=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
        REQUIRE(main);REQUIRE(probe);main->setFrameData(data);probe->setFrameData(data);
        std::array<std::vector<U64>,2> revisions;
        std::vector<std::shared_ptr<const ALMetalWorldAnimatedSource>> direct_sources;
        for(U32 mode=0;mode<2;++mode)for(U32 at=0;at<f.count;++at)revisions[mode].push_back(f.infos[mode*f.count+at]->state->revision());
        for(U32 step=0;step<3;++step)
        {
            CAPTURE(step);++Inputs::gFrameCount;beginBudgetFrame(budget,Inputs::gFrameCount);data->beginFrame(++native_epoch);f.pose(step==1?.375f:0.f,false);
          for(U32 route:{0u,1u,2u})
          {
            const bool gpu_commands=route!=0,gpu_visibility=route==2;CAPTURE(route,gpu_visibility);
            // The once-per-owner CPU compilation policy permits a new
            // optional GPU capability consideration in a later outer frame.
            if(!step && route==1)
            {++Inputs::gFrameCount;beginBudgetFrame(budget,Inputs::gFrameCount);data->beginFrame(++native_epoch);}
            std::array<GroupPublication::Image,2> pixels;std::array<std::array<U64,3>,2> queries;
            for(U32 mode=0;mode<2;++mode)
            {
                const auto serial=Inputs::sNativeSceneSerial;const auto source_builds=Inputs::ALScenePublicationNative::animated_source_publications;
                const auto resource_builds=Inputs::ALScenePublicationNative::resource_source_publications;
                f.captureArm(pipeline,mode,std::numeric_limits<U32>::max(),&budget,gpu_commands);const auto& capture=*pipeline.mMetalWorldSimpleCapture;
                if(mode){REQUIRE(capture.batches.empty());REQUIRE(capture.source_references.size()==f.count);
                    for(U32 at=0;at<capture.source_references.size();++at)
                    {
                        const auto& reference=capture.source_references[at];REQUIRE(reference.animated_source);REQUIRE(reference.animated_palette);
                        if(!step && !route)
                        {CHECK_FALSE(reference.animated_source->gpuReady());CHECK_FALSE(reference.source->native_resource_source);direct_sources.push_back(reference.animated_source->owner());}
                        if(!step && route==1)
                        {CHECK(reference.animated_source->gpuReady());CHECK(reference.animated_source!=direct_sources[at].get());CHECK_FALSE(direct_sources[at]->gpuReady());
                         CHECK(reference.animated_source->key().identity==direct_sources[at]->key().identity);
                         CHECK(reference.animated_source->key().flavor==direct_sources[at]->key().flavor);
                         CHECK(reference.animated_source->key().revision==direct_sources[at]->key().revision);
                         CHECK(reference.source->vertices==direct_sources[at]->prototype().vertices);
                         CHECK(reference.source->native_material_source==direct_sources[at]->prototype().native_material_source);
                         CHECK(reference.animated_source->nativeLease().get()==direct_sources[at]->nativeLease().get());}
                    }
                }
                if(!route)CHECK(Inputs::ALScenePublicationNative::resource_source_publications==resource_builds);
                if(step || route==2){CHECK(Inputs::sNativeSceneSerial==serial);CHECK(Inputs::ALScenePublicationNative::animated_source_publications==source_builds);}
                auto input=f.adoptArm(pipeline,native_epoch,mode,gpu_visibility,gpu_commands);INFO(f.error);REQUIRE(input);
                std::shared_ptr<ALMetalVisibilityResultBatch> results;const auto before=(mode?probe:main)->worldAnimatedStats();
                const auto declarations_before=(mode?probe:main)->worldSourceCommandStats().cache.declaration_calls;
                auto target=(mode?probe:main)->renderOwnedInputs(*input,&f.error,&results);INFO(f.error);REQUIRE(target);REQUIRE(results);
                pixels[mode]=f.read(*target);for(U32 at=0;at<3;++at){REQUIRE(results->wait(at,queries[mode][at],&f.error));CHECK(queries[mode][at]>0);}
                const auto after=(mode?probe:main)->worldAnimatedStats();
                CHECK(after.visibility_dispatches==before.visibility_dispatches+unsigned(mode && gpu_commands && gpu_visibility));
                CHECK(after.bounds_tests==before.bounds_tests+(mode && gpu_commands && gpu_visibility?f.count:0));CHECK(after.bounds_unknown==before.bounds_unknown);
                if(mode)
                {
                    CHECK(after.gpu_draws==before.gpu_draws+(gpu_commands?f.count:0));
                    CHECK(after.direct_draws==before.direct_draws+(gpu_commands?0:f.count));
                    CHECK(after.execute_calls==before.execute_calls+(gpu_commands?2:0)); // Query at count/2 splits this one Bank.
                }
                CHECK((mode?probe:main)->worldSourceCommandStats().cache.declaration_calls==declarations_before+unsigned(mode && gpu_commands));
                f.reclaim(input,pipeline);
            }
            CHECK(pixels[0]==pixels[1]);CHECK(queries[0]==queries[1]);
            for(U32 mode=0;mode<2;++mode)for(U32 at=0;at<f.count;++at)
                CHECK(f.infos[mode*f.count+at]->state->revision()==revisions[mode][at]);
          }
        }
        // The separate shadow API remains the existing native fallback for
        // these records. Preserve its actual rigging/mask and sun/spot states;
        // this does not claim the new G-buffer GPU bank consumes shadows.
        auto destination=f.shadowTarget();auto batches=f.shadows();const auto before_shadow=main->worldAnimatedStats();
        ALMetalWorldSpotShadowScene shadow;shadow.width=shadow.height=Publication::side;shadow.bulk_draws=false;
        shadow.batches=batches.data();shadow.batch_count=batches.size();
        for(auto& batch:batches)
        {
            batch.texture_count=1;batch.coverage=family==1?ALMetalWorldSpotShadowCoverage::legacy_mask:ALMetalWorldSpotShadowCoverage::pbr_mask;
            auto& coverage=batch.coverageData();coverage.textures[0]=f.white->getMetalTexture();coverage.alpha_cutoffs[0]=.5f;
            coverage.pbr_base_color_transforms[0][0][0]=coverage.pbr_base_color_transforms[0][0][1]=1;
        }
        for(U32 camera=0;camera<2;++camera)
        {shadow.depth_clamp=camera==0;REQUIRE(main->renderSpotShadow(*destination,camera,shadow,&f.error));}
        std::vector<F32> depth(Publication::side*Publication::side);
        for(U32 camera=0;camera<2;++camera)
        {REQUIRE(destination->readback(*f.device,0,camera,{},depth.data(),Publication::side*sizeof(F32),&f.error));
         CHECK(std::any_of(depth.begin(),depth.end(),[](F32 value){return value<1.f;}));}
        CHECK(main->worldAnimatedStats().requested_draws==before_shadow.requested_draws);
    }
    // The production direct gate observes producer identity/revision in two
    // DISTINCT outer frames. Replaying main/probe views in one frame cannot
    // qualify it, while a pose change does not change the static producer key.
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);Host f(family,1);ALNativeFrameBudget budget;Inputs::LLPipeline pipeline;
        auto data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(data);U64 epoch=0;
        auto library=data->builtinLibrary(&f.error);REQUIRE(library);
        auto main=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error),probe=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
        REQUIRE(main);REQUIRE(probe);main->setFrameData(data);probe->setFrameData(data);
        auto& info=*f.infos[1];const auto revision=info.state->revision();
        const auto new_frame=[&]{++Inputs::gFrameCount;beginBudgetFrame(budget,Inputs::gFrameCount);data->beginFrame(++epoch);};
        const auto draw=[&](ALMetalWorldGBufferRenderer& renderer,U32 mode)
        {
            auto input=f.adoptArm(pipeline,epoch,mode,false,false);REQUIRE(input);
            std::shared_ptr<ALMetalVisibilityResultBatch> queries;
            auto target=renderer.renderOwnedInputs(*input,&f.error,&queries);INFO(f.error);REQUIRE(target);REQUIRE(queries);
            for(U32 at=0;at<3;++at){U64 samples=0;REQUIRE(queries->wait(at,samples,&f.error));CHECK(samples>0);}
            auto pixels=f.read(*target);f.reclaim(input,pipeline);return pixels;
        };
        const auto raw=[&]
        {f.captureArm(pipeline,0,1,&budget,false,false);return draw(*main,0);};
        new_frame();const auto first_pixels=raw();
        auto before=Inputs::ALScenePublicationNative::animated_compilation_stats;
        const auto first_serial=Inputs::sNativeSceneSerial;
        for(U32 view=0;view<3;++view)
        {
            f.captureArm(pipeline,1,1,&budget,false,true);const auto& capture=*pipeline.mMetalWorldSimpleCapture;
            REQUIRE(capture.source_references.empty());REQUIRE(capture.batches.size()==1);
            CHECK(draw(view?*probe:*main,1)==first_pixels);
            const auto after=Inputs::ALScenePublicationNative::animated_compilation_stats;
            CHECK(after.requests==before.requests);CHECK(after.requested_bytes==before.requested_bytes);CHECK(after.admitted==before.admitted);
            CHECK(after.deferred==before.deferred);CHECK(after.stability_deferred==before.stability_deferred+1);
            CHECK(after.duplicate_views==before.duplicate_views+view);
        }
        CHECK(Inputs::sNativeSceneSerial==first_serial);CHECK(info.state->revision()==revision);
        new_frame();f.pose(.375f,false);const auto moved_pixels=raw();
        before=Inputs::ALScenePublicationNative::animated_compilation_stats;
        f.captureArm(pipeline,1,1,&budget,false,true);const auto& ready=*pipeline.mMetalWorldSimpleCapture;
        REQUIRE(ready.batches.empty());REQUIRE(ready.source_references.size()==1);
        const auto source=ready.source_references.front().animated_source->owner();REQUIRE(source);CHECK_FALSE(source->gpuReady());
        CHECK(draw(*main,1)==moved_pixels);auto after=Inputs::ALScenePublicationNative::animated_compilation_stats;
        CHECK(after.requests==before.requests+1);CHECK(after.admitted==before.admitted+1);CHECK(after.published==before.published+1);
        CHECK(after.requested_bytes>before.requested_bytes);CHECK(info.state->revision()==revision);
        // Ready main/probe repeats, then a new pose in a later outer frame,
        // incur no compilation ticket and keep the exact private source.
        const auto ready_serial=Inputs::sNativeSceneSerial;
        for(U32 frame=0;frame<2;++frame)
        {
            if(frame){new_frame();f.pose(0,false);}
            const auto expected=raw();before=Inputs::ALScenePublicationNative::animated_compilation_stats;
            for(U32 view=0;view<3;++view)
            {
                f.captureArm(pipeline,1,1,&budget,false,true);const auto& capture=*pipeline.mMetalWorldSimpleCapture;
                REQUIRE(capture.source_references.size()==1);CHECK(capture.source_references.front().animated_source==source.get());
                CHECK(draw(view?*probe:*main,1)==expected);
            }
            after=Inputs::ALScenePublicationNative::animated_compilation_stats;
            CHECK(after.requests==before.requests);CHECK(after.admitted==before.admitted);CHECK(after.requested_bytes==before.requested_bytes);
            CHECK(after.stability_deferred==before.stability_deferred);CHECK(after.duplicate_views==before.duplicate_views);
            CHECK(Inputs::sNativeSceneSerial==ready_serial);CHECK(info.state->revision()==revision);
        }
        for(U32 dirty=0;dirty<3;++dirty)
        {
            new_frame();info.state->invalidate();const auto expected=raw();
            before=Inputs::ALScenePublicationNative::animated_compilation_stats;
            for(U32 view=0;view<3;++view)
            {
                f.captureArm(pipeline,1,1,&budget,false,true);const auto& capture=*pipeline.mMetalWorldSimpleCapture;
                REQUIRE(capture.source_references.empty());REQUIRE(capture.batches.size()==1);
                CHECK(draw(view?*probe:*main,1)==expected);
            }
            after=Inputs::ALScenePublicationNative::animated_compilation_stats;
            CHECK(after.requests==before.requests);CHECK(after.requested_bytes==before.requested_bytes);CHECK(after.admitted==before.admitted);
            CHECK(after.deferred==before.deferred);CHECK(after.stability_deferred==before.stability_deferred+1);
            CHECK(after.duplicate_views==before.duplicate_views+2);
        }
    }
}

TEST_CASE("Actual optional animated promotion preserves an exact cache-withdrawn required reader",
          "[almetal][pipeline-buffer-publication][animated-gpu-records][PPD-289]")
{
    using namespace AnimatedGpuRecordActual;
    using namespace Inputs::ALScenePublicationNative;
    struct RestoreHook{~RestoreHook(){capabilityPromotionAllocationForTesting=nullptr;}} restore;
    Host f(1,1);ALNativeFrameBudget budget;Inputs::LLPipeline pipeline;beginBudgetFrame(budget,Inputs::gFrameCount);f.captureArm(pipeline,0,std::numeric_limits<U32>::max(),&budget);
    auto& info=*f.infos[0];auto reader=std::static_pointer_cast<const Payload>(info.state->publication());REQUIRE(reader);
    const auto native=reader->native_tuple;const auto revision=info.state->revision();const auto serial=Inputs::sNativeSceneSerial;
    auto baseline=f.adoptArm(pipeline,1,0);REQUIRE(baseline);auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);REQUIRE(renderer);
    auto original=renderer->renderOwnedInputs(*baseline,&f.error);INFO(f.error);REQUIRE(original);const auto expected=f.read(*original);f.reclaim(baseline,pipeline);
    info.state->evict();REQUIRE_FALSE(info.state->publication());REQUIRE(info.mMetalShadowSource->native_scene_transient.lock().get()==reader.get());
    static unsigned promotions=0;promotions=0;capabilityPromotionAllocationForTesting=[](ALNativePublication::State&){++promotions;throw std::bad_alloc();};
    const auto compiled=animated_source_publications,resources=resource_source_publications;
    const auto cpu_work=animated_compilation_stats;f.captureArm(pipeline,1,0,&budget);
    const auto& capture=*pipeline.mMetalWorldSimpleCapture;CHECK(capture.source_references.empty());REQUIRE(capture.batches.size()==1);
    CHECK(capture.animated_source_stats.requested_draws==1);CHECK(capture.animated_source_stats.source_unavailable==1);
    CHECK(promotions==1);CHECK(Inputs::sNativeSceneSerial==serial);CHECK(animated_source_publications==compiled);CHECK(resource_source_publications==resources);
    CHECK(animated_compilation_stats.admitted==cpu_work.admitted);CHECK(animated_compilation_stats.requested_bytes==cpu_work.requested_bytes);
    CHECK(info.state->revision()==revision);CHECK_FALSE(info.state->publication());CHECK(reader->native_tuple==native);
    auto input=f.adoptArm(pipeline,2,1);INFO(f.error);REQUIRE(input);auto target=renderer->renderOwnedInputs(*input,&f.error);INFO(f.error);REQUIRE(target);
    CHECK(f.read(*target)==expected);f.reclaim(input,pipeline);

    // A real reader with an existing direct certificate remains usable when
    // its cache publisher is withdrawn; a requested GPU superset cannot turn
    // that exact CPU source into either a new publication or missing geometry.
    capabilityPromotionAllocationForTesting=nullptr;++Inputs::gFrameCount;beginBudgetFrame(budget,Inputs::gFrameCount);
    f.captureArm(pipeline,1,1,&budget,false);const auto& direct_capture=*pipeline.mMetalWorldSimpleCapture;
    REQUIRE(direct_capture.source_references.size()==1);const auto* direct_source=direct_capture.source_references.front().animated_source;
    REQUIRE(direct_source);CHECK_FALSE(direct_source->gpuReady());CHECK_FALSE(direct_source->prototype().native_resource_source);
    auto& direct_info=*f.infos[1];auto cpu_reader=std::static_pointer_cast<const Payload>(direct_info.state->publication());REQUIRE(cpu_reader);
    const auto direct_native=cpu_reader->native_tuple;const auto direct_revision=direct_info.state->revision();
    auto data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(data);data->beginFrame(1);renderer->setFrameData(data);
    input=f.adoptArm(pipeline,3,1,false,false);REQUIRE(input);target=renderer->renderOwnedInputs(*input,&f.error);REQUIRE(target);
    const auto direct_expected=f.read(*target);f.reclaim(input,pipeline);
    direct_info.state->evict();REQUIRE_FALSE(direct_info.state->publication());
    REQUIRE(direct_info.mMetalShadowSource->native_scene_transient.lock().get()==cpu_reader.get());
    promotions=0;capabilityPromotionAllocationForTesting=[](ALNativePublication::State&){++promotions;throw std::bad_alloc();};
    const auto direct_compiled=animated_source_publications,direct_resources=resource_source_publications,direct_serial=Inputs::sNativeSceneSerial;
    const auto direct_work=animated_compilation_stats;f.captureArm(pipeline,1,1,&budget,true);
    const auto& fallback=*pipeline.mMetalWorldSimpleCapture;REQUIRE(fallback.batches.empty());REQUIRE(fallback.source_references.size()==1);
    CHECK(fallback.source_references.front().animated_source==direct_source);CHECK(fallback.animated_source_stats.gpu_capability_missing_draws==1);
    CHECK(promotions==1);CHECK(animated_source_publications==direct_compiled);CHECK(resource_source_publications==direct_resources);
    CHECK(Inputs::sNativeSceneSerial==direct_serial);CHECK(animated_compilation_stats.admitted==direct_work.admitted);
    CHECK(animated_compilation_stats.requested_bytes==direct_work.requested_bytes);CHECK_FALSE(direct_info.state->publication());
    CHECK(direct_info.state->revision()==direct_revision);CHECK(cpu_reader->native_tuple==direct_native);
    input=f.adoptArm(pipeline,4,1,false,true);REQUIRE(input);const auto before_direct=renderer->worldAnimatedStats();
    target=renderer->renderOwnedInputs(*input,&f.error);INFO(f.error);REQUIRE(target);CHECK(f.read(*target)==direct_expected);
    const auto after_direct=renderer->worldAnimatedStats();CHECK(after_direct.requested_draws==before_direct.requested_draws+1);
    CHECK(after_direct.fallback_draws==before_direct.fallback_draws+1);CHECK(after_direct.direct_draws==before_direct.direct_draws+1);
    f.reclaim(input,pipeline);
}

TEST_CASE("Actual animated GPU complete costs retain small cold warm pose and resource churn cases",
          "[almetal][.native-animated-gpu-record-cost]")
{
    using namespace AnimatedGpuRecordActual;
    constexpr U32 samples=12;
    // One setting per whole two-arm run. Do not read the environment inside a
    // capture/frame timer, or silently combine culler and fullcount samples.
    const char* visibility_env=std::getenv("ALCHEMY_ANIMATED_GPU_VISIBILITY");
    if(visibility_env)REQUIRE((std::strcmp(visibility_env,"0")==0 || std::strcmp(visibility_env,"1")==0));
    const bool gpu_visibility=visibility_env && std::strcmp(visibility_env,"1")==0;
    const char* commands_env=std::getenv("ALCHEMY_ANIMATED_GPU_COMMANDS");
    if(commands_env)REQUIRE((std::strcmp(commands_env,"0")==0 || std::strcmp(commands_env,"1")==0));
    const bool gpu_commands=commands_env && std::strcmp(commands_env,"1")==0;
    const char* stability_env=std::getenv("ALCHEMY_ANIMATED_SOURCE_STABILITY");
    if(stability_env)REQUIRE((std::strcmp(stability_env,"0")==0 || std::strcmp(stability_env,"1")==0));
    const bool source_stability=!gpu_commands && (!stability_env || std::strcmp(stability_env,"1")==0);
    struct Sample
    {
        double cpu=0,encode=0,settle=0,gpu=0;U64 gpu_ready=0,seals=0,source_builds=0,animated_builds=0;
        U64 requested=0,gpu_draws=0,fallback=0,executes=0,poses=0,source_gpu=0,bounds=0,unknown=0,bytes=0,cpu_admitted=0,cpu_deferred=0,metadata_deferred=0,captured=0,visibility_dispatches=0,declaration_calls=0;
        U64 direct_draws=0,direct_common_builds=0,direct_common_reuses=0,direct_recipe_reads=0,direct_source_builds=0,gpu_source_builds=0;
        U64 cpu_stability_deferred=0,direct_pose_publications=0,direct_pose_cache_hits=0;
    };
    struct Arm
    {ALNativeFrameBudget budget;Inputs::LLPipeline pipeline;std::shared_ptr<ALMetalWorldFrameData> data;std::unique_ptr<ALMetalWorldGBufferRenderer> main,probe;U64 epoch=0;};
    const auto cpu=[] {timespec value{};REQUIRE(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0);return U64(value.tv_sec)*1'000'000'000ULL+U64(value.tv_nsec);};
    const auto median=[](std::array<double,samples> values){std::sort(values.begin(),values.end());return (values[5]+values[6])*.5;};
    struct TimingRestore{bool old=ALMetalDevice::commandTimingEnabled();~TimingRestore(){ALMetalDevice::setCommandTimingEnabled(old);}} restore;
    ALMetalDevice::setCommandTimingEnabled(true);const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(8);
    for(U32 family:{1u,2u})for(U32 count:{1u,2u,4u,64u})for(U32 views:{1u,3u})
    {
        if(views==3 && count<4)continue;
        Host f(family,count);std::array<Arm,2> arms;
        for(auto& arm:arms)
        {
            arm.data=ALMetalWorldFrameData::create(*f.device,&f.error);REQUIRE(arm.data);auto library=arm.data->builtinLibrary(&f.error);REQUIRE(library);
            arm.main=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);arm.probe=ALMetalWorldGBufferRenderer::create(*f.device,1,library,&f.error);
            REQUIRE(arm.main);REQUIRE(arm.probe);arm.main->setFrameData(arm.data);arm.probe->setFrameData(arm.data);configureMetadataAdmission(*arm.data,arm.budget);
        }
        for(U32 condition=0;condition<4;++condition)
        {
            CAPTURE(family,count,views,condition);for(auto& info:f.infos)info->state->invalidate();
            const U32 warmups=count==64 && condition!=0?16:2;
            std::array<std::array<Sample,samples>,2> measured;
            for(U32 sample=0;sample<samples+warmups;++sample)
            {
                ++Inputs::gFrameCount;std::array<std::array<GroupPublication::Image,3>,2> images;
                std::array<std::array<std::array<U64,3>,3>,2> queries{};
                for(U32 position=0;position<2;++position)
                {
                    REQUIRE(std::chrono::steady_clock::now()<deadline);const U32 mode=(sample+position)%2;auto& arm=arms[mode];CAPTURE(sample,mode);
                    const auto before=arm.data->stats();const auto sealed=f.seals(),serial=Inputs::sNativeSceneSerial,compiled=Inputs::ALScenePublicationNative::animated_source_publications;
                    const auto before_compile=Inputs::ALScenePublicationNative::animated_compilation_stats;
                    const auto direct_compiled=Inputs::ALScenePublicationNative::animated_direct_source_publications;
                    const auto gpu_compiled=Inputs::ALScenePublicationNative::animated_gpu_source_publications;
                    const auto declarations_before=arm.main->worldSourceCommandStats().cache.declaration_calls;
                    const std::array<ALMetalWorldAnimatedStats,2> old{arm.main->worldAnimatedStats(),arm.probe->worldAnimatedStats()};
                    std::array<std::unique_ptr<ALMetalRenderTarget>,3> targets;std::array<std::shared_ptr<ALMetalVisibilityResultBatch>,3> results;
                    std::array<std::shared_ptr<const ALMetalCommandCompletion>,3> completions;Sample value;
                    const auto started=std::chrono::steady_clock::now();const auto cpu_start=cpu();beginBudgetFrame(arm.budget,Inputs::gFrameCount);arm.data->beginFrame(++arm.epoch);
                    const auto before_budget=arm.budget.snapshot();
                    f.pose(condition==2 && sample&1?.375f:0.f,false); // Both arms pay actual CPU record sealing each outer frame.
                    if(condition==0)for(U32 at=0;at<count;++at)f.infos[mode*count+at]->state->invalidate();
                    if(condition==3)f.changeResources(mode,sample);
                    for(U32 view=0;view<views;++view)
                    {
                        f.model=LLMatrix4a::translation(.01f*view,-.005f*view,0);f.captureArm(arm.pipeline,mode,std::numeric_limits<U32>::max(),&arm.budget,gpu_commands,source_stability);
                        value.captured+=arm.pipeline.mMetalWorldSimpleCapture->source_references.size();
                        auto input=f.adoptArm(arm.pipeline,arm.epoch,mode,gpu_visibility,gpu_commands);INFO(f.error);REQUIRE(input);
                        auto& renderer=view?*arm.probe:*arm.main;REQUIRE(renderer.beginCommandBatch("Actual animated GPU complete path",&f.error));
                        targets[view]=renderer.renderOwnedInputs(*input,&f.error,&results[view]);INFO(f.error);REQUIRE(targets[view]);REQUIRE(results[view]);
                        completions[view]=renderer.commandBatchCompletion();f.reclaim(input,arm.pipeline);REQUIRE(renderer.commitCommandBatch(&f.error));
                    }
                    const auto encoded=std::chrono::steady_clock::now();REQUIRE(f.device->waitUntilIdle(&f.error));
                    for(U32 view=0;view<views;++view)for(U32 at=0;at<3;++at)
                    {REQUIRE(results[view]->wait(at,queries[mode][view][at],&f.error));CHECK(queries[mode][view][at]>0);}
                    gGL.frameFences().release();const auto settled=std::chrono::steady_clock::now();value.cpu=(cpu()-cpu_start)/1e6;
                    value.encode=std::chrono::duration<double,std::milli>(encoded-started).count();value.settle=std::chrono::duration<double,std::milli>(settled-started).count();
                    for(U32 view=0;view<views;++view)
                    {ALMetalCommandGPUTime timing;if(completions[view]&&completions[view]->gpuTime(timing)==ALMetalCommandTimingStatus::ready){value.gpu+=timing.elapsed_nanoseconds/1e6;++value.gpu_ready;}}
                    const std::array<ALMetalWorldAnimatedStats,2> now{arm.main->worldAnimatedStats(),arm.probe->worldAnimatedStats()};
                    for(U32 renderer=0;renderer<2;++renderer)
                    {
                        value.requested+=now[renderer].requested_draws-old[renderer].requested_draws;value.gpu_draws+=now[renderer].gpu_draws-old[renderer].gpu_draws;
                        value.fallback+=now[renderer].fallback_draws-old[renderer].fallback_draws;value.executes+=now[renderer].execute_calls-old[renderer].execute_calls;
                        value.poses+=now[renderer].current_pose_publications-old[renderer].current_pose_publications;value.source_gpu+=now[renderer].source_publications-old[renderer].source_publications;
                        value.bounds+=now[renderer].bounds_tests-old[renderer].bounds_tests;value.unknown+=now[renderer].bounds_unknown-old[renderer].bounds_unknown;
                        value.visibility_dispatches+=now[renderer].visibility_dispatches-old[renderer].visibility_dispatches;
                        value.direct_draws+=now[renderer].direct_draws-old[renderer].direct_draws;
                        value.direct_common_builds+=now[renderer].direct_common_builds-old[renderer].direct_common_builds;
                        value.direct_common_reuses+=now[renderer].direct_common_reuses-old[renderer].direct_common_reuses;
                        value.direct_recipe_reads+=now[renderer].direct_recipe_reads-old[renderer].direct_recipe_reads;
                        value.direct_pose_publications+=now[renderer].direct_pose_publications-old[renderer].direct_pose_publications;
                        value.direct_pose_cache_hits+=now[renderer].direct_pose_cache_hits-old[renderer].direct_pose_cache_hits;
                    }
                    value.seals=f.seals()-sealed;value.source_builds=Inputs::sNativeSceneSerial-serial;
                    value.animated_builds=Inputs::ALScenePublicationNative::animated_source_publications-compiled;value.bytes=arm.data->stats().published_bytes-before.published_bytes;
                    value.direct_source_builds=Inputs::ALScenePublicationNative::animated_direct_source_publications-direct_compiled;
                    value.gpu_source_builds=Inputs::ALScenePublicationNative::animated_gpu_source_publications-gpu_compiled;
                    value.cpu_admitted=Inputs::ALScenePublicationNative::animated_compilation_stats.admitted-before_compile.admitted;
                    value.cpu_deferred=Inputs::ALScenePublicationNative::animated_compilation_stats.deferred-before_compile.deferred;
                    value.cpu_stability_deferred=Inputs::ALScenePublicationNative::animated_compilation_stats.stability_deferred-before_compile.stability_deferred;
                    value.metadata_deferred=arm.budget.snapshot().deferred-before_budget.deferred;
                    CHECK(value.seals==1);CHECK(value.requested==(gpu_commands?value.captured:0));CHECK(value.gpu_draws+value.fallback==value.requested);
                    CHECK(value.direct_draws==(gpu_commands?value.fallback:value.captured));
                    CHECK(value.direct_recipe_reads==value.direct_draws);CHECK(value.direct_common_builds+value.direct_common_reuses==value.direct_draws);
                    CHECK(value.direct_pose_publications+value.direct_pose_cache_hits==value.direct_draws);
                    CHECK(value.animated_builds==value.direct_source_builds+value.gpu_source_builds);
                    if(!gpu_commands || !gpu_visibility){CHECK(value.visibility_dispatches==0);CHECK(value.bounds==0);CHECK(value.unknown==0);}
                    if(sample>=warmups && condition==1){CHECK(value.source_builds==0);CHECK(value.animated_builds==0);}
                    if(sample>=warmups)measured[mode][sample-warmups]=value;
                    // PPD-293: sample the shared FrameData cache ONCE through
                    // main, after BOTH owner-CPU/settlement timers stopped.
                    // Metadata mixes physical backing and logical table debt;
                    // native binding/asset scopes overlap and are not net RAM.
                    const auto pages=arm.main->worldSourceStats().pages;
                    const auto commands=arm.main->worldSourceCommandStats().cache;
                    value.declaration_calls=commands.declaration_calls-declarations_before;
                    if(sample>=warmups)measured[mode][sample-warmups].declaration_calls=value.declaration_calls;
                    if(!mode)CHECK(value.declaration_calls==0);
                    CHECK(value.declaration_calls<=views);
                    if(value.gpu_draws==U64(count)*views)CHECK(value.declaration_calls==views);
                    const ALMetalWorldSourcePageLimits page_limits{};
                    const ALMetalWorldSourceCommandLimits command_limits{};
                    CHECK(pages.live_metadata_bytes<=page_limits.maximum_metadata_bytes);
                    if(pages.cached_cohort_native_asset_limit)
                        CHECK(pages.cached_cohort_native_asset_bytes<=pages.cached_cohort_native_asset_limit);
                    if(commands.retained_binding_limit_bytes)
                        CHECK(commands.unique_retained_binding_bytes<=commands.retained_binding_limit_bytes);
                    CHECK(commands.cached_banks<=command_limits.maximum_cached_banks);CHECK(commands.live_banks<=command_limits.maximum_live_banks);
                    CHECK(commands.live_icb_allocated_bytes<=command_limits.maximum_live_native_bytes);
                    if(commands.live_icb_allocated_bytes<=command_limits.maximum_live_native_bytes)
                        CHECK(commands.unique_retained_index_bytes<=command_limits.maximum_live_native_bytes-commands.live_icb_allocated_bytes);
                    if(sample==warmups-1||sample==samples+warmups-1)for(U32 view=0;view<views;++view)images[mode][view]=f.read(*targets[view]);
                    std::cout<<"NATIVE_ANIMATED_GPU_RAW family="<<family<<" sources="<<count<<" indices_per_source=96 views="<<views<<" condition="<<condition<<" mode="<<mode<<" sample="<<sample<<" warmup="<<(sample<warmups)
                        <<" gpu_commands="<<unsigned(gpu_commands)<<" source_stability="<<unsigned(source_stability)<<" gpu_visibility="<<unsigned(gpu_visibility)<<" visibility_dispatches="<<value.visibility_dispatches<<" declaration_calls="<<value.declaration_calls
                        <<" direct_draws="<<value.direct_draws<<" direct_common_builds="<<value.direct_common_builds<<" direct_common_reuses="<<value.direct_common_reuses<<" direct_recipe_reads="<<value.direct_recipe_reads
                        <<" direct_source_builds="<<value.direct_source_builds<<" gpu_source_builds="<<value.gpu_source_builds
                        <<" direct_pose_publications="<<value.direct_pose_publications<<" direct_pose_cache_hits="<<value.direct_pose_cache_hits<<" cpu_stability_deferred="<<value.cpu_stability_deferred
                        <<" owner_cpu_ms="<<value.cpu<<" capture_adopt_encode_reclaim_ms="<<value.encode<<" commit_query_settle_ms="<<value.settle<<" gpu_sum_ms="<<value.gpu<<" gpu_ready="<<value.gpu_ready
                        <<" pose_seals="<<value.seals<<" source_builds="<<value.source_builds<<" animated_builds="<<value.animated_builds<<" requested="<<value.requested<<" gpu_draws="<<value.gpu_draws<<" fallback="<<value.fallback
                        <<" execute_calls="<<value.executes<<" pose_publications="<<value.poses<<" source_gpu_publications="<<value.source_gpu<<" bounds_tests="<<value.bounds<<" bounds_unknown="<<value.unknown<<" arena_logical_bytes="<<value.bytes
                        <<" captured_animated_references="<<value.captured<<" required_raw_draws="<<(U64(count)*views-value.captured)
                        <<" cpu_compile_admitted="<<value.cpu_admitted<<" cpu_compile_deferred="<<value.cpu_deferred<<" shared_scene_deferred="<<value.metadata_deferred
                        <<" page_live_metadata_bytes="<<pages.live_metadata_bytes<<" page_metadata_limit_bytes="<<page_limits.maximum_metadata_bytes
                        <<" page_cached_native_asset_bytes="<<pages.cached_cohort_native_asset_bytes<<" page_cached_native_asset_limit_bytes="<<pages.cached_cohort_native_asset_limit
                        <<" command_unique_retained_binding_bytes="<<commands.unique_retained_binding_bytes<<" command_retained_binding_limit_bytes="<<commands.retained_binding_limit_bytes
                        <<" command_live_icb_allocated_bytes="<<commands.live_icb_allocated_bytes<<" command_unique_retained_index_bytes="<<commands.unique_retained_index_bytes
                        <<" command_cached_banks="<<commands.cached_banks<<" command_live_banks="<<commands.live_banks
                        <<" command_cache_limit_banks="<<command_limits.maximum_cached_banks<<" command_live_limit_banks="<<command_limits.maximum_live_banks
                        <<" command_icb_index_limit_bytes="<<command_limits.maximum_live_native_bytes<<" cache_census_shared_fd_once=1 cache_census_outside_timer=1"
                        <<" target_wrapper_retirement=outside_timer source_geometry=independent_real_mesh frame_data=independent_arm source_metadata=independent_arm current_pose=resealed_per_arm_frame shared_scene_budget=production_limits idle_budget_inputs=1 all_raw_samples=1 whole_viewer_fps_claim=0\n";
                }
                for(U32 view=0;view<views;++view)
                {CHECK(queries[0][view]==queries[1][view]);if(sample==warmups-1||sample==samples+warmups-1)CHECK(images[0][view]==images[1][view]);}
            }
            for(U32 mode=0;mode<2;++mode)
            {
                std::array<double,samples> cpu_values,encode,settle,gpu;U64 fallback=0,draws=0,visibility_dispatches=0,declaration_calls=0,direct_draws=0,direct_common_builds=0,direct_common_reuses=0,direct_recipe_reads=0,direct_pose_publications=0,direct_pose_cache_hits=0,cpu_stability_deferred=0;
                for(U32 sample=0;sample<samples;++sample){const auto& value=measured[mode][sample];cpu_values[sample]=value.cpu;encode[sample]=value.encode;settle[sample]=value.settle;gpu[sample]=value.gpu;fallback+=value.fallback;draws+=value.gpu_draws;visibility_dispatches+=value.visibility_dispatches;declaration_calls+=value.declaration_calls;direct_draws+=value.direct_draws;direct_common_builds+=value.direct_common_builds;direct_common_reuses+=value.direct_common_reuses;direct_recipe_reads+=value.direct_recipe_reads;direct_pose_publications+=value.direct_pose_publications;direct_pose_cache_hits+=value.direct_pose_cache_hits;cpu_stability_deferred+=value.cpu_stability_deferred;}
                std::cout<<"NATIVE_ANIMATED_GPU_COST family="<<family<<" sources="<<count<<" indices_per_source=96 views="<<views<<" condition="<<condition<<" mode="<<mode<<" samples="<<samples<<" warmups="<<warmups
                    <<" gpu_commands="<<unsigned(gpu_commands)<<" source_stability="<<unsigned(source_stability)<<" gpu_visibility="<<unsigned(gpu_visibility)<<" visibility_dispatches="<<visibility_dispatches<<" declaration_calls="<<declaration_calls
                    <<" direct_draws="<<direct_draws<<" direct_common_builds="<<direct_common_builds<<" direct_common_reuses="<<direct_common_reuses<<" direct_recipe_reads="<<direct_recipe_reads
                    <<" direct_pose_publications="<<direct_pose_publications<<" direct_pose_cache_hits="<<direct_pose_cache_hits<<" cpu_stability_deferred="<<cpu_stability_deferred
                    <<" owner_cpu_median_ms="<<median(cpu_values)<<" owner_cpu_max_ms="<<*std::max_element(cpu_values.begin(),cpu_values.end())<<" encode_reclaim_median_ms="<<median(encode)<<" encode_reclaim_max_ms="<<*std::max_element(encode.begin(),encode.end())
                    <<" settle_median_ms="<<median(settle)<<" settle_max_ms="<<*std::max_element(settle.begin(),settle.end())<<" gpu_sum_median_ms="<<median(gpu)<<" gpu_sum_max_ms="<<*std::max_element(gpu.begin(),gpu.end())<<" gpu_draws="<<draws<<" fallback_draws="<<fallback
                    <<" cold_and_adverse_cells_retained=1 geometry_texture_creation=excluded mutable_skeleton=typed_attachment_palette_input target_wrapper_retirement=outside_timer whole_viewer_fps_claim=0\n";
            }
        }
    }
}
