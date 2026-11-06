/**
 * @file aluniformbuffer.h
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

#pragma once

#include "algpubuffer.h"
#include "stdtypes.h"

#include <cstddef>
#include <vector>

// Constant data is stored in native Metal buffers. Whole-block data uses an
// object-owned store. Frequently rewritten shadowed blocks and scratch ranges
// use frame-timeline-protected shared rings so CPU writes never overwrite a
// range still referenced by an in-flight Metal command buffer.
//
// bind()/bindRange() publish explicit semantic state. Native encoders can read
// currentBinding() and bind its ALMetalBuffer at their pipeline-specific Metal
// index. This keeps the engine's stable block vocabulary without retaining an
// implicit graphics-context binding table or a fallback renderer.
class ALUniformBuffer
{
public:
    enum class EUsage { STREAM, DYNAMIC, STATIC };

    // Retained settings vocabulary. These values used to select driver-specific
    // upload strategies. All live values now select the one safe native ring;
    // keeping the range avoids breaking persisted settings and diagnostic UI.
    enum EUpdateMode : S32
    {
        UPDATE_DIRECT           = 0,
        UPDATE_ORPHAN           = 1,
        UPDATE_RING             = 2,
        UPDATE_PERSISTENT       = 3,
        UPDATE_PERSISTENT_FLUSH = 4,
    };
    static S32 sUpdateMode;
    static S32 clampUpdateMode(S32 mode);

    // Engine block points are currently 0..7. Extra points leave room for
    // ranged skin/material payloads while keeping invalid indices fail-closed.
    static constexpr U32 MAX_BINDINGS = 32;

    struct Binding
    {
        const ALGPUBuffer*   mStore  = nullptr;
        const ALMetalBuffer* mBuffer = nullptr;
        size_t mOffset = 0;
        size_t mSize   = 0;
        U64    mSerial = 0;

        explicit operator bool() const
        {
            return mStore && mBuffer && mSize != 0;
        }
    };

    ALUniformBuffer() = default;
    ~ALUniformBuffer();

    ALUniformBuffer(const ALUniformBuffer&) = delete;
    ALUniformBuffer& operator=(const ALUniformBuffer&) = delete;
    ALUniformBuffer(ALUniformBuffer&& other) noexcept;
    ALUniformBuffer& operator=(ALUniformBuffer&& other) noexcept;

    // Replace the whole store. STREAM/DYNAMIC use coherent shared Apple
    // Silicon storage; STATIC uses private storage and an ordered upload.
    void update(const void* data, size_t bytes,
                EUsage usage = EUsage::STREAM);

    void bind(U32 binding) const;
    // Metal constant-buffer offsets use the renderer's conservative 256-byte
    // alignment. Sizes may remain tightly packed.
    void bindRange(U32 binding, size_t offset, size_t size) const;

    // A snapshot of the semantic binding at this point in command encoding.
    // Invalid/out-of-range points return an empty Binding.
    static Binding currentBinding(U32 binding);

    // Shadowed high-frequency constant blocks.
    void initShadowed(size_t bytes);
    U8*  beginWrite() { return mShadow.data(); }
    void endWrite(size_t offset, size_t bytes);
    void flush(U32 binding);
    void bindCurrent(U32 binding);
    void ensureCurrent(U32 binding);

    // Changes when the native stream ring is recreated or completes a lap.
    static U32 ringWrapEpoch();

    bool dirty() const { return mDirty; }
    void release();
    static void cleanupClass();

    struct RingStats
    {
        U32 mSlices        = 0;
        U64 mBytes         = 0;
        U32 mSegmentSteps  = 0;
        U32 mWraps         = 0;
        U32 mStalls        = 0;
        U32 mStallTimeouts = 0;
        U32 mSpills        = 0;
        U32 mSegmentMask   = 0;
        U32 mChurnFlushes  = 0;
        U32 mLiveRebinds   = 0;
    };

    static void endFrame();
    static const RingStats& lastFramePersistentStats();
    static const RingStats& lastFrameRingStats();
    static const RingStats& lastFrameScratchStats();

    // Upload-once, bind-per-use ranged payloads such as skinning palettes.
    static S64  scratchWrite(const void* data, size_t bytes);
    static void scratchBindRange(U32 binding, S64 offset, size_t bytes);
    static U32  scratchGeneration();

    bool   allocated() const { return mStore.isAllocated(); }
    size_t size() const { return mSize; }
    ALMetalBuffer* getMetalBuffer() { return mStore.getMetalBuffer(); }
    const ALMetalBuffer* getMetalBuffer() const { return mStore.getMetalBuffer(); }

private:
    bool flushNative(U32 binding);
    bool sliceLive() const;
    void publish(U32 binding, const ALGPUBuffer& store,
                 size_t offset, size_t bytes) const;

    ALGPUBuffer mStore;
    size_t mSize = 0;

    std::vector<U8> mShadow;
    bool   mDirty = false;
    size_t mRingOffset = 0;
    U32    mRingGeneration = 0;
    const ALGPUBuffer* mSliceStore = nullptr;
};
