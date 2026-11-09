/**
 * @file almetalbenchmark.h
 * @brief Explicit, privacy-bounded per-frame Metal benchmark evidence.
 *
 * Copyright (c) 2026, Alchemy Viewer Project.
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in
 * doc/LGPL-licence.txt in this distribution.
 */

#ifndef AL_ALMETALBENCHMARK_H
#define AL_ALMETALBENCHMARK_H

#include "alquerypool.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <string>

class ALMetalBenchmarkRecorder
{
public:
    using Clock = std::chrono::steady_clock;

    enum class NativePresentationState : std::uint8_t
    {
        disabled,
        not_attempted,
        capture_started,
        capture_failed,
        surface_unavailable,
        surface_submit_failed,
        submitted
    };

    struct Config
    {
        std::string output_path;
        std::string scenario;
        std::string variant;
        std::string source_sha;
        std::string build_type;
        std::uint32_t warmup_frames = 120;
        std::uint32_t warmup_seconds = 30;
        std::uint32_t sample_frames = 300;
        bool require_exact_native = false;
        // Environment-based collection enables this only after an explicit
        // benchmark request. The sidecar is local, mode 0600, and never sent.
        bool collect_provenance = false;
    };

    struct Camera
    {
        std::array<float, 3> origin{}, at{}, left{}, up{};
        std::array<float, 16> modelview{}, projection{};
        float vertical_fov_radians = 0.f;
        float aspect = 0.f;
        float near_clip = 0.f;
        float draw_far_clip = 0.f;
        bool operator==(const Camera&) const = default;
    };

    struct ViewState
    {
        Camera viewer_camera;
        Camera pass_current_camera;
        std::array<double, 3> agent_camera_global{}, agent_focus_global{}, agent_focus_target_global{};
        std::array<float, 3> agent_camera_up{};
        std::array<std::int32_t, 4> viewport{};
        float projection_far_clip = 0.f;
        float requested_draw_distance = 0.f;
        std::uint32_t camera_id = 0;
        std::uint32_t agent_camera_mode = 0;
        std::uint32_t agent_camera_preset = 0;
        std::uint32_t occlusion_mode = 0;
        bool reverse_z = false;
        bool main_world_pass = false;
        bool operator==(const ViewState&) const = default;
    };

    struct Restoration
    {
        bool checked = false;
        bool viewer_camera_restored = false;
        bool pass_current_camera_restored = false;
        bool agent_camera_fields_restored = false;
        bool camera_id_restored = false;
        bool operator==(const Restoration&) const = default;
    };

    struct Provenance
    {
        // Captured immediately before main-world culling, separately from the
        // state at the end of UI/HUD rendering. No account or asset identities.
        bool main_view_captured = false;
        ViewState main_view;
        ViewState frame_end_view;
        Restoration hero_restoration;
        Restoration hud_restoration;
        ALQueryPool::Diagnostics main_world_queries;
    };

    struct Frame
    {
        Clock::time_point timestamp;
        std::uint64_t frame_index = 0;
        std::uint32_t drawable_width = 0;
        std::uint32_t drawable_height = 0;
        std::uint32_t quality_profile = 0;
        std::uint32_t draw_calls = 0;
        std::uint64_t metal_command_buffers_committed = 0;
        std::uint64_t metal_timing_submitted = 0;
        std::uint64_t metal_timing_completed = 0;
        std::uint64_t metal_timing_valid = 0;
        std::uint64_t metal_gpu_time_nanoseconds = 0;
        NativePresentationState native_presentation_state =
            NativePresentationState::disabled;
        bool native_exact = false;
        bool gl_fallback = false;
        bool eligible = false;
        std::optional<Provenance> provenance;
    };

    enum class Result
    {
        inactive,
        warming,
        recorded,
        complete,
        failed,
    };

    // No path means benchmarking was not requested. A supplied path makes
    // every other field mandatory and validates it without reading viewer or
    // account state. The path must be absolute and name a new .csv file.
    static std::optional<Config> configFromEnvironment(
        const std::string& source_sha,
        const std::string& build_type,
        std::string* error = nullptr);

    explicit ALMetalBenchmarkRecorder(Config config);
    ~ALMetalBenchmarkRecorder();

    ALMetalBenchmarkRecorder(const ALMetalBenchmarkRecorder&) = delete;
    ALMetalBenchmarkRecorder& operator=(
        const ALMetalBenchmarkRecorder&) = delete;

    Result record(const Frame& frame, std::string* error = nullptr);
    std::uint32_t samplesWritten() const noexcept;
    bool finished() const noexcept;

private:
    void resetSequence() noexcept;
    Result fail(const std::string& message, std::string* error);
    bool openOutput(std::string* error);
    bool recordProvenance(const Frame& frame, const char* boundary, std::string* error);
    bool closeProvenance() noexcept;

    Config mConfig;
    std::unique_ptr<std::ofstream> mOutput;
    std::FILE* mProvenanceOutput = nullptr;
    std::optional<Provenance> mLastObservedProvenance;
    std::optional<Provenance> mLastWrittenProvenance;
    std::uint64_t mLastProvenanceFrame = 0;
    std::uint64_t mLastObservedProvenanceFrame = 0;
    std::uint64_t mCameraChangeFrames = 0;
    std::uint32_t mStableCameraFrames = 0;
    std::optional<Clock::time_point> mSequenceStartTimestamp;
    std::optional<Clock::time_point> mLastTimestamp;
    std::uint64_t mLastFrameIndex = 0;
    std::uint64_t mLastMetalCommandBuffers = 0;
    std::uint64_t mLastMetalTimingSubmitted = 0;
    std::uint64_t mLastMetalTimingCompleted = 0;
    std::uint64_t mLastMetalTimingValid = 0;
    std::uint64_t mLastMetalGPUTimeNanoseconds = 0;
    std::uint32_t mSequenceWidth = 0;
    std::uint32_t mSequenceHeight = 0;
    std::uint32_t mSequenceQuality = 0;
    std::uint32_t mWarmupSeen = 0;
    std::uint32_t mSamplesWritten = 0;
    bool mFinished = false;
    bool mFailed = false;
};

#endif
