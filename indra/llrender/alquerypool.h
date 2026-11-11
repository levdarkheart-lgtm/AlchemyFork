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

    enum class GroupSkip : U8 { spatial_cull, state_sort };

    // Explicit benchmark-only, main-thread diagnostics. These count query
    // tickets, not occluded draw calls or saved GPU work. Phase counts overlap:
    // one ticket may first be pending and later become ready. Repeated polls
    // of the same phase are reported separately from first observations.
    struct Diagnostics
    {
        U64 epoch = 0;
        U64 acquired = 0;
        U64 attachment_calls = 0; // successful calls, including reattachment
        U64 attached_unique = 0;
        U64 pending_polls = 0;
        U64 ready_zero_polls = 0;
        U64 ready_nonzero_polls = 0;
        U64 expired_polls = 0;
        U64 pending_unique = 0;
        U64 ready_zero_unique = 0;
        U64 ready_nonzero_unique = 0;
        U64 expired_unique = 0;
        U64 duplicate_phase_polls = 0;
        U64 untracked_polls = 0;
        U64 discarded_unique = 0;
        U64 retired = 0;
        U64 retired_without_terminal_observation = 0; // no terminal result() poll; wait() keeps its existing counters
        U64 group_occluded_transitions = 0;
        U64 group_visible_transitions = 0;
        U64 spatial_group_skips = 0;
        U64 state_sort_group_skips = 0;
    };

    // Arming starts a new epoch. Existing tickets are deliberately untracked,
    // including tickets acquired while diagnostics were previously armed.
    void setDiagnosticsEnabled(bool enabled) noexcept;
    static bool diagnosticsEnabled() noexcept { return sDiagnosticsEnabled; }
    Diagnostics diagnostics() const noexcept { return mDiagnostics; }
    void recordMainWorldGroupTransition(bool occluded) noexcept;
    void recordMainWorldGroupSkip(GroupSkip skip) noexcept;

    ALQueryPool() noexcept;
    ALQueryPool(const ALQueryPool&) = delete;
    ALQueryPool& operator=(const ALQueryPool&) = delete;

    static bool supports(ALQueryKind kind);
    bool canBegin(ALQueryKind kind) const;

    ALQueryTicket begin(ALQueryKind kind);
    void end(const ALQueryTicket& ticket);

    Status result(const ALQueryTicket& ticket, U64& value) const;

    bool wait(const ALQueryTicket& ticket, U64& value);

    void endFrame();
    void release();

    // The native world owner brackets the exact retained traversal interval
    // in which bounding boxes can be captured. Results are attached after the
    // ordered G-buffer encoder has accepted the corresponding draws.
    bool beginVisibilityCapture(bool main_world = false);
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
        U64 diagnostic_epoch = 0;
        mutable U8 diagnostic_phases = 0;
    };

    struct Frame
    {
        U32 serial = 0;
        std::vector<Entry> entries;
    };

    Entry* find(const ALQueryTicket& ticket);
    const Entry* find(const ALQueryTicket& ticket) const;
    ALQueryTicket acquire(ALQueryKind kind);
    void retireEntries(Frame& frame);
    Status observeResult(const Entry* entry, Status status, U64 value) const;
    bool diagnosticEntry(const Entry& entry) const noexcept;
    bool diagnosticsArmed() const noexcept
    {
        return sDiagnosticsEnabled && mDiagnostics.epoch != 0 &&
            mDiagnostics.epoch == sDiagnosticEpoch;
    }

    static bool sDiagnosticsEnabled;
    static U64 sDiagnosticEpoch;
    mutable Diagnostics mDiagnostics;

    std::array<Frame, RESULT_FRAMES> mFrames;
    ALQueryTicket mOpen;
    U32 mSerial = 1;
    U32 mStalls = 0;
    U32 mAskedReads = 0;
    bool mVisibilityCaptureActive = false;
    bool mVisibilityCaptureMainWorld = false;
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
