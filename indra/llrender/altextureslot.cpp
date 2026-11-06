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
    bool issue_opengl_texture() noexcept
    {
        if (!gGLManager.mInited)
        {
            return false;
        }
#if LL_DARWIN
        return !gGL.isMetalUITextureCaptureAuthoritative();
#else
        return true;
#endif
    }

#if LL_DARWIN
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
    sWhiteTexture = image ? image->getTexName() : 0;
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

void ALTextureSlot::refreshState(void)
{
    // We set dirty to true so that the slot knows to ignore caching
    // and we reset the cached slot state

    gGL.flush();

#if LL_DARWIN
    // A logical native token is deliberately not a GL object name. Context
    // refresh must never reinterpret it as one.
    if (mCurrNativeTexture ||
        (mCurrImage && !mCurrImage->getTexName() &&
         mCurrImage->getMetalTexture()))
    {
        mCurrSampler = 0;
        return;
    }
#endif

    if (!issue_opengl_texture())
    {
        return;
    }

    glActiveTexture(GL_TEXTURE0 + mIndex);

    if (mCurrTexType != TT_NONE)
    {
        glBindTexture(sGLTextureType[mCurrTexType], mCurrTexture);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Samplers are logical Metal descriptors now. A context refresh has no
    // native sampler object to re-assert, but clearing the identity ensures the
    // next sampled bind republishes the descriptor selected by its caller.
    mCurrSampler = 0;
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

    if (issue_opengl_texture())
    {
        glBindSampler(mIndex, sampler);
    }
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

void ALTextureSlot::activate(void)
{
    if (mIndex < 0) return;

    if ((S32)gGL.mCurrTextureUnitIndex != mIndex || gGL.mDirty)
    {
        gGL.flush();
        if (issue_opengl_texture())
        {
            glActiveTexture(GL_TEXTURE0 + mIndex);
        }
        gGL.mCurrTextureUnitIndex = mIndex;
    }
}

void ALTextureSlot::setTarget(eTextureType type)
{
    if (mIndex < 0) return;

    if ( (mCurrTexType != type || gGL.mDirty) && (type != TT_NONE) )
    {
        activate();
        if (mCurrTexType != TT_NONE && !gGL.mDirty)
        {
            unbind(); // Release a resource of a different target first.
        }
        mCurrTexType = type;

        gGL.flush();
    }
}

// Sampler named by the CALLER: the bind site says how it wants to read, and the texture
// supplies only its data.
void ALTextureSlot::bindFast(LLTexture* texture, ALSampler key)
{
    // getGLTexture() is virtual and this is the per-draw path, so it is resolved once and the
    // sampler is chosen from the result rather than re-fetching.
    LLImageGL* gl_tex = texture->getGLTexture();
#if LL_DARWIN
    if (gGL.isMetalUITextureCaptureAuthoritative())
    {
        texture->setActive();
        gGL.bindMetalUITextureCapture(mIndex, gl_tex, key);
        if (gl_tex)
        {
            gl_tex->forceUpdateBindStats();
        }
        return;
    }
#endif
    bindFastImpl(texture, gl_tex, gGL.getSampler(key));
}

void ALTextureSlot::bindFastImpl(LLTexture* texture, LLImageGL* gl_tex, U32 sampler)
{
    llassert(gl_tex);
    ++sTextureBinds;
    texture->setActive();
    U32 texname = gl_tex->getTexName();
    eTextureType target = gl_tex->getTarget();
    const bool issue_gl = issue_opengl_texture();
#if LL_DARWIN
    if (!issue_gl && !gl_tex->getMetalTexture())
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
    const bool native_only = !texname && gl_tex->getMetalTexture();
    if (native_only)
    {
        // Native-only resources never cross back through a white GL
        // placeholder. Publish the real Metal identity and sampler to the
        // logical slot; retained GL traversal must not reinterpret token 1 as
        // an object name.
        mCurrTexture = 1u;
        mCurrTexType = TT_TEXTURE;
        mCurrImage = gl_tex;
        mCurrNativeTexture = nullptr;
        bindNativeSampler(sampler);
        gl_tex->forceUpdateBindStats();
        return;
    }
#endif
    if (issue_gl)
    {
        glActiveTexture(GL_TEXTURE0 + mIndex);
    }
    gGL.mCurrTextureUnitIndex = mIndex;
    mCurrTexture = texname;
    mCurrImage = gl_tex;
    mCurrNativeTexture = nullptr;
    if (!mCurrTexture)
    {
        LL_PROFILE_ZONE_NAMED("MISSING TEXTURE");
        //if deleted, will re-generate it immediately
        texture->forceImmediateUpdate();
        gl_tex->forceUpdateBindStats();
        texture->bindDefaultImage(mIndex);
        return;
    }
    // Classic bind, NOT glBindTextureUnit, even where DSA is available.
    //
    // The DSA form issues one GL call here instead of two, but it deliberately leaves the
    // active unit alone -- and this function used to park gGL.mCurrTextureUnitIndex at its
    // own index on every call, which let later activate() calls on the same slot skip their
    // glActiveTexture. Not doing that hands the saved call back at the next activate(), so
    // the win is smaller than it looks and can invert depending on the bind/activate mix.
    //
    // (It also differs on a stale name: glBindTexture silently conjures a fresh empty
    // texture object, glBindTextureUnit raises GL_INVALID_OPERATION.)
    if (issue_gl)
    {
        glBindTexture(sGLTextureType[target], mCurrTexture);
    }

    // Select the sampler. No flush, per this function's contract -- consistent with the
    // texture binding above, which is changed the same way.
    if (mCurrSampler != sampler)
    {
        ++sSamplerBinds;
        if (issue_gl)
        {
            glBindSampler(mIndex, sampler);
        }
        mCurrSampler = sampler;
    }
    else
    {
        ++sSamplerSkips;
    }

    // Track the actual bound target so later callers that read mCurrTexType don't act on a
    // stale target left over from earlier passes. The fast path used to skip this -- the
    // slow setTarget() path was the only place mCurrTexType was updated.
    mCurrTexType = target;
#if LL_DARWIN
    if (native_only)
    {
        gl_tex->forceUpdateBindStats();
    }
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
        const bool complete =
            gGL.bindMetalUITextureCapture(mIndex, texture, key);
        if (texture)
        {
            texture->forceUpdateBindStats();
        }
        return complete;
    }
    if (texture && !texture->getTexName() && texture->getMetalTexture())
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

// The sampler is selected ONCE here, from the caller's mask. The bind-then-setSampler*
// sequence this replaces could issue two glBindSampler calls and, worse, a gGL.flush()
// between them, splitting the draw batch for a sampler about to change again.
bool ALTextureSlot::bindImpl(LLTexture* texture, bool forceBind, ALSampler key)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
#if LL_DARWIN
    if (gGL.isMetalUITextureCaptureAuthoritative())
    {
        (void)forceBind;
        LLImageGL* image = texture ? texture->getGLTexture() : nullptr;
        const bool complete =
            gGL.bindMetalUITextureCapture(mIndex, image, key);
        if (texture && image)
        {
            texture->setActive();
            image->forceUpdateBindStats();
        }
        return complete;
    }
#endif
    stop_glerror();
    if (mIndex >= 0)
    {
        gGL.flush();

        LLImageGL* gl_tex = NULL ;

        if (texture != NULL && (gl_tex = texture->getGLTexture()))
        {
            U32 texname = gl_tex->getTexName();
            eTextureType target = gl_tex->getTarget();
#if LL_DARWIN
            if (!issue_opengl_texture() && !gl_tex->getMetalTexture())
            {
                texture->forceImmediateUpdate();
                if (!gl_tex->getMetalTexture())
                {
                    gl_tex->forceUpdateBindStats();
                    return texture->bindDefaultImage(mIndex);
                }
            }
            const bool native_only = !texname && gl_tex->getMetalTexture();
            if (native_only)
            {
                texture->setActive();
                return bindNativeSampled(
                    gl_tex, gGL.getSampler(key), forceBind);
            }
#endif
            if (texname)
            {
                if ((mCurrTexture != texname) || mCurrTexType != target || forceBind)
                {
                    activate();
                    setTarget(target);
                    mCurrTexture = texname;
                    if (issue_opengl_texture())
                    {
                        glBindTexture(sGLTextureType[target], mCurrTexture);
                    }
#if LL_DARWIN
                    if (!native_only && gl_tex->updateBindStats())
#else
                    if (gl_tex->updateBindStats())
#endif
                    {
                        texture->setActive();
                    }
                }
                // setTarget can unbind another target and install the white image. The
                // logical resource is what native capture needs, independent of the GL
                // placeholder actually bound above.
                mCurrImage = gl_tex;
                mCurrNativeTexture = nullptr;
#if LL_DARWIN
                if (native_only)
                {
                    gl_tex->forceUpdateBindStats();
                    texture->setActive();
                }
#endif
                bindSampler(gGL.getSampler(key));
            }
            else
            {
                //if deleted, will re-generate it immediately
                texture->forceImmediateUpdate() ;

                gl_tex->forceUpdateBindStats() ;
                return texture->bindDefaultImage(mIndex);
            }
        }
        else
        {
            if (texture)
            {
                LL_DEBUGS() << "NULL ALTextureSlot::bind GL image" << LL_ENDL;
            }
            else
            {
                LL_DEBUGS() << "NULL ALTextureSlot::bind texture" << LL_ENDL;
            }
            return false;
        }
    }
    else
    { // mIndex < 0
        return false;
    }

    return true;
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
    if (mIndex < 0) return false;

    if(!texture)
    {
        LL_DEBUGS() << "NULL ALTextureSlot::bind texture" << LL_ENDL;
        return false;
    }

#if LL_DARWIN
    if (!usename && !texture->getTexName() && texture->getMetalTexture())
    {
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
    }
#endif

    if (!issue_opengl_texture())
    {
#if LL_DARWIN
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
        return false;
#endif
    }

    stop_glerror();

    // Resolve texname after the null-check; previously this line dereferenced
    // `texture` when usename == 0 and texture was null, crashing before the
    // diagnostic could fire.
    U32 texname = usename ? usename : texture->getTexName();

    // A supplied name is a transient upload/edit target and may not be the
    // resource represented by texture's published Metal mirror. Treat the
    // logical identity as draw state: geometry queued under the previous
    // identity must drain before it changes, even when the GL name happens to
    // be the same (for example, bind-to-edit through an explicit name).
    LLImageGL* next_image = usename ? nullptr : texture;

    if(!texname)
    {
        if(LLImageGL::sDefaultGLTexture && LLImageGL::sDefaultGLTexture->getTexName())
        {
            return bind(LLImageGL::sDefaultGLTexture) ;
        }
        stop_glerror();
        return false ;
    }

    if (mCurrImage != next_image)
    {
        gGL.flush();
        mCurrImage = next_image;
        mCurrNativeTexture = nullptr;
    }

    if ((mCurrTexture != texname) || forceBind)
    {
        gGL.flush();
        stop_glerror();
        activate();
        stop_glerror();
        setTarget(texture->getTarget());
        stop_glerror();
        mCurrTexture = texname;
        glBindTexture(sGLTextureType[texture->getTarget()], mCurrTexture);
        stop_glerror();
        // A name passed in is one an upload writes into, on the LLImageGL thread or before
        // it publishes: not a use of the texture, and the frame time the stats stamp with is
        // the main thread's.
        if (!usename)
        {
            texture->updateBindStats();
        }
    }

    // The sampler is deliberately LEFT ALONE.
    //
    // This overload is bind-to-edit -- allocate, upload, generate mips -- and self-binds.
    // Nothing samples through it, so whatever sampler the slot happens to be holding is
    // irrelevant, and dropping it to 0 would cost a glBindSampler now plus another when the
    // next draw restores one. bindSampler() also flushes, so that pair would break the batch
    // every time a texture is uploaded mid-frame.
    //
    // Safe because no bind can sample without naming a sampler, and this path binds nothing
    // sampleable in the first place. See unbind() for the case where that is not enough.
    stop_glerror();

    return true;
}

bool ALTextureSlot::bindNative(ALMetalTexture* texture, eTextureType type,
                               ALSampler key, bool forceBind)
{
#if LL_DARWIN
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

// LLRenderTarget is unavailible on the mapserver since it uses FBOs.
bool ALTextureSlot::bind(LLRenderTarget* renderTarget, bool bindDepth, U32 sampler)
{
    if (mIndex < 0) return false;

    gGL.flush();

    if (bindDepth)
    {
        llassert(renderTarget->getDepth()); // target MUST have a depth buffer attachment

        // Sampler 0 means "sample through the texture object's own state", and render target
        // textures no longer carry any -- allocation expresses it as a sampler instead. So an
        // unspecified sampler resolves to the target's default rather than to nothing, which
        // would otherwise leave these binds on GL's defaults (nearest, repeat) and quietly
        // unfilter every pass that reads a target without naming a sampler.
        bindManual(renderTarget->getUsage(), renderTarget->getDepth(),
                   sampler ? sampler : renderTarget->getDefaultDepthSampler());
    }
    else
    {
        bindManual(renderTarget->getUsage(), renderTarget->getTexture(),
                   sampler ? sampler : renderTarget->getDefaultColorSampler(0));
    }

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
    if (!issue_opengl_texture())
    {
#if LL_DARWIN
        if (!next_image || !next_image->getMetalTexture())
        {
            return false;
        }
        gGL.flush();
        mCurrTexture = 1u;
        mCurrTexType = TT_TEXTURE;
        mCurrImage = next_image;
        mCurrNativeTexture = nullptr;
        bindNativeSampler(sampler);
        return true;
#else
        return false;
#endif
    }
    if (mCurrImage != next_image)
    {
        gGL.flush();
        mCurrImage = next_image;
        mCurrNativeTexture = nullptr;
    }

    if(mCurrTexture != texture)
    {
        gGL.flush();

        activate();
        setTarget(type);
        mCurrTexture = texture;
        // Classic bind rather than glBindTextureUnit even under DSA: several callers pass
        // a name straight out of glGenTextures and rely on this call to establish its
        // target (LLRenderTarget::allocateColorTexture and the pipeline
        // noise/SMAA LUTs). glBindTextureUnit rejects a target-less name.
        glBindTexture(sGLTextureType[type], texture);
    }

    // Outside the redundancy check: re-binding the same texture with a different sampler
    // is exactly what the post-process chain does, so this must not be skipped when the
    // texture happens to match. Multisample textures cannot be sampled through a sampler
    // object at all -- that is a draw-time error, not a silent fallback.
    bindSampler(type == TT_MULTISAMPLE_TEXTURE ? 0 : sampler);

    return true;
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

    const bool native_binding = getCurrMetalTexture() &&
        (mCurrNativeTexture || !mCurrImage || !mCurrImage->getTexName());
    const bool issue_gl = issue_opengl_texture() && !native_binding;
    if (issue_gl)
    {
        stop_glerror();
    }

    //always flush and activate for consistency
    //   some code paths assume unbind always flushes and sets the active texture
    gGL.flush();
    if (issue_gl)
    {
        activate();
    }

    if (mCurrTexType != TT_NONE)
    {
        mCurrTexture = 0;
        mCurrImage = (mCurrTexType == TT_TEXTURE) ? sWhiteImage.load() : nullptr;
        mCurrNativeTexture = nullptr;

        if (issue_gl && mCurrTexType == TT_TEXTURE)
        {
            glBindTexture(sGLTextureType[mCurrTexType], sWhiteTexture);
        }
        else if (issue_gl)
        {
            glBindTexture(sGLTextureType[mCurrTexType], 0);
        }

        // TT_NONE even where the white placeholder is now bound. It means "this slot holds
        // nothing a caller should reason about", which is true either way, and it is what
        // makes LLGLSLShader::disableTexture's target check meaningful: a slot that was
        // enabled but never bound reads TT_NONE and is skipped, so the check only ever
        // compares a target something actually put there.
        mCurrTexType = TT_NONE;

        // Sampler left in place. Whoever binds here next names their own, so releasing it
        // would just be a glBindSampler now and another one then -- with a flush attached.
        //
        // Retained application-owned GL sampling has no compare-sampler users. A future
        // compare sampler must own its teardown explicitly before this optimization remains
        // safe.
        stop_glerror();
    }
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
    const bool native_binding = getCurrMetalTexture() &&
        (mCurrNativeTexture || !mCurrImage || !mCurrImage->getTexName());
    if (!native_binding)
    {
        activate();
    }

    if (mCurrTexType != TT_NONE)
    {
        mCurrTexture = 0;
        mCurrImage = (mCurrTexType == TT_TEXTURE) ? sWhiteImage.load() : nullptr;
        mCurrNativeTexture = nullptr;

        const bool issue_gl = issue_opengl_texture() && !native_binding;
        if (issue_gl && mCurrTexType == TT_TEXTURE)
        {
            glBindTexture(sGLTextureType[mCurrTexType], sWhiteTexture);
        }
        else if (issue_gl)
        {
            glBindTexture(sGLTextureType[mCurrTexType], 0);
        }

        mCurrTexType = TT_NONE;

        // Sampler left in place -- see unbind(), including the compare-sampler exception. This
        // is the per-draw unbind, so the pair of glBindSampler calls it used to cost (release
        // here, restore at the next bind) landed squarely in the hot loop.
    }
}


// Useful for debugging that you've manually assigned a texture operation to the correct
// slot based on the currently set active texture in opengl.
void ALTextureSlot::debugTextureUnit(void)
{
    if (mIndex < 0 || !issue_opengl_texture()) return;

    GLint activeTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    if ((GL_TEXTURE0 + mIndex) != activeTexture)
    {
        U32 set_unit = (activeTexture - GL_TEXTURE0);
        LL_WARNS() << "Incorrect texture slot!  Expected: " << set_unit << " Actual: " << mIndex << LL_ENDL;
    }
}
