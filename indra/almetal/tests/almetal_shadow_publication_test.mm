/** SPDX-License-Identifier: LGPL-2.1-or-later */
#include "../almetalshadowpublication.h"
#include "../almetaldevice.h"
#include "../almetalresource.h"
#include "../../newview/pipeline_shadow_membership_native.inl"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <functional>

namespace
{
using Coverage = ALMetalWorldSpotShadowCoverage;
using Source = ALMetalShadowMembershipSource;
using Range = ALMetalShadowMembershipRange;
using Publisher = ALMetalShadowMembershipPublisher;
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device = ALMetalDevice::create(&error);
    std::shared_ptr<const ALMetalBuffer> vertices, indices;
    std::shared_ptr<const ALMetalTexture> texture;
    std::shared_ptr<const ALMetalAttachmentPaletteRecord> pose;
    ALNativeFrameBudget budget;
    ALNativeSourceWorkingSet residency{8192,256ULL*1024*1024,2};
    std::uint64_t serial = 0;
    Fixture()
    {
        REQUIRE(device);
        const std::array<float,12> p{-1,-1,0,1,1,-1,0,1,0,1,0,1};
        const std::array<float,12> w{.5f,0,0,0,.5f,0,0,0,.5f,0,0,0};
        const std::array<float,6> uv{0,0,1,0,.5f,1};
        std::array<std::uint8_t,132> bytes{};
        std::memcpy(bytes.data(),p.data(),48);std::memcpy(bytes.data()+48,w.data(),48);
        std::memcpy(bytes.data()+96,uv.data(),24);std::fill(bytes.begin()+120,bytes.end(),255);
        ALMetalBufferDescriptor b; b.length=bytes.size();
        auto vb=ALMetalBuffer::create(*device,b,&error);REQUIRE(vb);REQUIRE(vb->write(0,bytes.data(),bytes.size(),&error));vertices=std::move(vb);
        const std::array<std::uint16_t,3> ix{0,1,2};b.length=sizeof(ix);
        auto ib=ALMetalBuffer::create(*device,b,&error);REQUIRE(ib);REQUIRE(ib->write(0,ix.data(),sizeof(ix),&error));indices=std::move(ib);
        ALMetalTextureDescriptor td;td.width=td.height=1;td.format=ALMetalPixelFormat::rgba8_unorm;td.usage=ALMetalTextureUsage::shader_read;
        texture=ALMetalTexture::create(*device,td,&error);REQUIRE(texture);
        std::array<float,16> palette{};palette[4]=palette[9]=palette[14]=1;
        pose=ALMetalAttachmentPaletteRecord::create(palette.data(),palette.size(),1,&error);REQUIRE(pose);
        budget.beginFrame({.frame=1});
    }
    Source source(std::uint64_t generation, Coverage coverage=Coverage::opaque, bool rigged=false,
                  std::size_t reserved_texture_slots=0)
    {
        ALMetalWorldSpotShadowBatch batch;batch.vertices=vertices.get();batch.indices=indices.get();
        batch.vertex_count=batch.index_count=3;batch.index_type=ALMetalIndexType::uint16;
        batch.texcoord_offset=96;batch.color_offset=120;batch.coverage=coverage;
        std::copy(alMetalShadowIdentity.begin(),alMetalShadowIdentity.end(),batch.modelview_projection);
        if(coverage!=Coverage::opaque){batch.texture_count=1;batch.coverageData().textures[0]=texture.get();batch.coverageData().alpha_cutoffs[0]=.375f;}
        if(rigged){batch.weight_offset=48;batch.skin_palette_record=pose;batch.skin_palette=pose->data();batch.skin_palette_float_count=pose->size();batch.skin_joint_count=1;}
        std::vector<std::shared_ptr<const ALMetalTexture>> images;images.reserve(reserved_texture_slots);if(batch.texture_count)images.push_back(texture);
        auto record=ALMetalShadowSourceRecord::create(generation,batch,vertices,indices,std::move(images),&error);INFO(error);REQUIRE(record);
        auto state=std::make_shared<ALNativePublication::State>();REQUIRE(state->publish(state->revision(),record));
        return {state,state->identity(),state->revision(),record};
    }
};
std::shared_ptr<ALNativePublication::State> group() {return std::make_shared<ALNativePublication::State>();}
}

TEST_CASE("shadow publication: group keys reuse before factories through ordinary view sequence", "[almetal][shadow-publication]")
{
    @autoreleasepool
    {
        Fixture f;ALMetalShadowMembershipCache cache;
        auto a=group(),b=group();std::array rows{f.source(1),f.source(2)};
        const std::array ranges{Range{17,Coverage::opaque,0,2}};
        auto first=cache.seal(a,17,rows,ranges,f.residency,f.budget,f.serial,1,1);REQUIRE(first);
        CHECK(first.outcome==Publisher::Outcome::retained);CHECK(first.membership->resources().size()==2);
        REQUIRE(cache.seal(b,17,rows,ranges,f.residency,f.budget,f.serial,1,1));
        const auto before=cache.stats();f.budget.beginDrain();
        // Directional0..3, projector0..1, next-frame views: owner identity is
        // group membership, not camera, map index, frame or last whole-view list.
        for(unsigned view=0;view<14;++view){auto hit=cache.current(view%2?a:b,17,f.residency,1+view/7);REQUIRE(hit);CHECK(hit.outcome==Publisher::Outcome::reused);}
        CHECK(cache.stats().resource_unions==before.resource_unions);
        CHECK(cache.stats().hits==before.hits+14);CHECK(f.serial==2);
        auto same=cache.current(a,17,f.residency,2);REQUIRE(same);CHECK(same.membership==first.membership);
        CHECK(first.membership->members()[0].record==rows[0].record);
    }
}

