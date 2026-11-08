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
#include <limits>
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
            return true;
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
    };

    struct Slot
    {
        id<MTLIndirectCommandBuffer> buffer = nil;
        std::vector<CachedCommand> commands;
        std::vector<void*> resources;
        IdentitySet resource_set;
        std::shared_ptr<const ALMetalCommandCompletion> completion;
        std::size_t previous_count = 0;
        bool leased = false;
        bool used = false;
    };

    struct Pool
    {
        id<MTLDevice> device = nil;
        bool supported = false;
        std::vector<std::shared_ptr<Slot>> slots;
        std::vector<NativeDraw> scratch;
        IdentitySet validated_resources;
        ALMetalShadowIndirectStats statistics;
    };

    std::shared_ptr<Slot> makeSlot(Pool& pool, std::size_t count,
                                   std::string* error)
    {
        std::size_t capacity = 128;
        while (capacity < count) capacity *= 2;
        auto descriptor = [MTLIndirectCommandBufferDescriptor new];
        descriptor.commandTypes = MTLIndirectCommandTypeDrawIndexed;
        descriptor.inheritPipelineState = YES;
        descriptor.inheritBuffers = NO;
        descriptor.maxVertexBufferBindCount = 2;
        descriptor.maxFragmentBufferBindCount = 0;
        auto slot = std::make_shared<Slot>();
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
        slot->resources.reserve(capacity * 3);
        slot->resource_set.reset(capacity * 3);
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
    id<MTLCommandBuffer> command = nil;
    // Strong ownership prevents a later encoder from recycling the address
    // used to suppress duplicate resource declarations in this transaction.
    id<MTLRenderCommandEncoder> declared_encoder = nil;
    std::size_t count = 0;
};

ALMetalShadowIndirectCommands::Batch::Batch(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}

ALMetalShadowIndirectCommands::Batch::~Batch()
{
    if (mImpl->slot) mImpl->slot->leased = false;
}

std::size_t ALMetalShadowIndirectCommands::Batch::drawCount() const noexcept
{
    return mImpl->count;
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

std::unique_ptr<ALMetalShadowIndirectCommands::Batch>
ALMetalShadowIndirectCommands::prepare(
    ALMetalCommandBuffer& command,
    std::span<const ALMetalShadowIndirectDraw> draws, std::string* error)
{
    auto& pool = *mImpl->pool;
    if (!pool.supported || !command.nativeHandle() || command.isCommitted() ||
        command.nativeDevice() != (__bridge void*)pool.device ||
        draws.size() > maximum_draw_count)
    {
        setError(error, "Metal shadow indirect preparation is unavailable or invalid");
        return nullptr;
    }
    auto completion = command.completionToken();
    if (!completion)
    {
        setError(error, "Metal shadow indirect preparation requires completion tracking");
        return nullptr;
    }
    // Validate the entire draw list before touching a cached native command.
    pool.scratch.clear();
    pool.scratch.reserve(draws.size());
    pool.validated_resources.reset(draws.size() * 3);
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
            !fits(draw.constant_offset, constant_size, 1, draw.constants->length()))
        {
            setError(error, "Metal shadow indirect draw range is invalid");
            return nullptr;
        }
        void* positions = draw.positions->nativeHandle();
        void* constants = draw.constants->nativeHandle();
        void* indices = draw.indices->nativeHandle();
        for (void* handle : {positions, constants, indices})
        {
            if (!handle || (pool.validated_resources.insert(handle) &&
                ((__bridge id<MTLBuffer>)handle).device != pool.device))
            {
                setError(error, "Metal shadow indirect buffer belongs to another device");
                return nullptr;
            }
        }
        pool.scratch.push_back({positions, constants, indices,
            draw.position_offset, draw.constant_offset, draw.index_offset,
            draw.index_count, index32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16});
    }
    auto batch_impl = std::make_unique<Batch::Impl>();
    batch_impl->owner = mImpl->pool;
    batch_impl->command = (__bridge id<MTLCommandBuffer>)command.nativeHandle();
    batch_impl->count = draws.size();
    if (!draws.empty())
    {
        // useResources declares indirect access but does not retain buffers.
        // Keep the slot pool alive even if its renderer and public lease are
        // destroyed before submission/completion of the prepared command.
        if (!command.retainUntilCompletion(mImpl->pool, error)) return nullptr;
        std::shared_ptr<Slot> slot;
        for (const auto& candidate : pool.slots)
        {
            if (!candidate->leased && candidate->commands.size() >= draws.size() &&
                (!candidate->completion || candidate->completion->isComplete()) &&
                (!slot || candidate->commands.size() < slot->commands.size()))
                slot = candidate;
        }
        if (!slot) slot = makeSlot(pool, draws.size(), error);
        if (!slot) return nullptr;
        if (slot->used) ++pool.statistics.slot_reuses;
        slot->used = true;
        slot->resource_set.reset(draws.size() * 3);
        slot->resources.clear();
        for (std::size_t i = 0; i < draws.size(); ++i)
        {
            const auto& input = pool.scratch[i];
            auto& cached = slot->commands[i];
            id<MTLIndirectRenderCommand> __unsafe_unretained native = cached.command;
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
            cached.draw_set = false;
        }
        slot->previous_count = draws.size();
        slot->leased = true;
        slot->completion = std::move(completion);
        batch_impl->slot = std::move(slot);
    }
    pool.statistics.prepared_draws += draws.size();
    return std::unique_ptr<Batch>(new Batch(std::move(batch_impl)));
}

bool ALMetalShadowIndirectCommands::execute(
    ALMetalRenderEncoder& encoder, const Batch& batch,
    std::size_t first, std::size_t count, std::string* error)
{
    auto& prepared = *batch.mImpl;
    if (prepared.owner != mImpl->pool || first > prepared.count ||
        count > prepared.count - first || !encoder.canExecuteIndirect() ||
        encoder.nativeDeviceForIndirect() != (__bridge void*)mImpl->pool->device ||
        encoder.nativeCommandBufferForIndirect() != (__bridge void*)prepared.command)
    {
        setError(error, "Metal shadow indirect execution requires the prepared command and compatible encoder");
        return false;
    }
    if (count == 0) return true;
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
            usage:MTLResourceUsageRead stages:MTLRenderStageVertex];
        prepared.declared_encoder = native;
        mImpl->pool->statistics.resource_declarations += resources.size();
    }
    [native executeCommandsInBuffer:prepared.slot->buffer withRange:NSMakeRange(first, count)];
    encoder.recordIndirectDraws(static_cast<std::uint32_t>(count));
    encoder.invalidateIndirectBufferBindings();
    ++mImpl->pool->statistics.executions;
    mImpl->pool->statistics.executed_draws += count;
    return true;
}

ALMetalShadowIndirectStats ALMetalShadowIndirectCommands::stats() const noexcept
{
    auto result = mImpl->pool->statistics;
    result.slot_count = mImpl->pool->slots.size();
    return result;
}
