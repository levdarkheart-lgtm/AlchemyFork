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
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <latch>
#include <memory>
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
    explicit Queue(bool housekeeping = false, size_t requests = 1024 * 1024,
                   size_t callbacks = 1024 * 1024, bool paused = false)
        : LLQueuedThread("queued lifecycle", true, paused, requests, callbacks),
          mHousekeeping(housekeeping) {}
    ~Queue() override { shutdown(); }
    bool submit(U32 handle, Counts& counts, std::function<bool()> body, bool event = false, U32 flags = 0)
    {
        auto* request = new Request(handle, counts, std::move(body), event, flags);
        if (addRequest(request)) return true;
        request->dispose();
        return false;
    }
    bool one() { return processOne(); }
    void ready() { processPending(); }
    bool callback(std::function<void()> body) { return mRequestQueue.tryPost(std::move(body)); }
    bool postCallback(std::function<void()> body) { return mRequestQueue.post(std::move(body)); }
    void closeQueue() { mRequestQueue.close(); }
    size_t scheduled()
    {
        lockData();
        const auto size = mSchedule.size();
        unlockData();
        return size;
    }
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
    Queue queue(false, 2, 1);
    std::vector<int> order;
    REQUIRE(queue.submit(1, waiting, [&] { order.push_back(1); return waiting.calls == 2; }));
    queue.one();
    REQUIRE(queue.submit(2, ready, [&] { order.push_back(2); return true; }));
    queue.one();
    CHECK(order == std::vector<int>{1, 2});
    for (int i = 0; i < 2000; ++i) queue.wakeRequest(1);
    CHECK(queue.getPending() == 1);
    CHECK(queue.scheduled() == 1);
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
    Queue queue(false, 1, 1);
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
    Queue queue(false, 1, 1);
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
    Queue queue(false, 1, 1);
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
    Queue queue(false, 11, 1);
    std::vector<int> order;
    for (int i = 0; i < 11; ++i)
        REQUIRE(queue.submit(i + 1, counts[i], [&, i] { order.push_back(i); return true; }));
    for (int i = 2; i < 11; ++i)
        for (int repeat = 0; repeat < 100; ++repeat) queue.promoteRequest(i + 1);
    CHECK(queue.getPending() == 11);
    CHECK(queue.scheduled() == 11);
    queue.ready();
    CHECK(queue.scheduled() == 0);
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

TEST_CASE("queued thread: full new admission returns ownership without waiting for service", "[queuedthread]")
{
    Counts first, rejected, retained, next;
    Queue queue(false, 1, 1);
    REQUIRE(queue.submit(1, first, [] { return true; }));
    CHECK(queue.getPending() == 1);
    CHECK(queue.scheduled() == 1);

    // If an old implementation blocks in capacity admission, close releases
    // that wait before joining. A failed assertion must never strand a thread.
    auto admission = std::async(std::launch::async, [&]
    {
        return queue.submit(2, rejected, [] { return true; });
    });
    const bool returned = admission.wait_for(500ms) == std::future_status::ready;
    if (!returned) queue.closeQueue();
    const bool accepted = admission.get();
    CHECK(returned);
    CHECK_FALSE(accepted);
    CHECK(rejected.calls == 0);
    CHECK(rejected.finished == 0);
    CHECK(rejected.deleted == 1);
    if (!returned) return;

    REQUIRE(queue.one());
    CHECK(first.finished == 1);
    CHECK(first.complete);
    CHECK(queue.getPending() == 0);
    CHECK(queue.scheduled() == 0);
    // A terminal result still occupies its accepted-request slot until reaped.
    CHECK_FALSE(queue.submit(3, retained, [] { return true; }));
    CHECK(retained.deleted == 1);
    CHECK(retained.finished == 0);
    REQUIRE(queue.completeRequest(1));
    CHECK(first.deleted == 1);
    REQUIRE(queue.submit(4, next, [] { return true; }));
    CHECK(queue.getPending() == 1);
    REQUIRE(queue.one());
    CHECK(next.finished == 1);
    CHECK(queue.completeRequest(4));
}

