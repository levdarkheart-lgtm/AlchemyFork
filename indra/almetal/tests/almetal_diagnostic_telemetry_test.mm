/**
 * @file almetal_diagnostic_telemetry_test.mm
 * @brief Real bounded writer, scope, loss and native completion diagnostics.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetaldiagnostictelemetry.h"
#include "../almetaldevice.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <dispatch/dispatch.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>
#include <sys/stat.h>

namespace DT = ALMetalDiagnosticTelemetry;
namespace
{
struct Session
{
    DT::Config config;
    std::string directory;
    Session()
    {
        char pattern[] = "/tmp/almetal-diagnostic-test-XXXXXX";
        auto* path = mkdtemp(pattern);
        REQUIRE(path);
        directory = path;
        config.path = directory + "/events.jsonl";
        config.source_sha = "92ddb0966c21677956eab3ecfe8dbee44a9ab813";
        config.build_id = "contained-test";
    }
    ~Session()
    {
        DT::stop();
        // Requested cost evidence is deliberately retained by its external
        // directory. Ordinary contained fixtures do not retain temporary data.
        if (!directory.empty()) std::filesystem::remove_all(directory);
    }
    void start()
    {
        std::string error;
        REQUIRE(DT::start(config, &error));
        REQUIRE(error.empty());
    }
    std::vector<NSDictionary*> rows()
    {
        DT::stop();
        std::ifstream stream(config.path);
        REQUIRE(stream.good());
        std::vector<NSDictionary*> result;
        std::string line;
        while (std::getline(stream, line))
        {
            auto data = [NSData dataWithBytes:line.data() length:line.size()];
            NSError* error = nil;
            id object = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
            REQUIRE(error == nil);
            REQUIRE([object isKindOfClass:NSDictionary.class]);
            result.push_back(object);
        }
        REQUIRE(result.size() >= 2);
        CHECK([result.front()[@"event"] isEqual:@"header"]);
        CHECK([result.back()[@"event"] isEqual:@"footer"]);
        return result;
    }
};

std::vector<NSDictionary*> events(const std::vector<NSDictionary*>& rows, NSString* name)
{
    std::vector<NSDictionary*> result;
    for (auto row : rows) if ([row[@"event"] isEqual:name]) result.push_back(row);
    return result;
}

std::uint64_t number(NSDictionary* row, NSString* name)
{
    return [row[name] unsignedLongLongValue];
}

void awaitCompletions(std::uint64_t count)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (DT::statistics().commands_completed < count && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    REQUIRE(DT::statistics().commands_completed == count);
}
}

TEST_CASE("almetal: diagnostic disabled path leaves scopes and sinks untouched",
          "[almetal][diagnostic-telemetry]")
{
    DT::stop();
    CHECK_FALSE(DT::enabled());
    const auto before = DT::statistics();
    {
        ALMetalDiagnosticFrameScope frame({123, DT::ViewKind::main, 4});
        CHECK(DT::currentFrameTag().frame_id == 0);
        DT::recordMarker("disabled", {}, 1);
        DT::recordMetrics({}, "disabled", {});
        DT::recordFrame({});
        CHECK(DT::commandCreateBegin("Alchemy idle barrier").command_id == 0);
    }
    CHECK(DT::statistics().attempted == before.attempted);
    CHECK(DT::statistics().commands_created == before.commands_created);
}

TEST_CASE("almetal: input camera and memory observers remain inert without a session",
          "[almetal][diagnostic-telemetry][admission-observations]")
{
    DT::stop();
    const auto before = DT::statistics();
    std::array<float,16> matrix{};
    CHECK(DT::recordInputDispatch(DT::activeSessionId(), 1, 100, 150, 1000, 1010, 990, 1030).sequence == 0);
    CHECK(DT::latestInputObservation().sequence == 0);
    CHECK(DT::recordViewCamera({7, DT::ViewKind::main, -1}, 3, matrix, matrix).sequence == 0);
    CHECK(DT::latestViewObservation().sequence == 0);
    CHECK(DT::sampleProcessMemory().sampled_mach_ns == 0);
    DT::recordSessionMetrics(before.session_id, {}, "disabled_async", {});
    CHECK(DT::statistics().attempted == before.attempted);
}

TEST_CASE("almetal: input clock bounds exact camera identity and stale sessions stay explicit",
          "[almetal][diagnostic-telemetry][admission-observations]")
{
    @autoreleasepool
    {
        Session first;
        first.start();
        const auto input = DT::recordInputDispatch(DT::activeSessionId(), 1, 100, 150, 1000, 1010, 990, 1030);
        REQUIRE(input.sequence == 1);
        CHECK(input.event_mach_lower_ns == 950);
        CHECK(input.event_mach_upper_ns == 960);
        CHECK(input.dispatch_end_ns == 1030);
        std::array<float,16> modelview{}, projection{};
        modelview[0] = modelview[5] = modelview[10] = modelview[15] = 1.f;
        projection = modelview;
        const auto original = DT::recordViewCamera({7, DT::ViewKind::main, -1}, 3, modelview, projection);
        CHECK(original.input.sequence == input.sequence);
        CHECK_FALSE(original.camera_changed); // No previous observation exists.
        const auto equal = DT::recordViewCamera({8, DT::ViewKind::main, -1}, 4, modelview, projection);
        CHECK_FALSE(equal.camera_changed);
        CHECK(equal.modelview_identity == original.modelview_identity);
        modelview[12] = 1.f;
        const auto changed = DT::recordViewCamera({9, DT::ViewKind::main, -1}, 5, modelview, projection);
        CHECK(changed.camera_changed);
        CHECK(changed.modelview_identity != original.modelview_identity);
        CHECK(changed.projection_identity == original.projection_identity);
        DT::PresentationSourceObservation retained;
        retained.frame = {9, DT::ViewKind::main, -1};
        retained.source_frame = 7;
        retained.color_identity = 10; retained.color_generation = 2;
        retained.depth_identity = 11; retained.depth_generation = 3;
        retained.surface_generation = 4; retained.selected = true;
        DT::recordPresentationSource(retained);
        CHECK(DT::latestViewObservation().frame.frame_id == 9);
        CHECK(DT::latestPresentationSource().source_frame == 7); // Actual older world, not attempted9.
        retained.source_frame = UINT64_MAX;
        DT::recordPresentationSource(retained);
        CHECK(DT::latestPresentationSource().source_frame == UINT64_MAX); // Cleared loading backing.
        // Zero/future SDL timestamps are unknown event times, never silently
        // replaced with dispatch time. Processing is still observed.
        CHECK(DT::recordInputDispatch(DT::activeSessionId(), 4, 0, 150, 1000, 1010, 990, 1030).event_mach_lower_ns == 0);
        CHECK(DT::recordInputDispatch(DT::activeSessionId(), 4, 151, 150, 1000, 1010, 990, 1030).event_mach_upper_ns == 0);
        const auto memory = DT::sampleProcessMemory();
        REQUIRE(memory.sampled_mach_ns != 0);
        if (memory.valid) CHECK(memory.physical_footprint_bytes > 0);
        const auto cached = DT::sampleProcessMemory();
        CHECK(cached.sampled_mach_ns == memory.sampled_mach_ns);
        CHECK(cached.valid == memory.valid);
        const auto rows = first.rows();
        unsigned dispatches = 0, camera_rows = 0, component_rows = 0;
        for (auto row : events(rows, @"metrics"))
        {
            dispatches += [row[@"name"] isEqual:@"input_dispatch"];
            camera_rows += [row[@"name"] isEqual:@"view_camera"];
            component_rows += [row[@"name"] isEqual:@"view_camera_bits"];
        }
        CHECK(dispatches == 3);
        CHECK(camera_rows == 3);
        CHECK(component_rows == 12); // First and changed exact32-component snapshots.
        CHECK(DT::latestViewObservation().sequence == 0);

        Session second;
        second.start();
        const auto before = DT::statistics().attempted;
        DT::recordSessionMetrics(input.session_id, {}, "stale_present_callback", {});
        CHECK(DT::statistics().attempted == before);
        CHECK(DT::recordInputDispatch(input.session_id, 2, 100, 150, 1000, 1010, 990, 1030).sequence == 0);
        CHECK(DT::statistics().attempted == before); // Dispatch started in the previous session.
        CHECK(DT::latestInputObservation().sequence == 0);
        CHECK_FALSE(DT::latestPresentationSource().selected);
        CHECK(DT::recordInputDispatch(DT::activeSessionId(), 2, 100, 150, 1000, 1010, 990, 1030).sequence == 1);
        const auto second_rows = second.rows();
        for (auto row : events(second_rows, @"metrics"))
            CHECK_FALSE([row[@"name"] isEqual:@"stale_present_callback"]);
    }
}

TEST_CASE("almetal: command telemetry accessor preserves exact nonowning creation identity",
          "[almetal][diagnostic-telemetry][diagnostic-token]")
{
    @autoreleasepool
    {
        Session session;
        session.start();
        std::string error;
        auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        const auto commits = ALMetalDevice::committedCommandBufferCount();
        auto command = device->makeCommandBuffer("Alchemy idle barrier", &error);
        REQUIRE(command);
        const auto first = command->diagnosticToken();
        REQUIRE(first.command_id != 0);
        CHECK(DT::commandTokenActive(first));
        const auto created = DT::statistics().commands_created;
        for (unsigned index = 0; index < 32; ++index)
        {
            const auto repeated = command->diagnosticToken();
            CHECK(repeated.session_id == first.session_id);
            CHECK(repeated.command_id == first.command_id);
        }
        CHECK(DT::statistics().commands_created == created);
        CHECK(ALMetalDevice::committedCommandBufferCount() == commits);
        command.reset();
        CHECK(DT::statistics().commands_abandoned == 1);
        CHECK(first.command_id != 0); // Value token survives; wrapper ownership does not.
        const auto rows = session.rows();
        CHECK(events(rows, @"command_create_begin").size() == 1);
        CHECK(events(rows, @"command_abandon").size() == 1);
        CHECK(events(rows, @"command_commit_begin").empty());
        CHECK_FALSE(DT::commandTokenActive(first));
    }
}

TEST_CASE("almetal: pending command diagnostic IDs keep their creation session across restart",
          "[almetal][diagnostic-telemetry][diagnostic-token]")
{
    @autoreleasepool
    {
        Session first;
        first.start();
        std::string error;
        auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        auto old = device->makeCommandBuffer("Alchemy idle barrier", &error); REQUIRE(old);
        const auto old_token = old->diagnosticToken();
        REQUIRE(old_token.command_id == 1);
        const auto first_rows = first.rows(); // Deliberately keep the command unsubmitted/alive.
        CHECK(events(first_rows, @"command_abandon").empty());
        Session second;
        second.start();
        auto current = device->makeCommandBuffer("Alchemy idle barrier", &error); REQUIRE(current);
        const auto current_token = current->diagnosticToken();
        CHECK(current_token.command_id == old_token.command_id); // Numeric IDs can collide.
        CHECK(current_token.session_id != old_token.session_id);
        CHECK(old->diagnosticToken().session_id == old_token.session_id);
        CHECK_FALSE(DT::commandTokenActive(old->diagnosticToken()));
        CHECK(DT::commandTokenActive(current_token));
        const auto before = DT::statistics().attempted;
        DT::recordSessionMetrics(old_token.session_id, {}, "old_group_command", {});
        CHECK(DT::statistics().attempted == before);
        old.reset(); // Its old-session abandonment cannot modify the new counters.
        CHECK(DT::statistics().commands_abandoned == 0);
        current.reset();
        CHECK(DT::statistics().commands_abandoned == 1);
        const auto second_rows = second.rows();
        CHECK(events(second_rows, @"command_create_begin").size() == 1);
        CHECK(events(second_rows, @"command_abandon").size() == 1);
    }
}

TEST_CASE("almetal: diagnostic provenance numeric records and explicit nested worker scopes",
          "[almetal][diagnostic-telemetry]")
{
    Session session; session.start();
    DT::FrameTag main{41, DT::ViewKind::main, -1};
    {
        ALMetalDiagnosticFrameScope outer(main);
        CHECK(DT::currentFrameTag().frame_id == 41);
        {
            ALMetalDiagnosticFrameScope probe({41, DT::ViewKind::ordinary_probe, 2});
            DT::recordMarker("probe\"quoted", DT::currentFrameTag(), 7);
            CHECK(DT::currentFrameTag().view_index == 2);
        }
        CHECK(DT::currentFrameTag().view_kind == DT::ViewKind::main);
        std::atomic<bool> unknown{false}, explicit_tag{false};
        std::thread worker([&]
        {
            unknown.store(DT::currentFrameTag().frame_id == 0);
            DT::recordMarker("worker_unknown", DT::currentFrameTag());
            ALMetalDiagnosticFrameScope copied(main);
            explicit_tag.store(DT::currentFrameTag().frame_id == 41);
            DT::recordMetrics(DT::currentFrameTag(), "main_lighting", {3,4,2,1,0,0,0,0});
        });
        worker.join();
        REQUIRE(unknown); REQUIRE(explicit_tag);
        DT::FrameRecord f;
        f.iteration_id = 9; f.begin_display_frame = 40; f.end_display_frame = 41;
        f.start_mach_ns = DT::machTimeNs(); f.end_mach_ns = DT::machTimeNs();
        f.begin_startup_state = 2; f.end_startup_state = 3; f.begin_flags = 5; f.end_flags = 6;
        f.drawable_width = 64; f.drawable_height = 48; f.quality = 6;
        f.begin_teleport_state = 1; f.end_teleport_state = 2;
        f.outcome = DT::FrameOutcome::incomplete; f.native_exact = true;
        DT::recordFrame(f);
    }
    CHECK(DT::currentFrameTag().frame_id == 0);
    auto rows = session.rows();
    auto header = rows.front(); auto footer = rows.back();
    CHECK([header[@"source_sha"] isEqual:@"92ddb0966c21677956eab3ecfe8dbee44a9ab813"]);
    CHECK([header[@"cpu_clock"] isEqual:@"mach_absolute_time_ns"]);
    CHECK([header[@"gpu_clock"] isEqual:@"system_mach_seconds"]);
    CHECK(number(header,@"calibration_before_mach_ns") <= number(header,@"calibration_after_mach_ns"));
    CHECK(number(header,@"calibration_unix_ns") > 1'700'000'000'000'000'000ull);
    auto markers = events(rows,@"marker"); REQUIRE(markers.size() == 2);
    CHECK([markers[0][@"name"] isEqual:@"probe\"quoted"]);
    CHECK(number(markers[0],@"view_kind") == 3);
    CHECK(number(markers[0],@"view_index") == 2);
    CHECK(number(markers[1],@"frame_id") == 0);
    auto metrics = events(rows,@"metrics"); REQUIRE(metrics.size() == 1);
    CHECK(number(metrics[0],@"frame_id") == 41);
    CHECK(([metrics[0][@"values"] isEqual:@[@3,@4,@2,@1,@0,@0,@0,@0]]));
    auto frames = events(rows,@"frame"); REQUIRE(frames.size() == 1);
    CHECK(number(frames[0],@"iteration_id") == 9);
    CHECK(number(frames[0],@"begin_display_frame") == 40);
    CHECK(number(frames[0],@"end_display_frame") == 41);
    CHECK(number(frames[0],@"outcome") == 1);
    CHECK([frames[0][@"native_exact"] boolValue]);
    CHECK(number(footer,@"attempted") == 4);
    CHECK(number(footer,@"written") == 4);
    CHECK(number(footer,@"dropped_full") == 0);
    struct stat attributes{}; REQUIRE(::stat(session.config.path.c_str(), &attributes) == 0);
    CHECK((attributes.st_mode & 0777) == 0600);
}

TEST_CASE("almetal: diagnostic configuration refuses overwrite and invalid provenance",
          "[almetal][diagnostic-telemetry]")
{
    Session session; std::string error;
    auto invalid = session.config; invalid.source_sha = "not-a-source";
    CHECK_FALSE(DT::start(invalid, &error));
    CHECK_FALSE(std::filesystem::exists(session.config.path));
    session.start();
    CHECK_FALSE(DT::start(session.config, &error));
    DT::stop();
    auto bytes = std::filesystem::file_size(session.config.path);
    CHECK_FALSE(DT::start(session.config, &error));
    CHECK(std::filesystem::file_size(session.config.path) == bytes);
    CHECK_FALSE(DT::enabled());
}

TEST_CASE("almetal: diagnostic pressure is bounded lossy and visible in footer",
          "[almetal][diagnostic-telemetry]")
{
    Session session; session.config.ring_records = 2; session.config.writer_interval_ms = 100;
    session.start();
    std::array<std::thread,4> workers;
    for (auto& worker : workers) worker = std::thread([]
    {
        for (unsigned n = 0; n < 10000; ++n) DT::recordMetrics({}, "pressure", {n});
    });
    for (auto& worker : workers) worker.join();
    auto rows = session.rows(); auto footer = rows.back();
    CHECK(number(footer,@"attempted") == 40000);
    CHECK(number(footer,@"dropped_full") + number(footer,@"dropped_contention") > 0);
    CHECK(number(footer,@"queued") + number(footer,@"dropped_full") + number(footer,@"dropped_contention") == 40000);
    CHECK(number(footer,@"written") == number(footer,@"queued"));
}

TEST_CASE("almetal: diagnostic byte and duration limits stop independently of GPU progress",
          "[almetal][diagnostic-telemetry]")
{
    SECTION("byte limit")
    {
        Session session; session.config.max_bytes = 8192; session.config.writer_interval_ms = 1;
        session.start();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (DT::enabled() && std::chrono::steady_clock::now() < deadline)
            DT::recordMetrics({}, "bounded_bytes", {1,2,3,4,5,6,7,8});
        REQUIRE_FALSE(DT::enabled());
        auto rows = session.rows();
        CHECK([rows.back()[@"stop_reason"] isEqual:@"byte_limit"]);
        CHECK(number(rows.back(),@"dropped_budget") > 0);
        CHECK(std::filesystem::file_size(session.config.path) <= 8192);
    }
    SECTION("duration limit with pending command")
    {
        Session session; session.config.max_duration_seconds = 0.01; session.config.writer_interval_ms = 1;
        session.start();
        const auto command = DT::commandCreateBegin("private-asset-should-never-appear");
        DT::commandCreateEnd(command, true); DT::commandCommitBegin(command); DT::commandCommitEnd(command);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        REQUIRE_FALSE(DT::enabled());
        auto rows = session.rows();
        CHECK([rows.back()[@"stop_reason"] isEqual:@"duration_limit"]);
        CHECK(number(rows.back(),@"pending_committed_at_cutoff") == 1);
        CHECK(number(rows.back(),@"commands_completed") == 0);
        CHECK([events(rows,@"command_create_begin")[0][@"label"] isEqual:@"other_command"]);
        // A delayed completion must not reopen or append to the closed file.
        const auto bytes = std::filesystem::file_size(session.config.path);
        DT::commandCompleted(command, 4, 1.0, 2.0);
        CHECK(std::filesystem::file_size(session.config.path) == bytes);
    }
}

TEST_CASE("almetal: diagnostic names truncate and old sessions cannot contaminate a restart",
          "[almetal][diagnostic-telemetry]")
{
    Session first; first.start();
    auto previous = DT::commandCreateBegin("Alchemy idle barrier");
    DT::commandCreateEnd(previous, true);
    DT::stop();
    Session second; second.start();
    DT::commandCompleted(previous, 4, 1.0, 2.0);
    DT::commandWaitBegin(previous); DT::commandWaitEnd(previous, 4);
    std::string long_static_name(400, 'x');
    DT::recordMarker(long_static_name.c_str(), {});
    auto rows = second.rows();
    CHECK(number(rows.back(),@"commands_completed") == 0);
    CHECK(number(rows.back(),@"names_truncated") == 1);
    auto markers = events(rows,@"marker"); REQUIRE(markers.size() == 1);
    CHECK([markers[0][@"name"] length] == 95);
    CHECK(number(rows.front(),@"session_id") != previous.session_id);
    auto waits = events(rows,@"command_wait_begin"); REQUIRE(waits.size() == 1);
    CHECK(number(waits[0],@"command_id") == 0);
    CHECK(number(waits[0],@"frame_id") == 0);
}

TEST_CASE("almetal: native diagnostic completion preserves output and actual waits without timing mode",
          "[almetal][diagnostic-telemetry][diagnostic-native]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); REQUIRE(device);
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device->nativeHandle();
        id<MTLBuffer> output = [native_device newBufferWithLength:32 options:MTLResourceStorageModeShared]; REQUIRE(output);
        // Created before arming: its wait is visible with command_id0, not an
        // invented association with whatever display frame is current later.
        auto old = device->makeCommandBuffer("unobserved old command", &error); REQUIRE(old);
        const bool prior_timing = ALMetalDevice::commandTimingEnabled();
        ALMetalDevice::setCommandTimingEnabled(false);
        const auto timing_before = ALMetalDevice::commandTiming();
        Session session; session.start();
        std::unique_ptr<ALMetalCommandBuffer> command;
        {
            ALMetalDiagnosticFrameScope frame({101,DT::ViewKind::main,-1});
            command = device->makeCommandBuffer("Alchemy native world base lighting", &error); REQUIRE(command);
            auto native = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
            id<MTLBlitCommandEncoder> encoder = [native blitCommandEncoder]; REQUIRE(encoder);
            [encoder fillBuffer:output range:NSMakeRange(0,32) value:0x63]; [encoder endEncoding];
            REQUIRE(command->commit(&error));
            auto abandoned = device->makeCommandBuffer("must-not-expose-asset-identifier", &error); REQUIRE(abandoned);
        }
        {
            ALMetalDiagnosticFrameScope caller({202,DT::ViewKind::ordinary_probe,4});
            REQUIRE(command->waitUntilCompleted(&error));
            REQUIRE(old->commit(&error)); REQUIRE(old->waitUntilCompleted(&error));
        }
        awaitCompletions(1);
        for (unsigned i=0;i<32;++i) CHECK(static_cast<const unsigned char*>(output.contents)[i] == 0x63);
        auto rows = session.rows();
        auto complete = events(rows,@"command_complete"); REQUIRE(complete.size() == 1);
        CHECK(number(complete[0],@"frame_id") == 101);
        CHECK(number(complete[0],@"native_status") == 4);
        CHECK([complete[0][@"gpu_time_valid"] boolValue]);
        const double start = [complete[0][@"gpu_start_seconds"] doubleValue];
        const double end = [complete[0][@"gpu_end_seconds"] doubleValue];
        CHECK(start > 0); CHECK(end > start);
        CHECK(end * 1e9 <= double(number(complete[0],@"mach_ns")) + 1e6);
        auto waits = events(rows,@"command_wait_begin"); REQUIRE(waits.size() == 2);
        CHECK(number(waits[0],@"command_id") == number(complete[0],@"command_id"));
        CHECK(number(waits[0],@"frame_id") == 101);
        CHECK(number(waits[0],@"caller_frame_id") == 202);
        CHECK(number(waits[0],@"caller_view_index") == 4);
        CHECK(number(waits[1],@"command_id") == 0);
        CHECK(number(waits[1],@"frame_id") == 0);
        CHECK(events(rows,@"command_wait_end").size() == 2);
        CHECK(events(rows,@"command_abandon").size() == 1);
        CHECK(number(rows.back(),@"commands_committed") == 1);
        CHECK(number(rows.back(),@"commands_completed") == 1);
        CHECK(number(rows.back(),@"commands_abandoned") == 1);
        CHECK_FALSE(ALMetalDevice::commandTimingEnabled());
        CHECK(ALMetalDevice::commandTiming().submitted == timing_before.submitted);
        ALMetalDevice::setCommandTimingEnabled(prior_timing);
    }
}

TEST_CASE("almetal: native diagnostic completion survives wrapper retirement and gate delay",
          "[almetal][diagnostic-telemetry][diagnostic-native]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error); REQUIRE(device);
        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device->nativeHandle();
        id<MTLSharedEvent> gate = [native_device newSharedEvent]; REQUIRE(gate);
        Session session; session.start();
        __weak id<MTLCommandBuffer> weak_command;
        {
            ALMetalDiagnosticFrameScope frame({77,DT::ViewKind::shadow,3});
            auto command = device->makeCommandBuffer("Alchemy direct spot-shadow slice", &error); REQUIRE(command);
            id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
            weak_command = native;
            [native encodeWaitForEvent:gate value:1];
            // Bound a failed fixture; this is test-only GPU work, never a new
            // production synchronization primitive.
            id<MTLSharedEvent> rescue = gate;
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 3*NSEC_PER_SEC),
                dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0), ^{ rescue.signaledValue = 1; });
            REQUIRE(command->commit(&error));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        CHECK(DT::statistics().commands_completed == 0);
        CHECK(weak_command != nil);
        gate.signaledValue = 1;
        awaitCompletions(1);
        auto rows = session.rows();
        CHECK(events(rows,@"command_wait_begin").empty());
        CHECK(events(rows,@"command_complete").size() == 1);
        CHECK(number(events(rows,@"command_complete")[0],@"frame_id") == 77);
        CHECK(number(events(rows,@"command_complete")[0],@"view_kind") == 4);
        CHECK(number(rows.back(),@"pending_committed_at_cutoff") == 0);
        CHECK(number(rows.back(),@"commands_abandoned") == 0);
    }
}

TEST_CASE("almetal: contained diagnostic owner and settled cost observations",
          "[almetal][diagnostic-cost]")
{
    const char* directory = std::getenv("AL_METAL_DIAGNOSTIC_COST_DIRECTORY");
    if (!directory) { SUCCEED("Explicit private evidence directory enables this bounded cost observation"); return; }
    std::filesystem::create_directories(directory);
    std::ofstream raw(std::string(directory)+"/cost-iterations.jsonl", std::ios::app);
    REQUIRE(raw.good());
    std::string error;
    auto device = ALMetalDevice::create(&error); REQUIRE(device);
    id<MTLDevice> native_device = (__bridge id<MTLDevice>)device->nativeHandle();
    id<MTLBuffer> output = [native_device newBufferWithLength:4096 options:MTLResourceStorageModeShared]; REQUIRE(output);
    for (unsigned block=0;block<8;++block)
    {
        for (unsigned arm_index=0;arm_index<2;++arm_index)
        {
            const bool on = (block+arm_index)%2;
            Session session;
            if (on)
            {
                session.config.path = std::string(directory)+"/cost-native-"+std::to_string(block)+".jsonl";
                session.start();
            }
            for (unsigned iteration=0;iteration<96;++iteration)
            {
                @autoreleasepool
                {
                    const auto start = DT::machTimeNs();
                    ALMetalDiagnosticFrameScope frame({iteration+1,DT::ViewKind::main,-1});
                    auto command = device->makeCommandBuffer("Alchemy native world base lighting", &error); REQUIRE(command);
                    auto native = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
                    id<MTLBlitCommandEncoder> encoder = [native blitCommandEncoder]; REQUIRE(encoder);
                    [encoder fillBuffer:output range:NSMakeRange(0,4096) value:0x71]; [encoder endEncoding];
                    REQUIRE(command->commit(&error));
                    const auto submitted = DT::machTimeNs();
                    REQUIRE(command->waitUntilCompleted(&error)); // Contained settled oracle only.
                    const auto completed = DT::machTimeNs();
                    CHECK(static_cast<const unsigned char*>(output.contents)[0] == 0x71);
                    raw << "{\"block\":" << block << ",\"arm_index\":" << arm_index
                        << ",\"on\":" << (on?"true":"false") << ",\"iteration\":" << iteration
                        << ",\"warmup\":" << (iteration<16?"true":"false")
                        << ",\"owner_wall_ns\":" << submitted-start << ",\"settled_wall_ns\":" << completed-start << "}\n";
                }
            }
            if (on)
            {
                awaitCompletions(96); DT::stop();
                const auto s = DT::statistics();
                REQUIRE(s.io_errors == 0);
                raw << "{\"event\":\"arm_footer\",\"block\":" << block << ",\"on\":true,\"attempted\":" << s.attempted
                    << ",\"written\":" << s.written << ",\"dropped_full\":" << s.dropped_full
                    << ",\"dropped_contention\":" << s.dropped_contention << "}\n";
            }
        }
    }
    raw.flush(); REQUIRE(raw.good());
}
