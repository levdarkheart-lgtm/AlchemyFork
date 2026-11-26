// GENERATED from Aurora ee458fbf1d95e98c5b3a55c0027a523bb28d7e7f / pinned Naga 6c13013.
// FidelityFX SDK 1.1.4 / MIT: see tools/metal/aurora-evaluation/vendor/aurora/NOTICE.md.
// language: metal4.0
#include <metal_stdlib>
#include <simd/simd.h>

using metal::uint;

struct _mslBufferSizes {
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
typedef float type_13[1];
struct InstabilityOut {
    metal::float4 luma_history;
    metal::float4 instability;
};
struct Params {
    metal::int2 hr_pos;
    metal::float2 hr_uv;
    metal::float2 lr_uv_jittered;
    metal::float2 lr_uv_hw;
    metal::float2 mv;
    metal::float2 reprojected_hr_uv;
    float velocity_4k;
    float disocclusion;
    float reactive;
    float shading_change;
    float accumulation;
    float luma_instability;
    float farthest_depth_m;
    bool existing_sample;
    bool new_sample;
    char _pad15[2];
};
struct Data {
    RectificationBox clip;
    metal::packed_float3 upsampled_color;
    float upsampled_weight;
    metal::packed_float3 history_color;
    float history_weight;
    float lock;
    float lock_contribution;
    char _pad7[8];
};
struct type_14 {
    metal::float4 inner[4];
};
struct type_16 {
    metal::float3 inner[9];
};
struct AccumulateOut {
    metal::float4 internal;
    metal::float4 color;
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
constant float RCAS_LIMIT = 0.1875;

metal::int2 naga_f2i32(metal::float2 value) {
    return static_cast<metal::int2>(metal::clamp(value, -2147483600.0, 2147483500.0));
}

metal::int2 px_of(
    VsOut in_3
) {
    return naga_f2i32(in_3.clip.xy);
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
    bool local_2 = {};
    bool local_3 = {};
    bool local_4 = {};
    if (uv.x >= 0.0) {
        local_2 = uv.x <= 1.0;
    } else {
        local_2 = false;
    }
    bool _e10 = local_2;
    if (_e10) {
        local_3 = uv.y >= 0.0;
    } else {
        local_3 = false;
    }
    bool _e17 = local_3;
    if (_e17) {
        local_4 = uv.y <= 1.0;
    } else {
        local_4 = false;
    }
    bool _e24 = local_4;
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
    metal::float2 p_5 = (uv_2 * static_cast<metal::float2>(size_2)) - metal::float2(0.5);
    metal::float2 f = metal::fract(p_5);
    b.base = naga_f2i32(metal::floor(p_5));
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
    float a_2 = (0.4 * x2_) - 1.0;
    float b_5 = (0.25 * x2_) - 1.0;
    return (((1.5625 * a_2) * a_2) - 0.5625) * (b_5 * b_5);
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

metal::int2 naga_div(metal::int2 lhs, metal::int2 rhs) {
    return lhs / metal::select(rhs, 1, (lhs == (-2147483647 - 1) & rhs == -1) | (rhs == 0));
}

float farthest_depth_mip1_(
    metal::float2 uv_3,
    constant FsrParams& fsr,
    metal::sampler lin,
    metal::texture2d<float, metal::access::sample> farthest_mip1_tex
) {
    metal::int2 _e3 = fsr.render_size;
    metal::float2 _e10 = clamp_uv(uv_3, naga_div(_e3, metal::int2(2)), static_cast<metal::int2>(metal::uint2(farthest_mip1_tex.get_width(), farthest_mip1_tex.get_height())));
    metal::float4 _e14 = farthest_mip1_tex.sample(lin, _e10, metal::level(0.0));
    return _e14.x;
}

metal::float4 history_texel(
    metal::int2 p_2,
    metal::int2 offset_1,
    metal::int2 size_3,
    metal::texture2d<float, metal::access::sample> t5_
) {
    metal::int2 q = metal::clamp(as_type<metal::int2>(as_type<metal::uint2>(p_2) + as_type<metal::uint2>(offset_1)), metal::int2(1), as_type<metal::int2>(as_type<metal::uint2>(size_3) - as_type<metal::uint2>(metal::int2(2))));
    uint clamped_lod_e12 = metal::min(uint(0), t5_.get_num_mip_levels() - 1);
    metal::float4 _e12 = t5_.read(metal::min(metal::uint2(q), metal::uint2(t5_.get_width(clamped_lod_e12), t5_.get_height(clamped_lod_e12)) - 1), clamped_lod_e12);
    return _e12;
}

metal::float4 lanczos2_4_(
    metal::float4 c0_,
    metal::float4 c1_,
    metal::float4 c2_,
    metal::float4 c3_,
    float t
) {
    float _e7 = lanczos2_(-1.0 - t);
    float _e10 = lanczos2_(-0.0 - t);
    float _e13 = lanczos2_(1.0 - t);
    float _e16 = lanczos2_(2.0 - t);
    return ((((_e7 * c0_) + (_e10 * c1_)) + (_e13 * c2_)) + (_e16 * c3_)) / metal::float4(((_e7 + _e10) + _e13) + _e16);
}

metal::float4 history_sample(
    metal::float2 uv_4,
    metal::int2 size_4,
    metal::texture2d<float, metal::access::sample> t5_
) {
    metal::float2 pxs = {};
    type_14 rows = {};
    int y = 0;
    pxs = (uv_4 * static_cast<metal::float2>(size_4)) - metal::float2(0.5);
    metal::float2 _e8 = pxs;
    metal::float2 frac = metal::fract(_e8);
    metal::float2 _e10 = pxs;
    pxs = metal::clamp(_e10, metal::float2(0.0), static_cast<metal::float2>(as_type<metal::int2>(as_type<metal::uint2>(size_4) - as_type<metal::uint2>(metal::int2(1)))));
    metal::float2 _e18 = pxs;
    metal::int2 base_1 = naga_f2i32(metal::floor(_e18));
    uint2 loop_bound = uint2(4294967295u);
    bool loop_init = true;
    while(true) {
        if (metal::all(loop_bound == uint2(0u))) { break; }
        loop_bound -= uint2(loop_bound.y == 0u, 1u);
        if (!loop_init) {
            int _e56 = y;
            y = as_type<int>(as_type<uint>(_e56) + as_type<uint>(1));
        }
        loop_init = false;
        int _e24 = y;
        if (_e24 < 4) {
        } else {
            break;
        }
        {
            int _e27 = y;
            int _e29 = y;
            metal::float4 _e34 = history_texel(base_1, metal::int2(-1, as_type<int>(as_type<uint>(_e29) - as_type<uint>(1))), size_4, t5_);
            int _e35 = y;
            metal::float4 _e40 = history_texel(base_1, metal::int2(0, as_type<int>(as_type<uint>(_e35) - as_type<uint>(1))), size_4, t5_);
            int _e41 = y;
            metal::float4 _e46 = history_texel(base_1, metal::int2(1, as_type<int>(as_type<uint>(_e41) - as_type<uint>(1))), size_4, t5_);
            int _e47 = y;
            metal::float4 _e52 = history_texel(base_1, metal::int2(2, as_type<int>(as_type<uint>(_e47) - as_type<uint>(1))), size_4, t5_);
            metal::float4 _e54 = lanczos2_4_(_e34, _e40, _e46, _e52, frac.x);
            rows.inner[metal::min(unsigned(_e27), 3u)] = _e54;
        }
    }
    metal::float4 _e59 = rows.inner[0];
    metal::float4 _e61 = rows.inner[1];
    metal::float4 _e63 = rows.inner[2];
    metal::float4 _e65 = rows.inner[3];
    metal::float4 _e67 = lanczos2_4_(_e59, _e61, _e63, _e65, frac.y);
    metal::float4 _e71 = history_texel(base_1, metal::int2(0, 0), size_4, t5_);
    metal::float4 _e75 = history_texel(base_1, metal::int2(1, 0), size_4, t5_);
    metal::float4 _e79 = history_texel(base_1, metal::int2(0, 1), size_4, t5_);
    metal::float4 _e83 = history_texel(base_1, metal::int2(1, 1), size_4, t5_);
    metal::float4 mn = metal::min(metal::min(_e71, _e75), metal::min(_e79, _e83));
    metal::float4 mx = metal::max(metal::max(_e71, _e75), metal::max(_e79, _e83));
    return metal::clamp(_e67, mn, mx);
}

void reproject_history_color(
    Params params_1,
    thread Data& data_1,
    constant FsrParams& fsr,
    metal::texture2d<float, metal::access::sample> t5_
) {
    metal::int2 _e5 = fsr.prev_upscale_size;
    metal::float4 _e6 = history_sample(params_1.reprojected_hr_uv, _e5, t5_);
    float _e11 = fsr.delta_pre_exposure;
    float _e15 = fsr.exposure;
    metal::float3 _e17 = rgb_to_ycocg((_e6.xyz * _e11) * _e15);
    data_1.history_color = _e17;
    data_1.lock = _e6.w;
    return;
}

void update_lock_status(
    Params params_2,
    thread Data& data_2,
    constant FsrParams& fsr,
    device type_13 const& new_locks,
    constant _mslBufferSizes& _buffer_sizes
) {
    float _e3 = data_2.lock;
    data_2.lock = _e3 * static_cast<float>(!(params_2.new_sample));
    float _e9 = sat(params_2.shading_change);
    float lifetime_decrease_factor = metal::max(_e9, metal::max(params_2.reactive, params_2.disocclusion));
    float _e16 = data_2.lock;
    data_2.lock = metal::max(0.0, _e16 - (lifetime_decrease_factor * LOCK_MAX));
    float _e24 = data_2.lock;
    float _e27 = sat(_e24 - LOCK_THRESHOLD);
    float _e30 = sat(_e27 * 1.0);
    data_2.lock_contribution = _e30;
    int _e36 = fsr.upscale_size.x;
    int i_1 = as_type<int>(as_type<uint>(as_type<int>(as_type<uint>(params_2.hr_pos.y) * as_type<uint>(_e36))) + as_type<uint>(params_2.hr_pos.x));
    float _e43 = new_locks[metal::min(unsigned(i_1), (_buffer_sizes.size8 - 0 - 4) / 4)];
    float new_lock = _e43 * (1.0 - metal::max(params_2.shading_change * 0.0, params_2.reactive));
    float _e54 = data_2.lock;
    data_2.lock = metal::max(0.0, metal::min(_e54 + new_lock, LOCK_MAX));
    float _e62 = fsr.jitter_sequence_length;
    float lifetime_decrease = (0.1 / _e62) * (1.0 - lifetime_decrease_factor);
    float _e70 = data_2.lock;
    data_2.lock = metal::max(0.0, _e70 - lifetime_decrease);
    float _e75 = data_2.lock;
    bool _e79 = is_uv_inside(params_2.hr_uv - params_2.mv);
    data_2.lock = _e75 * static_cast<float>(_e79);
    return;
}

void compute_base_accumulation_weight(
    Params params_3,
    thread Data& data_3,
    constant FsrParams& fsr
) {
    float base = {};
    base = params_3.accumulation;
    float _e4 = base;
    float _e5 = base;
    float _e9 = fsr.velocity_factor;
    float _e15 = sat(metal::max(0.0, (params_3.velocity_4k * _e9) / 0.5));
    base = metal::min(_e4, metal::mix(_e5, 0.15, _e15));
    float _e20 = base;
    data_3.history_weight = _e20;
    return;
}

metal::float3 prepared_color(
    metal::int2 p_3,
    constant FsrParams& fsr,
    metal::texture2d<float, metal::access::sample> t4_
) {
    uint clamped_lod_e5 = metal::min(uint(0), t4_.get_num_mip_levels() - 1);
    metal::float4 _e5 = t4_.read(metal::min(metal::uint2(p_3), metal::uint2(t4_.get_width(clamped_lod_e5), t4_.get_height(clamped_lod_e5)) - 1), clamped_lod_e5);
    metal::float3 _e7 = safe_color(_e5.xyz);
    float _e11 = fsr.exposure;
    metal::float3 c_9 = metal::max(metal::float3(0.0), _e7) * _e11;
    metal::float3 _e13 = rgb_to_ycocg(c_9);
    return _e13;
}

float upsample_lanczos_weight(
    metal::float2 offset_2,
    float kernel_weight
) {
    metal::float2 biased = offset_2 * kernel_weight;
    float _e4 = lanczos2_approx_sq(metal::dot(biased, biased));
    return _e4;
}

void compute_upsampled_color_and_weight(
    Params params_4,
    thread Data& data_4,
    constant FsrParams& fsr,
    metal::texture2d<float, metal::access::sample> t4_
) {
    type_16 samples = {};
    int idx = 0;
    int row = 0;
    int col = {};
    metal::float3 s = {};
    RectificationBox clip = {};
    int row_1 = 0;
    int col_1 = {};
    bool local_5 = {};
    metal::float2 dst = static_cast<metal::float2>(params_4.hr_pos) + metal::float2(0.5);
    metal::float2 _e9 = fsr.downscale;
    metal::float2 src = dst * _e9;
    metal::int2 src_input = naga_f2i32(metal::floor(src));
    metal::float2 _e19 = fsr.jitter;
    metal::float2 src_unjittered = (static_cast<metal::float2>(src_input) + metal::float2(0.5)) - _e19;
    metal::float2 base_offset = src_unjittered - src;
    bool flip_col = src_unjittered.x > src.x;
    bool flip_row = src_unjittered.y > src.y;
    metal::int2 offset_tl = metal::int2(flip_col ? -2 : -1, flip_row ? -2 : -1);
    bool initial_1 = params_4.accumulation == 0.0;
    uint2 loop_bound_1 = uint2(4294967295u);
    bool loop_init_1 = true;
    while(true) {
        if (metal::all(loop_bound_1 == uint2(0u))) { break; }
        loop_bound_1 -= uint2(loop_bound_1.y == 0u, 1u);
        if (!loop_init_1) {
            int _e86 = row;
            row = as_type<int>(as_type<uint>(_e86) + as_type<uint>(1));
        }
        loop_init_1 = false;
        int _e43 = row;
        if (_e43 < 3) {
        } else {
            break;
        }
        {
            col = 0;
            uint2 loop_bound_2 = uint2(4294967295u);
            bool loop_init_2 = true;
            while(true) {
                if (metal::all(loop_bound_2 == uint2(0u))) { break; }
                loop_bound_2 -= uint2(loop_bound_2.y == 0u, 1u);
                if (!loop_init_2) {
                    int _e83 = col;
                    col = as_type<int>(as_type<uint>(_e83) + as_type<uint>(1));
                }
                loop_init_2 = false;
                int _e48 = col;
                if (_e48 < 3) {
                } else {
                    break;
                }
                {
                    int _e51 = col;
                    int _e52 = col;
                    int _e56 = row;
                    int _e57 = row;
                    metal::int2 cr = metal::int2(flip_col ? as_type<int>(as_type<uint>(3) - as_type<uint>(_e52)) : _e51, flip_row ? as_type<int>(as_type<uint>(3) - as_type<uint>(_e57)) : _e56);
                    metal::int2 _e68 = fsr.render_size;
                    metal::int2 _e69 = clamp_load(as_type<metal::int2>(as_type<metal::uint2>(as_type<metal::int2>(as_type<metal::uint2>(src_input) + as_type<metal::uint2>(offset_tl))) + as_type<metal::uint2>(cr)), metal::int2(0), _e68);
                    metal::float3 _e70 = prepared_color(_e69, fsr, t4_);
                    s = _e70;
                    if (initial_1) {
                        metal::float3 _e72 = s;
                        metal::float3 _e73 = ycocg_to_rgb(_e72);
                        metal::float3 _e74 = tonemap(_e73);
                        metal::float3 _e75 = rgb_to_ycocg(_e74);
                        s = _e75;
                    }
                    int _e76 = idx;
                    metal::float3 _e78 = s;
                    samples.inner[metal::min(unsigned(_e76), 8u)] = _e78;
                    int _e80 = idx;
                    idx = as_type<int>(as_type<uint>(_e80) + as_type<uint>(1));
                }
            }
        }
    }
    metal::float2 _e90 = fsr.downscale;
    float kernel_bias_max = metal::min(1.99, 1.0 + ((metal::float2(1.0) / _e90) - metal::float2(1.0)).x);
    float kernel_bias_min = metal::max(1.0, (1.0 + kernel_bias_max) * 0.3);
    float _e117 = data_4.history_weight;
    float _e120 = sat(_e117 * 5.0);
    float kernel_bias_weight = metal::min(1.0 - (params_4.disocclusion * 0.5), metal::min(1.0 - params_4.shading_change, _e120));
    float kernel_bias = metal::mix(kernel_bias_min, kernel_bias_max, kernel_bias_weight);
    idx = 0;
    uint2 loop_bound_3 = uint2(4294967295u);
    bool loop_init_3 = true;
    while(true) {
        if (metal::all(loop_bound_3 == uint2(0u))) { break; }
        loop_bound_3 -= uint2(loop_bound_3.y == 0u, 1u);
        if (!loop_init_3) {
            int _e196 = row_1;
            row_1 = as_type<int>(as_type<uint>(_e196) + as_type<uint>(1));
        }
        loop_init_3 = false;
        int _e128 = row_1;
        if (_e128 < 3) {
        } else {
            break;
        }
        {
            col_1 = 0;
            uint2 loop_bound_4 = uint2(4294967295u);
            bool loop_init_4 = true;
            while(true) {
                if (metal::all(loop_bound_4 == uint2(0u))) { break; }
                loop_bound_4 -= uint2(loop_bound_4.y == 0u, 1u);
                if (!loop_init_4) {
                    int _e193 = col_1;
                    col_1 = as_type<int>(as_type<uint>(_e193) + as_type<uint>(1));
                }
                loop_init_4 = false;
                int _e133 = col_1;
                if (_e133 < 3) {
                } else {
                    break;
                }
                {
                    int _e136 = col_1;
                    int _e137 = col_1;
                    int _e141 = row_1;
                    int _e142 = row_1;
                    metal::int2 cr_1 = metal::int2(flip_col ? as_type<int>(as_type<uint>(3) - as_type<uint>(_e137)) : _e136, flip_row ? as_type<int>(as_type<uint>(3) - as_type<uint>(_e142)) : _e141);
                    metal::float2 offset_3 = static_cast<metal::float2>(offset_tl) + static_cast<metal::float2>(cr_1);
                    metal::float2 sample_offset = base_offset + offset_3;
                    metal::int2 q_1 = as_type<metal::int2>(as_type<metal::uint2>(as_type<metal::int2>(as_type<metal::uint2>(src_input) + as_type<metal::uint2>(offset_tl))) + as_type<metal::uint2>(cr_1));
                    metal::int2 _e155 = fsr.render_size;
                    bool _e156 = is_on_screen(q_1, _e155);
                    float on_screen = static_cast<float>(_e156);
                    if (!(initial_1)) {
                        float _e159 = upsample_lanczos_weight(sample_offset, kernel_bias);
                        float w_2 = on_screen * _e159;
                        metal::float3 _e162 = data_4.upsampled_color;
                        int _e163 = idx;
                        metal::float3 _e165 = samples.inner[metal::min(unsigned(_e163), 8u)];
                        data_4.upsampled_color = _e162 + (_e165 * w_2);
                        float _e169 = data_4.upsampled_weight;
                        data_4.upsampled_weight = _e169 + w_2;
                    }
                    float box_weight = metal::exp(-2.3 * metal::dot(sample_offset, sample_offset)) * on_screen;
                    int _e176 = row_1;
                    if (_e176 == 0) {
                        int _e181 = col_1;
                        local_5 = _e181 == 0;
                    } else {
                        local_5 = false;
                    }
                    bool _e185 = local_5;
                    int _e186 = idx;
                    metal::float3 _e188 = samples.inner[metal::min(unsigned(_e186), 8u)];
                    box_add_sample(_e185, clip, _e188, box_weight);
                    int _e190 = idx;
                    idx = as_type<int>(as_type<uint>(_e190) + as_type<uint>(1));
                }
            }
        }
    }
    box_compute_variance(clip);
    RectificationBox _e199 = clip;
    data_4.clip = _e199;
    float _e201 = data_4.upsampled_weight;
    float _e203 = data_4.upsampled_weight;
    data_4.upsampled_weight = _e201 * static_cast<float>(_e203 > EPSILON);
    float _e209 = data_4.upsampled_weight;
    if (_e209 > EPSILON) {
        metal::float3 _e213 = data_4.upsampled_color;
        float _e215 = data_4.upsampled_weight;
        data_4.upsampled_color = _e213 / metal::float3(_e215);
        float _e219 = data_4.upsampled_weight;
        data_4.upsampled_weight = _e219 * AVERAGE_LANCZOS_WEIGHT_PER_FRAME;
        metal::float3 _e224 = data_4.upsampled_color;
        metal::float3 _e227 = data_4.clip.aabb_min;
        metal::float3 _e230 = data_4.clip.aabb_max;
        data_4.upsampled_color = metal::clamp(_e224, _e227, _e230);
    }
    if (initial_1) {
        metal::float3 _e235 = data_4.clip.center;
        metal::float3 _e236 = ycocg_to_rgb(_e235);
        metal::float3 _e237 = inverse_tonemap(_e236);
        metal::float3 _e238 = rgb_to_ycocg(_e237);
        data_4.upsampled_color = _e238;
        data_4.upsampled_weight = 1.0;
        data_4.history_weight = 0.0;
        return;
    } else {
        return;
    }
}

void rectify_history(
    Params params_5,
    thread Data& data_5
) {
    float _e5 = sat(params_5.velocity_4k / 20.0);
    float _e11 = sat(0.75 - (params_5.farthest_depth_m / 20.0));
    float accumulation_factor = 1.0 - params_5.accumulation;
    float reactive_factor = metal::pow(params_5.reactive, 0.5);
    float t_1 = metal::max(_e5, metal::max(_e11, metal::max(accumulation_factor, metal::max(reactive_factor, params_5.shading_change))));
    float box_scale = metal::mix(3.0, 1.0, t_1);
    metal::float3 _e28 = data_5.clip.vec_;
    metal::float3 scaled = (_e28 * metal::float3(1.7, 1.0, 1.0)) * box_scale;
    metal::float3 clamped_scaled = metal::max(scaled, metal::float3(0.0000001193));
    metal::float3 _e39 = data_5.history_color;
    metal::float3 _e42 = data_5.clip.center;
    metal::float3 transformed = (_e39 - _e42) / clamped_scaled;
    if (metal::length(transformed) > 1.0) {
        metal::float3 _e52 = data_5.clip.center;
        metal::float3 final_clamped = (metal::normalize(transformed) * scaled) + _e52;
        float _e56 = data_5.lock_contribution;
        float contribution = (metal::max(params_5.luma_instability, _e56) * params_5.accumulation) * (1.0 - params_5.disocclusion);
        metal::float3 _e66 = data_5.history_color;
        float _e67 = sat(contribution);
        data_5.history_color = metal::mix(final_clamped, _e66, _e67);
        return;
    } else {
        return;
    }
}

void accumulate(
    thread Data& data_6
) {
    float _e2 = data_6.history_weight;
    float _e4 = data_6.history_weight;
    data_6.history_weight = _e2 * static_cast<float>(_e4 > FP16_MIN);
    float _e12 = data_6.history_weight;
    float _e14 = data_6.upsampled_weight;
    data_6.history_weight = metal::max(EPSILON, _e12 + _e14);
    metal::float3 _e19 = data_6.upsampled_color;
    metal::float3 _e20 = ycocg_to_rgb(_e19);
    metal::float3 _e21 = tonemap(_e20);
    metal::float3 _e22 = rgb_to_ycocg(_e21);
    data_6.upsampled_color = _e22;
    metal::float3 _e25 = data_6.history_color;
    metal::float3 _e26 = ycocg_to_rgb(_e25);
    metal::float3 _e27 = tonemap(_e26);
    metal::float3 _e28 = rgb_to_ycocg(_e27);
    data_6.history_color = _e28;
    float _e30 = data_6.upsampled_weight;
    float _e32 = data_6.history_weight;
    float _e34 = sat(_e30 / _e32);
    metal::float3 _e37 = data_6.history_color;
    metal::float3 _e39 = data_6.upsampled_color;
    metal::float3 _e41 = ycocg_to_rgb(metal::mix(_e37, _e39, _e34));
    data_6.history_color = _e41;
    metal::float3 _e44 = data_6.history_color;
    metal::float3 _e45 = inverse_tonemap(_e44);
    data_6.history_color = _e45;
    return;
}

metal::float3 rcas_load(
    metal::int2 p_4,
    constant FsrParams& fsr,
    metal::texture2d<float, metal::access::sample> t5_
) {
    metal::int2 _e6 = fsr.upscale_size;
    metal::int2 _e7 = clamp_load(p_4, metal::int2(0), _e6);
    uint clamped_lod_e9 = metal::min(uint(0), t5_.get_num_mip_levels() - 1);
    metal::float4 _e9 = t5_.read(metal::min(metal::uint2(_e7), metal::uint2(t5_.get_width(clamped_lod_e9), t5_.get_height(clamped_lod_e9)) - 1), clamped_lod_e9);
    float _e13 = fsr.exposure;
    return _e9.xyz * _e13;
}

float max3_(
    float a,
    float b_3,
    float c_7
) {
    return metal::max(a, metal::max(b_3, c_7));
}

float min3_(
    float a_1,
    float b_4,
    float c_8
) {
    return metal::min(a_1, metal::min(b_4, c_8));
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
    float y_1 = static_cast<float>(vi & 2u);
    o.clip = metal::float4((x_3 * 2.0) - 1.0, (y_1 * 2.0) - 1.0, 0.0, 1.0);
    VsOut _e22 = o;
    const auto _tmp = _e22;
    return vs_fsrOutput { _tmp.clip };
}


struct fs_luma_instabilityInput {
};
struct fs_luma_instabilityOutput {
    metal::float4 luma_history [[color(0)]];
    metal::float4 instability [[color(1)]];
};
fragment fs_luma_instabilityOutput fs_luma_instability(
  metal::float4 clip_1 [[position]]
, constant FsrParams& fsr [[buffer(0)]]
, metal::sampler lin [[sampler(0)]]
, metal::texture2d<float, metal::access::sample> dilated_motion_tex [[texture(2)]]
, metal::texture2d<float, metal::access::sample> masks_tex [[texture(3)]]
, metal::texture2d<float, metal::access::sample> t4_ [[texture(4)]]
, metal::texture2d<float, metal::access::sample> t5_ [[texture(5)]]
) {
    const VsOut in = { clip_1 };
    metal::float4 history = metal::float4(0.0);
    float instability = 0.0;
    float max_similarity = {};
    float unstable = 0.0;
    int i = 1;
    InstabilityOut o_1 = {};
    metal::int2 _e1 = px_of(in);
    uint clamped_lod_e9 = metal::min(uint(0), dilated_motion_tex.get_num_mip_levels() - 1);
    metal::float4 _e9 = dilated_motion_tex.read(metal::min(metal::uint2(_e1), metal::uint2(dilated_motion_tex.get_width(clamped_lod_e9), dilated_motion_tex.get_height(clamped_lod_e9)) - 1), clamped_lod_e9);
    metal::float2 mv_1 = _e9.xy;
    metal::float2 _e15 = render_size(fsr);
    metal::float2 uv_5 = (static_cast<metal::float2>(_e1) + metal::float2(0.5)) / _e15;
    metal::float2 _e19 = fsr.jitter;
    metal::float2 _e20 = render_size(fsr);
    metal::float2 uv_cur_jittered = uv_5 + (_e19 / _e20);
    metal::float2 _e25 = fsr.prev_jitter;
    metal::int2 _e28 = fsr.prev_render_size;
    metal::float2 uv_prev_jittered = uv_5 + (_e25 / static_cast<metal::float2>(_e28));
    metal::float2 reprojected = uv_prev_jittered + mv_1;
    bool _e33 = is_uv_inside(reprojected);
    if (_e33) {
        metal::int2 _e38 = fsr.render_size;
        metal::int2 _e41 = fsr.max_render_size;
        metal::float2 _e42 = clamp_uv(uv_cur_jittered, _e38, _e41);
        metal::float4 masks = masks_tex.sample(lin, _e42, metal::level(0.0));
        float _e46 = sat(masks.x);
        float _e48 = sat(masks.y);
        float _e50 = sat(masks.z);
        float _e52 = sat(masks.w);
        if (_e52 > 0.9) {
            metal::int2 _e59 = fsr.render_size;
            metal::int2 _e62 = fsr.max_render_size;
            metal::float2 _e63 = clamp_uv(uv_cur_jittered, _e59, _e62);
            metal::float4 _e65 = t4_.sample(lin, _e63, metal::level(0.0));
            float _e69 = fsr.exposure;
            float current = _e65.x * _e69;
            metal::int2 _e75 = fsr.prev_render_size;
            metal::int2 _e78 = fsr.max_render_size;
            metal::float2 _e79 = clamp_uv(reprojected, _e75, _e78);
            metal::float4 _e81 = t5_.sample(lin, _e79, metal::level(0.0));
            float _e84 = fsr.delta_pre_exposure;
            float _e88 = fsr.exposure;
            history = (_e81 * _e84) * _e88;
            float _e91 = history.x;
            float diffs0_ = current - _e91;
            float _e94 = history.x;
            float _e96 = min_divided_by_max_or(current, _e94, 1.0);
            max_similarity = _e96;
            if (_e96 < 1.0) {
                uint2 loop_bound_5 = uint2(4294967295u);
                bool loop_init_5 = true;
                while(true) {
                    if (metal::all(loop_bound_5 == uint2(0u))) { break; }
                    loop_bound_5 -= uint2(loop_bound_5.y == 0u, 1u);
                    if (!loop_init_5) {
                        int _e121 = i;
                        i = as_type<int>(as_type<uint>(_e121) + as_type<uint>(1));
                    }
                    loop_init_5 = false;
                    int _e104 = i;
                    if (_e104 <= 3) {
                    } else {
                        break;
                    }
                    {
                        int _e107 = i;
                        float _e109 = history[metal::min(unsigned(_e107), 3u)];
                        float diffs1_ = current - _e109;
                        int _e111 = i;
                        float _e113 = history[metal::min(unsigned(_e111), 3u)];
                        float _e114 = min_divided_by_max(current, _e113);
                        if (metal::sign(diffs0_) == metal::sign(diffs1_)) {
                            float _e118 = max_similarity;
                            max_similarity = metal::max(_e118, _e114);
                        }
                    }
                }
                float _e123 = max_similarity;
                unstable = static_cast<float>(_e123 > _e96);
            }
            float _e127 = history.x;
            float _e129 = history.y;
            float _e131 = history.z;
            float _e135 = fsr.exposure;
            history = metal::float4(current, _e127, _e129, _e131) / metal::float4(_e135);
            float _e138 = unstable;
            float _e140 = history.w;
            instability = _e138 * static_cast<float>(_e140 != 0.0);
            float _e145 = get_4k_velocity(mv_1);
            float _e148 = sat(_e145 / 20.0);
            float velocity_weight = 1.0 - _e148;
            float _e151 = instability;
            instability = _e151 * (((velocity_weight * (1.0 - _e48)) * (1.0 - _e46)) * (1.0 - _e50));
        }
    }
    metal::float4 _e164 = history;
    o_1.luma_history = _e164;
    float _e166 = instability;
    o_1.instability = metal::float4(_e166, 0.0, 0.0, 0.0);
    InstabilityOut _e171 = o_1;
    const auto _tmp_1 = _e171;
    return fs_luma_instabilityOutput { _tmp_1.luma_history, _tmp_1.instability };
}


struct fs_accumulateInput {
};
struct fs_accumulateOutput {
    metal::float4 internal [[color(0)]];
    metal::float4 color [[color(1)]];
};
fragment fs_accumulateOutput fs_accumulate(
  metal::float4 clip_2 [[position]]
, constant FsrParams& fsr [[buffer(0)]]
, metal::sampler lin [[sampler(0)]]
, metal::texture2d<float, metal::access::sample> dilated_motion_tex [[texture(2)]]
, metal::texture2d<float, metal::access::sample> masks_tex [[texture(3)]]
, metal::texture2d<float, metal::access::sample> t4_ [[texture(4)]]
, metal::texture2d<float, metal::access::sample> t5_ [[texture(5)]]
, metal::texture2d<float, metal::access::sample> farthest_mip1_tex [[texture(6)]]
, metal::texture2d<float, metal::access::sample> luma_instability_tex [[texture(7)]]
, device type_13& new_locks [[buffer(8)]]
, constant _mslBufferSizes& _buffer_sizes [[buffer(30)]]
) {
    const VsOut in_1 = { clip_2 };
    Params params = {};
    bool local = {};
    Data data = {};
    bool local_1 = {};
    AccumulateOut o_2 = {};
    metal::int2 _e1 = px_of(in_1);
    params.hr_pos = _e1;
    metal::int2 _e11 = fsr.upscale_size;
    params.hr_uv = (static_cast<metal::float2>(_e1) + metal::float2(0.5)) / static_cast<metal::float2>(_e11);
    metal::float2 _e16 = params.hr_uv;
    metal::float2 _e19 = fsr.jitter;
    metal::float2 _e20 = render_size(fsr);
    params.lr_uv_jittered = _e16 + (_e19 / _e20);
    metal::float2 _e25 = params.lr_uv_jittered;
    metal::int2 _e28 = fsr.render_size;
    metal::int2 _e31 = fsr.max_render_size;
    metal::float2 _e32 = clamp_uv(_e25, _e28, _e31);
    params.lr_uv_hw = _e32;
    metal::float2 _e34 = params.hr_uv;
    metal::float2 _e35 = render_size(fsr);
    metal::int2 _e42 = fsr.render_size;
    metal::int2 _e43 = clamp_load(naga_f2i32(_e34 * _e35), metal::int2(0), _e42);
    uint clamped_lod_e47 = metal::min(uint(0), dilated_motion_tex.get_num_mip_levels() - 1);
    metal::float4 _e47 = dilated_motion_tex.read(metal::min(metal::uint2(_e43), metal::uint2(dilated_motion_tex.get_width(clamped_lod_e47), dilated_motion_tex.get_height(clamped_lod_e47)) - 1), clamped_lod_e47);
    params.mv = _e47.xy;
    metal::float2 _e51 = params.mv;
    float _e52 = get_4k_velocity(_e51);
    params.velocity_4k = _e52;
    metal::float2 _e55 = params.hr_uv;
    metal::float2 _e57 = params.mv;
    params.reprojected_hr_uv = _e55 + _e57;
    metal::float2 _e61 = params.reprojected_hr_uv;
    bool _e62 = is_uv_inside(_e61);
    params.existing_sample = _e62;
    metal::float2 _e67 = params.hr_uv;
    metal::int2 _e70 = fsr.render_size;
    metal::int2 _e73 = fsr.max_render_size;
    metal::float2 _e74 = clamp_uv(_e67, _e70, _e73);
    metal::float4 _e76 = luma_instability_tex.sample(lin, _e74, metal::level(0.0));
    params.luma_instability = _e76.x;
    metal::float2 _e80 = params.lr_uv_jittered;
    float _e81 = farthest_depth_mip1_(_e80, fsr, lin, farthest_mip1_tex);
    params.farthest_depth_m = _e81;
    bool _e84 = params.existing_sample;
    if (!(!(_e84))) {
        float _e91 = fsr.frame_index;
        local = _e91 == 0.0;
    } else {
        local = true;
    }
    bool _e95 = local;
    params.new_sample = _e95;
    metal::float2 _e99 = params.lr_uv_hw;
    metal::float4 masks_1 = masks_tex.sample(lin, _e99, metal::level(0.0));
    float _e104 = sat(masks_1.x);
    params.reactive = _e104;
    float _e107 = sat(masks_1.y);
    params.disocclusion = _e107;
    float _e110 = sat(masks_1.z);
    params.shading_change = _e110;
    float _e113 = sat(masks_1.w);
    params.accumulation = _e113;
    float _e115 = params.accumulation;
    float _e117 = params.accumulation;
    params.accumulation = _e115 * static_cast<float>(metal::rint(_e117 * 100.0) > 1.0);
    data.upsampled_color = metal::float3(0.0);
    data.history_color = metal::float3(0.0);
    data.history_weight = 1.0;
    data.upsampled_weight = 0.0;
    data.lock = 0.0;
    data.lock_contribution = 0.0;
    bool _e141 = params.existing_sample;
    if (_e141) {
        bool _e145 = params.new_sample;
        local_1 = !(_e145);
    } else {
        local_1 = false;
    }
    bool _e148 = local_1;
    if (_e148) {
        Params _e149 = params;
        reproject_history_color(_e149, data, fsr, t5_);
    }
    Params _e150 = params;
    update_lock_status(_e150, data, fsr, new_locks, _buffer_sizes);
    Params _e151 = params;
    compute_base_accumulation_weight(_e151, data, fsr);
    Params _e152 = params;
    compute_upsampled_color_and_weight(_e152, data, fsr, t4_);
    Params _e153 = params;
    rectify_history(_e153, data);
    accumulate(data);
    metal::float3 _e155 = data.history_color;
    float _e158 = fsr.exposure;
    metal::float3 color = metal::max(_e155 / metal::float3(_e158), metal::float3(0.0));
    int _e169 = fsr.upscale_size.x;
    new_locks[metal::min(unsigned(as_type<int>(as_type<uint>(as_type<int>(as_type<uint>(_e1.y) * as_type<uint>(_e169))) + as_type<uint>(_e1.x))), (_buffer_sizes.size8 - 0 - 4) / 4)] = 0.0;
    metal::float2 _e179 = fsr.downscale;
    metal::int2 _e186 = fsr.render_size;
    metal::int2 _e187 = clamp_load(naga_f2i32(static_cast<metal::float2>(_e1) * _e179), metal::int2(0), _e186);
    uint clamped_lod_e189 = metal::min(uint(0), t4_.get_num_mip_levels() - 1);
    metal::float4 _e189 = t4_.read(metal::min(metal::uint2(_e187), metal::uint2(t4_.get_width(clamped_lod_e189), t4_.get_height(clamped_lod_e189)) - 1), clamped_lod_e189);
    float glow = _e189.w;
    float _e194 = data.lock;
    o_2.internal = metal::float4(color, _e194);
    o_2.color = metal::float4(color, glow);
    AccumulateOut _e198 = o_2;
    const auto _tmp_2 = _e198;
    return fs_accumulateOutput { _tmp_2.internal, _tmp_2.color };
}


struct fs_rcasInput {
};
struct fs_rcasOutput {
    metal::float4 member_3 [[color(0)]];
};
fragment fs_rcasOutput fs_rcas(
  metal::float4 clip_3 [[position]]
, constant FsrParams& fsr [[buffer(0)]]
, metal::texture2d<float, metal::access::sample> t4_ [[texture(4)]]
, metal::texture2d<float, metal::access::sample> t5_ [[texture(5)]]
) {
    const VsOut in_2 = { clip_3 };
    float nz = {};
    float lobe = {};
    metal::int2 _e1 = px_of(in_2);
    metal::float3 _e6 = rcas_load(as_type<metal::int2>(as_type<metal::uint2>(_e1) + as_type<metal::uint2>(metal::int2(0, -1))), fsr, t5_);
    metal::float3 _e11 = rcas_load(as_type<metal::int2>(as_type<metal::uint2>(_e1) + as_type<metal::uint2>(metal::int2(-1, 0))), fsr, t5_);
    metal::float3 _e12 = rcas_load(_e1, fsr, t5_);
    metal::float3 _e17 = rcas_load(as_type<metal::int2>(as_type<metal::uint2>(_e1) + as_type<metal::uint2>(metal::int2(1, 0))), fsr, t5_);
    metal::float3 _e22 = rcas_load(as_type<metal::int2>(as_type<metal::uint2>(_e1) + as_type<metal::uint2>(metal::int2(0, 1))), fsr, t5_);
    float bl = (_e6.z * 0.5) + ((_e6.x * 0.5) + _e6.y);
    float dl = (_e11.z * 0.5) + ((_e11.x * 0.5) + _e11.y);
    float el = (_e12.z * 0.5) + ((_e12.x * 0.5) + _e12.y);
    float fl = (_e17.z * 0.5) + ((_e17.x * 0.5) + _e17.y);
    float hl = (_e22.z * 0.5) + ((_e22.x * 0.5) + _e22.y);
    nz = ((((0.25 * bl) + (0.25 * dl)) + (0.25 * fl)) + (0.25 * hl)) - el;
    float _e81 = nz;
    float _e83 = max3_(bl, dl, el);
    float _e84 = max3_(_e83, fl, hl);
    float _e85 = min3_(bl, dl, el);
    float _e86 = min3_(_e85, fl, hl);
    float _e89 = sat(metal::abs(_e81) / (_e84 - _e86));
    nz = _e89;
    float _e90 = nz;
    nz = (-0.5 * _e90) + 1.0;
    metal::float3 mn4_ = metal::min(metal::min(metal::min(_e6, _e11), _e17), _e22);
    metal::float3 mx4_ = metal::max(metal::max(metal::max(_e6, _e11), _e17), _e22);
    metal::float2 peak = metal::float2(1.0, -4.0);
    metal::float3 hit_min = mn4_ / (4.0 * mx4_);
    metal::float3 hit_max = (metal::float3(peak.x) - mx4_) / ((4.0 * mn4_) + metal::float3(peak.y));
    metal::float3 lobe3_ = metal::max(-(hit_min), hit_max);
    float _e122 = max3_(lobe3_.x, lobe3_.y, lobe3_.z);
    float _e128 = fsr.rcas;
    lobe = metal::max(-0.1875, metal::min(_e122, 0.0)) * _e128;
    float _e131 = lobe;
    float _e132 = nz;
    lobe = _e131 * _e132;
    float _e134 = lobe;
    float rcp_l = 1.0 / ((4.0 * _e134) + 1.0);
    float _e141 = lobe;
    float _e143 = lobe;
    float _e146 = lobe;
    float _e149 = lobe;
    float _e156 = fsr.exposure;
    metal::float3 c_10 = ((((((_e141 * _e6) + (_e143 * _e11)) + (_e146 * _e22)) + (_e149 * _e17)) + _e12) * rcp_l) / metal::float3(_e156);
    metal::float2 _e163 = fsr.downscale;
    metal::int2 _e170 = fsr.render_size;
    metal::int2 _e171 = clamp_load(naga_f2i32(static_cast<metal::float2>(_e1) * _e163), metal::int2(0), _e170);
    uint clamped_lod_e173 = metal::min(uint(0), t4_.get_num_mip_levels() - 1);
    metal::float4 _e173 = t4_.read(metal::min(metal::uint2(_e171), metal::uint2(t4_.get_width(clamped_lod_e173), t4_.get_height(clamped_lod_e173)) - 1), clamped_lod_e173);
    float glow_1 = _e173.w;
    return fs_rcasOutput { metal::float4(c_10, glow_1) };
}
