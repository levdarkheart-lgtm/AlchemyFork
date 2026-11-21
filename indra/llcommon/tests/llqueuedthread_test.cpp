/**
 * @file llqueuedthread_test.cpp
 * @brief Request identity, service fairness and shutdown lifecycle.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "llqueuedthread.h"
#include <catch2/catch_test_macros.hpp>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

namespace
{
using namespace std::chrono_literals;
struct Counts { int calls = 0, finished = 0, deleted = 0; bool complete = false; };
class Request final : public LLQueuedThread::QueuedRequest
{
public:
    Request(U32 handle, Counts& counts, std::function<bool()> body, bool event, U32 flags)
        : QueuedRequest(handle, flags), mCounts(counts), mBody(std::move(body)), mEvent(event) {}
    ~Request() override { ++mCounts.deleted; }
    void dispose() { deleteRequest(); }
private:
    bool processRequest() override { ++mCounts.calls; return mBody(); }
    void finishRequest(bool complete) override { ++mCounts.finished; mCounts.complete = complete; }
    bool waitsForWake() const override { return mEvent; }
    Counts& mCounts;
    std::function<bool()> mBody;
    bool mEvent;
};
class Queue final : public LLQueuedThread
{
public:
    explicit Queue(bool housekeeping = false) : LLQueuedThread("queued lifecycle"), mHousekeeping(housekeeping) {}
    ~Queue() override { shutdown(); }
    bool submit(U32 handle, Counts& counts, std::function<bool()> body, bool event = false, U32 flags = 0)
    {
        auto* request = new Request(handle, counts, std::move(body), event, flags);
        if (addRequest(request)) return true;
        request->dispose();
        return false;
    }
    bool one() { return mRequestQueue.runOne(); }
    void ready() { mRequestQueue.runPending(); }
    void closeQueue() { mRequestQueue.close(); }
    int updates = 0;
private:
    bool hasThreadedUpdate() const override { return mHousekeeping; }
    void threadedUpdate() override { ++updates; }
    bool mHousekeeping;
};
template<class PRED> bool eventually(PRED pred)
{
    const auto limit = std::chrono::steady_clock::now() + 2s;
    while (!pred())
    {
        if (std::chrono::steady_clock::now() > limit) return false;
        std::this_thread::yield();
    }
    return true;
}

TEST_CASE("queued thread: a delayed retry leaves ready work runnable and coalesces wakes", "[queuedthread]")
{
    Counts waiting, ready;
    Queue queue;
    std::vector<int> order;
    REQUIRE(queue.submit(1, waiting, [&] { order.push_back(1); return waiting.calls == 2; }));
    queue.one();
    REQUIRE(queue.submit(2, ready, [&] { order.push_back(2); return true; }));
    queue.one();
    CHECK(order == std::vector<int>{1, 2});
    for (int i = 0; i < 2000; ++i) queue.wakeRequest(1);
    CHECK(queue.getPending() <= 2);
    queue.ready();
    CHECK(order == std::vector<int>{1, 2, 1});
    CHECK(waiting.finished == 1);
    CHECK(waiting.complete);
    CHECK(queue.completeRequest(1));
    CHECK(queue.completeRequest(2));
    std::this_thread::sleep_for(25ms);
    queue.ready();
    CHECK(waiting.calls == 2);
    CHECK(waiting.deleted == 1);
}

TEST_CASE("queued thread: a wake during processing is consumed by the next retry", "[queuedthread]")
{
    Counts counts;
    Queue queue;
    REQUIRE(queue.submit(1, counts, [&]
    {
        if (counts.calls == 1) { queue.wakeRequest(1); return false; }
        return true;
    }, true));
    queue.ready();
    CHECK(counts.calls == 2);
    CHECK(counts.finished == 1);
    CHECK(queue.completeRequest(1));
}

TEST_CASE("queued thread: a stale timer cannot run a reused public handle", "[queuedthread]")
{
    Counts old, replacement;
    Queue queue;
    REQUIRE(queue.submit(7, old, [&] { return old.calls == 2; }, false, LLQueuedThread::FLAG_AUTO_COMPLETE));
    queue.one();
    queue.wakeRequest(7);
    queue.one();
    REQUIRE(old.deleted == 1);
    REQUIRE(queue.submit(7, replacement, [] { return false; }, true));
    queue.one();
    REQUIRE(replacement.calls == 1);
    std::this_thread::sleep_for(25ms);
    queue.ready();
    CHECK(replacement.calls == 1);
    queue.abortRequest(7, true);
    queue.ready();
    CHECK(replacement.finished == 1);
    CHECK_FALSE(replacement.complete);
    CHECK(replacement.deleted == 1);
}

TEST_CASE("queued thread: event-driven requests retain a finite fallback", "[queuedthread]")
{
    Counts counts;
    Queue queue;
    REQUIRE(queue.submit(1, counts, [&] { return counts.calls == 2; }, true));
    queue.one();
    std::this_thread::sleep_for(25ms);
    queue.ready();
    CHECK(counts.calls == 1);
    std::this_thread::sleep_for(250ms);
    queue.ready();
    CHECK(counts.calls == 2);
    CHECK(counts.finished == 1);
    CHECK(queue.completeRequest(1));
}

TEST_CASE("queued thread: promotions coalesce and cannot starve ordinary ready work", "[queuedthread]")
{
    Counts counts[11];
    Queue queue;
    std::vector<int> order;
    for (int i = 0; i < 11; ++i)
        REQUIRE(queue.submit(i + 1, counts[i], [&, i] { order.push_back(i); return true; }));
    for (int i = 2; i < 11; ++i)
        for (int repeat = 0; repeat < 100; ++repeat) queue.promoteRequest(i + 1);
    CHECK(queue.getPending() == 20);
    queue.ready();
    REQUIRE(order.size() == 11);
    const auto normal = std::find(order.begin(), order.end(), 0);
    CHECK(std::distance(order.begin(), normal) > 0);
    CHECK(std::distance(order.begin(), normal) <= 4);
    CHECK(normal < std::find(order.begin(), order.end(), 1));
    for (int i = 0; i < 11; ++i)
    {
        CHECK(counts[i].calls == 1);
        CHECK(counts[i].finished == 1);
        CHECK(queue.completeRequest(i + 1));
    }
}

TEST_CASE("queued thread: frame polls skip no-op housekeeping and coalesce useful updates", "[queuedthread]")
{
    {
        Queue no_op;
        for (int i = 0; i < 2000; ++i) no_op.update(0);
        CHECK(no_op.getPending() == 0);
        CHECK(no_op.updates == 0);
    }
    {
        Queue queue(true);
        for (int i = 0; i < 2000; ++i) queue.update(0);
        CHECK(queue.getPending() == 1);
        queue.ready();
        CHECK(queue.updates == 1);
        queue.update(0);
        queue.ready();
        CHECK(queue.updates == 2);
    }
}

TEST_CASE("queued thread: closed admission retains caller ownership and aborted waits terminate", "[queuedthread]")
{
    Counts rejected, aborted;
    Queue queue;
    REQUIRE(queue.submit(1, aborted, [] { return false; }, true));
    queue.one();
    queue.abortRequest(1, false);
    queue.ready();
    CHECK_FALSE(queue.waitForResult(1));
    CHECK(aborted.finished == 1);
    CHECK(aborted.deleted == 1);
    queue.closeQueue();
    CHECK_FALSE(queue.submit(2, rejected, [] { return true; }));
    CHECK(rejected.calls == 0);
    CHECK(rejected.finished == 0);
    CHECK(rejected.deleted == 1);
}

TEST_CASE("queued thread: shutdown waits for the active owner then aborts queued requests once", "[queuedthread]")
{
    Counts active, queued;
    std::atomic<bool> entered{false}, release{false};
    Queue queue;
    REQUIRE(queue.submit(1, active, [&]
    {
        entered.store(true, std::memory_order_release);
        while (!release.load(std::memory_order_acquire)) std::this_thread::yield();
        return false;
    }));
    REQUIRE(queue.submit(2, queued, [] { return true; }));
    queue.start();
    const bool running = eventually([&] { return entered.load(std::memory_order_acquire); });
    if (!running) release.store(true, std::memory_order_release);
    REQUIRE(running);
    std::thread releaser([&] { std::this_thread::sleep_for(20ms); release.store(true, std::memory_order_release); });
    queue.shutdown();
    releaser.join();
    CHECK(queue.isStopped());
    CHECK(active.calls == 1);
    CHECK(active.finished == 1);
    CHECK_FALSE(active.complete);
    CHECK(active.deleted == 1);
    CHECK(queued.calls == 0);
    CHECK(queued.finished == 1);
    CHECK_FALSE(queued.complete);
    CHECK(queued.deleted == 1);
    queue.shutdown();
}
}
