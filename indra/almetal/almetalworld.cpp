/**
 * @file almetalworld.cpp
 * @brief Native Metal world G-buffer bootstrap pass.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "almetalworld.h"

#include "../newview/SMAAAreaTex.h"
#include "../newview/SMAASearchTex.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace
{
    void setError(std::string* destination, const std::string& message)
    {
        if (destination)
        {
            *destination = message;
        }
    }

    float floatFromHalf(std::uint16_t value)
    {
        const std::uint32_t sign =
            static_cast<std::uint32_t>(value & 0x8000u) << 16;
        std::uint32_t exponent = (value >> 10) & 0x1fu;
        std::uint32_t mantissa = value & 0x03ffu;
        std::uint32_t bits = 0;
        if (exponent == 0)
        {
            if (mantissa == 0)
            {
                bits = sign;
            }
            else
            {
                std::int32_t unbiased = -14;
                while ((mantissa & 0x0400u) == 0)
                {
                    mantissa <<= 1;
                    --unbiased;
                }
                mantissa &= 0x03ffu;
                bits = sign |
                    (static_cast<std::uint32_t>(unbiased + 127) << 23) |
                    (mantissa << 13);
            }
        }
        else if (exponent == 0x1fu)
        {
            bits = sign | 0x7f800000u | (mantissa << 13);
        }
        else
        {
            bits = sign | ((exponent + 112u) << 23) | (mantissa << 13);
        }
        float result = 0.f;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }

    bool validBlendFactor(ALMetalBlendFactor factor)
    {
        switch (factor)
        {
            case ALMetalBlendFactor::zero:
            case ALMetalBlendFactor::one:
            case ALMetalBlendFactor::source_color:
            case ALMetalBlendFactor::one_minus_source_color:
            case ALMetalBlendFactor::source_alpha:
            case ALMetalBlendFactor::one_minus_source_alpha:
            case ALMetalBlendFactor::destination_color:
            case ALMetalBlendFactor::one_minus_destination_color:
            case ALMetalBlendFactor::destination_alpha:
            case ALMetalBlendFactor::one_minus_destination_alpha:
                return true;
        }
        return false;
    }

    struct alignas(16) WorldSimpleConstants
    {
        float modelview_projection[16];
        float modelview[16] = {
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
        float normal_matrix[3][4];
        float rigged_normal_matrix[3][4];
        float texture_matrix[16];
        float material[4]{};
        std::uint32_t depth_convention[4]{};
        float avatar_wind[4]{};
        float avatar_sin_wave[4]{};
        float avatar_gravity[4]{};
        float avatar_cloth_depth[4]{};
        float mirror_clip_plane[4] = {0.f, 0.f, 0.f, 1.f};
    };

    struct alignas(16) WorldHighlightConstants
    {
        float modelview_projection[16];
        float texture_matrix[16];
        float color[4];
        std::uint32_t config[4];
    };

    struct alignas(16) WorldSpotShadowConstants
    {
        float modelview_projection[16];
        float texture_matrix[16];
        float pbr_base_color_transforms[8][2][4];
        float alpha_cutoffs[32];
        std::uint32_t config[4]{};
        float avatar_wind[4]{};
        float avatar_sin_wave[4]{};
        float avatar_gravity[4]{};
        float avatar_cloth_depth[4]{};
        // x = per-mesh alpha used by avatarAlphaShadowF.
        float avatar_draw[4] = {1.f, 0.f, 0.f, 0.f};
    };
    static_assert(sizeof(WorldSpotShadowConstants) == 608);

    struct alignas(16) WorldDisplayPostConstants
    {
        // static exposure, adapted exposure, tonemap mix, tonemap type
        float post[4];
        float tonemap_params[4];
        // non-HDR dither, bloom strength, halation, cross-filter strength
        float display[4];
        float halation_tint[4];
        // cutout-premultiplied, HDR/legacy/bypass flags, animate dither, frame id
        std::uint32_t config[4];
        // squared/scaled amount, reciprocal falloff, anisotropy, legacy gamma
        float chromatic[4];
        // angle sine/cosine, red radial/tangential offset
        float chromatic_angle_offset_r[4];
        // blue radial/tangential offset
        float chromatic_offset_b[4];
        // sun UV, master strength, atmosphere-relative HDR scale.
        float flare_sun_strength[4];
        // length, falloff, width, intensity.
        float flare_streak[4];
        // linear tint RGB, vertical chromatic spread.
        float flare_streak_tint[4];
        // glow intensity/radius/falloff, ghost intensity.
        float flare_glow_ghost[4];
        // ghost count/spacing, halo intensity/radius.
        float flare_ghost_halo[4];
        // halo width, starburst intensity/spikes/sharpness.
        float flare_halo_starburst[4];
        // starburst falloff, linear light tint RGB.
        float flare_star_light[4];
        // strength, bloom response, flare response, reserved.
        float lens_dirt[4];
        float grade_white_balance[4];
        float grade_lift[4];
        float grade_inverse_gamma[4];
        float grade_gain[4];
        float grade_shadow_ratio[4];
        float grade_midtone_ratio[4];
        float grade_highlight_ratio[4];
        // midtone/split amount, shadow ramp scale/bias.
        float grade_split0[4];
        // highlight ramp scale/bias, black/white scale/bias.
        float grade_split1[4];
        // brightness/contrast scale/bias, highlight/shadow recovery.
        float grade_display0[4];
        // saturation, vibrance, normalized hue shift, enabled.
        float grade_display1[4];
        // LUT edge, flip-green, swap-blue/green, strength.
        float grade_lut[4];
        // Curve half-texel scale/bias, amount, reserved.
        float grade_curve[4];
    };
    static_assert(sizeof(WorldDisplayPostConstants) == 464);

    struct alignas(16) WorldPresentationConstants
    {
        float lens_primary[4];
        float lens_squeeze_center[4];
        float lens_tangent_reference[4];
        float vignette_primary[4];
        float vignette_outer_middle[4];
        float vignette_middle_feather[4];
        float vignette_center_aspect[4];
        float effects[4];
        float grain_tint_dither[4];
        std::uint32_t config[4];
        std::uint32_t noise[4];
    };
    static_assert(sizeof(WorldPresentationConstants) == 176);

    struct alignas(16) WorldLensFlareStateConstants
    {
        // source width/height, clamped dt, fade time.
        float screen_time[4];
        // sun UV, edge visibility, probe radius in screen heights.
        float sun_visibility_radius[4];
        // x = reverse-Z.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldLensFlareStateConstants) == 48);

    struct alignas(16) WorldLensDirtConstants
    {
        // plate width/height, seed, grime.
        float screen_seed_grime[4];
        // mote scale, smudge, scratch count, toe.
        float detail_toe[4];
        // gain and reserved values.
        float gain[4];
    };
    static_assert(sizeof(WorldLensDirtConstants) == 48);

    struct alignas(16) WorldDepthOfFieldConstants
    {
        float inverse_projection[16]{};
        // focal distance, blur constant, tan pixel angle, magnification.
        float focus[4]{};
        // max CoF, blur resolution scale, highlight threshold/gain.
        float limits[4]{};
        // highlight clamp, aperture rotation/curvature, cat-eye amount.
        float shape0[4]{};
        // fringe amount, spherical aberration, field stretch/falloff.
        float shape1[4]{};
        // coma, half sector, full sector, polygon apothem.
        float shape2[4]{};
        // x/y are the area-preserving anamorphic axes.
        float anamorphic[4] = {1.f, 1.f, 0.f, 0.f};
        float fringe_near[4] = {1.f, 0.85f, 1.f, 0.f};
        float fringe_far[4] = {0.85f, 1.f, 0.9f, 0.f};
        // reverse Z, near blur, shaped gather, aperture blades.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldDepthOfFieldConstants) == 208);

    struct alignas(16) WorldBloomExtractConstants
    {
        // threshold, knee, firefly clamp, alpha glow boost
        float params[4]{};
        // Kept ABI-identical to the shared preview/world shader entry. The
        // non-halation world target discards the computed alpha channel.
        float halation_weights[4]{};
    };
    static_assert(sizeof(WorldBloomExtractConstants) == 32);

    struct alignas(16) WorldBloomFilterConstants
    {
        // inverse source width/height, scatter or first-downsample flag
        float params[4]{};
    };
    static_assert(sizeof(WorldBloomFilterConstants) == 16);

    struct alignas(16) WorldCrossFilterConstants
    {
        // target texel x/y, authored length, chromatic dispersion
        float sampling[4]{};
        // arm direction x/y, pass scale, exponential falloff base
        float filtering[4]{};
    };
    static_assert(sizeof(WorldCrossFilterConstants) == 32);

    struct alignas(16) WorldCASConstants
    {
        // x = the negative CAS peak produced by CasSetup.
        float params[4]{};
    };
    static_assert(sizeof(WorldCASConstants) == 16);

    struct alignas(16) WorldFXAAConstants
    {
        // x = quality index: 0/1/2/3 -> preset 12/23/28/39.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldFXAAConstants) == 16);

    struct alignas(16) WorldSMAAEdgeConstants
    {
        // x = predication threshold, y = scale, z = strength.
        float predication[4]{};
        // x = quality index, y = predication enabled.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldSMAAEdgeConstants) == 32);

    struct alignas(16) WorldSMAAWeightConstants
    {
        // x = quality index.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldSMAAWeightConstants) == 16);

    struct alignas(16) WorldExposureMeterConstants
    {
        float patch[4]{};
    };
    static_assert(sizeof(WorldExposureMeterConstants) == 16);

    struct alignas(16) WorldExposureConstants
    {
        float exposure[4]{};
        float adaptation[4]{};
        float meter[4]{};
    };
    static_assert(sizeof(WorldExposureConstants) == 48);

    struct alignas(16) WorldTerrainSpotShadowConstants
    {
        float modelview_projection[16]{};
        // x = metres per terrain grid.
        float surface[4]{};
        // x = reverse-Z convention, y = smooth terrain surface.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldTerrainSpotShadowConstants) == 96);

    struct alignas(16) WorldWaterExclusionConstants
    {
        float modelview_projection[16]{};
        float modelview[16]{};
        float eye_to_agent[16]{};
        float cell_square[4]{};
        float camera_default_height[4]{};
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldWaterExclusionConstants) == 240);

    struct alignas(16) WorldWaterHazeConstants
    {
        float modelview_projection[16]{};
        float inverse_projection[16]{};
        float cell_square[4]{};
        float default_height[4]{};
        float rings[4]{};
        float water_plane[4]{};
        float fog_color[4]{};
        float fog[4]{};
        std::int32_t patch_rect[4]{};
        // x = reverse-Z, y = above-water field, z = field part.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldWaterHazeConstants) == 256);

    struct alignas(16) WorldAtmosphericHazeConstants
    {
        float inverse_projection[16]{};
        float parameters[4]{};
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldAtmosphericHazeConstants) == 96);
    static_assert(sizeof(ALMetalWorldAtmosphericHaze) == 80);

    struct alignas(16) WorldWaterSurfaceConstants
    {
        float modelview[16];
        float normal_matrix[3][4];
        float environment_matrix[3][4];
        float eye_position[4];
        float big_phase[4];
        float little_phase[4];
        float sweep_phase[4];
        float water_far[4];
        float map_mean[4];
        float lattice_fraction[4];
        std::int32_t lattice_base[4];
        float normal_scale_glint[4];
        float optics[4];
        float rim[4];
        float light_direction[4];
        float parameters[4];
        float wave_eye_density[4];
        float wave_reach_amplitude[4];
        std::int32_t patch_rect[4];
        std::int32_t patch_order[4];
        float screen[4];
        std::uint32_t config[4];
        // x = legacy sky-cube environment instead of reflection probes,
        // y = authored hero-mirror half-space clipping.
        std::uint32_t environment_config[4];
        float mirror_clip_plane[4];
    };
    static_assert(sizeof(WorldWaterSurfaceConstants) == 496);

    struct alignas(16) WorldWaterOcclusionConstants
    {
        float modelview_projection[16]{};
        float cell_square[4]{};
        // xy = source depth size, z = maximum wave displacement.
        float screen_displacement[4]{};
        std::int32_t patch_rect[4]{};
        std::int32_t patch_order[4]{};
        // x = patch count, y = pyramid levels, z = reverse-Z.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldWaterOcclusionConstants) == 144);

    struct alignas(16) WorldShinyConstants
    {
        float modelview_projection[16];
        float modelview[16];
        float normal_matrix[3][4];
        float rigged_normal_matrix[3][4];
        float texture_matrix[16];
        std::uint32_t config[4]{};
        float avatar_wind[4]{};
        float avatar_sin_wave[4]{};
        float avatar_gravity[4]{};
        float avatar_cloth_depth[4]{};
        float mirror_clip_plane[4] = {0.f, 0.f, 0.f, 1.f};
    };
    static_assert(sizeof(WorldShinyConstants) == 384);
    static_assert(sizeof(ALMetalWorldAlphaEnvironment) == 320);
    static_assert(sizeof(ALMetalWorldLegacyAlphaLight) == 64);
    static_assert(sizeof(ALMetalWorldLegacyAlphaLighting) == 496);
    static_assert(sizeof(ALMetalWorldHeroProbeConstants) == 112);
    static_assert(sizeof(ALMetalWorldScreenSpaceReflectionConstants) == 256);
    static_assert(sizeof(ALMetalWorldDirectionalShadowConstants) == 320);

    struct alignas(16) WorldHDRISkyConstants
    {
        float modelview_projection[16]{};
        float camera_exposure[4]{};
        float environment_rotation[3][4]{};
        float screen[4]{};
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldHDRISkyConstants) == 160);

    struct alignas(16) WorldWindLightSkyConstants
    {
        float modelview_projection[16]{};
        ALMetalWorldWindLightSkyConstants sky;
        // xy = destination dimensions, z = deferred sky_hdr_scale.
        float transfer[4]{};
        // x = reverse Z, y = emissive/HDR G-buffer.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(ALMetalWorldWindLightSkyConstants) == 192);
    static_assert(sizeof(WorldWindLightSkyConstants) == 288);

    struct alignas(16) WorldStarConstants
    {
        float modelview_projection[16]{};
        float screen_alpha_time[4]{};
        float moon_direction_brightness[4]{};
        float moon_quad_center[4]{};
        float moon_quad_u[4]{};
        float moon_quad_v[4]{};
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldStarConstants) == 160);

    struct alignas(16) WorldHeavenlyBodyConstants
    {
        float modelview_projection[16]{};
        // x = sun texture blend or moon brightness, y = moon direction z.
        float parameters[4]{};
        // x = reverse Z, y = emissive/HDR G-buffer, z = moon.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldHeavenlyBodyConstants) == 96);

    struct alignas(16) WorldMeteorConstants
    {
        float modelview_projection[16]{};
        // xy = target dimensions, z = half-width in pixels.
        float screen_width[4]{};
        // x = reverse Z, y = emissive/HDR G-buffer.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldMeteorConstants) == 96);

    struct alignas(16) WorldAuroraConstants
    {
        float modelview_projection[16]{};
        // xyz = camera in dome-local space, w = intensity.
        float camera_intensity[4]{};
        // x = wrapped time, yz = target dimensions.
        float time_screen[4]{};
        // x = reverse Z, y = emissive/HDR G-buffer.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldAuroraConstants) == 112);

    struct alignas(16) WorldCloudConstants
    {
        float modelview_projection[16]{};
        ALMetalWorldCloudConstants cloud;
        float screen[4]{};
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(ALMetalWorldCloudConstants) == 240);
    static_assert(sizeof(WorldCloudConstants) == 336);

    struct alignas(16) WorldMaterialSlotConstants
    {
        float specular_color[4];
        float properties[4];
    };

    struct alignas(16) WorldImpostorConstants
    {
        float modelview_projection[16];
        float modelview[16];
        float center[4];
        float left[4];
        float up[4];
        float normal_rotation[3][4];
        std::uint32_t depth_convention[4]{};
        float mirror_clip_plane[4] = {0.f, 0.f, 0.f, 1.f};
    };

    struct alignas(16) WorldMaterialConstants
    {
        WorldMaterialSlotConstants
            slots[ALMetalWorldSimpleBatch::max_material_slots]{};
        std::uint32_t config[4]{};
    };

    struct alignas(16) WorldPBRSlotConstants
    {
        float texture_transform[4][8];
        float emissive_color[4];
        // x = roughness, y = metallic, z = alpha cutoff,
        // w = streamed base-colour alpha factor.
        float properties[4];
    };

    struct alignas(16) WorldPBRConstants
    {
        WorldPBRSlotConstants
            slots[ALMetalWorldSimpleBatch::max_material_slots]{};
        // x = populated slot count, y = alpha mask, z = HDR normal target.
        std::uint32_t config[4]{};
        // Supplies the eye-space position used by PBR mirror clipping and
        // forward alpha lighting.
        float modelview[16]{};
    };

    struct alignas(16) WorldLocalLightingConstants
    {
        float inverse_projection[16]{};
        // x = legacy specular exponent, y = classic-mode output scale.
        float surface[4] = {128.f, 1.f, 0.f, 0.f};
        // x = light count, y = reverse-Z convention.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldLocalLightingConstants) == 96);
    static_assert(sizeof(ALMetalWorldLocalLight) == 32);

    struct alignas(16) WorldProjectorLightingConstants
    {
        float inverse_projection[16]{};
        float projector_matrix[16]{};
        float position_radius[4]{};
        float color_falloff[4]{};
        float projector_plane[4]{};
        float projector_normal[4]{};
        float projector_origin[4]{};
        // x = cookie LOD range, y = shadow fade,
        // z = legacy specular exponent, w = classic output scale.
        float sampling[4]{};
        // x = reverse Z, y = shadow channel + 1,
        // z = high-precision geometric-normal packing.
        std::uint32_t config[4]{};
    };
    static_assert(sizeof(WorldProjectorLightingConstants) == 240);
    static_assert(sizeof(ALMetalWorldLightMapConstants) == 208);
    static_assert(sizeof(ALMetalWorldBaseLightingConstants) == 336);

    struct alignas(16) WorldTerrainConstants
    {
        float modelview_projection[16];
        float modelview[16];
        float normal_matrix[3][4];
        float mirror_clip_plane[4] = {0.f, 0.f, 0.f, 1.f};
        float surface[4]{};
        float detail[4]{};
        std::uint32_t config[4]{};
        std::uint32_t pbr_config[4]{};
        float hex_fraction[8][4]{};
        std::int32_t hex_base[8][4]{};
        struct alignas(16) PBRSlot
        {
            float uv_transform[4]{};
            float uv_offset[4]{};
            float normal_axes[4]{};
            float base_color_factor[4]{};
            float emissive_color[4]{};
            // x = roughness, y = metallic, z = alpha cutoff.
            float properties[4]{};
        } pbr_slots[4];
    };

    bool finiteValues(const float* values, std::size_t count)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            if (!std::isfinite(values[index]))
            {
                return false;
            }
        }
        return true;
    }

    bool validWorldColorGrade(const ALMetalWorldColorGrade& grade)
    {
        if (!grade.enabled)
        {
            return true;
        }

        const auto valuesInRange = [](const float* values,
                                      std::size_t count,
                                      float minimum, float maximum)
        {
            for (std::size_t index = 0; index < count; ++index)
            {
                if (!std::isfinite(values[index]) ||
                    values[index] < minimum ||
                    values[index] > maximum)
                {
                    return false;
                }
            }
            return true;
        };

        return finiteValues(grade.white_balance_gain, 3) &&
               valuesInRange(grade.lift, 3, -0.5f, 0.5f) &&
               valuesInRange(grade.inverse_gamma, 3, 2.f / 3.f, 2.f) &&
               valuesInRange(grade.gain, 3, 0.5f, 1.5f) &&
               valuesInRange(grade.shadow_ratio, 3, 0.f, 10000.f) &&
               valuesInRange(grade.midtone_ratio, 3, 0.f, 10000.f) &&
               valuesInRange(grade.highlight_ratio, 3, 0.f, 10000.f) &&
               valuesInRange(grade.shadow_ramp, 2, -10000.f, 10000.f) &&
               valuesInRange(grade.highlight_ramp, 2, -10000.f, 10000.f) &&
               std::isfinite(grade.midtone_amount) &&
               grade.midtone_amount >= 0.f && grade.midtone_amount <= 1.f &&
               std::isfinite(grade.split_amount) &&
               grade.split_amount >= 0.f && grade.split_amount <= 1.f &&
               grade.shadow_ramp[0] > 0.f &&
               grade.highlight_ramp[0] > 0.f &&
               std::isfinite(grade.black_white_scale) &&
               grade.black_white_scale >= 1.f &&
               grade.black_white_scale <= 10000.f &&
               std::isfinite(grade.black_white_bias) &&
               grade.black_white_bias >= -5000.f &&
               grade.black_white_bias <= 0.f &&
               std::isfinite(grade.brightness_contrast_scale) &&
               grade.brightness_contrast_scale >= 0.f &&
               grade.brightness_contrast_scale <= 2.f &&
               std::isfinite(grade.brightness_contrast_bias) &&
               grade.brightness_contrast_bias >= -1.5f &&
               grade.brightness_contrast_bias <= 1.5f &&
               std::isfinite(grade.highlights_scaled) &&
               grade.highlights_scaled >= -0.3f &&
               grade.highlights_scaled <= 0.3f &&
               std::isfinite(grade.shadows_scaled) &&
               grade.shadows_scaled >= -0.3f &&
               grade.shadows_scaled <= 0.3f &&
               std::isfinite(grade.saturation) &&
               grade.saturation >= 0.f && grade.saturation <= 2.f &&
               std::isfinite(grade.vibrance) &&
               grade.vibrance >= -1.f && grade.vibrance <= 1.f &&
               std::isfinite(grade.hue_shift) &&
               grade.hue_shift >= -0.5f && grade.hue_shift <= 0.5f &&
               std::isfinite(grade.color_lut_strength) &&
               grade.color_lut_strength >= 0.f &&
               grade.color_lut_strength <= 1.f &&
               std::isfinite(grade.tone_curve_scale) &&
               std::isfinite(grade.tone_curve_bias) &&
               std::isfinite(grade.tone_curve_amount) &&
               grade.tone_curve_amount >= 0.f &&
               grade.tone_curve_amount <= 1.f &&
               ((grade.color_lut_strength == 0.f &&
                 !grade.color_lut_texture && grade.color_lut_size == 0) ||
                (grade.color_lut_strength > 0.f &&
                 grade.color_lut_texture && grade.color_lut_size >= 2 &&
                 grade.color_lut_texture->descriptor().type ==
                     ALMetalTextureType::texture_2d_array &&
                 grade.color_lut_texture->descriptor().width ==
                     grade.color_lut_size &&
                 grade.color_lut_texture->descriptor().height ==
                     grade.color_lut_size &&
                 grade.color_lut_texture->descriptor().array_length ==
                     grade.color_lut_size &&
                 grade.color_lut_texture->descriptor().sample_count == 1 &&
                 (grade.color_lut_texture->descriptor().format ==
                      ALMetalPixelFormat::rgba8_unorm ||
                  grade.color_lut_texture->descriptor().format ==
                      ALMetalPixelFormat::rgba16_unorm) &&
                 alMetalHasUsage(
                     grade.color_lut_texture->descriptor().usage,
                     ALMetalTextureUsage::shader_read))) &&
               ((grade.tone_curve_amount == 0.f &&
                 !grade.tone_curve_texture) ||
                (grade.tone_curve_amount > 0.f &&
                 grade.tone_curve_texture &&
                 grade.tone_curve_texture->descriptor().type ==
                     ALMetalTextureType::texture_2d &&
                 grade.tone_curve_texture->descriptor().width >= 2 &&
                 grade.tone_curve_texture->descriptor().height == 1 &&
                 grade.tone_curve_texture->descriptor().sample_count == 1 &&
                 grade.tone_curve_texture->descriptor().format ==
                     ALMetalPixelFormat::rgba16_unorm &&
                 std::abs(
                     grade.tone_curve_scale -
                     (1.f - 1.f / static_cast<float>(
                         grade.tone_curve_texture->descriptor().width))) <=
                     4.f * std::numeric_limits<float>::epsilon() &&
                 std::abs(
                     grade.tone_curve_bias -
                     (0.5f / static_cast<float>(
                         grade.tone_curve_texture->descriptor().width))) <=
                     4.f * std::numeric_limits<float>::epsilon() &&
                 alMetalHasUsage(
                     grade.tone_curve_texture->descriptor().usage,
                     ALMetalTextureUsage::shader_read)));
    }

    bool rangeFits(std::size_t offset, std::size_t count,
                   std::size_t stride, std::size_t length)
    {
        return count <= (std::numeric_limits<std::size_t>::max() - offset) /
                            stride &&
               offset + count * stride <= length;
    }

    bool validMaterialAlphaMode(std::uint32_t mode)
    {
        return mode == 0 || mode == 2 || mode == 3;
    }

    bool materialHasNormalMap(const ALMetalWorldSimpleBatch& batch)
    {
        return batch.material_slot_count != 0 &&
               batch.material_slots[0].normal_texture != nullptr;
    }

    bool materialHasSpecularMap(const ALMetalWorldSimpleBatch& batch)
    {
        return batch.material_slot_count != 0 &&
               batch.material_slots[0].specular_texture != nullptr;
    }

    bool isPBRBatch(const ALMetalWorldSimpleBatch& batch)
    {
        return batch.pbr_slot_count != 0;
    }

    bool sampleableTexture(const ALMetalTexture* texture)
    {
        return texture &&
               texture->descriptor().type == ALMetalTextureType::texture_2d &&
               alMetalHasUsage(texture->descriptor().usage,
                               ALMetalTextureUsage::shader_read);
    }

    bool validReflectionProbes(
        const ALMetalWorldReflectionProbes* probes)
    {
        static_assert(sizeof(ALMetalWorldReflectionProbeConstants) == 49216);
        if (!probes || !probes->radiance_texture ||
            !finiteValues(probes->constants.config, 4) ||
            !finiteValues(&probes->constants.env_matrix[0][0], 12))
        {
            return false;
        }
        const float count_value = probes->constants.config[0];
        if (count_value < 1.f ||
            count_value > static_cast<float>(
                ALMetalWorldReflectionProbeConstants::max_probe_count) ||
            std::floor(count_value) != count_value)
        {
            return false;
        }
        const std::size_t probe_count =
            static_cast<std::size_t>(count_value);

        const ALMetalTextureDescriptor& descriptor =
            probes->radiance_texture->descriptor();
        if (descriptor.type != ALMetalTextureType::texture_cube_array ||
            (descriptor.format != ALMetalPixelFormat::rgba16_float &&
             descriptor.format != ALMetalPixelFormat::rgba8_unorm) ||
            descriptor.width == 0 || descriptor.width != descriptor.height ||
            descriptor.array_length == 0 ||
            descriptor.mipmap_level_count == 0 ||
            descriptor.sample_count != 1 ||
            !alMetalHasUsage(descriptor.usage,
                             ALMetalTextureUsage::shader_read) ||
            probes->constants.config[1] < 0.f ||
            probes->constants.config[1] >
                static_cast<float>(descriptor.mipmap_level_count - 1))
        {
            return false;
        }

        for (std::size_t index = 0; index < probe_count; ++index)
        {
            const std::int32_t cube =
                probes->constants.ref_index[index][0];
            if (cube < 0 ||
                static_cast<std::uint32_t>(cube) >=
                    descriptor.array_length ||
                !finiteValues(probes->constants.ref_box[index], 16) ||
                !finiteValues(probes->constants.ref_sphere[index], 4) ||
                !finiteValues(probes->constants.ref_params[index], 4))
            {
                return false;
            }
        }
        return true;
    }

    bool validPBRReflectionProbes(
        const ALMetalWorldReflectionProbes* probes)
    {
        if (!validReflectionProbes(probes) ||
            !probes->irradiance_texture)
        {
            return false;
        }
        const ALMetalTextureDescriptor& descriptor =
            probes->irradiance_texture->descriptor();
        return descriptor.type == ALMetalTextureType::texture_2d &&
               descriptor.format == ALMetalPixelFormat::rgba32_uint &&
               descriptor.width >= 4 &&
               descriptor.height >=
                   probes->radiance_texture->descriptor().array_length &&
               descriptor.sample_count == 1 &&
               alMetalHasUsage(descriptor.usage,
                               ALMetalTextureUsage::shader_read);
    }

    bool validLegacyEnvironment(const ALMetalTexture* texture)
    {
        if (!texture)
        {
            return false;
        }
        const ALMetalTextureDescriptor& descriptor = texture->descriptor();
        return descriptor.type == ALMetalTextureType::texture_cube &&
               descriptor.format == ALMetalPixelFormat::rgba8_unorm &&
               descriptor.width != 0 &&
               descriptor.width == descriptor.height &&
               descriptor.mipmap_level_count != 0 &&
               descriptor.sample_count == 1 &&
               alMetalHasUsage(descriptor.usage,
                               ALMetalTextureUsage::shader_read);
    }

    bool validLegacyAlphaLighting(
        const ALMetalWorldLegacyAlphaLighting* lighting)
    {
        if (!lighting ||
            !finiteValues(lighting->sun_direction, 4) ||
            !finiteValues(lighting->sun_color, 4) ||
            !finiteValues(lighting->parameters, 4) ||
            lighting->parameters[0] <= 0.f ||
            lighting->parameters[1] < 0.f ||
            lighting->parameters[1] > 1.f ||
            lighting->parameters[2] < 0.f ||
            lighting->parameters[2] > 1.f ||
            (lighting->parameters[3] != 0.f &&
             lighting->parameters[3] != 1.f))
        {
            return false;
        }
        const float sun_length_squared =
            lighting->sun_direction[0] * lighting->sun_direction[0] +
            lighting->sun_direction[1] * lighting->sun_direction[1] +
            lighting->sun_direction[2] * lighting->sun_direction[2];
        if (sun_length_squared <= 0.f)
        {
            return false;
        }
        for (const ALMetalWorldLegacyAlphaLight& light :
             lighting->local_lights)
        {
            if (!finiteValues(light.position, 4) ||
                !finiteValues(light.direction, 4) ||
                !finiteValues(light.diffuse, 4) ||
                !finiteValues(light.parameters, 4) ||
                light.parameters[0] < 0.f ||
                (light.parameters[0] > 0.f &&
                 light.parameters[1] <= -1.f) ||
                light.parameters[2] < 0.f ||
                light.parameters[2] > 1.f)
            {
                return false;
            }
        }
        return true;
    }

    bool validHeroProbe(const ALMetalWorldHeroProbe* hero)
    {
        if (!hero || !hero->radiance_texture ||
            !finiteValues(hero->constants.hero_box, 16) ||
            !finiteValues(hero->constants.hero_sphere, 4) ||
            !finiteValues(hero->constants.clip_plane, 4) ||
            hero->constants.config[0] > 1 ||
            hero->constants.config[1] != 1)
        {
            return false;
        }

        const ALMetalTextureDescriptor& descriptor =
            hero->radiance_texture->descriptor();
        const std::uint32_t hero_max_lod = hero->constants.config[2];
        const std::uint32_t required_levels =
            static_cast<std::uint32_t>(
                std::ceil(static_cast<float>(hero_max_lod) * 0.25f)) + 1;
        return descriptor.type == ALMetalTextureType::texture_cube &&
               (descriptor.format == ALMetalPixelFormat::rg11b10_float ||
                descriptor.format == ALMetalPixelFormat::rgba8_unorm) &&
               descriptor.width != 0 &&
               descriptor.width == descriptor.height &&
               descriptor.array_length == 1 &&
               descriptor.mipmap_level_count == required_levels &&
               descriptor.sample_count == 1 &&
               alMetalHasUsage(
                   descriptor.usage, ALMetalTextureUsage::shader_read);
    }

    bool validScreenSpaceReflections(
        const ALMetalWorldScreenSpaceReflections* reflections)
    {
        if (!reflections || !reflections->scene_color ||
            !reflections->scene_depth ||
            !finiteValues(reflections->constants.projection, 16) ||
            !finiteValues(reflections->constants.inverse_projection, 16) ||
            !finiteValues(
                reflections->constants.inverse_modelview_delta, 16) ||
            !finiteValues(reflections->constants.screen, 4) ||
            !finiteValues(reflections->constants.trace, 4) ||
            !finiteValues(reflections->constants.sampling, 4) ||
            reflections->constants.screen[0] <= 0.f ||
            reflections->constants.screen[1] <= 0.f ||
            reflections->constants.screen[2] <= 0.f ||
            reflections->constants.trace[0] < 0.f ||
            reflections->constants.sampling[0] < 1.f ||
            reflections->constants.sampling[0] > 128.f ||
            reflections->constants.config[0] > 1 ||
            reflections->constants.config[1] != 1)
        {
            return false;
        }

        const ALMetalTextureDescriptor& color =
            reflections->scene_color->descriptor();
        const ALMetalTextureDescriptor& depth =
            reflections->scene_depth->descriptor();
        const bool color_format =
            color.format == ALMetalPixelFormat::rg11b10_float ||
            color.format == ALMetalPixelFormat::rgba8_unorm;
        return color.type == ALMetalTextureType::texture_2d &&
               depth.type == ALMetalTextureType::texture_2d &&
               color_format &&
               depth.format == ALMetalPixelFormat::r32_float &&
               color.width != 0 && color.height != 0 &&
               color.width == depth.width &&
               color.height == depth.height &&
               color.array_length == 1 && depth.array_length == 1 &&
               color.mipmap_level_count == 1 &&
               depth.mipmap_level_count == 1 &&
               color.sample_count == 1 && depth.sample_count == 1 &&
               alMetalHasUsage(
                   color.usage, ALMetalTextureUsage::shader_read) &&
               alMetalHasUsage(
                   depth.usage, ALMetalTextureUsage::shader_read);
    }

    bool validDirectionalShadows(
        const ALMetalWorldDirectionalShadows* shadows)
    {
        if (!shadows || !shadows->depth_texture)
        {
            return false;
        }
        const ALMetalTextureDescriptor& descriptor =
            shadows->depth_texture->descriptor();
        const ALMetalWorldDirectionalShadowConstants& constants =
            shadows->constants;
        const std::uint32_t filter_config = constants.config[3];
        const std::uint32_t filter_quality = filter_config &
            ALMetalWorldDirectionalShadowConstants::filter_quality_mask;
        return descriptor.type == ALMetalTextureType::texture_2d_array &&
               descriptor.format == ALMetalPixelFormat::r32_float &&
               descriptor.array_length == 4 &&
               descriptor.mipmap_level_count == 1 &&
               alMetalHasUsage(
                   descriptor.usage, ALMetalTextureUsage::shader_read) &&
               finiteValues(&constants.shadow_matrix[0][0], 4 * 16) &&
               finiteValues(constants.shadow_clip, 4) &&
               finiteValues(constants.resolution, 4) &&
               finiteValues(constants.parameters, 4) &&
               constants.resolution[0] == descriptor.width &&
               constants.resolution[1] == descriptor.height &&
               constants.resolution[2] > 0.f &&
               constants.resolution[3] > 0.f &&
               constants.parameters[3] > 0.f &&
               constants.config[0] >= 1 && constants.config[0] <= 4 &&
               (constants.config[1] & ~0xfu) == 0 &&
               constants.config[2] <= 1 &&
               (filter_config &
                ~ALMetalWorldDirectionalShadowConstants::filter_config_mask) ==
                   0 &&
               (filter_quality < 3 || constants.parameters[2] > 0.f);
    }

    bool validProjectorShadows(
        const ALMetalWorldDirectionalShadows* shadows)
    {
        if (!shadows || !shadows->depth_texture)
        {
            return false;
        }
        const ALMetalTextureDescriptor& descriptor =
            shadows->depth_texture->descriptor();
        const ALMetalWorldDirectionalShadowConstants& constants =
            shadows->constants;
        return descriptor.type == ALMetalTextureType::texture_2d_array &&
               descriptor.format == ALMetalPixelFormat::r32_float &&
               descriptor.width != 0 && descriptor.height != 0 &&
               descriptor.array_length == 2 &&
               descriptor.mipmap_level_count == 1 &&
               descriptor.sample_count == 1 &&
               alMetalHasUsage(
                   descriptor.usage, ALMetalTextureUsage::shader_read) &&
               finiteValues(&constants.shadow_matrix[0][0], 2 * 16) &&
               finiteValues(constants.shadow_clip, 4) &&
               finiteValues(constants.resolution, 4) &&
               finiteValues(constants.parameters, 4) &&
               constants.resolution[0] == descriptor.width &&
               constants.resolution[1] == descriptor.height &&
               constants.resolution[2] > 0.f &&
               constants.resolution[3] > 0.f &&
               constants.parameters[3] > 0.f &&
               constants.config[0] == 2 &&
               (constants.config[1] & ~0x3u) == 0 &&
               constants.config[2] <= 1 && constants.config[3] <= 3 &&
               (constants.config[3] < 3 || constants.parameters[2] > 0.f);
    }

}

struct ALMetalWorldGBufferRenderer::Impl
{
    using NormalPipelines =
        std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>;
    using RiggedPipelines = std::array<NormalPipelines, 2>;
    using MaskPipelines = std::array<RiggedPipelines, 2>;
    using BumpPipelines = std::array<MaskPipelines, 2>;
    using MaterialRiggedPipelines = std::array<NormalPipelines, 2>;
    using AvatarVariantPipelines = std::array<NormalPipelines, 3>;
    using TerrainPartitionPipelines = std::array<NormalPipelines, 2>;
    using OpaqueForwardRiggedPipelines =
        std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>;
    using OpaqueForwardMaskPipelines =
        std::array<OpaqueForwardRiggedPipelines, 2>;

    enum class PreWaterAlphaFamily : std::uint8_t
    {
        fullbright,
        simple,
        material,
        pbr,
    };

    struct PreWaterAlphaPipelineEntry
    {
        PreWaterAlphaFamily family = PreWaterAlphaFamily::fullbright;
        bool hdr = false;
        bool rigged = false;
        bool shadows = false;
        bool coverage = false;
        ALMetalBlendFactor source = ALMetalBlendFactor::source_alpha;
        ALMetalBlendFactor destination =
            ALMetalBlendFactor::one_minus_source_alpha;
        std::unique_ptr<ALMetalRenderPipeline> pipeline;
    };

    struct PostWaterDepthPipelines
    {
        std::unique_ptr<ALMetalRenderPipeline> fullbright;
        std::unique_ptr<ALMetalRenderPipeline> material;
        std::unique_ptr<ALMetalRenderPipeline> pbr;
    };

    const ALMetalDevice* device = nullptr;
    std::unique_ptr<ALMetalShaderLibrary> library;
    std::unique_ptr<ALMetalCommandBuffer> command_batch;
    bool command_batch_valid = true;
    std::string command_batch_failure;
    bool command_batch_generates_brdf_lut = false;

    ALMetalCommandBuffer* acquireCommandBuffer(
        const std::string& label,
        std::unique_ptr<ALMetalCommandBuffer>& owned,
        std::string* error)
    {
        if (command_batch)
        {
            if (!command_batch_valid)
            {
                setError(error, command_batch_failure.empty()
                    ? "Metal world command batch was invalidated"
                    : command_batch_failure);
                return nullptr;
            }
            return command_batch.get();
        }
        owned = device ? device->makeCommandBuffer(label, error) : nullptr;
        return owned.get();
    }

    bool finishCommandBuffer(
        std::unique_ptr<ALMetalCommandBuffer>& owned,
        bool encoded,
        std::string* error)
    {
        if (owned)
        {
            return encoded && owned->commit(error);
        }
        if (command_batch && !encoded)
        {
            command_batch_valid = false;
            if (command_batch_failure.empty())
            {
                command_batch_failure = error && !error->empty()
                    ? *error
                    : "Metal world command batch encoding failed";
            }
        }
        return encoded;
    }
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        spot_shadow_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        spot_shadow_coverage_pipelines;
    std::array<std::array<std::unique_ptr<ALMetalRenderPipeline>, 3>, 2>
        spot_shadow_avatar_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        spot_shadow_terrain_pipelines;
    std::array<BumpPipelines, 2> pipelines;
    std::array<MaterialRiggedPipelines, 2> material_pipelines;
    std::array<MaterialRiggedPipelines, 2> pbr_pipelines;
    std::array<MaskPipelines, 2> fullbright_pipelines;
    std::array<RiggedPipelines, 2> shiny_pipelines;
    std::array<RiggedPipelines, 2> emboss_pipelines;
    std::array<MaterialRiggedPipelines, 2> flat_alpha_pipelines;
    std::array<MaterialRiggedPipelines, 2> pbr_alpha_pipelines;
    std::array<MaterialRiggedPipelines, 2> legacy_alpha_pipelines;
    std::array<AvatarVariantPipelines, 2> avatar_pipelines;
    std::array<NormalPipelines, 2> impostor_pipelines;
    std::array<NormalPipelines, 2> coverage_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        cutout_background_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        cutout_coverage_pipelines;
    std::array<TerrainPartitionPipelines, 2> terrain_pipelines;
    std::array<TerrainPartitionPipelines, 2> pbr_terrain_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        local_light_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        local_light_accumulation_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        projector_light_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        lighting_composition_pipelines;
    std::array<std::array<std::array<
        std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>, 2>, 2>, 2>
        display_post_pipelines;
    std::unique_ptr<ALMetalRenderPipeline> presentation_post_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> lens_flare_state_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> lens_dirt_pipeline;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        bloom_extract_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        bloom_downsample_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        bloom_upsample_pipelines;
    std::unique_ptr<ALMetalRenderPipeline> cross_filter_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> cross_filter_add_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> cas_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> fxaa_pipeline;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        smaa_edge_pipelines;
    std::unique_ptr<ALMetalRenderPipeline> smaa_weight_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> smaa_neighborhood_pipeline;
    // format (R11G11B10F/RGBA16F) x shaped x near-blur.
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 8>
        depth_of_field_gather_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        depth_of_field_combine_pipelines;
    std::unique_ptr<ALMetalTexture> smaa_area_texture;
    std::unique_ptr<ALMetalTexture> smaa_search_texture;
    std::unique_ptr<ALMetalTexture> identity_color_lut;
    std::unique_ptr<ALMetalTexture> identity_tone_curve;
    std::unique_ptr<ALMetalTexture> identity_lens_flare_state;
    std::unique_ptr<ALMetalTexture> identity_lens_dirt;
    std::unique_ptr<ALMetalRenderPipeline> display_sample_pipeline;
    std::unique_ptr<ALMetalComputePipeline> exposure_meter_pipeline;
    std::array<std::unique_ptr<ALMetalComputePipeline>, 2>
        exposure_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        ssr_history_pipelines;
    std::array<OpaqueForwardMaskPipelines, 2>
        opaque_forward_fullbright_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        highlight_pipelines;
    std::array<OpaqueForwardRiggedPipelines, 2>
        opaque_forward_shiny_pipelines;
    std::array<OpaqueForwardRiggedPipelines, 2>
        opaque_forward_emboss_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        water_exclusion_pipelines;
    std::array<std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>, 2>
        water_haze_pipelines;
    std::array<std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>, 2>
        atmospheric_haze_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        underwater_flat_water_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        above_water_flat_water_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        underwater_wave_water_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        above_water_wave_water_pipelines;
    std::array<std::array<OpaqueForwardRiggedPipelines, 2>, 2>
        pre_water_fullbright_pipelines;
    std::array<std::array<
        std::array<OpaqueForwardRiggedPipelines, 2>, 2>, 2>
        pre_water_simple_pipelines;
    // HDR x coverage x shadows x rigid/skinned/cloth system-avatar mesh x
    // color-write state. The latter keeps the middle pass of three-pass
    // hair/skirt depth-only without changing its alpha-cutout shader.
    std::array<std::array<std::array<std::array<
        std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>, 3>, 2>, 2>, 2>
        avatar_alpha_pipelines;
    std::array<std::array<OpaqueForwardRiggedPipelines, 2>, 2>
        pre_water_material_pipelines;
    std::array<std::array<OpaqueForwardRiggedPipelines, 2>, 2>
        pre_water_pbr_pipelines;
    std::vector<PreWaterAlphaPipelineEntry>
        custom_pre_water_alpha_pipelines;
    std::array<PostWaterDepthPipelines, 2>
        post_water_depth_pipelines;
    std::array<OpaqueForwardRiggedPipelines, 2>
        legacy_glow_pipelines;
    std::array<OpaqueForwardRiggedPipelines, 2>
        pbr_glow_pipelines;
    std::array<OpaqueForwardRiggedPipelines, 2>
        post_water_pbr_glow_pipelines;
    std::unique_ptr<ALMetalRenderPipeline> light_map_pipeline;
    std::unique_ptr<ALMetalRenderPipeline> light_map_blur_pipeline;
    std::array<NormalPipelines, 2> hdri_sky_pipelines;
    std::array<NormalPipelines, 2> windlight_sky_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        windlight_sky_behind_pipelines;
    std::array<NormalPipelines, 2> windlight_star_pipelines;
    std::array<NormalPipelines, 2> windlight_sun_pipelines;
    std::array<NormalPipelines, 2> windlight_moon_pipelines;
    std::array<NormalPipelines, 2> windlight_meteor_pipelines;
    std::array<NormalPipelines, 2> windlight_aurora_pipelines;
    std::array<NormalPipelines, 2> windlight_cloud_pipelines;
    std::array<std::unique_ptr<ALMetalRenderPipeline>, 2>
        base_light_pipelines;
    std::unique_ptr<ALMetalComputePipeline> brdf_lut_pipeline;
    std::unique_ptr<ALMetalComputePipeline> hero_probe_filter_pipeline;
    std::unique_ptr<ALMetalComputePipeline> probe_gaussian_pipeline;
    std::unique_ptr<ALMetalComputePipeline> probe_downsample_pipeline;
    std::unique_ptr<ALMetalComputePipeline> probe_sh_rows_pipeline;
    std::unique_ptr<ALMetalComputePipeline> probe_sh_reduce_pipeline;
    std::unique_ptr<ALMetalTexture> probe_gaussian_horizontal;
    std::unique_ptr<ALMetalTexture> probe_gaussian_vertical;
    std::unique_ptr<ALMetalTexture> probe_downsample;
    std::unique_ptr<ALMetalTexture> probe_sh_partial;
    std::unique_ptr<ALMetalComputePipeline>
        water_depth_pyramid_first_pipeline;
    std::unique_ptr<ALMetalComputePipeline>
        water_depth_pyramid_last_pipeline;
    std::unique_ptr<ALMetalComputePipeline>
        water_wave_occlusion_pipeline;
    std::array<std::unique_ptr<ALMetalTexture>, 8>
        water_depth_pyramid;
    std::unique_ptr<ALMetalDepthStencilState> forward_depth;
    std::unique_ptr<ALMetalDepthStencilState> reverse_depth;
    std::unique_ptr<ALMetalDepthStencilState> forward_spot_shadow_depth;
    std::unique_ptr<ALMetalDepthStencilState> reverse_spot_shadow_depth;
    std::unique_ptr<ALMetalDepthStencilState> forward_readonly_depth;
    std::unique_ptr<ALMetalDepthStencilState> reverse_readonly_depth;
    std::unique_ptr<ALMetalDepthStencilState> forward_coverage_depth;
    std::unique_ptr<ALMetalDepthStencilState> reverse_coverage_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        forward_atmospheric_haze_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        reverse_atmospheric_haze_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        forward_water_exclusion_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        reverse_water_exclusion_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        forward_water_haze_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        reverse_water_haze_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        underwater_water_haze_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        forward_underwater_surface_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        reverse_underwater_surface_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        forward_underwater_excluded_depth;
    std::unique_ptr<ALMetalDepthStencilState>
        reverse_underwater_excluded_depth;
    std::unique_ptr<ALMetalSamplerState> diffuse_sampler;
    std::unique_ptr<ALMetalSamplerState> probe_sampler;
    std::unique_ptr<ALMetalSamplerState> point_clamp_sampler;
    std::unique_ptr<ALMetalSamplerState> point_repeat_sampler;
    std::unique_ptr<ALMetalSamplerState> point_mirror_sampler;
    std::unique_ptr<ALMetalSamplerState> linear_clamp_sampler;
    std::unique_ptr<ALMetalSamplerState> linear_mirror_sampler;
    std::unique_ptr<ALMetalSamplerState> ramp_sampler;
    std::unique_ptr<ALMetalSamplerState> hdri_sampler;
    std::unique_ptr<ALMetalTexture> brdf_lut;
    bool brdf_lut_ready = false;
    std::unique_ptr<ALMetalTexture> unshadowed_depth;
    ALMetalWorldDirectionalShadowConstants unshadowed_constants{};
    std::unique_ptr<ALMetalTexture> inactive_hero_probe;
    ALMetalWorldHeroProbeConstants inactive_hero_constants{};
    std::unique_ptr<ALMetalTexture> inactive_probe_radiance;
    std::unique_ptr<ALMetalTexture> inactive_probe_irradiance;
    std::unique_ptr<ALMetalTexture> inactive_ssr_color;
    std::unique_ptr<ALMetalTexture> inactive_ssr_depth;
    ALMetalWorldScreenSpaceReflectionConstants inactive_ssr_constants{};

    ALMetalRenderPipeline* customPreWaterAlphaPipeline(
        PreWaterAlphaFamily family, bool hdr, bool rigged, bool shadows,
        bool coverage,
        ALMetalBlendFactor source, ALMetalBlendFactor destination,
        std::string* error)
    {
        for (PreWaterAlphaPipelineEntry& entry :
             custom_pre_water_alpha_pipelines)
        {
            if (entry.family == family && entry.hdr == hdr &&
                entry.rigged == rigged && entry.shadows == shadows &&
                entry.coverage == coverage &&
                entry.source == source &&
                entry.destination == destination)
            {
                return entry.pipeline.get();
            }
        }

        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = library.get();
        descriptor.color_attachment_count = 1;
        descriptor.colors[0].format = hdr
            ? ALMetalPixelFormat::rgba16_float
            : ALMetalPixelFormat::rgba8_unorm;
        descriptor.colors[0].blending_enabled = true;
        descriptor.colors[0].source_rgb = source;
        descriptor.colors[0].destination_rgb = destination;
        descriptor.colors[0].source_alpha = coverage
            ? ALMetalBlendFactor::one : ALMetalBlendFactor::zero;
        descriptor.colors[0].destination_alpha =
            ALMetalBlendFactor::one_minus_source_alpha;
        descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        switch (family)
        {
            case PreWaterAlphaFamily::fullbright:
                descriptor.vertex_function = rigged
                    ? "almetal_world_shiny_skinned_vertex"
                    : "almetal_world_shiny_vertex";
                descriptor.fragment_function =
                    "almetal_world_pre_water_fullbright_fragment";
                break;
            case PreWaterAlphaFamily::simple:
                descriptor.vertex_function = rigged
                    ? "almetal_world_shiny_skinned_vertex"
                    : "almetal_world_shiny_vertex";
                descriptor.fragment_function = shadows
                    ? "almetal_world_pre_water_simple_shadow_fragment"
                    : "almetal_world_pre_water_simple_fragment";
                break;
            case PreWaterAlphaFamily::material:
                descriptor.vertex_function = rigged
                    ? "almetal_world_legacy_alpha_skinned_vertex"
                    : "almetal_world_legacy_alpha_vertex";
                descriptor.fragment_function =
                    "almetal_world_pre_water_material_fragment";
                break;
            case PreWaterAlphaFamily::pbr:
                descriptor.vertex_function = rigged
                    ? "almetal_world_pbr_skinned_vertex"
                    : "almetal_world_pbr_vertex";
                descriptor.fragment_function =
                    "almetal_world_pre_water_pbr_fragment";
                break;
        }
        descriptor.label =
            "Alchemy world custom-blend pre-water alpha pipeline";
        auto pipeline = ALMetalRenderPipeline::create(
            *device, descriptor, error);
        if (!pipeline)
        {
            return nullptr;
        }
        ALMetalRenderPipeline* result = pipeline.get();
        custom_pre_water_alpha_pipelines.push_back({
            family, hdr, rigged, shadows, coverage, source, destination,
            std::move(pipeline)});
        return result;
    }
};

ALMetalWorldGBufferRenderer::ALMetalWorldGBufferRenderer(
    std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalWorldGBufferRenderer::~ALMetalWorldGBufferRenderer() = default;

bool ALMetalWorldGBufferRenderer::beginCommandBatch(
    const std::string& label,
    std::string* error)
{
    if (!mImpl || !mImpl->device)
    {
        setError(error, "Metal world renderer is unavailable");
        return false;
    }
    if (mImpl->command_batch)
    {
        setError(error, "Metal world command batch is already active");
        return false;
    }
    mImpl->command_batch = mImpl->device->makeCommandBuffer(label, error);
    mImpl->command_batch_valid = static_cast<bool>(mImpl->command_batch);
    mImpl->command_batch_failure.clear();
    mImpl->command_batch_generates_brdf_lut = false;
    return static_cast<bool>(mImpl->command_batch);
}

bool ALMetalWorldGBufferRenderer::commitCommandBatch(std::string* error)
{
    if (!mImpl || !mImpl->command_batch)
    {
        setError(error, "Metal world command batch is not active");
        return false;
    }
    auto command = std::move(mImpl->command_batch);
    const bool valid = mImpl->command_batch_valid;
    const std::string failure = mImpl->command_batch_failure;
    const bool generates_brdf_lut =
        mImpl->command_batch_generates_brdf_lut;
    mImpl->command_batch_valid = true;
    mImpl->command_batch_failure.clear();
    mImpl->command_batch_generates_brdf_lut = false;
    if (!valid)
    {
        setError(error, failure.empty()
            ? "Metal world command batch was invalidated"
            : failure);
        return false;
    }
    const bool committed = command->commit(error);
    if (committed && generates_brdf_lut)
    {
        mImpl->brdf_lut_ready = true;
    }
    return committed;
}

void ALMetalWorldGBufferRenderer::cancelCommandBatch()
{
    if (mImpl)
    {
        mImpl->command_batch.reset();
        mImpl->command_batch_valid = true;
        mImpl->command_batch_failure.clear();
        mImpl->command_batch_generates_brdf_lut = false;
    }
}

bool ALMetalWorldGBufferRenderer::commandBatchActive() const
{
    return mImpl && static_cast<bool>(mImpl->command_batch);
}

std::unique_ptr<ALMetalWorldGBufferRenderer>
ALMetalWorldGBufferRenderer::create(const ALMetalDevice& device,
                                    std::string* error)
{
    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->library = ALMetalShaderLibrary::loadBuiltin(device, error);
    if (!impl->library ||
        !impl->library->hasFunction("almetal_resample_vertex") ||
        !impl->library->hasFunction("almetal_resample_fragment") ||
        !impl->library->hasFunction("almetal_world_spot_shadow_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_spot_shadow_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_spot_shadow_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_spot_shadow_coverage_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_spot_shadow_coverage_skinned_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_spot_shadow_coverage_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_terrain_spot_shadow_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_avatar_spot_shadow_rigid_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_avatar_spot_shadow_skinned_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_avatar_spot_shadow_cloth_vertex") ||
        !impl->library->hasFunction("almetal_world_simple_vertex") ||
        !impl->library->hasFunction("almetal_world_simple_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_fullbright_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_fullbright_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_bump_vertex") ||
        !impl->library->hasFunction("almetal_world_bump_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_material_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_material_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_pbr_vertex") ||
        !impl->library->hasFunction("almetal_world_pbr_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_avatar_rigid_vertex") ||
        !impl->library->hasFunction("almetal_world_avatar_skinned_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_avatar_cloth_skinned_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_avatar_alpha_rigid_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_avatar_alpha_skinned_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_avatar_alpha_cloth_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_impostor_vertex") ||
        !impl->library->hasFunction("almetal_world_coverage_vertex") ||
        !impl->library->hasFunction("almetal_world_terrain_vertex") ||
        !impl->library->hasFunction("almetal_world_simple_fragment") ||
        !impl->library->hasFunction("almetal_world_alpha_mask_fragment") ||
        !impl->library->hasFunction("almetal_world_bump_fragment") ||
        !impl->library->hasFunction("almetal_world_material_fragment") ||
        !impl->library->hasFunction("almetal_world_pbr_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_fullbright_fragment") ||
        !impl->library->hasFunction("almetal_world_shiny_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_shiny_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_shiny_fragment") ||
        !impl->library->hasFunction("almetal_world_emboss_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_emboss_skinned_vertex") ||
        !impl->library->hasFunction("almetal_world_emboss_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_water_exclusion_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_water_exclusion_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_water_haze_field_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_water_haze_fullscreen_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_water_haze_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_atmospheric_haze_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_atmospheric_haze_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_flat_water_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_wave_water_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_underwater_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_pre_water_fullbright_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_pre_water_simple_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_pre_water_simple_shadow_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_pre_water_material_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_pre_water_pbr_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_legacy_glow_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_legacy_glow_skinned_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_legacy_glow_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_pbr_glow_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_pbr_glow_skinned_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_pbr_glow_fragment") ||
        !impl->library->hasFunction("almetal_world_flat_alpha_fragment") ||
        !impl->library->hasFunction("almetal_world_pbr_alpha_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_legacy_alpha_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_legacy_alpha_skinned_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_legacy_alpha_fragment") ||
        !impl->library->hasFunction("almetal_world_impostor_fragment") ||
        !impl->library->hasFunction("almetal_world_coverage_fragment") ||
        !impl->library->hasFunction("almetal_world_terrain_fragment") ||
        !impl->library->hasFunction("almetal_world_pbr_terrain_fragment") ||
        !impl->library->hasFunction("almetal_world_local_light_vertex") ||
        !impl->library->hasFunction("almetal_world_local_light_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_projector_light_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_lighting_composition_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_lens_flare_state_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_lens_dirt_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_presentation_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_display_post_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_display_post_exposure_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_display_post_bloom_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_display_post_bloom_exposure_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_bloom_extract_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_bloom_downsample_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_bloom_upsample_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_cross_filter_fragment") ||
        !impl->library->hasFunction("almetal_world_cas_fragment") ||
        !impl->library->hasFunction("almetal_world_fxaa_fragment") ||
        !impl->library->hasFunction("almetal_world_smaa_edge_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_smaa_edge_predicated_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_smaa_weight_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_smaa_neighborhood_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_dof_gather_far_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_dof_gather_near_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_dof_gather_far_shaped_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_dof_gather_near_shaped_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_dof_combine_far_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_dof_combine_near_fragment") ||
        !impl->library->hasFunction("almetal_world_exposure_meter") ||
        !impl->library->hasFunction("almetal_world_exposure_initial") ||
        !impl->library->hasFunction("almetal_world_exposure_history") ||
        !impl->library->hasFunction("almetal_world_light_map_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_light_map_blur_fragment") ||
        !impl->library->hasFunction("almetal_world_hdri_sky_vertex") ||
        !impl->library->hasFunction("almetal_world_hdri_sky_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_windlight_sky_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_windlight_sky_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_windlight_sky_behind_fragment") ||
        !impl->library->hasFunction("almetal_world_star_vertex") ||
        !impl->library->hasFunction("almetal_world_star_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_heavenly_body_vertex") ||
        !impl->library->hasFunction(
            "almetal_world_heavenly_body_fragment") ||
        !impl->library->hasFunction("almetal_world_meteor_vertex") ||
        !impl->library->hasFunction("almetal_world_meteor_fragment") ||
        !impl->library->hasFunction("almetal_world_aurora_vertex") ||
        !impl->library->hasFunction("almetal_world_aurora_fragment") ||
        !impl->library->hasFunction("almetal_world_cloud_vertex") ||
        !impl->library->hasFunction("almetal_world_cloud_fragment") ||
        !impl->library->hasFunction("almetal_world_base_light_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_ssr_history_fragment") ||
        !impl->library->hasFunction(
            "almetal_world_hero_probe_filter") ||
        !impl->library->hasFunction(
            "almetal_world_probe_gaussian") ||
        !impl->library->hasFunction(
            "almetal_world_probe_downsample") ||
        !impl->library->hasFunction(
            "almetal_world_probe_sh_rows") ||
        !impl->library->hasFunction(
            "almetal_world_probe_sh_reduce") ||
        !impl->library->hasFunction(
            "almetal_world_depth_pyramid_first") ||
        !impl->library->hasFunction(
            "almetal_world_depth_pyramid_last") ||
        !impl->library->hasFunction(
            "almetal_world_water_wave_occlusion") ||
        !impl->library->hasFunction("almetal_pbr_preview_brdf_lut"))
    {
        if (impl->library)
        {
            setError(error,
                     "Built-in Metal library is missing world G-buffer functions");
        }
        return nullptr;
    }

    for (std::size_t rigged = 0; rigged < 2; ++rigged)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function = rigged != 0
            ? "almetal_world_spot_shadow_skinned_vertex"
            : "almetal_world_spot_shadow_vertex";
        descriptor.fragment_function =
            "almetal_world_spot_shadow_fragment";
        descriptor.color_attachment_count = 1;
        descriptor.colors[0].format = ALMetalPixelFormat::r32_float;
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        descriptor.label = rigged != 0
            ? "Alchemy rigged spot-shadow depth pipeline"
            : "Alchemy static spot-shadow depth pipeline";
        impl->spot_shadow_pipelines[rigged] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->spot_shadow_pipelines[rigged])
        {
            return nullptr;
        }

        descriptor.vertex_function = rigged != 0
            ? "almetal_world_spot_shadow_coverage_skinned_vertex"
            : "almetal_world_spot_shadow_coverage_vertex";
        descriptor.fragment_function =
            "almetal_world_spot_shadow_coverage_fragment";
        descriptor.label = rigged != 0
            ? "Alchemy rigged covered spot-shadow depth pipeline"
            : "Alchemy static covered spot-shadow depth pipeline";
        impl->spot_shadow_coverage_pipelines[rigged] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->spot_shadow_coverage_pipelines[rigged])
        {
            return nullptr;
        }
    }

    constexpr std::array<const char*, 3> avatar_spot_vertices{{
        "almetal_world_avatar_spot_shadow_rigid_vertex",
        "almetal_world_avatar_spot_shadow_skinned_vertex",
        "almetal_world_avatar_spot_shadow_cloth_vertex",
    }};
    for (std::size_t covered = 0; covered < 2; ++covered)
    {
        for (std::size_t variant = 0;
             variant < avatar_spot_vertices.size(); ++variant)
        {
            ALMetalRenderPipelineDescriptor descriptor;
            descriptor.library = impl->library.get();
            descriptor.vertex_function = avatar_spot_vertices[variant];
            descriptor.fragment_function = covered
                ? "almetal_world_spot_shadow_coverage_fragment"
                : "almetal_world_spot_shadow_fragment";
            descriptor.color_attachment_count = 1;
            descriptor.colors[0].format = ALMetalPixelFormat::r32_float;
            descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float;
            descriptor.label = covered
                ? "Alchemy covered avatar spot-shadow depth pipeline"
                : "Alchemy opaque avatar spot-shadow depth pipeline";
            impl->spot_shadow_avatar_pipelines[covered][variant] =
                ALMetalRenderPipeline::create(
                    device, descriptor, error);
            if (!impl->spot_shadow_avatar_pipelines[covered][variant])
            {
                return nullptr;
            }
        }
    }

    for (std::size_t pinned = 0; pinned < 2; ++pinned)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function =
            "almetal_world_terrain_spot_shadow_vertex";
        descriptor.fragment_function =
            "almetal_world_spot_shadow_fragment";
        descriptor.color_attachment_count = 1;
        descriptor.colors[0].format = ALMetalPixelFormat::r32_float;
        descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
        descriptor.tessellation.enabled = true;
        descriptor.tessellation.partition_mode = pinned
            ? ALMetalTessellationPartitionMode::integer
            : ALMetalTessellationPartitionMode::fractional_odd;
        descriptor.tessellation.maximum_factor = 64;
        descriptor.tessellation.factor_step_function =
            ALMetalTessellationFactorStepFunction::per_patch;
        descriptor.tessellation.output_winding =
            ALMetalTessellationOutputWinding::counter_clockwise;
        descriptor.label = pinned
            ? "Alchemy pinned terrain spot-shadow depth pipeline"
            : "Alchemy terrain spot-shadow depth pipeline";
        impl->spot_shadow_terrain_pipelines[pinned] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->spot_shadow_terrain_pipelines[pinned])
        {
            return nullptr;
        }
    }

    ALMetalRenderPipelineDescriptor light_map_descriptor;
    light_map_descriptor.library = impl->library.get();
    light_map_descriptor.vertex_function =
        "almetal_world_local_light_vertex";
    light_map_descriptor.fragment_function =
        "almetal_world_light_map_fragment";
    light_map_descriptor.colors[0].format = ALMetalPixelFormat::rg8_unorm;
    light_map_descriptor.label = "Alchemy world shadow/SSAO light-map pipeline";
    impl->light_map_pipeline = ALMetalRenderPipeline::create(
        device, light_map_descriptor, error);
    if (!impl->light_map_pipeline)
    {
        return nullptr;
    }
    light_map_descriptor.fragment_function =
        "almetal_world_light_map_blur_fragment";
    light_map_descriptor.label = "Alchemy world SSAO bilateral-blur pipeline";
    impl->light_map_blur_pipeline = ALMetalRenderPipeline::create(
        device, light_map_descriptor, error);
    if (!impl->light_map_blur_pipeline)
    {
        return nullptr;
    }

    ALMetalRenderPipelineDescriptor display_sample_descriptor;
    display_sample_descriptor.library = impl->library.get();
    display_sample_descriptor.vertex_function = "almetal_resample_vertex";
    display_sample_descriptor.fragment_function = "almetal_resample_fragment";
    display_sample_descriptor.colors[0].format =
        ALMetalPixelFormat::rgba8_unorm;
    display_sample_descriptor.label =
        "Alchemy world display-post point-sample pipeline";
    impl->display_sample_pipeline = ALMetalRenderPipeline::create(
        device, display_sample_descriptor, error);
    if (!impl->display_sample_pipeline)
    {
        return nullptr;
    }

    const auto make_bloom_pipeline = [&](const char* fragment,
                                         const char* label,
                                         bool additive,
                                         ALMetalPixelFormat format)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function = "almetal_resample_vertex";
        descriptor.fragment_function = fragment;
        descriptor.colors[0].format = format;
        descriptor.colors[0].blending_enabled = additive;
        if (additive)
        {
            descriptor.colors[0].source_rgb = ALMetalBlendFactor::one;
            descriptor.colors[0].destination_rgb = ALMetalBlendFactor::one;
            descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
            descriptor.colors[0].destination_alpha =
                ALMetalBlendFactor::one;
        }
        descriptor.label = label;
        return ALMetalRenderPipeline::create(device, descriptor, error);
    };
    constexpr std::array<ALMetalPixelFormat, 2> bloom_formats{{
        ALMetalPixelFormat::rg11b10_float,
        ALMetalPixelFormat::rgba16_float,
    }};
    for (std::size_t halation = 0; halation < bloom_formats.size();
         ++halation)
    {
        impl->bloom_extract_pipelines[halation] = make_bloom_pipeline(
            "almetal_world_bloom_extract_fragment",
            halation ? "Alchemy world halation extract pipeline"
                      : "Alchemy world bloom extract pipeline",
            false, bloom_formats[halation]);
        impl->bloom_downsample_pipelines[halation] = make_bloom_pipeline(
            "almetal_world_bloom_downsample_fragment",
            halation ? "Alchemy world halation downsample pipeline"
                      : "Alchemy world bloom downsample pipeline",
            false, bloom_formats[halation]);
        impl->bloom_upsample_pipelines[halation] = make_bloom_pipeline(
            "almetal_world_bloom_upsample_fragment",
            halation ? "Alchemy world halation upsample pipeline"
                      : "Alchemy world bloom upsample pipeline",
            true, bloom_formats[halation]);
        if (!impl->bloom_extract_pipelines[halation] ||
            !impl->bloom_downsample_pipelines[halation] ||
            !impl->bloom_upsample_pipelines[halation])
        {
            return nullptr;
        }
    }

    const auto make_cross_filter_pipeline = [&](bool additive)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function = "almetal_resample_vertex";
        descriptor.fragment_function =
            "almetal_world_cross_filter_fragment";
        descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
        descriptor.colors[0].blending_enabled = additive;
        if (additive)
        {
            descriptor.colors[0].source_rgb = ALMetalBlendFactor::one;
            descriptor.colors[0].destination_rgb = ALMetalBlendFactor::one;
            descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
            descriptor.colors[0].destination_alpha =
                ALMetalBlendFactor::one;
        }
        descriptor.label = additive
            ? "Alchemy world additive cross-filter pipeline"
            : "Alchemy world cross-filter pipeline";
        return ALMetalRenderPipeline::create(device, descriptor, error);
    };
    impl->cross_filter_pipeline = make_cross_filter_pipeline(false);
    impl->cross_filter_add_pipeline = make_cross_filter_pipeline(true);
    if (!impl->cross_filter_pipeline ||
        !impl->cross_filter_add_pipeline)
    {
        return nullptr;
    }

    ALMetalRenderPipelineDescriptor cas_descriptor;
    cas_descriptor.library = impl->library.get();
    cas_descriptor.vertex_function = "almetal_resample_vertex";
    cas_descriptor.fragment_function = "almetal_world_cas_fragment";
    cas_descriptor.colors[0].format = ALMetalPixelFormat::rgb10a2_unorm;
    cas_descriptor.label = "Alchemy world standalone CAS pipeline";
    impl->cas_pipeline = ALMetalRenderPipeline::create(
        device, cas_descriptor, error);
    if (!impl->cas_pipeline)
    {
        return nullptr;
    }

    ALMetalRenderPipelineDescriptor fxaa_descriptor;
    fxaa_descriptor.library = impl->library.get();
    fxaa_descriptor.vertex_function = "almetal_resample_vertex";
    fxaa_descriptor.fragment_function = "almetal_world_fxaa_fragment";
    fxaa_descriptor.colors[0].format =
        ALMetalPixelFormat::rgb10a2_unorm;
    fxaa_descriptor.label = "Alchemy world one-pass FXAA pipeline";
    impl->fxaa_pipeline = ALMetalRenderPipeline::create(
        device, fxaa_descriptor, error);
    if (!impl->fxaa_pipeline)
    {
        return nullptr;
    }

    constexpr std::array<const char*, 2> smaa_edge_functions{{
        "almetal_world_smaa_edge_fragment",
        "almetal_world_smaa_edge_predicated_fragment",
    }};
    for (std::size_t predication = 0;
         predication < smaa_edge_functions.size(); ++predication)
    {
        ALMetalRenderPipelineDescriptor smaa_edge_descriptor;
        smaa_edge_descriptor.library = impl->library.get();
        smaa_edge_descriptor.vertex_function = "almetal_resample_vertex";
        smaa_edge_descriptor.fragment_function =
            smaa_edge_functions[predication];
        smaa_edge_descriptor.colors[0].format =
            ALMetalPixelFormat::rgba8_unorm;
        smaa_edge_descriptor.label = predication
            ? "Alchemy world predicated SMAA edge pipeline"
            : "Alchemy world SMAA edge pipeline";
        impl->smaa_edge_pipelines[predication] =
            ALMetalRenderPipeline::create(
                device, smaa_edge_descriptor, error);
        if (!impl->smaa_edge_pipelines[predication])
        {
            return nullptr;
        }
    }

    ALMetalRenderPipelineDescriptor smaa_weight_descriptor;
    smaa_weight_descriptor.library = impl->library.get();
    smaa_weight_descriptor.vertex_function = "almetal_resample_vertex";
    smaa_weight_descriptor.fragment_function =
        "almetal_world_smaa_weight_fragment";
    smaa_weight_descriptor.colors[0].format =
        ALMetalPixelFormat::rgba8_unorm;
    smaa_weight_descriptor.label =
        "Alchemy world SMAA blending-weight pipeline";
    impl->smaa_weight_pipeline = ALMetalRenderPipeline::create(
        device, smaa_weight_descriptor, error);
    if (!impl->smaa_weight_pipeline)
    {
        return nullptr;
    }

    ALMetalRenderPipelineDescriptor smaa_neighborhood_descriptor;
    smaa_neighborhood_descriptor.library = impl->library.get();
    smaa_neighborhood_descriptor.vertex_function =
        "almetal_resample_vertex";
    smaa_neighborhood_descriptor.fragment_function =
        "almetal_world_smaa_neighborhood_fragment";
    smaa_neighborhood_descriptor.colors[0].format =
        ALMetalPixelFormat::rgb10a2_unorm;
    smaa_neighborhood_descriptor.label =
        "Alchemy world SMAA neighborhood pipeline";
    impl->smaa_neighborhood_pipeline = ALMetalRenderPipeline::create(
        device, smaa_neighborhood_descriptor, error);
    if (!impl->smaa_neighborhood_pipeline)
    {
        return nullptr;
    }

    constexpr std::array<const char*, 4> dof_gather_functions{{
        "almetal_world_dof_gather_far_fragment",
        "almetal_world_dof_gather_near_fragment",
        "almetal_world_dof_gather_far_shaped_fragment",
        "almetal_world_dof_gather_near_shaped_fragment",
    }};
    constexpr std::array<ALMetalPixelFormat, 2> dof_blur_formats{{
        ALMetalPixelFormat::rg11b10_float,
        ALMetalPixelFormat::rgba16_float,
    }};
    for (std::size_t format = 0; format < dof_blur_formats.size(); ++format)
    {
        for (std::size_t variant = 0;
             variant < dof_gather_functions.size(); ++variant)
        {
            ALMetalRenderPipelineDescriptor descriptor;
            descriptor.library = impl->library.get();
            descriptor.vertex_function = "almetal_resample_vertex";
            descriptor.fragment_function = dof_gather_functions[variant];
            descriptor.colors[0].format = dof_blur_formats[format];
            descriptor.label = "Alchemy world depth-of-field gather pipeline";
            const std::size_t index = format * dof_gather_functions.size() +
                variant;
            impl->depth_of_field_gather_pipelines[index] =
                ALMetalRenderPipeline::create(device, descriptor, error);
            if (!impl->depth_of_field_gather_pipelines[index])
            {
                return nullptr;
            }
        }
    }

    constexpr std::array<const char*, 2> dof_combine_functions{{
        "almetal_world_dof_combine_far_fragment",
        "almetal_world_dof_combine_near_fragment",
    }};
    for (std::size_t variant = 0;
         variant < dof_combine_functions.size(); ++variant)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function = "almetal_resample_vertex";
        descriptor.fragment_function = dof_combine_functions[variant];
        descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
        descriptor.label = "Alchemy world depth-of-field combine pipeline";
        impl->depth_of_field_combine_pipelines[variant] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->depth_of_field_combine_pipelines[variant])
        {
            return nullptr;
        }
    }

    ALMetalComputePipelineDescriptor exposure_descriptor;
    exposure_descriptor.library = impl->library.get();
    exposure_descriptor.function = "almetal_world_exposure_meter";
    exposure_descriptor.label = "Alchemy world exposure-meter pipeline";
    impl->exposure_meter_pipeline = ALMetalComputePipeline::create(
        device, exposure_descriptor, error);
    if (!impl->exposure_meter_pipeline)
    {
        return nullptr;
    }
    constexpr std::array<const char*, 2> exposure_functions{{
        "almetal_world_exposure_initial",
        "almetal_world_exposure_history",
    }};
    for (std::size_t history = 0; history < exposure_functions.size();
         ++history)
    {
        exposure_descriptor.function = exposure_functions[history];
        exposure_descriptor.label = history
            ? "Alchemy world exposure-history pipeline"
            : "Alchemy world initial-exposure pipeline";
        impl->exposure_pipelines[history] = ALMetalComputePipeline::create(
            device, exposure_descriptor, error);
        if (!impl->exposure_pipelines[history])
        {
            return nullptr;
        }
    }

    ALMetalRenderPipelineDescriptor flare_state_descriptor;
    flare_state_descriptor.library = impl->library.get();
    flare_state_descriptor.vertex_function = "almetal_resample_vertex";
    flare_state_descriptor.fragment_function =
        "almetal_world_lens_flare_state_fragment";
    flare_state_descriptor.colors[0].format =
        ALMetalPixelFormat::rgba16_float;
    flare_state_descriptor.label =
        "Alchemy world lens-flare state pipeline";
    impl->lens_flare_state_pipeline = ALMetalRenderPipeline::create(
        device, flare_state_descriptor, error);
    if (!impl->lens_flare_state_pipeline)
    {
        return nullptr;
    }

    ALMetalRenderPipelineDescriptor dirt_descriptor;
    dirt_descriptor.library = impl->library.get();
    dirt_descriptor.vertex_function = "almetal_resample_vertex";
    dirt_descriptor.fragment_function =
        "almetal_world_lens_dirt_fragment";
    dirt_descriptor.colors[0].format = ALMetalPixelFormat::r8_unorm;
    dirt_descriptor.label = "Alchemy world lens-dirt pipeline";
    impl->lens_dirt_pipeline = ALMetalRenderPipeline::create(
        device, dirt_descriptor, error);
    if (!impl->lens_dirt_pipeline)
    {
        return nullptr;
    }

    ALMetalRenderPipelineDescriptor presentation_descriptor;
    presentation_descriptor.library = impl->library.get();
    presentation_descriptor.vertex_function = "almetal_resample_vertex";
    presentation_descriptor.fragment_function =
        "almetal_world_presentation_fragment";
    presentation_descriptor.colors[0].format =
        ALMetalPixelFormat::bgra8_unorm;
    presentation_descriptor.label =
        "Alchemy world final presentation pipeline";
    impl->presentation_post_pipeline = ALMetalRenderPipeline::create(
        device, presentation_descriptor, error);
    if (!impl->presentation_post_pipeline)
    {
        return nullptr;
    }

    constexpr std::array<ALMetalPixelFormat, 2> emissive_formats{{
        ALMetalPixelFormat::rgba8_unorm,
        ALMetalPixelFormat::rg11b10_float,
    }};
    constexpr std::array<ALMetalPixelFormat, 2> screen_formats{{
        ALMetalPixelFormat::rgba8_unorm,
        ALMetalPixelFormat::rgba16_float,
    }};
    for (std::size_t hdr = 0; hdr < screen_formats.size(); ++hdr)
    {
        ALMetalRenderPipelineDescriptor highlight_descriptor;
        highlight_descriptor.library = impl->library.get();
        highlight_descriptor.vertex_function =
            "almetal_world_highlight_vertex";
        highlight_descriptor.fragment_function =
            "almetal_world_highlight_fragment";
        highlight_descriptor.colors[0].format = screen_formats[hdr];
        highlight_descriptor.colors[0].blending_enabled = true;
        highlight_descriptor.colors[0].source_rgb =
            ALMetalBlendFactor::source_alpha;
        highlight_descriptor.colors[0].destination_rgb =
            ALMetalBlendFactor::one_minus_source_alpha;
        highlight_descriptor.colors[0].source_alpha =
            ALMetalBlendFactor::source_alpha;
        highlight_descriptor.colors[0].destination_alpha =
            ALMetalBlendFactor::one_minus_source_alpha;
        highlight_descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        highlight_descriptor.label = hdr
            ? "Alchemy HDR world selected-face highlight pipeline"
            : "Alchemy world selected-face highlight pipeline";
        impl->highlight_pipelines[hdr] = ALMetalRenderPipeline::create(
            device, highlight_descriptor, error);
        if (!impl->highlight_pipelines[hdr])
        {
            return nullptr;
        }

        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function = "almetal_world_local_light_vertex";
        descriptor.fragment_function = "almetal_world_local_light_fragment";
        descriptor.colors[0].format = screen_formats[hdr];
        descriptor.label = hdr
            ? "Alchemy HDR world local-point-light pipeline"
            : "Alchemy world local-point-light pipeline";
        impl->local_light_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->local_light_pipelines[hdr])
        {
            return nullptr;
        }

        descriptor.colors[0].blending_enabled = true;
        descriptor.colors[0].source_rgb = ALMetalBlendFactor::one;
        descriptor.colors[0].destination_rgb = ALMetalBlendFactor::one;
        descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
        descriptor.colors[0].destination_alpha = ALMetalBlendFactor::one;
        descriptor.label = hdr
            ? "Alchemy HDR world local-point-light accumulation pipeline"
            : "Alchemy world local-point-light accumulation pipeline";
        impl->local_light_accumulation_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->local_light_accumulation_pipelines[hdr])
        {
            return nullptr;
        }

        descriptor.fragment_function =
            "almetal_world_projector_light_fragment";
        descriptor.label = hdr
            ? "Alchemy HDR world projector-light pipeline"
            : "Alchemy world projector-light pipeline";
        impl->projector_light_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->projector_light_pipelines[hdr])
        {
            return nullptr;
        }

        // The following owners replace the fragment function and do not
        // accumulate multiple draws into one target.
        descriptor.colors[0].blending_enabled = false;
        descriptor.colors[0].destination_rgb = ALMetalBlendFactor::zero;
        descriptor.colors[0].destination_alpha = ALMetalBlendFactor::zero;

        descriptor.fragment_function =
            "almetal_world_lighting_composition_fragment";
        descriptor.label = hdr
            ? "Alchemy HDR world lighting-composition pipeline"
            : "Alchemy world lighting-composition pipeline";
        impl->lighting_composition_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->lighting_composition_pipelines[hdr])
        {
            return nullptr;
        }

        descriptor.vertex_function = "almetal_resample_vertex";
        constexpr std::array<std::array<const char*, 2>, 2>
            display_functions{{
                {{"almetal_world_display_post_fragment",
                  "almetal_world_display_post_exposure_fragment"}},
                {{"almetal_world_display_post_bloom_fragment",
                  "almetal_world_display_post_bloom_exposure_fragment"}},
            }};
        for (std::size_t preserve_alpha = 0;
             preserve_alpha < 2; ++preserve_alpha)
        {
            descriptor.colors[0].format = hdr && !preserve_alpha
                ? ALMetalPixelFormat::rgb10a2_unorm
                : ALMetalPixelFormat::rgba8_unorm;
            for (std::size_t bloom = 0;
                 bloom < display_functions.size(); ++bloom)
            {
                for (std::size_t dynamic = 0;
                     dynamic < display_functions[bloom].size(); ++dynamic)
                {
                    descriptor.fragment_function =
                        display_functions[bloom][dynamic];
                    descriptor.label = std::string("Alchemy ") +
                        (hdr ? "HDR " : "") +
                        (preserve_alpha ? "alpha-preserving " : "") +
                        (bloom ? "bloom " : "") +
                        (dynamic ? "dynamic-exposure " : "") +
                        "world display-post pipeline";
                    impl->display_post_pipelines
                        [hdr][preserve_alpha][bloom][dynamic] =
                            ALMetalRenderPipeline::create(
                                device, descriptor, error);
                    if (!impl->display_post_pipelines
                            [hdr][preserve_alpha][bloom][dynamic])
                    {
                        return nullptr;
                    }
                }
            }
        }

        descriptor.vertex_function = "almetal_world_local_light_vertex";

        descriptor.fragment_function =
            "almetal_world_ssr_history_fragment";
        descriptor.colors[0].format = hdr
            ? ALMetalPixelFormat::rg11b10_float
            : ALMetalPixelFormat::rgba8_unorm;
        descriptor.color_attachment_count = 2;
        descriptor.colors[1].format = ALMetalPixelFormat::r32_float;
        descriptor.label = hdr
            ? "Alchemy HDR world SSR-history publication pipeline"
            : "Alchemy world SSR-history publication pipeline";
        impl->ssr_history_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->ssr_history_pipelines[hdr])
        {
            return nullptr;
        }

        descriptor.colors[1] = {};
        descriptor.color_attachment_count = 1;
        descriptor.colors[0].format = screen_formats[hdr];

        descriptor.fragment_function = "almetal_world_base_light_fragment";
        descriptor.label = hdr
            ? "Alchemy HDR world base-light pipeline"
            : "Alchemy world base-light pipeline";
        impl->base_light_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->base_light_pipelines[hdr])
        {
            return nullptr;
        }

        ALMetalRenderPipelineDescriptor sky_behind_descriptor;
        sky_behind_descriptor.library = impl->library.get();
        sky_behind_descriptor.vertex_function =
            "almetal_world_windlight_sky_vertex";
        sky_behind_descriptor.fragment_function =
            "almetal_world_windlight_sky_behind_fragment";
        sky_behind_descriptor.colors[0].format = emissive_formats[hdr];
        sky_behind_descriptor.label = hdr
            ? "Alchemy HDR WindLight sky-behind pipeline"
            : "Alchemy vintage WindLight sky-behind pipeline";
        impl->windlight_sky_behind_pipelines[hdr] =
            ALMetalRenderPipeline::create(
                device, sky_behind_descriptor, error);
        if (!impl->windlight_sky_behind_pipelines[hdr])
        {
            return nullptr;
        }

        for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
        {
            ALMetalRenderPipelineDescriptor sky_descriptor;
            sky_descriptor.library = impl->library.get();
            sky_descriptor.vertex_function =
                "almetal_world_hdri_sky_vertex";
            sky_descriptor.fragment_function =
                "almetal_world_hdri_sky_fragment";
            sky_descriptor.color_attachment_count = 4;
            sky_descriptor.colors[0].format =
                ALMetalPixelFormat::rgba8_unorm_srgb;
            sky_descriptor.colors[1].format =
                ALMetalPixelFormat::rgba8_unorm;
            sky_descriptor.colors[2].format = normal_hdr
                ? ALMetalPixelFormat::rgba16_unorm
                : ALMetalPixelFormat::rgb10a2_unorm;
            sky_descriptor.colors[3].format = emissive_formats[hdr];
            sky_descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            sky_descriptor.label = hdr
                ? "Alchemy HDRI sky G-buffer pipeline"
                : "Alchemy vintage HDRI sky G-buffer pipeline";
            impl->hdri_sky_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->hdri_sky_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }

            sky_descriptor.fragment_function =
                "almetal_world_windlight_sky_fragment";
            sky_descriptor.vertex_function =
                "almetal_world_windlight_sky_vertex";
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight sky G-buffer pipeline"
                : "Alchemy vintage WindLight sky G-buffer pipeline";
            impl->windlight_sky_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_sky_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }

            sky_descriptor.vertex_function =
                "almetal_world_star_vertex";
            sky_descriptor.fragment_function =
                "almetal_world_star_fragment";
            // The vintage GL owner disables framebuffer sRGB and adds star
            // values directly to the encoded albedo bytes. Its Metal pass
            // therefore targets the compatible linear UNORM view. HDR writes
            // stars to emissive and keeps the ordinary sRGB albedo view.
            if (hdr == 0)
            {
                sky_descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm;
            }
            for (std::size_t attachment = 0; attachment < 4;
                 ++attachment)
            {
                sky_descriptor.colors[attachment].blending_enabled = true;
                sky_descriptor.colors[attachment].source_rgb =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].destination_rgb =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].source_alpha =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].destination_alpha =
                    ALMetalBlendFactor::one;
            }
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight star G-buffer pipeline"
                : "Alchemy vintage WindLight raw-storage star G-buffer pipeline";
            impl->windlight_star_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_star_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }

            sky_descriptor.vertex_function =
                "almetal_world_heavenly_body_vertex";
            sky_descriptor.fragment_function =
                "almetal_world_heavenly_body_fragment";
            for (std::size_t attachment = 0; attachment < 4;
                 ++attachment)
            {
                sky_descriptor.colors[attachment].source_rgb =
                    ALMetalBlendFactor::source_alpha;
                sky_descriptor.colors[attachment].destination_rgb =
                    ALMetalBlendFactor::one_minus_source_alpha;
                sky_descriptor.colors[attachment].source_alpha =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].destination_alpha =
                    ALMetalBlendFactor::one_minus_source_alpha;
            }
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight sun G-buffer pipeline"
                : "Alchemy vintage WindLight raw-storage sun G-buffer pipeline";
            impl->windlight_sun_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_sun_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight moon G-buffer pipeline"
                : "Alchemy vintage WindLight raw-storage moon G-buffer pipeline";
            impl->windlight_moon_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_moon_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }

            sky_descriptor.vertex_function =
                "almetal_world_meteor_vertex";
            sky_descriptor.fragment_function =
                "almetal_world_meteor_fragment";
            for (std::size_t attachment = 0; attachment < 4;
                 ++attachment)
            {
                sky_descriptor.colors[attachment].source_rgb =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].destination_rgb =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].source_alpha =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].destination_alpha =
                    ALMetalBlendFactor::one;
            }
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight meteor G-buffer pipeline"
                : "Alchemy vintage WindLight raw-storage meteor G-buffer pipeline";
            impl->windlight_meteor_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_meteor_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }

            sky_descriptor.vertex_function =
                "almetal_world_aurora_vertex";
            sky_descriptor.fragment_function =
                "almetal_world_aurora_fragment";
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight aurora G-buffer pipeline"
                : "Alchemy vintage WindLight raw-storage aurora G-buffer pipeline";
            impl->windlight_aurora_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_aurora_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }

            sky_descriptor.vertex_function =
                "almetal_world_cloud_vertex";
            sky_descriptor.fragment_function =
                "almetal_world_cloud_fragment";
            for (std::size_t attachment = 0; attachment < 4;
                 ++attachment)
            {
                sky_descriptor.colors[attachment].source_rgb =
                    ALMetalBlendFactor::source_alpha;
                sky_descriptor.colors[attachment].destination_rgb =
                    ALMetalBlendFactor::one_minus_source_alpha;
                sky_descriptor.colors[attachment].source_alpha =
                    ALMetalBlendFactor::one;
                sky_descriptor.colors[attachment].destination_alpha =
                    ALMetalBlendFactor::one_minus_source_alpha;
            }
            sky_descriptor.label = hdr
                ? "Alchemy HDR WindLight cloud G-buffer pipeline"
                : "Alchemy vintage WindLight raw-storage cloud G-buffer pipeline";
            impl->windlight_cloud_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(
                    device, sky_descriptor, error);
            if (!impl->windlight_cloud_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }
        }
    }

    ALMetalComputePipelineDescriptor brdf_descriptor;
    brdf_descriptor.library = impl->library.get();
    brdf_descriptor.function = "almetal_pbr_preview_brdf_lut";
    brdf_descriptor.label = "Alchemy world PBR BRDF LUT pipeline";
    impl->brdf_lut_pipeline = ALMetalComputePipeline::create(
        device, brdf_descriptor, error);
    if (!impl->brdf_lut_pipeline)
    {
        return nullptr;
    }

    ALMetalComputePipelineDescriptor hero_filter_descriptor;
    hero_filter_descriptor.library = impl->library.get();
    hero_filter_descriptor.function =
        "almetal_world_hero_probe_filter";
    hero_filter_descriptor.label =
        "Alchemy world hero-probe radiance filter pipeline";
    impl->hero_probe_filter_pipeline = ALMetalComputePipeline::create(
        device, hero_filter_descriptor, error);
    if (!impl->hero_probe_filter_pipeline)
    {
        return nullptr;
    }

    ALMetalComputePipelineDescriptor probe_compute_descriptor;
    probe_compute_descriptor.library = impl->library.get();
    probe_compute_descriptor.function = "almetal_world_probe_gaussian";
    probe_compute_descriptor.label =
        "Alchemy world probe Gaussian pipeline";
    impl->probe_gaussian_pipeline = ALMetalComputePipeline::create(
        device, probe_compute_descriptor, error);
    if (!impl->probe_gaussian_pipeline)
    {
        return nullptr;
    }
    probe_compute_descriptor.function =
        "almetal_world_probe_downsample";
    probe_compute_descriptor.label =
        "Alchemy world probe downsample pipeline";
    impl->probe_downsample_pipeline = ALMetalComputePipeline::create(
        device, probe_compute_descriptor, error);
    if (!impl->probe_downsample_pipeline)
    {
        return nullptr;
    }
    probe_compute_descriptor.function =
        "almetal_world_probe_sh_rows";
    probe_compute_descriptor.label =
        "Alchemy world probe SH row pipeline";
    impl->probe_sh_rows_pipeline = ALMetalComputePipeline::create(
        device, probe_compute_descriptor, error);
    if (!impl->probe_sh_rows_pipeline)
    {
        return nullptr;
    }
    probe_compute_descriptor.function =
        "almetal_world_probe_sh_reduce";
    probe_compute_descriptor.label =
        "Alchemy world probe SH reduce pipeline";
    impl->probe_sh_reduce_pipeline = ALMetalComputePipeline::create(
        device, probe_compute_descriptor, error);
    if (!impl->probe_sh_reduce_pipeline)
    {
        return nullptr;
    }

    ALMetalComputePipelineDescriptor water_compute_descriptor;
    water_compute_descriptor.library = impl->library.get();
    water_compute_descriptor.function =
        "almetal_world_depth_pyramid_first";
    water_compute_descriptor.label =
        "Alchemy world water depth-pyramid first pipeline";
    impl->water_depth_pyramid_first_pipeline =
        ALMetalComputePipeline::create(
            device, water_compute_descriptor, error);
    if (!impl->water_depth_pyramid_first_pipeline)
    {
        return nullptr;
    }
    water_compute_descriptor.function =
        "almetal_world_depth_pyramid_last";
    water_compute_descriptor.label =
        "Alchemy world water depth-pyramid last pipeline";
    impl->water_depth_pyramid_last_pipeline =
        ALMetalComputePipeline::create(
            device, water_compute_descriptor, error);
    if (!impl->water_depth_pyramid_last_pipeline)
    {
        return nullptr;
    }
    water_compute_descriptor.function =
        "almetal_world_water_wave_occlusion";
    water_compute_descriptor.label =
        "Alchemy world water wave-occlusion pipeline";
    impl->water_wave_occlusion_pipeline =
        ALMetalComputePipeline::create(
            device, water_compute_descriptor, error);
    if (!impl->water_wave_occlusion_pipeline)
    {
        return nullptr;
    }

    constexpr std::uint32_t brdf_lut_size = 512;
    ALMetalTextureDescriptor brdf_texture;
    brdf_texture.format = ALMetalPixelFormat::rg16_float;
    brdf_texture.width = brdf_lut_size;
    brdf_texture.height = brdf_lut_size;
    brdf_texture.storage = ALMetalStorageMode::private_gpu;
    brdf_texture.usage = ALMetalTextureUsage::shader_read |
                         ALMetalTextureUsage::shader_write;
    brdf_texture.label = "Alchemy world PBR BRDF LUT";
    impl->brdf_lut = ALMetalTexture::create(device, brdf_texture, error);
    if (!impl->brdf_lut)
    {
        return nullptr;
    }
    constexpr std::array<const char*, 3> avatar_vertex_functions{{
        "almetal_world_avatar_rigid_vertex",
        "almetal_world_avatar_skinned_vertex",
        "almetal_world_avatar_cloth_skinned_vertex",
    }};
    constexpr std::array<const char*, 3> avatar_alpha_vertex_functions{{
        "almetal_world_avatar_alpha_rigid_vertex",
        "almetal_world_avatar_alpha_skinned_vertex",
        "almetal_world_avatar_alpha_cloth_skinned_vertex",
    }};
    const auto avatar_rgb_write_mask = static_cast<ALMetalColorWriteMask>(
        static_cast<std::uint8_t>(ALMetalColorWriteMask::red) |
        static_cast<std::uint8_t>(ALMetalColorWriteMask::green) |
        static_cast<std::uint8_t>(ALMetalColorWriteMask::blue));
    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t alpha_mask = 0; alpha_mask < 2; ++alpha_mask)
        {
            for (std::size_t rigged = 0; rigged < 2; ++rigged)
            {
                for (std::size_t normal_hdr = 0; normal_hdr < 2;
                     ++normal_hdr)
                {
                    ALMetalRenderPipelineDescriptor descriptor;
                    descriptor.library = impl->library.get();
                    descriptor.vertex_function = rigged
                        ? "almetal_world_fullbright_skinned_vertex"
                        : "almetal_world_fullbright_vertex";
                    descriptor.fragment_function =
                        "almetal_world_fullbright_fragment";
                    descriptor.color_attachment_count = 4;
                    descriptor.colors[0].format =
                        ALMetalPixelFormat::rgba8_unorm_srgb;
                    descriptor.colors[0].blending_enabled = !alpha_mask;
                    descriptor.colors[0].source_rgb =
                        ALMetalBlendFactor::source_alpha;
                    descriptor.colors[0].destination_rgb =
                        ALMetalBlendFactor::one_minus_source_alpha;
                    descriptor.colors[0].source_alpha =
                        ALMetalBlendFactor::one;
                    descriptor.colors[0].destination_alpha =
                        ALMetalBlendFactor::one_minus_source_alpha;
                    descriptor.colors[1].format =
                        ALMetalPixelFormat::rgba8_unorm;
                    descriptor.colors[1].write_mask =
                        ALMetalColorWriteMask::none;
                    descriptor.colors[2].format = normal_hdr
                        ? ALMetalPixelFormat::rgba16_unorm
                        : ALMetalPixelFormat::rgb10a2_unorm;
                    descriptor.colors[2].write_mask =
                        ALMetalColorWriteMask::none;
                    descriptor.colors[3].format = emissive_formats[hdr];
                    descriptor.colors[3].write_mask =
                        ALMetalColorWriteMask::none;
                    descriptor.depth_stencil_format =
                        ALMetalPixelFormat::depth32_float_stencil8;
                    descriptor.label = alpha_mask
                        ? "Alchemy world fullbright-mask impostor pipeline"
                        : "Alchemy world fullbright impostor pipeline";
                    auto& pipeline =
                        impl->fullbright_pipelines[hdr][alpha_mask]
                                                   [rigged][normal_hdr];
                    pipeline = ALMetalRenderPipeline::create(
                        device, descriptor, error);
                    if (!pipeline)
                    {
                        return nullptr;
                    }

                    if (normal_hdr == 0)
                    {
                        descriptor.color_attachment_count = 1;
                        descriptor.colors[0].format = screen_formats[hdr];
                        descriptor.label = alpha_mask
                            ? "Alchemy world final-color fullbright-mask pipeline"
                            : "Alchemy world final-color fullbright pipeline";
                        auto& opaque_pipeline =
                            impl->opaque_forward_fullbright_pipelines[hdr]
                                                                      [alpha_mask]
                                                                      [rigged];
                        opaque_pipeline = ALMetalRenderPipeline::create(
                            device, descriptor, error);
                        if (!opaque_pipeline)
                        {
                            return nullptr;
                        }
                    }
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < screen_formats.size(); ++hdr)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function =
            "almetal_world_water_exclusion_vertex";
        descriptor.fragment_function =
            "almetal_world_water_exclusion_fragment";
        descriptor.color_attachment_count = 1;
        descriptor.colors[0].format = screen_formats[hdr];
        descriptor.colors[0].write_mask = ALMetalColorWriteMask::none;
        descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        descriptor.label = "Alchemy world water-exclusion stencil pipeline";
        impl->water_exclusion_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->water_exclusion_pipelines[hdr])
        {
            return nullptr;
        }

        for (std::size_t above_water = 0; above_water < 2;
             ++above_water)
        {
            ALMetalRenderPipelineDescriptor haze_descriptor;
            haze_descriptor.library = impl->library.get();
            haze_descriptor.vertex_function = above_water
                ? "almetal_world_water_haze_field_vertex"
                : "almetal_world_water_haze_fullscreen_vertex";
            haze_descriptor.fragment_function =
                "almetal_world_water_haze_fragment";
            haze_descriptor.color_attachment_count = 1;
            haze_descriptor.colors[0].format = screen_formats[hdr];
            haze_descriptor.colors[0].blending_enabled = true;
            haze_descriptor.colors[0].source_rgb =
                ALMetalBlendFactor::one;
            haze_descriptor.colors[0].destination_rgb =
                ALMetalBlendFactor::source_alpha;
            haze_descriptor.colors[0].source_alpha =
                ALMetalBlendFactor::zero;
            haze_descriptor.colors[0].destination_alpha =
                ALMetalBlendFactor::source_alpha;
            haze_descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            haze_descriptor.label = above_water
                ? "Alchemy world above-water haze pipeline"
                : "Alchemy world underwater haze pipeline";
            impl->water_haze_pipelines[hdr][above_water] =
                ALMetalRenderPipeline::create(
                    device, haze_descriptor, error);
            if (!impl->water_haze_pipelines[hdr][above_water])
            {
                return nullptr;
            }
        }

        for (std::size_t coverage = 0; coverage < 2; ++coverage)
        {
            ALMetalRenderPipelineDescriptor haze_descriptor;
            haze_descriptor.library = impl->library.get();
            haze_descriptor.vertex_function =
                "almetal_world_atmospheric_haze_vertex";
            haze_descriptor.fragment_function =
                "almetal_world_atmospheric_haze_fragment";
            haze_descriptor.colors[0].format = screen_formats[hdr];
            haze_descriptor.colors[0].blending_enabled = true;
            haze_descriptor.colors[0].source_rgb =
                ALMetalBlendFactor::one;
            haze_descriptor.colors[0].destination_rgb =
                ALMetalBlendFactor::source_alpha;
            haze_descriptor.colors[0].source_alpha =
                ALMetalBlendFactor::zero;
            haze_descriptor.colors[0].destination_alpha = coverage
                ? ALMetalBlendFactor::one
                : ALMetalBlendFactor::source_alpha;
            haze_descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            haze_descriptor.label = coverage
                ? "Alchemy world atmospheric haze coverage pipeline"
                : "Alchemy world atmospheric haze pipeline";
            impl->atmospheric_haze_pipelines[hdr][coverage] =
                ALMetalRenderPipeline::create(
                    device, haze_descriptor, error);
            if (!impl->atmospheric_haze_pipelines[hdr][coverage])
            {
                return nullptr;
            }
        }

        ALMetalRenderPipelineDescriptor underwater_descriptor;
        underwater_descriptor.library = impl->library.get();
        underwater_descriptor.vertex_function =
            "almetal_world_flat_water_vertex";
        underwater_descriptor.fragment_function =
            "almetal_world_underwater_fragment";
        underwater_descriptor.color_attachment_count = 1;
        underwater_descriptor.colors[0].format = screen_formats[hdr];
        underwater_descriptor.colors[0].write_mask =
            static_cast<ALMetalColorWriteMask>(
                static_cast<std::uint8_t>(ALMetalColorWriteMask::red) |
                static_cast<std::uint8_t>(ALMetalColorWriteMask::green) |
                static_cast<std::uint8_t>(ALMetalColorWriteMask::blue));
        underwater_descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        underwater_descriptor.label =
            "Alchemy world flat underwater surface pipeline";
        impl->underwater_flat_water_pipelines[hdr] =
            ALMetalRenderPipeline::create(
                device, underwater_descriptor, error);
        if (!impl->underwater_flat_water_pipelines[hdr])
        {
            return nullptr;
        }

        ALMetalRenderPipelineDescriptor underwater_wave_descriptor =
            underwater_descriptor;
        underwater_wave_descriptor.vertex_function =
            "almetal_world_wave_water_vertex";
        underwater_wave_descriptor.tessellation.enabled = true;
        underwater_wave_descriptor.tessellation.partition_mode =
            ALMetalTessellationPartitionMode::fractional_odd;
        underwater_wave_descriptor.tessellation.maximum_factor = 64;
        underwater_wave_descriptor.tessellation.factor_step_function =
            ALMetalTessellationFactorStepFunction::per_patch;
        underwater_wave_descriptor.tessellation.output_winding =
            ALMetalTessellationOutputWinding::counter_clockwise;
        underwater_wave_descriptor.label =
            "Alchemy world wave underwater surface pipeline";
        impl->underwater_wave_water_pipelines[hdr] =
            ALMetalRenderPipeline::create(
                device, underwater_wave_descriptor, error);
        if (!impl->underwater_wave_water_pipelines[hdr])
        {
            return nullptr;
        }

        ALMetalRenderPipelineDescriptor above_water_descriptor =
            underwater_descriptor;
        above_water_descriptor.fragment_function =
            "almetal_world_above_water_fragment";
        above_water_descriptor.colors[0].write_mask =
            ALMetalColorWriteMask::all;
        above_water_descriptor.colors[0].blending_enabled = true;
        above_water_descriptor.colors[0].source_rgb =
            ALMetalBlendFactor::one;
        above_water_descriptor.colors[0].destination_rgb =
            ALMetalBlendFactor::zero;
        above_water_descriptor.colors[0].rgb_operation =
            ALMetalBlendOperation::add;
        above_water_descriptor.colors[0].source_alpha =
            ALMetalBlendFactor::one;
        above_water_descriptor.colors[0].destination_alpha =
            ALMetalBlendFactor::one;
        above_water_descriptor.colors[0].alpha_operation =
            ALMetalBlendOperation::maximum;
        above_water_descriptor.label =
            "Alchemy world flat above-water surface pipeline";
        impl->above_water_flat_water_pipelines[hdr] =
            ALMetalRenderPipeline::create(
                device, above_water_descriptor, error);
        if (!impl->above_water_flat_water_pipelines[hdr])
        {
            return nullptr;
        }

        ALMetalRenderPipelineDescriptor above_water_wave_descriptor =
            above_water_descriptor;
        above_water_wave_descriptor.vertex_function =
            "almetal_world_wave_water_vertex";
        above_water_wave_descriptor.tessellation.enabled = true;
        above_water_wave_descriptor.tessellation.partition_mode =
            ALMetalTessellationPartitionMode::fractional_odd;
        above_water_wave_descriptor.tessellation.maximum_factor = 64;
        above_water_wave_descriptor.tessellation.factor_step_function =
            ALMetalTessellationFactorStepFunction::per_patch;
        above_water_wave_descriptor.tessellation.output_winding =
            ALMetalTessellationOutputWinding::counter_clockwise;
        above_water_wave_descriptor.label =
            "Alchemy world wave above-water surface pipeline";
        impl->above_water_wave_water_pipelines[hdr] =
            ALMetalRenderPipeline::create(
                device, above_water_wave_descriptor, error);
        if (!impl->above_water_wave_water_pipelines[hdr])
        {
            return nullptr;
        }

        for (std::size_t coverage = 0; coverage < 2; ++coverage)
        {
            for (std::size_t rigged = 0; rigged < 2; ++rigged)
            {
                ALMetalRenderPipelineDescriptor alpha_descriptor;
                alpha_descriptor.library = impl->library.get();
                alpha_descriptor.vertex_function = rigged
                    ? "almetal_world_shiny_skinned_vertex"
                    : "almetal_world_shiny_vertex";
                alpha_descriptor.fragment_function =
                    "almetal_world_pre_water_fullbright_fragment";
                alpha_descriptor.color_attachment_count = 1;
                alpha_descriptor.colors[0].format = screen_formats[hdr];
                alpha_descriptor.colors[0].blending_enabled = true;
                alpha_descriptor.colors[0].source_rgb =
                    ALMetalBlendFactor::source_alpha;
                alpha_descriptor.colors[0].destination_rgb =
                    ALMetalBlendFactor::one_minus_source_alpha;
                alpha_descriptor.colors[0].source_alpha = coverage
                    ? ALMetalBlendFactor::one
                    : ALMetalBlendFactor::zero;
                alpha_descriptor.colors[0].destination_alpha =
                    ALMetalBlendFactor::one_minus_source_alpha;
                alpha_descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                alpha_descriptor.label = coverage
                    ? "Alchemy world coverage fullbright-alpha pipeline"
                    : "Alchemy world pre-water fullbright-alpha pipeline";
                impl->pre_water_fullbright_pipelines
                    [hdr][coverage][rigged] =
                    ALMetalRenderPipeline::create(
                        device, alpha_descriptor, error);
                if (!impl->pre_water_fullbright_pipelines
                        [hdr][coverage][rigged])
                {
                    return nullptr;
                }
                for (std::size_t shadows = 0; shadows < 2; ++shadows)
                {
                    alpha_descriptor.fragment_function = shadows
                        ? "almetal_world_pre_water_simple_shadow_fragment"
                        : "almetal_world_pre_water_simple_fragment";
                    alpha_descriptor.label = shadows
                        ? "Alchemy world simple-alpha shadow pipeline"
                        : "Alchemy world simple-alpha pipeline";
                    impl->pre_water_simple_pipelines
                        [hdr][coverage][shadows][rigged] =
                        ALMetalRenderPipeline::create(
                            device, alpha_descriptor, error);
                    if (!impl->pre_water_simple_pipelines
                            [hdr][coverage][shadows][rigged])
                    {
                        return nullptr;
                    }
                }

                alpha_descriptor.vertex_function = rigged
                    ? "almetal_world_legacy_alpha_skinned_vertex"
                    : "almetal_world_legacy_alpha_vertex";
                alpha_descriptor.fragment_function =
                    "almetal_world_pre_water_material_fragment";
                alpha_descriptor.label =
                    "Alchemy world legacy-material alpha pipeline";
                impl->pre_water_material_pipelines
                    [hdr][coverage][rigged] =
                    ALMetalRenderPipeline::create(
                        device, alpha_descriptor, error);
                if (!impl->pre_water_material_pipelines
                        [hdr][coverage][rigged])
                {
                    return nullptr;
                }

                alpha_descriptor.vertex_function = rigged
                    ? "almetal_world_pbr_skinned_vertex"
                    : "almetal_world_pbr_vertex";
                alpha_descriptor.fragment_function =
                    "almetal_world_pre_water_pbr_fragment";
                alpha_descriptor.label =
                    "Alchemy world PBR alpha pipeline";
                impl->pre_water_pbr_pipelines
                    [hdr][coverage][rigged] =
                    ALMetalRenderPipeline::create(
                        device, alpha_descriptor, error);
                if (!impl->pre_water_pbr_pipelines
                        [hdr][coverage][rigged])
                {
                    return nullptr;
                }
            }

            for (std::size_t shadows = 0; shadows < 2; ++shadows)
            {
                for (std::size_t variant = 0;
                     variant < avatar_alpha_vertex_functions.size();
                     ++variant)
                {
                    for (std::size_t color_write = 0;
                         color_write < 2; ++color_write)
                    {
                        ALMetalRenderPipelineDescriptor avatar_descriptor;
                        avatar_descriptor.library = impl->library.get();
                        avatar_descriptor.vertex_function =
                            avatar_alpha_vertex_functions[variant];
                        avatar_descriptor.fragment_function = shadows
                            ? "almetal_world_pre_water_simple_shadow_fragment"
                            : "almetal_world_pre_water_simple_fragment";
                        avatar_descriptor.color_attachment_count = 1;
                        avatar_descriptor.colors[0].format =
                            screen_formats[hdr];
                        avatar_descriptor.colors[0].blending_enabled = true;
                        avatar_descriptor.colors[0].source_rgb =
                            ALMetalBlendFactor::source_alpha;
                        avatar_descriptor.colors[0].destination_rgb =
                            ALMetalBlendFactor::one_minus_source_alpha;
                        avatar_descriptor.colors[0].source_alpha = coverage
                            ? ALMetalBlendFactor::one
                            : ALMetalBlendFactor::zero;
                        avatar_descriptor.colors[0].destination_alpha =
                            ALMetalBlendFactor::one_minus_source_alpha;
                        avatar_descriptor.colors[0].write_mask = !color_write
                            ? ALMetalColorWriteMask::none
                            : coverage ? ALMetalColorWriteMask::all
                                       : avatar_rgb_write_mask;
                        avatar_descriptor.depth_stencil_format =
                            ALMetalPixelFormat::depth32_float_stencil8;
                        avatar_descriptor.label = color_write
                            ? (shadows
                                   ? "Alchemy world avatar-alpha shadow pipeline"
                                   : "Alchemy world avatar-alpha pipeline")
                            : (shadows
                                   ? "Alchemy world avatar-alpha depth-only shadow pipeline"
                                   : "Alchemy world avatar-alpha depth-only pipeline");
                        impl->avatar_alpha_pipelines
                            [hdr][coverage][shadows][variant][color_write] =
                            ALMetalRenderPipeline::create(
                                device, avatar_descriptor, error);
                        if (!impl->avatar_alpha_pipelines
                                [hdr][coverage][shadows][variant][color_write])
                        {
                            return nullptr;
                        }
                    }
                }
            }
        }

        ALMetalRenderPipelineDescriptor depth_alpha_descriptor;
        depth_alpha_descriptor.library = impl->library.get();
        depth_alpha_descriptor.vertex_function =
            "almetal_world_shiny_vertex";
        depth_alpha_descriptor.fragment_function =
            "almetal_world_pre_water_fullbright_fragment";
        depth_alpha_descriptor.color_attachment_count = 1;
        depth_alpha_descriptor.colors[0].format = screen_formats[hdr];
        depth_alpha_descriptor.colors[0].write_mask =
            ALMetalColorWriteMask::none;
        depth_alpha_descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        depth_alpha_descriptor.label =
            "Alchemy world post-water flat DoF-depth pipeline";
        impl->post_water_depth_pipelines[hdr].fullbright =
            ALMetalRenderPipeline::create(
                device, depth_alpha_descriptor, error);
        if (!impl->post_water_depth_pipelines[hdr].fullbright)
        {
            return nullptr;
        }

        depth_alpha_descriptor.vertex_function =
            "almetal_world_legacy_alpha_vertex";
        depth_alpha_descriptor.fragment_function =
            "almetal_world_pre_water_material_fragment";
        depth_alpha_descriptor.label =
            "Alchemy world post-water material DoF-depth pipeline";
        impl->post_water_depth_pipelines[hdr].material =
            ALMetalRenderPipeline::create(
                device, depth_alpha_descriptor, error);
        if (!impl->post_water_depth_pipelines[hdr].material)
        {
            return nullptr;
        }

        depth_alpha_descriptor.vertex_function =
            "almetal_world_pbr_vertex";
        depth_alpha_descriptor.fragment_function =
            "almetal_world_pre_water_pbr_fragment";
        depth_alpha_descriptor.label =
            "Alchemy world post-water PBR DoF-depth pipeline";
        impl->post_water_depth_pipelines[hdr].pbr =
            ALMetalRenderPipeline::create(
                device, depth_alpha_descriptor, error);
        if (!impl->post_water_depth_pipelines[hdr].pbr)
        {
            return nullptr;
        }

        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            ALMetalRenderPipelineDescriptor glow_descriptor;
            glow_descriptor.library = impl->library.get();
            glow_descriptor.vertex_function = rigged
                ? "almetal_world_legacy_glow_skinned_vertex"
                : "almetal_world_legacy_glow_vertex";
            glow_descriptor.fragment_function =
                "almetal_world_legacy_glow_fragment";
            glow_descriptor.color_attachment_count = 1;
            glow_descriptor.colors[0].format = screen_formats[hdr];
            glow_descriptor.colors[0].blending_enabled = true;
            glow_descriptor.colors[0].source_alpha =
                ALMetalBlendFactor::one;
            glow_descriptor.colors[0].destination_alpha =
                ALMetalBlendFactor::one;
            glow_descriptor.colors[0].write_mask =
                ALMetalColorWriteMask::alpha;
            glow_descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            glow_descriptor.label =
                "Alchemy world legacy additive-glow pipeline";
            impl->legacy_glow_pipelines[hdr][rigged] =
                ALMetalRenderPipeline::create(
                    device, glow_descriptor, error);
            if (!impl->legacy_glow_pipelines[hdr][rigged])
            {
                return nullptr;
            }

            glow_descriptor.vertex_function = rigged
                ? "almetal_world_pbr_glow_skinned_vertex"
                : "almetal_world_pbr_glow_vertex";
            glow_descriptor.fragment_function =
                "almetal_world_pbr_glow_fragment";
            glow_descriptor.colors[0].blending_enabled = false;
            glow_descriptor.colors[0].destination_alpha =
                ALMetalBlendFactor::zero;
            glow_descriptor.label =
                "Alchemy world PBR glow-replacement pipeline";
            impl->pbr_glow_pipelines[hdr][rigged] =
                ALMetalRenderPipeline::create(
                    device, glow_descriptor, error);
            if (!impl->pbr_glow_pipelines[hdr][rigged])
            {
                return nullptr;
            }

            glow_descriptor.colors[0].blending_enabled = true;
            glow_descriptor.colors[0].source_alpha =
                ALMetalBlendFactor::one;
            glow_descriptor.colors[0].destination_alpha =
                ALMetalBlendFactor::one;
            glow_descriptor.label =
                "Alchemy world post-water PBR additive-glow pipeline";
            impl->post_water_pbr_glow_pipelines[hdr][rigged] =
                ALMetalRenderPipeline::create(
                    device, glow_descriptor, error);
            if (!impl->post_water_pbr_glow_pipelines[hdr][rigged])
            {
                return nullptr;
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_shiny_skinned_vertex"
                    : "almetal_world_shiny_vertex";
                descriptor.fragment_function = "almetal_world_shiny_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[1].write_mask =
                    ALMetalColorWriteMask::none;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[2].write_mask =
                    ALMetalColorWriteMask::none;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.colors[3].write_mask =
                    ALMetalColorWriteMask::none;
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label =
                    "Alchemy world shiny avatar-impostor pipeline";
                auto& pipeline =
                    impl->shiny_pipelines[hdr][rigged][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }

                if (normal_hdr == 0)
                {
                    descriptor.color_attachment_count = 1;
                    descriptor.colors[0].format = screen_formats[hdr];
                    descriptor.label =
                        "Alchemy world final-color shiny pipeline";
                    auto& opaque_pipeline =
                        impl->opaque_forward_shiny_pipelines[hdr][rigged];
                    opaque_pipeline = ALMetalRenderPipeline::create(
                        device, descriptor, error);
                    if (!opaque_pipeline)
                    {
                        return nullptr;
                    }
                }
            }
        }
    }

    const auto rgb_write_mask = static_cast<ALMetalColorWriteMask>(
        static_cast<std::uint8_t>(ALMetalColorWriteMask::red) |
        static_cast<std::uint8_t>(ALMetalColorWriteMask::green) |
        static_cast<std::uint8_t>(ALMetalColorWriteMask::blue));
    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_emboss_skinned_vertex"
                    : "almetal_world_emboss_vertex";
                descriptor.fragment_function =
                    "almetal_world_emboss_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[0].blending_enabled = true;
                descriptor.colors[0].source_rgb =
                    ALMetalBlendFactor::destination_color;
                descriptor.colors[0].destination_rgb =
                    ALMetalBlendFactor::source_color;
                descriptor.colors[0].write_mask = rgb_write_mask;
                descriptor.colors[1].format =
                    ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[1].write_mask =
                    ALMetalColorWriteMask::none;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[2].write_mask =
                    ALMetalColorWriteMask::none;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.colors[3].write_mask =
                    ALMetalColorWriteMask::none;
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label =
                    "Alchemy world emboss impostor pipeline";
                auto& pipeline =
                    impl->emboss_pipelines[hdr][rigged][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }

                if (normal_hdr == 0)
                {
                    descriptor.color_attachment_count = 1;
                    descriptor.colors[0].format = screen_formats[hdr];
                    descriptor.label =
                        "Alchemy world final-color emboss pipeline";
                    auto& opaque_pipeline =
                        impl->opaque_forward_emboss_pipelines[hdr][rigged];
                    opaque_pipeline = ALMetalRenderPipeline::create(
                        device, descriptor, error);
                    if (!opaque_pipeline)
                    {
                        return nullptr;
                    }
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_legacy_alpha_skinned_vertex"
                    : "almetal_world_legacy_alpha_vertex";
                descriptor.fragment_function =
                    "almetal_world_legacy_alpha_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[0].blending_enabled = true;
                descriptor.colors[0].source_rgb =
                    ALMetalBlendFactor::source_alpha;
                descriptor.colors[0].destination_rgb =
                    ALMetalBlendFactor::one_minus_source_alpha;
                descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
                descriptor.colors[0].destination_alpha =
                    ALMetalBlendFactor::one_minus_source_alpha;
                descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[1].write_mask = ALMetalColorWriteMask::none;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[2].write_mask = ALMetalColorWriteMask::none;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.colors[3].write_mask = ALMetalColorWriteMask::none;
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label =
                    "Alchemy world legacy-material impostor-alpha pipeline";
                auto& pipeline =
                    impl->legacy_alpha_pipelines[hdr][rigged][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_simple_skinned_vertex"
                    : "almetal_world_simple_vertex";
                descriptor.fragment_function =
                    "almetal_world_flat_alpha_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[0].blending_enabled = true;
                descriptor.colors[0].source_rgb =
                    ALMetalBlendFactor::source_alpha;
                descriptor.colors[0].destination_rgb =
                    ALMetalBlendFactor::one_minus_source_alpha;
                descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
                descriptor.colors[0].destination_alpha =
                    ALMetalBlendFactor::one_minus_source_alpha;
                descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[1].write_mask = ALMetalColorWriteMask::none;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[2].write_mask = ALMetalColorWriteMask::none;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.colors[3].write_mask = ALMetalColorWriteMask::none;
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label =
                    "Alchemy world flat impostor-alpha pipeline";
                auto& pipeline =
                    impl->flat_alpha_pipelines[hdr][rigged][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
        {
            ALMetalRenderPipelineDescriptor descriptor;
            descriptor.library = impl->library.get();
            descriptor.vertex_function = "almetal_world_impostor_vertex";
            descriptor.fragment_function = "almetal_world_impostor_fragment";
            descriptor.color_attachment_count = 4;
            descriptor.colors[0].format =
                ALMetalPixelFormat::rgba8_unorm_srgb;
            descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
            descriptor.colors[2].format = normal_hdr
                ? ALMetalPixelFormat::rgba16_unorm
                : ALMetalPixelFormat::rgb10a2_unorm;
            descriptor.colors[3].format = emissive_formats[hdr];
            descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            descriptor.label = "Alchemy world impostor G-buffer pipeline";
            impl->impostor_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(device, descriptor, error);
            if (!impl->impostor_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }
        }
    }
    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        ALMetalRenderPipelineDescriptor descriptor;
        descriptor.library = impl->library.get();
        descriptor.vertex_function = "almetal_world_coverage_vertex";
        descriptor.fragment_function = "almetal_world_coverage_fragment";
        descriptor.colors[0].format = hdr
            ? ALMetalPixelFormat::rgba16_float
            : ALMetalPixelFormat::rgba8_unorm;
        descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;

        // The coverage fragment returns (0, 0, 0, 1). Select zero as the
        // source-alpha blend factor to turn that same draw into a transparent
        // far-depth background clear without another shader permutation.
        descriptor.colors[0].blending_enabled = true;
        descriptor.colors[0].source_rgb = ALMetalBlendFactor::one;
        descriptor.colors[0].destination_rgb = ALMetalBlendFactor::zero;
        descriptor.colors[0].source_alpha = ALMetalBlendFactor::zero;
        descriptor.colors[0].destination_alpha = ALMetalBlendFactor::zero;
        descriptor.label = hdr
            ? "Alchemy HDR world cutout background pipeline"
            : "Alchemy world cutout background pipeline";
        impl->cutout_background_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->cutout_background_pipelines[hdr])
        {
            return nullptr;
        }

        descriptor.colors[0].blending_enabled = false;
        descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
        descriptor.colors[0].write_mask = ALMetalColorWriteMask::alpha;
        descriptor.label = hdr
            ? "Alchemy HDR world cutout opaque-coverage pipeline"
            : "Alchemy world cutout opaque-coverage pipeline";
        impl->cutout_coverage_pipelines[hdr] =
            ALMetalRenderPipeline::create(device, descriptor, error);
        if (!impl->cutout_coverage_pipelines[hdr])
        {
            return nullptr;
        }
    }
    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
        {
            ALMetalRenderPipelineDescriptor descriptor;
            descriptor.library = impl->library.get();
            descriptor.vertex_function = "almetal_world_coverage_vertex";
            descriptor.fragment_function = "almetal_world_coverage_fragment";
            descriptor.color_attachment_count = 4;
            descriptor.colors[0].format =
                ALMetalPixelFormat::rgba8_unorm_srgb;
            descriptor.colors[0].write_mask = ALMetalColorWriteMask::alpha;
            descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
            descriptor.colors[1].write_mask = ALMetalColorWriteMask::none;
            descriptor.colors[2].format = normal_hdr
                ? ALMetalPixelFormat::rgba16_unorm
                : ALMetalPixelFormat::rgb10a2_unorm;
            descriptor.colors[2].write_mask = ALMetalColorWriteMask::none;
            descriptor.colors[3].format = emissive_formats[hdr];
            descriptor.colors[3].write_mask = ALMetalColorWriteMask::none;
            descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            descriptor.label = "Alchemy world opaque-coverage pipeline";
            impl->coverage_pipelines[hdr][normal_hdr] =
                ALMetalRenderPipeline::create(device, descriptor, error);
            if (!impl->coverage_pipelines[hdr][normal_hdr])
            {
                return nullptr;
            }
        }
    }
    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t pinned = 0; pinned < 2; ++pinned)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = "almetal_world_terrain_vertex";
                descriptor.fragment_function = "almetal_world_terrain_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.tessellation.enabled = true;
                descriptor.tessellation.partition_mode = pinned
                    ? ALMetalTessellationPartitionMode::integer
                    : ALMetalTessellationPartitionMode::fractional_odd;
                descriptor.tessellation.maximum_factor = 64;
                descriptor.tessellation.factor_step_function =
                    ALMetalTessellationFactorStepFunction::per_patch;
                descriptor.tessellation.output_winding =
                    ALMetalTessellationOutputWinding::counter_clockwise;
                descriptor.label = "Alchemy world terrain G-buffer pipeline";
                auto& pipeline = impl->terrain_pipelines[hdr][pinned][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }

                descriptor.fragment_function =
                    "almetal_world_pbr_terrain_fragment";
                descriptor.label =
                    "Alchemy world PBR terrain G-buffer pipeline";
                auto& pbr_pipeline =
                    impl->pbr_terrain_pipelines[hdr][pinned][normal_hdr];
                pbr_pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pbr_pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t variant = 0;
             variant < avatar_vertex_functions.size(); ++variant)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = avatar_vertex_functions[variant];
                descriptor.fragment_function =
                    "almetal_world_alpha_mask_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[1].format =
                    ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label = "Alchemy world avatar G-buffer pipeline";
                auto& pipeline =
                    impl->avatar_pipelines[hdr][variant][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_pbr_skinned_vertex"
                    : "almetal_world_pbr_vertex";
                descriptor.fragment_function = "almetal_world_pbr_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[1].format =
                    ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label = "Alchemy world PBR G-buffer pipeline";
                auto& pipeline = impl->pbr_pipelines[hdr][rigged][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_pbr_skinned_vertex"
                    : "almetal_world_pbr_vertex";
                descriptor.fragment_function =
                    "almetal_world_pbr_alpha_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[0].blending_enabled = true;
                descriptor.colors[0].source_rgb =
                    ALMetalBlendFactor::source_alpha;
                descriptor.colors[0].destination_rgb =
                    ALMetalBlendFactor::one_minus_source_alpha;
                descriptor.colors[0].source_alpha = ALMetalBlendFactor::one;
                descriptor.colors[0].destination_alpha =
                    ALMetalBlendFactor::one_minus_source_alpha;
                descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[1].write_mask = ALMetalColorWriteMask::none;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[2].write_mask = ALMetalColorWriteMask::none;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.colors[3].write_mask = ALMetalColorWriteMask::none;
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label =
                    "Alchemy world PBR impostor-alpha pipeline";
                auto& pipeline =
                    impl->pbr_alpha_pipelines[hdr][rigged][normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t rigged = 0; rigged < 2; ++rigged)
        {
            for (std::size_t normal_hdr = 0; normal_hdr < 2; ++normal_hdr)
            {
                ALMetalRenderPipelineDescriptor descriptor;
                descriptor.library = impl->library.get();
                descriptor.vertex_function = rigged
                    ? "almetal_world_material_skinned_vertex"
                    : "almetal_world_material_vertex";
                descriptor.fragment_function =
                    "almetal_world_material_fragment";
                descriptor.color_attachment_count = 4;
                descriptor.colors[0].format =
                    ALMetalPixelFormat::rgba8_unorm_srgb;
                descriptor.colors[1].format =
                    ALMetalPixelFormat::rgba8_unorm;
                descriptor.colors[2].format = normal_hdr
                    ? ALMetalPixelFormat::rgba16_unorm
                    : ALMetalPixelFormat::rgb10a2_unorm;
                descriptor.colors[3].format = emissive_formats[hdr];
                descriptor.depth_stencil_format =
                    ALMetalPixelFormat::depth32_float_stencil8;
                descriptor.label =
                    "Alchemy world legacy material G-buffer pipeline";
                auto& pipeline = impl->material_pipelines[hdr][rigged]
                                                         [normal_hdr];
                pipeline = ALMetalRenderPipeline::create(
                    device, descriptor, error);
                if (!pipeline)
                {
                    return nullptr;
                }
            }
        }
    }

    for (std::size_t hdr = 0; hdr < emissive_formats.size(); ++hdr)
    {
        for (std::size_t bump = 0; bump < 2; ++bump)
        {
            for (std::size_t masked = 0; masked < 2; ++masked)
            {
                for (std::size_t rigged = 0; rigged < 2; ++rigged)
                {
                    for (std::size_t normal_hdr = 0; normal_hdr < 2;
                         ++normal_hdr)
                    {
                        ALMetalRenderPipelineDescriptor descriptor;
                        descriptor.library = impl->library.get();
                        descriptor.vertex_function = bump
                            ? (rigged ? "almetal_world_bump_skinned_vertex"
                                      : "almetal_world_bump_vertex")
                            : (rigged ? "almetal_world_simple_skinned_vertex"
                                      : "almetal_world_simple_vertex");
                        descriptor.fragment_function = bump
                            ? "almetal_world_bump_fragment"
                            : (masked ? "almetal_world_alpha_mask_fragment"
                                      : "almetal_world_simple_fragment");
                        descriptor.color_attachment_count = 4;
                        descriptor.colors[0].format =
                            ALMetalPixelFormat::rgba8_unorm_srgb;
                        descriptor.colors[1].format =
                            ALMetalPixelFormat::rgba8_unorm;
                        descriptor.colors[2].format = normal_hdr
                            ? ALMetalPixelFormat::rgba16_unorm
                            : ALMetalPixelFormat::rgb10a2_unorm;
                        descriptor.colors[3].format =
                            emissive_formats[hdr];
                        descriptor.depth_stencil_format =
                            ALMetalPixelFormat::depth32_float_stencil8;
                        descriptor.label =
                            "Alchemy world G-buffer pipeline";
                        auto& pipeline = impl->pipelines[hdr][bump][masked]
                                                         [rigged][normal_hdr];
                        pipeline = ALMetalRenderPipeline::create(
                            device, descriptor, error);
                        if (!pipeline)
                        {
                            return nullptr;
                        }
                    }
                }
            }
        }
    }

    ALMetalDepthStencilDescriptor depth;
    depth.depth_compare = ALMetalCompareFunction::less_equal;
    depth.depth_write_enabled = true;
    depth.label = "Alchemy forward-Z world G-buffer depth";
    impl->forward_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->forward_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater_equal;
    depth.label = "Alchemy reverse-Z world G-buffer depth";
    impl->reverse_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->reverse_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::less;
    depth.label = "Alchemy forward-Z spot-shadow depth";
    impl->forward_spot_shadow_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->forward_spot_shadow_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater;
    depth.label = "Alchemy reverse-Z spot-shadow depth";
    impl->reverse_spot_shadow_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->reverse_spot_shadow_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::less_equal;
    depth.depth_write_enabled = false;
    depth.label = "Alchemy forward-Z world read-only depth";
    impl->forward_readonly_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->forward_readonly_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater_equal;
    depth.label = "Alchemy reverse-Z world read-only depth";
    impl->reverse_readonly_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->reverse_readonly_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater;
    depth.depth_write_enabled = false;
    depth.label = "Alchemy forward-Z world coverage depth";
    impl->forward_coverage_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->forward_coverage_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::less;
    depth.label = "Alchemy reverse-Z world coverage depth";
    impl->reverse_coverage_depth = ALMetalDepthStencilState::create(
        device, depth, error);
    if (!impl->reverse_coverage_depth)
    {
        return nullptr;
    }

    depth.depth_compare = ALMetalCompareFunction::greater;
    depth.depth_write_enabled = false;
    depth.stencil_enabled = true;
    depth.front_stencil.compare = ALMetalCompareFunction::equal;
    depth.front_stencil.stencil_failure = ALMetalStencilOperation::keep;
    depth.front_stencil.depth_failure = ALMetalStencilOperation::keep;
    depth.front_stencil.depth_stencil_pass = ALMetalStencilOperation::keep;
    depth.front_stencil.read_mask = 2u;
    depth.front_stencil.write_mask = 0u;
    depth.back_stencil = depth.front_stencil;
    depth.label = "Alchemy forward-Z atmospheric haze water stencil";
    impl->forward_atmospheric_haze_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->forward_atmospheric_haze_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::less;
    depth.label = "Alchemy reverse-Z atmospheric haze water stencil";
    impl->reverse_atmospheric_haze_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->reverse_atmospheric_haze_depth)
    {
        return nullptr;
    }

    depth.depth_compare = ALMetalCompareFunction::less_equal;
    depth.depth_write_enabled = false;
    depth.stencil_enabled = true;
    depth.front_stencil.compare = ALMetalCompareFunction::always;
    depth.front_stencil.stencil_failure =
        ALMetalStencilOperation::keep;
    depth.front_stencil.depth_failure = ALMetalStencilOperation::keep;
    depth.front_stencil.depth_stencil_pass =
        ALMetalStencilOperation::replace;
    depth.front_stencil.read_mask = 0u;
    depth.front_stencil.write_mask = 1u;
    depth.back_stencil = depth.front_stencil;
    depth.label = "Alchemy forward-Z water-exclusion stencil";
    impl->forward_water_exclusion_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->forward_water_exclusion_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater_equal;
    depth.label = "Alchemy reverse-Z water-exclusion stencil";
    impl->reverse_water_exclusion_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->reverse_water_exclusion_depth)
    {
        return nullptr;
    }

    depth.depth_compare = ALMetalCompareFunction::less_equal;
    depth.depth_write_enabled = false;
    depth.stencil_enabled = true;
    depth.front_stencil.compare = ALMetalCompareFunction::equal;
    depth.front_stencil.stencil_failure = ALMetalStencilOperation::keep;
    depth.front_stencil.depth_failure = ALMetalStencilOperation::keep;
    depth.front_stencil.depth_stencil_pass = ALMetalStencilOperation::keep;
    depth.front_stencil.read_mask = 1u;
    depth.front_stencil.write_mask = 0u;
    depth.back_stencil = depth.front_stencil;
    depth.label = "Alchemy forward-Z above-water haze depth and stencil";
    impl->forward_water_haze_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->forward_water_haze_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater_equal;
    depth.label = "Alchemy reverse-Z above-water haze depth and stencil";
    impl->reverse_water_haze_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->reverse_water_haze_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::always;
    depth.stencil_enabled = false;
    depth.label = "Alchemy underwater water-haze depth";
    impl->underwater_water_haze_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->underwater_water_haze_depth)
    {
        return nullptr;
    }

    depth.depth_compare = ALMetalCompareFunction::less_equal;
    depth.depth_write_enabled = true;
    depth.stencil_enabled = true;
    depth.front_stencil.compare = ALMetalCompareFunction::equal;
    depth.front_stencil.stencil_failure = ALMetalStencilOperation::keep;
    depth.front_stencil.depth_failure = ALMetalStencilOperation::keep;
    depth.front_stencil.depth_stencil_pass =
        ALMetalStencilOperation::replace;
    depth.front_stencil.read_mask = 1u;
    depth.front_stencil.write_mask = 2u;
    depth.back_stencil = depth.front_stencil;
    depth.label = "Alchemy forward-Z underwater surface depth and stencil";
    impl->forward_underwater_surface_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->forward_underwater_surface_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater_equal;
    depth.label = "Alchemy reverse-Z underwater surface depth and stencil";
    impl->reverse_underwater_surface_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->reverse_underwater_surface_depth)
    {
        return nullptr;
    }

    depth.depth_compare = ALMetalCompareFunction::less_equal;
    depth.front_stencil.depth_stencil_pass = ALMetalStencilOperation::keep;
    depth.front_stencil.write_mask = 0u;
    depth.back_stencil = depth.front_stencil;
    depth.label = "Alchemy forward-Z excluded underwater surface depth";
    impl->forward_underwater_excluded_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->forward_underwater_excluded_depth)
    {
        return nullptr;
    }
    depth.depth_compare = ALMetalCompareFunction::greater_equal;
    depth.label = "Alchemy reverse-Z excluded underwater surface depth";
    impl->reverse_underwater_excluded_depth =
        ALMetalDepthStencilState::create(device, depth, error);
    if (!impl->reverse_underwater_excluded_depth)
    {
        return nullptr;
    }

    ALMetalSamplerDescriptor sampler;
    sampler.min_filter = ALMetalSamplerFilter::linear;
    sampler.mag_filter = ALMetalSamplerFilter::linear;
    sampler.mip_filter = ALMetalSamplerMipFilter::linear;
    sampler.address_s = ALMetalSamplerAddressMode::repeat;
    sampler.address_t = ALMetalSamplerAddressMode::repeat;
    sampler.address_r = ALMetalSamplerAddressMode::repeat;
    sampler.max_anisotropy = 16;
    sampler.label = "Alchemy world diffuse anisotropic wrap sampler";
    impl->diffuse_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->diffuse_sampler)
    {
        return nullptr;
    }

    sampler.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.label = "Alchemy world reflection-probe anisotropic clamp sampler";
    impl->probe_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->probe_sampler)
    {
        return nullptr;
    }

    sampler.min_filter = ALMetalSamplerFilter::nearest;
    sampler.mag_filter = ALMetalSamplerFilter::nearest;
    sampler.mip_filter = ALMetalSamplerMipFilter::none;
    sampler.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.max_anisotropy = 1;
    sampler.label = "Alchemy world terrain point clamp sampler";
    impl->point_clamp_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->point_clamp_sampler)
    {
        return nullptr;
    }

    sampler.address_s = ALMetalSamplerAddressMode::repeat;
    sampler.address_t = ALMetalSamplerAddressMode::repeat;
    sampler.address_r = ALMetalSamplerAddressMode::repeat;
    sampler.label = "Alchemy world point repeat sampler";
    impl->point_repeat_sampler = ALMetalSamplerState::create(
        device, sampler, error);
    if (!impl->point_repeat_sampler)
    {
        return nullptr;
    }

    sampler.address_s = ALMetalSamplerAddressMode::mirror_repeat;
    sampler.address_t = ALMetalSamplerAddressMode::mirror_repeat;
    sampler.address_r = ALMetalSamplerAddressMode::mirror_repeat;
    sampler.label = "Alchemy world point mirror sampler";
    impl->point_mirror_sampler = ALMetalSamplerState::create(
        device, sampler, error);
    if (!impl->point_mirror_sampler)
    {
        return nullptr;
    }

    sampler.min_filter = ALMetalSamplerFilter::linear;
    sampler.mag_filter = ALMetalSamplerFilter::linear;
    sampler.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.label = "Alchemy world terrain linear clamp sampler";
    impl->linear_clamp_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->linear_clamp_sampler)
    {
        return nullptr;
    }

    sampler.address_s = ALMetalSamplerAddressMode::mirror_repeat;
    sampler.address_t = ALMetalSamplerAddressMode::mirror_repeat;
    sampler.address_r = ALMetalSamplerAddressMode::mirror_repeat;
    sampler.label = "Alchemy world bloom linear mirror sampler";
    impl->linear_mirror_sampler = ALMetalSamplerState::create(
        device, sampler, error);
    if (!impl->linear_mirror_sampler)
    {
        return nullptr;
    }

    sampler.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_r = ALMetalSamplerAddressMode::clamp_to_edge;

    sampler.mip_filter = ALMetalSamplerMipFilter::linear;
    sampler.max_anisotropy = 16;
    sampler.label = "Alchemy world terrain ramp anisotropic clamp sampler";
    impl->ramp_sampler = ALMetalSamplerState::create(device, sampler, error);
    if (!impl->ramp_sampler)
    {
        return nullptr;
    }

    sampler.min_filter = ALMetalSamplerFilter::linear;
    sampler.mag_filter = ALMetalSamplerFilter::linear;
    sampler.mip_filter = ALMetalSamplerMipFilter::linear;
    sampler.address_s = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_t = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.address_r = ALMetalSamplerAddressMode::clamp_to_edge;
    sampler.max_anisotropy = 1;
    sampler.label = "Alchemy world HDRI trilinear clamp sampler";
    impl->hdri_sampler = ALMetalSamplerState::create(
        device, sampler, error);
    if (!impl->hdri_sampler)
    {
        return nullptr;
    }

    ALMetalTextureDescriptor smaa_lookup_descriptor;
    smaa_lookup_descriptor.format = ALMetalPixelFormat::rg8_unorm;
    smaa_lookup_descriptor.width = AREATEX_WIDTH;
    smaa_lookup_descriptor.height = AREATEX_HEIGHT;
    smaa_lookup_descriptor.storage = ALMetalStorageMode::private_gpu;
    smaa_lookup_descriptor.usage = ALMetalTextureUsage::shader_read;
    smaa_lookup_descriptor.label = "Alchemy SMAA area lookup";
    impl->smaa_area_texture = ALMetalTexture::create(
        device, smaa_lookup_descriptor, error);
    smaa_lookup_descriptor.format = ALMetalPixelFormat::r8_unorm;
    smaa_lookup_descriptor.width = SEARCHTEX_WIDTH;
    smaa_lookup_descriptor.height = SEARCHTEX_HEIGHT;
    smaa_lookup_descriptor.label = "Alchemy SMAA search lookup";
    impl->smaa_search_texture = ALMetalTexture::create(
        device, smaa_lookup_descriptor, error);
    auto smaa_lookup_upload =
        impl->smaa_area_texture && impl->smaa_search_texture
        ? device.makeCommandBuffer(
              "Alchemy native SMAA lookup upload", error)
        : nullptr;
    std::vector<std::uint8_t> area_lookup(AREATEX_SIZE);
    for (std::uint32_t row = 0; row < AREATEX_HEIGHT; ++row)
    {
        const std::uint32_t source_row = AREATEX_HEIGHT - 1u - row;
        std::memcpy(area_lookup.data() + row * AREATEX_PITCH,
                    areaTexBytes + source_row * AREATEX_PITCH,
                    AREATEX_PITCH);
    }
    std::vector<std::uint8_t> search_lookup(SEARCHTEX_SIZE);
    for (std::uint32_t row = 0; row < SEARCHTEX_HEIGHT; ++row)
    {
        const std::uint32_t source_row = SEARCHTEX_HEIGHT - 1u - row;
        std::memcpy(search_lookup.data() + row * SEARCHTEX_PITCH,
                    searchTexBytes + source_row * SEARCHTEX_PITCH,
                    SEARCHTEX_PITCH);
    }
    const bool smaa_lookup_ready = smaa_lookup_upload &&
        impl->smaa_area_texture->upload(
            *smaa_lookup_upload, 0, 0, {}, area_lookup.data(),
            AREATEX_PITCH, error) &&
        impl->smaa_search_texture->upload(
            *smaa_lookup_upload, 0, 0, {}, search_lookup.data(),
            SEARCHTEX_PITCH, error) &&
        smaa_lookup_upload->commitAndWait(error);
    if (!smaa_lookup_ready)
    {
        return nullptr;
    }

    ALMetalTextureDescriptor unshadowed_descriptor;
    unshadowed_descriptor.type = ALMetalTextureType::texture_2d_array;
    unshadowed_descriptor.format = ALMetalPixelFormat::r32_float;
    unshadowed_descriptor.width = 1;
    unshadowed_descriptor.height = 1;
    unshadowed_descriptor.array_length = 4;
    unshadowed_descriptor.storage = ALMetalStorageMode::private_gpu;
    unshadowed_descriptor.usage = ALMetalTextureUsage::shader_read;
    unshadowed_descriptor.label =
        "Alchemy world unshadowed directional fallback";
    impl->unshadowed_depth = ALMetalTexture::create(
        device, unshadowed_descriptor, error);
    auto unshadowed_upload = impl->unshadowed_depth
        ? device.makeCommandBuffer(
              "Alchemy world unshadowed directional fallback upload",
              error)
        : nullptr;
    const float fully_lit_depth = 1.f;
    bool unshadowed_ready = unshadowed_upload != nullptr;
    for (std::uint32_t cascade = 0;
         unshadowed_ready && cascade < 4; ++cascade)
    {
        unshadowed_ready = impl->unshadowed_depth->upload(
            *unshadowed_upload, 0, cascade, {}, &fully_lit_depth,
            sizeof(fully_lit_depth), error);
    }
    unshadowed_ready = unshadowed_ready &&
        unshadowed_upload->commitAndWait(error);
    if (!unshadowed_ready)
    {
        return nullptr;
    }
    impl->unshadowed_constants.resolution[0] = 1.f;
    impl->unshadowed_constants.resolution[1] = 1.f;
    impl->unshadowed_constants.resolution[2] = 1.f;
    impl->unshadowed_constants.resolution[3] = 1.f;
    impl->unshadowed_constants.parameters[2] = 1.f;
    impl->unshadowed_constants.config[0] = 4;
    impl->unshadowed_constants.config[1] = 0xfu;

    ALMetalTextureDescriptor hero_descriptor;
    hero_descriptor.type = ALMetalTextureType::texture_cube;
    hero_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
    hero_descriptor.width = 1;
    hero_descriptor.height = 1;
    hero_descriptor.storage = ALMetalStorageMode::private_gpu;
    hero_descriptor.usage = ALMetalTextureUsage::shader_read;
    hero_descriptor.label = "Alchemy world inactive hero-probe fallback";
    impl->inactive_hero_probe = ALMetalTexture::create(
        device, hero_descriptor, error);
    auto hero_upload = impl->inactive_hero_probe
        ? device.makeCommandBuffer(
              "Alchemy world inactive hero-probe fallback upload", error)
        : nullptr;
    const std::uint32_t black = 0;
    bool hero_ready = hero_upload != nullptr;
    for (std::uint32_t face = 0; hero_ready && face < 6; ++face)
    {
        hero_ready = impl->inactive_hero_probe->upload(
            *hero_upload, 0, face, {}, &black, sizeof(black), error);
    }
    hero_ready = hero_ready && hero_upload->commitAndWait(error);
    if (!hero_ready)
    {
        return nullptr;
    }

    ALMetalTextureDescriptor inactive_probe_descriptor;
    inactive_probe_descriptor.type =
        ALMetalTextureType::texture_cube_array;
    inactive_probe_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
    inactive_probe_descriptor.width = 1;
    inactive_probe_descriptor.height = 1;
    inactive_probe_descriptor.array_length = 1;
    inactive_probe_descriptor.storage = ALMetalStorageMode::private_gpu;
    inactive_probe_descriptor.usage = ALMetalTextureUsage::shader_read;
    inactive_probe_descriptor.label =
        "Alchemy world inactive reflection-probe radiance";
    impl->inactive_probe_radiance = ALMetalTexture::create(
        device, inactive_probe_descriptor, error);
    inactive_probe_descriptor.type = ALMetalTextureType::texture_2d;
    inactive_probe_descriptor.format = ALMetalPixelFormat::rgba32_uint;
    inactive_probe_descriptor.width = 4;
    inactive_probe_descriptor.height = 1;
    inactive_probe_descriptor.array_length = 1;
    inactive_probe_descriptor.label =
        "Alchemy world inactive reflection-probe irradiance";
    impl->inactive_probe_irradiance = ALMetalTexture::create(
        device, inactive_probe_descriptor, error);
    auto inactive_probe_upload =
        impl->inactive_probe_radiance && impl->inactive_probe_irradiance
        ? device.makeCommandBuffer(
              "Alchemy world inactive reflection-probe upload", error)
        : nullptr;
    std::array<std::uint32_t, 16> zero_irradiance{};
    bool inactive_probes_ready = inactive_probe_upload != nullptr;
    for (std::uint32_t face = 0;
         inactive_probes_ready && face < 6; ++face)
    {
        inactive_probes_ready = impl->inactive_probe_radiance->upload(
            *inactive_probe_upload, 0, face, {}, &black, sizeof(black),
            error);
    }
    inactive_probes_ready = inactive_probes_ready &&
        impl->inactive_probe_irradiance->upload(
            *inactive_probe_upload, 0, 0, {}, zero_irradiance.data(),
            4 * 4 * sizeof(std::uint32_t), error) &&
        inactive_probe_upload->commitAndWait(error);
    if (!inactive_probes_ready)
    {
        return nullptr;
    }

    ALMetalTextureDescriptor ssr_descriptor;
    ssr_descriptor.format = ALMetalPixelFormat::rgba8_unorm;
    ssr_descriptor.width = 1;
    ssr_descriptor.height = 1;
    ssr_descriptor.storage = ALMetalStorageMode::private_gpu;
    ssr_descriptor.usage = ALMetalTextureUsage::shader_read;
    ssr_descriptor.label = "Alchemy world inactive SSR color fallback";
    impl->inactive_ssr_color = ALMetalTexture::create(
        device, ssr_descriptor, error);
    ssr_descriptor.format = ALMetalPixelFormat::r32_float;
    ssr_descriptor.label = "Alchemy world inactive SSR depth fallback";
    impl->inactive_ssr_depth = ALMetalTexture::create(
        device, ssr_descriptor, error);
    auto ssr_upload = impl->inactive_ssr_color && impl->inactive_ssr_depth
        ? device.makeCommandBuffer(
              "Alchemy world inactive SSR fallback upload", error)
        : nullptr;
    const float far_depth = 1.f;
    const bool ssr_ready = ssr_upload &&
        impl->inactive_ssr_color->upload(
            *ssr_upload, 0, 0, {}, &black, sizeof(black), error) &&
        impl->inactive_ssr_depth->upload(
            *ssr_upload, 0, 0, {}, &far_depth, sizeof(far_depth), error) &&
        ssr_upload->commitAndWait(error);
    if (!ssr_ready)
    {
        return nullptr;
    }
    impl->inactive_ssr_constants.screen[0] = 1.f;
    impl->inactive_ssr_constants.screen[1] = 1.f;
    impl->inactive_ssr_constants.screen[2] = 1.f;
    impl->inactive_ssr_constants.sampling[0] = 1.f;

    ALMetalTextureDescriptor grade_descriptor;
    grade_descriptor.type = ALMetalTextureType::texture_2d_array;
    grade_descriptor.format = ALMetalPixelFormat::rgba16_unorm;
    grade_descriptor.width = 1;
    grade_descriptor.height = 1;
    grade_descriptor.array_length = 1;
    grade_descriptor.storage = ALMetalStorageMode::private_gpu;
    grade_descriptor.usage = ALMetalTextureUsage::shader_read;
    grade_descriptor.label = "Alchemy world inactive color-grade LUT";
    impl->identity_color_lut = ALMetalTexture::create(
        device, grade_descriptor, error);
    grade_descriptor.type = ALMetalTextureType::texture_2d;
    grade_descriptor.array_length = 1;
    grade_descriptor.label = "Alchemy world inactive tone curve";
    impl->identity_tone_curve = ALMetalTexture::create(
        device, grade_descriptor, error);
    ALMetalTextureDescriptor flare_state_texture_descriptor;
    flare_state_texture_descriptor.format =
        ALMetalPixelFormat::rgba16_float;
    flare_state_texture_descriptor.width = 2;
    flare_state_texture_descriptor.height = 1;
    flare_state_texture_descriptor.storage = ALMetalStorageMode::private_gpu;
    flare_state_texture_descriptor.usage =
        ALMetalTextureUsage::shader_read;
    flare_state_texture_descriptor.label =
        "Alchemy world inactive lens-flare state";
    impl->identity_lens_flare_state = ALMetalTexture::create(
        device, flare_state_texture_descriptor, error);
    ALMetalTextureDescriptor dirt_texture_descriptor;
    dirt_texture_descriptor.format = ALMetalPixelFormat::r8_unorm;
    dirt_texture_descriptor.width = 1;
    dirt_texture_descriptor.height = 1;
    dirt_texture_descriptor.storage = ALMetalStorageMode::private_gpu;
    dirt_texture_descriptor.usage = ALMetalTextureUsage::shader_read;
    dirt_texture_descriptor.label = "Alchemy world inactive lens dirt";
    impl->identity_lens_dirt = ALMetalTexture::create(
        device, dirt_texture_descriptor, error);
    auto grade_upload = impl->identity_color_lut &&
            impl->identity_tone_curve &&
            impl->identity_lens_flare_state &&
            impl->identity_lens_dirt
        ? device.makeCommandBuffer(
              "Alchemy world inactive color-grade texture upload", error)
        : nullptr;
    const std::array<std::uint16_t, 4> zero_grade{{0, 0, 0, 0xffffu}};
    const std::array<std::uint16_t, 8> zero_flare{};
    const std::array<std::uint8_t, 1> zero_dirt{};
    const bool grade_ready = grade_upload &&
        impl->identity_color_lut->upload(
            *grade_upload, 0, 0, {}, zero_grade.data(),
            sizeof(zero_grade), error) &&
        impl->identity_tone_curve->upload(
            *grade_upload, 0, 0, {}, zero_grade.data(),
            sizeof(zero_grade), error) &&
        impl->identity_lens_flare_state->upload(
            *grade_upload, 0, 0, {}, zero_flare.data(),
            sizeof(zero_flare), error) &&
        impl->identity_lens_dirt->upload(
            *grade_upload, 0, 0, {}, zero_dirt.data(),
            sizeof(zero_dirt), error) &&
        grade_upload->commitAndWait(error);
    if (!grade_ready)
    {
        return nullptr;
    }

    return std::unique_ptr<ALMetalWorldGBufferRenderer>(
        new ALMetalWorldGBufferRenderer(std::move(impl)));
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderSpotShadow(
    const ALMetalWorldSpotShadowScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || scene.width == 0 || scene.height == 0 ||
        (scene.batch_count != 0 && !scene.batches))
    {
        setError(error, "Metal spot-shadow scene is incomplete");
        return nullptr;
    }

    constexpr std::size_t float4_stride = sizeof(float) * 4;
    constexpr std::uint32_t max_skin_joints = 110;
    std::vector<std::unique_ptr<ALMetalBuffer>> palette_storage;
    std::vector<const ALMetalBuffer*> palette_buffers(
        scene.batch_count, nullptr);
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldSpotShadowBatch& batch = scene.batches[index];
        const bool valid_index_type =
            batch.index_type == ALMetalIndexType::uint16 ||
            batch.index_type == ALMetalIndexType::uint32;
        const std::size_t index_stride = batch.index_type ==
            ALMetalIndexType::uint32 ? sizeof(std::uint32_t)
                                     : sizeof(std::uint16_t);
        const std::uint32_t coverage =
            static_cast<std::uint32_t>(batch.coverage);
        const bool covered = batch.coverage !=
            ALMetalWorldSpotShadowCoverage::opaque;
        const bool pbr_coverage =
            batch.coverage == ALMetalWorldSpotShadowCoverage::pbr_mask ||
            batch.coverage == ALMetalWorldSpotShadowCoverage::pbr_blend;
        const bool avatar_coverage =
            batch.coverage == ALMetalWorldSpotShadowCoverage::avatar_mask ||
            batch.coverage == ALMetalWorldSpotShadowCoverage::avatar_blend;
        if (batch.terrain)
        {
            bool valid_factors = true;
            for (std::uint16_t factor :
                 batch.terrain_tessellation_factors)
            {
                valid_factors = valid_factors &&
                    (factor & 0x8000u) == 0 &&
                    (factor & 0x7fffu) != 0 &&
                    (factor & 0x7c00u) != 0x7c00u;
            }
            if (!batch.vertices || batch.indices ||
                batch.vertex_count != 4 || batch.index_count != 0 ||
                covered || batch.texture_count != 0 ||
                batch.position_offset % float4_stride != 0 ||
                !rangeFits(batch.position_offset, batch.vertex_count,
                           float4_stride, batch.vertices->length()) ||
                !finiteValues(batch.modelview_projection, 16) ||
                !sampleableTexture(batch.terrain_height_texture) ||
                batch.terrain_height_texture->descriptor().format !=
                    ALMetalPixelFormat::r32_float ||
                !std::isfinite(batch.terrain_grid_scale) ||
                batch.terrain_grid_scale <= 0.f || !valid_factors ||
                batch.skin_palette || batch.weight_offset != 0 ||
                batch.skin_palette_float_count != 0 ||
                batch.skin_joint_count != 0 || batch.double_sided ||
                batch.avatar || batch.avatar_cloth)
            {
                setError(error,
                         "Metal spot-shadow terrain batch is invalid");
                return nullptr;
            }
            continue;
        }
        if (!batch.vertices || !batch.indices || batch.vertex_count == 0 ||
            batch.index_count == 0 || !valid_index_type ||
            coverage > static_cast<std::uint32_t>(
                ALMetalWorldSpotShadowCoverage::avatar_blend) ||
            batch.position_offset % float4_stride != 0 ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error, "Metal spot-shadow batch geometry is invalid");
            return nullptr;
        }

        if (batch.avatar)
        {
            if ((covered && !avatar_coverage) ||
                (!covered && batch.coverage !=
                    ALMetalWorldSpotShadowCoverage::opaque) ||
                batch.texcoord_offset % (sizeof(float) * 2) != 0 ||
                !rangeFits(batch.texcoord_offset, batch.vertex_count,
                           sizeof(float) * 2, batch.vertices->length()) ||
                !finiteValues(batch.texture_matrix, 16) ||
                !std::isfinite(batch.avatar_draw_alpha) ||
                batch.avatar_draw_alpha < 0.f ||
                batch.avatar_draw_alpha > 1.f ||
                (covered &&
                 (batch.texture_count != 1 ||
                  !finiteValues(batch.alpha_cutoffs.data(), 1) ||
                  batch.alpha_cutoffs[0] < 0.f ||
                  batch.alpha_cutoffs[0] > 1.f ||
                  !sampleableTexture(batch.textures[0]))) ||
                (!covered && batch.texture_count != 0) ||
                (batch.avatar_cloth &&
                 (!batch.skin_palette ||
                  batch.normal_offset % float4_stride != 0 ||
                  batch.cloth_offset % float4_stride != 0 ||
                  !rangeFits(batch.normal_offset, batch.vertex_count,
                             float4_stride, batch.vertices->length()) ||
                  !rangeFits(batch.cloth_offset, batch.vertex_count,
                             float4_stride, batch.vertices->length()) ||
                  !finiteValues(batch.avatar_wind, 4) ||
                  !finiteValues(batch.avatar_sin_wave, 4) ||
                  !finiteValues(batch.avatar_gravity, 4) ||
                  !finiteValues(batch.avatar_cloth_depth, 4))))
            {
                setError(error,
                         "Metal spot-shadow avatar batch is invalid");
                return nullptr;
            }
        }
        else if (avatar_coverage || batch.avatar_cloth)
        {
            setError(error, "Metal spot-shadow avatar data is incomplete");
            return nullptr;
        }
        else if (covered)
        {
            if (batch.texture_count == 0 ||
                batch.texture_count >
                    ALMetalWorldSpotShadowBatch::max_textures ||
                (pbr_coverage && batch.texture_count >
                    ALMetalWorldSpotShadowBatch::max_pbr_slots) ||
                batch.texcoord_offset % (sizeof(float) * 2) != 0 ||
                batch.color_offset % sizeof(std::uint32_t) != 0 ||
                !rangeFits(batch.texcoord_offset, batch.vertex_count,
                           sizeof(float) * 2, batch.vertices->length()) ||
                !rangeFits(batch.color_offset, batch.vertex_count,
                           sizeof(std::uint32_t),
                           batch.vertices->length()) ||
                !finiteValues(batch.texture_matrix, 16) ||
                !finiteValues(batch.alpha_cutoffs.data(),
                              batch.texture_count) ||
                (pbr_coverage &&
                 !finiteValues(
                     &batch.pbr_base_color_transforms[0][0][0],
                     batch.texture_count * 8)))
            {
                setError(error,
                         "Metal spot-shadow coverage data is invalid");
                return nullptr;
            }
            for (std::size_t texture = 0;
                 texture < batch.texture_count; ++texture)
            {
                if (!sampleableTexture(batch.textures[texture]) ||
                    batch.alpha_cutoffs[texture] < 0.f ||
                    batch.alpha_cutoffs[texture] > 1.f)
                {
                    setError(error,
                             "Metal spot-shadow coverage texture is invalid");
                    return nullptr;
                }
            }
        }
        else if (batch.texture_count != 0)
        {
            setError(error,
                     "Metal opaque spot-shadow batch has coverage data");
            return nullptr;
        }

        if (batch.skin_palette)
        {
            const std::size_t expected_palette_floats = batch.avatar
                ? 45 * 4
                : 4 + static_cast<std::size_t>(
                      batch.skin_joint_count) * 12;
            const std::size_t weight_stride = batch.avatar
                ? sizeof(float) : float4_stride;
            const bool joint_count_valid = batch.avatar
                ? batch.skin_joint_count == 15
                : batch.skin_joint_count != 0 &&
                  batch.skin_joint_count <= max_skin_joints;
            const bool palette_size_valid =
                batch.skin_palette_float_count == expected_palette_floats;
            const bool weight_aligned =
                batch.weight_offset % weight_stride == 0;
            const bool weight_range_valid = rangeFits(
                batch.weight_offset, batch.vertex_count, weight_stride,
                batch.vertices->length());
            const bool palette_finite = finiteValues(
                batch.skin_palette, batch.skin_palette_float_count);
            std::size_t first_nonfinite =
                batch.skin_palette_float_count;
            if (!palette_finite)
            {
                for (std::size_t value = 0;
                     value < batch.skin_palette_float_count; ++value)
                {
                    if (!std::isfinite(batch.skin_palette[value]))
                    {
                        first_nonfinite = value;
                        break;
                    }
                }
            }
            if (!joint_count_valid || !palette_size_valid ||
                !weight_aligned || !weight_range_valid || !palette_finite)
            {
                setError(error,
                    "Metal spot-shadow skinning data is invalid (batch=" +
                    std::to_string(index) + ", avatar=" +
                    std::to_string(batch.avatar) + ", joints=" +
                    std::to_string(batch.skin_joint_count) + ", palette=" +
                    std::to_string(batch.skin_palette_float_count) + "/" +
                    std::to_string(expected_palette_floats) +
                    ", weight-offset=" +
                    std::to_string(batch.weight_offset) + ", stride=" +
                    std::to_string(weight_stride) + ", vertices=" +
                    std::to_string(batch.vertex_count) + ", vertex-bytes=" +
                    std::to_string(batch.vertices->length()) +
                    ", joint-count-valid=" +
                    std::to_string(joint_count_valid) +
                    ", palette-size-valid=" +
                    std::to_string(palette_size_valid) +
                    ", weight-aligned=" +
                    std::to_string(weight_aligned) + ", weight-range=" +
                    std::to_string(weight_range_valid) +
                    ", palette-finite=" +
                    std::to_string(palette_finite) +
                    ", first-nonfinite=" +
                    (first_nonfinite < batch.skin_palette_float_count
                         ? std::to_string(first_nonfinite) + ":" +
                           std::to_string(
                               batch.skin_palette[first_nonfinite])
                         : std::string("none")) + ")");
                return nullptr;
            }
            ALMetalBufferDescriptor descriptor;
            descriptor.length = batch.skin_palette_float_count *
                                sizeof(float);
            descriptor.storage = ALMetalStorageMode::shared;
            descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
            descriptor.label = "Alchemy spot-shadow skin palette";
            auto palette = ALMetalBuffer::create(
                *mImpl->device, descriptor, error);
            if (!palette || !palette->write(
                    0, batch.skin_palette, descriptor.length, error))
            {
                return nullptr;
            }
            palette_buffers[index] = palette.get();
            palette_storage.push_back(std::move(palette));
        }
        else if (batch.weight_offset != 0 ||
                 batch.skin_palette_float_count != 0 ||
                 batch.skin_joint_count != 0)
        {
            setError(error, "Metal spot-shadow skinning data is incomplete");
            return nullptr;
        }
    }

    std::unique_ptr<ALMetalBuffer> terrain_factor_buffer;
    std::vector<std::uint16_t> terrain_factors(scene.batch_count * 6, 0);
    bool has_terrain = false;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldSpotShadowBatch& batch = scene.batches[index];
        if (!batch.terrain)
        {
            continue;
        }
        has_terrain = true;
        std::memcpy(terrain_factors.data() + index * 6,
                    batch.terrain_tessellation_factors,
                    sizeof(batch.terrain_tessellation_factors));
    }
    if (has_terrain)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length = terrain_factors.size() *
                            sizeof(terrain_factors.front());
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label = "Alchemy spot-shadow terrain tessellation factors";
        terrain_factor_buffer = ALMetalBuffer::create(
            *mImpl->device, descriptor, error);
        if (!terrain_factor_buffer || !terrain_factor_buffer->write(
                0, terrain_factors.data(), descriptor.length, error))
        {
            return nullptr;
        }
    }

    ALMetalRenderTargetDescriptor target_descriptor;
    target_descriptor.width = scene.width;
    target_descriptor.height = scene.height;
    target_descriptor.color_attachment_count = 1;
    target_descriptor.colors[0].format = ALMetalPixelFormat::r32_float;
    target_descriptor.colors[0].label =
        "Alchemy direct spot-shadow raw depth";
    target_descriptor.depth_stencil_format =
        ALMetalPixelFormat::depth32_float;
    target_descriptor.label = "Alchemy direct spot-shadow slice";
    auto target = ALMetalRenderTarget::create(
        *mImpl->device, target_descriptor, error);
    if (!target)
    {
        return nullptr;
    }

    const double far_depth = scene.reverse_z ? 0.0 : 1.0;
    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {
        far_depth, far_depth, far_depth, far_depth};
    pass_config.depth_load = ALMetalLoadAction::clear;
    pass_config.depth_store = ALMetalStoreAction::dont_care;
    pass_config.clear_depth = far_depth;
    auto pass = target->makeRenderPass(pass_config, error);
    auto command = pass ? mImpl->device->makeCommandBuffer(
        "Alchemy direct spot-shadow slice", error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy direct spot-shadow encoder", error)
                           : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_spot_shadow_depth.get()
        : mImpl->forward_spot_shadow_depth.get();
    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0};
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    bool encoded =
        encoder->setDepthStencilState(depth, error) &&
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setDepthClipMode(
            scene.depth_clamp ? ALMetalDepthClipMode::clamp
                              : ALMetalDepthClipMode::clip,
            error) &&
        encoder->setFrontFacingWinding(ALMetalWinding::clockwise, error);
    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldSpotShadowBatch& batch = scene.batches[index];
        if (batch.terrain)
        {
            WorldTerrainSpotShadowConstants constants{};
            std::memcpy(constants.modelview_projection,
                        batch.modelview_projection,
                        sizeof(constants.modelview_projection));
            constants.surface[0] = batch.terrain_grid_scale;
            constants.config[0] = scene.reverse_z ? 1u : 0u;
            constants.config[1] = batch.terrain_smoothing ? 1u : 0u;
            encoded =
                encoder->setCullMode(ALMetalCullMode::back, error) &&
                encoder->setPipeline(
                    *mImpl->spot_shadow_terrain_pipelines[
                        batch.terrain_tessellation_pinned ? 1 : 0],
                    error) &&
                encoder->setVertexBuffer(
                    0, *batch.vertices, batch.position_offset, error) &&
                encoder->setVertexBytes(
                    1, &constants, sizeof(constants), error) &&
                encoder->setVertexTexture(
                    0, batch.terrain_height_texture, error) &&
                encoder->setTessellationFactorBuffer(
                    *terrain_factor_buffer,
                    index * sizeof(batch.terrain_tessellation_factors),
                    0, error) &&
                encoder->drawPatches(4, 0, 1, 1, 0, error);
            continue;
        }
        const bool rigged = batch.skin_palette != nullptr;
        const bool covered = batch.coverage !=
            ALMetalWorldSpotShadowCoverage::opaque;
        WorldSpotShadowConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.texture_matrix, batch.texture_matrix,
                    sizeof(constants.texture_matrix));
        std::memcpy(constants.pbr_base_color_transforms,
                    batch.pbr_base_color_transforms.data(),
                    sizeof(constants.pbr_base_color_transforms));
        std::memcpy(constants.alpha_cutoffs,
                    batch.alpha_cutoffs.data(),
                    sizeof(constants.alpha_cutoffs));
        std::memcpy(constants.avatar_wind, batch.avatar_wind,
                    sizeof(constants.avatar_wind));
        std::memcpy(constants.avatar_sin_wave, batch.avatar_sin_wave,
                    sizeof(constants.avatar_sin_wave));
        std::memcpy(constants.avatar_gravity, batch.avatar_gravity,
                    sizeof(constants.avatar_gravity));
        std::memcpy(constants.avatar_cloth_depth,
                    batch.avatar_cloth_depth,
                    sizeof(constants.avatar_cloth_depth));
        constants.avatar_draw[0] = batch.avatar_draw_alpha;
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] =
            static_cast<std::uint32_t>(batch.coverage);
        constants.config[2] = batch.texture_count;
        constants.config[3] = scene.height;
        if (batch.avatar)
        {
            const std::size_t variant = !rigged
                ? 0 : (batch.avatar_cloth ? 2 : 1);
            encoded =
                encoder->setCullMode(ALMetalCullMode::back, error) &&
                encoder->setPipeline(
                    *mImpl->spot_shadow_avatar_pipelines[
                        covered ? 1 : 0][variant], error) &&
                encoder->setVertexBuffer(
                    0, *batch.vertices, batch.position_offset, error) &&
                encoder->setVertexBytes(
                    1, &constants, sizeof(constants), error) &&
                encoder->setVertexBuffer(
                    2, *batch.vertices, batch.texcoord_offset, error);
            if (encoded && covered)
            {
                encoded =
                    encoder->setFragmentBytes(
                        0, &constants, sizeof(constants), error) &&
                    encoder->setFragmentSampler(
                        0, mImpl->diffuse_sampler.get(), error);
                for (std::size_t texture = 0;
                     encoded && texture <
                         ALMetalWorldSpotShadowBatch::max_textures;
                     ++texture)
                {
                    encoded = encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(texture),
                        batch.textures[0], error);
                }
            }
            if (encoded && rigged)
            {
                encoded =
                    encoder->setVertexBuffer(
                        4, *palette_buffers[index], 0, error) &&
                    encoder->setVertexBuffer(
                        5, *batch.vertices, batch.weight_offset, error);
            }
            if (encoded && batch.avatar_cloth)
            {
                encoded =
                    encoder->setVertexBuffer(
                        6, *batch.vertices, batch.normal_offset, error) &&
                    encoder->setVertexBuffer(
                        7, *batch.vertices, batch.cloth_offset, error);
            }
            encoded = encoded && encoder->drawIndexed(
                ALMetalPrimitiveType::triangle, batch.index_count,
                batch.index_type, *batch.indices, batch.index_offset,
                1, error);
            continue;
        }
        encoded =
            encoder->setCullMode(
                batch.double_sided ? ALMetalCullMode::none
                                   : ALMetalCullMode::back,
                error) &&
            encoder->setPipeline(
                *(covered
                    ? mImpl->spot_shadow_coverage_pipelines[
                          rigged ? 1 : 0]
                    : mImpl->spot_shadow_pipelines[rigged ? 1 : 0]),
                error) &&
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBytes(
                1, &constants, sizeof(constants), error);
        if (encoded && covered)
        {
            encoded =
                encoder->setVertexBuffer(
                    2, *batch.vertices, batch.texcoord_offset, error) &&
                encoder->setVertexBuffer(
                    3, *batch.vertices, batch.color_offset, error) &&
                encoder->setFragmentBytes(
                    0, &constants, sizeof(constants), error) &&
                encoder->setFragmentSampler(
                    0, mImpl->diffuse_sampler.get(), error);
            for (std::size_t texture = 0;
                 encoded && texture <
                     ALMetalWorldSpotShadowBatch::max_textures;
                 ++texture)
            {
                const std::size_t source = std::min<std::size_t>(
                    texture, batch.texture_count - 1);
                encoded = encoder->setFragmentTexture(
                    static_cast<std::uint32_t>(texture),
                    batch.textures[source], error);
            }
        }
        if (encoded && rigged && covered)
        {
            encoded =
                encoder->setVertexBuffer(
                    4, *palette_buffers[index], 0, error) &&
                encoder->setVertexBuffer(
                    5, *batch.vertices, batch.weight_offset, error) &&
                encoder->setVertexBytes(
                    6, &batch.skin_joint_count,
                    sizeof(batch.skin_joint_count), error);
        }
        else if (encoded && rigged)
        {
            encoded =
                encoder->setVertexBuffer(
                    2, *palette_buffers[index], 0, error) &&
                encoder->setVertexBuffer(
                    3, *batch.vertices, batch.weight_offset, error) &&
                encoder->setVertexBytes(
                    4, &batch.skin_joint_count,
                    sizeof(batch.skin_joint_count), error);
        }
        encoded = encoded && encoder->drawIndexed(
            ALMetalPrimitiveType::triangle, batch.index_count,
            batch.index_type, *batch.indices, batch.index_offset, 1, error);
    }
    if (!encoded || !encoder->end(error) || !command->commit(error))
    {
        return nullptr;
    }
    return target;
}

std::unique_ptr<ALMetalRenderTarget> ALMetalWorldGBufferRenderer::render(
    const ALMetalWorldGBufferScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches || scene.batch_count == 0 ||
        scene.width == 0 || scene.height == 0)
    {
        setError(error, "Metal world G-buffer scene is incomplete");
        return nullptr;
    }
    if (!finiteValues(scene.clear_color, 4) ||
        !finiteValues(scene.mirror_clip_plane, 4))
    {
        setError(error, "Metal world G-buffer clear/clip data is non-finite");
        return nullptr;
    }

    constexpr std::size_t float4_stride = sizeof(float) * 4;
    constexpr std::uint32_t max_skin_joints = 110;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        if (batch.impostor)
        {
            const ALMetalPixelFormat expected_normal = scene.normal_hdr
                ? ALMetalPixelFormat::rgba16_unorm
                : ALMetalPixelFormat::rgb10a2_unorm;
            const ALMetalPixelFormat expected_emissive = scene.hdr
                ? ALMetalPixelFormat::rg11b10_float
                : ALMetalPixelFormat::rgba8_unorm;
            if (batch.vertices || batch.indices || batch.vertex_count != 0 ||
                batch.index_count != 0 || batch.texture_count != 0 ||
                batch.normal_texture || batch.material_slot_count != 0 ||
                batch.pbr_slot_count != 0 || batch.skin_palette ||
                batch.avatar_rigid || batch.avatar_skinning ||
                batch.avatar_cloth || batch.impostor_fullbright ||
                batch.impostor_shiny ||
                batch.impostor_emboss ||
                batch.terrain ||
                !finiteValues(batch.modelview_projection, 16) ||
                !finiteValues(batch.impostor_center, 4) ||
                !finiteValues(batch.impostor_left, 4) ||
                !finiteValues(batch.impostor_up, 4) ||
                !finiteValues(&batch.impostor_normal_rotation[0][0], 12) ||
                !sampleableTexture(batch.impostor_textures[0]) ||
                !sampleableTexture(batch.impostor_textures[1]) ||
                !sampleableTexture(batch.impostor_textures[2]) ||
                !sampleableTexture(batch.impostor_textures[3]) ||
                batch.impostor_textures[0]->descriptor().format !=
                    ALMetalPixelFormat::rgba8_unorm_srgb ||
                batch.impostor_textures[1]->descriptor().format !=
                    ALMetalPixelFormat::rgba8_unorm ||
                batch.impostor_textures[2]->descriptor().format !=
                    expected_normal ||
                batch.impostor_textures[3]->descriptor().format !=
                    expected_emissive)
            {
                setError(error, "Metal world impostor batch is invalid");
                return nullptr;
            }
            continue;
        }
        if (batch.terrain)
        {
            const bool pbr_terrain = batch.terrain_pbr;
            const bool height_paint = batch.terrain_paint_type == 0;
            const std::size_t hex_value_count = pbr_terrain ? 32 : 8;
            bool valid_factors = true;
            for (std::uint16_t factor : batch.terrain_tessellation_factors)
            {
                valid_factors = valid_factors &&
                    (factor & 0x8000u) == 0 &&
                    (factor & 0x7fffu) != 0 &&
                    (factor & 0x7c00u) != 0x7c00u;
            }
            if (!batch.vertices || batch.indices || batch.vertex_count != 4 ||
                batch.index_count != 0 || batch.texture_count != 4 ||
                batch.normal_texture || batch.material_slot_count != 0 ||
                batch.pbr_slot_count != (pbr_terrain ? 4u : 0u) ||
                batch.skin_palette ||
                batch.avatar_rigid || batch.avatar_skinning ||
                batch.avatar_cloth || !valid_factors ||
                (!pbr_terrain &&
                 (batch.terrain_paint_texture || batch.alpha_mask ||
                  batch.terrain_flat_normals)) ||
                (pbr_terrain &&
                 (batch.terrain_paint_type > 1 ||
                  batch.terrain_pbr_detail < -4 ||
                  batch.terrain_pbr_detail > 0 ||
                  !finiteValues(batch.terrain_modelview, 16) ||
                  !finiteValues(&batch.terrain_uv_transform[0][0], 16) ||
                  !finiteValues(&batch.terrain_uv_offset[0][0], 8) ||
                  !finiteValues(&batch.terrain_normal_axes[0][0], 16))) ||
                batch.position_offset % float4_stride != 0 ||
                !rangeFits(batch.position_offset, batch.vertex_count,
                           float4_stride, batch.vertices->length()) ||
                !sampleableTexture(batch.terrain_height_texture) ||
                !sampleableTexture(batch.terrain_composition_texture) ||
                (!pbr_terrain &&
                 !sampleableTexture(batch.terrain_alpha_ramp_texture)) ||
                (pbr_terrain &&
                 (!sampleableTexture(
                      height_paint ? batch.terrain_alpha_ramp_texture
                                   : batch.terrain_paint_texture) ||
                  (height_paint && batch.terrain_paint_texture) ||
                  (!height_paint && batch.terrain_alpha_ramp_texture))) ||
                (batch.terrain_parcel_overlay_texture &&
                 !sampleableTexture(
                     batch.terrain_parcel_overlay_texture)) ||
                batch.terrain_height_texture->descriptor().format !=
                    ALMetalPixelFormat::r32_float ||
                batch.terrain_composition_texture->descriptor().format !=
                    ALMetalPixelFormat::rg16_float ||
                (batch.terrain_parcel_overlay_texture &&
                 batch.terrain_parcel_overlay_texture->descriptor().format !=
                     ALMetalPixelFormat::rgba8_unorm_srgb) ||
                batch.terrain_height_texture->descriptor().width !=
                    batch.terrain_composition_texture->descriptor().width ||
                batch.terrain_height_texture->descriptor().height !=
                    batch.terrain_composition_texture->descriptor().height ||
                !std::isfinite(batch.terrain_grid_scale) ||
                batch.terrain_grid_scale <= 0.f ||
                batch.impostor_fullbright || batch.impostor_shiny ||
                batch.impostor_emboss ||
                !std::isfinite(batch.terrain_region_scale) ||
                batch.terrain_region_scale <= 0.f ||
                !std::isfinite(batch.terrain_detail_scale) ||
                batch.terrain_detail_scale <= 0.f ||
                !finiteValues(batch.terrain_detail_phase, 2) ||
                !std::isfinite(batch.terrain_triplanar_blend) ||
                batch.terrain_triplanar_blend <= 0.f ||
                (batch.terrain_hex &&
                 (!finiteValues(&batch.terrain_hex_fraction[0][0],
                                hex_value_count) ||
                  std::any_of(
                      &batch.terrain_hex_fraction[0][0],
                      &batch.terrain_hex_fraction[0][0] + hex_value_count,
                      [](float value)
                      {
                          return value < 0.f || value >= 1.f;
                      }))) ||
                !finiteValues(batch.modelview_projection, 16) ||
                !finiteValues(&batch.normal_matrix[0][0], 12))
            {
                setError(error, "Metal world terrain batch is invalid");
                return nullptr;
            }
            for (std::uint32_t texture_index = 0;
                 texture_index < batch.texture_count; ++texture_index)
            {
                if (!sampleableTexture(
                        batch.diffuse_textures[texture_index]))
                {
                    setError(error,
                             "Metal world terrain detail texture is invalid");
                    return nullptr;
                }
            }
            if (pbr_terrain)
            {
                bool has_masked_material = false;
                for (std::uint32_t slot_index = 0;
                     slot_index < batch.pbr_slot_count; ++slot_index)
                {
                    const ALMetalWorldPBRSlot& slot =
                        batch.pbr_slots[slot_index];
                    has_masked_material = has_masked_material ||
                                          slot.alpha_cutoff >= 0.f;
                    if (!sampleableTexture(slot.normal_texture) ||
                        !sampleableTexture(slot.orm_texture) ||
                        !sampleableTexture(slot.emissive_texture) ||
                        !finiteValues(slot.base_color_factor, 4) ||
                        !finiteValues(slot.emissive_color, 4) ||
                        !std::isfinite(slot.roughness_factor) ||
                        !std::isfinite(slot.metallic_factor) ||
                        !std::isfinite(slot.alpha_cutoff) ||
                        slot.roughness_factor < 0.f ||
                        slot.roughness_factor > 1.f ||
                        slot.metallic_factor < 0.f ||
                        slot.metallic_factor > 1.f ||
                        slot.alpha_cutoff < -1.f ||
                        slot.alpha_cutoff > 1.f)
                    {
                        setError(error,
                                 "Metal world PBR terrain slot is invalid");
                        return nullptr;
                    }
                }
                if (has_masked_material != batch.alpha_mask)
                {
                    setError(error,
                             "Metal world PBR terrain mask state is invalid");
                    return nullptr;
                }
            }
            continue;
        }
        const bool bump = batch.normal_texture != nullptr;
        const bool material = batch.material_slot_count != 0;
        const bool pbr = isPBRBatch(batch);
        const bool avatar = batch.avatar_rigid || batch.avatar_skinning;
        const bool material_normal_map = materialHasNormalMap(batch);
        const bool material_specular_map = materialHasSpecularMap(batch);
        const bool tangent_required = bump || material_normal_map || pbr;
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices || batch.texture_count == 0 ||
            batch.impostor_fullbright || batch.impostor_shiny ||
            batch.impostor_emboss ||
            batch.texture_count >
                ALMetalWorldSimpleBatch::max_diffuse_textures ||
            (material &&
             (batch.material_slot_count >
                  ALMetalWorldSimpleBatch::max_material_slots ||
              batch.material_slot_count != batch.texture_count ||
              bump || pbr || batch.alpha_mask ||
              !validMaterialAlphaMode(batch.material_alpha_mode))) ||
            (pbr &&
             (batch.pbr_slot_count >
                  ALMetalWorldSimpleBatch::max_material_slots ||
              batch.pbr_slot_count != batch.texture_count ||
              material || bump)) ||
            (batch.avatar_cloth && !batch.avatar_skinning) ||
            (!batch.avatar_skinning && batch.cloth_offset != 0) ||
            (avatar &&
             (batch.avatar_rigid == batch.avatar_skinning ||
              (batch.avatar_rigid && batch.skin_palette != nullptr) ||
              batch.texture_count != 1 || !batch.alpha_mask ||
              batch.use_vertex_color || batch.alpha_cutoff_bias ||
              material || pbr || bump)) ||
            (!material && batch.material_alpha_mode != 0) ||
            batch.vertex_count == 0 || batch.index_count == 0 ||
            batch.index_count % 3 != 0 ||
            batch.position_offset % float4_stride != 0 ||
            batch.normal_offset % float4_stride != 0 ||
            batch.texcoord_offset % (sizeof(float) * 2) != 0 ||
            (!avatar &&
             batch.color_offset % (sizeof(std::uint8_t) * 4) != 0) ||
            (tangent_required &&
             batch.tangent_offset % float4_stride != 0) ||
            (material_normal_map &&
             batch.texcoord1_offset % (sizeof(float) * 2) != 0) ||
            (material_specular_map &&
             batch.texcoord2_offset % (sizeof(float) * 2) != 0) ||
            batch.index_offset % index_stride != 0 ||
            !std::isfinite(batch.alpha_cutoff) ||
            !finiteValues(batch.modelview_projection, 16) ||
            !finiteValues(&batch.normal_matrix[0][0], 12) ||
            !finiteValues(&batch.rigged_normal_matrix[0][0], 12) ||
            !finiteValues(batch.texture_matrix, 16) ||
            (batch.avatar_cloth &&
             (!finiteValues(batch.avatar_wind, 4) ||
              !finiteValues(batch.avatar_sin_wave, 4) ||
              !finiteValues(batch.avatar_gravity, 4) ||
              !finiteValues(batch.avatar_cloth_depth, 4))))
        {
            setError(error, "Metal world simple batch is invalid");
            return nullptr;
        }
        for (std::uint32_t slot = 0;
             slot < batch.material_slot_count; ++slot)
        {
            const ALMetalWorldMaterialSlot& material_slot =
                batch.material_slots[slot];
            if (!finiteValues(material_slot.specular_color, 4) ||
                !std::isfinite(material_slot.env_intensity) ||
                !std::isfinite(material_slot.alpha_cutoff) ||
                !std::isfinite(material_slot.fullbright) ||
                (material_normal_map !=
                 (material_slot.normal_texture != nullptr)) ||
                (material_specular_map !=
                 (material_slot.specular_texture != nullptr)) ||
                (material_normal_map &&
                 !sampleableTexture(material_slot.normal_texture)) ||
                (material_specular_map &&
                 !sampleableTexture(material_slot.specular_texture)))
            {
                setError(error, "Metal world material slot is invalid");
                return nullptr;
            }
        }
        for (std::uint32_t slot = 0; slot < batch.pbr_slot_count; ++slot)
        {
            const ALMetalWorldPBRSlot& pbr_slot = batch.pbr_slots[slot];
            if (!sampleableTexture(pbr_slot.normal_texture) ||
                !sampleableTexture(pbr_slot.orm_texture) ||
                !sampleableTexture(pbr_slot.emissive_texture) ||
                !finiteValues(&pbr_slot.texture_transform[0][0], 32) ||
                !finiteValues(pbr_slot.emissive_color, 4) ||
                !std::isfinite(pbr_slot.roughness_factor) ||
                !std::isfinite(pbr_slot.metallic_factor) ||
                !std::isfinite(pbr_slot.alpha_cutoff) ||
                pbr_slot.roughness_factor < 0.f ||
                pbr_slot.roughness_factor > 1.f ||
                pbr_slot.metallic_factor < 0.f ||
                pbr_slot.metallic_factor > 1.f ||
                pbr_slot.alpha_cutoff < -1.f ||
                pbr_slot.alpha_cutoff > 1.f)
            {
                setError(error, "Metal world PBR slot is invalid");
                return nullptr;
            }
        }
        if (!rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.normal_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       sizeof(float) * 2, batch.vertices->length()) ||
            (!avatar &&
             !rangeFits(batch.color_offset, batch.vertex_count,
                        sizeof(std::uint8_t) * 4,
                        batch.vertices->length())) ||
            (tangent_required &&
             !rangeFits(batch.tangent_offset, batch.vertex_count,
                        float4_stride, batch.vertices->length())) ||
            (material_normal_map &&
             !rangeFits(batch.texcoord1_offset, batch.vertex_count,
                        sizeof(float) * 2, batch.vertices->length())) ||
            (material_specular_map &&
             !rangeFits(batch.texcoord2_offset, batch.vertex_count,
                        sizeof(float) * 2, batch.vertices->length())) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error,
                     "Metal world simple geometry range exceeds its buffers");
            return nullptr;
        }
        if (batch.avatar_skinning)
        {
            constexpr std::size_t avatar_palette_floats = 45 * 4;
            if (!batch.skin_palette || batch.skin_joint_count != 15 ||
                batch.skin_palette_float_count != avatar_palette_floats ||
                batch.weight_offset % sizeof(float) != 0 ||
                !rangeFits(batch.weight_offset, batch.vertex_count,
                           sizeof(float), batch.vertices->length()) ||
                !finiteValues(batch.skin_palette,
                              avatar_palette_floats) ||
                (batch.avatar_cloth &&
                 (batch.cloth_offset % float4_stride != 0 ||
                  !rangeFits(batch.cloth_offset, batch.vertex_count,
                             float4_stride,
                             batch.vertices->length()))))
            {
                setError(error,
                         "Metal world avatar skinning data is invalid");
                return nullptr;
            }
        }
        else if (batch.skin_palette)
        {
            const std::size_t expected_palette_floats =
                4 + static_cast<std::size_t>(batch.skin_joint_count) * 12;
            if (batch.skin_joint_count == 0 ||
                batch.skin_joint_count > max_skin_joints ||
                batch.skin_palette_float_count != expected_palette_floats ||
                batch.weight_offset % float4_stride != 0 ||
                !rangeFits(batch.weight_offset, batch.vertex_count,
                           float4_stride, batch.vertices->length()) ||
                !finiteValues(batch.skin_palette,
                              expected_palette_floats))
            {
                setError(error,
                         "Metal world simple skinning data is invalid");
                return nullptr;
            }
        }
        else if (batch.weight_offset != 0 || batch.cloth_offset != 0 ||
                 batch.skin_palette_float_count != 0 ||
                 batch.skin_joint_count != 0)
        {
            setError(error,
                     "Metal world simple skinning data is incomplete");
            return nullptr;
        }
        for (std::uint32_t texture_index = 0;
             texture_index < batch.texture_count; ++texture_index)
        {
            const ALMetalTexture* texture =
                batch.diffuse_textures[texture_index];
            if (!texture || texture->descriptor().type !=
                    ALMetalTextureType::texture_2d ||
                !alMetalHasUsage(texture->descriptor().usage,
                                 ALMetalTextureUsage::shader_read))
            {
                setError(error,
                         "Metal world diffuse texture is not sampleable 2D");
                return nullptr;
            }
        }
        if (bump &&
            (batch.normal_texture->descriptor().type !=
                 ALMetalTextureType::texture_2d ||
             !alMetalHasUsage(batch.normal_texture->descriptor().usage,
                              ALMetalTextureUsage::shader_read)))
        {
            setError(error,
                     "Metal world normal texture is not sampleable 2D");
            return nullptr;
        }
    }

    std::unique_ptr<ALMetalBuffer> terrain_factor_buffer;
    std::vector<std::uint16_t> terrain_factors(scene.batch_count * 6, 0);
    bool has_terrain = false;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        if (!batch.terrain)
        {
            continue;
        }
        has_terrain = true;
        std::memcpy(terrain_factors.data() + index * 6,
                    batch.terrain_tessellation_factors,
                    sizeof(batch.terrain_tessellation_factors));
    }
    if (has_terrain)
    {
        ALMetalBufferDescriptor factor_descriptor;
        factor_descriptor.length = terrain_factors.size() *
                                   sizeof(terrain_factors.front());
        factor_descriptor.storage = ALMetalStorageMode::shared;
        factor_descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        factor_descriptor.label =
            "Alchemy world terrain tessellation factors";
        terrain_factor_buffer = ALMetalBuffer::create(
            *mImpl->device, factor_descriptor, error);
        if (!terrain_factor_buffer || !terrain_factor_buffer->write(
                0, terrain_factors.data(), factor_descriptor.length, error))
        {
            return nullptr;
        }
    }

    std::vector<std::unique_ptr<ALMetalBuffer>> palette_storage;
    std::vector<const ALMetalBuffer*> palette_buffers(
        scene.batch_count, nullptr);
    for (std::size_t batch_index = 0;
         batch_index < scene.batch_count; ++batch_index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[batch_index];
        if (batch.terrain || !batch.skin_palette)
        {
            continue;
        }
        for (std::size_t previous = 0; previous < batch_index; ++previous)
        {
            const ALMetalWorldSimpleBatch& candidate = scene.batches[previous];
            if (candidate.skin_palette == batch.skin_palette &&
                candidate.skin_palette_float_count ==
                    batch.skin_palette_float_count &&
                candidate.skin_joint_count == batch.skin_joint_count)
            {
                palette_buffers[batch_index] = palette_buffers[previous];
                break;
            }
        }
        if (palette_buffers[batch_index])
        {
            continue;
        }
        ALMetalBufferDescriptor palette_descriptor;
        palette_descriptor.length =
            batch.skin_palette_float_count * sizeof(float);
        palette_descriptor.storage = ALMetalStorageMode::shared;
        palette_descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        palette_descriptor.label = "Alchemy world skin palette";
        auto palette = ALMetalBuffer::create(
            *mImpl->device, palette_descriptor, error);
        if (!palette || !palette->write(
                0, batch.skin_palette, palette_descriptor.length, error))
        {
            return nullptr;
        }
        palette_buffers[batch_index] = palette.get();
        palette_storage.push_back(std::move(palette));
    }

    ALMetalRenderTargetDescriptor target_descriptor;
    target_descriptor.width = scene.width;
    target_descriptor.height = scene.height;
    target_descriptor.color_attachment_count = 4;
    target_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm_srgb;
    target_descriptor.colors[0].label = "Alchemy world G-buffer albedo";
    target_descriptor.colors[1].format = ALMetalPixelFormat::rgba8_unorm;
    target_descriptor.colors[1].label = "Alchemy world G-buffer specular ORM";
    target_descriptor.colors[2].format = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    target_descriptor.colors[2].label = "Alchemy world G-buffer normal";
    target_descriptor.colors[3].format = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    target_descriptor.colors[3].label = "Alchemy world G-buffer emissive";
    target_descriptor.depth_stencil_format =
        ALMetalPixelFormat::depth32_float_stencil8;
    target_descriptor.depth_stencil_shader_read = true;
    target_descriptor.label = "Alchemy native world G-buffer bootstrap";
    auto target = ALMetalRenderTarget::create(
        *mImpl->device, target_descriptor, error);
    if (!target)
    {
        return nullptr;
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::clear;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
        pass_config.colors[attachment].clear = {
            scene.clear_color[0], scene.clear_color[1],
            scene.clear_color[2], scene.clear_color[3],
        };
    }
    pass_config.depth_load = ALMetalLoadAction::clear;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.clear_depth = scene.reverse_z ? 0.0 : 1.0;
    pass_config.stencil_load = ALMetalLoadAction::clear;
    pass_config.stencil_store = ALMetalStoreAction::store;
    pass_config.clear_stencil = 0;
    auto pass = target->makeRenderPass(pass_config, error);
    if (!pass)
    {
        return nullptr;
    }
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world G-buffer bootstrap", owned_command, error);
    if (!command)
    {
        return nullptr;
    }
    auto encoder = ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world simple G-buffer encoder", error);
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_depth.get() : mImpl->forward_depth.get();
    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    bool encoded =
        encoder->setDepthStencilState(depth, error) &&
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::back, error) &&
        encoder->setFrontFacingWinding(ALMetalWinding::clockwise,
                                       error);

    for (std::size_t index = 0; encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        if (batch.impostor)
        {
            WorldImpostorConstants constants{};
            std::memcpy(constants.modelview_projection,
                        batch.modelview_projection,
                        sizeof(constants.modelview_projection));
            std::memcpy(constants.modelview, batch.modelview,
                        sizeof(constants.modelview));
            std::memcpy(constants.center, batch.impostor_center,
                        sizeof(constants.center));
            std::memcpy(constants.left, batch.impostor_left,
                        sizeof(constants.left));
            std::memcpy(constants.up, batch.impostor_up,
                        sizeof(constants.up));
            std::memcpy(constants.normal_rotation,
                        batch.impostor_normal_rotation,
                        sizeof(constants.normal_rotation));
            constants.depth_convention[0] = scene.reverse_z ? 1u : 0u;
            constants.depth_convention[1] = scene.mirror_clip ? 1u : 0u;
            std::memcpy(constants.mirror_clip_plane,
                        scene.mirror_clip_plane,
                        sizeof(constants.mirror_clip_plane));
            const ALMetalRenderPipeline* pipeline =
                mImpl->impostor_pipelines[scene.hdr ? 1 : 0]
                                         [scene.normal_hdr ? 1 : 0].get();
            encoded =
                // A camera-facing impostor is intrinsically two-sided. The
                // GL path inherits culling state, but its generated winding
                // changes sign with the viewer basis; accepting both avoids
                // dropping the billboard at those basis transitions.
                encoder->setCullMode(ALMetalCullMode::none, error) &&
                encoder->setPipeline(*pipeline, error) &&
                encoder->setVertexBytes(
                    0, &constants, sizeof(constants), error) &&
                encoder->setFragmentBytes(
                    0, &constants, sizeof(constants), error) &&
                encoder->setFragmentTextureLinear(
                    0, batch.impostor_textures[0], error) &&
                encoder->setFragmentTexture(
                    1, batch.impostor_textures[1], error) &&
                encoder->setFragmentTexture(
                    2, batch.impostor_textures[2], error) &&
                encoder->setFragmentTexture(
                    3, batch.impostor_textures[3], error) &&
                encoder->setFragmentSampler(
                    0, mImpl->point_clamp_sampler.get(), error) &&
                encoder->draw(
                    ALMetalPrimitiveType::triangle, 0, 6, 1, error);
            continue;
        }
        if (batch.terrain)
        {
            WorldTerrainConstants constants{};
            std::memcpy(constants.modelview_projection,
                        batch.modelview_projection,
                        sizeof(constants.modelview_projection));
            std::memcpy(constants.modelview, batch.terrain_modelview,
                        sizeof(constants.modelview));
            std::memcpy(constants.normal_matrix,
                        batch.normal_matrix,
                        sizeof(constants.normal_matrix));
            std::memcpy(constants.mirror_clip_plane,
                        scene.mirror_clip_plane,
                        sizeof(constants.mirror_clip_plane));
            constants.surface[0] = batch.terrain_grid_scale;
            constants.surface[1] = batch.terrain_region_scale;
            constants.surface[2] = batch.terrain_detail_scale;
            constants.surface[3] = batch.terrain_triplanar_blend;
            constants.detail[0] = batch.terrain_detail_phase[0];
            constants.detail[1] = batch.terrain_detail_phase[1];
            constants.detail[2] = batch.terrain_smoothing ? 1.f : 0.f;
            constants.detail[3] = batch.terrain_triplanar ? 1.f : 0.f;
            constants.config[0] = scene.reverse_z ? 1u : 0u;
            constants.config[1] =
                batch.terrain_parcel_overlay_texture ? 1u : 0u;
            constants.config[2] = batch.terrain_hex ? 1u : 0u;
            constants.config[3] = scene.normal_hdr ? 1u : 0u;
            constants.pbr_config[0] = static_cast<std::uint32_t>(
                batch.terrain_pbr_detail + 4);
            constants.pbr_config[1] = batch.terrain_paint_type;
            constants.pbr_config[2] = batch.alpha_mask ? 1u : 0u;
            constants.pbr_config[3] =
                batch.terrain_flat_normals ? 1u : 0u;
            std::memcpy(constants.hex_fraction,
                        batch.terrain_hex_fraction,
                        sizeof(constants.hex_fraction));
            std::memcpy(constants.hex_base,
                        batch.terrain_hex_base,
                        sizeof(constants.hex_base));
            for (std::size_t slot = 0;
                 slot < batch.pbr_slot_count; ++slot)
            {
                const ALMetalWorldPBRSlot& source = batch.pbr_slots[slot];
                WorldTerrainConstants::PBRSlot& destination =
                    constants.pbr_slots[slot];
                std::memcpy(destination.uv_transform,
                            batch.terrain_uv_transform[slot],
                            sizeof(destination.uv_transform));
                std::memcpy(destination.uv_offset,
                            batch.terrain_uv_offset[slot],
                            sizeof(batch.terrain_uv_offset[slot]));
                std::memcpy(destination.normal_axes,
                            batch.terrain_normal_axes[slot],
                            sizeof(destination.normal_axes));
                std::memcpy(destination.base_color_factor,
                            source.base_color_factor,
                            sizeof(destination.base_color_factor));
                std::memcpy(destination.emissive_color,
                            source.emissive_color,
                            sizeof(destination.emissive_color));
                destination.properties[0] = source.roughness_factor;
                destination.properties[1] = source.metallic_factor;
                destination.properties[2] = source.alpha_cutoff;
                destination.properties[3] = source.base_color_factor[3];
            }
            const auto& terrain_pipelines = batch.terrain_pbr
                ? mImpl->pbr_terrain_pipelines
                : mImpl->terrain_pipelines;
            const ALMetalRenderPipeline* pipeline =
                terrain_pipelines[scene.hdr ? 1 : 0]
                    [batch.terrain_tessellation_pinned ? 1 : 0]
                    [scene.normal_hdr ? 1 : 0].get();

            encoded =
                encoder->setCullMode(ALMetalCullMode::back, error) &&
                encoder->setPipeline(*pipeline, error) &&
                encoder->setVertexBuffer(
                    0, *batch.vertices, batch.position_offset, error) &&
                encoder->setVertexBytes(
                    4, &constants, sizeof(constants), error) &&
                encoder->setFragmentBytes(
                    0, &constants, sizeof(constants), error) &&
                encoder->setVertexTexture(
                    0, batch.terrain_height_texture, error) &&
                encoder->setVertexTexture(
                    1, batch.terrain_composition_texture, error) &&
                encoder->setVertexSampler(
                    2, mImpl->linear_clamp_sampler.get(), error) &&
                encoder->setFragmentSampler(
                    0, mImpl->diffuse_sampler.get(), error) &&
                encoder->setFragmentSampler(
                    1, mImpl->point_clamp_sampler.get(), error) &&
                encoder->setFragmentSampler(
                    3, mImpl->ramp_sampler.get(), error);
            for (std::uint32_t texture = 0;
                 encoded && texture < batch.texture_count; ++texture)
            {
                encoded = encoder->setFragmentTexture(
                    texture, batch.diffuse_textures[texture], error);
            }
            if (encoded && batch.terrain_pbr)
            {
                const ALMetalTexture* blend = batch.terrain_paint_type == 0
                    ? batch.terrain_alpha_ramp_texture
                    : batch.terrain_paint_texture;
                for (std::uint32_t slot = 0;
                     encoded && slot < batch.pbr_slot_count; ++slot)
                {
                    const ALMetalWorldPBRSlot& pbr_slot =
                        batch.pbr_slots[slot];
                    encoded =
                        encoder->setFragmentTextureLinear(
                            4 + slot, pbr_slot.normal_texture, error) &&
                        encoder->setFragmentTextureLinear(
                            8 + slot, pbr_slot.orm_texture, error) &&
                        encoder->setFragmentTexture(
                            12 + slot, pbr_slot.emissive_texture, error);
                }
                encoded = encoded &&
                    encoder->setFragmentTexture(
                        16, batch.terrain_height_texture, error) &&
                    encoder->setFragmentTexture(17, blend, error) &&
                    encoder->setFragmentTexture(
                        18, batch.terrain_parcel_overlay_texture
                            ? batch.terrain_parcel_overlay_texture : blend,
                        error);
            }
            else if (encoded)
            {
                encoded =
                    encoder->setFragmentTexture(
                        4, batch.terrain_height_texture, error) &&
                    encoder->setFragmentTexture(
                        5, batch.terrain_alpha_ramp_texture, error) &&
                    encoder->setFragmentTexture(
                        6, batch.terrain_parcel_overlay_texture
                            ? batch.terrain_parcel_overlay_texture
                            : batch.terrain_alpha_ramp_texture,
                        error);
            }
            encoded = encoded &&
                encoder->setTessellationFactorBuffer(
                    *terrain_factor_buffer,
                    index * sizeof(batch.terrain_tessellation_factors),
                    0, error) &&
                encoder->drawPatches(4, 0, 1, 1, 0, error);
            continue;
        }
        WorldSimpleConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.modelview, batch.modelview,
                    sizeof(constants.modelview));
        std::memcpy(constants.normal_matrix, batch.normal_matrix,
                    sizeof(constants.normal_matrix));
        std::memcpy(constants.rigged_normal_matrix,
                    batch.rigged_normal_matrix,
                    sizeof(constants.rigged_normal_matrix));
        std::memcpy(constants.texture_matrix, batch.texture_matrix,
                    sizeof(constants.texture_matrix));
        std::memcpy(constants.avatar_wind, batch.avatar_wind,
                    sizeof(constants.avatar_wind));
        std::memcpy(constants.avatar_sin_wave, batch.avatar_sin_wave,
                    sizeof(constants.avatar_sin_wave));
        std::memcpy(constants.avatar_gravity, batch.avatar_gravity,
                    sizeof(constants.avatar_gravity));
        std::memcpy(constants.avatar_cloth_depth,
                    batch.avatar_cloth_depth,
                    sizeof(constants.avatar_cloth_depth));
        std::memcpy(constants.mirror_clip_plane,
                    scene.mirror_clip_plane,
                    sizeof(constants.mirror_clip_plane));
        constants.material[0] = batch.alpha_cutoff;
        constants.material[1] = batch.use_vertex_color ? 1.f : 0.f;
        constants.material[2] = batch.alpha_cutoff_bias ? 1.f : 0.f;
        constants.material[3] = batch.alpha_mask ? 1.f : 0.f;
        constants.depth_convention[0] = scene.reverse_z ? 1u : 0u;
        constants.depth_convention[1] = batch.texture_count;
        constants.depth_convention[2] = scene.mirror_clip ? 1u : 0u;
        const bool avatar = batch.avatar_rigid || batch.avatar_skinning;
        const bool rigged = batch.skin_palette != nullptr && !avatar;
        const bool bump = batch.normal_texture != nullptr;
        const bool material = batch.material_slot_count != 0;
        const bool pbr = isPBRBatch(batch);
        const bool material_normal_map = materialHasNormalMap(batch);
        const bool material_specular_map = materialHasSpecularMap(batch);
        const std::size_t avatar_variant = batch.avatar_rigid
            ? 0 : (batch.avatar_cloth ? 2 : 1);
        const ALMetalRenderPipeline* pipeline = avatar
            ? mImpl->avatar_pipelines[scene.hdr ? 1 : 0]
                                     [avatar_variant]
                                     [scene.normal_hdr ? 1 : 0].get()
            : (pbr
                ? mImpl->pbr_pipelines[scene.hdr ? 1 : 0]
                                      [rigged ? 1 : 0]
                                      [scene.normal_hdr ? 1 : 0].get()
                : (material
                    ? mImpl->material_pipelines[scene.hdr ? 1 : 0]
                                               [rigged ? 1 : 0]
                                               [scene.normal_hdr ? 1 : 0].get()
                    : mImpl->pipelines[scene.hdr ? 1 : 0]
                                      [bump ? 1 : 0]
                                      [batch.alpha_mask ? 1 : 0]
                                      [rigged ? 1 : 0]
                                      [scene.normal_hdr ? 1 : 0].get()));

        encoded =
            encoder->setCullMode(
                pbr && batch.double_sided
                    ? ALMetalCullMode::none : ALMetalCullMode::back,
                error) &&
            encoder->setPipeline(*pipeline, error) &&
            encoder->setVertexBuffer(0, *batch.vertices,
                                     batch.position_offset, error) &&
            encoder->setVertexBuffer(1, *batch.vertices,
                                     batch.normal_offset, error) &&
            encoder->setVertexBuffer(2, *batch.vertices,
                                     batch.texcoord_offset, error) &&
            encoder->setVertexBytes(4, &constants, sizeof(constants), error) &&
            encoder->setFragmentSampler(0, mImpl->diffuse_sampler.get(), error);
        if (encoded && !avatar)
        {
            encoded = encoder->setVertexBuffer(
                3, *batch.vertices, batch.color_offset, error);
        }
        if (encoded && batch.skin_palette)
        {
            encoded =
                encoder->setVertexBuffer(
                    5, *palette_buffers[index], 0, error) &&
                encoder->setVertexBuffer(
                    6, *batch.vertices, batch.weight_offset, error) &&
                encoder->setVertexBytes(
                    7, &batch.skin_joint_count,
                    sizeof(batch.skin_joint_count), error);
        }
        if (encoded && batch.avatar_cloth)
        {
            encoded = encoder->setVertexBuffer(
                9, *batch.vertices, batch.cloth_offset, error);
        }
        if (encoded && bump)
        {
            encoded =
                encoder->setVertexBuffer(8, *batch.vertices,
                                         batch.tangent_offset, error) &&
                encoder->setFragmentTexture(
                    32, batch.normal_texture, error);
        }
        if (encoded && material)
        {
            encoded =
                encoder->setVertexBuffer(
                    8, *batch.vertices,
                    material_normal_map
                        ? batch.tangent_offset : batch.normal_offset,
                    error) &&
                encoder->setVertexBuffer(
                    9, *batch.vertices,
                    material_normal_map
                        ? batch.texcoord1_offset : batch.texcoord_offset,
                    error) &&
                encoder->setVertexBuffer(
                    10, *batch.vertices,
                    material_specular_map
                        ? batch.texcoord2_offset : batch.texcoord_offset,
                    error);
            WorldMaterialConstants material_constants{};
            for (std::size_t slot = 0;
                 slot < batch.material_slot_count; ++slot)
            {
                const ALMetalWorldMaterialSlot& source =
                    batch.material_slots[slot];
                WorldMaterialSlotConstants& destination =
                    material_constants.slots[slot];
                std::memcpy(destination.specular_color,
                            source.specular_color,
                            sizeof(destination.specular_color));
                destination.properties[0] = source.env_intensity;
                destination.properties[1] = source.alpha_cutoff;
                destination.properties[2] = source.fullbright;
            }
            material_constants.config[0] = batch.material_slot_count;
            material_constants.config[1] = batch.material_alpha_mode;
            material_constants.config[2] = material_normal_map ? 1u : 0u;
            material_constants.config[3] = material_specular_map ? 1u : 0u;
            encoded = encoded && encoder->setFragmentBytes(
                0, &material_constants, sizeof(material_constants), error);
        }
        if (encoded && pbr)
        {
            encoded = encoder->setVertexBuffer(
                8, *batch.vertices, batch.tangent_offset, error);
            WorldPBRConstants pbr_constants{};
            for (std::size_t slot = 0; slot < batch.pbr_slot_count; ++slot)
            {
                const ALMetalWorldPBRSlot& source = batch.pbr_slots[slot];
                WorldPBRSlotConstants& destination =
                    pbr_constants.slots[slot];
                std::memcpy(destination.texture_transform,
                            source.texture_transform,
                            sizeof(destination.texture_transform));
                std::memcpy(destination.emissive_color,
                            source.emissive_color,
                            sizeof(destination.emissive_color));
                destination.properties[0] = source.roughness_factor;
                destination.properties[1] = source.metallic_factor;
                destination.properties[2] = source.alpha_cutoff;
            }
            pbr_constants.config[0] = batch.pbr_slot_count;
            pbr_constants.config[1] = batch.alpha_mask ? 1u : 0u;
            pbr_constants.config[2] = scene.normal_hdr ? 1u : 0u;
            std::memcpy(pbr_constants.modelview, batch.modelview,
                        sizeof(pbr_constants.modelview));
            encoded = encoded &&
                encoder->setVertexBytes(
                    11, &pbr_constants, sizeof(pbr_constants), error) &&
                encoder->setFragmentBytes(
                    0, &pbr_constants, sizeof(pbr_constants), error);
        }
        for (std::size_t texture_index = 0;
             encoded && texture_index <
                 ALMetalWorldSimpleBatch::max_diffuse_textures;
             ++texture_index)
        {
            const ALMetalTexture* texture = texture_index < batch.texture_count
                ? batch.diffuse_textures[texture_index]
                : batch.diffuse_textures[0];
            encoded = encoder->setFragmentTexture(
                static_cast<std::uint32_t>(texture_index), texture, error);
        }
        if (encoded && material)
        {
            for (std::size_t slot = 0;
                 encoded && slot < ALMetalWorldSimpleBatch::max_material_slots;
                 ++slot)
            {
                const std::size_t source = std::min<std::size_t>(
                    slot, batch.material_slot_count - 1);
                const ALMetalTexture* normal_texture = material_normal_map
                    ? batch.material_slots[source].normal_texture
                    : batch.diffuse_textures[0];
                const ALMetalTexture* specular_texture = material_specular_map
                    ? batch.material_slots[source].specular_texture
                    : batch.diffuse_textures[0];
                encoded =
                    encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(8 + slot),
                        normal_texture, error) &&
                    encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(16 + slot),
                        specular_texture, error);
            }
        }
        if (encoded && pbr)
        {
            for (std::size_t slot = 0;
                 encoded && slot < ALMetalWorldSimpleBatch::max_material_slots;
                 ++slot)
            {
                const std::size_t source = std::min<std::size_t>(
                    slot, batch.pbr_slot_count - 1);
                const ALMetalWorldPBRSlot& pbr_slot =
                    batch.pbr_slots[source];
                encoded =
                    encoder->setFragmentTextureLinear(
                        static_cast<std::uint32_t>(8 + slot),
                        pbr_slot.normal_texture, error) &&
                    encoder->setFragmentTextureLinear(
                        static_cast<std::uint32_t>(16 + slot),
                        pbr_slot.orm_texture, error) &&
                    encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(24 + slot),
                        pbr_slot.emissive_texture, error);
            }
        }
        encoded = encoded &&
            encoder->drawIndexed(ALMetalPrimitiveType::triangle,
                                 batch.index_count, batch.index_type,
                                 *batch.indices, batch.index_offset, 1, error);
    }

    if (!encoded || !encoder->end(error))
    {
        return nullptr;
    }
    if (scene.stamp_opaque_coverage)
    {
        ALMetalRenderPassConfig coverage_config;
        for (std::size_t attachment = 0; attachment < 4; ++attachment)
        {
            coverage_config.colors[attachment].load =
                ALMetalLoadAction::load;
            coverage_config.colors[attachment].store =
                ALMetalStoreAction::store;
        }
        coverage_config.depth_load = ALMetalLoadAction::load;
        coverage_config.depth_store = ALMetalStoreAction::store;
        coverage_config.stencil_load = ALMetalLoadAction::load;
        coverage_config.stencil_store = ALMetalStoreAction::store;
        auto coverage_pass = target->makeRenderPass(coverage_config, error);
        if (!coverage_pass)
        {
            return nullptr;
        }
        auto coverage_encoder = ALMetalRenderEncoder::begin(
            *command, *coverage_pass,
            "Alchemy world opaque-coverage encoder", error);
        if (!coverage_encoder)
        {
            return nullptr;
        }
        const std::uint32_t reverse_z = scene.reverse_z ? 1u : 0u;
        const ALMetalDepthStencilState* coverage_depth = scene.reverse_z
            ? mImpl->reverse_coverage_depth.get()
            : mImpl->forward_coverage_depth.get();
        const ALMetalRenderPipeline* coverage_pipeline =
            mImpl->coverage_pipelines[scene.hdr ? 1 : 0]
                                     [scene.normal_hdr ? 1 : 0].get();
        encoded =
            coverage_encoder->setDepthStencilState(coverage_depth, error) &&
            coverage_encoder->setViewport(viewport, error) &&
            coverage_encoder->setScissor(scissor, error) &&
            coverage_encoder->setCullMode(ALMetalCullMode::none, error) &&
            coverage_encoder->setPipeline(*coverage_pipeline, error) &&
            coverage_encoder->setVertexBytes(
                0, &reverse_z, sizeof(reverse_z), error) &&
            coverage_encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
            coverage_encoder->end(error);
        if (!encoded)
        {
            return nullptr;
        }
    }
    if (!mImpl->finishCommandBuffer(owned_command, true, error))
    {
        return nullptr;
    }
    return target;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderLocalLights(
    const ALMetalWorldLocalLightingScene& scene,
    std::string* error)
{
    std::unique_ptr<ALMetalRenderTarget> output;
    if (!renderLocalLightsInternal(scene, nullptr, output, error))
    {
        return nullptr;
    }
    return output;
}

bool ALMetalWorldGBufferRenderer::accumulateLocalLights(
    ALMetalRenderTarget& target,
    const ALMetalWorldLocalLightingScene& scene,
    std::string* error)
{
    std::unique_ptr<ALMetalRenderTarget> unused;
    return renderLocalLightsInternal(scene, &target, unused, error) !=
        nullptr;
}

ALMetalRenderTarget*
ALMetalWorldGBufferRenderer::renderLocalLightsInternal(
    const ALMetalWorldLocalLightingScene& scene,
    ALMetalRenderTarget* accumulation_target,
    std::unique_ptr<ALMetalRenderTarget>& standalone_target,
    std::string* error)
{
    constexpr std::size_t max_light_count = 65535;
    if (!mImpl || !mImpl->device || !scene.gbuffer ||
        (scene.light_count != 0 && !scene.lights) ||
        scene.light_count > max_light_count ||
        !finiteValues(scene.inverse_projection, 16) ||
        !std::isfinite(scene.specular_exponent) ||
        scene.specular_exponent <= 0.f)
    {
        setError(error, "Metal world local-light scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input =
        scene.gbuffer->descriptor();
    const ALMetalTexture* albedo = scene.gbuffer->colorTexture(0);
    const ALMetalTexture* specular = scene.gbuffer->colorTexture(1);
    const ALMetalTexture* normal = scene.gbuffer->colorTexture(2);
    const ALMetalTexture* emissive = scene.gbuffer->colorTexture(3);
    const ALMetalTexture* depth = scene.gbuffer->depthStencilTexture();
    const bool normal_format =
        input.colors[2].format == ALMetalPixelFormat::rgb10a2_unorm ||
        input.colors[2].format == ALMetalPixelFormat::rgba16_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        input.color_attachment_count != 4 ||
        input.colors[0].format != ALMetalPixelFormat::rgba8_unorm_srgb ||
        input.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        !normal_format || input.colors[3].format != expected_emissive ||
        input.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8 ||
        !input.depth_stencil_shader_read ||
        !sampleableTexture(albedo) || !sampleableTexture(specular) ||
        !sampleableTexture(normal) || !sampleableTexture(emissive) ||
        !sampleableTexture(depth))
    {
        setError(error, "Metal world local-light G-buffer is incompatible");
        return nullptr;
    }

    for (std::size_t index = 0; index < scene.light_count; ++index)
    {
        const ALMetalWorldLocalLight& light = scene.lights[index];
        if (!finiteValues(light.position_radius, 4) ||
            !finiteValues(light.color_falloff, 4) ||
            light.position_radius[3] <= 0.f ||
            light.color_falloff[3] <= -1.f)
        {
            setError(error, "Metal world local point light is invalid");
            return nullptr;
        }
    }

    std::unique_ptr<ALMetalBuffer> light_buffer;
    if (scene.light_count != 0)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length = scene.light_count * sizeof(ALMetalWorldLocalLight);
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label = "Alchemy world local-point-light list";
        light_buffer = ALMetalBuffer::create(
            *mImpl->device, descriptor, error);
        if (!light_buffer || !light_buffer->write(
                0, scene.lights, descriptor.length, error))
        {
            return nullptr;
        }
    }

    const ALMetalPixelFormat output_format = scene.hdr
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rgba8_unorm;
    ALMetalRenderTarget* output = accumulation_target;
    if (output)
    {
        const ALMetalRenderTargetDescriptor& output_descriptor =
            output->descriptor();
        if (output_descriptor.width != input.width ||
            output_descriptor.height != input.height ||
            output_descriptor.sample_count != 1 ||
            output_descriptor.color_attachment_count != 1 ||
            output_descriptor.colors[0].format != output_format ||
            !output->colorTexture(0))
        {
            setError(
                error,
                "Metal world local-light accumulation target is incompatible");
            return nullptr;
        }
    }
    else
    {
        ALMetalRenderTargetDescriptor output_descriptor;
        output_descriptor.width = input.width;
        output_descriptor.height = input.height;
        output_descriptor.colors[0].format = output_format;
        output_descriptor.colors[0].label =
            "Alchemy world local-point-light accumulation";
        output_descriptor.label =
            "Alchemy native world local-point-light target";
        standalone_target = ALMetalRenderTarget::create(
            *mImpl->device, output_descriptor, error);
        output = standalone_target.get();
        if (!output)
        {
            return nullptr;
        }
    }

    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world local-point-light accumulation",
        owned_command, error);
    if (!command)
    {
        return nullptr;
    }

    constexpr std::uint32_t brdf_lut_size = 512;
    const bool generate_brdf_lut =
        scene.light_count != 0 && !mImpl->brdf_lut_ready &&
        !mImpl->command_batch_generates_brdf_lut;
    if (generate_brdf_lut)
    {
        auto compute = ALMetalComputeEncoder::begin(
            *command, "Alchemy world PBR BRDF LUT encoder", error);
        if (!compute ||
            !compute->setPipeline(*mImpl->brdf_lut_pipeline, error) ||
            !compute->setTexture(0, mImpl->brdf_lut.get(), error) ||
            !compute->dispatch(
                brdf_lut_size, brdf_lut_size, 1, 8, 8, 1, error) ||
            !compute->end(error))
        {
            return nullptr;
        }
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = accumulation_target
        ? ALMetalLoadAction::load : ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {0.0, 0.0, 0.0, 0.0};
    pass_config.bind_depth_stencil = false;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass,
        accumulation_target
            ? "Alchemy world in-place local-point-light encoder"
            : "Alchemy world local-point-light encoder",
        error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    bool encoded = true;
    if (scene.light_count != 0)
    {
        WorldLocalLightingConstants constants;
        std::memcpy(constants.inverse_projection,
                    scene.inverse_projection,
                    sizeof(constants.inverse_projection));
        constants.surface[0] = scene.specular_exponent;
        constants.surface[1] = scene.classic_mode ? 0.9f : 1.f;
        constants.config[0] =
            static_cast<std::uint32_t>(scene.light_count);
        constants.config[1] = scene.reverse_z ? 1u : 0u;
        const ALMetalViewport viewport{
            0.0, 0.0,
            static_cast<double>(input.width),
            static_cast<double>(input.height), 0.0, 1.0,
        };
        const ALMetalScissorRect scissor{
            0, 0, input.width, input.height,
        };
        encoded =
            encoder->setViewport(viewport, error) &&
            encoder->setScissor(scissor, error) &&
            encoder->setCullMode(ALMetalCullMode::none, error) &&
            encoder->setPipeline(
                *(accumulation_target
                    ? mImpl->local_light_accumulation_pipelines[
                          scene.hdr ? 1 : 0]
                    : mImpl->local_light_pipelines[
                          scene.hdr ? 1 : 0]),
                error) &&
            encoder->setFragmentBuffer(0, *light_buffer, 0, error) &&
            encoder->setFragmentBytes(
                1, &constants, sizeof(constants), error) &&
            encoder->setFragmentTexture(0, albedo, error) &&
            encoder->setFragmentTexture(1, specular, error) &&
            encoder->setFragmentTexture(2, normal, error) &&
            encoder->setFragmentTexture(3, depth, error) &&
            encoder->setFragmentTexture(4, mImpl->brdf_lut.get(), error) &&
            encoder->setFragmentSampler(
                0, mImpl->point_clamp_sampler.get(), error) &&
            encoder->setFragmentSampler(
                1, mImpl->linear_clamp_sampler.get(), error) &&
            encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    }
    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended, error))
    {
        return nullptr;
    }
    if (generate_brdf_lut)
    {
        if (mImpl->command_batch)
        {
            mImpl->command_batch_generates_brdf_lut = true;
        }
        else
        {
            // The renderer uses one queue. A later command therefore sees
            // this generation without a CPU wait.
            mImpl->brdf_lut_ready = true;
        }
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderProjectorLights(
    const ALMetalWorldProjectorLightingScene& scene,
    std::string* error)
{
    std::unique_ptr<ALMetalRenderTarget> output;
    if (!renderProjectorLightsInternal(scene, nullptr, output, error))
    {
        return nullptr;
    }
    return output;
}

bool ALMetalWorldGBufferRenderer::accumulateProjectorLights(
    ALMetalRenderTarget& target,
    const ALMetalWorldProjectorLightingScene& scene,
    std::string* error)
{
    std::unique_ptr<ALMetalRenderTarget> unused;
    return renderProjectorLightsInternal(scene, &target, unused, error) !=
        nullptr;
}

ALMetalRenderTarget*
ALMetalWorldGBufferRenderer::renderProjectorLightsInternal(
    const ALMetalWorldProjectorLightingScene& scene,
    ALMetalRenderTarget* accumulation_target,
    std::unique_ptr<ALMetalRenderTarget>& standalone_target,
    std::string* error)
{
    constexpr std::size_t max_light_count = 65535;
    if (!mImpl || !mImpl->device || !scene.gbuffer ||
        (scene.light_count != 0 && !scene.lights) ||
        scene.light_count > max_light_count ||
        !finiteValues(scene.inverse_projection, 16) ||
        !std::isfinite(scene.specular_exponent) ||
        scene.specular_exponent <= 0.f)
    {
        setError(error, "Metal world projector-light scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input =
        scene.gbuffer->descriptor();
    const ALMetalTexture* albedo = scene.gbuffer->colorTexture(0);
    const ALMetalTexture* specular = scene.gbuffer->colorTexture(1);
    const ALMetalTexture* normal = scene.gbuffer->colorTexture(2);
    const ALMetalTexture* emissive = scene.gbuffer->colorTexture(3);
    const ALMetalTexture* depth = scene.gbuffer->depthStencilTexture();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        input.color_attachment_count != 4 ||
        input.colors[0].format != ALMetalPixelFormat::rgba8_unorm_srgb ||
        input.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        input.colors[2].format != expected_normal ||
        input.colors[3].format != expected_emissive ||
        input.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8 ||
        !input.depth_stencil_shader_read ||
        !sampleableTexture(albedo) || !sampleableTexture(specular) ||
        !sampleableTexture(normal) || !sampleableTexture(emissive) ||
        !sampleableTexture(depth))
    {
        setError(error, "Metal world projector-light G-buffer is incompatible");
        return nullptr;
    }

    bool needs_shadows = false;
    for (std::size_t index = 0; index < scene.light_count; ++index)
    {
        const ALMetalWorldProjectorLight& light = scene.lights[index];
        const ALMetalTextureDescriptor* cookie = light.cookie_texture
            ? &light.cookie_texture->descriptor() : nullptr;
        const bool valid_cookie = sampleableTexture(light.cookie_texture) &&
            cookie && cookie->type == ALMetalTextureType::texture_2d &&
            cookie->sample_count == 1;
        const bool finite_position =
            finiteValues(light.position_radius, 4);
        const bool finite_color = finiteValues(light.color_falloff, 4);
        const bool finite_matrix =
            finiteValues(light.projector_matrix, 16);
        const bool finite_plane = finiteValues(light.projector_plane, 4);
        const bool finite_normal = finiteValues(light.projector_normal, 4);
        const bool finite_origin = finiteValues(light.projector_origin, 4);
        const bool finite_sampling = finiteValues(light.sampling, 4);
        const bool valid_scalars = light.position_radius[3] > 0.f &&
            light.color_falloff[3] > -1.f &&
            light.projector_normal[3] > 0.f &&
            light.sampling[0] >= 1.f && light.sampling[1] >= 0.f &&
            light.sampling[1] <= 1.f && light.shadow_channel >= -1 &&
            light.shadow_channel <= 1;
        if (!valid_cookie || !finite_position || !finite_color ||
            !finite_matrix || !finite_plane || !finite_normal ||
            !finite_origin || !finite_sampling || !valid_scalars)
        {
            setError(error,
                "Metal world projector light is invalid at index " +
                std::to_string(index) + " (cookie=" +
                std::to_string(valid_cookie) + ", position=" +
                std::to_string(finite_position) + ", color=" +
                std::to_string(finite_color) + ", matrix=" +
                std::to_string(finite_matrix) + ", plane=" +
                std::to_string(finite_plane) + ", normal=" +
                std::to_string(finite_normal) + ", origin=" +
                std::to_string(finite_origin) + ", sampling=" +
                std::to_string(finite_sampling) + ", radius=" +
                std::to_string(light.position_radius[3]) + ", falloff=" +
                std::to_string(light.color_falloff[3]) + ", range=" +
                std::to_string(light.projector_normal[3]) + ", focus=" +
                std::to_string(light.projector_origin[3]) + ", lod=" +
                std::to_string(light.sampling[0]) + ", shadow-fade=" +
                std::to_string(light.sampling[1]) + ", shadow-channel=" +
                std::to_string(light.shadow_channel) + ")");
            return nullptr;
        }
        needs_shadows = needs_shadows || light.shadow_channel >= 0;
    }
    if (needs_shadows)
    {
        if (!validProjectorShadows(scene.projector_shadows))
        {
            setError(error,
                     "Metal world projector shadows are incompatible");
            return nullptr;
        }
    }

    const ALMetalPixelFormat output_format = scene.hdr
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rgba8_unorm;
    ALMetalRenderTarget* output = accumulation_target;
    if (output)
    {
        const ALMetalRenderTargetDescriptor& output_descriptor =
            output->descriptor();
        if (output_descriptor.width != input.width ||
            output_descriptor.height != input.height ||
            output_descriptor.sample_count != 1 ||
            output_descriptor.color_attachment_count != 1 ||
            output_descriptor.colors[0].format != output_format ||
            !output->colorTexture(0))
        {
            setError(
                error,
                "Metal world projector-light accumulation target is incompatible");
            return nullptr;
        }
    }
    else
    {
        ALMetalRenderTargetDescriptor output_descriptor;
        output_descriptor.width = input.width;
        output_descriptor.height = input.height;
        output_descriptor.colors[0].format = output_format;
        output_descriptor.colors[0].label =
            "Alchemy world projector-light accumulation";
        output_descriptor.label =
            "Alchemy native world projector-light target";
        standalone_target = ALMetalRenderTarget::create(
            *mImpl->device, output_descriptor, error);
        output = standalone_target.get();
        if (!output)
        {
            return nullptr;
        }
    }

    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world projector-light accumulation",
        owned_command, error);
    if (!command)
    {
        return nullptr;
    }

    constexpr std::uint32_t brdf_lut_size = 512;
    const bool generate_brdf_lut =
        scene.light_count != 0 && !mImpl->brdf_lut_ready &&
        !mImpl->command_batch_generates_brdf_lut;
    if (generate_brdf_lut)
    {
        auto compute = ALMetalComputeEncoder::begin(
            *command, "Alchemy world projector BRDF LUT encoder", error);
        if (!compute ||
            !compute->setPipeline(*mImpl->brdf_lut_pipeline, error) ||
            !compute->setTexture(0, mImpl->brdf_lut.get(), error) ||
            !compute->dispatch(
                brdf_lut_size, brdf_lut_size, 1, 8, 8, 1, error) ||
            !compute->end(error))
        {
            return nullptr;
        }
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = accumulation_target
        ? ALMetalLoadAction::load : ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {0.0, 0.0, 0.0, 0.0};
    pass_config.bind_depth_stencil = false;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass,
        accumulation_target
            ? "Alchemy world in-place projector-light encoder"
            : "Alchemy world projector-light encoder",
        error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0,
        static_cast<double>(input.width),
        static_cast<double>(input.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, input.width, input.height};
    const ALMetalTexture* projector_shadow_depth = needs_shadows
        ? scene.projector_shadows->depth_texture
        : mImpl->unshadowed_depth.get();
    const ALMetalWorldDirectionalShadowConstants& projector_shadows =
        needs_shadows
            ? scene.projector_shadows->constants
            : mImpl->unshadowed_constants;
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(
            *mImpl->projector_light_pipelines[scene.hdr ? 1 : 0], error) &&
        encoder->setFragmentTexture(0, albedo, error) &&
        encoder->setFragmentTexture(1, specular, error) &&
        encoder->setFragmentTexture(2, normal, error) &&
        encoder->setFragmentTexture(3, depth, error) &&
        encoder->setFragmentTexture(4, mImpl->brdf_lut.get(), error) &&
        encoder->setFragmentTexture(6, projector_shadow_depth, error) &&
        encoder->setFragmentBytes(
            1, &projector_shadows, sizeof(projector_shadows), error) &&
        encoder->setFragmentSampler(
            0, mImpl->point_clamp_sampler.get(), error) &&
        encoder->setFragmentSampler(
            1, mImpl->linear_clamp_sampler.get(), error) &&
        encoder->setFragmentSampler(
            2, mImpl->diffuse_sampler.get(), error);

    for (std::size_t index = 0; encoded && index < scene.light_count;
         ++index)
    {
        const ALMetalWorldProjectorLight& light = scene.lights[index];
        WorldProjectorLightingConstants constants;
        std::memcpy(constants.inverse_projection,
                    scene.inverse_projection,
                    sizeof(constants.inverse_projection));
        std::memcpy(constants.projector_matrix,
                    light.projector_matrix,
                    sizeof(constants.projector_matrix));
        std::memcpy(constants.position_radius, light.position_radius,
                    sizeof(constants.position_radius));
        std::memcpy(constants.color_falloff, light.color_falloff,
                    sizeof(constants.color_falloff));
        std::memcpy(constants.projector_plane, light.projector_plane,
                    sizeof(constants.projector_plane));
        std::memcpy(constants.projector_normal, light.projector_normal,
                    sizeof(constants.projector_normal));
        std::memcpy(constants.projector_origin, light.projector_origin,
                    sizeof(constants.projector_origin));
        constants.sampling[0] = light.sampling[0];
        constants.sampling[1] = light.sampling[1];
        constants.sampling[2] = scene.specular_exponent;
        constants.sampling[3] = scene.classic_mode ? 0.9f : 1.f;
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = static_cast<std::uint32_t>(
            light.shadow_channel + 1);
        constants.config[2] = scene.normal_hdr ? 1u : 0u;
        encoded =
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->setFragmentTexture(
                5, light.cookie_texture, error) &&
            encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    }

    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended, error))
    {
        return nullptr;
    }
    if (generate_brdf_lut)
    {
        if (mImpl->command_batch)
        {
            mImpl->command_batch_generates_brdf_lut = true;
        }
        else
        {
            mImpl->brdf_lut_ready = true;
        }
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::composeLighting(
    const ALMetalWorldLightingCompositionScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.base_lighting ||
        !scene.local_lighting || !scene.projector_lighting)
    {
        setError(error, "Metal world lighting composition is incomplete");
        return nullptr;
    }

    const ALMetalPixelFormat expected_format = scene.hdr
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rgba8_unorm;
    const ALMetalRenderTargetDescriptor& base =
        scene.base_lighting->descriptor();
    const auto compatible = [&](const ALMetalRenderTarget& target)
    {
        const ALMetalRenderTargetDescriptor& descriptor =
            target.descriptor();
        return descriptor.width == base.width &&
            descriptor.height == base.height &&
            descriptor.sample_count == 1 &&
            descriptor.color_attachment_count == 1 &&
            descriptor.colors[0].format == expected_format &&
            sampleableTexture(target.colorTexture(0));
    };
    if (base.width == 0 || base.height == 0 ||
        !compatible(*scene.base_lighting) ||
        !compatible(*scene.local_lighting) ||
        !compatible(*scene.projector_lighting))
    {
        setError(error,
                 "Metal world lighting composition targets are incompatible");
        return nullptr;
    }
    if (scene.depth_source)
    {
        const ALMetalRenderTargetDescriptor& depth =
            scene.depth_source->descriptor();
        if (depth.width != base.width || depth.height != base.height ||
            depth.sample_count != 1 ||
            depth.depth_stencil_format !=
                ALMetalPixelFormat::depth32_float_stencil8 ||
            !scene.depth_source->depthStencilTexture())
        {
            setError(
                error,
                "Metal world lighting composition depth source is incompatible");
            return nullptr;
        }
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = base.width;
    output_descriptor.height = base.height;
    output_descriptor.colors[0].format = expected_format;
    output_descriptor.colors[0].label =
        "Alchemy world composed deferred lighting";
    if (scene.depth_source)
    {
        output_descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        output_descriptor.depth_stencil_shader_read =
            scene.depth_stencil_shader_read;
    }
    output_descriptor.label =
        "Alchemy native world deferred-light composition target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world deferred-light composition", owned_command,
        error);
    if (!command)
    {
        return nullptr;
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::dont_care;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.depth_load = ALMetalLoadAction::dont_care;
    pass_config.depth_store = ALMetalStoreAction::dont_care;
    pass_config.stencil_load = ALMetalLoadAction::dont_care;
    pass_config.stencil_store = ALMetalStoreAction::dont_care;
    pass_config.bind_depth_stencil = false;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass,
        "Alchemy world deferred-light composition encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(base.width),
        static_cast<double>(base.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{
        0, 0, base.width, base.height,
    };
    const bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(
            *mImpl->lighting_composition_pipelines[scene.hdr ? 1 : 0],
            error) &&
        encoder->setFragmentTexture(
            0, scene.base_lighting->colorTexture(0), error) &&
        encoder->setFragmentTexture(
            1, scene.local_lighting->colorTexture(0), error) &&
        encoder->setFragmentTexture(
            2, scene.projector_lighting->colorTexture(0), error) &&
        encoder->setFragmentSampler(
            0, mImpl->point_clamp_sampler.get(), error) &&
        encoder->draw(
            ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    const bool ended = encoder->end(error);
    const bool copied_depth =
        !scene.depth_source ||
        (encoded && ended && output->copyDepthStencilFrom(
            *command, *scene.depth_source->depthStencilTexture(), {},
            error));
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended && copied_depth, error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderBloom(
    const ALMetalWorldBloomScene& scene,
    std::string* error,
    std::unique_ptr<ALMetalRenderTarget>* cross_filter)
{
    if (cross_filter)
    {
        cross_filter->reset();
    }
    if (!mImpl || !mImpl->device || !scene.source)
    {
        setError(error, "Metal world bloom scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& source =
        scene.source->descriptor();
    if (source.width == 0 || source.height == 0 ||
        source.sample_count != 1 || source.color_attachment_count != 1 ||
        source.colors[0].format != ALMetalPixelFormat::rgba16_float ||
        !sampleableTexture(scene.source->colorTexture(0)))
    {
        setError(error, "Metal world bloom source is incompatible");
        return nullptr;
    }
    if (scene.mip_count < 3 || scene.mip_count > 7 ||
        scene.resolution_scale > 4 ||
        !std::isfinite(scene.threshold) ||
        !std::isfinite(scene.knee) ||
        !std::isfinite(scene.firefly_clamp) ||
        !std::isfinite(scene.scatter) ||
        !std::isfinite(scene.alpha_glow_boost) ||
        !finiteValues(scene.halation_weights, 3) ||
        !std::isfinite(scene.cross_filter_angle) ||
        !std::isfinite(scene.cross_filter_length) ||
        !std::isfinite(scene.cross_filter_falloff) ||
        !std::isfinite(scene.cross_filter_chromatic) ||
        scene.knee < 0.f || scene.firefly_clamp < 0.f ||
        scene.scatter < 0.f || scene.alpha_glow_boost < 0.f ||
        (scene.cross_filter && !cross_filter) ||
        scene.cross_filter_points < 2 || scene.cross_filter_points > 12 ||
        scene.cross_filter_angle < 0.f ||
        scene.cross_filter_angle > 360.f ||
        scene.cross_filter_length < 0.25f ||
        scene.cross_filter_length > 2.f ||
        scene.cross_filter_falloff < 0.1f ||
        scene.cross_filter_falloff > 3.f ||
        scene.cross_filter_chromatic < 0.f ||
        scene.cross_filter_chromatic > 1.f)
    {
        setError(error, "Metal world bloom state is invalid");
        return nullptr;
    }

    constexpr std::array<std::uint32_t, 5> scale_numerator{{
        1, 3, 1, 1, 1,
    }};
    constexpr std::array<std::uint32_t, 5> scale_denominator{{
        1, 4, 2, 4, 8,
    }};
    const std::size_t scale = scene.resolution_scale;
    const std::uint32_t base_width = std::max(
        1u, static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(source.width) *
                scale_numerator[scale] / scale_denominator[scale]));
    const std::uint32_t base_height = std::max(
        1u, static_cast<std::uint32_t>(
                static_cast<std::uint64_t>(source.height) *
                scale_numerator[scale] / scale_denominator[scale]));

    std::vector<std::unique_ptr<ALMetalRenderTarget>> targets;
    targets.reserve(scene.mip_count);
    for (std::uint32_t mip = 0; mip < scene.mip_count; ++mip)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = std::max(1u, base_width >> mip);
        descriptor.height = std::max(1u, base_height >> mip);
        descriptor.colors[0].format = scene.halation
            ? ALMetalPixelFormat::rgba16_float
            : ALMetalPixelFormat::rg11b10_float;
        descriptor.colors[0].shader_read = true;
        descriptor.colors[0].label =
            "Alchemy native world bloom color";
        descriptor.label = "Alchemy native world bloom target";
        auto target = ALMetalRenderTarget::create(
            *mImpl->device, descriptor, error);
        if (!target)
        {
            return nullptr;
        }
        targets.push_back(std::move(target));
        if (descriptor.width == 1 && descriptor.height == 1)
        {
            break;
        }
    }
    if (targets.size() < 3)
    {
        setError(error, "Metal world bloom pyramid is too shallow");
        return nullptr;
    }

    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world bloom", owned_command, error);
    if (!command)
    {
        return nullptr;
    }

    const auto encode_pass = [&command, error](
        ALMetalRenderTarget& target,
        const ALMetalRenderPipeline& pipeline,
        const ALMetalTexture& input,
        const ALMetalSamplerState& sampler,
        const void* constants,
        std::size_t constants_size,
        ALMetalLoadAction load,
        const char* label)
    {
        ALMetalRenderPassConfig config;
        config.colors[0].load = load;
        config.colors[0].store = ALMetalStoreAction::store;
        config.colors[0].clear = {0.0, 0.0, 0.0, 0.0};
        auto pass = target.makeRenderPass(config, error);
        auto encoder = pass ? ALMetalRenderEncoder::begin(
            *command, *pass, label, error) : nullptr;
        const ALMetalRenderTargetDescriptor& descriptor =
            target.descriptor();
        return encoder &&
            encoder->setViewport(
                {0.0, 0.0, static_cast<double>(descriptor.width),
                 static_cast<double>(descriptor.height), 0.0, 1.0},
                error) &&
            encoder->setScissor(
                {0, 0, descriptor.width, descriptor.height}, error) &&
            encoder->setCullMode(ALMetalCullMode::none, error) &&
            encoder->setPipeline(pipeline, error) &&
            encoder->setFragmentTexture(0, &input, error) &&
            encoder->setFragmentSampler(0, &sampler, error) &&
            encoder->setFragmentBytes(
                0, constants, constants_size, error) &&
            encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
            encoder->end(error);
    };

    WorldBloomExtractConstants extract;
    extract.params[0] = scene.threshold;
    extract.params[1] = scene.knee;
    extract.params[2] = scene.firefly_clamp;
    extract.params[3] = scene.alpha_glow_boost;
    std::memcpy(extract.halation_weights, scene.halation_weights,
                sizeof(scene.halation_weights));
    const std::size_t halation = scene.halation ? 1u : 0u;
    bool encoded = encode_pass(
        *targets[0], *mImpl->bloom_extract_pipelines[halation],
        *scene.source->colorTexture(0), *mImpl->linear_mirror_sampler,
        &extract, sizeof(extract), ALMetalLoadAction::clear,
        "Alchemy world bloom extract encoder");

    for (std::size_t mip = 1; encoded && mip < targets.size(); ++mip)
    {
        const ALMetalTexture* input =
            targets[mip - 1]->colorTexture(0);
        if (!input)
        {
            encoded = false;
            break;
        }
        WorldBloomFilterConstants filter;
        filter.params[0] = 1.f / static_cast<float>(input->width());
        filter.params[1] = 1.f / static_cast<float>(input->height());
        filter.params[2] = mip == 1 ? 1.f : 0.f;
        encoded = encode_pass(
            *targets[mip], *mImpl->bloom_downsample_pipelines[halation],
            *input, *mImpl->linear_mirror_sampler,
            &filter, sizeof(filter), ALMetalLoadAction::clear,
            "Alchemy world bloom downsample encoder");
    }

    std::array<std::unique_ptr<ALMetalRenderTarget>, 3>
        cross_targets;
    if (encoded && scene.cross_filter)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = std::max(
            1u, targets[0]->descriptor().width / 2u);
        descriptor.height = std::max(
            1u, targets[0]->descriptor().height / 2u);
        descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
        descriptor.colors[0].shader_read = true;
        descriptor.colors[0].label =
            "Alchemy native world cross-filter color";
        descriptor.label = "Alchemy native world cross-filter target";
        for (auto& target : cross_targets)
        {
            target = ALMetalRenderTarget::create(
                *mImpl->device, descriptor, error);
            if (!target)
            {
                mImpl->finishCommandBuffer(
                    owned_command, false, error);
                return nullptr;
            }
        }

        const float texel_x = 1.f /
            static_cast<float>(descriptor.width);
        const float texel_y = 1.f /
            static_cast<float>(descriptor.height);
        constexpr float pi = 3.14159265358979323846f;
        constexpr std::array<float, 3> pass_scales{{1.f, 4.f, 16.f}};
        constexpr float maximum_step = 63.f;
        const float angle = scene.cross_filter_angle * (pi / 180.f);
        const float falloff = std::exp(
            scene.cross_filter_falloff * 6.f / maximum_step);
        for (std::uint32_t arm = 0;
             encoded && arm < scene.cross_filter_points; ++arm)
        {
            const float theta = angle + 2.f * pi *
                static_cast<float>(arm) /
                static_cast<float>(scene.cross_filter_points);
            const ALMetalTexture* sources[3] = {
                targets[0]->colorTexture(0),
                cross_targets[0]->colorTexture(0),
                cross_targets[1]->colorTexture(0),
            };
            for (std::size_t pass_index = 0;
                 encoded && pass_index < pass_scales.size(); ++pass_index)
            {
                if (!sources[pass_index])
                {
                    encoded = false;
                    break;
                }
                WorldCrossFilterConstants constants;
                constants.sampling[0] = texel_x;
                constants.sampling[1] = texel_y;
                constants.sampling[2] = scene.cross_filter_length;
                constants.sampling[3] = scene.cross_filter_chromatic;
                constants.filtering[0] = std::cos(theta);
                constants.filtering[1] = std::sin(theta);
                constants.filtering[2] = pass_scales[pass_index];
                constants.filtering[3] = falloff;
                const bool additive = pass_index == 2 && arm > 0;
                encoded = encode_pass(
                    *cross_targets[pass_index],
                    additive ? *mImpl->cross_filter_add_pipeline
                             : *mImpl->cross_filter_pipeline,
                    *sources[pass_index], *mImpl->linear_clamp_sampler,
                    &constants, sizeof(constants),
                    additive ? ALMetalLoadAction::load
                             : ALMetalLoadAction::clear,
                    "Alchemy world cross-filter encoder");
            }
        }
    }

    for (std::size_t mip = targets.size() - 1;
         encoded && mip > 0; --mip)
    {
        const ALMetalTexture* input = targets[mip]->colorTexture(0);
        if (!input)
        {
            encoded = false;
            break;
        }
        WorldBloomFilterConstants filter;
        filter.params[0] = 1.f / static_cast<float>(input->width());
        filter.params[1] = 1.f / static_cast<float>(input->height());
        filter.params[2] = scene.scatter;
        encoded = encode_pass(
            *targets[mip - 1], *mImpl->bloom_upsample_pipelines[halation],
            *input, *mImpl->linear_mirror_sampler,
            &filter, sizeof(filter), ALMetalLoadAction::load,
            "Alchemy world bloom upsample encoder");
    }

    if (!mImpl->finishCommandBuffer(owned_command, encoded, error))
    {
        return nullptr;
    }
    if (scene.cross_filter)
    {
        *cross_filter = std::move(cross_targets[2]);
    }
    return std::move(targets[0]);
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderExposure(
    const ALMetalWorldExposureScene& scene,
    std::string* error)
{
    constexpr std::uint32_t meter_width = 128;
    constexpr std::uint32_t meter_height = 72;
    if (!mImpl || !mImpl->device || !scene.source || !scene.normal)
    {
        setError(error, "Metal world exposure scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& source =
        scene.source->descriptor();
    const ALMetalTextureDescriptor& normal = scene.normal->descriptor();
    const bool normal_format =
        normal.format == ALMetalPixelFormat::rgb10a2_unorm ||
        normal.format == ALMetalPixelFormat::rgba16_unorm;
    if (source.width == 0 || source.height == 0 ||
        source.sample_count != 1 || source.color_attachment_count != 1 ||
        source.colors[0].format != ALMetalPixelFormat::rgba16_float ||
        !sampleableTexture(scene.source->colorTexture(0)) ||
        normal.width != source.width || normal.height != source.height ||
        !normal_format || !sampleableTexture(scene.normal))
    {
        setError(error, "Metal world exposure source is incompatible");
        return nullptr;
    }

    if (scene.previous)
    {
        const ALMetalTextureDescriptor& previous =
            scene.previous->descriptor();
        if (previous.width != 1 || previous.height != 1 ||
            previous.format != ALMetalPixelFormat::rgba16_float ||
            !sampleableTexture(scene.previous))
        {
            setError(error,
                     "Metal world exposure history is incompatible");
            return nullptr;
        }
    }

    if (!std::isfinite(scene.delta_seconds) ||
        !std::isfinite(scene.compensation) ||
        !std::isfinite(scene.min_ev) ||
        !std::isfinite(scene.max_ev) ||
        !std::isfinite(scene.low_fraction) ||
        !std::isfinite(scene.high_fraction) ||
        !std::isfinite(scene.speed_up) ||
        !std::isfinite(scene.speed_down) ||
        !std::isfinite(scene.sky_weight) ||
        scene.delta_seconds < 0.f || scene.low_fraction < 0.f ||
        scene.low_fraction > 0.99f ||
        scene.high_fraction < scene.low_fraction + 0.01f ||
        scene.high_fraction > 1.f || scene.speed_up < 0.f ||
        scene.speed_down < 0.f || scene.sky_weight < 0.f ||
        scene.metering_mode > 2)
    {
        setError(error, "Metal world exposure state is invalid");
        return nullptr;
    }

    ALMetalRenderTargetDescriptor meter_descriptor;
    meter_descriptor.width = meter_width;
    meter_descriptor.height = meter_height;
    meter_descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
    meter_descriptor.colors[0].shader_write = true;
    meter_descriptor.colors[0].label =
        "Alchemy native world exposure meter color";
    meter_descriptor.label = "Alchemy native world exposure meter";
    auto meter = ALMetalRenderTarget::create(
        *mImpl->device, meter_descriptor, error);
    if (!meter)
    {
        return nullptr;
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = 1;
    output_descriptor.height = 1;
    output_descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
    output_descriptor.colors[0].shader_write = true;
    output_descriptor.colors[0].label =
        "Alchemy native world exposure color";
    output_descriptor.label = "Alchemy native world exposure";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    WorldExposureMeterConstants meter_constants;
    meter_constants.patch[0] =
        static_cast<float>(source.width) / static_cast<float>(meter_width);
    meter_constants.patch[1] =
        static_cast<float>(source.height) / static_cast<float>(meter_height);
    meter_constants.patch[2] = std::max(
        1.f, std::ceil(meter_constants.patch[0] * 0.5f));
    meter_constants.patch[3] = std::max(
        1.f, std::ceil(meter_constants.patch[1] * 0.5f));

    WorldExposureConstants exposure_constants;
    constexpr float exposure_key = 0.18f;
    exposure_constants.exposure[0] =
        std::log2(exposure_key) + scene.compensation;
    exposure_constants.exposure[1] =
        std::min(scene.min_ev, scene.max_ev);
    exposure_constants.exposure[2] =
        std::max(scene.min_ev, scene.max_ev);
    exposure_constants.exposure[3] = scene.delta_seconds;
    exposure_constants.adaptation[0] = scene.low_fraction;
    exposure_constants.adaptation[1] = scene.high_fraction;
    exposure_constants.adaptation[2] = scene.speed_up;
    exposure_constants.adaptation[3] = scene.speed_down;
    exposure_constants.meter[0] = scene.sky_weight;
    exposure_constants.meter[1] =
        static_cast<float>(scene.metering_mode);

    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world exposure", owned_command, error);
    if (!command)
    {
        return nullptr;
    }
    auto meter_encoder = ALMetalComputeEncoder::begin(
        *command, "Alchemy world exposure-meter encoder", error);
    const bool meter_encoded = meter_encoder &&
        meter_encoder->setPipeline(
            *mImpl->exposure_meter_pipeline, error) &&
        meter_encoder->setTexture(
            0, scene.source->colorTexture(0), error) &&
        meter_encoder->setTexture(1, scene.normal, error) &&
        meter_encoder->setTexture(2, meter->colorTexture(0), error) &&
        meter_encoder->setSampler(
            0, mImpl->linear_clamp_sampler.get(), error) &&
        meter_encoder->setBytes(
            0, &meter_constants, sizeof(meter_constants), error) &&
        meter_encoder->dispatch(
            meter_width, meter_height, 1, 8, 8, 1, error) &&
        meter_encoder->end(error);
    if (!meter_encoded)
    {
        mImpl->finishCommandBuffer(owned_command, false, error);
        return nullptr;
    }

    const std::size_t history = scene.previous ? 1u : 0u;
    auto exposure_encoder = ALMetalComputeEncoder::begin(
        *command, history
            ? "Alchemy world exposure-history encoder"
            : "Alchemy world initial-exposure encoder",
        error);
    bool encoded = exposure_encoder &&
        exposure_encoder->setPipeline(
            *mImpl->exposure_pipelines[history], error) &&
        exposure_encoder->setTexture(0, meter->colorTexture(0), error);
    if (encoded && scene.previous)
    {
        encoded = exposure_encoder->setTexture(
            1, scene.previous, error) &&
            exposure_encoder->setTexture(
                2, output->colorTexture(0), error);
    }
    else if (encoded)
    {
        encoded = exposure_encoder->setTexture(
            1, output->colorTexture(0), error);
    }
    encoded = encoded &&
        exposure_encoder->setBytes(
            0, &exposure_constants, sizeof(exposure_constants), error) &&
        exposure_encoder->dispatch(1, 1, 1, 1, 1, 1, error) &&
        exposure_encoder->end(error);
    if (!mImpl->finishCommandBuffer(owned_command, encoded, error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderLensFlareState(
    const ALMetalWorldLensFlareStateScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.source ||
        !scene.depth_texture)
    {
        setError(error, "Metal world lens-flare state scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& source =
        scene.source->descriptor();
    const ALMetalTextureDescriptor& depth =
        scene.depth_texture->descriptor();
    const bool color_format = source.color_attachment_count == 1 &&
        (source.colors[0].format == ALMetalPixelFormat::rgba16_float ||
         source.colors[0].format == ALMetalPixelFormat::rgba8_unorm);
    const bool depth_format =
        depth.format == ALMetalPixelFormat::depth32_float ||
        depth.format == ALMetalPixelFormat::depth32_float_stencil8;
    if (source.width == 0 || source.height == 0 ||
        source.sample_count != 1 || !color_format ||
        !sampleableTexture(scene.source->colorTexture(0)) ||
        !sampleableTexture(scene.depth_texture) || !depth_format ||
        depth.width != source.width || depth.height != source.height ||
        depth.array_length != 1 || depth.mipmap_level_count != 1 ||
        depth.sample_count != 1)
    {
        setError(error,
                 "Metal world lens-flare state inputs are incompatible");
        return nullptr;
    }

    if (!std::isfinite(scene.delta_seconds) ||
        !finiteValues(scene.sun_uv, 2) ||
        !std::isfinite(scene.edge_visibility) ||
        !std::isfinite(scene.occlusion_radius) ||
        !std::isfinite(scene.fade_time) ||
        scene.delta_seconds < 0.f ||
        scene.edge_visibility < 0.f || scene.edge_visibility > 1.f ||
        scene.occlusion_radius < 0.f || scene.occlusion_radius > 0.3f ||
        scene.fade_time < 0.1f || scene.fade_time > 1.f)
    {
        setError(error, "Metal world lens-flare state is invalid");
        return nullptr;
    }

    if (scene.previous)
    {
        const ALMetalTextureDescriptor& previous =
            scene.previous->descriptor();
        if (previous.type != ALMetalTextureType::texture_2d ||
            previous.format != ALMetalPixelFormat::rgba16_float ||
            previous.width != 2 || previous.height != 1 ||
            previous.array_length != 1 ||
            previous.mipmap_level_count != 1 ||
            previous.sample_count != 1 ||
            !sampleableTexture(scene.previous))
        {
            setError(error,
                     "Metal world lens-flare history is incompatible");
            return nullptr;
        }
    }

    ALMetalRenderTargetDescriptor descriptor;
    descriptor.width = 2;
    descriptor.height = 1;
    descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
    descriptor.colors[0].label = "Alchemy world lens-flare state";
    descriptor.label = "Alchemy native world lens-flare state target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    WorldLensFlareStateConstants constants{};
    constants.screen_time[0] = static_cast<float>(source.width);
    constants.screen_time[1] = static_cast<float>(source.height);
    constants.screen_time[2] = std::min(scene.delta_seconds, 0.25f);
    constants.screen_time[3] = scene.fade_time;
    constants.sun_visibility_radius[0] = scene.sun_uv[0];
    constants.sun_visibility_radius[1] = scene.sun_uv[1];
    constants.sun_visibility_radius[2] = scene.edge_visibility;
    constants.sun_visibility_radius[3] = scene.occlusion_radius;
    constants.config[0] = scene.reverse_z ? 1u : 0u;

    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world lens-flare state", owned_command, error);
    if (!command)
    {
        return nullptr;
    }
    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::dont_care;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world lens-flare state encoder", error)
        : nullptr;
    if (!encoder)
    {
        mImpl->finishCommandBuffer(owned_command, false, error);
        return nullptr;
    }
    const ALMetalViewport viewport{0.0, 0.0, 2.0, 1.0, 0.0, 1.0};
    const ALMetalScissorRect scissor{0, 0, 2, 1};
    const ALMetalTexture* previous = scene.previous
        ? scene.previous : mImpl->identity_lens_flare_state.get();
    const bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(*mImpl->lens_flare_state_pipeline, error) &&
        encoder->setFragmentTexture(
            0, scene.source->colorTexture(0), error) &&
        encoder->setFragmentTexture(1, scene.depth_texture, error) &&
        encoder->setFragmentTexture(2, previous, error) &&
        encoder->setFragmentSampler(
            0, mImpl->point_clamp_sampler.get(), error) &&
        encoder->setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended, error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderLensDirt(
    const ALMetalWorldLensDirtScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || scene.width == 0 ||
        scene.height == 0 || !std::isfinite(scene.seed) ||
        !std::isfinite(scene.grime) ||
        !std::isfinite(scene.mote_scale) ||
        !std::isfinite(scene.smudge) || !std::isfinite(scene.toe) ||
        !std::isfinite(scene.gain) || scene.seed < 0.f ||
        scene.seed > 999.f || scene.grime < 0.f || scene.grime > 2.f ||
        scene.mote_scale < 0.5f || scene.mote_scale > 2.f ||
        scene.smudge < 0.f || scene.smudge > 2.f ||
        scene.scratches > 32 || scene.toe < 0.6f || scene.toe > 4.f ||
        scene.gain < 0.5f || scene.gain > 2.5f)
    {
        setError(error, "Metal world lens-dirt scene is invalid");
        return nullptr;
    }

    ALMetalRenderTargetDescriptor descriptor;
    descriptor.width = scene.width;
    descriptor.height = scene.height;
    descriptor.colors[0].format = ALMetalPixelFormat::r8_unorm;
    descriptor.colors[0].label = "Alchemy world lens-dirt plate";
    descriptor.label = "Alchemy native world lens-dirt target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    WorldLensDirtConstants constants{};
    constants.screen_seed_grime[0] = static_cast<float>(scene.width);
    constants.screen_seed_grime[1] = static_cast<float>(scene.height);
    constants.screen_seed_grime[2] = scene.seed;
    constants.screen_seed_grime[3] = scene.grime;
    constants.detail_toe[0] = scene.mote_scale;
    constants.detail_toe[1] = scene.smudge;
    constants.detail_toe[2] = static_cast<float>(scene.scratches);
    constants.detail_toe[3] = scene.toe;
    constants.gain[0] = scene.gain;

    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native world lens-dirt generation", error);
    if (!command)
    {
        return nullptr;
    }
    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::dont_care;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world lens-dirt encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }
    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0};
    const ALMetalScissorRect scissor{
        0, 0, scene.width, scene.height};
    const bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(*mImpl->lens_dirt_pipeline, error) &&
        encoder->setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    if (!encoded || !encoder->end(error) || !command->commit(error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderDisplayPost(
    const ALMetalWorldDisplayPostScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.source)
    {
        setError(error, "Metal world display-post scene is incomplete");
        return nullptr;
    }

    const ALMetalPixelFormat source_format = scene.hdr
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rgba8_unorm;
    const ALMetalRenderTargetDescriptor& source =
        scene.source->descriptor();
    if (source.width == 0 || source.height == 0 ||
        source.sample_count != 1 || source.color_attachment_count != 1 ||
        source.colors[0].format != source_format ||
        !sampleableTexture(scene.source->colorTexture(0)))
    {
        setError(error, "Metal world display-post source is incompatible");
        return nullptr;
    }

    if (!std::isfinite(scene.legacy_gamma_exponent) ||
        !std::isfinite(scene.static_exposure) ||
        !std::isfinite(scene.adapted_exposure) ||
        !std::isfinite(scene.tonemap_mix) ||
        scene.legacy_gamma_exponent < 0.f ||
        scene.legacy_gamma_exponent > 20.f ||
        scene.static_exposure < 0.f || scene.adapted_exposure < 0.f ||
        scene.tonemap_mix < 0.f || scene.tonemap_mix > 1.f ||
        scene.tonemap_type > 6 ||
        !finiteValues(scene.tonemap_params, 4) ||
        !std::isfinite(scene.dither_amount) ||
        !std::isfinite(scene.bloom_strength) ||
        !std::isfinite(scene.halation_strength) ||
        !std::isfinite(scene.cross_filter_strength) ||
        !finiteValues(scene.halation_tint, 3) ||
        !std::isfinite(scene.chromatic_aberration_amount) ||
        !std::isfinite(
            scene.chromatic_aberration_reciprocal_falloff) ||
        !std::isfinite(scene.chromatic_aberration_anisotropy) ||
        !finiteValues(scene.chromatic_aberration_angle_sin_cos, 2) ||
        !finiteValues(scene.chromatic_aberration_offset_r, 2) ||
        !finiteValues(scene.chromatic_aberration_offset_b, 2) ||
        !std::isfinite(scene.lens_flare_strength) ||
        !finiteValues(scene.lens_flare_sun_uv, 2) ||
        !finiteValues(scene.lens_flare_light_color, 3) ||
        !std::isfinite(scene.lens_flare_sky_hdr_scale) ||
        !std::isfinite(scene.lens_flare_streak_length) ||
        !std::isfinite(scene.lens_flare_streak_falloff) ||
        !std::isfinite(scene.lens_flare_streak_width) ||
        !std::isfinite(scene.lens_flare_streak_intensity) ||
        !finiteValues(scene.lens_flare_streak_tint, 3) ||
        !std::isfinite(scene.lens_flare_chromatic_spread) ||
        !std::isfinite(scene.lens_flare_glow) ||
        !std::isfinite(scene.lens_flare_glow_radius) ||
        !std::isfinite(scene.lens_flare_glow_falloff) ||
        !std::isfinite(scene.lens_flare_ghost) ||
        !std::isfinite(scene.lens_flare_ghost_spacing) ||
        !std::isfinite(scene.lens_flare_halo) ||
        !std::isfinite(scene.lens_flare_halo_radius) ||
        !std::isfinite(scene.lens_flare_halo_width) ||
        !std::isfinite(scene.lens_flare_starburst) ||
        !std::isfinite(scene.lens_flare_starburst_sharpness) ||
        !std::isfinite(scene.lens_flare_starburst_falloff) ||
        !std::isfinite(scene.lens_dirt_strength) ||
        !std::isfinite(scene.lens_dirt_bloom_response) ||
        !std::isfinite(scene.lens_dirt_flare_response) ||
        scene.dither_amount < 0.f || scene.dither_amount > 1.f ||
        scene.bloom_strength < 0.f ||
        scene.halation_strength < 0.f ||
        scene.cross_filter_strength < 0.f ||
        scene.chromatic_aberration_amount < 0.f ||
        scene.chromatic_aberration_amount > 0.02f ||
        scene.chromatic_aberration_reciprocal_falloff < 0.25f ||
        scene.chromatic_aberration_reciprocal_falloff > 2.f ||
        scene.chromatic_aberration_anisotropy < -1.f ||
        scene.chromatic_aberration_anisotropy > 1.f ||
        std::abs(scene.chromatic_aberration_angle_sin_cos[0]) > 1.f ||
        std::abs(scene.chromatic_aberration_angle_sin_cos[1]) > 1.f ||
        std::abs(
            scene.chromatic_aberration_angle_sin_cos[0] *
                    scene.chromatic_aberration_angle_sin_cos[0] +
                scene.chromatic_aberration_angle_sin_cos[1] *
                    scene.chromatic_aberration_angle_sin_cos[1] -
                1.f) > 0.001f ||
        std::abs(scene.chromatic_aberration_offset_r[0]) > 1.f ||
        std::abs(scene.chromatic_aberration_offset_r[1]) > 1.f ||
        std::abs(scene.chromatic_aberration_offset_b[0]) > 1.f ||
        std::abs(scene.chromatic_aberration_offset_b[1]) > 1.f ||
        scene.lens_flare_strength < 0.f ||
        scene.lens_flare_strength > 1.f ||
        scene.lens_flare_sky_hdr_scale < 0.f ||
        scene.lens_flare_streak_length < 0.01f ||
        scene.lens_flare_streak_length > 2.f ||
        scene.lens_flare_streak_falloff < 0.1f ||
        scene.lens_flare_streak_falloff > 10.f ||
        scene.lens_flare_streak_width < 0.001f ||
        scene.lens_flare_streak_width > 0.05f ||
        scene.lens_flare_streak_intensity < 0.f ||
        scene.lens_flare_streak_intensity > 5.f ||
        scene.lens_flare_chromatic_spread < 0.f ||
        scene.lens_flare_chromatic_spread > 0.1f ||
        scene.lens_flare_glow < 0.f || scene.lens_flare_glow > 5.f ||
        scene.lens_flare_glow_radius < 0.01f ||
        scene.lens_flare_glow_radius > 0.5f ||
        scene.lens_flare_glow_falloff < 1.f ||
        scene.lens_flare_glow_falloff > 30.f ||
        scene.lens_flare_ghost < 0.f || scene.lens_flare_ghost > 5.f ||
        scene.lens_flare_ghost_count > 8 ||
        scene.lens_flare_ghost_spacing < 0.1f ||
        scene.lens_flare_ghost_spacing > 1.f ||
        scene.lens_flare_halo < 0.f || scene.lens_flare_halo > 5.f ||
        scene.lens_flare_halo_radius < 0.01f ||
        scene.lens_flare_halo_radius > 1.f ||
        scene.lens_flare_halo_width < 0.01f ||
        scene.lens_flare_halo_width > 0.5f ||
        scene.lens_flare_starburst < 0.f ||
        scene.lens_flare_starburst > 5.f ||
        scene.lens_flare_starburst_spikes < 1 ||
        scene.lens_flare_starburst_spikes > 32 ||
        scene.lens_flare_starburst_sharpness < 1.f ||
        scene.lens_flare_starburst_sharpness > 256.f ||
        scene.lens_flare_starburst_falloff < 3.809523f ||
        scene.lens_flare_starburst_falloff > 80.00001f ||
        (scene.lens_flare_strength > 0.f &&
         !scene.lens_flare_state_texture) ||
        scene.lens_dirt_strength < 0.f ||
        scene.lens_dirt_strength > 2.f ||
        scene.lens_dirt_bloom_response < 0.f ||
        scene.lens_dirt_bloom_response > 2.f ||
        scene.lens_dirt_flare_response < 0.f ||
        scene.lens_dirt_flare_response > 2.f ||
        (scene.lens_dirt_strength > 0.f &&
         !scene.lens_dirt_texture) ||
        (scene.preserve_alpha &&
         (!scene.hdr || !scene.cutout_premultiplied)) ||
        !validWorldColorGrade(scene.color_grade) ||
        (scene.bloom_texture == nullptr) !=
            (scene.bloom_strength == 0.f &&
             scene.halation_strength == 0.f) ||
        (scene.cross_filter_texture == nullptr) !=
            (scene.cross_filter_strength == 0.f) ||
        (scene.cross_filter_strength > 0.f &&
         (!scene.bloom_texture || scene.bloom_strength <= 0.f)) ||
        ((scene.tonemap_type == 2 || scene.tonemap_type == 3 ||
          scene.tonemap_type == 4) && scene.tonemap_params[0] <= 0.f) ||
        (scene.tonemap_type == 6 &&
         (scene.tonemap_params[0] <= 0.f ||
          scene.tonemap_params[1] <= 0.f ||
          scene.tonemap_params[2] <= 0.f ||
          scene.tonemap_params[3] <= 0.f)))
    {
        setError(error, "Metal world display-post state is invalid");
        return nullptr;
    }

    if (scene.adapted_exposure_texture)
    {
        const ALMetalTextureDescriptor& exposure =
            scene.adapted_exposure_texture->descriptor();
        if (!scene.hdr || exposure.width != 1 || exposure.height != 1 ||
            exposure.format != ALMetalPixelFormat::rgba16_float ||
            !sampleableTexture(scene.adapted_exposure_texture))
        {
            setError(error,
                     "Metal world display-post exposure is incompatible");
            return nullptr;
        }
    }

    if (scene.bloom_texture)
    {
        const ALMetalTextureDescriptor& bloom =
            scene.bloom_texture->descriptor();
        const bool bloom_format =
            bloom.format == ALMetalPixelFormat::rg11b10_float ||
            bloom.format == ALMetalPixelFormat::rgba16_float;
        if (!scene.hdr || bloom.width == 0 || bloom.height == 0 ||
            !bloom_format ||
            (scene.halation_strength > 0.f &&
             bloom.format != ALMetalPixelFormat::rgba16_float) ||
            !sampleableTexture(scene.bloom_texture))
        {
            setError(error,
                     "Metal world display-post bloom is incompatible");
            return nullptr;
        }
    }

    if (scene.cross_filter_texture)
    {
        const ALMetalTextureDescriptor& cross_filter =
            scene.cross_filter_texture->descriptor();
        if (!scene.hdr || cross_filter.width == 0 ||
            cross_filter.height == 0 ||
            cross_filter.format != ALMetalPixelFormat::rgba16_float ||
            !sampleableTexture(scene.cross_filter_texture))
        {
            setError(error,
                     "Metal world display-post cross-filter is incompatible");
            return nullptr;
        }
    }

    if (scene.lens_flare_state_texture)
    {
        const ALMetalTextureDescriptor& flare =
            scene.lens_flare_state_texture->descriptor();
        if (!scene.hdr || flare.type != ALMetalTextureType::texture_2d ||
            flare.width != 2 || flare.height != 1 ||
            flare.format != ALMetalPixelFormat::rgba16_float ||
            flare.array_length != 1 || flare.mipmap_level_count != 1 ||
            flare.sample_count != 1 ||
            !sampleableTexture(scene.lens_flare_state_texture))
        {
            setError(error,
                     "Metal world display-post lens flare is incompatible");
            return nullptr;
        }
    }

    if (scene.lens_dirt_texture)
    {
        const ALMetalTextureDescriptor& dirt =
            scene.lens_dirt_texture->descriptor();
        if (dirt.type != ALMetalTextureType::texture_2d ||
            dirt.width != source.width || dirt.height != source.height ||
            dirt.format != ALMetalPixelFormat::r8_unorm ||
            dirt.array_length != 1 || dirt.mipmap_level_count != 1 ||
            dirt.sample_count != 1 ||
            !sampleableTexture(scene.lens_dirt_texture))
        {
            setError(error,
                     "Metal world display-post lens dirt is incompatible");
            return nullptr;
        }
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = source.width;
    output_descriptor.height = source.height;
    output_descriptor.colors[0].format = scene.hdr && !scene.preserve_alpha
        ? ALMetalPixelFormat::rgb10a2_unorm
        : ALMetalPixelFormat::rgba8_unorm;
    output_descriptor.colors[0].label = scene.hdr && !scene.preserve_alpha
        ? "Alchemy HDR world RGB10_A2 post color"
        : (scene.preserve_alpha
            ? "Alchemy HDR world alpha-preserving RGBA8 post color"
            : "Alchemy world RGBA8 post color");
    output_descriptor.label = scene.preserve_alpha
        ? "Alchemy native HDR cutout display-post target"
        : (scene.hdr
            ? "Alchemy native HDR world display-post target"
            : "Alchemy native world display-post target");
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    WorldDisplayPostConstants constants{};
    constants.post[0] = scene.static_exposure;
    constants.post[1] = scene.adapted_exposure;
    constants.post[2] = scene.tonemap_mix;
    constants.post[3] = static_cast<float>(scene.tonemap_type);
    std::memcpy(constants.tonemap_params, scene.tonemap_params,
                sizeof(constants.tonemap_params));
    constants.display[0] = scene.dither_amount;
    constants.display[1] = scene.bloom_strength;
    constants.display[2] = scene.halation_strength;
    constants.display[3] = scene.cross_filter_strength;
    std::memcpy(constants.halation_tint, scene.halation_tint,
                sizeof(scene.halation_tint));
    constants.config[0] = scene.cutout_premultiplied ? 1u : 0u;
    constants.config[1] = (scene.hdr ? 1u : 0u) |
        (scene.legacy_gamma ? 2u : 0u) |
        (scene.bypass_tonemap ? 4u : 0u);
    constants.config[2] = scene.dither_animated ? 1u : 0u;
    constants.config[3] = scene.frame_id;
    constants.chromatic[0] = scene.chromatic_aberration_amount;
    constants.chromatic[1] =
        scene.chromatic_aberration_reciprocal_falloff;
    constants.chromatic[2] = scene.chromatic_aberration_anisotropy;
    constants.chromatic[3] = scene.legacy_gamma_exponent;
    std::memcpy(constants.chromatic_angle_offset_r,
                scene.chromatic_aberration_angle_sin_cos,
                sizeof(scene.chromatic_aberration_angle_sin_cos));
    std::memcpy(constants.chromatic_angle_offset_r + 2,
                scene.chromatic_aberration_offset_r,
                sizeof(scene.chromatic_aberration_offset_r));
    std::memcpy(constants.chromatic_offset_b,
                scene.chromatic_aberration_offset_b,
                sizeof(scene.chromatic_aberration_offset_b));
    constants.flare_sun_strength[0] = scene.lens_flare_sun_uv[0];
    constants.flare_sun_strength[1] = scene.lens_flare_sun_uv[1];
    constants.flare_sun_strength[2] = scene.lens_flare_strength;
    constants.flare_sun_strength[3] = scene.lens_flare_sky_hdr_scale;
    constants.flare_streak[0] = scene.lens_flare_streak_length;
    constants.flare_streak[1] = scene.lens_flare_streak_falloff;
    constants.flare_streak[2] = scene.lens_flare_streak_width;
    constants.flare_streak[3] = scene.lens_flare_streak_intensity;
    std::memcpy(constants.flare_streak_tint,
                scene.lens_flare_streak_tint,
                sizeof(scene.lens_flare_streak_tint));
    constants.flare_streak_tint[3] =
        scene.lens_flare_chromatic_spread;
    constants.flare_glow_ghost[0] = scene.lens_flare_glow;
    constants.flare_glow_ghost[1] = scene.lens_flare_glow_radius;
    constants.flare_glow_ghost[2] = scene.lens_flare_glow_falloff;
    constants.flare_glow_ghost[3] = scene.lens_flare_ghost;
    constants.flare_ghost_halo[0] =
        static_cast<float>(scene.lens_flare_ghost_count);
    constants.flare_ghost_halo[1] = scene.lens_flare_ghost_spacing;
    constants.flare_ghost_halo[2] = scene.lens_flare_halo;
    constants.flare_ghost_halo[3] = scene.lens_flare_halo_radius;
    constants.flare_halo_starburst[0] = scene.lens_flare_halo_width;
    constants.flare_halo_starburst[1] = scene.lens_flare_starburst;
    constants.flare_halo_starburst[2] =
        static_cast<float>(scene.lens_flare_starburst_spikes);
    constants.flare_halo_starburst[3] =
        scene.lens_flare_starburst_sharpness;
    constants.flare_star_light[0] = scene.lens_flare_starburst_falloff;
    std::memcpy(constants.flare_star_light + 1,
                scene.lens_flare_light_color,
                sizeof(scene.lens_flare_light_color));
    constants.lens_dirt[0] = scene.lens_dirt_strength;
    constants.lens_dirt[1] = scene.lens_dirt_bloom_response;
    constants.lens_dirt[2] = scene.lens_dirt_flare_response;
    const ALMetalWorldColorGrade& grade = scene.color_grade;
    std::memcpy(constants.grade_white_balance, grade.white_balance_gain,
                sizeof(grade.white_balance_gain));
    std::memcpy(constants.grade_lift, grade.lift, sizeof(grade.lift));
    std::memcpy(constants.grade_inverse_gamma, grade.inverse_gamma,
                sizeof(grade.inverse_gamma));
    std::memcpy(constants.grade_gain, grade.gain, sizeof(grade.gain));
    std::memcpy(constants.grade_shadow_ratio, grade.shadow_ratio,
                sizeof(grade.shadow_ratio));
    std::memcpy(constants.grade_midtone_ratio, grade.midtone_ratio,
                sizeof(grade.midtone_ratio));
    std::memcpy(constants.grade_highlight_ratio, grade.highlight_ratio,
                sizeof(grade.highlight_ratio));
    constants.grade_split0[0] = grade.midtone_amount;
    constants.grade_split0[1] = grade.split_amount;
    constants.grade_split0[2] = grade.shadow_ramp[0];
    constants.grade_split0[3] = grade.shadow_ramp[1];
    constants.grade_split1[0] = grade.highlight_ramp[0];
    constants.grade_split1[1] = grade.highlight_ramp[1];
    constants.grade_split1[2] = grade.black_white_scale;
    constants.grade_split1[3] = grade.black_white_bias;
    constants.grade_display0[0] = grade.brightness_contrast_scale;
    constants.grade_display0[1] = grade.brightness_contrast_bias;
    constants.grade_display0[2] = grade.highlights_scaled;
    constants.grade_display0[3] = grade.shadows_scaled;
    constants.grade_display1[0] = grade.saturation;
    constants.grade_display1[1] = grade.vibrance;
    constants.grade_display1[2] = grade.hue_shift;
    constants.grade_display1[3] = grade.enabled ? 1.f : 0.f;
    constants.grade_lut[0] = static_cast<float>(grade.color_lut_size);
    constants.grade_lut[1] = grade.color_lut_flip_green ? 1.f : 0.f;
    constants.grade_lut[2] = grade.color_lut_swap_blue_green ? 1.f : 0.f;
    constants.grade_lut[3] = grade.color_lut_strength;
    constants.grade_curve[0] = grade.tone_curve_scale;
    constants.grade_curve[1] = grade.tone_curve_bias;
    constants.grade_curve[2] = grade.tone_curve_amount;

    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world display post", owned_command, error);
    if (!command)
    {
        return nullptr;
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::dont_care;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world display-post encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(source.width),
        static_cast<double>(source.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{
        0, 0, source.width, source.height,
    };
    const std::size_t dynamic_exposure =
        scene.adapted_exposure_texture ? 1u : 0u;
    const std::size_t bloom = scene.bloom_texture ? 1u : 0u;
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(
            *mImpl->display_post_pipelines
                [scene.hdr ? 1 : 0][scene.preserve_alpha ? 1 : 0]
                [bloom][dynamic_exposure], error) &&
        encoder->setFragmentTexture(
            0, scene.source->colorTexture(0), error);
    if (encoded && scene.adapted_exposure_texture)
    {
        encoded = encoder->setFragmentTexture(
            1, scene.adapted_exposure_texture, error);
    }
    if (encoded && scene.bloom_texture)
    {
        const ALMetalTexture* cross_filter = scene.cross_filter_texture
            ? scene.cross_filter_texture : scene.bloom_texture;
        encoded = encoder->setFragmentTexture(
            2, scene.bloom_texture, error) &&
            encoder->setFragmentTexture(3, cross_filter, error) &&
            encoder->setFragmentSampler(
                1, mImpl->linear_mirror_sampler.get(), error) &&
            encoder->setFragmentSampler(
                2, mImpl->linear_clamp_sampler.get(), error);
    }
    encoded = encoded &&
        encoder->setFragmentSampler(
            0, scene.chromatic_aberration_amount > 0.f
                ? mImpl->linear_mirror_sampler.get()
                : mImpl->point_clamp_sampler.get(), error) &&
        encoder->setFragmentTexture(
            4, grade.enabled && grade.color_lut_strength > 0.f
                ? grade.color_lut_texture
                : mImpl->identity_color_lut.get(), error) &&
        encoder->setFragmentTexture(
            5, grade.enabled && grade.tone_curve_amount > 0.f
                ? grade.tone_curve_texture
                : mImpl->identity_tone_curve.get(), error) &&
        encoder->setFragmentTexture(
            6, scene.lens_flare_state_texture
                ? scene.lens_flare_state_texture
                : mImpl->identity_lens_flare_state.get(), error) &&
        encoder->setFragmentTexture(
            7, scene.lens_dirt_texture
                ? scene.lens_dirt_texture
                : mImpl->identity_lens_dirt.get(), error) &&
        encoder->setFragmentSampler(
            3, mImpl->linear_clamp_sampler.get(), error) &&
        encoder->setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        encoder->draw(
            ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended, error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderPresentationPost(
    const ALMetalWorldPresentationScene& scene,
    std::string* error)
{
    const auto finite = [](const float* values, std::size_t count)
    {
        for (std::size_t index = 0; index < count; ++index)
        {
            if (!std::isfinite(values[index]))
            {
                return false;
            }
        }
        return true;
    };
    const auto unit_color = [&finite](const float* values)
    {
        return finite(values, 3) &&
            values[0] >= 0.f && values[0] <= 1.f &&
            values[1] >= 0.f && values[1] <= 1.f &&
            values[2] >= 0.f && values[2] <= 1.f;
    };
    if (!mImpl || !mImpl->device || !scene.source ||
        scene.reference_mode > 2 || scene.cvd_mode > 3 ||
        scene.grain_style > 3 || scene.preview_mode > 7 ||
        (scene.dither_bits != 8 && scene.dither_bits != 10) ||
        !std::isfinite(scene.reference_position) ||
        scene.reference_position < 0.f || scene.reference_position > 1.f ||
        !std::isfinite(scene.lens_distortion_amount) ||
        scene.lens_distortion_amount < 0.f ||
        scene.lens_distortion_amount > 1.f ||
        !finite(scene.lens_distortion_k, 2) ||
        scene.lens_distortion_k[0] < -0.5f ||
        scene.lens_distortion_k[0] > 0.5f ||
        scene.lens_distortion_k[1] < -0.25f ||
        scene.lens_distortion_k[1] > 0.25f ||
        !std::isfinite(scene.lens_distortion_scale) ||
        scene.lens_distortion_scale < 0.1f ||
        scene.lens_distortion_scale > 10.f ||
        !finite(scene.lens_distortion_squeeze, 2) ||
        scene.lens_distortion_squeeze[0] < 0.4f ||
        scene.lens_distortion_squeeze[0] > 2.f ||
        scene.lens_distortion_squeeze[1] < 0.4f ||
        scene.lens_distortion_squeeze[1] > 2.f ||
        !finite(scene.lens_distortion_center, 2) ||
        scene.lens_distortion_center[0] < -0.5f ||
        scene.lens_distortion_center[0] > 0.5f ||
        scene.lens_distortion_center[1] < -0.5f ||
        scene.lens_distortion_center[1] > 0.5f ||
        !finite(scene.lens_distortion_tangential, 2) ||
        scene.lens_distortion_tangential[0] < -0.05f ||
        scene.lens_distortion_tangential[0] > 0.05f ||
        scene.lens_distortion_tangential[1] < -0.05f ||
        scene.lens_distortion_tangential[1] > 0.05f ||
        !std::isfinite(scene.vignette_amount) ||
        scene.vignette_amount < 0.f || scene.vignette_amount > 1.f ||
        !std::isfinite(scene.vignette_radius) ||
        scene.vignette_radius < 0.25f || scene.vignette_radius > 1.5f ||
        !std::isfinite(scene.vignette_softness) ||
        scene.vignette_softness < 0.05f ||
        scene.vignette_softness > 1.f ||
        !std::isfinite(scene.vignette_shape) ||
        scene.vignette_shape < 0.f || scene.vignette_shape > 1.f ||
        !unit_color(scene.vignette_outer_color) ||
        !unit_color(scene.vignette_middle_color) ||
        !std::isfinite(scene.vignette_middle_point) ||
        scene.vignette_middle_point < 0.f ||
        scene.vignette_middle_point > 1.f ||
        !finite(scene.vignette_center, 2) ||
        scene.vignette_center[0] < -0.5f ||
        scene.vignette_center[0] > 0.5f ||
        scene.vignette_center[1] < -0.5f ||
        scene.vignette_center[1] > 0.5f ||
        !finite(scene.vignette_aspect, 2) ||
        scene.vignette_aspect[0] <= 0.f ||
        scene.vignette_aspect[1] <= 0.f ||
        !std::isfinite(scene.vignette_feather) ||
        scene.vignette_feather < 0.2f || scene.vignette_feather > 4.f ||
        !std::isfinite(scene.cvd_amount) ||
        scene.cvd_amount < 0.f || scene.cvd_amount > 1.f ||
        !std::isfinite(scene.grain_amount) ||
        scene.grain_amount < 0.f || scene.grain_amount > 1.f ||
        !std::isfinite(scene.grain_size) ||
        scene.grain_size < 1.f || scene.grain_size > 8.f ||
        !std::isfinite(scene.grain_range) ||
        scene.grain_range < 0.f || scene.grain_range > 1.f ||
        !unit_color(scene.grain_tint) ||
        !std::isfinite(scene.dither_amount) ||
        scene.dither_amount < 0.f || scene.dither_amount > 1.f)
    {
        setError(error, "Metal world presentation scene is invalid");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& source =
        scene.source->descriptor();
    const ALMetalPixelFormat source_format = source.colors[0].format;
    if (source.width == 0 || source.height == 0 ||
        source.sample_count != 1 || source.color_attachment_count != 1 ||
        (source_format != ALMetalPixelFormat::rgb10a2_unorm &&
         source_format != ALMetalPixelFormat::rgba8_unorm) ||
        !sampleableTexture(scene.source->colorTexture(0)))
    {
        setError(error, "Metal world presentation source is incompatible");
        return nullptr;
    }
    if (scene.reference_mode != 0)
    {
        if (!scene.reference_texture)
        {
            setError(error, "Metal world presentation reference is missing");
            return nullptr;
        }
        const ALMetalTextureDescriptor& reference =
            scene.reference_texture->descriptor();
        if (reference.type != ALMetalTextureType::texture_2d ||
            reference.width != source.width ||
            reference.height != source.height ||
            reference.format != source_format ||
            reference.array_length != 1 ||
            reference.mipmap_level_count != 1 ||
            reference.sample_count != 1 ||
            !sampleableTexture(scene.reference_texture))
        {
            setError(error,
                     "Metal world presentation reference is incompatible");
            return nullptr;
        }
    }

    WorldPresentationConstants constants{};
    constants.lens_primary[0] = scene.lens_distortion_amount;
    constants.lens_primary[1] = scene.lens_distortion_k[0];
    constants.lens_primary[2] = scene.lens_distortion_k[1];
    constants.lens_primary[3] = scene.lens_distortion_scale;
    std::memcpy(constants.lens_squeeze_center,
                scene.lens_distortion_squeeze,
                sizeof(scene.lens_distortion_squeeze));
    std::memcpy(constants.lens_squeeze_center + 2,
                scene.lens_distortion_center,
                sizeof(scene.lens_distortion_center));
    std::memcpy(constants.lens_tangent_reference,
                scene.lens_distortion_tangential,
                sizeof(scene.lens_distortion_tangential));
    constants.lens_tangent_reference[2] = scene.reference_position;
    constants.vignette_primary[0] = scene.vignette_amount;
    constants.vignette_primary[1] = scene.vignette_radius;
    constants.vignette_primary[2] = scene.vignette_softness;
    constants.vignette_primary[3] = scene.vignette_shape;
    std::memcpy(constants.vignette_outer_middle,
                scene.vignette_outer_color,
                sizeof(scene.vignette_outer_color));
    constants.vignette_outer_middle[3] = scene.vignette_middle_point;
    std::memcpy(constants.vignette_middle_feather,
                scene.vignette_middle_color,
                sizeof(scene.vignette_middle_color));
    constants.vignette_middle_feather[3] = scene.vignette_feather;
    std::memcpy(constants.vignette_center_aspect,
                scene.vignette_center,
                sizeof(scene.vignette_center));
    std::memcpy(constants.vignette_center_aspect + 2,
                scene.vignette_aspect,
                sizeof(scene.vignette_aspect));
    constants.effects[0] = scene.cvd_amount;
    constants.effects[1] = scene.grain_amount;
    constants.effects[2] = scene.grain_size;
    constants.effects[3] = scene.grain_range;
    std::memcpy(constants.grain_tint_dither, scene.grain_tint,
                sizeof(scene.grain_tint));
    constants.grain_tint_dither[3] = scene.dither_amount;
    constants.config[0] = scene.reference_mode;
    constants.config[1] = scene.cvd_mode;
    constants.config[2] = scene.grain_style;
    constants.config[3] = scene.preview_mode;
    constants.noise[0] = scene.frame_id;
    constants.noise[1] = scene.grain_animated ? 1u : 0u;
    constants.noise[2] = scene.dither_bits;
    constants.noise[3] = scene.dither_animated ? 1u : 0u;

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = source.width;
    output_descriptor.height = source.height;
    output_descriptor.colors[0].format = ALMetalPixelFormat::bgra8_unorm;
    output_descriptor.colors[0].label =
        "Alchemy final BGRA8 presentation color";
    output_descriptor.label =
        "Alchemy native world final-presentation target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world final presentation", owned_command, error);
    if (!command)
    {
        return nullptr;
    }
    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::dont_care;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world final-presentation encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }
    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(source.width),
        static_cast<double>(source.height), 0.0, 1.0};
    const ALMetalScissorRect scissor{
        0, 0, source.width, source.height};
    const ALMetalTexture* reference = scene.reference_mode != 0
        ? scene.reference_texture : scene.source->colorTexture(0);
    const bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(*mImpl->presentation_post_pipeline, error) &&
        encoder->setFragmentTexture(
            0, scene.source->colorTexture(0), error) &&
        encoder->setFragmentTexture(1, reference, error) &&
        encoder->setFragmentSampler(
            0, mImpl->linear_mirror_sampler.get(), error) &&
        encoder->setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended, error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderDepthOfField(
    const ALMetalWorldDepthOfFieldScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.source ||
        !scene.depth_texture)
    {
        setError(error, "Metal world depth-of-field scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& source =
        scene.source->descriptor();
    const ALMetalTextureDescriptor& depth =
        scene.depth_texture->descriptor();
    const bool depth_format =
        depth.format == ALMetalPixelFormat::depth32_float ||
        depth.format == ALMetalPixelFormat::depth32_float_stencil8;
    if (source.width == 0 || source.height == 0 ||
        source.sample_count != 1 || source.color_attachment_count != 1 ||
        source.colors[0].format != ALMetalPixelFormat::rgba16_float ||
        !sampleableTexture(scene.source->colorTexture(0)) ||
        !sampleableTexture(scene.depth_texture) || !depth_format ||
        depth.width != source.width || depth.height != source.height ||
        depth.array_length != 1 || depth.mipmap_level_count != 1 ||
        depth.sample_count != 1)
    {
        setError(error, "Metal world depth-of-field inputs are incompatible");
        return nullptr;
    }

    constexpr float pi = 3.14159265358979323846f;
    if (!finiteValues(scene.inverse_projection, 16) ||
        !std::isfinite(scene.focal_distance) ||
        !std::isfinite(scene.blur_constant) ||
        !std::isfinite(scene.tan_pixel_angle) ||
        !std::isfinite(scene.magnification) ||
        !std::isfinite(scene.max_cof) ||
        !std::isfinite(scene.resolution_scale) ||
        !std::isfinite(scene.highlight_threshold) ||
        !std::isfinite(scene.highlight_gain) ||
        !std::isfinite(scene.highlight_clamp) ||
        !std::isfinite(scene.aperture_rotation) ||
        !std::isfinite(scene.aperture_curvature) ||
        !std::isfinite(scene.anamorphic_squeeze) ||
        !std::isfinite(scene.cat_eye) ||
        !std::isfinite(scene.fringe_amount) ||
        !finiteValues(scene.fringe_near_tint, 3) ||
        !finiteValues(scene.fringe_far_tint, 3) ||
        !std::isfinite(scene.spherical_aberration) ||
        !std::isfinite(scene.field_stretch) ||
        !std::isfinite(scene.field_falloff) ||
        !std::isfinite(scene.coma_asymmetry) ||
        scene.focal_distance >= 0.f || scene.blur_constant <= 0.f ||
        scene.tan_pixel_angle <= 0.f || scene.magnification <= 0.f ||
        scene.max_cof <= 0.f || scene.resolution_scale <= 0.f ||
        scene.resolution_scale > 1.f || scene.highlight_threshold < 0.f ||
        scene.highlight_gain < 0.f || scene.highlight_clamp < 0.f ||
        scene.aperture_blades < 0 || scene.aperture_blades > 11 ||
        scene.aperture_rotation < 0.f ||
        scene.aperture_rotation > 2.f * pi ||
        scene.aperture_curvature < 0.f ||
        scene.aperture_curvature > 1.f ||
        scene.anamorphic_squeeze < 0.25f ||
        scene.anamorphic_squeeze > 4.f ||
        scene.cat_eye < 0.f || scene.cat_eye > 1.5f ||
        scene.fringe_amount < 0.f || scene.fringe_amount > 1.f ||
        scene.spherical_aberration < -1.f ||
        scene.spherical_aberration > 1.f ||
        scene.field_stretch < -1.f || scene.field_stretch > 1.f ||
        scene.field_falloff < 1.f || scene.field_falloff > 4.f ||
        scene.coma_asymmetry < 0.f || scene.coma_asymmetry > 1.f)
    {
        setError(error, "Metal world depth-of-field state is invalid");
        return nullptr;
    }

    const bool anamorphic = scene.anamorphic_squeeze < 0.999f ||
        scene.anamorphic_squeeze > 1.001f;
    const bool shaped = scene.aperture_blades >= 3 ||
        scene.cat_eye > 0.f || scene.fringe_amount > 0.f || anamorphic ||
        scene.spherical_aberration != 0.f ||
        scene.field_stretch != 0.f || scene.coma_asymmetry > 0.f;
    const std::uint32_t blur_width = std::max(
        1u, static_cast<std::uint32_t>(
                static_cast<float>(source.width) * scene.resolution_scale));
    const std::uint32_t blur_height = std::max(
        1u, static_cast<std::uint32_t>(
                static_cast<float>(source.height) * scene.resolution_scale));
    const float resolution_scale = std::min(
        static_cast<float>(blur_width) / static_cast<float>(source.width),
        static_cast<float>(blur_height) / static_cast<float>(source.height));

    ALMetalRenderTargetDescriptor blur_descriptor;
    blur_descriptor.width = blur_width;
    blur_descriptor.height = blur_height;
    blur_descriptor.colors[0].format = scene.rgba16_blur
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rg11b10_float;
    blur_descriptor.colors[0].shader_read = true;
    blur_descriptor.colors[0].label =
        "Alchemy native world depth-of-field blur color";
    blur_descriptor.label =
        "Alchemy native world depth-of-field blur target";
    auto blur = ALMetalRenderTarget::create(
        *mImpl->device, blur_descriptor, error);
    if (!blur)
    {
        return nullptr;
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = source.width;
    output_descriptor.height = source.height;
    output_descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
    output_descriptor.colors[0].shader_read = true;
    output_descriptor.colors[0].label =
        "Alchemy native world defocused HDR color";
    output_descriptor.label =
        "Alchemy native world depth-of-field target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    WorldDepthOfFieldConstants constants{};
    std::memcpy(constants.inverse_projection, scene.inverse_projection,
                sizeof(constants.inverse_projection));
    constants.focus[0] = scene.focal_distance;
    constants.focus[1] = scene.blur_constant;
    constants.focus[2] = scene.tan_pixel_angle;
    constants.focus[3] = scene.magnification;
    constants.limits[0] = scene.max_cof;
    constants.limits[1] = resolution_scale;
    constants.limits[2] = scene.highlight_threshold;
    constants.limits[3] = scene.highlight_gain;
    constants.shape0[0] = scene.highlight_clamp;
    constants.shape0[1] = scene.aperture_rotation;
    constants.shape0[2] = scene.aperture_curvature;
    constants.shape0[3] = scene.cat_eye;
    constants.shape1[0] = scene.fringe_amount;
    constants.shape1[1] = scene.spherical_aberration;
    constants.shape1[2] = scene.field_stretch;
    constants.shape1[3] = scene.field_falloff;
    constants.shape2[0] = scene.coma_asymmetry;
    const float sides = static_cast<float>(
        std::max(scene.aperture_blades, 3));
    constants.shape2[1] = pi / sides;
    constants.shape2[2] = 2.f * constants.shape2[1];
    constants.shape2[3] = std::cos(constants.shape2[1]);
    const float anamorphic_root = std::sqrt(scene.anamorphic_squeeze);
    constants.anamorphic[0] = 1.f / anamorphic_root;
    constants.anamorphic[1] = anamorphic_root;
    std::memcpy(constants.fringe_near, scene.fringe_near_tint,
                sizeof(scene.fringe_near_tint));
    std::memcpy(constants.fringe_far, scene.fringe_far_tint,
                sizeof(scene.fringe_far_tint));
    constants.config[0] = scene.reverse_z ? 1u : 0u;
    constants.config[1] = scene.near_blur ? 1u : 0u;
    constants.config[2] = shaped ? 1u : 0u;
    constants.config[3] = static_cast<std::uint32_t>(
        scene.aperture_blades);

    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world depth of field", owned_command, error);
    if (!command)
    {
        return nullptr;
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::dont_care;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    auto gather_pass = blur->makeRenderPass(pass_config, error);
    auto gather = gather_pass ? ALMetalRenderEncoder::begin(
        *command, *gather_pass,
        "Alchemy world depth-of-field gather encoder", error) : nullptr;
    if (!gather)
    {
        mImpl->finishCommandBuffer(owned_command, false, error);
        return nullptr;
    }
    const std::size_t gather_pipeline =
        (scene.rgba16_blur ? 4u : 0u) +
        (shaped ? 2u : 0u) + (scene.near_blur ? 1u : 0u);
    const bool gathered =
        gather->setViewport(
            {0.0, 0.0, static_cast<double>(blur_width),
             static_cast<double>(blur_height), 0.0, 1.0}, error) &&
        gather->setScissor(
            {0, 0, blur_width, blur_height}, error) &&
        gather->setCullMode(ALMetalCullMode::none, error) &&
        gather->setPipeline(
            *mImpl->depth_of_field_gather_pipelines[gather_pipeline],
            error) &&
        gather->setFragmentTexture(
            0, scene.source->colorTexture(0), error) &&
        gather->setFragmentTexture(1, scene.depth_texture, error) &&
        gather->setFragmentSampler(
            0, mImpl->point_mirror_sampler.get(), error) &&
        gather->setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        gather->draw(
            ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
        gather->end(error);
    if (!gathered)
    {
        mImpl->finishCommandBuffer(owned_command, false, error);
        return nullptr;
    }

    auto combine_pass = output->makeRenderPass(pass_config, error);
    auto combine = combine_pass ? ALMetalRenderEncoder::begin(
        *command, *combine_pass,
        "Alchemy world depth-of-field combine encoder", error) : nullptr;
    if (!combine)
    {
        mImpl->finishCommandBuffer(owned_command, false, error);
        return nullptr;
    }
    const bool combined =
        combine->setViewport(
            {0.0, 0.0, static_cast<double>(source.width),
             static_cast<double>(source.height), 0.0, 1.0}, error) &&
        combine->setScissor(
            {0, 0, source.width, source.height}, error) &&
        combine->setCullMode(ALMetalCullMode::none, error) &&
        combine->setPipeline(
            *mImpl->depth_of_field_combine_pipelines
                [scene.near_blur ? 1u : 0u], error) &&
        combine->setFragmentTexture(0, blur->colorTexture(0), error) &&
        combine->setFragmentTexture(
            1, scene.source->colorTexture(0), error) &&
        combine->setFragmentTexture(2, scene.depth_texture, error) &&
        combine->setFragmentSampler(
            0, mImpl->point_mirror_sampler.get(), error) &&
        combine->setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        combine->draw(
            ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
        combine->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, combined, error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderCAS(
    const ALMetalRenderTarget& source,
    float sharpness,
    std::string* error)
{
    if (!mImpl || !mImpl->device)
    {
        setError(error, "Metal world CAS is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input = source.descriptor();
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        input.color_attachment_count != 1 ||
        input.colors[0].format != ALMetalPixelFormat::rgb10a2_unorm ||
        !sampleableTexture(source.colorTexture(0)))
    {
        setError(error, "Metal world CAS source is incompatible");
        return nullptr;
    }
    if (!std::isfinite(sharpness) || sharpness <= 0.f)
    {
        setError(error, "Metal world CAS state is invalid");
        return nullptr;
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = input.width;
    output_descriptor.height = input.height;
    output_descriptor.colors[0].format =
        ALMetalPixelFormat::rgb10a2_unorm;
    output_descriptor.colors[0].label =
        "Alchemy world CAS RGB10_A2 color";
    output_descriptor.label = "Alchemy native world CAS target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    WorldCASConstants constants{};
    const float clamped_sharpness = std::clamp(sharpness, 0.f, 1.f);
    constants.params[0] = -1.f /
        (8.f + (5.f - 8.f) * clamped_sharpness);

    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world CAS", owned_command, error);
    if (!command)
    {
        return nullptr;
    }
    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::dont_care;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world CAS encoder", error) : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(input.width),
        static_cast<double>(input.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, input.width, input.height};
    const bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(*mImpl->cas_pipeline, error) &&
        encoder->setFragmentTexture(0, source.colorTexture(0), error) &&
        encoder->setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended, error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderFXAA(
    const ALMetalRenderTarget& source,
    std::uint32_t quality,
    std::string* error)
{
    if (!mImpl || !mImpl->device)
    {
        setError(error, "Metal world FXAA is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input = source.descriptor();
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        input.color_attachment_count != 1 ||
        input.colors[0].format != ALMetalPixelFormat::rgb10a2_unorm ||
        !sampleableTexture(source.colorTexture(0)))
    {
        setError(error, "Metal world FXAA source is incompatible");
        return nullptr;
    }
    if (quality > 3)
    {
        setError(error, "Metal world FXAA state is invalid");
        return nullptr;
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = input.width;
    output_descriptor.height = input.height;
    output_descriptor.colors[0].format =
        ALMetalPixelFormat::rgb10a2_unorm;
    output_descriptor.colors[0].label =
        "Alchemy world FXAA RGB10_A2 color";
    output_descriptor.label = "Alchemy native world FXAA target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    WorldFXAAConstants constants{};
    constants.config[0] = quality;
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world FXAA", owned_command, error);
    if (!command)
    {
        return nullptr;
    }
    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::dont_care;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world FXAA encoder", error) : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(input.width),
        static_cast<double>(input.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, input.width, input.height};
    const bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(*mImpl->fxaa_pipeline, error) &&
        encoder->setFragmentTexture(0, source.colorTexture(0), error) &&
        encoder->setFragmentSampler(
            0, mImpl->linear_mirror_sampler.get(), error) &&
        encoder->setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended, error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderSMAAEdges(
    const ALMetalWorldSMAAEdgeScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.source)
    {
        setError(error, "Metal world SMAA edge scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input =
        scene.source->descriptor();
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        input.color_attachment_count != 1 ||
        input.colors[0].format != ALMetalPixelFormat::rgb10a2_unorm ||
        !sampleableTexture(scene.source->colorTexture(0)))
    {
        setError(error, "Metal world SMAA edge source is incompatible");
        return nullptr;
    }
    if (scene.quality > 3 ||
        !std::isfinite(scene.predication_threshold) ||
        !std::isfinite(scene.predication_scale) ||
        !std::isfinite(scene.predication_strength) ||
        scene.predication_scale < 1.f || scene.predication_scale > 5.f ||
        scene.predication_strength < 0.f ||
        scene.predication_strength > 1.f)
    {
        setError(error, "Metal world SMAA edge state is invalid");
        return nullptr;
    }

    if (scene.predication_texture)
    {
        const ALMetalTextureDescriptor& predication =
            scene.predication_texture->descriptor();
        const bool format =
            predication.format == ALMetalPixelFormat::depth32_float ||
            predication.format ==
                ALMetalPixelFormat::depth32_float_stencil8;
        if (predication.width != input.width ||
            predication.height != input.height ||
            predication.sample_count != 1 || !format ||
            !sampleableTexture(scene.predication_texture))
        {
            setError(error,
                     "Metal world SMAA predication texture is incompatible");
            return nullptr;
        }
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = input.width;
    output_descriptor.height = input.height;
    output_descriptor.colors[0].format =
        ALMetalPixelFormat::rgba8_unorm;
    output_descriptor.colors[0].label =
        "Alchemy world SMAA RGBA8 edges";
    output_descriptor.label = "Alchemy native world SMAA edge target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    WorldSMAAEdgeConstants constants{};
    constants.predication[0] = scene.predication_threshold;
    constants.predication[1] = scene.predication_scale;
    constants.predication[2] = scene.predication_strength;
    constants.config[0] = scene.quality;
    constants.config[1] = scene.predication_texture ? 1u : 0u;
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world SMAA edges", owned_command, error);
    if (!command)
    {
        return nullptr;
    }
    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {0.f, 0.f, 0.f, 0.f};
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world SMAA edge encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(input.width),
        static_cast<double>(input.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, input.width, input.height};
    const std::size_t predication = scene.predication_texture ? 1u : 0u;
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(
            *mImpl->smaa_edge_pipelines[predication], error) &&
        encoder->setFragmentTexture(
            0, scene.source->colorTexture(0), error) &&
        encoder->setFragmentSampler(
            0, mImpl->linear_clamp_sampler.get(), error);
    if (encoded && scene.predication_texture)
    {
        encoded = encoder->setFragmentTexture(
            1, scene.predication_texture, error) &&
            encoder->setFragmentSampler(
                1, mImpl->point_clamp_sampler.get(), error);
    }
    encoded = encoded &&
        encoder->setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended, error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderSMAABlendWeights(
    const ALMetalWorldSMAAWeightScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.edges ||
        !mImpl->smaa_area_texture || !mImpl->smaa_search_texture)
    {
        setError(error, "Metal world SMAA weight scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input =
        scene.edges->descriptor();
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        input.color_attachment_count != 1 ||
        input.colors[0].format != ALMetalPixelFormat::rgba8_unorm ||
        !sampleableTexture(scene.edges->colorTexture(0)))
    {
        setError(error, "Metal world SMAA edge target is incompatible");
        return nullptr;
    }
    if (scene.quality > 3)
    {
        setError(error, "Metal world SMAA weight state is invalid");
        return nullptr;
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = input.width;
    output_descriptor.height = input.height;
    output_descriptor.colors[0].format =
        ALMetalPixelFormat::rgba8_unorm;
    output_descriptor.colors[0].label =
        "Alchemy world SMAA RGBA8 blending weights";
    output_descriptor.label = "Alchemy native world SMAA weight target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    WorldSMAAWeightConstants constants{};
    constants.config[0] = scene.quality;
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world SMAA blending weights", owned_command,
        error);
    if (!command)
    {
        return nullptr;
    }
    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {0.f, 0.f, 0.f, 0.f};
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world SMAA weight encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(input.width),
        static_cast<double>(input.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, input.width, input.height};
    const bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(*mImpl->smaa_weight_pipeline, error) &&
        encoder->setFragmentTexture(
            0, scene.edges->colorTexture(0), error) &&
        encoder->setFragmentTexture(
            1, mImpl->smaa_area_texture.get(), error) &&
        encoder->setFragmentTexture(
            2, mImpl->smaa_search_texture.get(), error) &&
        encoder->setFragmentSampler(
            0, mImpl->linear_clamp_sampler.get(), error) &&
        encoder->setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended, error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderSMAANeighborhood(
    const ALMetalWorldSMAANeighborhoodScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.source || !scene.weights)
    {
        setError(error, "Metal world SMAA neighborhood scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& source =
        scene.source->descriptor();
    const ALMetalRenderTargetDescriptor& weights =
        scene.weights->descriptor();
    if (source.width == 0 || source.height == 0 ||
        source.sample_count != 1 || source.color_attachment_count != 1 ||
        source.colors[0].format != ALMetalPixelFormat::rgb10a2_unorm ||
        !sampleableTexture(scene.source->colorTexture(0)))
    {
        setError(error, "Metal world SMAA neighborhood source is incompatible");
        return nullptr;
    }
    if (weights.width != source.width || weights.height != source.height ||
        weights.sample_count != 1 || weights.color_attachment_count != 1 ||
        weights.colors[0].format != ALMetalPixelFormat::rgba8_unorm ||
        !sampleableTexture(scene.weights->colorTexture(0)))
    {
        setError(error, "Metal world SMAA weight target is incompatible");
        return nullptr;
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = source.width;
    output_descriptor.height = source.height;
    output_descriptor.colors[0].format =
        ALMetalPixelFormat::rgb10a2_unorm;
    output_descriptor.colors[0].label =
        "Alchemy world SMAA RGB10_A2 neighborhood output";
    output_descriptor.label =
        "Alchemy native world SMAA neighborhood target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world SMAA neighborhood", owned_command, error);
    if (!command)
    {
        return nullptr;
    }
    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::dont_care;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world SMAA neighborhood encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(source.width),
        static_cast<double>(source.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{
        0, 0, source.width, source.height,
    };
    const bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(*mImpl->smaa_neighborhood_pipeline, error) &&
        encoder->setFragmentTexture(
            0, scene.source->colorTexture(0), error) &&
        encoder->setFragmentTexture(
            1, scene.weights->colorTexture(0), error) &&
        encoder->setFragmentSampler(
            0, mImpl->linear_clamp_sampler.get(), error) &&
        encoder->setFragmentSampler(
            1, mImpl->linear_mirror_sampler.get(), error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended, error))
    {
        return nullptr;
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderSMAA(
    const ALMetalWorldSMAAScene& scene,
    std::string* error)
{
    ALMetalWorldSMAAEdgeScene edge_scene;
    edge_scene.source = scene.source;
    edge_scene.predication_texture = scene.predication_texture;
    edge_scene.quality = scene.quality;
    edge_scene.predication_threshold = scene.predication_threshold;
    edge_scene.predication_scale = scene.predication_scale;
    edge_scene.predication_strength = scene.predication_strength;
    auto edges = renderSMAAEdges(edge_scene, error);
    if (!edges)
    {
        return nullptr;
    }

    ALMetalWorldSMAAWeightScene weight_scene;
    weight_scene.edges = edges.get();
    weight_scene.quality = scene.quality;
    auto weights = renderSMAABlendWeights(weight_scene, error);
    if (!weights)
    {
        return nullptr;
    }

    ALMetalWorldSMAANeighborhoodScene neighborhood_scene;
    neighborhood_scene.source = scene.source;
    neighborhood_scene.weights = weights.get();
    return renderSMAANeighborhood(neighborhood_scene, error);
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderDisplaySample(
    const ALMetalRenderTarget& source,
    std::uint32_t width,
    std::uint32_t height,
    std::string* error)
{
    if (!mImpl || !mImpl->device || width == 0 || height == 0)
    {
        setError(error, "Metal world display sample is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input = source.descriptor();
    const ALMetalPixelFormat format = input.colors[0].format;
    if (input.width == 0 || input.height == 0 ||
        input.sample_count != 1 || input.color_attachment_count != 1 ||
        (format != ALMetalPixelFormat::rgb10a2_unorm &&
         format != ALMetalPixelFormat::rgba8_unorm) ||
        !sampleableTexture(source.colorTexture(0)))
    {
        setError(error, "Metal world display sample source is incompatible");
        return nullptr;
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = width;
    output_descriptor.height = height;
    output_descriptor.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
    output_descriptor.colors[0].label =
        "Alchemy world RGBA8 display sample";
    output_descriptor.label =
        "Alchemy native world display-sample target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native world display sample", error);
    if (!command)
    {
        return nullptr;
    }
    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::dont_care;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world display-sample encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(width),
        static_cast<double>(height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, width, height};
    const bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(*mImpl->display_sample_pipeline, error) &&
        encoder->setFragmentTexture(0, source.colorTexture(0), error) &&
        encoder->setFragmentSampler(
            0, mImpl->point_clamp_sampler.get(), error) &&
        encoder->draw(
            ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    if (!encoded || !encoder->end(error) || !command->commit(error))
    {
        return nullptr;
    }
    return output;
}

bool ALMetalWorldGBufferRenderer::readSceneColorPixel(
    const ALMetalRenderTarget& source,
    std::uint32_t x,
    std::uint32_t y,
    float color[4],
    std::string* error) const
{
    if (!mImpl || !mImpl->device || !color)
    {
        setError(error, "Metal world scene-pixel reader is incomplete");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = source.descriptor();
    const ALMetalPixelFormat format = descriptor.colors[0].format;
    const ALMetalTexture* texture = source.colorTexture(0);
    if (descriptor.width == 0 || descriptor.height == 0 ||
        descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 1 ||
        (format != ALMetalPixelFormat::rgba8_unorm &&
         format != ALMetalPixelFormat::rgba16_float) ||
        !texture || x >= descriptor.width || y >= descriptor.height)
    {
        setError(error, "Metal world scene-pixel source is incompatible");
        return false;
    }

    const ALMetalRegion2D region{x, y, 1, 1};
    float decoded[4]{};
    if (format == ALMetalPixelFormat::rgba8_unorm)
    {
        std::array<std::uint8_t, 4> pixel{};
        if (!texture->readback(
                *mImpl->device, 0, 0, region, pixel.data(), pixel.size(),
                error))
        {
            return false;
        }
        constexpr float byte_scale = 1.f / 255.f;
        for (std::size_t channel = 0; channel < pixel.size(); ++channel)
        {
            decoded[channel] = static_cast<float>(pixel[channel]) *
                byte_scale;
        }
    }
    else
    {
        std::array<std::uint16_t, 4> pixel{};
        if (!texture->readback(
                *mImpl->device, 0, 0, region, pixel.data(),
                pixel.size() * sizeof(pixel[0]), error))
        {
            return false;
        }
        for (std::size_t channel = 0; channel < pixel.size(); ++channel)
        {
            decoded[channel] = floatFromHalf(pixel[channel]);
        }
    }
    std::copy(decoded, decoded + 4, color);
    return true;
}

bool ALMetalWorldGBufferRenderer::updateScreenSpaceReflectionHistory(
    const ALMetalRenderTarget& source,
    std::unique_ptr<ALMetalRenderTarget>& history,
    std::string* error)
{
    if (!mImpl || !mImpl->device)
    {
        setError(error, "Metal world SSR-history renderer is unavailable");
        return false;
    }

    const ALMetalRenderTargetDescriptor& input = source.descriptor();
    const bool hdr = input.color_attachment_count == 1 &&
        input.colors[0].format == ALMetalPixelFormat::rgba16_float;
    const bool color_format = hdr ||
        (input.color_attachment_count == 1 &&
         input.colors[0].format == ALMetalPixelFormat::rgba8_unorm);
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        !color_format ||
        input.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8 ||
        !input.depth_stencil_shader_read ||
        !sampleableTexture(source.colorTexture(0)) ||
        !sampleableTexture(source.depthStencilTexture()))
    {
        setError(error,
                 "Metal world SSR-history source is incompatible");
        return false;
    }

    ALMetalRenderTargetDescriptor descriptor;
    descriptor.width = input.width;
    descriptor.height = input.height;
    descriptor.color_attachment_count = 2;
    descriptor.colors[0].format = hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    descriptor.colors[0].label =
        "Alchemy native SSR scene-color history";
    descriptor.colors[1].format = ALMetalPixelFormat::r32_float;
    descriptor.colors[1].label =
        "Alchemy native SSR scene-depth history";
    descriptor.label = "Alchemy native SSR scene-history target";

    const auto matches = [&descriptor](
        const std::unique_ptr<ALMetalRenderTarget>& target)
    {
        if (!target)
        {
            return false;
        }
        const ALMetalRenderTargetDescriptor& current = target->descriptor();
        return current.width == descriptor.width &&
               current.height == descriptor.height &&
               current.sample_count == 1 &&
               current.color_attachment_count == 2 &&
               current.colors[0].format == descriptor.colors[0].format &&
               current.colors[1].format == descriptor.colors[1].format &&
               target->colorTexture(0) && target->colorTexture(1);
    };

    std::unique_ptr<ALMetalRenderTarget> replacement;
    ALMetalRenderTarget* destination = history.get();
    if (!matches(history))
    {
        replacement = ALMetalRenderTarget::create(
            *mImpl->device, descriptor, error);
        destination = replacement.get();
    }
    if (!destination)
    {
        return false;
    }

    ALMetalRenderPassConfig pass_config;
    auto pass = destination->makeRenderPass(pass_config, error);
    auto command = pass ? mImpl->device->makeCommandBuffer(
        "Alchemy native SSR scene-history publication", error) : nullptr;
    auto encoder = command && pass ? ALMetalRenderEncoder::begin(
        *command, *pass,
        "Alchemy native SSR scene-history encoder", error) : nullptr;
    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(input.width),
        static_cast<double>(input.height), 0.0, 1.0};
    const ALMetalScissorRect scissor{0, 0, input.width, input.height};
    const bool encoded = encoder &&
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(
            *mImpl->ssr_history_pipelines[hdr ? 1 : 0], error) &&
        encoder->setFragmentTexture(0, source.colorTexture(0), error) &&
        encoder->setFragmentTexture(
            1, source.depthStencilTexture(), error) &&
        encoder->draw(
            ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    if (!encoded || !encoder->end(error) || !command->commit(error))
    {
        return false;
    }
    if (replacement)
    {
        history = std::move(replacement);
    }
    return true;
}

bool ALMetalWorldGBufferRenderer::updateHeroProbeFace(
    const ALMetalRenderTarget& source,
    std::uint32_t face,
    std::uint32_t max_probe_lod,
    std::unique_ptr<ALMetalTexture>& source_cube,
    std::uint32_t& valid_faces,
    std::string* error)
{
    if (!mImpl || !mImpl->device)
    {
        setError(error, "Metal hero-probe face publisher is unavailable");
        return false;
    }

    const ALMetalRenderTargetDescriptor& input = source.descriptor();
    const bool color_format = input.color_attachment_count == 1 &&
        (input.colors[0].format == ALMetalPixelFormat::rgba16_float ||
         input.colors[0].format == ALMetalPixelFormat::rgba8_unorm);
    const std::uint32_t resolution = input.width;
    if (!color_format || !source.colorTexture(0) || resolution < 2 ||
        input.height != resolution || input.sample_count != 1 || face >= 6 ||
        (resolution & (resolution - 1)) != 0)
    {
        setError(error, "Metal hero-probe face source is incompatible");
        return false;
    }

    std::uint32_t mip_count = 0;
    for (std::uint32_t dimension = resolution; dimension > 1;
         dimension >>= 1)
    {
        ++mip_count;
    }
    if (mip_count == 0 || max_probe_lod + 1 != mip_count)
    {
        setError(error, "Metal hero-probe mip range is incompatible");
        return false;
    }

    const ALMetalTextureDescriptor* retained = source_cube
        ? &source_cube->descriptor() : nullptr;
    const bool recreate = !retained ||
        retained->type != ALMetalTextureType::texture_cube ||
        retained->format != input.colors[0].format ||
        retained->width != resolution || retained->height != resolution ||
        retained->mipmap_level_count != mip_count;
    std::unique_ptr<ALMetalTexture> replacement;
    ALMetalTexture* destination = source_cube.get();
    if (recreate)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.type = ALMetalTextureType::texture_cube;
        descriptor.format = input.colors[0].format;
        descriptor.width = resolution;
        descriptor.height = resolution;
        descriptor.mipmap_level_count = mip_count;
        descriptor.storage = ALMetalStorageMode::private_gpu;
        descriptor.usage = ALMetalTextureUsage::shader_read;
        descriptor.label = "Alchemy native hero-probe source cube";
        replacement = ALMetalTexture::create(
            *mImpl->device, descriptor, error);
        destination = replacement.get();
    }
    if (!destination)
    {
        return false;
    }

    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native hero-probe face publication", error);
    ALMetalTextureCopy copy;
    copy.destination_slice = face;
    if (!command || !destination->copyFrom(
            *command, *source.colorTexture(0), copy, error) ||
        !command->commit(error))
    {
        return false;
    }

    if (replacement)
    {
        source_cube = std::move(replacement);
        valid_faces = 0;
    }
    valid_faces |= 1u << face;
    return true;
}

bool ALMetalWorldGBufferRenderer::updateReflectionProbeFace(
    const ALMetalRenderTarget& source,
    std::uint32_t face,
    std::uint32_t probe_resolution,
    std::uint32_t max_probe_lod,
    std::unique_ptr<ALMetalTexture>& source_cube,
    std::uint32_t& valid_faces,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !mImpl->probe_gaussian_pipeline ||
        !mImpl->probe_downsample_pipeline)
    {
        setError(error, "Metal reflection-probe face publisher is unavailable");
        return false;
    }

    const ALMetalRenderTargetDescriptor& input = source.descriptor();
    const ALMetalTexture* input_texture = source.colorTexture(0);
    const bool color_format = input.color_attachment_count == 1 &&
        (input.colors[0].format == ALMetalPixelFormat::rgba16_float ||
         input.colors[0].format == ALMetalPixelFormat::rgba8_unorm);
    const std::uint32_t supersample = probe_resolution == 0
        ? 0 : input.width / probe_resolution;
    if (!color_format || !input_texture || probe_resolution < 2 ||
        input.width != input.height || input.sample_count != 1 ||
        face >= 6 || input.width % probe_resolution != 0 ||
        (supersample != 1 && supersample != 2 && supersample != 4) ||
        (probe_resolution & (probe_resolution - 1)) != 0 ||
        !alMetalHasUsage(input_texture->descriptor().usage,
                         ALMetalTextureUsage::shader_read))
    {
        setError(error, "Metal reflection-probe face source is incompatible");
        return false;
    }

    std::uint32_t mip_count = 0;
    for (std::uint32_t dimension = probe_resolution; dimension > 1;
         dimension >>= 1)
    {
        ++mip_count;
    }
    if (mip_count == 0 || max_probe_lod + 1 != mip_count)
    {
        setError(error, "Metal reflection-probe mip range is incompatible");
        return false;
    }

    const auto ensure_scratch = [this, &input, error](
        std::unique_ptr<ALMetalTexture>& texture,
        std::uint32_t width, std::uint32_t height,
        const char* label)
    {
        const ALMetalTextureDescriptor* current = texture
            ? &texture->descriptor() : nullptr;
        if (current && current->type == ALMetalTextureType::texture_2d &&
            current->format == input.colors[0].format &&
            current->width == width && current->height == height &&
            current->mipmap_level_count == 1 &&
            alMetalHasUsage(current->usage,
                            ALMetalTextureUsage::shader_read) &&
            alMetalHasUsage(current->usage,
                            ALMetalTextureUsage::shader_write))
        {
            return true;
        }
        ALMetalTextureDescriptor descriptor;
        descriptor.format = input.colors[0].format;
        descriptor.width = width;
        descriptor.height = height;
        descriptor.storage = ALMetalStorageMode::private_gpu;
        descriptor.usage = ALMetalTextureUsage::shader_read |
                           ALMetalTextureUsage::shader_write;
        descriptor.label = label;
        texture = ALMetalTexture::create(
            *mImpl->device, descriptor, error);
        return texture != nullptr;
    };

    const ALMetalTexture* prepared = input_texture;
    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native reflection-probe face publication", error);
    if (!command)
    {
        return false;
    }
    if (supersample > 1)
    {
        if (!ensure_scratch(
                mImpl->probe_gaussian_horizontal,
                input.width, input.height,
                "Alchemy reflection-probe horizontal Gaussian") ||
            !ensure_scratch(
                mImpl->probe_gaussian_vertical,
                input.width, input.height,
                "Alchemy reflection-probe vertical Gaussian") ||
            !ensure_scratch(
                mImpl->probe_downsample,
                probe_resolution, probe_resolution,
                "Alchemy reflection-probe downsample"))
        {
            return false;
        }

        struct alignas(16) GaussianConstants
        {
            float direction[2]{};
            float padding[2]{};
        };
        GaussianConstants horizontal;
        horizontal.direction[0] =
            1.f / static_cast<float>(probe_resolution * 2u);
        auto horizontal_encoder = ALMetalComputeEncoder::begin(
            *command, "Alchemy reflection-probe horizontal Gaussian",
            error);
        if (!horizontal_encoder ||
            !horizontal_encoder->setPipeline(
                *mImpl->probe_gaussian_pipeline, error) ||
            !horizontal_encoder->setTexture(0, input_texture, error) ||
            !horizontal_encoder->setTexture(
                1, mImpl->probe_gaussian_horizontal.get(), error) ||
            !horizontal_encoder->setBytes(
                0, &horizontal, sizeof(horizontal), error) ||
            !horizontal_encoder->dispatch(
                input.width, input.height, 1, 8, 8, 1, error) ||
            !horizontal_encoder->end(error))
        {
            return false;
        }

        GaussianConstants vertical;
        vertical.direction[1] =
            1.f / static_cast<float>(probe_resolution * 2u);
        auto vertical_encoder = ALMetalComputeEncoder::begin(
            *command, "Alchemy reflection-probe vertical Gaussian",
            error);
        if (!vertical_encoder ||
            !vertical_encoder->setPipeline(
                *mImpl->probe_gaussian_pipeline, error) ||
            !vertical_encoder->setTexture(
                0, mImpl->probe_gaussian_horizontal.get(), error) ||
            !vertical_encoder->setTexture(
                1, mImpl->probe_gaussian_vertical.get(), error) ||
            !vertical_encoder->setBytes(
                0, &vertical, sizeof(vertical), error) ||
            !vertical_encoder->dispatch(
                input.width, input.height, 1, 8, 8, 1, error) ||
            !vertical_encoder->end(error))
        {
            return false;
        }

        auto downsample_encoder = ALMetalComputeEncoder::begin(
            *command, "Alchemy reflection-probe downsample", error);
        if (!downsample_encoder ||
            !downsample_encoder->setPipeline(
                *mImpl->probe_downsample_pipeline, error) ||
            !downsample_encoder->setTexture(
                0, mImpl->probe_gaussian_vertical.get(), error) ||
            !downsample_encoder->setTexture(
                1, mImpl->probe_downsample.get(), error) ||
            !downsample_encoder->dispatch(
                probe_resolution, probe_resolution, 1,
                8, 8, 1, error) ||
            !downsample_encoder->end(error))
        {
            return false;
        }
        prepared = mImpl->probe_downsample.get();
    }

    const ALMetalTextureDescriptor* retained = source_cube
        ? &source_cube->descriptor() : nullptr;
    const bool recreate = !retained ||
        retained->type != ALMetalTextureType::texture_cube ||
        retained->format != input.colors[0].format ||
        retained->width != probe_resolution ||
        retained->height != probe_resolution ||
        retained->mipmap_level_count != mip_count;
    std::unique_ptr<ALMetalTexture> replacement;
    ALMetalTexture* destination = source_cube.get();
    if (recreate)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.type = ALMetalTextureType::texture_cube;
        descriptor.format = input.colors[0].format;
        descriptor.width = probe_resolution;
        descriptor.height = probe_resolution;
        descriptor.mipmap_level_count = mip_count;
        descriptor.storage = ALMetalStorageMode::private_gpu;
        descriptor.usage = ALMetalTextureUsage::shader_read;
        descriptor.label = "Alchemy native reflection-probe source cube";
        replacement = ALMetalTexture::create(
            *mImpl->device, descriptor, error);
        destination = replacement.get();
    }
    ALMetalTextureCopy copy;
    copy.destination_slice = face;
    if (!destination ||
        !destination->copyFrom(*command, *prepared, copy, error) ||
        !command->commit(error))
    {
        return false;
    }
    if (replacement)
    {
        source_cube = std::move(replacement);
        valid_faces = 0;
    }
    valid_faces |= 1u << face;
    return true;
}

bool ALMetalWorldGBufferRenderer::filterHeroProbeRadiance(
    ALMetalTexture& source_cube,
    std::uint32_t valid_faces,
    std::uint32_t max_probe_lod,
    float probe_strength,
    std::unique_ptr<ALMetalTexture>& radiance_cube,
    std::string* error)
{
    const ALMetalTextureDescriptor& source = source_cube.descriptor();
    const bool hdr = source.format == ALMetalPixelFormat::rgba16_float;
    const std::uint32_t radiance_mips =
        (max_probe_lod + 3u) / 4u + 1u;
    return filterProbeRadiance(
        source_cube, valid_faces, max_probe_lod,
        radiance_mips, 4u,
        hdr ? ALMetalPixelFormat::rg11b10_float
            : ALMetalPixelFormat::rgba8_unorm,
        true,
        probe_strength,
        radiance_cube, error);
}

bool ALMetalWorldGBufferRenderer::filterReflectionProbeRadiance(
    ALMetalTexture& source_cube,
    std::uint32_t valid_faces,
    std::uint32_t max_probe_lod,
    std::unique_ptr<ALMetalTexture>& radiance_cube,
    std::string* error)
{
    return filterProbeRadiance(
        source_cube, valid_faces, max_probe_lod,
        max_probe_lod + 1u, 32u,
        source_cube.descriptor().format,
        false,
        1.f,
        radiance_cube, error);
}

bool ALMetalWorldGBufferRenderer::filterProbeRadiance(
    ALMetalTexture& source_cube,
    std::uint32_t valid_faces,
    std::uint32_t max_probe_lod,
    std::uint32_t output_mip_count,
    std::uint32_t sample_scale,
    ALMetalPixelFormat output_format,
    bool hero_probe,
    float probe_strength,
    std::unique_ptr<ALMetalTexture>& radiance_cube,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !mImpl->hero_probe_filter_pipeline)
    {
        setError(error, hero_probe
            ? "Metal hero-probe radiance filter is unavailable"
            : "Metal reflection-probe radiance filter is unavailable");
        return false;
    }
    const ALMetalTextureDescriptor& source = source_cube.descriptor();
    const bool hdr = source.format == ALMetalPixelFormat::rgba16_float;
    const bool color_format = hdr ||
        source.format == ALMetalPixelFormat::rgba8_unorm;
    if (valid_faces != 0x3fu ||
        source.type != ALMetalTextureType::texture_cube ||
        !color_format || source.width < 2 || source.height != source.width ||
        source.mipmap_level_count != max_probe_lod + 1 ||
        !alMetalHasUsage(source.usage, ALMetalTextureUsage::shader_read) ||
        output_mip_count == 0 ||
        output_mip_count > source.mipmap_level_count ||
        sample_scale == 0 ||
        (output_format != ALMetalPixelFormat::rgba16_float &&
         output_format != ALMetalPixelFormat::rgba8_unorm &&
         output_format != ALMetalPixelFormat::rg11b10_float) ||
        !std::isfinite(probe_strength) || probe_strength < 0.f)
    {
        setError(error, hero_probe
            ? "Metal hero-probe radiance source is incomplete"
            : "Metal reflection-probe radiance source is incomplete");
        return false;
    }

    const ALMetalTextureDescriptor* retained = radiance_cube
        ? &radiance_cube->descriptor() : nullptr;
    const bool recreate = !retained ||
        retained->type != ALMetalTextureType::texture_cube ||
        retained->format != output_format ||
        retained->width != source.width ||
        retained->height != source.height ||
        retained->mipmap_level_count != output_mip_count;
    std::unique_ptr<ALMetalTexture> replacement;
    ALMetalTexture* destination = radiance_cube.get();
    if (recreate)
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.type = ALMetalTextureType::texture_cube;
        descriptor.format = output_format;
        descriptor.width = source.width;
        descriptor.height = source.height;
        descriptor.mipmap_level_count = output_mip_count;
        descriptor.storage = ALMetalStorageMode::private_gpu;
        descriptor.usage = ALMetalTextureUsage::shader_read |
                           ALMetalTextureUsage::shader_write;
        descriptor.label = hero_probe
            ? "Alchemy native hero-probe radiance cube"
            : "Alchemy native reflection-probe radiance cube";
        replacement = ALMetalTexture::create(
            *mImpl->device, descriptor, error);
        destination = replacement.get();
    }
    if (!destination)
    {
        return false;
    }

    struct alignas(16) HeroFilterConstants
    {
        std::uint32_t mip_level = 0;
        std::uint32_t max_probe_lod = 0;
        std::uint32_t resolution = 0;
        std::uint32_t sample_scale = 0;
        float probe_strength = 1.f;
        float padding[3]{};
    };
    static_assert(sizeof(HeroFilterConstants) == 32);

    auto command = mImpl->device->makeCommandBuffer(
        hero_probe
            ? "Alchemy native hero-probe radiance publication"
            : "Alchemy native reflection-probe radiance publication",
        error);
    if (!command || !source_cube.generateMipmaps(*command, error))
    {
        return false;
    }
    for (std::uint32_t mip = 0; mip < output_mip_count; ++mip)
    {
        HeroFilterConstants constants;
        constants.mip_level = mip;
        constants.max_probe_lod = max_probe_lod;
        constants.resolution = std::max(source.width >> mip, 1u);
        constants.sample_scale = sample_scale;
        constants.probe_strength = probe_strength;
        auto encoder = ALMetalComputeEncoder::begin(
            *command,
            hero_probe
                ? "Alchemy native hero-probe radiance encoder"
                : "Alchemy native reflection-probe radiance encoder",
            error);
        if (!encoder ||
            !encoder->setPipeline(
                *mImpl->hero_probe_filter_pipeline, error) ||
            !encoder->setTexture(0, &source_cube, error) ||
            !encoder->setTexture(1, destination, error) ||
            !encoder->setBytes(
                0, &constants, sizeof(constants), error) ||
            !encoder->dispatch(
                constants.resolution, constants.resolution, 6,
                8, 8, 1, error) ||
            !encoder->end(error))
        {
            return false;
        }
    }
    if (!command->commit(error))
    {
        return false;
    }
    if (replacement)
    {
        radiance_cube = std::move(replacement);
    }
    return true;
}

bool ALMetalWorldGBufferRenderer::projectReflectionProbeSH(
    ALMetalTexture& source_cube,
    std::uint32_t valid_faces,
    std::uint32_t source_mip,
    std::uint32_t source_resolution,
    ALMetalTexture& destination,
    std::uint32_t destination_row,
    std::unique_ptr<ALMetalBuffer>& packed_words,
    std::unique_ptr<ALMetalCommandBuffer>& pending_command,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !mImpl->probe_sh_rows_pipeline ||
        !mImpl->probe_sh_reduce_pipeline)
    {
        setError(error, "Metal reflection-probe SH projector is unavailable");
        return false;
    }
    const ALMetalTextureDescriptor& source = source_cube.descriptor();
    const ALMetalTextureDescriptor& output = destination.descriptor();
    const bool color_format =
        source.format == ALMetalPixelFormat::rgba16_float ||
        source.format == ALMetalPixelFormat::rgba8_unorm;
    if (valid_faces != 0x3fu ||
        source.type != ALMetalTextureType::texture_cube || !color_format ||
        source_mip >= source.mipmap_level_count ||
        source_cube.width(source_mip) != source_resolution ||
        source_cube.height(source_mip) != source_resolution ||
        source_resolution < 4 ||
        output.type != ALMetalTextureType::texture_2d ||
        output.format != ALMetalPixelFormat::rgba32_uint ||
        output.width < 4 || destination_row >= output.height ||
        !alMetalHasUsage(source.usage, ALMetalTextureUsage::shader_read) ||
        !alMetalHasUsage(output.usage, ALMetalTextureUsage::shader_write))
    {
        setError(error, "Metal reflection-probe SH inputs are incompatible");
        return false;
    }
    if (pending_command &&
        pending_command->status() != ALMetalCommandStatus::completed &&
        pending_command->status() != ALMetalCommandStatus::error)
    {
        setError(error, "Metal reflection-probe SH publication is still pending");
        return false;
    }
    pending_command.reset();

    const std::uint32_t partial_height = 6u * source_resolution;
    const ALMetalTextureDescriptor* partial = mImpl->probe_sh_partial
        ? &mImpl->probe_sh_partial->descriptor() : nullptr;
    if (!partial || partial->type != ALMetalTextureType::texture_2d ||
        partial->format != ALMetalPixelFormat::rgba16_float ||
        partial->width != 9 || partial->height != partial_height ||
        !alMetalHasUsage(partial->usage,
                         ALMetalTextureUsage::shader_read) ||
        !alMetalHasUsage(partial->usage,
                         ALMetalTextureUsage::shader_write))
    {
        ALMetalTextureDescriptor descriptor;
        descriptor.format = ALMetalPixelFormat::rgba16_float;
        descriptor.width = 9;
        descriptor.height = partial_height;
        descriptor.storage = ALMetalStorageMode::private_gpu;
        descriptor.usage = ALMetalTextureUsage::shader_read |
                           ALMetalTextureUsage::shader_write;
        descriptor.label = "Alchemy reflection-probe SH partial rows";
        mImpl->probe_sh_partial = ALMetalTexture::create(
            *mImpl->device, descriptor, error);
        if (!mImpl->probe_sh_partial)
        {
            return false;
        }
    }

    constexpr std::size_t packed_bytes = 4 * 4 * sizeof(std::uint32_t);
    if (!packed_words || packed_words->length() != packed_bytes ||
        packed_words->storageMode() != ALMetalStorageMode::shared)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length = packed_bytes;
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.label = "Alchemy reflection-probe packed SH words";
        packed_words = ALMetalBuffer::create(
            *mImpl->device, descriptor, error);
        if (!packed_words)
        {
            return false;
        }
    }

    struct alignas(16) SHConstants
    {
        std::uint32_t source_mip = 0;
        std::uint32_t source_resolution = 0;
        std::uint32_t destination_row = 0;
        std::uint32_t padding = 0;
    } constants;
    constants.source_mip = source_mip;
    constants.source_resolution = source_resolution;
    constants.destination_row = destination_row;

    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native reflection-probe SH publication", error);
    if (!command || !source_cube.generateMipmaps(*command, error))
    {
        return false;
    }
    auto rows = ALMetalComputeEncoder::begin(
        *command, "Alchemy reflection-probe SH rows", error);
    if (!rows ||
        !rows->setPipeline(*mImpl->probe_sh_rows_pipeline, error) ||
        !rows->setTexture(0, &source_cube, error) ||
        !rows->setTexture(1, mImpl->probe_sh_partial.get(), error) ||
        !rows->setBytes(0, &constants, sizeof(constants), error) ||
        !rows->dispatch(9, partial_height, 1, 9, 1, 1, error) ||
        !rows->end(error))
    {
        return false;
    }
    auto reduce = ALMetalComputeEncoder::begin(
        *command, "Alchemy reflection-probe SH reduce", error);
    if (!reduce ||
        !reduce->setPipeline(*mImpl->probe_sh_reduce_pipeline, error) ||
        !reduce->setTexture(0, mImpl->probe_sh_partial.get(), error) ||
        !reduce->setTexture(1, &destination, error) ||
        !reduce->setBytes(0, &constants, sizeof(constants), error) ||
        !reduce->setBuffer(1, *packed_words, 0, error) ||
        !reduce->dispatch(4, 1, 1, 4, 1, 1, error) ||
        !reduce->end(error) || !command->commit(error))
    {
        return false;
    }
    pending_command = std::move(command);
    return true;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderOpaquePostDeferred(
    const ALMetalWorldOpaquePostDeferredScene& scene,
    std::string* error)
{
    return renderOpaquePostDeferred(scene, nullptr, error);
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderOpaquePostDeferred(
    const ALMetalWorldOpaquePostDeferredScene& scene,
    std::unique_ptr<ALMetalRenderTarget> composed_lighting_owner,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.gbuffer ||
        !scene.composed_lighting ||
        (scene.batch_count != 0 && !scene.batches) ||
        scene.width == 0 || scene.height == 0)
    {
        setError(error, "Metal world opaque post-deferred scene is incomplete");
        return nullptr;
    }
    if (scene.highlight_batch_count != 0 && !scene.highlight_batches)
    {
        setError(error,
                 "Metal world highlight batch storage is missing");
        return nullptr;
    }
    if (!finiteValues(scene.mirror_clip_plane, 4))
    {
        setError(error,
                 "Metal world opaque post-deferred mirror clip is non-finite");
        return nullptr;
    }

    const ALMetalPixelFormat color_format = scene.hdr
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rgba8_unorm;
    const ALMetalRenderTargetDescriptor& gbuffer =
        scene.gbuffer->descriptor();
    const ALMetalRenderTargetDescriptor& lighting =
        scene.composed_lighting->descriptor();
    const bool continue_composed_target =
        composed_lighting_owner != nullptr;
    if (gbuffer.width != scene.width || gbuffer.height != scene.height ||
        gbuffer.sample_count != 1 || gbuffer.color_attachment_count != 4 ||
        gbuffer.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8 ||
        !scene.gbuffer->depthStencilTexture() ||
        lighting.width != scene.width || lighting.height != scene.height ||
        lighting.sample_count != 1 || lighting.color_attachment_count != 1 ||
        lighting.colors[0].format != color_format ||
        !sampleableTexture(scene.composed_lighting->colorTexture(0)))
    {
        setError(error, "Metal world opaque post-deferred inputs are incompatible");
        return nullptr;
    }
    if (continue_composed_target &&
        (composed_lighting_owner.get() != scene.composed_lighting ||
         lighting.depth_stencil_format !=
             ALMetalPixelFormat::depth32_float_stencil8 ||
         !scene.composed_lighting->depthStencilTexture()))
    {
        setError(
            error,
            "Metal world opaque post-deferred continuation target is incompatible");
        return nullptr;
    }

    constexpr std::size_t float4_stride = sizeof(float) * 4;
    constexpr std::size_t float2_stride = sizeof(float) * 2;
    constexpr std::size_t color_stride = sizeof(std::uint32_t);
    constexpr std::uint32_t max_skin_joints = 110;
    for (std::size_t index = 0;
         index < scene.highlight_batch_count; ++index)
    {
        const ALMetalWorldHighlightBatch& batch =
            scene.highlight_batches[index];
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices || !batch.texture ||
            batch.vertex_count == 0 || batch.index_count == 0 ||
            batch.index_count % 3 != 0 ||
            batch.position_offset % float4_stride != 0 ||
            batch.texcoord_offset % float2_stride != 0 ||
            batch.index_offset % index_stride != 0 ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       float2_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()) ||
            !sampleableTexture(batch.texture) ||
            !finiteValues(batch.modelview_projection, 16) ||
            !finiteValues(batch.texture_matrix, 16) ||
            !finiteValues(batch.color, 4))
        {
            setError(error, "Metal world highlight batch is invalid");
            return nullptr;
        }
    }
    bool has_shiny = false;
    bool has_water_exclusion = false;
    bool has_water_haze = false;
    bool has_water_surface = false;
    bool has_above_water_surface = false;
    bool has_atmospheric_haze = false;
    bool water_uses_shadows = false;
    bool has_pre_water_alpha = false;
    bool has_avatar_alpha = false;
    bool has_pre_water_simple = false;
    bool has_pre_water_material = false;
    bool has_pre_water_pbr = false;
    bool has_pre_water_shadows = false;
    bool has_post_water_alpha = false;
    bool saw_pre_water_alpha = false;
    bool saw_post_water = false;
    bool saw_post_water_depth = false;
    bool saw_post_exclusion = false;
    const bool water_surface_requires_haze = scene.water_surface &&
        (scene.water_surface->underwater || scene.water_surface->refracts);
    const auto valid_skinning = [=](
        const ALMetalWorldSimpleBatch& batch, bool rigged)
    {
        const bool avatar = batch.avatar_rigid || batch.avatar_skinning;
        if (avatar)
        {
            if (batch.avatar_rigid == batch.avatar_skinning ||
                (batch.avatar_cloth && !batch.avatar_skinning))
            {
                return false;
            }
            if (batch.avatar_rigid)
            {
                return !rigged && batch.skin_palette_float_count == 0 &&
                    batch.skin_joint_count == 0 &&
                    batch.weight_offset == 0 && !batch.avatar_cloth;
            }
            return rigged && batch.skin_joint_count == 15 &&
                batch.skin_palette_float_count == 45 * 4 &&
                batch.weight_offset % sizeof(float) == 0 &&
                rangeFits(batch.weight_offset, batch.vertex_count,
                          sizeof(float), batch.vertices->length()) &&
                finiteValues(batch.skin_palette,
                             batch.skin_palette_float_count) &&
                (!batch.avatar_cloth ||
                 (batch.cloth_offset % float4_stride == 0 &&
                  rangeFits(batch.cloth_offset, batch.vertex_count,
                            float4_stride, batch.vertices->length()) &&
                  finiteValues(batch.avatar_wind, 4) &&
                  finiteValues(batch.avatar_sin_wave, 4) &&
                  finiteValues(batch.avatar_gravity, 4) &&
                  finiteValues(batch.avatar_cloth_depth, 4)));
        }
        return rigged
            ? batch.skin_joint_count != 0 &&
                  batch.skin_joint_count <= max_skin_joints &&
                  batch.skin_palette_float_count ==
                      4 + static_cast<std::size_t>(
                              batch.skin_joint_count) * 12 &&
                  batch.weight_offset % float4_stride == 0 &&
                  rangeFits(batch.weight_offset, batch.vertex_count,
                            float4_stride, batch.vertices->length()) &&
                  finiteValues(batch.skin_palette,
                               batch.skin_palette_float_count)
            : batch.skin_palette_float_count == 0 &&
                  batch.skin_joint_count == 0;
    };
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        const bool alpha = batch.pre_water_alpha ||
            batch.post_water_alpha;
        const bool avatar_alpha = batch.avatar_alpha;
        const bool alpha_lit = alpha || avatar_alpha;
        const bool glow = batch.legacy_glow || batch.pbr_glow;
        const std::uint32_t family_count =
            (batch.impostor_fullbright ? 1u : 0u) +
            (batch.impostor_shiny ? 1u : 0u) +
            (batch.impostor_emboss ? 1u : 0u) +
            (batch.water_exclusion ? 1u : 0u) +
            (batch.water_haze ? 1u : 0u) +
            (batch.water_surface ? 1u : 0u) +
            (batch.atmospheric_haze ? 1u : 0u) +
            (alpha ? 1u : 0u) +
            (avatar_alpha ? 1u : 0u) +
            (batch.legacy_glow ? 1u : 0u) +
            (batch.pbr_glow ? 1u : 0u);
        const bool rigged = batch.skin_palette != nullptr;
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (family_count != 1 ||
            ((batch.pre_water_fullbright || batch.pre_water_simple ||
              batch.pre_water_material || batch.pre_water_pbr ||
              batch.pre_water_shadows) &&
             !alpha_lit) ||
            (batch.pre_water_alpha && batch.post_water_alpha) ||
            (avatar_alpha &&
             ((!batch.avatar_rigid && !batch.avatar_skinning) ||
              batch.pre_water_fullbright ||
              !batch.pre_water_simple || batch.pre_water_material ||
              batch.pre_water_pbr || batch.pre_water_particle ||
              (batch.avatar_cull_mode != ALMetalCullMode::none &&
               batch.avatar_cull_mode != ALMetalCullMode::front &&
               batch.avatar_cull_mode != ALMetalCullMode::back))) ||
            (batch.post_water_depth_only &&
             (!batch.post_water_alpha || rigged)) ||
            (batch.post_water_glow && (!glow || alpha)))
        {
            setError(
                error,
                "Metal world opaque post-deferred batch is invalid at index " +
                    std::to_string(index) +
                    " (family contract: families=" +
                    std::to_string(family_count) +
                    ", alpha=" + std::to_string(alpha) +
                    ", avatar-alpha=" +
                    std::to_string(avatar_alpha) +
                    ", glow=" + std::to_string(glow) +
                    ", pre=" +
                    std::to_string(batch.pre_water_alpha) +
                    ", post=" +
                    std::to_string(batch.post_water_alpha) +
                    ", depth-only=" +
                    std::to_string(batch.post_water_depth_only) +
                    ", post-glow=" +
                    std::to_string(batch.post_water_glow) + ")");
            return nullptr;
        }
        const bool post_family = batch.post_water_alpha ||
            batch.post_water_glow;
        const bool post_boundary_ready =
            (!scene.water_surface || has_water_surface) &&
            (!scene.atmospheric_haze || has_atmospheric_haze);
        if ((has_water_surface && !batch.atmospheric_haze &&
             !post_family) ||
            (has_atmospheric_haze && !batch.water_surface &&
             !post_family) ||
            (has_water_haze && !batch.pre_water_alpha &&
             !batch.water_surface && !batch.atmospheric_haze &&
             !post_family) ||
            (saw_pre_water_alpha && !batch.pre_water_alpha &&
             !batch.water_surface && !batch.atmospheric_haze &&
             !post_family) ||
            (post_family && !post_boundary_ready) ||
            (batch.post_water_glow && !has_post_water_alpha) ||
            (batch.post_water_depth_only && !has_post_water_alpha) ||
            (saw_post_water_depth && !batch.post_water_depth_only) ||
            (saw_post_water && !post_family))
        {
            setError(error,
                     "Metal world water-haze ordering is invalid");
            return nullptr;
        }
        if (batch.water_haze)
        {
            if (has_water_haze || saw_pre_water_alpha ||
                batch.vertices || batch.indices ||
                batch.vertex_count != 0 || batch.index_count != 0 ||
                batch.texture_count != 0 || batch.normal_texture ||
                batch.material_slot_count != 0 ||
                batch.pbr_slot_count != 0 || rigged ||
                batch.skin_palette_float_count != 0 ||
                batch.skin_joint_count != 0)
            {
                setError(error, "Metal world water-haze marker is invalid");
                return nullptr;
            }
            has_water_haze = true;
            saw_post_exclusion = true;
            continue;
        }
        if (batch.atmospheric_haze)
        {
            const ALMetalWorldAtmosphericHaze* haze =
                scene.atmospheric_haze;
            if (!haze || has_atmospheric_haze ||
                (haze->above_water && scene.water_surface &&
                 !has_water_surface) ||
                (!haze->above_water && has_water_surface) ||
                batch.vertices || batch.indices ||
                batch.vertex_count != 0 || batch.index_count != 0 ||
                batch.texture_count != 0 || batch.normal_texture ||
                batch.material_slot_count != 0 ||
                batch.pbr_slot_count != 0 || rigged ||
                batch.skin_palette_float_count != 0 ||
                batch.skin_joint_count != 0)
            {
                setError(error,
                         "Metal world atmospheric-haze marker is invalid");
                return nullptr;
            }
            has_atmospheric_haze = true;
            saw_post_exclusion = true;
            continue;
        }
        if (batch.water_surface)
        {
            if ((water_surface_requires_haze && !has_water_haze) ||
                has_water_surface ||
                (has_atmospheric_haze &&
                 scene.atmospheric_haze->above_water) ||
                batch.vertices || batch.indices ||
                batch.vertex_count != 0 || batch.index_count != 0 ||
                batch.texture_count != 0 || batch.normal_texture ||
                batch.material_slot_count != 0 ||
                batch.pbr_slot_count != 0 || rigged ||
                batch.skin_palette_float_count != 0 ||
                batch.skin_joint_count != 0)
            {
                setError(error,
                         "Metal world water-surface marker is invalid");
                return nullptr;
            }
            has_water_surface = true;
            continue;
        }
        if (!batch.vertices || !batch.indices ||
            batch.vertex_count == 0 || batch.index_count == 0 ||
            batch.normal_texture || batch.impostor || batch.terrain ||
            ((batch.avatar_rigid || batch.avatar_skinning ||
              batch.avatar_cloth) && !avatar_alpha) ||
            batch.index_offset % index_stride != 0 ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()) ||
            batch.position_offset % float4_stride != 0 ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !finiteValues(batch.modelview_projection, 16))
        {
            setError(
                error,
                "Metal world opaque post-deferred batch is invalid at index " +
                    std::to_string(index) +
                    " (geometry contract: vertices=" +
                    std::to_string(batch.vertex_count) +
                    ", indices=" + std::to_string(batch.index_count) +
                    ", position-offset=" +
                    std::to_string(batch.position_offset) +
                    ", index-offset=" +
                    std::to_string(batch.index_offset) + ")");
            return nullptr;
        }
        if (batch.water_exclusion)
        {
            if (saw_post_exclusion || batch.texture_count != 0 || rigged ||
                batch.material_slot_count != 0 ||
                batch.pbr_slot_count != 0 ||
                batch.skin_palette_float_count != 0 ||
                batch.skin_joint_count != 0 ||
                !finiteValues(batch.modelview, 16))
            {
                setError(
                    error,
                    "Metal world water-exclusion batch is invalid");
                return nullptr;
            }
            has_water_exclusion = true;
            continue;
        }
        saw_post_exclusion = true;
        if (alpha_lit)
        {
            has_avatar_alpha = has_avatar_alpha || avatar_alpha;
            has_pre_water_alpha = has_pre_water_alpha ||
                batch.pre_water_alpha;
            has_post_water_alpha = has_post_water_alpha ||
                (batch.post_water_alpha &&
                 !batch.post_water_depth_only);
            has_pre_water_simple =
                has_pre_water_simple || batch.pre_water_simple;
            has_pre_water_material =
                has_pre_water_material || batch.pre_water_material;
            has_pre_water_pbr =
                has_pre_water_pbr || batch.pre_water_pbr;
            has_pre_water_shadows =
                has_pre_water_shadows || batch.pre_water_shadows;
            saw_pre_water_alpha = saw_pre_water_alpha ||
                (!avatar_alpha && batch.pre_water_alpha);
        }
        saw_post_water = saw_post_water || post_family;
        saw_post_water_depth = saw_post_water_depth ||
            batch.post_water_depth_only;
        if (batch.texture_count == 0 ||
            batch.texture_count >
                ALMetalWorldSimpleBatch::max_diffuse_textures ||
            batch.texcoord_offset % float2_stride != 0 ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       float2_stride, batch.vertices->length()) ||
            !finiteValues(batch.texture_matrix, 16) ||
            !valid_skinning(batch, rigged) ||
            (glow &&
             (batch.material_slot_count != 0 || batch.alpha_mask ||
              batch.material_alpha_mode != 0 ||
              batch.emissive_offset % color_stride != 0 ||
              !rangeFits(batch.emissive_offset, batch.vertex_count,
                         color_stride, batch.vertices->length()))) ||
            (batch.legacy_glow &&
             (batch.pbr_slot_count != 0 || batch.double_sided)) ||
            (batch.pbr_glow &&
             (batch.pbr_slot_count == 0 ||
              batch.pbr_slot_count != batch.texture_count ||
              batch.pbr_slot_count >
                  ALMetalWorldSimpleBatch::max_material_slots)) ||
            (alpha_lit &&
             ((batch.pre_water_fullbright ? 1u : 0u) +
                      (batch.pre_water_simple ? 1u : 0u) +
                      (batch.pre_water_material ? 1u : 0u) +
                      (batch.pre_water_pbr ? 1u : 0u) != 1u ||
              (batch.pre_water_shadows &&
               !batch.pre_water_simple &&
               !batch.pre_water_material &&
               !batch.pre_water_pbr) ||
              !std::isfinite(batch.pre_water_minimum_alpha) ||
              (batch.pre_water_minimum_alpha != 0.f &&
               batch.pre_water_minimum_alpha != 0.004f &&
               batch.pre_water_minimum_alpha != 0.33f &&
               (!avatar_alpha ||
                batch.pre_water_minimum_alpha != 0.2f)) ||
              (avatar_alpha &&
               batch.pre_water_minimum_alpha != 0.2f) ||
              (batch.pre_water_minimum_alpha == 0.33f &&
               (!batch.post_water_depth_only ||
                !batch.pre_water_fullbright)) ||
              (batch.post_water_depth_only &&
               batch.pre_water_fullbright &&
               batch.pre_water_minimum_alpha == 0.004f) ||
              (batch.post_water_depth_only &&
               batch.pre_water_alpha_writes_coverage) ||
              !validBlendFactor(batch.pre_water_source_rgb) ||
              !validBlendFactor(batch.pre_water_destination_rgb) ||
              batch.alpha_mask ||
              (batch.double_sided && !batch.pre_water_pbr) ||
              (batch.pre_water_material
                   ? (batch.material_alpha_mode != 1 ||
                      batch.material_slot_count == 0 ||
                      batch.material_slot_count != batch.texture_count ||
                      batch.material_slot_count >
                          ALMetalWorldSimpleBatch::max_material_slots ||
                      batch.pbr_slot_count != 0)
                   : batch.pre_water_pbr
                   ? (batch.material_alpha_mode != 0 ||
                      batch.material_slot_count != 0 ||
                      batch.pbr_slot_count == 0 ||
                      batch.pbr_slot_count != batch.texture_count ||
                      batch.pbr_slot_count >
                          ALMetalWorldSimpleBatch::max_material_slots)
                   : (batch.material_alpha_mode != 0 ||
                      batch.material_slot_count != 0 ||
                      batch.pbr_slot_count != 0)) ||
              !finiteValues(batch.modelview, 16) ||
              !finiteValues(&batch.normal_matrix[0][0], 12) ||
              !finiteValues(&batch.rigged_normal_matrix[0][0], 12))) ||
            (!glow && !alpha &&
             (batch.material_slot_count != 0 ||
              batch.pbr_slot_count != 0)) ||
            (batch.impostor_fullbright &&
             (!std::isfinite(batch.alpha_cutoff) ||
              batch.alpha_cutoff < 0.f || batch.alpha_cutoff > 1.f)) ||
            (!glow && !batch.impostor_emboss && !avatar_alpha &&
             (batch.color_offset % color_stride != 0 ||
              !rangeFits(batch.color_offset, batch.vertex_count,
                         color_stride, batch.vertices->length()))) ||
            (!glow && !batch.impostor_emboss &&
             !batch.impostor_fullbright &&
             (batch.normal_offset % float4_stride != 0 ||
              !rangeFits(batch.normal_offset, batch.vertex_count,
                         float4_stride, batch.vertices->length()))) ||
            (batch.impostor_emboss &&
             (batch.texcoord1_offset % float2_stride != 0 ||
              !rangeFits(batch.texcoord1_offset, batch.vertex_count,
                         float2_stride, batch.vertices->length()))) ||
            (batch.impostor_shiny &&
             (!finiteValues(batch.modelview, 16) ||
              !finiteValues(&batch.normal_matrix[0][0], 12) ||
              !finiteValues(&batch.rigged_normal_matrix[0][0], 12))) ||
            (batch.pre_water_material &&
             ((materialHasNormalMap(batch) &&
               (batch.tangent_offset % float4_stride != 0 ||
                batch.texcoord1_offset % float2_stride != 0 ||
                !rangeFits(batch.tangent_offset, batch.vertex_count,
                           float4_stride, batch.vertices->length()) ||
                !rangeFits(batch.texcoord1_offset, batch.vertex_count,
                           float2_stride, batch.vertices->length()))) ||
              (materialHasSpecularMap(batch) &&
               (batch.texcoord2_offset % float2_stride != 0 ||
                !rangeFits(batch.texcoord2_offset, batch.vertex_count,
                           float2_stride, batch.vertices->length()))))) ||
            (batch.pre_water_pbr &&
             (batch.tangent_offset % float4_stride != 0 ||
              !rangeFits(batch.tangent_offset, batch.vertex_count,
                         float4_stride, batch.vertices->length()))))
        {
            setError(
                error,
                "Metal world opaque post-deferred batch is invalid at index " +
                    std::to_string(index) +
                    " (attribute contract: textures=" +
                    std::to_string(batch.texture_count) +
                    ", material-slots=" +
                    std::to_string(batch.material_slot_count) +
                    ", pbr-slots=" +
                    std::to_string(batch.pbr_slot_count) +
                    ", alpha=" + std::to_string(alpha) +
                    ", glow=" + std::to_string(glow) +
                    ", rigged=" + std::to_string(rigged) +
                    ", minimum-alpha=" +
                    std::to_string(batch.pre_water_minimum_alpha) + ")");
            return nullptr;
        }
        for (std::uint32_t texture = 0;
             texture < batch.texture_count; ++texture)
        {
            if (!sampleableTexture(batch.diffuse_textures[texture]))
            {
                setError(error,
                         "Metal world opaque post-deferred texture is invalid");
                return nullptr;
            }
        }
        if (batch.pre_water_material)
        {
            const bool normal_map = materialHasNormalMap(batch);
            const bool specular_map = materialHasSpecularMap(batch);
            for (std::uint32_t slot = 0;
                 slot < batch.material_slot_count; ++slot)
            {
                const ALMetalWorldMaterialSlot& material =
                    batch.material_slots[slot];
                if (!finiteValues(material.specular_color, 4) ||
                    !std::isfinite(material.env_intensity) ||
                    !std::isfinite(material.fullbright) ||
                    material.env_intensity < 0.f ||
                    material.env_intensity > 1.f ||
                    material.fullbright < 0.f ||
                    material.fullbright > 1.f ||
                    (normal_map &&
                     !sampleableTexture(material.normal_texture)) ||
                    (specular_map &&
                     !sampleableTexture(material.specular_texture)))
                {
                    setError(
                        error,
                        "Metal world pre-water legacy material is invalid");
                    return nullptr;
                }
            }
        }
        if (batch.pbr_glow || batch.pre_water_pbr)
        {
            for (std::uint32_t slot = 0;
                 slot < batch.pbr_slot_count; ++slot)
            {
                const ALMetalWorldPBRSlot& material =
                    batch.pbr_slots[slot];
                if (!sampleableTexture(material.emissive_texture) ||
                    !finiteValues(
                        &material.texture_transform[0][0], 32) ||
                    !finiteValues(material.emissive_color, 4) ||
                    !finiteValues(material.base_color_factor, 4) ||
                    !std::isfinite(material.roughness_factor) ||
                    !std::isfinite(material.metallic_factor) ||
                    material.base_color_factor[3] < 0.f ||
                    material.base_color_factor[3] > 1.f ||
                    material.roughness_factor < 0.f ||
                    material.roughness_factor > 1.f ||
                    material.metallic_factor < 0.f ||
                    material.metallic_factor > 1.f ||
                    !std::isfinite(material.alpha_cutoff) ||
                    material.alpha_cutoff < -1.f ||
                    material.alpha_cutoff > 1.f ||
                    (batch.post_water_glow &&
                     material.alpha_cutoff != 0.004f) ||
                    (batch.pre_water_pbr &&
                     (material.alpha_cutoff != -1.f ||
                      !sampleableTexture(material.normal_texture) ||
                      !sampleableTexture(material.orm_texture))))
                {
                    setError(
                        error, batch.pre_water_pbr
                            ? "Metal world pre-water PBR slot is invalid"
                            : "Metal world PBR glow slot is invalid");
                    return nullptr;
                }
            }
        }
        has_shiny = has_shiny || batch.impostor_shiny;
    }
    if (has_water_exclusion)
    {
        const ALMetalWorldWaterExclusionSurface* surface =
            scene.water_exclusion_surface;
        const ALMetalTextureDescriptor* cell_map = surface &&
                surface->cell_map
            ? &surface->cell_map->descriptor() : nullptr;
        if (!surface || !cell_map ||
            cell_map->type != ALMetalTextureType::texture_2d ||
            cell_map->format != ALMetalPixelFormat::rg32_float ||
            cell_map->width == 0 || cell_map->height == 0 ||
            cell_map->sample_count != 1 ||
            !sampleableTexture(surface->cell_map) ||
            !finiteValues(surface->eye_to_agent, 16) ||
            !finiteValues(surface->cell_square, 4) ||
            !finiteValues(surface->camera_default_height, 4) ||
            surface->cell_square[2] <= 0.f ||
            surface->cell_square[3] <= 0.f ||
            surface->cell_square[3] >
                static_cast<float>(std::min(
                    cell_map->width, cell_map->height)))
        {
            setError(error,
                     "Metal world water-exclusion surface is invalid");
            return nullptr;
        }
    }
    if (has_water_haze)
    {
        const ALMetalWorldWaterHaze* haze = scene.water_haze;
        if (!haze || !gbuffer.depth_stencil_shader_read ||
            !sampleableTexture(scene.gbuffer->depthStencilTexture()) ||
            !finiteValues(haze->modelview_projection, 16) ||
            !finiteValues(haze->inverse_projection, 16) ||
            !finiteValues(haze->cell_square, 4) ||
            !finiteValues(haze->default_height, 4) ||
            !finiteValues(haze->rings, 4) ||
            !finiteValues(haze->water_plane, 4) ||
            !finiteValues(haze->fog_color, 4) ||
            !finiteValues(haze->fog, 4) ||
            haze->fog[0] <= 0.f || haze->fog[1] < 0.f)
        {
            setError(error, "Metal world water-haze inputs are invalid");
            return nullptr;
        }
        if (haze->above_water)
        {
            const ALMetalTextureDescriptor* cell_map = haze->cell_map
                ? &haze->cell_map->descriptor() : nullptr;
            if (!cell_map ||
                cell_map->type != ALMetalTextureType::texture_2d ||
                cell_map->format != ALMetalPixelFormat::rg32_float ||
                cell_map->width == 0 || cell_map->height == 0 ||
                cell_map->sample_count != 1 ||
                !sampleableTexture(haze->cell_map) ||
                haze->cell_square[2] <= 0.f ||
                haze->cell_square[3] <= 0.f ||
                haze->cell_square[3] > static_cast<float>(std::min(
                    cell_map->width, cell_map->height)) ||
                haze->rings[0] <= 0.f || haze->rings[1] <= 0.f ||
                haze->rings[2] <= 0.f || haze->draw_count != 3)
            {
                setError(error, "Metal world water-haze field is invalid");
                return nullptr;
            }
            for (std::uint32_t draw = 0; draw < haze->draw_count;
                 ++draw)
            {
                const ALMetalWorldWaterFieldDraw& field =
                    haze->draws[draw];
                if (field.part != draw || field.vertex_count == 0 ||
                    field.instance_count == 0 ||
                    field.triangle_strip != (draw == 0))
                {
                    setError(error,
                             "Metal world water-haze field draw is invalid");
                    return nullptr;
                }
            }
        }
        else if (haze->cell_map || haze->draw_count != 0)
        {
            setError(error, "Metal world underwater haze is invalid");
            return nullptr;
        }
    }
    if (has_water_surface)
    {
        const ALMetalWorldWaterSurface* surface = scene.water_surface;
        const ALMetalWorldWaterHaze* field = surface
            ? &surface->field : nullptr;
        const ALMetalTextureDescriptor* cell_map = field && field->cell_map
            ? &field->cell_map->descriptor() : nullptr;
        if (!surface || !field || !field->above_water || !cell_map ||
            cell_map->type != ALMetalTextureType::texture_2d ||
            cell_map->format != ALMetalPixelFormat::rg32_float ||
            cell_map->width == 0 || cell_map->height == 0 ||
            cell_map->sample_count != 1 ||
            !sampleableTexture(field->cell_map) ||
            !sampleableTexture(surface->normal_maps[0]) ||
            !sampleableTexture(surface->normal_maps[1]) ||
            !finiteValues(field->modelview_projection, 16) ||
            !finiteValues(field->cell_square, 4) ||
            !finiteValues(field->default_height, 4) ||
            !finiteValues(field->rings, 4) ||
            !finiteValues(field->water_plane, 4) ||
            !finiteValues(field->fog_color, 4) ||
            !finiteValues(field->fog, 4) ||
            !finiteValues(surface->modelview, 16) ||
            !finiteValues(surface->eye_position, 4) ||
            !finiteValues(surface->big_phase, 4) ||
            !finiteValues(surface->little_phase, 4) ||
            !finiteValues(surface->sweep_phase, 4) ||
            !finiteValues(surface->water_far, 4) ||
            !finiteValues(surface->map_mean, 4) ||
            !finiteValues(surface->lattice_fraction, 4) ||
            !finiteValues(surface->normal_scale_glint, 4) ||
            !finiteValues(surface->optics, 4) ||
            !finiteValues(surface->rim, 4) ||
            !finiteValues(surface->light_direction, 4) ||
            !finiteValues(surface->parameters, 4) ||
            !finiteValues(surface->wave_eye_density, 4) ||
            !finiteValues(surface->wave_reach_amplitude, 4) ||
            field->cell_square[2] <= 0.f ||
            field->cell_square[3] <= 0.f ||
            field->cell_square[3] > static_cast<float>(std::min(
                cell_map->width, cell_map->height)) ||
            field->rings[0] <= 0.f || field->rings[1] <= 0.f ||
            field->rings[2] <= 0.f ||
            field->draw_count != (surface->waves ? 4u : 3u) ||
            field->fog[0] <= 0.f || field->fog[1] < 0.f ||
            surface->parameters[0] < 0.f ||
            surface->parameters[0] > 1.f ||
            surface->parameters[1] < 0.f ||
            surface->parameters[2] != 1.f ||
            surface->parameters[3] < 0.f ||
            surface->parameters[3] > 2.f ||
            std::floor(surface->parameters[3]) !=
                surface->parameters[3] ||
            surface->water_far[0] <= 0.f ||
            (!surface->underwater &&
             (!finiteValues(&surface->normal_matrix[0][0], 12) ||
              !finiteValues(&surface->environment_matrix[0][0], 12) ||
              surface->normal_scale_glint[3] < 0.05f ||
              surface->normal_scale_glint[3] > 1.f ||
              surface->optics[2] < 0.f ||
              surface->has_excluded_surface)))
        {
            setError(error, "Metal world water-surface inputs are invalid");
            return nullptr;
        }
        if (surface->waves)
        {
            const ALMetalTextureDescriptor* height_a =
                surface->height_maps[0]
                    ? &surface->height_maps[0]->descriptor() : nullptr;
            const ALMetalTextureDescriptor* height_b =
                surface->height_maps[1]
                    ? &surface->height_maps[1]->descriptor() : nullptr;
            const std::int64_t patch_width =
                static_cast<std::int64_t>(surface->patch_rect[2]) -
                surface->patch_rect[0];
            const std::int64_t patch_height =
                static_cast<std::int64_t>(surface->patch_rect[3]) -
                surface->patch_rect[1];
            const std::uint64_t expected_patches = patch_width > 0 &&
                    patch_height > 0
                ? static_cast<std::uint64_t>(patch_width) *
                    static_cast<std::uint64_t>(patch_height) * 64u
                : 0u;
            bool valid_factors = expected_patches ==
                    surface->patch_count &&
                surface->tessellation_factors.size() ==
                    static_cast<std::size_t>(surface->patch_count) * 6u;
            for (std::size_t patch = 0;
                 valid_factors && patch < surface->patch_count; ++patch)
            {
                bool all_zero = true;
                bool all_valid = true;
                for (std::size_t factor = 0; factor < 6; ++factor)
                {
                    const std::uint16_t value =
                        surface->tessellation_factors[patch * 6 + factor];
                    all_zero = all_zero && value == 0;
                    all_valid = all_valid &&
                        value >= 0x3c00u && value <= 0x5400u;
                }
                valid_factors = all_zero || all_valid;
            }
            if (!height_a || !height_b ||
                height_a->type != ALMetalTextureType::texture_2d ||
                height_b->type != ALMetalTextureType::texture_2d ||
                height_a->format != ALMetalPixelFormat::rg16_float ||
                height_b->format != ALMetalPixelFormat::rg16_float ||
                height_a->width == 0 || height_a->height == 0 ||
                height_b->width == 0 || height_b->height == 0 ||
                height_a->sample_count != 1 ||
                height_b->sample_count != 1 ||
                !sampleableTexture(surface->height_maps[0]) ||
                !sampleableTexture(surface->height_maps[1]) ||
                surface->wave_eye_density[3] <= 0.f ||
                surface->wave_reach_amplitude[0] <= 0.f ||
                surface->wave_reach_amplitude[1] <= 0.f ||
                surface->wave_reach_amplitude[2] < 0.f ||
                surface->wave_reach_amplitude[3] < 0.f ||
                (surface->wave_reach_amplitude[2] == 0.f &&
                 surface->wave_reach_amplitude[3] == 0.f) ||
                surface->patch_rect[0] <= 0 ||
                surface->patch_rect[1] <= 0 ||
                surface->patch_rect[2] >= field->cell_square[3] ||
                surface->patch_rect[3] >= field->cell_square[3] ||
                surface->patch_order[0] < 0 ||
                surface->patch_order[0] > 1 ||
                surface->patch_order[1] < 0 ||
                surface->patch_order[1] > 1 ||
                surface->patch_order[2] < 0 ||
                surface->patch_order[2] > 1 ||
                surface->patch_order[3] != 0 || !valid_factors)
            {
                setError(error,
                         "Metal world wave water-surface inputs are invalid");
                return nullptr;
            }
        }
        else if (surface->height_maps[0] || surface->height_maps[1] ||
                 surface->patch_count != 0 ||
                 !surface->tessellation_factors.empty() ||
                 surface->wave_occlusion)
        {
            setError(error,
                     "Metal world flat water-surface inputs are invalid");
            return nullptr;
        }
        has_above_water_surface = !surface->underwater;
        water_uses_shadows = has_above_water_surface &&
            surface->uses_shadows;
        if (has_above_water_surface)
        {
            const float light_length =
                surface->light_direction[0] *
                    surface->light_direction[0] +
                surface->light_direction[1] *
                    surface->light_direction[1] +
                surface->light_direction[2] *
                    surface->light_direction[2];
            if (light_length <= 0.f)
            {
                setError(
                    error, "Metal world water-surface inputs are invalid");
                return nullptr;
            }
        }
        for (std::uint32_t draw = 0; draw < field->draw_count; ++draw)
        {
            const ALMetalWorldWaterFieldDraw& command =
                field->draws[draw];
            if (command.part != draw || command.vertex_count == 0 ||
                command.instance_count == 0 ||
                command.triangle_strip != (draw == 0))
            {
                setError(error,
                         "Metal world water-surface field draw is invalid");
                return nullptr;
            }
        }
    }
    else if (scene.water_surface)
    {
        setError(error, "Metal world water-surface marker is missing");
        return nullptr;
    }
    if (has_atmospheric_haze)
    {
        const ALMetalWorldAtmosphericHaze& haze =
            *scene.atmospheric_haze;
        const ALMetalTextureDescriptor* sky = scene.sky_behind
            ? &scene.sky_behind->descriptor() : nullptr;
        const ALMetalPixelFormat sky_format = scene.hdr
            ? ALMetalPixelFormat::rg11b10_float
            : ALMetalPixelFormat::rgba8_unorm;
        if (!finiteValues(haze.inverse_projection, 16) ||
            !std::isfinite(haze.water_ground_fade) ||
            haze.water_ground_fade < 0.f ||
            haze.water_ground_fade > 1.f ||
            (scene.water_surface &&
             haze.above_water == scene.water_surface->underwater) ||
            (haze.sky_behind &&
             (!haze.above_water || !has_water_surface)) ||
            haze.sky_behind != (scene.sky_behind != nullptr) ||
            (!haze.sky_behind && haze.water_ground_fade != 0.f) ||
            (haze.sky_behind &&
             (!sky || sky->type != ALMetalTextureType::texture_2d ||
              sky->format != sky_format ||
              sky->width != std::max(scene.width / 2u, 1u) ||
              sky->height != std::max(scene.height / 2u, 1u) ||
              sky->sample_count != 1 ||
              !sampleableTexture(scene.sky_behind))))
        {
            setError(error,
                     "Metal world atmospheric-haze inputs are invalid");
            return nullptr;
        }
    }
    else if (scene.atmospheric_haze || scene.sky_behind)
    {
        setError(error,
                 "Metal world atmospheric-haze marker is missing");
        return nullptr;
    }
    if (has_pre_water_alpha || has_avatar_alpha ||
        has_post_water_alpha ||
        has_above_water_surface ||
        has_atmospheric_haze ||
        (has_shiny && scene.legacy_environment))
    {
        const ALMetalWorldAlphaEnvironment* environment =
            scene.alpha_environment;
        const float water_normal_length = environment
            ? environment->water_plane[0] * environment->water_plane[0] +
                  environment->water_plane[1] * environment->water_plane[1] +
                  environment->water_plane[2] * environment->water_plane[2]
            : 0.f;
        const float light_length = environment
            ? environment->light_direction[0] *
                  environment->light_direction[0] +
                  environment->light_direction[1] *
                  environment->light_direction[1] +
                  environment->light_direction[2] *
                  environment->light_direction[2]
            : 0.f;
        if (!environment ||
            !finiteValues(environment->water_plane, 4) ||
            !finiteValues(&environment->eye_to_sky[0][0], 12) ||
            !finiteValues(environment->haze_extinction, 4) ||
            !finiteValues(environment->glow_max_y, 4) ||
            !finiteValues(environment->haze_additive, 4) ||
            !finiteValues(environment->haze_additive_glow, 4) ||
            !finiteValues(environment->water_fog_color, 4) ||
            !finiteValues(environment->light_direction, 4) ||
            !finiteValues(environment->atmos_sunlit, 4) ||
            !finiteValues(environment->atmos_sunlit_linear, 4) ||
            !finiteValues(environment->atmos_amblit, 4) ||
            !finiteValues(environment->atmospheric, 4) ||
            !finiteValues(environment->water_fog, 4) ||
            !finiteValues(&environment->environment_matrix[0][0], 12) ||
            !finiteValues(environment->environment, 4) ||
            water_normal_length <= 0.f || light_length <= 0.f ||
            environment->atmospheric[0] < 0.f ||
            environment->atmospheric[1] < 0.f ||
            environment->atmospheric[3] < 0.f ||
            environment->atmos_sunlit[3] < 0.f ||
            environment->atmos_sunlit_linear[3] < 0.f ||
            environment->water_fog[0] <= 0.f ||
            environment->water_fog[1] < 0.f ||
            std::abs(environment->water_fog[2]) != 1.f ||
            environment->environment_config[0] != 0 ||
            environment->environment_config[1] != 0 ||
            environment->environment_config[2] != 0 ||
            environment->environment_config[3] != 0 ||
            (has_atmospheric_haze &&
             ((scene.atmospheric_haze->above_water &&
               environment->water_fog[2] != -1.f) ||
              (!scene.atmospheric_haze->above_water &&
               environment->water_fog[2] != 1.f))))
        {
            setError(error,
                     "Metal world pre-water alpha environment is invalid");
            return nullptr;
        }
    }
    const bool probe_environment =
        validPBRReflectionProbes(scene.reflection_probes);
    const bool legacy_environment =
        validLegacyEnvironment(scene.legacy_environment);
    const bool has_forward_environment =
        has_shiny || has_pre_water_simple || has_pre_water_material ||
        has_pre_water_pbr || has_above_water_surface;
    if ((has_pre_water_simple || has_pre_water_material ||
         has_pre_water_pbr) &&
        !validLegacyAlphaLighting(scene.alpha_lighting))
    {
        setError(error,
                 "Metal world pre-water alpha lighting is invalid");
        return nullptr;
    }
    if ((scene.legacy_environment && !legacy_environment) ||
        (scene.reflection_probes && !probe_environment) ||
        (scene.legacy_environment && scene.reflection_probes) ||
        (has_forward_environment &&
         probe_environment == legacy_environment) ||
        (legacy_environment &&
         (scene.hero_probe || scene.screen_space_reflections)))
    {
        setError(
            error,
            has_above_water_surface
                ? "Metal world above-water environment is invalid"
                : "Metal world forward environment is invalid");
        return nullptr;
    }
    if (has_above_water_surface &&
        !validLegacyAlphaLighting(scene.alpha_lighting))
    {
        setError(error,
                 "Metal world pre-water alpha lighting is invalid");
        return nullptr;
    }
    if ((has_pre_water_shadows || water_uses_shadows) !=
            (scene.alpha_directional_shadows != nullptr) ||
        ((has_pre_water_shadows || water_uses_shadows) &&
         !validDirectionalShadows(scene.alpha_directional_shadows)))
    {
        setError(error,
                 "Metal world pre-water alpha shadows are invalid");
        return nullptr;
    }
    if (scene.hero_probe && !validHeroProbe(scene.hero_probe))
    {
        setError(error,
                 "Metal world opaque post-deferred hero probe is invalid");
        return nullptr;
    }
    if (scene.screen_space_reflections &&
        !validScreenSpaceReflections(scene.screen_space_reflections))
    {
        setError(
            error,
            "Metal world opaque post-deferred screen-space reflections are invalid");
        return nullptr;
    }

    std::vector<std::unique_ptr<ALMetalBuffer>> palette_storage;
    std::vector<const ALMetalBuffer*> palette_buffers(
        scene.batch_count, nullptr);
    for (std::size_t batch_index = 0;
         batch_index < scene.batch_count; ++batch_index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[batch_index];
        if (!batch.skin_palette)
        {
            continue;
        }
        for (std::size_t previous = 0; previous < batch_index; ++previous)
        {
            const ALMetalWorldSimpleBatch& candidate = scene.batches[previous];
            if (candidate.skin_palette == batch.skin_palette &&
                candidate.skin_palette_float_count ==
                    batch.skin_palette_float_count &&
                candidate.skin_joint_count == batch.skin_joint_count)
            {
                palette_buffers[batch_index] = palette_buffers[previous];
                break;
            }
        }
        if (palette_buffers[batch_index])
        {
            continue;
        }
        ALMetalBufferDescriptor descriptor;
        descriptor.length = batch.skin_palette_float_count * sizeof(float);
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label = "Alchemy world opaque post-deferred skin palette";
        auto palette = ALMetalBuffer::create(*mImpl->device, descriptor, error);
        if (!palette || !palette->write(
                0, batch.skin_palette, descriptor.length, error))
        {
            return nullptr;
        }
        palette_buffers[batch_index] = palette.get();
        palette_storage.push_back(std::move(palette));
    }

    std::unique_ptr<ALMetalBuffer> probe_constants_buffer;
    if (has_shiny || has_pre_water_simple || has_pre_water_material ||
        has_pre_water_pbr || has_above_water_surface)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length = sizeof(ALMetalWorldReflectionProbeConstants);
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label =
            "Alchemy world opaque post-deferred reflection constants";
        probe_constants_buffer = ALMetalBuffer::create(
            *mImpl->device, descriptor, error);
        const ALMetalWorldReflectionProbeConstants inactive_constants{};
        const ALMetalWorldReflectionProbeConstants& constants =
            scene.reflection_probes
                ? scene.reflection_probes->constants
                : inactive_constants;
        if (!probe_constants_buffer || !probe_constants_buffer->write(
                0, &constants,
                descriptor.length, error))
        {
            return nullptr;
        }
    }

    std::unique_ptr<ALMetalBuffer> water_factor_buffer;
    if (has_water_surface && scene.water_surface->waves)
    {
        const std::vector<std::uint16_t>& factors =
            scene.water_surface->tessellation_factors;
        ALMetalBufferDescriptor factor_descriptor;
        factor_descriptor.length = factors.size() * sizeof(factors.front());
        factor_descriptor.storage = ALMetalStorageMode::shared;
        factor_descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        factor_descriptor.label =
            "Alchemy world water tessellation factors";
        water_factor_buffer = ALMetalBuffer::create(
            *mImpl->device, factor_descriptor, error);
        if (!water_factor_buffer || !water_factor_buffer->write(
                0, factors.data(), factor_descriptor.length, error))
        {
            return nullptr;
        }
    }

    const bool water_wave_occlusion = has_water_surface &&
        scene.water_surface->waves &&
        scene.water_surface->wave_occlusion;
    if (water_wave_occlusion)
    {
        constexpr std::uint64_t base_alignment = 128u;
        const auto base_size = [](std::uint32_t value)
        {
            constexpr std::uint64_t alignment = 128u;
            const std::uint64_t half =
                (static_cast<std::uint64_t>(value) + 1u) / 2u;
            return (half + alignment - 1u) / alignment * alignment;
        };
        const std::uint64_t base_width = base_size(scene.width);
        const std::uint64_t base_height = base_size(scene.height);
        if (base_width > std::numeric_limits<std::uint32_t>::max() ||
            base_height > std::numeric_limits<std::uint32_t>::max() ||
            base_width < base_alignment || base_height < base_alignment)
        {
            setError(error,
                     "Metal world water depth-pyramid size is invalid");
            return nullptr;
        }
        const bool recreate = !mImpl->water_depth_pyramid[0] ||
            mImpl->water_depth_pyramid[0]->width() != base_width ||
            mImpl->water_depth_pyramid[0]->height() != base_height;
        if (recreate)
        {
            std::array<std::unique_ptr<ALMetalTexture>, 8> pyramid;
            ALMetalTextureDescriptor descriptor;
            descriptor.format = ALMetalPixelFormat::r32_float;
            descriptor.storage = ALMetalStorageMode::private_gpu;
            descriptor.usage = ALMetalTextureUsage::shader_read |
                               ALMetalTextureUsage::shader_write;
            for (std::size_t level = 0; level < pyramid.size(); ++level)
            {
                descriptor.width = static_cast<std::uint32_t>(
                    base_width >> level);
                descriptor.height = static_cast<std::uint32_t>(
                    base_height >> level);
                descriptor.label =
                    "Alchemy world water depth pyramid level " +
                    std::to_string(level);
                pyramid[level] = ALMetalTexture::create(
                    *mImpl->device, descriptor, error);
                if (!pyramid[level])
                {
                    return nullptr;
                }
            }
            mImpl->water_depth_pyramid = std::move(pyramid);
        }
    }

    const bool needs_shader_read_depth = water_wave_occlusion ||
        scene.publish_screen_space_reflection_history;
    if (continue_composed_target && needs_shader_read_depth &&
        !lighting.depth_stencil_shader_read)
    {
        setError(
            error,
            "Metal world opaque post-deferred continuation depth is not shader-readable");
        return nullptr;
    }
    auto output = std::move(composed_lighting_owner);
    if (!output)
    {
        ALMetalRenderTargetDescriptor output_descriptor;
        output_descriptor.width = scene.width;
        output_descriptor.height = scene.height;
        output_descriptor.colors[0].format = color_format;
        output_descriptor.colors[0].label =
            "Alchemy world post-deferred final color";
        output_descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        output_descriptor.depth_stencil_shader_read =
            needs_shader_read_depth;
        output_descriptor.label =
            "Alchemy native world opaque post-deferred target";
        output = ALMetalRenderTarget::create(
            *mImpl->device, output_descriptor, error);
    }
    if (!output)
    {
        return nullptr;
    }
    std::unique_ptr<ALMetalRenderTarget> haze_depth_snapshot;
    if (has_water_haze)
    {
        ALMetalRenderTargetDescriptor haze_depth_descriptor;
        haze_depth_descriptor.width = scene.width;
        haze_depth_descriptor.height = scene.height;
        haze_depth_descriptor.color_attachment_count = 0;
        haze_depth_descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        haze_depth_descriptor.depth_stencil_shader_read = true;
        haze_depth_descriptor.label =
            "Alchemy native world water-haze depth snapshot";
        haze_depth_snapshot = ALMetalRenderTarget::create(
            *mImpl->device, haze_depth_descriptor, error);
        if (!haze_depth_snapshot)
        {
            return nullptr;
        }
    }
    std::unique_ptr<ALMetalRenderTarget> atmospheric_depth_snapshot;
    if (has_atmospheric_haze)
    {
        ALMetalRenderTargetDescriptor depth_descriptor;
        depth_descriptor.width = scene.width;
        depth_descriptor.height = scene.height;
        depth_descriptor.color_attachment_count = 0;
        depth_descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        depth_descriptor.depth_stencil_shader_read = true;
        depth_descriptor.label =
            "Alchemy native world atmospheric-haze depth snapshot";
        atmospheric_depth_snapshot = ALMetalRenderTarget::create(
            *mImpl->device, depth_descriptor, error);
        if (!atmospheric_depth_snapshot)
        {
            return nullptr;
        }
    }
    std::unique_ptr<ALMetalRenderTarget> water_color_snapshot;
    std::unique_ptr<ALMetalRenderTarget> water_depth_snapshot;
    if (has_water_surface && scene.water_surface->refracts)
    {
        ALMetalRenderTargetDescriptor water_color_descriptor;
        water_color_descriptor.width = scene.width;
        water_color_descriptor.height = scene.height;
        water_color_descriptor.colors[0].format = color_format;
        water_color_descriptor.colors[0].label =
            "Alchemy water refraction color";
        water_color_descriptor.label =
            "Alchemy native water refraction snapshot";
        water_color_snapshot = ALMetalRenderTarget::create(
            *mImpl->device, water_color_descriptor, error);
        if (!water_color_snapshot)
        {
            return nullptr;
        }
        if (has_above_water_surface)
        {
            ALMetalRenderTargetDescriptor water_depth_descriptor;
            water_depth_descriptor.width = scene.width;
            water_depth_descriptor.height = scene.height;
            water_depth_descriptor.color_attachment_count = 0;
            water_depth_descriptor.depth_stencil_format =
                ALMetalPixelFormat::depth32_float_stencil8;
            water_depth_descriptor.depth_stencil_shader_read = true;
            water_depth_descriptor.label =
                "Alchemy native water refraction depth snapshot";
            water_depth_snapshot = ALMetalRenderTarget::create(
                *mImpl->device, water_depth_descriptor, error);
            if (!water_depth_snapshot)
            {
                return nullptr;
            }
        }
    }
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world opaque post-deferred", owned_command, error);
    const auto fail_command = [&]()
        -> std::unique_ptr<ALMetalRenderTarget>
    {
        mImpl->finishCommandBuffer(owned_command, false, error);
        return nullptr;
    };
    if (!command ||
        (!continue_composed_target &&
         (!output->copyColorFrom(
              *command, 0, *scene.composed_lighting->colorTexture(0), {},
              error) ||
          !output->copyDepthStencilFrom(
              *command, *scene.gbuffer->depthStencilTexture(), {}, error))))
    {
        return fail_command();
    }

    constexpr std::uint32_t pre_water_brdf_lut_size = 512;
    const bool generate_pre_water_brdf_lut =
        has_pre_water_pbr && !mImpl->brdf_lut_ready &&
        !mImpl->command_batch_generates_brdf_lut;
    if (generate_pre_water_brdf_lut)
    {
        auto compute = ALMetalComputeEncoder::begin(
            *command, "Alchemy world pre-water PBR BRDF LUT encoder",
            error);
        if (!compute ||
            !compute->setPipeline(*mImpl->brdf_lut_pipeline, error) ||
            !compute->setTexture(0, mImpl->brdf_lut.get(), error) ||
            !compute->dispatch(
                pre_water_brdf_lut_size, pre_water_brdf_lut_size, 1,
                8, 8, 1, error) ||
            !compute->end(error))
        {
            return fail_command();
        }
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::load;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::clear;
    pass_config.stencil_store = ALMetalStoreAction::store;
    pass_config.clear_stencil = 0;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass,
        "Alchemy world opaque post-deferred encoder", error) : nullptr;
    if (!encoder)
    {
        return fail_command();
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setFrontFacingWinding(ALMetalWinding::clockwise, error) &&
        encoder->setFragmentSampler(0, mImpl->diffuse_sampler.get(), error);
    const auto bind_forward_resources = [&](ALMetalRenderEncoder& active)
    {
        bool bound = true;
        if (has_shiny || has_pre_water_simple || has_pre_water_material ||
            has_pre_water_pbr || has_above_water_surface)
        {
            const ALMetalTexture* probe_radiance = scene.reflection_probes
                ? scene.reflection_probes->radiance_texture
                : mImpl->inactive_probe_radiance.get();
            bound =
            active.setFragmentSampler(
                1, mImpl->probe_sampler.get(), error) &&
            active.setFragmentTexture(
                32, probe_radiance, error) &&
            active.setFragmentBuffer(
                1, *probe_constants_buffer, 0, error);
        }
        if (bound && (has_pre_water_simple || has_pre_water_material ||
                      has_pre_water_pbr))
        {
            const ALMetalTexture* probe_irradiance =
                scene.reflection_probes
                    ? scene.reflection_probes->irradiance_texture
                    : mImpl->inactive_probe_irradiance.get();
            bound = active.setFragmentTexture(
                37, probe_irradiance, error);
        }
        if (bound && (has_shiny || has_pre_water_simple ||
                      has_pre_water_material || has_pre_water_pbr))
        {
            const ALMetalTexture* legacy_texture =
                scene.legacy_environment
                    ? scene.legacy_environment
                    : mImpl->inactive_hero_probe.get();
            bound = active.setFragmentTexture(
                39, legacy_texture, error);
        }
        if (bound && (has_shiny || has_pre_water_material ||
                      has_pre_water_pbr))
        {
            const ALMetalTexture* hero_texture = scene.hero_probe
                ? scene.hero_probe->radiance_texture
                : mImpl->inactive_hero_probe.get();
            const ALMetalWorldHeroProbeConstants& hero_constants =
                scene.hero_probe
                    ? scene.hero_probe->constants
                    : mImpl->inactive_hero_constants;
            bound =
                active.setFragmentTexture(
                    34, hero_texture, error) &&
                active.setFragmentBytes(
                    4, &hero_constants, sizeof(hero_constants), error);
        }
        if (bound && (has_pre_water_material || has_pre_water_pbr))
        {
            const ALMetalTexture* ssr_color =
                scene.screen_space_reflections
                    ? scene.screen_space_reflections->scene_color
                    : mImpl->inactive_ssr_color.get();
            const ALMetalTexture* ssr_depth =
                scene.screen_space_reflections
                    ? scene.screen_space_reflections->scene_depth
                    : mImpl->inactive_ssr_depth.get();
            const ALMetalWorldScreenSpaceReflectionConstants&
                ssr_constants = scene.screen_space_reflections
                    ? scene.screen_space_reflections->constants
                    : mImpl->inactive_ssr_constants;
            bound =
                active.setFragmentSampler(
                    2, mImpl->linear_clamp_sampler.get(), error) &&
                active.setFragmentSampler(
                    3, mImpl->point_clamp_sampler.get(), error) &&
                active.setFragmentTexture(35, ssr_color, error) &&
                active.setFragmentTexture(36, ssr_depth, error) &&
                active.setFragmentBytes(
                    5, &ssr_constants, sizeof(ssr_constants), error);
        }
        if (bound && has_pre_water_pbr)
        {
            bound = active.setFragmentTexture(
                38, mImpl->brdf_lut.get(), error);
        }
        return bound;
    };
    encoded = encoded && bind_forward_resources(*encoder);

    bool cutout_coverage_seeded = !scene.seed_cutout_coverage;
    const auto seed_cutout_coverage = [&]()
    {
        if (cutout_coverage_seeded)
        {
            return true;
        }
        const std::uint32_t reverse_z = scene.reverse_z ? 1u : 0u;
        const std::size_t format = scene.hdr ? 1u : 0u;
        const ALMetalDepthStencilState* background_depth = scene.reverse_z
            ? mImpl->reverse_readonly_depth.get()
            : mImpl->forward_readonly_depth.get();
        const ALMetalDepthStencilState* coverage_depth = scene.reverse_z
            ? mImpl->reverse_coverage_depth.get()
            : mImpl->forward_coverage_depth.get();
        const bool seeded =
            encoder->setCullMode(ALMetalCullMode::none, error) &&
            encoder->setDepthBias(0.f, 0.f, 0.f, error) &&
            encoder->setVertexBytes(
                0, &reverse_z, sizeof(reverse_z), error) &&
            encoder->setDepthStencilState(background_depth, error) &&
            encoder->setPipeline(
                *mImpl->cutout_background_pipelines[format], error) &&
            encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
            encoder->setDepthStencilState(coverage_depth, error) &&
            encoder->setPipeline(
                *mImpl->cutout_coverage_pipelines[format], error) &&
            encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error);
        cutout_coverage_seeded = seeded;
        return seeded;
    };

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        const bool rigged = batch.skin_palette != nullptr;
        if (batch.water_haze)
        {
            const ALMetalWorldWaterHaze& haze = *scene.water_haze;
            if (!encoder->end(error) || !haze_depth_snapshot ||
                !haze_depth_snapshot->copyDepthStencilFrom(
                    *command, *output->depthStencilTexture(), {}, error))
            {
                return fail_command();
            }
            ALMetalRenderPassConfig haze_pass_config;
            haze_pass_config.colors[0].load = ALMetalLoadAction::load;
            haze_pass_config.colors[0].store = ALMetalStoreAction::store;
            haze_pass_config.depth_load = ALMetalLoadAction::load;
            haze_pass_config.depth_store = ALMetalStoreAction::store;
            haze_pass_config.stencil_load = ALMetalLoadAction::load;
            haze_pass_config.stencil_store = ALMetalStoreAction::store;
            auto haze_pass = output->makeRenderPass(
                haze_pass_config, error);
            encoder = haze_pass ? ALMetalRenderEncoder::begin(
                *command, *haze_pass,
                "Alchemy world water-haze encoder", error) : nullptr;
            if (!encoder)
            {
                return fail_command();
            }
            encoded =
                encoder->setViewport(viewport, error) &&
                encoder->setScissor(scissor, error) &&
                encoder->setFrontFacingWinding(
                    ALMetalWinding::clockwise, error) &&
                encoder->setFragmentSampler(
                    0, mImpl->diffuse_sampler.get(), error);
            encoded = encoded && bind_forward_resources(*encoder);
            WorldWaterHazeConstants constants{};
            std::memcpy(constants.modelview_projection,
                        haze.modelview_projection,
                        sizeof(constants.modelview_projection));
            std::memcpy(constants.inverse_projection,
                        haze.inverse_projection,
                        sizeof(constants.inverse_projection));
            std::memcpy(constants.cell_square, haze.cell_square,
                        sizeof(constants.cell_square));
            std::memcpy(constants.default_height, haze.default_height,
                        sizeof(constants.default_height));
            std::memcpy(constants.rings, haze.rings,
                        sizeof(constants.rings));
            std::memcpy(constants.water_plane, haze.water_plane,
                        sizeof(constants.water_plane));
            std::memcpy(constants.fog_color, haze.fog_color,
                        sizeof(constants.fog_color));
            std::memcpy(constants.fog, haze.fog,
                        sizeof(constants.fog));
            constants.config[0] = scene.reverse_z ? 1u : 0u;
            constants.config[1] = haze.above_water ? 1u : 0u;

            const ALMetalDepthStencilState* haze_depth =
                haze.above_water
                    ? (scene.reverse_z
                           ? mImpl->reverse_water_haze_depth.get()
                           : mImpl->forward_water_haze_depth.get())
                    : mImpl->underwater_water_haze_depth.get();
            encoded =
                encoder->setDepthStencilState(haze_depth, error) &&
                encoder->setDepthBias(0.f, 0.f, 0.f, error) &&
                encoder->setStencilReferenceValue(0u, error) &&
                encoder->setCullMode(ALMetalCullMode::none, error) &&
                encoder->setPipeline(
                    *mImpl->water_haze_pipelines
                        [scene.hdr ? 1 : 0][haze.above_water ? 1 : 0],
                    error) &&
                encoder->setFragmentTexture(
                    33, haze_depth_snapshot->depthStencilTexture(),
                    error) &&
                encoder->setFragmentSampler(
                    1, mImpl->point_clamp_sampler.get(), error) &&
                encoder->setFragmentBytes(
                    0, &constants, sizeof(constants), error);
            if (encoded && haze.above_water)
            {
                encoded = encoder->setVertexTexture(
                    0, haze.cell_map, error);
                for (std::uint32_t draw = 0;
                     encoded && draw < haze.draw_count; ++draw)
                {
                    const ALMetalWorldWaterFieldDraw& field =
                        haze.draws[draw];
                    constants.config[2] = field.part;
                    encoded =
                        encoder->setVertexBytes(
                            0, &constants, sizeof(constants), error) &&
                        encoder->draw(
                            field.triangle_strip
                                ? ALMetalPrimitiveType::triangle_strip
                                : ALMetalPrimitiveType::triangle,
                            0, field.vertex_count,
                            field.instance_count, error);
                }
            }
            else if (encoded)
            {
                encoded =
                    encoder->setVertexBytes(
                        0, &constants, sizeof(constants), error) &&
                    encoder->draw(
                        ALMetalPrimitiveType::triangle, 0, 3, 1, error);
            }
            continue;
        }
        if (batch.atmospheric_haze)
        {
            const ALMetalWorldAtmosphericHaze& haze =
                *scene.atmospheric_haze;
            if (!encoder->end(error) || !atmospheric_depth_snapshot ||
                !atmospheric_depth_snapshot->copyDepthStencilFrom(
                    *command, *output->depthStencilTexture(), {}, error))
            {
                return fail_command();
            }

            ALMetalRenderPassConfig haze_pass_config;
            haze_pass_config.colors[0].load = ALMetalLoadAction::load;
            haze_pass_config.colors[0].store = ALMetalStoreAction::store;
            haze_pass_config.depth_load = ALMetalLoadAction::load;
            haze_pass_config.depth_store = ALMetalStoreAction::store;
            haze_pass_config.stencil_load = ALMetalLoadAction::load;
            haze_pass_config.stencil_store = ALMetalStoreAction::store;
            auto haze_pass = output->makeRenderPass(
                haze_pass_config, error);
            encoder = haze_pass ? ALMetalRenderEncoder::begin(
                *command, *haze_pass,
                "Alchemy world atmospheric-haze encoder", error)
                : nullptr;
            if (!encoder)
            {
                return fail_command();
            }

            WorldAtmosphericHazeConstants constants{};
            std::memcpy(constants.inverse_projection,
                        haze.inverse_projection,
                        sizeof(constants.inverse_projection));
            constants.parameters[0] = haze.water_ground_fade;
            constants.config[0] = scene.reverse_z ? 1u : 0u;
            const ALMetalTexture* sky_behind = haze.sky_behind
                ? scene.sky_behind : mImpl->inactive_ssr_color.get();
            const ALMetalDepthStencilState* depth = haze.sky_behind
                ? (scene.reverse_z
                       ? mImpl->reverse_atmospheric_haze_depth.get()
                       : mImpl->forward_atmospheric_haze_depth.get())
                : (scene.reverse_z
                       ? mImpl->reverse_coverage_depth.get()
                       : mImpl->forward_coverage_depth.get());
            encoded =
                encoder->setViewport(viewport, error) &&
                encoder->setScissor(scissor, error) &&
                encoder->setCullMode(ALMetalCullMode::none, error) &&
                encoder->setDepthBias(0.f, 0.f, 0.f, error) &&
                encoder->setDepthStencilState(depth, error) &&
                encoder->setPipeline(
                    *mImpl->atmospheric_haze_pipelines
                        [scene.hdr ? 1 : 0]
                        [haze.alpha_writes_coverage ? 1 : 0],
                    error) &&
                encoder->setFragmentTexture(
                    33,
                    atmospheric_depth_snapshot->depthStencilTexture(),
                    error) &&
                encoder->setFragmentTexture(34, sky_behind, error) &&
                encoder->setFragmentSampler(
                    1, mImpl->point_clamp_sampler.get(), error) &&
                encoder->setFragmentSampler(
                    2, mImpl->linear_clamp_sampler.get(), error) &&
                encoder->setFragmentBytes(
                    1, scene.alpha_environment,
                    sizeof(*scene.alpha_environment), error);
            if (encoded && haze.sky_behind)
            {
                constants.config[1] = 0u;
                encoded =
                    encoder->setStencilReferenceValue(0u, error) &&
                    encoder->setVertexBytes(
                        0, &constants, sizeof(constants), error) &&
                    encoder->setFragmentBytes(
                        0, &constants, sizeof(constants), error) &&
                    encoder->draw(
                        ALMetalPrimitiveType::triangle, 0, 3, 1, error);
                constants.config[1] = 1u;
                encoded = encoded &&
                    encoder->setStencilReferenceValue(2u, error) &&
                    encoder->setVertexBytes(
                        0, &constants, sizeof(constants), error) &&
                    encoder->setFragmentBytes(
                        0, &constants, sizeof(constants), error) &&
                    encoder->draw(
                        ALMetalPrimitiveType::triangle, 0, 3, 1, error);
            }
            else if (encoded)
            {
                constants.config[1] = 0u;
                encoded =
                    encoder->setStencilReferenceValue(0u, error) &&
                    encoder->setVertexBytes(
                        0, &constants, sizeof(constants), error) &&
                    encoder->setFragmentBytes(
                        0, &constants, sizeof(constants), error) &&
                    encoder->draw(
                        ALMetalPrimitiveType::triangle, 0, 3, 1, error);
            }
            continue;
        }
        if (batch.water_surface)
        {
            const ALMetalWorldWaterSurface& water =
                *scene.water_surface;
            const ALMetalWorldWaterHaze& field = water.field;
            if (!encoder->end(error) ||
                (water.refracts &&
                 (!water_color_snapshot ||
                  !water_color_snapshot->copyColorFrom(
                      *command, 0, *output->colorTexture(0), {}, error))) ||
                (!water.underwater && water.refracts &&
                 (!water_depth_snapshot ||
                  !water_depth_snapshot->copyDepthStencilFrom(
                      *command, *output->depthStencilTexture(), {},
                      error))))
            {
                return fail_command();
            }

            if (water.wave_occlusion)
            {
                const std::uint32_t reverse_z =
                    scene.reverse_z ? 1u : 0u;
                auto first = ALMetalComputeEncoder::begin(
                    *command,
                    "Alchemy world water depth-pyramid first encoder",
                    error);
                bool computed = first && first->setPipeline(
                    *mImpl->water_depth_pyramid_first_pipeline, error) &&
                    first->setTexture(
                        0, output->depthStencilTexture(), error) &&
                    first->setBytes(
                        0, &reverse_z, sizeof(reverse_z), error);
                for (std::size_t level = 0;
                     computed && level < 5; ++level)
                {
                    computed = first->setTexture(
                        static_cast<std::uint32_t>(level + 1),
                        mImpl->water_depth_pyramid[level].get(), error);
                }
                computed = computed && first->dispatch(
                    mImpl->water_depth_pyramid[0]->width(),
                    mImpl->water_depth_pyramid[0]->height(), 1,
                    16, 16, 1, error) && first->end(error);
                if (!computed)
                {
                    return fail_command();
                }

                auto last = ALMetalComputeEncoder::begin(
                    *command,
                    "Alchemy world water depth-pyramid last encoder",
                    error);
                computed = last && last->setPipeline(
                    *mImpl->water_depth_pyramid_last_pipeline, error) &&
                    last->setTexture(
                        0, mImpl->water_depth_pyramid[4].get(), error) &&
                    last->setBytes(
                        0, &reverse_z, sizeof(reverse_z), error);
                for (std::size_t level = 5;
                     computed && level < 8; ++level)
                {
                    computed = last->setTexture(
                        static_cast<std::uint32_t>(level - 4),
                        mImpl->water_depth_pyramid[level].get(), error);
                }
                const std::uint32_t last_width =
                    (mImpl->water_depth_pyramid[5]->width() + 15u) & ~15u;
                const std::uint32_t last_height =
                    (mImpl->water_depth_pyramid[5]->height() + 15u) & ~15u;
                computed = computed && last->dispatch(
                    last_width, last_height, 1, 16, 16, 1, error) &&
                    last->end(error);
                if (!computed)
                {
                    return fail_command();
                }

                WorldWaterOcclusionConstants constants{};
                std::memcpy(constants.modelview_projection,
                            field.modelview_projection,
                            sizeof(constants.modelview_projection));
                std::memcpy(constants.cell_square, field.cell_square,
                            sizeof(constants.cell_square));
                constants.screen_displacement[0] =
                    static_cast<float>(scene.width);
                constants.screen_displacement[1] =
                    static_cast<float>(scene.height);
                constants.screen_displacement[2] =
                    water.wave_reach_amplitude[1];
                std::memcpy(constants.patch_rect, water.patch_rect,
                            sizeof(constants.patch_rect));
                std::memcpy(constants.patch_order, water.patch_order,
                            sizeof(constants.patch_order));
                constants.config[0] = water.patch_count;
                constants.config[1] = 8u;
                constants.config[2] = reverse_z;

                auto cull = ALMetalComputeEncoder::begin(
                    *command,
                    "Alchemy world water wave-occlusion encoder", error);
                computed = cull && cull->setPipeline(
                    *mImpl->water_wave_occlusion_pipeline, error) &&
                    water_factor_buffer && cull->setBuffer(
                        0, *water_factor_buffer, 0, error) &&
                    cull->setBytes(
                        1, &constants, sizeof(constants), error) &&
                    cull->setTexture(0, field.cell_map, error);
                for (std::size_t level = 0;
                     computed && level < 8; ++level)
                {
                    computed = cull->setTexture(
                        static_cast<std::uint32_t>(level + 1),
                        mImpl->water_depth_pyramid[level].get(), error);
                }
                computed = computed && cull->dispatch(
                    water.patch_count, 1, 1, 64, 1, 1, error) &&
                    cull->end(error);
                if (!computed)
                {
                    return fail_command();
                }
            }

            ALMetalRenderPassConfig water_pass_config;
            water_pass_config.colors[0].load = ALMetalLoadAction::load;
            water_pass_config.colors[0].store = ALMetalStoreAction::store;
            water_pass_config.depth_load = ALMetalLoadAction::load;
            water_pass_config.depth_store = ALMetalStoreAction::store;
            water_pass_config.stencil_load = ALMetalLoadAction::load;
            water_pass_config.stencil_store = ALMetalStoreAction::store;
            auto water_pass = output->makeRenderPass(
                water_pass_config, error);
            encoder = water_pass ? ALMetalRenderEncoder::begin(
                *command, *water_pass,
                water.underwater
                    ? "Alchemy world underwater surface encoder"
                    : "Alchemy world above-water surface encoder",
                error)
                : nullptr;
            if (!encoder)
            {
                return fail_command();
            }

            WorldWaterHazeConstants field_constants{};
            std::memcpy(field_constants.modelview_projection,
                        field.modelview_projection,
                        sizeof(field_constants.modelview_projection));
            std::memcpy(field_constants.inverse_projection,
                        field.inverse_projection,
                        sizeof(field_constants.inverse_projection));
            std::memcpy(field_constants.cell_square, field.cell_square,
                        sizeof(field_constants.cell_square));
            std::memcpy(field_constants.default_height,
                        field.default_height,
                        sizeof(field_constants.default_height));
            std::memcpy(field_constants.rings, field.rings,
                        sizeof(field_constants.rings));
            std::memcpy(field_constants.water_plane, field.water_plane,
                        sizeof(field_constants.water_plane));
            std::memcpy(field_constants.fog_color, field.fog_color,
                        sizeof(field_constants.fog_color));
            std::memcpy(field_constants.fog, field.fog,
                        sizeof(field_constants.fog));
            std::memcpy(field_constants.patch_rect,
                        water.patch_rect,
                        sizeof(field_constants.patch_rect));
            field_constants.config[0] = scene.reverse_z ? 1u : 0u;
            field_constants.config[1] = 1u;

            WorldWaterSurfaceConstants water_constants{};
            std::memcpy(water_constants.modelview, water.modelview,
                        sizeof(water_constants.modelview));
            std::memcpy(water_constants.normal_matrix,
                        water.normal_matrix,
                        sizeof(water_constants.normal_matrix));
            std::memcpy(water_constants.environment_matrix,
                        water.environment_matrix,
                        sizeof(water_constants.environment_matrix));
            std::memcpy(water_constants.eye_position,
                        water.eye_position,
                        sizeof(water_constants.eye_position));
            std::memcpy(water_constants.big_phase, water.big_phase,
                        sizeof(water_constants.big_phase));
            std::memcpy(water_constants.little_phase,
                        water.little_phase,
                        sizeof(water_constants.little_phase));
            std::memcpy(water_constants.sweep_phase,
                        water.sweep_phase,
                        sizeof(water_constants.sweep_phase));
            std::memcpy(water_constants.water_far, water.water_far,
                        sizeof(water_constants.water_far));
            std::memcpy(water_constants.map_mean, water.map_mean,
                        sizeof(water_constants.map_mean));
            std::memcpy(water_constants.lattice_fraction,
                        water.lattice_fraction,
                        sizeof(water_constants.lattice_fraction));
            std::memcpy(water_constants.lattice_base,
                        water.lattice_base,
                        sizeof(water_constants.lattice_base));
            std::memcpy(water_constants.normal_scale_glint,
                        water.normal_scale_glint,
                        sizeof(water_constants.normal_scale_glint));
            std::memcpy(water_constants.optics, water.optics,
                        sizeof(water_constants.optics));
            std::memcpy(water_constants.rim, water.rim,
                        sizeof(water_constants.rim));
            std::memcpy(water_constants.light_direction,
                        water.light_direction,
                        sizeof(water_constants.light_direction));
            std::memcpy(water_constants.parameters,
                        water.parameters,
                        sizeof(water_constants.parameters));
            std::memcpy(water_constants.wave_eye_density,
                        water.wave_eye_density,
                        sizeof(water_constants.wave_eye_density));
            std::memcpy(water_constants.wave_reach_amplitude,
                        water.wave_reach_amplitude,
                        sizeof(water_constants.wave_reach_amplitude));
            std::memcpy(water_constants.patch_rect,
                        water.patch_rect,
                        sizeof(water_constants.patch_rect));
            std::memcpy(water_constants.patch_order,
                        water.patch_order,
                        sizeof(water_constants.patch_order));
            water_constants.screen[0] = static_cast<float>(scene.width);
            water_constants.screen[1] = static_cast<float>(scene.height);
            water_constants.config[0] = scene.reverse_z ? 1u : 0u;
            water_constants.config[1] = water.uses_shadows ? 1u : 0u;
            water_constants.config[2] = static_cast<std::uint32_t>(
                water.parameters[3]);
            water_constants.config[3] = water.refracts ? 1u : 0u;
            water_constants.environment_config[0] =
                scene.legacy_environment ? 1u : 0u;
            water_constants.environment_config[1] =
                scene.mirror_clip ? 1u : 0u;
            std::memcpy(water_constants.mirror_clip_plane,
                        scene.mirror_clip_plane,
                        sizeof(water_constants.mirror_clip_plane));

            const ALMetalTexture* refraction = water.refracts
                ? water_color_snapshot->colorTexture(0)
                : mImpl->inactive_ssr_color.get();
            const ALMetalTexture* refraction_depth =
                !water.underwater && water.refracts
                    ? water_depth_snapshot->depthStencilTexture()
                    : mImpl->inactive_ssr_depth.get();
            const ALMetalRenderPipeline* flat_water_pipeline =
                (water.underwater
                    ? mImpl->underwater_flat_water_pipelines
                    : mImpl->above_water_flat_water_pipelines)
                    [scene.hdr ? 1 : 0].get();
            const ALMetalRenderPipeline* wave_water_pipeline =
                water.waves
                    ? (water.underwater
                        ? mImpl->underwater_wave_water_pipelines
                        : mImpl->above_water_wave_water_pipelines)
                        [scene.hdr ? 1 : 0].get()
                    : nullptr;
            encoded =
                encoder->setViewport(viewport, error) &&
                encoder->setScissor(scissor, error) &&
                encoder->setFrontFacingWinding(
                    ALMetalWinding::clockwise, error) &&
                encoder->setCullMode(ALMetalCullMode::none, error) &&
                encoder->setDepthBias(0.f, 0.f, 0.f, error) &&
                encoder->setPipeline(*flat_water_pipeline, error) &&
                encoder->setVertexTexture(0, field.cell_map, error) &&
                encoder->setFragmentTextureLinear(
                    1, water.normal_maps[0], error) &&
                encoder->setFragmentTextureLinear(
                    2, water.normal_maps[1], error) &&
                encoder->setFragmentTexture(3, refraction, error) &&
                encoder->setFragmentTexture(
                    4, refraction_depth, error) &&
                encoder->setFragmentSampler(
                    0, mImpl->diffuse_sampler.get(), error) &&
                encoder->setFragmentSampler(
                    1, mImpl->linear_clamp_sampler.get(), error) &&
                encoder->setVertexBytes(
                    1, &water_constants,
                    sizeof(water_constants), error) &&
                encoder->setFragmentBytes(
                    0, &field_constants,
                    sizeof(field_constants), error);
            if (encoded && water.waves)
            {
                encoded =
                    encoder->setVertexTexture(
                        1, water.height_maps[0], error) &&
                    encoder->setVertexTexture(
                        2, water.height_maps[1], error) &&
                    encoder->setVertexSampler(
                        0, mImpl->diffuse_sampler.get(), error);
            }

            if (encoded && !water.underwater)
            {
                const ALMetalTexture* directional_depth =
                    water.uses_shadows
                        ? scene.alpha_directional_shadows->depth_texture
                        : mImpl->unshadowed_depth.get();
                const ALMetalWorldDirectionalShadowConstants&
                    directional_constants = water.uses_shadows
                        ? scene.alpha_directional_shadows->constants
                        : mImpl->unshadowed_constants;
                const ALMetalTexture* hero_texture = scene.hero_probe
                    ? scene.hero_probe->radiance_texture
                    : mImpl->inactive_hero_probe.get();
                const ALMetalWorldHeroProbeConstants& hero_constants =
                    scene.hero_probe
                        ? scene.hero_probe->constants
                        : mImpl->inactive_hero_constants;
                const ALMetalTexture* ssr_color =
                    scene.screen_space_reflections
                        ? scene.screen_space_reflections->scene_color
                        : mImpl->inactive_ssr_color.get();
                const ALMetalTexture* ssr_depth =
                    scene.screen_space_reflections
                        ? scene.screen_space_reflections->scene_depth
                        : mImpl->inactive_ssr_depth.get();
                const ALMetalWorldScreenSpaceReflectionConstants&
                    ssr_constants = scene.screen_space_reflections
                        ? scene.screen_space_reflections->constants
                        : mImpl->inactive_ssr_constants;
                const ALMetalTexture* probe_radiance =
                    scene.reflection_probes
                        ? scene.reflection_probes->radiance_texture
                        : mImpl->inactive_probe_radiance.get();
                const ALMetalTexture* legacy_environment_texture =
                    scene.legacy_environment
                        ? scene.legacy_environment
                        : mImpl->inactive_hero_probe.get();
                encoded =
                    encoder->setFragmentBytes(
                        2, scene.alpha_environment,
                        sizeof(*scene.alpha_environment), error) &&
                    encoder->setFragmentBuffer(
                        3, *probe_constants_buffer, 0, error) &&
                    encoder->setFragmentBytes(
                        4, scene.alpha_lighting,
                        sizeof(*scene.alpha_lighting), error) &&
                    encoder->setFragmentTexture(
                        33, directional_depth, error) &&
                    encoder->setFragmentBytes(
                        5, &directional_constants,
                        sizeof(directional_constants), error) &&
                    encoder->setFragmentTexture(
                        34, hero_texture, error) &&
                    encoder->setFragmentBytes(
                        6, &hero_constants,
                        sizeof(hero_constants), error) &&
                    encoder->setFragmentTexture(
                        35, ssr_color, error) &&
                    encoder->setFragmentTexture(
                        36, ssr_depth, error) &&
                    encoder->setFragmentBytes(
                        7, &ssr_constants,
                        sizeof(ssr_constants), error) &&
                    encoder->setFragmentTexture(
                        32, probe_radiance, error) &&
                    encoder->setFragmentTexture(
                        37, legacy_environment_texture, error) &&
                    encoder->setFragmentSampler(
                        2, mImpl->probe_sampler.get(), error) &&
                    encoder->setFragmentSampler(
                        3, mImpl->point_clamp_sampler.get(), error);
            }

            const auto draw_water_field = [&]()
            {
                bool drew = encoder->setPipeline(
                    *flat_water_pipeline, error);
                for (std::uint32_t draw = 0;
                     drew && draw < field.draw_count; ++draw)
                {
                    const ALMetalWorldWaterFieldDraw& command =
                        field.draws[draw];
                    field_constants.config[2] = command.part;
                    drew =
                        encoder->setVertexBytes(
                            0, &field_constants,
                            sizeof(field_constants), error) &&
                        encoder->draw(
                            command.triangle_strip
                                ? ALMetalPrimitiveType::triangle_strip
                                : ALMetalPrimitiveType::triangle,
                            0, command.vertex_count,
                            command.instance_count, error);
                }
                return drew;
            };

            const auto draw_water_patches = [&]()
            {
                return !water.waves ||
                    (wave_water_pipeline && water_factor_buffer &&
                     encoder->setPipeline(
                         *wave_water_pipeline, error) &&
                     encoder->setVertexBytes(
                         0, &field_constants,
                         sizeof(field_constants), error) &&
                     encoder->setTessellationFactorBuffer(
                         *water_factor_buffer, 0, 0, error) &&
                     encoder->drawPatches(
                         4, 0, water.patch_count, 1, 0, error));
            };

            const ALMetalDepthStencilState* surface_depth = scene.reverse_z
                ? mImpl->reverse_underwater_surface_depth.get()
                : mImpl->forward_underwater_surface_depth.get();
            encoded = encoded &&
                encoder->setDepthStencilState(surface_depth, error) &&
                encoder->setStencilReferenceValue(2u, error) &&
                encoder->setFragmentBytes(
                    1, &water_constants,
                    sizeof(water_constants), error) &&
                draw_water_patches() &&
                draw_water_field();

            if (encoded && water.has_excluded_surface)
            {
                water_constants.parameters[2] = 0.f;
                const ALMetalDepthStencilState* excluded_depth =
                    scene.reverse_z
                        ? mImpl->reverse_underwater_excluded_depth.get()
                        : mImpl->forward_underwater_excluded_depth.get();
                encoded =
                    encoder->setDepthStencilState(
                        excluded_depth, error) &&
                    encoder->setStencilReferenceValue(1u, error) &&
                    encoder->setFragmentBytes(
                        1, &water_constants,
                        sizeof(water_constants), error) &&
                    draw_water_patches() &&
                    draw_water_field();
            }
            continue;
        }
        if (batch.pre_water_alpha || batch.post_water_alpha ||
            batch.avatar_alpha)
        {
            encoded = encoded && seed_cutout_coverage();
            // Water, haze and group-local glow reuse fixed texture/sampler and
            // constant slots. Re-establish the complete forward pack at every
            // sorted alpha boundary so a material batch following any of those
            // owners cannot inherit their incompatible bindings.
            encoded = encoded && bind_forward_resources(*encoder);
            WorldShinyConstants world{};
            std::memcpy(world.modelview_projection,
                        batch.modelview_projection,
                        sizeof(world.modelview_projection));
            std::memcpy(world.modelview, batch.modelview,
                        sizeof(world.modelview));
            std::memcpy(world.normal_matrix, batch.normal_matrix,
                        sizeof(world.normal_matrix));
            std::memcpy(world.rigged_normal_matrix,
                        batch.rigged_normal_matrix,
                        sizeof(world.rigged_normal_matrix));
            std::memcpy(world.texture_matrix, batch.texture_matrix,
                        sizeof(world.texture_matrix));
            world.config[0] = scene.reverse_z ? 1u : 0u;
            world.config[1] = batch.texture_count;
            std::memcpy(world.avatar_wind, batch.avatar_wind,
                        sizeof(world.avatar_wind));
            std::memcpy(world.avatar_sin_wave, batch.avatar_sin_wave,
                        sizeof(world.avatar_sin_wave));
            std::memcpy(world.avatar_gravity, batch.avatar_gravity,
                        sizeof(world.avatar_gravity));
            std::memcpy(world.avatar_cloth_depth,
                        batch.avatar_cloth_depth,
                        sizeof(world.avatar_cloth_depth));
            world.config[2] = scene.mirror_clip ? 1u : 0u;
            std::memcpy(world.mirror_clip_plane,
                        scene.mirror_clip_plane,
                        sizeof(world.mirror_clip_plane));

            WorldSimpleConstants pbr_world{};
            std::memcpy(pbr_world.modelview_projection,
                        batch.modelview_projection,
                        sizeof(pbr_world.modelview_projection));
            std::memcpy(pbr_world.normal_matrix,
                        batch.normal_matrix,
                        sizeof(pbr_world.normal_matrix));
            std::memcpy(pbr_world.rigged_normal_matrix,
                        batch.rigged_normal_matrix,
                        sizeof(pbr_world.rigged_normal_matrix));
            std::memcpy(pbr_world.texture_matrix,
                        batch.texture_matrix,
                        sizeof(pbr_world.texture_matrix));
            pbr_world.depth_convention[0] =
                scene.reverse_z ? 1u : 0u;
            pbr_world.depth_convention[1] = batch.texture_count;
            pbr_world.depth_convention[2] =
                scene.mirror_clip ? 1u : 0u;
            std::memcpy(pbr_world.mirror_clip_plane,
                        scene.mirror_clip_plane,
                        sizeof(pbr_world.mirror_clip_plane));

            ALMetalWorldAlphaEnvironment alpha_environment =
                *scene.alpha_environment;
            alpha_environment.environment_config[0] =
                legacy_environment ? 1u : 0u;
            if (batch.post_water_alpha)
            {
                // The same shaders implement both alpha pools. The live
                // water sign selects the opposite side for the pre-water
                // sweep, so invert it for the post-water camera-side sweep.
                alpha_environment.water_fog[2] *= -1.f;
            }
            alpha_environment.water_fog[3] =
                batch.pre_water_minimum_alpha;

            const bool standard_blend =
                batch.pre_water_source_rgb ==
                    ALMetalBlendFactor::source_alpha &&
                batch.pre_water_destination_rgb ==
                    ALMetalBlendFactor::one_minus_source_alpha;
            const Impl::PreWaterAlphaFamily alpha_family =
                batch.pre_water_pbr
                    ? Impl::PreWaterAlphaFamily::pbr
                    : batch.pre_water_material
                    ? Impl::PreWaterAlphaFamily::material
                    : batch.pre_water_simple
                    ? Impl::PreWaterAlphaFamily::simple
                    : Impl::PreWaterAlphaFamily::fullbright;
            ALMetalRenderPipeline* alpha_pipeline = nullptr;
            if (batch.avatar_alpha)
            {
                const std::size_t avatar_variant = batch.avatar_rigid
                    ? 0 : (batch.avatar_cloth ? 2 : 1);
                alpha_pipeline = mImpl->avatar_alpha_pipelines
                    [scene.hdr ? 1 : 0]
                    [batch.pre_water_alpha_writes_coverage ? 1 : 0]
                    [batch.pre_water_shadows ? 1 : 0]
                    [avatar_variant]
                    [batch.avatar_color_write ? 1 : 0].get();
            }
            else if (batch.post_water_depth_only)
            {
                const Impl::PostWaterDepthPipelines& depth_pipelines =
                    mImpl->post_water_depth_pipelines[scene.hdr ? 1 : 0];
                alpha_pipeline = batch.pre_water_pbr
                    ? depth_pipelines.pbr.get()
                    : batch.pre_water_material
                    ? depth_pipelines.material.get()
                    : depth_pipelines.fullbright.get();
            }
            else if (!standard_blend)
            {
                alpha_pipeline = mImpl->customPreWaterAlphaPipeline(
                    alpha_family, scene.hdr, rigged,
                    batch.pre_water_simple &&
                        batch.pre_water_shadows,
                    batch.pre_water_alpha_writes_coverage,
                    batch.pre_water_source_rgb,
                    batch.pre_water_destination_rgb, error);
                if (!alpha_pipeline)
                {
                    return fail_command();
                }
            }
            else
            {
                alpha_pipeline = batch.pre_water_pbr
                    ? mImpl->pre_water_pbr_pipelines
                          [scene.hdr ? 1 : 0]
                          [batch.pre_water_alpha_writes_coverage ? 1 : 0]
                          [rigged ? 1 : 0].get()
                    : batch.pre_water_material
                    ? mImpl->pre_water_material_pipelines
                          [scene.hdr ? 1 : 0]
                          [batch.pre_water_alpha_writes_coverage ? 1 : 0]
                          [rigged ? 1 : 0].get()
                    : batch.pre_water_simple
                    ? mImpl->pre_water_simple_pipelines
                          [scene.hdr ? 1 : 0]
                          [batch.pre_water_alpha_writes_coverage ? 1 : 0]
                          [batch.pre_water_shadows ? 1 : 0]
                          [rigged ? 1 : 0].get()
                    : mImpl->pre_water_fullbright_pipelines
                          [scene.hdr ? 1 : 0]
                          [batch.pre_water_alpha_writes_coverage ? 1 : 0]
                          [rigged ? 1 : 0].get();
            }

            const bool writes_depth = batch.avatar_alpha
                ? batch.avatar_depth_write
                : batch.pre_water_alpha || rigged ||
                      batch.post_water_depth_only;
            const ALMetalDepthStencilState* depth = scene.reverse_z
                ? (writes_depth ? mImpl->reverse_depth.get()
                                : mImpl->reverse_readonly_depth.get())
                : (writes_depth ? mImpl->forward_depth.get()
                                : mImpl->forward_readonly_depth.get());
            encoded =
                encoder->setDepthStencilState(depth, error) &&
                encoder->setDepthBias(0.f, 0.f, 0.f, error) &&
                encoder->setCullMode(
                    batch.avatar_alpha
                        ? batch.avatar_cull_mode
                        : batch.pre_water_particle ||
                            (batch.pre_water_pbr && batch.double_sided)
                        ? ALMetalCullMode::none : ALMetalCullMode::back,
                    error) &&
                encoder->setPipeline(*alpha_pipeline, error) &&
                encoder->setFragmentSampler(
                    0, batch.pre_water_particle
                        ? mImpl->probe_sampler.get()
                        : mImpl->diffuse_sampler.get(),
                    error) &&
                encoder->setVertexBuffer(
                    0, *batch.vertices, batch.position_offset, error) &&
                encoder->setVertexBuffer(
                    1, *batch.vertices, batch.normal_offset, error) &&
                encoder->setVertexBuffer(
                    2, *batch.vertices, batch.texcoord_offset, error) &&
                encoder->setVertexBuffer(
                    3, *batch.vertices, batch.color_offset, error) &&
                encoder->setVertexBytes(
                    4,
                    batch.pre_water_pbr
                        ? static_cast<const void*>(&pbr_world)
                        : static_cast<const void*>(&world),
                    batch.pre_water_pbr
                        ? sizeof(pbr_world) : sizeof(world),
                    error);
            if (encoded && batch.pre_water_material)
            {
                const bool normal_map = materialHasNormalMap(batch);
                const bool specular_map = materialHasSpecularMap(batch);
                WorldMaterialConstants material{};
                for (std::size_t slot = 0;
                     slot < batch.material_slot_count; ++slot)
                {
                    const ALMetalWorldMaterialSlot& source =
                        batch.material_slots[slot];
                    std::memcpy(
                        material.slots[slot].specular_color,
                        source.specular_color,
                        sizeof(material.slots[slot].specular_color));
                    material.slots[slot].properties[0] =
                        source.env_intensity;
                    material.slots[slot].properties[1] =
                        source.alpha_cutoff;
                    material.slots[slot].properties[2] =
                        source.fullbright;
                }
                material.config[0] = batch.material_slot_count;
                material.config[1] = batch.material_alpha_mode;
                material.config[2] = normal_map ? 1u : 0u;
                material.config[3] = specular_map ? 1u : 0u;
                const ALMetalTexture* directional_depth =
                    batch.pre_water_shadows
                        ? scene.alpha_directional_shadows->depth_texture
                        : mImpl->unshadowed_depth.get();
                const ALMetalWorldDirectionalShadowConstants&
                    directional_constants = batch.pre_water_shadows
                        ? scene.alpha_directional_shadows->constants
                        : mImpl->unshadowed_constants;
                encoded =
                    encoder->setVertexBuffer(
                        8, *batch.vertices,
                        normal_map
                            ? batch.tangent_offset : batch.normal_offset,
                        error) &&
                    encoder->setVertexBuffer(
                        9, *batch.vertices,
                        normal_map
                            ? batch.texcoord1_offset
                            : batch.texcoord_offset,
                        error) &&
                    encoder->setVertexBuffer(
                        10, *batch.vertices,
                        specular_map
                            ? batch.texcoord2_offset
                            : batch.texcoord_offset,
                        error) &&
                    encoder->setFragmentBytes(
                        0, &material, sizeof(material), error) &&
                    encoder->setFragmentBytes(
                        2, scene.alpha_lighting,
                        sizeof(*scene.alpha_lighting), error) &&
                    encoder->setFragmentTexture(
                        33, directional_depth, error) &&
                    encoder->setFragmentBytes(
                        3, &directional_constants,
                        sizeof(directional_constants), error) &&
                    encoder->setFragmentBytes(
                        6, &alpha_environment,
                        sizeof(alpha_environment), error);
                for (std::size_t slot = 0;
                     encoded && slot <
                         ALMetalWorldSimpleBatch::max_material_slots;
                     ++slot)
                {
                    const std::size_t source = std::min<std::size_t>(
                        slot, batch.material_slot_count - 1);
                    const ALMetalTexture* normal_texture = normal_map
                        ? batch.material_slots[source].normal_texture
                        : batch.diffuse_textures[0];
                    const ALMetalTexture* specular_texture = specular_map
                        ? batch.material_slots[source].specular_texture
                        : batch.diffuse_textures[0];
                    encoded =
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(slot),
                            batch.diffuse_textures[source], error) &&
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(8 + slot),
                            normal_texture, error) &&
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(16 + slot),
                            specular_texture, error);
                }
            }
            else if (encoded && batch.pre_water_pbr)
            {
                WorldPBRConstants pbr{};
                for (std::size_t slot = 0;
                     slot < batch.pbr_slot_count; ++slot)
                {
                    const ALMetalWorldPBRSlot& source =
                        batch.pbr_slots[slot];
                    WorldPBRSlotConstants& destination =
                        pbr.slots[slot];
                    std::memcpy(destination.texture_transform,
                                source.texture_transform,
                                sizeof(destination.texture_transform));
                    std::memcpy(destination.emissive_color,
                                source.emissive_color,
                                sizeof(destination.emissive_color));
                    destination.properties[0] = source.roughness_factor;
                    destination.properties[1] = source.metallic_factor;
                    destination.properties[2] = source.alpha_cutoff;
                    destination.properties[3] =
                        source.base_color_factor[3];
                }
                pbr.config[0] = batch.pbr_slot_count;
                std::memcpy(pbr.modelview, batch.modelview,
                            sizeof(pbr.modelview));
                const ALMetalTexture* directional_depth =
                    batch.pre_water_shadows
                        ? scene.alpha_directional_shadows->depth_texture
                        : mImpl->unshadowed_depth.get();
                const ALMetalWorldDirectionalShadowConstants&
                    directional_constants = batch.pre_water_shadows
                        ? scene.alpha_directional_shadows->constants
                        : mImpl->unshadowed_constants;
                encoded =
                    encoder->setVertexBuffer(
                        8, *batch.vertices, batch.tangent_offset, error) &&
                    encoder->setVertexBytes(
                        11, &pbr, sizeof(pbr), error) &&
                    encoder->setFragmentBytes(
                        0, &pbr, sizeof(pbr), error) &&
                    encoder->setFragmentBytes(
                        2, scene.alpha_lighting,
                        sizeof(*scene.alpha_lighting), error) &&
                    encoder->setFragmentTexture(
                        33, directional_depth, error) &&
                    encoder->setFragmentBytes(
                        3, &directional_constants,
                        sizeof(directional_constants), error) &&
                    encoder->setFragmentBytes(
                        6, &alpha_environment,
                        sizeof(alpha_environment), error);
                for (std::size_t slot = 0;
                     encoded && slot <
                         ALMetalWorldSimpleBatch::max_material_slots;
                     ++slot)
                {
                    const std::size_t source = std::min<std::size_t>(
                        slot, batch.pbr_slot_count - 1);
                    encoded =
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(slot),
                            batch.diffuse_textures[source], error) &&
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(8 + slot),
                            batch.pbr_slots[source].normal_texture,
                            error) &&
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(16 + slot),
                            batch.pbr_slots[source].orm_texture,
                            error) &&
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(24 + slot),
                            batch.pbr_slots[source].emissive_texture,
                            error);
                }
            }
            else if (encoded)
            {
                encoded = encoder->setFragmentBytes(
                    0, &alpha_environment,
                    sizeof(alpha_environment), error);
                if (encoded && batch.pre_water_simple)
                {
                    encoded = encoder->setFragmentBytes(
                        2, scene.alpha_lighting,
                        sizeof(*scene.alpha_lighting), error);
                }
                if (encoded && batch.pre_water_shadows)
                {
                    encoded =
                        encoder->setFragmentTexture(
                            33,
                            scene.alpha_directional_shadows->depth_texture,
                            error) &&
                        encoder->setFragmentBytes(
                            3,
                            &scene.alpha_directional_shadows->constants,
                            sizeof(scene.alpha_directional_shadows
                                       ->constants),
                            error);
                }
            }
            if (encoded && rigged)
            {
                encoded =
                    encoder->setVertexBuffer(
                        5, *palette_buffers[index], 0, error) &&
                    encoder->setVertexBuffer(
                        6, *batch.vertices, batch.weight_offset, error) &&
                    encoder->setVertexBytes(
                        7, &batch.skin_joint_count,
                        sizeof(batch.skin_joint_count), error);
            }
            if (encoded && batch.avatar_cloth)
            {
                encoded = encoder->setVertexBuffer(
                    9, *batch.vertices, batch.cloth_offset, error);
            }
            for (std::size_t texture = 0;
                 encoded && !batch.pre_water_material &&
                 !batch.pre_water_pbr &&
                 texture < batch.texture_count; ++texture)
            {
                encoded = encoder->setFragmentTexture(
                    static_cast<std::uint32_t>(texture),
                    batch.diffuse_textures[texture], error);
            }
            encoded = encoded && encoder->drawIndexed(
                ALMetalPrimitiveType::triangle, batch.index_count,
                batch.index_type, *batch.indices,
                batch.index_offset, 1, error);
            continue;
        }
        if (batch.water_exclusion)
        {
            WorldWaterExclusionConstants constants{};
            std::memcpy(constants.modelview_projection,
                        batch.modelview_projection,
                        sizeof(constants.modelview_projection));
            std::memcpy(constants.modelview, batch.modelview,
                        sizeof(constants.modelview));
            std::memcpy(constants.eye_to_agent,
                        scene.water_exclusion_surface->eye_to_agent,
                        sizeof(constants.eye_to_agent));
            std::memcpy(constants.cell_square,
                        scene.water_exclusion_surface->cell_square,
                        sizeof(constants.cell_square));
            std::memcpy(constants.camera_default_height,
                        scene.water_exclusion_surface
                            ->camera_default_height,
                        sizeof(constants.camera_default_height));
            constants.config[0] = scene.reverse_z ? 1u : 0u;
            const ALMetalDepthStencilState* water_depth = scene.reverse_z
                ? mImpl->reverse_water_exclusion_depth.get()
                : mImpl->forward_water_exclusion_depth.get();
            encoded =
                encoder->setDepthStencilState(water_depth, error) &&
                encoder->setDepthBias(0.f, 0.f, 0.f, error) &&
                encoder->setStencilReferenceValue(1u, error) &&
                encoder->setCullMode(ALMetalCullMode::back, error) &&
                encoder->setPipeline(
                    *mImpl->water_exclusion_pipelines
                        [scene.hdr ? 1 : 0],
                    error) &&
                encoder->setVertexBuffer(
                    0, *batch.vertices, batch.position_offset, error) &&
                encoder->setVertexBytes(
                    1, &constants, sizeof(constants), error) &&
                encoder->setFragmentBytes(
                    0, &constants, sizeof(constants), error) &&
                encoder->setFragmentTexture(
                    0, scene.water_exclusion_surface->cell_map, error) &&
                encoder->drawIndexed(
                    ALMetalPrimitiveType::triangle, batch.index_count,
                    batch.index_type, *batch.indices,
                    batch.index_offset, 1, error);
            continue;
        }
        if (batch.legacy_glow || batch.pbr_glow)
        {
            WorldSimpleConstants world{};
            std::memcpy(world.modelview_projection,
                        batch.modelview_projection,
                        sizeof(world.modelview_projection));
            std::memcpy(world.texture_matrix, batch.texture_matrix,
                        sizeof(world.texture_matrix));
            world.depth_convention[0] = scene.reverse_z ? 1u : 0u;
            world.depth_convention[1] = batch.texture_count;

            const ALMetalDepthStencilState* depth = scene.reverse_z
                ? mImpl->reverse_readonly_depth.get()
                : mImpl->forward_readonly_depth.get();
            const ALMetalRenderPipeline* glow_pipeline = batch.pbr_glow
                ? (batch.post_water_glow
                       ? mImpl->post_water_pbr_glow_pipelines
                             [scene.hdr ? 1 : 0][rigged ? 1 : 0].get()
                       : mImpl->pbr_glow_pipelines[scene.hdr ? 1 : 0]
                                                  [rigged ? 1 : 0].get())
                : mImpl->legacy_glow_pipelines[scene.hdr ? 1 : 0]
                                                    [rigged ? 1 : 0].get();
            encoded =
                encoder->setDepthStencilState(depth, error) &&
                encoder->setDepthBias(
                    batch.legacy_glow && !batch.post_water_glow
                        ? (scene.reverse_z ? 1.f : -1.f) : 0.f,
                    batch.legacy_glow && !batch.post_water_glow
                        ? (scene.reverse_z ? 1.f : -1.f) : 0.f,
                    0.f, error) &&
                encoder->setCullMode(
                    batch.pre_water_particle ||
                            (batch.pbr_glow && batch.double_sided)
                        ? ALMetalCullMode::none : ALMetalCullMode::back,
                    error) &&
                encoder->setPipeline(*glow_pipeline, error) &&
                encoder->setVertexBuffer(
                    0, *batch.vertices, batch.position_offset, error) &&
                encoder->setVertexBuffer(
                    2, *batch.vertices, batch.texcoord_offset, error) &&
                encoder->setVertexBuffer(
                    3, *batch.vertices, batch.emissive_offset, error) &&
                encoder->setVertexBytes(
                    4, &world, sizeof(world), error);
            if (encoded && rigged)
            {
                encoded =
                    encoder->setVertexBuffer(
                        5, *palette_buffers[index], 0, error) &&
                    encoder->setVertexBuffer(
                        6, *batch.vertices, batch.weight_offset, error) &&
                    encoder->setVertexBytes(
                        7, &batch.skin_joint_count,
                        sizeof(batch.skin_joint_count), error);
            }
            if (encoded && batch.pbr_glow)
            {
                WorldPBRConstants pbr{};
                for (std::size_t slot = 0;
                     slot < batch.pbr_slot_count; ++slot)
                {
                    const ALMetalWorldPBRSlot& source =
                        batch.pbr_slots[slot];
                    WorldPBRSlotConstants& destination = pbr.slots[slot];
                    std::memcpy(destination.texture_transform,
                                source.texture_transform,
                                sizeof(destination.texture_transform));
                    std::memcpy(destination.emissive_color,
                                source.emissive_color,
                                sizeof(destination.emissive_color));
                    destination.properties[0] = source.roughness_factor;
                    destination.properties[1] = source.metallic_factor;
                    destination.properties[2] = source.alpha_cutoff;
                    destination.properties[3] =
                        source.base_color_factor[3];
                }
                pbr.config[0] = batch.pbr_slot_count;
                pbr.config[1] = batch.post_water_glow ? 1u : 0u;
                encoded =
                    encoder->setVertexBytes(
                        11, &pbr, sizeof(pbr), error) &&
                    encoder->setFragmentBytes(
                        0, &pbr, sizeof(pbr), error);
                for (std::size_t slot = 0;
                     encoded && slot <
                         ALMetalWorldSimpleBatch::max_material_slots; ++slot)
                {
                    const std::size_t source = std::min<std::size_t>(
                        slot, batch.pbr_slot_count - 1);
                    encoded =
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(slot),
                            batch.diffuse_textures[source], error) &&
                        encoder->setFragmentTexture(
                            static_cast<std::uint32_t>(8 + slot),
                            batch.pbr_slots[source].emissive_texture, error);
                }
            }
            else
            {
                for (std::size_t texture = 0;
                     encoded && texture <
                         ALMetalWorldSimpleBatch::max_diffuse_textures;
                     ++texture)
                {
                    const std::size_t source = std::min<std::size_t>(
                        texture, batch.texture_count - 1);
                    encoded = encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(texture),
                        batch.diffuse_textures[source], error);
                }
            }
            encoded = encoded && encoder->drawIndexed(
                ALMetalPrimitiveType::triangle, batch.index_count,
                batch.index_type, *batch.indices,
                batch.index_offset, 1, error);
            continue;
        }
        const ALMetalRenderPipeline* pipeline = nullptr;
        if (batch.impostor_emboss)
        {
            pipeline = mImpl->opaque_forward_emboss_pipelines
                [scene.hdr ? 1 : 0][rigged ? 1 : 0].get();
        }
        else if (batch.impostor_shiny)
        {
            pipeline = mImpl->opaque_forward_shiny_pipelines
                [scene.hdr ? 1 : 0][rigged ? 1 : 0].get();
        }
        else
        {
            pipeline = mImpl->opaque_forward_fullbright_pipelines
                [scene.hdr ? 1 : 0][batch.alpha_mask ? 1 : 0]
                [rigged ? 1 : 0].get();
        }

        WorldSimpleConstants world{};
        std::memcpy(world.modelview_projection,
                    batch.modelview_projection,
                    sizeof(world.modelview_projection));
        std::memcpy(world.normal_matrix, batch.normal_matrix,
                    sizeof(world.normal_matrix));
        std::memcpy(world.rigged_normal_matrix,
                    batch.rigged_normal_matrix,
                    sizeof(world.rigged_normal_matrix));
        std::memcpy(world.texture_matrix, batch.texture_matrix,
                    sizeof(world.texture_matrix));
        world.material[0] = batch.alpha_cutoff;
        world.material[3] = batch.alpha_mask ? 1.f : 0.f;
        world.depth_convention[0] = scene.reverse_z ? 1u : 0u;
        world.depth_convention[1] = batch.texture_count;
        world.depth_convention[2] = scene.mirror_clip ? 1u : 0u;
        std::memcpy(world.modelview, batch.modelview,
                    sizeof(world.modelview));
        std::memcpy(world.mirror_clip_plane,
                    scene.mirror_clip_plane,
                    sizeof(world.mirror_clip_plane));

        WorldShinyConstants shiny_world{};
        std::memcpy(shiny_world.modelview_projection,
                    batch.modelview_projection,
                    sizeof(shiny_world.modelview_projection));
        std::memcpy(shiny_world.modelview, batch.modelview,
                    sizeof(shiny_world.modelview));
        std::memcpy(shiny_world.normal_matrix, batch.normal_matrix,
                    sizeof(shiny_world.normal_matrix));
        std::memcpy(shiny_world.rigged_normal_matrix,
                    batch.rigged_normal_matrix,
                    sizeof(shiny_world.rigged_normal_matrix));
        std::memcpy(shiny_world.texture_matrix, batch.texture_matrix,
                    sizeof(shiny_world.texture_matrix));
        shiny_world.config[0] = scene.reverse_z ? 1u : 0u;
        shiny_world.config[1] = batch.texture_count;
        shiny_world.config[2] = scene.mirror_clip ? 1u : 0u;
        std::memcpy(shiny_world.mirror_clip_plane,
                    scene.mirror_clip_plane,
                    sizeof(shiny_world.mirror_clip_plane));

        const ALMetalDepthStencilState* depth = batch.impostor_emboss
            ? (scene.reverse_z ? mImpl->reverse_readonly_depth.get()
                               : mImpl->forward_readonly_depth.get())
            : (scene.reverse_z ? mImpl->reverse_depth.get()
                               : mImpl->forward_depth.get());
        encoded =
            encoder->setDepthStencilState(depth, error) &&
            encoder->setDepthBias(0.f, 0.f, 0.f, error) &&
            encoder->setCullMode(ALMetalCullMode::back, error) &&
            encoder->setPipeline(*pipeline, error) &&
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                2, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBytes(
                4,
                batch.impostor_shiny
                    ? static_cast<const void*>(&shiny_world)
                    : static_cast<const void*>(&world),
                batch.impostor_shiny
                    ? sizeof(shiny_world) : sizeof(world),
                error);
        if (encoded && batch.impostor_emboss)
        {
            encoded = encoder->setVertexBuffer(
                9, *batch.vertices, batch.texcoord1_offset, error);
        }
        else if (encoded && batch.impostor_fullbright)
        {
            encoded = encoder->setVertexBuffer(
                3, *batch.vertices, batch.color_offset, error);
        }
        else if (encoded)
        {
            encoded =
                encoder->setVertexBuffer(
                    1, *batch.vertices, batch.normal_offset, error) &&
                encoder->setVertexBuffer(
                    3, *batch.vertices, batch.color_offset, error);
        }
        if (encoded && rigged)
        {
            encoded =
                encoder->setVertexBuffer(
                    5, *palette_buffers[index], 0, error) &&
                encoder->setVertexBuffer(
                    6, *batch.vertices, batch.weight_offset, error) &&
                encoder->setVertexBytes(
                    7, &batch.skin_joint_count,
                    sizeof(batch.skin_joint_count), error);
        }
        if (encoded && batch.impostor_shiny)
        {
            ALMetalWorldAlphaEnvironment shiny_environment =
                scene.alpha_environment
                    ? *scene.alpha_environment
                    : ALMetalWorldAlphaEnvironment{};
            shiny_environment.environment_config[0] =
                legacy_environment ? 1u : 0u;
            encoded = encoder->setFragmentBytes(
                0, &shiny_environment,
                sizeof(shiny_environment), error);
        }
        for (std::size_t texture = 0;
             encoded && texture < batch.texture_count; ++texture)
        {
            encoded = encoder->setFragmentTexture(
                static_cast<std::uint32_t>(texture),
                batch.diffuse_textures[texture], error);
        }
        encoded = encoded && encoder->drawIndexed(
            ALMetalPrimitiveType::triangle, batch.index_count,
            batch.index_type, *batch.indices, batch.index_offset, 1, error);
    }

    encoded = encoded && seed_cutout_coverage();

    const ALMetalDepthStencilState* highlight_depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    for (std::size_t index = 0;
         encoded && index < scene.highlight_batch_count; ++index)
    {
        const ALMetalWorldHighlightBatch& batch =
            scene.highlight_batches[index];
        WorldHighlightConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.texture_matrix, batch.texture_matrix,
                    sizeof(constants.texture_matrix));
        std::memcpy(constants.color, batch.color,
                    sizeof(constants.color));
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        encoded =
            encoder->setDepthStencilState(highlight_depth, error) &&
            encoder->setDepthBias(0.f, 0.f, 0.f, error) &&
            encoder->setCullMode(ALMetalCullMode::back, error) &&
            encoder->setPipeline(
                *mImpl->highlight_pipelines[scene.hdr ? 1 : 0], error) &&
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                2, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBytes(
                4, &constants, sizeof(constants), error) &&
            encoder->setFragmentTexture(0, batch.texture, error) &&
            encoder->setFragmentSampler(
                0, mImpl->diffuse_sampler.get(), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle, batch.index_count,
                batch.index_type, *batch.indices,
                batch.index_offset, 1, error);
    }

    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended, error))
    {
        return nullptr;
    }
    if (generate_pre_water_brdf_lut)
    {
        if (mImpl->command_batch)
        {
            mImpl->command_batch_generates_brdf_lut = true;
        }
        else
        {
            mImpl->brdf_lut_ready = true;
        }
    }
    return output;
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderLightMap(
    const ALMetalWorldLightMapScene& scene,
    std::string* error)
{
    const ALMetalWorldLightMapConstants& constants = scene.constants;
    const bool directional_shadows = constants.config[1] != 0;
    const bool ssao = constants.config[2] != 0;
    if (!mImpl || !mImpl->device || !scene.gbuffer ||
        !finiteValues(constants.inverse_projection, 16) ||
        !finiteValues(constants.light_direction, 4) ||
        !finiteValues(constants.ssao, 4) ||
        !finiteValues(&constants.blur_taps[0][0], 16) ||
        !finiteValues(constants.blur, 4) ||
        !finiteValues(constants.screen, 4) ||
        constants.config[0] > 1 || constants.config[1] > 1 ||
        constants.config[2] > 1 || constants.config[3] > 1 ||
        (scene.directional_shadows &&
         !validDirectionalShadows(scene.directional_shadows)) ||
        (ssao && (!sampleableTexture(scene.noise_texture) ||
                  constants.ssao[0] < 0.f || constants.ssao[1] < 0.f ||
                  constants.ssao[2] < 0.f || constants.ssao[3] <= 0.f ||
                  constants.blur_taps[0][1] <= 0.f ||
                  constants.blur[0] < 0.f || constants.blur[1] < 0.f)))
    {
        setError(error, "Metal world light-map scene is incomplete");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input =
        scene.gbuffer->descriptor();
    const ALMetalTexture* normal = scene.gbuffer->colorTexture(2);
    const ALMetalTexture* depth = scene.gbuffer->depthStencilTexture();
    const bool normal_format =
        input.colors[2].format == ALMetalPixelFormat::rgb10a2_unorm ||
        input.colors[2].format == ALMetalPixelFormat::rgba16_unorm;
    const ALMetalTextureDescriptor* noise = ssao
        ? &scene.noise_texture->descriptor() : nullptr;
    const bool valid_noise = !noise ||
        (noise->format == ALMetalPixelFormat::rg32_float &&
         noise->width == 128 && noise->height == 128 &&
         noise->sample_count == 1 && noise->mipmap_level_count == 1);
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        input.color_attachment_count != 4 || !normal_format ||
        input.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8 ||
        !input.depth_stencil_shader_read ||
        constants.screen[0] != static_cast<float>(input.width) ||
        constants.screen[1] != static_cast<float>(input.height) ||
        !valid_noise || !sampleableTexture(normal) ||
        !sampleableTexture(depth))
    {
        setError(error, "Metal world light-map G-buffer is incompatible");
        return nullptr;
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = input.width;
    output_descriptor.height = input.height;
    output_descriptor.colors[0].format = ALMetalPixelFormat::rg8_unorm;
    output_descriptor.colors[0].label =
        "Alchemy world directional-shadow and SSAO light map";
    output_descriptor.label = "Alchemy native world light-map target";
    auto raw = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!raw)
    {
        return nullptr;
    }

    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world light-map production", owned_command, error);
    if (!command)
    {
        return nullptr;
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {1.0, 1.0, 0.0, 0.0};
    auto pass = raw->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world light-map encoder", error)
        : nullptr;
    if (!encoder)
    {
        mImpl->finishCommandBuffer(owned_command, false, error);
        return nullptr;
    }

    const ALMetalWorldDirectionalShadowConstants& shadow_constants =
        directional_shadows && scene.directional_shadows
            ? scene.directional_shadows->constants
            : mImpl->unshadowed_constants;
    const ALMetalTexture* shadow_texture =
        directional_shadows && scene.directional_shadows
            ? scene.directional_shadows->depth_texture
            : mImpl->unshadowed_depth.get();
    const ALMetalTexture* noise_texture = ssao
        ? scene.noise_texture : mImpl->inactive_ssr_color.get();
    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(input.width),
        static_cast<double>(input.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, input.width, input.height};
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(*mImpl->light_map_pipeline, error) &&
        encoder->setFragmentBytes(
            0, &constants, sizeof(constants), error) &&
        encoder->setFragmentBytes(
            1, &shadow_constants, sizeof(shadow_constants), error) &&
        encoder->setFragmentTexture(0, normal, error) &&
        encoder->setFragmentTexture(1, depth, error) &&
        encoder->setFragmentTexture(2, shadow_texture, error) &&
        encoder->setFragmentTexture(3, noise_texture, error) &&
        encoder->setFragmentSampler(
            0, mImpl->point_clamp_sampler.get(), error) &&
        encoder->setFragmentSampler(
            1, mImpl->point_repeat_sampler.get(), error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
        encoder->end(error);
    if (!encoded)
    {
        mImpl->finishCommandBuffer(owned_command, false, error);
        return nullptr;
    }

    if (!ssao)
    {
        return mImpl->finishCommandBuffer(owned_command, true, error)
            ? std::move(raw) : nullptr;
    }

    auto scratch = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!scratch || !output)
    {
        mImpl->finishCommandBuffer(owned_command, false, error);
        return nullptr;
    }
    const float horizontal[4] = {1.f, 0.f, 0.f, 0.f};
    const float vertical[4] = {0.f, 1.f, 0.f, 0.f};
    ALMetalRenderTarget* destinations[2] = {scratch.get(), output.get()};
    const ALMetalTexture* sources[2] = {
        raw->colorTexture(0), scratch->colorTexture(0),
    };
    const float* deltas[2] = {horizontal, vertical};
    for (std::size_t axis = 0; axis < 2; ++axis)
    {
        auto blur_pass = destinations[axis]->makeRenderPass(
            pass_config, error);
        auto blur_encoder = blur_pass ? ALMetalRenderEncoder::begin(
            *command, *blur_pass,
            axis == 0 ? "Alchemy world horizontal SSAO blur encoder"
                      : "Alchemy world vertical SSAO blur encoder",
            error) : nullptr;
        encoded = blur_encoder &&
            blur_encoder->setViewport(viewport, error) &&
            blur_encoder->setScissor(scissor, error) &&
            blur_encoder->setCullMode(ALMetalCullMode::none, error) &&
            blur_encoder->setPipeline(
                *mImpl->light_map_blur_pipeline, error) &&
            blur_encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            blur_encoder->setFragmentBytes(
                1, deltas[axis], sizeof(horizontal), error) &&
            blur_encoder->setFragmentTexture(0, sources[axis], error) &&
            blur_encoder->setFragmentTexture(1, normal, error) &&
            blur_encoder->setFragmentTexture(2, depth, error) &&
            blur_encoder->setFragmentSampler(
                0, mImpl->point_clamp_sampler.get(), error) &&
            blur_encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
            blur_encoder->end(error);
        if (!encoded)
        {
            mImpl->finishCommandBuffer(owned_command, false, error);
            return nullptr;
        }
    }

    return mImpl->finishCommandBuffer(owned_command, true, error)
        ? std::move(output) : nullptr;
}

bool ALMetalWorldGBufferRenderer::compositeHDRISky(
    ALMetalRenderTarget& target,
    const ALMetalWorldHDRISkyScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || !sampleableTexture(
            scene.environment_texture) ||
        scene.width == 0 || scene.height == 0 ||
        !finiteValues(scene.camera_local, 4) ||
        !finiteValues(&scene.environment_rotation[0][0], 12) ||
        !std::isfinite(scene.exposure_scale) || scene.exposure_scale < 0.f ||
        !std::isfinite(scene.horizontal_fraction) ||
        scene.horizontal_fraction < 0.f ||
        scene.horizontal_fraction > 1.f)
    {
        setError(error, "Metal world HDRI sky scene is incomplete");
        return false;
    }

    const ALMetalTextureDescriptor& environment =
        scene.environment_texture->descriptor();
    if (environment.type != ALMetalTextureType::texture_2d ||
        environment.format != ALMetalPixelFormat::rgba16_float ||
        environment.width == 0 || environment.height == 0 ||
        environment.array_length != 1 ||
        environment.mipmap_level_count == 0 ||
        environment.sample_count != 1)
    {
        setError(error, "Metal world HDRI sky environment is incompatible");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal world HDRI sky target is incompatible");
        return false;
    }

    constexpr std::size_t float4_stride = sizeof(float) * 4;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices || batch.vertex_count == 0 ||
            batch.index_count < 3 ||
            batch.position_offset % float4_stride != 0 ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error, "Metal world HDRI sky batch is invalid");
            return false;
        }
    }

    const double fractional_width =
        static_cast<double>(scene.horizontal_fraction) * scene.width;
    const std::uint32_t scissor_width = static_cast<std::uint32_t>(
        std::clamp(std::ceil(fractional_width - 0.5),
                   0.0, static_cast<double>(scene.width)));
    if (scissor_width == 0)
    {
        return true;
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = pass ? mImpl->acquireCommandBuffer(
        "Alchemy native world HDRI sky", owned_command, error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world HDRI sky encoder", error) : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{
        0, 0, scissor_width, scene.height,
    };
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    const ALMetalRenderPipeline* pipeline =
        mImpl->hdri_sky_pipelines[scene.hdr ? 1 : 0]
                                 [scene.normal_hdr ? 1 : 0].get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setPipeline(*pipeline, error) &&
        encoder->setFragmentTexture(
            0, scene.environment_texture, error) &&
        encoder->setFragmentSampler(0, mImpl->hdri_sampler.get(), error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        WorldHDRISkyConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.camera_exposure, scene.camera_local,
                    sizeof(scene.camera_local));
        constants.camera_exposure[3] = scene.exposure_scale;
        std::memcpy(constants.environment_rotation,
                    scene.environment_rotation,
                    sizeof(constants.environment_rotation));
        constants.screen[0] = static_cast<float>(scene.width);
        constants.screen[1] = static_cast<float>(scene.height);
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBytes(
                1, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle_strip,
                batch.index_count, batch.index_type,
                *batch.indices, batch.index_offset, 1, error);
    }

    const bool ended = encoder->end(error);
    return mImpl->finishCommandBuffer(
        owned_command, encoded && ended, error);
}

bool ALMetalWorldGBufferRenderer::compositeWindLightSky(
    ALMetalRenderTarget& target,
    const ALMetalWorldWindLightSkyScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 ||
        !sampleableTexture(scene.rainbow_texture) ||
        !sampleableTexture(scene.halo_texture) ||
        scene.width == 0 || scene.height == 0 ||
        !finiteValues(scene.constants.camera_local, 4) ||
        !finiteValues(scene.constants.light_density, 4) ||
        !finiteValues(scene.constants.blue_horizon, 4) ||
        !finiteValues(scene.constants.glow, 4) ||
        !finiteValues(scene.constants.haze_extinction, 4) ||
        !finiteValues(scene.constants.blue_weight, 4) ||
        !finiteValues(scene.constants.haze_weight, 4) ||
        !finiteValues(scene.constants.sky_light, 4) ||
        !finiteValues(scene.constants.sky_ambient, 4) ||
        !finiteValues(scene.constants.sky_light_atten, 4) ||
        !finiteValues(scene.constants.sky_ambient_below, 4) ||
        !finiteValues(scene.constants.optics, 4) ||
        scene.constants.light_density[3] < 0.f ||
        scene.constants.glow[3] < 0.f ||
        scene.constants.optics[0] < 0.f ||
        scene.constants.optics[1] < 0.f ||
        scene.constants.optics[2] < 0.f ||
        !std::isfinite(scene.sky_hdr_scale) ||
        scene.sky_hdr_scale < 0.f)
    {
        setError(error, "Metal world WindLight sky scene is incomplete");
        return false;
    }

    const ALMetalTextureDescriptor& rainbow =
        scene.rainbow_texture->descriptor();
    const ALMetalTextureDescriptor& halo = scene.halo_texture->descriptor();
    if (rainbow.type != ALMetalTextureType::texture_2d ||
        halo.type != ALMetalTextureType::texture_2d ||
        rainbow.width == 0 || rainbow.height == 0 ||
        halo.width == 0 || halo.height == 0 ||
        rainbow.sample_count != 1 || halo.sample_count != 1)
    {
        setError(error, "Metal world WindLight sky optics are incompatible");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal world WindLight sky target is incompatible");
        return false;
    }

    constexpr std::size_t float4_stride = sizeof(float) * 4;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices || batch.vertex_count == 0 ||
            batch.index_count < 3 ||
            batch.position_offset % float4_stride != 0 ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error, "Metal world WindLight sky batch is invalid");
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = pass ? mImpl->acquireCommandBuffer(
        "Alchemy native world WindLight sky", owned_command, error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world WindLight sky encoder", error)
                           : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    const ALMetalRenderPipeline* pipeline =
        mImpl->windlight_sky_pipelines[scene.hdr ? 1 : 0]
                                      [scene.normal_hdr ? 1 : 0].get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setPipeline(*pipeline, error) &&
        // GL binds both fetched optics maps with AnisoWrap, not the colour
        // AnisoWrapSRGB variant.  Fetched RGBA assets still have sRGB Metal
        // storage, so sample the compatible raw-storage views to preserve the
        // bytes consumed by skyF.glsl before its explicit pow/additive math.
        encoder->setFragmentTextureLinear(
            0, scene.rainbow_texture, error) &&
        encoder->setFragmentTextureLinear(
            1, scene.halo_texture, error) &&
        encoder->setFragmentSampler(
            0, mImpl->diffuse_sampler.get(), error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        WorldWindLightSkyConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        constants.sky = scene.constants;
        constants.transfer[0] = static_cast<float>(scene.width);
        constants.transfer[1] = static_cast<float>(scene.height);
        constants.transfer[2] = scene.sky_hdr_scale;
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBytes(
                1, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle_strip,
                batch.index_count, batch.index_type,
                *batch.indices, batch.index_offset, 1, error);
    }

    const bool ended = encoder->end(error);
    return mImpl->finishCommandBuffer(
        owned_command, encoded && ended, error);
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderWindLightSkyBehind(
    const ALMetalWorldWindLightSkyScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 ||
        !sampleableTexture(scene.rainbow_texture) ||
        !sampleableTexture(scene.halo_texture) ||
        scene.width == 0 || scene.height == 0 ||
        !finiteValues(scene.constants.camera_local, 4) ||
        !finiteValues(scene.constants.light_density, 4) ||
        !finiteValues(scene.constants.blue_horizon, 4) ||
        !finiteValues(scene.constants.glow, 4) ||
        !finiteValues(scene.constants.haze_extinction, 4) ||
        !finiteValues(scene.constants.blue_weight, 4) ||
        !finiteValues(scene.constants.haze_weight, 4) ||
        !finiteValues(scene.constants.sky_light, 4) ||
        !finiteValues(scene.constants.sky_ambient, 4) ||
        !finiteValues(scene.constants.sky_light_atten, 4) ||
        !finiteValues(scene.constants.sky_ambient_below, 4) ||
        !finiteValues(scene.constants.optics, 4) ||
        scene.constants.light_density[3] < 0.f ||
        scene.constants.glow[3] < 0.f ||
        scene.constants.optics[0] < 0.f ||
        scene.constants.optics[1] < 0.f ||
        scene.constants.optics[2] < 0.f ||
        !std::isfinite(scene.sky_hdr_scale) ||
        scene.sky_hdr_scale < 0.f)
    {
        setError(error, "Metal world WindLight sky-behind scene is incomplete");
        return nullptr;
    }

    const ALMetalTextureDescriptor& rainbow =
        scene.rainbow_texture->descriptor();
    const ALMetalTextureDescriptor& halo = scene.halo_texture->descriptor();
    if (rainbow.type != ALMetalTextureType::texture_2d ||
        halo.type != ALMetalTextureType::texture_2d ||
        rainbow.width == 0 || rainbow.height == 0 ||
        halo.width == 0 || halo.height == 0 ||
        rainbow.sample_count != 1 || halo.sample_count != 1)
    {
        setError(error, "Metal world WindLight sky-behind optics are incompatible");
        return nullptr;
    }

    constexpr std::size_t float4_stride = sizeof(float) * 4;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices || batch.vertex_count == 0 ||
            batch.index_count < 3 ||
            batch.position_offset % float4_stride != 0 ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error, "Metal world WindLight sky-behind batch is invalid");
            return nullptr;
        }
    }

    const std::uint32_t width = std::max(scene.width / 2u, 1u);
    const std::uint32_t height = std::max(scene.height / 2u, 1u);
    ALMetalRenderTargetDescriptor target_descriptor;
    target_descriptor.width = width;
    target_descriptor.height = height;
    target_descriptor.colors[0].format = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    target_descriptor.colors[0].label =
        "Alchemy WindLight sky-behind color";
    target_descriptor.label =
        "Alchemy native WindLight sky-behind target";
    auto target = ALMetalRenderTarget::create(
        *mImpl->device, target_descriptor, error);
    if (!target)
    {
        return nullptr;
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    auto pass = target->makeRenderPass(pass_config, error);
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = pass ? mImpl->acquireCommandBuffer(
        "Alchemy native WindLight sky-behind", owned_command, error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy WindLight sky-behind encoder", error)
                           : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(width),
        static_cast<double>(height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, width, height};
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(
            *mImpl->windlight_sky_behind_pipelines[scene.hdr ? 1 : 0],
            error) &&
        // Match the main WindLight dome and GL's raw AnisoWrap optics reads.
        encoder->setFragmentTextureLinear(
            0, scene.rainbow_texture, error) &&
        encoder->setFragmentTextureLinear(
            1, scene.halo_texture, error) &&
        encoder->setFragmentSampler(
            0, mImpl->diffuse_sampler.get(), error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        WorldWindLightSkyConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        constants.sky = scene.constants;
        constants.transfer[0] = static_cast<float>(width);
        constants.transfer[1] = static_cast<float>(height);
        constants.transfer[2] = scene.sky_hdr_scale;
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBytes(
                1, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle_strip,
                batch.index_count, batch.index_type,
                *batch.indices, batch.index_offset, 1, error);
    }

    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended, error))
    {
        return nullptr;
    }
    return target;
}

bool ALMetalWorldGBufferRenderer::compositeWindLightStars(
    ALMetalRenderTarget& target,
    const ALMetalWorldStarScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || scene.width == 0 || scene.height == 0 ||
        !std::isfinite(scene.star_alpha) || scene.star_alpha < 0.001f ||
        !std::isfinite(scene.time) ||
        !finiteValues(scene.moon_direction, 4) ||
        !finiteValues(scene.moon_quad_center, 4) ||
        !finiteValues(scene.moon_quad_u, 4) ||
        !finiteValues(scene.moon_quad_v, 4) ||
        !std::isfinite(scene.moon_brightness) ||
        scene.moon_brightness < 0.f ||
        (scene.moon_mask && !sampleableTexture(scene.moon_texture)))
    {
        setError(error, "Metal world WindLight star scene is incomplete");
        return false;
    }

    if (scene.moon_mask)
    {
        const ALMetalTextureDescriptor& moon =
            scene.moon_texture->descriptor();
        if (moon.type != ALMetalTextureType::texture_2d ||
            moon.width == 0 || moon.height == 0 || moon.sample_count != 1)
        {
            setError(error,
                     "Metal world WindLight star moon mask is incompatible");
            return false;
        }
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal world WindLight star target is incompatible");
        return false;
    }

    constexpr std::size_t position_stride = sizeof(float) * 4;
    constexpr std::size_t color_stride = sizeof(std::uint8_t) * 4;
    constexpr std::size_t texcoord_stride = sizeof(float) * 2;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldStarBatch& batch = scene.batches[index];
        if (!batch.vertices || batch.vertex_count < 3 ||
            batch.vertex_count % 3 != 0 ||
            batch.position_offset % position_stride != 0 ||
            batch.color_offset % color_stride != 0 ||
            batch.texcoord_offset % alignof(float) != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       position_stride, batch.vertices->length()) ||
            !rangeFits(batch.color_offset, batch.vertex_count,
                       color_stride, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       texcoord_stride, batch.vertices->length()))
        {
            setError(error, "Metal world WindLight star batch is invalid");
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.colors[0].linear_storage_view = !scene.hdr;
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = pass ? mImpl->acquireCommandBuffer(
        "Alchemy native world WindLight stars", owned_command, error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world WindLight star encoder", error)
                           : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    const ALMetalRenderPipeline* pipeline =
        mImpl->windlight_star_pipelines[scene.hdr ? 1 : 0]
                                       [scene.normal_hdr ? 1 : 0].get();
    const ALMetalTexture* moon = scene.moon_mask
        ? scene.moon_texture : mImpl->inactive_ssr_color.get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setPipeline(*pipeline, error) &&
        encoder->setFragmentTexture(0, moon, error) &&
        encoder->setFragmentSampler(0, mImpl->ramp_sampler.get(), error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldStarBatch& batch = scene.batches[index];
        WorldStarConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        constants.screen_alpha_time[0] = static_cast<float>(scene.width);
        constants.screen_alpha_time[1] = static_cast<float>(scene.height);
        constants.screen_alpha_time[2] = scene.star_alpha;
        constants.screen_alpha_time[3] = scene.time;
        std::memcpy(constants.moon_direction_brightness,
                    scene.moon_direction,
                    sizeof(scene.moon_direction));
        constants.moon_direction_brightness[3] = scene.moon_brightness;
        std::memcpy(constants.moon_quad_center, scene.moon_quad_center,
                    sizeof(scene.moon_quad_center));
        std::memcpy(constants.moon_quad_u, scene.moon_quad_u,
                    sizeof(scene.moon_quad_u));
        std::memcpy(constants.moon_quad_v, scene.moon_quad_v,
                    sizeof(scene.moon_quad_v));
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                1, *batch.vertices, batch.color_offset, error) &&
            encoder->setVertexBuffer(
                2, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBytes(
                3, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->draw(
                ALMetalPrimitiveType::triangle, 0,
                batch.vertex_count, 1, error);
    }

    const bool ended = encoder->end(error);
    return mImpl->finishCommandBuffer(
        owned_command, encoded && ended, error);
}

bool ALMetalWorldGBufferRenderer::compositeWindLightHeavenlyBodies(
    ALMetalRenderTarget& target,
    const ALMetalWorldHeavenlyBodyScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || scene.width == 0 || scene.height == 0)
    {
        setError(error,
                 "Metal world WindLight heavenly-body scene is incomplete");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error,
                 "Metal world WindLight heavenly-body target is incompatible");
        return false;
    }

    constexpr std::size_t position_stride = sizeof(float) * 4;
    constexpr std::size_t texcoord_stride = sizeof(float) * 2;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldHeavenlyBodyBatch& batch = scene.batches[index];
        const std::size_t index_stride = batch.index_type ==
            ALMetalIndexType::uint32 ? sizeof(std::uint32_t)
                                     : sizeof(std::uint16_t);
        if (!batch.vertices || !batch.indices || !batch.texture_a ||
            !batch.texture_b || batch.vertex_count < 3 ||
            batch.index_count < 3 ||
            batch.position_offset % position_stride != 0 ||
            batch.texcoord_offset % alignof(float) != 0 ||
            batch.index_offset % index_stride != 0 ||
            !sampleableTexture(batch.texture_a) ||
            !sampleableTexture(batch.texture_b) ||
            !finiteValues(batch.modelview_projection, 16) ||
            !finiteValues(batch.parameters, 4) ||
            batch.parameters[0] < 0.f ||
            (!batch.moon && batch.parameters[0] > 1.f) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       position_stride, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       texcoord_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error,
                     "Metal world WindLight heavenly-body batch is invalid");
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.colors[0].linear_storage_view = !scene.hdr;
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = pass ? mImpl->acquireCommandBuffer(
        "Alchemy native world WindLight heavenly bodies", owned_command,
        error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass,
        "Alchemy world WindLight heavenly-body encoder", error) : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setFragmentSampler(
            0, mImpl->ramp_sampler.get(), error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldHeavenlyBodyBatch& batch = scene.batches[index];
        WorldHeavenlyBodyConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.parameters, batch.parameters,
                    sizeof(constants.parameters));
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        constants.config[2] = batch.moon ? 1u : 0u;
        const ALMetalRenderPipeline* pipeline = batch.moon
            ? mImpl->windlight_moon_pipelines[scene.hdr ? 1 : 0]
                                                [scene.normal_hdr ? 1 : 0]
                  .get()
            : mImpl->windlight_sun_pipelines[scene.hdr ? 1 : 0]
                                               [scene.normal_hdr ? 1 : 0]
                  .get();
        encoded =
            encoder->setPipeline(*pipeline, error) &&
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                1, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBytes(
                2, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->setFragmentTexture(
                0, batch.texture_a, error) &&
            encoder->setFragmentTexture(
                1, batch.texture_b, error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle,
                batch.index_count, batch.index_type,
                *batch.indices, batch.index_offset, 1, error);
    }

    const bool ended = encoder->end(error);
    return mImpl->finishCommandBuffer(
        owned_command, encoded && ended, error);
}

bool ALMetalWorldGBufferRenderer::compositeWindLightMeteors(
    ALMetalRenderTarget& target,
    const ALMetalWorldMeteorScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || scene.width == 0 || scene.height == 0 ||
        !std::isfinite(scene.width_pixels) || scene.width_pixels <= 0.f)
    {
        setError(error, "Metal world WindLight meteor scene is incomplete");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal world WindLight meteor target is incompatible");
        return false;
    }

    constexpr std::size_t vector_stride = sizeof(float) * 4;
    constexpr std::size_t color_stride = sizeof(std::uint8_t) * 4;
    constexpr std::size_t texcoord_stride = sizeof(float) * 2;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldMeteorBatch& batch = scene.batches[index];
        if (!batch.vertices || batch.vertex_count < 3 ||
            batch.vertex_count % 3 != 0 ||
            batch.position_offset % vector_stride != 0 ||
            batch.normal_offset % vector_stride != 0 ||
            batch.color_offset % color_stride != 0 ||
            batch.texcoord_offset % alignof(float) != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       vector_stride, batch.vertices->length()) ||
            !rangeFits(batch.normal_offset, batch.vertex_count,
                       vector_stride, batch.vertices->length()) ||
            !rangeFits(batch.color_offset, batch.vertex_count,
                       color_stride, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       texcoord_stride, batch.vertices->length()))
        {
            setError(error, "Metal world WindLight meteor batch is invalid");
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.colors[0].linear_storage_view = !scene.hdr;
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = pass ? mImpl->acquireCommandBuffer(
        "Alchemy native world WindLight meteors", owned_command, error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world WindLight meteor encoder", error)
                           : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    const ALMetalRenderPipeline* pipeline =
        mImpl->windlight_meteor_pipelines[scene.hdr ? 1 : 0]
                                           [scene.normal_hdr ? 1 : 0]
            .get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setPipeline(*pipeline, error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldMeteorBatch& batch = scene.batches[index];
        WorldMeteorConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        constants.screen_width[0] = static_cast<float>(scene.width);
        constants.screen_width[1] = static_cast<float>(scene.height);
        constants.screen_width[2] = scene.width_pixels;
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                1, *batch.vertices, batch.normal_offset, error) &&
            encoder->setVertexBuffer(
                2, *batch.vertices, batch.color_offset, error) &&
            encoder->setVertexBuffer(
                3, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBytes(
                4, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->draw(
                ALMetalPrimitiveType::triangle, 0,
                batch.vertex_count, 1, error);
    }

    const bool ended = encoder->end(error);
    return mImpl->finishCommandBuffer(
        owned_command, encoded && ended, error);
}

bool ALMetalWorldGBufferRenderer::compositeWindLightAurora(
    ALMetalRenderTarget& target,
    const ALMetalWorldAuroraScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || scene.width == 0 || scene.height == 0 ||
        !finiteValues(scene.camera_local, 4) ||
        !std::isfinite(scene.intensity) || scene.intensity <= 0.001f ||
        !std::isfinite(scene.time))
    {
        setError(error, "Metal world WindLight aurora scene is incomplete");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal world WindLight aurora target is incompatible");
        return false;
    }

    constexpr std::size_t position_stride = sizeof(float) * 4;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        const std::size_t index_stride = batch.index_type ==
            ALMetalIndexType::uint32 ? sizeof(std::uint32_t)
                                     : sizeof(std::uint16_t);
        if (!batch.vertices || !batch.indices || batch.vertex_count < 3 ||
            batch.index_count < 3 ||
            batch.position_offset % position_stride != 0 ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       position_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error, "Metal world WindLight aurora batch is invalid");
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.colors[0].linear_storage_view = !scene.hdr;
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = pass ? mImpl->acquireCommandBuffer(
        "Alchemy native world WindLight aurora", owned_command, error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world WindLight aurora encoder", error)
                           : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    const ALMetalRenderPipeline* pipeline =
        mImpl->windlight_aurora_pipelines[scene.hdr ? 1 : 0]
                                           [scene.normal_hdr ? 1 : 0]
            .get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setPipeline(*pipeline, error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldHDRISkyBatch& batch = scene.batches[index];
        WorldAuroraConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        std::memcpy(constants.camera_intensity, scene.camera_local,
                    sizeof(scene.camera_local));
        constants.camera_intensity[3] = scene.intensity;
        constants.time_screen[0] = scene.time;
        constants.time_screen[1] = static_cast<float>(scene.width);
        constants.time_screen[2] = static_cast<float>(scene.height);
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBytes(
                1, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle_strip,
                batch.index_count, batch.index_type,
                *batch.indices, batch.index_offset, 1, error);
    }

    const bool ended = encoder->end(error);
    return mImpl->finishCommandBuffer(
        owned_command, encoded && ended, error);
}

bool ALMetalWorldGBufferRenderer::compositeWindLightClouds(
    ALMetalRenderTarget& target,
    const ALMetalWorldCloudScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 ||
        !sampleableTexture(scene.noise_texture) ||
        !sampleableTexture(scene.next_noise_texture) ||
        scene.width == 0 || scene.height == 0 ||
        !finiteValues(scene.constants.camera_local, 4) ||
        !finiteValues(scene.constants.light_density, 4) ||
        !finiteValues(scene.constants.blue_horizon, 4) ||
        !finiteValues(scene.constants.glow, 4) ||
        !finiteValues(scene.constants.haze_extinction, 4) ||
        !finiteValues(scene.constants.blue_weight, 4) ||
        !finiteValues(scene.constants.haze_weight, 4) ||
        !finiteValues(scene.constants.sky_sunlight, 4) ||
        !finiteValues(scene.constants.atmos_ambient, 4) ||
        !finiteValues(scene.constants.sky_light_atten, 4) ||
        !finiteValues(scene.constants.cloud_sunlight, 4) ||
        !finiteValues(scene.constants.cloud_color_shadow, 4) ||
        !finiteValues(scene.constants.cloud_pos_density1, 4) ||
        !finiteValues(scene.constants.cloud_pos_density2, 4) ||
        !finiteValues(scene.constants.parameters, 4) ||
        scene.constants.light_density[3] < 0.f ||
        scene.constants.glow[3] < 0.f ||
        scene.constants.parameters[0] < 0.f ||
        scene.constants.parameters[1] < 0.f ||
        scene.constants.parameters[2] < 0.f ||
        scene.constants.parameters[2] > 1.f)
    {
        setError(error, "Metal world WindLight cloud scene is incomplete");
        return false;
    }

    const ALMetalTextureDescriptor& noise =
        scene.noise_texture->descriptor();
    const ALMetalTextureDescriptor& next_noise =
        scene.next_noise_texture->descriptor();
    if (noise.type != ALMetalTextureType::texture_2d ||
        next_noise.type != ALMetalTextureType::texture_2d ||
        noise.width == 0 || noise.height == 0 ||
        next_noise.width == 0 || next_noise.height == 0 ||
        noise.sample_count != 1 || next_noise.sample_count != 1)
    {
        setError(error, "Metal world WindLight cloud noise is incompatible");
        return false;
    }

    const ALMetalRenderTargetDescriptor& descriptor = target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (descriptor.width != scene.width ||
        descriptor.height != scene.height || descriptor.sample_count != 1 ||
        descriptor.color_attachment_count != 4 ||
        descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        descriptor.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        descriptor.colors[2].format != expected_normal ||
        descriptor.colors[3].format != expected_emissive ||
        descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error, "Metal world WindLight cloud target is incompatible");
        return false;
    }

    constexpr std::size_t position_stride = sizeof(float) * 4;
    constexpr std::size_t texcoord_stride = sizeof(float) * 2;
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldCloudBatch& batch = scene.batches[index];
        const std::size_t index_stride = batch.index_type ==
            ALMetalIndexType::uint32 ? sizeof(std::uint32_t)
                                     : sizeof(std::uint16_t);
        if (!batch.vertices || !batch.indices || batch.vertex_count < 3 ||
            batch.index_count < 3 ||
            batch.position_offset % position_stride != 0 ||
            batch.texcoord_offset % alignof(float) != 0 ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       position_stride, batch.vertices->length()) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       texcoord_stride, batch.vertices->length()) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error, "Metal world WindLight cloud batch is invalid");
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.colors[0].linear_storage_view = !scene.hdr;
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = pass ? mImpl->acquireCommandBuffer(
        "Alchemy native world WindLight clouds", owned_command, error) : nullptr;
    auto encoder = command ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world WindLight cloud encoder", error)
                           : nullptr;
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    const ALMetalDepthStencilState* depth = scene.reverse_z
        ? mImpl->reverse_readonly_depth.get()
        : mImpl->forward_readonly_depth.get();
    const ALMetalRenderPipeline* pipeline =
        mImpl->windlight_cloud_pipelines[scene.hdr ? 1 : 0]
                                          [scene.normal_hdr ? 1 : 0]
            .get();
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(depth, error) &&
        encoder->setPipeline(*pipeline, error) &&
        // cloudsF.glsl binds both fetched noise maps with raw AnisoWrap.
        // Their viewer texture mirrors are sRGB-backed, so use compatible
        // linear views before density/self-shadow arithmetic just as GL's
        // GL_SKIP_DECODE_EXT sampler does.
        encoder->setFragmentTextureLinear(
            0, scene.noise_texture, error) &&
        encoder->setFragmentTextureLinear(
            1, scene.next_noise_texture, error) &&
        encoder->setFragmentSampler(
            0, mImpl->diffuse_sampler.get(), error);

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldCloudBatch& batch = scene.batches[index];
        WorldCloudConstants constants{};
        std::memcpy(constants.modelview_projection,
                    batch.modelview_projection,
                    sizeof(constants.modelview_projection));
        constants.cloud = scene.constants;
        constants.screen[0] = static_cast<float>(scene.width);
        constants.screen[1] = static_cast<float>(scene.height);
        constants.config[0] = scene.reverse_z ? 1u : 0u;
        constants.config[1] = scene.hdr ? 1u : 0u;
        encoded =
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                1, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBytes(
                2, &constants, sizeof(constants), error) &&
            encoder->setFragmentBytes(
                0, &constants, sizeof(constants), error) &&
            encoder->drawIndexed(
                ALMetalPrimitiveType::triangle_strip,
                batch.index_count, batch.index_type,
                *batch.indices, batch.index_offset, 1, error);
    }

    const bool ended = encoder->end(error);
    return mImpl->finishCommandBuffer(
        owned_command, encoded && ended, error);
}

std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::renderBaseLighting(
    const ALMetalWorldBaseLightingScene& scene,
    std::string* error)
{
    const bool probe_environment =
        validPBRReflectionProbes(scene.reflection_probes);
    const bool legacy_environment =
        validLegacyEnvironment(scene.legacy_environment);
    if (!mImpl || !mImpl->device || !scene.gbuffer ||
        !sampleableTexture(scene.light_map) ||
        probe_environment == legacy_environment ||
        probe_environment != (scene.reflection_probes != nullptr) ||
        legacy_environment != (scene.legacy_environment != nullptr) ||
        !finiteValues(scene.constants.inverse_projection, 16) ||
        !finiteValues(scene.constants.light_direction, 4) ||
        !finiteValues(scene.constants.atmos_sunlit, 4) ||
        !finiteValues(scene.constants.atmos_sunlit_linear, 4) ||
        !finiteValues(scene.constants.atmos_amblit, 4) ||
        !finiteValues(&scene.constants.ssao_effect[0][0], 12) ||
        !finiteValues(scene.constants.lighting, 4) ||
        !finiteValues(scene.constants.surface, 4) ||
        !finiteValues(&scene.constants.environment_matrix[0][0], 12) ||
        !finiteValues(scene.constants.environment, 4) ||
        !finiteValues(scene.constants.sky_transfer, 4) ||
        scene.constants.surface[0] <= 0.f ||
        scene.constants.sky_transfer[0] < 0.f ||
        scene.constants.sky[0] > 1 ||
        scene.constants.sky[1] > 1 ||
        scene.constants.sky[2] > 2 ||
        scene.constants.sky[3] != 0 ||
        (scene.constants.sky[0] != 0 &&
         scene.constants.sky[1] != (scene.hdr ? 1u : 0u)) ||
        (scene.constants.sky[0] == 0 && scene.constants.sky[2] != 0) ||
        scene.constants.config[0] > 1 ||
        scene.constants.config[1] > 1 ||
        scene.constants.config[2] > 1 ||
        scene.constants.config[3] > 1 ||
        scene.constants.environment_config[0] > 1 ||
        scene.constants.environment_config[1] != 0 ||
        scene.constants.environment_config[2] != 0 ||
        scene.constants.environment_config[3] != 0 ||
        (scene.constants.environment_config[0] != 0) !=
            legacy_environment ||
        (legacy_environment &&
         (scene.hero_probe || scene.screen_space_reflections)))
    {
        setError(error, "Metal world base-light scene is incomplete");
        return nullptr;
    }
    if (scene.hero_probe && !validHeroProbe(scene.hero_probe))
    {
        setError(error, "Metal world base-light hero probe is invalid");
        return nullptr;
    }
    if (scene.screen_space_reflections &&
        !validScreenSpaceReflections(scene.screen_space_reflections))
    {
        setError(
            error,
            "Metal world base-light screen-space reflections are invalid");
        return nullptr;
    }

    const ALMetalRenderTargetDescriptor& input =
        scene.gbuffer->descriptor();
    const ALMetalTexture* albedo = scene.gbuffer->colorTexture(0);
    const ALMetalTexture* specular = scene.gbuffer->colorTexture(1);
    const ALMetalTexture* normal = scene.gbuffer->colorTexture(2);
    const ALMetalTexture* emissive = scene.gbuffer->colorTexture(3);
    const ALMetalTexture* depth = scene.gbuffer->depthStencilTexture();
    const bool normal_format =
        input.colors[2].format == ALMetalPixelFormat::rgb10a2_unorm ||
        input.colors[2].format == ALMetalPixelFormat::rgba16_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    const ALMetalTextureDescriptor& light_map =
        scene.light_map->descriptor();
    if (input.width == 0 || input.height == 0 || input.sample_count != 1 ||
        input.color_attachment_count != 4 ||
        input.colors[0].format != ALMetalPixelFormat::rgba8_unorm_srgb ||
        input.colors[1].format != ALMetalPixelFormat::rgba8_unorm ||
        !normal_format || input.colors[3].format != expected_emissive ||
        input.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8 ||
        !input.depth_stencil_shader_read ||
        light_map.format != ALMetalPixelFormat::rg8_unorm ||
        light_map.width != input.width || light_map.height != input.height ||
        !sampleableTexture(albedo) || !sampleableTexture(specular) ||
        !sampleableTexture(normal) || !sampleableTexture(emissive) ||
        !sampleableTexture(depth))
    {
        setError(error, "Metal world base-light G-buffer is incompatible");
        return nullptr;
    }

    ALMetalRenderTargetDescriptor output_descriptor;
    output_descriptor.width = input.width;
    output_descriptor.height = input.height;
    output_descriptor.colors[0].format = scene.hdr
        ? ALMetalPixelFormat::rgba16_float
        : ALMetalPixelFormat::rgba8_unorm;
    output_descriptor.colors[0].label =
        "Alchemy world base-light accumulation";
    if (scene.carry_depth)
    {
        output_descriptor.depth_stencil_format =
            ALMetalPixelFormat::depth32_float_stencil8;
        output_descriptor.depth_stencil_shader_read =
            scene.depth_stencil_shader_read;
    }
    output_descriptor.label = "Alchemy native world base-light target";
    auto output = ALMetalRenderTarget::create(
        *mImpl->device, output_descriptor, error);
    if (!output)
    {
        return nullptr;
    }

    ALMetalBufferDescriptor probe_buffer_descriptor;
    probe_buffer_descriptor.length =
        sizeof(ALMetalWorldReflectionProbeConstants);
    probe_buffer_descriptor.storage = ALMetalStorageMode::shared;
    probe_buffer_descriptor.cpu_cache =
        ALMetalCpuCacheMode::write_combined;
    probe_buffer_descriptor.label =
        "Alchemy world base-light reflection-probe constants";
    auto probe_constants_buffer = ALMetalBuffer::create(
        *mImpl->device, probe_buffer_descriptor, error);
    const ALMetalWorldReflectionProbeConstants inactive_probe_constants{};
    const ALMetalWorldReflectionProbeConstants& probe_constants =
        scene.reflection_probes
            ? scene.reflection_probes->constants
            : inactive_probe_constants;
    if (!probe_constants_buffer || !probe_constants_buffer->write(
            0, &probe_constants,
            probe_buffer_descriptor.length, error))
    {
        return nullptr;
    }

    std::unique_ptr<ALMetalCommandBuffer> owned_command;
    ALMetalCommandBuffer* command = mImpl->acquireCommandBuffer(
        "Alchemy native world base lighting", owned_command, error);
    if (!command)
    {
        return nullptr;
    }

    constexpr std::uint32_t brdf_lut_size = 512;
    const bool generate_brdf_lut = !mImpl->brdf_lut_ready &&
        !mImpl->command_batch_generates_brdf_lut;
    if (generate_brdf_lut)
    {
        auto compute = ALMetalComputeEncoder::begin(
            *command, "Alchemy world PBR BRDF LUT encoder", error);
        if (!compute ||
            !compute->setPipeline(*mImpl->brdf_lut_pipeline, error) ||
            !compute->setTexture(0, mImpl->brdf_lut.get(), error) ||
            !compute->dispatch(
                brdf_lut_size, brdf_lut_size, 1, 8, 8, 1, error) ||
            !compute->end(error))
        {
            return nullptr;
        }
    }

    ALMetalRenderPassConfig pass_config;
    pass_config.colors[0].load = ALMetalLoadAction::clear;
    pass_config.colors[0].store = ALMetalStoreAction::store;
    pass_config.colors[0].clear = {0.0, 0.0, 0.0, 0.0};
    // The base pipeline only shades color. An optional carried depth
    // attachment is populated by the exact G-buffer blit below and must not
    // participate in this render pass's pipeline compatibility contract.
    pass_config.bind_depth_stencil = false;
    auto pass = output->makeRenderPass(pass_config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world base-light encoder", error)
        : nullptr;
    if (!encoder)
    {
        return nullptr;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(input.width),
        static_cast<double>(input.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, input.width, input.height};
    const ALMetalTexture* probe_radiance = scene.reflection_probes
        ? scene.reflection_probes->radiance_texture
        : mImpl->inactive_probe_radiance.get();
    const ALMetalTexture* probe_irradiance = scene.reflection_probes
        ? scene.reflection_probes->irradiance_texture
        : mImpl->inactive_probe_irradiance.get();
    const ALMetalTexture* legacy_environment_texture =
        scene.legacy_environment
            ? scene.legacy_environment
            : mImpl->inactive_hero_probe.get();
    const ALMetalTexture* hero_texture = scene.hero_probe
        ? scene.hero_probe->radiance_texture
        : mImpl->inactive_hero_probe.get();
    const ALMetalWorldHeroProbeConstants& hero_constants =
        scene.hero_probe
            ? scene.hero_probe->constants
            : mImpl->inactive_hero_constants;
    const ALMetalTexture* ssr_color = scene.screen_space_reflections
        ? scene.screen_space_reflections->scene_color
        : mImpl->inactive_ssr_color.get();
    const ALMetalTexture* ssr_depth = scene.screen_space_reflections
        ? scene.screen_space_reflections->scene_depth
        : mImpl->inactive_ssr_depth.get();
    const ALMetalWorldScreenSpaceReflectionConstants& ssr_constants =
        scene.screen_space_reflections
            ? scene.screen_space_reflections->constants
            : mImpl->inactive_ssr_constants;
    const bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(
            *mImpl->base_light_pipelines[scene.hdr ? 1 : 0], error) &&
        encoder->setFragmentBytes(
            0, &scene.constants, sizeof(scene.constants), error) &&
        encoder->setFragmentBuffer(
            1, *probe_constants_buffer, 0, error) &&
        encoder->setFragmentBytes(
            2, &hero_constants, sizeof(hero_constants), error) &&
        encoder->setFragmentBytes(
            3, &ssr_constants, sizeof(ssr_constants), error) &&
        encoder->setFragmentTexture(0, albedo, error) &&
        encoder->setFragmentTexture(1, specular, error) &&
        encoder->setFragmentTexture(2, normal, error) &&
        encoder->setFragmentTexture(3, emissive, error) &&
        encoder->setFragmentTexture(4, depth, error) &&
        encoder->setFragmentTexture(5, scene.light_map, error) &&
        encoder->setFragmentTexture(6, probe_radiance, error) &&
        encoder->setFragmentTexture(7, probe_irradiance, error) &&
        encoder->setFragmentTexture(8, mImpl->brdf_lut.get(), error) &&
        encoder->setFragmentTexture(9, hero_texture, error) &&
        encoder->setFragmentTexture(10, ssr_color, error) &&
        encoder->setFragmentTexture(11, ssr_depth, error) &&
        encoder->setFragmentTexture(
            12, legacy_environment_texture, error) &&
        encoder->setFragmentSampler(
            0, mImpl->point_clamp_sampler.get(), error) &&
        encoder->setFragmentSampler(
            1, mImpl->probe_sampler.get(), error) &&
        encoder->setFragmentSampler(
            2, mImpl->linear_clamp_sampler.get(), error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    const bool ended = encoder->end(error);
    const bool copied_depth =
        !scene.carry_depth ||
        (encoded && ended && output->copyDepthStencilFrom(
            *command, *depth, {}, error));
    if (!mImpl->finishCommandBuffer(
            owned_command, encoded && ended && copied_depth, error))
    {
        return nullptr;
    }
    if (generate_brdf_lut)
    {
        if (mImpl->command_batch)
        {
            mImpl->command_batch_generates_brdf_lut = true;
        }
        else
        {
            mImpl->brdf_lut_ready = true;
        }
    }
    return output;
}

bool ALMetalWorldGBufferRenderer::compositeImpostorPostDeferred(
    ALMetalRenderTarget& target,
    const ALMetalWorldGBufferScene& scene,
    std::string* error)
{
    if (!mImpl || !mImpl->device || !scene.batches ||
        scene.batch_count == 0 || scene.width == 0 || scene.height == 0)
    {
        setError(error,
                 "Metal world impostor post-deferred scene is incomplete");
        return false;
    }

    const ALMetalRenderTargetDescriptor& target_descriptor =
        target.descriptor();
    const ALMetalPixelFormat expected_normal = scene.normal_hdr
        ? ALMetalPixelFormat::rgba16_unorm
        : ALMetalPixelFormat::rgb10a2_unorm;
    const ALMetalPixelFormat expected_emissive = scene.hdr
        ? ALMetalPixelFormat::rg11b10_float
        : ALMetalPixelFormat::rgba8_unorm;
    if (target_descriptor.width != scene.width ||
        target_descriptor.height != scene.height ||
        target_descriptor.sample_count != 1 ||
        target_descriptor.color_attachment_count != 4 ||
        target_descriptor.colors[0].format !=
            ALMetalPixelFormat::rgba8_unorm_srgb ||
        target_descriptor.colors[1].format !=
            ALMetalPixelFormat::rgba8_unorm ||
        target_descriptor.colors[2].format != expected_normal ||
        target_descriptor.colors[3].format != expected_emissive ||
        target_descriptor.depth_stencil_format !=
            ALMetalPixelFormat::depth32_float_stencil8)
    {
        setError(error,
                 "Metal world impostor post-deferred target is incompatible");
        return false;
    }

    constexpr std::size_t float4_stride = sizeof(float) * 4;
    constexpr std::uint32_t max_skin_joints = 110;
    const bool has_shiny = std::any_of(
        scene.batches, scene.batches + scene.batch_count,
        [](const ALMetalWorldSimpleBatch& batch)
        {
            return batch.impostor_shiny;
        });
    const bool has_legacy_alpha = std::any_of(
        scene.batches, scene.batches + scene.batch_count,
        [](const ALMetalWorldSimpleBatch& batch)
        {
            return batch.material_slot_count != 0 &&
                   batch.material_alpha_mode == 1;
        });
    // The material fragment has one fixed resource contract. Bind a coherent
    // probe generation even for an env-zero slot; Metal validates declared
    // texture/buffer arguments independently of uniform control flow.
    const bool legacy_alpha_uses_probes = has_legacy_alpha;
    if ((has_shiny || legacy_alpha_uses_probes) &&
        !validReflectionProbes(scene.reflection_probes))
    {
        setError(error,
                 has_shiny
                     ? "Metal world shiny impostor reflection probes are invalid"
                     : "Metal world legacy alpha reflection probes are invalid");
        return false;
    }
    if (scene.hero_probe && !validHeroProbe(scene.hero_probe))
    {
        setError(error,
                 "Metal world impostor hero probe is invalid");
        return false;
    }
    if (scene.screen_space_reflections &&
        !validScreenSpaceReflections(scene.screen_space_reflections))
    {
        setError(error,
                 "Metal world impostor screen-space reflections are invalid");
        return false;
    }
    if (has_legacy_alpha &&
        !validLegacyAlphaLighting(scene.legacy_alpha_lighting))
    {
        setError(error,
                 "Metal world legacy alpha lighting is invalid");
        return false;
    }
    if (has_legacy_alpha && scene.directional_shadows &&
        !validDirectionalShadows(scene.directional_shadows))
    {
        setError(error,
                 "Metal world legacy alpha directional shadows are invalid");
        return false;
    }
    for (std::size_t index = 0; index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        const bool pbr = isPBRBatch(batch);
        const bool fullbright = batch.impostor_fullbright;
        const bool shiny = batch.impostor_shiny;
        const bool emboss = batch.impostor_emboss;
        const bool material = batch.material_slot_count != 0;
        const bool material_normal_map = materialHasNormalMap(batch);
        const bool material_specular_map = materialHasSpecularMap(batch);
        const std::size_t index_stride =
            batch.index_type == ALMetalIndexType::uint32 ? 4 : 2;
        if (!batch.vertices || !batch.indices ||
            batch.vertex_count == 0 || batch.index_count == 0 ||
            batch.index_count % 3 != 0 ||
            batch.texture_count == 0 ||
            batch.texture_count >
                ALMetalWorldSimpleBatch::max_diffuse_textures ||
            (emboss && batch.texture_count != 1) ||
            (pbr &&
             (batch.texture_count >
                  ALMetalWorldSimpleBatch::max_material_slots ||
              batch.pbr_slot_count != batch.texture_count)) ||
            (material &&
             (batch.material_alpha_mode != 1 ||
              batch.material_slot_count >
                  ALMetalWorldSimpleBatch::max_material_slots ||
              batch.material_slot_count != batch.texture_count ||
              pbr || fullbright || shiny || emboss || batch.alpha_mask)) ||
            (!material && batch.material_alpha_mode != 0) ||
            batch.normal_texture ||
            (!fullbright && batch.alpha_mask) ||
            (fullbright && (pbr || material || shiny || emboss)) ||
            (shiny && (pbr || material || emboss)) ||
            (emboss && (pbr || material)) ||
            batch.avatar_rigid ||
            batch.avatar_skinning || batch.avatar_cloth ||
            batch.impostor || batch.terrain ||
            (!emboss && !batch.use_vertex_color) ||
            (!pbr && batch.double_sided) ||
            batch.position_offset % float4_stride != 0 ||
            (!emboss && !fullbright &&
             batch.normal_offset % float4_stride != 0) ||
            batch.texcoord_offset % (sizeof(float) * 2) != 0 ||
            (emboss &&
             batch.texcoord1_offset % (sizeof(float) * 2) != 0) ||
            (!emboss &&
             batch.color_offset % (sizeof(std::uint8_t) * 4) != 0) ||
            ((pbr || material_normal_map) &&
             batch.tangent_offset % float4_stride != 0) ||
            (material_normal_map &&
             batch.texcoord1_offset % (sizeof(float) * 2) != 0) ||
            (material_specular_map &&
             batch.texcoord2_offset % (sizeof(float) * 2) != 0) ||
            batch.index_offset % index_stride != 0 ||
            !finiteValues(batch.modelview_projection, 16) ||
            ((shiny || material) && !finiteValues(batch.modelview, 16)) ||
            !finiteValues(&batch.normal_matrix[0][0], 12) ||
            !finiteValues(&batch.rigged_normal_matrix[0][0], 12) ||
            !finiteValues(batch.texture_matrix, 16) ||
            (fullbright &&
             (!std::isfinite(batch.alpha_cutoff) ||
              batch.alpha_cutoff < 0.f || batch.alpha_cutoff > 1.f)) ||
            !rangeFits(batch.position_offset, batch.vertex_count,
                       float4_stride, batch.vertices->length()) ||
            (!emboss && !fullbright &&
             !rangeFits(batch.normal_offset, batch.vertex_count,
                        float4_stride, batch.vertices->length())) ||
            !rangeFits(batch.texcoord_offset, batch.vertex_count,
                       sizeof(float) * 2, batch.vertices->length()) ||
            (emboss &&
             !rangeFits(batch.texcoord1_offset, batch.vertex_count,
                        sizeof(float) * 2,
                        batch.vertices->length())) ||
            (!emboss &&
             !rangeFits(batch.color_offset, batch.vertex_count,
                        sizeof(std::uint8_t) * 4,
                        batch.vertices->length())) ||
            (pbr && !rangeFits(batch.tangent_offset, batch.vertex_count,
                               float4_stride,
                               batch.vertices->length())) ||
            (material_normal_map &&
             (!rangeFits(batch.tangent_offset, batch.vertex_count,
                         float4_stride, batch.vertices->length()) ||
              !rangeFits(batch.texcoord1_offset, batch.vertex_count,
                         sizeof(float) * 2, batch.vertices->length()))) ||
            (material_specular_map &&
             !rangeFits(batch.texcoord2_offset, batch.vertex_count,
                        sizeof(float) * 2, batch.vertices->length())) ||
            !rangeFits(batch.index_offset, batch.index_count,
                       index_stride, batch.indices->length()))
        {
            setError(error,
                     "Metal world impostor post-deferred batch is invalid");
            return false;
        }
        if (batch.skin_palette)
        {
            const std::size_t expected_palette_floats =
                4 + static_cast<std::size_t>(batch.skin_joint_count) * 12;
            if (batch.skin_joint_count == 0 ||
                batch.skin_joint_count > max_skin_joints ||
                batch.skin_palette_float_count != expected_palette_floats ||
                batch.weight_offset % float4_stride != 0 ||
                !rangeFits(batch.weight_offset, batch.vertex_count,
                           float4_stride, batch.vertices->length()) ||
                !finiteValues(batch.skin_palette,
                              expected_palette_floats))
            {
                setError(error,
                         "Metal world impostor post-deferred skinning data is invalid");
                return false;
            }
        }
        else if (batch.weight_offset != 0 ||
                 batch.skin_palette_float_count != 0 ||
                 batch.skin_joint_count != 0)
        {
            setError(error,
                     "Metal world impostor post-deferred skinning data is incomplete");
            return false;
        }
        for (std::uint32_t slot = 0; slot < batch.texture_count; ++slot)
        {
            if (!sampleableTexture(batch.diffuse_textures[slot]))
            {
                setError(error,
                         "Metal world impostor post-deferred texture is invalid");
                return false;
            }
            if (pbr &&
                (!finiteValues(
                     &batch.pbr_slots[slot].texture_transform[0][0], 32) ||
                 batch.pbr_slots[slot].alpha_cutoff != -1.f))
            {
                setError(error,
                         "Metal world PBR alpha material is invalid");
                return false;
            }
            if (material)
            {
                const ALMetalWorldMaterialSlot& material_slot =
                    batch.material_slots[slot];
                if (!finiteValues(material_slot.specular_color, 4) ||
                    !std::isfinite(material_slot.env_intensity) ||
                    !std::isfinite(material_slot.fullbright) ||
                    material_slot.env_intensity < 0.f ||
                    material_slot.env_intensity > 1.f ||
                    material_slot.fullbright < 0.f ||
                    material_slot.fullbright > 1.f ||
                    (material_normal_map &&
                     !sampleableTexture(material_slot.normal_texture)) ||
                    (material_specular_map &&
                     !sampleableTexture(material_slot.specular_texture)))
                {
                    setError(error,
                             "Metal world legacy alpha material is invalid");
                    return false;
                }
            }
        }
    }

    std::vector<std::unique_ptr<ALMetalBuffer>> palette_storage;
    std::vector<const ALMetalBuffer*> palette_buffers(
        scene.batch_count, nullptr);
    for (std::size_t batch_index = 0;
         batch_index < scene.batch_count; ++batch_index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[batch_index];
        if (!batch.skin_palette)
        {
            continue;
        }
        for (std::size_t previous = 0; previous < batch_index; ++previous)
        {
            const ALMetalWorldSimpleBatch& candidate = scene.batches[previous];
            if (candidate.skin_palette == batch.skin_palette &&
                candidate.skin_palette_float_count ==
                    batch.skin_palette_float_count &&
                candidate.skin_joint_count == batch.skin_joint_count)
            {
                palette_buffers[batch_index] = palette_buffers[previous];
                break;
            }
        }
        if (palette_buffers[batch_index])
        {
            continue;
        }
        ALMetalBufferDescriptor descriptor;
        descriptor.length = batch.skin_palette_float_count * sizeof(float);
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label = "Alchemy world impostor post-deferred skin palette";
        auto palette = ALMetalBuffer::create(
            *mImpl->device, descriptor, error);
        if (!palette || !palette->write(
                0, batch.skin_palette, descriptor.length, error))
        {
            return false;
        }
        palette_buffers[batch_index] = palette.get();
        palette_storage.push_back(std::move(palette));
    }

    std::unique_ptr<ALMetalBuffer> probe_constants_buffer;
    if (has_shiny || legacy_alpha_uses_probes)
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length = sizeof(ALMetalWorldReflectionProbeConstants);
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
        descriptor.label = "Alchemy world reflection-probe constants";
        probe_constants_buffer = ALMetalBuffer::create(
            *mImpl->device, descriptor, error);
        if (!probe_constants_buffer || !probe_constants_buffer->write(
                0, &scene.reflection_probes->constants,
                descriptor.length, error))
        {
            return false;
        }
    }

    ALMetalRenderPassConfig pass_config;
    for (std::size_t attachment = 0; attachment < 4; ++attachment)
    {
        pass_config.colors[attachment].load = ALMetalLoadAction::load;
        pass_config.colors[attachment].store = ALMetalStoreAction::store;
    }
    pass_config.depth_load = ALMetalLoadAction::load;
    pass_config.depth_store = ALMetalStoreAction::store;
    pass_config.stencil_load = ALMetalLoadAction::load;
    pass_config.stencil_store = ALMetalStoreAction::store;
    auto pass = target.makeRenderPass(pass_config, error);
    if (!pass)
    {
        return false;
    }
    auto command = mImpl->device->makeCommandBuffer(
        "Alchemy native avatar-impostor post-deferred", error);
    if (!command)
    {
        return false;
    }
    auto encoder = ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy world impostor post-deferred encoder",
        error);
    if (!encoder)
    {
        return false;
    }

    const ALMetalViewport viewport{
        0.0, 0.0, static_cast<double>(scene.width),
        static_cast<double>(scene.height), 0.0, 1.0,
    };
    const ALMetalScissorRect scissor{0, 0, scene.width, scene.height};
    bool encoded =
        encoder->setViewport(viewport, error) &&
        encoder->setScissor(scissor, error) &&
        encoder->setFrontFacingWinding(ALMetalWinding::clockwise, error) &&
        encoder->setFragmentSampler(0, mImpl->diffuse_sampler.get(), error);
    if (encoded && (has_shiny || legacy_alpha_uses_probes))
    {
        const ALMetalTexture* hero_texture = scene.hero_probe
            ? scene.hero_probe->radiance_texture
            : mImpl->inactive_hero_probe.get();
        const ALMetalWorldHeroProbeConstants& hero_constants =
            scene.hero_probe
                ? scene.hero_probe->constants
                : mImpl->inactive_hero_constants;
        encoded =
            encoder->setFragmentSampler(
                1, mImpl->probe_sampler.get(), error) &&
            encoder->setFragmentTexture(
                32, scene.reflection_probes->radiance_texture, error) &&
            encoder->setFragmentBuffer(
                1, *probe_constants_buffer, 0, error) &&
            encoder->setFragmentTexture(
                34, hero_texture, error) &&
            encoder->setFragmentBytes(
                4, &hero_constants, sizeof(hero_constants), error);
    }
    if (encoded && has_shiny)
    {
        const ALMetalWorldAlphaEnvironment probe_environment{};
        encoded =
            encoder->setFragmentTexture(
                39, mImpl->inactive_hero_probe.get(), error) &&
            encoder->setFragmentBytes(
                0, &probe_environment,
                sizeof(probe_environment), error);
    }
    if (encoded && legacy_alpha_uses_probes)
    {
        const ALMetalTexture* ssr_color = scene.screen_space_reflections
            ? scene.screen_space_reflections->scene_color
            : mImpl->inactive_ssr_color.get();
        const ALMetalTexture* ssr_depth = scene.screen_space_reflections
            ? scene.screen_space_reflections->scene_depth
            : mImpl->inactive_ssr_depth.get();
        const ALMetalWorldScreenSpaceReflectionConstants& ssr_constants =
            scene.screen_space_reflections
                ? scene.screen_space_reflections->constants
                : mImpl->inactive_ssr_constants;
        encoded =
            encoder->setFragmentSampler(
                2, mImpl->linear_clamp_sampler.get(), error) &&
            encoder->setFragmentSampler(
                3, mImpl->point_clamp_sampler.get(), error) &&
            encoder->setFragmentTexture(35, ssr_color, error) &&
            encoder->setFragmentTexture(36, ssr_depth, error) &&
            encoder->setFragmentBytes(
                5, &ssr_constants, sizeof(ssr_constants), error);
    }

    for (std::size_t index = 0;
         encoded && index < scene.batch_count; ++index)
    {
        const ALMetalWorldSimpleBatch& batch = scene.batches[index];
        const bool pbr_batch = isPBRBatch(batch);
        const bool fullbright_batch = batch.impostor_fullbright;
        const bool shiny_batch = batch.impostor_shiny;
        const bool emboss_batch = batch.impostor_emboss;
        const bool material_batch = batch.material_slot_count != 0;
        const bool material_normal_map = materialHasNormalMap(batch);
        const bool material_specular_map = materialHasSpecularMap(batch);
        const bool rigged = batch.skin_palette != nullptr;
        const bool depth_writable =
            fullbright_batch || shiny_batch || (rigged && !emboss_batch);
        const ALMetalDepthStencilState* depth = depth_writable
            ? (scene.reverse_z ? mImpl->reverse_depth.get()
                               : mImpl->forward_depth.get())
            : (scene.reverse_z ? mImpl->reverse_readonly_depth.get()
                               : mImpl->forward_readonly_depth.get());
        const ALMetalRenderPipeline* pipeline = nullptr;
        if (emboss_batch)
        {
            pipeline = mImpl->emboss_pipelines[scene.hdr ? 1 : 0]
                                                [rigged ? 1 : 0]
                                                [scene.normal_hdr ? 1 : 0].get();
        }
        else if (shiny_batch)
        {
            pipeline = mImpl->shiny_pipelines[scene.hdr ? 1 : 0]
                                              [rigged ? 1 : 0]
                                              [scene.normal_hdr ? 1 : 0].get();
        }
        else if (fullbright_batch)
        {
            pipeline = mImpl->fullbright_pipelines[scene.hdr ? 1 : 0]
                                                   [batch.alpha_mask ? 1 : 0]
                                                   [rigged ? 1 : 0]
                                                   [scene.normal_hdr ? 1 : 0].get();
        }
        else if (pbr_batch)
        {
            pipeline = mImpl->pbr_alpha_pipelines[scene.hdr ? 1 : 0]
                                                  [rigged ? 1 : 0]
                                                  [scene.normal_hdr ? 1 : 0].get();
        }
        else if (material_batch)
        {
            pipeline = mImpl->legacy_alpha_pipelines[scene.hdr ? 1 : 0]
                                                   [rigged ? 1 : 0]
                                                   [scene.normal_hdr ? 1 : 0].get();
        }
        else
        {
            pipeline = mImpl->flat_alpha_pipelines[scene.hdr ? 1 : 0]
                                                   [rigged ? 1 : 0]
                                                   [scene.normal_hdr ? 1 : 0].get();
        }

        WorldSimpleConstants world{};
        std::memcpy(world.modelview_projection,
                    batch.modelview_projection,
                    sizeof(world.modelview_projection));
        std::memcpy(world.normal_matrix, batch.normal_matrix,
                    sizeof(world.normal_matrix));
        std::memcpy(world.rigged_normal_matrix,
                    batch.rigged_normal_matrix,
                    sizeof(world.rigged_normal_matrix));
        std::memcpy(world.texture_matrix, batch.texture_matrix,
                    sizeof(world.texture_matrix));
        world.material[0] = batch.alpha_cutoff;
        world.material[3] = batch.alpha_mask ? 1.f : 0.f;
        world.depth_convention[0] = scene.reverse_z ? 1u : 0u;
        world.depth_convention[1] = batch.texture_count;

        WorldShinyConstants shiny_world{};
        std::memcpy(shiny_world.modelview_projection,
                    batch.modelview_projection,
                    sizeof(shiny_world.modelview_projection));
        std::memcpy(shiny_world.modelview, batch.modelview,
                    sizeof(shiny_world.modelview));
        std::memcpy(shiny_world.normal_matrix, batch.normal_matrix,
                    sizeof(shiny_world.normal_matrix));
        std::memcpy(shiny_world.rigged_normal_matrix,
                    batch.rigged_normal_matrix,
                    sizeof(shiny_world.rigged_normal_matrix));
        std::memcpy(shiny_world.texture_matrix, batch.texture_matrix,
                    sizeof(shiny_world.texture_matrix));
        shiny_world.config[0] = scene.reverse_z ? 1u : 0u;
        shiny_world.config[1] = batch.texture_count;

        encoded =
            encoder->setDepthStencilState(depth, error) &&
            encoder->setCullMode(
                pbr_batch && batch.double_sided
                    ? ALMetalCullMode::none : ALMetalCullMode::back,
                error) &&
            encoder->setPipeline(*pipeline, error) &&
            encoder->setVertexBuffer(
                0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(
                2, *batch.vertices, batch.texcoord_offset, error);
        if (encoded)
        {
            encoded = (shiny_batch || material_batch)
                ? encoder->setVertexBytes(
                      4, &shiny_world, sizeof(shiny_world), error)
                : encoder->setVertexBytes(
                      4, &world, sizeof(world), error);
        }
        if (encoded && emboss_batch)
        {
            encoded = encoder->setVertexBuffer(
                9, *batch.vertices, batch.texcoord1_offset, error);
        }
        else if (encoded && fullbright_batch)
        {
            encoded = encoder->setVertexBuffer(
                3, *batch.vertices, batch.color_offset, error);
        }
        else if (encoded)
        {
            encoded =
                encoder->setVertexBuffer(
                    1, *batch.vertices, batch.normal_offset, error) &&
                encoder->setVertexBuffer(
                    3, *batch.vertices, batch.color_offset, error);
        }
        if (encoded && rigged)
        {
            encoded =
                encoder->setVertexBuffer(
                    5, *palette_buffers[index], 0, error) &&
                encoder->setVertexBuffer(
                    6, *batch.vertices, batch.weight_offset, error) &&
                encoder->setVertexBytes(
                    7, &batch.skin_joint_count,
                    sizeof(batch.skin_joint_count), error);
        }
        if (encoded && pbr_batch)
        {
            WorldPBRConstants pbr{};
            for (std::size_t slot = 0;
                 slot < batch.pbr_slot_count; ++slot)
            {
                const ALMetalWorldPBRSlot& source = batch.pbr_slots[slot];
                std::memcpy(pbr.slots[slot].texture_transform,
                            source.texture_transform,
                            sizeof(pbr.slots[slot].texture_transform));
            }
            pbr.config[0] = batch.pbr_slot_count;
            encoded =
                encoder->setVertexBuffer(
                    8, *batch.vertices, batch.tangent_offset, error) &&
                encoder->setVertexBytes(11, &pbr, sizeof(pbr), error) &&
                encoder->setFragmentBytes(0, &pbr, sizeof(pbr), error);
            for (std::size_t slot = 0;
                 encoded && slot <
                     ALMetalWorldSimpleBatch::max_material_slots; ++slot)
            {
                const std::size_t source = std::min<std::size_t>(
                    slot, batch.pbr_slot_count - 1);
                encoded = encoder->setFragmentTexture(
                    static_cast<std::uint32_t>(slot),
                    batch.diffuse_textures[source], error);
            }
        }
        else if (encoded && material_batch)
        {
            const ALMetalTexture* directional_depth =
                scene.directional_shadows
                    ? scene.directional_shadows->depth_texture
                    : mImpl->unshadowed_depth.get();
            const ALMetalWorldDirectionalShadowConstants&
                directional_constants = scene.directional_shadows
                    ? scene.directional_shadows->constants
                    : mImpl->unshadowed_constants;
            WorldMaterialConstants material{};
            for (std::size_t slot = 0;
                 slot < batch.material_slot_count; ++slot)
            {
                const ALMetalWorldMaterialSlot& source =
                    batch.material_slots[slot];
                std::memcpy(material.slots[slot].specular_color,
                            source.specular_color,
                            sizeof(material.slots[slot].specular_color));
                material.slots[slot].properties[0] = source.env_intensity;
                material.slots[slot].properties[1] = source.alpha_cutoff;
                material.slots[slot].properties[2] = source.fullbright;
            }
            material.config[0] = batch.material_slot_count;
            material.config[1] = batch.material_alpha_mode;
            material.config[2] = material_normal_map ? 1u : 0u;
            material.config[3] = material_specular_map ? 1u : 0u;
            encoded =
                encoder->setVertexBuffer(
                    8, *batch.vertices,
                    material_normal_map
                        ? batch.tangent_offset : batch.normal_offset,
                    error) &&
                encoder->setVertexBuffer(
                    9, *batch.vertices,
                    material_normal_map
                        ? batch.texcoord1_offset : batch.texcoord_offset,
                    error) &&
                encoder->setVertexBuffer(
                    10, *batch.vertices,
                    material_specular_map
                        ? batch.texcoord2_offset : batch.texcoord_offset,
                    error) &&
                encoder->setFragmentBytes(
                    0, &material, sizeof(material), error) &&
                encoder->setFragmentBytes(
                    2, scene.legacy_alpha_lighting,
                    sizeof(*scene.legacy_alpha_lighting), error) &&
                encoder->setFragmentTexture(
                    33, directional_depth, error) &&
                encoder->setFragmentBytes(
                    3, &directional_constants,
                    sizeof(directional_constants), error);
            for (std::size_t slot = 0;
                 encoded && slot <
                     ALMetalWorldSimpleBatch::max_material_slots; ++slot)
            {
                const std::size_t source = std::min<std::size_t>(
                    slot, batch.material_slot_count - 1);
                const ALMetalTexture* normal_texture = material_normal_map
                    ? batch.material_slots[source].normal_texture
                    : batch.diffuse_textures[0];
                const ALMetalTexture* specular_texture = material_specular_map
                    ? batch.material_slots[source].specular_texture
                    : batch.diffuse_textures[0];
                encoded =
                    encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(slot),
                        batch.diffuse_textures[source], error) &&
                    encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(8 + slot),
                        normal_texture, error) &&
                    encoder->setFragmentTexture(
                        static_cast<std::uint32_t>(16 + slot),
                        specular_texture, error);
            }
        }
        else
        {
            for (std::size_t slot = 0;
                 encoded && slot < batch.texture_count; ++slot)
            {
                encoded = encoder->setFragmentTexture(
                    static_cast<std::uint32_t>(slot),
                    batch.diffuse_textures[slot], error);
            }
        }
        encoded = encoded && encoder->drawIndexed(
            ALMetalPrimitiveType::triangle, batch.index_count,
            batch.index_type, *batch.indices, batch.index_offset, 1, error);
    }

    if (!encoded || !encoder->end(error))
    {
        return false;
    }
    if (scene.stamp_opaque_coverage)
    {
        auto coverage_pass = target.makeRenderPass(pass_config, error);
        if (!coverage_pass)
        {
            return false;
        }
        auto coverage_encoder = ALMetalRenderEncoder::begin(
            *command, *coverage_pass,
            "Alchemy world post-deferred coverage encoder", error);
        if (!coverage_encoder)
        {
            return false;
        }
        const std::uint32_t reverse_z = scene.reverse_z ? 1u : 0u;
        const ALMetalDepthStencilState* coverage_depth = scene.reverse_z
            ? mImpl->reverse_coverage_depth.get()
            : mImpl->forward_coverage_depth.get();
        const ALMetalRenderPipeline* coverage_pipeline =
            mImpl->coverage_pipelines[scene.hdr ? 1 : 0]
                                     [scene.normal_hdr ? 1 : 0].get();
        encoded =
            coverage_encoder->setDepthStencilState(coverage_depth, error) &&
            coverage_encoder->setViewport(viewport, error) &&
            coverage_encoder->setScissor(scissor, error) &&
            coverage_encoder->setCullMode(ALMetalCullMode::none, error) &&
            coverage_encoder->setPipeline(*coverage_pipeline, error) &&
            coverage_encoder->setVertexBytes(
                0, &reverse_z, sizeof(reverse_z), error) &&
            coverage_encoder->draw(
                ALMetalPrimitiveType::triangle, 0, 3, 1, error) &&
            coverage_encoder->end(error);
        if (!encoded)
        {
            return false;
        }
    }
    return command->commit(error);
}
