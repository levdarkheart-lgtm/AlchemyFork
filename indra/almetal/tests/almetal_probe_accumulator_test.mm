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
struct ProbeCheckInputs { uint4 flags; float4 surface; float4 normal; };
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
    const float3 n = normalize(c.normal.xyz);
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
    float normal[4]{.17f,-.23f,1.f,0.f};
};
struct alignas(16) ShinyConstants
{
    float modelview_projection[16]{}, modelview[16]{};
    float normal_matrix[3][4]{}, rigged_normal_matrix[3][4]{};
    float texture_matrix[16]{};
    std::uint32_t config[4]{};
    float avatar[4][4]{}, mirror_clip_plane[4]{};
};
static_assert(sizeof(Inputs) == 48 && sizeof(ShinyConstants) == 384);
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
    Layout{"zero-SH-scale",20,9}, Layout{"SH-scale-regimes",39,10},
    Layout{"rotated-environment",39,11},
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
    std::array<id<MTLRenderPipelineState>, 2> helper_pso{}, base_pso{}, simple_pso{}, shadow_pso{};
    std::array<std::array<id<MTLTexture>, 3>, 2> output{};
    std::array<std::array<id<MTLBuffer>, 3>, 2> readback{};
    std::array<id<MTLTexture>, 2> shadow_output{};
    std::array<id<MTLBuffer>, 2> shadow_readback{};
    id<MTLTexture> shadow_maps = nil, render_depth = nil, alpha_diffuse = nil;
    id<MTLDepthStencilState> readonly_depth = nil;
    id<MTLBuffer> shadow_buffer = nil;
    ALMetalWorldDirectionalShadowConstants shadows{};
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
        const char* vertex, const char* fragment, unsigned attachments, bool shadowed_alpha = false)
    {
        auto d = [MTLRenderPipelineDescriptor new];
        d.vertexFunction = [library newFunctionWithName:[NSString stringWithUTF8String:vertex]];
        d.fragmentFunction = [library newFunctionWithName:[NSString stringWithUTF8String:fragment]];
        INFO(vertex); INFO(fragment); REQUIRE(d.vertexFunction); REQUIRE(d.fragmentFunction);
        for (unsigned a = 0; a != attachments; ++a)
            d.colorAttachments[a].pixelFormat = shadowed_alpha ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA32Float;
        if (shadowed_alpha)
        {
            d.colorAttachments[0].blendingEnabled = true;
            d.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            d.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            d.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
            d.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            d.depthAttachmentPixelFormat = d.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
        }
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
            // These are the SDK defaults used by the original fixture,
            // explicit here so the runtime helper compilation is observable.
            options.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsFast;
            options.optimizationLevel = MTLLibraryOptimizationLevelDefault;
            std::printf("probe_compiler variant=%u language=%lu math_mode=%ld math_functions=%ld optimization=%ld production_library=%s\n",
                variant,static_cast<unsigned long>(options.languageVersion),static_cast<long>(options.mathMode),
                static_cast<long>(options.mathFloatingPointFunctions),static_cast<long>(options.optimizationLevel),libraries[variant].c_str());
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
            // The main-view function constant is intentionally undefined,
            // preserving the complete runtime directional-shadow filter.
            shadow_pso[variant] = pipeline(production_library[variant], "almetal_world_shiny_vertex", "almetal_world_pre_water_simple_shadow_fragment", 1, true);
            shadow_output[variant] = texture(width,height,MTLPixelFormatRGBA16Float,MTLTextureType2D,1,1,true);
            shadow_readback[variant] = buffer(width * height * 8);
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
        shadow_buffer = buffer(sizeof(shadows));
        auto depth = [MTLDepthStencilDescriptor new]; depth.depthCompareFunction = MTLCompareFunctionLessEqual;
        depth.depthWriteEnabled = false; readonly_depth = [device newDepthStencilStateWithDescriptor:depth]; REQUIRE(readonly_depth);
        render_depth = texture(width,height,MTLPixelFormatDepth32Float_Stencil8,MTLTextureType2D,1,1,true);
        initializeTextures(); initializeVertices(); initializeShadows();
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
        uploadSH(false);
        for (unsigned slot = 0; slot != base_textures.size(); ++slot)
        {
            if (slot == 6) { base_textures[slot] = radiance; continue; }
            if (slot == 7) { base_textures[slot] = irradiance; continue; }
            if (slot == 9 || slot == 12) { base_textures[slot] = sky; continue; }
            base_textures[slot] = texture(width, height, MTLPixelFormatRGBA32Float);
        }
    }
    void uploadSH(bool signed_basis)
    {
        std::vector<_Float16> sh(layers * 32);
        for (unsigned layer = 0; layer != layers; ++layer)
            for (unsigned coefficient = 0; coefficient != 27; ++coefficient)
                sh[layer * 32 + coefficient] = coefficient < 3
                    ? (signed_basis ? .12f : .8f) + .013f * layer + .17f * coefficient
                    : (signed_basis ? .65f : .009f) * (int(coefficient % 5) - 2);
        [irradiance replaceRegion:MTLRegionMake2D(0,0,4,layers) mipmapLevel:0 withBytes:sh.data() bytesPerRow:64];
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
    void initializeShadows()
    {
        constexpr unsigned map_size = 128;
        shadow_maps = texture(map_size,map_size,MTLPixelFormatR32Float,MTLTextureType2DArray,4);
        std::vector<float> depths(map_size * map_size);
        for (unsigned cascade = 0; cascade != 4; ++cascade)
        {
            for (unsigned y = 0; y != map_size; ++y) for (unsigned x = 0; x != map_size; ++x)
                depths[y * map_size + x] = ((x / 8 + y / 8 + cascade) % 2 ? .35f : .65f) + .03f * float(x) / map_size;
            [shadow_maps replaceRegion:MTLRegionMake2D(0,0,map_size,map_size) mipmapLevel:0 slice:cascade
                withBytes:depths.data() bytesPerRow:map_size * 4 bytesPerImage:map_size * map_size * 4];
        }
        alpha_diffuse = texture(8,8,MTLPixelFormatRGBA8Unorm_sRGB);
        std::array<std::uint8_t,8 * 8 * 4> texels{};
        for (unsigned i = 0; i != 64; ++i)
        {
            texels[i*4] = 137; texels[i*4+1] = 170; texels[i*4+2] = 203;
            texels[i*4+3] = i % 2 ? 150 : 215;
        }
        [alpha_diffuse replaceRegion:MTLRegionMake2D(0,0,8,8) mipmapLevel:0 withBytes:texels.data() bytesPerRow:32];
    }
    void configure(const Layout& layout, const Lobe& lobe, bool mixed_material = false)
    {
        layout_name = layout.name; lobe_name = lobe.name; layout_kind = layout.kind; mixed_gbuffer = mixed_material;
        probes = {}; probes.config[0] = layout.count; probes.config[1] = 3;
        // The struct's C++ default member initializer already provides this
        // identity. Make it explicit and checked for fixture provenance.
        for (unsigned column = 0; column != 3; ++column) for (unsigned row = 0; row != 4; ++row)
            probes.env_matrix[column][row] = column == row ? 1.f : 0.f;
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
        if (layout.kind == 9) for (auto& params : probes.ref_params) params[0] = 0;
        if (layout.kind == 10) for (unsigned i=0;i!=256;++i)
            probes.ref_params[i][0] = std::array<float,4>{0.f,.5f,1.f,2.f}[i%4];
        if (layout.kind == 11)
        {
            probes.env_matrix[0][0] = 0; probes.env_matrix[0][1] = 1;
            probes.env_matrix[1][0] = -1; probes.env_matrix[1][1] = 0;
        }
        inputs = {}; inputs.flags[0] = lobe.pbr; inputs.flags[1] = lobe.classic; inputs.flags[2] = lobe.water;
        inputs.surface[0] = lobe.gloss; inputs.surface[1] = lobe.environment;
        inputs.surface[2] = layout.span; inputs.surface[3] = -4;
        std::memcpy(probes_buffer.contents, &probes, sizeof(probes));
        std::memcpy(inputs_buffer.contents, &inputs, sizeof(inputs));
        configureProduction(lobe, layout.span, mixed_material);
    }
    void configureHomeAlpha(unsigned count, unsigned family, unsigned quality, bool rotated_environment = false)
    {
        configure(Layout{"home-shadow-alpha",count,family},lobes[0]);
        if (rotated_environment)
        {
            probes.env_matrix[0][0] = 0; probes.env_matrix[0][1] = 1;
            probes.env_matrix[1][0] = -1; probes.env_matrix[1][1] = 0;
            std::memcpy(probes_buffer.contents,&probes,sizeof(probes));
        }
        shadows = {};
        for (unsigned cascade = 0; cascade != 4; ++cascade)
        {
            identity(shadows.shadow_matrix[cascade]);
            shadows.shadow_matrix[cascade][0] = shadows.shadow_matrix[cascade][5] = .125f;
            shadows.shadow_matrix[cascade][10] = .05f;
            shadows.shadow_matrix[cascade][12] = shadows.shadow_matrix[cascade][13] = .5f;
            shadows.shadow_matrix[cascade][14] = .7f;
        }
        const std::array<float,4> clips{2,8,20,40}; std::copy(clips.begin(),clips.end(),shadows.shadow_clip);
        shadows.resolution[0] = shadows.resolution[1] = 128;
        shadows.resolution[2] = float(width); shadows.resolution[3] = float(height);
        shadows.parameters[0] = .0001f; shadows.parameters[1] = .002f;
        shadows.parameters[2] = 48.f; shadows.parameters[3] = float(height);
        shadows.config[0] = 4; shadows.config[3] = quality;
        std::memcpy(shadow_buffer.contents,&shadows,sizeof(shadows));
        auto* environment = static_cast<ALMetalWorldAlphaEnvironment*>(environment_buffer.contents);
        // Keep direct sun and local lights live alongside SH/probe work. The
        // captured Home values are not reproduced: this is a fixed valid
        // complete-main-filter PSO sample with spatially heterogeneous shadows.
        for (unsigned channel = 0; channel != 3; ++channel)
        {
            environment->atmos_sunlit[channel] = .3f + .08f * channel;
            environment->atmos_sunlit_linear[channel] = .25f + .07f * channel;
        }
        auto* lighting = static_cast<ALMetalWorldLegacyAlphaLighting*>(lighting_buffer.contents);
        for (unsigned i = 0; i != lighting->local_lights.size(); ++i)
        {
            auto& light = lighting->local_lights[i];
            light.position[0] = .3f * (int(i) - 3); light.position[1] = .2f * (int(i % 3) - 1);
            light.position[2] = -1; light.direction[2] = -1;
            light.diffuse[0] = .01f * (i+1); light.diffuse[1] = .02f; light.diffuse[2] = .015f;
            light.parameters[0] = 8; light.parameters[1] = 0; light.parameters[2] = 1;
        }
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
        environment.water_plane[2] = 1; environment.water_plane[3] = 8;
        environment.water_fog[0] = 1; environment.water_fog[2] = 1;
        environment.atmospheric[3] = 1; environment.glow_max_y[2] = 1;
        for (unsigned i = 0; i != 3; ++i) environment.eye_to_sky[i][i] = 1;
        environment.atmos_sunlit_linear[3] = 1;
        std::copy_n(base.atmos_amblit, 3, environment.atmos_amblit);
        ALMetalWorldLegacyAlphaLighting lighting{}; lighting.parameters[3] = lobe.classic;
        lighting.sun_direction[2] = 1;
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
    enum class Path { helper, base, simple, simple_shadow };
    struct Timing { double gpu_ns, wall_ns; };
    Timing render(unsigned variant, Path path, bool copy = true, unsigned draws = 1)
    {
        const auto begin = std::chrono::steady_clock::now();
        auto command = [queue commandBuffer]; REQUIRE(command);
        command.label = path == Path::helper ? @"Isolated production probe helper" : path == Path::base ? @"Production base lighting fragment" : @"Production forward simple alpha fragment";
        const bool shadowed = path == Path::simple_shadow;
        const unsigned attachments = path == Path::helper ? 3 : 1;
        auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
        for (unsigned a = 0; a != attachments; ++a)
        {
            pass.colorAttachments[a].texture = shadowed ? shadow_output[variant] : output[variant][a];
            pass.colorAttachments[a].loadAction = MTLLoadActionClear;
            pass.colorAttachments[a].storeAction = MTLStoreActionStore;
            pass.colorAttachments[a].clearColor = shadowed ? MTLClearColorMake(.08,.13,.21,.4) : MTLClearColorMake(-1,-1,-1,-1);
        }
        if (shadowed)
        {
            pass.depthAttachment.texture = pass.stencilAttachment.texture = render_depth;
            pass.depthAttachment.loadAction = pass.stencilAttachment.loadAction = MTLLoadActionClear;
            pass.depthAttachment.storeAction = pass.stencilAttachment.storeAction = MTLStoreActionDontCare;
            pass.depthAttachment.clearDepth = 1; pass.stencilAttachment.clearStencil = 0;
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
            [encoder setRenderPipelineState:shadowed ? shadow_pso[variant] : simple_pso[variant]];
            if (shadowed) [encoder setDepthStencilState:readonly_depth];
            for (unsigned i = 0; i != 4; ++i) [encoder setVertexBuffer:vertex_buffers[i] offset:0 atIndex:i];
            [encoder setVertexBuffer:shiny_buffer offset:0 atIndex:4];
            [encoder setFragmentBuffer:environment_buffer offset:0 atIndex:0];
            [encoder setFragmentBuffer:probes_buffer offset:0 atIndex:1];
            [encoder setFragmentBuffer:lighting_buffer offset:0 atIndex:2];
            for (unsigned slot = 0; slot != 32; ++slot) [encoder setFragmentTexture:shadowed ? alpha_diffuse : base_textures[0] atIndex:slot];
            [encoder setFragmentTexture:radiance atIndex:32]; [encoder setFragmentTexture:irradiance atIndex:37];
            [encoder setFragmentTexture:sky atIndex:39];
            [encoder setFragmentSamplerState:sampler atIndex:0]; [encoder setFragmentSamplerState:sampler atIndex:1];
            if (shadowed)
            {
                [encoder setFragmentTexture:shadow_maps atIndex:33];
                [encoder setFragmentBuffer:shadow_buffer offset:0 atIndex:3];
            }
        }
        for (unsigned draw = 0; draw != draws; ++draw) [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];
        if (copy)
        {
            auto blit = [command blitCommandEncoder]; REQUIRE(blit);
            for (unsigned a = 0; a != attachments; ++a)
                [blit copyFromTexture:shadowed ? shadow_output[variant] : output[variant][a]
                    sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0)
                    sourceSize:MTLSizeMake(width,height,1) toBuffer:shadowed ? shadow_readback[variant] : readback[variant][a] destinationOffset:0
                    destinationBytesPerRow:width * (shadowed ? 8 : 16) destinationBytesPerImage:width * height * (shadowed ? 8 : 16)];
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
            const bool shadowed = path == Path::simple_shadow;
            const auto* original = static_cast<const float*>(readback[0][a].contents);
            const auto* candidate = static_cast<const float*>(readback[1][a].contents);
            const auto* original_half = static_cast<const _Float16*>(shadow_readback[0].contents);
            const auto* candidate_half = static_cast<const _Float16*>(shadow_readback[1].contents);
            for (unsigned i = 0; i != width * height * 4; ++i)
            {
                const float av = shadowed ? float(original_half[i]) : original[i];
                const float bv = shadowed ? float(candidate_half[i]) : candidate[i];
                if (!std::isfinite(av) || !std::isfinite(bv)) { ++nonfinite; continue; }
                const float delta = std::abs(av - bv);
                max_error = std::max(max_error, delta); max_value = std::max(max_value, std::abs(av));
                if (i % 4 != 3) max_rgb = std::max(max_rgb, std::abs(av));
                failures += delta > (shadowed ? 5e-4f + 1e-3f * std::abs(av) : 2e-5f + 3e-5f * std::abs(av));
            }
        }
        std::printf("probe_accumulator_parity layout=%s count=%.0f kind=%u lobe=%s path=%s mixed_gbuffer=%u max_absolute_error=%.9g max_rgb=%.9g nonfinite=%u failures=%u\n",
            layout_name,probes.config[0],layout_kind,lobe_name,path==Path::helper?"helper-mrt":path==Path::base?"production-base":path==Path::simple_shadow?"production-simple-shadow-alpha":"production-simple-alpha",
            unsigned(mixed_gbuffer),max_error,max_rgb,nonfinite,failures);
        CAPTURE(max_error, max_value, failures, nonfinite);
        CHECK(nonfinite == 0); CHECK(failures == 0); CHECK(max_rgb > 1e-4f);
        // A cleared or discarded whole output cannot satisfy this guard.
        const float first_alpha = path == Path::simple_shadow
            ? float(static_cast<const _Float16*>(shadow_readback[0].contents)[3])
            : static_cast<const float*>(readback[0][0].contents)[3];
        CHECK(first_alpha >= 0.f);
    }
    void requireShadowChangesOutput()
    {
        render(0,Path::simple_shadow);
        const auto* first = static_cast<const _Float16*>(shadow_readback[0].contents);
        const std::vector<_Float16> filtered(first,first+width*height*4);
        const auto mask = shadows.config[1]; shadows.config[1] = 15;
        std::memcpy(shadow_buffer.contents,&shadows,sizeof(shadows));
        render(0,Path::simple_shadow);
        const auto* unfiltered = static_cast<const _Float16*>(shadow_readback[0].contents);
        float delta = 0;
        for (unsigned i=0;i!=width*height*4;++i) delta=std::max(delta,std::abs(float(filtered[i])-float(unfiltered[i])));
        CAPTURE(delta); CHECK(delta > .005f);
        shadows.config[1] = mask; std::memcpy(shadow_buffer.contents,&shadows,sizeof(shadows));
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
        // Signed SH coefficients and opposing directions exercise cancellation
        // and the final nonnegative clamp, not just a positive L0 field.
        f.uploadSH(true);
        constexpr std::array<std::array<float,3>,6> normals{{
            {{1,0,0}},{{-1,0,0}},{{0,1,0}},{{0,-1,0}},{{0,0,1}},{{0,0,-1}}}};
        for (const unsigned kind : {0u,3u,9u,10u,11u}) for (const auto& normal : normals)
        {
            CAPTURE(kind,normal[0],normal[1],normal[2]);
            f.configure(Layout{"signed-SH-basis",20,kind},lobes[3]);
            std::copy(normal.begin(),normal.end(),f.inputs.normal);
            std::memcpy(f.inputs_buffer.contents,&f.inputs,sizeof(f.inputs));
            f.compare(Fixture::Path::helper);
        }
    }
}

