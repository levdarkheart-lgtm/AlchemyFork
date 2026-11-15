/**
 * @file almetalshadowprepare.mm
 * @brief Pure shadow packet preparation on bounded Dispatch chunks.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetalshadowprepare.h"
#include "../llcommon/alnativeframebudget.h"

#include <arm_neon.h>
#include <dispatch/dispatch.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace
{
static_assert(std::is_trivially_copyable_v<ALMetalWorldSpotShadowValues>,
              "Detached preparation copies scalar values without shared ownership");
static_assert(std::is_trivially_copyable_v<ALMetalShadowPrepareInstance>,
              "Production instances must not acquire per-index owners");
bool fail(std::string* error, const char* message)
{
    if (error) *error = message;
    return false;
}

// Bit checks stay meaningful even when surrounding renderer flags enable
// fast-math; NaN/Inf are structural rejection conditions at this boundary.
bool finite(float value) noexcept
{
    return (std::bit_cast<std::uint32_t>(value) & 0x7f800000u) != 0x7f800000u;
}

bool finite(const float* values, std::size_t count) noexcept
{
    for (std::size_t at = 0; at < count; ++at)
        if (!finite(values[at])) return false;
    return true;
}

bool fits(std::size_t offset, std::size_t count, std::size_t stride,
          std::size_t length) noexcept
{
    return offset <= length && count <= (length - offset) / stride;
}

// Same ARM64 row weighting and three explicit fused additions as
// LLMatrix4a::rowMul. Do not reassociate the two matrix products.
void multiply(const float* a, const float* b, float* output) noexcept
{
    const auto b0 = vld1q_f32(b), b1 = vld1q_f32(b + 4);
    const auto b2 = vld1q_f32(b + 8), b3 = vld1q_f32(b + 12);
    for (unsigned row = 0; row < 4; ++row)
    {
        const auto r = vld1q_f32(a + row * 4);
        auto value = vmulq_laneq_f32(b0, r, 0);
        value = vfmaq_laneq_f32(value, b1, r, 1);
        value = vfmaq_laneq_f32(value, b2, r, 2);
        value = vfmaq_laneq_f32(value, b3, r, 3);
        vst1q_f32(output + row * 4, value);
    }
}

template<typename Input>
struct Preparation
{
    std::span<const Input> inputs;
    std::span<ALMetalWorldSpotShadowBatch> output;
    std::uint64_t epoch;
    const std::atomic<std::uint64_t>* current_epoch;
    std::size_t jobs;
    const ALMetalShadowPrepareContext* context;
    std::atomic<bool> invalid{false};
    std::atomic<bool> cancelled{false};
};

template<typename Input>
void prepareRange(void* pointer, std::size_t job) noexcept
{
    auto& work = *static_cast<Preparation<Input>*>(pointer);
    const auto middle = work.inputs.size() / 2;
    const auto begin = work.jobs == 1 || job == 0 ? 0 : middle;
    const auto end = work.jobs == 1 || job == 1 ? work.inputs.size() : middle;
    for (auto index = begin; index < end; ++index)
    {
        if (work.current_epoch &&
            work.current_epoch->load(std::memory_order_acquire) != work.epoch)
        {
            work.cancelled.store(true, std::memory_order_relaxed);
            return;
        }
        const auto& input = work.inputs[index];
        const auto* source = [&] {
            if constexpr (std::is_same_v<Input, ALMetalShadowPrepareInstance>) return input.source;
            else return input.source.get();
        }();
        const auto* palette = [&] {
            if constexpr (std::is_same_v<Input, ALMetalShadowPrepareInstance>) return input.palette;
            else return input.palette.get();
        }();
        const auto* view = [&] {
            if constexpr (std::is_same_v<Input, ALMetalShadowPrepareInstance>) return work.context->view.data();
            else return input.view.data();
        }();
        const auto* projection = [&] {
            if constexpr (std::is_same_v<Input, ALMetalShadowPrepareInstance>) return work.context->projection.data();
            else return input.projection.data();
        }();
        bool context_valid = true;
        if constexpr (!std::is_same_v<Input, ALMetalShadowPrepareInstance>)
            context_valid = finite(view, 16) && finite(projection, 16);
        if (!source || input.epoch != work.epoch ||
            input.source_generation != source->generation() ||
            source->requiresPalette() != bool(palette) || !context_valid ||
            (input.has_model && !finite(input.model.data(), 16)))
        {
            work.invalid.store(true, std::memory_order_relaxed);
            return;
        }
        const float* texture_matrix = nullptr;
        if constexpr (std::is_same_v<Input, ALMetalShadowPrepareInstance>)
        {
            if (input.texture_matrix_slot != ALMetalShadowPrepareInstance::no_texture_matrix)
            {
                if (input.texture_matrix_slot >= work.context->texture_matrices.size() ||
                    !finite(work.context->texture_matrices[input.texture_matrix_slot].data(), 16))
                {
                    work.invalid.store(true, std::memory_order_relaxed);
                    return;
                }
                texture_matrix = work.context->texture_matrices[input.texture_matrix_slot].data();
            }
        }
        auto& batch = work.output[index];
        const auto& prototype = source->prototype();
        static_cast<ALMetalWorldSpotShadowValues&>(batch) = prototype;
        batch.borrowPreparedInputs(source, palette,
                                   prototype.coverageDataIdentity());
        if (texture_matrix)
            std::memcpy(batch.texture_matrix, texture_matrix, sizeof(batch.texture_matrix));
        alignas(16) float modelview[16];
        if (input.has_model)
            multiply(input.model.data(), view, modelview);
        else
            std::memcpy(modelview, view, sizeof(modelview));
        multiply(modelview, projection, batch.modelview_projection);
        if (!finite(batch.modelview_projection, 16))
        {
            work.invalid.store(true, std::memory_order_relaxed);
            return;
        }
        if (palette)
        {
            batch.skin_palette = palette->data();
            batch.skin_palette_float_count = palette->size();
            batch.skin_joint_count = palette->jointCount();
        }
    }
}
}

ALMetalShadowSourceRecord::ALMetalShadowSourceRecord(
    std::uint64_t generation, ALMetalWorldSpotShadowBatch prototype,
    std::shared_ptr<const ALMetalBuffer> vertices,
    std::shared_ptr<const ALMetalBuffer> indices,
    std::vector<std::shared_ptr<const ALMetalTexture>> textures, bool rigged)
    : mGeneration(generation), mPrototype(std::move(prototype)),
      mVertices(std::move(vertices)), mIndices(std::move(indices)),
      mTextures(std::move(textures)), mRequiresPalette(rigged)
{}

bool ALMetalShadowSourceRecord::matchesStableInputs(
    const ALMetalWorldSpotShadowBatch& batch) const noexcept
{
    const auto& value = mPrototype;
    return batch.sourceRecord() == this &&
        bool(batch.skin_palette) == mRequiresPalette &&
        (!mRequiresPalette || batch.skinPaletteRecord()) &&
        batch.vertices == value.vertices && batch.indices == value.indices &&
        batch.position_offset == value.position_offset &&
        batch.normal_offset == value.normal_offset &&
        batch.texcoord_offset == value.texcoord_offset &&
        batch.color_offset == value.color_offset &&
        batch.weight_offset == value.weight_offset &&
        batch.cloth_offset == value.cloth_offset &&
        batch.vertex_count == value.vertex_count &&
        batch.index_offset == value.index_offset &&
        batch.index_type == value.index_type && batch.index_count == value.index_count &&
        batch.texture_count == value.texture_count && batch.coverage == value.coverage &&
        batch.double_sided == value.double_sided &&
        batch.terrain == value.terrain && batch.avatar == value.avatar &&
        batch.avatar_cloth == value.avatar_cloth &&
        batch.terrain_smoothing == value.terrain_smoothing &&
        batch.terrain_tessellation_pinned == value.terrain_tessellation_pinned &&
        batch.terrain_height_texture == value.terrain_height_texture &&
        std::bit_cast<std::uint32_t>(batch.terrain_grid_scale) ==
            std::bit_cast<std::uint32_t>(value.terrain_grid_scale) &&
        std::bit_cast<std::uint32_t>(batch.avatar_draw_alpha) ==
            std::bit_cast<std::uint32_t>(value.avatar_draw_alpha) &&
        batch.coverageDataIdentity() == value.coverageDataIdentity() &&
        std::memcmp(batch.avatar_wind, value.avatar_wind, sizeof(value.avatar_wind)) == 0 &&
        std::memcmp(batch.avatar_sin_wave, value.avatar_sin_wave, sizeof(value.avatar_sin_wave)) == 0 &&
        std::memcmp(batch.avatar_gravity, value.avatar_gravity, sizeof(value.avatar_gravity)) == 0 &&
        std::memcmp(batch.avatar_cloth_depth, value.avatar_cloth_depth, sizeof(value.avatar_cloth_depth)) == 0 &&
        std::memcmp(batch.terrain_tessellation_factors, value.terrain_tessellation_factors,
                    sizeof(value.terrain_tessellation_factors)) == 0;
}

std::shared_ptr<const ALMetalShadowSourceRecord> ALMetalShadowSourceRecord::create(
    std::uint64_t generation, const ALMetalWorldSpotShadowBatch& prototype,
    std::shared_ptr<const ALMetalBuffer> vertices,
    std::shared_ptr<const ALMetalBuffer> indices,
    std::vector<std::shared_ptr<const ALMetalTexture>> textures, std::string* error)
{
    const auto index_stride = prototype.index_type == ALMetalIndexType::uint32 ? 4u : 2u;
    const bool covered = prototype.coverage != ALMetalWorldSpotShadowCoverage::opaque;
    const bool pbr = prototype.coverage == ALMetalWorldSpotShadowCoverage::pbr_mask ||
                     prototype.coverage == ALMetalWorldSpotShadowCoverage::pbr_blend;
    const auto* palette = prototype.skinPaletteRecord();
    const bool rigged = prototype.skin_palette || palette ||
                        prototype.skin_palette_float_count || prototype.skin_joint_count;
    if (!generation || !vertices || !indices || prototype.vertices != vertices.get() ||
        prototype.indices != indices.get() || prototype.terrain || prototype.avatar ||
        prototype.avatar_cloth || prototype.terrain_height_texture ||
        prototype.vertex_count == 0 || prototype.index_count == 0 ||
        (prototype.index_type != ALMetalIndexType::uint16 &&
         prototype.index_type != ALMetalIndexType::uint32) ||
        static_cast<unsigned>(prototype.coverage) >
            static_cast<unsigned>(ALMetalWorldSpotShadowCoverage::pbr_blend) ||
        prototype.position_offset % 16 || prototype.index_offset % index_stride ||
        !fits(prototype.position_offset, prototype.vertex_count, 16, vertices->length()) ||
        !fits(prototype.index_offset, prototype.index_count, index_stride, indices->length()))
    {
        fail(error, "Shadow source geometry, generation or lease identity is invalid");
        return {};
    }
    if (textures.size() != prototype.texture_count ||
        (!covered && prototype.texture_count) ||
        (covered && (prototype.texture_count == 0 ||
            prototype.texture_count > ALMetalWorldSpotShadowBatch::max_textures ||
            (pbr && prototype.texture_count > ALMetalWorldSpotShadowBatch::max_pbr_slots) ||
            prototype.texcoord_offset % 8 || prototype.color_offset % 4 ||
            !fits(prototype.texcoord_offset, prototype.vertex_count, 8, vertices->length()) ||
            !fits(prototype.color_offset, prototype.vertex_count, 4, vertices->length()) ||
            !finite(prototype.texture_matrix, 16))))
    {
        fail(error, "Shadow source coverage layout is invalid");
        return {};
    }
    for (std::size_t slot = 0; slot < textures.size(); ++slot)
    {
        const auto& coverage = prototype.coverageData();
        if (!textures[slot] || textures[slot].get() != coverage.textures[slot] ||
            textures[slot]->descriptor().type != ALMetalTextureType::texture_2d ||
            !alMetalHasUsage(textures[slot]->descriptor().usage, ALMetalTextureUsage::shader_read) ||
            !finite(coverage.alpha_cutoffs[slot]) || coverage.alpha_cutoffs[slot] < 0.f ||
            coverage.alpha_cutoffs[slot] > 1.f ||
            (pbr && !finite(coverage.pbr_base_color_transforms[slot][0].data(), 8)))
        {
            fail(error, "Shadow source texture lease or coverage values are invalid");
            return {};
        }
    }
    if ((!rigged && prototype.weight_offset != 0) ||
        (rigged && (!palette ||
        prototype.skin_palette != palette->data() ||
        prototype.skin_palette_float_count != palette->size() ||
        prototype.skin_joint_count != palette->jointCount() ||
        prototype.weight_offset % 16 ||
        !fits(prototype.weight_offset, prototype.vertex_count, 16, vertices->length()))))
    {
        fail(error, "Shadow source requires a certified attachment palette and valid weights");
        return {};
    }
    try
    {
        auto sealed = prototype;
        sealed.freezeCoverageData();
        // Pose and view identity belong to the admitted per-view packet, never
        // the retained geometry/material publication.
        sealed.skin_palette_record.reset();
        sealed.source_record.reset();
        sealed.clearBorrowedInputs();
        sealed.skin_palette = nullptr;
        sealed.skin_palette_float_count = 0;
        sealed.skin_joint_count = 0;
        std::memcpy(sealed.modelview_projection, alMetalShadowIdentity.data(),
                    sizeof(sealed.modelview_projection));
        return std::shared_ptr<const ALMetalShadowSourceRecord>(
            new ALMetalShadowSourceRecord(generation, std::move(sealed),
                std::move(vertices), std::move(indices), std::move(textures), rigged));
    }
    catch (const std::bad_alloc&)
    {
        fail(error, "Could not allocate immutable shadow source");
        return {};
    }
}

namespace
{
template<typename Input>
bool prepareBatches(
    std::span<const Input> inputs,
    const ALMetalShadowPrepareContext* context,
    std::span<ALMetalWorldSpotShadowBatch> output,
    std::uint64_t epoch, const std::atomic<std::uint64_t>* current_epoch,
    bool allow_dispatch, ALMetalShadowPrepareStats* stats, std::string* error,
    ALNativeFrameBudget* budget = nullptr)
{
    if (stats) *stats = {};
    const auto clear = [&]
    {
        for (auto& batch : output) batch = ALMetalWorldSpotShadowBatch{};
    };
    if (inputs.size() != output.size())
    {
        clear();
        return fail(error, "Shadow preparation input/output lengths differ");
    }
    if (current_epoch && current_epoch->load(std::memory_order_acquire) != epoch)
    {
        clear();
        if (stats) *stats = {inputs.size(), 0, false, true};
        return fail(error, "Shadow preparation epoch was cancelled");
    }
    if constexpr (std::is_same_v<Input, ALMetalShadowPrepareInstance>)
    {
        if (!context || !finite(context->view.data(), 16) || !finite(context->projection.data(), 16))
        {
            clear();
            return fail(error, "Shadow preparation view context is invalid");
        }
    }
    if (inputs.empty()) return true;
    std::array<ALNativeFrameBudget::Lease, 2> range_credits;
    if (budget && allow_dispatch && inputs.size() >= 256)
    {
        try
        {
            range_credits[0] = budget->tryAcquire(ALNativeFrameBudget::Work::preparation).credit;
            if (range_credits[0])
                range_credits[1] = budget->tryAcquire(ALNativeFrameBudget::Work::preparation).credit;
        }
        catch (const std::bad_alloc&) {} // Optional credit allocation cannot remove required preparation.
        if (!range_credits[0] || !range_credits[1])
        {
            // A first reservation must not occupy the remaining shared
            // slot while the required packet falls back to the owner.
            range_credits[1].reset();
            range_credits[0].reset();
            allow_dispatch = false;
        }
    }
    // Release old output owners on the calling owner before jobs start. The
    // range kernel only copies scalar values and writes immutable borrows.
    for (auto& batch : output) batch.clearInputOwnership();
    const std::size_t jobs = allow_dispatch && inputs.size() >= 256 ? 2 : 1;
    Preparation<Input> work{inputs, output, epoch, current_epoch, jobs, context};
    if (jobs == 2)
        dispatch_apply_f(jobs, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                         &work, prepareRange<Input>);
    else
        prepareRange<Input>(&work, 0);
    const bool cancelled = work.cancelled.load(std::memory_order_relaxed) ||
        (current_epoch && current_epoch->load(std::memory_order_acquire) != epoch);
    if (stats) *stats = {inputs.size(), jobs, jobs == 2, cancelled};
    if (cancelled || work.invalid.load(std::memory_order_relaxed))
    {
        clear();
        return fail(error, cancelled ? "Shadow preparation epoch was cancelled" :
                                      "Shadow preparation input generation, palette or matrix is invalid");
    }
    return true;
}
}

bool alMetalPrepareShadowBatches(
    std::span<const ALMetalShadowPrepareInput> inputs,
    std::span<ALMetalWorldSpotShadowBatch> output,
    std::uint64_t epoch, const std::atomic<std::uint64_t>* current_epoch,
    bool allow_dispatch, ALMetalShadowPrepareStats* stats, std::string* error)
{
    return prepareBatches(inputs, nullptr, output, epoch, current_epoch, allow_dispatch, stats, error);
}

bool alMetalPrepareShadowBatches(
    std::span<const ALMetalShadowPrepareInstance> inputs,
    const ALMetalShadowPrepareContext& context,
    std::span<ALMetalWorldSpotShadowBatch> output,
    std::uint64_t epoch, const std::atomic<std::uint64_t>* current_epoch,
    bool allow_dispatch, ALMetalShadowPrepareStats* stats, std::string* error)
{
    const ALMetalShadowPrepareContext immutable_context = context;
    return prepareBatches(inputs, &immutable_context, output, epoch, current_epoch, allow_dispatch, stats, error);
}

bool alMetalPrepareShadowBatches(
    std::span<const ALMetalShadowPrepareInstance> inputs,
    const ALMetalShadowPrepareContext& context,
    std::span<ALMetalWorldSpotShadowBatch> output,
    ALNativeFrameBudget& budget,
    std::uint64_t epoch, const std::atomic<std::uint64_t>* current_epoch,
    bool allow_dispatch, ALMetalShadowPrepareStats* stats, std::string* error)
{
    const ALMetalShadowPrepareContext immutable_context = context;
    return prepareBatches(inputs, &immutable_context, output, epoch, current_epoch,
                          allow_dispatch, stats, error, &budget);
}
