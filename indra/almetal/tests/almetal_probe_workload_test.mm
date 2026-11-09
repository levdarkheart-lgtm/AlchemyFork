/**
 * @file almetal_probe_workload_test.mm
 * @brief Opt-in sparse probe records, same-command ordering and private output.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalprobeworkload.h"
#include "../almetalworld.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    struct Environment
    {
        std::optional<std::string> previous;
        bool timing = ALMetalDevice::commandTimingEnabled();
        Environment()
        {
            if (const char* value = std::getenv(ALMetalProbeWorkloadCapture::directory_environment)) previous = value;
            unsetenv(ALMetalProbeWorkloadCapture::directory_environment);
            ALMetalDevice::setCommandTimingEnabled(false);
        }
        ~Environment()
        {
            if (previous) setenv(ALMetalProbeWorkloadCapture::directory_environment, previous->c_str(), 1);
            else unsetenv(ALMetalProbeWorkloadCapture::directory_environment);
            ALMetalDevice::setCommandTimingEnabled(timing);
        }
    };
    struct Directory
    {
        fs::path path;
        Directory()
        {
            char pattern[] = "/private/tmp/almetal-probe-workload-test.XXXXXX";
            const char* created = mkdtemp(pattern);
            REQUIRE(created);
            path = created;
            REQUIRE(chmod(path.c_str(), 0700) == 0);
            REQUIRE(setenv(ALMetalProbeWorkloadCapture::directory_environment, path.c_str(), 1) == 0);
        }
        ~Directory() { std::error_code ignored; fs::remove_all(path, ignored); }
    };
    std::vector<char> readBytes(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        REQUIRE(input.good());
        return {std::istreambuf_iterator<char>(input), {}};
    }
    NSDictionary* readJSON(const fs::path& path)
    {
        NSData* data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:path.c_str()]];
        REQUIRE(data);
        NSError* error = nil;
        id object = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
        REQUIRE(error == nil);
        REQUIRE([object isKindOfClass:NSDictionary.class]);
        return object;
    }
    template<typename Predicate> bool eventually(Predicate predicate)
    {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < end)
        {
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return predicate();
    }
    struct Fixture
    {
        std::string error;
        std::unique_ptr<ALMetalDevice> device = ALMetalDevice::create(&error);
        std::unique_ptr<ALMetalTexture> depth, normal;
        ALMetalWorldBaseLightingConstants lighting{};
        ALMetalWorldReflectionProbeConstants probes{};
        ALMetalProbeWorkloadView view{};
        Fixture()
        {
            INFO(error);
            REQUIRE(device);
            for (unsigned i = 0; i < 4; ++i) view.projection[i * 5] = lighting.inverse_projection[i * 5] = 1.f;
            view.view_kind = 1;
            view.frame_index = 100;
            lighting.config[0] = 1;
            probes.config[0] = 2;
            probes.ref_sphere[1][0] = 3.25f;
        }
        void targets(ALMetalPixelFormat format)
        {
            ALMetalTextureDescriptor descriptor;
            descriptor.width = 130; descriptor.height = 82;
            descriptor.format = ALMetalPixelFormat::depth32_float_stencil8;
            descriptor.usage = ALMetalTextureUsage::shader_read | ALMetalTextureUsage::render_target;
            descriptor.label = "Probe workload test depth";
            depth = ALMetalTexture::create(*device, descriptor, &error);
            REQUIRE(depth);
            descriptor.format = format;
            descriptor.label = "Probe workload test normal";
            normal = ALMetalTexture::create(*device, descriptor, &error);
            REQUIRE(normal);
        }
        void clear(ALMetalCommandBuffer& command, double depth_value, double alpha)
        {
            @autoreleasepool
            {
                auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
                pass.colorAttachments[0].texture = (__bridge id<MTLTexture>)normal->nativeHandle();
                pass.colorAttachments[0].loadAction = MTLLoadActionClear;
                pass.colorAttachments[0].storeAction = MTLStoreActionStore;
                pass.colorAttachments[0].clearColor = MTLClearColorMake(.2,.3,.4,alpha);
                pass.depthAttachment.texture = (__bridge id<MTLTexture>)depth->nativeHandle();
                pass.depthAttachment.loadAction = MTLLoadActionClear;
                pass.depthAttachment.storeAction = MTLStoreActionStore;
                pass.depthAttachment.clearDepth = depth_value;
                pass.stencilAttachment.texture = pass.depthAttachment.texture;
                pass.stencilAttachment.loadAction = MTLLoadActionClear;
                pass.stencilAttachment.storeAction = MTLStoreActionStore;
                auto native = (__bridge id<MTLCommandBuffer>)command.nativeHandle();
                auto encoder = [native renderCommandEncoderWithDescriptor:pass];
                REQUIRE(encoder);
                [encoder endEncoding];
            }
        }
        void arm(ALMetalProbeWorkloadCapture& capture)
        {
            const auto buffers = alMetalResourceAllocationStats().buffers_created;
            auto command = device->makeCommandBuffer("Probe workload arming", &error);
            REQUIRE(command);
            ALMetalDevice::setCommandTimingEnabled(true);
            REQUIRE(capture.captureIfReady(*command, *depth, *normal, lighting, &probes, view, &error));
            REQUIRE(capture.stats().scheduled == 0);
            CHECK(alMetalResourceAllocationStats().buffers_created == buffers);
            ALMetalDevice::setCommandTimingEnabled(false);
        }
        fs::path record(const Directory& directory, const char* extension) const
        {
            return directory.path / ("probe-workload-" + std::to_string(getpid()) + '-' +
                std::to_string(view.frame_index) + '-' + std::to_string(view.view_kind) + '-' +
                std::to_string(view.face_index) + extension);
        }
    };
}

TEST_CASE("almetal: probe workload is absent by default and rejects nonprivate directories", "[almetal][probe-workload]")
{
    Environment environment;
    Fixture f;
    const auto before = alMetalResourceAllocationStats();
    REQUIRE_FALSE(ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error));
    CHECK(alMetalResourceAllocationStats().buffers_created == before.buffers_created);
    REQUIRE(setenv(ALMetalProbeWorkloadCapture::directory_environment, "relative", 1) == 0);
    REQUIRE_FALSE(ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error));
    CHECK_FALSE(f.error.empty());
    Directory directory;
    REQUIRE(chmod(directory.path.c_str(), 0755) == 0);
    REQUIRE_FALSE(ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error));
    REQUIRE(chmod(directory.path.c_str(), 0700) == 0);
    CHECK(fs::is_empty(directory.path));
}

TEST_CASE("almetal: sparse probe workload samples only after timing and actual GPU completion", "[almetal][probe-workload]")
{
    Environment environment;
    Directory directory;
    Fixture f;
    auto capture = ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error);
    INFO(f.error);
    REQUIRE(capture);
    for (unsigned iteration = 0; iteration < 3; ++iteration)
    {
        f.targets(iteration == 1 ? ALMetalPixelFormat::rgb10a2_unorm : ALMetalPixelFormat::rgba16_unorm);
        f.view.view_kind = iteration == 0 ? 1 : iteration == 1 ? 2 : 3;
        f.view.face_index = iteration;
        f.view.frame_index = 100 + iteration;
        f.lighting.surface[1] = iteration != 1;
        if (iteration == 0)
        {
            f.arm(*capture);
            CHECK(fs::is_empty(directory.path));
        }
        auto command = f.device->makeCommandBuffer("Probe workload same-command test", &f.error);
        REQUIRE(command);
        f.clear(*command, iteration == 2 ? 0.0 : .375, iteration == 0 ? .67 : 0.0);
        const auto expected_probes = f.probes;
        REQUIRE(capture->captureIfReady(*command, *f.depth, *f.normal, f.lighting, &f.probes, f.view, &f.error));
        REQUIRE(capture->captureIfReady(*command, *f.depth, *f.normal, f.lighting, &f.probes, f.view, &f.error));
        CHECK(capture->stats().scheduled == iteration + 1);
        CHECK_FALSE(fs::exists(f.record(directory, ".json")));
        f.probes.ref_sphere[1][0] += 1.f;
        f.depth.reset(); f.normal.reset();
        // Last capture must survive destruction of its CPU helper and both
        // source texture wrappers before submission/completion.
        if (iteration == 2) capture.reset();
        REQUIRE(command->commitAndWait(&f.error));
        if (capture)
        {
            REQUIRE(eventually([&] { return capture->stats().written == iteration + 1; }));
            CHECK(capture->stats().failed == 0);
        }
        else REQUIRE(eventually([&]
        {
            NSData* data = [NSData dataWithContentsOfFile:[NSString stringWithUTF8String:f.record(directory, ".json").c_str()]];
            return data && [NSJSONSerialization JSONObjectWithData:data options:0 error:nullptr] != nil;
        }));
        const auto binary_path = f.record(directory, ".bin");
        const auto json_path = f.record(directory, ".json");
        const auto data = readBytes(binary_path);
        REQUIRE(data.size() == 49616 + 2560 * 32);
        CHECK(std::memcmp(data.data(), "ALPRWK1", 7) == 0);
        CHECK(std::memcmp(data.data() + 400, &expected_probes, sizeof(expected_probes)) == 0);
        NSDictionary* json = readJSON(json_path);
        CHECK([json[@"status"] isEqualToString:@"complete"]);
        CHECK([json[@"gpu_completed"] boolValue]);
        CHECK([json[@"width"] unsignedIntValue] == 130);
        CHECK([json[@"height"] unsignedIntValue] == 82);
        struct Sample { std::uint32_t x,y; float depth,alpha; std::uint32_t kind,geometry,reserved[2]; };
        for (unsigned y = 0; y < 40; ++y) for (unsigned x = 0; x < 64; ++x)
        {
            Sample sample{};
            std::memcpy(&sample, data.data() + 49616 + (y * 64 + x) * 32, 32);
            CHECK(sample.x == (2*x+1)*130/128);
            CHECK(sample.y == (2*y+1)*82/80);
            CHECK(sample.depth == (iteration == 2 ? 0.f : .375f));
            CHECK(sample.kind == (iteration == 0 ? 1u : iteration == 1 ? 0u : 2u));
            CHECK(sample.geometry == (iteration != 2));
            CHECK(sample.reserved[0] == 0);
            CHECK(sample.reserved[1] == 0);
        }
        for (const auto& path : {binary_path, json_path})
        {
            struct stat status{};
            REQUIRE(stat(path.c_str(), &status) == 0);
            CHECK((status.st_mode & 0777) == 0600);
            CHECK(status.st_uid == geteuid());
        }
    }
}

TEST_CASE("almetal: abandoned probe workload commands emit no GPU sample bytes", "[almetal][probe-workload]")
{
    Environment environment;
    Directory directory;
    Fixture f;
    f.targets(ALMetalPixelFormat::rgba16_unorm);
    auto capture = ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error);
    REQUIRE(capture);
    f.arm(*capture);
    {
        auto command = f.device->makeCommandBuffer("Abandoned probe workload", &f.error);
        REQUIRE(command);
        f.clear(*command, .375, .67);
        REQUIRE(capture->captureIfReady(*command, *f.depth, *f.normal, f.lighting, &f.probes, f.view, &f.error));
    }
    auto poll = f.device->makeCommandBuffer("Probe workload completion polling", &f.error);
    REQUIRE(poll);
    ALMetalProbeWorkloadView ignored{};
    REQUIRE(capture->captureIfReady(*poll, *f.depth, *f.normal, f.lighting, &f.probes, ignored, &f.error));
    REQUIRE(eventually([&] { return capture->stats().cancelled == 1; }));
    CHECK_FALSE(fs::exists(f.record(directory, ".bin")));
    NSDictionary* json = readJSON(f.record(directory, ".json"));
    CHECK([json[@"status"] isEqualToString:@"cancelled"]);
    CHECK_FALSE([json[@"gpu_completed"] boolValue]);
}

TEST_CASE("almetal: probe workload output never replaces existing files", "[almetal][probe-workload]")
{
    Environment environment;
    Directory directory;
    Fixture f;
    f.targets(ALMetalPixelFormat::rgba16_unorm);
    const auto json_path = f.record(directory, ".json");
    { std::ofstream sentinel(json_path); sentinel << "existing private record"; }
    REQUIRE(chmod(json_path.c_str(), 0600) == 0);
    const auto original = readBytes(json_path);
    auto capture = ALMetalProbeWorkloadCapture::createIfRequested(*f.device, &f.error);
    REQUIRE(capture);
    f.arm(*capture);
    auto command = f.device->makeCommandBuffer("Probe workload no-overwrite test", &f.error);
    REQUIRE(command);
    f.clear(*command, .375, .67);
    REQUIRE(capture->captureIfReady(*command, *f.depth, *f.normal, f.lighting, &f.probes, f.view, &f.error));
    REQUIRE(command->commitAndWait(&f.error));
    REQUIRE(eventually([&] { return capture->stats().failed == 1; }));
    CHECK(readBytes(json_path) == original);
    CHECK_FALSE(fs::exists(f.record(directory, ".bin")));
}
