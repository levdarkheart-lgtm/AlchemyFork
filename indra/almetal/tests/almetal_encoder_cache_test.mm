/**
 * @file almetal_encoder_cache_test.mm
 * @brief Real-device regressions for encoder-local native state caching.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "../almetaldevice.h"
#include "../almetalpipeline.h"
#include "../almetalresource.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace
{
    const char* cacheTestSource = R"metal(
#include <metal_stdlib>
using namespace metal;
struct CacheVertex { float4 position [[position]]; float4 value; };
vertex CacheVertex cache_vertex(uint id [[vertex_id]],
    constant float4& value [[buffer(0)]])
{
    const float2 points[3] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
    return {float4(points[id], 0.5f, 1.f), value};
}
fragment float4 cache_color(CacheVertex in [[stage_in]],
    constant float4& color [[buffer(0)]]) { return in.value * color; }
fragment float4 cache_inverse(CacheVertex in [[stage_in]],
    constant float4& color [[buffer(0)]]) { return 1.f - in.value * color; }
[[patch(quad, 4)]]
vertex CacheVertex cache_patch(float2 uv [[position_in_patch]])
{
    return {float4(uv * 2.f - 1.f, 0.5f, 1.f), float4(1.f)};
}
)metal";

    std::unique_ptr<ALMetalRenderPipeline> cachePipeline(
        const ALMetalDevice& device, const ALMetalShaderLibrary& library,
        const char* vertex, const char* fragment, bool tessellated,
        std::string& error)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = &library;
        descriptor.vertex_function = vertex;
        descriptor.fragment_function = fragment;
        descriptor.label = std::string("Encoder cache ") + vertex + "/" + fragment;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.tessellation.enabled = tessellated;
        descriptor.tessellation.maximum_factor = tessellated ? 4 : 1;
        return ALMetalRenderPipeline::create(device, descriptor, &error);
    }
}

TEST_CASE("almetal: encoder caches preserve buffer bytes offsets and raster transitions",
          "[almetal][encoder-cache]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        auto library = ALMetalShaderLibrary::compileSource(
            *device, cacheTestSource, "Encoder cache regression", &error);
        INFO(error);
        REQUIRE(library);
        auto pipeline = cachePipeline(*device, *library,
            "cache_vertex", "cache_color", false, error);
        auto inverse = cachePipeline(*device, *library,
            "cache_vertex", "cache_inverse", false, error);
        REQUIRE(pipeline);
        REQUIRE(inverse);
        ALMetalDepthStencilDescriptor depth_descriptor;
        depth_descriptor.depth_compare = ALMetalCompareFunction::always;
        depth_descriptor.depth_write_enabled = false;
        depth_descriptor.label = "Encoder cache disabled depth state";
        auto depth = ALMetalDepthStencilState::create(*device, depth_descriptor, &error);
        REQUIRE(depth);

        ALMetalBufferDescriptor buffer_descriptor;
        buffer_descriptor.length = 512;
        buffer_descriptor.label = "Encoder cache argument values";
        auto vertex_values = ALMetalBuffer::create(*device, buffer_descriptor, &error);
        auto colors = ALMetalBuffer::create(*device, buffer_descriptor, &error);
        REQUIRE(vertex_values);
        REQUIRE(colors);
        const std::array<float, 4> one = {1.f, 1.f, 1.f, 1.f};
        const std::array<float, 4> half = {0.5f, 0.5f, 0.5f, 1.f};
        const std::array<float, 4> quarter = {0.25f, 0.25f, 0.25f, 1.f};
        const std::array<float, 4> red = {1.f, 0.f, 0.f, 1.f};
        const std::array<float, 4> green = {0.f, 1.f, 0.f, 1.f};
        const std::array<float, 4> blue = {0.f, 0.f, 1.f, 1.f};
        REQUIRE(vertex_values->write(0, one.data(), sizeof(one), &error));
        REQUIRE(vertex_values->write(256, quarter.data(), sizeof(quarter), &error));
        REQUIRE(colors->write(0, red.data(), sizeof(red), &error));
        REQUIRE(colors->write(256, blue.data(), sizeof(blue), &error));

        ALMetalRenderTargetDescriptor target_descriptor;
        target_descriptor.width = 16;
        target_descriptor.height = 4;
        target_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        target_descriptor.label = "Encoder cache transition pixels";
        auto target = ALMetalRenderTarget::create(*device, target_descriptor, &error);
        REQUIRE(target);
        auto pass = target->makeRenderPass({}, &error);
        auto command = device->makeCommandBuffer("Encoder cache transitions", &error);
        REQUIRE(pass);
        REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Cache pass", &error);
        REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline, &error));
        REQUIRE(encoder->setPipeline(*pipeline, &error));
        const ALMetalViewport viewport{0., 0., 16., 4., 0., 1.};
        REQUIRE(encoder->setViewport(viewport, &error));
        REQUIRE(encoder->setViewport(viewport, &error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
        REQUIRE(encoder->setTriangleFillMode(ALMetalTriangleFillMode::fill, &error));
        REQUIRE(encoder->setDepthStencilState(depth.get(), &error));
        REQUIRE(encoder->setDepthStencilState(depth.get(), &error));
        REQUIRE(encoder->setDepthBias(0.f, 0.f, 0.f, &error));
        REQUIRE(encoder->setDepthBias(0.f, 0.f, 0.f, &error));
        REQUIRE(encoder->setStencilReferenceValue(0, &error));
        REQUIRE(encoder->setStencilReferenceValue(0, &error));
        REQUIRE(encoder->setDepthClipMode(ALMetalDepthClipMode::clip, &error));
        REQUIRE(encoder->setDepthClipMode(ALMetalDepthClipMode::clamp, &error));
        REQUIRE(encoder->setDepthClipMode(ALMetalDepthClipMode::clip, &error));
        REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::disabled, 0, &error));
        REQUIRE(encoder->setVisibilityResultMode(ALMetalVisibilityResultMode::disabled, 0, &error));
        const auto draw_pixel = [&](std::uint32_t x, std::uint32_t y)
        {
            const ALMetalScissorRect scissor{x, y, 1, 1};
            REQUIRE(encoder->setScissor(scissor, &error));
            REQUIRE(encoder->setScissor(scissor, &error));
            REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        };
        REQUIRE(encoder->setVertexBuffer(0, *vertex_values, 0, &error));
        REQUIRE(encoder->setFragmentBuffer(0, *colors, 0, &error));
        draw_pixel(0, 0);
        REQUIRE(encoder->setVertexBytes(0, half.data(), sizeof(half), &error));
        draw_pixel(1, 0);
        // Rebinding the same pre-bytes buffer must enter Metal again.
        REQUIRE(encoder->setVertexBuffer(0, *vertex_values, 0, &error));
        draw_pixel(2, 0);
        REQUIRE(encoder->setVertexBuffer(0, *vertex_values, 256, &error));
        draw_pixel(3, 0);
        REQUIRE(encoder->setVertexBuffer(0, *vertex_values, 0, &error));
        REQUIRE(encoder->setVertexBuffer(0, *vertex_values, 0, &error));
        draw_pixel(4, 0);
        REQUIRE(encoder->setFragmentBytes(0, green.data(), sizeof(green), &error));
        draw_pixel(5, 0);
        REQUIRE(encoder->setFragmentBuffer(0, *colors, 0, &error));
        REQUIRE(encoder->setFragmentBuffer(0, *colors, 0, &error));
        draw_pixel(6, 0);
        REQUIRE(encoder->setFragmentBuffer(0, *colors, 256, &error));
        draw_pixel(7, 0);
        REQUIRE(encoder->setFragmentBuffer(0, *colors, 0, &error));
        REQUIRE(encoder->setPipeline(*inverse, &error));
        draw_pixel(8, 0);
        REQUIRE(encoder->setPipeline(*pipeline, &error));
        draw_pixel(9, 0);

        // Invalid setters must still fail on a cache hit, leaving the last
        // successfully issued state in place.
        CHECK_FALSE(encoder->setVertexBuffer(0, *vertex_values, 512, &error));
        CHECK_FALSE(encoder->setFragmentBuffer(31, *colors, 0, &error));
        CHECK_FALSE(encoder->setVertexBytes(0, one.data(), 0, &error));
        CHECK_FALSE(encoder->setScissor({0, 0, 0, 1}, &error));
        draw_pixel(10, 0);

        REQUIRE(encoder->setFrontFacingWinding(ALMetalWinding::counter_clockwise, &error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::front, &error));
        draw_pixel(11, 0);
        REQUIRE(encoder->setCullMode(ALMetalCullMode::back, &error));
        draw_pixel(12, 0);
        REQUIRE(encoder->setFrontFacingWinding(ALMetalWinding::clockwise, &error));
        draw_pixel(13, 0);
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
        REQUIRE(encoder->setTriangleFillMode(ALMetalTriangleFillMode::lines, &error));
        draw_pixel(14, 0);
        REQUIRE(encoder->setTriangleFillMode(ALMetalTriangleFillMode::fill, &error));
        draw_pixel(15, 0);
        REQUIRE(encoder->end(&error));
        CHECK_FALSE(encoder->setPipeline(*pipeline, &error));
        CHECK_FALSE(encoder->setVertexBuffer(0, *vertex_values, 0, &error));

        ALMetalRenderPassConfig load;
        load.colors[0].load = ALMetalLoadAction::load;
        auto next_pass = target->makeRenderPass(load, &error);
        REQUIRE(next_pass);
        auto next = ALMetalRenderEncoder::begin(*command, *next_pass, "New cache scope", &error);
        REQUIRE(next);
        CHECK_FALSE(next->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        REQUIRE(next->setPipeline(*pipeline, &error));
        REQUIRE(next->setVertexBuffer(0, *vertex_values, 0, &error));
        REQUIRE(next->setFragmentBuffer(0, *colors, 0, &error));
        REQUIRE(next->setViewport(viewport, &error));
        REQUIRE(next->setScissor({0, 1, 1, 1}, &error));
        REQUIRE(next->setCullMode(ALMetalCullMode::none, &error));
        REQUIRE(next->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
        REQUIRE(next->end(&error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);
        std::array<std::uint8_t, 16 * 4 * 4> pixels{};
        REQUIRE(target->colorTexture(0)->readback(
            *device, 0, 0, {}, pixels.data(), 16 * 4, &error));
        const auto pixel = [&](unsigned x, unsigned y, unsigned channel)
        { return pixels[(y * 16 + x) * 4 + channel]; };
        for (unsigned x : {0u, 2u, 4u, 6u, 9u, 10u, 15u})
        {
            CHECK(pixel(x, 0, 0) == 255);
            CHECK(pixel(x, 0, 1) == 0);
            CHECK(pixel(x, 0, 2) == 0);
        }
        CHECK(std::abs(int(pixel(1, 0, 0)) - 128) <= 1);
        CHECK(std::abs(int(pixel(3, 0, 0)) - 64) <= 1);
        CHECK(pixel(5, 0, 1) == 255);
        CHECK(pixel(7, 0, 2) == 255);
        CHECK(pixel(8, 0, 0) == 0);
        CHECK(pixel(8, 0, 1) == 255);
        CHECK(pixel(8, 0, 2) == 255);
        CHECK(int(pixel(11, 0, 0)) + int(pixel(12, 0, 0)) == 255);
        CHECK(pixel(11, 0, 0) == pixel(13, 0, 0));
        CHECK(pixel(14, 0, 0) == 0);
        CHECK(pixel(0, 1, 0) == 255);
    }
}

TEST_CASE("almetal: repeated pipelines retain tessellation factor preconditions",
          "[almetal][encoder-cache]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        REQUIRE(device);
        auto library = ALMetalShaderLibrary::compileSource(
            *device, cacheTestSource, "Tessellation cache regression", &error);
        REQUIRE(library);
        auto patch = cachePipeline(*device, *library, "cache_patch", "cache_color", true, error);
        auto plain = cachePipeline(*device, *library, "cache_vertex", "cache_color", false, error);
        INFO(error);
        REQUIRE(patch);
        REQUIRE(plain);
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = descriptor.height = 8;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.label = "Encoder cache tessellation pixels";
        auto target = ALMetalRenderTarget::create(*device, descriptor, &error);
        REQUIRE(target);
        auto pass = target->makeRenderPass({}, &error);
        auto command = device->makeCommandBuffer("Tessellation cache preconditions", &error);
        REQUIRE(pass);
        REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Tessellation cache", &error);
        REQUIRE(encoder);
        ALMetalBufferDescriptor factors_descriptor;
        factors_descriptor.length = 256;
        factors_descriptor.label = "Encoder cache tessellation factors";
        auto factors = ALMetalBuffer::create(*device, factors_descriptor, &error);
        REQUIRE(factors);
        std::array<std::uint16_t, 6> factor_data;
        factor_data.fill(0x3c00); // Six half-precision factors set to one.
        REQUIRE(factors->write(0, factor_data.data(), sizeof(factor_data), &error));
        const std::array<float, 4> red = {1.f, 0.f, 0.f, 1.f};
        REQUIRE(encoder->setPipeline(*patch, &error));
        CHECK_FALSE(encoder->drawPatches(4, 0, 1, 1, 0, &error));
        REQUIRE(encoder->setTessellationFactorBuffer(*factors, 0, 0, &error));
        REQUIRE(encoder->setPipeline(*patch, &error));
        // The old wrapper resets this precondition at every setPipeline;
        // skipping the native duplicate must preserve that contract.
        CHECK_FALSE(encoder->drawPatches(4, 0, 1, 1, 0, &error));
        REQUIRE(encoder->setTessellationFactorBuffer(*factors, 0, 0, &error));
        REQUIRE(encoder->setFragmentBytes(0, red.data(), sizeof(red), &error));
        REQUIRE(encoder->setViewport({0., 0., 8., 8., 0., 1.}, &error));
        REQUIRE(encoder->setScissor({0, 0, 8, 8}, &error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
        REQUIRE(encoder->drawPatches(4, 0, 1, 1, 0, &error));
        REQUIRE(encoder->setPipeline(*plain, &error));
        CHECK_FALSE(encoder->setTessellationFactorBuffer(*factors, 0, 0, &error));
        CHECK_FALSE(encoder->drawPatches(4, 0, 1, 1, 0, &error));
        REQUIRE(encoder->setPipeline(*patch, &error));
        CHECK_FALSE(encoder->drawPatches(4, 0, 1, 1, 0, &error));
        REQUIRE(encoder->end(&error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);
        std::array<std::uint8_t, 8 * 8 * 4> pixels{};
        REQUIRE(target->colorTexture(0)->readback(
            *device, 0, 0, {}, pixels.data(), 8 * 4, &error));
        CHECK(pixels[(4 * 8 + 4) * 4] == 255);
    }
}
