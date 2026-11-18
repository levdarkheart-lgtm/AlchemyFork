/**
 * @file almetalworldsourcecommands.h
 * @brief Immutable indexed command banks inheriting source-view state.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetalworldsourcepages.h"
#include <array>
#include <vector>
#include <utility>

struct ALMetalWorldSourceCommandDraw
{
    ALMetalWorldSourceKey key;
    std::uint32_t view_index = 0;
    const ALMetalBuffer* indices = nullptr;
    std::size_t index_offset = 0;
    std::uint32_t index_count = 0;
    ALMetalIndexType index_type = ALMetalIndexType::uint16;
};
enum class ALMetalWorldSourceCommandFailure : std::uint8_t
{
    none, unsupported, validation, capacity, admission, native_allocation,
    host_allocation, retention, execution, pipeline, short_run, source_setup, count
};
const char* alMetalWorldSourceCommandFailureName(ALMetalWorldSourceCommandFailure) noexcept;
struct ALMetalWorldSourceCommandLimits
{
    std::size_t maximum_cached_banks = 64, maximum_live_banks = 128;
    std::size_t maximum_unique_index_buffers = 4096;
    std::uint64_t maximum_live_native_bytes = 64ULL * 1024 * 1024;
};
struct ALMetalWorldSourceCommandStats
{
    std::uint64_t requests=0,hits=0,builds=0,executions=0,logical_draws=0,evictions=0;
    // Actual animated closure-declaration attempts, including failed/abandoned
    // transactions. Exact encoder-scope reuse does not add another call.
    std::uint64_t declaration_calls=0;
    std::uint64_t admission_requests=0,admission_denials=0,charged_logical_argument_bytes=0;
    std::uint64_t charged_logical_binding_bytes=0;
    // Unique existing index footprint, not a new allocation or another GPU
    // memory total. Both native quantities include still-pending retired banks.
    std::uint64_t live_icb_allocated_bytes=0,unique_retained_index_bytes=0;
    // Additional gross native binding/asset footprint uses SourcePages' resolved
    // hardware-derived asset limit, separate from the unchanged ICB/index cap.
    // Each scope deduplicates allocation identity. A legal combined allocation
    // used in both roles overlaps these gross scopes; do not add them as RAM.
    std::uint64_t unique_retained_binding_bytes=0,retained_binding_limit_bytes=0;
    std::uint64_t direct_builds=0,direct_hits=0,direct_encoded_calls=0;
    // Encoded compute dispatches/rows, not GPU-executed or visible draws.
    std::uint64_t static_visibility_dispatches=0,static_visibility_rows=0;
    // Qualified inputs in encoded dispatches; this is not a visible-row count.
    std::uint64_t static_visibility_qualified_rows=0;
    std::size_t cached_banks=0,live_banks=0;
    std::array<std::uint64_t,static_cast<std::size_t>(ALMetalWorldSourceCommandFailure::count)> failures{};
    ALMetalWorldSourceCommandFailure last_failure=ALMetalWorldSourceCommandFailure::none;
};

enum class ALMetalWorldSourceCommandRoute : std::uint8_t { direct, cpu_visibility, gpu_visibility };
enum class ALMetalWorldSourceCommandReason : std::uint8_t
{
    unsupported, invalid_input, raster_policy, fragmented, small_work,
    unknown_bounds, inside, outside, heavy_partial, heavy_animated
};
struct ALMetalWorldSourceCommandContext
{
    // Copied from the actual selected native pipeline descriptor and pass.
    bool pipeline_supports_indirect=false;
    bool wireframe=false,depth_clamp=false,opaque_coverage_stamp=false;
    std::size_t ordered_ranges=1;
};
struct ALMetalWorldSourceCommandPolicy
{
    // Initial applicability bounds, to be qualified by complete producer-to-
    // completion costs. They count actual indexed input, not GPU invocations.
    std::uint64_t minimum_qualified_indices=196608;
    std::size_t maximum_ordered_ranges=128;
};
struct ALMetalWorldSourceCommandDecision
{
    ALMetalWorldSourceCommandRoute route=ALMetalWorldSourceCommandRoute::direct;
    ALMetalWorldSourceCommandReason reason=ALMetalWorldSourceCommandReason::invalid_input;
    std::uint64_t indexed_elements=0,qualified_indexed_elements=0;
    std::size_t draws=0;
};

// Submission owner only; completion threads retire native-accounting leases.
// The idle cache owns no CPU source graph or strong C++ geometry generation.
class ALMetalWorldSourceCommands final
{
public:
    class Batch final
    {
    public:
        ~Batch();
        std::size_t size() const noexcept;
    private:
        struct Impl;
        explicit Batch(std::unique_ptr<Impl>);
        std::unique_ptr<Impl> mImpl;
        friend class ALMetalWorldSourceCommands;
    };
    // Null before a native prefix permits the required direct path. Null
    // after a compute begin attempt requires cancellation of this command;
    // callers must not hide an unknown/failed optional prefix with fallback.
    struct Preparation
    {
        std::shared_ptr<const Batch> batch;
        bool native_prefix = false;
        explicit operator bool() const noexcept { return bool(batch); }
        bool mayFallback() const noexcept { return !batch && !native_prefix; }
    };
    class DeclarationScope final
    {
    public:
        DeclarationScope()=default;
        explicit operator bool() const noexcept {return bool(mIdentity);}
    private:
        explicit DeclarationScope(std::shared_ptr<const void> identity):mIdentity(std::move(identity)){}
        std::shared_ptr<const void> mIdentity;
        friend class ALMetalWorldSourceCommands;
    };
    // Create inside the active encoder lifetime, before its draws. Scope owns
    // only an inert identity; ended/new encoders cannot reuse its proof. An
    // allocation failure returns empty, allowing the self-declaring API below.
    DeclarationScope makeDeclarationScope(ALMetalRenderEncoder&,ALMetalCommandBuffer&,std::string* error=nullptr);
    static std::shared_ptr<ALMetalWorldSourceCommands> create(const ALMetalDevice&,
        ALMetalWorldSourceCommandLimits={},std::string* error=nullptr);
    ~ALMetalWorldSourceCommands();
    bool supported() const noexcept;
    // Call BEFORE SourcePages/native-bank allocation. Small/unqualified views
    // keep the existing scalar route; this never forces indirect machinery.
    ALMetalWorldSourceCommandDecision applicability(const ALMetalWorldSourceCohortView&,
        const ALMetalWorldSourceCommandContext&,ALMetalWorldSourceCommandPolicy={}) const noexcept;
    ALMetalWorldSourceCommandDecision applicability(
        std::span<const ALMetalWorldSourceReference* const>,const ALMetalWorldSourceCommandContext&,
        ALMetalWorldSourceCommandPolicy={}) const noexcept;
    void setPublicationAdmission(ALMetalWorldSourcePages::PublicationAdmission);
    // Pins native Bank+exact FrameView on the actual command before emission.
    // Warm hits use no publication credit. Draw descriptors are not retained.
    std::shared_ptr<const Batch> prepareForCommand(ALMetalCommandBuffer&,
        std::shared_ptr<const ALMetalWorldSourceFrameView>,
        std::span<const ALMetalWorldSourceCommandDraw>,std::string* error=nullptr,
        ALMetalWorldSourceCommandFailure* failure=nullptr);
    // Uses private immutable ordered cohort membership issued by SourcePages.
    // Warm lookup compares exact membership identity, not N reconstructed
    // draw/key values. The idle cache keeps only a weak membership identity;
    // this actual command retains the Bank and current native-only FrameView.
    std::shared_ptr<const Batch> prepareCohortForCommand(ALMetalCommandBuffer&,
        std::shared_ptr<const ALMetalWorldSourceFrameView>,std::string* error=nullptr,
        ALMetalWorldSourceCommandFailure* failure=nullptr);
    // DIRECT-only bank kind. Immutable cold bounds/index rows drive one GPU
    // draw-count update on this exact publication command before rendering.
    // Neither CPU readiness/readback nor per-row warm writes are required.
    std::shared_ptr<const Batch> prepareVisibleCohortForCommand(ALMetalCommandBuffer&,
        std::shared_ptr<const ALMetalWorldSourceFrameView>,const ALMetalDevice&,const ALMetalShaderLibrary&,
        std::string* error=nullptr,ALMetalWorldSourceCommandFailure* failure=nullptr);
    Preparation prepareVisibleCohortChecked(ALMetalCommandBuffer&,
        std::shared_ptr<const ALMetalWorldSourceFrameView>,const ALMetalDevice&,const ALMetalShaderLibrary&,
        std::string* error=nullptr,ALMetalWorldSourceCommandFailure* failure=nullptr);
    // Current pose/view updates only; immutable native bindings and indexed
    // calls are encoded once per reusable completion-qualified slot. The
    // returned Batch and command own native generations, never CPU records.
    std::shared_ptr<const Batch> prepareAnimatedForCommand(ALMetalCommandBuffer&,
        const ALMetalDevice&,const ALMetalShaderLibrary&,
        std::span<const ALMetalWorldAnimatedGpuView>,const ALMetalWorldSourceGlobals&,
        ALMetalWorldFrameData&,std::string* error=nullptr,bool gpu_visibility=true);
    Preparation prepareAnimatedChecked(ALMetalCommandBuffer&,
        const ALMetalDevice&,const ALMetalShaderLibrary&,
        std::vector<ALMetalWorldAnimatedGpuView>&&,const ALMetalWorldSourceGlobals&,
        ALMetalWorldFrameData&,std::string* error=nullptr,bool gpu_visibility=true);
    // Transfers the completed late-view vector before callbacks. Callers must
    // surrender all mutable aliases, as with immutable input adoption. Public
    // borrowed spans retain their defensive snapshot semantics above.
    std::shared_ptr<const Batch> prepareAnimatedForCommand(ALMetalCommandBuffer&,
        const ALMetalDevice&,const ALMetalShaderLibrary&,
        std::vector<ALMetalWorldAnimatedGpuView>&&,const ALMetalWorldSourceGlobals&,
        ALMetalWorldFrameData&,std::string* error=nullptr,bool gpu_visibility=true);
    bool execute(ALMetalRenderEncoder&,ALMetalCommandBuffer&,const Batch&,std::string* error=nullptr);
    // Execute an ordered nonempty subset of the same immutable Bank. Callers
    // split at original query/raster boundaries and restore inherited state;
    // range selection neither republishes the Bank nor retains CPU packets.
    bool executeRange(ALMetalRenderEncoder&,ALMetalCommandBuffer&,const Batch&,
        std::size_t first,std::size_t count,std::string* error=nullptr);
    bool executeRange(ALMetalRenderEncoder&,ALMetalCommandBuffer&,const Batch&,
        std::size_t first,std::size_t count,const DeclarationScope&,std::string* error=nullptr);
    void clear() noexcept;
    void trimUnusedBanks() noexcept;
    ALMetalWorldSourceCommandStats stats() const noexcept;
    void failNextPublicationForTesting() noexcept;
    // One-shot CPU-owner fault seam; disabled unless explicitly armed. Fail
    // before emitting the range after this many successful range submissions.
    void failExecutionAfterForTesting(std::size_t successful_ranges) noexcept;
    // One-shot correctness seam after compute begin, before dispatch. Native
    // prefix propagation is observable without committing a failing command.
    void failVisibilityAfterBeginForTesting() noexcept;
private:
    std::shared_ptr<const Batch> prepareCohortForCommandImpl(ALMetalCommandBuffer&,
        std::shared_ptr<const ALMetalWorldSourceFrameView>,const ALMetalDevice*,const ALMetalShaderLibrary*,
        std::string*,ALMetalWorldSourceCommandFailure*,bool* native_prefix);
    std::shared_ptr<const Batch> prepareAnimatedForCommandImpl(ALMetalCommandBuffer&,
        const ALMetalDevice&,const ALMetalShaderLibrary&,
        std::vector<ALMetalWorldAnimatedGpuView>&&,const ALMetalWorldSourceGlobals&,
        ALMetalWorldFrameData&,std::string*,bool gpu_visibility,bool* native_prefix);
    bool executeRangeImpl(ALMetalRenderEncoder&,ALMetalCommandBuffer&,const Batch&,
        std::size_t first,std::size_t count,const DeclarationScope*,std::string* error);
    struct Impl;
    explicit ALMetalWorldSourceCommands(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};
