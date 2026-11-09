/**
 * @file almetal_hero_overwrite_test.mm
 * @brief Production shiny output against the frozen pre-optimization owner.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "../almetalworld.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
// Frozen from 83641ef8f82dd058c9d28f49b6568f98ac8e5bf3, original shader
// SHA256 e9345c0dd8f4a1b02624a2f5194144006f40e61ac9b37561d918021d9dcc8d56.
// The old fragment MUST call this old finish, never the optimized helper.
// Unchanged ordinary selection/tapping is shared with the production source.
const char* frozen_reference = R"frozen(
static float regression_original_hero_weight(float3 position, float glossiness,
    constant ALMetalWorldHeroProbeConstants& hero)
{
    if (hero.config.y < 1u || glossiness <= 0.75)
    {
        return 0.0;
    }

    float weight;
    if (hero.config.x < 1u)
    {
        const float3 box_position =
            (hero.hero_box * float4(position, 1.0)).xyz;
        weight = max(almetal_probe_box_depth(box_position), 0.0);
    }
    else
    {
        const float radius = hero.hero_sphere.w;
        const float inner_radius = radius * 0.5;
        const float distance = max(
            length(position - hero.hero_sphere.xyz), 0.001);
        const float attenuation = 1.0 -
            max(distance - inner_radius, 0.0) /
            max(radius - inner_radius, 0.001);
        weight = attenuation / distance;
    }

    float clip_distance =
        dot(position, hero.clip_plane.xyz) + hero.clip_plane.w;
    clip_distance = clamp((clip_distance * 0.95 + 0.05) * 10.0,
                          0.0, 1.0);
    weight = clamp(weight * 10.0 * clip_distance, 0.0, 1.0);
    weight *= clamp(glossiness - 0.75, 0.0, 1.0) * 4.0;
    if (weight <= 0.0)
    {
        return 0.0;
    }


    return weight;
}
static float3 regression_original_hero_finish(
    float3 environment_color, float3 position, float3 normal,
    float glossiness,
    texturecube<float> hero_radiance, sampler probe_sampler,
    constant ALMetalWorldReflectionProbeConstants& probes,
    constant ALMetalWorldHeroProbeConstants& hero)
{
    if (hero.config.y < 1u || glossiness <= 0.75)
    {
        return environment_color;
    }

    float weight;
    const float3 reflected = reflect(position, normal);
    if (hero.config.x < 1u)
    {
        const float3 box_position =
            (hero.hero_box * float4(position, 1.0)).xyz;
        weight = max(almetal_probe_box_depth(box_position), 0.0);
    }
    else
    {
        const float radius = hero.hero_sphere.w;
        const float inner_radius = radius * 0.5;
        const float distance = max(
            length(position - hero.hero_sphere.xyz), 0.001);
        const float attenuation = 1.0 -
            max(distance - inner_radius, 0.0) /
            max(radius - inner_radius, 0.001);
        weight = attenuation / distance;
    }

    float clip_distance =
        dot(position, hero.clip_plane.xyz) + hero.clip_plane.w;
    clip_distance = clamp((clip_distance * 0.95 + 0.05) * 10.0,
                          0.0, 1.0);
    weight = clamp(weight * 10.0 * clip_distance, 0.0, 1.0);
    weight *= clamp(glossiness - 0.75, 0.0, 1.0) * 4.0;
    if (weight <= 0.0)
    {
        return environment_color;
    }

    const float3x3 environment = float3x3(
        probes.env_matrix[0].xyz,
        probes.env_matrix[1].xyz,
        probes.env_matrix[2].xyz);
    const float3 hero_environment = hero_radiance.sample(
        probe_sampler, environment * reflected,
        level((1.0 - glossiness) * float(hero.config.z))).rgb;
    return mix(environment_color, hero_environment, weight);
}

fragment float4 regression_original_shiny_fragment(
    ALMetalWorldShinyVaryings input [[stage_in]],
    array<texture2d<float>, 32> diffuse_textures [[texture(0)]],
    texturecube_array<float> reflection_probes [[texture(32)]],
    texturecube<float> hero_probe [[texture(34)]],
    texturecube<float> legacy_sky [[texture(39)]],
    sampler diffuse_sampler [[sampler(0)]],
    sampler probe_sampler [[sampler(1)]],
    constant ALMetalWorldAlphaEnvironment& environment [[buffer(0)]],
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(1)]],
    constant ALMetalWorldHeroProbeConstants& hero [[buffer(4)]])
{
    const float4 diffuse = input.texture_index < input.texture_count
        ? diffuse_textures[min(input.texture_index, 31u)].sample(
              diffuse_sampler, input.texcoord)
        : float4(1.0, 0.0, 1.0, 1.0);
    float3 color = diffuse.rgb * input.color.rgb;
    const float3 normal = normalize(input.normal);
    const float environment_intensity = input.color.a;
    const bool legacy_environment_path =
        environment.environment_config.x != 0u;
    float3 legacy_environment = legacy_environment_path
        ? almetal_sample_legacy_sky(
              input.eye_position, normal, legacy_sky,
              probe_sampler, environment)
        : almetal_sample_legacy_probes(
              input.eye_position, normal,
              reflection_probes, probe_sampler, probes);
    if (legacy_environment_path)
    {
        color = almetal_srgb_to_linear(mix(
            almetal_linear_to_srgb(color),
            legacy_environment * 2.0,
            environment_intensity));
    }
    else
    {
        legacy_environment = regression_original_hero_finish(
            legacy_environment, input.eye_position, normal, 1.0,
            hero_probe, probe_sampler, probes, hero);
        float fresnel = 1.0 + dot(
            normalize(input.eye_position), normal);
        fresnel *= fresnel;
        fresnel = min(fresnel + environment_intensity, 1.0);
        const float3 reflected_color = legacy_environment *
            (environment_intensity * fresnel);
        color = mix(
            color, reflected_color * 0.5,
            environment_intensity);
    }
    return max(float4(color, 1.0), float4(0.0));
}

fragment float4 regression_original_weight_fragment(ALMetalWorldShinyVaryings input [[stage_in]],
    constant ALMetalWorldHeroProbeConstants& hero [[buffer(4)]])
{
    const float w = regression_original_hero_weight(input.eye_position, 1.0, hero);
    return float4(w, w == 1.0, w > 0.0 && w < 1.0, w <= 0.0);
}

)frozen";

constexpr unsigned image_width = 64, image_height = 40;
constexpr unsigned cube_layers = 35;

struct alignas(16) ShinyVertexConstants
{
    float modelview_projection[16]{};
    float modelview[16]{};
    float normal_matrix[3][4]{};
    float rigged_normal_matrix[3][4]{};
    float texture_matrix[16]{};
    std::uint32_t config[4]{};
    float avatar[4][4]{};
    float mirror_clip_plane[4]{};
};
static_assert(sizeof(ShinyVertexConstants) == 384);
static_assert(sizeof(ALMetalWorldReflectionProbeConstants) == 49216);

struct Case
{
    const char* name;
    unsigned hero_shape = 1, layout = 2;
    bool enabled = true;
    float clip = 1.f, span = 2.f, ordinary = 1.f, hero = 1.f;
    bool legacy_sky = false, nonuniform = false, rotated = false;
    float intensity = .8f;
    bool zero_diffuse = false, pure_partial = false;
};

void identity(float* matrix)
{
    std::fill(matrix, matrix + 16, 0.f);
    for (unsigned i = 0; i < 4; ++i) matrix[i * 5] = 1.f;
}
void requireNative(bool ok, const char* message)
{
    INFO(message);
    REQUIRE(ok);
}
void finish(id<MTLCommandBuffer> command)
{
    [command commit];
    [command waitUntilCompleted];
    requireNative(command.status == MTLCommandBufferStatusCompleted,
        command.error ? command.error.description.UTF8String : "GPU command did not complete");
}

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device_owner;
    std::unique_ptr<ALMetalShaderLibrary> original_library, production_library;
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    std::array<id<MTLLibrary>, 2> libraries{}; // frozen original, actual packaged production
    std::array<id<MTLRenderPipelineState>, 2> pipelines{};
    id<MTLRenderPipelineState> census_pipeline = nil;
    std::array<id<MTLTexture>, 2> output{};
    std::array<id<MTLBuffer>, 2> readback{};
    std::array<id<MTLBuffer>, 4> vertex_buffers{};
    id<MTLTexture> radiance = nil, hero = nil, diffuse = nil, sky = nil;
    id<MTLSamplerState> sampler = nil;
    id<MTLBuffer> environment_buffer = nil, probes_buffer = nil;
    id<MTLBuffer> hero_buffer = nil, vertex_constants_buffer = nil;
    ALMetalWorldAlphaEnvironment environment{};
    ALMetalWorldReflectionProbeConstants probes{};
    ALMetalWorldHeroProbeConstants hero_constants{};
    ShinyVertexConstants vertex_constants;
    MTLPixelFormat format = MTLPixelFormatRGBA32Float;
    unsigned bytes_per_pixel = 16;

    id<MTLBuffer> buffer(std::size_t size, const char* label)
    {
        auto value = [device newBufferWithLength:size options:MTLResourceStorageModeShared];
        requireNative(value != nil, label);
        value.label = [NSString stringWithUTF8String:label];
        return value;
    }
    id<MTLTexture> texture(unsigned width, unsigned height, MTLPixelFormat pixel_format,
        MTLTextureType type = MTLTextureType2D, unsigned layers = 1, unsigned mips = 1)
    {
        auto descriptor = [MTLTextureDescriptor new];
        descriptor.width = width; descriptor.height = height;
        descriptor.pixelFormat = pixel_format; descriptor.textureType = type;
        descriptor.arrayLength = layers; descriptor.mipmapLevelCount = mips;
        descriptor.storageMode = MTLStorageModePrivate;
        descriptor.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
        auto value = [device newTextureWithDescriptor:descriptor];
        requireNative(value != nil, "Hero overwrite fixture texture");
        return value;
    }
    void upload(id<MTLBlitCommandEncoder> encoder, id<MTLTexture> target,
        unsigned slice, unsigned mip, const void* bytes, std::size_t row_bytes)
    {
        const unsigned width = std::max(1u, unsigned(target.width) >> mip);
        const unsigned height = std::max(1u, unsigned(target.height) >> mip);
        const auto stride = (row_bytes + 255) & ~std::size_t(255);
        auto staging = buffer(stride * height, "Hero overwrite immutable upload");
        for (unsigned y = 0; y < height; ++y)
            std::memcpy(static_cast<char*>(staging.contents) + y * stride,
                static_cast<const char*>(bytes) + y * row_bytes, row_bytes);
        [encoder copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:stride
            sourceBytesPerImage:stride * height sourceSize:MTLSizeMake(width,height,1)
            toTexture:target destinationSlice:slice destinationLevel:mip
            destinationOrigin:MTLOriginMake(0,0,0)];
    }
    Fixture()
    {
        device_owner = ALMetalDevice::create(&error);
        INFO(error); REQUIRE(device_owner);
        device = (__bridge id<MTLDevice>)device_owner->nativeHandle();
        queue = [device newCommandQueue]; REQUIRE(queue);
        production_library = ALMetalShaderLibrary::loadBuiltin(*device_owner, &error);
        INFO(error); REQUIRE(production_library);
        // Test sources live beside the source under test. The production side
        // always loads the ordinary compiled metallib, not this runtime copy.
        const auto source_path = std::filesystem::path(__FILE__).parent_path().parent_path() /
            "shaders" / "almetal_builtin.metal";
        std::ifstream source(source_path);
        REQUIRE(source.good());
        const std::string current{std::istreambuf_iterator<char>(source), {}};
        original_library = ALMetalShaderLibrary::compileSource(*device_owner,
            current + frozen_reference, "Frozen original fullbright shiny regression", &error);
        INFO(error); REQUIRE(original_library);
        libraries[0] = (__bridge id<MTLLibrary>)original_library->nativeHandle();
        libraries[1] = (__bridge id<MTLLibrary>)production_library->nativeHandle();
        auto descriptor = [MTLSamplerDescriptor new];
        descriptor.minFilter = descriptor.magFilter = MTLSamplerMinMagFilterLinear;
        descriptor.mipFilter = MTLSamplerMipFilterLinear;
        descriptor.sAddressMode = descriptor.tAddressMode = descriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
        sampler = [device newSamplerStateWithDescriptor:descriptor]; REQUIRE(sampler);
        environment_buffer = buffer(sizeof(environment), "Regression environment");
        probes_buffer = buffer(sizeof(probes), "Regression probe constants");
        hero_buffer = buffer(sizeof(hero_constants), "Regression hero constants");
        vertex_constants_buffer = buffer(sizeof(vertex_constants), "Production shiny vertex constants");
        const float positions[3][4]{{-1,-1,0,0},{3,-1,0,0},{-1,3,0,0}};
        const float normals[3][4]{{0,0,1,0},{0,0,1,0},{0,0,1,0}};
        const float texcoords[3][2]{{0,0},{2,0},{0,2}};
        vertex_buffers[0] = buffer(sizeof(positions), "Production shiny positions");
        vertex_buffers[1] = buffer(sizeof(normals), "Production shiny normals");
        vertex_buffers[2] = buffer(sizeof(texcoords), "Production shiny texcoords");
        vertex_buffers[3] = buffer(12, "Production shiny encoded colors");
        std::memcpy(vertex_buffers[0].contents, positions, sizeof(positions));
        std::memcpy(vertex_buffers[1].contents, normals, sizeof(normals));
        std::memcpy(vertex_buffers[2].contents, texcoords, sizeof(texcoords));
        radiance = texture(8,8,MTLPixelFormatRGBA16Float,MTLTextureTypeCubeArray,cube_layers,4);
        hero = texture(8,8,MTLPixelFormatRGBA16Float,MTLTextureTypeCube,1,4);
        sky = texture(8,8,MTLPixelFormatRGBA16Float,MTLTextureTypeCube);
        diffuse = texture(8,8,MTLPixelFormatRGBA16Float);
    }
    void setFormat(MTLPixelFormat value)
    {
        format = value;
        bytes_per_pixel = value == MTLPixelFormatRGBA32Float ? 16 : value == MTLPixelFormatRGBA16Float ? 8 : 4;
        for (unsigned variant = 0; variant < 2; ++variant)
        {
            NSError* native_error = nil;
            auto descriptor = [MTLRenderPipelineDescriptor new];
            descriptor.label = variant ? @"Actual production shiny" : @"Frozen original shiny";
            descriptor.vertexFunction = [libraries[variant] newFunctionWithName:@"almetal_world_shiny_vertex"];
            descriptor.fragmentFunction = [libraries[variant] newFunctionWithName:
                variant ? @"almetal_world_shiny_fragment" : @"regression_original_shiny_fragment"];
            REQUIRE(descriptor.vertexFunction);
            REQUIRE(descriptor.fragmentFunction);
            descriptor.colorAttachments[0].pixelFormat = value;
            pipelines[variant] = [device newRenderPipelineStateWithDescriptor:descriptor error:&native_error];
            requireNative(pipelines[variant] != nil, native_error ? native_error.description.UTF8String : "Shiny PSO");
            output[variant] = texture(image_width,image_height,value);
            readback[variant] = buffer(image_width * image_height * bytes_per_pixel, "Packed production target comparison");
        }
        if (value == MTLPixelFormatRGBA32Float)
        {
            NSError* native_error = nil;
            auto descriptor = [MTLRenderPipelineDescriptor new];
            descriptor.label = @"Frozen original hero-weight census";
            descriptor.vertexFunction = [libraries[0] newFunctionWithName:@"almetal_world_shiny_vertex"];
            descriptor.fragmentFunction = [libraries[0] newFunctionWithName:@"regression_original_weight_fragment"];
            REQUIRE(descriptor.vertexFunction);
            REQUIRE(descriptor.fragmentFunction);
            descriptor.colorAttachments[0].pixelFormat = value;
            census_pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&native_error];
            requireNative(census_pipeline != nil, native_error ? native_error.description.UTF8String : "Census PSO");
        }
    }
    void configure(const Case& c)
    {
        environment = {}; environment.environment_config[0] = c.legacy_sky;
        vertex_constants = {};
        identity(vertex_constants.modelview_projection); identity(vertex_constants.modelview);
        identity(vertex_constants.texture_matrix);
        vertex_constants.modelview[0] = vertex_constants.modelview[5] = c.span;
        vertex_constants.modelview[14] = -4;
        for (unsigned i=0;i<3;++i) vertex_constants.normal_matrix[i][i] = 1;
        vertex_constants.config[0] = 1; vertex_constants.config[1] = 1;
        auto colors = static_cast<std::uint8_t*>(vertex_buffers[3].contents);
        for (unsigned i=0;i<3;++i)
        {
            colors[i*4] = 215; colors[i*4+1] = 230; colors[i*4+2] = 240;
            colors[i*4+3] = static_cast<std::uint8_t>(std::lround(c.intensity*255.f));
        }
        probes = {}; probes.config[0] = c.layout == 0 ? 1 : cube_layers; probes.config[1] = 3;
        std::fill(std::begin(probes.ref_neighbor), std::end(probes.ref_neighbor), -1);
        for (auto& bucket:probes.ref_bucket) bucket[0] = 1;
        for (unsigned i=0;i<cube_layers;++i)
        {
            identity(probes.ref_box[i]);
            probes.ref_sphere[i][0] = .013f*i; probes.ref_sphere[i][1] = .011f*i;
            probes.ref_sphere[i][2] = -4; probes.ref_sphere[i][3] = i ? 9 : 4096;
            probes.ref_params[i][0] = probes.ref_params[i][1] = 1;
            probes.ref_params[i][2] = .7; probes.ref_params[i][3] = -6;
            probes.ref_index[i][0] = i; probes.ref_index[i][1] = -1; probes.ref_index[i][3] = i%2;
        }
        probes.ref_index[0][3] = 0;
        if (c.layout >= 2)
        {
            probes.ref_index[1][1] = 0; probes.ref_index[1][2] = 31;
            for (unsigned i=0;i<31;++i) probes.ref_neighbor[i] = i+2;
        }
        if (c.layout == 3) probes.ref_params[1][2] = 0; // Accepted/cap-consuming zero-fade owner.
        if (c.layout == 4)
        {
            probes.ref_index[2][3] = -1;
            probes.ref_box[2][0] = probes.ref_box[2][5] = probes.ref_box[2][10] = .25;
            probes.ref_box[2][14] = 1;
        }
        if (c.layout == 5) probes.ref_index[1][2] = 0; // Empty first-owner neighbors are final.
        if (c.rotated)
        {
            probes.env_matrix[0][0] = 0; probes.env_matrix[0][1] = 1;
            probes.env_matrix[1][0] = -1; probes.env_matrix[1][1] = 0;
        }
        hero_constants = {}; identity(hero_constants.hero_box);
        hero_constants.hero_box[0] = hero_constants.hero_box[5] = hero_constants.hero_box[10] = .15;
        hero_constants.hero_box[14] = .6;
        hero_constants.hero_sphere[2] = -4; hero_constants.hero_sphere[3] = c.span > 2 ? 2.5 : 8;
        hero_constants.clip_plane[0] = c.clip == .5f ? .2f : 0;
        hero_constants.clip_plane[3] = c.clip == .5f ? -.05f : c.clip;
        hero_constants.config[0] = c.hero_shape; hero_constants.config[1] = c.enabled;
        hero_constants.config[2] = 3;
        if (c.pure_partial)
        {
            hero_constants.hero_sphere[0] = 100; hero_constants.hero_sphere[3] = 400;
        }
        std::memcpy(environment_buffer.contents,&environment,sizeof(environment));
        std::memcpy(probes_buffer.contents,&probes,sizeof(probes));
        std::memcpy(hero_buffer.contents,&hero_constants,sizeof(hero_constants));
        std::memcpy(vertex_constants_buffer.contents,&vertex_constants,sizeof(vertex_constants));
        auto command = [queue commandBuffer]; auto encoder = [command blitCommandEncoder];
        for (unsigned layer=0;layer<cube_layers;++layer) for (unsigned face=0;face<6;++face) for (unsigned mip=0;mip<4;++mip)
        {
            const unsigned size = 8 >> mip; std::vector<_Float16> pixels(size*size*4);
            for (unsigned y=0;y<size;++y) for (unsigned x=0;x<size;++x)
            {
                const auto at = (y*size+x)*4;
                const float modulation = c.nonuniform ? .45f+.03f*face+.025f*mip+.02f*x/size+.02f*y/size : 1;
                pixels[at] = c.ordinary * modulation * (1+.002f*(layer%7));
                pixels[at+1] = c.ordinary * modulation * .5f;
                pixels[at+2] = c.ordinary * modulation * .25f; pixels[at+3] = 1;
            }
            upload(encoder,radiance,layer*6+face,mip,pixels.data(),size*8);
        }
        for (unsigned face=0;face<6;++face) for (unsigned mip=0;mip<4;++mip)
        {
            const unsigned size = 8 >> mip; std::vector<_Float16> pixels(size*size*4);
            for (unsigned y=0;y<size;++y) for (unsigned x=0;x<size;++x)
            {
                const auto at = (y*size+x)*4;
                const float modulation = c.nonuniform ? .4f+.04f*face+.03f*mip+.05f*x/size+.02f*y/size : 1;
                pixels[at] = c.hero*modulation; pixels[at+1] = c.hero*modulation*.5f;
                pixels[at+2] = c.hero*modulation*.25f; pixels[at+3] = 1;
            }
            upload(encoder,hero,face,mip,pixels.data(),size*8);
            if (mip == 0) upload(encoder,sky,face,0,pixels.data(),size*8);
        }
        std::array<_Float16,8*8*4> diffuse_pixels{};
        for (unsigned i=0;i<64;++i)
        {
            diffuse_pixels[i*4] = c.zero_diffuse ? 0 : .2;
            diffuse_pixels[i*4+1] = c.zero_diffuse ? 0 : .4;
            diffuse_pixels[i*4+2] = c.zero_diffuse ? 0 : .6; diffuse_pixels[i*4+3] = 1;
        }
        upload(encoder,diffuse,0,0,diffuse_pixels.data(),64);
        [encoder endEncoding]; finish(command);
    }
    void render(unsigned variant, bool census = false)
    {
        auto command = [queue commandBuffer]; auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = output[variant];
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(.9,.8,.7,0);
        auto encoder = [command renderCommandEncoderWithDescriptor:pass]; REQUIRE(encoder);
        [encoder setRenderPipelineState:census ? census_pipeline : pipelines[variant]];
        [encoder setViewport:MTLViewport{0,0,image_width,image_height,0,1}];
        [encoder setCullMode:MTLCullModeNone];
        for (unsigned i=0;i<4;++i) [encoder setVertexBuffer:vertex_buffers[i] offset:0 atIndex:i];
        [encoder setVertexBuffer:vertex_constants_buffer offset:0 atIndex:4];
        [encoder setFragmentBuffer:environment_buffer offset:0 atIndex:0];
        [encoder setFragmentBuffer:probes_buffer offset:0 atIndex:1];
        [encoder setFragmentBuffer:hero_buffer offset:0 atIndex:4];
        if (!census)
        {
            for (unsigned i=0;i<32;++i) [encoder setFragmentTexture:diffuse atIndex:i];
            [encoder setFragmentTexture:radiance atIndex:32]; [encoder setFragmentTexture:hero atIndex:34];
            [encoder setFragmentTexture:sky atIndex:39];
            [encoder setFragmentSamplerState:sampler atIndex:0]; [encoder setFragmentSamplerState:sampler atIndex:1];
        }
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];
        auto blit = [command blitCommandEncoder];
        [blit copyFromTexture:output[variant] sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
            sourceSize:MTLSizeMake(image_width,image_height,1) toBuffer:readback[variant] destinationOffset:0
            destinationBytesPerRow:image_width*bytes_per_pixel
            destinationBytesPerImage:image_width*image_height*bytes_per_pixel];
        [blit endEncoding]; finish(command);
    }
    void compare(const Case& c)
    {
        render(0); render(1);
        const auto bytes = image_width*image_height*bytes_per_pixel;
        CHECK(std::memcmp(readback[0].contents,readback[1].contents,bytes) == 0);
        // A clear-only/failed rasterization cannot pass by producing two
        // identical untouched attachments. Alpha must be opaque everywhere.
        if (format != MTLPixelFormatRG11B10Float)
        {
            unsigned bad_alpha = 0;
            for (unsigned i=0;i<image_width*image_height;++i)
            {
                const auto pixel = static_cast<const std::uint8_t*>(readback[1].contents) + i*bytes_per_pixel;
                if (format == MTLPixelFormatRGBA32Float)
                {
                    float alpha; std::memcpy(&alpha,pixel+12,4); bad_alpha += alpha != 1;
                    for (unsigned channel=0;channel<3;++channel)
                    { float value;std::memcpy(&value,pixel+channel*4,4);REQUIRE(std::isfinite(value)); }
                }
                else if (format == MTLPixelFormatRGBA16Float)
                { std::uint16_t alpha;std::memcpy(&alpha,pixel+6,2);bad_alpha += alpha != 0x3c00; }
                else bad_alpha += pixel[3] != 255;
            }
            CHECK(bad_alpha == 0);
        }
        if (format == MTLPixelFormatRGBA32Float)
        {
            render(0,true);
            const auto values = static_cast<const float*>(readback[0].contents);
            unsigned full=0,partial=0,zero=0;
            for (unsigned i=0;i<image_width*image_height;++i)
            { full+=values[i*4+1]!=0;partial+=values[i*4+2]!=0;zero+=values[i*4+3]!=0; }
            CHECK(full+partial+zero == image_width*image_height);
            if (c.enabled && c.clip == 1.f && c.span <= 2.f && !c.pure_partial)
                CHECK(full == image_width*image_height);
            if (c.pure_partial) { CHECK(partial == image_width*image_height); CHECK(full == 0); }
            if (!c.enabled || c.clip < 0) CHECK(zero == image_width*image_height);
            if (c.clip == .5f) { CHECK(full > 0); CHECK(partial > 0); CHECK(zero > 0); }
        }
    }
};
}

TEST_CASE("almetal: hero overwrite preserves frozen shiny output and target formats",
          "[almetal][hero-overwrite]")
{
    @autoreleasepool
    {
        Fixture fixture;
        std::vector<Case> cases{
            {"hero disabled",1,2,false}, {"sphere full",1,2}, {"box full",0,2},
            {"sphere mixed",1,2,true,1,4}, {"clip mixed",0,2,true,.5}, {"clip rejected",0,2,true,-1},
            {"zero-fade accepted owner",1,3}, {"box precedence",0,4}, {"empty owner neighbors",1,5},
            {"default only",1,0}, {"legacy sky",1,2,true,1,2,1,1,true},
            {"HDR large ordinary tiny hero",1,2,true,1,2,64000,.000244140625f},
            {"HDR tiny ordinary large hero",0,2,true,1,2,.000244140625f,64000},
            {"signed-zero hero",0,2,true,1,2,64000,-0.f}, {"negative HDR hero",1,2,true,1,2,1,-16}
        };
        auto directional = cases[4]; directional.name = "nonuniform rotated environment";
        directional.nonuniform = directional.rotated = true; cases.push_back(directional);
        auto negative_ordinary = cases[1]; negative_ordinary.name = "negative ordinary HDR";
        negative_ordinary.ordinary = -64000; cases.push_back(negative_ordinary);
        auto partial = cases[1]; partial.name = "pure partial without full pixels";
        partial.pure_partial = true; cases.push_back(partial);
        // Remove the positive diffuse contribution that would otherwise mask
        // signed-zero/final-clamp behavior. No numeric tolerance is used.
        for (const float intensity : {0.f,1.f}) for (const float hero : {0.f,-0.f,-16.f})
        {
            Case c{"zero diffuse endpoint",0,2,true,1,2,64000,hero};
            c.intensity = intensity; c.zero_diffuse = true; cases.push_back(c);
        }
        for (const auto format : {MTLPixelFormatRGBA32Float,MTLPixelFormatRGBA16Float,
                                  MTLPixelFormatRGBA8Unorm,MTLPixelFormatRG11B10Float})
        {
            INFO("target format " << static_cast<unsigned long>(format));
            fixture.setFormat(format);
            for (const auto& c : cases)
            {
                INFO(c.name << "; intensity=" << c.intensity << "; hero=" << c.hero);
                fixture.configure(c); fixture.compare(c);
            }
        }
    }
}
