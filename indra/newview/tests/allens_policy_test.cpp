/**
 * @file allens_policy_test.cpp
 * @brief Actual viewer lens configuration admits native shader identities.
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 */
#include "linden_common.h"
#include "../pipeline.h"
#include "llcontrol.h"
#include "llglslshader.h"
#include "llmatrix4a.h"
#include "v3color.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
// Only viewer inputs are substituted. The configuration structs and method
// bodies are the production definitions, with real settings notifications and
// LLGLSLShader's native configure/unload lifecycle. No viewer/GPU is constructed.
LLControlGroup lens_policy_settings("Native lens policy test");
LLGLSLShader* lens_policy_program = nullptr;
std::vector<std::string> lens_policy_control_names;
void reset_controls()
{
    for (const auto& name : lens_policy_control_names)
        lens_policy_settings.getControl(name)->resetToDefault(true);
}
std::string setting(const char* name) { return std::string("LensPolicyTest.") + name; }
template<typename T> struct LensPolicyCachedControl : LLCachedControl<T>
{
    LensPolicyCachedControl(LLControlGroup& group, const char* name, const T& value)
        : LLCachedControl<T>(group, setting(name), value)
    { lens_policy_control_names.push_back(setting(name)); }
};
struct LensPolicySky
{
    using ptr_t = std::shared_ptr<LensPolicySky>;
    F32 sun_scale = 1.f, moon_scale = 2.f, ambiance = 0.f, gamma = 1.f;
    bool auto_adjust = false;
    F32 getSunScale() const { return sun_scale; }
    F32 getMoonScale() const { return moon_scale; }
    F32 getReflectionProbeAmbiance() const { return ambiance; }
    F32 getGamma() const { return gamma; }
    bool canAutoAdjust() const { return auto_adjust; }
};
struct LensPolicyEnvironment
{
    bool sun_up = true;
    LensPolicySky::ptr_t sky = std::make_shared<LensPolicySky>();
    static LensPolicyEnvironment& instance() { static LensPolicyEnvironment value; return value; }
    bool getIsSunUp() const { return sun_up; }
    LensPolicySky::ptr_t getCurrentSky() const { return sky; }
};
struct LensPolicyCamera
{
    LLMatrix4a view = LLMatrix4a::identity();
    LLMatrix4a projection = LLMatrix4a::perspective(F_PI_BY_TWO, 1.f, .1f, 100.f);
    F32 fov = F_PI_BY_TWO, zoom = 1.f;
    static LensPolicyCamera& getCurrent() { static LensPolicyCamera value; return value; }
    static LensPolicyCamera* getInstance() { return &getCurrent(); }
    const LLMatrix4a& getModelview() const { return view; }
    const LLMatrix4a& getProjection() const { return projection; }
    F32 getView() const { return fov; }
    F32 getZoomFactor() const { return zoom; }
};
struct LensPolicyBody { F32 getDiskRadius() const { return .5f; } };
struct LensPolicySkyObject
{
    LensPolicyBody body;
    const LensPolicyBody& getSun() const { return body; }
    const LensPolicyBody& getMoon() const { return body; }
};
struct LensPolicySkyPointer
{
    bool present = false;
    LensPolicySkyObject object;
    bool notNull() const { return present; }
    const LensPolicySkyObject* operator->() const { return &object; }
};
struct { LensPolicySkyPointer mVOSkyp; } lens_policy_sky;
constexpr F32 HEAVENLY_BODY_FACTOR = .1f; // Original llvosky.h angular-radius scale.
struct LensPolicyCapture
{
    std::unique_ptr<int> lens_flare_state_target;
    U32 lens_flare_state_frame = 0;
    U64 lens_flare_state_epoch = 0;
};
struct LensPolicyPipeline
{
    using LensFlareStateConfig = LLPipeline::LensFlareStateConfig;
    using LensFlareConfig = LLPipeline::LensFlareConfig;
    LensFlareStateConfig lensFlareStateConfig();
    LensFlareConfig lensFlareConfig(bool world_frame) const;
    bool suppress = false;
    bool suppressesLensAndPrintEffects() const { return suppress; }
    bool mLensFlareSunUp = true;
    bool mMetalLensFlareHistoryReady = true;
    U64 mMetalLensFlareHistoryEpoch = 1;
    LLVector4 mSunDir{0.f, 0.f, -1.f, 0.f}, mMoonDir{.5f, 0.f, -1.f, 0.f};
    LLColor4 mSunDiffuse{.5f, .25f, .75f, 1.f}, mMoonDiffuse{.25f, .5f, .125f, 1.f};
    LLVector2 mLensFlareSunUV{.5f, .5f};
    std::unique_ptr<LensPolicyCapture> mMetalWorldSimpleCapture;
};

