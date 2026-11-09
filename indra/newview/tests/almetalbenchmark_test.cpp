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
#include "llsdjson.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdlib>

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
        auto sidecar = mPath;
        sidecar.replace_extension(".provenance.jsonl");
        std::filesystem::remove(sidecar, ignored);
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
        result.warmup_seconds = 0;
        result.sample_frames = samples;
        result.require_exact_native = require_exact;
        return result;
    }

    const std::filesystem::path& path() const { return mPath; }
    std::filesystem::path sidecar() const
    { auto result = mPath; return result.replace_extension(".provenance.jsonl"); }

private:
    std::filesystem::path mPath;
};

class environment_override
{
public:
    environment_override(const char* name, const char* value) : mName(name)
    {
        if (const char* previous = std::getenv(name)) mPrevious = previous;
        if (value) ::setenv(name, value, 1); else ::unsetenv(name);
    }
    ~environment_override()
    {
        if (mPrevious) ::setenv(mName, mPrevious->c_str(), 1); else ::unsetenv(mName);
    }
private:
    const char* mName;
    std::optional<std::string> mPrevious;
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
    result.native_presentation_state = exact
        ? ALMetalBenchmarkRecorder::NativePresentationState::submitted
        : ALMetalBenchmarkRecorder::NativePresentationState::not_attempted;
    result.native_exact = exact;
    result.gl_fallback = !exact;
    result.eligible = true;
    return result;
}

ALMetalBenchmarkRecorder::Frame ready_frame(std::uint64_t index, std::int64_t milliseconds)
{
    auto result = frame(index, milliseconds);
    result.provenance.emplace();
    result.provenance->main_view_captured = true;
    result.provenance->main_view.main_world_pass = true;
    auto& s = result.probe_readiness.emplace();
    s.frame_index = index;
    s.enabled = s.layout_ready = s.default_ready = s.native_pairs_required = true;
    s.budget = s.dynamic_count = s.assigned = s.member_count = s.main_visible = 2;
    s.resolution = 128;
    s.members[0] = {1234567894, 0, 19, true};
    s.members[1] = {1234567895, 1, 23, true};
    return result;
}

