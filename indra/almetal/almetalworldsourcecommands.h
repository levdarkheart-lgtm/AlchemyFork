/**
 * @file almetalworldsourcecommands.h
 * @brief Immutable indexed command banks inheriting source-view state.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include "almetalworldsourcepages.h"
#include <array>
#include <vector>

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
    std::uint64_t admission_requests=0,admission_denials=0,charged_logical_argument_bytes=0;
    // Unique existing index footprint, not a new allocation or another GPU
    // memory total. Both native quantities include still-pending retired banks.
    std::uint64_t live_icb_allocated_bytes=0,unique_retained_index_bytes=0;
    std::size_t cached_banks=0,live_banks=0;
    std::array<std::uint64_t,static_cast<std::size_t>(ALMetalWorldSourceCommandFailure::count)> failures{};
    ALMetalWorldSourceCommandFailure last_failure=ALMetalWorldSourceCommandFailure::none;
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
    static std::shared_ptr<ALMetalWorldSourceCommands> create(const ALMetalDevice&,
        ALMetalWorldSourceCommandLimits={},std::string* error=nullptr);
    ~ALMetalWorldSourceCommands();
    bool supported() const noexcept;
    void setPublicationAdmission(ALMetalWorldSourcePages::PublicationAdmission);
    // Pins native Bank+exact FrameView on the actual command before emission.
    // Warm hits use no publication credit. Draw descriptors are not retained.
    std::shared_ptr<const Batch> prepareForCommand(ALMetalCommandBuffer&,
        std::shared_ptr<const ALMetalWorldSourceFrameView>,
        std::span<const ALMetalWorldSourceCommandDraw>,std::string* error=nullptr,
        ALMetalWorldSourceCommandFailure* failure=nullptr);
    bool execute(ALMetalRenderEncoder&,ALMetalCommandBuffer&,const Batch&,std::string* error=nullptr);
    void clear() noexcept;
    ALMetalWorldSourceCommandStats stats() const noexcept;
    void failNextPublicationForTesting() noexcept;
private:
    struct Impl;
    explicit ALMetalWorldSourceCommands(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> mImpl;
};
