/**
 * @file almetalaurora.mm
 * @brief Native Metal graph for Aurora ee458fb's WGSL port of FSR 3.1 (RCAS OFF).
 * Native host derived from Aurora's pinned fsr.rs, GPL-3.0-or-later.
 * Additional native integration Copyright (C) 2026, Alchemy Viewer Project.
 * The generated algorithm is separately covered by retained upstream notices
 * in shaders/aurora/NOTICE.md and the complete vendored licence texts.
 */
#include "almetalaurora.h"
#include "almetalworld.h"
#include "shaders/aurora/sources.h"
#import <Metal/Metal.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
namespace RP = ALMetalResourcePlanning;
struct FsrU
{
    float        motion_reproj[16];
    std::int32_t render_size[2], prev_render_size[2], upscale_size[2], prev_upscale_size[2];
    std::int32_t max_render_size[2], max_upscale_size[2];
    float        device_to_view[4], jitter[2], prev_jitter[2], downscale[2];
    float        jitter_sequence_length, delta_pre_exposure, frame_index, velocity_factor;
    float        accumulation_added_per_frame, min_disocclusion_accumulation, exposure, rcas;
    float        view_to_meters, motion_on, reactive_on, composition_on, pad[2];
};
static_assert(sizeof(FsrU) == 208);
using T = std::unique_ptr<ALMetalTexture>;
using B = std::unique_ptr<ALMetalBuffer>;
id<MTLTexture> tex(const ALMetalTexture* t)
{
    return t ? (__bridge id<MTLTexture>)t->nativeHandle() : nil;
}
id<MTLBuffer> buf(const ALMetalBuffer* b)
{
    return b ? (__bridge id<MTLBuffer>)b->nativeHandle() : nil;
}
bool fail(std::string* error, const std::string& message)
{
    if (error)
        *error = message;
    return false;
}
// These two small integration adapters are not changes to the reconstruction.
// They convert Alchemy's public input conventions and preserve its glow contract.
constexpr const char* adapter_source = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct U { float4x4 reproj; int2 size,prev_size,out_size,prev_out,max_size,max_out; float4 depth; float2 jitter,prev_jitter,downscale; };
struct V { float4 pos [[position]]; };
vertex V aurora_vertex(uint i [[vertex_id]]) { return {float4(float((i<<1)&2)*2-1,float(i&2)*2-1,0,1)}; }
struct A { float4 motion [[color(0)]]; float depth [[depth(any)]]; };
A adapt(uint2 p, float d, float2 m, float marker, constant U& u) {
 A a; a.motion=float4(marker>0 ? 2.0f:0.0f,0,-m/float2(u.size)); a.depth=d; return a;
}
fragment A aurora_adapter_r32(V v [[stage_in]], texture2d<float,access::read> d [[texture(0)]], texture2d<float,access::read> m [[texture(1)]], texture2d<float,access::read> c [[texture(2)]], constant U& u [[buffer(0)]]) {
 uint2 p=uint2(v.pos.xy); return adapt(p,d.read(p).r,m.read(p).xy,c.read(p).r,u);
}
fragment A aurora_adapter_depth(V v [[stage_in]], depth2d<float,access::read> d [[texture(0)]], texture2d<float,access::read> m [[texture(1)]], texture2d<float,access::read> c [[texture(2)]], constant U& u [[buffer(0)]]) {
 uint2 p=uint2(v.pos.xy); return adapt(p,d.read(p),m.read(p).xy,c.read(p).r,u);
}
fragment float4 aurora_glow(V v [[stage_in]],texture2d<float> reconstructed [[texture(0)]],texture2d<float> original [[texture(1)]],sampler lin [[sampler(0)]],constant U& u [[buffer(0)]]) {
 float2 uv=v.pos.xy/float2(u.out_size); return float4(reconstructed.sample(lin,uv).rgb,original.sample(lin,uv+u.jitter/float2(u.size)).a);
}
)MSL";
struct ResourceScope
{
    ALMetalCommandBuffer& command;
    std::uint64_t         identity = 0;
    bool                  active   = false;
    ResourceScope(ALMetalCommandBuffer& c, std::string* error) : command(c)
    {
        active = c.beginResourcePass(RP::Pass::render, identity, error);
    }
    ~ResourceScope()
    {
        if (active)
            command.endResourcePass(identity);
    }
    bool end(std::string* error)
    {
        if (!active)
            return false;
        active = false;
        return command.endResourcePass(identity, error);
    }
};
} // namespace

