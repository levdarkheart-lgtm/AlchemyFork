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
            capture.world_resources->retain(group);
            const F32* inverse_values = inverse.getF32ptr();
            const F32* model_values = modelview.getF32ptr();
            for (const auto& source : group->sources)
            {
                if (!source)
                { capture.markIncomplete(__LINE__); break; }
                auto& batch = capture.batches.emplace_back(*source.prototype);
                std::memcpy(batch.modelview_projection, projection.getF32ptr(), sizeof(batch.modelview_projection));
                std::memcpy(batch.modelview, model_values, sizeof(batch.modelview));
                for (U32 column = 0; column < 3; ++column)
                    for (U32 row = 0; row < 3; ++row)
                    {
                        batch.normal_matrix[column][row] = inverse_values[row * 4 + column];
                        batch.rigged_normal_matrix[column][row] = model_values[column * 4 + row];
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
    if (!capture.complete) capture.batches.clear();
}

