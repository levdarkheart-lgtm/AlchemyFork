/** Main-world Temporal integration. Included only by almetalworld.cpp. */
std::unique_ptr<ALMetalRenderTarget>
ALMetalWorldGBufferRenderer::prepareTemporalTarget(std::uint32_t width,
    std::uint32_t height, bool shader_write, std::string* error)
{
    if (!mImpl || !mImpl->device || !width || !height) return nullptr;
    if (!mImpl->temporal_resolve_pipeline)
    {
        ALMetalRenderPipelineDescriptor pipeline;
        pipeline.library = mImpl->library.get();
        pipeline.vertex_function = "almetal_resample_vertex";
        pipeline.fragment_function = "almetal_world_temporal_color_fragment";
        pipeline.colors[0].format = ALMetalPixelFormat::rgba16_float;
        pipeline.label = "Alchemy Temporal RGB and world glow-alpha resolve";
        mImpl->temporal_resolve_pipeline = ALMetalRenderPipeline::create(*mImpl->device, pipeline, error);
        if (!mImpl->temporal_resolve_pipeline) return nullptr;
    }
    ALMetalRenderTargetDescriptor descriptor;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.colors[0].format = ALMetalPixelFormat::rgba16_float;
    descriptor.colors[0].shader_write = shader_write;
    descriptor.label = "Alchemy main-world Temporal HDR output";
    return mImpl->makeRenderTarget(*mImpl->device, descriptor, error);
}

bool ALMetalWorldGBufferRenderer::encodeTemporalUpscale(ALMetalTemporalScaler& scaler,
    const ALMetalTemporalScalerDescriptor& descriptor, const ALMetalTexture& color,
    const ALMetalRenderTarget& motion, ALMetalRenderTarget& output,
    const ALMetalTemporalScalerFrame& frame, ALMetalTemporalScaler::Ticket& ticket,
    std::string* error)
{
    Impl::GBufferFailureScope transaction(mImpl.get(), error);
    // Main Temporal always joins the enclosing required-world post transaction.
    // A standalone encode could advance native history before publication fails.
    if (!mImpl || !mImpl->command_batch || !mImpl->command_batch_valid)
    {
        setError(error, "Temporal reconstruction requires an active valid main-world transaction");
        return false;
    }
    ALMetalTemporalScalerInputs inputs;
    inputs.color = &color;
    inputs.motion = motion.colorTexture(0);
    inputs.reactive = motion.colorTexture(1);
    inputs.depth = motion.colorTexture(2);
    inputs.output = output.colorTexture(0);
    const bool encoded = scaler.encode(*mImpl->command_batch, descriptor, inputs, frame, ticket, error);
    if (encoded) mImpl->command_batch_temporal_encoded=true;
    transaction.succeeded = encoded;
    return encoded;
}

bool ALMetalWorldGBufferRenderer::resolveTemporalColor(const ALMetalRenderTarget& original,
    const ALMetalRenderTarget& reconstructed, ALMetalRenderTarget& output,
    std::string* error, const float* jitter_pixels)
{
    Impl::GBufferFailureScope transaction(mImpl.get(), error);
    if (!mImpl || !mImpl->command_batch || !mImpl->command_batch_valid ||
        !mImpl->temporal_resolve_pipeline || !original.colorTexture(0) ||
        !reconstructed.colorTexture(0) || !output.colorTexture(0) ||
        output.descriptor().colors[0].format != ALMetalPixelFormat::rgba16_float ||
        output.descriptor().width != reconstructed.descriptor().width ||
        output.descriptor().height != reconstructed.descriptor().height)
    {
        setError(error, "Temporal color resolve requires complete matching preallocated HDR outputs");
        return false;
    }
    const auto& input_descriptor=original.descriptor();
    const std::array<float,4> alpha_offset{
        jitter_pixels ? jitter_pixels[0]/float(input_descriptor.width) : 0.f,
        jitter_pixels ? jitter_pixels[1]/float(input_descriptor.height) : 0.f,0.f,0.f};
    if (!std::isfinite(alpha_offset[0]) || !std::isfinite(alpha_offset[1]))
    { setError(error,"Temporal glow-alpha jitter must be finite"); return false; }
    ALMetalRenderPassConfig config;
    config.colors[0].load = ALMetalLoadAction::dont_care;
    auto pass = output.makeRenderPass(config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(*mImpl->command_batch, *pass,
        "Alchemy Temporal RGB with original glow alpha", error) : nullptr;
    const auto& desc = output.descriptor();
    const ALMetalViewport viewport{0.,0.,double(desc.width),double(desc.height),0.,1.};
    const ALMetalScissorRect scissor{0,0,desc.width,desc.height};
    const bool encoded = encoder && encoder->setViewport(viewport,error) &&
        encoder->setScissor(scissor,error) && encoder->setCullMode(ALMetalCullMode::none,error) &&
        encoder->setPipeline(*mImpl->temporal_resolve_pipeline,error) &&
        encoder->setFragmentTexture(0,reconstructed.colorTexture(0),error) &&
        encoder->setFragmentTexture(1,original.colorTexture(0),error) &&
        encoder->setFragmentSampler(0,mImpl->linear_clamp_sampler.get(),error) &&
        encoder->setFragmentBytes(0,alpha_offset.data(),sizeof(alpha_offset),error) &&
        encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,error);
    const bool ended = encoder && encoder->end(error);
    transaction.succeeded = encoded && ended;
    return transaction.succeeded;
}
