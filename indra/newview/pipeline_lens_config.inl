/**
 * @file pipeline_lens_config.inl
 * @brief Lens-flare CPU configuration shared by the viewer and its no-driver regression.
 *
 * Original behavior:1eb44bbd pipeline.cpp generateLensFlareState/colorCorrect.
 * Included after the viewer dependencies; no renderer is constructed here.
 */

LLPipeline::LensFlareStateConfig LLPipeline::lensFlareStateConfig()
{
    static LLCachedControl<F32> strength_setting(
        gSavedSettings, "RenderLensFlareStrength", 0.f);
    static LLCachedControl<F32> occlusion_scale_setting(
        gSavedSettings, "RenderLensFlareOcclusionScale", 1.f);
    static LLCachedControl<F32> fade_time_setting(
        gSavedSettings, "RenderLensFlareFadeTime", 0.35f);

    LensFlareStateConfig config;
    config.hold = suppressesLensAndPrintEffects();
    // This is CPU pass availability, not the retired GL executable gate.
    // Native encoder/resource admission is checked by the capture owner.
    config.enabled = !config.hold && strength_setting() > 0.f &&
        gLensFlareStateProgram.isAvailable();
    config.sun_up = LLEnvironment::instance().getIsSunUp();
    if (!config.enabled)
    {
        return config;
    }

    LLEnvironment& environment = LLEnvironment::instance();
    LLSettingsSky::ptr_t sky = environment.getCurrentSky();
    if (!sky)
    {
        config.enabled = false;
        return config;
    }

    if (config.sun_up != mLensFlareSunUp)
    {
        // A history for the previous body must not tint the new anchor.
        // Retire the native history before this frame's state is encoded.
        mLensFlareSunUp = config.sun_up;
#if LL_DARWIN
        ++mMetalLensFlareHistoryEpoch;
        if (mMetalLensFlareHistoryEpoch == 0)
        {
            ++mMetalLensFlareHistoryEpoch;
        }
        mMetalLensFlareHistoryReady = false;
        if (mMetalWorldSimpleCapture)
        {
            mMetalWorldSimpleCapture->lens_flare_state_target.reset();
            mMetalWorldSimpleCapture->lens_flare_state_frame =
                std::numeric_limits<U32>::max();
            mMetalWorldSimpleCapture->lens_flare_state_epoch = 0;
        }
#endif
    }

    const LLVector4 light_dir = config.sun_up ? mSunDir : mMoonDir;
    LLVector4a sun_eye;
    LLVector4a sun_clip;
    LLViewerCamera::getCurrent().getModelview().rotate(
        LLVector4a(light_dir.mV[0], light_dir.mV[1],
                   light_dir.mV[2], 0.f), sun_eye);
    LLViewerCamera::getCurrent().getProjection().transform4(
        sun_eye, sun_clip);
    if (sun_clip[3] > 0.f)
    {
        const LLVector2 sun_ndc(
            sun_clip[0] / sun_clip[3], sun_clip[1] / sun_clip[3]);
        const LLVector2 sun_uv =
            sun_ndc * 0.5f + LLVector2(0.5f, 0.5f);
        constexpr F32 margin = 0.2f;
        config.edge_visibility =
            llclamp((sun_uv.mV[VX] + margin) / margin, 0.f, 1.f) *
            llclamp(((1.f + margin) - sun_uv.mV[VX]) / margin,
                    0.f, 1.f) *
            llclamp((sun_uv.mV[VY] + margin) / margin, 0.f, 1.f) *
            llclamp(((1.f + margin) - sun_uv.mV[VY]) / margin,
                    0.f, 1.f);
        mLensFlareSunUV = sun_uv;
    }
    config.sun_uv = {
        mLensFlareSunUV.mV[VX], mLensFlareSunUV.mV[VY]};

    // The probe follows the body's drawn quad, including its authored scale
    // and horizon growth. Before the sky's first update its corners give zero.
    const F32 half_tan = (config.sun_up ? mSky.getSun() : mSky.getMoon()).angularHalfSize();
    const LLViewerCamera* camera = LLViewerCamera::getInstance();
    const F32 fov_y = llclamp(camera->getView(),
                              0.01f, F_PI - 0.01f);
    config.occlusion_radius = llmin(
        0.5f * half_tan / tanf(fov_y * 0.5f) *
            llmax(camera->getZoomFactor(), 1.f) *
            llclamp(occlusion_scale_setting(), 0.25f, 2.f),
        0.3f);
    config.fade_time = llclamp(fade_time_setting(), 0.1f, 1.f);
    return config;
}

