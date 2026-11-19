/**
 * @file alworldretentioninputs.h
 * @brief Opt-in bounded snapshots of the actual retained-world boundary (PPD003).
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "alwarningdiagnostics.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <ostream>
#include <string_view>

namespace ALWorldRetentionInputs
{
enum class View : unsigned { other, main, hero, probe };
enum class BaseState : unsigned
{
    unobserved, prerequisite, ordinary_probes, legacy_environment,
    hero_inputs, staged, encoding_failed, sky_failed, encoded, cleared
};
inline const char* baseStateName(BaseState state)
{
    switch (state)
    {
    case BaseState::prerequisite: return "prerequisite";
    case BaseState::ordinary_probes: return "ordinary_probes";
    case BaseState::legacy_environment: return "legacy_environment";
    case BaseState::hero_inputs: return "hero_inputs";
    case BaseState::staged: return "staged";
    case BaseState::encoding_failed: return "encoding_failed";
    case BaseState::sky_failed: return "sky_failed";
    case BaseState::encoded: return "encoded";
    case BaseState::cleared: return "cleared";
    default: return "unobserved";
    }
}
struct Snapshot
{
    static constexpr std::uint64_t unknown = std::numeric_limits<std::uint64_t>::max();
    View view = View::other;
    std::uint64_t current_frame = 0, background_frame = unknown, capture_frame = unknown;
    std::uint64_t display_frame = unknown, presentation_frame = unknown;
    std::uint64_t exposure_frame = unknown, exposure_epoch = 0, expected_exposure_epoch = 0;
    std::uint64_t exposure_policy_frame = unknown, post_policy_frame = unknown;
    std::uint32_t first_incomplete_line = 0;
    bool complete = false, attempted = false, gbuffer = false;
    bool light_map = false, base_light = false, fused_light = false, composed_light = false;
    bool opaque_post = false, display_post = false, presentation = false;
    bool exposure_ready = false, exposure = false, hdr = false;
    bool meter_owner = false, adaptation = false, classic_lighting = false;
    bool legacy_gamma = false, no_post = false, dynamic_exposure_enabled = false;
    bool base_attempted = false, local_attempted = false, projector_attempted = false;
    bool composition_attempted = false, opaque_attempted = false;
    BaseState base_state = BaseState::unobserved;
    std::uint64_t base_state_frame = unknown;
    bool hero_selected = false, hero_ready = false, hero_advertised = false;
    bool hero_owner_match = false, hero_epoch_match = false;
    // Observation limits and time refer to this capture owner's Observer.
    // halted_seconds measures only its continuous, valid clock window.
    bool halted_time_known = false, repeat_event = false;
    double halted_seconds = 0.;
    unsigned observed_distinct_states = 0, observed_repeat_events = 0;
    bool distinct_cap_reached = false, repeat_cap_reached = false;

    bool current(std::uint64_t frame) const { return frame != unknown && frame == current_frame; }
    bool exposureCurrent() const
    {
        return exposure_ready && exposure && current(exposure_frame) &&
            exposure_epoch == expected_exposure_epoch;
    }
    bool exposureRequired() const
    {
        return current(post_policy_frame) && hdr && !no_post && dynamic_exposure_enabled;
    }
    // Classification of available products, not proof of the causal producer.
    const char* earliestMissing() const
    {
        if (!current(capture_frame)) return "current_capture";
        if (!complete) return "capture_incomplete";
        if (!gbuffer) return "gbuffer";
        // Fused staging may already have been consumed, and separate map/
        // base targets are optional in valid paths. Preserve all flags below
        // rather than inventing which lighting producer failed.
        if (!composed_light) return "unknown_with_lighting_readiness";
        if (!opaque_post) return "opaque_post";
        if (exposureRequired() && !exposureCurrent()) return "exposure";
        if (!display_post || !current(display_frame)) return "display_post";
        if (!presentation || !current(presentation_frame)) return "presentation";
        return "unknown_with_readiness_flags";
    }
    std::uint64_t readiness() const
    {
        const bool flags[] = {
            current(capture_frame), complete, attempted, gbuffer, light_map,
            base_light, fused_light, composed_light, opaque_post, display_post,
            current(display_frame), presentation, current(presentation_frame),
            current(exposure_policy_frame), current(post_policy_frame), hdr,
            meter_owner, adaptation, classic_lighting, legacy_gamma, no_post,
            dynamic_exposure_enabled, exposure_ready, exposure,
            exposure_epoch == expected_exposure_epoch, current(exposure_frame),
            base_attempted, local_attempted, projector_attempted,
            composition_attempted, opaque_attempted, current(base_state_frame),
            hero_selected, hero_ready, hero_advertised, hero_owner_match, hero_epoch_match};
        std::uint64_t mask = 0;
        for (unsigned i = 0; i < std::size(flags); ++i) if (flags[i]) mask |= std::uint64_t{1} << i;
        return mask;
    }
};
struct Key
{
    std::array<char, 96> data{};
    std::size_t size = 0;
    std::string_view view() const { return {data.data(), size}; }
};
inline Key key(const Snapshot& snapshot)
{
    Key result;
    // Deliberately excludes every frame/epoch/age and all runtime identities.
    const int size = std::snprintf(result.data.data(), result.data.size(), "v=%u;r=%llx;l=%u;b=%u",
        static_cast<unsigned>(snapshot.view), static_cast<unsigned long long>(snapshot.readiness()),
        snapshot.first_incomplete_line, static_cast<unsigned>(snapshot.base_state));
    if (size > 0 && static_cast<std::size_t>(size) < result.data.size()) result.size = size;
    return result;
}
inline const char* viewName(View view)
{
    switch (view)
    {
    case View::main: return "main";
    case View::hero: return "hero";
    case View::probe: return "probe";
    default: return "other";
    }
}
inline std::ostream& operator<<(std::ostream& out, const Snapshot& s)
{
    out << "view=" << viewName(s.view) << " current_frame=" << s.current_frame
        << " background_frame=" << s.background_frame << " background_age_known="
        << (s.background_frame != Snapshot::unknown && s.current_frame >= s.background_frame);
    if (s.background_frame != Snapshot::unknown && s.current_frame >= s.background_frame)
        out << " background_age=" << s.current_frame - s.background_frame;
    return out << " earliest_missing_product=" << s.earliestMissing()
        << " first_incomplete_line=" << s.first_incomplete_line
        << " capture(frame/current/complete/attempted)=" << s.capture_frame << '/' << s.current(s.capture_frame)
        << '/' << s.complete << '/' << s.attempted
        << " lighting(gbuffer/map/base/fused/composed)=" << s.gbuffer << '/' << s.light_map << '/'
        << s.base_light << '/' << s.fused_light << '/' << s.composed_light
        << " attempts(base/local/projector/composition/opaque)=" << s.base_attempted << '/'
        << s.local_attempted << '/' << s.projector_attempted << '/' << s.composition_attempted << '/' << s.opaque_attempted
        << " post(opaque/display/presentation)=" << s.opaque_post << '/' << s.display_post << '/' << s.presentation
        << " display(frame/current)=" << s.display_frame << '/' << s.current(s.display_frame)
        << " presentation(frame/current)=" << s.presentation_frame << '/' << s.current(s.presentation_frame)
        << " exposure(ready/target/current/epoch_match)=" << s.exposure_ready << '/' << s.exposure << '/'
        << s.current(s.exposure_frame) << '/' << (s.exposure_epoch == s.expected_exposure_epoch)
        << " exposure_frame=" << s.exposure_frame << " exposure_epoch=" << s.exposure_epoch
        << " expected_exposure_epoch=" << s.expected_exposure_epoch
        << " exposure_policy(frame/current/meter/adapt/classic)=" << s.exposure_policy_frame << '/'
        << s.current(s.exposure_policy_frame) << '/' << s.meter_owner << '/' << s.adaptation << '/' << s.classic_lighting
        << " post_policy(frame/current/gamma/no_post/dynamic/hdr)=" << s.post_policy_frame << '/'
        << s.current(s.post_policy_frame) << '/' << s.legacy_gamma << '/' << s.no_post << '/'
        << s.dynamic_exposure_enabled << '/' << s.hdr
        << " base_state=" << baseStateName(s.base_state)
        << " base_state(frame/current)=" << s.base_state_frame << '/' << s.current(s.base_state_frame)
        << " hero(selected/ready/advertised/owner_match/epoch_match)=" << s.hero_selected << '/'
        << s.hero_ready << '/' << s.hero_advertised << '/' << s.hero_owner_match << '/' << s.hero_epoch_match
        << " halted(time_known/seconds)=" << s.halted_time_known << '/' << s.halted_seconds
        << " observation(distinct/repeats/distinct_cap/repeat_cap/repeat)="
        << s.observed_distinct_states << '/' << s.observed_repeat_events << '/'
        << s.distinct_cap_reached << '/' << s.repeat_cap_reached << '/' << s.repeat_event;
}

// Owner-thread confined; no retained runtime identities or heap allocation.
// A capture owner emits at most64 distinct states plus64 repeats in its lifetime.
// Recovery resets timing, not either budget. The final admitted slot reports its
// cap, so silence after exhaustion is distinguishable from continuous success.
class Observer
{
public:
    static constexpr unsigned distinct_limit = 64, repeat_limit = 64;
    static constexpr double initial_halt_seconds = .1, repeat_seconds = 1.;

    void recovered() noexcept
    {
        mActive = mClockKnown = mRepeatedSinceProgress = false;
    }

    template<class Clock, class Collect, class Emit>
    void observe(Clock&& clock, Collect&& collect, Emit&& emit) noexcept
    {
        // Do not even read clock/metadata or construct a key while off.
        if (!ALWarningDiagnostics::enabled()) return;
        try
        {
            const double now = clock();
            Snapshot snapshot = collect();
            if (snapshot.background_frame == snapshot.current_frame)
            {
                recovered();
                return;
            }
            const Key event_key = key(snapshot);
            if (!event_key.size) return;
            const bool initial = !mActive;
            const bool changed = !initial && event_key.view() != mLastKey.view();
            const bool progressed = initial || snapshot.background_frame != mBackgroundFrame;
            const bool finite = std::isfinite(now);
            const bool continuous_clock = finite && mClockKnown && now >= mLastClock;
            // Unknown/backward clocks cannot fabricate elapsed time. Rebase
            // the next valid observation window, retaining all output limits.
            if (initial || progressed || !continuous_clock)
            {
                mProgressTime = mLastReportTime = finite ? now : 0.;
                mRepeatedSinceProgress = false;
            }
            snapshot.halted_time_known = finite && (initial || continuous_clock);
            snapshot.halted_seconds = snapshot.halted_time_known ? now - mProgressTime : 0.;
            const double cadence = mRepeatedSinceProgress ? repeat_seconds : initial_halt_seconds;
            const bool timed_repeat = !initial && !changed && !progressed && continuous_clock &&
                now - mProgressTime >= initial_halt_seconds && now - mLastReportTime >= cadence;
            mActive = true;
            mClockKnown = finite;
            mLastClock = finite ? now : 0.;
            mBackgroundFrame = snapshot.background_frame;
            mLastKey = event_key;
            if (!initial && !changed && !timed_repeat) return;
            unsigned at = 0;
            while (at < mDistinct && mKeys[at].view() != event_key.view()) ++at;
            const bool repeat = at != mDistinct;
            if (repeat)
            {
                if (mRepeats == repeat_limit) return;
                ++mRepeats;
            }
            else
            {
                if (mDistinct == distinct_limit) return;
                mKeys[mDistinct++] = event_key;
            }
            snapshot.observed_distinct_states = mDistinct;
            snapshot.observed_repeat_events = mRepeats;
            snapshot.distinct_cap_reached = mDistinct == distinct_limit;
            snapshot.repeat_cap_reached = mRepeats == repeat_limit;
            snapshot.repeat_event = repeat;
            if (finite) mLastReportTime = now;
            if (timed_repeat) mRepeatedSinceProgress = true;
            emit(snapshot);
        }
        catch (...) {} // Optional evidence cannot alter rendering or input.
    }

private:
    std::array<Key, distinct_limit> mKeys{};
    Key mLastKey{};
    unsigned mDistinct = 0, mRepeats = 0;
    bool mActive = false, mClockKnown = false, mRepeatedSinceProgress = false;
    std::uint64_t mBackgroundFrame = Snapshot::unknown;
    double mLastClock = 0., mProgressTime = 0., mLastReportTime = 0.;
};
}
