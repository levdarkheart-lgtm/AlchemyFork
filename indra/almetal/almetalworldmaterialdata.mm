/**
 * @file almetalworldmaterialdata.mm
 * @brief Owner-only weak material-data publication without retained CPU copies.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalworldmaterialdata.h"

#include <cstring>
#include <new>
#include <unordered_map>

namespace
{
std::uint64_t hashBytes(std::span<const std::byte> bytes) noexcept
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto value : bytes)
    {
        hash ^= std::to_integer<std::uint8_t>(value);
        hash *= 1099511628211ULL;
    }
    return hash;
}
}

struct ALMetalWorldMaterialDataCache::Impl
{
    Impl(const ALMetalDevice& owner, std::size_t maximum)
        : device(&owner), native_device(owner.nativeHandle()), max_entries(maximum) {}

    const ALMetalDevice* const device;
    void* const native_device;
    const std::size_t max_entries;
    std::unordered_multimap<std::uint64_t, std::weak_ptr<const ALMetalBuffer>> entries;
    Stats statistics;

    void pruneExpired() noexcept
    {
        for (auto at = entries.begin(); at != entries.end();)
            if (at->second.expired())
            {
                at = entries.erase(at);
                ++statistics.pruned;
            }
            else ++at;
    }
};

ALMetalWorldMaterialDataCache::ALMetalWorldMaterialDataCache(
    const ALMetalDevice& device, std::size_t max_entries)
    : mImpl(std::make_unique<Impl>(device, max_entries)) {}
ALMetalWorldMaterialDataCache::~ALMetalWorldMaterialDataCache() = default;

std::shared_ptr<const ALMetalBuffer> ALMetalWorldMaterialDataCache::getOrPublish(
    std::span<const std::byte> bytes, const char* label, std::string* error)
try
{
    auto& impl = *mImpl;
    if (!impl.device->nativeHandle() || bytes.empty() || !bytes.data() ||
        bytes.size() > impl.device->info().max_buffer_length)
    {
        if (error) *error = "Native material constants require a device and valid nonempty byte range";
        ++impl.statistics.failures;
        return {};
    }
    const auto hash = hashBytes(bytes);
    const auto [first, last] = impl.entries.equal_range(hash);
    for (auto at = first; at != last;)
    {
        // A weak control block, never an address, establishes allocation
        // identity even when a later wrapper occupies the same memory.
        auto buffer = at->second.lock();
        if (!buffer)
        {
            at = impl.entries.erase(at);
            ++impl.statistics.pruned;
            continue;
        }
        const auto* mapped = buffer->mappedBytes();
        if (buffer->length() == bytes.size() && mapped &&
            std::memcmp(mapped, bytes.data(), bytes.size()) == 0)
        {
            ++impl.statistics.hits;
            return buffer;
        }
        ++at; // A live hash collision is not a match.
    }
    if (impl.max_entries && impl.entries.size() >= impl.max_entries)
    {
        impl.pruneExpired();
        if (impl.entries.size() >= impl.max_entries)
        {
            impl.entries.erase(impl.entries.begin());
            ++impl.statistics.evictions;
        }
    }
    ALMetalBufferDescriptor descriptor;
    descriptor.length = bytes.size();
    descriptor.storage = ALMetalStorageMode::shared;
    // Reuse validates the mapped bytes on CPU. Keep the default CPU cache,
    // rather than write-combined storage intended for unread streaming data.
    descriptor.label = label ? label : "Alchemy immutable world material constants";
    std::shared_ptr<ALMetalBuffer> buffer = ALMetalBuffer::create(*impl.device, descriptor, error);
    if (!buffer || !buffer->write(0, bytes.data(), bytes.size(), error))
    {
        ++impl.statistics.failures;
        return {};
    }
    std::shared_ptr<const ALMetalBuffer> published = std::move(buffer);
    if (impl.max_entries) impl.entries.emplace(hash, published);
    ++impl.statistics.publications;
    impl.statistics.published_bytes += bytes.size();
    return published;
}
catch (const std::bad_alloc&)
{
    ++mImpl->statistics.failures;
    if (error) error->clear();
    return {};
}

bool ALMetalWorldMaterialDataCache::belongsTo(const ALMetalDevice& device) const noexcept
{
    return &device == mImpl->device && mImpl->native_device &&
        device.nativeHandle() == mImpl->native_device;
}

ALMetalWorldMaterialDataCache::Stats ALMetalWorldMaterialDataCache::stats() const noexcept
{
    auto result = mImpl->statistics;
    result.entries = mImpl->entries.size();
    return result;
}

void ALMetalWorldMaterialDataCache::clear() noexcept
{
    mImpl->entries.clear();
}
