/**
 * @file almetal_resource_plan_native_test.mm
 * @brief Actual native backing, command, pool-generation and readback contracts.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetaldevice.h"
#include "../almetalpipeline.h"
#include "../almetalresource.h"
#include "../almetalshadowindirect.h"
#include <catch2/catch_test_macros.hpp>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace
{
namespace RP = ALMetalResourcePlanning;
constexpr std::uint32_t side = 8;
void validation(MTLShaderValidation actual, const char* name)
{
    std::cout << "resource_plan_shader_validation name=" << name << " value=" << static_cast<unsigned>(actual) << '\n';
    if (const char* requested = std::getenv("MTL_SHADER_VALIDATION"))
    {
        if (std::string(requested) == "1") CHECK(actual == MTLShaderValidationEnabled);
        if (std::string(requested) == "0") CHECK(actual == MTLShaderValidationDisabled);
    }
}
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    Fixture() { device = ALMetalDevice::create(&error, ALMetalResourcePlanMode::enabled); REQUIRE(device); }
    ALMetalTextureDescriptor textureDescriptor(ALMetalStorageMode storage) const
    {
        ALMetalTextureDescriptor d; d.width = side; d.height = side;
        d.format = ALMetalPixelFormat::rgba8_unorm; d.storage = storage;
        d.usage = ALMetalTextureUsage::shader_read | ALMetalTextureUsage::shader_write | ALMetalTextureUsage::render_target;
        return d;
    }
    ALMetalRenderTargetDescriptor targetDescriptor() const
    {
        ALMetalRenderTargetDescriptor d; d.width = side; d.height = side;
        d.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        return d;
    }
    void clear(ALMetalRenderTarget& target, ALMetalCommandBuffer& command, ALMetalClearColor color)
    {
        CHECK((__bridge id<MTLCommandBuffer>)command.nativeIdentity());
        CHECK([(__bridge id<MTLCommandBuffer>)command.nativeIdentity() retainedReferences]);
        ALMetalRenderPassConfig config; config.colors[0].load = ALMetalLoadAction::clear;
        config.colors[0].store = ALMetalStoreAction::store; config.colors[0].clear = color;
        auto pass = target.makeRenderPass(config, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(command, *pass, "Resource-plan actual clear", &error);
        REQUIRE(encoder); REQUIRE(encoder->end(&error));
    }
};
struct Gate
{
    id<MTLSharedEvent> event = nil;
    explicit Gate(const ALMetalDevice& device)
    {
        event = [(__bridge id<MTLDevice>)device.nativeHandle() newSharedEvent]; REQUIRE(event);
    }
    ~Gate() { release(); }
    void release() { if (event.signaledValue < 1) event.signaledValue = 1; }
    void encode(ALMetalCommandBuffer& command)
    { [(__bridge id<MTLCommandBuffer>)command.nativeHandle() encodeWaitForEvent:event value:1]; }
};
template<class Predicate> bool eventually(Predicate predicate)
{
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!predicate())
    {
        if (std::chrono::steady_clock::now() >= end) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}
void magenta(const std::array<std::uint8_t, side * side * 4>& pixels)
{
    for (std::size_t i = 0; i < pixels.size(); i += 4)
    {
        CHECK(pixels[i] == 255); CHECK(pixels[i + 1] == 0);
        CHECK(pixels[i + 2] == 255); CHECK(pixels[i + 3] == 255);
    }
}
}

TEST_CASE("Native resource plan shares backing identity across shader views and independent wrappers",
          "[metal-resource-plan-native]")
{
    @autoreleasepool
    {
        Fixture f;
        auto d = f.textureDescriptor(ALMetalStorageMode::shared);
        d.format = ALMetalPixelFormat::rgba8_unorm_srgb;
        d.usage = ALMetalTextureUsage::shader_read | ALMetalTextureUsage::render_target;
        auto texture = ALMetalTexture::create(*f.device, d, &f.error); REQUIRE(texture);
        auto alias = ALMetalTexture::retainSharedBacking(*texture, &f.error); REQUIRE(alias);
        REQUIRE(texture->nativeLinearHandle()); REQUIRE(alias->nativeLinearHandle());
        CHECK(texture->resourceView().identity() == alias->resourceView().identity());
        CHECK(texture->resourceView().generation() == alias->resourceView().generation());
        CHECK(f.device->uniqueResourceStats().owned_backings == 1);
        CHECK(f.device->uniqueResourceStats().owned_declared_bytes == side * side * 4);
        ALMetalRenderPassColorConfig color;
        auto linear_pass = ALMetalRenderPass::createForColorAttachment(*f.device, texture->nativeLinearHandle(),
            ALMetalPixelFormat::rgba8_unorm, color, texture->resourceView(), &f.error); REQUIRE(linear_pass);
        CHECK(f.device->uniqueResourceStats().owned_backings == 1);
        CHECK(f.device->uniqueResourceStats().external_unknown_records == 0);
        linear_pass.reset();
        CHECK_FALSE(texture->resourceView().canIssueGeneration());
        texture.reset(); CHECK(f.device->uniqueResourceStats().owned_backings == 1);
        alias.reset(); CHECK(f.device->uniqueResourceStats().owned_backings == 0);
    }
}

TEST_CASE("Native pool waits for real commit ordering while retaining an in-flight old generation",
          "[metal-resource-plan-native]")
{
    @autoreleasepool
    {
        Fixture f; Gate gate(*f.device);
        auto pool = ALMetalRenderTargetPool::create(*f.device, 1 << 20); REQUIRE(pool);
        auto first = pool->acquire(f.targetDescriptor(), &f.error); REQUIRE(first);
        const auto backing = first->colorTexture(0)->nativeHandle();
        const auto identity = first->colorTexture(0)->resourceView().identity();
        const auto generation = first->colorTexture(0)->resourceView().generation();
        auto producer = f.device->makeCommandBuffer("Resource-plan gated old generation", &f.error); REQUIRE(producer);
        f.clear(*first, *producer, {0, 0, 0, 1});
        first.reset(); // Pool wrapper goes idle while its native use is uncommitted.
        auto other = pool->acquire(f.targetDescriptor(), &f.error); REQUIRE(other);
        CHECK(other->colorTexture(0)->nativeHandle() != backing);
        CHECK(pool->stats().dependency_deferrals >= 1);
        gate.encode(*producer); // Add the event only after isolating uncommitted-use refusal.
        REQUIRE(producer->commit(&f.error));
        CHECK(producer->status() != ALMetalCommandStatus::completed); // The event is still closed.
        auto reused = pool->acquire(f.targetDescriptor(), &f.error); REQUIRE(reused);
        CHECK(reused->colorTexture(0)->nativeHandle() == backing);
        CHECK(reused->colorTexture(0)->resourceView().identity() == identity);
        CHECK(reused->colorTexture(0)->resourceView().generation() == generation + 1);
        CHECK(f.device->uniqueResourceStats().submitted_uses >= 1);
        auto consumer = f.device->makeCommandBuffer("Resource-plan queued new generation", &f.error); REQUIRE(consumer);
        f.clear(*reused, *consumer, {1, 0, 1, 1});
        REQUIRE(consumer->commit(&f.error));
        gate.release(); REQUIRE(consumer->waitUntilCompleted(&f.error));
        std::array<std::uint8_t, side * side * 4> pixels{};
        REQUIRE(reused->colorTexture(0)->readback(*f.device, 0, 0, {}, pixels.data(), side * 4, &f.error));
        magenta(pixels);
        CHECK(eventually([&] { return f.device->uniqueResourceStats().submitted_uses == 0; }));
    }
}

TEST_CASE("Native aliases prevent pool recycling independently of completed command uses",
          "[metal-resource-plan-native]")
{
    @autoreleasepool
    {
        Fixture f;
        auto pool = ALMetalRenderTargetPool::create(*f.device, 1 << 20); REQUIRE(pool);
        auto target = pool->acquire(f.targetDescriptor(), &f.error); REQUIRE(target);
        auto backing = target->colorTexture(0)->nativeHandle();
        auto alias = ALMetalTexture::retainSharedBacking(*target->colorTexture(0), &f.error); REQUIRE(alias);
        target.reset();
        auto other = pool->acquire(f.targetDescriptor(), &f.error); REQUIRE(other);
        CHECK(other->colorTexture(0)->nativeHandle() != backing);
        alias.reset();
        auto reused = pool->acquire(f.targetDescriptor(), &f.error); REQUIRE(reused);
        CHECK(reused->colorTexture(0)->nativeHandle() == backing);
    }
}

TEST_CASE("Native command backing leases retire at last completion or abandonment after wrapper destruction",
          "[metal-resource-plan-native]")
{
    @autoreleasepool
    {
        Fixture f;
        auto target = ALMetalRenderTarget::create(*f.device, f.targetDescriptor(), &f.error); REQUIRE(target);
        auto abandoned = f.device->makeCommandBuffer("Resource-plan abandoned", &f.error); REQUIRE(abandoned);
        f.clear(*target, *abandoned, {1, 0, 1, 1});
        target.reset(); CHECK(f.device->uniqueResourceStats().owned_backings == 1);
        abandoned.reset(); CHECK(f.device->uniqueResourceStats().owned_backings == 0);
        Gate gate(*f.device);
        target = ALMetalRenderTarget::create(*f.device, f.targetDescriptor(), &f.error); REQUIRE(target);
        auto committed = f.device->makeCommandBuffer("Resource-plan last native reader", &f.error); REQUIRE(committed);
        gate.encode(*committed); f.clear(*target, *committed, {1, 0, 1, 1});
        REQUIRE(committed->commit(&f.error)); target.reset(); committed.reset();
        CHECK(f.device->uniqueResourceStats().owned_backings == 1);
        gate.release(); REQUIRE(f.device->waitUntilIdle(&f.error));
        CHECK(eventually([&] { return f.device->uniqueResourceStats().owned_backings == 0; }));
    }
}

TEST_CASE("Native shared texture readback refuses unqueued writers and settles only committed same-queue work",
          "[metal-resource-plan-native][PPD-417]")
{
    @autoreleasepool
    {
        Fixture f; Gate gate(*f.device);
        auto texture = ALMetalTexture::create(*f.device, f.textureDescriptor(ALMetalStorageMode::shared), &f.error); REQUIRE(texture);
        auto d = f.targetDescriptor(); d.colors[0].texture = texture.get();
        auto target = ALMetalRenderTarget::create(*f.device, d, &f.error); REQUIRE(target);
        auto unqueued = f.device->makeCommandBuffer("Resource-plan isolated unqueued writer", &f.error); REQUIRE(unqueued);
        f.clear(*target, *unqueued, {1, 0, 1, 1}); // No opaque native/event escape here.
        std::array<std::uint8_t, side * side * 4> pixels{}; pixels.fill(0x6d);
        const auto before = ALMetalDevice::committedCommandBufferCount();
        CHECK_FALSE(texture->readback(*f.device, 0, 0, {}, pixels.data(), side * 4, &f.error));
        CHECK(ALMetalDevice::committedCommandBufferCount() == before);
        CHECK(pixels.front() == 0x6d);
        unqueued.reset(); // Retire the negative control without submitting its writes.
        f.error.clear();
        auto producer = f.device->makeCommandBuffer("Resource-plan gated shared texture writer", &f.error); REQUIRE(producer);
        gate.encode(*producer); f.clear(*target, *producer, {1, 0, 1, 1});
        REQUIRE(producer->commit(&f.error));
        const auto after_producer = ALMetalDevice::committedCommandBufferCount();
        std::atomic<bool> finished{false}; bool read_ok = false;
        std::thread reader([&] {
            read_ok = texture->readback(*f.device, 0, 0, {}, pixels.data(), side * 4, &f.error);
            finished.store(true, std::memory_order_release);
        });
        // No fatal assertion while reader owns a gated wait: always release
        // the event and join, including failed sequencing controls.
        CHECK(eventually([&] { return ALMetalDevice::committedCommandBufferCount() > after_producer || finished.load(std::memory_order_acquire); }));
        CHECK_FALSE(finished.load(std::memory_order_acquire));
        gate.release(); reader.join();
        INFO(f.error); REQUIRE(read_ok); magenta(pixels);
    }
}

TEST_CASE("Native compute blit staging and readback retain exact backing uses",
          "[metal-resource-plan-native]")
{
    @autoreleasepool
    {
        Fixture f;
        const std::string source = R"MSL(
#include <metal_stdlib>
using namespace metal;
kernel void resource_plan_fill(device uint* value [[buffer(0)]],
    texture2d<float,access::write> output [[texture(0)]], uint2 p [[thread_position_in_grid]])
{
    if (p.x >= output.get_width() || p.y >= output.get_height()) return;
    output.write(float4(1,0,1,1), p);
    if (p.x == 0 && p.y == 0) value[0] = 0x1234abcdu;
}
)MSL";
        auto library = ALMetalShaderLibrary::compileSource(*f.device, source, "Resource-plan native fixture", &f.error); REQUIRE(library);
        ALMetalComputePipelineDescriptor pd; pd.library = library.get(); pd.function = "resource_plan_fill";
        auto pipeline = ALMetalComputePipeline::create(*f.device, pd, &f.error); REQUIRE(pipeline);
        validation([(__bridge id<MTLComputePipelineState>)pipeline->nativeHandle() shaderValidation], "compute_fill");
        ALMetalBufferDescriptor bd; bd.length = 64; bd.storage = ALMetalStorageMode::private_gpu;
        auto buffer = ALMetalBuffer::create(*f.device, bd, &f.error); REQUIRE(buffer);
        auto texture = ALMetalTexture::create(*f.device, f.textureDescriptor(ALMetalStorageMode::private_gpu), &f.error); REQUIRE(texture);
        auto command = f.device->makeCommandBuffer("Resource-plan compute and transfers", &f.error); REQUIRE(command);
        REQUIRE(buffer->fillWords(*command, 0, bd.length, 0x875421ab, &f.error));
        auto encoder = ALMetalComputeEncoder::begin(*command, "Resource-plan native compute", &f.error); REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline, &f.error)); REQUIRE(encoder->setBuffer(0, *buffer, 0, &f.error));
        REQUIRE(encoder->setTexture(0, texture.get(), &f.error));
        REQUIRE(encoder->dispatch(side, side, 1, side, side, 1, &f.error)); REQUIRE(encoder->end(&f.error));
        CHECK(command->resourceUsePlan()->stats().resources >= 3); // Includes the fill staging backing.
        REQUIRE(command->commitAndWait(&f.error));
        std::uint32_t value = 0; REQUIRE(buffer->readback(*f.device, 0, &value, sizeof(value), &f.error));
        CHECK(value == 0x1234abcdu);
        std::array<std::uint8_t, side * side * 4> pixels{};
        REQUIRE(texture->readback(*f.device, 0, 0, {}, pixels.data(), side * 4, &f.error)); magenta(pixels);
        const auto stats = f.device->uniqueResourceStats();
        CHECK(stats.backing_creations >= 5); CHECK(stats.errored_commands == 0);
        CHECK(eventually([&] { return f.device->uniqueResourceStats().uncommitted_uses == 0 && f.device->uniqueResourceStats().submitted_uses == 0; }));
    }
}

TEST_CASE("Native pool reuses ended passes only with the exact uncommitted command proof",
          "[metal-resource-plan-native]")
{
    @autoreleasepool
    {
        Fixture f;
        auto pool = ALMetalRenderTargetPool::create(*f.device, 1 << 20); REQUIRE(pool);
        auto command = f.device->makeCommandBuffer("Resource-plan same-command intermediates", &f.error); REQUIRE(command);
        auto target = pool->acquire(f.targetDescriptor(), &f.error); REQUIRE(target);
        const auto backing = target->colorTexture(0)->nativeHandle();
        const auto generation = target->colorTexture(0)->resourceView().generation();
        f.clear(*target, *command, {0, 0, 0, 1}); target.reset();
        auto reused = pool->acquire(f.targetDescriptor(), &f.error, command->resourceUsePlan()); REQUIRE(reused);
        CHECK(reused->colorTexture(0)->nativeHandle() == backing);
        CHECK(reused->colorTexture(0)->resourceView().generation() == generation + 1);
        f.clear(*reused, *command, {1, 0, 1, 1});
        REQUIRE(command->commitAndWait(&f.error));
        std::array<std::uint8_t, side * side * 4> pixels{};
        REQUIRE(reused->colorTexture(0)->readback(*f.device, 0, 0, {}, pixels.data(), side * 4, &f.error));
        magenta(pixels);
    }
}

TEST_CASE("Native supplied color backing preserves imported identity and rejects mismatched tokens",
          "[metal-resource-plan-native][PPD-423]")
{
    @autoreleasepool
    {
        Fixture f;
        auto a = ALMetalTexture::create(*f.device, f.textureDescriptor(ALMetalStorageMode::shared), &f.error); REQUIRE(a);
        auto b = ALMetalTexture::create(*f.device, f.textureDescriptor(ALMetalStorageMode::shared), &f.error); REQUIRE(b);
        ALMetalRenderPassColorConfig color; color.load = ALMetalLoadAction::clear;
        color.clear = {1, 0, 1, 1}; color.store = ALMetalStoreAction::store;
        const auto before = f.device->uniqueResourceStats();
        CHECK_FALSE(ALMetalRenderPass::createForColorAttachment(*f.device, a->nativeHandle(),
            ALMetalPixelFormat::rgba8_unorm, color, b->resourceView(), &f.error));
        f.error.clear();
        auto pass = ALMetalRenderPass::createForColorAttachment(*f.device, a->nativeHandle(),
            ALMetalPixelFormat::rgba8_unorm, color, a->resourceView(), &f.error); REQUIRE(pass);
        CHECK(f.device->uniqueResourceStats().owned_backings == before.owned_backings);
        CHECK(f.device->uniqueResourceStats().external_unknown_records == 0);
        auto command = f.device->makeCommandBuffer("Resource-plan supplied owned attachment", &f.error); REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Supplied backing clear", &f.error); REQUIRE(encoder);
        REQUIRE(encoder->end(&f.error)); CHECK_FALSE(command->resourceUsePlan()->stats().opaque);
        REQUIRE(command->commitAndWait(&f.error));
        std::array<std::uint8_t, side * side * 4> pixels{};
        REQUIRE(a->readback(*f.device, 0, 0, {}, pixels.data(), side * 4, &f.error)); magenta(pixels);

        // Standalone foreign texture models an imported drawable BACKING,
        // without claiming a window/drawable presentation test.
        auto descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
            width:side height:side mipmapped:NO];
        descriptor.storageMode = MTLStorageModeShared;
        descriptor.usage = MTLTextureUsageRenderTarget;
        auto foreign = [(__bridge id<MTLDevice>)f.device->nativeHandle() newTextureWithDescriptor:descriptor]; REQUIRE(foreign);
        RP::Description allocation; allocation.kind = RP::Kind::texture; allocation.origin = RP::Origin::external_unknown;
        RP::View imported;
        REQUIRE(f.device->registerNativeBacking((__bridge void*)foreign, allocation, imported, &f.error));
        const auto imported_id = imported.identity();
        auto first = ALMetalRenderPass::createForColorAttachment(*f.device, (__bridge void*)foreign,
            ALMetalPixelFormat::rgba8_unorm, color, imported, &f.error); REQUIRE(first);
        auto second = ALMetalRenderPass::createForColorAttachment(*f.device, (__bridge void*)foreign,
            ALMetalPixelFormat::rgba8_unorm, color, imported, &f.error); REQUIRE(second);
        CHECK(imported.identity() == imported_id); CHECK(f.device->uniqueResourceStats().external_unknown_records == 1);
        auto imported_command = f.device->makeCommandBuffer("Resource-plan same imported backing passes", &f.error); REQUIRE(imported_command);
        auto imported_encoder = ALMetalRenderEncoder::begin(*imported_command, *first, "Imported first", &f.error); REQUIRE(imported_encoder);
        REQUIRE(imported_encoder->end(&f.error));
        imported_encoder = ALMetalRenderEncoder::begin(*imported_command, *second, "Imported second", &f.error); REQUIRE(imported_encoder);
        REQUIRE(imported_encoder->end(&f.error));
        CHECK(imported_command->resourceUsePlan()->stats().resources == 1);
        CHECK_FALSE(imported_command->resourceUsePlan()->stats().opaque);
        REQUIRE(imported_command->commitAndWait(&f.error));
    }
}

TEST_CASE("Native declared encoder bridge flushes earlier joined indexed draws before raw state changes",
          "[metal-resource-plan-native][PPD-424]")
{
    @autoreleasepool
    {
        Fixture f;
        const std::string source = R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 resource_plan_vertex(uint v [[vertex_id]])
{
    const float2 positions[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
    return float4(positions[v],0,1);
}
fragment float4 resource_plan_color(constant float4& color [[buffer(0)]]) { return color; }
)MSL";
        auto library = ALMetalShaderLibrary::compileSource(*f.device, source, "Resource-plan ordered native bridge", &f.error); REQUIRE(library);
        ALMetalRenderPipelineDescriptor pd; pd.library = library.get();
        pd.vertex_function = "resource_plan_vertex"; pd.fragment_function = "resource_plan_color";
        pd.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        auto pipeline = ALMetalRenderPipeline::create(*f.device, pd, &f.error); REQUIRE(pipeline);
        validation([(__bridge id<MTLRenderPipelineState>)pipeline->nativeHandle() shaderValidation], "joined_native_order");
        ALMetalBufferDescriptor bd; bd.length = 3 * sizeof(std::uint16_t); bd.storage = ALMetalStorageMode::shared;
        auto indices = ALMetalBuffer::create(*f.device, bd, &f.error); REQUIRE(indices);
        const std::array<std::uint16_t, 3> values{0,1,2};
        std::memcpy(indices->mappedBytes(), values.data(), sizeof(values));
        auto target = ALMetalRenderTarget::create(*f.device, f.targetDescriptor(), &f.error); REQUIRE(target);
        ALMetalRenderPassConfig config; config.colors[0].load = ALMetalLoadAction::clear;
        config.colors[0].clear = {0,0,0,1}; config.colors[0].store = ALMetalStoreAction::store;
        auto pass = target->makeRenderPass(config, &f.error); REQUIRE(pass);
        auto command = f.device->makeCommandBuffer("Resource-plan indexed-before-native order", &f.error); REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Ordered indexed/native", &f.error); REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline, &f.error));
        REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &f.error));
        REQUIRE(encoder->setViewport({0,0,side,side,0,1}, &f.error));
        const std::array<float,4> red{1,0,0,1}, blue{0,0,1,1};
        REQUIRE(encoder->setFragmentBytes(0, red.data(), sizeof(red), &f.error));
        REQUIRE(encoder->setIndexedTriangleJoining(true, &f.error));
        REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle, 3, ALMetalIndexType::uint16,
            *indices, 0, 1, &f.error));
        auto native = (__bridge id<MTLRenderCommandEncoder>)encoder->nativeHandleForDeclaredResources(); REQUIRE(native);
        [native setScissorRect:MTLScissorRect{0,0,side/2,side}];
        [native setFragmentBytes:blue.data() length:sizeof(blue) atIndex:0];
        [native drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        REQUIRE(encoder->end(&f.error)); CHECK_FALSE(command->resourceUsePlan()->stats().opaque);
        REQUIRE(command->commitAndWait(&f.error));
        std::array<std::uint8_t, side * side * 4> pixels{};
        REQUIRE(target->colorTexture(0)->readback(*f.device, 0, 0, {}, pixels.data(), side * 4, &f.error));
        for (std::size_t y=0; y<side; ++y) for (std::size_t x=0; x<side; ++x)
        {
            const auto i=(y*side+x)*4;
            CHECK(pixels[i] == (x<side/2 ? 0 : 255)); CHECK(pixels[i+1] == 0);
            CHECK(pixels[i+2] == (x<side/2 ? 255 : 0)); CHECK(pixels[i+3] == 255);
        }
        auto opaque_command = f.device->makeCommandBuffer("Resource-plan explicit opaque fallback", &f.error); REQUIRE(opaque_command);
        auto opaque_encoder = ALMetalRenderEncoder::begin(*opaque_command, *pass, "Opaque render fallback", &f.error); REQUIRE(opaque_encoder);
        REQUIRE(opaque_encoder->nativeHandleForOpaqueResources());
        CHECK(opaque_command->resourceUsePlan()->stats().opaque);
        REQUIRE(opaque_encoder->end(&f.error)); CHECK_FALSE(opaque_encoder->nativeHandleForOpaqueResources());
        auto opaque_compute = ALMetalComputeEncoder::begin(*opaque_command, "Opaque compute fallback", &f.error); REQUIRE(opaque_compute);
        REQUIRE(opaque_compute->nativeHandleForOpaqueResources());
        REQUIRE(opaque_compute->end(&f.error)); CHECK_FALSE(opaque_compute->nativeHandleForOpaqueResources());
        REQUIRE(opaque_command->commitAndWait(&f.error));
    }
}

TEST_CASE("Native shadow direct ICB and same-queue visibility preserve exact typed resource closure",
          "[metal-resource-plan-native]")
{
    @autoreleasepool
    {
        Fixture f;
        const std::string source = R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 resource_plan_shadow_vertex(device const float4* positions [[buffer(0)]], uint v [[vertex_id]])
{ return positions[v]; }
fragment float4 resource_plan_shadow_fragment() { return float4(1,0,1,1); }
)MSL";
        auto library = ALMetalShaderLibrary::compileSource(*f.device, source, "Resource-plan shadow closure", &f.error); REQUIRE(library);
        ALMetalRenderPipelineDescriptor pd; pd.library = library.get();
        pd.vertex_function = "resource_plan_shadow_vertex"; pd.fragment_function = "resource_plan_shadow_fragment";
        pd.colors[0].format = ALMetalPixelFormat::rgba8_unorm; pd.support_indirect_commands = true;
        auto pipeline = ALMetalRenderPipeline::create(*f.device, pd, &f.error); REQUIRE(pipeline);
        validation([(__bridge id<MTLRenderPipelineState>)pipeline->nativeHandle() shaderValidation], "shadow_closure");
        auto shadow = ALMetalShadowIndirectCommands::create(*f.device, &f.error); REQUIRE(shadow);
        REQUIRE(shadow->stats().indirect_supported);
        ALMetalBufferDescriptor bd; bd.storage = ALMetalStorageMode::shared; bd.length = 3 * 16;
        auto positions = ALMetalBuffer::create(*f.device, bd, &f.error); REQUIRE(positions);
        const std::array<float,12> points{-1,-1,0,1, 3,-1,0,1, -1,3,0,1};
        std::memcpy(positions->mappedBytes(), points.data(), sizeof(points));
        bd.length = 80; auto constants = ALMetalBuffer::create(*f.device, bd, &f.error); REQUIRE(constants);
        bd.length = 3 * sizeof(std::uint16_t); auto indices = ALMetalBuffer::create(*f.device, bd, &f.error); REQUIRE(indices);
        const std::array<std::uint16_t,3> values{0,1,2};
        std::memcpy(indices->mappedBytes(), values.data(), sizeof(values));
        ALMetalShadowIndirectDraw draw; draw.positions = positions.get(); draw.constants = constants.get();
        draw.indices = indices.get(); draw.vertex_count = 3; draw.index_count = 3;
        const std::array draws{draw};
        for (unsigned mode = 0; mode != 3; ++mode)
        {
            INFO("shadow closure mode=" << mode);
            auto command = f.device->makeCommandBuffer("Resource-plan shadow direct/indirect closure", &f.error); REQUIRE(command);
            auto batch = mode == 0 ? shadow->prepareDirect(*command, draws, &f.error)
                                  : shadow->prepare(*command, draws, &f.error, mode == 2);
            REQUIRE(batch);
            if (mode == 2)
            {
                ALMetalShadowVisibilityInput input;
                input.matrix[0]=input.matrix[5]=input.matrix[10]=input.matrix[15]=1;
                input.minimum[0]=input.minimum[1]=-1; input.maximum[0]=input.maximum[1]=3;
                input.maximum[2]=input.minimum[2]=0; input.config[0]=1;
                const std::array inputs{input}; const std::array<ALMetalShadowVisibilityRun,1> runs{{{0,1}}};
                REQUIRE(shadow->prepareVisibility(*command, *batch, inputs, runs, false, &f.error));
            }
            auto target = ALMetalRenderTarget::create(*f.device, f.targetDescriptor(), &f.error); REQUIRE(target);
            ALMetalRenderPassConfig config; config.colors[0].load = ALMetalLoadAction::clear;
            config.colors[0].clear = {0,0,0,1}; config.colors[0].store = ALMetalStoreAction::store;
            auto pass = target->makeRenderPass(config, &f.error); REQUIRE(pass);
            auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Resource-plan shadow closure output", &f.error); REQUIRE(encoder);
            REQUIRE(encoder->setPipeline(*pipeline, &f.error));
            REQUIRE(encoder->setCullMode(ALMetalCullMode::none, &f.error));
            REQUIRE(encoder->setViewport({0,0,side,side,0,1}, &f.error));
            if (mode == 0) REQUIRE(shadow->executeDirect(*encoder, *batch, 0, 1, &f.error));
            else REQUIRE(shadow->execute(*encoder, *batch, 0, 1, &f.error));
            REQUIRE(encoder->end(&f.error));
            CHECK_FALSE(command->resourceUsePlan()->stats().opaque);
            CHECK(command->resourceUsePlan()->stats().resources >= 4);
            if (mode == 2) CHECK(command->resourceUsePlan()->stats().passes == 3);
            REQUIRE(command->commitAndWait(&f.error));
            std::array<std::uint8_t, side * side * 4> pixels{};
            REQUIRE(target->colorTexture(0)->readback(*f.device, 0, 0, {}, pixels.data(), side * 4, &f.error));
            magenta(pixels);
        }
        CHECK(shadow->stats().direct_executions == 1); CHECK(shadow->stats().executions == 2);
        CHECK(shadow->stats().visibility_dispatches == 1);
        CHECK(eventually([&] { const auto s=f.device->uniqueResourceStats(); return s.uncommitted_uses==0 && s.submitted_uses==0; }));
    }
}
