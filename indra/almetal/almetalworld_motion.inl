/** Native Temporal motion replay. Included after the renderer's existing private implementation. */
namespace
{
struct alignas(16) WorldTemporalMotionConstants
{
    float current_mvp[16]{}, previous_mvp[16]{}, texture_matrix[16]{};
    float current_wind[4]{}, current_sin[4]{}, current_gravity[4]{}, current_cloth_depth[4]{};
    float previous_wind[4]{}, previous_sin[4]{}, previous_gravity[4]{}, previous_cloth_depth[4]{};
    float extent_bias[4]{}, raster[4]{};
    std::uint32_t config[4]{}, coverage[4]{};
    float thresholds[8][4]{};
    float impostor_center[4]{}, impostor_left[4]{}, impostor_up[4]{};
};
struct alignas(16) WorldTemporalReprojectionConstants
{
    float current_to_previous[16]{}, inverse_projection[16]{}, eye_water_plane[4]{};
    float extent_jitter[4]{};
    std::uint32_t config[4]{};
};
}

bool ALMetalWorldGBufferRenderer::prepareTemporalMotion(std::string* error)
{
    if (!mImpl || !mImpl->device || !mImpl->library)
    { setError(error, "Temporal motion renderer is unavailable"); return false; }
    if (mImpl->temporal_motion_background && mImpl->temporal_motion_geometry[0] &&
        mImpl->temporal_motion_geometry[1] && mImpl->temporal_motion_reactive_background &&
        mImpl->temporal_motion_impostor) return true;
    ALMetalRenderPipelineDescriptor descriptor;
    descriptor.library = mImpl->library.get();
    descriptor.color_attachment_count = 3;
    descriptor.colors[0].format = ALMetalPixelFormat::rg16_float;
    descriptor.colors[1].format = ALMetalPixelFormat::r8_unorm;
    descriptor.colors[2].format = ALMetalPixelFormat::r32_float;
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
    descriptor.colors[0].write_mask = ALMetalColorWriteMask::none;
    descriptor.colors[2].write_mask = ALMetalColorWriteMask::none;
    descriptor.colors[1].blending_enabled = true;
    descriptor.colors[1].source_rgb = ALMetalBlendFactor::one;
    descriptor.colors[1].destination_rgb = ALMetalBlendFactor::one;
    descriptor.colors[1].rgb_operation = ALMetalBlendOperation::maximum;
    descriptor.label = "Alchemy Temporal transparent history rejection";
    geometry[1] = ALMetalRenderPipeline::create(*mImpl->device, descriptor, error);
    if (!geometry[1]) return false;
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
    mImpl->temporal_motion_impostor = std::move(impostor);
    mImpl->temporal_motion_reactive_background = std::move(reactive_background);
    mImpl->temporal_motion_background = std::move(background);
    mImpl->temporal_motion_geometry = std::move(geometry);
    return true;
}

std::unique_ptr<ALMetalRenderTarget> ALMetalWorldGBufferRenderer::renderTemporalMotion(
    const ALMetalWorldTemporalMotionScene& scene, std::string* error)
