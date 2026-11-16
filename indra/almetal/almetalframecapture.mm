/**
 * @file almetalframecapture.mm
 * @brief Default-off capture of one complete eligible display interval.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "almetalframecapture.h"
#include "almetaldevice.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <mach/mach_time.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace
{
    void setError(std::string* error, const char* message) noexcept
    {
        try { if (error) *error = message; }
        catch (...) {} // Diagnostics must never interrupt normal rendering.
    }

    struct Descriptor
    {
        int value = -1;
        ~Descriptor() { if (value >= 0) close(value); }
        int release() noexcept { return std::exchange(value, -1); }
    };

    bool writeAll(int descriptor, const char* bytes, std::size_t length) noexcept
    {
        while (length)
        {
            const ssize_t count = write(descriptor, bytes, length);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) return false;
            bytes += count;
            length -= static_cast<std::size_t>(count);
        }
        return true;
    }

    std::uint64_t monotonicNanoseconds() noexcept
    {
        static const auto timebase = []
        {
            mach_timebase_info_data_t value{};
            mach_timebase_info(&value);
            return value;
        }();
        return static_cast<std::uint64_t>(
            (static_cast<__uint128_t>(mach_absolute_time()) * timebase.numer) / timebase.denom);
    }

    const char* triggerName(ALMetalFrameCapture::Trigger trigger) noexcept
    {
        using Trigger = ALMetalFrameCapture::Trigger;
        switch (trigger)
        {
            case Trigger::quiet_early: return "quiet_early";
            case Trigger::quiet_late_decline: return "quiet_late_decline";
            case Trigger::quiet_late_fallback: return "quiet_late_fallback";
            default: return "legacy";
        }
    }
}

struct ALMetalFrameCapture::Impl
{
    int parent_directory = -1;
    int directory = -1;
    int metadata = -1;
    std::string parent_path;
    std::string session_path;
    const char* child = session_directory;
    id<MTLDevice> device = nil;
    MTLCaptureManager* manager = nil;
    NSURL* trace_url = nil;
    Stats stats;
    std::uint64_t registry_id = 0;
    // Only a successful start by THIS helper gives it permission to stop.
    // All capture control is main-owner-only; do not concurrently stop/restart
    // this process's singleton capture manager from another controller.
    bool owns_capture = false;
    bool selected_pass = false;
    Mode mode = Mode::display;
    unsigned records = 0;
    std::uint64_t last_frame = 0;

    ~Impl()
    {
        if (metadata >= 0) close(metadata);
        if (directory >= 0) close(directory);
        if (parent_directory >= 0) close(parent_directory);
    }

    bool outputLocationUnchanged() const noexcept
    {
        struct stat parent{}, session{}, path_parent{}, path_session{}, child{};
        const auto secure = [](const struct stat& value)
        {
            return S_ISDIR(value.st_mode) && value.st_uid == geteuid() &&
                (value.st_mode & 07777) == 0700;
        };
        const auto same = [](const struct stat& left, const struct stat& right)
        {
            return left.st_dev == right.st_dev && left.st_ino == right.st_ino;
        };
        // The fd collision check and Metal's pathname must still identify
        // the same reserved private directory after the entire warm-up.
        // The caller owns this path cooperatively: Metal has no dirfd-based
        // output API that would eliminate the remaining check/start race.
        return fstat(parent_directory, &parent) == 0 &&
            fstat(directory, &session) == 0 &&
            lstat(parent_path.c_str(), &path_parent) == 0 &&
            lstat(session_path.c_str(), &path_session) == 0 &&
            fstatat(parent_directory, this->child, &child, AT_SYMLINK_NOFOLLOW) == 0 &&
            secure(parent) && secure(session) && secure(path_parent) && secure(path_session) &&
            same(parent, path_parent) && same(session, path_session) && same(session, child);
    }

    bool record(const char* status, std::uint64_t frame,
                std::int64_t native_error = 0) noexcept
    {
        // Fixed strings and numeric, non-identifying fields only. At most
        // eight short records, including cancellation/error, per helper.
        if (metadata < 0 || records >= 8) return false;
        char json[1536];
        const int size = std::snprintf(json, sizeof(json),
            "{\"schema_version\":1,\"status\":\"%s\",\"frame_index\":%llu,"
            "\"start_frame\":%llu,\"stop_frame\":%llu,\"device_registry_id\":%llu,"
            "\"cooldown_eligible_intervals\":%u,\"eligible_intervals_waited\":%u,"
            "\"start_attempts\":%u,\"native_error_code\":%lld,"
            "\"quiet_trigger\":\"%s\","
            "\"quiet_cpu_clock\":\"mach_absolute_time_ns\","
            "\"quiet_start_request_monotonic_ns\":%llu,"
            "\"quiet_start_return_monotonic_ns\":%llu,"
            "\"quiet_stop_request_monotonic_ns\":%llu,"
            "\"quiet_stop_return_monotonic_ns\":%llu,"
            "\"quiet_cpu_bounds_are_gpu_timestamps\":false,"
            "\"scope\":\"device_all_queues\","
            "\"interval\":\"%s\","
            "\"includes_intervening_idle_uploads\":true,"
            "\"gpu_completion_waited\":false,\"present_duration_measurement\":false}\n",
            status, static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(stats.start_frame),
            static_cast<unsigned long long>(stats.stop_frame),
            static_cast<unsigned long long>(registry_id), cooldown_frames,
            stats.eligible_intervals_waited, stats.start_attempts,
            static_cast<long long>(native_error),
            triggerName(stats.trigger),
            static_cast<unsigned long long>(stats.start_request_monotonic_ns),
            static_cast<unsigned long long>(stats.start_return_monotonic_ns),
            static_cast<unsigned long long>(stats.stop_request_monotonic_ns),
            static_cast<unsigned long long>(stats.stop_return_monotonic_ns),
            mode == Mode::hero_forward ? "selected_hero_forward_face_1"
                : selected_pass ? "selected_main_base_pass"
                : "eligible_display_to_next_eligible_display");
        if (size <= 0 || static_cast<std::size_t>(size) >= sizeof(json)) return false;
        ++records;
        return writeAll(metadata, json, static_cast<std::size_t>(size));
    }

    bool fail(const char* message, const char* status, std::uint64_t frame,
              std::string* error, std::int64_t native_error = 0) noexcept
    {
        stats.state = State::failed;
        record(status, frame, native_error);
        setError(error, message);
        return false;
    }

    bool start(std::uint64_t frame, std::string* error)
    {
        if (manager.isCapturing)
            return fail("Another controller started capture; it was left unchanged",
                        "capture_conflict", frame, error);
        if (!outputLocationUnchanged())
            return fail("Frame capture output path changed during warm-up; no capture attempted",
                        "path_changed", frame, error);
        struct stat existing{};
        if (fstatat(directory, trace_name, &existing, AT_SYMLINK_NOFOLLOW) == 0 || errno != ENOENT)
            return fail("Frame capture trace already exists; no overwrite attempted",
                        "output_conflict", frame, error);

        ++stats.start_attempts;
        MTLCaptureDescriptor* descriptor = [[MTLCaptureDescriptor alloc] init];
        descriptor.captureObject = device;
        descriptor.destination = MTLCaptureDestinationGPUTraceDocument;
        descriptor.outputURL = trace_url;
        NSError* native_error = nil;
        stats.start_request_monotonic_ns = monotonicNanoseconds();
        const bool started = [manager startCaptureWithDescriptor:descriptor error:&native_error];
        stats.start_return_monotonic_ns = monotonicNanoseconds();
        if (!started)
            return fail("Metal could not start frame capture; normal rendering continues",
                        "start_failed", frame, error, native_error.code);
        owns_capture = true;
        stats.start_frame = frame;
        stats.state = State::capturing;
        if (!record("capturing", frame))
        {
            stop(frame, true, nullptr);
            return fail("Could not write frame capture start metadata",
                        "metadata_error", frame, error);
        }
        return true;
    }

    bool stop(std::uint64_t frame, bool cancelled, std::string* error)
    {
        if (!owns_capture) return true;
        if (!manager.isCapturing)
        {
            owns_capture = false;
            return fail("Frame capture was stopped by another controller",
                        "interrupted", frame, error);
        }
        stats.stop_request_monotonic_ns = monotonicNanoseconds();
        [manager stopCapture];
        stats.stop_return_monotonic_ns = monotonicNanoseconds();
        owns_capture = false;
        stats.stop_frame = frame;
        if (cancelled)
            return fail("Frame capture ended before its requested work completed",
                        "cancelled", frame, error);
        struct stat output{};
        if (fstatat(directory, trace_name, &output, AT_SYMLINK_NOFOLLOW) != 0 ||
            (!S_ISDIR(output.st_mode) && !S_ISREG(output.st_mode)))
            return fail("Frame capture stopped without a GPU trace document",
                        "missing_trace", frame, error);
        // stopCapture returned and an output exists; this does not assert
        // successful replay, GPU completion or performance validity.
        stats.state = State::stopped;
        if (!record("stopped", frame))
            return fail("Could not write frame capture stop metadata",
                        "metadata_error", frame, error);
        return true;
    }
};

ALMetalFrameCapture::ALMetalFrameCapture(std::unique_ptr<Impl> impl)
    : mImpl(std::move(impl)) {}

ALMetalFrameCapture::~ALMetalFrameCapture()
{
    if (!mImpl || !mImpl->owns_capture) return;
    @autoreleasepool
    {
        @try { mImpl->stop(mImpl->last_frame, true, nullptr); }
        @catch (NSException*) {} // A diagnostic teardown must remain isolated.
    }
}

std::unique_ptr<ALMetalFrameCapture> ALMetalFrameCapture::createIfRequested(
    ALMetalDevice& device, std::string* error) noexcept
{
    const char* path = std::getenv(directory_environment);
    if (!path || !*path) return nullptr;
    const char* mode = std::getenv(mode_environment);
    const bool hero_forward = mode && std::strcmp(mode, "hero-forward") == 0;
    const bool selected_pass = hero_forward ||
        (mode && std::strcmp(mode, "main-base") == 0);
    if (mode && *mode && !selected_pass && std::strcmp(mode, "display") != 0)
    {
        setError(error, "Frame capture mode must be display, main-base or hero-forward");
        return nullptr;
    }
    return create(device, path, session_directory,
                  hero_forward ? Mode::hero_forward
                      : selected_pass ? Mode::main_base : Mode::display,
                  Trigger::legacy, error);
}

std::unique_ptr<ALMetalFrameCapture> ALMetalFrameCapture::createTriggeredDisplay(
    ALMetalDevice& device, const char* path, Trigger trigger, std::string* error) noexcept
{
    if (trigger != Trigger::quiet_early && trigger != Trigger::quiet_late_decline &&
        trigger != Trigger::quiet_late_fallback)
    {
        setError(error, "Explicit frame capture requires a quiet trigger");
        return nullptr;
    }
    const char* legacy_directory = std::getenv(directory_environment);
    const char* legacy_mode = std::getenv(mode_environment);
    if ((legacy_directory && *legacy_directory) || (legacy_mode && *legacy_mode))
    {
        setError(error, "Quiet frame capture conflicts with a configured legacy capture controller");
        return nullptr;
    }
    return create(device, path,
                  trigger == Trigger::quiet_early ? quiet_early_directory : quiet_late_directory,
                  Mode::display, trigger, error);
}

std::unique_ptr<ALMetalFrameCapture> ALMetalFrameCapture::create(
    ALMetalDevice& device, const char* path, const char* child,
    Mode mode, Trigger trigger, std::string* error) noexcept
{
    try
    {
        @autoreleasepool
        {
            @try
            {
                if (!path || path[0] != '/')
                {
                    setError(error, "Frame capture requires an absolute existing private directory");
                    return nullptr;
                }
                Descriptor directory{open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)};
                struct stat status{};
                if (directory.value < 0 || fstat(directory.value, &status) != 0 ||
                    !S_ISDIR(status.st_mode) || status.st_uid != geteuid() ||
                    (status.st_mode & 07777) != 0700)
                {
                    setError(error, "Frame capture directory must be owned by this user and mode0700");
                    return nullptr;
                }
                struct stat existing{};
                if (fstatat(directory.value, child, &existing,
                            AT_SYMLINK_NOFOLLOW) == 0 || errno != ENOENT)
                {
                    setError(error, "Frame capture output already exists or cannot be checked; no overwrite attempted");
                    return nullptr;
                }
                const char* enabled = std::getenv("MTL_CAPTURE_ENABLED");
                if (!enabled || std::strcmp(enabled, "1") != 0)
                {
                    setError(error, "Frame capture requires MTL_CAPTURE_ENABLED=1 at process launch");
                    return nullptr;
                }
                if (!device.nativeHandle())
                {
                    setError(error, "Frame capture requires a native Metal device");
                    return nullptr;
                }
                MTLCaptureManager* manager = [MTLCaptureManager sharedCaptureManager];
                if (![manager supportsDestination:MTLCaptureDestinationGPUTraceDocument])
                {
                    setError(error, "Metal GPU trace document capture is not supported by this process");
                    return nullptr;
                }
                if (manager.isCapturing)
                {
                    setError(error, "Another controller is already capturing; frame capture left it unchanged");
                    return nullptr;
                }
                char canonical[PATH_MAX];
                if (!realpath(path, canonical))
                {
                    setError(error, "Could not resolve frame capture output directory");
                    return nullptr;
                }
                struct stat canonical_status{};
                if (stat(canonical, &canonical_status) != 0 ||
                    canonical_status.st_dev != status.st_dev ||
                    canonical_status.st_ino != status.st_ino)
                {
                    setError(error, "Frame capture output directory changed during initialization");
                    return nullptr;
                }
                auto impl = std::make_unique<Impl>();
                impl->parent_path = canonical;
                impl->child = child;
                impl->session_path = impl->parent_path + '/' + child;
                impl->device = (__bridge id<MTLDevice>)device.nativeHandle();
                impl->manager = manager;
                impl->selected_pass = mode != Mode::display;
                impl->mode = mode;
                impl->stats.trigger = trigger;
                if (trigger != Trigger::legacy) impl->stats.state = State::ready_for_display;
                impl->registry_id = device.info().registry_id;
                // Atomic exclusive reservation: even two helpers targeting
                // the same directory cannot supply an existing trace to Metal.
                if (mkdirat(directory.value, child, 0700) != 0)
                {
                    setError(error, "Frame capture output reservation failed; no overwrite attempted");
                    return nullptr;
                }
                Descriptor session{openat(directory.value, child,
                    O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC)};
                if (session.value < 0 || fchmod(session.value, 0700) != 0)
                {
                    setError(error, "Could not secure frame capture session directory");
                    return nullptr;
                }
                Descriptor metadata{openat(session.value, metadata_name,
                    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600)};
                if (metadata.value < 0 || fchmod(metadata.value, 0600) != 0)
                {
                    setError(error, "Could not create private frame capture metadata");
                    return nullptr;
                }
                const std::string trace_path = std::string(canonical) + '/' +
                    child + '/' + trace_name;
                NSString* native_path = [NSString stringWithUTF8String:trace_path.c_str()];
                if (!native_path)
                {
                    setError(error, "Frame capture output path is not valid UTF-8");
                    return nullptr;
                }
                impl->trace_url = [NSURL fileURLWithPath:native_path];
                impl->directory = session.release();
                impl->parent_directory = directory.release();
                impl->metadata = metadata.release();
                if (!impl->record(trigger == Trigger::legacy ? "waiting_for_timing"
                                                            : "quiet_ready_for_display", 0))
                {
                    setError(error, "Could not write initial frame capture metadata");
                    return nullptr;
                }
                return std::unique_ptr<ALMetalFrameCapture>(new ALMetalFrameCapture(std::move(impl)));
            }
            @catch (NSException*)
            {
                setError(error, "Frame capture initialization skipped after a native diagnostic exception");
                return nullptr;
            }
        }
    }
    catch (...)
    {
        setError(error, "Frame capture initialization skipped after a diagnostic exception");
        return nullptr;
    }
}

bool ALMetalFrameCapture::advanceDisplayInterval(
    std::uint64_t frame, bool eligible, std::string* error) noexcept
{
    try
    {
        @autoreleasepool
        {
            @try
            {
                mImpl->last_frame = frame;
                if (mImpl->stats.state == State::failed) return false;
                if (mImpl->stats.state == State::stopped) return true;
                if (mImpl->stats.state == State::capturing)
                    return !eligible ||
                        (mImpl->stats.trigger != Trigger::legacy && frame <= mImpl->stats.start_frame) ||
                        mImpl->stop(frame, mImpl->selected_pass, error);
                if (mImpl->stats.trigger != Trigger::legacy) return true;

                // Observe timing even during warm-up/ineligible main entries.
                if (ALMetalDevice::commandTimingEnabled())
                {
                    if (mImpl->stats.state == State::waiting_for_timing &&
                        !mImpl->record("waiting_for_disarm", frame))
                        return mImpl->fail("Could not write frame capture arming metadata",
                                           "metadata_error", frame, error);
                    mImpl->stats.state = State::waiting_for_disarm;
                    mImpl->stats.eligible_intervals_waited = 0;
                    return true;
                }
                if (mImpl->stats.state == State::waiting_for_timing) return true;
                if (mImpl->stats.state == State::ready_for_pass) return true;
                if (mImpl->stats.state == State::waiting_for_disarm)
                {
                    mImpl->stats.state = State::cooling_down;
                    if (!mImpl->record("cooling_down", frame))
                        return mImpl->fail("Could not write frame capture cooldown metadata",
                                           "metadata_error", frame, error);
                    // This entry starts the first post-disarm interval; it
                    // has not yet finished, so do not count it as waited.
                    return true;
                }
                if (!eligible) return true;
                if (++mImpl->stats.eligible_intervals_waited < cooldown_frames) return true;
                if (mImpl->selected_pass)
                {
                    mImpl->stats.state = State::ready_for_pass;
                    return mImpl->record("ready_for_pass", frame) ||
                        mImpl->fail("Could not write selected-pass readiness metadata",
                                    "metadata_error", frame, error);
                }
                return mImpl->start(frame, error);
            }
            @catch (NSException*)
            {
                @try { mImpl->stop(frame, true, nullptr); }
                @catch (NSException*) {}
                return mImpl->fail("Frame capture skipped after a native diagnostic exception",
                                   "native_exception", frame, error);
            }
        }
    }
    catch (...)
    {
        @try { mImpl->stop(frame, true, nullptr); }
        @catch (NSException*) {}
        return mImpl->fail("Frame capture skipped after a diagnostic exception",
                           "exception", frame, error);
    }
}

ALMetalFrameCapture::Mode ALMetalFrameCapture::mode() const noexcept
{
    return mImpl->mode;
}

bool ALMetalFrameCapture::beginDisplayInterval(
    std::uint64_t frame, std::string* error) noexcept
{
    if (mImpl->stats.trigger == Trigger::legacy ||
        mImpl->stats.state != State::ready_for_display)
    {
        setError(error, "Explicit display capture is not ready");
        return false;
    }
    @autoreleasepool
    {
        @try
        {
            try
            {
                mImpl->last_frame = frame;
                return mImpl->start(frame, error);
            }
            catch (...)
            {
                mImpl->stop(frame, true, nullptr);
                return mImpl->fail("Explicit display capture failed after a diagnostic exception",
                                   "exception", frame, error);
            }
        }
        @catch (NSException*)
        {
            @try { mImpl->stop(frame, true, nullptr); }
            @catch (NSException*) {}
            return mImpl->fail("Explicit display capture failed after a native exception",
                               "native_exception", frame, error);
        }
    }
}

bool ALMetalFrameCapture::cancelDisplayInterval(
    std::uint64_t frame, std::string* error) noexcept
{
    if (mImpl->stats.trigger == Trigger::legacy)
    {
        setError(error, "Explicit cancellation requires a quiet display capture");
        return false;
    }
    @autoreleasepool
    {
        @try
        {
            mImpl->last_frame = frame;
            if (mImpl->stats.state == State::stopped) return true;
            if (mImpl->stats.state == State::failed) return false;
            if (mImpl->owns_capture) return mImpl->stop(frame, true, error);
            return mImpl->fail("Explicit display capture cancelled before start",
                               "cancelled", frame, error);
        }
        @catch (NSException*)
        {
            return mImpl->fail("Explicit display cancellation failed after a native exception",
                               "native_exception", frame, error);
        }
    }
}

bool ALMetalFrameCapture::selectedPassReady() const noexcept
{
    return mImpl->selected_pass &&
        mImpl->stats.state == State::ready_for_pass &&
        !ALMetalDevice::commandTimingEnabled();
}

bool ALMetalFrameCapture::beginSelectedPass(
    std::uint64_t frame, std::string* error) noexcept
{
    if (!selectedPassReady())
    {
        setError(error, "Selected-pass capture is not armed and ready");
        return false;
    }
    @autoreleasepool
    {
        @try
        {
            mImpl->last_frame = frame;
            return mImpl->start(frame, error);
        }
        @catch (NSException*)
        {
            @try { mImpl->stop(frame, true, nullptr); }
            @catch (NSException*) {}
            return mImpl->fail("Selected-pass capture skipped after a native exception",
                               "native_exception", frame, error);
        }
    }
}

bool ALMetalFrameCapture::finishSelectedPass(
    std::uint64_t frame, std::string* error, bool completed) noexcept
{
    if (!mImpl->selected_pass)
    {
        setError(error, "Manual capture completion requires selected-pass mode");
        return false;
    }
    @autoreleasepool
    {
        @try
        {
            mImpl->last_frame = frame;
            if (mImpl->stats.state == State::stopped) return true;
            if (mImpl->stats.state != State::capturing)
            {
                setError(error, "Selected-pass capture has not started");
                return false;
            }
            return mImpl->stop(frame, !completed, error);
        }
        @catch (NSException*)
        {
            @try { mImpl->stop(frame, true, nullptr); }
            @catch (NSException*) {}
            return mImpl->fail("Selected-pass capture completion failed after a native exception",
                               "native_exception", frame, error);
        }
    }
}

ALMetalFrameCapture::Stats ALMetalFrameCapture::stats() const noexcept
{
    return mImpl->stats;
}
