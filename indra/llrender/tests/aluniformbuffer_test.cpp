/**
 * @file aluniformbuffer_test.cpp
 * @brief Native Metal constant-buffer storage, ranges and ring tests.
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
#include "aluniformbuffer.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace
{
    std::vector<U32> pattern(U32 words, U32 base)
    {
        std::vector<U32> result(words);
        for (U32 index = 0; index < words; ++index)
        {
            result[index] = base + index * 7u;
        }
        return result;
    }

    std::vector<U32> read_binding(U32 binding)
    {
        const ALUniformBuffer::Binding current =
            ALUniformBuffer::currentBinding(binding);
        REQUIRE(current);
        REQUIRE(current.mOffset % 256 == 0);
        std::vector<U32> result(current.mSize / sizeof(U32));
        REQUIRE(current.mStore->read(current.mOffset, result.data(),
                                     current.mSize));
        return result;
    }

    class NativeUniformScope
    {
    public:
        explicit NativeUniformScope(ALMetalDevice* device)
            : mPrevious(ALGPUBuffer::getMetalDevice())
        {
            ALUniformBuffer::cleanupClass();
            ALGPUBuffer::setMetalDevice(device);
        }

        ~NativeUniformScope()
        {
            ALUniformBuffer::cleanupClass();
            ALGPUBuffer::setMetalDevice(mPrevious);
        }

    private:
        ALMetalDevice* mPrevious = nullptr;
    };
}

TEST_CASE("ALUniformBuffer publishes native whole and ranged bindings",
          "[aluniformbuffer][almetal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);
    NativeUniformScope scope(device.get());

    constexpr U32 whole_binding = 3;
    constexpr U32 range_binding = 4;
    const std::vector<U32> words = pattern(128, 50);

    ALUniformBuffer block;
    block.update(words.data(), words.size() * sizeof(U32));
    REQUIRE(block.allocated());
    REQUIRE(block.getMetalBuffer());
    block.bind(whole_binding);
    CHECK(read_binding(whole_binding) == words);

    block.bindRange(range_binding, 64 * sizeof(U32), 12 * sizeof(U32));
    const ALUniformBuffer::Binding range =
        ALUniformBuffer::currentBinding(range_binding);
    REQUIRE(range);
    CHECK(range.mOffset == 64 * sizeof(U32));
    CHECK(range.mSize == 12 * sizeof(U32));
    std::vector<U32> range_words(12);
    REQUIRE(range.mStore->read(range.mOffset, range_words.data(),
                               range.mSize));
    CHECK(range_words == std::vector<U32>(words.begin() + 64,
                                          words.begin() + 76));

    ALUniformBuffer moved(std::move(block));
    CHECK_FALSE(block.allocated());
    CHECK(ALUniformBuffer::currentBinding(whole_binding));
    CHECK(read_binding(whole_binding) == words);

    moved.release();
    CHECK_FALSE(ALUniformBuffer::currentBinding(whole_binding));
    CHECK_FALSE(ALUniformBuffer::currentBinding(range_binding));
}

TEST_CASE("ALUniformBuffer streams shadowed constants through native ring slices",
          "[aluniformbuffer][almetal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);
    NativeUniformScope scope(device.get());

    constexpr U32 binding = 7;
    constexpr U32 word_count = 92;
    ALUniformBuffer block;
    block.initShadowed(word_count * sizeof(U32));

    const std::vector<U32> first = pattern(word_count, 1000);
    std::memcpy(block.beginWrite(), first.data(), first.size() * sizeof(U32));
    block.endWrite(0, first.size() * sizeof(U32));
    REQUIRE(block.dirty());
    block.flush(binding);
    CHECK_FALSE(block.dirty());
    const ALUniformBuffer::Binding first_binding =
        ALUniformBuffer::currentBinding(binding);
    REQUIRE(first_binding);
    CHECK(first_binding.mStore != nullptr);
    CHECK(first_binding.mStore->getMapped() != nullptr);
    CHECK(read_binding(binding) == first);

    const std::vector<U32> second = pattern(word_count, 9000);
    std::memcpy(block.beginWrite(), second.data(), second.size() * sizeof(U32));
    block.endWrite(0, second.size() * sizeof(U32));
    block.ensureCurrent(binding);
    const ALUniformBuffer::Binding second_binding =
        ALUniformBuffer::currentBinding(binding);
    REQUIRE(second_binding);
    CHECK(second_binding.mStore == first_binding.mStore);
    CHECK(second_binding.mOffset >=
          first_binding.mOffset + first_binding.mSize);
    CHECK(read_binding(binding) == second);

    std::vector<U32> retained(word_count);
    REQUIRE(first_binding.mStore->read(first_binding.mOffset,
                                       retained.data(),
                                       first_binding.mSize));
    CHECK(retained == first);
}

TEST_CASE("ALUniformBuffer scratch payloads retain native ranged state",
          "[aluniformbuffer][almetal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);
    NativeUniformScope scope(device.get());

    constexpr U32 binding = 12;
    const std::vector<U32> first = pattern(16, 25);
    const S64 first_offset = ALUniformBuffer::scratchWrite(
        first.data(), first.size() * sizeof(U32));
    REQUIRE(first_offset >= 0);
    ALUniformBuffer::scratchBindRange(
        binding, first_offset, first.size() * sizeof(U32));
    const ALUniformBuffer::Binding first_binding =
        ALUniformBuffer::currentBinding(binding);
    REQUIRE(first_binding);
    CHECK(first_binding.mOffset == static_cast<size_t>(first_offset));
    CHECK(read_binding(binding) == first);

    const U64 serial = first_binding.mSerial;
    ALUniformBuffer::scratchBindRange(
        binding, first_offset, first.size() * sizeof(U32));
    CHECK(ALUniformBuffer::currentBinding(binding).mSerial == serial);

    const std::vector<U32> second = pattern(16, 700);
    const S64 second_offset = ALUniformBuffer::scratchWrite(
        second.data(), second.size() * sizeof(U32));
    REQUIRE(second_offset > first_offset);
    ALUniformBuffer::scratchBindRange(
        binding, second_offset, second.size() * sizeof(U32));
    CHECK(read_binding(binding) == second);
}
