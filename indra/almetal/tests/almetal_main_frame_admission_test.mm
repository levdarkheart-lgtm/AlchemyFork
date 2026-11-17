/**
 * @file almetal_main_frame_admission_test.mm
 * @brief Native ordinary-main graph admission and completion ownership.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalworld.h"
#include "../almetalpipeline.h"
#include "../../llcommon/alnativeframebudget.h"
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <dispatch/dispatch.h>
#include <array>
#include <atomic>
#include <chrono>
#include <limits>
#include <thread>

namespace
{
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device = ALMetalDevice::create(&error);
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::shared_ptr<ALMetalWorldFrameData> data;
    Fixture()
    {
        INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error); REQUIRE(renderer);
        data = ALMetalWorldFrameData::create(*device, &error); REQUIRE(data);
        renderer->setFrameData(data); data->beginFrame(1);
    }
    ~Fixture() { if (device) device->waitUntilIdle(); }
    std::unique_ptr<ALMetalRenderTarget> clear(unsigned channel,
        std::shared_ptr<const void> owner = {})
    {
        ALMetalWorldGBufferScene scene;
        scene.width = scene.height = 4;
        scene.hdr = scene.normal_hdr = false;
        std::fill_n(scene.clear_color, 4, 0.f);
        scene.clear_color[channel] = scene.clear_color[3] = 1.f;
        scene.source_owner = std::move(owner);
        auto target = renderer->render(scene, &error);
        INFO(error); REQUIRE(target); return target;
    }
    void checkColor(const ALMetalRenderTarget& target, unsigned channel)
    { checkColor(*target.colorTexture(0), channel); }
    void checkColor(const ALMetalTexture& texture, unsigned channel)
    {
        std::array<std::uint8_t, 64> bytes{};
        REQUIRE(texture.readback(*device, 0, 0, {}, bytes.data(), 16, &error));
        for (unsigned i = 0; i < bytes.size(); ++i)
            CHECK(bytes[i] == ((i % 4 == channel || i % 4 == 3) ? 255 : 0));
    }
};

// A broken same-frame wait is released by a watchdog and reported as a test
// failure. Cleanup always releases before the renderer/device can wait idle.
struct Gate
{
    id<MTLSharedEvent> event;
    std::shared_ptr<std::atomic<unsigned>> state = std::make_shared<std::atomic<unsigned>>(0);
    explicit Gate(Fixture& f)
    {
        event = [(__bridge id<MTLDevice>)f.device->nativeHandle() newSharedEvent]; REQUIRE(event);
        auto command = f.device->makeCommandBuffer("Main admission queue gate", &f.error); REQUIRE(command);
        [(__bridge id<MTLCommandBuffer>)command->nativeHandle() encodeWaitForEvent:event value:1];
        REQUIRE(command->commit(&f.error));
        const auto observed = state;
        id<MTLSharedEvent> retained = event;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC),
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                unsigned expected = 0;
                if (observed->compare_exchange_strong(expected, 2)) retained.signaledValue = 1;
            });
    }
    void release() { unsigned expected = 0; state->compare_exchange_strong(expected, 1); event.signaledValue = 1; }
    bool watchdogTripped() const { return state->load() == 2; }
    ~Gate() { release(); }
};

struct BudgetObservation
{
    ALNativeFrameBudget& budget = ALNativeFrameBudget::instance();
    ALNativeFrameBudget::Frame frame = 0;
    BudgetObservation()
    {
        const auto previous = budget.snapshot();
        REQUIRE(previous.frame < std::numeric_limits<ALNativeFrameBudget::Frame>::max() - 1);
        REQUIRE_FALSE(previous.draining);
        frame = previous.frame + 1;
        ALNativeFrameBudget::FrameInputs inputs;
        inputs.frame = frame;
        inputs.frame_interval_ns = 64000000;
        inputs.required_main_cpu_ns = 14000000;
        budget.beginFrame(inputs);
        REQUIRE_FALSE(budget.snapshot().main_busy);
    }
    ~BudgetObservation()
    {
        budget.closeMainObservations(frame);
        // Leave the shared native-test singleton without synthetic pressure.
        // Outstanding resource credits still retire through their own leases.
        ALNativeFrameBudget::FrameInputs neutral;
        neutral.frame = frame + 1;
        budget.beginFrame(neutral);
        budget.closeMainObservations(neutral.frame);
    }
};
}

TEST_CASE("almetal: main admission preserves three early commits and waits only older frame groups",
          "[almetal][main-frame-admission]")
{
    @autoreleasepool
    {
        Fixture f;
        BudgetObservation observation;
        Gate gate(f);
        std::vector<std::unique_ptr<ALMetalRenderTarget>> targets;
        std::vector<std::shared_ptr<const ALMetalCommandCompletion>> completions;
        auto source = std::make_shared<int>(7);
        std::weak_ptr<int> retained = source;
        const auto before = ALMetalDevice::committedCommandBufferCount();
        for (unsigned group = 1; group <= 2; ++group)
        {
            f.data->beginFrame(group);
            for (unsigned stage = 0; stage < 3; ++stage)
            {
                REQUIRE(f.renderer->beginMainFrameBatch(group, "Main graph early transaction", &f.error));
                targets.push_back(f.clear(stage, source));
                completions.push_back(f.renderer->commandBatchCompletion());
                REQUIRE(f.renderer->commitCommandBatch(&f.error));
            }
        }
        REQUIRE_FALSE(gate.watchdogTripped());
        CHECK(ALMetalDevice::committedCommandBufferCount() == before + 6);
        source.reset();
        CHECK_FALSE(retained.expired());
        for (const auto& completion : completions) { REQUIRE(completion); CHECK_FALSE(completion->isComplete()); }
        auto stats = f.renderer->mainFrameStats();
        CHECK(stats.pending_groups == 2); CHECK(stats.pending_commands == 6);
        CHECK(stats.older_group_waits == 0); CHECK(stats.reservations == 0);
        CHECK(observation.budget.snapshot().required_main_gpu_wait_observations == 0);
        CHECK_FALSE(f.renderer->beginMainFrameBatch(2, "Fourth main stage denied", &f.error));
        CHECK_FALSE(f.renderer->beginMainFrameBatch(1, "Stale main group denied", &f.error));
        CHECK(f.renderer->mainFrameStats().width_denials == 1);

        // The third group must wait an older real GPU command. This release
        // is independent of the owner blocked inside beginMainFrameBatch.
        auto released = std::make_shared<std::atomic<bool>>(false);
        const auto observed = released;
        id<MTLSharedEvent> event = gate.event;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 50 * NSEC_PER_MSEC),
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                observed->store(true); event.signaledValue = 1;
            });
        REQUIRE(f.renderer->beginMainFrameBatch(3, "Third main group", &f.error));
        CHECK(released->load());
        CHECK(f.renderer->mainFrameStats().older_group_waits == 1);
        CHECK(f.renderer->mainFrameStats().wait_nanoseconds > 0);
        const auto pressure = observation.budget.snapshot();
        CHECK(pressure.required_main_gpu_wait_observations == 1);
        CHECK(pressure.required_main_gpu_wait_unknown_observations == 0);
        CHECK(pressure.required_main_gpu_wait_wall_ns > 0);
        CHECK(pressure.required_main_gpu_wait_non_cpu_ns > 0);
        CHECK(pressure.required_main_pressure_ns ==
            pressure.required_main_cpu_ns + pressure.required_main_gpu_wait_non_cpu_ns);
        CHECK(pressure.main_busy);
        f.renderer->cancelCommandBatch();
        gate.release(); REQUIRE(f.device->waitUntilIdle(&f.error));
        for (unsigned i = 0; i < targets.size(); ++i) f.checkColor(*targets[i], i % 3);
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!retained.expired() && std::chrono::steady_clock::now() < end) std::this_thread::yield();
        CHECK(retained.expired());
        CHECK(f.renderer->mainFrameStats().peak_pending_groups == 2);
        CHECK(f.renderer->mainFrameStats().peak_pending_commands == 6);
    }
}

TEST_CASE("almetal: canceled invalid and reentrant main batches settle their exact reservation",
          "[almetal][main-frame-admission]")
{
    @autoreleasepool
    {
        Fixture f;
        REQUIRE(f.renderer->beginMainFrameBatch(1, "Canceled main stage", &f.error));
        auto abandoned = f.renderer->commandBatchCompletion(); REQUIRE(abandoned);
        bool cancellation = false;
        REQUIRE(f.renderer->observeCommandBatchSettlement([&](bool committed) { cancellation = !committed; }, &f.error));
        f.renderer->cancelCommandBatch();
        CHECK(cancellation); CHECK(abandoned->isComplete());
        CHECK(f.renderer->mainFrameStats().reservations == 0);

        REQUIRE(f.renderer->beginMainFrameBatch(1, "Invalid main stage", &f.error));
        ALMetalWorldGBufferScene invalid; invalid.width = 0;
        CHECK_FALSE(f.renderer->render(invalid, &f.error));
        CHECK_FALSE(f.renderer->commitCommandBatch(&f.error));
        CHECK_FALSE(f.renderer->commandBatchActive());
        CHECK(f.renderer->mainFrameStats().reservations == 0);
        CHECK(f.renderer->mainFrameStats().committed == 0);

        bool reentered = false;
        REQUIRE(f.renderer->beginMainFrameBatch(1, "Main settlement origin", &f.error));
        auto first = f.clear(0);
        REQUIRE(f.renderer->observeCommandBatchSettlement([&](bool committed) {
            if (committed) reentered = f.renderer->beginMainFrameBatch(1, "Main settlement next stage", &f.error);
        }, &f.error));
        REQUIRE(f.renderer->commitCommandBatch(&f.error));
        CHECK(reentered); REQUIRE(f.renderer->commandBatchActive());
        auto second = f.clear(1); REQUIRE(f.renderer->commitCommandBatch(&f.error));
        REQUIRE(f.renderer->beginMainFrameBatch(1, "Main settlement final stage", &f.error));
        auto third = f.clear(2); REQUIRE(f.renderer->commitCommandBatch(&f.error));
        CHECK_FALSE(f.renderer->beginMainFrameBatch(1, "Completed stages still count", &f.error));
        REQUIRE(f.device->waitUntilIdle(&f.error));
        f.checkColor(*first, 0); f.checkColor(*second, 1); f.checkColor(*third, 2);
        CHECK(f.renderer->mainFrameStats().committed == 3);
        CHECK(f.renderer->mainFrameStats().canceled == 2);
        CHECK(f.renderer->mainFrameStats().reservations == 0);
    }
}

TEST_CASE("almetal: main authority survives shader reload and retires through legacy utility admission",
          "[almetal][main-frame-admission]")
{
    @autoreleasepool
    {
        Fixture f;
        BudgetObservation observation;
        Gate gate(f);
        for (unsigned i = 0; i < 3; ++i)
        {
            REQUIRE(f.renderer->beginMainFrameBatch(1, "Main before reload", &f.error));
            auto target = f.clear(i); REQUIRE(f.renderer->commitCommandBatch(&f.error));
        }
        REQUIRE_FALSE(gate.watchdogTripped());
        // Shader reload eagerly uploads fallback/look-up textures and waits
        // on this same queue. Release the artificial GPU gate before reload;
        // the old main admission receipts still need to transfer and retire.
        gate.release();
        const std::array owners{f.renderer.get()};
        REQUIRE(ALMetalWorldGBufferRenderer::reloadSharedShaders(*f.data, owners, &f.error));
        CHECK(f.renderer->mainFrameStats().pending_commands == 3);
        Gate utility_gate(f);
        REQUIRE(f.renderer->beginCommandBatch("Legacy utility retains one-command cap", &f.error, 1));
        auto utility = f.clear(1);
        auto utility_completion = f.renderer->commandBatchCompletion(); REQUIRE(utility_completion);
        REQUIRE(f.renderer->commitCommandBatch(&f.error));
        CHECK_FALSE(utility_completion->isComplete());
        CHECK(f.renderer->mainFrameStats().pending_commands == 0);
        CHECK(f.renderer->mainFrameStats().completed == 3);
        id<MTLSharedEvent> utility_event = utility_gate.event;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 50 * NSEC_PER_MSEC),
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                utility_event.signaledValue = 1;
            });
        REQUIRE(f.renderer->beginMainFrameBatch(2, "Main after utility", &f.error));
        CHECK(utility_completion->isComplete());
        CHECK(observation.budget.snapshot().required_main_gpu_wait_observations == 0);
        CHECK_FALSE(observation.budget.snapshot().main_busy);
        utility_gate.release();
        auto main = f.clear(2); REQUIRE(f.renderer->commitCommandBatch(&f.error));
        REQUIRE(f.renderer->beginShadowFrameBatch(1, "Independent shadow authority", &f.error));
        f.renderer->cancelCommandBatch();
        CHECK(f.renderer->mainFrameStats().committed == 4);
        CHECK(f.renderer->mainFrameStats().reservations == 0);
        REQUIRE(f.device->waitUntilIdle(&f.error));
        f.checkColor(*utility, 1); f.checkColor(*main, 2);
    }
}

TEST_CASE("almetal: queued pooled main targets preserve earlier readers across frame groups",
          "[almetal][main-frame-admission]")
{
    @autoreleasepool
    {
        Fixture f;
        Gate gate(f);
        std::vector<std::unique_ptr<ALMetalTexture>> snapshots;
        const ALMetalTexture* reused = nullptr;
        for (unsigned group = 1; group <= 2; ++group)
        {
            f.data->beginFrame(group);
            for (unsigned stage = 0; stage < 3; ++stage)
            {
                REQUIRE(f.renderer->beginMainFrameBatch(group, "Pooled main writer", &f.error));
                auto target = f.clear(stage);
                if (!reused) reused = target->colorTexture(0);
                CHECK(target->colorTexture(0) == reused);
                REQUIRE(f.renderer->commitCommandBatch(&f.error));
                auto descriptor = target->colorTexture(0)->descriptor();
                auto snapshot = ALMetalTexture::create(*f.device, descriptor, &f.error); REQUIRE(snapshot);
                auto copy = f.device->makeCommandBuffer("Earlier pooled main reader", &f.error); REQUIRE(copy);
                REQUIRE(snapshot->copyFrom(*copy, *target->colorTexture(0), {}, &f.error));
                REQUIRE(copy->commit(&f.error));
                snapshots.push_back(std::move(snapshot));
                // The next writer reuses this pooled texture while every
                // previous writer and reader is still blocked on the GPU.
                target.reset();
            }
        }
        REQUIRE_FALSE(gate.watchdogTripped());
        CHECK(f.renderer->mainFrameStats().pending_groups == 2);
        CHECK(f.renderer->mainFrameStats().pending_commands == 6);
        gate.release(); REQUIRE(f.device->waitUntilIdle(&f.error));
        for (unsigned i = 0; i < snapshots.size(); ++i) f.checkColor(*snapshots[i], i % 3);
    }
}
