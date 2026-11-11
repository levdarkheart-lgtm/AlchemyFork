/**
 * @file algputimer_metal_test.cpp
 * @brief Native isolated-command receipts used by avatar GPU profiling.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../algputimer.h"
#include "almetaldevice.h"
#include "almetalresource.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <utility>

namespace
{
constexpr U32 pattern = 0x13579bdfu;
constexpr std::size_t words = 64;

struct EncodedCopy
{
    std::unique_ptr<ALMetalCommandBuffer> command;
    std::unique_ptr<ALMetalBuffer> output;
};

EncodedCopy encodeCopy(ALMetalDevice& device, std::string& error)
{
    ALMetalBufferDescriptor bd;
    bd.length = words * sizeof(U32);
    bd.storage = ALMetalStorageMode::shared;
    bd.label = "Avatar timer native input";
    auto input = ALMetalBuffer::create(device, bd, &error); REQUIRE(input);
    bd.label = "Avatar timer native output";
    auto output = ALMetalBuffer::create(device, bd, &error); REQUIRE(output);
    std::array<U32, words> values;
    values.fill(pattern);
    REQUIRE(input->write(0, values.data(), sizeof(values), &error));
    values.fill(0);
    REQUIRE(output->write(0, values.data(), sizeof(values), &error));
    auto command = device.makeCommandBuffer("Avatar timer exact GPU copy", &error);
    REQUIRE(command);
    REQUIRE(output->copyFrom(*command, *input, 0, 0, bd.length, &error));
    // Native command retention owns the encoded input after this wrapper dies.
    return {std::move(command), std::move(output)};
}

void checkCopy(const ALMetalBuffer& output, std::string& error)
{
    std::array<U32, words> values{};
    REQUIRE(output.read(0, values.data(), sizeof(values), &error));
    for (const auto word : values) CHECK(word == pattern);
}
}

TEST_CASE("ALGPUTimer consumes one native command receipt exactly once",
          "[algputimer][almetal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
    ALGPUTimer timer;
    CHECK_FALSE(timer.isPending());
    U64 empty = 99;
    CHECK_FALSE(timer.poll(empty)); CHECK(empty == 0);
    CHECK_FALSE(timer.attachNativeCompletion({}));
    auto work = encodeCopy(*device, error);
    auto receipt = work.command->completionToken(); REQUIRE(receipt);
    REQUIRE(timer.attachNativeCompletion(receipt));
    REQUIRE(timer.isPending());
    CHECK_FALSE(timer.attachNativeCompletion(receipt));
    U64 ns = 99;
    const auto submissions = ALMetalDevice::committedCommandBufferCount();
    CHECK_FALSE(timer.poll(ns));
    CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);
    CHECK(ns == 0);
    CHECK(timer.isPending());
    REQUIRE(work.command->commit(&error));
    work.command.reset();
    REQUIRE(device->waitUntilIdle(&error)); // Test oracle/teardown only.
    ALMetalCommandGPUTime expected;
    REQUIRE(receipt->gpuTime(expected) == ALMetalCommandTimingStatus::ready);
    receipt.reset(); // The timer is now the caller's only ticket owner.
    REQUIRE(timer.poll(ns));
    CHECK(ns == expected.elapsed_nanoseconds);
    CHECK(ns > 0);
    CHECK_FALSE(timer.isPending());
    ns = 99;
    CHECK_FALSE(timer.poll(ns));
    CHECK(ns == 0);
    checkCopy(*work.output, error);
}

TEST_CASE("ALGPUTimer releases abandoned receipts and accepts the next native profile",
          "[algputimer][almetal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
    ALGPUTimer timer;
    auto abandoned = device->makeCommandBuffer("Abandoned avatar profile", &error);
    REQUIRE(abandoned);
    REQUIRE(timer.attachNativeCompletion(abandoned->completionToken()));
    abandoned.reset();
    U64 ns = 99;
    CHECK_FALSE(timer.poll(ns));
    CHECK(ns == 0); // Failure is not a successful zero-time profile.
    CHECK_FALSE(timer.isPending());
    auto work = encodeCopy(*device, error);
    auto receipt = work.command->completionToken(); REQUIRE(receipt);
    REQUIRE(work.command->commit(&error));
    // Match the production caller: attach only after successful commit.
    REQUIRE(timer.attachNativeCompletion(receipt));
    work.command.reset(); receipt.reset();
    REQUIRE(device->waitUntilIdle(&error));
    REQUIRE(timer.poll(ns));
    CHECK(ns > 0);
    CHECK_FALSE(timer.isPending());
    checkCopy(*work.output, error);
}

TEST_CASE("ALGPUTimer retains unread native intervals across independent profiles and reuses the owner",
          "[algputimer][almetal]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
    ALGPUTimer retained, independent;
    auto first = encodeCopy(*device, error);
    auto receipt = first.command->completionToken(); REQUIRE(receipt);
    REQUIRE(retained.attachNativeCompletion(receipt));
    REQUIRE(first.command->commitAndWait(&error));
    ALMetalCommandGPUTime expected;
    REQUIRE(receipt->gpuTime(expected) == ALMetalCommandTimingStatus::ready);
    first.command.reset(); receipt.reset();

    // The original query pool expired results after 64 frames. Native
    // command receipts own their result independently of later profiles.
    for (U32 profile = 0; profile < 65; ++profile)
    {
        auto other = encodeCopy(*device, error);
        auto other_receipt = other.command->completionToken(); REQUIRE(other_receipt);
        REQUIRE(independent.attachNativeCompletion(other_receipt));
        CHECK_FALSE(retained.attachNativeCompletion(other_receipt));
        REQUIRE(other.command->commitAndWait(&error));
        ALMetalCommandGPUTime other_expected;
        REQUIRE(other_receipt->gpuTime(other_expected) == ALMetalCommandTimingStatus::ready);
        other.command.reset(); other_receipt.reset();
        U64 ns = 99;
        REQUIRE(independent.poll(ns));
        CHECK(ns == other_expected.elapsed_nanoseconds);
        CHECK_FALSE(independent.isPending());
        CHECK(retained.isPending());
        CHECK_FALSE(independent.poll(ns));
        CHECK(ns == 0);
        checkCopy(*other.output, error);
    }
    U64 ns = 99;
    REQUIRE(retained.poll(ns));
    CHECK(ns == expected.elapsed_nanoseconds);
    CHECK_FALSE(retained.isPending());
    CHECK_FALSE(retained.poll(ns)); CHECK(ns == 0);
    checkCopy(*first.output, error);

    auto next = encodeCopy(*device, error);
    auto next_receipt = next.command->completionToken(); REQUIRE(next_receipt);
    REQUIRE(next.command->commitAndWait(&error));
    REQUIRE(next_receipt->gpuTime(expected) == ALMetalCommandTimingStatus::ready);
    REQUIRE(retained.attachNativeCompletion(next_receipt));
    next.command.reset(); next_receipt.reset();
    REQUIRE(retained.poll(ns));
    CHECK(ns == expected.elapsed_nanoseconds);
    CHECK_FALSE(retained.isPending());
    checkCopy(*next.output, error);
}
