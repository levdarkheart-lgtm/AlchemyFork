/**
 * @file almetal_avatar_resources_test.mm
 * @brief Distinct native avatar publications survive view and owner changes.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalavatarresources.h"
#include "../almetaldevice.h"
#include "../almetalworld.h"

#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <dispatch/dispatch.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
using Cache = ALMetalAvatarResources;

// Test-only rescue: failures must not strand the device queue behind this gate.
struct AvatarResourceGate
{
    id<MTLSharedEvent> event;
    std::shared_ptr<std::atomic<int>> phase =
        std::make_shared<std::atomic<int>>(0); // active, disarmed, tripped

    explicit AvatarResourceGate(id<MTLSharedEvent> value) : event(value)
    {
        const auto state = phase;
        id<MTLSharedEvent> retained_event = event;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC),
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
                int expected = 0;
                if (state->compare_exchange_strong(expected, 2))
                    retained_event.signaledValue = 1;
            });
    }
    void release()
    {
        int expected = 0;
        phase->compare_exchange_strong(expected, 1);
        event.signaledValue = 1;
    }
    bool tripped() const { return phase->load() == 2; }
    ~AvatarResourceGate() { release(); }
};

Cache::Record publication(const ALMetalDevice& device,
                          std::array<std::uint8_t, 4> color,
                          bool hdr, bool normal_hdr)
{
    std::string error;
    Cache::Record result;
    result.device = &device;
    result.width = result.height = 2;
    result.hdr = hdr;
    result.normal_hdr = normal_hdr;
    const std::array<ALMetalPixelFormat, 4> formats{
        ALMetalPixelFormat::rgba8_unorm_srgb,
        ALMetalPixelFormat::rgba8_unorm,
        normal_hdr ? ALMetalPixelFormat::rgba16_unorm
                   : ALMetalPixelFormat::rgb10a2_unorm,
        hdr ? ALMetalPixelFormat::rg11b10_float
            : ALMetalPixelFormat::rgba8_unorm};
    auto command = device.makeCommandBuffer("Avatar resource fixture upload", &error);
    INFO(error); REQUIRE(command);
    for (std::size_t index = 0; index < result.textures.size(); ++index)
    {
        ALMetalTextureDescriptor d;
        d.width = d.height = 2;
        d.format = formats[index];
        d.usage = ALMetalTextureUsage::shader_read;
        d.label = "Distinct native avatar quartet";
        result.textures[index] = ALMetalTexture::create(device, d, &error);
        INFO(error); REQUIRE(result.textures[index]);
        const std::size_t stride = result.textures[index]->bytesPerPixel();
        std::array<std::uint8_t, 8> texel{};
        if (index == 0)
            std::memcpy(texel.data(), color.data(), 4);
        else if (index == 1)
        {
            const std::array<std::uint8_t, 4> spec{
                color[0], color[1], color[2], 64};
            std::memcpy(texel.data(), spec.data(), 4);
        }
        else if (index == 2 && normal_hdr)
        {
            const std::array<std::uint16_t, 4> normal{
                32768, 32768, 0, 22282};
            std::memcpy(texel.data(), normal.data(), 8);
        }
        else if (index == 2)
        {
            const std::uint32_t normal = 512u | (512u << 10u) | (1u << 30u);
            std::memcpy(texel.data(), &normal, 4);
        }
        else if (!hdr)
        {
            const std::array<std::uint8_t, 4> emissive{
                color[0], color[1], color[2], 0};
            std::memcpy(texel.data(), emissive.data(), 4);
        }
        std::vector<std::uint8_t> pixels(4 * stride);
        for (std::size_t pixel = 0; pixel < 4; ++pixel)
            std::memcpy(pixels.data() + pixel * stride, texel.data(), stride);
        REQUIRE(result.textures[index]->upload(
            *command, 0, 0, {}, pixels.data(), 2 * stride, &error));
    }
    // Fixture only: no unresolved producer is mislabeled a ready publication.
    REQUIRE(command->commitAndWait(&error));
    INFO(error);
    return result;
}

using Snapshot = std::array<std::vector<std::uint8_t>, 5>;

std::unique_ptr<ALMetalRenderTarget> encodePair(
    ALMetalWorldGBufferRenderer& renderer,
    const std::array<Cache::Lease, 2>& records, std::string& error,
    bool retain_records = false)
{
    std::array<ALMetalWorldSimpleBatch, 2> batches{};
    for (std::size_t index = 0; index < batches.size(); ++index)
    {
        REQUIRE(records[index]);
        auto& batch = batches[index];
        batch.impostor = true;
        if (retain_records) batch.impostor_texture_owner = records[index];
        for (std::size_t attachment = 0; attachment < 4; ++attachment)
            batch.impostor_textures[attachment] =
                records[index]->textures[attachment].get();
        batch.impostor_center[0] = index == 0 ? -0.5f : 0.5f;
        batch.impostor_center[2] = -0.5f;
        batch.impostor_left[0] = 0.4f;
        batch.impostor_up[1] = 0.7f;
        for (std::size_t axis = 0; axis < 4; ++axis)
        {
            batch.modelview_projection[axis * 4 + axis] = 1.f;
            batch.modelview[axis * 4 + axis] = 1.f;
            if (axis < 3) batch.impostor_normal_rotation[axis][axis] = 1.f;
        }
    }
    REQUIRE(records[0]->hdr == records[1]->hdr);
    REQUIRE(records[0]->normal_hdr == records[1]->normal_hdr);
    ALMetalWorldGBufferScene scene;
    scene.width = 64; scene.height = 32;
    scene.hdr = records[0]->hdr;
    scene.normal_hdr = records[0]->normal_hdr;
    scene.batches = batches.data(); scene.batch_count = batches.size();
    auto target = renderer.render(scene, &error);
    INFO(error); REQUIRE(target);
    return target;
}

Snapshot snapshot(const ALMetalDevice& device, const ALMetalRenderTarget& target,
                  std::string& error)
{
    Snapshot result;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        const ALMetalTexture* texture = target.colorTexture(attachment);
        REQUIRE(texture);
        const auto stride = texture->bytesPerPixel();
        result[attachment].resize(64 * 32 * stride);
        REQUIRE(texture->readback(device, 0, 0, {}, result[attachment].data(),
                                 64 * stride, &error));
    }
    result[4].resize(64 * 32 * sizeof(float));
    REQUIRE(target.depthStencilTexture()->readback(
        device, 0, 0, {}, result[4].data(), 64 * sizeof(float), &error));
    return result;
}
}

TEST_CASE("Native avatar publications retain distinct quartets across view ownership",
          "[almetal][avatar-resources]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error); REQUIRE(device);
    auto renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
    INFO(error); REQUIRE(renderer);
    Cache::Key a{}, b{}; a[0] = 1; b[0] = 2;
    auto main_owner = std::make_shared<Cache>();
    auto probe_owner = main_owner;
    REQUIRE(main_owner->publish(a, publication(*device, {255,0,0,255}, false, true), &error));
    REQUIRE(main_owner->publish(b, publication(*device, {0,255,0,255}, false, true), &error));
    std::array<Cache::Lease, 2> queued{
        main_owner->find(a, *device, false, true),
        main_owner->find(b, *device, false, true)};
    REQUIRE(queued[0]); REQUIRE(queued[1]);
    const auto baseline_target = encodePair(*renderer, queued, error);
    const auto baseline = snapshot(*device, *baseline_target, error);
    const std::size_t left = (16 * 64 + 16) * 4;
    const std::size_t right = (16 * 64 + 48) * 4;
    CHECK(baseline[0][left] == 255); CHECK(baseline[0][left + 1] == 0);
    CHECK(baseline[0][right] == 0); CHECK(baseline[0][right + 1] == 255);

    std::swap(main_owner, probe_owner);
    // A newly initialized view must not discard compatible main-view bakes.
    probe_owner->discardIncompatible(*device, false, true);
    std::array<Cache::Lease, 2> probe_records{
        probe_owner->find(a, *device, false, true),
        probe_owner->find(b, *device, false, true)};
    const auto probe_target = encodePair(*renderer, probe_records, error);
    CHECK(snapshot(*device, *probe_target, error) == baseline);
    probe_records = {};

    auto invalid = publication(*device, {0,0,255,255}, false, true);
    invalid.width = 3; // All actual native textures are still 2x2.
    CHECK_FALSE(main_owner->publish(a, std::move(invalid), &error));
    CHECK(main_owner->find(a, *device, false, true) == queued[0]);
    REQUIRE(main_owner->publish(a, publication(*device, {0,0,255,255}, false, true), &error));
    auto replacement = main_owner->find(a, *device, false, true);
    REQUIRE(replacement); CHECK(replacement != queued[0]);
    std::array<Cache::Lease, 2> fresh{replacement, queued[1]};
    const auto replacement_target = encodePair(*renderer, fresh, error);
    const auto replacement_pixels = snapshot(*device, *replacement_target, error);
    CHECK(replacement_pixels[0][left] == 0);
    CHECK(replacement_pixels[0][left + 2] == 255);
    CHECK(replacement_pixels[0][right + 1] == 255);
    fresh = {}; replacement.reset();
    main_owner->erase(b);
    CHECK_FALSE(probe_owner->find(b, *device, false, true));
    CHECK(main_owner->size() == 1);

    // Queue both OLD avatar records, then retire every cache/lease owner
    // before submission. Native command retention must preserve actual reads.
    REQUIRE(renderer->beginCommandBatch("Avatar resource lease retirement", &error));
    const auto pending_target = encodePair(*renderer, queued, error);
    REQUIRE(renderer->commandBatchActive());
    std::weak_ptr<const Cache::Record> weak_a = queued[0], weak_b = queued[1];
    main_owner->clear(); queued = {};
    main_owner.reset(); probe_owner.reset();
    CHECK(weak_a.expired()); CHECK(weak_b.expired());
    REQUIRE(renderer->commitCommandBatch(&error));
    CHECK(snapshot(*device, *pending_target, error) == baseline);

    // Production captures retain the Record itself, not only native textures.
    // Replace/erase all CPU publications before commit, then prove their
    // command leases remain alive while GPU execution is deliberately blocked.
    auto production_owner = std::make_shared<Cache>();
    REQUIRE(production_owner->publish(a,
        publication(*device, {255,0,0,255}, false, true), &error));
    REQUIRE(production_owner->publish(b,
        publication(*device, {0,255,0,255}, false, true), &error));
    std::array<Cache::Lease, 2> production_records{
        production_owner->find(a, *device, false, true),
        production_owner->find(b, *device, false, true)};
    REQUIRE(production_records[0]); REQUIRE(production_records[1]);
    auto blocker = device->makeCommandBuffer("Avatar lease completion gate", &error);
    REQUIRE(blocker);
    id<MTLCommandBuffer> native_blocker =
        (__bridge id<MTLCommandBuffer>)blocker->nativeHandle();
    id<MTLSharedEvent> event = [native_blocker.device newSharedEvent];
    REQUIRE(event);
    AvatarResourceGate gate(event);
    [native_blocker encodeWaitForEvent:event value:1];
    REQUIRE(blocker->commit(&error));
    REQUIRE(renderer->beginCommandBatch("Avatar production lease retirement", &error));
    const auto production_target = encodePair(*renderer, production_records, error, true);
    std::weak_ptr<const Cache::Record> production_a = production_records[0];
    std::weak_ptr<const Cache::Record> production_b = production_records[1];
    production_owner->clear(); production_records = {}; production_owner.reset();
    CHECK_FALSE(production_a.expired()); CHECK_FALSE(production_b.expired());
    REQUIRE(renderer->commitCommandBatch(&error));
    CHECK(blocker->status() != ALMetalCommandStatus::completed);
    CHECK_FALSE(production_a.expired()); CHECK_FALSE(production_b.expired());
    gate.release();
    REQUIRE(blocker->waitUntilCompleted(&error));
    CHECK(snapshot(*device, *production_target, error) == baseline);
    // GPU completion and its CPU completed-handler retirement need not be
    // observed simultaneously. Bound this fixture-only callback drain.
    for (unsigned attempt = 0;
         (!production_a.expired() || !production_b.expired()) && attempt < 1000;
         ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK(production_a.expired()); CHECK(production_b.expired());
    CHECK_FALSE(gate.tripped());
}

TEST_CASE("Native avatar publications validate formats and context invalidation",
          "[almetal][avatar-resources]")
{
    std::string error;
    auto device = ALMetalDevice::create(&error);
    auto other_device = ALMetalDevice::create(&error);
    INFO(error); REQUIRE(device); REQUIRE(other_device);
    // On M1 Max these wrappers can share the same physical device. This test
    // checks the publication-owner identity, not a fabricated second GPU.
    REQUIRE(device.get() != other_device.get());
    Cache cache;
    Cache::Key key{}; key[15] = 9;
    for (bool hdr : {false,true})
    for (bool normal_hdr : {false,true})
    {
        REQUIRE(cache.publish(key, publication(*device, {255,0,0,255}, hdr, normal_hdr), &error));
        const auto lease = cache.find(key, *device, hdr, normal_hdr);
        REQUIRE(lease);
        CHECK_FALSE(cache.find(key, *device, !hdr, normal_hdr));
        CHECK_FALSE(cache.find(key, *device, hdr, !normal_hdr));
        CHECK_FALSE(cache.find(key, *other_device, hdr, normal_hdr));
        auto invalid = publication(*device, {0,255,0,255}, hdr, normal_hdr);
        invalid.hdr = !hdr; // Actual emissive format must determine validity.
        CHECK_FALSE(cache.publish(key, std::move(invalid), &error));
        CHECK(cache.find(key, *device, hdr, normal_hdr) == lease);
        cache.discardIncompatible(*device, hdr, normal_hdr);
        CHECK(cache.find(key, *device, hdr, normal_hdr) == lease);
        cache.discardIncompatible(*device, !hdr, normal_hdr);
        CHECK(cache.size() == 0);
        REQUIRE(cache.publish(key, publication(*device, {255,0,0,255}, hdr, normal_hdr), &error));
        cache.discardIncompatible(*device, hdr, !normal_hdr);
        CHECK(cache.size() == 0);
        REQUIRE(cache.publish(key, publication(*device, {255,0,0,255}, hdr, normal_hdr), &error));
        cache.discardIncompatible(*other_device, hdr, normal_hdr);
        CHECK(cache.size() == 0);
        CHECK(lease->textures[0]->nativeHandle() != nullptr);
    }
}
