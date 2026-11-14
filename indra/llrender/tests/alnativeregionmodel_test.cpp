/**
 * @file alnativeregionmodel_test.cpp
 * @brief Actual native region transform publisher with typed origin inputs.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "llmath.h"
#include "../../newview/alnativeregionmodel.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstring>
#include <functional>
#include <memory>
#include <type_traits>

static_assert(std::is_trivially_copyable_v<ALNativeRegionModel::Snapshot>);
static_assert(std::is_trivially_destructible_v<ALNativeRegionModel::Snapshot>);

namespace
{
struct ModelObserver final : ALNativePublication::Observer
{
    std::function<void(ALNativePublication::Phase)> callback;
    std::array<unsigned, 3> phases{};
    void nativePublicationChanged(ALNativePublication::Phase phase) noexcept override
    {
        ++phases[static_cast<std::size_t>(phase)];
        if (callback) callback(phase);
    }
};

// LLViewerRegion's actual helper is tested directly. getOriginAgent() and
// the caller's region/world lifecycle are typed frontier inputs; no test
// claims to instantiate networking, the region cache or a live SL world.
struct ModelOwner
{
    ALNativeRegionModel native;
    LLMatrix4a authoritative;
    bool update(const LLVector3& origin) { return native.update(authoritative, origin); }
    ~ModelOwner() { native.withdraw(); }
};

bool sameMatrix(const ALNativeRegionModel::Snapshot& snapshot, const LLMatrix4a& matrix)
{
    return std::memcmp(snapshot.model.data(), matrix.getF32ptr(), sizeof(snapshot.model)) == 0;
}

LLMatrix4a originalRegionMatrix(const LLVector3& origin)
{
    // Original/current pre-publication region invariant: exactly these two
    // operations, without GPU recomposition or changed coordinate arithmetic.
    LLMatrix4a matrix;
    matrix.setIdentity();
    matrix.setTranslation(origin);
    return matrix;
}
}

TEST_CASE("Native region models preserve exact matrix bits and skip unchanged epochs",
          "[native-region-model][native-publication]")
{
    ModelOwner owner;
    CHECK_FALSE(owner.native.snapshot());
    const LLVector3 first(1.25f, -512.5f, 13.75f);
    REQUIRE(owner.update(first));
    const auto published = owner.native.snapshot();
    REQUIRE(published);CHECK(published.revision == 1);
    CHECK(sameMatrix(published, owner.authoritative));
    CHECK(sameMatrix(published, originalRegionMatrix(first)));
    auto observer = std::make_shared<ModelObserver>();
    owner.native.publication().observe(observer);
    const auto mutation = owner.native.publication().mutationRevision();
    const auto generation = owner.native.publication().publishedRevision();
    REQUIRE(owner.update(first));
    CHECK(owner.native.snapshot().identity == published.identity);
    CHECK(owner.native.snapshot().revision == published.revision);
    CHECK(owner.native.publication().mutationRevision() == mutation);
    CHECK(owner.native.publication().publishedRevision() == generation);
    CHECK((observer->phases == std::array<unsigned, 3>{0, 0, 0}));

    const LLVector3 rebased(-254.75f, 1023.5f, -6.25f);
    REQUIRE(owner.update(rebased));
    const auto current = owner.native.snapshot();
    REQUIRE(current);CHECK(current.identity == published.identity);
    CHECK(current.revision == published.revision + 1);
    CHECK(sameMatrix(current, originalRegionMatrix(rebased)));
    CHECK(sameMatrix(current, owner.authoritative));
    CHECK(sameMatrix(published, originalRegionMatrix(first))); // copied older view is immutable.
    CHECK((observer->phases == std::array<unsigned, 3>{1, 1, 0}));
    const std::weak_ptr<ModelObserver> weak_observer = observer;observer.reset();
    CHECK(weak_observer.expired()); // the live model signal owns only a weak subscription.
}

TEST_CASE("Native region prewrite withdraws availability before authoritative replacement",
          "[native-region-model][native-publication]")
{
    ModelOwner owner;const LLVector3 first(256.f, -128.f, 8.f), next(-256.f, 128.f, -8.f);
    REQUIRE(owner.update(first));const auto prior = owner.native.snapshot();
    auto observer = std::make_shared<ModelObserver>();
    bool unavailable_before_write = false, completed_after_write = false;
    observer->callback = [&](auto phase)
    {
        if (phase == ALNativePublication::Phase::will_write)
            unavailable_before_write = !owner.native.snapshot() && sameMatrix(prior, owner.authoritative);
        if (phase == ALNativePublication::Phase::published)
        {
            const auto current = owner.native.snapshot();
            completed_after_write = current && current.revision == prior.revision + 1 &&
                sameMatrix(current, owner.authoritative) && sameMatrix(current, originalRegionMatrix(next));
        }
    };
    owner.native.publication().observe(observer);
    owner.native.publication().observe(observer);
    REQUIRE(owner.update(next));CHECK(unavailable_before_write);CHECK(completed_after_write);
    CHECK((observer->phases == std::array<unsigned, 3>{1, 1, 0}));
}

TEST_CASE("Distinct region owners and replacement owners cannot alias native model identity",
          "[native-region-model][native-publication]")
{
    const LLVector3 origin(0.f, 0.f, 0.f);ModelOwner first, second;
    REQUIRE(first.update(origin));REQUIRE(second.update(origin));
    const auto a = first.native.snapshot(), b = second.native.snapshot();
    CHECK(a.identity != b.identity);CHECK(a.model == b.model);
    CHECK(a.revision == b.revision);
    ALNativeRegionModel::Snapshot retained;
    auto observer = std::make_shared<ModelObserver>();
    {
        auto ended = std::make_unique<ModelOwner>();REQUIRE(ended->update(LLVector3(1.f, 2.f, 3.f)));
        retained = ended->native.snapshot();
        ended->native.publication().observe(observer);
        ended->native.withdraw();CHECK_FALSE(ended->native.snapshot());
        CHECK_FALSE(ended->update(origin));
        CHECK(observer->phases[static_cast<std::size_t>(ALNativePublication::Phase::withdrawn)] == 1);
    }
    // Explicit region withdrawal plus both destructor fallbacks notify once.
    CHECK(observer->phases[static_cast<std::size_t>(ALNativePublication::Phase::withdrawn)] == 1);
    CHECK(sameMatrix(retained, originalRegionMatrix(LLVector3(1.f, 2.f, 3.f))));
    auto replacement = std::make_unique<ModelOwner>();REQUIRE(replacement->update(origin));
    CHECK(replacement->native.snapshot().identity != retained.identity);
    CHECK(replacement->native.snapshot().revision == 1);
}

TEST_CASE("Native region destruction withdraws before member teardown and snapshots own no observers",
          "[native-region-model][native-publication]")
{
    auto observer = std::make_shared<ModelObserver>();
    ALNativeRegionModel::Snapshot retained;
    bool unavailable_on_destruction = false;
    {
        auto owner = std::make_unique<ModelOwner>();REQUIRE(owner->update(LLVector3(2.f, 4.f, 8.f)));
        retained = owner->native.snapshot();auto* live = owner.get();
        observer->callback = [&, live](auto phase)
        {
            if (phase == ALNativePublication::Phase::withdrawn)
                unavailable_on_destruction = !live->native.snapshot();
        };
        owner->native.publication().observe(observer);
    }
    CHECK(unavailable_on_destruction);
    CHECK(observer->phases[static_cast<std::size_t>(ALNativePublication::Phase::withdrawn)] == 1);
    const std::weak_ptr<ModelObserver> weak = observer;observer.reset();
    CHECK(weak.expired()); // copied model values retain neither owner nor subscription.
    CHECK(sameMatrix(retained, originalRegionMatrix(LLVector3(2.f, 4.f, 8.f))));
}

TEST_CASE("Reentrant native region publication cannot overwrite a newer model or retirement",
          "[native-region-model][native-publication][publication-edge]")
{
    ModelOwner owner;REQUIRE(owner.update(LLVector3(0.f, 0.f, 0.f)));
    const auto initial = owner.native.snapshot();auto observer = std::make_shared<ModelObserver>();
    bool armed = true, nested_success = false;
    SECTION("a newer prewrite model wins")
    {
        observer->callback = [&](auto phase)
        {
            if (armed && phase == ALNativePublication::Phase::will_write)
            {armed = false;nested_success = owner.update(LLVector3(9.f, 8.f, 7.f));}
        };
        owner.native.publication().observe(observer);
        CHECK_FALSE(owner.update(LLVector3(1.f, 2.f, 3.f)));CHECK(nested_success);
        const auto current = owner.native.snapshot();REQUIRE(current);
        CHECK(current.identity == initial.identity);CHECK(current.revision == initial.revision + 1);
        CHECK(sameMatrix(current, originalRegionMatrix(LLVector3(9.f, 8.f, 7.f))));
        CHECK(sameMatrix(current, owner.authoritative));
    }
    SECTION("retirement in prewrite rejects the pending authoritative write")
    {
        observer->callback = [&](auto phase)
        {
            if (armed && phase == ALNativePublication::Phase::will_write)
            {armed = false;owner.native.withdraw();}
        };
        owner.native.publication().observe(observer);
        CHECK_FALSE(owner.update(LLVector3(1.f, 2.f, 3.f)));
        CHECK_FALSE(owner.native.snapshot());CHECK(sameMatrix(initial, owner.authoritative));
    }
    SECTION("a newer published callback model wins")
    {
        observer->callback = [&](auto phase)
        {
            if (armed && phase == ALNativePublication::Phase::published)
            {armed = false;nested_success = owner.update(LLVector3(-9.f, -8.f, -7.f));}
        };
        owner.native.publication().observe(observer);
        CHECK_FALSE(owner.update(LLVector3(1.f, 2.f, 3.f)));CHECK(nested_success);
        const auto current = owner.native.snapshot();REQUIRE(current);
        CHECK(current.revision == initial.revision + 2);
        CHECK(sameMatrix(current, originalRegionMatrix(LLVector3(-9.f, -8.f, -7.f))));
        CHECK(sameMatrix(current, owner.authoritative));
    }
}
