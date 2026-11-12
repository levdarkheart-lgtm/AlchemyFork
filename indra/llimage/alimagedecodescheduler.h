/**
 * @file alimagedecodescheduler.h
 * @brief Bounded admission for image decoding, independent of worker lifetime.
 *
 * Copyright (c) 2026, Alchemy Viewer contributors.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef AL_IMAGE_DECODE_SCHEDULER_H
#define AL_IMAGE_DECODE_SCHEDULER_H

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

// This admits tasks; it neither reserves CPU cores nor chooses their affinity.
// Policy thresholds below are initial empirical choices, not hardware limits.
class ALImageDecodeScheduler
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Now = std::function<TimePoint()>;
    using Ticket = std::uint64_t;

    struct Limits
    {
        unsigned minimum = 2;
        unsigned baseline = 2;
        unsigned maximum = 2;
        unsigned active_cores = 0;
        unsigned effective_cores = 0;
        bool topology_valid = false;

        // Performance levels must be ordered fastest first and describe the
        // active physical cores. An incomplete/inconsistent topology falls back
        // to total - 4. Preserve the historical two-worker floor even if the
        // reported/effective total is 0 or 1; otherwise do not exceed that total.
        static Limits fromTopology(unsigned active,
                                   const std::vector<unsigned>& performance_levels,
                                   unsigned emulate = 0)
        {
            Limits result;
            result.active_cores = active;
            result.effective_cores = emulate ? std::min(active, emulate) : active;
            std::uint64_t sum = 0;
            bool valid = active != 0 && !performance_levels.empty();
            for (unsigned count : performance_levels)
            {
                valid = valid && count != 0;
                sum += count;
            }
            result.topology_valid = valid && sum == active;
            const unsigned available = result.topology_valid
                ? std::min(performance_levels.front(), result.effective_cores)
                : result.effective_cores;
            const unsigned headroom = result.topology_valid ? 2 : 4;
            const unsigned candidate = available > headroom ? available - headroom : 0;
            const unsigned ceiling = std::max(2u, std::min(16u, result.effective_cores));
            result.maximum = std::clamp(candidate, 2u, ceiling);
            result.baseline = std::clamp((2 * result.maximum + 2) / 3, 2u, result.maximum);
            return result;
        }
    };

    struct Pressure
    {
        double system_busy = 0;       // Interval CPU busy fraction, not GPU/FPS.
        double main_thread_busy = 0;  // Main CPU time / sampled wall interval.
        bool cpu_valid = false;
        int thermal_level = -1;       // -1 unknown, 0 nominal, 1 fair, 2 serious, 3 critical.
        bool low_power = false;
    };

    struct Stats
    {
        std::uint64_t count = 0;
        double total_s = 0;
        double max_s = 0;
    };

    struct Window
    {
        // Ten 500 ms buckets: bounded telemetry storage, boundary resolution
        // 500 ms. duration_s is the represented portion of those buckets.
        double duration_s = 0;
        double resolution_s = 0.5;
        std::uint64_t submitted = 0;
        std::uint64_t started = 0;
        std::uint64_t completed = 0;
        std::uint64_t cancelled = 0;
        Stats queue_wait;
        Stats decode_wall;   // Only jobs whose permit.decoded() was called.
        Stats service_wall;  // Full admitted lifetime, including completion callback.
    };

    struct Snapshot
    {
        unsigned minimum = 2;
        unsigned baseline = 2;
        unsigned maximum = 2;
        unsigned current = 2;
        bool adaptive = true;
        bool draining = false;
        std::size_t waiting = 0; // Registered tickets: WorkQueue plus gate waiters.
        std::size_t active = 0;
        double oldest_wait_s = 0;
        std::uint64_t submitted = 0;
        std::uint64_t started = 0;
        std::uint64_t completed = 0;
        std::uint64_t cancelled = 0;
        Stats queue_wait;
        Stats decode_wall;
        Stats service_wall;
        Window window;
        double last_interval_occupancy = 0;
        double last_interval_saturated_backlog_s = 0;
        std::string transition_reason;
        TimePoint last_transition{};
    };

private:
    static double seconds(Clock::duration value)
    {
        return std::max(0.0, std::chrono::duration<double>(value).count());
    }

    static void add(Stats& stats, double value)
    {
        ++stats.count;
        stats.total_s += value;
        stats.max_s = std::max(stats.max_s, value);
    }

    static void merge(Stats& into, const Stats& other)
    {
        into.count += other.count;
        into.total_s += other.total_s;
        into.max_s = std::max(into.max_s, other.max_s);
    }

    struct State
    {
        struct Bucket : Window
        {
            std::uint64_t ordinal = (std::numeric_limits<std::uint64_t>::max)();
        };

        State(Limits input, bool enabled, Now clock)
            : limits(input), adaptive(enabled), now(std::move(clock))
        {
            limits.minimum = std::clamp(limits.minimum, 2u, 16u);
            limits.maximum = std::clamp(limits.maximum, limits.minimum, 16u);
            limits.baseline = std::clamp(limits.baseline, limits.minimum, limits.maximum);
            current = adaptive ? limits.baseline : limits.maximum;
            origin = accounted = last_evaluation = last_transition = quiet_since = now();
            recovery_not_before = last_quiet_reduction = last_cpu_reduction = origin;
            reason = adaptive ? "baseline" : "fixed";
        }

        TimePoint advance(TimePoint time)
        {
            // A supplied clock must be monotonic. Clamp regressions so invalid
            // samples cannot produce negative telemetry or early recovery.
            time = std::max(time, accounted);
            const double elapsed = seconds(time - accounted);
            occupied_slot_s += std::min<std::size_t>(active, current) * elapsed;
            available_slot_s += current * elapsed;
            if (!pending.empty() && active >= current)
                saturated_backlog_s += elapsed;
            accounted = time;
            return time;
        }

        void refreshQuiet(TimePoint time)
        {
            const bool empty = pending.empty() && active == 0;
            if (empty && !quiet)
                quiet_since = time;
            quiet = empty;
        }

        std::uint64_t ordinal(TimePoint time) const
        {
            return static_cast<std::uint64_t>(seconds(time - origin) / 0.5);
        }

        Bucket& bucket(TimePoint time)
        {
            const auto index = ordinal(time);
            Bucket& result = buckets[index % buckets.size()];
            if (result.ordinal != index)
            {
                result = Bucket{};
                result.ordinal = index;
            }
            return result;
        }

        Window window(TimePoint time) const
        {
            Window result;
            const auto index = ordinal(time);
            const auto first = index >= buckets.size() - 1 ? index - (buckets.size() - 1) : 0;
            result.duration_s = std::max(0.0, seconds(time - origin) - first * 0.5);
            for (const auto& item : buckets)
            {
                if (item.ordinal < first || item.ordinal > index)
                    continue;
                result.submitted += item.submitted;
                result.started += item.started;
                result.completed += item.completed;
                result.cancelled += item.cancelled;
                merge(result.queue_wait, item.queue_wait);
                merge(result.decode_wall, item.decode_wall);
                merge(result.service_wall, item.service_wall);
            }
            return result;
        }

        void change(unsigned value, const char* why, TimePoint time)
        {
            if (value != current)
            {
                current = value;
                reason = why;
                last_transition = time;
            }
        }

        void finish(TimePoint start) noexcept
        {
            TimePoint time;
            try { time = now(); }
            catch (...) { time = Clock::now(); }
            {
                std::lock_guard<std::mutex> lock(mutex);
                time = advance(time);
                --active;
                ++completed;
                const double elapsed = seconds(time - start);
                add(service_wall, elapsed);
                auto& item = bucket(time);
                ++item.completed;
                add(item.service_wall, elapsed);
                refreshQuiet(time);
            }
            changed.notify_all();
        }

        Limits limits;
        bool adaptive;
        Now now;
        std::mutex mutex;
        std::condition_variable changed;
        unsigned current = 2;
        bool draining = false;
        Ticket next_ticket = 1;
        std::map<Ticket, TimePoint> pending;
        std::size_t active = 0;
        std::uint64_t submitted = 0, started = 0, completed = 0, cancelled = 0;
        Stats queue_wait, decode_wall, service_wall;
        std::array<Bucket, 10> buckets;
        TimePoint origin, accounted, last_evaluation, last_transition, quiet_since;
        TimePoint recovery_not_before, last_quiet_reduction, last_cpu_reduction;
        TimePoint busy_pressure_since{};
        bool quiet = true, busy_pressure = false;
        double occupied_slot_s = 0, available_slot_s = 0, saturated_backlog_s = 0;
        double last_interval_occupancy = 0, last_interval_saturated_backlog_s = 0;
        std::string reason;
    };

public:
    class Permit
    {
    public:
        Permit() = default;
        Permit(const Permit&) = delete;
        Permit& operator=(const Permit&) = delete;
        Permit(Permit&& other) noexcept
            : mState(std::move(other.mState)), mStart(other.mStart), mDecoded(other.mDecoded) {}
        Permit& operator=(Permit&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                mState = std::move(other.mState);
                mStart = other.mStart;
                mDecoded = other.mDecoded;
            }
            return *this;
        }
        ~Permit() { reset(); }
        explicit operator bool() const noexcept { return bool(mState); }

        // Call immediately after processRequest(), before finishRequest().
        // Completion/service statistics are still recorded if an exception
        // bypasses this marker; decode_wall counts only explicit markers.
        void decoded()
        {
            if (!mState || mDecoded)
                return;
            auto time = mState->now();
            std::lock_guard<std::mutex> lock(mState->mutex);
            time = mState->advance(time);
            const double elapsed = seconds(time - mStart);
            add(mState->decode_wall, elapsed);
            add(mState->bucket(time).decode_wall, elapsed);
            mDecoded = true;
        }

        void reset() noexcept
        {
            auto state = std::move(mState);
            if (state)
                state->finish(mStart);
        }

    private:
        friend class ALImageDecodeScheduler;
        Permit(std::shared_ptr<State> state, TimePoint start)
            : mState(std::move(state)), mStart(start) {}
        std::shared_ptr<State> mState;
        TimePoint mStart{};
        bool mDecoded = false;
    };

    explicit ALImageDecodeScheduler(Limits limits, bool adaptive = true, Now now = {})
        : mState(std::make_shared<State>(limits, adaptive,
              now ? std::move(now) : Now([] { return Clock::now(); }))) {}
    ALImageDecodeScheduler(const ALImageDecodeScheduler&) = delete;
    ALImageDecodeScheduler& operator=(const ALImageDecodeScheduler&) = delete;
    ~ALImageDecodeScheduler() { beginDrain(); }

    Ticket submit()
    {
        auto state = mState;
        const auto supplied = state->now();
        std::lock_guard<std::mutex> lock(state->mutex);
        const auto time = state->advance(supplied);
        if (state->draining || state->next_ticket == 0)
            return 0;
        const Ticket ticket = state->next_ticket++;
        state->pending.emplace(ticket, time);
        ++state->submitted;
        ++state->bucket(time).submitted;
        state->refreshQuiet(time);
        return ticket;
    }

    // For an unsuccessful WorkQueue post, not cancellation of an active task.
    bool cancel(Ticket ticket)
    {
        auto state = mState;
        const auto supplied = state->now();
        bool erased;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            const auto time = state->advance(supplied);
            erased = state->pending.erase(ticket) != 0;
            if (erased)
            {
                ++state->cancelled;
                ++state->bucket(time).cancelled;
                state->refreshQuiet(time);
            }
        }
        if (erased)
            state->changed.notify_all();
        return erased;
    }

    Permit acquire(Ticket ticket)
    {
        auto state = mState;
        std::unique_lock<std::mutex> lock(state->mutex);
        state->changed.wait(lock, [&]
        {
            // Registration precedes posting, and producers can post out of
            // registration order. Do not wait for the oldest ticket: it might
            // not be posted yet, while every worker has popped a later job.
            return state->pending.find(ticket) == state->pending.end() ||
                state->active < state->current;
        });
        const auto found = state->pending.find(ticket);
        if (found == state->pending.end())
            return {};
        const auto time = state->advance(state->now());
        const double wait = seconds(time - found->second);
        state->pending.erase(found);
        ++state->active;
        ++state->started;
        add(state->queue_wait, wait);
        auto& item = state->bucket(time);
        ++item.started;
        add(item.queue_wait, wait);
        state->refreshQuiet(time);
        lock.unlock();
        state->changed.notify_all();
        return Permit(std::move(state), time);
    }

    // Call from the owner's regular update with interval CPU pressure. Workers
    // block on notifications; they do not run policy polling loops.
    void tick(TimePoint supplied, const Pressure& pressure)
    {
        auto state = mState;
        std::unique_lock<std::mutex> lock(state->mutex);
        const auto time = state->advance(supplied);
        if (state->draining || !state->adaptive)
            return;

        const bool hard_cap = pressure.low_power || pressure.thermal_level >= 2;
        const unsigned ceiling = hard_cap ? state->limits.minimum
            : pressure.thermal_level == 1 ? state->limits.baseline : state->limits.maximum;
        const double interval = seconds(time - state->last_evaluation);
        // Thermal/power caps are immediate, including the first update.
        if (hard_cap)
        {
            state->change(ceiling, pressure.low_power ? "low-power" : "thermal-serious", time);
            state->recovery_not_before = time + std::chrono::seconds(2);
        }
        else if (state->current > ceiling)
        {
            state->change(ceiling, "thermal-fair", time);
            state->recovery_not_before = time + std::chrono::seconds(2);
        }
        if (interval < 0.5)
        {
            lock.unlock();
            state->changed.notify_all();
            return;
        }

        state->last_interval_occupancy = state->available_slot_s
            ? state->occupied_slot_s / state->available_slot_s : 0;
        state->last_interval_saturated_backlog_s = state->saturated_backlog_s;
        const bool cpu_busy = pressure.cpu_valid && std::isfinite(pressure.system_busy) &&
            std::isfinite(pressure.main_thread_busy) && pressure.system_busy >= 0.9 &&
            pressure.main_thread_busy >= 0.85;
        if (cpu_busy && !state->busy_pressure)
            state->busy_pressure_since = time;
        state->busy_pressure = cpu_busy;

        if (!hard_cap && cpu_busy)
        {
            state->recovery_not_before = time + std::chrono::seconds(2);
            if (seconds(time - state->busy_pressure_since) >= 1 &&
                seconds(time - state->last_cpu_reduction) >= 1)
            {
                state->change(std::max(state->limits.minimum, state->current - 1), "cpu-pressure", time);
                state->last_cpu_reduction = time;
            }
        }
        else if (!hard_cap && state->current < state->limits.baseline)
        {
            if (time >= state->recovery_not_before)
            {
                state->change(state->current + 1, "recovery", time);
                state->recovery_not_before = time + std::chrono::seconds(2);
            }
        }
        else if (!hard_cap && state->quiet && state->current > state->limits.baseline &&
                 seconds(time - state->quiet_since) >= 5 &&
                 seconds(time - state->last_quiet_reduction) >= 1)
        {
            state->change(state->current - 1, "quiet", time);
            state->last_quiet_reduction = time;
        }
        else if (!hard_cap && state->current < ceiling && time >= state->recovery_not_before &&
                 !state->pending.empty() && state->active >= state->current &&
                 seconds(time - state->pending.begin()->second) >= 0.05 &&
                 state->last_interval_occupancy >= 0.95 &&
                 state->saturated_backlog_s >= interval * 0.90)
        {
            // Integrate occupied slots and saturated backlog throughout the
            // interval. A momentary busy snapshot cannot trigger a ramp.
            state->change(state->current + 1, "backlog", time);
        }
        state->last_evaluation = time;
        state->occupied_slot_s = state->available_slot_s = state->saturated_backlog_s = 0;
        lock.unlock();
        state->changed.notify_all();
    }

    void beginDrain()
    {
        auto state = mState;
        TimePoint supplied;
        try { supplied = state->now(); }
        catch (...) { supplied = Clock::now(); }
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            const auto time = state->advance(supplied);
            state->draining = true;
            state->change(state->limits.maximum, "drain", time);
        }
        state->changed.notify_all();
    }

    Snapshot snapshot() const { return snapshot(mState->now()); }
    Snapshot snapshot(TimePoint supplied) const
    {
        auto state = mState;
        std::lock_guard<std::mutex> lock(state->mutex);
        const auto time = state->advance(supplied);
        Snapshot result;
        result.minimum = state->limits.minimum;
        result.baseline = state->limits.baseline;
        result.maximum = state->limits.maximum;
        result.current = state->current;
        result.adaptive = state->adaptive;
        result.draining = state->draining;
        result.waiting = state->pending.size();
        result.active = state->active;
        result.oldest_wait_s = state->pending.empty() ? 0 : seconds(time - state->pending.begin()->second);
        result.submitted = state->submitted;
        result.started = state->started;
        result.completed = state->completed;
        result.cancelled = state->cancelled;
        result.queue_wait = state->queue_wait;
        result.decode_wall = state->decode_wall;
        result.service_wall = state->service_wall;
        result.window = state->window(time);
        result.last_interval_occupancy = state->last_interval_occupancy;
        result.last_interval_saturated_backlog_s = state->last_interval_saturated_backlog_s;
        result.transition_reason = state->reason;
        result.last_transition = state->last_transition;
        return result;
    }

private:
    std::shared_ptr<State> mState;
};

#endif
