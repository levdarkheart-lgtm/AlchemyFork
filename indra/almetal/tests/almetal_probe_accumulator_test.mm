/**
 * @file almetal_probe_accumulator_test.mm
 * @brief Compare complete original/candidate Metal libraries on fixed probe inputs.
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
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
// These wrappers call the actual helpers in each complete input source.
// They contain no selection, weighting, blending, or texture-sampling oracle.
// MRTs preserve the three lobes independently; their timings are helper-fixture
// timings, not production-fragment or whole-viewer measurements.
constexpr const char* wrapper = R"metal(
struct ProbeCheckInputs { uint4 flags; float4 surface; };
struct ProbeCheckOutputs {
    float4 ambient [[color(0)]];
    float4 gloss [[color(1)]];
    float4 legacy [[color(2)]];
};
fragment ProbeCheckOutputs probe_accumulator_check(
    ALMetalResampleVaryings in [[stage_in]],
    constant ALMetalWorldReflectionProbeConstants& probes [[buffer(0)]],
    constant ProbeCheckInputs& c [[buffer(1)]],
    texturecube_array<float> radiance [[texture(0)]],
    texture2d<uint> irradiance [[texture(1)]], sampler s [[sampler(0)]])
{
    const float3 p = float3((in.texcoord * 2.0 - 1.0) * c.surface.z, c.surface.w);
    const float3 n = normalize(float3(0.17, -0.23, 1.0));
    float3 a = 0.0, g = 0.0, l = 0.0;
    if (c.flags.z != 0u) {
        g = almetal_sample_water_probes(p, n, c.surface.x, radiance, s, probes);
    } else {
        almetal_sample_base_probes(c.flags.x != 0u, p, n, c.surface.x,
            c.surface.y, float3(0.13, 0.21, 0.31), c.flags.y != 0u,
            radiance, irradiance, s, probes, a, g, l);
    }
    ProbeCheckOutputs out;
    out.ambient = float4(a, 1.0); out.gloss = float4(g, 1.0);
    out.legacy = float4(l, 1.0); return out;
}
)metal";

struct alignas(16) Inputs
{
    std::uint32_t flags[4]{}; // PBR, classic, water
    float surface[4]{0.f, 0.f, 2.f, -4.f}; // gloss, environment, span, eye Z
};
struct alignas(16) ShinyConstants
{
    float modelview_projection[16]{}, modelview[16]{};
    float normal_matrix[3][4]{}, rigged_normal_matrix[3][4]{};
    float texture_matrix[16]{};
    std::uint32_t config[4]{};
    float avatar[4][4]{}, mirror_clip_plane[4]{};
};
static_assert(sizeof(Inputs) == 32 && sizeof(ShinyConstants) == 384);
static_assert(sizeof(ALMetalWorldReflectionProbeConstants) == 49216);

void identity(float* m)
{
    std::fill(m, m + 16, 0.f);
    for (unsigned i = 0; i != 4; ++i) m[i * 5] = 1.f;
}
std::string requiredEnvironment(const char* key)
{
    const char* value = std::getenv(key);
    INFO(key);
    REQUIRE(value);
    REQUIRE(*value);
    REQUIRE(std::filesystem::is_regular_file(value));
    return value;
}
std::string readSource(const std::string& path)
{
    std::ifstream input(path);
    REQUIRE(input.good());
    return {std::istreambuf_iterator<char>(input), {}};
}
void finish(id<MTLCommandBuffer> command)
{
    REQUIRE(command);
    [command commit];
    [command waitUntilCompleted];
    INFO((command.error ? command.error.description.UTF8String : "completed native command"));
    REQUIRE(command.status == MTLCommandBufferStatusCompleted);
}

struct Layout
{
    const char* name;
    unsigned count;
    unsigned kind = 0; // automatic, manual, mixed, boxes, 64 candidates, accepted cap, fades
    float span = 2.f;
};
constexpr std::array layouts{
    Layout{"configured-zero-default", 0}, Layout{"default-only", 1},
    Layout{"twenty-automatic", 20}, Layout{"thirty-nine-automatic", 39},
    Layout{"twenty-manual", 20, 1}, Layout{"thirty-nine-mixed", 39, 2},
    Layout{"manual-box-precedence", 39, 3}, Layout{"sphere-volume-edge", 20, 2, 6.f},
    Layout{"box-volume-edge", 20, 3, 6.f}, Layout{"neighbor-64-late-valid", 70, 4},
    Layout{"accepted-32-cap", 70, 5}, Layout{"zero-and-partial-fades", 39, 6},
    Layout{"neighbor-invalid-indices", 39, 7}, Layout{"empty-neighbors-final", 39, 8},
};
struct Lobe { const char* name; bool pbr, classic, water; float gloss, environment; };
constexpr std::array lobes{
    Lobe{"ambient-only", false, false, false, 0.f, 0.f},
    Lobe{"legacy-gloss", false, false, false, .65f, 0.f},
    Lobe{"legacy-environment", false, false, false, 0.f, .75f},
    Lobe{"legacy-all-lobes", false, false, false, .65f, .75f},
    Lobe{"pbr-rough", true, false, false, .15f, 0.f},
    Lobe{"pbr-gloss", true, false, false, .95f, 0.f},
    Lobe{"classic-legacy", false, true, false, .65f, .75f},
    Lobe{"classic-ambient", false, true, false, 0.f, 0.f},
    Lobe{"classic-pbr", true, true, false, .45f, 0.f},
    Lobe{"water", false, false, true, .7f, 0.f},
};

struct Fixture
{
    static constexpr unsigned layers = 72, cube_size = 8;
    unsigned width, height;
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    id<MTLCommandQueue> queue = nil;
    std::array<id<MTLLibrary>, 2> helper_library{}, production_library{};
    std::array<id<MTLRenderPipelineState>, 2> helper_pso{}, base_pso{}, simple_pso{};
    std::array<std::array<id<MTLTexture>, 3>, 2> output{};
    std::array<std::array<id<MTLBuffer>, 3>, 2> readback{};
    id<MTLTexture> radiance = nil, irradiance = nil, sky = nil;
    std::array<id<MTLTexture>, 13> base_textures{};
    id<MTLSamplerState> sampler = nil;
    id<MTLBuffer> probes_buffer = nil, inputs_buffer = nil, base_buffer = nil;
    id<MTLBuffer> hero_buffer = nil, ssr_buffer = nil, environment_buffer = nil;
    id<MTLBuffer> lighting_buffer = nil, shiny_buffer = nil;
    std::array<id<MTLBuffer>, 4> vertex_buffers{};
    ALMetalWorldReflectionProbeConstants probes{};
    ALMetalWorldBaseLightingConstants base{};
    Inputs inputs{};
    const char* layout_name = "unset";
    const char* lobe_name = "unset";
    unsigned layout_kind = 0;
    bool mixed_gbuffer = false;

    id<MTLBuffer> buffer(std::size_t bytes)
    {
        auto b = [device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
        REQUIRE(b); return b;
    }
    id<MTLTexture> texture(unsigned w, unsigned h, MTLPixelFormat format,
        MTLTextureType type = MTLTextureType2D, unsigned array_count = 1,
        unsigned mips = 1, bool target = false)
    {
        auto d = [MTLTextureDescriptor new]; d.width = w; d.height = h;
        d.pixelFormat = format; d.textureType = type; d.arrayLength = array_count;
        d.mipmapLevelCount = mips;
        d.storageMode = target ? MTLStorageModePrivate : MTLStorageModeShared;
        d.usage = target ? MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead
                         : MTLTextureUsageShaderRead;
        auto t = [device newTextureWithDescriptor:d]; REQUIRE(t); return t;
    }
    id<MTLRenderPipelineState> pipeline(id<MTLLibrary> library,
        const char* vertex, const char* fragment, unsigned attachments)
    {
        auto d = [MTLRenderPipelineDescriptor new];
        d.vertexFunction = [library newFunctionWithName:[NSString stringWithUTF8String:vertex]];
        d.fragmentFunction = [library newFunctionWithName:[NSString stringWithUTF8String:fragment]];
        INFO(vertex); INFO(fragment); REQUIRE(d.vertexFunction); REQUIRE(d.fragmentFunction);
        for (unsigned a = 0; a != attachments; ++a) d.colorAttachments[a].pixelFormat = MTLPixelFormatRGBA32Float;
        NSError* error = nil;
        auto p = [device newRenderPipelineStateWithDescriptor:d error:&error];
        INFO((error ? error.description.UTF8String : "native pipeline")); REQUIRE(p); return p;
    }
    Fixture(unsigned w = 64, unsigned h = 40) : width(w), height(h)
    {
        REQUIRE(device); queue = [device newCommandQueue]; REQUIRE(queue);
        const std::array sources{
            requiredEnvironment("ALCHEMY_PROBE_TEST_BASELINE_SOURCE"),
            requiredEnvironment("ALCHEMY_PROBE_TEST_CANDIDATE_SOURCE")};
        const std::array libraries{
            requiredEnvironment("ALCHEMY_PROBE_TEST_BASELINE_LIBRARY"),
            requiredEnvironment("ALCHEMY_PROBE_TEST_CANDIDATE_LIBRARY")};
        for (unsigned variant = 0; variant != 2; ++variant)
        {
            NSError* error = nil;
            auto options = [MTLCompileOptions new]; options.mathMode = MTLMathModeFast;
            options.languageVersion = MTLLanguageVersion3_1;
            const auto source = readSource(sources[variant]) + wrapper;
            helper_library[variant] = [device newLibraryWithSource:
                [NSString stringWithUTF8String:source.c_str()] options:options error:&error];
            INFO((error ? error.description.UTF8String : "complete source plus wrappers"));
            REQUIRE(helper_library[variant]);
            production_library[variant] = [device newLibraryWithURL:
                [NSURL fileURLWithPath:[NSString stringWithUTF8String:libraries[variant].c_str()]] error:&error];
            INFO((error ? error.description.UTF8String : "offline optimized complete production library"));
            REQUIRE(production_library[variant]);
            helper_pso[variant] = pipeline(helper_library[variant], "almetal_resample_vertex", "probe_accumulator_check", 3);
            base_pso[variant] = pipeline(production_library[variant], "almetal_world_base_light_vertex", "almetal_world_base_light_geometry_fragment", 1);
            simple_pso[variant] = pipeline(production_library[variant], "almetal_world_shiny_vertex", "almetal_world_pre_water_simple_fragment", 1);
            for (unsigned a = 0; a != 3; ++a)
            {
                output[variant][a] = texture(width, height, MTLPixelFormatRGBA32Float, MTLTextureType2D, 1, 1, true);
                readback[variant][a] = buffer(width * height * 16);
            }
        }
        auto s = [MTLSamplerDescriptor new]; s.minFilter = s.magFilter = MTLSamplerMinMagFilterLinear;
        s.mipFilter = MTLSamplerMipFilterLinear;
        s.sAddressMode = s.tAddressMode = s.rAddressMode = MTLSamplerAddressModeClampToEdge;
        sampler = [device newSamplerStateWithDescriptor:s]; REQUIRE(sampler);
        probes_buffer = buffer(sizeof(probes)); inputs_buffer = buffer(sizeof(inputs));
        base_buffer = buffer(sizeof(base)); hero_buffer = buffer(sizeof(ALMetalWorldHeroProbeConstants));
        ssr_buffer = buffer(sizeof(ALMetalWorldScreenSpaceReflectionConstants));
        std::memset(hero_buffer.contents, 0, hero_buffer.length); std::memset(ssr_buffer.contents, 0, ssr_buffer.length);
        environment_buffer = buffer(sizeof(ALMetalWorldAlphaEnvironment));
        lighting_buffer = buffer(sizeof(ALMetalWorldLegacyAlphaLighting)); shiny_buffer = buffer(sizeof(ShinyConstants));
        initializeTextures(); initializeVertices();
    }
    void initializeTextures()
    {
        radiance = texture(cube_size, cube_size, MTLPixelFormatRGBA16Float, MTLTextureTypeCubeArray, layers, 4);
        sky = texture(cube_size, cube_size, MTLPixelFormatRGBA16Float, MTLTextureTypeCube, 1, 4);
        for (unsigned layer = 0; layer != layers; ++layer)
        for (unsigned face = 0; face != 6; ++face)
        for (unsigned mip = 0; mip != 4; ++mip)
        {
            const unsigned size = cube_size >> mip;
            std::vector<_Float16> texels(size * size * 4);
            for (unsigned y = 0; y != size; ++y) for (unsigned x = 0; x != size; ++x)
            {
                const auto at = (y * size + x) * 4;
                texels[at] = .11f + .007f * layer + .013f * face + .025f * mip + .02f * x / size;
                texels[at+1] = .21f + .003f * layer + .017f * face + .015f * mip + .03f * y / size;
                texels[at+2] = .41f + .002f * layer + .011f * face + .035f * mip;
                texels[at+3] = 1;
            }
            [radiance replaceRegion:MTLRegionMake2D(0,0,size,size) mipmapLevel:mip
                slice:layer * 6 + face withBytes:texels.data() bytesPerRow:size * 8 bytesPerImage:size * size * 8];
            if (layer == 0) [sky replaceRegion:MTLRegionMake2D(0,0,size,size) mipmapLevel:mip
                slice:face withBytes:texels.data() bytesPerRow:size * 8 bytesPerImage:size * size * 8];
        }
        irradiance = texture(4, layers, MTLPixelFormatRGBA32Uint);
        std::vector<_Float16> sh(layers * 32);
        for (unsigned layer = 0; layer != layers; ++layer)
            for (unsigned coefficient = 0; coefficient != 27; ++coefficient)
                sh[layer * 32 + coefficient] = coefficient < 3
                    ? .8f + .013f * layer + .17f * coefficient
                    : .009f * (int(coefficient % 5) - 2);
        [irradiance replaceRegion:MTLRegionMake2D(0,0,4,layers) mipmapLevel:0 withBytes:sh.data() bytesPerRow:64];
        for (unsigned slot = 0; slot != base_textures.size(); ++slot)
        {
            if (slot == 6) { base_textures[slot] = radiance; continue; }
            if (slot == 7) { base_textures[slot] = irradiance; continue; }
            if (slot == 9 || slot == 12) { base_textures[slot] = sky; continue; }
            base_textures[slot] = texture(width, height, MTLPixelFormatRGBA32Float);
        }
    }
    void initializeVertices()
    {
        const float positions[3][4]{{-1,-1,0,0},{3,-1,0,0},{-1,3,0,0}};
        const float normals[3][4]{{.17f,-.23f,1,0},{.17f,-.23f,1,0},{.17f,-.23f,1,0}};
        const float uv[3][2]{{0,0},{2,0},{0,2}};
        const std::uint8_t colors[3][4]{{220,180,140,170},{220,180,140,170},{220,180,140,170}};
        const std::array<const void*,4> bytes{positions,normals,uv,colors};
        const std::array<std::size_t,4> sizes{sizeof(positions),sizeof(normals),sizeof(uv),sizeof(colors)};
        for (unsigned i = 0; i != 4; ++i) { vertex_buffers[i] = buffer(sizes[i]); std::memcpy(vertex_buffers[i].contents, bytes[i], sizes[i]); }
    }
    void configure(const Layout& layout, const Lobe& lobe, bool mixed_material = false)
    {
        layout_name = layout.name; lobe_name = lobe.name; layout_kind = layout.kind; mixed_gbuffer = mixed_material;
        probes = {}; probes.config[0] = layout.count; probes.config[1] = 3;
        std::fill(std::begin(probes.ref_neighbor), std::end(probes.ref_neighbor), -1);
        for (auto& bucket : probes.ref_bucket) bucket[0] = 1;
        for (unsigned i = 0; i != 256; ++i)
        {
            identity(probes.ref_box[i]);
            probes.ref_sphere[i][0] = .019f * (i % 7); probes.ref_sphere[i][1] = -.017f * (i % 5);
            probes.ref_sphere[i][2] = -4; probes.ref_sphere[i][3] = i == 0 ? 4096.f : 4.f;
            probes.ref_params[i][0] = .55f + .1f * (i % 9); probes.ref_params[i][1] = .7f + .04f * (i % 6);
            probes.ref_params[i][2] = .65f; probes.ref_params[i][3] = -10.f;
            probes.ref_index[i][0] = i % layers; probes.ref_index[i][1] = -1;
            probes.ref_index[i][3] = layout.kind == 0 ? 0 : layout.kind == 1 ? 1 : int(i % 2);
        }
        probes.ref_index[0][3] = 0;
        if (layout.kind == 3)
            for (unsigned i = 2; i < layout.count; i += 3)
            {
                probes.ref_index[i][3] = -1;
                probes.ref_box[i][0] = probes.ref_box[i][5] = .35f;
                probes.ref_box[i][10] = .25f; probes.ref_box[i][14] = 1.f;
            }
        if (layout.kind == 4)
        {
            probes.ref_index[1][1] = 0; probes.ref_index[1][2] = 64;
            for (unsigned n = 0; n != 64; ++n) probes.ref_neighbor[n] = n + 2;
            for (unsigned i = 2; i != 65; ++i) probes.ref_sphere[i][0] = 10000.f;
        }
        if (layout.kind == 5)
        {
            probes.ref_index[1][1] = 0; probes.ref_index[1][2] = 64;
            for (unsigned n = 0; n != 64; ++n) probes.ref_neighbor[n] = n + 2;
        }
        if (layout.kind == 6)
            for (unsigned i = 1; i < layout.count; ++i) probes.ref_params[i][2] = i % 3 == 0 ? 0.f : .015f * (i % 5);
        if (layout.kind == 7)
        {
            probes.ref_index[1][1] = 0; probes.ref_index[1][2] = 64;
            for (unsigned n = 0; n != 64; ++n) probes.ref_neighbor[n] = n % 3 == 0 ? -1 : n % 3 == 1 ? 256 : int(n % 37 + 2);
        }
        if (layout.kind == 8) { probes.ref_index[1][1] = 0; probes.ref_index[1][2] = 0; }
        inputs = {}; inputs.flags[0] = lobe.pbr; inputs.flags[1] = lobe.classic; inputs.flags[2] = lobe.water;
        inputs.surface[0] = lobe.gloss; inputs.surface[1] = lobe.environment;
        inputs.surface[2] = layout.span; inputs.surface[3] = -4;
        std::memcpy(probes_buffer.contents, &probes, sizeof(probes));
        std::memcpy(inputs_buffer.contents, &inputs, sizeof(inputs));
        configureProduction(lobe, layout.span, mixed_material);
    }
    void configureProduction(const Lobe& lobe, float span, bool mixed_material)
    {
        base = {}; identity(base.inverse_projection);
        base.inverse_projection[0] = base.inverse_projection[5] = span;
        base.inverse_projection[10] = 0; base.inverse_projection[14] = -4;
        base.light_direction[2] = 1; base.lighting[0] = base.lighting[1] = 1;
        base.atmos_amblit[0] = .18f; base.atmos_amblit[1] = .25f; base.atmos_amblit[2] = .33f;
        base.config[1] = lobe.classic;
        std::memcpy(base_buffer.contents, &base, sizeof(base));
        ALMetalWorldAlphaEnvironment environment{};
        environment.light_direction[2] = 1; environment.atmos_sunlit[3] = 1;
        environment.water_plane[3] = 1; environment.water_fog[2] = 1;
        environment.atmospheric[3] = 1; environment.glow_max_y[2] = 1;
        for (unsigned i = 0; i != 3; ++i) environment.eye_to_sky[i][i] = 1;
        environment.atmos_sunlit_linear[3] = 1;
        std::copy_n(base.atmos_amblit, 3, environment.atmos_amblit);
        ALMetalWorldLegacyAlphaLighting lighting{}; lighting.parameters[3] = lobe.classic;
        ShinyConstants shiny{}; identity(shiny.modelview_projection); identity(shiny.modelview); identity(shiny.texture_matrix);
        shiny.modelview[0] = shiny.modelview[5] = span; shiny.modelview[14] = -4;
        for (unsigned i = 0; i != 3; ++i) shiny.normal_matrix[i][i] = 1;
        shiny.config[0] = 1; shiny.config[1] = 1;
        std::memcpy(environment_buffer.contents, &environment, sizeof(environment));
        std::memcpy(lighting_buffer.contents, &lighting, sizeof(lighting));
        std::memcpy(shiny_buffer.contents, &shiny, sizeof(shiny));
        std::vector<float> pixels(width * height * 4);
        for (unsigned slot = 0; slot < base_textures.size(); ++slot)
        {
            if (slot == 6 || slot == 7 || slot == 9 || slot == 12) continue;
            for (unsigned y = 0; y != height; ++y) for (unsigned x = 0; x != width; ++x)
            {
                const bool pbr = mixed_material ? x >= width / 2 : lobe.pbr;
                const std::array<float,4> color = slot == 0 ? std::array<float,4>{.25f,.4f,.6f,0.f}
                    : slot == 1 ? (pbr ? std::array<float,4>{.8f,1.f-lobe.gloss,.2f,0.f} : std::array<float,4>{.3f,.25f,.2f,lobe.gloss})
                    : slot == 2 ? std::array<float,4>{.55f,.43f,pbr ? .5f : lobe.environment,pbr ? .67f : 0.f}
                    : slot == 4 ? std::array<float,4>{.5f,0,0,0}
                    : slot == 5 ? std::array<float,4>{1,1,0,0}
                    : slot == 8 ? std::array<float,4>{.6f,.2f,0,0}
                    : std::array<float,4>{0,0,0,0};
                std::copy(color.begin(), color.end(), pixels.begin() + (y * width + x) * 4);
            }
            [base_textures[slot] replaceRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0 withBytes:pixels.data() bytesPerRow:width * 16];
        }
    }
    enum class Path { helper, base, simple };
    struct Timing { double gpu_ns, wall_ns; };
    Timing render(unsigned variant, Path path, bool copy = true, unsigned draws = 1)
    {
        const auto begin = std::chrono::steady_clock::now();
        auto command = [queue commandBuffer]; REQUIRE(command);
        command.label = path == Path::helper ? @"Isolated production probe helper" : path == Path::base ? @"Production base lighting fragment" : @"Production forward simple alpha fragment";
        const unsigned attachments = path == Path::helper ? 3 : 1;
        auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
        for (unsigned a = 0; a != attachments; ++a)
        {
            pass.colorAttachments[a].texture = output[variant][a];
            pass.colorAttachments[a].loadAction = MTLLoadActionClear;
            pass.colorAttachments[a].storeAction = MTLStoreActionStore;
            pass.colorAttachments[a].clearColor = MTLClearColorMake(-1,-1,-1,-1);
        }
        auto encoder = [command renderCommandEncoderWithDescriptor:pass]; REQUIRE(encoder);
        [encoder setViewport:MTLViewport{0,0,double(width),double(height),0,1}];
        [encoder setCullMode:MTLCullModeNone];
        if (path == Path::helper)
        {
            [encoder setRenderPipelineState:helper_pso[variant]];
            [encoder setFragmentBuffer:probes_buffer offset:0 atIndex:0];
            [encoder setFragmentBuffer:inputs_buffer offset:0 atIndex:1];
            [encoder setFragmentTexture:radiance atIndex:0]; [encoder setFragmentTexture:irradiance atIndex:1];
            [encoder setFragmentSamplerState:sampler atIndex:0];
        }
        else if (path == Path::base)
        {
            [encoder setRenderPipelineState:base_pso[variant]];
            [encoder setVertexBuffer:base_buffer offset:0 atIndex:0];
            [encoder setFragmentBuffer:base_buffer offset:0 atIndex:0];
            [encoder setFragmentBuffer:probes_buffer offset:0 atIndex:1];
            [encoder setFragmentBuffer:hero_buffer offset:0 atIndex:2];
            [encoder setFragmentBuffer:ssr_buffer offset:0 atIndex:3];
            for (unsigned slot = 0; slot != base_textures.size(); ++slot) [encoder setFragmentTexture:base_textures[slot] atIndex:slot];
            for (unsigned slot = 0; slot != 3; ++slot) [encoder setFragmentSamplerState:sampler atIndex:slot];
        }
        else
        {
            [encoder setRenderPipelineState:simple_pso[variant]];
            for (unsigned i = 0; i != 4; ++i) [encoder setVertexBuffer:vertex_buffers[i] offset:0 atIndex:i];
            [encoder setVertexBuffer:shiny_buffer offset:0 atIndex:4];
            [encoder setFragmentBuffer:environment_buffer offset:0 atIndex:0];
            [encoder setFragmentBuffer:probes_buffer offset:0 atIndex:1];
            [encoder setFragmentBuffer:lighting_buffer offset:0 atIndex:2];
            for (unsigned slot = 0; slot != 32; ++slot) [encoder setFragmentTexture:base_textures[0] atIndex:slot];
            [encoder setFragmentTexture:radiance atIndex:32]; [encoder setFragmentTexture:irradiance atIndex:37];
            [encoder setFragmentTexture:sky atIndex:39];
            [encoder setFragmentSamplerState:sampler atIndex:0]; [encoder setFragmentSamplerState:sampler atIndex:1];
        }
        for (unsigned draw = 0; draw != draws; ++draw) [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];
        if (copy)
        {
            auto blit = [command blitCommandEncoder]; REQUIRE(blit);
            for (unsigned a = 0; a != attachments; ++a)
                [blit copyFromTexture:output[variant][a] sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                    sourceSize:MTLSizeMake(width,height,1) toBuffer:readback[variant][a] destinationOffset:0
                    destinationBytesPerRow:width * 16 destinationBytesPerImage:width * height * 16];
            [blit endEncoding];
        }
        finish(command);
        const auto end = std::chrono::steady_clock::now();
        return {(command.GPUEndTime - command.GPUStartTime) * 1e9,
                std::chrono::duration<double,std::nano>(end - begin).count()};
    }
    void compare(Path path)
    {
        render(0, path); render(1, path);
        const unsigned attachments = path == Path::helper ? 3 : 1;
        float max_error = 0.f, max_value = 0.f, max_rgb = 0.f; unsigned failures = 0, nonfinite = 0;
        for (unsigned a = 0; a != attachments; ++a)
        {
            const auto* original = static_cast<const float*>(readback[0][a].contents);
            const auto* candidate = static_cast<const float*>(readback[1][a].contents);
            for (unsigned i = 0; i != width * height * 4; ++i)
            {
                if (!std::isfinite(original[i]) || !std::isfinite(candidate[i])) { ++nonfinite; continue; }
                const float delta = std::abs(original[i] - candidate[i]);
                max_error = std::max(max_error, delta); max_value = std::max(max_value, std::abs(original[i]));
                if (i % 4 != 3) max_rgb = std::max(max_rgb, std::abs(original[i]));
                failures += delta > 2e-5f + 3e-5f * std::abs(original[i]);
            }
        }
        std::printf("probe_accumulator_parity layout=%s count=%.0f kind=%u lobe=%s path=%s mixed_gbuffer=%u max_absolute_error=%.9g max_rgb=%.9g nonfinite=%u failures=%u\n",
            layout_name,probes.config[0],layout_kind,lobe_name,path==Path::helper?"helper-mrt":path==Path::base?"production-base":"production-simple-alpha",
            unsigned(mixed_gbuffer),max_error,max_rgb,nonfinite,failures);
        CAPTURE(max_error, max_value, failures, nonfinite);
        CHECK(nonfinite == 0); CHECK(failures == 0); CHECK(max_rgb > 1e-4f);
        // A cleared or discarded whole output cannot satisfy this guard.
        const auto* first = static_cast<const float*>(readback[0][0].contents);
        CHECK(first[3] >= 0.f);
    }
};
}

TEST_CASE("almetal: scalar probe accumulators preserve production lobes and fragments",
          "[almetal][.probe-accumulator-parity]")
{
    @autoreleasepool
    {
        Fixture f;
        for (const auto& layout : layouts) for (const auto& lobe : lobes)
        {
            CAPTURE(layout.name, lobe.name);
            f.configure(layout, lobe); f.compare(Fixture::Path::helper);
        }
        for (const unsigned count : {0u, 1u, 20u, 39u})
        for (const unsigned kind : {0u, 2u, 3u, 6u})
        for (const auto& lobe : lobes)
        {
            if (lobe.water) continue;
            CAPTURE(count, kind, lobe.name);
            const Layout layout{"production-fragments", count, kind};
            f.configure(layout, lobe); f.compare(Fixture::Path::base);
            f.compare(Fixture::Path::simple);
        }
        f.configure(Layout{"mixed-gbuffer",39,3}, lobes[3], true);
        f.compare(Fixture::Path::base);
    }
}

TEST_CASE("almetal: bounded paired probe accumulator helper and production fragment costs",
          "[almetal][.probe-accumulator-cost]")
{
    @autoreleasepool
    {
        Fixture f(512,288);
        constexpr unsigned draws = 1, pairs = 5;
        for (const unsigned count : {1u,39u}) for (const unsigned lobe_index : {0u,3u,5u,9u})
        {
            const auto& lobe = lobes[lobe_index];
            const Layout layout{"bounded-cost",count,2};
            f.configure(layout,lobe);
            for (const auto path : {Fixture::Path::helper,Fixture::Path::base,Fixture::Path::simple})
            {
                if (lobe.water && path != Fixture::Path::helper) continue;
                if (path == Fixture::Path::simple && lobe_index != 0) continue;
                for (unsigned warmup=0;warmup!=2;++warmup)
                    for (unsigned variant=0;variant!=2;++variant) f.render(variant,path,false,draws);
                for (unsigned pair=0;pair!=pairs;++pair) for (unsigned ordinal=0;ordinal!=2;++ordinal)
                {
                    const unsigned variant = (pair + ordinal) % 2;
                    const auto timing = f.render(variant,path,false,draws);
                    REQUIRE(timing.gpu_ns > 0);
                    std::printf("probe_accumulator_cost count=%u family=mixed lobe=%s path=%s width=%u height=%u draws=%u pair=%u ordinal=%u variant=%s gpu_ns=%.0f owner_wall_ns=%.0f\n",
                        count,lobe.name,path==Fixture::Path::helper?"helper-mrt":path==Fixture::Path::base?"production-base":"production-simple-alpha",
                        f.width,f.height,draws,pair,ordinal,variant?"candidate":"baseline",timing.gpu_ns,timing.wall_ns);
                }
            }
        }
    }
}
