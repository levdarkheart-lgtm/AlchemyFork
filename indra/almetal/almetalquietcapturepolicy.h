/**
 * @file almetalquietcapturepolicy.h
 * @brief Bounded quiet-world capture triggers; no renderer or clock ownership.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#pragma once

#include <cmath>
#include <cstdint>

class ALMetalQuietCapturePolicy final
{
public:
    static constexpr std::uint64_t second = 1000000000ull;
    struct Config
    {
        std::uint64_t early_age_ns = 2 * second;
        std::uint32_t early_min_intervals = 4;
        std::uint64_t late_min_age_ns = 60 * second;
        std::uint64_t decline_window_ns = 10 * second;
        double decline_period_ratio = 1.35;
        double adjacent_window_mean_tolerance = 0.10;
        std::uint64_t fallback_age_ns = 90 * second;
        std::uint64_t maximum_age_ns = 300 * second;
        // Opt-in orchestration may explicitly rearm after Home -> club.
        // Two attempts per generation, at most four for the whole session.
        std::uint32_t maximum_scene_generations = 2;
    };
    struct Input
    {
        std::uint64_t now_ns = 0;
        std::uint64_t frame = 0;
        bool world_ready = false;
        bool normal_display = false;
        bool cadence_valid = false; // Visible/focused; no artificial pacing.
        bool world_invalidated = false; // Teleport/disconnect/logout after first world.
        bool previous_interval_captured = false;
    };
    enum class Action : std::uint8_t
    {
        none,
        early,
        late_decline,
        late_fallback,
        invalidated,
        expired
    };
    struct Stats
    {
        std::uint32_t attempts = 0;
        std::uint32_t scene_generation = 1;
        std::uint32_t total_attempts = 0;
        std::uint32_t normal_intervals = 0;
        std::uint32_t cadence_suspensions = 0;
        std::uint64_t world_start_ns = 0;
        std::uint64_t baseline_periods = 0;
        std::uint64_t baseline_total_ns = 0;
        double baseline_mean_period_ns = 0;
        double previous_window_mean_period_ns = 0;
        double last_window_mean_period_ns = 0;
        bool started = false;
        bool terminal = false;
        bool capture_failed = false;
        Action last_action = Action::none;
    };

    ALMetalQuietCapturePolicy() noexcept : ALMetalQuietCapturePolicy(Config{}) {}
    explicit ALMetalQuietCapturePolicy(Config config) noexcept : mConfig(config)
    {
        if (config.early_age_ns == 0 || config.early_min_intervals < 4 ||
            config.late_min_age_ns < config.early_age_ns ||
            config.decline_window_ns == 0 ||
            !std::isfinite(config.decline_period_ratio) || config.decline_period_ratio <= 1 ||
            !std::isfinite(config.adjacent_window_mean_tolerance) ||
            config.adjacent_window_mean_tolerance < 0 || config.adjacent_window_mean_tolerance > 1 ||
            config.fallback_age_ns < config.late_min_age_ns ||
            config.maximum_age_ns < config.fallback_age_ns ||
            config.maximum_age_ns > 300 * second ||
            config.maximum_scene_generations < 1 || config.maximum_scene_generations > 2)
            mStats.terminal = true;
    }

    const Config& config() const noexcept { return mConfig; }
    const Stats& stats() const noexcept { return mStats; }

    // The caller owns teleport/disconnect distinction and native cancellation.
    // Invoke only after cancelling/releasing the old capture and observing a
    // supported, connected post-teleport arrival. Never silently restart after
    // a failed capture or expiry. No clock, settings, renderer or GPU is owned.
    bool rearmAfterInvalidation(const Input& arrival) noexcept
    {
        if (!mStats.started || !mStats.terminal || !mEndReported ||
            mStats.capture_failed || mStats.last_action != Action::invalidated ||
            mStats.scene_generation >= mConfig.maximum_scene_generations ||
            mStats.total_attempts >= 2u * mConfig.maximum_scene_generations ||
            !arrival.world_ready || !arrival.normal_display || !arrival.cadence_valid ||
            arrival.world_invalidated || arrival.now_ns <= mInvalidatedNs ||
            arrival.frame <= mInvalidatedFrame)
            return false;
        const auto generation = mStats.scene_generation + 1;
        const auto total_attempts = mStats.total_attempts;
        mStats = Stats{};
        mStats.scene_generation = generation;
        mStats.total_attempts = total_attempts;
        mEndReported = false;
        mHavePrevious = false;
        mPreviousNs = 0;
        mHaveFrame = true;
        mLastFrame = mInvalidatedFrame;
        resetWindow();
        return true;
    }

    // Invoke before normal display commands, with the *previous* interval's
    // capture status. Start-to-start periods include ordinary intervening
    // probes and idle work. Nested entries and duplicate frames add no sample.
    // Each returned capture action consumes one of TWO generation attempts
    // before native work is requested; a failed request terminates without retry.
    Action update(const Input& input) noexcept
    {
        // A second capture can still be in flight after its attempt exhausts
        // the trigger budget. World invalidation/timeout must cancel it too.
        if (!mStats.capture_failed && mStats.started && input.world_invalidated)
        {
            if (input.now_ns > mInvalidatedNs) mInvalidatedNs = input.now_ns;
            if (mHavePrevious && mPreviousNs > mInvalidatedNs) mInvalidatedNs = mPreviousNs;
            if (input.frame > mInvalidatedFrame) mInvalidatedFrame = input.frame;
            if (mHaveFrame && mLastFrame > mInvalidatedFrame) mInvalidatedFrame = mLastFrame;
            if (!mEndReported) return terminal(Action::invalidated);
        }
        if (!mEndReported && !mStats.capture_failed && mStats.started && input.now_ns >= mStats.world_start_ns &&
            input.now_ns - mStats.world_start_ns >= mConfig.maximum_age_ns)
            return terminal(Action::expired);
        if (mStats.terminal) return Action::none;
        if (!input.cadence_valid) suspend();
        if (!input.normal_display) return Action::none;
        if (mHaveFrame && input.frame <= mLastFrame) return Action::none;
        mHaveFrame = true;
        mLastFrame = input.frame;
        if (!input.world_ready || !input.cadence_valid)
        {
            suspend();
            return Action::none;
        }
        if (!mStats.started)
        {
            mStats.started = true;
            mStats.world_start_ns = input.now_ns;
        }
        if (input.now_ns < mStats.world_start_ns ||
            (mHavePrevious && input.now_ns <= mPreviousNs))
        {
            suspend();
            return Action::none;
        }
        const std::uint64_t age = input.now_ns - mStats.world_start_ns;
        ++mStats.normal_intervals;
        const bool valid_period = mHavePrevious && !input.previous_interval_captured;
        const std::uint64_t period = valid_period ? input.now_ns - mPreviousNs : 0;
        mPreviousNs = input.now_ns;
        mHavePrevious = true;
        if (input.previous_interval_captured) invalidateWindows();

        if (mStats.attempts == 0)
        {
            // Only the first configured early-age period forms the baseline;
            // a delayed fourth display never extends it into loading/late work.
            if (valid_period && age <= mConfig.early_age_ns)
            {
                ++mStats.baseline_periods;
                mStats.baseline_total_ns += period;
            }
            if (age < mConfig.early_age_ns ||
                mStats.normal_intervals < mConfig.early_min_intervals)
                return Action::none;
            if (mStats.baseline_periods >= 3)
                mStats.baseline_mean_period_ns =
                    static_cast<double>(mStats.baseline_total_ns) / mStats.baseline_periods;
            invalidateWindows();
            return attempt(Action::early);
        }

        if (valid_period)
        {
            mWindowTotalNs += period;
            ++mWindowPeriods;
            if (mWindowTotalNs >= mConfig.decline_window_ns)
            {
                mStats.previous_window_mean_period_ns = mStats.last_window_mean_period_ns;
                mStats.last_window_mean_period_ns =
                    static_cast<double>(mWindowTotalNs) / mWindowPeriods;
                const double previous = mStats.previous_window_mean_period_ns;
                const double current = mStats.last_window_mean_period_ns;
                const double threshold = mStats.baseline_mean_period_ns * mConfig.decline_period_ratio;
                const bool declined = age >= mConfig.late_min_age_ns &&
                    mStats.baseline_mean_period_ns > 0 &&
                    previous >= threshold && current >= threshold &&
                    std::abs(current - previous) <=
                        previous * mConfig.adjacent_window_mean_tolerance;
                resetWindow();
                if (declined) return attempt(Action::late_decline);
            }
        }
        if (age >= mConfig.fallback_age_ns) return attempt(Action::late_fallback);
        return Action::none;
    }

    void markCaptureFailed() noexcept
    {
        mStats.capture_failed = true;
        mStats.terminal = true;
    }

private:
    Action terminal(Action action) noexcept
    {
        mEndReported = true;
        mStats.terminal = true;
        mStats.last_action = action;
        return action;
    }
    Action attempt(Action action) noexcept
    {
        ++mStats.attempts;
        ++mStats.total_attempts;
        mStats.last_action = action;
        if (mStats.attempts == 2) mStats.terminal = true;
        return action;
    }
    void resetWindow() noexcept { mWindowTotalNs = mWindowPeriods = 0; }
    void invalidateWindows() noexcept
    {
        resetWindow();
        mStats.previous_window_mean_period_ns = mStats.last_window_mean_period_ns = 0;
    }
    void suspend() noexcept
    {
        if (mHavePrevious) ++mStats.cadence_suspensions;
        mHavePrevious = false;
        invalidateWindows();
    }
    Config mConfig;
    Stats mStats;
    bool mHaveFrame = false;
    bool mEndReported = false;
    bool mHavePrevious = false;
    std::uint64_t mLastFrame = 0;
    std::uint64_t mPreviousNs = 0;
    std::uint64_t mWindowTotalNs = 0;
    std::uint64_t mWindowPeriods = 0;
    std::uint64_t mInvalidatedNs = 0;
    std::uint64_t mInvalidatedFrame = 0;
};
