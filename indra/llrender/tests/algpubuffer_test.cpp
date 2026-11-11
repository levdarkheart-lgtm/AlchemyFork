/**
 * @file algpubuffer_test.cpp
 * @brief Native Metal ALGPUBuffer storage and transfer tests.
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

#include "catch2/catch_test_macros.hpp"

#include "algpubuffer.h"
#include "almetaldevice.h"
#include "almetalresource.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr U32 WORDS = 64;

    std::vector<U32> pattern(U32 base)
    {
        std::vector<U32> words(WORDS);
        for (U32 index = 0; index < WORDS; ++index)
        {
            words[index] = base + index * 3u;
        }
        return words;
    }

    std::vector<U32> readAll(const ALGPUBuffer& buffer)
    {
        std::vector<U32> words(buffer.size() / sizeof(U32), 0xffffffffu);
        REQUIRE(buffer.read(0, words.data(), words.size() * sizeof(U32)));
        return words;
    }

    class MetalBufferDeviceScope
    {
    public:
        explicit MetalBufferDeviceScope(ALMetalDevice* device)
        : mPrevious(ALGPUBuffer::getMetalDevice())
        {
            ALGPUBuffer::setMetalDevice(device);
        }

        ~MetalBufferDeviceScope()
        {
            ALGPUBuffer::setMetalDevice(mPrevious);
        }

    private:
        ALMetalDevice* mPrevious = nullptr;
    };
}

TEST_CASE("ALGPUBuffer uses native Metal storage for transfer and readback",
          "[algpubuffer][almetal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);
    MetalBufferDeviceScope device_scope(device.get());

    const U64 bytes_before = ALGPUBuffer::sBytesAllocated;
    {
        const auto initial = pattern(11);
        ALGPUBuffer buffer;
        REQUIRE(buffer.allocate(initial.size() * sizeof(U32),
                                ALGPUBuffer::DYNAMIC,
                                initial.data(), "native private words"));
        CHECK(buffer.isAllocated());
        REQUIRE(buffer.getMetalBuffer());
        CHECK(buffer.getMetalBuffer()->nativeHandle());
        CHECK(buffer.size() == WORDS * sizeof(U32));
        CHECK(buffer.getMapped() == nullptr);
        CHECK(buffer.getFlags() == U32(ALGPUBuffer::DYNAMIC));
        CHECK(ALGPUBuffer::sBytesAllocated ==
              bytes_before + initial.size() * sizeof(U32));
        CHECK(readAll(buffer) == initial);

        const std::vector<U32> replacement(8, 0x12345678u);
        buffer.upload(4 * sizeof(U32), replacement.data(),
                      replacement.size() * sizeof(U32));
        auto expected = initial;
        std::copy(replacement.begin(), replacement.end(), expected.begin() + 4);
        CHECK(readAll(buffer) == expected);

        buffer.clearRange(20 * sizeof(U32), 12 * sizeof(U32), 0xaabbccddu);
        std::fill(expected.begin() + 20, expected.begin() + 32, 0xaabbccddu);
        CHECK(readAll(buffer) == expected);

        std::vector<U32> part(4);
        REQUIRE(buffer.read(23 * sizeof(U32), part.data(), part.size() * sizeof(U32)));
        CHECK(part == std::vector<U32>(expected.begin() + 23, expected.begin() + 27));

        buffer.clear(7);
        CHECK(readAll(buffer) == std::vector<U32>(WORDS, 7));
    }
    CHECK(ALGPUBuffer::sBytesAllocated == bytes_before);
}

TEST_CASE("ALGPUBuffer copies native stores and moves ownership",
          "[algpubuffer][almetal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);
    MetalBufferDeviceScope device_scope(device.get());

    const U64 bytes_before = ALGPUBuffer::sBytesAllocated;
    const auto source_words = pattern(100);
    const std::vector<U32> zeroes(WORDS, 0);
    ALGPUBuffer source;
    ALGPUBuffer target;
    REQUIRE(source.allocate(source_words.size() * sizeof(U32), 0,
                            source_words.data(), "native copy source"));
    REQUIRE(target.allocate(zeroes.size() * sizeof(U32),
                            0, zeroes.data(),
                            "native copy target"));

    target.copyFrom(source, 4 * sizeof(U32), 32 * sizeof(U32),
                    10 * sizeof(U32));
    auto expected = zeroes;
    std::copy(source_words.begin() + 4, source_words.begin() + 14,
              expected.begin() + 32);
    CHECK(readAll(target) == expected);

    target.copyFrom(target, 32 * sizeof(U32), 0, 10 * sizeof(U32));
    std::copy(expected.begin() + 32, expected.begin() + 42,
              expected.begin());
    CHECK(readAll(target) == expected);
    CHECK(readAll(source) == source_words);

    const ALMetalBuffer* original = target.getMetalBuffer();
    const void* native = original->nativeHandle();
    ALGPUBuffer moved(std::move(target));
    CHECK_FALSE(target.isAllocated());
    CHECK(target.size() == 0);
    CHECK(target.getMetalBuffer() == nullptr);
    CHECK(moved.isAllocated());
    CHECK(moved.getMetalBuffer() == original);
    CHECK(moved.getMetalBuffer()->nativeHandle() == native);
    CHECK(readAll(moved) == expected);

    ALGPUBuffer other;
    REQUIRE(other.allocate(2 * WORDS * sizeof(U32)));
    CHECK(ALGPUBuffer::sBytesAllocated == bytes_before + 4 * WORDS * sizeof(U32));
    other = std::move(moved);
    CHECK_FALSE(moved.isAllocated());
    CHECK(moved.size() == 0);
    CHECK(moved.getMetalBuffer() == nullptr);
    CHECK(other.getMetalBuffer() == original);
    CHECK(other.getMetalBuffer()->nativeHandle() == native);
    CHECK(readAll(other) == expected);
    CHECK(ALGPUBuffer::sBytesAllocated == bytes_before + 2 * WORDS * sizeof(U32));
    other.clear(0xa5a5a5a5u);
    CHECK(readAll(other) == std::vector<U32>(WORDS, 0xa5a5a5a5u));
    other.release();
    CHECK_FALSE(other.isAllocated());
    CHECK(other.size() == 0);
    CHECK(other.getMetalBuffer() == nullptr);
    CHECK(other.getMapped() == nullptr);
    CHECK(ALGPUBuffer::sBytesAllocated ==
          bytes_before + source_words.size() * sizeof(U32));
    source.release();
    CHECK(ALGPUBuffer::sBytesAllocated == bytes_before);
}

TEST_CASE("ALGPUBuffer streaming and readback stores map coherent shared memory",
          "[algpubuffer][almetal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);
    MetalBufferDeviceScope device_scope(device.get());

    const auto words = pattern(900);
    for (U32 flags : { U32(ALGPUBuffer::STREAM),
                      U32(ALGPUBuffer::STREAM | ALGPUBuffer::FLUSH_EXPLICIT) })
    {
        INFO(flags);
        ALGPUBuffer stream;
        REQUIRE(stream.allocate(words.size() * sizeof(U32), flags,
                                nullptr, "native stream"));
        REQUIRE(stream.getMapped());
        CHECK(stream.getMetalBuffer()->storageMode() == ALMetalStorageMode::shared);
        stream.upload(0, words.data(), words.size() * sizeof(U32));

        ALGPUBuffer gpu_copy;
        REQUIRE(gpu_copy.allocate(words.size() * sizeof(U32)));
        gpu_copy.copyFrom(stream, 0, 0, words.size() * sizeof(U32));
        CHECK(readAll(gpu_copy) == words);

        auto expected = words;
        const std::vector<U32> replacement(8, 9000);
        std::memcpy(stream.getMapped() + 16 * sizeof(U32), replacement.data(),
                    replacement.size() * sizeof(U32));
        stream.flushMapped(16 * sizeof(U32), replacement.size() * sizeof(U32));
        std::copy(replacement.begin(), replacement.end(), expected.begin() + 16);
        gpu_copy.copyFrom(stream, 0, 0, expected.size() * sizeof(U32));
        CHECK(readAll(gpu_copy) == expected);

        ALGPUBuffer readback;
        REQUIRE(readback.allocate(words.size() * sizeof(U32),
                                  ALGPUBuffer::READBACK, nullptr,
                                  "native readback"));
        REQUIRE(readback.getMapped());
        CHECK(readback.getMetalBuffer()->storageMode() == ALMetalStorageMode::shared);
        auto command = device->makeCommandBuffer("native mapped readback copy", &error);
        REQUIRE(command);
        REQUIRE(readback.getMetalBuffer()->copyFrom(*command, *gpu_copy.getMetalBuffer(),
                                                   0, 0, expected.size() * sizeof(U32), &error));
        REQUIRE(command->commit(&error));
        REQUIRE(command->waitUntilCompleted(&error));
        CHECK(std::memcmp(readback.getMapped(), expected.data(),
                          expected.size() * sizeof(U32)) == 0);
        CHECK(readAll(readback) == expected);
    }
}
TEST_CASE("ALGPUBuffer queued native copies retain storage beyond wrapper and backend retirement",
          "[algpubuffer][almetal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);
    MetalBufferDeviceScope device_scope(device.get());
    const U64 bytes_before = ALGPUBuffer::sBytesAllocated;
    const auto expected = pattern(7000);

    ALMetalBufferDescriptor descriptor;
    descriptor.length = expected.size() * sizeof(U32);
    descriptor.label = "native retained buffer result";
    auto result = ALMetalBuffer::create(*device, descriptor, &error);
    REQUIRE(result);
    auto command = device->makeCommandBuffer("native buffer owner retirement", &error);
    REQUIRE(command);
    {
        ALGPUBuffer source;
        REQUIRE(source.allocate(descriptor.length, ALGPUBuffer::STREAM,
                                expected.data(), "native retiring stream"));
        U8* mapped = source.getMapped();
        REQUIRE(mapped);
        const ALMetalBuffer* original = source.getMetalBuffer();
        ALGPUBuffer moved(std::move(source));
        CHECK(moved.getMapped() == mapped);
        CHECK(moved.getMetalBuffer() == original);
        CHECK(moved.getFlags() == U32(ALGPUBuffer::STREAM));
        CHECK(source.getMapped() == nullptr);
        REQUIRE(result->copyFrom(*command, *moved.getMetalBuffer(),
                                 0, 0, descriptor.length, &error));
        CHECK_FALSE(command->isCommitted());
        // Native command ownership protects the encoded source. Retiring the
        // renderer device binding cannot prevent the wrapper releasing bytes.
        ALGPUBuffer::setMetalDevice(nullptr);
    }
    CHECK(ALGPUBuffer::sBytesAllocated == bytes_before);
    REQUIRE(command->commitAndWait(&error));
    CHECK(command->status() == ALMetalCommandStatus::completed);
    std::vector<U32> actual(expected.size());
    REQUIRE(result->read(0, actual.data(), descriptor.length, &error));
    CHECK(actual == expected);
    ALGPUBuffer::setMetalDevice(device.get());
}
