/**
 * @file almetal_benchmark_test.mm
 * @brief Real GPU sample counts, pixels, timing and benchmark resource leases.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "../almetaltexturebenchmark.h"
#include "../almetalresource.h"
#include "../almetalstagetiming.h"
#include <catch2/catch_test_macros.hpp>
#import <Metal/Metal.h>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
#include <vector>

namespace
{
std::vector<std::uint8_t> sourcePixels()
{
    // A nonuniform RGBA pattern detects flipped rows, wrong channels, blending,
    // incomplete coverage and sampling from any uninitialized target.
    std::vector<std::uint8_t> pixels(ALMetalTextureBenchmark::source_bytes);
    std::uint32_t seed = 0x75e89134;
    for (auto& value : pixels)
    {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        value = static_cast<std::uint8_t>(seed >> 24);
    }
    return pixels;
}

void checkClear(const ALMetalTextureBenchmarkResult& result)
{
    CHECK(result.gpu_time.start_seconds == 0.0);
    CHECK(result.gpu_time.end_seconds == 0.0);
    CHECK(result.gpu_time.elapsed_nanoseconds == 0);
    CHECK(result.samples_drawn == 0);
    CHECK(result.sampled_bytes == 0);
    CHECK(result.gigabytes_per_second == 0.0);
}

void checkPixels(const ALMetalDevice& device, const ALMetalTextureBenchmarkRun& run,
                 const std::vector<std::uint8_t>& expected)
{
    REQUIRE(run.outputTexture());
    std::string error;
    std::vector<std::uint8_t> actual(expected.size());
    REQUIRE(run.outputTexture()->readback(device, 0, 0, {}, actual.data(),
        ALMetalTextureBenchmark::resolution * 4, &error));
    INFO(error);
    CHECK(actual == expected);
}
}

TEST_CASE("Developer benchmark measures actual complete fixed-workload samples and GPU time",
          "[almetal][developer-benchmark]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        REQUIRE_FALSE(ALMetalDevice::commandTimingEnabled());
        const auto aggregate = ALMetalDevice::commandTiming();
        const auto draws = ALMetalDevice::encodedDrawCallCount();
        const auto pixels = sourcePixels();
        auto benchmark = ALMetalTextureBenchmark::create(*device, pixels, &error);
        INFO(error); REQUIRE(benchmark);
        for (unsigned index = 0; index < ALMetalTextureBenchmark::run_count; ++index)
        {
            auto run = benchmark->encodeRun(&error); INFO(error); REQUIRE(run);
            ALMetalTextureBenchmarkResult result{{1.0, 2.0, 3}, 4, 5, 6.0};
            CHECK(run->poll(result, &error) == ALMetalTextureBenchmarkRun::Status::pending);
            checkClear(result);
            CHECK_FALSE(run->wait(result, &error));
            checkClear(result);
            CHECK_FALSE(benchmark->encodeRun(&error));
            const auto completion = run->completion(); REQUIRE(completion);
            REQUIRE(run->commit(&error));
            REQUIRE(run->wait(result, &error)); INFO(error);
            // 64 x 32 separate counting queries must accumulate to 2^31
            // actual passing samples, not an inferred viewport-area estimate.
            CHECK(result.samples_drawn == 2'147'483'648ULL);
            CHECK(result.sampled_bytes == 8'589'934'592ULL);
            CHECK(result.gpu_time.elapsed_nanoseconds > 0);
            ALMetalCommandGPUTime native_time;
            REQUIRE(completion->gpuTime(native_time) == ALMetalCommandTimingStatus::ready);
            CHECK(result.gpu_time.start_seconds == native_time.start_seconds);
            CHECK(result.gpu_time.end_seconds == native_time.end_seconds);
            CHECK(result.gpu_time.elapsed_nanoseconds == native_time.elapsed_nanoseconds);
            CHECK(std::isfinite(result.gigabytes_per_second));
            CHECK(result.gigabytes_per_second ==
                double(result.sampled_bytes) / native_time.elapsed_nanoseconds);
            checkPixels(*device, *run, pixels);
            std::cout << "Developer benchmark GPU receipt: " << device->info().name
                << ", run=" << index << ", passing_samples=" << result.samples_drawn
                << ", sampled_bytes=" << result.sampled_bytes
                << ", command_gpu_ns=" << result.gpu_time.elapsed_nanoseconds
                << ", exact_RGBA8_output=true\n";
            ALMetalTextureBenchmarkResult repeated;
            CHECK(run->poll(repeated, &error) == ALMetalTextureBenchmarkRun::Status::ready);
            CHECK(repeated.sampled_bytes == result.sampled_bytes);
            CHECK(repeated.gpu_time.elapsed_nanoseconds == result.gpu_time.elapsed_nanoseconds);
        }
        // The manual action needs no process-wide timing or draw collector.
        CHECK_FALSE(ALMetalDevice::commandTimingEnabled());
        CHECK(ALMetalDevice::commandTiming().submitted == aggregate.submitted);
        CHECK(ALMetalDevice::encodedDrawCallCount() == draws);
    }
}

TEST_CASE("Developer benchmark leases survive owner retirement and reject diagnostic conflicts",
          "[almetal][developer-benchmark][lifetime]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
    REQUIRE_FALSE(ALMetalDevice::commandTimingEnabled());
    const auto baseline = alMetalResourceAllocationStats();
    @autoreleasepool
    {
        const auto submissions = ALMetalDevice::committedCommandBufferCount();
        CHECK_FALSE(ALMetalTextureBenchmark::create(*device, {}, &error));
        CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);
        const auto pixels = sourcePixels();
        auto benchmark = ALMetalTextureBenchmark::create(*device, pixels, &error);
        INFO(error); REQUIRE(benchmark);
        CHECK(alMetalResourceAllocationStats().texture_bytes_live ==
            baseline.texture_bytes_live + 2 * ALMetalTextureBenchmark::texture_count * ALMetalTextureBenchmark::source_bytes);
        auto abandoned = benchmark->encodeRun(&error); REQUIRE(abandoned);
        auto abandoned_ticket = abandoned->completion(); REQUIRE(abandoned_ticket);
        const auto before_abandon = ALMetalDevice::committedCommandBufferCount();
        abandoned.reset();
        CHECK(abandoned_ticket->isComplete());
        CHECK(ALMetalDevice::committedCommandBufferCount() == before_abandon);
        auto run = benchmark->encodeRun(&error); INFO(error); REQUIRE(run);
        __weak id<MTLTexture> native_output =
            (__bridge id<MTLTexture>)run->outputTexture()->nativeHandle();
        benchmark.reset(); // The run has not even submitted its commands yet.
        REQUIRE(native_output != nil);

        struct RestoreTiming
        {
            ~RestoreTiming() { ALMetalDevice::setCommandTimingEnabled(false); }
        } restore;
        ALMetalDevice::setCommandTimingEnabled(true);
        CHECK_FALSE(run->commit(&error));
        CHECK_FALSE(ALMetalTextureBenchmark::create(*device, pixels, &error));
        CHECK(ALMetalDevice::commandTimingEnabled());
        CHECK(ALMetalDevice::committedCommandBufferCount() == before_abandon);
        ALMetalDevice::setCommandTimingEnabled(false);
        {
            auto window = ALMetalStageTimingWindow::create(*device, 1, &error);
            REQUIRE(window);
            ALMetalStageTimingScope scope(window.get(), 17, 1, -1);
            const auto* original_scope = ALMetalStageTimingScope::current();
            CHECK_FALSE(run->commit(&error));
            CHECK_FALSE(ALMetalTextureBenchmark::create(*device, pixels, &error));
            CHECK(ALMetalStageTimingScope::current() == original_scope);
            CHECK(window->preparedPassCount() == 0);
        }
        REQUIRE(run->commit(&error));
        ALMetalTextureBenchmarkResult result;
        REQUIRE(run->wait(result, &error)); INFO(error);
        CHECK(result.samples_drawn == 2'147'483'648ULL);
        checkPixels(*device, *run, pixels);
        run.reset();
    }
    // Completion handlers may release their command-owned payload after the
    // driver's completed status becomes observable. Check actual retirement
    // without imposing a synchronous production destructor.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((alMetalResourceAllocationStats().texture_bytes_live != baseline.texture_bytes_live ||
            alMetalResourceAllocationStats().buffer_bytes_live != baseline.buffer_bytes_live) &&
           std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK(alMetalResourceAllocationStats().texture_bytes_live == baseline.texture_bytes_live);
    CHECK(alMetalResourceAllocationStats().buffer_bytes_live == baseline.buffer_bytes_live);
}
