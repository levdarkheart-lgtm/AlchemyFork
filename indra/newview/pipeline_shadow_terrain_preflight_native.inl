/**
 * @file pipeline_shadow_terrain_preflight_native.inl
 * @brief Actual ordered terrain-shadow preparation and first-failure action.
 * SPDX-License-Identifier: LGPL-2.1-only
 * Function-local include after resolving the existing owner pointer chain.
 */
    ShadowTerrainPreflightSnapshot preflight;
    const auto reason = [&]()
    {
        using Reason = ShadowTerrainPreflightReason;
        if (!face) return Reason::missing_face;
        if (!patch) return Reason::missing_patch;
        if (!surface) return Reason::missing_surface;
        if (!region) return Reason::missing_region;
        if (!buffer) return Reason::missing_buffer;

        // Original bindSurface() supplied this dependency before drawing.
        // Direct native shadows run before the lit/probe producer. Prepare
        // the current height publication here; an old handle alone is not
        // sufficient after a dirty upload failed. This does no upload once
        // the surface's height generation is current.
        auto& maps = surface->getSurfaceMaps();
        preflight.observed |= 1u;
        preflight.height_data = surface->hasZData();
        preflight.map_version = maps.getVersion();
        preflight.height_current = maps.ensureMetalHeightUploaded();
        height_texture = maps.getMetalHeightTexture();
        preflight.height_present = height_texture != nullptr;
        if (!preflight.height_current) return Reason::height_not_current;
        if (!height_texture) return Reason::missing_height_texture;

        preflight.observed |= 2u;
        preflight.prepared = buffer->prepareMetalBuffers(false);
        if (!preflight.prepared) return Reason::native_vertex_not_current;
        preflight.observed |= 4u;
        preflight.position = buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX);
        if (!preflight.position) return Reason::missing_position;
        preflight.observed |= 8u;
        preflight.geom_count = face->getGeomCount();
        if (preflight.geom_count != LLVOSurfacePatch::PATCH_CORNERS)
            return Reason::corner_count;
        preflight.observed |= 16u;
        const auto geom_index = face->getGeomIndex();
        const auto num_vertices = buffer->getNumVerts();
        preflight.geom_index = geom_index;
        preflight.num_vertices = num_vertices;
        if (geom_index + LLVOSurfacePatch::PATCH_CORNERS > num_vertices)
            return Reason::geometry_range;
        if (!std::isfinite(tess_density)) return Reason::tess_density;
        if (!tessellation_pinned && tess_density <= 0.f) return Reason::tess_pin_relation;
        if (!std::isfinite(raster_segment)) return Reason::raster_segment;
        preflight.observed |= 32u;
        preflight.grid_scale = surface->getMetersPerGrid();
        if (!std::isfinite(preflight.grid_scale) || preflight.grid_scale <= 0.f)
            return Reason::grid_scale;
        return Reason::ready;
    }();
    if (reason != ShadowTerrainPreflightReason::ready)
    {
        preflight.reason = reason;
        preflight.frame = gFrameCount;
        preflight.startup_state = static_cast<U32>(LLStartUp::getStartupState());
        preflight.map = capture.spot_shadow_map;
        preflight.directional = capture.spot_shadow_capture_directional;
        preflight.present = U32(face != nullptr) | (U32(drawable != nullptr) << 1) |
            (U32(object != nullptr) << 2) | (U32(patch != nullptr) << 3) |
            (U32(surface != nullptr) << 4) | (U32(region != nullptr) << 5) |
            (U32(buffer != nullptr) << 6);
        preflight.density = tess_density;
        preflight.segment = raster_segment;
        preflight.pinned = tessellation_pinned;
        if (capture.first_shadow_terrain_preflight_failure.reason ==
            ShadowTerrainPreflightReason::not_evaluated)
            capture.first_shadow_terrain_preflight_failure = preflight;
        capture.recent_shadow_terrain_preflight_failure = preflight;
        ++capture.shadow_terrain_preflight_failures;
        ++capture.shadow_terrain_preflight_reasons[static_cast<std::size_t>(reason)];
        capture.spot_shadow_first_incomplete_source_line = shadow_terrain_preflight_source_line;
        capture.spot_shadow_capture_complete = false;
        capture.spot_shadow_batches.clear();
        return;
    }
