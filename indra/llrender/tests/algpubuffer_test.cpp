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

#include "alframefences.h"
#include "algpubuffer.h"
#include "almetaldevice.h"
#include "almetalframe.h"

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
        CHECK(buffer.getGLName() == 0);
        CHECK(buffer.getMetalBuffer() != nullptr);
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

        buffer.clear(0);
        CHECK(readAll(buffer) == std::vector<U32>(WORDS, 0));
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
                            ALGPUBuffer::DYNAMIC, zeroes.data(),
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

    ALGPUBuffer moved(std::move(target));
    CHECK_FALSE(target.isAllocated());
    CHECK(moved.isAllocated());
    CHECK(readAll(moved) == expected);
    moved.release();
    CHECK_FALSE(moved.isAllocated());
    CHECK(ALGPUBuffer::sBytesAllocated ==
          bytes_before + source_words.size() * sizeof(U32));
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
    ALGPUBuffer stream;
    REQUIRE(stream.allocate(words.size() * sizeof(U32),
                            ALGPUBuffer::STREAM |
                                ALGPUBuffer::FLUSH_EXPLICIT,
                            nullptr, "native stream"));
    REQUIRE(stream.getMapped());
    std::memcpy(stream.getMapped(), words.data(), words.size() * sizeof(U32));
    stream.flushMapped(0, words.size() * sizeof(U32));
    CHECK(readAll(stream) == words);

    ALGPUBuffer readback;
    REQUIRE(readback.allocate(words.size() * sizeof(U32),
                              ALGPUBuffer::READBACK, words.data(),
                              "native readback"));
    REQUIRE(readback.getMapped());
    CHECK(readAll(readback) == words);
}

TEST_CASE("ALGPUStream refuses to overwrite a region used by the open Metal frame",
          "[algpubuffer][almetal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);
    MetalBufferDeviceScope device_scope(device.get());

    auto scheduler = ALMetalFrameScheduler::create(*device, 2, &error);
    INFO(error);
    REQUIRE(scheduler);
    ALFrameFences fences;
    fences.setMetalScheduler(scheduler.get());

    ALGPUStream stream(&fences);
    REQUIRE(stream.allocate(4 * sizeof(U32), 2, "native stream ring"));
    auto frame = scheduler->beginFrame("native stream ring frame", UINT32_MAX,
                                       &error);
    INFO(error);
    REQUIRE(frame);

    REQUIRE(stream.map());
    const size_t first = stream.publish(4 * sizeof(U32));
    REQUIRE(stream.map());
    const size_t second = stream.publish(4 * sizeof(U32));
    CHECK(first != second);

    // Both regions are now recorded as read by the still-open logical frame.
    // Reusing the first before its command buffer is committed must fail rather
    // than corrupt the data the GPU is about to consume.
    CHECK(stream.map() == nullptr);

    REQUIRE(scheduler->submit(*frame, &error));
    fences.endFrame();
    REQUIRE(stream.map());
    CHECK(stream.publish(4 * sizeof(U32)) == first);

    fences.release();
}
