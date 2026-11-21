/**
 * @file pipeline_producer_recipe_native.inl
 * @brief Ordinary producer recipes from authoritative draw-pool semantics.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
namespace ALSceneProducerDrainNative
{
struct Policy
{
    bool native_groups = false;
    bool cohort_bounds = false, heavy_bounds = false;
    ALSceneSourcePolicyNative::OrdinaryPass pass;
    U64 signature() const noexcept
    {
        return 1 | (U64(native_groups) << 3) | (U64(cohort_bounds) << 4) |
            (U64(pass.animated_sources_active) << 6) |
            (U64(pass.animated_direct_stability) << 8) | (U64(heavy_bounds) << 9);
    }
};

// Same ordinary maps as the draw-pool callers. Unsupported special/alpha/body
// passes remain authored required factories, never guessed from material data.
inline std::optional<ALScenePublicationNative::WorldFlavor> ordinaryFlavor(
    U32 type, const LLDrawInfo& info, const Policy& policy)
{
    auto pass = policy.pass;
    U32 family = 0;
    bool indexed = true, supported = false;
    if (type == LLRenderPass::PASS_SIMPLE || type == LLRenderPass::PASS_SIMPLE_RIGGED ||
        type == LLRenderPass::PASS_ALPHA_MASK || type == LLRenderPass::PASS_ALPHA_MASK_RIGGED ||
        type == LLRenderPass::PASS_GRASS)
    {
        pass.rigged = type == LLRenderPass::PASS_SIMPLE_RIGGED || type == LLRenderPass::PASS_ALPHA_MASK_RIGGED;
        pass.alpha_mask = type == LLRenderPass::PASS_ALPHA_MASK || type == LLRenderPass::PASS_ALPHA_MASK_RIGGED ||
            type == LLRenderPass::PASS_GRASS;
        indexed = type != LLRenderPass::PASS_GRASS;
        // The actual simple alpha-mask pool delegates indexed legacy slots.
        if (pass.alpha_mask && indexed && info.mMaterialSlotList.size() > 1) return {};
        supported = true;
    }
    else
    {
        for (const auto& description : LLDrawPoolMaterials::DEFERRED_PASSES)
        {
            if (type != description.render_type && type != description.render_type + 1) continue;
            family = 1;
            pass.rigged = type == description.render_type + 1;
            pass.material_normal_map = description.hasNormalMap();
            pass.material_specular_map = description.hasSpecularMap();
            pass.material_alpha_mode = description.alphaMode();
            indexed = info.mMaterialSlotList.size() > 1;
            supported = true;
            break;
        }
        if (!supported && (type == LLRenderPass::PASS_GLTF_PBR || type == LLRenderPass::PASS_GLTF_PBR_RIGGED ||
            type == LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK || type == LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK_RIGGED))
        {
            family = 2;
            pass.rigged = type == LLRenderPass::PASS_GLTF_PBR_RIGGED || type == LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK_RIGGED;
            pass.alpha_mask = type == LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK || type == LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK_RIGGED;
            indexed = info.mGLTFMaterialList.size() > 1;
            supported = true;
        }
    }
    if (!supported) return {};
    auto flavor = ALSceneSourcePolicyNative::select(info, family, indexed, false, pass).flavor;
    // The base producer ticket must not nest the separate per-source animated
    // capability admission. Current poses remain live; optional animated
    // certificates use their existing admitted promotion phase over this base.
    flavor.animated_source = flavor.stabilize_animated_direct = false;
    // Cohort certificates are optional supersets, not different shading keys.
    flavor.cohort_source = policy.native_groups && !pass.rigged && !info.mTextureMatrix &&
        (type == LLRenderPass::PASS_SIMPLE || family != 0);
    flavor.cohort_bounds = flavor.cohort_bounds || (flavor.cohort_source && policy.cohort_bounds);
    return flavor;
}

}
