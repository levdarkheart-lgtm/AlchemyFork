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
