/**
 * @file pipeline_shadow_terrain_preflight_types_native.inl
 * @brief Copied diagnostics for the native terrain-shadow prerequisite guard.
 * SPDX-License-Identifier: LGPL-2.1-only
 */
enum class ShadowTerrainPreflightReason : U8
{
    not_evaluated, ready, missing_face, missing_patch, missing_surface,
    missing_region, missing_buffer, height_not_current, missing_height_texture,
    native_vertex_not_current, missing_position, corner_count, geometry_range,
    tess_density, tess_pin_relation, raster_segment, grid_scale, count
};

const char* shadowTerrainPreflightReasonName(ShadowTerrainPreflightReason reason) noexcept
{
    static constexpr const char* names[]{
        "not_evaluated", "ready", "missing_face", "missing_patch", "missing_surface",
        "missing_region", "missing_buffer", "height_not_current", "missing_height_texture",
        "native_vertex_not_current", "missing_position", "corner_count", "geometry_range",
        "tess_density", "tess_pin_relation", "raster_segment", "grid_scale"};
    const auto index = static_cast<std::size_t>(reason);
    return index < std::size(names) ? names[index] : "unknown";
}

struct ShadowTerrainPreflightSnapshot
{
    ShadowTerrainPreflightReason reason = ShadowTerrainPreflightReason::not_evaluated;
    U32 frame = 0, map = 0, startup_state = 0, present = 0, observed = 0;
    bool directional = false, height_data = false, height_current = false;
    bool height_present = false, prepared = false, position = false, pinned = false;
    U32 map_version = 0;
    S64 geom_count = -1, geom_index = -1, num_vertices = -1;
    F32 density = 0.f, segment = 0.f, grid_scale = 0.f;
};

std::ostream& operator<<(std::ostream& out, const ShadowTerrainPreflightSnapshot& value)
{
    return out << "source_stage=shadow_terrain, first_predicate="
        << shadowTerrainPreflightReasonName(value.reason)
        << ", frame/startup/map/directional=" << value.frame << '/' << value.startup_state
        << '/' << value.map << '/' << value.directional
        << ", presence/observed_mask=" << value.present << '/' << value.observed
        << ", height(data/current/present/version)=" << value.height_data << '/'
        << value.height_current << '/' << value.height_present << '/' << value.map_version
        << ", vertex(prepared/position)=" << value.prepared << '/' << value.position
        << ", geometry(count/start/vertices)=" << value.geom_count << '/'
        << value.geom_index << '/' << value.num_vertices
        << ", tess(density/pinned/segment/grid_scale)=" << value.density << '/'
        << value.pinned << '/' << value.segment << '/' << value.grid_scale;
}