TEST_CASE("shadow publication: exact membership order and source writes invalidate independently", "[almetal][shadow-publication]")
{
    @autoreleasepool
    {
        Fixture f;ALMetalShadowMembershipCache cache;auto owner=group();
        std::array rows{f.source(1),f.source(2)};const std::array ranges{Range{4,Coverage::opaque,0,2}};
        auto old=cache.seal(owner,4,rows,ranges,f.residency,f.budget,f.serial,1,1);REQUIRE(old);
        rows[0].producer->evict(); // Source lookup withdrawal is not mutation.
        REQUIRE(cache.current(owner,4,f.residency,1));
        owner->invalidate();CHECK_FALSE(cache.current(owner,4,f.residency,1));
        std::swap(rows[0],rows[1]);auto changed=cache.seal(owner,4,rows,ranges,f.residency,f.budget,f.serial,1,1);REQUIRE(changed);
        CHECK(changed.membership!=old.membership);CHECK(changed.membership->members()[0].record==rows[0].record);
        // Immutable command reader keeps the old exact order and native leases.
        CHECK(old.membership->members()[0].record==rows[1].record);
        rows[0].producer->invalidate();CHECK_FALSE(cache.current(owner,4,f.residency,1));
        auto stale=cache.seal(owner,4,rows,ranges,f.residency,f.budget,f.serial,1,1);CHECK_FALSE(stale);CHECK(stale.failure==Publisher::Failure::invalid_source);
        CHECK(old.membership->members()[0].record->vertexLease()==f.vertices);
    }
}

TEST_CASE("shadow publication: limits denial and native eviction preserve transient readers", "[almetal][shadow-publication]")
{
    @autoreleasepool
    {
        Fixture f;ALMetalShadowMembershipCache::Limits limits;limits.maximum_entries=2;
        ALMetalShadowMembershipCache cache(limits);std::array rows{f.source(1)};const std::array ranges{Range{3,Coverage::opaque,0,1}};
        auto a=group(),b=group(),c=group();auto reader=cache.seal(a,3,rows,ranges,f.residency,f.budget,f.serial,1,1);REQUIRE(reader);
        REQUIRE(cache.seal(b,3,rows,ranges,f.residency,f.budget,f.serial,1,1));REQUIRE(cache.current(b,3,f.residency,1));
        REQUIRE(cache.seal(c,3,rows,ranges,f.residency,f.budget,f.serial,1,1));CHECK(cache.stats().entries==2);CHECK_FALSE(cache.current(a,3,f.residency,1));
        CHECK(reader.membership->members()[0].record==rows[0].record);
        f.budget.beginDrain();auto denied=cache.seal(group(),3,rows,ranges,f.residency,f.budget,f.serial,1,1);CHECK_FALSE(denied);CHECK(denied.failure==Publisher::Failure::admission);
        ALNativeFrameBudget fresh;fresh.beginFrame({.frame=2});ALNativeSourceWorkingSet noNative{0,0,0};ALMetalShadowMembershipCache transient;
        auto fallback=transient.seal(a,3,rows,ranges,noNative,fresh,f.serial,2,2);REQUIRE(fallback);CHECK(fallback.outcome==Publisher::Outcome::transient);CHECK_FALSE(transient.current(a,3,noNative,2));
        CHECK(fallback.membership->resources().size()==2);
        limits.maximum_entries=1;limits.maximum_metadata_bytes=1;ALMetalShadowMembershipCache tiny(limits);
        auto bounded=tiny.seal(a,3,rows,ranges,f.residency,fresh,f.serial,2,2);CHECK_FALSE(bounded);CHECK(bounded.failure==Publisher::Failure::bounds);CHECK(tiny.stats().entries==0);
    }
}

