// Copyright (c) 2026, Alchemy Viewer Project.
// SPDX-License-Identifier: LGPL-2.1-or-later

/** @file alprobereadiness.cpp
 * Fixed-capacity, single-main-owner benchmark observations; no renderer work.
 */
#include "alprobereadiness.h"

#include <algorithm>

namespace ALProbeReadiness
{
namespace
{
std::optional<Snapshot> latest;

bool valid(const Snapshot& s) noexcept
{
    if (s.invalid || s.budget == 0 || s.budget > capacity ||
        s.dynamic_count > capacity || s.assigned > capacity ||
        s.member_count != s.assigned || s.main_visible > s.assigned ||
        s.incomplete > s.assigned || s.fading > s.assigned ||
        s.unpublished_pairs > s.assigned)
        return false;
    unsigned visible = 0;
    for (std::uint32_t i = 0; i < s.member_count; ++i)
    {
        const auto& member = s.members[i];
        if (!member.owner || member.slot < 0 ||
            static_cast<std::uint32_t>(member.slot) >= s.budget ||
            (i && s.members[i - 1].slot >= member.slot))
            return false;
        // One owner cannot simultaneously own two assigned cube slots.
        for (std::uint32_t j = 0; j < i; ++j)
            if (s.members[j].owner == member.owner) return false;
        visible += member.main_visible;
    }
    return visible == s.main_visible;
}

bool ready(const Snapshot& s) noexcept
{
    if (!s.enabled || !s.layout_ready || s.reset || s.paused ||
        s.reset_fade_pending || !s.default_ready || s.updating_incomplete ||
        s.dynamic_count != s.budget || !s.resolution ||
        s.queued_create || s.queued_kill || !s.assigned ||
        s.incomplete || s.fading || s.unpublished_pairs ||
        s.members[0].slot != 0)
        return false;
    if (s.native_pairs_required)
        for (std::uint32_t i = 0; i < s.member_count; ++i)
            if (!s.members[i].generation) return false;
    return true;
}

bool sameMembership(const Snapshot& a, const Snapshot& b) noexcept
{
    if (a.enabled != b.enabled || a.budget != b.budget ||
        a.dynamic_count != b.dynamic_count || a.resolution != b.resolution ||
        a.native_pairs_required != b.native_pairs_required ||
        a.member_count != b.member_count)
        return false;
    return std::equal(a.members.begin(), a.members.begin() + a.member_count,
                      b.members.begin());
}
}

void requestObservation(bool requested) noexcept
{
    Detail::requested = requested;
    latest.reset();
}

void publish(std::uint64_t frame, const Snapshot& snapshot) noexcept
{
    if (!observationRequested()) return;
    latest = snapshot;
    latest->frame_index = frame;
}

std::optional<Snapshot> observe(std::uint64_t frame) noexcept
{
    if (!observationRequested() || !latest || latest->frame_index != frame)
        return std::nullopt;
    return latest;
}

void Gate::reset() noexcept
{
    mPrevious.reset();
    mLastFrame = 0;
    mStatus = {};
}

Status Gate::observe(std::uint64_t frame, const Snapshot* snapshot) noexcept
{
    const Status previous = mStatus;
    mStatus = {};
    mStatus.fresh = snapshot && snapshot->frame_index == frame;
    if (!mStatus.fresh || !valid(*snapshot))
    {
        mPrevious.reset();
        mLastFrame = 0;
        return mStatus;
    }
    mStatus.valid = true;
    mStatus.ready = ready(*snapshot);
    mStatus.membership_changed = mPrevious && !sameMembership(*mPrevious, *snapshot);
    if (mStatus.ready)
    {
        const bool consecutive = mPrevious && frame == mLastFrame + 1;
        mStatus.consecutive_ready_frames = consecutive && previous.ready &&
            !mStatus.membership_changed
            ? std::min(previous.consecutive_ready_frames + 1, required_stable_frames)
            : 1;
    }
    mPrevious = *snapshot;
    mLastFrame = frame;
    return mStatus;
}
}