#pragma push_macro("LLPipeline")
#pragma push_macro("LLCachedControl")
#pragma push_macro("gSavedSettings")
#pragma push_macro("gLensFlareStateProgram")
#pragma push_macro("LLEnvironment")
#pragma push_macro("LLSettingsSky")
#pragma push_macro("LLViewerCamera")
#pragma push_macro("gSky")
#define LLPipeline LensPolicyPipeline
#define LLCachedControl LensPolicyCachedControl
#define gSavedSettings lens_policy_settings
#define gLensFlareStateProgram (*lens_policy_program)
#define LLEnvironment LensPolicyEnvironment
#define LLSettingsSky LensPolicySky
#define LLViewerCamera LensPolicyCamera
#define gSky lens_policy_sky
#include "../pipeline_lens_config.inl"
#pragma pop_macro("gSky")
#pragma pop_macro("LLViewerCamera")
#pragma pop_macro("LLSettingsSky")
#pragma pop_macro("LLEnvironment")
#pragma pop_macro("gLensFlareStateProgram")
#pragma pop_macro("gSavedSettings")
#pragma pop_macro("LLCachedControl")
#pragma pop_macro("LLPipeline")

struct LensPolicyCase
{
    U32 linked = LLGLSLShader::sLinked, kept = LLGLSLShader::sKept;
    LLGLSLShader program;
    LensPolicyPipeline pipeline;
    LensPolicyCase()
    {
        REQUIRE_FALSE(gGLManager.mInited);
        LensPolicyEnvironment::instance() = LensPolicyEnvironment{};
        LensPolicyCamera::getCurrent() = LensPolicyCamera{};
        lens_policy_sky.mVOSkyp.present = false;
        lens_policy_program = &program;
        // Instantiate all real cached controls before resetting them.
        pipeline.lensFlareStateConfig(); pipeline.lensFlareConfig(true);
        reset_controls();
        lens_policy_settings.setF32(setting("RenderLensFlareStrength"), .75f);
    }
    ~LensPolicyCase()
    {
        program.unload();
        lens_policy_program = nullptr;
        reset_controls();
        LLGLSLShader::sLinked = linked; LLGLSLShader::sKept = kept;
    }
    void load()
    {
        program.mName = "Lens Flare State";
        program.mShaderFiles = {
            {"deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER},
            {"alchemy/lensFlareStateF.glsl", GL_FRAGMENT_SHADER}};
        REQUIRE(program.createShader());
        REQUIRE(program.isNativeConfigured());
        REQUIRE_FALSE(program.isComplete());
    }
};
}

TEST_CASE("Native viewer flare CPU preflight follows semantic shader load and reload",
          "[native-lens-policy]")
{
    LensPolicyCase f;
    CHECK_FALSE(f.pipeline.lensFlareStateConfig().enabled);
    CHECK_FALSE(f.pipeline.lensFlareConfig(true).enabled);
    f.load();
    CHECK(f.pipeline.lensFlareStateConfig().enabled);
    CHECK(f.pipeline.lensFlareConfig(true).enabled);
    CHECK(f.pipeline.lensFlareConfig(true).strength == .75f);
    CHECK_FALSE(f.pipeline.lensFlareConfig(false).enabled);
    f.pipeline.suppress = true;
    CHECK(f.pipeline.lensFlareStateConfig().hold);
    CHECK_FALSE(f.pipeline.lensFlareStateConfig().enabled);
    CHECK_FALSE(f.pipeline.lensFlareConfig(true).enabled);
    f.pipeline.suppress = false;
    CHECK(f.pipeline.lensFlareStateConfig().enabled);
    f.program.unload();
    CHECK_FALSE(f.pipeline.lensFlareStateConfig().enabled);
    CHECK_FALSE(f.pipeline.lensFlareConfig(true).enabled);
    f.load();
    CHECK(f.pipeline.lensFlareStateConfig().enabled);
    CHECK(f.pipeline.lensFlareConfig(true).enabled);
    lens_policy_settings.setF32(setting("RenderLensFlareStrength"), 0.f);
    CHECK_FALSE(f.pipeline.lensFlareStateConfig().enabled);
    CHECK_FALSE(f.pipeline.lensFlareConfig(true).enabled);
    lens_policy_settings.setF32(setting("RenderLensFlareStrength"), 2.f);
    CHECK(f.pipeline.lensFlareStateConfig().enabled);
    CHECK(f.pipeline.lensFlareConfig(true).strength == 1.f);
    LensPolicyEnvironment::instance().sky.reset();
    CHECK_FALSE(f.pipeline.lensFlareStateConfig().enabled);
}

