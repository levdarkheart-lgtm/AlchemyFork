/**
 * @file almetaldiagnostictelemetry.h
 * @brief Opt-in, bounded asynchronous command/frame diagnostics; no GPU waits.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace ALMetalDiagnosticTelemetry
{
// Frame IDs are actual display gFrameCount values only when an owner explicitly
// scopes them. A worker must copy this value explicitly; zero means unknown.
enum class ViewKind : std::uint32_t
{
    unknown = 0, main = 1, hero = 2, ordinary_probe = 3, shadow = 4, upload = 5,
};

struct FrameTag
{
    std::uint64_t frame_id = 0;
    ViewKind view_kind = ViewKind::unknown;
    std::int32_t view_index = -1;
};

enum class FrameOutcome : std::uint32_t
{
    completed = 0, incomplete = 1, exception = 2,
};

struct FrameRecord
{
    std::uint64_t iteration_id = 0;
    std::uint64_t begin_display_frame = 0, end_display_frame = 0;
    std::uint64_t start_mach_ns = 0, end_mach_ns = 0;
    std::uint32_t begin_startup_state = 0, end_startup_state = 0;
    std::uint64_t begin_flags = 0, end_flags = 0;
    std::uint32_t drawable_width = 0, drawable_height = 0;
    std::int32_t quality = 0;
    std::uint32_t begin_teleport_state = 0, end_teleport_state = 0;
    FrameOutcome outcome = FrameOutcome::completed;
    bool native_exact = false;
};

struct Config
{
    std::string path; // Local new file only; existing files fail closed.
    std::string source_sha;
    std::string build_id;
    double max_duration_seconds = 600.0;
    std::uint64_t max_bytes = 64u * 1024u * 1024u;
    std::uint32_t ring_records = 8192;
    std::uint32_t writer_interval_ms = 20;
};

struct Statistics
{
    std::uint64_t session_id = 0, attempted = 0, queued = 0, written = 0;
    std::uint64_t dropped_full = 0, dropped_contention = 0, dropped_budget = 0;
    std::uint64_t names_truncated = 0, command_labels_other = 0;
    std::uint64_t commands_created = 0, commands_committed = 0;
    std::uint64_t commands_completed = 0, commands_abandoned = 0;
    std::uint64_t bytes_written = 0, io_errors = 0;
    bool enabled = false;
};

// Configure only at owner startup/shutdown. No environment lookup occurs here
// or on any frame/command path. stop joins the CPU writer, never the GPU; late
// native callbacks are ignored by session identity. Pending counts at cutoff
// remain pending and are not manufactured as completed.
bool start(const Config& config, std::string* error = nullptr) noexcept;
void stop() noexcept;
bool enabled() noexcept;
std::uint64_t activeSessionId() noexcept;
Statistics statistics() noexcept;

// Nanoseconds from system mach_absolute_time, also the documented GPUStartTime
// / GPUEndTime clock domain. Header brackets a Unix UTC calibration read.
std::uint64_t machTimeNs() noexcept;
// Explicit low-frequency owner sampling only: NSProcessInfo thermalState,
// 0 nominal / 1 fair / 2 serious / 3 critical, -1 if unavailable.
std::int32_t systemThermalState() noexcept;
FrameTag currentFrameTag() noexcept;
void recordFrame(const FrameRecord& frame) noexcept;
// Names must be program-authored static labels, never asset/location/account
// names, UUIDs, native pointers or user strings. Values have owner-documented
// units; no implicit conversion to physical allocation or GPU-busy time.
void recordMetrics(FrameTag frame, const char* static_name,
                   const std::array<std::uint64_t, 8>& values) noexcept;
void recordMarker(const char* static_name, FrameTag frame,
                  std::uint64_t reason = 0) noexcept;

// Main-owner diagnostic observations only. No input text, key values or
// coordinates enter the stream. SDL's event clock is calibrated with a
// caller-supplied Mach bracket around SDL_GetTicksNS, never assumed identical.
struct InputObservation
{
    std::uint64_t session_id = 0, sequence = 0;
    std::uint64_t event_mach_lower_ns = 0, event_mach_upper_ns = 0;
    std::uint64_t dispatch_end_ns = 0;
    std::uint32_t kind = 0; // 1 motion, 2 button, 3 wheel, 4 keyboard.
};
InputObservation recordInputDispatch(std::uint64_t expected_session, std::uint32_t kind,
    std::uint64_t event_sdl_ns, std::uint64_t sample_sdl_ns,
    std::uint64_t mach_before_ns, std::uint64_t mach_after_ns,
    std::uint64_t dispatch_begin_ns, std::uint64_t dispatch_end_ns) noexcept;
InputObservation latestInputObservation() noexcept;

struct ViewObservation
{
    std::uint64_t session_id = 0, sequence = 0;
    FrameTag frame;
    InputObservation input;
    std::uint64_t modelview_identity = 0, projection_identity = 0;
    bool camera_changed = false;
};
// Record the actual camera matrices paired with a main view, not an arbitrary
// object's model transform. Raw component bits permit independent exact
// identity checks; a changed camera is not itself proof of an input cause.
ViewObservation recordViewCamera(FrameTag frame, std::uint64_t group,
    std::span<const float,16> modelview, std::span<const float,16> projection) noexcept;
ViewObservation latestViewObservation() noexcept;
struct PresentationSourceObservation
{
    std::uint64_t session_id = 0;
    FrameTag frame;
    std::uint64_t source_frame = UINT64_MAX;
    std::uint64_t color_identity = 0, color_generation = 0;
    std::uint64_t depth_identity = 0, depth_generation = 0;
    std::uint64_t surface_generation = 0;
    bool selected = false;
};
// Actual immutable UI background selected by its existing owner, which can
// be an older world or cleared loading backing. No attempted-camera fallback.
void recordPresentationSource(PresentationSourceObservation source) noexcept;
PresentationSourceObservation latestPresentationSource() noexcept;
// Session-qualified asynchronous metric publication; late callbacks cannot
// enter a subsequently started diagnostic session.
void recordSessionMetrics(std::uint64_t session_id, FrameTag frame,
    const char* static_name, const std::array<std::uint64_t,8>& values) noexcept;
struct ProcessMemoryObservation
{
    std::uint64_t sampled_mach_ns = 0, physical_footprint_bytes = 0;
    std::uint64_t sampling_ns = 0;
    bool valid = false;
};
// Explicit sparse owner sampling; cached for100ms, and no syscall when OFF.
ProcessMemoryObservation sampleProcessMemory() noexcept;

// Device integration only. Tokens contain no native/asset pointers and do not
// own a Metal object. Event timestamps are CPU observation boundaries; GPU
// execution timestamps are supplied only by an actual native completion.
struct CommandToken
{
    std::uint64_t session_id = 0, command_id = 0;
    FrameTag frame;
};
CommandToken commandCreateBegin(const char* label) noexcept;
bool commandTokenActive(CommandToken token) noexcept;
void commandCreateEnd(CommandToken token, bool created) noexcept;
void commandCommitBegin(CommandToken token) noexcept;
void commandCommitEnd(CommandToken token) noexcept;
void commandCompleted(CommandToken token, std::uint32_t native_status,
                      double gpu_start_seconds, double gpu_end_seconds,
                      std::int64_t native_error_code = 0) noexcept;
void commandAbandoned(CommandToken token) noexcept;
// Observe only an already-requested wrapper wait. A zero token denotes a
// command created outside this session; caller scope is still recorded.
void commandWaitBegin(CommandToken token) noexcept;
void commandWaitEnd(CommandToken token, std::uint32_t native_status) noexcept;
}

class ALMetalDiagnosticFrameScope final
{
public:
    explicit ALMetalDiagnosticFrameScope(ALMetalDiagnosticTelemetry::FrameTag frame) noexcept;
    ~ALMetalDiagnosticFrameScope();
    ALMetalDiagnosticFrameScope(const ALMetalDiagnosticFrameScope&) = delete;
    ALMetalDiagnosticFrameScope& operator=(const ALMetalDiagnosticFrameScope&) = delete;
private:
    ALMetalDiagnosticTelemetry::FrameTag mPrevious;
    bool mEnabled = false;
};
