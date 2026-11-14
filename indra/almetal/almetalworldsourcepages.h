/**
 * @file almetalworldsourcepages.h
 * @brief Bounded native source arguments and actual-view generation leases.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetalworldresources.h"
#include <span>
#include <functional>

struct ALMetalWorldSimpleBatch;

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
};
struct ALMetalWorldSourcePageStats
{
    std::uint64_t requests=0,hits=0,row_publications=0,failures=0;
    std::uint64_t encoded_row_bytes=0,copied_page_bytes=0,copied_bank_bytes=0;
    std::uint64_t page_allocations=0,bank_allocations=0,view_bytes=0,binds=0;
    std::size_t rows=0,live_metadata_bytes=0;
    std::uint64_t admission_requests=0,admission_denials=0,admitted_bytes=0;
};

// Logical encoder ranges and exact CPU copy/zero operations are separate
// quantities. Their checked sum is the publication-work admission unit, not
// unique physical RAM or measured driver/GPU memory traffic. Allocation bytes
// exclude required per-view region/global/map buffers and are not added twice.
struct ALMetalWorldSourcePublicationWork
{
    std::uint64_t encoded_row_bytes=0,encoded_table_bytes=0,encoded_bank_range_bytes=0;
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
    ALMetalWorldSourcePageStats stats() const noexcept;
    void failNextPublicationForTesting() noexcept;
private:
    std::shared_ptr<const ALMetalWorldSourceFrameView> freezeInternal(
        ALMetalCommandBuffer*, std::span<const ALMetalWorldSourceRequest>,
        std::span<const ALMetalWorldSourceRegion>, const ALMetalWorldSourceGlobals&,
        std::string*);
    struct Impl;
    explicit ALMetalWorldSourcePages(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};
