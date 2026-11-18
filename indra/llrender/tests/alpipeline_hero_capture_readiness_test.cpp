/**
 * @file alpipeline_hero_capture_readiness_test.cpp
 * @brief The actual hero readiness boundary defers work before a face opens.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

namespace
{
namespace HeroReadinessTest
{
enum StartupState { STATE_PRECACHE, STATE_STARTED };
struct LLStartUp
{
    inline static StartupState state = STATE_STARTED;
    static StartupState getStartupState() { return state; }
};
struct ALMetalDevice {};
const ALMetalDevice* current_device = nullptr;
const ALMetalDevice* getViewerMetalDevice() { return current_device; }
std::ostringstream warning_output;

struct LLPipeline
{
    struct MetalWorldSimpleCapture
    {
        const ALMetalDevice* device = nullptr;
        std::shared_ptr<int> renderer;
        std::shared_ptr<int> renderer_build;
        std::string renderer_error;
        std::shared_ptr<int> avatar_resources = std::make_shared<int>(11);
        // These are persistent publication inputs, not readiness outputs.
        std::shared_ptr<int> hero_probe_source_cube = std::make_shared<int>(12);
        std::shared_ptr<int> hero_probe_radiance_cube = std::make_shared<int>(13);
        std::uint32_t hero_probe_valid_faces = 0x35;
        std::uint64_t hero_probe_owner_epoch = 19;
    };
    bool initialized = true;
    bool mMetalHeroWorldCaptureActive = false;
    bool mMetalReflectionWorldCaptureActive = false;
    int mMainRT = 0, other_rt = 0;
    int* mRT = &mMainRT;
    std::unique_ptr<MetalWorldSimpleCapture> mMetalWorldSimpleCapture;
    std::unique_ptr<MetalWorldSimpleCapture> mMetalProbeWorldSimpleCapture;
    unsigned initialize_calls = 0, prepare_calls = 0;
    MetalWorldSimpleCapture* initialized_owner = nullptr;
    MetalWorldSimpleCapture* prepared_owner = nullptr;
    const ALMetalDevice* initialized_device = nullptr;
    const ALMetalDevice* prepared_device = nullptr;
    bool initialized_as_main = true, prepared_as_probe = false;

    bool isInit() const { return initialized; }
    bool prepareMetalHeroProbeCapture();
    // Explicit external boundaries: device reconciliation and asynchronous
    // renderer creation have their own owners. The fixture observes calls,
    // and does not reimplement the worker/build or duplicate-post policy.
    void initializeMetalWorldCaptureDevice(MetalWorldSimpleCapture& capture,
                                          const ALMetalDevice* device,
                                          bool main_world_capture)
    {
        ++initialize_calls;
        initialized_owner = &capture;
        initialized_device = device;
        initialized_as_main = main_world_capture;
        capture.device = device;
    }
    void prepareMetalWorldRenderer(MetalWorldSimpleCapture& capture,
                                   const ALMetalDevice* device, bool probe_face)
    {
        ++prepare_calls;
        prepared_owner = &capture;
        prepared_device = device;
        prepared_as_probe = probe_face;
    }
};

// Include the identical production readiness body. Only its external device,
// renderer-preparation and logging boundaries are explicit test observations.
#pragma push_macro("LL_WARNS_ONCE")
#pragma push_macro("LL_ENDL")
#undef LL_WARNS_ONCE
#undef LL_ENDL
#define LL_WARNS_ONCE(category) warning_output
#define LL_ENDL '\n'
#include "../../newview/pipeline_hero_capture_readiness_native.inl"
#pragma pop_macro("LL_ENDL")
#pragma pop_macro("LL_WARNS_ONCE")

struct Fixture
{
    ALMetalDevice device, other_device;
    LLPipeline pipeline;
    Fixture()
    {
        LLStartUp::state = STATE_STARTED;
        current_device = &device;
        warning_output.str({});
        warning_output.clear();
        pipeline.mMetalWorldSimpleCapture =
            std::make_unique<LLPipeline::MetalWorldSimpleCapture>();
        pipeline.mMetalWorldSimpleCapture->device = &device;
        pipeline.mMetalWorldSimpleCapture->renderer = std::make_shared<int>(1);
    }
    ~Fixture() { current_device = nullptr; }
    LLPipeline::MetalWorldSimpleCapture& installProbe()
    {
        pipeline.mMetalProbeWorldSimpleCapture =
            std::make_unique<LLPipeline::MetalWorldSimpleCapture>();
        pipeline.mMetalProbeWorldSimpleCapture->device = &device;
        return *pipeline.mMetalProbeWorldSimpleCapture;
    }
};
}
}

TEST_CASE("Hero readiness rejects ineligible transactions before creating a probe owner",
          "[hero-capture-readiness]")
{
    using namespace HeroReadinessTest;
    Fixture fixture;
    auto& pipeline = fixture.pipeline;
    auto* main = pipeline.mMetalWorldSimpleCapture.get();
    SECTION("pipeline has not initialized") { pipeline.initialized = false; }
    SECTION("startup has not reached native world rendering") { LLStartUp::state = STATE_PRECACHE; }
    SECTION("a hero face is already active") { pipeline.mMetalHeroWorldCaptureActive = true; }
    SECTION("an ordinary reflection face is already active") { pipeline.mMetalReflectionWorldCaptureActive = true; }
    SECTION("the main render target is not current") { pipeline.mRT = &pipeline.other_rt; }
    SECTION("the main owner has not been created")
    {
        pipeline.mMetalWorldSimpleCapture.reset();
        main = nullptr;
    }
    SECTION("the main renderer is still unavailable") { main->renderer.reset(); }
    CHECK_FALSE(pipeline.prepareMetalHeroProbeCapture());
    CHECK(pipeline.mMetalWorldSimpleCapture.get() == main);
    CHECK_FALSE(pipeline.mMetalProbeWorldSimpleCapture);
    CHECK(pipeline.initialize_calls == 0);
    CHECK(pipeline.prepare_calls == 0);
    CHECK(warning_output.str().empty());
}

TEST_CASE("Hero readiness defers absent or mismatched Metal devices without owner mutation",
          "[hero-capture-readiness]")
{
    using namespace HeroReadinessTest;
    Fixture fixture;
    auto& pipeline = fixture.pipeline;
    auto* main = pipeline.mMetalWorldSimpleCapture.get();
    SECTION("device is absent") { current_device = nullptr; }
    SECTION("main renderer belongs to a different device") { current_device = &fixture.other_device; }
    CHECK_FALSE(pipeline.prepareMetalHeroProbeCapture());
    CHECK(pipeline.mMetalWorldSimpleCapture.get() == main);
    CHECK_FALSE(pipeline.mMetalProbeWorldSimpleCapture);
    CHECK(pipeline.initialize_calls == 0);
    CHECK(pipeline.prepare_calls == 0);
    CHECK(warning_output.str().empty());
}

TEST_CASE("Hero readiness prepares the independent probe owner before any face can begin",
          "[hero-capture-readiness]")
{
    using namespace HeroReadinessTest;
    Fixture fixture;
    auto& pipeline = fixture.pipeline;
    auto* main = pipeline.mMetalWorldSimpleCapture.get();
    const auto main_renderer = main->renderer;
    CHECK_FALSE(pipeline.prepareMetalHeroProbeCapture());
    auto* probe = pipeline.mMetalProbeWorldSimpleCapture.get();
    REQUIRE(probe);
    CHECK(probe != main);
    CHECK(probe->avatar_resources == main->avatar_resources);
    CHECK(pipeline.mMetalWorldSimpleCapture.get() == main);
    CHECK(main->renderer == main_renderer);
    CHECK(pipeline.mRT == &pipeline.mMainRT);
    CHECK_FALSE(pipeline.mMetalHeroWorldCaptureActive);
    CHECK_FALSE(pipeline.mMetalReflectionWorldCaptureActive);
    CHECK(pipeline.initialize_calls == 1);
    CHECK(pipeline.initialized_owner == probe);
    CHECK(pipeline.initialized_device == &fixture.device);
    CHECK_FALSE(pipeline.initialized_as_main);
    CHECK(pipeline.prepare_calls == 1);
    CHECK(pipeline.prepared_owner == probe);
    CHECK(pipeline.prepared_device == &fixture.device);
    CHECK(pipeline.prepared_as_probe);
    CHECK(warning_output.str().empty());
}

TEST_CASE("Hero readiness polls preserve pending and published probe identity then accept readiness",
          "[hero-capture-readiness]")
{
    using namespace HeroReadinessTest;
    Fixture fixture;
    auto& pipeline = fixture.pipeline;
    auto* main = pipeline.mMetalWorldSimpleCapture.get();
    auto& probe = fixture.installProbe();
    probe.renderer_build = std::make_shared<int>(14);
    const auto pending = probe.renderer_build;
    const auto source_cube = probe.hero_probe_source_cube;
    const auto radiance_cube = probe.hero_probe_radiance_cube;
    const auto avatar_resources = probe.avatar_resources;
    for (unsigned poll = 0; poll < 3; ++poll)
    {
        CHECK_FALSE(pipeline.prepareMetalHeroProbeCapture());
        CHECK(pipeline.mMetalWorldSimpleCapture.get() == main);
        CHECK(pipeline.mMetalProbeWorldSimpleCapture.get() == &probe);
        CHECK(probe.renderer_build == pending);
        CHECK(probe.hero_probe_source_cube == source_cube);
        CHECK(probe.hero_probe_radiance_cube == radiance_cube);
        CHECK(probe.avatar_resources == avatar_resources);
        CHECK(probe.hero_probe_valid_faces == 0x35);
        CHECK(probe.hero_probe_owner_epoch == 19);
        CHECK_FALSE(pipeline.mMetalHeroWorldCaptureActive);
    }
    // Simulate the asynchronous owner's accepted completion, rather than
    // duplicating its implementation or creating a renderer in the fixture.
    probe.renderer_build.reset();
    probe.renderer = std::make_shared<int>(15);
    CHECK(pipeline.prepareMetalHeroProbeCapture());
    CHECK(pipeline.mMetalWorldSimpleCapture.get() == main);
    CHECK(pipeline.mMetalProbeWorldSimpleCapture.get() == &probe);
    CHECK(probe.hero_probe_source_cube == source_cube);
    CHECK(probe.hero_probe_radiance_cube == radiance_cube);
    CHECK(probe.hero_probe_valid_faces == 0x35);
    CHECK(probe.hero_probe_owner_epoch == 19);
    CHECK(warning_output.str().empty());
}

TEST_CASE("Hero readiness retains and reports a real probe renderer failure",
          "[hero-capture-readiness]")
{
    using namespace HeroReadinessTest;
    Fixture fixture;
    auto& pipeline = fixture.pipeline;
    auto& probe = fixture.installProbe();
    probe.renderer_error = "Recorded renderer construction failure";
    CHECK_FALSE(pipeline.prepareMetalHeroProbeCapture());
    CHECK(probe.renderer_error == "Recorded renderer construction failure");
    CHECK(warning_output.str().find(probe.renderer_error) != std::string::npos);
    CHECK_FALSE(probe.renderer);
    CHECK_FALSE(pipeline.mMetalHeroWorldCaptureActive);
    CHECK(pipeline.mRT == &pipeline.mMainRT);
}
