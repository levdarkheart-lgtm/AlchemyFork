/**
 * @file pipeline_group_publication_native.inl
 * @brief Completed native group membership, independent of per-view draw maps.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Include after pipeline_scene_publication_native.inl on the scene owner.
 * The immutable result contains native sources and a copied model transform;
 * it has no LLDrawInfo/LLSpatialGroup/region or mutable matrix pointers.
 */
#include <map>
namespace ALSceneGroupPublicationNative
{
struct Payload
{
    std::vector<ALScenePublicationNative::WorldLease> sources;
    std::vector<ALNativeSourceWorkingSet::Resource> resources;
    std::array<float, 16> model{};
    U64 serial = 0;
    U64 index_count = 0;
};
// Group retention is independently budgeted. Evicting a child's lookup cache
// does not mutate its native tuple; its active group can remain resident.
// Actual producer changes (including resource withdrawal) reach the child
// State as invalidate(), and notify will_write before backing COW decisions.
class Dependencies final : public ALNativePublication::Observer
{
public:
    explicit Dependencies(const std::shared_ptr<ALNativePublication::State>& state) : mState(state) {}
    void nativePublicationChanged(ALNativePublication::Phase phase) noexcept override
    {
        if (phase != ALNativePublication::Phase::withdrawn)
            if (auto state = mState.lock()) state->invalidate();
    }
private:
    std::weak_ptr<ALNativePublication::State> mState;
};
struct Statistics
{
    U64 hits = 0, builds = 0, declined = 0, failures = 0;
    U64 admitted_groups = 0, admitted_draws = 0, captured_groups = 0, captured_draws = 0;
};
inline Statistics statistics;

inline std::shared_ptr<const Payload> current(
    const std::shared_ptr<ALNativePublication::State>& state,
    const LLMatrix4a& model, ALNativeSourceWorkingSet& working_set, U32 frame)
{
    if (auto value = state->publication())
    {
        auto group = std::static_pointer_cast<const Payload>(value);
        // Region rebases are separate from mesh publication. Compare one
        // copied transform per group, never retain the region's matrix pointer.
        if (std::memcmp(group->model.data(), model.getF32ptr(), sizeof(group->model)) != 0)
            state->invalidate();
        else if (working_set.touch(state->identity(), group->serial, frame))
            return group;
        else
            state->evict();
    }
    return {};
}

template<class Draws>
std::shared_ptr<const Payload> seal(
    const std::shared_ptr<ALNativePublication::State>& state,
    std::shared_ptr<ALNativePublication::Observer>& dependency_owner,
    const LLMatrix4a& model, const Draws& draws,
    ALNativeSourceWorkingSet& working_set, U64& source_serial, U32 frame)
try
{
    if (auto group = current(state, model, working_set, frame))
    {
        ++statistics.hits;
        return group;
    }
    if (draws.empty()) return {};
    const auto revision = state->revision();
    auto value = std::make_shared<Payload>();
    auto dependencies = std::make_shared<Dependencies>(state);
    // Allocation identities, never recycled raw addresses, deduplicate the
    // group's resource union once at dirty publication. Warm views touch only
    // the group residency entry, not every child or native allocation.
    std::map<std::weak_ptr<const void>, ALNativeSourceWorkingSet::Resource,
        std::owner_less<std::weak_ptr<const void>>> resources;
    std::memcpy(value->model.data(), model.getF32ptr(), sizeof(value->model));
    value->sources.reserve(draws.size());
    for (const auto& entry : draws)
    {
        const LLDrawInfo* info = entry;
        if (!info)
        {
            ++statistics.failures;
            return {};
        }
        // Source changes must discard this derived publisher lease before
        // backing-store COW checks. Actual queued views keep their own lease.
        info->metalSourcePublication()->observe(dependencies);
        if (info->mModelMatrix != &model || info->mTextureMatrix ||
            info->mAvatar || info->mSkinInfo || !info->mCount ||
            info->mGLTFMaterial || !info->mGLTFMaterialList.empty() ||
            info->mMaterial || !info->mMaterialSlotList.empty())
        {
            ++statistics.declined;
            return {};
        }
        ALScenePublicationNative::WorldFlavor flavor;
        flavor.indexed_textures = true;
        auto source = ALScenePublicationNative::world(*info, flavor,
            working_set, source_serial, frame);
        if (!source)
        {
            ++statistics.failures;
            return {};
        }
        value->index_count += info->mCount;
        for (const auto& resource : source.owner->resources)
        {
            const auto [at, inserted] = resources.emplace(resource.allocation, resource);
            if (!inserted && at->second.bytes != resource.bytes)
            { ++statistics.failures; return {}; }
        }
        value->sources.push_back(std::move(source));
    }
    value->resources.reserve(resources.size());
    for (auto& [identity, resource] : resources)
        value->resources.push_back(std::move(resource));
    value->serial = ++source_serial;
    // No valid-looking prefix may replace the group's complete publication.
    if (!state->publish(revision, value))
    {
        ++statistics.failures;
        return {};
    }
    const std::shared_ptr<const void> expected = value;
    if (!working_set.retain(state, value->serial, expected, value->resources, frame))
    {
        // A reader may retain transient children, but it must not manufacture
        // an unbudgeted strong group publisher from those reader leases.
        state->evict();
        ++statistics.failures;
        return {};
    }
    dependency_owner = std::move(dependencies);
    ++statistics.builds;
    return value;
}
catch (const std::bad_alloc&)
{
    ++statistics.failures;
    return {};
}
}