TEST_CASE("queued thread: even service post returns promptly at callback capacity", "[queuedthread]")
{
    int calls = 0;
    Queue queue(false, 1, 1);
    REQUIRE(queue.callback([&] { ++calls; }));
    auto post = std::async(std::launch::async, [&]
    {
        return queue.postCallback([&] { ++calls; });
    });
    const bool returned = post.wait_for(500ms) == std::future_status::ready;
    if (!returned) queue.closeQueue();
    const bool accepted = post.get();
    CHECK(returned);
    CHECK_FALSE(accepted);
    if (!returned) return;

    CHECK(queue.getPending() == 1);
    REQUIRE(queue.one());
    CHECK(calls == 1);
    CHECK(queue.getPending() == 0);
    REQUIRE(queue.postCallback([&] { ++calls; }));
    queue.ready();
    CHECK(calls == 2);
}

TEST_CASE("queued thread: a full callback queue cannot block or discard an accepted retry", "[queuedthread]")
{
    Counts counts;
    std::vector<int> order;
    bool posted = false;
    Queue queue(false, 1, 1);
    REQUIRE(queue.submit(1, counts, [&]
    {
        if (counts.calls == 1)
        {
            order.push_back(1);
            posted = queue.callback([&] { order.push_back(2); });
            return false;
        }
        order.push_back(3);
        return true;
    }, true));

    auto first = std::async(std::launch::async, [&] { return queue.one(); });
    const bool returned = first.wait_for(500ms) == std::future_status::ready;
    if (!returned) queue.closeQueue();
    const bool ran = first.get();
    CHECK(returned);
    CHECK(ran);
    if (!returned) return;

    REQUIRE(posted);
    CHECK(counts.calls == 1);
    CHECK(counts.finished == 0);
    CHECK(queue.getPending() == 2);
    CHECK(queue.scheduled() == 1);
    for (int i = 0; i < 2000; ++i)
    {
        queue.wakeRequest(1);
        queue.promoteRequest(1);
    }
    // One reusable logical request and one callback, with no stale timers.
    CHECK(queue.getPending() == 2);
    CHECK(queue.scheduled() == 1);
    queue.ready();
    CHECK(order == std::vector<int>{1, 2, 3});
    CHECK(counts.calls == 2);
    CHECK(counts.finished == 1);
    CHECK(counts.complete);
    CHECK(queue.getPending() == 0);
    CHECK(queue.scheduled() == 0);
    CHECK(queue.completeRequest(1));
    CHECK(counts.deleted == 1);
}

TEST_CASE("queued thread: callback and request service alternate while both stay ready", "[queuedthread]")
{
    Counts counts[6];
    std::vector<char> order;
    std::function<void()> callback;
    int calls = 0;
    bool reposted = true;
    Queue queue(false, 6, 1);
    callback = [&]
    {
        order.push_back('c');
        if (++calls < 6) reposted = queue.callback(callback) && reposted;
    };
    REQUIRE(queue.callback(callback));
    for (int i = 0; i < 6; ++i)
    {
        REQUIRE(queue.submit(i + 1, counts[i], [&] { order.push_back('r'); return true; }));
    }
    CHECK(queue.getPending() == 7);
    queue.ready();
    REQUIRE(order.size() == 12);
    CHECK(reposted);
    CHECK(calls == 6);
    for (size_t i = 1; i < order.size(); ++i) CHECK(order[i] != order[i - 1]);
    CHECK(queue.getPending() == 0);
    for (int i = 0; i < 6; ++i)
    {
        CHECK(counts[i].calls == 1);
        CHECK(counts[i].finished == 1);
        CHECK(queue.completeRequest(i + 1));
    }
}

