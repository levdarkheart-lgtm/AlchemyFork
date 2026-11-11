/**
 * @file pipeline_hud_debug_native.inl
 * @brief Native HUD diagnostic admission with preserved world capture.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright © 2026, Rye <rye@alchemyviewer.org>
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
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

namespace
{
// Position-only diagnostics use the original current HUD matrices and explicit
// native debug shader. The caller's RGB-only mask/depth remain authoritative.
bool drawHUDDiagnosticTriangles(LLVertexBuffer* buffer, U32 first, U32 count,
                                const LLColor4& color, const char* failure)
{
    if (!count) return true;
    if (!buffer)
    {
        gGL.invalidateMetalUITextureCapture(failure);
        return false;
    }
    buffer->setBuffer(); // The original pushVerts draw retried queued CPU stores.
    if (count % 3 != 0 || first > buffer->getNumIndices() ||
        count > buffer->getNumIndices() - first ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) ||
        !buffer->hasPublishedMetalVertexBuffer() ||
        !buffer->hasPublishedMetalIndexBuffer() ||
        (buffer->getIndexStride() != 2 && buffer->getIndexStride() != 4))
    {
        gGL.invalidateMetalUITextureCapture(failure);
        return false;
    }
    gGL.flush(); // Retire preceding immediate geometry before this indexed draw.
    LLMatrix4a mvp;
    mvp.setMul(gGL.getModelviewMatrix(), gGL.getProjectionMatrix());
    ALMetalHUDIndexedBatch batch;
    batch.shader = ALMetalHUDShader::debug;
    batch.vertices = buffer->getMetalVertexBuffer();
    batch.position_offset = buffer->getOffset(LLVertexBuffer::TYPE_VERTEX);
    batch.vertex_count = buffer->getNumVerts();
    batch.indices = buffer->getMetalIndexBuffer();
    batch.index_offset = static_cast<std::size_t>(first) * buffer->getIndexStride();
    batch.index_type = buffer->getIndexStride() == 4
        ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
    batch.index_count = count;
    batch.double_sided = true;
    std::memcpy(batch.matrices.modelview_projection, mvp.getF32ptr(), sizeof(batch.matrices.modelview_projection));
    std::memcpy(batch.highlight_color, color.mV, sizeof(batch.highlight_color));
    // Restore the original per-diagnostic enable scope removed with the GL draw.
    LLGLEnable blend(GL_BLEND);
    if (!gGL.drawMetalHUDIndexedBatch(batch))
    {
        gGL.invalidateMetalUITextureCapture(failure);
        return false;
    }
    return true;
}

void drawHUDDiagnosticBoxOutline(const LLVector3& pos, const LLVector3& size)
{
    if (!pos.isFinite() || !size.isFinite())
        return;

    LLVector3 v1 = size.scaledVec(LLVector3( 1, 1,1));
    LLVector3 v2 = size.scaledVec(LLVector3(-1, 1,1));
    LLVector3 v3 = size.scaledVec(LLVector3(-1,-1,1));
    LLVector3 v4 = size.scaledVec(LLVector3( 1,-1,1));

    gGL.begin(LLRender::LINES);

    //top
    gGL.vertex3fv((pos+v1).mV);
    gGL.vertex3fv((pos+v2).mV);
    gGL.vertex3fv((pos+v2).mV);
    gGL.vertex3fv((pos+v3).mV);
    gGL.vertex3fv((pos+v3).mV);
    gGL.vertex3fv((pos+v4).mV);
    gGL.vertex3fv((pos+v4).mV);
    gGL.vertex3fv((pos+v1).mV);

    //bottom
    gGL.vertex3fv((pos-v1).mV);
    gGL.vertex3fv((pos-v2).mV);
    gGL.vertex3fv((pos-v2).mV);
    gGL.vertex3fv((pos-v3).mV);
    gGL.vertex3fv((pos-v3).mV);
    gGL.vertex3fv((pos-v4).mV);
    gGL.vertex3fv((pos-v4).mV);
    gGL.vertex3fv((pos-v1).mV);

    //right
    gGL.vertex3fv((pos+v1).mV);
    gGL.vertex3fv((pos-v3).mV);

    gGL.vertex3fv((pos+v4).mV);
    gGL.vertex3fv((pos-v2).mV);

    //left
    gGL.vertex3fv((pos+v2).mV);
    gGL.vertex3fv((pos-v4).mV);

    gGL.vertex3fv((pos+v3).mV);
    gGL.vertex3fv((pos-v1).mV);

    gGL.end();
}
}

void LLPipeline::captureMetalTextureAnimBatch(
    const LLDrawInfo& params)
{
    if (sRenderingHUDs && gGL.hasMetalUITextureCapture())
    {
        drawHUDDiagnosticTriangles(params.mVertexBuffer.get(), params.mOffset,
            params.mCount, LLColor4(1.f, 1.f, 0.f, 0.5f),
            "Texture Anim HUD geometry is incomplete");
        return;
    }

    if (!mMetalWorldSimpleCapture ||
        !mMetalWorldSimpleCapture->frame_active ||
        !mMetalWorldSimpleCapture->world_post_deferred_active ||
        mMetalWorldSimpleCapture->world_post_deferred_phase)
    {
        return;
    }

    MetalWorldSimpleCapture& capture = *mMetalWorldSimpleCapture;
    LLVertexBuffer* buffer = params.mVertexBuffer.get();
    if (!capture.complete || !buffer || params.mCount == 0 ||
        params.mCount % 3 != 0 ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) ||
        !buffer->getMetalVertexBuffer() ||
        !buffer->getMetalIndexBuffer() ||
        (buffer->getIndexStride() != 2 &&
         buffer->getIndexStride() != 4) ||
        capture.render_debug_order ==
            std::numeric_limits<U64>::max())
    {
        capture.markIncomplete(__LINE__);
        capture.texture_anim_batches.clear();
        return;
    }

    LLMatrix4a modelview_projection;
    modelview_projection.setMul(
        gGL.getModelviewMatrix(), gGL.getProjectionMatrix());
    ALMetalWorldTextureAnimBatch batch;
    batch.vertices = buffer->getMetalVertexBuffer();
    batch.position_offset = buffer->getOffset(
        LLVertexBuffer::TYPE_VERTEX);
    batch.vertex_count = buffer->getNumVerts();
    batch.indices = buffer->getMetalIndexBuffer();
    batch.index_offset =
        static_cast<std::size_t>(params.mOffset) *
        buffer->getIndexStride();
    batch.index_type = buffer->getIndexStride() == 4
        ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
    batch.index_count = params.mCount;
    batch.order = capture.render_debug_order++;
    std::memcpy(batch.modelview_projection,
                modelview_projection.getF32ptr(),
                sizeof(batch.modelview_projection));
    try
    {
        capture.texture_anim_batches.push_back(batch);
    }
    catch (const std::bad_alloc&)
    {
        capture.markIncomplete(__LINE__);
        capture.texture_anim_batches.clear();
    }
}

void LLPipeline::captureMetalUpdateTypeFace(
    const LLFace& face, const LLColor4& color)
{
    if (sRenderingHUDs && gGL.hasMetalUITextureCapture())
    {
        if (face.getGeomCount() == 0 || face.getIndicesCount() == 0) return;
        const bool valid_color = color.mV[3] == 0.5f &&
            ((color.mV[0] == 0.f && color.mV[1] == 1.f &&
              (color.mV[2] == 0.f || color.mV[2] == 1.f)) ||
             (color.mV[0] == 1.f && color.mV[2] == 0.f &&
              (color.mV[1] == 0.f || color.mV[1] == 1.f)) ||
             (color.mV[0] == 0.f && color.mV[1] == 0.f && color.mV[2] == 1.f));
        if (face.getIndicesStart() < 0 || !valid_color)
            gGL.invalidateMetalUITextureCapture("Update Type HUD state is incomplete");
        else
            drawHUDDiagnosticTriangles(face.getVertexBuffer(), face.getIndicesStart(),
                face.getIndicesCount(), color, "Update Type HUD geometry is incomplete");
        return;
    }

    if (!mMetalWorldSimpleCapture ||
        !mMetalWorldSimpleCapture->frame_active ||
        !mMetalWorldSimpleCapture->world_post_deferred_active ||
        mMetalWorldSimpleCapture->world_post_deferred_phase)
    {
        return;
    }

    MetalWorldSimpleCapture& capture = *mMetalWorldSimpleCapture;
    LLVertexBuffer* buffer = face.getVertexBuffer();
    if (!buffer || face.getGeomCount() == 0 ||
        face.getIndicesCount() == 0)
    {
        return;
    }

    const auto color_is = [&](F32 red, F32 green, F32 blue)
    {
        return color.mV[0] == red && color.mV[1] == green &&
               color.mV[2] == blue && color.mV[3] == 0.5f;
    };
    const bool valid_color =
        color_is(0.f, 1.f, 0.f) || color_is(0.f, 1.f, 1.f) ||
        color_is(1.f, 0.f, 0.f) || color_is(1.f, 1.f, 0.f) ||
        color_is(0.f, 0.f, 1.f);
    if (!capture.complete || face.getIndicesCount() % 3 != 0 ||
        face.getIndicesStart() < 0 ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) ||
        !buffer->getMetalVertexBuffer() ||
        !buffer->getMetalIndexBuffer() ||
        (buffer->getIndexStride() != 2 &&
         buffer->getIndexStride() != 4) ||
        capture.render_debug_order ==
            std::numeric_limits<U64>::max() ||
        !valid_color)
    {
        capture.markIncomplete(__LINE__);
        capture.update_type_batches.clear();
        return;
    }

    LLMatrix4a modelview_projection;
    modelview_projection.setMul(
        gGL.getModelviewMatrix(), gGL.getProjectionMatrix());
    ALMetalWorldUpdateTypeBatch batch;
    batch.vertices = buffer->getMetalVertexBuffer();
    batch.position_offset = buffer->getOffset(
        LLVertexBuffer::TYPE_VERTEX);
    batch.vertex_count = buffer->getNumVerts();
    batch.indices = buffer->getMetalIndexBuffer();
    batch.index_offset =
        static_cast<std::size_t>(face.getIndicesStart()) *
        buffer->getIndexStride();
    batch.index_type = buffer->getIndexStride() == 4
        ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
    batch.index_count = face.getIndicesCount();
    batch.order = capture.render_debug_order++;
    std::memcpy(batch.modelview_projection,
                modelview_projection.getF32ptr(),
                sizeof(batch.modelview_projection));
    std::memcpy(batch.color, color.mV, sizeof(batch.color));
    try
    {
        capture.update_type_batches.push_back(batch);
    }
    catch (const std::bad_alloc&)
    {
        capture.markIncomplete(__LINE__);
        capture.update_type_batches.clear();
    }
}

void LLPipeline::captureMetalLightDebugBox(
    const LLVector4a& center, const LLVector4a& half_extent,
    const LLColor4& color, bool radius_bounds)
{
    if (sRenderingHUDs && gGL.hasMetalUITextureCapture())
    {
        if (!center.isFinite3() || !half_extent.isFinite3()) return;
        const bool color_valid = color.mV[0] == 1.f && color.mV[1] == 1.f &&
            color.mV[2] == (radius_bounds ? 0.f : 1.f) && color.mV[3] == 1.f;
        if (!color_valid)
        {
            gGL.invalidateMetalUITextureCapture("Lights HUD bounds color is incomplete");
            return;
        }
        gGL.flush();
        const LLColor4 previous_color = gGL.getDiffuseColor();
        LLGLEnable blend(GL_BLEND); // Original renderLights enable scope.
        gGL.diffuseColor4fv(color.mV);
        if (!radius_bounds)
        {
            LLGLDepthTest depth(GL_FALSE, GL_TRUE);
            drawHUDDiagnosticBoxOutline(LLVector3(center), LLVector3(half_extent));
            gGL.flush(); // Encode before restoring the depth-test state.
        }
        else
        {
            drawHUDDiagnosticBoxOutline(LLVector3(center), LLVector3(half_extent));
            gGL.flush();
        }
        gGL.diffuseColor4fv(previous_color.mV);
        return;
    }

    if (!mMetalWorldSimpleCapture ||
        !mMetalWorldSimpleCapture->frame_active ||
        !mMetalWorldSimpleCapture->world_post_deferred_active ||
        mMetalWorldSimpleCapture->world_post_deferred_phase)
    {
        return;
    }
    if (!center.isFinite3() || !half_extent.isFinite3())
    {
        // drawBoxOutline() makes the same valid no-output decision.
        return;
    }

    MetalWorldSimpleCapture& capture = *mMetalWorldSimpleCapture;
    const bool color_valid = radius_bounds
        ? color.mV[0] == 1.f && color.mV[1] == 1.f &&
              color.mV[2] == 0.f && color.mV[3] == 1.f
        : color.mV[0] == 1.f && color.mV[1] == 1.f &&
              color.mV[2] == 1.f && color.mV[3] == 1.f;
    if (!capture.complete ||
        capture.render_debug_order ==
            std::numeric_limits<U64>::max() ||
        !color_valid)
    {
        capture.markIncomplete(__LINE__);
        capture.light_debug_batches.clear();
        return;
    }

    LLMatrix4a modelview_projection;
    modelview_projection.setMul(
        gGL.getModelviewMatrix(), gGL.getProjectionMatrix());
    ALMetalWorldLightDebugBatch batch;
    batch.kind = radius_bounds
        ? ALMetalWorldLightDebugKind::radius_bounds
        : ALMetalWorldLightDebugKind::spatial_bounds;
    batch.order = capture.render_debug_order++;
    std::memcpy(batch.modelview_projection,
                modelview_projection.getF32ptr(),
                sizeof(batch.modelview_projection));
    std::memcpy(batch.center, center.getF32ptr(), sizeof(float) * 3);
    batch.center[3] = 1.f;
    std::memcpy(batch.half_extent, half_extent.getF32ptr(),
                sizeof(float) * 3);
    batch.half_extent[3] = 0.f;
    std::memcpy(batch.color, color.mV, sizeof(batch.color));
    try
    {
        capture.light_debug_batches.push_back(batch);
    }
    catch (const std::bad_alloc&)
    {
        capture.markIncomplete(__LINE__);
        capture.light_debug_batches.clear();
    }
}
