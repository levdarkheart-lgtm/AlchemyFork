/**
 * @file almetal_shadow_gather_test.mm
 * @brief Actual shadow gather against the frozen four-read integer footprint.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalpipeline.h"
#import <Foundation/Foundation.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

namespace
{
struct alignas(16) Query { float u, v, layer, unused = 0; };
static_assert(sizeof(Query) == 16);
// Frozen 0fa4e331333f99401955bc0722929fafb6bdff22, shader SHA5dea7fbf.
// Independent integer footprint AND direct texture reads: never call the
// production gather/depth helper from this reference implementation.
constexpr const char* oracle = R"metal(
static float4 regression_four_read_shadow_gather(
    texture2d_array<float> maps, uint layer, float2 uv, uint2 size)
{
    const int2 lo = int2(floor(uv * float2(size) - 0.5));
    const int2 maximum = int2(size) - 1;
    return float4(
        maps.read(uint2(clamp(lo + int2(0,1), int2(0), maximum)), layer).r,
        maps.read(uint2(clamp(lo + int2(1,1), int2(0), maximum)), layer).r,
        maps.read(uint2(clamp(lo + int2(1,0), int2(0), maximum)), layer).r,
        maps.read(uint2(clamp(lo, int2(0), maximum)), layer).r);
}
kernel void regression_actual_shadow_gather(
    texture2d_array<float> maps [[texture(0)]],
    device uint4* reference [[buffer(0)]], device uint4* actual [[buffer(1)]],
    constant float4* queries [[buffer(2)]], constant uint& count [[buffer(3)]],
    uint id [[thread_position_in_grid]])
{
    if (id >= count) return;
    const float4 query = queries[id];
    const uint2 size(maps.get_width(), maps.get_height());
    reference[id] = as_type<uint4>(regression_four_read_shadow_gather(
        maps, uint(query.z), query.xy, size));
    actual[id] = as_type<uint4>(almetal_shadow_gather_depth(
        maps, uint(query.z), query.xy, size));
}
)metal";
}

TEST_CASE("almetal: production shadow gather preserves raw values and clamped order",
          "[almetal][shadow-gather]")
{
    @autoreleasepool
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error); REQUIRE(device);
        const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() /
            "shaders" / "almetal_builtin.metal";
        std::ifstream stream(path);
        REQUIRE(stream.good());
        const std::string production{std::istreambuf_iterator<char>(stream), {}};
        auto library = ALMetalShaderLibrary::compileSource(*device, production + oracle,
            "Actual production shadow gather regression", &error);
        INFO(error); REQUIRE(library);
        ALMetalComputePipelineDescriptor pd;
        pd.library = library.get(); pd.function = "regression_actual_shadow_gather";
        pd.label = "Actual shadow gather against frozen integer reads";
        auto pipeline = ALMetalComputePipeline::create(*device, pd, &error);
        REQUIRE(pipeline);
        auto buffer = [&](std::size_t length)
        {
            ALMetalBufferDescriptor bd;
            bd.length = length; bd.storage = ALMetalStorageMode::shared;
            bd.label = "Shadow gather regression input/output";
            auto result = ALMetalBuffer::create(*device, bd, &error);
            REQUIRE(result); return result;
        };
        constexpr std::array<std::array<std::uint32_t, 2>, 9> dimensions{{
            {{1,1}}, {{2,3}}, {{3,2}}, {{7,11}}, {{31,17}}, {{64,63}},
            {{1009,1023}}, {{1024,1024}}, {{2048,2048}}}};
        for (const auto& dimension : dimensions)
        {
            const auto w = dimension[0], h = dimension[1];
            CAPTURE(w, h);
            ALMetalTextureDescriptor td;
            td.type = ALMetalTextureType::texture_2d_array;
            td.format = ALMetalPixelFormat::r32_float;
            td.width = w; td.height = h; td.array_length = 4;
            td.label = "Four-layer row-varying finite raw shadow depths";
            auto maps = ALMetalTexture::create(*device, td, &error);
            REQUIRE(maps);
            auto command = device->makeCommandBuffer("Shadow gather dimensions", &error);
            REQUIRE(command);
            std::vector<float> values(std::size_t(w) * h);
            for (std::uint32_t layer = 0; layer < 4; ++layer)
            {
                for (std::uint32_t y = 0; y < h; ++y) for (std::uint32_t x = 0; x < w; ++x)
                {
                    const std::uint32_t global = (layer * h + y) * w + x;
                    // Odd multiplication permutes all24lowbits: all16,777,216
                    // production2048²×4 texels have distinct positive finite
                    // IEEE patterns, unlike a repeating1024-value ramp.
                    const std::uint32_t bits = 0x3e000000u |
                        ((global * 0x9e3779b9u) & 0x00ffffffu);
                    values[std::size_t(y) * w + x] = std::bit_cast<float>(bits);
                }
                // Physical depth endpoints and adjacent normal finite values.
                if (layer == 0)
                {
                    constexpr std::array<std::uint32_t, 4> endpoints{{
                        0u, 0x3f800000u, 0x3f7fffffu, 0x00800000u}};
                    for (std::size_t i = 0; i < std::min(values.size(), endpoints.size()); ++i)
                        values[i] = std::bit_cast<float>(endpoints[i]);
                }
                REQUIRE(maps->upload(*command, 0, layer, {0,0,w,h},
                    values.data(), std::size_t(w) * 4, &error));
            }
            std::vector<Query> queries;
            const std::array<float, 10> xs{{-1,0,.5f,1,float(w)/2,
                float(w)-1,float(w)-.5f,float(w),float(w)+.5f,float(w)+1}};
            const std::array<float, 10> ys{{-1,0,.5f,1,float(h)/2,
                float(h)-1,float(h)-.5f,float(h),float(h)+.5f,float(h)+1}};
            for (std::uint32_t layer = 0; layer < 4; ++layer)
            {
                for (float x : xs) for (float y : ys)
                {
                    const float u = x / w, v = y / h;
                    queries.push_back({u,v,float(layer)});
                    queries.push_back({std::nextafter(u,-INFINITY),std::nextafter(v,-INFINITY),float(layer)});
                    queries.push_back({std::nextafter(u,INFINITY),std::nextafter(v,INFINITY),float(layer)});
                    queries.push_back({std::nextafter(u,-INFINITY),v,float(layer)});
                    queries.push_back({std::nextafter(u,INFINITY),v,float(layer)});
                    queries.push_back({u,std::nextafter(v,-INFINITY),float(layer)});
                    queries.push_back({u,std::nextafter(v,INFINITY),float(layer)});
                }
                std::uint32_t random = 0x9e3779b9u + layer;
                for (unsigned i = 0; i < 256; ++i)
                {
                    random = random * 1664525u + 1013904223u;
                    const float u = float(random >> 8) / 16777216.f * 3.f - 1.f;
                    random = random * 1664525u + 1013904223u;
                    const float v = float(random >> 8) / 16777216.f * 3.f - 1.f;
                    queries.push_back({u,v,float(layer)});
                }
            }
            const std::uint32_t count = static_cast<std::uint32_t>(queries.size());
            const auto bytes = std::size_t(count) * 16;
            auto query_buffer = buffer(bytes), reference = buffer(bytes), actual = buffer(bytes);
            REQUIRE(query_buffer->write(0, queries.data(), bytes, &error));
            auto encoder = ALMetalComputeEncoder::begin(*command, "Raw shadow gather oracle", &error);
            REQUIRE(encoder);
            REQUIRE(encoder->setPipeline(*pipeline, &error));
            REQUIRE(encoder->setTexture(0, maps.get(), &error));
            REQUIRE(encoder->setBuffer(0, *reference, 0, &error));
            REQUIRE(encoder->setBuffer(1, *actual, 0, &error));
            REQUIRE(encoder->setBuffer(2, *query_buffer, 0, &error));
            REQUIRE(encoder->setBytes(3, &count, sizeof(count), &error));
            REQUIRE(encoder->dispatch(count, 1, 1, 64, 1, 1, &error));
            REQUIRE(encoder->end(&error));
            REQUIRE(command->commitAndWait(&error));
            std::vector<std::array<std::uint32_t, 4>> a(count), b(count);
            REQUIRE(reference->read(0, a.data(), bytes, &error));
            REQUIRE(actual->read(0, b.data(), bytes, &error));
            std::size_t nonuniform = 0;
            for (std::size_t i = 0; i < count; ++i)
            {
                CAPTURE(i, queries[i].u, queries[i].v, queries[i].layer);
                CHECK(a[i] == b[i]); // allfloat4bits, noepsilon or numeric conversion
                nonuniform += a[i][0] != a[i][1] || a[i][0] != a[i][2] || a[i][0] != a[i][3];
            }
            if (w > 1 || h > 1) CHECK(nonuniform > 0);
        }
    }
}
