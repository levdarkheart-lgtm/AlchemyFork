/**
 * @file almetal_stage_timing_test.mm
 * @brief Native stage-counter output, retirement and failure tests.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */
#include "../almetalstagetiming.h"
#include "../almetalpipeline.h"
#include "../almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#import <Metal/Metal.h>
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <utility>

// Requires root's opt-in normal ALMetalRenderEncoder::begin scope hook.
// The same public begin signature is used with and without a caller scope.
namespace
{
constexpr std::uint32_t side = 64;
constexpr std::size_t bytes = side * side * 4;
const std::array<float, 4> red{1,0,0,1}, blue{0,0,1,1};
const char* shader_source = R"metal(
#include <metal_stdlib>
using namespace metal;
struct V { float4 position [[position]]; };
vertex V stage_test_vertex(uint id [[vertex_id]]) {
    const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
    V v; v.position=float4(p[id],.375,1); return v;
}
fragment float4 stage_test_rgba(constant float4& c [[buffer(0)]]) { return c; }
fragment float stage_test_r32(V v [[stage_in]]) { return v.position.z; }
)metal";

struct Snapshot
{
    std::unique_ptr<ALMetalBuffer> color, depth;
};

struct Fixture
{
    std::string error;
    bool r32;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;
    std::unique_ptr<ALMetalDepthStencilState> depth;
    std::unique_ptr<ALMetalRenderTarget> target;
    std::unique_ptr<ALMetalRenderPass> pass;

    explicit Fixture(bool raw_depth) : r32(raw_depth)
    {
        device = ALMetalDevice::create(&error); REQUIRE(device);
        library = ALMetalShaderLibrary::compileSource(*device, shader_source,
            "Stage counter wrapper oracle", &error); REQUIRE(library);
        ALMetalRenderPipelineDescriptor pd;
        pd.library=library.get(); pd.vertex_function="stage_test_vertex";
        pd.fragment_function=r32?"stage_test_r32":"stage_test_rgba";
        pd.colors[0].format=r32?ALMetalPixelFormat::r32_float:ALMetalPixelFormat::rgba8_unorm;
        pd.depth_stencil_format=r32?ALMetalPixelFormat::depth32_float:ALMetalPixelFormat::invalid;
        pd.label="Exact counted and uncounted stage oracle pipeline";
        pipeline=ALMetalRenderPipeline::create(*device,pd,&error); REQUIRE(pipeline);
        ALMetalDepthStencilDescriptor dd;
        dd.depth_compare=ALMetalCompareFunction::always; dd.depth_write_enabled=r32;
        dd.label="Stage oracle depth state";
        depth=ALMetalDepthStencilState::create(*device,dd,&error); REQUIRE(depth);
        ALMetalRenderTargetDescriptor td;
        td.width=td.height=side; td.colors[0].format=pd.colors[0].format;
        td.depth_stencil_format=pd.depth_stencil_format; td.label="Stage oracle attachments";
        target=ALMetalRenderTarget::create(*device,td,&error); REQUIRE(target);
        ALMetalRenderPassConfig pc;
        pc.colors[0].load=ALMetalLoadAction::clear; pc.colors[0].store=ALMetalStoreAction::store;
        pc.colors[0].clear={.125,.25,.5,0}; pc.depth_load=ALMetalLoadAction::clear;
        pc.depth_store=ALMetalStoreAction::store; pc.clear_depth=1;
        pass=target->makeRenderPass(pc,&error); REQUIRE(pass);
    }

