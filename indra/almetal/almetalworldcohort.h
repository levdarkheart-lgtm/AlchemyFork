/**
 * @file almetalworldcohort.h
 * @brief Sealed ordinary material source groups and one copied view per group.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetalworlddraws.h"
#include "almetalworldsourcepolicy.h"
#include "almetalworldvisibility.h"
#include <memory>
#include <span>
#include <string>

class ALMetalTexture;

struct ALMetalWorldKeyedSource
{
    ALMetalWorldSourceKey key;
    const ALMetalWorldSimpleBatch* prototype=nullptr;
};

struct ALMetalWorldScalarRecipe
{
    std::uint8_t family=0; // SIMPLE0, Legacy1, PBR2; no view/library state.
    bool normal_map=false,specular_map=false;
};

// One immutable generation, issued after ordinary static validation. Plain
// SIMPLE records contain no owning native fields and may be CPU-cached after
// their native generations expire; reuse must independently lock exact native
// generations. Active
// Material records require immutable numeric uniforms. A resource-table
// descriptor is optional for direct rendering, and must match when present.
// WorldVariants may share it with cohorts; idle producer layouts keep only a
// weak reference. It owns no parent publication or native-tuple aggregate.
class ALMetalWorldCohortSource final
{
public:
    static std::shared_ptr<const ALMetalWorldCohortSource> seal(
        const ALMetalWorldKeyedSource&,std::string* error=nullptr,
        const ALMetalWorldConservativeBounds* bounds=nullptr);
    ~ALMetalWorldCohortSource();
    ALMetalWorldCohortSource(const ALMetalWorldCohortSource&)=delete;
    ALMetalWorldCohortSource& operator=(const ALMetalWorldCohortSource&)=delete;
    const ALMetalWorldSourceKey& key() const noexcept;
    const ALMetalWorldSimpleBatch& source() const noexcept;
    std::uint32_t family() const noexcept;
    bool doubleSided() const noexcept;
    ALMetalWorldScalarRecipe scalarRecipe() const noexcept;
    bool hasBoundsPublication() const noexcept;
    const ALMetalWorldConservativeBounds& bounds() const noexcept;
    std::size_t ownedBytes() const noexcept;
private:
    struct Impl;
    struct Storage;
    explicit ALMetalWorldCohortSource(const Impl*) noexcept;
    const Impl* mImpl;
};

// Seal only completed immutable producer publications. Static prototypes and
// keys live in private per-generation records; the native tuple owns their
// exact native generations. Both owner identities are weak to avoid retaining
// a parent producer publication or adding idle COW readers. The view locks
// those owners explicitly before any description is accessed.
class ALMetalWorldSourceCohort final
{
public:
    using NativeTextureResolver=std::shared_ptr<const ALMetalTexture> (*)(
        const std::shared_ptr<const void>&,const ALMetalTexture*) noexcept;
    static std::shared_ptr<const ALMetalWorldSourceCohort> seal(
        std::span<const ALMetalWorldKeyedSource>,
        std::shared_ptr<const void> cpu_backing,
        std::shared_ptr<const void> native_tuple,
        std::uint64_t region_identity,std::string* error=nullptr,NativeTextureResolver=nullptr);
    // Completed producer generations are already immutable/validated. A
    // partially dirty group copies only these shared record references.
    static std::shared_ptr<const ALMetalWorldSourceCohort> seal(
        std::span<const std::shared_ptr<const ALMetalWorldCohortSource>>,
        std::shared_ptr<const void> cpu_backing,
        std::shared_ptr<const void> native_tuple,
        std::uint64_t region_identity,std::string* error=nullptr,NativeTextureResolver=nullptr);
    ~ALMetalWorldSourceCohort();
    std::size_t size() const noexcept;
    std::uint32_t family() const noexcept; // 0: SIMPLE, 1: Legacy, 2: PBR.
    bool doubleSided() const noexcept; // First source; use only if uniformDoubleSided().
    bool uniformDoubleSided() const noexcept;
    // Conservative retained descriptor closure, including each record; shared
    // records are not new physical allocations when another cohort reuses them.
    std::size_t metadataBytes() const noexcept;
    std::size_t ownedBytes() const noexcept;
    std::uint64_t regionIdentity() const noexcept;
    const ALMetalWorldSimpleBatch& source(std::size_t) const noexcept;
    const ALMetalWorldSourceKey& key(std::size_t) const noexcept;
    ALMetalWorldScalarRecipe scalarRecipe(std::size_t) const noexcept;
    // Cold exact comparison of immutable fields proves adjacent rows have
    // identical 400B common values under ONE actual cohort view. Camera,
    // clip/reverse state remain consumption-owned; no copied payload here.
    bool sameScalarCommonAsPrevious(std::size_t) const noexcept;
    std::size_t scalarCommonMetadataBytes() const noexcept;
    const ALMetalWorldConservativeBounds& bounds(std::size_t) const noexcept;
    std::size_t qualifiedBoundsCount() const noexcept;
    // Every source's optional bounds scan was attempted. Unknown/unqualified
    // results still complete this capability; warm callers must not retry them.
    bool hasBoundsPublication() const noexcept;
    // Union of valid qualified rows, built once at immutable publication. An
    // unqualified row never participates and must independently stay visible.
    const ALMetalWorldConservativeBounds& qualifiedBoundsUnion() const noexcept;
    bool matchesOwners(const std::shared_ptr<const void>& cpu,
                       const std::shared_ptr<const void>& native) const noexcept;
    // Cold optional GPU certification only. The completed producer returns an
    // alias of the original typed allocation, never of the tuple control block.
    std::shared_ptr<const ALMetalTexture> resolveNativeTexture(
        const std::shared_ptr<const void>& native,const ALMetalTexture*) const noexcept;
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
