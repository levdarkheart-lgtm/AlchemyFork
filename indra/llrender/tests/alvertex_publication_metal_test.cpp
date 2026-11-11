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
#include "../../almetal/almetaluirenderer.h"
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

TEST_CASE("Native buffer preparation retries only the required current stores",
          "[llvertexbuffer][almetal][native-buffer-prepare]")
{
    PublicationFixture f; f.init();
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE);
    struct MissingPublisher
    {
        ALMetalDevice* device;
        explicit MissingPublisher(ALMetalDevice* value) : device(value)
        { LLVertexBuffer::setMetalDevice(nullptr); }
        ~MissingPublisher() { LLVertexBuffer::setMetalDevice(device); }
    };
    enum class Phase { first_store, first_backing, vertex, index, array };
    Phase phase = Phase::first_store;
    SECTION("first allocated resources have no published CPU data") {}
    SECTION("first native backing can be absent while CPU data is queued")
    { phase = Phase::first_backing; }
    SECTION("a failed warm vertex replacement cannot admit the old store")
    { phase = Phase::vertex; }
    SECTION("a failed warm index replacement cannot admit the old store")
    { phase = Phase::index; }
    SECTION("nonindexed draws ignore an unrelated failed index store")
    { phase = Phase::array; }
    auto clean = f.buffer();
    f.quad(*clean, 0, -1.f, 1.f, green); f.indices(*clean, 0, 0);
    clean->unmapBuffer(); REQUIRE(clean->prepareMetalBuffers(true));
    LLPointer<LLVertexBuffer> buffer = new LLVertexBuffer(mask);
    std::unique_ptr<MissingPublisher> missing;
    if (phase == Phase::first_backing)
        missing = std::make_unique<MissingPublisher>(f.device.get());
    REQUIRE(buffer->allocateBuffer(4, 6));
    if (phase == Phase::first_store)
    {
        REQUIRE(buffer->getMetalVertexBuffer());
        REQUIRE(buffer->getMetalIndexBuffer());
        CHECK_FALSE(buffer->prepareMetalBuffers(false));
        CHECK_FALSE(buffer->prepareMetalBuffers(true));
    }
    std::memset(buffer->getMappedData(), 0, buffer->getSize());
    std::memset(buffer->getMappedIndices(), 0, buffer->getIndicesSize());
    f.quad(*buffer, 0, -1.f, 1.f, red); f.indices(*buffer, 0, 0);
    const bool warm = phase == Phase::vertex || phase == Phase::index || phase == Phase::array;
    if (!warm && !missing)
        missing = std::make_unique<MissingPublisher>(f.device.get());
    buffer->unmapBuffer();
    auto* previous_vertices = buffer->getMetalVertexBuffer();
    auto* previous_indices = buffer->getMetalIndexBuffer();
    if (warm)
    {
        REQUIRE(buffer->prepareMetalBuffers(true));
        missing = std::make_unique<MissingPublisher>(f.device.get());
        if (phase == Phase::vertex) f.colours(*buffer, 0, green);
        else f.indices(*buffer, 0, 0);
        buffer->unmapBuffer();
    }
    const auto stamp = buffer->getContentStamp();
    const std::vector<U8> expected_vertices(buffer->getMappedData(), buffer->getMappedData()+buffer->getSize());
    const std::vector<U8> expected_indices(buffer->getMappedIndices(), buffer->getMappedIndices()+buffer->getIndicesSize());
    LLVertexBuffer::flushBuffers(); LLVertexBuffer::flushBuffers();
    CHECK_FALSE(buffer->prepareMetalBuffers(true));
    CHECK(buffer->prepareMetalBuffers(false) == (phase == Phase::index || phase == Phase::array));
    CHECK(buffer->getMetalVertexBuffer() == previous_vertices);
    CHECK(buffer->getMetalIndexBuffer() == previous_indices);
    // An unrelated failed store is not a veto on an already-current draw.
    auto* clean_vertices = clean->getMetalVertexBuffer();
    auto* clean_indices = clean->getMetalIndexBuffer();
    REQUIRE(clean->prepareMetalBuffers(true));
    CHECK(clean->getMetalVertexBuffer() == clean_vertices);
    CHECK(clean->getMetalIndexBuffer() == clean_indices);
    if (phase == Phase::array)
    {
        f.begin(); buffer->drawArrays(LLRender::TRIANGLE_STRIP, 0, 4); f.finish(red, red);
        CHECK_FALSE(buffer->hasPublishedMetalIndexBuffer());
    }
    missing.reset();
    REQUIRE(buffer->prepareMetalBuffers(true));
    CHECK(buffer->getContentStamp() == stamp);
    const auto read = [&f](const ALMetalBuffer* store)
    {
        REQUIRE(store); std::vector<U8> bytes(store->length());
        REQUIRE(store->read(0, bytes.data(), bytes.size(), &f.error)); return bytes;
    };
    CHECK(read(buffer->getMetalVertexBuffer()) == expected_vertices);
    CHECK(read(buffer->getMetalIndexBuffer()) == expected_indices);
    if (warm) CHECK((phase == Phase::vertex
        ? buffer->getMetalIndexBuffer() == previous_indices
        : buffer->getMetalVertexBuffer() == previous_vertices));
    const auto* current_vertices = buffer->getMetalVertexBuffer();
    const auto* current_indices = buffer->getMetalIndexBuffer();
    REQUIRE(buffer->prepareMetalBuffers(true));
    CHECK(buffer->getMetalVertexBuffer() == current_vertices);
    CHECK(buffer->getMetalIndexBuffer() == current_indices);
    f.begin(); f.draw(*buffer);
    const auto& expected_color = phase == Phase::vertex ? green : red;
    f.finish(expected_color, expected_color);
    auto array = f.buffer(4, 0);
    f.quad(*array, 0, -1.f, 1.f, blue);
    REQUIRE(array->prepareMetalBuffers(false));
    CHECK_FALSE(array->prepareMetalBuffers(true));
    CHECK_FALSE(array->getMetalIndexBuffer());
    f.begin(); array->drawArrays(LLRender::TRIANGLE_STRIP, 0, 4); f.finish(blue, blue);
}

