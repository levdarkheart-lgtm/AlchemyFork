/**
 * @file alquerypool.h
 * @brief Renderer query contract for the native Metal viewer.
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

#include <array>
#include <memory>
#include <vector>

class ALFrameFences;
class ALMetalVisibilityResultBatch;

enum class ALQueryKind : U8
{
    Occlusion,
    OcclusionExact,
    Samples,
    Primitives,
    Statistics,
    Count
};

enum class ALStatistic : U8
{
    FragmentInvocations,
    ClippingPrimitives,
    Count
};

struct ALStatistics
{
    U64 mCounts[U32(ALStatistic::Count)] = {};

    U64 operator[](ALStatistic statistic) const { return mCounts[U32(statistic)]; }
};

class ALQueryTicket
{
public:
    bool isValid() const { return mSerial != 0; }

    bool operator==(const ALQueryTicket& other) const
    {
        return mSerial == other.mSerial && mSlot == other.mSlot && mKind == other.mKind;
    }

private:
    friend class ALQueryPool;

    U32 mSerial    = 0;
    U32 mSlot : 24 = 0;
    U32 mKind : 8  = 0;
};

class ALQuerySpan
{
public:
    bool isValid() const { return mBegin.isValid(); }

private:
    friend class ALQueryPool;

    ALQueryTicket mBegin;
    ALQueryTicket mEnd;
};

/// Legacy-facing asynchronous query contract backed by Metal visibility
/// result buffers. Only the two visibility kinds are native: unsupported
/// profiling/counter kinds remain invalid rather than fabricating data.
class ALQueryPool
{
public:
    static constexpr U32 FRAMES        = 4;
    static constexpr U32 RESULT_FRAMES = 64;

    enum class Status : U8
    {
        Pending,
        Ready,
        Expired,
    };

    explicit ALQueryPool(ALFrameFences&) noexcept;
    ALQueryPool(const ALQueryPool&) = delete;
    ALQueryPool& operator=(const ALQueryPool&) = delete;

    static bool supports(ALQueryKind kind);
    bool canBegin(ALQueryKind kind) const;

    ALQueryTicket begin(ALQueryKind kind);
    void end(const ALQueryTicket& ticket);

    ALQuerySpan beginSpan();
    void endSpan(ALQuerySpan& span);

    Status result(const ALQueryTicket& ticket, U64& value) const;
    Status result(const ALQueryTicket& ticket, ALStatistics& statistics) const;
    Status result(const ALQuerySpan& span, U64& nanoseconds) const;

    bool wait(const ALQueryTicket& ticket, U64& value);
    bool wait(const ALQueryTicket& ticket, ALStatistics& statistics);
    bool wait(const ALQuerySpan& span, U64& nanoseconds);

    void reserve(ALQueryKind kind, bool reserved);
    ALQueryTicket beginReserved(ALQueryKind kind);

    void beginConditional(const ALQueryTicket& ticket);
    void endConditional();
    void checkPassBoundary() const;

    void retire();
    void makeRoom();
    void endFrame();
    void release();

    // The native world owner brackets the exact retained traversal interval
    // in which bounding boxes can be captured. Results are attached after the
    // ordered G-buffer encoder has accepted the corresponding draws.
    bool beginVisibilityCapture();
    void endVisibilityCapture();
    bool visibilityCaptureActive() const { return mVisibilityCaptureActive; }
    bool attachVisibilityResult(
        const ALQueryTicket& ticket,
        std::shared_ptr<ALMetalVisibilityResultBatch> results,
        std::size_t result_index);
    void discard(const ALQueryTicket& ticket);

    U32 getStalls() const { return mStalls; }
    U32 getAskedReads() const { return mAskedReads; }
    U32 getNameCount() const;

private:
    struct Entry
    {
        ALQueryTicket ticket;
        std::shared_ptr<ALMetalVisibilityResultBatch> results;
        std::size_t result_index = 0;
        bool closed = false;
        bool discarded = false;
    };

    struct Frame
    {
        U32 serial = 0;
        std::vector<Entry> entries;
    };

    Entry* find(const ALQueryTicket& ticket);
    const Entry* find(const ALQueryTicket& ticket) const;
    ALQueryTicket acquire(ALQueryKind kind, bool reserved);

    std::array<Frame, RESULT_FRAMES> mFrames;
    ALQueryTicket mOpen;
    U32 mSerial = 1;
    U32 mStalls = 0;
    U32 mAskedReads = 0;
    bool mVisibilityCaptureActive = false;
    bool mVisibilityReserved = false;
};

class ALQueryScope
{
public:
    ALQueryScope(ALQueryPool& pool, ALQueryKind kind) : mPool(pool), mTicket(pool.begin(kind)) {}
    ~ALQueryScope() { mPool.end(mTicket); }

    ALQueryScope(const ALQueryScope&) = delete;
    ALQueryScope& operator=(const ALQueryScope&) = delete;

    const ALQueryTicket& ticket() const { return mTicket; }

private:
    ALQueryPool&  mPool;
    ALQueryTicket mTicket;
};

class ALConditionalRender
{
public:
    ALConditionalRender(ALQueryPool& pool, const ALQueryTicket& ticket) : mPool(pool)
    {
        pool.beginConditional(ticket);
    }
    ~ALConditionalRender() { mPool.endConditional(); }

    ALConditionalRender(const ALConditionalRender&) = delete;
    ALConditionalRender& operator=(const ALConditionalRender&) = delete;

private:
    ALQueryPool& mPool;
};
