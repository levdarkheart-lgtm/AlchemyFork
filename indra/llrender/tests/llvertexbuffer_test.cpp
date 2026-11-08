/**
 * @file llvertexbuffer_test.cpp
 * @brief Native Metal vertex-buffer publication and lifetime tests.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../llrender.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetalframe.h"
#include "../../almetal/almetalresource.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

TEST_CASE(
    "LLVertexBuffer preserves selective native stores and in-flight generations",
    "[llvertexbuffer][almetal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);
    auto scheduler = ALMetalFrameScheduler::create(*device, 2, &error);
    INFO(error);
    REQUIRE(scheduler);

    LLVertexBuffer::setMetalDevice(device.get());
    LLVertexBuffer::setMetalBufferAuthority(true);
    gGL.frameFences().setMetalScheduler(scheduler.get());
    LLVertexBuffer::initClass(nullptr);
    struct ResetMetalAuthority
    {
        ~ResetMetalAuthority()
        {
            LLVertexBuffer::flushBuffers();
            gGL.frameFences().setMetalScheduler(nullptr);
            LLVertexBuffer::cleanupClass();
            LLVertexBuffer::setMetalBufferAuthority(false);
            LLVertexBuffer::setMetalDevice(nullptr);
        }
    } reset_metal_authority;

    LLPointer<LLVertexBuffer> buffer =
        new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
    REQUIRE(buffer->allocateBuffer(3, 3));
    std::memset(buffer->getMappedData(), 0, buffer->getSize());
    std::memset(buffer->getMappedIndices(), 0,
                buffer->getIndicesSize());

    LLStrider<LLVector3> positions;
    REQUIRE(buffer->getVertexStrider(positions));
    positions[0].set(-1.f, 0.f, 0.f);
    positions[1].set(1.f, 0.f, 0.f);
    positions[2].set(0.f, 1.f, 0.f);
    LLStrider<U16> indices;
    REQUIRE(buffer->getIndexStrider(indices));
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    buffer->unmapBuffer();

    const auto read_store = [&error](const ALMetalBuffer& store)
    {
        std::vector<U8> bytes(store.length());
        REQUIRE(store.read(0, bytes.data(), bytes.size(), &error));
        INFO(error);
        return bytes;
    };

    ALMetalBuffer* first_vertices = buffer->getMetalVertexBuffer();
    ALMetalBuffer* first_indices = buffer->getMetalIndexBuffer();
    REQUIRE(first_vertices);
    REQUIRE(first_indices);
    const std::vector<U8> initial_vertices = read_store(*first_vertices);
    const std::vector<U8> initial_indices = read_store(*first_indices);

    ALMetalBufferDescriptor result_descriptor;
    result_descriptor.length = initial_vertices.size();
    result_descriptor.storage = ALMetalStorageMode::shared;
    result_descriptor.label = "Alchemy vertex generation result";
    auto result = ALMetalBuffer::create(*device, result_descriptor, &error);
    INFO(error);
    REQUIRE(result);
    auto frame = scheduler->beginFrame(
        "Alchemy vertex generation frame", UINT32_MAX, &error);
    INFO(error);
    REQUIRE(frame);
    REQUIRE(result->copyFrom(
        frame->commandBuffer(), *first_vertices, 0, 0,
        initial_vertices.size(), &error));
    INFO(error);

    // A vertex-only update must neither publish the deliberately changed CPU
    // index shadow nor overwrite the generation the queued GPU copy reads.
    buffer->getMappedIndices()[0] ^= 0x7f;
    REQUIRE(buffer->getVertexStrider(positions));
    positions[0].set(-0.5f, 0.f, 0.f);
    buffer->unmapBuffer();
    REQUIRE(buffer->getMetalVertexBuffer() != first_vertices);
    CHECK(buffer->getMetalIndexBuffer() == first_indices);
    CHECK(read_store(*buffer->getMetalIndexBuffer()) == initial_indices);
    CHECK(read_store(*first_vertices) == initial_vertices);

    ALMetalBuffer* second_vertices = buffer->getMetalVertexBuffer();
    const std::vector<U8> updated_vertices = read_store(*second_vertices);

    // The inverse update leaves the current vertex generation untouched.
    buffer->getMappedData()[0] ^= 0x5a;
    REQUIRE(buffer->getIndexStrider(indices));
    indices[0] = 2;
    indices[1] = 1;
    indices[2] = 0;
    buffer->unmapBuffer();
    CHECK(buffer->getMetalVertexBuffer() == second_vertices);
    REQUIRE(buffer->getMetalIndexBuffer() != first_indices);
    CHECK(read_store(*buffer->getMetalVertexBuffer()) == updated_vertices);

    const U32 first_frame = gGL.frameFences().frame();
    REQUIRE(scheduler->submit(*frame, &error));
    gGL.frameFences().endFrame();
    gGL.frameFences().waitFinished(first_frame);
    CHECK(read_store(*result) == initial_vertices);

    // Completed generations are reused rather than growing without bound.
    REQUIRE(buffer->getVertexStrider(positions));
    positions[1].set(0.5f, 0.f, 0.f);
    buffer->unmapBuffer();
    CHECK(buffer->getMetalVertexBuffer() == first_vertices);

    // Native authority deliberately has no mGLIndices. Resizing must still
    // release the old CPU and Metal index stores before allocating new ones.
    REQUIRE(buffer->allocateBuffer(4, 6));
    CHECK(buffer->getIndicesSize() == 6 * sizeof(U16));
    CHECK(LLVertexBuffer::getBytesAllocated() ==
          buffer->getSize() + buffer->getIndicesSize());
    REQUIRE(buffer->getMetalVertexBuffer());
    REQUIRE(buffer->getMetalIndexBuffer());

    // Destruction after pointer capture but before submission must not leave
    // the encoder with a dangling wrapper or discard the old bytes.
    std::memset(buffer->getMappedData(), 0x2d, buffer->getSize());
    std::memset(buffer->getMappedIndices(), 0,
                buffer->getIndicesSize());
    REQUIRE(buffer->getVertexStrider(positions));
    REQUIRE(buffer->getIndexStrider(indices));
    buffer->unmapBuffer();
    ALMetalBuffer* final_vertices = buffer->getMetalVertexBuffer();
    REQUIRE(final_vertices);
    const std::vector<U8> final_bytes = read_store(*final_vertices);

    result_descriptor.length = final_bytes.size();
    auto final_result = ALMetalBuffer::create(
        *device, result_descriptor, &error);
    INFO(error);
    REQUIRE(final_result);
    auto final_frame = scheduler->beginFrame(
        "Alchemy retired vertex generation frame", UINT32_MAX, &error);
    INFO(error);
    REQUIRE(final_frame);
    buffer = nullptr;
    CHECK(LLVertexBuffer::getBytesAllocated() == 0);
    REQUIRE(final_result->copyFrom(
        final_frame->commandBuffer(), *final_vertices, 0, 0,
        final_bytes.size(), &error));
    INFO(error);

    const U32 retired_frame = gGL.frameFences().frame();
    REQUIRE(scheduler->submit(*final_frame, &error));
    gGL.frameFences().endFrame();
    gGL.frameFences().waitFinished(retired_frame);
    CHECK(read_store(*final_result) == final_bytes);
    LLVertexBuffer::flushBuffers();
}