TEST_CASE("shadow publication: range and source generation contracts reject stale views", "[almetal][shadow-publication]")
{
    @autoreleasepool
    {
        Fixture f;ALMetalShadowMembershipCache cache;auto owner=group();std::array rows{f.source(1),f.source(2,Coverage::legacy_mask,true)};
        const std::array ranges{Range{1,Coverage::opaque,0,1},Range{2,Coverage::legacy_mask,1,1}};
        auto sealed=cache.seal(owner,99,rows,ranges,f.residency,f.budget,f.serial,1,1);REQUIRE(sealed);
        auto bad=ranges;bad[1].first=0;CHECK_FALSE(cache.seal(group(),99,rows,bad,f.residency,f.budget,f.serial,1,1));
        bad=ranges;bad[1].coverage=Coverage::pbr_mask;CHECK_FALSE(cache.seal(group(),99,rows,bad,f.residency,f.budget,f.serial,1,1));
        ALMetalShadowPrepareContext context;context.view[12]=.125f;context.projection[5]=.5f;
        std::array<ALMetalShadowMembershipViewInput,1> facts{};facts[0].source_generation=2;facts[0].palette=f.pose.get();facts[0].has_model=true;facts[0].model[13]=.25f;facts[0].has_texture_matrix=true;facts[0].texture_matrix[12]=.375f;
        std::array<ALMetalShadowPrepareInstance,1> scratch{},reference{};std::array<std::array<float,16>,1> textures{};std::array<ALMetalWorldSpotShadowBatch,1> output{},expected{};
        reference[0].source=rows[1].record.get();reference[0].source_generation=2;reference[0].epoch=7;reference[0].palette=f.pose.get();reference[0].model=facts[0].model;reference[0].has_model=true;reference[0].texture_matrix_slot=0;
        const std::array<std::array<float,16>,1> refTextures{facts[0].texture_matrix};auto refContext=context;refContext.texture_matrices=refTextures;
        REQUIRE(alMetalPrepareShadowBatches(reference,refContext,expected,f.budget,7,nullptr,false));
        REQUIRE(alMetalPrepareShadowMembershipRange(*sealed.membership,1,facts,context,scratch,textures,output,f.budget,7));
        CHECK(std::memcmp(output[0].modelview_projection,expected[0].modelview_projection,64)==0);
        CHECK(std::memcmp(output[0].texture_matrix,expected[0].texture_matrix,64)==0);CHECK(output[0].skin_palette==f.pose->data());CHECK(output[0].vertices==f.vertices.get());
        facts[0].source_generation=1;CHECK_FALSE(alMetalPrepareShadowMembershipRange(*sealed.membership,1,facts,context,scratch,textures,output,f.budget,7));CHECK_FALSE(output[0].vertices);
        facts[0].source_generation=2;std::atomic<std::uint64_t> cancelled{8};CHECK_FALSE(alMetalPrepareShadowMembershipRange(*sealed.membership,1,facts,context,scratch,textures,output,f.budget,7,&cancelled));CHECK_FALSE(output[0].vertices);
        facts[0].source_generation=1;facts[0].palette=nullptr;CHECK_FALSE(alMetalPrepareShadowMembershipRange(*sealed.membership,0,facts,context,scratch,textures,output,f.budget,7));CHECK_FALSE(output[0].vertices);
    }
}

TEST_CASE("shadow publication: retain callbacks cannot publish stale source membership", "[almetal][shadow-publication]")
{
    @autoreleasepool
    {
        Fixture f;ALNativeSourceWorkingSet one{1,1024,2};auto victim=group();auto allocation=std::make_shared<int>(7);REQUIRE(victim->publish(victim->revision(),allocation));
        const std::array<ALNativeSourceWorkingSet::Resource,1> resources{{{allocation,4}}};REQUIRE(one.retain(victim,1,allocation,resources,1));
        std::array rows{f.source(1)};
        struct Mutate final:ALNativePublication::Observer{std::shared_ptr<ALNativePublication::State> source;unsigned callbacks=0;explicit Mutate(std::shared_ptr<ALNativePublication::State> s):source(std::move(s)){}void nativePublicationChanged(ALNativePublication::Phase p)noexcept override{if(p==ALNativePublication::Phase::withdrawn){++callbacks;source->invalidate();}}};
        auto mutation=std::make_shared<Mutate>(rows[0].producer);victim->observe(mutation);
        ALMetalShadowMembershipCache cache;auto owner=group();const std::array ranges{Range{0,Coverage::opaque,0,1}};
        // PPD372: same-frame victims are intentionally protected. Prove the
        // callback did NOT run, instead of mislabelling that path as reentrant.
        auto protected_result=cache.seal(owner,0,rows,ranges,one,f.budget,f.serial,1,1);
        REQUIRE(protected_result);CHECK(protected_result.outcome==Publisher::Outcome::transient);
        CHECK(mutation->callbacks==0);CHECK(one.stats().capacity_rejections[1]==1);
        f.budget.beginFrame({.frame=2});
        auto result=cache.seal(owner,0,rows,ranges,one,f.budget,f.serial,2,1);
        CHECK(mutation->callbacks==1);
        CHECK_FALSE(result);CHECK(result.failure==Publisher::Failure::changed_source);CHECK_FALSE(cache.current(owner,0,one,1));CHECK(rows[0].record->vertexLease()==f.vertices);
    }
}

