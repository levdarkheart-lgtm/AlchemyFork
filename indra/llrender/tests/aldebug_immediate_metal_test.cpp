/**
 * @file aldebug_immediate_metal_test.cpp
 * @brief Native semantic Debug layouts reach real immediate UI capture.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llgl.h"
#include "../llglslshader.h"
#include "../llimagegl.h"
#include "../llrender.h"
#include "../llrender2dutils.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetalframe.h"
#include "../../almetal/almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <string>
#include <vector>

namespace
{
using Point = std::array<F32, 2>;
using Pixels = std::vector<U8>;
constexpr U32 side = 64;

struct NativeDebug
{
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalFrameScheduler> frames;
    LLGLSLShader debug, missing_layout;
    ALRenderCapabilities old_capabilities = gRenderCapabilities;
    ALMetalDevice* old_image_device = LLImageGL::getMetalDevice();
    bool old_authority = LLVertexBuffer::isMetalBufferAuthority();
    LLGLSLShader* old_shader = LLGLSLShader::sCurBoundShaderPtr;
    U32 old_program = LLGLSLShader::sCurBoundShader;
    LLRender::eMatrixMode old_mode = gGL.getMatrixMode();
    bool initialized = false;
    bool matrices_pushed = false;
    GLenum old_cull_face = gGL.getCullFace();
    std::string error;

    void init()
    {
        REQUIRE_FALSE(gGLManager.mInited);
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        frames = ALMetalFrameScheduler::create(*device, 2, &error);
        INFO(error); REQUIRE(frames);
        gRenderCapabilities.initializeMetal(device->info());
        LLImageGL::setMetalDevice(device.get());
        LLVertexBuffer::setMetalDevice(device.get());
        LLVertexBuffer::setMetalBufferAuthority(true);
        LLVertexBuffer::initClass(nullptr);
        gGL.setMetalBackend(device.get(), frames.get(), nullptr);
        initialized = true;
        REQUIRE(gGL.initMetal(true));
        for (auto mode : {LLRender::MM_MODELVIEW, LLRender::MM_PROJECTION,
                          LLRender::MM_TEXTURE0})
        {
            gGL.matrixMode(mode); gGL.pushMatrix(); gGL.loadIdentity();
        }
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        matrices_pushed = true;

        // Same semantic source and explicit native layout as the real Debug
        // loader. This test target cannot link the newview shader manager:
        // it tests configureNative -> immediate capture, not that callsite.
        configure(debug, true);
        configure(missing_layout, false);
        REQUIRE(debug.mAttributeMask == LLVertexBuffer::MAP_VERTEX);
        REQUIRE(missing_layout.mAttributeMask == 0);
    }

    static void configure(LLGLSLShader& shader, bool position_layout)
    {
        shader.mName = position_layout ? "Native Debug regression"
                                      : "Native missing Debug layout control";
        shader.mShaderLevel = 1;
        shader.mShaderFiles = {
            {"interface/debugV.glsl", GL_VERTEX_SHADER},
            {"interface/debugF.glsl", GL_FRAGMENT_SHADER}};
        if (position_layout)
            shader.setNativeAttributeMask(LLVertexBuffer::MAP_VERTEX);
        // Real native configuration supplies mAttributeMask. Assigning the
        // public result directly would skip the originally broken boundary.
        REQUIRE(shader.createShader());
        REQUIRE(shader.isAvailable());
    }

    ~NativeDebug()
    {
        if (!initialized) return;
        if (gGL.hasMetalUITextureCapture())
            gGL.endMetalUITextureCapture(false);
        if (LLGLSLShader::sCurBoundShaderPtr)
            LLGLSLShader::sCurBoundShaderPtr->unbind();
        if (matrices_pushed)
        {
            for (auto mode : {LLRender::MM_TEXTURE0, LLRender::MM_PROJECTION,
                              LLRender::MM_MODELVIEW})
            { gGL.matrixMode(mode); gGL.popMatrix(); }
        }
        gGL.matrixMode(old_mode);
        gGL.setCullFace(old_cull_face);
        CHECK(frames->waitUntilIdle(5000, &error)); // Test teardown only.
        gGL.shutdown();
        gGL.setMetalBackend(nullptr, nullptr, nullptr);
        LLVertexBuffer::cleanupClass();
        LLVertexBuffer::setMetalBufferAuthority(old_authority);
        LLVertexBuffer::setMetalDevice(old_image_device);
        LLImageGL::setMetalDevice(old_image_device);
        gRenderCapabilities = old_capabilities;
        LLGLSLShader::sCurBoundShader = old_program;
        LLGLSLShader::sCurBoundShaderPtr = old_shader;
    }

    void begin(LLGLSLShader& shader)
    {
        REQUIRE(gGL.beginMetalUITextureCapture(
            ALMetalPixelFormat::rgba8_unorm, &gUIProgram, nullptr, nullptr,
            &shader, LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
        gGL.setViewport(0, 0, side, side);
        shader.bind();
        gGL.getTextureSlot(0)->unbind();
    }

    Pixels finish(bool complete = true)
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
        Pixels pixels(side * side * 4);
        REQUIRE(output->readback(*device, 0, 0, {}, pixels.data(), side * 4, &error));
        INFO(error);
        return pixels;
    }
};

void triangle(const Point& a, const Point& b, const Point& c,
              const LLColor4& color)
{
    gGL.diffuseColor4fv(color.mV);
    gGL.begin(LLRender::TRIANGLES);
    for (const auto& p : {a, b, c}) gGL.vertex2f(p[0], p[1]);
    gGL.end();
    gGL.flush();
}

Pixels ring(NativeDebug& f, bool oracle)
{
    f.begin(f.debug);
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    const LLColor4 red(1.f, 0.f, 0.f, 1.f);
    gGL.diffuseColor4fv(red.mV);
    if (!oracle)
    {
        // Actual helper called by Rotate. Four steps give a diamond washer;
        // .7/.3 keep the edges away from pixel-center ties despite its
        // incremental sin/cos roundoff. The native source buffer has positions
        // only, so stale UV/colour streams cannot provide this result.
        gl_washer_2d(.7f, .3f, 4, red, red);
    }
    else
    {
        // Independent authored diamond corners, not helper-generated data or
        // the Metal fan/strip remapping implementation.
        const std::array<Point, 4> outer{{{.7f,0},{0,.7f},{-.7f,0},{0,-.7f}}};
        const std::array<Point, 4> inner{{{.3f,0},{0,.3f},{-.3f,0},{0,-.3f}}};
        for (size_t i=0; i<4; ++i)
        {
            const auto n=(i+1)%4;
            triangle(outer[i], inner[i], outer[n], red);
            triangle(outer[n], inner[i], inner[n], red);
        }
    }
    return f.finish();
}

Pixels fan(NativeDebug& f, bool oracle)
{
    f.begin(f.debug);
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    const LLColor4 green(0.f, 1.f, 0.f, 1.f);
    gGL.diffuseColor4fv(green.mV);
    if (!oracle) gl_circle_2d(0.f, 0.f, .63f, 4, true);
    else
    {
        const std::array<Point,4> corners{{{.63f,0},{0,.63f},{-.63f,0},{0,-.63f}}};
        for (size_t i=0; i<4; ++i)
            triangle({0,0}, corners[i], corners[(i+1)%4], green);
    }
    return f.finish();
}

Pixels winding(NativeDebug& f, GLenum cull_face, bool left, bool right)
{
    f.begin(f.debug);
    LLGLDisable blend(GL_BLEND);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    LLGLState cull(GL_CULL_FACE, cull_face != 0);
    if (cull_face) gGL.setCullFace(cull_face);
    if (left)
        triangle({-.9f,-.5f}, {-.1f,-.5f}, {-.5f,.5f}, LLColor4::red); // GL CCW.
    if (right)
        triangle({.1f,-.5f}, {.5f,.5f}, {.9f,-.5f}, LLColor4::green); // GL CW.
    return f.finish();
}

std::array<U8,4> pixel(const Pixels& p, U32 x, U32 y)
{
    const size_t i=(y*side+x)*4;
    return {p[i], p[i+1], p[i+2], p[i+3]};
}
}

TEST_CASE("Native Debug semantics preserve immediate Rotate geometry and culling",
          "[llrender][almetal][debug-immediate]")
{
    NativeDebug f; f.init();
    // No expected shader failure or GPU-invalid work: missing reflection
    // metadata fails the actual CPU admission and abandons the UI transaction.
    {
        f.begin(f.missing_layout);
        LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE);
        LLGLDepthTest depth(GL_FALSE, GL_FALSE);
        gGL.diffuseColor4f(1.f, 0.f, 0.f, 1.f);
        gl_circle_2d(0.f, 0.f, .63f, 4, true);
        f.finish(false);
    }
    const auto ring_pixels=ring(f, false), ring_oracle=ring(f, true);
    CHECK((ring_pixels == ring_oracle));
    CHECK((pixel(ring_pixels, 32, 32) == std::array<U8,4>{0,0,0,0})); // Hole.
    CHECK((pixel(ring_pixels, 48, 32) == std::array<U8,4>{255,0,0,255}));
    const auto fan_pixels=fan(f, false), fan_oracle=fan(f, true);
    CHECK((fan_pixels == fan_oracle));
    CHECK((pixel(fan_pixels, 32, 32) == std::array<U8,4>{0,255,0,255}));
    CHECK((ring_pixels != fan_pixels));

    const auto both=winding(f, 0, true, true);
    const auto only_left=winding(f, 0, true, false);
    const auto only_right=winding(f, 0, false, true);
    CHECK((pixel(both, 16, 32) == std::array<U8,4>{255,0,0,255}));
    CHECK((pixel(both, 48, 32) == std::array<U8,4>{0,255,0,255}));
    const auto back=winding(f, GL_BACK, true, true);
    const auto front=winding(f, GL_FRONT, true, true);
    CHECK((back == only_left));
    CHECK((front == only_right));
    CHECK((back != both));
    CHECK((front != both));
}
