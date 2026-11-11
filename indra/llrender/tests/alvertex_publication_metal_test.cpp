/**
 * @file alvertex_publication_metal_test.cpp
 * @brief Published vertex stores and content identities survive real draws.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llgl.h"
#include "../llglslshader.h"
#include "../llimagegl.h"
#include "../llrender.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetalframe.h"
#include "../../almetal/almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace
{
constexpr U32 side = 16;
constexpr U32 mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 |
                     LLVertexBuffer::MAP_COLOR;
const LLColor4U red(255, 0, 0, 255), green(0, 255, 0, 255), blue(0, 0, 255, 255);

struct PublicationFixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalFrameScheduler> frames;
    LLGLSLShader alpha, uniform;
    LLPointer<LLImageGL> white;
    ALRenderCapabilities old_capabilities = gRenderCapabilities;
    ALMetalDevice* old_device = LLImageGL::getMetalDevice();
    bool old_image_authority = LLImageGL::isMetalTextureAuthority();
    bool old_vertex_authority = LLVertexBuffer::isMetalBufferAuthority();
    LLGLSLShader* old_shader = LLGLSLShader::sCurBoundShaderPtr;
    U32 old_shader_id = LLGLSLShader::sCurBoundShader;
    U32 old_calls = LLRender::sDrawCalls;
    U64 old_elements = LLRender::sDrawElements;
    LLRender::eMatrixMode old_mode = gGL.getMatrixMode();
    bool initialized = false, matrices = false;

    void init()
    {
        REQUIRE_FALSE(gGLManager.mInited);
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        frames = ALMetalFrameScheduler::create(*device, 2, &error); REQUIRE(frames);
        gRenderCapabilities.initializeMetal(device->info());
        LLImageGL::setMetalDevice(device.get());
        LLImageGL::setMetalTextureAuthority(true);
        LLVertexBuffer::setMetalDevice(device.get());
        LLVertexBuffer::setMetalBufferAuthority(true);
        LLVertexBuffer::initClass(nullptr);
        gGL.setMetalBackend(device.get(), frames.get(), nullptr);
        initialized = true;
        REQUIRE(gGL.initMetal(true));
        for (auto mode : {LLRender::MM_MODELVIEW, LLRender::MM_PROJECTION, LLRender::MM_TEXTURE0})
        { gGL.matrixMode(mode); gGL.pushMatrix(); gGL.loadIdentity(); }
        matrices = true;
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        alpha.mName = "Native vertex publication pixel oracle";
        alpha.mShaderFiles = {{"interface/alphamaskV.glsl", GL_VERTEX_SHADER},
                              {"interface/alphamaskF.glsl", GL_FRAGMENT_SHADER}};
        alpha.setNativeAttributeMask(mask); REQUIRE(alpha.createShader());
        uniform.mName = "Native publication capture uniform colour";
        uniform.mShaderFiles = {{"interface/debugV.glsl", GL_VERTEX_SHADER},
                                {"interface/debugF.glsl", GL_FRAGMENT_SHADER}};
        uniform.setNativeAttributeMask(LLVertexBuffer::MAP_VERTEX);
        REQUIRE(uniform.createShader());
        LLPointer<LLImageRaw> raw = new LLImageRaw(1, 1, 4);
        std::memset(raw->getData(), 255, 4);
        white = new LLImageGL(raw, false); REQUIRE(white->getMetalTexture());
    }
    ~PublicationFixture()
    {
        if (!initialized) return;
        if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
        if (LLGLSLShader::sCurBoundShaderPtr) LLGLSLShader::sCurBoundShaderPtr->unbind();
        if (matrices)
            for (auto mode : {LLRender::MM_TEXTURE0, LLRender::MM_PROJECTION, LLRender::MM_MODELVIEW})
            { gGL.matrixMode(mode); gGL.popMatrix(); }
        gGL.matrixMode(old_mode);
        CHECK(frames->waitUntilIdle(5000, &error));
        white = nullptr;
        gGL.shutdown(); gGL.setMetalBackend(nullptr, nullptr, nullptr);
        LLVertexBuffer::cleanupClass();
        LLVertexBuffer::setMetalBufferAuthority(old_vertex_authority);
        LLVertexBuffer::setMetalDevice(old_device);
        LLImageGL::setMetalDevice(old_device);
        LLImageGL::setMetalTextureAuthority(old_image_authority);
        gRenderCapabilities = old_capabilities;
        LLGLSLShader::sCurBoundShader = old_shader_id;
        LLGLSLShader::sCurBoundShaderPtr = old_shader;
        LLRender::sDrawCalls = old_calls; LLRender::sDrawElements = old_elements;
    }
    static LLPointer<LLVertexBuffer> buffer(U32 vertices = 4, U32 indices = 6)
    {
        LLPointer<LLVertexBuffer> result = new LLVertexBuffer(mask);
        REQUIRE(result->allocateBuffer(vertices, indices)); return result;
    }
    static void colours(LLVertexBuffer& buffer, U32 first, const LLColor4U& colour)
    {
        LLStrider<LLColor4U> output; REQUIRE(buffer.getColorStrider(output, first, 4));
        for (U32 i = 0; i < 4; ++i) output[i] = colour;
    }
    static void quad(LLVertexBuffer& buffer, U32 first, float left, float right,
                     const LLColor4U& colour)
    {
        LLStrider<LLVector3> position; REQUIRE(buffer.getVertexStrider(position, first, 4));
        position[0].set(left, -1.f, 0.f); position[1].set(right, -1.f, 0.f);
        position[2].set(left, 1.f, 0.f); position[3].set(right, 1.f, 0.f);
        LLStrider<LLVector2> uv; REQUIRE(buffer.getTexCoord0Strider(uv, first, 4));
        for (U32 i = 0; i < 4; ++i) uv[i].set(.5f, .5f);
        colours(buffer, first, colour);
    }
    static void indices(LLVertexBuffer& buffer, U32 offset, U16 first)
    {
        LLStrider<U16> output; REQUIRE(buffer.getIndexStrider(output, offset, 6));
        const std::array<U16, 6> quad{0, 1, 2, 2, 1, 3};
        for (U32 i = 0; i < quad.size(); ++i) output[i] = first + quad[i];
    }
    void begin()
    {
        REQUIRE(gGL.beginMetalUITextureCapture(ALMetalPixelFormat::rgba8_unorm,
            &alpha, nullptr, nullptr, &uniform,
            LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
        gGL.setViewport(0, 0, side, side);
        alpha.bind(); alpha.setMinimumAlpha(.01f);
        REQUIRE(gGL.getTextureSlot(0)->bindSampled(white, ALSamplers::PointClamp));
    }
    static void draw(LLVertexBuffer& buffer)
    {
        buffer.setBuffer();
        buffer.draw(LLRender::TRIANGLES, buffer.getNumIndices(), 0);
    }
    void finish(const LLColor4U& left, const LLColor4U& right)
    {
        gGL.flush();
        LLRender::EMetalUICaptureEndState state{};
        auto output = gGL.endMetalUITextureCapture(true, false, nullptr, &error, &state);
        INFO(error); REQUIRE(output);
        REQUIRE(state == LLRender::EMetalUICaptureEndState::COMPLETE);
        std::vector<U8> pixels(side * side * 4);
        REQUIRE(output->readback(*device, 0, 0, {}, pixels.data(), side * 4, &error));
        for (U32 y = 0; y < side; ++y) for (U32 x = 0; x < side; ++x)
        {
            const auto& expected = x < side / 2 ? left : right;
            for (U32 channel = 0; channel < 4; ++channel)
                CHECK(pixels[(y * side + x) * 4 + channel] == expected.mV[channel]);
        }
    }
};
}

TEST_CASE("Native vertex draws preserve interleaved uploads, selective stores and resized geometry",
          "[llvertexbuffer][almetal][native-publication]")
{
    PublicationFixture fixture; fixture.init();
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    const LLColor4U clear(0, 0, 0, 0);
    auto left = fixture.buffer();
    fixture.quad(*left, 0, -1.f, 0.f, red); fixture.indices(*left, 0, 0);
    left->unmapBuffer();
    fixture.begin(); fixture.draw(*left);
    // Create/upload the second store while the first draw is still encoded.
    auto right = fixture.buffer();
    fixture.quad(*right, 0, 0.f, 1.f, green); fixture.indices(*right, 0, 0);
    right->unmapBuffer(); fixture.draw(*right);
    fixture.finish(red, green);

    // The unmarked CPU index edit must not replace the published index store.
    auto* raw_indices = reinterpret_cast<U16*>(right->getMappedIndices());
    std::fill(raw_indices, raw_indices + 6, U16(0));
    fixture.colours(*right, 0, blue); right->unmapBuffer();
    fixture.begin(); fixture.draw(*right); fixture.finish(clear, blue);

    // Conversely, index-only publication must not upload raw CPU colours.
    auto* raw_colours = reinterpret_cast<LLColor4U*>(right->getMappedData() +
        right->getOffset(LLVertexBuffer::TYPE_COLOR));
    std::fill(raw_colours, raw_colours + 4, red);
    fixture.indices(*right, 0, 0); right->unmapBuffer();
    fixture.begin(); fixture.draw(*right); fixture.finish(clear, blue);

    REQUIRE(right->allocateBuffer(8, 12));
    CHECK_FALSE(right->hasPublishedMetalVertexBuffer());
    CHECK_FALSE(right->hasPublishedMetalIndexBuffer());
    fixture.quad(*right, 0, -1.f, 0.f, red);
    fixture.quad(*right, 4, 0.f, 1.f, blue);
    fixture.indices(*right, 0, 0); fixture.indices(*right, 6, 4);
    right->unmapBuffer();
    fixture.begin(); fixture.draw(*right); fixture.finish(red, blue);
}

TEST_CASE("Native vertex content stamps identify CPU writes and allocation lifetimes, not draws",
          "[llvertexbuffer][almetal][native-publication]")
{
    PublicationFixture fixture; fixture.init();
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    auto buffer = fixture.buffer(), other = fixture.buffer();
    REQUIRE(buffer->getContentStamp() != 0);
    CHECK(buffer->getContentStamp() != other->getContentStamp());
    fixture.quad(*buffer, 0, -1.f, 1.f, green); fixture.indices(*buffer, 0, 0);
    buffer->unmapBuffer();
    U64 stamp = buffer->getContentStamp();
    fixture.begin();
    const U32 calls = LLRender::sDrawCalls;
    fixture.draw(*buffer); fixture.draw(*buffer);
    CHECK(LLRender::sDrawCalls == calls + 2);
    CHECK(buffer->getContentStamp() == stamp);
    fixture.finish(green, green);
    fixture.colours(*buffer, 0, blue);
    CHECK(buffer->getContentStamp() > stamp); // Before publication.
    stamp = buffer->getContentStamp(); buffer->unmapBuffer();
    CHECK(buffer->getContentStamp() == stamp);
    LLStrider<U16> mapped; REQUIRE(buffer->getIndexStrider(mapped, 0, 6));
    CHECK(buffer->getContentStamp() > stamp);
    stamp = buffer->getContentStamp(); buffer->unmapBuffer();
    CHECK(buffer->getContentStamp() == stamp);
    std::array<LLVector4a, 4> positions;
    for (auto& position : positions) position.set(.5f, 0.f, 0.f, 1.f);
    buffer->setPositionData(positions.data());
    CHECK(buffer->getContentStamp() > stamp); stamp = buffer->getContentStamp();
    const std::array<U16, 6> indices{0, 1, 2, 2, 1, 3};
    buffer->setIndexData(indices.data());
    CHECK(buffer->getContentStamp() > stamp); stamp = buffer->getContentStamp();
    REQUIRE(buffer->allocateBuffer(8, 12));
    CHECK(buffer->getContentStamp() > stamp);
    U64 latest = std::max(buffer->getContentStamp(), other->getContentStamp());
    for (U32 replacement = 0; replacement < 4; ++replacement)
    {
        other = fixture.buffer();
        CHECK(other->getContentStamp() > latest); latest = other->getContentStamp();
    }
}
