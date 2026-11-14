/**
 * @file alnativepublication_test.cpp
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "alnativepublication.h"
#include <catch2/catch_test_macros.hpp>
#include <functional>

namespace
{
struct Callback final : ALNativePublication::Observer
{
    std::function<void(ALNativePublication::Phase)> callback;
    unsigned calls = 0;
    void nativePublicationChanged(ALNativePublication::Phase phase) noexcept override
    {
        ++calls;
        if (callback) callback(phase);
    }
};
}

TEST_CASE("Native producer write releases only publisher leases before the write decision",
          "[native-publication][generation-leases]")
{
    ALNativePublication::Signal producer;
    auto state = std::make_shared<ALNativePublication::State>();
    producer.observe(state);
    auto backing = std::make_shared<int>(17);
    CHECK(state->publish(state->revision(), backing));
    CHECK(backing.use_count() == 2);
    auto actual_reader = state->publication();
    CHECK(backing.use_count() == 3);
    const auto published = producer.publishedRevision();
    producer.willWrite();
    CHECK(state->dirty());
    CHECK_FALSE(state->publication());
    CHECK(producer.publishedRevision() == published);
    CHECK(backing.use_count() == 2);
    CHECK(*std::static_pointer_cast<const int>(actual_reader) == 17);
    actual_reader.reset();
    CHECK(backing.use_count() == 1);
    producer.didPublish();
    CHECK(producer.publishedRevision() == published + 1);
}

TEST_CASE("Native producer revisions reject publication made stale while sealing",
          "[native-publication]")
{
    ALNativePublication::Signal a, b;
    CHECK(a.identity() != b.identity());
    auto state = std::make_shared<ALNativePublication::State>();
    a.observe(state);
    const auto before = state->revision();
    a.didPublish();
    CHECK_FALSE(state->publish(before, std::make_shared<int>(1)));
    CHECK(state->dirty());
    CHECK(state->publish(state->revision(), std::make_shared<int>(2)));
    CHECK_FALSE(state->dirty());
    a.withdraw();
    CHECK(state->dirty());
    CHECK_FALSE(state->publication());
    CHECK(a.publishedRevision() == 2);
}

TEST_CASE("Native producer observers are weak deduplicated and safe to mutate in callbacks",
          "[native-publication]")
{
    ALNativePublication::Signal producer;
    auto first = std::make_shared<Callback>();
    auto removed = std::make_shared<Callback>();
    auto added = std::make_shared<Callback>();
    std::weak_ptr<Callback> weak_first = first;
    first->callback = [&](auto)
    {
        producer.forget(first);
        producer.forget(removed);
        removed.reset();
        producer.observe(added);
        first.reset();
    };
    producer.observe(first);
    producer.observe(first);
    producer.observe(removed);
    producer.willWrite();
    CHECK(weak_first.expired());
    CHECK(added->calls == 0);
    producer.didPublish();
    CHECK(added->calls == 1);
    producer.observe(added);
    producer.willWrite();
    CHECK(added->calls == 2);
    std::weak_ptr<Callback> weak_added = added;
    added.reset();
    CHECK(weak_added.expired());
    producer.withdraw();
}

TEST_CASE("Native source eviction preserves reader payload without producer mutation",
          "[native-publication][generation-leases]")
{
    ALNativePublication::State state;
    const auto revision = state.revision();
    CHECK_FALSE(state.publish(revision, {}));
    CHECK(state.publish(revision, std::make_shared<int>(23)));
    auto reader = state.publication();
    state.evict();
    CHECK(state.dirty());
    CHECK(state.revision() == revision);
    CHECK_FALSE(state.publication());
    CHECK(*std::static_pointer_cast<const int>(reader) == 23);
}

TEST_CASE("Reentrant old source destruction cannot acknowledge a stale native publication",
          "[native-publication][publication-edge]")
{
    auto state = std::make_shared<ALNativePublication::State>();
    std::weak_ptr<ALNativePublication::State> weak_state(state);
    auto successor = std::make_shared<int>(9);
    bool replace_after_invalidation = false;
    std::shared_ptr<const void> old(new int(1),
        [weak_state, successor, &replace_after_invalidation](const void* pointer)
        {
            delete static_cast<const int*>(pointer);
            if (auto owner = weak_state.lock())
            {
                owner->invalidate();
                if (replace_after_invalidation)
                    owner->publish(owner->revision(), successor);
            }
        });
    REQUIRE(state->publish(state->revision(), old));
    old.reset();
    const auto sealed_revision = state->revision();
    SECTION("invalidation keeps the producer dirty")
    {
        CHECK_FALSE(state->publish(sealed_revision, std::make_shared<int>(2)));
        CHECK(state->revision() != sealed_revision);
        CHECK(state->dirty());
        CHECK_FALSE(state->publication());
    }
    SECTION("a reentrant replacement is preserved rather than overwritten")
    {
        replace_after_invalidation = true;
        CHECK_FALSE(state->publish(sealed_revision, std::make_shared<int>(2)));
        CHECK(state->revision() != sealed_revision);
        CHECK_FALSE(state->dirty());
        CHECK(state->publication().get() == successor.get());
    }
}
