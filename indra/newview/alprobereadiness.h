// Copyright (c) 2026, Alchemy Viewer Project.
// SPDX-License-Identifier: LGPL-2.1-or-later

/**
 * @file alprobereadiness.h
 * @brief Opt-in, bounded probe-readiness observations for benchmark evidence.
 */
#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace ALProbeReadiness
{
inline constexpr std::uint32_t capacity = 256;
inline constexpr std::uint32_t required_stable_frames = 120;

// These are ephemeral, opaque identities used ONLY for exact in-process
// membership comparison. Never serialize, hash into evidence, or retain an
// owner through this structure; retaining a probe changes its lifetime.
struct Member
{
    std::uintptr_t owner = 0;
    std::int32_t slot = -1;
    std::uint64_t generation = 0;
    bool main_visible = false;
    bool operator==(const Member&) const = default;
};

struct Snapshot
{
    std::uint64_t frame_index = 0; // publish() stamps the real gFrameCount.
    bool enabled = false;
    bool layout_ready = false;
    bool invalid = false; // Overflow, invalid slot, duplicate ownership, etc.
    bool reset = false;
    bool paused = false;
    bool reset_fade_pending = false; // Includes nonfinite or subunit fade.
    bool default_ready = false;
    bool native_pairs_required = false;
    bool updating_incomplete = false;
    // Routine refresh is informational, deliberately NOT a readiness blocker.
    bool refreshing = false;
    bool radiance_pass = false;
    std::uint32_t updating_face = 0;
    std::uint32_t budget = 0;
    std::uint32_t dynamic_count = 0;
    std::uint32_t resolution = 0;
    std::uint32_t queued_create = 0;
    std::uint32_t queued_kill = 0;
    std::uint32_t assigned = 0;
    std::uint32_t incomplete = 0;
    std::uint32_t fading = 0; // Includes nonfinite fades.
    std::uint32_t unpublished_pairs = 0;
    std::uint32_t main_visible = 0;
    std::uint32_t member_count = 0;
    // Canonical ascending slot order, including assigned default slot0.
    std::array<Member, capacity> members{};
};

namespace Detail { inline bool requested = false; }
// Inline guard: no out-of-line call, allocation or atomics while disabled.
inline bool observationRequested() noexcept { return Detail::requested; }
// Single main-thread owner. Explicit session arming/disarming clears old data.
void requestObservation(bool requested) noexcept;
void publish(std::uint64_t frame, const Snapshot& snapshot) noexcept;
// Exact frame only; absent, disabled and stale observations return nullopt.
std::optional<Snapshot> observe(std::uint64_t frame) noexcept;

struct Status
{
    bool fresh = false;
    bool valid = false;
    bool ready = false;
    bool membership_changed = false;
    std::uint32_t consecutive_ready_frames = 0;
    bool admitted() const noexcept
    { return ready && consecutive_ready_frames >= required_stable_frames; }
};

class Gate
{
public:
    Status observe(std::uint64_t frame, const Snapshot* snapshot) noexcept;
    void reset() noexcept;
    const Status& status() const noexcept { return mStatus; }
private:
    std::optional<Snapshot> mPrevious;
    std::uint64_t mLastFrame = 0;
    Status mStatus;
};
}
