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

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <system_error>
#include <sys/stat.h>
#include <unistd.h>

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

std::string provenance_path(const std::string& csv)
{
    auto path = std::filesystem::path(csv);
    path.replace_extension(".provenance.jsonl");
    return path.string();
}

void json_string(std::ostream& out, const std::string& value)
{
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (const unsigned char c : value)
    {
        if (c == '"' || c == '\\') out << '\\' << char(c);
        else if (c < 0x20) out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else out << char(c);
    }
    out << '"';
}

template<typename T> void json_number(std::ostream& out, T value)
{
    if (std::isfinite(value)) out << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
    else out << "null";
}

template<typename T, std::size_t N>
void json_array(std::ostream& out, const std::array<T, N>& values)
{
    out << '[';
    for (std::size_t i = 0; i < N; ++i)
    {
        if (i) out << ',';
        json_number(out, values[i]);
    }
    out << ']';
}

void json_camera(std::ostream& out, const ALMetalBenchmarkRecorder::Camera& camera)
{
    out << "{\"origin\":"; json_array(out, camera.origin);
    out << ",\"at\":"; json_array(out, camera.at);
    out << ",\"left\":"; json_array(out, camera.left);
    out << ",\"up\":"; json_array(out, camera.up);
    out << ",\"modelview\":"; json_array(out, camera.modelview);
    out << ",\"projection\":"; json_array(out, camera.projection);
    out << ",\"vertical_fov_radians\":"; json_number(out, camera.vertical_fov_radians);
    out << ",\"aspect\":"; json_number(out, camera.aspect);
    out << ",\"near_clip\":"; json_number(out, camera.near_clip);
    out << ",\"draw_far_clip\":"; json_number(out, camera.draw_far_clip);
    out << '}';
}

template<typename T, std::size_t N>
bool finite_array(const std::array<T, N>& values)
{
    return std::all_of(values.begin(), values.end(), [](T value) { return std::isfinite(value); });
}

bool finite_camera(const ALMetalBenchmarkRecorder::Camera& camera)
{
    return finite_array(camera.origin) && finite_array(camera.at) &&
        finite_array(camera.left) && finite_array(camera.up) &&
        finite_array(camera.modelview) && finite_array(camera.projection) &&
        std::isfinite(camera.vertical_fov_radians) && std::isfinite(camera.aspect) &&
        std::isfinite(camera.near_clip) && std::isfinite(camera.draw_far_clip);
}

void json_view(std::ostream& out, const ALMetalBenchmarkRecorder::ViewState& view)
{
    const bool far_infinite = std::isinf(view.projection_far_clip);
    // A positive infinite projection far plane is intentional for reverse-Z.
    // This tests the number policy, not full geometric frustum validity.
    const bool far_valid = std::isfinite(view.projection_far_clip) ||
        (view.reverse_z && far_infinite && view.projection_far_clip > 0.f);
    const bool finite = finite_camera(view.viewer_camera) && finite_camera(view.pass_current_camera) &&
        finite_array(view.agent_camera_global) && finite_array(view.agent_focus_global) &&
        finite_array(view.agent_focus_target_global) && finite_array(view.agent_camera_up) &&
        far_valid && std::isfinite(view.requested_draw_distance);
    out << "{\"finite_camera_values\":" << (finite ? "true" : "false") << ",\"viewer_camera\":";
    json_camera(out, view.viewer_camera);
    out << ",\"pass_current_camera\":"; json_camera(out, view.pass_current_camera);
    out << ",\"agent_camera_global\":"; json_array(out, view.agent_camera_global);
    out << ",\"agent_focus_global\":"; json_array(out, view.agent_focus_global);
    out << ",\"agent_focus_target_global\":"; json_array(out, view.agent_focus_target_global);
    out << ",\"agent_camera_up\":"; json_array(out, view.agent_camera_up);
    out << ",\"viewport\":"; json_array(out, view.viewport);
    out << ",\"projection_far_clip\":"; json_number(out, view.projection_far_clip);
    out << ",\"projection_far_clip_infinite\":" << (far_infinite ? "true" : "false")
        << ",\"projection_far_clip_valid\":" << (far_valid ? "true" : "false");
    out << ",\"requested_draw_distance\":"; json_number(out, view.requested_draw_distance);
    out << ",\"camera_id\":" << view.camera_id
        << ",\"agent_camera_mode\":" << view.agent_camera_mode
        << ",\"agent_camera_preset\":" << view.agent_camera_preset
        << ",\"occlusion_mode\":" << view.occlusion_mode
        << ",\"reverse_z\":" << (view.reverse_z ? "true" : "false")
        << ",\"main_world_pass\":" << (view.main_world_pass ? "true" : "false") << '}';
}