TEST_CASE("almetal: complete main shadow alpha preserves probe and filtered-light pixels",
          "[almetal][.probe-accumulator-home-parity]")
{
    @autoreleasepool
    {
        Fixture f;
        for (const unsigned count : {1u,20u,39u})
        for (const unsigned family : {0u,2u,3u})
        for (const unsigned quality : {2u,3u})
        for (const bool rotated : {false,true})
        {
            CAPTURE(count,family,quality,rotated);
            f.configureHomeAlpha(count,family,quality,rotated);
            f.compare(Fixture::Path::simple_shadow);
        }
        for (const unsigned quality : {2u,3u})
        {
            f.configureHomeAlpha(20,0,quality);
            f.requireShadowChangesOutput();
        }
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

TEST_CASE("almetal: bounded complete main shadow alpha probe costs",
          "[almetal][.probe-accumulator-home-cost]")
{
    @autoreleasepool
    {
        Fixture f(1024,576);
        constexpr unsigned pairs = 5;
        // The late saved Home capture configured count20; counts1/39 remain
        // controls. These overlapping fixture volumes do not reproduce its
        // unknown per-pixel accepted/candidate distribution or texture set.
        for (const unsigned count : {1u,20u,39u})
        for (const unsigned family : {0u,2u})
        {
            constexpr unsigned quality = 3;
            f.configureHomeAlpha(count,family,quality);
            for (unsigned warmup=0;warmup!=2;++warmup)
                for (unsigned variant=0;variant!=2;++variant) f.render(variant,Fixture::Path::simple_shadow,false);
            for (unsigned pair=0;pair!=pairs;++pair) for (unsigned ordinal=0;ordinal!=2;++ordinal)
            {
                const unsigned variant = (pair+ordinal)%2;
                const auto timing = f.render(variant,Fixture::Path::simple_shadow,false);
                REQUIRE(timing.gpu_ns > 0);
                std::printf("probe_accumulator_home_cost count=%u family=%s shadow_quality=%u function_constant=undefined path=production-simple-shadow-alpha width=%u height=%u format=RGBA16Float blend=src_alpha/one_minus_src_alpha depth=readonly_less_equal draws=1 pair=%u ordinal=%u variant=%s gpu_ns=%.0f owner_wall_ns=%.0f\n",
                    count,family==0?"automatic":"mixed",quality,f.width,f.height,pair,ordinal,variant?"candidate":"baseline",timing.gpu_ns,timing.wall_ns);
            }
        }
    }
}
