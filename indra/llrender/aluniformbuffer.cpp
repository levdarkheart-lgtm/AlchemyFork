/**
 * @file aluniformbuffer.cpp
 * @brief Native Metal constant-buffer storage and binding state.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "aluniformbuffer.h"

#include "alframefences.h"
#include "llrender.h"

#include <array>
#include <cstring>
#include <utility>

namespace
{
    constexpr size_t NATIVE_CONSTANT_ALIGNMENT = 256;
    constexpr int    RING_SEGMENTS = 4;
    constexpr int    RING_SHRINK_COOLDOWN = 240;
    constexpr U64    WAIT_NANOSECONDS = 1000000000ull;

    constexpr size_t STREAM_MIN_BYTES = 1 * 1024 * 1024;
    constexpr size_t STREAM_MAX_BYTES = 16 * 1024 * 1024;
    constexpr int    STREAM_SLACK_FRAMES = 4;

    constexpr size_t SCRATCH_MIN_BYTES = 1 * 1024 * 1024;
    constexpr size_t SCRATCH_MAX_BYTES = 16 * 1024 * 1024;
    constexpr size_t SCRATCH_MAX_PAYLOAD = 256 * 1024;
    constexpr int    SCRATCH_SLACK_FRAMES = 4;

    static_assert(STREAM_MIN_BYTES <= STREAM_MAX_BYTES);
    static_assert(SCRATCH_MIN_BYTES <= SCRATCH_MAX_BYTES);
    static_assert(SCRATCH_MAX_PAYLOAD <=
                  SCRATCH_MIN_BYTES / RING_SEGMENTS);

    struct BindingSlot
    {
        ALUniformBuffer::Binding mBinding;
        const ALUniformBuffer*   mOwner = nullptr;
    };

    std::array<BindingSlot, ALUniformBuffer::MAX_BINDINGS> sBindings;
    U64 sBindingSerial = 0;

    ALGPUBuffer sStreamRing;
    size_t sStreamHead = 0;
    size_t sStreamBytes = STREAM_MIN_BYTES;
    int sStreamSegment = 0;
    U32 sStreamGeneration = 1;
    U32 sStreamReadIn[RING_SEGMENTS] = {};
    int sStreamShrinkCooldown = 0;
    U64 sStreamPeakBytes = 0;

    ALGPUBuffer sScratchRing;
    size_t sScratchHead = 0;
    size_t sScratchBytes = SCRATCH_MIN_BYTES;
    int sScratchSegment = 0;
    U32 sScratchGeneration = 1;
    U32 sScratchReadIn[RING_SEGMENTS] = {};
    int sScratchShrinkCooldown = 0;
    U64 sScratchPeakBytes = 0;

    ALUniformBuffer::RingStats sPersistentStats;
    ALUniformBuffer::RingStats sRingStats;
    ALUniformBuffer::RingStats sScratchStats;
    ALUniformBuffer::RingStats sPersistentLastFrame;
    ALUniformBuffer::RingStats sRingLastFrame;
    ALUniformBuffer::RingStats sScratchLastFrame;

    size_t align_up(size_t value, size_t alignment)
    {
        return (value + alignment - 1) / alignment * alignment;
    }

    bool in_range(size_t offset, size_t bytes, size_t size)
    {
        return offset <= size && bytes <= size - offset;
    }

    void clear_slot(BindingSlot& slot)
    {
        slot = BindingSlot();
    }

    void invalidate_owner(const ALUniformBuffer* owner)
    {
        for (BindingSlot& slot : sBindings)
        {
            if (slot.mOwner == owner)
            {
                clear_slot(slot);
            }
        }
    }

    void invalidate_store(const ALGPUBuffer* store)
    {
        for (BindingSlot& slot : sBindings)
        {
            if (slot.mBinding.mStore == store)
            {
                clear_slot(slot);
            }
        }
    }

    void retarget_owner(const ALUniformBuffer* from,
                        const ALUniformBuffer* to,
                        const ALGPUBuffer* from_store,
                        const ALGPUBuffer* to_store)
    {
        for (BindingSlot& slot : sBindings)
        {
            if (slot.mOwner == from)
            {
                slot.mOwner = to;
            }
            if (slot.mBinding.mStore == from_store)
            {
                slot.mBinding.mStore = to_store;
                slot.mBinding.mBuffer = to_store->getMetalBuffer();
            }
        }
    }

    bool publish_binding(const ALUniformBuffer* owner, U32 binding,
                         const ALGPUBuffer& store, size_t offset,
                         size_t bytes)
    {
        if (binding >= sBindings.size() || !store.isAllocated() ||
            !store.getMetalBuffer() || bytes == 0 ||
            !in_range(offset, bytes, store.size()))
        {
            llassert(false);
            return false;
        }

        BindingSlot& slot = sBindings[binding];
        if (slot.mOwner == owner &&
            slot.mBinding.mStore == &store &&
            slot.mBinding.mBuffer == store.getMetalBuffer() &&
            slot.mBinding.mOffset == offset &&
            slot.mBinding.mSize == bytes)
        {
            return false;
        }

        slot.mOwner = owner;
        slot.mBinding.mStore = &store;
        slot.mBinding.mBuffer = store.getMetalBuffer();
        slot.mBinding.mOffset = offset;
        slot.mBinding.mSize = bytes;
        slot.mBinding.mSerial = ++sBindingSerial;
        return true;
    }

    bool binding_matches(const ALUniformBuffer* owner, U32 binding,
                         const ALGPUBuffer& store, size_t offset,
                         size_t bytes)
    {
        if (binding >= sBindings.size())
        {
            return false;
        }
        const BindingSlot& slot = sBindings[binding];
        return slot.mOwner == owner &&
               slot.mBinding.mStore == &store &&
               slot.mBinding.mBuffer == store.getMetalBuffer() &&
               slot.mBinding.mOffset == offset &&
               slot.mBinding.mSize == bytes;
    }

    ALUniformBuffer::RingStats& stream_stats()
    {
        return ALUniformBuffer::sUpdateMode == ALUniformBuffer::UPDATE_RING
            ? sRingStats : sPersistentStats;
    }

    bool ensure_ring(ALGPUBuffer& ring, size_t bytes, const char* label)
    {
        if (ring.isAllocated())
        {
            return ring.size() == bytes && ring.getMapped();
        }
        return ring.allocate(bytes, ALGPUBuffer::STREAM, nullptr, label) &&
               ring.getMapped();
    }

    // Leaves the current segment only when the destination segment is safe.
    // An open-frame collision returns false so the caller can grow or spill;
    // it never waits for an unsubmitted command buffer and never overwrites it.
    bool advance_ring(size_t aligned, size_t ring_bytes,
                      size_t& head, int& segment, U32* read_in,
                      U32& generation, ALUniformBuffer::RingStats& stats,
                      const char* label)
    {
        const size_t segment_bytes = ring_bytes / RING_SEGMENTS;
        if (head + aligned <=
            static_cast<size_t>(segment + 1) * segment_bytes)
        {
            return true;
        }

        ALFrameFences& frames = gGL.frameFences();
        int next = segment + 1;
        bool wraps = false;
        if (next == RING_SEGMENTS)
        {
            next = 0;
            wraps = true;
        }

        if (!frames.isComplete(read_in[next]))
        {
            if (read_in[next] >= frames.frame())
            {
                return false;
            }

            bool stalled = false;
            if (!frames.wait(read_in[next], WAIT_NANOSECONDS, &stalled))
            {
                ++stats.mStallTimeouts;
                LL_WARNS_ONCE("ShaderUniform")
                    << label
                    << " native constant ring could not retire a segment in one second; "
                       "using safe overflow storage."
                    << LL_ENDL;
                return false;
            }
            if (stalled)
            {
                ++stats.mStalls;
            }
        }

        read_in[segment] = frames.frame();
        read_in[next] = 0;
        ++stats.mSegmentSteps;
        stats.mSegmentMask |= U32(1u << segment);
        segment = next;
        head = static_cast<size_t>(next) * segment_bytes;
        if (wraps)
        {
            ++generation;
            ++stats.mWraps;
        }
        return true;
    }

    size_t ring_size_for(U64 frame_bytes, size_t floor_bytes,
                         size_t cap_bytes, int slack)
    {
        const U64 wanted = frame_bytes * static_cast<U64>(slack);
        size_t result = floor_bytes;
        while (result < wanted && result < cap_bytes)
        {
            result <<= 1;
        }
        return llmin(result, cap_bytes);
    }

    size_t next_ring_size(size_t current, U64 frame_bytes,
                          size_t floor_bytes, size_t cap_bytes, int slack,
                          int& shrink_cooldown, U64& peak_bytes)
    {
        peak_bytes = llmax(peak_bytes, frame_bytes);
        const size_t target =
            ring_size_for(frame_bytes, floor_bytes, cap_bytes, slack);
        if (target > current)
        {
            shrink_cooldown = 0;
            peak_bytes = 0;
            return target;
        }
        if (++shrink_cooldown < RING_SHRINK_COOLDOWN)
        {
            return current;
        }
        const size_t window =
            ring_size_for(peak_bytes, floor_bytes, cap_bytes, slack);
        shrink_cooldown = 0;
        peak_bytes = 0;
        return window;
    }

    void teardown_stream_ring()
    {
        invalidate_store(&sStreamRing);
        sStreamRing.release();
        std::memset(sStreamReadIn, 0, sizeof(sStreamReadIn));
        sStreamHead = 0;
        sStreamSegment = 0;
        ++sStreamGeneration;
    }

    void resize_stream_ring(size_t bytes)
    {
        sStreamBytes = bytes;
        teardown_stream_ring();
    }

    void teardown_scratch_ring()
    {
        invalidate_store(&sScratchRing);
        sScratchRing.release();
        std::memset(sScratchReadIn, 0, sizeof(sScratchReadIn));
        sScratchHead = 0;
        sScratchSegment = 0;
        ++sScratchGeneration;
    }

    void resize_scratch_ring(size_t bytes)
    {
        sScratchBytes = bytes;
        teardown_scratch_ring();
    }

    bool grow_stream_ring()
    {
        if (sStreamBytes >= STREAM_MAX_BYTES)
        {
            return false;
        }
        resize_stream_ring(llmin(sStreamBytes << 1, STREAM_MAX_BYTES));
        return ensure_ring(sStreamRing, sStreamBytes,
                           "Alchemy native constant stream ring");
    }

    bool grow_scratch_ring()
    {
        if (sScratchBytes >= SCRATCH_MAX_BYTES)
        {
            return false;
        }
        resize_scratch_ring(llmin(sScratchBytes << 1, SCRATCH_MAX_BYTES));
        return ensure_ring(sScratchRing, sScratchBytes,
                           "Alchemy native constant scratch ring");
    }
}

S32 ALUniformBuffer::sUpdateMode = ALUniformBuffer::UPDATE_ORPHAN;

S32 ALUniformBuffer::clampUpdateMode(S32 mode)
{
    if (mode < UPDATE_DIRECT || mode > UPDATE_PERSISTENT_FLUSH)
    {
        LL_WARNS_ONCE("ShaderUniform")
            << "Constant-buffer update mode " << mode
            << " is not a live setting; using " << UPDATE_ORPHAN << "."
            << LL_ENDL;
        return UPDATE_ORPHAN;
    }
    return mode;
}

ALUniformBuffer::~ALUniformBuffer()
{
    release();
}

ALUniformBuffer::ALUniformBuffer(ALUniformBuffer&& other) noexcept
    : mStore(std::move(other.mStore))
    , mSize(other.mSize)
    , mShadow(std::move(other.mShadow))
    , mDirty(other.mDirty)
    , mRingOffset(other.mRingOffset)
    , mRingGeneration(other.mRingGeneration)
    , mSliceStore(other.mSliceStore == &other.mStore
                      ? &mStore : other.mSliceStore)
{
    retarget_owner(&other, this, &other.mStore, &mStore);
    other.mSize = 0;
    other.mDirty = false;
    other.mRingOffset = 0;
    other.mRingGeneration = 0;
    other.mSliceStore = nullptr;
}

ALUniformBuffer& ALUniformBuffer::operator=(ALUniformBuffer&& other) noexcept
{
    if (this != &other)
    {
        release();
        mStore = std::move(other.mStore);
        mSize = other.mSize;
        mShadow = std::move(other.mShadow);
        mDirty = other.mDirty;
        mRingOffset = other.mRingOffset;
        mRingGeneration = other.mRingGeneration;
        mSliceStore = other.mSliceStore == &other.mStore
            ? &mStore : other.mSliceStore;
        retarget_owner(&other, this, &other.mStore, &mStore);

        other.mSize = 0;
        other.mDirty = false;
        other.mRingOffset = 0;
        other.mRingGeneration = 0;
        other.mSliceStore = nullptr;
    }
    return *this;
}

void ALUniformBuffer::update(const void* data, size_t bytes, EUsage usage)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    invalidate_owner(this);
    mStore.release();
    mSize = 0;
    mSliceStore = nullptr;
    mRingOffset = 0;
    mRingGeneration = 0;
    mDirty = false;

    if (bytes == 0)
    {
        return;
    }

    const U32 flags = usage == EUsage::STATIC ? 0 : ALGPUBuffer::STREAM;
    const char* label = usage == EUsage::STATIC
        ? "Alchemy native static constant buffer"
        : "Alchemy native dynamic constant buffer";
    if (!mStore.allocate(bytes, flags, data, label))
    {
        return;
    }
    mSize = bytes;
    mSliceStore = &mStore;
}

void ALUniformBuffer::publish(U32 binding, const ALGPUBuffer& store,
                              size_t offset, size_t bytes) const
{
    publish_binding(this, binding, store, offset, bytes);
}

void ALUniformBuffer::bind(U32 binding) const
{
    if (!mStore.isAllocated() || mSize == 0)
    {
        return;
    }
    publish(binding, mStore, 0, mSize);
}

void ALUniformBuffer::bindRange(U32 binding, size_t offset,
                                size_t bytes) const
{
    if (!mStore.isAllocated() || bytes == 0 ||
        offset % NATIVE_CONSTANT_ALIGNMENT != 0 ||
        !in_range(offset, bytes, mSize))
    {
        llassert(!mStore.isAllocated() || bytes == 0 ||
                 (offset % NATIVE_CONSTANT_ALIGNMENT == 0 &&
                  in_range(offset, bytes, mSize)));
        return;
    }
    publish(binding, mStore, offset, bytes);
}

ALUniformBuffer::Binding ALUniformBuffer::currentBinding(U32 binding)
{
    if (binding >= sBindings.size())
    {
        return Binding();
    }
    const Binding& result = sBindings[binding].mBinding;
    if (!result.mStore || !result.mStore->isAllocated() ||
        result.mBuffer != result.mStore->getMetalBuffer() ||
        !in_range(result.mOffset, result.mSize, result.mStore->size()))
    {
        return Binding();
    }
    return result;
}

void ALUniformBuffer::initShadowed(size_t bytes)
{
    mShadow.assign(bytes, 0);
    update(mShadow.data(), bytes, EUsage::DYNAMIC);
}

void ALUniformBuffer::endWrite(size_t offset, size_t bytes)
{
    if (!in_range(offset, bytes, mShadow.size()))
    {
        llassert(false);
        return;
    }
    if (bytes != 0)
    {
        mDirty = true;
    }
}

bool ALUniformBuffer::flushNative(U32 binding)
{
    const size_t bytes = mShadow.size();
    if (bytes == 0)
    {
        return false;
    }

    RingStats& stats = stream_stats();
    const size_t aligned =
        align_up(bytes, NATIVE_CONSTANT_ALIGNMENT);

    bool appended = false;
    for (;;)
    {
        if (!ensure_ring(sStreamRing, sStreamBytes,
                         "Alchemy native constant stream ring"))
        {
            break;
        }
        const size_t segment_bytes = sStreamBytes / RING_SEGMENTS;
        if (aligned > segment_bytes)
        {
            if (!grow_stream_ring())
            {
                break;
            }
            continue;
        }
        if (advance_ring(aligned, sStreamBytes, sStreamHead,
                         sStreamSegment, sStreamReadIn,
                         sStreamGeneration, stats, "Main"))
        {
            appended = true;
            break;
        }
        if (!grow_stream_ring())
        {
            break;
        }
    }

    if (appended)
    {
        std::memcpy(sStreamRing.getMapped() + sStreamHead,
                    mShadow.data(), bytes);
        sStreamRing.flushMapped(sStreamHead, bytes);
        publish(binding, sStreamRing, sStreamHead, bytes);
        mSliceStore = &sStreamRing;
        mRingOffset = sStreamHead;
        mRingGeneration = sStreamGeneration;
        sStreamHead += aligned;
        ++stats.mSlices;
        stats.mBytes += aligned;
        return true;
    }

    // The ring can be full of ranges referenced by the still-open frame. A
    // new object-owned store is safe because Metal command buffers retain the
    // old resource they encoded; never wait on or overwrite an open frame.
    ++stats.mSpills;
    invalidate_owner(this);
    if (!mStore.allocate(bytes, ALGPUBuffer::STREAM, mShadow.data(),
                         "Alchemy native constant overflow"))
    {
        mSliceStore = nullptr;
        mSize = 0;
        return false;
    }
    mSize = bytes;
    mSliceStore = &mStore;
    mRingOffset = 0;
    mRingGeneration = 0;
    publish(binding, mStore, 0, bytes);
    return true;
}

void ALUniformBuffer::flush(U32 binding)
{
    if (!mDirty)
    {
        return;
    }
    if (flushNative(binding))
    {
        mDirty = false;
    }
}

bool ALUniformBuffer::sliceLive() const
{
    if (mSliceStore == &mStore)
    {
        return mStore.isAllocated() && mSize != 0;
    }
    if (mSliceStore != &sStreamRing || !sStreamRing.isAllocated() ||
        mRingGeneration != sStreamGeneration)
    {
        return false;
    }
    const size_t segment_bytes = sStreamBytes / RING_SEGMENTS;
    return mRingOffset / segment_bytes ==
           static_cast<size_t>(sStreamSegment);
}

void ALUniformBuffer::bindCurrent(U32 binding)
{
    if (mShadow.empty())
    {
        bind(binding);
        return;
    }
    if (mDirty || !sliceLive())
    {
        if (!mDirty)
        {
            ++stream_stats().mChurnFlushes;
        }
        mDirty = true;
        flush(binding);
        return;
    }

    const size_t bytes = mShadow.size();
    const size_t offset = mSliceStore == &mStore ? 0 : mRingOffset;
    if (!binding_matches(this, binding, *mSliceStore, offset, bytes))
    {
        publish(binding, *mSliceStore, offset, bytes);
    }
    else
    {
        ++stream_stats().mLiveRebinds;
    }
}

void ALUniformBuffer::ensureCurrent(U32 binding)
{
    bindCurrent(binding);
}

U32 ALUniformBuffer::ringWrapEpoch()
{
    return sStreamGeneration;
}

void ALUniformBuffer::release()
{
    invalidate_owner(this);
    mStore.release();
    mSize = 0;
    mShadow.clear();
    mShadow.shrink_to_fit();
    mDirty = false;
    mRingOffset = 0;
    mRingGeneration = 0;
    mSliceStore = nullptr;
}

void ALUniformBuffer::endFrame()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    sPersistentLastFrame = sPersistentStats;
    sRingLastFrame = sRingStats;
    sScratchLastFrame = sScratchStats;

    LL_DEBUGS("UBORing")
        << "native constants (" << (sStreamBytes >> 10) << " KiB): "
        << (sPersistentLastFrame.mSlices + sRingLastFrame.mSlices)
        << " slices, "
        << (sPersistentLastFrame.mBytes + sRingLastFrame.mBytes)
        << " B, "
        << (sPersistentLastFrame.mSegmentSteps +
            sRingLastFrame.mSegmentSteps)
        << " segment steps, "
        << (sPersistentLastFrame.mStalls + sRingLastFrame.mStalls)
        << " stalls, "
        << (sPersistentLastFrame.mSpills + sRingLastFrame.mSpills)
        << " safe overflows | scratch (" << (sScratchBytes >> 10)
        << " KiB): " << sScratchLastFrame.mSlices << " payloads, "
        << sScratchLastFrame.mBytes << " B, "
        << sScratchLastFrame.mStalls << " stalls, "
        << sScratchLastFrame.mSpills << " refused overflows"
        << LL_ENDL;

    const U64 stream_bytes =
        sPersistentLastFrame.mBytes + sRingLastFrame.mBytes;
    sPersistentStats = RingStats();
    sRingStats = RingStats();
    sScratchStats = RingStats();

    if (sStreamRing.isAllocated())
    {
        const size_t next =
            next_ring_size(sStreamBytes, stream_bytes,
                           STREAM_MIN_BYTES, STREAM_MAX_BYTES,
                           STREAM_SLACK_FRAMES,
                           sStreamShrinkCooldown, sStreamPeakBytes);
        if (next != sStreamBytes)
        {
            resize_stream_ring(next);
        }
    }
    if (sScratchRing.isAllocated())
    {
        const size_t next =
            next_ring_size(sScratchBytes, sScratchLastFrame.mBytes,
                           SCRATCH_MIN_BYTES, SCRATCH_MAX_BYTES,
                           SCRATCH_SLACK_FRAMES,
                           sScratchShrinkCooldown, sScratchPeakBytes);
        if (next != sScratchBytes)
        {
            resize_scratch_ring(next);
        }
    }
}

const ALUniformBuffer::RingStats&
ALUniformBuffer::lastFramePersistentStats()
{
    return sPersistentLastFrame;
}

const ALUniformBuffer::RingStats& ALUniformBuffer::lastFrameRingStats()
{
    return sRingLastFrame;
}

const ALUniformBuffer::RingStats&
ALUniformBuffer::lastFrameScratchStats()
{
    return sScratchLastFrame;
}

S64 ALUniformBuffer::scratchWrite(const void* data, size_t bytes)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    if (!data || bytes == 0 || bytes > SCRATCH_MAX_PAYLOAD)
    {
        return -1;
    }

    const size_t aligned =
        align_up(bytes, NATIVE_CONSTANT_ALIGNMENT);
    for (;;)
    {
        if (!ensure_ring(sScratchRing, sScratchBytes,
                         "Alchemy native constant scratch ring"))
        {
            return -1;
        }
        if (advance_ring(aligned, sScratchBytes, sScratchHead,
                         sScratchSegment, sScratchReadIn,
                         sScratchGeneration, sScratchStats, "Scratch"))
        {
            break;
        }
        if (!grow_scratch_ring())
        {
            ++sScratchStats.mSpills;
            return -1;
        }
    }

    std::memcpy(sScratchRing.getMapped() + sScratchHead, data, bytes);
    sScratchRing.flushMapped(sScratchHead, bytes);
    const S64 offset = static_cast<S64>(sScratchHead);
    sScratchHead += aligned;
    ++sScratchStats.mSlices;
    sScratchStats.mBytes += aligned;
    return offset;
}

void ALUniformBuffer::scratchBindRange(U32 binding, S64 offset,
                                       size_t bytes)
{
    if (!sScratchRing.isAllocated() || offset < 0 || bytes == 0 ||
        !in_range(static_cast<size_t>(offset), bytes,
                  sScratchRing.size()))
    {
        return;
    }

    const size_t segment_bytes = sScratchBytes / RING_SEGMENTS;
    const size_t segment = static_cast<size_t>(offset) / segment_bytes;
    if (segment < RING_SEGMENTS &&
        segment != static_cast<size_t>(sScratchSegment))
    {
        sScratchReadIn[segment] = gGL.frameFences().frame();
    }

    if (!publish_binding(nullptr, binding, sScratchRing,
                         static_cast<size_t>(offset), bytes))
    {
        ++sScratchStats.mLiveRebinds;
    }
}

U32 ALUniformBuffer::scratchGeneration()
{
    return sScratchGeneration;
}

void ALUniformBuffer::cleanupClass()
{
    teardown_stream_ring();
    sStreamBytes = STREAM_MIN_BYTES;
    sStreamShrinkCooldown = 0;
    sStreamPeakBytes = 0;

    teardown_scratch_ring();
    sScratchBytes = SCRATCH_MIN_BYTES;
    sScratchShrinkCooldown = 0;
    sScratchPeakBytes = 0;

    for (BindingSlot& slot : sBindings)
    {
        clear_slot(slot);
    }
    sPersistentStats = RingStats();
    sRingStats = RingStats();
    sScratchStats = RingStats();
    sPersistentLastFrame = RingStats();
    sRingLastFrame = RingStats();
    sScratchLastFrame = RingStats();
}
