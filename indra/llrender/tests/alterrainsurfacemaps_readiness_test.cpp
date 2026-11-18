/**
 * @file alterrainsurfacemaps_readiness_test.cpp
 * @brief Actual terrain publication rejects stale height after failed uploads.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace
{
namespace TerrainReadinessTest
{
using S32 = std::int32_t;
using U32 = std::uint32_t;
using F32 = float;
constexpr int GL_R32F = 1, GL_RED = 2, GL_RG16F = 3, GL_RG = 4;
struct ALMetalTexture { F32 sample = 0.f; };
struct Image
{
    std::shared_ptr<ALMetalTexture> native;
};
struct ImageRef
{
    std::shared_ptr<Image> value;
    bool isNull() const { return !value; }
};
struct Surface
{
    bool has_height_data = true;
    F32 height = 1.f;
    Surface* mNeighbors[8]{};
    S32 getGridsPerEdge() const { return 9; }
    bool hasZData() const { return has_height_data; }
    F32 sampleZ(S32, S32, const bool (&)[8]) const { return height; }
};

struct ALTerrainSurfaceMaps
{
    static constexpr S32 APRON = 2;
    struct Rect
    {
        S32 x0 = 0, y0 = 0, x1 = -1, y1 = -1;
        bool empty() const { return x1 < x0 || y1 < y0; }
    };
    using Sampler = std::function<void(S32, S32, F32*)>;
    explicit ALTerrainSurfaceMaps(Surface& surface) : mSurface(surface) {}
    Surface& mSurface;
    ImageRef mHeight, mComposition;
    std::vector<F32> mHeightStaging, mCompositionStaging;
    S32 mSize = 0;
    bool mNoiseReady = false, mDirtyWhole = true, mMetalHeightCurrent = false;
    Rect mDirtyRect;
    U32 mVersion = 0;
    bool height_upload_ok = true, composition_upload_ok = true;
    U32 height_uploads = 0, composition_uploads = 0;
    bool last_height_whole = false;
    Rect last_height_rect;

    void markDirty();
    void markDirty(const Rect& rect);
    void ensureUploaded();
    bool ensureMetalHeightUploaded();
    bool refresh();

    // Explicit sampling/texture-publication boundary fixtures. The production
    // body below owns readiness, dirty state, refresh ordering and return values.
    const ALMetalTexture* getMetalHeightTexture() const
    { return mHeight.value ? mHeight.value->native.get() : nullptr; }
    F32 compositionAt(S32, S32) const { return 2.f; }
    F32 noiseAt(S32, S32) const { return 3.f; }
    static Rect wholeMap(S32 edge) { return {-APRON, -APRON, edge + APRON - 1, edge + APRON - 1}; }
    static Rect merge(const Rect& a, const Rect& b)
    {
        if (a.empty()) return b;
        if (b.empty()) return a;
        return {std::min(a.x0, b.x0), std::min(a.y0, b.y0),
                std::max(a.x1, b.x1), std::max(a.y1, b.y1)};
    }
    static Rect intersect(const Rect& a, const Rect& b)
    {
        return {std::max(a.x0, b.x0), std::max(a.y0, b.y0),
                std::min(a.x1, b.x1), std::min(a.y1, b.y1)};
    }
    static void fill(std::vector<F32>& data, S32, U32 channels, const Sampler& sample)
    {
        data.resize(channels);
        sample(0, 0, data.data());
    }
    static void fillRect(std::vector<F32>& data, S32 edge, U32 channels,
                         const Rect&, const Sampler& sample)
    { fill(data, edge, channels, sample); }
    bool upload(ImageRef& image, S32, U32, std::uint8_t,
                const std::vector<F32>& data, const Rect& rect, bool whole)
    {
        const bool height = &image == &mHeight;
        if (height)
        {
            ++height_uploads;
            last_height_whole = whole;
            last_height_rect = rect;
        }
        else ++composition_uploads;
        if (!(height ? height_upload_ok : composition_upload_ok))
        {
            // The real upload can fail while preserving an old native handle.
            // Supply that boundary condition without reimplementing its COW.
            return false;
        }
        auto next = std::make_shared<Image>();
        next->native = std::make_shared<ALMetalTexture>();
        next->native->sample = data.front();
        image.value = std::move(next);
        return true;
    }
};

#pragma push_macro("LL_DARWIN")
#pragma push_macro("LL_PROFILE_ZONE_SCOPED")
#undef LL_DARWIN
#undef LL_PROFILE_ZONE_SCOPED
#define LL_DARWIN 1
#define LL_PROFILE_ZONE_SCOPED
#include "../../newview/alterrainsurfacemaps_readiness_native.inl"
#pragma pop_macro("LL_PROFILE_ZONE_SCOPED")
#pragma pop_macro("LL_DARWIN")
}
}

TEST_CASE("Terrain height publication retries missing CPU data then stays lazy when current",
          "[terrain-height-readiness]")
{
    using namespace TerrainReadinessTest;
    Surface surface;
    surface.has_height_data = false;
    ALTerrainSurfaceMaps maps(surface);
    CHECK_FALSE(maps.ensureMetalHeightUploaded());
    CHECK(maps.height_uploads == 0);
    CHECK(maps.composition_uploads == 0);
    CHECK(maps.getMetalHeightTexture() == nullptr);
    surface.has_height_data = true;
    REQUIRE(maps.ensureMetalHeightUploaded());
    const auto* published = maps.getMetalHeightTexture();
    REQUIRE(published);
    CHECK(published->sample == 1.f);
    CHECK(maps.height_uploads == 1);
    CHECK(maps.composition_uploads == 1);
    CHECK_FALSE(maps.mDirtyWhole);
    CHECK(maps.mDirtyRect.empty());
    for (unsigned query = 0; query < 20; ++query)
    {
        CHECK(maps.ensureMetalHeightUploaded());
        CHECK(maps.getMetalHeightTexture() == published);
    }
    CHECK(maps.height_uploads == 1);
    CHECK(maps.composition_uploads == 1);
}

TEST_CASE("Terrain dirty marking invalidates readiness until the exact height update succeeds",
          "[terrain-height-readiness]")
{
    using namespace TerrainReadinessTest;
    Surface surface;
    ALTerrainSurfaceMaps maps(surface);
    REQUIRE(maps.ensureMetalHeightUploaded());
    const auto previous = maps.mHeight.value;
    surface.height = 7.f;
    SECTION("whole surface") { maps.markDirty(); }
    SECTION("changed rectangle") { maps.markDirty({1, 2, 3, 4}); }
    CHECK_FALSE(maps.mMetalHeightCurrent);
    CHECK(maps.getMetalHeightTexture() == previous->native.get());
    CHECK(maps.mVersion == 1);
    REQUIRE(maps.ensureMetalHeightUploaded());
    CHECK(maps.getMetalHeightTexture() != previous->native.get());
    CHECK(maps.getMetalHeightTexture()->sample == 7.f);
    CHECK(maps.height_uploads == 2);
    CHECK_FALSE(maps.mDirtyWhole);
    CHECK(maps.mDirtyRect.empty());
}

TEST_CASE("Failed dirty height upload rejects a retained stale handle and retries",
          "[terrain-height-readiness]")
{
    using namespace TerrainReadinessTest;
    Surface surface;
    ALTerrainSurfaceMaps maps(surface);
    REQUIRE(maps.ensureMetalHeightUploaded());
    const auto previous = maps.mHeight.value;
    surface.height = 9.f;
    maps.markDirty({1, 2, 3, 4});
    maps.height_upload_ok = false;
    for (unsigned retry = 0; retry < 2; ++retry)
    {
        CHECK_FALSE(maps.ensureMetalHeightUploaded());
        CHECK_FALSE(maps.mMetalHeightCurrent);
        CHECK(maps.getMetalHeightTexture() == previous->native.get());
        CHECK(maps.getMetalHeightTexture()->sample == 1.f);
        CHECK_FALSE(maps.mDirtyRect.empty());
    }
    maps.height_upload_ok = true;
    REQUIRE(maps.ensureMetalHeightUploaded());
    CHECK(maps.getMetalHeightTexture()->sample == 9.f);
    CHECK(maps.height_uploads == 4);
    CHECK(maps.mDirtyRect.empty());
    CHECK(maps.mMetalHeightCurrent);
}

TEST_CASE("Empty or outside terrain rectangles preserve current height without uploading",
          "[terrain-height-readiness]")
{
    using namespace TerrainReadinessTest;
    Surface surface;
    ALTerrainSurfaceMaps maps(surface);
    REQUIRE(maps.ensureMetalHeightUploaded());
    const auto published = maps.mHeight.value;
    SECTION("an empty dirty rectangle") { maps.markDirty({}); }
    SECTION("a neighbor rectangle beyond the map apron") { maps.markDirty({100, 100, 102, 102}); }
    CHECK(maps.mVersion == 1);
    CHECK(maps.mDirtyRect.empty());
    CHECK_FALSE(maps.mDirtyWhole);
    CHECK(maps.mMetalHeightCurrent);
    for (unsigned query = 0; query < 5; ++query)
    {
        CHECK(maps.ensureMetalHeightUploaded());
        CHECK(maps.mHeight.value == published);
    }
    CHECK(maps.height_uploads == 1);
    CHECK(maps.composition_uploads == 1);
    // An empty follow-up notification must also retain real pending changes.
    maps.markDirty({1, 2, 3, 4});
    maps.markDirty({100, 100, 102, 102});
    CHECK(maps.mVersion == 3);
    CHECK_FALSE(maps.mMetalHeightCurrent);
    CHECK_FALSE(maps.mDirtyRect.empty());
    REQUIRE(maps.ensureMetalHeightUploaded());
    CHECK(maps.height_uploads == 2);
}

TEST_CASE("A composition failure leaves successful current height usable without repeated shadow uploads",
          "[terrain-height-readiness]")
{
    using namespace TerrainReadinessTest;
    Surface surface;
    ALTerrainSurfaceMaps maps(surface);
    SECTION("cold publication") {}
    SECTION("dirty publication")
    {
        REQUIRE(maps.ensureMetalHeightUploaded());
        maps.markDirty({1, 2, 3, 4});
        surface.height = 8.f;
    }
    maps.composition_upload_ok = false;
    REQUIRE(maps.ensureMetalHeightUploaded());
    CHECK(maps.mMetalHeightCurrent);
    CHECK(maps.getMetalHeightTexture()->sample == surface.height);
    const auto height_uploads = maps.height_uploads;
    const auto composition_uploads = maps.composition_uploads;
    const auto published = maps.mHeight.value;
    for (unsigned query = 0; query < 12; ++query)
    {
        CHECK(maps.ensureMetalHeightUploaded());
        CHECK(maps.mHeight.value == published);
    }
    CHECK(maps.height_uploads == height_uploads);
    CHECK(maps.composition_uploads == composition_uploads);
    // Composition may still leave dirty work pending. Notifications with no
    // new height texels must not invalidate the height upload that succeeded.
    maps.markDirty({});
    CHECK(maps.mMetalHeightCurrent);
    CHECK(maps.ensureMetalHeightUploaded());
    maps.markDirty({100, 100, 102, 102});
    CHECK(maps.mMetalHeightCurrent);
    CHECK(maps.ensureMetalHeightUploaded());
    CHECK(maps.mHeight.value == published);
    CHECK(maps.height_uploads == height_uploads);
    CHECK(maps.composition_uploads == composition_uploads);
    // Existing lit/probe callers retain their combined-map retry behavior.
    maps.composition_upload_ok = true;
    maps.ensureUploaded();
    CHECK(maps.height_uploads == height_uploads + 1);
    CHECK(maps.composition_uploads == composition_uploads + 1);
    CHECK_FALSE(maps.mDirtyWhole);
    CHECK(maps.mDirtyRect.empty());
    CHECK(maps.mMetalHeightCurrent);
}

TEST_CASE("A disappeared native height handle requires fresh publication even without a CPU edit",
          "[terrain-height-readiness]")
{
    using namespace TerrainReadinessTest;
    Surface surface;
    ALTerrainSurfaceMaps maps(surface);
    REQUIRE(maps.ensureMetalHeightUploaded());
    maps.mHeight.value->native.reset();
    CHECK(maps.mMetalHeightCurrent);
    CHECK(maps.getMetalHeightTexture() == nullptr);
    SECTION("recreation succeeds")
    {
        REQUIRE(maps.ensureMetalHeightUploaded());
        CHECK(maps.getMetalHeightTexture() != nullptr);
        CHECK(maps.last_height_whole);
        CHECK_FALSE(maps.mDirtyWhole);
    }
    SECTION("recreation fails and later retries")
    {
        maps.height_upload_ok = false;
        CHECK_FALSE(maps.ensureMetalHeightUploaded());
        CHECK_FALSE(maps.mMetalHeightCurrent);
        CHECK(maps.mDirtyWhole);
        maps.height_upload_ok = true;
        CHECK(maps.ensureMetalHeightUploaded());
        CHECK(maps.getMetalHeightTexture() != nullptr);
    }
}
