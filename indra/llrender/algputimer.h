/**
 * @file algputimer.h
 * @brief The GPU time a span of draws takes, read back without waiting on the GPU.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
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
 * $/LicenseInfo$
 */

#pragma once

#include "stdtypes.h"
#include "alquerypool.h"

#include <memory>

class ALMetalCommandCompletion;

/// The GPU time one span of draws takes, as a span of a query pool's. begin() and end() bracket the span, and poll()
/// hands the time back once the pool has read the span's frame, without waiting for it unless asked to. One span at a
/// time: begin() refuses while the last span's time is still to be read, since a second span would leave whoever is
/// polling for the first reading the second.
///
/// Native profiling attaches the completion of one isolated render command;
/// generic native query-pool spans remain unsupported. Non-copyable.
class ALGPUTimer
{
public:
    /// Spans from `pool`, or from gGL's where it is null.
    explicit ALGPUTimer(ALQueryPool* pool = nullptr) : mPool(pool) {}

    ALGPUTimer(const ALGPUTimer&) = delete;
    ALGPUTimer& operator=(const ALGPUTimer&) = delete;

    /// Starts timing the draws that follow. False, starting nothing, while a span is open or its time is pending, or
    /// where the pool refuses the span.
    bool begin();

    /// Ends the span begin() started, if it started one. Its time is then pending.
    void end();

    /// Attach one isolated native command's GPU interval. Refuses null or an
    /// already open/pending measurement; never replaces an unread result.
    bool attachNativeCompletion(std::shared_ptr<const ALMetalCommandCompletion> completion);

    /// Whether an ended span or native receipt has not been consumed.
    bool isPending() const { return mPending || bool(mNativeCompletion); }

    /// The pending span's time in nanoseconds, once its frame has been read: false while it has not, when nothing is
    /// pending, or when the time expired unread. `wait` waits for a query-pool
    /// span instead; native receipts always poll without waiting, even when
    /// `wait` is true. Failed native receipts clear pending without success.
    bool poll(U64& nanoseconds, bool wait = false);

private:
    ALQueryPool& pool() const;

    ALQueryPool* mPool;
    ALQuerySpan  mSpan;
    std::shared_ptr<const ALMetalCommandCompletion> mNativeCompletion;
    bool         mOpen    = false;
    bool         mPending = false;
};
