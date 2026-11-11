/**
 * @file allight_screen_rect_native_test.cpp
 * @brief Actual light-bound producer versus unbounded native lighting.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "llmath.h"
#include "llmatrix4a.h"
#include "almetalworld.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

namespace
{
// This is the production CPU projection, not a mirrored test algorithm.
#include "../../newview/light_screen_rect_native.inl"

constexpr U32 size = 48;
using Pixels = std::vector<U16>;

LLMatrix4a projection(bool reverse)
{
    constexpr float near_plane = .05f, far_plane = 20.f;
    std::array<float, 16> values{};
    values[0] = values[5] = 1.f;
    values[11] = -1.f;
    values[10] = reverse ? near_plane / (far_plane - near_plane)
                        : -(far_plane + near_plane) / (far_plane - near_plane);
    values[14] = reverse ? far_plane * near_plane / (far_plane - near_plane)
                        : -2.f * far_plane * near_plane / (far_plane - near_plane);
    return LLMatrix4a(values.data());
}

struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    std::array<std::unique_ptr<ALMetalTexture>, 4> source;
    std::unique_ptr<ALMetalTexture> hdr_emissive;
    std::unique_ptr<ALMetalTexture> cookie;

    Fixture()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalWorldGBufferRenderer::create(*device, 1, &error);
        INFO(error); REQUIRE(renderer);
        auto frame = ALMetalWorldFrameData::create(*device, &error);
        INFO(error); REQUIRE(frame);
        frame->beginFrame(1); renderer->setFrameData(std::move(frame));
        const auto make = [&](ALMetalPixelFormat format)
        {
            ALMetalTextureDescriptor desc;
            desc.width = desc.height = 1; desc.format = format;
            desc.storage = ALMetalStorageMode::private_gpu;
            desc.usage = ALMetalTextureUsage::shader_read;
            auto texture = ALMetalTexture::create(*device, desc, &error);
            INFO(error); REQUIRE(texture); return texture;
        };
        source[0] = make(ALMetalPixelFormat::rgba8_unorm_srgb);
        source[1] = make(ALMetalPixelFormat::rgba8_unorm);
        source[2] = make(ALMetalPixelFormat::rgba16_unorm);
        source[3] = make(ALMetalPixelFormat::rgba8_unorm);
        hdr_emissive = make(ALMetalPixelFormat::rg11b10_float);
        cookie = make(ALMetalPixelFormat::rgba8_unorm);
        const std::array<U8, 4> albedo{128, 96, 64, 255}, black{0, 0, 0, 0};
        const std::array<U8, 4> white{255, 255, 255, 255};
        const std::array<U16, 4> normal{32768, 32768, 0, 22282};
        auto command = device->makeCommandBuffer("Authored local-light inputs", &error);
        REQUIRE(command);
        REQUIRE(source[0]->upload(*command, 0, 0, {}, albedo.data(), 4, &error));
        REQUIRE(source[1]->upload(*command, 0, 0, {}, black.data(), 4, &error));
        REQUIRE(source[2]->upload(*command, 0, 0, {}, normal.data(), 8, &error));
        REQUIRE(source[3]->upload(*command, 0, 0, {}, black.data(), 4, &error));
        REQUIRE(hdr_emissive->upload(*command, 0, 0, {}, black.data(), 4, &error));
        REQUIRE(cookie->upload(*command, 0, 0, {}, white.data(), 4, &error));
        REQUIRE(command->commitAndWait(&error));
    }
    ~Fixture()
    {
        CHECK(device->waitUntilIdle(&error));
    }

    std::unique_ptr<ALMetalRenderTarget> receiver(
        const LLMatrix4a& view, const LLMatrix4a& proj,
        bool reverse, bool hdr, float z)
    {
        // Named flat receiver and camera-height inputs, not a viewer/avatar.
        // The actual G-buffer billboard shader writes its real color/normal/depth.
        ALMetalWorldSimpleBatch batch;
        batch.impostor = true;
        for (U32 index = 0; index < 4; ++index)
            batch.impostor_textures[index] = source[index].get();
        batch.impostor_textures[3] = hdr ? hdr_emissive.get() : source[3].get();
        batch.impostor_center[2] = z;
        batch.impostor_left[0] = 3.f; batch.impostor_up[1] = 3.f;
        for (U32 axis = 0; axis < 3; ++axis)
            batch.impostor_normal_rotation[axis][axis] = 1.f;
        LLMatrix4a mvp; mvp.setMul(view, proj);
        std::memcpy(batch.modelview_projection, mvp.getF32ptr(), sizeof(batch.modelview_projection));
        ALMetalWorldGBufferScene scene;
        scene.batches = &batch; scene.batch_count = 1;
        scene.width = scene.height = size;
        scene.hdr = hdr; scene.normal_hdr = true; scene.reverse_z = reverse;
        std::fill(std::begin(scene.clear_color), std::end(scene.clear_color), 0.f);
        auto result = renderer->render(scene, &error); INFO(error); REQUIRE(result);
        return result;
    }

    std::unique_ptr<ALMetalRenderTarget> zero()
    {
        ALMetalRenderTargetDescriptor desc;
        desc.width = desc.height = size;
        desc.colors[0].format = ALMetalPixelFormat::rgba16_float;
        auto target = ALMetalRenderTarget::create(*device, desc, &error); REQUIRE(target);
        ALMetalRenderPassConfig clear;
        clear.colors[0].clear = {0., 0., 0., 0.};
        auto pass = target->makeRenderPass(clear, &error); REQUIRE(pass);
        auto command = device->makeCommandBuffer("Zero local-light control", &error); REQUIRE(command);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Clear control", &error);
        REQUIRE(encoder); REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        return target;
    }

    Pixels read(const ALMetalRenderTarget& target)
    {
        const auto* texture = target.colorTexture(0); REQUIRE(texture);
        REQUIRE(texture->descriptor().format == ALMetalPixelFormat::rgba16_float);
        REQUIRE(texture->width() == size); REQUIRE(texture->height() == size);
        Pixels result(size * size * 4);
        REQUIRE(texture->readback(*device, 0, 0, {}, result.data(), size * 8, &error));
        return result;
    }

    void compare(const Pixels& bounded, const Pixels& control)
    {
        REQUIRE(bounded.size() == control.size());
        std::size_t positive_pixels = 0, different_channels = 0;
        for (std::size_t index = 0; index < control.size(); ++index)
        {
            if (index % 4 == 0 && control[index] != 0) ++positive_pixels;
            if (bounded[index] != control[index]) ++different_channels;
        }
        CAPTURE(positive_pixels, different_channels);
        REQUIRE(positive_pixels > 4);
        CHECK(different_channels == 0); // Exact native half-storage equality; no tolerance.
    }

    void exercise(bool reverse, bool hdr, float camera_height,
                  float light_y, bool eye_crossing)
    {
        CAPTURE(reverse, hdr, camera_height, light_y, eye_crossing);
        const auto proj = projection(reverse);
        LLMatrix4a inverse; REQUIRE(inverse.setInverse(proj));
        LLMatrix4a view; view.setIdentity();
        view.setTranslation(LLVector3(0.f, -camera_height, 0.f));
        auto gbuffer = receiver(view, proj, reverse, hdr, eye_crossing ? -.3f : -2.f);
        LLVector4a center;
        view.affineTransform(LLVector4a(0.f, light_y, eye_crossing ? -.1f : -1.9f, 1.f), center);
        ALMetalWorldLocalLight light;
        std::copy_n(center.getF32ptr(), 3, light.position_radius);
        light.position_radius[3] = eye_crossing ? .4f : .25f;
        light.color_falloff[0] = .8f; light.color_falloff[1] = .4f;
        light.color_falloff[2] = .3f; light.color_falloff[3] = .5f;
        setMetalLightScreenRect(light.position_radius, light.screen_rect, proj, size, size);
        CAPTURE(light.screen_rect[0], light.screen_rect[1], light.screen_rect[2], light.screen_rect[3]);
        if (eye_crossing)
        {
            CHECK(light.screen_rect[0] == 0); CHECK(light.screen_rect[1] == 0);
            CHECK(light.screen_rect[2] == size); CHECK(light.screen_rect[3] == size);
        }
        else
        {
            REQUIRE(light.screen_rect[3] > 0); REQUIRE(light.screen_rect[3] < size);
        }

        ALMetalWorldLocalLightingScene point;
        point.gbuffer = gbuffer.get(); point.lights = &light; point.light_count = 1;
        point.reverse_z = reverse; point.hdr = hdr;
        std::memcpy(point.inverse_projection, inverse.getF32ptr(), sizeof(point.inverse_projection));
        auto full = zero();
        REQUIRE(renderer->accumulateLocalLights(*full, point, &error));
        const auto full_pixels = read(*full);
        point.bounded_light_count = 1;
        auto bounded = zero();
        REQUIRE(renderer->accumulateLocalLights(*bounded, point, &error));
        compare(read(*bounded), full_pixels);

        ALMetalWorldProjectorLight projector;
        projector.cookie_texture = cookie.get();
        std::copy_n(light.position_radius, 4, projector.position_radius);
        std::copy_n(light.color_falloff, 4, projector.color_falloff);
        // Named white-cookie transform, kept away from original cookie hard edges.
        projector.projector_matrix[0] = projector.projector_matrix[5] = .1f;
        projector.projector_matrix[12] = projector.projector_matrix[13] = .5f;
        projector.projector_matrix[14] = projector.projector_matrix[15] = 1.f;
        projector.projector_normal[2] = -1.f; projector.projector_normal[3] = 2.f;
        std::copy_n(light.position_radius, 3, projector.projector_origin);
        projector.projector_origin[3] = 2.f; projector.sampling[0] = 1.f;
        ALMetalWorldProjectorLightingScene spot;
        spot.gbuffer = gbuffer.get(); spot.lights = &projector; spot.light_count = 1;
        spot.reverse_z = reverse; spot.hdr = hdr; spot.normal_hdr = true;
        std::memcpy(spot.inverse_projection, inverse.getF32ptr(), sizeof(spot.inverse_projection));
        auto full_spot = zero();
        REQUIRE(renderer->accumulateProjectorLights(*full_spot, spot, &error));
        const auto spot_pixels = read(*full_spot);
        setMetalLightScreenRect(projector.position_radius, projector.screen_rect, proj, size, size);
        auto bounded_spot = zero();
        REQUIRE(renderer->accumulateProjectorLights(*bounded_spot, spot, &error));
        compare(read(*bounded_spot), spot_pixels);
    }
};
}

TEST_CASE("Projected native light bounds cover asymmetric receivers exactly",
          "[almetal][native-light-screen-rect]")
{
    Fixture fixture;
    for (bool reverse : {false, true})
        for (bool hdr : {false, true})
            for (float camera_height : {0.f, .3f})
                for (float light_y : {-.75f, .75f})
                    fixture.exercise(reverse, hdr, camera_height, light_y, false);
}

TEST_CASE("Native light bounds retain fullscreen coverage across the eye plane",
          "[almetal][native-light-screen-rect]")
{
    Fixture fixture;
    for (bool reverse : {false, true})
        for (bool hdr : {false, true})
            fixture.exercise(reverse, hdr, 0.f, 0.f, true);
}
