// GENERATED from Aurora ee458fbf1d95e98c5b3a55c0027a523bb28d7e7f / pinned Naga 6c13013.
// FidelityFX SDK 1.1.4 / MIT: see tools/metal/aurora-evaluation/vendor/aurora/NOTICE.md.
// language: metal4.0
#include <metal_stdlib>
#include <simd/simd.h>

using metal::uint;

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
struct type_13 {
    metal::int2 inner[5];
};
struct type_14 {
    float inner[5];
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
constant int SET_SIZE = 5;
constant int SHADING_CHANGE_LEVELS = 3;
constant float REACTIVE_SCALE = 1.0;
constant float REACTIVE_THRESHOLD = 0.2;
constant float REACTIVE_BINARY_VALUE = 0.9;

metal::int2 naga_f2i32(metal::float2 value) {
    return static_cast<metal::int2>(metal::clamp(value, -2147483600.0, 2147483500.0));
}

metal::int2 px_of(
    VsOut in_5
) {
    return naga_f2i32(in_5.clip.xy);
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
    metal::float2 p_3 = (uv_2 * static_cast<metal::float2>(size_2)) - metal::float2(0.5);
    metal::float2 f = metal::fract(p_3);
    b.base = naga_f2i32(metal::floor(p_3));
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
    float a_1 = (0.4 * x2_) - 1.0;
    float b_4 = (0.25 * x2_) - 1.0;
    return (((1.5625 * a_1) * a_1) - 0.5625) * (b_4 * b_4);
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

type_13 sample_offsets(
) {
    return type_13 {{metal::int2(0, 0), metal::int2(-1, 0), metal::int2(1, 0), metal::int2(0, -1), metal::int2(0, 1)}};
}

void compare_swap(
    thread type_14& s,
    int i_3,
    int j
) {
    float _e4 = s.inner[metal::min(unsigned(i_3), 4u)];
    float _e6 = s.inner[metal::min(unsigned(j), 4u)];
    float lo = metal::min(_e4, _e6);
    float _e10 = s.inner[metal::min(unsigned(i_3), 4u)];
    float _e12 = s.inner[metal::min(unsigned(j), 4u)];
    s.inner[metal::min(unsigned(j), 4u)] = metal::max(_e10, _e12);
    s.inner[metal::min(unsigned(i_3), 4u)] = lo;
    return;
}

void sort_set(
    thread type_14& s_1
) {
    compare_swap(s_1, 0, 3);
    compare_swap(s_1, 1, 4);
    compare_swap(s_1, 0, 2);
    compare_swap(s_1, 1, 3);
    compare_swap(s_1, 0, 1);
    compare_swap(s_1, 2, 4);
    compare_swap(s_1, 1, 2);
    compare_swap(s_1, 3, 4);
    compare_swap(s_1, 2, 3);
    return;
}

float minimum_difference(
    type_14 set0_in,
    type_14 set1_in
) {
    type_14 set0_ = {};
    type_14 set1_ = {};
    float min_diff = 65503.0;
    int a = 0;
    int b_3 = 0;
    int i_4 = 0;
    bool local_3 = {};
    float diff = {};
    set0_ = set0_in;
    set1_ = set1_in;
    sort_set(set0_);
    sort_set(set1_);
    float _e11 = set0_.inner[4];
    float _e13 = set1_.inner[4];
    float fmax = metal::min(_e11, _e13);
    if (fmax > FP32_MIN) {
        uint2 loop_bound = uint2(4294967295u);
        bool loop_init = true;
        while(true) {
            if (metal::all(loop_bound == uint2(0u))) { break; }
            loop_bound -= uint2(loop_bound.y == 0u, 1u);
            if (!loop_init) {
                int _e74 = i_4;
                i_4 = as_type<int>(as_type<uint>(_e74) + as_type<uint>(1));
            }
            loop_init = false;
            int _e19 = i_4;
            if (_e19 < SET_SIZE) {
                float _e24 = min_diff;
                local_3 = _e24 < FP16_MAX;
            } else {
                local_3 = false;
            }
            bool _e28 = local_3;
            if (_e28) {
            } else {
                break;
            }
            {
                int _e29 = a;
                float s0_ = set0_.inner[metal::min(unsigned(metal::min(_e29, 4)), 4u)];
                int _e34 = b_3;
                float s1_ = set1_.inner[metal::min(unsigned(metal::min(_e34, 4)), 4u)];
                diff = s0_ - s1_;
                float _e41 = diff;
                if (metal::abs(_e41) > FP16_MIN) {
                    float _e45 = diff;
                    float _e47 = min_divided_by_max(s0_, s1_);
                    diff = metal::sign(_e45) * (1.0 - _e47);
                    float _e51 = min_diff;
                    float _e52 = diff;
                    float _e53 = diff;
                    float _e55 = min_diff;
                    min_diff = (metal::abs(_e53) < metal::abs(_e55)) ? _e52 : _e51;
                    int _e59 = a;
                    a = as_type<int>(as_type<uint>(_e59) + as_type<uint>(static_cast<int>(s0_ < s1_)));
                    int _e63 = b_3;
                    int _e64 = a;
                    float _e68 = set0_.inner[metal::min(unsigned(metal::min(_e64, 4)), 4u)];
                    b_3 = as_type<int>(as_type<uint>(_e63) + as_type<uint>(static_cast<int>(_e68 >= s1_)));
                } else {
                    min_diff = FP16_MAX;
                }
            }
        }
    }
    float _e76 = min_diff;
    float _e77 = min_diff;
    return _e76 * static_cast<float>(_e77 < 65503.0);
}

float luma_difference(
    metal::int2 p_2,
    metal::float2 uv_3,
    metal::float2 mv_1,
    constant FsrParams& fsr,
    metal::texture2d<float, metal::access::sample> src0_,
    metal::texture2d<float, metal::access::sample> src1_
) {
    type_13 offsets = {};
    type_14 current = {};
    int i_5 = 0;
    type_14 previous = {};
    int i_6 = 0;
    type_13 _e3 = sample_offsets();
    offsets = _e3;
    metal::float2 _e8 = fsr.jitter;
    metal::float2 _e9 = render_size(fsr);
    metal::float2 _e12 = render_size(fsr);
    metal::int2 cur_base = naga_f2i32(metal::floor((uv_3 + (_e8 / _e9)) * _e12));
    uint2 loop_bound_1 = uint2(4294967295u);
    bool loop_init_1 = true;
    while(true) {
        if (metal::all(loop_bound_1 == uint2(0u))) { break; }
        loop_bound_1 -= uint2(loop_bound_1.y == 0u, 1u);
        if (!loop_init_1) {
            int _e41 = i_5;
            i_5 = as_type<int>(as_type<uint>(_e41) + as_type<uint>(1));
        }
        loop_init_1 = false;
        int _e18 = i_5;
        if (_e18 < SET_SIZE) {
        } else {
            break;
        }
        {
            int _e21 = i_5;
            metal::int2 _e23 = offsets.inner[metal::min(unsigned(_e21), 4u)];
            metal::int2 _e26 = fsr.render_size;
            metal::int2 _e27 = clamp_load(cur_base, _e23, _e26);
            int _e28 = i_5;
            uint clamped_lod_e32 = metal::min(uint(0), src0_.get_num_mip_levels() - 1);
            metal::float4 _e32 = src0_.read(metal::min(metal::uint2(_e27), metal::uint2(src0_.get_width(clamped_lod_e32), src0_.get_height(clamped_lod_e32)) - 1), clamped_lod_e32);
            float _e36 = fsr.exposure;
            current.inner[metal::min(unsigned(_e28), 4u)] = metal::max(_e32.x * _e36, EPSILON);
        }
    }
    metal::int2 _e45 = fsr.prev_render_size;
    metal::float2 prev_size = static_cast<metal::float2>(_e45);
    metal::float2 _e49 = fsr.prev_jitter;
    metal::float2 reprojected = (uv_3 + (_e49 / prev_size)) + mv_1;
    bool _e53 = is_uv_inside(reprojected);
    if (!(_e53)) {
        return 0.0;
    }
    metal::int2 prev_base = naga_f2i32(metal::floor(reprojected * prev_size));
    uint2 loop_bound_2 = uint2(4294967295u);
    bool loop_init_2 = true;
    while(true) {
        if (metal::all(loop_bound_2 == uint2(0u))) { break; }
        loop_bound_2 -= uint2(loop_bound_2.y == 0u, 1u);
        if (!loop_init_2) {
            int _e89 = i_6;
            i_6 = as_type<int>(as_type<uint>(_e89) + as_type<uint>(1));
        }
        loop_init_2 = false;
        int _e62 = i_6;
        if (_e62 < SET_SIZE) {
        } else {
            break;
        }
        {
            int _e65 = i_6;
            metal::int2 _e67 = offsets.inner[metal::min(unsigned(_e65), 4u)];
            metal::int2 _e70 = fsr.prev_render_size;
            metal::int2 _e71 = clamp_load(prev_base, _e67, _e70);
            int _e72 = i_6;
            uint clamped_lod_e76 = metal::min(uint(0), src1_.get_num_mip_levels() - 1);
            metal::float4 _e76 = src1_.read(metal::min(metal::uint2(_e71), metal::uint2(src1_.get_width(clamped_lod_e76), src1_.get_height(clamped_lod_e76)) - 1), clamped_lod_e76);
            float _e80 = fsr.delta_pre_exposure;
            float _e84 = fsr.exposure;
            previous.inner[metal::min(unsigned(_e72), 4u)] = metal::max((_e76.x * _e80) * _e84, EPSILON);
        }
    }
    type_14 _e91 = current;
    type_14 _e92 = previous;
    float _e93 = minimum_difference(_e91, _e92);
    return _e93;
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


struct fs_farthest_depth_mip1_Input {
};
struct fs_farthest_depth_mip1_Output {
    metal::float4 member_1 [[color(0)]];
};
fragment fs_farthest_depth_mip1_Output fs_farthest_depth_mip1_(
  metal::float4 clip [[position]]
, constant FsrParams& fsr [[buffer(0)]]
, metal::texture2d<float, metal::access::sample> src0_ [[texture(2)]]
) {
    const VsOut in = { clip };
    float sum = 0.0;
    int i = 0;
    metal::int2 _e1 = px_of(in);
    metal::int2 p_4 = as_type<metal::int2>(as_type<metal::uint2>(_e1) * as_type<uint>(2));
    uint2 loop_bound_3 = uint2(4294967295u);
    bool loop_init_3 = true;
    while(true) {
        if (metal::all(loop_bound_3 == uint2(0u))) { break; }
        loop_bound_3 -= uint2(loop_bound_3.y == 0u, 1u);
        if (!loop_init_3) {
            int _e29 = i;
            i = as_type<int>(as_type<uint>(_e29) + as_type<uint>(1));
        }
        loop_init_3 = false;
        int _e8 = i;
        if (_e8 < 4) {
        } else {
            break;
        }
        {
            int _e11 = i;
            int _e14 = i;
            metal::int2 _e20 = fsr.render_size;
            metal::int2 _e21 = clamp_load(p_4, metal::int2(_e11 & 1, _e14 >> 1u), _e20);
            float _e22 = sum;
            uint clamped_lod_e25 = metal::min(uint(0), src0_.get_num_mip_levels() - 1);
            metal::float4 _e25 = src0_.read(metal::min(metal::uint2(_e21), metal::uint2(src0_.get_width(clamped_lod_e25), src0_.get_height(clamped_lod_e25)) - 1), clamped_lod_e25);
            sum = _e22 + _e25.x;
        }
    }
    float _e31 = sum;
    return fs_farthest_depth_mip1_Output { metal::float4(_e31 * 0.25, 0.0, 0.0, 0.0) };
}


struct fs_shading_change_mip0_Input {
};
struct fs_shading_change_mip0_Output {
    metal::float4 member_2 [[color(0)]];
};
fragment fs_shading_change_mip0_Output fs_shading_change_mip0_(
  metal::float4 clip_1 [[position]]
, constant FsrParams& fsr [[buffer(0)]]
, metal::texture2d<float, metal::access::sample> src0_ [[texture(2)]]
, metal::texture2d<float, metal::access::sample> src1_ [[texture(3)]]
, metal::texture2d<float, metal::access::sample> src2_ [[texture(4)]]
) {
    const VsOut in_1 = { clip_1 };
    metal::float2 sum_1 = metal::float2(0.0);
    int i_1 = 0;
    metal::int2 _e1 = px_of(in_1);
    metal::int2 p_5 = as_type<metal::int2>(as_type<metal::uint2>(_e1) * as_type<uint>(2));
    uint2 loop_bound_4 = uint2(4294967295u);
    bool loop_init_4 = true;
    while(true) {
        if (metal::all(loop_bound_4 == uint2(0u))) { break; }
        loop_bound_4 -= uint2(loop_bound_4.y == 0u, 1u);
        if (!loop_init_4) {
            int _e43 = i_1;
            i_1 = as_type<int>(as_type<uint>(_e43) + as_type<uint>(1));
        }
        loop_init_4 = false;
        int _e9 = i_1;
        if (_e9 < 4) {
        } else {
            break;
        }
        {
            int _e12 = i_1;
            int _e15 = i_1;
            metal::int2 _e21 = fsr.render_size;
            metal::int2 _e22 = clamp_load(p_5, metal::int2(_e12 & 1, _e15 >> 1u), _e21);
            uint clamped_lod_e25 = metal::min(uint(0), src2_.get_num_mip_levels() - 1);
            metal::float4 _e25 = src2_.read(metal::min(metal::uint2(_e22), metal::uint2(src2_.get_width(clamped_lod_e25), src2_.get_height(clamped_lod_e25)) - 1), clamped_lod_e25);
            metal::float2 mv_2 = _e25.xy;
            metal::float2 _e31 = render_size(fsr);
            metal::float2 uv_4 = (static_cast<metal::float2>(_e22) + metal::float2(0.5)) / _e31;
            float _e33 = luma_difference(_e22, uv_4, mv_2, fsr, src0_, src1_);
            metal::float2 _e34 = sum_1;
            sum_1 = _e34 + metal::float2(_e33, (_e33 != 0.0) ? metal::sign(_e33) : 0.0);
        }
    }
    metal::float2 _e45 = sum_1;
    return fs_shading_change_mip0_Output { metal::float4(_e45 * 0.25, 0.0, 0.0) };
}


struct fs_downsampleInput {
};
struct fs_downsampleOutput {
    metal::float4 member_3 [[color(0)]];
};
fragment fs_downsampleOutput fs_downsample(
  metal::float4 clip_2 [[position]]
, metal::texture2d<float, metal::access::sample> src0_ [[texture(2)]]
) {
    const VsOut in_2 = { clip_2 };
    metal::float2 sum_2 = metal::float2(0.0);
    int i_2 = 0;
    metal::int2 size_3 = static_cast<metal::int2>(metal::uint2(src0_.get_width(), src0_.get_height()));
    metal::int2 _e4 = px_of(in_2);
    metal::int2 p_6 = as_type<metal::int2>(as_type<metal::uint2>(_e4) * as_type<uint>(2));
    uint2 loop_bound_5 = uint2(4294967295u);
    bool loop_init_5 = true;
    while(true) {
        if (metal::all(loop_bound_5 == uint2(0u))) { break; }
        loop_bound_5 -= uint2(loop_bound_5.y == 0u, 1u);
        if (!loop_init_5) {
            int _e30 = i_2;
            i_2 = as_type<int>(as_type<uint>(_e30) + as_type<uint>(1));
        }
        loop_init_5 = false;
        int _e12 = i_2;
        if (_e12 < 4) {
        } else {
            break;
        }
        {
            metal::float2 _e15 = sum_2;
            int _e17 = i_2;
            int _e20 = i_2;
            metal::int2 _e24 = clamp_load(p_6, metal::int2(_e17 & 1, _e20 >> 1u), size_3);
            uint clamped_lod_e26 = metal::min(uint(0), src0_.get_num_mip_levels() - 1);
            metal::float4 _e26 = src0_.read(metal::min(metal::uint2(_e24), metal::uint2(src0_.get_width(clamped_lod_e26), src0_.get_height(clamped_lod_e26)) - 1), clamped_lod_e26);
            sum_2 = _e15 + _e26.xy;
        }
    }
    metal::float2 _e32 = sum_2;
    return fs_downsampleOutput { metal::float4(_e32 * 0.25, 0.0, 0.0) };
}


struct fs_shading_changeInput {
};
struct fs_shading_changeOutput {
    metal::float4 member_4 [[color(0)]];
};
fragment fs_shading_changeOutput fs_shading_change(
  metal::float4 clip_3 [[position]]
, constant FsrParams& fsr [[buffer(0)]]
, metal::sampler lin [[sampler(0)]]
, metal::texture2d<float, metal::access::sample> src0_ [[texture(2)]]
) {
    const VsOut in_3 = { clip_3 };
    float change = 0.0;
    int level = 0;
    metal::int2 _e1 = px_of(in_3);
    metal::int2 _e2 = shading_change_render_size(fsr);
    bool _e3 = is_on_screen(_e1, _e2);
    if (!(_e3)) {
        return fs_shading_changeOutput { metal::float4(0.0) };
    }
    metal::float2 _e15 = fsr.jitter;
    metal::float2 _e16 = render_size(fsr);
    metal::float2 uv_5 = ((static_cast<metal::float2>(_e1) + metal::float2(0.5)) / static_cast<metal::float2>(_e2)) + (_e15 / _e16);
    metal::float2 _e23 = clamp_uv(uv_5, _e2, static_cast<metal::int2>(metal::uint2(src0_.get_width(0), src0_.get_height(0))));
    uint2 loop_bound_6 = uint2(4294967295u);
    bool loop_init_6 = true;
    while(true) {
        if (metal::all(loop_bound_6 == uint2(0u))) { break; }
        loop_bound_6 -= uint2(loop_bound_6.y == 0u, 1u);
        if (!loop_init_6) {
            int _e46 = level;
            level = as_type<int>(as_type<uint>(_e46) + as_type<uint>(1));
        }
        loop_init_6 = false;
        int _e28 = level;
        if (_e28 < SHADING_CHANGE_LEVELS) {
        } else {
            break;
        }
        {
            int _e33 = level;
            metal::float4 _e35 = src0_.sample(lin, _e23, metal::level(static_cast<float>(_e33)));
            metal::float2 s_2 = _e35.xy;
            float v_1 = metal::abs(s_2.x * s_2.y);
            if (v_1 > 0.0) {
                float _e43 = change;
                change = metal::max(_e43, v_1);
            }
        }
    }
    float _e48 = change;
    float _e49 = sat(_e48);
    return fs_shading_changeOutput { metal::float4(_e49, 0.0, 0.0, 0.0) };
}


struct fs_autogen_reactiveInput {
};
struct fs_autogen_reactiveOutput {
    metal::float4 member_5 [[color(0)]];
};
fragment fs_autogen_reactiveOutput fs_autogen_reactive(
  metal::float4 clip_4 [[position]]
, constant FsrParams& fsr [[buffer(0)]]
, metal::texture2d<float, metal::access::sample> src0_ [[texture(2)]]
, metal::texture2d<float, metal::access::sample> src1_ [[texture(3)]]
) {
    const VsOut in_4 = { clip_4 };
    metal::int2 _e1 = px_of(in_4);
    metal::int2 _e6 = fsr.render_size;
    metal::int2 _e7 = clamp_load(_e1, metal::int2(0), _e6);
    uint clamped_lod_e12 = metal::min(uint(0), src0_.get_num_mip_levels() - 1);
    metal::float4 _e12 = src0_.read(metal::min(metal::uint2(_e7), metal::uint2(src0_.get_width(clamped_lod_e12), src0_.get_height(clamped_lod_e12)) - 1), clamped_lod_e12);
    metal::float3 _e14 = safe_color(_e12.xyz);
    metal::float3 _e16 = tonemap(metal::max(metal::float3(0.0), _e14));
    uint clamped_lod_e21 = metal::min(uint(0), src1_.get_num_mip_levels() - 1);
    metal::float4 _e21 = src1_.read(metal::min(metal::uint2(_e7), metal::uint2(src1_.get_width(clamped_lod_e21), src1_.get_height(clamped_lod_e21)) - 1), clamped_lod_e21);
    metal::float3 _e23 = safe_color(_e21.xyz);
    metal::float3 _e25 = tonemap(metal::max(metal::float3(0.0), _e23));
    metal::float3 delta = metal::abs(_e25 - _e16);
    float _e37 = fsr.reactive_on;
    float v_2 = (metal::max(delta.x, metal::max(delta.y, delta.z)) * REACTIVE_SCALE) * _e37;
    return fs_autogen_reactiveOutput { metal::float4((v_2 < REACTIVE_THRESHOLD) ? 0.0 : REACTIVE_BINARY_VALUE, 0.0, 0.0, 0.0) };
}
