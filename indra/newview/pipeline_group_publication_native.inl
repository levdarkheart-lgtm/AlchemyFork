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
#include <iterator>
#include <list>
#include <unordered_map>
namespace ALSceneGroupPublicationNative
{
struct WeakResource
{
    std::weak_ptr<const void> allocation;
    U64 bytes = 0;
};
// Persistent owner-side CPU data has no strong native resource or viewer
// owner. A withdrawn publisher therefore releases its GPU leases before COW,
// while unchanged rows remain available for the next dirty publication.
struct Source
{
    ALMetalWorldSimpleBatch prototype;
    std::weak_ptr<ALNativePublication::State> producer;
    U64 identity = 0, revision = 0;
    std::vector<WeakResource> resources;
};
struct SourceLease
{
    std::shared_ptr<const Source> owner;
    const ALMetalWorldSimpleBatch* prototype = nullptr;
    bool reused = false;
    explicit operator bool() const noexcept { return owner && prototype == &owner->prototype; }
};
struct Layout
{
    U64 group_identity = 0;
    std::vector<std::shared_ptr<const Source>> sources;
    std::shared_ptr<const std::vector<WeakResource>> resources;
};
struct Payload
{
    std::vector<SourceLease> sources;
    std::vector<ALNativeSourceWorkingSet::Resource> resources;
    std::shared_ptr<const ALScenePublicationNative::NativeTuple> native_tuple;
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
    U64 cpu_source_builds = 0, cpu_source_reuses = 0, resource_union_builds = 0;
};
inline Statistics statistics;

struct CpuLayoutLimits
{
    std::size_t maximum_layouts = 2048, maximum_rows = 8192;
    U64 maximum_bytes = 32ULL * 1024 * 1024;
};
struct CpuLayoutStats
{
    std::size_t layouts = 0, rows = 0;
    U64 estimated_bytes = 0, evictions = 0, rejections = 0;
};
// Scene-owner only, including eviction. Entries are weak CPU identities and
// contain no native or LL owner. The bound covers cached CPU records, weak
// resource tuples and publisher containers; independently queued Payload
// readers remain outside this cache budget until their command retires.
class CpuLayoutResidency
{
    struct Entry
    {
        std::weak_ptr<Layout> layout;
        std::weak_ptr<ALNativePublication::State> publisher;
        U64 identity = 0, serial = 0, bytes = 0;
        std::size_t rows = 0;
        U32 frame = 0;
    };
    using Entries = std::list<Entry>;
    Entries mEntries;
    std::unordered_map<U64, Entries::iterator> mLookup;
    CpuLayoutLimits mLimits;
    CpuLayoutStats mStats;

