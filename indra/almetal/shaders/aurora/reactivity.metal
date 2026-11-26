// GENERATED from Aurora ee458fbf1d95e98c5b3a55c0027a523bb28d7e7f / pinned Naga 6c13013.
// FidelityFX SDK 1.1.4 / MIT: see tools/metal/aurora-evaluation/vendor/aurora/NOTICE.md.
// language: metal4.0
#include <metal_stdlib>
#include <simd/simd.h>

using metal::uint;

struct _mslBufferSizes {
    uint size7;
    uint size8;
};

struct FsrParams {
    metal::float4x4 motion_reproj;
    metal::int2 render_size;
    metal::int2 prev_render_size;
    metal::int2 upscale_size;
    metal::int2 prev_upscale_size;
    metal::int2 max_render_size;
    metal::int2 max_upscale_size;
    metal::float4 device_to_view;
    metal::float2 jitter;
    metal::float2 prev_jitter;
    metal::float2 downscale;
    float jitter_sequence_length;
    float delta_pre_exposure;
    float frame_index;
    float velocity_factor;
    float accumulation_added_per_frame;
    float min_disocclusion_accumulation;
    float exposure;
    float rcas;
    float view_to_meters;
    float motion_on;
    float reactive_on;
    float composition_on;
    float pad1_;
    float pad2_;
};
struct VsOut {
    metal::float4 clip;
};
struct Bilinear {
    metal::int2 base;
    char _pad1[8];
    metal::float4 weights;
};
struct type_10 {
    metal::int2 inner[4];
};
struct RectificationBox {
    metal::float3 center;
    metal::float3 vec_;
    metal::float3 aabb_min;
    metal::packed_float3 aabb_max;
    float center_weight;
};
typedef uint type_13[1];
typedef float type_14[1];
struct ReactivityOut {
    metal::float4 masks;
    metal::float4 accumulation;
};
struct type_15 {
    metal::int2 inner[9];
};
struct type_16 {
    float inner[9];
};
struct type_17 {
    uint inner[4];
};
constant float FP16_MIN = 0.000061;
constant float FP16_MAX = 65504.0;
constant float EPSILON = 0.000061;
constant float TONEMAP_EPSILON = 0.000061;
constant float FP32_MAX = 340282350000000000000000000000000000000.0;
constant float FP32_MIN = 0.000000000000000000000000000000000000011754944;
constant float RECONSTRUCTED_DEPTH_BILINEAR_WEIGHT_THRESHOLD = 0.00061;
constant float UPSAMPLE_LANCZOS_WEIGHT_SCALE = 0.0625;
constant float AVERAGE_LANCZOS_WEIGHT_PER_FRAME = 0.04625;
constant float LOCK_THRESHOLD = 1.0;
constant float LOCK_MAX = 2.0;
constant int REACTIVE = 0;
constant int DISOCCLUSION = 1;
constant int SHADING_CHANGE = 2;
constant int ACCUMULATION = 3;
constant type_10 BILINEAR_OFFSETS = type_10 {{metal::int2(0, 0), metal::int2(1, 0), metal::int2(0, 1), metal::int2(1, 1)}};

metal::int2 naga_f2i32(metal::float2 value) {
    return static_cast<metal::int2>(metal::clamp(value, -2147483600.0, 2147483500.0));
}

metal::int2 px_of(
    VsOut in_1
) {
    return naga_f2i32(in_1.clip.xy);
}

float sat(
    float x
) {
    return (x != x) ? 0.0 : metal::clamp(x, 0.0, 1.0);
}

metal::float3 sat3_(
    metal::float3 v
) {
    float _e2 = sat(v.x);
    float _e4 = sat(v.y);
    float _e6 = sat(v.z);
    return metal::float3(_e2, _e4, _e6);
}

metal::float2 render_size(
    constant FsrParams& fsr
) {
    metal::int2 _e2 = fsr.render_size;
    return static_cast<metal::float2>(_e2);
}

metal::int2 shading_change_render_size(
    constant FsrParams& fsr
) {
    metal::int2 _e2 = fsr.render_size;
    return naga_f2i32(static_cast<metal::float2>(_e2) * 0.5);
}

metal::int2 shading_change_max_render_size(
    constant FsrParams& fsr
) {
    metal::int2 _e2 = fsr.max_render_size;
    return naga_f2i32(static_cast<metal::float2>(_e2) * 0.5);
}

