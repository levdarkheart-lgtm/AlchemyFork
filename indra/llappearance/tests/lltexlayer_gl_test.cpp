/**
 * @file lltexlayer_gl_test.cpp
 * @brief Native ordered appearance morph-mask readback on the hidden window.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../lltexlayer.h"

#include "llgl.h"
#include "llimage.h"
#include "llimagegl.h"
#include "llrender.h"
#include "llrender2dutils.h"
#include "../../llrender/tests/llheadlessgl_fixture.h"

#if LL_DARWIN
#include "../../almetal/almetalresource.h"
#endif

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <vector>

#if LL_WINDOWS
#include "llwin32headers.h"
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace
{
    constexpr S32 TARGET_W = 64;
    constexpr S32 TARGET_H = 48;
    constexpr S32 LAYER_X = 7;
    constexpr S32 LAYER_Y = 5;
    constexpr S32 LAYER_W = 23;
    constexpr S32 LAYER_H = 19;

    U8 alphaAt(S32 x, S32 y)
    {
        return (U8)(x * 7 + y * 13 + (x >> 3) * 59 + (y >> 2) * 101);
    }

    // `size` bytes ending at a protected page: an overrun faults at its first
    // byte rather than corrupting an unrelated test allocation.
    class GuardedBuffer
    {
    public:
        explicit GuardedBuffer(size_t size)
        : mSize(size)
        {
#if LL_WINDOWS
            SYSTEM_INFO info;
            GetSystemInfo(&info);
            const size_t page = info.dwPageSize;
#else
            const size_t page = (size_t)sysconf(_SC_PAGESIZE);
#endif
            const size_t usable = (size + page - 1) / page * page;
            mMapped = usable + page;
#if LL_WINDOWS
            mBase = static_cast<U8*>(VirtualAlloc(
                nullptr, mMapped, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            DWORD old_protection = 0;
            mGuarded = mBase && VirtualProtect(
                mBase + usable, page, PAGE_NOACCESS, &old_protection);
#else
            void* mapped = mmap(
                nullptr, mMapped, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            mBase = mapped == MAP_FAILED ? nullptr : static_cast<U8*>(mapped);
            mGuarded = mBase &&
                mprotect(mBase + usable, page, PROT_NONE) == 0;
#endif
            if (mBase)
            {
                mData = mBase + usable - size;
            }
        }

        ~GuardedBuffer()
        {
            if (mBase)
            {
#if LL_WINDOWS
                VirtualFree(mBase, 0, MEM_RELEASE);
#else
                munmap(mBase, mMapped);
#endif
            }
        }

        GuardedBuffer(const GuardedBuffer&) = delete;
        GuardedBuffer& operator=(const GuardedBuffer&) = delete;

        bool guarded() const { return mGuarded; }
        U8* data() const { return mData; }
        size_t size() const { return mSize; }

    private:
        U8* mBase = nullptr;
        U8* mData = nullptr;
        size_t mSize = 0;
        size_t mMapped = 0;
        bool mGuarded = false;
    };

#if LL_DARWIN
    struct CaptureGuard
    {
        ~CaptureGuard()
        {
            if (active)
            {
                gGL.endMetalUITextureCapture(false);
            }
        }
        bool active = false;
    };

    struct MetalBackendGuard
    {
        ~MetalBackendGuard()
        {
            gGL.setMetalBackend(nullptr, nullptr, nullptr);
            LLImageGL::setMetalDevice(nullptr);
            LLVertexBuffer::setMetalDevice(nullptr);
        }
    };

    TEST_CASE("lltexlayer: ordered Metal capture returns exact morph alpha",
              "[lltexlayer_gl][almetal]")
    {
        ll_test::HeadlessGL context(
            /*needs_vbos=*/true, /*needs_imagegl=*/true,
            /*needs_llrender=*/true, /*needs_render=*/true);
        LLWindow* window = context.window();
        REQUIRE(window);
        REQUIRE(window->getMetalDevice());
        REQUIRE(window->getMetalFrameScheduler());
        LLVertexBuffer::setMetalDevice(window->getMetalDevice());
        LLImageGL::setMetalDevice(window->getMetalDevice());
        gGL.setMetalBackend(
            window->getMetalDevice(), window->getMetalFrameScheduler(),
            window->getMetalSurface());
        MetalBackendGuard metal_backend;

        LLPointer<LLImageRaw> raw =
            new LLImageRaw(TARGET_W, TARGET_H, 4);
        U8* texels = raw->getData();
        for (S32 y = 0; y < TARGET_H; ++y)
        {
            for (S32 x = 0; x < TARGET_W; ++x)
            {
                U8* texel = &texels[((size_t)y * TARGET_W + x) * 4];
                texel[0] = (U8)x;
                texel[1] = (U8)y;
                texel[2] = 0x5a;
                texel[3] = alphaAt(x, y);
            }
        }
        LLPointer<LLImageGL> texture = new LLImageGL(false);
        REQUIRE(texture->createGLTexture(0, raw.get()));
        REQUIRE(texture->getMetalTexture());

        gUIProgram.bind();
        gGL.setViewport(0, 0, TARGET_W, TARGET_H);
        glViewport(0, 0, TARGET_W, TARGET_H);
        gGL.setColorMask(true, true);
        gGL.setSceneBlendType(LLRender::BT_REPLACE);
        gGL.color4f(1.f, 1.f, 1.f, 1.f);

        CaptureGuard capture;
        capture.active = gGL.beginMetalUITextureCapture(
            ALMetalPixelFormat::rgba8_unorm, &gUIProgram);
        REQUIRE(capture.active);

        gGL.getTextureSlot(0)->bindSampled(
            texture.get(), ALSamplers::PointClamp);
        // The shared fixture's projection spans 256 pixels. The smaller
        // viewport maps this complete quad to the 64x48 capture target.
        gl_rect_2d_simple_tex(256, 256);
        gGL.getTextureSlot(0)->unbind();

        GuardedBuffer scratch(
            LLTexLayer::alphaReadbackScratchBytes(LAYER_W, LAYER_H));
        GuardedBuffer alpha((size_t)LAYER_W * LAYER_H);
        REQUIRE(scratch.guarded());
        REQUIRE(alpha.guarded());
        std::fill_n(alpha.data(), alpha.size(), U8(0xcd));

        REQUIRE(LLTexLayer::readBackAlpha(
            LAYER_X, LAYER_Y, LAYER_W, LAYER_H,
            scratch.data(), scratch.size(), alpha.data()));
        for (S32 row = 0; row < LAYER_H; ++row)
        {
            for (S32 col = 0; col < LAYER_W; ++col)
            {
                INFO("pixel " << col << ", " << row);
                CHECK(alpha.data()[(size_t)row * LAYER_W + col] ==
                      alphaAt(LAYER_X + col, LAYER_Y + row));
            }
        }

        LLRender::EMetalUICaptureEndState end_state =
            LLRender::EMetalUICaptureEndState::INACTIVE;
        auto completed = gGL.endMetalUITextureCapture(
            true, false, nullptr, nullptr, &end_state);
        capture.active = false;
        REQUIRE(completed);
        CHECK(end_state == LLRender::EMetalUICaptureEndState::COMPLETE);
    }

    TEST_CASE("lltexlayer: native alpha readback rejects short scratch",
              "[lltexlayer_gl][almetal]")
    {
        std::vector<U8> scratch(
            LLTexLayer::alphaReadbackScratchBytes(LAYER_W, LAYER_H) - 1);
        std::vector<U8> alpha((size_t)LAYER_W * LAYER_H, 0xcd);
        CHECK_FALSE(LLTexLayer::readBackAlpha(
            LAYER_X, LAYER_Y, LAYER_W, LAYER_H,
            scratch.data(), scratch.size(), alpha.data()));
        CHECK(std::all_of(
            alpha.begin(), alpha.end(),
            [](U8 value) { return value == 0xcd; }));
    }

    TEST_CASE("lltexlayer: authoritative Metal capture needs no GL backing",
              "[lltexlayer_gl][almetal]")
    {
        ll_test::HeadlessGL context(
            /*needs_vbos=*/true, /*needs_imagegl=*/true,
            /*needs_llrender=*/true, /*needs_render=*/true);
        LLWindow* window = context.window();
        REQUIRE(window);
        REQUIRE(window->getMetalDevice());
        REQUIRE(window->getMetalFrameScheduler());
        LLVertexBuffer::setMetalDevice(window->getMetalDevice());
        LLImageGL::setMetalDevice(window->getMetalDevice());
        gGL.setMetalBackend(
            window->getMetalDevice(), window->getMetalFrameScheduler(),
            window->getMetalSurface());
        MetalBackendGuard metal_backend;

        LLPointer<LLImageRaw> raw =
            new LLImageRaw(TARGET_W, TARGET_H, 4);
        U8* texels = raw->getData();
        for (S32 y = 0; y < TARGET_H; ++y)
        {
            for (S32 x = 0; x < TARGET_W; ++x)
            {
                U8* texel = &texels[((size_t)y * TARGET_W + x) * 4];
                texel[0] = (U8)(x * 3);
                texel[1] = (U8)(y * 5);
                texel[2] = 0x6d;
                texel[3] = alphaAt(x, y);
            }
        }
        LLPointer<LLImageGL> texture = new LLImageGL(false);
        REQUIRE(texture->createMetalTexture(0, raw.get()));
        REQUIRE(texture->getMetalTexture());
        REQUIRE(texture->getTexName() == 0);

        gUIProgram.bind();
        gGL.flush();

        struct PhysicalState
        {
            GLint program = 0;
            GLint array_buffer = 0;
            GLint element_buffer = 0;
            GLint texture = 0;
            GLint sampler = 0;
            std::array<GLint, 4> viewport{};
            std::array<GLboolean, 4> color_mask{};
            GLboolean blend = GL_FALSE;
            GLboolean depth = GL_FALSE;
        } before, after;
        const auto read_physical_state = [](PhysicalState& state)
        {
            glGetIntegerv(GL_CURRENT_PROGRAM, &state.program);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state.array_buffer);
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,
                          &state.element_buffer);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.texture);
            glGetIntegerv(GL_SAMPLER_BINDING, &state.sampler);
            glGetIntegerv(GL_VIEWPORT, state.viewport.data());
            glGetBooleanv(GL_COLOR_WRITEMASK, state.color_mask.data());
            state.blend = glIsEnabled(GL_BLEND);
            state.depth = glIsEnabled(GL_DEPTH_TEST);
        };
        read_physical_state(before);
        LLGLSLShader* const logical_shader =
            LLGLSLShader::sCurBoundShaderPtr;
        const U32 logical_program = LLGLSLShader::sCurBoundShader;
        const F32 logical_minimum_alpha = gUIProgram.getMinimumAlpha();
        ALTextureSlot* const slot = gGL.getTextureSlot(0);
        REQUIRE(slot);
        LLImageGL* const logical_image = slot->getCurrImage();
        const U32 logical_texture = slot->getCurrTexture();
        const ALTextureSlot::eTextureType logical_type = slot->getCurrType();

        CaptureGuard capture;
        capture.active = gGL.beginMetalUITextureCapture(
            ALMetalPixelFormat::rgba8_unorm_srgb, &gUIProgram,
            nullptr, nullptr, nullptr,
            LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE,
            LLRender::EMetalUICaptureStorage::LINEAR_STORAGE);
        REQUIRE(capture.active);
        REQUIRE(gGL.isMetalUITextureCaptureAuthoritative());

        gUIProgram.bind();
        gUIProgram.setMinimumAlpha(0.f);
        gGL.setViewport(0, 0, TARGET_W, TARGET_H);
        gGL.setColorMask(true, true);
        gGL.setSceneBlendType(LLRender::BT_REPLACE);
        gGL.color4f(1.f, 1.f, 1.f, 1.f);
        REQUIRE(gGL.getTextureSlot(0)->bindSampled(
            texture.get(), ALSamplers::PointClamp));
        gl_rect_2d_simple_tex(256, 256);
        gGL.getTextureSlot(0)->unbind();

        // Production bakes checkpoint morph alpha in the middle of this sRGB
        // storage transaction. The render view is deliberately linear so
        // these are the original encoded bytes, matching GL with framebuffer
        // sRGB disabled. Prove both the read and the resumed draw sequence.
        GuardedBuffer scratch(
            LLTexLayer::alphaReadbackScratchBytes(LAYER_W, LAYER_H));
        GuardedBuffer alpha((size_t)LAYER_W * LAYER_H);
        REQUIRE(scratch.guarded());
        REQUIRE(alpha.guarded());
        std::fill_n(alpha.data(), alpha.size(), U8(0xcd));
        REQUIRE(LLTexLayer::readBackAlpha(
            LAYER_X, LAYER_Y, LAYER_W, LAYER_H,
            scratch.data(), scratch.size(), alpha.data()));
        for (S32 row = 0; row < LAYER_H; ++row)
        {
            for (S32 col = 0; col < LAYER_W; ++col)
            {
                INFO("morph pixel " << col << ", " << row);
                CHECK(alpha.data()[(size_t)row * LAYER_W + col] ==
                      alphaAt(LAYER_X + col, LAYER_Y + row));
            }
        }

        REQUIRE(gGL.getTextureSlot(0)->bindSampled(
            texture.get(), ALSamplers::PointClamp));
        gl_rect_2d_simple_tex(256, 256);
        gGL.getTextureSlot(0)->unbind();
        gUIProgram.unbind();

        auto completed = gGL.endMetalUITextureCapture(true);
        capture.active = false;
        REQUIRE(completed);
        CHECK_FALSE(gGL.isMetalUITextureCaptureAuthoritative());

        std::vector<U8> readback((size_t)TARGET_W * TARGET_H * 4);
        ALMetalRegion2D region{
            0, 0, static_cast<U32>(TARGET_W), static_cast<U32>(TARGET_H)};
        std::string error;
        REQUIRE(completed->readback(
            *window->getMetalDevice(), 0, 0, region,
            readback.data(), (size_t)TARGET_W * 4, &error));
        INFO(error);
        for (S32 y = 0; y < TARGET_H; ++y)
        {
            for (S32 x = 0; x < TARGET_W; ++x)
            {
                INFO("pixel " << x << ", " << y);
                CHECK(readback[((size_t)y * TARGET_W + x) * 4] ==
                      (U8)(x * 3));
                CHECK(readback[((size_t)y * TARGET_W + x) * 4 + 1] ==
                      (U8)(y * 5));
                CHECK(readback[((size_t)y * TARGET_W + x) * 4 + 2] ==
                      0x6d);
                CHECK(readback[((size_t)y * TARGET_W + x) * 4 + 3] ==
                      alphaAt(x, y));
            }
        }

        read_physical_state(after);
        CHECK(after.program == before.program);
        CHECK(after.array_buffer == before.array_buffer);
        CHECK(after.element_buffer == before.element_buffer);
        CHECK(after.texture == before.texture);
        CHECK(after.sampler == before.sampler);
        CHECK(after.viewport == before.viewport);
        CHECK(after.color_mask == before.color_mask);
        CHECK(after.blend == before.blend);
        CHECK(after.depth == before.depth);
        CHECK(LLGLSLShader::sCurBoundShaderPtr == logical_shader);
        CHECK(LLGLSLShader::sCurBoundShader == logical_program);
        CHECK(gUIProgram.getMinimumAlpha() == logical_minimum_alpha);
        CHECK(slot->getCurrImage() == logical_image);
        CHECK(slot->getCurrTexture() == logical_texture);
        CHECK(slot->getCurrType() == logical_type);
        CHECK(texture->getTexName() == 0);

        LLGLSLShader::unbind();
    }
#endif
}