TEST_CASE("shadow publication: owned capacity charges once and native completion sheds CPU records", "[almetal][shadow-publication]")
{
    @autoreleasepool
    {
        Fixture f;auto plain=f.source(1);auto reserved=f.source(2,Coverage::opaque,false,4096);
        CHECK(reserved.record->textureLeases().empty());
        CHECK(reserved.record->ownedBytes()>=plain.record->ownedBytes()+4096*sizeof(std::shared_ptr<const ALMetalTexture>));
        auto covered=f.source(3,Coverage::legacy_mask,false,2048);
        CHECK(covered.record->ownedBytes()>=sizeof(ALMetalShadowSourceRecord)+2048*sizeof(std::shared_ptr<const ALMetalTexture>)+sizeof(ALMetalWorldSpotShadowBatch::CoverageData));
        ALMetalShadowMembershipCache cache;auto owner=group();const std::array single{covered};const std::array duplicate{covered,covered};
        const std::array one{Range{7,Coverage::legacy_mask,0,1}},two{Range{7,Coverage::legacy_mask,0,2}};
        auto a=cache.seal(owner,1,single,one,f.residency,f.budget,f.serial,1,1);REQUIRE(a);
        auto b=cache.seal(owner,2,duplicate,two,f.residency,f.budget,f.serial,1,1);REQUIRE(b);
        CHECK(b.membership->metadataBytes()>a.membership->metadataBytes());
        CHECK(b.membership->metadataBytes()-a.membership->metadataBytes()<covered.record->ownedBytes());
        ALMetalShadowMembershipCache::Limits limits;limits.publication.maximum_metadata_bytes=65536;
        ALMetalShadowMembershipCache small(limits);const std::array excessive{reserved};const std::array opaque{Range{1,Coverage::opaque,0,1}};
        auto rejected=small.seal(group(),1,excessive,opaque,f.residency,f.budget,f.serial,1,1);CHECK_FALSE(rejected);CHECK(rejected.failure==Publisher::Failure::bounds);

        // An independently retained native-only completion owner does not pin
        // the large CPU membership/Record/coverage once CPU encoding is done.
        Fixture lateFixture;ALMetalShadowMembershipCache lateCache;
        auto late=lateFixture.source(4,Coverage::legacy_mask);std::array lastRows{late};
        auto result=lateCache.seal(owner,3,lastRows,one,lateFixture.residency,lateFixture.budget,lateFixture.serial,1,1);REQUIRE(result);
        std::weak_ptr<const ALMetalShadowSourceRecord> record=late.record;
        std::weak_ptr<const ALMetalBuffer> vertices=lateFixture.vertices;std::weak_ptr<const ALMetalTexture> texture=lateFixture.texture;
        auto completion=result.membership->nativeOwner();REQUIRE(completion);
        const auto nativeBytes=result.membership->nativeMetadataBytes();const auto accounting=lateCache.accounting();
        late.producer->evict();late={};lastRows={};lateCache.clear();result.membership.reset();
        lateFixture.vertices.reset();lateFixture.indices.reset();lateFixture.texture.reset();
        CHECK(record.expired());
        CHECK(nativeBytes>0);CHECK(accounting->bytes()==nativeBytes);
        CHECK_FALSE(vertices.expired());CHECK_FALSE(texture.expired());completion.reset();
        CHECK(accounting->bytes()==0);
        CHECK(vertices.expired());CHECK(texture.expired());
    }
}

TEST_CASE("shadow publication: global CPU debt follows last reader beyond cache destruction", "[almetal][shadow-publication]")
{
    @autoreleasepool
    {
        Fixture f;ALMetalShadowMembershipCache::Limits limits;limits.maximum_metadata_bytes=100000;
        auto domain=std::make_shared<ALMetalShadowMembershipAccounting>(limits.maximum_metadata_bytes);
        std::array rows{f.source(1,Coverage::opaque,false,4096)};const std::array ranges{Range{3,Coverage::opaque,0,1}};
        std::shared_ptr<const ALMetalShadowMembership> cpuReader;std::shared_ptr<const void> nativeReader;
        std::uint64_t heldBytes=0,nativeBytes=0;
        {
            ALMetalShadowMembershipCache cache(limits,domain);auto result=cache.seal(group(),3,rows,ranges,f.residency,f.budget,f.serial,1,1);REQUIRE(result);
            cpuReader=result.membership;nativeReader=cpuReader->nativeOwner();heldBytes=cpuReader->metadataBytes();nativeBytes=cpuReader->nativeMetadataBytes();
            CHECK(domain->bytes()>heldBytes);cache.clear();CHECK(domain->bytes()==heldBytes);
        }
        // No Entry, Publisher, group State or original cache remains. The
        // outstanding CPU view still charges the SAME scene accounting domain.
        CHECK(domain->bytes()==heldBytes);
        ALMetalShadowMembershipCache next(limits,domain);auto owner=group();
        auto denied=next.seal(owner,3,rows,ranges,f.residency,f.budget,f.serial,1,1);CHECK_FALSE(denied);CHECK(denied.failure==Publisher::Failure::cpu_residency);
        CHECK(domain->bytes()>=heldBytes);cpuReader.reset();CHECK(domain->bytes()<heldBytes);
        auto recovered=next.seal(owner,3,rows,ranges,f.residency,f.budget,f.serial,1,1);REQUIRE(recovered);
        next.clear();recovered.membership.reset();CHECK(domain->bytes()==nativeBytes);
        REQUIRE(nativeReader); // Big CPU rows retire, small native container remains.
        CHECK(nativeBytes>0);nativeReader.reset();CHECK(domain->bytes()==0);
    }
}

