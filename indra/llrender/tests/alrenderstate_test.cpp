/**
 * @file alrenderstate_test.cpp
 * @brief Logical renderer-state tests that require no OpenGL context.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#include "catch2/catch_test_macros.hpp"

#include "llgl.h"
#include "llglslshader.h"
#include "llglstates.h"
#include "llrender.h"

TEST_CASE("Renderer state remains semantic without an OpenGL context",
          "[alrenderstate][metal-cutover]")
{
    REQUIRE_FALSE(gGLManager.mInited);

    LLRender render;
    render.setLineWidth(3.f);
    render.setPolygonOffset(-2.f, -4.f);
    render.setPatchVertices(4);
    render.setPolygonMode(GL_LINE);
    render.setCullFace(GL_FRONT);
    render.setPointSize(5.f);
    render.setStencilFunc(GL_EQUAL, 7, 0x0fu);
    render.setStencilOp(GL_ZERO, GL_INCR, GL_REPLACE);
    render.setStencilMask(0xf0u);
    render.setBlendEquation(GL_FUNC_ADD, GL_MAX);
    render.setViewport(1, 2, 640, 480);
    render.setScissor(3, 4, 320, 240);
    render.setClearColor(LLColor4(0.25f, 0.5f, 0.75f, 1.f));

    CHECK(render.getLineWidth() == 3.f);
    CHECK(render.getPolygonOffsetFactor() == -2.f);
    CHECK(render.getPolygonOffsetUnits() == -4.f);
    CHECK(render.getPolygonMode() == GL_LINE);
    CHECK(render.getCullFace() == GL_FRONT);
    CHECK(render.getPointSize() == 5.f);
    CHECK(render.getStencilMask() == 0xf0u);
    CHECK(render.getClearColor() == LLColor4(0.25f, 0.5f, 0.75f, 1.f));

    U32 stencil_func = 0;
    S32 stencil_ref = 0;
    U32 stencil_read_mask = 0;
    render.getStencilFunc(stencil_func, stencil_ref, stencil_read_mask);
    CHECK(stencil_func == GL_EQUAL);
    CHECK(stencil_ref == 7);
    CHECK(stencil_read_mask == 0x0fu);

    U32 stencil_fail = 0;
    U32 depth_fail = 0;
    U32 stencil_pass = 0;
    render.getStencilOp(stencil_fail, depth_fail, stencil_pass);
    CHECK(stencil_fail == GL_ZERO);
    CHECK(depth_fail == GL_INCR);
    CHECK(stencil_pass == GL_REPLACE);

    U32 blend_rgb = 0;
    U32 blend_alpha = 0;
    render.getBlendEquation(blend_rgb, blend_alpha);
    CHECK(blend_rgb == GL_FUNC_ADD);
    CHECK(blend_alpha == GL_MAX);

    S32 viewport[4]{};
    render.getViewport(viewport);
    CHECK(viewport[0] == 1);
    CHECK(viewport[1] == 2);
    CHECK(viewport[2] == 640);
    CHECK(viewport[3] == 480);

    S32 scissor[4]{};
    render.getScissor(scissor);
    CHECK(scissor[0] == 3);
    CHECK(scissor[1] == 4);
    CHECK(scissor[2] == 320);
    CHECK(scissor[3] == 240);
}

TEST_CASE("Scoped render state remains balanced without an OpenGL context",
          "[alrenderstate][metal-cutover]")
{
    REQUIRE_FALSE(gGLManager.mInited);
    LLGLState::initClass();

    CHECK_FALSE(LLGLState::isEnabled(GL_BLEND));
    CHECK_FALSE(LLGLDepthTest::depthEnabled());
    CHECK(LLGLDepthTest::depthWriteEnabled());
    CHECK(LLGLDepthTest::depthFunction() == GL_LESS);

    {
        LLGLEnable blend(GL_BLEND);
        LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_GREATER);
        CHECK(LLGLState::isEnabled(GL_BLEND));
        CHECK(LLGLDepthTest::depthEnabled());
        CHECK_FALSE(LLGLDepthTest::depthWriteEnabled());
        CHECK(LLGLDepthTest::depthFunction() == GL_GREATER);
    }

    CHECK_FALSE(LLGLState::isEnabled(GL_BLEND));
    CHECK_FALSE(LLGLDepthTest::depthEnabled());
    CHECK(LLGLDepthTest::depthWriteEnabled());
    CHECK(LLGLDepthTest::depthFunction() == GL_LESS);
}

TEST_CASE("Sampler intent remains available without an OpenGL context",
          "[alrenderstate][metal-cutover]")
{
    REQUIRE_FALSE(gGLManager.mInited);

    struct SamplerReset
    {
        ~SamplerReset() { gGL.clearSamplers(); }
    } reset;

    gGL.clearSamplers();
    gGL.warmupSamplers();
    const U32 point = gGL.getSampler(ALSamplers::PointClamp);
    const U32 linear = gGL.getSampler(ALSamplers::BilinearClamp);
    CHECK(point != 0);
    CHECK(linear != 0);
    CHECK(point != linear);

    ALSamplerDesc custom = ALSamplerCache::makeDesc(ALSamplers::BilinearClamp);
    custom.mMagFilter = GL_NEAREST;
    const U32 custom_name = gGL.getSampler(custom);
    CHECK(custom_name != 0);
    CHECK(custom_name != linear);

    ALTextureSlot* slot = gGL.getTextureSlot(31);
    ALTextureSlot::sSamplerBinds = 0;
    ALTextureSlot::sSamplerSkips = 0;
    slot->bindSampler(point);
    slot->bindSampler(point);
    CHECK(ALTextureSlot::sSamplerBinds == 1);
    CHECK(ALTextureSlot::sSamplerSkips == 1);
    slot->bindSampler(0);
}

TEST_CASE("Shader binding remains logical without an OpenGL context",
          "[alrenderstate][metal-cutover]")
{
    REQUIRE_FALSE(gGLManager.mInited);

    LLGLSLShader shader;
    shader.bind();
    CHECK(LLGLSLShader::sCurBoundShader == 0);
    CHECK(LLGLSLShader::sCurBoundShaderPtr == &shader);

    LLGLSLShader::unbind();
    CHECK(LLGLSLShader::sCurBoundShader == 0);
    CHECK(LLGLSLShader::sCurBoundShaderPtr == nullptr);
}
