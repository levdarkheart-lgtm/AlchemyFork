/**
 * @file pipeline_source_policy_native.inl
 * @brief Shared native source semantics for producer preparation and capture.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#pragma once

namespace ALSceneSourcePolicyNative
{
// Only stable pass semantics and optional capabilities. View transforms,
// current poses, UV overrides, draw order and queries remain capture inputs.
struct OrdinaryPass
{
    bool visibility_metadata_failed = false;
    bool impostor_alpha_active = false, world_post_deferred_active = false;
    bool source_rows_active = false;
    bool material_normal_map = false, material_specular_map = false;
    U32 material_alpha_mode = 0;
    bool alpha_mask = false, rigged = false, pbr_glow = false;
    bool post_water_pbr_glow = false;
    bool impostor_fullbright = false, impostor_shiny = false, impostor_emboss = false;
    bool impostor_bake = false;
    bool animated_sources_active = false, animated_gpu_records_active = false;
    bool animated_direct_stability = false;
};

struct Selection
{
    ALScenePublicationNative::WorldFlavor flavor;
    bool ordinary_animated = false;
};

template<typename Capture>
Selection select(const LLDrawInfo& params, U32 family, bool indexed, bool editable,
    const Capture& capture, bool native_visibility, bool native_resources, bool native_groups)
{
    Selection selected;
    auto& flavor = selected.flavor;
    // These expressions are shared with the actual required capture, rather
    // than inferring shading semantics from a material pointer or pass number.
    flavor.cohort_bounds = native_visibility && !native_groups && !capture.visibility_metadata_failed;
    flavor.native_materials = !capture.impostor_alpha_active &&
        !capture.world_post_deferred_active && !capture.pbr_glow;
    flavor.native_resources = (native_resources || capture.source_rows_active) && flavor.native_materials;
    flavor.family = static_cast<ALScenePublicationNative::WorldFamily>(family);
    flavor.indexed_textures = family == 0 && indexed;
    flavor.indexed_materials = family != 0 && indexed;
    flavor.normal_map = capture.material_normal_map;
    flavor.specular_map = capture.material_specular_map;
    flavor.material_alpha_mode = capture.material_alpha_mode;
    flavor.alpha_mask = capture.alpha_mask;
    flavor.rigged = capture.rigged;
    flavor.pbr_glow = capture.pbr_glow;
    flavor.post_water_pbr_glow = family == 2 && capture.pbr_glow && capture.post_water_pbr_glow;
    flavor.impostor_fullbright = capture.impostor_fullbright;
    flavor.impostor_shiny = capture.impostor_shiny;
    flavor.impostor_emboss = capture.impostor_emboss;
    selected.ordinary_animated = !editable && capture.rigged &&
        !capture.impostor_bake && !capture.impostor_alpha_active && !capture.world_post_deferred_active &&
        !capture.pbr_glow && !capture.impostor_fullbright && !capture.impostor_shiny && !capture.impostor_emboss &&
        !params.mTextureMatrix && flavor.hasAnimatedConsumer();
    flavor.animated_source = capture.animated_sources_active && selected.ordinary_animated;
    flavor.animated_gpu_source = capture.animated_gpu_records_active && selected.ordinary_animated;
    flavor.stabilize_animated_direct = capture.animated_direct_stability && selected.ordinary_animated;
    return selected;
}
}
