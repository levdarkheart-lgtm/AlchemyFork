/** One ordinary main-world Temporal stream; included inside MetalWorldSimpleCapture. */
    std::unique_ptr<ALMetalTemporalScaler> temporal_scaler;
    ALMetalTemporalWorldHistory temporal_history;
    ALMetalTemporalScalerDescriptor temporal_descriptor;
    std::shared_ptr<const ALMetalWorldViewInputs> temporal_opaque_inputs;
    std::unique_ptr<ALMetalRenderTarget> temporal_raw_output, temporal_resolved_output, temporal_motion;
    ALMetalTemporalScaler::Ticket temporal_ticket = 0;
    std::array<F32, 2> temporal_jitter{};
    U64 temporal_world_serial = 0, temporal_gpu_failures = 0;
    bool temporal_effective = false, temporal_encoded = false;
    std::string temporal_fallback;
    LLVector3 temporal_camera_position, temporal_previous_camera_position;
    LLVector3 temporal_camera_direction, temporal_previous_camera_direction;
    bool temporal_previous_camera_valid = false;

    void abandonTemporalFrame() noexcept
    {
        if (temporal_scaler && temporal_ticket) temporal_scaler->abandon(temporal_ticket);
        temporal_ticket = 0;
        temporal_history.abandon();
        temporal_encoded = false;
        temporal_opaque_inputs.reset();
    }

    bool prepareTemporalFrame(const ALRenderResourceInputs& inputs,
        const ALRenderResourceInputs::MainViewDimensions& dimensions, bool ordinary,
        const std::string& ineligible, const LLViewerCamera& camera, std::string* error)
    {
        abandonTemporalFrame();
        temporal_effective = false;
        temporal_jitter = {};
        temporal_raw_output.reset(); temporal_resolved_output.reset(); temporal_motion.reset();
        temporal_fallback.clear();
        if (!inputs.mainResolutionPolicy().usesTemporal())
        {
            temporal_history.invalidate();
            if (temporal_scaler) temporal_scaler->invalidate();
            temporal_previous_camera_valid = false;
            return false;
        }
        const auto fail = [&](const std::string& reason)
        {
            temporal_fallback = reason.empty() ? "Temporal preparation is unavailable" : reason;
            temporal_history.invalidate();
            if (temporal_scaler) temporal_scaler->invalidate();
            return false;
        };
        if (!ordinary || !ineligible.empty()) return fail(ineligible.empty()
            ? "This independent view does not use main-world Temporal history" : ineligible);
        if (!temporal_scaler) temporal_scaler = ALMetalTemporalScaler::create(*device, error);
        if (!temporal_scaler) return fail(error ? *error : "Temporal is unsupported on this device");
        temporal_descriptor = {width, height, dimensions.mOutputWidth, dimensions.mOutputHeight};
        ALMetalTemporalScalerRequirements requirements;
        if (!temporal_scaler->prepare(temporal_descriptor, requirements, error) ||
            !renderer->prepareTemporalMotion(error)) return fail(error ? *error : "Temporal resources are unavailable");
        if (temporal_scaler->stats().gpu_failures != temporal_gpu_failures)
        {
            temporal_gpu_failures = temporal_scaler->stats().gpu_failures;
            temporal_history.invalidate();
        }
        temporal_raw_output = renderer->prepareTemporalTarget(dimensions.mOutputWidth,
            dimensions.mOutputHeight, alMetalHasUsage(requirements.output_usage,ALMetalTextureUsage::shader_write),error);
        temporal_resolved_output = renderer->prepareTemporalTarget(dimensions.mOutputWidth,
            dimensions.mOutputHeight,false,error);
        if (!temporal_raw_output || !temporal_resolved_output) return fail(error ? *error : "Temporal output allocation failed");
        ALMetalTemporalWorldHistory::FrameInput frame;
        frame.world_id = ++temporal_world_serial;
        frame.input_width = width; frame.input_height = height;
        frame.output_width = dimensions.mOutputWidth; frame.output_height = dimensions.mOutputHeight;
        frame.mode = static_cast<U32>(inputs.mMainResolutionMode);
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