TEST_CASE("shadow publication: admitted cull spans cannot borrow newer group membership", "[almetal][shadow-publication]")
{
    @autoreleasepool
    {
        Fixture f;ALMetalShadowMembershipCache cache;auto owner=group();std::array rows{f.source(1)};
        namespace Bridge=ALSceneShadowMembershipNative;
        const auto admitted=Bridge::admit(owner,17,Coverage::opaque,9,1,true);
        REQUIRE(Bridge::seal(cache,admitted,rows,f.residency,f.budget,f.serial,1,1));
        REQUIRE(Bridge::current(cache,admitted,f.residency,1));
        auto subset=admitted;subset.whole_group=false;
        CHECK_FALSE(Bridge::current(cache,subset,f.residency,1));CHECK_FALSE(Bridge::seal(cache,subset,rows,f.residency,f.budget,f.serial,1,1));
        owner->invalidate();const auto newer=Bridge::admit(owner,17,Coverage::opaque,9,1,true);
        REQUIRE(Bridge::seal(cache,newer,rows,f.residency,f.budget,f.serial,1,1));
        const auto before=cache.stats().resource_unions;
        // Even when a newer valid group publication exists, an old cull's
        // admitted rows cannot silently adopt that newer membership.
        CHECK_FALSE(Bridge::current(cache,admitted,f.residency,1));
        CHECK_FALSE(Bridge::seal(cache,admitted,rows,f.residency,f.budget,f.serial,1,1));
        CHECK(cache.stats().resource_unions==before);
    }
}

TEST_CASE("shadow publication: nested newer membership survives stale outer seal cleanup", "[almetal][shadow-publication][shadow-publication-reentrant]")
{
    @autoreleasepool
    {
        for(bool replace_entry:{false,true})
        {
            CAPTURE(replace_entry);
            Fixture f;f.budget.beginFrame({.frame=2});ALNativeSourceWorkingSet one{1,1024,2};
            auto victim=group();auto allocation=std::make_shared<int>(7);REQUIRE(victim->publish(victim->revision(),allocation));
            const std::array<ALNativeSourceWorkingSet::Resource,1> resources{{{allocation,4}}};REQUIRE(one.retain(victim,1,allocation,resources,1));
            ALMetalShadowMembershipCache cache;auto owner=group();std::array outer_rows{f.source(1)},inner_rows{f.source(2)};
            const std::array ranges{Range{0,Coverage::opaque,0,1}};Publisher::Result inner;
            struct Reenter final:ALNativePublication::Observer
            {
                std::function<void()> action;unsigned callbacks=0;bool threw=false;
                void nativePublicationChanged(ALNativePublication::Phase p)noexcept override
                {if(p==ALNativePublication::Phase::withdrawn){++callbacks;try{action();}catch(...){threw=true;}}}
            };
            auto reenter=std::make_shared<Reenter>();reenter->action=[&]{
                if(replace_entry)cache.clear();else owner->invalidate();
                inner=cache.seal(owner,0,inner_rows,ranges,one,f.budget,f.serial,2,1);
                // An old in-progress child observer can still be alive in
                // the outer transaction after inner membership has changed.
                outer_rows[0].producer->invalidate();
            };victim->observe(reenter);
            auto outer=cache.seal(owner,0,outer_rows,ranges,one,f.budget,f.serial,2,1);
            auto current=cache.current(owner,0,one,2);const auto debt=cache.accounting()->bytes();
            std::printf("shadow_reentrant_diagnostic replace_entry=%u callbacks=%u threw=%u outer=%u inner=%u current=%u debt=%llu inner_metadata=%llu inner_native_metadata=%llu successful_resource_unions=%llu\n",unsigned(replace_entry),reenter->callbacks,unsigned(reenter->threw),unsigned(bool(outer)),unsigned(bool(inner)),unsigned(bool(current)),static_cast<unsigned long long>(debt),static_cast<unsigned long long>(inner?inner.membership->metadataBytes():0),static_cast<unsigned long long>(inner?inner.membership->nativeMetadataBytes():0),static_cast<unsigned long long>(cache.stats().resource_unions));std::fflush(stdout);
            CHECK(reenter->callbacks==1);CHECK_FALSE(reenter->threw);CHECK_FALSE(outer);CHECK(outer.failure==Publisher::Failure::changed_source);
            REQUIRE(inner);REQUIRE(current);CHECK(current.membership==inner.membership);
            CHECK(current.membership->members()[0].record==inner_rows[0].record);CHECK(current.membership->resources().size()==2);
            CHECK(debt>=inner.membership->metadataBytes());CHECK(debt<=cache.accounting()->maximumBytes());
            CHECK(cache.stats().resource_unions==1);
            // The old in-progress A callback ran after inner B committed. It
            // must not mutate B's witness even when B later loses native cache.
            one.clear();
            CHECK_FALSE(cache.current(owner,0,one,3,3).defer_preparation);
            inner_rows[0].producer->invalidate();
            CHECK(cache.current(owner,0,one,3,3).defer_preparation);
        }
    }
}

