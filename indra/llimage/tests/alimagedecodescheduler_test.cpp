/**
 * @file alimagedecodescheduler_test.cpp
 * @brief Admission, feedback and lifecycle tests without viewer dependencies.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../alimagedecodescheduler.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <stdexcept>
#include <thread>

namespace
{
using Scheduler = ALImageDecodeScheduler;
using namespace std::chrono_literals;

struct ManualClock
{
    Scheduler::TimePoint value{};
    void at(double seconds)
    {
        value = Scheduler::TimePoint{} +
            std::chrono::duration_cast<Scheduler::Clock::duration>(std::chrono::duration<double>(seconds));
    }
    auto now() { return [this] { return value; }; }
};

Scheduler::Pressure cool()
{
    Scheduler::Pressure result;
    result.system_busy = 0.4;
    result.main_thread_busy = 0.4;
    result.cpu_valid = true;
    result.thermal_level = 0;
    return result;
}

std::vector<Scheduler::Ticket> submit(Scheduler& gate, unsigned count)
{
    std::vector<Scheduler::Ticket> result;
    for (unsigned i = 0; i < count; ++i)
        result.push_back(gate.submit());
    return result;
}
}

TEST_CASE("Decoder hardware limits use the highest performance tier", "[image][decode][scheduler]")
{
    struct Case
    {
        unsigned active;
        std::vector<unsigned> tiers;
        unsigned emulate;
        unsigned maximum;
        unsigned baseline;
        unsigned effective;
        bool valid;
    };
    const Case cases[] = {
        {10, {8, 2}, 0, 8, 4, 10, true},
        {8, {6, 2}, 0, 6, 3, 8, true},
        {8, {4, 4}, 0, 4, 2, 8, true},
        {6, {2, 4}, 0, 2, 2, 6, true}, // Largest tier is not necessarily fastest.
        {24, {12, 8, 4}, 0, 12, 6, 24, true},
        {100, {80, 20}, 0, 16, 8, 100, true},
        {8, {8}, 0, 6, 3, 8, true}, // No lower tier: total-minus-two bounds peak.
        {10, {7, 3}, 0, 7, 4, 10, true}, // Odd peak rounds the normal budget up.
        {8, {5, 3}, 0, 5, 3, 8, true},
        {4, {4}, 0, 2, 2, 4, true},
        {3, {3}, 0, 2, 2, 3, true},
        {2, {2}, 0, 2, 2, 2, true},
        {1, {1}, 0, 2, 2, 1, true},
        {10, {}, 0, 6, 4, 10, false},
        {10, {8, 1}, 0, 6, 4, 10, false},
        {10, {8, 0, 2}, 0, 6, 4, 10, false},
        {100, {}, 0, 16, 11, 100, false}, // Unknown topology retains old baseline.
        {10, {8, 2}, 8, 6, 3, 8, true},
        {10, {8, 2}, 6, 4, 2, 6, true},
        {10, {8, 2}, 4, 2, 2, 4, true},
        {10, {8, 2}, 1, 2, 2, 1, true}, // Historical two-worker floor exception.
        {10, {8, 2}, 100, 8, 4, 10, true},
        {10, {}, 6, 2, 2, 6, false},
        {0, {}, 0, 2, 2, 0, false},
    };
    for (const auto& test : cases)
    {
        INFO("active=" << test.active << " emulate=" << test.emulate);
        const auto limits = Scheduler::Limits::fromTopology(test.active, test.tiers, test.emulate);
        CHECK(limits.minimum == 2);
        CHECK(limits.maximum == test.maximum);
        CHECK(limits.baseline == test.baseline);
        CHECK(limits.effective_cores == test.effective);
        CHECK(limits.topology_valid == test.valid);
        if (limits.effective_cores >= 2)
            CHECK(limits.maximum <= limits.effective_cores);
    }
}

TEST_CASE("Decoder tickets count waiting work and failed posts", "[image][decode][scheduler]")
{
    ManualClock clock;
    Scheduler gate(Scheduler::Limits::fromTopology(10, {8, 2}), true, clock.now());
    const auto first = gate.submit();
    clock.at(0.125);
    const auto second = gate.submit();
    auto snapshot = gate.snapshot();
    CHECK(first != 0);
    CHECK(second > first);
    CHECK(snapshot.current == 4);
    CHECK(snapshot.waiting == 2);
    CHECK(snapshot.active == 0);
    CHECK(snapshot.oldest_wait_s == Catch::Approx(0.125));
    CHECK(gate.cancel(first));
    CHECK_FALSE(gate.cancel(first));
    CHECK_FALSE(gate.cancel(0));
    CHECK_FALSE(gate.acquire(first));
    CHECK_FALSE(gate.acquire(0));
    snapshot = gate.snapshot();
    CHECK(snapshot.waiting == 1);
    CHECK(snapshot.oldest_wait_s == 0);
    CHECK(snapshot.submitted == 2);
    CHECK(snapshot.cancelled == 1);
    CHECK(snapshot.completed == 0);
    CHECK(gate.cancel(second));
}

TEST_CASE("Decoder permits measure queue decode and callback lifetime", "[image][decode][scheduler]")
{
    ManualClock clock;
    Scheduler gate(Scheduler::Limits::fromTopology(10, {8, 2}), true, clock.now());
    const auto ticket = gate.submit();
    clock.at(0.1);
    auto permit = gate.acquire(ticket);
    REQUIRE(permit);
    CHECK_FALSE(gate.cancel(ticket));
    clock.at(0.3);
    permit.decoded();
    permit.decoded(); // Idempotent marker.
    CHECK(gate.snapshot().active == 1); // Callback still owns the admission.
    clock.at(0.5);
    auto moved = std::move(permit);
    CHECK_FALSE(permit);
    moved.reset();
    moved.reset();
    auto snapshot = gate.snapshot();
    CHECK(snapshot.active == 0);
    CHECK(snapshot.completed == 1);
    CHECK(snapshot.queue_wait.count == 1);
    CHECK(snapshot.queue_wait.total_s == Catch::Approx(0.1));
    CHECK(snapshot.decode_wall.count == 1);
    CHECK(snapshot.decode_wall.total_s == Catch::Approx(0.2));
    CHECK(snapshot.service_wall.count == 1);
    CHECK(snapshot.service_wall.total_s == Catch::Approx(0.4));
    CHECK(snapshot.service_wall.max_s == Catch::Approx(0.4));

    const auto failure = gate.submit();
    CHECK_THROWS_AS(([&]
    {
        auto guard = gate.acquire(failure);
        throw std::runtime_error("decoder or callback exception");
    }()), std::runtime_error);
    snapshot = gate.snapshot();
    CHECK(snapshot.active == 0);
    CHECK(snapshot.completed == 2);
    CHECK(snapshot.service_wall.count == 2);
    CHECK(snapshot.decode_wall.count == 1); // Failed path did not claim a decode marker.
}

TEST_CASE("Decoder burst ramp requires interval occupancy and pending age", "[image][decode][scheduler]")
{
    ManualClock clock;
    // Exercise six-slot policy mechanics independently of hardware selection.
    Scheduler gate(Scheduler::Limits{2, 4, 6, 10, 10, true}, true, clock.now());
    const auto tickets = submit(gate, 8);
    std::vector<Scheduler::Permit> active;
    for (unsigned i = 0; i < 4; ++i)
        active.push_back(gate.acquire(tickets[i]));
    clock.at(0.499);
    gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 4);
    clock.at(0.5);
    gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 5);
    CHECK(gate.snapshot().last_interval_occupancy == Catch::Approx(1));
    active.push_back(gate.acquire(tickets[4]));
    clock.at(1.0);
    gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 6);
    active.push_back(gate.acquire(tickets[5]));
    clock.at(1.5);
    gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 6);
    CHECK(gate.snapshot().transition_reason == "backlog");
    active.clear();
    CHECK(gate.cancel(tickets[6]));
    CHECK(gate.cancel(tickets[7]));
    clock.at(6.0);
    gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 6);
    clock.at(6.5);
    gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 5);
    clock.at(7.0);
    gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 5);
    clock.at(7.5);
    gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 4);
    CHECK(gate.snapshot().transition_reason == "quiet");
}

TEST_CASE("M1 Max burst reaches eight and drains without interrupting active decodes", "[image][decode][scheduler]")
{
    ManualClock clock;
    const auto limits = Scheduler::Limits::fromTopology(10, {8, 2});
    REQUIRE(limits.baseline == 4);
    REQUIRE(limits.maximum == 8);
    Scheduler gate(limits, true, clock.now());
    const auto tickets = submit(gate, 10);
    std::vector<Scheduler::Permit> active;
    for (unsigned i = 0; i < 4; ++i)
        active.push_back(gate.acquire(tickets[i]));
    for (unsigned limit = 5; limit <= 8; ++limit)
    {
        clock.at(0.5 * (limit - 4));
        gate.tick(clock.value, cool());
        // Fail before acquiring, so a broken ceiling cannot hang this test.
        REQUIRE(gate.snapshot().current == limit);
        CHECK(gate.snapshot().active == limit - 1);
        CHECK(gate.snapshot().last_interval_occupancy == Catch::Approx(1));
        active.push_back(gate.acquire(tickets[limit - 1]));
        CHECK(gate.snapshot().active == limit);
    }
    clock.at(2.5);
    gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 8);
    CHECK(gate.snapshot().active == 8);
    CHECK(gate.snapshot().waiting == 2);
    CHECK(gate.snapshot().completed == 0);

    auto hot = cool();
    hot.thermal_level = 2;
    clock.at(2.501);
    gate.tick(clock.value, hot);
    CHECK(gate.snapshot().current == 2);
    CHECK(gate.snapshot().active == 8);
    CHECK(gate.snapshot().completed == 0);
    CHECK(std::all_of(active.begin(), active.end(), [](const auto& permit) { return bool(permit); }));

    // Shutdown restores the hardware ceiling and accepts the already queued
    // requests even though no further main-loop policy ticks will occur.
    gate.beginDrain();
    CHECK(gate.snapshot().current == 8);
    CHECK(gate.snapshot().draining);
    CHECK(gate.submit() == 0);
    active.back().reset();
    auto ninth = gate.acquire(tickets[8]);
    REQUIRE(ninth);
    CHECK(gate.snapshot().active == 8);
    ninth.decoded();
    ninth.reset();
    active.clear();
    auto tenth = gate.acquire(tickets[9]);
    REQUIRE(tenth);
    tenth.decoded();
    tenth.reset();
    const auto snapshot = gate.snapshot();
    CHECK(snapshot.active == 0);
    CHECK(snapshot.waiting == 0);
    CHECK(snapshot.started == tickets.size());
    CHECK(snapshot.completed == tickets.size());
    CHECK(snapshot.cancelled == 0);
}

TEST_CASE("Decoder controller does not mistake an instant busy snapshot for sustained load", "[image][decode][scheduler]")
{
    ManualClock clock;
    Scheduler gate(Scheduler::Limits::fromTopology(10, {8, 2}), true, clock.now());
    const auto tickets = submit(gate, 8);
    clock.at(0.49);
    std::vector<Scheduler::Permit> active;
    for (unsigned i = 0; i < 4; ++i)
        active.push_back(gate.acquire(tickets[i]));
    clock.at(0.5);
    gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 4);
    CHECK(gate.snapshot().last_interval_occupancy == Catch::Approx(0.02));
    CHECK(gate.snapshot().last_interval_saturated_backlog_s == Catch::Approx(0.01));
    active.clear();
    for (unsigned i = 4; i < 8; ++i)
        CHECK(gate.cancel(tickets[i]));
}

TEST_CASE("Sustained CPU pressure backs off and recovery has a cooldown", "[image][decode][scheduler]")
{
    ManualClock clock;
    Scheduler gate(Scheduler::Limits::fromTopology(10, {8, 2}), true, clock.now());
    const auto tickets = submit(gate, 6);
    std::vector<Scheduler::Permit> active;
    for (unsigned i = 0; i < 4; ++i)
        active.push_back(gate.acquire(tickets[i]));
    auto pressure = cool();
    pressure.system_busy = 0.95;
    pressure.main_thread_busy = 0.90;
    clock.at(0.5); gate.tick(clock.value, pressure);
    CHECK(gate.snapshot().current == 4);
    clock.at(1.0); gate.tick(clock.value, pressure);
    CHECK(gate.snapshot().current == 4);
    clock.at(1.5); gate.tick(clock.value, pressure);
    CHECK(gate.snapshot().current == 3);
    CHECK(gate.snapshot().active == 4); // Downshift never cancels running work.
    clock.at(2.0); gate.tick(clock.value, pressure);
    CHECK(gate.snapshot().current == 3);
    clock.at(2.5); gate.tick(clock.value, pressure);
    CHECK(gate.snapshot().current == 2);
    clock.at(4.0); gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 2);
    clock.at(4.5); gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 3);
    clock.at(5.0); gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 3);
    clock.at(6.5); gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 4);
    active.clear();
    CHECK(gate.cancel(tickets[4]));
    CHECK(gate.cancel(tickets[5]));
}

TEST_CASE("Thermal and low power caps are immediate while fixed mode stays fixed", "[image][decode][scheduler]")
{
    ManualClock clock;
    const auto limits = Scheduler::Limits::fromTopology(10, {8, 2});
    Scheduler gate(limits, true, clock.now());
    auto pressure = cool();
    pressure.thermal_level = 2;
    gate.tick(clock.value, pressure);
    CHECK(gate.snapshot().current == 2);
    CHECK(gate.snapshot().transition_reason == "thermal-serious");
    pressure.thermal_level = -1;
    clock.at(1.5); gate.tick(clock.value, pressure);
    CHECK(gate.snapshot().current == 2);
    clock.at(2.0); gate.tick(clock.value, pressure);
    CHECK(gate.snapshot().current == 3);
    pressure.low_power = true;
    clock.at(2.001); gate.tick(clock.value, pressure);
    CHECK(gate.snapshot().current == 2);
    CHECK(gate.snapshot().transition_reason == "low-power");

    Scheduler fixed(limits, false, clock.now());
    REQUIRE(fixed.snapshot().current == 8);
    const auto fixed_tickets = submit(fixed, 8);
    std::vector<Scheduler::Permit> fixed_active;
    for (auto ticket : fixed_tickets)
        fixed_active.push_back(fixed.acquire(ticket));
    CHECK(fixed.snapshot().active == 8);
    CHECK(fixed.snapshot().waiting == 0);
    clock.at(20);
    fixed.tick(clock.value, pressure);
    CHECK(fixed.snapshot().current == 8);
    CHECK(fixed.snapshot().active == 8);
    CHECK_FALSE(fixed.snapshot().adaptive);
    CHECK(fixed.snapshot().transition_reason == "fixed");
}

TEST_CASE("CPU backoff requires valid sustained system and main pressure", "[image][decode][scheduler]")
{
    ManualClock clock;
    Scheduler gate(Scheduler::Limits::fromTopology(10, {8, 2}), true, clock.now());
    auto pressure = cool();
    pressure.system_busy = 1;
    for (double time : {0.5, 1.0, 1.5, 2.0})
    {
        clock.at(time); gate.tick(clock.value, pressure);
        CHECK(gate.snapshot().current == 4); // Other processes busy, main not busy.
    }
    pressure.main_thread_busy = 1;
    pressure.cpu_valid = false;
    clock.at(4); gate.tick(clock.value, pressure);
    CHECK(gate.snapshot().current == 4);
    pressure.cpu_valid = true;
    pressure.system_busy = std::numeric_limits<double>::quiet_NaN();
    clock.at(6); gate.tick(clock.value, pressure);
    CHECK(gate.snapshot().current == 4);
    pressure.system_busy = 1;
    clock.at(6.5); gate.tick(clock.value, pressure);
    clock.at(7.0); gate.tick(clock.value, cool()); // Brief pressure did not persist.
    CHECK(gate.snapshot().current == 4);

    const auto tickets = submit(gate, 8);
    std::vector<Scheduler::Permit> active;
    for (unsigned i = 0; i < 4; ++i)
        active.push_back(gate.acquire(tickets[i]));
    clock.at(7.5); gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 4); // Pressure recovery holds boosts for 2s.
    clock.at(8.5); gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 5);
    active.push_back(gate.acquire(tickets[4]));
    clock.at(9.0); gate.tick(clock.value, cool());
    CHECK(gate.snapshot().current == 6);
    auto fair = cool(); fair.thermal_level = 1;
    clock.at(9.001); gate.tick(clock.value, fair);
    CHECK(gate.snapshot().current == 4);
    CHECK(gate.snapshot().active == 5);
    CHECK(gate.snapshot().transition_reason == "thermal-fair");
    active.clear();
    for (unsigned i = 5; i < tickets.size(); ++i)
        CHECK(gate.cancel(tickets[i]));
}

TEST_CASE("A cancelled waiting admission wakes without cancelling active permits", "[image][decode][scheduler]")
{
    Scheduler gate(Scheduler::Limits::fromTopology(4, {4}));
    const auto tickets = submit(gate, 3);
    auto first = gate.acquire(tickets[0]);
    auto second = gate.acquire(tickets[1]);
    std::mutex mutex;
    std::condition_variable changed;
    bool done = false;
    bool acquired = true;
    std::thread waiter([&]
    {
        auto permit = gate.acquire(tickets[2]);
        {
            std::lock_guard<std::mutex> lock(mutex);
            acquired = bool(permit);
            done = true;
        }
        changed.notify_all();
    });
    CHECK(gate.cancel(tickets[2]));
    bool woke;
    {
        std::unique_lock<std::mutex> lock(mutex);
        woke = changed.wait_for(lock, 1s, [&] { return done; });
    }
    CHECK(gate.snapshot().active == 2);
    CHECK(gate.snapshot().completed == 0);
    // Move assignment releases its old admission exactly once.
    first = std::move(second);
    CHECK_FALSE(second);
    CHECK(gate.snapshot().active == 1);
    first.reset(); // Also unblocks cleanup if a broken cancel omitted notification.
    waiter.join();
    CHECK(woke);
    CHECK_FALSE(acquired);
    CHECK(gate.snapshot().active == 0);
    CHECK(gate.snapshot().completed == 2);
    CHECK(gate.snapshot().cancelled == 1);
}

TEST_CASE("Decoder telemetry expires windows without dropping lifetime totals", "[image][decode][scheduler]")
{
    ManualClock clock;
    Scheduler gate(Scheduler::Limits::fromTopology(10, {8, 2}), true, clock.now());
    const auto cancelled = gate.submit();
    CHECK(gate.cancel(cancelled));
    const auto job = gate.submit();
    auto permit = gate.acquire(job);
    clock.at(0.2); permit.decoded();
    clock.at(0.3); permit.reset();
    const auto recent = gate.snapshot();
    CHECK(recent.window.submitted == 2);
    CHECK(recent.window.cancelled == 1);
    CHECK(recent.window.started == 1);
    CHECK(recent.window.completed == 1);
    CHECK(recent.window.decode_wall.total_s == Catch::Approx(0.2));
    CHECK(recent.window.service_wall.total_s == Catch::Approx(0.3));
    clock.at(6.0);
    const auto expired = gate.snapshot();
    CHECK(expired.submitted == 2);
    CHECK(expired.completed == 1);
    CHECK(expired.window.submitted == 0);
    CHECK(expired.window.completed == 0);
    CHECK(expired.window.service_wall.count == 0);
    CHECK(expired.window.duration_s <= 5);
}

TEST_CASE("Out of order posts cannot be blocked behind an unposted older ticket", "[image][decode][scheduler]")
{
    Scheduler gate(Scheduler::Limits::fromTopology(4, {4}));
    const auto parked = gate.submit(); // Producer pauses before its WorkQueue post.
    const auto later = submit(gate, 4);
    // A gate ordered by registration would block here and could occupy every
    // pool worker before the older job can be popped from the WorkQueue.
    std::mutex mutex;
    std::condition_variable changed;
    bool admitted = false;
    std::thread worker([&]
    {
        auto first = gate.acquire(later[0]);
        auto second = gate.acquire(later[1]);
        {
            std::lock_guard<std::mutex> lock(mutex);
            admitted = bool(first) && bool(second);
        }
        changed.notify_all();
    });
    bool made_progress;
    {
        std::unique_lock<std::mutex> lock(mutex);
        made_progress = changed.wait_for(lock, 1s, [&] { return admitted; });
    }
    // Always unblock a broken registration-order implementation before join,
    // so this regression reports a failure rather than hanging the test runner.
    CHECK(gate.cancel(parked));
    worker.join();
    CHECK(made_progress);
    gate.beginDrain();
    CHECK(gate.submit() == 0);
    for (unsigned i = 2; i < later.size(); ++i)
    {
        auto permit = gate.acquire(later[i]);
        CHECK(bool(permit));
    }
    const auto snapshot = gate.snapshot();
    CHECK(snapshot.draining);
    CHECK(snapshot.active == 0);
    CHECK(snapshot.waiting == 0);
    CHECK(snapshot.completed == 4);
}

TEST_CASE("Permit shared state survives destruction of its scheduler owner", "[image][decode][scheduler]")
{
    auto gate = std::make_shared<Scheduler>(Scheduler::Limits::fromTopology(10, {8, 2}));
    const auto ticket = gate->submit();
    auto permit = gate->acquire(ticket);
    gate.reset();
    CHECK(bool(permit));
    permit.decoded();
    permit.reset();
    CHECK_FALSE(permit);
}

TEST_CASE("Real decoder contenders obey admission downshifts and drain accepted work", "[image][decode][scheduler]")
{
    std::atomic<long long> clock_us{0};
    Scheduler gate(Scheduler::Limits{2, 2, 4, 8, 8, true}, true, [&]
    {
        return Scheduler::TimePoint{} + std::chrono::microseconds(clock_us.load());
    });
    const auto tickets = submit(gate, 12);
    std::mutex mutex;
    std::condition_variable changed;
    unsigned entered = 0, running = 0, maximum_running = 0;
    bool release = false;
    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    for (auto ticket : tickets)
    {
        threads.emplace_back([&, ticket]
        {
            try
            {
                auto permit = gate.acquire(ticket);
                if (!permit)
                {
                    failed = true;
                    return;
                }
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    ++entered;
                    maximum_running = std::max(maximum_running, ++running);
                    changed.notify_all();
                    changed.wait(lock, [&] { return release; });
                    --running;
                }
                permit.decoded();
            }
            catch (...) { failed = true; }
        });
    }
    auto waitEntered = [&](unsigned count)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, 2s, [&] { return entered >= count; });
    };
    // Use nonfatal assertions until every std::thread has been joined.
    CHECK(waitEntered(2));
    CHECK(gate.snapshot().active == 2);
    CHECK(gate.snapshot().waiting == 10);
    clock_us = 500000; gate.tick(Scheduler::TimePoint{} + 500ms, cool());
    CHECK(waitEntered(3));
    clock_us = 1000000; gate.tick(Scheduler::TimePoint{} + 1s, cool());
    CHECK(waitEntered(4));
    auto hot = cool(); hot.thermal_level = 3;
    clock_us = 1001000; gate.tick(Scheduler::TimePoint{} + 1001ms, hot);
    CHECK(gate.snapshot().current == 2);
    CHECK(gate.snapshot().active == 4);
    {
        std::unique_lock<std::mutex> lock(mutex);
        CHECK_FALSE(changed.wait_for(lock, 20ms, [&] { return entered > 4; }));
    }
    gate.beginDrain();
    CHECK(gate.snapshot().current == 4);
    CHECK(gate.submit() == 0);
    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
    }
    changed.notify_all();
    for (auto& thread : threads)
        thread.join();
    const auto snapshot = gate.snapshot();
    CHECK_FALSE(failed.load());
    CHECK(entered == tickets.size());
    CHECK(maximum_running == 4);
    CHECK(snapshot.active == 0);
    CHECK(snapshot.waiting == 0);
    CHECK(snapshot.completed == tickets.size());
    CHECK(snapshot.decode_wall.count == tickets.size());
    CHECK(snapshot.service_wall.count == tickets.size());
}