TEST_CASE("queued thread: concurrent producers wake native service through a tiny callback queue", "[queuedthread]")
{
    constexpr size_t producers = 2;
    constexpr size_t perProducer = 256;
    std::array<std::atomic<unsigned>, producers * perProducer> executions{};
    std::atomic<size_t> completed{0};
    std::latch waiting(producers), start(1);
    Queue queue(false, 1, 2);
    queue.start();
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    std::array<std::future<size_t>, producers> submissions;
    for (size_t producer = 0; producer < producers; ++producer)
    {
        submissions[producer] = std::async(std::launch::async, [&, producer]
        {
            waiting.count_down();
            start.wait();
            size_t accepted = 0;
            while (accepted < perProducer && std::chrono::steady_clock::now() < deadline)
            {
                const auto id = producer * perProducer + accepted;
                if (queue.callback([&, id]
                {
                    executions[id].fetch_add(1, std::memory_order_relaxed);
                    completed.fetch_add(1, std::memory_order_release);
                }))
                {
                    ++accepted;
                }
                else std::this_thread::yield();
            }
            return accepted;
        });
    }
    waiting.wait();
    start.count_down();
    std::array<size_t, producers> accepted;
    size_t total = 0;
    for (size_t producer = 0; producer < producers; ++producer)
    {
        accepted[producer] = submissions[producer].get();
        total += accepted[producer];
    }
    // Observe native progress before close can supply an extra wake or drain.
    // No frame update or manual pump participates in this test.
    const bool progressed = eventually([&]
    {
        return completed.load(std::memory_order_acquire) == total;
    });
    queue.shutdown();

    // Producers have joined and the owner has drained before any assertion.
    CHECK(progressed);
    CHECK(queue.isStopped());
    CHECK(queue.getPending() == 0);
    for (size_t producer = 0; producer < producers; ++producer)
    {
        CHECK(accepted[producer] == perProducer);
        for (size_t i = 0; i < perProducer; ++i)
        {
            const auto id = producer * perProducer + i;
            CHECK(executions[id].load(std::memory_order_relaxed) == (i < accepted[producer] ? 1u : 0u));
        }
    }
}

TEST_CASE("queued thread: coalesced housekeeping progresses outside full callback admission", "[queuedthread]")
{
    Counts counts;
    int callbacks = 0;
    Queue queue(true, 1, 1);
    REQUIRE(queue.submit(1, counts, [] { return true; }));
    REQUIRE(queue.callback([&] { ++callbacks; }));
    for (int i = 0; i < 2000; ++i) queue.update(0);
    CHECK(queue.getPending() == 3);
    queue.ready();
    CHECK(callbacks == 1);
    CHECK(queue.updates == 1);
    CHECK(counts.finished == 1);
    CHECK(queue.getPending() == 0);
    CHECK(queue.completeRequest(1));
    queue.closeQueue();
    CHECK(queue.isAdmissionClosed());
    queue.update(0);
    CHECK(queue.getPending() == 0);
}

TEST_CASE("queued thread: paused never-started shutdown drains callbacks and aborts requests once", "[queuedthread]")
{
    Counts counts[2];
    int callbacks = 0;
    auto pin = std::make_shared<int>(7);
    std::weak_ptr<int> weak = pin;
    Queue queue(false, 2, 1, true);
    for (int i = 0; i < 2; ++i)
        REQUIRE(queue.submit(i + 1, counts[i], [] { return true; }));
    REQUIRE(queue.callback([held = pin, &callbacks]
    {
        if (*held == 7) ++callbacks;
    }));
    pin.reset();
    CHECK_FALSE(weak.expired());
    CHECK(queue.getPending() == 3);
    queue.shutdown();
    CHECK(queue.isStopped());
    CHECK(queue.isAdmissionClosed());
    CHECK(callbacks == 1);
    CHECK(weak.expired());
    CHECK(queue.getPending() == 0);
    for (const auto& count : counts)
    {
        CHECK(count.calls == 0);
        CHECK(count.finished == 1);
        CHECK_FALSE(count.complete);
        CHECK(count.deleted == 1);
    }
    queue.shutdown();
    CHECK(callbacks == 1);
    for (const auto& count : counts)
    {
        CHECK(count.finished == 1);
        CHECK(count.deleted == 1);
    }
}

TEST_CASE("queued thread: closed admission retains caller ownership and aborted waits terminate", "[queuedthread]")
{
    Counts rejected, aborted;
    Queue queue(false, 2, 1);
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
    int callbacks = 0;
    Queue queue(false, 2, 1);
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
    const bool posted = queue.callback([&] { ++callbacks; });
    if (!posted) release.store(true, std::memory_order_release);
    REQUIRE(posted);
    CHECK(queue.getPending() == 3);
    std::thread releaser([&] { std::this_thread::sleep_for(20ms); release.store(true, std::memory_order_release); });
    queue.shutdown();
    releaser.join();
    CHECK(queue.isStopped());
    CHECK(callbacks == 1);
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
