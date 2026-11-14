/**
 * @file almetalshadowcache.h
 * @brief Bounded main-owner cache of immutable native shadow sources.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

class ALMetalShadowSourceRecord;

// Main-owner only. Identity owners exclude pointer reuse; native source owners
// exclude backing-store reuse. The byte budget counts distinct native wrapper
// allocations retained by this cache, separately from live command leases.
// Pass current monotonically advancing display epochs (uint32 wrap is allowed).
// Entries used in this epoch are protected from overflow admission; older
// entries may be evicted. Explicit budget reduction always enforces its cap.
class ALMetalShadowSourceCache final
{
public:
    explicit ALMetalShadowSourceCache(std::size_t maxEntries = 8192,
        std::uint64_t maxBytes = 256ull * 1024 * 1024,
        std::uint32_t maxAge = 2);
    ~ALMetalShadowSourceCache();
    ALMetalShadowSourceCache(const ALMetalShadowSourceCache&) = delete;
    ALMetalShadowSourceCache& operator=(const ALMetalShadowSourceCache&) = delete;

    // The returned owner reference remains valid until its entry is replaced,
    // evicted, cleared or this cache is destroyed. A touch/splice preserves it.
    const std::shared_ptr<const ALMetalShadowSourceRecord>* find(
        const void* owner) const noexcept;
    void touch(const void* owner, std::uint32_t displayEpoch);
    // Replacement first removes the previous generation. A false result leaves
    // no stale owner entry; the caller keeps the new source/command lease.
    // Allocation failure clears coherent cache state and propagates.
    bool publish(std::shared_ptr<const void> owner,
        std::shared_ptr<const ALMetalShadowSourceRecord> record,
        std::uint32_t displayEpoch);
    void trim(std::uint32_t displayEpoch);
    void clear() noexcept;
    void setByteBudget(std::uint64_t bytes);
    std::size_t entryCount() const noexcept;
    std::uint64_t estimatedRetainedBytes() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
