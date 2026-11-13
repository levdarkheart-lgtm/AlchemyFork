/**
 * @file alvertex_clone_metal_test.cpp
 * @brief Selection-buffer layout cloning through real native publication and draws.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "llfontmetal_fixture.h"
#include <array>
#include <cstring>

namespace
{
constexpr U32 mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 |
                     LLVertexBuffer::MAP_COLOR;
constexpr U32 side = 16;
const LLColor4U green(0, 255, 0, 255);

struct CloneFixture : ll_test::NativeFontUI
{
    LLPointer<LLImageGL> white;
    std::string error;
    CloneFixture()
    {
        initRenderer();
        gGL.matrixMode(LLRender::MM_PROJECTION); gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW); gGL.loadIdentity();
        LLPointer<LLImageRaw> raw = new LLImageRaw(1, 1, 4);
        std::memset(raw->getData(), 255, 4);
        white = new LLImageGL(raw, false);
        REQUIRE(white->getMetalTexture());
    }
    static LLPointer<LLVertexBuffer> buffer(U32 type_mask = mask, U32 indices = 3)
    {
        LLPointer<LLVertexBuffer> result = new LLVertexBuffer(type_mask);
        REQUIRE(result->allocateBuffer(3, indices));
        return result;
    }
    static void fill(LLVertexBuffer& buffer, bool wide = false)
    {
        // clone establishes a layout, not a copy of the source's contents.
        // Fill every stream this actual UI draw consumes, as LLFace does.
        REQUIRE(buffer.getMappedData()); REQUIRE(buffer.getMappedIndices());
        std::memset(buffer.getMappedData(), 0, buffer.getSize());
        const std::array<LLVector4a, 3> positions{
            LLVector4a(-1.f, -1.f, 0.f, 1.f),
            LLVector4a(3.f, -1.f, 0.f, 1.f),
            LLVector4a(-1.f, 3.f, 0.f, 1.f)};
        const std::array<LLVector2, 3> uv{LLVector2(.5f,.5f), LLVector2(.5f,.5f), LLVector2(.5f,.5f)};
        const std::array<LLColor4U, 3> colors{green, green, green};
        buffer.setPositionData(positions.data());
        buffer.setTexCoord0Data(uv.data()); buffer.setColorData(colors.data());
        if (wide)
        {
            const std::array<U32, 3> indices{0, 1, 2}; buffer.setIndexData(indices.data());
        }
        else
        {
            const std::array<U16, 3> indices{0, 1, 2}; buffer.setIndexData(indices.data());
        }
        buffer.unmapBuffer();
        REQUIRE(buffer.hasPublishedMetalVertexBuffer());
        REQUIRE(buffer.hasPublishedMetalIndexBuffer());
    }
    void draw(LLVertexBuffer& buffer)
    {
        LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
        LLGLDepthTest depth(GL_FALSE, GL_FALSE);
        REQUIRE(gGL.beginMetalUITextureCapture(ALMetalPixelFormat::rgba8_unorm,
            &gUIProgram, nullptr, nullptr, nullptr,
            LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE));
        gGL.setViewport(0, 0, side, side); gUIProgram.bind();
        REQUIRE(gGL.getTextureSlot(0)->bindSampled(white, ALSamplers::PointClamp));
        buffer.setBuffer();
        const U32 before = LLRender::sDrawCalls;
        buffer.draw(LLRender::TRIANGLES, 3, 0);
        CHECK(LLRender::sDrawCalls == before + 1);
        LLRender::EMetalUICaptureEndState state{};
        auto output = gGL.endMetalUITextureCapture(true, false, nullptr, &error, &state);
        INFO(error); REQUIRE(output);
        REQUIRE(state == LLRender::EMetalUICaptureEndState::COMPLETE);
        std::vector<U8> pixels(side * side * 4);
        REQUIRE(output->readback(*device, 0, 0, {}, pixels.data(), side * 4, &error));
        for (U32 pixel = 0; pixel < side * side; ++pixel)
            for (U32 channel = 0; channel < 4; ++channel)
                CHECK(pixels[pixel * 4 + channel] == green.mV[channel]);
    }
};
}

TEST_CASE("Native empty vertex layout clones need no CPU or GPU stores", "[llvertexbuffer][almetal][native-clone]")
{
    CloneFixture fixture;
    LLPointer<LLVertexBuffer> source = new LLVertexBuffer(mask);
    LLPointer<LLVertexBuffer> target = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
    REQUIRE(source->clone(*target));
    CHECK(target->getTypeMask() == source->getTypeMask());
    CHECK(target->getNumVerts() == 0); CHECK(target->getNumIndices() == 0);
    CHECK(target->getSize() == 0); CHECK(target->getIndicesSize() == 0);
    CHECK(target->getMappedData() == nullptr); CHECK(target->getMappedIndices() == nullptr);
    CHECK(target->getMetalVertexBuffer() == nullptr); CHECK(target->getMetalIndexBuffer() == nullptr);
}

TEST_CASE("Native matching U16 layout clones retain warm published buffers and indexed pixels", "[llvertexbuffer][almetal][native-clone]")
{
    CloneFixture fixture;
    auto source = fixture.buffer(), target = fixture.buffer();
    fixture.fill(*target);
    auto* vertices = target->getMappedData(); auto* indices = target->getMappedIndices();
    auto* metal_vertices = target->getMetalVertexBuffer(); auto* metal_indices = target->getMetalIndexBuffer();
    const U64 stamp = target->getContentStamp();
    REQUIRE(source->clone(*target));
    CHECK(target->getMappedData() == vertices); CHECK(target->getMappedIndices() == indices);
    CHECK(target->getMetalVertexBuffer() == metal_vertices); CHECK(target->getMetalIndexBuffer() == metal_indices);
    CHECK(target->getContentStamp() == stamp);
    REQUIRE(target->hasPublishedMetalVertexBuffer()); REQUIRE(target->hasPublishedMetalIndexBuffer());
    CHECK(target->getIndexStride() == sizeof(U16));
    fixture.draw(*target);
}

TEST_CASE("Native clone layout changes recalculate equal-count offsets even at equal byte size", "[llvertexbuffer][almetal][native-clone]")
{
    CloneFixture fixture;
    for (bool equal_bytes : {false, true})
    {
        CAPTURE(equal_bytes);
        auto source = fixture.buffer(mask | LLVertexBuffer::MAP_NORMAL);
        auto target = fixture.buffer(mask | (equal_bytes ? LLVertexBuffer::MAP_TANGENT : 0));
        const U32 old_size = target->getSize();
        const U32 old_uv_offset = target->getOffset(LLVertexBuffer::TYPE_TEXCOORD0);
        if (equal_bytes) REQUIRE(old_size == source->getSize());
        REQUIRE(old_uv_offset != source->getOffset(LLVertexBuffer::TYPE_TEXCOORD0));
        REQUIRE(source->clone(*target));
        CHECK(target->getTypeMask() == source->getTypeMask());
        CHECK(target->getNumVerts() == 3); CHECK(target->getNumIndices() == 3);
        CHECK(target->getSize() == source->getSize());
        for (auto type : {LLVertexBuffer::TYPE_VERTEX, LLVertexBuffer::TYPE_NORMAL,
                          LLVertexBuffer::TYPE_TEXCOORD0, LLVertexBuffer::TYPE_COLOR})
            CHECK(target->getOffset(type) == source->getOffset(type));
        fixture.fill(*target); fixture.draw(*target);
    }
}

TEST_CASE("Native U32 layout cloning preserves physical capacity logical indices and indexed pixels", "[llvertexbuffer][almetal][native-clone]")
{
    CloneFixture fixture;
    auto source = fixture.buffer(mask, 6);
    fixture.fill(*source, true); // Six U16 allocation units become three U32 indices.
    REQUIRE(source->getNumIndices() == 3); REQUIRE(source->getIndicesSize() == 12);
    REQUIRE(source->getIndexStride() == sizeof(U32));
    for (bool existing_u16 : {false, true})
    {
        CAPTURE(existing_u16);
        LLPointer<LLVertexBuffer> target = new LLVertexBuffer(mask);
        if (existing_u16) REQUIRE(target->allocateBuffer(3, 3));
        REQUIRE(source->clone(*target));
        REQUIRE(target->getIndexStride() == sizeof(U32));
        REQUIRE(target->getNumIndices() == 3);
        // Guard before writing, so the old half-capacity defect fails safely.
        REQUIRE(target->getIndicesSize() == 12);
        REQUIRE(target->getMappedIndices());
        fixture.fill(*target, true);
        std::array<U32, 3> actual{};
        REQUIRE(target->getMetalIndexBuffer()->readback(*fixture.device, 0, actual.data(), sizeof(actual), &fixture.error));
        const std::array<U32, 3> expected{0, 1, 2}; CHECK(actual == expected);
        CHECK(target->getNumIndices() == 3);
        fixture.draw(*target);
    }
}
