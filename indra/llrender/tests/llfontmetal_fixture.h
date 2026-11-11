/**
 * @file llfontmetal_fixture.h
 * @brief Scoped device-only font atlas and UI fixtures for native tests.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#ifndef LL_TEST_LLFONTMETAL_FIXTURE_H
#define LL_TEST_LLFONTMETAL_FIXTURE_H
#include "linden_common.h"
#include "../alfontshaping.h"
#include "../alrendercapabilities.h"
#include "../llfontbitmapcache.h"
#include "../llfontfreetype.h"
#include "../llfontgl.h"
#include "../llgl.h"
#include "../llglslshader.h"
#include "../llimagegl.h"
#include "../llrender.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalframe.h"
#include "../../almetal/almetalresource.h"
#include "alteststateguard.h"
#include "altestteardown.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_assertion_result.hpp>
#include <catch2/interfaces/catch_interfaces_capture.hpp>
#include <stdexcept>
#include <vector>
namespace ll_test
{
    // A real device and font manager replace the displaced hidden GL window.
    // The original rasterizer/packing/cache owners are exercised unchanged.
    struct NativeFont
    {
        ALRenderCapabilities old_capabilities = gRenderCapabilities;
        ALMetalDevice* old_device = LLImageGL::getMetalDevice();
        bool old_authority = LLImageGL::isMetalTextureAuthority();
        bool old_gl = LLFontBitmapCache::usesGL();
        bool old_metal = LLFontBitmapCache::usesMetal();
        std::unique_ptr<ALMetalDevice> device;
        NativeFont()
        {
            REQUIRE_FALSE(gFontManagerp);
            std::string error;
            device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
            gRenderCapabilities.initializeMetal(device->info());
            LLImageGL::setMetalDevice(device.get());
            LLImageGL::setMetalTextureAuthority(true);
            LLFontBitmapCache::setTextureBackend(false, true);
            LLFontManager::initClass();
        }
        ~NativeFont()
        {
            ALFontShaping::clearCache();
            LLFontManager::cleanupClass();
            LLFontBitmapCache::setTextureBackend(old_gl, old_metal);
            LLImageGL::setMetalTextureAuthority(old_authority);
            LLImageGL::setMetalDevice(old_device);
            gRenderCapabilities = old_capabilities;
        }
    };

    struct NativeFontUI : NativeFont
    {
        static constexpr S32 WIDTH = 256, HEIGHT = 256;
        std::unique_ptr<ALMetalFrameScheduler> frames;
        bool old_authority = LLVertexBuffer::isMetalBufferAuthority();
        LLGLSLShader* old_shader = LLGLSLShader::sCurBoundShaderPtr;
        U32 old_shader_id = LLGLSLShader::sCurBoundShader;
        U32 old_ui_mask = gUIProgram.mAttributeMask;
        U32 old_calls = LLRender::sDrawCalls;
        U64 old_elements = LLRender::sDrawElements;
        LLRender::eMatrixMode old_mode = gGL.getMatrixMode();
        bool initialized = false, matrices = false;
        void initRenderer()
        {
            std::string error;
            frames = ALMetalFrameScheduler::create(*device, 2, &error);
            INFO(error); REQUIRE(frames);
            LLVertexBuffer::setMetalDevice(device.get());
            LLVertexBuffer::setMetalBufferAuthority(true);
            LLVertexBuffer::initClass(nullptr);
            gGL.setMetalBackend(device.get(), frames.get(), nullptr);
            initialized = true;
            REQUIRE(gGL.initMetal(true));
            for (auto mode : {LLRender::MM_MODELVIEW, LLRender::MM_PROJECTION, LLRender::MM_TEXTURE0})
            { gGL.matrixMode(mode); gGL.pushMatrix(); gGL.loadIdentity(); }
            matrices = true;
            gGL.matrixMode(LLRender::MM_PROJECTION);
            gGL.ortho(0.f, F32(WIDTH), 0.f, F32(HEIGHT), -1.f, 1.f);
            gGL.matrixMode(LLRender::MM_MODELVIEW);
            gGL.setViewport(0, 0, WIDTH, HEIGHT);
            // The native UI owner selects this global semantic identity and
            // consumes the same position/UV/colour layout as the font producer.
            gUIProgram.mAttributeMask = LLVertexBuffer::MAP_VERTEX |
                LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR;
            gUIProgram.bind();
        }
        // Process-shared font tests end after Catch's assertion context. Let
        // their teardown report an ordinary error without invoking CHECK there.
        bool shutdownRenderer(std::string* error)
        {
            if (!initialized) return true;
            if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
            if (LLGLSLShader::sCurBoundShaderPtr) LLGLSLShader::sCurBoundShaderPtr->unbind();
            if (matrices)
                for (auto mode : {LLRender::MM_TEXTURE0, LLRender::MM_PROJECTION, LLRender::MM_MODELVIEW})
                { gGL.matrixMode(mode); gGL.popMatrix(); }
            gGL.matrixMode(old_mode);
            const bool idle = frames->waitUntilIdle(5000, error);
            gGL.shutdown(); gGL.setMetalBackend(nullptr, nullptr, nullptr);
            LLVertexBuffer::cleanupClass();
            LLVertexBuffer::setMetalBufferAuthority(old_authority);
            LLVertexBuffer::setMetalDevice(NativeFont::old_device);
            gUIProgram.mAttributeMask = old_ui_mask;
            LLGLSLShader::sCurBoundShader = old_shader_id;
            LLGLSLShader::sCurBoundShaderPtr = old_shader;
            LLRender::sDrawCalls = old_calls; LLRender::sDrawElements = old_elements;
            initialized = false;
            return idle;
        }
        ~NativeFontUI()
        {
            if (!initialized) return;
            std::string error;
            const bool idle = shutdownRenderer(&error);
            INFO(error); CHECK(idle);
        }
    };

    // Default-font getters retain raw registry pointers for the process. The
    // original FontGL/TextCache binary therefore shared one context/registry;
    // its supported reloads replace face data in place, never these pointers.
    // This device-only session keeps that contract and ends before APR/logging.
    struct SharedNativeFontSession : NativeFontUI
    {
        std::unique_ptr<ALMetalTexture> black;
        SharedNativeFontSession()
        {
            initRenderer();
            ALMetalTextureDescriptor desc;
            desc.width = WIDTH; desc.height = HEIGHT;
            desc.label = "native default-font test opaque black backing";
            std::string error;
            black = ALMetalTexture::create(*device, desc, &error);
            INFO(error); REQUIRE(black);
            std::vector<U8> pixels(static_cast<size_t>(WIDTH) * HEIGHT * 4, 0);
            for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
            auto command = device->makeCommandBuffer("default-font test backing", &error);
            REQUIRE(command);
            REQUIRE(black->upload(*command, 0, 0, {0, 0, WIDTH, HEIGHT},
                                  pixels.data(), WIDTH * 4, &error));
            REQUIRE(command->commitAndWait(&error));
        }
        ~SharedNativeFontSession()
        {
            if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
            LLFontGL::destroyDefaultFonts();
        }
        void clearFramebuffer()
        {
            if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
            REQUIRE(gGL.beginMetalUITextureCapture(
                ALMetalPixelFormat::rgba8_unorm, &gUIProgram, black.get(),
                nullptr, nullptr, LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
            gGL.setSceneBlendType(LLRender::BT_ALPHA);
        }
        std::vector<U8> readFramebufferRGBA(S32 width, S32 height)
        {
            REQUIRE(width == WIDTH); REQUIRE(height == HEIGHT);
            std::vector<U8> pixels(static_cast<size_t>(width) * height * 4);
            REQUIRE(gGL.readMetalUITextureCaptureRGBA(
                0, 0, width, height, pixels.data(), width * 4));
            return pixels;
        }
        void finishCase(bool require_draw)
        {
            std::string error;
            LLRender::EMetalUICaptureEndState state;
            auto output = gGL.endMetalUITextureCapture(true, false, nullptr, &error, &state);
            INFO(error);
            if (require_draw || state != LLRender::EMetalUICaptureEndState::EMPTY)
            {
                CHECK(state == LLRender::EMetalUICaptureEndState::COMPLETE);
                CHECK(output);
            }
            // The old shared-context guard allowed its registry-owned atlas
            // pages to grow between cases. Darwin otherwise counts every live
            // image regardless of device lifetime. Accept only that one count;
            // the terminal teardown below still requires exact release.
            StateGuard::accept("GL images outliving their context");
        }
    };

    inline SharedNativeFontSession& sharedNativeFontSession()
    {
        static SharedNativeFontSession* session = []
        {
            const size_t images_before = LLImageGL::sImageList.size();
            auto* made = new SharedNativeFontSession;
            Teardown::add([made, images_before]
            {
                std::string error;
                const bool idle = made->shutdownRenderer(&error);
                delete made;
                if (!idle) throw std::runtime_error("shared native font wait failed: " + error);
                if (LLImageGL::sImageList.size() != images_before)
                    throw std::runtime_error("shared native font session leaked LLImageGL owners");
            });
            StateGuard::accept();
            return made;
        }();
        return *session;
    }

    struct SharedNativeFontCase
    {
        SharedNativeFontSession& gl = sharedNativeFontSession();
        bool require_draw;
        explicit SharedNativeFontCase(bool draw = false) : require_draw(draw)
        { gl.clearFramebuffer(); }
        ~SharedNativeFontCase()
        {
            // Preserve the original optional font/fallback SKIPs. No draw was
            // promised when the body explicitly reported unavailable assets.
            const auto* result = Catch::getResultCapture().getLastResult();
            const bool skipped = result &&
                result->getResultType() == Catch::ResultWas::ExplicitSkip;
            gl.finishCase(require_draw && !skipped);
        }
    };

}
#endif
