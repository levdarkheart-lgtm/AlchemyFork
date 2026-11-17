/**
 * @file alquietdiagnostics.h
 * @brief Explicit quiet-scene diagnostics; never a rendering policy.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include "../almetal/almetaldiagnostictelemetry.h"
#include <cstdint>

class ALMetalDevice;

// Initialize once at the first app iteration, before owner login. A missing
// ALCHEMY_METAL_DIAGNOSTIC_DIRECTORY leaves all collection disabled. auto
// captures additionally require ALCHEMY_METAL_QUIET_CAPTURE=auto and capture
// support at process launch. No settings or benchmark state is changed.
void alQuietDiagnosticsInitialize() noexcept;
void alQuietDiagnosticsShutdown(std::uint64_t frame) noexcept;

struct ALQuietDiagnosticDisplay
{
    std::uint64_t frame = 0; // Upcoming normal display ID at entry.
    bool normal_display = false;
    bool world_ready = false;
    bool world_invalidated = false;
    bool teleport_in_progress = false;
    bool cadence_valid = false;
    std::uint32_t width = 0, height = 0;
    std::uint64_t tracked_texture_bytes = 0, tracked_device_bytes = 0;
    std::uint64_t object_count = 0;
};

// Entry precedes every new display command. A capture spans this complete
// display interval plus intervening ordinary probes and idle uploads. Calls
// never install stage-timing scopes, change batches, or wait for the GPU.
void alQuietDiagnosticsAdvanceDisplay(ALMetalDevice* device,
    const ALQuietDiagnosticDisplay& observation) noexcept;
// App iterations continue even without display; cancel an unfinished capture
// after invalidation or a finite wall limit instead of awaiting another view.
void alQuietDiagnosticsHeartbeat(std::uint64_t frame,
    bool world_invalidated, bool cadence_valid = true,
    bool teleport_in_progress = false) noexcept;

// Main-owner observations for capture attribution and contained controller
// checks. This is not a renderer state or a runtime control interface.
struct ALQuietDiagnosticStatistics
{
    bool automatic = false, started = false, terminal = false;
    bool capture_active = false, capture_failed = false;
    std::uint32_t attempts = 0;
    std::uint32_t scene_generation = 1, total_attempts = 0;
};
ALQuietDiagnosticStatistics alQuietDiagnosticsStatistics() noexcept;

class ALQuietDiagnosticSpan final
{
public:
    explicit ALQuietDiagnosticSpan(const char* static_name) noexcept
        : mName(static_name)
    {
        if (ALMetalDiagnosticTelemetry::enabled())
        {
            mTag = ALMetalDiagnosticTelemetry::currentFrameTag();
            mStart = ALMetalDiagnosticTelemetry::machTimeNs();
        }
    }
    ~ALQuietDiagnosticSpan()
    {
        if (mStart)
            ALMetalDiagnosticTelemetry::recordMetrics(mTag, mName,
                {mStart, ALMetalDiagnosticTelemetry::machTimeNs(), 0, 0, 0, 0, 0, 0});
    }
    ALQuietDiagnosticSpan(const ALQuietDiagnosticSpan&) = delete;
    ALQuietDiagnosticSpan& operator=(const ALQuietDiagnosticSpan&) = delete;
private:
    const char* mName;
    ALMetalDiagnosticTelemetry::FrameTag mTag;
    std::uint64_t mStart = 0;
};
