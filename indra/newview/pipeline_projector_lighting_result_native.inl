/** PPD003 actual capture-owner projector lighting result body. */
    std::string error;
    if (!capture.renderer->accumulateProjectorLights(
            *capture.base_light_target, scene, &error))
    {
        // The raw maps and selected shadow-owning lights are one frame's
        // transaction. A retry must capture both again rather than combine
        // a later light list with stale spot-map contents or matrices.
        capture.projector_shadows = {};
        capture.projector_shadow_frame = std::numeric_limits<U32>::max();
        const U32 failed_frames =
            ++capture.projector_lighting_failed_frames;
        if (failed_frames >= MetalWorldSimpleCapture::max_failed_frames)
        {
            capture.projector_lighting_attempted = true;
            if (failed_frames == MetalWorldSimpleCapture::max_failed_frames)
            {
                LL_WARNS("Metal")
                    << "Native world projector-light owner left the native frame incomplete after "
                    << failed_frames << " failures: " << error << LL_ENDL;
            }
        }
        return;
    }

    capture.projector_lighting_attempted = true;
    capture.projector_lighting_failed_frames = 0;
