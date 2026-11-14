/**
 * @file almetalframeadmission.cpp
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "almetalframeadmission.h"
#include <algorithm>

ALMetalFrameAdmission::ALMetalFrameAdmission(std::size_t frames, std::size_t width)
    : mMaxFrames(std::max<std::size_t>(frames, 1)),
      mMaxInvocations(std::max<std::size_t>(width, 1))
{}

ALMetalFrameAdmission::Decision ALMetalFrameAdmission::reserve(Frame frame)
{
    if (mHasLatestFrame && frame < mLatestFrame)
        return {Outcome::stale_frame, 0, 0};
    if (mHasLatestFrame && frame == mLatestFrame && mLatestInvocations >= mMaxInvocations)
        return {Outcome::same_frame_limit, 0, frame};
    const bool existing_frame = std::any_of(mSlots.begin(), mSlots.end(),
        [frame](const Slot& slot) { return slot.frame == frame; });
    if (!existing_frame && pendingFrameCount() >= mMaxFrames)
    {
        const Frame oldest = mSlots.front().frame;
        const bool unsubmitted = std::any_of(mSlots.begin(), mSlots.end(),
            [oldest](const Slot& slot) { return slot.frame == oldest && !slot.submitted; });
        return {unsubmitted ? Outcome::unsubmitted_prior_frame : Outcome::wait_for_prior_frame,
                0, oldest};
    }
    if (mNextTicket == 0) return {Outcome::ticket_exhausted, 0, 0};
    const Ticket ticket = mNextTicket;
    mSlots.push_back({ticket, frame, false});
    ++mNextTicket;
    if (!mHasLatestFrame || frame > mLatestFrame)
    {
        mHasLatestFrame = true;
        mLatestFrame = frame;
        mLatestInvocations = 0;
    }
    ++mLatestInvocations;
    return {Outcome::admitted, ticket, 0};
}

bool ALMetalFrameAdmission::submitted(Ticket reservation) noexcept
{
    const auto slot = std::find_if(mSlots.begin(), mSlots.end(),
        [reservation](const Slot& value) { return value.ticket == reservation; });
    if (slot == mSlots.end() || slot->submitted) return false;
    slot->submitted = true;
    return true;
}

bool ALMetalFrameAdmission::cancel(Ticket reservation) noexcept
{
    const auto slot = std::find_if(mSlots.begin(), mSlots.end(),
        [reservation](const Slot& value) { return value.ticket == reservation; });
    if (slot == mSlots.end() || slot->submitted) return false;
    if (slot->frame == mLatestFrame) --mLatestInvocations;
    mSlots.erase(slot);
    return true;
}

ALMetalFrameAdmission::Retirement ALMetalFrameAdmission::prune(
    std::span<const CompletionReceipt> receipts)
{
    Retirement result;
    for (const auto& receipt : receipts)
    {
        const auto slot = std::find_if(mSlots.begin(), mSlots.end(),
            [&receipt](const Slot& value) { return value.ticket == receipt.ticket; });
        if (slot == mSlots.end() || !slot->submitted)
        {
            ++result.ignored_receipts;
            continue;
        }
        if (receipt.state == CompletionState::pending) continue;
        if (receipt.state != CompletionState::completed && receipt.state != CompletionState::failed)
        {
            ++result.ignored_receipts;
            continue;
        }
        if (receipt.state == CompletionState::failed) result.failed.push_back(receipt.ticket);
        else result.completed.push_back(receipt.ticket);
        mSlots.erase(slot);
    }
    return result;
}

std::vector<ALMetalFrameAdmission::Ticket>
ALMetalFrameAdmission::submittedTickets(Frame frame) const
{
    std::vector<Ticket> result;
    for (const auto& slot : mSlots)
        if (slot.frame == frame && slot.submitted) result.push_back(slot.ticket);
    return result;
}

std::size_t ALMetalFrameAdmission::pendingFrameCount() const noexcept
{
    std::size_t result = 0;
    Frame previous = 0;
    for (const auto& slot : mSlots)
    {
        if (result == 0 || slot.frame != previous) ++result;
        previous = slot.frame;
    }
    return result;
}

std::size_t ALMetalFrameAdmission::pendingSubmissionCount() const noexcept
{
    return std::count_if(mSlots.begin(), mSlots.end(),
        [](const Slot& slot) { return slot.submitted; });
}

std::size_t ALMetalFrameAdmission::reservationCount() const noexcept
{
    return mSlots.size() - pendingSubmissionCount();
}

std::size_t ALMetalFrameAdmission::latestFrameInvocationCount() const noexcept
{
    return mLatestInvocations;
}
