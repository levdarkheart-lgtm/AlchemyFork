/**
 * @file alnativesourceworkingset_test.cpp
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "alnativesourceworkingset.h"

#include <catch2/catch_test_macros.hpp>
#include <array>

namespace
{
struct Source
{
    std::shared_ptr<const void> geometry;
    std::shared_ptr<const void> image;
};
using State = ALNativePublication::State;
std::shared_ptr<const Source> publish(const std::shared_ptr<State>& state,
    const std::shared_ptr<const void>& geometry, const std::shared_ptr<const void>& image = {})
{
    auto source = std::make_shared<const Source>(Source{geometry, image});
    REQUIRE(state->publish(state->revision(), source));
    return source;
}
}

TEST_CASE("Native source residency releases producer leases before writes but preserves packets",
          "[native-source-working-set][publication-lifetime]")
{
    ALNativeSourceWorkingSet cache(8, 1024, 2);
    ALNativePublication::Signal geometry_signal;
    auto state = std::make_shared<State>();
    geometry_signal.observe(state);
    auto geometry = std::make_shared<int>(7);
    auto source = publish(state, geometry);
    const std::array<ALNativeSourceWorkingSet::Resource, 1> resources{{{geometry, 128}}};
    REQUIRE(cache.retain(state, 1, state->publication(), resources, 10));
    std::weak_ptr<const Source> weak_source(source);
    source.reset();
    REQUIRE_FALSE(weak_source.expired());
    auto packet = state->publication();
    geometry_signal.willWrite();
    CHECK(state->dirty());
    CHECK_FALSE(state->publication());
    CHECK_FALSE(weak_source.expired());
    CHECK(std::static_pointer_cast<const Source>(packet)->geometry.get() == geometry.get());
    packet.reset();
    // Residency metadata is weak: a stale entry never keeps a native source
    // alive and never creates a false reader in the backing-store COW test.
    CHECK(weak_source.expired());
    CHECK_FALSE(cache.touch(state->identity(), 1, 10));
    CHECK(cache.entryCount() == 0);
    CHECK(cache.estimatedRetainedBytes() == 0);
}

TEST_CASE("Native source residency counts shared allocations once and protects current-frame subsets",
          "[native-source-working-set][residency-budget]")
{
    ALNativeSourceWorkingSet cache(2, 200, 2);
    auto geometry = std::make_shared<int>(1);
    auto image = std::make_shared<int>(2);
    const std::array<ALNativeSourceWorkingSet::Resource, 3> resources{{
        {geometry, 100}, {image, 80}, {geometry, 100}}};
    auto a = std::make_shared<State>(), b = std::make_shared<State>(), c = std::make_shared<State>();
    auto packet_a = publish(a, geometry, image);
    auto packet_b = publish(b, geometry, image);
    REQUIRE(cache.retain(a, 1, a->publication(), resources, 10));
    REQUIRE(cache.retain(b, 2, b->publication(), resources, 10));
    CHECK(cache.estimatedRetainedBytes() == 180);
    auto packet_c = publish(c, geometry, image);
    CHECK_FALSE(cache.retain(c, 3, c->publication(), resources, 10));
    CHECK_FALSE(c->publication());
    CHECK(a->publication());
    CHECK(b->publication());
    CHECK(packet_c->geometry.get() == geometry.get());
    CHECK(cache.entryCount() == 2);
    // Next frame can evict a cold entry; there is still only one shared native
    // allocation estimate and the old packet remains independently valid.
    publish(c, geometry, image);
    REQUIRE(cache.retain(c, 4, c->publication(), resources, 11));
    CHECK_FALSE(a->publication());
    CHECK(packet_a->image.get() == image.get());
    CHECK(cache.estimatedRetainedBytes() == 180);
    CHECK(cache.entryCount() == 2);
}

TEST_CASE("Late old native packet retirement cannot evict its replacement source",
          "[native-source-working-set][publication-generation]")
{
    ALNativeSourceWorkingSet cache(8, 1024, 2);
    auto state = std::make_shared<State>();
    auto old_geometry = std::make_shared<int>(1);
    auto old_packet = publish(state, old_geometry);
    const std::array<ALNativeSourceWorkingSet::Resource, 1> old_resources{{{old_geometry, 100}}};
    REQUIRE(cache.retain(state, 100, state->publication(), old_resources, 1));
    state->invalidate();
    auto new_geometry = std::make_shared<int>(2);
    auto new_packet = publish(state, new_geometry);
    const std::array<ALNativeSourceWorkingSet::Resource, 1> new_resources{{{new_geometry, 200}}};
    REQUIRE(cache.retain(state, 101, state->publication(), new_resources, 2));
    CHECK_FALSE(cache.erase(state->identity(), 100));
    CHECK(state->publication().get() == new_packet.get());
    CHECK(cache.estimatedRetainedBytes() == 200);
    old_packet.reset();
    CHECK(cache.touch(state->identity(), 101, 2));
    CHECK(cache.erase(state->identity(), 101));
    CHECK_FALSE(state->publication());
    CHECK(new_packet->geometry.get() == new_geometry.get());
}

TEST_CASE("Native source byte-budget shrink and age eviction release only publisher ownership",
          "[native-source-working-set][residency-retirement]")
{
    ALNativeSourceWorkingSet cache(8, 1024, 2);
    auto a = std::make_shared<State>(), b = std::make_shared<State>();
    auto first = std::make_shared<int>(1), second = std::make_shared<int>(2);
    auto packet_a = publish(a, first), packet_b = publish(b, second);
    const std::array<ALNativeSourceWorkingSet::Resource, 1> ra{{{first, 100}}}, rb{{{second, 200}}};
    REQUIRE(cache.retain(a, 1, a->publication(), ra, 8));
    REQUIRE(cache.retain(b, 2, b->publication(), rb, 9));
    cache.setByteBudget(200);
    CHECK_FALSE(a->publication());
    CHECK(b->publication());
    CHECK(cache.estimatedRetainedBytes() == 200);
    cache.trim(12);
    CHECK_FALSE(b->publication());
    CHECK(cache.entryCount() == 0);
    CHECK(cache.estimatedRetainedBytes() == 0);
    CHECK(packet_a->geometry.get() == first.get());
    CHECK(packet_b->geometry.get() == second.get());
}

TEST_CASE("Native source identity follows allocation control blocks instead of reused addresses",
          "[native-source-working-set][publication-identity]")
{
    ALNativeSourceWorkingSet cache(8, 1024, 2);
    int reused_address = 0;
    auto allocation_a = std::shared_ptr<const void>(&reused_address, [](const void*) {});
    auto allocation_b = std::shared_ptr<const void>(&reused_address, [](const void*) {});
    REQUIRE(allocation_a.get() == allocation_b.get());
    auto a = std::make_shared<State>(), b = std::make_shared<State>();
    publish(a, allocation_a); publish(b, allocation_b);
    const std::array<ALNativeSourceWorkingSet::Resource, 1> ra{{{allocation_a, 100}}}, rb{{{allocation_b, 200}}};
    REQUIRE(cache.retain(a, 1, a->publication(), ra, 1));
    REQUIRE(cache.retain(b, 2, b->publication(), rb, 1));
    CHECK(cache.estimatedRetainedBytes() == 300);
    cache.clear();
    CHECK_FALSE(a->publication());
    CHECK_FALSE(b->publication());
    CHECK(cache.entryCount() == 0);
}

TEST_CASE("Stale native source admission cannot withdraw an already newer publisher",
          "[native-source-working-set][publication-edge]")
{
    ALNativeSourceWorkingSet cache;
    auto state = std::make_shared<State>();
    auto old_geometry = std::make_shared<int>(1), new_geometry = std::make_shared<int>(2);
    auto old_packet = publish(state, old_geometry);
    state->invalidate();
    auto current = publish(state, new_geometry);
    const std::array<ALNativeSourceWorkingSet::Resource, 1> current_resources{{{new_geometry, 200}}};
    const std::array<ALNativeSourceWorkingSet::Resource, 1> stale_resources{{{old_geometry, 100}}};
    REQUIRE(cache.retain(state, 101, state->publication(), current_resources, 2));
    CHECK_FALSE(cache.retain(state, 100, old_packet, stale_resources, 2));
    CHECK_FALSE(cache.retain(state, 100, current, current_resources, 2));
    CHECK_FALSE(cache.retain(state, 102, current, current_resources, 2));
    CHECK(state->publication().get() == current.get());
    CHECK_FALSE(state->dirty());
    CHECK(cache.estimatedRetainedBytes() == 200);
    REQUIRE(cache.erase(state->identity(), 101));
    auto uncached_current = publish(state, new_geometry);
    CHECK_FALSE(cache.retain(state, 100, old_packet, stale_resources, 2));
    CHECK(state->publication().get() == uncached_current.get());
    CHECK_FALSE(state->dirty());
    CHECK(cache.entryCount() == 0);
}

TEST_CASE("Conflicting duplicate native capacity is rejected before resource deduplication",
          "[native-source-working-set][publication-edge]")
{
    ALNativeSourceWorkingSet cache;
    auto state = std::make_shared<State>();
    auto geometry = std::make_shared<int>(1);
    auto packet = publish(state, geometry);
    const std::array<ALNativeSourceWorkingSet::Resource, 2> resources{{{geometry, 100}, {geometry, 200}}};
    CHECK_FALSE(cache.retain(state, 1, state->publication(), resources, 1));
    CHECK_FALSE(state->publication());
    CHECK(cache.entryCount() == 0);
    CHECK(cache.estimatedRetainedBytes() == 0);
    CHECK(packet->geometry.get() == geometry.get());
}
