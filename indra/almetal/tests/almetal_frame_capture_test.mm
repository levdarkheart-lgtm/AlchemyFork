/**
 * @file almetal_frame_capture_test.mm
 * @brief Private-path policy and opt-in real-device display interval capture.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalframecapture.h"
#include "../almetaldevice.h"
#include "../almetalresource.h"
#include "../almetalpipeline.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    struct Environment
    {
        std::optional<std::string> directory;
        std::optional<std::string> capture;
        std::optional<std::string> mode;
        bool timing = ALMetalDevice::commandTimingEnabled();
        Environment()
        {
            if (const char* value = std::getenv(ALMetalFrameCapture::directory_environment)) directory = value;
            if (const char* value = std::getenv("MTL_CAPTURE_ENABLED")) capture = value;
            if (const char* value = std::getenv(ALMetalFrameCapture::mode_environment)) mode = value;
            unsetenv(ALMetalFrameCapture::directory_environment);
            unsetenv(ALMetalFrameCapture::mode_environment);
            ALMetalDevice::setCommandTimingEnabled(false);
        }
        ~Environment()
        {
            if (directory) setenv(ALMetalFrameCapture::directory_environment, directory->c_str(), 1);
            else unsetenv(ALMetalFrameCapture::directory_environment);
            if (capture) setenv("MTL_CAPTURE_ENABLED", capture->c_str(), 1);
            else unsetenv("MTL_CAPTURE_ENABLED");
            if (mode) setenv(ALMetalFrameCapture::mode_environment, mode->c_str(), 1);
            else unsetenv(ALMetalFrameCapture::mode_environment);
            ALMetalDevice::setCommandTimingEnabled(timing);
        }
    };

    struct Directory
    {
        fs::path path;
        Directory()
        {
            char pattern[] = "/private/tmp/almetal-frame-capture-test.XXXXXX";
            const char* created = mkdtemp(pattern);
            REQUIRE(created);
            path = created;
            REQUIRE(chmod(path.c_str(), 0700) == 0);
            REQUIRE(setenv(ALMetalFrameCapture::directory_environment, path.c_str(), 1) == 0);
        }
        ~Directory() { std::error_code ignored; fs::remove_all(path, ignored); }
        fs::path session() const { return path / ALMetalFrameCapture::session_directory; }
        fs::path trace() const { return session() / ALMetalFrameCapture::trace_name; }
    };

    std::vector<NSDictionary*> records(const Directory& directory)
    {
        std::ifstream input(directory.session() / ALMetalFrameCapture::metadata_name);
        REQUIRE(input.good());
        std::vector<NSDictionary*> result;
        for (std::string line; std::getline(input, line);)
        {
            NSData* data = [[NSString stringWithUTF8String:line.c_str()] dataUsingEncoding:NSUTF8StringEncoding];
            NSError* error = nil;
            id object = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
            REQUIRE(error == nil);
            REQUIRE([object isKindOfClass:NSDictionary.class]);
            CHECK([object[@"schema_version"] unsignedIntValue] == 1);
            CHECK([object[@"present_duration_measurement"] boolValue] == NO);
            CHECK([object[@"gpu_completion_waited"] boolValue] == NO);
            CHECK([object[@"scope"] isEqualToString:@"device_all_queues"]);
            result.push_back(object);
        }
        CHECK(result.size() <= 8);
        return result;
    }

    bool captureTestRequested()
    {
        const char* requested = std::getenv("ALCHEMY_METAL_RUN_FRAME_CAPTURE_TEST");
        return requested && std::strcmp(requested, "1") == 0;
    }

    void reachStart(ALMetalFrameCapture& capture, std::string& error)
    {
        using State = ALMetalFrameCapture::State;
        // No timing arm, even with eligibility, must never start capture.
        for (std::uint64_t frame = 1; frame <= 70; ++frame)
            REQUIRE(capture.advanceDisplayInterval(frame, true, &error));
        CHECK(capture.stats().state == State::waiting_for_timing);
        ALMetalDevice::setCommandTimingEnabled(true);
        REQUIRE(capture.advanceDisplayInterval(100, false, &error));
        CHECK(capture.stats().state == State::waiting_for_disarm);
        ALMetalDevice::setCommandTimingEnabled(false);
        REQUIRE(capture.advanceDisplayInterval(101, false, &error));
        CHECK(capture.stats().state == State::cooling_down);
        for (std::uint64_t frame = 102; frame <= 180; ++frame)
            REQUIRE(capture.advanceDisplayInterval(frame, false, &error));
        CHECK(capture.stats().eligible_intervals_waited == 0);
        for (std::uint64_t frame = 200; frame < 259; ++frame)
            REQUIRE(capture.advanceDisplayInterval(frame, true, &error));
        CHECK(capture.stats().eligible_intervals_waited == 59);
        CHECK(capture.stats().start_attempts == 0);
        REQUIRE(capture.advanceDisplayInterval(259, true, &error));
        INFO(error);
        REQUIRE(capture.stats().state == State::capturing);
        CHECK(capture.stats().start_attempts == 1);
        CHECK(capture.stats().start_frame == 259);
    }
}

TEST_CASE("almetal: frame capture defaults off and rejects unsafe or colliding output",
          "[almetal][frame-capture]")
{
    Environment environment;
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);
    const auto resources = alMetalResourceAllocationStats();
    error = "unchanged";
    CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
    CHECK(error == "unchanged");
    CHECK(alMetalResourceAllocationStats().buffers_created == resources.buffers_created);
    CHECK(alMetalResourceAllocationStats().textures_created == resources.textures_created);
    REQUIRE(setenv(ALMetalFrameCapture::directory_environment, "relative", 1) == 0);
    CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
    CHECK(error.find("absolute") != std::string::npos);

    Directory directory;
    REQUIRE(chmod(directory.path.c_str(), 0755) == 0);
    CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
    CHECK(error.find("mode0700") != std::string::npos);
    REQUIRE(chmod(directory.path.c_str(), 0700) == 0);
    const auto link = directory.path / "link";
    fs::create_directory_symlink(directory.path, link);
    REQUIRE(setenv(ALMetalFrameCapture::directory_environment, link.c_str(), 1) == 0);
    CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
    fs::remove(link);
    REQUIRE(setenv(ALMetalFrameCapture::directory_environment, directory.path.c_str(), 1) == 0);
    REQUIRE(unsetenv("MTL_CAPTURE_ENABLED") == 0);
    CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
    CHECK(error.find("process launch") != std::string::npos);
    CHECK(fs::is_empty(directory.path));

    // Collision policy runs before any capture-manager request. Preserve a
    // real sentinel to prove initialization never deletes/reuses old output.
    REQUIRE(fs::create_directory(directory.session()));
    const auto sentinel = directory.session() / "sentinel";
    { std::ofstream output(sentinel); output << "keep"; }
    CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
    CHECK(error.find("no overwrite") != std::string::npos);
    std::ifstream input(sentinel);
    std::string text;
    input >> text;
    CHECK(text == "keep");
}

// Run in a NEW process launched with MTL_CAPTURE_ENABLED=1 and the explicit
// ALCHEMY_METAL_RUN_FRAME_CAPTURE_TEST=1 flag. Hidden because frame capture
// adds instrumentation and produces a real trace; never run during timing.
// On the validated Xcode26.6/macOS26 host, MTL_SHADER_VALIDATION must be UNSET:
// native capture start fails with shader validation, but passes with no layers
// and with MTL_DEBUG_LAYER=1 alone. Validate shaders separately, capture OFF.
TEST_CASE("almetal: opted-in display interval captures a completed clear once",
          "[.][almetal][frame-capture-gpu]")
{
    if (!captureTestRequested())
    {
        WARN("Set ALCHEMY_METAL_RUN_FRAME_CAPTURE_TEST=1 and MTL_CAPTURE_ENABLED=1 at process launch");
        return;
    }
    @autoreleasepool
    {
        Environment environment;
        REQUIRE(environment.capture == std::optional<std::string>("1"));
        Directory directory;
        std::string error;
        auto device = ALMetalDevice::create(&error);
        REQUIRE(device);
        MTLCaptureManager* manager = [MTLCaptureManager sharedCaptureManager];
        if (![manager supportsDestination:MTLCaptureDestinationGPUTraceDocument])
        {
            CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
            CHECK(error.find("not supported") != std::string::npos);
            CHECK(fs::is_empty(directory.path));
            WARN("GPUTraceDocument unsupported; no real capture executed");
            return;
        }
        REQUIRE_FALSE(manager.isCapturing);
        auto capture = ALMetalFrameCapture::createIfRequested(*device, &error);
        INFO(error);
        REQUIRE(capture);
        CHECK(capture->mode() == ALMetalFrameCapture::Mode::display);
        struct stat status{};
        REQUIRE(stat(directory.session().c_str(), &status) == 0);
        CHECK((status.st_mode & 0777) == 0700);
        REQUIRE(stat((directory.session() / ALMetalFrameCapture::metadata_name).c_str(), &status) == 0);
        CHECK((status.st_mode & 0777) == 0600);
        CHECK_FALSE(fs::exists(directory.trace()));
        CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
        CHECK_FALSE(capture->selectedPassReady());
        CHECK_FALSE(capture->beginSelectedPass(1, &error));
        reachStart(*capture, error);
        REQUIRE(manager.isCapturing);
        CHECK_FALSE(capture->finishSelectedPass(259, &error));
        CHECK(manager.isCapturing); // A manual finish must not stop default display mode.

        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device->nativeHandle();
        MTLTextureDescriptor* texture_descriptor = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:8 height:8 mipmapped:NO];
        texture_descriptor.storageMode = MTLStorageModeShared;
        texture_descriptor.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> texture = [native_device newTextureWithDescriptor:texture_descriptor];
        REQUIRE(texture);
        texture.label = @"Frame capture regression clear";
        auto command = device->makeCommandBuffer("Frame capture regression clear command", &error);
        REQUIRE(command);
        auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(.25, .5, .75, 1);
        auto native_command = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
        auto encoder = [native_command renderCommandEncoderWithDescriptor:pass];
        REQUIRE(encoder);
        encoder.label = @"Frame capture regression clear encoder";
        [encoder endEncoding];
        REQUIRE(command->commit(&error));
        CHECK(capture->advanceDisplayInterval(260, false, &error));
        CHECK(manager.isCapturing); // Nested/ineligible display is not a stop.
        REQUIRE(capture->advanceDisplayInterval(261, true, &error));
        CHECK_FALSE(manager.isCapturing);
        CHECK(capture->stats().state == ALMetalFrameCapture::State::stopped);
        CHECK(capture->stats().stop_frame == 261);
        REQUIRE(command->waitUntilCompleted(&error)); // Test-only completion proof.
        std::array<unsigned char, 8 * 8 * 4> pixels{};
        [texture getBytes:pixels.data() bytesPerRow:8 * 4 fromRegion:MTLRegionMake2D(0,0,8,8) mipmapLevel:0];
        for (unsigned pixel = 0; pixel < 64; ++pixel)
        {
            CHECK(pixels[pixel * 4] == 64);
            CHECK(pixels[pixel * 4 + 1] == 128);
            CHECK(pixels[pixel * 4 + 2] == 191);
            CHECK(pixels[pixel * 4 + 3] == 255);
        }
        REQUIRE(fs::exists(directory.trace()));
        const auto output = records(directory);
        REQUIRE(output.size() == 5);
        CHECK([output.back()[@"status"] isEqualToString:@"stopped"]);
        CHECK([output.back()[@"start_frame"] unsignedLongLongValue] == 259);
        CHECK([output.back()[@"stop_frame"] unsignedLongLongValue] == 261);
        const auto metadata_size = fs::file_size(directory.session() / ALMetalFrameCapture::metadata_name);
        ALMetalDevice::setCommandTimingEnabled(true);
        REQUIRE(capture->advanceDisplayInterval(300, true, &error));
        ALMetalDevice::setCommandTimingEnabled(false);
        for (std::uint64_t frame = 301; frame < 380; ++frame)
            REQUIRE(capture->advanceDisplayInterval(frame, true, &error));
        CHECK(capture->stats().start_attempts == 1);
        capture.reset();
        CHECK_FALSE(manager.isCapturing);
        CHECK(fs::file_size(directory.session() / ALMetalFrameCapture::metadata_name) == metadata_size);
    }
}

TEST_CASE("almetal: opted-in capture respects another controller and stops its own abandoned interval",
          "[.][almetal][frame-capture-gpu]")
{
    if (!captureTestRequested())
    {
        WARN("Real frame capture fixture requires the explicit startup flags");
        return;
    }
    @autoreleasepool
    {
        Environment environment;
        REQUIRE(environment.capture == std::optional<std::string>("1"));
        Directory directory;
        std::string error;
        auto device = ALMetalDevice::create(&error);
        REQUIRE(device);
        MTLCaptureManager* manager = [MTLCaptureManager sharedCaptureManager];
        if (![manager supportsDestination:MTLCaptureDestinationGPUTraceDocument])
        {
            WARN("GPUTraceDocument unsupported; no real capture executed");
            return;
        }
        REQUIRE_FALSE(manager.isCapturing);
        auto capture = ALMetalFrameCapture::createIfRequested(*device, &error);
        INFO(error);
        REQUIRE(capture);

        SECTION("a later external capture is neither joined nor stopped")
        {
            MTLCaptureDescriptor* descriptor = [[MTLCaptureDescriptor alloc] init];
            descriptor.captureObject = (__bridge id<MTLDevice>)device->nativeHandle();
            descriptor.destination = MTLCaptureDestinationGPUTraceDocument;
            descriptor.outputURL = [NSURL fileURLWithPath:
                [NSString stringWithUTF8String:(directory.path / "external.gputrace").c_str()]];
            NSError* native_error = nil;
            REQUIRE([manager startCaptureWithDescriptor:descriptor error:&native_error]);
            struct StopExternal
            {
                MTLCaptureManager* manager;
                ~StopExternal() { if (manager.isCapturing) [manager stopCapture]; }
            } external{manager};
            // Factory rejection must also leave the external capture alone.
            Directory second_directory;
            CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
            CHECK(error.find("already capturing") != std::string::npos);
            CHECK(manager.isCapturing);
            ALMetalDevice::setCommandTimingEnabled(true);
            REQUIRE(capture->advanceDisplayInterval(1, false, &error));
            ALMetalDevice::setCommandTimingEnabled(false);
            REQUIRE(capture->advanceDisplayInterval(2, true, &error));
            for (std::uint64_t frame = 3; frame < 62; ++frame)
                REQUIRE(capture->advanceDisplayInterval(frame, true, &error));
            CHECK_FALSE(capture->advanceDisplayInterval(62, true, &error));
            CHECK(capture->stats().state == ALMetalFrameCapture::State::failed);
            CHECK(capture->stats().start_attempts == 0);
            CHECK(manager.isCapturing);
            capture.reset();
            CHECK(manager.isCapturing);
            CHECK_FALSE(fs::exists(directory.trace()));
            const auto output = records(directory);
            REQUIRE(output.size() == 4);
            CHECK([output.back()[@"status"] isEqualToString:@"capture_conflict"]);
        }

        SECTION("destruction stops only the helper's successfully started capture")
        {
            reachStart(*capture, error);
            auto command = device->makeCommandBuffer("Frame capture cancellation regression", &error);
            REQUIRE(command);
            ALMetalBufferDescriptor buffer_descriptor;
            buffer_descriptor.length = 4;
            buffer_descriptor.storage = ALMetalStorageMode::shared;
            buffer_descriptor.label = "Frame capture cancellation output";
            auto buffer = ALMetalBuffer::create(*device, buffer_descriptor, &error);
            REQUIRE(buffer);
            auto native_command = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
            auto encoder = [native_command blitCommandEncoder];
            REQUIRE(encoder);
            [encoder fillBuffer:(__bridge id<MTLBuffer>)buffer->nativeHandle() range:NSMakeRange(0,4) value:0x63];
            [encoder endEncoding];
            REQUIRE(command->commit(&error));
            capture.reset();
            CHECK_FALSE(manager.isCapturing);
            REQUIRE(command->waitUntilCompleted(&error));
            CHECK(static_cast<const unsigned char*>(buffer->mappedBytes())[0] == 0x63);
            REQUIRE(fs::exists(directory.trace()));
            const auto output = records(directory);
            REQUIRE(output.size() == 5);
            CHECK([output.back()[@"status"] isEqualToString:@"cancelled"]);
        }

        SECTION("a replaced output directory fails closed before capture starts")
        {
            const auto reserved = directory.path / "moved-original-session";
            fs::rename(directory.session(), reserved);
            REQUIRE(fs::create_directory(directory.session()));
            REQUIRE(chmod(directory.session().c_str(), 0700) == 0);
            { std::ofstream planted(directory.trace()); planted << "preserve-old-output"; }
            ALMetalDevice::setCommandTimingEnabled(true);
            REQUIRE(capture->advanceDisplayInterval(1, false, &error));
            ALMetalDevice::setCommandTimingEnabled(false);
            REQUIRE(capture->advanceDisplayInterval(2, true, &error));
            for (std::uint64_t frame = 3; frame < 62; ++frame)
                REQUIRE(capture->advanceDisplayInterval(frame, true, &error));
            CHECK_FALSE(capture->advanceDisplayInterval(62, true, &error));
            CHECK(error.find("path changed") != std::string::npos);
            CHECK(capture->stats().state == ALMetalFrameCapture::State::failed);
            CHECK(capture->stats().start_attempts == 0);
            CHECK_FALSE(manager.isCapturing);
            capture.reset();
            std::ifstream planted(directory.trace());
            std::string sentinel;
            planted >> sentinel;
            CHECK(sentinel == "preserve-old-output");
            CHECK(fs::exists(reserved / ALMetalFrameCapture::metadata_name));
        }
    }
}

TEST_CASE("almetal: selected capture mode preserves default-off and rejects invalid modes",
          "[almetal][frame-capture]")
{
    Environment environment;
    std::string error;
    auto device = ALMetalDevice::create(&error);
    REQUIRE(device);
    REQUIRE(setenv(ALMetalFrameCapture::mode_environment, "not-a-mode", 1) == 0);
    error = "unchanged";
    CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
    CHECK(error == "unchanged"); // No directory means no diagnostic, regardless of mode.

    Directory directory;
    REQUIRE(unsetenv("MTL_CAPTURE_ENABLED") == 0);
    for (const char* mode : {"not-a-mode", "MAIN-BASE", "HERO-FORWARD", "hero_forward", "1"})
    {
        CAPTURE(mode);
        REQUIRE(setenv(ALMetalFrameCapture::mode_environment, mode, 1) == 0);
        CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
        CHECK(error.find("mode") != std::string::npos);
        CHECK(fs::is_empty(directory.path));
    }
    for (const char* mode : {"display", "main-base", "hero-forward"})
    {
        CAPTURE(mode);
        REQUIRE(setenv(ALMetalFrameCapture::mode_environment, mode, 1) == 0);
        CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
        CHECK(error.find("process launch") != std::string::npos);
        CHECK(fs::is_empty(directory.path));
    }
    REQUIRE(unsetenv(ALMetalFrameCapture::mode_environment) == 0);
    CHECK_FALSE(ALMetalFrameCapture::createIfRequested(*device, &error));
    CHECK(error.find("process launch") != std::string::npos);
}

// These fixtures verify the real producing command and capture lifecycle.
// A trace file alone does NOT establish that Xcode replay or profiling succeeds.
// Launch in a fresh process with capture/test flags; shader validation UNSET.
TEST_CASE("almetal: selected pass modes capture a fresh consumer after an uncaptured producer",
          "[.][almetal][frame-capture-gpu]")
{
    if (!captureTestRequested())
    {
        WARN("Selected pass capture requires the explicit startup flags");
        return;
    }
    const char* selected_mode = GENERATE("main-base", "hero-forward");
    CAPTURE(selected_mode);
    const auto expected_mode = std::strcmp(selected_mode, "hero-forward") == 0
        ? ALMetalFrameCapture::Mode::hero_forward : ALMetalFrameCapture::Mode::main_base;
    NSString* expected_interval = expected_mode == ALMetalFrameCapture::Mode::hero_forward
        ? @"selected_hero_forward_face_1" : @"selected_main_base_pass";
    @autoreleasepool
    {
        Environment environment;
        REQUIRE(environment.capture == std::optional<std::string>("1"));
        REQUIRE(setenv(ALMetalFrameCapture::mode_environment, selected_mode, 1) == 0);
        Directory directory;
        std::string error;
        auto device = ALMetalDevice::create(&error);
        REQUIRE(device);
        MTLCaptureManager* manager = [MTLCaptureManager sharedCaptureManager];
        if (![manager supportsDestination:MTLCaptureDestinationGPUTraceDocument])
        {
            WARN("GPUTraceDocument unsupported; selected-pass capture was not executed");
            return;
        }
        REQUIRE_FALSE(manager.isCapturing);
        auto capture = ALMetalFrameCapture::createIfRequested(*device, &error);
        INFO(error);
        REQUIRE(capture);
        CHECK(capture->mode() == expected_mode);
        // Mode is a factory snapshot, not a mutable environment query.
        REQUIRE(setenv(ALMetalFrameCapture::mode_environment,
            expected_mode == ALMetalFrameCapture::Mode::hero_forward ? "main-base" : "hero-forward", 1) == 0);
        CHECK(capture->mode() == expected_mode);
        REQUIRE(setenv(ALMetalFrameCapture::mode_environment, selected_mode, 1) == 0);
        CHECK_FALSE(capture->selectedPassReady());
        CHECK_FALSE(capture->beginSelectedPass(1, &error));
        CHECK_FALSE(manager.isCapturing);
        CHECK(capture->stats().start_attempts == 0);

        const auto ready = [&]()
        {
            ALMetalDevice::setCommandTimingEnabled(true);
            REQUIRE(capture->advanceDisplayInterval(100, false, &error));
            ALMetalDevice::setCommandTimingEnabled(false);
            REQUIRE(capture->advanceDisplayInterval(101, false, &error));
            for (std::uint64_t frame = 200; frame < 259; ++frame)
                REQUIRE(capture->advanceDisplayInterval(frame, true, &error));
            CHECK(capture->stats().eligible_intervals_waited == 59);
            REQUIRE(capture->advanceDisplayInterval(259, true, &error));
            REQUIRE(capture->stats().state == ALMetalFrameCapture::State::ready_for_pass);
            REQUIRE(capture->selectedPassReady());
            CHECK(capture->stats().start_attempts == 0);
            CHECK_FALSE(manager.isCapturing); // Unlike the default display mode, no automatic start.
            REQUIRE(capture->advanceDisplayInterval(260, false, &error));
            CHECK(capture->selectedPassReady());
            CHECK_FALSE(manager.isCapturing);
        };
        ready();

        SECTION("timing rearm refuses manual start and requires another complete cooldown")
        {
            ALMetalDevice::setCommandTimingEnabled(true);
            CHECK_FALSE(capture->selectedPassReady());
            CHECK_FALSE(capture->beginSelectedPass(260, &error));
            CHECK_FALSE(manager.isCapturing);
            CHECK(capture->stats().start_attempts == 0);
            REQUIRE(capture->advanceDisplayInterval(261, false, &error));
            CHECK(capture->stats().state == ALMetalFrameCapture::State::waiting_for_disarm);
            ALMetalDevice::setCommandTimingEnabled(false);
            REQUIRE(capture->advanceDisplayInterval(262, true, &error));
            for (std::uint64_t frame = 263; frame < 322; ++frame)
                REQUIRE(capture->advanceDisplayInterval(frame, true, &error));
            CHECK_FALSE(capture->selectedPassReady());
            REQUIRE(capture->advanceDisplayInterval(322, true, &error));
            CHECK(capture->selectedPassReady());
            CHECK(capture->stats().start_attempts == 0);
            CHECK_FALSE(manager.isCapturing);
            REQUIRE(capture->beginSelectedPass(323, &error));
            ALMetalBufferDescriptor descriptor;
            descriptor.length = 4;
            descriptor.storage = ALMetalStorageMode::shared;
            auto output = ALMetalBuffer::create(*device, descriptor, &error);
            REQUIRE(output);
            auto command = device->makeCommandBuffer("Rearmed selected pass", &error);
            REQUIRE(command);
            auto native_command = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
            auto encoder = [native_command blitCommandEncoder];
            REQUIRE(encoder);
            [encoder fillBuffer:(__bridge id<MTLBuffer>)output->nativeHandle()
                         range:NSMakeRange(0, 4) value:0x62];
            [encoder endEncoding];
            REQUIRE(command->commit(&error));
            REQUIRE(capture->finishSelectedPass(323, &error));
            REQUIRE(command->waitUntilCompleted(&error));
            CHECK(static_cast<const unsigned char*>(output->mappedBytes())[0] == 0x62);
            CHECK(capture->stats().state == ALMetalFrameCapture::State::stopped);
            CHECK_FALSE(manager.isCapturing);
            const auto metadata = records(directory);
            CHECK(metadata.size() == 8);
            CHECK([metadata.back()[@"status"] isEqualToString:@"stopped"]);
        }

        SECTION("deferred start rechecks output collision without overwriting")
        {
            { std::ofstream planted(directory.trace()); planted << "preserve-selected-output"; }
            CHECK_FALSE(capture->beginSelectedPass(260, &error));
            CHECK(capture->stats().state == ALMetalFrameCapture::State::failed);
            CHECK(capture->stats().start_attempts == 0);
            CHECK_FALSE(manager.isCapturing);
            capture.reset();
            std::ifstream input(directory.trace());
            std::string sentinel;
            input >> sentinel;
            CHECK(sentinel == "preserve-selected-output");
        }

        SECTION("deferred start never joins or stops an external controller")
        {
            MTLCaptureDescriptor* descriptor = [[MTLCaptureDescriptor alloc] init];
            descriptor.captureObject = (__bridge id<MTLDevice>)device->nativeHandle();
            descriptor.destination = MTLCaptureDestinationGPUTraceDocument;
            descriptor.outputURL = [NSURL fileURLWithPath:
                [NSString stringWithUTF8String:(directory.path / "external-selected.gputrace").c_str()]];
            NSError* native_error = nil;
            const bool started =
                [manager startCaptureWithDescriptor:descriptor error:&native_error];
            INFO((native_error ? native_error.description.UTF8String : "no native error"));
            REQUIRE(started);
            struct StopExternal
            {
                MTLCaptureManager* manager;
                ~StopExternal() { if (manager.isCapturing) [manager stopCapture]; }
            } external{manager};
            CHECK_FALSE(capture->beginSelectedPass(260, &error));
            CHECK(capture->stats().state == ALMetalFrameCapture::State::failed);
            CHECK(capture->stats().start_attempts == 0);
            CHECK(manager.isCapturing);
            capture->finishSelectedPass(260, &error);
            CHECK(manager.isCapturing);
            capture.reset();
            CHECK(manager.isCapturing);
            CHECK_FALSE(fs::exists(directory.trace()));
        }

        SECTION("fresh captured consumer samples a previously committed private texture")
        {
            ALMetalTextureDescriptor descriptor;
            descriptor.width = descriptor.height = 8;
            descriptor.format = ALMetalPixelFormat::rgba8_unorm;
            descriptor.storage = ALMetalStorageMode::private_gpu;
            descriptor.usage = ALMetalTextureUsage::shader_read | ALMetalTextureUsage::render_target;
            descriptor.label = "Selected-pass pre-capture input";
            auto input = ALMetalTexture::create(*device, descriptor, &error);
            REQUIRE(input);
            auto library = ALMetalShaderLibrary::loadBuiltin(*device, &error);
            REQUIRE(library);
            ALMetalComputePipelineDescriptor pipeline_descriptor;
            pipeline_descriptor.library = library.get();
            pipeline_descriptor.function = "almetal_test_sample_texture";
            pipeline_descriptor.label = "Selected-pass sample pre-capture input";
            auto pipeline = ALMetalComputePipeline::create(*device, pipeline_descriptor, &error);
            REQUIRE(pipeline);
            ALMetalBufferDescriptor result_descriptor;
            result_descriptor.length = 4 * sizeof(float);
            result_descriptor.storage = ALMetalStorageMode::shared;
            result_descriptor.label = "Selected-pass sampled output";
            auto result = ALMetalBuffer::create(*device, result_descriptor, &error);
            REQUIRE(result);

            // This producer is CREATED and COMMITTED before capture, without a CPU wait.
            auto producer = device->makeCommandBuffer("Selected-pass uncaptured producer", &error);
            REQUIRE(producer);
            auto native_producer = (__bridge id<MTLCommandBuffer>)producer->nativeHandle();
            auto clear = [MTLRenderPassDescriptor renderPassDescriptor];
            clear.colorAttachments[0].texture = (__bridge id<MTLTexture>)input->nativeHandle();
            clear.colorAttachments[0].loadAction = MTLLoadActionClear;
            clear.colorAttachments[0].storeAction = MTLStoreActionStore;
            clear.colorAttachments[0].clearColor = MTLClearColorMake(64.0 / 255.0, 128.0 / 255.0, 192.0 / 255.0, 1.0);
            auto clear_encoder = [native_producer renderCommandEncoderWithDescriptor:clear];
            REQUIRE(clear_encoder);
            [clear_encoder endEncoding];
            REQUIRE(producer->commit(&error));
            REQUIRE_FALSE(manager.isCapturing);

            REQUIRE(capture->beginSelectedPass(260, &error));
            REQUIRE(manager.isCapturing);
            CHECK(capture->stats().start_frame == 260);
            CHECK(capture->stats().start_attempts == 1);
            auto consumer = device->makeCommandBuffer("Selected-pass fresh captured consumer", &error);
            REQUIRE(consumer);
            auto encoder = ALMetalComputeEncoder::begin(*consumer, "Selected-pass sampled consumer", &error);
            REQUIRE(encoder);
            REQUIRE(encoder->setPipeline(*pipeline, &error));
            REQUIRE(encoder->setTexture(0, input.get(), &error));
            REQUIRE(encoder->setBuffer(0, *result, 0, &error));
            REQUIRE(encoder->dispatch(1, 1, 1, 1, 1, 1, &error));
            REQUIRE(encoder->end(&error));
            REQUIRE(consumer->commit(&error));
            REQUIRE(capture->finishSelectedPass(260, &error));
            CHECK_FALSE(manager.isCapturing);
            CHECK(capture->stats().state == ALMetalFrameCapture::State::stopped);
            CHECK(capture->stats().stop_frame == 260);
            CHECK_FALSE(capture->selectedPassReady());
            REQUIRE(consumer->waitUntilCompleted(&error)); // Test-only data proof after capture stopped.
            REQUIRE(producer->waitUntilCompleted(&error));
            std::array<float, 4> sampled{};
            REQUIRE(result->read(0, sampled.data(), sizeof(sampled), &error));
            const std::array<float, 4> expected{{64.0f / 255.0f, 128.0f / 255.0f, 192.0f / 255.0f, 1.f}};
            for (std::size_t channel = 0; channel < 4; ++channel)
                CHECK(std::abs(sampled[channel] - expected[channel]) < 0.00001f);
            REQUIRE(fs::exists(directory.trace()));
            const auto metadata = records(directory);
            REQUIRE_FALSE(metadata.empty());
            CHECK([metadata.back()[@"status"] isEqualToString:@"stopped"]);
            CHECK([metadata.back()[@"interval"] isEqualToString:expected_interval]);
            CHECK([metadata.back()[@"includes_intervening_idle_uploads"] boolValue] == YES);
            const auto metadata_bytes = fs::file_size(directory.session() / ALMetalFrameCapture::metadata_name);
            CHECK_FALSE(capture->beginSelectedPass(261, &error));
            REQUIRE(capture->finishSelectedPass(261, &error));
            REQUIRE(capture->advanceDisplayInterval(262, true, &error));
            CHECK(capture->stats().start_attempts == 1);
            CHECK_FALSE(manager.isCapturing);
            CHECK(fs::file_size(directory.session() / ALMetalFrameCapture::metadata_name) == metadata_bytes);
        }

        SECTION("abandoned selected capture is stopped only by its owner")
        {
            REQUIRE(capture->beginSelectedPass(260, &error));
            REQUIRE(manager.isCapturing);
            ALMetalBufferDescriptor descriptor;
            descriptor.length = 4;
            descriptor.storage = ALMetalStorageMode::shared;
            descriptor.label = "Selected-pass cancellation result";
            auto result = ALMetalBuffer::create(*device, descriptor, &error);
            REQUIRE(result);
            auto command = device->makeCommandBuffer("Selected-pass cancellation command", &error);
            REQUIRE(command);
            auto native_command = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
            auto encoder = [native_command blitCommandEncoder];
            REQUIRE(encoder);
            [encoder fillBuffer:(__bridge id<MTLBuffer>)result->nativeHandle()
                         range:NSMakeRange(0, 4) value:0x79];
            [encoder endEncoding];
            REQUIRE(command->commit(&error));
            SECTION("destruction cancels its successfully started interval")
            {
                capture.reset();
                CHECK_FALSE(manager.isCapturing);
            }
            SECTION("next eligible display rejects a selected interval left open")
            {
                REQUIRE(capture->advanceDisplayInterval(261, false, &error));
                CHECK(manager.isCapturing); // Nested/ineligible does not truncate.
                CHECK_FALSE(capture->advanceDisplayInterval(262, true, &error));
                CHECK(capture->stats().state == ALMetalFrameCapture::State::failed);
                CHECK_FALSE(manager.isCapturing);
                CHECK_FALSE(capture->finishSelectedPass(262, &error));
            }
            SECTION("an unsuccessful selected pass is recorded as cancelled")
            {
                CHECK_FALSE(capture->finishSelectedPass(260, &error, false));
                CHECK(capture->stats().state == ALMetalFrameCapture::State::failed);
                CHECK_FALSE(manager.isCapturing);
            }
            REQUIRE(command->waitUntilCompleted(&error));
            CHECK(static_cast<const unsigned char*>(result->mappedBytes())[0] == 0x79);
            REQUIRE(fs::exists(directory.trace()));
            const auto metadata = records(directory);
            REQUIRE_FALSE(metadata.empty());
            CHECK([metadata.back()[@"status"] isEqualToString:@"cancelled"]);
            CHECK([metadata.back()[@"interval"] isEqualToString:expected_interval]);
        }
    }
}
