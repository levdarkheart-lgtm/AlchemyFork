/**
 * @file alnative_ui_texture_binding_test.cpp
 * @brief Raw native UI textures preserve ordered binding and sampling.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "llfontmetal_fixture.h"
#include <array>
#include <cstring>

namespace
{
    using Texels = std::array<U8, 16>;
    using Color = std::array<U8, 4>;
    constexpr Texels first_pixels{255,0,0,255, 0,255,0,255,
                                 0,0,255,255, 255,255,0,255};
    constexpr Texels second_pixels{17,33,65,255, 101,7,209,255,
                                  231,88,19,255, 41,171,98,255};
    constexpr S32 width = 48, height = 16;

    struct NativeUI : ll_test::NativeFontUI
    {
        NativeUI()
        {
            initRenderer();
            gGL.matrixMode(LLRender::MM_PROJECTION);
            gGL.loadIdentity(); gGL.ortho(0.f, F32(width), 0.f, F32(height), -1.f, 1.f);
            gGL.matrixMode(LLRender::MM_MODELVIEW);
        }
        std::unique_ptr<ALMetalTexture> texture(const Texels& pixels)
        {
            ALMetalTextureDescriptor desc;
            desc.width = 2; desc.height = 2;
            desc.format = ALMetalPixelFormat::rgba8_unorm;
            desc.storage = ALMetalStorageMode::private_gpu;
            desc.usage = ALMetalTextureUsage::shader_read;
            desc.label = "native UI asymmetric raw input";
            std::string error;
            auto result = ALMetalTexture::create(*device, desc, &error);
            INFO(error); REQUIRE(result);
            auto command = device->makeCommandBuffer("native UI raw texture upload", &error);
            REQUIRE(command);
            REQUIRE(result->upload(*command, 0, 0, {}, pixels.data(), 8, &error));
            REQUIRE(command->commitAndWait(&error));
            return result;
        }
        LLPointer<LLImageGL> image(const Texels& pixels)
        {
            LLPointer<LLImageRaw> raw = new LLImageRaw(2, 2, 4);
            std::memcpy(raw->getData(), pixels.data(), pixels.size());
            LLPointer<LLImageGL> result = new LLImageGL(raw, false);
            REQUIRE(result->getMetalTexture());
            return result;
        }
        void begin(LLRender::EMetalUICaptureMode mode = LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE)
        {
            REQUIRE(gGL.beginMetalUITextureCapture(ALMetalPixelFormat::rgba8_unorm,
                &gUIProgram, nullptr, nullptr, nullptr, mode));
            gGL.setViewport(0, 0, width, height);
            gUIProgram.bind();
            gGL.color4ub(255, 255, 255, 255);
        }
        static void quad(F32 left, F32 right, F32 u = .25f, F32 v = .25f)
        {
            gGL.begin(LLRender::TRIANGLES);
            for (const auto& xy : std::array<std::array<F32, 2>, 6>{{
                {left,0.f},{right,0.f},{right,F32(height)},
                {left,0.f},{right,F32(height)},{left,F32(height)}}})
            { gGL.texCoord2f(u, v); gGL.vertex2f(xy[0], xy[1]); }
            gGL.end();
        }
        std::vector<U8> finish()
        {
            gGL.flush();
            std::string error;
            LLRender::EMetalUICaptureEndState state{};
            auto result = gGL.endMetalUITextureCapture(true, false, nullptr, &error, &state);
            INFO(error); REQUIRE(state == LLRender::EMetalUICaptureEndState::COMPLETE);
            REQUIRE(result);
            std::vector<U8> pixels(width * height * 4);
            REQUIRE(result->readback(*device, 0, 0, {}, pixels.data(), width * 4, &error));
            return pixels;
        }
        static void rejected()
        {
            gGL.flush();
            std::string error;
            LLRender::EMetalUICaptureEndState state{};
            CHECK_FALSE(gGL.endMetalUITextureCapture(true, false, nullptr, &error, &state));
            CHECK(state == LLRender::EMetalUICaptureEndState::CAPTURE_INCOMPLETE);
            CHECK_FALSE(error.empty());
        }
        static void checkStripe(const std::vector<U8>& pixels, S32 left, S32 right, Color expected)
        {
            for (S32 y = 0; y < height; ++y)
                for (S32 x = left; x < right; ++x)
                {
                    INFO(x << "," << y);
                    const size_t at = (y * width + x) * 4;
                    const Color actual{pixels[at], pixels[at+1], pixels[at+2], pixels[at+3]};
                    CHECK(actual == expected);
                }
        }
    };
}

TEST_CASE("Raw native UI binding changes flush preceding texture and unbind draws solid",
          "[native_ui_texture][almetal]")
{
    NativeUI ui;
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE);
    auto first = ui.texture(first_pixels);
    auto second = ui.texture(second_pixels);
    auto logical = ui.image(first_pixels);
    auto* slot = gGL.getTextureSlot(0);
    for (auto mode : {LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE,
                      LLRender::EMetalUICaptureMode::GL_MIRROR})
    {
        INFO(U32(mode));
        ui.begin(mode);
        REQUIRE(slot->bindNative(first.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp));
        ui.quad(0, 12);
        REQUIRE(slot->bindNative(second.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp));
        ui.quad(12, 24);
        slot->unbind();
        ui.quad(24, 36);
        REQUIRE(slot->bindSampled(logical.get(), ALSamplers::PointClamp));
        ui.quad(36, 48, .75f, .75f);
        const auto pixels = ui.finish();
        ui.checkStripe(pixels, 0, 12, {255,0,0,255});
        ui.checkStripe(pixels, 12, 24, {17,33,65,255});
        ui.checkStripe(pixels, 24, 36, {255,255,255,255});
        ui.checkStripe(pixels, 36, 48, {255,255,0,255});
    }
}

TEST_CASE("Raw native UI sampler changes apply to the same resource",
          "[native_ui_texture][almetal]")
{
    NativeUI ui;
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE);
    auto texture = ui.texture(first_pixels);
    auto* slot = gGL.getTextureSlot(0);
    ui.begin();
    REQUIRE(slot->bindNative(texture.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp));
    ui.quad(0, 16, 1.25f);
    REQUIRE(slot->bindNative(texture.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::PointWrap));
    ui.quad(16, 32, 1.25f);
    REQUIRE(slot->bindNative(texture.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::BilinearClamp));
    ui.quad(32, 48, .5f, .5f);
    const auto pixels = ui.finish();
    ui.checkStripe(pixels, 0, 16, {0,255,0,255});
    ui.checkStripe(pixels, 16, 32, {255,0,0,255});
    ui.checkStripe(pixels, 32, 48, {128,128,64,255});
}

TEST_CASE("Native UI capture restores raw and image bindings including their samplers",
          "[native_ui_texture][almetal]")
{
    NativeUI ui;
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE);
    auto texture = ui.texture(first_pixels);
    auto logical = ui.image(first_pixels);
    auto* slot = gGL.getTextureSlot(0);
    REQUIRE(slot->bindNative(texture.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::PointWrap));
    ui.begin();
    REQUIRE(slot->bindSampled(logical.get(), ALSamplers::PointClamp));
    ui.quad(0, width, 1.25f);
    ui.checkStripe(ui.finish(), 0, width, {0,255,0,255});
    CHECK(slot->getCurrImage() == nullptr);
    CHECK(slot->getCurrMetalTexture() == texture.get());
    CHECK(slot->getCurrType() == ALTextureSlot::TT_TEXTURE);
    // A mirror capture consumes the restored ordinary slot and sampler without
    // rebinding it. The original wrap selection must still sample red.
    ui.begin(LLRender::EMetalUICaptureMode::GL_MIRROR);
    ui.quad(0, width, 1.25f);
    ui.checkStripe(ui.finish(), 0, width, {255,0,0,255});

    REQUIRE(slot->bindSampled(logical.get(), ALSamplers::PointClamp));
    ui.begin();
    REQUIRE(slot->bindNative(texture.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::BilinearClamp));
    ui.quad(0, width);
    gGL.flush();
    CHECK_FALSE(gGL.endMetalUITextureCapture(false));
    CHECK(slot->getCurrImage() == logical.get());
    CHECK(slot->getCurrMetalTexture() == logical->getMetalTexture());
    ui.begin(LLRender::EMetalUICaptureMode::GL_MIRROR);
    ui.quad(0, width, 1.25f);
    ui.checkStripe(ui.finish(), 0, width, {0,255,0,255});
}

TEST_CASE("Encoded raw native UI texture survives caller retirement before submission",
          "[native_ui_texture][almetal]")
{
    NativeUI ui;
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE);
    auto texture = ui.texture(second_pixels);
    auto* slot = gGL.getTextureSlot(0);
    slot->unbind();
    ui.begin();
    REQUIRE(slot->bindNative(texture.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp));
    ui.quad(0, width, .25f, .75f);
    gGL.flush(); // Actual encoding borrows the wrapper only through this call.
    slot->unbind();
    texture.reset(); // The command still has not been submitted by end capture.
    ui.checkStripe(ui.finish(), 0, width, {231,88,19,255});
    CHECK(slot->getCurrMetalTexture() == nullptr);
}

TEST_CASE("Authoritative raw native UI binding rejects incompatible resources and slots",
          "[native_ui_texture][almetal]")
{
    NativeUI ui;
    auto texture = ui.texture(first_pixels);
    std::string error;
    ALMetalTextureDescriptor desc;
    desc.width = 2; desc.height = 2;
    desc.usage = ALMetalTextureUsage::render_target;
    desc.label = "native UI non-sampled rejection";
    auto nonsampled = ALMetalTexture::create(*ui.device, desc, &error);
    REQUIRE(nonsampled);
    desc.usage = ALMetalTextureUsage::shader_read;
    desc.format = ALMetalPixelFormat::rgba8_uint;
    auto integer = ALMetalTexture::create(*ui.device, desc, &error);
    REQUIRE(integer);
    desc.format = ALMetalPixelFormat::rgba8_unorm;
    desc.type = ALMetalTextureType::texture_2d_array; desc.array_length = 2;
    auto layered = ALMetalTexture::create(*ui.device, desc, &error);
    REQUIRE(layered);
    auto* slot = gGL.getTextureSlot(0);
    for (unsigned invalid = 0; invalid < 6; ++invalid)
    {
        INFO(invalid);
        ui.begin();
        switch (invalid)
        {
            case 0: CHECK_FALSE(slot->bindNative(nullptr, ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp)); break;
            case 1: CHECK_FALSE(gGL.getTextureSlot(1)->bindNative(texture.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp)); break;
            case 2: CHECK_FALSE(slot->bindNative(texture.get(), ALTextureSlot::TT_CUBE_MAP, ALSamplers::PointClamp)); break;
            case 3: CHECK_FALSE(slot->bindNative(nonsampled.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp)); break;
            case 4: CHECK_FALSE(slot->bindNative(integer.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp)); break;
            case 5: CHECK_FALSE(slot->bindNative(layered.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp)); break;
        }
        ui.rejected();
    }
}

TEST_CASE("Image UI display lists reject raw bindings without caching unowned resources",
          "[native_ui_texture][almetal]")
{
    NativeUI ui;
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE);
    auto texture = ui.texture(first_pixels);
    auto* slot = gGL.getTextureSlot(0);
    for (auto mode : {LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE,
                      LLRender::EMetalUICaptureMode::GL_MIRROR})
    {
        std::list<LLVertexBufferData> list;
        ui.begin(mode);
        gGL.beginList(&list);
        CHECK_FALSE(slot->bindNative(texture.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp));
        gGL.endList();
        CHECK(list.empty());
        ui.rejected();

        ui.begin(mode);
        REQUIRE(slot->bindNative(texture.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp));
        gGL.beginList(&list); // A raw binding can also predate recording.
        ui.quad(0, width);
        gGL.endList();
        CHECK(list.empty());
        ui.rejected();
    }
}

TEST_CASE("Retained image UI replay keeps its image while an unrelated raw texture is bound",
          "[native_ui_texture][almetal]")
{
    NativeUI ui;
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE);
    auto logical = ui.image(first_pixels);
    auto unrelated = ui.texture(second_pixels);
    auto* slot = gGL.getTextureSlot(0);
    std::list<LLVertexBufferData> list;
    ui.begin();
    gGL.beginList(&list);
    REQUIRE(slot->bindSampled(logical.get(), ALSamplers::PointClamp));
    ui.quad(0, width, .75f, .75f);
    gGL.endList();
    REQUIRE(list.size() == 1);
    CHECK(list.front().mTexImage == logical.get());
    ui.checkStripe(ui.finish(), 0, width, {255,255,0,255});
    logical = nullptr; // The recorded image owner must keep the allocation live.
    ui.begin();
    REQUIRE(slot->bindNative(unrelated.get(), ALTextureSlot::TT_TEXTURE, ALSamplers::BilinearWrap));
    list.front().drawWithMatrix();
    ui.checkStripe(ui.finish(), 0, width, {255,255,0,255});
}
