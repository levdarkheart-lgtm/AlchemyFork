/**
 * @file almetal_shadow_cache_test.mm
 * @brief Actual immutable-source cache bounds and scan admission.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "../almetalshadowcache.h"
#include "../almetalshadowprepare.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace
{
using Source = ALMetalShadowSourceRecord;
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::shared_ptr<const ALMetalBuffer> vertices, indices;
    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        vertices = buffer(256); indices = buffer(6);
    }
    std::shared_ptr<const ALMetalBuffer> buffer(std::size_t length)
    {
        ALMetalBufferDescriptor descriptor; descriptor.length = length;
        auto result = ALMetalBuffer::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result); return result;
    }
    std::shared_ptr<const Source> source(std::uint64_t generation,
        std::shared_ptr<const ALMetalBuffer> vb = {},
        std::shared_ptr<const ALMetalTexture> image = {})
    {
        if (!vb) vb = vertices;
        ALMetalWorldSpotShadowBatch batch;
        batch.vertices = vb.get(); batch.indices = indices.get();
        batch.vertex_count = 3; batch.index_count = 3;
        std::vector<std::shared_ptr<const ALMetalTexture>> textures;
        if (image)
        {
            batch.coverage = ALMetalWorldSpotShadowCoverage::texture_mask;
            batch.texture_count = 2; batch.texcoord_offset = 64; batch.color_offset = 96;
            for (std::size_t slot = 0; slot < 2; ++slot)
            {
                batch.coverageData().textures[slot] = image.get();
                batch.coverageData().alpha_cutoffs[slot] = .5f;
                textures.push_back(image);
            }
        }
        auto result = Source::create(generation, batch, vb, indices, std::move(textures), &error);
        if (!result) FAIL(error);
        return result;
    }
};
}

TEST_CASE("almetal: source admission preserves the current epoch across a capacity-plus-one scan PPD106",
          "[almetal][shadow-source-cache][ppd106-scan]")
{
    @autoreleasepool
    {
        Fixture fixture;
        constexpr std::size_t capacity = 8192, count = capacity + 1;
        ALMetalShadowSourceCache cache(capacity, 1024 * 1024, 2);
        std::vector<std::shared_ptr<const void>> owners;
        std::vector<std::shared_ptr<const Source>> sources;
        owners.reserve(count); sources.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            owners.push_back(std::make_shared<std::size_t>(i));
            sources.push_back(fixture.source(i + 1));
            cache.publish(owners.back(), sources.back(), 42);
        }
        CHECK(cache.entryCount() == capacity);
        CHECK(cache.estimatedRetainedBytes() == 262);
        for (std::size_t pass = 1; pass < 6; ++pass)
        {
            std::size_t hits = 0, publications = 0;
            for (std::size_t i = 0; i < count; ++i)
            {
                if (const auto* found = cache.find(owners[i].get()))
                {
                    if (found->get() != sources[i].get()) FAIL("Cache changed source identity");
                    ++hits; cache.touch(owners[i].get(), 42);
                }
                else publications += cache.publish(owners[i], sources[i], 42);
            }
            std::cout << "SHADOW_CACHE_SCAN capacity=" << capacity << " sources=" << count
                << " pass=" << pass << " hits=" << hits << " admitted_misses=" << publications
                << " retained=" << cache.entryCount() << " claim=cache_admission_not_viewer_timing\n";
            CAPTURE(pass, hits, publications);
            CHECK(hits == capacity);
            CHECK(publications == 0);
            CHECK(cache.entryCount() == capacity);
            CHECK(cache.estimatedRetainedBytes() == 262);
        }
    }
}

TEST_CASE("almetal: shadow cache admits a new camera epoch and expires old owners across wrap",
          "[almetal][shadow-source-cache]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALMetalShadowSourceCache cache(3, 4096, 2);
        std::array<std::shared_ptr<const void>, 6> owners;
        std::array<std::shared_ptr<const Source>, 6> sources;
        for (std::size_t i = 0; i < owners.size(); ++i)
        {owners[i] = std::make_shared<std::size_t>(i); sources[i] = fixture.source(i + 1);}
        for (std::size_t i = 0; i < 3; ++i) REQUIRE(cache.publish(owners[i], sources[i], 10));
        CHECK_FALSE(cache.publish(owners[3], sources[3], 10));
        for (std::size_t i = 0; i < 3; ++i) CHECK(cache.find(owners[i].get()));
        for (std::size_t i = 3; i < 6; ++i) REQUIRE(cache.publish(owners[i], sources[i], 11));
        for (std::size_t i = 0; i < 3; ++i) CHECK_FALSE(cache.find(owners[i].get()));
        for (std::size_t i = 3; i < 6; ++i) CHECK(cache.find(owners[i].get()));
        const auto* stable = cache.find(owners[3].get()); REQUIRE(stable);
        cache.touch(owners[3].get(), 11);
        CHECK(cache.find(owners[3].get()) == stable);
        CHECK(stable->get() == sources[3].get());
        cache.trim(13); CHECK(cache.entryCount() == 3);
        cache.trim(14); CHECK(cache.entryCount() == 0); CHECK(cache.estimatedRetainedBytes() == 0);
        REQUIRE(cache.publish(owners[0], sources[0], std::numeric_limits<std::uint32_t>::max() - 1));
        cache.trim(0); CHECK(cache.entryCount() == 1);
        cache.trim(1); CHECK(cache.entryCount() == 0);
    }
}

TEST_CASE("almetal: shadow cache replaces current generations without invalidating packet leases",
          "[almetal][shadow-source-cache]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALMetalShadowSourceCache cache(1, 1024, 2);
        auto owner = std::make_shared<int>(1);
        auto old = fixture.source(1);
        std::weak_ptr<const Source> old_lifetime = old;
        REQUIRE(cache.publish(owner, old, 8));
        auto packet = *cache.find(owner.get()); old.reset();
        auto next_vertices = fixture.buffer(512);
        auto next = fixture.source(2, next_vertices);
        REQUIRE(cache.publish(owner, next, 8));
        REQUIRE(cache.find(owner.get()));
        CHECK(cache.find(owner.get())->get() == next.get());
        CHECK(cache.entryCount() == 1); CHECK(cache.estimatedRetainedBytes() == 518);
        CHECK_FALSE(old_lifetime.expired());
        auto oversized = fixture.source(3, fixture.buffer(2048));
        CHECK_FALSE(cache.publish(owner, oversized, 8));
        CHECK_FALSE(cache.find(owner.get()));
        CHECK(cache.entryCount() == 0); CHECK(cache.estimatedRetainedBytes() == 0);
        CHECK(oversized->prototype().vertices->length() == 2048);
        cache.clear(); CHECK_FALSE(old_lifetime.expired());
        packet.reset(); CHECK(old_lifetime.expired());
    }
}

TEST_CASE("almetal: shadow cache accounts aliased buffers and repeated material slots only once",
          "[almetal][shadow-source-cache]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALMetalTextureDescriptor descriptor; descriptor.width = descriptor.height = 2;
        descriptor.format = ALMetalPixelFormat::rgba8_unorm;
        descriptor.usage = ALMetalTextureUsage::shader_read;
        std::shared_ptr<const ALMetalTexture> image = ALMetalTexture::create(*fixture.device, descriptor, &fixture.error);
        REQUIRE(image);
        const auto texture_bytes = image->estimatedAllocationBytes(); REQUIRE(texture_bytes > 0);
        ALMetalShadowSourceCache cache(8, std::numeric_limits<std::uint64_t>::max(), 2);
        auto a = std::make_shared<int>(1), b = std::make_shared<int>(2), c = std::make_shared<int>(3);
        auto source_a = fixture.source(1);
        auto source_b = fixture.source(2, {}, image);
        auto source_c = fixture.source(3, fixture.buffer(512), image);
        REQUIRE(cache.publish(a, source_a, 1)); CHECK(cache.estimatedRetainedBytes() == 262);
        REQUIRE(cache.publish(b, source_b, 1)); CHECK(cache.estimatedRetainedBytes() == 262 + texture_bytes);
        REQUIRE(cache.publish(c, source_c, 1)); CHECK(cache.estimatedRetainedBytes() == 774 + texture_bytes);
        CHECK_FALSE(cache.publish(a, {}, 1)); CHECK_FALSE(cache.find(a.get()));
        CHECK(cache.estimatedRetainedBytes() == 774 + texture_bytes);
        REQUIRE(cache.publish(b, source_c, 1));
        CHECK(cache.estimatedRetainedBytes() == 518 + texture_bytes);
        cache.clear(); CHECK(cache.entryCount() == 0); CHECK(cache.estimatedRetainedBytes() == 0);
    }
}

TEST_CASE("almetal: shadow cache enforces unique byte pressure without scan eviction or oversized flushing",
          "[almetal][shadow-source-cache]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALMetalShadowSourceCache cache(4, 518, 2);
        std::array<std::shared_ptr<const void>, 3> owners;
        std::array<std::shared_ptr<const Source>, 3> sources;
        for (std::size_t i = 0; i < owners.size(); ++i)
        {owners[i] = std::make_shared<std::size_t>(i); sources[i] = fixture.source(i + 1, fixture.buffer(256));}
        REQUIRE(cache.publish(owners[0], sources[0], 20));
        REQUIRE(cache.publish(owners[1], sources[1], 20));
        CHECK_FALSE(cache.publish(owners[2], sources[2], 20));
        CHECK(cache.find(owners[0].get())); CHECK(cache.find(owners[1].get()));
        CHECK(cache.entryCount() == 2); CHECK(cache.estimatedRetainedBytes() == 518);
        auto huge_owner = std::make_shared<int>(4);
        CHECK_FALSE(cache.publish(huge_owner, fixture.source(4, fixture.buffer(1024)), 21));
        CHECK(cache.entryCount() == 2); CHECK(cache.estimatedRetainedBytes() == 518);
        REQUIRE(cache.publish(owners[2], sources[2], 21));
        CHECK_FALSE(cache.find(owners[0].get())); CHECK(cache.find(owners[1].get()));
        CHECK(cache.find(owners[2].get())); CHECK(cache.estimatedRetainedBytes() == 518);
        cache.setByteBudget(262); CHECK(cache.entryCount() == 1); CHECK(cache.estimatedRetainedBytes() == 262);
        cache.setByteBudget(261); CHECK(cache.entryCount() == 0); CHECK(cache.estimatedRetainedBytes() == 0);
        cache.setByteBudget(0); CHECK_FALSE(cache.publish(owners[0], sources[0], 21));
        ALMetalShadowSourceCache disabled(0, 4096, 2);
        CHECK_FALSE(disabled.publish(owners[0], sources[0], 21));
        CHECK_FALSE(cache.publish({}, sources[0], 21));
        CHECK(cache.entryCount() == 0); CHECK(cache.estimatedRetainedBytes() == 0);
    }
}

TEST_CASE("almetal: shadow cache eviction releases identity owners while independent source pins survive",
          "[almetal][shadow-source-cache]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALMetalShadowSourceCache cache;
        auto owner = std::make_shared<int>(1);
        auto source = fixture.source(1);
        std::weak_ptr<const void> owner_lifetime = owner;
        std::weak_ptr<const Source> source_lifetime = source;
        REQUIRE(cache.publish(owner, source, 1));
        auto packet = *cache.find(owner.get()); owner.reset(); source.reset();
        CHECK_FALSE(owner_lifetime.expired()); CHECK_FALSE(source_lifetime.expired());
        cache.clear();
        CHECK(owner_lifetime.expired()); CHECK_FALSE(source_lifetime.expired());
        CHECK(packet->prototype().vertices == fixture.vertices.get());
        packet.reset(); CHECK(source_lifetime.expired());
        CHECK(cache.entryCount() == 0); CHECK(cache.estimatedRetainedBytes() == 0);
    }
}

TEST_CASE("almetal: declined byte admission after old eviction leaves no resource reference residue",
          "[almetal][shadow-source-cache]")
{
    @autoreleasepool
    {
        Fixture fixture;
        ALMetalShadowSourceCache cache(8, 506, 2);
        auto old_owner = std::make_shared<int>(1), protected_owner = std::make_shared<int>(2);
        auto declined_owner = std::make_shared<int>(3), small_owner = std::make_shared<int>(4);
        auto old = fixture.source(1, fixture.buffer(100));
        auto protected_source = fixture.source(2, fixture.buffer(400));
        auto declined = fixture.source(3, fixture.buffer(150));
        REQUIRE(cache.publish(old_owner, old, 49));
        REQUIRE(cache.publish(protected_owner, protected_source, 50));
        CHECK(cache.estimatedRetainedBytes() == 506);
        CHECK_FALSE(cache.publish(declined_owner, declined, 50));
        CHECK_FALSE(cache.find(old_owner.get())); CHECK_FALSE(cache.find(declined_owner.get()));
        CHECK(cache.find(protected_owner.get()));
        CHECK(cache.entryCount() == 1); CHECK(cache.estimatedRetainedBytes() == 406);
        REQUIRE(cache.publish(small_owner, fixture.source(4, fixture.buffer(80)), 50));
        CHECK(cache.entryCount() == 2); CHECK(cache.estimatedRetainedBytes() == 486);
        cache.clear(); CHECK(cache.estimatedRetainedBytes() == 0);
    }
}
