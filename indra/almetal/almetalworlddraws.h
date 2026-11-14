/**
 * @file almetalworlddraws.h
 * @brief Ordered native G-buffer source references and copied view matrices.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

struct ALMetalWorldSimpleBatch;
struct ALMetalWorldGBufferScene;
struct ALMetalWorldSourceCohortView;

// These are the only per-view overrides for the first compact slice. Source
// geometry, material, coverage and native generation facts remain immutable.
struct alignas(16) ALMetalWorldViewMatrices
{
    float modelview_projection[16]{};
    float modelview[16]{};
    float normal_matrix[3][4]{};
    float rigged_normal_matrix[3][4]{};
    float texture_matrix[16] = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
    };
};
static_assert(sizeof(ALMetalWorldViewMatrices) == 288);

// Borrowed source lifetime belongs to the packet's aggregate CPU-description
// owner. Native generations have a separate command-completion owner.
struct alignas(16) ALMetalWorldSourceReference
{
    const ALMetalWorldSimpleBatch* source = nullptr;
    ALMetalWorldViewMatrices view;
};
static_assert(sizeof(ALMetalWorldSourceReference) == 304);
static_assert(std::is_trivially_copyable_v<ALMetalWorldSourceReference>);

using ALMetalWorldDrawIndex = std::uint32_t;
inline constexpr ALMetalWorldDrawIndex alMetalWorldDrawReferenceBit = 0x80000000u;
inline constexpr ALMetalWorldDrawIndex alMetalWorldDrawIndexMask = 0x7fffffffu;
constexpr bool alMetalWorldDrawIsReference(ALMetalWorldDrawIndex index) noexcept
{ return (index & alMetalWorldDrawReferenceBit) != 0; }
constexpr ALMetalWorldDrawIndex alMetalWorldDrawStorageIndex(ALMetalWorldDrawIndex index) noexcept
{ return index & alMetalWorldDrawIndexMask; }
constexpr bool alMetalWorldDrawIndexFits(std::size_t index) noexcept
{ return index <= alMetalWorldDrawIndexMask; }

// A resolved draw is three borrowed pointers. No consumer may reconstruct a full
// batch just to apply view overrides. Matrix getters are defined after the
// native batch is complete, alongside its existing packing/validation code.
class ALMetalWorldDraw final
{
public:
    explicit ALMetalWorldDraw(const ALMetalWorldSimpleBatch& source,
        const ALMetalWorldViewMatrices* view = nullptr,
        const float* fixed_texture_matrix = nullptr) noexcept
        : mSource(&source), mView(view), mFixedTextureMatrix(fixed_texture_matrix) {}
    const ALMetalWorldSimpleBatch& source() const noexcept { return *mSource; }
    const ALMetalWorldViewMatrices* view() const noexcept { return mView; }
    const float* modelviewProjection() const noexcept;
    const float* modelview() const noexcept;
    const float* normalMatrix() const noexcept;
    const float* riggedNormalMatrix() const noexcept;
    const float* textureMatrix() const noexcept;

private:
    const ALMetalWorldSimpleBatch* mSource;
    const ALMetalWorldViewMatrices* mView;
    const float* mFixedTextureMatrix;
};
static_assert(sizeof(ALMetalWorldDraw) == sizeof(void*) * 3);

// Local storage checks, also used before resolving an untrusted direct helper
// request. Whole scene ordering/owner proofs are validated on adoption/render.
const ALMetalWorldSourceCohortView* alMetalWorldCohortAt(
    const ALMetalWorldGBufferScene&,std::size_t logical_index) noexcept;
bool alMetalWorldDrawStorageAvailable(
    const ALMetalWorldGBufferScene&,std::size_t logical_index) noexcept;

// Sequential native consumption advances through cohort ranges, not through
// their constituent rows when skipping a certified group or an encoded run.
class ALMetalWorldDrawCursor final
{
public:
    explicit ALMetalWorldDrawCursor(const ALMetalWorldGBufferScene&,
                                   std::size_t first=0) noexcept;
    ALMetalWorldDraw draw() const noexcept;
    const ALMetalWorldSourceCohortView* cohort() const noexcept;
    void advance(std::size_t count=1) noexcept;
private:
    const ALMetalWorldGBufferScene* mScene;
    std::size_t mLogical=0,mBase=0,mCohort=0;
};
