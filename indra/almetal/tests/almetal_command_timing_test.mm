/**
 * @file almetal_command_timing_test.mm
 * @brief Native command GPU intervals without blocking or wrapper ownership.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "../almetaldevice.h"
#include "../almetalresource.h"
#include "../almetalworld.h"

#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <dispatch/dispatch.h>

#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>

namespace
{
// A broken blocking poll/destructor must fail this test, not strand the GPU.
struct ReleaseGate
{
    id<MTLSharedEvent> event;
    std::shared_ptr<std::atomic<int>> phase =
        std::make_shared<std::atomic<int>>(0); // active, disarmed, tripped

    explicit ReleaseGate(id<MTLSharedEvent> value) : event(value)
    {
        const auto state = phase;
        id<MTLSharedEvent> retained_event = event;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC),
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                int expected = 0;
                if (state->compare_exchange_strong(expected, 2))
                    retained_event.signaledValue = 1;
            });
    }
    void release()
    {
        int expected = 0;
        phase->compare_exchange_strong(expected, 1);
        event.signaledValue = 1;
    }
    bool tripped() const { return phase->load() == 2; }
    ~ReleaseGate() { release(); }
};

void checkCleared(const ALMetalCommandGPUTime& value)
{
    CHECK(value.start_seconds == 0.0);
    CHECK(value.end_seconds == 0.0);
    CHECK(value.elapsed_nanoseconds == 0);
}
}

TEST_CASE("almetal: command GPU timing fails closed on abandonment",
          "[almetal][completion][command-timing]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error); REQUIRE(device);
    const auto before = ALMetalDevice::committedCommandBufferCount();
    auto command = device->makeCommandBuffer("Unsubmitted GPU timing", &error);
    REQUIRE(command);
    auto ticket = command->completionToken(); REQUIRE(ticket);
    CHECK(ticket == command->completionToken());
    ALMetalCommandGPUTime value{1.0, 2.0, 3};
    CHECK(ticket->gpuTime(value) == ALMetalCommandTimingStatus::pending);
    checkCleared(value);
    command.reset();
    CHECK(ticket->isComplete());
    value = {1.0, 2.0, 3};
    CHECK(ticket->gpuTime(value) == ALMetalCommandTimingStatus::failed);
    checkCleared(value);
    CHECK(ALMetalDevice::committedCommandBufferCount() == before);
}

TEST_CASE("almetal: command GPU timing survives wrappers and excludes queue delay",
          "[almetal][completion][command-timing]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error); REQUIRE(device);
    struct RestoreTiming
    {
        bool enabled = ALMetalDevice::commandTimingEnabled();
        ~RestoreTiming() { ALMetalDevice::setCommandTimingEnabled(enabled); }
    } restore;
    ALMetalDevice::setCommandTimingEnabled(false);
    const auto aggregate_before = ALMetalDevice::commandTiming();

    id<MTLDevice> native_device = (__bridge id<MTLDevice>)device->nativeHandle();
    id<MTLSharedEvent> event = [native_device newSharedEvent]; REQUIRE(event);
    ReleaseGate gate(event);
    ALMetalBufferDescriptor bd;
    bd.length = 32; bd.storage = ALMetalStorageMode::shared;
    bd.label = "Command interval exact-byte output";
    auto buffer = ALMetalBuffer::create(*device, bd, &error); REQUIRE(buffer);
    std::shared_ptr<const ALMetalCommandCompletion> ticket;
    __weak id<MTLCommandBuffer> weak_command;
    const auto submissions = ALMetalDevice::committedCommandBufferCount();
    @autoreleasepool
    {
        auto blocker = device->makeCommandBuffer("Prior queue work, not measured", &error);
        auto command = device->makeCommandBuffer("Measured GPU execution only", &error);
        REQUIRE(blocker); REQUIRE(command);
        id<MTLCommandBuffer> native_blocker =
            (__bridge id<MTLCommandBuffer>)blocker->nativeHandle();
        [native_blocker encodeWaitForEvent:event value:1];
        id<MTLBlitCommandEncoder> prior_blit = [native_blocker blitCommandEncoder];
        REQUIRE(prior_blit);
        [prior_blit fillBuffer:(__bridge id<MTLBuffer>)buffer->nativeHandle()
                        range:NSMakeRange(0, 16) value:0x31];
        [prior_blit endEncoding];
        id<MTLCommandBuffer> native_command =
            (__bridge id<MTLCommandBuffer>)command->nativeHandle();
        id<MTLBlitCommandEncoder> blit = [native_command blitCommandEncoder]; REQUIRE(blit);
        [blit fillBuffer:(__bridge id<MTLBuffer>)buffer->nativeHandle()
                  range:NSMakeRange(16, 16) value:0x73];
        [blit endEncoding];
        ticket = command->completionToken(); REQUIRE(ticket);
        weak_command = native_command;
        REQUIRE(blocker->commit(&error));
        REQUIRE(command->commit(&error));
    }
    CHECK(weak_command != nil);
    // This deliberate delay belongs to preceding queue work, not the measured
    // command. The oracle below compares the exact native GPU timestamp pair.
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    ALMetalCommandGPUTime value{1.0, 2.0, 3};
    CHECK(ticket->gpuTime(value) == ALMetalCommandTimingStatus::pending);
    CHECK_FALSE(gate.tripped());
    checkCleared(value);
    CHECK_FALSE(ticket->isComplete());
    CHECK(ALMetalDevice::committedCommandBufferCount() == submissions + 2);
    gate.release();
    REQUIRE(device->waitUntilIdle(&error)); // Test teardown/oracle only.
    REQUIRE(ticket->gpuTime(value) == ALMetalCommandTimingStatus::ready);
    REQUIRE(ticket->isComplete());
    id<MTLCommandBuffer> native_command = weak_command;
    REQUIRE(native_command);
    CHECK(value.start_seconds == native_command.GPUStartTime);
    CHECK(value.end_seconds == native_command.GPUEndTime);
    CHECK(value.elapsed_nanoseconds == static_cast<std::uint64_t>(std::round(
        (native_command.GPUEndTime - native_command.GPUStartTime) * 1'000'000'000.0)));
    CHECK(value.elapsed_nanoseconds > 0);
    ALMetalCommandGPUTime repeated;
    CHECK(ticket->gpuTime(repeated) == ALMetalCommandTimingStatus::ready);
    CHECK(repeated.start_seconds == value.start_seconds);
    CHECK(repeated.end_seconds == value.end_seconds);
    CHECK(repeated.elapsed_nanoseconds == value.elapsed_nanoseconds);
    const auto* bytes = static_cast<const unsigned char*>(buffer->mappedBytes());
    REQUIRE(bytes);
    for (std::size_t i = 0; i < 32; ++i)
        CHECK(bytes[i] == (i < 16 ? 0x31 : 0x73));
    CHECK(ALMetalDevice::commandTiming().submitted == aggregate_before.submitted);
}

TEST_CASE("almetal: dropping a pending GPU timing ticket does not wait",
          "[almetal][completion][command-timing]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error); REQUIRE(device);
    id<MTLDevice> native_device = (__bridge id<MTLDevice>)device->nativeHandle();
    id<MTLSharedEvent> event = [native_device newSharedEvent]; REQUIRE(event);
    ReleaseGate gate(event);
    auto command = device->makeCommandBuffer("Dropped pending GPU timing", &error);
    REQUIRE(command);
    id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
    [native encodeWaitForEvent:event value:1];
    auto ticket = command->completionToken(); REQUIRE(ticket);
    REQUIRE(command->commit(&error));
    command.reset(); native = nil;
    ticket.reset();
    CHECK_FALSE(gate.tripped());
    gate.release();
    REQUIRE(device->waitUntilIdle(&error));
}

TEST_CASE("almetal: renderer batch timing and slot readiness poll real GPU work",
          "[almetal][completion][command-timing][world]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
    // Warm the real renderer before arming the bounded GPU gate.
    auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
    INFO(error); REQUIRE(renderer);
    CHECK(renderer->commandBatchSlotAvailable(1));
    CHECK_FALSE(renderer->commandBatchCompletion());
    id<MTLDevice> native_device = (__bridge id<MTLDevice>)device->nativeHandle();
    id<MTLSharedEvent> event = [native_device newSharedEvent]; REQUIRE(event);
    ReleaseGate gate(event);
    auto blocker = device->makeCommandBuffer("Renderer receipt prior queue gate", &error);
    REQUIRE(blocker);
    id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)blocker->nativeHandle();
    [native encodeWaitForEvent:event value:1];
    REQUIRE(blocker->commit(&error));

    REQUIRE(renderer->beginCommandBatch("Real clear-pass timing receipt", &error));
    auto receipt = renderer->commandBatchCompletion(); REQUIRE(receipt);
    CHECK(receipt == renderer->commandBatchCompletion());
    CHECK_FALSE(renderer->commandBatchSlotAvailable(1));
    ALMetalWorldGBufferScene scene;
    scene.width = scene.height = 8;
    scene.clear_color[0] = 1.f; scene.clear_color[1] = 0.f;
    scene.clear_color[2] = 0.f; scene.clear_color[3] = 1.f;
    // Empty geometry still encodes a real render pass with four stored MRTs
    // and cleared depth, rather than an empty native command buffer.
    auto target = renderer->render(scene, &error); INFO(error); REQUIRE(target);
    REQUIRE(renderer->commitCommandBatch(&error));
    CHECK_FALSE(renderer->commandBatchActive());
    CHECK_FALSE(renderer->commandBatchCompletion());
    ALMetalCommandGPUTime timing{1.0, 2.0, 3};
    CHECK(receipt->gpuTime(timing) == ALMetalCommandTimingStatus::pending);
    checkCleared(timing);
    CHECK_FALSE(renderer->commandBatchSlotAvailable(1));
    CHECK_FALSE(gate.tripped());

    gate.release();
    REQUIRE(device->waitUntilIdle(&error)); // Test oracle/teardown only.
    REQUIRE(receipt->gpuTime(timing) == ALMetalCommandTimingStatus::ready);
    CHECK(timing.elapsed_nanoseconds > 0);
    CHECK(renderer->commandBatchSlotAvailable(1));
    const auto* color = target->colorTexture(0); REQUIRE(color);
    std::array<std::uint8_t, 8 * 8 * 4> pixels{};
    REQUIRE(color->readback(*device, 0, 0, {}, pixels.data(), 8 * 4, &error));
    for (std::size_t pixel = 0; pixel < 8 * 8; ++pixel)
    {
        CHECK(pixels[pixel * 4] == 255);
        CHECK(pixels[pixel * 4 + 1] == 0);
        CHECK(pixels[pixel * 4 + 2] == 0);
        CHECK(pixels[pixel * 4 + 3] == 255);
    }
    CHECK_FALSE(gate.tripped());
}
