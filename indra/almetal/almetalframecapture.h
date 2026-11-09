/**
 * @file almetalframecapture.h
 * @brief Explicit one-shot, post-benchmark Metal debugger capture.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>

class ALMetalDevice;

class ALMetalFrameCapture final
{
public:
    static constexpr const char* directory_environment =
        "ALCHEMY_METAL_FRAME_CAPTURE_DIRECTORY";
    static constexpr const char* mode_environment =
        "ALCHEMY_METAL_FRAME_CAPTURE_MODE";
    static constexpr const char* session_directory = "frame-capture";
    static constexpr const char* trace_name = "display-interval.gputrace";
    static constexpr const char* metadata_name = "metadata.jsonl";
    static constexpr std::uint32_t cooldown_frames = 60;
    static constexpr std::uint32_t hero_forward_face_index = 1;

    enum class Mode : std::uint8_t
    {
        display,
        main_base,
        hero_forward
    };

    enum class State : std::uint8_t
    {
        waiting_for_timing,
        waiting_for_disarm,
        cooling_down,
        capturing,
        stopped,
        failed,
        ready_for_pass
    };

    struct Stats
    {
        State state = State::waiting_for_timing;
        std::uint32_t eligible_intervals_waited = 0;
        std::uint32_t start_attempts = 0;
        std::uint64_t start_frame = 0;
        std::uint64_t stop_frame = 0;
    };

    // Create before benchmark arming. Unset/empty directory returns nullptr
    // without allocation or touching MTLCaptureManager. An explicit request
    // requires an absolute, existing, user-owned mode0700 directory and
    // MTL_CAPTURE_ENABLED=1 at PROCESS LAUNCH. A fixed child directory is
    // reserved exclusively; an existing child is never reused or overwritten.
    // Capture uses the supplied device and ALL of its command queues.
    // The caller must retain exclusive ownership of this directory/path for
    // the helper's lifetime. Paths are revalidated before start, but Metal's
    // URL-only output API cannot make pathname resolution atomic with start.
    static std::unique_ptr<ALMetalFrameCapture> createIfRequested(
        ALMetalDevice& device, std::string* error = nullptr) noexcept;
    ~ALMetalFrameCapture();
    ALMetalFrameCapture(const ALMetalFrameCapture&) = delete;
    ALMetalFrameCapture& operator=(const ALMetalFrameCapture&) = delete;

    // Call on the main CPU owner, BEFORE new commands at each display entry.
    // Timing is observed even when eligible=false: true -> false arms a
    // 60-eligible-interval cooldown. The caller must make eligibility require
    // SUCCESSFUL benchmark completion, a normal main display and no snapshot.
    // Capture starts once, then stops at the NEXT eligible display entry.
    // Ineligible/nested entries neither advance nor truncate the interval.
    //
    // This interval includes display_end_frame, ordinary probes and intervening
    // idle uploads; it is NOT a measurement of one present's GPU duration.
    // Only buffers created after start and committed before stop are captured.
    // No commands, waits, renderer state or quality changes are introduced.
    // false is a terminal diagnostic failure; normal rendering must continue.
    bool advanceDisplayInterval(std::uint64_t frame, bool eligible,
                                std::string* error = nullptr) noexcept;

    // With MODE=main-base or hero-forward, the same arming/cooldown prepares one
    // selected pass. Begin BEFORE creating its command buffer and finish
    // AFTER committing it. The caller must omit any enclosing command batch.
    // Device scope can also include concurrent upload commands; inspect the
    // actual capture rather than assuming a single command buffer.
    bool selectedPassReady() const noexcept;
    Mode mode() const noexcept;
    bool beginSelectedPass(std::uint64_t frame,
                           std::string* error = nullptr) noexcept;
    bool finishSelectedPass(std::uint64_t frame,
                            std::string* error = nullptr,
                            bool completed = true) noexcept;
    Stats stats() const noexcept;

private:
    struct Impl;
    explicit ALMetalFrameCapture(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;
};
