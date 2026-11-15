/**
 * @file alnativesourceworkingset.cpp
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "alnativesourceworkingset.h"

#include <algorithm>
#include <list>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
using Weak = std::weak_ptr<const void>;
bool sameOwner(const Weak& a, const Weak& b) noexcept
{
    return !a.owner_before(b) && !b.owner_before(a);
}
}

struct ALNativeSourceWorkingSet::Impl
{
    struct Resource { Weak allocation; std::uint64_t bytes; };
    struct Accounting { std::size_t references = 0; std::uint64_t bytes = 0; };
    struct Entry
    {
        std::shared_ptr<ALNativePublication::State> state;
        Weak publication;
        Serial serial;
        std::uint32_t frame;
        std::vector<Resource> resources;
    };
    using Entries = std::list<Entry>;
    Entries entries;
    std::unordered_map<Identity, Entries::iterator> lookup;
    std::map<Weak, Accounting, std::owner_less<Weak>> resources;
    std::size_t max_entries;
    std::uint64_t max_bytes;
    std::uint32_t max_age;
    std::uint64_t bytes = 0;
    Stats statistics;

    unsigned capacityMask(std::uint64_t additional) const noexcept
    {
        return (entries.size() >= max_entries ? 1u : 0u) |
            (additional > max_bytes - std::min(bytes, max_bytes) ? 2u : 0u);
    }

    void remove(Entries::iterator at) noexcept
    {
        auto state = at->state;
        const auto publication = at->publication;
        const auto found_identity = lookup.find(state->identity());
        const bool owns_identity = found_identity != lookup.end() && found_identity->second == at;
        if (owns_identity) lookup.erase(found_identity);
        for (const auto& resource : at->resources)
        {
            const auto found = resources.find(resource.allocation);
            if (found != resources.end() && --found->second.references == 0)
            {
                bytes -= found->second.bytes;
                resources.erase(found);
            }
        }
        entries.erase(at);
        // Eviction is not a producer mutation. Never erase a newer source that
        // the same State published while an older packet remained in flight.
        if (owns_identity && sameOwner(publication, Weak(state->publication()))) state->evict();
    }

    std::uint64_t additionalBytes(const std::vector<Resource>& values) const noexcept
    {
        std::uint64_t additional = 0;
        for (const auto& resource : values)
            if (!resources.contains(resource.allocation)) additional += resource.bytes;
        return additional;
    }
};

ALNativeSourceWorkingSet::ALNativeSourceWorkingSet(
    std::size_t entries, std::uint64_t bytes, std::uint32_t age)
    : mImpl(std::make_unique<Impl>())
{
    mImpl->max_entries = entries;
    mImpl->max_bytes = bytes;
    mImpl->max_age = age;
}
ALNativeSourceWorkingSet::~ALNativeSourceWorkingSet() { clear(); }

bool ALNativeSourceWorkingSet::retain(
    std::shared_ptr<ALNativePublication::State> state, Serial serial,
    const std::shared_ptr<const void>& publication,
    std::span<const Resource> resources, std::uint32_t frame)
{
    ++mImpl->statistics.retain_calls;
    const auto result = [&](bool accepted)
    {
        if (accepted) ++mImpl->statistics.retain_successes;
        else ++mImpl->statistics.retain_failures;
        return accepted;
    };
    if (!state || !serial || !publication) return result(false);
    // The argument can alias State::publication(). Freeze its owner and keep
    // its resource span alive even when an eviction callback replaces State.
    const auto requested_publication = publication;
    const Weak requested(requested_publication);
    const auto revision = state->revision();
    const auto current = [&]
    {
        return state->revision() == revision && !state->dirty() &&
            sameOwner(requested, Weak(state->publication()));
    };
    if (!current()) return result(false);
    const auto reject = [&]
    {
        if (current() && !mImpl->lookup.contains(state->identity())) state->evict();
        return false;
    };
    // A notification may have already retained this exact request, or a newer
    // publication. Never overwrite either reentrant ledger entry.
    const auto resume = [&]
    {
        if (!current()) return -1;
        const auto found = mImpl->lookup.find(state->identity());
        if (found == mImpl->lookup.end()) return 0;
        if (found->second->serial == serial && sameOwner(found->second->publication, requested))
            return touch(state->identity(), serial, frame) ? 1 : -1;
        return -1;
    };
    try
    {
        const auto prior = mImpl->lookup.find(state->identity());
        if (prior != mImpl->lookup.end())
        {
            if (serial < prior->second->serial) return result(false);
            const bool same_publication = sameOwner(prior->second->publication, requested);
            if (serial == prior->second->serial)
                return result(same_publication && touch(state->identity(), serial, frame));
            // A new serial qualifies a newly sealed payload, not a relabeling
            // of the current one. Invalid admission never withdraws it.
            if (same_publication) return result(false);
            mImpl->remove(prior->second);
            const auto status = resume();
            if (status) return result(status > 0);
        }
        // remove() may evict only a matching old payload. A replacement is
        // checked again before admitting it into the residency ledger.
        while (!mImpl->entries.empty() &&
               std::uint32_t(frame - mImpl->entries.front().frame) > mImpl->max_age)
        {
            ++mImpl->statistics.age_evictions;
            mImpl->remove(mImpl->entries.begin());
            const auto status = resume();
            if (status) return result(status > 0);
        }
        Impl::Entry candidate{state, requested, serial, frame, {}};
        std::uint64_t source_bytes = 0;
        for (const auto& resource : resources)
        {
            if (!resource.allocation)
            { ++mImpl->statistics.invalid_resources; return result(reject()); }
            const Weak weak(resource.allocation);
            const auto duplicate = std::find_if(candidate.resources.begin(), candidate.resources.end(),
                [&](const auto& prior_resource) { return sameOwner(prior_resource.allocation, weak); });
            if (duplicate != candidate.resources.end())
            {
                if (duplicate->bytes != resource.bytes)
                { ++mImpl->statistics.resource_size_conflicts; return result(reject()); }
                continue;
            }
            if (resource.bytes > mImpl->max_bytes - source_bytes)
            {
                ++mImpl->statistics.oversized_sources;
                if (resource.bytes > mImpl->max_bytes) ++mImpl->statistics.oversized_resources;
                return result(reject());
            }
            const auto existing = mImpl->resources.find(weak);
            // Native wrapper allocations have immutable capacity. An
            // inconsistent estimate must not silently under-account residency.
            if (existing != mImpl->resources.end() && existing->second.bytes != resource.bytes)
            { ++mImpl->statistics.resource_size_conflicts; return result(reject()); }
            source_bytes += resource.bytes;
            candidate.resources.push_back({weak, resource.bytes});
        }
        if (!mImpl->max_entries)
        { ++mImpl->statistics.entry_disabled_rejections; return result(reject()); }
        auto additional = mImpl->additionalBytes(candidate.resources);
        while (const auto mask = mImpl->capacityMask(additional))
        {
            ++mImpl->statistics.capacity_events[mask];
            // Preserve the current-frame working subset instead of making a
            // repeated overflow scan reconstruct every source on every view.
            if (mImpl->entries.empty() || mImpl->entries.front().frame == frame)
            {
                ++mImpl->statistics.capacity_rejections[mask];
                return result(reject());
            }
            ++mImpl->statistics.capacity_evictions;
            mImpl->remove(mImpl->entries.begin());
            const auto status = resume();
            if (status) return result(status > 0);
            if (source_bytes > mImpl->max_bytes)
            { ++mImpl->statistics.oversized_sources; return result(reject()); }
            additional = mImpl->additionalBytes(candidate.resources);
        }
        const auto status = resume();
        if (status) return result(status > 0);
        // Victim notifications may also have admitted shared allocations.
        // Validate their capacity before changing any reference accounting.
        for (const auto& resource : candidate.resources)
            if (const auto existing = mImpl->resources.find(resource.allocation);
                existing != mImpl->resources.end() && existing->second.bytes != resource.bytes)
            { ++mImpl->statistics.resource_size_conflicts; return result(reject()); }
        for (const auto& resource : candidate.resources)
        {
            auto& accounting = mImpl->resources[resource.allocation];
            if (accounting.references++ == 0)
            { accounting.bytes = resource.bytes; mImpl->bytes += resource.bytes; }
        }
        mImpl->entries.push_back(std::move(candidate));
        const auto at = std::prev(mImpl->entries.end());
        if (!mImpl->lookup.emplace(state->identity(), at).second)
        {
            mImpl->remove(at);
            return result(resume() > 0);
        }
        ++mImpl->statistics.new_retains;
        return result(true);
    }
    catch (...)
    {
        ++mImpl->statistics.retain_exceptions;
        clear();
        if (current() && !mImpl->lookup.contains(state->identity())) state->evict();
        throw;
    }
}

bool ALNativeSourceWorkingSet::touch(Identity identity, Serial serial, std::uint32_t frame) noexcept
{
    ++mImpl->statistics.touch_calls;
    const auto found = mImpl->lookup.find(identity);
    if (found == mImpl->lookup.end())
    { ++mImpl->statistics.touch_missing_identity; return false; }
    if (found->second->serial != serial)
    { ++mImpl->statistics.touch_serial_mismatch; return false; }
    auto at = found->second;
    if (!sameOwner(at->publication, Weak(at->state->publication())))
    { ++mImpl->statistics.touch_publication_mismatch; mImpl->remove(at); return false; }
    at->frame = frame;
    mImpl->entries.splice(mImpl->entries.end(), mImpl->entries, at);
    ++mImpl->statistics.touch_hits;
    return true;
}

bool ALNativeSourceWorkingSet::erase(Identity identity, Serial serial) noexcept
{
    const auto found = mImpl->lookup.find(identity);
    if (found == mImpl->lookup.end() || found->second->serial != serial) return false;
    mImpl->remove(found->second);
    return true;
}

void ALNativeSourceWorkingSet::trim(std::uint32_t frame) noexcept
{
    while (!mImpl->entries.empty() &&
        std::uint32_t(frame - mImpl->entries.front().frame) > mImpl->max_age)
    {
        ++mImpl->statistics.age_evictions;
        mImpl->remove(mImpl->entries.begin());
    }
}

void ALNativeSourceWorkingSet::clear() noexcept
{
    // Detach the old ledger before notifications. Reentrant publications and
    // their accounting belong to the live ledger and survive this clear.
    Impl::Entries previous;
    previous.splice(previous.end(), mImpl->entries);
    mImpl->lookup.clear();
    mImpl->resources.clear();
    mImpl->bytes = 0;
    for (const auto& entry : previous)
        if (sameOwner(entry.publication, Weak(entry.state->publication()))) entry.state->evict();
}

void ALNativeSourceWorkingSet::setByteBudget(std::uint64_t bytes) noexcept
{
    mImpl->max_bytes = bytes;
    while (mImpl->bytes > bytes && !mImpl->entries.empty())
    {
        ++mImpl->statistics.budget_shrink_evictions;
        mImpl->remove(mImpl->entries.begin());
    }
}

std::size_t ALNativeSourceWorkingSet::entryCount() const noexcept { return mImpl->entries.size(); }
std::uint64_t ALNativeSourceWorkingSet::estimatedRetainedBytes() const noexcept { return mImpl->bytes; }
ALNativeSourceWorkingSet::Stats ALNativeSourceWorkingSet::stats() const noexcept
{
    auto result = mImpl->statistics;
    result.max_entries = mImpl->max_entries;
    result.max_bytes = mImpl->max_bytes;
    result.max_age = mImpl->max_age;
    result.entries = mImpl->entries.size();
    result.estimated_bytes = mImpl->bytes;
    result.unique_resources = mImpl->resources.size();
    return result;
}