TEST_CASE("shadow publication: removed child cannot invalidate replacement membership", "[almetal][shadow-publication][shadow-publication-dependencies]")
{
    @autoreleasepool
    {
        Fixture f;ALMetalShadowMembershipCache cache;auto owner=group();
        std::array a{f.source(1)},b{f.source(2)};const std::array ranges{Range{3,Coverage::opaque,0,1}};
        auto old=cache.seal(owner,3,a,ranges,f.residency,f.budget,f.serial,1,1);REQUIRE(old);
        owner->invalidate();auto replacement=cache.seal(owner,3,b,ranges,f.residency,f.budget,f.serial,1,1);REQUIRE(replacement);
        REQUIRE(cache.current(owner,3,f.residency,1));a[0].producer->invalidate();
        auto after_removed=cache.current(owner,3,f.residency,1);
        std::printf("shadow_dependency_diagnostic removed_child_current=%u current_record_generation=%llu debt=%llu\n",unsigned(bool(after_removed)),static_cast<unsigned long long>(after_removed?after_removed.membership->members()[0].record->generation():0),static_cast<unsigned long long>(cache.accounting()->bytes()));std::fflush(stdout);
        REQUIRE(after_removed);CHECK(after_removed.membership==replacement.membership);
        CHECK(after_removed.membership->members()[0].record==b[0].record);
        b[0].producer->invalidate();CHECK_FALSE(cache.current(owner,3,f.residency,1));
        // Already captured immutable readers still own both exact generations.
        CHECK(old.membership->members()[0].record==a[0].record);CHECK(replacement.membership->members()[0].record==b[0].record);
    }
}

TEST_CASE("shadow publication: group gather preserves one ordered family join and global UV slots", "[almetal][shadow-publication]")
{
    @autoreleasepool
    {
        Fixture f;ALMetalShadowMembershipCache cache;const std::array a{f.source(1)},b{f.source(2,Coverage::legacy_mask)};
        const std::array ar{Range{1,Coverage::opaque,0,1}},br{Range{2,Coverage::legacy_mask,0,1}};
        auto first=cache.seal(group(),1,a,ar,f.residency,f.budget,f.serial,1,1);auto second=cache.seal(group(),2,b,br,f.residency,f.budget,f.serial,1,1);REQUIRE(first);REQUIRE(second);
        std::array<ALMetalShadowMembershipViewInput,3> facts{};for(unsigned i=0;i<3;++i){facts[i].source_generation=i?2:1;facts[i].has_model=true;facts[i].model[12]=.125f*float(i+1);facts[i].has_texture_matrix=i!=0;facts[i].texture_matrix[12]=.25f*float(i);}
        std::array<ALMetalShadowPrepareInstance,3> inputs{};std::array<std::array<float,16>,3> textures{};std::array<ALMetalWorldSpotShadowBatch,3> output{};
        std::size_t written=99;
        REQUIRE(alMetalGatherShadowMembershipRange(*first.membership,0,std::span(facts).first(1),std::span(inputs).first(1),{},0,7,written,&f.error));CHECK(written==0);
        REQUIRE(alMetalGatherShadowMembershipRange(*second.membership,0,std::span(facts).subspan(1,1),std::span(inputs).subspan(1,1),std::span(textures).subspan(1,1),1,7,written,&f.error));CHECK(written==1);CHECK(inputs[1].texture_matrix_slot==1);
        REQUIRE(alMetalGatherShadowMembershipRange(*second.membership,0,std::span(facts).subspan(2,1),std::span(inputs).subspan(2,1),std::span(textures).subspan(2,1),2,7,written,&f.error));CHECK(written==1);CHECK(inputs[2].texture_matrix_slot==2);
        ALMetalShadowPrepareContext context;context.texture_matrices=textures;
        REQUIRE(alMetalPrepareShadowBatches(inputs,context,output,f.budget,7,nullptr,false));
        for(unsigned i=0;i<3;++i){CHECK(output[i].vertices==f.vertices.get());CHECK(std::memcmp(output[i].modelview_projection,facts[i].model.data(),64)==0);if(i)CHECK(std::memcmp(output[i].texture_matrix,facts[i].texture_matrix.data(),64)==0);}
        CHECK_FALSE(alMetalGatherShadowMembershipRange(*second.membership,0,std::span(facts).subspan(1,1),std::span(inputs).subspan(1,1),std::span(textures).first(1),std::numeric_limits<std::uint32_t>::max(),7,written,&f.error));CHECK_FALSE(inputs[1].source);CHECK(written==0);
    }
}

TEST_CASE("shadow publication: pending age survives busy denial and revision churn then resets", "[almetal][shadow-publication]")
{
    @autoreleasepool
    {
        Fixture f;ALNativeFrameBudget::Limits limits;limits.maximum_deferral_frames=3;
        ALNativeFrameBudget busy(limits);ALMetalShadowMembershipCache cache;auto owner=group();
        const std::array rows{f.source(1)};const std::array ranges{Range{1,Coverage::opaque,0,1}};
        Publisher::Result completed;
        for(std::uint64_t frame=0;frame<=3;++frame)
        {
            ALNativeFrameBudget::FrameInputs input;input.frame=frame;input.frame_interval_ns=100;input.required_main_cpu_ns=90;busy.beginFrame(input);
            if(frame)owner->invalidate(); // New revisions must not restart pending age.
            auto result=cache.seal(owner,1,rows,ranges,f.residency,busy,f.serial,std::uint32_t(frame),frame);
            if(frame<3){CHECK_FALSE(result);CHECK(result.failure==Publisher::Failure::admission);}
            else{REQUIRE(result);CHECK(result.outcome==Publisher::Outcome::retained);completed=std::move(result);}
        }
        CHECK(busy.snapshot().progress_admissions==1);
        // A successful exact retained publication retires its pending age.
        // A later dirty revision starts fresh, even under unchanged pressure.
        owner->invalidate();ALNativeFrameBudget::FrameInputs next;next.frame=4;next.frame_interval_ns=100;next.required_main_cpu_ns=90;busy.beginFrame(next);
        auto fresh=cache.seal(owner,1,rows,ranges,f.residency,busy,f.serial,4,4);
        CHECK_FALSE(fresh);CHECK(fresh.failure==Publisher::Failure::admission);CHECK(busy.snapshot().progress_admissions==1);
        REQUIRE(completed.membership);CHECK(completed.membership->members()[0].record==rows[0].record);
    }
}