float reconstructed_depth_mv_px_threshold(
    float nearest_depth_in_meters
) {
    float _e3 = sat(nearest_depth_in_meters / 100.0);
    return metal::mix(0.25, 0.75, _e3);
}

float get_4k_velocity(
    metal::float2 mv
) {
    return metal::length(mv * metal::float2(3840.0, 2160.0));
}

float min_divided_by_max_or(
    float v0_,
    float v1_,
    float on_zero
) {
    float m_1 = metal::max(v0_, v1_);
    return (m_1 != 0.0) ? (metal::min(v0_, v1_) / m_1) : on_zero;
}

float min_divided_by_max(
    float v0_1,
    float v1_1
) {
    float _e3 = min_divided_by_max_or(v0_1, v1_1, 0.0);
    return _e3;
}

metal::float3 ycocg_to_rgb(
    metal::float3 c
) {
    return metal::float3((c.x + c.y) - c.z, c.x + c.z, (c.x - c.y) - c.z);
}

metal::float3 rgb_to_ycocg(
    metal::float3 c_1
) {
    return metal::float3(((0.25 * c_1.x) + (0.5 * c_1.y)) + (0.25 * c_1.z), (0.5 * c_1.x) - (0.5 * c_1.z), ((-0.25 * c_1.x) + (0.5 * c_1.y)) - (0.25 * c_1.z));
}

float rgb_to_luma(
    metal::float3 c_2
) {
    return metal::dot(c_2, metal::float3(0.2126, 0.7152, 0.0722));
}

metal::float3 tonemap(
    metal::float3 c_3
) {
    return c_3 / metal::float3(metal::max(metal::max(0.0, c_3.x), metal::max(c_3.y, c_3.z)) + 1.0);
}

metal::float3 inverse_tonemap(
    metal::float3 c_4
) {
    return c_4 / metal::float3(metal::max(TONEMAP_EPSILON, 1.0 - metal::max(c_4.x, metal::max(c_4.y, c_4.z))));
}

bool is_uv_inside(
    metal::float2 uv
) {
    bool local = {};
    bool local_1 = {};
    bool local_2 = {};
    if (uv.x >= 0.0) {
        local = uv.x <= 1.0;
    } else {
        local = false;
    }
    bool _e10 = local;
    if (_e10) {
        local_1 = uv.y >= 0.0;
    } else {
        local_1 = false;
    }
    bool _e17 = local_1;
    if (_e17) {
        local_2 = uv.y <= 1.0;
    } else {
        local_2 = false;
    }
    bool _e24 = local_2;
    return _e24;
}

metal::int2 clamp_load(
    metal::int2 p,
    metal::int2 offset,
    metal::int2 size
) {
    return metal::clamp(as_type<metal::int2>(as_type<metal::uint2>(p) + as_type<metal::uint2>(offset)), metal::int2(0), as_type<metal::int2>(as_type<metal::uint2>(size) - as_type<metal::uint2>(metal::int2(1))));
}

metal::float2 clamp_uv(
    metal::float2 uv_1,
    metal::int2 texture_size,
    metal::int2 resource_size
) {
    metal::float2 location = uv_1 * static_cast<metal::float2>(texture_size);
    metal::float2 clamped = metal::max(metal::float2(0.5), metal::min(location, static_cast<metal::float2>(texture_size) - metal::float2(0.5)));
    return clamped / static_cast<metal::float2>(resource_size);
}

bool is_on_screen(
    metal::int2 p_1,
    metal::int2 size_1
) {
    return metal::all(static_cast<metal::uint2>(p_1) < static_cast<metal::uint2>(size_1));
}

metal::int2 compute_hr_pos_from_lr_pos(
    metal::int2 lr,
    constant FsrParams& fsr
) {
    metal::float2 _e7 = fsr.jitter;
    metal::float2 jittered = (static_cast<metal::float2>(lr) + metal::float2(0.5)) - _e7;
    metal::float2 _e9 = render_size(fsr);
    metal::int2 _e13 = fsr.upscale_size;
    metal::float2 in_hr = (jittered / _e9) * static_cast<metal::float2>(_e13);
    return naga_f2i32(metal::floor(in_hr));
}

float view_space_depth(
    float device_depth,
    constant FsrParams& fsr
) {
    float _e4 = fsr.device_to_view.y;
    float _e8 = fsr.device_to_view.x;
    return _e4 / (device_depth - _e8);
}

