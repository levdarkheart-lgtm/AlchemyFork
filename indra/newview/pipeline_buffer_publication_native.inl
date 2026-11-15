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
                                               bool batch_textures, bool editable)
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
            capture.clearDraws();
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
        capture.retainGeometry(*buffer);
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
    captureMetalPublishedWorldBatch(params, 0, batch_textures, editable);
}

void LLPipeline::captureMetalPublishedWorldBatch(const LLDrawInfo& params, U32 family, bool indexed, bool editable)
try
{
    if (!mMetalWorldSimpleCapture || !mMetalWorldSimpleCapture->frame_active ||
        !mMetalWorldSimpleCapture->pass_active) return;
    auto& capture = *mMetalWorldSimpleCapture;
    if (!capture.complete || !params.mCount) return;
    static LLCachedControl<bool> native_visibility(gSavedSettings, "RenderMetalNativeVisibility", false);
    static LLCachedControl<bool> native_resources(gSavedSettings, "RenderMetalWorldResourceTables", false);
    static LLCachedControl<bool> native_groups(gSavedSettings, "RenderMetalGroupSources", false);
    ALScenePublicationNative::WorldFlavor flavor;
    // Retained cohorts qualify bounds inside admitted compilation. A denied
    // or unsupported raw fallback does no new optional bounds scan. The
    // standalone legacy flag retains its existing per-source qualification.
    flavor.cohort_bounds = native_visibility() && !native_groups() && !capture.visibility_metadata_failed;
    // Forward/alpha passes have their own lighting ABI. Publish stable tables
    // for the ordinary G-buffer consumer, including opaque impostor bakes.
    flavor.native_materials = !capture.impostor_alpha_active &&
        !capture.world_post_deferred_active && !capture.pbr_glow;
    flavor.native_resources = (native_resources() || capture.source_rows_active) && flavor.native_materials;
    flavor.family = static_cast<ALScenePublicationNative::WorldFamily>(family);
    flavor.indexed_textures = family == 0 && indexed;
    flavor.indexed_materials = family != 0 && indexed;
    flavor.normal_map = capture.material_normal_map;
    flavor.specular_map = capture.material_specular_map;
    flavor.material_alpha_mode = capture.material_alpha_mode;
    flavor.alpha_mask = capture.alpha_mask;
    flavor.rigged = capture.rigged;
    flavor.pbr_glow = capture.pbr_glow;
    flavor.post_water_pbr_glow = family == 2 && capture.pbr_glow && capture.post_water_pbr_glow;
    flavor.impostor_fullbright = capture.impostor_fullbright;
    flavor.impostor_shiny = capture.impostor_shiny;
    flavor.impostor_emboss = capture.impostor_emboss;
    const bool ordinary_animated = !editable && capture.rigged &&
        !capture.impostor_bake && !capture.impostor_alpha_active && !capture.world_post_deferred_active &&
        !capture.pbr_glow && !capture.impostor_fullbright && !capture.impostor_shiny && !capture.impostor_emboss &&
        !params.mTextureMatrix && flavor.hasAnimatedConsumer();
    flavor.animated_source = capture.animated_sources_active && ordinary_animated;
    flavor.animated_gpu_source = capture.animated_gpu_records_active && ordinary_animated;
    flavor.stabilize_animated_direct = capture.animated_direct_stability && ordinary_animated;
    auto source = ALScenePublicationNative::world(params, flavor,
        sNativeSceneSources, sNativeSceneSerial, gFrameCount,capture.animated_compilation_budget);
    if (!source)
    {
        ++capture.scene_source_failures;
        capture.last_scene_source_failure = source.failure;
        capture.markIncomplete(__LINE__); capture.clearDraws(); return;
    }
    if (source.reused) ++capture.scene_source_hits; else ++capture.scene_source_builds;
    LLMatrix4a modelview, inverse_modelview, modelview_projection;
    if (!buildMetalWorldDrawMatrices(params, modelview, inverse_modelview, modelview_projection))
    { capture.markIncomplete(__LINE__); capture.clearDraws(); return; }
    if (!source.owner->native_tuple)
    { capture.markIncomplete(__LINE__); capture.clearDraws(); return; }
    capture.world_resources->retain(source.owner->native_tuple);
    ALMetalWorldViewMatrices view;
    std::memcpy(view.modelview_projection, modelview_projection.getF32ptr(), sizeof(view.modelview_projection));
    std::memcpy(view.modelview, modelview.getF32ptr(), sizeof(view.modelview));
    std::memcpy(view.texture_matrix, source.prototype->texture_matrix, sizeof(view.texture_matrix));
    const F32* inverse = inverse_modelview.getF32ptr();
    const F32* values = modelview.getF32ptr();
    for (U32 column = 0; column < 3; ++column)
        for (U32 row = 0; row < 3; ++row)
        {
            view.normal_matrix[column][row] = inverse[row * 4 + column];
            view.rigged_normal_matrix[column][row] = values[column * 4 + row];
        }
    const bool indexed_source = family == 0
        ? indexed && params.mTextureList.size() > 1 : indexed;
    if (!indexed_source && params.mTextureMatrix &&
        (!gCubeSnapshot || mHeroProbeManager.isMirrorPass()))
        std::memcpy(view.texture_matrix, params.mTextureMatrix->mMatrix, sizeof(view.texture_matrix));
    std::shared_ptr<const ALMetalAttachmentPaletteRecord> palette;
    if(capture.rigged)
    {
        palette=params.mAvatar->updateSkinInfoMatrixPalette(params.mSkinInfo).getMetalAttachmentPalette();
        if(!palette)
        {capture.markIncomplete(__LINE__);capture.clearDraws();return;}
    }
    if(capture.tryCaptureAnimatedSource(params,source,flavor,view,palette,ordinary_animated))return;
    const bool ordinary_source = !editable &&
        !native_visibility() && !capture.rigged && !capture.impostor_bake &&
        !capture.impostor_alpha_active && !capture.world_post_deferred_active &&
        !capture.pbr_glow && !capture.impostor_fullbright &&
        !capture.impostor_shiny && !capture.impostor_emboss &&
        (family != 1 || capture.material_alpha_mode != 1);
    capture.captureSourceRow(params, source, flavor, view, ordinary_source);
    const bool compact = capture.compact_gbuffer_active && ordinary_source;
    if (compact)
    {
        capture.retainCpu(source.owner);
        if (!capture.appendReference(*source.prototype, view))
            capture.markIncomplete(__LINE__);
        return;
    }
    auto& batch = capture.batches.emplace_back(*source.prototype);
    std::memcpy(batch.modelview_projection, view.modelview_projection, sizeof(batch.modelview_projection));
    std::memcpy(batch.modelview, view.modelview, sizeof(batch.modelview));
    std::memcpy(batch.normal_matrix, view.normal_matrix, sizeof(batch.normal_matrix));
    std::memcpy(batch.rigged_normal_matrix, view.rigged_normal_matrix, sizeof(batch.rigged_normal_matrix));
    std::memcpy(batch.texture_matrix, view.texture_matrix, sizeof(batch.texture_matrix));
    if (native_visibility() && !capture.visibility_metadata_failed && !capture.rigged && source.owner->bounds.qualified)
    {
        try
        {
            if (ALScenePublicationNative::visibilityMetadataAllocationForTesting)
                ALScenePublicationNative::visibilityMetadataAllocationForTesting();
            if (sNativeSceneVisibilityDevice != capture.device || !sNativeSceneVisibility)
            {
                sNativeSceneVisibility = ALMetalWorldVisibility::create(*capture.device);
                sNativeSceneVisibilityDevice = capture.device;
            }
            if (!sNativeSceneVisibility)
            {
                capture.world_visibility_members.clear();capture.world_visibility_indices.clear();
                capture.visibility_metadata_failed = true;++capture.visibility_metadata_failures;
            }
            if (sNativeSceneVisibility)
            {
                auto& metadata = *params.mMetalShadowSource;
                if (metadata.visibility.lock() != sNativeSceneVisibility ||
                    metadata.bounds_revision != source.owner->producer_revision)
                {
                    metadata.bounds = sNativeSceneVisibility->publishBounds(params.metalSourcePublication()->identity(),
                        source.owner->producer_revision, source.owner->bounds);
                    metadata.bounds_revision = source.owner->producer_revision;
                    metadata.visibility = sNativeSceneVisibility;
                }
                if (metadata.bounds.identity && capture.logicalDrawCount() <= std::numeric_limits<U32>::max())
                {
                    ALMetalWorldVisibilityMember member;
                    std::memcpy(member.modelview_projection, batch.modelview_projection, sizeof(member.modelview_projection));
                    member.identity = metadata.bounds.identity; member.revision = metadata.bounds.revision;
                    member.slot = metadata.bounds.slot; member.index_count = batch.index_count; member.enabled = 1;
                    capture.world_visibility_members.push_back(member);
                    capture.world_visibility_indices.push_back(static_cast<U32>(capture.logicalDrawCount() - 1));
                }
            }
        }
        catch (const std::bad_alloc&)
        {
            // PPD-224: this optional transaction cannot clear required scene
            // geometry, native leases, ordering, queries or cohort captures.
            capture.world_visibility_members.clear();capture.world_visibility_indices.clear();
            capture.visibility_metadata_failed = true;++capture.visibility_metadata_failures;
        }
    }
    if (capture.rigged)
    {
        capture.retainCpu(palette);
        batch.skin_palette_record = palette.get();
        batch.skin_palette = palette->data();
        batch.skin_palette_float_count = palette->size();
        batch.skin_joint_count = palette->jointCount();
    }
}
catch (const std::bad_alloc&)
{
    if (mMetalWorldSimpleCapture)
    {
        mMetalWorldSimpleCapture->markIncomplete(__LINE__);
        mMetalWorldSimpleCapture->clearDraws();
    }
}
