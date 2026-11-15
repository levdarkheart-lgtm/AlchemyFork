/**
 * @file almetalworldanimated.h
 * @brief Immutable regular-rigged native sources and late attachment poses.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetalworldsourcepages.h"
#include <memory>
#include <span>

struct ALMetalWorldSimpleBatch;
// Each row encloses indexed bind-space vertices influenced by this joint.
// The current pose clamps the joint to its actual count, as objectSkinV does.
struct alignas(16) ALMetalWorldAnimatedBoneBounds
{
    float minimum[4]{},maximum[4]{};
    std::uint32_t config[4]{}; // joint, reserved
};
static_assert(sizeof(ALMetalWorldAnimatedBoneBounds)==48);

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
    std::shared_ptr<const void> nativeLease() const noexcept;
    std::span<const ALMetalWorldAnimatedBoneBounds> boneBounds() const noexcept;
    bool boundsQualified() const noexcept;
    std::size_t ownedBytes() const noexcept;
private:
    struct Impl;
    explicit ALMetalWorldAnimatedSource(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};

struct ALMetalWorldAnimatedStats
{
    // Successfully encoded views only; GPU draw/cull completion is separate.
    std::uint64_t requested_draws=0,gpu_draws=0,fallback_draws=0,execute_calls=0;
    std::uint64_t current_pose_publications=0,source_publications=0;
    std::uint64_t bounds_unknown=0,bounds_tests=0;
    // Actual compute dispatches in successfully encoded view transactions;
    // full-count mode performs no bounds work or visibility dispatch.
    std::uint64_t visibility_dispatches=0;
};
