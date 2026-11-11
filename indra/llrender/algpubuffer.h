/**
 * @file algpubuffer.h
 * @brief Native Metal buffer storage for shader data, indirect commands,
 *        readback and streaming rings.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * $/LicenseInfo$
 */

#pragma once

#include "stdtypes.h"

#include <cstddef>
#include <memory>
#include <vector>

class ALFrameFences;
class ALMetalBuffer;
class ALMetalDevice;

// Stable storage-binding vocabulary retained by the renderer; native pipeline
// layouts reserve these first eight points.
constexpr U32 AL_NUM_STORAGE_BINDINGS = 8;

// A native Metal buffer the GPU reads and writes. STREAM and READBACK stores use
// coherent shared Apple Silicon memory; other stores use private GPU memory and
// ordered blit commands for upload, fill, copy and synchronous exceptional
// readback. The device is installed with the renderer backend, so allocation
// fails closed before native initialization or after teardown. Non-copyable;
// movable.
class ALGPUBuffer
{
public:
    enum EFlags : U32
    {
        DYNAMIC        = 1 << 0,  // upload() may rewrite private storage
        READBACK       = 1 << 1,  // coherent shared storage mapped for CPU reads
        STREAM         = 1 << 2,  // write-combined shared storage mapped for CPU writes
        FLUSH_EXPLICIT = 1 << 3,  // retained ring intent; a validated no-op on coherent Apple memory
    };

    ALGPUBuffer();
    ~ALGPUBuffer();

    ALGPUBuffer(const ALGPUBuffer&) = delete;
    ALGPUBuffer& operator=(const ALGPUBuffer&) = delete;

    // Moving transfers the native store and mapping; the source is left empty.
    ALGPUBuffer(ALGPUBuffer&& other) noexcept;
    ALGPUBuffer& operator=(ALGPUBuffer&& other) noexcept;

    static void setMetalDevice(ALMetalDevice* device) noexcept;
    static ALMetalDevice* getMetalDevice() noexcept;

    // Replaces any store this held. `data` may be null (contents undefined
    // until written or cleared). `label` names it in Metal diagnostics.
    bool allocate(size_t bytes, U32 flags = 0, const void* data = nullptr, const char* label = nullptr);

    // Release native storage. Destruction is also safe after backend teardown.
    void release();

    bool   isAllocated() const { return mBuffer != nullptr; }
    size_t size() const { return mSize; }
    // Temporary compile boundary for legacy consumers that are unreachable in
    // the Metal product. A native allocation never has a GL object name.
    U32    getGLName() const { return 0; }
    ALMetalBuffer* getMetalBuffer() { return mBuffer.get(); }
    const ALMetalBuffer* getMetalBuffer() const { return mBuffer.get(); }
    U32    getFlags() const { return mFlags; }

    // The persistent map of a READBACK or STREAM shared store; null for private
    // storage. The CPU sees GPU writes after their frame has completed, and the
    // GPU sees CPU writes in commands submitted after the write. FLUSH_EXPLICIT
    // is retained as ring intent but is a range-validated no-op on coherent
    // Apple Silicon shared memory.
    U8* getMapped() const { return mMapped; }

    // Apple Silicon shared storage is coherent; retained as an explicit
    // publication point for ring callers and validated as a range.
    void flushMapped(size_t offset, size_t bytes);

    // Write `bytes` from `data` at `offset`. DYNAMIC and STREAM buffers only (asserted). A mapped STREAM store takes
    // it as a copy into the map, which the GPU must not be reading, flushed there for FLUSH_EXPLICIT.
    void upload(size_t offset, const void* data, size_t bytes);

    // Set every 4-byte word to `value`.
    void clear(U32 value = 0);

    // Set every 4-byte word of [offset, offset + bytes) to `value`; both
    // multiples of 4. Encoded as an ordered Metal blit operation.
    void clearRange(size_t offset, size_t bytes, U32 value = 0);

    // Copy `bytes` of `src` from `src_offset` to `dst_offset` here on the GPU.
    // `src` may be this buffer if the ranges do not overlap.
    void copyFrom(const ALGPUBuffer& src, size_t src_offset, size_t dst_offset, size_t bytes);

    // Read `bytes` at `offset` into `out`. Synchronous: waits for the GPU. Tests and debugging only.
    bool read(size_t offset, void* out, size_t bytes) const;

    // Every live ALGPUBuffer's store, in bytes.
    static U64 sBytesAllocated;

private:
    std::unique_ptr<ALMetalBuffer> mBuffer;
    ALMetalDevice* mDevice = nullptr;
    size_t mSize   = 0;
    U32    mFlags  = 0;
    U8*    mMapped = nullptr;
};

// Data the CPU writes for the GPU every frame, without waiting on an upload or on the GPU: a ring of regions in one
// STREAM buffer. map() gives the next region to write, publish() hands it to the GPU, and the GPU has it until the ring
// comes round to it again, `regions` publishes later, and until the frame its reads were issued in has finished
// (ALFrameFences); only a GPU that many frames behind, or a ring mapped round within one frame, makes map() wait. Where
// persistent maps did not resolve, map() gives memory of its own and publish() uploads it.
//
// A published region's reads -- binds, draws, dispatches -- are issued before the next map(), which notes the frame
// they were issued in, or are noted with noteRead() in the frame they are issued, as a user that keeps what it
// published for later frames does: the region stays as published until the ring comes round to it, `regions` maps on.
// Neither call lays or polls a fence while the GPU keeps up.
//
// Frame completion comes from the renderer's Metal scheduler (gGL's frame
// timeline unless another is provided). Non-copyable.
class ALGPUStream
{
public:
    explicit ALGPUStream(ALFrameFences* frames = nullptr) : mFrameFences(frames) {}
    ~ALGPUStream();

    ALGPUStream(const ALGPUStream&) = delete;
    ALGPUStream& operator=(const ALGPUStream&) = delete;

    // Replaces what this held: `regions` regions of up to `bytes` each.
    bool allocate(size_t bytes, U32 regions = 3, const char* label = nullptr);
    void release();

    bool   isAllocated() const { return mStore.isAllocated(); }
    size_t getRegionSize() const { return mRegionBytes; }
    // The buffer a published region lies in, for LLRender::bindStorageBuffer and the indirect calls.
    const ALGPUBuffer& getBuffer() const { return mStore; }

    // Where the next region's bytes go, getRegionSize() of them. Notes the frame the region published last was read in.
    U8* map();

    // Hand the GPU the `bytes` written since map(). Returns where they start in getBuffer(): a multiple of
    // the native 256-byte storage/constant slice alignment.
    size_t publish(size_t bytes);

    // The region publish() placed at `offset` is read again in this frame, after a later map(): the ring writes it
    // again only once this frame has finished. Every read, in every frame, is noted so.
    void noteRead(size_t offset);

    // map() calls that found their region still in use by the GPU, and waited.
    U32 getStalls() const { return mStalls; }

private:
    ALFrameFences& frames() const;

    ALFrameFences*     mFrameFences  = nullptr; // gGL's where null
    ALGPUBuffer        mStore;
    std::vector<U32>   mReadIn;            // per region, the frame its last reads were issued in, 0 for none
    std::vector<U8>    mStaging;           // what map() gives where mStore is not mapped
    size_t             mRegionBytes  = 0;
    size_t             mRegionStride = 0;
    S32                mPublished    = -1; // the region published last, its frame not yet noted
    S32                mWriting      = -1; // the region map() gave, not yet published
    U32                mNext         = 0;  // the region the next map() gives
    U32                mStalls       = 0;
};
