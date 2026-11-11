/**
 * @file dynamic_texture_native.inl
 * @brief Native dynamic image backing creation.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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
// One production definition shared with the device-only publication tests.

void LLViewerDynamicTexture::generateMetalTexture()
{
    if (mComponents < 1 || mComponents > 4)
    {
        LL_ERRS() << "Bad number of components in dynamic texture: "
                  << mComponents << LL_ENDL;
    }

    // Keep the established logical image wrapper: UI sampling and native
    // publication consume its Metal object. The native creation path leaves
    // LLImageGL::mTexName at zero, so a successful targetless render never
    // allocates an OpenGL texture. Native-backed subclasses remain pending if
    // that Metal storage cannot be created; they do not manufacture retained
    // backing as a recovery renderer.
    LLViewerTexture::generateGLTexture();
    LLPointer<LLImageRaw> raw_image =
        new LLImageRaw(mFullWidth, mFullHeight, mComponents);
    raw_image->clear();
    const bool created =
        createMetalTexture(0, raw_image, LLGLTexture::DYNAMIC_TEX);
    llassert(mGLTexturep.notNull() && !mGLTexturep->getHasGLTexture());
    if (!created)
    {
        LL_WARNS("Metal")
            << "Could not create native dynamic-texture backing; native "
               "publication will remain pending"
            << LL_ENDL;
    }
}
