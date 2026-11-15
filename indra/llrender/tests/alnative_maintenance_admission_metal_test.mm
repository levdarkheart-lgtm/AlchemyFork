/**
 * @file alnative_maintenance_admission_metal_test.mm
 * @brief Actual native texture reduction admission and completion ownership.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "llfontmetal_fixture.h"
#include "alnativeframebudget.h"
#import <Metal/Metal.h>
#include <dispatch/dispatch.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

namespace
{
namespace NativeMaintenance
{
using Result=LLImageGL::ScaleDownResult;
struct Fixture : ll_test::NativeFontUI
{
    std::string error;
    Fixture(){initRenderer();}
    LLPointer<LLImageGL> image(bool mips)
    {
        LLPointer<LLImageGL> value=new LLImageGL(8,8,4,mips);
        value->setNeedsAlphaAndPickMask(false);
        value->setExplicitFormat(GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE);
        const std::array<LLColor4U,4> colors{
            LLColor4U(255,0,0,255),LLColor4U(0,255,0,255),
            LLColor4U(0,0,255,255),LLColor4U(255,255,255,255)};
        std::array<std::vector<U8>,4> pixels;
        std::array<LLImageGL::MetalMipLevel,4> levels;
        for(unsigned mip=0;mip<(mips?4u:1u);++mip)
        {
            const U32 width=8u>>mip;pixels[mip].resize(width*width*4);
            for(std::size_t at=0;at<pixels[mip].size();at+=4)
                std::memcpy(pixels[mip].data()+at,&colors[mip],4);
            levels[mip]={pixels[mip].data(),width,width};
        }
        if(mips)REQUIRE(value->createMetalTexture(0,std::span(levels)));
        else REQUIRE(value->createMetalTexture(0,pixels[0].data()));
        REQUIRE(value->getMetalTexture());return value;
    }
    std::vector<U8> pixels(const ALMetalTexture& value)
    {
        std::vector<U8> result(value.width()*value.height()*4);
        REQUIRE(value.readback(*device,0,0,{},result.data(),value.width()*4,&error));return result;
    }
    void settled(ALNativeFrameBudget& budget)
    {
        REQUIRE(device->waitUntilIdle(&error));
        const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(budget.snapshot().maintenance_in_flight && std::chrono::steady_clock::now()<until)
            std::this_thread::yield();
        CHECK(budget.snapshot().maintenance_in_flight==0);
        CHECK(budget.snapshot().maintenance_bytes_in_flight==0);
    }
};
struct QueueGate
{
    id<MTLSharedEvent> event;
    std::shared_ptr<std::atomic<unsigned>> phase=std::make_shared<std::atomic<unsigned>>(0);
    std::unique_ptr<ALMetalCommandBuffer> blocker;
    explicit QueueGate(Fixture& f)
    {
        event=[(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent];REQUIRE(event);
        id<MTLSharedEvent> retained=event;auto state=phase;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,5*NSEC_PER_SEC),
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
                unsigned expected=0;if(state->compare_exchange_strong(expected,2))retained.signaledValue=1;
            });
        blocker=f.device->makeCommandBuffer("Hold native maintenance completion",&f.error);REQUIRE(blocker);
        [(__bridge id<MTLCommandBuffer>)blocker->nativeHandle() encodeWaitForEvent:event value:1];
        REQUIRE(blocker->commit(&f.error));
    }
    void release(){unsigned expected=0;phase->compare_exchange_strong(expected,1);event.signaledValue=1;}
    bool watchdogFired() const{return phase->load()==2;}
    ~QueueGate(){release();}
};
void noMutation(Fixture& f,LLImageGL& image,ALNativeFrameBudget& budget,Result expected)
{
    const auto* texture=image.getMetalTexture();const auto discard=image.getDiscardLevel();
    const auto mutation=image.nativePublication().mutationRevision(),publication=image.nativePublication().publishedRevision();
    const auto allocations=alMetalResourceAllocationStats();const auto commands=ALMetalDevice::committedCommandBufferCount();
    CHECK(image.scaleDownWithBudget(discard+1,budget)==expected);
    CHECK(image.getMetalTexture()==texture);CHECK(image.getDiscardLevel()==discard);
    CHECK(image.nativePublication().mutationRevision()==mutation);
    CHECK(image.nativePublication().publishedRevision()==publication);
    const auto after=alMetalResourceAllocationStats();
    CHECK(after.textures_created==allocations.textures_created);CHECK(after.buffers_created==allocations.buffers_created);
    CHECK(ALMetalDevice::committedCommandBufferCount()==commands);
    (void)f;
}
}

TEST_CASE("Native texture maintenance denial leaves completed image generations and mutation signals untouched",
          "[almetal][native-maintenance-admission][PPD-277]")
{
    using namespace NativeMaintenance;
    @autoreleasepool
    {
        Fixture f;
        for(bool mips:{false,true})
        {
            CAPTURE(mips);auto image=f.image(mips);auto original=image->getMetalTextureLease();REQUIRE(original);
            const auto expected=f.pixels(*original);
            ALNativeFrameBudget budget;ALNativeFrameBudget::FrameInputs frame;frame.frame=1;budget.beginFrame(frame);
            auto first=budget.tryAcquire(ALNativeFrameBudget::Work::native_maintenance,64);
            auto second=budget.tryAcquire(ALNativeFrameBudget::Work::native_maintenance,64);REQUIRE(first);REQUIRE(second);
            noMutation(f,*image,budget,Result::deferred);CHECK(f.pixels(*original)==expected);
            const auto occupied=budget.snapshot();CHECK(occupied.maintenance_in_flight==2);
            CHECK(occupied.maintenance_this_frame==2);CHECK(occupied.maintenance_bytes_this_frame==128);
            CHECK(image->scaleDownWithBudget(0,budget)==Result::not_needed);
            CHECK(budget.snapshot().maintenance_this_frame==2);
            first.credit.reset();second.credit.reset();CHECK(budget.snapshot().maintenance_in_flight==0);
            frame.frame=2;budget.beginFrame(frame);budget.beginDrain();
            noMutation(f,*image,budget,Result::deferred);CHECK(budget.snapshot().maintenance_this_frame==0);
        }
    }
}

TEST_CASE("Native mip blits and resampling hold their own admission through actual queued GPU completion",
          "[almetal][native-maintenance-admission][PPD-277]")
{
    using namespace NativeMaintenance;
    @autoreleasepool
    {
        Fixture f;
        for(bool mips:{false,true})
        {
            CAPTURE(mips);
            // Finish all uploads, library creation and reference reads BEFORE
            // blocking this device queue. The tested call only enqueues work.
            auto image=f.image(mips),control=f.image(mips),later=f.image(mips);
            auto original=image->getMetalTextureLease();REQUIRE(original);
            const auto old_pixels=f.pixels(*original);
            REQUIRE(control->scaleDown(1));const auto expected=f.pixels(*control->getMetalTexture());
            ALNativeFrameBudget::Limits limits;limits.maintenance_per_frame=1;limits.maintenance_in_flight=1;
            ALNativeFrameBudget budget(limits);ALNativeFrameBudget::FrameInputs frame;frame.frame=1;budget.beginFrame(frame);
            QueueGate gate(f);
            const auto mutation=image->nativePublication().mutationRevision();
            const auto publication=image->nativePublication().publishedRevision();
            const auto commands=ALMetalDevice::committedCommandBufferCount();
            REQUIRE(image->scaleDownWithBudget(1,budget)==Result::submitted);
            CHECK(ALMetalDevice::committedCommandBufferCount()==commands+1);
            // PPD280: willWrite and didPublish each advance mutation; only
            // didPublish advances the completed-publication revision.
            CHECK(image->getDiscardLevel()==1);CHECK(image->nativePublication().mutationRevision()==mutation+2);
            CHECK(image->nativePublication().publishedRevision()==publication+1);
            auto reduced=image->getMetalTextureLease();REQUIRE(reduced);CHECK(reduced.get()!=original.get());
            CHECK(reduced->width()==4);CHECK(reduced->height()==4);
            auto pending=budget.snapshot();CHECK(pending.maintenance_in_flight==1);
            CHECK(pending.maintenance_bytes_in_flight==(mips?84:64));
            CHECK(pending.maintenance_bytes_this_frame==(mips?84:64));
            // A new frame cannot release debt owned by an actual GPU command.
            frame.frame=2;budget.beginFrame(frame);noMutation(f,*later,budget,Result::deferred);
            image=nullptr;CHECK(budget.snapshot().maintenance_in_flight==1);
            gate.release();f.settled(budget);CHECK_FALSE(gate.watchdogFired());
            CHECK(f.pixels(*reduced)==expected);CHECK(f.pixels(*original)==old_pixels);
            frame.frame=3;budget.beginFrame(frame);REQUIRE(later->scaleDownWithBudget(1,budget)==Result::submitted);
            f.settled(budget);CHECK(f.pixels(*later->getMetalTexture())==expected);
        }
    }
}

TEST_CASE("Native maintenance releases failed and abandoned credits and progresses under pressure without a burst",
          "[almetal][native-maintenance-admission][PPD-277]")
{
    using namespace NativeMaintenance;
    @autoreleasepool
    {
        Fixture f;auto image=f.image(true);auto later=f.image(false);
        const auto* original=image->getMetalTexture();const auto discard=image->getDiscardLevel();
        const auto publication=image->nativePublication().publishedRevision();
        ALNativeFrameBudget budget;ALNativeFrameBudget::FrameInputs frame;frame.frame=1;budget.beginFrame(frame);
        const auto allocations=alMetalResourceAllocationStats();
        const auto commands=ALMetalDevice::committedCommandBufferCount();
        {
            struct RestoreDevice{ALMetalDevice* device;~RestoreDevice(){LLImageGL::setMetalDevice(device);}} restore{f.device.get()};
            LLImageGL::setMetalDevice(nullptr);CHECK(image->scaleDownWithBudget(1,budget)==Result::failed);
        }
        CHECK(budget.snapshot().maintenance_this_frame==1);CHECK(budget.snapshot().maintenance_in_flight==0);
        CHECK(budget.snapshot().maintenance_bytes_in_flight==0);CHECK(image->getMetalTexture()==original);
        CHECK(image->getDiscardLevel()==discard);CHECK(image->nativePublication().publishedRevision()==publication);
        CHECK(ALMetalDevice::committedCommandBufferCount()==commands);
        CHECK(alMetalResourceAllocationStats().textures_created==allocations.textures_created);
        CHECK(alMetalResourceAllocationStats().buffers_created==allocations.buffers_created);

        // This separately covers the native command retainer's abandonment
        // contract. scaleDown itself always commits its successful command.
        frame.frame=2;budget.beginFrame(frame);
        auto decision=budget.tryAcquire(ALNativeFrameBudget::Work::native_maintenance,84);REQUIRE(decision);
        auto command=f.device->makeCommandBuffer("Abandon native maintenance credit",&f.error);REQUIRE(command);
        REQUIRE(command->retainUntilCompletion(decision.credit,&f.error));auto completion=command->completionToken();REQUIRE(completion);
        decision.credit.reset();CHECK(budget.snapshot().maintenance_in_flight==1);command.reset();
        CHECK(completion->isComplete());CHECK(budget.snapshot().maintenance_in_flight==0);

        ALNativeFrameBudget::Limits limits;limits.maintenance_bytes_per_frame=1;limits.maintenance_bytes_in_flight=1;
        ALNativeFrameBudget pressure(limits);frame.frame=3;frame.os_available_bytes=0;pressure.beginFrame(frame);
        REQUIRE(pressure.snapshot().memory_pressure);
        REQUIRE(image->scaleDownWithBudget(1,pressure)==Result::submitted);
        f.settled(pressure);CHECK(pressure.snapshot().maintenance_this_frame==1);
        CHECK(pressure.snapshot().maintenance_bytes_this_frame==84); // Solitary oversized progress.
        noMutation(f,*later,pressure,Result::deferred);
        frame.frame=4;pressure.beginFrame(frame);REQUIRE(later->scaleDownWithBudget(1,pressure)==Result::submitted);
        f.settled(pressure);CHECK(pressure.snapshot().maintenance_bytes_this_frame==64);
    }
}
}
