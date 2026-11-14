/**
 * @file pipeline_group_capture_native.inl
 * @brief Ordered actual native simple-pass capture shared with owner fixtures.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
void LLPipeline::captureMetalWorldSimplePass()
{
    if (!mMetalWorldSimpleCapture || !mMetalWorldSimpleCapture->frame_active ||
        !mMetalWorldSimpleCapture->pass_active || !sCull) return;
    auto& capture = *mMetalWorldSimpleCapture;
    if (!capture.complete) return;
    auto* at = beginRenderMap(LLRenderPass::PASS_SIMPLE);
    auto* end = endRenderMap(LLRenderPass::PASS_SIMPLE);
    U32 legacy_offset = 0;
    const auto raw_until = [&](U32 offset)
    {
        while (capture.complete && legacy_offset < offset && at != end)
        {
            const LLDrawInfo* info = *at;
            LLCullResult::increment_iterator(at, end);
            ++legacy_offset;
            if (info) captureMetalWorldGBufferBatch(*info, true);
        }
        return capture.complete && legacy_offset == offset;
    };
    try
    {
        for (const auto& range : sCull->nativeSimpleRanges())
        {
            if (!raw_until(range.legacy_offset) || !range.publication)
            { capture.markIncomplete(__LINE__); break; }
            auto group = std::static_pointer_cast<const ALSceneGroupPublicationNative::Payload>(range.publication);
            if (group->sources.size() != range.draw_count)
            { capture.markIncomplete(__LINE__); break; }
            LLMatrix4a model(group->model.data()), modelview, inverse, projection;
            if (!buildMetalWorldDrawMatrices(&model, modelview, inverse, projection))
            { capture.markIncomplete(__LINE__); break; }
            if (!group->native_tuple)
            { capture.markIncomplete(__LINE__); break; }
            capture.world_resources->retain(group->native_tuple);
            if (capture.compact_gbuffer_active) capture.retainCpu(group);
            const F32* inverse_values = inverse.getF32ptr();
            const F32* model_values = modelview.getF32ptr();
            for (const auto& source : group->sources)
            {
                if (!source)
                { capture.markIncomplete(__LINE__); break; }
                ALMetalWorldViewMatrices view;
                std::memcpy(view.modelview_projection, projection.getF32ptr(), sizeof(view.modelview_projection));
                std::memcpy(view.modelview, model_values, sizeof(view.modelview));
                std::memcpy(view.texture_matrix, source.prototype->texture_matrix, sizeof(view.texture_matrix));
                for (U32 column = 0; column < 3; ++column)
                    for (U32 row = 0; row < 3; ++row)
                    {
                        view.normal_matrix[column][row] = inverse_values[row * 4 + column];
                        view.rigged_normal_matrix[column][row] = model_values[column * 4 + row];
                    }
                if (capture.compact_gbuffer_active)
                {
                    if (!capture.appendReference(*source.prototype, view))
                    { capture.markIncomplete(__LINE__); break; }
                }
                else
                {
                    auto& batch = capture.batches.emplace_back(*source.prototype);
                    std::memcpy(batch.modelview_projection, view.modelview_projection, sizeof(batch.modelview_projection));
                    std::memcpy(batch.modelview, view.modelview, sizeof(batch.modelview));
                    std::memcpy(batch.normal_matrix, view.normal_matrix, sizeof(batch.normal_matrix));
                    std::memcpy(batch.rigged_normal_matrix, view.rigged_normal_matrix, sizeof(batch.rigged_normal_matrix));
                }
                ++ALSceneGroupPublicationNative::statistics.captured_draws;
            }
            if (!capture.complete) break;
            ++ALSceneGroupPublicationNative::statistics.captured_groups;
        }
        if (capture.complete)
        {
            while (at != end)
            {
                const LLDrawInfo* info = *at;
                LLCullResult::increment_iterator(at, end);
                if (info) captureMetalWorldGBufferBatch(*info, true);
                if (!capture.complete) break;
            }
        }
    }
    catch (const std::bad_alloc&) { capture.markIncomplete(__LINE__); }
    if (!capture.complete) capture.clearDraws();
}

void LLPipeline::captureMetalWorldMaterialPass(U32 type)
{ captureMetalWorldPublishedPass(type, 1); }

void LLPipeline::captureMetalWorldPBRPass(U32 type)
{ captureMetalWorldPublishedPass(type, 2); }

void LLPipeline::captureMetalWorldPublishedPass(U32 type, U32 family)
{
    if (!mMetalWorldSimpleCapture || !sCull || !mMetalWorldSimpleCapture->frame_active ||
        !mMetalWorldSimpleCapture->pass_active) return;
    auto& capture = *mMetalWorldSimpleCapture;
    if (!capture.complete || (family == 2 && !capture.pbr)) return;
    auto* at = beginRenderMap(type);auto* end = endRenderMap(type);U32 raw_offset = 0;
    const auto raw = [&]
    {
        const LLDrawInfo* info = *at;LLCullResult::increment_iterator(at, end);++raw_offset;
        if (!info)
        {
            if (capture.direct_probe_scene_active) { capture.markIncomplete(__LINE__);capture.clearDraws(); }
            return;
        }
        if (family == 1) captureMetalWorldMaterialBatch(*info, info->mMaterialSlotList.size() > 1);
        else captureMetalWorldPBRBatch(*info, info->mGLTFMaterialList.size() > 1);
    };
    try
    {
        for (const auto& range : sCull->nativeMaterialRanges(type))
        {
            while (capture.complete && raw_offset < range.legacy_offset && at != end) raw();
            if (!capture.complete || raw_offset != range.legacy_offset || !range.publication || !range.model)
            { capture.markIncomplete(__LINE__);break; }
            if (range.raw_skip && (range.raw_skip != range.draw_count ||
                range.raw_skip > static_cast<std::size_t>(end - at)))
            { capture.markIncomplete(__LINE__);break; }
            auto group = std::static_pointer_cast<const ALSceneGroupPublicationNative::Payload>(range.publication);
            ALScenePublicationNative::WorldFlavor flavor;
            flavor.family = static_cast<ALScenePublicationNative::WorldFamily>(family);
            flavor.native_materials = flavor.native_resources = true;
            flavor.normal_map = capture.material_normal_map;flavor.specular_map = capture.material_specular_map;
            flavor.material_alpha_mode = capture.material_alpha_mode;flavor.alpha_mask = capture.alpha_mask;
            if (capture.rigged || capture.impostor_bake || capture.impostor_alpha_active || capture.world_post_deferred_active ||
                capture.pbr_glow || capture.impostor_fullbright || capture.impostor_shiny || capture.impostor_emboss ||
                group->region_identity != range.model.identity || group->material_flavor != ALSceneGroupPublicationNative::materialFlavorKey(flavor) ||
                !group->cohort || group->cohort->size() != range.draw_count ||
                group->cohort->family() != family || group->cohort->regionIdentity() != range.model.identity ||
                !group->cohort->matchesOwners(group, group->native_tuple))
            { capture.markIncomplete(__LINE__);break; }
            LLMatrix4a model(range.model.model.data()), modelview, inverse, projection;
            if (!buildMetalWorldDrawMatrices(&model, modelview, inverse, projection))
            { capture.markIncomplete(__LINE__);break; }
            ALMetalWorldViewMatrices view;
            std::memcpy(view.modelview_projection, projection.getF32ptr(), sizeof(view.modelview_projection));
            std::memcpy(view.modelview, modelview.getF32ptr(), sizeof(view.modelview));
            const auto* inverse_values = inverse.getF32ptr();const auto* model_values = modelview.getF32ptr();
            for (U32 column = 0;column < 3;++column) for (U32 row = 0;row < 3;++row)
            {
                view.normal_matrix[column][row] = inverse_values[row * 4 + column];
                view.rigged_normal_matrix[column][row] = model_values[column * 4 + row];
            }
            // The private producer certificate already validated every static
            // source. One view retains its exact CPU/native owners; fixed UV
            // transforms remain in each immutable source, not this shared view.
            if (!capture.appendCohort(group->cohort, group, group->native_tuple, view))
            { capture.markIncomplete(__LINE__);break; }
            at += range.raw_skip;raw_offset += range.raw_skip;
            ALSceneGroupPublicationNative::statistics.captured_draws += range.draw_count;
            ++ALSceneGroupPublicationNative::statistics.captured_groups;
            ++ALSceneGroupPublicationNative::statistics.material_captured_groups;
            ALSceneGroupPublicationNative::statistics.material_captured_draws += range.draw_count;
        }
        while (capture.complete && at != end) raw();
    }
    catch (const std::bad_alloc&) { capture.markIncomplete(__LINE__); }
    if (!capture.complete) capture.clearDraws();
}

