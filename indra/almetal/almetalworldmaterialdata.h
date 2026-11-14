/**
 * @file almetalworldmaterialdata.h
 * @brief Weak reuse of immutable native world material constant buffers.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include "almetalresource.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

// Scene owner only. getOrPublish requires the original device wrapper alive;
// check belongsTo and replace this cache when that owner changes. The cache
// retains bounded weak metadata, never a publisher
// or reader lease, and neither GPU completion nor resource destruction calls
// back into it. The returned buffer is written once before publication; all
// consumers must preserve its immutable bytes, including through mappedBytes().
class ALMetalWorldMaterialDataCache final
{
public:
    struct Stats
    {
        std::uint64_t published_bytes = 0;
        std::uint64_t publications = 0;
        std::uint64_t hits = 0;
        std::uint64_t failures = 0;
        std::uint64_t pruned = 0;
        std::uint64_t evictions = 0;
        std::size_t entries = 0;
    };

    // A zero bound disables reuse metadata; valid requests still publish.
    explicit ALMetalWorldMaterialDataCache(const ALMetalDevice& device,
                                          std::size_t max_entries = 4096);
    ~ALMetalWorldMaterialDataCache();
    ALMetalWorldMaterialDataCache(const ALMetalWorldMaterialDataCache&) = delete;
    ALMetalWorldMaterialDataCache& operator=(const ALMetalWorldMaterialDataCache&) = delete;

    // Exact live-buffer length/hash/byte comparison, never a hash-only match.
    // No additional CPU byte copy is retained. Allocation/write failures return
    // null and increment failures; recovering from allocation failure does not
    // allocate a diagnostic string. Label affects new buffers only.
    std::shared_ptr<const ALMetalBuffer> getOrPublish(
        std::span<const std::byte> bytes, const char* label,
        std::string* error = nullptr);
    // Compares captured wrapper/native identities without reading the old
    // wrapper, so a new wrapper for the same physical GPU is still rejected.
    bool belongsTo(const ALMetalDevice& device) const noexcept;
    Stats stats() const noexcept;
    // Drops metadata only; leases remain valid and cumulative stats survive.
    void clear() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
