/** Private benchmark provenance is valid JSON and preserves the existing CSV. */
#include "linden_common.h"
#include <catch2/catch_test_macros.hpp>

#include "../almetalbenchmark.h"
#include "llsdjson.h"
#include "lluuid.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sys/stat.h>
#include <vector>

namespace
{
using Recorder = ALMetalBenchmarkRecorder;

struct EvidenceFiles
{
    std::filesystem::path csv = std::filesystem::temp_directory_path() /
        ("almetalprovenance_" + LLUUID::generateNewID().asString() + ".csv");
    std::filesystem::path sidecar = std::filesystem::path(csv).replace_extension(".provenance.jsonl");
    ~EvidenceFiles()
    {
        std::error_code ignored;
        std::filesystem::remove(csv, ignored);
        std::filesystem::remove(sidecar, ignored);
    }
    Recorder::Config config(U32 warmup, U32 samples) const
    {
        Recorder::Config c;
        c.output_path = csv.string();
        c.scenario = "synthetic-view";
        c.variant = "diagnostics";
        c.source_sha = "source-with-quote\"and-newline\n";
        c.build_type = "Release";
        c.warmup_frames = warmup;
        c.warmup_seconds = 0;
        c.sample_frames = samples;
        c.require_exact_native = true;
        c.collect_provenance = true;
        return c;
    }
};

Recorder::Frame frame(U64 index)
{
    Recorder::Frame f;
    f.timestamp = Recorder::Clock::time_point(std::chrono::milliseconds(index * 100));
    f.frame_index = index;
    f.drawable_width = 3456;
    f.drawable_height = 2168;
    f.quality_profile = 6;
    f.native_exact = true;
    f.eligible = true;
    f.native_presentation_state = Recorder::NativePresentationState::submitted;
    f.provenance.emplace();
    auto& p = *f.provenance;
    p.main_view_captured = true;
    p.main_view.main_world_pass = true;
    p.main_view.viewer_camera.origin = {1.23456788f, -15.25f, 999.f};
    p.main_view.viewer_camera.at = {1.f, 0.f, 0.f};
    p.main_view.viewer_camera.left = {0.f, 1.f, 0.f};
    p.main_view.viewer_camera.up = {0.f, 0.f, 1.f};
    p.main_view.viewer_camera.vertical_fov_radians = 1.23456788f;
    p.main_view.viewer_camera.aspect = 3456.f / 2168.f;
    p.main_view.viewer_camera.near_clip = 0.1f;
    p.main_view.viewer_camera.draw_far_clip = 256.f;
    p.main_view.projection_far_clip = 1048576.f;
    p.main_view.agent_camera_global = {1000000.123456789, 2000.25, 3000.5};
    p.main_view.viewport = {0, 0, 3456, 2168};
    p.main_view.occlusion_mode = 2;
    p.main_view.reverse_z = true;
    p.main_view.pass_current_camera = p.main_view.viewer_camera;
    p.frame_end_view = p.main_view;
    p.hero_restoration = {true, true, true, true, true};
    p.hud_restoration = p.hero_restoration;
    p.main_world_queries.epoch = 7;
    p.main_world_queries.acquired = index * 10;
    p.main_world_queries.ready_zero_unique = index;
    p.main_world_queries.duplicate_phase_polls = index * 2;
    return f;
}

std::vector<LLSD> read_json_lines(const std::filesystem::path& path)
{
    std::ifstream in(path);
    REQUIRE(in.good());
    std::vector<LLSD> lines;
    for (std::string line; std::getline(in, line);)
    {
        LLSD parsed;
        std::string error;
        INFO(error);
        REQUIRE(LlsdFromJsonString(line, parsed, &error));
        lines.push_back(std::move(parsed));
    }
    return lines;
}
}

