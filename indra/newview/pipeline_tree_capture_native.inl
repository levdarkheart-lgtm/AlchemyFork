/**
 * @file pipeline_tree_capture_native.inl
 * @brief Native tree capture with original default-image semantics.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-only
 * Function-local body shared with the actual capture contract tests.
 */
    if (!mMetalWorldSimpleCapture ||
        !mMetalWorldSimpleCapture->frame_active ||
        !mMetalWorldSimpleCapture->pass_active)
    {
        return;
    }
    MetalWorldSimpleCapture& capture = *mMetalWorldSimpleCapture;
    if (!capture.complete)
    {
        return;
    }
    LLDrawable* drawable = face ? face->getDrawable() : nullptr;
    LLViewerRegion* region = drawable ? drawable->getRegion() : nullptr;
    LLVertexBuffer* buffer = face ? face->getVertexBuffer() : nullptr;
    LLImageGL* image = texture ? texture->getGLTexture() : nullptr;
    if (image && !image->getMetalTexture())
    {
        // Revival precedes setActive, as in the existing native UI binder:
        // fetched revival tests DELETED, which activation would clear.
        texture->forceImmediateUpdate();
        image = texture->getGLTexture();
        texture->setActive();
        if (image) image->forceUpdateBindStats();
        if (image && !image->getMetalTexture())
        {
            LLViewerTexture* fallback =
                LLViewerFetchedTexture::sDefaultImagep.get();
            LLImageGL* fallback_image = fallback
                ? fallback->getGLTexture() : nullptr;
            if (fallback_image && fallback_image->getMetalTexture())
            {
                image = fallback_image;
            }
        }
    }
    enum class TreeFailure : std::size_t
    {
        ready, region, buffer, image, position, normal, texcoord, color,
        publication, texture, vertices, indices, matrices, count
    };
    const auto reject = [&](TreeFailure reason)
    {
        // Main and probe captures share this producer. Name the view and the
        // first failed prerequisite; a source line alone cannot identify it.
        const U32 view = capture.hero_probe_face_capture ? 2u
            : capture.reflection_probe_face_capture ? 3u
            : isMainWorldPass() ? 1u : 0u;
        constexpr std::array<const char*, static_cast<std::size_t>(TreeFailure::count)> names{
            "ready", "missing_region", "missing_buffer", "missing_image",
            "missing_position", "missing_normal", "missing_texcoord", "missing_color",
            "geometry_not_published", "texture_not_published", "zero_vertices",
            "zero_indices", "invalid_matrices"};
        static std::array<std::array<bool, names.size()>, 4> reported{};
        const auto index = static_cast<std::size_t>(reason);
        if (!reported[view][index])
        {
            reported[view][index] = true;
            try
            {
                LL_WARNS("Metal") << "Native tree capture input unavailable: reason="
                    << names[index] << ", view=" << view << ", frame=" << gFrameCount
                    << LL_ENDL;
            }
            catch (...) {} // An optional diagnostic cannot change rejection.
        }
        capture.markIncomplete(tree_capture_source_line);
        capture.clearDraws();
    };
    // Preserve the original short-circuit order and prepare geometry once.
    const TreeFailure failure = [&]()
    {
        if (!region) return TreeFailure::region;
        if (!buffer) return TreeFailure::buffer;
        if (!image) return TreeFailure::image;
        if (!buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX)) return TreeFailure::position;
        if (!buffer->hasDataType(LLVertexBuffer::TYPE_NORMAL)) return TreeFailure::normal;
        if (!buffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0)) return TreeFailure::texcoord;
        if (!buffer->hasDataType(LLVertexBuffer::TYPE_COLOR)) return TreeFailure::color;
        if (!buffer->prepareMetalBuffers(true)) return TreeFailure::publication;
        if (!image->getMetalTexture()) return TreeFailure::texture;
        if (buffer->getNumVerts() == 0) return TreeFailure::vertices;
        if (buffer->getNumIndices() == 0) return TreeFailure::indices;
        return TreeFailure::ready;
    }();
    if (failure != TreeFailure::ready)
    {
        reject(failure);
        return;
    }

    LLMatrix4a modelview;
    LLMatrix4a inverse_modelview;
    LLMatrix4a modelview_projection;
    if (!buildMetalWorldDrawMatrices(
            &region->mRenderMatrix, modelview, inverse_modelview,
            modelview_projection))
    {
        reject(TreeFailure::matrices);
        return;
    }
    ALMetalWorldSimpleBatch& batch = capture.batches.emplace_back();
    capture.retainGeometry(*buffer);
    batch.vertices = buffer->getMetalVertexBuffer();
    batch.temporal_instance_id=buffer->nativePublication().identity();
    batch.temporal_generation=buffer->nativePublication().publishedRevision();
    batch.position_offset = buffer->getOffset(LLVertexBuffer::TYPE_VERTEX);
    batch.normal_offset = buffer->getOffset(LLVertexBuffer::TYPE_NORMAL);
    batch.texcoord_offset = buffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD0);
    batch.color_offset = buffer->getOffset(LLVertexBuffer::TYPE_COLOR);
    batch.vertex_count = buffer->getNumVerts();
    batch.indices = buffer->getMetalIndexBuffer();
    batch.index_type = buffer->getIndexStride() == 4
        ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
    batch.index_count = buffer->getNumIndices();
    capture.retainImage(image);
    batch.diffuse_textures[0] = image->getMetalTexture();
    batch.texture_count = 1;
    batch.alpha_mask = true;
    batch.use_vertex_color = false;
    batch.alpha_cutoff_bias = false;
    batch.alpha_cutoff = 0.5f;
    std::memcpy(batch.modelview_projection,
                modelview_projection.getF32ptr(),
                sizeof(batch.modelview_projection));
    // treeF applies the hero-mirror plane to vary_position in eye space.
    // The shared Metal cutout vertex path derives the same value from this
    // matrix; leaving it zero retains every tree fragment at clip distance 0.
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
