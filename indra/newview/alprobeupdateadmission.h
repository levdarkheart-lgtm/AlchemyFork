/**
 * @file alprobeupdateadmission.h
 * @brief Fair selection of fresh ordinary-probe update groups.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include <array>
#include <cstdint>

// The manager retains a credit for coherent continuations. This selects only
// NEW groups; serving realtime must not erase an older scheduled request's age.
class ALProbeUpdateAdmission final
{
public:
    enum class Kind : unsigned { scheduled, realtime };
    using Frame = std::uint64_t;
    void request(Kind kind, Frame frame) noexcept
    {
        auto& value = mPending[index(kind)];
        if (!value.waiting) value = {frame, true};
    }
    void cancel(Kind kind) noexcept { mPending[index(kind)] = {}; }
    bool waiting(Kind kind) const noexcept { return mPending[index(kind)].waiting; }
    Frame oldest(Kind kind) const noexcept { return mPending[index(kind)].frame; }
    bool mayStart(Kind kind) const noexcept
    {
        const auto& value = mPending[index(kind)];
        const auto& other = mPending[index(opposite(kind))];
        return value.waiting && (!other.waiting || value.frame < other.frame ||
            (value.frame == other.frame && mNext == kind));
    }
    void started(Kind kind) noexcept
    {
        cancel(kind);
        mNext = opposite(kind);
    }
    void reset() noexcept { mPending = {}; mNext = Kind::realtime; }
private:
    struct Pending { Frame frame = 0; bool waiting = false; };
    static unsigned index(Kind kind) noexcept { return static_cast<unsigned>(kind); }
    static Kind opposite(Kind kind) noexcept
    { return kind == Kind::scheduled ? Kind::realtime : Kind::scheduled; }
    std::array<Pending, 2> mPending{};
    // Retain the original realtime-first order when both first become ready.
    Kind mNext = Kind::realtime;
};
