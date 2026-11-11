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
    explicit FinalizeSource(ALMetalPixelFormat format = ALMetalPixelFormat::rgba8_unorm)
    {
        dimensions.width = 8; dimensions.height = 6;
        dimensions.colors[0].format = format;
    }
    const ALMetalRenderTargetDescriptor& descriptor() const { return dimensions; }
};
struct FinalizeHistoryTexture
{
    ALMetalTextureDescriptor dimensions;
    const ALMetalTextureDescriptor& descriptor() const { return dimensions; }
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
        // The renderer's original-mapped contract is RGB10_A2 for either
        // display source format. Exact conversion is tested by its GPU owner.
        result->dimensions.colors[0].format = ALMetalPixelFormat::rgb10a2_unorm;
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
// Named observation of the immediate-stream boundary only. This does not
// encode, submit or claim a native GPU draw; actual policy bodies call it.
struct FinalizeFlushBoundary
{
    unsigned calls = 0;
    std::function<void()> observe;
    void flush()
    {
        ++calls;
        if (observe) observe();
    }
} finalize_flush;
struct FinalizePolicyPipeline
{
    using LensDistortFit = LLPipeline::LensDistortFit;
    using DepthOfFieldConfig = LLPipeline::DepthOfFieldConfig;
    using scene_pixel_cb_t = LLPipeline::scene_pixel_cb_t;
    struct MetalWorldSimpleCapture
    {
        bool hdr = true, device = true;
        U32 width = 8, height = 6;
        std::unique_ptr<int> impostor_ssr_history_target;
        struct Reflections
        {
            const FinalizeHistoryTexture* scene_color = nullptr;
            const FinalizeHistoryTexture* scene_depth = nullptr;
        } impostor_screen_space_reflections;
        std::unique_ptr<int> exposure_target;
        std::unique_ptr<FinalizeSource> reference_still_target;
        std::unique_ptr<int> lens_flare_state_target;
        U32 exposure_frame = 7, continuous_world_frame = 7;
        U64 exposure_epoch = 7;
        U32 lens_flare_state_frame = 7;
        U64 lens_flare_state_epoch = 7;
        U32 display_post_frame = 7, depth_of_field_frame = 7;
        std::unique_ptr<FinalizePixelReader> renderer = std::make_unique<FinalizePixelReader>();
        std::unique_ptr<FinalizeSource> opaque_post_deferred_target =
            std::make_unique<FinalizeSource>(ALMetalPixelFormat::rgba16_float);
        std::unique_ptr<FinalizeSource> display_post_target = std::make_unique<FinalizeSource>();
    };
    std::unique_ptr<MetalWorldSimpleCapture> mMetalWorldSimpleCapture = std::make_unique<MetalWorldSimpleCapture>();
    std::unique_ptr<MetalWorldSimpleCapture> mMetalProbeWorldSimpleCapture;
    ALRenderResourceInputs mResourceInputs;
    ALRenderResourcePlan mResourcePlan;
    LensDistortFit mLensDistortFit;
    bool mReferenceStillWanted = false;
    bool sCutoutRender = false, main = true, hdr = true, suppress = false, scope_eligible = true;
    bool scopes_open = true, presentation_eligible = true, mMetalWorldSnapshotRequested = false;
    bool mMetalHeroWorldCaptureActive = false, mMetalReflectionWorldCaptureActive = false;
    bool mMetalExposureHistoryReady = false;
    U64 mMetalExposureHistoryEpoch = 7;
    U64 mMetalLensFlareHistoryEpoch = 7;
    bool mMetalLensFlareHistoryReady = false;
    struct ViewInput { U32 bloomMipCount = 6; } mMainRT, other_rt;
    ViewInput* mRT = &mMainRT;
    scene_pixel_cb_t mScenePixelCallback;
    S32 mScenePixelX = 0, mScenePixelY = 0;
    bool mScenePixelPending = false;
    bool isMainWorldPass() const { return main; }
    bool hdrEnabled() const { return hdr; }
    bool suppressesLensAndPrintEffects() const { return suppress; }
    bool canCaptureMetalWorldPresentation() const { return presentation_eligible; }
    bool canCaptureMetalScopeSample(bool) const { return scopes_open && scope_eligible && presentation_eligible; }
    bool consumeReferenceStillRequest();
    std::unique_ptr<FinalizeSource> captureReferenceStill(const FinalizeSource& source);
    void publishReferenceStill(std::unique_ptr<FinalizeSource> pending);
    bool hasReferenceStill() const;
    void clearReferenceStill();
    F32 lensDistortionFitScale(const LensDistortFit::Inputs& inputs);
    bool configureMetalPresentationLens(ALMetalWorldPresentationScene& presentation);
    bool wantsMetalDepthOfField(const DepthOfFieldConfig& dof) const;
    bool wantsMetalBloom(bool, F32, F32) const;
    bool wantsMetalScreenSpaceReflections() const;
    bool hasMetalScreenSpaceReflectionHistory() const;
    void clearSceneMap();
    void reconcileNativeHistoryState(const ALRenderResourcePlan& plan, bool hdr, bool render_sized_recreated);
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
#pragma push_macro("gGL")
#define LLPipeline FinalizePolicyPipeline
#define LLCachedControl FinalizeCachedControl
#define gSavedSettings finalize_settings
#define gCubeSnapshot finalize_cube_snapshot
#define gFrameCount finalize_frame
#define gViewerWindow (&finalize_window)
#define ALMetalRenderTarget FinalizeSource
#define gGL finalize_flush
#include "../pipeline_finalize_config.inl"
#pragma pop_macro("gGL")
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
        finalize_flush = {};
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
        finalize_flush = {};
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

TEST_CASE("Native reference retains packed storage across display format changes",
          "[native-finalize-policy][native-reference-format-policy]")
{
    FinalizeCase f; auto& p = f.pipeline; auto& c = *p.mMetalWorldSimpleCapture;
    FinalizeSource source;
    source.dimensions.colors[0].format = ALMetalPixelFormat::rgba8_unorm;
    p.mReferenceStillWanted = true;
    auto pending = p.captureReferenceStill(source);
    REQUIRE(pending);
    CHECK(pending->descriptor().colors[0].format == ALMetalPixelFormat::rgb10a2_unorm);
    const auto* packed = pending.get();
    p.publishReferenceStill(std::move(pending));
    for (const auto format : {ALMetalPixelFormat::rgba8_unorm,
                              ALMetalPixelFormat::rgb10a2_unorm,
                              ALMetalPixelFormat::rgba8_unorm})
    {
        source.dimensions.colors[0].format = format;
        p.hdr = c.hdr = format == ALMetalPixelFormat::rgb10a2_unorm;
        CHECK_FALSE(p.captureReferenceStill(source));
        CHECK(p.hasReferenceStill());
        CHECK(c.reference_still_target.get() == packed);
        CHECK(c.renderer->reference_copies == 1);
    }
    c.renderer->reference_succeeds = false; p.mReferenceStillWanted = true;
    CHECK_FALSE(p.captureReferenceStill(source));
    CHECK_FALSE(p.mReferenceStillWanted);
    CHECK(c.reference_still_target.get() == packed);
    source.dimensions.height += 1;
    CHECK_FALSE(p.captureReferenceStill(source));
    CHECK_FALSE(p.hasReferenceStill());
}

TEST_CASE("Native HDR bloom follows presentation readiness independently of scope enable",
          "[native-finalize-policy][native-bloom-policy]")
{
    FinalizeCase f; auto& p = f.pipeline;
    p.scopes_open = false;
    CHECK(p.wantsMetalBloom(true, 1.f, 0.f));
    CHECK(p.wantsMetalBloom(true, 0.f, 1.f));
    CHECK_FALSE(p.wantsMetalBloom(true, 0.f, 0.f));
    CHECK_FALSE(p.wantsMetalBloom(false, 1.f, 1.f));
    p.presentation_eligible = false;
    CHECK_FALSE(p.wantsMetalBloom(true, 1.f, 1.f)); p.presentation_eligible = true;
    p.scopes_open = true;
    CHECK(p.wantsMetalBloom(true, 1.f, 0.f));
    p.mMainRT.bloomMipCount = 2;
    CHECK_FALSE(p.wantsMetalBloom(true, 1.f, 1.f));
    p.mMainRT.bloomMipCount = 3;
    CHECK(p.wantsMetalBloom(true, 1.f, 1.f));
    p.mMetalWorldSimpleCapture->hdr = false;
    CHECK_FALSE(p.wantsMetalBloom(true, 1.f, 1.f)); p.mMetalWorldSimpleCapture->hdr = true;
    p.main = false; CHECK_FALSE(p.wantsMetalBloom(true, 1.f, 1.f)); p.main = true;
    p.mRT = &p.other_rt; CHECK_FALSE(p.wantsMetalBloom(true, 1.f, 1.f)); p.mRT = &p.mMainRT;
    p.sCutoutRender = p.mMetalWorldSnapshotRequested = true;
    CHECK_FALSE(p.wantsMetalBloom(true, 1.f, 1.f));
    p.sCutoutRender = p.mMetalWorldSnapshotRequested = false;
    p.mMetalWorldSimpleCapture.reset(); CHECK_FALSE(p.wantsMetalBloom(true, 1.f, 1.f));
}

TEST_CASE("Native SSR isolation retires warm history until a new ordinary publication",
          "[native-finalize-policy][native-ssr-isolation-policy]")
{
    FinalizeCase f; auto& p = f.pipeline;
    ALRenderResourceInputs inputs;
    inputs.mWidth = 8; inputs.mHeight = 6;
    inputs.mSSR = inputs.mProbes = true;
    p.mResourcePlan = ALRenderResourcePlan::make(inputs);
    REQUIRE(p.mResourcePlan.usesScreenSpaceReflections());
    FinalizeHistoryTexture color, depth;
    color.dimensions.width = depth.dimensions.width = 8;
    color.dimensions.height = depth.dimensions.height = 6;
    color.dimensions.format = ALMetalPixelFormat::rg11b10_float;
    depth.dimensions.format = ALMetalPixelFormat::r32_float;
    auto warm = [&](FinalizePolicyPipeline::MetalWorldSimpleCapture& capture)
    {
        capture.impostor_ssr_history_target = std::make_unique<int>(17);
        capture.impostor_screen_space_reflections = {&color, &depth};
    };
    auto& main_capture = *p.mMetalWorldSimpleCapture;
    p.mMetalProbeWorldSimpleCapture =
        std::make_unique<FinalizePolicyPipeline::MetalWorldSimpleCapture>();
    warm(main_capture); warm(*p.mMetalProbeWorldSimpleCapture);
    CHECK(p.wantsMetalScreenSpaceReflections());
    CHECK(p.hasMetalScreenSpaceReflectionHistory());
    // This is the actual isolation caller order: clear BEFORE setting cutout.
    p.clearSceneMap();
    for (const auto* capture : {p.mMetalWorldSimpleCapture.get(), p.mMetalProbeWorldSimpleCapture.get()})
    {
        CHECK_FALSE(capture->impostor_ssr_history_target);
        CHECK_FALSE(capture->impostor_screen_space_reflections.scene_color);
        CHECK_FALSE(capture->impostor_screen_space_reflections.scene_depth);
    }
    p.sCutoutRender = true;
    CHECK_FALSE(p.wantsMetalScreenSpaceReflections());
    CHECK_FALSE(p.hasMetalScreenSpaceReflectionHistory());
    // Returning to the same-sized ordinary view must not reveal pre-cutout
    // owners. Without a successful producer publication, repeated reads miss.
    p.sCutoutRender = false;
    CHECK(p.wantsMetalScreenSpaceReflections());
    CHECK_FALSE(p.hasMetalScreenSpaceReflectionHistory());
    CHECK_FALSE(p.hasMetalScreenSpaceReflectionHistory());
    // Explicitly authored successful producer output is an input to this CPU
    // policy; the GPU companion checks the real publication/sample contract.
    warm(main_capture);
    CHECK(p.hasMetalScreenSpaceReflectionHistory());
    p.sCutoutRender = true;
    CHECK_FALSE(p.hasMetalScreenSpaceReflectionHistory()); p.sCutoutRender = false;
    finalize_cube_snapshot = true;
    CHECK_FALSE(p.wantsMetalScreenSpaceReflections()); finalize_cube_snapshot = false;
    inputs.mSSR = false; p.mResourcePlan = ALRenderResourcePlan::make(inputs);
    CHECK_FALSE(p.wantsMetalScreenSpaceReflections());
    inputs.mSSR = true; inputs.mProbes = false;
    p.mResourcePlan = ALRenderResourcePlan::make(inputs);
    CHECK_FALSE(p.wantsMetalScreenSpaceReflections());
    color.dimensions.width = 7;
    CHECK_FALSE(p.hasMetalScreenSpaceReflectionHistory()); color.dimensions.width = 8;
    depth.dimensions.format = ALMetalPixelFormat::rgba8_unorm;
    CHECK_FALSE(p.hasMetalScreenSpaceReflectionHistory()); depth.dimensions.format = ALMetalPixelFormat::r32_float;
    p.hdr = false;
    CHECK_FALSE(p.hasMetalScreenSpaceReflectionHistory());
    color.dimensions.format = ALMetalPixelFormat::rgba8_unorm;
    CHECK(p.hasMetalScreenSpaceReflectionHistory());
    p.clearSceneMap(); p.clearSceneMap();
    CHECK_FALSE(p.hasMetalScreenSpaceReflectionHistory());
    p.mMetalWorldSimpleCapture.reset(); p.mMetalProbeWorldSimpleCapture.reset();
    p.clearSceneMap();
    CHECK_FALSE(p.hasMetalScreenSpaceReflectionHistory());
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

TEST_CASE("Native resource history invalidation preserves the original reconciliation event matrix",
          "[native-finalize-policy][native-resource-history]")
{
    using Inputs = ALRenderResourceInputs;
    using Plan = ALRenderResourcePlan;
    Inputs ordinary; ordinary.mWidth = 96; ordinary.mHeight = 64;
    ordinary.mSSR = ordinary.mProbes = true;
    struct Event
    {
        const char* name;
        Inputs before, after;
        bool initial = false, recreated = false;
        bool scene_reset = false, exposure_reset = false;
    };
    std::vector<Event> events;
    const auto add = [&](const char* name, const auto& change, bool scene_reset,
                         bool recreated = false)
    {
        Event event{name, ordinary, ordinary}; change(event);
        event.recreated = recreated; event.scene_reset = scene_reset;
        events.push_back(event);
    };
    const auto unchanged = [](Event&) {};
    add("first resources", [](Event& e) { e.initial = e.exposure_reset = true; }, true);
    add("first resources without SSR", [](Event& e)
        { e.initial = e.exposure_reset = true; e.after.mSSR = false; }, false);
    add("unchanged", unchanged, false);
    add("width changes", [](Event& e) { e.after.mWidth += 16; }, true);
    add("height changes", [](Event& e) { e.after.mHeight += 16; }, true);
    add("resolution divisor changes applied size", [](Event& e) { e.after.mResolutionDivisor = 2; }, true);
    add("resolution multiplier changes applied size", [](Event& e) { e.after.mResolutionMultiplier = .5f; }, true);
    add("different raw dimensions preserve applied size", [](Event& e)
        { e.after.mWidth *= 2; e.after.mHeight *= 2; e.after.mResolutionDivisor = 2; }, false);
    add("integer division preserves applied size", [](Event& e)
        { e.before.mResolutionDivisor = e.after.mResolutionDivisor = 3; ++e.after.mWidth; ++e.after.mHeight; }, false);
    add("active divisor takes precedence over multiplier", [](Event& e)
        { e.before.mResolutionDivisor = e.after.mResolutionDivisor = 2; e.after.mResolutionMultiplier = .5f; }, false);
    // The old fixed exposure/flare descriptors did not change on HDR or resize.
    // The separate existing HDR settings invalidation is not part of this event.
    add("SSR storage HDR format changes", [](Event& e) { e.after.mHDR = false; }, true);
    add("SSR storage depth format changes", [](Event& e) { e.after.mReverseZ = false; }, true);
    add("shadow-only depth format does not change SSR", [](Event& e) { e.after.mShadowDepth32F = true; }, false);
    add("GBuffer normal format does not change SSR", [](Event& e) { e.after.mGBufferNormHDR = false; }, false);
    add("emissive attachment does not change SSR", [](Event& e) { e.after.mEmissive = false; }, false);
    add("bloom setting does not change histories", [](Event& e) { e.after.mBloomMips = 7; }, false);
    add("disable SSR", [](Event& e) { e.after.mSSR = false; }, false);
    add("enable SSR", [](Event& e) { e.before.mSSR = false; }, true);
    add("SSR without probes", [](Event& e)
        { e.before.mProbes = e.after.mProbes = false; e.before.mSSR = false; }, false);
    add("disable probes", [](Event& e) { e.after.mProbes = false; }, false);
    add("enable probes", [](Event& e) { e.before.mProbes = false; }, true);
    add("same-key render-sized recreation", unchanged, true, true);
    add("render-sized recreation with SSR off", [](Event& e)
        { e.before.mSSR = e.after.mSSR = false; }, false, true);
    add("release and recreate", [](Event& e) { e.initial = e.exposure_reset = true; }, true);
    for (const auto& event : events)
    {
        CAPTURE(event.name);
        FinalizeCase f; auto& p = f.pipeline;
        p.mResourceInputs = event.before;
        p.mResourcePlan = event.initial ? Plan{} : Plan::make(event.before);
        const Plan after = Plan::make(event.after);
        if (!event.initial && event.before.mSSR != event.after.mSSR &&
            event.before.mProbes == event.after.mProbes)
        {
            // Logical intent still changes when no retained storage does.
            REQUIRE(Plan::changed(p.mResourcePlan, after).none());
        }
        auto& main = *p.mMetalWorldSimpleCapture;
        p.mMetalProbeWorldSimpleCapture = std::make_unique<FinalizePolicyPipeline::MetalWorldSimpleCapture>();
        auto& probe = *p.mMetalProbeWorldSimpleCapture;
        for (auto* capture : {&main, &probe})
        {
            capture->impostor_ssr_history_target = std::make_unique<int>(1);
            capture->exposure_target = std::make_unique<int>(2);
        }
        main.lens_flare_state_target = std::make_unique<int>(3);
        p.mMetalExposureHistoryReady = p.mMetalLensFlareHistoryReady = true;
        // Execute the actual native reconciliation policy with actual applied
        // plan metadata. The named captures model owner presence, not a GPU.
        p.reconcileNativeHistoryState(after, event.after.mHDR, event.recreated);
        CHECK(finalize_flush.calls == (event.scene_reset ? 1u : 0u));
        CHECK(!main.impostor_ssr_history_target == event.scene_reset);
        CHECK(!probe.impostor_ssr_history_target == event.scene_reset);
        CHECK(!main.exposure_target == event.exposure_reset);
        CHECK(!probe.exposure_target == event.exposure_reset);
        CHECK(!main.lens_flare_state_target == event.exposure_reset);
        CHECK(p.mMetalExposureHistoryEpoch == (event.exposure_reset ? 8u : 7u));
        CHECK(p.mMetalLensFlareHistoryEpoch == (event.exposure_reset ? 8u : 7u));
        CHECK(p.mMetalExposureHistoryReady == !event.exposure_reset);
        CHECK(p.mMetalLensFlareHistoryReady == !event.exposure_reset);
    }
}

TEST_CASE("Native scene history clearing retains the pre-cutout immediate boundary",
          "[native-finalize-policy][native-resource-flush]")
{
    FinalizeCase f; auto& p = f.pipeline;
    ALRenderResourceInputs inputs; inputs.mWidth = 8; inputs.mHeight = 6;
    inputs.mSSR = inputs.mProbes = true;
    p.mResourcePlan = ALRenderResourcePlan::make(inputs);
    p.mMetalProbeWorldSimpleCapture =
        std::make_unique<FinalizePolicyPipeline::MetalWorldSimpleCapture>();
    FinalizeHistoryTexture color, depth;
    color.dimensions.width = depth.dimensions.width = 8;
    color.dimensions.height = depth.dimensions.height = 6;
    color.dimensions.format = ALMetalPixelFormat::rg11b10_float;
    depth.dimensions.format = ALMetalPixelFormat::r32_float;
    for (auto* capture : {p.mMetalWorldSimpleCapture.get(), p.mMetalProbeWorldSimpleCapture.get()})
    {
        capture->impostor_ssr_history_target = std::make_unique<int>(1);
        capture->impostor_screen_space_reflections = {&color, &depth};
    }
    std::vector<bool> caller_cutout_at_flush;
    finalize_flush.observe = [&]
    {
        caller_cutout_at_flush.push_back(p.sCutoutRender);
        for (const auto* capture : {p.mMetalWorldSimpleCapture.get(), p.mMetalProbeWorldSimpleCapture.get()})
        {
            CHECK_FALSE(capture->impostor_ssr_history_target);
            CHECK_FALSE(capture->impostor_screen_space_reflections.scene_color);
            CHECK_FALSE(capture->impostor_screen_space_reflections.scene_depth);
        }
    };
    // Actual Scope caller sequence: native payload/owner retirement and the
    // immediate boundary occur before the caller raises cutout. This executes
    // clearSceneMap's actual body, not the viewer Scope constructor or a GPU.
    p.clearSceneMap();
    p.sCutoutRender = true;
    CHECK(finalize_flush.calls == 1);
    CHECK(caller_cutout_at_flush == std::vector<bool>{false});
    finalize_flush.observe = {};
    p.sCutoutRender = false;
    inputs.mSSR = false; p.mResourcePlan = ALRenderResourcePlan::make(inputs);
    p.clearSceneMap(); CHECK(finalize_flush.calls == 1);
    inputs.mSSR = true; inputs.mProbes = false;
    p.mResourcePlan = ALRenderResourcePlan::make(inputs);
    p.clearSceneMap(); CHECK(finalize_flush.calls == 1);
    inputs.mProbes = true; p.mResourcePlan = ALRenderResourcePlan::make(inputs);
    p.mMetalWorldSimpleCapture.reset(); p.mMetalProbeWorldSimpleCapture.reset();
    p.clearSceneMap(); CHECK(finalize_flush.calls == 2);
    p.mResourcePlan = {};
    p.clearSceneMap(); CHECK(finalize_flush.calls == 2);
}