    void remove(Entries::iterator at, bool withdraw, bool eviction) noexcept
    {
        const auto entry = *at;
        const auto found = mLookup.find(entry.identity);
        if (found != mLookup.end() && found->second == at) mLookup.erase(found);
        mStats.rows -= entry.rows; mStats.estimated_bytes -= entry.bytes;
        mEntries.erase(at);
        if (!withdraw) return;
        if (auto layout = entry.layout.lock())
        {
            if (eviction && !layout->sources.empty()) ++mStats.evictions;
            // clear() alone would retain vector capacity after eviction.
            std::vector<std::shared_ptr<const Source>>().swap(layout->sources);
            layout->resources.reset();
        }
        if (auto publisher = entry.publisher.lock())
            if (auto publication = publisher->publication())
                if (std::static_pointer_cast<const Payload>(publication)->serial == entry.serial)
                    publisher->evict();
    }
public:
    CpuLayoutLimits limits() const noexcept { return mLimits; }
    CpuLayoutStats stats() const noexcept
    { auto result = mStats; result.layouts = mEntries.size(); return result; }
    // Internal policy seam also used by deterministic budget fixtures. Native
    // resource limits stay owned by ALNativeSourceWorkingSet independently.
    void configure(CpuLayoutLimits limits) noexcept
    {
        mLimits = limits;
        while (!mEntries.empty() && (mEntries.size() > mLimits.maximum_layouts ||
            mStats.rows > mLimits.maximum_rows || mStats.estimated_bytes > mLimits.maximum_bytes))
            remove(mEntries.begin(), true, true);
    }
    bool acceptsRows(std::size_t count) noexcept
    {
        constexpr U64 base = sizeof(Layout) + sizeof(Payload);
        constexpr U64 row = sizeof(Source) + sizeof(std::shared_ptr<const Source>) + sizeof(SourceLease);
        if (!mLimits.maximum_layouts || count > mLimits.maximum_rows || mLimits.maximum_bytes < base ||
            count > (mLimits.maximum_bytes - base) / row)
        { ++mStats.rejections; return false; }
        return true;
    }
    bool touch(U64 identity, U64 serial, U32 frame) noexcept
    {
        const auto found = mLookup.find(identity);
        if (found == mLookup.end() || found->second->serial != serial) return false;
        if (found->second->layout.expired()) { remove(found->second, false, false); return false; }
        found->second->frame = frame;
        mEntries.splice(mEntries.end(), mEntries, found->second);
        return true;
    }
    bool contains(const std::shared_ptr<Layout>& layout) const noexcept
    {
        const auto found = mLookup.find(layout->group_identity);
        return found != mLookup.end() && found->second->layout.lock() == layout;
    }
    // Ended producers may leave only weak ledger metadata until the next
    // pressure pass. This remains a conservative upper bound on cache use.
    bool retain(const std::shared_ptr<Layout>& layout,
                const std::shared_ptr<ALNativePublication::State>& publisher,
                const Payload& payload, U32 frame)
    {
        const auto current = [&]
        {
            return !publisher->dirty() && publisher->publication().get() == &payload &&
                std::static_pointer_cast<const Payload>(publisher->publication())->serial == payload.serial;
        };
        if (!current()) { ++mStats.rejections; return false; }
        // Conservative descriptor/container estimate, excluding allocator
        // bookkeeping and actual reader copies. Each row's full current batch
        // representation is charged; a compact SIMPLE record is future work.
        U64 bytes = sizeof(Layout) + sizeof(Payload) +
            U64(layout->sources.capacity()) * sizeof(std::shared_ptr<const Source>) +
            U64(payload.sources.capacity()) * sizeof(SourceLease) +
            U64(payload.resources.capacity()) * sizeof(ALNativeSourceWorkingSet::Resource);
        if (layout->resources)
            bytes += sizeof(std::vector<WeakResource>) + U64(layout->resources->capacity()) * sizeof(WeakResource);
        // NativeTuple is intentionally independent of the CPU payload. The
        // compatibility .resources vector duplicates its metadata at dirty
        // sealing, so charge both publisher containers explicitly.
        if (payload.native_tuple)
            bytes += sizeof(ALScenePublicationNative::NativeTuple) +
                U64(payload.native_tuple->resources.capacity()) * sizeof(ALNativeSourceWorkingSet::Resource);
        for (const auto& source : layout->sources)
            bytes += sizeof(Source) + U64(source->resources.capacity()) * sizeof(WeakResource);
        if (!mLimits.maximum_layouts || layout->sources.size() > mLimits.maximum_rows || bytes > mLimits.maximum_bytes)
        { ++mStats.rejections; return false; }
        const auto previous = mLookup.find(publisher->identity());
        if (previous != mLookup.end())
        {
            const bool same_layout = previous->second->layout.lock() == layout;
            remove(previous->second, !same_layout, false);
            if (!current()) { ++mStats.rejections; return false; }
        }
        while (!mEntries.empty() && (mEntries.size() >= mLimits.maximum_layouts ||
            layout->sources.size() > mLimits.maximum_rows - mStats.rows ||
            bytes > mLimits.maximum_bytes - mStats.estimated_bytes))
        {
            // Preserve the admitted current-frame subset instead of cycling
            // it through every main/probe view under a busy-scene overflow.
            if (mEntries.front().frame == frame) { ++mStats.rejections; return false; }
            remove(mEntries.begin(), true, true);
            // State eviction notifies real owner observers, which may publish
            // and retain a newer tuple for this same incoming producer.
            if (!current()) { ++mStats.rejections; return false; }
        }
        if (!current() || !mLimits.maximum_layouts || layout->sources.size() > mLimits.maximum_rows || bytes > mLimits.maximum_bytes)
        { ++mStats.rejections; return false; }
        mEntries.push_back({layout, publisher, publisher->identity(), payload.serial, bytes, layout->sources.size(), frame});
        const auto at = std::prev(mEntries.end());
        try
        {
            if (!current() || !mLookup.emplace(publisher->identity(), at).second)
            { mEntries.erase(at); ++mStats.rejections; return false; }
        }
        catch (...) { mEntries.erase(at); throw; }
        mStats.rows += layout->sources.size(); mStats.estimated_bytes += bytes;
        return true;
    }
};
inline CpuLayoutResidency cpu_layouts;
inline CpuLayoutStats cpuLayoutStats() noexcept { return cpu_layouts.stats(); }

inline bool sameResources(const std::vector<WeakResource>& a, const std::vector<WeakResource>& b) noexcept
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].bytes != b[i].bytes || a[i].allocation.owner_before(b[i].allocation) ||
            b[i].allocation.owner_before(a[i].allocation)) return false;
    return true;
}