ALMetalBenchmarkRecorder::Config ready_config(const benchmark_file& output, unsigned samples = 2)
{
    auto result = output.config(0, samples);
    result.collect_provenance = result.require_ready_probes = true;
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
    CHECK(contents.find(",2048,1280,5,321,2,2,2,3.000000,1,1,1,0,"
                        "submitted\n") !=
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

TEST_CASE("ALMetalBenchmark requires elapsed warmup time as well as frames",
          "[ALMetalBenchmark]")
{
    benchmark_file output;
    auto config = output.config(2, 1);
    config.warmup_seconds = 30;
    ALMetalBenchmarkRecorder recorder(std::move(config));
    std::string error;
    CHECK(recorder.record(frame(100, 1'000), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder.record(frame(101, 1'010), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder.record(frame(102, 1'020), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder.record(frame(103, 30'999), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    CHECK_FALSE(std::filesystem::exists(output.path()));
    CHECK(recorder.record(frame(104, 31'000), &error) ==
          ALMetalBenchmarkRecorder::Result::complete);
    CHECK(error.empty());

    std::ifstream csv(output.path());
    REQUIRE(csv.good());
    std::string contents{
        std::istreambuf_iterator<char>(csv),
        std::istreambuf_iterator<char>()};
    CHECK(contents.find("quiet-region,candidate,1.000000,104") !=
          std::string::npos);
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

TEST_CASE("ALMetalBenchmark rejects inconsistent presentation telemetry",
          "[ALMetalBenchmark]")
{
    benchmark_file output;
    ALMetalBenchmarkRecorder recorder(output.config(0, 2, false));
    std::string error;
    CHECK(recorder.record(frame(60, 600), &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder.record(frame(61, 610), &error) ==
          ALMetalBenchmarkRecorder::Result::recorded);

    auto inconsistent = frame(62, 620);
    inconsistent.native_presentation_state =
        ALMetalBenchmarkRecorder::NativePresentationState::capture_failed;
    CHECK(recorder.record(inconsistent, &error) ==
          ALMetalBenchmarkRecorder::Result::failed);
    CHECK(error.find("presentation telemetry") != std::string::npos);
    CHECK(recorder.samplesWritten() == 1);
}

TEST_CASE("ALMetalBenchmark accepts an unpublished native-only frame without GL fallback",
          "[ALMetalBenchmark]")
{
    benchmark_file output;
    ALMetalBenchmarkRecorder recorder(output.config(0, 1, false));
    auto native_only = frame(70, 700, false);
    native_only.native_presentation_state =
        ALMetalBenchmarkRecorder::NativePresentationState::capture_failed;
    native_only.gl_fallback = false;
    std::string error;
    CHECK(recorder.record(native_only, &error) ==
          ALMetalBenchmarkRecorder::Result::warming);
    native_only.frame_index = 71;
    native_only.timestamp = ALMetalBenchmarkRecorder::Clock::time_point(
        std::chrono::milliseconds(710));
    native_only.metal_command_buffers_committed += 2;
    native_only.metal_timing_submitted += 2;
    native_only.metal_timing_completed += 2;
    native_only.metal_timing_valid += 2;
    native_only.metal_gpu_time_nanoseconds += 6'000'000;
    CHECK(recorder.record(native_only, &error) ==
          ALMetalBenchmarkRecorder::Result::complete);
    CHECK(error.empty());
}

TEST_CASE("Probe-ready benchmarks run timed warmup in parallel with initialization", "[ALMetalBenchmark]")
{
    benchmark_file output;
    auto config = ready_config(output);
    config.warmup_frames = 120;
    config.warmup_seconds = 300;
    ALMetalBenchmarkRecorder recorder(config);
    REQUIRE(recorder.requiresReadyProbes());
    std::string error;
    for (std::uint64_t i = 1; i < 320; ++i)
    {
        auto f = ready_frame(i, i * 1000);
        if (i <= 200) f.probe_readiness->incomplete = 1;
        // Initial membership progress must not restart the 300-second clock.
        if (i <= 150) f.probe_readiness->members[1].owner = 77;
        CHECK(recorder.record(f, &error) == ALMetalBenchmarkRecorder::Result::warming);
    }
    CHECK_FALSE(std::filesystem::exists(output.path()));
    CHECK(recorder.record(ready_frame(320, 320000), &error) == ALMetalBenchmarkRecorder::Result::recorded);
    auto refreshed = ready_frame(321, 321000);
    refreshed.probe_readiness->refreshing = refreshed.probe_readiness->radiance_pass = true;
    refreshed.probe_readiness->updating_face = 5;
    CHECK(recorder.record(refreshed, &error) == ALMetalBenchmarkRecorder::Result::complete);
    CHECK(error.empty());
    std::ifstream sidecar(output.sidecar());
    std::string line;
    bool found_start = false;
    while (std::getline(sidecar, line))
    {
        LLSD parsed;
        REQUIRE(LlsdFromJsonString(line, parsed, &error));
        CHECK(line.find("1234567894") == std::string::npos);
        CHECK(line.find("1234567895") == std::string::npos);
        CHECK_FALSE(parsed["probe_readiness"].has("members"));
        if (parsed["event"].asString() == "sample_start")
        {
            found_start = true;
            CHECK(parsed["frame_index"].asInteger() == 320);
            CHECK(parsed["probe_readiness"]["required"].asBoolean());
            CHECK(parsed["probe_readiness"]["admitted"].asBoolean());
            CHECK(parsed["probe_readiness"]["consecutive_ready_frames"].asInteger() == 120);
            CHECK(parsed["probe_readiness"]["assigned"].asInteger() == 2);
        }
    }
    CHECK(found_start);
}

TEST_CASE("Probe-ready samples fail on missing stale changing or unready populations", "[ALMetalBenchmark]")
{
    for (unsigned variant = 0; variant < 8; ++variant)
    {
        benchmark_file output;
        ALMetalBenchmarkRecorder recorder(ready_config(output));
        std::string error;
        for (std::uint64_t i = 1; i <= 120; ++i)
        {
            const auto result = recorder.record(ready_frame(i, i * 10), &error);
            REQUIRE(result == (i == 120 ? ALMetalBenchmarkRecorder::Result::recorded
                                       : ALMetalBenchmarkRecorder::Result::warming));
        }
        auto changed = ready_frame(121, 1210);
        switch (variant)
        {
        case 0: changed.probe_readiness.reset(); break;
        case 1: --changed.probe_readiness->frame_index; break;
        case 2: changed.probe_readiness->queued_create = 1; break;
        case 3: changed.probe_readiness->fading = 1; break;
        case 4: ++changed.probe_readiness->members[1].generation; break;
        case 5: changed.probe_readiness->members[1].owner = 555; break;
        case 6:
            changed.probe_readiness->members[1].main_visible = false;
            --changed.probe_readiness->main_visible;
            break;
        case 7: changed.probe_readiness->unpublished_pairs = 1; break;
        }
        INFO("loss " << variant);
        CHECK(recorder.record(changed, &error) == ALMetalBenchmarkRecorder::Result::failed);
        CHECK(recorder.samplesWritten() == 1);
        CHECK(recorder.finished());
        CHECK(error.find("probe population") != std::string::npos);
    }
}

TEST_CASE("Probe-ready warmup resets on main-view discontinuities", "[ALMetalBenchmark]")
{
    for (unsigned variant = 0; variant < 3; ++variant)
    {
        benchmark_file output;
        ALMetalBenchmarkRecorder recorder(ready_config(output, 1));
        std::string error;
        for (std::uint64_t i = 1; i <= 119; ++i)
            REQUIRE(recorder.record(ready_frame(i, i * 10), &error) == ALMetalBenchmarkRecorder::Result::warming);
        auto changed = ready_frame(120, 1200);
        switch (variant)
        {
        case 0: changed.eligible = false; break;
        case 1: changed.frame_index = changed.probe_readiness->frame_index = 121; break;
        case 2: changed.provenance->main_view.viewer_camera.origin[0] = 1.f; break;
        }
        const auto result = recorder.record(changed, &error);
        CHECK((result == ALMetalBenchmarkRecorder::Result::warming || result == ALMetalBenchmarkRecorder::Result::inactive));
        CHECK(recorder.samplesWritten() == 0);
        CHECK_FALSE(std::filesystem::exists(output.path()));
    }
}

TEST_CASE("Display observations reject early returns and keep their recorder alive", "[ALMetalBenchmark]")
{
    benchmark_file output;
    auto config = output.config(0, 2);
    config.warmup_seconds = 300;
    auto recorder = std::make_shared<ALMetalBenchmarkRecorder>(config);
    std::string error;
    CHECK(recorder->record(frame(1, 0), &error) == ALMetalBenchmarkRecorder::Result::warming);
    { ALMetalBenchmarkRecorder::DisplayObservation early(recorder); }
    CHECK(recorder->record(frame(2, 400000), &error) == ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder->record(frame(3, 400010), &error) == ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder->record(frame(4, 700000), &error) == ALMetalBenchmarkRecorder::Result::recorded);
    std::weak_ptr<ALMetalBenchmarkRecorder> weak = recorder;
    {
        ALMetalBenchmarkRecorder::DisplayObservation early(recorder);
        recorder.reset();
        REQUIRE_FALSE(weak.expired());
        CHECK_FALSE(weak.lock()->finished());
    }
    CHECK(weak.expired());
    // Observe the terminal result while another owner survives.
    benchmark_file second_output;
    recorder = std::make_shared<ALMetalBenchmarkRecorder>(second_output.config(0, 2));
    recorder->record(frame(1, 10));
    recorder->record(frame(2, 20));
    { ALMetalBenchmarkRecorder::DisplayObservation early(recorder); }
    CHECK(recorder->finished());
    CHECK(recorder->record(frame(3, 30)) == ALMetalBenchmarkRecorder::Result::failed);
    ALMetalBenchmarkRecorder::DisplayObservation null(nullptr);
    CHECK(null.record(frame(1, 10)) == ALMetalBenchmarkRecorder::Result::inactive);
}

TEST_CASE("Probe readiness is default-off and cannot suppress its provenance", "[ALMetalBenchmark]")
{
    benchmark_file output;
    auto config = output.config(0, 1);
    ALMetalBenchmarkRecorder recorder(config);
    CHECK_FALSE(recorder.requiresReadyProbes());
    CHECK(recorder.record(frame(1, 10)) == ALMetalBenchmarkRecorder::Result::warming);
    CHECK(recorder.record(frame(2, 20)) == ALMetalBenchmarkRecorder::Result::complete);
    benchmark_file invalid_output;
    config = ready_config(invalid_output);
    config.collect_provenance = false;
    ALMetalBenchmarkRecorder invalid(config);
    CHECK(invalid.record(ready_frame(1, 10)) == ALMetalBenchmarkRecorder::Result::failed);
    CHECK_FALSE(std::filesystem::exists(invalid_output.path()));
}

TEST_CASE("Probe readiness requires an explicit bounded environment flag", "[ALMetalBenchmark]")
{
    benchmark_file output;
    environment_override csv("ALCHEMY_METAL_BENCHMARK_CSV", output.path().c_str());
    environment_override scenario("ALCHEMY_METAL_BENCHMARK_SCENARIO", "unit");
    environment_override variant("ALCHEMY_METAL_BENCHMARK_VARIANT", "candidate");
    environment_override provenance("ALCHEMY_METAL_BENCHMARK_PROVENANCE", "1");
    environment_override ready("ALCHEMY_METAL_BENCHMARK_REQUIRE_READY_PROBES", nullptr);
    std::string error;
    auto config = ALMetalBenchmarkRecorder::configFromEnvironment("source", "Release", &error);
    REQUIRE(config);
    CHECK_FALSE(config->require_ready_probes);
    {
        environment_override enabled("ALCHEMY_METAL_BENCHMARK_REQUIRE_READY_PROBES", "1");
        config = ALMetalBenchmarkRecorder::configFromEnvironment("source", "Release", &error);
        REQUIRE(config);
        CHECK(config->require_ready_probes);
        environment_override disabled_provenance("ALCHEMY_METAL_BENCHMARK_PROVENANCE", "0");
        CHECK_FALSE(ALMetalBenchmarkRecorder::configFromEnvironment("source", "Release", &error));
        CHECK(error.find("requires benchmark provenance") != std::string::npos);
    }
    {
        environment_override malformed("ALCHEMY_METAL_BENCHMARK_REQUIRE_READY_PROBES", "yes");
        CHECK_FALSE(ALMetalBenchmarkRecorder::configFromEnvironment("source", "Release", &error));
        CHECK(error.find("must be 0 or 1") != std::string::npos);
    }
    environment_override no_csv("ALCHEMY_METAL_BENCHMARK_CSV", nullptr);
    environment_override standalone("ALCHEMY_METAL_BENCHMARK_REQUIRE_READY_PROBES", "1");
    CHECK_FALSE(ALMetalBenchmarkRecorder::configFromEnvironment("source", "Release", &error));
}
}