struct ALMetalAuroraReconstructor::Impl
{
    struct Entry
    {
        ALMetalAuroraDescriptor      d;
        T                            adapted_depth, adapted_motion, empty_composition, reactive, dilated_motion, dilated_depth, farthest;
        T                            luma[2], farthest_mip1, spd, shading_change, masks, accumulation[2], luma_history[2], instability;
        T                            internal[2], output, display_depth;
        std::vector<id<MTLTexture>>  spd_views;
        B                            reconstructed, new_locks;
        bool                         initialized   = false;
        std::uint64_t                texture_bytes = 0, buffer_bytes = 0;
        std::array<ALMetalTexture*, 22> textures() const
        {
            return { adapted_depth.get(),
                     adapted_motion.get(),
                     empty_composition.get(),
                     reactive.get(),
                     dilated_motion.get(),
                     dilated_depth.get(),
                     farthest.get(),
                     luma[0].get(),
                     luma[1].get(),
                     farthest_mip1.get(),
                     spd.get(),
                     shading_change.get(),
                     masks.get(),
                     accumulation[0].get(),
                     accumulation[1].get(),
                     luma_history[0].get(),
                     luma_history[1].get(),
                     instability.get(),
                     internal[0].get(),
                     internal[1].get(),
                     output.get(),
                     display_depth.get() };
        }
    };
    const ALMetalDevice*                   device      = nullptr;
    id<MTLDevice>                          native      = nil;
    id<MTLCommandQueue>                    queue       = nil;
    id<MTLSamplerState>                    sampler     = nil;
    id<MTLDepthStencilState>               depth_state = nil;
    id<MTLRenderPipelineState>             adapter_r32 = nil, adapter_depth = nil, glow = nil;
    id<MTLRenderPipelineState>             autogen = nil, prepare = nil, farthest = nil, change0 = nil, downsample = nil, change = nil;
    id<MTLRenderPipelineState>             reactivity = nil, instability = nil, accumulate = nil, display = nil;
    std::shared_ptr<ALMetalWorldFrameData> frame_data;
    bool                                   own_frame_data = true;
    std::uint64_t                          generation     = 0;
    std::shared_ptr<Entry>                 current;
    ALMetalAuroraStats                     counters;
    bool                                   needs_reset = true;
    std::uint32_t                          parity      = 0;
    float                                  frame_index = 0.f, prev_jitter[2]{};
    Ticket                                 next_ticket = 1, encoded_ticket = 0;
    ALMetalAuroraFrame                     encoded_frame;
    std::shared_ptr<const ALMetalCommandCompletion>              encoded_completion;
    std::vector<std::shared_ptr<const ALMetalCommandCompletion>> submitted;
    bool                                                         encoded_reset            = false;
    bool                                                         encoded_failure_observed = false;
    id<MTLLibrary>                                               library(const char* source, std::string* error)
    {
        NSError* native_error      = nil;
        auto     options           = [MTLCompileOptions new];
        options.languageVersion    = MTLLanguageVersion4_0;
        options.preserveInvariance = YES; // same as pinned wgpu Metal backend
        auto result = [native newLibraryWithSource:[NSString stringWithUTF8String:source] options:options error:&native_error];
        if (!result)
            fail(error, "Aurora shader compilation: " + std::string(native_error.localizedDescription.UTF8String ?: "unknown error"));
        return result;
    }
    id<MTLRenderPipelineState> pipeline(id<MTLLibrary> lib, const char* entry, std::initializer_list<MTLPixelFormat> colors, bool depth,
                                        std::string* error, const char* vertex = "vs_fsr")
    {
        auto d             = [MTLRenderPipelineDescriptor new];
        d.label            = [NSString stringWithUTF8String:entry];
        d.vertexFunction   = [lib newFunctionWithName:[NSString stringWithUTF8String:vertex]];
        d.fragmentFunction = [lib newFunctionWithName:[NSString stringWithUTF8String:entry]];
        // Metal permits a vertex-only pipeline even with colour attachments.
        // Missing translated entry points must reject preparation explicitly,
        // rather than silently turn a reconstruction stage into a clear.
        if (!d.vertexFunction || !d.fragmentFunction)
        {
            fail(error, "Aurora translated pipeline function missing: " + std::string(entry));
            return nil;
        }
        std::size_t i = 0;
        for (auto f : colors)
            d.colorAttachments[i++].pixelFormat = f;
        if (depth)
            d.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
        NSError* e      = nil;
        auto     result = [native newRenderPipelineStateWithDescriptor:d error:&e];
        if (!result)
            fail(error,
                 "Aurora pipeline " + std::string(entry) + ": " + std::string(e.localizedDescription.UTF8String ?: "missing function"));
        return result;
    }
    bool pipelines(std::string* error)
    {
        auto a = library(adapter_source, error);
        if (!a)
            return false;
        adapter_r32   = pipeline(a, "aurora_adapter_r32", { MTLPixelFormatRGBA16Float }, true, error, "aurora_vertex");
        adapter_depth = pipeline(a, "aurora_adapter_depth", { MTLPixelFormatRGBA16Float }, true, error, "aurora_vertex");
        glow          = pipeline(a, "aurora_glow", { MTLPixelFormatRGBA16Float }, false, error, "aurora_vertex");
        auto p        = library(alAuroraShaders::prepare_inputs, error);
        if (!p)
            return false;
        prepare = pipeline(p, alAuroraShaders::prepare_inputs_fs_prepare_inputs,
                           { MTLPixelFormatRG16Float, MTLPixelFormatR32Float, MTLPixelFormatR16Float, MTLPixelFormatR16Float }, false,
                           error, alAuroraShaders::prepare_inputs_vs_fsr);
        display = pipeline(p, alAuroraShaders::prepare_inputs_fs_display_depth, {}, true, error, alAuroraShaders::prepare_inputs_vs_fsr);
        auto y  = library(alAuroraShaders::pyramids, error);
        if (!y)
            return false;
        autogen    = pipeline(y, alAuroraShaders::pyramids_fs_autogen_reactive, { MTLPixelFormatR8Unorm }, false, error,
                              alAuroraShaders::pyramids_vs_fsr);
        farthest   = pipeline(y, alAuroraShaders::pyramids_fs_farthest_depth_mip1, { MTLPixelFormatR16Float }, false, error,
                              alAuroraShaders::pyramids_vs_fsr);
        change0    = pipeline(y, alAuroraShaders::pyramids_fs_shading_change_mip0, { MTLPixelFormatRG16Float }, false, error,
                              alAuroraShaders::pyramids_vs_fsr);
        downsample = pipeline(y, alAuroraShaders::pyramids_fs_downsample, { MTLPixelFormatRG16Float }, false, error,
                              alAuroraShaders::pyramids_vs_fsr);
        change     = pipeline(y, alAuroraShaders::pyramids_fs_shading_change, { MTLPixelFormatR8Unorm }, false, error,
                              alAuroraShaders::pyramids_vs_fsr);
        auto r     = library(alAuroraShaders::reactivity, error);
        if (!r)
            return false;
        reactivity = pipeline(r, alAuroraShaders::reactivity_fs_prepare_reactivity, { MTLPixelFormatRGBA8Unorm, MTLPixelFormatR8Unorm },
                              false, error, alAuroraShaders::reactivity_vs_fsr);
        auto c     = library(alAuroraShaders::accumulate, error);
        if (!c)
            return false;
        instability = pipeline(c, alAuroraShaders::accumulate_fs_luma_instability, { MTLPixelFormatRGBA16Float, MTLPixelFormatR16Float },
                               false, error, alAuroraShaders::accumulate_vs_fsr);
        accumulate = pipeline(c, alAuroraShaders::accumulate_fs_accumulate, { MTLPixelFormatRGBA16Float, MTLPixelFormatRGBA16Float }, false,
                              error, alAuroraShaders::accumulate_vs_fsr);
        return adapter_r32 && adapter_depth && glow && prepare && display && autogen && farthest && change0 && downsample && change &&
               reactivity && instability && accumulate;
    }
    void invalidate()
    {
        needs_reset            = true;
        counters.history_valid = false;
    }
    void poll()
    {
        std::size_t keep = 0;
        for (const auto& token : submitted)
        {
            const auto status = token->status();
            if (status == ALMetalCommandStatus::error || status == ALMetalCommandStatus::unknown)
            {
                ++counters.gpu_failures;
                invalidate();
                if (current)
                    current->initialized = false;
            }
            else if (!token->isComplete())
                submitted[keep++] = token;
        }
        submitted.resize(keep);
    }
};

