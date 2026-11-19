/** Actual native base-light prelude and zero-color mode. PPD494. */
    if (!isMetalWorldCapturePass() ||
        (!mMetalHeroWorldCaptureActive &&
         !mMetalReflectionWorldCaptureActive && mRT != &mMainRT) ||
        !mMetalWorldSimpleCapture)
    {
        return;
    }
    MetalWorldSimpleCapture& capture = *mMetalWorldSimpleCapture;
    using BaseState = ALWorldRetentionInputs::BaseState;
    const auto observe_base = [&](BaseState state)
    {
        if (ALWarningDiagnostics::enabled())
        {
            capture.retention_inputs_base_state = state;
            capture.retention_inputs_base_state_frame = gFrameCount;
        }
    };
    // A repeated call after encoding must preserve that frame's result.
    if (!capture.base_lighting_attempted) observe_base(BaseState::prerequisite);
    if (!capture.device || !capture.renderer || !capture.target ||
        capture.base_lighting_attempted)
    {
        return;
    }

    if (!RenderDeferredAtmospheric)
    {
        // Original deferred lighting starts from transparent zero even when
        // the sun/soften-sky pass is disabled. Local/projector lighting and
        // forward geometry still require a fresh color plus current depth.
        std::string error;
        auto target = capture.renderer->renderZeroLighting(*capture.target, &error);
        if (!target)
        {
            observe_base(BaseState::encoding_failed);
            ++capture.base_lighting_failed_frames;
            capture.base_lighting_attempted = true;
            LL_WARNS("Metal") << "Native world zero-light initialization failed: "
                << error << LL_ENDL;
            return;
        }
        capture.base_light_target = std::move(target);
        capture.base_lighting_attempted = true;
        capture.base_lighting_failed_frames = 0;
        observe_base(BaseState::cleared);
        return;
    }
    if (!capture.base_light_map_target && !capture.pending_probe_light_map)
    {
        return;
    }

