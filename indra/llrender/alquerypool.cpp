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
    bool visibilityKind(ALQueryKind kind)
    {
        return kind == ALQueryKind::Occlusion ||
               kind == ALQueryKind::OcclusionExact;
    }
}

ALQueryPool::ALQueryPool(ALFrameFences&) noexcept
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
           !mOpen.isValid() && !mVisibilityReserved;
}

ALQueryTicket ALQueryPool::acquire(ALQueryKind kind, bool reserved)
{
    if (!mVisibilityCaptureActive || !supports(kind) ||
        mOpen.isValid() || (mVisibilityReserved != reserved))
    {
        return {};
    }

    Frame& frame = mFrames[mSerial % RESULT_FRAMES];
    if (frame.serial != mSerial)
    {
        frame.entries.clear();
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
    return ticket;
}

ALQueryTicket ALQueryPool::begin(ALQueryKind kind)
{
    return acquire(kind, false);
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

ALQuerySpan ALQueryPool::beginSpan()
{
    return {};
}

void ALQueryPool::endSpan(ALQuerySpan&)
{
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
        return Status::Expired;
    }

    U64 native_value = 0;
    switch (entry->results->status())
    {
    case ALMetalVisibilityResultBatch::Status::pending:
        return Status::Pending;
    case ALMetalVisibilityResultBatch::Status::ready:
        if (!entry->results->result(entry->result_index, native_value))
        {
            return Status::Expired;
        }
        value = native_value;
        return Status::Ready;
    case ALMetalVisibilityResultBatch::Status::failed:
        return Status::Expired;
    }
    return Status::Expired;
}

ALQueryPool::Status ALQueryPool::result(
    const ALQueryTicket&, ALStatistics& statistics) const
{
    statistics = {};
    return Status::Expired;
}

ALQueryPool::Status ALQueryPool::result(
    const ALQuerySpan&, U64& nanoseconds) const
{
    nanoseconds = 0;
    return Status::Expired;
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

bool ALQueryPool::wait(const ALQueryTicket&, ALStatistics& statistics)
{
    statistics = {};
    return false;
}

bool ALQueryPool::wait(const ALQuerySpan&, U64& nanoseconds)
{
    nanoseconds = 0;
    return false;
}

void ALQueryPool::reserve(ALQueryKind kind, bool reserved)
{
    if (supports(kind))
    {
        mVisibilityReserved = reserved;
    }
}

ALQueryTicket ALQueryPool::beginReserved(ALQueryKind kind)
{
    return acquire(kind, true);
}

void ALQueryPool::beginConditional(const ALQueryTicket&)
{
}

void ALQueryPool::endConditional()
{
}

void ALQueryPool::checkPassBoundary() const
{
}

void ALQueryPool::retire()
{
}

void ALQueryPool::makeRoom()
{
}

void ALQueryPool::endFrame()
{
    endVisibilityCapture();
    if (++mSerial == 0)
    {
        // Zero is the invalid ticket sentinel. A wrap is practically
        // unreachable, but releasing retained entries preserves that rule.
        release();
        mSerial = 1;
    }
    Frame& frame = mFrames[mSerial % RESULT_FRAMES];
    frame.entries.clear();
    frame.serial = mSerial;
}

void ALQueryPool::release()
{
    for (Frame& frame : mFrames)
    {
        frame.entries.clear();
        frame.serial = 0;
    }
    mOpen = {};
    mVisibilityCaptureActive = false;
    mVisibilityReserved = false;
    mFrames[mSerial % RESULT_FRAMES].serial = mSerial;
}

bool ALQueryPool::beginVisibilityCapture()
{
    if (mVisibilityCaptureActive || mOpen.isValid())
    {
        return false;
    }
    mVisibilityCaptureActive = true;
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
    return true;
}

void ALQueryPool::discard(const ALQueryTicket& ticket)
{
    Entry* entry = find(ticket);
    if (entry)
    {
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