TEST_CASE("shadow publication: source witness survives empty cache and denied attempts", "[almetal][shadow-publication][shadow-publication-source-stability]")
{
    @autoreleasepool
    {
        Fixture f;ALNativeFrameBudget budget;ALMetalShadowMembershipCache cache;auto owner=group();
        budget.beginFrame({.frame=0});std::array rows{f.source(1)};
        const std::array ranges{Range{3,Coverage::opaque,0,1}};
        auto initial=cache.seal(owner,3,rows,ranges,f.residency,budget,f.serial,0,0);REQUIRE(initial);
        auto ready=cache.current(owner,3,f.residency,0,0);REQUIRE(ready);CHECK_FALSE(ready.defer_preparation);
        f.residency.clear();ready={};initial={};
        auto same_frame=cache.current(owner,3,f.residency,0,0);CHECK_FALSE(same_frame);CHECK(same_frame.defer_preparation);
        // Frame0 is a real attempt, not an unknown sentinel. Unopted current
        // retains its old validity-only contract and does not reset the watch.
        CHECK_FALSE(cache.current(owner,3,f.residency,0).defer_preparation);
        CHECK_FALSE(cache.current(owner,3,f.residency,1,1).defer_preparation);
        const auto mutate=[&](std::uint64_t generation){
            rows[0].producer->invalidate();auto fresh=f.source(generation);
            rows[0].record=std::move(fresh.record);rows[0].revision=rows[0].producer->revision();
            REQUIRE(rows[0].producer->publish(rows[0].revision,rows[0].record));
        };
        mutate(2);CHECK(cache.current(owner,3,f.residency,1,1).defer_preparation);
        mutate(3);CHECK(cache.current(owner,3,f.residency,2,2).defer_preparation);
        CHECK_FALSE(cache.current(owner,3,f.residency,3,3).defer_preparation);
        ALNativeFrameBudget::FrameInputs pressure;pressure.frame=3;pressure.frame_interval_ns=100;pressure.required_main_cpu_ns=90;budget.beginFrame(pressure);
        auto denied=cache.seal(owner,3,rows,ranges,f.residency,budget,f.serial,3,0);
        CHECK_FALSE(denied);CHECK(denied.failure==Publisher::Failure::admission);
        CHECK(cache.current(owner,3,f.residency,3,3).defer_preparation);
        // The failed attempt advanced derived validity, but must keep watching
        // real changes. Otherwise this second write would appear stable.
        mutate(4);CHECK(cache.current(owner,3,f.residency,4,4).defer_preparation);
        mutate(5);CHECK(cache.current(owner,3,f.residency,5,5).defer_preparation);
        CHECK(cache.stats().resource_unions==1);
        CHECK_FALSE(cache.current(owner,3,f.residency,6,6).defer_preparation);
        budget.beginFrame({.frame=6});
        auto progressed=cache.seal(owner,3,rows,ranges,f.residency,budget,f.serial,6,0);REQUIRE(progressed);
        auto current=cache.current(owner,3,f.residency,6,6);REQUIRE(current);CHECK_FALSE(current.defer_preparation);
        CHECK(current.membership==progressed.membership);CHECK(cache.stats().resource_unions==2);
    }
}