std::unique_ptr<ALMetalAuroraReconstructor> ALMetalAuroraReconstructor::create(const ALMetalDevice& device, std::string* error)
{
    try
    {
        @try
        {
            auto impl    = std::make_unique<Impl>();
            impl->device = &device;
            impl->native = (__bridge id<MTLDevice>)device.nativeHandle();
            impl->queue  = (__bridge id<MTLCommandQueue>)device.nativeCommandQueue();
            if (!impl->native || !impl->queue)
            {
                fail(error, "Aurora needs the existing native Metal device and queue");
                return nullptr;
            }
            auto sampler               = [MTLSamplerDescriptor new];
            sampler.minFilter          = MTLSamplerMinMagFilterLinear;
            sampler.magFilter          = MTLSamplerMinMagFilterLinear;
            sampler.mipFilter          = MTLSamplerMipFilterLinear;
            sampler.sAddressMode       = MTLSamplerAddressModeClampToEdge;
            sampler.tAddressMode       = MTLSamplerAddressModeClampToEdge;
            impl->sampler              = [impl->native newSamplerStateWithDescriptor:sampler];
            auto depth                 = [MTLDepthStencilDescriptor new];
            depth.depthCompareFunction = MTLCompareFunctionAlways;
            depth.depthWriteEnabled    = YES;
            impl->depth_state          = [impl->native newDepthStencilStateWithDescriptor:depth];
            impl->frame_data           = ALMetalWorldFrameData::create(device, error);
            impl->submitted.reserve(8);
            if (!impl->sampler || !impl->depth_state || !impl->frame_data || !impl->pipelines(error))
                return nullptr;
            return std::unique_ptr<ALMetalAuroraReconstructor>(new ALMetalAuroraReconstructor(std::move(impl)));
        }
        @catch (NSException* exception)
        {
            fail(error, "Aurora native preparation exception: " + std::string(exception.reason.UTF8String ?: "unknown"));
            return nullptr;
        }
    }
    catch (const std::exception& e)
    {
        fail(error, std::string("Could not create Aurora native owner: ") + e.what());
        return nullptr;
    }
}
ALMetalAuroraReconstructor::ALMetalAuroraReconstructor(std::unique_ptr<Impl> impl) : mImpl(std::move(impl))
{
}
ALMetalAuroraReconstructor::~ALMetalAuroraReconstructor() = default;

