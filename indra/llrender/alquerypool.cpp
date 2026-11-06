/**
 * @file alquerypool.cpp
 * @brief Fail-closed renderer query contract for the native Metal viewer.
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

// Metal visibility-result and counter-sample buffers must be attached while
// their owning render pass is encoded. The legacy immediate query API has no
// such encoder boundary yet. Advertising support here used to enter removed
// GL calls from ordinary world culling after the GL context was gone. Until a
// native encoder adapter owns those scopes, every acquisition is invalid and
// every result expires. Visibility users consequently render (fail open)
// rather than incorrectly cull, while profiling users receive no fabricated
// measurements.

// static
bool ALQueryPool::supports(ALQueryKind)
{
    return false;
}

bool ALQueryPool::canBegin(ALQueryKind) const
{
    return false;
}

ALQueryTicket ALQueryPool::begin(ALQueryKind)
{
    return {};
}

void ALQueryPool::end(const ALQueryTicket&)
{
}

ALQuerySpan ALQueryPool::beginSpan()
{
    return {};
}

void ALQueryPool::endSpan(ALQuerySpan&)
{
}

ALQueryPool::Status ALQueryPool::result(const ALQueryTicket&, U64& value) const
{
    value = 0;
    return Status::Expired;
}

ALQueryPool::Status ALQueryPool::result(const ALQueryTicket&, ALStatistics& statistics) const
{
    statistics = {};
    return Status::Expired;
}

ALQueryPool::Status ALQueryPool::result(const ALQuerySpan&, U64& nanoseconds) const
{
    nanoseconds = 0;
    return Status::Expired;
}

bool ALQueryPool::wait(const ALQueryTicket&, U64& value)
{
    value = 0;
    return false;
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

void ALQueryPool::reserve(ALQueryKind, bool)
{
}

ALQueryTicket ALQueryPool::beginReserved(ALQueryKind)
{
    return {};
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
}

void ALQueryPool::release()
{
}
