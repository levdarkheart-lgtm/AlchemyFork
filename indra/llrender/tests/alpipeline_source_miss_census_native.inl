/**
 * @file alpipeline_source_miss_census_native.inl
 * @brief Actual child-source getter and factory miss-stage attribution.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after the real producer/publication fixture definitions.
 */
TEST_CASE("Actual child getter misses distinguish producer mutation and reader retirement",
          "[almetal][pipeline-buffer-publication][source-miss-census]")
{
    using namespace Inputs::ALScenePublicationNative;
    const auto index=[](CurrentMissReason reason){return static_cast<std::size_t>(reason);};
    Publication f;Inputs::LLDrawInfo info;info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    ALNativeSourceWorkingSet residency(16,8*1024*1024,100);
    auto before=source_miss_statistics;
    CHECK_FALSE(current(info));
    CHECK(source_miss_statistics.current_misses[index(CurrentMissReason::first_encounter)]==before.current_misses[index(CurrentMissReason::first_encounter)]+1);
    CHECK(source_miss_statistics.world_misses==before.world_misses);
    CHECK(source_miss_statistics.shadow_misses==before.shadow_misses);
    WorldFlavor flavor;
    auto reader=world(info,flavor,residency,Inputs::sNativeSceneSerial,1);REQUIRE(reader);
    before=source_miss_statistics;
    const auto walks=info.dependency_walks;
    CHECK(current(info)==reader.owner);CHECK(info.dependency_walks==walks);
    CHECK(source_miss_statistics.current_misses==before.current_misses);
    CHECK(source_miss_statistics.world_misses==before.world_misses);
    const auto revision=info.state->revision();
    REQUIRE(residency.erase(info.state->identity(),reader.owner->serial));
    CHECK_FALSE(info.state->publication());CHECK(info.state->revision()==revision);
    // Withdrawal alone is not a source miss while a genuine immutable reader
    // still owns the same-revision payload. No publisher is resurrected.
    CHECK(current(info)==reader.owner);
    auto reused=world(info,flavor,residency,Inputs::sNativeSceneSerial,2);REQUIRE(reused);
    CHECK(reused.reused);CHECK(reused.owner==reader.owner);CHECK_FALSE(info.state->publication());
    CHECK(source_miss_statistics.current_misses==before.current_misses);
    CHECK(source_miss_statistics.world_misses==before.world_misses);
    std::weak_ptr<const Payload> weak=reader.owner;reader={};reused={};CHECK(weak.expired());
    before=source_miss_statistics;CHECK_FALSE(current(info));
    CHECK(source_miss_statistics.current_misses[index(CurrentMissReason::same_revision_transient_expired)]==before.current_misses[index(CurrentMissReason::same_revision_transient_expired)]+1);
    CHECK(source_miss_statistics.current_misses[index(CurrentMissReason::first_encounter)]==before.current_misses[index(CurrentMissReason::first_encounter)]);
    auto rebuilt=world(info,flavor,residency,Inputs::sNativeSceneSerial,3);REQUIRE(rebuilt);
    CHECK(rebuilt.owner->producer_revision==revision);
    // The actual LLVB write-begin signal invalidates the subscribed producer,
    // even while this old source remains an immutable reader.
    f.positions(*info.mVertexBuffer,true);info.mVertexBuffer->unmapBuffer();
    REQUIRE(info.state->revision()!=revision);
    before=source_miss_statistics;CHECK_FALSE(current(info));
    CHECK(source_miss_statistics.current_misses[index(CurrentMissReason::producer_revision_changed)]==before.current_misses[index(CurrentMissReason::producer_revision_changed)]+1);
    CHECK(source_miss_statistics.current_misses[index(CurrentMissReason::same_revision_transient_expired)]==before.current_misses[index(CurrentMissReason::same_revision_transient_expired)]);
    auto changed=world(info,flavor,residency,Inputs::sNativeSceneSerial,4);REQUIRE(changed);
    CHECK(changed.owner->producer_revision==info.state->revision());CHECK(changed.owner!=rebuilt.owner);
}

