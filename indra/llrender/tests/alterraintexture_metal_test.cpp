/**
 * @file alterraintexture_metal_test.cpp
 * @brief Pending terrain images preserve the original loading placeholder.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../almetalfetchedtexture.h"
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalresource.h"
#include "../../almetal/almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <vector>

namespace
{
struct FetchedInput final : LLTexture
{
    LLPointer<LLImageGL> image;
    LLPointer<LLImageGL> publish_on_update;
    unsigned updates = 0;
    bool deleted = true;
    explicit FetchedInput(LLImageGL* value) : image(value) {}
    ~FetchedInput() override = default;
    LLImageGL* getGLTexture() const override { return image; }
    void setActive() override { deleted = false; }
    void forceImmediateUpdate() override
    {
        ++updates;
        if (deleted && publish_on_update.notNull()) image = publish_on_update;
    }
};

struct TerrainFixture
{
    std::unique_ptr<ALMetalDevice> device;
    ALMetalDevice* previous_device = LLImageGL::getMetalDevice();
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::unique_ptr<ALMetalBuffer> vertices;
    std::unique_ptr<ALMetalTexture> height, composition;
    LLPointer<LLImageGL> ramp;
    std::string error;
    TerrainFixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        LLImageGL::setMetalDevice(device.get());
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error); REQUIRE(renderer);
        const std::array<std::array<float, 4>, 4> points{{
            {0,0,0,1}, {2,0,0,1}, {2,2,0,1}, {0,2,0,1}}};
        ALMetalBufferDescriptor vd; vd.length = sizeof(points);
        vd.storage = ALMetalStorageMode::shared; vd.label = "Terrain loading control points";
        vertices = ALMetalBuffer::create(*device, vd, &error); REQUIRE(vertices);
        REQUIRE(vertices->write(0, points.data(), sizeof(points), &error));
        ALMetalTextureDescriptor td; td.width = td.height = 8;
        td.format = ALMetalPixelFormat::r32_float;
        td.usage = ALMetalTextureUsage::shader_read; td.label = "Exact generated terrain height";
        height = ALMetalTexture::create(*device, td, &error); REQUIRE(height);
        td.format = ALMetalPixelFormat::rg16_float;
        td.label = "Exact generated terrain composition";
        composition = ALMetalTexture::create(*device, td, &error); REQUIRE(composition);
        std::array<float, 64> z{};
        std::array<std::uint16_t, 128> comp{};
        auto command = device->makeCommandBuffer("Terrain loading generated maps", &error);
        REQUIRE(command);
        REQUIRE(height->upload(*command, 0, 0, {}, z.data(), 8 * sizeof(float), &error));
        REQUIRE(composition->upload(*command, 0, 0, {}, comp.data(), 8 * 4, &error));
        REQUIRE(command->commitAndWait(&error));
        ramp = image({0,0,0,255}, false);
    }
    ~TerrainFixture() { LLImageGL::setMetalDevice(previous_device); }
    LLPointer<LLImageGL> image(std::array<U8,4> pixel, bool srgb)
    {
        LLPointer<LLImageGL> result = new LLImageGL(1, 1, 4, false);
        result->setExplicitFormat(srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
        REQUIRE(result->createMetalTexture(0, pixel.data()));
        REQUIRE(result->getMetalTexture());
        return result;
    }
    LLPointer<LLImageGL> overlayImage(bool srgb)
    {
        const std::array<U8,8> pixels{64,128,192,255, 192,64,32,255};
        LLPointer<LLImageGL> result = new LLImageGL(2, 1, 4, false);
        result->setExplicitFormat(srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
        REQUIRE(result->createMetalTexture(0, pixels.data()));
        return result;
    }
    std::unique_ptr<ALMetalTexture> rawImage(std::array<U8,4> pixel)
    {
        ALMetalTextureDescriptor td; td.width = td.height = 1;
        td.format = ALMetalPixelFormat::rgba8_unorm;
        td.usage = ALMetalTextureUsage::shader_read; td.label = "Terrain raw-view independent oracle";
        auto result = ALMetalTexture::create(*device, td, &error); REQUIRE(result);
        auto command = device->makeCommandBuffer("Terrain oracle upload", &error); REQUIRE(command);
        REQUIRE(result->upload(*command, 0, 0, {}, pixel.data(), 4, &error));
        REQUIRE(command->commitAndWait(&error));
        return result;
    }
    using Pixels = std::array<std::vector<U8>, 5>;
    Pixels render(const ALMetalFetchedTextureBinding& input, bool reverse,
                  const ALMetalTexture* oracle = nullptr,
                  const ALMetalFetchedTextureBinding* overlay = nullptr,
                  bool pbr = false, bool point_overlay = false)
    {
        ALMetalWorldSimpleBatch batch;
        batch.terrain = true; batch.vertices = vertices.get(); batch.vertex_count = 4;
        batch.texture_count = 4;
        for (unsigned slot = 0; slot < 4; ++slot)
        {
            batch.diffuse_textures[slot] = oracle ? oracle : input.nativeTexture();
            batch.terrain_diffuse_srgb[slot] = oracle || !input.fallback;
        }
        batch.terrain_height_texture = height.get();
        batch.terrain_composition_texture = composition.get();
        batch.terrain_alpha_ramp_texture = ramp->getMetalTexture();
        if (overlay)
        {
            batch.terrain_parcel_overlay_texture = overlay->nativeTexture();
            batch.terrain_overlay_wrap = overlay->fallback && !point_overlay;
            batch.terrain_overlay_srgb = !overlay->fallback;
        }
        std::fill_n(batch.terrain_tessellation_factors, 6, std::uint16_t{0x4400});
        batch.terrain_tessellation_pinned = true;
        batch.terrain_region_scale = 1.f; batch.terrain_detail_scale = 1.f;
        batch.terrain_triplanar = false;
        batch.modelview_projection[0] = batch.modelview_projection[5] = .8f;
        batch.modelview_projection[12] = batch.modelview_projection[13] = -.8f;
        batch.modelview_projection[14] = reverse ? .5f : 0.f;
        batch.modelview_projection[15] = 1.f;
        for (unsigned axis = 0; axis < 3; ++axis) batch.normal_matrix[axis][axis] = 1.f;
        if (pbr)
        {
            batch.terrain_pbr = true; batch.pbr_slot_count = 4;
            batch.terrain_pbr_detail = 0;
            for (unsigned axis = 0; axis < 4; ++axis)
                batch.terrain_modelview[axis * 4 + axis] = 1.f;
            for (unsigned slot = 0; slot < 4; ++slot)
            {
                auto& material = batch.pbrSlots()[slot];
                material.normal_texture = ramp->getMetalTexture();
                material.orm_texture = ramp->getMetalTexture();
                material.emissive_texture = oracle ? oracle : input.nativeTexture();
                material.roughness_factor = 1.f; material.metallic_factor = 0.f;
                material.emissive_color[0] = .25f;
                material.emissive_color[1] = .5f;
                material.emissive_color[2] = .75f;
                material.alpha_cutoff = -1.f;
                batch.terrain_emissive_srgb[slot] = oracle || !input.fallback;
                batch.terrain_uv_transform[slot][0] = 1.f;
                batch.terrain_uv_transform[slot][3] = 1.f;
                batch.terrain_normal_axes[slot][0] = 1.f;
                batch.terrain_normal_axes[slot][3] = 1.f;
            }
        }
        ALMetalWorldGBufferScene scene;
        scene.batches = &batch; scene.batch_count = 1; scene.width = scene.height = 32;
        scene.hdr = scene.normal_hdr = true; scene.reverse_z = reverse;
        auto target = renderer->render(scene, &error); INFO(error); REQUIRE(target);
        Pixels pixels;
        for (unsigned slot = 0; slot < 4; ++slot)
        {
            auto* texture = target->colorTexture(slot); REQUIRE(texture);
            const auto row = 32 * texture->bytesPerPixel(); pixels[slot].resize(32 * row);
            REQUIRE(texture->readback(*device, 0, 0, {}, pixels[slot].data(), row, &error));
        }
        pixels[4].resize(32 * 32 * sizeof(float));
        REQUIRE(target->depthStencilTexture()->readback(*device, 0, 0, {},
            pixels[4].data(), 32 * sizeof(float), &error));
        float center_depth = 0.f;
        std::memcpy(&center_depth, pixels[4].data() + (16 * 32 + 16) * sizeof(float), sizeof(float));
        CHECK(center_depth == .5f);
        return pixels;
    }
};
}

TEST_CASE("Fetched terrain placeholders render until the requested native image arrives",
          "[almetal][terrain][fetched]")
{
    TerrainFixture f;
    const std::array<U8,4> placeholder{64,128,192,255};
    const std::array<U8,4> ready_pixel{192,64,32,255};
    auto ready_default = f.image(placeholder, true);
    auto ready_requested = f.image(ready_pixel, true);
    auto raw_oracle = f.rawImage(placeholder);
    LLPointer<LLImageGL> pending = new LLImageGL(1, 1, 4, false);
    LLPointer<FetchedInput> requested = new FetchedInput(pending);
    LLPointer<FetchedInput> default_input = new FetchedInput(ready_default);
    LLPointer<FetchedInput> null_input = new FetchedInput(f.ramp);
    ALMetalFetchedTextureBinding fallback;
    REQUIRE(alResolveMetalFetchedTexture(requested, default_input, null_input, fallback));
    CHECK(fallback.fallback); CHECK(fallback.owner.get() == default_input.get());
    CHECK(requested->updates == 1);
    for (bool reverse : {false, true})
    for (bool pbr : {false, true})
    {
        const auto actual = f.render(fallback, reverse, nullptr, nullptr, pbr);
        CHECK(actual == f.render(fallback, reverse, raw_oracle.get(), nullptr, pbr));
        ALMetalFetchedTextureBinding decoded = fallback; decoded.fallback = false;
        CHECK(actual[0] != f.render(decoded, reverse, nullptr, nullptr, pbr)[0]);
    }
    // The SAME formerly pending LLImageGL becomes ready; no stale fallback
    // cache or private branch model may hide the newly uploaded resource.
    pending->setExplicitFormat(GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE);
    REQUIRE(pending->createMetalTexture(0, ready_pixel.data()));
    ALMetalFetchedTextureBinding loaded;
    REQUIRE(alResolveMetalFetchedTexture(requested, default_input, null_input, loaded));
    CHECK_FALSE(loaded.fallback); CHECK(loaded.image == pending);
    ALMetalFetchedTextureBinding direct;
    REQUIRE(alResolveMetalFetchedTexture(new FetchedInput(ready_requested), default_input, null_input, direct));
    CHECK(f.render(loaded, false) == f.render(direct, false));
    CHECK(f.render(loaded, false)[0] != f.render(fallback, false)[0]);

    LLPointer<FetchedInput> missing_default = new FetchedInput(new LLImageGL(1,1,4,false));
    LLPointer<FetchedInput> missing = new FetchedInput(new LLImageGL(1,1,4,false));
    ALMetalFetchedTextureBinding null_fallback;
    REQUIRE(alResolveMetalFetchedTexture(missing, missing_default, null_input, null_fallback));
    CHECK(null_fallback.owner.get() == null_input.get()); CHECK(null_fallback.fallback);
    ALMetalFetchedTextureBinding unresolved = loaded;
    CHECK_FALSE(alResolveMetalFetchedTexture(missing, missing, missing, unresolved));
    CHECK_FALSE(unresolved.nativeTexture()); CHECK_FALSE(unresolved.owner);
    // Immediate revival must happen before setActive clears DELETED.
    missing->deleted = true; missing->publish_on_update = ready_requested;
    REQUIRE(alResolveMetalFetchedTexture(missing, default_input, null_input, unresolved));
    CHECK_FALSE(unresolved.fallback); CHECK(unresolved.image == ready_requested);

    // Overlay fallback must accept raw RGBA8 as well as SRGB and use its raw
    // view. Both resources hold the same encoded values, so output is exact.
    ALMetalFetchedTextureBinding raw_overlay = fallback;
    raw_overlay.image = f.overlayImage(true);
    auto linear_overlay = f.overlayImage(false);
    ALMetalFetchedTextureBinding linear_binding = fallback; linear_binding.image = linear_overlay;
    CHECK(f.render(loaded, false, nullptr, &raw_overlay) ==
          f.render(loaded, false, nullptr, &linear_binding));
    CHECK(f.render(loaded, false, nullptr, &raw_overlay)[0] !=
          f.render(loaded, false, nullptr, &raw_overlay, false, true)[0]);
}
