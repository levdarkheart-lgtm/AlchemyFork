/**
 * @file pipeline_impostor_readiness_native.inl
 * @brief Actual native impostor capture and per-avatar readiness boundary.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright © 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

void LLPipeline::captureMetalWorldImpostorBatch(
    LLVOAvatar& avatar, const LLVector3& center,
    const LLVector3& left, const LLVector3& up,
    const LLMatrix3a& normal_rotation, F32 debug_outline_width)
{
    if (!mMetalWorldSimpleCapture ||
        !mMetalWorldSimpleCapture->frame_active ||
        !mMetalWorldSimpleCapture->pass_active ||
        !mMetalWorldSimpleCapture->impostor)
    {
        return;
    }
    MetalWorldSimpleCapture& capture = *mMetalWorldSimpleCapture;
    const bool debug_requested =
        hasRenderDebugMask(RENDER_DEBUG_IMPOSTORS);
    const bool debug_outline = debug_outline_width > 0.f;
    bool debug_state_valid = !debug_requested && !debug_outline &&
        debug_outline_width == 0.f;
    if (debug_requested && debug_outline)
    {
        LLRender::eBlendFactor color_source;
        LLRender::eBlendFactor color_destination;
        LLRender::eBlendFactor alpha_source;
        LLRender::eBlendFactor alpha_destination;
        gGL.getBlendFunc(color_source, color_destination,
                         alpha_source, alpha_destination);
        U32 rgb_equation = 0;
        U32 alpha_equation = 0;
        gGL.getBlendEquation(rgb_equation, alpha_equation);
        bool color_mask[4]{};
        gGL.getColorMask(color_mask);
        debug_state_valid =
            std::isfinite(debug_outline_width) &&
            debug_outline_width >= 1.f && debug_outline_width <= 5.f &&
            gGL.getLineWidth() == debug_outline_width &&
            LLGLState::isEnabled(GL_BLEND) &&
            LLGLState::isEnabled(GL_CULL_FACE) &&
            LLGLDepthTest::depthEnabled() &&
            LLGLDepthTest::depthWriteEnabled() &&
            LLGLDepthTest::depthFunction() == GL_LEQUAL &&
            color_source == LLRender::BF_ONE &&
            color_destination == LLRender::BF_ONE &&
            alpha_source == LLRender::BF_ONE &&
            alpha_destination == LLRender::BF_ONE &&
            rgb_equation == GL_FUNC_ADD &&
            alpha_equation == GL_FUNC_ADD &&
            color_mask[0] && color_mask[1] &&
            color_mask[2] && color_mask[3];
    }
    if (!capture.complete || !debug_state_valid)
    {
        capture.markIncomplete(__LINE__);
        capture.batches.clear();
        capture.avatar_palettes.clear();
        return;
    }
    ALMetalAvatarResources::Key key{};
    std::copy_n(avatar.getID().mData, key.size(), key.begin());
    auto textures = capture.avatar_resources->find(
        key, *capture.device, capture.hdr, capture.normal_hdr);
    const bool has_textures = static_cast<bool>(textures);
    if (!has_textures)
    {
        // Original renderImpostor skips only this avatar while its backing
        // is unavailable. A logical target is not a native publication receipt;
        // keep the other accepted batches and current-camera world advancing.
        // The normal scheduler retries the complete quartet, and its failed
        // bakes remain diagnosed. Existing valid publications are not replaced
        // until a new bake succeeds; no synthetic texture is substituted here.
        avatar.mNeedsImpostorUpdate = true;
        return;
    }

    const LLMatrix4a& modelview =
        LLViewerCamera::getCurrent().getModelview();
    LLMatrix4a modelview_projection;
    modelview_projection.setMul(
        modelview, LLViewerCamera::getCurrent().getProjection());

    ALMetalWorldSimpleBatch batch;
    batch.impostor = true;
    batch.impostor_texture_owner = textures;
    batch.impostor_debug_outline = debug_outline;
    batch.impostor_debug_line_width = debug_outline_width;
    for (std::size_t texture = 0;
         texture < batch.impostor_textures.size(); ++texture)
    {
        batch.impostor_textures[texture] =
            textures->textures[texture].get();
    }
    std::memcpy(batch.modelview_projection,
                modelview_projection.getF32ptr(),
                sizeof(batch.modelview_projection));
    std::memcpy(batch.modelview, modelview.getF32ptr(),
                sizeof(batch.modelview));
    std::memcpy(batch.impostor_center, center.mV, sizeof(center.mV));
    std::memcpy(batch.impostor_left, left.mV, sizeof(left.mV));
    std::memcpy(batch.impostor_up, up.mV, sizeof(up.mV));
    for (U32 row = 0; row < 3; ++row)
    {
        std::memcpy(batch.impostor_normal_rotation[row],
                    normal_rotation.getRow(row).getF32ptr(),
                    sizeof(batch.impostor_normal_rotation[row]));
    }
    try
    {
        capture.batches.push_back(batch);
    }
    catch (const std::bad_alloc&)
    {
        capture.markIncomplete(__LINE__);
        capture.batches.clear();
        capture.avatar_palettes.clear();
    }
}
