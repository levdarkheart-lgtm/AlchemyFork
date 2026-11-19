/**
 * @file alheropublicationinputs_test.cpp
 * @brief Actual hero-uniform publication body with synthetic owner boundaries.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The production function body is included verbatim. These tests use the real
 * LL vector, matrix and UUID types; only the camera/pipeline, volume and probe
 * owners are controlled. They do not simulate native radiance generation or
 * execute a viewer/GPU frame. Publication stamps are test inputs, not a second
 * implementation of the publication predicate.
 */
#include "linden_common.h"
#include "llmath.h"
#include "llsimdmath.h"
#include "llvector4a.h"
#include "llmatrix4a.h"
#include "lluuid.h"
#include "m4math.h"
#include "v3math.h"
#include "v4math.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

namespace
{
static_assert(LL_DARWIN, "The fixture exercises the native Darwin publication branch");

struct ControlledPipeline
{
    struct Inputs { bool mMirrors = true; } inputs;
    const Inputs& resourceInputs() const { return inputs; }
} gPipeline;

struct LLViewerCamera
{
    LLMatrix4a modelview = LLMatrix4a::identity();
    static unsigned queries;

    static LLViewerCamera& getCurrent()
    {
        ++queries;
        static LLViewerCamera camera;
        return camera;
    }
    const LLMatrix4a& getModelview() const { return modelview; }
};
unsigned LLViewerCamera::queries = 0;

struct ControlledVolume
{
    explicit ControlledVolume(const char* synthetic_id) : id(synthetic_id) {}
    LLUUID id;
    bool dead = false, box = false;
    LLVector3 scale{4.f, 6.f, 8.f};
    mutable unsigned scale_queries = 0;

    bool isDead() const { return dead; }
    const LLUUID& getID() const { return id; }
    bool getReflectionProbeIsBox() const { return box; }
    const LLVector3& getScale() const { ++scale_queries; return scale; }
};

struct ControlledProbe
{
    LLVector4a mOrigin;
    F32 mRadius = 91.f;
    LLMatrix4 box;
    bool has_box = false;
    unsigned box_queries = 0;

    bool getBox(LLMatrix4& output)
    {
        ++box_queries;
        if (has_box) output = box;
        return has_box;
    }
};

struct ProbePointer
{
    ControlledProbe* value = nullptr;
    bool notNull() const { return value != nullptr; }
    ControlledProbe* operator->() const { return value; }
};

struct UniformData
{
    LLMatrix4 heroBox;
    LLVector4 heroSphere{71.f, 72.f, 73.f, 74.f};
    S32 heroShape = 17;
    F32 heroMaxLod = -1.f;
    S32 heroProbeCount = 7;
};

struct Fixture
{
    // Fixed identifiers are synthetic and have no relation to viewer inputs.
    ControlledVolume owner_a{"00000000-0000-0000-0000-000000000001"};
    ControlledVolume owner_b{"00000000-0000-0000-0000-000000000002"};
    ControlledProbe probe;
    std::vector<ProbePointer> mProbes{{&probe}};
    ControlledVolume* mNearestHero = &owner_a;
    UniformData mHeroData;
    F32 mMaxProbeLOD = 6.25f;
    U64 mMetalRadianceGeneration = 41;
    U64 mMetalRadianceEpoch = 9;
    bool mMetalRadianceReady = true;
    bool mMetalRadianceCaptureComplete = true;
    LLUUID mNativePublishedOwner = owner_a.id;
    U64 mNativePublishedEpoch = 9;

    Fixture()
    {
        gPipeline.inputs.mMirrors = true;
        auto& camera = LLViewerCamera::getCurrent();
        camera.modelview = LLMatrix4a::translation(11.f, 13.f, 17.f);
        LLViewerCamera::queries = 0;
        probe.mOrigin.set(2.f, 3.f, 5.f, 1.f);
        owner_b.scale.set(12.f, 14.f, 16.f);
        for (unsigned row = 0; row != 4; ++row)
        {
            for (unsigned column = 0; column != 4; ++column)
            {
                probe.box.mMatrix[row][column] = 10.f * row + column + 1.f;
                mHeroData.heroBox.mMatrix[row][column] = 100.f + 10.f * row + column;
            }
        }
    }

