/**
 * @file almetalshadowpublication.h
 * @brief Producer-sealed ordered native shadow membership and resource union.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include "almetalshadowprepare.h"
#include "../llcommon/alnativepublication.h"
#include "../llcommon/alnativesourceworkingset.h"
#include "../llcommon/alnativeframebudget.h"

// Source and membership operations are publication-owner only. Detached jobs
// receive immutable records and copied view values, never these producer States.
struct ALMetalShadowMembershipSource
{
    std::shared_ptr<ALNativePublication::State> producer;
    // From the exact completed source payload, BEFORE a callback can mutate it.
    std::uint64_t identity = 0, revision = 0;
    std::shared_ptr<const ALMetalShadowSourceRecord> record;
};

struct ALMetalShadowMembershipRange
{
    std::uint32_t pass = 0;
    ALMetalWorldSpotShadowCoverage coverage = ALMetalWorldSpotShadowCoverage::opaque;
    std::size_t first = 0, count = 0;
    bool operator==(const ALMetalShadowMembershipRange&) const = default;
};

// One logical CPU debt domain shared by the existing scene owner. A payload's
// RAII reservation survives cache eviction and cache/publisher destruction,
// and retires only with its LAST reader. Native-only completion owners drop
// row/Record debt but retain the smaller resource-container metadata debt.
// This is not a physical UMA allocation total.
class ALMetalShadowMembershipAccounting final
{
public:
    explicit ALMetalShadowMembershipAccounting(std::uint64_t maximum_bytes) noexcept : mMaximum(maximum_bytes) {}
    std::uint64_t bytes() const noexcept { return mBytes.load(std::memory_order_relaxed); }
    std::uint64_t maximumBytes() const noexcept { return mMaximum; }
private:
    friend struct ALMetalShadowMembershipReservation;
    const std::uint64_t mMaximum;
    std::atomic<std::uint64_t> mBytes{0};
};

class ALMetalShadowMembership final
{
public:
    struct Member
    {
        std::weak_ptr<ALNativePublication::State> producer;
        std::uint64_t identity = 0, revision = 0;
        std::shared_ptr<const ALMetalShadowSourceRecord> record;
    };
    std::span<const Member> members() const noexcept { return mMembers; }
    std::span<const ALMetalShadowMembershipRange> ranges() const noexcept { return mRanges; }
    std::span<const ALNativeSourceWorkingSet::Resource> resources() const noexcept;
    // Native-only completion lease: no Member/SourceRecord/pose/viewer owner.
    // The CPU encoder separately retains this Membership until it joins.
    std::shared_ptr<const void> nativeOwner() const noexcept { return mResources; }
    std::uint64_t serial() const noexcept { return mSerial; }
    std::uint64_t metadataBytes() const noexcept { return mMetadataBytes; }
    std::uint64_t nativeMetadataBytes() const noexcept { return mNativeMetadataBytes; }
private:
    friend class ALMetalShadowMembershipPublisher;
    // Declared first: release the reservation LAST, after rows/Record owners
    // and copied storage have actually retired, even if destruction reenters.
    std::shared_ptr<const void> mCPUReservation;
    std::vector<Member> mMembers;
    std::vector<ALMetalShadowMembershipRange> mRanges;
    struct NativeResources;
    std::shared_ptr<const NativeResources> mResources;
    std::uint64_t mSerial = 0, mRevision = 0, mMetadataBytes = 0, mNativeMetadataBytes = 0;
};

class ALMetalShadowMembershipPublisher final
{
public:
    enum class Outcome { unavailable, reused, retained, transient };
    enum class Failure
    {
        none, invalid_ranges, invalid_source, changed_source, bounds,
        admission, cpu_residency, resource_union, allocation, publication
    };
    struct Result
    {
        std::shared_ptr<const ALMetalShadowMembership> membership;
        Outcome outcome = Outcome::unavailable;
        Failure failure = Failure::none;
        // Work completed by THIS transaction, never a shared before/after
        // Publisher delta that may include a nested reentrant transaction.
        bool sealed_resource_union = false;
        explicit operator bool() const noexcept { return bool(membership); }
    };
    struct Limits
    {
        std::size_t maximum_members = 131072;
        std::size_t maximum_ranges = 1024;
        // Checked copied rows/ranges/resource-vector capacities for ONE cached
        // owner publication. Native resources use the existing global ledger.
        // Command-held readers are separately bounded by existing view owners.
        std::uint64_t maximum_metadata_bytes = 32ULL * 1024 * 1024;
    };
    struct Stats
    {
        std::uint64_t sealed = 0, reused = 0, transient = 0;
        std::uint64_t resource_unions = 0, declined = 0;
    };
    ALMetalShadowMembershipPublisher();
    explicit ALMetalShadowMembershipPublisher(Limits);
    ~ALMetalShadowMembershipPublisher();
    ALMetalShadowMembershipPublisher(const ALMetalShadowMembershipPublisher&) = delete;
    ALMetalShadowMembershipPublisher& operator=(const ALMetalShadowMembershipPublisher&) = delete;

    // The caller must supply the CURRENT ordered cull membership; no reuse is
    // inferred from frame/camera/cull pointers. On an authoritative membership
    // mutation, call invalidate BEFORE testing resource reuse/COW. Child source
    // changes independently invalidate this publisher through dependency edges.
    void invalidate() noexcept;
    Result seal(std::span<const ALMetalShadowMembershipSource>,
        std::span<const ALMetalShadowMembershipRange>,
        ALNativeSourceWorkingSet&, ALNativeFrameBudget&,
        std::uint64_t& source_serial, std::uint32_t frame,
        ALNativeFrameBudget::Frame oldest_dirty_frame);
    Stats stats() const noexcept { return mStats; }
private:
    friend class ALMetalShadowMembershipCache;
    class Dependencies;
    std::shared_ptr<ALNativePublication::State> mState;
    std::shared_ptr<ALNativePublication::Observer> mDependencies;
    std::shared_ptr<ALMetalShadowMembershipAccounting> mAccounting;
    Limits mLimits;
    Stats mStats;
};

// Scene-owner typed cache, keyed by the EXISTING immutable group-membership
// State identity and a pass/coverage flavor. It never owns that scene State or
// an LLSpatialGroup. Its sole publication leases remain the derived States
// tracked by ALNativeSourceWorkingSet. One cache instance belongs to the normal
// scene owner, shared across directional/projector/probe shadow views.
class ALMetalShadowMembershipCache final
{
public:
    using Result = ALMetalShadowMembershipPublisher::Result;
    struct Limits
    {
        std::size_t maximum_entries = 8192;
        std::uint64_t maximum_metadata_bytes = 32ULL * 1024 * 1024;
        ALMetalShadowMembershipPublisher::Limits publication;
    };
    struct Stats
    {
        std::uint64_t hits = 0, misses = 0, evictions = 0, resource_unions = 0;
        std::size_t entries = 0;
        std::uint64_t metadata_bytes = 0;
    };
    ALMetalShadowMembershipCache();
    explicit ALMetalShadowMembershipCache(Limits);
    ALMetalShadowMembershipCache(Limits,
        std::shared_ptr<ALMetalShadowMembershipAccounting> scene_accounting);
    ~ALMetalShadowMembershipCache();
    ALMetalShadowMembershipCache(const ALMetalShadowMembershipCache&) = delete;
    ALMetalShadowMembershipCache& operator=(const ALMetalShadowMembershipCache&) = delete;
    // O(log bounded entries), no source-row scan/factory/native union walk.
    // Call BEFORE resolving LLDrawInfo rows, but only after the ordinary cull,
    // render-type, group validity and exact span-membership decisions.
    Result current(const std::shared_ptr<ALNativePublication::State>& membership_owner,
        std::uint64_t flavor, ALNativeSourceWorkingSet&, std::uint32_t frame);
    Result seal(const std::shared_ptr<ALNativePublication::State>& membership_owner,
        std::uint64_t flavor, std::span<const ALMetalShadowMembershipSource>,
        std::span<const ALMetalShadowMembershipRange>, ALNativeSourceWorkingSet&,
        ALNativeFrameBudget&, std::uint64_t& serial, std::uint32_t frame,
        ALNativeFrameBudget::Frame oldest_dirty_frame);
    void clear() noexcept;
    Stats stats() const noexcept;
    std::shared_ptr<const ALMetalShadowMembershipAccounting> accounting() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

struct ALMetalShadowMembershipViewInput
{
    // Guard against stale or reordered owner-side facts. Generation is the
    // actual native Record generation, not a simulation or camera frame stamp.
    std::uint64_t source_generation = 0;
    const ALMetalAttachmentPaletteRecord* palette = nullptr;
    std::array<float,16> model = alMetalShadowIdentity;
    std::array<float,16> texture_matrix = alMetalShadowIdentity;
    bool has_model = false, has_texture_matrix = false;
};

// Gather into a slice of the EXISTING family input packet without dispatch.
// This keeps many small producer groups in one >=8192 family preparation join.
// texture_base_slot indexes the caller's whole immutable context UV array;
// texture_scratch is that array's available slice. No native/scene owner is
// transferred here: caller retains membership/current palettes through join.
bool alMetalGatherShadowMembershipRange(const ALMetalShadowMembership&,
    std::size_t range_index, std::span<const ALMetalShadowMembershipViewInput>,
    std::span<ALMetalShadowPrepareInstance> instance_scratch,
    std::span<std::array<float,16>> texture_scratch, std::uint32_t texture_base_slot,
    std::uint64_t epoch, std::size_t& texture_count, std::string* error = nullptr);

// Existing CPU capture owner must retain membership and CURRENT palettes
// through the encoding join. Command completion retains nativeOwner() and
// independently required copied palette/page resources. This adds no queue,
// wait, draw merge, cull, pose cache or query operation. Reusable owner scratch
// is consumed synchronously; small/denied work executes the existing serial
// kernel. A failure clears the entire output, never exposes a prepared prefix.
// allow_dispatch=false also preserves the ordinary small-output validation
// contract: borrow coverage/current palette, not an optional SourceRecord proof.
bool alMetalPrepareShadowMembershipRange(const ALMetalShadowMembership&,
    std::size_t range_index, std::span<const ALMetalShadowMembershipViewInput>,
    const ALMetalShadowPrepareContext&,
    std::span<ALMetalShadowPrepareInstance> instance_scratch,
    std::span<std::array<float,16>> texture_scratch,
    std::span<ALMetalWorldSpotShadowBatch> output,
    ALNativeFrameBudget&, std::uint64_t epoch,
    const std::atomic<std::uint64_t>* current_epoch = nullptr,
    bool allow_dispatch = false, ALMetalShadowPrepareStats* stats = nullptr,
    std::string* error = nullptr);
