/**
 * @file almetaldiagnostictelemetry.mm
 * @brief Bounded, lossy diagnostics with a deferred private JSONL writer.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetaldiagnostictelemetry.h"

#import <Foundation/Foundation.h>

#include <mach/mach_time.h>
#include <mach/mach.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread/qos.h>

#include <algorithm>
#include <atomic>
#include <bit>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <type_traits>

namespace ALMetalDiagnosticTelemetry
{
namespace
{
constexpr std::size_t maximum_ring_records = 8192;
constexpr std::uint64_t footer_reserve = 4096;
constexpr std::size_t name_capacity = 96;
std::atomic<std::uint64_t> active_session{0};
thread_local FrameTag current_frame;
struct OwnerObservations
{
    std::uint64_t session = 0, next_input = 0, next_view = 0;
    InputObservation input;
    ViewObservation view;
    PresentationSourceObservation presentation;
    std::array<std::uint32_t,32> camera_bits{};
    bool camera_known = false;
    ProcessMemoryObservation memory;
};
// Input dispatch, camera capture and present submission use the existing main
// owner. Native callbacks receive value snapshots, never this mutable state.
thread_local OwnerObservations owner_observations;
struct State;
std::atomic<State*> initialized_state{nullptr};

enum class Kind : std::uint32_t
{
    create_begin, create_end, commit_begin, commit_end, complete, abandon,
    wait_begin, wait_end, frame, metrics, marker,
};
struct Record
{
    Kind kind = Kind::marker;
    std::uint64_t sequence = 0, mach_ns = 0, command_id = 0;
    FrameTag frame, caller;
    FrameRecord frame_record;
    std::array<std::uint64_t, 8> values{};
    char name[name_capacity]{};
    double gpu_start = 0, gpu_end = 0;
    std::int64_t error_code = 0;
    std::uint32_t native_status = 0;
    bool created = false;
};
static_assert(std::is_trivially_copyable_v<Record>);

struct Counters
{
    std::atomic<std::uint64_t> attempted{0}, queued{0}, written{0};
    std::atomic<std::uint64_t> full{0}, contention{0}, budget{0};
    std::atomic<std::uint64_t> truncated{0}, other_labels{0};
    std::atomic<std::uint64_t> created{0}, committed{0}, completed{0}, abandoned{0};
    std::atomic<std::uint64_t> bytes{0}, io_errors{0};
    void reset() noexcept
    {
        for (auto* value : {&attempted,&queued,&written,&full,&contention,&budget,
                           &truncated,&other_labels,&created,&committed,&completed,
                           &abandoned,&bytes,&io_errors}) value->store(0);
    }
};

enum class StopReason { none, requested, duration_limit, byte_limit, io_error };
struct State
{
    std::mutex control;
    // This process-lifetime bounded sink is never destroyed underneath a late
    // native callback. Only active session IDs can touch its current ring.
    std::array<Record, maximum_ring_records> ring;
    std::atomic_flag ring_busy = ATOMIC_FLAG_INIT;
    std::size_t read = 0, write = 0, count = 0, capacity = 0;
    std::atomic<std::uint32_t> producers{0};
    std::atomic<StopReason> reason{StopReason::none};
    std::atomic<bool> writer_finished{true};
    std::atomic<bool> writer_released{false};
    std::uint64_t next_session = 0;
    std::atomic<std::uint64_t> session_id{0};
    std::atomic<std::uint64_t> next_command{0};
    std::uint64_t start_ns = 0, duration_ns = 0;
    Config config;
    int fd = -1;
    std::thread writer;
    Counters counters;
};

State& state()
{
    static State* value = []
    {
        auto* result = new State();
        initialized_state.store(result, std::memory_order_release);
        return result;
    }();
    return *value;
}

struct StartupFile
{
    State& sink;
    bool owned = true;
    ~StartupFile()
    {
        if (owned && sink.fd >= 0) { ::close(sink.fd); sink.fd = -1; }
    }
};

void setError(std::string* output, const char* message) noexcept
{
    if (output) try { *output = message; } catch (...) {}
}

// A producer never waits for the ring, file, writer or another producer. The
// short lifetime count lets start/stop reset storage after CPU copies finish;
// it is unrelated to native completion and never holds a Metal object.
struct Producer
{
    State* sink = nullptr;
    explicit Producer(std::uint64_t expected = 0) noexcept
    {
        const auto epoch = active_session.load(std::memory_order_acquire);
        if (!epoch || (expected && epoch != expected)) return;
        auto& s = state();
        s.producers.fetch_add(1, std::memory_order_acq_rel);
        if (active_session.load(std::memory_order_acquire) != epoch)
            s.producers.fetch_sub(1, std::memory_order_release);
        else sink = &s;
    }
    ~Producer()
    {
        if (sink) sink->producers.fetch_sub(1, std::memory_order_release);
    }
};

void enqueue(State& s, Record& record) noexcept
{
    record.sequence = s.counters.attempted.fetch_add(1, std::memory_order_relaxed) + 1;
    record.mach_ns = machTimeNs();
    if (s.ring_busy.test_and_set(std::memory_order_acquire))
    {
        s.counters.contention.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (s.count == s.capacity)
        s.counters.full.fetch_add(1, std::memory_order_relaxed);
    else
    {
        s.ring[s.write] = record;
        s.write = (s.write + 1) % s.capacity;
        ++s.count;
        s.counters.queued.fetch_add(1, std::memory_order_relaxed);
    }
    s.ring_busy.clear(std::memory_order_release);
}

std::size_t drain(State& s, std::array<Record, 32>& batch) noexcept
{
    if (s.ring_busy.test_and_set(std::memory_order_acquire)) return 0;
    const auto count = std::min(s.count, batch.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        batch[i] = s.ring[s.read];
        s.read = (s.read + 1) % s.capacity;
    }
    s.count -= count;
    s.ring_busy.clear(std::memory_order_release);
    return count;
}

void copyName(State& s, char* destination, const char* name) noexcept
{
    if (!name) name = "unnamed";
    std::size_t i = 0;
    // Labels are code-authored. Reject control/non-ASCII bytes rather than
    // allowing arbitrary user/asset strings into the diagnostic stream.
    for (; i + 1 < name_capacity && name[i]; ++i)
    {
        const auto c = static_cast<unsigned char>(name[i]);
        destination[i] = c >= 32 && c < 127 ? static_cast<char>(c) : '_';
    }
    destination[i] = 0;
    if (name[i]) s.counters.truncated.fetch_add(1, std::memory_order_relaxed);
}

const char* staticCommandLabel(const char* label) noexcept
{
    // Exact program literals only. Unknown/dynamic native labels may contain
    // object/asset identities, so their raw text is never copied or written.
    static constexpr const char* labels[] = {
        "Alchemy PBR preview render", "Alchemy buffer readback",
        "Alchemy cleared UI backing", "Alchemy direct spot-shadow slice",
        "Alchemy idle barrier", "Alchemy lit preview render",
        "Alchemy native SMAA lookup upload", "Alchemy native SSR scene-history publication",
        "Alchemy native WindLight sky-behind", "Alchemy native avatar-impostor post-deferred",
        "Alchemy native final-presentation reference capture",
        "Alchemy native hero-probe face publication", "Alchemy native hero-probe face transaction",
        "Alchemy native legacy glow", "Alchemy native legacy glow combine",
        "Alchemy native reflection-probe SH publication", "Alchemy native reflection-probe face publication",
        "Alchemy native world CAS", "Alchemy native world FXAA",
        "Alchemy native world G-buffer bootstrap", "Alchemy native world HDRI sky",
        "Alchemy native world RLVa sphere", "Alchemy native world SMAA blending weights",
        "Alchemy native world SMAA edges", "Alchemy native world SMAA neighborhood",
        "Alchemy native world WindLight aurora", "Alchemy native world WindLight clouds",
        "Alchemy native world WindLight heavenly bodies", "Alchemy native world WindLight meteors",
        "Alchemy native world WindLight sky", "Alchemy native world WindLight stars",
        "Alchemy native world base lighting", "Alchemy native world bloom",
        "Alchemy native world depth of field", "Alchemy native world display post",
        "Alchemy native world display sample", "Alchemy native world exposure",
        "Alchemy native world final presentation", "Alchemy native world lens-dirt generation",
        "Alchemy native world lens-flare state", "Alchemy native world light-map production",
        "Alchemy native world local-point-light accumulation", "Alchemy native world opaque post-deferred",
        "Alchemy native world projector-light accumulation", "Alchemy neutral sky cube",
        "Alchemy packed sky stars", "Alchemy ray dome cube bake", "Alchemy ray sky composite",
        "Alchemy retained world scale", "Alchemy scene loading difference",
        "Alchemy shared buffer readback completion", "Alchemy star attachment clear",
        "Alchemy terrain paintmap bake", "Alchemy texture readback",
        "Alchemy world inactive SSR fallback upload", "Alchemy world inactive color-grade texture upload",
        "Alchemy world inactive hero-probe fallback upload", "Alchemy world inactive reflection-probe upload",
        "Alchemy world unshadowed directional fallback upload",
        "Developer benchmark measured texture draws", "Developer benchmark untimed upload and clear",
        "Alchemy native world opaque and optical post", "Alchemy native world exposure and flare history",
        "Alchemy native world optical and presentation post", "Alchemy static legacy glow noise upload",
        "Alchemy early resident static bounds", "Alchemy native world G-buffer and ordered sky",
        "Alchemy static SSAO noise upload", "Alchemy native world light-map and deferred-light chain",
        "Alchemy image upload batch", "Alchemy image upload",
        "Alchemy native image subimage upload", "Alchemy image scale-down blit",
        "Alchemy image scale-down bilinear render",
    };
    if (label)
        for (const auto* known : labels) if (std::strcmp(label, known) == 0) return known;
    return nullptr;
}

std::string quoted(const char* text)
{
    std::string result = "\"";
    for (; *text; ++text)
    {
        if (*text == '\\' || *text == '"') result += '\\';
        result += *text;
    }
    result += '"';
    return result;
}

void tagJSON(std::ostringstream& out, FrameTag frame, const char* prefix = "")
{
    out << ",\"" << prefix << "frame_id\":" << frame.frame_id
        << ",\"" << prefix << "view_kind\":" << static_cast<std::uint32_t>(frame.view_kind)
        << ",\"" << prefix << "view_index\":" << frame.view_index;
}

const char* kindName(Kind kind) noexcept
{
    switch (kind)
    {
        case Kind::create_begin: return "command_create_begin";
        case Kind::create_end: return "command_create_end";
        case Kind::commit_begin: return "command_commit_begin";
        case Kind::commit_end: return "command_commit_end";
        case Kind::complete: return "command_complete";
        case Kind::abandon: return "command_abandon";
        case Kind::wait_begin: return "command_wait_begin";
        case Kind::wait_end: return "command_wait_end";
        case Kind::frame: return "frame";
        case Kind::metrics: return "metrics";
        case Kind::marker: return "marker";
    }
    return "unknown";
}

std::string serialize(const Record& r, std::uint64_t session)
{
    std::ostringstream out;
    out << std::setprecision(17) << "{\"event\":\"" << kindName(r.kind)
        << "\",\"session_id\":" << session << ",\"sequence\":" << r.sequence
        << ",\"mach_ns\":" << r.mach_ns;
    tagJSON(out, r.frame);
    if (r.kind == Kind::frame)
    {
        const auto& f = r.frame_record;
        out << ",\"iteration_id\":" << f.iteration_id
            << ",\"begin_display_frame\":" << f.begin_display_frame
            << ",\"end_display_frame\":" << f.end_display_frame
            << ",\"start_mach_ns\":" << f.start_mach_ns << ",\"end_mach_ns\":" << f.end_mach_ns
            << ",\"begin_startup_state\":" << f.begin_startup_state
            << ",\"end_startup_state\":" << f.end_startup_state
            << ",\"begin_flags\":" << f.begin_flags << ",\"end_flags\":" << f.end_flags
            << ",\"drawable_width\":" << f.drawable_width << ",\"drawable_height\":" << f.drawable_height
            << ",\"quality\":" << f.quality
            << ",\"begin_teleport_state\":" << f.begin_teleport_state
            << ",\"end_teleport_state\":" << f.end_teleport_state
            << ",\"outcome\":" << static_cast<std::uint32_t>(f.outcome)
            << ",\"native_exact\":" << (f.native_exact ? "true" : "false");
    }
    else if (r.kind == Kind::metrics || r.kind == Kind::marker)
    {
        out << ",\"name\":" << quoted(r.name);
        if (r.kind == Kind::marker) out << ",\"reason\":" << r.values[0];
        else
        {
            out << ",\"values\":[";
            for (std::size_t i = 0; i < r.values.size(); ++i) out << (i ? "," : "") << r.values[i];
            out << ']';
        }
    }
    else
    {
        out << ",\"command_id\":" << r.command_id;
        if (r.kind == Kind::create_begin) out << ",\"label\":" << quoted(r.name);
        if (r.kind == Kind::create_end) out << ",\"created\":" << (r.created ? "true" : "false");
        if (r.kind == Kind::wait_begin || r.kind == Kind::wait_end) tagJSON(out, r.caller, "caller_");
        if (r.kind == Kind::wait_end || r.kind == Kind::complete) out << ",\"native_status\":" << r.native_status;
        if (r.kind == Kind::complete)
        {
            const bool valid = r.native_status == 4u && std::isfinite(r.gpu_start) &&
                std::isfinite(r.gpu_end) && r.gpu_start > 0.0 && r.gpu_end > r.gpu_start;
            out << ",\"native_error_code\":" << r.error_code << ",\"gpu_time_valid\":" << (valid ? "true" : "false")
                << ",\"gpu_start_seconds\":";
            if (valid) out << r.gpu_start; else out << "null";
            out << ",\"gpu_end_seconds\":";
            if (valid) out << r.gpu_end; else out << "null";
        }
    }
    out << "}\n";
    return out.str();
}

bool writeText(State& s, const std::string& value) noexcept
{
    std::size_t offset = 0;
    while (offset < value.size())
    {
        const auto written = ::write(s.fd, value.data() + offset, value.size() - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0)
        {
            s.counters.io_errors.fetch_add(1);
            s.reason.store(StopReason::io_error);
            active_session.store(0, std::memory_order_release);
            return false;
        }
        offset += static_cast<std::size_t>(written);
        s.counters.bytes.fetch_add(static_cast<std::uint64_t>(written));
    }
    return true;
}

const char* reasonName(StopReason reason) noexcept
{
    switch (reason)
    {
        case StopReason::none: return "none";
        case StopReason::requested: return "requested";
        case StopReason::duration_limit: return "duration_limit";
        case StopReason::byte_limit: return "byte_limit";
        case StopReason::io_error: return "io_error";
    }
    return "unknown";
}

void disable(State& s, StopReason reason) noexcept
{
    StopReason expected = StopReason::none;
    s.reason.compare_exchange_strong(expected, reason);
    active_session.store(0, std::memory_order_release);
}

void footer(State& s)
{
    const auto count = statistics();
    std::ostringstream out;
    out << "{\"event\":\"footer\",\"session_id\":" << s.session_id
        << ",\"mach_ns\":" << machTimeNs() << ",\"stop_reason\":\"" << reasonName(s.reason.load())
        << "\",\"attempted\":" << count.attempted << ",\"queued\":" << count.queued
        << ",\"written\":" << count.written << ",\"dropped_full\":" << count.dropped_full
        << ",\"dropped_contention\":" << count.dropped_contention << ",\"dropped_budget\":" << count.dropped_budget
        << ",\"names_truncated\":" << count.names_truncated << ",\"command_labels_other\":" << count.command_labels_other
        << ",\"commands_created\":" << count.commands_created << ",\"commands_committed\":" << count.commands_committed
        << ",\"commands_completed\":" << count.commands_completed << ",\"commands_abandoned\":" << count.commands_abandoned
        << ",\"pending_created_at_cutoff\":" << count.commands_created - std::min(count.commands_created, count.commands_completed + count.commands_abandoned)
        << ",\"pending_committed_at_cutoff\":" << count.commands_committed - std::min(count.commands_committed, count.commands_completed)
        << ",\"bytes_before_footer\":" << count.bytes_written << ",\"io_errors\":" << count.io_errors
        << ",\"late_callbacks_ignored\":true}\n";
    writeText(s, out.str());
}

void writerMain(State* sink) noexcept
{
    auto& s = *sink;
    // Diagnostic serialization is utility work, never renderer-priority work.
    // This is a scheduling hint for this new CPU writer only, not a change to
    // viewer workers, Metal command queues or frame admission.
    (void)pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
    // start publishes the session only after std::thread construction has
    // succeeded. No deadline or stop transition races that startup handoff.
    while (!s.writer_released.load(std::memory_order_acquire)) std::this_thread::yield();
    try
    {
        std::array<Record, 32> batch;
        bool stopping = false;
        for (;;)
        {
            if (!stopping && machTimeNs() - s.start_ns >= s.duration_ns)
                disable(s, StopReason::duration_limit);
            stopping = s.reason.load() != StopReason::none;
            // A producer inside its bounded memory copy may finish after
            // disable. Drain only after all such CPU copies have retired.
            if (stopping && s.producers.load(std::memory_order_acquire) != 0)
            { std::this_thread::yield(); continue; }
            const auto count = drain(s, batch);
            if (!count)
            {
                if (stopping) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(s.config.writer_interval_ms));
                continue;
            }
            for (std::size_t i = 0; i < count; ++i)
            {
                auto row = serialize(batch[i], s.session_id);
                if (s.counters.bytes.load() + row.size() + footer_reserve > s.config.max_bytes)
                {
                    s.counters.budget.fetch_add(1);
                    disable(s, StopReason::byte_limit);
                    continue;
                }
                if (writeText(s, row)) s.counters.written.fetch_add(1);
            }
        }
        footer(s);
    }
    catch (...)
    {
        s.counters.io_errors.fetch_add(1);
        disable(s, StopReason::io_error);
    }
    if (s.fd >= 0) { ::close(s.fd); s.fd = -1; }
    s.writer_finished.store(true, std::memory_order_release);
}

void commandEvent(CommandToken token, Kind kind) noexcept
{
    if (!token.session_id) return;
    Producer producer(token.session_id);
    if (!producer.sink) return;
    Record record;
    record.kind = kind; record.command_id = token.command_id; record.frame = token.frame;
    if (kind == Kind::abandon) producer.sink->counters.abandoned.fetch_add(1);
    if (kind == Kind::commit_end) producer.sink->counters.committed.fetch_add(1);
    enqueue(*producer.sink, record);
}
}

std::uint64_t machTimeNs() noexcept
{
    static const auto timebase = [] { mach_timebase_info_data_t t{}; mach_timebase_info(&t); return t; }();
    return static_cast<std::uint64_t>((static_cast<__uint128_t>(mach_absolute_time()) * timebase.numer) / timebase.denom);
}

std::int32_t systemThermalState() noexcept
{
    @autoreleasepool
    {
        return static_cast<std::int32_t>(NSProcessInfo.processInfo.thermalState);
    }
}

bool start(const Config& config, std::string* error) noexcept
{
    try
    {
        auto& s = state();
        std::lock_guard<std::mutex> guard(s.control);
        if (active_session.load() || !s.writer_finished.load())
        { setError(error, "Diagnostic telemetry is already active"); return false; }
        if (s.writer.joinable()) s.writer.join();
        if (config.path.empty() || config.path[0] != '/' || config.path.find('\0') != std::string::npos ||
            config.source_sha.size() != 40 || config.source_sha.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos ||
            config.build_id.empty() || config.build_id.size() > 64 ||
            config.build_id.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._-") != std::string::npos ||
            !std::isfinite(config.max_duration_seconds) || config.max_duration_seconds <= 0 || config.max_duration_seconds > 3600 ||
            config.max_bytes < 8192 || config.ring_records < 2 || config.ring_records > maximum_ring_records ||
            config.writer_interval_ms < 1 || config.writer_interval_ms > 100)
        { setError(error, "Diagnostic telemetry configuration is invalid"); return false; }
        s.config = config;
        StartupFile startup_file{s};
        s.fd = ::open(config.path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (s.fd < 0) { setError(error, "Diagnostic telemetry could not create its new local file"); return false; }
        s.session_id = ++s.next_session;
        s.counters.reset(); s.next_command.store(0); s.read = s.write = s.count = 0;
        s.capacity = config.ring_records; s.reason.store(StopReason::none);
        s.duration_ns = static_cast<std::uint64_t>(config.max_duration_seconds * 1e9);
        const auto before = machTimeNs();
        const auto utc_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto after = machTimeNs();
        s.start_ns = after;
        std::ostringstream header;
        header << "{\"event\":\"header\",\"schema\":1,\"session_id\":" << s.session_id
            << ",\"process_id\":" << ::getpid()
            << ",\"source_sha\":" << quoted(config.source_sha.c_str()) << ",\"build_id\":" << quoted(config.build_id.c_str())
            << ",\"cpu_clock\":\"mach_absolute_time_ns\",\"gpu_clock\":\"system_mach_seconds\""
            << ",\"calibration_before_mach_ns\":" << before << ",\"calibration_unix_ns\":" << utc_ns
            << ",\"calibration_after_mach_ns\":" << after << ",\"ring_records\":" << s.capacity
            << ",\"record_size_bytes\":" << sizeof(Record) << ",\"max_bytes\":" << config.max_bytes
            << ",\"max_duration_ns\":" << s.duration_ns
            << ",\"lossy_nonblocking_producers\":true,\"gpu_interval_is_frame_or_busy_time\":false}\n";
        if (!writeText(s, header.str()))
        { setError(error, "Diagnostic telemetry header could not be written"); return false; }
        s.writer_finished.store(false);
        s.writer_released.store(false);
        try { s.writer = std::thread(writerMain, &s); }
        catch (...)
        { s.writer_finished.store(true); setError(error, "Diagnostic telemetry writer could not start"); return false; }
        startup_file.owned = false;
        active_session.store(s.session_id.load(), std::memory_order_release);
        s.writer_released.store(true, std::memory_order_release);
        return true;
    }
    catch (...) { setError(error, "Diagnostic telemetry startup failed"); return false; }
}

void stop() noexcept
{
    auto* existing = initialized_state.load(std::memory_order_acquire);
    if (!existing) return;
    try
    {
        auto& s = *existing; std::lock_guard<std::mutex> guard(s.control);
        disable(s, StopReason::requested);
        if (s.writer.joinable()) s.writer.join();
    }
    catch (...) {}
}

bool enabled() noexcept { return active_session.load(std::memory_order_relaxed) != 0; }
FrameTag currentFrameTag() noexcept { return current_frame; }

Statistics statistics() noexcept
{
    auto* existing = initialized_state.load(std::memory_order_acquire);
    if (!existing) return {};
    auto& s = *existing; auto& c = s.counters; Statistics out;
    out.session_id = s.session_id; out.enabled = enabled();
    out.attempted = c.attempted.load(); out.queued = c.queued.load(); out.written = c.written.load();
    out.dropped_full = c.full.load(); out.dropped_contention = c.contention.load(); out.dropped_budget = c.budget.load();
    out.names_truncated = c.truncated.load(); out.command_labels_other = c.other_labels.load();
    out.commands_created = c.created.load(); out.commands_committed = c.committed.load();
    out.commands_completed = c.completed.load(); out.commands_abandoned = c.abandoned.load();
    out.bytes_written = c.bytes.load(); out.io_errors = c.io_errors.load(); return out;
}

void recordFrame(const FrameRecord& frame) noexcept
{
    Producer producer; if (!producer.sink) return;
    Record record; record.kind = Kind::frame; record.frame = current_frame; record.frame_record = frame;
    enqueue(*producer.sink, record);
}

void recordMetrics(FrameTag frame, const char* name, const std::array<std::uint64_t, 8>& values) noexcept
{
    Producer producer; if (!producer.sink) return;
    Record record; record.kind = Kind::metrics; record.frame = frame; record.values = values;
    copyName(*producer.sink, record.name, name); enqueue(*producer.sink, record);
}

void recordMarker(const char* name, FrameTag frame, std::uint64_t reason) noexcept
{
    Producer producer; if (!producer.sink) return;
    Record record; record.kind = Kind::marker; record.frame = frame; record.values[0] = reason;
    copyName(*producer.sink, record.name, name); enqueue(*producer.sink, record);
}

void recordSessionMetrics(std::uint64_t session, FrameTag frame,
    const char* name, const std::array<std::uint64_t,8>& values) noexcept
{
    if (!session) return;
    Producer producer(session); if (!producer.sink) return;
    Record record;
    record.kind = Kind::metrics; record.frame = frame; record.values = values;
    copyName(*producer.sink, record.name, name); enqueue(*producer.sink, record);
}

std::uint64_t activeSessionId() noexcept
{
    return active_session.load(std::memory_order_acquire);
}

InputObservation recordInputDispatch(std::uint64_t expected_session, std::uint32_t kind,
    std::uint64_t event_sdl, std::uint64_t sample_sdl,
    std::uint64_t before, std::uint64_t after,
    std::uint64_t begin, std::uint64_t end) noexcept
{
    const auto session = active_session.load(std::memory_order_acquire);
    if (!session || session != expected_session || !kind || kind > 4) return {};
    auto& owner = owner_observations;
    if (owner.session != session) { owner = {}; owner.session = session; }
    if (owner.next_input == std::numeric_limits<std::uint64_t>::max()) return {};
    InputObservation input{session, ++owner.next_input, 0, 0, end, kind};
    if (event_sdl && sample_sdl >= event_sdl && after >= before)
    {
        const auto age = sample_sdl - event_sdl;
        if (before >= age)
        {
            input.event_mach_lower_ns = before - age;
            input.event_mach_upper_ns = after - age;
        }
    }
    owner.input = input;
    recordSessionMetrics(session, current_frame, "input_dispatch",
        {input.sequence, kind, event_sdl, sample_sdl, before, after, begin, end});
    return input;
}

InputObservation latestInputObservation() noexcept
{
    const auto session = active_session.load(std::memory_order_acquire);
    return session && owner_observations.session == session
        ? owner_observations.input : InputObservation{};
}

ViewObservation recordViewCamera(FrameTag frame, std::uint64_t group,
    std::span<const float,16> modelview, std::span<const float,16> projection) noexcept
{
    const auto session = active_session.load(std::memory_order_acquire);
    if (!session) return {};
    auto& owner = owner_observations;
    if (owner.session != session) { owner = {}; owner.session = session; }
    if (owner.next_view == std::numeric_limits<std::uint64_t>::max()) return {};
    std::array<std::uint32_t,32> bits;
    bool finite = true;
    for (std::size_t index = 0; index < 16; ++index)
    {
        bits[index] = std::bit_cast<std::uint32_t>(modelview[index]);
        bits[16 + index] = std::bit_cast<std::uint32_t>(projection[index]);
        finite = finite && std::isfinite(modelview[index]) && std::isfinite(projection[index]);
    }
    const bool previous = owner.camera_known;
    const bool changed = previous && bits != owner.camera_bits;
    const auto identity = [](const std::uint32_t* values)
    {
        std::uint64_t result = 14695981039346656037ull;
        for (std::size_t index = 0; index < 16; ++index)
        { result ^= values[index]; result *= 1099511628211ull; }
        return result;
    };
    ViewObservation view{session, ++owner.next_view, frame, owner.input,
        identity(bits.data()), identity(bits.data() + 16), changed};
    recordSessionMetrics(session, frame, "view_input",
        {view.sequence, group, owner.view.input.sequence, view.input.sequence,
         view.input.event_mach_lower_ns, view.input.event_mach_upper_ns,
         view.input.dispatch_end_ns, view.input.session_id == session});
    recordSessionMetrics(session, frame, "view_camera",
        {view.sequence, group, view.modelview_identity, view.projection_identity,
         previous, changed, finite, 0});
    if (!previous || changed)
        for (std::size_t first = 0; first < bits.size(); first += 6)
        {
            std::array<std::uint64_t,8> row{view.sequence, first};
            for (std::size_t index = 0; index < 6 && first + index < bits.size(); ++index)
                row[index + 2] = bits[first + index];
            recordSessionMetrics(session, frame, "view_camera_bits", row);
        }
    owner.camera_bits = bits; owner.camera_known = true; owner.view = view;
    return view;
}

ViewObservation latestViewObservation() noexcept
{
    const auto session = active_session.load(std::memory_order_acquire);
    return session && owner_observations.session == session
        ? owner_observations.view : ViewObservation{};
}

void recordPresentationSource(PresentationSourceObservation source) noexcept
{
    const auto session = active_session.load(std::memory_order_acquire);
    if (!session) return;
    auto& owner = owner_observations;
    if (owner.session != session) { owner = {}; owner.session = session; }
    source.session_id = session;
    owner.presentation = source;
    recordSessionMetrics(session, source.frame, "ui_background_source",
        {source.source_frame, source.surface_generation, source.color_identity,
         source.color_generation, source.depth_identity, source.depth_generation,
         source.selected, source.source_frame == UINT64_MAX});
}

PresentationSourceObservation latestPresentationSource() noexcept
{
    const auto session = active_session.load(std::memory_order_acquire);
    return session && owner_observations.session == session
        ? owner_observations.presentation : PresentationSourceObservation{};
}

ProcessMemoryObservation sampleProcessMemory() noexcept
{
    const auto session = active_session.load(std::memory_order_acquire);
    if (!session) return {};
    auto& owner = owner_observations;
    if (owner.session != session) { owner = {}; owner.session = session; }
    const auto now = machTimeNs();
    if (owner.memory.sampled_mach_ns && now >= owner.memory.sampled_mach_ns &&
        now - owner.memory.sampled_mach_ns < 100'000'000ull) return owner.memory;
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    const auto status = task_info(mach_task_self(), TASK_VM_INFO,
        reinterpret_cast<task_info_t>(&info), &count);
    const auto end = machTimeNs();
    const bool valid = status == KERN_SUCCESS && count >= TASK_VM_INFO_REV1_COUNT;
    owner.memory = {now, valid ? info.phys_footprint : 0,
        end >= now ? end - now : 0, valid};
    return owner.memory;
}

CommandToken commandCreateBegin(const char* label) noexcept
{
    Producer producer; if (!producer.sink) return {};
    auto& s = *producer.sink;
    CommandToken token{s.session_id, s.next_command.fetch_add(1) + 1, current_frame};
    Record record; record.kind = Kind::create_begin; record.command_id = token.command_id; record.frame = token.frame;
    const auto* known = staticCommandLabel(label);
    if (!known) { known = "other_command"; s.counters.other_labels.fetch_add(1); }
    copyName(s, record.name, known); enqueue(s, record); return token;
}

bool commandTokenActive(CommandToken token) noexcept
{
    return token.session_id && active_session.load(std::memory_order_relaxed) == token.session_id;
}

void commandCreateEnd(CommandToken token, bool created) noexcept
{
    Producer producer(token.session_id); if (!token.session_id || !producer.sink) return;
    Record record; record.kind = Kind::create_end; record.command_id = token.command_id; record.frame = token.frame; record.created = created;
    if (created) producer.sink->counters.created.fetch_add(1);
    enqueue(*producer.sink, record);
}
void commandCommitBegin(CommandToken token) noexcept { commandEvent(token, Kind::commit_begin); }
void commandCommitEnd(CommandToken token) noexcept { commandEvent(token, Kind::commit_end); }
void commandAbandoned(CommandToken token) noexcept { commandEvent(token, Kind::abandon); }

void commandCompleted(CommandToken token, std::uint32_t status, double start, double end, std::int64_t error) noexcept
{
    Producer producer(token.session_id); if (!token.session_id || !producer.sink) return;
    Record record; record.kind = Kind::complete; record.command_id = token.command_id; record.frame = token.frame;
    record.native_status = status; record.gpu_start = start; record.gpu_end = end; record.error_code = error;
    producer.sink->counters.completed.fetch_add(1); enqueue(*producer.sink, record);
}

void commandWaitBegin(CommandToken token) noexcept
{
    Producer producer; if (!producer.sink) return;
    if (token.session_id != producer.sink->session_id.load()) token = {};
    Record record; record.kind = Kind::wait_begin; record.command_id = token.command_id; record.frame = token.frame; record.caller = current_frame;
    enqueue(*producer.sink, record);
}

void commandWaitEnd(CommandToken token, std::uint32_t status) noexcept
{
    Producer producer; if (!producer.sink) return;
    if (token.session_id != producer.sink->session_id.load()) token = {};
    Record record; record.kind = Kind::wait_end; record.command_id = token.command_id; record.frame = token.frame; record.caller = current_frame; record.native_status = status;
    enqueue(*producer.sink, record);
}
}

ALMetalDiagnosticFrameScope::ALMetalDiagnosticFrameScope(ALMetalDiagnosticTelemetry::FrameTag frame) noexcept
{
    mEnabled = ALMetalDiagnosticTelemetry::enabled();
    if (mEnabled)
    {
        mPrevious = ALMetalDiagnosticTelemetry::current_frame;
        ALMetalDiagnosticTelemetry::current_frame = frame;
    }
}
ALMetalDiagnosticFrameScope::~ALMetalDiagnosticFrameScope()
{
    if (mEnabled) ALMetalDiagnosticTelemetry::current_frame = mPrevious;
}
