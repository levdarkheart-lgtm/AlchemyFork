/**
 * @file almetalworlddiagnostics.cpp
 * @brief Bounded CPU-only post-deferred workload provenance.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "almetalworlddiagnostics.h"
#include "almetalworld.h"

#include <algorithm>

namespace
{
    constexpr std::uint64_t seed = 14695981039346656037ull;
    void hashBytes(std::uint64_t& hash, const void* bytes, std::size_t size)
    {
        const auto* data = static_cast<const unsigned char*>(bytes);
        for (std::size_t i = 0; i < size; ++i)
            hash = (hash ^ data[i]) * 1099511628211ull;
    }
    template <typename T> void hashValue(std::uint64_t& hash, const T& value)
    {
        hashBytes(hash, &value, sizeof(value));
    }
    ALMetalWorldPostFamily family(const ALMetalWorldSimpleBatch& batch)
    {
        using F = ALMetalWorldPostFamily;
        // Same branch precedence as renderOpaquePostDeferred's draw loop.
        if (batch.water_haze) return F::water_haze;
        if (batch.atmospheric_haze) return F::atmospheric_haze;
        if (batch.water_surface) return F::water_surface;
        if (batch.avatar_alpha) return F::alpha_avatar;
        if (batch.pre_water_alpha || batch.post_water_alpha)
        {
            if (batch.pre_water_pbr) return F::alpha_pbr;
            if (batch.pre_water_material) return F::alpha_material;
            if (batch.pre_water_fullbright) return F::alpha_fullbright;
            return F::alpha_simple;
        }
        if (batch.water_exclusion) return F::water_exclusion;
        if (batch.legacy_glow || batch.pbr_glow) return F::glow;
        if (batch.impostor_emboss) return F::emboss;
        if (batch.impostor_shiny) return F::shiny;
        return F::fullbright;
    }
    void texture(const ALMetalTexture* value,
                 std::uint64_t& descriptors, std::uint64_t& resources)
    {
        const auto identity = reinterpret_cast<std::uintptr_t>(
            value ? value->nativeHandle() : nullptr);
        hashValue(resources, identity);
        const bool present = value != nullptr;
        hashValue(descriptors, present);
        if (!value) return;
        const auto& d = value->descriptor();
        hashValue(descriptors, d.type);
        hashValue(descriptors, d.format);
        hashValue(descriptors, d.width);
        hashValue(descriptors, d.height);
        hashValue(descriptors, d.array_length);
        hashValue(descriptors, d.mipmap_level_count);
        hashValue(descriptors, d.sample_count);
    }
}

ALMetalWorldPostDiagnostics alMetalDescribeOpaquePost(
    const ALMetalWorldOpaquePostDeferredScene& scene,
    std::uint64_t sequence) noexcept
{
    ALMetalWorldPostDiagnostics out;
    out.valid = true;
    out.sample_sequence = out.encoded_calls = sequence;
    out.source_frame_index = scene.diagnostic_frame_index;
    out.source_view_kind = scene.diagnostic_view_kind;
    out.source_view_index = scene.diagnostic_view_index;
    out.width = scene.width;
    out.height = scene.height;
    out.flags = (scene.reverse_z ? 1u : 0u) | (scene.hdr ? 2u : 0u) |
        (scene.mirror_clip ? 4u : 0u) | (scene.seed_cutout_coverage ? 8u : 0u) |
        (scene.wireframe ? 16u : 0u) | (scene.reflection_probes ? 32u : 0u) |
        (scene.legacy_environment ? 64u : 0u) | (scene.hero_probe ? 128u : 0u) |
        (scene.screen_space_reflections ? 256u : 0u) |
        (scene.atmospheric_haze ? 512u : 0u) | (scene.water_surface ? 1024u : 0u) |
        (scene.water_haze ? 2048u : 0u) |
        (scene.reflection_probe_volume_constants ? 4096u : 0u);
    out.probe_count = scene.reflection_probes
        ? scene.reflection_probes->constants.config[0] : 0;
    out.batch_count = scene.batch_count;
    out.ordered_geometry_hash = seed;
    out.texture_descriptor_hash = seed;
    out.process_resource_hash = seed;
    bool initial = true;
    std::size_t segment = 0;
    for (std::size_t i = 0; scene.batches && i < scene.batch_count; ++i)
    {
        const auto& batch = scene.batches[i];
        const auto kind = family(batch);
        const auto n = static_cast<std::size_t>(kind);
        if (kind == ALMetalWorldPostFamily::water_haze) segment = 1;
        if (kind == ALMetalWorldPostFamily::atmospheric_haze) segment = 2;
        if (kind == ALMetalWorldPostFamily::water_surface) segment = 3;
        if (segment != 0) initial = false;
        ++out.batches[n];
        ++out.segment_batches[segment][n];
        out.indices[n] += batch.index_count;
        if (initial)
        {
            ++out.initial_batch_count;
            ++out.initial_batches[n];
        }
        std::uint64_t item = seed;
        hashValue(item, kind);
        hashValue(item, batch.vertex_count);
        hashValue(item, batch.index_count);
        hashValue(item, batch.index_type);
        hashValue(item, batch.position_offset);
        hashValue(item, batch.index_offset);
        hashValue(item, batch.texture_count);
        hashValue(item, batch.material_slot_count);
        hashValue(item, batch.pbr_slot_count);
        for (const bool flag : {batch.alpha_mask, batch.double_sided,
             batch.avatar_rigid, batch.avatar_skinning, batch.avatar_cloth,
             batch.avatar_depth_write, batch.avatar_color_write,
             batch.impostor_debug_outline, batch.pre_water_alpha,
             batch.post_water_alpha, batch.post_water_depth_only,
             batch.post_water_glow, batch.pre_water_shadows,
             batch.pre_water_particle, batch.pre_water_alpha_writes_coverage,
             batch.use_vertex_color, batch.alpha_cutoff_bias})
            hashValue(item, flag);
        hashValue(item, batch.avatar_cull_mode);
        hashValue(item, batch.pre_water_source_rgb);
        hashValue(item, batch.pre_water_destination_rgb);
        hashValue(item, batch.alpha_cutoff);
        hashValue(item, batch.pre_water_minimum_alpha);
        hashBytes(item, batch.modelview_projection, sizeof(batch.modelview_projection));
        hashBytes(item, batch.modelview, sizeof(batch.modelview));
        hashBytes(item, batch.texture_matrix, sizeof(batch.texture_matrix));
        hashValue(out.ordered_geometry_hash, item);
        out.unordered_geometry_hash += item;
        for (const auto* buffer : {batch.vertices, batch.indices})
        {
            const auto id = reinterpret_cast<std::uintptr_t>(
                buffer ? buffer->nativeHandle() : nullptr);
            hashValue(out.process_resource_hash, id);
        }
        const auto count = std::min<std::size_t>(batch.texture_count,
            ALMetalWorldSimpleBatch::max_diffuse_textures);
        for (std::size_t slot = 0; slot < count; ++slot)
            texture(batch.diffuse_textures[slot], out.texture_descriptor_hash,
                    out.process_resource_hash);
        texture(batch.normal_texture, out.texture_descriptor_hash,
                out.process_resource_hash);
        for (std::size_t slot = 0; slot < std::min<std::size_t>(
                 batch.material_slot_count, ALMetalWorldSimpleBatch::max_material_slots); ++slot)
        {
            texture(batch.materialSlots()[slot].normal_texture,
                    out.texture_descriptor_hash, out.process_resource_hash);
            texture(batch.materialSlots()[slot].specular_texture,
                    out.texture_descriptor_hash, out.process_resource_hash);
        }
        for (std::size_t slot = 0; slot < std::min<std::size_t>(
                 batch.pbr_slot_count, ALMetalWorldSimpleBatch::max_material_slots); ++slot)
        {
            texture(batch.pbrSlots()[slot].normal_texture,
                    out.texture_descriptor_hash, out.process_resource_hash);
            texture(batch.pbrSlots()[slot].orm_texture,
                    out.texture_descriptor_hash, out.process_resource_hash);
            texture(batch.pbrSlots()[slot].emissive_texture,
                    out.texture_descriptor_hash, out.process_resource_hash);
        }
    }
    out.tail_segment = static_cast<std::uint32_t>(segment);
    out.auxiliary_items = scene.highlight_batch_count + scene.debug_blip_count +
        scene.wind_vector_point_count + scene.wind_vector_line_vertex_count +
        scene.agent_target_line_vertex_count + scene.avatar_volume_batch_count +
        scene.avatar_joint_batch_count + scene.bounding_box_batch_count +
        scene.raycast_batch_count + scene.occlusion_batch_count +
        scene.octree_batch_count + scene.physics_shape_batch_count +
        scene.normal_batch_count + scene.debug_point_batch_count +
        scene.texture_anim_batch_count + scene.update_type_batch_count +
        scene.texel_density_batch_count + scene.batch_size_batch_count +
        scene.light_debug_batch_count + scene.shadow_frustum_batch_count +
        scene.reflection_probe_link_vertex_count + scene.far_plane_overlay_column_count +
        scene.composition_debug_height_count;
    return out;
}
