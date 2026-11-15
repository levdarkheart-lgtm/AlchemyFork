/**
 * @file alpipeline_static_restoration_native.inl
 * @brief Actual static CPU restoration, weak generations and complete costs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
namespace
{
namespace StaticRestorationTest
{
using namespace Inputs::ALSceneGroupPublicationNative;
struct Rendered
{
    GroupPublication::Image image;
    std::array<U64,2> queries{};
    bool operator==(const Rendered&) const=default;
};
Rendered render(MaterialGroupPublication& f,const std::shared_ptr<const NativeGroup>& packet,
                ALMetalWorldGBufferRenderer& renderer)
{
    Inputs::LLPipeline pipeline;auto& capture=*pipeline.mMetalWorldSimpleCapture;
    f.configure(capture,false,false);Inputs::LLCullResult cull;
    if(packet)
        REQUIRE((f.type==Inputs::LLRenderPass::PASS_SIMPLE?
            cull.pushNativeSimpleRange(packet,U32(packet->sources.size()),f.region.snapshot()):
            cull.pushNativeMaterialRange(f.type,packet,U32(packet->sources.size()),f.region.snapshot())));
    else for(const auto& info:f.infos)cull.pushDrawInfo(f.type,info.get());
    {CullScope scope(cull);f.capture(pipeline);}REQUIRE(capture.complete);
    std::vector<ALMetalWorldVisibilityQuery> queries(2);
    for(U32 at=0;at<2;++at)
    {
        auto& query=queries[at];std::memcpy(query.modelview_projection,f.model.getF32ptr(),sizeof(query.modelview_projection));
        query.center[0]=.35f;query.center[1]=-.5f;query.center[2]=.75f;
        query.half_extent[0]=query.half_extent[1]=query.half_extent[2]=.08f;
        query.batch_offset=at?capture.logicalDrawCount():0;query.fan=7;query.count_samples=1;
    }
    auto scene=f.scene(capture);scene.visibility_queries=queries.data();scene.visibility_query_count=queries.size();
    auto inputs=ALMetalWorldViewInputs::adopt(scene,std::move(capture.batches),std::move(capture.source_references),
        std::move(capture.draw_order),std::move(capture.source_cohorts),std::move(queries),std::move(capture.cpu_sources),
        capture.world_resources,Inputs::gFrameCount,&f.error);INFO(f.error);REQUIRE(inputs);
    std::shared_ptr<ALMetalVisibilityResultBatch> results;
    auto target=renderer.renderOwnedInputs(*inputs,&f.error,&results);INFO(f.error);REQUIRE(target);REQUIRE(results);
    Rendered output;output.image=f.read(*target);
    for(U32 at=0;at<2;++at)REQUIRE(results->wait(at,output.queries[at],&f.error));
    REQUIRE(ALMetalWorldViewInputs::reclaimCpuStorage(inputs,capture.batches,capture.source_references,
        capture.draw_order,capture.source_cohorts,queries));capture.clearDraws();return output;
}
void evict(MaterialGroupPublication& f)
{
    f.group->evict();for(const auto& info:f.infos)info->state->evict();gGL.frameFences().release();
}
inline U32 fault_calls=0,fault_after=0;
void allocationFailure(){if(fault_calls++==fault_after)throw std::bad_alloc{};}
struct FaultRestore
{~FaultRestore(){staticRestorationAllocationForTesting=nullptr;}};
}
}

TEST_CASE("Actual compact restoration keeps active values and restores expired proofs with native numeric reuse",
          "[almetal][pipeline-buffer-publication][static-restoration][PPD-256]")
{
    using namespace StaticRestorationTest;
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family,family==1?14:1);
        f.materialDraw(false);f.materialDraw(true);
        if(family==2)for(U32 at=0;at<f.materials.size();++at)
        {
            f.materials[at]->mBaseColor=LLColor4(.25f,.5f,.75f,.875f);
            f.materials[at]->mTextureTransform[1].packed[4]=F32(at)*.125f;
            f.materials[at]->mEmissiveColor=LLColor3(.1f,.2f,.3f);
        }
        auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);REQUIRE(renderer);
        auto packet=f.sealMaterial();REQUIRE(packet);const auto expected=render(f,packet,*renderer);
        const auto rows=f.group_layout->sources;
        std::vector<std::weak_ptr<const Inputs::ALScenePublicationNative::Payload>> worlds;
        std::vector<std::weak_ptr<const ALMetalWorldCohortSource>> proofs;
        std::vector<std::shared_ptr<const ALMetalWorldMaterialSource>> external_materials;
        for(const auto& source:packet->sources)
        {
            REQUIRE(source.owner->restoration);const auto& tuple=*source.owner->restoration;const auto& batch=*source.prototype;
            CHECK(tuple.diffuse.size()==batch.texture_count);CHECK(tuple.material.size()==batch.material_slot_count);
            CHECK(tuple.pbr.size()==batch.pbr_slot_count);CHECK(tuple.ownedBytes()<sizeof(ALMetalWorldSimpleBatch));
            if(family==2)for(U32 slot=0;slot<batch.pbr_slot_count;++slot)
            {
                const auto& original=batch.pbrSlots()[slot];const auto& saved=tuple.pbr[slot];
                CHECK(std::equal(std::begin(original.base_color_factor),std::end(original.base_color_factor),saved.base_color_factor));
                CHECK(std::equal(&original.texture_transform[0][0],&original.texture_transform[0][0]+32,&saved.texture_transform[0][0]));
                CHECK(std::equal(std::begin(original.emissive_color),std::end(original.emissive_color),saved.emissive_color));
            }
            worlds.push_back(source.world_owner);proofs.push_back(source.owner->cohort_source);
            external_materials.push_back(batch.native_material_source);
        }
        packet.reset();evict(f);for(const auto& world:worlds)CHECK(world.expired());for(const auto& proof:proofs)CHECK(proof.expired());
        const auto reads=f.reads();const auto before=statistics;const auto native=alMetalResourceAllocationStats();
        auto restored=f.sealMaterial();REQUIRE(restored);CHECK(f.reads()==reads);
        CHECK(statistics.restore_tuple_builds==before.restore_tuple_builds);
        CHECK(statistics.restore_tuple_expansions==before.restore_tuple_expansions+rows.size());
        CHECK(statistics.restore_live_proof_reuses==before.restore_live_proof_reuses);
        CHECK(alMetalResourceAllocationStats().buffers_created==native.buffers_created);
        for(U32 row=0;row<rows.size();++row)
        {
            CHECK(restored->sources[row].owner==rows[row]);const auto& material=restored->sources[row].prototype->native_material_source;
            CHECK(material!=external_materials[row]);
            CHECK(material->materialLease()==external_materials[row]->materialLease());
            CHECK(material->pbrLease(false)==external_materials[row]->pbrLease(false));
            CHECK(material->pbrLease(true)==external_materials[row]->pbrLease(true));
        }
        CHECK(render(f,restored,*renderer)==expected);
        const auto warm=statistics;
        for(U32 view=0;view<3;++view){CHECK(f.currentMaterial()==restored);CHECK(f.sealMaterial()==restored);}
        CHECK(statistics.restore_tuple_expansions==warm.restore_tuple_expansions);CHECK(f.reads()==reads);
        std::cout<<"NATIVE_STATIC_RESTORE_EXPIRED family="<<family<<" rows="<<rows.size()
            <<" expansions="<<statistics.restore_tuple_expansions-before.restore_tuple_expansions
            <<" externally_live_material_sources="<<external_materials.size()<<" native_buffer_allocations=0\n";
    }
}

TEST_CASE("Actual compact layouts reuse live private proofs and SIMPLE has no restoration batch",
          "[almetal][pipeline-buffer-publication][static-restoration]")
{
    using namespace StaticRestorationTest;
    for(U32 family:{0u,1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);for(U32 row=0;row<4;++row)f.materialDraw();
        auto packet=f.sealMaterial();REQUIRE(packet);
        std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> proofs;
        for(const auto& source:packet->sources)
        {
            if(family){REQUIRE(source.owner->restoration);proofs.push_back(source.owner->cohort_source.lock());}
            else{CHECK_FALSE(source.owner->restoration);CHECK(source.prototype==&source.owner->simple_proof->source());proofs.push_back(source.owner->simple_proof);}
            REQUIRE(proofs.back());
        }
        packet.reset();evict(f);const auto before=statistics;const auto reads=f.reads();
        auto restored=f.sealMaterial();REQUIRE(restored);CHECK(f.reads()==reads);
        CHECK(statistics.restore_tuple_expansions==before.restore_tuple_expansions);
        if(family)CHECK(statistics.restore_live_proof_reuses==before.restore_live_proof_reuses+4);
        for(U32 row=0;row<4;++row)CHECK(&restored->cohort->source(row)==&proofs[row]->source());
        auto rebased=f.model;rebased.mMatrix[3].set(.25f,-.5f,0.f,1.f);f.model=rebased;
        REQUIRE(f.region.update(f.model,LLVector3(.25f,-.5f,0.f)));CHECK(f.currentMaterial()==restored);
        CHECK(statistics.restore_tuple_expansions==before.restore_tuple_expansions);CHECK(f.reads()==reads);
    }
}

TEST_CASE("Actual compact restoration allocation failures preserve whole raw fallback and retained layouts",
          "[almetal][pipeline-buffer-publication][static-restoration]")
{
    using namespace StaticRestorationTest;FaultRestore reset;
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);for(U32 row=0;row<3;++row)f.materialDraw();
        auto renderer=ALMetalWorldGBufferRenderer::create(*f.device,&f.error);REQUIRE(renderer);
        fault_calls=0;fault_after=1;staticRestorationAllocationForTesting=&allocationFailure;
        CHECK_FALSE(f.sealMaterial());CHECK_FALSE(f.group->publication());CHECK_FALSE(f.group_layout);
        staticRestorationAllocationForTesting=nullptr;const auto raw=render(f,{},*renderer);
        auto packet=f.sealMaterial();REQUIRE(packet);CHECK(render(f,packet,*renderer)==raw);
        auto layout=f.group_layout;const auto rows=layout->sources;packet.reset();evict(f);
        fault_calls=0;fault_after=1;staticRestorationAllocationForTesting=&allocationFailure;
        CHECK_FALSE(f.sealMaterial());CHECK_FALSE(f.group->publication());CHECK(f.group_layout==layout);CHECK(layout->sources==rows);
        staticRestorationAllocationForTesting=nullptr;auto recovered=f.sealMaterial();REQUIRE(recovered);
        CHECK(recovered->sources.size()==3);CHECK(render(f,recovered,*renderer)==raw);
        recovered.reset();evict(f);
        // The layout's CPU description cannot manufacture an expired native
        // generation. This uses the existing deliberate detached-signal seam.
        // PPD258: replacement admission may clear the old Layout container;
        // the immutable Source snapshot already retained above stays valid.
        auto& info=*f.infos[0];const auto old=rows[0];
        info.mVertexBuffer->nativePublication().forget(info.state);info.mVertexBuffer=f.buffer();gGL.frameFences().release();
        // PPD260: queue/readback completion does not itself join the older
        // native command's CPU completion-handler retirement.
        const auto retired_by=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!old->vertices.expired() && std::chrono::steady_clock::now()<retired_by)std::this_thread::yield();
        REQUIRE(old->vertices.expired());const auto before=statistics.restore_tuple_expansions;
        CHECK_FALSE(restoreMaterial(*old,info.state,Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount));
        CHECK(statistics.restore_tuple_expansions==before);
    }
}

TEST_CASE("Actual compact static restoration complete small group costs",
          "[almetal][.native-static-restoration-cost]")
{actualDirectCohortCosts({0,1,2},{1,2,4},true);}

TEST_CASE("Actual expired and live private proof restoration complete costs",
          "[almetal][.native-expired-static-restoration-cost]")
{actualDirectCohortCosts({1,2},{1,2,4},true,{3,4});}
