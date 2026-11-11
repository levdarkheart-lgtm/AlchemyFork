/**
 * @file altextureslot.cpp
 * @brief ALTextureSlot implementation
 *
 * Split out of llrender.cpp; see altextureslot.h for what this models.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
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
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "altextureslot.h"

#include "llcubemap.h"
#include "llimagegl.h"
#include "llrender.h"
#include "llrendertarget.h"
#include "lltexture.h"
#if LL_DARWIN
#include "almetalresource.h"
#endif

static const GLenum sGLTextureType[] =
{
    GL_TEXTURE_2D,
    GL_TEXTURE_RECTANGLE,
    GL_TEXTURE_CUBE_MAP,
    GL_TEXTURE_CUBE_MAP_ARRAY,
    GL_TEXTURE_2D_MULTISAMPLE,
    GL_TEXTURE_3D
};

namespace
{
#if LL_DARWIN
    // The virtual viewer fallback can bind another LLTexture. Keep its existing
    // policy, but reject a missing-default cycle without allocating a side cache.
    struct NativeUIFallback
    {
        const LLTexture* texture;
        const NativeUIFallback* previous;
        static thread_local const NativeUIFallback* current;
        explicit NativeUIFallback(const LLTexture* value)
            : texture(value), previous(current) { current = this; }
        ~NativeUIFallback() { current = previous; }
        static bool contains(const LLTexture* value)
        {
            for (auto* item = current; item; item = item->previous)
                if (item->texture == value) return true;
            return false;
        }
    };
    thread_local const NativeUIFallback* NativeUIFallback::current = nullptr;

    bool bind_native_ui_sampled(ALTextureSlot& slot, LLTexture* texture,
                                LLImageGL* image, ALSampler key,
                                bool fast)
    {
        // Preserve the capture's unsupported-slot guard: no other unit may
        // overwrite slot zero or silently make an incomplete capture publish.
        if (slot.getIndex() != 0 || !texture)
            return gGL.bindMetalUITextureCapture(slot.getIndex(), image, key);

        if (!image || !image->getMetalTexture())
        {
            // A fetched texture tests DELETED in forceImmediateUpdate();
            // setActive() can clear that state, so request revival first.
            texture->forceImmediateUpdate();
            image = texture->getGLTexture();
        }
        texture->setActive();
        if (!image || !image->getMetalTexture())
        {
            if (image) image->forceUpdateBindStats();
            if (NativeUIFallback::contains(texture))
            {
                gGL.invalidateMetalUITextureCapture(
                    "native UI default texture fallback was cyclic");
                return false;
            }
            // Flush the preceding geometry before clearing its binding.
            // A virtual fallback returning true without binding anything
            // must not accidentally reuse that preceding ready image.
            gGL.unbindMetalUITextureCapture(slot.getIndex());
            NativeUIFallback fallback(texture);
            const bool complete = texture->bindDefaultImage(slot.getIndex());
            image = slot.getCurrImage();
            if (!complete || !image || !image->getMetalTexture())
            {
                gGL.invalidateMetalUITextureCapture(
                    "native UI default texture fallback was not ready");
                return false;
            }
            // Original bindFast reapplies its caller's sampler after the
            // default bind; bindImpl returns the default binder's sampler.
            return !fast || gGL.bindMetalUITextureCapture(
                slot.getIndex(), image, key);
        }
        image->forceUpdateBindStats();
        return gGL.bindMetalUITextureCapture(slot.getIndex(), image, key);
    }

    bool matches_native_type(const ALMetalTexture& texture,
                             ALTextureSlot::eTextureType type) noexcept
    {
        switch (type)
        {
            case ALTextureSlot::TT_TEXTURE:
            case ALTextureSlot::TT_RECT_TEXTURE:
                return texture.descriptor().type == ALMetalTextureType::texture_2d;
            case ALTextureSlot::TT_CUBE_MAP:
                return texture.descriptor().type == ALMetalTextureType::texture_cube;
            case ALTextureSlot::TT_CUBE_MAP_ARRAY:
                return texture.descriptor().type == ALMetalTextureType::texture_cube_array;
            case ALTextureSlot::TT_MULTISAMPLE_TEXTURE:
                return texture.descriptor().type == ALMetalTextureType::texture_2d_multisample;
            case ALTextureSlot::TT_TEXTURE_3D:
            case ALTextureSlot::TT_NONE:
                return false;
        }
        return false;
    }
#endif
}

thread_local U32 ALTextureSlot::sSamplerBinds  = 0;
thread_local U32 ALTextureSlot::sSamplerSkips  = 0;
thread_local U32 ALTextureSlot::sTextureBinds  = 0;
thread_local U32 ALTextureSlot::sSamplerBindsFlushed = 0;
thread_local U32 ALTextureSlot::sSamplerBindsSplitBatch = 0;

ALTextureSlot::ALTextureSlot(S32 index)
    : mCurrTexType(TT_NONE),
    mCurrTexture(0),
    mIndex(index)
{
    llassert_always(index < (S32)AL_NUM_TEXTURE_SLOTS);
}

// static
void ALTextureSlot::setWhiteImage(LLImageGL* image)
{
    sWhiteImage = image;
    // A native texture deliberately has no GL name. Keep a non-zero logical token for
    // the few retained manual-white call sites so zero never aliases a missing resource.
    sWhiteTexture = image ? 1u : 0u;
}

ALMetalTexture* ALTextureSlot::getCurrMetalTexture() const
{
#if LL_DARWIN
    return mCurrImage ? mCurrImage->getMetalTexture() : mCurrNativeTexture;
#else
    return nullptr;
#endif
}

//static
U32 ALTextureSlot::getInternalType(eTextureType type)
{
    return sGLTextureType[type];
}

void ALTextureSlot::bindSampler(U32 sampler)
{
    if (mIndex < 0)
    {
        return;
    }

    if (mCurrSampler == sampler)
    {
        ++sSamplerSkips;
        return;
    }

    // Zone only on the ISSUED path (a couple thousand per frame, not tens of thousands of
    // skips), so a Tracy capture attributes the standing sampler-bind total by enclosing
    // zone -- which pass, which pool, which bind chain -- without per-suspect counters.
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LL_PROFILE_ZONE_NUM(mIndex);

    ++sSamplerBinds;
    ++sSamplerBindsFlushed;
    if (gGL.mCount > 0)
    {
        // The flush below will emit a draw: this sampler change genuinely split a batch.
        ++sSamplerBindsSplitBatch;
    }

    // Sampler state is consulted at sample time, not at bind time, so a change has to be
    // ordered against draws the same way a texture change is.
    gGL.flush();

    mCurrSampler = sampler;
}

void ALTextureSlot::bindNativeSampler(U32 sampler)
{
    if (mCurrSampler == sampler)
    {
        ++sSamplerSkips;
        return;
    }
    ++sSamplerBinds;
    mCurrSampler = sampler;
}

// Sampler named by the CALLER: the bind site says how it wants to read, and the texture
// supplies only its data.
void ALTextureSlot::bindFast(LLTexture* texture, ALSampler key)
{
    // getGLTexture() is virtual and this is the per-draw path, so it is resolved once and the
    // sampler is chosen from the result rather than re-fetching.
    LLImageGL* gl_tex = texture ? texture->getGLTexture() : nullptr;
#if LL_DARWIN
    if (gGL.isMetalUITextureCaptureAuthoritative())
    {
        bind_native_ui_sampled(*this, texture, gl_tex, key, true);
        return;
    }
#endif
    if (mIndex < 0 || !texture || !gl_tex) return;
    bindFastImpl(texture, gl_tex, gGL.getSampler(key));
}

void ALTextureSlot::bindFastImpl(LLTexture* texture, LLImageGL* gl_tex, U32 sampler)
{
    llassert(gl_tex);
    ++sTextureBinds;
    texture->setActive();
#if LL_DARWIN
    if (!gl_tex->getMetalTexture())
    {
        LL_PROFILE_ZONE_NAMED("MISSING NATIVE TEXTURE");
        texture->forceImmediateUpdate();
        if (!gl_tex->getMetalTexture())
        {
            gl_tex->forceUpdateBindStats();
            texture->bindDefaultImage(mIndex);
            return;
        }
    }
    mCurrTexture = 1u;
    mCurrImage = gl_tex;
    mCurrNativeTexture = nullptr;
    mCurrTexType = gl_tex->getTarget();
    bindNativeSampler(sampler);
    gl_tex->forceUpdateBindStats();
#else
    (void)sampler;
    texture->bindDefaultImage(mIndex);
#endif
}

// The sampler is named by the caller and the texture's own mode is not consulted at all --
// it has none to consult.
bool ALTextureSlot::bindSampled(LLTexture* texture, ALSampler key, bool forceBind)
{
    return bindImpl(texture, forceBind, key);
}

// Same for a raw LLImageGL -- the font atlas is one, since glyph pages are not LLTextures.
bool ALTextureSlot::bindSampled(LLImageGL* texture, ALSampler key, bool forceBind)
{
#if LL_DARWIN
    if (gGL.isMetalUITextureCaptureAuthoritative())
    {
        (void)forceBind;
        if (mIndex != 0 || !texture)
            return gGL.bindMetalUITextureCapture(mIndex, texture, key);
        if (!texture->getMetalTexture())
        {
            // Original raw-image bind substitutes the global ready default.
            // Check readiness and identity before delegation to prevent a
            // missing default from recursively substituting itself.
            LLImageGL* fallback = LLImageGL::sDefaultGLTexture;
            if (!fallback || fallback == texture || !fallback->getMetalTexture())
            {
                gGL.invalidateMetalUITextureCapture(
                    "native UI raw default texture was not ready");
                return false;
            }
            texture = fallback;
        }
        texture->forceUpdateBindStats();
        return gGL.bindMetalUITextureCapture(mIndex, texture, key);
    }
    if (texture && texture->getMetalTexture())
    {
        return bindNativeSampled(texture, gGL.getSampler(key), forceBind);
    }
#endif
    if (!bind(texture, false, forceBind))
    {
        return false;
    }
    bindSampler(gGL.getSampler(key));
    return true;
}

// The sampler is selected ONCE here, from the caller's mask. Selecting it separately could
// flush between two changes and split a draw batch for a sampler about to change again.
bool ALTextureSlot::bindImpl(LLTexture* texture, bool forceBind, ALSampler key)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
#if LL_DARWIN
    if (gGL.isMetalUITextureCaptureAuthoritative())
    {
        (void)forceBind;
        return bind_native_ui_sampled(
            *this, texture, texture ? texture->getGLTexture() : nullptr,
            key, false);
    }
#endif
    if (mIndex < 0 || !texture)
    {
        LL_DEBUGS() << "NULL or invalid ALTextureSlot::bind texture" << LL_ENDL;
        return false;
    }

    LLImageGL* image = texture->getGLTexture();
#if LL_DARWIN
    if (!image || !image->getMetalTexture())
    {
        texture->forceImmediateUpdate();
        image = texture->getGLTexture();
        if (!image || !image->getMetalTexture())
        {
            if (image)
            {
                image->forceUpdateBindStats();
            }
            return texture->bindDefaultImage(mIndex);
        }
    }
    texture->setActive();
    return bindNativeSampled(image, gGL.getSampler(key), forceBind);
#else
    (void)image;
    (void)forceBind;
    (void)key;
    return false;
#endif
}

#if LL_DARWIN
bool ALTextureSlot::bindNativeSampled(LLImageGL* image, U32 sampler, bool forceBind)
{
    if (mIndex < 0 || !image || !image->getMetalTexture() ||
        image->getTarget() != TT_TEXTURE)
    {
        return false;
    }

    const bool changed = mCurrImage != image ||
                         mCurrTexType != TT_TEXTURE ||
                         mCurrSampler != sampler || forceBind;
    if (changed)
    {
        gGL.flush();
    }
    mCurrTexture = 1u;
    mCurrTexType = TT_TEXTURE;
    mCurrImage = image;
    mCurrNativeTexture = nullptr;
    bindNativeSampler(sampler);
    image->forceUpdateBindStats();
    return true;
}
#endif

bool ALTextureSlot::bind(LLImageGL* texture, bool for_rendering, bool forceBind, S32 usename)
{
    (void)for_rendering;
    if (mIndex < 0) return false;

    if (!texture)
    {
        LL_DEBUGS() << "NULL ALTextureSlot::bind texture" << LL_ENDL;
        return false;
    }

#if LL_DARWIN
    // An explicit integer name is legacy bind-to-edit state with no native
    // resource identity. It cannot be represented by a Metal texture slot.
    if (usename || !texture->getMetalTexture())
    {
        return false;
    }
    if (mCurrImage != texture || mCurrTexType != texture->getTarget() ||
        forceBind)
    {
        gGL.flush();
    }
    mCurrTexture = 1u;
    mCurrTexType = texture->getTarget();
    mCurrImage = texture;
    mCurrNativeTexture = nullptr;
    texture->updateBindStats();
    return true;
#else
    (void)forceBind;
    (void)usename;
    return false;
#endif
}

bool ALTextureSlot::bindNative(ALMetalTexture* texture, eTextureType type,
                               ALSampler key, bool forceBind)
{
#if LL_DARWIN
    if (gGL.mBufferDataList)
    {
        gGL.invalidateMetalUITextureCapture(
            "raw native texture cannot be retained by an image UI display list");
        return false;
    }
    if (gGL.isMetalUITextureCaptureAuthoritative())
    {
        return gGL.bindMetalUITextureCaptureNative(mIndex, texture, type, key);
    }
    if (mIndex < 0 || !texture || !matches_native_type(*texture, type))
    {
        return false;
    }

    const U32 sampler = type == TT_MULTISAMPLE_TEXTURE
        ? 0 : gGL.getSampler(key);
    if (mCurrNativeTexture != texture || mCurrImage ||
        mCurrTexType != type || mCurrSampler != sampler || forceBind)
    {
        gGL.flush();
    }
    mCurrTexture = 1u; // logical native binding; never handed to OpenGL
    mCurrTexType = type;
    mCurrImage = nullptr;
    mCurrNativeTexture = texture;
    bindNativeSampler(sampler);
    return true;
#else
    (void)texture;
    (void)type;
    (void)key;
    (void)forceBind;
    return false;
#endif
}

bool ALTextureSlot::bind(LLCubeMap* cubeMap, ALSampler key)
{
    if (mIndex < 0) return false;

    if (cubeMap == NULL)
    {
        LL_WARNS() << "NULL ALTextureSlot::bind cubemap" << LL_ENDL;
        return false;
    }

#if LL_DARWIN
    return bindNative(cubeMap->getMetalTexture(), TT_CUBE_MAP, key);
#else
    return false;
#endif
}

// Render targets publish typed native attachments. Keep the historical
// sampler-name argument because several shader helpers already resolve their
// ALSampler key before arriving here, but never manufacture an integer
// texture identity for the resource itself.
bool ALTextureSlot::bind(LLRenderTarget* renderTarget, bool bindDepth, U32 sampler)
{
    const bool authoritative = gGL.isMetalUITextureCaptureAuthoritative();
    if (mIndex < 0 || !renderTarget)
    {
        if (authoritative)
            gGL.invalidateMetalUITextureCapture("native UI render target was absent");
        return false;
    }

    ALMetalTexture* texture = const_cast<ALMetalTexture*>(bindDepth
        ? renderTarget->getMetalDepthTexture()
        : renderTarget->getMetalColorTexture(0));
    if (!texture || !matches_native_type(*texture, renderTarget->getUsage()))
    {
        if (authoritative)
            gGL.invalidateMetalUITextureCapture("native UI render target attachment was absent or incompatible");
        llassert(!bindDepth || renderTarget->hasDepthAttachment());
        return false;
    }

    sampler = sampler ? sampler : (bindDepth
        ? renderTarget->getDefaultDepthSampler()
        : renderTarget->getDefaultColorSampler(0));
    if (authoritative)
    {
        // UI shares raw colour-texture admission (depth/integer/layered inputs
        // fail closed). Ordinary world binding below keeps its typed contract.
        // Resolve the complete cached descriptor: a custom sampler may address
        // S and T differently, which no single ALSampler mask can represent.
        ALSamplerDesc desc;
        if (!gGL.describeSampler(sampler, desc))
        {
            gGL.invalidateMetalUITextureCapture("native UI render target sampler was not available");
            return false;
        }
        return gGL.bindMetalUITextureCaptureNative(
            mIndex, texture, renderTarget->getUsage(), desc);
    }
    if (mCurrNativeTexture != texture || mCurrImage ||
        mCurrTexType != renderTarget->getUsage() || mCurrSampler != sampler)
    {
        gGL.flush();
    }
    mCurrTexture = 1u;
    mCurrTexType = renderTarget->getUsage();
    mCurrImage = nullptr;
    mCurrNativeTexture = texture;
    bindNativeSampler(sampler);
    return true;
}

bool ALTextureSlot::bindManual(eTextureType type, U32 texture, U32 sampler)
{
    if (mIndex < 0)
    {
        return false;
    }

    LLImageGL* next_image = (type == TT_TEXTURE && texture == sWhiteTexture.load())
        ? sWhiteImage.load() : nullptr;
#if LL_DARWIN
    if (!next_image || !next_image->getMetalTexture())
    {
        return false;
    }
    if (mCurrImage != next_image || mCurrTexType != TT_TEXTURE ||
        mCurrSampler != sampler)
    {
        gGL.flush();
    }
    mCurrTexture = 1u;
    mCurrTexType = TT_TEXTURE;
    mCurrImage = next_image;
    mCurrNativeTexture = nullptr;
    bindNativeSampler(sampler);
    return true;
#else
    (void)type;
    (void)texture;
    (void)sampler;
    return false;
#endif
}

void ALTextureSlot::unbind()
{
#if LL_DARWIN
    if (gGL.isMetalUITextureCaptureAuthoritative())
    {
        gGL.unbindMetalUITextureCapture(mIndex);
        return;
    }
#endif
    if (mIndex < 0) return;

    gGL.flush();
    mCurrTexture = 0;
    mCurrTexType = TT_NONE;
    mCurrImage = nullptr;
    mCurrNativeTexture = nullptr;
    // Keep the sampler identity: the next resource bind names its own sampler,
    // and clearing it here would manufacture an extra state transition.
}

void ALTextureSlot::unbindFast()
{
#if LL_DARWIN
    if (gGL.isMetalUITextureCaptureAuthoritative())
    {
        gGL.unbindMetalUITextureCapture(mIndex);
        return;
    }
#endif
    mCurrTexture = 0;
    mCurrTexType = TT_NONE;
    mCurrImage = nullptr;
    mCurrNativeTexture = nullptr;
}
