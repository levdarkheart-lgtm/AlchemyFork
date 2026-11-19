/** Function-local observation at the actual stale-background boundary. PPD003. */
capture.retention_inputs_observer.observe([] { return LLTimer::getTotalSeconds(); }, [&]
{
    ALWorldRetentionInputs::Snapshot s;
    using View = ALWorldRetentionInputs::View;
    s.view = capture.hero_probe_face_capture ? View::hero
        : capture.reflection_probe_face_capture ? View::probe
        : isMainWorldPass() ? View::main : View::other;
    const auto frame = [](U32 value)
    {
        return value == std::numeric_limits<U32>::max()
            ? ALWorldRetentionInputs::Snapshot::unknown : static_cast<U64>(value);
    };
    s.current_frame = gFrameCount;
    s.base_state = capture.retention_inputs_base_state;
    s.base_state_frame = frame(capture.retention_inputs_base_state_frame);
    const auto& hero = mHeroProbeManager;
    s.hero_selected = hero.mNearestHero.notNull() && !hero.mNearestHero->isDead();
    s.hero_ready = hero.mMetalRadianceReady;
    s.hero_advertised = hero.mHeroData.heroProbeCount == 1;
    s.hero_owner_match = s.hero_selected &&
        hero.mNativePublishedOwner == hero.mNearestHero->getID();
    s.hero_epoch_match = hero.mNativePublishedEpoch == hero.mMetalRadianceEpoch;
    s.background_frame = capture.ui_frame_background->source_frame;
    s.capture_frame = frame(capture.continuous_world_frame);
    s.first_incomplete_line = capture.first_incomplete_source_line;
    s.complete = capture.complete;
    s.attempted = capture.attempted;
    s.gbuffer = static_cast<bool>(capture.target);
    s.light_map = static_cast<bool>(capture.base_light_map_target);
    s.base_light = static_cast<bool>(capture.base_light_target);
    s.fused_light = static_cast<bool>(capture.main_lighting_ready);
    s.composed_light = static_cast<bool>(capture.composed_light_target);
    s.opaque_post = static_cast<bool>(capture.opaque_post_deferred_target);
    s.display_post = static_cast<bool>(capture.display_post_target);
    s.display_frame = frame(capture.display_post_frame);
    s.presentation = static_cast<bool>(capture.presentation_target);
    s.presentation_frame = frame(capture.presentation_frame);
    s.exposure_ready = mMetalExposureHistoryReady;
    s.exposure = static_cast<bool>(capture.exposure_target);
    s.exposure_frame = frame(capture.exposure_frame);
    s.exposure_epoch = capture.exposure_epoch;
    s.expected_exposure_epoch = mMetalExposureHistoryEpoch;
    s.hdr = capture.hdr;
    s.exposure_policy_frame = frame(capture.retention_inputs_exposure_policy_frame);
    s.meter_owner = capture.retention_inputs_meter_owner;
    s.adaptation = capture.retention_inputs_adaptation;
    s.classic_lighting = capture.retention_inputs_classic;
    s.post_policy_frame = frame(capture.retention_inputs_post_policy_frame);
    s.legacy_gamma = capture.retention_inputs_gamma;
    s.no_post = capture.retention_inputs_no_post;
    s.dynamic_exposure_enabled = capture.retention_inputs_dynamic;
    s.base_attempted = capture.base_lighting_attempted;
    s.local_attempted = capture.local_lighting_attempted;
    s.projector_attempted = capture.projector_lighting_attempted;
    s.composition_attempted = capture.lighting_composition_attempted;
    s.opaque_attempted = capture.opaque_post_deferred_attempted;
    return s;
}, [&](const ALWorldRetentionInputs::Snapshot& snapshot)
{
    LL_INFOS("WarningInputs") << "world_retained_inputs " << snapshot << LL_ENDL;
});