    void draw(ALMetalCommandBuffer& command, const std::array<float,4>& color,
              const std::string& label)
    {
        auto encoder=ALMetalRenderEncoder::begin(command,*pass,label,&error); REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline,&error));
        REQUIRE(encoder->setDepthStencilState(depth.get(),&error));
        REQUIRE(encoder->setViewport({0,0,side,side,0,1},&error));
        REQUIRE(encoder->setScissor({0,0,side,side},&error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&error));
        if (!r32) REQUIRE(encoder->setFragmentBytes(0,color.data(),sizeof(color),&error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,&error));
        REQUIRE(encoder->end(&error));
    }

    std::unique_ptr<ALMetalBuffer> snapshotTexture(ALMetalCommandBuffer& command,
                                                const ALMetalTexture& texture)
    {
        ALMetalBufferDescriptor bd;
        bd.length=bytes; bd.storage=ALMetalStorageMode::shared; bd.label="Stage oracle raw snapshot";
        auto output=ALMetalBuffer::create(*device,bd,&error); REQUIRE(output);
        id<MTLCommandBuffer> native=(__bridge id<MTLCommandBuffer>)command.nativeHandle();
        auto blit=[native blitCommandEncoder]; REQUIRE(blit);
        [blit copyFromTexture:(__bridge id<MTLTexture>)texture.nativeHandle()
            sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(side,side,1)
            toBuffer:(__bridge id<MTLBuffer>)output->nativeHandle() destinationOffset:0
            destinationBytesPerRow:side*4 destinationBytesPerImage:bytes];
        [blit endEncoding]; return output;
    }

    Snapshot snapshot(ALMetalCommandBuffer& command)
    {
        Snapshot result; result.color=snapshotTexture(command,*target->colorTexture(0));
        if (r32) result.depth=snapshotTexture(command,*target->depthStencilTexture());
        return result;
    }

    void checkPixels(const ALMetalBuffer& buffer, bool is_blue)
    {
        std::array<std::uint8_t,bytes> data{};
        REQUIRE(buffer.read(0,data.data(),data.size(),&error));
        if (r32)
        {
            float expected=.375f;
            for (std::size_t i=0;i<side*side;++i)
                CHECK(std::memcmp(data.data()+4*i,&expected,4)==0);
        }
        else
        {
            const std::array<std::uint8_t,4> expected=is_blue
                ?std::array<std::uint8_t,4>{0,0,255,255}:std::array<std::uint8_t,4>{255,0,0,255};
            for (std::size_t i=0;i<side*side;++i)
                CHECK(std::memcmp(data.data()+4*i,expected.data(),4)==0);
        }
    }
};

struct ReleaseGate
{
    id<MTLSharedEvent> event = nil;
    struct State
    {
        enum : unsigned { active, disarmed, tripped };
        std::atomic<unsigned> phase{active};
    };
    std::shared_ptr<State> state=std::make_shared<State>();
    explicit ReleaseGate(id<MTLSharedEvent> value) : event(value)
    {
        // Test-only deadlock guard. An accidental GPU wait in poll/destruction
        // must fail the assertion below rather than hang before RAII unwinds.
        const auto flags=state;
        id<MTLSharedEvent> strong_event=event;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, static_cast<std::int64_t>(5*NSEC_PER_SEC)),
            dispatch_get_global_queue(QOS_CLASS_DEFAULT,0), ^{
                unsigned expected=State::active;
                if (flags->phase.compare_exchange_strong(expected,State::tripped,std::memory_order_acq_rel))
                    strong_event.signaledValue=1;
            });
    }
    void release() noexcept
    {
        unsigned expected=State::active;
        (void)state->phase.compare_exchange_strong(expected,State::disarmed,std::memory_order_acq_rel);
        event.signaledValue=1;
    }
    bool tripped() const noexcept { return state->phase.load(std::memory_order_acquire)==State::tripped; }
    ~ReleaseGate() { release(); }
};
}

