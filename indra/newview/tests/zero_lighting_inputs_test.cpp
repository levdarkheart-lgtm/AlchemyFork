/**
 * @file zero_lighting_inputs_test.cpp
 * @brief PPD494 actual base-light prelude and main/probe caller dispatch.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The included production prelude owns admission, zero initialization and
 * result flags. Renderer/device/G-buffer handles are controlled CPU boundaries;
 * physical clear/depth correctness belongs to the native renderer tests. The
 * remainder of normal atmospheric lighting is represented only by a counter.
 * No probe API exists here: the zero branch must return before needing one.
 * Local/projector/composition are deliberately not implemented or certified.
 *
 * Include overrides let private negative controls use exact old production
 * snippets without committing copies or machine-specific dependency paths.
 */
#include "../alworldretentioninputs.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#ifndef ALCHEMY_ZERO_LIGHTING_PRELUDE_INCLUDE
#define ALCHEMY_ZERO_LIGHTING_PRELUDE_INCLUDE "../pipeline_zero_lighting_inputs_native.inl"
#endif
#ifndef ALCHEMY_ZERO_LIGHTING_MAIN_DISPATCH_INCLUDE
#define ALCHEMY_ZERO_LIGHTING_MAIN_DISPATCH_INCLUDE "../pipeline_base_lighting_dispatch_native.inl"
#endif
#ifndef ALCHEMY_ZERO_LIGHTING_PROBE_DISPATCH_INCLUDE
#define ALCHEMY_ZERO_LIGHTING_PROBE_DISPATCH_INCLUDE "../pipeline_base_lighting_dispatch_native.inl"
#endif

namespace
{
using U32 = std::uint32_t;
using F32 = float;
using BaseState = ALWorldRetentionInputs::BaseState;
bool diagnostics = false;
unsigned cases = 0, checks = 0, failures = 0;
void check(bool value, const char* expression, unsigned line)
{
    ++checks;
    if (!value)
    {
        ++failures;
        std::cerr << "FAILED line " << line << ": " << expression << '\n';
    }
}
#define CHECK(value) check(bool(value), #value, __LINE__)

struct Device {};
struct RenderTarget {};
struct GBuffer { unsigned identity = 17; };
struct LightingTarget { unsigned identity = 23; };
struct Camera {};
struct Renderer
{
    unsigned zero_calls = 0;
    bool fail_clear = false;
    const GBuffer* seen_gbuffer = nullptr;
    std::unique_ptr<LightingTarget> renderZeroLighting(
        const GBuffer& gbuffer, std::string* error)
    {
        ++zero_calls;
        seen_gbuffer = &gbuffer;
        if (fail_clear)
        {
            if (error) *error = "controlled zero-light failure";
            return {};
        }
        return std::make_unique<LightingTarget>();
    }
};
struct MetalWorldSimpleCapture
{
    Device* device = nullptr;
    Renderer* renderer = nullptr;
    std::unique_ptr<GBuffer> target = std::make_unique<GBuffer>();
    bool base_light_map_target = false, pending_probe_light_map = false;
    std::unique_ptr<LightingTarget> base_light_target;
    bool base_lighting_attempted = false;
    unsigned base_lighting_failed_frames = 0;
    // These later consumers remain separate mandatory owners. The actual
    // included prelude must not claim their work has happened.
    bool local_lighting_attempted = false, projector_lighting_attempted = false;
    bool lighting_composition_attempted = false, composed_light_target = false;
    BaseState retention_inputs_base_state = BaseState::hero_inputs;
    U32 retention_inputs_base_state_frame = 37;
};
struct Fixture
{
    Device device;
    Renderer renderer;
    RenderTarget mMainRT, other_rt;
    RenderTarget* mRT = &mMainRT;
    MetalWorldSimpleCapture data;
    MetalWorldSimpleCapture* mMetalWorldSimpleCapture = &data;
    bool metal_pass = true;
    bool mMetalHeroWorldCaptureActive = false, mMetalReflectionWorldCaptureActive = false;
    bool RenderDeferredAtmospheric = false;
    U32 gFrameCount = 100;
    struct { int mShadowDetail = 2; bool mSSAO = true; } mResourceInputs;
    Camera camera_data;
    unsigned base_calls = 0, normal_sun_path = 0, warning_count = 0;
    Camera* seen_camera = nullptr;
    bool seen_sun_shadow = false, seen_ssao = false;
    F32 seen_ssao_scale = 0.f, seen_ssao_max = 0.f;
    std::ostringstream warnings;

