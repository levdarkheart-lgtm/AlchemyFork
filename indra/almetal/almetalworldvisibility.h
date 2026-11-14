/**
 * @file almetalworldvisibility.h
 * @brief Shared persistent conservative bounds and ordered native draw arguments.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetalpipeline.h"
#include <array>
#include <limits>
#include <span>

struct ALMetalWorldBoundsHandle
{
    std::uint32_t slot = std::numeric_limits<std::uint32_t>::max();
    std::uint64_t identity = 0, revision = 0;
};
struct ALMetalWorldConservativeBounds
{
    float minimum[4]{}, maximum[4]{};
    // Producer proof for this exact immutable geometry generation. Animated,
    // cloth, rigged or otherwise unproved extents must leave this false.
    bool qualified = false;
};
struct alignas(16) ALMetalWorldVisibilityMember
{
    float modelview_projection[16]{};
    std::uint64_t identity = 0, revision = 0;
    std::uint32_t slot = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t index_count = 0;
    std::uint32_t enabled = 0;
    std::uint32_t depth_clamp = 0;
};
static_assert(sizeof(ALMetalWorldVisibilityMember) == 96);
struct ALMetalWorldIndexedArguments
{
    std::uint32_t index_count = 0, instance_count = 1, index_start = 0;
    std::int32_t base_vertex = 0;
    std::uint32_t base_instance = 0;
};
static_assert(sizeof(ALMetalWorldIndexedArguments) == 20);
enum class ALMetalWorldVisibilityFailure : std::uint8_t
{
    none, invalid_input, completion_tracking, bounds_allocation,
    arguments_busy, arguments_allocation, publication, retention, encoder,
    host_allocation, count
};
struct ALMetalWorldVisibilityStats
{
    std::uint64_t bounds_publications = 0, stale_publications = 0;
    std::uint64_t generation_copies = 0, generation_reuses = 0;
    std::uint64_t dirty_records_uploaded = 0, uploaded_bytes = 0;
    std::uint64_t views = 0, argument_candidates = 0, fallbacks = 0;
    std::array<std::uint64_t, static_cast<std::size_t>(ALMetalWorldVisibilityFailure::count)> fallback_reasons{};
    ALMetalWorldVisibilityFailure last_fallback = ALMetalWorldVisibilityFailure::none;
};
class ALMetalWorldVisibility;
class ALMetalWorldVisibilityView final
{
public:
    ~ALMetalWorldVisibilityView();
    std::size_t size() const noexcept;
private:
    struct Impl;
    explicit ALMetalWorldVisibilityView(std::shared_ptr<Impl>);
    std::shared_ptr<Impl> mImpl;
    friend class ALMetalWorldVisibility;
    friend class ALMetalWorldVisibilityArguments;
};
class ALMetalWorldVisibilityArguments final
{
public:
    ~ALMetalWorldVisibilityArguments();
    bool appliesTo(std::size_t index, std::uint32_t index_count,
                   const float* modelview_projection = nullptr,
                   bool required_depth_clamp = false) const noexcept;
    // Only the command that encoded these arguments may consume them.
    bool drawIndexed(ALMetalRenderEncoder&, ALMetalCommandBuffer&,
        std::size_t index, ALMetalIndexType, const ALMetalBuffer& indices,
        std::size_t index_offset, std::uint32_t index_count, std::string* error = nullptr) const;
    std::uint64_t boundsGeneration() const noexcept;
    // Post-completion tests only: never waits, submits, or reads pending data.
    bool readCompletedArgumentForTesting(std::size_t, ALMetalWorldIndexedArguments&) const;
private:
    struct Impl;
    explicit ALMetalWorldVisibilityArguments(std::shared_ptr<Impl>,
        std::shared_ptr<const ALMetalWorldVisibilityView::Impl>);
    std::shared_ptr<Impl> mImpl;
    std::shared_ptr<const ALMetalWorldVisibilityView::Impl> mView;
    friend class ALMetalWorldVisibility;
};

// Publication/encoding owner only. CPU jobs may retain immutable View objects,
// but never mutate this table. No viewer objects or camera globals are stored.
class ALMetalWorldVisibility final
{
public:
    static std::shared_ptr<ALMetalWorldVisibility> create(const ALMetalDevice&, std::string* error = nullptr);
    ~ALMetalWorldVisibility();
    ALMetalWorldBoundsHandle publishBounds(std::uint64_t identity, std::uint64_t revision,
        const ALMetalWorldConservativeBounds&);
    bool retire(ALMetalWorldBoundsHandle) noexcept;
    std::shared_ptr<const ALMetalWorldVisibilityView> makeView(
        std::vector<ALMetalWorldVisibilityMember> ordered_members, std::string* error = nullptr);
    // Same-command compute precedes render; no readback, CPU wait or ICB.
    // nullptr leaves the caller's exact native direct path available.
    static std::shared_ptr<ALMetalWorldVisibilityArguments> encode(
        ALMetalCommandBuffer&, const ALMetalWorldVisibilityView&, std::string* error = nullptr);
    ALMetalWorldVisibilityStats stats() const noexcept;
private:
    friend class ALMetalWorldVisibilityView;
    friend class ALMetalWorldVisibilityArguments;
    struct Impl;
    explicit ALMetalWorldVisibility(std::shared_ptr<Impl>);
    std::shared_ptr<Impl> mImpl;
};
