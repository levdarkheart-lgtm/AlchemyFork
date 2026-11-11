/**
 * @file llshadermgr.cpp
 * @brief Reserved native render-pass attributes and uniform metadata.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "llshadermgr.h"
#include "lldir.h"
#if LL_DARWIN
#include "almetalpipelinecache.h"
#endif

#include <set>

LLShaderMgr * LLShaderMgr::sInstance = NULL;

LLShaderMgr::LLShaderMgr()
{
}


LLShaderMgr::~LLShaderMgr()
{
}

// static
LLShaderMgr * LLShaderMgr::instance()
{
    if(NULL == sInstance)
    {
        LL_ERRS("Shaders") << "LLShaderMgr should already have been instantiated by the application!" << LL_ENDL;
    }

    return sInstance;
}

void LLShaderMgr::clearShaderCache()
{
    // Invalidate live archive writers before purging files, so teardown cannot
    // restore pre-purge state. Actual PSO replacement remains renderer-owned.
    const std::string shader_cache = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "shader_cache");
#if LL_DARWIN
    std::string error;
    if (!ALMetalPipelineCache::clearDirectory(shader_cache, &error))
        LL_WARNS("ShaderCache") << "Could not clear native pipeline cache: "
                                << error << LL_ENDL;
#endif
    LL_INFOS("ShaderMgr") << "Removing shader cache at " << shader_cache << LL_ENDL;
    gDirUtilp->deleteFilesInDir(shader_cache, "*");
}

//virtual
void LLShaderMgr::initAttribsAndUniforms()
{
    //MUST match order of enum in LLVertexBuffer.h
    mReservedAttribs.push_back("position");
    mReservedAttribs.push_back("normal");
    mReservedAttribs.push_back("texcoord0");
    mReservedAttribs.push_back("texcoord1");
    mReservedAttribs.push_back("texcoord2");
    mReservedAttribs.push_back("texcoord3");
    mReservedAttribs.push_back("diffuse_color");
    mReservedAttribs.push_back("emissive");
    mReservedAttribs.push_back("tangent");
    mReservedAttribs.push_back("weight");
    mReservedAttribs.push_back("weight4");
    mReservedAttribs.push_back("clothing");
    mReservedAttribs.push_back("joint");
    mReservedAttribs.push_back("texture_index");

    //matrix state
    mReservedUniforms.push_back("modelview_matrix");
    mReservedUniforms.push_back("projection_matrix");
    mReservedUniforms.push_back("inv_proj");
    mReservedUniforms.push_back("modelview_projection_matrix");
    mReservedUniforms.push_back("inv_modelview");
    mReservedUniforms.push_back("identity_matrix");
    mReservedUniforms.push_back("normal_matrix");
    mReservedUniforms.push_back("texture_matrix0");
    mReservedUniforms.push_back("texture_matrix1");
    mReservedUniforms.push_back("object_plane_s");
    mReservedUniforms.push_back("object_plane_t");

    mReservedUniforms.push_back("texture_base_color_transform"); // (GLTF)
    mReservedUniforms.push_back("texture_normal_transform"); // (GLTF)
    mReservedUniforms.push_back("texture_metallic_roughness_transform"); // (GLTF)
    mReservedUniforms.push_back("texture_emissive_transform"); // (GLTF)
    mReservedUniforms.push_back("base_color_texcoord"); // (GLTF)
    mReservedUniforms.push_back("emissive_texcoord"); // (GLTF)
    mReservedUniforms.push_back("normal_texcoord"); // (GLTF)
    mReservedUniforms.push_back("metallic_roughness_texcoord"); // (GLTF)

    mReservedUniforms.push_back("terrain_uv_transform");
    mReservedUniforms.push_back("terrain_uv_offset");
    mReservedUniforms.push_back("terrain_normal_axes");
    mReservedUniforms.push_back("terrain_hex_frac");
    mReservedUniforms.push_back("terrain_hex_base");

    llassert(mReservedUniforms.size() == LLShaderMgr::TERRAIN_HEX_BASE +1);

    mReservedUniforms.push_back("viewport");

    mReservedUniforms.push_back("light_position");
    mReservedUniforms.push_back("light_direction");
    mReservedUniforms.push_back("light_attenuation");
    mReservedUniforms.push_back("light_deferred_attenuation");
    mReservedUniforms.push_back("light_diffuse");
    mReservedUniforms.push_back("light_ambient");
    mReservedUniforms.push_back("light_count");
    mReservedUniforms.push_back("light");
    mReservedUniforms.push_back("light_col");
    mReservedUniforms.push_back("far_z");

    llassert(mReservedUniforms.size() == LLShaderMgr::MULTI_LIGHT_FAR_Z+1);

    //NOTE: MUST match order in eGLSLReservedUniforms
    mReservedUniforms.push_back("proj_mat");
    mReservedUniforms.push_back("proj_near");
    mReservedUniforms.push_back("proj_p");
    mReservedUniforms.push_back("proj_n");
    mReservedUniforms.push_back("proj_origin");
    mReservedUniforms.push_back("proj_range");
    mReservedUniforms.push_back("proj_ambiance");
    mReservedUniforms.push_back("proj_focus");
    mReservedUniforms.push_back("proj_lod");
    mReservedUniforms.push_back("proj_ambient_lod");

    llassert(mReservedUniforms.size() == LLShaderMgr::PROJECTOR_AMBIENT_LOD+1);

    mReservedUniforms.push_back("color");
    mReservedUniforms.push_back("emissiveColor");
    mReservedUniforms.push_back("metallicFactor");
    mReservedUniforms.push_back("roughnessFactor");
    mReservedUniforms.push_back("baseColorAlpha");
    mReservedUniforms.push_back("clipPlane");
    mReservedUniforms.push_back("clipSign");

    mReservedUniforms.push_back("diffuseMap");
    mReservedUniforms.push_back("altDiffuseMap");
    mReservedUniforms.push_back("specularMap");
    mReservedUniforms.push_back("metallicRoughnessMap");
    mReservedUniforms.push_back("normalMap");
    mReservedUniforms.push_back("emissiveMap");
    mReservedUniforms.push_back("bumpMap");
    mReservedUniforms.push_back("bumpMap2");
    mReservedUniforms.push_back("environmentMap");
    mReservedUniforms.push_back("reflectionProbes");
    mReservedUniforms.push_back("shCoeffs");
    mReservedUniforms.push_back("heroProbes");
    mReservedUniforms.push_back("cloud_noise_texture");
    mReservedUniforms.push_back("cloud_noise_texture_next");
    mReservedUniforms.push_back("lightnorm");
    mReservedUniforms.push_back("sunlight_color");
    mReservedUniforms.push_back("ambient_color");
    mReservedUniforms.push_back("sky_hdr_scale");
    mReservedUniforms.push_back("sky_sunlight_scale");
    mReservedUniforms.push_back("sky_ambient_scale");
    mReservedUniforms.push_back("blue_horizon");
    mReservedUniforms.push_back("blue_density");
    mReservedUniforms.push_back("haze_horizon");
    mReservedUniforms.push_back("haze_density");
    mReservedUniforms.push_back("cloud_shadow");
    mReservedUniforms.push_back("density_multiplier");
    mReservedUniforms.push_back("distance_multiplier");
    mReservedUniforms.push_back("max_y");
    mReservedUniforms.push_back("glow");
    mReservedUniforms.push_back("cloud_color");
    mReservedUniforms.push_back("cloud_pos_density1");
    mReservedUniforms.push_back("cloud_pos_density2");
    mReservedUniforms.push_back("cloud_scale");
    mReservedUniforms.push_back("gamma");
    mReservedUniforms.push_back("scene_light_strength");

    llassert(mReservedUniforms.size() == LLShaderMgr::SCENE_LIGHT_STRENGTH+1);

    mReservedUniforms.push_back("center");
    mReservedUniforms.push_back("size");
    mReservedUniforms.push_back("falloff");

    mReservedUniforms.push_back("box_center");
    mReservedUniforms.push_back("box_size");


    mReservedUniforms.push_back("minLuminance");
    mReservedUniforms.push_back("maxExtractAlpha");
    mReservedUniforms.push_back("lumWeights");
    mReservedUniforms.push_back("warmthWeights");
    mReservedUniforms.push_back("warmthAmount");
    mReservedUniforms.push_back("glowStrength");
    mReservedUniforms.push_back("glowDelta");
    mReservedUniforms.push_back("glowNoiseMap");

    llassert(mReservedUniforms.size() == LLShaderMgr::GLOW_NOISE_MAP+1);

    mReservedUniforms.push_back("bloom_threshold");
    mReservedUniforms.push_back("bloom_knee");
    mReservedUniforms.push_back("bloom_firefly_clamp");
    mReservedUniforms.push_back("bloom_texel_size");
    mReservedUniforms.push_back("bloom_scatter");
    mReservedUniforms.push_back("bloom_strength");
    mReservedUniforms.push_back("alpha_glow_boost");
    mReservedUniforms.push_back("bloomMap");
    mReservedUniforms.push_back("halation_strength");
    mReservedUniforms.push_back("halation_tint");
    mReservedUniforms.push_back("halation_lum_weights");

    llassert(mReservedUniforms.size() == LLShaderMgr::HALATION_LUM_WEIGHTS+1);


    mReservedUniforms.push_back("minimum_alpha");
    mReservedUniforms.push_back("emissive_brightness");
    mReservedUniforms.push_back("vertex_alpha_coverage");

    // Deferred
    mReservedUniforms.push_back("env_mat");
    mReservedUniforms.push_back("ssao_radius");
    mReservedUniforms.push_back("ssao_max_radius");
    mReservedUniforms.push_back("ssao_factor");
    mReservedUniforms.push_back("ssao_factor_inv");
    mReservedUniforms.push_back("ssao_effect_mat");
    mReservedUniforms.push_back("ssao_irradiance_scale");
    mReservedUniforms.push_back("ssao_irradiance_max");
    mReservedUniforms.push_back("screen_res");
    mReservedUniforms.push_back("near_clip");
    mReservedUniforms.push_back("sun_dir");
    mReservedUniforms.push_back("moon_dir");
    mReservedUniforms.push_back("depth_cutoff");
    mReservedUniforms.push_back("norm_cutoff");
    llassert(mReservedUniforms.size() == LLShaderMgr::DEFERRED_NORM_CUTOFF + 1);

    mReservedUniforms.push_back("iterationCount");
    mReservedUniforms.push_back("rayStep");
    mReservedUniforms.push_back("distanceBias");
    mReservedUniforms.push_back("depthRejectBias");
    mReservedUniforms.push_back("glossySampleCount");
    mReservedUniforms.push_back("noiseSine");
    mReservedUniforms.push_back("adaptiveStepMultiplier");

    mReservedUniforms.push_back("modelview_delta");
    mReservedUniforms.push_back("inv_modelview_delta");
    mReservedUniforms.push_back("cube_snapshot");

    mReservedUniforms.push_back("tc_scale");
    mReservedUniforms.push_back("rcp_screen_res");
    mReservedUniforms.push_back("rcp_frame_opt");
    mReservedUniforms.push_back("rcp_frame_opt2");

    mReservedUniforms.push_back("focal_distance");
    mReservedUniforms.push_back("blur_constant");
    mReservedUniforms.push_back("tan_pixel_angle");
    mReservedUniforms.push_back("magnification");
    mReservedUniforms.push_back("max_cof");
    mReservedUniforms.push_back("res_scale");
    mReservedUniforms.push_back("dof_width");
    mReservedUniforms.push_back("dof_height");
    mReservedUniforms.push_back("dof_uv_scale");

    mReservedUniforms.push_back("depthMap");
    llassert(mReservedUniforms.size() == LLShaderMgr::DEFERRED_DEPTH+1);

    mReservedUniforms.push_back("diffuseRect");
    mReservedUniforms.push_back("specularRect");
    mReservedUniforms.push_back("emissiveRect");
    mReservedUniforms.push_back("noiseMap");
    mReservedUniforms.push_back("lightMap");
    mReservedUniforms.push_back("projectionMap");
    mReservedUniforms.push_back("norm_mat");
    mReservedUniforms.push_back("impostor_norm_rot");

    mReservedUniforms.push_back("specular_color");
    mReservedUniforms.push_back("env_intensity");

    mReservedUniforms.push_back("matrixPalette");
    mReservedUniforms.push_back("skin_origin");

    mReservedUniforms.push_back("screenTex");
    mReservedUniforms.push_back("exclusionDistort");
    mReservedUniforms.push_back("eyeVec");
    mReservedUniforms.push_back("time");
    mReservedUniforms.push_back("waterBigPhase");
    mReservedUniforms.push_back("waterLittlePhase");
    mReservedUniforms.push_back("lightDir");
    mReservedUniforms.push_back("specular");
    mReservedUniforms.push_back("lightExp");
    mReservedUniforms.push_back("waterFogColor");
    mReservedUniforms.push_back("waterFogColorLinear");
    mReservedUniforms.push_back("waterFogDensity");
    mReservedUniforms.push_back("waterFogKS");
    mReservedUniforms.push_back("refScale");
    mReservedUniforms.push_back("waterPlane");
    mReservedUniforms.push_back("waterSign");
    mReservedUniforms.push_back("normScale");
    mReservedUniforms.push_back("fresnelScale");
    mReservedUniforms.push_back("fresnelOffset");
    mReservedUniforms.push_back("blurMultiplier");
    mReservedUniforms.push_back("above_water");
    llassert(mReservedUniforms.size() == LLShaderMgr::WATER_ABOVE_WATER + 1);

    mReservedUniforms.push_back("camPosLocal");
// [RLVa:KB] - @setsphere
    mReservedUniforms.push_back("rlvEffectMode");
    mReservedUniforms.push_back("rlvEffectParam1");
    mReservedUniforms.push_back("rlvEffectParam2");
    mReservedUniforms.push_back("rlvEffectParam3");
    mReservedUniforms.push_back("rlvEffectParam4");
    mReservedUniforms.push_back("rlvEffectParam5");
// [/RLV:KB]

    mReservedUniforms.push_back("gWindDir");
    mReservedUniforms.push_back("gSinWaveParams");
    mReservedUniforms.push_back("gGravity");
    mReservedUniforms.push_back("gClothDepthRow");

    mReservedUniforms.push_back("detail_0");
    mReservedUniforms.push_back("detail_1");
    mReservedUniforms.push_back("detail_2");
    mReservedUniforms.push_back("detail_3");

    mReservedUniforms.push_back("alpha_ramp");
    mReservedUniforms.push_back("paint_map");

    mReservedUniforms.push_back("terrain_height_map");
    mReservedUniforms.push_back("terrain_composition_map");
    mReservedUniforms.push_back("terrain_tess_origin");
    mReservedUniforms.push_back("terrain_tess_density");
    mReservedUniforms.push_back("terrain_tess_clip");
    mReservedUniforms.push_back("terrain_tess_clip_origin");
    mReservedUniforms.push_back("terrain_tess_raster");
    mReservedUniforms.push_back("terrain_grid_scale");
    mReservedUniforms.push_back("terrain_smoothing");
    mReservedUniforms.push_back("parcel_overlay");
    mReservedUniforms.push_back("show_parcel_owners");

    mReservedUniforms.push_back("detail_0_base_color");
    mReservedUniforms.push_back("detail_1_base_color");
    mReservedUniforms.push_back("detail_2_base_color");
    mReservedUniforms.push_back("detail_3_base_color");
    mReservedUniforms.push_back("detail_0_normal");
    mReservedUniforms.push_back("detail_1_normal");
    mReservedUniforms.push_back("detail_2_normal");
    mReservedUniforms.push_back("detail_3_normal");
    mReservedUniforms.push_back("detail_0_metallic_roughness");
    mReservedUniforms.push_back("detail_1_metallic_roughness");
    mReservedUniforms.push_back("detail_2_metallic_roughness");
    mReservedUniforms.push_back("detail_3_metallic_roughness");
    mReservedUniforms.push_back("detail_0_emissive");
    mReservedUniforms.push_back("detail_1_emissive");
    mReservedUniforms.push_back("detail_2_emissive");
    mReservedUniforms.push_back("detail_3_emissive");

    mReservedUniforms.push_back("baseColorFactors");
    mReservedUniforms.push_back("metallicFactors");
    mReservedUniforms.push_back("roughnessFactors");
    mReservedUniforms.push_back("emissiveColors");
    mReservedUniforms.push_back("minimum_alphas");

    mReservedUniforms.push_back("region_scale");
    llassert(mReservedUniforms.size() == LLShaderMgr::REGION_SCALE + 1);

    mReservedUniforms.push_back("gltf_minimum_alpha");
    mReservedUniforms.push_back("gltf_basecolor_transform");
    mReservedUniforms.push_back("gltf_emissive_color");
    mReservedUniforms.push_back("gltf_emissive_transform");
    mReservedUniforms.push_back("gltf_roughness_factor");
    mReservedUniforms.push_back("gltf_metallic_factor");
    mReservedUniforms.push_back("gltf_normal_transform");
    mReservedUniforms.push_back("gltf_mr_transform");
    mReservedUniforms.push_back("gltf_base_color_alpha");

    mReservedUniforms.push_back("mat_specular_color");
    mReservedUniforms.push_back("mat_env_intensity");
    mReservedUniforms.push_back("mat_minimum_alpha");
    mReservedUniforms.push_back("mat_emissive_brightness");

    mReservedUniforms.push_back("origin");
    mReservedUniforms.push_back("display_gamma");


    mReservedUniforms.push_back("blend_factor");
    mReservedUniforms.push_back("moisture_level");
    mReservedUniforms.push_back("droplet_radius");
    mReservedUniforms.push_back("ice_level");
    mReservedUniforms.push_back("rainbow_map");
    mReservedUniforms.push_back("halo_map");
    mReservedUniforms.push_back("moon_brightness");
    mReservedUniforms.push_back("cloud_variance");
    mReservedUniforms.push_back("reflection_probe_ambiance");
    mReservedUniforms.push_back("max_probe_lod");
    mReservedUniforms.push_back("probe_strength");

    mReservedUniforms.push_back("resScale");
    mReservedUniforms.push_back("direction");
    mReservedUniforms.push_back("znear");
    mReservedUniforms.push_back("zfar");
    mReservedUniforms.push_back("sourceIdx");
    mReservedUniforms.push_back("mipLevel");
    mReservedUniforms.push_back("roughness");
    mReservedUniforms.push_back("u_width");


    mReservedUniforms.push_back("sun_moon_glow_factor");
    mReservedUniforms.push_back("sun_up_factor");
    mReservedUniforms.push_back("moonlight_color");

    // Pathfinding Debug
    mReservedUniforms.push_back("tint");
    mReservedUniforms.push_back("ambiance");
    mReservedUniforms.push_back("alpha_scale");

    // Bump mapping
    mReservedUniforms.push_back("norm_scale");
    mReservedUniforms.push_back("stepX");
    mReservedUniforms.push_back("stepY");
    mReservedUniforms.push_back("bump_code");

    mReservedUniforms.push_back("delta");
    mReservedUniforms.push_back("dist_factor");
    mReservedUniforms.push_back("kern");
    mReservedUniforms.push_back("kern_scale");

    // Debug
    mReservedUniforms.push_back("tolerance");
    mReservedUniforms.push_back("dither_scale");
    mReservedUniforms.push_back("dither_scale_s");
    mReservedUniforms.push_back("dither_scale_t");

    // SMAA
    mReservedUniforms.push_back("edgesTex");
    mReservedUniforms.push_back("areaTex");
    mReservedUniforms.push_back("searchTex");
    mReservedUniforms.push_back("blendTex");
    mReservedUniforms.push_back("predicationTex");
    mReservedUniforms.push_back("SMAA_RT_METRICS");

    // CAS
    mReservedUniforms.push_back("cas_param_0");
    mReservedUniforms.push_back("cas_param_1");
    mReservedUniforms.push_back("out_screen_res");

    // Tonemapping + Exposure
    mReservedUniforms.push_back("dt");
    mReservedUniforms.push_back("noiseVec");
    mReservedUniforms.push_back("dynamic_exposure_params");
    mReservedUniforms.push_back("dynamic_exposure_params2");

    mReservedUniforms.push_back("exposure");
    mReservedUniforms.push_back("tonemap_type");
    mReservedUniforms.push_back("tonemap_mix");
    mReservedUniforms.push_back("tonemap_params");
    mReservedUniforms.push_back("hdri_split_screen");
    mReservedUniforms.push_back("meter_params");
    mReservedUniforms.push_back("meterMap");
    mReservedUniforms.push_back("exposureHistogram");

    // Stars/Aurora/Meteors
    mReservedUniforms.push_back("custom_alpha");
    mReservedUniforms.push_back("meteor_width_pixels");
    mReservedUniforms.push_back("aurora_intensity");
    mReservedUniforms.push_back("aurora_time");
    llassert(mReservedUniforms.size() == LLShaderMgr::AURORA_TIME + 1);

    // Alchemy Effects Stack
    mReservedUniforms.push_back("uFrameId");
    mReservedUniforms.push_back("uResolution");

    // Chromatic Aberration
    mReservedUniforms.push_back("uCAAmount");
    mReservedUniforms.push_back("uCAFalloff");
    mReservedUniforms.push_back("uCAAngleSinCos");
    mReservedUniforms.push_back("uCAOffsetR");
    mReservedUniforms.push_back("uCAOffsetB");
    mReservedUniforms.push_back("uCAAnisotropy");

    // Lens Flare
    mReservedUniforms.push_back("uLensFlareStrength");
    mReservedUniforms.push_back("uLensFlareSunPos");
    mReservedUniforms.push_back("uLensFlareSunVisibility");
    mReservedUniforms.push_back("uLensFlareStreakLength");
    mReservedUniforms.push_back("uLensFlareStreakFalloff");
    mReservedUniforms.push_back("uLensFlareStreakWidth");
    mReservedUniforms.push_back("uLensFlareStreakIntensity");
    mReservedUniforms.push_back("uLensFlareStreakTint");
    mReservedUniforms.push_back("uLensFlareChromaticSpread");
    mReservedUniforms.push_back("uLensFlareGlowRadius");
    mReservedUniforms.push_back("uLensFlareGlowFalloff");
    mReservedUniforms.push_back("uLensFlareGlow");
    mReservedUniforms.push_back("uLensFlareGhostCount");
    mReservedUniforms.push_back("uLensFlareGhostSpacing");
    mReservedUniforms.push_back("uLensFlareGhost");
    mReservedUniforms.push_back("uLensFlareHaloRadius");
    mReservedUniforms.push_back("uLensFlareHaloWidth");
    mReservedUniforms.push_back("uLensFlareHalo");
    mReservedUniforms.push_back("uLensFlareOcclusionRadius");
    mReservedUniforms.push_back("uLensFlareStarburst");
    mReservedUniforms.push_back("uLensFlareStarburstSpikes");
    mReservedUniforms.push_back("uLensFlareStarburstSharpness");
    mReservedUniforms.push_back("uLensFlareStarburstFalloff");
    mReservedUniforms.push_back("uLensFlareLightColor");
    mReservedUniforms.push_back("uLensFlareStateMap");
    mReservedUniforms.push_back("uLensFlareFadeTime");

    // Color Correction LUT
    mReservedUniforms.push_back("uColorGradeLut");
    mReservedUniforms.push_back("uColorGradeLutSize");
    mReservedUniforms.push_back("uColorGradeLutStrength");

    // Linear-space grading (pre-tonemap)
    mReservedUniforms.push_back("uWhiteBalanceGain");
    mReservedUniforms.push_back("uLift");
    mReservedUniforms.push_back("uInvGammaCC");
    mReservedUniforms.push_back("uGain");

    // Split toning
    mReservedUniforms.push_back("uShadowRatio");
    mReservedUniforms.push_back("uHighlightRatio");
    mReservedUniforms.push_back("uMidtoneRatio");
    mReservedUniforms.push_back("uMidtoneAmount");
    mReservedUniforms.push_back("uSplitToneShadowRamp");
    mReservedUniforms.push_back("uSplitToneHighlightRamp");
    mReservedUniforms.push_back("uToneAmount");

    // Display-space grading
    mReservedUniforms.push_back("uBWPScale");
    mReservedUniforms.push_back("uBWPBias");
    mReservedUniforms.push_back("uBCScale");
    mReservedUniforms.push_back("uBCBias");
    mReservedUniforms.push_back("uHighlightsScaled");
    mReservedUniforms.push_back("uShadowsScaled");
    mReservedUniforms.push_back("uSaturation");
    mReservedUniforms.push_back("uVibrance");
    mReservedUniforms.push_back("uHueShiftNorm");

    // Vignette
    mReservedUniforms.push_back("uVignetteAmount");
    mReservedUniforms.push_back("uVignetteRadius");
    mReservedUniforms.push_back("uVignetteSoft");
    mReservedUniforms.push_back("uVignetteShape");
    mReservedUniforms.push_back("uVignetteColor");
    mReservedUniforms.push_back("uVignetteMidColor");
    mReservedUniforms.push_back("uVignetteMidPoint");
    mReservedUniforms.push_back("uVignetteCenter");
    mReservedUniforms.push_back("uVignetteAspect");
    mReservedUniforms.push_back("uVignetteFeather");

    // CVD
    mReservedUniforms.push_back("uCompensateMode");
    mReservedUniforms.push_back("uCompensateAmount");

    // Film Grain
    mReservedUniforms.push_back("uGrainAmount");
    mReservedUniforms.push_back("uGrainStyle");
    mReservedUniforms.push_back("uGrainSize");
    mReservedUniforms.push_back("uGrainRange");
    mReservedUniforms.push_back("uGrainTint");
    mReservedUniforms.push_back("uGrainAnimate");

    // Dithering
    mReservedUniforms.push_back("uDitherAmount");
    mReservedUniforms.push_back("uDitherBits");
    mReservedUniforms.push_back("uDitherAnimate");

    // Previews
    mReservedUniforms.push_back("uPreviewMode");

    // Reference still
    mReservedUniforms.push_back("uReferenceStill");
    mReservedUniforms.push_back("uRefWipeMode");
    mReservedUniforms.push_back("uRefWipePos");

    // Geometric lens distortion
    mReservedUniforms.push_back("uLensDistortAmount");
    mReservedUniforms.push_back("uLensDistortK");
    mReservedUniforms.push_back("uLensDistortScale");
    mReservedUniforms.push_back("uLensDistortSqueeze");
    mReservedUniforms.push_back("uLensDistortCenter");
    mReservedUniforms.push_back("uLensDistortTangential");

    // Bokeh
    mReservedUniforms.push_back("uBokehHighlightThreshold");
    mReservedUniforms.push_back("uBokehHighlightGain");
    mReservedUniforms.push_back("uBokehHighlightClamp");
    mReservedUniforms.push_back("uBokehBlades");
    mReservedUniforms.push_back("uBokehApertureRotation");
    mReservedUniforms.push_back("uBokehApertureCurvature");
    mReservedUniforms.push_back("uBokehApertureConst");
    mReservedUniforms.push_back("uBokehAnamorphic");
    mReservedUniforms.push_back("uBokehCatEye");
    mReservedUniforms.push_back("uBokehFringeAmount");
    mReservedUniforms.push_back("uBokehFringeNearTint");
    mReservedUniforms.push_back("uBokehFringeFarTint");

    // Lens dirt
    mReservedUniforms.push_back("uLensDirtMap");
    mReservedUniforms.push_back("uLensDirtStrength");
    mReservedUniforms.push_back("uLensDirtBloomResponse");
    mReservedUniforms.push_back("uLensDirtFlareResponse");

    // Cross-screen filter
    mReservedUniforms.push_back("uCrossTexel");
    mReservedUniforms.push_back("uCrossDir");
    mReservedUniforms.push_back("uCrossLength");
    mReservedUniforms.push_back("uCrossFalloff");
    mReservedUniforms.push_back("uCrossChromatic");
    mReservedUniforms.push_back("uCrossPassScale");
    mReservedUniforms.push_back("uCrossStrength");

    mReservedUniforms.push_back("uBokehSpherical");
    mReservedUniforms.push_back("uBokehFieldStretch");
    mReservedUniforms.push_back("uBokehFieldFalloff");
    mReservedUniforms.push_back("uBokehComaAsymmetry");

    mReservedUniforms.push_back("crossFilterMap");

    mReservedUniforms.push_back("uCutoutPremultiplied");

    // Text Shadow
    mReservedUniforms.push_back("textShadowMode");

    // End Alchemy Effects Stack

    mReservedUniforms.push_back("shPartial");

    mReservedUniforms.push_back("waterFar");
    mReservedUniforms.push_back("waterRim");

    mReservedUniforms.push_back("eyeToSky");
    mReservedUniforms.push_back("skyBehindMap");

    mReservedUniforms.push_back("haze_extinction");
    mReservedUniforms.push_back("blue_weight");
    mReservedUniforms.push_back("haze_weight");
    mReservedUniforms.push_back("atmos_sunlit");
    mReservedUniforms.push_back("atmos_ambient");
    mReservedUniforms.push_back("sky_sunlight");
    mReservedUniforms.push_back("sky_light");
    mReservedUniforms.push_back("sky_ambient");
    mReservedUniforms.push_back("atmos_sunlit_linear");
    mReservedUniforms.push_back("atmos_amblit");
    mReservedUniforms.push_back("haze_additive");
    mReservedUniforms.push_back("haze_additive_glow");
    mReservedUniforms.push_back("sky_light_atten");
    mReservedUniforms.push_back("cloud_sunlight");
    mReservedUniforms.push_back("sky_ambient_below");

    mReservedUniforms.push_back("moonQuadCenter");
    mReservedUniforms.push_back("moonQuadU");
    mReservedUniforms.push_back("moonQuadV");

    // Selection outlines, hover glow and wireframe (ALSelectionOutline)
    mReservedUniforms.push_back("selection_id");
    mReservedUniforms.push_back("selection_priority");
    mReservedUniforms.push_back("selection_show_hidden");
    mReservedUniforms.push_back("selection_outlined");
    mReservedUniforms.push_back("selection_scene_depth");
    mReservedUniforms.push_back("outline_width");
    mReservedUniforms.push_back("outline_inner_width");
    mReservedUniforms.push_back("outline_glow_radius");
    mReservedUniforms.push_back("outline_glow_brightness");
    mReservedUniforms.push_back("wireframe_pass");
    mReservedUniforms.push_back("wireframe_width");
    mReservedUniforms.push_back("selectionIdMap");
    mReservedUniforms.push_back("selectionJumpMap");
    mReservedUniforms.push_back("selectionTileMap");
    mReservedUniforms.push_back("selectionPalette");
    mReservedUniforms.push_back("selectionGlowReachMap");
    mReservedUniforms.push_back("selectionGlowRowMap");
    mReservedUniforms.push_back("selectionBackdrop");
    mReservedUniforms.push_back("selection_contrast");
    mReservedUniforms.push_back("selection_keep_cut");
    mReservedUniforms.push_back("selectionTileDepthMap");
    mReservedUniforms.push_back("outline_rim_width");

    mReservedUniforms.push_back("waterCellMap");
    mReservedUniforms.push_back("waterCellSquare");
    mReservedUniforms.push_back("waterDefaultHeight");
    mReservedUniforms.push_back("waterRings");
    mReservedUniforms.push_back("waterSweepPhase");
    mReservedUniforms.push_back("waterLayout");
    mReservedUniforms.push_back("waterGlintRoughness");
    mReservedUniforms.push_back("waterLatticeFrac");
    mReservedUniforms.push_back("waterLatticeBase");
    mReservedUniforms.push_back("waterMapMean");
    mReservedUniforms.push_back("waterPatchRect");
    mReservedUniforms.push_back("waterTessDensity");
    mReservedUniforms.push_back("waterMaxDisplacement");
    mReservedUniforms.push_back("waterCullPlanes");
    mReservedUniforms.push_back("waterHeightMap");
    mReservedUniforms.push_back("waterHeightMap2");
    mReservedUniforms.push_back("waterWaveAmp");
    mReservedUniforms.push_back("waterWaveReach");
    mReservedUniforms.push_back("waterWaveEye");
    mReservedUniforms.push_back("waterRaise");
    mReservedUniforms.push_back("waterGroundFade");

    // The light map's ambient occlusion blur in one dispatch
    mReservedUniforms.push_back("blur_light_apron");
    mReservedUniforms.push_back("blurLightTarget");

    // The waves' patches
    mReservedUniforms.push_back("waterPatchOrder");

    // Settings the programs read as uniforms
    mReservedUniforms.push_back("terrainTriplanarBlend");
    mReservedUniforms.push_back("smaaPredicationThreshold");
    mReservedUniforms.push_back("smaaPredicationScale");
    mReservedUniforms.push_back("smaaPredicationStrength");

    // The enum and this list are parallel, and an entry added or removed on one side only
    // shifts every later uniform index for every shader in the viewer -- silently, since a
    // wrong index still resolves to some other real uniform. Fatal, like the duplicate check
    // below: there is no partial recovery from it.
    if (mReservedUniforms.size() != END_RESERVED_UNIFORMS)
    {
        LL_ERRS() << "Reserved uniform table has " << mReservedUniforms.size()
                  << " entries but eGLSLReservedUniforms declares " << (U32)END_RESERVED_UNIFORMS
                  << " -- the enum and the table are out of sync" << LL_ENDL;
    }

    std::set<std::string> dupe_check;

    for (U32 i = 0; i < mReservedUniforms.size(); ++i)
    {
        if (dupe_check.find(mReservedUniforms[i]) != dupe_check.end())
        {
            LL_ERRS() << "Duplicate reserved uniform name found: " << mReservedUniforms[i] << LL_ENDL;
        }
        dupe_check.insert(mReservedUniforms[i]);

        // Reserved arrays use their bare names so every producer refers to the
        // same metadata entry, independently of element selection at a binding site.
        if (mReservedUniforms[i].find('[') != std::string::npos)
        {
            LL_ERRS() << "Reserved uniform '" << mReservedUniforms[i] << "' carries a subscript; "
                      << "array uniforms are declared here by their bare name" << LL_ENDL;
        }
    }

}