    Fixture()
    {
        ++cases;
        data.device = &device;
        data.renderer = &renderer;
    }
    bool isMetalWorldCapturePass() const { return metal_pass; }
    std::ostream& warningStream() { ++warning_count; return warnings; }
    void renderMetalWorldBaseLighting(
        Camera& camera, bool has_sun_shadow, bool has_ssao, F32 ssao_scale, F32 ssao_max)
    {
        ++base_calls;
#define LL_WARNS(...) warningStream()
#define LL_ENDL std::endl
#include ALCHEMY_ZERO_LIGHTING_PRELUDE_INCLUDE
#undef LL_ENDL
#undef LL_WARNS
        // The rest of the actual atmospheric path has its own renderer/probe
        // qualification. Reaching it proves only prelude/caller admission.
        ++normal_sun_path;
        seen_camera = &camera;
        seen_sun_shadow = has_sun_shadow;
        seen_ssao = has_ssao;
        seen_ssao_scale = ssao_scale;
        seen_ssao_max = ssao_max;
    }
    void mainCaller()
    {
        Camera* camera = &camera_data;
        [[maybe_unused]] const bool has_sun_shadow = mResourceInputs.mShadowDetail > 0;
        [[maybe_unused]] const bool has_ssao = mResourceInputs.mSSAO;
        const F32 ssao_scale = 2.f, ssao_max = 3.f;
#include ALCHEMY_ZERO_LIGHTING_MAIN_DISPATCH_INCLUDE
    }
    void probeCaller()
    {
        Camera* camera = &camera_data;
        const bool has_sun_shadow = false, has_ssao = false;
        const F32 ssao_scale = 4.f, ssao_max = 5.f;
#include ALCHEMY_ZERO_LIGHTING_PROBE_DISPATCH_INCLUDE
    }
    void direct()
    {
        renderMetalWorldBaseLighting(camera_data, true, false, 6.f, 7.f);
    }
};

void state(const Fixture& f, BaseState expected, bool observed = true)
{
    CHECK(f.data.retention_inputs_base_state ==
        (diagnostics && observed ? expected : BaseState::hero_inputs));
    CHECK(f.data.retention_inputs_base_state_frame == (diagnostics && observed ? 100u : 37u));
}
void downstream_untouched(const Fixture& f)
{
    CHECK(!f.data.local_lighting_attempted);
    CHECK(!f.data.projector_lighting_attempted);
    CHECK(!f.data.lighting_composition_attempted);
    CHECK(!f.data.composed_light_target);
}
void zero_result(const Fixture& f)
{
    CHECK(f.base_calls == 1);
    CHECK(f.renderer.zero_calls == 1);
    CHECK(f.renderer.seen_gbuffer == f.data.target.get());
    CHECK(f.data.base_light_target != nullptr);
    CHECK(f.data.base_lighting_attempted);
    CHECK(f.data.base_lighting_failed_frames == 0);
    CHECK(f.normal_sun_path == 0);
    CHECK(f.warning_count == 0);
    state(f, BaseState::cleared);
    downstream_untouched(f);
}
void zero_without_inputs()
{
    Fixture direct;
    CHECK(!direct.data.base_light_map_target && !direct.data.pending_probe_light_map);
    direct.direct();
    zero_result(direct);

    Fixture main;
    main.mainCaller();
    zero_result(main);

    Fixture hero;
    hero.mMetalHeroWorldCaptureActive = true;
    hero.mRT = &hero.other_rt;
    hero.probeCaller();
    zero_result(hero);

    Fixture probe;
    probe.mMetalReflectionWorldCaptureActive = true;
    probe.mRT = &probe.other_rt;
    probe.probeCaller();
    zero_result(probe);
}
void zero_ignores_optional_map()
{
    Fixture map;
    map.data.base_light_map_target = true;
    map.data.base_lighting_failed_frames = 2;
    map.direct();
    zero_result(map);

    Fixture pending;
    pending.data.pending_probe_light_map = true;
    pending.direct();
    zero_result(pending);
}
void normal_path_controls()
{
    Fixture missing;
    missing.RenderDeferredAtmospheric = true;
    missing.mainCaller();
    CHECK(missing.base_calls == 1 && missing.renderer.zero_calls == 0);
    CHECK(missing.normal_sun_path == 0 && !missing.data.base_lighting_attempted);
    CHECK(!missing.data.base_light_target);
    state(missing, BaseState::prerequisite);

    Fixture map;
    map.RenderDeferredAtmospheric = true;
    map.data.base_light_map_target = true;
    map.mainCaller();
    CHECK(map.base_calls == 1 && map.renderer.zero_calls == 0 && map.normal_sun_path == 1);
    CHECK(map.seen_camera == &map.camera_data && map.seen_sun_shadow && map.seen_ssao);
    CHECK(map.seen_ssao_scale == 2.f && map.seen_ssao_max == 3.f);
    CHECK(!map.data.base_light_target && !map.data.base_lighting_attempted);
    state(map, BaseState::prerequisite);
    downstream_untouched(map);

    Fixture pending;
    pending.RenderDeferredAtmospheric = true;
    pending.data.pending_probe_light_map = true;
    pending.mMetalReflectionWorldCaptureActive = true;
    pending.mRT = &pending.other_rt;
    pending.probeCaller();
    CHECK(pending.base_calls == 1 && pending.renderer.zero_calls == 0 && pending.normal_sun_path == 1);
    CHECK(pending.seen_camera == &pending.camera_data && !pending.seen_sun_shadow && !pending.seen_ssao);
    CHECK(pending.seen_ssao_scale == 4.f && pending.seen_ssao_max == 5.f);
    state(pending, BaseState::prerequisite);
}
void failed_clear_and_repeat()
{
    Fixture f;
    f.renderer.fail_clear = true;
    f.data.base_lighting_failed_frames = 2;
    f.mainCaller();
    CHECK(f.base_calls == 1 && f.renderer.zero_calls == 1);
    CHECK(!f.data.base_light_target && f.data.base_lighting_attempted);
    CHECK(f.data.base_lighting_failed_frames == 3);
    CHECK(f.normal_sun_path == 0 && f.warning_count == 1);
    state(f, BaseState::encoding_failed);
    downstream_untouched(f);
    f.renderer.fail_clear = false;
    f.mainCaller();
    CHECK(f.base_calls == 2 && f.renderer.zero_calls == 1);
    CHECK(!f.data.base_light_target && f.data.base_lighting_failed_frames == 3);
    CHECK(f.warning_count == 1 && f.normal_sun_path == 0);
    state(f, BaseState::encoding_failed);
}
void successful_repeat()
{
    Fixture f;
    f.mainCaller();
    LightingTarget* published = f.data.base_light_target.get();
    CHECK(published != nullptr);
    f.probeCaller();
    CHECK(f.base_calls == 2 && f.renderer.zero_calls == 1);
    CHECK(f.data.base_light_target.get() == published);
    CHECK(f.data.base_lighting_attempted && f.data.base_lighting_failed_frames == 0);
    CHECK(f.normal_sun_path == 0);
    state(f, BaseState::cleared);
    downstream_untouched(f);
}
void guards()
{
    for (unsigned missing = 0; missing < 7; ++missing)
    {
        Fixture f;
        if (missing == 0) f.metal_pass = false;
        if (missing == 1) f.mRT = &f.other_rt;
        if (missing == 2) f.mMetalWorldSimpleCapture = nullptr;
        if (missing == 3) f.data.device = nullptr;
        if (missing == 4) f.data.renderer = nullptr;
        if (missing == 5) f.data.target.reset();
        if (missing == 6) f.data.base_lighting_attempted = true;
        f.mainCaller();
        CHECK(f.base_calls == 1 && f.renderer.zero_calls == 0);
        CHECK(f.normal_sun_path == 0 && !f.data.base_light_target);
        CHECK(f.data.base_lighting_failed_frames == 0 && f.warning_count == 0);
        if (missing >= 3 && missing <= 5) state(f, BaseState::prerequisite);
        else state(f, BaseState::hero_inputs, false);
        downstream_untouched(f);
    }
}
}

int main(int argc, char** argv)
{
    diagnostics = argc == 2 && std::string(argv[1]) == "enabled";
    if (diagnostics) setenv("ALCHEMY_WARNING_DIAGNOSTICS", "1", 1);
    else unsetenv("ALCHEMY_WARNING_DIAGNOSTICS");
    zero_without_inputs();
    zero_ignores_optional_map();
    normal_path_controls();
    failed_clear_and_repeat();
    successful_repeat();
    guards();
    std::cout << cases << " cases, " << checks << " checks, " << failures
              << " failures; diagnostics=" << diagnostics << '\n';
    return failures ? 1 : 0;
}