try
{
    Impl::GBufferFailureScope transaction(mImpl.get(), error);
    if (!scene.width || !scene.height || !scene.depth || !scene.opaque_scene ||
        scene.depth->descriptor().width != scene.width || scene.depth->descriptor().height != scene.height ||
        !finiteValues(scene.clip_to_previous_clip, 16) || !finiteValues(scene.jitter_pixels, 2) ||
        (scene.previous_opaque_count && !scene.previous_opaque) ||
        (scene.previous_forward_count && !scene.previous_forward) ||
        !prepareTemporalMotion(error))
    { setError(error, "Temporal motion inputs are incomplete or incompatible"); return {}; }
    const auto& opaque = *scene.opaque_scene;
    if (!alMetalValidateWorldView(opaque, error)) return {};
    ALMetalRenderTargetDescriptor desc;
    desc.width = scene.width; desc.height = scene.height;
    desc.color_attachment_count = 3;
    desc.colors[0].format = ALMetalPixelFormat::rg16_float;
    desc.colors[0].label = "Alchemy Temporal current-to-previous input pixels";
    desc.colors[1].format = ALMetalPixelFormat::r8_unorm;
    desc.colors[1].label = "Alchemy Temporal reactive rejection";
    desc.colors[2].format = ALMetalPixelFormat::r32_float;
    desc.colors[2].label = "Alchemy Temporal raw device depth";
    desc.label = "Alchemy Temporal motion/depth/rejection inputs";
    auto target = mImpl->makeRenderTarget(*mImpl->device, desc, error);
    if (!target) return {};
    ALMetalRenderPassConfig config;
    for (unsigned i = 0; i != 3; ++i)
    { config.colors[i].load = ALMetalLoadAction::clear; config.colors[i].store = ALMetalStoreAction::store; }
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
    bool ok = encoder->setViewport({0, 0, double(scene.width), double(scene.height), 0, 1}, error) &&
        encoder->setScissor({0, 0, scene.width, scene.height}, error) &&
        encoder->setFrontFacingWinding(ALMetalWinding::clockwise, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        encoder->setPipeline(*mImpl->temporal_motion_background, error) &&
        mImpl->setFragmentData(*encoder, *command, 0, &reprojection, sizeof(reprojection), error) &&
        encoder->setFragmentTextureLinear(32, scene.depth, error) &&
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
                encoder->setCullMode(ALMetalCullMode::none, error) &&
                mImpl->setVertexData(*encoder, *command, 0, &constants, sizeof(constants), error) &&
                encoder->setFragmentTextureLinear(0, batch.impostor_textures[0], error) &&
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
        WorldTemporalMotionConstants constants;
        std::memcpy(constants.current_mvp, draw.modelviewProjection(), sizeof(constants.current_mvp));
        std::memcpy(constants.previous_mvp, valid ? previous->modelview_projection : draw.modelviewProjection(), sizeof(constants.previous_mvp));
        std::memcpy(constants.texture_matrix, draw.textureMatrix(), sizeof(constants.texture_matrix));
        std::memcpy(constants.current_wind, batch.avatar_wind, sizeof(constants.current_wind));
        std::memcpy(constants.current_sin, batch.avatar_sin_wave, sizeof(constants.current_sin));
        std::memcpy(constants.current_gravity, batch.avatar_gravity, sizeof(constants.current_gravity));
        std::memcpy(constants.current_cloth_depth, batch.avatar_cloth_depth, sizeof(constants.current_cloth_depth));
        std::memcpy(constants.previous_wind, valid ? previous->avatar_wind : batch.avatar_wind, sizeof(constants.previous_wind));
        std::memcpy(constants.previous_sin, valid ? previous->avatar_sin_wave : batch.avatar_sin_wave, sizeof(constants.previous_sin));
        std::memcpy(constants.previous_gravity, valid ? previous->avatar_gravity : batch.avatar_gravity, sizeof(constants.previous_gravity));
        std::memcpy(constants.previous_cloth_depth, valid ? previous->avatar_cloth_depth : batch.avatar_cloth_depth, sizeof(constants.previous_cloth_depth));
        constants.extent_bias[0] = float(scene.width); constants.extent_bias[1] = float(scene.height);
        constants.extent_bias[2] = opaque.material_mip_bias;
        constants.extent_bias[3] = valid && !previous->reactive ? 0.f : 1.f;
        constants.raster[0] = scene.jitter_pixels[0]; constants.raster[1] = scene.jitter_pixels[1];
        constants.raster[2] = forward ? batch.pre_water_minimum_alpha : batch.alpha_cutoff;
        constants.config[0] = attachment ? 1 : body ? (batch.avatar_cloth ? 3 : 2) : 0;
        constants.config[1] = draw.skinJointCount(); constants.config[2] = valid ? previous->skin_joint_count : draw.skinJointCount();
        constants.config[3] = scene.reverse_z;
        const unsigned family = batch.pbr_slot_count ? 2 : batch.material_slot_count ? 1 : 0;
        constants.coverage[0] = batch.texture_count; constants.coverage[1] = family;
        constants.coverage[2] = batch.material_alpha_mode;
        constants.coverage[3] = (batch.alpha_mask ? 1u : 0u) | (batch.use_vertex_color ? 2u : 0u) |
            (batch.alpha_cutoff_bias ? 4u : 0u) | (forward ? 8u : 0u) | ((batch.avatar_skinning || batch.avatar_rigid) ? 16u : 0u);
        const bool animated_uv = valid && std::memcmp(previous->texture_matrix, draw.textureMatrix(), sizeof(previous->texture_matrix)) != 0;
        if (forward || animated_uv) constants.extent_bias[3] = 1.f;
        for (unsigned i = 0; i < 8; ++i)
        {
            constants.thresholds[i][0] = batch.alpha_cutoff;
            constants.thresholds[i][1] = 1.f;
            if (family == 1 && i < batch.material_slot_count) constants.thresholds[i][0] = batch.materialSlots()[i].alpha_cutoff;
            if (family == 2 && i < batch.pbr_slot_count)
            { constants.thresholds[i][0] = batch.pbrSlots()[i].alpha_cutoff;
              constants.thresholds[i][1] = batch.pbrSlots()[i].base_color_factor[3]; }
        }
        bool encoded = encoder->setPipeline(*mImpl->temporal_motion_geometry[forward ? 1 : 0], error) &&
            encoder->setCullMode((batch.double_sided || forward) ? ALMetalCullMode::none :
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
            mImpl->setVertexAndFragmentData(*encoder, *command, 4, 1, &constants, sizeof(constants), error) &&
            encoder->setFragmentTextureLinear(32, scene.depth, error) &&
            encoder->setFragmentSampler(0, mImpl->diffuse_sampler.get(), error);
        WorldPBRConstants pbr{};
        if (family == 2) pbr = buildWorldPBRConstants(opaque, batch);
        encoded = encoded && mImpl->setVertexData(*encoder, *command, 11, &pbr, sizeof(pbr), error);
        for (unsigned t = 0; encoded && t < 32; ++t)
        {
            const auto* texture = t < batch.texture_count ? batch.diffuse_textures[t] : batch.diffuse_textures[0];
            encoded = encoder->setFragmentTexture(t, texture, error);
        }
        return encoded && encoder->drawIndexed(ALMetalPrimitiveType::triangle, batch.index_count,
            batch.index_type, *batch.indices, batch.index_offset, 1, error);
    };
    ALMetalWorldDrawCursor cursor(opaque);
    for (std::size_t i = 0; ok && i < opaque.drawCount(); ++i, cursor.advance())
        ok = draw_motion(cursor.draw(), i < scene.previous_opaque_count ? scene.previous_opaque + i : nullptr, false);
    if (scene.forward_scene)
        for (std::size_t i = 0; ok && i < scene.forward_scene->batch_count; ++i)
            ok = draw_motion(ALMetalWorldDraw(scene.forward_scene->batches[i]),
                i < scene.previous_forward_count ? scene.previous_forward + i : nullptr, true);
    // Opaque replays may overwrite the initial mask. Union sky/water/reset rejection after all surfaces.
    ok = ok && encoder->setPipeline(*mImpl->temporal_motion_reactive_background, error) &&
        encoder->setCullMode(ALMetalCullMode::none, error) &&
        mImpl->setFragmentData(*encoder, *command, 0, &reprojection, sizeof(reprojection), error) &&
        encoder->setFragmentTextureLinear(32, scene.depth, error) &&
        encoder->draw(ALMetalPrimitiveType::triangle, 0, 3, 1, error);
    const bool ended = encoder->end(error);
    if (!mImpl->finishCommandBuffer(owned, ok && ended, error)) return {};
    transaction.succeeded = true;
    return target;
}
catch (const std::bad_alloc&)
{ setError(error, "Could not allocate Temporal motion inputs"); return {}; }
