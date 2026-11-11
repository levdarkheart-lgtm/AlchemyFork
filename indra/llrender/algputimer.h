/**
 * @file algputimer.h
 * @brief An isolated native command GPU interval, consumed without waiting.
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

#include <memory>

class ALMetalCommandCompletion;

/// Owns one isolated native render command's completion until its GPU interval
/// is consumed. A pending or unread result cannot be replaced by another
/// profile. Polling never waits for or submits GPU work. Non-copyable.
class ALGPUTimer
{
public:
    ALGPUTimer() = default;
    ALGPUTimer(const ALGPUTimer&) = delete;
    ALGPUTimer& operator=(const ALGPUTimer&) = delete;

    /// Refuses null or an already pending/unread measurement.
    bool attachNativeCompletion(std::shared_ptr<const ALMetalCommandCompletion> completion);
    bool isPending() const { return bool(mNativeCompletion); }

    /// Consume a ready interval in nanoseconds exactly once. Pending receipts
    /// return false and remain owned; failed/abandoned receipts are discarded
    /// without reporting a successful zero-time profile. False resets output.
    bool poll(U64& nanoseconds);

private:
    std::shared_ptr<const ALMetalCommandCompletion> mNativeCompletion;
};
