/**
 * @file texlayer_preview_native.inl
 * @brief Viewer texture layer. Used for avatars.
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
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

void LLViewerTexLayerSetBuffer::preRenderTexLayerSet()
{
#if LL_DARWIN
    mMetalComposite.reset();
    // Metadata-valid composites can publish directly without old backing.
    // Only bootstrap a destination whose first creation never initialized it.
    if (mGLTexturep.isNull() || mGLTexturep->getDiscardLevel() < 0)
    {
        generateMetalTexture();
    }
    ALMetalTexture* destination = mGLTexturep.notNull()
        ? mGLTexturep->getMetalTexture() : nullptr;
    const ALMetalPixelFormat format = destination
        ? destination->descriptor().format
        : ALMetalPixelFormat::rgba8_unorm_srgb;
    mMetalCaptureActive =
        (!destination || destination->descriptor().mipmap_level_count == 1) &&
        gGL.beginMetalUITextureCapture(
            format,
            &gAlphaMaskProgram, nullptr, nullptr, nullptr,
            LLRender::EMetalUICaptureMode::NATIVE_AUTHORITATIVE,
            LLRender::EMetalUICaptureStorage::LINEAR_STORAGE);
    if (!mMetalCaptureActive)
    {
        LL_DEBUGS("Metal")
            << "Native appearance composite is not ready; update remains pending"
            << LL_ENDL;
        return;
    }

    // The native transaction owns the complete target. These calls update
    // only the logical matrix/viewport/texture state that its encoder reads.
    LLTexLayerSetBuffer::preRenderTexLayerSet();
    mOrigin.set(0, 0);
    gGL.getTextureSlot(0)->unbind();
    gGL.setViewport(mOrigin.mX, mOrigin.mY, mFullWidth, mFullHeight);
#else
    LLTexLayerSetBuffer::preRenderTexLayerSet();
    mOrigin.set(0, 0);
#endif
}

void LLViewerTexLayerSetBuffer::postRenderTexLayerSet(bool success)
{
#if LL_DARWIN
    if (!mMetalCaptureActive)
    {
        mLastNativeRenderSucceeded = false;
        return;
    }

    // Restore the balanced matrix stack while the capture is still native-
    // authoritative, then submit and restore the pre-transaction renderer
    // caches without issuing any corresponding GL state calls.
    LLTexLayerSetBuffer::postRenderTexLayerSet(success);
    std::string failure;
    mMetalComposite = gGL.endMetalUITextureCapture(
        success, false, nullptr, &failure);
    mMetalCaptureActive = false;

    const bool published = success && mMetalComposite && mGLTexturep.notNull() &&
        mGLTexturep->adoptMetalRenderTextureAuthoritative(
            std::move(mMetalComposite), LLGLTexture::DYNAMIC_TEX);
    mLastNativeRenderSucceeded = published;
    if (published)
    {
        const bool update_now = mNeedsUpdate && isReadyToUpdate();
        if (update_now)
        {
            doUpdate();
        }
        LL_INFOS_ONCE("Metal")
            << "Appearance composites publish authoritative native Metal textures"
            << LL_ENDL;
    }
    else
    {
        LL_WARNS("Metal")
            << "Native appearance composite remained pending"
            << (failure.empty() ? std::string() : ": " + failure)
            << LL_ENDL;
    }
    mMetalComposite.reset();
#else
    LLTexLayerSetBuffer::postRenderTexLayerSet(success);
#endif
}

LLViewerDynamicTexture::ETargetlessRenderResult
LLViewerTexLayerSetBuffer::renderWithoutTarget()
{
#if LL_DARWIN
    mLastNativeRenderSucceeded = false;
    preRenderTexLayerSet();
    if (!mMetalCaptureActive)
    {
        return ETargetlessRenderResult::PENDING;
    }

    const bool rendered = renderTexLayerSet(nullptr);
    postRenderTexLayerSet(rendered);
    return mLastNativeRenderSucceeded
        ? ETargetlessRenderResult::COMPLETED
        : ETargetlessRenderResult::PENDING;
#else
    return ETargetlessRenderResult::PENDING;
#endif
}