TEST_CASE("Native viewer flare CPU inputs retain projection fade clamps and body history",
          "[native-lens-policy]")
{
    LensPolicyCase f; f.load();
    auto state = f.pipeline.lensFlareStateConfig();
    REQUIRE(state.enabled);
    CHECK(state.sun_uv[0] == .5f); CHECK(state.sun_uv[1] == .5f);
    CHECK(state.edge_visibility == 1.f);
    // Half the original .1 angular factor, .5 fallback disk and1.5 horizon
    // enlargement, divided by tan(FOV/2)==1. The state uses vertical UV units.
    CHECK(state.occlusion_radius == Catch::Approx(.0375f));
    lens_policy_settings.setF32(setting("RenderLensFlareOcclusionScale"), 20.f);
    lens_policy_settings.setF32(setting("RenderLensFlareFadeTime"), 0.f);
    LensPolicyCamera::getCurrent().zoom = 4.f;
    state = f.pipeline.lensFlareStateConfig();
    CHECK(state.occlusion_radius == Catch::Approx(.3f));
    CHECK(state.fade_time == .1f);
    lens_policy_settings.setF32(setting("RenderLensFlareFadeTime"), 2.f);
    CHECK(f.pipeline.lensFlareStateConfig().fade_time == 1.f);

    f.pipeline.mSunDir = LLVector4(1.2f, 0.f, -1.f, 0.f);
    state = f.pipeline.lensFlareStateConfig();
    CHECK(state.sun_uv[0] == Catch::Approx(1.1f));
    CHECK(state.edge_visibility == Catch::Approx(.5f));
    f.pipeline.mSunDir = LLVector4(0.f, 0.f, 1.f, 0.f);
    state = f.pipeline.lensFlareStateConfig();
    CHECK(state.edge_visibility == 0.f);
    CHECK(state.sun_uv[0] == Catch::Approx(1.1f)); // Behind-camera anchor holds.

    auto& capture = f.pipeline.mMetalWorldSimpleCapture;
    capture = std::make_unique<LensPolicyCapture>();
    capture->lens_flare_state_target = std::make_unique<int>(1);
    capture->lens_flare_state_frame = 12;
    capture->lens_flare_state_epoch = 1;
    LensPolicyEnvironment::instance().sun_up = false;
    f.pipeline.suppress = true;
    state = f.pipeline.lensFlareStateConfig();
    CHECK(state.hold); CHECK_FALSE(state.enabled);
    CHECK(f.pipeline.mLensFlareSunUp);
    CHECK(f.pipeline.mMetalLensFlareHistoryEpoch == 1);
    CHECK(capture->lens_flare_state_target);
    f.pipeline.suppress = false;
    state = f.pipeline.lensFlareStateConfig();
    CHECK_FALSE(state.sun_up);
    CHECK(f.pipeline.mMetalLensFlareHistoryEpoch == 2);
    CHECK_FALSE(f.pipeline.mMetalLensFlareHistoryReady);
    CHECK_FALSE(capture->lens_flare_state_target);
    CHECK(capture->lens_flare_state_frame == std::numeric_limits<U32>::max());
    CHECK(capture->lens_flare_state_epoch == 0);
    CHECK(state.sun_uv[0] == Catch::Approx(.75f));
    f.pipeline.lensFlareStateConfig();
    CHECK(f.pipeline.mMetalLensFlareHistoryEpoch == 2); // No repeated invalidation.
}

TEST_CASE("Native viewer flare CPU appearance retains authored parameter mappings",
          "[native-lens-policy]")
{
    LensPolicyCase f; f.load();
    lens_policy_settings.setF32(setting("RenderLensFlareStreakThickness"), 0.f);
    lens_policy_settings.setF32(setting("RenderLensFlareChromaticSpread"), 2.f);
    lens_policy_settings.setS32(setting("RenderLensFlareGhostCount"), 20);
    lens_policy_settings.setS32(setting("RenderLensFlareStarburstSpikes"), -1);
    lens_policy_settings.setF32(setting("RenderLensFlareStarburstLength"), 0.f);
    auto display = f.pipeline.lensFlareConfig(true);
    REQUIRE(display.enabled);
    CHECK(display.streak_width == .001f); CHECK(display.chromatic_spread == .1f);
    CHECK(display.ghost_count == 8); CHECK(display.starburst_spikes == 1);
    CHECK(display.starburst_falloff == 80.f);
    const auto sun = linearColor3(f.pipeline.mSunDiffuse);
    for (U32 channel = 0; channel < 3; ++channel)
        CHECK(display.light_color[channel] == sun.mV[channel]);
    LensPolicyEnvironment::instance().sun_up = false;
    auto& sky = LensPolicyEnvironment::instance().sky;
    sky->ambiance = 1.f; sky->gamma = 4.f;
    display = f.pipeline.lensFlareConfig(true);
    CHECK(display.sky_hdr_scale == 4.f);
    const auto moon = linearColor3(f.pipeline.mMoonDiffuse);
    for (U32 channel = 0; channel < 3; ++channel)
        CHECK(display.light_color[channel] == moon.mV[channel]);
    sky->ambiance = 0.f; sky->auto_adjust = true;
    lens_policy_settings.setBOOL(setting("RenderSkyAutoAdjustLegacy"), true);
    lens_policy_settings.setF32(setting("RenderSkyAutoAdjustHDRScale"), 3.f);
    CHECK(f.pipeline.lensFlareConfig(true).sky_hdr_scale == 3.f);
}