void json_environment(std::ostream& out, const ALMetalBenchmarkRecorder::Environment& state)
{
    const bool local = state.captured && state.local_sky_available;
    const bool current = state.captured && state.current_sky_available;
    const bool sun = current && state.sun_direction_available;
    out << "{\"captured\":" << (state.captured ? "true" : "false")
        << ",\"selected_environment\":";
    if (state.captured) out << state.selected_environment;
    else out << "null";
    out << ",\"local_sky_available\":" << (local ? "true" : "false")
        << ",\"local_sky_asset_id\":";
    if (local) json_string(out, state.local_sky_asset_id.asString());
    else out << "null";
    out << ",\"current_sky_available\":" << (current ? "true" : "false")
        << ",\"current_sky_asset_id\":";
    if (current) json_string(out, state.current_sky_asset_id.asString());
    else out << "null";
    out << ",\"sun_direction_available\":" << (sun ? "true" : "false")
        << ",\"sun_direction_finite\":" << (sun && finite_array(state.sun_direction) ? "true" : "false")
        << ",\"sun_direction\":";
    if (sun) json_array(out, state.sun_direction);
    else out << "null";
    out << '}';
}

void json_restoration(std::ostream& out, const ALMetalBenchmarkRecorder::Restoration& state)
{
    out << "{\"checked\":" << (state.checked ? "true" : "false")
        << ",\"viewer_camera_restored\":" << (state.viewer_camera_restored ? "true" : "false")
        << ",\"pass_current_camera_restored\":" << (state.pass_current_camera_restored ? "true" : "false")
        << ",\"agent_camera_fields_restored\":" << (state.agent_camera_fields_restored ? "true" : "false")
        << ",\"camera_id_restored\":" << (state.camera_id_restored ? "true" : "false") << '}';
}

void json_queries(std::ostream& out, const ALQueryPool::Diagnostics& d)
{
    out << '{';
#define QUERY_FIELD(name) out << "\"" #name "\":" << d.name << ','
    QUERY_FIELD(epoch); QUERY_FIELD(acquired);
    QUERY_FIELD(attachment_calls); QUERY_FIELD(attached_unique);
    QUERY_FIELD(pending_polls); QUERY_FIELD(ready_zero_polls);
    QUERY_FIELD(ready_nonzero_polls); QUERY_FIELD(expired_polls);
    QUERY_FIELD(pending_unique); QUERY_FIELD(ready_zero_unique);
    QUERY_FIELD(ready_nonzero_unique); QUERY_FIELD(expired_unique);
    QUERY_FIELD(duplicate_phase_polls); QUERY_FIELD(untracked_polls);
    QUERY_FIELD(discarded_unique); QUERY_FIELD(retired);
    QUERY_FIELD(retired_without_terminal_observation);
    QUERY_FIELD(group_occluded_transitions); QUERY_FIELD(group_visible_transitions);
    QUERY_FIELD(spatial_group_skips);
#undef QUERY_FIELD
    out << "\"state_sort_group_skips\":" << d.state_sort_group_skips << '}';
}

bool same_view(const ALMetalBenchmarkRecorder::Provenance& a,
               const ALMetalBenchmarkRecorder::Provenance& b)
{
    return a.main_view_captured == b.main_view_captured &&
        a.main_view == b.main_view && a.frame_end_view == b.frame_end_view &&
        a.hero_restoration == b.hero_restoration && a.hud_restoration == b.hud_restoration;
}

