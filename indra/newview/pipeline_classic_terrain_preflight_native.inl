/**
 * @file pipeline_classic_terrain_preflight_native.inl
 * @brief Actual ordered classic-terrain preflight and failed-capture action.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Function-local include after resolving the original owner pointer chain.
 * A failed preflight returns from the containing capture function.
 */
    TerrainPreflightSnapshot preflight;
    // Keep the original compound guard's order. In particular native prepare
    // may publish pending stores, so it occurs exactly once and only after
    // all original pointer prerequisites; later getters/phase reads remain
    // unevaluated when an earlier predicate fails.
    const auto preflight_reason=[&]()
    {
        using Reason=TerrainPreflightReason;
        if(!face)return Reason::missing_face;
        if(!detail_textures)return Reason::missing_details;
        if(!alpha_ramp)return Reason::missing_ramp;
        if(!detail_phase)return Reason::missing_phase;
        if(!patch)return Reason::missing_patch;
        if(!surface)return Reason::missing_surface;
        if(!region)return Reason::missing_region;
        if(!buffer)return Reason::missing_buffer;
        preflight.observed|=1u;
        preflight.prepared=buffer->prepareMetalBuffers(false);
        if(!preflight.prepared)return Reason::native_vertex_not_current;
        preflight.observed|=2u;
        preflight.position=buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX);
        if(!preflight.position)return Reason::missing_position;
        preflight.observed|=4u;
        const auto geom_count=face->getGeomCount();preflight.geom_count=geom_count;
        if(geom_count!=LLVOSurfacePatch::PATCH_CORNERS)return Reason::corner_count;
        preflight.observed|=8u;
        const auto geom_index=face->getGeomIndex();preflight.geom_index=geom_index;
        const auto num_vertices=buffer->getNumVerts();preflight.num_vertices=num_vertices;
        if(geom_index+LLVOSurfacePatch::PATCH_CORNERS>num_vertices)return Reason::geometry_range;
        if(!std::isfinite(detail_scale)||detail_scale<=0.f)return Reason::detail_scale;
        preflight.observed|=16u;preflight.phase_u=detail_phase[0];
        if(!std::isfinite(preflight.phase_u))return Reason::phase_u;
        preflight.observed|=32u;preflight.phase_v=detail_phase[1];
        if(!std::isfinite(preflight.phase_v))return Reason::phase_v;
        if(!std::isfinite(tess_density))return Reason::tess_density;
        if(!tessellation_pinned&&tess_density<=0.f)return Reason::tess_pin_relation;
        if(!std::isfinite(triplanar_blend)||triplanar_blend<=0.f)return Reason::triplanar_blend;
        return Reason::ready;
    }();
    if(preflight_reason==TerrainPreflightReason::missing_buffer)
    {
        // The original LLFace::renderIndexed emits nothing for a face whose
        // vertex buffer is not ready. Preserve that omission only after all
        // preceding owner prerequisites; a nonnull native failure still
        // invalidates the transaction through the unchanged path below.
        ++capture.terrain_preflight_unready_skips;
        return;
    }
    if(preflight_reason!=TerrainPreflightReason::ready)
    {
        preflight.reason=preflight_reason;preflight.frame=gFrameCount;
        preflight.view=capture.hero_probe_face_capture?2:capture.reflection_probe_face_capture?3:isMainWorldPass()?1:0;
        preflight.face=capture.hero_probe_face_capture?static_cast<S32>(capture.hero_probe_face):
            capture.reflection_probe_face_capture?static_cast<S32>(capture.reflection_probe_face):-1;
        preflight.present=U32(face!=nullptr)|(U32(detail_textures!=nullptr)<<1)|(U32(alpha_ramp!=nullptr)<<2)|
            (U32(detail_phase!=nullptr)<<3)|(U32(drawable!=nullptr)<<4)|(U32(object!=nullptr)<<5)|
            (U32(patch!=nullptr)<<6)|(U32(surface!=nullptr)<<7)|(U32(region!=nullptr)<<8)|(U32(buffer!=nullptr)<<9);
        preflight.drawable_dead=drawable&&drawable->isDead();
        if(buffer)
        {
            preflight.vertex_current=buffer->hasPublishedMetalVertexBuffer();
            preflight.write_failure=buffer->hasNativeVertexWriteFailure();
            preflight.vertex_revision=buffer->getMetalVertexPublicationRevision();
        }
        preflight.scale=detail_scale;preflight.density=tess_density;preflight.blend=triplanar_blend;
        preflight.pinned=tessellation_pinned;preflight.triplanar=triplanar;
        if(capture.first_terrain_preflight_failure.reason==TerrainPreflightReason::not_evaluated)
            capture.first_terrain_preflight_failure=preflight;
        capture.recent_terrain_preflight_failure=preflight;++capture.terrain_preflight_failures;
        capture.markIncomplete(terrain_preflight_source_line);
        capture.clearDraws();
        return;
    }

