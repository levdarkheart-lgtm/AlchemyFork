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
#include "../llgl.h"
#include "../llglslshader.h"
#include "../llimagegl.h"
#include "../llrender.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalframe.h"
#include "../../almetal/almetalresource.h"
#include <catch2/catch_test_macros.hpp>
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
        ~NativeFontUI()
        {
            if (!initialized) return;
            if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
            if (LLGLSLShader::sCurBoundShaderPtr) LLGLSLShader::sCurBoundShaderPtr->unbind();
            if (matrices)
                for (auto mode : {LLRender::MM_TEXTURE0, LLRender::MM_PROJECTION, LLRender::MM_MODELVIEW})
                { gGL.matrixMode(mode); gGL.popMatrix(); }
            gGL.matrixMode(old_mode);
            std::string error; CHECK(frames->waitUntilIdle(5000, &error));
            gGL.shutdown(); gGL.setMetalBackend(nullptr, nullptr, nullptr);
            LLVertexBuffer::cleanupClass();
            LLVertexBuffer::setMetalBufferAuthority(old_authority);
            LLVertexBuffer::setMetalDevice(NativeFont::old_device);
            gUIProgram.mAttributeMask = old_ui_mask;
            LLGLSLShader::sCurBoundShader = old_shader_id;
            LLGLSLShader::sCurBoundShaderPtr = old_shader;
            LLRender::sDrawCalls = old_calls; LLRender::sDrawElements = old_elements;
        }
    };

}
#endif
