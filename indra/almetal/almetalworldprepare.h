/**
 * @file almetalworldprepare.h
 * @brief Owned native world inputs and scheduler-owned CPU preparation jobs.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include "almetalworld.h"
#include "almetalworlddraws.h"

#include <atomic>
#include <span>

// Created on the scene owner after native generations have been published.
// The aggregate owner must lease every referenced buffer/texture generation,
// including destination wrappers when supplied. Raw palettes are copied here;
// sealed attachment/body records are pinned CPU-only without a second copy.
// no detached job reads viewer-owned pose storage or any LL* object.
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
    // its exact CPU backing through jobs/encode and its native tuple through
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
    // Owner only, after jobs/encoding have joined and no future CPU reader can
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

struct ALMetalWorldPrepareStats
{
    std::size_t input_count = 0;
    std::size_t prepared_count = 0;
    std::size_t constant_bytes = 0;
    std::size_t jobs = 0;
    bool cancelled = false;
    std::size_t uncompacted_constant_bytes = 0;
    std::size_t shared_common_records = 0;
};

// Internal immutable record ABI between the CPU kernel and native encoder.
// Offsets address one packed constant publication; no GPU wrapper is touched
// by the range kernel. Original draw indices and query offsets never change.
struct ALMetalWorldPreparedRecord
{
    enum class Family : std::uint8_t { simple, terrain, impostor };
    Family family = Family::simple;
    std::size_t offset = 0, length = 0;
    // A zero table length with an active material/PBR family means the batch
    // has a matched immutable native material source, not an inline table.
    std::size_t material_offset = 0, material_length = 0;
    std::size_t pbr_offset = 0, pbr_length = 0;
    bool avatar = false, rigged = false, bump = false, material = false, pbr = false;
    bool material_normal_map = false, material_specular_map = false;
    std::uint8_t avatar_variant = 0;
};

class ALMetalPreparedWorldView final
{
public:
    ~ALMetalPreparedWorldView();
    const ALMetalWorldGBufferScene& scene() const noexcept;
    const ALMetalWorldPrepareStats& stats() const noexcept;
private:
    bool isCurrent() const noexcept;
    std::shared_ptr<const void> inputLease() const noexcept;
    std::span<const ALMetalWorldPreparedRecord> records() const noexcept;
    std::span<const std::byte> constants() const noexcept;
    struct Impl;
    explicit ALMetalPreparedWorldView(std::shared_ptr<Impl>);
    std::shared_ptr<Impl> mImpl;
    friend class ALMetalWorldPreparation;
    friend class ALMetalWorldGBufferRenderer;
};

// This is a dependency ticket, not a queue/scheduler. The scene owner asks the
// shared ALNativeFrameBudget for credits before scheduling runJob(). A denied
// credit executes the same required job locally; it never removes a view.
// Jobs are disjoint, bounded to two, and allocate no Metal resources. Capture a
// shared ticket in every scheduled callback. Join/finish stays on the owner.
class ALMetalWorldPreparation final
{
public:
    static std::shared_ptr<ALMetalWorldPreparation> create(
        std::shared_ptr<const ALMetalWorldViewInputs> inputs,
        std::size_t requested_jobs = 1,
        std::shared_ptr<const std::atomic<std::uint64_t>> current_epoch = {},
        std::string* error = nullptr);
    ~ALMetalWorldPreparation();
    std::size_t jobCount() const noexcept;
    bool runJob(std::size_t index) noexcept;
    void cancel() noexcept;
    // Requires all jobs to have joined. Compacts exact consecutive common
    // values in place on the owner, then seals once; repeated finish is stable.
    // Never returns a valid-looking prefix.
    std::shared_ptr<const ALMetalPreparedWorldView> finish(std::string* error = nullptr);
private:
    struct Impl;
    explicit ALMetalWorldPreparation(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};

// Same native semantics used by direct render(scene) and the detached kernel.
// These functions read only the immutable scene/batch values and const native
// resource metadata. They do not access renderer state, commands or frame data.
bool alMetalValidateWorldView(const ALMetalWorldGBufferScene&, std::string* error);
bool alMetalValidateWorldBatch(const ALMetalWorldGBufferScene&,
                              const ALMetalWorldSimpleBatch&, std::string* error);
bool alMetalValidateWorldBatch(const ALMetalWorldGBufferScene&,
                              const ALMetalWorldDraw&, std::string* error);
bool alMetalLayoutWorldRecord(const ALMetalWorldSimpleBatch&,
                             std::size_t& next, ALMetalWorldPreparedRecord&,
                             std::string* error);
bool alMetalPrepareWorldRecord(const ALMetalWorldGBufferScene&, std::size_t index,
                              ALMetalWorldPreparedRecord&,
                              std::span<std::byte> bytes, std::string* error);
