/**
 * @file gltf_preview_native.inl
 *
 * $LicenseInfo:firstyear=2023&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2023, Linden Research, Inc.
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
// Shared production method bodies; viewer-only scene inputs remain in their owners.

LLViewerDynamicTexture::ETargetlessRenderResult
LLGLTFPreviewTexture::renderWithoutTarget()
{
#if LL_DARWIN
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;

    llassert(mShouldRender);
    if (!mShouldRender)
    {
        return ETargetlessRenderResult::PENDING;
    }

    // Apple Silicon previews have one rendering owner. Readiness and actual
    // failures both stay targetless and retry Metal.
    (void)ensureMetalTexture();
    preRender();
    const bool rendered = render();

    const bool published = rendered && mMetalPreview && publishMetalPreview();
    if (!published)
    {
        mMetalPreview.reset();
        // A completed native render whose logical texture refuses adoption is
        // a publication failure. Keep it observable and retry natively.
        if (mNativeAttemptResult == ENativeAttemptResult::READY)
        {
            mNativeAttemptResult = ENativeAttemptResult::FAILED;
        }
        mShouldRender = true;
        return ETargetlessRenderResult::PENDING;
    }

    mShouldRender = false;
    mBestLoad = mPendingLoad;
    return ETargetlessRenderResult::COMPLETED;
#else
    return ETargetlessRenderResult::PENDING;
#endif
}
