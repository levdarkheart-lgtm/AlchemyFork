/**
 * @file alquietdiagnostics_controller_test.mm
 * @brief Actual quiet controller lifecycle, run one selector per fresh process.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../alquietdiagnostics.h"
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalframecapture.h"
#include "../../almetal/almetalresource.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace
{
namespace fs = std::filesystem;
namespace D = ALMetalDiagnosticTelemetry;

struct Environment
{
    const char* names[5]{"ALCHEMY_METAL_DIAGNOSTIC_DIRECTORY", "ALCHEMY_METAL_QUIET_CAPTURE",
        "ALCHEMY_METAL_DIAGNOSTIC_MAX_MIB",
        ALMetalFrameCapture::directory_environment, ALMetalFrameCapture::mode_environment};
    std::optional<std::string> previous[5];
    Environment()
    {
        for (unsigned i = 0; i < 5; ++i)
        {
            if (const char* value = std::getenv(names[i])) previous[i] = value;
            REQUIRE(unsetenv(names[i]) == 0);
        }
    }
    ~Environment()
    {
        for (unsigned i = 0; i < 5; ++i)
            if (previous[i]) setenv(names[i], previous[i]->c_str(), 1);
            else unsetenv(names[i]);
    }
};

struct Directory
{
    fs::path path;
    bool preserve = false;
    Directory()
    {
        fs::path parent = "/private/tmp";
        if (const char* requested = std::getenv("ALCHEMY_METAL_TEST_ARTIFACT_DIRECTORY");
            requested && *requested)
        {
            struct stat status{};
            REQUIRE(*requested == '/');
            REQUIRE(lstat(requested, &status) == 0);
            REQUIRE(S_ISDIR(status.st_mode));
            REQUIRE(status.st_uid == geteuid());
            REQUIRE((status.st_mode & 07777) == 0700);
            parent = requested;
            preserve = true;
        }
        std::string pattern = (parent / "alquiet-controller-test.XXXXXX").string();
        const char* result = mkdtemp(pattern.data());
        REQUIRE(result);
        path = result;
        REQUIRE(chmod(path.c_str(), 0700) == 0);
        REQUIRE(setenv("ALCHEMY_METAL_DIAGNOSTIC_DIRECTORY", path.c_str(), 1) == 0);
        REQUIRE(setenv("ALCHEMY_METAL_QUIET_CAPTURE", "auto", 1) == 0);
    }
    ~Directory()
    {
        if (preserve) std::printf("quiet_controller_retained_artifacts=%s\n", path.c_str());
        else { std::error_code ignored; fs::remove_all(path, ignored); }
    }
};

struct Shutdown
{
    std::uint64_t frame = 1;
    ~Shutdown() { alQuietDiagnosticsShutdown(frame); }
};

bool nativeRequested()
{
    const char* flag = std::getenv("ALCHEMY_METAL_RUN_QUIET_CONTROLLER_TEST");
    return flag && std::strcmp(flag, "1") == 0;
}

ALQuietDiagnosticDisplay quietNormalDisplay(std::uint64_t frame)
{
    ALQuietDiagnosticDisplay result;
    result.frame = frame;
    result.normal_display = result.world_ready = result.cadence_valid = true;
    result.width = result.height = 8;
    return result;
}

void reachFirstAttempt(ALMetalDevice& device, Shutdown& shutdown)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    // Real start-to-start cadence, with no generated GPU workload or fake clock.
    // This is a correctness fixture, never a frame-rate/performance benchmark.
    while (!alQuietDiagnosticsStatistics().attempts && std::chrono::steady_clock::now() < deadline)
    {
        alQuietDiagnosticsAdvanceDisplay(&device, quietNormalDisplay(++shutdown.frame));
        if (!alQuietDiagnosticsStatistics().attempts)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    REQUIRE(alQuietDiagnosticsStatistics().attempts == 1);
}
}

// The controller intentionally initializes ONCE per process, as the viewer
// does. Run each selector below in a separate process; do not combine filters.
TEST_CASE("quiet controller: startup byte budget is bounded and published in the actual header",
          "[.][quiet-controller-budget]")
{
    Environment environment;
    Directory directory;
    Shutdown shutdown;
    REQUIRE(setenv("ALCHEMY_METAL_QUIET_CAPTURE", "off", 1) == 0);
    const char* requested = std::getenv("ALCHEMY_METAL_TEST_DIAGNOSTIC_MAX_MIB");
    if (requested) REQUIRE(setenv("ALCHEMY_METAL_DIAGNOSTIC_MAX_MIB", requested, 1) == 0);
    const bool accepted = !requested || !*requested ||
        std::strcmp(requested, "64") == 0 || std::strcmp(requested, "00064") == 0 ||
        std::strcmp(requested, "65") == 0 || std::strcmp(requested, "72") == 0 ||
        std::strcmp(requested, "128") == 0 || std::strcmp(requested, "255") == 0 ||
        std::strcmp(requested, "256") == 0;
    alQuietDiagnosticsInitialize();
    CHECK(D::enabled() == accepted);
    alQuietDiagnosticsShutdown(shutdown.frame);
    const auto output = directory.path / "diagnostic.jsonl";
    CHECK(fs::exists(output) == accepted);
    if (accepted)
    {
        std::ifstream input(output);
        std::string header;
        REQUIRE(static_cast<bool>(std::getline(input, header)));
        const auto mib = requested && *requested ? std::stoull(requested) : 64ull;
        const auto bytes = mib * 1024 * 1024;
        CHECK(header.find("\"max_bytes\":" + std::to_string(bytes)) != std::string::npos);
        CHECK_FALSE(alQuietDiagnosticsStatistics().automatic);
    }
}

TEST_CASE("quiet controller: unset directory leaves the actual controller disabled",
          "[.][quiet-controller-off]")
{
    Environment environment;
    Shutdown shutdown;
    REQUIRE_FALSE(D::enabled());
    alQuietDiagnosticsInitialize();
    alQuietDiagnosticsAdvanceDisplay(nullptr, quietNormalDisplay(++shutdown.frame));
    alQuietDiagnosticsHeartbeat(shutdown.frame, true);
    const auto stats = alQuietDiagnosticsStatistics();
    CHECK_FALSE(stats.automatic);
    CHECK_FALSE(stats.started);
    CHECK_FALSE(stats.terminal);
    CHECK_FALSE(stats.capture_active);
    CHECK_FALSE(stats.capture_failed);
    CHECK(stats.attempts == 0);
    CHECK_FALSE(D::enabled());
}

TEST_CASE("quiet controller: actual early capture stops then heartbeat invalidates between captures",
          "[.][quiet-controller-native]")
{
    if (!nativeRequested()) return;
    @autoreleasepool
    {
        REQUIRE(std::getenv("MTL_CAPTURE_ENABLED"));
        REQUIRE(std::strcmp(std::getenv("MTL_CAPTURE_ENABLED"), "1") == 0);
        Environment environment;
        Directory directory;
        std::string error;
        auto device = ALMetalDevice::create(&error);
        REQUIRE(device);
        auto manager = [MTLCaptureManager sharedCaptureManager];
        REQUIRE([manager supportsDestination:MTLCaptureDestinationGPUTraceDocument]);
        REQUIRE_FALSE(manager.isCapturing);
        Shutdown shutdown;
        alQuietDiagnosticsInitialize();
        REQUIRE(D::enabled());
        REQUIRE(alQuietDiagnosticsStatistics().automatic);
        reachFirstAttempt(*device, shutdown);
        REQUIRE(alQuietDiagnosticsStatistics().capture_active);
        REQUIRE(manager.isCapturing);

        ALMetalBufferDescriptor descriptor;
        descriptor.length = 4;
        descriptor.storage = ALMetalStorageMode::shared;
        auto output = ALMetalBuffer::create(*device, descriptor, &error);
        REQUIRE(output);
        auto command = device->makeCommandBuffer("Quiet controller ordinary work", &error);
        REQUIRE(command);
        auto native_command = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
        auto blit = [native_command blitCommandEncoder];
        REQUIRE(blit);
        [blit fillBuffer:(__bridge id<MTLBuffer>)output->nativeHandle()
                   range:NSMakeRange(0,4) value:0x74];
        [blit endEncoding];
        REQUIRE(command->commit(&error));
        alQuietDiagnosticsAdvanceDisplay(device.get(), quietNormalDisplay(shutdown.frame));
        CHECK(alQuietDiagnosticsStatistics().capture_active); // Duplicate cannot stop.
        auto nested = quietNormalDisplay(++shutdown.frame);
        nested.normal_display = false;
        alQuietDiagnosticsAdvanceDisplay(device.get(), nested);
        CHECK(alQuietDiagnosticsStatistics().capture_active);
        alQuietDiagnosticsAdvanceDisplay(device.get(), quietNormalDisplay(++shutdown.frame));
        CHECK_FALSE(alQuietDiagnosticsStatistics().capture_active);
        CHECK_FALSE(manager.isCapturing);
        REQUIRE(command->waitUntilCompleted(&error)); // Test-only, after controller stop.
        CHECK(*static_cast<const unsigned*>(output->mappedBytes()) == 0x74747474u);
        CHECK(fs::exists(directory.path / ALMetalFrameCapture::quiet_early_directory /
                         ALMetalFrameCapture::trace_name));
        auto stats = alQuietDiagnosticsStatistics();
        CHECK(stats.started);
        CHECK_FALSE(stats.terminal);
        CHECK_FALSE(stats.capture_failed);
        CHECK(stats.attempts == 1);

        // The bug-catching boundary: there is NO active helper and NO display
        // entry carrying the teleport flag. Only the actual heartbeat sees it.
        alQuietDiagnosticsHeartbeat(shutdown.frame, true);
        stats = alQuietDiagnosticsStatistics();
        CHECK(stats.terminal);
        CHECK_FALSE(stats.capture_failed);
        CHECK_FALSE(stats.capture_active);
        CHECK(stats.attempts == 1);
        alQuietDiagnosticsAdvanceDisplay(device.get(), quietNormalDisplay(++shutdown.frame));
        CHECK(alQuietDiagnosticsStatistics().attempts == 1);
        CHECK_FALSE(alQuietDiagnosticsStatistics().capture_active);
        CHECK_FALSE(fs::exists(directory.path / ALMetalFrameCapture::quiet_late_directory));
    }
}

TEST_CASE("quiet controller: actual factory collision consumes one failed attempt without overwrite",
          "[.][quiet-controller-conflict]")
{
    if (!nativeRequested()) return;
    @autoreleasepool
    {
        Environment environment;
        Directory directory;
        const auto child = directory.path / ALMetalFrameCapture::quiet_early_directory;
        REQUIRE(fs::create_directory(child));
        const auto sentinel = child / "sentinel";
        { std::ofstream output(sentinel); output << "preserved"; }
        std::string error;
        auto device = ALMetalDevice::create(&error);
        REQUIRE(device);
        Shutdown shutdown;
        alQuietDiagnosticsInitialize();
        REQUIRE(D::enabled());
        reachFirstAttempt(*device, shutdown);
        const auto stats = alQuietDiagnosticsStatistics();
        CHECK(stats.terminal);
        CHECK(stats.capture_failed);
        CHECK_FALSE(stats.capture_active);
        CHECK_FALSE([MTLCaptureManager sharedCaptureManager].isCapturing);
        std::ifstream input(sentinel);
        std::string contents;
        input >> contents;
        CHECK(contents == "preserved");
        CHECK_FALSE(fs::exists(child / ALMetalFrameCapture::trace_name));
        for (unsigned i = 0; i < 4; ++i)
            alQuietDiagnosticsAdvanceDisplay(device.get(), quietNormalDisplay(++shutdown.frame));
        CHECK(alQuietDiagnosticsStatistics().attempts == 1);
        CHECK_FALSE(fs::exists(directory.path / ALMetalFrameCapture::quiet_late_directory));
    }
}

TEST_CASE("quiet controller: telemetry cutoff cancels an active capture without waiting for a display",
          "[.][quiet-controller-cutoff]")
{
    if (!nativeRequested()) return;
    @autoreleasepool
    {
        Environment environment;
        Directory directory;
        std::string error;
        auto device = ALMetalDevice::create(&error);
        REQUIRE(device);
        Shutdown shutdown;
        alQuietDiagnosticsInitialize();
        REQUIRE(D::enabled());
        reachFirstAttempt(*device, shutdown);
        REQUIRE(alQuietDiagnosticsStatistics().capture_active);
        REQUIRE([MTLCaptureManager sharedCaptureManager].isCapturing);
        ALMetalBufferDescriptor descriptor;
        descriptor.length = 4;
        descriptor.storage = ALMetalStorageMode::shared;
        auto output = ALMetalBuffer::create(*device, descriptor, &error);
        REQUIRE(output);
        auto command = device->makeCommandBuffer("Quiet controller cutoff ordinary work", &error);
        REQUIRE(command);
        auto native_command = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
        auto blit = [native_command blitCommandEncoder];
        REQUIRE(blit);
        [blit fillBuffer:(__bridge id<MTLBuffer>)output->nativeHandle()
                   range:NSMakeRange(0,4) value:0x75];
        [blit endEncoding];
        REQUIRE(command->commit(&error));
        D::stop(); // CPU writer cutoff only; no viewer display or GPU wait.
        REQUIRE_FALSE(D::enabled());
        alQuietDiagnosticsHeartbeat(shutdown.frame, false);
        const auto stats = alQuietDiagnosticsStatistics();
        CHECK_FALSE(stats.capture_active);
        CHECK(stats.terminal);
        CHECK(stats.capture_failed);
        CHECK(stats.attempts == 1);
        CHECK_FALSE([MTLCaptureManager sharedCaptureManager].isCapturing);
        REQUIRE(command->waitUntilCompleted(&error)); // Test-only preservation check.
        CHECK(*static_cast<const unsigned*>(output->mappedBytes()) == 0x75757575u);
        alQuietDiagnosticsAdvanceDisplay(device.get(), quietNormalDisplay(++shutdown.frame));
        CHECK(alQuietDiagnosticsStatistics().attempts == 1);
        CHECK_FALSE(alQuietDiagnosticsStatistics().capture_active);
        CHECK_FALSE(fs::exists(directory.path / ALMetalFrameCapture::quiet_late_directory));
    }
}
