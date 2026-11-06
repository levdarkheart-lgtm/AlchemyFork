/**
 * @file alquerypool_test.cpp
 * @brief Native-only legacy query contract tests.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * $/LicenseInfo$
 */

#include "catch2/catch_test_macros.hpp"

#include "alframefences.h"
#include "alquerypool.h"

TEST_CASE("Legacy query acquisition fails closed without a fallback backend",
          "[alquerypool][metal-cutover]")
{
    ALFrameFences frames;
    ALQueryPool queries(frames);

    for (U32 value = 0; value < U32(ALQueryKind::Count); ++value)
    {
        const auto kind = ALQueryKind(value);
        CHECK_FALSE(ALQueryPool::supports(kind));
        CHECK_FALSE(queries.canBegin(kind));
        CHECK_FALSE(queries.begin(kind).isValid());
        queries.reserve(kind, true);
        CHECK_FALSE(queries.beginReserved(kind).isValid());
        queries.reserve(kind, false);
    }

    ALQueryTicket ticket;
    U64 count = 99;
    CHECK(queries.result(ticket, count) == ALQueryPool::Status::Expired);
    CHECK(count == 0);
    count = 99;
    CHECK_FALSE(queries.wait(ticket, count));
    CHECK(count == 0);

    ALStatistics statistics;
    statistics.mCounts[0] = 3;
    statistics.mCounts[1] = 4;
    CHECK(queries.result(ticket, statistics) == ALQueryPool::Status::Expired);
    CHECK(statistics.mCounts[0] == 0);
    CHECK(statistics.mCounts[1] == 0);

    ALQuerySpan span = queries.beginSpan();
    CHECK_FALSE(span.isValid());
    queries.endSpan(span);
    U64 nanoseconds = 99;
    CHECK(queries.result(span, nanoseconds) == ALQueryPool::Status::Expired);
    CHECK(nanoseconds == 0);
    nanoseconds = 99;
    CHECK_FALSE(queries.wait(span, nanoseconds));
    CHECK(nanoseconds == 0);

    queries.beginConditional(ticket);
    queries.endConditional();
    queries.checkPassBoundary();
    queries.endFrame();
    queries.retire();
    queries.makeRoom();
    queries.release();
    CHECK(queries.getStalls() == 0);
    CHECK(queries.getAskedReads() == 0);
    CHECK(queries.getNameCount() == 0);
}
