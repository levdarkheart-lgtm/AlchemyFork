/**
 * @file model_preview_native.inl
 * @brief Native model-preview batch admission and exact scene packing.
 *
 * $LicenseInfo:firstyear=2020&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2020, Linden Research, Inc.
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

// Included at the existing private production scope and by its scoped test.
// Camera and pipeline light values are supplied by the owner.

    bool appendMetalModelPreviewBatch(
        LLVertexBuffer& buffer,
        const ALMetalTexture* texture,
        const LLColor4& diffuse_color,
        bool show_edges,
        const LLMatrix4a& modelview,
        const LLMatrix4a& projection,
        std::vector<ALMetalLitPreviewBatch>& batches,
        const float* skin_palette = nullptr,
        std::size_t skin_palette_float_count = 0,
        U32 skin_joint_count = 0)
    {
        if (!buffer.hasDataType(LLVertexBuffer::TYPE_VERTEX) ||
            !buffer.hasDataType(LLVertexBuffer::TYPE_NORMAL))
        {
            LL_WARNS_ONCE("Metal") << "Model preview remains pending because a "
                                      "batch has no native position/normal layout"
                                   << LL_ENDL;
            return false;
        }
        if (skin_palette &&
            (!buffer.hasDataType(LLVertexBuffer::TYPE_WEIGHT4) ||
             skin_palette_float_count !=
                 4 + static_cast<std::size_t>(skin_joint_count) * 12))
        {
            LL_WARNS_ONCE("Metal") << "Skinned model preview remains pending because a "
                                      "batch has incomplete weights or palette data"
                                   << LL_ENDL;
            return false;
        }

        buffer.setBuffer();
        if (!buffer.hasPublishedMetalVertexBuffer() ||
            !buffer.hasPublishedMetalIndexBuffer())
        {
            LL_WARNS_ONCE("Metal") << "Model preview remains pending because a "
                                      "vertex or index store is not published"
                                   << LL_ENDL;
            return false;
        }
        ALMetalBuffer* vertices = buffer.getMetalVertexBuffer();
        ALMetalBuffer* indices = buffer.getMetalIndexBuffer();

        LLMatrix4a inverse_modelview;
        if (!inverse_modelview.setAffineInverse(modelview))
        {
            LL_WARNS_ONCE("Metal") << "Model preview remains pending because its "
                                      "model-view transform is singular"
                                   << LL_ENDL;
            return false;
        }
        LLMatrix4a modelview_projection;
        modelview_projection.setMul(modelview, projection);

        ALMetalLitPreviewBatch batch;
        batch.vertices = vertices;
        batch.position_offset = buffer.getOffset(LLVertexBuffer::TYPE_VERTEX);
        batch.normal_offset = buffer.getOffset(LLVertexBuffer::TYPE_NORMAL);
        if (skin_palette)
        {
            batch.weight_offset = buffer.getOffset(
                LLVertexBuffer::TYPE_WEIGHT4);
            batch.skin_palette = skin_palette;
            batch.skin_palette_float_count = skin_palette_float_count;
            batch.skin_joint_count = skin_joint_count;
        }
        if (texture)
        {
            if (!buffer.hasDataType(LLVertexBuffer::TYPE_TEXCOORD0))
            {
                LL_WARNS_ONCE("Metal") << "Model preview remains pending because a "
                                          "textured batch has no texture coordinates"
                                       << LL_ENDL;
                return false;
            }
            batch.texcoord_offset = buffer.getOffset(
                LLVertexBuffer::TYPE_TEXCOORD0);
            batch.texture = texture;
        }
        batch.vertex_count = buffer.getNumVerts();
        batch.indices = indices;
        batch.index_type = buffer.getIndexStride() == 4
            ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
        batch.index_count = buffer.getNumIndices();
        std::memcpy(batch.modelview_projection,
                    modelview_projection.getF32ptr(),
                    sizeof(batch.modelview_projection));

        const F32* inverse = inverse_modelview.getF32ptr();
        for (U32 column = 0; column < 3; ++column)
        {
            batch.normal_matrix[column][0] = inverse[0 * 4 + column];
            batch.normal_matrix[column][1] = inverse[1 * 4 + column];
            batch.normal_matrix[column][2] = inverse[2 * 4 + column];
        }

        LLVector3 light_directions[3] = {
            LLPipeline::PreviewDirection0,
            LLPipeline::PreviewDirection1,
            LLPipeline::PreviewDirection2,
        };
        const LLColor4 light_diffuse[3] = {
            LLPipeline::PreviewDiffuse0,
            LLPipeline::PreviewDiffuse1,
            LLPipeline::PreviewDiffuse2,
        };
        for (U32 light = 0; light < 3; ++light)
        {
            light_directions[light].normVec();
            std::memcpy(batch.light_direction[light],
                        light_directions[light].mV, sizeof(F32) * 3);
            std::memcpy(batch.light_diffuse[light],
                        light_diffuse[light].mV, sizeof(F32) * 3);
        }
        std::memcpy(batch.diffuse_color, diffuse_color.mV,
                    sizeof(batch.diffuse_color));
        batch.show_edges = show_edges;
        std::memcpy(batch.edge_color, PREVIEW_EDGE_COL.mV,
                    sizeof(batch.edge_color));
        batches.push_back(batch);
        return true;
    }
