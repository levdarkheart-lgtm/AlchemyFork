/**
 * @file image_preview_native.inl
 * @brief Targetless image-preview publication transactions.
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2026, Alchemy Viewer Project
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
// Included by the production caller and its device-only transaction tests.
// Camera/scene construction stays with the caller; native image publication
// and retry state are defined here once.

LLViewerDynamicTexture::ETargetlessRenderResult
LLImagePreviewAvatar::renderWithoutTarget()
{
#if LL_DARWIN
    if (mGLTexturep.isNull() || !mGLTexturep->getMetalTexture())
    {
        // Initial native allocation can fail before a device is available.
        // Retry the missing destination, as the original postRender did.
        generateMetalTexture();
        if (mGLTexturep.isNull() || !mGLTexturep->getMetalTexture())
        {
            return ETargetlessRenderResult::PENDING;
        }
    }

    bool rendered = false;
    {
        LLScopedImagePreviewCameraState saved_state;
        mOrigin.set(0, 0);
        rendered = render();
    }

    const bool published = rendered && mMetalPreview &&
        mGLTexturep->adoptMetalRenderTextureAuthoritative(
            std::move(mMetalPreview), LLGLTexture::DYNAMIC_TEX);
    mMetalPreview.reset();
    mMetalPreviewDescriptor.reset();
    if (!published)
    {
        mNeedsUpdate = true;
        LL_DEBUGS("Metal")
            << "Avatar image preview remains pending; its prior native "
               "result is unchanged"
            << LL_ENDL;
        return ETargetlessRenderResult::PENDING;
    }

    mNeedsUpdate = false;
    LL_INFOS_ONCE("Metal")
        << "Avatar image previews are targetless native Metal producers"
        << LL_ENDL;
    return ETargetlessRenderResult::COMPLETED;
#else
    return ETargetlessRenderResult::PENDING;
#endif
}

LLViewerDynamicTexture::ETargetlessRenderResult
LLImagePreviewSculpted::renderWithoutTarget()
{
#if LL_DARWIN
    if (mGLTexturep.isNull() || !mGLTexturep->getMetalTexture())
    {
        generateMetalTexture();
        if (mGLTexturep.isNull() || !mGLTexturep->getMetalTexture())
        {
            return ETargetlessRenderResult::PENDING;
        }
    }

    bool rendered = false;
    {
        LLScopedImagePreviewCameraState saved_state;
        mOrigin.set(0, 0);
        rendered = render();
    }

    const bool published = rendered && mMetalPreview &&
        mGLTexturep->adoptMetalRenderTextureAuthoritative(
            std::move(mMetalPreview), LLGLTexture::DYNAMIC_TEX);
    mMetalPreview.reset();
    mMetalPreviewDescriptor.reset();
    if (!published)
    {
        mNeedsUpdate = true;
        LL_DEBUGS("Metal")
            << "Sculpt image preview remains pending; its prior native "
               "result is unchanged"
            << LL_ENDL;
        return ETargetlessRenderResult::PENDING;
    }

    mNeedsUpdate = false;
    LL_INFOS_ONCE("Metal")
        << "Sculpt image previews are targetless native Metal producers"
        << LL_ENDL;
    return ETargetlessRenderResult::COMPLETED;
#else
    return ETargetlessRenderResult::PENDING;
#endif
}
