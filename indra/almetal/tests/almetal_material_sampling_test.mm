/**
 * @file almetal_material_sampling_test.mm
 * @brief Per-view material sampling through the production world encoders.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "almetal_world_inputs_fixture.h"
#include <bit>
#include <cmath>

namespace
{
using RGBA = std::array<std::uint8_t, 4>;

const ALMetalTexture* materialMipChain(Fixture& f, RGBA fine, RGBA coarse)
{
    ALMetalTextureDescriptor descriptor;
    descriptor.width = descriptor.height = 128;
    descriptor.mipmap_level_count = 8;
    descriptor.format = ALMetalPixelFormat::rgba8_unorm;
    descriptor.usage = ALMetalTextureUsage::shader_read;
    descriptor.label = "Material sampling authored colored mips";
    std::shared_ptr<ALMetalTexture> texture = ALMetalTexture::create(*f.device, descriptor, &f.error);
    INFO(f.error); REQUIRE(texture);
    auto command = f.device->makeCommandBuffer("Material sampling mip upload", &f.error);
    REQUIRE(command);
    for (unsigned mip = 0; mip < descriptor.mipmap_level_count; ++mip)
    {
        const auto width = texture->width(mip);
        const std::vector<RGBA> pixels(width * width, mip & 1u ? fine : coarse);
        REQUIRE(texture->upload(*command, mip, 0, {}, pixels.data(), width * sizeof(RGBA), &f.error));
    }
    REQUIRE(command->commitAndWait(&f.error));
    f.resources->textures.push_back(texture);
    return texture.get();
}

const ALMetalTexture* materialDistinctMips(Fixture& f, const std::array<RGBA, 8>& levels)
{
    ALMetalTextureDescriptor descriptor;
    descriptor.width = descriptor.height = 128; descriptor.mipmap_level_count = levels.size();
    descriptor.format = ALMetalPixelFormat::rgba8_unorm; descriptor.usage = ALMetalTextureUsage::shader_read;
    descriptor.label = "Temporal material independently authored LOD colors";
    std::shared_ptr<ALMetalTexture> texture = ALMetalTexture::create(*f.device, descriptor, &f.error);
    INFO(f.error); REQUIRE(texture);
    auto command = f.device->makeCommandBuffer("Temporal material distinct mip upload", &f.error); REQUIRE(command);
    for (unsigned mip = 0; mip < levels.size(); ++mip)
    {
        const auto width = texture->width(mip);
        const std::vector<RGBA> pixels(width * width, levels[mip]);
        REQUIRE(texture->upload(*command,mip,0,{},pixels.data(),width*sizeof(RGBA),&f.error));
    }
    REQUIRE(command->commitAndWait(&f.error)); f.resources->textures.push_back(texture); return texture.get();
}

void materialSamplingTriangle(Fixture& f)
{
    // Full-screen affine UVs: 128 texels / 32 pixels = rho4, exactly LOD2.
    // A bias of -1 must select the authored LOD1, independently of coverage.
    const float positions[3][4]{{-1, -1, .25f, 1}, {3, -1, .25f, 1}, {-1, 3, .25f, 1}};
    const float uv[3][2]{{0, 0}, {2, 0}, {0, 2}};
    std::memcpy(f.geometry.positions, positions, sizeof(positions));
    std::memcpy(f.geometry.uv, uv, sizeof(uv));
    std::memset(f.geometry.colors, 255, sizeof(f.geometry.colors));
    f.vertices = f.buffer(&f.geometry, sizeof(f.geometry));
}

enum class MaterialMap { diffuse, normal, auxiliary, emissive };

ALMetalWorldSimpleBatch samplingBatch(Fixture& f, unsigned family, MaterialMap map, const ALMetalTexture* texture)
{
    auto batch = f.batch(family);
    batch.texcoord1_offset = batch.texcoord2_offset = batch.texcoord_offset;
    if (map == MaterialMap::diffuse) batch.diffuse_textures[0] = texture;
    if (family == 1)
    {
        if (map == MaterialMap::normal) batch.materialSlots()[0].normal_texture = texture;
        if (map == MaterialMap::auxiliary) batch.materialSlots()[0].specular_texture = texture;
    }
    if (family == 2)
    {
        auto& material = batch.pbrSlots()[0];
        if (map == MaterialMap::normal) material.normal_texture = texture;
        if (map == MaterialMap::auxiliary) material.orm_texture = texture;
        if (map == MaterialMap::emissive)
        {
            material.emissive_texture = texture;
            std::fill_n(material.emissive_color, 3, .8f);
        }
    }
    return batch;
}

Pixels sampleGBuffer(Fixture& f, const ALMetalWorldSimpleBatch& batch, float bias, bool owned = false)
{
    auto scene = f.scene();
    scene.batches = &batch; scene.batch_count = 1; scene.material_mip_bias = bias;
    std::unique_ptr<ALMetalRenderTarget> target;
    if (owned)
    {
        auto inputs = f.adopt(scene, {batch});
        target = f.renderer->renderOwnedInputs(*inputs, &f.error);
    }
    else target = f.renderer->render(scene, &f.error);
    INFO(f.error); REQUIRE(target);
    return f.read(*target);
}

void equalGBuffer(const Pixels& actual, const Pixels& expected)
{
    for (unsigned slot = 0; slot < actual.colors.size(); ++slot)
    {
        CAPTURE(slot);
        CHECK(std::equal(actual.colors[slot].begin(), actual.colors[slot].end(), expected.colors[slot].begin()));
    }
    CHECK(std::equal(actual.depth.begin(), actual.depth.end(), expected.depth.begin()));
    CHECK(std::equal(actual.stencil.begin(), actual.stencil.end(), expected.stencil.begin()));
}

void closeSamplingGBuffer(const Pixels& actual, const Pixels& expected)
{
    // The independent explicit-LOD reference is stored as normalized 16-bit
    // constant texels. Allow only that conversion and G-buffer quantization:
    // exact depth/stencil, <=1/255 color and <=8/65535 normal storage.
    for (unsigned slot = 0; slot < actual.colors.size(); ++slot)
    {
        REQUIRE(actual.colors[slot].size() == expected.colors[slot].size());
        if (actual.colors[slot].size() == side * side * 8)
        {
            for (std::size_t i = 0; i < actual.colors[slot].size(); i += 2)
            {
                std::uint16_t a, b;
                std::memcpy(&a,actual.colors[slot].data()+i,2); std::memcpy(&b,expected.colors[slot].data()+i,2);
                REQUIRE(std::abs(int(a)-int(b)) <= 8);
            }
        }
        else for (std::size_t i = 0; i < actual.colors[slot].size(); ++i)
            REQUIRE(std::abs(int(actual.colors[slot][i])-int(expected.colors[slot][i])) <= 1);
    }
    REQUIRE(actual.depth == expected.depth); REQUIRE(actual.stencil == expected.stencil);
}

struct SamplingPolicyCase { const char* name; float bias; };
const std::array<SamplingPolicyCase, 5> methodSamplingPolicies{{
    {"Native",0.f}, {"Spatial50",-1.f}, {"Temporal50",-2.f},
    {"Temporal67",std::log2(2304.f/3456.f)-1.f},
    {"Temporal75",std::log2(2592.f/3456.f)-1.f}}};

void scaleSamplingUV(Fixture& f, const Fixture::Geometry& original, float scale)
{
    f.geometry = original;
    for (auto& uv : f.geometry.uv) for (float& axis : uv) axis *= scale;
    f.vertices = f.buffer(&f.geometry,sizeof(f.geometry));
}

// Independent native explicit-LOD reference for the rho=4 triangle. A
// fractional bias and a derivative rescale are not byte-equivalent on M1 Max:
// native calculate_lod and texture-filter LOD precision differ. Explicit level
// (2+bias) independently reproduces the sampler's actual requested LOD without
// cloning any production material shader, hardcoding a GPU quantization grid,
// or widening image tolerances. Private PPD598 oracle retains that distinction.
struct ExplicitMaterialLodReference
{
    id<MTLDevice> device;
    id<MTLRenderPipelineState> pipeline;
    explicit ExplicitMaterialLodReference(Fixture& f)
    {
        device=(__bridge id<MTLDevice>)f.device->nativeHandle();
        NSString* source=@R"METAL(
            #include <metal_stdlib>
            using namespace metal;
            struct V {float4 p[[position]];float2 uv;};
            vertex V material_reference_vertex(uint i[[vertex_id]]) {
                float2 uv=float2((i<<1)&2,i&2); return {float4(uv.x*2-1,1-uv.y*2,0,1),uv};
            }
            fragment float4 material_reference_fragment(V v[[stage_in]],texture2d<float> t[[texture(0)]],
                constant float& lod[[buffer(0)]]) {
                constexpr sampler s(coord::normalized,address::repeat,filter::linear,mip_filter::linear,max_anisotropy(1));
                return t.sample(s,v.uv,level(lod));
            }
        )METAL";
        NSError* error=nil;
        auto options=[MTLCompileOptions new]; options.languageVersion=MTLLanguageVersion3_1;
        auto library=[device newLibraryWithSource:source options:options error:&error];
        INFO((error ? error.description.UTF8String : "")); REQUIRE(library);
        auto descriptor=[MTLRenderPipelineDescriptor new];
        descriptor.vertexFunction=[library newFunctionWithName:@"material_reference_vertex"];
        descriptor.fragmentFunction=[library newFunctionWithName:@"material_reference_fragment"];
        descriptor.colorAttachments[0].pixelFormat=MTLPixelFormatRGBA32Float;
        pipeline=[device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        INFO((error ? error.description.UTF8String : "")); REQUIRE(pipeline);
    }
    const ALMetalTexture* constant(Fixture& f,const ALMetalTexture& texture,float lod)
    {
        auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
            width:side height:side mipmapped:NO];
        descriptor.storageMode=MTLStorageModeShared; descriptor.usage=MTLTextureUsageRenderTarget;
        auto output=[device newTextureWithDescriptor:descriptor]; REQUIRE(output);
        auto pass=[MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture=output; pass.colorAttachments[0].loadAction=MTLLoadActionDontCare;
        pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        auto command=[(__bridge id<MTLCommandQueue>)f.device->nativeCommandQueue() commandBuffer]; REQUIRE(command);
        auto encoder=[command renderCommandEncoderWithDescriptor:pass]; REQUIRE(encoder);
        [encoder setRenderPipelineState:pipeline]; [encoder setViewport:(MTLViewport){0,0,side,side,0,1}];
        [encoder setFragmentTexture:(__bridge id<MTLTexture>)texture.nativeHandle() atIndex:0];
        [encoder setFragmentBytes:&lod length:sizeof(lod) atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [encoder endEncoding];
        [command commit]; [command waitUntilCompleted]; REQUIRE(command.status==MTLCommandBufferStatusCompleted);
        std::array<float,4> pixel{};
        [output getBytes:pixel.data() bytesPerRow:sizeof(pixel) fromRegion:MTLRegionMake2D(side/2,side/2,1,1) mipmapLevel:0];
        std::array<std::uint16_t,4> normalized;
        for (unsigned c=0;c<4;++c)
        { REQUIRE(std::isfinite(pixel[c])); normalized[c]=std::lround(std::clamp(pixel[c],0.f,1.f)*65535.f); }
        return f.texture(ALMetalPixelFormat::rgba16_unorm,1,normalized.data(),sizeof(normalized));
    }
};

std::unique_ptr<ALMetalRenderTarget> samplingSeed(Fixture& f, bool gbuffer)
{
    ALMetalRenderTargetDescriptor descriptor;
    descriptor.width = descriptor.height = side;
    descriptor.color_attachment_count = gbuffer ? 4 : 1;
    descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float_stencil8;
    for (unsigned slot = 0; slot < descriptor.color_attachment_count; ++slot)
        descriptor.colors[slot].format = ALMetalPixelFormat::rgba16_float;
    auto target = ALMetalRenderTarget::create(*f.device, descriptor, &f.error);
    INFO(f.error); REQUIRE(target);
    ALMetalRenderPassConfig config;
    config.clear_depth = 1.; config.clear_stencil = 0x5a;
    config.depth_store = config.stencil_store = ALMetalStoreAction::store;
    for (unsigned slot = 0; slot < descriptor.color_attachment_count; ++slot)
        config.colors[slot].clear = {.03125, .0625, .125, .5};
    auto command = f.device->makeCommandBuffer("Material sampling independent alpha seed", &f.error);
    REQUIRE(command);
    auto pass = target->makeRenderPass(config, &f.error); REQUIRE(pass);
    auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Material alpha seed", &f.error);
    REQUIRE(encoder); REQUIRE(encoder->end(&f.error)); REQUIRE(command->commitAndWait(&f.error));
    return target;
}

struct AlphaPixels
{
    std::vector<std::array<std::uint16_t, 4>> color;
    std::vector<std::uint32_t> depth;
    bool operator==(const AlphaPixels&) const = default;
};

const ALMetalTexture* materialSamplingEnvironment(Fixture& f)
{
    // Material/PBR forward owners require exactly one real environment source.
    // A black legacy cube isolates material sampling without a probe traversal.
    ALMetalTextureDescriptor descriptor;
    descriptor.type = ALMetalTextureType::texture_cube;
    descriptor.format = ALMetalPixelFormat::rgba8_unorm;
    descriptor.usage = ALMetalTextureUsage::shader_read;
    std::shared_ptr<ALMetalTexture> texture = ALMetalTexture::create(*f.device, descriptor, &f.error);
    INFO(f.error); REQUIRE(texture);
    auto command = f.device->makeCommandBuffer("Material sampling authored legacy environment", &f.error);
    REQUIRE(command);
    const RGBA black{0, 0, 0, 255};
    for (unsigned face = 0; face < 6; ++face)
        REQUIRE(texture->upload(*command, 0, face, {}, black.data(), sizeof(black), &f.error));
    REQUIRE(command->commitAndWait(&f.error));
    f.resources->textures.push_back(texture);
    return texture.get();
}

AlphaPixels sampleAlpha(Fixture& f, unsigned family, const ALMetalTexture* texture, float bias,
    const ALMetalTexture* legacy_environment, bool include_depth=true)
{
    auto gbuffer = samplingSeed(f, true), composed = samplingSeed(f, false);
    auto color = samplingBatch(f, family, MaterialMap::diffuse, texture);
    color.post_water_alpha = true;
    color.pre_water_fullbright = family == 0;
    color.pre_water_material = family == 1;
    color.pre_water_pbr = family == 2;
    color.pre_water_minimum_alpha = .004f;
    if (family == 1)
    {
        color.material_alpha_mode = 1;
        color.materialSlots()[0].alpha_cutoff = -1.f;
        color.materialSlots()[0].fullbright = 1.f;
    }
    if (family == 2) std::fill_n(color.pbrSlots()[0].emissive_color, 3, .8f);
    auto depth = color;
    depth.post_water_depth_only = true;
    if (family == 0) depth.pre_water_minimum_alpha = .33f;
    const std::array batches{color, depth};
    ALMetalWorldAlphaEnvironment environment;
    environment.water_plane[2] = 1.f; environment.water_plane[3] = -100.f;
    environment.water_fog[0] = 1.f; environment.water_fog[2] = 1.f;
    environment.glow_max_y[0] = 1.f; environment.glow_max_y[2] = -1.f; environment.glow_max_y[3] = 1.f;
    environment.light_direction[2] = 1.f;
    environment.atmospheric[1] = environment.atmospheric[2] = environment.atmospheric[3] = 1.f;
    environment.atmos_sunlit[3] = environment.atmos_sunlit_linear[3] = 1.f;
    environment.environment[0] = 1.f;
    for (unsigned axis = 0; axis < 3; ++axis)
    {
        environment.eye_to_sky[axis][axis] = 1.f;
        environment.atmos_sunlit[axis] = .25f;
        environment.atmos_sunlit_linear[axis] = .2f + .05f * axis;
        environment.atmos_amblit[axis] = .1f;
    }
    ALMetalWorldLegacyAlphaLighting lighting;
    lighting.sun_direction[2] = 1.f;
    lighting.parameters[0] = 128.f; lighting.parameters[1] = .004f; lighting.parameters[2] = 1.f;
    ALMetalWorldOpaquePostDeferredScene scene;
    scene.gbuffer = gbuffer.get(); scene.composed_lighting = composed.get();
    scene.width = scene.height = side;
    scene.batches = batches.data(); scene.batch_count = include_depth ? batches.size() : 1;
    scene.alpha_environment = &environment; scene.alpha_lighting = &lighting;
    scene.legacy_environment = legacy_environment;
    scene.material_mip_bias = bias;
    auto target = f.renderer->renderOpaquePostDeferred(scene, &f.error);
    INFO(f.error); REQUIRE(target);
    AlphaPixels result;
    result.color.resize(side * side); result.depth.resize(side * side);
    REQUIRE(target->colorTexture(0)->readback(*f.device, 0, 0, {}, result.color.data(), side * 8, &f.error));
    REQUIRE(target->depthStencilTexture()->readback(*f.device, 0, 0, {}, result.depth.data(), side * 4, &f.error));
    return result;
}
}

TEST_CASE("almetal: per-view material bias selects real legacy and PBR mip samples", "[almetal][material-sampling]")
{
    @autoreleasepool
    {
        Fixture f;
        materialSamplingTriangle(f);
        for (unsigned family : {0u, 1u, 2u})
        for (MaterialMap map : {MaterialMap::diffuse, MaterialMap::normal, MaterialMap::auxiliary, MaterialMap::emissive})
        {
            if ((family == 0 && map != MaterialMap::diffuse) || (family == 1 && map == MaterialMap::emissive)) continue;
            CAPTURE(family, static_cast<unsigned>(map));
            const RGBA fine = map == MaterialMap::normal ? RGBA{192, 128, 240, 255} :
                map == MaterialMap::auxiliary ? RGBA{255, 64, 32, 255} : RGBA{0, 255, 0, 255};
            const RGBA coarse = map == MaterialMap::normal ? RGBA{128, 192, 240, 255} :
                map == MaterialMap::auxiliary ? RGBA{128, 224, 192, 255} : RGBA{0, 0, 255, 255};
            const auto* chain = materialMipChain(f, fine, coarse);
            const auto* selected_fine = materialMipChain(f, fine, fine);
            const auto* selected_coarse = materialMipChain(f, coarse, coarse);
            const auto batch = samplingBatch(f, family, map, chain);
            const auto zero = sampleGBuffer(f, batch, 0.f);
            const auto negative = sampleGBuffer(f, batch, -1.f);
            CHECK_FALSE(zero == negative);
            equalGBuffer(zero, sampleGBuffer(f, samplingBatch(f, family, map, selected_coarse), 0.f));
            equalGBuffer(negative, sampleGBuffer(f, samplingBatch(f, family, map, selected_fine), 0.f));
            equalGBuffer(sampleGBuffer(f, batch, -1.f, true), negative);
            equalGBuffer(sampleGBuffer(f, batch, 0.f), zero); // Bias cannot leak into the next view.
        }
    }
}

TEST_CASE("almetal: alpha color and depth consume the same per-view material mip bias", "[almetal][material-sampling]")
{
    @autoreleasepool
    {
        Fixture f;
        materialSamplingTriangle(f);
        const RGBA opaque{0, 255, 0, 255}, transparent{0, 0, 255, 0};
        const auto* chain = materialMipChain(f, opaque, transparent);
        const auto* selected_opaque = materialMipChain(f, opaque, opaque);
        const auto* selected_transparent = materialMipChain(f, transparent, transparent);
        const auto* environment = materialSamplingEnvironment(f);
        for (unsigned family : {0u, 1u, 2u})
        {
            CAPTURE(family);
            const auto zero = sampleAlpha(f, family, chain, 0.f, environment);
            const auto negative = sampleAlpha(f, family, chain, -1.f, environment);
            const auto far = std::bit_cast<std::uint32_t>(1.f);
            CHECK(std::all_of(zero.depth.begin(), zero.depth.end(), [far](auto value) { return value == far; }));
            CHECK(std::any_of(negative.depth.begin(), negative.depth.end(), [far](auto value) { return value != far; }));
            CHECK(zero.color != negative.color);
            CHECK(bool(zero == sampleAlpha(f, family, selected_transparent, 0.f, environment)));
            CHECK(bool(negative == sampleAlpha(f, family, selected_opaque, 0.f, environment)));
            CHECK(bool(zero == sampleAlpha(f, family, chain, 0.f, environment)));
        }
    }
}

TEST_CASE("almetal: terrain PBR explicit gradients apply the same mip bias as a UV derivative scale", "[almetal][material-sampling]")
{
    @autoreleasepool
    {
        Fixture f;
        const auto* chain = materialMipChain(f, RGBA{0, 255, 0, 255}, RGBA{0, 0, 255, 255});
        auto batch = f.batch(8);
        batch.terrain_triplanar = false; batch.terrain_hex = false;
        std::fill_n(batch.diffuse_textures.begin(), 4, chain);
        const auto zero = sampleGBuffer(f, batch, 0.f);
        const auto negative = sampleGBuffer(f, batch, -1.f);
        for (unsigned slot = 0; slot < batch.pbr_slot_count; ++slot)
            for (unsigned axis = 0; axis < 4; ++axis) batch.terrain_uv_transform[slot][axis] *= .5f;
        const auto scaled_derivatives = sampleGBuffer(f, batch, 0.f);
        CHECK_FALSE(zero == negative);
        equalGBuffer(negative, scaled_derivatives);
    }
}

TEST_CASE("almetal: Temporal material bias selects LOD0 and fractional finer mips without stacking Spatial bias", "[almetal][material-sampling][temporal-material]")
{
    @autoreleasepool
    {
        Fixture f; materialSamplingTriangle(f); ExplicitMaterialLodReference native_lod(f);
        const auto original = f.geometry;
        for (unsigned family : {0u,1u,2u})
        for (MaterialMap map : {MaterialMap::diffuse,MaterialMap::normal,MaterialMap::auxiliary,MaterialMap::emissive})
        {
            if ((family == 0 && map != MaterialMap::diffuse) || (family == 1 && map == MaterialMap::emissive)) continue;
            CAPTURE(family,static_cast<unsigned>(map));
            std::array<RGBA,8> levels;
            levels.fill(RGBA{64,64,64,255});
            if (map == MaterialMap::normal)
            { levels[0]={128,128,255,255}; levels[1]={192,128,240,255}; levels[2]={128,192,240,255}; }
            else if (map == MaterialMap::auxiliary)
            { levels[0]={200,32,220,255}; levels[1]={255,64,32,255}; levels[2]={128,224,192,255}; }
            else { levels[0]={255,0,0,255}; levels[1]={0,255,0,255}; levels[2]={0,0,255,255}; }
            const auto* chain = materialDistinctMips(f,levels);
            scaleSamplingUV(f,original,1.f);
            const auto batch = samplingBatch(f,family,map,chain);
            const auto native = sampleGBuffer(f,batch,0.f);
            std::array<Pixels,5> observed;
            for (unsigned index=0;index<methodSamplingPolicies.size();++index)
            {
                const auto policy=methodSamplingPolicies[index]; CAPTURE(policy.name,policy.bias);
                observed[index]=sampleGBuffer(f,batch,policy.bias);
                const bool integer=policy.bias==std::floor(policy.bias);
                scaleSamplingUV(f,original,integer ? std::exp2(policy.bias) : 1.f);
                const auto* reference_texture=integer ? chain : native_lod.constant(f,*chain,2.f+policy.bias);
                const auto reference=samplingBatch(f,family,map,reference_texture);
                closeSamplingGBuffer(observed[index],sampleGBuffer(f,reference,0.f));
            }
            REQUIRE_FALSE(observed[0] == observed[1]); REQUIRE_FALSE(observed[1] == observed[2]);
            REQUIRE_FALSE(observed[2] == observed[3]); REQUIRE_FALSE(observed[3] == observed[4]);
            equalGBuffer(sampleGBuffer(f,batch,-2.f,true),observed[2]);
            equalGBuffer(sampleGBuffer(f,batch,0.f),native);
        }
    }
}

TEST_CASE("almetal: Temporal alpha color and depth keep matching finer and fractional mip choices", "[almetal][material-sampling][temporal-material]")
{
    @autoreleasepool
    {
        Fixture f; materialSamplingTriangle(f); const auto original=f.geometry; ExplicitMaterialLodReference native_lod(f);
        std::array<RGBA,8> levels; levels.fill(RGBA{0,0,255,0}); levels[0]={255,0,0,255};
        const auto* chain=materialDistinctMips(f,levels);
        const auto* environment=materialSamplingEnvironment(f);
        for (unsigned family : {0u,1u,2u})
        {
            scaleSamplingUV(f,original,1.f);
            const auto native=sampleAlpha(f,family,chain,0.f,environment);
            for (const auto policy : methodSamplingPolicies)
            {
                CAPTURE(family,policy.name,policy.bias);
                scaleSamplingUV(f,original,1.f);
                const auto actual=sampleAlpha(f,family,chain,policy.bias,environment);
                // Compare the two production passes directly at the SAME
                // actual bias, independently of any fractional-LOD oracle.
                const auto color_only=sampleAlpha(f,family,chain,policy.bias,environment,false);
                REQUIRE(actual.color==color_only.color);
                const std::array<__fp16,4> seed{.03125f,.0625f,.125f,.5f};
                std::array<std::uint16_t,4> seed_bits; std::memcpy(seed_bits.data(),seed.data(),sizeof(seed_bits));
                const auto far=std::bit_cast<std::uint32_t>(1.f);
                for (std::size_t p=0;p<actual.depth.size();++p)
                    REQUIRE((color_only.color[p]!=seed_bits)==(actual.depth[p]!=far));
                const bool integer=policy.bias==std::floor(policy.bias);
                scaleSamplingUV(f,original,integer ? std::exp2(policy.bias) : 1.f);
                const auto* reference_texture=integer ? chain : native_lod.constant(f,*chain,2.f+policy.bias);
                const auto expected=sampleAlpha(f,family,reference_texture,0.f,environment);
                REQUIRE(actual.depth == expected.depth); REQUIRE(actual.color.size() == expected.color.size());
                for (std::size_t p=0;p<actual.color.size();++p) for (unsigned c=0;c<4;++c)
                {
                    __fp16 a,b; std::memcpy(&a,&actual.color[p][c],2); std::memcpy(&b,&expected.color[p][c],2);
                    REQUIRE(std::abs(float(a)-float(b)) < .002f);
                }
                if (policy.bias == -2.f)
                {
                    const auto far=std::bit_cast<std::uint32_t>(1.f);
                    REQUIRE(std::all_of(actual.depth.begin(),actual.depth.end(),[far](auto d){return d != far;}));
                    REQUIRE(actual.color != native.color);
                }
            }
            scaleSamplingUV(f,original,1.f);
            REQUIRE(sampleAlpha(f,family,chain,0.f,environment) == native);
        }
    }
}

TEST_CASE("almetal: Temporal terrain gradients select the same detail as method-specific derivative scaling", "[almetal][material-sampling][temporal-material]")
{
    @autoreleasepool
    {
        Fixture f;
        std::array<RGBA,8> levels; levels.fill(RGBA{64,64,64,255});
        levels[0]={255,0,0,255}; levels[1]={0,255,0,255}; levels[2]={0,0,255,255};
        const auto* chain=materialDistinctMips(f,levels);
        auto original=f.batch(8); original.terrain_triplanar=false; original.terrain_hex=false;
        std::fill_n(original.diffuse_textures.begin(),4,chain);
        const auto native=sampleGBuffer(f,original,0.f);
        for (const auto policy:methodSamplingPolicies)
        {
            CAPTURE(policy.name,policy.bias);
            const auto actual=sampleGBuffer(f,original,policy.bias);
            auto reference=original;
            for (unsigned slot=0;slot<reference.pbr_slot_count;++slot)
                for (unsigned axis=0;axis<4;++axis) reference.terrain_uv_transform[slot][axis] *= std::exp2(policy.bias);
            closeSamplingGBuffer(actual,sampleGBuffer(f,reference,0.f));
        }
        equalGBuffer(sampleGBuffer(f,original,0.f),native);
    }
}
