/**
 * @file pipeline_buffer_publication_native.inl
 * @brief Native buffer publication capture bodies shared with actual-owner tests.
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

void LLPipeline::captureMetalWorldGBufferBatch(const LLDrawInfo& params,
                                               bool batch_textures)
{
    if (!mMetalWorldSimpleCapture ||
        !mMetalWorldSimpleCapture->frame_active ||
        !mMetalWorldSimpleCapture->pass_active)
    {
        return;
    }
    MetalWorldSimpleCapture& capture = *mMetalWorldSimpleCapture;
    if (!capture.complete || !params.mCount)
    {
        return;
    }
    // Legacy glow has a deliberately smaller vertex contract and is captured
    // directly by its native draw-pool enumerator.
    if (capture.legacy_glow)
    {
        return;
    }
    LLVertexBuffer* buffer = params.mVertexBuffer.get();
    if (capture.water_exclusion)
    {
        if (!buffer ||
            !buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) ||
            !buffer->prepareMetalBuffers(true))
        {
            capture.markIncomplete(__LINE__);
            capture.batches.clear();
            return;
        }
        LLMatrix4a modelview;
        LLMatrix4a modelview_projection;
        buildMetalWorldDrawMatrices(
            params, modelview, modelview_projection);
        // Build directly in the retained vector. A failed field/resource
        // check below clears the transaction before returning, so no partial
        // record can reach the renderer.
        ALMetalWorldSimpleBatch& batch = capture.batches.emplace_back();
        batch.vertices = buffer->getMetalVertexBuffer();
        batch.position_offset = buffer->getOffset(
            LLVertexBuffer::TYPE_VERTEX);
        batch.vertex_count = buffer->getNumVerts();
        batch.indices = buffer->getMetalIndexBuffer();
        batch.index_offset = static_cast<std::size_t>(params.mOffset) *
                             buffer->getIndexStride();
        batch.index_type = buffer->getIndexStride() == 4
            ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
        batch.index_count = params.mCount;
        batch.water_exclusion = true;
        std::memcpy(batch.modelview_projection,
                    modelview_projection.getF32ptr(),
                    sizeof(batch.modelview_projection));
        std::memcpy(batch.modelview, modelview.getF32ptr(),
                    sizeof(batch.modelview));
        return;
    }
    const bool indexed_textures =
        batch_textures && params.mTextureList.size() > 1;
    const std::size_t texture_count = indexed_textures
        ? params.mTextureList.size() : 1;
    if (!buffer || texture_count >
            ALMetalWorldSimpleBatch::max_diffuse_textures ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) ||
        (!capture.impostor_fullbright &&
         !buffer->hasDataType(LLVertexBuffer::TYPE_NORMAL)) ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0) ||
        !buffer->hasDataType(LLVertexBuffer::TYPE_COLOR) ||
        (indexed_textures &&
         !buffer->hasDataType(LLVertexBuffer::TYPE_TEXTURE_INDEX)) ||
        !buffer->prepareMetalBuffers(true))
    {
        capture.markIncomplete(__LINE__);
        capture.batches.clear();
        return;
    }

    LLMatrix4a modelview;
    LLMatrix4a inverse_modelview;
    LLMatrix4a modelview_projection;
    if (!buildMetalWorldDrawMatrices(
            params, modelview, inverse_modelview,
            modelview_projection))
    {
        capture.markIncomplete(__LINE__);
        capture.batches.clear();
        return;
    }

    ALMetalWorldSimpleBatch& batch = capture.batches.emplace_back();
    batch.vertices = buffer->getMetalVertexBuffer();
    batch.position_offset = buffer->getOffset(LLVertexBuffer::TYPE_VERTEX);
    batch.normal_offset = buffer->hasDataType(LLVertexBuffer::TYPE_NORMAL)
        ? buffer->getOffset(LLVertexBuffer::TYPE_NORMAL) : 0;
    batch.texcoord_offset = buffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD0);
    batch.color_offset = buffer->getOffset(LLVertexBuffer::TYPE_COLOR);
    batch.vertex_count = buffer->getNumVerts();
    batch.indices = buffer->getMetalIndexBuffer();
    batch.index_offset = static_cast<std::size_t>(params.mOffset) *
                         buffer->getIndexStride();
    batch.index_type = buffer->getIndexStride() == 4
        ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
    batch.index_count = params.mCount;
    batch.impostor_fullbright = capture.impostor_fullbright;
    batch.impostor_shiny = capture.impostor_shiny;
    batch.impostor_emboss = capture.impostor_emboss;
    batch.alpha_mask = capture.alpha_mask;
    batch.alpha_cutoff = capture.alpha_mask
        ? params.mAlphaMaskCutoff : 0.f;
    batch.texture_count = static_cast<U32>(texture_count);
    for (std::size_t index = 0; index < texture_count; ++index)
    {
        LLViewerTexture* texture = indexed_textures
            ? params.mTextureList[index].get() : params.mTexture.get();
        if (!texture)
        {
            texture = LLViewerFetchedTexture::sWhiteImagep.get();
        }
        LLImageGL* image = texture ? texture->getGLTexture() : nullptr;
        ALMetalTexture* metal_texture = image
            ? image->getMetalTexture() : nullptr;
        if (!metal_texture)
        {
            // Preserve streamed-texture substitution without consulting a GL
            // object name: native publication readiness is authoritative.
            LLViewerFetchedTexture* fallback =
                LLViewerFetchedTexture::sDefaultImagep.get();
            LLImageGL* fallback_image = fallback
                ? fallback->getGLTexture() : nullptr;
            metal_texture = fallback_image
                ? fallback_image->getMetalTexture() : nullptr;
        }
        batch.diffuse_textures[index] = metal_texture;
        if (!batch.diffuse_textures[index])
        {
            capture.markIncomplete(__LINE__);
            capture.batches.clear();
            return;
        }
    }
    std::memcpy(batch.modelview_projection,
                modelview_projection.getF32ptr(),
                sizeof(batch.modelview_projection));
    std::memcpy(batch.modelview, modelview.getF32ptr(),
                sizeof(batch.modelview));

    const F32* inverse = inverse_modelview.getF32ptr();
    const F32* modelview_values = modelview.getF32ptr();
    for (U32 column = 0; column < 3; ++column)
    {
        batch.normal_matrix[column][0] = inverse[0 * 4 + column];
        batch.normal_matrix[column][1] = inverse[1 * 4 + column];
        batch.normal_matrix[column][2] = inverse[2 * 4 + column];
        batch.rigged_normal_matrix[column][0] =
            modelview_values[column * 4 + 0];
        batch.rigged_normal_matrix[column][1] =
            modelview_values[column * 4 + 1];
        batch.rigged_normal_matrix[column][2] =
            modelview_values[column * 4 + 2];
    }
    if (!indexed_textures && params.mTextureMatrix &&
        (!gCubeSnapshot || mHeroProbeManager.isMirrorPass()))
    {
        std::memcpy(batch.texture_matrix, params.mTextureMatrix->mMatrix,
                    sizeof(batch.texture_matrix));
    }
    if (capture.rigged)
    {
        if (!params.mAvatar || !params.mSkinInfo ||
            !buffer->hasDataType(LLVertexBuffer::TYPE_WEIGHT4))
        {
            capture.markIncomplete(__LINE__);
            capture.batches.clear();
            return;
        }
        const LLVOAvatar::MatrixPaletteCache& palette =
            params.mAvatar->updateSkinInfoMatrixPalette(params.mSkinInfo);
        const U32 joint_count =
            static_cast<U32>(palette.mMatrixPalette.size());
        if (joint_count == 0 || palette.mGLMp.size() !=
                4 + static_cast<std::size_t>(joint_count) * 12)
        {
            capture.markIncomplete(__LINE__);
            capture.batches.clear();
            return;
        }
        batch.weight_offset = buffer->getOffset(
            LLVertexBuffer::TYPE_WEIGHT4);
        batch.skin_palette = palette.mGLMp.data();
        batch.skin_palette_float_count = palette.mGLMp.size();
        batch.skin_joint_count = joint_count;
    }
}
