/**
 * @file alnativesourceworkingset.h
 * @brief Bounded residency for producer-published native scene inputs.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include "alnativepublication.h"

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

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
