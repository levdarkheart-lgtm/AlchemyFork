/**
 * @file alrastertile_geometry_metal_test.mm
 * @brief Native raster-tile geometry preserves rendered attachment contracts.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "../../almetal/almetalpipeline.h"
#include <catch2/catch_test_macros.hpp>
#import <Metal/Metal.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace
{
using HalfPixel = std::array<std::uint16_t, 4>;
constexpr unsigned attachments = 3;
constexpr std::array<float, 4> clear_color{0.0625f, 0.125f, 0.25f, 1.f};

std::uint16_t halfBits(float value)
{
    const _Float16 half = static_cast<_Float16>(value);
    std::uint16_t result = 0;
    std::memcpy(&result, &half, sizeof(result));
    return result;
}

std::uint32_t floatBits(float value)
{
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

struct Snapshot
{
    std::array<std::vector<HalfPixel>, attachments> color;
    std::vector<std::uint32_t> depth;
    std::vector<std::uint8_t> stencil;
    std::vector<std::array<unsigned, 2>> actual_tiles;
};

// Original1eb44 llrendertarget.cpp:890 bindTarget establishes the MRT and
// viewport; clear/flush preserve attachment contents and ordered pass boundaries.
// Explicit native raster-tile dimensions are new and must not change those
// observable contracts. This fixture uses real draws, blends and stored D32S8.
struct RasterTiles
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device = ALMetalDevice::create(&error);
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;
    std::unique_ptr<ALMetalDepthStencilState> depth_state;

    RasterTiles()
    {
        INFO(error); REQUIRE(device);
        library = ALMetalShaderLibrary::compileSource(*device, R"(
#include <metal_stdlib>
using namespace metal;
struct Parameters { uint phase; float depth; uint2 padding; };
vertex float4 raster_tile_vertex(uint i [[vertex_id]],
    constant Parameters& p [[buffer(0)]]) {
    return float4(float2((i << 1) & 2, i & 2) * 2.0 - 1.0, p.depth, 1.0);
}
struct MRT { float4 a [[color(0)]]; float4 b [[color(1)]]; float4 c [[color(2)]]; };
fragment MRT raster_tile_fragment(float4 position [[position]],
    constant Parameters& p [[buffer(0)]]) {
    const uint2 xy = uint2(position.xy);
    const float4 value = float4(float((xy.x + p.phase) & 3u) * 0.125 + 0.125,
        float(xy.y & 1u) * 0.25 + 0.125, 0.5, 0.5);
    return {value, value.bgra, value.gbra};
}
)", "Native raster tile observable-output oracle", &error);
        INFO(error); REQUIRE(library);
        ALMetalRenderPipelineDescriptor pd;
        pd.library = library.get();
        pd.vertex_function = "raster_tile_vertex";
        pd.fragment_function = "raster_tile_fragment";
        pd.color_attachment_count = attachments;
        pd.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        for (unsigned i = 0; i < attachments; ++i)
        {
            auto& color = pd.colors[i];
            color.format = ALMetalPixelFormat::rgba16_float;
            color.blending_enabled = true;
            color.source_rgb = ALMetalBlendFactor::source_alpha;
            color.destination_rgb = ALMetalBlendFactor::one_minus_source_alpha;
            color.source_alpha = ALMetalBlendFactor::zero;
            color.destination_alpha = ALMetalBlendFactor::one_minus_source_alpha;
        }
        pipeline = ALMetalRenderPipeline::create(*device, pd, &error);
        INFO(error); REQUIRE(pipeline);
        ALMetalDepthStencilDescriptor dd;
        dd.depth_compare = ALMetalCompareFunction::less;
        dd.depth_write_enabled = true;
        dd.stencil_enabled = true;
        dd.front_stencil.depth_stencil_pass = ALMetalStencilOperation::replace;
        dd.back_stencil = dd.front_stencil;
        depth_state = ALMetalDepthStencilState::create(*device, dd, &error);
        INFO(error); REQUIRE(depth_state);
    }

    id<MTLDevice> native() const
    { return (__bridge id<MTLDevice>)device->nativeHandle(); }

    std::unique_ptr<ALMetalRenderTarget> target(unsigned width, unsigned height)
    {
        ALMetalRenderTargetDescriptor td;
        td.width = width; td.height = height;
        td.color_attachment_count = attachments;
        for (unsigned i = 0; i < attachments; ++i)
            td.colors[i].format = ALMetalPixelFormat::rgba16_float;
        td.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
        auto result = ALMetalRenderTarget::create(*device, td, &error);
        INFO(error); REQUIRE(result); return result;
    }

    Snapshot render(unsigned width, unsigned height, bool restart,
                    ALMetalRasterTileSize tile_size)
    {
        auto output = target(width, height);
        auto command = device->makeCommandBuffer("Raster tile correctness draws", &error);
        REQUIRE(command);
        Snapshot result;
        for (unsigned phase = 0; phase < (restart ? 2u : 1u); ++phase)
        {
            ALMetalRenderPassConfig config;
            config.raster_tile_size = tile_size;
            for (unsigned i = 0; i < attachments; ++i)
            {
                config.colors[i].load = phase ? ALMetalLoadAction::load : ALMetalLoadAction::clear;
                config.colors[i].store = ALMetalStoreAction::store;
                config.colors[i].clear = {clear_color[0], clear_color[1], clear_color[2], clear_color[3]};
            }
            config.depth_load = phase ? ALMetalLoadAction::load : ALMetalLoadAction::clear;
            config.depth_store = ALMetalStoreAction::store;
            config.clear_depth = 1.0;
            config.stencil_load = phase ? ALMetalLoadAction::load : ALMetalLoadAction::clear;
            config.stencil_store = ALMetalStoreAction::store;
            config.clear_stencil = 9;
            auto pass = output->makeRenderPass(config, &error);
            INFO(error); REQUIRE(pass);
            auto descriptor = (__bridge MTLRenderPassDescriptor*)pass->nativeHandle();
            auto defaults = [MTLRenderPassDescriptor renderPassDescriptor];
            CHECK(descriptor.tileWidth == (tile_size == ALMetalRasterTileSize::tile16 ? 16u : defaults.tileWidth));
            CHECK(descriptor.tileHeight == (tile_size == ALMetalRasterTileSize::tile16 ? 16u : defaults.tileHeight));
            CHECK(descriptor.threadgroupMemoryLength == defaults.threadgroupMemoryLength);
            auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Raster tile correctness pass", &error);
            REQUIRE(encoder);
            result.actual_tiles.push_back({encoder->rasterTileWidth(), encoder->rasterTileHeight()});
            CAPTURE(width, height, phase, encoder->rasterTileWidth(), encoder->rasterTileHeight());
            if (tile_size == ALMetalRasterTileSize::tile16)
            {
                CHECK(encoder->rasterTileWidth() == 16); CHECK(encoder->rasterTileHeight() == 16);
            }
            struct alignas(16) Parameters
            {
                std::uint32_t phase;
                float depth;
                std::uint32_t padding[2];
            } parameters{phase, phase ? 0.25f : 0.5f, {0, 0}};
            static_assert(sizeof(Parameters) == 16);
            REQUIRE(encoder->setPipeline(*pipeline, &error));
            REQUIRE(encoder->setDepthStencilState(depth_state.get(), &error));
            REQUIRE(encoder->setStencilReferenceValue(phase ? 73 : 37, &error));
            REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &error));
            REQUIRE(encoder->setViewport({0, 0, double(width), double(height), 0, 1}, &error));
            REQUIRE(encoder->setScissor(phase
                ? ALMetalScissorRect{width / 3, height / 4, width / 2, height / 3}
                : ALMetalScissorRect{0, 0, width, height}, &error));
            REQUIRE(encoder->setVertexBytes(0, &parameters, sizeof(parameters), &error));
            REQUIRE(encoder->setFragmentBytes(0, &parameters, sizeof(parameters), &error));
            REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, &error));
            REQUIRE(encoder->end(&error));
            CHECK(encoder->rasterTileWidth() == 0);
            CHECK(encoder->rasterTileHeight() == 0);
        }
        INFO(error); REQUIRE(command->commitAndWait(&error));
        for (unsigned i = 0; i < attachments; ++i)
        {
            result.color[i].resize(std::size_t(width) * height);
            REQUIRE(output->colorTexture(i)->readback(*device, 0, 0, {}, result.color[i].data(), width * 8, &error));
        }
        // Read both stored aspects independently; the generic texture readback
        // deliberately exposes only depth for a combined depth/stencil texture.
        const unsigned depth_row = (width * 4u + 255u) & ~255u;
        const unsigned stencil_row = (width + 255u) & ~255u;
        const std::size_t stencil_offset = std::size_t(depth_row) * height;
        id<MTLBuffer> staging = [native() newBufferWithLength:stencil_offset + std::size_t(stencil_row) * height
            options:MTLResourceStorageModeShared]; REQUIRE(staging);
        auto readback = device->makeCommandBuffer("Raster tile depth/stencil readback", &error);
        REQUIRE(readback);
        id<MTLBlitCommandEncoder> blit = [(__bridge id<MTLCommandBuffer>)readback->nativeHandle() blitCommandEncoder];
        REQUIRE(blit);
        auto source = (__bridge id<MTLTexture>)output->depthStencilTexture()->nativeHandle();
        for (unsigned aspect = 0; aspect < 2; ++aspect)
            [blit copyFromTexture:source sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
                sourceSize:MTLSizeMake(width, height, 1) toBuffer:staging
                destinationOffset:aspect ? stencil_offset : 0
                destinationBytesPerRow:aspect ? stencil_row : depth_row
                destinationBytesPerImage:std::size_t(aspect ? stencil_row : depth_row) * height
                options:aspect ? MTLBlitOptionStencilFromDepthStencil : MTLBlitOptionDepthFromDepthStencil];
        [blit endEncoding]; REQUIRE(readback->commitAndWait(&error));
        result.depth.resize(std::size_t(width) * height);
        result.stencil.resize(std::size_t(width) * height);
        const auto* bytes = static_cast<const std::uint8_t*>(staging.contents);
        for (unsigned y = 0; y < height; ++y)
        {
            std::memcpy(result.depth.data() + std::size_t(y) * width, bytes + std::size_t(y) * depth_row, width * 4);
            std::memcpy(result.stencil.data() + std::size_t(y) * width,
                bytes + stencil_offset + std::size_t(y) * stencil_row, width);
        }
        return result;
    }

    void golden(const Snapshot& image, unsigned width, unsigned height, bool restart)
    {
        std::size_t changed = 0;
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x)
            {
                const bool second = restart && x >= width / 3 && y >= height / 4 &&
                    x < width / 3 + width / 2 && y < height / 4 + height / 3;
                changed += second;
                std::array<std::array<float, 4>, attachments> expected;
                for (auto& color : expected) color = clear_color;
                for (unsigned phase = 0; phase <= unsigned(second); ++phase)
                {
                    const std::array<float, 4> a{float((x + phase) & 3u) * 0.125f + 0.125f,
                        float(y & 1u) * 0.25f + 0.125f, 0.5f, 0.5f};
                    const std::array<std::array<float, 4>, attachments> sources{a,
                        std::array<float, 4>{a[2], a[1], a[0], a[3]},
                        std::array<float, 4>{a[1], a[2], a[0], a[3]}};
                    for (unsigned i = 0; i < attachments; ++i)
                    {
                        for (unsigned c = 0; c < 3; ++c)
                            expected[i][c] = (sources[i][c] + expected[i][c]) * 0.5f;
                        expected[i][3] *= 0.5f;
                    }
                }
                const std::size_t pixel = std::size_t(y) * width + x;
                for (unsigned i = 0; i < attachments; ++i)
                    for (unsigned c = 0; c < 4; ++c)
                        REQUIRE(image.color[i][pixel][c] == halfBits(expected[i][c]));
                REQUIRE(image.depth[pixel] == floatBits(second ? 0.25f : 0.5f));
                REQUIRE(image.stencil[pixel] == (second ? 73 : 37));
            }
        CHECK(changed == (restart ? std::size_t(width / 2) * (height / 3) : 0));
    }
};
} // namespace

TEST_CASE_METHOD(RasterTiles, "Explicit raster tile dimensions preserve MRT blending and restarted depth-stencil passes",
    "[almetal][raster-tile-geometry]")
{
    const bool supported = [native() supportsFamily:MTLGPUFamilyApple4];
    REQUIRE(ALMetalRenderPassConfig{}.raster_tile_size == ALMetalRasterTileSize::automatic);
    if (!supported)
    {
        auto output = target(97, 65);
        ALMetalRenderPassConfig config;
        REQUIRE(output->makeRenderPass(config, &error));
        config.raster_tile_size = ALMetalRasterTileSize::tile16;
        CHECK_FALSE(output->makeRenderPass(config, &error));
        CHECK_FALSE(error.empty());
        return;
    }
    for (const auto extent : std::array<std::array<unsigned, 2>, 2>{{{96, 64}, {97, 65}}})
        for (bool restart : {false, true})
        {
            CAPTURE(extent[0], extent[1], restart);
            const auto automatic = render(extent[0], extent[1], restart, ALMetalRasterTileSize::automatic);
            const auto explicit16 = render(extent[0], extent[1], restart, ALMetalRasterTileSize::tile16);
            CHECK(automatic.actual_tiles.size() == (restart ? 2u : 1u));
            CHECK(explicit16.actual_tiles.size() == automatic.actual_tiles.size());
            CHECK(explicit16.color == automatic.color);
            CHECK(explicit16.depth == automatic.depth);
            CHECK(explicit16.stencil == automatic.stencil);
            golden(automatic, extent[0], extent[1], restart);
            golden(explicit16, extent[0], extent[1], restart);
        }
}

TEST_CASE_METHOD(RasterTiles, "Invalid raster tile requests fail before a native encoder exists",
    "[almetal][raster-tile-geometry]")
{
    auto output = target(17, 19);
    ALMetalRenderPassConfig config;
    config.raster_tile_size = static_cast<ALMetalRasterTileSize>(255);
    CHECK_FALSE(output->makeRenderPass(config, &error));
    CHECK(error.find("raster tile size") != std::string::npos);
    // Invalid requests must not mutate the target or the automatic fallback.
    config.raster_tile_size = ALMetalRasterTileSize::automatic;
    error.clear();
    auto pass = output->makeRenderPass(config, &error);
    INFO(error); REQUIRE(pass);
    auto descriptor = (__bridge MTLRenderPassDescriptor*)pass->nativeHandle();
    auto defaults = [MTLRenderPassDescriptor renderPassDescriptor];
    CHECK(descriptor.tileWidth == defaults.tileWidth);
    CHECK(descriptor.tileHeight == defaults.tileHeight);
}
