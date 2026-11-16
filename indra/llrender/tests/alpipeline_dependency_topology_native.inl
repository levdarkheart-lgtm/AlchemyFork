/**
 * @file alpipeline_dependency_topology_native.inl
 * @brief Production dependency visitor with real LLVB/LLImageGL signals.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Explicit viewer facade/material inputs remain typed fixture values. The
 * visitor, relay, exact topology matching and subscription transaction below
 * are the production implementation, not Inputs::LLDrawInfo's old stub.
 */
namespace DependencyTopology
{
using LLViewerTexture = Inputs::LLViewerTexture;
using LLViewerFetchedTexture = Inputs::LLViewerFetchedTexture;
using LLGLTFMaterial = Inputs::LLGLTFMaterial;
using LLFetchedGLTFMaterial = Inputs::LLFetchedGLTFMaterial;
#include "../../newview/llspatialpartition_native_dependencies.inl"
using Relay = ALNativeDrawSourceDependencies::Relay;

struct Owner : Inputs::LLDrawInfo
{
    mutable std::shared_ptr<Relay> dependencies;
    mutable U64 dependency_revision = 0;
    U64 layout_revision = 1;
    void observeShared() const
    {
        const auto& current = metalSourcePublication();
        if (dependencies && dependency_revision == current->revision()) return;
        const auto observed_revision = current->revision();
        auto next = ALNativeDrawSourceDependencies::refresh(*this, current, dependencies);
        dependencies = std::move(next);
        dependency_revision = observed_revision;
    }
};
struct Image
{
    LLPointer<LLImageGL> native;
    LLViewerFetchedTexture facade;
    Image()
    {
        // Dependency topology uses the actual wrapper's Signal, independently
        // of whether that wrapper currently has published texture storage.
        native = new LLImageGL(false);
        facade.image = native;
    }
};
struct SubscriptionFault
{
    inline static std::size_t attempted = 0;
    inline static std::size_t fail_at = std::numeric_limits<std::size_t>::max();
    void (*previous)(std::size_t) = ALNativeDrawSourceDependencies::subscriptionAttemptForTesting;
    SubscriptionFault(std::size_t fail = std::numeric_limits<std::size_t>::max())
    {
        attempted = 0; fail_at = fail;
        ALNativeDrawSourceDependencies::subscriptionAttemptForTesting = [](std::size_t at)
        {
            ++attempted;
            if (at == fail_at) throw std::bad_alloc();
        };
    }
    ~SubscriptionFault()
    {
        ALNativeDrawSourceDependencies::subscriptionAttemptForTesting = previous;
        fail_at = std::numeric_limits<std::size_t>::max();
    }
};
}

