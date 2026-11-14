/**
 * @file almetalshadowcache.cpp
 * @brief Native shadow source admission and unique-resource accounting.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalshadowcache.h"
#include "almetalshadowprepare.h"

#include <algorithm>
#include <list>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

struct ALMetalShadowSourceCache::Impl
{
    struct Resource {const void* identity; std::uint64_t bytes;};
    struct Accounting {std::size_t references = 0; std::uint64_t bytes = 0;};
    struct Entry
    {
        std::shared_ptr<const void> owner;
        std::shared_ptr<const ALMetalShadowSourceRecord> record;
        std::uint32_t epoch;
        std::vector<Resource> resources;
    };
    using Entries = std::list<Entry>;
    Entries entries;
    std::unordered_map<const void*, Entries::iterator> lookup;
    std::unordered_map<const void*, Accounting> resources;
    std::size_t max_entries;
    std::uint64_t max_bytes;
    std::uint32_t max_age;
    std::uint64_t bytes = 0;

    void remove(Entries::iterator at) noexcept
    {
        for (const auto& resource : at->resources)
        {
            const auto found = resources.find(resource.identity);
            if (found != resources.end() && --found->second.references == 0)
            {
                bytes -= found->second.bytes;
                resources.erase(found);
            }
        }
        lookup.erase(at->owner.get());
        entries.erase(at);
    }
    std::uint64_t additionalBytes(const std::vector<Resource>& values) const noexcept
    {
        std::uint64_t additional = 0;
        for (const auto& value : values)
        {
            if (resources.contains(value.identity)) continue;
            if (value.bytes > std::numeric_limits<std::uint64_t>::max() - additional)
                return std::numeric_limits<std::uint64_t>::max();
            additional += value.bytes;
        }
        return additional;
    }
};

ALMetalShadowSourceCache::ALMetalShadowSourceCache(
    std::size_t maxEntries, std::uint64_t maxBytes, std::uint32_t maxAge)
    : mImpl(std::make_unique<Impl>())
{
    mImpl->max_entries = maxEntries;
    mImpl->max_bytes = maxBytes;
    mImpl->max_age = maxAge;
}
ALMetalShadowSourceCache::~ALMetalShadowSourceCache() = default;

const std::shared_ptr<const ALMetalShadowSourceRecord>*
ALMetalShadowSourceCache::find(const void* owner) const noexcept
{
    const auto found = mImpl->lookup.find(owner);
    return found == mImpl->lookup.end() ? nullptr : &found->second->record;
}
void ALMetalShadowSourceCache::touch(const void* owner, std::uint32_t epoch)
{
    const auto found = mImpl->lookup.find(owner);
    if (found == mImpl->lookup.end()) return;
    found->second->epoch = epoch;
    mImpl->entries.splice(mImpl->entries.end(), mImpl->entries, found->second);
}
void ALMetalShadowSourceCache::trim(std::uint32_t epoch)
{
    // Display epochs advance on the main owner (unsigned wrap is intentional).
    while (!mImpl->entries.empty() &&
        std::uint32_t(epoch - mImpl->entries.front().epoch) > mImpl->max_age)
        mImpl->remove(mImpl->entries.begin());
}
void ALMetalShadowSourceCache::clear() noexcept
{
    mImpl->lookup.clear();
    mImpl->entries.clear();
    mImpl->resources.clear();
    mImpl->bytes = 0;
}
void ALMetalShadowSourceCache::setByteBudget(std::uint64_t bytes)
{
    mImpl->max_bytes = bytes;
    // Explicit budget reductions take precedence over working-set protection.
    while (mImpl->bytes > bytes && !mImpl->entries.empty())
        mImpl->remove(mImpl->entries.begin());
}
std::size_t ALMetalShadowSourceCache::entryCount() const noexcept
{ return mImpl->entries.size(); }
std::uint64_t ALMetalShadowSourceCache::estimatedRetainedBytes() const noexcept
{ return mImpl->bytes; }

bool ALMetalShadowSourceCache::publish(std::shared_ptr<const void> owner,
    std::shared_ptr<const ALMetalShadowSourceRecord> record, std::uint32_t epoch)
{
    if (!owner) return false;
    try
    {
        const auto prior = mImpl->lookup.find(owner.get());
        if (prior != mImpl->lookup.end()) mImpl->remove(prior->second);
        if (!record) return false;
        trim(epoch);
        if (mImpl->max_entries == 0) return false;
        Impl::Entry entry{std::move(owner), std::move(record), epoch, {}};
        const auto add = [&](const void* identity, std::uint64_t bytes)
        {
            if (std::none_of(entry.resources.begin(), entry.resources.end(),
                    [&](const auto& value) {return value.identity == identity;}))
                entry.resources.push_back({identity, bytes});
        };
        const auto& batch = entry.record->prototype();
        add(batch.vertices, batch.vertices->length());
        add(batch.indices, batch.indices->length());
        for (std::size_t slot = 0; slot < batch.texture_count; ++slot)
        {
            const auto* texture = batch.coverageData().textures[slot];
            add(texture, texture->estimatedAllocationBytes());
        }
        // A source that cannot fit by itself must not flush a useful older
        // working set before inevitably falling back to the caller's lease.
        std::uint64_t source_bytes = 0;
        for (const auto& resource : entry.resources)
        {
            if (resource.bytes > mImpl->max_bytes - source_bytes) return false;
            source_bytes += resource.bytes;
        }
        auto additional = mImpl->additionalBytes(entry.resources);
        while (mImpl->entries.size() >= mImpl->max_entries ||
            additional > mImpl->max_bytes - std::min(mImpl->bytes, mImpl->max_bytes))
        {
            if (mImpl->entries.empty()) return false;
            // PPD106: never scan-evict a working subset already used in this
            // display epoch. LRU touches put current entries at the tail, so
            // finding one at the front means every remaining entry is protected.
            // The uncached overflow stays valid in its caller/command aggregate.
            if (mImpl->entries.front().epoch == epoch) return false;
            mImpl->remove(mImpl->entries.begin());
            additional = mImpl->additionalBytes(entry.resources);
        }
        for (const auto& value : entry.resources)
        {
            auto& accounting = mImpl->resources[value.identity];
            if (accounting.references++ == 0)
            {accounting.bytes = value.bytes; mImpl->bytes += value.bytes;}
        }
        mImpl->entries.push_back(std::move(entry));
        auto at = std::prev(mImpl->entries.end());
        mImpl->lookup.emplace(at->owner.get(), at);
        return true;
    }
    catch (...)
    {
        // A failed allocation may have partially updated resource references.
        // Reset every ledger rather than expose inconsistent ownership/bytes.
        clear();
        throw;
    }
}
