/**
 * @file almetal_submission_owner_test.mm
 * @brief Work-group ownership follows native command abandonment/completion.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetaldevice.h"
#include "../almetalresource.h"

#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <thread>

TEST_CASE("almetal: submission scopes restore nested owners and never leak across threads",
          "[almetal][submission-owner]")
{
    auto main_owner = std::make_shared<int>(1);
    auto probe_owner = std::make_shared<int>(2);
    REQUIRE_FALSE(ALMetalSubmissionScope::current());
    {
        ALMetalSubmissionScope outer(main_owner);
        CHECK(ALMetalSubmissionScope::current().get() == main_owner.get());
        {
            ALMetalSubmissionScope probe(probe_owner);
            CHECK(ALMetalSubmissionScope::current().get() == probe_owner.get());
            bool other_thread_empty = false;
            std::thread worker([&] { other_thread_empty = !ALMetalSubmissionScope::current(); });
            worker.join();
            CHECK(other_thread_empty);
        }
        CHECK(ALMetalSubmissionScope::current().get() == main_owner.get());
    }
    CHECK_FALSE(ALMetalSubmissionScope::current());
}

TEST_CASE("almetal: scope exit and queue commit cannot retire pending native work ownership",
          "[almetal][submission-owner]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    REQUIRE(device);
    auto owner = std::make_shared<int>(17);
    std::weak_ptr<int> weak_owner(owner);
    std::unique_ptr<ALMetalCommandBuffer> command;
    {
        ALMetalSubmissionScope work(owner);
        command = device->makeCommandBuffer("Native group-credit ownership test", &error);
        REQUIRE(command);
    }
    owner.reset();
    REQUIRE_FALSE(weak_owner.expired());

    SECTION("uncommitted abandonment releases its owner without a future frame")
    {
        command.reset();
        CHECK(weak_owner.expired());
    }
    SECTION("real GPU completion releases an owner after its command wrapper is gone")
    {
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device->nativeHandle();
        id<MTLSharedEvent> gate = [native_device newSharedEvent];
        REQUIRE(gate);
        struct SignalOnExit
        {
            id<MTLSharedEvent> event;
            ~SignalOnExit() { event.signaledValue = 1; }
        } release{gate};
        ALMetalBufferDescriptor descriptor;
        descriptor.length = 16;
        descriptor.storage = ALMetalStorageMode::shared;
        auto buffer = ALMetalBuffer::create(*device, descriptor, &error);
        REQUIRE(buffer);
        id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
        [native encodeWaitForEvent:gate value:1];
        id<MTLBlitCommandEncoder> blit = [native blitCommandEncoder];
        REQUIRE(blit);
        [blit fillBuffer:(__bridge id<MTLBuffer>)buffer->nativeHandle()
                  range:NSMakeRange(0, 16) value:0x57];
        [blit endEncoding];
        REQUIRE(command->commit(&error));
        command.reset();
        CHECK_FALSE(weak_owner.expired());
        gate.signaledValue = 1;
        [native waitUntilCompleted];
        CHECK(native.status == MTLCommandBufferStatusCompleted);
        CHECK(weak_owner.expired());
        CHECK(static_cast<const unsigned char*>(buffer->mappedBytes())[0] == 0x57);
    }
}
