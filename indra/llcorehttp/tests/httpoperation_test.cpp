/**
 * @file httpoperation_test.cpp
 * @brief unit tests for the LLCore::HttpOperation-derived classes
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
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
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */
#include "linden_common.h"

#include <catch2/catch_test_macros.hpp>

#include "_httpoperation.h"
#include "_httpreplyqueue.h"
#include "httphandler.h"

#include <iostream>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>


using namespace LLCore;
using namespace LLCoreInt;


namespace
{

class TestHandler : public LLCore::HttpHandler
{
public:
    virtual void onCompleted(HttpHandle, HttpResponse *)
        {
            std::cout << "TestHandler::onCompleted() invoked" << std::endl;
        }

};


}  // end namespace anonymous


namespace
{
    TEST_CASE("HttpOperation Tests: a new HttpOpNull has one reference", "[HttpOperation_Tests]")
    {
        // create a new ref counted object with an implicit reference
        HttpOperation::ptr_t op (new HttpOpNull());
        CHECK(op.use_count() == 1);

        // release the implicit reference, causing the object to be released
        op.reset();
    }

    TEST_CASE("HttpOperation Tests: setting a reply handler adds no reference to the op", "[HttpOperation_Tests]")
    {
        // Get some handlers
        LLCore::HttpHandler::ptr_t h1 (new TestHandler());

        // create a new ref counted object with an implicit reference
        HttpOperation::ptr_t op (new HttpOpNull());

        // Add the handlers
        op->setReplyPath(LLCore::HttpOperation::HttpReplyQueuePtr_t(), h1);

        // Check ref count
        CHECK(op.use_count() == 1);

        // release the reference, releasing the operation but
        // not the handlers.
        op.reset();

        // release the handlers
        h1.reset();
    }

    TEST_CASE("HttpOperation Tests: a reply lets go of the reply queue", "[HttpOperation_Tests]")
    {
        // The queue holds a reply until it is fetched. Were the op to keep holding the
        // queue, the two would keep each other alive once the request that made the
        // queue went with the reply unfetched, as an event poll's last cancel does.
        auto queue = std::make_shared<HttpReplyQueue>();
        std::weak_ptr<HttpReplyQueue> watch(queue);
        HttpOperation::ptr_t op(new HttpOpNull());
        op->setReplyPath(queue, LLCore::HttpHandler::ptr_t());
        CHECK(queue.use_count() == 2);

        op->stageFromRequest(nullptr);  // HttpOpNull bounces straight to its reply
        CHECK(queue.use_count() == 1);
        CHECK(op.use_count() == 2);     // its own and the queue's

        op.reset();
        queue.reset();
        CHECK(watch.expired());
    }

}

TEST_CASE("HTTP reply wake: late installation signals retained replies and clear detaches", "[HttpReplyWake]")
{
    auto queue = std::make_shared<HttpReplyQueue>();
    auto first = std::make_shared<HttpOpNull>();
    first->setReplyPath(queue, {});
    first->stageFromRequest(nullptr);
    int signals = 0;
    queue->setReplyNotification([&]() noexcept { ++signals; });
    CHECK(signals == 1);
    queue->setReplyNotification({});
    auto second = std::make_shared<HttpOpNull>();
    second->setReplyPath(queue, {});
    second->stageFromRequest(nullptr);
    CHECK(signals == 1);
    CHECK(queue->fetchOp() == first);
    CHECK(queue->fetchOp() == second);
    CHECK_FALSE(queue->fetchOp());
}

TEST_CASE("HTTP reply wake: clearing synchronizes an in-flight producer before target teardown", "[HttpReplyWake]")
{
    using namespace std::chrono_literals;
    auto queue = std::make_shared<HttpReplyQueue>();
    std::atomic<bool> entered{false}, release{false};
    std::atomic<int> signals{0};
    queue->setReplyNotification([&]() noexcept
    {
        ++signals;
        entered.store(true, std::memory_order_release);
        // A bounded scheduling gate only for this teardown race fixture. Real
        // notifications must be nonblocking owner-event signals.
        const auto until = std::chrono::steady_clock::now() + 500ms;
        while (!release.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < until)
            std::this_thread::yield();
    });
    auto op = std::make_shared<HttpOpNull>();
    op->setReplyPath(queue, {});
    std::jthread producer([&] { op->stageFromRequest(nullptr); });
    const auto until = std::chrono::steady_clock::now() + 1s;
    while (!entered.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < until)
        std::this_thread::yield();
    CHECK(entered.load(std::memory_order_acquire));
    auto clear = std::async(std::launch::async, [&] { queue->setReplyNotification({}); });
    CHECK(clear.wait_for(10ms) == std::future_status::timeout);
    release.store(true, std::memory_order_release);
    CHECK(clear.wait_for(1s) == std::future_status::ready);
    clear.get();
    producer.join();
    CHECK(signals == 1);
    auto later = std::make_shared<HttpOpNull>();
    later->setReplyPath(queue, {});
    later->stageFromRequest(nullptr);
    CHECK(signals == 1);
}
