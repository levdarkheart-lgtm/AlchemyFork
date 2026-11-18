/**
 * @file alquietdiagnostics.cpp
 * @brief Startup timing and bounded early/late capture orchestration.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "llviewerprecompiledheaders.h"
#include "alquietdiagnostics.h"
#include "../almetal/almetaldevice.h"
#include "../almetal/almetalframecapture.h"
#include "../almetal/almetalquietcapturepolicy.h"
#include "llversioninfo.h"
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
namespace D = ALMetalDiagnosticTelemetry;
struct Session
{
    bool initialized = false, automatic = false;
    std::string directory, capture_directory;
    ALMetalQuietCapturePolicy policy;
    std::unique_ptr<ALMetalFrameCapture> capture;
    std::uint64_t capture_start_ns = 0, last_resource_ns = 0;
    std::uint64_t world_start_ns = 0;
    bool world_invalidated = false;
    bool teleport_pending = false;
};
Session& quietSession() { static Session value; return value; }

void warning(const char* reason) noexcept
{
    try { LL_WARNS("QuietDiagnostics") << reason << LL_ENDL; }
    catch (...) {}
}

bool privateDirectory(const char* value) noexcept
{
    if (!value || *value != '/') return false;
    struct stat status{};
    return ::lstat(value, &status) == 0 && S_ISDIR(status.st_mode) &&
        status.st_uid == ::geteuid() && (status.st_mode & 0777) == 0700;
}

void cancel(Session& value, std::uint64_t frame, const char* reason,
    bool failure = true) noexcept
{
    if (value.capture)
    {
        std::string error;
        if (!value.capture->cancelDisplayInterval(frame, &error))
            value.policy.markCaptureFailed();
        value.capture.reset();
        D::recordMarker(reason, {frame, D::ViewKind::main, -1});
    }
    value.capture_start_ns = 0;
    if (failure) value.policy.markCaptureFailed();
}
}

void alQuietDiagnosticsInitialize() noexcept
{
    auto& value = quietSession();
    if (value.initialized) return;
    value.initialized = true;
    const char* directory = std::getenv("ALCHEMY_METAL_DIAGNOSTIC_DIRECTORY");
    if (!directory || !*directory) return;
    try
    {
        if (!privateDirectory(directory))
        { warning("Diagnostics require an existing owned mode0700 directory"); return; }
        value.directory = directory;
        value.capture_directory = directory;
        const auto& version = LLVersionInfo::instance();
        D::Config config;
        config.path = (std::filesystem::path(directory) / "diagnostic.jsonl").string();
        config.source_sha = version.getSourceRevision();
        config.build_id = version.getVersion() + "-" + version.getBuildConfig();
        // An opt-in session may need more retained events at a higher FPS.
        // Parse once at startup; the ring and all producer paths stay fixed.
        if (const char* requested = std::getenv("ALCHEMY_METAL_DIAGNOSTIC_MAX_MIB");
            requested && *requested)
        {
            const std::string_view budget(requested);
            std::uint32_t mib = 0;
            const auto parsed = std::from_chars(budget.data(), budget.data() + budget.size(), mib);
            if (parsed.ec != std::errc{} || parsed.ptr != budget.data() + budget.size() ||
                mib < 64 || mib > 256)
            {
                warning("Diagnostic byte budget must be an integer from 64 through 256 MiB; collection disabled");
                return;
            }
            config.max_bytes = std::uint64_t(mib) * 1024u * 1024u;
        }
        std::string error;
        if (!D::start(config, &error))
        { warning("Could not start private diagnostic telemetry; collection disabled"); return; }
        const char* mode = std::getenv("ALCHEMY_METAL_QUIET_CAPTURE");
        value.automatic = mode && std::string_view(mode) == "auto";
        if (mode && *mode && !value.automatic && std::string_view(mode) != "off")
            warning("Unknown quiet capture mode; only command/frame telemetry is active");
        if (const char* requested = std::getenv("ALCHEMY_METAL_QUIET_CAPTURE_LATE_SECONDS"))
        {
            const std::string_view seconds(requested);
            std::uint32_t late_seconds = 0;
            const auto parsed = std::from_chars(seconds.data(), seconds.data() + seconds.size(), late_seconds);
            if (parsed.ec != std::errc{} || parsed.ptr != seconds.data() + seconds.size() ||
                late_seconds < 60 || late_seconds > 240)
            {
                value.automatic = false;
                warning("ALCHEMY_METAL_QUIET_CAPTURE_LATE_SECONDS must be an integer from 60 through 240; "
                    "automatic captures disabled; command/frame telemetry remains active");
            }
            else
            {
                auto capture_config = value.policy.config();
                capture_config.late_min_age_ns = capture_config.fallback_age_ns =
                    std::uint64_t(late_seconds) * ALMetalQuietCapturePolicy::second;
                value.policy = ALMetalQuietCapturePolicy(capture_config);
            }
        }
        const auto& policy = value.policy.config();
        D::recordMetrics({}, "quiet_policy_config", {policy.early_age_ns,
            policy.early_min_intervals, policy.late_min_age_ns,
            policy.decline_window_ns,
            static_cast<std::uint64_t>(policy.decline_period_ratio * 1000),
            policy.fallback_age_ns, policy.maximum_age_ns, value.automatic});
        D::recordMetrics({}, "quiet_policy_stability", {
            static_cast<std::uint64_t>(policy.adjacent_window_mean_tolerance * 1000),
            policy.maximum_scene_generations, 0, 0, 0, 0, 0, 0});
        LL_INFOS("QuietDiagnostics") << "Private asynchronous command/frame telemetry armed; automatic captures="
            << value.automatic << "; normal rendering and graphics settings retained" << LL_ENDL;
    }
    catch (...)
    {
        value.automatic = false;
        D::stop();
        warning("Diagnostic initialization failed; collection disabled");
    }
}

void alQuietDiagnosticsHeartbeat(std::uint64_t frame,
    bool world_invalidated, bool cadence_valid, bool teleport_in_progress) noexcept
{
    auto& value = quietSession();
    // The caller excludes disconnect/logout/exit from its teleport signal,
    // including simultaneous terminal invalidation. Such a transition must
    // revoke a previously observed teleport before any later readiness.
    if (world_invalidated && !teleport_in_progress)
        value.teleport_pending = false;
    if (teleport_in_progress && value.policy.stats().started)
        value.teleport_pending = true;
    if (world_invalidated && value.world_start_ns && !value.world_invalidated)
    {
        value.world_invalidated = true;
        D::recordMarker("quiet_world_invalidated", {frame, D::ViewKind::main, -1});
    }
    if (!value.automatic && !value.capture) return;
    try
    {
        if (!value.capture && (!world_invalidated || !value.policy.stats().started)) return;
        const auto now = D::machTimeNs();
        if (world_invalidated)
        {
            const auto action = value.policy.update({now, frame, false, false, false, true, false});
            if (action == ALMetalQuietCapturePolicy::Action::invalidated || value.capture)
            {
                cancel(value, frame, "quiet_capture_world_invalidated", false);
                D::recordMarker("quiet_sequence_world_invalidated",
                    {frame, D::ViewKind::main, -1});
            }
            return;
        }
        if (value.capture && !cadence_valid)
        { cancel(value, frame, "quiet_capture_cadence_invalid"); return; }
        if (value.capture && (!D::enabled() || now < value.capture_start_ns ||
            now - value.capture_start_ns > 5'000'000'000ull))
            cancel(value, frame, "quiet_capture_interval_timeout");
    }
    catch (...) { value.capture.reset(); value.policy.markCaptureFailed(); }
}

void alQuietDiagnosticsAdvanceDisplay(ALMetalDevice* device,
    const ALQuietDiagnosticDisplay& observation) noexcept
{
    if (!D::enabled()) return;
    auto& value = quietSession();
    try
    {
        const auto now = D::machTimeNs();
        const D::FrameTag tag{observation.frame, D::ViewKind::main, -1};
        if (!value.world_start_ns && observation.normal_display && observation.world_ready &&
            observation.cadence_valid)
        {
            value.world_start_ns = now;
            D::recordMarker("quiet_world_started", tag);
        }
        if (observation.normal_display && (!value.last_resource_ns ||
                now - value.last_resource_ns >= 1'000'000'000ull))
        {
            value.last_resource_ns = now;
            D::recordMetrics(tag, "scene_resources", {observation.width, observation.height,
                observation.tracked_texture_bytes, observation.tracked_device_bytes,
                observation.object_count, device ? device->currentAllocatedSize() : 0,
                ALMetalDevice::committedCommandBufferCount(),
                static_cast<std::uint64_t>(D::systemThermalState())});
        }
        if (!value.automatic) return;
        const bool captured = value.capture &&
            value.capture->stats().state == ALMetalFrameCapture::State::capturing;
        alQuietDiagnosticsHeartbeat(observation.frame, observation.world_invalidated,
            !observation.normal_display || observation.cadence_valid,
            observation.teleport_in_progress);
        const ALMetalQuietCapturePolicy::Input policy_input{now, observation.frame,
            observation.world_ready, observation.normal_display, observation.cadence_valid,
            observation.world_invalidated, captured};
        // Only an observed teleport may open a second scene generation, and
        // only after the old native capture has been released. Disconnect,
        // logout, expiry and capture failure never silently restart capture.
        if (value.teleport_pending && !value.capture &&
            value.policy.rearmAfterInvalidation(policy_input))
        {
            const auto scene_directory = std::filesystem::path(value.directory) /
                ("scene-" + std::to_string(value.policy.stats().scene_generation));
            if (!std::filesystem::create_directory(scene_directory))
            {
                value.policy.markCaptureFailed();
                warning("Post-teleport capture directory exists; no overwrite or capture attempted");
                return;
            }
            std::filesystem::permissions(scene_directory, std::filesystem::perms::owner_all,
                std::filesystem::perm_options::replace);
            const auto path = scene_directory.string();
            if (!privateDirectory(path.c_str()))
            {
                value.policy.markCaptureFailed();
                warning("Post-teleport capture directory is not private; capture disabled");
                return;
            }
            value.capture_directory = path;
            value.world_invalidated = false;
            value.teleport_pending = false;
            D::recordMetrics(tag, "quiet_scene_started", {
                value.policy.stats().scene_generation, value.policy.stats().total_attempts,
                now, 0, 0, 0, 0, 0});
        }
        if (value.capture)
        {
            std::string error;
            if (!value.capture->advanceDisplayInterval(observation.frame,
                    observation.normal_display && observation.world_ready, &error))
            { cancel(value, observation.frame, "quiet_capture_advance_failed"); return; }
            if (value.capture->stats().state == ALMetalFrameCapture::State::stopped)
            {
                D::recordMarker("quiet_capture_stopped", tag,
                    static_cast<std::uint64_t>(value.capture->stats().trigger));
                value.capture.reset();
                value.capture_start_ns = 0;
            }
        }
        const auto action = value.policy.update(policy_input);
        using A = ALMetalQuietCapturePolicy::Action;
        if (action == A::none) return;
        if (action == A::invalidated || action == A::expired)
        {
            cancel(value, observation.frame, "quiet_sequence_ended", false);
            D::recordMarker("quiet_sequence_ended", tag, static_cast<std::uint64_t>(action));
            return;
        }
        const auto& stats = value.policy.stats();
        D::recordMetrics(tag, "quiet_capture_trigger", {static_cast<std::uint64_t>(action),
            stats.attempts, stats.world_start_ns, stats.baseline_periods,
            stats.baseline_total_ns, static_cast<std::uint64_t>(stats.baseline_mean_period_ns),
            static_cast<std::uint64_t>(stats.last_window_mean_period_ns), stats.cadence_suspensions});
        D::recordMetrics(tag, "quiet_capture_windows", {
            static_cast<std::uint64_t>(stats.previous_window_mean_period_ns),
            static_cast<std::uint64_t>(stats.last_window_mean_period_ns),
            stats.scene_generation, stats.total_attempts, 0, 0, 0, 0});
        const auto trigger = action == A::early ? ALMetalFrameCapture::Trigger::quiet_early :
            action == A::late_decline ? ALMetalFrameCapture::Trigger::quiet_late_decline :
            ALMetalFrameCapture::Trigger::quiet_late_fallback;
        std::string error;
        if (device) value.capture = ALMetalFrameCapture::createTriggeredDisplay(
            *device, value.capture_directory.c_str(), trigger, &error);
        if (!value.capture || !value.capture->beginDisplayInterval(observation.frame, &error))
        {
            D::recordMarker("quiet_capture_start_failed", tag, static_cast<std::uint64_t>(action));
            value.capture.reset();
            value.policy.markCaptureFailed();
            warning(error.empty() ? "Automatic quiet capture failed; rendering and telemetry continue" :
                error.c_str());
            return;
        }
        value.capture_start_ns = D::machTimeNs();
        D::recordMarker("quiet_capture_started", tag, static_cast<std::uint64_t>(action));
    }
    catch (...)
    {
        value.capture.reset();
        value.policy.markCaptureFailed();
        warning("Automatic quiet capture disabled after a diagnostic exception");
    }
}

void alQuietDiagnosticsShutdown(std::uint64_t frame) noexcept
{
    auto& value = quietSession();
    if (value.capture) cancel(value, frame, "quiet_capture_shutdown");
    D::stop();
}

ALQuietDiagnosticStatistics alQuietDiagnosticsStatistics() noexcept
{
    const auto& value = quietSession();
    const auto& policy = value.policy.stats();
    return {value.automatic, policy.started, policy.terminal,
        value.capture && value.capture->stats().state == ALMetalFrameCapture::State::capturing,
        policy.capture_failed, policy.attempts, policy.scene_generation, policy.total_attempts};
}