float view_space_depth_in_meters(
    float device_depth_1,
    constant FsrParams& fsr
) {
    float _e1 = view_space_depth(device_depth_1, fsr);
    float _e4 = fsr.view_to_meters;
    return _e1 * _e4;
}

Bilinear bilinear_data(
    metal::float2 uv_2,
    metal::int2 size_2
) {
    Bilinear b = {};
    metal::float2 p_8 = (uv_2 * static_cast<metal::float2>(size_2)) - metal::float2(0.5);
    metal::float2 f = metal::fract(p_8);
    b.base = naga_f2i32(metal::floor(p_8));
    b.weights = metal::float4((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);
    Bilinear _e34 = b;
    return _e34;
}

float lanczos2_no_clamp(
    float x_1
) {
    if (metal::abs(x_1) < EPSILON) {
        return 1.0;
    }
    return (metal::sin(3.1415927 * x_1) / (3.1415927 * x_1)) * (metal::sin((0.5 * 3.1415927) * x_1) / ((0.5 * 3.1415927) * x_1));
}

float lanczos2_(
    float x_2
) {
    float _e4 = lanczos2_no_clamp(metal::min(metal::abs(x_2), 2.0));
    return _e4;
}

float lanczos2_approx_sq(
    float x2_in
) {
    float x2_ = metal::min(x2_in, 4.0);
    float a = (0.4 * x2_) - 1.0;
    float b_3 = (0.25 * x2_) - 1.0;
    return (((1.5625 * a) * a) - 0.5625) * (b_3 * b_3);
}

void box_add_sample(
    bool initial,
    thread RectificationBox& b_1,
    metal::float3 c_5,
    float w
) {
    metal::float3 weighted = c_5 * w;
    if (initial) {
        b_1.aabb_min = c_5;
        b_1.aabb_max = c_5;
        b_1.center = weighted;
        b_1.vec_ = c_5 * weighted;
        b_1.center_weight = w;
        return;
    } else {
        metal::float3 _e13 = b_1.aabb_min;
        b_1.aabb_min = metal::min(_e13, c_5);
        metal::float3 _e17 = b_1.aabb_max;
        b_1.aabb_max = metal::max(_e17, c_5);
        metal::float3 _e20 = b_1.center;
        b_1.center = _e20 + weighted;
        metal::float3 _e23 = b_1.vec_;
        b_1.vec_ = _e23 + (c_5 * weighted);
        float _e27 = b_1.center_weight;
        b_1.center_weight = _e27 + w;
        return;
    }
}

void box_compute_variance(
    thread RectificationBox& b_2
) {
    float _e2 = b_2.center_weight;
    float _e4 = b_2.center_weight;
    float w_1 = (metal::abs(_e4) > FP32_MIN) ? _e2 : 1.0;
    b_2.center_weight = w_1;
    metal::float3 _e12 = b_2.center;
    b_2.center = _e12 / metal::float3(w_1);
    metal::float3 _e16 = b_2.vec_;
    b_2.vec_ = _e16 / metal::float3(w_1);
    metal::float3 _e21 = b_2.vec_;
    metal::float3 _e23 = b_2.center;
    metal::float3 _e25 = b_2.center;
    b_2.vec_ = metal::sqrt(metal::abs(_e21 - (_e23 * _e25)));
    return;
}

metal::float3 safe_color(
    metal::float3 c_6
) {
    return metal::select(metal::min(c_6, metal::float3(65504.0)), metal::float3(0.0), c_6 != c_6);
}

float load_dilated_depth_or_zero(
    metal::int2 p_2,
    metal::texture2d<float, metal::access::sample> dilated_depth_tex
) {
    bool _e4 = is_on_screen(p_2, static_cast<metal::int2>(metal::uint2(dilated_depth_tex.get_width(), dilated_depth_tex.get_height())));
    if (!(_e4)) {
        return 0.0;
    }
    uint clamped_lod_e9 = metal::min(uint(0), dilated_depth_tex.get_num_mip_levels() - 1);
    metal::float4 _e9 = dilated_depth_tex.read(metal::min(metal::uint2(p_2), metal::uint2(dilated_depth_tex.get_width(clamped_lod_e9), dilated_depth_tex.get_height(clamped_lod_e9)) - 1), clamped_lod_e9);
    return _e9.x;
}

metal::float2 load_dilated_motion_or_zero(
    metal::int2 p_3,
    metal::texture2d<float, metal::access::sample> dilated_motion_tex
) {
    bool _e4 = is_on_screen(p_3, static_cast<metal::int2>(metal::uint2(dilated_motion_tex.get_width(), dilated_motion_tex.get_height())));
    if (!(_e4)) {
        return metal::float2(0.0);
    }
    uint clamped_lod_e10 = metal::min(uint(0), dilated_motion_tex.get_num_mip_levels() - 1);
    metal::float4 _e10 = dilated_motion_tex.read(metal::min(metal::uint2(p_3), metal::uint2(dilated_motion_tex.get_width(clamped_lod_e10), dilated_motion_tex.get_height(clamped_lod_e10)) - 1), clamped_lod_e10);
    return _e10.xy;
}

float compute_disocclusions(
    metal::float2 uv_3,
    metal::float2 mv_in,
    float current_depth_view,
    constant FsrParams& fsr,
    device type_13 const& reconstructed,
    constant _mslBufferSizes& _buffer_sizes
) {
    type_10 offsets = BILINEAR_OFFSETS;
    float disocclusion = 0.0;
    float weight_sum = 0.0;
    bool potential = true;
    int i = 0;
    bool local_3 = {};
    bool local_4 = {};
    bool local_5 = {};
    float _e5 = fsr.view_to_meters;
    float nearest_m = metal::min(current_depth_view * _e5, FP16_MAX);
    float _e9 = reconstructed_depth_mv_px_threshold(nearest_m);
    float _e10 = get_4k_velocity(mv_in);
    metal::float2 mv_3 = mv_in * static_cast<float>(_e10 > _e9);
    metal::int2 _e17 = fsr.render_size;
    Bilinear _e18 = bilinear_data(uv_3 + mv_3, _e17);
    uint2 loop_bound = uint2(4294967295u);
    bool loop_init = true;
    while(true) {
        if (metal::all(loop_bound == uint2(0u))) { break; }
        loop_bound -= uint2(loop_bound.y == 0u, 1u);
        if (!loop_init) {
            int _e88 = i;
            i = as_type<int>(as_type<uint>(_e88) + as_type<uint>(1));
        }
        loop_init = false;
        int _e29 = i;
        if (_e29 < 4) {
            bool _e34 = potential;
            local_3 = _e34;
        } else {
            local_3 = false;
        }
        bool _e36 = local_3;
        if (_e36) {
        } else {
            break;
        }
        {
            int _e38 = i;
            metal::int2 _e40 = offsets.inner[metal::min(unsigned(_e38), 3u)];
            metal::int2 _e43 = fsr.render_size;
            metal::int2 _e44 = clamp_load(_e18.base, _e40, _e43);
            int _e46 = i;
            float w_2 = _e18.weights[metal::min(unsigned(_e46), 3u)];
            if (w_2 > RECONSTRUCTED_DEPTH_BILINEAR_WEIGHT_THRESHOLD) {
                int _e55 = fsr.render_size.x;
                uint _e60 = reconstructed[metal::min(unsigned(as_type<int>(as_type<uint>(as_type<int>(as_type<uint>(_e44.y) * as_type<uint>(_e55))) + as_type<uint>(_e44.x))), (_buffer_sizes.size7 - 0 - 4) / 4)];
                float prev_depth = as_type<float>(_e60);
                float _e62 = view_space_depth(prev_depth, fsr);
                float difference = current_depth_view - _e62;
                bool _e64 = potential;
                if (_e64) {
                    local_4 = difference > FP32_MIN;
                } else {
                    local_4 = false;
                }
                bool _e70 = local_4;
                potential = _e70;
                bool _e71 = potential;
                if (_e71) {
                    metal::float2 _e72 = render_size(fsr);
                    float half_viewport = metal::length(_e72 * 0.5);
                    float depth_threshold = metal::max(current_depth_view, _e62);
                    float separation = (0.0000137 * half_viewport) * depth_threshold;
                    float _e80 = disocclusion;
                    float _e82 = sat(separation / difference);
                    disocclusion = _e80 + (_e82 * w_2);
                    float _e85 = weight_sum;
                    weight_sum = _e85 + w_2;
                }
            }
        }
    }
    float _e90 = disocclusion;
    float _e91 = weight_sum;
    float _e95 = sat(1.0 - (_e90 / _e91));
    bool _e96 = potential;
    if (_e96) {
        float _e99 = weight_sum;
        local_5 = _e99 > 0.0;
    } else {
        local_5 = false;
    }
    bool _e103 = local_5;
    return _e103 ? _e95 : 0.0;
}

float compute_motion_divergence(
    metal::float2 uv_4,
    metal::float2 mv_1,
    float current_depth,
    constant FsrParams& fsr,
    metal::texture2d<float, metal::access::sample> dilated_motion_tex,
    metal::texture2d<float, metal::access::sample> dilated_depth_tex
) {
    metal::float2 _e4 = render_size(fsr);
    metal::int2 q = naga_f2i32((uv_4 + mv_1) * _e4);
    float _e7 = load_dilated_depth_or_zero(q, dilated_depth_tex);
    metal::float2 _e8 = load_dilated_motion_or_zero(q, dilated_motion_tex);
    float _e9 = get_4k_velocity(_e8);
    float _e10 = get_4k_velocity(mv_1);
    float _e11 = view_space_depth_in_meters(_e7, fsr);
    float _e12 = view_space_depth_in_meters(current_depth, fsr);
    float _e13 = min_divided_by_max(_e11, _e12);
    float _e16 = sat(_e10 / 10.0);
    float _e18 = sat(_e9 / _e10);
    return ((1.0 - _e18) * _e13) * _e16;
}

float thin_feature_confidence(
    metal::int2 p_4,
    constant FsrParams& fsr,
    metal::texture2d<float, metal::access::sample> luma_tex
) {
    type_15 offsets_1 = type_15 {{metal::int2(0, 0), metal::int2(-1, -1), metal::int2(0, -1), metal::int2(1, -1), metal::int2(-1, 0), metal::int2(1, 0), metal::int2(-1, 1), metal::int2(0, 1), metal::int2(1, 1)}};
    type_16 samples = {};
    float luma_min = FP32_MAX;
    float luma_max = FP32_MIN;
    int i_1 = 0;
    float dissimilar_min = FP32_MAX;
    float dissimilar_max = 0.0;
    uint pattern = 1u;
    type_17 rejection = type_17 {{23u, 45u, 209u, 417u}};
    int i_2 = 1;
    bool local_6 = {};
    int i_3 = 0;
    uint2 loop_bound_1 = uint2(4294967295u);
    bool loop_init_1 = true;
    while(true) {
        if (metal::all(loop_bound_1 == uint2(0u))) { break; }
        loop_bound_1 -= uint2(loop_bound_1.y == 0u, 1u);
        if (!loop_init_1) {
            int _e68 = i_1;
            i_1 = as_type<int>(as_type<uint>(_e68) + as_type<uint>(1));
        }
        loop_init_1 = false;
        int _e37 = i_1;
        if (_e37 < 9) {
        } else {
            break;
        }
        {
            int _e40 = i_1;
            int _e43 = i_1;
            metal::int2 _e45 = offsets_1.inner[metal::min(unsigned(_e43), 8u)];
            metal::int2 _e48 = fsr.render_size;
            metal::int2 _e49 = clamp_load(p_4, _e45, _e48);
            uint clamped_lod_e51 = metal::min(uint(0), luma_tex.get_num_mip_levels() - 1);
            metal::float4 _e51 = luma_tex.read(metal::min(metal::uint2(_e49), metal::uint2(luma_tex.get_width(clamped_lod_e51), luma_tex.get_height(clamped_lod_e51)) - 1), clamped_lod_e51);
            float _e55 = fsr.exposure;
            samples.inner[metal::min(unsigned(_e40), 8u)] = _e51.x * _e55;
            float _e57 = luma_min;
            int _e58 = i_1;
            float _e60 = samples.inner[metal::min(unsigned(_e58), 8u)];
            luma_min = metal::min(_e57, _e60);
            float _e62 = luma_max;
            int _e63 = i_1;
            float _e65 = samples.inner[metal::min(unsigned(_e63), 8u)];
            luma_max = metal::max(_e62, _e65);
        }
    }
    uint2 loop_bound_2 = uint2(4294967295u);
    bool loop_init_2 = true;
    while(true) {
        if (metal::all(loop_bound_2 == uint2(0u))) { break; }
        loop_bound_2 -= uint2(loop_bound_2.y == 0u, 1u);
        if (!loop_init_2) {
            int _e117 = i_2;
            i_2 = as_type<int>(as_type<uint>(_e117) + as_type<uint>(1));
        }
        loop_init_2 = false;
        int _e85 = i_2;
        if (_e85 < 9) {
        } else {
            break;
        }
        {
            int _e88 = i_2;
            float _e90 = samples.inner[metal::min(unsigned(_e88), 8u)];
            float _e92 = samples.inner[0];
            float _e95 = luma_max;
            float _e96 = luma_min;
            float difference_1 = metal::abs(_e90 - _e92) / (_e95 - _e96);
            if (difference_1 < 0.9) {
                uint _e100 = pattern;
                int _e102 = i_2;
                pattern = _e100 | (1u << static_cast<uint>(_e102));
            } else {
                float _e106 = dissimilar_min;
                int _e107 = i_2;
                float _e109 = samples.inner[metal::min(unsigned(_e107), 8u)];
                dissimilar_min = metal::min(_e106, _e109);
                float _e111 = dissimilar_max;
                int _e112 = i_2;
                float _e114 = samples.inner[metal::min(unsigned(_e112), 8u)];
                dissimilar_max = metal::max(_e111, _e114);
            }
        }
    }
    float _e120 = samples.inner[0];
    float _e121 = dissimilar_max;
    if (!((_e120 > _e121))) {
        float _e127 = samples.inner[0];
        float _e128 = dissimilar_min;
        local_6 = _e127 < _e128;
    } else {
        local_6 = true;
    }
    bool ridge = local_6;
    if (!(ridge)) {
        return 0.0;
    }
    uint2 loop_bound_3 = uint2(4294967295u);
    bool loop_init_3 = true;
    while(true) {
        if (metal::all(loop_bound_3 == uint2(0u))) { break; }
        loop_bound_3 -= uint2(loop_bound_3.y == 0u, 1u);
        if (!loop_init_3) {
            int _e150 = i_3;
            i_3 = as_type<int>(as_type<uint>(_e150) + as_type<uint>(1));
        }
        loop_init_3 = false;
        int _e136 = i_3;
        if (_e136 < 4) {
        } else {
            break;
        }
        {
            uint _e139 = pattern;
            int _e140 = i_3;
            uint _e142 = rejection.inner[metal::min(unsigned(_e140), 3u)];
            int _e144 = i_3;
            uint _e146 = rejection.inner[metal::min(unsigned(_e144), 3u)];
            if ((_e139 & _e142) == _e146) {
                return 0.0;
            }
        }
    }
    float _e152 = luma_min;
    float _e153 = luma_max;
    return 1.0 - (_e152 / _e153);
}

metal::float2 update_accumulation(
    metal::int2 p_5,
    metal::float2 uv_5,
    metal::float2 mv_2,
    float disocclusion_1,
    float shading_change,
    constant FsrParams& fsr,
    metal::sampler lin,
    metal::texture2d<float, metal::access::sample> prev_accumulation_tex
) {
    float accumulation = 0.0;
    metal::float2 reprojected = uv_5 + mv_2;
    bool _e8 = is_uv_inside(reprojected);
    if (_e8) {
        metal::int2 _e11 = fsr.prev_render_size;
        metal::int2 _e14 = fsr.max_render_size;
        metal::float2 _e15 = clamp_uv(reprojected, _e11, _e14);
        metal::float4 _e19 = prev_accumulation_tex.sample(lin, _e15, metal::level(0.0));
        float _e21 = sat(_e19.x);
        accumulation = _e21;
    }
    float _e22 = accumulation;
    accumulation = metal::mix(_e22, 0.0, shading_change);
    float _e25 = accumulation;
    float _e28 = fsr.min_disocclusion_accumulation;
    float _e29 = accumulation;
    accumulation = metal::mix(_e25, metal::min(_e28, _e29), disocclusion_1);
    float _e32 = accumulation;
    float _e33 = accumulation;
    accumulation = _e32 * static_cast<float>(metal::rint(_e33 * 100.0) > 1.0);
    float _e41 = accumulation;
    float _e42 = accumulation;
    float _e45 = fsr.accumulation_added_per_frame;
    float _e47 = sat(_e42 + _e45);
    return metal::float2(_e41, _e47);
}

float dilate_reactive_masks(
    metal::int2 p_6,
    constant FsrParams& fsr,
    metal::texture2d<float, metal::access::sample> reactive_tex
) {
    float m = 0.0;
    int y = -1;
    int x_3 = {};
    uint2 loop_bound_4 = uint2(4294967295u);
    bool loop_init_4 = true;
    while(true) {
        if (metal::all(loop_bound_4 == uint2(0u))) { break; }
        loop_bound_4 -= uint2(loop_bound_4.y == 0u, 1u);
        if (!loop_init_4) {
            int _e30 = y;
            y = as_type<int>(as_type<uint>(_e30) + as_type<uint>(1));
        }
        loop_init_4 = false;
        int _e5 = y;
        if (_e5 <= 1) {
        } else {
            break;
        }
        {
            x_3 = -1;
            uint2 loop_bound_5 = uint2(4294967295u);
            bool loop_init_5 = true;
            while(true) {
                if (metal::all(loop_bound_5 == uint2(0u))) { break; }
                loop_bound_5 -= uint2(loop_bound_5.y == 0u, 1u);
                if (!loop_init_5) {
                    int _e27 = x_3;
                    x_3 = as_type<int>(as_type<uint>(_e27) + as_type<uint>(1));
                }
                loop_init_5 = false;
                int _e10 = x_3;
                if (_e10 <= 1) {
                } else {
                    break;
                }
                {
                    float _e13 = m;
                    int _e15 = x_3;
                    int _e16 = y;
                    metal::int2 _e20 = fsr.render_size;
                    metal::int2 _e21 = clamp_load(p_6, metal::int2(_e15, _e16), _e20);
                    uint clamped_lod_e23 = metal::min(uint(0), reactive_tex.get_num_mip_levels() - 1);
                    metal::float4 _e23 = reactive_tex.read(metal::min(metal::uint2(_e21), metal::uint2(reactive_tex.get_width(clamped_lod_e23), reactive_tex.get_height(clamped_lod_e23)) - 1), clamped_lod_e23);
                    m = metal::max(_e13, _e23.x);
                }
            }
        }
    }
    float _e32 = m;
    return _e32;
}

float compute_shading_change(
    metal::float2 uv_6,
    constant FsrParams& fsr,
    metal::sampler lin,
    metal::texture2d<float, metal::access::sample> shading_change_tex
) {
    metal::float2 _e3 = fsr.jitter;
    metal::float2 _e4 = render_size(fsr);
    metal::int2 _e7 = shading_change_render_size(fsr);
    metal::int2 _e8 = shading_change_max_render_size(fsr);
    metal::float2 _e9 = clamp_uv(uv_6 - (_e3 / _e4), _e7, _e8);
    metal::float4 _e13 = shading_change_tex.sample(lin, _e9, metal::level(0.0));
    float _e15 = sat(_e13.x);
    return _e15;
}

float composition_mask(
    metal::int2 p_7,
    constant FsrParams& fsr,
    metal::texture2d<float, metal::access::sample> normal_tex
) {
    metal::int2 _e6 = fsr.render_size;
    metal::int2 _e7 = clamp_load(p_7, metal::int2(0), _e6);
    uint clamped_lod_e9 = metal::min(uint(0), normal_tex.get_num_mip_levels() - 1);
    metal::float4 _e9 = normal_tex.read(metal::min(metal::uint2(_e7), metal::uint2(normal_tex.get_width(clamped_lod_e9), normal_tex.get_height(clamped_lod_e9)) - 1), clamped_lod_e9);
    float x_4 = _e9.x;
    float _e13 = fsr.composition_on;
    return (x_4 > 1.5) ? _e13 : 0.0;
}

struct vs_fsrInput {
};
struct vs_fsrOutput {
    metal::float4 clip [[position]];
};
vertex vs_fsrOutput vs_fsr(
  uint vi [[vertex_id]]
) {
    VsOut o = {};
    float x_5 = static_cast<float>((vi << 1u) & 2u);
    float y_1 = static_cast<float>(vi & 2u);
    o.clip = metal::float4((x_5 * 2.0) - 1.0, (y_1 * 2.0) - 1.0, 0.0, 1.0);
    VsOut _e22 = o;
    const auto _tmp = _e22;
    return vs_fsrOutput { _tmp.clip };
}


struct fs_prepare_reactivityInput {
};
struct fs_prepare_reactivityOutput {
    metal::float4 masks [[color(0)]];
    metal::float4 accumulation [[color(1)]];
};
fragment fs_prepare_reactivityOutput fs_prepare_reactivity(
  metal::float4 clip [[position]]
, constant FsrParams& fsr [[buffer(0)]]
, metal::sampler lin [[sampler(0)]]
, metal::texture2d<float, metal::access::sample> dilated_motion_tex [[texture(2)]]
, metal::texture2d<float, metal::access::sample> dilated_depth_tex [[texture(3)]]
, metal::texture2d<float, metal::access::sample> shading_change_tex [[texture(4)]]
, metal::texture2d<float, metal::access::sample> prev_accumulation_tex [[texture(5)]]
, metal::texture2d<float, metal::access::sample> luma_tex [[texture(6)]]
, device type_13 const& reconstructed [[buffer(7)]]
, device type_14& new_locks [[buffer(8)]]
, metal::texture2d<float, metal::access::sample> reactive_tex [[texture(9)]]
, metal::texture2d<float, metal::access::sample> normal_tex [[texture(10)]]
, constant _mslBufferSizes& _buffer_sizes [[buffer(30)]]
) {
    const VsOut in = { clip };
    ReactivityOut o_1 = {};
    metal::int2 _e1 = px_of(in);
    metal::float2 _e6 = render_size(fsr);
    metal::float2 uv_7 = (static_cast<metal::float2>(_e1) + metal::float2(0.5)) / _e6;
    uint clamped_lod_e10 = metal::min(uint(0), dilated_motion_tex.get_num_mip_levels() - 1);
    metal::float4 _e10 = dilated_motion_tex.read(metal::min(metal::uint2(_e1), metal::uint2(dilated_motion_tex.get_width(clamped_lod_e10), dilated_motion_tex.get_height(clamped_lod_e10)) - 1), clamped_lod_e10);
    metal::float2 mv_4 = _e10.xy;
    uint clamped_lod_e14 = metal::min(uint(0), dilated_depth_tex.get_num_mip_levels() - 1);
    metal::float4 _e14 = dilated_depth_tex.read(metal::min(metal::uint2(_e1), metal::uint2(dilated_depth_tex.get_width(clamped_lod_e14), dilated_depth_tex.get_height(clamped_lod_e14)) - 1), clamped_lod_e14);
    float depth = _e14.x;
    float _e16 = view_space_depth(depth, fsr);
    float _e17 = compute_disocclusions(uv_7, mv_4, _e16, fsr, reconstructed, _buffer_sizes);
    float _e18 = dilate_reactive_masks(_e1, fsr, reactive_tex);
    float _e19 = compute_shading_change(uv_7, fsr, lin, shading_change_tex);
    float shading_change_1 = metal::max(_e18, _e19);
    float _e21 = compute_motion_divergence(uv_7, mv_4, depth, fsr, dilated_motion_tex, dilated_depth_tex);
    float _e22 = composition_mask(_e1, fsr, normal_tex);
    float reactive = metal::max(_e21, _e22);
    metal::float2 _e24 = update_accumulation(_e1, uv_7, mv_4, _e17, shading_change_1, fsr, lin, prev_accumulation_tex);
    float _e25 = thin_feature_confidence(_e1, fsr, luma_tex);
    if (_e25 > 0.01) {
        metal::int2 _e28 = compute_hr_pos_from_lr_pos(_e1, fsr);
        metal::int2 _e31 = fsr.upscale_size;
        bool _e32 = is_on_screen(_e28, _e31);
        if (_e32) {
            int _e38 = fsr.upscale_size.x;
            new_locks[metal::min(unsigned(as_type<int>(as_type<uint>(as_type<int>(as_type<uint>(_e28.y) * as_type<uint>(_e38))) + as_type<uint>(_e28.x))), (_buffer_sizes.size8 - 0 - 4) / 4)] = _e25;
        }
    }
    o_1.masks = metal::float4(reactive, _e17, shading_change_1, _e24.x);
    o_1.accumulation = metal::float4(_e24.y, 0.0, 0.0, 0.0);
    ReactivityOut _e53 = o_1;
    const auto _tmp_1 = _e53;
    return fs_prepare_reactivityOutput { _tmp_1.masks, _tmp_1.accumulation };
}
