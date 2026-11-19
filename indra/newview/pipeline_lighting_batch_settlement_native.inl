/** PPD003 actual capture-owner lighting batch settlement body. */
        if (lighting_batch_active)
        {
            MetalWorldSimpleCapture& capture =
                *mMetalWorldSimpleCapture;
            lighting_batch_error.clear();
            // Composition can already abandon a failed native prefix. Do
            // cleanup once, without committing an absent command or counting
            // that intentional cancellation as a second failed frame.
            const bool command_active = capture.renderer->commandBatchActive();
            if (!command_active || !capture.renderer->commitCommandBatch(
                    &lighting_batch_error))
            {
                capture.base_light_map_target.reset();
                capture.pending_probe_light_map.reset();
                capture.main_lighting_ready.reset();
                capture.base_light_target.reset();
                capture.composed_light_target.reset();
                // Composition keeps its existing per-transaction failure
                // status. Only the actual batch owner owns this retry streak.
                if (command_active) ++capture.lighting_composition_failed_frames;
                const U32 failed_frames = command_active
                    ? ++capture.lighting_batch_commit_failed_frames
                    : capture.lighting_batch_commit_failed_frames;
                if (command_active && failed_frames ==
                    MetalWorldSimpleCapture::max_failed_frames)
                {
                    LL_WARNS("Metal")
                        << "Native deferred-light batch left the frame incomplete after "
                        << failed_frames << " commit failures: "
                        << lighting_batch_error << LL_ENDL;
                }
            }
            else
            {
                // An absent/canceled command is not successful recovery.
                capture.lighting_batch_commit_failed_frames = 0;
            }
        }
