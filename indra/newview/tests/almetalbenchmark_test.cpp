/**
 * @file almetalbenchmark_test.cpp
 * @brief Unit tests for privacy-bounded Metal benchmark evidence.
 *
 * Copyright (c) 2026, Alchemy Viewer Project.
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in
 * doc/LGPL-licence.txt in this distribution.
 */

#include "linden_common.h"

#include <catch2/catch_test_macros.hpp>

#include "../almetalbenchmark.h"

#include "llfile.h"
#include "lluuid.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace
{
class benchmark_file
{
public:
    benchmark_file()
    {
        mPath = std::filesystem::temp_directory_path() /
            ("almetalbenchmark_" + LLUUID::generateNewID().asString() +
             ".csv");
    }

    ~benchmark_file()
    {
        std::error_code ignored;
        std::filesystem::remove(mPath, ignored);
    }

    ALMetalBenchmarkRecorder::Config config(
        std::uint32_t warmup,
        std::uint32_t samples,
        bool require_exact = true) const
    {
        ALMetalBenchmarkRecorder::Config result;
        result.output_path = mPath.string();
        result.scenario = "quiet-region";
        result.variant = "candidate";
        result.source_sha = "0123456789abcdef0123456789abcdef01234567";
        result.build_type = "RelWithDebInfo";
        result.warmup_frames = warmup;
        result.sample_frames = samples;
        result.require_exact_native = require_exact;
        return result;
    }

    const std::filesystem::path& path() const { return mPath; }

private:
    std::filesystem::path mPath;
};

ALMetalBenchmarkRecorder::Frame frame(
    std::uint64_t index,
    std::int64_t milliseconds,
    bool exact = true)
{
    ALMetalBenchmarkRecorder::Frame result;
    result.timestamp = ALMetalBenchmarkRecorder::Clock::time_point(
        std::chrono::milliseconds(milliseconds));
    result.frame_index = index;
    result.drawable_width = 2048;
    result.drawable_height = 1280;
    result.quality_profile = 5;
    result.draw_calls = 321;
    result.metal_command_buffers_committed = index * 2;
    result.metal_timing_submitted = index * 2;
    result.metal_timing_completed = index * 2;
    result.metal_timing_valid = index * 2;
    result.metal_gpu_time_nanoseconds = index * 3'000'000;
    result.native_exact = exact;
    result.eligible = true;
    return result;
}

TEST_CASE("ALMetalBenchmark records only the requested consecutive post-warmup frames",
          "[ALMetalBenchmark]")
{
    benchmark_file output;
    ALMetalBenchmarkRecorder recorder(output.config(2, 3));
    std::string error;
    CHECK(recorder.record(frame(10, 100), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder.record(frame(11, 110), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder.record(frame(12, 120), &error) ==
          ALMetalBenchmarkRecorder::Result::recorded);
    CHECK(recorder.record(frame(13, 130), &error) ==
          ALMetalBenchmarkRecorder::Result::recorded);
    CHECK(recorder.record(frame(14, 140), &error) ==
          ALMetalBenchmarkRecorder::Result::complete);
    CHECK(error.empty());
    CHECK(recorder.samplesWritten() == 3);
    CHECK(recorder.finished());

    std::ifstream csv(output.path());
    REQUIRE(csv.good());
    std::string contents{
        std::istreambuf_iterator<char>(csv),
        std::istreambuf_iterator<char>()};
    CHECK(contents.find("scenario,variant,frame_ms") == 0);
    CHECK(contents.find("quiet-region,candidate,10.000000,12") !=
          std::string::npos);
    CHECK(contents.find(",2048,1280,5,321,2,2,2,3.000000,1\n") !=
          std::string::npos);
    CHECK(std::count(contents.begin(), contents.end(), '\n') == 4);
}

TEST_CASE("ALMetalBenchmark restarts warmup before sampling but rejects a broken evidence run",
          "[ALMetalBenchmark]")
{
    benchmark_file output;
    ALMetalBenchmarkRecorder recorder(output.config(1, 3));
    std::string error;
    CHECK(recorder.record(frame(20, 200), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder.record(frame(22, 220), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder.record(frame(23, 230), &error) ==
          ALMetalBenchmarkRecorder::Result::recorded);

    auto changed = frame(24, 240);
    changed.drawable_width = 1920;
    CHECK(recorder.record(changed, &error) ==
          ALMetalBenchmarkRecorder::Result::failed);
    CHECK(error.find("consecutive benchmark sequence") != std::string::npos);
    CHECK(recorder.samplesWritten() == 1);
    CHECK(recorder.finished());
}

TEST_CASE("ALMetalBenchmark exact candidate gating resets before evidence and never overwrites",
          "[ALMetalBenchmark]")
{
    benchmark_file output;
    ALMetalBenchmarkRecorder recorder(output.config(0, 1));
    std::string error;
    CHECK(recorder.record(frame(30, 300, false), &error) ==
          ALMetalBenchmarkRecorder::Result::inactive);
    CHECK(recorder.record(frame(31, 310), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder.record(frame(32, 320), &error) ==
          ALMetalBenchmarkRecorder::Result::complete);

    ALMetalBenchmarkRecorder second(output.config(0, 1));
    CHECK(second.record(frame(40, 400), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    CHECK(second.record(frame(41, 410), &error) ==
          ALMetalBenchmarkRecorder::Result::failed);
    CHECK((error.find("overwrite") != std::string::npos ||
           error.find("appeared") != std::string::npos));
}

TEST_CASE("ALMetalBenchmark rejects a command-buffer counter regression after sampling",
          "[ALMetalBenchmark]")
{
    benchmark_file output;
    ALMetalBenchmarkRecorder recorder(output.config(0, 2));
    std::string error;
    CHECK(recorder.record(frame(35, 350), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder.record(frame(36, 360), &error) ==
          ALMetalBenchmarkRecorder::Result::recorded);

    auto regressed = frame(37, 370);
    regressed.metal_command_buffers_committed = 1;
    CHECK(recorder.record(regressed, &error) ==
          ALMetalBenchmarkRecorder::Result::failed);
    CHECK(error.find("telemetry counter") != std::string::npos);
    CHECK(recorder.samplesWritten() == 1);
}

TEST_CASE("ALMetalBenchmark rejects invalid asynchronous completion telemetry",
          "[ALMetalBenchmark]")
{
    benchmark_file output;
    ALMetalBenchmarkRecorder recorder(output.config(0, 2));
    std::string error;
    CHECK(recorder.record(frame(40, 400), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);

    auto impossible = frame(41, 410);
    impossible.metal_timing_completed =
        impossible.metal_timing_submitted + 1;
    CHECK(recorder.record(impossible, &error) ==
          ALMetalBenchmarkRecorder::Result::inactive);
    CHECK(error.empty());

    auto valid = frame(42, 420);
    CHECK(recorder.record(valid, &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    valid = frame(43, 430);
    CHECK(recorder.record(valid, &error) ==
          ALMetalBenchmarkRecorder::Result::recorded);
    auto regressed = frame(44, 440);
    regressed.metal_gpu_time_nanoseconds =
        valid.metal_gpu_time_nanoseconds - 1;
    CHECK(recorder.record(regressed, &error) ==
          ALMetalBenchmarkRecorder::Result::failed);
    CHECK(error.find("telemetry counter") != std::string::npos);
}

TEST_CASE("ALMetalBenchmark rejects an invalid direct configuration",
          "[ALMetalBenchmark]")
{
    benchmark_file output;
    ALMetalBenchmarkRecorder recorder(output.config(0, 0));
    std::string error;
    CHECK(recorder.record(frame(50, 500), &error) ==
          ALMetalBenchmarkRecorder::Result::failed);
    CHECK(error.find("frame counts") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(output.path()));
}
}
