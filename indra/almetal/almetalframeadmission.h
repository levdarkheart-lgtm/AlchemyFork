/**
 * @file almetalframeadmission.h
 * @brief Pure render-owner policy for bounded, frame-tagged submissions.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Each render owner supplies its own authority and finite transaction width:
// ordinary-main and shadow groups never share reservations. The native owner
// keeps the commands and supplies their actual completion receipts.
// Call only from that renderer's submission owner; this class does not wait,
// submit work, infer GPU completion or invoke callbacks during destruction.
class ALMetalFrameAdmission final
{
public:
    using Frame = std::uint64_t;
    using Ticket = std::uint64_t;
    enum class Outcome
    {
        admitted,
        wait_for_prior_frame,
        same_frame_limit,
        unsubmitted_prior_frame,
        stale_frame,
        ticket_exhausted
    };
    enum class CompletionState { pending, completed, failed };
    struct Decision
    {
        Outcome outcome = Outcome::ticket_exhausted;
        Ticket reservation = 0;
        Frame blocking_frame = 0; // meaningful only for a blocking outcome
        explicit operator bool() const noexcept { return outcome == Outcome::admitted; }
    };
    struct CompletionReceipt
    {
        Ticket ticket = 0;
        CompletionState state = CompletionState::pending;
    };
    struct Retirement
    {
        std::vector<Ticket> completed;
        std::vector<Ticket> failed; // native owner must propagate these errors
        std::size_t ignored_receipts = 0;
    };

    // Zero clamps to one; it never requests an unbounded budget.
    explicit ALMetalFrameAdmission(std::size_t max_pending_frames = 2,
                                   std::size_t max_invocations_per_frame = 6);
    ALMetalFrameAdmission(const ALMetalFrameAdmission&) = delete;
    ALMetalFrameAdmission& operator=(const ALMetalFrameAdmission&) = delete;
    ALMetalFrameAdmission(ALMetalFrameAdmission&&) noexcept = default;
    ALMetalFrameAdmission& operator=(ALMetalFrameAdmission&&) noexcept = default;

    // Frame serials are monotonic per budget owner. Repeated admission in the
    // current frame never waits for that same frame. Completed submissions
    // still count toward its total invocation bound; cancelled reservations
    // may retry without consuming it. Prune actual native states first.
    Decision reserve(Frame frame);
    // Call only after native commit succeeds; a failed commit cancels its
    // still-unsubmitted reservation instead.
    bool submitted(Ticket reservation) noexcept;
    // Only an uncommitted reservation may be cancelled. A committed command
    // remains pending until an explicit completion/error receipt is supplied.
    bool cancel(Ticket reservation) noexcept;
    Retirement prune(std::span<const CompletionReceipt> receipts);

    // On wait_for_prior_frame, the native owner waits/inspects every remaining
    // command in blocking_frame, then prunes the actual results and retries.
    // unsubmitted_prior_frame requires cancellation/settlement, never a wait.
    std::vector<Ticket> submittedTickets(Frame frame) const;
    std::size_t pendingFrameCount() const noexcept;
    std::size_t pendingSubmissionCount() const noexcept;
    std::size_t reservationCount() const noexcept;
    std::size_t latestFrameInvocationCount() const noexcept;

private:
    struct Slot { Ticket ticket; Frame frame; bool submitted; };
    std::size_t mMaxFrames, mMaxInvocations;
    std::vector<Slot> mSlots;
    Ticket mNextTicket = 1;
    Frame mLatestFrame = 0;
    bool mHasLatestFrame = false;
    std::size_t mLatestInvocations = 0;
};
