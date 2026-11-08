/**
 * @file almetal_completion_test.mm
 * @brief Real GPU completion identities used by shared frame-data arenas.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "../almetaldevice.h"
#include "../almetalresource.h"

#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("almetal: completion tokens survive wrappers and distinguish abandonment",
          "[almetal][completion]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    REQUIRE(device);

    SECTION("discarded uncommitted commands release arena ownership")
    {
        auto command = device->makeCommandBuffer("abandoned arena command", &error);
        REQUIRE(command);
        auto token = command->completionToken();
        REQUIRE(token);
        CHECK(token == command->completionToken());
        CHECK_FALSE(token->isComplete());
        command.reset();
        CHECK(token->isComplete());
    }

    SECTION("submitted GPU work remains pending after wrapper destruction")
    {
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device->nativeHandle();
        id<MTLSharedEvent> gate = [native_device newSharedEvent];
        REQUIRE(gate);
        // Always release the queue, including when a REQUIRE unwinds.
        struct SignalOnExit
        {
            id<MTLSharedEvent> event;
            ~SignalOnExit() { event.signaledValue = 1; }
        } signal{gate};

        ALMetalBufferDescriptor descriptor;
        descriptor.length = 16;
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.label = "Completion lifetime regression buffer";
        auto buffer = ALMetalBuffer::create(*device, descriptor, &error);
        REQUIRE(buffer);
        auto command = device->makeCommandBuffer("pending arena command", &error);
        REQUIRE(command);
        id<MTLCommandBuffer> native_command =
            (__bridge id<MTLCommandBuffer>)command->nativeHandle();
        [native_command encodeWaitForEvent:gate value:1];
        id<MTLBlitCommandEncoder> blit = [native_command blitCommandEncoder];
        REQUIRE(blit);
        [blit fillBuffer:(__bridge id<MTLBuffer>)buffer->nativeHandle()
                  range:NSMakeRange(0, descriptor.length) value:0x73];
        [blit endEncoding];
        auto token = command->completionToken();
        REQUIRE(token);
        REQUIRE(command->commit(&error));
        command.reset();
        CHECK_FALSE(token->isComplete());
        native_command = nil;
        gate.signaledValue = 1;
        REQUIRE(device->waitUntilIdle(&error));
        CHECK(token->isComplete());
        CHECK(static_cast<const unsigned char*>(buffer->mappedBytes())[0] == 0x73);
    }
}
