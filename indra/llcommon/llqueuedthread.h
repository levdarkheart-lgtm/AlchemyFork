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

#include <queue>
#include <string>
#include <map>
#include <set>
#include <atomic>

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
            return mFlags;
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
            mFlags |= flags;
        }

        virtual bool processRequest() = 0; // Return true when request has completed
        virtual void finishRequest(bool completed); // Always called from thread after request has completed or aborted
        virtual void deleteRequest(); // Only method to delete a request
        // Event-driven owners may use a longer safety retry. A wake received
        // during processRequest() is retained until that call returns.
        virtual bool waitsForWake() const { return false; }

    protected:
        LLAtomicBase<status_t> mStatus;
        U32 mFlags;

    private:
        // Protected by the queue's data lock. Neither timed nor promoted work
        // captures a request pointer: handle reuse and superseded tickets must
        // not revive a deleted request.
        U64 mGeneration = 0;
        U64 mScheduleGeneration = 0;
        bool mScheduled = false;
        bool mImmediatePending = false;
        bool mUrgentPending = false;
        bool mWakeRequested = false;
    };

    //------------------------------------------------------------------------

public:
    static handle_t nullHandle() { return handle_t(0); }

public:
    LLQueuedThread(const std::string& name, bool threaded = true, bool should_pause = false);
    virtual ~LLQueuedThread();
    virtual void shutdown();

    // Starts the thread, unless the queue is run on the main thread. Its owner
    // calls this once the object is made; a constructor cannot, since the
    // thread calls run(), and run() the rest, through the vtable each derived
    // constructor writes in its turn.
    void start();

    // No copy constructor or copy assignment
    LLQueuedThread(const LLQueuedThread&) = delete;
    LLQueuedThread& operator=(const LLQueuedThread&) = delete;

private:
    virtual void run(void);
    virtual void startThread(void);
    virtual void endThread(void);
    virtual void threadedUpdate(void);
    virtual bool hasThreadedUpdate() const { return false; }

protected:
    handle_t generateHandle();
    bool addRequest(QueuedRequest* req);
    void processRequest(QueuedRequest* req);
    void incQueue();

private:
    using TimePoint = LL::WorkSchedule::TimePoint;
    bool postRequest(handle_t handle, U64 generation, U64 schedule,
                     bool urgent, const TimePoint& when);
    void dispatchRequest(handle_t handle, U64 generation, U64 schedule, bool urgent);
    void finishRequestLocked(QueuedRequest* req, bool complete);
    void wakeRequest(handle_t handle, bool urgent);

public:
    bool waitForResult(handle_t handle, bool auto_complete = true);

    virtual size_t update(F32 max_time_ms);
    size_t updateQueue(F32 max_time_ms);

    void waitOnPending();
    void waitOnPending(F32 max_time_sec);
    void printQueueStats();

    virtual size_t getPending();
    bool getThreaded() { return mThreaded; }

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

    //typedef std::set<QueuedRequest*, queued_request_less> request_queue_t;
    //request_queue_t mRequestQueue;
    LL::WorkSchedule mRequestQueue;

    enum { REQUEST_HASH_SIZE = 512 }; // must be power of 2
    typedef LLSimpleHash<handle_t, REQUEST_HASH_SIZE> request_hash_t;
    request_hash_t mRequestHash;

    handle_t mNextHandle;
    U64 mNextGeneration = 0;
    U32 mUrgentBurst = 0; // worker only
    std::atomic<bool> mThreadedUpdatePending{false};
    bool mShutdownStarted = false; // protected by data lock
};

#endif // LL_LLQUEUEDTHREAD_H
