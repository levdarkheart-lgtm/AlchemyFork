/**
 * @file alreflection_policy_test.cpp
 * @brief Actual viewer settings project ordinary shading without losing probes.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "linden_common.h"
#include "llmath.h"
#include "llmatrix4a.h"
#include "llplane.h"
#include "llrefcount.h"
#include "llpointer.h"
#include "v4math.h"
#include "llglheaders.h"
#include "../llreflectionmapmanager.h"
#include "../../almetal/almetalworld.h"
#include "llcontrol.h"
#include <array>
#include <cstring>
#include <catch2/catch_test_macros.hpp>

namespace
{
LLControlGroup reflection_policy_settings("Native reflection policy test");

struct ReflectionPolicyCamera
{
    LLMatrix4a view = LLMatrix4a::identity();
    static ReflectionPolicyCamera& getCurrent()
    {
        static ReflectionPolicyCamera value;
        return value;
    }
    const LLMatrix4a& getModelview() const { return view; }
};

// Real packed schema, real controls, and the production method bodies below.
// Only camera input and the owning class name are substituted. This does not
// instantiate a viewer, create GPU resources, or access private object storage.
struct ReflectionPolicyManager
{
    using ReflectionProbeData = LLReflectionMapManager::ReflectionProbeData;
    ReflectionProbeData mProbeData{};
    F32 mMaxProbeLOD = 6.f;
    S32 mRenderReflectionProbeDetail = -1;
    S32 mRenderReflectionProbeLevel = 3;
    U32 mRenderReflectionProbeCount = 256;
    S32 mRenderReflectionProbeDynamicAllocation = -1;
    void refreshSettings();
    bool fillMetalReflectionProbeConstants(
        ALMetalWorldReflectionProbeConstants& constants) const;
    bool fillMetalReflectionProbeShadingConstants(
        ALMetalWorldReflectionProbeConstants& constants) const;
};

#pragma push_macro("LLReflectionMapManager")
#pragma push_macro("LLViewerCamera")
#pragma push_macro("gSavedSettings")
#define LLReflectionMapManager ReflectionPolicyManager
#define LLViewerCamera ReflectionPolicyCamera
#define gSavedSettings reflection_policy_settings
#include "../llreflectionmapmanager_config.inl"
#pragma pop_macro("gSavedSettings")
#pragma pop_macro("LLViewerCamera")
#pragma pop_macro("LLReflectionMapManager")

struct ReflectionPolicyCase
{
    ReflectionPolicyManager manager;
    unsigned refreshes = 0;
    boost::signals2::scoped_connection changed;

    ReflectionPolicyCase()
    {
        if (!reflection_policy_settings.controlExists("RenderReflectionProbeLevel"))
        {
            reflection_policy_settings.declareS32("RenderReflectionProbeDetail", -1, "test");
            reflection_policy_settings.declareS32("RenderReflectionProbeLevel", 3, "test");
            reflection_policy_settings.declareU32("RenderReflectionProbeCount", 256, "test");
            reflection_policy_settings.declareS32("RenderReflectionProbeDynamicAllocation", -1, "test");
        }
        reflection_policy_settings.setS32("RenderReflectionProbeDetail", -1);
        reflection_policy_settings.setS32("RenderReflectionProbeLevel", 3);
        reflection_policy_settings.setU32("RenderReflectionProbeCount", 256);
        reflection_policy_settings.setS32("RenderReflectionProbeDynamicAllocation", -1);
        manager.refreshSettings();
        // The production callback first calls refreshSettings, then schedules
        // resource reconciliation. Observe that actual settings-to-CPU step;
        // allocation, reset, exposure and the full pack are separate owners.
        changed = reflection_policy_settings.getControl("RenderReflectionProbeLevel")
            ->getSignal()->connect([this](LLControlVariable*, const LLSD&, const LLSD&)
            {
                ++refreshes;
                manager.refreshSettings();
            });
        ReflectionPolicyCamera::getCurrent() = ReflectionPolicyCamera{};
        auto& data = manager.mProbeData;
        data.refmapCount = 3;
        data.heroProbeCount = 1;
        data.heroShape = 1;
        data.heroMaxLod = 2.f;
        data.heroSphere.set(0.f, 0.f, -2.f, 10.f);
        data.iterationCount = 32.f;
        data.rayStep = 1.25f;
        data.depthRejectBias = .125f;
        for (unsigned i = 0; i < 3; ++i)
        {
            data.refSphere[i].set(0.f, 0.f, -2.f, i == 0 ? 4096.f : 10.f);
            data.refParams[i].set(1.f, 1.f, 1.f, 0.f);
            data.refIndex[i][0] = i;
            data.refIndex[i][1] = -1;
            data.refIndex[i][3] = i == 0 ? 0 : 1;
        }
        data.refNeighbor[0] = 2;
        data.refIndex[1][1] = 0;
        data.refIndex[1][2] = 1;
        for (auto& bucket : data.refBucket) bucket[0] = 1;
    }
};

using ProbeBytes = std::array<std::byte,
    sizeof(LLReflectionMapManager::ReflectionProbeData)>;
ProbeBytes packed_bytes(const ReflectionPolicyManager& manager)
{
    ProbeBytes bytes;
    std::memcpy(bytes.data(), &manager.mProbeData, bytes.size());
    return bytes;
}

void check_snapshot(const ALMetalWorldReflectionProbeConstants& actual,
                    ALMetalWorldReflectionProbeConstants expected,
                    F32 count)
{
    expected.config[0] = count;
    CHECK(std::memcmp(&actual, &expected, sizeof(actual)) == 0);
}
}

TEST_CASE("Native ordinary probe quality follows warm settings without losing the full pack",
          "[native-reflection-policy]")
{
    ReflectionPolicyCase f;
    const auto packed = packed_bytes(f.manager);
    ALMetalWorldReflectionProbeConstants full, shaded;
    REQUIRE(f.manager.fillMetalReflectionProbeConstants(full));
    REQUIRE(f.manager.fillMetalReflectionProbeShadingConstants(shaded));
    CHECK(full.config[0] == 3.f);
    check_snapshot(shaded, full, 3.f);

    reflection_policy_settings.setS32("RenderReflectionProbeLevel", 0);
    CHECK(f.refreshes == 1);
    CHECK(f.manager.mRenderReflectionProbeLevel == 0);
    CHECK(f.manager.mRenderReflectionProbeDynamicAllocation == -1);
    CHECK(f.manager.mRenderReflectionProbeCount == 256);
    REQUIRE(f.manager.fillMetalReflectionProbeShadingConstants(shaded));
    CHECK(shaded.config[0] == 1.f);
    check_snapshot(shaded, full, 1.f);
    ALMetalWorldReflectionProbeConstants debug;
    REQUIRE(f.manager.fillMetalReflectionProbeConstants(debug));
    check_snapshot(debug, full, 3.f);
    CHECK(packed_bytes(f.manager) == packed);
    CHECK(f.manager.mProbeData.heroProbeCount == 1);
    CHECK(f.manager.mProbeData.iterationCount == 32.f);
    CHECK(f.manager.mProbeData.rayStep == 1.25f);

    reflection_policy_settings.setS32("RenderReflectionProbeLevel", 3);
    CHECK(f.refreshes == 2);
    REQUIRE(f.manager.fillMetalReflectionProbeShadingConstants(shaded));
    check_snapshot(shaded, full, 3.f);
    CHECK(packed_bytes(f.manager) == packed);
}

TEST_CASE("Native probe projection keeps bootstrap invalid counts and nonzero quality semantics",
          "[native-reflection-policy]")
{
    ReflectionPolicyCase f;
    for (const S32 level : {0, 1, 2, 3})
    {
        reflection_policy_settings.setS32("RenderReflectionProbeLevel", level);
        for (const S32 count : {0, 1, 3, LL_MAX_REFLECTION_PROBE_COUNT})
        {
            INFO("level=" << level << " count=" << count);
            f.manager.mProbeData.refmapCount = count;
            ALMetalWorldReflectionProbeConstants full, shaded;
            REQUIRE(f.manager.fillMetalReflectionProbeConstants(full));
            REQUIRE(f.manager.fillMetalReflectionProbeShadingConstants(shaded));
            check_snapshot(shaded, full, level == 0 && count > 0 ? 1.f : F32(count));
            CHECK(f.manager.mProbeData.refmapCount == count);
        }
        for (const S32 count : {-1, LL_MAX_REFLECTION_PROBE_COUNT + 1})
        {
            f.manager.mProbeData.refmapCount = count;
            ALMetalWorldReflectionProbeConstants full{}, shaded{};
            full.config[0] = shaded.config[0] = 17.f;
            CHECK_FALSE(f.manager.fillMetalReflectionProbeConstants(full));
            CHECK_FALSE(f.manager.fillMetalReflectionProbeShadingConstants(shaded));
            CHECK(full.config[0] == 17.f);
            CHECK(shaded.config[0] == 17.f);
        }
    }
}
