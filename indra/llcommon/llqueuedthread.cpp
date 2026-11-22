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
LLQueuedThread::LLQueuedThread(const std::string& name, bool threaded, bool should_pause,
                               size_t request_capacity, size_t callback_capacity) :
    LLThread(name),
    mThreaded(threaded),
    mStarted(false),
    mIdleThread(true),
    mRequestQueue(name, callback_capacity, *this),
    mNextHandle(0),
    mRequestCapacity(request_capacity)
{
    llassert(threaded); // foreground pumping is retained only for contained owners
    llassert_always(request_capacity && callback_capacity);
    if (mThreaded && should_pause) pause();
}

LLQueuedThread::ServiceQueue::ServiceQueue(const std::string& name, size_t capacity,
                                         LLQueuedThread& owner) :
    // LLQueuedThread owns close/drain/join; an independent LLApp queue listener
    // must not tear down this member outside that lifecycle.
    LL::WorkQueue(name, capacity, false), mOwner(owner)
{
}

bool LLQueuedThread::ServiceQueue::post(const Work& work) { return tryPost(work); }
bool LLQueuedThread::ServiceQueue::post(Work&& work) { return tryPost(std::move(work)); }

bool LLQueuedThread::ServiceQueue::tryPost(const Work& work)
{
    if (mOwner.isAdmissionClosed() || !LL::WorkQueue::tryPost(work)) return false;
    mOwner.notifyWork();
    return true;
}

bool LLQueuedThread::ServiceQueue::tryPost(Work&& work)
{
    if (mOwner.isAdmissionClosed() || !LL::WorkQueue::tryPost(std::move(work))) return false;
    mOwner.notifyWork();
    return true;
}

void LLQueuedThread::ServiceQueue::close()
{
    // Linearize callback admission before allowing the owner to finish its drain.
    LL::WorkQueue::close();
    mOwner.beginShutdown();
}

bool LLQueuedThread::ServiceQueue::processOne()
{
    return runOneReady();
}

void LLQueuedThread::notifyWork() noexcept
{
    if (!mWakePending.exchange(true, std::memory_order_acq_rel)) mWakeEvent.release();
}

bool LLQueuedThread::requestThreadedUpdate() noexcept
{
    if (!mThreaded) return false;
    U32 state = mThreadedUpdateState.load(std::memory_order_acquire);
    for (;;)
    {
        if (state & UPDATE_CLOSED) return false;
        if (state & UPDATE_PENDING) return true;
        if (mThreadedUpdateState.compare_exchange_weak(state, state | UPDATE_PENDING,
                std::memory_order_acq_rel, std::memory_order_acquire))
        {
            notifyWork();
            return true;
        }
    }
}

void LLQueuedThread::consumeNotification() noexcept
{
    if (mWakeEvent.try_acquire()) mWakePending.store(false, std::memory_order_release);
}

