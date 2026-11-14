/**
 * @file alnativesourceworkingset_test.cpp
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "alnativesourceworkingset.h"

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <exception>
#include <functional>

namespace
{
struct Source
{
    std::shared_ptr<const void> geometry;
    std::shared_ptr<const void> image;
};
using State = ALNativePublication::State;
struct OnWithdrawal final : ALNativePublication::Observer
{
    std::function<void()> action;
    std::exception_ptr error;
    bool called = false;
    void nativePublicationChanged(ALNativePublication::Phase phase) noexcept override
    {
        if (phase != ALNativePublication::Phase::withdrawn || called) return;
        called = true;
        try { action(); }
        catch (...) { error = std::current_exception(); }
    }
};
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

TEST_CASE("Native source admission cannot relabel a callback replacement during trim or pressure",
          "[native-source-working-set][publication-reentrancy]")
{
    for (bool age_trim : {false, true})
    {
        CAPTURE(age_trim);
        ALNativeSourceWorkingSet cache(2, 1024, age_trim ? 2 : 100);
        auto victim = std::make_shared<State>(), other = std::make_shared<State>(), incoming = std::make_shared<State>();
        auto a = std::make_shared<int>(1), b = std::make_shared<int>(2);
        auto old_geometry = std::make_shared<int>(3), new_geometry = std::make_shared<int>(4);
        auto victim_packet = publish(victim, a), other_packet = publish(other, b);
        const std::array<ALNativeSourceWorkingSet::Resource, 1> ra{{{a, 100}}}, rb{{{b, 120}}};
        const std::array<ALNativeSourceWorkingSet::Resource, 1> old_resources{{{old_geometry, 80}}}, new_resources{{{new_geometry, 320}}};
        REQUIRE(cache.retain(victim, 1, victim->publication(), ra, 1));
        REQUIRE(cache.retain(other, 2, other->publication(), rb, 1));
        auto old_packet = publish(incoming, old_geometry);
        std::shared_ptr<const Source> replacement;
        const std::uint32_t frame = age_trim ? 4 : 2;
        auto observer = std::make_shared<OnWithdrawal>();
        observer->action = [&]
        {
            replacement = publish(incoming, new_geometry);
            REQUIRE(cache.retain(incoming, 51, incoming->publication(), new_resources, frame));
        };
        victim->observe(observer);
        // Deliberately pass State's shared_ptr member by reference: replacement
        // must not turn the old caller resources/serial into a new admission.
        CHECK_FALSE(cache.retain(incoming, 50, incoming->publication(), old_resources, frame));
        REQUIRE(observer->called); REQUIRE_FALSE(observer->error);
        REQUIRE(replacement);
        CHECK(incoming->publication().get() == replacement.get());
        CHECK_FALSE(incoming->dirty());
        CHECK(cache.touch(incoming->identity(), 51, frame));
        CHECK_FALSE(cache.touch(incoming->identity(), 50, frame));
        CHECK_FALSE(cache.erase(incoming->identity(), 50));
        CHECK(cache.entryCount() == (age_trim ? 1 : 2));
        CHECK(cache.estimatedRetainedBytes() == (age_trim ? 320 : 440));
        if (!age_trim) CHECK(cache.touch(other->identity(), 2, frame));
        CHECK(old_packet->geometry.get() == old_geometry.get());
        CHECK(victim_packet->geometry.get() == a.get());
        CHECK(other_packet->geometry.get() == b.get());
        REQUIRE(cache.erase(incoming->identity(), 51));
        CHECK(cache.estimatedRetainedBytes() == (age_trim ? 0 : 120));
        CHECK(replacement->geometry.get() == new_geometry.get());
    }
}

TEST_CASE("Native source eviction callbacks can invalidate an incoming publication without admitting it",
          "[native-source-working-set][publication-reentrancy]")
{
    ALNativeSourceWorkingSet cache(1, 1024, 100);
    auto victim = std::make_shared<State>(), incoming = std::make_shared<State>();
    auto a = std::make_shared<int>(1), b = std::make_shared<int>(2);
    publish(victim, a); auto reader = publish(incoming, b);
    const std::array<ALNativeSourceWorkingSet::Resource, 1> ra{{{a, 100}}}, rb{{{b, 200}}};
    REQUIRE(cache.retain(victim, 1, victim->publication(), ra, 1));
    auto observer = std::make_shared<OnWithdrawal>();
    observer->action = [&] { incoming->invalidate(); };
    victim->observe(observer);
    CHECK_FALSE(cache.retain(incoming, 2, incoming->publication(), rb, 2));
    REQUIRE(observer->called); REQUIRE_FALSE(observer->error);
    CHECK(incoming->dirty()); CHECK_FALSE(incoming->publication());
    CHECK(cache.entryCount() == 0); CHECK(cache.estimatedRetainedBytes() == 0);
    CHECK_FALSE(cache.touch(incoming->identity(), 2, 2));
    CHECK(reader->geometry.get() == b.get());
}

TEST_CASE("A callback that already retained the exact native request is acknowledged once",
          "[native-source-working-set][publication-reentrancy]")
{
    ALNativeSourceWorkingSet cache(1, 1024, 100);
    auto victim = std::make_shared<State>(), incoming = std::make_shared<State>();
    auto a = std::make_shared<int>(1), b = std::make_shared<int>(2);
    publish(victim, a); auto reader = publish(incoming, b);
    const std::array<ALNativeSourceWorkingSet::Resource, 1> ra{{{a, 100}}}, rb{{{b, 200}}};
    REQUIRE(cache.retain(victim, 1, victim->publication(), ra, 1));
    auto observer = std::make_shared<OnWithdrawal>();
    observer->action = [&] { REQUIRE(cache.retain(incoming, 2, incoming->publication(), rb, 2)); };
    victim->observe(observer);
    REQUIRE(cache.retain(incoming, 2, incoming->publication(), rb, 2));
    REQUIRE(observer->called); REQUIRE_FALSE(observer->error);
    CHECK(incoming->publication().get() == reader.get());
    CHECK_FALSE(incoming->dirty());
    CHECK(cache.entryCount() == 1); CHECK(cache.estimatedRetainedBytes() == 200);
    CHECK(cache.touch(incoming->identity(), 2, 2));
    REQUIRE(cache.erase(incoming->identity(), 2));
    CHECK(cache.entryCount() == 0); CHECK(cache.estimatedRetainedBytes() == 0);
}

TEST_CASE("Native source clear preserves a newer tuple admitted by an eviction observer",
          "[native-source-working-set][publication-reentrancy]")
{
    ALNativeSourceWorkingSet cache(4, 1024, 100);
    auto a = std::make_shared<State>(), b = std::make_shared<State>();
    auto ga = std::make_shared<int>(1), gb = std::make_shared<int>(2), fresh = std::make_shared<int>(3);
    auto reader_a = publish(a, ga), reader_b = publish(b, gb);
    const std::array<ALNativeSourceWorkingSet::Resource, 1> ra{{{ga, 100}}}, rb{{{gb, 120}}}, rc{{{fresh, 320}}};
    REQUIRE(cache.retain(a, 1, a->publication(), ra, 1));
    REQUIRE(cache.retain(b, 2, b->publication(), rb, 1));
    std::shared_ptr<const Source> replacement;
    auto observer = std::make_shared<OnWithdrawal>();
    observer->action = [&]
    {
        replacement = publish(b, fresh);
        REQUIRE(cache.retain(b, 3, b->publication(), rc, 2));
    };
    a->observe(observer);
    cache.clear();
    REQUIRE(observer->called); REQUIRE_FALSE(observer->error);
    CHECK_FALSE(a->publication());
    REQUIRE(replacement); CHECK(b->publication().get() == replacement.get());
    CHECK_FALSE(b->dirty());
    CHECK(cache.entryCount() == 1); CHECK(cache.estimatedRetainedBytes() == 320);
    CHECK(cache.touch(b->identity(), 3, 2));
    CHECK_FALSE(cache.erase(b->identity(), 2));
    CHECK(reader_a->geometry.get() == ga.get()); CHECK(reader_b->geometry.get() == gb.get());
    cache.clear();
    CHECK_FALSE(b->publication());
    CHECK(cache.entryCount() == 0); CHECK(cache.estimatedRetainedBytes() == 0);
}
