/**
 * @file almetal_resource_plan_native_test.mm
 * @brief Actual native backing, command, pool-generation and readback contracts.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetaldevice.h"
#include "../almetalpipeline.h"
#include "../almetalpipelinecache.h"
#include "../almetalresource.h"
#include "../almetalshadowindirect.h"
#include <catch2/catch_test_macros.hpp>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <time.h>
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

TEST_CASE("Native execution factories share one same-mode owner concurrently",
          "[metal-resource-plan-native][PPD-459]")
{
    @autoreleasepool
    {
        constexpr unsigned count = 8;
        std::array<std::unique_ptr<ALMetalDevice>, count> devices;
        std::array<std::string, count> errors;
        std::array<std::thread, count> workers;
        std::barrier<> start(count);
        for (unsigned i = 0; i < count; ++i)
            workers[i] = std::thread([&, i]
            {
                @autoreleasepool
                {
                    start.arrive_and_wait();
                    devices[i] = ALMetalDevice::create(&errors[i], ALMetalResourcePlanMode::accounting_only);
                }
            });
        for (auto& worker : workers) worker.join();
        for (unsigned i = 0; i < count; ++i)
        {
            INFO(errors[i]); REQUIRE(devices[i]);
            CHECK(devices[i]->nativeHandle() == devices[0]->nativeHandle());
            CHECK(devices[i]->nativeCommandQueue() == devices[0]->nativeCommandQueue());
            CHECK(devices[i]->resourcePlan().get() == devices[0]->resourcePlan().get());
        }
        auto command = devices[0]->makeCommandBuffer("Accounting session remains untracked", &errors[0]);
        REQUIRE(command); CHECK_FALSE(command->resourceUsePlan());
    }
}

TEST_CASE("Native same-mode wrappers share allocation accounting and ordered pixels",
          "[metal-resource-plan-native][PPD-459]")
{
    @autoreleasepool
    {
        Fixture f;
        auto other = ALMetalDevice::create(&f.error, ALMetalResourcePlanMode::enabled); REQUIRE(other);
        CHECK(other->nativeHandle() == f.device->nativeHandle());
        CHECK(other->nativeCommandQueue() == f.device->nativeCommandQueue());
        CHECK(other->resourcePlan().get() == f.device->resourcePlan().get());
        const auto before = f.device->uniqueResourceStats();
        auto texture = ALMetalTexture::create(*other, f.textureDescriptor(ALMetalStorageMode::private_gpu), &f.error);
        REQUIRE(texture);
        auto alias = ALMetalTexture::retainSharedBacking(*texture, &f.error); REQUIRE(alias);
        CHECK(f.device->uniqueResourceStats().owned_backings == before.owned_backings + 1);
        CHECK(other->uniqueResourceStats().owned_backings == before.owned_backings + 1);
        CHECK(other->uniqueResourceStats().owned_declared_bytes == before.owned_declared_bytes + side * side * 4);
        auto descriptor = f.targetDescriptor(); descriptor.colors[0].texture = texture.get();
        auto target = ALMetalRenderTarget::create(*f.device, descriptor, &f.error); REQUIRE(target);
        auto command = f.device->makeCommandBuffer("Cross-wrapper same-session clear", &f.error); REQUIRE(command);
        f.clear(*target, *command, {1, 0, 1, 1});
        REQUIRE(command->commitAndWait(&f.error));
        std::array<std::uint8_t, side * side * 4> pixels{};
        REQUIRE(alias->readback(*other, 0, 0, {}, pixels.data(), side * 4, &f.error));
        magenta(pixels);
        target.reset(); texture.reset();
        CHECK(other->uniqueResourceStats().owned_backings == before.owned_backings + 1);
        alias.reset();
        CHECK(other->uniqueResourceStats().owned_backings == before.owned_backings);
        CHECK(f.device->uniqueResourceStats().owned_declared_bytes == before.owned_declared_bytes);
    }
}

TEST_CASE("Native execution owner survives facades while an old GPU command retains backing",
          "[metal-resource-plan-native][PPD-459]")
{
    @autoreleasepool
    {
        std::string error;
        std::weak_ptr<RP::Plan> registry;
        std::shared_ptr<const ALMetalCommandCompletion> completion;
        std::unique_ptr<Gate> gate;
        __weak id<MTLBuffer> backing = nil;
        const void* queue = nullptr;
        RP::Stats before;
        {
            @autoreleasepool
            {
                auto device = ALMetalDevice::create(&error, ALMetalResourcePlanMode::enabled); REQUIRE(device);
                before = device->uniqueResourceStats();
                registry = device->resourcePlan(); queue = device->nativeCommandQueue();
                gate = std::make_unique<Gate>(*device);
                ALMetalBufferDescriptor bd; bd.length = 64; bd.storage = ALMetalStorageMode::private_gpu;
                auto buffer = ALMetalBuffer::create(*device, bd, &error); REQUIRE(buffer);
                backing = (__bridge id<MTLBuffer>)buffer->nativeHandle();
                auto command = device->makeCommandBuffer("Session outlives device and command facades", &error); REQUIRE(command);
                gate->encode(*command);
                REQUIRE(buffer->fillWords(*command, 0, bd.length, 0xa5a5a5a5u, &error));
                completion = command->completionToken(); REQUIRE(completion);
                REQUIRE(command->commit(&error));
                buffer.reset(); command.reset(); device.reset();
            }
        }
        auto replacement = ALMetalDevice::create(&error, ALMetalResourcePlanMode::enabled); REQUIRE(replacement);
        CHECK(replacement->nativeCommandQueue() == queue);
        CHECK_FALSE(registry.expired());
        CHECK(backing != nil);
        CHECK(replacement->uniqueResourceStats().owned_backings == before.owned_backings + 1);
        CHECK(replacement->uniqueResourceStats().submitted_uses == before.submitted_uses + 1);
        const auto commits = ALMetalDevice::committedCommandBufferCount();
        gate->release();
        REQUIRE(completion->waitUntilCompleted(&error));
        CHECK(ALMetalDevice::committedCommandBufferCount() == commits);
        CHECK(eventually([&]
        {
            const auto now = replacement->uniqueResourceStats();
            return now.owned_backings == before.owned_backings && now.submitted_uses == before.submitted_uses;
        }));
        CHECK(replacement->uniqueResourceStats().backing_retirements == before.backing_retirements + 1);
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

namespace resource_plan_owner_cost
{
struct Stamp
{
    std::uint64_t wall = 0, cpu = 0;
};
Stamp stamp()
{
    timespec cpu{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpu) != 0)
        throw std::runtime_error("Resource owner thread CPU clock is unavailable");
    return {std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count()),
        std::uint64_t(cpu.tv_sec) * 1'000'000'000ull + std::uint64_t(cpu.tv_nsec)};
}
struct Row
{
    std::uint64_t encode_wall = 0, encode_cpu = 0, submit_wall = 0, submit_cpu = 0;
    std::uint64_t wait_wall = 0, wait_cpu = 0, observe_wall = 0, observe_cpu = 0, retire_wall = 0, retire_cpu = 0;
    std::uint64_t total_wall = 0, total_cpu = 0, resources = 0, passes = 0;
    std::uint64_t live_backings = 0, live_declared = 0, live_reported = 0;
    std::uint64_t native_commit_delta = 0, pool_reuses = 0, cached_targets = 0;
    double gpu_start = 0, gpu_end = 0;
};
struct Owner
{
    std::string error;
    ALMetalResourcePlanMode mode;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalRenderPipeline> render;
    std::unique_ptr<ALMetalComputePipeline> compute;
    std::unique_ptr<ALMetalBuffer> indices;
    std::vector<std::unique_ptr<ALMetalBuffer>> alternate_indices;
    std::shared_ptr<ALMetalRenderTargetPool> pool;
    static constexpr unsigned width = 128, height = 128;
    explicit Owner(ALMetalResourcePlanMode value,unsigned index_count=1) : mode(value)
    {
        device = ALMetalDevice::create(&error, mode); INFO(error); REQUIRE(device);
        REQUIRE(device->resourcePlanningMode() == mode);
        REQUIRE_FALSE(device->pipelineCache().stats().enabled);
        const std::string source = R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 resource_owner_vertex(uint v [[vertex_id]])
{
    const float2 p[3] = {float2(-1,-1),float2(3,-1),float2(-1,3)};
    return float4(p[v],0,1);
}
fragment float4 resource_owner_fragment(constant float4& color [[buffer(0)]]) { return color; }
kernel void resource_owner_compute(texture2d<float,access::read> input [[texture(0)]],
    texture2d<float,access::write> output [[texture(1)]], device const uint* pattern [[buffer(0)]],
    uint2 p [[thread_position_in_grid]])
{
    if (p.x >= output.get_width() || p.y >= output.get_height()) return;
    output.write(float4(input.read(p).r, float(pattern[0] != 0x875421abu), 1, 1), p);
}
)MSL";
        library = ALMetalShaderLibrary::compileSource(*device, source, "Resource owner cost exact authored flow", &error);
        INFO(error); REQUIRE(library);
        ALMetalRenderPipelineDescriptor rd; rd.library = library.get();
        rd.vertex_function = "resource_owner_vertex"; rd.fragment_function = "resource_owner_fragment";
        rd.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        render = ALMetalRenderPipeline::create(*device, rd, &error); INFO(error); REQUIRE(render);
        ALMetalComputePipelineDescriptor cd; cd.library = library.get(); cd.function = "resource_owner_compute";
        compute = ALMetalComputePipeline::create(*device, cd, &error); INFO(error); REQUIRE(compute);
        const auto rv = [(__bridge id<MTLRenderPipelineState>)render->nativeHandle() shaderValidation];
        const auto cv = [(__bridge id<MTLComputePipelineState>)compute->nativeHandle() shaderValidation];
        std::cout << "resource_owner_validation mode=" << unsigned(mode) << " render=" << unsigned(rv)
                  << " compute=" << unsigned(cv) << '\n';
        REQUIRE(rv == MTLShaderValidationDisabled); REQUIRE(cv == MTLShaderValidationDisabled);
        ALMetalBufferDescriptor bd; bd.length = 6; bd.storage = ALMetalStorageMode::shared;
        indices = ALMetalBuffer::create(*device, bd, &error); INFO(error); REQUIRE(indices);
        const std::array<std::uint16_t,3> data{0,1,2};
        std::memcpy(indices->mappedBytes(), data.data(), sizeof(data));
        REQUIRE(index_count>=1); REQUIRE(index_count<=256);
        for(unsigned i=1;i<index_count;++i)
        {
            auto buffer=ALMetalBuffer::create(*device,bd,&error); REQUIRE(buffer);
            const auto changed=i%2?std::array<std::uint16_t,3>{0,2,1}:data;
            std::memcpy(buffer->mappedBytes(),changed.data(),sizeof(changed));
            alternate_indices.push_back(std::move(buffer));
        }
        pool = ALMetalRenderTargetPool::create(*device, 1 << 20); REQUIRE(pool);
    }
    ALMetalRenderTargetDescriptor targetDescriptor() const
    {
        ALMetalRenderTargetDescriptor d; d.width = width; d.height = height;
        d.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
        d.colors[0].shader_read = true; d.colors[0].shader_write = true;
        return d;
    }
    unsigned indexCount() const noexcept {return unsigned(alternate_indices.size())+1;}
    const ALMetalBuffer& index(unsigned draw) const noexcept
    {
        if(alternate_indices.empty())return *indices;
        const auto at=draw%indexCount();
        return at?*alternate_indices[at-1]:*indices;
    }
};

Row frame(Owner& owner, unsigned cell, bool verify)
{
    Row row;
    const auto check = [&](bool ok, const char* stage) {
        if (!ok) throw std::runtime_error(std::string("Resource owner ") + stage + ": " + owner.error);
    };
    const auto begin = stamp();
    Stamp before_retire;
    @autoreleasepool
    {
        // Cell3 measures a cold transient pool and all backing creation/retirement
        // each frame. Other cells retain only the ordinary idle target pool.
        auto pool = cell == 3 ? ALMetalRenderTargetPool::create(*owner.device, 1 << 20) : owner.pool;
        check(bool(pool), "pool-create");
        const auto reuse_before = pool->stats().reuses;
        const auto committed_before = ALMetalDevice::committedCommandBufferCount();
        auto command = owner.device->makeCommandBuffer("Resource owner complete native flow", &owner.error); check(bool(command), "command-create");
        ALMetalBufferDescriptor bd; bd.length = 64; bd.storage = ALMetalStorageMode::private_gpu;
        auto color = ALMetalBuffer::create(*owner.device, bd, &owner.error); check(bool(color), "color-create");
        auto pattern = ALMetalBuffer::create(*owner.device, bd, &owner.error); check(bool(pattern), "pattern-create");
        const std::array<float,16> red{1,0,0,1};
        check(bool(color->upload(*command, 0, red.data(), sizeof(red), &owner.error)), "color-upload");
        check(bool(pattern->fillWords(*command, 0, bd.length, 0x875421abu, &owner.error)), "pattern-fill");
        auto source = pool->acquire(owner.targetDescriptor(), &owner.error, command->resourceUsePlan()); check(bool(source), "source-acquire");
        auto output = pool->acquire(owner.targetDescriptor(), &owner.error, command->resourceUsePlan()); check(bool(output), "output-acquire");
        void* source_native = source->colorTexture(0)->nativeHandle();
        ALMetalRenderPassConfig config; config.colors[0].load = ALMetalLoadAction::clear;
        config.colors[0].clear = {0,0,0,1}; config.colors[0].store = ALMetalStoreAction::store;
        auto pass = source->makeRenderPass(config, &owner.error); check(bool(pass), "render-pass");
        auto render = ALMetalRenderEncoder::begin(*command, *pass, "Resource owner indexed render", &owner.error); check(bool(render), "render-encoder");
        check(bool(render->setPipeline(*owner.render, &owner.error)), "render-pipeline");
        check(bool(render->setViewport({0,0,Owner::width,Owner::height,0,1}, &owner.error)), "viewport");
        check(bool(render->setCullMode(ALMetalCullMode::none, &owner.error)), "cull-mode");
        check(bool(render->setFragmentBuffer(0, *color, 0, &owner.error)), "color-bind");
        const unsigned draws = cell == 0 ? 1 : ((cell == 2 || cell>=4) ? 256 : 16);
        for (unsigned i = 0; i != draws; ++i)
            check(bool(render->drawIndexed(ALMetalPrimitiveType::triangle, 3, ALMetalIndexType::uint16,
                owner.index(i), 0, 1, &owner.error)), "indexed-draw");
        check(bool(render->end(&owner.error)), "render-end"); render.reset(); pass.reset();
        auto compute = ALMetalComputeEncoder::begin(*command, "Resource owner sampled compute", &owner.error); check(bool(compute), "compute-encoder");
        check(bool(compute->setPipeline(*owner.compute, &owner.error)), "compute-pipeline");
        check(bool(compute->setBuffer(0, *pattern, 0, &owner.error)), "compute-pattern");
        check(bool(compute->setTexture(0, source->colorTexture(0), &owner.error)), "compute-source");
        check(bool(compute->setTexture(1, output->colorTexture(0), &owner.error)), "compute-output");
        check(bool(compute->dispatch(Owner::width,Owner::height,1,8,8,1,&owner.error)), "compute-dispatch");
        check(bool(compute->end(&owner.error)), "compute-end"); compute.reset();
        source.reset();
        auto final = pool->acquire(owner.targetDescriptor(), &owner.error, command->resourceUsePlan()); check(bool(final), "final-acquire");
        check(bool(final->colorTexture(0)->nativeHandle() == source_native), "same-command-reuse");
        check(bool(final->copyColorFrom(*command, 0, *output->colorTexture(0), {}, &owner.error)), "final-copy");
        const auto encoded = stamp();
        check(bool(command->commit(&owner.error)), "commit");
        const auto submitted = stamp();
        check(bool(command->waitUntilCompleted(&owner.error)), "wait-complete");
        const auto completed = stamp();
        id<MTLCommandBuffer> native = (__bridge id<MTLCommandBuffer>)command->nativeIdentity();
        row.gpu_start = native.GPUStartTime; row.gpu_end = native.GPUEndTime;
        check(bool(row.gpu_start > 0), "gpu-start"); check(bool(row.gpu_end > row.gpu_start), "gpu-end");
        if (const auto& graph = command->resourceUsePlan())
        {
            const auto state = graph->stats();
            check(bool(state.valid), "graph-valid"); check(bool(state.retired), "graph-retired"); check(bool(!(state.opaque)), "graph-not-opaque");
            check(bool(state.resources >= owner.indexCount()+7), "graph-use-count"); check(bool(state.passes == 5), "graph-pass-count");
            row.resources = state.resources; row.passes = state.passes;
        }
        if (verify)
        {
            std::vector<std::uint8_t> pixels(Owner::width * Owner::height * 4);
            check(bool(final->colorTexture(0)->readback(*owner.device, 0, 0, {}, pixels.data(), Owner::width * 4, &owner.error)), "pixel-readback");
            for (std::size_t i=0; i<pixels.size(); i+=4)
            {
                check(bool(pixels[i] == 255), "pixel-red"); check(bool(pixels[i+1] == 0), "pixel-green");
                check(bool(pixels[i+2] == 255), "pixel-blue"); check(bool(pixels[i+3] == 255), "pixel-alpha");
            }
            std::uint32_t value = 0;
            check(bool(pattern->readback(*owner.device, 0, &value, sizeof(value), &owner.error)), "pattern-readback");
            check(bool(value == 0x875421abu), "pattern-value");
        }
        row.native_commit_delta = ALMetalDevice::committedCommandBufferCount() - committed_before;
        check(bool(row.native_commit_delta == (verify ? 3 : 1)), "commit-count");
        row.encode_wall = encoded.wall - begin.wall; row.encode_cpu = encoded.cpu - begin.cpu;
        row.submit_wall = submitted.wall - encoded.wall; row.submit_cpu = submitted.cpu - encoded.cpu;
        row.wait_wall = completed.wall - submitted.wall; row.wait_cpu = completed.cpu - submitted.cpu;
        before_retire = stamp();
        row.observe_wall = before_retire.wall - completed.wall;
        row.observe_cpu = before_retire.cpu - completed.cpu;
        command.reset(); color.reset(); pattern.reset(); output.reset(); final.reset();
        row.pool_reuses = pool->stats().reuses - reuse_before;
        check(bool(row.pool_reuses >= 1), "pool-reuse-count");
        row.cached_targets = pool->cachedTargetCount(); check(bool(row.cached_targets == 2), "cached-target-count");
        if (cell == 3) { pool->purge(); check(bool(pool->cachedTargetCount() == 0), "transient-purge"); }
        pool.reset();
    }
    const auto end = stamp(); // Includes the frame-local Objective-C autorelease pool.
    row.retire_wall = end.wall - before_retire.wall; row.retire_cpu = end.cpu - before_retire.cpu;
    row.total_wall = end.wall - begin.wall; row.total_cpu = end.cpu - begin.cpu;
    check(row.total_wall == row.encode_wall + row.submit_wall + row.wait_wall + row.observe_wall + row.retire_wall, "wall-partition");
    check(row.total_cpu == row.encode_cpu + row.submit_cpu + row.wait_cpu + row.observe_cpu + row.retire_cpu, "thread-cpu-partition");
    const auto stats = owner.device->uniqueResourceStats();
    check(bool(stats.active_commands == 0), "active-command-retirement"); check(bool(stats.uncommitted_uses == 0), "uncommitted-use-retirement"); check(bool(stats.submitted_uses == 0), "submitted-use-retirement");
    check(bool(stats.opaque_uncommitted_commands == 0), "opaque-owner-retirement");
    const auto expected = owner.mode == ALMetalResourcePlanMode::disabled ? 0u : owner.indexCount()+(cell==3?0u:2u);
    check(bool(stats.owned_backings == expected), "unique-backing-conservation");
    row.live_backings = stats.owned_backings; row.live_declared = stats.owned_declared_bytes;
    row.live_reported = stats.owned_metal_reported_bytes;
    return row;
}
}

TEST_CASE("Complete native resource allocation use and retirement owner cost",
          "[.][metal-resource-plan-owner-cost]")
{
#if !defined(NDEBUG)
    SKIP("Native resource owner costs require an optimized build");
#endif
    const auto setting = [](const char* name) { const char* value=std::getenv(name); return value ? std::string(value) : std::string(); };
    REQUIRE(setting("ALCHEMY_METAL_RESOURCE_PLAN_OWNER_COST") == "1");
    REQUIRE(setting("MTL_SHADER_VALIDATION") == "0"); REQUIRE(setting("MTL_DEBUG_LAYER") == "0");
    for (const char* name : {"MTL_CAPTURE_ENABLED", "MTL_HUD_ENABLED"})
        REQUIRE((setting(name).empty() || setting(name)=="0"));
    const auto cell_text = setting("ALCHEMY_METAL_RESOURCE_PLAN_OWNER_COST_CELL");
    const auto pair_text = setting("ALCHEMY_METAL_RESOURCE_PLAN_OWNER_COST_PAIR");
    REQUIRE(cell_text.size()==1); REQUIRE(cell_text[0]>='0'); REQUIRE(cell_text[0]<='5');
    REQUIRE(pair_text.size()==1); REQUIRE(pair_text[0]>='0'); REQUIRE(pair_text[0]<='2');
    const auto precheck_text = setting("ALCHEMY_METAL_RESOURCE_PLAN_OWNER_PRECHECK_ONLY");
    REQUIRE((precheck_text.empty() || precheck_text=="0" || precheck_text=="1"));
    const bool precheck_only = precheck_text=="1";
    const unsigned cell=unsigned(cell_text[0]-'0'), pair=unsigned(pair_text[0]-'0');
    const std::array<ALMetalResourcePlanMode,3> modes{ALMetalResourcePlanMode::disabled,
        ALMetalResourcePlanMode::accounting_only, ALMetalResourcePlanMode::enabled};
    const unsigned ai = pair==2 ? 1 : 0, bi = pair==1 ? 1 : 2;
    const unsigned index_count=cell==4?2:(cell==5?256:1);
    resource_plan_owner_cost::Owner a(modes[ai],index_count), b(modes[bi],index_count);
    const auto check_a=resource_plan_owner_cost::frame(a,cell,true);
    const auto check_b=resource_plan_owner_cost::frame(b,cell,true);
    REQUIRE(check_a.native_commit_delta == check_b.native_commit_delta);
    const auto drain = [](resource_plan_owner_cost::Owner& owner) {
        owner.pool->purge(); owner.indices.reset(); owner.alternate_indices.clear();
        const auto state=owner.device->uniqueResourceStats();
        REQUIRE(state.active_commands==0); REQUIRE(state.uncommitted_uses==0); REQUIRE(state.submitted_uses==0);
        REQUIRE(state.owned_backings==0); REQUIRE(state.owned_declared_bytes==0); REQUIRE(state.owned_metal_reported_bytes==0);
        REQUIRE(state.backing_creations==state.backing_retirements);
    };
    std::cout << "resource_owner_precheck cell=" << cell << " pair=" << pair
              << " exact_pixels=1 exact_pattern=1 live_a=" << check_a.live_backings << " live_b=" << check_b.live_backings << '\n';
    if (precheck_only) { drain(a); drain(b); return; }
    // Warmups and every timed observation perform the same native work. Only
    // these two immediate prechecks copy pixels/words back to the CPU.
    for (unsigned i=0;i!=10;++i)
    {
        resource_plan_owner_cost::frame(i%2 ? b:a,cell,false);
        resource_plan_owner_cost::frame(i%2 ? a:b,cell,false);
    }
    std::cout << "resource_owner_cost_scope cell=" << cell << " pair=" << pair << " mode_a=" << ai << " mode_b=" << bi
              << " configured_draws=" << (cell==0?1:((cell==2||cell>=4)?256:16)) << " index_buffers=" << index_count
              << " extent=128x128 passes=5 main_commands=1 thread_cpu_not_all_process_cpu=1"
              << " compile_device_pso_long_lived_index_setup_excluded=1 timer_and_postwait_observer_overhead_retained=1 cold_pool=" << (cell==3)
              << " pixel_prechecks=2 readback_in_measurement=0 retirement_in_total=1\n";
    std::cout << "resource_owner_cost_header,cell,pair,block,position,arm,mode,encode_wall_ns,encode_thread_cpu_ns,submit_wall_ns,submit_thread_cpu_ns,wait_wall_ns,wait_thread_cpu_ns,post_wait_observation_wall_ns,post_wait_observation_thread_cpu_ns,retire_wall_ns,retire_thread_cpu_ns,total_wall_ns,total_thread_cpu_ns,gpu_start_s,gpu_end_s,gpu_envelope_ms,resources,passes,live_backings,live_declared_bytes,live_reported_bytes,native_commit_delta,pool_reuses,cached_targets\n";
    double previous_gpu_end=0;
    for (unsigned block=0;block!=150;++block)
    {
        const std::array<unsigned,4> order = block%2 ? std::array<unsigned,4>{1,0,0,1} : std::array<unsigned,4>{0,1,1,0};
        for (unsigned position=0;position!=4;++position)
        {
            const auto arm=order[position];
            const auto row=resource_plan_owner_cost::frame(arm?b:a,cell,false);
            REQUIRE(row.gpu_start >= previous_gpu_end); previous_gpu_end=row.gpu_end;
            std::cout << std::setprecision(17) << "resource_owner_cost," << cell << ',' << pair << ',' << block << ',' << position << ',' << arm << ',' << (arm?bi:ai)
                      << ',' << row.encode_wall << ',' << row.encode_cpu << ',' << row.submit_wall << ',' << row.submit_cpu
                      << ',' << row.wait_wall << ',' << row.wait_cpu << ',' << row.observe_wall << ',' << row.observe_cpu << ',' << row.retire_wall << ',' << row.retire_cpu
                      << ',' << row.total_wall << ',' << row.total_cpu << ',' << row.gpu_start << ',' << row.gpu_end
                      << ',' << (row.gpu_end-row.gpu_start)*1000.0 << ',' << row.resources << ',' << row.passes
                      << ',' << row.live_backings << ',' << row.live_declared << ',' << row.live_reported
                      << ',' << row.native_commit_delta << ',' << row.pool_reuses << ',' << row.cached_targets << '\n';
            std::cout.flush();
        }
    }
    drain(a); drain(b);
}

TEST_CASE("Native index reads reject an isolated mode registry on the same Metal device and recover only in a new encoder",
          "[metal-resource-plan-native][PPD-435]")
{
    @autoreleasepool
    {
        Fixture f;
        auto other=ALMetalDevice::create(&f.error,ALMetalResourcePlanMode::accounting_only); REQUIRE(other);
        REQUIRE(other->nativeHandle()==f.device->nativeHandle()); // Actual SAME MTLDevice, deliberately isolated modes.
        REQUIRE(other->nativeCommandQueue()!=f.device->nativeCommandQueue());
        REQUIRE(other->resourcePlan().get()!=f.device->resourcePlan().get());
        ALMetalBufferDescriptor bd; bd.length=6; bd.storage=ALMetalStorageMode::shared;
        auto index=ALMetalBuffer::create(*f.device,bd,&f.error); REQUIRE(index);
        auto foreign=ALMetalBuffer::create(*other,bd,&f.error); REQUIRE(foreign);
        // Process-shared domains need not have equal counters. The pure Plan
        // tests independently retain the colliding-numeric-ID negative.
        REQUIRE(index->resourceView().generation()==foreign->resourceView().generation());
        const std::array<std::uint16_t,3> values{0,1,2};
        std::memcpy(index->mappedBytes(),values.data(),sizeof(values));
        std::memcpy(foreign->mappedBytes(),values.data(),sizeof(values));
        const std::string source=R"MSL(
#include <metal_stdlib>
using namespace metal;
vertex float4 cache_vertex(uint v [[vertex_id]])
{
    const float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
    return float4(p[v],0,1);
}
fragment float4 cache_fragment() { return float4(1,0,1,1); }
)MSL";
        auto library=ALMetalShaderLibrary::compileSource(*f.device,source,"Index cache actual registry contract",&f.error); REQUIRE(library);
        ALMetalRenderPipelineDescriptor pd; pd.library=library.get();
        pd.vertex_function="cache_vertex";pd.fragment_function="cache_fragment";
        pd.colors[0].format=ALMetalPixelFormat::rgba8_unorm;
        auto pipeline=ALMetalRenderPipeline::create(*f.device,pd,&f.error); REQUIRE(pipeline);
        validation([(__bridge id<MTLRenderPipelineState>)pipeline->nativeHandle() shaderValidation],"index_cache_registry");
        auto target=ALMetalRenderTarget::create(*f.device,f.targetDescriptor(),&f.error); REQUIRE(target);
        ALMetalRenderPassConfig config;config.colors[0].load=ALMetalLoadAction::clear;
        config.colors[0].clear={0,0,0,1};config.colors[0].store=ALMetalStoreAction::store;
        auto pass=target->makeRenderPass(config,&f.error);REQUIRE(pass);
        auto command=f.device->makeCommandBuffer("Index cache invalidated joined prefix",&f.error);REQUIRE(command);
        auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"Index registry failure",&f.error);REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline,&f.error));REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&f.error));
        REQUIRE(encoder->setViewport({0,0,side,side,0,1},&f.error));
        REQUIRE(encoder->setIndexedTriangleJoining(true,&f.error));
        REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle,3,ALMetalIndexType::uint16,*index,0,1,&f.error));
        CHECK_FALSE(encoder->drawIndexed(ALMetalPrimitiveType::triangle,3,ALMetalIndexType::uint16,*foreign,0,1,&f.error));
        CHECK_FALSE(command->resourceUsePlan()->stats().valid);
        CHECK_FALSE(encoder->drawIndexed(ALMetalPrimitiveType::triangle,3,ALMetalIndexType::uint16,*index,0,1,&f.error));
        REQUIRE(encoder->end(&f.error));
        const auto commits=ALMetalDevice::committedCommandBufferCount();
        CHECK_FALSE(command->commit(&f.error));CHECK(ALMetalDevice::committedCommandBufferCount()==commits);
        encoder.reset();command.reset();CHECK(f.device->uniqueResourceStats().uncommitted_uses==0);
        f.error.clear();
        auto changed=ALMetalBuffer::create(*f.device,bd,&f.error);REQUIRE(changed);
        std::memcpy(changed->mappedBytes(),values.data(),sizeof(values));
        command=f.device->makeCommandBuffer("Index cache new encoder valid exact tickets",&f.error);REQUIRE(command);
        encoder=ALMetalRenderEncoder::begin(*command,*pass,"Index registry positive",&f.error);REQUIRE(encoder);
        REQUIRE(encoder->setPipeline(*pipeline,&f.error));REQUIRE(encoder->setCullMode(ALMetalCullMode::none,&f.error));
        REQUIRE(encoder->setViewport({0,0,side,side,0,1},&f.error));
        for(const ALMetalBuffer* current:{index.get(),index.get(),changed.get(),index.get()})
            REQUIRE(encoder->drawIndexed(ALMetalPrimitiveType::triangle,3,ALMetalIndexType::uint16,*current,0,1,&f.error));
        REQUIRE(encoder->end(&f.error));CHECK(command->resourceUsePlan()->stats().resources==3);
        index.reset();changed.reset();CHECK(f.device->uniqueResourceStats().owned_backings==3);
        REQUIRE(command->commitAndWait(&f.error));
        CHECK(f.device->uniqueResourceStats().owned_backings==1);
        std::array<std::uint8_t,side*side*4> pixels{};
        REQUIRE(target->colorTexture(0)->readback(*f.device,0,0,{},pixels.data(),side*4,&f.error));magenta(pixels);
    }
}

TEST_CASE("Native cached reads rerecord an exact same-command generation and release backing on completion",
          "[metal-resource-plan-native][PPD-435]")
{
    @autoreleasepool
    {
        Fixture f;using Result=RP::Command::ReadUseResult;
        id<MTLBuffer> source=[(__bridge id<MTLDevice>)f.device->nativeHandle() newBufferWithLength:16 options:MTLResourceStorageModeShared];REQUIRE(source);
        const std::array<std::uint8_t,16> input{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        std::memcpy(source.contents,input.data(),input.size());
        RP::Description description;description.declared_bytes=input.size();RP::View source_view;
        REQUIRE(f.device->registerNativeBacking((__bridge void*)source,description,source_view,&f.error));
        ALMetalBufferDescriptor bd;bd.length=input.size();bd.storage=ALMetalStorageMode::shared;
        auto output=ALMetalBuffer::create(*f.device,bd,&f.error);REQUIRE(output);
        auto command=f.device->makeCommandBuffer("Cached exact generations actual native copies",&f.error);REQUIRE(command);
        RP::Command::ReadUseCache cache;
        const auto original=source_view.generation();
        for(unsigned round=0;round!=2;++round)
        {
            std::uint64_t pass=0;REQUIRE(command->beginResourcePass(RP::Pass::blit,pass,&f.error));
            CHECK(command->resourceUsePlan()->useCachedRead(source_view,cache)==Result::recorded);
            CHECK(command->resourceUsePlan()->useCachedRead(source_view,cache)==Result::cached);
            REQUIRE(command->useResource(output->resourceView(),RP::Access::write,&f.error));
            auto native=(__bridge id<MTLCommandBuffer>)command->nativeHandleForDeclaredResources();REQUIRE(native);
            auto blit=[native blitCommandEncoder];REQUIRE(blit);
            [blit copyFromBuffer:source sourceOffset:0 toBuffer:(__bridge id<MTLBuffer>)output->nativeHandle() destinationOffset:0 size:input.size()];
            [blit endEncoding];REQUIRE(command->endResourcePass(pass,&f.error));
            if(round==0)REQUIRE(source_view.issueGeneration(command->resourceUsePlan()));
        }
        CHECK(source_view.generation()==original+1);
        CHECK(command->resourceUsePlan()->stats().resources==3);CHECK(command->resourceUsePlan()->stats().passes==2);
        CHECK(f.device->uniqueResourceStats().logical_aliases==2);
        source_view={};source=nil;CHECK(f.device->uniqueResourceStats().owned_backings==2);
        REQUIRE(command->commitAndWait(&f.error));CHECK(f.device->uniqueResourceStats().owned_backings==1);
        std::array<std::uint8_t,16> result{};REQUIRE(output->read(0,result.data(),result.size(),&f.error));CHECK(result==input);
        CHECK(command->resourceUsePlan()->useCachedRead({},cache)==Result::failed);
        output.reset();CHECK(f.device->uniqueResourceStats().owned_backings==0);
    }
}
