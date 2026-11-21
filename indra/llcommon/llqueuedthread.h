/**
 * @file llqueuedthread.h
 * @brief
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

#ifndef LL_LLQUEUEDTHREAD_H
#define LL_LLQUEUEDTHREAD_H

#include <string>
#include <map>
#include <atomic>
#include <semaphore>

#include "llatomic.h"

#include "llthread.h"
#include "llsimplehash.h"
#include "workqueue.h"

//============================================================================
// Note: ~LLQueuedThread is O(N) N=# of queued threads, assumed to be small
//   It is assumed that LLQueuedThreads are rarely created/destroyed.

class LL_COMMON_API LLQueuedThread : public LLThread
{
    //------------------------------------------------------------------------
public:
    enum status_t {
        STATUS_EXPIRED = -1,
        STATUS_UNKNOWN = 0,
        STATUS_QUEUED = 1,
        STATUS_INPROGRESS = 2,
        STATUS_COMPLETE = 3,
        STATUS_ABORTED = 4,
        STATUS_DELETE = 5
    };
    enum flags_t {
        FLAG_AUTO_COMPLETE = 1,
        FLAG_AUTO_DELETE = 2, // child-class dependent
        FLAG_ABORT = 4
    };

    typedef U32 handle_t;

private:
    using TimePoint = LL::WorkQueueBase::TimePoint;
    using RequestIdentity = std::pair<handle_t, U64>;
    using ScheduleKey = std::pair<TimePoint, U64>;
    using RequestSchedule = std::map<ScheduleKey, RequestIdentity>;

public:
    static constexpr size_t DEFAULT_CAPACITY = 1024 * 1024;

    //------------------------------------------------------------------------
public:

    class LL_COMMON_API QueuedRequest : public LLSimpleHashEntry<handle_t>
    {
        friend class LLQueuedThread;

    protected:
        virtual ~QueuedRequest(); // use deleteRequest()

    public:
        QueuedRequest(handle_t handle, U32 flags = 0);

        status_t getStatus()
        {
            return mStatus;
        }
        U32 getFlags() const
        {
            return mFlags.load(std::memory_order_acquire);
        }

    protected:
        status_t setStatus(status_t newstatus)
        {
            status_t oldstatus = mStatus;
            mStatus = newstatus;
            return oldstatus;
        }
        void setFlags(U32 flags)
        {
            // NOTE: flags are |'d
            mFlags.fetch_or(flags, std::memory_order_acq_rel);
        }

        virtual bool processRequest() = 0; // Return true when request has completed
        virtual void finishRequest(bool completed); // Always called from thread after request has completed or aborted
        virtual void deleteRequest(); // Only method to delete a request
        // Event-driven owners may use a longer safety retry. A wake received
        // during processRequest() is retained until that call returns.
        virtual bool waitsForWake() const { return false; }

    protected:
        LLAtomicBase<status_t> mStatus;
        std::atomic<U32> mFlags;

    private:
        // One allocation per accepted request. While queued its node is in
        // the owner's schedule; while running/terminal the request retains it.
        // Rekeying a node never allocates or consumes callback capacity.
        U64 mGeneration = 0;
        RequestSchedule::iterator mSchedulePosition;
        RequestSchedule::node_type mScheduleNode;
        bool mScheduled = false;
        bool mImmediatePending = false;
        bool mUrgentPending = false;
        bool mWakeRequested = false;

    };

    //------------------------------------------------------------------------

public:
    static handle_t nullHandle() { return handle_t(0); }

public:
    LLQueuedThread(const std::string& name, bool threaded = true, bool should_pause = false,
                   size_t request_capacity = DEFAULT_CAPACITY,
                   size_t callback_capacity = DEFAULT_CAPACITY);
    virtual ~LLQueuedThread();
    void shutdown() override;

    // Starts the thread, unless the queue is run on the main thread. Its owner
    // calls this once the object is made; a constructor cannot, since the
    // thread calls run(), and run() the rest, through the vtable each derived
    // constructor writes in its turn.
    void start();

    // No copy constructor or copy assignment
    LLQueuedThread(const LLQueuedThread&) = delete;
    LLQueuedThread& operator=(const LLQueuedThread&) = delete;

private:
    // This owner uses its native work event for idle service. LLThread's
    // condition is reserved for explicit pause/unpause, not a second idle gate.
    bool runCondition() final override { return true; }
    void run() override;
    virtual void startThread(void);
    virtual void endThread(void);
    virtual void threadedUpdate(void);
    virtual bool hasThreadedUpdate() const { return false; }

protected:
    handle_t generateHandle();
    bool addRequest(QueuedRequest* req);
    void processRequest(QueuedRequest* req);
    void incQueue();

    // A single owner pumps requests and callbacks. Exposed to derived owners
    // for contained service/tests, never to concurrent consumers.
    bool processOne();
    void processPending();

private:
    void scheduleRequestLocked(QueuedRequest* req, const TimePoint& when, bool urgent);
    void finishRequestLocked(QueuedRequest* req, bool complete);
    void eraseRequestLocked(QueuedRequest* req);
    void wakeRequest(handle_t handle, bool urgent);
    void beginShutdown();
    void notifyWork() noexcept;
    void consumeNotification() noexcept;
    void waitForWork(const TimePoint& deadline);
    TimePoint nextDeadline();

public:
    bool waitForResult(handle_t handle, bool auto_complete = true);

    virtual size_t update(F32 max_time_ms);
    size_t updateQueue(F32 max_time_ms);

    void waitOnPending();
    void waitOnPending(F32 max_time_sec);
    void printQueueStats();

    virtual size_t getPending();
    bool getThreaded() { return mThreaded; }
    // Busy means retryable nonacceptance. Closed means no further request or
    // callback will be accepted; existing work still settles on its owner.
    bool isAdmissionClosed() const { return mShutdownStarted.load(std::memory_order_acquire) || isQuitting(); }

    // Request accessors
    status_t getRequestStatus(handle_t handle);
    // Any thread, with the queue owner kept alive by the caller. Wakes are
    // coalesced; promotion also admits a not-yet-started request ahead of normal
    // ready work, with at most four promoted starts before normal service.
    void wakeRequest(handle_t handle) { wakeRequest(handle, false); }
    void promoteRequest(handle_t handle) { wakeRequest(handle, true); }
    void abortRequest(handle_t handle, bool autocomplete);
    void setFlags(handle_t handle, U32 flags);
    bool completeRequest(handle_t handle);
    // This is public for support classes like LLWorkerThread,
    // but generally the methods above should be used.
    QueuedRequest* getRequest(handle_t handle);

    // debug (see source)
    bool check();

protected:
    bool mThreaded;  // if false, run on main thread and do updates during update()
    bool mStarted;  // when mThreaded is false, whether update() has called startThread(); main thread only
    LLAtomicBool mIdleThread; // request queue is empty (or we are quitting) and the thread is idle

    // Keep the established named WorkQueue and exception dispatch, but make
    // all producer admission bounded and nonblocking. Notifications must not
    // take the target's data lock from another owner's completion callback.
    class ServiceQueue final : public LL::WorkQueue
    {
    public:
        ServiceQueue(const std::string& name, size_t capacity, LLQueuedThread& owner);
        bool post(const Work& work) override;
        bool post(Work&& work) override;
        bool tryPost(const Work& work) override;
        bool tryPost(Work&& work) override;
        void close() override;
        bool processOne();
        void invoke(const Work& work) { callWork(work); }
    private:
        LLQueuedThread& mOwner;
    };
    ServiceQueue mRequestQueue;

    enum { REQUEST_HASH_SIZE = 512 }; // must be power of 2
    typedef LLSimpleHash<handle_t, REQUEST_HASH_SIZE> request_hash_t;
    request_hash_t mRequestHash;

    handle_t mNextHandle;
    RequestSchedule mSchedule;
    const size_t mRequestCapacity;
    size_t mRegisteredRequests = 0; // includes terminal entries until reaped
    size_t mActiveRequests = 0;     // queued/in progress, not retained results
    U64 mNextGeneration = 0;
    U64 mNextScheduleOrder = 0;
    U32 mUrgentBurst = 0;
    bool mPreferService = true;
    bool mPreferHousekeeping = true;
    bool mServiceActive = false;
    bool mHousekeepingActive = false;
    bool mThreadedUpdatePending = false;
    std::atomic<bool> mShutdownStarted{false};

    // A coalesced native-thread event, independent of data/queue locks. The
    // pending bit is cleared only after acquiring its semaphore token, so a
    // timed-out waiter cannot race an in-flight release and overfill it.
    std::binary_semaphore mWakeEvent{0};
    std::atomic<bool> mWakePending{false};

};

#endif // LL_LLQUEUEDTHREAD_H
