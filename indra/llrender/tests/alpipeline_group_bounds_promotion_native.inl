/**
 * @file alpipeline_group_bounds_promotion_native.inl
 * @brief Actual warm group capability promotion, independent of qualification.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included after the existing actual producer fixture definitions.
 */
namespace
{
TEST_CASE("Actual warm SIMPLE Legacy and PBR groups acquire bounds once without changing old readers",
          "[almetal][pipeline-buffer-publication][group-bounds-promotion][PPD-276]")
{
    using namespace Inputs::ALSceneGroupPublicationNative;
    for(U32 family:{0u,1u,2u})for(bool qualified:{false,true})
    {
        CAPTURE(family,qualified);MaterialGroupPublication f(family);
        Inputs::gSavedSettings.native_visibility=false;Inputs::gSavedSettings.native_resources=false;
        f.flavor.native_resources=false;
        for(unsigned row=0;row<2;++row)
        {
            auto& info=f.materialDraw();
            if(!qualified)
            {
                // The established direct capacity contract accepts this
                // range, but the exact optional bounds scan cannot qualify
                // index3 in a three-vertex generation. Never render it.
                const U16 indices[]{0,1,3};info.mVertexBuffer->setIndexData(indices);
                info.mVertexBuffer->unmapBuffer();
                REQUIRE(info.mVertexBuffer->validateRange(info.mStart,info.mEnd,info.mCount,info.mOffset));
            }
        }
        const auto members=f.draws();const auto region=f.region.snapshot();
        const auto current=[&](bool bounds)
        {
            if(!family)return Inputs::ALSceneGroupPublicationNative::current(
                f.group,region,Inputs::sNativeSceneSources,Inputs::gFrameCount,bounds);
            auto flavor=f.flavor;flavor.cohort_bounds=bounds;
            return currentMaterial(f.group,region,flavor,Inputs::sNativeSceneSources,Inputs::gFrameCount);
        };
        const auto publish=[&](bool bounds)
        {
            if(!family)return seal(f.group,f.group_dependencies,f.group_layout,f.model,region,members,
                Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount,bounds);
            auto flavor=f.flavor;flavor.cohort_bounds=bounds;
            return sealMaterial(f.group,f.group_dependencies,f.group_layout,f.model,region,members,flavor,
                Inputs::sNativeSceneSources,Inputs::sNativeSceneSerial,Inputs::gFrameCount);
        };
        auto old=publish(false);REQUIRE(old);REQUIRE(old->cohort);REQUIRE(old->cohort->size()==members.size());
        CHECK_FALSE(old->cohort->hasBoundsPublication());CHECK(old->cohort->qualifiedBoundsCount()==0);
        const auto revision=f.group->revision();const auto builds=statistics.builds;
        const auto before_missing=statistics.current_misses[static_cast<std::size_t>(CurrentMiss::bounds_capability)];
        CHECK_FALSE(current(true));CHECK(f.group->publication().get()==old.get());CHECK(f.group->revision()==revision);
        CHECK(statistics.current_misses[static_cast<std::size_t>(CurrentMiss::bounds_capability)]==before_missing+1);
        CHECK(current(false)==old);CHECK(publish(false)==old);CHECK(statistics.builds==builds);

        // Use the real source factory's optional-promotion allocation seam.
        // A truthy old source is a valid raw fallback, not a fulfilled group
        // bounds capability. The old publication must survive this attempt.
        struct RestoreHook
        {
            decltype(Inputs::ALScenePublicationNative::capabilityPromotionAllocationForTesting) previous=
                Inputs::ALScenePublicationNative::capabilityPromotionAllocationForTesting;
            ~RestoreHook(){Inputs::ALScenePublicationNative::capabilityPromotionAllocationForTesting=previous;}
        } restore;
        Inputs::ALScenePublicationNative::capabilityPromotionAllocationForTesting=
            [](ALNativePublication::State&){throw std::bad_alloc();};
        CHECK_FALSE(publish(true));CHECK(f.group->publication().get()==old.get());CHECK(f.group->revision()==revision);
        CHECK_FALSE(old->cohort->hasBoundsPublication());CHECK(statistics.builds==builds);
        Inputs::ALScenePublicationNative::capabilityPromotionAllocationForTesting=nullptr;

        auto upgraded=publish(true);REQUIRE(upgraded);REQUIRE(upgraded->cohort);CHECK(upgraded!=old);
        CHECK(upgraded->cohort->hasBoundsPublication());
        CHECK(upgraded->cohort->qualifiedBoundsCount()==(qualified?members.size():0));
        CHECK(f.group->publication().get()==upgraded.get());CHECK(f.group->revision()==revision);
        CHECK(statistics.builds==builds+1);
        for(std::size_t row=0;row<members.size();++row)
        {
            const auto& before=old->cohort->source(row);const auto& after=upgraded->cohort->source(row);
            CHECK(before.vertices==after.vertices);CHECK(before.indices==after.indices);
            CHECK(old->cohort->key(row).identity==upgraded->cohort->key(row).identity);
            CHECK(old->cohort->key(row).revision==upgraded->cohort->key(row).revision);
            CHECK_FALSE(old->cohort->bounds(row).qualified);
            CHECK(upgraded->cohort->bounds(row).qualified==qualified);
        }
        const auto reads=f.reads();const auto serial=Inputs::sNativeSceneSerial;
        for(unsigned view=0;view<3;++view)
        {CHECK(current(true)==upgraded);CHECK(publish(true)==upgraded);CHECK(current(false)==upgraded);}
        // Unknown is a completed attempted scan too: main/probe warm calls
        // perform no source discovery, rebuild or repeated capability work.
        CHECK(f.reads()==reads);CHECK(Inputs::sNativeSceneSerial==serial);CHECK(statistics.builds==builds+1);
        CHECK_FALSE(old->cohort->hasBoundsPublication());CHECK(old->cohort->qualifiedBoundsCount()==0);
    }
}
}
