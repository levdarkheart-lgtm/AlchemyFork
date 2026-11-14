/**
 * @file almetalworldcohort.h
 * @brief Sealed ordinary material source groups and one copied view per group.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetalworlddraws.h"
#include "almetalworldsourcepages.h"
#include <memory>
#include <span>
#include <string>

struct ALMetalWorldKeyedSource
{
    ALMetalWorldSourceKey key;
    const ALMetalWorldSimpleBatch* prototype=nullptr;
};

// Seal only completed immutable producer publications. Static prototypes and
// keys are copied once into private storage; the native tuple owns their
// exact native generations. Both owner identities are weak to avoid retaining
// a parent producer publication or adding idle COW readers. The view locks
// those owners explicitly before any description is accessed.
class ALMetalWorldSourceCohort final
{
public:
    static std::shared_ptr<const ALMetalWorldSourceCohort> seal(
        std::span<const ALMetalWorldKeyedSource>,
        std::shared_ptr<const void> cpu_backing,
        std::shared_ptr<const void> native_tuple,
        std::uint64_t region_identity,std::string* error=nullptr);
    ~ALMetalWorldSourceCohort();
    std::size_t size() const noexcept;
    std::uint32_t family() const noexcept; // 1: Legacy, 2: PBR.
    bool doubleSided() const noexcept; // First source; use only if uniformDoubleSided().
    bool uniformDoubleSided() const noexcept;
    std::size_t metadataBytes() const noexcept;
    std::size_t ownedBytes() const noexcept;
    std::uint64_t regionIdentity() const noexcept;
    const ALMetalWorldSimpleBatch& source(std::size_t) const noexcept;
    const ALMetalWorldSourceKey& key(std::size_t) const noexcept;
    bool matchesOwners(const std::shared_ptr<const void>& cpu,
                       const std::shared_ptr<const void>& native) const noexcept;
    static bool validView(const ALMetalWorldViewMatrices&) noexcept;
private:
    struct Impl;
    explicit ALMetalWorldSourceCohort(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};

struct ALMetalWorldSourceCohortView
{
    // Cohorts insert their source rows between the ordinary raw/ref tokens.
    // logical_begin equals base_before plus all preceding cohort row counts.
    std::size_t logical_begin=0,base_before=0;
    std::shared_ptr<const ALMetalWorldSourceCohort> cohort;
    std::shared_ptr<const void> cpu_owner,native_owner;
    ALMetalWorldViewMatrices view;
};
