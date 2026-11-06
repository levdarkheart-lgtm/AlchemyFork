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

class ALFrameFences;

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

/// Query acquisition fails closed until the native render encoders expose
/// visibility and counter-sample scopes through this legacy-facing contract.
/// Invalid/expired results make visibility callers render rather than cull,
/// preserving correctness without claiming or calling a fallback backend.
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

    explicit ALQueryPool(ALFrameFences&) noexcept {}
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

    U32 getStalls() const { return 0; }
    U32 getAskedReads() const { return 0; }
    U32 getNameCount() const { return 0; }
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