void json_readiness(std::ostream& out, const ALMetalBenchmarkRecorder::Frame& frame,
                    bool required, const ALProbeReadiness::Status& status)
{
    out << "{\"required\":" << (required ? "true" : "false")
        << ",\"fresh\":" << (status.fresh ? "true" : "false")
        << ",\"valid\":" << (status.valid ? "true" : "false")
        << ",\"ready\":" << (status.ready ? "true" : "false")
        << ",\"admitted\":" << (status.admitted() ? "true" : "false")
        << ",\"membership_changed\":" << (status.membership_changed ? "true" : "false")
        << ",\"consecutive_ready_frames\":" << status.consecutive_ready_frames;
    if (required && status.fresh && frame.probe_readiness)
    {
        const auto& s = *frame.probe_readiness;
#define READINESS_BOOL(name) out << ",\"" #name "\":" << (s.name ? "true" : "false")
#define READINESS_COUNT(name) out << ",\"" #name "\":" << s.name
        READINESS_BOOL(enabled); READINESS_BOOL(layout_ready); READINESS_BOOL(invalid);
        READINESS_BOOL(reset); READINESS_BOOL(paused); READINESS_BOOL(reset_fade_pending);
        READINESS_BOOL(default_ready); READINESS_BOOL(native_pairs_required);
        READINESS_BOOL(updating_incomplete); READINESS_BOOL(refreshing);
        READINESS_BOOL(radiance_pass); READINESS_COUNT(updating_face);
        READINESS_COUNT(budget); READINESS_COUNT(dynamic_count); READINESS_COUNT(resolution);
        READINESS_COUNT(queued_create); READINESS_COUNT(queued_kill); READINESS_COUNT(assigned);
        READINESS_COUNT(incomplete); READINESS_COUNT(fading); READINESS_COUNT(unpublished_pairs);
        READINESS_COUNT(main_visible); READINESS_COUNT(member_count);
#undef READINESS_BOOL
#undef READINESS_COUNT
    }
    // Opaque owner/slot/generation tuples are deliberately never serialized.
    out << '}';
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
            config.require_exact_native, error) ||
        !bool_environment(
            "ALCHEMY_METAL_BENCHMARK_PROVENANCE", true,
            config.collect_provenance, error) ||
        !bool_environment(
            "ALCHEMY_METAL_BENCHMARK_REQUIRE_READY_PROBES", false,
            config.require_ready_probes, error))
    {
        return std::nullopt;
    }
    if (config.require_ready_probes && !config.collect_provenance)
    {
        set_error(error, "Probe-readiness collection requires benchmark provenance");
        return std::nullopt;
    }
    if (config.collect_provenance)
    {
        const bool sidecar_exists = std::filesystem::exists(
            provenance_path(config.output_path), filesystem_error);
        if (filesystem_error || sidecar_exists)
        {
            set_error(error, "The benchmark provenance sidecar exists or could not be inspected safely");
            return std::nullopt;
        }
    }
    return config;
}

ALMetalBenchmarkRecorder::ALMetalBenchmarkRecorder(Config config) :
    mConfig(std::move(config))
{
}

ALMetalBenchmarkRecorder::~ALMetalBenchmarkRecorder()
{
    closeProvenance();
}

ALMetalBenchmarkRecorder::DisplayObservation::~DisplayObservation() noexcept
{
    if (!mObserved && mRecorder && !mRecorder->finished())
    {
        try { mRecorder->record(Frame{}); }
        catch (...)
        {
            // An early return must never leave an apparently continuous run.
            mRecorder->mFailed = true;
            mRecorder->mFinished = true;
            mRecorder->closeProvenance();
            try { if (mRecorder->mOutput) mRecorder->mOutput->close(); }
            catch (...) {} // The guard itself must never escape a destructor.
        }
    }
}

ALMetalBenchmarkRecorder::Result ALMetalBenchmarkRecorder::DisplayObservation::record(
    const Frame& frame, std::string* error)
{
    mObserved = true;
    return mRecorder ? mRecorder->record(frame, error) : Result::inactive;
}

