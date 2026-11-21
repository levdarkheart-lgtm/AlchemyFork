/**
 * @file almetalworldinputs.h
 * @brief Owned immutable native world inputs for scalar encoding.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include "almetalworld.h"
#include "almetalworlddraws.h"

// Created on the scene owner after native generations have been published.
// The aggregate owner must lease every referenced buffer/texture generation,
// including destination wrappers when supplied. Raw palettes are copied here;
// sealed attachment/body records are pinned CPU-only without a second copy.
// no encoder reads viewer-owned pose storage or any LL* object.
class ALMetalWorldViewInputs final
{
public:
    static std::shared_ptr<const ALMetalWorldViewInputs> adopt(
        ALMetalWorldGBufferScene scene,
        std::vector<ALMetalWorldSimpleBatch> batches,
        std::vector<ALMetalWorldVisibilityQuery> queries,
        std::shared_ptr<const void> resource_owner,
        std::uint64_t epoch,
        std::string* error = nullptr);
    // Cohort rows are not raw/ref order tokens. Each copied group view pins
    // its exact CPU backing through scalar encode and its native tuple through
    // a separate native-only completion aggregate.
    static std::shared_ptr<const ALMetalWorldViewInputs> adopt(
        ALMetalWorldGBufferScene scene,
        std::vector<ALMetalWorldSimpleBatch> batches,
        std::vector<ALMetalWorldSourceReference> references,
        std::vector<ALMetalWorldDrawIndex> draw_order,
        std::vector<ALMetalWorldSourceCohortView> cohorts,
        std::vector<ALMetalWorldVisibilityQuery> queries,
        std::shared_ptr<const void> cpu_description_owner,
        std::shared_ptr<const void> native_resource_owner,
        std::uint64_t epoch,
        std::string* error = nullptr);
    // Compact references borrow immutable descriptions from cpu_description_owner.
    // Only native_resource_owner is retained by consuming GPU commands.
    static std::shared_ptr<const ALMetalWorldViewInputs> adopt(
        ALMetalWorldGBufferScene scene,
        std::vector<ALMetalWorldSimpleBatch> batches,
        std::vector<ALMetalWorldSourceReference> references,
        std::vector<ALMetalWorldDrawIndex> draw_order,
        std::vector<ALMetalWorldVisibilityQuery> queries,
        std::shared_ptr<const void> cpu_description_owner,
        std::shared_ptr<const void> native_resource_owner,
        std::uint64_t epoch,
        std::string* error = nullptr);
    ~ALMetalWorldViewInputs();
    const ALMetalWorldGBufferScene& scene() const noexcept;
    std::uint64_t epoch() const noexcept;
    // Exact immutable aggregate supplied to adopt(): native source generations
    // and destination wrappers, with no ownership of this CPU input packet.
    std::shared_ptr<const void> resourceLease() const noexcept;
    // Owner only, after encoding has returned and no future CPU reader can
    // access the packet. Unique strong ownership is only a backstop; it does
    // not synchronize a racing weak lock. Every output vector must be empty.
    // Success returns their capture capacity with values cleared and resets
    // input (including copied poses). Refusal leaves every argument intact.
    static bool reclaimCpuStorage(
        std::shared_ptr<const ALMetalWorldViewInputs>& input,
        std::vector<ALMetalWorldSimpleBatch>& empty_batches,
        std::vector<ALMetalWorldVisibilityQuery>& empty_queries) noexcept;
    static bool reclaimCpuStorage(
        std::shared_ptr<const ALMetalWorldViewInputs>& input,
        std::vector<ALMetalWorldSimpleBatch>& empty_batches,
        std::vector<ALMetalWorldSourceReference>& empty_references,
        std::vector<ALMetalWorldDrawIndex>& empty_draw_order,
        std::vector<ALMetalWorldSourceCohortView>& empty_cohorts,
        std::vector<ALMetalWorldVisibilityQuery>& empty_queries) noexcept;
    // Returns raw, reference, order and query capacities together. The raw-only
    // overload refuses packets containing compact records or ordered storage.
    static bool reclaimCpuStorage(
        std::shared_ptr<const ALMetalWorldViewInputs>& input,
        std::vector<ALMetalWorldSimpleBatch>& empty_batches,
        std::vector<ALMetalWorldSourceReference>& empty_references,
        std::vector<ALMetalWorldDrawIndex>& empty_draw_order,
        std::vector<ALMetalWorldVisibilityQuery>& empty_queries) noexcept;
private:
    struct Impl;
    explicit ALMetalWorldViewInputs(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};

// Shared validation for raw scenes and owned scalar input packets.
bool alMetalValidateWorldView(const ALMetalWorldGBufferScene&, std::string* error);
bool alMetalValidateWorldBatch(const ALMetalWorldGBufferScene&,
                              const ALMetalWorldSimpleBatch&, std::string* error);
bool alMetalValidateWorldBatch(const ALMetalWorldGBufferScene&,
                              const ALMetalWorldDraw&, std::string* error);
