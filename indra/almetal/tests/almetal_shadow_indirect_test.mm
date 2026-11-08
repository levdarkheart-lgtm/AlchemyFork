/**
 * @file almetal_shadow_indirect_test.mm
 * @brief Ordered static-shadow ICB execution and completion-lifetime regressions.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalshadowindirect.h"

#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <future>
#include <limits>
#include <vector>

namespace
{
    constexpr unsigned width = 32, height = 8;
    struct alignas(16) Constants
    {
        std::array<float, 16> mvp{};
        std::array<std::uint32_t, 4> config{};
    };
    static_assert(sizeof(Constants) == ALMetalShadowIndirectCommands::constant_size);

    const char* source = R"metal(
#include <metal_stdlib>
using namespace metal;
struct Constants { float4x4 mvp; uint4 config; };
struct Out { float4 position [[position]]; float value; };
vertex Out indirect_vertex(uint index [[vertex_id]],
    device const float4* positions [[buffer(0)]],
    constant Constants& constants [[buffer(1)]])
{
    float4 clip = constants.mvp * positions[index];
    if (constants.config.x) clip.z = clip.w - clip.z;
    return {clip, float(constants.config.y) / 1024.f};
}
fragment float indirect_fragment(Out in [[stage_in]])
{
    return in.value + in.position.z / 8.f;
}
)metal";

    struct Fixture
    {
        std::string error;
        std::unique_ptr<ALMetalDevice> device;
        std::unique_ptr<ALMetalShaderLibrary> library;
        std::unique_ptr<ALMetalRenderPipeline> pipeline, scalar_pipeline;
        std::unique_ptr<ALMetalDepthStencilState> depth;
        std::unique_ptr<ALMetalShadowIndirectCommands> indirect;

        Fixture()
        {
            device = ALMetalDevice::create(&error);
            INFO(error);
            REQUIRE(device);
            library = ALMetalShaderLibrary::compileSource(
                *device, source, "Indirect shadow regression", &error);
            REQUIRE(library);
            ALMetalRenderPipelineDescriptor descriptor;
            descriptor.library = library.get();
            descriptor.vertex_function = "indirect_vertex";
            descriptor.fragment_function = "indirect_fragment";
            descriptor.colors[0].format = ALMetalPixelFormat::r32_float;
            descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
            descriptor.support_indirect_commands = true;
            descriptor.label = "Indirect shadow regression PSO";
            pipeline = ALMetalRenderPipeline::create(*device, descriptor, &error);
            REQUIRE(pipeline);
            descriptor.support_indirect_commands = false;
            scalar_pipeline = ALMetalRenderPipeline::create(*device, descriptor, &error);
            REQUIRE(scalar_pipeline);
            ALMetalDepthStencilDescriptor depth_descriptor;
            depth_descriptor.depth_compare = ALMetalCompareFunction::always;
            depth_descriptor.depth_write_enabled = true;
            depth_descriptor.label = "Indirect shadow regression depth";
            depth = ALMetalDepthStencilState::create(*device, depth_descriptor, &error);
            REQUIRE(depth);
            indirect = ALMetalShadowIndirectCommands::create(*device, &error);
            REQUIRE(indirect);
            REQUIRE(indirect->supported());
        }

        std::unique_ptr<ALMetalBuffer> buffer(const void* data, std::size_t length)
        {
            ALMetalBufferDescriptor descriptor;
            descriptor.length = length;
            descriptor.storage = ALMetalStorageMode::shared;
            descriptor.label = "Indirect shadow regression data";
            auto result = ALMetalBuffer::create(*device, descriptor, &error);
            REQUIRE(result);
            REQUIRE(result->write(0, data, length, &error));
            return result;
        }

        std::unique_ptr<ALMetalRenderTarget> target()
        {
            ALMetalRenderTargetDescriptor descriptor;
            descriptor.width = width;
            descriptor.height = height;
            descriptor.colors[0].format = ALMetalPixelFormat::r32_float;
            descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
            descriptor.label = "Indirect shadow regression target";
            auto result = ALMetalRenderTarget::create(*device, descriptor, &error);
            REQUIRE(result);
            return result;
        }

        std::unique_ptr<ALMetalRenderEncoder> encoder(
            ALMetalCommandBuffer& command, ALMetalRenderTarget& target,
            bool indirect_pipeline = true)
        {
            auto pass = target.makeRenderPass({}, &error);
            REQUIRE(pass);
            auto result = ALMetalRenderEncoder::begin(command, *pass,
                "Indirect shadow regression pass", &error);
            REQUIRE(result);
            REQUIRE(result->setPipeline(
                indirect_pipeline ? *pipeline : *scalar_pipeline, &error));
            REQUIRE(result->setViewport({0., 0., width, height, 0., 1.}, &error));
            REQUIRE(result->setCullMode(ALMetalCullMode::none, &error));
            REQUIRE(result->setDepthClipMode(ALMetalDepthClipMode::clamp, &error));
            REQUIRE(result->setDepthStencilState(depth.get(), &error));
            return result;
        }

        std::array<float, width * height> pixels(ALMetalRenderTarget& target)
        {
            std::array<float, width * height> result{};
            REQUIRE(target.colorTexture(0)->readback(*device, 0, 0, {},
                result.data(), width * sizeof(float), &error));
            return result;
        }
    };

    struct Draws
    {
        std::unique_ptr<ALMetalBuffer> positions, indices16, indices32, constants;
        std::vector<ALMetalShadowIndirectDraw> values;

        explicit Draws(Fixture& fixture, std::size_t count, unsigned seed = 0)
        {
            // Two independently offset position blocks and both index widths.
            const std::array<float, 32> points = {
                -1,-1,.25f,1, 3,-1,.25f,1, -1,3,.25f,1, 0,0,.25f,1,
                -1,-1,.75f,1, 3,-1,.75f,1, -1,3,.75f,1, 0,0,.75f,1};
            const std::array<std::uint16_t, 8> short_indices = {7,7,0,1,2,0,1,2};
            const std::array<std::uint32_t, 8> long_indices = {7,7,0,1,2,0,1,2};
            positions = fixture.buffer(points.data(), sizeof(points));
            indices16 = fixture.buffer(short_indices.data(), sizeof(short_indices));
            indices32 = fixture.buffer(long_indices.data(), sizeof(long_indices));
            std::vector<Constants> data(std::max<std::size_t>(1, count));
            for (std::size_t i = 0; i < data.size(); ++i)
            {
                auto& constant = data[i];
                constant.mvp[0] = constant.mvp[5] = constant.mvp[10] = constant.mvp[15] = 1.f;
                constant.config[0] = (i + seed) % 2;
                constant.config[1] = unsigned(i + seed + 1);
                // Include positive/negative clip depths with depth clamping.
                if (i % 5 == 2) constant.mvp[14] = 2.f;
                if (i % 5 == 3) constant.mvp[14] = -2.f;
            }
            constants = fixture.buffer(data.data(), data.size() * sizeof(Constants));
            values.reserve(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                ALMetalShadowIndirectDraw draw;
                draw.positions = positions.get();
                draw.indices = i % 2 ? indices32.get() : indices16.get();
                draw.constants = constants.get();
                draw.position_offset = (i % 2) * 64;
                draw.index_offset = i % 2 ? 8 : 4;
                draw.constant_offset = i * sizeof(Constants);
                draw.vertex_count = 4;
                draw.index_count = i % 3 ? 3 : 6;
                draw.index_type = i % 2 ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
                values.push_back(draw);
            }
        }
    };

    void bind(Fixture& fixture, ALMetalRenderEncoder& encoder,
              const ALMetalShadowIndirectDraw& draw)
    {
        REQUIRE(encoder.setVertexBuffer(0, *draw.positions, draw.position_offset, &fixture.error));
        REQUIRE(encoder.setVertexBuffer(1, *draw.constants, draw.constant_offset, &fixture.error));
    }

    void draw(Fixture& fixture, ALMetalRenderEncoder& encoder,
              const ALMetalShadowIndirectDraw& command)
    {
        bind(fixture, encoder, command);
        REQUIRE(encoder.drawIndexed(ALMetalPrimitiveType::triangle,
            command.index_count, command.index_type, *command.indices,
            command.index_offset, 1, &fixture.error));
    }
}

TEST_CASE("almetal: shadow indirect ranges match scalar order and restore buffer bindings",
          "[almetal][shadow-indirect]")
{
    @autoreleasepool
    {
        Fixture fixture;
        Draws draws(fixture, 257);
        auto reference = fixture.target(), candidate = fixture.target(), bulk = fixture.target();
        auto command = fixture.device->makeCommandBuffer("Indirect ordered ranges", &fixture.error);
        REQUIRE(command);
        auto batch = fixture.indirect->prepare(*command, draws.values, &fixture.error);
        REQUIRE(batch);
        REQUIRE(batch->drawCount() == draws.values.size());
        CHECK(batch->isIndirect());
        auto direct_batch = fixture.indirect->prepareDirect(*command, draws.values, &fixture.error);
        REQUIRE(direct_batch);
        CHECK_FALSE(direct_batch->isIndirect());
        const bool timing_was_enabled = ALMetalDevice::commandTimingEnabled();
        ALMetalDevice::setCommandTimingEnabled(true);
        struct RestoreTiming
        {
            bool value;
            ~RestoreTiming() { ALMetalDevice::setCommandTimingEnabled(value); }
        } restore{timing_was_enabled};
        const auto census_before = ALMetalDevice::encodedDrawCallCount();
        for (unsigned mode : {0u, 1u, 2u})
        {
            auto encoder = fixture.encoder(*command,
                mode == 0 ? *reference : mode == 1 ? *candidate : *bulk, mode == 1);
            REQUIRE(encoder->setScissor({0, 0, 8, height}, &fixture.error));
            // Seed the scalar cache before indirect execution, then restore
            // exactly those identities afterwards. ICB changes must invalidate it.
            bind(fixture, *encoder, draws.values[0]);
            if (mode == 1)
                REQUIRE(fixture.indirect->execute(*encoder, *batch, 0, 128, &fixture.error));
            else if (mode == 2)
                REQUIRE(fixture.indirect->executeDirect(*encoder, *direct_batch, 0, 128, &fixture.error));
            else
                for (std::size_t i = 0; i < 128; ++i) draw(fixture, *encoder, draws.values[i]);
            REQUIRE(encoder->setScissor({8, 0, 8, height}, &fixture.error));
            draw(fixture, *encoder, draws.values[0]);
            REQUIRE(encoder->setScissor({16, 0, 8, height}, &fixture.error));
            if (mode == 1)
                REQUIRE(fixture.indirect->execute(*encoder, *batch, 128, 129, &fixture.error));
            else if (mode == 2)
                REQUIRE(fixture.indirect->executeDirect(*encoder, *direct_batch, 128, 129, &fixture.error));
            else
                for (std::size_t i = 128; i < 257; ++i) draw(fixture, *encoder, draws.values[i]);
            REQUIRE(encoder->setScissor({24, 0, 8, height}, &fixture.error));
            draw(fixture, *encoder, draws.values[1]);
            REQUIRE(encoder->end(&fixture.error));
        }
        CHECK(ALMetalDevice::encodedDrawCallCount() - census_before == 3 * (257 + 2));
        CHECK(fixture.indirect->stats().executions == 2);
        CHECK(fixture.indirect->stats().executed_draws == 257);
        // Four unique native buffers, declared once for both ordered ranges.
        CHECK(fixture.indirect->stats().resource_declarations == 4);
        CHECK(fixture.indirect->stats().direct_executions == 2);
        CHECK(fixture.indirect->stats().direct_executed_draws == 257);
        REQUIRE(command->commitAndWait(&fixture.error));
        const auto expected = fixture.pixels(*reference);
        const auto actual = fixture.pixels(*candidate);
        CHECK(actual == expected);
        CHECK(fixture.pixels(*bulk) == expected);
        CHECK(std::count_if(actual.begin(), actual.end(), [](float value)
            { return std::isfinite(value) && value != 0.f; }) == width * height);
        CHECK(actual[0] != actual[8]);
        CHECK(actual[16] != actual[24]);
    }
}

TEST_CASE("almetal: shadow indirect leases retain resources and retire on actual completion",
          "[almetal][shadow-indirect]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto target_a = fixture.target(), target_b = fixture.target();
        auto command_a = fixture.device->makeCommandBuffer("Indirect pending view A", &fixture.error);
        auto command_b = fixture.device->makeCommandBuffer("Indirect pending view B", &fixture.error);
        REQUIRE(command_a);
        REQUIRE(command_b);
        const auto prepare_view = [&](ALMetalCommandBuffer& command,
                                      ALMetalRenderTarget& target, unsigned seed)
        {
            Draws draws(fixture, 3, seed);
            auto batch = fixture.indirect->prepare(command, draws.values, &fixture.error);
            REQUIRE(batch);
            auto encoder = fixture.encoder(command, target);
            REQUIRE(fixture.indirect->execute(*encoder, *batch, 0, 3, &fixture.error));
            REQUIRE(encoder->end(&fixture.error));
            // Both the public lease and every source buffer wrapper die
            // before submission. A live uncommitted command still owns its slot.
        };
        prepare_view(*command_a, *target_a, 0);
        prepare_view(*command_b, *target_b, 100);
        CHECK(fixture.indirect->stats().slot_count == 2);
        CHECK(fixture.indirect->stats().slot_reuses == 0);

        id<MTLDevice> native_device = (__bridge id<MTLDevice>)fixture.device->nativeHandle();
        id<MTLSharedEvent> gate = [native_device newSharedEvent];
        REQUIRE(gate);
        struct SignalOnExit
        {
            id<MTLSharedEvent> event;
            ~SignalOnExit() { event.signaledValue = 1; }
        } signal{gate};
        id<MTLCommandBuffer> native_a = (__bridge id<MTLCommandBuffer>)command_a->nativeHandle();
        // Wait after encoded draws so A cannot complete while selecting C.
        [native_a encodeWaitForEvent:gate value:1];
        REQUIRE(command_a->commit(&fixture.error));
        REQUIRE(command_b->commit(&fixture.error));
        command_a.reset();
        command_b.reset();
        native_a = nil;
        Draws next(fixture, 1, 200);
        auto command_c = fixture.device->makeCommandBuffer("Indirect third pending view", &fixture.error);
        REQUIRE(command_c);
        auto third = fixture.indirect->prepare(*command_c, next.values, &fixture.error);
        REQUIRE(third);
        CHECK(fixture.indirect->stats().slot_count == 3);
        third.reset();
        command_c.reset(); // Explicit abandonment permits that slot's reuse.
        auto abandoned_reuse_command = fixture.device->makeCommandBuffer("Indirect abandoned reuse", &fixture.error);
        REQUIRE(abandoned_reuse_command);
        auto reused = fixture.indirect->prepare(*abandoned_reuse_command, next.values, &fixture.error);
        REQUIRE(reused);
        CHECK(fixture.indirect->stats().slot_count == 3);
        CHECK(fixture.indirect->stats().slot_reuses == 1);
        gate.signaledValue = 1;
        REQUIRE(fixture.device->waitUntilIdle(&fixture.error));
        auto pixels_a = fixture.pixels(*target_a), pixels_b = fixture.pixels(*target_b);
        CHECK(pixels_a[0] != pixels_b[0]);
        // Depth clamping affects the depth attachment; the fragment position
        // remains the interpolated clip depth used by this diagnostic color.
        CHECK(std::abs(pixels_a[0] - (3.f / 1024.f + 2.25f / 8.f)) < 1e-6f);
        CHECK(std::abs(pixels_b[0] - (103.f / 1024.f + 2.25f / 8.f)) < 1e-6f);

        // Reuse a completed 3-command slot with one draw, then expand it.
        // A reset tail must rebind its buffers and re-record its indexed draw.
        reused.reset();
        abandoned_reuse_command.reset();
        for (std::size_t count : {std::size_t(1), std::size_t(3)})
        {
            Draws later(fixture, count, 300);
            auto command = fixture.device->makeCommandBuffer("Indirect shrink expand", &fixture.error);
            REQUIRE(command);
            auto batch = fixture.indirect->prepare(*command, later.values, &fixture.error);
            REQUIRE(batch);
            auto encoder = fixture.encoder(*command, *target_a);
            REQUIRE(fixture.indirect->execute(*encoder, *batch, 0, count, &fixture.error));
            REQUIRE(encoder->end(&fixture.error));
            REQUIRE(command->commitAndWait(&fixture.error));
            auto result = fixture.pixels(*target_a);
            const float expected = float(300 + count) / 1024.f + (count == 1 ? .25f : 2.25f) / 8.f;
            CHECK(std::abs(result[0] - expected) < 1e-6f);
        }
        CHECK(fixture.indirect->stats().slot_count == 3);
        CHECK(fixture.indirect->stats().slot_reuses == 3);

        // Hero cascades share one parent command. Releasing the first public
        // lease must not let the second preparation overwrite its ICB/data.
        auto shared_command = fixture.device->makeCommandBuffer(
            "Indirect two views in one command", &fixture.error);
        REQUIRE(shared_command);
        const auto before_shared = fixture.indirect->stats();
        prepare_view(*shared_command, *target_a, 400);
        prepare_view(*shared_command, *target_b, 501);
        CHECK(fixture.indirect->stats().slot_count == before_shared.slot_count);
        CHECK(fixture.indirect->stats().slot_reuses == before_shared.slot_reuses + 2);
        REQUIRE(shared_command->commitAndWait(&fixture.error));
        pixels_a = fixture.pixels(*target_a);
        pixels_b = fixture.pixels(*target_b);
        CHECK(std::abs(pixels_a[0] - (403.f / 1024.f + 2.25f / 8.f)) < 1e-6f);
        CHECK(std::abs(pixels_b[0] - (504.f / 1024.f - 1.25f / 8.f)) < 1e-6f);
    }
}

TEST_CASE("almetal: shadow indirect validation rejects partial and foreign execution",
          "[almetal][shadow-indirect]")
{
    @autoreleasepool
    {
        Fixture fixture;
        Draws draws(fixture, 2);
        auto command = fixture.device->makeCommandBuffer("Indirect input validation", &fixture.error);
        REQUIRE(command);
        auto empty = fixture.indirect->prepare(*command, {}, &fixture.error);
        REQUIRE(empty);
        CHECK(empty->drawCount() == 0);
        const auto before = fixture.indirect->stats();
        const auto rejects = [&](ALMetalShadowIndirectDraw invalid)
        {
            const std::array list{draws.values[0], invalid};
            CHECK_FALSE(fixture.indirect->prepare(*command, list, &fixture.error));
            CHECK(fixture.indirect->stats().prepared_draws == before.prepared_draws);
            CHECK(fixture.indirect->stats().slot_count == before.slot_count);
        };
        auto invalid = draws.values[1];
        invalid.positions = nullptr;
        rejects(invalid);
        invalid = draws.values[1]; invalid.position_offset = 1; rejects(invalid);
        invalid = draws.values[1]; invalid.constant_offset = 1; rejects(invalid);
        invalid = draws.values[1]; invalid.index_offset = 1; rejects(invalid);
        invalid = draws.values[1]; invalid.vertex_count = 0; rejects(invalid);
        invalid = draws.values[1]; invalid.index_count = 0; rejects(invalid);
        invalid = draws.values[1]; invalid.vertex_count = std::numeric_limits<std::uint32_t>::max(); rejects(invalid);
        invalid = draws.values[1]; invalid.index_count = std::numeric_limits<std::uint32_t>::max(); rejects(invalid);
        invalid = draws.values[1]; invalid.constant_offset = std::numeric_limits<std::size_t>::max() - 15; rejects(invalid);
        invalid = draws.values[1]; invalid.index_type = static_cast<ALMetalIndexType>(99); rejects(invalid);
        std::vector<ALMetalShadowIndirectDraw> too_many(
            ALMetalShadowIndirectCommands::maximum_draw_count + 1, draws.values[0]);
        CHECK_FALSE(fixture.indirect->prepare(*command, too_many, &fixture.error));
        auto batch = fixture.indirect->prepare(*command, draws.values, &fixture.error);
        REQUIRE(batch);
        auto target = fixture.target();
        auto encoder = fixture.encoder(*command, *target);
        REQUIRE(fixture.indirect->execute(*encoder, *empty, 0, 0, &fixture.error));
        CHECK_FALSE(fixture.indirect->execute(*encoder, *batch, 2, 1, &fixture.error));
        CHECK_FALSE(fixture.indirect->execute(*encoder, *batch, 0,
            std::numeric_limits<std::size_t>::max(), &fixture.error));
        REQUIRE(encoder->setPipeline(*fixture.scalar_pipeline, &fixture.error));
        CHECK_FALSE(fixture.indirect->execute(*encoder, *batch, 0, 1, &fixture.error));
        REQUIRE(encoder->setPipeline(*fixture.pipeline, &fixture.error));
        auto other_helper = ALMetalShadowIndirectCommands::create(*fixture.device, &fixture.error);
        REQUIRE(other_helper);
        CHECK_FALSE(other_helper->execute(*encoder, *batch, 0, 1, &fixture.error));
        REQUIRE(encoder->end(&fixture.error));
        CHECK_FALSE(fixture.indirect->execute(*encoder, *batch, 0, 1, &fixture.error));
        auto other_command = fixture.device->makeCommandBuffer("Indirect foreign command", &fixture.error);
        REQUIRE(other_command);
        auto other_encoder = fixture.encoder(*other_command, *target);
        CHECK_FALSE(fixture.indirect->execute(*other_encoder, *batch, 0, 1, &fixture.error));
        REQUIRE(other_encoder->end(&fixture.error));
        CHECK(fixture.indirect->stats().executed_draws == 0);
        REQUIRE(command->commitAndWait(&fixture.error));
        CHECK_FALSE(fixture.indirect->prepare(*command, draws.values, &fixture.error));

        // The last legal command remains addressable; the adjacent size
        // above was rejected before selecting or modifying a pooled slot.
        auto limit_command = fixture.device->makeCommandBuffer("Indirect exact command limit", &fixture.error);
        REQUIRE(limit_command);
        too_many.pop_back();
        auto limit_batch = fixture.indirect->prepare(*limit_command, too_many, &fixture.error);
        REQUIRE(limit_batch);
        CHECK(limit_batch->drawCount() == ALMetalShadowIndirectCommands::maximum_draw_count);
        auto limit_encoder = fixture.encoder(*limit_command, *target);
        REQUIRE(fixture.indirect->execute(*limit_encoder, *limit_batch,
            ALMetalShadowIndirectCommands::maximum_draw_count - 1, 1, &fixture.error));
        REQUIRE(limit_encoder->end(&fixture.error));
        REQUIRE(limit_command->commitAndWait(&fixture.error));
        CHECK(fixture.indirect->stats().executed_draws == 1);
    }
}

TEST_CASE("almetal: indirect owners survive renderer destruction and release completion cycles",
          "[almetal][shadow-indirect][completion]")
{
    @autoreleasepool
    {
        Fixture fixture;
        SECTION("native buffer owners survive helper and source wrappers")
        {
            auto target = fixture.target();
            auto command = fixture.device->makeCommandBuffer(
                "Indirect destroyed renderer", &fixture.error);
            REQUIRE(command);
            __weak id<MTLBuffer> weak_positions = nil;
            {
                Draws draws(fixture, 3);
                weak_positions = (__bridge id<MTLBuffer>)draws.positions->nativeHandle();
                auto batch = fixture.indirect->prepare(*command, draws.values, &fixture.error);
                REQUIRE(batch);
                auto encoder = fixture.encoder(*command, *target);
                REQUIRE(fixture.indirect->execute(*encoder, *batch, 0, 3, &fixture.error));
                REQUIRE(encoder->end(&fixture.error));
            }
            fixture.indirect.reset();
            // Neither useResources nor ICB vertex bindings retain this native
            // resource; only the command's explicit owner keeps it alive.
            CHECK(weak_positions != nil);
            REQUIRE(command->commitAndWait(&fixture.error));
            const auto pixels = fixture.pixels(*target);
            CHECK(std::abs(pixels[0] - (3.f / 1024.f + 2.25f / 8.f)) < 1e-6f);
        }
        SECTION("generic owners deduplicate and abandon without a callback cycle")
        {
            auto command = fixture.device->makeCommandBuffer(
                "Indirect abandoned owner", &fixture.error);
            REQUIRE(command);
            struct Owner
            {
                std::shared_ptr<const ALMetalCommandCompletion> completion;
            };
            auto owner = std::make_shared<Owner>();
            owner->completion = command->completionToken();
            std::weak_ptr<Owner> weak_owner = owner;
            CHECK_FALSE(command->retainUntilCompletion({}, &fixture.error));
            REQUIRE(command->retainUntilCompletion(owner, &fixture.error));
            REQUIRE(command->retainUntilCompletion(owner, &fixture.error));
            CHECK(owner.use_count() == 2);
            // An alias with a different address shares the same owner.
            REQUIRE(command->retainUntilCompletion(
                std::shared_ptr<const void>(owner, &owner->completion), &fixture.error));
            CHECK(owner.use_count() == 2);
            // Distinct control blocks must both survive even when their
            // aliases happen to expose exactly the same address.
            auto first_alias_owner = std::make_shared<int>(1);
            auto second_alias_owner = std::make_shared<int>(2);
            std::weak_ptr<int> weak_first = first_alias_owner;
            std::weak_ptr<int> weak_second = second_alias_owner;
            REQUIRE(command->retainUntilCompletion(
                std::shared_ptr<const void>(first_alias_owner, &fixture), &fixture.error));
            REQUIRE(command->retainUntilCompletion(
                std::shared_ptr<const void>(second_alias_owner, &fixture), &fixture.error));
            first_alias_owner.reset();
            second_alias_owner.reset();
            CHECK_FALSE(weak_first.expired());
            CHECK_FALSE(weak_second.expired());
            owner.reset();
            CHECK_FALSE(weak_owner.expired());
            command.reset();
            CHECK(weak_owner.expired());
            CHECK(weak_first.expired());
            CHECK(weak_second.expired());
        }
        SECTION("completed owners break a real native command token cycle")
        {
            auto command = fixture.device->makeCommandBuffer(
                "Indirect completed owner", &fixture.error);
            REQUIRE(command);
            id<MTLDevice> native_device = (__bridge id<MTLDevice>)fixture.device->nativeHandle();
            id<MTLSharedEvent> gate = [native_device newSharedEvent];
            REQUIRE(gate);
            struct SignalOnExit
            {
                id<MTLSharedEvent> event;
                ~SignalOnExit() { event.signaledValue = 1; }
            } signal{gate};
            id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)command->nativeHandle();
            [native encodeWaitForEvent:gate value:1];
            struct Owner
            {
                std::shared_ptr<const ALMetalCommandCompletion> completion;
                std::shared_ptr<std::promise<void>> destroyed;
                ~Owner() { destroyed->set_value(); }
            };
            auto destroyed = std::make_shared<std::promise<void>>();
            auto future = destroyed->get_future();
            auto owner = std::make_shared<Owner>();
            owner->completion = command->completionToken();
            owner->destroyed = destroyed;
            std::weak_ptr<Owner> weak_owner = owner;
            REQUIRE(command->retainUntilCompletion(owner, &fixture.error));
            owner.reset();
            REQUIRE(command->commit(&fixture.error));
            CHECK_FALSE(command->retainUntilCompletion(destroyed, &fixture.error));
            command.reset();
            native = nil;
            CHECK_FALSE(weak_owner.expired());
            gate.signaledValue = 1;
            REQUIRE(future.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
            CHECK(weak_owner.expired());
            REQUIRE(fixture.device->waitUntilIdle(&fixture.error));
        }
    }
}

TEST_CASE("almetal: native direct shadow packets isolate pending views and validate mode",
          "[almetal][shadow-indirect][shadow-direct]")
{
    @autoreleasepool
    {
        Fixture fixture;
        auto target_a = fixture.target(), target_b = fixture.target();
        auto command = fixture.device->makeCommandBuffer("Native static pending views", &fixture.error);
        REQUIRE(command);
        const auto prepare_view = [&](ALMetalRenderTarget& target, unsigned seed)
        {
            Draws draws(fixture, 3, seed);
            auto batch = fixture.indirect->prepareDirect(*command, draws.values, &fixture.error);
            REQUIRE(batch);
            auto encoder = fixture.encoder(*command, target, false);
            CHECK_FALSE(fixture.indirect->execute(*encoder, *batch, 0, 3, &fixture.error));
            REQUIRE(fixture.indirect->executeDirect(*encoder, *batch, 0, 3, &fixture.error));
            REQUIRE(encoder->end(&fixture.error));
            CHECK_FALSE(fixture.indirect->executeDirect(*encoder, *batch, 0, 1, &fixture.error));
        };
        prepare_view(*target_a, 400);
        prepare_view(*target_b, 501);
        CHECK(fixture.indirect->stats().direct_slot_count == 2);
        CHECK(fixture.indirect->stats().direct_slot_reuses == 0);
        REQUIRE(command->commitAndWait(&fixture.error));
        CHECK(std::abs(fixture.pixels(*target_a)[0] - (403.f / 1024.f + 2.25f / 8.f)) < 1e-6f);
        CHECK(std::abs(fixture.pixels(*target_b)[0] - (504.f / 1024.f - 1.25f / 8.f)) < 1e-6f);

        auto next_command = fixture.device->makeCommandBuffer("Native static packet reuse", &fixture.error);
        auto foreign_command = fixture.device->makeCommandBuffer("Native static foreign command", &fixture.error);
        REQUIRE(next_command);
        REQUIRE(foreign_command);
        Draws next(fixture, 1, 600);
        auto bad = next.values[0];
        bad.indices = nullptr;
        const std::array invalid_list{next.values[0], bad};
        const auto before_invalid = fixture.indirect->stats();
        CHECK_FALSE(fixture.indirect->prepareDirect(*next_command, invalid_list, &fixture.error));
        CHECK(fixture.indirect->stats().direct_prepared_draws == before_invalid.direct_prepared_draws);
        CHECK(fixture.indirect->stats().direct_slot_reuses == before_invalid.direct_slot_reuses);
        std::vector<ALMetalShadowIndirectDraw> too_many(
            ALMetalShadowIndirectCommands::maximum_draw_count + 1, next.values[0]);
        CHECK_FALSE(fixture.indirect->prepareDirect(*next_command, too_many, &fixture.error));
        auto batch = fixture.indirect->prepareDirect(*next_command, next.values, &fixture.error);
        auto indirect = fixture.indirect->prepare(*next_command, next.values, &fixture.error);
        auto empty = fixture.indirect->prepareDirect(*next_command, {}, &fixture.error);
        REQUIRE(batch);
        REQUIRE(indirect);
        REQUIRE(empty);
        CHECK(fixture.indirect->stats().direct_slot_count == 2);
        CHECK(fixture.indirect->stats().direct_slot_reuses == 1);
        auto foreign_encoder = fixture.encoder(*foreign_command, *target_a, false);
        CHECK_FALSE(fixture.indirect->executeDirect(*foreign_encoder, *batch, 0, 1, &fixture.error));
        REQUIRE(foreign_encoder->end(&fixture.error));
        auto encoder = fixture.encoder(*next_command, *target_a, false);
        CHECK_FALSE(fixture.indirect->executeDirect(*encoder, *indirect, 0, 1, &fixture.error));
        CHECK_FALSE(fixture.indirect->executeDirect(*encoder, *batch, 1, 1, &fixture.error));
        CHECK_FALSE(fixture.indirect->executeDirect(*encoder, *batch, 0,
            std::numeric_limits<std::size_t>::max(), &fixture.error));
        REQUIRE(fixture.indirect->executeDirect(*encoder, *empty, 0, 0, &fixture.error));
        REQUIRE(fixture.indirect->executeDirect(*encoder, *batch, 0, 1, &fixture.error));
        REQUIRE(encoder->end(&fixture.error));
        batch.reset();
        indirect.reset();
        empty.reset();
        fixture.indirect.reset(); // The prepared command keeps packet owners alive.
        next.positions.reset();
        next.constants.reset();
        next.indices16.reset();
        next.indices32.reset();
        REQUIRE(next_command->commitAndWait(&fixture.error));
        CHECK(std::abs(fixture.pixels(*target_a)[0] - (601.f / 1024.f + .25f / 8.f)) < 1e-6f);
    }
}