TEST_CASE("Production draw dependency visitor covers all roles and reuses unchanged content topology",
          "[almetal][pipeline-dependency-topology]")
{
    Publication fixture;
    DependencyTopology::Owner info;
    info.mVertexBuffer = fixture.buffer(false);
    info.mTexture.value = &fixture.image;
    info.mNormalMap.value = &fixture.image;
    info.mSpecularMap.value = &fixture.image;
    info.mTextureList = {{&fixture.image}, {&fixture.image}};
    info.mMaterialSlotList.resize(2);
    for (auto& slot : info.mMaterialSlotList)
        slot.mDiffuse.value = slot.mNormalMap.value = slot.mSpecularMap.value = &fixture.image;
    Inputs::LLFetchedGLTFMaterial material;
    material.textures.fill(&fixture.image);
    info.mGLTFMaterial.value = &material;
    info.mGLTFMaterialList = {{&material}, {&material}};
    const auto before = DependencyTopology::ALNativeDrawSourceDependencies::statistics;
    {
        DependencyTopology::SubscriptionFault count_subscriptions;
        info.observeShared();
        CHECK(DependencyTopology::SubscriptionFault::attempted == 3);
    }
    REQUIRE(info.dependencies);
    CHECK(info.dependencies->signalCount() == 30);
    auto relay = info.dependencies;
    const auto reads = fixture.image.image_reads;
    info.observeShared();
    CHECK(fixture.image.image_reads == reads); // Original same-revision fast return.
    CHECK(info.dependencies == relay);
    std::vector<ALNativePublication::Revision> identities;
    REQUIRE(DependencyTopology::ALNativeDrawSourceDependencies::visit(info,
        [&](ALNativePublication::Signal& signal) { identities.push_back(signal.identity()); return true; }));
    REQUIRE(identities.size() == 30);
    CHECK(identities.front() == info.mVertexBuffer->nativePublication().identity());
    CHECK(identities[12] == material.signal.identity());
    CHECK(identities[17] == material.signal.identity());
    CHECK(identities[22] == material.signal.identity());
    // Duplicated roles preserve the exact list but only one observer edge.
    auto revision = info.state->revision();
    fixture.white->nativePublication().didPublish();
    CHECK(info.state->revision() == revision + 1);
    {
        DependencyTopology::SubscriptionFault forbid_subscriptions(0);
        info.observeShared();
        CHECK(DependencyTopology::SubscriptionFault::attempted == 0);
    }
    CHECK(info.dependencies == relay);
    revision = info.state->revision();
    material.signal.willWrite();
    CHECK(info.state->revision() == revision + 1);
    info.observeShared();
    CHECK(info.dependencies == relay);
    revision = info.state->revision();
    info.mVertexBuffer->nativePublication().withdraw();
    CHECK(info.state->revision() == revision + 1);
    info.observeShared();
    CHECK(info.dependencies == relay);
    CHECK(info.layout_revision == 1);
    const auto dependency_revision = info.dependency_revision;
    info.state->evict();
    info.observeShared();
    CHECK(info.dependency_revision == dependency_revision); // Cache withdrawal is not a producer mutation.
    CHECK(info.dependencies == relay);
    const auto after = DependencyTopology::ALNativeDrawSourceDependencies::statistics;
    CHECK(after.refresh_calls - before.refresh_calls == 4);
    CHECK(after.topology_reuses - before.topology_reuses == 3);
    CHECK(after.relay_completions - before.relay_completions == 1);
    CHECK(after.actual_subscription_calls - before.actual_subscription_calls == 3);
    CHECK(after.failures == before.failures);
}

TEST_CASE("Production dependency topology sees native wrapper replacement removal and ordering",
          "[almetal][pipeline-dependency-topology]")
{
    Publication fixture;
    DependencyTopology::Image first, second;
    DependencyTopology::Owner info;
    info.mVertexBuffer = fixture.buffer(false);
    info.mTexture.value = &first.facade;
    info.observeShared();
    std::weak_ptr<DependencyTopology::Relay> old = info.dependencies;
    first.native->nativePublication().withdraw();
    // The facade/layout object remains the same, but its native Signal does not.
    first.facade.image = second.native;
    info.observeShared();
    CHECK(old.expired());
    CHECK(info.layout_revision == 1);
    auto revision = info.state->revision();
    first.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision); // The removed native producer no longer observes this source.
    second.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision + 1);
    info.observeShared();
    info.mTexture.value = nullptr;
    info.state->invalidate();
    old = info.dependencies;
    info.observeShared();
    CHECK(old.expired());
    revision = info.state->revision();
    second.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision);
    first.facade.image = first.native;
    info.mTextureList = {{&first.facade}, {&second.facade}};
    info.state->invalidate(); info.observeShared();
    old = info.dependencies;
    std::swap(info.mTextureList[0], info.mTextureList[1]);
    info.state->invalidate(); info.observeShared();
    CHECK(old.expired()); // Exact ordered identities, not an unordered/hash fingerprint.
    revision = info.state->revision();
    first.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision + 1);
}

TEST_CASE("Dependency subscription failure preserves the old relay until a complete replacement",
          "[almetal][pipeline-dependency-topology]")
{
    Publication fixture;
    DependencyTopology::Image first, second;
    DependencyTopology::Owner info;
    info.mVertexBuffer = fixture.buffer(false);
    info.mTexture.value = &first.facade;
    const auto before = DependencyTopology::ALNativeDrawSourceDependencies::statistics;
    info.observeShared();
    auto* original = info.dependencies.get();
    std::weak_ptr<DependencyTopology::Relay> old = info.dependencies;
    const auto cached = info.dependency_revision;
    info.mTexture.value = &second.facade;
    info.state->invalidate();
    {
        DependencyTopology::SubscriptionFault fail_after_two(2);
        REQUIRE_THROWS_AS(info.observeShared(), std::bad_alloc);
        CHECK(DependencyTopology::SubscriptionFault::attempted == 3);
    }
    CHECK(info.dependencies.get() == original);
    CHECK(info.dependency_revision == cached);
    CHECK_FALSE(old.expired());
    auto revision = info.state->revision();
    second.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision); // The partially subscribed fresh relay died on failure.
    first.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision + 1);
    info.observeShared();
    CHECK(old.expired());
    revision = info.state->revision();
    first.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision);
    second.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision + 1);
    const auto after = DependencyTopology::ALNativeDrawSourceDependencies::statistics;
    CHECK(after.refresh_calls - before.refresh_calls == 3);
    CHECK(after.topology_reuses == before.topology_reuses);
    CHECK(after.relay_completions - before.relay_completions == 2);
    CHECK(after.actual_subscription_calls - before.actual_subscription_calls == 8);
    CHECK(after.failures - before.failures == 1);
}

