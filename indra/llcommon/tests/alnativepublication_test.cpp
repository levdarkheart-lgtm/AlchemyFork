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

struct DerivedPayload
{
    explicit DerivedPayload(std::shared_ptr<const void> value) : source(std::move(value)) {}
    std::shared_ptr<const void> source;
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

TEST_CASE("Derived native publisher caches release before the backing write decision",
          "[native-publication][derived-publication][generation-leases]")
{
    ALNativePublication::Signal backing_producer;
    auto source = std::make_shared<ALNativePublication::State>();
    auto group = std::make_shared<ALNativePublication::State>();
    backing_producer.observe(source);
    source->observe(group);
    source->observe(group); // One dependency edge, not two invalidations.
    auto backing = std::make_shared<int>(17);
    REQUIRE(source->publish(source->revision(), backing));
    REQUIRE(group->publish(group->revision(),
        std::make_shared<DerivedPayload>(source->publication())));
    CHECK(backing.use_count() == 3);

    std::shared_ptr<const void> actual_reader;
    SECTION("publisher caches alone do not force copy on write") {}
    SECTION("an actual queued reader still forces copy on write")
    {
        actual_reader = group->publication();
    }
    const bool reader_retained = bool(actual_reader);
    const auto source_revision = source->revision();
    const auto group_revision = group->revision();
    const auto backing_revision = backing_producer.publishedRevision();
    bool saw_withdrawn_source_and_group = false;
    auto phase_seen = ALNativePublication::Phase::published;
    long leases_during_notification = 0;
    auto audit = std::make_shared<Callback>();
    audit->callback = [&](auto phase)
    {
        phase_seen = phase;
        saw_withdrawn_source_and_group = source->dirty() && !source->publication() &&
            group->dirty() && !group->publication();
        // The source's old publisher lease remains alive until every derived
        // observer has run, even though its public slot is already withdrawn.
        leases_during_notification = backing.use_count();
    };
    source->observe(audit);
    backing_producer.willWrite();
    CHECK(audit->calls == 1);
    CHECK(phase_seen == ALNativePublication::Phase::will_write);
    CHECK(saw_withdrawn_source_and_group);
    CHECK(leases_during_notification == (reader_retained ? 3 : 2));
    CHECK(source->revision() == source_revision + 1);
    CHECK(group->revision() == group_revision + 1);
    CHECK(backing_producer.publishedRevision() == backing_revision);
    CHECK(backing.use_count() == (reader_retained ? 2 : 1));
    CHECK((backing.use_count() > 1) == reader_retained);
    if (!reader_retained)
    {
        *backing = 29; // The now-unleased backing can be written in place.
        CHECK(*backing == 29);
    }
    if (actual_reader)
    {
        auto replacement = std::make_shared<int>(*backing);
        *replacement = 29;
        CHECK(*replacement == 29);
        auto published_group = std::static_pointer_cast<const DerivedPayload>(actual_reader);
        CHECK(*std::static_pointer_cast<const int>(published_group->source) == 17);
        actual_reader.reset();
        published_group.reset();
    }
    CHECK(backing.use_count() == 1);
}

TEST_CASE("Source eviction withdraws derived caches without changing the producer revision",
          "[native-publication][derived-publication][generation-leases]")
{
    auto source = std::make_shared<ALNativePublication::State>();
    auto group = std::make_shared<ALNativePublication::State>();
    source->observe(group);
    auto backing = std::make_shared<int>(23);
    REQUIRE(source->publish(source->revision(), backing));
    REQUIRE(group->publish(group->revision(),
        std::make_shared<DerivedPayload>(source->publication())));
    auto actual_reader = group->publication();
    const auto source_revision = source->revision();
    const auto group_revision = group->revision();
    auto phase_seen = ALNativePublication::Phase::published;
    ALNativePublication::Revision revision_during_notification = 0;
    bool saw_withdrawn_group = false;
    long leases_during_notification = 0;
    auto audit = std::make_shared<Callback>();
    audit->callback = [&](auto phase)
    {
        phase_seen = phase;
        revision_during_notification = source->revision();
        saw_withdrawn_group = group->dirty() && !group->publication();
        leases_during_notification = backing.use_count();
    };
    source->observe(audit);
    source->evict();
    CHECK(audit->calls == 1);
    CHECK(phase_seen == ALNativePublication::Phase::withdrawn);
    CHECK(revision_during_notification == source_revision);
    CHECK(saw_withdrawn_group);
    CHECK(leases_during_notification == 3); // owner, old source lease, reader
    CHECK(source->dirty());
    CHECK_FALSE(source->publication());
    CHECK(source->revision() == source_revision);
    CHECK(group->revision() == group_revision + 1);
    CHECK(backing.use_count() == 2);
    CHECK(*std::static_pointer_cast<const int>(
        std::static_pointer_cast<const DerivedPayload>(actual_reader)->source) == 23);
    actual_reader.reset();
    CHECK(backing.use_count() == 1);
}

TEST_CASE("Same revision source extension preserves valid derived publications",
          "[native-publication][derived-publication]")
{
    struct SourcePayload
    {
        std::shared_ptr<int> backing;
        unsigned variants;
    };
    auto source = std::make_shared<ALNativePublication::State>();
    auto group = std::make_shared<ALNativePublication::State>();
    auto audit = std::make_shared<Callback>();
    source->observe(group);
    source->observe(audit);
    auto backing = std::make_shared<int>(31);
    REQUIRE(source->publish(source->revision(),
        std::make_shared<SourcePayload>(SourcePayload{backing, 1})));
    REQUIRE(group->publish(group->revision(),
        std::make_shared<DerivedPayload>(source->publication())));
    const auto source_revision = source->revision();
    const auto group_revision = group->revision();
    const auto old_group = group->publication();
    REQUIRE(source->publish(source_revision,
        std::make_shared<SourcePayload>(SourcePayload{backing, 3})));
    CHECK(audit->calls == 0);
    CHECK(source->revision() == source_revision);
    CHECK(group->revision() == group_revision);
    CHECK_FALSE(group->dirty());
    CHECK(group->publication() == old_group);
    const auto old_source = std::static_pointer_cast<const SourcePayload>(
        std::static_pointer_cast<const DerivedPayload>(old_group)->source);
    CHECK(old_source->variants == 1);
    CHECK(*old_source->backing == 31);
    CHECK(std::static_pointer_cast<const SourcePayload>(source->publication())->variants == 3);
}

TEST_CASE("Derived invalidation preserves a replacement installed by old payload destruction",
          "[native-publication][derived-publication][publication-edge]")
{
    auto source = std::make_shared<ALNativePublication::State>();
    auto group = std::make_shared<ALNativePublication::State>();
    source->observe(group);
    std::weak_ptr<ALNativePublication::State> weak_source(source);
    auto replacement = std::make_shared<int>(41);
    bool notification_complete = false;
    bool destroyed_after_notification = false;
    bool replacement_installed = false;
    unsigned old_destroyed = 0;
    std::shared_ptr<const void> old(new int(37),
        [&, weak_source, replacement](const void* pointer)
        {
            delete static_cast<const int*>(pointer);
            ++old_destroyed;
            destroyed_after_notification = notification_complete;
            if (auto owner = weak_source.lock())
                replacement_installed = owner->publish(owner->revision(), replacement);
        });
    REQUIRE(source->publish(source->revision(), old));
    old.reset();
    REQUIRE(group->publish(group->revision(),
        std::make_shared<DerivedPayload>(source->publication())));
    bool saw_withdrawn_group = false;
    unsigned destroyed_during_notification = 0;
    auto audit = std::make_shared<Callback>();
    audit->callback = [&](auto)
    {
        saw_withdrawn_group = group->dirty() && !group->publication();
        destroyed_during_notification = old_destroyed;
        notification_complete = true;
    };
    source->observe(audit);
    const auto source_revision = source->revision();
    SECTION("producer mutation")
    {
        source->invalidate();
        CHECK(source->revision() == source_revision + 1);
    }
    SECTION("working set eviction")
    {
        source->evict();
        CHECK(source->revision() == source_revision);
    }
    CHECK(audit->calls == 1);
    CHECK(saw_withdrawn_group);
    CHECK(destroyed_during_notification == 0);
    CHECK(old_destroyed == 1);
    CHECK(destroyed_after_notification);
    CHECK(replacement_installed);
    CHECK_FALSE(source->dirty());
    CHECK(source->publication().get() == replacement.get());
    CHECK(group->dirty());
    CHECK_FALSE(group->publication());
}

TEST_CASE("Derived invalidation callbacks cannot be overwritten after notification",
          "[native-publication][derived-publication][publication-edge]")
{
    auto source = std::make_shared<ALNativePublication::State>();
    auto replacement = std::make_shared<int>(47);
    REQUIRE(source->publish(source->revision(), std::make_shared<int>(43)));
    bool saw_withdrawn_source = false;
    bool replacement_installed = false;
    auto audit = std::make_shared<Callback>();
    audit->callback = [&](auto)
    {
        saw_withdrawn_source = source->dirty() && !source->publication();
        replacement_installed = source->publish(source->revision(), replacement);
    };
    source->observe(audit);
    source->invalidate();
    CHECK(audit->calls == 1);
    CHECK(saw_withdrawn_source);
    CHECK(replacement_installed);
    CHECK_FALSE(source->dirty());
    CHECK(source->publication().get() == replacement.get());
}

TEST_CASE("Derived publisher subscriptions are weak deduplicated and callback safe",
          "[native-publication][derived-publication]")
{
    ALNativePublication::State source;
    source.observe({});
    auto first = std::make_shared<Callback>();
    auto added = std::make_shared<Callback>();
    std::weak_ptr<Callback> weak_first(first), weak_added(added);
    auto phase_seen = ALNativePublication::Phase::published;
    first->callback = [&](auto phase)
    {
        phase_seen = phase;
        source.observe(added);
        first.reset();
    };
    source.observe(first);
    source.observe(std::shared_ptr<ALNativePublication::Observer>(first, first.get()));
    source.invalidate();
    CHECK(phase_seen == ALNativePublication::Phase::will_write);
    CHECK(weak_first.expired());
    CHECK(added->calls == 0); // A new edge does not join the current event.
    source.observe(added);
    source.evict();
    CHECK(added->calls == 1);
    source.invalidate();
    CHECK(added->calls == 2);
    added.reset();
    CHECK(weak_added.expired());
    source.evict(); // Expired derived publishers do not keep an owner cycle.
}