    void updateUniforms()
    {
        // Only instrumentation is suppressed. The whole production function
        // body, including count clearing, guards and all math, is compiled.
#pragma push_macro("LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY")
#pragma push_macro("LL_PROFILE_GPU_ZONE")
#undef LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY
#undef LL_PROFILE_GPU_ZONE
#define LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY ((void)0)
#define LL_PROFILE_GPU_ZONE(name) ((void)0);
#include "../llhero_uniforms_native.inl"
#pragma pop_macro("LL_PROFILE_GPU_ZONE")
#pragma pop_macro("LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY")
    }
};

void check_sphere(const LLVector4& sphere, F32 x, F32 y, F32 z, F32 radius)
{
    CHECK(sphere.mV[0] == Catch::Approx(x));
    CHECK(sphere.mV[1] == Catch::Approx(y));
    CHECK(sphere.mV[2] == Catch::Approx(z));
    CHECK(sphere.mV[3] == Catch::Approx(radius));
}

void check_matrix(const LLMatrix4& actual, const LLMatrix4& expected)
{
    for (unsigned row = 0; row != 4; ++row)
        for (unsigned column = 0; column != 4; ++column)
            CHECK(actual.mMatrix[row][column] == expected.mMatrix[row][column]);
}

TEST_CASE("Hero uniforms do not advertise a newly selected owner with the previous owner's radiance",
          "[hero-publication][native]")
{
    Fixture f;
    f.updateUniforms();
    REQUIRE(f.mHeroData.heroProbeCount == 1);
    check_sphere(f.mHeroData.heroSphere, 13.f, 16.f, 22.f, 2.f);
    const LLVector4 published_sphere = f.mHeroData.heroSphere;
    const LLMatrix4 published_box = f.mHeroData.heroBox;
    const F32 published_radius = f.probe.mRadius;

    // update() selected B while the completed native publication still names A.
    f.mNearestHero = &f.owner_b;
    f.probe.mOrigin.set(7.f, 8.f, 9.f, 1.f);
    f.mMetalRadianceCaptureComplete = false;
    const auto previous_box_queries = f.probe.box_queries;
    f.updateUniforms();
    CHECK(f.mHeroData.heroProbeCount == 0);
    CHECK(f.owner_b.scale_queries == 0);
    CHECK(f.probe.box_queries == previous_box_queries);
    CHECK(f.probe.mRadius == published_radius);
    check_sphere(f.mHeroData.heroSphere, published_sphere.mV[0], published_sphere.mV[1],
                 published_sphere.mV[2], published_sphere.mV[3]);
    check_matrix(f.mHeroData.heroBox, published_box);
    CHECK(f.mHeroData.heroMaxLod == f.mMaxProbeLOD);

    // A later successful publication supplies B's exact owner/epoch stamp.
    f.mNativePublishedOwner = f.owner_b.id;
    f.mNativePublishedEpoch = f.mMetalRadianceEpoch;
    f.mMetalRadianceCaptureComplete = true;
    f.updateUniforms();
    CHECK(f.mHeroData.heroProbeCount == 1);
    CHECK(f.owner_b.scale_queries == 1);
    check_sphere(f.mHeroData.heroSphere, 18.f, 21.f, 26.f, 6.f);
}

TEST_CASE("Hero uniforms reject old-epoch radiance until the selected owner is republished",
          "[hero-publication][native]")
{
    Fixture f;
    f.updateUniforms();
    REQUIRE(f.mHeroData.heroProbeCount == 1);
    const LLVector4 published_sphere = f.mHeroData.heroSphere;
    const auto previous_scale_queries = f.owner_a.scale_queries;
    ++f.mMetalRadianceEpoch;
    f.updateUniforms();
    CHECK(f.mHeroData.heroProbeCount == 0);
    CHECK(f.owner_a.scale_queries == previous_scale_queries);
    check_sphere(f.mHeroData.heroSphere, published_sphere.mV[0], published_sphere.mV[1],
                 published_sphere.mV[2], published_sphere.mV[3]);

    f.mNativePublishedEpoch = f.mMetalRadianceEpoch;
    f.updateUniforms();
    CHECK(f.mHeroData.heroProbeCount == 1);
}

TEST_CASE("Hero uniforms retain a coherent published owner during its same-owner refresh",
          "[hero-publication][native]")
{
    Fixture f;
    f.mMetalRadianceCaptureComplete = false;
    f.updateUniforms();
    CHECK(f.mHeroData.heroProbeCount == 1);
    CHECK(f.mHeroData.heroShape == 1);
    check_sphere(f.mHeroData.heroSphere, 13.f, 16.f, 22.f, 2.f);

    // Stamp coherence cannot manufacture a missing completed native resource.
    f.mMetalRadianceReady = false;
    f.mMetalRadianceCaptureComplete = true;
    f.updateUniforms();
    CHECK(f.mHeroData.heroProbeCount == 0);
}

TEST_CASE("Hero uniforms clear count before mirrors-off and preserve the existing early-return shape",
          "[hero-publication][native]")
{
    Fixture f;
    f.updateUniforms();
    REQUIRE(f.mHeroData.heroProbeCount == 1);
    const LLVector4 published_sphere = f.mHeroData.heroSphere;
    const LLMatrix4 published_box = f.mHeroData.heroBox;
    const auto previous_camera_queries = LLViewerCamera::queries;
    const auto previous_box_queries = f.probe.box_queries;
    const auto previous_scale_queries = f.owner_a.scale_queries;
    const F32 previous_lod = f.mHeroData.heroMaxLod;
    f.mMaxProbeLOD = 11.f;
    gPipeline.inputs.mMirrors = false;
    f.updateUniforms();
    CHECK(f.mHeroData.heroProbeCount == 0);
    CHECK(LLViewerCamera::queries == previous_camera_queries);
    CHECK(f.probe.box_queries == previous_box_queries);
    CHECK(f.owner_a.scale_queries == previous_scale_queries);
    CHECK(f.mHeroData.heroMaxLod == previous_lod);
    check_sphere(f.mHeroData.heroSphere, published_sphere.mV[0], published_sphere.mV[1],
                 published_sphere.mV[2], published_sphere.mV[3]);
    check_matrix(f.mHeroData.heroBox, published_box);
    gPipeline.inputs.mMirrors = true;
    f.updateUniforms();
    CHECK(f.mHeroData.heroProbeCount == 1);
    CHECK(f.mHeroData.heroMaxLod == 11.f);
}

TEST_CASE("Hero uniforms leave missing dead or empty owners unadvertised",
          "[hero-publication][native]")
{
    for (unsigned invalid = 0; invalid != 5; ++invalid)
    {
        CAPTURE(invalid);
        Fixture f;
        f.updateUniforms();
        REQUIRE(f.mHeroData.heroProbeCount == 1);
        const auto previous_box_queries = f.probe.box_queries;
        const auto previous_scale_queries = f.owner_a.scale_queries;
        switch (invalid)
        {
        case 0: f.mNearestHero = nullptr; break;
        case 1: f.owner_a.dead = true; break;
        case 2: f.mProbes.clear(); break;
        case 3: f.mProbes[0].value = nullptr; break;
        case 4: f.mNativePublishedOwner.setNull(); break;
        }
        f.updateUniforms();
        CHECK(f.mHeroData.heroProbeCount == 0);
        CHECK(f.probe.box_queries == previous_box_queries);
        CHECK(f.owner_a.scale_queries == previous_scale_queries);
        CHECK(f.mHeroData.heroMaxLod == f.mMaxProbeLOD);
    }
}

TEST_CASE("Hero uniform sphere and box math stays in the actual production body",
          "[hero-publication][native]")
{
    Fixture f;
    const std::array<F32, 16> modelview{
        2.f, 0.f, 0.f, 0.f,
        0.f, 3.f, 0.f, 0.f,
        0.f, 0.f, 4.f, 0.f,
        11.f, 13.f, 17.f, 1.f};
    LLViewerCamera::getCurrent().modelview.loadu(modelview.data());
    f.owner_a.scale.set(6.f, 8.f, 0.f);
    f.updateUniforms();
    CHECK(f.mHeroData.heroProbeCount == 1);
    CHECK(f.mHeroData.heroShape == 1);
    CHECK(f.probe.mRadius == 3.f); // sphere uses scale.x / 2
    check_sphere(f.mHeroData.heroSphere, 15.f, 22.f, 37.f, 3.f);

    f.owner_a.box = f.probe.has_box = true;
    f.updateUniforms();
    CHECK(f.mHeroData.heroProbeCount == 1);
    CHECK(f.mHeroData.heroShape == 0);
    CHECK(f.probe.mRadius == Catch::Approx(5.f)); // half extents (3,4,0)
    check_sphere(f.mHeroData.heroSphere, 15.f, 22.f, 37.f, 5.f);
    check_matrix(f.mHeroData.heroBox, f.probe.box);
    CHECK(f.mHeroData.heroMaxLod == f.mMaxProbeLOD);
}
} // namespace