bool ALMetalAuroraReconstructor::prepare(const ALMetalAuroraDescriptor& d, std::string* error)
{
    auto& impl = *mImpl;
    impl.poll();
    if (impl.encoded_ticket)
        return fail(error, "Settle the encoded Aurora transaction before preparing another world");
    if (impl.current && impl.current->d == d)
    {
        ++impl.counters.prepare_reuses;
        return true;
    }
    if (!d.input_width || !d.input_height || d.input_width > d.output_width || d.input_height > d.output_height ||
        d.output_width > impl.device->info().max_texture_2d_dimension || d.output_height > impl.device->info().max_texture_2d_dimension)
        return fail(error, "Invalid Aurora scene/output dimensions");
    try
    {
        @try
        {
            auto e    = std::make_shared<Impl::Entry>();
            e->d      = d;
            auto make = [&](T& t, std::uint32_t w, std::uint32_t h, ALMetalPixelFormat f, const char* label, std::uint32_t levels = 1)
            {
                ALMetalTextureDescriptor desc;
                desc.width              = w;
                desc.height             = h;
                desc.format             = f;
                desc.mipmap_level_count = levels;
                desc.usage              = ALMetalTextureUsage::shader_read | ALMetalTextureUsage::render_target;
                desc.storage            = ALMetalStorageMode::private_gpu;
                desc.label              = label;
                t                       = ALMetalTexture::create(*impl.device, desc, error);
                if (t)
                    e->texture_bytes += t->estimatedAllocationBytes();
                return bool(t);
            };
            const auto w = d.input_width, h = d.input_height, ow = d.output_width, oh = d.output_height;
            const auto hw = std::max(w / 2, 1u), hh = std::max(h / 2, 1u);
            auto       levels = std::min(3u, 32u - std::uint32_t(__builtin_clz(std::max(hw, hh))));
            if (!make(e->adapted_depth, w, h, ALMetalPixelFormat::depth32_float, "Aurora input reverse-Z depth") ||
                !make(e->adapted_motion, w, h, ALMetalPixelFormat::rgba16_float, "Aurora UV motion and composition") ||
                !make(e->empty_composition, w, h, ALMetalPixelFormat::r8_unorm, "Aurora zero static composition") ||
                !make(e->reactive, w, h, ALMetalPixelFormat::r8_unorm, "Aurora generated reactive") ||
                !make(e->dilated_motion, w, h, ALMetalPixelFormat::rg16_float, "Aurora dilated motion") ||
                !make(e->dilated_depth, w, h, ALMetalPixelFormat::r32_float, "Aurora dilated depth") ||
                !make(e->farthest, w, h, ALMetalPixelFormat::r16_float, "Aurora farthest depth") ||
                !make(e->farthest_mip1, hw, hh, ALMetalPixelFormat::r16_float, "Aurora farthest half") ||
                !make(e->spd, hw, hh, ALMetalPixelFormat::rg16_float, "Aurora shading pyramid", levels) ||
                !make(e->shading_change, hw, hh, ALMetalPixelFormat::r8_unorm, "Aurora shading change") ||
                !make(e->masks, w, h, ALMetalPixelFormat::rgba8_unorm, "Aurora masks") ||
                !make(e->instability, w, h, ALMetalPixelFormat::r16_float, "Aurora luma instability") ||
                !make(e->output, ow, oh, ALMetalPixelFormat::rgba16_float, "Aurora immediate output") ||
                !make(e->display_depth, ow, oh, ALMetalPixelFormat::depth32_float, "Aurora reconstructed display depth"))
                return false;
            for (std::uint32_t i = 0; i < 2; ++i)
                if (!make(e->luma[i], w, h, ALMetalPixelFormat::r16_float, "Aurora luma history") ||
                    !make(e->accumulation[i], w, h, ALMetalPixelFormat::r8_unorm, "Aurora accumulation history") ||
                    !make(e->luma_history[i], w, h, ALMetalPixelFormat::rgba16_float, "Aurora luma stability history") ||
                    !make(e->internal[i], ow, oh, ALMetalPixelFormat::rgba16_float, "Aurora internal history"))
                    return false;
            for (std::uint32_t i = 0; i < levels; ++i)
            {
                auto native = tex(e->spd.get());
                auto v      = [native newTextureViewWithPixelFormat:native.pixelFormat
                                                   textureType:MTLTextureType2D
                                                        levels:NSMakeRange(i, 1)
                                                        slices:NSMakeRange(0, 1)];
                if (!v)
                    return fail(error, "Aurora pyramid mip view creation failed");
                e->spd_views.push_back(v);
            }
            auto buffer = [&](B& b, std::size_t bytes, const char* label)
            {
                ALMetalBufferDescriptor desc;
                desc.length  = bytes;
                desc.storage = ALMetalStorageMode::private_gpu;
                desc.label   = label;
                b            = ALMetalBuffer::create(*impl.device, desc, error);
                if (b)
                    e->buffer_bytes += bytes;
                return bool(b);
            };
            if (!buffer(e->reconstructed, std::size_t(w) * h * 4, "Aurora reconstructed previous depth") ||
                !buffer(e->new_locks, std::size_t(ow) * oh * 4, "Aurora new locks"))
                return false;
            impl.current        = std::move(e);
            impl.parity         = 0;
            impl.frame_index    = 0;
            impl.prev_jitter[0] = impl.prev_jitter[1] = 0;
            impl.invalidate();
            ++impl.counters.preparations;
            ++impl.counters.creation_successes;
            return true;
        }
        @catch (NSException* exception)
        {
            return fail(error, "Aurora resource preparation exception: " + std::string(exception.reason.UTF8String ?: "unknown"));
        }
    }
    catch (const std::exception& e)
    {
        return fail(error, std::string("Aurora resource preparation failed: ") + e.what());
    }
}

