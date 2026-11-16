/**
 * @file alnativeframeplan_completion_test.mm
 * @brief Actual presentation-command retirement census without a window.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "../alnativeframeplan.h"
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <tuple>

namespace
{
// A real GPU dependency keeps submitted commands pending without timing
// assumptions. Release on assertion unwinding before scheduler destruction.
struct CompletionGate
{
    id<MTLSharedEvent> event;
    explicit CompletionGate(const ALMetalDevice& device)
    {
        event = [(__bridge id<MTLDevice>)device.nativeHandle() newSharedEvent];
        REQUIRE(event);
        auto command = [(__bridge id<MTLCommandQueue>)device.nativeCommandQueue() commandBuffer];
        REQUIRE(command);
        [command encodeWaitForEvent:event value:1];
        [command commit];
    }
    void release() noexcept { event.signaledValue = 1; }
    ~CompletionGate() { release(); }
};
}

TEST_CASE("Native main completion census observes real bounded scheduler receipts without admission",
          "[native-frame-plan][main-completion-census]")
{
    @autoreleasepool
    {
        using namespace ALNativeFramePlan;
        std::string error;
        auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        auto scheduler = ALMetalFrameScheduler::create(*device, 3, &error); INFO(error); REQUIRE(scheduler);
        auto& budget = ALNativeFrameBudget::instance();
        const auto budget_before = budget.snapshot();
        const auto initial = mainCompletionCensus();
        const bool timing_before = ALMetalDevice::commandTimingEnabled();

        // A reserved, unsubmitted slot is not a submitted in-flight command.
        auto reserved = scheduler->beginFrame("Reserved main receipt", 0, &error);
        INFO(error); REQUIRE(reserved);
        const auto commands_before_observation = ALMetalDevice::committedCommandBufferCount();
        observeMainCompletion(scheduler.get(), MainCompletionPhase::frame_begin);
        auto begin = mainCompletionCensus()[0];
        CHECK(begin.samples == initial[0].samples + 1);
        CHECK(begin.available);
        CHECK(begin.latest.begun == 1);
        CHECK(begin.latest.submitted == 0);
        CHECK(begin.latest.in_flight == 0);
        CHECK(begin.latest.maximum_in_flight == 3);
        CHECK(begin.full_samples == initial[0].full_samples);
        CHECK(begin.last_submitted_serial == 0);
        CHECK(begin.last_submission_retired); // Known scheduler's empty timeline.
        CHECK(ALMetalDevice::committedCommandBufferCount() == commands_before_observation);
        reserved.reset();

        CompletionGate gate(*device);
        std::array<std::unique_ptr<ALMetalFrame>, 3> pending;
        for (auto& frame : pending)
        {
            frame = scheduler->beginFrame("Gated main completion receipt", 0, &error);
            INFO(error); REQUIRE(frame);
            REQUIRE(scheduler->submit(*frame, &error));
        }
        const auto submitted = ALMetalDevice::committedCommandBufferCount();
        observeMainCompletion(scheduler.get(), MainCompletionPhase::before_ordinary_probes);
        auto probes = mainCompletionCensus()[1];
        CHECK(probes.samples == initial[1].samples + 1);
        CHECK(probes.available);
        CHECK(probes.latest.submitted == 3);
        CHECK(probes.latest.completed == 0);
        CHECK(probes.latest.failed == 0);
        CHECK(probes.latest.in_flight == 3);
        CHECK(probes.latest.maximum_in_flight == 3);
        CHECK(probes.full_samples == initial[1].full_samples + 1);
        CHECK(probes.peak_in_flight == std::max(initial[1].peak_in_flight, 3u));
        CHECK(probes.last_submitted_serial == scheduler->lastSubmittedSerial());
        CHECK_FALSE(probes.last_submission_retired);
        CHECK(mainCompletionCensus()[0].samples == begin.samples); // Phase buckets are independent.
        CHECK(ALMetalDevice::committedCommandBufferCount() == submitted);

        gate.release();
        REQUIRE(scheduler->waitUntilIdle(5000, &error)); INFO(error);
        observeMainCompletion(scheduler.get(), MainCompletionPhase::before_ordinary_probes);
        const auto retired = mainCompletionCensus()[1];
        CHECK(retired.samples == probes.samples + 1);
        CHECK(retired.latest.in_flight == 0);
        CHECK(retired.latest.completed == 3);
        CHECK(retired.latest.failed == 0);
        CHECK(retired.last_submitted_serial == probes.last_submitted_serial);
        CHECK(retired.last_submission_retired);
        CHECK(retired.full_samples == probes.full_samples);
        CHECK(retired.peak_in_flight == probes.peak_in_flight);

        observeMainCompletion(nullptr, MainCompletionPhase::before_ordinary_probes);
        const auto unknown = mainCompletionCensus()[1];
        CHECK(unknown.samples == retired.samples + 1);
        CHECK(unknown.unavailable_samples == initial[1].unavailable_samples + 1);
        CHECK_FALSE(unknown.available);
        CHECK_FALSE(unknown.last_submission_retired); // Unknown is not the known zero-serial receipt.
        CHECK(unknown.last_submitted_serial == 0);
        CHECK(unknown.latest.begun == 0);
        CHECK(unknown.latest.submitted == 0);
        CHECK(unknown.latest.completed == 0);
        CHECK(unknown.latest.failed == 0);
        CHECK(unknown.latest.in_flight == 0);
        CHECK(unknown.latest.maximum_in_flight == 0);
        CHECK(unknown.full_samples == retired.full_samples);
        CHECK(unknown.peak_in_flight == retired.peak_in_flight);
        CHECK(ALMetalDevice::committedCommandBufferCount() == submitted);
        CHECK(ALMetalDevice::commandTimingEnabled() == timing_before);

        // Observation must not begin a frame, mutate pressure/backlog, consume
        // a common admission credit or retire an unrelated active owner.
        const auto after = budget.snapshot();
        CHECK(std::tie(after.frame, after.initialized, after.draining, after.main_busy,
            after.memory_pressure, after.admitted, after.deferred, after.retired,
            after.uploads_in_flight, after.upload_bytes_in_flight,
            after.scene_publications_in_flight, after.scene_bytes_in_flight,
            after.preparation_in_flight, after.preparation_bytes_in_flight,
            after.optional_views_in_flight, after.maintenance_in_flight,
            after.maintenance_bytes_in_flight) ==
            std::tie(budget_before.frame, budget_before.initialized, budget_before.draining,
            budget_before.main_busy, budget_before.memory_pressure, budget_before.admitted,
            budget_before.deferred, budget_before.retired, budget_before.uploads_in_flight,
            budget_before.upload_bytes_in_flight, budget_before.scene_publications_in_flight,
            budget_before.scene_bytes_in_flight, budget_before.preparation_in_flight,
            budget_before.preparation_bytes_in_flight, budget_before.optional_views_in_flight,
            budget_before.maintenance_in_flight, budget_before.maintenance_bytes_in_flight));
        CHECK(std::tie(after.streaming.queued_requests, after.streaming.queued_input_bytes,
            after.streaming.oldest_request_frame, after.streaming.pending_publications,
            after.streaming.pressure_requests, after.uploads_this_frame,
            after.scene_publications_this_frame, after.maintenance_this_frame,
            after.optional_views_this_frame, after.upload_bytes_this_frame,
            after.scene_bytes_this_frame, after.maintenance_bytes_this_frame,
            after.progress_admissions) ==
            std::tie(budget_before.streaming.queued_requests, budget_before.streaming.queued_input_bytes,
            budget_before.streaming.oldest_request_frame, budget_before.streaming.pending_publications,
            budget_before.streaming.pressure_requests, budget_before.uploads_this_frame,
            budget_before.scene_publications_this_frame, budget_before.maintenance_this_frame,
            budget_before.optional_views_this_frame, budget_before.upload_bytes_this_frame,
            budget_before.scene_bytes_this_frame, budget_before.maintenance_bytes_this_frame,
            budget_before.progress_admissions));
    }
}
