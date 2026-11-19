/** PPD003 actual capture-owner local lighting result body. */
    std::string error;
    if (!capture.renderer->accumulateLocalLights(
            *capture.base_light_target, scene, &error))
    {
        if (++capture.local_lighting_failed_frames >=
            MetalWorldSimpleCapture::max_failed_frames)
        {
            capture.local_lighting_attempted = true;
            if (capture.local_lighting_failed_frames == MetalWorldSimpleCapture::max_failed_frames)
            {
                LL_WARNS("Metal")
                    << "Native world local-point-light owner left the native frame incomplete after "
                    << capture.local_lighting_failed_frames
                    << " failures: " << error << LL_ENDL;
            }
        }
        return;
    }

    capture.local_lighting_attempted = true;
    capture.local_lighting_failed_frames = 0;