TEST_CASE("Repeated dependency roles retain exact order and multiplicity after subscription deduplication",
          "[almetal][pipeline-dependency-topology]")
{
    Publication fixture;
    DependencyTopology::Image first, second;
    DependencyTopology::Owner info;
    info.mVertexBuffer = fixture.buffer(false);
    info.mTextureList = {{&first.facade}, {&first.facade}, {&second.facade}};
    info.observeShared();
    REQUIRE(info.dependencies->signalCount() == 7);
    std::weak_ptr<DependencyTopology::Relay> old(info.dependencies);
    const auto before = DependencyTopology::ALNativeDrawSourceDependencies::statistics;
    // The unique Signals and the role count stay equal, but the full ordered
    // topology changes. A deduplicated topology fingerprint would miss this.
    std::swap(info.mTextureList[1], info.mTextureList[2]);
    info.state->invalidate();
    {
        DependencyTopology::SubscriptionFault count_subscriptions;
        info.observeShared();
        CHECK(DependencyTopology::SubscriptionFault::attempted == 4);
    }
    CHECK(old.expired());
    CHECK(info.dependencies->signalCount() == 7);
    old = info.dependencies;
    // Change multiplicity without changing the unique producers or role count.
    info.mTextureList[0].value = &second.facade;
    info.state->invalidate();
    info.observeShared();
    CHECK(old.expired());
    CHECK(info.dependencies->signalCount() == 7);
    auto revision = info.state->revision();
    first.native->nativePublication().willWrite();
    CHECK(info.state->revision() == revision + 1);
    second.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision + 2);
    const auto after = DependencyTopology::ALNativeDrawSourceDependencies::statistics;
    CHECK(after.relay_completions - before.relay_completions == 2);
    CHECK(after.actual_subscription_calls - before.actual_subscription_calls == 8);
    CHECK(after.topology_reuses == before.topology_reuses);
}

TEST_CASE("Repeated dependency roles preserve rollback before a later unique subscription",
          "[almetal][pipeline-dependency-topology]")
{
    Publication fixture;
    DependencyTopology::Image first, second, third;
    DependencyTopology::Owner info;
    info.mVertexBuffer = fixture.buffer(false);
    info.mTexture.value = &first.facade;
    info.observeShared();
    auto* original = info.dependencies.get();
    const auto cached = info.dependency_revision;
    info.mTexture.value = info.mNormalMap.value = info.mSpecularMap.value = &second.facade;
    info.mTextureList = {{&second.facade}, {&third.facade}};
    info.state->invalidate();
    const auto before = DependencyTopology::ALNativeDrawSourceDependencies::statistics;
    {
        // Vertex, second image, then third image at role five. The repeated
        // second-image roles keep their topology positions without callbacks.
        DependencyTopology::SubscriptionFault fail_on_third_unique(5);
        REQUIRE_THROWS_AS(info.observeShared(), std::bad_alloc);
        CHECK(DependencyTopology::SubscriptionFault::attempted == 3);
    }
    CHECK(info.dependencies.get() == original);
    CHECK(info.dependency_revision == cached);
    auto revision = info.state->revision();
    second.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision); // The partial new relay expired.
    first.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision + 1);
    std::weak_ptr<DependencyTopology::Relay> old(info.dependencies);
    {
        DependencyTopology::SubscriptionFault count_subscriptions;
        info.observeShared();
        CHECK(DependencyTopology::SubscriptionFault::attempted == 4);
    }
    CHECK(old.expired());
    CHECK(info.dependencies->signalCount() == 9);
    revision = info.state->revision();
    first.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision);
    second.native->nativePublication().didPublish();
    CHECK(info.state->revision() == revision + 1);
    third.native->nativePublication().willWrite();
    CHECK(info.state->revision() == revision + 2);
    const auto after = DependencyTopology::ALNativeDrawSourceDependencies::statistics;
    CHECK(after.actual_subscription_calls - before.actual_subscription_calls == 6);
    CHECK(after.failures - before.failures == 1);
}

