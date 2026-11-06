/**
 * @file alimageslot.h
 * @brief One image unit: the texture level a shader loads from and stores to, tracked like a texture slot.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
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
 * $/LicenseInfo$
 */

#pragma once

#include "stdtypes.h"

class LLImageGL;
class LLRenderTarget;
class ALMetalTexture;

// GL 4.2's minimum GL_MAX_IMAGE_UNITS, so every unit here exists on every context that has images at all.
constexpr U32 AL_NUM_IMAGE_SLOTS = 8;

// One native compute-image binding: a single mip level of a texture, or every
// layer of it, that a shader reads and writes texel by texel. This is the
// writable counterpart of ALTextureSlot's sampled resource binding.
//
// LLRender owns one per index (getImageSlot) and keeps typed Metal resource
// identity plus subresource and access intent. A redundant bind changes
// nothing. Metal compute encoders consume the recorded resource explicitly;
// there is no ambient driver image-unit state to restore.
//
// Shader writes through a unit reach later reads only past the barrier for that kind of read: LLRender::memoryBarrier.
class ALImageSlot
{
    friend class LLRender;
public:
    enum eAccess : U8
    {
        READ_ONLY,
        WRITE_ONLY,
        READ_WRITE,
    };

    // Logical native binding changes, for checking redundant-bind suppression.
    static U32 sImageBinds;

    ALImageSlot(S32 index = -1);

    // The image's format is the texture's own sized internal format; a format image load/store cannot take (sRGB,
    // depth, compressed) warns and binds nothing (returns false). `level` is the mip level the shader addresses.
    bool bind(LLRenderTarget* target, U32 attachment, eAccess access, U32 level = 0);
    bool bind(LLImageGL* image, eAccess access, U32 level = 0);
    // Bind an already-owned native resource. The owner must outlive the use of
    // this slot. `layered` selects every array/cube layer; otherwise `layer`
    // selects one. `format` is the legacy sized-format contract, when known.
    bool bindNative(const ALMetalTexture* texture, U32 level, bool layered, S32 layer,
                    eAccess access, U32 format = 0);
    // Integer texture identities have no native owner and fail closed. Kept
    // temporarily so remaining callers cannot silently fall back to GL.
    bool bindManual(U32 texture, U32 level, bool layered, S32 layer, eAccess access, U32 format);

    // Release the recorded native resource.
    void unbind();

    // Metal has no ambient image-unit state; retained for state-refresh callers.
    void refreshState();

    S32 getIndex() const { return mIndex; }
    U32 getCurrTexture() const { return mCurrTexture; }
    const ALMetalTexture* getCurrMetalTexture() const { return mCurrMetalTexture; }
    U32 getCurrLevel() const { return mCurrLevel; }
    U32 getCurrFormat() const { return mCurrFormat; }
    eAccess getCurrAccess() const { return mCurrAccess; }
    S32 getCurrLayer() const { return mCurrLayer; }
    bool getCurrLayered() const { return mCurrLayered; }

    // A sized internal format image load/store takes: GL's table of image unit formats.
    static bool isImageFormat(U32 format);

private:
    // What an empty native binding holds.
    void reset();

    S32     mIndex;
    // Compatibility truthiness only: 1 means a typed native resource is held;
    // the actual identity is mCurrMetalTexture and is never handed to GL.
    U32     mCurrTexture = 0;
    const ALMetalTexture* mCurrMetalTexture = nullptr;
    U32     mCurrLevel   = 0;
    S32     mCurrLayer   = 0;
    U32     mCurrFormat;
    eAccess mCurrAccess  = READ_ONLY;
    bool    mCurrLayered = false;
};
