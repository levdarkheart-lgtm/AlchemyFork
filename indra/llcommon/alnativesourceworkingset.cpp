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

    void remove(Entries::iterator at) noexcept
    {
        auto state = at->state;
        const auto publication = at->publication;
        lookup.erase(state->identity());
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
        if (sameOwner(publication, Weak(state->publication()))) state->evict();
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
    if (!state || !serial || state->dirty() || !publication ||
        !sameOwner(Weak(publication), Weak(state->publication()))) return false;
    try
    {
        const auto prior = mImpl->lookup.find(state->identity());
        if (prior != mImpl->lookup.end())
        {
            if (serial < prior->second->serial) return false;
            const bool same_publication = sameOwner(prior->second->publication, Weak(publication));
            if (serial == prior->second->serial)
                return same_publication && touch(state->identity(), serial, frame);
            // A new serial qualifies a newly sealed payload, not a relabeling
            // of the current one. Invalid admission never withdraws it.
            if (same_publication) return false;
            mImpl->remove(prior->second);
        }
        // remove() may evict only a matching old payload. A replacement is
        // checked again before admitting it into the residency ledger.
        if (!state->publication()) return false;
        trim(frame);
        Impl::Entry candidate{state, Weak(state->publication()), serial, frame, {}};
        std::uint64_t source_bytes = 0;
        for (const auto& resource : resources)
        {
            if (!resource.allocation) { state->evict(); return false; }
            const Weak weak(resource.allocation);
            const auto duplicate = std::find_if(candidate.resources.begin(), candidate.resources.end(),
                [&](const auto& prior_resource) { return sameOwner(prior_resource.allocation, weak); });
            if (duplicate != candidate.resources.end())
            {
                if (duplicate->bytes != resource.bytes) { state->evict(); return false; }
                continue;
            }
            if (resource.bytes > mImpl->max_bytes - source_bytes)
            { state->evict(); return false; }
            const auto existing = mImpl->resources.find(weak);
            // Native wrapper allocations have immutable capacity. An
            // inconsistent estimate must not silently under-account residency.
            if (existing != mImpl->resources.end() && existing->second.bytes != resource.bytes)
            { state->evict(); return false; }
            source_bytes += resource.bytes;
            candidate.resources.push_back({weak, resource.bytes});
        }
        if (!mImpl->max_entries) { state->evict(); return false; }
        auto additional = mImpl->additionalBytes(candidate.resources);
        while (mImpl->entries.size() >= mImpl->max_entries ||
               additional > mImpl->max_bytes - std::min(mImpl->bytes, mImpl->max_bytes))
        {
            // Preserve the current-frame working subset instead of making a
            // repeated overflow scan reconstruct every source on every view.
            if (mImpl->entries.empty() || mImpl->entries.front().frame == frame)
            { state->evict(); return false; }
            mImpl->remove(mImpl->entries.begin());
            additional = mImpl->additionalBytes(candidate.resources);
        }
        for (const auto& resource : candidate.resources)
        {
            auto& accounting = mImpl->resources[resource.allocation];
            if (accounting.references++ == 0)
            { accounting.bytes = resource.bytes; mImpl->bytes += resource.bytes; }
        }
        mImpl->entries.push_back(std::move(candidate));
        const auto at = std::prev(mImpl->entries.end());
        mImpl->lookup.emplace(state->identity(), at);
        return true;
    }
    catch (...)
    {
        clear();
        state->evict();
        throw;
    }
}

bool ALNativeSourceWorkingSet::touch(Identity identity, Serial serial, std::uint32_t frame) noexcept
{
    const auto found = mImpl->lookup.find(identity);
    if (found == mImpl->lookup.end() || found->second->serial != serial) return false;
    auto at = found->second;
    if (!sameOwner(at->publication, Weak(at->state->publication())))
    { mImpl->remove(at); return false; }
    at->frame = frame;
    mImpl->entries.splice(mImpl->entries.end(), mImpl->entries, at);
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
        mImpl->remove(mImpl->entries.begin());
}

void ALNativeSourceWorkingSet::clear() noexcept
{
    // Clear identity lookup first: a payload's destruction may call erase().
    mImpl->lookup.clear();
    for (const auto& entry : mImpl->entries)
        if (sameOwner(entry.publication, Weak(entry.state->publication()))) entry.state->evict();
    mImpl->entries.clear();
    mImpl->resources.clear();
    mImpl->bytes = 0;
}

void ALNativeSourceWorkingSet::setByteBudget(std::uint64_t bytes) noexcept
{
    mImpl->max_bytes = bytes;
    while (mImpl->bytes > bytes && !mImpl->entries.empty())
        mImpl->remove(mImpl->entries.begin());
}

std::size_t ALNativeSourceWorkingSet::entryCount() const noexcept { return mImpl->entries.size(); }
std::uint64_t ALNativeSourceWorkingSet::estimatedRetainedBytes() const noexcept { return mImpl->bytes; }
