/**
 * @file altextureslot_metal_test.cpp
 * @brief Pending viewer images retain the original default-image UI contract.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * This library is free software; you can redistribute it and/or modify it
 * under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../llrender.h"
#include "../llgl.h"
#include "../llglslshader.h"
#include "../llimagegl.h"
#include "../lltexture.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetalframe.h"
#include "../../almetal/almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstring>
#include <functional>
#include <vector>

namespace
{
    // No LLViewerTexture dependency: the override exercises its public virtual
    // default-binding contract through the real ALTextureSlot/LLRender owner.
    struct PendingTexture final : LLTexture
    {
        LLPointer<LLImageGL> image;
        LLPointer<LLImageGL> fallback;
        LLPointer<LLImageGL> publish_on_update;
        LLTexture* recursive_fallback = nullptr;
        unsigned updates = 0, defaults = 0, active = 0;
        bool noop_default = false;
        bool deleted = true;
        explicit PendingTexture(LLImageGL* value) : image(value) {}
        ~PendingTexture() override = default;
        LLImageGL* getGLTexture() const override { return image; }
        void setActive() override { ++active; deleted = false; }
        void forceImmediateUpdate() override
        {
            ++updates;
            if (deleted && publish_on_update.notNull()) image = publish_on_update;
        }
        bool bindDefaultImage(S32 stage) override
        {
            ++defaults;
            if (noop_default) return true;
            if (recursive_fallback)
                return gGL.getTextureSlot(stage)->bindSampled(
                    recursive_fallback, ALSamplers::AnisoWrap);
            return fallback.notNull() && gGL.getTextureSlot(stage)->bindSampled(
                fallback.get(), ALSamplers::AnisoWrap);
        }
    };

    struct NativeUI
    {
        std::unique_ptr<ALMetalDevice> device;
        std::unique_ptr<ALMetalFrameScheduler> frames;
        ALMetalDevice* previous_image_device = LLImageGL::getMetalDevice();
        bool previous_authority = LLVertexBuffer::isMetalBufferAuthority();
        LLImageGL* previous_default = LLImageGL::sDefaultGLTexture;
        LLGLSLShader* previous_shader = LLGLSLShader::sCurBoundShaderPtr;
        U32 previous_program = LLGLSLShader::sCurBoundShader;
        U32 previous_ui_mask = gUIProgram.mAttributeMask;
        LLRender::eMatrixMode previous_mode = gGL.getMatrixMode();
        std::string error;
        NativeUI()
        {
            device = ALMetalDevice::create(&error);
            INFO(error); REQUIRE(device);
            frames = ALMetalFrameScheduler::create(*device, 2, &error);
            INFO(error); REQUIRE(frames);
            LLImageGL::setMetalDevice(device.get());
            LLVertexBuffer::setMetalDevice(device.get());
            LLVertexBuffer::setMetalBufferAuthority(true);
            LLVertexBuffer::initClass(nullptr);
            gGL.setMetalBackend(device.get(), frames.get(), nullptr);
            REQUIRE(gGL.initMetal(true));
            for (auto mode : {LLRender::MM_MODELVIEW, LLRender::MM_PROJECTION,
                              LLRender::MM_TEXTURE0})
            {
                gGL.matrixMode(mode); gGL.pushMatrix(); gGL.loadIdentity();
            }
            gGL.matrixMode(LLRender::MM_MODELVIEW);
            gUIProgram.mAttributeMask = LLVertexBuffer::MAP_VERTEX |
                LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR;
        }
        ~NativeUI()
        {
            if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
            for (auto mode : {LLRender::MM_TEXTURE0, LLRender::MM_PROJECTION,
                              LLRender::MM_MODELVIEW})
            { gGL.matrixMode(mode); gGL.popMatrix(); }
            gGL.matrixMode(previous_mode);
            LLImageGL::sDefaultGLTexture = previous_default;
            CHECK(frames->waitUntilIdle(5000, &error));
            gGL.shutdown();
            gGL.setMetalBackend(nullptr, nullptr, nullptr);
            LLVertexBuffer::cleanupClass();
            LLVertexBuffer::setMetalBufferAuthority(previous_authority);
            LLVertexBuffer::setMetalDevice(previous_image_device);
            LLImageGL::setMetalDevice(previous_image_device);
            gUIProgram.mAttributeMask = previous_ui_mask;
            LLGLSLShader::sCurBoundShader = previous_program;
            LLGLSLShader::sCurBoundShaderPtr = previous_shader;
        }
        void begin()
        {
            REQUIRE(gGL.beginMetalUITextureCapture(
                ALMetalPixelFormat::rgba8_unorm, &gUIProgram, nullptr, nullptr,
                nullptr, LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
            gGL.setViewport(0, 0, 64, 64);
            gUIProgram.bind();
        }
        static void quad(F32 left, F32 right)
        {
            // U=1.25 distinguishes caller PointClamp (green) from default
            // AnisoWrap (red), independently of any private sampler cache.
            gGL.color4ub(255, 255, 255, 255);
            gGL.begin(LLRender::TRIANGLES);
            for (const auto& xy : std::array<std::array<F32, 2>, 6>{{
                     {left,-1.f},{right,-1.f},{right,1.f},
                     {left,-1.f},{right,1.f},{left,1.f}}})
            { gGL.texCoord2f(1.25f, .5f); gGL.vertex2f(xy[0], xy[1]); }
            gGL.end();
        }
        std::vector<U8> finish(bool complete = true)
        {
            gGL.flush();
            LLRender::EMetalUICaptureEndState state{};
            std::string failure;
            auto output = gGL.endMetalUITextureCapture(
                true, false, nullptr, &failure, &state);
            INFO(failure);
            if (!complete)
            {
                CHECK_FALSE(output);
                CHECK(state == LLRender::EMetalUICaptureEndState::CAPTURE_INCOMPLETE);
                return {};
            }
            REQUIRE(state == LLRender::EMetalUICaptureEndState::COMPLETE);
            REQUIRE(output);
            std::vector<U8> pixels(64 * 64 * 4);
            REQUIRE(output->readback(*device, 0, 0, {}, pixels.data(), 64 * 4, &error));
            INFO(error);
            return pixels;
        }
        std::vector<U8> render(const std::function<void()>& bind)
        {
            begin(); bind(); quad(-1.f,1.f); return finish();
        }
    };
}

TEST_CASE("Pending texture fallback preserves complete native UI publication",
          "[altextureslot][almetal][native_ui][pending]")
{
    NativeUI ui;
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE);
    LLPointer<LLImageRaw> raw = new LLImageRaw(2, 1, 4);
    const U8 colors[]{255,0,0,255, 0,255,0,255};
    std::memcpy(raw->getData(), colors, sizeof(colors));
    LLPointer<LLImageGL> ready = new LLImageGL(raw, false);
    LLPointer<LLImageGL> pending = new LLImageGL(2, 1, 4, false);
    REQUIRE(ready->getMetalTexture());
    REQUIRE_FALSE(pending->getMetalTexture());
    CHECK(ready->getTexName() == 0);
    LLImageGL::sDefaultGLTexture = ready;
    auto* slot = gGL.getTextureSlot(0);
    const auto clamp = ui.render([&] { REQUIRE(slot->bindSampled(ready.get(), ALSamplers::PointClamp)); });
    const auto wrap = ui.render([&] { REQUIRE(slot->bindSampled(ready.get(), ALSamplers::AnisoWrap)); });
    REQUIRE(clamp != wrap);
    const std::size_t center = (32 * 64 + 32) * 4;
    CHECK(clamp[center] == 0); CHECK(clamp[center+1] == 255);
    CHECK(wrap[center] == 255); CHECK(wrap[center+1] == 0);

    PendingTexture texture(pending);
    texture.fallback = ready;
    CHECK(ui.render([&] { slot->bindFast(&texture, ALSamplers::PointClamp); }) == clamp);
    CHECK(texture.updates == 1); CHECK(texture.defaults == 1);
    CHECK(ui.render([&] { REQUIRE(slot->bindSampled(&texture, ALSamplers::PointClamp)); }) == wrap);
    CHECK(texture.updates == 2); CHECK(texture.defaults == 2);
    CHECK(ui.render([&] { REQUIRE(slot->bindSampled(pending.get(), ALSamplers::PointClamp)); }) == clamp);

    // Re-resolve after forceImmediateUpdate: the LLTexture may publish a new
    // LLImageGL object, not merely fill the originally returned object.
    texture.deleted = true; // Real setActive clears the deleted/revive gate.
    texture.publish_on_update = ready;
    CHECK(ui.render([&] { REQUIRE(slot->bindSampled(&texture, ALSamplers::PointClamp)); }) == clamp);
    CHECK(texture.updates == 3); CHECK(texture.defaults == 2);
    CHECK(ui.render([&] { slot->bindFast(&texture, ALSamplers::PointClamp); }) == clamp);
    CHECK(texture.updates == 3); CHECK(texture.defaults == 2);

    // Actual same-image publication must stop using the default on the next
    // bind, without recreating or resetting the capture sampler cache.
    texture.publish_on_update = nullptr;
    texture.image = pending;
    REQUIRE(pending->createMetalTexture(0, raw));
    CHECK(ui.render([&] { slot->bindFast(&texture, ALSamplers::PointClamp); }) == clamp);
    CHECK(texture.updates == 3); CHECK(texture.defaults == 2);
    pending = new LLImageGL(2, 1, 4, false);
    texture.image = pending;

    // Queued prefix geometry must retain its prior sampler across fallback.
    const auto two_halves = [&](bool use_fallback)
    {
        ui.begin(); REQUIRE(slot->bindSampled(ready.get(), ALSamplers::AnisoWrap));
        ui.quad(-1.f,0.f);
        if (use_fallback) { texture.image = pending; slot->bindFast(&texture, ALSamplers::PointClamp); }
        else REQUIRE(slot->bindSampled(ready.get(), ALSamplers::PointClamp));
        ui.quad(0.f,1.f); return ui.finish();
    };
    texture.publish_on_update = nullptr;
    CHECK(two_halves(true) == two_halves(false));

    // A virtual true/no-op must not accept the prefix's previous ready image.
    // Failure suppresses the entire transaction, including the encoded prefix.
    for (bool fast : {false,true})
    {
        ui.begin(); REQUIRE(slot->bindSampled(ready.get(), ALSamplers::PointClamp));
        ui.quad(-1.f,0.f); texture.image = pending; texture.noop_default = true;
        if (fast) slot->bindFast(&texture, ALSamplers::PointClamp);
        else CHECK_FALSE(slot->bindSampled(&texture, ALSamplers::PointClamp));
        CHECK(slot->getCurrImage() == nullptr);
        ui.finish(false);
    }
    texture.noop_default = false;
    texture.recursive_fallback = &texture;
    ui.begin(); CHECK_FALSE(slot->bindSampled(&texture, ALSamplers::PointClamp)); ui.finish(false);
    texture.recursive_fallback = nullptr;
    LLImageGL::sDefaultGLTexture = pending;
    ui.begin(); CHECK_FALSE(slot->bindSampled(pending.get(), ALSamplers::PointClamp)); ui.finish(false);
    LLImageGL::sDefaultGLTexture = nullptr;
    ui.begin(); CHECK_FALSE(slot->bindSampled(pending.get(), ALSamplers::PointClamp)); ui.finish(false);

    // Do not relax the authoritative capture's unsupported slot guard. The
    // attempted slot1 bind must leave UI0's identity intact and fail closed.
    ui.begin(); REQUIRE(slot->bindSampled(ready.get(), ALSamplers::PointClamp));
    ui.quad(-1.f,1.f);
    CHECK_FALSE(gGL.getTextureSlot(1)->bindSampled(ready.get(), ALSamplers::AnisoWrap));
    CHECK(slot->getCurrImage() == ready.get());
    CHECK(gGL.getTextureSlot(1)->getCurrImage() == nullptr);
    ui.finish(false);
}
