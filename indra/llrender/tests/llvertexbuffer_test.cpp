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

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace
{
unsigned buffer_gl_calls = 0;
void APIENTRY bufferBindTrap(GLenum, GLuint) { ++buffer_gl_calls; }

struct BufferGLTraps
{
    PFNGLBINDBUFFERPROC bind = glBindBuffer;
    unsigned previous_calls = buffer_gl_calls;

    BufferGLTraps()
    {
        buffer_gl_calls = 0;
        glBindBuffer = bufferBindTrap;
    }
    ~BufferGLTraps()
    {
        glBindBuffer = bind;
        buffer_gl_calls = previous_calls;
    }
};
}

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

TEST_CASE(
    "LLVertexBuffer retries failed native stores without another CPU write",
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
        ALMetalDevice* device;
        ~ResetMetalAuthority()
        {
            LLVertexBuffer::setMetalDevice(device);
            LLVertexBuffer::flushBuffers();
            gGL.frameFences().setMetalScheduler(nullptr);
            LLVertexBuffer::cleanupClass();
            LLVertexBuffer::setMetalBufferAuthority(false);
            LLVertexBuffer::setMetalDevice(nullptr);
        }
    } reset{device.get()};
    struct MissingMetalDevice
    {
        ALMetalDevice* device;
        explicit MissingMetalDevice(ALMetalDevice* previous) : device(previous)
        {
            LLVertexBuffer::setMetalDevice(nullptr);
        }
        ~MissingMetalDevice() { LLVertexBuffer::setMetalDevice(device); }
    };

    const auto make_buffer = []
    {
        LLPointer<LLVertexBuffer> result =
            new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
        REQUIRE(result->allocateBuffer(3, 3));
        return result;
    };
    const auto write_cpu_stores = [](LLVertexBuffer& buffer, F32 offset,
                                    bool reverse)
    {
        // Initialize padding too, so whole-store byte comparisons are exact.
        std::memset(buffer.getMappedData(), 0, buffer.getSize());
        std::memset(buffer.getMappedIndices(), 0, buffer.getIndicesSize());
        LLStrider<LLVector3> positions;
        REQUIRE(buffer.getVertexStrider(positions));
        positions[0].set(-1.f + offset, 0.f, 0.f);
        positions[1].set(1.f + offset, 0.f, 0.f);
        positions[2].set(offset, 1.f, 0.f);
        LLStrider<U16> indices;
        REQUIRE(buffer.getIndexStrider(indices));
        indices[0] = reverse ? 2 : 0;
        indices[1] = 1;
        indices[2] = reverse ? 0 : 2;
    };
    const auto cpu_vertices = [](const LLVertexBuffer& buffer)
    {
        return std::vector<U8>(buffer.getMappedData(),
                              buffer.getMappedData() + buffer.getSize());
    };
    const auto cpu_indices = [](const LLVertexBuffer& buffer)
    {
        return std::vector<U8>(buffer.getMappedIndices(),
                              buffer.getMappedIndices() + buffer.getIndicesSize());
    };
    const auto read_store = [&error](const ALMetalBuffer* store)
    {
        REQUIRE(store);
        std::vector<U8> bytes(store->length());
        REQUIRE(store->read(0, bytes.data(), bytes.size(), &error));
        INFO(error);
        return bytes;
    };

    SECTION("first publication retries both vertex and index stores")
    {
        auto buffer = make_buffer();
        write_cpu_stores(*buffer, 0.125f, true);
        const auto expected_vertices = cpu_vertices(*buffer);
        const auto expected_indices = cpu_indices(*buffer);
        const U64 stamp = buffer->getContentStamp();
        {
            MissingMetalDevice missing(device.get());
            // Expected missing-device warnings. No native store is published;
            // the second flush also must leave both pending writes queued.
            LLVertexBuffer::flushBuffers();
            LLVertexBuffer::flushBuffers();
        }
        // Restore the device and retry ONLY by flushing, without another map,
        // setter or CPU write. Pre-fix this reads the never-uploaded stores.
        LLVertexBuffer::flushBuffers();
        CHECK(buffer->getContentStamp() == stamp);
        CHECK(read_store(buffer->getMetalVertexBuffer()) == expected_vertices);
        CHECK(read_store(buffer->getMetalIndexBuffer()) == expected_indices);
        // Destroy a genuinely failed, still-dirty queued buffer. Its
        // destructor must disarm rather than resurrect the retry entry.
        write_cpu_stores(*buffer, 0.25f, false);
        {
            MissingMetalDevice missing(device.get());
            LLVertexBuffer::flushBuffers();
            buffer = nullptr;
        }
        LLVertexBuffer::flushBuffers();
        CHECK(LLVertexBuffer::getBytesAllocated() == 0);
    }

    SECTION("queue compaction removes a clean entry and preserves a failed update")
    {
        auto clean = make_buffer();
        auto retry = make_buffer();
        write_cpu_stores(*clean, 0.f, false);
        write_cpu_stores(*retry, 0.f, false);
        LLVertexBuffer::flushBuffers();
        const auto previous_vertices = read_store(retry->getMetalVertexBuffer());
        const auto previous_indices = read_store(retry->getMetalIndexBuffer());

        write_cpu_stores(*clean, 0.25f, true);
        write_cpu_stores(*retry, 0.5f, true);
        const auto clean_vertices = cpu_vertices(*clean);
        const auto expected_vertices = cpu_vertices(*retry);
        const auto expected_indices = cpu_indices(*retry);
        // The public lazy draw boundary uploads this entry, but leaves it on
        // the global mapped list until flushBuffers processes that list.
        clean->setBuffer();
        CHECK(read_store(clean->getMetalVertexBuffer()) == clean_vertices);
        {
            MissingMetalDevice missing(device.get());
            LLVertexBuffer::flushBuffers();
            CHECK(read_store(retry->getMetalVertexBuffer()) == previous_vertices);
            CHECK(read_store(retry->getMetalIndexBuffer()) == previous_indices);
            // Destruction of the already-clean entry must not erase the
            // retained retry entry or leave a dangling mapped-list pointer.
            clean = nullptr;
        }
        LLVertexBuffer::flushBuffers();
        CHECK(read_store(retry->getMetalVertexBuffer()) == expected_vertices);
        CHECK(read_store(retry->getMetalIndexBuffer()) == expected_indices);
        retry = nullptr;
        LLVertexBuffer::flushBuffers();
        CHECK(LLVertexBuffer::getBytesAllocated() == 0);
    }
}