TEST_CASE("Actual world misses separate absent payload flavor extension and optional promotion",
          "[almetal][pipeline-buffer-publication][source-miss-census]")
{
    using namespace Inputs::ALScenePublicationNative;
    const auto index=[](FactoryMissStage stage){return static_cast<std::size_t>(stage);};
    Publication f;Inputs::LLDrawInfo info;info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    ALNativeSourceWorkingSet residency(16,8*1024*1024,100);WorldFlavor flavor;
    auto before=source_miss_statistics;
    auto first=world(info,flavor,residency,Inputs::sNativeSceneSerial,1);REQUIRE(first);
    CHECK(source_miss_statistics.world_misses[index(FactoryMissStage::current_payload_missing)]==before.world_misses[index(FactoryMissStage::current_payload_missing)]+1);
    CHECK(source_miss_statistics.world_misses[index(FactoryMissStage::variant_missing)]==before.world_misses[index(FactoryMissStage::variant_missing)]);
    CHECK(source_miss_statistics.world_misses[index(FactoryMissStage::optional_capability_promotion)]==before.world_misses[index(FactoryMissStage::optional_capability_promotion)]);
    const auto revision=info.state->revision();before=source_miss_statistics;
    auto warm=world(info,flavor,residency,Inputs::sNativeSceneSerial,1);REQUIRE(warm);CHECK(warm.reused);
    CHECK(source_miss_statistics.current_misses==before.current_misses);CHECK(source_miss_statistics.world_misses==before.world_misses);
    auto masked=flavor;masked.alpha_mask=true;
    auto variant=world(info,masked,residency,Inputs::sNativeSceneSerial,1);REQUIRE(variant);
    CHECK(info.state->revision()==revision);CHECK(variant.owner!=first.owner);
    CHECK(source_miss_statistics.current_misses==before.current_misses);
    CHECK(source_miss_statistics.world_misses[index(FactoryMissStage::variant_missing)]==before.world_misses[index(FactoryMissStage::variant_missing)]+1);
    CHECK(source_miss_statistics.world_misses[index(FactoryMissStage::current_payload_missing)]==before.world_misses[index(FactoryMissStage::current_payload_missing)]);
    before=source_miss_statistics;
    auto requested=masked;requested.cohort_bounds=true;
    CHECK(requested.key()==masked.key());
    auto promoted=world(info,requested,residency,Inputs::sNativeSceneSerial,1);REQUIRE(promoted);CHECK(promoted.reused);
    CHECK(promoted.owner->bounds_attempted);CHECK(promoted.owner->producer_revision==revision);
    CHECK(promoted.owner->native_tuple==variant.owner->native_tuple);
    CHECK(source_miss_statistics.current_misses==before.current_misses);
    CHECK(source_miss_statistics.world_misses[index(FactoryMissStage::optional_capability_promotion)]==before.world_misses[index(FactoryMissStage::optional_capability_promotion)]+1);
    CHECK(source_miss_statistics.world_misses[index(FactoryMissStage::variant_missing)]==before.world_misses[index(FactoryMissStage::variant_missing)]);
    before=source_miss_statistics;
    auto shared=world(info,requested,residency,Inputs::sNativeSceneSerial,1);REQUIRE(shared);CHECK(shared.owner==promoted.owner);
    CHECK(source_miss_statistics.current_misses==before.current_misses);CHECK(source_miss_statistics.world_misses==before.world_misses);
    // These are factory-entry stages, not successful-build counters: missing
    // geometry can fail after an absent-payload stage without any publication.
    Inputs::LLDrawInfo invalid;before=source_miss_statistics;
    CHECK_FALSE(world(invalid,flavor,residency,Inputs::sNativeSceneSerial,1));
    CHECK_FALSE(invalid.state->publication());
    CHECK(source_miss_statistics.world_misses[index(FactoryMissStage::current_payload_missing)]==before.world_misses[index(FactoryMissStage::current_payload_missing)]+1);
}

