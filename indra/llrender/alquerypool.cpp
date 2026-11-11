/**
 * @file alquerypool.cpp
 * @brief Asynchronous renderer queries backed by Metal visibility results.
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

#include "linden_common.h"

#include "alquerypool.h"

#include "almetalresource.h"

#include <algorithm>
#include <limits>
#include <new>

namespace
{
    constexpr U8 PHASE_ATTACHED = 1u << 0;
    constexpr U8 PHASE_PENDING = 1u << 1;
    constexpr U8 PHASE_READY_ZERO = 1u << 2;
    constexpr U8 PHASE_READY_NONZERO = 1u << 3;
    constexpr U8 PHASE_EXPIRED = 1u << 4;
    constexpr U8 PHASE_TERMINAL = PHASE_READY_ZERO | PHASE_READY_NONZERO | PHASE_EXPIRED;

    bool visibilityKind(ALQueryKind kind)
    {
        return kind == ALQueryKind::Occlusion ||
               kind == ALQueryKind::OcclusionExact;
    }
}

bool ALQueryPool::sDiagnosticsEnabled = false;
U64 ALQueryPool::sDiagnosticEpoch = 0;

void ALQueryPool::setDiagnosticsEnabled(bool enabled) noexcept
{
    sDiagnosticsEnabled = enabled;
    if (enabled)
    {
        if (++sDiagnosticEpoch == 0) ++sDiagnosticEpoch;
        mDiagnostics = {};
        mDiagnostics.epoch = sDiagnosticEpoch;
    }
}

bool ALQueryPool::diagnosticEntry(const Entry& entry) const noexcept
{
    return entry.diagnostic_epoch != 0 &&
        entry.diagnostic_epoch == mDiagnostics.epoch;
}

void ALQueryPool::recordMainWorldGroupTransition(bool occluded) noexcept
{
    if (diagnosticsArmed())
    {
        if (occluded) ++mDiagnostics.group_occluded_transitions;
        else ++mDiagnostics.group_visible_transitions;
    }
}

void ALQueryPool::recordMainWorldGroupSkip(GroupSkip skip) noexcept
{
    if (diagnosticsArmed())
    {
        if (skip == GroupSkip::spatial_cull) ++mDiagnostics.spatial_group_skips;
        else ++mDiagnostics.state_sort_group_skips;
    }
}

void ALQueryPool::retireEntries(Frame& frame)
{
    if (diagnosticsArmed())
    {
        for (const Entry& entry : frame.entries)
        {
            if (diagnosticEntry(entry))
            {
                ++mDiagnostics.retired;
                if (!(entry.diagnostic_phases & PHASE_TERMINAL))
                    ++mDiagnostics.retired_without_terminal_observation;
            }
        }
    }
    frame.entries.clear();
}

ALQueryPool::Status ALQueryPool::observeResult(
    const Entry* entry, Status status, U64 value) const
{
    if (!diagnosticsArmed()) return status;
    if (!entry || !diagnosticEntry(*entry))
    {
        // A missing/older ticket cannot establish its producer or uniqueness.
        ++mDiagnostics.untracked_polls;
        return status;
    }
    U8 phase = PHASE_EXPIRED;
    U64* unique = &mDiagnostics.expired_unique;
    if (status == Status::Pending)
    {
        phase = PHASE_PENDING;
        unique = &mDiagnostics.pending_unique;
        ++mDiagnostics.pending_polls;
    }
    else if (status == Status::Ready && value == 0)
    {
        phase = PHASE_READY_ZERO;
        unique = &mDiagnostics.ready_zero_unique;
        ++mDiagnostics.ready_zero_polls;
    }
    else if (status == Status::Ready)
    {
        phase = PHASE_READY_NONZERO;
        unique = &mDiagnostics.ready_nonzero_unique;
        ++mDiagnostics.ready_nonzero_polls;
    }
    else ++mDiagnostics.expired_polls;
    if (entry->diagnostic_phases & phase) ++mDiagnostics.duplicate_phase_polls;
    else
    {
        entry->diagnostic_phases |= phase;
        ++*unique;
    }
    return status;
}

ALQueryPool::ALQueryPool() noexcept
{
    mFrames[mSerial % RESULT_FRAMES].serial = mSerial;
}

// static
bool ALQueryPool::supports(ALQueryKind kind)
{
    return visibilityKind(kind);
}

bool ALQueryPool::canBegin(ALQueryKind kind) const
{
    return mVisibilityCaptureActive && supports(kind) &&
           !mOpen.isValid();
}

ALQueryTicket ALQueryPool::acquire(ALQueryKind kind)
{
    if (!mVisibilityCaptureActive || !supports(kind) ||
        mOpen.isValid())
    {
        return {};
    }

    Frame& frame = mFrames[mSerial % RESULT_FRAMES];
    if (frame.serial != mSerial)
    {
        retireEntries(frame);
        frame.serial = mSerial;
    }
    constexpr std::size_t ticket_slot_count = 1u << 24;
    if (frame.entries.size() >= ticket_slot_count)
    {
        return {};
    }

    ALQueryTicket ticket;
    ticket.mSerial = mSerial;
    ticket.mSlot = static_cast<U32>(frame.entries.size());
    ticket.mKind = static_cast<U32>(kind);
    try
    {
        frame.entries.push_back({ ticket });
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
    mOpen = ticket;
    if (diagnosticsArmed() && mVisibilityCaptureMainWorld)
    {
        frame.entries.back().diagnostic_epoch = mDiagnostics.epoch;
        ++mDiagnostics.acquired;
    }
    return ticket;
}

ALQueryTicket ALQueryPool::begin(ALQueryKind kind)
{
    return acquire(kind);
}

void ALQueryPool::end(const ALQueryTicket& ticket)
{
    if (!ticket.isValid() || !(ticket == mOpen))
    {
        return;
    }
    if (Entry* entry = find(ticket))
    {
        entry->closed = true;
    }
    mOpen = {};
}

ALQueryPool::Entry* ALQueryPool::find(const ALQueryTicket& ticket)
{
    return const_cast<Entry*>(
        static_cast<const ALQueryPool*>(this)->find(ticket));
}

const ALQueryPool::Entry* ALQueryPool::find(
    const ALQueryTicket& ticket) const
{
    if (!ticket.isValid())
    {
        return nullptr;
    }
    const Frame& frame = mFrames[ticket.mSerial % RESULT_FRAMES];
    if (frame.serial != ticket.mSerial ||
        ticket.mSlot >= frame.entries.size())
    {
        return nullptr;
    }
    const Entry& entry = frame.entries[ticket.mSlot];
    return entry.ticket == ticket ? &entry : nullptr;
}

ALQueryPool::Status ALQueryPool::result(
    const ALQueryTicket& ticket, U64& value) const
{
    value = 0;
    const Entry* entry = find(ticket);
    if (!entry || !entry->closed || entry->discarded || !entry->results)
    {
        return sDiagnosticsEnabled ? observeResult(entry, Status::Expired, 0) : Status::Expired;
    }

    U64 native_value = 0;
    switch (entry->results->status())
    {
    case ALMetalVisibilityResultBatch::Status::pending:
        return sDiagnosticsEnabled ? observeResult(entry, Status::Pending, 0) : Status::Pending;
    case ALMetalVisibilityResultBatch::Status::ready:
        if (!entry->results->result(entry->result_index, native_value))
        {
            return sDiagnosticsEnabled ? observeResult(entry, Status::Expired, 0) : Status::Expired;
        }
        value = native_value;
        return sDiagnosticsEnabled ? observeResult(entry, Status::Ready, value) : Status::Ready;
    case ALMetalVisibilityResultBatch::Status::failed:
        return sDiagnosticsEnabled ? observeResult(entry, Status::Expired, 0) : Status::Expired;
    }
    return sDiagnosticsEnabled ? observeResult(entry, Status::Expired, 0) : Status::Expired;
}

bool ALQueryPool::wait(const ALQueryTicket& ticket, U64& value)
{
    value = 0;
    Entry* entry = find(ticket);
    if (!entry || !entry->closed || entry->discarded || !entry->results)
    {
        return false;
    }
    ++mAskedReads;
    const auto before = entry->results->status();
    U64 native_value = 0;
    if (!entry->results->wait(entry->result_index, native_value))
    {
        return false;
    }
    if (before == ALMetalVisibilityResultBatch::Status::pending)
    {
        ++mStalls;
    }
    value = native_value;
    return true;
}

void ALQueryPool::endFrame()
{
    endVisibilityCapture();
    if (++mSerial == 0)
    {
        // Zero is the invalid ticket sentinel. A wrap is practically
        // unreachable, but releasing retained entries preserves that rule.
        release();
    }
    Frame& frame = mFrames[mSerial % RESULT_FRAMES];
    retireEntries(frame);
    frame.serial = mSerial;
}

void ALQueryPool::release()
{
    for (Frame& frame : mFrames)
    {
        retireEntries(frame);
        frame.serial = 0;
    }
    mOpen = {};
    mVisibilityCaptureActive = false;
    mVisibilityCaptureMainWorld = false;
    // The original pool advanced its serial on release: tickets owned by
    // prior captures must never act on the next generation's reused slots.
    // Zero is the invalid sentinel, including endFrame's wrap boundary.
    if (++mSerial == 0) mSerial = 1;
    mFrames[mSerial % RESULT_FRAMES].serial = mSerial;
}

bool ALQueryPool::beginVisibilityCapture(bool main_world)
{
    if (mVisibilityCaptureActive || mOpen.isValid())
    {
        return false;
    }
    mVisibilityCaptureActive = true;
    mVisibilityCaptureMainWorld = main_world;
    return true;
}

void ALQueryPool::endVisibilityCapture()
{
    if (mOpen.isValid())
    {
        discard(mOpen);
    }
    mOpen = {};
    mVisibilityCaptureActive = false;
    mVisibilityCaptureMainWorld = false;
}

bool ALQueryPool::attachVisibilityResult(
    const ALQueryTicket& ticket,
    std::shared_ptr<ALMetalVisibilityResultBatch> results,
    std::size_t result_index)
{
    Entry* entry = find(ticket);
    if (!entry || !entry->closed || entry->discarded || !results ||
        result_index >= results->resultCount())
    {
        return false;
    }
    entry->results = std::move(results);
    entry->result_index = result_index;
    if (diagnosticsArmed() && diagnosticEntry(*entry))
    {
        ++mDiagnostics.attachment_calls;
        if (!(entry->diagnostic_phases & PHASE_ATTACHED))
        {
            entry->diagnostic_phases |= PHASE_ATTACHED;
            ++mDiagnostics.attached_unique;
        }
    }
    return true;
}

void ALQueryPool::discard(const ALQueryTicket& ticket)
{
    Entry* entry = find(ticket);
    if (entry)
    {
        if (diagnosticsArmed() && diagnosticEntry(*entry) && !entry->discarded)
            ++mDiagnostics.discarded_unique;
        entry->results.reset();
        entry->closed = true;
        entry->discarded = true;
    }
    if (ticket == mOpen)
    {
        mOpen = {};
    }
}

U32 ALQueryPool::getNameCount() const
{
    std::size_t count = 0;
    for (const Frame& frame : mFrames)
    {
        count += frame.entries.size();
    }
    return static_cast<U32>(std::min<std::size_t>(
        count, std::numeric_limits<U32>::max()));
}
