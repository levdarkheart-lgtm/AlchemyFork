/**
 * @file altexturepressurequeue_test.cpp
 * @brief Unit tests for native-first texture-pressure queue draining.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include <catch2/catch_test_macros.hpp>

#include "../altexturepressurequeue.h"
#include "alnativeframebudget.h"

#include <queue>
#include <vector>

namespace
{
    struct Entry
    {
        int id;
        bool native;
    };

    using Queue = std::queue<Entry>;

    Queue makeQueue(std::initializer_list<Entry> entries)
    {
        Queue queue;
        for (Entry entry : entries)
        {
            queue.push(entry);
        }
        return queue;
    }

    std::vector<int> ids(Queue queue)
    {
        std::vector<int> result;
        while (!queue.empty())
        {
            result.push_back(queue.front().id);
            queue.pop();
        }
        return result;
    }
}

TEST_CASE("ALTexturePressureQueue drains native entries and preserves retained order",
          "[texture][pressure]")
{
    Queue queue = makeQueue({ { 1, false }, { 2, true }, { 3, false },
                              { 4, true }, { 5, false } });
    std::vector<int> drained;

    const auto result = ALTexturePressureQueue::drainNative(
        queue, 0,
        [](const Entry& entry) { return entry.native; },
        [&](const Entry& entry) { drained.push_back(entry.id); },
        [] { return false; });

    CHECK(result == ALTexturePressureQueue::DrainResult::Complete);
    CHECK(drained == std::vector<int>{ 2, 4 });
    CHECK(ids(queue) == std::vector<int>{ 1, 3, 5 });
}

TEST_CASE("ALTexturePressureQueue timeout leaves unexamined native work pending without reordering",
          "[texture][pressure]")
{
    Queue queue = makeQueue({ { 1, false }, { 2, true }, { 3, false },
                              { 4, true }, { 5, false } });
    std::vector<int> drained;

    auto result = ALTexturePressureQueue::drainNative(
        queue, 2,
        [](const Entry& entry) { return entry.native; },
        [&](const Entry& entry) { drained.push_back(entry.id); },
        [] { return true; });

    REQUIRE(result == ALTexturePressureQueue::DrainResult::BudgetExhausted);
    CHECK(drained == std::vector<int>{ 2 });
    CHECK(ids(queue) == std::vector<int>{ 1, 3, 4, 5 });

    result = ALTexturePressureQueue::drainNative(
        queue, 0,
        [](const Entry& entry) { return entry.native; },
        [&](const Entry& entry) { drained.push_back(entry.id); },
        [] { return false; });

    CHECK(result == ALTexturePressureQueue::DrainResult::Complete);
    CHECK(drained == std::vector<int>{ 2, 4 });
    CHECK(ids(queue) == std::vector<int>{ 1, 3, 5 });
}

TEST_CASE("ALTexturePressureQueue honors its minimum scan count after budget expiry",
          "[texture][pressure]")
{
    Queue queue = makeQueue({ { 1, false }, { 2, false }, { 3, true },
                              { 4, true } });
    std::vector<int> drained;

    const auto result = ALTexturePressureQueue::drainNative(
        queue, 3,
        [](const Entry& entry) { return entry.native; },
        [&](const Entry& entry) { drained.push_back(entry.id); },
        [] { return true; });

    CHECK(result == ALTexturePressureQueue::DrainResult::BudgetExhausted);
    CHECK(drained == std::vector<int>{ 3 });
    CHECK(ids(queue) == std::vector<int>{ 1, 2, 4 });
}

TEST_CASE("ALTexturePressureQueue keeps admission-denied native requests pending in order",
          "[texture][pressure][native-maintenance][PPD-277]")
{
    using Budget=ALNativeFrameBudget;
    Budget::Limits limits;limits.maintenance_per_frame=limits.maintenance_in_flight=1;
    Budget budget(limits);Budget::FrameInputs input;input.frame=1;input.os_available_bytes=0;budget.beginFrame(input);
    Queue queue=makeQueue({{1,true},{2,false},{3,true},{4,true}});
    std::vector<int> consumed;std::vector<Budget::Lease> native_commands;
    const auto drain=[&](const Entry& entry)
    {
        auto decision=budget.tryAcquire(Budget::Work::native_maintenance,4);
        if(!decision)return ALTexturePressureQueue::NativeResult::deferred;
        consumed.push_back(entry.id);native_commands.push_back(std::move(decision.credit));
        return ALTexturePressureQueue::NativeResult::consumed;
    };
    const auto run=[&]
    {return ALTexturePressureQueue::drainNative(queue,0,[](const Entry& entry){return entry.native;},drain,[]{return false;});};
    CHECK(run()==ALTexturePressureQueue::DrainResult::Complete);CHECK(consumed==std::vector<int>{1});CHECK(ids(queue)==std::vector<int>{2,3,4});
    input.frame=2;budget.beginFrame(input);CHECK(run()==ALTexturePressureQueue::DrainResult::Complete);
    CHECK(consumed==std::vector<int>{1});CHECK(ids(queue)==std::vector<int>{2,3,4}); // Prior command still owns credit.
    native_commands.clear();input.frame=3;budget.beginFrame(input);CHECK(run()==ALTexturePressureQueue::DrainResult::Complete);
    CHECK(consumed==std::vector<int>{1,3});CHECK(ids(queue)==std::vector<int>{2,4});
    native_commands.clear();input.frame=4;budget.beginFrame(input);CHECK(run()==ALTexturePressureQueue::DrainResult::Complete);
    CHECK(consumed==std::vector<int>{1,3,4});CHECK(ids(queue)==std::vector<int>{2});
}

TEST_CASE("ALTexturePressureQueue does not retry a deferred entry within the same traversal",
          "[texture][pressure][native-maintenance][PPD-277]")
{
    Queue queue=makeQueue({{1,true},{2,false},{3,true},{4,true}});std::vector<int> visited;
    const auto result=ALTexturePressureQueue::drainNative(queue,2,[](const Entry& entry){return entry.native;},
        [&](const Entry& entry){visited.push_back(entry.id);return ALTexturePressureQueue::NativeResult::deferred;},[]{return true;});
    CHECK(result==ALTexturePressureQueue::DrainResult::BudgetExhausted);CHECK(visited==std::vector<int>{1});
    CHECK(ids(queue)==std::vector<int>{1,2,3,4});
}
