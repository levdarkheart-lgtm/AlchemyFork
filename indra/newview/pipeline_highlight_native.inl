/**
 * @file pipeline_highlight_native.inl
 * @brief Actual native selection highlight admission and shared tests.
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

#if LL_DARWIN
void LLPipeline::captureMetalWorldHighlightBatch(
    LLVertexBuffer* buffer, U32 index_count, U32 index_start,
    LLViewerTexture* texture, const LLColor4& color, U32 texcoord_type)
{
    if (sRenderingHUDs && gGL.hasMetalUITextureCapture())
    {
        // Selected HUD faces are drawn after the world transaction ended,
        // inside the HUD's depth bracket in the ordered UI transaction.
        // Preserve the original setBuffer/draw boundary and sampled checker,
        // rather than appending HUD-space geometry to a later world frame.
        if (index_count == 0)
        {
            return;
        }
        gGL.flush();
        LLImageGL* image = texture ? texture->getGLTexture() : nullptr;
        if (!buffer || !image || !image->getMetalTexture() ||
            texcoord_type < LLVertexBuffer::TYPE_TEXCOORD0 ||
            texcoord_type > LLVertexBuffer::TYPE_TEXCOORD3 ||
            !buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) ||
            !buffer->hasDataType(
                static_cast<LLVertexBuffer::AttributeType>(texcoord_type)))
        {
            gGL.invalidateMetalUITextureCapture(
                "Selected HUD face is missing geometry or its sampled checker");
            return;
        }
        buffer->setBuffer();
        if (!buffer->hasPublishedMetalVertexBuffer() ||
            !buffer->hasPublishedMetalIndexBuffer())
        {
            gGL.invalidateMetalUITextureCapture(
                "Selected HUD face has unpublished native geometry");
            return;
        }

        LLMatrix4a modelview_projection;
        modelview_projection.setMul(
            gGL.getModelviewMatrix(), gGL.getProjectionMatrix());
        ALMetalHUDIndexedBatch batch;
        batch.shader = ALMetalHUDShader::highlight;
        batch.vertices = buffer->getMetalVertexBuffer();
        batch.position_offset = buffer->getOffset(LLVertexBuffer::TYPE_VERTEX);
        batch.texcoord_offset = buffer->getOffset(
            static_cast<LLVertexBuffer::AttributeType>(texcoord_type));
        batch.vertex_count = buffer->getNumVerts();
        batch.indices = buffer->getMetalIndexBuffer();
        batch.index_offset = static_cast<std::size_t>(index_start) *
                             buffer->getIndexStride();
        batch.index_type = buffer->getIndexStride() == 4
            ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
        batch.index_count = index_count;
        batch.textures[0] = image->getMetalTexture();
        batch.texture_count = 1;
        std::memcpy(batch.matrices.modelview_projection,
                    modelview_projection.getF32ptr(),
                    sizeof(batch.matrices.modelview_projection));
        std::memcpy(batch.matrices.texture0,
                    gGL.getTextureMatrix().getF32ptr(),
                    sizeof(batch.matrices.texture0));
        std::memcpy(batch.highlight_color, color.mV,
                    sizeof(batch.highlight_color));
        gGL.drawMetalHUDIndexedBatch(batch, ALSamplers::AnisoWrap);
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
    LLImageGL* image = texture ? texture->getGLTexture() : nullptr;
    if (!capture.complete || !buffer || index_count == 0 ||
        texcoord_type < LLVertexBuffer::TYPE_TEXCOORD0 ||
        texcoord_type > LLVertexBuffer::TYPE_TEXCOORD3 || !image ||
        !image->getMetalTexture() ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) ||
        !buffer->getMetalVertexBuffer() ||
        !buffer->getMetalIndexBuffer())
    {
        capture.markIncomplete(__LINE__);
        capture.batches.clear();
        capture.highlight_batches.clear();
        return;
    }

    const auto texcoord_attribute =
        static_cast<LLVertexBuffer::AttributeType>(texcoord_type);
    if (!buffer->hasDataType(texcoord_attribute))
    {
        capture.markIncomplete(__LINE__);
        capture.batches.clear();
        capture.highlight_batches.clear();
        return;
    }

    LLMatrix4a modelview_projection;
    modelview_projection.setMul(
        gGL.getModelviewMatrix(), gGL.getProjectionMatrix());

    ALMetalWorldHighlightBatch batch;
    batch.vertices = buffer->getMetalVertexBuffer();
    batch.position_offset = buffer->getOffset(
        LLVertexBuffer::TYPE_VERTEX);
    batch.texcoord_offset = buffer->getOffset(texcoord_attribute);
    batch.vertex_count = buffer->getNumVerts();
    batch.indices = buffer->getMetalIndexBuffer();
    batch.index_offset = static_cast<std::size_t>(index_start) *
                         buffer->getIndexStride();
    batch.index_type = buffer->getIndexStride() == 4
        ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
    batch.index_count = index_count;
    batch.texture = image->getMetalTexture();
    std::memcpy(batch.modelview_projection,
                modelview_projection.getF32ptr(),
                sizeof(batch.modelview_projection));
    std::memcpy(batch.texture_matrix,
                gGL.getTextureMatrix().getF32ptr(),
                sizeof(batch.texture_matrix));
    std::memcpy(batch.color, color.mV, sizeof(batch.color));
    // renderGeomPostDeferred's RGB-only mask preserves glow/coverage alpha
    // under selected-face markers, just as it does under transparent debug.
    bool color_mask[4];
    gGL.getColorMask(color_mask);
    batch.write_alpha = color_mask[3];
    capture.highlight_batches.push_back(batch);
}

#endif

void LLPipeline::renderHighlights()
{
    assertInitialized();

    // The original highlight owner enables blending independently of the
    // preceding pools, then restores their state after the selected faces.
    LLGLSPipelineAlpha gls_pipeline_alpha;

    if (hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_SELECTED))
    {
        if (sRenderHighlightTextureChannel == LLRender::DIFFUSE_MAP ||
            sRenderHighlightTextureChannel == LLRender::BASECOLOR_MAP ||
            sRenderHighlightTextureChannel == LLRender::METALLIC_ROUGHNESS_MAP ||
            sRenderHighlightTextureChannel == LLRender::GLTF_NORMAL_MAP ||
            sRenderHighlightTextureChannel == LLRender::EMISSIVE_MAP ||
            sRenderHighlightTextureChannel == LLRender::NUM_TEXTURE_CHANNELS)
        {
            static const LLColor4 highlight_selected_color(1.f, 1.f, 1.f, 0.5f);
            renderSelectedFaces(
                highlight_selected_color,
                LLVertexBuffer::TYPE_TEXCOORD0);
        }

        // Paint 'em red!
        static const LLColor4 highlight_face_color(1.f, 0.f, 0.f, 0.5f);
        for (auto facep : mHighlightFaces)
        {
            facep->renderSelected(
                LLViewerTexture::sNullImagep, highlight_face_color,
                LLVertexBuffer::TYPE_TEXCOORD0);
        }
    }

    // Contains a list of the faces of objects that are physical or
    // have touch-handlers.
    mHighlightFaces.clear();

    if (hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_SELECTED))
    {
        if (sRenderHighlightTextureChannel == LLRender::NORMAL_MAP)
        {
            static const LLColor4 highlight_normal_color(1.0f, 0.5f, 0.5f, 0.5f);
            renderSelectedFaces(
                highlight_normal_color,
                LLVertexBuffer::TYPE_TEXCOORD1);
        }
        else if (sRenderHighlightTextureChannel == LLRender::SPECULAR_MAP)
        {
            static const LLColor4 highlight_specular_color(0.0f, 0.3f, 1.0f, 0.8f);
            renderSelectedFaces(
                highlight_specular_color,
                LLVertexBuffer::TYPE_TEXCOORD2);
        }
    }
}
