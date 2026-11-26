/** HDR Temporal reconstructs after reduced-resolution DoF, before bloom/display transfer. */
            if (capture.temporal_effective)
            {
                ALMetalWorldTemporalMotionScene motion;
                motion.opaque_scene = capture.temporal_opaque_inputs ? &capture.temporal_opaque_inputs->scene() : nullptr;
                motion.forward_scene = &scene;
                // The ray sky is captured independently of the haze/sky-behind consumer.
                // Motion/rejection must use the sky that actually produced this world's background.
                motion.modern_sky = capture.modern_sky_valid && capture.modern_sky_drawn
                    ? &capture.modern_sky.scene : nullptr;
                motion.reject_sky_history = capture.stageTemporalSky(motion.modern_sky);
                motion.depth = capture.target ? capture.target->depthStencilTexture() : nullptr;
                motion.opaque_depth = capture.temporal_opaque_depth ? capture.temporal_opaque_depth->colorTexture(0) : nullptr;
                const auto& previous_opaque=capture.temporal_history.previousOpaque();
                const auto& previous_forward=capture.temporal_history.previousForward();
                motion.previous_opaque=previous_opaque.data(); motion.previous_opaque_count=previous_opaque.size();
                motion.previous_forward=previous_forward.data(); motion.previous_forward_count=previous_forward.size();
                std::copy(capture.temporal_history.clipToPrevious().begin(),capture.temporal_history.clipToPrevious().end(),motion.clip_to_previous_clip);
                std::copy(capture.temporal_jitter.begin(),capture.temporal_jitter.end(),motion.jitter_pixels);
                motion.width=capture.width; motion.height=capture.height;
                motion.reverse_z=capture.reverse_z; motion.reset=capture.temporal_history.resetRequired();
                LLMatrix4a inverse_projection;
                inverse_projection.setInverse(LLViewerCamera::getCurrent().getProjection());
                std::copy_n(inverse_projection.getF32ptr(),16,motion.inverse_projection);
                if (scene.water_surface && !scene.water_surface->underwater)
                {
                    motion.reject_water=true;
                    std::copy_n(scene.water_surface->field.water_plane,4,motion.eye_water_plane);
                    motion.water_rejection_margin=scene.water_surface->waves ?
                        std::abs(scene.water_surface->wave_reach_amplitude[1]) : 0.f;
                }
                std::string temporal_error;
                capture.temporal_motion = exact_dof && motion.opaque_scene
                    ? capture.renderer->renderTemporalMotion(motion,&temporal_error,std::move(capture.temporal_motion)) : nullptr;
                ALMetalTemporalScalerFrame temporal_frame;
                temporal_frame.jitter_x=capture.temporal_jitter[0]; temporal_frame.jitter_y=capture.temporal_jitter[1];
                temporal_frame.depth_reversed=capture.reverse_z;
                temporal_frame.reset=capture.temporal_history.resetRequired();
                bool reconstructed=false;
                if (capture.aurora_selected)
                {
                    ALMetalAuroraFrame aurora_frame;
                    aurora_frame.jitter_x=capture.temporal_jitter[0];
                    aurora_frame.jitter_y=capture.temporal_jitter[1];
                    aurora_frame.reset=capture.temporal_history.resetRequired();
                    aurora_frame.jitter_phase_count=capture.temporal_history.jitterPhaseCount();
                    alMetalAuroraSetViewerCamera(aurora_frame,
                        LLViewerCamera::getCurrent().getProjection().getF32ptr(),
                        capture.temporal_history.clipToPrevious().data());
                    reconstructed=capture.temporal_motion && capture.temporal_resolved_output &&
                        capture.aurora_opaque_color &&
                        capture.renderer->encodeAuroraUpscale(*capture.aurora_reconstructor,capture.aurora_descriptor,
                            *post_source->colorTexture(0),*capture.aurora_opaque_color->colorTexture(0),
                            *capture.temporal_motion,*capture.temporal_resolved_output,aurora_frame,
                            capture.aurora_ticket,&temporal_error);
                }
                else
                {
                    reconstructed=capture.temporal_motion && capture.temporal_raw_output &&
                        capture.temporal_resolved_output &&
                        capture.renderer->encodeTemporalUpscale(*capture.temporal_scaler,capture.temporal_descriptor,
                            *post_source->colorTexture(0),*capture.temporal_motion,*capture.temporal_raw_output,
                            temporal_frame,capture.temporal_ticket,&temporal_error) &&
                        capture.renderer->resolveTemporalColor(*post_source,*capture.temporal_raw_output,
                            *capture.temporal_resolved_output,&temporal_error,capture.temporal_jitter.data());
                }
                if (!reconstructed)
                {
                    capture.renderer->cancelCommandBatch();
                    capture.abandonTemporalFrame();
                    capture.temporal_history.invalidate();
                    capture.invalidateReconstruction();
                    capture.markIncomplete(__LINE__);
                    LL_WARNS("Metal") << "Temporal world publication canceled; retaining the previous display: "
                        << temporal_error << LL_ENDL;
                    return;
                }
                capture.temporal_encoded=true;
                post_source=capture.temporal_resolved_output.get();
            }
