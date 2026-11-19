/**
 * @file almetal_exposure_transition_test.mm
 * @brief PPD082 actual sky blend and native exposure producer/admission checks.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../almetaldevice.h"
#include "../almetalresource.h"
#include "../almetalpipeline.h"
#include "../almetalworld.h"
#include "../../llinventory/llsettingssky.h"
#include "llsdutil.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <string>

namespace
{
// Only settings/event-loop/view ownership are controlled. The inserted code is
// the production producer and admission owner, and all texture work is native.
struct Settings
{
    std::map<std::string, double> values;
} gSavedSettings;
template<class T> class Control
{
    Settings& settings;
    std::string name;
    T fallback;
public:
    Control(Settings& s, const char* n, T value) : settings(s), name(n), fallback(value) {}
    T operator()() const
    {
        const auto it = settings.values.find(name);
        return it == settings.values.end() ? fallback : static_cast<T>(it->second);
    }
    operator T() const { return operator()(); }
};
struct RenderState { static inline bool sClassicMode = false; };
class Sky final : public LLSettingsSky
{
public:
    explicit Sky(const LLSD& data) : LLSettingsSky(data) {}
    ptr_t buildClone() override { return std::make_shared<Sky>(getSettings()); }
};
LLSettingsSky::ptr_t sky(bool legacy)
{
    auto data = llsd_clone(LLSettingsSky::defaults());
    if (legacy) data.erase(LLSettingsSky::SETTING_REFLECTION_PROBE_AMBIANCE);
    else data[LLSettingsSky::SETTING_REFLECTION_PROBE_AMBIANCE] = 2.f;
    return std::make_shared<Sky>(data);
}
LLSettingsSky::ptr_t blended(F64 fraction)
{
    auto value = sky(false);
    LLSettingsBase::ptr_t end = sky(true);
    value->blend(end, fraction); // Actual production LLSettingsSky blend.
    return value;
}
struct Fixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    struct Capture
    {
        bool hdr = true, reverse_z = false;
        std::unique_ptr<ALMetalWorldGBufferRenderer> renderer;
        std::unique_ptr<ALMetalRenderTarget> target, exposure_target, exposure_meter_target;
        U32 exposure_frame = 0, exposure_epoch = 0;
    } capture;
    std::unique_ptr<ALMetalRenderTarget> target;
    U32 gFrameCount = 10, mMetalExposureHistoryEpoch = 1;
    F32 gFrameIntervalSeconds = 0.1f;
    bool mMetalExposureHistoryReady = false;
    bool main = true, mMetalHeroWorldCaptureActive = false, mMetalReflectionWorldCaptureActive = false;
    bool sCutoutRender = false, has_water_surface = false;
    int mMainRT = 0;
    int* mRT = &mMainRT;
    bool isMainWorldPass() const { return main; }

    Fixture()
    {
        gSavedSettings.values.clear();
        device = ALMetalDevice::create(&error);
        INFO(error); REQUIRE(device);
        capture.renderer = ALMetalWorldGBufferRenderer::create(*device, &error);
        INFO(error); REQUIRE(capture.renderer);
        auto data = ALMetalWorldFrameData::create(*device, &error);
        REQUIRE(data); data->beginFrame(1); capture.renderer->setFrameData(std::move(data));
        ALMetalRenderTargetDescriptor source;
        source.width = source.height = 4;
        source.colors[0].format = ALMetalPixelFormat::rgba16_float;
        target = ALMetalRenderTarget::create(*device, source, &error);
        INFO(error); REQUIRE(target);
        auto command = device->makeCommandBuffer("PPD082 authored linear scene", &error);
        REQUIRE(command);
        ALMetalRenderPassConfig clear; clear.colors[0].clear = {4.0, 4.0, 4.0, 1.0};
        auto pass = target->makeRenderPass(clear, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "PPD082 scene clear", &error);
        REQUIRE(encoder); REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        capture.target = normals(4);
    }
    std::unique_ptr<ALMetalRenderTarget> normals(U32 width)
    {
        ALMetalRenderTargetDescriptor descriptor;
        descriptor.width = descriptor.height = width;
        descriptor.color_attachment_count = 3;
        for (auto& color : descriptor.colors) color.format = ALMetalPixelFormat::rgba16_float;
        descriptor.colors[2].format = ALMetalPixelFormat::rgb10a2_unorm;
        auto result = ALMetalRenderTarget::create(*device, descriptor, &error);
        INFO(error); REQUIRE(result);
        auto command = device->makeCommandBuffer("PPD082 authored normal classification", &error);
        REQUIRE(command); ALMetalRenderPassConfig clear;
        auto pass = result->makeRenderPass(clear, &error); REQUIRE(pass);
        auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "PPD082 normal clear", &error);
        REQUIRE(encoder); REQUIRE(encoder->end(&error)); REQUIRE(command->commitAndWait(&error));
        return result;
    }
    bool produce(const LLSettingsSky::ptr_t& current_sky, bool auto_adjust = false, bool suppress_post = false)
    {
        ++gFrameCount;
        RenderState::sClassicMode = current_sky->canAutoAdjust() && !auto_adjust;
        const bool no_post = suppress_post || current_sky->getReflectionProbeAmbiance(auto_adjust) == 0.f;
        const bool cutout_snapshot = false;
#define LLCachedControl Control
#define LLRender RenderState
#include "../../newview/pipeline_exposure_policy_native.inl"
#include "../../newview/pipeline_exposure_publication_native.inl"
#include "../../newview/pipeline_exposure_admission_native.inl"
#undef LLRender
#undef LLCachedControl
        return exact_dynamic_exposure;
    }
    std::array<U16, 4> exposure()
    {
        REQUIRE(capture.exposure_target);
        std::array<U16, 4> result{};
        REQUIRE(capture.exposure_target->colorTexture(0)->readback(
            *device, 0, 0, {}, result.data(), 8, &error));
        INFO(error); return result;
    }
    void fixed()
    {
        REQUIRE(capture.exposure_target);
        CHECK(mMetalExposureHistoryReady);
        CHECK(capture.exposure_frame == gFrameCount);
        CHECK(capture.exposure_epoch == mMetalExposureHistoryEpoch);
        CHECK(capture.exposure_meter_target);
        // Original exposureF.glsl meter=0 writes exactly (1,0,0,1), even
        // when the previous frame held nonzero adapted EV.
        CHECK((exposure() == std::array<U16,4>{0x3c00, 0, 0, 0x3c00}));
    }
};
}

TEST_CASE("PPD082 real modern-to-legacy sky blends publish current fixed EV", "[ppd082][exposure-transition]")
{
    Fixture f;
    for (const F64 fraction : {0., 0.01, 0.25, 0.5, 0.9, 0.999, 1.})
    {
        INFO(fraction);
        auto current = blended(fraction);
        REQUIRE(current->canAutoAdjust());
        if (fraction < 1.) CHECK(current->getReflectionProbeAmbiance(false) > 0.f);
        CHECK(f.produce(current));
        if (f.capture.exposure_frame == f.gFrameCount) f.fixed();
        else CHECK(f.capture.exposure_frame == f.gFrameCount);
    }
}

TEST_CASE("PPD082 fixed output replaces adapted history and dynamic exposure resumes", "[ppd082][exposure-transition]")
{
    Fixture f;
    REQUIRE(f.produce(sky(false)));
    const auto initial = f.exposure();
    CHECK(initial[1] != 0);
    REQUIRE(f.produce(sky(false)));
    const auto warmer = f.exposure();
    CHECK(warmer[1] != initial[1]);
    REQUIRE(f.produce(blended(0.5)));
    f.fixed();
    REQUIRE(f.produce(sky(false)));
    const auto resumed = f.exposure();
    CHECK(resumed[1] != 0);
    CHECK(resumed == initial); // Fixed EV0 matches the defined initial-history bootstrap.
    CHECK(resumed[1] != warmer[1]); // Does not resume from the pre-classic adapted texel.
    ++f.mMetalExposureHistoryEpoch;
    REQUIRE(f.produce(sky(false)));
    CHECK(f.exposure() == initial); // Epoch mismatch restarts without old history.
}

TEST_CASE("PPD082 disabled adaptation publishes fixed output with normal view gates", "[ppd082][exposure-transition]")
{
    Fixture f;
    gSavedSettings.values["RenderDynamicExposureEnabled"] = 0;
    CHECK(f.produce(sky(false)));
    if (f.capture.exposure_frame == f.gFrameCount) f.fixed();
    else CHECK(f.capture.exposure_frame == f.gFrameCount);
    for (int mode = 0; mode < 4; ++mode)
    {
        const auto before = f.capture.exposure_target.get();
        const auto frame = f.capture.exposure_frame;
        f.main = mode != 0;
        f.mMetalHeroWorldCaptureActive = mode == 1;
        f.mMetalReflectionWorldCaptureActive = mode == 2;
        f.capture.hdr = mode != 3;
        CHECK(f.produce(sky(false)));
        CHECK(f.capture.exposure_target.get() == before);
        CHECK(f.capture.exposure_frame == frame);
    }
}

TEST_CASE("PPD082 auto-adjust and no-post controls retain dynamic production", "[ppd082][exposure-transition]")
{
    Fixture f;
    auto legacy = sky(true);
    REQUIRE(legacy->canAutoAdjust());
    REQUIRE(f.produce(legacy, true));
    CHECK(f.capture.exposure_frame == f.gFrameCount);
    CHECK(f.exposure()[1] != 0);
    REQUIRE(f.produce(sky(false), false, true));
    CHECK(f.capture.exposure_frame == f.gFrameCount);
    CHECK(f.exposure()[1] != 0);
}

TEST_CASE("PPD082 an actual native input failure invalidates exposure and permits retry", "[ppd082][exposure-transition]")
{
    Fixture f;
    REQUIRE(f.produce(sky(false)));
    f.capture.target = f.normals(8); // Actual renderer rejects dimensions, not a mock failure return.
    CHECK_FALSE(f.produce(blended(0.5)));
    CHECK_FALSE(f.mMetalExposureHistoryReady);
    CHECK_FALSE(f.capture.exposure_target);
    CHECK(f.capture.exposure_epoch == 0);
    f.capture.target = f.normals(4);
    CHECK(f.produce(blended(0.5)));
    if (f.capture.exposure_frame == f.gFrameCount) f.fixed();
    else CHECK(f.capture.exposure_frame == f.gFrameCount);
}
