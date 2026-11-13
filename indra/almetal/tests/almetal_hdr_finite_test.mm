/**
 * @file almetal_hdr_finite_test.mm
 * @brief Physical GPU regressions for upstream #595's native HDR boundaries.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "almetalworld.h"
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

namespace
{
using Pixel = std::array<float, 4>;
using Encode = std::function<void(id<MTLComputeCommandEncoder>)>;
using Shade = std::function<void(id<MTLRenderCommandEncoder>)>;

void replace_once(std::string& source, const std::string& from, const std::string& to)
{
    const auto at = source.find(from);
    REQUIRE(at != std::string::npos);
    REQUIRE(source.find(from, at + from.size()) == std::string::npos);
    source.replace(at, from.size(), to);
}

// Frozen pre-fix MSL expressions from b04d9bf6ef. The controls compile the
// complete production module with only these boundaries restored. They run
// on the same physical GPU and formats; no CPU lighting/postprocess oracle.
std::string unsafe_control(std::string source)
{
    replace_once(source,
        "    return almetal_bounded_radiance(color, 65504.0);",
        "    color = select(color, float3(65504.0), isinf(color));\n"
        "    color = select(color, float3(0.0), isnan(color));\n"
        "    return almetal_pbr_clamp_radiance(color);");
    replace_once(source,
        "    float3 color = select(clamp(source.rgb, float3(0.0), float3(65504.0)),\n"
        "                          float3(0.0), isnan(source.rgb));",
        "    float3 color = max(source.rgb, float3(0.0));");
    replace_once(source,
        "            float luminance = dot(float3(0.2126, 0.7152, 0.0722), color);\n"
        "            luminance = isnan(luminance) ? 0.0 : min(luminance, 60000.0);",
        "            const float luminance = min(dot(float3(0.2126, 0.7152, 0.0722), color), 60000.0);");
    replace_once(source,
        "    if (!isfinite(previous_ev))\n    {\n        previous_ev = target;\n    }\n", "");
    replace_once(source,
        "    if (probes.ref_params[index].x <= 0.0)\n    {\n        return ambient;\n    }\n", "");
    replace_once(source,
        "    const float3 color = almetal_bounded_radiance(\n"
        "        source.sample(linear_sampler, uv).rgb, 65024.0);\n"
        "    destination.write(float4(color, 0.0), position);",
        "    destination.write(float4(source.sample(linear_sampler, uv).rgb, 0.0), position);");
    replace_once(source,
        "            ? clamp(sums[coefficient - first][packed_index - 3u * coefficient],\n"
        "                    -65504.0, 65504.0)",
        "            ? sums[coefficient - first][packed_index - 3u * coefficient]");
    replace_once(source,
        "                final_color += almetal_bounded_radiance(\n"
        "                    distance_attenuation * light.color_falloff.rgb * 3.0 * lobe,\n"
        "                    pass_ceiling);",
        "                final_color += distance_attenuation * light.color_falloff.rgb * 3.0 *\n"
        "                    almetal_world_bounded_radiance(lobe);");
    replace_once(source,
        "            final_color += almetal_bounded_radiance(\n"
        "                contribution, pass_ceiling);",
        "            final_color += almetal_world_bounded_radiance(contribution);");
    replace_once(source,
        "    return float4(almetal_bounded_radiance(\n"
        "        final_color * constants.surface.y,\n"
        "        pass_ceiling), 0.0);",
        "    return float4(max(final_color * constants.surface.y, float3(0.0)), 0.0);");
    replace_once(source,
        "                final_color += almetal_bounded_radiance(\n"
        "                    distance_attenuation * projected_color * 3.0 * shadow * lobe,\n"
        "                    pass_ceiling);",
        "                final_color += distance_attenuation * projected_color * 3.0 * shadow *\n"
        "                    almetal_world_bounded_radiance(lobe);");
    replace_once(source,
        "    return float4(almetal_bounded_radiance(\n"
        "        final_color * constants.sampling.w,\n"
        "        pass_ceiling), 0.0);",
        "    return float4(max(final_color * constants.sampling.w, float3(0.0)), 0.0);");
    replace_once(source, "almetal_bounded_radiance(color * output_scale, 32752.0)",
                 "almetal_world_bounded_radiance(color * output_scale)");
    replace_once(source,
        "    return float4(almetal_bounded_radiance(sky_color, 65504.0), 0.0);",
        "    sky_color = select(sky_color, float3(65504.0), isinf(sky_color));\n"
        "    sky_color = select(sky_color, float3(0.0), isnan(sky_color));\n"
        "    return float4(sky_color, 0.0);");
    return source;
}

const char* wrappers = R"metal(
// Only binding adapters: all bounded-radiance, pass-ceiling and SH evaluation
// math here is called from the unmodified production functions above.
kernel void hdr_read(texture2d<float, access::read> source [[texture(0)]],
    device float4* result [[buffer(0)]]) { result[0] = source.read(uint2(0)); }
kernel void hdr_sh_zero(texture2d<uint> sh [[texture(0)]],
    device float4* result [[buffer(0)]],
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(1)]])
{
    result[0] = float4(almetal_probe_sh_irradiance(0, float3(0), float3(0),
        float3(0,0,1), float3(.125,.25,.5), sh, probes), 1);
}
fragment float4 hdr_pass_fragment(ALMetalWorldLocalLightVaryings input [[stage_in]],
    constant uint& count [[buffer(0)]])
{
    return float4(almetal_bounded_radiance(float3(1e10,5e9,2.5e9),
        almetal_world_light_pass_ceiling(count)), 0);
}
)metal";

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> owner;
    std::unique_ptr<ALMetalShaderLibrary> builtin_owner, old_owner, adapter_owner;
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLLibrary> builtin, old, adapter;
    id<MTLSamplerState> sampler;
    Fixture()
    {
        owner = ALMetalDevice::create(&error); INFO(error); REQUIRE(owner);
        device = (__bridge id<MTLDevice>)owner->nativeHandle();
        INFO([[device name] UTF8String]);
        REQUIRE([device hasUnifiedMemory]);
        queue = [device newCommandQueue]; REQUIRE(queue);
        builtin_owner = ALMetalShaderLibrary::loadBuiltin(*owner, &error);
        INFO(error); REQUIRE(builtin_owner);
        builtin = (__bridge id<MTLLibrary>)builtin_owner->nativeHandle();
        std::ifstream input(AL_METAL_HDR_SOURCE_PATH); REQUIRE(input.good());
        const std::string source{std::istreambuf_iterator<char>(input), {}};
        adapter_owner = ALMetalShaderLibrary::compileSource(*owner, source + wrappers,
            "Actual HDR helper binding adapters", &error); INFO(error); REQUIRE(adapter_owner);
        adapter = (__bridge id<MTLLibrary>)adapter_owner->nativeHandle();
        old_owner = ALMetalShaderLibrary::compileSource(*owner, unsafe_control(source) + wrappers,
            "Frozen pre-fix HDR GPU control", &error); INFO(error); REQUIRE(old_owner);
        old = (__bridge id<MTLLibrary>)old_owner->nativeHandle();
        auto* descriptor = [[MTLSamplerDescriptor alloc] init];
        descriptor.minFilter = descriptor.magFilter = MTLSamplerMinMagFilterNearest;
        descriptor.sAddressMode = descriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
        sampler = [device newSamplerStateWithDescriptor:descriptor]; REQUIRE(sampler);
    }
    id<MTLTexture> texture(MTLPixelFormat format, unsigned width=1, unsigned height=1)
    {
        auto* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
            width:width height:height mipmapped:NO];
        d.storageMode = MTLStorageModeShared;
        d.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
        auto result = [device newTextureWithDescriptor:d]; REQUIRE(result); return result;
    }
    id<MTLTexture> pixel(Pixel value, MTLPixelFormat format=MTLPixelFormatRGBA32Float)
    {
        auto result = texture(format);
        if (format == MTLPixelFormatRGBA16Float)
        {
            std::array<__fp16,4> halves{};
            std::copy(value.begin(), value.end(), halves.begin());
            [result replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0
                withBytes:halves.data() bytesPerRow:8];
        }
        else
        {
            REQUIRE(format == MTLPixelFormatRGBA32Float);
            [result replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0
                withBytes:value.data() bytesPerRow:16];
        }
        return result;
    }
    id<MTLBuffer> buffer(const void* bytes, std::size_t size)
    {
        auto result = bytes ? [device newBufferWithBytes:bytes length:size options:MTLResourceStorageModeShared]
                            : [device newBufferWithLength:size options:MTLResourceStorageModeShared];
        REQUIRE(result); return result;
    }
    void wait(id<MTLCommandBuffer> command)
    {
        [command commit]; [command waitUntilCompleted];
        INFO((command.error ? command.error.localizedDescription.UTF8String : ""));
        REQUIRE(command.status == MTLCommandBufferStatusCompleted);
    }
    void compute(id<MTLLibrary> library, const char* name, Encode encode, unsigned width=1, unsigned height=1)
    {
        auto function = [library newFunctionWithName:[NSString stringWithUTF8String:name]];
        REQUIRE(function); NSError* failure = nil;
        auto pipeline = [device newComputePipelineStateWithFunction:function error:&failure];
        INFO((failure ? failure.localizedDescription.UTF8String : "")); REQUIRE(pipeline);
        auto command = [queue commandBuffer]; auto encoder = [command computeCommandEncoder];
        [encoder setComputePipelineState:pipeline]; encode(encoder);
        [encoder dispatchThreads:MTLSizeMake(width,height,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)];
        [encoder endEncoding]; wait(command);
    }
    Pixel read(id<MTLTexture> source)
    {
        auto output = buffer(nullptr, sizeof(Pixel));
        compute(adapter, "hdr_read", [&](id<MTLComputeCommandEncoder> e) {
            [e setTexture:source atIndex:0]; [e setBuffer:output offset:0 atIndex:0]; });
        Pixel result; std::memcpy(result.data(), output.contents, sizeof(result)); return result;
    }
    id<MTLTexture> render(id<MTLLibrary> library, const char* fragment, Shade shade,
        MTLPixelFormat format=MTLPixelFormatRGBA32Float, unsigned draws=1, bool additive=false,
        double base=0.)
    {
        auto* d = [[MTLRenderPipelineDescriptor alloc] init];
        const bool bloom = std::string(fragment).find("bloom") != std::string::npos;
        d.vertexFunction = [library newFunctionWithName:bloom ? @"almetal_resample_vertex"
                                                               : @"almetal_world_local_light_vertex"];
        d.fragmentFunction = [library newFunctionWithName:[NSString stringWithUTF8String:fragment]];
        REQUIRE(d.vertexFunction); REQUIRE(d.fragmentFunction);
        if (d.fragmentFunction.functionConstantsDictionary.count != 0)
        {
            auto* values = [[MTLFunctionConstantValues alloc] init];
            NSError* specialization_error = nil;
            d.fragmentFunction = [library newFunctionWithName:[NSString stringWithUTF8String:fragment]
                constantValues:values error:&specialization_error];
            INFO((specialization_error ? specialization_error.localizedDescription.UTF8String : ""));
            REQUIRE(d.fragmentFunction);
        }
        d.colorAttachments[0].pixelFormat = format;
        d.colorAttachments[0].blendingEnabled = additive;
        d.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
        d.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
        d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
        NSError* failure = nil;
        auto pipeline = [device newRenderPipelineStateWithDescriptor:d error:&failure];
        INFO((failure ? failure.localizedDescription.UTF8String : "")); REQUIRE(pipeline);
        auto output = texture(format);
        auto* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = output;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(base,base,base,0);
        auto command = [queue commandBuffer]; auto encoder = [command renderCommandEncoderWithDescriptor:pass];
        [encoder setRenderPipelineState:pipeline]; [encoder setCullMode:MTLCullModeNone];
        for (unsigned slot=0; slot<3; ++slot) [encoder setFragmentSamplerState:sampler atIndex:slot];
        shade(encoder);
        for (unsigned i=0; i<draws; ++i)
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding]; wait(command); return output;
    }
};

bool finite(const Pixel& value)
{ return std::all_of(value.begin(), value.end(), [](float v) { return std::isfinite(v); }); }
void same(const Pixel& value, const Pixel& expected, float tolerance=.00002f)
{
    for (unsigned c=0; c<4; ++c) { CAPTURE(c, value[c], expected[c]); CHECK(std::abs(value[c]-expected[c])<=tolerance); }
}
void identity(float* matrix)
{ for (unsigned i=0;i<16;++i) matrix[i]=i%5==0?1.f:0.f; }

void lighting_fragments(Fixture& f)
{
    struct alignas(16) LocalConstants
    { float inverse_projection[16]{}; float surface[4]{128,1,0,0}; unsigned config[4]{1,0,63,0}; };
    struct alignas(16) ProjectorConstants
    {
        float inverse_projection[16]{},projector_matrix[16]{};
        float position_radius[4]{0,0,0,100},color_falloff[4]{1,1,1,.5f};
        float projector_plane[4]{},projector_normal[4]{0,0,-1,100},projector_origin[4]{};
        float sampling[4]{1,0,128,1}; unsigned config[4]{0,0,0,63};
    };
    static_assert(sizeof(LocalConstants)==96); static_assert(sizeof(ProjectorConstants)==240);
    LocalConstants local; ProjectorConstants projector;
    identity(local.inverse_projection); local.inverse_projection[14]=-1;
    identity(projector.inverse_projection); projector.inverse_projection[14]=-1;
    projector.projector_matrix[12]=projector.projector_matrix[13]=projector.projector_matrix[14]=.5f;
    projector.projector_matrix[15]=1;
    auto albedo=f.pixel({1,.5f,.25f,0});
    auto specular=f.pixel({1,.5f,1,0});
    auto depth=f.pixel({.5f,0,0,0});
    auto brdf=f.pixel({.5f,.1f,0,0});
    auto cookie=f.pixel({1,1,1,1});
    auto* array_descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float width:1 height:1 mipmapped:NO];
    array_descriptor.textureType=MTLTextureType2DArray; array_descriptor.arrayLength=1;
    array_descriptor.storageMode=MTLStorageModeShared;
    auto shadow_array=[f.device newTextureWithDescriptor:array_descriptor]; REQUIRE(shadow_array);
    ALMetalWorldDirectionalShadowConstants shadows;
    for(bool pbr : {false,true})
    for(float intensity : {1.f,1.e10f,1.e38f})
    {
        CAPTURE(pbr,intensity);
        auto normal=f.pixel({.5f,.5f,0,pbr?.67f:.25f});
        ALMetalWorldLocalLight light;
        light.position_radius[3]=100;
        light.color_falloff[0]=light.color_falloff[1]=light.color_falloff[2]=intensity;
        light.color_falloff[3]=.5f;
        auto lights=f.buffer(&light,sizeof(light));
        projector.color_falloff[0]=projector.color_falloff[1]=projector.color_falloff[2]=intensity;
        const auto bind=[&](id<MTLRenderCommandEncoder> e) {
            [e setFragmentTexture:albedo atIndex:0]; [e setFragmentTexture:specular atIndex:1];
            [e setFragmentTexture:normal atIndex:2]; [e setFragmentTexture:depth atIndex:3]; [e setFragmentTexture:brdf atIndex:4]; };
        for(bool spot : {false,true})
        {
            const char* entry=spot?"almetal_world_projector_light_fragment":"almetal_world_local_light_fragment";
            const auto shade=[&](id<MTLRenderCommandEncoder> e) {
                bind(e);
                if(spot)
                {
                    [e setFragmentBytes:&projector length:sizeof(projector) atIndex:0];
                    [e setFragmentBytes:&shadows length:sizeof(shadows) atIndex:1];
                    [e setFragmentTexture:cookie atIndex:5]; [e setFragmentTexture:shadow_array atIndex:6];
                }
                else { [e setFragmentBuffer:lights offset:0 atIndex:0]; [e setFragmentBytes:&local length:sizeof(local) atIndex:1]; }
            };
            const auto now=f.read(f.render(f.builtin,entry,shade));
            const auto old=f.read(f.render(f.old,entry,shade)); CAPTURE(spot,now,old);
            CHECK(finite(now)); CHECK(now[0]<=512.f);
            if(intensity==1.f) { same(now,old); CHECK(now[0]>0.f); }
            else { CHECK(now[0]>0.f); CHECK((!finite(old) || old[0]!=now[0])); }
        }
    }
    // Native base/soften and sky are distinct production fragments. Only
    // geometry receives the half-range limit; sky keeps the whole range.
    ALMetalWorldBaseLightingConstants base;
    identity(base.inverse_projection); base.inverse_projection[14]=-1;
    base.light_direction[2]=1; base.lighting[0]=1;
    ALMetalWorldReflectionProbeConstants probes;
    ALMetalWorldHeroProbeConstants hero;
    ALMetalWorldScreenSpaceReflectionConstants ssr;
    ssr.screen[0]=ssr.screen[1]=1;
    auto probe_constants=f.buffer(&probes,sizeof(probes));
    auto normal=f.pixel({.5f,.5f,0,.67f});
    auto light_map=f.pixel({1,1,0,0});
    for(float intensity : {1.f,1.e10f})
    for(bool classic : {false,true})
    {
        auto emission=f.pixel({intensity,intensity*.5f,intensity*.25f,0});
        base.config[1]=classic?1u:0u;
        const auto shade=[&](id<MTLRenderCommandEncoder> e) {
            [e setFragmentBytes:&base length:sizeof(base) atIndex:0];
            [e setFragmentBuffer:probe_constants offset:0 atIndex:1];
            [e setFragmentBytes:&hero length:sizeof(hero) atIndex:2];
            [e setFragmentBytes:&ssr length:sizeof(ssr) atIndex:3];
            [e setFragmentTexture:albedo atIndex:0]; [e setFragmentTexture:specular atIndex:1];
            [e setFragmentTexture:normal atIndex:2]; [e setFragmentTexture:emission atIndex:3];
            [e setFragmentTexture:depth atIndex:4]; [e setFragmentTexture:light_map atIndex:5];
            [e setFragmentTexture:brdf atIndex:8]; };
        const auto now=f.read(f.render(f.builtin,"almetal_world_base_light_geometry_fragment",shade));
        const auto old=f.read(f.render(f.old,"almetal_world_base_light_geometry_fragment",shade));
        CAPTURE(intensity,classic,now,old); CHECK(finite(now)); CHECK(now[0]<=32752.f);
        if(intensity==1.f) same(now,old);
        else { CHECK(now[0]==32752.f); CHECK(old[0]>32752.f); CHECK(now[1]==now[0]*.5f); }
    }
    normal=f.pixel({0,0,0,1}); base.sky[0]=base.sky[1]=1;
    for(float intensity : {1.f,1.e10f})
    {
        auto emission=f.pixel({intensity,intensity*.5f,intensity*.25f,0});
        const auto shade=[&](id<MTLRenderCommandEncoder> e) {
            [e setFragmentBytes:&base length:sizeof(base) atIndex:0];
            [e setFragmentTexture:albedo atIndex:0]; [e setFragmentTexture:normal atIndex:2];
            [e setFragmentTexture:emission atIndex:3]; };
        const auto now=f.read(f.render(f.builtin,"almetal_world_base_light_sky_fragment",shade));
        const auto old=f.read(f.render(f.old,"almetal_world_base_light_sky_fragment",shade));
        CAPTURE(intensity,now,old); CHECK(finite(now)); CHECK(now[0]<=65504.f);
        if(intensity==1.f) same(now,old);
        else { CHECK(now[0]==65504.f); CHECK(old[0]>65504.f); CHECK(now[1]==now[0]*.5f); }
    }
}
}

TEST_CASE("Native HDR boundaries contain nonfinite inputs and preserve ordinary values on the GPU",
          "[almetal][hdr-finite]")
{
    @autoreleasepool
    {
        Fixture f;
        lighting_fragments(f);
        const float inf=std::numeric_limits<float>::infinity();
        const float nan=std::numeric_limits<float>::quiet_NaN();

        // Actual bloom fragments, including the shared preview entry point.
        const std::array<float,8> bloom_constants{1.f,.5f,10.f,1.f,1.f,.5f,.25f,0.f};
        for (const char* entry : {"almetal_world_bloom_extract_fragment", "almetal_pbr_preview_bloom_extract_fragment"})
        for (const Pixel& value : {Pixel{inf,4.f,nan,.25f}, Pixel{4.f,2.f,1.f,.25f}})
        {
            CAPTURE(entry,value);
            auto source=f.pixel(value,MTLPixelFormatRGBA16Float);
            const auto shade=[&](id<MTLRenderCommandEncoder> e) {
                [e setFragmentTexture:source atIndex:0];
                [e setFragmentBytes:bloom_constants.data() length:sizeof(bloom_constants) atIndex:0]; };
            const auto now=f.read(f.render(f.builtin,entry,shade));
            const auto old=f.read(f.render(f.old,entry,shade));
            CHECK(finite(now));
            if (std::isinf(value[0])) { CHECK_FALSE(finite(old)); CHECK(now[0]>0.f); }
            else same(now,old);
        }

        // Production meter: NaN is black; infinity still meters at 60000.
        const Pixel patch{1,1,1,1};
        auto normal=f.pixel({.5f,.5f,0.f,.25f});
        for (const Pixel& value : {Pixel{nan,1,1,0},Pixel{inf,inf,inf,0},Pixel{4,4,4,0}})
        {
            auto source=f.pixel(value,MTLPixelFormatRGBA16Float);
            const auto meter=[&](id<MTLLibrary> library) {
                auto target=f.texture(MTLPixelFormatRGBA16Float);
                f.compute(library,"almetal_world_exposure_meter",[&](id<MTLComputeCommandEncoder> e) {
                    [e setTexture:source atIndex:0]; [e setTexture:normal atIndex:1]; [e setTexture:target atIndex:2];
                    [e setSamplerState:f.sampler atIndex:0]; [e setBytes:patch.data() length:16 atIndex:0]; });
                return f.read(target); };
            const auto now=meter(f.builtin), old=meter(f.old); CAPTURE(value,now,old);
            CHECK(finite(now));
            if (std::isnan(value[0])) { CHECK(now[0]==0.f); CHECK(old[0]!=0.f); }
            else same(now,old);
        }
        // Production exposure history: each nonfinite EV resets to this target.
        const std::array<float,12> exposure_constants{0,-16,16,.1f, 0,1,1,1, 1,0,0,0};
        auto meter=f.pixel({4,0,0,1},MTLPixelFormatRGBA16Float);
        for (float ev : {nan,inf,-inf,0.f})
        {
            auto history=f.pixel({1,ev,0,1},MTLPixelFormatRGBA16Float);
            const auto resolve=[&](id<MTLLibrary> library) {
                auto target=f.texture(MTLPixelFormatRGBA32Float);
                f.compute(library,"almetal_world_exposure_history",[&](id<MTLComputeCommandEncoder> e) {
                    [e setTexture:meter atIndex:0]; [e setTexture:history atIndex:1]; [e setTexture:target atIndex:2];
                    [e setBytes:exposure_constants.data() length:sizeof(exposure_constants) atIndex:0]; });
                return f.read(target); };
            const auto now=resolve(f.builtin),old=resolve(f.old); CAPTURE(ev,now,old);
            CHECK(finite(now));
            if (!std::isfinite(ev)) { CHECK_FALSE(finite(old)); CHECK(now[1]==now[2]); CHECK(now[1]==2.f); }
            else same(now,old);
        }

        // Actual native RG11B10Float probe boundary, not a float-only stand-in.
        for (const Pixel& value : {Pixel{inf,-inf,nan,0},Pixel{0,inf,0,0},Pixel{0,0,inf,0},
             Pixel{inf,inf,inf,0},Pixel{65024,65024,65024,0},Pixel{65504,32752,16376,0},Pixel{4,2,1,0}})
        {
            auto source=f.pixel(value,MTLPixelFormatRGBA16Float);
            const auto downsample=[&](id<MTLLibrary> library) {
                auto target=f.texture(MTLPixelFormatRG11B10Float);
                f.compute(library,"almetal_world_probe_downsample",[&](id<MTLComputeCommandEncoder> e) {
                    [e setTexture:source atIndex:0]; [e setTexture:target atIndex:1]; });
                return f.read(target); };
            const auto now=downsample(f.builtin),old=downsample(f.old); CAPTURE(value,now,old);
            CHECK(finite(now));
            if (!finite(value))
            {
                CHECK_FALSE(finite(old));
                for (unsigned c=0;c<3;++c)
                {
                    CAPTURE(c,now[c]);
                    if (std::isinf(value[c]) && value[c]>0.f)
                    { CHECK(now[c]>0.f); CHECK(now[c]<=(c==2?64512.f:65024.f)); }
                    else CHECK(now[c]==0.f);
                }
            }
            else if(value[0]<65024.f) same(now,old);
            else if(value[0]==65024.f) { CHECK(now[0]==65024.f); CHECK(now[1]==65024.f); CHECK(now[2]<=64512.f); }
            else { CHECK(now[0]==65024.f); CHECK(now[1]==32512.f); CHECK(now[2]==16128.f); }
        }

        // Actual two-stage SH reduction's half packing: signed coefficients
        // can exceed half range even when every partial row is finite.
        auto partial=f.texture(MTLPixelFormatRGBA16Float,9,24);
        std::vector<__fp16> rows(9*24*4);
        for(unsigned y=0;y<24;++y) for(unsigned x=0;x<9;++x)
        { const auto at=(y*9+x)*4; rows[at]=65504; rows[at+1]=-65504; rows[at+2]=32; rows[at+3]=1; }
        [partial replaceRegion:MTLRegionMake2D(0,0,9,24) mipmapLevel:0 withBytes:rows.data() bytesPerRow:9*8];
        const std::array<unsigned,4> sh_constants{0,4,0,0};
        for(auto library : {f.old,f.builtin})
        {
            auto destination=f.texture(MTLPixelFormatRGBA32Uint,4,1);
            auto words=f.buffer(nullptr,64);
            f.compute(library,"almetal_world_probe_sh_reduce",[&](id<MTLComputeCommandEncoder> e) {
                [e setTexture:partial atIndex:0]; [e setTexture:destination atIndex:1];
                [e setBytes:sh_constants.data() length:16 atIndex:0]; [e setBuffer:words offset:0 atIndex:1]; },4);
            const auto* halves=static_cast<const __fp16*>(words.contents);
            for(unsigned coefficient=0;coefficient<9;++coefficient)
            {
                if(library==f.builtin) { CHECK(float(halves[coefficient*3])==65504.f); CHECK(float(halves[coefficient*3+1])==-65504.f); }
                else { CHECK(std::isinf(float(halves[coefficient*3]))); CHECK(std::isinf(float(halves[coefficient*3+1]))); }
                CHECK(float(halves[coefficient*3+2])==768.f);
            }
        }

        // Call the actual SH evaluation helper with corrupt stored halves.
        auto invalid_sh=f.texture(MTLPixelFormatRGBA32Uint,4,1);
        std::array<unsigned,16> bad_words{}; bad_words[0]=0x7c007c00u; bad_words[1]=0x00007c00u;
        [invalid_sh replaceRegion:MTLRegionMake2D(0,0,4,1) mipmapLevel:0 withBytes:bad_words.data() bytesPerRow:64];
        ALMetalWorldReflectionProbeConstants probes;
        probes.ref_sphere[0][3]=4; probes.ref_index[0][3]=1;
        for(unsigned i=0;i<3;++i) probes.env_matrix[i][i]=1;
        for(float scale : {0.f,-1.f})
        {
            probes.ref_params[0][0]=scale;
            auto constants=f.buffer(&probes,sizeof(probes));
            const auto evaluate=[&](id<MTLLibrary> library) {
                auto output=f.buffer(nullptr,16);
                f.compute(library,"hdr_sh_zero",[&](id<MTLComputeCommandEncoder> e) {
                    [e setTexture:invalid_sh atIndex:0]; [e setBuffer:output offset:0 atIndex:0]; [e setBuffer:constants offset:0 atIndex:1]; });
                Pixel result; std::memcpy(result.data(),output.contents,16); return result; };
            same(evaluate(f.adapter),{.125f,.25f,.5f,1.f});
            CHECK_FALSE(finite(evaluate(f.old)));
        }

        // Shader's actual ceiling helper plus physical per-draw FP16 additive
        // blending. Includes dyadic transitions, default256, API65535 and both
        // families' accepted maximum131070, with the worst allowed base store.
        for(unsigned count : {1u,63u,64u,126u,127u,256u,1008u,1009u,2016u,2017u,65535u,131070u})
        {
            const auto output=f.render(f.adapter,"hdr_pass_fragment",[&](id<MTLRenderCommandEncoder> e) {
                [e setFragmentBytes:&count length:4 atIndex:0]; },MTLPixelFormatRGBA16Float,count,true,32752.);
            const auto now=f.read(output); CAPTURE(count,now); CHECK(finite(now)); CHECK(now[0]<=65024.f); CHECK(now[0]>=32752.f);
        }
        // A fixed512 budget exceeds the conservative65024 envelope at64
        // draws. Record the physical blend result: Metal can saturate its
        // render-store conversion rather than manufacture an infinity.
        std::string fixed=wrappers;
        replace_once(fixed,"almetal_world_light_pass_ceiling(count)","512.0");
        std::ifstream input(AL_METAL_HDR_SOURCE_PATH);
        const std::string source{std::istreambuf_iterator<char>(input),{}};
        auto unsafe=f.adapter_owner->compileSource(*f.owner,source+fixed,"Unsafe fixed512 blend control",&f.error);
        INFO(f.error); REQUIRE(unsafe);
        const unsigned count=64;
        auto unsafe_native=(__bridge id<MTLLibrary>)unsafe->nativeHandle();
        const auto overflow=f.read(f.render(unsafe_native,"hdr_pass_fragment",[&](id<MTLRenderCommandEncoder> e) {
            [e setFragmentBytes:&count length:4 atIndex:0]; },MTLPixelFormatRGBA16Float,count,true,32752.));
        CAPTURE(overflow);
        CHECK(overflow[0]>65024.f);
    }
}

TEST_CASE("Native HDR scene owners publish the shared draw bound and safe isolated fallback",
          "[almetal][hdr-finite][hdr-budget-abi]")
{
    @autoreleasepool
    {
        std::string error;
        auto device=ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        auto renderer=ALMetalWorldGBufferRenderer::create(*device,&error); INFO(error); REQUIRE(renderer);
        auto frame=ALMetalWorldFrameData::create(*device,&error); REQUIRE(frame);
        frame->beginFrame(1); renderer->setFrameData(std::move(frame));
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width=descriptor.height=1; descriptor.color_attachment_count=4;
        descriptor.colors[0].format=ALMetalPixelFormat::rgba8_unorm_srgb;
        descriptor.colors[1].format=ALMetalPixelFormat::rgba8_unorm;
        descriptor.colors[2].format=ALMetalPixelFormat::rgba16_unorm;
        descriptor.colors[3].format=ALMetalPixelFormat::rg11b10_float;
        descriptor.depth_stencil_format=ALMetalPixelFormat::depth32_float_stencil8;
        descriptor.depth_stencil_shader_read=true;
        auto gbuffer=ALMetalRenderTarget::create(*device,descriptor,&error); REQUIRE(gbuffer);
        const auto clear=[&](ALMetalRenderTarget& target,double color,bool material) {
            ALMetalRenderPassConfig config;
            config.colors[0].clear={color,color,color,0};
            config.colors[2].clear={.5,.5,0,.25};
            config.clear_depth=.5;
            config.depth_store=config.stencil_store=ALMetalStoreAction::store;
            if(!material) config.bind_depth_stencil=false;
            auto pass=target.makeRenderPass(config,&error); REQUIRE(pass);
            auto command=device->makeCommandBuffer("HDR ABI authored target",&error); REQUIRE(command);
            auto encoder=ALMetalRenderEncoder::begin(*command,*pass,"HDR ABI clear",&error); REQUIRE(encoder);
            REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error)); };
        clear(*gbuffer,1.,true);
        descriptor.color_attachment_count=1;
        descriptor.colors[0].format=ALMetalPixelFormat::rgba16_float;
        descriptor.depth_stencil_format=ALMetalPixelFormat::invalid;
        descriptor.depth_stencil_shader_read=false;
        auto target=ALMetalRenderTarget::create(*device,descriptor,&error); REQUIRE(target);
        const auto read=[&]() {
            std::array<__fp16,4> halves;
            REQUIRE(target->colorTexture(0)->readback(*device,0,0,{},halves.data(),8,&error));
            Pixel result; std::copy(halves.begin(),halves.end(),result.begin()); CHECK(finite(result)); return result; };
        std::vector<ALMetalWorldLocalLight> lights(128);
        for(auto& light:lights)
        {
            light.position_radius[3]=100;
            light.color_falloff[0]=light.color_falloff[1]=light.color_falloff[2]=1.e10f;
            light.color_falloff[3]=.5f; light.screen_rect[2]=light.screen_rect[3]=1;
        }
        ALMetalWorldLocalLightingScene scene;
        scene.gbuffer=gbuffer.get(); scene.lights=lights.data(); scene.light_count=scene.bounded_light_count=lights.size();
        scene.hdr=true; identity(scene.inverse_projection); scene.inverse_projection[14]=-1;
        std::array<Pixel,3> actual;
        const std::array<std::size_t,3> bounds{128,256,0};
        for(unsigned i=0;i<bounds.size();++i)
        {
            clear(*target,32752.,false); scene.additive_pass_count=bounds[i];
            const bool accumulated=renderer->accumulateLocalLights(*target,scene,&error);
            INFO(error); REQUIRE(accumulated);
            actual[i]=read(); CAPTURE(bounds[i],actual[i]); CHECK(actual[i][0]<=65024.f);
        }
        CHECK(actual[0][0]>actual[1][0]); same(actual[1],actual[2]);

        ALMetalTextureDescriptor cookie_descriptor;
        cookie_descriptor.format=ALMetalPixelFormat::rgba8_unorm;
        auto cookie=ALMetalTexture::create(*device,cookie_descriptor,&error); REQUIRE(cookie);
        const std::array<unsigned char,4> white{255,255,255,255};
        auto upload=device->makeCommandBuffer("HDR ABI projector cookie",&error); REQUIRE(upload);
        REQUIRE(cookie->upload(*upload,0,0,{},white.data(),4,&error)); REQUIRE(upload->commitAndWait(&error));
        ALMetalWorldProjectorLight light;
        light.cookie_texture=cookie.get(); light.position_radius[3]=100;
        light.color_falloff[0]=light.color_falloff[1]=light.color_falloff[2]=1.e10f; light.color_falloff[3]=.5f;
        light.projector_matrix[12]=light.projector_matrix[13]=light.projector_matrix[14]=.5f; light.projector_matrix[15]=1;
        light.projector_normal[2]=-1; light.projector_normal[3]=100; light.sampling[0]=1;
        ALMetalWorldProjectorLightingScene spot;
        spot.gbuffer=gbuffer.get(); spot.lights=&light; spot.light_count=1; spot.hdr=true; spot.normal_hdr=true;
        identity(spot.inverse_projection); spot.inverse_projection[14]=-1;
        for(const std::size_t bound : {63u,64u})
        {
            spot.additive_pass_count=bound; clear(*target,0.,false);
            const bool accumulated=renderer->accumulateProjectorLights(*target,spot,&error);
            INFO(error); REQUIRE(accumulated);
            const auto result=read(); CAPTURE(bound,result); CHECK(result[0]==(bound==63?512.f:256.f));
        }
    }
}
