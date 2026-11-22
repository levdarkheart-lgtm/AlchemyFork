/**
 * @file _httpreplyqueue.h
 * @brief Internal declarations for the operation reply queue.
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

#ifndef _LLCORE_HTTP_REPLY_QUEUE_H_
#define _LLCORE_HTTP_REPLY_QUEUE_H_


#include "_refcounted.h"
#include "_mutex.h"
#include <functional>

namespace LLCore
{


class HttpOperation;


/// Almost identical to the HttpRequestQueue class but
/// whereas that class is a singleton and is known to the
/// HttpService object, this queue is 1:1 with HttpRequest
/// instances and isn't explicitly referenced by the
/// service object.  Instead, HttpOperation objects that
/// want to generate replies back to their creators also
/// keep references to the corresponding HttpReplyQueue.
/// The HttpService plumbing then simply delivers replies
/// to the requested reply queue.
///
/// One result of that is that the fetch operations do
/// not have a wait forever option.  The service object
/// doesn't keep handles on everything it would need to
/// notify so it can't wake up sleepers should it need to
/// shutdown.  So only non-blocking or timed-blocking modes
/// are anticipated.  These are how most application consumers
/// will be coded anyway so it shouldn't be too much of a
/// burden.

class HttpReplyQueue
{

public:
    typedef std::shared_ptr<HttpOperation>    opPtr_t;
    typedef std::shared_ptr<HttpReplyQueue>   ptr_t;

    HttpReplyQueue();
    virtual ~HttpReplyQueue();

    HttpReplyQueue(const HttpReplyQueue&) = delete;
    HttpReplyQueue& operator=(const HttpReplyQueue&) = delete;

public:

    typedef std::vector< opPtr_t > OpContainer;

    /// Insert an object at the back of the reply queue.
    ///
    /// Library also takes possession of one reference count to pass
    /// through the queue.
    ///
    /// Threading:  callable by any thread.
    void addOp(const opPtr_t &op);

    /// Fetch an operation from the head of the queue.  Returns
    /// NULL if none exists.
    ///
    /// Caller acquires reference count on returned operation.
    ///
    /// Threading:  callable by any thread.
    opPtr_t fetchOp();

    /// Caller acquires reference count on each returned operation
    ///
    /// Threading:  callable by any thread.
    void fetchAll(OpContainer & ops);

    // Wake-only notification, not a completion handler. Called under the queue
    // mutex after publication: must not block, throw, reenter this queue or take
    // another owner's data lock. Replacing/clearing synchronizes any in-flight
    // notification. The consumer must clear it before destroying its wake target.
    // Installing on a nonempty queue also signals existing replies. Timing is
    // opt-in with this notification; ordinary queues do not read the clock.
    void setReplyNotification(std::function<void()> notification);

    // Re-arm a limited consumer turn if replies remain. Same wake-only and
    // lifetime contract as above; check and signal are linearized with addOp.
    void notifyIfPending();

protected:

    OpContainer                         mQueue;
    LLCoreInt::HttpMutex                mQueueMutex;
    std::function<void()>               mReplyNotification;

}; // end class HttpReplyQueue

}  // end namespace LLCore


#endif  // _LLCORE_HTTP_REPLY_QUEUE_H_