TEST_CASE(
    "LLVertexBuffer stages and uploads natively without legacy allocation policy flags",
    "[llvertexbuffer][almetal][native-allocation]")
{
    REQUIRE_FALSE(gGLManager.mInited);
    BufferGLTraps gl_traps;
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);
    auto scheduler = ALMetalFrameScheduler::create(*device, 2, &error);
    REQUIRE(scheduler);
    const bool previous_authority = LLVertexBuffer::isMetalBufferAuthority();
    const bool previous_apple = gGLManager.mIsApple;
    LLVertexBuffer::setMetalDevice(device.get());
    LLVertexBuffer::setMetalBufferAuthority(false);
    gGLManager.mIsApple = false;
    gGL.frameFences().setMetalScheduler(scheduler.get());
    LLVertexBuffer::initClass(nullptr);
    struct Reset
    {
        bool authority, apple;
        ~Reset()
        {
            LLVertexBuffer::flushBuffers();
            gGL.frameFences().setMetalScheduler(nullptr);
            // Draw/client-array compatibility is outside this allocation
            // cleanup. Keep cleanupClass's retained unbind on its native path.
            LLVertexBuffer::setMetalBufferAuthority(true);
            LLVertexBuffer::cleanupClass();
            LLVertexBuffer::setMetalBufferAuthority(authority);
            LLVertexBuffer::setMetalDevice(nullptr);
            gGLManager.mIsApple = apple;
        }
    } reset{previous_authority, previous_apple};

    LLPointer<LLVertexBuffer> buffer = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
    REQUIRE(buffer->allocateBuffer(3, 6));
    CHECK(reinterpret_cast<std::uintptr_t>(buffer->getMappedData()) % 16 == 0);
    CHECK(reinterpret_cast<std::uintptr_t>(buffer->getMappedIndices()) % 16 == 0);
    CHECK(LLVertexBuffer::getBytesAllocated() == buffer->getSize() + buffer->getIndicesSize());
    std::array<LLVector4a, 3> positions;
    positions[0].set(-1.f, 0.f, 0.f, 1.f);
    positions[1].set(1.f, 0.f, 0.f, 1.f);
    positions[2].set(0.f, 1.f, 0.f, 1.f);
    const std::array<U32, 3> indices{2, 1, 0};
    std::memset(buffer->getMappedData(), 0, buffer->getSize());
    buffer->setPositionData(positions.data());
    buffer->setIndexData(indices.data());
    CHECK(buffer->getIndexStride() == sizeof(U32));
    CHECK(buffer->getNumIndices() == indices.size());
    const auto published_stamp = buffer->getContentStamp();
    // Zero-length partial writes must not dirty a store or mint a new stamp.
    buffer->setPositionData(positions.data(), 1, 0);
    CHECK(buffer->getContentStamp() == published_stamp);
    buffer->unmapBuffer();
    CHECK(buffer->getContentStamp() == published_stamp);

    const auto read_store = [&error](const ALMetalBuffer* store)
    {
        REQUIRE(store);
        std::vector<U8> bytes(store->length());
        REQUIRE(store->read(0, bytes.data(), bytes.size(), &error));
        INFO(error);
        return bytes;
    };
    const auto expected = [](const void* bytes, std::size_t count)
    {
        const auto* first = static_cast<const U8*>(bytes);
        return std::vector<U8>(first, first + count);
    };
    CHECK(read_store(buffer->getMetalVertexBuffer()) == expected(positions.data(), sizeof(positions)));
    const auto index_bytes = expected(indices.data(), sizeof(indices));
    CHECK(read_store(buffer->getMetalIndexBuffer()) == index_bytes);
    auto* original_indices = buffer->getMetalIndexBuffer();

    LLVector4a replacement;
    replacement.set(0.25f, -0.5f, 0.75f, 1.f);
    buffer->setPositionData(&replacement, 1, 1);
    CHECK(buffer->getContentStamp() != published_stamp);
    buffer->unmapBuffer();
    positions[1] = replacement;
    CHECK(read_store(buffer->getMetalVertexBuffer()) == expected(positions.data(), sizeof(positions)));
    CHECK(buffer->getMetalIndexBuffer() == original_indices);
    CHECK(read_store(buffer->getMetalIndexBuffer()) == index_bytes);

    auto* unchanged_vertices = buffer->getMetalVertexBuffer();
    const U32 replacement_index = 2;
    buffer->setIndexData(&replacement_index, 1, 1);
    buffer->unmapBuffer();
    const std::array<U32, 3> updated_indices{2, 2, 0};
    CHECK(buffer->getMetalVertexBuffer() == unchanged_vertices);
    CHECK(read_store(buffer->getMetalIndexBuffer()) ==
          expected(updated_indices.data(), sizeof(updated_indices)));
    CHECK(buffer_gl_calls == 0);

    buffer = nullptr;
    LLVertexBuffer::flushBuffers();
    CHECK(LLVertexBuffer::getBytesAllocated() == 0);
    CHECK(buffer_gl_calls == 0);
}