TEST_CASE("almetal: stage window preserves normal wrapper RGBA and raw depth passes",
          "[almetal][stage-timing]")
{
    @autoreleasepool
    {
        REQUIRE(ALMetalStageTimingScope::current()==nullptr);
        for (bool r32:{false,true})
        {
            CAPTURE(r32); Fixture f(r32);
            auto reference=f.device->makeCommandBuffer("Two uncounted wrapper passes",&f.error); REQUIRE(reference);
            f.draw(*reference,red,"Stage first"); auto first=f.snapshot(*reference);
            f.draw(*reference,blue,"Stage second"); auto second=f.snapshot(*reference);
            REQUIRE(reference->commitAndWait(&f.error));
            f.checkPixels(*first.color,false); f.checkPixels(*second.color,true);
            if (r32) { f.checkPixels(*first.depth,false); f.checkPixels(*second.depth,true); }

            const auto mach_start=mach_absolute_time();
            auto window=ALMetalStageTimingWindow::create(*f.device,2,&f.error); REQUIRE(window);
            const auto mach_end=mach_absolute_time();
            auto counted=f.device->makeCommandBuffer("Two counted wrapper passes",&f.error); REQUIRE(counted);
            id<MTLDevice> native_device=(__bridge id<MTLDevice>)f.device->nativeHandle();
            ReleaseGate release{[native_device newSharedEvent]}; REQUIRE(release.event);
            [(__bridge id<MTLCommandBuffer>)counted->nativeHandle() encodeWaitForEvent:release.event value:1];
            Snapshot a,b;
            {
                ALMetalStageTimingScope scope(window.get(),101,2,3);
                CHECK(ALMetalStageTimingScope::current()==&scope);
                { ALMetalStageTimingScope disabled(nullptr,102,4,-1);
                  CHECK(ALMetalStageTimingScope::current()->window()==nullptr); }
                CHECK(ALMetalStageTimingScope::current()==&scope);
                f.draw(*counted,red,"Stage first"); a=f.snapshot(*counted);
                f.draw(*counted,blue,"Stage second"); b=f.snapshot(*counted);
            }
            CHECK(ALMetalStageTimingScope::current()==nullptr);
            REQUIRE(window->preparedPassCount()==2);
            REQUIRE(counted->commit(&f.error)); REQUIRE(window->seal(&f.error));
            const auto commits=ALMetalDevice::committedCommandBufferCount();
            std::vector<ALMetalStageTimingResult> ready;
            REQUIRE(window->poll(ready,&f.error));
            CHECK_FALSE(release.tripped());
            CHECK(ready.empty()); CHECK(window->pendingPassCount()==2); CHECK_FALSE(window->isFinished());
            CHECK(ALMetalDevice::committedCommandBufferCount()==commits);
            release.release(); REQUIRE(counted->waitUntilCompleted(&f.error));
            const auto mach_poll_start=mach_absolute_time();
            REQUIRE(window->poll(ready,&f.error));
            const auto mach_poll_end=mach_absolute_time();
            REQUIRE(ready.size()==2); CHECK(window->isFinished());
            mach_timebase_info_data_t timebase{}; REQUIRE(mach_timebase_info(&timebase)==KERN_SUCCESS);
            const auto ns=[&](std::uint64_t raw) { return double(raw)*timebase.numer/timebase.denom; };
            for (std::size_t i=0;i<ready.size();++i)
            {
                const auto& result=ready[i]; CAPTURE(i);
                CHECK(result.status==ALMetalStageTimingStatus::complete);
                REQUIRE(result.elapsed); REQUIRE(result.raw_samples); REQUIRE(result.calibration);
                CHECK(result.pass_index==i); CHECK(result.first_sample==i*4);
                CHECK(result.tag.frame==101); CHECK(result.tag.view_kind==2); CHECK(result.tag.face==3);
                CHECK(result.tag.label==(i==0?"Stage first":"Stage second"));
                CHECK(result.elapsed->vertex_ns>0); CHECK(result.elapsed->fragment_ns>0);
                CHECK((*result.raw_samples)[1]>(*result.raw_samples)[0]);
                CHECK((*result.raw_samples)[3]>(*result.raw_samples)[2]);
                CHECK(double(result.calibration->cpu_before_ns)>=ns(mach_start));
                CHECK(double(result.calibration->cpu_before_ns)<=ns(mach_end));
                CHECK(double(result.calibration->cpu_after_ns)>=ns(mach_poll_start));
                CHECK(double(result.calibration->cpu_after_ns)<=ns(mach_poll_end));
            }
            std::array<std::uint8_t,bytes> expected{},actual{};
            for (const auto pair:{std::pair{&first,&a},std::pair{&second,&b}})
            {
                REQUIRE(pair.first->color->read(0,expected.data(),bytes,&f.error));
                REQUIRE(pair.second->color->read(0,actual.data(),bytes,&f.error)); CHECK(actual==expected);
                if (r32)
                {
                    REQUIRE(pair.first->depth->read(0,expected.data(),bytes,&f.error));
                    REQUIRE(pair.second->depth->read(0,actual.data(),bytes,&f.error)); CHECK(actual==expected);
                }
            }
            REQUIRE(window->poll(ready,&f.error)); CHECK(ready.empty());
        }
    }
}

