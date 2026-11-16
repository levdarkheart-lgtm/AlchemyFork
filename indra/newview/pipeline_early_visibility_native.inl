/**
 * @file pipeline_early_visibility_native.inl
 * @brief Weak successful static publications for speculative native bounds work.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Include after pipeline_group_publication_native.inl. All calls are scene-owner
 * only. Neither idle entries nor their removal own or mutate scene publishers.
 */
#include <cstring>
#include <span>
#include <vector>

namespace ALSceneGroupPublicationNative::early
{
inline constexpr std::size_t maximum_groups = 2048, maximum_rows = 8192;
inline constexpr U64 maximum_cpu_bytes = 32ULL * 1024 * 1024;

struct Candidate
{
    std::shared_ptr<const ALMetalWorldSourceCohort> cohort;
    std::shared_ptr<const void> cpu_owner, native_owner;
    ALNativeRegionModel::Snapshot region;
    std::weak_ptr<ALNativePublication::State> producer;
    U64 state_identity = 0, revision = 0, serial = 0;
    U64 material_flavor = 0, estimated_cpu_bytes = 0;
    U32 family = 0;
};

struct Statistics
{
    U64 registrations = 0, registration_denials = 0, stale_pruned = 0;
    U64 snapshots = 0, candidates = 0, region_misses = 0;
    U64 snapshot_denials = 0, allocation_failures = 0;
    std::size_t resident_groups = 0, resident_rows = 0;
    U64 resident_estimated_cpu_bytes = 0;
};

namespace detail
{
struct Entry
{
    std::weak_ptr<ALNativePublication::State> producer;
    std::weak_ptr<const Payload> payload;
    U64 identity = 0, revision = 0, serial = 0, region_identity = 0;
    U64 material_flavor = 0, estimated_cpu_bytes = 0;
    std::size_t rows = 0;
    U32 family = 0;
};
inline bool active = false;
inline std::vector<Entry> entries;
inline Statistics statistics;

// The same conservative CPU descriptor scale as group layout residency. Native
// allocation sizes and allocator bookkeeping are not physical-RAM claims here.
// Shared child descriptions may be counted more than once. This runs only at
// successful publication, never across candidate rows on a view snapshot.
inline U64 estimateCPUBytes(const Payload& payload) noexcept
{
    U64 bytes = 0;
    const auto add = [&](std::size_t count, std::size_t unit) -> bool
    {
        if (!unit || count > (maximum_cpu_bytes - bytes) / unit) return false;
        bytes += U64(count) * unit;
        return true;
    };
    if (!add(1, sizeof(Payload) + sizeof(Candidate) + sizeof(Entry)) ||
        !add(payload.sources.capacity(), sizeof(SourceLease)) ||
        !add(payload.keyed_sources.capacity(), sizeof(Payload::KeyedSource)) ||
        !add(payload.resources.capacity(), sizeof(ALNativeSourceWorkingSet::Resource)) ||
        !payload.native_tuple ||
        !add(1, sizeof(ALScenePublicationNative::NativeTuple)) ||
        !add(payload.native_tuple->resources.capacity(), sizeof(ALNativeSourceWorkingSet::Resource)) ||
        !payload.cohort || !add(payload.cohort->metadataBytes(), 1))
        return maximum_cpu_bytes + 1;
    for (const auto& lease : payload.sources)
    {
        if (!lease.owner || !add(1, sizeof(Source)) ||
            !add(lease.owner->resources.capacity(), sizeof(WeakResource)) ||
            !add(lease.owner->textures.capacity(), sizeof(std::weak_ptr<const ALMetalTexture>)) ||
            (lease.owner->restoration && !add(lease.owner->restoration->ownedBytes(), 1)) ||
            (lease.owner->simple_proof && !add(lease.owner->simple_proof->ownedBytes(), 1)))
            return maximum_cpu_bytes + 1;
        if (const auto& world = lease.world_owner)
        {
            if (!add(1, sizeof(ALScenePublicationNative::Payload)) ||
                !add(world->worlds.capacity(), sizeof(ALScenePublicationNative::WorldVariant)) ||
                !add(world->shadows.capacity(), sizeof(ALScenePublicationNative::ShadowVariant)) ||
                !add(world->resources.capacity(), sizeof(ALNativeSourceWorkingSet::Resource)) ||
                (world->native_tuple &&
                    (!add(1, sizeof(ALScenePublicationNative::NativeTuple)) ||
                     !add(world->native_tuple->resources.capacity(), sizeof(ALNativeSourceWorkingSet::Resource)))))
                return maximum_cpu_bytes + 1;
            for (const auto& variant : world->worlds)
                if (variant.cohort_source && !add(variant.cohort_source->ownedBytes(), 1))
                    return maximum_cpu_bytes + 1;
        }
    }
    return bytes;
}

inline bool current(const Entry& entry, const std::shared_ptr<ALNativePublication::State>& state,
    const std::shared_ptr<const Payload>& payload) noexcept
{
    if (!state || !payload) return false;
    const auto& published = state->publication();
    return !state->dirty() && state->identity() == entry.identity &&
        state->revision() == entry.revision && published && published.get() == payload.get() &&
        !published.owner_before(payload) && !payload.owner_before(published) &&
        payload->serial == entry.serial && payload->region_identity == entry.region_identity &&
        payload->material_flavor == entry.material_flavor && payload->cohort &&
        payload->cohort->family() == entry.family && payload->cohort->size() == entry.rows &&
        payload->cohort->regionIdentity() == entry.region_identity && payload->native_tuple &&
        payload->cohort->matchesOwners(payload, payload->native_tuple);
}

inline void remove(std::vector<Entry>::iterator at) noexcept
{
    statistics.resident_rows -= at->rows;
    statistics.resident_estimated_cpu_bytes -= at->estimated_cpu_bytes;
    entries.erase(at);
}

inline void prune() noexcept
{
    for (auto at = entries.begin(); at != entries.end();)
    {
        const auto state = at->producer.lock();
        const auto payload = at->payload.lock();
        if (current(*at, state, payload)) ++at;
        else
        {
            const auto offset = at - entries.begin();
            remove(at);
            at = entries.begin() + offset;
            ++statistics.stale_pruned;
        }
    }
}
} // namespace detail

inline void enable(bool value) noexcept
{
    detail::active = value;
    if (!value)
    {
        std::vector<detail::Entry>().swap(detail::entries);
        detail::statistics.resident_rows = 0;
        detail::statistics.resident_estimated_cpu_bytes = 0;
    }
}

inline bool enabled() noexcept { return detail::active; }

inline Statistics stats() noexcept
{
    auto result = detail::statistics;
    result.resident_groups = detail::entries.size();
    return result;
}

inline void registerResident(const std::shared_ptr<ALNativePublication::State>& state,
    const std::shared_ptr<const Payload>& payload) noexcept
{
    if (!detail::active) return;
    if (!state || !payload || !payload->cohort || !payload->cohort->size() ||
        payload->cohort->size() > maximum_rows || payload->cohort->family() > 2 ||
        !payload->cohort->hasBoundsPublication() || !payload->cohort->qualifiedBoundsCount())
    { ++detail::statistics.registration_denials; return; }
    detail::prune();
    detail::Entry entry{state, payload, state->identity(), state->revision(), payload->serial,
        payload->region_identity, payload->material_flavor, detail::estimateCPUBytes(*payload),
        payload->cohort->size(), payload->cohort->family()};
    if (!detail::current(entry, state, payload))
    { ++detail::statistics.registration_denials; return; }
    // Replacing this optional weak entry has no effect on producer residency.
    for (auto at = detail::entries.begin(); at != detail::entries.end(); ++at)
        if (at->identity == entry.identity) { detail::remove(at); break; }
    if (detail::entries.size() >= maximum_groups ||
        entry.rows > maximum_rows - detail::statistics.resident_rows ||
        entry.estimated_cpu_bytes > maximum_cpu_bytes - detail::statistics.resident_estimated_cpu_bytes)
    { ++detail::statistics.registration_denials; return; }
    try { detail::entries.push_back(std::move(entry)); }
    catch (const std::bad_alloc&)
    { ++detail::statistics.allocation_failures; ++detail::statistics.registration_denials; return; }
    const auto& stored = detail::entries.back();
    detail::statistics.resident_rows += stored.rows;
    detail::statistics.resident_estimated_cpu_bytes += stored.estimated_cpu_bytes;
    ++detail::statistics.registrations;
}

// Regions are freshly copied by the owner for THIS main view. Old publication
// model matrices are not reused after a region rebase. The consumer must still
// reject a later changed region/view/publication; a retained snapshot alone is
// a lifetime proof, not permission to render obsolete state.
inline std::vector<Candidate> snapshot(std::span<const ALNativeRegionModel::Snapshot> regions) noexcept
{
    std::vector<Candidate> result;
    if (!detail::active) return result;
    ++detail::statistics.snapshots;
    detail::prune();
    try
    {
        result.reserve(detail::entries.size());
        std::size_t rows = 0;
        U64 bytes = 0;
        for (const auto& entry : detail::entries)
        {
            const auto state = entry.producer.lock();
            const auto payload = entry.payload.lock();
            if (!detail::current(entry, state, payload)) continue;
            const ALNativeRegionModel::Snapshot* region = nullptr;
            bool conflicting = false;
            for (const auto& current : regions)
                if (current.identity == entry.region_identity)
                {
                    if (!current || (region && (current.revision != region->revision ||
                        std::memcmp(current.model.data(), region->model.data(), sizeof(current.model)))))
                    { conflicting = true; break; }
                    region = &current;
                }
            if (!region || conflicting) { ++detail::statistics.region_misses; continue; }
            if (!payload->cohort->hasBoundsPublication() || !payload->cohort->qualifiedBoundsCount() ||
                result.size() >= maximum_groups || entry.rows > maximum_rows - rows ||
                entry.estimated_cpu_bytes > maximum_cpu_bytes - bytes)
            { ++detail::statistics.snapshot_denials; continue; }
            result.push_back(Candidate{payload->cohort, payload, payload->native_tuple, *region,
                state, entry.identity, entry.revision, entry.serial, entry.material_flavor,
                entry.estimated_cpu_bytes, entry.family});
            rows += entry.rows;
            bytes += entry.estimated_cpu_bytes;
        }
    }
    catch (const std::bad_alloc&)
    {
        ++detail::statistics.allocation_failures;
        ++detail::statistics.snapshot_denials;
        result.clear();
    }
    detail::statistics.candidates += result.size();
    return result;
}
} // namespace ALSceneGroupPublicationNative::early
