/**
 * @file llimageworker_test.cpp
 * @author Merov Linden
 * @date 2009-04-28
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

// Precompiled header: almost always required for newview cpp files
#include "linden_common.h"
// Class to test
#include "../llimageworker.h"
// For timer class
#include "../llcommon/lltimer.h"
// for lltrace class
#include "../llcommon/lltrace.h"
#include "threadpool.h"
// Catch2 header
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <vector>

// -------------------------------------------------------------------------------------------
// Stubbing: Declarations required to link and run the class being tested
// Notes:
// * Add here stubbed implementation of the few classes and methods used in the class to be tested
// * Add as little as possible (let the link errors guide you)
// * Do not make any assumption as to how those classes or methods work (i.e. don't copy/paste code)
// * A simulator for a class can be implemented here. Please comment and document thoroughly.

LLImageBase::LLImageBase()
: mData(NULL),
mDataSize(0),
mWidth(0),
mHeight(0),
mComponents(0),
mBadBufferAllocation(false),
mAllowOverSize(false)
{
}
LLImageBase::~LLImageBase() {}
void LLImageBase::dump() { }
void LLImageBase::sanityCheck() { }
void LLImageBase::deleteData() { }
U8* LLImageBase::allocateData(S32 size) { return NULL; }
U8* LLImageBase::reallocateData(S32 size) { return NULL; }

LLImageFormatted::LLImageFormatted(S8 codec)
    : LLImageBase(),
      mCodec(codec),
      mDecoding(0),
      mDecoded(0),
      mDiscardLevel(-1),
      mLevels(0)
{
}
LLImageFormatted::~LLImageFormatted(){}
void LLImageFormatted::dump() { }
void LLImageFormatted::sanityCheck() { }
void LLImageFormatted::deleteData() { }
U8* LLImageFormatted::allocateData(S32 size) { return NULL; }
U8* LLImageFormatted::reallocateData(S32 size) { return NULL; }
void LLImageFormatted::resetLastError() { }
void LLImageFormatted::setLastError(const std::string&, const std::string&) { }
S32 LLImageFormatted::calcDataSize(S32 discard_level) { return 0; }
S32 LLImageFormatted::calcDiscardLevelBytes(S32 bytes) { return 0; }
bool LLImageFormatted::decodeChannels(LLImageRaw* raw_image,F32  decode_time, S32 first_channel, S32 max_channel) { return false; }

LLImageRaw::LLImageRaw(U16 width, U16 height, S8 components) { }
LLImageRaw::~LLImageRaw() { }
void LLImageRaw::deleteData() { }
U8* LLImageRaw::allocateData(S32 size) { return NULL; }
U8* LLImageRaw::reallocateData(S32 size) { return NULL; }
const U8* LLImageBase::getData() const { return NULL; }
U8* LLImageBase::getData() { return NULL; }
const std::string& LLImage::getLastThreadError() { static std::string msg; return msg; }

// End Stubbing
// -------------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------
// Tests
// -------------------------------------------------------------------------------------------

namespace
{
    // Test wrapper declarations

    // Note: We derive the responder class for 2 reasons:
    // 1. It's a pure virtual class and we can't compile without completed() being implemented
    // 2. We actually need a responder to test that the thread work test completed
    // We implement this making no assumption on what's done in the thread or worker
    // though, just that the responder's completed() method is called in the end.
    // Note on responders: responders are ref counted and *will* be deleted by the request they are
    // attached to when the queued request is deleted. The recommended way of using them is to
    // create them when creating a request, put a callback method in completed() and not rely on
    // anything to survive in the responder object once completed() has been called. Let the request
    // do the deletion and clean up itself.
    class responder_test : public LLImageDecodeThread::Responder
    {
        public:
            responder_test(std::shared_ptr<std::atomic<bool>> res)
                : done(std::move(res))
            {
            }
            virtual void completed(bool success, const std::string& error_message, LLImageRaw* raw, LLImageRaw* aux, U32 request_id)
            {
                done->store(true);
            }
        private:
            // This is what can be thought of as the minimal implementation of a responder
            // Done will be switched to true when completed() is called and can be tested
            // outside the responder. A better way of doing this is to store a callback here.
            // completed() runs on a decode worker thread while the test polls the flag, so
            // the flag is atomic; and the responder shares ownership of it, so a responder
            // that completes after the test has given up waiting writes to a live flag
            // rather than into a stack frame that is gone.
            std::shared_ptr<std::atomic<bool>> done;
    };

    // Test wrapper declaration : decode thread
    struct imagedecodethread_test
    {
        // Instance to be tested
        LLImageDecodeThread* mThread;
        // Constructor and destructor of the test wrapper
        imagedecodethread_test()
        {
            mThread = NULL;
        }
        ~imagedecodethread_test()
        {
            delete mThread;
        }
    };

    // Block in a formatted image's header parser, reached by the production
    // ImageRequest. Each queued request gets its own image/data lock, while
    // the shared gate observes actual admission rather than scheduler math.
    struct decode_gate
    {
        std::mutex mutex;
        std::condition_variable changed;
        unsigned entered = 0;
        unsigned active = 0;
        unsigned peak = 0;
        bool released = false;

        void enter()
        {
            std::unique_lock<std::mutex> lock(mutex);
            ++entered;
            ++active;
            peak = std::max(peak, active);
            changed.notify_all();
            changed.wait(lock, [&] { return released; });
            --active;
        }

        bool waitFor(unsigned count)
        {
            std::unique_lock<std::mutex> lock(mutex);
            return changed.wait_for(lock, std::chrono::seconds(5),
                                    [&] { return entered >= count; });
        }

        void release()
        {
            std::lock_guard<std::mutex> lock(mutex);
            released = true;
            changed.notify_all();
        }
    };

    struct release_decode_gate
    {
        std::shared_ptr<decode_gate> gate;
        ~release_decode_gate() { gate->release(); }
    };

    class controlled_image final : public LLImageFormatted
    {
    public:
        explicit controlled_image(std::shared_ptr<decode_gate> gate)
            : LLImageFormatted(IMG_CODEC_J2C), mGate(std::move(gate)) {}
        std::string getExtension() override { return "controlled"; }
        bool updateData() override
        {
            mGate->enter();
            return false; // A malformed header must still complete once.
        }
        bool decode(LLImageRaw*, F32) override { return false; }
        bool encode(const LLImageRaw*, F32) override { return false; }
    private:
        std::shared_ptr<decode_gate> mGate;
    };

    class throwing_image final : public LLImageFormatted
    {
    public:
        throwing_image() : LLImageFormatted(IMG_CODEC_J2C) {}
        std::string getExtension() override { return "throwing"; }
        bool updateData() override
        {
            throw std::runtime_error("controlled decode failure");
        }
        bool decode(LLImageRaw*, F32) override { return false; }
        bool encode(const LLImageRaw*, F32) override { return false; }
    };

    struct completion_state
    {
        struct result
        {
            U32 id;
            bool success;
            std::string error;
        };
        std::mutex mutex;
        std::condition_variable changed;
        std::vector<result> results;
        unsigned returned = 0;
        bool block = false;
        bool released = false;

        void complete(U32 id, bool success, const std::string& error)
        {
            std::unique_lock<std::mutex> lock(mutex);
            results.push_back({id, success, error});
            changed.notify_all();
            changed.wait(lock, [&] { return !block || released; });
            ++returned;
            changed.notify_all();
        }

        bool waitFor(unsigned count, bool callbacks_returned = true)
        {
            std::unique_lock<std::mutex> lock(mutex);
            return changed.wait_for(lock, std::chrono::seconds(5), [&]
            {
                return callbacks_returned ? returned >= count : results.size() >= count;
            });
        }

        void release()
        {
            std::lock_guard<std::mutex> lock(mutex);
            released = true;
            changed.notify_all();
        }

        std::vector<result> copy()
        {
            std::lock_guard<std::mutex> lock(mutex);
            return results;
        }
    };

    struct release_completion
    {
        std::shared_ptr<completion_state> state;
        ~release_completion() { state->release(); }
    };

    class counting_responder final : public LLImageDecodeThread::Responder
    {
    public:
        explicit counting_responder(std::shared_ptr<completion_state> state)
            : mState(std::move(state)) {}
        void completed(bool success, const std::string& error,
                       LLImageRaw*, LLImageRaw*, U32 id) override
        {
            mState->complete(id, success, error);
        }
    private:
        std::shared_ptr<completion_state> mState;
    };

    class rejected_responder final : public LLImageDecodeThread::Responder
    {
    public:
        rejected_responder(std::shared_ptr<std::atomic<unsigned>> destroyed,
                           std::shared_ptr<completion_state> state)
            : mDestroyed(std::move(destroyed)), mState(std::move(state)) {}
        ~rejected_responder() override { ++*mDestroyed; }
        void completed(bool success, const std::string& error,
                       LLImageRaw*, LLImageRaw*, U32 id) override
        {
            mState->complete(id, success, error);
        }
    private:
        std::shared_ptr<std::atomic<unsigned>> mDestroyed;
        std::shared_ptr<completion_state> mState;
    };

    struct decode_clock
    {
        std::shared_ptr<std::atomic<std::int64_t>> milliseconds =
            std::make_shared<std::atomic<std::int64_t>>(0);
        ALImageDecodeScheduler::Now now() const
        {
            return [value = milliseconds]
            {
                return ALImageDecodeScheduler::TimePoint{} +
                       std::chrono::milliseconds(value->load());
            };
        }
        void set(std::int64_t value) { milliseconds->store(value); }
    };

    ALImageDecodeScheduler::Limits limits(unsigned baseline, unsigned maximum)
    {
        return {2, baseline, maximum, 8, 8, true};
    }

    std::set<U32> enqueue_blocked(LLImageDecodeThread& thread, unsigned count,
                                  const std::shared_ptr<decode_gate>& gate,
                                  const std::shared_ptr<completion_state>& completions)
    {
        std::set<U32> ids;
        for (unsigned index = 0; index < count; ++index)
        {
            LLPointer<LLImageFormatted> image = new controlled_image(gate);
            const auto id = thread.decodeImage(image, 0, false,
                                              new counting_responder(completions));
            CHECK(id != 0);
            CHECK(ids.insert(id).second);
        }
        return ids;
    }

    void check_failed_once(const std::shared_ptr<completion_state>& completions,
                           const std::set<U32>& expected)
    {
        const auto results = completions->copy();
        CHECK(results.size() == expected.size());
        std::set<U32> received;
        for (const auto& result : results)
        {
            CHECK_FALSE(result.success);
            CHECK(received.insert(result.id).second);
        }
        CHECK(received == expected);
    }


    // ---------------------------------------------------------------------------------------
    // Test functions
    // Notes:
    // * Test as many as you possibly can without requiring a full blown simulation of everything
    // ---------------------------------------------------------------------------------------

    // ---------------------------------------------------------------------------------------
    // Test the LLImageDecodeThread interface
    // ---------------------------------------------------------------------------------------

    TEST_CASE_METHOD(imagedecodethread_test, "LLImageDecodeThread: a threaded decodeImage calls the responder", "[LLImageDecodeThread]")
    {
        // Test a *threaded* instance of the class
        mThread = new LLImageDecodeThread(true);
        REQUIRE(mThread != NULL);
        // Insert something in the queue
        const auto done = std::make_shared<std::atomic<bool>>(false);
        LLImageDecodeThread::handle_t decodeHandle = mThread->decodeImage(NULL, 0, false, new responder_test(done));
        // Verifies we get back a valid handle
        CHECK(decodeHandle != 0);
        // Wait till the thread has time to handle the work order (though it doesn't do much per work order...)
        const U32 INCREMENT_TIME = 500;             // 500 milliseconds
        const U32 MAX_TIME = 20 * INCREMENT_TIME;   // Do the loop 20 times max, i.e. wait 10 seconds but no more
        U32 total_time = 0;
        while (!done->load() && (total_time < MAX_TIME))
        {
            ms_sleep(INCREMENT_TIME);
            total_time += INCREMENT_TIME;
        }
        // Verifies that the responder has now been called
        CHECK(done->load());
    }

    TEST_CASE_METHOD(imagedecodethread_test,
        "LLImageDecodeThread: pending includes admitted and waiting requests",
        "[LLImageDecodeThread]")
    {
        decode_clock clock;
        mThread = new LLImageDecodeThread(true, limits(2, 2), false, clock.now());
        const auto gate = std::make_shared<decode_gate>();
        release_decode_gate release{gate};
        const auto completed = std::make_shared<completion_state>();
        const auto ids = enqueue_blocked(*mThread, 6, gate, completed);
        REQUIRE(gate->waitFor(2));
        const auto stats = mThread->getDecodeStats();
        CHECK(stats.current == 2);
        CHECK(stats.active == 2);
        CHECK(stats.waiting == 4);
        CHECK(mThread->getPending() == 6);
        CHECK(completed->copy().empty());

        gate->release();
        mThread->shutdown();
        check_failed_once(completed, ids);
        const auto drained = mThread->getDecodeStats();
        CHECK(mThread->getPending() == 0);
        CHECK(drained.active == 0);
        CHECK(drained.waiting == 0);
        CHECK(drained.completed == 6);
        CHECK(drained.cancelled == 0);
    }

    TEST_CASE_METHOD(imagedecodethread_test,
        "LLImageDecodeThread: an admitted request remains pending through its responder",
        "[LLImageDecodeThread]")
    {
        decode_clock clock;
        mThread = new LLImageDecodeThread(true, limits(2, 2), false, clock.now());
        const auto completed = std::make_shared<completion_state>();
        completed->block = true;
        release_completion release{completed};
        const auto id = mThread->decodeImage(nullptr, 0, false,
                                            new counting_responder(completed));
        REQUIRE(id != 0);
        REQUIRE(completed->waitFor(1, false));
        const auto during_callback = mThread->getDecodeStats();
        CHECK(during_callback.active == 1);
        CHECK(during_callback.waiting == 0);
        CHECK(during_callback.completed == 0);
        CHECK(during_callback.decode_wall.count == 1);
        CHECK(mThread->getPending() == 1);

        clock.set(100);
        completed->release();
        mThread->shutdown();
        check_failed_once(completed, {id});
        CHECK(mThread->getPending() == 0);
        CHECK(mThread->getDecodeStats().service_wall.count == 1);
    }

    TEST_CASE_METHOD(imagedecodethread_test,
        "LLImageDecodeThread: shutdown drains permit waiters without another control tick",
        "[LLImageDecodeThread]")
    {
        decode_clock clock;
        mThread = new LLImageDecodeThread(true, limits(2, 4), true, clock.now());
        const auto gate = std::make_shared<decode_gate>();
        release_decode_gate release{gate};
        const auto completed = std::make_shared<completion_state>();
        const auto ids = enqueue_blocked(*mThread, 8, gate, completed);
        REQUIRE(gate->waitFor(2));
        CHECK(mThread->getDecodeStats().active == 2);
        auto closing = std::async(std::launch::async, [thread = mThread]
        {
            thread->shutdown();
        });
        // This guard must unwind before the future, whose destructor joins.
        release_decode_gate release_before_join{gate};
        REQUIRE(gate->waitFor(4));
        CHECK(mThread->getDecodeStats().draining);
        CHECK(mThread->getPending() == 8);
        CHECK(closing.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout);
        gate->release();
        REQUIRE(closing.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
        closing.get();
        check_failed_once(completed, ids);
        CHECK(mThread->getPending() == 0);
        CHECK(mThread->getDecodeStats().completed == 8);
        mThread->shutdown(); // Explicit close and destructor close are idempotent.
    }

    TEST_CASE_METHOD(imagedecodethread_test,
        "LLImageDecodeThread: a closed decoder rejects work without a phantom pending job",
        "[LLImageDecodeThread]")
    {
        mThread = new LLImageDecodeThread(true, limits(2, 2), false);
        mThread->shutdown();
        const auto before = mThread->getDecodeStats();
        const auto completed = std::make_shared<completion_state>();
        CHECK(mThread->decodeImage(nullptr, 0, false,
                                  new counting_responder(completed)) == 0);
        CHECK(completed->copy().empty());
        CHECK(mThread->getPending() == 0);
        const auto after = mThread->getDecodeStats();
        CHECK(after.submitted == before.submitted);
        CHECK(after.started == before.started);
        CHECK(after.completed == before.completed);
        CHECK(after.waiting == 0);
        CHECK(after.active == 0);
    }

    TEST_CASE_METHOD(imagedecodethread_test,
        "LLImageDecodeThread: a closed work queue cancels its admission ticket",
        "[LLImageDecodeThread]")
    {
        mThread = new LLImageDecodeThread(true, limits(2, 2), false);
        auto pool = LL::ThreadPoolBase::getInstance("ImageDecode");
        REQUIRE(pool);
        // Use the public pool close, which also joins its threads. Closing
        // only its queue would bypass that lifetime contract.
        pool->close();
        const auto before = mThread->getDecodeStats();
        REQUIRE_FALSE(before.draining);
        const auto completed = std::make_shared<completion_state>();
        const auto destroyed = std::make_shared<std::atomic<unsigned>>(0);
        CHECK(mThread->decodeImage(nullptr, 0, false,
              new rejected_responder(destroyed, completed)) == 0);
        const auto after = mThread->getDecodeStats();
        CHECK(after.submitted == before.submitted + 1);
        CHECK(after.cancelled == before.cancelled + 1);
        CHECK(after.started == before.started);
        CHECK(after.completed == before.completed);
        CHECK(after.waiting == 0);
        CHECK(after.active == 0);
        CHECK(mThread->getPending() == 0);
        CHECK(completed->copy().empty());
        CHECK(destroyed->load() == 1);
        mThread->shutdown();
    }

    TEST_CASE_METHOD(imagedecodethread_test,
        "LLImageDecodeThread: codec exceptions preserve WorkQueue propagation and release admission",
        "[LLImageDecodeThread]")
    {
        REQUIRE_FALSE(LL::WorkQueue::getInstance("mainloop"));
        LL::WorkQueue mainloop("mainloop", 1024, false);
        mThread = new LLImageDecodeThread(true, limits(2, 2), false);
        const auto completed = std::make_shared<completion_state>();
        LLPointer<LLImageFormatted> image = new throwing_image;
        const auto failed = mThread->decodeImage(image, 0, false,
                                                new counting_responder(completed));
        const auto next = mThread->decodeImage(nullptr, 0, false,
                                              new counting_responder(completed));
        REQUIRE(failed != 0);
        REQUIRE(next != 0);
        mThread->shutdown();
        // A codec exception is not a corrupt-texture result: the original
        // WorkQueue path forwards it to the main loop rather than calling
        // the responder with a failure that could evict a valid cache entry.
        check_failed_once(completed, {next});
        REQUIRE(mainloop.size() == 1);
        CHECK_THROWS_AS(mainloop.runOne(), std::runtime_error);
        CHECK(mainloop.size() == 0);
        const auto stats = mThread->getDecodeStats();
        CHECK(mThread->getPending() == 0);
        CHECK(stats.active == 0);
        CHECK(stats.waiting == 0);
        CHECK(stats.completed == 2);
        CHECK(stats.decode_wall.count == 1);
        CHECK(stats.service_wall.count == 2);
    }

    TEST_CASE_METHOD(imagedecodethread_test,
        "LLImageDecodeThread: control changes admit more work without cancelling active decodes",
        "[LLImageDecodeThread]")
    {
        decode_clock clock;
        mThread = new LLImageDecodeThread(true, limits(2, 4), true, clock.now());
        const auto gate = std::make_shared<decode_gate>();
        release_decode_gate release{gate};
        const auto completed = std::make_shared<completion_state>();
        const auto ids = enqueue_blocked(*mThread, 8, gate, completed);
        REQUIRE(gate->waitFor(2));
        ALImageDecodeScheduler::Pressure headroom;
        headroom.system_busy = 0.1;
        headroom.main_thread_busy = 0.1;
        headroom.cpu_valid = true;
        headroom.thermal_level = 0;
        headroom.low_power = false;
        clock.set(500);
        mThread->updateConcurrency(headroom);
        REQUIRE(gate->waitFor(3));
        clock.set(1000);
        mThread->updateConcurrency(headroom);
        REQUIRE(gate->waitFor(4));
        CHECK(mThread->getDecodeStats().current == 4);
        CHECK(mThread->getDecodeStats().active == 4);

        auto hot = headroom;
        hot.thermal_level = 3;
        clock.set(1500);
        mThread->updateConcurrency(hot);
        const auto reduced = mThread->getDecodeStats();
        CHECK(reduced.current == 2);
        CHECK(reduced.active == 4);
        CHECK(reduced.waiting == 4);
        CHECK(reduced.cancelled == 0);
        CHECK(mThread->getPending() == 8);
        CHECK(completed->copy().empty());
        gate->release();
        mThread->shutdown();
        check_failed_once(completed, ids);
        CHECK(mThread->getPending() == 0);
    }

    TEST_CASE_METHOD(imagedecodethread_test,
        "LLImageDecodeThread: disabling adaptation admits the configured fixed maximum",
        "[LLImageDecodeThread]")
    {
        mThread = new LLImageDecodeThread(true, limits(2, 4), false);
        const auto gate = std::make_shared<decode_gate>();
        release_decode_gate release{gate};
        const auto completed = std::make_shared<completion_state>();
        const auto ids = enqueue_blocked(*mThread, 8, gate, completed);
        REQUIRE(gate->waitFor(4));
        const auto stats = mThread->getDecodeStats();
        CHECK_FALSE(stats.adaptive);
        CHECK(stats.baseline == 2);
        CHECK(stats.current == 4);
        CHECK(stats.maximum == 4);
        CHECK(LL::ThreadPoolBase::getWidth("ImageDecode", 0) == 4);
        CHECK(stats.active == 4);
        CHECK(stats.waiting == 4);
        CHECK(mThread->getPending() == 8);
        gate->release();
        mThread->shutdown();
        check_failed_once(completed, ids);
        CHECK(mThread->getPending() == 0);
    }

    TEST_CASE_METHOD(imagedecodethread_test,
        "LLImageDecodeThread: missing topology retains a bounded fallback through the real queue",
        "[LLImageDecodeThread]")
    {
        decode_clock clock;
        const auto fallback = ALImageDecodeScheduler::Limits::fromTopology(0, {});
        mThread = new LLImageDecodeThread(true, fallback, true, clock.now());
        const auto gate = std::make_shared<decode_gate>();
        release_decode_gate release{gate};
        const auto completed = std::make_shared<completion_state>();
        const auto ids = enqueue_blocked(*mThread, 4, gate, completed);
        REQUIRE(gate->waitFor(2));
        ALImageDecodeScheduler::Pressure unknown;
        clock.set(5000);
        mThread->updateConcurrency(unknown);
        const auto stats = mThread->getDecodeStats();
        CHECK(stats.maximum == 2);
        CHECK(stats.current == 2);
        CHECK(stats.active == 2);
        CHECK(stats.waiting == 2);
        CHECK(mThread->getPending() == 4);
        gate->release();
        mThread->shutdown();
        check_failed_once(completed, ids);
        CHECK(mThread->getPending() == 0);
    }
}
