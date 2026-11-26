/** Native Temporal motion replay. Included after the renderer's existing private implementation. */
namespace
{
struct alignas(16) WorldTemporalMotionConstants
{
    float current_mvp[16]{}, previous_mvp[16]{}, texture_matrix[16]{}, current_modelview[16]{};
    float current_wind[4]{}, current_sin[4]{}, current_gravity[4]{}, current_cloth_depth[4]{};
    float previous_wind[4]{}, previous_sin[4]{}, previous_gravity[4]{}, previous_cloth_depth[4]{};
    float extent_bias[4]{}, raster[4]{}, water_plane[4]{};
    std::uint32_t config[4]{}, coverage[4]{};
    float thresholds[8][4]{};
    float impostor_center[4]{}, impostor_left[4]{}, impostor_up[4]{};
};
struct alignas(16) WorldTemporalReprojectionConstants
{
    float current_to_previous[16]{}, inverse_projection[16]{}, eye_water_plane[4]{};
    float extent_jitter[4]{}, water_parameters[4]{};
    std::uint32_t config[4]{};
};
}

bool ALMetalWorldGBufferRenderer::prepareTemporalMotion(std::string* error)
{
    if (!mImpl || !mImpl->device || !mImpl->library)
    { setError(error, "Temporal motion renderer is unavailable"); return false; }
    if (mImpl->temporal_motion_background && mImpl->temporal_motion_geometry[0] &&
        mImpl->temporal_motion_geometry[1] && mImpl->temporal_motion_legacy_geometry[0] &&
        mImpl->temporal_motion_legacy_geometry[1] &&
        mImpl->temporal_motion_reactive_background &&
        mImpl->temporal_motion_impostor && mImpl->temporal_opaque_depth_copy &&
        mImpl->temporal_motion_seed_depth && mImpl->temporal_motion_overlay_depth) return true;
    ALMetalRenderPipelineDescriptor descriptor;
    descriptor.library = mImpl->library.get();
    descriptor.color_attachment_count = 3;
    descriptor.colors[0].format = ALMetalPixelFormat::rg16_float;
    descriptor.colors[1].format = ALMetalPixelFormat::r8_unorm;
    descriptor.colors[2].format = ALMetalPixelFormat::r32_float;
    descriptor.depth_stencil_format = ALMetalPixelFormat::depth32_float;
    descriptor.vertex_function = "almetal_resample_vertex";
    descriptor.fragment_function = "almetal_temporal_camera_motion_fragment";
    descriptor.label = "Alchemy Temporal camera/background motion";
    auto background = ALMetalRenderPipeline::create(*mImpl->device, descriptor, error);
    if (!background) return false;
    std::array<std::shared_ptr<ALMetalRenderPipeline>, 2> geometry;
    descriptor.vertex_function = "almetal_temporal_motion_vertex";
    descriptor.fragment_function = "almetal_temporal_motion_fragment";
    descriptor.label = "Alchemy Temporal object/skin motion and matched coverage";
    geometry[0] = ALMetalRenderPipeline::create(*mImpl->device, descriptor, error);
    if (!geometry[0]) return false;
    std::array<std::shared_ptr<ALMetalRenderPipeline>, 2> legacy_geometry;
    descriptor.vertex_function = "almetal_temporal_legacy_motion_vertex";
    descriptor.fragment_function = "almetal_temporal_legacy_motion_fragment";
    descriptor.label = "Alchemy Temporal legacy glass with actual color coverage";
    legacy_geometry[0] = ALMetalRenderPipeline::create(*mImpl->device, descriptor, error);
    if (!legacy_geometry[0]) return false;
    descriptor.colors[0].write_mask = ALMetalColorWriteMask::none;
    descriptor.colors[2].write_mask = ALMetalColorWriteMask::none;
    descriptor.colors[1].write_mask = ALMetalColorWriteMask::none;
    descriptor.label = "Alchemy Temporal legacy glass depth-only visibility";
    legacy_geometry[1] = ALMetalRenderPipeline::create(*mImpl->device, descriptor, error);
    if (!legacy_geometry[1]) return false;
    descriptor.vertex_function = "almetal_temporal_motion_vertex";
    descriptor.fragment_function = "almetal_temporal_motion_fragment";
    descriptor.label = "Alchemy Temporal alpha depth-only visibility";
    geometry[1] = ALMetalRenderPipeline::create(*mImpl->device, descriptor, error);
    if (!geometry[1]) return false;
    descriptor.colors[1].write_mask = ALMetalColorWriteMask::all;
    descriptor.colors[1].blending_enabled = true;
    descriptor.colors[1].source_rgb = ALMetalBlendFactor::one;
    descriptor.colors[1].destination_rgb = ALMetalBlendFactor::one;
    descriptor.colors[1].rgb_operation = ALMetalBlendOperation::maximum;
    descriptor.vertex_function = "almetal_resample_vertex";
    descriptor.fragment_function = "almetal_temporal_camera_motion_fragment";
    descriptor.label = "Alchemy Temporal background/water rejection union";
    auto reactive_background = ALMetalRenderPipeline::create(*mImpl->device, descriptor, error);
    if (!reactive_background) return false;
    descriptor.vertex_function = "almetal_world_impostor_vertex";
    descriptor.fragment_function = "almetal_temporal_impostor_reactive_fragment";
    descriptor.label = "Alchemy Temporal impostor history rejection";
    auto impostor = ALMetalRenderPipeline::create(*mImpl->device, descriptor, error);
    if (!impostor) return false;
    descriptor.color_attachment_count = 1;
    descriptor.depth_stencil_format = ALMetalPixelFormat::invalid;
    descriptor.colors[0] = {};
    descriptor.colors[0].format = ALMetalPixelFormat::r32_float;
    descriptor.vertex_function = "almetal_resample_vertex";
    descriptor.fragment_function = "almetal_temporal_opaque_depth_fragment";
    descriptor.label = "Alchemy Temporal pristine opaque depth copy";
    auto depth_copy = ALMetalRenderPipeline::create(*mImpl->device, descriptor, error);
    if (!depth_copy) return false;
    ALMetalDepthStencilDescriptor seed;
    seed.depth_compare = ALMetalCompareFunction::always;
    seed.depth_write_enabled = true;
    seed.label = "Alchemy Temporal seed private visibility depth";
    auto seed_depth = ALMetalDepthStencilState::create(*mImpl->device, seed, error);
    seed.depth_write_enabled = false;
    seed.label = "Alchemy Temporal background rejection depth-independent overlay";
    auto overlay_depth = ALMetalDepthStencilState::create(*mImpl->device, seed, error);
    if (!seed_depth || !overlay_depth) return false;
    mImpl->temporal_opaque_depth_copy = std::move(depth_copy);
    mImpl->temporal_motion_impostor = std::move(impostor);
    mImpl->temporal_motion_reactive_background = std::move(reactive_background);
    mImpl->temporal_motion_background = std::move(background);
    mImpl->temporal_motion_geometry = std::move(geometry);
    mImpl->temporal_motion_legacy_geometry = std::move(legacy_geometry);
    mImpl->temporal_motion_seed_depth = std::move(seed_depth);
    mImpl->temporal_motion_overlay_depth = std::move(overlay_depth);
    return true;
}

