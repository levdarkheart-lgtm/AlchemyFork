/**
 * @file almetalbenchmark.cpp
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

#include "llviewerprecompiledheaders.h"

#include "almetalbenchmark.h"

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <system_error>

namespace
{
constexpr std::uint32_t MAX_FRAME_COUNT = 1'000'000;
constexpr std::uint32_t MAX_WARMUP_SECONDS = 3'600;

void set_error(std::string* error, const std::string& message)
{
    if (error)
    {
        *error = message;
    }
}

const char* environment(const char* name)
{
    const char* value = std::getenv(name);
    return value && *value ? value : nullptr;
}

bool safe_label(const std::string& value)
{
    if (value.empty() || value.size() > 64)
    {
        return false;
    }
    for (const unsigned char character : value)
    {
        if (!(std::isalnum(character) || character == '-' ||
              character == '_' || character == '.'))
        {
            return false;
        }
    }
    return true;
}

bool unsigned_environment(
    const char* name,
    std::uint32_t fallback,
    bool allow_zero,
    std::uint32_t maximum,
    const char* unit,
    std::uint32_t& value,
    std::string* error)
{
    const char* text = environment(name);
    if (!text)
    {
        value = fallback;
        return true;
    }
    errno = 0;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed > maximum || (!allow_zero && parsed == 0))
    {
        set_error(error, std::string(name) +
            " must be a bounded unsigned " + unit);
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool bool_environment(
    const char* name,
    bool fallback,
    bool& value,
    std::string* error)
{
    const char* text = environment(name);
    if (!text)
    {
        value = fallback;
        return true;
    }
    if (std::string(text) == "0")
    {
        value = false;
        return true;
    }
    if (std::string(text) == "1")
    {
        value = true;
        return true;
    }
    set_error(error, std::string(name) + " must be 0 or 1");
    return false;
}

std::string csv_field(const std::string& value)
{
    if (value.find_first_of(",\"\r\n") == std::string::npos)
    {
        return value;
    }
    std::string quoted;
    quoted.reserve(value.size() + 2);
    quoted.push_back('"');
    for (const char character : value)
    {
        if (character == '"')
        {
            quoted.push_back('"');
        }
        quoted.push_back(character);
    }
    quoted.push_back('"');
    return quoted;
}

const char* presentation_state_name(
    ALMetalBenchmarkRecorder::NativePresentationState state)
{
    using State = ALMetalBenchmarkRecorder::NativePresentationState;
    switch (state)
    {
        case State::disabled: return "disabled";
        case State::not_attempted: return "not_attempted";
        case State::capture_started: return "capture_started";
        case State::capture_failed: return "capture_failed";
        case State::surface_unavailable: return "surface_unavailable";
        case State::surface_submit_failed: return "surface_submit_failed";
        case State::submitted: return "submitted";
    }
    return nullptr;
}
}

std::optional<ALMetalBenchmarkRecorder::Config>
ALMetalBenchmarkRecorder::configFromEnvironment(
    const std::string& source_sha,
    const std::string& build_type,
    std::string* error)
{
    const char* output = environment("ALCHEMY_METAL_BENCHMARK_CSV");
    if (!output)
    {
        return std::nullopt;
    }
    const char* scenario = environment("ALCHEMY_METAL_BENCHMARK_SCENARIO");
    const char* variant = environment("ALCHEMY_METAL_BENCHMARK_VARIANT");
    if (!scenario || !variant || !safe_label(scenario) ||
        !safe_label(variant))
    {
        set_error(error,
            "ALCHEMY_METAL_BENCHMARK_SCENARIO and _VARIANT must be "
            "1-64 character pseudonymous labels using letters, numbers, "
            "dot, dash or underscore");
        return std::nullopt;
    }

    const std::filesystem::path path(output);
    if (!path.is_absolute() || path.extension() != ".csv")
    {
        set_error(error,
            "ALCHEMY_METAL_BENCHMARK_CSV must be an absolute .csv path");
        return std::nullopt;
    }
    std::error_code filesystem_error;
    const std::filesystem::path parent = path.parent_path();
    const bool parent_is_directory =
        std::filesystem::is_directory(parent, filesystem_error);
    if (filesystem_error || parent.empty() || !parent_is_directory)
    {
        set_error(error,
            "The benchmark CSV parent directory is not accessible");
        return std::nullopt;
    }
    const bool output_exists = std::filesystem::exists(path, filesystem_error);
    if (filesystem_error)
    {
        set_error(error,
            "The benchmark CSV path could not be inspected safely");
        return std::nullopt;
    }
    if (output_exists)
    {
        set_error(error,
            "The benchmark CSV already exists; refusing to overwrite evidence");
        return std::nullopt;
    }

    Config config;
    config.output_path = path.string();
    config.scenario = scenario;
    config.variant = variant;
    config.source_sha = source_sha.empty() ? "unknown" : source_sha;
    config.build_type = build_type.empty() ? "unknown" : build_type;
    if (!unsigned_environment(
            "ALCHEMY_METAL_BENCHMARK_WARMUP_FRAMES", 120, true,
            MAX_FRAME_COUNT, "frame count", config.warmup_frames, error) ||
        !unsigned_environment(
            "ALCHEMY_METAL_BENCHMARK_WARMUP_SECONDS", 30, true,
            MAX_WARMUP_SECONDS, "second count", config.warmup_seconds,
            error) ||
        !unsigned_environment(
            "ALCHEMY_METAL_BENCHMARK_SAMPLE_FRAMES", 300, false,
            MAX_FRAME_COUNT, "frame count", config.sample_frames, error) ||
        !bool_environment(
            "ALCHEMY_METAL_BENCHMARK_REQUIRE_EXACT", false,
            config.require_exact_native, error))
    {
        return std::nullopt;
    }
    return config;
}

ALMetalBenchmarkRecorder::ALMetalBenchmarkRecorder(Config config) :
    mConfig(std::move(config))
{
}

ALMetalBenchmarkRecorder::~ALMetalBenchmarkRecorder() = default;

void ALMetalBenchmarkRecorder::resetSequence() noexcept
{
    mSequenceStartTimestamp.reset();
    mLastTimestamp.reset();
    mLastFrameIndex = 0;
    mLastMetalCommandBuffers = 0;
    mLastMetalTimingSubmitted = 0;
    mLastMetalTimingCompleted = 0;
    mLastMetalTimingValid = 0;
    mLastMetalGPUTimeNanoseconds = 0;
    mSequenceWidth = 0;
    mSequenceHeight = 0;
    mSequenceQuality = 0;
    mWarmupSeen = 0;
}

ALMetalBenchmarkRecorder::Result ALMetalBenchmarkRecorder::fail(
    const std::string& message,
    std::string* error)
{
    set_error(error, message);
    mFailed = true;
    mFinished = true;
    if (mOutput)
    {
        mOutput->close();
    }
    return Result::failed;
}

bool ALMetalBenchmarkRecorder::openOutput(std::string* error)
{
    if (mOutput)
    {
        return true;
    }
    std::error_code filesystem_error;
    const bool output_exists =
        std::filesystem::exists(mConfig.output_path, filesystem_error);
    if (filesystem_error)
    {
        set_error(error,
            "The benchmark CSV path could not be inspected safely");
        return false;
    }
    if (output_exists)
    {
        set_error(error,
            "The benchmark CSV appeared before collection and will not be overwritten");
        return false;
    }
    mOutput = std::make_unique<std::ofstream>(
        mConfig.output_path, std::ios::out | std::ios::trunc);
    if (!*mOutput)
    {
        set_error(error, "Could not create the benchmark CSV");
        mOutput.reset();
        return false;
    }
    *mOutput
        << "scenario,variant,frame_ms,frame_index,source_sha,build_type,"
           "drawable_width,drawable_height,quality_profile,draw_calls,"
           "metal_submissions,metal_completed,metal_timed,metal_gpu_ms,"
           "native_requested,native_attempted,native_exact,gl_fallback,"
           "native_presentation_state\n";
    return static_cast<bool>(*mOutput);
}

ALMetalBenchmarkRecorder::Result ALMetalBenchmarkRecorder::record(
    const Frame& frame,
    std::string* error)
{
    if (mFinished)
    {
        return mFailed ? Result::failed : Result::complete;
    }
    if (mConfig.sample_frames == 0 ||
        mConfig.sample_frames > MAX_FRAME_COUNT ||
        mConfig.warmup_frames > MAX_FRAME_COUNT ||
        mConfig.warmup_seconds > MAX_WARMUP_SECONDS)
    {
        return fail("The benchmark frame counts are invalid", error);
    }
    const bool acceptable = frame.eligible && frame.drawable_width != 0 &&
        frame.drawable_height != 0 &&
        (!mConfig.require_exact_native || frame.native_exact);
    if (!acceptable)
    {
        if (mSamplesWritten != 0)
        {
            return fail(
                "The required consecutive benchmark sequence became ineligible",
                error);
        }
        resetSequence();
        return Result::inactive;
    }
    const bool valid_timing_snapshot =
        frame.metal_timing_completed <= frame.metal_timing_submitted &&
        frame.metal_timing_valid <= frame.metal_timing_completed;
    if (!valid_timing_snapshot)
    {
        if (mSamplesWritten != 0)
        {
            return fail(
                "The Metal completion telemetry snapshot is inconsistent",
                error);
        }
        resetSequence();
        return Result::inactive;
    }
    const char* presentation_state =
        presentation_state_name(frame.native_presentation_state);
    const bool native_requested =
        frame.native_presentation_state !=
            NativePresentationState::disabled;
    const bool native_attempted = native_requested &&
        frame.native_presentation_state !=
            NativePresentationState::not_attempted;
    const bool native_submitted =
        frame.native_presentation_state ==
            NativePresentationState::submitted;
    const bool valid_presentation_snapshot = presentation_state &&
        frame.native_exact == native_submitted &&
        (!frame.gl_fallback ||
         (native_requested && !native_submitted));
    if (!valid_presentation_snapshot)
    {
        if (mSamplesWritten != 0)
        {
            return fail(
                "The native presentation telemetry snapshot is inconsistent",
                error);
        }
        resetSequence();
        return Result::inactive;
    }

    if (!mLastTimestamp)
    {
        mSequenceStartTimestamp = frame.timestamp;
        mLastTimestamp = frame.timestamp;
        mLastFrameIndex = frame.frame_index;
        mLastMetalCommandBuffers = frame.metal_command_buffers_committed;
        mLastMetalTimingSubmitted = frame.metal_timing_submitted;
        mLastMetalTimingCompleted = frame.metal_timing_completed;
        mLastMetalTimingValid = frame.metal_timing_valid;
        mLastMetalGPUTimeNanoseconds =
            frame.metal_gpu_time_nanoseconds;
        mSequenceWidth = frame.drawable_width;
        mSequenceHeight = frame.drawable_height;
        mSequenceQuality = frame.quality_profile;
        mWarmupSeen = mConfig.warmup_frames == 0 ? 0 : 1;
        return Result::warming;
    }

    const bool consecutive = frame.frame_index == mLastFrameIndex + 1;
    const bool stable = frame.drawable_width == mSequenceWidth &&
        frame.drawable_height == mSequenceHeight &&
        frame.quality_profile == mSequenceQuality;
    const bool monotonic_submissions =
        frame.metal_command_buffers_committed >= mLastMetalCommandBuffers;
    const bool monotonic_timing =
        frame.metal_timing_submitted >= mLastMetalTimingSubmitted &&
        frame.metal_timing_completed >= mLastMetalTimingCompleted &&
        frame.metal_timing_valid >= mLastMetalTimingValid &&
        frame.metal_gpu_time_nanoseconds >=
            mLastMetalGPUTimeNanoseconds;
    const double frame_ms =
        std::chrono::duration<double, std::milli>(
            frame.timestamp - *mLastTimestamp).count();
    if (!consecutive || !stable || !monotonic_submissions ||
        !monotonic_timing ||
        !(frame_ms > 0.0) ||
        !std::isfinite(frame_ms))
    {
        if (mSamplesWritten != 0)
        {
            return fail(
                "The consecutive benchmark sequence changed frame, size, "
                "quality, Metal telemetry counter or clock ordering",
                error);
        }
        resetSequence();
        return record(frame, error);
    }

    const std::uint64_t metal_submissions =
        frame.metal_command_buffers_committed - mLastMetalCommandBuffers;
    const std::uint64_t metal_completed =
        frame.metal_timing_completed - mLastMetalTimingCompleted;
    const std::uint64_t metal_timed =
        frame.metal_timing_valid - mLastMetalTimingValid;
    const double metal_gpu_ms = static_cast<double>(
        frame.metal_gpu_time_nanoseconds -
            mLastMetalGPUTimeNanoseconds) / 1'000'000.0;
    mLastTimestamp = frame.timestamp;
    mLastFrameIndex = frame.frame_index;
    mLastMetalCommandBuffers = frame.metal_command_buffers_committed;
    mLastMetalTimingSubmitted = frame.metal_timing_submitted;
    mLastMetalTimingCompleted = frame.metal_timing_completed;
    mLastMetalTimingValid = frame.metal_timing_valid;
    mLastMetalGPUTimeNanoseconds = frame.metal_gpu_time_nanoseconds;
    const bool warmup_time_complete = mSequenceStartTimestamp &&
        frame.timestamp - *mSequenceStartTimestamp >=
            std::chrono::seconds(mConfig.warmup_seconds);
    if (mWarmupSeen < mConfig.warmup_frames || !warmup_time_complete)
    {
        ++mWarmupSeen;
        return Result::warming;
    }

    if (!openOutput(error))
    {
        return fail(error && !error->empty()
            ? *error : "Could not open benchmark CSV", error);
    }
    *mOutput << csv_field(mConfig.scenario) << ','
             << csv_field(mConfig.variant) << ','
             << std::fixed << std::setprecision(6) << frame_ms << ','
             << frame.frame_index << ','
             << csv_field(mConfig.source_sha) << ','
             << csv_field(mConfig.build_type) << ','
             << frame.drawable_width << ',' << frame.drawable_height << ','
             << frame.quality_profile << ',' << frame.draw_calls << ','
             << metal_submissions << ',' << metal_completed << ','
             << metal_timed << ',' << std::fixed << std::setprecision(6)
             << metal_gpu_ms << ','
             << (native_requested ? 1 : 0) << ','
             << (native_attempted ? 1 : 0) << ','
             << (frame.native_exact ? 1 : 0) << ','
             << (frame.gl_fallback ? 1 : 0) << ','
             << presentation_state << '\n';
    if (!*mOutput)
    {
        return fail("Writing the benchmark CSV failed", error);
    }
    ++mSamplesWritten;
    if (mSamplesWritten == mConfig.sample_frames)
    {
        mOutput->close();
        if (mOutput->fail())
        {
            return fail("Closing the benchmark CSV failed", error);
        }
        mFinished = true;
        return Result::complete;
    }
    return Result::recorded;
}

std::uint32_t ALMetalBenchmarkRecorder::samplesWritten() const noexcept
{
    return mSamplesWritten;
}

bool ALMetalBenchmarkRecorder::finished() const noexcept
{
    return mFinished;
}
