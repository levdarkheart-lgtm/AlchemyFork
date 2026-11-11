/**
 * @file alframefences_metal_test.mm
 * @brief Viewer logical-frame retirement and pacing over real Metal submissions.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alframefences.h"
#include "almetalframe.h"
#include "almetalresource.h"

#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <dispatch/dispatch.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace
{
constexpr U64 second = 1'000'000'000ull;

// Test control, not a GPU workload/timing estimate. Every queued command has a
// real GPU-side wait and an exact blit result. A failed assertion releases all
// gates before the fixture drains its scheduler; a watchdog also bounds any
// accidental wait on the wrong logical frame.
struct Gate
{
    id<MTLSharedEvent> event;
    std::shared_ptr<std::atomic<int>> phase = std::make_shared<std::atomic<int>>(0);
    explicit Gate(id<MTLSharedEvent> value) : event(value)
    {
        id<MTLSharedEvent> retained = event;
        auto state = phase;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC),
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0), ^{
                int expected=0;
                if (state->compare_exchange_strong(expected,2)) retained.signaledValue=1;
            });
    }
    void release()
    {
        int expected=0;
        phase->compare_exchange_strong(expected,1);
        event.signaledValue=1;
    }
    void releaseAfter(unsigned milliseconds)
    {
        id<MTLSharedEvent> retained = event;
        auto state = phase;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, std::int64_t(milliseconds)*NSEC_PER_MSEC),
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0), ^{
                int expected=0;
                if (state->compare_exchange_strong(expected,1)) retained.signaledValue=1;
            });
    }
    bool watchdogFired() const { return phase->load()==2; }
    ~Gate() { release(); }
};

struct Packet
{
    std::unique_ptr<ALMetalBuffer> bytes;
    std::shared_ptr<const ALMetalCommandCompletion> completion;
    U64 serial=0;
    U8 value=0;
};

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalFrameScheduler> scheduler, replacement;
    ALFrameFences frames;
    std::vector<std::unique_ptr<Gate>> gates;

    Fixture()
    {
        device=ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        scheduler=ALMetalFrameScheduler::create(*device,3,&error);
        INFO(error); REQUIRE(scheduler);
        frames.setMetalScheduler(scheduler.get());
    }
    ~Fixture()
    {
        for (auto& gate:gates) gate->release();
        // Both possible schedulers outlive the borrowed pointer and drain.
        frames.setMetalScheduler(nullptr);
        if (scheduler) scheduler->waitUntilIdle(2000,nullptr);
        if (replacement) replacement->waitUntilIdle(2000,nullptr);
    }
    Gate& gate()
    {
        id<MTLSharedEvent> event=[(__bridge id<MTLDevice>)device->nativeHandle() newSharedEvent];
        REQUIRE(event);
        gates.push_back(std::make_unique<Gate>(event));
        return *gates.back();
    }
    Packet submit(Gate* gate=nullptr, U8 value=0x4b, ALMetalFrameScheduler* selected=nullptr)
    {
        if (!selected) selected=scheduler.get();
        ALMetalBufferDescriptor descriptor;
        descriptor.length=16;
        descriptor.storage=ALMetalStorageMode::shared;
        descriptor.label="Frame-fence actual GPU completion witness";
        Packet result;
        result.bytes=ALMetalBuffer::create(*device,descriptor,&error); REQUIRE(result.bytes);
        const std::array<U8,16> initial{};
        REQUIRE(result.bytes->write(0,initial.data(),initial.size(),&error));
        auto frame=selected->beginFrame("Frame-fence native blit frame",1000,&error);
        INFO(error); REQUIRE(frame);
        id<MTLCommandBuffer> command=(__bridge id<MTLCommandBuffer>)frame->commandBuffer().nativeHandle();
        if (gate) [command encodeWaitForEvent:gate->event value:1];
        id<MTLBlitCommandEncoder> blit=[command blitCommandEncoder]; REQUIRE(blit);
        [blit fillBuffer:(__bridge id<MTLBuffer>)result.bytes->nativeHandle()
            range:NSMakeRange(0,16) value:value];
        [blit endEncoding];
        result.completion=frame->commandBuffer().completionToken(); REQUIRE(result.completion);
        REQUIRE(selected->submit(*frame,&error));
        result.serial=selected->lastSubmittedSerial();
        result.value=value;
        return result;
    }
    void finish(Packet& packet, ALMetalFrameScheduler* selected=nullptr)
    {
        if (!selected) selected=scheduler.get();
        REQUIRE(selected->waitForSubmission(packet.serial,2*second,&error));
        CHECK(selected->isSubmissionComplete(packet.serial));
        std::array<U8,16> actual{};
        REQUIRE(packet.bytes->read(0,actual.data(),actual.size(),&error));
        for (auto byte:actual) CHECK(byte==packet.value);
    }
    void pending(const Packet& packet, ALMetalFrameScheduler* selected=nullptr)
    {
        if (!selected) selected=scheduler.get();
        CHECK_FALSE(selected->isSubmissionComplete(packet.serial));
        CHECK_FALSE(packet.completion->isComplete());
        std::array<U8,16> actual;
        REQUIRE(packet.bytes->read(0,actual.data(),actual.size(),&error));
        for (auto byte:actual) CHECK(byte==0);
    }
    void checkGates()
    {
        for (const auto& gate:gates) CHECK_FALSE(gate->watchdogFired());
    }
};
}

TEST_CASE("ALFrameFences: ready native work becomes complete at a later logical frame end",
          "[alframefences][almetal]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALFrameFences detached;
        CHECK(detached.isComplete(0));
        CHECK_FALSE(detached.isComplete(detached.frame()));
        bool stalled=true;
        CHECK_FALSE(detached.wait(detached.frame(),0,&stalled));
        CHECK_FALSE(stalled);
        CHECK(detached.getCalls()==0);
        auto packet=fixture.submit();
        fixture.finish(packet);
        auto& frames=fixture.frames;
        const auto first=frames.frame(), calls=frames.getCalls();
        CHECK_FALSE(frames.isComplete(first));
        frames.endFrame();
        CHECK(frames.frame()==first+1);
        CHECK_FALSE(frames.isComplete(first));
        CHECK(frames.getCalls()==calls); // No second native fence/API submission.
        frames.endFrame();
        CHECK(frames.isComplete(first));
        CHECK_FALSE(frames.isComplete(first+1));
        CHECK(frames.getCalls()==calls+1); // Exactly one earlier submission query.
        const auto before=frames.getCalls();
        stalled=true;
        CHECK(frames.wait(first,0,&stalled));
        CHECK_FALSE(stalled);
        CHECK(frames.getCalls()==before); // Cached completion is CPU-only.
    }
}

TEST_CASE("ALFrameFences: native polls stop at the oldest pending frame then retire all completed predecessors",
          "[alframefences][almetal]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto& gate=fixture.gate();
        const auto first=fixture.frames.frame();
        std::vector<Packet> packets;
        for (unsigned i=0;i<3;++i)
        {
            packets.push_back(fixture.submit(i==0 ? &gate : nullptr,U8(0x40+i)));
            fixture.frames.endFrame();
        }
        CHECK_FALSE(fixture.frames.isComplete(first));
        CHECK(fixture.frames.getCalls()==2); // One oldest-only poll at each later end.
        for (const auto& packet:packets) fixture.pending(packet);
        gate.release();
        fixture.finish(packets.back());
        fixture.frames.endFrame();
        CHECK(fixture.frames.isComplete(first+2));
        CHECK_FALSE(fixture.frames.isComplete(first+3));
        CHECK(fixture.frames.getCalls()==5); // All three earlier logical frame ends.
        for (auto& packet:packets) fixture.finish(packet);
        fixture.checkGates();
    }
}

TEST_CASE("ALFrameFences: native finite waits count every query and reject unsubmitted open frames",
          "[alframefences][almetal][frame-fences-call-count]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto& gate=fixture.gate();
        auto packet=fixture.submit(&gate);
        const auto ended=fixture.frames.frame();
        fixture.frames.endFrame();
        const auto calls=fixture.frames.getCalls(), stalls=fixture.frames.getStalls();
        bool stalled=false;
        CHECK_FALSE(fixture.frames.wait(ended,0,&stalled));
        CHECK(stalled);
        CHECK(fixture.frames.getStalls()==stalls+1);
        // poll + readiness classification + finite scheduler wait. The native
        // path has one more query than the original driver's poll + wait pair.
        CHECK(fixture.frames.getCalls()==calls+3);
        fixture.pending(packet);
        const auto pending_calls=fixture.frames.getCalls();
        const auto submissions=ALMetalDevice::committedCommandBufferCount();
        const auto open=fixture.frames.frame();
        stalled=true;
        CHECK_FALSE(fixture.frames.wait(open,second,&stalled));
        CHECK_FALSE(stalled);
        CHECK_FALSE(fixture.frames.wait(open+1,0));
        CHECK(fixture.frames.getCalls()==pending_calls);
        CHECK(fixture.frames.frame()==open);
        CHECK_FALSE(fixture.frames.isComplete(ended));
        CHECK_FALSE(fixture.frames.isComplete(open));
        CHECK(ALMetalDevice::committedCommandBufferCount()==submissions);
        gate.release();
        fixture.finish(packet);
        stalled=true;
        CHECK(fixture.frames.wait(ended,second,&stalled));
        CHECK_FALSE(stalled);
        CHECK(fixture.frames.isComplete(ended));
        CHECK(fixture.frames.getStalls()==stalls+1);
        const auto ready_calls=fixture.frames.getCalls();
        CHECK(fixture.frames.wait(ended,0));
        CHECK(fixture.frames.getCalls()==ready_calls);
        fixture.checkGates();
    }
}

TEST_CASE("ALFrameFences: the full native ring waits out its oldest submission before slot reuse",
          "[alframefences][almetal]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto& gate=fixture.gate();
        auto packet=fixture.submit(&gate);
        const auto first=fixture.frames.frame();
        for (unsigned i=0;i<ALFrameFences::FRAMES;++i) fixture.frames.endFrame();
        CHECK_FALSE(fixture.frames.isComplete(first));
        CHECK(fixture.frames.getStalls()==0);
        CHECK(fixture.frames.getPacingWaits()==0);
        fixture.pending(packet);
        gate.releaseAfter(100);
        fixture.frames.endFrame(); // Must make room before recording slot one again.
        CHECK(fixture.frames.isComplete(first+ALFrameFences::FRAMES-1));
        CHECK_FALSE(fixture.frames.isComplete(fixture.frames.frame()-1));
        CHECK(fixture.frames.getStalls()==1);
        CHECK(fixture.frames.getPacingWaits()==0);
        fixture.finish(packet);
        for (unsigned i=0;i<ALFrameFences::FRAMES+1;++i) fixture.frames.endFrame();
        CHECK(fixture.frames.isComplete(fixture.frames.frame()-2));
        CHECK_FALSE(fixture.frames.isComplete(fixture.frames.frame()-1));
        fixture.checkGates();
    }
}

TEST_CASE("ALFrameFences: release detach and replacement drain the open committed native timeline",
          "[alframefences][almetal]")
{
    @autoreleasepool
    {
        Fixture fixture;
        enum class Action { release, detach, replace } action=Action::release;
        SECTION("release") { action=Action::release; }
        SECTION("detach") { action=Action::detach; }
        SECTION("replace")
        {
            action=Action::replace;
            fixture.replacement=ALMetalFrameScheduler::create(*fixture.device,2,&fixture.error);
            REQUIRE(fixture.replacement);
        }
        auto& gate=fixture.gate();
        auto packet=fixture.submit(&gate);
        const auto open=fixture.frames.frame();
        fixture.pending(packet);
        // This committed native serial has not yet been stored by endFrame().
        gate.releaseAfter(100);
        if (action==Action::release) fixture.frames.release();
        else fixture.frames.setMetalScheduler(action==Action::detach ? nullptr : fixture.replacement.get());
        CHECK(fixture.frames.isComplete(open));
        CHECK(fixture.frames.frame()==open+1);
        CHECK_FALSE(fixture.frames.isComplete(fixture.frames.frame()));
        fixture.finish(packet);
        auto* selected=action==Action::replace ? fixture.replacement.get() : fixture.scheduler.get();
        fixture.frames.setMetalScheduler(selected);
        const auto next=fixture.frames.frame();
        auto fresh=fixture.submit(nullptr,0x68,selected);
        fixture.frames.endFrame();
        CHECK_FALSE(fixture.frames.isComplete(next));
        fixture.finish(fresh,selected);
        fixture.frames.endFrame();
        CHECK(fixture.frames.isComplete(next));
        CHECK(fixture.frames.slack().budget()==0.0);
        fixture.checkGates();
    }
}

TEST_CASE("ALFrameFences: paced native ends wait exactly one frame behind without counting stalls",
          "[alframefences][almetal]")
{
    @autoreleasepool
    {
        Fixture fixture;
        Gate* preceding=nullptr;
        std::vector<Packet> packets;
        const auto first=fixture.frames.frame();
        for (unsigned i=0;i<3;++i)
        {
            auto& current=fixture.gate();
            packets.push_back(fixture.submit(&current,U8(0x50+i)));
            if (preceding) preceding->releaseAfter(100);
            fixture.frames.endFrame(1);
            CHECK_FALSE(fixture.frames.isComplete(first+i));
            if (i) CHECK(fixture.frames.isComplete(first+i-1));
            fixture.pending(packets.back());
            preceding=&current;
        }
        CHECK(fixture.frames.getPacingWaits()==2);
        CHECK(fixture.frames.getStalls()==0);
        CHECK(fixture.frames.getCalls()==4); // A query + wait per actual pacing wait.
        preceding->release();
        for (auto& packet:packets) fixture.finish(packet);
        fixture.checkGates();
    }
}

TEST_CASE("ALFrameFences: unpaced and two-ahead ends leave later native work pending",
          "[alframefences][almetal]")
{
    @autoreleasepool
    {
        Fixture fixture;
        unsigned ahead=0;
        SECTION("unpaced") { ahead=0; }
        SECTION("two frames ahead") { ahead=2; }
        auto ready=fixture.submit();
        fixture.finish(ready);
        fixture.frames.endFrame(ahead);
        auto& gate=fixture.gate();
        auto busy=fixture.submit(&gate);
        const auto frame=fixture.frames.frame();
        fixture.frames.endFrame(ahead);
        fixture.frames.endFrame(ahead);
        CHECK(fixture.frames.isComplete(frame-1));
        CHECK_FALSE(fixture.frames.isComplete(frame));
        CHECK(fixture.frames.getPacingWaits()==0);
        CHECK(fixture.frames.getStalls()==0);
        fixture.pending(busy);
        CHECK(gate.event.signaledValue==0);
        gate.release();
        fixture.finish(busy);
        fixture.checkGates();
    }
}

TEST_CASE("ALFrameFences: actual native pacing slack grants a budget and resets at its original boundaries",
          "[alframefences][almetal]")
{
    @autoreleasepool
    {
        Fixture fixture;
        enum class Reset { ready, unpaced, release } reset=Reset::ready;
        SECTION("a paced wait that finds completed work") { reset=Reset::ready; }
        SECTION("an unpaced end") { reset=Reset::unpaced; }
        SECTION("release") { reset=Reset::release; }
        auto* preceding=&fixture.gate();
        auto previous=fixture.submit(preceding);
        fixture.frames.endFrame(1);
        for (unsigned i=0;i<ALPacingSlack::FRAMES;++i)
        {
            auto& current=fixture.gate();
            auto packet=fixture.submit(&current);
            preceding->releaseAfter(100);
            fixture.frames.endFrame(1);
            fixture.finish(previous);
            fixture.pending(packet);
            preceding=&current;
            previous=std::move(packet);
        }
        CHECK(fixture.frames.getPacingWaits()==ALPacingSlack::FRAMES);
        CHECK(fixture.frames.getStalls()==0);
        CHECK(fixture.frames.slack().budget()>fixture.frames.slack().reserve());
        if (reset==Reset::ready)
        {
            preceding->release();
            fixture.finish(previous);
            const auto waits=fixture.frames.getPacingWaits();
            fixture.frames.endFrame(1);
            CHECK(fixture.frames.getPacingWaits()==waits);
        }
        else if (reset==Reset::unpaced) fixture.frames.endFrame();
        else
        {
            preceding->releaseAfter(100);
            fixture.frames.release();
            fixture.finish(previous);
        }
        CHECK(fixture.frames.slack().budget()==0.0);
        fixture.checkGates();
    }
}

TEST_CASE("ALFrameFences: native long waits retry past one second and preserve pacing versus stall accounting",
          "[alframefences][almetal]")
{
    @autoreleasepool
    {
        Fixture fixture;
        enum class Wait { pace, finish_long, finish_short } wait=Wait::pace;
        SECTION("pacing retries") { wait=Wait::pace; }
        SECTION("resource wait retries") { wait=Wait::finish_long; }
        SECTION("one resource wait and a cached repeat") { wait=Wait::finish_short; }
        auto& gate=fixture.gate();
        auto packet=fixture.submit(&gate);
        const auto busy=fixture.frames.frame();
        fixture.frames.endFrame();
        const auto calls=fixture.frames.getCalls();
        gate.releaseAfter(wait==Wait::finish_short ? 100 : 1500);
        bool stalled=false;
        if (wait==Wait::pace)
        {
            fixture.frames.endFrame(1);
            CHECK(fixture.frames.getPacingWaits()==1);
            CHECK(fixture.frames.getStalls()==0);
            CHECK(fixture.frames.getCalls()>=calls+3); // Query + at least two waits.
        }
        else
        {
            fixture.frames.waitFinished(busy,&stalled);
            CHECK(stalled);
            CHECK(fixture.frames.getStalls()==(wait==Wait::finish_short ? 1u : 2u));
            CHECK(fixture.frames.getPacingWaits()==0);
        }
        CHECK(fixture.frames.isComplete(busy));
        fixture.finish(packet);
        const auto ready_calls=fixture.frames.getCalls();
        stalled=true;
        fixture.frames.waitFinished(busy,&stalled);
        CHECK_FALSE(stalled);
        CHECK(fixture.frames.getCalls()==ready_calls);
        fixture.checkGates();
    }
}