TEST_CASE("Dependency reuse preserves independent readers and requires the exact State owner",
          "[almetal][pipeline-dependency-topology]")
{
    Publication fixture;
    DependencyTopology::Owner info;
    info.mVertexBuffer = fixture.buffer(false);
    info.mTexture.value = &fixture.image;
    info.observeShared();
    auto value = std::make_shared<int>(17);
    std::weak_ptr<int> weak_value(value);
    REQUIRE(info.state->publish(info.state->revision(), value));
    auto reader = info.state->publication(); value.reset();
    auto relay = info.dependencies;
    info.mVertexBuffer->nativePublication().willWrite();
    CHECK_FALSE(info.state->publication()); CHECK_FALSE(weak_value.expired());
    info.observeShared(); CHECK(info.dependencies == relay);
    CHECK(*std::static_pointer_cast<const int>(reader) == 17);
    reader.reset(); CHECK(weak_value.expired());
    // A relay must not be transferred to another State with equal revisions.
    auto other = std::make_shared<ALNativePublication::State>();
    while (other->revision() != info.state->revision()) other->invalidate();
    auto replacement = DependencyTopology::ALNativeDrawSourceDependencies::refresh(info, other, relay);
    CHECK(replacement != relay); CHECK(replacement->sameState(other));
    CHECK_FALSE(replacement->sameState(info.state));
    std::weak_ptr<ALNativePublication::State> weak_state(other);
    other.reset(); CHECK(weak_state.expired()); // Relays retain no State or viewer owner.
}

TEST_CASE("Dependency refresh does not acknowledge a newer producer epoch raised while subscribing",
          "[almetal][pipeline-dependency-topology]")
{
    Publication fixture;
    DependencyTopology::Image replacement;
    DependencyTopology::Owner info;
    info.mVertexBuffer = fixture.buffer(false);
    info.mTexture.value = &fixture.image;
    info.observeShared();
    info.mTexture.value = &replacement.facade;
    info.state->invalidate();
    const auto observed = info.state->revision();
    // The captureless hook uses one function-local owner pointer whose
    // lifetime this scope controls.
    static ALNativePublication::State* target = nullptr;
    struct Restore
    {
        void (*previous)(std::size_t);
        ALNativePublication::State*& target;
        ~Restore() { DependencyTopology::ALNativeDrawSourceDependencies::subscriptionAttemptForTesting = previous; target = nullptr; }
    } restore{DependencyTopology::ALNativeDrawSourceDependencies::subscriptionAttemptForTesting, target};
    target = info.state.get();
    DependencyTopology::ALNativeDrawSourceDependencies::subscriptionAttemptForTesting = [](std::size_t at)
    {
        if (at == 0 && target) { auto* source = std::exchange(target, nullptr); source->invalidate(); }
    };
    info.observeShared();
    CHECK(info.state->revision() == observed + 1);
    CHECK(info.dependency_revision == observed);
    auto relay = info.dependencies;
    const auto reads = replacement.facade.image_reads;
    info.observeShared();
    CHECK(replacement.facade.image_reads > reads);
    CHECK(info.dependency_revision == info.state->revision());
    CHECK(info.dependencies == relay);
}

