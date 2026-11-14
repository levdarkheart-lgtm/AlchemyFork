/**
 * @file almetalshadowindirect.mm
 * @brief Pooled, completion-retired static opaque shadow ICB commands.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "almetalshadowindirect.h"

#import <Metal/Metal.h>

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace
{
    void setError(std::string* error, const char* message)
    {
        if (error) *error = message;
    }

    bool fits(std::size_t offset, std::size_t count, std::size_t stride,
              std::size_t length)
    {
        return offset <= length && count <= (length - offset) / stride;
    }

    // Reusable flat identity set: no per-resource heap allocations. Native
    // identity is safe only while its owner retains the native resources.
    struct IdentitySet
    {
        std::vector<void*> buckets;
        std::vector<std::uint64_t> addresses;
        void reset(std::size_t maximum_entries)
        {
            std::size_t capacity = 8;
            while (capacity < maximum_entries * 2) capacity *= 2;
            if (buckets.size() < capacity) buckets.resize(capacity);
            std::fill(buckets.begin(), buckets.end(), nullptr);
        }
        bool insert(void* handle)
        {
            const std::size_t mask = buckets.size() - 1;
            std::size_t index = ((reinterpret_cast<std::uintptr_t>(handle) >> 4) *
                std::uintptr_t(0x9e3779b97f4a7c15ull)) & mask;
            while (buckets[index] && buckets[index] != handle)
                index = (index + 1) & mask;
            if (buckets[index]) return false;
            buckets[index] = handle;
            if (index < addresses.size()) addresses[index] = 0;
            return true;
        }
        std::uint64_t gpuAddress(void* handle)
        {
            if (addresses.size() < buckets.size()) addresses.resize(buckets.size());
            const std::size_t mask = buckets.size() - 1;
            std::size_t index = ((reinterpret_cast<std::uintptr_t>(handle) >> 4) *
                std::uintptr_t(0x9e3779b97f4a7c15ull)) & mask;
            while (buckets[index] && buckets[index] != handle) index = (index + 1) & mask;
            if (!buckets[index])
            { buckets[index] = handle; addresses[index] = ((__bridge id<MTLBuffer>)handle).gpuAddress; }
            else if (!addresses[index]) addresses[index] = ((__bridge id<MTLBuffer>)handle).gpuAddress;
            return addresses[index];
        }
    };

    struct NativeDraw
    {
        void* positions;
        void* constants;
        void* indices;
        std::size_t position_offset;
        std::size_t constant_offset;
        std::size_t index_offset;
        std::uint32_t index_count;
        MTLIndexType index_type;
        std::array<void*, 7> extra_buffers{};
        std::array<std::size_t, 7> extra_offsets{};
        std::uint32_t binding_mask = 0;
    };

    struct CachedCommand
    {
        id<MTLIndirectRenderCommand> command = nil;
        // Keep old identities alive until the matching native binding/draw
        // has been overwritten. A wrapper pointer or stale void* is unsafe:
        // Objective-C may recycle the released native resource's address.
        id<MTLBuffer> positions = nil;
        id<MTLBuffer> constants = nil;
        id<MTLBuffer> indices = nil;
        std::size_t position_offset = 0;
        std::size_t constant_offset = 0;
        std::size_t index_offset = 0;
        std::uint32_t index_count = 0;
        MTLIndexType index_type = MTLIndexTypeUInt16;
        bool draw_set = false;
        std::array<id<MTLBuffer>, 7> extra_buffers{};
        std::array<std::size_t, 7> extra_offsets{};
        std::uint32_t binding_mask = 0;
    };

    struct Slot
    {
        id<MTLIndirectCommandBuffer> buffer = nil;
        id<MTLIndirectCommandBuffer> visible_buffer = nil;
        std::shared_ptr<ALMetalShadowVisibility::Batch> visibility;
        std::vector<NativeDraw> generated_draws;
        std::vector<id<MTLBuffer>> generated_resources;
        bool generated = false;
        std::size_t capacity = 0;
        std::vector<CachedCommand> commands;
        std::vector<void*> resources;
        IdentitySet resource_set;
        std::shared_ptr<const ALMetalCommandCompletion> completion;
        std::size_t previous_count = 0;
        bool leased = false;
        bool used = false;
    };

    struct DirectSlot
    {
        std::vector<NativeDraw> draws;
        // One strong owner per unique native buffer, rather than three ARC
        // owners per draw. The packet array itself contains non-owning IDs.
        std::vector<id<MTLBuffer>> resources;
        std::shared_ptr<const ALMetalCommandCompletion> completion;
        bool leased = false;
        bool used = false;
    };

    struct Pool
    {
        const ALMetalDevice* device_wrapper = nullptr;
        id<MTLDevice> device = nil;
        bool supported = false;
        bool fail_next_wrapper_allocation_for_testing = false;
        std::vector<std::shared_ptr<Slot>> slots;
        std::vector<std::shared_ptr<DirectSlot>> direct_slots;
        std::vector<NativeDraw> scratch;
        IdentitySet validated_resources;
        IdentitySet gpu_addresses;
        std::vector<void*> validated_handles;
        ALMetalShadowIndirectStats statistics;
        std::unique_ptr<ALMetalShadowVisibility> visibility;
    };

    bool validateDraws(Pool& pool,
                       std::span<const ALMetalShadowIndirectDraw> draws,
                       std::string* error)
    {
        // Validate the complete list before mutating a native command or
        // leased packet array. Keep scratch capacity rounded for slot swaps.
        pool.scratch.clear();
        if (!draws.empty())
        {
            std::size_t capacity = 128;
            while (capacity < draws.size()) capacity *= 2;
            pool.scratch.reserve(capacity);
        }
        pool.validated_resources.reset(draws.size() * 10);
        pool.validated_handles.clear();
        std::array<void*, 3> previous_handles{};
        for (const auto& draw : draws)
        {
            const bool index32 = draw.index_type == ALMetalIndexType::uint32;
            const std::size_t index_stride = index32 ? 4 : 2;
            if (!draw.positions || !draw.indices || !draw.constants ||
                (!index32 && draw.index_type != ALMetalIndexType::uint16) ||
                draw.vertex_count == 0 || draw.index_count == 0 ||
                draw.position_offset % 16 != 0 ||
                draw.constant_offset % 16 != 0 ||
                draw.index_offset % index_stride != 0 ||
                !fits(draw.position_offset, draw.vertex_count, 16, draw.positions->length()) ||
                !fits(draw.index_offset, draw.index_count, index_stride, draw.indices->length()) ||
                draw.constant_length < ALMetalShadowIndirectCommands::constant_size ||
                !fits(draw.constant_offset, draw.constant_length,
                      1, draw.constants->length()))
            {
                setError(error, "Metal shadow draw range is invalid");
                return false;
            }
            void* positions = draw.positions->nativeHandle();
            void* constants = draw.constants->nativeHandle();
            void* indices = draw.indices->nativeHandle();
            const std::array handles{positions, constants, indices};
            for (std::size_t role = 0; role < handles.size(); ++role)
            {
                void* handle = handles[role];
                if (!handle)
                {
                    setError(error, "Metal shadow draw buffer is unavailable");
                    return false;
                }
                // The constants slab is normally unchanged for every draw,
                // and adjacent faces often share position/index resources.
                // Their ranges are still checked above for every record.
                if (previous_handles[role] == handle) continue;
                previous_handles[role] = handle;
                if (pool.validated_resources.insert(handle))
                {
                    if (((__bridge id<MTLBuffer>)handle).device != pool.device)
                    {
                        setError(error, "Metal shadow buffer belongs to another device");
                        return false;
                    }
                    pool.validated_handles.push_back(handle);
                }
            }
            NativeDraw prepared{positions, constants, indices,
                draw.position_offset, draw.constant_offset, draw.index_offset,
                draw.index_count, index32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16};
            for (std::size_t binding = 0; binding < 7; ++binding)
            {
                const auto& range = binding < 5 ? draw.vertex_bindings[binding]
                                               : draw.fragment_bindings[binding - 5];
                if (!range.buffer)
                {
                    if (range.offset || range.length)
                    {
                        setError(error, "Metal shadow optional buffer range is incomplete");
                        return false;
                    }
                    continue;
                }
                if (!range.length || range.offset % 4 != 0 ||
                    !fits(range.offset, range.length, 1, range.buffer->length()))
                {
                    setError(error, "Metal shadow optional buffer range is invalid");
                    return false;
                }
                void* handle = range.buffer->nativeHandle();
                if (!handle)
                {
                    setError(error, "Metal shadow optional buffer is unavailable");
                    return false;
                }
                prepared.extra_buffers[binding] = handle;
                prepared.extra_offsets[binding] = range.offset;
                prepared.binding_mask |= 1u << binding;
                if (pool.validated_resources.insert(handle))
                {
                    if (((__bridge id<MTLBuffer>)handle).device != pool.device)
                    {
                        setError(error, "Metal shadow optional buffer belongs to another device");
                        return false;
                    }
                    pool.validated_handles.push_back(handle);
                }
            }
            pool.scratch.push_back(std::move(prepared));
        }
        return true;
    }

    std::shared_ptr<Slot> makeSlot(Pool& pool, std::size_t count,
                                   std::string* error, bool generated = false)
    {
        std::size_t capacity = 128;
        while (capacity < count) capacity *= 2;
        auto descriptor = [MTLIndirectCommandBufferDescriptor new];
        descriptor.commandTypes = MTLIndirectCommandTypeDrawIndexed;
        descriptor.inheritPipelineState = YES;
        descriptor.inheritBuffers = NO;
        descriptor.maxVertexBufferBindCount = 7;
        descriptor.maxFragmentBufferBindCount = 2;
        auto slot = std::make_shared<Slot>();
        slot->capacity = capacity;
        slot->generated = generated;
        if (!generated)
        {
        slot->buffer = [pool.device newIndirectCommandBufferWithDescriptor:descriptor
            maxCommandCount:capacity options:MTLResourceStorageModeShared];
        if (!slot->buffer)
        {
            setError(error, "Metal could not allocate shadow indirect commands");
            return nullptr;
        }
        slot->buffer.label = @"Alchemy pooled opaque shadow indirect commands";
        slot->commands.resize(capacity);
        for (std::size_t i = 0; i < capacity; ++i)
        {
            slot->commands[i].command = [slot->buffer indirectRenderCommandAtIndex:i];
            if (!slot->commands[i].command)
            {
                setError(error, "Metal could not create a shadow indirect command proxy");
                return nullptr;
            }
        }
        slot->resources.reserve(capacity * 10);
        slot->resource_set.reset(capacity * 10);
        }
        ++pool.statistics.slot_allocations;
        pool.slots.push_back(slot);
        return slot;
    }
}

struct ALMetalShadowIndirectCommands::Impl
{
    std::shared_ptr<Pool> pool;
};

struct ALMetalShadowIndirectCommands::Batch::Impl
{
    std::shared_ptr<Pool> owner;
    std::shared_ptr<Slot> slot;
    std::shared_ptr<DirectSlot> direct_slot;
    id<MTLCommandBuffer> command = nil;
    // Strong ownership prevents a later encoder from recycling the address
    // used to suppress duplicate resource declarations in this transaction.
    id<MTLRenderCommandEncoder> declared_encoder = nil;
    std::size_t count = 0;
    bool indirect = true;
    std::vector<ALMetalShadowVisibilityRun> visibility_runs;
    ~Impl()
    {
        // The inner owner also covers failure of the final public Batch
        // allocation. A completion token still prevents premature GPU reuse.
        if (slot) slot->leased = false;
        if (direct_slot) direct_slot->leased = false;
    }
};

ALMetalShadowIndirectCommands::Batch::Batch(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}

ALMetalShadowIndirectCommands::Batch::~Batch() = default;

std::size_t ALMetalShadowIndirectCommands::Batch::drawCount() const noexcept
{
    return mImpl->count;
}

bool ALMetalShadowIndirectCommands::Batch::isIndirect() const noexcept
{
    return mImpl->indirect;
}

ALMetalShadowIndirectCommands::ALMetalShadowIndirectCommands(
    std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
ALMetalShadowIndirectCommands::~ALMetalShadowIndirectCommands() = default;

std::unique_ptr<ALMetalShadowIndirectCommands>
ALMetalShadowIndirectCommands::create(const ALMetalDevice& device,
                                       std::string* error)
{
    @autoreleasepool
    {
        if (!device.nativeHandle())
        {
            setError(error, "Metal shadow indirect commands require a device");
            return nullptr;
        }
        auto impl = std::make_unique<Impl>();
        impl->pool = std::make_shared<Pool>();
        impl->pool->device_wrapper = &device;
        impl->pool->device = (__bridge id<MTLDevice>)device.nativeHandle();
        // M1 and subsequent Apple Silicon GPUs implement this capability;
        // allocation remains the runtime proof, not an SDK availability test.
        if ([impl->pool->device supportsFamily:MTLGPUFamilyApple7])
        {
            impl->pool->supported = static_cast<bool>(makeSlot(*impl->pool, 1, error));
        }
        return std::unique_ptr<ALMetalShadowIndirectCommands>(
            new ALMetalShadowIndirectCommands(std::move(impl)));
    }
}

bool ALMetalShadowIndirectCommands::supported() const noexcept
{
    return mImpl->pool->supported;
}

void ALMetalShadowIndirectCommands::failNextWrapperAllocationForTesting() noexcept
{
    mImpl->pool->fail_next_wrapper_allocation_for_testing = true;
}

std::unique_ptr<ALMetalShadowIndirectCommands::Batch>
ALMetalShadowIndirectCommands::prepare(
    ALMetalCommandBuffer& command,
    std::span<const ALMetalShadowIndirectDraw> draws, std::string* error, bool gpu_generated,
    ALMetalShadowPrepareFailure* failure)
try
{
    if (failure) *failure = ALMetalShadowPrepareFailure::none;
    auto& pool = *mImpl->pool;
    if (!pool.supported || !command.nativeHandle() || command.isCommitted() ||
        command.nativeDevice() != (__bridge void*)pool.device ||
        draws.size() > maximum_draw_count)
    {
        if (failure) *failure = ALMetalShadowPrepareFailure::unavailable;
        setError(error, "Metal shadow indirect preparation is unavailable or invalid");
        return nullptr;
    }
    auto completion = command.completionToken();
    if (!completion)
    {
        if (failure) *failure = ALMetalShadowPrepareFailure::completion_tracking;
        setError(error, "Metal shadow indirect preparation requires completion tracking");
        return nullptr;
    }
    if (!validateDraws(pool, draws, error))
    {
        if (failure) *failure = ALMetalShadowPrepareFailure::validation;
        return nullptr;
    }
    auto batch_impl = std::make_unique<Batch::Impl>();
    batch_impl->owner = mImpl->pool;
    batch_impl->command = (__bridge id<MTLCommandBuffer>)command.nativeHandle();
    batch_impl->count = draws.size();
    std::uint64_t extended_count = 0;
    if (!draws.empty())
    {
        // useResources declares indirect access but does not retain buffers.
        // Keep the slot pool alive even if its renderer and public lease are
        // destroyed before submission/completion of the prepared command.
        if (!command.retainUntilCompletion(mImpl->pool, error))
        {
            if (failure) *failure = ALMetalShadowPrepareFailure::retention;
            return nullptr;
        }
        std::shared_ptr<Slot> slot;
        for (const auto& candidate : pool.slots)
        {
            if (!candidate->leased && candidate->generated == gpu_generated &&
                candidate->capacity >= draws.size() &&
                (!candidate->completion || candidate->completion->isComplete()) &&
                (!candidate->visibility || candidate->visibility->computeComplete()) &&
                (!slot || candidate->capacity < slot->capacity))
                slot = candidate;
        }
        if (!slot) slot = makeSlot(pool, draws.size(), error, gpu_generated);
        if (!slot)
        {
            if (failure) *failure = ALMetalShadowPrepareFailure::native_allocation;
            return nullptr;
        }
        if (slot->used) ++pool.statistics.slot_reuses;
        slot->visibility.reset();
        slot->used = true;
        if (!gpu_generated) slot->resource_set.reset(draws.size() * 10);
        slot->resources.clear();
        slot->generated = gpu_generated;
        if (gpu_generated)
        {
            // The immutable native record table is the GPU command source.
            // No CPU setVertexBuffer/setFragmentBuffer/draw emission occurs.
            slot->generated_draws.swap(pool.scratch);
            slot->resources = pool.validated_handles;
            slot->generated_resources.clear();
            slot->generated_resources.reserve(pool.validated_handles.size());
            for (void* handle : pool.validated_handles)
                slot->generated_resources.push_back((__bridge id<MTLBuffer>)handle);
        }
        else
        {
        slot->generated_draws.clear();
        slot->generated_resources.clear();
        for (std::size_t i = 0; i < draws.size(); ++i)
        {
            const auto& input = pool.scratch[i];
            auto& cached = slot->commands[i];
            id<MTLIndirectRenderCommand> __unsafe_unretained native = cached.command;
            if (cached.binding_mask != input.binding_mask)
            {
                [native reset];
                cached.positions = nil;
                cached.constants = nil;
                cached.indices = nil;
                cached.extra_buffers = {};
                cached.draw_set = false;
                cached.binding_mask = input.binding_mask;
            }
            id<MTLBuffer> __unsafe_unretained positions = (__bridge id<MTLBuffer>)input.positions;
            id<MTLBuffer> __unsafe_unretained constants = (__bridge id<MTLBuffer>)input.constants;
            id<MTLBuffer> __unsafe_unretained indices = (__bridge id<MTLBuffer>)input.indices;
            if (cached.positions != positions || cached.position_offset != input.position_offset)
            {
                [native setVertexBuffer:positions offset:input.position_offset atIndex:0];
                cached.positions = positions;
                cached.position_offset = input.position_offset;
            }
            if (cached.constants != constants || cached.constant_offset != input.constant_offset)
            {
                [native setVertexBuffer:constants offset:input.constant_offset atIndex:1];
                cached.constants = constants;
                cached.constant_offset = input.constant_offset;
            }
            if (!cached.draw_set || cached.indices != indices ||
                cached.index_offset != input.index_offset ||
                cached.index_count != input.index_count || cached.index_type != input.index_type)
            {
                [native drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                    indexCount:input.index_count indexType:input.index_type
                    indexBuffer:indices indexBufferOffset:input.index_offset
                    instanceCount:1 baseVertex:0 baseInstance:0];
                cached.indices = indices;
                cached.index_offset = input.index_offset;
                cached.index_count = input.index_count;
                cached.index_type = input.index_type;
                cached.draw_set = true;
            }
            for (std::size_t binding = 0; binding < 7; ++binding)
            {
                if (!(input.binding_mask & (1u << binding))) continue;
                id<MTLBuffer> __unsafe_unretained buffer =
                    (__bridge id<MTLBuffer>)input.extra_buffers[binding];
                if (cached.extra_buffers[binding] != buffer ||
                    cached.extra_offsets[binding] != input.extra_offsets[binding])
                {
                    if (binding < 5)
                        [native setVertexBuffer:buffer offset:input.extra_offsets[binding] atIndex:binding + 2];
                    else
                        [native setFragmentBuffer:buffer offset:input.extra_offsets[binding] atIndex:binding - 5];
                    cached.extra_buffers[binding] = buffer;
                    cached.extra_offsets[binding] = input.extra_offsets[binding];
                }
                if (slot->resource_set.insert(input.extra_buffers[binding]))
                    slot->resources.push_back(input.extra_buffers[binding]);
            }
            if (input.binding_mask) ++extended_count;
            for (void* handle : {input.positions, input.constants, input.indices})
                if (slot->resource_set.insert(handle)) slot->resources.push_back(handle);
        }
        for (std::size_t i = draws.size(); i < slot->previous_count; ++i)
        {
            auto& cached = slot->commands[i];
            [cached.command reset];
            cached.positions = nil;
            cached.constants = nil;
            cached.indices = nil;
            cached.extra_buffers = {};
            cached.binding_mask = 0;
            cached.draw_set = false;
        }
        slot->previous_count = draws.size();
        }
        slot->leased = true;
        slot->completion = std::move(completion);
        batch_impl->slot = std::move(slot);
    }
    if (std::exchange(pool.fail_next_wrapper_allocation_for_testing, false))
        throw std::bad_alloc{};
    auto result = std::unique_ptr<Batch>(new Batch(std::move(batch_impl)));
    pool.statistics.prepared_draws += draws.size();
    pool.statistics.extended_prepared_draws += extended_count;
    if (gpu_generated) pool.statistics.gpu_generated_draws += draws.size();
    return result;
}
catch (const std::bad_alloc&)
{
    if (failure) *failure = ALMetalShadowPrepareFailure::host_allocation;
    // Do not allocate another diagnostic string while handling allocation
    // failure. The typed reason remains available to the optional caller.
    if (error) error->clear();
    return nullptr;
}

bool ALMetalShadowIndirectCommands::execute(
    ALMetalRenderEncoder& encoder, const Batch& batch,
    std::size_t first, std::size_t count, std::string* error)
{
    auto& prepared = *batch.mImpl;
    if (!prepared.indirect || prepared.owner != mImpl->pool || first > prepared.count ||
        count > prepared.count - first || !encoder.canExecuteIndirect() ||
        encoder.nativeDeviceForIndirect() != (__bridge void*)mImpl->pool->device ||
        encoder.nativeCommandBufferForIndirect() != (__bridge void*)prepared.command)
    {
        setError(error, "Metal shadow indirect execution requires the prepared command and compatible encoder");
        return false;
    }
    if (count == 0) return true;
    if (prepared.slot->generated && !prepared.slot->visibility)
    {
        setError(error, "GPU-generated shadow commands require their visibility preparation");
        return false;
    }
    id<MTLRenderCommandEncoder> __unsafe_unretained native =
        (__bridge id<MTLRenderCommandEncoder>)encoder.nativeHandleForIndirect();
    if (!native)
    {
        setError(error, "Metal shadow indirect render encoder is unavailable");
        return false;
    }
    if (prepared.declared_encoder != native)
    {
        const auto& resources = prepared.slot->resources;
        const auto* native_resources = reinterpret_cast<
            id<MTLResource> __unsafe_unretained const*>(resources.data());
        [native useResources:native_resources count:resources.size()
            usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment];
        prepared.declared_encoder = native;
        mImpl->pool->statistics.resource_declarations += resources.size();
    }
    if (prepared.slot->visibility)
    {
        const auto found=std::find_if(prepared.visibility_runs.begin(),prepared.visibility_runs.end(),
            [&](const auto& run){return run.first==first && run.count==count;});
        if(found==prepared.visibility_runs.end())
        {setError(error,"Visible shadow execution must preserve a prepared state/query run");return false;}
        const auto index=std::size_t(found-prepared.visibility_runs.begin());
        [native executeCommandsInBuffer:prepared.slot->visible_buffer
            indirectBuffer:(__bridge id<MTLBuffer>)prepared.slot->visibility->executionRanges()
            indirectBufferOffset:index*sizeof(ALMetalShadowVisibilityRun)];
    }
    else [native executeCommandsInBuffer:prepared.slot->buffer withRange:NSMakeRange(first, count)];
    // A range submission does not expose post-reset/GPU-modified ICB contents.
    // Keep its range metadata distinct from exact direct draw/element totals.
    encoder.recordIndirectSubmission(static_cast<std::uint32_t>(count));
    encoder.invalidateIndirectBufferBindings();
    ++mImpl->pool->statistics.executions;
    mImpl->pool->statistics.executed_draws += count;
    return true;
}

bool ALMetalShadowIndirectCommands::prepareVisibility(
    ALMetalCommandBuffer& command,Batch& batch,
    std::span<const ALMetalShadowVisibilityInput> inputs,
    std::span<const ALMetalShadowVisibilityRun> runs,bool request_metal4,std::string* error)
{
    auto& prepared=*batch.mImpl;
    if(!prepared.indirect || prepared.owner!=mImpl->pool || !prepared.slot ||
        prepared.command!=(__bridge id<MTLCommandBuffer>)command.nativeHandle() ||
        inputs.size()!=prepared.count || prepared.slot->visibility)
    {setError(error,"Shadow visibility requires its own unmodified indirect batch");return false;}
    auto& pool=*mImpl->pool;
    if(!pool.visibility)
    {
        pool.visibility=ALMetalShadowVisibility::create(*pool.device_wrapper,error);
    }
    if(!pool.visibility)return false;
    if(!prepared.slot->visible_buffer)
    {
        auto descriptor=[MTLIndirectCommandBufferDescriptor new];
        descriptor.commandTypes=MTLIndirectCommandTypeDrawIndexed;
        descriptor.inheritPipelineState=YES;descriptor.inheritBuffers=NO;
        descriptor.maxVertexBufferBindCount=7;descriptor.maxFragmentBufferBindCount=2;
        prepared.slot->visible_buffer=[pool.device newIndirectCommandBufferWithDescriptor:descriptor
            maxCommandCount:prepared.slot->capacity options:MTLResourceStorageModePrivate];
        if(!prepared.slot->visible_buffer)
        {setError(error,"Could not allocate GPU-visible shadow commands");return false;}
    }
    prepared.visibility_runs.assign(runs.begin(),runs.end());
    std::vector<ALMetalShadowVisibilityDraw> draws(prepared.count);
    pool.gpu_addresses.reset(prepared.slot->resources.size());
    const auto address = [&](void* handle, std::size_t offset) {
        return pool.gpu_addresses.gpuAddress(handle) + offset;
    };
    for(std::size_t i=0;i<prepared.count;++i)
    {
        auto& draw=draws[i];
        if(prepared.slot->generated)
        {
            const auto& input=prepared.slot->generated_draws[i];
            draw.vertex_buffers[0]=address(input.positions,input.position_offset);
            draw.vertex_buffers[1]=address(input.constants,input.constant_offset);
            for(std::size_t binding=0;binding<7;++binding)
            {
                if(!(input.binding_mask&(1u<<binding)))continue;
                const auto value=address(input.extra_buffers[binding],input.extra_offsets[binding]);
                if(binding<5)draw.vertex_buffers[binding+2]=value;
                else draw.fragment_buffers[binding-5]=value;
            }
            draw.indices=address(input.indices,input.index_offset);
            draw.index_count=input.index_count;draw.index32=input.index_type==MTLIndexTypeUInt32;
        }
        else
        {
        const auto& input=prepared.slot->commands[i];
        draw.vertex_buffers[0]=address((__bridge void*)input.positions,input.position_offset);
        draw.vertex_buffers[1]=address((__bridge void*)input.constants,input.constant_offset);
        for(std::size_t binding=0;binding<7;++binding)
        {
            if(!(input.binding_mask&(1u<<binding)))continue;
            const auto value=address((__bridge void*)input.extra_buffers[binding],input.extra_offsets[binding]);
            if(binding<5)draw.vertex_buffers[binding+2]=value;
            else draw.fragment_buffers[binding-5]=value;
        }
        draw.indices=address((__bridge void*)input.indices,input.index_offset);
        draw.index_count=input.index_count;draw.index32=input.index_type==MTLIndexTypeUInt32;
        }
    }
    prepared.slot->visibility=pool.visibility->encode(command,
        (__bridge void*)prepared.slot->visible_buffer,
        inputs,runs,prepared.slot->resources,draws,request_metal4,error);
    if(!prepared.slot->visibility)return false;
    ++pool.statistics.visibility_dispatches;
    if(prepared.slot->visibility->usedMetal4())++pool.statistics.metal4_visibility_dispatches;
    return true;
}

std::unique_ptr<ALMetalShadowIndirectCommands::Batch>
ALMetalShadowIndirectCommands::prepareDirect(
    ALMetalCommandBuffer& command,
    std::span<const ALMetalShadowIndirectDraw> draws, std::string* error,
    ALMetalShadowPrepareFailure* failure)
try
{
    if (failure) *failure = ALMetalShadowPrepareFailure::none;
    auto& pool = *mImpl->pool;
    if (!command.nativeHandle() || command.isCommitted() ||
        command.nativeDevice() != (__bridge void*)pool.device ||
        draws.size() > maximum_draw_count)
    {
        if (failure) *failure = ALMetalShadowPrepareFailure::unavailable;
        setError(error, "Metal static shadow preparation requires a compatible uncommitted command");
        return nullptr;
    }
    auto completion = command.completionToken();
    if (!completion)
    {
        if (failure) *failure = ALMetalShadowPrepareFailure::completion_tracking;
        setError(error, "Metal static shadow preparation requires completion tracking");
        return nullptr;
    }
    if (!validateDraws(pool, draws, error))
    {
        if (failure) *failure = ALMetalShadowPrepareFailure::validation;
        return nullptr;
    }
    auto batch_impl = std::make_unique<Batch::Impl>();
    batch_impl->owner = mImpl->pool;
    batch_impl->command = (__bridge id<MTLCommandBuffer>)command.nativeHandle();
    batch_impl->count = draws.size();
    batch_impl->indirect = false;
    if (!draws.empty())
    {
        if (!command.retainUntilCompletion(mImpl->pool, error))
        {
            if (failure) *failure = ALMetalShadowPrepareFailure::retention;
            return nullptr;
        }
        std::shared_ptr<DirectSlot> slot;
        for (const auto& candidate : pool.direct_slots)
        {
            if (!candidate->leased && candidate->draws.capacity() >= draws.size() &&
                (!candidate->completion || candidate->completion->isComplete()) &&
                (!slot || candidate->draws.capacity() < slot->draws.capacity()))
                slot = candidate;
        }
        if (!slot)
        {
            slot = std::make_shared<DirectSlot>();
            pool.direct_slots.push_back(slot);
            ++pool.statistics.direct_slot_allocations;
        }
        if (slot->used) ++pool.statistics.direct_slot_reuses;
        slot->used = true;
        // No per-view packet copy or per-draw ARC ownership. Scratch takes
        // the retired array's capacity for the next preparation.
        slot->draws.swap(pool.scratch);
        slot->resources.clear();
        slot->resources.reserve(pool.validated_handles.size());
        for (void* handle : pool.validated_handles)
            slot->resources.push_back((__bridge id<MTLBuffer>)handle);
        slot->completion = std::move(completion);
        slot->leased = true;
        batch_impl->direct_slot = std::move(slot);
    }
    if (std::exchange(pool.fail_next_wrapper_allocation_for_testing, false))
        throw std::bad_alloc{};
    auto result = std::unique_ptr<Batch>(new Batch(std::move(batch_impl)));
    pool.statistics.direct_prepared_draws += draws.size();
    return result;
}
catch (const std::bad_alloc&)
{
    if (failure) *failure = ALMetalShadowPrepareFailure::host_allocation;
    if (error) error->clear();
    return nullptr;
}

bool ALMetalShadowIndirectCommands::executeDirect(
    ALMetalRenderEncoder& encoder, const Batch& batch,
    std::size_t first, std::size_t count, std::string* error)
{
    const auto& prepared = *batch.mImpl;
    if (prepared.indirect || prepared.owner != mImpl->pool ||
        first > prepared.count || count > prepared.count - first ||
        !encoder.canExecuteStaticDraws() ||
        encoder.nativeDeviceForIndirect() != (__bridge void*)mImpl->pool->device ||
        encoder.nativeCommandBufferForIndirect() != (__bridge void*)prepared.command)
    {
        setError(error, "Metal static shadow execution requires its prepared command and active pipeline");
        return false;
    }
    if (count == 0) return true;
    id<MTLRenderCommandEncoder> __unsafe_unretained native =
        (__bridge id<MTLRenderCommandEncoder>)encoder.nativeHandleForIndirect();
    if (!native)
    {
        setError(error, "Metal static shadow render encoder is unavailable");
        return false;
    }
    id<MTLBuffer> __unsafe_unretained positions = nil;
    id<MTLBuffer> __unsafe_unretained constants = nil;
    std::size_t position_offset = 0, constant_offset = 0;
    std::uint64_t input_elements = 0;
    for (std::size_t i = first; i < first + count; ++i)
    {
        const auto& draw = prepared.direct_slot->draws[i];
        id<MTLBuffer> __unsafe_unretained next_positions =
            (__bridge id<MTLBuffer>)draw.positions;
        id<MTLBuffer> __unsafe_unretained next_constants =
            (__bridge id<MTLBuffer>)draw.constants;
        if (positions != next_positions)
        {
            [native setVertexBuffer:next_positions offset:draw.position_offset atIndex:0];
            positions = next_positions;
            position_offset = draw.position_offset;
        }
        else if (position_offset != draw.position_offset)
        {
            [native setVertexBufferOffset:draw.position_offset atIndex:0];
            position_offset = draw.position_offset;
        }
        if (constants != next_constants)
        {
            [native setVertexBuffer:next_constants offset:draw.constant_offset atIndex:1];
            constants = next_constants;
            constant_offset = draw.constant_offset;
        }
        else if (constant_offset != draw.constant_offset)
        {
            [native setVertexBufferOffset:draw.constant_offset atIndex:1];
            constant_offset = draw.constant_offset;
        }
        for (std::size_t binding = 0; binding < 7; ++binding)
        {
            if (!(draw.binding_mask & (1u << binding))) continue;
            id<MTLBuffer> __unsafe_unretained resource =
                (__bridge id<MTLBuffer>)draw.extra_buffers[binding];
            if (binding < 5)
                [native setVertexBuffer:resource offset:draw.extra_offsets[binding] atIndex:binding + 2];
            else
                [native setFragmentBuffer:resource offset:draw.extra_offsets[binding] atIndex:binding - 5];
        }
        [native drawIndexedPrimitives:MTLPrimitiveTypeTriangle
            indexCount:draw.index_count indexType:draw.index_type
            indexBuffer:(__bridge id<MTLBuffer>)draw.indices
            indexBufferOffset:draw.index_offset instanceCount:1 baseVertex:0 baseInstance:0];
        // prepareDirect bounds count by maximum_draw_count (16384), so this
        // sum of uint32 index counts fits uint64 without a GPU readback.
        input_elements += draw.index_count;
    }
    encoder.recordPrevalidatedDraws(static_cast<std::uint32_t>(count), input_elements);
    encoder.invalidateIndirectBufferBindings();
    ++mImpl->pool->statistics.direct_executions;
    mImpl->pool->statistics.direct_executed_draws += count;
    return true;
}

ALMetalShadowIndirectStats ALMetalShadowIndirectCommands::stats() const noexcept
{
    auto result = mImpl->pool->statistics;
    result.indirect_supported = mImpl->pool->supported;
    result.slot_count = mImpl->pool->slots.size();
    result.direct_slot_count = mImpl->pool->direct_slots.size();
    result.visibility_failures = mImpl->pool->visibility ? mImpl->pool->visibility->failures() : 0;
    result.metal4_timed_dispatches = mImpl->pool->visibility ? mImpl->pool->visibility->metal4TimedDispatches() : 0;
    result.metal4_gpu_time_nanoseconds = mImpl->pool->visibility ? mImpl->pool->visibility->metal4GPUTimeNanoseconds() : 0;
    return result;
}
