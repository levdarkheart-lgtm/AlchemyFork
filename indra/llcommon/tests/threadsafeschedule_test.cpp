/**
 * @file   threadsafeschedule_test.cpp
 * @author Nat Goodspeed
 * @date   2021-10-04
 * @brief  Test for threadsafeschedule.
 *
 * $LicenseInfo:firstyear=2021&license=viewerlgpl$
 * Copyright (c) 2021, Linden Research, Inc.
 * $/LicenseInfo$
 */

// Precompiled header
#include "linden_common.h"
// associated header
#include "threadsafeschedule.h"
// STL headers
// std headers
#include <chrono>
#include <future>
// external library headers
// other Linden headers
#include <catch2/catch_test_macros.hpp>

using namespace std::literals::chrono_literals; // ms suffix
using namespace std::literals::string_literals; // s suffix
using Queue = LL::ThreadSafeSchedule<std::string>;

/*****************************************************************************
*   Tests
*****************************************************************************/
namespace
{
    struct threadsafeschedule_data
    {
        Queue queue;
    };

    // Observe the first future head while the consumer holds the queue lock.
    // A producer notified here cannot insert until the consumer begins waiting.
    class ObservedSchedule: public Queue
    {
    public:
        mutable std::promise<void> futureHead;

    private:
        bool canPop(const TimeTuple& head) const override
        {
            const bool due = std::get<0>(head) <= Clock::now();
            if (!due && !mObserved)
            {
                mObserved = true;
                futureHead.set_value();
            }
            return due;
        }

        mutable bool mObserved = false;
    };

    TEST_CASE_METHOD(threadsafeschedule_data, "threadsafeschedule: a caller deadline preserves future work and output", "[threadsafeschedule]")
    {
        const auto now = Queue::Clock::now();
        const auto due = now + 400ms;
        queue.push(due, "future"s);
        const Queue::TimeTuple untouched(now - 1s, "unchanged"s);
        auto entry = untouched;

        CHECK_FALSE(queue.tryPopUntil(now + 40ms, entry));
        CHECK(entry == untouched);
        CHECK(Queue::Clock::now() < due);
        CHECK(queue.size() == 1);

        // Retire the retained item without leaving a future wait in the test.
        queue.close();
        if (!queue.done())
        {
            CHECK(queue.popWithTime() == Queue::TimeTuple(due, "future"s));
        }
        CHECK(queue.done());
    }

    TEST_CASE("threadsafeschedule: inserting an earlier future head updates the pending wait", "[threadsafeschedule]")
    {
        ObservedSchedule queue;
        auto observed = queue.futureHead.get_future();
        const auto originalDue = Queue::Clock::now() + 1200ms;
        queue.push(originalDue, "later"s);
        auto consumer = std::async(std::launch::async, [&queue, originalDue]
        {
            std::string entry;
            const bool popped = queue.tryPopUntil(originalDue + 1s, entry);
            return std::make_tuple(popped, std::move(entry), Queue::Clock::now());
        });
        REQUIRE(observed.wait_for(500ms) == std::future_status::ready);

        const auto earlierDue = Queue::Clock::now() + 80ms;
        queue.push(earlierDue, "earlier"s);
        // Allow ample scheduling slack, while staying well before the old head.
        CHECK(consumer.wait_until(earlierDue + 250ms) == std::future_status::ready);
        const auto [popped, entry, returnedAt] = consumer.get();
        CHECK(popped);
        CHECK(entry == "earlier"s);
        CHECK(returnedAt >= earlierDue);
        CHECK(returnedAt < originalDue);
        CHECK(queue.size() == 1);
    }

    TEST_CASE_METHOD(threadsafeschedule_data, "threadsafeschedule: expired deadlines still allow due items and closed queues drain", "[threadsafeschedule]")
    {
        const auto past = Queue::Clock::now() - 1s;
        queue.push(past, "due"s);
        queue.close();
        std::string entry = "unchanged"s;
        CHECK(queue.tryPopUntil(past, entry));
        CHECK(entry == "due"s);
        CHECK(queue.done());

        entry = "unchanged"s;
        CHECK_FALSE(queue.tryPopUntil(past, entry));
        CHECK(entry == "unchanged"s);
        CHECK_THROWS_AS(queue.pop(), Queue::Closed);
    }

    TEST_CASE_METHOD(threadsafeschedule_data, "threadsafeschedule: entries pop in timestamp order and a closed queue drains", "[threadsafeschedule]")
    {
        // Simply calling push() a few times might result in indeterminate
        // delivery order if the resolution of steady_clock is coarser than
        // the real time required for each push() call. Explicitly increment
        // the timestamp for each one -- but since we're passing explicit
        // timestamps, make the queue reorder them.
        auto now{ Queue::Clock::now() };
        queue.push(Queue::TimeTuple(now + 200ms, "ghi"s));
        // Given the various push() overloads, you have to match the type
        // exactly: conversions are ambiguous.
        queue.push(now, "abc"s);
        queue.push(now + 100ms, "def"s);
        queue.close();
        auto entry = queue.pop();
        CHECK(std::get<0>(entry) == "abc"s);
        entry = queue.pop();
        CHECK(std::get<0>(entry) == "def"s);
        CHECK(queue.isClosed());
        CHECK_FALSE(queue.done());
        std::string s;
        bool popped = queue.tryPopFor(1s, s);
        UNSCOPED_INFO("failed to pop third");
        CHECK(popped);
        CHECK(s == "ghi"s);
        popped = queue.tryPop(s);
        UNSCOPED_INFO("queue not empty");
        CHECK_FALSE(popped);
        CHECK(queue.done());
    }
} // anonymous namespace