TEST_CASE("Production dependency visitor observes distinct material image and fallback roles",
          "[almetal][pipeline-dependency-topology]")
{
    Publication fixture;
    std::vector<std::unique_ptr<DependencyTopology::Image>> images;
    for (unsigned at = 0; at < 29; ++at) images.push_back(std::make_unique<DependencyTopology::Image>());
    DependencyTopology::Owner info;
    info.mVertexBuffer = fixture.buffer(false);
    std::array<Inputs::LLFetchedGLTFMaterial, 3> materials;
    std::vector<ALNativePublication::Signal*> expected;
    expected.push_back(&info.mVertexBuffer->nativePublication());
    std::size_t at = 0;
    const auto image = [&]() -> Inputs::LLViewerFetchedTexture*
    {
        auto* result = &images[at++]->facade;
        expected.push_back(&result->image->nativePublication());
        return result;
    };
    info.mTexture.value = image(); info.mSpecularMap.value = image(); info.mNormalMap.value = image();
    info.mTextureList = {{image()}, {image()}};
    info.mMaterialSlotList.resize(2);
    for (auto& slot : info.mMaterialSlotList)
    {
        slot.mDiffuse.value = image(); slot.mNormalMap.value = image(); slot.mSpecularMap.value = image();
    }
    for (auto& material : materials)
    {
        expected.push_back(&material.signal);
        for (auto& texture : material.textures) texture = image();
    }
    info.mGLTFMaterial.value = &materials[0];
    info.mGLTFMaterialList = {{&materials[1]}, {&materials[2]}};
    struct RestoreFallbacks
    {
        Inputs::Ref<Inputs::LLViewerFetchedTexture> defaults = Inputs::LLViewerFetchedTexture::sDefaultImagep;
        Inputs::Ref<Inputs::LLViewerFetchedTexture> white = Inputs::LLViewerFetchedTexture::sWhiteImagep;
        Inputs::Ref<Inputs::LLViewerFetchedTexture> normal = Inputs::LLViewerFetchedTexture::sFlatNormalImagep;
        ~RestoreFallbacks()
        {
            Inputs::LLViewerFetchedTexture::sDefaultImagep = defaults;
            Inputs::LLViewerFetchedTexture::sWhiteImagep = white;
            Inputs::LLViewerFetchedTexture::sFlatNormalImagep = normal;
        }
    } restore;
    Inputs::LLViewerFetchedTexture::sDefaultImagep.value = image();
    Inputs::LLViewerFetchedTexture::sWhiteImagep.value = image();
    Inputs::LLViewerFetchedTexture::sFlatNormalImagep.value = image();
    REQUIRE(at == 26); REQUIRE(expected.size() == 30);
    std::vector<ALNativePublication::Revision> actual;
    REQUIRE(DependencyTopology::ALNativeDrawSourceDependencies::visit(info,
        [&](ALNativePublication::Signal& signal) { actual.push_back(signal.identity()); return true; }));
    REQUIRE(actual.size() == expected.size());
    std::unordered_set<ALNativePublication::Revision> unique;
    for (std::size_t row = 0; row < expected.size(); ++row)
    {
        CHECK(actual[row] == expected[row]->identity()); unique.insert(actual[row]);
    }
    CHECK(unique.size() == 30);
    const auto before = DependencyTopology::ALNativeDrawSourceDependencies::statistics;
    {
        DependencyTopology::SubscriptionFault count_subscriptions;
        info.observeShared();
        CHECK(DependencyTopology::SubscriptionFault::attempted == 30);
    }
    auto relay = info.dependencies;
    {
        DependencyTopology::SubscriptionFault forbid_subscriptions(0);
        for (auto* signal : expected)
        {
            auto revision = info.state->revision();
            signal->willWrite(); CHECK(info.state->revision() == revision + 1);
            info.observeShared(); CHECK(info.dependencies == relay);
            signal->didPublish(); CHECK(info.state->revision() == revision + 2);
            info.observeShared(); CHECK(info.dependencies == relay);
        }
        CHECK(DependencyTopology::SubscriptionFault::attempted == 0);
    }
    // Replacing each fallback changes its Signal identity even with no layout
    // revision. No old fallback is aliased by another role in this case.
    std::array<Inputs::Ref<Inputs::LLViewerFetchedTexture>*, 3> fallback_slots{
        &Inputs::LLViewerFetchedTexture::sDefaultImagep,
        &Inputs::LLViewerFetchedTexture::sWhiteImagep,
        &Inputs::LLViewerFetchedTexture::sFlatNormalImagep};
    relay.reset();
    for (std::size_t index = 0; index < fallback_slots.size(); ++index)
    {
        auto& old_image = *images[23 + index];
        old_image.native->nativePublication().withdraw();
        std::weak_ptr<DependencyTopology::Relay> prior(info.dependencies);
        fallback_slots[index]->value = &images[26 + index]->facade;
        info.observeShared(); CHECK(prior.expired());
        auto revision = info.state->revision();
        old_image.native->nativePublication().didPublish(); CHECK(info.state->revision() == revision);
        images[26 + index]->native->nativePublication().didPublish(); CHECK(info.state->revision() == revision + 1);
        info.observeShared();
    }
    CHECK(info.layout_revision == 1);
    const auto after = DependencyTopology::ALNativeDrawSourceDependencies::statistics;
    CHECK(after.refresh_calls - before.refresh_calls == 67);
    CHECK(after.topology_reuses - before.topology_reuses == 63);
    CHECK(after.relay_completions - before.relay_completions == 4);
    CHECK(after.actual_subscription_calls - before.actual_subscription_calls == 120);
    CHECK(after.failures == before.failures);
}