LLPipeline::LensFlareConfig
LLPipeline::lensFlareConfig(bool world_frame) const
{
    static LLCachedControl<F32> strength_setting(
        gSavedSettings, "RenderLensFlareStrength", 0.f);
    static LLCachedControl<F32> streak_length_setting(
        gSavedSettings, "RenderLensFlareStreakLength", 0.5f);
    static LLCachedControl<F32> streak_falloff_setting(
        gSavedSettings, "RenderLensFlareStreakFalloff", 1.5f);
    static LLCachedControl<F32> streak_thickness_setting(
        gSavedSettings, "RenderLensFlareStreakThickness", 0.08f);
    static LLCachedControl<F32> streak_intensity_setting(
        gSavedSettings, "RenderLensFlareStreakIntensity", 1.f);
    static LLCachedControl<LLColor3> streak_tint_setting(
        gSavedSettings, "RenderLensFlareStreakTint",
        LLColor3(0.6f, 0.7f, 1.f));
    static LLCachedControl<F32> chromatic_spread_setting(
        gSavedSettings, "RenderLensFlareChromaticSpread", 0.08f);
    static LLCachedControl<F32> glow_radius_setting(
        gSavedSettings, "RenderLensFlareGlowRadius", 0.12f);
    static LLCachedControl<F32> glow_falloff_setting(
        gSavedSettings, "RenderLensFlareGlowFalloff", 8.f);
    static LLCachedControl<F32> glow_setting(
        gSavedSettings, "RenderLensFlareGlow", 1.f);
    static LLCachedControl<F32> ghost_setting(
        gSavedSettings, "RenderLensFlareGhost", 0.f);
    static LLCachedControl<S32> ghost_count_setting(
        gSavedSettings, "RenderLensFlareGhostCount", 4);
    static LLCachedControl<F32> ghost_spacing_setting(
        gSavedSettings, "RenderLensFlareGhostSpacing", 0.3f);
    static LLCachedControl<F32> halo_setting(
        gSavedSettings, "RenderLensFlareHalo", 0.f);
    static LLCachedControl<F32> halo_radius_setting(
        gSavedSettings, "RenderLensFlareHaloRadius", 0.5f);
    static LLCachedControl<F32> halo_width_setting(
        gSavedSettings, "RenderLensFlareHaloWidth", 0.15f);
    static LLCachedControl<F32> starburst_setting(
        gSavedSettings, "RenderLensFlareStarburst", 0.f);
    static LLCachedControl<S32> starburst_spikes_setting(
        gSavedSettings, "RenderLensFlareStarburstSpikes", 4);
    static LLCachedControl<F32> starburst_sharpness_setting(
        gSavedSettings, "RenderLensFlareStarburstSharpness", 24.f);
    static LLCachedControl<F32> starburst_length_setting(
        gSavedSettings, "RenderLensFlareStarburstLength", 0.25f);

    LensFlareConfig config;
    config.strength = suppressesLensAndPrintEffects() || !world_frame ||
            !gLensFlareStateProgram.isAvailable()
        ? 0.f : llclamp(strength_setting(), 0.f, 1.f);
    config.enabled = config.strength > 0.f;
    config.sun_uv = {
        mLensFlareSunUV.mV[VX], mLensFlareSunUV.mV[VY]};
    config.streak_length = llclamp(
        streak_length_setting(), 0.01f, 2.f);
    config.streak_falloff = llclamp(
        streak_falloff_setting(), 0.1f, 10.f);
    config.streak_width = llmax(
        llclamp(streak_thickness_setting(), 0.f, 1.f) * 0.05f,
        0.001f);
    config.streak_intensity = llclamp(
        streak_intensity_setting(), 0.f, 5.f);
    const LLColor3 streak_tint = linearColor3(streak_tint_setting());
    std::copy_n(streak_tint.mV, 3, config.streak_tint.begin());
    config.chromatic_spread = llclamp(
        chromatic_spread_setting(), 0.f, 1.f) * 0.1f;
    config.glow_radius = llclamp(glow_radius_setting(), 0.01f, 0.5f);
    config.glow_falloff = llclamp(glow_falloff_setting(), 1.f, 30.f);
    config.glow = llclamp(glow_setting(), 0.f, 5.f);
    config.ghost = llclamp(ghost_setting(), 0.f, 5.f);
    config.ghost_count = static_cast<U32>(
        llclamp(ghost_count_setting(), 0, 8));
    config.ghost_spacing = llclamp(
        ghost_spacing_setting(), 0.1f, 1.f);
    config.halo = llclamp(halo_setting(), 0.f, 5.f);
    config.halo_radius = llclamp(halo_radius_setting(), 0.01f, 1.f);
    config.halo_width = llclamp(halo_width_setting(), 0.01f, 0.5f);
    config.starburst = llclamp(starburst_setting(), 0.f, 5.f);
    config.starburst_spikes = static_cast<U32>(
        llclamp(starburst_spikes_setting(), 1, 32));
    config.starburst_sharpness = llclamp(
        starburst_sharpness_setting(), 1.f, 256.f);
    config.starburst_falloff = 4.f /
        (llclamp(starburst_length_setting(), 0.f, 1.f) + 0.05f);

    const bool sun_up = LLEnvironment::instance().getIsSunUp();
    const LLColor4 light_color = linearColor3(
        sun_up ? mSunDiffuse : mMoonDiffuse);
    std::copy_n(light_color.mV, 3, config.light_color.begin());
    if (LLSettingsSky::ptr_t sky =
            LLEnvironment::instance().getCurrentSky())
    {
        static LLCachedControl<bool> should_auto_adjust(
            gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
        static LLCachedControl<F32> auto_adjust_hdr_scale(
            gSavedSettings, "RenderSkyAutoAdjustHDRScale", 2.f);
        config.sky_hdr_scale = sky->getReflectionProbeAmbiance() != 0.f
            ? sqrtf(sky->getGamma()) * 2.f
            : sky->canAutoAdjust() && should_auto_adjust
                ? static_cast<F32>(auto_adjust_hdr_scale) : 1.f;
    }
    return config;
}
