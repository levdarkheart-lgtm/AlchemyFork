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
#include "../almetal/almetalworldcohort.h"
namespace ALSceneGroupPublicationNative
{
// Cold optional GPU publication resolves only exact typed roles from the
// already completed native union. The alias owns the original allocation's
// control block, never the tuple or a CPU producer graph.
inline std::shared_ptr<const ALMetalTexture> resolveCohortTexture(
    const std::shared_ptr<const void>& native_owner,const ALMetalTexture* texture) noexcept
{
    if (!native_owner || !texture) return {};
    const auto tuple=std::static_pointer_cast<const ALScenePublicationNative::NativeTuple>(native_owner);
    for (const auto& resource:tuple->resources)
        if (resource.allocation.get()==texture && resource.bytes==ALScenePublicationNative::textureBytes(*texture))
            return std::shared_ptr<const ALMetalTexture>(resource.allocation,texture);
    return {};
}
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
    U64 flavor_key = 0;
    std::vector<WeakResource> resources;
    std::weak_ptr<const ALScenePublicationNative::Payload> world_owner;
    std::weak_ptr<const ALMetalBuffer> vertices, indices;
    std::weak_ptr<const ALMetalWorldMaterialSource> material_source;
    std::weak_ptr<const ALMetalWorldResourceSource> resource_source;
    std::weak_ptr<const ALMetalWorldCohortSource> cohort_source;
    // Plain SIMPLE proofs contain only copied CPU facts/raw native addresses;
    // their immutable snapshots are safe to retain across cache withdrawal.
    // Material proofs retain native wrapper leases and stay weak above.
    std::shared_ptr<const ALMetalWorldCohortSource> simple_proof;
    std::vector<std::weak_ptr<const ALMetalTexture>> textures;
    ALMetalWorldConservativeBounds bounds;
    bool bounds_attempted = false;
};
struct SourceLease
{
    std::shared_ptr<const Source> owner;
    const ALMetalWorldSimpleBatch* prototype = nullptr;
    bool reused = false;
    std::shared_ptr<const ALScenePublicationNative::Payload> world_owner;
    explicit operator bool() const noexcept
    {
        return owner && prototype && (world_owner
            ? ALScenePublicationNative::findWorld(*world_owner, owner->flavor_key) == prototype
            : prototype == &owner->prototype);
    }
};
struct Layout
{
    U64 group_identity = 0;
    std::vector<std::shared_ptr<const Source>> sources;
    std::shared_ptr<const std::vector<WeakResource>> resources;
};
struct Payload
{
    using KeyedSource = ALMetalWorldKeyedSource;
    std::vector<SourceLease> sources;
    // Producer-only ordered span, built once dirty. The private cohort shares
    // source-generation proofs instead of copying/validating every row again.
    std::vector<KeyedSource> keyed_sources;
    std::vector<ALNativeSourceWorkingSet::Resource> resources;
    std::shared_ptr<const ALScenePublicationNative::NativeTuple> native_tuple;
    // The certificate shares const source records and weak backing/native
    // identities. It cannot keep this payload alive through a parent cycle.
    std::shared_ptr<const ALMetalWorldSourceCohort> cohort;
    std::array<float, 16> model{};
    U64 serial = 0;
    U64 index_count = 0;
    U64 region_identity = 0, material_flavor = 0;
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
    U64 material_builds = 0, material_restores = 0;
    U64 material_admitted_groups = 0, material_admitted_draws = 0;
    U64 material_captured_groups = 0, material_captured_draws = 0;
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
            U64(payload.keyed_sources.capacity()) * sizeof(Payload::KeyedSource) +
            U64(payload.resources.capacity()) * sizeof(ALNativeSourceWorkingSet::Resource);
        if (layout->resources)
            bytes += sizeof(std::vector<WeakResource>) + U64(layout->resources->capacity()) * sizeof(WeakResource);
        // NativeTuple is intentionally independent of the CPU payload. The
        // compatibility .resources vector duplicates its metadata at dirty
        // sealing, so charge both publisher containers explicitly.
        if (payload.native_tuple)
            bytes += sizeof(ALScenePublicationNative::NativeTuple) +
                U64(payload.native_tuple->resources.capacity()) * sizeof(ALNativeSourceWorkingSet::Resource);
        if (payload.cohort) bytes += payload.cohort->metadataBytes();
        for (const auto& source : layout->sources)
        {
            bytes += sizeof(Source) + U64(source->resources.capacity()) * sizeof(WeakResource) +
                U64(source->textures.capacity()) * sizeof(std::weak_ptr<const ALMetalTexture>);
            if (source->prototype.material_slot_count) bytes += sizeof(ALMetalWorldSimpleBatch::MaterialSlots);
            if (source->prototype.pbr_slot_count) bytes += sizeof(ALMetalWorldSimpleBatch::PBRSlots);
            if (source->simple_proof) bytes += source->simple_proof->ownedBytes();
        }
        // Material packets can share source descriptions with their child
        // publisher. Charge their retained CPU graph conservatively as well;
        // the persistent Layout itself still owns no strong native lease.
        for (const auto& source : payload.sources)
            if (const auto& world = source.world_owner)
            {
                bytes += sizeof(ALScenePublicationNative::Payload) +
                    U64(world->worlds.capacity()) * sizeof(ALScenePublicationNative::WorldVariant) +
                    U64(world->shadows.capacity()) * sizeof(ALScenePublicationNative::ShadowVariant) +
                    U64(world->resources.capacity()) * sizeof(ALNativeSourceWorkingSet::Resource);
                if (world->native_tuple)
                    bytes += sizeof(ALScenePublicationNative::NativeTuple) +
                        U64(world->native_tuple->resources.capacity()) * sizeof(ALNativeSourceWorkingSet::Resource);
                for (const auto& variant : world->worlds)
                    if (variant.cohort_source) bytes += variant.cohort_source->ownedBytes();
            }
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
    const ALNativeRegionModel::Snapshot& region, ALNativeSourceWorkingSet& working_set, U32 frame)
{
    if (!state || !region) return {};
    if (auto value = state->publication())
    {
        auto group = std::static_pointer_cast<const Payload>(value);
        // The fresh view owns the region epoch/matrix. A rebase does not
        // invalidate these immutable geometry/source-generation proofs.
        if (group->region_identity != region.identity)
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
    const LLMatrix4a& model, const ALNativeRegionModel::Snapshot& region, const Draws& draws,
    ALNativeSourceWorkingSet& working_set, U64& source_serial, U32 frame, bool request_bounds=false)
try
{
    if (!state || !region || std::memcmp(region.model.data(), model.getF32ptr(), sizeof(region.model))) return {};
    if (auto group = current(state, region, working_set, frame))
    {
        ++statistics.hits;
        return group;
    }
    if (draws.empty()) return {};
    if (!cpu_layouts.acceptsRows(draws.size())) { ++statistics.failures; return {}; }
    const auto revision = state->revision();
    auto value = std::make_shared<Payload>();
    value->region_identity = region.identity;
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
    value->keyed_sources.reserve(draws.size());
    std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> proofs;proofs.reserve(draws.size());
    for (std::size_t row = 0; row < draws.size(); ++row)
    {
        auto record = layout->sources[row];
        auto producer = record ? record->producer.lock() : nullptr;
        bool reuse = producer && producer->identity() == record->identity && producer->revision() == record->revision &&
            record->simple_proof && (!request_bounds || record->simple_proof->hasBoundsPublication()) &&
            record->simple_proof->family() == 0 && cpuOnlySimple(record->simple_proof->source());
        if (reuse)
        {
            const auto& key = record->simple_proof->key();
            reuse = key.identity == record->identity && key.revision == record->revision && key.flavor == record->flavor_key;
        }
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
            flavor.indexed_textures = true;flavor.cohort_source = true;flavor.cohort_bounds = request_bounds;
            ALScenePublicationNative::WorldLease source;
            if (!producer->publication() && info->mMetalShadowSource)
            {
                // A real raw reader can outlive its rejected child cache.
                // Reuse that exact revision's immutable data without turning
                // it into a newly admitted child publisher merely to add proof.
                auto transient = ALScenePublicationNative::current(*info);
                if (transient && transient->producer_revision == producer->revision())
                {
                    auto normalized = flavor;normalized.indexed_textures = info->mTextureList.size() > 1;
                    if (const auto* prototype = ALScenePublicationNative::findWorld(*transient, normalized.key());
                        prototype && (!request_bounds || transient->bounds_attempted))
                        source = {std::move(transient), prototype, nullptr, true};
                }
            }
            if (!source) source = ALScenePublicationNative::world(*info, flavor, working_set, source_serial, frame);
            if (!source || !cpuOnlySimple(*source.prototype))
            { ++statistics.failures; return {}; }
            auto next = std::make_shared<Source>();
            next->prototype = *source.prototype;
            next->producer = producer; next->identity = producer->identity();
            next->revision = source.owner->producer_revision;
            for (const auto& variant : source.owner->worlds)
                if (&variant.prototype == source.prototype)
                { next->simple_proof = variant.cohort_source;next->flavor_key = variant.key;break; }
            if (!next->simple_proof && ALScenePublicationNative::completeWorldResources(*source.owner, *source.prototype))
            {
                next->simple_proof = ALMetalWorldCohortSource::seal(
                    {{next->identity, next->flavor_key, next->revision}, source.prototype}, nullptr,
                    source.owner->bounds_attempted ? &source.owner->bounds : nullptr);
                if (next->simple_proof) ++ALScenePublicationNative::cohort_source_publications;
            }
            if (!next->simple_proof || next->simple_proof->family() != 0 || !cpuOnlySimple(next->simple_proof->source()))
            { ++statistics.failures;return {}; }
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
        value->keyed_sources.push_back({record->simple_proof->key(), &record->simple_proof->source()});
        proofs.push_back(record->simple_proof);
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
    // The original typed producer built this exact union. Plain private
    // records retain no native owners; lock/verify every addressed generation
    // before publishing a view that can bypass repeated static validation.
    std::unordered_map<const void*, U64> native;native.reserve(value->resources.size());
    for (const auto& resource : value->resources) native.emplace(resource.allocation.get(), resource.bytes);
    for (const auto& proof : proofs)
    {
        const auto& batch = proof->source();
        const auto present = [&](const void* pointer) { return native.find(pointer) != native.end(); };
        if (!present(batch.vertices) || !present(batch.indices) ||
            native.at(batch.vertices) != batch.vertices->length() || native.at(batch.indices) != batch.indices->length())
        { ++statistics.failures;return {}; }
        for (U32 slot = 0; slot < batch.texture_count; ++slot)
            if (!present(batch.diffuse_textures[slot]) ||
                native.at(batch.diffuse_textures[slot]) != ALScenePublicationNative::textureBytes(*batch.diffuse_textures[slot]))
            { ++statistics.failures;return {}; }
    }
    value->cohort = ALMetalWorldSourceCohort::seal(proofs, value, value->native_tuple, region.identity,nullptr,&resolveCohortTexture);
    if (!value->cohort) { ++statistics.failures;return {}; }
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

inline U64 materialFlavorKey(ALScenePublicationNative::WorldFlavor flavor) noexcept
{
    flavor.indexed_materials = false;
    return flavor.key();
}
inline bool ordinaryMaterial(const ALMetalWorldSimpleBatch& batch) noexcept
{
    return batch.vertices && batch.indices && batch.index_count &&
        ((batch.material_slot_count != 0) != (batch.pbr_slot_count != 0)) &&
        !batch.normal_texture && !batch.skin_palette && !batch.skin_joint_count && !batch.skin_palette_float_count &&
        !batch.avatar_rigid && !batch.avatar_skinning && !batch.avatar_cloth && !batch.avatar_alpha &&
        !batch.terrain && !batch.impostor && !batch.impostor_texture_owner && !batch.terrain_texture_owner &&
        !batch.impostor_fullbright && !batch.impostor_shiny && !batch.impostor_emboss &&
        !batch.water_exclusion && !batch.water_haze && !batch.water_surface && !batch.atmospheric_haze &&
        !batch.pre_water_alpha && !batch.post_water_alpha && !batch.post_water_depth_only &&
        !batch.post_water_glow && !batch.legacy_glow && !batch.pbr_glow &&
        batch.material_alpha_mode != 1 && batch.native_material_source && batch.native_material_source->matches(batch) &&
        (!batch.native_resource_source || batch.native_resource_source->matches(batch));
}

inline std::shared_ptr<const Payload> currentMaterial(
    const std::shared_ptr<ALNativePublication::State>& state,
    const ALNativeRegionModel::Snapshot& model, ALScenePublicationNative::WorldFlavor flavor,
    ALNativeSourceWorkingSet& working_set, U32 frame)
{
    if (!state || !model) return {};
    if (auto publication = state->publication())
    {
        auto value = std::static_pointer_cast<const Payload>(publication);
        // The current view owns its copied region epoch. Geometry/material
        // publication does not change merely because the region rebased.
        if (value->region_identity != model.identity || value->material_flavor != materialFlavorKey(flavor))
            state->invalidate();
        else if (working_set.touch(state->identity(), value->serial, frame) &&
                 cpu_layouts.touch(state->identity(), value->serial, frame)) return value;
        else state->evict();
    }
    return {};
}

// Recover only immutable CPU descriptions and exact typed native generations.
// No LLDrawInfo/texture facade/material dependency walk occurs on this path.
inline bool sourceProofMatches(const std::shared_ptr<const ALMetalWorldCohortSource>& proof,
                               U64 identity, U64 flavor, U64 revision,
                               const ALMetalWorldSimpleBatch& prototype) noexcept
{
    if (!proof) return false;
    const auto& key = proof->key();const auto& certified = proof->source();
    return key.identity == identity && key.flavor == flavor && key.revision == revision &&
        certified.vertices == prototype.vertices && certified.indices == prototype.indices &&
        certified.native_material_source == prototype.native_material_source &&
        certified.texture_count == prototype.texture_count &&
        certified.texture_count <= certified.diffuse_textures.size() &&
        std::equal(certified.diffuse_textures.begin(), certified.diffuse_textures.begin() + certified.texture_count, prototype.diffuse_textures.begin()) &&
        (!prototype.native_resource_source || prototype.native_resource_source->matches(prototype));
}
inline ALScenePublicationNative::WorldLease restoreMaterial(
    const Source& source, const std::shared_ptr<ALNativePublication::State>& producer,
    ALNativeSourceWorkingSet& working_set, U64& serial, U32 frame)
{
    const auto usable = [&](std::shared_ptr<const ALScenePublicationNative::Payload> value)
        -> ALScenePublicationNative::WorldLease
    {
        if (!value || value->producer_revision != source.revision || !value->native_tuple) return {};
        const auto* prototype = ALScenePublicationNative::findWorld(*value, source.flavor_key);
        const auto* variant = ALScenePublicationNative::findWorldVariant(*value, source.flavor_key);
        if (!prototype || !ordinaryMaterial(*prototype) || !variant ||
            !sourceProofMatches(variant->cohort_source, source.identity, source.flavor_key, source.revision, *prototype)) return {};
        working_set.touch(producer->identity(), value->serial, frame);
        return {std::move(value), prototype, nullptr, true};
    };
    if (auto result = usable(source.world_owner.lock())) return result;
    std::shared_ptr<const ALScenePublicationNative::Payload> cached;
    if (auto publication = producer->publication())
    {
        cached = std::static_pointer_cast<const ALScenePublicationNative::Payload>(publication);
        if (auto result = usable(cached)) return result;
    }
    auto vertices = source.vertices.lock(), indices = source.indices.lock();
    if (!vertices || !indices || vertices.get() != source.prototype.vertices || indices.get() != source.prototype.indices)
        return {};
    std::vector<std::shared_ptr<const ALMetalTexture>> textures;
    textures.reserve(source.textures.size());
    for (const auto& weak : source.textures)
    {
        auto texture = weak.lock();if (!texture) return {};
        textures.push_back(std::move(texture));
    }
    auto value = cached && cached->producer_revision == source.revision
        ? std::make_shared<ALScenePublicationNative::Payload>(*cached)
        : std::make_shared<ALScenePublicationNative::Payload>();
    value->producer_revision = source.revision;value->serial = ++serial;
    value->bounds = source.bounds;value->bounds_attempted = source.bounds_attempted;
    value->vertices = std::move(vertices);value->indices = std::move(indices);
    std::erase_if(value->worlds, [&](const auto& variant) { return variant.key == source.flavor_key; });
    auto& variant = value->worlds.emplace_back();variant.key = source.flavor_key;variant.prototype = source.prototype;
    auto& prototype = variant.prototype;
    prototype.native_material_source = source.material_source.lock();
    prototype.native_resource_source = source.resource_source.lock();
    auto* device = LLImageGL::getMetalDevice();if (!device) return {};
    if (!prototype.native_material_source || !prototype.native_material_source->matches(prototype))
    {
        if (!ALScenePublicationNative::material_data_cache || !ALScenePublicationNative::material_data_cache->belongsTo(*device))
            ALScenePublicationNative::material_data_cache = std::make_unique<ALMetalWorldMaterialDataCache>(*device);
        if (!ALMetalWorldMaterialSource::publish(*device, prototype, *ALScenePublicationNative::material_data_cache)) return {};
    }
    if (prototype.native_resource_source && !prototype.native_resource_source->matches(prototype))
        prototype.native_resource_source.reset();
    if (!ordinaryMaterial(prototype)) return {};
    variant.cohort_source = source.cohort_source.lock();
    if (!sourceProofMatches(variant.cohort_source, source.identity, source.flavor_key, source.revision, prototype))
        variant.cohort_source.reset();
    const auto buffer = [&](const std::shared_ptr<const ALMetalBuffer>& allocation)
    { if (allocation) ALScenePublicationNative::account(*value, allocation, allocation->length()); };
    buffer(value->vertices);buffer(value->indices);
    buffer(prototype.native_material_source->materialLease());
    buffer(prototype.native_material_source->pbrLease(false));buffer(prototype.native_material_source->pbrLease(true));
    for (const auto& texture : textures)
        ALScenePublicationNative::account(*value, texture, ALScenePublicationNative::textureBytes(*texture));
    if (!variant.cohort_source)
        ALScenePublicationNative::sealCohortSource(*value, variant, producer->identity(), source.revision);
    if (!variant.cohort_source) return {};
    value->native_tuple = ALScenePublicationNative::nativeTuple(value->resources);
    if (!producer->publish(source.revision, value)) return {};
    const std::shared_ptr<const void> expected = value;
    working_set.retain(producer, value->serial, expected, value->resources, frame);
    if (producer->revision() != source.revision) return {};
    ++statistics.material_restores;
    return {std::move(value), &variant.prototype, nullptr, true};
}

template<class Draws>
std::shared_ptr<const Payload> sealMaterial(
    const std::shared_ptr<ALNativePublication::State>& state,
    std::shared_ptr<ALNativePublication::Observer>& dependency_owner,
    std::shared_ptr<Layout>& layout_owner, const LLMatrix4a& model,
    const ALNativeRegionModel::Snapshot& region, const Draws& draws,
    ALScenePublicationNative::WorldFlavor flavor, ALNativeSourceWorkingSet& working_set,
    U64& source_serial, U32 frame)
try
{
    using Family = ALScenePublicationNative::WorldFamily;
    flavor.cohort_source = true;
    flavor.native_resources = false;
    if (!state || !region || draws.empty() ||
        (flavor.family != Family::legacy && flavor.family != Family::pbr) ||
        flavor.rigged || flavor.pbr_glow || flavor.visibility_bounds || flavor.material_alpha_mode == 1 ||
        !flavor.native_materials ||
        std::memcmp(region.model.data(), model.getF32ptr(), sizeof(region.model))) return {};
    if (auto value = currentMaterial(state, region, flavor, working_set, frame)) return value;
    if (!cpu_layouts.acceptsRows(draws.size())) { ++statistics.failures;return {}; }
    const auto revision = state->revision();
    auto value = std::make_shared<Payload>();value->region_identity = region.identity;
    value->material_flavor = materialFlavorKey(flavor);std::memcpy(value->model.data(), region.model.data(), sizeof(value->model));
    const bool have_layout = layout_owner && layout_owner->group_identity == state->identity() && layout_owner->sources.size() == draws.size();
    auto layout = have_layout ? layout_owner : std::make_shared<Layout>();
    if (!have_layout) { layout->group_identity = state->identity();layout->sources.resize(draws.size()); }
    const bool new_dependencies = !have_layout || !dependency_owner;
    auto dependencies = new_dependencies ? std::shared_ptr<ALNativePublication::Observer>(std::make_shared<Dependencies>(state)) : dependency_owner;
    value->sources.reserve(draws.size());value->keyed_sources.reserve(draws.size());
    std::vector<std::shared_ptr<const ALMetalWorldCohortSource>> cohort_sources;
    cohort_sources.reserve(draws.size());
    for (std::size_t row = 0;row < draws.size();++row)
    {
        auto record = layout->sources[row];auto producer = record ? record->producer.lock() : nullptr;
        const bool reuse = producer && producer->identity() == record->identity && producer->revision() == record->revision;
        bool record_reused = false;
        ALScenePublicationNative::WorldLease source;
        if (reuse) { source = restoreMaterial(*record, producer, working_set, source_serial, frame);if (new_dependencies) producer->observe(dependencies); }
        if (source && flavor.cohort_bounds && !source.owner->bounds_attempted) source = {};
        if (!source)
        {
            const LLDrawInfo* info = draws[row];
            if (!info || info->mModelMatrix != &model || info->mTextureMatrix || info->mAvatar || info->mSkinInfo || !info->mCount)
            { ++statistics.declined;return {}; }
            producer = info->metalSourcePublication();producer->observe(dependencies);
            auto row_flavor = flavor;
            row_flavor.indexed_materials = flavor.family == Family::legacy ? info->mMaterialSlotList.size() > 1 : info->mGLTFMaterialList.size() > 1;
            source = ALScenePublicationNative::world(*info, row_flavor, working_set, source_serial, frame);
            if (!source || !ordinaryMaterial(*source.prototype)) { ++statistics.failures;return {}; }
            auto next = std::make_shared<Source>();next->prototype = *source.prototype;
            next->prototype.native_material_source.reset();next->prototype.native_resource_source.reset();
            next->producer = producer;next->identity = producer->identity();next->revision = source.owner->producer_revision;
            next->flavor_key = row_flavor.key();next->world_owner = source.owner;
            next->bounds = source.owner->bounds;next->bounds_attempted = source.owner->bounds_attempted;
            next->vertices = source.owner->vertices;next->indices = source.owner->indices;
            next->material_source = source.prototype->native_material_source;next->resource_source = source.prototype->native_resource_source;
            const auto* variant = ALScenePublicationNative::findWorldVariant(*source.owner, next->flavor_key);
            if (!variant || !variant->cohort_source) { ++statistics.failures;return {}; }
            next->cohort_source = variant->cohort_source;
            std::vector<std::shared_ptr<const ALMetalTexture>> textures;
            if (!ALScenePublicationNative::collectWorldImages(*source.owner, *source.prototype, textures))
            { ++statistics.failures;return {}; }
            for (const auto& texture : textures) next->textures.push_back(texture);
            for (const auto& resource : source.owner->resources) next->resources.push_back({resource.allocation, resource.bytes});
            if (layout == layout_owner) layout = std::make_shared<Layout>(*layout);
            record = std::move(next);layout->sources[row] = record;++statistics.cpu_source_builds;
        }
        else { ++statistics.cpu_source_reuses;record_reused = true; }
        value->index_count += source.prototype->index_count;
        value->sources.push_back({record, source.prototype, record_reused, source.owner});
        value->keyed_sources.push_back({{record->identity, record->flavor_key, record->revision}, source.prototype});
        const auto* variant = ALScenePublicationNative::findWorldVariant(*source.owner, record->flavor_key);
        if (!variant || !sourceProofMatches(variant->cohort_source, record->identity, record->flavor_key, record->revision, *source.prototype))
        { ++statistics.failures;return {}; }
        cohort_sources.push_back(variant->cohort_source);
    }
    std::map<std::weak_ptr<const void>, WeakResource, std::owner_less<std::weak_ptr<const void>>> resources;
    for (const auto& source : value->sources) for (const auto& resource : source.world_owner->resources)
    {
        const auto [at, inserted] = resources.emplace(resource.allocation, WeakResource{resource.allocation, resource.bytes});
        if (!inserted && at->second.bytes != resource.bytes) { ++statistics.failures;return {}; }
    }
    auto weak_union = std::make_shared<std::vector<WeakResource>>();weak_union->reserve(resources.size());
    value->resources.reserve(resources.size());
    for (const auto& [identity, resource] : resources)
    {
        auto allocation = resource.allocation.lock();if (!allocation) { ++statistics.failures;return {}; }
        weak_union->push_back(resource);value->resources.push_back({std::move(allocation), resource.bytes});
    }
    if (layout == layout_owner) layout = std::make_shared<Layout>(*layout);
    layout->resources = std::move(weak_union);++statistics.resource_union_builds;
    value->native_tuple = ALScenePublicationNative::nativeTuple(value->resources);value->serial = ++source_serial;
    // Only this completed producer knows the typed generation union. A void
    // owner passed to the certificate cannot independently prove ownership.
    // Certify all native addresses against the already-built exact union now,
    // once dirty, before issuing the private immutable cohort certificate.
    const auto contains = [&](const auto& allocation, U64 bytes)
    {
        if (!allocation) return true;
        const auto at = resources.find(std::weak_ptr<const void>(allocation));
        return at != resources.end() && at->second.bytes == bytes &&
            at->second.allocation.lock().get() == allocation.get();
    };
    for (const auto& source : value->sources)
    {
        const auto& prototype = *source.prototype;
        const auto& world = source.world_owner;
        const auto& material = prototype.native_material_source;
        const auto buffer_present = [&](const auto& buffer) { return !buffer || contains(buffer, buffer->length()); };
        if (world->vertices.get() != prototype.vertices || world->indices.get() != prototype.indices ||
            !buffer_present(world->vertices) || !buffer_present(world->indices) ||
            !buffer_present(material->materialLease()) || !buffer_present(material->pbrLease(false)) ||
            !buffer_present(material->pbrLease(true)))
        { ++statistics.failures;return {}; }
        std::vector<std::shared_ptr<const ALMetalTexture>> textures;
        if (!ALScenePublicationNative::collectWorldImages(*world, prototype, textures)) { ++statistics.failures;return {}; }
        for (const auto& texture : textures)
            if (!texture || !contains(texture, ALScenePublicationNative::textureBytes(*texture)))
            { ++statistics.failures;return {}; }
    }
    value->cohort = ALMetalWorldSourceCohort::seal(cohort_sources, value, value->native_tuple, region.identity,nullptr,&resolveCohortTexture);
    if (!value->cohort) { ++statistics.failures;return {}; }
    if (!state->publish(revision, value)) { ++statistics.failures;return {}; }
    const auto still_current = [&] { return state->revision() == revision && !state->dirty() && state->publication().get() == value.get(); };
    const auto drop_candidate = [&]
    { if (state->revision() == revision && state->publication().get() == value.get()) state->evict(); };
    const std::shared_ptr<const void> expected = value;
    try
    {
        if (!working_set.retain(state, value->serial, expected, value->resources, frame) || !still_current() ||
            !cpu_layouts.retain(layout, state, *value, frame) || !still_current())
        { drop_candidate();++statistics.failures;return {}; }
    }
    catch (const std::bad_alloc&)
    { drop_candidate();++statistics.failures;return {}; }
    dependency_owner = std::move(dependencies);layout_owner = std::move(layout);
    ++statistics.builds;++statistics.material_builds;return value;
}
catch (const std::bad_alloc&) { ++statistics.failures;return {}; }
}