std::unique_ptr<ALMetalRenderTarget> ALMetalWorldGBufferRenderer::prepareTemporalOpaqueDepth(
    std::uint32_t width, std::uint32_t height, std::string* error)
{
    if (!width || !height || !prepareTemporalMotion(error)) return {};
    ALMetalRenderTargetDescriptor descriptor;
    descriptor.width = width; descriptor.height = height;
    descriptor.colors[0].format = ALMetalPixelFormat::r32_float;
    descriptor.label = "Alchemy Temporal immutable opaque device depth";
    return mImpl->makeRenderTarget(*mImpl->device, descriptor, error);
}

bool ALMetalWorldGBufferRenderer::captureTemporalOpaqueDepth(const ALMetalTexture& depth,
    ALMetalRenderTarget& output, std::string* error)
{
    Impl::GBufferFailureScope transaction(mImpl.get(), error);
    const auto& descriptor = output.descriptor();
    if (!mImpl || !mImpl->command_batch || !mImpl->command_batch_valid ||
        mImpl->command_batch_temporal_encoded || !mImpl->temporal_opaque_depth_copy ||
        descriptor.sample_count != 1 || descriptor.color_attachment_count != 1 ||
        descriptor.colors[0].format != ALMetalPixelFormat::r32_float ||
        !descriptor.width || !descriptor.height || !output.colorTexture(0) ||
        depth.descriptor().format != ALMetalPixelFormat::depth32_float_stencil8 ||
        depth.descriptor().sample_count != 1 || !sampleableTexture(&depth) ||
        depth.width() != descriptor.width || depth.height() != descriptor.height)
    { setError(error, "Temporal opaque depth capture requires a prepared matching target and active main batch"); return false; }
    ALMetalRenderPassConfig config;
    config.colors[0].load = ALMetalLoadAction::dont_care;
    auto pass = output.makeRenderPass(config, error);
    auto encoder = pass ? ALMetalRenderEncoder::begin(*mImpl->command_batch, *pass,
        "Alchemy Temporal preserve opaque depth before forward", error) : nullptr;
    const bool encoded = encoder && encoder->setViewport({0.,0.,double(descriptor.width),double(descriptor.height),0.,1.},error) &&
        encoder->setScissor({0,0,descriptor.width,descriptor.height},error) &&
        encoder->setCullMode(ALMetalCullMode::none,error) &&
        encoder->setPipeline(*mImpl->temporal_opaque_depth_copy,error) &&
        encoder->setFragmentTextureLinear(32,&depth,error) &&
        encoder->draw(ALMetalPrimitiveType::triangle,0,3,1,error);
    const bool ended = encoder && encoder->end(error);
    transaction.succeeded = encoded && ended;
    return transaction.succeeded;
}

