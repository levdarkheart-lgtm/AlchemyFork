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

TEST_CASE("Metal visibility tickets are bounded by retained capture scopes",
          "[alquerypool][metal-cutover]")
{
    ALFrameFences frames;
    ALQueryPool queries(frames);

    for (U32 value = 0; value < U32(ALQueryKind::Count); ++value)
    {
        const auto kind = ALQueryKind(value);
        const bool visibility = kind == ALQueryKind::Occlusion ||
            kind == ALQueryKind::OcclusionExact;
        CHECK(ALQueryPool::supports(kind) == visibility);
        CHECK_FALSE(queries.canBegin(kind));
        CHECK_FALSE(queries.begin(kind).isValid());
    }

    REQUIRE(queries.beginVisibilityCapture());
    CHECK_FALSE(queries.beginVisibilityCapture());
    CHECK(queries.visibilityCaptureActive());
    CHECK(queries.canBegin(ALQueryKind::Occlusion));
    CHECK(queries.canBegin(ALQueryKind::OcclusionExact));
    CHECK_FALSE(queries.canBegin(ALQueryKind::Samples));

    ALQueryTicket ticket = queries.begin(ALQueryKind::Occlusion);
    REQUIRE(ticket.isValid());
    CHECK_FALSE(queries.canBegin(ALQueryKind::Occlusion));
    CHECK_FALSE(queries.begin(ALQueryKind::Occlusion).isValid());
    queries.end(ticket);
    CHECK(queries.canBegin(ALQueryKind::Occlusion));
    CHECK(queries.getNameCount() == 1);

    U64 count = 99;
    // No producer attachment means the draw was not accepted by a native
    // command buffer. Expiring it is the fail-open outcome.
    CHECK(queries.result(ticket, count) == ALQueryPool::Status::Expired);
    CHECK(count == 0);
    count = 99;
    CHECK_FALSE(queries.wait(ticket, count));
    CHECK(count == 0);

    queries.reserve(ALQueryKind::Occlusion, true);
    CHECK_FALSE(queries.canBegin(ALQueryKind::Occlusion));
    CHECK_FALSE(queries.begin(ALQueryKind::Occlusion).isValid());
    ALQueryTicket reserved =
        queries.beginReserved(ALQueryKind::OcclusionExact);
    REQUIRE(reserved.isValid());
    queries.end(reserved);
    queries.reserve(ALQueryKind::Occlusion, false);

    ALQueryTicket discarded = queries.begin(ALQueryKind::Occlusion);
    REQUIRE(discarded.isValid());
    queries.discard(discarded);
    CHECK(queries.canBegin(ALQueryKind::Occlusion));
    CHECK(queries.result(discarded, count) ==
          ALQueryPool::Status::Expired);

    queries.endVisibilityCapture();
    CHECK_FALSE(queries.visibilityCaptureActive());
    CHECK_FALSE(queries.canBegin(ALQueryKind::Occlusion));

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
