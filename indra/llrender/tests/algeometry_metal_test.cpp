/**
 * @file algeometry_metal_test.cpp
 * @brief Generated volume geometry through real native buffers and G-buffer draws.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "llfontmetal_fixture.h"
#include "llvolume.h"
#include "almetalworld.h"
#include <algorithm>
#include <array>
#include <cstring>

namespace
{
struct Geometry : ll_test::NativeFontUI
{
    static constexpr U32 side = 48;
    std::string error;
    std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
    LLPointer<LLImageGL> white, normal;
    std::vector<LLPointer<LLVertexBuffer>> owners;
    Geometry()
    {
        initRenderer();
        renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error); REQUIRE(renderer);
        LLPointer<LLImageRaw> raw = new LLImageRaw(1, 1, 4);
        std::memset(raw->getData(), 255, 4);
        white = new LLImageGL(raw, false);
        const std::array<U8,4> tilted{192,128,238,255};
        std::memcpy(raw->getData(), tilted.data(), 4);
        normal = new LLImageGL(raw, false);
        REQUIRE(white->getMetalTexture()); REQUIRE(normal->getMetalTexture());
    }
    ~Geometry() { CHECK(device->waitUntilIdle(&error)); }

    std::vector<ALMetalWorldSimpleBatch> capture(LLVolume& volume, bool planar)
    {
        std::vector<ALMetalWorldSimpleBatch> batches;
        for (S32 face_index = 0; face_index < volume.getNumVolumeFaces(); ++face_index)
        {
            auto& face = volume.getVolumeFace(face_index);
            if (face.mNumVertices == 0 || face.mNumIndices == 0) continue;
            REQUIRE(volume.genTangents(face_index));
            LLPointer<LLVertexBuffer> buffer = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX |
                LLVertexBuffer::MAP_NORMAL | LLVertexBuffer::MAP_TEXCOORD0 |
                LLVertexBuffer::MAP_TANGENT | LLVertexBuffer::MAP_COLOR);
            REQUIRE(buffer->allocateBuffer(face.mNumVertices, face.mNumIndices));
            std::memset(buffer->getMappedData(), 0, buffer->getSize());
            LLStrider<LLVector3> position, normals;
            LLStrider<LLVector4a> tangents;
            LLStrider<LLVector2> uv;
            LLStrider<LLColor4U> color;
            REQUIRE(buffer->getVertexStrider(position)); REQUIRE(buffer->getNormalStrider(normals));
            REQUIRE(buffer->getTangentStrider(tangents)); REQUIRE(buffer->getTexCoord0Strider(uv));
            REQUIRE(buffer->getColorStrider(color));
            for (S32 i = 0; i < face.mNumVertices; ++i)
            {
                REQUIRE(face.mPositions[i].isFinite3()); REQUIRE(face.mNormals[i].isFinite3());
                position[i].set(face.mPositions[i].getF32ptr());
                normals[i].set(face.mNormals[i].getF32ptr());
                tangents[i] = face.mTangents[i];
                uv[i] = face.mTexCoords[i];
                color[i].set(255,255,255,255);
            }
            if (planar)
            {
                const LLVector4a scale(1.f,1.f,1.f,1.f);
                face.getPlanarTexCoords(scale, nullptr, reinterpret_cast<F32*>(uv.get()));
                face.getPlanarTangents(scale, tangents.get());
            }
            for (S32 i = 0; i < face.mNumVertices; ++i) REQUIRE(tangents[i].isFinite3());
            buffer->setIndexData(face.mIndices); buffer->unmapBuffer();
            REQUIRE(buffer->hasPublishedMetalVertexBuffer()); REQUIRE(buffer->hasPublishedMetalIndexBuffer());
            ALMetalWorldSimpleBatch batch;
            batch.vertices = buffer->getMetalVertexBuffer(); batch.indices = buffer->getMetalIndexBuffer();
            batch.vertex_count = face.mNumVertices; batch.index_count = face.mNumIndices;
            batch.position_offset = buffer->getOffset(LLVertexBuffer::TYPE_VERTEX);
            batch.normal_offset = buffer->getOffset(LLVertexBuffer::TYPE_NORMAL);
            batch.tangent_offset = buffer->getOffset(LLVertexBuffer::TYPE_TANGENT);
            batch.texcoord_offset = buffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD0);
            batch.color_offset = buffer->getOffset(LLVertexBuffer::TYPE_COLOR);
            batch.diffuse_textures[0] = white->getMetalTexture(); batch.texture_count = 1;
            batch.double_sided = true;
            for (U32 i = 0; i < 4; ++i)
            {
                batch.modelview_projection[i*5] = 1.f; batch.modelview[i*5] = 1.f;
                if (i < 3) batch.normal_matrix[i][i] = 1.f;
            }
            // Orthographic object bounds fit the target and Metal's depth interval.
            batch.modelview_projection[10] = .5f; batch.modelview_projection[14] = .5f;
            owners.push_back(buffer); batches.push_back(std::move(batch));
        }
        return batches;
    }
    std::pair<std::vector<U8>,std::vector<U16>> draw(std::vector<ALMetalWorldSimpleBatch>& batches, bool mapped)
    {
        for (auto& batch : batches) batch.normal_texture = mapped ? normal->getMetalTexture() : nullptr;
        ALMetalWorldGBufferScene scene; scene.width = scene.height = side;
        scene.batches = batches.data(); scene.batch_count = batches.size(); scene.normal_hdr = true;
        std::fill(std::begin(scene.clear_color), std::end(scene.clear_color), 0.f);
        auto target = renderer->render(scene, &error); INFO(error); REQUIRE(target);
        std::vector<U8> color(side*side*4); std::vector<U16> normals(side*side*4);
        REQUIRE(target->colorTexture(0)->readback(*device,0,0,{},color.data(),side*4,&error));
        REQUIRE(target->colorTexture(2)->readback(*device,0,0,{},normals.data(),side*8,&error));
        return {std::move(color),std::move(normals)};
    }
};
}

TEST_CASE("Generated prims publish complete native buffers and shade with both tangent frames", "[almetal][geometry-port]")
{
    Geometry fixture;
    for (const auto shape : {LL_PCODE_PROFILE_SQUARE, LL_PCODE_PROFILE_CIRCLE})
    for (bool planar : {false,true})
    {
        CAPTURE(shape,planar);
        LLVolumeParams params; params.setType(shape, LL_PCODE_PATH_LINE);
        params.setShear(.15f,.1f); params.setRatio(.75f,.85f);
        LLPointer<LLVolume> volume = new LLVolume(params, 2.5f);
        auto batches = fixture.capture(*volume, planar); REQUIRE_FALSE(batches.empty());
        const auto plain = fixture.draw(batches, false), mapped = fixture.draw(batches, true);
        CHECK(plain.first == mapped.first);
        U32 covered = 0, changed = 0;
        for (U32 i = 0; i < Geometry::side*Geometry::side; ++i)
        {
            if (plain.first[i*4] == 0) continue;
            ++covered;
            changed += plain.second[i*4] != mapped.second[i*4] ||
                       plain.second[i*4+1] != mapped.second[i*4+1];
        }
        CHECK(covered > 100); CHECK(changed > 20); CHECK(plain.first[0] == 0);
    }
}
