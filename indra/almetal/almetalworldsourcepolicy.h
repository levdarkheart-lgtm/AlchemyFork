/**
 * @file almetalworldsourcepolicy.h
 * @brief Cheap source-capability eligibility shared by producers and consumers.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <functional>
#include <memory>

// Stable identity shared by ordinary immutable source and cohort records.
struct ALMetalWorldSourceKey
{
    std::uint64_t identity = 0, flavor = 0, revision = 0;
};

class ALMetalBuffer;
// Exact native generation owners used by CPU animated-source certificates.
struct ALMetalWorldSourceNativeScope
{
    std::shared_ptr<const void> owner;
    std::shared_ptr<const ALMetalBuffer> vertices, indices;
};

// Checked logical publication work. This is shared with active probe-mask
// publication; it is not a physical resident-memory or GPU-traffic estimate.
struct ALMetalWorldSourcePublicationWork
{
    std::uint64_t encoded_bytes=0, copied_bytes=0;
    std::uint64_t allocation_bytes=0;
    std::uint64_t chargedBytes() const noexcept
    {
        constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
        return encoded_bytes > maximum - copied_bytes
            ? maximum : encoded_bytes + copied_bytes;
    }
};
using ALMetalWorldPublicationAdmission =
    std::function<std::shared_ptr<const void>(const ALMetalWorldSourcePublicationWork&)>;

struct ALMetalWorldSourceCapabilityInput
{
    // Stable indexed input from completed producer membership, not GPU work.
    std::uint64_t indexed_elements=0;
    std::size_t rows=0;
    bool ordinary_static=false;
    bool indexed_material=false; // Actual supported Legacy/PBR G-buffer family.
    bool published_geometry=false;
    bool immutable_uv=false;
};

// This predicate performs no source scan, allocation or admission. The caller
// separately acquires bounded CPU publication credit before extracting bounds.
// It deliberately excludes animation: that capability owns its existing
// independently admitted source phase and cannot nest under E1 credit.
inline bool alMetalWorldNeedsHeavySourceBounds(const ALMetalWorldSourceCapabilityInput& input,
    std::uint64_t minimum_indexed_elements=196608) noexcept
{
    return input.ordinary_static && input.indexed_material && input.published_geometry && input.immutable_uv &&
        input.rows && input.rows<=4096 && input.indexed_elements>=minimum_indexed_elements;
}

// Completed draw maps are pass-ordered. Keep one dominant actual pass without
// allocating per-group buckets or counting mutually exclusive passes together.
// Producers commit a chunk-local copy only after successful base preparation.
struct ALMetalWorldSourceCapabilitySummary
{
    std::uint32_t current_pass=std::numeric_limits<std::uint32_t>::max();
    std::uint32_t pass=std::numeric_limits<std::uint32_t>::max();
    std::uint64_t current_elements=0, indexed_elements=0;
    std::size_t current_rows=0, rows=0;
    void add(std::uint32_t actual_pass, std::uint64_t elements) noexcept
    {
        if(current_pass!=actual_pass)
        { current_pass=actual_pass; current_elements=0; current_rows=0; }
        const auto maximum=std::numeric_limits<std::uint64_t>::max();
        current_elements=elements>maximum-current_elements?maximum:current_elements+elements;
        ++current_rows;
        if(current_elements>indexed_elements || pass==actual_pass)
        { pass=actual_pass; indexed_elements=current_elements; rows=current_rows; }
    }
    ALMetalWorldSourceCapabilityInput input() const noexcept
    { return {indexed_elements,rows,true,true,true,true}; }
};