TEST_CASE("shadow publication: watcher debt survives rows and retires with cache owner", "[almetal][shadow-publication][shadow-publication-source-stability]")
{
    @autoreleasepool
    {
        Fixture f;ALMetalShadowMembershipCache::Limits limits;limits.maximum_metadata_bytes=1024*1024;
        ALMetalShadowMembershipCache cache(limits);auto owner=group();auto source=f.source(1);
        std::vector<Source> rows(1024,source);const std::array ranges{Range{3,Coverage::opaque,0,rows.size()}};
        std::vector<std::shared_ptr<ALNativePublication::State>> owners;
        unsigned retained=0;Publisher::Result denied;
        // Conservatively charged source subscriptions remain after all large
        // membership/native readers retire; they cannot manufacture free debt.
        for(unsigned attempt=0;attempt<16;++attempt)
        {
            auto next=group();owners.push_back(next);f.budget.beginFrame({.frame=attempt+1});
            auto result=cache.seal(next,3,rows,ranges,f.residency,f.budget,f.serial,attempt+1,0);
            if(!result){denied=std::move(result);break;}
            ++retained;f.residency.clear();result={};
            CHECK(cache.accounting()->bytes()>0);CHECK(cache.accounting()->bytes()<=limits.maximum_metadata_bytes);
        }
        CHECK(retained>0);CHECK(retained<16);CHECK_FALSE(denied);CHECK(denied.failure==Publisher::Failure::cpu_residency);
        const auto held=cache.accounting()->bytes();CHECK(held>sizeof(Publisher));
        // The watch owns neither native SourceRecord nor source State.
        std::weak_ptr<const ALMetalShadowSourceRecord> record=source.record;
        std::weak_ptr<ALNativePublication::State> producer=source.producer;
        source.producer->evict();source={};rows.clear();
        CHECK(record.expired());CHECK(producer.expired());CHECK(cache.accounting()->bytes()==held);
        cache.clear();CHECK(cache.accounting()->bytes()==0);
        // The same domain can now admit ordinary required-current inputs.
        auto fresh=f.source(2);const std::array new_rows{fresh};const std::array one{Range{3,Coverage::opaque,0,1}};
        f.budget.beginFrame({.frame=20});auto recovered=cache.seal(owner,3,new_rows,one,f.residency,f.budget,f.serial,20,20);REQUIRE(recovered);
        auto native=recovered.membership->nativeOwner();const auto native_bytes=recovered.membership->nativeMetadataBytes();
        f.residency.clear();recovered={};CHECK(cache.accounting()->bytes()>native_bytes);
        cache.clear();CHECK(cache.accounting()->bytes()==native_bytes);native.reset();CHECK(cache.accounting()->bytes()==0);
    }
}

TEST_CASE("shadow publication: transient attempts throttle opted preparation and keep child witness", "[almetal][shadow-publication][shadow-publication-source-stability]")
{
    @autoreleasepool
    {
        Fixture f;ALNativeSourceWorkingSet none{0,0,0};ALMetalShadowMembershipCache cache;auto owner=group();
        std::array rows{f.source(1)};const std::array ranges{Range{3,Coverage::opaque,0,1}};
        auto result=cache.seal(owner,3,rows,ranges,none,f.budget,f.serial,1,1);REQUIRE(result);CHECK(result.outcome==Publisher::Outcome::transient);
        CHECK(cache.current(owner,3,none,1,1).defer_preparation);
        CHECK_FALSE(cache.current(owner,3,none,2,2).defer_preparation);
        rows[0].producer->invalidate();CHECK(cache.current(owner,3,none,2,2).defer_preparation);
        CHECK_FALSE(cache.current(owner,3,none,3,3).defer_preparation);
        CHECK(result.membership->members()[0].record==rows[0].record);
    }
}

TEST_CASE("shadow publication: first denied frame0 opts into bounded retry without unwatched proof", "[almetal][shadow-publication][shadow-publication-source-stability]")
{
    @autoreleasepool
    {
        Fixture f;ALNativeFrameBudget budget;ALMetalShadowMembershipCache cache;auto owner=group();
        std::array rows{f.source(1)};const std::array ranges{Range{3,Coverage::opaque,0,1}};
        ALNativeFrameBudget::FrameInputs busy;busy.frame=0;busy.frame_interval_ns=100;busy.required_main_cpu_ns=90;budget.beginFrame(busy);
        auto denied=cache.seal(owner,3,rows,ranges,f.residency,budget,f.serial,0,0);
        CHECK_FALSE(denied);CHECK(denied.failure==Publisher::Failure::admission);CHECK(cache.stats().resource_unions==0);
        for(unsigned view=0;view<7;++view)CHECK(cache.current(owner,3,f.residency,0,0).defer_preparation);
        CHECK_FALSE(cache.current(owner,3,f.residency,0).defer_preparation);
        // With no successful source set yet, a later frame permits one retry;
        // it does not invent a subscription or claim source stability proof.
        CHECK_FALSE(cache.current(owner,3,f.residency,1,1).defer_preparation);
        budget.beginFrame({.frame=1});auto ready=cache.seal(owner,3,rows,ranges,f.residency,budget,f.serial,1,0);REQUIRE(ready);
        auto hit=cache.current(owner,3,f.residency,1,1);REQUIRE(hit);CHECK_FALSE(hit.defer_preparation);
        CHECK(cache.stats().resource_unions==1);
    }
}

TEST_CASE("shadow publication: opted new-key hard-cap preflight never allocates an entry", "[almetal][shadow-publication][shadow-publication-source-stability]")
{
    @autoreleasepool
    {
        Fixture f;ALMetalShadowMembershipCache::Limits limits;
        auto domain=std::make_shared<ALMetalShadowMembershipAccounting>(1);
        ALMetalShadowMembershipCache cache(limits,domain);auto owner=group();const auto before=f.budget.snapshot();
        CHECK_FALSE(cache.current(owner,3,f.residency,1).defer_preparation);
        auto impossible=cache.current(owner,3,f.residency,1,1);CHECK_FALSE(impossible);CHECK(impossible.defer_preparation);
        CHECK(cache.stats().entries==0);CHECK(domain->bytes()==0);
        CHECK(f.budget.snapshot().admitted==before.admitted);CHECK(f.budget.snapshot().deferred==before.deferred);
    }
}
