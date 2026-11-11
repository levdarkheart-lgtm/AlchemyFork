/**
 * @file appearance_hint_native.inl
 * @brief A tool to manipulate faces..
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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
// Shared production method bodies; viewer-only scene inputs remain in their owners.

LLViewerDynamicTexture::ETargetlessRenderResult
LLVisualParamHint::renderWithoutTarget()
{
#if LL_DARWIN
    if (!ensureMetalTexture())
    {
        return ETargetlessRenderResult::PENDING;
    }

    LLViewerCamera* camera = LLViewerCamera::getInstance();
    const LLCamera saved_camera = *camera;
    const LLCamera saved_current = LLViewerCamera::getCurrent();
    const std::array<S32, 4> saved_viewport{{
        gGLViewport[0], gGLViewport[1], gGLViewport[2], gGLViewport[3],
    }};

    preRender(false);
    const bool rendered = render();

    camera->setOrigin(saved_camera);
    camera->setAxes(saved_camera);
    camera->setAspect(saved_camera.getAspect());
    camera->setViewNoBroadcast(saved_camera.getView());
    camera->setNear(saved_camera.getNear());
    camera->setFar(saved_camera.getFar());
    camera->setModelview(saved_camera.getModelview());
    camera->setProjection(saved_camera.getProjection());
    LLViewerCamera::updateFrustumPlanes(*camera);
    LLViewerCamera::setCurrent(saved_current);
    std::copy(saved_viewport.begin(), saved_viewport.end(), gGLViewport);

    const bool published = rendered && mMetalPreview &&
        mGLTexturep->adoptMetalRenderTextureAuthoritative(
            std::move(mMetalPreview), LLGLTexture::DYNAMIC_TEX);
    mMetalPreview.reset();
    mMetalPreviewDescriptor.reset();
    if (!published)
    {
        mNeedsUpdate = true;
        LL_DEBUGS("Metal")
            << "Visual-parameter hint remains pending; prior native result "
               "is unchanged"
            << LL_ENDL;
        return ETargetlessRenderResult::PENDING;
    }

    mNeedsUpdate = false;
    mIsVisible = true;
    LL_INFOS_ONCE("Metal")
        << "Visual-parameter hints are targetless native Metal producers"
        << LL_ENDL;
    return ETargetlessRenderResult::COMPLETED;
#else
    return ETargetlessRenderResult::PENDING;
#endif
}

LLViewerDynamicTexture::ETargetlessRenderResult
LLVisualParamReset::renderWithoutTarget()
{
#if LL_DARWIN
    render();
    return ETargetlessRenderResult::COMPLETED;
#else
    return ETargetlessRenderResult::PENDING;
#endif
}

bool LLVisualParamReset::render()
{
    if (sDirty)
    {
        gAgentAvatarp->updateComposites();
        gAgentAvatarp->updateVisualParams();
        gAgentAvatarp->updateGeometry(gAgentAvatarp->mDrawable);
        sDirty = false;
    }

    return false;
}
