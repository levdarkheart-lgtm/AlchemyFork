/**
 * @file almetalworldsourcepolicy.h
 * @brief Cheap source-capability eligibility shared by producers and consumers.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once
#include <cstddef>
#include <cstdint>

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
// independently admitted source/bone phase and cannot nest under E1 credit.
inline bool alMetalWorldNeedsHeavySourceBounds(const ALMetalWorldSourceCapabilityInput& input,
    std::uint64_t minimum_indexed_elements=196608) noexcept
{
    return input.ordinary_static && input.indexed_material && input.published_geometry && input.immutable_uv &&
        input.rows && input.rows<=4096 && input.indexed_elements>=minimum_indexed_elements;
}
