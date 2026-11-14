/**
 * @file almetal_world_source_admission_test.mm
 * @brief Dirty source work admission before native publication and exact command retirement.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_source_pages_fixture.h"
#include "alnativeframebudget.h"

namespace
{
using SourceBudget=ALNativeFrameBudget;
SourceBudget::Limits admissionLimits()
{
    SourceBudget::Limits limits;limits.scene_publications_per_frame=2;limits.scene_publications_in_flight=1;
    limits.scene_bytes_per_frame=2*1024*1024;limits.scene_bytes_in_flight=4*1024*1024;
    limits.maximum_deferral_frames=3;return limits;
}
SourceBudget::FrameInputs admissionFrame(std::uint64_t frame,bool pressure=false)
{
    SourceBudget::FrameInputs input;input.frame=frame;input.frame_interval_ns=16'666'667;
    if(pressure){input.required_main_cpu_ns=16'000'000;input.native_budget_bytes=1;input.native_allocated_bytes=1;}
    return input;
}
struct AdmissionPolicy
{
    SourceBudget budget{admissionLimits()};
    unsigned calls=0;
    SourceBudget::Frame oldest=0;
    ALMetalWorldSourcePublicationWork last;
    std::shared_ptr<const void> admit(const ALMetalWorldSourcePublicationWork& work)
    {
        ++calls;last=work;REQUIRE(work.chargedBytes()>0);
        if(!oldest)oldest=budget.snapshot().frame;
        auto result=budget.tryAcquire(SourceBudget::Work::scene_publication,work.chargedBytes(),oldest);
        if(result)oldest=0;
        return result.credit;
    }
};
void waitSceneCredit(SourceBudget& budget)
{
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(budget.snapshot().scene_publications_in_flight&&std::chrono::steady_clock::now()<end)std::this_thread::yield();
    REQUIRE(budget.snapshot().scene_publications_in_flight==0);
    CHECK(budget.snapshot().scene_bytes_in_flight==0);
}
void unrelatedLanesRemainEmpty(const SourceBudget::Snapshot& snapshot)
{
    CHECK(snapshot.uploads_this_frame==0);CHECK(snapshot.uploads_in_flight==0);
    CHECK(snapshot.upload_bytes_this_frame==0);CHECK(snapshot.upload_bytes_in_flight==0);
    CHECK(snapshot.optional_views_this_frame==0);CHECK(snapshot.optional_views_in_flight==0);
    CHECK(snapshot.preparation_in_flight==0);
}
}

TEST_CASE("almetal: denied source publication is read only and aged retry does not consume other work lanes",
          "[almetal][world-source-admission]")
{
    @autoreleasepool
    {
        PageFixture f;auto pages=f.pages();auto request=f.request();auto policy=std::make_shared<AdmissionPolicy>();
        policy->budget.beginFrame(admissionFrame(1,true));
        // Product FrameData copies one mutable closure per source context.
        // Its value-captured age must then persist across individual freezes.
        pages->setPublicationAdmission([policy,oldest=SourceBudget::Frame{0}](const auto& work) mutable ->std::shared_ptr<const void>
        {
            ++policy->calls;policy->last=work;
            if(!oldest)oldest=policy->budget.snapshot().frame;
            policy->oldest=oldest;
            auto result=policy->budget.tryAcquire(SourceBudget::Work::scene_publication,work.chargedBytes(),oldest);
            if(result){oldest=0;policy->oldest=0;}
            return result.credit;
        });
        const auto before=alMetalResourceAllocationStats();
        CHECK_FALSE(pages->freeze(std::span(&request,1),std::span(&f.region,1),f.globals,&f.error));
        CHECK(policy->calls==0); // A standalone call cannot park a credit.
        for(unsigned attempt=0;attempt<2;++attempt)
        {
            auto command=f.device->makeCommandBuffer("Deferred source publication",&f.error);REQUIRE(command);
            CHECK_FALSE(pages->freezeForCommand(*command,std::span(&request,1),std::span(&f.region,1),f.globals,&f.error));
            CHECK(pages->stats().rows==0);CHECK(pages->stats().row_publications==0);
            CHECK(pages->stats().page_allocations==0);CHECK(pages->stats().bank_allocations==0);
            CHECK(pages->stats().live_metadata_bytes==0);
            CHECK(alMetalResourceAllocationStats().buffers_created==before.buffers_created);
        }
        CHECK(policy->calls==2);CHECK(policy->oldest==1);
        CHECK(policy->last.encoded_table_bytes>0);CHECK(policy->last.encoded_row_bytes>0);
        CHECK(policy->last.zeroed_page_bytes>0);CHECK(policy->last.zeroed_bank_bytes>0);
        unrelatedLanesRemainEmpty(policy->budget.snapshot());
        policy->budget.beginFrame(admissionFrame(4,true));
        auto command=f.device->makeCommandBuffer("Aged source publication",&f.error);REQUIRE(command);
        auto view=pages->freezeForCommand(*command,std::span(&request,1),std::span(&f.region,1),f.globals,&f.error);
        INFO(f.error);REQUIRE(view);
        CHECK(view->matchesSourceCommand(0,request.key,*f.indices));
        auto wrong_key=request.key;++wrong_key.revision;
        CHECK_FALSE(view->matchesSourceCommand(0,wrong_key,*f.indices));
        CHECK_FALSE(view->matchesSourceCommand(0,request.key,*f.vertices));
        CHECK_FALSE(view->matchesSourceCommand(view->size(),request.key,*f.indices));
        CHECK(policy->budget.snapshot().scene_publications_in_flight==1);
        CHECK(policy->budget.snapshot().scene_bytes_in_flight==policy->last.chargedBytes());
        CHECK(policy->budget.snapshot().progress_admissions==1);
        command.reset();waitSceneCredit(policy->budget);
        // A long-held actual view is not the owner of the publication receipt.
        REQUIRE(view);const auto calls=policy->calls;
        auto warm=pages->freeze(std::span(&request,1),std::span(&f.region,1),f.globals,&f.error);REQUIRE(warm);
        CHECK(policy->calls==calls);CHECK(pages->stats().row_publications==1);
        unrelatedLanesRemainEmpty(policy->budget.snapshot());
    }
}

TEST_CASE("almetal: source publication credits retire on the exact native command independently of long held views",
          "[almetal][world-source-admission]")
{
    @autoreleasepool
    {
        PageFixture f;auto pages=f.pages();auto request=f.request();auto policy=std::make_shared<AdmissionPolicy>();
        policy->budget.beginFrame(admissionFrame(1));pages->setPublicationAdmission([policy](const auto& work){return policy->admit(work);});
        auto command=f.device->makeCommandBuffer("Admitted source command",&f.error);REQUIRE(command);
        auto view=pages->freezeForCommand(*command,std::span(&request,1),std::span(&f.region,1),f.globals,&f.error);REQUIRE(view);
        auto encoded=f.encode(*view,f.cpu->source,std::move(command)); // PSO creation precedes the queue gate.
        const auto charged=policy->last.chargedBytes();REQUIRE(charged>0);
        id<MTLSharedEvent> gate=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(gate);
        struct OpenGate {id<MTLSharedEvent> gate;~OpenGate(){gate.signaledValue=1;}} open{gate};
        auto blocker=f.device->makeCommandBuffer("Hold admitted source work",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:gate value:1];REQUIRE(blocker->commit(&f.error));
        REQUIRE(encoded.command->commit(&f.error));
        CHECK(policy->budget.snapshot().scene_bytes_in_flight==charged);
        const auto calls=policy->calls;
        auto warm_command=f.device->makeCommandBuffer("Unchanged source command",&f.error);REQUIRE(warm_command);
        auto warm=pages->freezeForCommand(*warm_command,std::span(&request,1),std::span(&f.region,1),f.globals,&f.error);REQUIRE(warm);
        CHECK(policy->calls==calls);CHECK(policy->budget.snapshot().scene_publications_this_frame==1);
        gate.signaledValue=1;REQUIRE(f.device->waitUntilIdle(&f.error));waitSceneCredit(policy->budget);
        const auto pixels=f.read(*encoded.target);
        CHECK(std::any_of(pixels.depth.begin(),pixels.depth.end(),[](auto value){return value!=0x3f800000u;}));
        REQUIRE(view);REQUIRE(warm); // Neither retained view keeps the receipt.
        auto changed=std::make_shared<PageDescription>(*f.cpu);changed->source.materialSlots()[0].env_intensity=.8f;f.publish(changed->source);
        request.source=&changed->source;request.cpu_owner=changed;request.key.revision=2;pages->invalidate(1,2);
        policy->budget.beginFrame(admissionFrame(2));
        auto abandoned=f.device->makeCommandBuffer("Abandoned dirty source command",&f.error);REQUIRE(abandoned);
        auto new_view=pages->freezeForCommand(*abandoned,std::span(&request,1),std::span(&f.region,1),f.globals,&f.error);REQUIRE(new_view);
        CHECK(policy->last.copied_page_bytes>0);CHECK(policy->last.copied_bank_bytes>0);
        CHECK(policy->budget.snapshot().scene_publications_in_flight==1);
        abandoned.reset();waitSceneCredit(policy->budget);REQUIRE(new_view);
        unrelatedLanesRemainEmpty(policy->budget.snapshot());
    }
}

TEST_CASE("almetal: dirty source plans coalesce equal native tables before any allocation",
          "[almetal][world-source-admission]")
{
    @autoreleasepool
    {
        PageFixture f;auto pages=f.pages();auto first=f.request();
        auto other=std::make_shared<PageDescription>(*f.cpu);other->source.materialSlots()[0].env_intensity=.7f;f.publish(other->source);
        auto second=f.request(2);second.source=&other->source;second.cpu_owner=other;
        ALMetalWorldSourcePublicationWork observed;unsigned calls=0;
        pages->setPublicationAdmission([&](const auto& work)->std::shared_ptr<const void>{observed=work;++calls;return {};});
        const auto before=alMetalResourceAllocationStats();
        auto denied=f.device->makeCommandBuffer("Denied unique table plan",&f.error);REQUIRE(denied);
        CHECK_FALSE(pages->freezeForCommand(*denied,std::span(&first,1),std::span(&f.region,1),f.globals,&f.error));
        const auto single=observed;
        std::array<ALMetalWorldSourceRequest,2> requests{first,second};
        CHECK_FALSE(pages->freezeForCommand(*denied,requests,std::span(&f.region,1),f.globals,&f.error));
        CHECK(calls==2);CHECK(observed.encoded_row_bytes==single.encoded_row_bytes*2);
        CHECK(observed.encoded_table_bytes==single.encoded_table_bytes);
        CHECK(observed.zeroed_page_bytes==single.zeroed_page_bytes);
        CHECK(alMetalResourceAllocationStats().buffers_created==before.buffers_created);
        CHECK(pages->stats().rows==0);CHECK(pages->stats().row_publications==0);
        SourceBudget budget(admissionLimits());budget.beginFrame(admissionFrame(1));
        pages->setPublicationAdmission([&](const auto& work)->std::shared_ptr<const void>
            {return budget.tryAcquire(SourceBudget::Work::scene_publication,work.chargedBytes()).credit;});
        auto view=pages->freezeForCommand(*denied,requests,std::span(&f.region,1),f.globals,&f.error);REQUIRE(view);
        CHECK(pages->stats().row_publications==2);CHECK_FALSE(view->tableGenerationForTesting(0).expired());
        CHECK(view->tableGenerationForTesting(1).expired()); // One actual unique native argument table.
        denied.reset();waitSceneCredit(budget);
    }
}

TEST_CASE("almetal: source admission rejects a reentrant retired plan before native publication",
          "[almetal][world-source-admission]")
{
    @autoreleasepool
    {
        PageFixture f;auto pages=f.pages();auto request=f.request();SourceBudget budget(admissionLimits());
        budget.beginFrame(admissionFrame(1));
        pages->setPublicationAdmission([&](const auto& work)->std::shared_ptr<const void>
        {
            auto grant=budget.tryAcquire(SourceBudget::Work::scene_publication,work.chargedBytes());
            pages->invalidate(request.key.identity,2);return grant.credit;
        });
        const auto before=alMetalResourceAllocationStats();auto command=f.device->makeCommandBuffer("Retired source plan",&f.error);REQUIRE(command);
        CHECK_FALSE(pages->freezeForCommand(*command,std::span(&request,1),std::span(&f.region,1),f.globals,&f.error));
        CHECK(pages->stats().row_publications==0);CHECK(pages->stats().live_metadata_bytes==0);CHECK(pages->stats().rows==0);
        CHECK(alMetalResourceAllocationStats().buffers_created==before.buffers_created);waitSceneCredit(budget);
        request.key.revision=2;budget.beginFrame(admissionFrame(2));
        pages->setPublicationAdmission([&](const auto& work)->std::shared_ptr<const void>
            {return budget.tryAcquire(SourceBudget::Work::scene_publication,work.chargedBytes()).credit;});
        auto recovered=pages->freezeForCommand(*command,std::span(&request,1),std::span(&f.region,1),f.globals,&f.error);REQUIRE(recovered);
        command.reset();waitSceneCredit(budget);unrelatedLanesRemainEmpty(budget.snapshot());
        request.key.revision=3;pages->invalidate(1,3);budget.beginFrame(admissionFrame(3));
        struct CallbackLife {bool* ended;~CallbackLife(){*ended=true;}};
        bool ended=false;auto life=std::make_shared<CallbackLife>();life->ended=&ended;
        std::weak_ptr<CallbackLife> weak=life;
        pages->setPublicationAdmission([&,life](const auto& work)->std::shared_ptr<const void>
        {
            auto grant=budget.tryAcquire(SourceBudget::Work::scene_publication,work.chargedBytes());
            pages->setPublicationAdmission({});
            CHECK_FALSE(weak.expired()); // The executing closure survives replacement.
            return grant.credit;
        });
        life.reset();const auto before_replace=pages->stats();
        auto replaced=f.device->makeCommandBuffer("Replaced source policy",&f.error);REQUIRE(replaced);
        CHECK_FALSE(pages->freezeForCommand(*replaced,std::span(&request,1),std::span(&f.region,1),f.globals,&f.error));
        CHECK(ended);CHECK(weak.expired());CHECK(pages->stats().row_publications==before_replace.row_publications);
        CHECK(pages->stats().copied_page_bytes==before_replace.copied_page_bytes);
        waitSceneCredit(budget);
        ALMetalWorldSourcePublicationWork overflow;overflow.encoded_row_bytes=std::numeric_limits<std::uint64_t>::max();overflow.encoded_table_bytes=1;
        CHECK(overflow.chargedBytes()==std::numeric_limits<std::uint64_t>::max());
    }
}
