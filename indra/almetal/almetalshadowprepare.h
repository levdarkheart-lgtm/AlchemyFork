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
