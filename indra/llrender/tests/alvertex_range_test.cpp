/**
 * @file alvertex_range_test.cpp
 * @brief Actual vertex/index allocation bounds for adjacent viewer batches.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetalpipeline.h"

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <limits>
#include <vector>

namespace
{
using Range = LLVertexBuffer::DrawRange;

struct BufferScope
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    bool previous_authority = LLVertexBuffer::isMetalBufferAuthority();

    explicit BufferScope(bool native = false)
    {
        if (native)
        {
            device = ALMetalDevice::create(&error);
            INFO(error); REQUIRE(device);
            LLVertexBuffer::setMetalDevice(device.get());
        }
        LLVertexBuffer::setMetalBufferAuthority(true);
        LLVertexBuffer::initClass(nullptr);
    }
    ~BufferScope()
    {
        LLVertexBuffer::cleanupClass();
        LLVertexBuffer::setMetalBufferAuthority(previous_authority);
        if (device) LLVertexBuffer::setMetalDevice(nullptr);
    }
    LLPointer<LLVertexBuffer> buffer(U32 vertices, U32 indices)
    {
        LLPointer<LLVertexBuffer> result = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
        REQUIRE(result->allocateBuffer(vertices, indices));
        CHECK(result->getNumVerts() == vertices);
        CHECK(result->getNumIndices() == indices);
        return result;
    }
};
}

TEST_CASE("LLVertexBuffer appends only adjacent ranges in the same allocation",
          "[alvertexrange]")
{
    BufferScope scope;
    auto buffer = scope.buffer(16, 24);
    auto other = scope.buffer(16, 24);
    const Range first{2, 4, 3, 6};
    const Range next{6, 10, 9, 15};
    CHECK(buffer->canAppendRange(first, *buffer, next));
    CHECK_FALSE(buffer->canAppendRange(first, *other, next));
    CHECK_FALSE(buffer->canAppendRange(first, *buffer, {7, 9, 9, 15}));
    CHECK_FALSE(buffer->canAppendRange(first, *buffer, {5, 11, 9, 15}));
    CHECK_FALSE(buffer->canAppendRange(first, *buffer, {6, 10, 10, 14}));
    CHECK_FALSE(buffer->canAppendRange(first, *buffer, {6, 10, 8, 16}));
    CHECK_FALSE(buffer->canAppendRange(next, *buffer, first));
    CHECK_FALSE(buffer->canAppendRange(first, *buffer, {6, 11, 9, 15}));
    CHECK_FALSE(buffer->canAppendRange(first, *buffer, {6, 10, 9, 16}));
    CHECK_FALSE(buffer->canAppendRange({2, 15, 3, 6}, *buffer, next));
    CHECK_FALSE(buffer->canAppendRange({2, 4, 3, 22}, *buffer, next));
    CHECK_FALSE(buffer->canAppendRange({2, 0, 3, 6}, *buffer, next));
    CHECK_FALSE(buffer->canAppendRange({2, 4, 3, 0}, *buffer, next));
    CHECK_FALSE(buffer->canAppendRange(first, *buffer, {6, 0, 9, 15}));
    CHECK_FALSE(buffer->canAppendRange(first, *buffer, {6, 10, 9, 0}));
    CHECK_FALSE(buffer->canAppendRange(first, *buffer, {16, 1, 9, 15}));
    CHECK_FALSE(buffer->canAppendRange(first, *buffer, {6, 10, 24, 1}));
    auto empty = scope.buffer(0, 0);
    CHECK_FALSE(empty->canAppendRange(first, *empty, next));
}

TEST_CASE("LLVertexBuffer append bounds reject wrapping spans and preserve U16 capacity",
          "[alvertexrange]")
{
    BufferScope scope;
    auto buffer = scope.buffer(16, 24);
    const Range first{0, 4, 0, 6}, next{4, 12, 6, 18};
    constexpr U32 maximum = std::numeric_limits<U32>::max();
    const std::array<Range, 8> overflow{
        Range{maximum, 2, 0, 6}, Range{2, maximum, 0, 6},
        Range{0, 4, maximum, 2}, Range{0, 4, 2, maximum},
        Range{maximum-2, 8, 0, 6}, Range{0, 4, maximum-2, 8},
        Range{4, maximum-1, 6, 18}, Range{4, 12, 6, maximum-1}};
    for (const auto& range : overflow)
    {
        CHECK_FALSE(buffer->canAppendRange(range, *buffer, next));
        CHECK_FALSE(buffer->canAppendRange(first, *buffer, range));
    }
    // These are the existing volume and grass allocation ceilings, not new
    // hardware limits. Adjacency can consume the complete allocated range.
    auto volume = scope.buffer(65535, 196605);
    CHECK(volume->canAppendRange({0, 32768, 0, 98304}, *volume,
                                 {32768, 32767, 98304, 98301}));
    CHECK_FALSE(volume->canAppendRange({0, 32768, 0, 98304}, *volume,
                                       {32768, 32768, 98304, 98301}));
    auto grass = scope.buffer(65536, 196608);
    CHECK(grass->canAppendRange({0, 32768, 0, 98304}, *grass,
                                {32768, 32768, 98304, 98304}));
    // The separate grass batching policy still retains its original 4096
    // predicate at the caller; allocation validity does not impose that policy.
}

TEST_CASE("LLVertexBuffer adjacent native ranges render identically combined or split",
          "[alvertexrange][almetal]")
{
    BufferScope scope(true);
    auto buffer = scope.buffer(6, 6);
    const std::array<U16, 6> indices{0, 1, 2, 3, 4, 5};
    std::array<LLVector4a, 6> positions;
    positions[0].set(-1.f, -1.f, 0.f, 1.f);
    positions[1].set( 1.f, -1.f, 0.f, 1.f);
    positions[2].set(-1.f,  1.f, 0.f, 1.f);
    positions[3].set( 1.f, -1.f, 0.f, 1.f);
    positions[4].set( 1.f,  1.f, 0.f, 1.f);
    positions[5].set(-1.f,  1.f, 0.f, 1.f);
    buffer->setPositionData(positions.data(), 0, positions.size());
    buffer->setIndexData(indices.data(), 0, indices.size());
    buffer->unmapBuffer();
    REQUIRE(buffer->getMetalVertexBuffer()); REQUIRE(buffer->getMetalIndexBuffer());
    const Range first{0, 3, 0, 3}, next{3, 3, 3, 3};
    REQUIRE(buffer->canAppendRange(first, *buffer, next));
    const std::string source = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct Out { float4 position [[position]]; float4 color; };
vertex Out range_vertex(uint index [[vertex_id]],
                        const device float4* positions [[buffer(0)]])
{
    Out result;
    result.position = float4(positions[index].xyz, 1.0);
    result.color = index < 3 ? float4(1,0,0,1) : float4(0,1,0,1);
    return result;
}
fragment float4 range_fragment(Out input [[stage_in]]) { return input.color; }
)MSL";
    auto library = ALMetalShaderLibrary::compileSource(*scope.device, source,
        "Adjacent indexed batch output oracle", &scope.error);
    INFO(scope.error); REQUIRE(library);
    ALMetalRenderPipelineDescriptor pd;
    pd.library = library.get(); pd.vertex_function = "range_vertex";
    pd.fragment_function = "range_fragment"; pd.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
    auto pipeline = ALMetalRenderPipeline::create(*scope.device, pd, &scope.error);
    INFO(scope.error); REQUIRE(pipeline);
    auto render = [&](bool combined)
    {
        ALMetalRenderTargetDescriptor td; td.width = td.height = 8;
        td.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        auto target = ALMetalRenderTarget::create(*scope.device, td, &scope.error); REQUIRE(target);
        ALMetalRenderPassConfig pc; pc.colors[0].clear = {0, 0, 1, 1};
        auto pass = target->makeRenderPass(pc, &scope.error); REQUIRE(pass);
        auto command = scope.device->makeCommandBuffer("Adjacent indexed batch regression", &scope.error);
        REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Combined versus split", &scope.error);
        REQUIRE(encoder); REQUIRE(encoder->setPipeline(*pipeline, &scope.error));
        REQUIRE(encoder->setVertexBuffer(0, *buffer->getMetalVertexBuffer(),
            buffer->getOffset(LLVertexBuffer::TYPE_VERTEX), &scope.error));
        REQUIRE(encoder->setViewport({0,0,8,8,0,1}, &scope.error));
        REQUIRE(encoder->setScissor({0,0,8,8}, &scope.error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &scope.error));
        REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle,
            combined ? first.index_count + next.index_count : first.index_count,
            ALMetalIndexType::uint16, *buffer->getMetalIndexBuffer(), 0, 1, &scope.error));
        if (!combined)
            REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle, next.index_count,
                ALMetalIndexType::uint16, *buffer->getMetalIndexBuffer(),
                next.first_index * sizeof(U16), 1, &scope.error));
        REQUIRE(encoder->end(&scope.error)); REQUIRE(command->commitAndWait(&scope.error));
        std::vector<U8> pixels(8 * 8 * 4);
        REQUIRE(target->colorTexture(0)->readback(*scope.device, 0, 0, {}, pixels.data(), 8 * 4, &scope.error));
        return pixels;
    };
    const auto split = render(false), combined = render(true);
    CHECK(split == combined);
    unsigned red = 0, green = 0;
    for (std::size_t i = 0; i < combined.size(); i += 4)
    {
        CHECK(combined[i+2] == 0); CHECK(combined[i+3] == 255);
        if (combined[i] == 255 && combined[i+1] == 0) ++red;
        if (combined[i] == 0 && combined[i+1] == 255) ++green;
    }
    CHECK(red > 0); CHECK(green > 0); CHECK(red + green == 64);
    std::array<U16, 6> retained_indices{};
    REQUIRE(buffer->getMetalIndexBuffer()->read(0, retained_indices.data(), sizeof(retained_indices), &scope.error));
    CHECK(retained_indices == indices);
    CHECK(buffer->getNumVerts() == 6); CHECK(buffer->getNumIndices() == 6);
}
