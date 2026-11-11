/**
 * @file almetal_render_info_test.mm
 * @brief Native encoded draw census, independent of timing and GPU execution.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalshadowindirect.h"
#include "../almetaluirenderer.h"

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstring>

namespace
{
constexpr unsigned side = 8;
const char* source = R"metal(
#include <metal_stdlib>
using namespace metal;
struct Out { float4 position [[position]]; };
vertex Out counter_vertex(uint index [[vertex_id]])
{
    const float2 points[3] = {float2(-1,-1), float2(3,-1), float2(-1,3)};
    return {float4(points[index % 3], .5f, 1.f)};
}
[[patch(quad, 4)]]
vertex Out counter_patch(float2 uv [[position_in_patch]])
{
    return {float4(uv * 2.f - 1.f, .5f, 1.f)};
}
fragment float4 counter_fragment() { return float4(0,1,0,1); }
)metal";

struct RestoreCounters
{
    bool enabled = ALMetalDevice::renderInfo().enabled;
    bool timing = ALMetalDevice::commandTimingEnabled();
    ~RestoreCounters()
    {
        ALMetalDevice::beginRenderInfoFrame(enabled);
        ALMetalDevice::setCommandTimingEnabled(timing);
    }
};

struct Fixture
{
    RestoreCounters restore;
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> pipeline;

    Fixture()
    {
        ALMetalDevice::setCommandTimingEnabled(false);
        ALMetalDevice::beginRenderInfoFrame(false);
        device = ALMetalDevice::create(&error);
        INFO(error); REQUIRE(device);
        library = ALMetalShaderLibrary::compileSource(*device, source, "RenderInfo actual workload", &error);
        INFO(error); REQUIRE(library);
        pipeline = makePipeline(false, false);
    }
    std::unique_ptr<ALMetalRenderPipeline> makePipeline(bool patches, bool indirect)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.vertex_function = patches ? "counter_patch" : "counter_vertex";
        descriptor.fragment_function = "counter_fragment";
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.support_indirect_commands = indirect;
        descriptor.tessellation.enabled = patches;
        descriptor.tessellation.maximum_factor = patches ? 4 : 1;
        descriptor.label = "RenderInfo real native PSO";
        auto result = ALMetalRenderPipeline::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result);
        return result;
    }
    std::unique_ptr<ALMetalBuffer> buffer(const void* bytes, std::size_t length)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length = length;
        descriptor.label = "RenderInfo real native input";
        auto result = ALMetalBuffer::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result);
        REQUIRE(result->write(0, bytes, length, &error));
        return result;
    }
    std::unique_ptr<ALMetalRenderTarget> target()
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = descriptor.height = side;
        descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.label = "RenderInfo full pixel oracle";
        auto result = ALMetalRenderTarget::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result);
        return result;
    }
    std::unique_ptr<ALMetalRenderEncoder> encoder(ALMetalCommandBuffer& command,
        ALMetalRenderTarget& target, const ALMetalRenderPipeline* selected = nullptr)
    {
        auto pass = target.makeRenderPass({}, &error); REQUIRE(pass);
        auto result = ALMetalRenderEncoder::begin(command, *pass, "RenderInfo test encoder", &error);
        INFO(error); REQUIRE(result);
        REQUIRE(result->setPipeline(selected ? *selected : *pipeline, &error));
        REQUIRE(result->setViewport({0,0,side,side,0,1}, &error));
        REQUIRE(result->setCullMode(ALMetalCullMode::none, &error));
        return result;
    }
    void pixels(const ALMetalTexture& texture)
    {
        std::array<std::uint8_t, side * side * 4> bytes;
        REQUIRE(texture.readback(*device, 0, 0, {}, bytes.data(), side * 4, &error));
        for (unsigned i = 0; i < side * side; ++i)
        {
            CAPTURE(i);
            CHECK(bytes[i*4] == 0);
            CHECK(bytes[i*4+1] == 255);
            CHECK(bytes[i*4+2] == 0);
            CHECK(bytes[i*4+3] == 255);
        }
    }
};

void knownCounts(std::uint64_t calls, std::uint64_t elements, bool enabled = true)
{
    const auto counts = ALMetalDevice::renderInfo();
    CHECK(counts.enabled == enabled);
    CHECK(counts.draw_calls_known);
    CHECK(counts.input_elements_known);
    CHECK(counts.draw_calls == calls);
    CHECK(counts.input_elements == elements);
    CHECK(counts.indirect_submissions == 0);
    CHECK(counts.indirect_command_slots == 0);
}
}

TEST_CASE("almetal: RenderInfo counts actual direct emissions and joined elements with timing off",
          "[almetal][render-info]")
{
    @autoreleasepool
    {
        Fixture fixture;
        bool enabled = true;
        SECTION("explicitly armed") { enabled = true; }
        SECTION("default disabled leaves real draws unchanged") { enabled = false; }
        auto target = fixture.target();
        const std::array<std::uint16_t, 6> values{0,1,2,0,1,2};
        auto indices = fixture.buffer(values.data(), sizeof(values));
        auto command = fixture.device->makeCommandBuffer("RenderInfo direct and joined", &fixture.error);
        REQUIRE(command);
        const auto timing_before = ALMetalDevice::commandTiming();
        const auto submissions = ALMetalDevice::committedCommandBufferCount();
        const auto timed_draws = ALMetalDevice::encodedDrawCallCount();
        ALMetalDevice::beginRenderInfoFrame(enabled);
        auto encoder = fixture.encoder(*command, *target);
        CHECK_FALSE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 0, 1, &fixture.error));
        CHECK_FALSE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 0, &fixture.error));
        CHECK_FALSE(encoder->drawIndexed(ALMetalPrimitiveType::triangle, 3, ALMetalIndexType::uint16,
            *indices, indices->length(), 1, &fixture.error));
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 2, &fixture.error));
        REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle, 3, ALMetalIndexType::uint16,
            *indices, 0, 3, &fixture.error));
        REQUIRE(encoder->setIndexedTriangleJoining(true, &fixture.error));
        REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle, 3, ALMetalIndexType::uint16,
            *indices, 0, 1, &fixture.error));
        REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle, 3, ALMetalIndexType::uint16,
            *indices, 6, 1, &fixture.error));
        // Two contiguous requests become one actual draw with six indices.
        // Encoder-local totals publish once, even before command submission.
        knownCounts(0, 0, enabled);
        REQUIRE(encoder->end(&fixture.error));
        knownCounts(enabled ? 3 : 0, enabled ? 21 : 0, enabled);
        CHECK_FALSE(encoder->end(&fixture.error));
        encoder.reset();
        knownCounts(enabled ? 3 : 0, enabled ? 21 : 0, enabled);
        CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);
        CHECK(ALMetalDevice::encodedDrawCallCount() == timed_draws);
        CHECK_FALSE(ALMetalDevice::commandTimingEnabled());
        REQUIRE(command->commitAndWait(&fixture.error));
        fixture.pixels(*target->colorTexture(0));
        const auto timing_after = ALMetalDevice::commandTiming();
        CHECK(timing_after.submitted == timing_before.submitted);
        CHECK(timing_after.completed == timing_before.completed);
        CHECK(timing_after.timed == timing_before.timed);
        CHECK(timing_after.gpu_time_nanoseconds == timing_before.gpu_time_nanoseconds);
    }
}

TEST_CASE("almetal: RenderInfo counts patch control points and instances after real admission",
          "[almetal][render-info]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto target = fixture.target();
        auto patch = fixture.makePipeline(true, false);
        std::array<std::uint16_t, 12> values;
        values.fill(0x3c00); // Two quad patches, each six half factors set to one.
        auto factors = fixture.buffer(values.data(), sizeof(values));
        auto command = fixture.device->makeCommandBuffer("RenderInfo tessellated inputs", &fixture.error);
        REQUIRE(command);
        ALMetalDevice::beginRenderInfoFrame(true);
        auto encoder = fixture.encoder(*command, *target, patch.get());
        CHECK_FALSE(encoder->drawPatches(4,0,2,3,0,&fixture.error));
        REQUIRE(encoder->setTessellationFactorBuffer(*factors, 0, 0, &fixture.error));
        CHECK_FALSE(encoder->drawPatches(33,0,2,3,0,&fixture.error));
        REQUIRE(encoder->drawPatches(4,0,2,3,0,&fixture.error));
        REQUIRE(encoder->end(&fixture.error));
        knownCounts(1,24);
        REQUIRE(command->commitAndWait(&fixture.error));
        fixture.pixels(*target->colorTexture(0));
    }
}

TEST_CASE("almetal: RenderInfo frame resets reject old encoders and count abandonment as encoding",
          "[almetal][render-info]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto target = fixture.target();
        const auto submissions = ALMetalDevice::committedCommandBufferCount();
        ALMetalDevice::beginRenderInfoFrame(true);
        auto old_command = fixture.device->makeCommandBuffer("RenderInfo previous frame", &fixture.error);
        REQUIRE(old_command);
        auto old_encoder = fixture.encoder(*old_command, *target);
        REQUIRE(old_encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,&fixture.error));
        ALMetalDevice::beginRenderInfoFrame(true);
        REQUIRE(old_encoder->end(&fixture.error));
        knownCounts(0,0);
        old_command.reset();

        auto command = fixture.device->makeCommandBuffer("RenderInfo current encoded frame", &fixture.error);
        REQUIRE(command);
        auto encoder = fixture.encoder(*command, *target);
        REQUIRE(encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,&fixture.error));
        // RAII endEncoding is an emission boundary too; commit/completion is
        // deliberately not required by the original CPU-issued draw unit.
        encoder.reset();
        command.reset();
        knownCounts(1,3);
        CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);
        auto stale_command = fixture.device->makeCommandBuffer("RenderInfo disable boundary", &fixture.error);
        REQUIRE(stale_command);
        auto stale_encoder = fixture.encoder(*stale_command, *target);
        REQUIRE(stale_encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,&fixture.error));
        ALMetalDevice::beginRenderInfoFrame(false);
        ALMetalDevice::beginRenderInfoFrame(true);
        stale_encoder.reset();
        knownCounts(0,0);
    }
}

TEST_CASE("almetal: RenderInfo distinguishes exact static draws from indirect range metadata",
          "[almetal][render-info]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto target = fixture.target();
        auto indirect_pipeline = fixture.makePipeline(false, true);
        auto indirect = ALMetalShadowIndirectCommands::create(*fixture.device, &fixture.error);
        INFO(fixture.error); REQUIRE(indirect);
        if (!indirect->supported()) SKIP("Native indirect commands unavailable on this Metal device");
        const std::array<float, 12> positions{-1,-1,.5f,1, 3,-1,.5f,1, -1,3,.5f,1};
        const std::array<std::uint16_t, 6> indices{0,1,2,0,1,2};
        const std::array<std::uint8_t, ALMetalShadowIndirectCommands::constant_size> constants{};
        auto vertex_buffer = fixture.buffer(positions.data(),sizeof(positions));
        auto index_buffer = fixture.buffer(indices.data(),sizeof(indices));
        auto constant_buffer = fixture.buffer(constants.data(),sizeof(constants));
        std::array<ALMetalShadowIndirectDraw, 2> draws;
        for (unsigned i = 0; i < draws.size(); ++i)
        {
            draws[i].positions = vertex_buffer.get(); draws[i].indices = index_buffer.get();
            draws[i].constants = constant_buffer.get(); draws[i].vertex_count = 3;
            draws[i].index_count = i == 0 ? 3 : 6;
        }
        auto command = fixture.device->makeCommandBuffer("RenderInfo native static and ICB", &fixture.error);
        REQUIRE(command);
        auto direct_batch = indirect->prepareDirect(*command,draws,&fixture.error); REQUIRE(direct_batch);
        auto indirect_batch = indirect->prepare(*command,draws,&fixture.error); REQUIRE(indirect_batch);
        ALMetalDevice::beginRenderInfoFrame(true);
        auto encoder = fixture.encoder(*command,*target);
        CHECK_FALSE(indirect->executeDirect(*encoder,*direct_batch,0,3,&fixture.error));
        REQUIRE(indirect->executeDirect(*encoder,*direct_batch,0,0,&fixture.error));
        REQUIRE(indirect->executeDirect(*encoder,*direct_batch,0,2,&fixture.error));
        REQUIRE(encoder->end(&fixture.error));
        knownCounts(2,9);
        encoder = fixture.encoder(*command,*target,indirect_pipeline.get());
        CHECK_FALSE(indirect->execute(*encoder,*indirect_batch,1,2,&fixture.error));
        REQUIRE(indirect->execute(*encoder,*indirect_batch,0,0,&fixture.error));
        REQUIRE(indirect->execute(*encoder,*indirect_batch,0,2,&fixture.error));
        REQUIRE(encoder->end(&fixture.error));
        const auto counts = ALMetalDevice::renderInfo();
        CHECK(counts.enabled);
        CHECK_FALSE(counts.draw_calls_known);
        CHECK_FALSE(counts.input_elements_known);
        CHECK(counts.draw_calls == 2); // Exact direct subtotal, never ICB slot count.
        CHECK(counts.input_elements == 9);
        CHECK(counts.indirect_submissions == 1);
        CHECK(counts.indirect_command_slots == 2);
        REQUIRE(command->commitAndWait(&fixture.error));
        fixture.pixels(*target->colorTexture(0));
    }
}

TEST_CASE("almetal: RenderInfo counts actual production UI emissions once",
          "[almetal][render-info]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto scheduler = ALMetalFrameScheduler::create(*fixture.device, 3, &fixture.error); REQUIRE(scheduler);
        auto renderer = ALMetalUIRenderer::createForTexture(*fixture.device,*scheduler,
            ALMetalPixelFormat::rgba8_unorm,&fixture.error);
        INFO(fixture.error); REQUIRE(renderer);
        constexpr unsigned uv_offset = 3*4*sizeof(float), color_offset = uv_offset+3*2*sizeof(float);
        std::array<std::uint8_t, color_offset+3*4> bytes{};
        const std::array<float,12> positions{-1,-1,0,1, 3,-1,0,1, -1,3,0,1};
        std::memcpy(bytes.data(),positions.data(),sizeof(positions));
        for (unsigned i = 0; i < 3; ++i) { bytes[color_offset+i*4+1]=255; bytes[color_offset+i*4+3]=255; }
        auto vertices = fixture.buffer(bytes.data(),bytes.size());
        ALMetalUIBatch batch;
        batch.vertices = vertices.get(); batch.vertex_count = 3;
        batch.texcoord_offset = uv_offset; batch.color_offset = color_offset;
        batch.shader = ALMetalUIShader::vertex_color; batch.blend.enabled = false;
        batch.viewport = {0,0,side,side,0,1}; batch.scissor = {0,0,side,side};
        batch.target_width = batch.target_height = side;
        for (unsigned i : {0u,5u,10u,15u})
            batch.matrices.modelview[i] = batch.matrices.projection[i] =
            batch.matrices.modelview_projection[i] = batch.matrices.inverse_projection[i] =
            batch.matrices.texture0[i] = 1.f;
        ALMetalDevice::beginRenderInfoFrame(true);
        REQUIRE(renderer->draw(batch,&fixture.error));
        REQUIRE(renderer->endFrame(&fixture.error));
        REQUIRE(scheduler->waitUntilIdle(1000,&fixture.error));
        CHECK(renderer->stats().batches == 1);
        knownCounts(1,3);
        REQUIRE(renderer->outputTexture());
        fixture.pixels(*renderer->outputTexture());
    }
}

TEST_CASE("almetal: RenderInfo retains internal UI background HUD and resumed-pass draws",
          "[almetal][render-info][render-info-mixed-ui]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto scheduler = ALMetalFrameScheduler::create(*fixture.device, 3, &fixture.error); REQUIRE(scheduler);
        auto renderer = ALMetalUIRenderer::createForTexture(*fixture.device,*scheduler,
            ALMetalPixelFormat::rgba8_unorm,&fixture.error); REQUIRE(renderer);
        ALMetalTextureDescriptor texture_descriptor;
        texture_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        texture_descriptor.width = texture_descriptor.height = side;
        texture_descriptor.label = "RenderInfo retained blue background";
        auto background = ALMetalTexture::create(*fixture.device,texture_descriptor,&fixture.error);
        REQUIRE(background);
        std::array<std::uint8_t,side*side*4> blue{};
        for (unsigned i=0;i<side*side;++i) { blue[i*4+2]=255; blue[i*4+3]=255; }
        auto upload = fixture.device->makeCommandBuffer("RenderInfo blue backing upload",&fixture.error);
        REQUIRE(upload);
        REQUIRE(background->upload(*upload,0,0,{},blue.data(),side*4,&fixture.error));
        REQUIRE(upload->commitAndWait(&fixture.error));
        REQUIRE(renderer->setFrameBackground(background.get(),&fixture.error));
        constexpr unsigned uv_offset=3*4*sizeof(float), color_offset=uv_offset+3*2*sizeof(float);
        const std::array<float,12> positions{-1,-1,0,1, 3,-1,0,1, -1,3,0,1};
        std::array<std::uint8_t,color_offset+3*4> bytes{};
        std::memcpy(bytes.data(),positions.data(),sizeof(positions));
        for (unsigned i=0;i<3;++i) { bytes[color_offset+i*4+1]=255; bytes[color_offset+i*4+3]=255; }
        auto vertices = fixture.buffer(bytes.data(),bytes.size());
        const std::array<std::uint16_t,3> indices{0,1,2};
        auto index_buffer = fixture.buffer(indices.data(),sizeof(indices));
        ALMetalUIBatch batch;
        batch.vertices=vertices.get(); batch.vertex_count=3;
        batch.texcoord_offset=uv_offset; batch.color_offset=color_offset;
        batch.shader=ALMetalUIShader::vertex_color; batch.blend.enabled=false;
        batch.viewport={0,0,side,side,0,1}; batch.scissor={0,0,side/2,side};
        batch.target_width=batch.target_height=side;
        for (unsigned i:{0u,5u,10u,15u})
            batch.matrices.modelview[i]=batch.matrices.projection[i]=
            batch.matrices.modelview_projection[i]=batch.matrices.inverse_projection[i]=
            batch.matrices.texture0[i]=1.f;
        ALMetalHUDIndexedBatch hud;
        hud.vertices=vertices.get(); hud.vertex_count=3;
        hud.indices=index_buffer.get(); hud.index_count=3;
        hud.shader=ALMetalHUDShader::debug; hud.double_sided=true;
        hud.blend.enabled=false; hud.highlight_color[0]=1.f;
        hud.highlight_color[1]=hud.highlight_color[2]=0.f;
        hud.viewport=batch.viewport; hud.scissor={side/2,0,side/2,side};
        hud.matrices=batch.matrices; hud.target_width=hud.target_height=side;
        ALMetalDevice::beginRenderInfoFrame(true);
        REQUIRE(renderer->draw(batch,&fixture.error));
        // Snapshot a still-blue pixel and resume the same native UI frame.
        // The blit itself contributes no render call; resumed foreground
        // and typed HUD geometry each contribute one actual native emission.
        auto snapshot = renderer->copyFrameColorPixel(side-1,side/2,&fixture.error);
        INFO(fixture.error); REQUIRE(snapshot);
        REQUIRE(renderer->draw(batch,&fixture.error));
        REQUIRE(renderer->drawHUDIndexed(hud,&fixture.error));
        REQUIRE(renderer->endFrame(&fixture.error));
        REQUIRE(scheduler->waitUntilIdle(1000,&fixture.error));
        CHECK(renderer->stats().batches==3);
        knownCounts(4,12); // Background + two foreground batches + indexed HUD.
        std::array<std::uint8_t,4> before_hud{};
        REQUIRE(snapshot->readback(*fixture.device,0,0,{},before_hud.data(),4,&fixture.error));
        CHECK(before_hud==std::array<std::uint8_t,4>{0,0,255,255});
        std::array<std::uint8_t,side*side*4> result{};
        REQUIRE(renderer->outputTexture()->readback(*fixture.device,0,0,{},result.data(),side*4,&fixture.error));
        for (unsigned y=0;y<side;++y)
            for (unsigned x=0;x<side;++x)
            {
                CAPTURE(x,y);
                const auto i=(y*side+x)*4;
                CHECK(result[i]==(x<side/2 ? 0 : 255));
                CHECK(result[i+1]==(x<side/2 ? 255 : 0));
                CHECK(result[i+2]==0);
                CHECK(result[i+3]==255);
            }
    }
}
