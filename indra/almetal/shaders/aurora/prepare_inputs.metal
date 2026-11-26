// GENERATED from Aurora ee458fbf1d95e98c5b3a55c0027a523bb28d7e7f / pinned Naga 6c13013.
// FidelityFX SDK 1.1.4 / MIT: see tools/metal/aurora-evaluation/vendor/aurora/NOTICE.md.
// language: metal4.0
#include <metal_stdlib>
#include <simd/simd.h>

using metal::uint;

struct _mslBufferSizes {
    uint size5;
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
typedef metal::atomic_uint type_15[1];
struct PrepareOut {
    metal::float4 dilated_motion;
    metal::float4 dilated_depth;
    metal::float4 farthest_depth;
    metal::float4 luma;
};
struct type_16 {
    metal::int2 inner[9];
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
    VsOut in_2
) {
    return naga_f2i32(in_2.clip.xy);
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
    float m = metal::max(v0_, v1_);
    return (m != 0.0) ? (metal::min(v0_, v1_) / m) : on_zero;
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
    metal::float2 p_4 = (uv_2 * static_cast<metal::float2>(size_2)) - metal::float2(0.5);
    metal::float2 f = metal::fract(p_4);
    b.base = naga_f2i32(metal::floor(p_4));
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

float load_depth(
    metal::int2 p_2,
    constant FsrParams& fsr,
    metal::depth2d<float, metal::access::sample> depth_tex
) {
    metal::int2 _e6 = fsr.render_size;
    metal::int2 _e7 = clamp_load(p_2, metal::int2(0), _e6);
    uint clamped_lod_e9 = metal::min(uint(0), depth_tex.get_num_mip_levels() - 1);
    float _e9 = depth_tex.read(metal::min(metal::uint2(_e7), metal::uint2(depth_tex.get_width(clamped_lod_e9), depth_tex.get_height(clamped_lod_e9)) - 1), clamped_lod_e9);
    return _e9;
}

metal::float2 camera_motion(
    metal::float4x4 reproj,
    metal::float2 uv_3,
    float depth,
    metal::float2 jitter_uv
) {
    metal::float2 cur_uv = uv_3 - jitter_uv;
    metal::float2 ndc = metal::float2((cur_uv.x * 2.0) - 1.0, 1.0 - (cur_uv.y * 2.0));
    metal::float4 pp = reproj * metal::float4(ndc, metal::max(depth, 0.0000001), 1.0);
    if (pp.w <= 0.0) {
        return metal::float2(2.0);
    }
    metal::float2 prev_uv = metal::float2(((pp.x / pp.w) * 0.5) + 0.5, 0.5 - ((pp.y / pp.w) * 0.5));
    return prev_uv - cur_uv;
}

metal::float2 load_input_motion_vector(
    metal::int2 p_in,
    constant FsrParams& fsr,
    metal::depth2d<float, metal::access::sample> depth_tex,
    metal::texture2d<float, metal::access::sample> motion_tex
) {
    bool local_3 = {};
    bool local_4 = {};
    metal::int2 _e5 = fsr.render_size;
    metal::int2 _e6 = clamp_load(p_in, metal::int2(0), _e5);
    uint clamped_lod_e9 = metal::min(uint(0), motion_tex.get_num_mip_levels() - 1);
    metal::float4 _e9 = motion_tex.read(metal::min(metal::uint2(_e6), metal::uint2(motion_tex.get_width(clamped_lod_e9), motion_tex.get_height(clamped_lod_e9)) - 1), clamped_lod_e9);
    metal::float2 motion = _e9.zw;
    uint clamped_lod_e13 = metal::min(uint(0), depth_tex.get_num_mip_levels() - 1);
    float depth_2 = depth_tex.read(metal::min(metal::uint2(_e6), metal::uint2(depth_tex.get_width(clamped_lod_e13), depth_tex.get_height(clamped_lod_e13)) - 1), clamped_lod_e13);
    float _e16 = fsr.motion_on;
    if (_e16 > 0.5) {
        local_3 = depth_2 > 0.0;
    } else {
        local_3 = false;
    }
    bool _e24 = local_3;
    if (_e24) {
        local_4 = metal::any(motion != metal::float2(0.0));
    } else {
        local_4 = false;
    }
    bool _e32 = local_4;
    if (_e32) {
        return -(motion);
    }
    metal::float2 _e38 = render_size(fsr);
    metal::float2 uv_4 = (static_cast<metal::float2>(_e6) + metal::float2(0.5)) / _e38;
    metal::float4x4 _e42 = fsr.motion_reproj;
    metal::float2 _e45 = fsr.jitter;
    metal::float2 _e46 = render_size(fsr);
    metal::float2 _e48 = camera_motion(_e42, uv_4, depth_2, _e45 / _e46);
    return _e48;
}

void reconstruct_prev_depth(
    metal::int2 p_3,
    float depth_1,
    metal::float2 mv_in,
    constant FsrParams& fsr,
    device type_15& reconstructed,
    constant _mslBufferSizes& _buffer_sizes
) {
    type_10 offsets_1 = BILINEAR_OFFSETS;
    int i_1 = 0;
    float _e3 = view_space_depth_in_meters(depth_1, fsr);
    float nearest_m = metal::min(_e3, FP16_MAX);
    float _e6 = reconstructed_depth_mv_px_threshold(nearest_m);
    float _e7 = get_4k_velocity(mv_in);
    metal::float2 mv_1 = mv_in * static_cast<float>(_e7 > _e6);
    metal::float2 _e15 = render_size(fsr);
    metal::float2 uv_5 = (static_cast<metal::float2>(p_3) + metal::float2(0.5)) / _e15;
    metal::int2 _e20 = fsr.render_size;
    Bilinear _e21 = bilinear_data(uv_5 + mv_1, _e20);
    uint2 loop_bound = uint2(4294967295u);
    bool loop_init = true;
    while(true) {
        if (metal::all(loop_bound == uint2(0u))) { break; }
        loop_bound -= uint2(loop_bound.y == 0u, 1u);
        if (!loop_init) {
            int _e56 = i_1;
            i_1 = as_type<int>(as_type<uint>(_e56) + as_type<uint>(1));
        }
        loop_init = false;
        int _e26 = i_1;
        if (_e26 < 4) {
        } else {
            break;
        }
        {
            int _e30 = i_1;
            if (_e21.weights[metal::min(unsigned(_e30), 3u)] > RECONSTRUCTED_DEPTH_BILINEAR_WEIGHT_THRESHOLD) {
                int _e35 = i_1;
                metal::int2 _e37 = offsets_1.inner[metal::min(unsigned(_e35), 3u)];
                metal::int2 q = as_type<metal::int2>(as_type<metal::uint2>(_e21.base) + as_type<metal::uint2>(_e37));
                metal::int2 _e41 = fsr.render_size;
                bool _e42 = is_on_screen(q, _e41);
                if (_e42) {
                    int _e48 = fsr.render_size.x;
                    uint _e54 = metal::atomic_fetch_max_explicit(&reconstructed[metal::min(unsigned(as_type<int>(as_type<uint>(as_type<int>(as_type<uint>(q.y) * as_type<uint>(_e48))) + as_type<uint>(q.x))), (_buffer_sizes.size5 - 0 - 4) / 4)], as_type<uint>(depth_1), metal::memory_order_relaxed);
                }
            }
        }
    }
    return;
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
    float x_3 = static_cast<float>((vi << 1u) & 2u);
    float y = static_cast<float>(vi & 2u);
    o.clip = metal::float4((x_3 * 2.0) - 1.0, (y * 2.0) - 1.0, 0.0, 1.0);
    VsOut _e22 = o;
    const auto _tmp = _e22;
    return vs_fsrOutput { _tmp.clip };
}


struct fs_prepare_inputsInput {
};
struct fs_prepare_inputsOutput {
    metal::float4 dilated_motion [[color(0)]];
    metal::float4 dilated_depth [[color(1)]];
    metal::float4 farthest_depth [[color(2)]];
    metal::float4 luma [[color(3)]];
};
fragment fs_prepare_inputsOutput fs_prepare_inputs(
  metal::float4 clip [[position]]
, constant FsrParams& fsr [[buffer(0)]]
, metal::texture2d<float, metal::access::sample> color_tex [[texture(2)]]
, metal::depth2d<float, metal::access::sample> depth_tex [[texture(3)]]
, metal::texture2d<float, metal::access::sample> motion_tex [[texture(4)]]
, device type_15& reconstructed [[buffer(5)]]
, constant _mslBufferSizes& _buffer_sizes [[buffer(30)]]
) {
    const VsOut in = { clip };
    type_16 offsets = type_16 {{metal::int2(0, 0), metal::int2(1, 0), metal::int2(0, 1), metal::int2(0, -1), metal::int2(-1, 0), metal::int2(-1, 1), metal::int2(1, 1), metal::int2(-1, -1), metal::int2(1, -1)}};
    float nearest = {};
    metal::int2 nearest_at = {};
    float farthest = {};
    int i = 1;
    PrepareOut o_1 = {};
    metal::int2 _e1 = px_of(in);
    float _e31 = load_depth(_e1, fsr, depth_tex);
    nearest = _e31;
    nearest_at = _e1;
    float _e34 = nearest;
    farthest = _e34;
    uint2 loop_bound_1 = uint2(4294967295u);
    bool loop_init_1 = true;
    while(true) {
        if (metal::all(loop_bound_1 == uint2(0u))) { break; }
        loop_bound_1 -= uint2(loop_bound_1.y == 0u, 1u);
        if (!loop_init_1) {
            int _e55 = i;
            i = as_type<int>(as_type<uint>(_e55) + as_type<uint>(1));
        }
        loop_init_1 = false;
        int _e38 = i;
        if (_e38 < 9) {
        } else {
            break;
        }
        {
            int _e41 = i;
            metal::int2 _e43 = offsets.inner[metal::min(unsigned(_e41), 8u)];
            metal::int2 q_1 = as_type<metal::int2>(as_type<metal::uint2>(_e1) + as_type<metal::uint2>(_e43));
            metal::int2 _e47 = fsr.render_size;
            bool _e48 = is_on_screen(q_1, _e47);
            if (_e48) {
                float _e49 = load_depth(q_1, fsr, depth_tex);
                float _e50 = nearest;
                if (_e49 > _e50) {
                    float _e52 = farthest;
                    farthest = metal::min(_e52, _e49);
                    nearest_at = q_1;
                    nearest = _e49;
                }
            }
        }
    }
    metal::int2 _e57 = nearest_at;
    metal::float2 _e58 = load_input_motion_vector(_e57, fsr, depth_tex, motion_tex);
    float _e59 = nearest;
    reconstruct_prev_depth(_e1, _e59, _e58, fsr, reconstructed, _buffer_sizes);
    metal::int2 _e67 = fsr.render_size;
    metal::int2 _e68 = clamp_load(_e1, metal::int2(0), _e67);
    uint clamped_lod_e70 = metal::min(uint(0), color_tex.get_num_mip_levels() - 1);
    metal::float4 _e70 = color_tex.read(metal::min(metal::uint2(_e68), metal::uint2(color_tex.get_width(clamped_lod_e70), color_tex.get_height(clamped_lod_e70)) - 1), clamped_lod_e70);
    metal::float3 _e72 = safe_color(_e70.xyz);
    metal::float3 rgb = metal::max(metal::float3(0.0), _e72);
    o_1.dilated_motion = metal::float4(_e58, 0.0, 0.0);
    float _e80 = nearest;
    o_1.dilated_depth = metal::float4(_e80, 0.0, 0.0, 0.0);
    float _e86 = farthest;
    float _e87 = view_space_depth_in_meters(_e86, fsr);
    o_1.farthest_depth = metal::float4(metal::min(_e87, FP16_MAX), 0.0, 0.0, 0.0);
    float _e95 = rgb_to_luma(rgb);
    o_1.luma = metal::float4(_e95, 0.0, 0.0, 0.0);
    PrepareOut _e100 = o_1;
    const auto _tmp_1 = _e100;
    return fs_prepare_inputsOutput { _tmp_1.dilated_motion, _tmp_1.dilated_depth, _tmp_1.farthest_depth, _tmp_1.luma };
}


struct fs_display_depthInput {
};
struct fs_display_depthOutput {
    float member_2 [[depth(any)]];
};
fragment fs_display_depthOutput fs_display_depth(
  metal::float4 clip_1 [[position]]
, constant FsrParams& fsr [[buffer(0)]]
, metal::depth2d<float, metal::access::sample> depth_tex [[texture(3)]]
) {
    const VsOut in_1 = { clip_1 };
    metal::float2 _e5 = fsr.downscale;
    float _e8 = load_depth(naga_f2i32(in_1.clip.xy * _e5), fsr, depth_tex);
    return fs_display_depthOutput { _e8 };
}