std::unique_ptr<ALMetalRenderTarget> ALMetalWorldGBufferRenderer::prepareTemporalMotionTarget(
    std::uint32_t width, std::uint32_t height, std::string* error)
{
    if (!width || !height || !prepareTemporalMotion(error)) return {};
    ALMetalRenderTargetDescriptor desc;
    desc.width = width; desc.height = height;
    desc.color_attachment_count = 3;
    desc.colors[0].format = ALMetalPixelFormat::rg16_float;
    desc.colors[0].label = "Alchemy Temporal current-to-previous input pixels";
    desc.colors[1].format = ALMetalPixelFormat::r8_unorm;
    desc.colors[1].label = "Alchemy Temporal reactive rejection";
    desc.colors[2].format = ALMetalPixelFormat::r32_float;
    desc.colors[2].label = "Alchemy Temporal raw device depth";
    // Reproduce color's evolving depth test independently of the motion/depth
    // proxy. In particular post-water alpha is read-only while pre-water alpha
    // writes depth. Tile-local storage lives for this single render encoder.
    desc.depth_stencil_format = ALMetalPixelFormat::depth32_float;
    desc.memoryless_depth_stencil = true;
    desc.label = "Alchemy Temporal motion/depth/rejection inputs";
    return mImpl->makeRenderTarget(*mImpl->device, desc, error);
}

std::unique_ptr<ALMetalRenderTarget> ALMetalWorldGBufferRenderer::renderTemporalMotion(
    const ALMetalWorldTemporalMotionScene& scene, std::string* error,
    std::unique_ptr<ALMetalRenderTarget> prepared_output)
