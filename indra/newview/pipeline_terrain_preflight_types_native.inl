/**
 * @file pipeline_terrain_preflight_types_native.inl
 * @brief Private copied classic-terrain preflight diagnostics.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included within the native scene owner implementation namespace.
 */
// PPD125: copied owner-thread diagnostics only. No scene/resource owner or
// pointer survives the classic terrain preflight, and no guard is relaxed.
enum class TerrainPreflightReason : U8
{
    not_evaluated, ready, missing_face, missing_details, missing_ramp,
    missing_phase, missing_patch, missing_surface, missing_region,
    missing_buffer, native_vertex_not_current, missing_position,
    corner_count, geometry_range, detail_scale, phase_u, phase_v,
    tess_density, tess_pin_relation, triplanar_blend
};

const char* terrainPreflightReasonName(TerrainPreflightReason reason) noexcept
{
    static constexpr const char* names[]{
        "not_evaluated", "ready", "missing_face", "missing_details", "missing_ramp",
        "missing_phase", "missing_patch", "missing_surface", "missing_region",
        "missing_buffer", "native_vertex_not_current", "missing_position",
        "corner_count", "geometry_range", "detail_scale", "phase_u", "phase_v",
        "tess_density", "tess_pin_relation", "triplanar_blend"};
    const auto index=static_cast<std::size_t>(reason);
    return index<std::size(names)?names[index]:"unknown";
}

struct TerrainPreflightSnapshot
{
    TerrainPreflightReason reason=TerrainPreflightReason::not_evaluated;
    U32 frame=0,view=0;
    S32 face=-1;
    // Presence bits: face/details/ramp/phase/drawable/patch-object/patch/
    // surface/region/buffer. Observed bits: prepare/position/count/range/U/V.
    U32 present=0,observed=0;
    S64 geom_count=-1,geom_index=-1,num_vertices=-1;
    U64 vertex_revision=0;
    bool drawable_dead=false,prepared=false,vertex_current=false,write_failure=false;
    bool position=false,pinned=false,triplanar=false;
    F32 scale=0.f,phase_u=0.f,phase_v=0.f,density=0.f,blend=0.f;
};

std::ostream& operator<<(std::ostream& out,const TerrainPreflightSnapshot& snapshot)
{
    return out << "source_stage=classic_compound, source_file=pipeline_classic_terrain_preflight_native.inl, first_predicate="
        << terrainPreflightReasonName(snapshot.reason)
        << ", frame/view_code/face=" << snapshot.frame << '/' << snapshot.view << '/' << snapshot.face
        << ", presence/observed_mask=" << snapshot.present << '/' << snapshot.observed
        << ", drawable_dead=" << snapshot.drawable_dead
        << ", geometry(count/start/vertices)=" << snapshot.geom_count << '/' << snapshot.geom_index << '/' << snapshot.num_vertices
        << ", native(prepared/current/write_failure/revision)=" << snapshot.prepared << '/' << snapshot.vertex_current
        << '/' << snapshot.write_failure << '/' << snapshot.vertex_revision
        << ", position=" << snapshot.position
        << ", detail(scale/U/V)=" << snapshot.scale << '/' << snapshot.phase_u << '/' << snapshot.phase_v
        << ", tess(density/pinned)=" << snapshot.density << '/' << snapshot.pinned
        << ", triplanar(enabled/blend)=" << snapshot.triplanar << '/' << snapshot.blend;
}
