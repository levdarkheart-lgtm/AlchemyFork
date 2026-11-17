/**
 * @file almetalshadowprepare.h
 * @brief Owned shadow inputs and bounded native CPU preparation.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include "almetalworld.h"

#include <atomic>
#include <limits>
#include <span>

class ALNativeFrameBudget;
class ALMetalShadowSourceRecipe;

// Published by the render owner after resolving mutable viewer state. A lease
// must exclude backing-store reuse, not only keep an Objective-C name alive.
// This record deliberately has no LLDrawInfo, avatar, cull or matrix pointers.
class ALMetalShadowSourceRecord final
{
public:
    static std::shared_ptr<const ALMetalShadowSourceRecord> create(
        std::uint64_t generation,
        const ALMetalWorldSpotShadowBatch& prototype,
        std::shared_ptr<const ALMetalBuffer> vertices,
        std::shared_ptr<const ALMetalBuffer> indices,
        std::vector<std::shared_ptr<const ALMetalTexture>> textures,
        std::string* error = nullptr);

    ALMetalShadowSourceRecord(const ALMetalShadowSourceRecord&) = delete;
    ALMetalShadowSourceRecord& operator=(const ALMetalShadowSourceRecord&) = delete;
    std::uint64_t generation() const noexcept { return mGeneration; }
    const ALMetalWorldSpotShadowBatch& prototype() const noexcept { return mPrototype; }
    bool requiresPalette() const noexcept { return mRequiresPalette; }
    const std::shared_ptr<const ALMetalBuffer>& vertexLease() const noexcept { return mVertices; }
    const std::shared_ptr<const ALMetalBuffer>& indexLease() const noexcept { return mIndices; }
    std::span<const std::shared_ptr<const ALMetalTexture>> textureLeases() const noexcept { return mTextures; }
    // Conservative record-owned CPU allocation charge, including reserved
    // texture slots and immutable coverage. Native resources have separate
    // working-set leases; this is neither their bytes nor physical RAM.
    std::uint64_t ownedBytes() const noexcept
    {
        constexpr std::uint64_t shared_control_block = 4 * sizeof(void*);
        const std::uint64_t fixed = sizeof(*this) + shared_control_block +
            (mPrototype.sharedCoverageData()
                ? sizeof(ALMetalWorldSpotShadowBatch::CoverageData) + shared_control_block : 0);
        constexpr std::uint64_t slot = sizeof(std::shared_ptr<const ALMetalTexture>);
        constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
        if (mTextures.capacity() > (maximum - fixed) / slot) return maximum;
        return fixed + static_cast<std::uint64_t>(mTextures.capacity()) * slot;
    }
    // Cheap certification of the exact immutable geometry/material tuple.
    // View MVP, current pose and animated texture matrix are deliberately NOT
    // certified here. Shared producer-independent sources supply identity UV;
    // legacy fixed-UV sources retain their prototype matrix. A per-instance
    // override is copied from immutable context storage. Covered rendering
    // ALWAYS validates the resulting UV before any certified-source bypass.
    // A change to any certified stable field or mutable coverage detachment
    // fails proof; none of the per-instance exclusions waive renderer validation.
    bool matchesStableInputs(const ALMetalWorldSpotShadowBatch&) const noexcept;

private:
    friend class ALMetalShadowSourceRecipe;
    ALMetalShadowSourceRecord(std::uint64_t, ALMetalWorldSpotShadowBatch,
        std::shared_ptr<const ALMetalBuffer>, std::shared_ptr<const ALMetalBuffer>,
        std::vector<std::shared_ptr<const ALMetalTexture>>, bool);
    const std::uint64_t mGeneration;
    const ALMetalWorldSpotShadowBatch mPrototype;
    const std::shared_ptr<const ALMetalBuffer> mVertices;
    const std::shared_ptr<const ALMetalBuffer> mIndices;
    const std::vector<std::shared_ptr<const ALMetalTexture>> mTextures;
    const bool mRequiresPalette;
};

// CPU-only restoration certificate, made exclusively from a fully validated
// source record. It keeps copied stable values/immutable coverage and WEAK
// exact native generations; it owns no record, pose, viewer or native resource.
// The scene owner must independently prove that the producer identity/revision
// is still current before and after restoring. Cache eviction is not such a
// mutation, while resource/geometry/material writes invalidate that producer.
class ALMetalShadowSourceRecipe final
{
public:
    static std::shared_ptr<const ALMetalShadowSourceRecipe> create(
        const ALMetalShadowSourceRecord&, std::string* error = nullptr);
    std::shared_ptr<const ALMetalShadowSourceRecord> restore(
        std::uint64_t new_generation, std::string* error = nullptr) const;
    ALMetalShadowSourceRecipe(const ALMetalShadowSourceRecipe&) = delete;
    ALMetalShadowSourceRecipe& operator=(const ALMetalShadowSourceRecipe&) = delete;
    bool requiresPalette() const noexcept { return mRequiresPalette; }
    std::uint64_t certifiedGeneration() const noexcept { return mCertifiedGeneration; }
    // Logical CPU allocation charge and cached logical native byte operands,
    // not physical RAM/residency. Native descriptors are inspected only once
    // when creating the recipe, never by restore(). Repeated texture roles
    // remain ordered; the caller's native union deduplicates their owners.
    std::uint64_t ownedBytes() const noexcept;
    std::uint64_t vertexBytes() const noexcept { return mVertexBytes; }
    std::uint64_t indexBytes() const noexcept { return mIndexBytes; }
    std::uint64_t textureBytes(std::size_t slot) const noexcept;
private:
    struct Texture
    {
        std::weak_ptr<const ALMetalTexture> allocation;
        std::uint64_t bytes = 0;
    };
    explicit ALMetalShadowSourceRecipe(const ALMetalShadowSourceRecord&);
    const std::uint64_t mCertifiedGeneration;
    const ALMetalWorldSpotShadowBatch mPrototype;
    const std::weak_ptr<const ALMetalBuffer> mVertices, mIndices;
    const std::uint64_t mVertexBytes, mIndexBytes;
    const bool mRequiresPalette;
    std::vector<Texture> mTextures;
};

inline constexpr std::array<float, 16> alMetalShadowIdentity = {
    1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};

struct ALMetalShadowPrepareInput
{
    std::shared_ptr<const ALMetalShadowSourceRecord> source;
    std::uint64_t source_generation = 0;
    std::uint64_t epoch = 0;
    // Row vectors and row-major floats, exactly LLMatrix4a's convention.
    // Preserve (model * view) * projection, including its operation order.
    std::array<float, 16> model = alMetalShadowIdentity;
    std::array<float, 16> view = alMetalShadowIdentity;
    std::array<float, 16> projection = alMetalShadowIdentity;
    bool has_model = false;
    std::shared_ptr<const ALMetalAttachmentPaletteRecord> palette;
};

// Production instance values borrow an aggregate command lease. Sources and
// poses are pinned once in that owner, across every view using the command;
// acquisition/destruction of this per-index array never changes a refcount.
struct ALMetalShadowPrepareInstance
{
    const ALMetalShadowSourceRecord* source = nullptr;
    const ALMetalAttachmentPaletteRecord* palette = nullptr;
    std::uint64_t source_generation = 0;
    std::uint64_t epoch = 0;
    std::array<float, 16> model = alMetalShadowIdentity;
    // No slot retains the source prototype UV. A selected slot indexes the context's copied
    // immutable matrix span, never an LLDrawInfo/LLMatrix4 or mutable producer.
    static constexpr std::uint32_t no_texture_matrix = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t texture_matrix_slot = no_texture_matrix;
    bool has_model = false;
};

struct ALMetalShadowPrepareContext
{
    // Row vectors/row-major, copied and validated once per synchronous call.
    std::array<float, 16> view = alMetalShadowIdentity;
    std::array<float, 16> projection = alMetalShadowIdentity;
    // The scene owner copies animated UV values before starting jobs and pins
    // this immutable storage through the synchronous join. Outputs copy the
    // selected matrix, so the span may be recycled immediately after success.
    std::span<const std::array<float, 16>> texture_matrices;
};

struct ALMetalShadowPrepareStats
{
    std::size_t input_count = 0;
    std::size_t jobs = 0;
    bool dispatched = false;
    bool cancelled = false;
};

// Synchronous dependency join, not a new frame queue. The caller allocates the
// output once and retains its immutable input packet through native encoding;
// the scene's source_owner transfers that packet lease to command completion.
// Prepared outputs borrow source/coverage/pose with no per-output refcounting.
// The caller publishes output ONLY on true. An epoch change cancels the packet;
// false resets every output and never publishes a valid-looking prefix.
// Both modes execute the same allocation-free, per-index kernel. Dispatch is
// limited to two chunks and starts at 256 inputs; smaller packets stay local.
bool alMetalPrepareShadowBatches(
    std::span<const ALMetalShadowPrepareInput> inputs,
    std::span<ALMetalWorldSpotShadowBatch> output,
    std::uint64_t epoch,
    const std::atomic<std::uint64_t>* current_epoch = nullptr,
    bool allow_dispatch = false,
    ALMetalShadowPrepareStats* stats = nullptr,
    std::string* error = nullptr);

// Compact production overload. The caller keeps the aggregate source/pose
// lease alive throughout this join, encoding and actual GPU completion via
// scene.source_owner. Instance scratch itself may be reused after the join;
// outputs borrow only immutable records, never instance/context storage.
bool alMetalPrepareShadowBatches(
    std::span<const ALMetalShadowPrepareInstance> inputs,
    const ALMetalShadowPrepareContext& context,
    std::span<ALMetalWorldSpotShadowBatch> output,
    std::uint64_t epoch,
    const std::atomic<std::uint64_t>* current_epoch = nullptr,
    bool allow_dispatch = false,
    ALMetalShadowPrepareStats* stats = nullptr,
    std::string* error = nullptr);

// Application admission overload. A two-range dispatch reserves TWO shared
// preparation credits before starting either range and holds them only until
// the synchronous join. Any denial or disabled/small work uses the identical
// required serial kernel. Initialization, pressure and draining policy belong
// to ALNativeFrameBudget; this adapter adds no separate eligibility veto. Credits
// never enter the input packet, output records or native command lifetime.
// The pure overload above remains available for isolated mechanism checks.
bool alMetalPrepareShadowBatches(
    std::span<const ALMetalShadowPrepareInstance> inputs,
    const ALMetalShadowPrepareContext& context,
    std::span<ALMetalWorldSpotShadowBatch> output,
    ALNativeFrameBudget& budget,
    std::uint64_t epoch,
    const std::atomic<std::uint64_t>* current_epoch = nullptr,
    bool allow_dispatch = false,
    ALMetalShadowPrepareStats* stats = nullptr,
    std::string* error = nullptr);
