/**
 * @file llspatialpartition_native_dependencies.inl
 * @brief Owner-side native draw dependency topology and weak subscriptions.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Include after the viewer input types and ALNativePublication declarations.
 * The test appendix includes this same visitor with explicit typed inputs.
 */
#pragma once

namespace ALNativeDrawSourceDependencies
{
struct Stats
{
    // Owner-only invocation totals. At a quiescent boundary every refresh
    // returns a reuse/completed relay or throws. Same-revision owner returns
    // never enter refresh. Subscription calls include duplicate Signal edges
    // and a Signal::observe which throws, but exclude a pre-call test fault.
    std::uint64_t refresh_calls = 0, topology_reuses = 0, relay_completions = 0;
    std::uint64_t actual_subscription_calls = 0, failures = 0;
};
inline Stats statistics;

// The input objects are read only on their scene owner. Signal identities,
// rather than raw object addresses or content revisions, identify topology.
template<typename DrawInfo, typename Visitor>
bool visit(const DrawInfo& info, Visitor&& visitor)
{
#if LL_DARWIN
    if (info.mVertexBuffer && !visitor(info.mVertexBuffer->nativePublication())) return false;
    const auto image = [&](LLViewerTexture* texture)
    {
        LLImageGL* native = texture ? texture->getGLTexture() : nullptr;
        return !native || visitor(native->nativePublication());
    };
    const auto material = [&](LLFetchedGLTFMaterial* value)
    {
        if (!value) return true;
        if (!visitor(value->nativePublication())) return false;
        for (U32 slot = 0; slot < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++slot)
            if (!image(value->getRenderTexture(static_cast<LLGLTFMaterial::TextureInfo>(slot)))) return false;
        return true;
    };
    if (!image(info.mTexture.get()) || !image(info.mSpecularMap.get()) || !image(info.mNormalMap.get())) return false;
    for (const auto& value : info.mTextureList) if (!image(value.get())) return false;
    for (const auto& value : info.mMaterialSlotList)
        if (!image(value.mDiffuse.get()) || !image(value.mNormalMap.get()) || !image(value.mSpecularMap.get())) return false;
    if (!material(info.mGLTFMaterial.get())) return false;
    for (const auto& value : info.mGLTFMaterialList) if (!material(value.get())) return false;
    // Keep every fallback which the original native source walk observes.
    if (!image(LLViewerFetchedTexture::sDefaultImagep.get()) ||
        !image(LLViewerFetchedTexture::sWhiteImagep.get()) ||
        !image(LLViewerFetchedTexture::sFlatNormalImagep.get())) return false;
#endif
    return true;
}

class Relay final : public ALNativePublication::Observer
{
public:
    explicit Relay(const std::shared_ptr<ALNativePublication::State>& state)
        : mState(state), mStateIdentity(state->identity()) {}
    Relay(const Relay&) = delete;
    Relay& operator=(const Relay&) = delete;

    void nativePublicationChanged(ALNativePublication::Phase phase) noexcept override
    {
        if (auto state = mState.lock()) state->nativePublicationChanged(phase);
    }

    bool sameState(const std::shared_ptr<ALNativePublication::State>& state) const noexcept
    {
        return state && state->identity() == mStateIdentity &&
            !mState.owner_before(state) && !state.owner_before(mState);
    }

    template<typename DrawInfo>
    bool sameTopology(const DrawInfo& info) const
    {
        const auto signals = identities();
        std::size_t at = 0;
        const bool matched = visit(info, [&](ALNativePublication::Signal& signal)
        {
            if (at == signals.size() || signals[at] != signal.identity()) return false;
            ++at;
            return true;
        });
        return matched && at == signals.size();
    }

    void remember(ALNativePublication::Revision identity)
    {
        if (mOverflow.empty() && mSignalCount < inline_capacity)
        {
            mInline[mSignalCount++] = identity;
            return;
        }
        if (mOverflow.empty())
        {
            mOverflow.reserve(inline_capacity * 2);
            mOverflow.insert(mOverflow.end(), mInline.begin(), mInline.end());
        }
        mOverflow.push_back(identity);
        ++mSignalCount;
    }
    std::size_t signalCount() const noexcept { return mSignalCount; }

private:
    std::span<const ALNativePublication::Revision> identities() const noexcept
    {
        return mOverflow.empty()
            ? std::span<const ALNativePublication::Revision>(mInline.data(), mSignalCount)
            : std::span<const ALNativePublication::Revision>(mOverflow);
    }
    // Eight is a bounded storage hypothesis, not a measured runtime topology
    // histogram. Only initialized numeric IDs are read; larger sets spill.
    static constexpr std::size_t inline_capacity = 8;
    std::weak_ptr<ALNativePublication::State> mState;
    ALNativePublication::Revision mStateIdentity;
    std::array<ALNativePublication::Revision, inline_capacity> mInline;
    std::size_t mSignalCount = 0;
    std::vector<ALNativePublication::Revision> mOverflow;
};

// Owner-only fault seam immediately before the existing subscription call.
// A failure after earlier subscriptions must leave the old relay installed.
inline void (*subscriptionAttemptForTesting)(std::size_t) = nullptr;

template<typename DrawInfo>
std::shared_ptr<Relay> refresh(const DrawInfo& info,
    const std::shared_ptr<ALNativePublication::State>& state,
    const std::shared_ptr<Relay>& previous)
{
    ++statistics.refresh_calls;
    try
    {
        if (previous && previous->sameState(state) && previous->sameTopology(info))
        {
            ++statistics.topology_reuses;
            return previous;
        }
        auto relay = std::make_shared<Relay>(state);
        visit(info, [&](ALNativePublication::Signal& signal)
        {
            relay->remember(signal.identity());
            if (subscriptionAttemptForTesting) subscriptionAttemptForTesting(relay->signalCount() - 1);
            ++statistics.actual_subscription_calls;
            signal.observe(relay);
            return true;
        });
        // The caller replaces its relay only after every subscription succeeded.
        ++statistics.relay_completions;
        return relay;
    }
    catch (...)
    {
        ++statistics.failures;
        throw;
    }
}
}
