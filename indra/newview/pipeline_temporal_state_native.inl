/** One ordinary main-world Temporal stream; included inside MetalWorldSimpleCapture. */
    std::unique_ptr<ALMetalTemporalScaler> temporal_scaler;
    std::unique_ptr<ALMetalAuroraReconstructor> aurora_reconstructor;
    ALMetalAuroraDescriptor aurora_descriptor;
    ALMetalAuroraReconstructor::Ticket aurora_ticket = 0;
    bool aurora_selected = false;
    std::unique_ptr<ALMetalRenderTarget> aurora_opaque_color;

    void invalidateReconstruction() noexcept
    {
        if (temporal_scaler) temporal_scaler->invalidate();
        if (aurora_reconstructor) aurora_reconstructor->invalidate();
    }
    bool submitReconstruction(std::string* error)
    {
        if (aurora_selected)
        {
            if (!aurora_reconstructor || !aurora_ticket || !aurora_reconstructor->submitted(aurora_ticket,error)) return false;
            aurora_ticket = 0;
            return true;
        }
        if (!temporal_scaler || !temporal_ticket || !temporal_scaler->submitted(temporal_ticket,error)) return false;
        temporal_ticket = 0;
        return true;
    }
    ALMetalTemporalScalerStats reconstructionStats() const noexcept
    {
        if (aurora_selected && aurora_reconstructor)
        {
            const auto source=aurora_reconstructor->stats();
            ALMetalTemporalScalerStats result;
            result.creation_successes=source.creation_successes;
            result.prepare_reuses=source.prepare_reuses;
            result.encoded_calls=source.encoded_calls;
            result.submitted_calls=source.submitted_calls;
            result.abandoned_calls=source.abandoned_calls;
            result.reset_calls=source.reset_calls;
            result.gpu_failures=source.gpu_failures;
            result.pending_submissions=source.pending_submissions;
            result.history_valid=source.history_valid;
            return result;
        }
        return temporal_scaler ? temporal_scaler->stats() : ALMetalTemporalScalerStats{};
    }
    ALMetalTemporalWorldHistory temporal_history;
    ALMetalTemporalScalerDescriptor temporal_descriptor;
    std::shared_ptr<const ALMetalWorldViewInputs> temporal_opaque_inputs;
    std::unique_ptr<ALMetalRenderTarget> temporal_raw_output, temporal_resolved_output, temporal_motion;
    // Forward alpha-depth is allowed to update the G-buffer attachment. Keep
    // the original opaque visibility for matching composite motion afterward.
    std::unique_ptr<ALMetalRenderTarget> temporal_opaque_depth;
    ALMetalTemporalScaler::Ticket temporal_ticket = 0;
    std::array<F32, 2> temporal_jitter{};
    U64 temporal_world_serial = 0, temporal_gpu_failures = 0;
    bool temporal_effective = false, temporal_encoded = false, temporal_submitted = false, temporal_stream_selected = false;
    std::string temporal_fallback;
    LLVector3 temporal_camera_position, temporal_previous_camera_position;
    LLVector3 temporal_camera_direction, temporal_previous_camera_direction;
    bool temporal_previous_camera_valid = false;

    // Only discontinuous sky inputs belong here. Camera matrices, cloud scroll,
    // time and the frame-owned star attachment must not invalidate a static dome.
    struct TemporalSkyIdentity
    {
        bool visible = false;
        std::array<U32, 2> layers{};
        std::array<std::pair<U64, U64>, 10> maps{};
        std::array<F32, 12> hdri_rotation{};
        F32 hdri_split = 0.f;
        bool operator==(const TemporalSkyIdentity&) const = default;
    };
    std::optional<TemporalSkyIdentity> temporal_previous_sky, temporal_pending_sky;

    bool stageTemporalSky(const ALMetalWorldModernSkyScene* sky)
    {
        TemporalSkyIdentity current;
        if (sky)
        {
            current.visible = true;
            current.layers = {sky->constants.config[0], sky->constants.config[1]};
            const ALMetalTexture* maps[] = {sky->rainbow_texture, sky->halo_texture, sky->hdri_texture,
                sky->sun_texture, sky->sun_next_texture, sky->moon_texture, sky->bloom_texture,
                sky->bloom_next_texture, sky->cloud_texture, sky->cloud_next_texture};
            const auto& c = sky->constants;
            const bool hdri = (c.config[0] & 128u) != 0u;
            const bool dome = !hdri || c.transfer[1] < 1.f;
            const bool active[] = {dome && c.dome.optics[0] != 0.f, dome && c.dome.optics[2] != 0.f,
                hdri && c.transfer[1] > 0.f,
                !hdri && (c.config[0] & 4u), !hdri && (c.config[0] & 4u) && c.body[0] > 0.f,
                !hdri && (c.config[0] & 8u),
                !hdri && (c.config[0] & 2u), !hdri && (c.config[0] & 2u) && c.body[3] > 0.f,
                !hdri && (c.config[0] & 64u), !hdri && (c.config[0] & 64u) && c.clouds.parameters[2] > 0.f};
            for (std::size_t i = 0; i < current.maps.size(); ++i)
                if (active[i] && maps[i]) current.maps[i] = {maps[i]->resourceView().identity(), maps[i]->resourceView().generation()};
            if (sky->constants.config[0] & 128u)
            {
                std::copy_n(&sky->constants.hdri_rotation[0][0], current.hdri_rotation.size(), current.hdri_rotation.begin());
                current.hdri_split = sky->constants.transfer[1];
            }
        }
        const bool changed = !temporal_previous_sky || *temporal_previous_sky != current;
        temporal_pending_sky = current;
        return changed;
    }

    void abandonTemporalFrame() noexcept
    {
        if (temporal_scaler && temporal_ticket) temporal_scaler->abandon(temporal_ticket);
        if (aurora_reconstructor && aurora_ticket) aurora_reconstructor->abandon(aurora_ticket);
        aurora_ticket = 0;
        temporal_ticket = 0;
        temporal_history.abandon();
        temporal_encoded = false;
        temporal_opaque_inputs.reset();
        temporal_pending_sky.reset();
    }

    bool prepareTemporalFrame(const ALRenderResourceInputs& inputs,
        const ALRenderResourceInputs::MainViewDimensions& dimensions, bool ordinary,
        const std::string& ineligible, const LLViewerCamera& camera, std::string* error)
    {
        // Native/Spatial frames do not update or allocate Temporal resources. Clean up once on mode exit.
        if (!inputs.mainResolutionPolicy().usesTemporal() && !temporal_stream_selected)
        { temporal_effective=false; temporal_jitter={}; return false; }
        abandonTemporalFrame();
        temporal_effective = false;
        temporal_submitted = false;
        temporal_jitter = {};
        temporal_raw_output.reset(); temporal_resolved_output.reset(); temporal_motion.reset();
        temporal_opaque_depth.reset(); aurora_opaque_color.reset();
        temporal_fallback.clear();
        if (!inputs.mainResolutionPolicy().usesTemporal())
        {
            temporal_history.invalidate();
            temporal_scaler.reset(); aurora_reconstructor.reset(); aurora_selected=false;
            temporal_stream_selected=false;
            temporal_previous_camera_valid = false;
            temporal_previous_sky.reset();
            return false;
        }
        temporal_stream_selected=true;
        const auto fail = [&](const std::string& reason)
        {
            temporal_fallback = reason.empty() ? "Temporal preparation is unavailable" : reason;
            temporal_history.invalidate();
            invalidateReconstruction();
            return false;
        };
        if (!ordinary || !ineligible.empty()) return fail(ineligible.empty()
            ? "This independent view does not use main-world Temporal history" : ineligible);
        const bool wants_aurora = inputs.mainResolutionPolicy().usesAurora();
        if (aurora_selected != wants_aurora)
        {
            temporal_history.invalidate(); invalidateReconstruction();
            temporal_scaler.reset(); aurora_reconstructor.reset();
            temporal_previous_camera_valid=false; temporal_previous_sky.reset();
        }
        aurora_selected = wants_aurora;
        bool output_shader_write = false;
        if (aurora_selected)
        {
            const auto* p=camera.getProjection().getF32ptr();
            const bool perspective=std::all_of(p,p+16,[](F32 v){return std::isfinite(v);}) &&
                std::abs(p[11]+1.f)<1.e-4f && std::abs(p[15])<1.e-6f && p[14]>0.f;
            if (!LLRender::sReverseZ || !perspective)
                return fail("Aurora requires reverse-Z perspective depth");
            if (!aurora_reconstructor) aurora_reconstructor = ALMetalAuroraReconstructor::create(*device,error);
            if (!aurora_reconstructor) return fail(error ? *error : "Aurora preparation failed");
            aurora_descriptor = {width,height,dimensions.mOutputWidth,dimensions.mOutputHeight};
            if (!aurora_reconstructor->prepare(aurora_descriptor,error))
                return fail(error ? *error : "Aurora resources are unavailable");
            aurora_opaque_color=renderer->prepareTemporalTarget(width,height,false,error);
            if (!aurora_opaque_color) return fail(error ? *error : "Aurora opaque HDR preservation is unavailable");
        }
        else
        {
            if (!temporal_scaler) temporal_scaler = ALMetalTemporalScaler::create(*device, error);
            if (!temporal_scaler) return fail(error ? *error : "Temporal is unsupported on this device");
            temporal_descriptor = {width, height, dimensions.mOutputWidth, dimensions.mOutputHeight};
            ALMetalTemporalScalerRequirements requirements;
            if (!temporal_scaler->prepare(temporal_descriptor, requirements, error))
                return fail(error ? *error : "Temporal resources are unavailable");
            output_shader_write=alMetalHasUsage(requirements.output_usage,ALMetalTextureUsage::shader_write);
        }
        if (!renderer->prepareTemporalMotion(error)) return fail(error ? *error : "Temporal motion is unavailable");
        if (reconstructionStats().gpu_failures != temporal_gpu_failures)
        {
            temporal_gpu_failures = reconstructionStats().gpu_failures;
            temporal_history.invalidate();
        }
        if (!aurora_selected)
            temporal_raw_output = renderer->prepareTemporalTarget(dimensions.mOutputWidth,
                dimensions.mOutputHeight,output_shader_write,error);
        temporal_resolved_output = renderer->prepareTemporalTarget(dimensions.mOutputWidth,
            dimensions.mOutputHeight,false,error);
        temporal_motion=renderer->prepareTemporalMotionTarget(width,height,error);
        temporal_opaque_depth=renderer->prepareTemporalOpaqueDepth(width,height,error);
        if ((!aurora_selected && !temporal_raw_output) || !temporal_resolved_output || !temporal_motion || !temporal_opaque_depth)
            return fail(error ? *error : "Temporal frame-input/output allocation failed");
        ALMetalTemporalWorldHistory::FrameInput frame;
        frame.world_id = ++temporal_world_serial;
        frame.input_width = width; frame.input_height = height;
        frame.output_width = dimensions.mOutputWidth; frame.output_height = dimensions.mOutputHeight;
        frame.mode = static_cast<U32>(inputs.mMainResolutionMode);
        if (aurora_selected)
        {
            const F32 ratio=F32(dimensions.mOutputWidth)/F32(width);
            frame.jitter_phase_count=std::clamp(U32(8.f*ratio*ratio),1u,128u);
        }
        frame.reverse_z = LLRender::sReverseZ;
        LLMatrix4a camera_mvp;
        camera_mvp.setMul(camera.getModelview(),camera.getProjection());
        std::copy_n(camera_mvp.getF32ptr(),16,frame.camera_mvp);
        if (gAgent.getRegion())
        {
            const auto& origin = gAgent.getRegion()->getOriginGlobal();
            for (U32 i=0;i<3;++i) frame.origin[i]=origin.mdV[i];
        }
        temporal_camera_position = camera.getOrigin();
        temporal_camera_direction = camera.getAtAxis();
        frame.force_reset = gAgent.getTeleportState() != LLAgent::TELEPORT_NONE ||
            (temporal_previous_camera_valid &&
             ((temporal_camera_position-temporal_previous_camera_position).lengthSquared()>4096.f ||
              temporal_camera_direction*temporal_previous_camera_direction < 0.3f));
        if (!temporal_history.beginFrame(frame,error)) return fail(error ? *error : "Temporal camera history is invalid");
        temporal_jitter = temporal_history.jitterPixels();
        temporal_effective = true;
        return true;
    }
