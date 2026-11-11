/**
 * @file alrendersphere_metal_test.cpp
 * @brief Original sphere range invariant through native debug/UI captures.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * $/LicenseInfo$
 */
#include "llfontmetal_fixture.h"
#include "../llrendersphere.h"
#include <vector>

namespace
{
struct NativeSphere : ll_test::NativeFontUI
{
    static constexpr U32 side = 16;
    LLGLSLShader positions;
    NativeSphere()
    {
        initRenderer();
        positions.mName = "Native sphere position-only debug";
        positions.mShaderFiles = {
            {"interface/debugV.glsl", GL_VERTEX_SHADER},
            {"interface/debugF.glsl", GL_FRAGMENT_SHADER}};
        positions.setNativeAttributeMask(LLVertexBuffer::MAP_VERTEX);
        REQUIRE(positions.createShader());
        REQUIRE(positions.mAttributeMask == LLVertexBuffer::MAP_VERTEX);
        gGL.matrixMode(LLRender::MM_PROJECTION); gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW); gGL.loadIdentity();
        // Original test shader multiplies the unit sphere's positions by .5.
        gGL.scalef(.5f, .5f, .5f);
        gGL.getTextureSlot(0)->unbind();
    }
    ~NativeSphere()
    {
        if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
        LLGLSLShader::unbind();
        positions.unload();
    }
    std::vector<U8> draw(LLRenderSphere& sphere, LLGLSLShader& shader)
    {
        REQUIRE(gGL.beginMetalUITextureCapture(ALMetalPixelFormat::rgba8_unorm,
            &gUIProgram, nullptr, nullptr, &positions,
            LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
        gGL.setViewport(0, 0, side, side);
        shader.bind();
        gGL.color4f(1.f, 1.f, 1.f, 1.f);
        const U32 before_calls = LLRender::sDrawCalls;
        const U64 before_elements = LLRender::sDrawElements;
        CHECK_NOTHROW(sphere.renderGGL());
        gGL.flush();
        CHECK(LLRender::sDrawCalls - before_calls == (&shader == &positions ? 1u : 2u));
        CHECK(LLRender::sDrawElements - before_elements == 3600u);
        std::string error;
        LLRender::EMetalUICaptureEndState state{};
        auto output = gGL.endMetalUITextureCapture(true, false, nullptr, &error, &state);
        INFO(error);
        REQUIRE(state == LLRender::EMetalUICaptureEndState::COMPLETE);
        REQUIRE(output);
        std::vector<U8> pixels(side * side * 4);
        REQUIRE(output->readback(*device, 0, 0, {}, pixels.data(), side * 4, &error));
        INFO(error);
        CHECK(pixels[(side / 2 * side + side / 2) * 4] == 255);
        CHECK(pixels[0] == 0);
        return pixels;
    }
};
}

TEST_CASE_METHOD(NativeSphere,
    "Native sphere buffer stays in range and matches its immediate geometry",
    "[native-sphere]")
{
    LLRenderSphere sphere; // Releases its allocation before the native fixture.
    struct DebugScope
    {
        bool previous = gDebugGL;
        DebugScope() { gDebugGL = true; }
        ~DebugScope() { gDebugGL = previous; }
    } debug;
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    const auto buffered = draw(sphere, positions);
    const auto immediate = draw(sphere, gUIProgram);
    CHECK(buffered == immediate);
}