inline bool cpuOnlySimple(const ALMetalWorldSimpleBatch& batch) noexcept
{
    // Never copy a strong native owner into persistent CPU layout. These are
    // the complete public owning fields of ALMetalWorldSimpleBatch; material
    // and special-pass prototypes are deliberately outside this static slice.
    return !batch.native_material_source && !batch.native_resource_source &&
        !batch.impostor_texture_owner && !batch.terrain_texture_owner &&
        !batch.material_slot_count && !batch.pbr_slot_count && !batch.normal_texture &&
        !batch.skin_palette && !batch.skin_palette_float_count && !batch.skin_joint_count &&
        !batch.avatar_rigid && !batch.avatar_skinning && !batch.avatar_cloth && !batch.avatar_alpha &&
        !batch.impostor && !batch.terrain && !batch.alpha_mask &&
        !batch.impostor_fullbright && !batch.impostor_shiny && !batch.impostor_emboss &&
        !batch.water_exclusion && !batch.water_haze && !batch.water_surface && !batch.atmospheric_haze &&
        !batch.pre_water_alpha && !batch.post_water_alpha && !batch.post_water_depth_only &&
        !batch.post_water_glow && !batch.legacy_glow && !batch.pbr_glow &&
        batch.vertices && batch.indices && batch.index_count;
}

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
        else if (working_set.touch(state->identity(), group->serial, frame) &&
                 cpu_layouts.touch(state->identity(), group->serial, frame))
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
    std::shared_ptr<Layout>& layout_owner,
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
    if (!cpu_layouts.acceptsRows(draws.size())) { ++statistics.failures; return {}; }
    const auto revision = state->revision();
    auto value = std::make_shared<Payload>();
    // Membership/order changes must reset layout_owner at their LLSpatialGroup
    // owner boundary. Child producer invalidation deliberately preserves it.
    const bool have_layout = layout_owner && layout_owner->group_identity == state->identity() &&
        layout_owner->sources.size() == draws.size();
    auto layout = have_layout ? layout_owner : std::make_shared<Layout>();
    if (!have_layout)
    {
        layout->group_identity = state->identity();
        layout->sources.resize(draws.size());
    }
    const bool new_dependencies = !have_layout || !dependency_owner;
    auto dependencies = new_dependencies ? std::shared_ptr<ALNativePublication::Observer>(std::make_shared<Dependencies>(state))
                                        : dependency_owner;
    bool resources_changed = !have_layout || !layout->resources;
    // A source whose individual residency is rejected may still be admitted
    // by the complete group's own budget. Keep only refreshed source leases
    // until the exact union is locked; no such lease enters persistent Layout.
    std::vector<ALScenePublicationNative::WorldLease> refreshed;
    std::memcpy(value->model.data(), model.getF32ptr(), sizeof(value->model));
    value->sources.reserve(draws.size());
    for (std::size_t row = 0; row < draws.size(); ++row)
    {
        auto record = layout->sources[row];
        auto producer = record ? record->producer.lock() : nullptr;
        bool reuse = producer && producer->identity() == record->identity && producer->revision() == record->revision;
        if (reuse)
            for (const auto& resource : record->resources)
                if (resource.allocation.expired()) { reuse = false; break; }
        if (reuse)
        {
            if (new_dependencies) producer->observe(dependencies);
            ++statistics.cpu_source_reuses;
        }
        else
        {
            const LLDrawInfo* info = draws[row];
            if (!info) { ++statistics.failures; return {}; }
            producer = info->metalSourcePublication();
            producer->observe(dependencies);
            if (info->mModelMatrix != &model || info->mTextureMatrix ||
                info->mAvatar || info->mSkinInfo || !info->mCount ||
                info->mGLTFMaterial || !info->mGLTFMaterialList.empty() ||
                info->mMaterial || !info->mMaterialSlotList.empty())
            { ++statistics.declined; return {}; }
            ALScenePublicationNative::WorldFlavor flavor;
            flavor.indexed_textures = true;
            auto source = ALScenePublicationNative::world(*info, flavor, working_set, source_serial, frame);
            if (!source || !cpuOnlySimple(*source.prototype))
            { ++statistics.failures; return {}; }
            auto next = std::make_shared<Source>();
            next->prototype = *source.prototype;
            next->producer = producer; next->identity = producer->identity();
            next->revision = source.owner->producer_revision;
            next->resources.reserve(source.owner->resources.size());
            for (const auto& resource : source.owner->resources)
                next->resources.push_back({resource.allocation, resource.bytes});
            resources_changed = resources_changed || !record || !sameResources(record->resources, next->resources);
            if (layout == layout_owner) layout = std::make_shared<Layout>(*layout);
            record = std::move(next);
            layout->sources[row] = record;
            refreshed.push_back(std::move(source));
            ++statistics.cpu_source_builds;
        }
        value->index_count += record->prototype.index_count;
        value->sources.push_back({record, &record->prototype, reuse});
    }
    if (resources_changed)
    {
        std::map<std::weak_ptr<const void>, WeakResource, std::owner_less<std::weak_ptr<const void>>> resources;
        for (const auto& record : layout->sources)
            for (const auto& resource : record->resources)
            {
                const auto [at, inserted] = resources.emplace(resource.allocation, resource);
                if (!inserted && at->second.bytes != resource.bytes) { ++statistics.failures; return {}; }
            }
        auto weak_union = std::make_shared<std::vector<WeakResource>>();
        weak_union->reserve(resources.size());
        for (const auto& [identity, resource] : resources) weak_union->push_back(resource);
        if (layout == layout_owner) layout = std::make_shared<Layout>(*layout);
        layout->resources = std::move(weak_union);
        ++statistics.resource_union_builds;
    }
    value->resources.reserve(layout->resources->size());
    for (const auto& resource : *layout->resources)
    {
        auto allocation = resource.allocation.lock();
        if (!allocation) { ++statistics.failures; return {}; }
        value->resources.push_back({std::move(allocation), resource.bytes});
    }
    value->native_tuple = ALScenePublicationNative::nativeTuple(value->resources);
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
        if (state->publication().get() == value.get()) state->evict();
        ++statistics.failures;
        return {};
    }
    // Both residency ledgers can invoke State observers while evicting an
    // older publisher. A reentrant replacement always wins over this work.
    const auto still_current = [&]
    { return state->revision() == revision && !state->dirty() && state->publication().get() == value.get(); };
    if (!still_current()) { ++statistics.failures; return {}; }
    bool cpu_cached = false;
    const auto drop_untracked_layout = [&]
    {
        if (layout == layout_owner && !cpu_layouts.contains(layout))
        {
            layout_owner.reset();
            std::vector<std::shared_ptr<const Source>>().swap(layout->sources);
            layout->resources.reset();
        }
    };
    try { cpu_cached = cpu_layouts.retain(layout, state, *value, frame); }
    catch (const std::bad_alloc&)
    {
        if (state->publication().get() == value.get()) state->evict();
        drop_untracked_layout();
        ++statistics.failures;
        return {};
    }
    if (!cpu_cached)
    {
        if (state->publication().get() == value.get()) state->evict();
        drop_untracked_layout();
        ++statistics.failures;
        return {};
    }
    if (!still_current()) { ++statistics.failures; return {}; }
    dependency_owner = std::move(dependencies);
    layout_owner = std::move(layout);
    ++statistics.builds;
    return value;
}
catch (const std::bad_alloc&)
{
    ++statistics.failures;
    return {};
}
}
