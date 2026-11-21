/**
 * @file almetalworldanimated.h
 * @brief Immutable regular-rigged native sources and late attachment poses.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetalworldsourcepolicy.h"
#include "almetalworldcohort.h"
#include <memory>
#include <span>

struct ALMetalWorldSimpleBatch;
// Issue once from a completed geometry/material publication. Pose/view/UV
// animation never enter this private static generation. The caller leases the
// exact native tuple separately; neither a CPU producer nor its pose may be
// retained by native command completion.
class ALMetalWorldAnimatedSource final
{
public:
    static std::shared_ptr<const ALMetalWorldAnimatedSource> seal(
        ALMetalWorldSourceKey,const ALMetalWorldSimpleBatch& pose_free,
        const ALMetalWorldSourceNativeScope&,std::string* error=nullptr);
    ~ALMetalWorldAnimatedSource();
    const ALMetalWorldSimpleBatch& prototype() const noexcept;
    const ALMetalWorldSourceKey& key() const noexcept;
    std::shared_ptr<const ALMetalWorldAnimatedSource> owner() const noexcept;
    bool matchesNativeScope(const ALMetalWorldSourceNativeScope&) const noexcept;
    // Exact owner proof without constructing temporary typed generation leases.
    // Callers separately compare their vertex/index wrapper identities.
    bool matchesNativeOwner(const std::shared_ptr<const void>&) const noexcept;
    std::shared_ptr<const void> nativeLease() const noexcept;
    ALMetalWorldScalarRecipe scalarRecipe() const noexcept;
    // Exact immutable common policy, excluding matrices supplied by Draw.
    bool sameScalarCommon(const ALMetalWorldAnimatedSource&) const noexcept;
    std::size_t ownedBytes() const noexcept;
private:
    struct Impl;
    explicit ALMetalWorldAnimatedSource(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};

struct ALMetalWorldAnimatedStats
{
    // Ordinary shader consumption of private rigged source references.
    std::uint64_t direct_draws=0,direct_recipe_reads=0;
    std::uint64_t direct_common_builds=0,direct_common_reuses=0;
    // Per-render private-record cache. Publications count successful
    // palette+joint-count slice acquisition pairs; palette bytes may intern.
    // Hits still ask the encoder to restore slots5/7, including after queries.
    std::uint64_t direct_pose_cache_hits=0,direct_pose_publications=0;
};
