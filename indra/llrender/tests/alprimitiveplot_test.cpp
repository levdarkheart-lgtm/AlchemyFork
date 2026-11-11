/**
 * @file alprimitiveplot_test.cpp
 * @brief Unsupported native counters must not become numeric Tracy samples.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../llgl.h"
#include "../llrender.h"
#include <catch2/catch_test_macros.hpp>
#include <vector>

namespace
{
struct PrimitiveObservation
{
    const char* name;
    int64_t value;
};
bool primitive_profiler_connected = false;
std::vector<PrimitiveObservation> primitive_observations;
void observe_primitive_plot(const char* name, int64_t value)
{
    primitive_observations.push_back({name, value});
}
}

// Compile the actual production owner with its profiler-enabled branch, even
// when the surrounding test binary has Tracy disabled. Observe the boundary
// that would emit a Tracy sample, without a server or a production test hook.
// A distinct class name prevents collisions with the normal library object.
// All ordinary headers have already selected the binary's normal ABI above.
#pragma push_macro("LL_PROFILER_CONFIGURATION")
#pragma push_macro("TracyIsConnected")
#pragma push_macro("LL_PROFILE_PLOT")
#pragma push_macro("ALPrimitivePlot")
#undef LL_PROFILER_CONFIGURATION
#define LL_PROFILER_CONFIGURATION LL_PROFILER_CONFIG_TRACY
#undef TracyIsConnected
#define TracyIsConnected primitive_profiler_connected
#undef LL_PROFILE_PLOT
#define LL_PROFILE_PLOT(name, value) observe_primitive_plot(name, value)
#define ALPrimitivePlot ALPrimitivePlotProfilerTest
#include "../alprimitiveplot.cpp"
#pragma pop_macro("ALPrimitivePlot")
#pragma pop_macro("LL_PROFILE_PLOT")
#pragma pop_macro("TracyIsConnected")
#pragma pop_macro("LL_PROFILER_CONFIGURATION")

TEST_CASE("Unavailable native primitive counters emit no numeric profiler samples",
          "[alprimitiveplot][native-profiler]")
{
    REQUIRE_FALSE(gGLManager.mInited);
    REQUIRE_FALSE(ALQueryPool::supports(ALQueryKind::Primitives));
    struct Reset
    {
        bool apple = gGLManager.mIsApple;
        ~Reset()
        {
            gGLManager.mIsApple = apple;
            primitive_profiler_connected = false;
            primitive_observations.clear();
        }
    } reset;
    // Both old GL policy predicates permit counting. The native query
    // capability must independently withhold this unsupported measurement.
    gGLManager.mIsApple = false;
    primitive_profiler_connected = true;
    primitive_observations.clear();

    SECTION("zero draw spans do not turn unavailable into measured zero")
    {
        for (const bool tessellated : {false, true})
        {
            CAPTURE(tessellated);
            ALPrimitivePlotProfilerTest plot("native generated primitives", tessellated);
            // Water's native owner no longer opens the old GL query spans.
            // Cross the complete result ring repeatedly, not just admission.
            for (U32 frame = 0; frame < ALQueryPool::FRAMES * 3; ++frame) plot.plot();
            INFO("An emitted zero is a numeric result, not unavailable");
            CAPTURE(primitive_observations.empty() ? -1 : primitive_observations.front().value);
            CHECK(primitive_observations.empty());
            plot.release();
        }
    }
    SECTION("attempted spans remain unavailable")
    {
        ALPrimitivePlotProfilerTest plot("native terrain generated primitives", true);
        for (U32 frame = 0; frame < ALQueryPool::FRAMES * 3; ++frame)
        {
            plot.plot();
            plot.begin();
            plot.end();
        }
        CHECK(primitive_observations.empty());
        plot.release();
    }
}