TEST_CASE("Native handle capture must follow publication and preserve earlier encoded leases",
          "[llvertexbuffer][almetal][native-buffer-prepare]")
{
    PublicationFixture f; f.init();
    auto buffer = f.buffer();
    f.quad(*buffer, 0, -1.f, 1.f, red); f.indices(*buffer, 0, 0);
    buffer->unmapBuffer(); REQUIRE(buffer->prepareMetalBuffers(true));
    auto renderer = ALMetalUIRenderer::createForTexture(*f.device, *f.frames,
        ALMetalPixelFormat::rgba8_unorm, &f.error); REQUIRE(renderer);
    const auto capture = [&]
    {
        ALMetalUIBatch batch;
        batch.vertices = buffer->getMetalVertexBuffer();
        batch.position_offset = buffer->getOffset(LLVertexBuffer::TYPE_VERTEX);
        batch.texcoord_offset = buffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD0);
        batch.color_offset = buffer->getOffset(LLVertexBuffer::TYPE_COLOR);
        batch.vertex_count = 4;
        batch.indices = buffer->getMetalIndexBuffer(); batch.index_count = 6;
        batch.texture = f.white->getMetalTexture(); batch.sample_linear_storage = true;
        batch.blend.enabled = false; batch.target_width = batch.target_height = side;
        batch.viewport = {0,0,side,side,0,1}; batch.scissor = {0,0,side,side};
        for (U32 i=0;i<4;++i) batch.matrices.modelview_projection[i*4+i] = batch.matrices.texture0[i*4+i] = 1.f;
        return batch;
    };
    const auto draw = [&](const ALMetalUIBatch& batch, const LLColor4U& color)
    {
        REQUIRE(renderer->draw(batch,&f.error)); REQUIRE(renderer->endFrame(&f.error));
        std::vector<U8> pixels(side*side*4);
        REQUIRE(renderer->outputTexture()->readback(*f.device,0,0,{},pixels.data(),side*4,&f.error));
        for (U32 i=0;i<pixels.size();++i) CHECK(pixels[i]==color.mV[i%4]);
    };
    f.colours(*buffer, 0, green);
    const auto premature = capture(); // The former capture-before-setBuffer order.
    buffer->setBuffer();
    REQUIRE(buffer->hasPublishedMetalVertexBuffer());
    CHECK(premature.vertices != buffer->getMetalVertexBuffer());
    draw(premature, red); draw(capture(), green);

    auto* earlier = buffer->getMetalVertexBuffer();
    std::vector<U8> expected(earlier->length());
    REQUIRE(earlier->read(0,expected.data(),expected.size(),&f.error));
    ALMetalBufferDescriptor desc; desc.length=expected.size(); desc.storage=ALMetalStorageMode::shared;
    auto copy=ALMetalBuffer::create(*f.device,desc,&f.error);REQUIRE(copy);
    auto command=f.device->makeCommandBuffer("Earlier native buffer lease",&f.error);REQUIRE(command);
    REQUIRE(copy->copyFrom(*command,*earlier,0,0,expected.size(),&f.error));
    f.colours(*buffer, 0, blue);
    const auto stamp=buffer->getContentStamp();
    REQUIRE(buffer->prepareMetalBuffers(true));
    CHECK(buffer->getContentStamp()==stamp);
    CHECK(earlier!=buffer->getMetalVertexBuffer());
    draw(capture(),blue);
    buffer=nullptr;
    REQUIRE(command->commitAndWait(&f.error));
    std::vector<U8> actual(expected.size());REQUIRE(copy->read(0,actual.data(),actual.size(),&f.error));
    CHECK(actual==expected);
}