TEST_CASE("Actual shadow misses preserve type coverage keys and warm builder suppression",
          "[almetal][pipeline-buffer-publication][source-miss-census]")
{
    using namespace Inputs::ALScenePublicationNative;
    const auto index=[](FactoryMissStage stage){return static_cast<std::size_t>(stage);};
    Publication f;Inputs::LLDrawInfo info;info.mVertexBuffer=f.buffer();info.mTexture.value=&f.image;
    ALNativeSourceWorkingSet residency(16,8*1024*1024,100);
    auto coverage=ALMetalWorldSpotShadowCoverage::opaque;U32 builds=0;
    const auto build=[&](ALMetalWorldSpotShadowBatch& batch)
    {
        ++builds;const auto& buffer=*info.mVertexBuffer;
        batch.vertices=buffer.getMetalVertexBuffer();batch.indices=buffer.getMetalIndexBuffer();
        batch.vertex_count=buffer.getNumVerts();batch.index_count=info.mCount;
        batch.position_offset=buffer.getOffset(LLVertexBuffer::TYPE_VERTEX);
        batch.texcoord_offset=buffer.getOffset(LLVertexBuffer::TYPE_TEXCOORD0);
        batch.color_offset=buffer.getOffset(LLVertexBuffer::TYPE_COLOR);batch.coverage=coverage;
        if(coverage!=ALMetalWorldSpotShadowCoverage::opaque)
        {batch.texture_count=1;batch.coverageData().textures[0]=f.white->getMetalTexture();batch.coverageData().alpha_cutoffs[0]=.5f;}
        return true;
    };
    auto before=source_miss_statistics;
    auto first=shadow(info,7,coverage,residency,Inputs::sNativeSceneSerial,1,build);REQUIRE(first);CHECK(builds==1);
    CHECK(source_miss_statistics.shadow_misses[index(FactoryMissStage::current_payload_missing)]==before.shadow_misses[index(FactoryMissStage::current_payload_missing)]+1);
    const auto revision=info.state->revision();before=source_miss_statistics;
    auto warm=shadow(info,7,coverage,residency,Inputs::sNativeSceneSerial,1,build);REQUIRE(warm);CHECK(warm.reused);CHECK(builds==1);
    CHECK(source_miss_statistics.current_misses==before.current_misses);CHECK(source_miss_statistics.shadow_misses==before.shadow_misses);
    auto different_type=shadow(info,8,coverage,residency,Inputs::sNativeSceneSerial,1,build);REQUIRE(different_type);CHECK(builds==2);
    CHECK(info.state->revision()==revision);
    CHECK(source_miss_statistics.shadow_misses[index(FactoryMissStage::variant_missing)]==before.shadow_misses[index(FactoryMissStage::variant_missing)]+1);
    coverage=ALMetalWorldSpotShadowCoverage::texture_mask;before=source_miss_statistics;
    auto different_coverage=shadow(info,7,coverage,residency,Inputs::sNativeSceneSerial,1,build);REQUIRE(different_coverage);CHECK(builds==3);
    CHECK(info.state->revision()==revision);CHECK(source_miss_statistics.current_misses==before.current_misses);
    CHECK(source_miss_statistics.shadow_misses[index(FactoryMissStage::variant_missing)]==before.shadow_misses[index(FactoryMissStage::variant_missing)]+1);
    CHECK(source_miss_statistics.shadow_misses[index(FactoryMissStage::optional_capability_promotion)]==before.shadow_misses[index(FactoryMissStage::optional_capability_promotion)]);
    before=source_miss_statistics;
    auto reused=shadow(info,7,coverage,residency,Inputs::sNativeSceneSerial,1,build);REQUIRE(reused);CHECK(reused.reused);CHECK(builds==3);
    CHECK(source_miss_statistics.current_misses==before.current_misses);CHECK(source_miss_statistics.shadow_misses==before.shadow_misses);
}