try
{
    Impl::GBufferFailureScope transaction(mImpl.get(), error);
    if (!scene.width || !scene.height || !scene.depth || !scene.opaque_scene ||
        scene.depth->descriptor().format != ALMetalPixelFormat::depth32_float_stencil8 ||
        scene.depth->descriptor().sample_count != 1 || !sampleableTexture(scene.depth) ||
        scene.depth->descriptor().width != scene.width || scene.depth->descriptor().height != scene.height ||
        (scene.opaque_depth && (scene.opaque_depth->descriptor().format != ALMetalPixelFormat::r32_float ||
            scene.opaque_depth->descriptor().sample_count != 1 || !sampleableTexture(scene.opaque_depth) ||
            scene.opaque_depth->width() != scene.width || scene.opaque_depth->height() != scene.height)) ||
        !finiteValues(scene.clip_to_previous_clip, 16) || !finiteValues(scene.jitter_pixels, 2) ||
        !std::isfinite(scene.water_rejection_margin) || scene.water_rejection_margin < 0.f ||
        ((scene.reject_water || scene.modern_sky) && !finiteValues(scene.inverse_projection, 16)) ||
        (scene.reject_water && !finiteValues(scene.eye_water_plane, 4)) ||
        (scene.modern_sky && (scene.modern_sky->width != scene.width || scene.modern_sky->height != scene.height ||
            !modernSkyValid(*scene.modern_sky, true, error))) ||
        (scene.previous_opaque_count && !scene.previous_opaque) ||
        (scene.previous_forward_count && !scene.previous_forward) ||
        !prepareTemporalMotion(error))
    { setError(error, "Temporal motion inputs are incomplete or incompatible"); return {}; }
    const auto& opaque = *scene.opaque_scene;
    if (!alMetalValidateWorldView(opaque, error)) return {};
    auto target = std::move(prepared_output);
    if (target)
    {
        const auto& descriptor = target->descriptor();
        if (descriptor.width != scene.width || descriptor.height != scene.height ||
            descriptor.sample_count != 1 || descriptor.color_attachment_count != 3 ||
            descriptor.colors[0].format != ALMetalPixelFormat::rg16_float ||
            descriptor.colors[1].format != ALMetalPixelFormat::r8_unorm ||
            descriptor.colors[2].format != ALMetalPixelFormat::r32_float ||
            descriptor.depth_stencil_format != ALMetalPixelFormat::depth32_float)
        { setError(error, "Temporal prepared motion target is incompatible"); return {}; }
    }
    else target = prepareTemporalMotionTarget(scene.width, scene.height, error);
    if (!target) return {};
    ALMetalRenderPassConfig config;
    for (unsigned i = 0; i != 3; ++i)
    { config.colors[i].load = ALMetalLoadAction::clear; config.colors[i].store = ALMetalStoreAction::store; }
    config.clear_depth = scene.reverse_z ? 0. : 1.;
    config.depth_store = ALMetalStoreAction::dont_care;
    auto pass = target->makeRenderPass(config, error);
    std::unique_ptr<ALMetalCommandBuffer> owned;
    auto* command = pass ? mImpl->acquireCommandBuffer("Alchemy Temporal motion inputs", owned, error) : nullptr;
    if (!command) return {};
    if (opaque.source_owner && !command->retainUntilCompletion(opaque.source_owner, error)) return {};
    for (std::size_t i = 0; i < opaque.cohort_count; ++i)
        if (!command->retainUntilCompletion(opaque.cohorts[i].native_owner, error)) return {};
    if (scene.forward_scene && scene.forward_scene->source_owner &&
        !command->retainUntilCompletion(scene.forward_scene->source_owner, error)) return {};
    auto encoder = ALMetalRenderEncoder::begin(*command, *pass, "Alchemy Temporal motion and rejection", error);
    if (!encoder) return {};
    WorldTemporalReprojectionConstants reprojection;
    std::memcpy(reprojection.current_to_previous, scene.clip_to_previous_clip, sizeof(reprojection.current_to_previous));
    reprojection.extent_jitter[0] = float(scene.width); reprojection.extent_jitter[1] = float(scene.height);
    reprojection.extent_jitter[2] = scene.jitter_pixels[0]; reprojection.extent_jitter[3] = scene.jitter_pixels[1];
    std::memcpy(reprojection.inverse_projection, scene.inverse_projection, sizeof(reprojection.inverse_projection));
    std::memcpy(reprojection.eye_water_plane, scene.eye_water_plane, sizeof(reprojection.eye_water_plane));
    reprojection.config[0] = scene.reverse_z; reprojection.config[1] = scene.reset;
    reprojection.config[2] = scene.reject_water;
    reprojection.config[3] = (scene.modern_sky ? 1u : 0u) | (scene.reject_sky_history ? 2u : 0u);
    reprojection.water_parameters[0] = std::max(scene.water_rejection_margin, 0.f);
    const ALMetalWorldModernSkyScene empty_sky;
    const auto& sky = scene.modern_sky ? *scene.modern_sky : empty_sky;
    const auto bind_background = [&]()
    {
        bool bound = encoder->setPipeline(*mImpl->temporal_motion_background, error) &&
            mImpl->setFragmentData(*encoder, *command, 0, &reprojection, sizeof(reprojection), error) &&
            mImpl->setFragmentData(*encoder, *command, 1, &sky.constants, sizeof(sky.constants), error) &&
            mImpl->setFragmentData(*encoder, *command, 2, sky.meteor_data, sizeof(sky.meteor_data), error) &&
            encoder->setFragmentTextureLinear(32, scene.depth, error) &&
            encoder->setFragmentTextureLinear(33, scene.opaque_depth ? scene.opaque_depth : scene.depth, error);
        const ALMetalTexture* maps[] = {sky.rainbow_texture, sky.halo_texture, sky.hdri_texture,
            sky.stars_texture, sky.sun_texture, sky.sun_next_texture, sky.moon_texture,
            sky.bloom_texture, sky.bloom_next_texture, sky.cloud_texture, sky.cloud_next_texture};
        for (std::uint32_t i = 0; bound && i < 11; ++i)
        {
            const auto* map = maps[i] ? maps[i] : mImpl->inactive_ssr_color.get();
            bound = i == 3 ? encoder->setFragmentTexture(i, map, error) :
                encoder->setFragmentTextureLinear(i, map, error);
        }
        return bound && encoder->setFragmentSampler(0, mImpl->diffuse_sampler.get(), error) &&
            encoder->setFragmentSampler(1, mImpl->ramp_sampler.get(), error) &&
            encoder->setFragmentSampler(2, mImpl->point_clamp_sampler.get(), error) &&
            encoder->setFragmentSampler(3, mImpl->hdri_sampler.get(), error);
    };
    bool ok = encoder->setViewport({0, 0, double(scene.width), double(scene.height), 0, 1}, error) &&
        encoder->setScissor({0, 0, scene.width, scene.height}, error) &&
        encoder->setFrontFacingWinding(ALMetalWinding::clockwise, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setDepthStencilState(mImpl->temporal_motion_seed_depth.get(), error) &&
        bind_background() &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    std::vector<std::unique_ptr<ALMetalBuffer>> palette_storage;
    const auto bind_palette = [&](unsigned slot, const float* bytes, std::size_t count, const ALMetalBuffer& dummy)
    {
        if (!bytes || !count) return encoder->setVertexBuffer(slot, dummy, 0, error);
        if (count * sizeof(float) <= 4096 || mImpl->frame_data)
            return mImpl->setVertexData(*encoder, *command, slot, bytes, count * sizeof(float), error);
        ALMetalBufferDescriptor palette_desc;
        palette_desc.length = count * sizeof(float); palette_desc.storage = ALMetalStorageMode::shared;
        palette_desc.label = "Alchemy Temporal previous/current pose";
        auto buffer = ALMetalBuffer::create(*mImpl->device, palette_desc, error);
        if (!buffer || !buffer->write(0, bytes, palette_desc.length, error)) return false;
        const bool bound = encoder->setVertexBuffer(slot, *buffer, 0, error);
        palette_storage.push_back(std::move(buffer)); return bound;
    };
    // One immutable lighting pack for every legacy replay consumer in this
    // command, matching the forward color owner's publication contract.
    ALMetalWorldDataSlice legacy_probe_slice;
    const ALMetalWorldReflectionProbeConstants legacy_probe_constants =
        scene.forward_scene && scene.forward_scene->reflection_probes
            ? scene.forward_scene->reflection_probes->constants : ALMetalWorldReflectionProbeConstants{};
    const auto draw_motion = [&](const ALMetalWorldDraw& draw,
        const ALMetalWorldTemporalPreviousDraw* previous, bool forward) -> bool
    {
        const auto& batch = draw.source();
        // Terrain deformation is already represented by the authoritative depth; static region geometry uses
        // camera reprojection. Water's dedicated generated mesh is handled by the main-view reactive owner.
        if (batch.terrain || batch.water_surface || batch.water_exclusion || batch.water_haze ||
            batch.atmospheric_haze || batch.legacy_glow || batch.pbr_glow || batch.post_water_glow) return true;
        if (batch.impostor)
        {
            auto view = opaque;
            view.temporal_jitter_pixels[0] = scene.jitter_pixels[0];
            view.temporal_jitter_pixels[1] = scene.jitter_pixels[1];
            const auto constants = buildWorldImpostorConstants(view, batch);
            return encoder->setPipeline(*mImpl->temporal_motion_impostor, error) &&
                encoder->setDepthStencilState(mImpl->temporal_motion_overlay_depth.get(), error) &&
                encoder->setCullMode(ALMetalCullMode::none, error) &&
                mImpl->setVertexData(*encoder, *command, 0, &constants, sizeof(constants), error) &&
                encoder->setFragmentTextureLinear(0, batch.impostor_textures[0], error) &&
                encoder->setFragmentTextureLinear(32, scene.depth, error) &&
                encoder->setFragmentTextureLinear(33, scene.opaque_depth ? scene.opaque_depth : scene.depth, error) &&
                encoder->setFragmentSampler(0, mImpl->point_clamp_sampler.get(), error) &&
                encoder->draw(ALMetalPrimitiveType::triangle, 0, 6, 1, error);
        }
        if (!batch.vertices || !batch.indices || !batch.index_count || !batch.vertex_count) return true;
        const bool body = batch.avatar_skinning;
        const bool attachment = !body && draw.skinPalette() && draw.skinJointCount();
        const auto* palette = draw.skinPalette();
        const auto palette_count = draw.skinPaletteFloatCount();
        const bool palette_valid = !palette || (previous && previous->skin_joint_count == draw.skinJointCount() &&
            previous->skin_palette.size() == palette_count);
        const bool valid = previous && previous->valid && palette_valid && !scene.reset;
        const bool alpha_family = forward && (batch.pre_water_alpha || batch.post_water_alpha || batch.avatar_alpha);
        WorldTemporalMotionConstants constants;
        std::memcpy(constants.current_mvp, draw.modelviewProjection(), sizeof(constants.current_mvp));
        std::memcpy(constants.previous_mvp, valid ? previous->modelview_projection : draw.modelviewProjection(), sizeof(constants.previous_mvp));
        std::memcpy(constants.texture_matrix, draw.textureMatrix(), sizeof(constants.texture_matrix));
        std::memcpy(constants.current_modelview, draw.modelview(), sizeof(constants.current_modelview));
        std::memcpy(constants.current_wind, batch.avatar_wind, sizeof(constants.current_wind));
        std::memcpy(constants.current_sin, batch.avatar_sin_wave, sizeof(constants.current_sin));
        std::memcpy(constants.current_gravity, batch.avatar_gravity, sizeof(constants.current_gravity));
        std::memcpy(constants.current_cloth_depth, batch.avatar_cloth_depth, sizeof(constants.current_cloth_depth));
        std::memcpy(constants.previous_wind, valid ? previous->avatar_wind : batch.avatar_wind, sizeof(constants.previous_wind));
        std::memcpy(constants.previous_sin, valid ? previous->avatar_sin_wave : batch.avatar_sin_wave, sizeof(constants.previous_sin));
        std::memcpy(constants.previous_gravity, valid ? previous->avatar_gravity : batch.avatar_gravity, sizeof(constants.previous_gravity));
        std::memcpy(constants.previous_cloth_depth, valid ? previous->avatar_cloth_depth : batch.avatar_cloth_depth, sizeof(constants.previous_cloth_depth));
        constants.extent_bias[0] = float(scene.width); constants.extent_bias[1] = float(scene.height);
        constants.extent_bias[2] = forward && scene.forward_scene ? scene.forward_scene->material_mip_bias : opaque.material_mip_bias;
        constants.extent_bias[3] = valid && !previous->reactive ? 0.f : 1.f;
        constants.raster[0] = scene.jitter_pixels[0]; constants.raster[1] = scene.jitter_pixels[1];
        constants.raster[2] = alpha_family ? batch.pre_water_minimum_alpha : batch.alpha_cutoff;
        constants.config[0] = attachment ? 1 : body ? (batch.avatar_cloth ? 3 : 2) : 0;
        constants.config[1] = draw.skinJointCount(); constants.config[2] = valid ? previous->skin_joint_count : draw.skinJointCount();
        constants.config[3] = scene.reverse_z;
        const unsigned family = batch.pbr_slot_count ? 2 : batch.material_slot_count ? 1 : 0;
        constants.coverage[0] = batch.texture_count; constants.coverage[1] = family;
        constants.coverage[2] = batch.material_alpha_mode;
        constants.coverage[3] = (batch.alpha_mask ? 1u : 0u) | (batch.use_vertex_color ? 2u : 0u) |
            (batch.alpha_cutoff_bias ? 4u : 0u) | (forward ? 8u : 0u) | ((batch.avatar_skinning || batch.avatar_rigid) ? 16u : 0u);
        if (alpha_family) constants.coverage[3] |= 512u;
        // Opaque-forward fullbright MASK tests texture alpha only, using its
        // fixed quantized cutoff. Streamed vertex alpha is shininess there.
        if (forward && !alpha_family && batch.impostor_fullbright && batch.alpha_mask)
            constants.coverage[3] |= 4u;
        const bool animated_uv = valid && std::memcmp(previous->texture_matrix, draw.textureMatrix(), sizeof(previous->texture_matrix)) != 0;
        if (forward || animated_uv) constants.extent_bias[3] = 1.f;
        if (alpha_family && scene.forward_scene && scene.forward_scene->alpha_environment)
        {
            const auto& environment = *scene.forward_scene->alpha_environment;
            std::copy_n(environment.water_plane, 4, constants.water_plane);
            constants.raster[3] = environment.water_fog[2] * (batch.post_water_alpha ? -1.f : 1.f);
            constants.coverage[3] |= 256u;
        }
        const bool generic_alpha = forward && !family &&
            (batch.pre_water_alpha || batch.post_water_alpha) &&
            (batch.pre_water_fullbright || batch.pre_water_simple) &&
            !batch.pre_water_material && !batch.pre_water_pbr && !batch.pre_water_particle &&
            !batch.avatar_alpha && !body && !batch.avatar_rigid && !batch.alpha_mask &&
            !batch.impostor_fullbright && !batch.impostor_shiny && !batch.impostor_emboss;
        if (generic_alpha) constants.coverage[3] |= 64u; // actual forward color always multiplies vertex alpha
        // Every validated glTF slot uses texture alpha * encoded vertex alpha.
        // The vertex path selects the same per-slot base UV transform as color;
        // LLFace baked baseColorFactor into that vertex attribute exactly once.
        const bool pbr_alpha = forward && family == 2 && batch.pbr_slot_count >= 1 &&
            batch.pbr_slot_count <= batch.max_material_slots && batch.texture_count == batch.pbr_slot_count &&
            (batch.pre_water_alpha || batch.post_water_alpha) && batch.pre_water_pbr &&
            !batch.material_slot_count && !batch.pre_water_material && !batch.pre_water_simple &&
            !batch.pre_water_fullbright && !batch.pre_water_particle && !batch.avatar_alpha &&
            !body && !batch.avatar_rigid && !batch.alpha_mask &&
            !batch.impostor_fullbright && !batch.impostor_shiny && !batch.impostor_emboss;
        // Legacy BLEND has lighting-dependent coverage. Its dedicated shader
        // calls the actual rigid/object-skinned material color helper instead
        // of guessing raw diffuse alpha. Avatar/particle/special color families
        // have different shading contracts and retain conservative rejection.
        const bool legacy_alpha = forward && family == 1 && batch.pre_water_material &&
            batch.material_alpha_mode == 1 && (batch.pre_water_alpha || batch.post_water_alpha) &&
            !batch.pre_water_pbr && !batch.pre_water_simple && !batch.pre_water_fullbright &&
            !batch.pre_water_particle && !batch.avatar_alpha && !body && !batch.avatar_rigid &&
            !batch.alpha_mask && !batch.impostor_fullbright && !batch.impostor_shiny && !batch.impostor_emboss;
        if ((generic_alpha || pbr_alpha || legacy_alpha) && scene.opaque_depth &&
            batch.pre_water_source_rgb == ALMetalBlendFactor::source_alpha &&
            batch.pre_water_destination_rgb == ALMetalBlendFactor::one_minus_source_alpha)
        {
            // Current coverage/depth is useful even when previous pose or UV
            // history is unavailable. History permission is a separate flag.
            constants.coverage[3] |= 128u;
            if (valid && !previous->reactive && !animated_uv) constants.coverage[3] |= 32u;
        }
        for (unsigned i = 0; i < 8; ++i)
        {
            constants.thresholds[i][0] = batch.alpha_cutoff;
            constants.thresholds[i][1] = 1.f;
            if (family == 1 && i < batch.material_slot_count) constants.thresholds[i][0] = batch.materialSlots()[i].alpha_cutoff;
            if (family == 2 && i < batch.pbr_slot_count)
            { constants.thresholds[i][0] = batch.pbrSlots()[i].alpha_cutoff;
              constants.thresholds[i][1] = batch.pbrSlots()[i].base_color_factor[3]; }
        }
        const unsigned motion_pipeline = forward && batch.post_water_depth_only ? 1u : 0u;
        const bool writes_depth = forward && worldForwardWritesDepth(batch, attachment || body);
        // Opaque replay retains its existing explicit depth-match tolerance;
        // only forward composition needs the evolving hardware visibility test.
        const auto* depth_state = !forward ? mImpl->temporal_motion_overlay_depth.get() : scene.reverse_z
            ? (writes_depth ? mImpl->reverse_depth.get() : mImpl->reverse_readonly_depth.get())
            : (writes_depth ? mImpl->forward_depth.get() : mImpl->forward_readonly_depth.get());
        if (legacy_alpha && (!scene.forward_scene || !scene.forward_scene->alpha_environment ||
            !validLegacyAlphaLighting(scene.forward_scene->alpha_lighting) ||
            batch.material_slot_count != batch.texture_count ||
            batch.material_slot_count > batch.max_material_slots ||
            (scene.forward_scene->reflection_probes && !validPBRReflectionProbes(scene.forward_scene->reflection_probes)) ||
            (scene.forward_scene->legacy_environment && !validLegacyEnvironment(scene.forward_scene->legacy_environment)) ||
            (batch.pre_water_shadows && (!scene.forward_scene->alpha_directional_shadows ||
                !validDirectionalShadows(scene.forward_scene->alpha_directional_shadows))) ||
            (scene.forward_scene->hero_probe && !validHeroProbe(scene.forward_scene->hero_probe)) ||
            (scene.forward_scene->screen_space_reflections && !validScreenSpaceReflections(scene.forward_scene->screen_space_reflections)) ||
            (validPBRReflectionProbes(scene.forward_scene->reflection_probes) ==
             validLegacyEnvironment(scene.forward_scene->legacy_environment))))
        { setError(error, "Temporal legacy coverage requires the actual forward material/environment inputs"); return false; }
        bool encoded = encoder->setPipeline(*(legacy_alpha ? mImpl->temporal_motion_legacy_geometry[motion_pipeline] :
            mImpl->temporal_motion_geometry[motion_pipeline]), error) &&
            encoder->setDepthStencilState(depth_state, error) &&
            encoder->setCullMode(forward ? worldAlphaCullMode(batch) : batch.double_sided ? ALMetalCullMode::none :
                (body || batch.avatar_rigid) ? batch.avatar_cull_mode : ALMetalCullMode::back, error) &&
            encoder->setVertexBuffer(0, *batch.vertices, batch.position_offset, error) &&
            encoder->setVertexBuffer(1, *batch.vertices, batch.normal_offset, error) &&
            encoder->setVertexBuffer(2, *batch.vertices, batch.texcoord_offset, error) &&
            encoder->setVertexBuffer(3, *batch.vertices, batch.color_offset, error) &&
            encoder->setVertexBuffer(6, *batch.vertices, batch.weight_offset, error) &&
            encoder->setVertexBuffer(8, *batch.vertices, batch.cloth_offset, error) &&
            encoder->setVertexBuffer(9, *batch.vertices, batch.weight_offset, error) &&
            bind_palette(5, palette, palette_count, *batch.vertices) &&
            bind_palette(7, valid && !previous->skin_palette.empty() ? previous->skin_palette.data() : palette,
                valid && !previous->skin_palette.empty() ? previous->skin_palette.size() : palette_count, *batch.vertices) &&
            mImpl->setVertexAndFragmentData(*encoder, *command, 4, legacy_alpha ? 13u : 1u, &constants, sizeof(constants), error) &&
            mImpl->setFragmentData(*encoder, *command, legacy_alpha ? 12u : 0u, &reprojection, sizeof(reprojection), error) &&
            encoder->setFragmentTextureLinear(32, scene.depth, error) &&
            encoder->setFragmentTextureLinear(33, scene.opaque_depth ? scene.opaque_depth : scene.depth, error) &&
            encoder->setFragmentSampler(0, mImpl->diffuse_sampler.get(), error);
        WorldPBRConstants pbr{};
        if (family == 2) pbr = buildWorldPBRConstants(opaque, batch);
        encoded = encoded && mImpl->setVertexData(*encoder, *command, 11, &pbr, sizeof(pbr), error);
        for (unsigned t = 0; encoded && !legacy_alpha && t < 32; ++t)
        {
            const auto* texture = t < batch.texture_count ? batch.diffuse_textures[t] : batch.diffuse_textures[0];
            encoded = encoder->setFragmentTexture(t, texture, error);
        }
        if (encoded && legacy_alpha)
        {
            const auto& forward_scene = *scene.forward_scene;
            const bool normal_map = materialHasNormalMap(batch);
            const bool specular_map = materialHasSpecularMap(batch);
            WorldShinyConstants world{};
            std::copy_n(draw.modelviewProjection(),16,world.modelview_projection);
            alMetalApplyTemporalJitter(world.modelview_projection, scene.jitter_pixels[0], scene.jitter_pixels[1],
                scene.width,scene.height);
            std::copy_n(draw.modelview(),16,world.modelview);
            std::memcpy(world.normal_matrix,draw.normalMatrix(),sizeof(world.normal_matrix));
            std::memcpy(world.rigged_normal_matrix,draw.riggedNormalMatrix(),sizeof(world.rigged_normal_matrix));
            std::copy_n(draw.textureMatrix(),16,world.texture_matrix);
            world.config[0]=scene.reverse_z; world.config[1]=batch.texture_count;
            world.config[2]=forward_scene.mirror_clip;
            std::copy_n(forward_scene.mirror_clip_plane,4,world.mirror_clip_plane);
            const auto material = buildWorldMaterialConstants(opaque,batch,normal_map,specular_map);
            auto environment = *forward_scene.alpha_environment;
            environment.environment_config[0]=validLegacyEnvironment(forward_scene.legacy_environment) ? 1u : 0u;
            if (batch.post_water_alpha) environment.water_fog[2]*=-1.f;
            environment.water_fog[3]=batch.pre_water_minimum_alpha;
            const auto& shadows = batch.pre_water_shadows ? forward_scene.alpha_directional_shadows->constants :
                mImpl->unshadowed_constants;
            const auto* shadow_map = batch.pre_water_shadows ? forward_scene.alpha_directional_shadows->depth_texture :
                mImpl->unshadowed_depth.get();
            const auto& hero = forward_scene.hero_probe ? forward_scene.hero_probe->constants : mImpl->inactive_hero_constants;
            const auto& ssr = forward_scene.screen_space_reflections ? forward_scene.screen_space_reflections->constants :
                mImpl->inactive_ssr_constants;
            if (!legacy_probe_slice.buffer && !mImpl->publishLightingData(*command,&legacy_probe_constants,
                sizeof(legacy_probe_constants),legacy_probe_slice,error,true)) return false;
            encoded = encoder->setVertexBuffer(8,*batch.vertices,normal_map ? batch.tangent_offset : batch.normal_offset,error) &&
                encoder->setVertexBuffer(9,*batch.vertices,normal_map ? batch.texcoord1_offset : batch.texcoord_offset,error) &&
                encoder->setVertexBuffer(10,*batch.vertices,specular_map ? batch.texcoord2_offset : batch.texcoord_offset,error) &&
                mImpl->setVertexData(*encoder,*command,12,&world,sizeof(world),error) &&
                mImpl->setFragmentData(*encoder,*command,0,&material,sizeof(material),error,true) &&
                encoder->setFragmentBuffer(1,*legacy_probe_slice.buffer,legacy_probe_slice.offset,error) &&
                mImpl->setFragmentData(*encoder,*command,2,forward_scene.alpha_lighting,sizeof(*forward_scene.alpha_lighting),error) &&
                mImpl->setFragmentData(*encoder,*command,3,&shadows,sizeof(shadows),error) &&
                mImpl->setFragmentData(*encoder,*command,4,&hero,sizeof(hero),error) &&
                mImpl->setFragmentData(*encoder,*command,5,&ssr,sizeof(ssr),error) &&
                mImpl->setFragmentData(*encoder,*command,6,&environment,sizeof(environment),error) &&
                encoder->setFragmentSampler(1,mImpl->probe_sampler.get(),error) &&
                encoder->setFragmentSampler(2,mImpl->linear_clamp_sampler.get(),error) &&
                encoder->setFragmentSampler(3,mImpl->point_clamp_sampler.get(),error) &&
                encoder->setFragmentTexture(32,forward_scene.reflection_probes ? forward_scene.reflection_probes->radiance_texture :
                    mImpl->inactive_probe_radiance.get(),error) &&
                encoder->setFragmentTexture(33,shadow_map,error) &&
                encoder->setFragmentTexture(34,forward_scene.hero_probe ? forward_scene.hero_probe->radiance_texture :
                    mImpl->inactive_hero_probe.get(),error) &&
                encoder->setFragmentTexture(35,forward_scene.screen_space_reflections ? forward_scene.screen_space_reflections->scene_color :
                    mImpl->inactive_ssr_color.get(),error) &&
                encoder->setFragmentTexture(36,forward_scene.screen_space_reflections ? forward_scene.screen_space_reflections->scene_depth :
                    mImpl->inactive_ssr_depth.get(),error) &&
                encoder->setFragmentTexture(37,forward_scene.reflection_probes ? forward_scene.reflection_probes->irradiance_texture :
                    mImpl->inactive_probe_irradiance.get(),error) &&
                encoder->setFragmentTexture(39,forward_scene.legacy_environment ? forward_scene.legacy_environment :
                    mImpl->inactive_hero_probe.get(),error) &&
                encoder->setFragmentTextureLinear(41,scene.opaque_depth ? scene.opaque_depth : scene.depth,error) &&
                mImpl->setMaterialSampling(*encoder,forward_scene.material_mip_bias,error);
            for (std::uint32_t slot=0; encoded && slot<batch.material_slot_count; ++slot)
                encoded = encoder->setFragmentTexture(slot,batch.diffuse_textures[slot],error) &&
                    encoder->setFragmentTextureLinear(8+slot,normal_map ? batch.materialSlots()[slot].normal_texture : batch.diffuse_textures[0],error) &&
                    encoder->setFragmentTexture(16+slot,specular_map ? batch.materialSlots()[slot].specular_texture : batch.diffuse_textures[0],error);
        }
        return encoded && encoder->drawIndexed(ALMetalPrimitiveType::triangle, batch.index_count,
            batch.index_type, *batch.indices, batch.index_offset, 1, error);
    };
    ALMetalWorldDrawCursor cursor(opaque);
    for (std::size_t i = 0; ok && i < opaque.drawCount(); ++i, cursor.advance())
        ok = draw_motion(cursor.draw(), i < scene.previous_opaque_count ? scene.previous_opaque + i : nullptr, false);
    // Classify background contributions before ordered alpha composition. A
    // later fully covering reliable foreground can replace hidden background;
    // partial transparency preserves its rejection. Water in FRONT of a full
    // foreground is checked again at that foreground's actual eye position.
    reprojection.config[3] |= 4u;
    ok = ok && bind_background() &&
        encoder->setPipeline(*mImpl->temporal_motion_reactive_background, error) &&
        encoder->setDepthStencilState(mImpl->temporal_motion_overlay_depth.get(), error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    if (scene.forward_scene)
        for (std::size_t i = 0; ok && i < scene.forward_scene->batch_count; ++i)
            ok = draw_motion(ALMetalWorldDraw(scene.forward_scene->batches[i]),
                i < scene.previous_forward_count ? scene.previous_forward + i : nullptr, true);
    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(owned, ok && ended, error)) return {};
    transaction.succeeded = true;
    return target;
}
catch (const std::bad_alloc&)
{ setError(error, "Could not allocate Temporal motion inputs"); return {}; }
