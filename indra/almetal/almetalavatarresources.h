/**
 * @file almetalavatarresources.h
 * @brief Avatar-owned native impostor publications shared by world views.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#pragma once

#include "almetalresource.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

// Immutable owner-copied body pose. It contains no joint/skin/mesh pointers
// and no view matrix: every main/probe/shadow consumer composes its own view.
class ALMetalAvatarPoseRecord final
{
public:
    static constexpr std::size_t max_joints = 15;
    static constexpr std::size_t palette_float_count = max_joints * 3 * 4;
    using Matrix = std::array<float, 16>;
    using Pivot = std::array<float, 3>;
    struct Inputs
    {
        std::array<Matrix, max_joints> world_matrices{};
        std::array<Pivot, max_joints> pivots{};
        std::uint32_t joint_count = 0;
        std::uint32_t pivot_count = 0;
        std::uint64_t frame = 0;
        std::uint64_t pose_revision = 0;
        std::uint64_t mesh_revision = 0;
        std::uint64_t lifecycle_revision = 0;
    };
    static std::shared_ptr<const ALMetalAvatarPoseRecord> create(
        Inputs inputs, std::string* error = nullptr);
    const Inputs& inputs() const noexcept { return mInputs; }
private:
    explicit ALMetalAvatarPoseRecord(Inputs inputs) : mInputs(std::move(inputs)) {}
    const Inputs mInputs;
};

// Single CPU owner, like the viewer's avatar publication path. Both world
// capture owners share this cache; changing views never changes avatar identity.
class ALMetalAvatarResources
{
public:
    using Key = std::array<std::uint8_t, 16>;

    struct Record
    {
        // Publication-owner identity. The encoder separately validates each
        // actual native device when binding these ordinary tracked resources.
        const ALMetalDevice* device = nullptr;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool hdr = false;
        bool normal_hdr = false;
        // Albedo(sRGB), ORM/specular, normal/flags, emissive.
        std::array<std::unique_ptr<ALMetalTexture>, 4> textures;
    };

    using Lease = std::shared_ptr<const Record>;

    // Caller has committed the producing bake before publishing: native
    // queue ordering, rather than this CPU cache, establishes texel readiness.
    // A complete, validated quartet replaces exactly one avatar. A rejected
    // publication leaves its previous record unchanged. No old generations
    // are retained here beyond leases held by queued/encoded consumers.
    bool publish(const Key& key, Record record, std::string* error = nullptr);

    // The lease pins all four wrappers while a capture queues their raw
    // pointers, even if that avatar is replaced or erased before encoding.
    Lease find(const Key& key, const ALMetalDevice& device,
               bool hdr, bool normal_hdr) const noexcept;
    void erase(const Key& key) noexcept;
    void clear() noexcept;
    // Safe on a new view owner's first use: compatible shared publications
    // survive, while a real device/format change retires incompatible ones.
    void discardIncompatible(const ALMetalDevice& device,
                             bool hdr, bool normal_hdr) noexcept;
    std::size_t size() const noexcept;

private:
    struct Hash
    {
        std::size_t operator()(const Key& key) const noexcept;
    };
    std::unordered_map<Key, Lease, Hash> mRecords;
};