TEST_CASE("Dependency identity storage preserves eight nine and large topology transactions",
          "[almetal][pipeline-dependency-topology]")
{
    Publication fixture;
    struct ClearFallbacks
    {
        Inputs::Ref<Inputs::LLViewerFetchedTexture> defaults = Inputs::LLViewerFetchedTexture::sDefaultImagep;
        Inputs::Ref<Inputs::LLViewerFetchedTexture> white = Inputs::LLViewerFetchedTexture::sWhiteImagep;
        Inputs::Ref<Inputs::LLViewerFetchedTexture> normal = Inputs::LLViewerFetchedTexture::sFlatNormalImagep;
        ClearFallbacks()
        {
            Inputs::LLViewerFetchedTexture::sDefaultImagep.value = nullptr;
            Inputs::LLViewerFetchedTexture::sWhiteImagep.value = nullptr;
            Inputs::LLViewerFetchedTexture::sFlatNormalImagep.value = nullptr;
        }
        ~ClearFallbacks()
        {
            Inputs::LLViewerFetchedTexture::sDefaultImagep = defaults;
            Inputs::LLViewerFetchedTexture::sWhiteImagep = white;
            Inputs::LLViewerFetchedTexture::sFlatNormalImagep = normal;
        }
    } clear;
    for (const std::size_t count : {8u, 9u, 64u})
    {
        CAPTURE(count);
        std::vector<std::unique_ptr<DependencyTopology::Image>> images;
        for (std::size_t at = 0; at < count + 1; ++at) images.push_back(std::make_unique<DependencyTopology::Image>());
        DependencyTopology::Owner info;
        info.mVertexBuffer = fixture.buffer(false);
        for (std::size_t at = 0; at < count - 1; ++at) info.mTextureList.push_back({&images[at]->facade});
        const auto before = DependencyTopology::ALNativeDrawSourceDependencies::statistics;
        info.observeShared();
        REQUIRE(info.dependencies->signalCount() == count);
        auto* original = info.dependencies.get();
        images[count - 2]->native->nativePublication().didPublish();
        info.observeShared(); CHECK(info.dependencies.get() == original);
        info.mTextureList.front().value = &images[count - 1]->facade;
        info.state->invalidate();
        const auto cached = info.dependency_revision;
        {
            DependencyTopology::SubscriptionFault fail_at_boundary(count - 1);
            REQUIRE_THROWS_AS(info.observeShared(), std::bad_alloc);
            CHECK(DependencyTopology::SubscriptionFault::attempted == count);
        }
        CHECK(info.dependencies.get() == original);
        CHECK(info.dependency_revision == cached);
        auto revision = info.state->revision();
        images[count - 1]->native->nativePublication().didPublish(); CHECK(info.state->revision() == revision);
        images.front()->native->nativePublication().didPublish(); CHECK(info.state->revision() == revision + 1);
        std::weak_ptr<DependencyTopology::Relay> old(info.dependencies);
        info.observeShared(); CHECK(old.expired());
        REQUIRE(info.dependencies->signalCount() == count);
        revision = info.state->revision();
        images.front()->native->nativePublication().didPublish(); CHECK(info.state->revision() == revision);
        images[count - 1]->native->nativePublication().didPublish(); CHECK(info.state->revision() == revision + 1);
        info.observeShared();
        old = info.dependencies;
        info.mTextureList.push_back({&images[count]->facade}); info.state->invalidate();
        info.observeShared(); CHECK(old.expired());
        CHECK(info.dependencies->signalCount() == count + 1);
        const auto after = DependencyTopology::ALNativeDrawSourceDependencies::statistics;
        CHECK(after.refresh_calls - before.refresh_calls == 6);
        CHECK(after.topology_reuses - before.topology_reuses == 2);
        CHECK(after.relay_completions - before.relay_completions == 3);
        CHECK(after.actual_subscription_calls - before.actual_subscription_calls == count * 4);
        CHECK(after.failures - before.failures == 1);
    }
}
