/**
 * @file almetalworldsourcepages.h
 * @brief Bounded native source arguments and actual-view generation leases.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetalworldresources.h"
#include "almetalworlddraws.h"
#include <span>
#include <functional>

struct ALMetalWorldSimpleBatch;
struct ALMetalWorldSourceCohortView;
class ALMetalWorldFrameData;
class ALMetalWorldAnimatedSource;
class ALMetalAttachmentPaletteRecord;

class ALMetalWorldAnimatedGpuSource;

inline constexpr std::size_t alMetalWorldSourceMaximumRows = 4096;
inline constexpr std::size_t alMetalWorldSourceRowsPerPage = 128;

struct alignas(16) ALMetalWorldSourceRegion
{
    float modelview_projection[16]{};
    float modelview[16]{};
    float normal_matrix[3][4]{};
};
static_assert(sizeof(ALMetalWorldSourceRegion) == 176);
struct alignas(16) ALMetalWorldSourceGlobals
{
    float mirror_clip_plane[4]{0,0,0,1};
    std::uint32_t flags[4]{}; // reverse Z, mirror clip, normal HDR, reserved
};
static_assert(sizeof(ALMetalWorldSourceGlobals) == 32);

struct ALMetalWorldSourceKey
{
    std::uint64_t identity = 0, flavor = 0, revision = 0;
};

struct ALMetalWorldSourceIndexedCall
{
    ALMetalWorldSourceKey key;
    void* native_indices=nullptr;
    std::size_t index_offset=0;
    std::uint32_t index_count=0;
    ALMetalIndexType index_type=ALMetalIndexType::uint16;
    std::uint32_t row_id=0,row_generation=0;
};
struct ALMetalWorldSourceCohortRange
{
    std::size_t logical_begin=0,first=0,count=0;
};
struct ALMetalWorldSourceCommandRun
{
    std::size_t first=0,count=0;
    bool double_sided=false;
};
struct ALMetalWorldSourceDirectBinding
{
    std::array<void*,7> geometry{};
    std::array<std::size_t,7> offsets{};
    void* legacy_numeric=nullptr;
    void* pbr_ldr=nullptr;
    void* pbr_hdr=nullptr;
    void* resource_table=nullptr;
    std::size_t policy_offset=0;
    std::uint32_t region=0;
};
struct ALMetalWorldSourceNativeFootprint
{
    void* allocation=nullptr;
    std::uint64_t bytes=0;
};
struct ALMetalWorldAnimatedNativeBinding
{
    ALMetalWorldSourceKey key;
    std::array<void*,8> geometry{}; // position/normal/uv/color/tangent/uv1/uv2/weight4
    std::array<std::size_t,8> offsets{};
    std::array<void*,3> numeric{}; // legacy, PBR LDR, PBR normal-HDR
    void* indices=nullptr;
    std::size_t index_offset=0;
    std::uint32_t index_count=0,bone_count=0;
    ALMetalIndexType index_type=ALMetalIndexType::uint16;
    const ALMetalBuffer* policy=nullptr;
    const ALMetalBuffer* bones=nullptr;
    std::size_t bone_offset=0;
    bool pbr=false,double_sided=false,qualified=false;
};
// Native-only immutable GPU publication. It owns its metadata allocations and
// table, never the producer/source/pose or real C++ geometry generation.
class ALMetalWorldAnimatedGpuSource final
{
public:
    ~ALMetalWorldAnimatedGpuSource();
    const ALMetalWorldAnimatedNativeBinding& binding() const noexcept;
    std::shared_ptr<const ALMetalWorldResourceTable> table() const noexcept;
    std::span<const ALMetalWorldSourceNativeFootprint> assets() const noexcept;
    // Idle metadata proof only. Already prepared commands own independent
    // native generations and remain valid after CPU publication retirement.
    bool currentGeneration() const noexcept;
    bool matchesNativeLease(const std::shared_ptr<const void>&) const noexcept;
    bool declareResources(ALMetalRenderEncoder&,ALMetalCommandBuffer&,std::string* error=nullptr) const;
private:
    struct Impl;
    explicit ALMetalWorldAnimatedGpuSource(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
    friend class ALMetalWorldSourcePages;
};
// CPU preparation only. Native command owners extract only source and native;
// the late pose and matrix copy are discarded after synchronous encoding.
struct ALMetalWorldAnimatedGpuView
{
    std::shared_ptr<const ALMetalWorldAnimatedGpuSource> source;
    std::shared_ptr<const ALMetalAttachmentPaletteRecord> pose;
    std::shared_ptr<const void> native;
    ALMetalWorldViewMatrices view;
};
// Fixed-address native storage. Cached commands may retain this storage;
// only an actual FrameView holds its separate overwrite-prevention token.
class ALMetalWorldSourceDirectSlot final
{
public:
    ~ALMetalWorldSourceDirectSlot();
    void* nativeBuffer() const noexcept;
    std::size_t regionOffset(std::uint32_t) const noexcept;
    std::size_t globalsOffset() const noexcept;
private:
    struct Impl;
    explicit ALMetalWorldSourceDirectSlot(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
    friend class ALMetalWorldSourcePages;
};

// Issued only by successful native publication. Immutable numeric metadata,
// never a CPU cohort, C++ source generation or actual reader-token owner.
class ALMetalWorldSourceCommandMembership final
{
public:
    ~ALMetalWorldSourceCommandMembership();
    std::span<const ALMetalWorldSourceIndexedCall> calls() const noexcept;
    std::span<const ALMetalWorldSourceCommandRun> runs() const noexcept;
    void* nativeDevice() const noexcept;
    std::span<const ALMetalWorldSourceDirectBinding> directBindings() const noexcept;
    std::span<const ALMetalWorldSourceNativeFootprint> directAssets() const noexcept;
    void* directPolicyBuffer() const noexcept;
private:
    struct Impl;
    explicit ALMetalWorldSourceCommandMembership(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
    friend class ALMetalWorldSourcePages;
};

// Supplied by the completed native producer. owner is a native-only aggregate
// holding the exact C++ vertex/index/texture generations addressed by source;
// it must never alias a SourcePayload/LL object. Optional typed geometry leases
// allow callers already holding typed generations to prove their match here.
struct ALMetalWorldSourceNativeScope
{
    std::shared_ptr<const void> owner;
    std::shared_ptr<const ALMetalBuffer> vertices, indices;
};
struct ALMetalWorldSourceRequest
{
    ALMetalWorldSourceKey key;
    const ALMetalWorldSimpleBatch* source = nullptr;
    // May be empty for a synchronous borrowed Scene. Nonempty immutable CPU
    // owners remain local to freeze and are never retained by the GPU view.
    std::shared_ptr<const void> cpu_owner;
    ALMetalWorldSourceNativeScope native;
    std::uint32_t region = 0;
};
struct ALMetalWorldSourcePageLimits
{
    std::size_t maximum_rows = alMetalWorldSourceMaximumRows;
    std::uint64_t maximum_metadata_bytes = 32ULL * 1024 * 1024;
    // Cache entries are ordered compositions; total retained calls/metadata
    // still bound the underlying cohort references and reverse-row nodes.
    std::size_t maximum_cached_cohorts=128,maximum_cached_cohort_rows=8192;
    std::size_t maximum_cached_cohort_metadata_bytes=8*1024*1024;
    // Zero resolves to the existing hardware-derived source-working-set cap.
    std::uint64_t maximum_cached_cohort_native_asset_bytes=0;
};
struct ALMetalWorldSourcePageStats
{
    std::uint64_t requests=0,hits=0,row_publications=0,failures=0;
    std::uint64_t encoded_row_bytes=0,encoded_mapping_bytes=0,copied_page_bytes=0,copied_bank_bytes=0;
    std::uint64_t page_allocations=0,bank_allocations=0,view_bytes=0,binds=0;
    // Actual padded CPU copies into FrameData; arena physical pages remain
    // in FrameData accounting, separate from the standalone view buffers.
    std::uint64_t arena_view_bytes=0,arena_views=0;
    std::size_t rows=0,live_metadata_bytes=0;
    std::uint64_t admission_requests=0,admission_denials=0,admitted_bytes=0;
    // Unique native buffers per freeze, not attributes/draws. A proof hit
    // reuses a live weak native object's immutable device, without querying it.
    std::uint64_t device_checks=0,device_proof_hits=0,device_rejections=0;
    std::size_t device_proof_entries=0;
    std::uint64_t cohort_requests=0,cohort_hits=0,cohort_builds=0,cohort_failures=0;
    std::size_t cached_compositions=0,cached_cohorts=0,cached_cohort_rows=0,cached_cohort_metadata_bytes=0;
    // Gross unique root texture allocations held by this idle cache; overlaps
    // other active owners and must not be added to a physical memory total.
    std::uint64_t cached_cohort_native_asset_bytes=0,cached_cohort_native_asset_limit=0;
    std::uint64_t direct_policy_bytes=0,direct_slot_allocations=0,direct_slot_hits=0,direct_view_bytes=0;
    std::uint64_t animated_source_publications=0,animated_source_hits=0;
    std::size_t live_direct_slots=0,live_direct_slot_bytes=0;
};

// Logical encoder ranges and exact CPU copy/zero operations are separate
// quantities. Their checked sum is the publication-work admission unit, not
// unique physical RAM or measured driver/GPU memory traffic. Allocation bytes
// include retained composition-map allocations, exclude required per-view
// camera/global/ephemeral-map buffers, and are not added to the work sum twice.
struct ALMetalWorldSourcePublicationWork
{
    std::uint64_t encoded_row_bytes=0,encoded_table_bytes=0,encoded_bank_range_bytes=0,encoded_mapping_bytes=0;
    std::uint64_t copied_page_bytes=0,copied_bank_bytes=0,zeroed_page_bytes=0,zeroed_bank_bytes=0;
    std::uint64_t allocation_bytes=0;
    std::uint64_t chargedBytes() const noexcept;
};

class ALMetalWorldSourceFrameView final : public std::enable_shared_from_this<ALMetalWorldSourceFrameView>
{
public:
    ~ALMetalWorldSourceFrameView();
    std::size_t size() const noexcept;
    std::uint32_t rowId(std::size_t request_index) const noexcept;
    std::uint32_t allocationGeneration(std::size_t request_index) const noexcept;
    std::shared_ptr<const ALMetalWorldSourceCommandMembership> commandMembership() const noexcept;
    // Cumulative native call offsets, retaining original logical gaps/order.
    std::span<const ALMetalWorldSourceCohortRange> cohortRanges() const noexcept;
    bool directBindings() const noexcept;
    bool directNormalHDR() const noexcept;
    std::uint64_t directBindingAssetLimit() const noexcept;
    std::shared_ptr<const ALMetalWorldSourceDirectSlot> directBindingSlot() const noexcept;
    std::span<const std::shared_ptr<const ALMetalWorldResourceTable>> directBindingTables() const noexcept;
    // Includes the exact index generation whose native device freeze proved.
    bool matchesSourceCommand(std::size_t request_index,const ALMetalWorldSourceKey&,
                              const ALMetalBuffer& indices) const noexcept;
    // Weak diagnostic observation of a unique table lease (not draw order).
    // Does not add a publisher/native generation pin to the source cache.
    std::weak_ptr<const ALMetalWorldResourceTable> tableGenerationForTesting(std::size_t unique_table_index=0) const noexcept;
    // Binds bank0 VS/FS, regions1 VS, globals2 VS/FS and source_regions3 VS.
    // Retains this native-only view before exposing any indirect resource.
    bool bind(ALMetalRenderEncoder&, ALMetalCommandBuffer&, std::string* error=nullptr) const;
private:
    struct Impl;
    explicit ALMetalWorldSourceFrameView(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
    friend class ALMetalWorldSourcePages;
    friend class ALMetalWorldSourceCommands;
    bool matchesDirectCommand(const ALMetalCommandBuffer&) const noexcept;
};

// Publication owner only. Persistent rows contain weak C++ generation proofs;
// GPU buffers/cache references are not real readers. freeze acquires reader
// tokens and exact native/table leases BEFORE the caller queues CPU work.
class ALMetalWorldSourcePages final
{
public:
    using PublicationAdmission=std::function<std::shared_ptr<const void>(const ALMetalWorldSourcePublicationWork&)>;
    static std::shared_ptr<ALMetalWorldSourcePages> create(
        const ALMetalDevice&, const ALMetalShaderLibrary&, std::uint32_t max_anisotropy=1,
        ALMetalWorldSourcePageLimits={}, std::string* error=nullptr);
    ~ALMetalWorldSourcePages();
    bool supported() const noexcept;
    // Coalesced cold GPU source publication; late poses stay CPU-local here.
    // Native-only cache entries reuse the existing metadata/asset bounds.
    bool prepareAnimatedSourcesForCommand(ALMetalCommandBuffer&,
        std::span<const ALMetalWorldSourceReference* const>,
        std::vector<ALMetalWorldAnimatedGpuView>& output,std::string* error=nullptr);
    bool belongsTo(const ALMetalDevice&, const ALMetalShaderLibrary&, std::uint32_t max_anisotropy) const noexcept;
    // Requests contain only selected eligible Legacy/PBR rows. Any failed row
    // rejects this whole optional view; the required scalar path stays intact.
    // Dynamic region selection is separate from immutable source signatures.
    std::shared_ptr<const ALMetalWorldSourceFrameView> freeze(
        std::span<const ALMetalWorldSourceRequest>, std::span<const ALMetalWorldSourceRegion>,
        const ALMetalWorldSourceGlobals&, std::string* error=nullptr);
    std::shared_ptr<const ALMetalWorldSourceFrameView> freezeForCommand(
        ALMetalCommandBuffer&, std::span<const ALMetalWorldSourceRequest>,
        std::span<const ALMetalWorldSourceRegion>, const ALMetalWorldSourceGlobals&,
        std::string* error=nullptr);
    // Optional FrameData publication is synchronous and command-scoped: its
    // native-only view may bind only on that exact incomplete command. Null
    // retains the standalone immutable-buffer lifetime.
    std::shared_ptr<const ALMetalWorldSourceFrameView> freezeCohortForCommand(
        ALMetalCommandBuffer&,const ALMetalWorldSourceCohortView&,
        const ALMetalWorldSourceGlobals&,std::string* error=nullptr,ALMetalWorldFrameData* frame_data=nullptr);
    // Homogeneous ordered pass. Only cold publication expands static sources;
    // warm qualification visits cohort owners/views. Equal region identities
    // require equal176B transforms; a source row cannot select two regions.
    std::shared_ptr<const ALMetalWorldSourceFrameView> freezeCohortsForCommand(
        ALMetalCommandBuffer&,std::span<const ALMetalWorldSourceCohortView>,
        const ALMetalWorldSourceGlobals&,std::string* error=nullptr,ALMetalWorldFrameData* frame_data=nullptr,
        bool direct_bindings=false);
    // Product owner injection only; generic/native fixtures default to no
    // policy. Dirty standalone freeze refuses an active policy (no command
    // can own its credit); fully reused standalone views remain legal.
    void setPublicationAdmission(PublicationAdmission);
    // Called synchronously by the owner before producer backing COW. No GPU
    // writes, allocation, LL callbacks or reader reclamation occurs here.
    void invalidate(std::uint64_t producer_identity, std::uint64_t minimum_revision=0) noexcept;
    // Cache-only loss changes availability, not the authoritative revision.
    void withdraw(std::uint64_t producer_identity) noexcept;
    void clear() noexcept;
    // Existing frame owner calls this even while the optional GPU path is off.
    void trimUnusedCohorts() noexcept;
    ALMetalWorldSourcePageStats stats() const noexcept;
    void failNextPublicationForTesting() noexcept;
    void failNextCohortCacheInsertionForTesting() noexcept;
private:
    std::shared_ptr<const ALMetalWorldSourceFrameView> freezeInternal(
        ALMetalCommandBuffer*, std::span<const ALMetalWorldSourceRequest>,
        std::span<const ALMetalWorldSourceRegion>, const ALMetalWorldSourceGlobals&,
        std::string*, std::span<const std::shared_ptr<const ALMetalWorldResourceSource>> overrides={},
        bool certified_cohort=false,ALMetalWorldFrameData* frame_data=nullptr,std::size_t direct_policy_bytes=0);
    struct Impl;
    explicit ALMetalWorldSourcePages(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};