bool ALMetalAuroraReconstructor::encode(ALMetalCommandBuffer& command, const ALMetalAuroraDescriptor& d, const ALMetalAuroraInputs& input,
                                        const ALMetalAuroraFrame& frame, Ticket& ticket, std::string* error)
{
    ticket = 0;
    if (!prepare(d, error))
        return false;
    auto&      impl = *mImpl;
    const auto e    = impl.current;
    if (!std::isfinite(frame.jitter_x) || !std::isfinite(frame.jitter_y) || std::abs(frame.jitter_x) > .5f ||
        std::abs(frame.jitter_y) > .5f)
        return fail(error, "Aurora raster jitter must be finite input pixels within half a pixel");
    for (float v : frame.projection)
        if (!std::isfinite(v))
            return fail(error, "Aurora projection must be finite");
    for (float v : frame.motion_reprojection)
        if (!std::isfinite(v))
            return fail(error, "Aurora reprojection must be finite");
    auto compatible = [&](const ALMetalTexture* t, std::uint32_t w, std::uint32_t h, ALMetalPixelFormat f)
    {
        if (!t)
            return false;
        const auto& td = t->descriptor();
        auto        n  = tex(t);
        return n && n.device == impl.native && td.width == w && td.height == h && td.format == f && td.sample_count == 1 &&
               td.type == ALMetalTextureType::texture_2d && td.array_length == 1 && td.mipmap_level_count == 1 &&
               td.hazard_tracking == ALMetalHazardTrackingMode::tracked && n.hazardTrackingMode == MTLHazardTrackingModeTracked &&
               (n.usage & MTLTextureUsageShaderRead) && n.storageMode != MTLStorageModeMemoryless;
    };
    const bool depth_valid = compatible(input.depth, d.input_width, d.input_height, ALMetalPixelFormat::r32_float) ||
                             compatible(input.depth, d.input_width, d.input_height, ALMetalPixelFormat::depth32_float);
    if (!compatible(input.color, d.input_width, d.input_height, ALMetalPixelFormat::rgba16_float) ||
        !compatible(input.opaque, d.input_width, d.input_height, ALMetalPixelFormat::rgba16_float) || !depth_valid ||
        !compatible(input.motion, d.input_width, d.input_height, ALMetalPixelFormat::rg16_float) ||
        !compatible(input.output, d.output_width, d.output_height, ALMetalPixelFormat::rgba16_float) ||
        !(tex(input.output).usage & MTLTextureUsageRenderTarget) ||
        (input.composition && !compatible(input.composition, d.input_width, d.input_height, ALMetalPixelFormat::r8_unorm)))
        return fail(error, "Aurora requires compatible HDR opaque/final, reverse depth, pixel motion, composition and output textures");
    if (input.output->nativeHandle() == input.color->nativeHandle() || input.output->nativeHandle() == input.opaque->nativeHandle())
        return fail(error, "Aurora reconstruction output must not alias its scene inputs");
    if (command.isCommitted() || command.nativeDevice() != (__bridge void*)impl.native)
        return fail(error, "Aurora needs the owner's uncommitted native main-queue command");
    const auto completion = command.completionToken();
    if (!completion || !command.retainUntilCompletion(e, error))
        return fail(error, "Aurora resource lease retention failed");
    try
    {
        impl.submitted.reserve(impl.submitted.size() + 1);
    }
    catch (...)
    {
        return fail(error, "Aurora completion reservation failed");
    }
    const auto w = impl.parity, r = 1 - w;
    const bool reset = frame.reset || impl.needs_reset || !impl.counters.history_valid;
    FsrU       u{};
    std::copy(frame.motion_reprojection.begin(), frame.motion_reprojection.end(), u.motion_reproj);
    for (auto dst : { u.render_size, u.prev_render_size, u.max_render_size })
    {
        dst[0] = d.input_width;
        dst[1] = d.input_height;
    }
    for (auto dst : { u.upscale_size, u.prev_upscale_size, u.max_upscale_size })
    {
        dst[0] = d.output_width;
        dst[1] = d.output_height;
    }
    u.device_to_view[0]      = -frame.projection[10] - std::numeric_limits<float>::epsilon();
    u.device_to_view[1]      = frame.projection[14];
    u.jitter[0]              = frame.jitter_x;
    u.jitter[1]              = frame.jitter_y;
    u.prev_jitter[0]         = reset ? frame.jitter_x : impl.prev_jitter[0];
    u.prev_jitter[1]         = reset ? frame.jitter_y : impl.prev_jitter[1];
    u.downscale[0]           = float(d.input_width) / d.output_width;
    u.downscale[1]           = float(d.input_height) / d.output_height;
    const float ratio        = float(d.output_width) / d.input_width;
    u.jitter_sequence_length = float(frame.jitter_phase_count ? frame.jitter_phase_count : std::max(1u, std::uint32_t(8 * ratio * ratio)));
    u.delta_pre_exposure     = 1;
    u.frame_index            = reset ? 0 : impl.frame_index;
    u.velocity_factor        = 1;
    u.accumulation_added_per_frame  = 1.f / 3;
    u.min_disocclusion_accumulation = -1.f / 3;
    u.exposure                      = 1;
    u.rcas                          = .25f;
    u.view_to_meters                = 1;
    u.motion_on = u.reactive_on = u.composition_on = 1;
    if (impl.own_frame_data)
        impl.frame_data->beginFrame(++impl.generation);
    ALMetalWorldDataSlice constants;
    if (!impl.frame_data->publish(command, &u, sizeof(u), constants, error, false))
        return false;
    ResourceScope scope(command, error);
    if (!scope.active)
        return false;
    for (auto t : { input.color, input.opaque, input.depth, input.motion })
        if (!command.useResource(t->resourceView(), RP::Access::read, error))
            return false;
    if (input.composition && !command.useResource(input.composition->resourceView(), RP::Access::read, error))
        return false;
    if (!command.useResource(input.output->resourceView(), RP::Access::write, error) ||
        !command.useResource(constants.buffer->resourceView(), RP::Access::read, error))
        return false;
    for (auto t : e->textures())
        if (!command.useResource(t->resourceView(), RP::Access::read_write, error))
            return false;
    for (auto b : { e->reconstructed.get(), e->new_locks.get() })
        if (!command.useResource(b->resourceView(), RP::Access::read_write, error))
            return false;
    // Raw native encoders retain the existing opaque-resource safeguard.
    // The main renderer pre-acquires all downstream post targets before this
    // point, exactly as its existing MetalFX Temporal transaction does.
    auto native = (__bridge id<MTLCommandBuffer>)command.nativeHandle();
    if (!native || native.commandQueue != impl.queue || !native.retainedReferences)
        return fail(error, "Aurora native command queue/retention mismatch");
    auto clear = [&](id<MTLTexture> t, std::uint32_t mip = 0)
    {
        auto p = [MTLRenderPassDescriptor renderPassDescriptor];
        if (t.pixelFormat == MTLPixelFormatDepth32Float)
        {
            p.depthAttachment.texture     = t;
            p.depthAttachment.level       = mip;
            p.depthAttachment.loadAction  = MTLLoadActionClear;
            p.depthAttachment.storeAction = MTLStoreActionStore;
            p.depthAttachment.clearDepth  = 0;
        }
        else
        {
            p.colorAttachments[0].texture     = t;
            p.colorAttachments[0].level       = mip;
            p.colorAttachments[0].loadAction  = MTLLoadActionClear;
            p.colorAttachments[0].storeAction = MTLStoreActionStore;
            p.colorAttachments[0].clearColor  = MTLClearColorMake(0, 0, 0, 0);
        }
        auto encoder = [native renderCommandEncoderWithDescriptor:p];
        if (!encoder)
            return false;
        encoder.label = @"Aurora history initialization";
        [encoder endEncoding];
        return true;
    };
    auto run = [&](const char* label, id<MTLRenderPipelineState> pipeline, std::initializer_list<id<MTLTexture>> targets,
                   std::initializer_list<std::pair<std::uint32_t, id<MTLTexture>>> textures,
                   std::initializer_list<std::pair<std::uint32_t, id<MTLBuffer>>> buffers = {}, id<MTLTexture> depth = nil,
                   bool reactive_sizes = false, bool accumulation_sizes = false)
    {
        auto          p     = [MTLRenderPassDescriptor renderPassDescriptor];
        std::uint32_t index = 0;
        for (auto t : targets)
        {
            p.colorAttachments[index].texture     = t;
            p.colorAttachments[index].loadAction  = MTLLoadActionClear;
            p.colorAttachments[index].storeAction = MTLStoreActionStore;
            p.colorAttachments[index].clearColor  = MTLClearColorMake(0, 0, 0, 0);
            ++index;
        }
        if (depth)
        {
            p.depthAttachment.texture     = depth;
            p.depthAttachment.loadAction  = MTLLoadActionClear;
            p.depthAttachment.storeAction = MTLStoreActionStore;
            p.depthAttachment.clearDepth  = 0;
        }
        const auto target = targets.size() ? *targets.begin() : depth;
        auto       enc    = [native renderCommandEncoderWithDescriptor:p];
        if (!enc)
            return false;
        enc.label = [NSString stringWithUTF8String:label];
        [enc setRenderPipelineState:pipeline];
        [enc setCullMode:MTLCullModeNone];
        [enc setViewport:MTLViewport{ 0, 0, double(target.width), double(target.height), 0, 1 }];
        [enc setScissorRect:MTLScissorRect{ 0, 0, target.width, target.height }];
        if (depth)
            [enc setDepthStencilState:impl.depth_state];
        [enc setFragmentBuffer:buf(constants.buffer) offset:constants.offset atIndex:0];
        [enc setFragmentSamplerState:impl.sampler atIndex:0];
        for (auto [binding, t] : textures)
            [enc setFragmentTexture:t atIndex:binding];
        for (auto [binding, b] : buffers)
            [enc setFragmentBuffer:b offset:0 atIndex:binding];
        const std::uint32_t sizes[2] = { std::uint32_t(reactive_sizes       ? e->reconstructed->length()
                                                       : accumulation_sizes ? e->new_locks->length()
                                                                            : e->reconstructed->length()),
                                         std::uint32_t(e->new_locks->length()) };
        [enc setFragmentBytes:sizes length:sizeof(sizes) atIndex:30];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [enc endEncoding];
        return true;
    };
    @try
    {
        if (!e->initialized)
        {
            for (auto t : e->textures())
                for (std::uint32_t mip = 0; mip < t->descriptor().mipmap_level_count; ++mip)
                    if (!clear(tex(t), mip))
                    {
                        impl.invalidate();
                        return fail(error, "Aurora initial texture clear failed");
                    }
        }
        else if (reset)
            for (auto t : { e->accumulation[r].get(), e->luma_history[r].get(), e->luma[r].get() })
                if (!clear(tex(t)))
                {
                    impl.invalidate();
                    return fail(error, "Aurora reset clear failed");
                }
        auto blit = [native blitCommandEncoder];
        if (!blit)
        {
            impl.invalidate();
            return fail(error, "Aurora depth clear encoder failed");
        }
        blit.label = @"Aurora reconstructed previous-depth clear";
        [blit fillBuffer:buf(e->reconstructed.get()) range:NSMakeRange(0, e->reconstructed->length()) value:0];
        if (!e->initialized)
            [blit fillBuffer:buf(e->new_locks.get()) range:NSMakeRange(0, e->new_locks->length()) value:0];
        [blit endEncoding];
        const auto motion = tex(e->adapted_motion.get()), depth = tex(e->adapted_depth.get());
        const auto composition = tex(input.composition ? input.composition : e->empty_composition.get());
        bool       ok          = run("Aurora input convention adapter",
                      input.depth->descriptor().format == ALMetalPixelFormat::r32_float ? impl.adapter_r32 : impl.adapter_depth, { motion },
                                     { { 0, tex(input.depth) }, { 1, tex(input.motion) }, { 2, composition } }, {}, depth);
        ok                     = ok && run("Aurora autogen reactive", impl.autogen, { tex(e->reactive.get()) },
                                           { { 2, tex(input.opaque) }, { 3, tex(input.color) }, { 4, tex(input.color) } });
        ok                     = ok && run("Aurora prepare inputs", impl.prepare,
                                           { tex(e->dilated_motion.get()), tex(e->dilated_depth.get()), tex(e->farthest.get()), tex(e->luma[w].get()) },
                                           { { 2, tex(input.color) }, { 3, depth }, { 4, motion } }, { { 5, buf(e->reconstructed.get()) } });
        ok                     = ok && run("Aurora farthest depth", impl.farthest, { tex(e->farthest_mip1.get()) },
                                           { { 2, tex(e->farthest.get()) }, { 3, tex(e->farthest.get()) }, { 4, tex(e->farthest.get()) } });
        ok                     = ok && run("Aurora shading change pyramid", impl.change0, { e->spd_views[0] },
                                           { { 2, tex(e->luma[w].get()) }, { 3, tex(e->luma[r].get()) }, { 4, tex(e->dilated_motion.get()) } });
        for (std::size_t level = 1; level < e->spd_views.size() && ok; ++level)
            ok = run("Aurora downsample", impl.downsample, { e->spd_views[level] },
                     { { 2, e->spd_views[level - 1] }, { 3, e->spd_views[level - 1] }, { 4, e->spd_views[level - 1] } });
        ok = ok && run("Aurora shading change", impl.change, { tex(e->shading_change.get()) },
                       { { 2, tex(e->spd.get()) }, { 3, tex(e->spd.get()) }, { 4, tex(e->spd.get()) } });
        ok = ok && run("Aurora prepare reactivity", impl.reactivity, { tex(e->masks.get()), tex(e->accumulation[w].get()) },
                       { { 2, tex(e->dilated_motion.get()) },
                         { 3, tex(e->dilated_depth.get()) },
                         { 4, tex(e->shading_change.get()) },
                         { 5, tex(e->accumulation[r].get()) },
                         { 6, tex(e->luma[w].get()) },
                         { 9, tex(e->reactive.get()) },
                         { 10, motion } },
                       { { 7, buf(e->reconstructed.get()) }, { 8, buf(e->new_locks.get()) } }, nil, true);
        ok = ok && run("Aurora luma instability", impl.instability, { tex(e->luma_history[w].get()), tex(e->instability.get()) },
                       { { 2, tex(e->dilated_motion.get()) },
                         { 3, tex(e->masks.get()) },
                         { 4, tex(e->luma[w].get()) },
                         { 5, tex(e->luma_history[r].get()) },
                         { 6, tex(e->farthest_mip1.get()) },
                         { 7, tex(e->farthest_mip1.get()) } },
                       { { 8, buf(e->new_locks.get()) } }, nil, false, true);
        ok = ok && run("Aurora accumulate RCAS OFF", impl.accumulate, { tex(e->internal[w].get()), tex(e->output.get()) },
                       { { 2, tex(e->dilated_motion.get()) },
                         { 3, tex(e->masks.get()) },
                         { 4, tex(input.color) },
                         { 5, tex(e->internal[r].get()) },
                         { 6, tex(e->farthest_mip1.get()) },
                         { 7, tex(e->instability.get()) } },
                       { { 8, buf(e->new_locks.get()) } }, nil, false, true);
        ok = ok && run("Aurora display depth", impl.display, {}, { { 2, tex(input.color) }, { 3, depth }, { 4, motion } },
                       { { 5, buf(e->reconstructed.get()) } }, tex(e->display_depth.get()));
        ok = ok && run("Alchemy Aurora glow-alpha resolve", impl.glow, { tex(input.output) },
                       { { 0, tex(e->output.get()) }, { 1, tex(input.color) } });
        if (!ok)
        {
            impl.invalidate();
            return fail(error, "Aurora encoder creation failed; cancel this main-world transaction");
        }
    }
    @catch (NSException* exception)
    {
        impl.invalidate();
        return fail(error,
                    "Aurora native encoding exception; cancel this transaction: " + std::string(exception.reason.UTF8String ?: "unknown"));
    }
    if (!scope.end(error))
    {
        impl.invalidate();
        return false;
    }
    impl.encoded_ticket = impl.next_ticket++;
    if (!impl.next_ticket)
        ++impl.next_ticket;
    impl.encoded_frame            = frame;
    impl.encoded_completion       = completion;
    impl.encoded_reset            = reset;
    impl.encoded_failure_observed = false;
    ticket                        = impl.encoded_ticket;
    ++impl.counters.encoded_calls;
    if (reset)
        ++impl.counters.reset_calls;
    return true;
}

