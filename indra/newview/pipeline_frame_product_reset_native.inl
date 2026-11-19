/** PPD003 actual capture-owner frame product reset body. */
    if (!capture.frame_active &&
        (snapshot_request ||
         (capture.attempted &&
          (capture.continuous_world_capture || mScenePixelPending ||
           refresh_scope || refresh_lens_flare || refresh_lens_dirt ||
           request_world_ui_chain) &&
          capture.continuous_world_frame != gFrameCount)))
    {
        // Continuous SSR history makes every main-world frame a fresh
        // transaction; a pending one-shot CPU probe or an exactly supported
        // native scope sample requests the same refresh without making the
        // chain permanently continuous. Retire only
        // per-frame products and attempt gates; persistent probe, shadow and
        // history resources stay live. A failed stage therefore retains the
        // previous Metal drawable and retries instead of wedging a gate.
        capture.target.reset();
        capture.composed_light_target.reset();
        capture.opaque_post_deferred_target.reset();
        capture.display_post_target.reset();
        capture.display_post_frame = std::numeric_limits<U32>::max();
        capture.presentation_target.reset();
        capture.presentation_frame = std::numeric_limits<U32>::max();
        capture.world_ui_target.reset();
        capture.world_ui_frame = std::numeric_limits<U32>::max();
        capture.world_ui_chain_active = false;
        capture.world_ui_chain_counted = false;
        capture.world_ui_chain_snapshot = false;
        capture.world_ui_chain_debug_color = false;
        capture.depth_of_field_target.reset();
        capture.depth_of_field_frame = std::numeric_limits<U32>::max();
        capture.bloom_target.reset();
        capture.bloom_frame = std::numeric_limits<U32>::max();
        capture.base_light_map_target.reset();
        capture.pending_probe_light_map.reset();
        capture.main_lighting_ready.reset();
        capture.base_light_target.reset();
        capture.attempted = false;
        capture.local_lighting_attempted = false;
        capture.projector_lighting_attempted = false;
        capture.lighting_composition_attempted = false;
        capture.lighting_composition_failed_frames = 0;
        capture.opaque_post_deferred_attempted = false;
        capture.base_lighting_attempted = false;
        capture.base_lighting_failed_frames = 0;
        capture.failed_frames = 0;
    }
