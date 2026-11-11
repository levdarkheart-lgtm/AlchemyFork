/**
 * @file alvertex_label_metal_test.mm
 * @brief Native buffer labels survive publication without changing GPU data.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../llrender.h"
#include "../llvertexbuffer.h"
#include "../llimagegl.h"
#include "../../almetal/almetalframe.h"
#include "../../almetal/almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#import <Metal/Metal.h>
#include <array>
#include <cstring>
#include <vector>

TEST_CASE("Native vertex diagnostic labels follow real stores and completed generation reuse",
          "[llvertexbuffer][almetal][native-label]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        auto frames = ALMetalFrameScheduler::create(*device, 2, &error); REQUIRE(frames);
        const bool old_authority = LLVertexBuffer::isMetalBufferAuthority();
        auto* old_device = LLImageGL::getMetalDevice();
        LLVertexBuffer::setMetalDevice(device.get());
        LLVertexBuffer::setMetalBufferAuthority(false);
        LLVertexBuffer::initClass(nullptr);
        gGL.frameFences().setMetalScheduler(frames.get());
        struct Reset
        {
            bool authority;
            ALMetalDevice* device;
            ~Reset()
            {
                LLVertexBuffer::flushBuffers();
                gGL.frameFences().setMetalScheduler(nullptr);
                LLVertexBuffer::cleanupClass();
                LLVertexBuffer::setMetalBufferAuthority(authority);
                LLVertexBuffer::setMetalDevice(device);
            }
        } reset{old_authority, old_device};

        ALMetalBufferDescriptor control_descriptor;
        control_descriptor.length = 16;
        control_descriptor.storage = ALMetalStorageMode::shared;
        control_descriptor.label = "Native diagnostic label visibility control";
        auto control = ALMetalBuffer::create(*device, control_descriptor, &error); REQUIRE(control);
        id<MTLBuffer> native_control = (__bridge id<MTLBuffer>)control->nativeHandle();
        const bool labels_visible = native_control.label != nil;
        if (!labels_visible) WARN("This Metal runtime does not expose optional resource labels; byte/ownership checks still run");
        const auto check_label = [labels_visible](id<MTLBuffer> native, NSString* expected)
        {
            REQUIRE(native != nil);
            if (labels_visible)
            {
                if (expected.length) CHECK([native.label isEqualToString:expected]);
                else CHECK((native.label == nil || native.label.length == 0));
            }
        };
        const auto read = [&error](const ALMetalBuffer& source)
        {
            std::vector<U8> bytes(source.length());
            REQUIRE(source.read(0, bytes.data(), bytes.size(), &error)); return bytes;
        };

        LLPointer<LLVertexBuffer> buffer = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
        LL_LABEL_VERTEX_BUFFER(buffer, "PASS_INITIAL"); // Label before allocation.
        REQUIRE(buffer->allocateBuffer(3, 3));
        id<MTLBuffer> first_vertices = (__bridge id<MTLBuffer>)buffer->getMetalVertexBuffer()->nativeHandle();
        id<MTLBuffer> first_indices = (__bridge id<MTLBuffer>)buffer->getMetalIndexBuffer()->nativeHandle();
        check_label(first_vertices, @"PASS_INITIAL vertex data");
        check_label(first_indices, @"PASS_INITIAL index data");
        CHECK_FALSE(buffer->hasPublishedMetalVertexBuffer());
        std::memset(buffer->getMappedData(), 0, buffer->getSize());
        std::array<LLVector4a, 3> positions;
        positions[0].set(-1.f, -1.f, 0.f, 1.f);
        positions[1].set(1.f, -1.f, 0.f, 1.f);
        positions[2].set(0.f, 1.f, 0.f, 1.f);
        const std::array<U16, 3> indices{0,1,2};
        buffer->setPositionData(positions.data()); buffer->setIndexData(indices.data());
        buffer->unmapBuffer();
        REQUIRE(buffer->hasPublishedMetalVertexBuffer()); REQUIRE(buffer->hasPublishedMetalIndexBuffer());
        const auto initial_bytes = read(*buffer->getMetalVertexBuffer());
        const auto index_bytes = read(*buffer->getMetalIndexBuffer());
        const auto stamp = buffer->getContentStamp();
        const auto allocations = alMetalResourceAllocationStats();
        const auto submissions = ALMetalDevice::committedCommandBufferCount();
        LL_LABEL_VERTEX_BUFFER(buffer, "PASS_SECOND");
        LL_LABEL_VERTEX_BUFFER(buffer, "PASS_SECOND"); // Cached, no new store.
        check_label(first_vertices, @"PASS_SECOND vertex data");
        check_label(first_indices, @"PASS_SECOND index data");
        CHECK(buffer->getMetalVertexBuffer()->nativeHandle() == (__bridge void*)first_vertices);
        CHECK(buffer->getMetalIndexBuffer()->nativeHandle() == (__bridge void*)first_indices);
        CHECK(buffer->getContentStamp() == stamp);
        CHECK(read(*buffer->getMetalVertexBuffer()) == initial_bytes);
        CHECK(read(*buffer->getMetalIndexBuffer()) == index_bytes);
        CHECK(buffer->hasPublishedMetalVertexBuffer()); CHECK(buffer->hasPublishedMetalIndexBuffer());
        CHECK(alMetalResourceAllocationStats().buffers_created == allocations.buffers_created);
        CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);

        ALMetalBufferDescriptor copy_descriptor;
        copy_descriptor.length = initial_bytes.size(); copy_descriptor.storage = ALMetalStorageMode::shared;
        auto copy = ALMetalBuffer::create(*device, copy_descriptor, &error); REQUIRE(copy);
        auto frame = frames->beginFrame("Label diagnostic prior-generation lease", UINT32_MAX, &error);
        REQUIRE(frame);
        REQUIRE(copy->copyFrom(frame->commandBuffer(), *buffer->getMetalVertexBuffer(),
            0, 0, initial_bytes.size(), &error));
        const auto old_frame = gGL.frameFences().frame();
        positions[0].set(-.5f, -.5f, 0.f, 1.f);
        buffer->setPositionData(positions.data()); buffer->unmapBuffer();
        REQUIRE(buffer->getMetalVertexBuffer()->nativeHandle() != (__bridge void*)first_vertices);
        check_label((__bridge id<MTLBuffer>)buffer->getMetalVertexBuffer()->nativeHandle(), @"PASS_SECOND vertex data");
        LL_LABEL_VERTEX_BUFFER(buffer, "PASS_THIRD");
        check_label((__bridge id<MTLBuffer>)buffer->getMetalVertexBuffer()->nativeHandle(), @"PASS_THIRD vertex data");
        check_label(first_vertices, @"PASS_SECOND vertex data"); // Retired generation unchanged.
        REQUIRE(frames->submit(*frame, &error));
        gGL.frameFences().endFrame(); gGL.frameFences().waitFinished(old_frame);
        CHECK(read(*copy) == initial_bytes);

        // Reuse the now-complete first store. Its old pass label must not leak
        // back into the newly published current generation.
        positions[1].set(.5f, -.5f, 0.f, 1.f);
        buffer->setPositionData(positions.data()); buffer->unmapBuffer();
        CHECK(buffer->getMetalVertexBuffer()->nativeHandle() == (__bridge void*)first_vertices);
        check_label(first_vertices, @"PASS_THIRD vertex data");
        CHECK(read(*copy) == initial_bytes);
        CHECK(read(*buffer->getMetalVertexBuffer()) != initial_bytes);
        const std::array<U16, 3> reversed{2,1,0};
        buffer->setIndexData(reversed.data()); buffer->unmapBuffer();
        REQUIRE(buffer->getMetalIndexBuffer()->nativeHandle() != (__bridge void*)first_indices);
        id<MTLBuffer> current_indices = (__bridge id<MTLBuffer>)buffer->getMetalIndexBuffer()->nativeHandle();
        check_label(current_indices, @"PASS_THIRD index data");
        CHECK(buffer->getMetalVertexBuffer()->nativeHandle() == (__bridge void*)first_vertices);
        const auto changed_indices = read(*buffer->getMetalIndexBuffer());
        REQUIRE(changed_indices.size() == sizeof(reversed));
        CHECK(std::memcmp(changed_indices.data(), reversed.data(), sizeof(reversed)) == 0);
        const auto final_stamp = buffer->getContentStamp();
        const auto final_submissions = ALMetalDevice::committedCommandBufferCount();
        buffer->setLabel(nullptr); buffer->setLabel("");
        check_label((__bridge id<MTLBuffer>)buffer->getMetalVertexBuffer()->nativeHandle(), nil);
        check_label(current_indices, nil);
        check_label(first_indices, @"PASS_THIRD index data"); // Older lease is not relabelled.
        CHECK(buffer->getContentStamp() == final_stamp);
        CHECK(ALMetalDevice::committedCommandBufferCount() == final_submissions);
        CHECK(buffer->hasPublishedMetalVertexBuffer()); CHECK(buffer->hasPublishedMetalIndexBuffer());
    }
}