void LLQueuedThread::waitForWork(const TimePoint& deadline)
{
    if (deadline == TimePoint::max())
    {
        mWakeEvent.acquire();
        mWakePending.store(false, std::memory_order_release);
    }
    else if (mWakeEvent.try_acquire_until(deadline))
    {
        mWakePending.store(false, std::memory_order_release);
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

void LLQueuedThread::beginShutdown()
{
    {
        LLMutexLock lock(mRunCondition);
        mThreadedUpdateState.store(UPDATE_CLOSED, std::memory_order_release);
        mShutdownStarted.store(true, std::memory_order_release);
    }
    setQuitting();
    unpause();
    notifyWork();
}

void LLQueuedThread::shutdown()
{
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
            ms_sleep(1);
        }
    }
    // A never-started or failed owner has no concurrent consumer. Settle its
    // accepted callbacks/requests too, before deleting storage they may pin.
    processPending();

    LLMutexLock lock(mRunCondition);
    while (auto* entry = mRequestHash.pop_element())
    {
        auto* req = static_cast<QueuedRequest*>(entry);
        llassert_always(req->getStatus() != STATUS_INPROGRESS && !req->mScheduled);
        --mRegisteredRequests;
        req->deleteRequest();
    }
    llassert_always(mActiveRequests == 0 && mRegisteredRequests == 0 && mSchedule.empty());
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
    if (mThreaded)
    {
        if (!isAdmissionClosed() && hasThreadedUpdate())
        {
            LLMutexLock lock(mRunCondition);
            if (!mShutdownStarted && !mHousekeepingActive)
            {
                requestThreadedUpdate();
            }
        }
        if (getPending()) unpause();
    }
    else
    {
        const auto until = TimePoint::clock::now() + std::chrono::duration_cast<TimePoint::duration>(
            std::chrono::duration<double, std::milli>(llclamp(max_time_ms, 0.f, 1000.f)));
        while (TimePoint::clock::now() < until && processOne()) {}
        if (!isAdmissionClosed()) threadedUpdate();
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
    LLMutexLock lock(mRunCondition);
    return mActiveRequests + mRequestQueue.size() +
        size_t(bool(mThreadedUpdateState.load(std::memory_order_acquire) & UPDATE_PENDING)) + size_t(mServiceActive);
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
    LL_INFOS() << mName << " pending work: " << getPending() << LL_ENDL;
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
    {
        LLMutexLock lock(mRunCondition);
        if (isAdmissionClosed() || mRegisteredRequests == mRequestCapacity ||
            mRequestHash.find(req->getHashKey())) return false;
        llassert_always(mNextGeneration != std::numeric_limits<U64>::max());
        llassert_always(mNextScheduleOrder != std::numeric_limits<U64>::max());
        const auto generation = mNextGeneration + 1;
        // The only scheduling allocation occurs before ownership transfer.
        const auto [position, inserted] = mSchedule.emplace(
            ScheduleKey{TimePoint::clock::now(), mNextScheduleOrder + 1},
            RequestIdentity{req->getHashKey(), generation});
        llassert_always(inserted);
        ++mNextGeneration;
        ++mNextScheduleOrder;
        req->mGeneration = generation;
        req->mSchedulePosition = position;
        req->mScheduled = true;
        req->mImmediatePending = true;
        req->mUrgentPending = false;
        req->mWakeRequested = false;
        req->setStatus(STATUS_QUEUED);
        mRequestHash.insert(req);
        ++mRegisteredRequests;
        ++mActiveRequests;
    }
    notifyWork();
    return true;
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
                eraseRequestLocked(req);
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
        eraseRequestLocked(req);
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

void LLQueuedThread::scheduleRequestLocked(QueuedRequest* req, const TimePoint& when, bool urgent)
{
    llassert_always(!req->mScheduled && !req->mScheduleNode.empty());
    llassert_always(mNextScheduleOrder != std::numeric_limits<U64>::max());
    req->mScheduleNode.key() = {urgent ? TimePoint::min() : when, ++mNextScheduleOrder};
    const auto inserted = mSchedule.insert(std::move(req->mScheduleNode));
    llassert_always(inserted.inserted);
    req->mSchedulePosition = inserted.position;
    req->mScheduled = true;
    req->mImmediatePending = urgent || when <= TimePoint::clock::now();
    req->mUrgentPending = urgent;
}

void LLQueuedThread::wakeRequest(handle_t handle, bool urgent)
{
    {
        LLMutexLock lock(mRunCondition);
        auto* req = static_cast<QueuedRequest*>(mRequestHash.find(handle));
        if (!req || isAdmissionClosed()) return;
        if (req->getStatus() == STATUS_INPROGRESS)
        {
            req->mWakeRequested = true;
            return; // the active owner cannot be asleep
        }
        if (req->getStatus() != STATUS_QUEUED || req->mUrgentPending ||
            (!urgent && req->mImmediatePending)) return;
        req->mScheduleNode = mSchedule.extract(req->mSchedulePosition);
        req->mScheduled = false;
        scheduleRequestLocked(req, TimePoint::clock::now(), urgent);
    }
    notifyWork();
}

void LLQueuedThread::eraseRequestLocked(QueuedRequest* req)
{
    llassert_always(!req->mScheduled && req->getStatus() != STATUS_INPROGRESS &&
                   req->getStatus() != STATUS_QUEUED);
    mRequestHash.erase(req);
    --mRegisteredRequests;
    req->deleteRequest();
}

void LLQueuedThread::finishRequestLocked(QueuedRequest* req, bool complete)
{
    llassert_always(!req->mScheduled);
    req->mImmediatePending = false;
    req->mUrgentPending = false;
    req->mWakeRequested = false;
    req->setStatus(complete ? STATUS_COMPLETE : STATUS_ABORTED);
    --mActiveRequests;
    req->finishRequest(complete);
    if (req->getFlags() & FLAG_AUTO_COMPLETE) eraseRequestLocked(req);
}

bool LLQueuedThread::processOne()
{
    QueuedRequest* req = nullptr;
    bool housekeeping = false;
    {
        LLMutexLock lock(mRunCondition);
        auto position = mSchedule.begin();
        const auto now = TimePoint::clock::now();
        const bool stopping = isAdmissionClosed();
        if (position != mSchedule.end() && !stopping && mUrgentBurst >= 4 &&
            position->first.first == TimePoint::min())
        {
            const auto normal = mSchedule.upper_bound({TimePoint::min(), std::numeric_limits<U64>::max()});
            if (normal != mSchedule.end() && normal->first.first <= now) position = normal;
        }
        const bool ready = position != mSchedule.end() && (stopping || position->first.first <= now);
        const bool callbacks = mRequestQueue.size() != 0;
        const bool update = (mThreadedUpdateState.load(std::memory_order_acquire) & UPDATE_PENDING) && !stopping;
        if ((callbacks || update) && (mPreferService || !ready))
        {
            housekeeping = update && (mPreferHousekeeping || !callbacks);
            if (housekeeping)
            {
                mThreadedUpdateState.fetch_and(~UPDATE_PENDING, std::memory_order_acq_rel);
                mHousekeepingActive = true;
            }
            mPreferHousekeeping = !housekeeping;
            mPreferService = false;
            mServiceActive = true;
        }
        else if (ready)
        {
            const auto [handle, generation] = position->second;
            req = static_cast<QueuedRequest*>(mRequestHash.find(handle));
            llassert_always(req && req->mGeneration == generation && req->mScheduled &&
                            req->getStatus() == STATUS_QUEUED);
            const bool urgent = req->mUrgentPending;
            req->mScheduleNode = mSchedule.extract(position);
            req->mScheduled = false;
            req->mImmediatePending = false;
            req->mUrgentPending = false;
            mUrgentBurst = urgent ? llmin(mUrgentBurst + 1, 4u) : 0;
            mPreferService = true;
            req->setStatus(STATUS_INPROGRESS);
        }
        else return false;
        mIdleThread = false;
    }
    bool processed = true;
    if (req)
    {
        mRequestQueue.invoke([this, req] { processRequest(req); });
    }
    else
    {
        if (housekeeping) mRequestQueue.invoke([this] { if (!isAdmissionClosed()) threadedUpdate(); });
        else processed = mRequestQueue.processOne();
        LLMutexLock lock(mRunCondition);
        mServiceActive = false;
        mHousekeepingActive = false;
    }
    mIdleThread = true;
    return processed;
}

void LLQueuedThread::processPending()
{
    while (processOne()) {}
}

void LLQueuedThread::processRequest(QueuedRequest* req)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_THREAD;
    bool complete = false;
    bool waits_for_wake = false;
    try
    {
        if (!isAdmissionClosed() && !(req->getFlags() & FLAG_ABORT))
        {
            complete = req->processRequest();
            waits_for_wake = !complete && req->waitsForWake();
        }
    }
    catch (...)
    {
        LLMutexLock lock(mRunCondition);
        finishRequestLocked(req, false);
        throw;
    }
    LLMutexLock lock(mRunCondition);
    if (complete || (req->getFlags() & FLAG_ABORT) || isAdmissionClosed())
    {
        finishRequestLocked(req, complete && !(req->getFlags() & FLAG_ABORT) && !isAdmissionClosed());
    }
    else
    {
        using namespace std::chrono_literals;
        const auto delay = req->mWakeRequested ? 0ms : (waits_for_wake ? 250ms : 16ms);
        req->mWakeRequested = false;
        req->setStatus(STATUS_QUEUED);
        scheduleRequestLocked(req, TimePoint::clock::now() + delay, false);
    }
}

LLQueuedThread::TimePoint LLQueuedThread::nextDeadline()
{
    LLMutexLock lock(mRunCondition);
    // Recheck after the last failed pump. A producer also leaves a semaphore
    // token, so work published between this snapshot and wait cannot be lost.
    if (mShutdownStarted || mRequestQueue.size() ||
        (mThreadedUpdateState.load(std::memory_order_acquire) & UPDATE_PENDING))
        return TimePoint::clock::now();
    return mSchedule.empty() ? TimePoint::max() : mSchedule.begin()->first.first;
}

void LLQueuedThread::run()
{
    // Explicit pause is a startup/control policy. Idle service uses the native
    // event below, not a subclass runCondition that callbacks cannot signal.
    if (isPaused()) checkPause();
    startThread();
    for (;;)
    {
        consumeNotification();
        if (isQuitting() && !mShutdownStarted) mRequestQueue.close();
        if (isPaused() && !isAdmissionClosed()) checkPause();
        if (processOne()) continue;
        if (mShutdownStarted && !LLQueuedThread::getPending()) break;
        waitForWork(nextDeadline());
    }
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