TEST_CASE("Benchmark provenance records bounded checkpoints and round-trip camera data",
          "[ALMetalBenchmark][provenance]")
{
    EvidenceFiles output;
    Recorder recorder(output.config(2, 300));
    for (U64 i = 10; i <= 311; ++i)
    {
        const auto result = recorder.record(frame(i));
        CHECK(result != Recorder::Result::failed);
    }
    REQUIRE(recorder.finished());
    CHECK(recorder.samplesWritten() == 300);
    const auto lines = read_json_lines(output.sidecar);
    REQUIRE(lines.size() == 5);
    CHECK(lines[0]["event"].asString() == "warmup_start");
    CHECK(lines[1]["event"].asString() == "sample_start");
    CHECK(lines[2]["event"].asString() == "periodic");
    CHECK(lines[3]["event"].asString() == "periodic");
    CHECK(lines[4]["event"].asString() == "sample_end");
    CHECK(lines[4]["samples_written"].asInteger() == 300);
    CHECK(lines[4]["camera_change_frames"].asInteger() == 0);
    CHECK(lines[0]["source_sha"].asString() == output.config(0, 1).source_sha);
    const auto& camera = lines[0]["main_view"]["viewer_camera"];
    CHECK(static_cast<float>(camera["origin"][0].asReal()) == frame(10).provenance->main_view.viewer_camera.origin[0]);
    CHECK(static_cast<float>(camera["vertical_fov_radians"].asReal()) == frame(10).provenance->main_view.viewer_camera.vertical_fov_radians);
    CHECK(lines[0]["main_view"]["agent_camera_global"][0].asReal() == frame(10).provenance->main_view.agent_camera_global[0]);
    CHECK(lines[4]["main_world_queries"]["acquired"].asInteger() == 3110);
    CHECK(lines[4]["main_world_queries"]["ready_zero_unique"].asInteger() == 311);
    CHECK(lines[4]["main_world_queries"]["duplicate_phase_polls"].asInteger() == 622);

    struct stat permissions{};
    REQUIRE(::stat(output.sidecar.c_str(), &permissions) == 0);
    CHECK((permissions.st_mode & 0777) == 0600);
    std::ifstream csv(output.csv);
    std::string header;
    REQUIRE(std::getline(csv, header));
    CHECK(header == "scenario,variant,frame_ms,frame_index,source_sha,build_type,drawable_width,drawable_height,quality_profile,draw_calls,metal_submissions,metal_completed,metal_timed,metal_gpu_ms,native_requested,native_attempted,native_exact,gl_fallback,native_presentation_state");
}

TEST_CASE("Benchmark provenance waits for stable camera changes instead of writing every jitter",
          "[ALMetalBenchmark][provenance]")
{
    EvidenceFiles output;
    Recorder recorder(output.config(0, 8));
    for (U64 i = 1; i <= 9; ++i)
    {
        auto f = frame(i);
        if (i >= 3) f.provenance->main_view.viewer_camera.origin[0] += 1.f;
        if (i >= 5) f.provenance->main_view.viewer_camera.origin[0] += 1.f;
        if (i >= 6) f.provenance->main_view.viewer_camera.origin[0] += 1.f;
        REQUIRE(recorder.record(f) != Recorder::Result::failed);
    }
    const auto lines = read_json_lines(output.sidecar);
    REQUIRE(lines.size() == 5);
    CHECK(lines[2]["event"].asString() == "camera_change");
    CHECK(lines[2]["frame_index"].asInteger() == 4);
    CHECK(lines[3]["event"].asString() == "camera_change");
    CHECK(lines[3]["frame_index"].asInteger() == 7);
    CHECK(lines[4]["camera_change_frames"].asInteger() == 3);
}

TEST_CASE("Benchmark provenance fails closed for missing main views and existing evidence",
          "[ALMetalBenchmark][provenance]")
{
    EvidenceFiles output;
    {
        std::ofstream existing(output.sidecar);
        existing << "existing evidence\n";
    }
    Recorder recorder(output.config(0, 1));
    auto missing = frame(1);
    missing.provenance.reset();
    CHECK(recorder.record(missing) == Recorder::Result::inactive);
    std::string error;
    CHECK(recorder.record(frame(2), &error) == Recorder::Result::failed);
    CHECK_FALSE(error.empty());
    CHECK_FALSE(std::filesystem::exists(output.csv));
    std::ifstream existing(output.sidecar);
    std::string contents{std::istreambuf_iterator<char>(existing), {}};
    CHECK(contents == "existing evidence\n");
}

TEST_CASE("Benchmark provenance marks non-finite cameras without emitting invalid JSON numbers",
          "[ALMetalBenchmark][provenance]")
{
    EvidenceFiles output;
    Recorder recorder(output.config(0, 1));
    for (U64 i = 1; i <= 2; ++i)
    {
        auto f = frame(i);
        f.provenance->main_view.viewer_camera.modelview[0] = std::numeric_limits<float>::infinity();
        f.provenance->main_view.agent_camera_global[0] = std::numeric_limits<double>::quiet_NaN();
        REQUIRE(recorder.record(f) != Recorder::Result::failed);
    }
    const auto lines = read_json_lines(output.sidecar);
    REQUIRE(lines.size() == 3);
    CHECK_FALSE(lines[0]["main_view"]["finite_camera_values"].asBoolean());
    CHECK(lines[0]["main_view"]["viewer_camera"]["modelview"][0].isUndefined());
    CHECK(lines[0]["main_view"]["agent_camera_global"][0].isUndefined());
}
