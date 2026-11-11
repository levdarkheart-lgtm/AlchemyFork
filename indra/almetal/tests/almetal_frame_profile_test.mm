/**
 * @file almetal_frame_profile_test.mm
 * @brief Physical GPU frame-profile samples, attribution and lifetime tests.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "../almetalstagetiming.h"
#include "../almetalpipeline.h"
#include "../almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <vector>

namespace
{
const char* source = R"metal(
#include <metal_stdlib>
using namespace metal;
vertex float4 profile_vertex(uint i [[vertex_id]]) {
    const float2 p[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
    return float4(p[i],0,1);
}
fragment float4 profile_red() { return float4(1,0,0,1); }
fragment float4 profile_blue() { return float4(0,0,1,1); }
kernel void profile_compute(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
    out[i] = i * 3 + 17;
}
)metal";

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> red, blue;
    std::unique_ptr<ALMetalComputePipeline> compute;
    std::unique_ptr<ALMetalRenderTarget> target;
    std::unique_ptr<ALMetalBuffer> output;
    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        library = ALMetalShaderLibrary::compileSource(*device, source, "Physical frame profile", &error);
        INFO(error); REQUIRE(library);
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get(); descriptor.vertex_function = "profile_vertex";
        descriptor.fragment_function = "profile_red";
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        red = ALMetalRenderPipeline::create(*device, descriptor, &error); REQUIRE(red);
        descriptor.fragment_function = "profile_blue";
        blue = ALMetalRenderPipeline::create(*device, descriptor, &error); REQUIRE(blue);
        ALMetalComputePipelineDescriptor kernel;
        kernel.library = library.get(); kernel.function = "profile_compute";
        compute = ALMetalComputePipeline::create(*device, kernel, &error); REQUIRE(compute);
        ALMetalRenderTargetDescriptor target_descriptor;
        target_descriptor.width = target_descriptor.height = 64;
        target_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        target = ALMetalRenderTarget::create(*device, target_descriptor, &error); REQUIRE(target);
        ALMetalBufferDescriptor buffer;
        buffer.length = 64 * sizeof(std::uint32_t); buffer.storage = ALMetalStorageMode::shared;
        output = ALMetalBuffer::create(*device, buffer, &error); REQUIRE(output);
    }
    std::unique_ptr<ALMetalCommandBuffer> command()
    {
        auto result = device->makeCommandBuffer("Profile output invariant", &error);
        INFO(error); REQUIRE(result); return result;
    }
    void render(ALMetalCommandBuffer& command)
    {
        auto pass = target->makeRenderPass({}, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(command, *pass, "Mixed physical render encoder", &error);
        REQUIRE(encoder);
        REQUIRE(encoder->setViewport({0,0,64,64,0,1}, &error));
        REQUIRE(encoder->setScissor({0,0,64,64}, &error));
        REQUIRE(encoder->setPipeline(*red, &error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        REQUIRE(encoder->setPipeline(*blue, &error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        // Repeated bindings must not invent additional shader measurements.
        REQUIRE(encoder->setPipeline(*blue, &error));
        REQUIRE(encoder->end(&error));
    }
    void dispatch(ALMetalCommandBuffer& command)
    {
        auto encoder = ALMetalComputeEncoder::begin(command, "Physical compute encoder", &error);
        REQUIRE(encoder); REQUIRE(encoder->setPipeline(*compute, &error));
        REQUIRE(encoder->setBuffer(0, *output, 0, &error));
        REQUIRE(encoder->dispatch(64, 1, 1, 32, 1, 1, &error));
        REQUIRE(encoder->end(&error));
    }
    std::vector<std::uint8_t> pixels()
    {
        std::vector<std::uint8_t> bytes(64 * 64 * 4);
        REQUIRE(target->colorTexture(0)->readback(*device, 0, 0, {}, bytes.data(), 256, &error));
        return bytes;
    }
    void checkCompute()
    {
        std::array<std::uint32_t,64> bytes{};
        REQUIRE(output->read(0, bytes.data(), sizeof(bytes), &error));
        for (unsigned i = 0; i < bytes.size(); ++i) CHECK(bytes[i] == i * 3 + 17);
    }
};
}

TEST_CASE("Native manual profile measures physical mixed render and compute encoders",
          "[almetal][frame-profile]")
{
    @autoreleasepool
    {
        Fixture f;
        auto baseline = f.command(); f.render(*baseline); f.dispatch(*baseline);
        REQUIRE(baseline->commitAndWait(&f.error));
        const auto expected = f.pixels(); f.checkCompute();
        for (std::size_t i = 0; i < expected.size(); i += 4)
        { CHECK(expected[i] == 0); CHECK(expected[i+1] == 0); CHECK(expected[i+2] == 255); CHECK(expected[i+3] == 255); }
        const auto capabilities = ALMetalStageTimingWindow::capabilities(*f.device);
        CHECK(capabilities.stage_boundary); CHECK(capabilities.timestamps);
        if (f.device->info().name == "Apple M1 Max")
        { CHECK_FALSE(capabilities.draw_boundary); CHECK_FALSE(capabilities.dispatch_boundary); CHECK_FALSE(capabilities.statistics); }
        auto window = ALMetalStageTimingWindow::create(*f.device, 4, &f.error, true);
        INFO(f.error); REQUIRE(window);
        auto command = f.command();
        {
            ALMetalStageTimingScope scope(window.get(), 77, 1, -1);
            f.render(*command);
            { ALMetalStageTimingScope hero(window.get(), 77, 2, 3); f.dispatch(*command); }
            CHECK(ALMetalStageTimingScope::current() == &scope);
        }
        CHECK(ALMetalStageTimingScope::current() == nullptr);
        REQUIRE(command->commit(&f.error));
        REQUIRE(window->seal(&f.error));
        std::vector<ALMetalStageTimingResult> reports;
        REQUIRE(window->poll(reports, &f.error)); // Never blocks for GPU completion.
        REQUIRE(command->waitUntilCompleted(&f.error));
        std::vector<ALMetalStageTimingResult> remaining;
        REQUIRE(window->poll(remaining, &f.error));
        for (auto& result : remaining) reports.push_back(std::move(result));
        REQUIRE(window->isFinished()); REQUIRE(reports.size() == 2);
        const auto& render = reports[0]; const auto& compute = reports[1];
        CHECK_FALSE(render.compute); CHECK(compute.compute);
        CHECK(render.tag.view_kind == 1); CHECK(compute.tag.view_kind == 2); CHECK(compute.tag.face == 3);
        REQUIRE(render.pipelines.size() == 2); REQUIRE(compute.pipelines.size() == 1);
        CHECK(render.pipelines[0].vertex == "profile_vertex");
        CHECK(render.pipelines[0].fragment == "profile_red");
        CHECK(render.pipelines[1].fragment == "profile_blue");
        CHECK(render.pipelines[0].native_pipeline == reinterpret_cast<std::uintptr_t>(f.red->nativeHandle()));
        CHECK(render.pipelines[1].native_pipeline == reinterpret_cast<std::uintptr_t>(f.blue->nativeHandle()));
        CHECK(compute.pipelines[0].compute == "profile_compute");
        CHECK(compute.pipelines[0].native_pipeline == reinterpret_cast<std::uintptr_t>(f.compute->nativeHandle()));
        // One interval per physical encoder, never a copy per bound pipeline.
        for (const auto& result : reports)
        {
            CHECK(result.status == ALMetalStageTimingStatus::complete);
            REQUIRE(result.raw_samples); REQUIRE(result.calibration); REQUIRE(result.elapsed);
            if (result.compute)
            { CHECK(result.elapsed->compute_ns > 0); CHECK(result.elapsed->vertex_ns == 0); CHECK(result.elapsed->fragment_ns == 0); }
            else
            { CHECK(result.elapsed->vertex_ns > 0); CHECK(result.elapsed->fragment_ns > 0); CHECK(result.elapsed->compute_ns == 0); }
        }
        CHECK(window->rejectedPrepareCount() == 0);
        CHECK(f.pixels() == expected); f.checkCompute();
    }
}

TEST_CASE("Native profile cancellation retains command samples and restores caller scopes",
          "[almetal][frame-profile]")
{
    @autoreleasepool
    {
        Fixture f;
        auto window = ALMetalStageTimingWindow::create(*f.device, 1, &f.error, true); REQUIRE(window);
        auto command = f.command();
        {
            ALMetalStageTimingScope scope(window.get(), 88, 1, -1);
            { ALMetalStageTimingScope suppressed(nullptr, 0, 0, -1); f.render(*command); }
            CHECK(window->preparedPassCount() == 0);
            f.dispatch(*command);
            f.render(*command); // Limit rejection is diagnostic-only.
            CHECK(window->preparedPassCount() == 1); CHECK(window->rejectedPrepareCount() == 1);
        }
        CHECK(ALMetalStageTimingScope::current() == nullptr);
        REQUIRE(window->seal(&f.error));
        std::vector<ALMetalStageTimingResult> reports;
        REQUIRE(window->poll(reports, &f.error)); REQUIRE(reports.size() == 1);
        CHECK(reports[0].compute);
        CHECK(reports[0].status == ALMetalStageTimingStatus::unsubmitted_at_seal);
        CHECK_FALSE(reports[0].elapsed); CHECK_FALSE(reports[0].raw_samples);
        window.reset();
        // Cancellation discards admission/results, not the command's sample
        // allocation or normal output. Commit only after destroying the window.
        REQUIRE(command->commitAndWait(&f.error)); f.checkCompute();
        const auto pixels = f.pixels();
        CHECK(pixels[0] == 0); CHECK(pixels[2] == 255); CHECK(pixels[3] == 255);
        auto formal = ALMetalStageTimingWindow::create(*f.device, 1, &f.error); REQUIRE(formal);
        auto formal_command = f.command();
        { ALMetalStageTimingScope scope(formal.get(), 99, 1, -1); f.dispatch(*formal_command); }
        CHECK(formal->preparedPassCount() == 0); // Existing formal scope unchanged.
        REQUIRE(formal_command->commitAndWait(&f.error)); f.checkCompute();
    }
}
