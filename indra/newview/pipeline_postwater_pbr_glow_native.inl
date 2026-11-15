/**
 * @file pipeline_postwater_pbr_glow_native.inl
 * @brief Exact post-water glow source flavor and per-view raster markers.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
void LLPipeline::captureMetalWorldPostWaterPBRGlowBatch(
    const LLDrawInfo& params, bool particle_or_hud_particle)
{
    if (!mMetalWorldSimpleCapture ||
        !mMetalWorldSimpleCapture->world_post_deferred_active ||
        !mMetalWorldSimpleCapture->world_post_water_alpha_active ||
        mMetalWorldSimpleCapture->world_post_deferred_phase)
    {
        return;
    }
    MetalWorldSimpleCapture& capture = *mMetalWorldSimpleCapture;
    if (capture.pass_active)
    {
        capture.markIncomplete(__LINE__);
        capture.clearDraws();
        return;
    }
    const std::size_t first_batch = capture.batches.size();
    capture.rigged = params.mAvatar != nullptr;
    capture.alpha_mask = false;
    capture.pbr = true;
    capture.legacy_glow = false;
    capture.pbr_glow = true;
    // This coverage floor is part of the immutable source flavor, distinct
    // from opaque/masked glow. Per-view capture only adds raster markers.
    const bool prior_post_water_glow = capture.post_water_pbr_glow;
    capture.post_water_pbr_glow = true;
    capture.pass_active = true;
    captureMetalWorldPBRBatch(params, false);
    capture.pass_active = false;
    capture.post_water_pbr_glow = prior_post_water_glow;
    capture.pbr = false;
    capture.pbr_glow = false;
    if (capture.complete &&
        capture.batches.size() == first_batch + 1)
    {
        ALMetalWorldSimpleBatch& batch = capture.batches.back();
        batch.post_water_glow = true;
        batch.pre_water_particle = particle_or_hud_particle;
    }
}

