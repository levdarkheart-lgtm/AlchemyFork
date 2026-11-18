/**
 * @file alpipeline_producer_preparation_native.inl
 * @brief Actual source factories prepared on the producer before view capture.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

TEST_CASE("Producer preparation makes the first required source capture a warm hit",
          "[almetal][pipeline-buffer-publication][producer-preparation]")
{
    using namespace Inputs::ALScenePublicationNative;
    Publication f;Inputs::LLDrawInfo info;
    info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    ALNativeFrameBudget budget;ALNativeFrameBudget::FrameInputs frame;frame.frame=1;budget.beginFrame(frame);
    ALNativeCohortCompilationAdmission admission;U64 serial=0;
    const std::array requests{ProducerWorldRequest{producerSource(info),WorldFlavor{}}};
    const auto prepared=prepareProducerSources(requests,{},Inputs::sNativeSceneSources,serial,1,admission,budget,
        sizeof(Payload)+sizeof(WorldVariant),[]{return true;},[](const auto&,auto&){return false;});
    REQUIRE(prepared);CHECK(prepared.status==ProducerPreparationStatus::prepared);
    CHECK(prepared.prepared==1);CHECK(prepared.warm==0);CHECK_FALSE(admission.pending());
    CHECK(budget.snapshot().scene_publications_this_frame==1);
    CHECK(budget.snapshot().scene_publications_in_flight==0);
    const auto reads=f.image.image_reads;const auto published=serial;
    auto source=world(info,WorldFlavor{},Inputs::sNativeSceneSources,serial,1);REQUIRE(source);
    CHECK(source.reused);CHECK(serial==published);CHECK(f.image.image_reads==reads);
    CHECK(source.prototype->vertices==info.mVertexBuffer->getMetalVertexBuffer());
    // The producer source is the same real source used by the required view,
    // not a synthetic observer-only publication.
    // The stable prototype deliberately has no live view transform. Add the
    // authored current view only to this local required consumer instance.
    auto instance=*source.prototype;
    for(U32 i=0;i<16;++i)
    {
        CHECK(source.prototype->modelview_projection[i]==0.f);
        instance.modelview_projection[i]=instance.modelview[i]=(i%5==0?1.f:0.f);
    }
    for(U32 axis=0;axis<3;++axis)instance.normal_matrix[axis][axis]=1.f;
    const auto pixels=f.render(instance);
    CHECK(std::any_of(pixels.begin(),pixels.end(),[](const auto& p){return p[0]>0;}));
}

TEST_CASE("Producer preparation reuses explicit legacy and PBR material variants",
          "[almetal][pipeline-buffer-publication][producer-preparation]")
{
    using namespace Inputs::ALScenePublicationNative;
    for(U32 family:{1u,2u})
    {
        CAPTURE(family);MaterialGroupPublication f(family);auto& info=f.materialDraw();
        ALNativeFrameBudget budget;ALNativeFrameBudget::FrameInputs frame;frame.frame=1;budget.beginFrame(frame);
        ALNativeCohortCompilationAdmission admission;U64 serial=0;
        const std::array requests{ProducerWorldRequest{producerSource(info),f.flavor}};
        auto result=prepareProducerSources(requests,{},Inputs::sNativeSceneSources,serial,1,admission,budget,
            sizeof(Payload)+sizeof(WorldVariant)+4096,[]{return true;},[](const auto&,auto&){return false;});
        REQUIRE(result);CHECK(result.prepared==1);
        const auto reads=f.reads();const auto old_serial=serial;
        auto source=world(info,f.flavor,Inputs::sNativeSceneSources,serial,1);REQUIRE(source);
        CHECK(source.reused);CHECK(serial==old_serial);CHECK(f.reads()[2]==reads[2]);
        CHECK(source.prototype->native_material_source);CHECK(source.prototype->native_resource_source);
    }
}

TEST_CASE("Warm producer chunks bypass admission while denied cold work preserves required fallback",
          "[almetal][pipeline-buffer-publication][producer-preparation]")
{
    using namespace Inputs::ALScenePublicationNative;
    Publication f;Inputs::LLDrawInfo info;info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    U64 serial=0;const std::array requests{ProducerWorldRequest{producerSource(info),WorldFlavor{}}};
    auto original=world(info,WorldFlavor{},Inputs::sNativeSceneSources,serial,1);REQUIRE(original);
    ALNativeFrameBudget budget;budget.beginDrain();ALNativeCohortCompilationAdmission admission;
    const auto reads=f.image.image_reads;
    auto warm=prepareProducerSources(requests,{},Inputs::sNativeSceneSources,serial,2,admission,budget,4096,
        []{return true;},[](const auto&,auto&){return false;});
    REQUIRE(warm);CHECK(warm.status==ProducerPreparationStatus::ready);CHECK(warm.warm==1);
    CHECK_FALSE(admission.lastConsideredFrame());CHECK_FALSE(admission.pending());CHECK(f.image.image_reads==reads);
    info.state->invalidate();
    const std::array changed{ProducerWorldRequest{producerSource(info),WorldFlavor{}}};
    const auto before=serial;
    auto denied=prepareProducerSources(changed,{},Inputs::sNativeSceneSources,serial,2,admission,budget,4096,
        []{return true;},[](const auto&,auto&){return false;});
    CHECK_FALSE(denied);CHECK(denied.status==ProducerPreparationStatus::deferred);
    CHECK(serial==before);CHECK_FALSE(info.state->publication());CHECK(f.image.image_reads==reads);
    auto fallback=world(info,WorldFlavor{},Inputs::sNativeSceneSources,serial,2);REQUIRE(fallback);
    CHECK_FALSE(fallback.reused);CHECK(fallback.prototype->index_count==info.mCount);
}

TEST_CASE("Producer chunk bounds and stale identities fail before live source dereference",
          "[almetal][pipeline-buffer-publication][producer-preparation][PPD-410]")
{
    using namespace Inputs::ALScenePublicationNative;
    Publication f;Inputs::LLDrawInfo info;info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    ALNativeFrameBudget budget;ALNativeFrameBudget::FrameInputs frame;frame.frame=1;budget.beginFrame(frame);
    ALNativeCohortCompilationAdmission admission;U64 serial=0;
    SECTION("an oversized or unknown-byte chunk performs no admission")
    {
        std::array<ProducerWorldRequest,257> too_many{};U32 checks=0;
        auto denied=prepareProducerSources(too_many,{},Inputs::sNativeSceneSources,serial,1,admission,budget,1,
            [&]{++checks;return true;},[](const auto&,auto&){return false;});
        CHECK(denied.status==ProducerPreparationStatus::unsupported);CHECK(checks==0);CHECK(serial==0);
        const std::array one{ProducerWorldRequest{producerSource(info),WorldFlavor{}}};
        denied=prepareProducerSources(one,{},Inputs::sNativeSceneSources,serial,1,admission,budget,
            ALNativeFrameBudget::unknown_bytes,[&]{++checks;return true;},[](const auto&,auto&){return false;});
        CHECK(denied.status==ProducerPreparationStatus::unsupported);CHECK(checks==0);
    }
    SECTION("old membership never dereferences its raw rows")
    {
        const std::array one{ProducerWorldRequest{producerSource(info),WorldFlavor{}}};
        const auto reads=info.publication_reads;
        auto stale=prepareProducerSources(one,{},Inputs::sNativeSceneSources,serial,1,admission,budget,4096,
            []{return false;},[](const auto&,auto&){return false;});
        CHECK(stale.status==ProducerPreparationStatus::stale);CHECK(info.publication_reads==reads);
        CHECK_FALSE(admission.lastConsideredFrame());
    }
    SECTION("same-address identity or source layout replacement is stale")
    {
        const std::array one{ProducerWorldRequest{producerSource(info),WorldFlavor{}}};
        info.state=std::make_shared<ALNativePublication::State>();
        auto stale=prepareProducerSources(one,{},Inputs::sNativeSceneSources,serial,1,admission,budget,4096,
            []{return true;},[](const auto&,auto&){return false;});
        CHECK(stale.status==ProducerPreparationStatus::stale);CHECK(serial==0);
        const std::array new_identity{ProducerWorldRequest{producerSource(info),WorldFlavor{}}};++info.layout_revision;
        stale=prepareProducerSources(new_identity,{},Inputs::sNativeSceneSources,serial,1,admission,budget,4096,
            []{return true;},[](const auto&,auto&){return false;});
        CHECK(stale.status==ProducerPreparationStatus::stale);CHECK_FALSE(info.state->publication());
    }
}

TEST_CASE("A reader-only transient is not a retained producer preparation hit",
          "[almetal][pipeline-buffer-publication][producer-preparation][PPD-410]")
{
    using namespace Inputs::ALScenePublicationNative;
    Publication f;Inputs::LLDrawInfo info;info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    ALNativeSourceWorkingSet tiny(1,1,0);U64 serial=0;
    auto reader=world(info,WorldFlavor{},tiny,serial,1);REQUIRE(reader);CHECK_FALSE(info.state->publication());
    const std::array requests{ProducerWorldRequest{producerSource(info),WorldFlavor{}}};
    CHECK_FALSE(producerWorldReady(requests[0]));
    ALNativeFrameBudget budget;ALNativeFrameBudget::FrameInputs frame;frame.frame=1;budget.beginFrame(frame);
    ALNativeCohortCompilationAdmission admission;
    auto unavailable=prepareProducerSources(requests,{},tiny,serial,1,admission,budget,4096,
        []{return true;},[](const auto&,auto&){return false;});
    CHECK(unavailable.status==ProducerPreparationStatus::incomplete);CHECK_FALSE(info.state->publication());
    CHECK(admission.pending());CHECK(reader.prototype->vertices==info.mVertexBuffer->getMetalVertexBuffer());
    auto required=world(info,WorldFlavor{},tiny,serial,1);REQUIRE(required);CHECK(required.reused);
    CHECK(required.owner==reader.owner);
}

TEST_CASE("Producer shadow builders reject a reentrant source or membership replacement",
          "[almetal][pipeline-buffer-publication][producer-preparation][PPD-410]")
{
    using namespace Inputs::ALScenePublicationNative;
    Publication f;Inputs::LLDrawInfo info;info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    ALNativeFrameBudget budget;ALNativeFrameBudget::FrameInputs frame;frame.frame=1;budget.beginFrame(frame);
    ALNativeCohortCompilationAdmission admission;U64 serial=0;bool owner_current=true;
    const std::array requests{ProducerShadowRequest{producerSource(info),7,ALMetalWorldSpotShadowCoverage::opaque}};
    U32 builds=0;
    auto stale=prepareProducerSources({},requests,Inputs::sNativeSceneSources,serial,1,admission,budget,4096,
        [&]{return owner_current;},[&](const auto&,auto&)
        {++builds;owner_current=false;info.state->invalidate();return true;});
    CHECK(stale.status==ProducerPreparationStatus::stale);CHECK(builds==1);
    CHECK_FALSE(info.state->publication());CHECK(admission.pending());
    CHECK(budget.snapshot().scene_publications_in_flight==0);
}

TEST_CASE("Producer shadow preparation shares the exact world geometry tuple before a view",
          "[almetal][pipeline-buffer-publication][producer-preparation]")
{
    using namespace Inputs::ALScenePublicationNative;
    Publication f;Inputs::LLDrawInfo info;info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    ALNativeFrameBudget budget;ALNativeFrameBudget::FrameInputs frame;frame.frame=1;budget.beginFrame(frame);
    ALNativeCohortCompilationAdmission admission;U64 serial=0;U32 builds=0;
    const std::array worlds{ProducerWorldRequest{producerSource(info),WorldFlavor{}}};
    const std::array shadows{ProducerShadowRequest{producerSource(info),7,ALMetalWorldSpotShadowCoverage::opaque}};
    const auto build=[&](const auto& request,ALMetalWorldSpotShadowBatch& prototype)
    {
        ++builds;auto* buffer=request.source.info->mVertexBuffer.get();
        prototype.vertices=buffer->getMetalVertexBuffer();prototype.indices=buffer->getMetalIndexBuffer();
        prototype.position_offset=buffer->getOffset(LLVertexBuffer::TYPE_VERTEX);
        prototype.vertex_count=buffer->getNumVerts();prototype.index_count=info.mCount;
        prototype.index_offset=std::size_t(info.mOffset)*buffer->getIndexStride();
        prototype.index_type=buffer->getIndexStride()==4?ALMetalIndexType::uint32:ALMetalIndexType::uint16;
        prototype.coverage=request.coverage;return true;
    };
    auto result=prepareProducerSources(worlds,shadows,Inputs::sNativeSceneSources,serial,1,admission,budget,8192,
        []{return true;},build);
    REQUIRE(result);CHECK(result.prepared==2);CHECK(builds==1);
    const auto captured_serial=serial;
    auto visible=world(info,WorldFlavor{},Inputs::sNativeSceneSources,serial,1);REQUIRE(visible);CHECK(visible.reused);
    auto shadowed=shadow(info,7,ALMetalWorldSpotShadowCoverage::opaque,Inputs::sNativeSceneSources,serial,1,
        [&](auto&){++builds;return false;});
    REQUIRE(shadowed);CHECK(shadowed.reused);CHECK(builds==1);CHECK(serial==captured_serial);
    CHECK(visible.owner==shadowed.owner);CHECK((*shadowed.record)->vertexLease()==visible.owner->vertices);
    CHECK((*shadowed.record)->indexLease()==visible.owner->indices);
}
