/**
 * @file almetalshadowvisibility.mm
 * @brief Native GPU visibility/compaction with a Metal4 compute pilot.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "almetalshadowvisibility.h"
#include "almetalshadowvisibilitysource.h"
#import <Metal/Metal.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
void fail(std::string* error,const char* text) { if(error)*error=text; }
constexpr const char* names[]{"almetal_shadow_visibility_classify", "almetal_shadow_visibility_compact"};
struct Feedback
{
    std::atomic<std::uint64_t> failures{0},timed{0},nanoseconds{0};
};
}

struct ALMetalShadowVisibility::Batch::Impl
{
    std::array<id<MTLBuffer>,7> buffers{};
    std::vector<id<MTLBuffer>> referenced_buffers;
    id<MTLIndirectCommandBuffer> destination=nil;
    id<MTL4CommandAllocator> allocator=nil;
    id<MTL4CommandBuffer> work=nil;
    id<MTL4CommandQueue> queue=nil;
    std::array<id<MTLComputePipelineState>,2> pipelines{};
    id<MTL4ArgumentTable> table=nil;
    id<MTLResidencySet> residency=nil;
    id<MTLSharedEvent> event=nil;
    std::uint64_t event_value=0;
    std::shared_ptr<const ALMetalCommandCompletion> consumer;
    bool metal4=false;
    bool compute_submitted=false;
    bool complete() const noexcept
    {
        return (!compute_submitted || (event && event.signaledValue>=event_value)) &&
            (!consumer || consumer->isComplete());
    }
};
struct ALMetalShadowVisibility::Impl
{
    id<MTLDevice> device=nil;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::array<id<MTLComputePipelineState>,2> pipelines{}, metal4_pipelines{};
    id<MTLArgumentEncoder> icb_encoder=nil;
    id<MTL4CommandQueue> queue=nil;
    MTLSharedEventListener* listener=nil;
    std::vector<std::shared_ptr<Batch::Impl>> slots;
    std::shared_ptr<Feedback> feedback=std::make_shared<Feedback>();
};
ALMetalShadowVisibility::Batch::Batch(std::shared_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalShadowVisibility::Batch::~Batch()=default;
bool ALMetalShadowVisibility::Batch::computeComplete()const noexcept
{ return !mImpl->compute_submitted || (mImpl->event && mImpl->event.signaledValue>=mImpl->event_value); }
bool ALMetalShadowVisibility::Batch::usedMetal4()const noexcept { return mImpl->metal4; }
void* ALMetalShadowVisibility::Batch::executionRanges()const noexcept
{ return (__bridge void*)mImpl->buffers[4]; }
ALMetalShadowVisibility::ALMetalShadowVisibility(std::unique_ptr<Impl> impl):mImpl(std::move(impl)){}
ALMetalShadowVisibility::~ALMetalShadowVisibility()=default;
bool ALMetalShadowVisibility::metal4Supported()const noexcept { return mImpl->queue!=nil; }
std::uint64_t ALMetalShadowVisibility::failures()const noexcept
{return mImpl->feedback->failures.load(std::memory_order_relaxed);}
std::uint64_t ALMetalShadowVisibility::metal4TimedDispatches()const noexcept
{return mImpl->feedback->timed.load(std::memory_order_acquire);}
std::uint64_t ALMetalShadowVisibility::metal4GPUTimeNanoseconds()const noexcept
{return mImpl->feedback->nanoseconds.load(std::memory_order_relaxed);}

std::unique_ptr<ALMetalShadowVisibility> ALMetalShadowVisibility::create(
    const ALMetalDevice& device,std::string* error)
{
    auto impl=std::make_unique<Impl>();
    impl->device=(__bridge id<MTLDevice>)device.nativeHandle();
    impl->library=ALMetalShaderLibrary::compileSource(device,alMetalShadowVisibilitySource,
        "Alchemy ordered shadow visibility",error);
    if(!impl->library)return nullptr;
    id<MTLLibrary> library=(__bridge id<MTLLibrary>)impl->library->nativeHandle();
    std::array<id<MTLFunction>,2> functions;
    for(std::size_t i=0;i<2;++i)
    {
        functions[i]=[library newFunctionWithName:[NSString stringWithUTF8String:names[i]]];
        impl->pipelines[i]=[impl->device newComputePipelineStateWithFunction:functions[i] error:nil];
        if(!impl->pipelines[i]) { fail(error,"Could not compile native shadow visibility"); return nullptr; }
    }
    impl->icb_encoder=[functions[1] newArgumentEncoderWithBufferIndex:0];
    if(!impl->icb_encoder) { fail(error,"Could not create visibility ICB argument encoder"); return nullptr; }
    if(impl->pipelines[1].maxTotalThreadsPerThreadgroup<256)
    {fail(error,"Ordered shadow command generation requires 256-thread groups");return nullptr;}
    if([impl->device supportsFamily:MTLGPUFamilyMetal4])
    {
        auto compiler=[impl->device newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:nil];
        if(compiler)
        {
            for(std::size_t i=0;i<2;++i)
            {
                auto function=[MTL4LibraryFunctionDescriptor new];
                function.library=library; function.name=[NSString stringWithUTF8String:names[i]];
                auto descriptor=[MTL4ComputePipelineDescriptor new];
                descriptor.computeFunctionDescriptor=function;
                impl->metal4_pipelines[i]=[compiler newComputePipelineStateWithDescriptor:descriptor compilerTaskOptions:nil error:nil];
            }
            if(impl->metal4_pipelines[0] && impl->metal4_pipelines[1] &&
               impl->metal4_pipelines[1].maxTotalThreadsPerThreadgroup>=256)
            {
                impl->queue=[impl->device newMTL4CommandQueue];
                impl->listener=[MTLSharedEventListener new];
            }
        }
    }
    return std::unique_ptr<ALMetalShadowVisibility>(new ALMetalShadowVisibility(std::move(impl)));
}

std::shared_ptr<ALMetalShadowVisibility::Batch> ALMetalShadowVisibility::encode(
    ALMetalCommandBuffer& command,void* destination_icb,
    std::span<const ALMetalShadowVisibilityInput> inputs,
    std::span<const ALMetalShadowVisibilityRun> runs,
    std::span<void* const> native_buffers,
    std::span<const ALMetalShadowVisibilityDraw> draws,
    bool request_metal4,std::string* error)
{
    if(command.isCommitted() || command.nativeDevice()!=(__bridge void*)mImpl->device ||
       !destination_icb || inputs.empty() || inputs.size()>16384 || runs.empty() ||
       draws.size()!=inputs.size())
    { fail(error,"Shadow visibility input is invalid"); return nullptr; }
    std::size_t next=0;
    for(const auto& run:runs)
    {
        if(run.first!=next || !run.count || run.count>inputs.size()-next)
        { fail(error,"Shadow visibility runs must partition the command list in order"); return nullptr; }
        next+=run.count;
    }
    if(next!=inputs.size()) { fail(error,"Shadow visibility runs are incomplete"); return nullptr; }
    id<MTLIndirectCommandBuffer> destination=(__bridge id<MTLIndirectCommandBuffer>)destination_icb;
    if(destination.device!=mImpl->device || destination.size<inputs.size())
    { fail(error,"Shadow visibility requires a compatible output command table"); return nullptr; }
    std::shared_ptr<Batch::Impl> slot;
    for(const auto& candidate:mImpl->slots)
        if(candidate.use_count()==1 && candidate->complete()) { slot=candidate;break; }
    if(!slot) { slot=std::make_shared<Batch::Impl>();mImpl->slots.push_back(slot); }
    slot->compute_submitted=false;
    slot->metal4=request_metal4 && metal4Supported();
    slot->destination=destination;
    slot->consumer=command.completionToken();
    if(!slot->consumer) {fail(error,"Shadow visibility requires command completion");return nullptr;}
    const std::array<std::size_t,7> sizes{mImpl->icb_encoder.encodedLength,
        inputs.size_bytes(),inputs.size()*4,runs.size_bytes(),runs.size()*8,draws.size_bytes(),8};
    for(std::size_t i=0;i<7;++i)
    {
        if(!slot->buffers[i] || slot->buffers[i].length<sizes[i])
            slot->buffers[i]=[mImpl->device newBufferWithLength:sizes[i] options:MTLResourceStorageModeShared];
        if(!slot->buffers[i]) {fail(error,"Could not allocate shadow visibility data");return nullptr;}
    }
    std::memcpy(slot->buffers[1].contents,inputs.data(),inputs.size_bytes());
    std::memcpy(slot->buffers[3].contents,runs.data(),runs.size_bytes());
    std::memcpy(slot->buffers[5].contents,draws.data(),draws.size_bytes());
    const std::array<std::uint32_t,2> counts{std::uint32_t(inputs.size()),std::uint32_t(runs.size())};
    std::memcpy(slot->buffers[6].contents,counts.data(),sizeof(counts));
    [mImpl->icb_encoder setArgumentBuffer:slot->buffers[0] offset:0];
    [mImpl->icb_encoder setIndirectCommandBuffer:destination atIndex:0];
    slot->referenced_buffers.clear();
    for(void* handle:native_buffers) slot->referenced_buffers.push_back((__bridge id<MTLBuffer>)handle);
    if(!command.retainUntilCompletion(slot,error))return nullptr;
    id<MTLCommandBuffer> consumer=(__bridge id<MTLCommandBuffer>)command.nativeHandle();
    if(slot->metal4)
    {
        if(!slot->allocator)slot->allocator=[mImpl->device newCommandAllocator];
        else [slot->allocator reset];
        if(!slot->event)slot->event=[mImpl->device newSharedEvent];
        if(!slot->table)
        {
            auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=7;
            slot->table=[mImpl->device newArgumentTableWithDescriptor:descriptor error:nil];
        }
        if(!slot->residency)
            slot->residency=[mImpl->device newResidencySetWithDescriptor:[MTLResidencySetDescriptor new] error:nil];
        if(!slot->allocator || !slot->event || !slot->table || !slot->residency)
        {fail(error,"Could not allocate Metal4 visibility ownership");return nullptr;}
        [slot->residency removeAllAllocations];
        [slot->residency addAllocation:destination];
        for(std::size_t i=0;i<7;++i)
        {
            [slot->residency addAllocation:slot->buffers[i]];
            [slot->table setAddress:slot->buffers[i].gpuAddress atIndex:i];
        }
        for(id<MTLBuffer> buffer:slot->referenced_buffers)[slot->residency addAllocation:buffer];
        [slot->residency commit];
        auto work=[mImpl->device newCommandBuffer];
        if(!work) {fail(error,"Could not create Metal4 visibility command");return nullptr;}
        slot->work=work;slot->queue=mImpl->queue;slot->pipelines=mImpl->metal4_pipelines;
        [work beginCommandBufferWithAllocator:slot->allocator];[work useResidencySet:slot->residency];
        for(std::size_t phase=0;phase<2;++phase)
        {
            auto encoder=[work computeCommandEncoder];
            if(!encoder) { [work endCommandBuffer]; fail(error,"Could not encode Metal4 visibility");return nullptr; }
            if(phase)[encoder barrierAfterQueueStages:MTLStageDispatch beforeStages:MTLStageDispatch visibilityOptions:MTL4VisibilityOptionDevice];
            [encoder setComputePipelineState:mImpl->metal4_pipelines[phase]];
            [encoder setArgumentTable:slot->table];
            if(phase)[encoder dispatchThreadgroups:MTLSizeMake(runs.size(),1,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
            else [encoder dispatchThreads:MTLSizeMake(inputs.size(),1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
            [encoder endEncoding];
        }
        [work endCommandBuffer];
        const auto value=++slot->event_value;
        // Bounds/address records are immutable CPU publications: no dependency on an
        // uncommitted legacy command is introduced. Cancellation cannot strand
        // this queue waiting for a signal that will never be submitted.
        const auto feedback_state=mImpl->feedback;
        auto options=[MTL4CommitOptions new];
        [options addFeedbackHandler:^(id<MTL4CommitFeedback> feedback){
            if(feedback.error)
            {
                feedback_state->failures.fetch_add(1,std::memory_order_relaxed);
                NSLog(@"Alchemy Metal4 shadow visibility failed: %@",feedback.error);
            }
            else
            {
                const double start=feedback.GPUStartTime,end=feedback.GPUEndTime;
                const double nanoseconds=(end-start)*1.e9;
                if(start>0. && end>start && std::isfinite(nanoseconds) &&
                   nanoseconds<double(std::numeric_limits<std::uint64_t>::max()))
                {
                    feedback_state->nanoseconds.fetch_add(std::uint64_t(nanoseconds),std::memory_order_relaxed);
                    feedback_state->timed.fetch_add(1,std::memory_order_release);
                }
            }
        }];
        slot->compute_submitted=true;
        [mImpl->queue commit:&work count:1 options:options];[mImpl->queue signalEvent:slot->event value:value];
        [consumer encodeWaitForEvent:slot->event value:value];
        const auto keepalive=slot;
        [slot->event notifyListener:mImpl->listener atValue:value block:^(id<MTLSharedEvent>,uint64_t){(void)keepalive;}];
    }
    else
    {
        for(std::size_t phase=0;phase<2;++phase)
        {
            auto encoder=[consumer computeCommandEncoder];
            if(!encoder) {fail(error,"Could not encode shadow visibility");return nullptr;}
            [encoder setComputePipelineState:mImpl->pipelines[phase]];
            for(std::size_t i=0;i<7;++i)[encoder setBuffer:slot->buffers[i] offset:0 atIndex:i];
            [encoder useResource:destination usage:MTLResourceUsageWrite];
            if(phase)[encoder dispatchThreadgroups:MTLSizeMake(runs.size(),1,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
            else [encoder dispatchThreads:MTLSizeMake(inputs.size(),1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
            [encoder endEncoding];
        }
    }
    return std::shared_ptr<Batch>(new Batch(std::move(slot)));
}
