/**
 * @file llqueuedthread.cpp
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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

#include "linden_common.h"
#include "llqueuedthread.h"

#include <chrono>
#include <limits>

#include "llstl.h"
#include "lltimer.h"    // ms_sleep()
#include "llmutex.h"

//============================================================================

// MAIN THREAD
LLQueuedThread::LLQueuedThread(const std::string& name, bool threaded, bool should_pause) :
    LLThread(name),
    mIdleThread(true),
    mNextHandle(0),
    mStarted(false),
    mThreaded(threaded),
    mRequestQueue(name, 1024 * 1024)
{
    llassert(threaded); // not threaded implementation is deprecated

    // The thread starts when the owner calls start(), once the object is made.
    if (mThreaded && should_pause)
    {
        pause();
    }
}

// MAIN THREAD
void LLQueuedThread::start()
{
    if (mThreaded)
    {
        LLThread::start();
    }
}

// MAIN THREAD
LLQueuedThread::~LLQueuedThread()
{
    if (!mThreaded)
    {
        endThread();
    }
    shutdown();
    // ~LLThread() will be called here
}

void LLQueuedThread::shutdown()
{
    lockData();
    mShutdownStarted = true;
    unlockData();
    setQuitting();
    unpause();
    // A nonempty queue must close too. Existing tickets drain on the owner;
    // retries observe quitting and finish aborted instead of posting again.
    mRequestQueue.close();
    if (mThreaded)
    {
        LLTimer timer;
        bool warned = false;
        while (!isStopped())
        {
            if (!warned && timer.getElapsedTimeF32() >= 5.f)
            {
                LL_WARNS() << "Waiting for LLQueuedThread " << mName
                           << " to stop before releasing its requests" << LL_ENDL;
                warned = true;
            }
            // Destruction cannot safely free a request still in processRequest.
            // This drain runs no foreground callbacks and needs no frame polling.
            ms_sleep(1);
        }
    }
    else
    {
        mStatus = STOPPED;
    }

    lockData();
    while (auto* entry = mRequestHash.pop_element())
    {
        auto* req = static_cast<QueuedRequest*>(entry);
        if (req->getStatus() == STATUS_QUEUED)
        {
            req->setStatus(STATUS_ABORTED);
            req->finishRequest(false);
        }
        llassert_always(req->getStatus() != STATUS_INPROGRESS);
        req->deleteRequest();
    }
    unlockData();
}

//----------------------------------------------------------------------------

// MAIN THREAD
// virtual
size_t LLQueuedThread::update(F32 max_time_ms)
{
    LL_PROFILE_ZONE_SCOPED;
    if (!mStarted)
    {
        if (!mThreaded)
        {
            startThread();
            mStarted = true;
        }
    }
    return updateQueue(max_time_ms);
}

size_t LLQueuedThread::updateQueue(F32 max_time_ms)
{
    LL_PROFILE_ZONE_SCOPED;
    // Frame Update
    if (mThreaded)
    {
        // Most queued-thread owners have no housekeeping. The fetch owner
        // opts in; repeated frame polls coalesce while its callback is queued
        // or executing, rather than manufacturing a file/cache backlog.
        if (!isQuitting() && hasThreadedUpdate() &&
            !mThreadedUpdatePending.exchange(true, std::memory_order_acq_rel))
        {
            // Housekeeping is a retryable hint. Never wait for queue capacity
            // on the foreground owner, especially before unpausing service.
            const bool posted = mRequestQueue.tryPost([this]()
                {
                    LL_PROFILE_ZONE_NAMED_CATEGORY_THREAD("qt - update");
                    mIdleThread = false;
                    if (!isQuitting()) threadedUpdate();
                    mIdleThread = true;
                    mThreadedUpdatePending.store(false, std::memory_order_release);
                });
            if (!posted) mThreadedUpdatePending.store(false, std::memory_order_release);
        }

        if(getPending() > 0)
        {
            unpause();
        }
    }
    else
    {
        mRequestQueue.runFor(std::chrono::microseconds((int) (max_time_ms*1000.f)));
        threadedUpdate();
    }
    return getPending();
}

void LLQueuedThread::incQueue()
{
    // Something has been added to the queue
    if (!isPaused())
    {
        if (mThreaded)
        {
            wake(); // Wake the thread up if necessary.
        }
    }
}

//virtual
// May be called from any thread
size_t LLQueuedThread::getPending()
{
    return mRequestQueue.size();
}

// MAIN thread
void LLQueuedThread::waitOnPending()
{
    while(1)
    {
        update(0);

        if (mIdleThread && getPending() == 0)
        {
            break;
        }
        if (mThreaded)
        {
            yield();
        }
    }
    return;
}

void LLQueuedThread::waitOnPending(F32 max_time_sec)
{
    LLTimer wait_timer;
    wait_timer.reset();
    while (1)
    {
        update(0);

        if (mIdleThread && getPending() == 0)
        {
            break;
        }

        if (wait_timer.getElapsedTimeF64() >= max_time_sec)
        {
            LL_WARNS() << "waitOnPending() timed out after " << max_time_sec
                << " seconds in thread: " << mName << LL_ENDL;
            break;
        }

        if (mThreaded)
        {
            yield();
        }
    }
}

// MAIN thread
void LLQueuedThread::printQueueStats()
{
    auto size = mRequestQueue.size();
    if (size > 0)
    {
        LL_INFOS() << llformat("Pending Requests:%d ", mRequestQueue.size()) << LL_ENDL;
    }
    else
    {
        LL_INFOS() << "Queued Thread Idle" << LL_ENDL;
    }
}

// MAIN thread
LLQueuedThread::handle_t LLQueuedThread::generateHandle()
{
    lockData();
    handle_t handle;
    do
    {
        handle = ++mNextHandle;
    }
    while (handle == nullHandle() || mRequestHash.find(handle));
    unlockData();
    return handle;
}

// MAIN thread; ownership transfers only on successful registration.
bool LLQueuedThread::addRequest(QueuedRequest* req)
{
    LL_PROFILE_ZONE_SCOPED;
    lockData();
    if (mShutdownStarted || isQuitting() || mRequestHash.find(req->getHashKey()))
    {
        unlockData();
        return false;
    }
    llassert_always(mNextGeneration != std::numeric_limits<U64>::max());
    req->mGeneration = ++mNextGeneration;
    req->mScheduleGeneration = 1;
    req->mScheduled = true;
    req->mImmediatePending = true;
    req->setStatus(STATUS_QUEUED);
    mRequestHash.insert(req);
    const auto handle = req->getHashKey();
    const auto generation = req->mGeneration;
    unlockData();

    if (postRequest(handle, generation, 1, false, TimePoint::clock::now())) return true;
    lockData();
    auto* current = static_cast<QueuedRequest*>(mRequestHash.find(handle));
    const bool still_owned = current && current->mGeneration == generation;
    if (still_owned)
    {
        mRequestHash.erase(current);
        current->setStatus(STATUS_UNKNOWN);
    }
    unlockData();
    // A simultaneous shutdown may already have consumed the accepted request.
    return !still_owned;
}

// MAIN thread
bool LLQueuedThread::waitForResult(LLQueuedThread::handle_t handle, bool auto_complete)
{
    LL_PROFILE_ZONE_SCOPED;
    llassert (handle != nullHandle());
    bool res = false;
    bool waspaused = isPaused();
    bool done = false;
    while(!done)
    {
        update(0); // unpauses
        lockData();
        QueuedRequest* req = (QueuedRequest*)mRequestHash.find(handle);
        if (!req)
        {
            done = true; // request does not exist
        }
        else if (req->getStatus() == STATUS_COMPLETE || req->getStatus() == STATUS_ABORTED)
        {
            res = req->getStatus() == STATUS_COMPLETE;
            if (auto_complete)
            {
                mRequestHash.erase(handle);
                req->deleteRequest();
//              check();
            }
            done = true;
        }
        unlockData();

        if (!done && mThreaded)
        {
            yield();
        }
    }
    if (waspaused)
    {
        pause();
    }
    return res;
}

// MAIN thread
LLQueuedThread::QueuedRequest* LLQueuedThread::getRequest(handle_t handle)
{
    if (handle == nullHandle())
    {
        return 0;
    }
    lockData();
    QueuedRequest* res = (QueuedRequest*)mRequestHash.find(handle);
    unlockData();
    return res;
}

LLQueuedThread::status_t LLQueuedThread::getRequestStatus(handle_t handle)
{
    status_t res = STATUS_EXPIRED;
    lockData();
    QueuedRequest* req = (QueuedRequest*)mRequestHash.find(handle);
    if (req)
    {
        res = req->getStatus();
    }
    unlockData();
    return res;
}

void LLQueuedThread::abortRequest(handle_t handle, bool autocomplete)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_THREAD;
    lockData();
    QueuedRequest* req = (QueuedRequest*)mRequestHash.find(handle);
    if (req)
    {
        req->setFlags(FLAG_ABORT | (autocomplete ? FLAG_AUTO_COMPLETE : 0));
    }
    unlockData();
    wakeRequest(handle);
}

// MAIN thread
void LLQueuedThread::setFlags(handle_t handle, U32 flags)
{
    lockData();
    QueuedRequest* req = (QueuedRequest*)mRequestHash.find(handle);
    if (req)
    {
        req->setFlags(flags);
    }
    unlockData();
}

bool LLQueuedThread::completeRequest(handle_t handle)
{
    LL_PROFILE_ZONE_SCOPED;
    bool res = false;
    lockData();
    QueuedRequest* req = (QueuedRequest*)mRequestHash.find(handle);
    if (req)
    {
        llassert_always(req->getStatus() != STATUS_QUEUED);
        llassert_always(req->getStatus() != STATUS_INPROGRESS);
#if _DEBUG
//      LL_INFOS() << llformat("LLQueuedThread::Completed req [%08d]",handle) << LL_ENDL;
#endif
        mRequestHash.erase(handle);
        req->deleteRequest();
//      check();
        res = true;
    }
    unlockData();
    return res;
}

bool LLQueuedThread::check()
{
#if 0 // not a reliable check once mNextHandle wraps, just for quick and dirty debugging
    for (int i=0; i<REQUEST_HASH_SIZE; i++)
    {
        LLSimpleHashEntry<handle_t>* entry = mRequestHash.get_element_at_index(i);
        while (entry)
        {
            if (entry->getHashKey() > mNextHandle)
            {
                LL_ERRS() << "Hash Error" << LL_ENDL;
                return false;
            }
            entry = entry->getNextEntry();
        }
    }
#endif
    return true;
}

//============================================================================
// Runs on its OWN thread

bool LLQueuedThread::postRequest(handle_t handle, U64 generation, U64 schedule,
                                 bool urgent, const TimePoint& when)
{
    return mRequestQueue.post([this, handle, generation, schedule, urgent]()
        { dispatchRequest(handle, generation, schedule, urgent); }, when);
}

// Any thread. The data lock serializes wake publication with the transition
// from INPROGRESS to a timed retry, closing the notification-before-retry race.
void LLQueuedThread::wakeRequest(handle_t handle, bool urgent)
{
    lockData();
    auto* req = static_cast<QueuedRequest*>(mRequestHash.find(handle));
    if (!req || mShutdownStarted)
    {
        unlockData();
        return;
    }
    if (req->getStatus() == STATUS_INPROGRESS)
    {
        req->mWakeRequested = true;
        unlockData();
        return;
    }
    if (req->getStatus() != STATUS_QUEUED ||
        (urgent ? req->mUrgentPending : req->mImmediatePending))
    {
        unlockData();
        return;
    }
    const U64 generation = req->mGeneration;
    llassert_always(req->mScheduleGeneration != std::numeric_limits<U64>::max());
    const U64 schedule = ++req->mScheduleGeneration;
    req->mScheduled = true;
    req->mImmediatePending = true;
    req->mUrgentPending = urgent;
    unlockData();
    if (!postRequest(handle, generation, schedule, urgent,
                     urgent ? TimePoint::min() : TimePoint::clock::now()))
    {
        // A closed queue is drained by shutdown, which retains every request
        // until the worker has stopped. No request pointer escaped the lock.
        llassert(mRequestQueue.isClosed());
    }
}

void LLQueuedThread::finishRequestLocked(QueuedRequest* req, bool complete)
{
    req->mScheduled = false;
    req->mImmediatePending = false;
    req->mUrgentPending = false;
    req->setStatus(complete ? STATUS_COMPLETE : STATUS_ABORTED);
    req->finishRequest(complete);
    if (req->getFlags() & FLAG_AUTO_COMPLETE)
    {
        mRequestHash.erase(req);
        req->deleteRequest();
    }
}

// The worker is the sole dispatcher. Resolving both identities while locked
// makes stale initial, delayed, woken and promoted tickets harmless, including
// after completion, auto-deletion and reuse of a 32-bit public handle.
void LLQueuedThread::dispatchRequest(handle_t handle, U64 generation, U64 schedule, bool urgent)
{
    lockData();
    auto* req = static_cast<QueuedRequest*>(mRequestHash.find(handle));
    if (!req || req->mGeneration != generation ||
        req->mScheduleGeneration != schedule || !req->mScheduled ||
        req->getStatus() != STATUS_QUEUED)
    {
        unlockData();
        return;
    }
    if (urgent && mUrgentBurst >= 4 && !mShutdownStarted)
    {
        // Yield to normal tickets already ready. Promotion is bounded even
        // when many urgent tickets were admitted before the first could run.
        const U64 next = ++req->mScheduleGeneration;
        req->mUrgentPending = false;
        unlockData();
        postRequest(handle, generation, next, false, TimePoint::clock::now());
        return;
    }
    mUrgentBurst = urgent ? mUrgentBurst + 1 : 0;
    req->mScheduled = false;
    req->mImmediatePending = false;
    req->mUrgentPending = false;
    mIdleThread = false;
    if ((req->getFlags() & FLAG_ABORT) || mShutdownStarted || isQuitting())
    {
        finishRequestLocked(req, false);
        unlockData();
        mIdleThread = true;
        return;
    }
    req->setStatus(STATUS_INPROGRESS);
    req->mWakeRequested = false;
    unlockData();
    processRequest(req);
}

void LLQueuedThread::processRequest(QueuedRequest* req)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_THREAD;
    const bool complete = req->processRequest();
    // A request remains INPROGRESS (and therefore undeletable) until this
    // owner has inspected its event hint and published the terminal/retry state.
    const bool waits_for_wake = !complete && req->waitsForWake();
    lockData();
    if (complete || (req->getFlags() & FLAG_ABORT) || mShutdownStarted || isQuitting())
    {
        finishRequestLocked(req, complete && !(req->getFlags() & FLAG_ABORT) &&
                            !mShutdownStarted && !isQuitting());
        unlockData();
    }
    else
    {
        using namespace std::chrono_literals;
        const auto delay = req->mWakeRequested ? 0ms : (waits_for_wake ? 250ms : 16ms);
        const auto handle = req->getHashKey();
        const U64 generation = req->mGeneration;
        llassert_always(req->mScheduleGeneration != std::numeric_limits<U64>::max());
        const U64 schedule = ++req->mScheduleGeneration;
        req->setStatus(STATUS_QUEUED);
        req->mScheduled = true;
        req->mImmediatePending = delay == 0ms;
        req->mWakeRequested = false;
        unlockData();
        // This is a timestamp in the schedule, never a sleeping callback.
        postRequest(handle, generation, schedule, false, TimePoint::clock::now() + delay);
    }
    mIdleThread = true;
}

// virtual
void LLQueuedThread::run()
{
    // A queue made paused waits here until it is unpaused.
    checkPause();
    startThread();


    mRequestQueue.runUntilClose();

    endThread();
    LL_INFOS() << "LLQueuedThread " << mName << " EXITING." << LL_ENDL;


}

// virtual
void LLQueuedThread::startThread()
{
}

// virtual
void LLQueuedThread::endThread()
{
}

// virtual
void LLQueuedThread::threadedUpdate()
{
}

//============================================================================

LLQueuedThread::QueuedRequest::QueuedRequest(LLQueuedThread::handle_t handle, U32 flags) :
    LLSimpleHashEntry<LLQueuedThread::handle_t>(handle),
    mStatus(STATUS_UNKNOWN),
    mFlags(flags)
{
}

LLQueuedThread::QueuedRequest::~QueuedRequest()
{
    if (mStatus != STATUS_DELETE)
    {
        // The only method to delete a request is deleteRequest(),
        // it should have set the status to STATUS_DELETE
        LL_ERRS() << "LLQueuedThread::QueuedRequest deleted with status " << mStatus << LL_ENDL;
    }
}

//virtual
void LLQueuedThread::QueuedRequest::finishRequest(bool completed)
{
}

//virtual
void LLQueuedThread::QueuedRequest::deleteRequest()
{
    llassert_always(mStatus != STATUS_INPROGRESS);
    setStatus(STATUS_DELETE);
    delete this;
}
