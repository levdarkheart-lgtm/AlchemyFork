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

#include <chrono>
#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <string>

class ALMetalBenchmarkRecorder
{
public:
    using Clock = std::chrono::steady_clock;

    struct Config
    {
        std::string output_path;
        std::string scenario;
        std::string variant;
        std::string source_sha;
        std::string build_type;
        std::uint32_t warmup_frames = 120;
        std::uint32_t sample_frames = 300;
        bool require_exact_native = false;
    };

    struct Frame
    {
        Clock::time_point timestamp;
        std::uint64_t frame_index = 0;
        std::uint32_t drawable_width = 0;
        std::uint32_t drawable_height = 0;
        std::uint32_t quality_profile = 0;
        std::uint32_t draw_calls = 0;
        bool native_exact = false;
        bool eligible = false;
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

    Config mConfig;
    std::unique_ptr<std::ofstream> mOutput;
    std::optional<Clock::time_point> mLastTimestamp;
    std::uint64_t mLastFrameIndex = 0;
    std::uint32_t mSequenceWidth = 0;
    std::uint32_t mSequenceHeight = 0;
    std::uint32_t mSequenceQuality = 0;
    std::uint32_t mWarmupSeen = 0;
    std::uint32_t mSamplesWritten = 0;
    bool mFinished = false;
    bool mFailed = false;
};

#endif
