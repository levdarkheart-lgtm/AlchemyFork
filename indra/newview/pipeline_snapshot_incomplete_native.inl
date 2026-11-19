/** Actual exact-current snapshot failure body and opt-in producer inputs. PPD493. */
        if (error)
        {
            *error = fmt::format(
                "Metal world snapshot did not complete exactly: "
                "chain_snapshot={}, capture_active={}, chain_active={}, "
                "complete={}, first_incomplete_line={}, "
                "frame_active={}, renderer={}, target={}, composed_light={}, "
                "lighting_attempted={}, post_attempted={}, "
                "presentation_target={}, presentation_frame={}, "
                "world_ui_target={}, world_ui_frame={}, current_frame={}, "
                "device={}, device_match={}",
                capture.world_ui_chain_snapshot,
                capture.world_ui_capture_active,
                capture.world_ui_chain_active,
                capture.complete,
                capture.first_incomplete_source_line,
                capture.frame_active,
                static_cast<bool>(capture.renderer),
                static_cast<bool>(capture.target),
                static_cast<bool>(capture.composed_light_target),
                capture.lighting_composition_attempted,
                capture.opaque_post_deferred_attempted,
                static_cast<bool>(capture.presentation_target),
                capture.presentation_frame,
                static_cast<bool>(capture.world_ui_target),
                capture.world_ui_frame,
                gFrameCount,
                device != nullptr,
                capture.device == device);
            if (ALWarningDiagnostics::enabled())
            {
                try
                {
                    const auto& hero = mHeroProbeManager;
                    const bool selected = hero.mNearestHero.notNull() &&
                        !hero.mNearestHero->isDead();
                    // These are observed producer inputs, not a causal verdict
                    // or a separately stamped uniform-publication receipt.
                    *error += fmt::format(
                        "; diagnostic_inputs base_state={}, base_frame={}, "
                        "base_current={}, deferred_atmospheric={}, "
                        "hero(selected/ready/advertised/owner_match/epoch_match)="
                        "{}/{}/{}/{}/{}",
                        ALWorldRetentionInputs::baseStateName(capture.retention_inputs_base_state),
                        capture.retention_inputs_base_state_frame,
                        capture.retention_inputs_base_state_frame != std::numeric_limits<U32>::max() &&
                            capture.retention_inputs_base_state_frame == gFrameCount,
                        RenderDeferredAtmospheric,
                        selected,
                        hero.mMetalRadianceReady,
                        hero.mHeroData.heroProbeCount == 1,
                        selected && hero.mNativePublishedOwner == hero.mNearestHero->getID(),
                        hero.mNativePublishedEpoch == hero.mMetalRadianceEpoch);
                }
                catch (...) {} // Optional evidence preserves the original failure.
            }
        }
        return false;