TEST_CASE("almetal: stage window keeps optional fallback and later committed owners safe",
          "[almetal][stage-timing]")
{
    @autoreleasepool
    {
        Fixture f(false);
        CHECK_FALSE(ALMetalStageTimingWindow::create(*f.device,0,&f.error));
        CHECK_FALSE(ALMetalStageTimingWindow::create(*f.device,257,&f.error));
        auto window=ALMetalStageTimingWindow::create(*f.device,1,&f.error); REQUIRE(window);
        auto later=f.device->makeCommandBuffer("Commit after diagnostic seal",&f.error); REQUIRE(later);
        auto other=f.device->makeCommandBuffer("Capacity fallback remains visible",&f.error); REQUIRE(other);
        ALMetalStageTimingTag tag{201,3,1,"Later committed prepared pass"};
        auto prepared=window->prepare(*later,*f.pass,tag,&f.error); REQUIRE(prepared);
        CHECK_FALSE(prepared->consume(*other,&f.error));
        auto* descriptor=(__bridge MTLRenderPassDescriptor*)prepared->consume(*later,&f.error); REQUIRE(descriptor);
        CHECK_FALSE(prepared->consume(*later,&f.error));
        __weak id<MTLCounterSampleBuffer> weak_buffer=descriptor.sampleBufferAttachments[0].sampleBuffer;
        REQUIRE(weak_buffer!=nil);
        auto native=[(__bridge id<MTLCommandBuffer>)later->nativeHandle() renderCommandEncoderWithDescriptor:descriptor];
        REQUIRE(native);
        [native setRenderPipelineState:(__bridge id<MTLRenderPipelineState>)f.pipeline->nativeHandle()];
        [native setCullMode:MTLCullModeNone]; [native setViewport:MTLViewport{0,0,side,side,0,1}];
        [native setFragmentBytes:red.data() length:sizeof(red) atIndex:0];
        [native drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [native endEncoding];
        prepared.reset(); descriptor=nil; native=nil;
        {
            ALMetalStageTimingScope scope(window.get(),202,1,-1);
            // Admission is full: root's actual wrapper hook must use the
            // original descriptor and preserve this blue draw.
            f.draw(*other,blue,"Fallback blue");
        }
        REQUIRE(other->commitAndWait(&f.error));
        std::array<std::uint8_t,bytes> blue_pixels{};
        REQUIRE(f.target->colorTexture(0)->readback(*f.device,0,0,{},blue_pixels.data(),side*4,&f.error));
        CHECK(blue_pixels[0]==0); CHECK(blue_pixels[2]==255);
        CHECK(window->rejectedPrepareCount()>=1);
        REQUIRE(window->seal(&f.error));
        std::vector<ALMetalStageTimingResult> ready; REQUIRE(window->poll(ready,&f.error));
        REQUIRE(ready.size()==1); CHECK(ready[0].status==ALMetalStageTimingStatus::unsubmitted_at_seal);
        CHECK_FALSE(ready[0].elapsed); CHECK_FALSE(ready[0].raw_samples); CHECK(window->isFinished());
        window.reset(); CHECK(weak_buffer!=nil); // Buffer-only owner belongs to later, not the window.
        REQUIRE(later->commitAndWait(&f.error));
        std::array<std::uint8_t,bytes> red_pixels{};
        REQUIRE(f.target->colorTexture(0)->readback(*f.device,0,0,{},red_pixels.data(),side*4,&f.error));
        for (std::size_t i=0;i<side*side;++i)
        { CHECK(red_pixels[4*i]==255); CHECK(red_pixels[4*i+1]==0); CHECK(red_pixels[4*i+2]==0); CHECK(red_pixels[4*i+3]==255); }

        // Dropping an unsubmitted wrapper is also unavailable-at-seal, without
        // decoding its unwritten bytes or changing the completed red image.
        auto cancelled_window=ALMetalStageTimingWindow::create(*f.device,1,&f.error); REQUIRE(cancelled_window);
        const auto cancel_commits=ALMetalDevice::committedCommandBufferCount();
        {
            auto cancelled=f.device->makeCommandBuffer("Drop counter-attached unsubmitted command",&f.error); REQUIRE(cancelled);
            ALMetalStageTimingScope scope(cancelled_window.get(),203,2,0);
            f.draw(*cancelled,blue,"Unsubmitted blue");
        }
        REQUIRE(cancelled_window->seal(&f.error)); REQUIRE(cancelled_window->poll(ready,&f.error));
        REQUIRE(ready.size()==1); CHECK(ready[0].status==ALMetalStageTimingStatus::unsubmitted_at_seal);
        CHECK_FALSE(ready[0].raw_samples); CHECK_FALSE(ready[0].elapsed);
        CHECK(ALMetalDevice::committedCommandBufferCount()==cancel_commits);
        std::array<std::uint8_t,bytes> after_cancel{};
        REQUIRE(f.target->colorTexture(0)->readback(*f.device,0,0,{},after_cancel.data(),side*4,&f.error));
        CHECK(after_cancel==red_pixels);

        // Counter-encoder creation may fail AFTER consume. Explicit abandon
        // must retain original drawing and never decode unwritten/stale data.
        auto fallback_window=ALMetalStageTimingWindow::create(*f.device,1,&f.error); REQUIRE(fallback_window);
        auto fallback_command=f.device->makeCommandBuffer("Abandon consumed counter descriptor",&f.error); REQUIRE(fallback_command);
        auto failed_counter=fallback_window->prepare(*fallback_command,*f.pass,tag,&f.error); REQUIRE(failed_counter);
        REQUIRE(failed_counter->consume(*fallback_command,&f.error)!=nullptr);
        failed_counter->abandon();
        CHECK_FALSE(failed_counter->consume(*fallback_command,&f.error));
        {
            ALMetalStageTimingScope original_only(nullptr,205,1,-1);
            f.draw(*fallback_command,blue,"Original draw after counter admission failure");
        }
        REQUIRE(fallback_command->commitAndWait(&f.error)); REQUIRE(fallback_window->seal(&f.error));
        REQUIRE(fallback_window->poll(ready,&f.error)); REQUIRE(ready.size()==1);
        CHECK(ready[0].status==ALMetalStageTimingStatus::not_encoded);
        CHECK_FALSE(ready[0].raw_samples); CHECK_FALSE(ready[0].elapsed);
        std::array<std::uint8_t,bytes> original_after_abandon{};
        REQUIRE(f.target->colorTexture(0)->readback(*f.device,0,0,{},original_after_abandon.data(),side*4,&f.error));
        CHECK(original_after_abandon==blue_pixels);

        // Never-consumed descriptors must not decode unwritten sample bytes.
        auto unused_window=ALMetalStageTimingWindow::create(*f.device,1,&f.error); REQUIRE(unused_window);
        auto empty=f.device->makeCommandBuffer("Prepared but never encoded",&f.error); REQUIRE(empty);
        auto unused=unused_window->prepare(*empty,*f.pass,tag,&f.error); REQUIRE(unused);
        id<MTLDevice> unused_native_device=(__bridge id<MTLDevice>)f.device->nativeHandle();
        ReleaseGate unused_release{[unused_native_device newSharedEvent]}; REQUIRE(unused_release.event);
        [(__bridge id<MTLCommandBuffer>)empty->nativeHandle() encodeWaitForEvent:unused_release.event value:1];
        REQUIRE(empty->commit(&f.error)); REQUIRE(unused_window->seal(&f.error));
        REQUIRE(unused_window->poll(ready,&f.error)); CHECK(ready.empty());
        CHECK_FALSE(unused_release.tripped());
        CHECK_FALSE(unused_window->isFinished()); CHECK(unused_window->pendingPassCount()==1);
        unused_release.release(); REQUIRE(empty->waitUntilCompleted(&f.error));
        REQUIRE(unused_window->poll(ready,&f.error)); REQUIRE(ready.size()==1);
        CHECK(ready[0].status==ALMetalStageTimingStatus::not_encoded);
        CHECK_FALSE(ready[0].raw_samples); CHECK_FALSE(ready[0].elapsed);
        CHECK(unused_window->isFinished());

        // Window teardown while a committed command is still GPU-gated.
        auto teardown=ALMetalStageTimingWindow::create(*f.device,1,&f.error); REQUIRE(teardown);
        auto inflight=f.device->makeCommandBuffer("Stage window destroyed before GPU completion",&f.error); REQUIRE(inflight);
        id<MTLDevice> native_device=(__bridge id<MTLDevice>)f.device->nativeHandle();
        ReleaseGate release{[native_device newSharedEvent]}; REQUIRE(release.event);
        [(__bridge id<MTLCommandBuffer>)inflight->nativeHandle() encodeWaitForEvent:release.event value:1];
        {
            ALMetalStageTimingScope scope(teardown.get(),204,4,-1);
            f.draw(*inflight,blue,"Counter owner after window teardown");
        }
        REQUIRE(inflight->commit(&f.error)); REQUIRE(teardown->seal(&f.error));
        teardown.reset(); // Must not wait for the gated GPU or release needed storage.
        CHECK_FALSE(release.tripped());
        release.release(); REQUIRE(inflight->waitUntilCompleted(&f.error));
        std::array<std::uint8_t,bytes> after_teardown{};
        REQUIRE(f.target->colorTexture(0)->readback(*f.device,0,0,{},after_teardown.data(),side*4,&f.error));
        CHECK(after_teardown==blue_pixels);
    }
}
