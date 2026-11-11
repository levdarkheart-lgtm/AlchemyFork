/**
 * @file alvertex_draw_metal_test.cpp
 * @brief Native vertex entry points retain logical UI state without GL flags.
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
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
constexpr U32 side = 32;
using Pixels = std::vector<U8>;
struct Fixture
{
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalFrameScheduler> frames;
    LLGLSLShader alpha, uniform, unsupported;
    ALRenderCapabilities old_capabilities = gRenderCapabilities;
    ALMetalDevice* old_image_device = LLImageGL::getMetalDevice();
    bool old_image_authority = LLImageGL::isMetalTextureAuthority();
    bool old_authority = LLVertexBuffer::isMetalBufferAuthority();
    bool old_apple = gGLManager.mIsApple;
    bool old_debug = gDebugGL;
    LLGLSLShader* old_shader = LLGLSLShader::sCurBoundShaderPtr;
    U32 old_program = LLGLSLShader::sCurBoundShader;
    U32 old_calls = LLRender::sDrawCalls;
    U64 old_elements = LLRender::sDrawElements;
    LLRender::eMatrixMode old_mode = gGL.getMatrixMode();
    bool initialized = false, matrices = false;
    std::string error;

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
        alpha.mName = "Vertex draw alpha-mask regression";
        alpha.mShaderFiles = {{"interface/alphamaskV.glsl", GL_VERTEX_SHADER},
                             {"interface/alphamaskF.glsl", GL_FRAGMENT_SHADER}};
        alpha.setNativeAttributeMask(LLVertexBuffer::MAP_VERTEX |
            LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR);
        REQUIRE(alpha.createShader());
        for (auto* shader : {&uniform, &unsupported})
        {
            shader->mName = "Vertex draw uniform-colour regression";
            shader->mShaderFiles = {{"interface/debugV.glsl", GL_VERTEX_SHADER},
                                   {"interface/debugF.glsl", GL_FRAGMENT_SHADER}};
            shader->setNativeAttributeMask(LLVertexBuffer::MAP_VERTEX);
            REQUIRE(shader->createShader());
        }
        // These old GL migration hints must not decide whether native draws
        // or teardown work. Initial native setup alone requires authority.
        LLVertexBuffer::setMetalBufferAuthority(false);
        gGLManager.mIsApple = false;
        gDebugGL = false;
    }
    ~Fixture()
    {
        if (!initialized) return;
        if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
        if (LLGLSLShader::sCurBoundShaderPtr) LLGLSLShader::sCurBoundShaderPtr->unbind();
        if (matrices)
            for (auto mode : {LLRender::MM_TEXTURE0, LLRender::MM_PROJECTION, LLRender::MM_MODELVIEW})
            { gGL.matrixMode(mode); gGL.popMatrix(); }
        gGL.matrixMode(old_mode);
        CHECK(frames->waitUntilIdle(5000, &error));
        gGL.shutdown(); gGL.setMetalBackend(nullptr, nullptr, nullptr);
        LLVertexBuffer::cleanupClass();
        LLVertexBuffer::setMetalBufferAuthority(old_authority);
        LLVertexBuffer::setMetalDevice(old_image_device);
        LLImageGL::setMetalDevice(old_image_device);
        LLImageGL::setMetalTextureAuthority(old_image_authority);
        gGLManager.mIsApple = old_apple; gDebugGL = old_debug;
        gRenderCapabilities = old_capabilities;
        LLGLSLShader::sCurBoundShader = old_program;
        LLGLSLShader::sCurBoundShaderPtr = old_shader;
        LLRender::sDrawCalls = old_calls; LLRender::sDrawElements = old_elements;
    }
    LLPointer<LLVertexBuffer> buffer(bool wide_indices = false)
    {
        LLPointer<LLVertexBuffer> result = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX |
            LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR);
        REQUIRE(result->allocateBuffer(9, wide_indices ? 18 : 9));
        std::memset(result->getMappedData(), 0, result->getSize());
        std::array<LLVector4a, 9> positions;
        std::array<LLVector2, 9> uv;
        std::array<LLColor4U, 9> colors;
        for (U32 i = 0; i < 9; ++i)
        {
            positions[i].set(8.f, 8.f, 0.f, 1.f);
            uv[i].set(-.5f, .5f); colors[i].set(64, 255, 0, 64);
        }
        for (U32 i = 3; i < 6; ++i)
        { uv[i].set(.25f, .5f); colors[i].set(255, 128, 255, 255); }
        positions[3].set(-1.f, -1.f, 0.f, 1.f);
        positions[4].set(3.f, -1.f, 0.f, 1.f);
        positions[5].set(-1.f, 3.f, 0.f, 1.f);
        result->setPositionData(positions.data()); result->setTexCoord0Data(uv.data());
        result->setColorData(colors.data());
        const std::array<U16, 9> indices{0, 0, 0, 3, 4, 5, 8, 8, 8};
        if (wide_indices)
        {
            const std::array<U32, 9> wide{0, 0, 0, 3, 4, 5, 8, 8, 8};
            result->setIndexData(wide.data());
        }
        else result->setIndexData(indices.data());
        REQUIRE(result->getNumIndices() == 9);
        REQUIRE(result->getIndexStride() == (wide_indices ? sizeof(U32) : sizeof(U16)));
        result->setBuffer(); // Real lazy publication with both old flags false.
        REQUIRE(result->getMetalVertexBuffer()); REQUIRE(result->getMetalIndexBuffer());
        REQUIRE(result->hasPublishedMetalVertexBuffer());
        REQUIRE(result->hasPublishedMetalIndexBuffer());
        return result;
    }
    LLPointer<LLImageGL> image()
    {
        LLPointer<LLImageRaw> raw = new LLImageRaw(2, 1, 4);
        const U8 pixels[]{255,0,0,128, 0,255,0,128};
        std::memcpy(raw->getData(), pixels, sizeof(pixels));
        LLPointer<LLImageGL> result = new LLImageGL(raw, false);
        REQUIRE(result->getMetalTexture()); return result;
    }
    void begin(LLImageGL* image, bool authoritative = false, F32 cutoff = .4f)
    {
        REQUIRE(gGL.beginMetalUITextureCapture(ALMetalPixelFormat::rgba8_unorm,
            &alpha, nullptr, nullptr, &uniform, authoritative
                ? LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE
                : LLRender::EMetalUICaptureMode::GL_MIRROR));
        gGL.setViewport(0, 0, side, side);
        gGL.matrixMode(LLRender::MM_MODELVIEW); gGL.loadIdentity(); gGL.translatef(.5f, 0.f, 0.f);
        gGL.matrixMode(LLRender::MM_TEXTURE0); gGL.loadIdentity(); gGL.translatef(.5f, 0.f, 0.f);
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        alpha.bind(); alpha.setMinimumAlpha(cutoff);
        REQUIRE(gGL.getTextureSlot(0)->bindSampled(image, ALSamplers::PointClamp));
    }
    Pixels finish(bool complete = true)
    {
        gGL.flush();
        LLRender::EMetalUICaptureEndState state{}; std::string failure;
        auto output = gGL.endMetalUITextureCapture(true, false, nullptr, &failure, &state);
        INFO(failure);
        if (!complete)
        {
            CHECK_FALSE(output); CHECK(state == LLRender::EMetalUICaptureEndState::CAPTURE_INCOMPLETE);
            CHECK_FALSE(failure.empty()); return {};
        }
        REQUIRE(output); REQUIRE(state == LLRender::EMetalUICaptureEndState::COMPLETE);
        Pixels pixels(side * side * 4);
        REQUIRE(output->readback(*device, 0, 0, {}, pixels.data(), side * 4, &error));
        return pixels;
    }
    static void check(const Pixels& pixels, bool discarded = false, int green = 128)
    {
        for (U32 y = 0; y < side; ++y) for (U32 x = 0; x < side; ++x)
        {
            const auto i = (y * side + x) * 4;
            CHECK(pixels[i] == 0); CHECK(pixels[i+2] == 0);
            const bool clear = x < side / 4 || discarded;
            CHECK(std::abs(int(pixels[i+1]) - (clear ? 0 : green)) <= 1);
            CHECK(std::abs(int(pixels[i+3]) - (clear ? 0 : 128)) <= 1);
        }
    }
};
}

TEST_CASE("Native vertex draws preserve offsets, sampled state and capture ownership with GL flags false",
          "[llvertexbuffer][almetal][native-draw]")
{
    Fixture f; f.init();
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    auto image = f.image();
    for (bool authority : {false, true}) for (U32 entry = 0; entry < 4; ++entry)
    {
        auto buffer = f.buffer(entry == 1);
        f.begin(image, authority);
        const auto calls = LLRender::sDrawCalls; const auto elements = LLRender::sDrawElements;
        if (entry == 0) buffer->drawRange(LLRender::TRIANGLES, 3, 5, 3, 3);
        if (entry == 1) buffer->drawRangeFast(LLRender::TRIANGLES, 3, 5, 3, 3);
        if (entry == 2) buffer->draw(LLRender::TRIANGLES, 3, 3);
        if (entry == 3) buffer->drawArrays(LLRender::TRIANGLES, 3, 3);
        CHECK(LLRender::sDrawCalls == calls + 1); CHECK(LLRender::sDrawElements == elements + 3);
        // The command is still uncommitted when its original buffer owner dies.
        buffer = nullptr; LLVertexBuffer::unbind();
        Fixture::check(f.finish());
    }
    f.begin(image, false, .6f);
    auto buffer = f.buffer(); buffer->drawRange(LLRender::TRIANGLES, 3, 5, 3, 3);
    gGL.getTextureSlot(0)->unbind();
    image = nullptr; buffer = nullptr;
    Fixture::check(f.finish(), true);
}

TEST_CASE("Native vertex entry points fail UI ranges closed without replaying typed world draws",
          "[llvertexbuffer][almetal][native-draw]")
{
    Fixture f; f.init();
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    auto image = f.image(); auto buffer = f.buffer();
    REQUIRE_FALSE(gGL.hasMetalUITextureCapture());
    const auto submissions = ALMetalDevice::committedCommandBufferCount();
    const auto calls = LLRender::sDrawCalls; const auto elements = LLRender::sDrawElements;
    buffer->drawRange(LLRender::TRIANGLES, 3, 5, 3, 3);
    buffer->drawArrays(LLRender::TRIANGLES, 3, 3);
    CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);
    CHECK(LLRender::sDrawCalls == calls); CHECK(LLRender::sDrawElements == elements);
    constexpr U32 maximum = std::numeric_limits<U32>::max();
    for (U32 bad = 0; bad < 7; ++bad)
    {
        f.begin(image, bad % 2 != 0);
        if (bad == 0) buffer->drawRange(LLRender::TRIANGLES, 5, 3, 3, 3);
        if (bad == 1) buffer->drawRange(LLRender::TRIANGLES, 3, 9, 3, 3);
        if (bad == 2) buffer->drawRange(LLRender::TRIANGLES, 3, 5, maximum, 3);
        if (bad == 3) buffer->drawRangeFast(LLRender::TRIANGLES, 3, 5, 3, maximum);
        if (bad == 4) buffer->drawArrays(LLRender::TRIANGLES, maximum, 3);
        if (bad == 5) buffer->drawArrays(LLRender::PATCHES, 3, 3);
        if (bad == 6)
        { f.unsupported.bind(); buffer->drawRange(LLRender::TRIANGLES, 3, 5, 3, 3); }
        CHECK(LLRender::sDrawCalls == calls); CHECK(LLRender::sDrawElements == elements);
        f.finish(false);
    }
    CHECK(buffer->validateRange(3, 5, 3, 3));
    CHECK_FALSE(buffer->validateRange(3, 5, maximum, 3));
    CHECK_FALSE(buffer->validateRange(3, 5, 3, maximum));
    CHECK_FALSE(buffer->validateRange(5, 3, 3, 3));
    LLVertexBuffer::setMetalDevice(nullptr);
    buffer->setBuffer(); LLVertexBuffer::unbind();
    CHECK_FALSE(gGLManager.mInited);
    CHECK_FALSE(LLVertexBuffer::isMetalBufferAuthority());
    LLVertexBuffer::setMetalDevice(f.device.get());
}

TEST_CASE("Native direct vertex draws drain earlier pending immediate geometry",
          "[llvertexbuffer][almetal][native-draw]")
{
    Fixture f; f.init();
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    auto image = f.image(); auto buffer = f.buffer();
    for (U32 entry = 0; entry < 4; ++entry)
    {
        f.begin(image, false);
        // Both draws share every binding and matrix. The earlier immediate
        // triangle has white vertex colour (green255 after texturing); the
        // later buffer has green128. No intervening state change can flush
        // the three vertices retained by LLRender::end for TRIANGLES.
        gGL.color4ub(255, 255, 255, 255);
        gGL.begin(LLRender::TRIANGLES);
        gGL.texCoord2f(.25f, .5f); gGL.vertex3f(-1.f, -1.f, 0.f);
        gGL.texCoord2f(.25f, .5f); gGL.vertex3f(3.f, -1.f, 0.f);
        gGL.texCoord2f(.25f, .5f); gGL.vertex3f(-1.f, 3.f, 0.f);
        gGL.end();
        if (entry == 0) buffer->drawRange(LLRender::TRIANGLES, 3, 5, 3, 3);
        if (entry == 1) buffer->drawRangeFast(LLRender::TRIANGLES, 3, 5, 3, 3);
        if (entry == 2) buffer->draw(LLRender::TRIANGLES, 3, 3);
        if (entry == 3) buffer->drawArrays(LLRender::TRIANGLES, 3, 3);
        Fixture::check(f.finish());
    }
}

TEST_CASE("Native UI admits only current published vertex and required index stores",
          "[llvertexbuffer][almetal][native-draw]")
{
    Fixture f; f.init();
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    auto image = f.image();
    struct MissingMetalDevice
    {
        ALMetalDevice* device;
        explicit MissingMetalDevice(ALMetalDevice* value) : device(value)
        { LLVertexBuffer::setMetalDevice(nullptr); }
        ~MissingMetalDevice() { LLVertexBuffer::setMetalDevice(device); }
    };

    SECTION("allocated but never published resources cannot draw")
    {
        LLPointer<LLVertexBuffer> buffer = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX |
            LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR);
        REQUIRE(buffer->allocateBuffer(9, 9));
        REQUIRE(buffer->getMetalVertexBuffer()); REQUIRE(buffer->getMetalIndexBuffer());
        CHECK_FALSE(buffer->hasPublishedMetalVertexBuffer());
        CHECK_FALSE(buffer->hasPublishedMetalIndexBuffer());
        f.begin(image, true);
        const auto calls = LLRender::sDrawCalls;
        buffer->setBuffer(); buffer->drawRange(LLRender::TRIANGLES, 3, 5, 3, 3);
        CHECK(LLRender::sDrawCalls == calls); f.finish(false);
    }
    SECTION("failed vertex update withholds stale bytes and retry preserves earlier GPU leases")
    {
        auto buffer = f.buffer();
        auto* old_vertices = buffer->getMetalVertexBuffer();
        std::vector<U8> previous(old_vertices->length());
        REQUIRE(old_vertices->read(0, previous.data(), previous.size(), &f.error));
        ALMetalBufferDescriptor d; d.length = previous.size(); d.storage = ALMetalStorageMode::shared;
        auto lease = ALMetalBuffer::create(*f.device, d, &f.error); REQUIRE(lease);
        auto command = f.device->makeCommandBuffer("Earlier published vertex lease", &f.error);
        REQUIRE(command); REQUIRE(lease->copyFrom(*command, *old_vertices, 0, 0, previous.size(), &f.error));
        std::array<LLColor4U, 3> colors;
        for (auto& color : colors) color.set(255, 64, 255, 255);
        f.begin(image, false);
        {
            MissingMetalDevice missing(f.device.get());
            buffer->setColorData(colors.data(), 3, 3); buffer->setBuffer();
            CHECK(buffer->getMetalVertexBuffer() == old_vertices);
            CHECK_FALSE(buffer->hasPublishedMetalVertexBuffer());
            CHECK(buffer->hasPublishedMetalIndexBuffer());
            const auto calls = LLRender::sDrawCalls;
            buffer->drawRange(LLRender::TRIANGLES, 3, 5, 3, 3);
            CHECK(LLRender::sDrawCalls == calls); f.finish(false);
        }
        LLVertexBuffer::flushBuffers();
        REQUIRE(buffer->hasPublishedMetalVertexBuffer());
        REQUIRE(buffer->hasPublishedMetalIndexBuffer());
        f.begin(image, true); buffer->drawRange(LLRender::TRIANGLES, 3, 5, 3, 3);
        Fixture::check(f.finish(), false, 64);
        buffer = nullptr;
        // Commit only after the old wrapper owner and replacement generation
        // have gone. Encoded native resources still belong to this command.
        REQUIRE(command->commitAndWait(&f.error));
        std::vector<U8> retained(previous.size());
        REQUIRE(lease->read(0, retained.data(), retained.size(), &f.error));
        CHECK(retained == previous);
    }
    SECTION("an unrelated failed index update does not withhold clean array draws")
    {
        auto buffer = f.buffer();
        auto* old_indices = buffer->getMetalIndexBuffer();
        const U16 unchanged = 3;
        {
            MissingMetalDevice missing(f.device.get());
            buffer->setIndexData(&unchanged, 3, 1); buffer->setBuffer();
            CHECK(buffer->hasPublishedMetalVertexBuffer());
            CHECK_FALSE(buffer->hasPublishedMetalIndexBuffer());
            CHECK(buffer->getMetalIndexBuffer() == old_indices);
            f.begin(image, false); buffer->drawArrays(LLRender::TRIANGLES, 3, 3);
            Fixture::check(f.finish());
            f.begin(image, true);
            const auto calls = LLRender::sDrawCalls;
            buffer->drawRangeFast(LLRender::TRIANGLES, 3, 5, 3, 3);
            CHECK(LLRender::sDrawCalls == calls); f.finish(false);
        }
        LLVertexBuffer::flushBuffers();
        REQUIRE(buffer->hasPublishedMetalIndexBuffer());
        f.begin(image, true); buffer->drawRange(LLRender::TRIANGLES, 3, 5, 3, 3);
        Fixture::check(f.finish());
    }
}

TEST_CASE("Retained native UI replays the original final buffer publication retry",
          "[llvertexbuffer][almetal][native-draw-retry]")
{
    Fixture f; f.init();
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    auto image = f.image();
    auto buffer = f.buffer();
    f.begin(image, true);
    const ALSamplerDesc sampler = ALSamplerCache::makeDesc(ALSamplers::PointClamp);
    LLVertexBufferData retained(buffer, LLRender::TRIANGLES, buffer->getNumVerts(),
        image, true, sampler, gGL.getModelviewMatrix(), gGL.getProjectionMatrix(),
        gGL.getTextureMatrix());
    bool with_matrix = false;
    SECTION("draw retries before taking the native generation") {}
    SECTION("drawWithMatrix retries before taking the native generation")
    { with_matrix = true; }
    const auto replay = [&]
    {
        if (with_matrix) retained.drawWithMatrix();
        else retained.draw();
    };
    replay();
    const auto previous_output = f.finish();
    Fixture::check(previous_output);
    auto* previous_vertices = buffer->getMetalVertexBuffer();
    auto* previous_indices = buffer->getMetalIndexBuffer();
    U64 pending_stamp = 0;
    {
        struct MissingPublisher
        {
            ALMetalDevice* device;
            explicit MissingPublisher(ALMetalDevice* d) : device(d)
            { LLVertexBuffer::setMetalDevice(nullptr); }
            ~MissingPublisher() { LLVertexBuffer::setMetalDevice(device); }
        } missing(f.device.get());
        std::array<LLColor4U, 3> colors;
        for (auto& color : colors) color.set(255, 64, 255, 255);
        buffer->setColorData(colors.data(), 3, 3);
        LLVertexBuffer::flushBuffers();
        LLVertexBuffer::flushBuffers();
        pending_stamp = buffer->getContentStamp();
        CHECK_FALSE(buffer->hasPublishedMetalVertexBuffer());
        CHECK(buffer->hasPublishedMetalIndexBuffer());
        CHECK(buffer->getMetalVertexBuffer() == previous_vertices);
        f.begin(image, true); replay(); f.finish(false);
    }
    // No new CPU write or global flush happens after restoring the publisher.
    // Original LLVertexBufferData called setBuffer immediately before drawing.
    f.begin(image, true); replay();
    const auto updated_output = f.finish();
    Fixture::check(updated_output, false, 64);
    CHECK(updated_output != previous_output);
    CHECK(buffer->hasPublishedMetalVertexBuffer());
    CHECK(buffer->getContentStamp() == pending_stamp);
    CHECK(buffer->getMetalIndexBuffer() == previous_indices);
}
