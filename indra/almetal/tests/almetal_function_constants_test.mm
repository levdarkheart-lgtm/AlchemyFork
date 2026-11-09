/**
 * @file almetal_function_constants_test.mm
 * @brief Fragment boolean specialization and fail-closed library contracts.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalpipeline.h"

#import <Foundation/Foundation.h>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

TEST_CASE("almetal: fragment bool constants specialize pixels and reject stale contracts",
          "[almetal][function-constants]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        constexpr const char* source = R"metal(
#include <metal_stdlib>
using namespace metal;
constant bool cube [[function_constant(3)]];
constant uint wrong_type [[function_constant(5)]];
constant uint required_value [[function_constant(7)]];
struct Out { float4 position [[position]]; };
vertex Out constants_vertex(uint i [[vertex_id]])
{
    const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
    return {float4(p[i], 0, 1)};
}
vertex Out unsupported_constants_vertex(uint i [[vertex_id]])
{
    return {float4(float(i), 0, float(required_value), 1)};
}
fragment float4 constants_fragment()
{
    bool enabled = false;
    if (is_function_constant_defined(cube)) enabled = cube;
    return enabled ? float4(0,1,0,1) : float4(1,0,0,1);
}
fragment float4 wrong_type_fragment()
{
    return float4(float(wrong_type), 0, 0, 1);
}
fragment float4 required_fragment()
{
    return float4(cube ? 1.f : 0.f, float(required_value), 0, 1);
}
)metal";
        auto library = ALMetalShaderLibrary::compileSource(
            *device, source, "Bool function constants regression", &error);
        REQUIRE(library);
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = "constants_vertex";
        descriptor.fragment_function = "constants_fragment";
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.label = "Bool constants regression PSO";
        std::array<std::unique_ptr<ALMetalRenderPipeline>, 5> pipelines;
        pipelines[0] = ALMetalRenderPipeline::create(*device, descriptor, &error);
        REQUIRE(pipelines[0]);
        CHECK(pipelines[0]->descriptor().fragment_bool_constants.empty());
        // Reuse and mutate the SAME descriptor: no stale MTLFunctionConstantValues.
        for (std::size_t i = 1; i < pipelines.size(); ++i)
        {
            const bool enabled = i % 2 == 0;
            descriptor.fragment_bool_constants = {{3, enabled}};
            pipelines[i] = ALMetalRenderPipeline::create(*device, descriptor, &error);
            REQUIRE(pipelines[i]);
            const auto& retained = pipelines[i]->descriptor();
            REQUIRE(retained.fragment_bool_constants.size() == 1);
            CHECK(retained.fragment_bool_constants[0].index == 3);
            CHECK(retained.fragment_bool_constants[0].value == enabled);
            CHECK(retained.library == nullptr);
        }
        auto rejected = [&](ALMetalRenderPipelineDescriptor invalid,
                            const char* expected)
        {
            error.clear();
            CHECK_FALSE(ALMetalRenderPipeline::create(*device, invalid, &error));
            INFO(error);
            CHECK_FALSE(error.empty());
            CHECK(error.find(expected) != std::string::npos);
        };
        auto invalid = descriptor;
        invalid.fragment_bool_constants = {{3, true}, {3, true}};
        rejected(invalid, "duplicate");
        invalid.fragment_bool_constants = {{3, true}, {3, false}};
        rejected(invalid, "duplicate");
        invalid.fragment_bool_constants = {{4, true}};
        rejected(invalid, "absent or is not bool");
        invalid.fragment_function = "wrong_type_fragment";
        invalid.fragment_bool_constants = {{5, true}};
        rejected(invalid, "absent or is not bool");
        invalid = descriptor;
        invalid.fragment_function.clear();
        rejected(invalid, "require a fragment function");
        invalid = descriptor;
        invalid.fragment_function = "required_fragment";
        invalid.fragment_bool_constants = {{3, true}};
        rejected(invalid, "Could not specialize Metal fragment function required_fragment");
        invalid.fragment_bool_constants.clear();
        rejected(invalid, "Could not specialize Metal fragment function required_fragment");
        invalid = descriptor;
        invalid.vertex_function = "unsupported_constants_vertex";
        rejected(invalid, "vertex function constants are not supported");

        constexpr const char* stale_source = R"metal(
#include <metal_stdlib>
using namespace metal;
struct Out { float4 position [[position]]; };
vertex Out constants_vertex(uint i [[vertex_id]])
{
    const float2 p[] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
    return {float4(p[i], 0, 1)};
}
fragment float4 constants_fragment() { return float4(1,0,0,1); }
)metal";
        auto stale = ALMetalShaderLibrary::compileSource(
            *device, stale_source, "Old library without expected constants", &error);
        REQUIRE(stale);
        invalid = descriptor;
        invalid.library = stale.get();
        rejected(invalid, "absent or is not bool");
        // All pipeline states must own native compiled functions independently.
        library.reset();
        stale.reset();
        descriptor.library = nullptr;
        invalid.library = nullptr;

        ALMetalRenderTargetDescriptor target_descriptor;
        target_descriptor.width = target_descriptor.height = 4;
        target_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        target_descriptor.label = "Bool constants regression target";
        auto target = ALMetalRenderTarget::create(*device, target_descriptor, &error);
        REQUIRE(target);
        std::array<std::uint8_t, 4 * 4 * 4> generic_pixels{};
        for (std::size_t i = 0; i < pipelines.size(); ++i)
        {
            CAPTURE(i);
            ALMetalRenderPassConfig config;
            config.colors[0].load = ALMetalLoadAction::clear;
            config.colors[0].store = ALMetalStoreAction::store;
            config.colors[0].clear = {0, 0, 1, 0};
            auto pass = target->makeRenderPass(config, &error);
            REQUIRE(pass);
            auto command = device->makeCommandBuffer("Bool constants regression draw", &error);
            REQUIRE(command);
            auto encoder = ALMetalRenderEncoder::begin(
                *command, *pass, "Bool constants regression encoder", &error);
            REQUIRE(encoder);
            REQUIRE(encoder->setPipeline(*pipelines[i], &error));
            REQUIRE(encoder->setViewport({0., 0., 4., 4., 0., 1.}, &error));
            REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
            REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
            REQUIRE(encoder->end(&error));
            REQUIRE(command->commitAndWait(&error));
            std::array<std::uint8_t, 4 * 4 * 4> pixels{};
            REQUIRE(target->colorTexture(0)->readback(
                *device, 0, 0, {}, pixels.data(), 4 * 4, &error));
            const bool enabled = i != 0 && i % 2 == 0;
            for (std::size_t p = 0; p < pixels.size(); p += 4)
            {
                CHECK(pixels[p] == (enabled ? 0 : 255));
                CHECK(pixels[p + 1] == (enabled ? 255 : 0));
                CHECK(pixels[p + 2] == 0);
                CHECK(pixels[p + 3] == 255);
            }
            if (i == 0) generic_pixels = pixels;
            else if (!enabled) CHECK(pixels == generic_pixels);
        }
    }
}
