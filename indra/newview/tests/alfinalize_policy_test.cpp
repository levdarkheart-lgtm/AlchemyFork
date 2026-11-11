/**
 * @file alfinalize_policy_test.cpp
 * @brief Actual native frame policy without constructing a viewer or GPU.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "linden_common.h"
#include "../pipeline.h"
#include "../../almetal/almetalworld.h"
#include "llcontrol.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
LLControlGroup finalize_settings("Native finalization policy test");
std::vector<std::string> control_names;
template<typename T> struct FinalizeCachedControl : LLCachedControl<T>
{
    FinalizeCachedControl(LLControlGroup& group, const char* name, const T& value)
        : LLCachedControl<T>(group, std::string("FinalizePolicyTest.") + name, value)
    { control_names.emplace_back(std::string("FinalizePolicyTest.") + name); }
};
template<typename T> void set(const char* name, const T& value)
{
    finalize_settings.getControl(std::string("FinalizePolicyTest.") + name)->setValue(LLSD(value));
}
bool finalize_cube_snapshot = false;
U32 finalize_frame = 7;
struct FinalizeWindow
{
    LLRect scaled{100, 230, 260, 110};
    LLRect getWorldViewRectScaled() const { return scaled; }
} finalize_window;

// Named CPU input/presence doubles are never cast to viewer or Metal objects.
// They expose an authored descriptor and record the callback's requested
// pixel; real GPU bytes/leases are checked by almetal_finalize_admission_test.
struct FinalizeSource
{
    ALMetalRenderTargetDescriptor dimensions;
    FinalizeSource() { dimensions.width = 8; dimensions.height = 6; }
    const ALMetalRenderTargetDescriptor& descriptor() const { return dimensions; }
};
struct FinalizePixelReader
{
    unsigned reads = 0;
    U32 x = 0, y = 0;
    bool succeeds = true;
    bool reference_succeeds = true;
    unsigned reference_copies = 0;
    std::unique_ptr<FinalizeSource> capturePresentationReference(
        const FinalizeSource& source, std::string* error)
    {
        ++reference_copies;
        if (!reference_succeeds)
        {
            *error = "Authored copy admission failure";
            return nullptr;
        }
        auto result = std::make_unique<FinalizeSource>();
        result->dimensions = source.descriptor();
        return result;
    }
    bool readSceneColorPixel(const FinalizeSource&, U32 px, U32 py,
                            F32* pixel, std::string*)
    {
        ++reads; x = px; y = py;
        pixel[0] = 4.f; pixel[1] = 2.f; pixel[2] = 1.f; pixel[3] = .25f;
        return succeeds;
    }
};
struct FinalizePolicyPipeline
{
    using LensDistortFit = LLPipeline::LensDistortFit;
    using DepthOfFieldConfig = LLPipeline::DepthOfFieldConfig;
    using scene_pixel_cb_t = LLPipeline::scene_pixel_cb_t;
    struct MetalWorldSimpleCapture
    {
        bool hdr = true, device = true;
        std::unique_ptr<int> exposure_target;
        std::unique_ptr<FinalizeSource> reference_still_target;
        std::unique_ptr<int> lens_flare_state_target;
        U32 exposure_frame = 7, continuous_world_frame = 7;
        U64 exposure_epoch = 7;
        U32 lens_flare_state_frame = 7;
        U64 lens_flare_state_epoch = 7;
        U32 display_post_frame = 7, depth_of_field_frame = 7;
        std::unique_ptr<FinalizePixelReader> renderer = std::make_unique<FinalizePixelReader>();
        std::unique_ptr<FinalizeSource> opaque_post_deferred_target = std::make_unique<FinalizeSource>();
        std::unique_ptr<FinalizeSource> display_post_target = std::make_unique<FinalizeSource>();
    };
    std::unique_ptr<MetalWorldSimpleCapture> mMetalWorldSimpleCapture = std::make_unique<MetalWorldSimpleCapture>();
    std::unique_ptr<MetalWorldSimpleCapture> mMetalProbeWorldSimpleCapture;
    LensDistortFit mLensDistortFit;
    bool mReferenceStillWanted = false;
    bool sCutoutRender = false, main = true, hdr = true, suppress = false, scope_eligible = true;
    bool mMetalHeroWorldCaptureActive = false, mMetalReflectionWorldCaptureActive = false;
    bool mMetalExposureHistoryReady = false;
    U64 mMetalExposureHistoryEpoch = 7;
    U64 mMetalLensFlareHistoryEpoch = 7;
    bool mMetalLensFlareHistoryReady = false;
    int mMainRT = 0, other_rt = 0;
    int* mRT = &mMainRT;
    scene_pixel_cb_t mScenePixelCallback;
    S32 mScenePixelX = 0, mScenePixelY = 0;
    bool mScenePixelPending = false;
    bool isMainWorldPass() const { return main; }
    bool hdrEnabled() const { return hdr; }
    bool suppressesLensAndPrintEffects() const { return suppress; }
    bool canCaptureMetalScopeSample(bool) const { return scope_eligible; }
    bool consumeReferenceStillRequest();
    std::unique_ptr<FinalizeSource> captureReferenceStill(const FinalizeSource& source);
    void publishReferenceStill(std::unique_ptr<FinalizeSource> pending);
    bool hasReferenceStill() const;
    void clearReferenceStill();
    F32 lensDistortionFitScale(const LensDistortFit::Inputs& inputs);
    bool configureMetalPresentationLens(ALMetalWorldPresentationScene& presentation);
    bool wantsMetalDepthOfField(const DepthOfFieldConfig& dof) const;
    bool metalScopeSourceCurrent(U32, U32) const;
    void updateNativeExposureReadiness();
    void resetExposureHistory();
    void clearLensFlareState();
    void requestScenePixel(S32, S32, scene_pixel_cb_t);
    void serviceScenePixelProbe();
    static bool worldViewUV(S32, S32, F32&, F32&);
};

#pragma push_macro("LLPipeline")
#pragma push_macro("LLCachedControl")
#pragma push_macro("gSavedSettings")
#pragma push_macro("gCubeSnapshot")
#pragma push_macro("gFrameCount")
#pragma push_macro("gViewerWindow")
#pragma push_macro("ALMetalRenderTarget")
#define LLPipeline FinalizePolicyPipeline
#define LLCachedControl FinalizeCachedControl
#define gSavedSettings finalize_settings
#define gCubeSnapshot finalize_cube_snapshot
#define gFrameCount finalize_frame
#define gViewerWindow (&finalize_window)
#define ALMetalRenderTarget FinalizeSource
#include "../pipeline_finalize_config.inl"
#pragma pop_macro("ALMetalRenderTarget")
#pragma pop_macro("gViewerWindow")
#pragma pop_macro("gFrameCount")
#pragma pop_macro("gCubeSnapshot")
#pragma pop_macro("gSavedSettings")
#pragma pop_macro("LLCachedControl")
#pragma pop_macro("LLPipeline")

struct FinalizeCase
{
    bool classic = LLRender::sClassicMode;
    FinalizePolicyPipeline pipeline;
    FinalizeCase()
    {
        LLRender::sClassicMode = false;
        finalize_cube_snapshot = false; finalize_frame = 7;
        finalize_window = FinalizeWindow{};
        // Instantiate the real static control caches before resetting them.
        ALMetalWorldPresentationScene presentation;
        presentation.output_width = presentation.output_height = 32;
        pipeline.configureMetalPresentationLens(presentation);
        pipeline.updateNativeExposureReadiness();
        for (const auto& name : control_names)
            finalize_settings.getControl(name)->resetToDefault(true);
    }
    ~FinalizeCase()
    {
        LLRender::sClassicMode = classic;
        finalize_cube_snapshot = false;
        for (const auto& name : control_names)
            finalize_settings.getControl(name)->resetToDefault(true);
    }
};
}

TEST_CASE("Native reference request is consumed once at an ordinary copy attempt",
          "[native-finalize-policy]")
{
    FinalizeCase f; auto& p = f.pipeline;
    CHECK_FALSE(p.hasReferenceStill());
    p.mReferenceStillWanted = true;
    p.sCutoutRender = true;
    CHECK_FALSE(p.consumeReferenceStillRequest()); CHECK(p.mReferenceStillWanted);
    p.sCutoutRender = false; finalize_cube_snapshot = true;
    CHECK_FALSE(p.consumeReferenceStillRequest()); CHECK(p.mReferenceStillWanted);
    finalize_cube_snapshot = false;
    CHECK(p.consumeReferenceStillRequest());
    CHECK_FALSE(p.mReferenceStillWanted);
    // Allocation/encoding/batch failure after this attempt cannot re-arm it.
    CHECK_FALSE(p.consumeReferenceStillRequest());
    p.mMetalWorldSimpleCapture->reference_still_target = std::make_unique<FinalizeSource>();
    CHECK(p.hasReferenceStill());
    const auto* held_owner = p.mMetalWorldSimpleCapture->reference_still_target.get();
    CHECK_FALSE(p.consumeReferenceStillRequest());
    CHECK(p.mMetalWorldSimpleCapture->reference_still_target.get() == held_owner);
    // Clean plates suppress lens/print effects, not the original pre-print
    // reference capture. Offscreen requests obey the same one-shot policy.
    p.suppress = true; p.mReferenceStillWanted = true;
    CHECK(p.consumeReferenceStillRequest()); CHECK_FALSE(p.mReferenceStillWanted);
    CHECK(p.mMetalWorldSimpleCapture->reference_still_target.get() == held_owner);
    p.suppress = false;
    p.mReferenceStillWanted = true; p.clearReferenceStill();
    CHECK_FALSE(p.hasReferenceStill()); CHECK_FALSE(p.mReferenceStillWanted);
}

TEST_CASE("Native lens preflight solves changed viewport and settings before presentation",
          "[native-finalize-policy]")
{
    FinalizeCase f;
    set("RenderLensDistortionAmount", 1.f);
    ALMetalWorldPresentationScene scene;
    scene.output_width = scene.output_height = 32;
    REQUIRE(f.pipeline.configureMetalPresentationLens(scene));
    CHECK(scene.lens_distortion_k[0] == -.2f);
    CHECK(scene.lens_distortion_scale == Catch::Approx(1.f / .95f).epsilon(1e-6));
    const auto square = f.pipeline.mLensDistortFit.inputs;
    CHECK(square.axis_x == 1.f); CHECK(square.axis_y == 1.f);
    const F32 first_scale = scene.lens_distortion_scale;
    REQUIRE(f.pipeline.configureMetalPresentationLens(scene));
    CHECK(scene.lens_distortion_scale == first_scale);
    scene.output_width = 64;
    REQUIRE(f.pipeline.configureMetalPresentationLens(scene));
    CHECK(f.pipeline.mLensDistortFit.inputs.axis_x == 2.f);
    CHECK(f.pipeline.mLensDistortFit.inputs.axis_y == 1.f);
    set("RenderLensDistortionFit", 0);
    REQUIRE(f.pipeline.configureMetalPresentationLens(scene));
    CHECK(scene.lens_distortion_scale == 1.f);
    const auto held = f.pipeline.mLensDistortFit.inputs;
    f.pipeline.suppress = true;
    REQUIRE(f.pipeline.configureMetalPresentationLens(scene));
    CHECK(scene.lens_distortion_amount == 0.f);
    CHECK(f.pipeline.mLensDistortFit.inputs == held);
    scene.output_width = 0;
    CHECK_FALSE(f.pipeline.configureMetalPresentationLens(scene));
}

TEST_CASE("Native reference staging preserves a committed still until successful replacement",
          "[native-finalize-policy]")
{
    FinalizeCase f; auto& p = f.pipeline; auto& c = *p.mMetalWorldSimpleCapture;
    FinalizeSource source;
    c.reference_still_target = std::make_unique<FinalizeSource>();
    const auto* previous = c.reference_still_target.get();
    c.renderer->reference_succeeds = false;
    p.mReferenceStillWanted = true;
    auto pending = p.captureReferenceStill(source);
    CHECK_FALSE(pending); CHECK_FALSE(p.mReferenceStillWanted);
    CHECK(c.renderer->reference_copies == 1);
    CHECK(c.reference_still_target.get() == previous);
    p.publishReferenceStill(std::move(pending));
    CHECK(c.reference_still_target.get() == previous);

    // A staged copy is not publication. Cancellation drops only pending.
    c.reference_still_target = std::make_unique<FinalizeSource>();
    previous = c.reference_still_target.get();
    c.renderer->reference_succeeds = true; p.mReferenceStillWanted = true;
    pending = p.captureReferenceStill(source); REQUIRE(pending);
    CHECK(c.reference_still_target.get() == previous);
    pending.reset(); p.publishReferenceStill(nullptr);
    CHECK(c.reference_still_target.get() == previous);
    CHECK_FALSE(p.mReferenceStillWanted);
    p.mReferenceStillWanted = true;
    pending = p.captureReferenceStill(source); REQUIRE(pending);
    const auto* replacement = pending.get();
    p.publishReferenceStill(std::move(pending));
    CHECK(c.reference_still_target.get() == replacement);

    // Temporary cutout/probe geometry cannot invalidate a held ordinary
    // reference. An ordinary size change drops it even without a new grab.
    source.dimensions.width = 16;
    previous = c.reference_still_target.get(); p.mReferenceStillWanted = true;
    p.sCutoutRender = true; CHECK_FALSE(p.captureReferenceStill(source));
    CHECK(c.reference_still_target.get() == previous); CHECK(p.mReferenceStillWanted);
    p.sCutoutRender = false; finalize_cube_snapshot = true;
    CHECK_FALSE(p.captureReferenceStill(source));
    CHECK(c.reference_still_target.get() == previous); CHECK(p.mReferenceStillWanted);
    finalize_cube_snapshot = false; p.mReferenceStillWanted = false;
    CHECK_FALSE(p.captureReferenceStill(source)); CHECK_FALSE(p.hasReferenceStill());
    auto reader = std::move(c.renderer); p.mReferenceStillWanted = true;
    CHECK_FALSE(p.captureReferenceStill(source)); CHECK(p.mReferenceStillWanted);
    c.renderer = std::move(reader);
}

TEST_CASE("Native DoF admission preserves both display modes and ordinary view guards",
          "[native-finalize-policy]")
{
    FinalizeCase f; auto& p = f.pipeline;
    LLPipeline::DepthOfFieldConfig dof; dof.enabled = true;
    for (bool hdr : {false, true})
    {
        p.mMetalWorldSimpleCapture->hdr = hdr;
        CHECK(p.wantsMetalDepthOfField(dof));
        p.main = false; CHECK_FALSE(p.wantsMetalDepthOfField(dof)); p.main = true;
        p.mRT = &p.other_rt; CHECK_FALSE(p.wantsMetalDepthOfField(dof)); p.mRT = &p.mMainRT;
        p.sCutoutRender = true; CHECK_FALSE(p.wantsMetalDepthOfField(dof)); p.sCutoutRender = false;
        finalize_cube_snapshot = true; CHECK_FALSE(p.wantsMetalDepthOfField(dof)); finalize_cube_snapshot = false;
    }
    dof.enabled = false; CHECK_FALSE(p.wantsMetalDepthOfField(dof));
    dof.enabled = true; p.mMetalWorldSimpleCapture.reset(); CHECK_FALSE(p.wantsMetalDepthOfField(dof));
}

TEST_CASE("Native scene pixel callbacks rearm and retain lower-row scaled coordinates",
          "[native-finalize-policy]")
{
    FinalizeCase f; auto& p = f.pipeline;
    auto& reader = *p.mMetalWorldSimpleCapture->renderer;
    unsigned calls = 0;
    p.requestScenePixel(140, 170, [&](const LLColor3& value)
    {
        ++calls; CHECK(value == LLColor3(4.f, 2.f, 1.f));
        p.requestScenePixel(260, 230, [&](const LLColor3&) { ++calls; });
    });
    p.serviceScenePixelProbe();
    CHECK(calls == 1); CHECK(reader.x == 2); CHECK(reader.y == 3);
    CHECK(p.mScenePixelPending);
    p.serviceScenePixelProbe();
    CHECK(calls == 2); CHECK(reader.x == 7); CHECK(reader.y == 5);
    CHECK_FALSE(p.mScenePixelPending);
    p.serviceScenePixelProbe(); CHECK(calls == 2); CHECK(reader.reads == 2);
    p.requestScenePixel(140, 170, [&](const LLColor3&) { ++calls; });
    finalize_cube_snapshot = true; p.serviceScenePixelProbe();
    CHECK(p.mScenePixelPending); CHECK(reader.reads == 2);
    finalize_cube_snapshot = false;
    p.mMetalWorldSimpleCapture->continuous_world_frame = 6;
    p.serviceScenePixelProbe();
    CHECK_FALSE(p.mScenePixelPending); CHECK(calls == 2); CHECK(reader.reads == 2);
    p.mMetalWorldSimpleCapture->continuous_world_frame = 7;
    reader.succeeds = false;
    p.requestScenePixel(140, 170, [&](const LLColor3&) { ++calls; });
    p.serviceScenePixelProbe();
    CHECK_FALSE(p.mScenePixelPending); CHECK(calls == 2); CHECK(reader.reads == 3);
    p.requestScenePixel(99, 170, [&](const LLColor3&) { ++calls; });
    p.serviceScenePixelProbe(); CHECK_FALSE(p.mScenePixelPending); CHECK(reader.reads == 3);
}

TEST_CASE("Native scope source admission keeps SDR and HDR with exact geometry and generations",
          "[native-finalize-policy]")
{
    FinalizeCase f; auto& p = f.pipeline; auto& c = *p.mMetalWorldSimpleCapture;
    for (bool hdr : {false, true})
    {
        c.hdr = hdr;
        CHECK(p.metalScopeSourceCurrent(8, 6));
        CHECK_FALSE(p.metalScopeSourceCurrent(16, 12));
        c.display_post_frame = 6; CHECK_FALSE(p.metalScopeSourceCurrent(8, 6)); c.display_post_frame = 7;
        c.depth_of_field_frame = 6; CHECK_FALSE(p.metalScopeSourceCurrent(8, 6)); c.depth_of_field_frame = 7;
        c.device = false; CHECK_FALSE(p.metalScopeSourceCurrent(8, 6)); c.device = true;
        p.scope_eligible = false; CHECK_FALSE(p.metalScopeSourceCurrent(8, 6)); p.scope_eligible = true;
    }
    c.display_post_target.reset(); CHECK_FALSE(p.metalScopeSourceCurrent(8, 6));
    p.mMetalWorldSimpleCapture.reset(); CHECK_FALSE(p.metalScopeSourceCurrent(8, 6));
}

TEST_CASE("Native exposure cleanup keeps fixed EV and original HDR cutout boundaries",
          "[native-finalize-policy]")
{
    FinalizeCase f; auto& p = f.pipeline; auto& c = *p.mMetalWorldSimpleCapture;
    c.exposure_target = std::make_unique<int>(1);
    p.updateNativeExposureReadiness(); CHECK(p.mMetalExposureHistoryReady);
    set("RenderDynamicExposureEnabled", false);
    p.updateNativeExposureReadiness();
    CHECK_FALSE(c.exposure_target); CHECK(c.exposure_epoch == 0);
    CHECK(c.exposure_frame == std::numeric_limits<U32>::max()); CHECK_FALSE(p.mMetalExposureHistoryReady);
    c.exposure_target = std::make_unique<int>(1); c.exposure_epoch = 7; c.exposure_frame = 7;
    set("RenderDynamicExposureEnabled", true);
    p.updateNativeExposureReadiness(); CHECK(p.mMetalExposureHistoryReady);
    LLRender::sClassicMode = true;
    p.updateNativeExposureReadiness(); CHECK_FALSE(c.exposure_target); CHECK_FALSE(p.mMetalExposureHistoryReady);
    LLRender::sClassicMode = false; set("RenderDynamicExposureEnabled", false);
    c.exposure_target = std::make_unique<int>(1); c.exposure_epoch = 7;
    p.hdr = false; p.updateNativeExposureReadiness(); CHECK(c.exposure_target); CHECK(c.exposure_epoch == 7);
    p.hdr = true; p.sCutoutRender = true;
    p.updateNativeExposureReadiness(); CHECK(c.exposure_target); CHECK(c.exposure_epoch == 7);
}

TEST_CASE("Explicit native history reset invalidates both owners without changing a held reference",
          "[native-finalize-policy]")
{
    FinalizeCase f; auto& p = f.pipeline;
    p.mMetalProbeWorldSimpleCapture = std::make_unique<FinalizePolicyPipeline::MetalWorldSimpleCapture>();
    for (auto* capture : {p.mMetalWorldSimpleCapture.get(), p.mMetalProbeWorldSimpleCapture.get()})
        capture->exposure_target = std::make_unique<int>(1);
    auto& main = *p.mMetalWorldSimpleCapture;
    main.lens_flare_state_target = std::make_unique<int>(1);
    main.reference_still_target = std::make_unique<FinalizeSource>();
    const auto* held = main.reference_still_target.get();
    p.mMetalExposureHistoryReady = p.mMetalLensFlareHistoryReady = true;
    p.mMetalExposureHistoryEpoch = p.mMetalLensFlareHistoryEpoch = std::numeric_limits<U64>::max();
    p.mReferenceStillWanted = true;
    p.hdr = false; p.sCutoutRender = true; p.suppress = true;
    p.resetExposureHistory();
    CHECK(p.mMetalExposureHistoryEpoch == 1);
    CHECK(p.mMetalLensFlareHistoryEpoch == 1);
    CHECK_FALSE(p.mMetalExposureHistoryReady); CHECK_FALSE(p.mMetalLensFlareHistoryReady);
    for (auto* capture : {p.mMetalWorldSimpleCapture.get(), p.mMetalProbeWorldSimpleCapture.get()})
    {
        CHECK_FALSE(capture->exposure_target); CHECK(capture->exposure_epoch == 0);
        CHECK(capture->exposure_frame == std::numeric_limits<U32>::max());
    }
    CHECK_FALSE(main.lens_flare_state_target); CHECK(main.lens_flare_state_epoch == 0);
    CHECK(main.lens_flare_state_frame == std::numeric_limits<U32>::max());
    CHECK(main.reference_still_target.get() == held);
    CHECK(p.mReferenceStillWanted);
}