bool ALMetalBenchmarkRecorder::closeProvenance() noexcept
{
    if (!mProvenanceOutput) return true;
    const bool flushed = std::fflush(mProvenanceOutput) == 0;
    const bool closed = std::fclose(mProvenanceOutput) == 0;
    mProvenanceOutput = nullptr;
    return flushed && closed;
}

bool ALMetalBenchmarkRecorder::recordProvenance(
    const Frame& frame, const char* boundary, std::string* error)
{
    if (!mConfig.collect_provenance) return true;
    if (!frame.provenance || !frame.provenance->main_view_captured)
    {
        set_error(error, "The main-world benchmark provenance snapshot is missing");
        return false;
    }
    const Provenance& p = *frame.provenance;
    if (!mLastObservedProvenance || mLastObservedProvenanceFrame != frame.frame_index)
    {
        const bool unchanged = mLastObservedProvenance && same_view(p, *mLastObservedProvenance);
        if (mLastObservedProvenance && !unchanged) ++mCameraChangeFrames;
        mStableCameraFrames = unchanged && frame.frame_index == mLastObservedProvenanceFrame + 1
            ? mStableCameraFrames + 1 : 1;
        mLastObservedProvenance = p;
        mLastObservedProvenanceFrame = frame.frame_index;
    }
    const bool changed = !mLastWrittenProvenance || !same_view(p, *mLastWrittenProvenance);
    // Moving cameras are observed every frame but written after two stable
    // observations or at the bounded periodic checkpoint, not once per jitter.
    if (!boundary && !(changed && mStableCameraFrames >= 2) &&
        frame.frame_index < mLastProvenanceFrame + 120) return true;
    if (!mProvenanceOutput)
    {
        const int fd = ::open(provenance_path(mConfig.output_path).c_str(),
            O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (fd == -1)
        {
            set_error(error, "Could not create a new private benchmark provenance sidecar");
            return false;
        }
        mProvenanceOutput = ::fdopen(fd, "w");
        if (!mProvenanceOutput)
        {
            ::close(fd);
            set_error(error, "Could not open the benchmark provenance stream");
            return false;
        }
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\"schema_version\":1,\"event\":";
    json_string(out, boundary ? boundary : (changed ? "camera_change" : "periodic"));
    out << ",\"frame_index\":" << frame.frame_index
        << ",\"samples_written\":" << mSamplesWritten
        << ",\"camera_change_frames\":" << mCameraChangeFrames
        << ",\"consecutive_same_view_frames\":" << mStableCameraFrames
        << ",\"scenario\":"; json_string(out, mConfig.scenario);
    out << ",\"variant\":"; json_string(out, mConfig.variant);
    out << ",\"source_sha\":"; json_string(out, mConfig.source_sha);
    out << ",\"build_type\":"; json_string(out, mConfig.build_type);
    out << ",\"drawable_width\":" << frame.drawable_width
        << ",\"drawable_height\":" << frame.drawable_height
        << ",\"quality_profile\":" << frame.quality_profile
        << ",\"main_view\":"; json_view(out, p.main_view);
    out << ",\"frame_end_view\":"; json_view(out, p.frame_end_view);
    out << ",\"main_environment\":"; json_environment(out, p.main_environment);
    out << ",\"hero_restoration\":"; json_restoration(out, p.hero_restoration);
    out << ",\"hud_restoration\":"; json_restoration(out, p.hud_restoration);
    out << ",\"main_world_queries\":"; json_queries(out, p.main_world_queries);
    out << ",\"probe_readiness\":";
    json_readiness(out, frame, mConfig.require_ready_probes, mProbeReadiness.status());
    out << ",\"discard_bias\":{\"scope\":\"recorded_samples_only\",\"finite_samples\":"
        << mDiscardFinite << ",\"missing_samples\":" << mDiscardMissing
        << ",\"nonfinite_samples\":" << mDiscardNonfinite
        << ",\"minimum\":";
    if (mDiscardMinimum) json_number(out, *mDiscardMinimum); else out << "null";
    out << ",\"maximum\":";
    if (mDiscardMaximum) json_number(out, *mDiscardMaximum); else out << "null";
    out << '}';
    out << "}\n";
    const auto line = out.str();
    if (std::fwrite(line.data(), 1, line.size(), mProvenanceOutput) != line.size() ||
        std::fflush(mProvenanceOutput) != 0)
    {
        set_error(error, "Writing the benchmark provenance sidecar failed");
        return false;
    }
    mLastWrittenProvenance = p;
    mLastProvenanceFrame = frame.frame_index;
    return true;
}

void ALMetalBenchmarkRecorder::resetSequence() noexcept
{
    mProbeReadiness.reset();
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
    mDiscardFinite = mDiscardMissing = mDiscardNonfinite = 0;
    mDiscardMinimum.reset();
    mDiscardMaximum.reset();
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
    closeProvenance();
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
    if (mConfig.require_ready_probes && !mConfig.collect_provenance)
        return fail("Probe-readiness collection requires benchmark provenance", error);
    const bool acceptable = frame.eligible && frame.drawable_width != 0 &&
        frame.drawable_height != 0 &&
        (!mConfig.require_exact_native || frame.native_exact) &&
        (!mConfig.collect_provenance || (frame.provenance &&
            frame.provenance->main_view_captured && frame.provenance->main_view.main_world_pass));
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

    // This stricter mode owns a controlled main-view sequence. Probe
    // initialization and routine refresh are outside camera equality.
    if (mConfig.require_ready_probes && mLastTimestamp &&
        mLastObservedProvenance && frame.provenance &&
        !same_view(*mLastObservedProvenance, *frame.provenance))
    {
        if (mSamplesWritten)
            return fail("The probe-ready benchmark main view changed during sampling", error);
        resetSequence();
    }
    if (mConfig.require_ready_probes)
    {
        const auto status = mProbeReadiness.observe(frame.frame_index,
            frame.probe_readiness ? &*frame.probe_readiness : nullptr);
        if (mSamplesWritten && !status.admitted())
        {
            recordProvenance(frame, "probe_readiness_failure", nullptr);
            return fail("The sampled probe population became unready or changed membership", error);
        }
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
        if (!recordProvenance(frame, "warmup_start", error))
            return fail(error && !error->empty() ? *error : "Could not record benchmark warmup provenance", error);
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
    if (mWarmupSeen < mConfig.warmup_frames || !warmup_time_complete ||
        (mConfig.require_ready_probes && !mProbeReadiness.status().admitted()))
    {
        if (mWarmupSeen < MAX_FRAME_COUNT) ++mWarmupSeen;
        if (!recordProvenance(frame, nullptr, error))
            return fail(error && !error->empty() ? *error : "Could not record benchmark warmup provenance", error);
        return Result::warming;
    }

    if (!recordProvenance(frame, mSamplesWritten == 0 ? "sample_start" : nullptr, error))
        return fail(error && !error->empty() ? *error : "Could not record benchmark sample provenance", error);

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
    // Include only successfully written sample rows, never warmup or rejected
    // observations. Nonfinite input is counted without emitting invalid JSON.
    if (!frame.texture_discard_bias) ++mDiscardMissing;
    else if (!std::isfinite(*frame.texture_discard_bias)) ++mDiscardNonfinite;
    else
    {
        ++mDiscardFinite;
        mDiscardMinimum = mDiscardMinimum
            ? std::min(*mDiscardMinimum, *frame.texture_discard_bias)
            : *frame.texture_discard_bias;
        mDiscardMaximum = mDiscardMaximum
            ? std::max(*mDiscardMaximum, *frame.texture_discard_bias)
            : *frame.texture_discard_bias;
    }
    ++mSamplesWritten;
    if (mSamplesWritten == mConfig.sample_frames)
    {
        if (!recordProvenance(frame, "sample_end", error) || !closeProvenance())
            return fail(error && !error->empty() ? *error : "Could not finish benchmark provenance", error);
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
