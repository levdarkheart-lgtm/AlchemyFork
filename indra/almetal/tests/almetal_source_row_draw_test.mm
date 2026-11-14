/**
 * @file almetal_source_row_draw_test.mm
 * @brief Native source IDs preserve ordered draws under inherited bindings.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalpipeline.h"
#include <catch2/catch_test_macros.hpp>

#include <array>

namespace
{
constexpr std::uint32_t side = 32;
constexpr const char* shaders = R"metal(
#include <metal_stdlib>
using namespace metal;
struct Output { float4 position [[position]]; uint source [[flat]]; };
vertex Output source_vertex(const device float4* positions [[buffer(0)]],
    uint vertex [[vertex_id]], uint source [[instance_id]])
{
    return {positions[vertex], source};
}
fragment float4 source_fragment(Output input [[stage_in]])
{
    return input.source == 1u ? float4(0,1,0,1)
        : input.source == 2u ? float4(0,0,1,1) : float4(1,0,0,1);
}
)metal";
}

TEST_CASE("almetal: source instance IDs flush pending joins without changing ordinary adjacency",
          "[almetal][world-source-rows][source-row-draw]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
    auto library = ALMetalShaderLibrary::compileSource(*device, shaders, "Source ID draw proof", &error);
    INFO(error); REQUIRE(library);
    ALMetalRenderPipelineDescriptor pipeline_descriptor;
    pipeline_descriptor.library = library.get();
    pipeline_descriptor.vertex_function = "source_vertex";
    pipeline_descriptor.fragment_function = "source_fragment";
    pipeline_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
    auto pipeline = ALMetalRenderPipeline::create(*device, pipeline_descriptor, &error);
    INFO(error); REQUIRE(pipeline);
    const float positions[6][4]{{-.8f,-.8f,.5f,1},{-.2f,-.8f,.5f,1},{-.5f,.8f,.5f,1},
                                {.2f,-.8f,.5f,1},{.8f,-.8f,.5f,1},{.5f,.8f,.5f,1}};
    const auto buffer = [&](const void* bytes, std::size_t length)
    {
        ALMetalBufferDescriptor descriptor; descriptor.storage = ALMetalStorageMode::shared; descriptor.length = length;
        auto result = ALMetalBuffer::create(*device, descriptor, &error); INFO(error); REQUIRE(result);
        REQUIRE(result->write(0, bytes, length, &error)); return result;
    };
    auto vertices = buffer(positions, sizeof(positions));
    struct TimingRestore
    {
        bool previous = ALMetalDevice::commandTimingEnabled();
        ~TimingRestore() { ALMetalDevice::setCommandTimingEnabled(previous); }
    } timing;
    ALMetalDevice::setCommandTimingEnabled(true);
    for (auto index_type : {ALMetalIndexType::uint16, ALMetalIndexType::uint32})
    {
        const std::array<std::uint16_t,6> small{0,1,2,3,4,5};
        const std::array<std::uint32_t,6> wide{0,1,2,3,4,5};
        auto indices = index_type == ALMetalIndexType::uint16
            ? buffer(small.data(), sizeof(small)) : buffer(wide.data(), sizeof(wide));
        const std::size_t stride = index_type == ALMetalIndexType::uint16 ? 2 : 4;
        // 0 preserves the old one-call adjacent join. 1 interleaves a pending
        // ordinary draw with an overriding source-ID draw and another join.
        // 2 changes only source ID between adjacent ranges of one index buffer.
        for (unsigned mode = 0; mode < 3; ++mode)
        {
            CAPTURE(index_type, mode);
            ALMetalRenderTargetDescriptor target_descriptor;
            target_descriptor.width = target_descriptor.height = side;
            target_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
            auto target = ALMetalRenderTarget::create(*device, target_descriptor, &error); REQUIRE(target);
            auto pass = target->makeRenderPass({}, &error); REQUIRE(pass);
            auto command = device->makeCommandBuffer("Ordered source instance draw", &error); REQUIRE(command);
            auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Inherited source bindings", &error);
            INFO(error); REQUIRE(encoder);
            REQUIRE(encoder->setPipeline(*pipeline, &error));
            REQUIRE(encoder->setViewport({0,0,side,side,0,1}, &error));
            REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
            REQUIRE(encoder->setVertexBuffer(0, *vertices, 0, &error));
            REQUIRE(encoder->setIndexedTriangleJoining(true, &error));
            const auto before = ALMetalDevice::encodedDrawCallCount();
            if (mode == 2)
            {
                REQUIRE(encoder->drawIndexedWithBaseInstance(ALMetalPrimitiveType::triangle,
                    3, index_type, *indices, 0, 1, &error));
                REQUIRE(encoder->drawIndexedWithBaseInstance(ALMetalPrimitiveType::triangle,
                    3, index_type, *indices, 3 * stride, 2, &error));
            }
            else
            {
                REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle, 3, index_type, *indices, 0, 1, &error));
                if (mode == 1)
                {
                    // Rejected requests must not emit or consume pending work.
                    CHECK_FALSE(encoder->drawIndexedWithBaseInstance(ALMetalPrimitiveType::triangle,
                        0, index_type, *indices, 0, 1, &error));
                    CHECK_FALSE(encoder->drawIndexedWithBaseInstance(ALMetalPrimitiveType::triangle,
                        3, index_type, *indices, indices->length(), 1, &error));
                    REQUIRE(encoder->drawIndexedWithBaseInstance(ALMetalPrimitiveType::triangle,
                        3, index_type, *indices, 0, 1, &error));
                }
                REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle, 3, index_type,
                    *indices, 3 * stride, 1, &error));
            }
            REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
            CHECK(ALMetalDevice::encodedDrawCallCount() - before == (mode == 0 ? 1 : mode == 1 ? 3 : 2));
            std::array<std::uint8_t, side * side * 4> pixels;
            REQUIRE(target->colorTexture(0)->readback(*device, 0, 0, {}, pixels.data(), side * 4, &error));
            const auto color = [&](unsigned x, unsigned y)
            {
                const auto offset = (y * side + x) * 4;
                return std::array<std::uint8_t,4>{pixels[offset],pixels[offset+1],pixels[offset+2],pixels[offset+3]};
            };
            const std::array<std::uint8_t,4> red{255,0,0,255}, green{0,255,0,255}, blue{0,0,255,255};
            CHECK(color(8,16) == (mode == 0 ? red : green));
            CHECK(color(24,16) == (mode == 2 ? blue : red));
        }
    }
}