bool ALMetalAuroraReconstructor::submitted(Ticket ticket, std::string* error)
{
    auto& i = *mImpl;
    if (!ticket || ticket != i.encoded_ticket || !i.encoded_completion)
        return fail(error, "Aurora submission ticket mismatch");
    auto status = i.encoded_completion->status();
    if (status == ALMetalCommandStatus::error)
    {
        if (!i.encoded_failure_observed)
        {
            ++i.counters.gpu_failures;
            i.encoded_failure_observed = true;
        }
        i.current->initialized = false;
        i.invalidate();
        return fail(error, "Aurora main-world command failed before history settlement");
    }
    if (status == ALMetalCommandStatus::not_enqueued || status == ALMetalCommandStatus::enqueued || status == ALMetalCommandStatus::unknown)
        return fail(error, "Aurora history cannot advance before actual main-world submission");
    i.submitted.push_back(std::move(i.encoded_completion));
    i.encoded_ticket         = 0;
    i.prev_jitter[0]         = i.encoded_frame.jitter_x;
    i.prev_jitter[1]         = i.encoded_frame.jitter_y;
    i.frame_index            = i.encoded_reset ? 1.f : i.frame_index + 1.f;
    i.parity                 = 1 - i.parity;
    i.needs_reset            = false;
    i.current->initialized   = true;
    i.counters.history_valid = true;
    ++i.counters.submitted_calls;
    i.poll();
    return true;
}
void ALMetalAuroraReconstructor::abandon(Ticket ticket) noexcept
{
    auto& i = *mImpl;
    if (ticket && ticket == i.encoded_ticket)
    {
        i.encoded_ticket = 0;
        i.encoded_completion.reset();
        ++i.counters.abandoned_calls;
        i.invalidate();
    }
}
void ALMetalAuroraReconstructor::invalidate() noexcept
{
    mImpl->invalidate();
}
void ALMetalAuroraReconstructor::setFrameData(std::shared_ptr<ALMetalWorldFrameData> data)
{
    if (data)
    {
        mImpl->frame_data     = std::move(data);
        mImpl->own_frame_data = false;
    }
}
const ALMetalTexture* ALMetalAuroraReconstructor::rawOutput() const noexcept
{
    return mImpl->current ? mImpl->current->output.get() : nullptr;
}
const ALMetalTexture* ALMetalAuroraReconstructor::displayDepth() const noexcept
{
    return mImpl->current ? mImpl->current->display_depth.get() : nullptr;
}
const ALMetalTexture* ALMetalAuroraReconstructor::generatedReactive() const noexcept
{
    return mImpl->current ? mImpl->current->reactive.get() : nullptr;
}
ALMetalAuroraStats ALMetalAuroraReconstructor::stats() const noexcept
{
    auto result                = mImpl->counters;
    result.parity              = mImpl->parity;
    result.pending_submissions = mImpl->submitted.size();
    if (mImpl->current)
    {
        result.texture_bytes = mImpl->current->texture_bytes;
        result.buffer_bytes  = mImpl->current->buffer_bytes;
    }
    return result;
}
const char* ALMetalAuroraReconstructor::algorithmIdentity() noexcept
{
    return "Aurora WGSL FSR 3.1 ee458fbf1d95e98c5b3a55c0027a523bb28d7e7f / Naga 6c13013b / native Metal / RCAS OFF";
}
