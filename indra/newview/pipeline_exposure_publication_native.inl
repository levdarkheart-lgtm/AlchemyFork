/**
 * @file pipeline_exposure_publication_native.inl
 * @brief Function-local native exposure owner, shared with physical Metal tests.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
// PPD082: this function-local body is also executed by the native transition fixture.
if (main_exposure_meter)
{
    static LLCachedControl<F32> compensation(
        gSavedSettings, "AlchemyExposureCompensation", 0.f);
    static LLCachedControl<F32> min_ev(
        gSavedSettings, "AlchemyExposureMinEV", -3.f);
    static LLCachedControl<F32> max_ev(
        gSavedSettings, "AlchemyExposureMaxEV", 4.f);
    static LLCachedControl<F32> low_percent(
        gSavedSettings, "AlchemyExposureLowPercent", 10.f);
    static LLCachedControl<F32> high_percent(
        gSavedSettings, "AlchemyExposureHighPercent", 95.f);
    static LLCachedControl<F32> speed_up(
        gSavedSettings, "AlchemyExposureSpeedUp", 3.f);
    static LLCachedControl<F32> speed_down(
        gSavedSettings, "AlchemyExposureSpeedDown", 1.f);
    static LLCachedControl<U32> metering_mode(
        gSavedSettings, "AlchemyExposureMeteringMode", 1);
    static LLCachedControl<F32> sky_weight(
        gSavedSettings, "AlchemyExposureSkyWeight", 1.f);

    const F32 low = llclamp(
        low_percent() * 0.01f, 0.f, 0.99f);
    ALMetalWorldExposureScene exposure;
    exposure.meter_output = &capture.exposure_meter_target;
    exposure.source = target.get();
    exposure.normal = capture.target->colorTexture(2);
    exposure.depth = target->depthStencilTexture();
    exposure.reverse_z = capture.reverse_z;
    exposure.water_stencil = has_water_surface ? target.get() : nullptr;
    exposure.previous = main_dynamic_exposure && use_exposure_history
        ? capture.exposure_target->colorTexture(0) : nullptr;
    exposure.delta_seconds = gFrameIntervalSeconds;
    exposure.compensation = compensation();
    exposure.min_ev = llmin(min_ev(), max_ev());
    exposure.max_ev = llmax(min_ev(), max_ev());
    exposure.low_fraction = low;
    exposure.high_fraction = llclamp(
        high_percent() * 0.01f, low + 0.01f, 1.f);
    exposure.speed_up = llmax(speed_up(), 0.f);
    exposure.speed_down = llmax(speed_down(), 0.f);
    exposure.sky_weight = llmax(sky_weight(), 0.f);
    exposure.metering_mode = llmin(
        static_cast<U32>(metering_mode()), 2u);
    if (!main_dynamic_exposure)
    {
        // Original generateExposure still publishes (1,0,0,1) when classic
        // lighting or the preference disables adaptation. A sky blend can
        // already use classic lighting while probe ambiance remains nonzero,
        // so final presentation still needs this frame's fixed-EV result.
        // No previous texel participates in the fixed output or its history.
        exposure.delta_seconds = 0.f;
        exposure.compensation = 0.f;
        exposure.min_ev = exposure.max_ev = 0.f;
        exposure.speed_up = exposure.speed_down = 0.f;
    }
    std::string exposure_error;
    auto exposure_target =
        capture.renderer->renderExposure(
            exposure, &exposure_error);
    if (exposure_target)
    {
        capture.exposure_target =
            std::move(exposure_target);
        capture.exposure_frame = gFrameCount;
        capture.exposure_epoch =
            mMetalExposureHistoryEpoch;
        mMetalExposureHistoryReady = true;
    }
    else
    {
        capture.exposure_target.reset();
        capture.exposure_frame =
            std::numeric_limits<U32>::max();
        capture.exposure_epoch = 0;
        mMetalExposureHistoryReady = false;
        LL_WARNS("Metal")
            << "Native world exposure history did not advance: "
            << exposure_error << LL_ENDL;
    }
}

