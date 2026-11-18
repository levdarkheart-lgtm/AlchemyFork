/**
 * @file alpipeline_shadow_terrain_preflight_test.cpp
 * @brief The actual terrain-shadow guard records its first failed prerequisite.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
namespace ShadowTerrainPreflightTest
{
using U8 = std::uint8_t;
using U32 = std::uint32_t;
using U64 = std::uint64_t;
using S32 = std::int32_t;
using S64 = std::int64_t;
using F32 = float;
U32 gFrameCount = 31;
struct LLStartUp
{
    inline static int state = 9;
    static int getStartupState() { return state; }
};
struct LLVOSurfacePatch { static constexpr U32 PATCH_CORNERS = 4; };
struct LLVertexBuffer { enum AttributeType { TYPE_VERTEX }; };
struct ALMetalTexture {};

#include "../../newview/pipeline_shadow_terrain_preflight_types_native.inl"
static_assert(std::is_trivially_copyable_v<ShadowTerrainPreflightSnapshot>);

struct Observations
{
    std::vector<std::string> order;
    U32 height_prepare = 0, height_getter = 0, native_prepare = 0;
    U32 position = 0, count = 0, index = 0, vertices = 0, grid = 0;
    bool forbid_height = false, forbid_native = false, forbid_geometry = false;
    bool forbid_grid = false;
};
struct Maps
{
    Observations& calls;
    ALMetalTexture texture;
    const ALMetalTexture* height = &texture;
    bool current = true;
    U32 version = 17;
    bool ensureMetalHeightUploaded()
    {
        if (calls.forbid_height) throw std::logic_error("Height preparation evaluated after an earlier failure");
        ++calls.height_prepare;
        calls.order.emplace_back("height");
        return current;
    }
    const ALMetalTexture* getMetalHeightTexture()
    {
        if (calls.forbid_height) throw std::logic_error("Height getter evaluated after an earlier failure");
        ++calls.height_getter;
        return height;
    }
    U32 getVersion() const { return version; }
};
struct Surface
{
    Observations& calls;
    Maps maps;
    bool has_height_data = true;
    F32 grid_scale = 1.f;
    explicit Surface(Observations& value) : calls(value), maps{value} {}
    Maps& getSurfaceMaps()
    {
        if (calls.forbid_height) throw std::logic_error("Surface maps evaluated after an earlier failure");
        return maps;
    }
    bool hasZData() const { return has_height_data; }
    F32 getMetersPerGrid()
    {
        if (calls.forbid_grid) throw std::logic_error("Grid getter evaluated after an earlier failure");
        ++calls.grid;
        return grid_scale;
    }
};
struct Drawable { bool isDead() const { return false; } };
struct PatchObject {};
struct Patch {};
struct Region {};
struct Face
{
    Observations& calls;
    U32 count = 4, index = 0;
    U32 getGeomCount()
    {
        if (calls.forbid_geometry) throw std::logic_error("Geometry count evaluated after an earlier failure");
        ++calls.count;
        return count;
    }
    U32 getGeomIndex()
    {
        if (calls.forbid_geometry) throw std::logic_error("Geometry index evaluated after an earlier failure");
        ++calls.index;
        return index;
    }
};
struct Buffer
{
    Observations& calls;
    bool prepared = true, position = true;
    U32 vertices = 4;
    bool prepareMetalBuffers(bool require_indices)
    {
        if (calls.forbid_native) throw std::logic_error("Native preparation evaluated after an earlier failure");
        CHECK_FALSE(require_indices);
        ++calls.native_prepare;
        calls.order.emplace_back("native");
        return prepared;
    }
    bool hasDataType(LLVertexBuffer::AttributeType)
    {
        if (calls.forbid_native) throw std::logic_error("Position getter evaluated after an earlier failure");
        ++calls.position;
        return position;
    }
    U32 getNumVerts()
    {
        if (calls.forbid_geometry) throw std::logic_error("Vertex count evaluated after an earlier failure");
        ++calls.vertices;
        return vertices;
    }
};
struct Capture
{
    ShadowTerrainPreflightSnapshot first_shadow_terrain_preflight_failure;
    ShadowTerrainPreflightSnapshot recent_shadow_terrain_preflight_failure;
    U64 shadow_terrain_preflight_failures = 0;
    std::array<U64, static_cast<std::size_t>(ShadowTerrainPreflightReason::count)>
        shadow_terrain_preflight_reasons{};
    U32 spot_shadow_map = 2;
    bool spot_shadow_capture_directional = true;
    U32 spot_shadow_first_incomplete_source_line = 0;
    bool spot_shadow_capture_complete = true;
    std::vector<int> spot_shadow_batches{1, 2, 3};
};
struct Fixture
{
    Observations calls;
    Capture capture;
    Face face_value{calls};
    Face* face = &face_value;
    Drawable drawable_value;
    Drawable* drawable = &drawable_value;
    PatchObject object_value;
    PatchObject* object = &object_value;
    Patch patch_value;
    Patch* patch = &patch_value;
    Surface surface_value{calls};
    Surface* surface = &surface_value;
    Region region_value;
    Region* region = &region_value;
    Buffer buffer_value{calls};
    Buffer* buffer = &buffer_value;
    const ALMetalTexture* height_texture = nullptr;
    F32 tess_density = 8.f, raster_segment = 4.f;
    bool tessellation_pinned = false;
    bool accepted = false;
    static constexpr U32 source_line = 712;
    Fixture()
    {
        gFrameCount = 31;
        LLStartUp::state = 9;
    }
    void run()
    {
        const U32 shadow_terrain_preflight_source_line = source_line;
#include "../../newview/pipeline_shadow_terrain_preflight_native.inl"
        accepted = true;
    }
    void checkFailure(ShadowTerrainPreflightReason reason) const
    {
        CHECK_FALSE(accepted);
        CHECK_FALSE(capture.spot_shadow_capture_complete);
        CHECK(capture.spot_shadow_batches.empty());
        CHECK(capture.spot_shadow_first_incomplete_source_line == source_line);
        CHECK(capture.recent_shadow_terrain_preflight_failure.reason == reason);
        CHECK(capture.shadow_terrain_preflight_failures == 1);
        CHECK(capture.shadow_terrain_preflight_reasons[static_cast<std::size_t>(reason)] == 1);
    }
};
}
}

TEST_CASE("Terrain shadow preflight rejects owner prerequisites before any height or native work",
          "[shadow-terrain-preflight]")
{
    using namespace ShadowTerrainPreflightTest;
    Fixture f;
    auto expected = ShadowTerrainPreflightReason::missing_face;
    SECTION("face") { f.face = nullptr; }
    SECTION("patch") { f.patch = nullptr; expected = ShadowTerrainPreflightReason::missing_patch; }
    SECTION("surface") { f.surface = nullptr; expected = ShadowTerrainPreflightReason::missing_surface; }
    SECTION("region") { f.region = nullptr; expected = ShadowTerrainPreflightReason::missing_region; }
    SECTION("buffer") { f.buffer = nullptr; expected = ShadowTerrainPreflightReason::missing_buffer; }
    f.calls.forbid_height = f.calls.forbid_native = true;
    f.calls.forbid_geometry = f.calls.forbid_grid = true;
    f.run();
    f.checkFailure(expected);
    CHECK(f.calls.order.empty());
}

TEST_CASE("Terrain shadow preflight obtains current height before preparing native geometry",
          "[shadow-terrain-preflight]")
{
    using namespace ShadowTerrainPreflightTest;
    Fixture f;
    f.run();
    CHECK(f.accepted);
    CHECK(f.capture.spot_shadow_capture_complete);
    CHECK(f.capture.spot_shadow_batches == std::vector<int>{1, 2, 3});
    CHECK(f.height_texture == &f.surface_value.maps.texture);
    CHECK(f.calls.height_prepare == 1);
    CHECK(f.calls.native_prepare == 1);
    CHECK(f.calls.order == std::vector<std::string>{"height", "native"});
    CHECK(f.capture.shadow_terrain_preflight_failures == 0);
}

TEST_CASE("Terrain shadow preflight rejects an unready height even if an old texture still exists",
          "[shadow-terrain-preflight]")
{
    using namespace ShadowTerrainPreflightTest;
    Fixture f;
    f.surface_value.maps.current = false;
    f.calls.forbid_native = f.calls.forbid_geometry = f.calls.forbid_grid = true;
    f.run();
    f.checkFailure(ShadowTerrainPreflightReason::height_not_current);
    CHECK(f.calls.height_prepare == 1);
    CHECK(f.calls.native_prepare == 0);
    CHECK(f.capture.recent_shadow_terrain_preflight_failure.height_present);
    CHECK_FALSE(f.capture.recent_shadow_terrain_preflight_failure.height_current);
    CHECK(f.capture.recent_shadow_terrain_preflight_failure.observed == 1);
}

TEST_CASE("Terrain shadow preflight retains the missing native texture guard",
          "[shadow-terrain-preflight]")
{
    using namespace ShadowTerrainPreflightTest;
    Fixture f;
    // A boundary reporting success must still supply the native texture.
    f.surface_value.maps.height = nullptr;
    f.calls.forbid_native = f.calls.forbid_geometry = f.calls.forbid_grid = true;
    f.run();
    f.checkFailure(ShadowTerrainPreflightReason::missing_height_texture);
    CHECK(f.calls.height_prepare == 1);
    CHECK_FALSE(f.capture.recent_shadow_terrain_preflight_failure.height_present);
}

TEST_CASE("Terrain shadow preflight preserves native geometry guards and short circuit order",
          "[shadow-terrain-preflight]")
{
    using namespace ShadowTerrainPreflightTest;
    Fixture f;
    auto expected = ShadowTerrainPreflightReason::native_vertex_not_current;
    SECTION("native publication failure precedes every geometry read")
    {
        f.buffer_value.prepared = false;
        f.calls.forbid_geometry = true;
    }
    SECTION("missing position precedes every geometry read")
    {
        f.buffer_value.position = false;
        f.calls.forbid_geometry = true;
        expected = ShadowTerrainPreflightReason::missing_position;
    }
    SECTION("wrong patch corner count precedes the range read")
    {
        f.face_value.count = 3;
        expected = ShadowTerrainPreflightReason::corner_count;
    }
    SECTION("range extends beyond current vertices")
    {
        f.face_value.index = 1;
        expected = ShadowTerrainPreflightReason::geometry_range;
    }
    SECTION("empty native vertex allocation")
    {
        f.buffer_value.vertices = 0;
        expected = ShadowTerrainPreflightReason::geometry_range;
    }
    f.calls.forbid_grid = true;
    f.run();
    f.checkFailure(expected);
    CHECK(f.calls.height_prepare == 1);
    CHECK(f.calls.native_prepare == 1);
    CHECK(f.calls.order == std::vector<std::string>{"height", "native"});
    if (expected == ShadowTerrainPreflightReason::native_vertex_not_current)
    {
        CHECK(f.calls.position == 0);
        CHECK_FALSE(f.capture.recent_shadow_terrain_preflight_failure.prepared);
    }
    if (expected == ShadowTerrainPreflightReason::corner_count)
    {
        CHECK(f.calls.index == 0);
        CHECK(f.calls.vertices == 0);
    }
}

TEST_CASE("Terrain shadow preflight retains every numeric prerequisite",
          "[shadow-terrain-preflight]")
{
    using namespace ShadowTerrainPreflightTest;
    Fixture f;
    auto expected = ShadowTerrainPreflightReason::tess_density;
    SECTION("NaN density") { f.tess_density = std::numeric_limits<F32>::quiet_NaN(); }
    SECTION("infinite density") { f.tess_density = std::numeric_limits<F32>::infinity(); }
    SECTION("unpinned zero density")
    {
        f.tess_density = 0.f;
        expected = ShadowTerrainPreflightReason::tess_pin_relation;
    }
    SECTION("unpinned negative density")
    {
        f.tess_density = -1.f;
        expected = ShadowTerrainPreflightReason::tess_pin_relation;
    }
    SECTION("NaN raster segment")
    {
        f.raster_segment = std::numeric_limits<F32>::quiet_NaN();
        expected = ShadowTerrainPreflightReason::raster_segment;
    }
    SECTION("infinite raster segment")
    {
        f.raster_segment = std::numeric_limits<F32>::infinity();
        expected = ShadowTerrainPreflightReason::raster_segment;
    }
    SECTION("NaN grid scale")
    {
        f.surface_value.grid_scale = std::numeric_limits<F32>::quiet_NaN();
        expected = ShadowTerrainPreflightReason::grid_scale;
    }
    SECTION("infinite grid scale")
    {
        f.surface_value.grid_scale = std::numeric_limits<F32>::infinity();
        expected = ShadowTerrainPreflightReason::grid_scale;
    }
    SECTION("zero grid scale")
    {
        f.surface_value.grid_scale = 0.f;
        expected = ShadowTerrainPreflightReason::grid_scale;
    }
    SECTION("negative grid scale")
    {
        f.surface_value.grid_scale = -1.f;
        expected = ShadowTerrainPreflightReason::grid_scale;
    }
    f.calls.forbid_grid = expected != ShadowTerrainPreflightReason::grid_scale;
    f.run();
    f.checkFailure(expected);
    CHECK(f.calls.native_prepare == 1);
    CHECK(f.calls.grid == (expected == ShadowTerrainPreflightReason::grid_scale ? 1 : 0));
}

TEST_CASE("Terrain shadow preflight preserves original valid pinned and uncapped inputs",
          "[shadow-terrain-preflight]")
{
    using namespace ShadowTerrainPreflightTest;
    Fixture f;
    SECTION("pinned zero density")
    {
        f.tess_density = 0.f;
        f.tessellation_pinned = true;
    }
    SECTION("zero raster cap") { f.raster_segment = 0.f; }
    f.run();
    CHECK(f.accepted);
    CHECK(f.capture.spot_shadow_capture_complete);
    CHECK(f.capture.shadow_terrain_preflight_failures == 0);
    CHECK(f.calls.native_prepare == 1);
}

TEST_CASE("Terrain shadow preflight records the first cause and updates cumulative recent evidence",
          "[shadow-terrain-preflight]")
{
    using namespace ShadowTerrainPreflightTest;
    Fixture f;
    f.surface_value.maps.current = false;
    f.run();
    f.checkFailure(ShadowTerrainPreflightReason::height_not_current);
    const auto first = f.capture.first_shadow_terrain_preflight_failure;
    CHECK(first.frame == 31);
    CHECK(first.startup_state == 9);
    CHECK(first.map == 2);
    CHECK(first.directional);
    CHECK(first.present == 0x7f);
    CHECK(first.map_version == 17);
    CHECK(first.height_data);
    CHECK_FALSE(first.height_current);
    CHECK(first.height_present);
    CHECK(first.geom_count == -1);
    CHECK(first.geom_index == -1);
    CHECK(first.num_vertices == -1);

    // The containing capture owns reset boundaries. With that first record
    // still present, a later rejection must update recent/cumulative evidence
    // without overwriting its original cause.
    f.capture.spot_shadow_capture_complete = true;
    f.capture.spot_shadow_batches = {4, 5};
    f.surface_value.maps.current = true;
    f.buffer_value.position = false;
    f.capture.spot_shadow_map = 1;
    f.capture.spot_shadow_capture_directional = false;
    gFrameCount = 32;
    LLStartUp::state = 10;
    f.run();
    CHECK_FALSE(f.accepted);
    CHECK_FALSE(f.capture.spot_shadow_capture_complete);
    CHECK(f.capture.spot_shadow_batches.empty());
    CHECK(f.capture.shadow_terrain_preflight_failures == 2);
    CHECK(f.capture.shadow_terrain_preflight_reasons[static_cast<std::size_t>(ShadowTerrainPreflightReason::height_not_current)] == 1);
    CHECK(f.capture.shadow_terrain_preflight_reasons[static_cast<std::size_t>(ShadowTerrainPreflightReason::missing_position)] == 1);
    CHECK(f.capture.first_shadow_terrain_preflight_failure.reason == first.reason);
    CHECK(f.capture.first_shadow_terrain_preflight_failure.frame == first.frame);
    const auto recent = f.capture.recent_shadow_terrain_preflight_failure;
    CHECK(recent.reason == ShadowTerrainPreflightReason::missing_position);
    CHECK(recent.frame == 32);
    CHECK(recent.startup_state == 10);
    CHECK(recent.map == 1);
    CHECK_FALSE(recent.directional);
    CHECK(recent.prepared);
    CHECK_FALSE(recent.position);
    std::ostringstream text;
    text << first;
    CHECK(text.str().find("first_predicate=height_not_current") != std::string::npos);
    CHECK(text.str().find("frame/startup/map/directional=31/9/2/1") != std::string::npos);
    CHECK(text.str().find("height(data/current/present/version)=1/0/1/17") != std::string::npos);
    gFrameCount = 31;
    LLStartUp::state = 9;
}
