/**
 * @file alnativesourceworkingset.h
 * @brief Bounded residency for producer-published native scene inputs.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include "alnativepublication.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

// Publication-owner only. The producer State owns the sole retained source
// payload; this working set never copies that payload or a backing-resource
// lease. Thus willWrite() can drop cache-induced leases before an in-place
// update, while actual queued/encoded/GPU readers retain their own payloads.
// Weak resource control blocks provide allocation identity even after a native
// wrapper dies or another wrapper reuses its address. Bytes are unique wrapper
// estimates, not another physical-memory pool to add to process/device totals.
class ALNativeSourceWorkingSet final
{
public:
    using Identity = ALNativePublication::Revision;
    using Serial = std::uint64_t;
    struct Resource
    {
        std::shared_ptr<const void> allocation;
        std::uint64_t bytes = 0;
    };
    struct Stats
    {
        std::size_t max_entries = 0, entries = 0, unique_resources = 0;
        std::uint64_t max_bytes = 0, estimated_bytes = 0;
        std::uint32_t max_age = 0;
        // Invocation totals include nested calls and retain's internal touch
        // calls. At a quiescent owner boundary calls = success + false + throw.
        // A callback can insert a request which the outer call acknowledges:
        // both invocations succeed, but only one new ledger entry is counted.
        std::uint64_t retain_calls = 0, retain_successes = 0, retain_failures = 0;
        std::uint64_t retain_exceptions = 0, new_retains = 0;
        // Actual capacity-loop iterations, before any eviction callback, and
        // the subset rejected because no eligible victim remained. Mask bits:
        // 1 entry bound, 2 additional unique bytes, 3 both; bucket 0 unused.
        // One retain can visit several masks or recover after eviction; these
        // events are not unique producers or an exhaustive false-return sum.
        std::array<std::uint64_t, 4> capacity_events{}, capacity_rejections{};
        // Ledger entries removed by each existing policy branch; an entry
        // need not still own a publisher when removed. Explicit erase, clear,
        // replacement and stale-publication cleanup are excluded.
        std::uint64_t age_evictions = 0, capacity_evictions = 0, budget_shrink_evictions = 0;
        // Oversized source visits include a resource exceeding the whole cap;
        // oversized_resources is that subset, observed during the existing
        // resource walk. A callback's cap shrink can reject an already-scanned
        // source without another resource walk or resource-level attribution.
        std::uint64_t oversized_sources = 0, oversized_resources = 0;
        std::uint64_t invalid_resources = 0, resource_size_conflicts = 0, entry_disabled_rejections = 0;
        // Mutually exclusive touch outcomes, including internal touch calls.
        std::uint64_t touch_calls = 0, touch_hits = 0, touch_missing_identity = 0;
        std::uint64_t touch_serial_mismatch = 0, touch_publication_mismatch = 0;
    };

    explicit ALNativeSourceWorkingSet(std::size_t max_entries = 8192,
        std::uint64_t max_bytes = 256ULL * 1024 * 1024,
        std::uint32_t max_age = 2);
    ~ALNativeSourceWorkingSet();
    ALNativeSourceWorkingSet(const ALNativeSourceWorkingSet&) = delete;
    ALNativeSourceWorkingSet& operator=(const ALNativeSourceWorkingSet&) = delete;

    // State must already hold this immutable publication. The caller first
    // acquires its packet payload, so rejection/eviction cannot drop a reader.
    // A new serial is required for every sealed payload, even if its producer
    // revision is unchanged after eviction. Late old-packet destruction may
    // then erase only that old serial, never a replacement publication.
    bool retain(std::shared_ptr<ALNativePublication::State> state, Serial serial,
                const std::shared_ptr<const void>& publication,
                std::span<const Resource> resources, std::uint32_t frame);
    bool touch(Identity identity, Serial serial, std::uint32_t frame) noexcept;
    bool erase(Identity identity, Serial serial) noexcept;
    void trim(std::uint32_t frame) noexcept;
    void clear() noexcept;
    void setByteBudget(std::uint64_t bytes) noexcept;
    std::size_t entryCount() const noexcept;
    std::uint64_t estimatedRetainedBytes() const noexcept;
    Stats stats() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
