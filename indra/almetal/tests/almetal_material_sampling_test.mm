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
    const ALMetalTexture* legacy_environment)
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
    scene.width = scene.height = side; scene.batches = batches.data(); scene.batch_count = batches.size();
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
