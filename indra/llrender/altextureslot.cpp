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

static const GLenum sGLTextureType[] =
{
    GL_TEXTURE_2D,
    GL_TEXTURE_RECTANGLE,
    GL_TEXTURE_CUBE_MAP,
    GL_TEXTURE_CUBE_MAP_ARRAY,
    GL_TEXTURE_2D_MULTISAMPLE,
    GL_TEXTURE_3D
};

#if LL_DARWIN
static U32 native_sample_fallback()
{
    U32 fallback = ALTextureSlot::sWhiteTexture.load();
    if (!fallback && LLImageGL::sDefaultGLTexture)
    {
        fallback = LLImageGL::sDefaultGLTexture->getTexName();
    }
    return fallback;
}
#endif

thread_local U32 ALTextureSlot::sSamplerBinds  = 0;
thread_local U32 ALTextureSlot::sSamplerSkips  = 0;
thread_local U32 ALTextureSlot::sTextureBinds  = 0;
thread_local U32 ALTextureSlot::sSamplerBindsFlushed = 0;
thread_local U32 ALTextureSlot::sSamplerBindsSplitBatch = 0;
thread_local U32 ALTextureSlot::sSamplerBindsShadowCycle = 0;

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

    glActiveTexture(GL_TEXTURE0 + mIndex);

    if (mCurrTexType != TT_NONE)
    {
        glBindTexture(sGLTextureType[mCurrTexType], mCurrTexture);
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // The sampler binding is per-slot context state like the texture binding, so it has
    // to be re-asserted here too. Drop to the texture object's own state and let the next
    // bind re-select; mCurrSampler is cleared so that re-selection isn't cached away.
    glBindSampler(mIndex, 0);
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

    glBindSampler(mIndex, sampler);
    mCurrSampler = sampler;
}

void ALTextureSlot::activate(void)
{
    if (mIndex < 0) return;

    if ((S32)gGL.mCurrTextureUnitIndex != mIndex || gGL.mDirty)
    {
        gGL.flush();
        glActiveTexture(GL_TEXTURE0 + mIndex);
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
    ++sTextureBinds;
    texture->setActive();
    U32 texname = gl_tex->getTexName();
    eTextureType target = gl_tex->getTarget();
#if LL_DARWIN
    const bool native_only = !texname && gl_tex->getMetalTexture();
    if (native_only)
    {
        texname = native_sample_fallback();
        target = TT_TEXTURE;
    }
#endif
    glActiveTexture(GL_TEXTURE0 + mIndex);
    gGL.mCurrTextureUnitIndex = mIndex;
    mCurrTexture = texname;
    mCurrImage = gl_tex;
    if (!mCurrTexture)
    {
        LL_PROFILE_ZONE_NAMED("MISSING TEXTURE");
        //if deleted, will re-generate it immediately
        texture->forceImmediateUpdate();
        gl_tex->forceUpdateBindStats();
        texture->bindDefaultImage(mIndex);
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
    glBindTexture(sGLTextureType[target], mCurrTexture);

    // Select the sampler. No flush, per this function's contract -- consistent with the
    // texture binding above, which is changed the same way.
    if (mCurrSampler != sampler)
    {
        ++sSamplerBinds;
        glBindSampler(mIndex, sampler);
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
            const bool native_only = !texname && gl_tex->getMetalTexture();
            if (native_only)
            {
                texname = native_sample_fallback();
                target = TT_TEXTURE;
            }
#endif
            if (texname)
            {
                if ((mCurrTexture != texname) || mCurrTexType != target || forceBind)
                {
                    activate();
                    setTarget(target);
                    mCurrTexture = texname;
                    glBindTexture(sGLTextureType[target], mCurrTexture);
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
    if (mIndex < 0 || !image || image->getTexName() || !image->getMetalTexture() ||
        image->getTarget() != TT_TEXTURE)
    {
        return false;
    }

    const U32 fallback = native_sample_fallback();
    if (!fallback)
    {
        return false;
    }

    const bool physical_change = mCurrTexture != fallback ||
                                 mCurrTexType != TT_TEXTURE || forceBind;
    if (mCurrImage != image || physical_change)
    {
        gGL.flush();
    }
    if (physical_change)
    {
        LLImageGL* white = sWhiteImage.load();
        if (white && white->getTexName() == fallback)
        {
            if (!bind(white, false, forceBind))
            {
                return false;
            }
        }
        else if (!bindManual(TT_TEXTURE, fallback))
        {
            return false;
        }
    }

    // Physical fallback binds record the white/default image. Replace that bookkeeping only
    // after the prior batch is drained so native capture sees this image for subsequent work.
    mCurrImage = image;
    bindSampler(sampler);
    image->forceUpdateBindStats();
    return true;
}
#endif

bool ALTextureSlot::bind(LLImageGL* texture, bool for_rendering, bool forceBind, S32 usename)
{
    stop_glerror();
    if (mIndex < 0) return false;

    if(!texture)
    {
        LL_DEBUGS() << "NULL ALTextureSlot::bind texture" << LL_ENDL;
        return false;
    }

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

bool ALTextureSlot::bind(LLCubeMap* cubeMap, ALSampler key)
{
    if (mIndex < 0) return false;

    gGL.flush();
    mCurrImage = nullptr;

    if (cubeMap == NULL)
    {
        LL_WARNS() << "NULL ALTextureSlot::bind cubemap" << LL_ENDL;
        return false;
    }

    // mImages[0] is normally populated by LLCubeMap::initGL, but a
    // partially-constructed cubemap could have a null face here.
    if (cubeMap->mImages[0].isNull())
    {
#if LL_DARWIN
        // The native-only legacy sky cube deliberately has no GL face. Clear
        // any older cube binding so retained GL traversal cannot sample stale
        // storage while the Metal world owner consumes getMetalTexture().
        unbind();
#else
        LL_WARNS() << "ALTextureSlot::bind cubemap with null face 0" << LL_ENDL;
#endif
        return false;
    }

    if (mCurrTexture != cubeMap->mImages[0]->getTexName())
    {
        activate();
        setTarget(ALTextureSlot::TT_CUBE_MAP);
        mCurrTexture = cubeMap->mImages[0]->getTexName();
        glBindTexture(GL_TEXTURE_CUBE_MAP, mCurrTexture);
        cubeMap->mImages[0]->updateBindStats();
    }

    // Named by the caller, not read off face 0. A cube map is as much a shared
    // resource as any other texture -- the environment map is sampled by every
    // shiny surface in the scene -- so which sampler a pass wants is the pass's
    // business. Outside the redundancy check, as in bindImpl: the same cube can be
    // bound again after a pass that left a different sampler on this slot.
    bindSampler(gGL.getSampler(key));
    return true;
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
    if (mCurrImage != next_image)
    {
        gGL.flush();
        mCurrImage = next_image;
    }

    if(mCurrTexture != texture)
    {
        gGL.flush();

        activate();
        setTarget(type);
        mCurrTexture = texture;
        // Classic bind rather than glBindTextureUnit even under DSA: several callers pass
        // a name straight out of glGenTextures and rely on this call to establish its
        // target (LLRenderTarget::allocateColorTexture, LLCubeMap::initGL, the pipeline
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
    stop_glerror();

    if (mIndex < 0) return;

    //always flush and activate for consistency
    //   some code paths assume unbind always flushes and sets the active texture
    gGL.flush();
    activate();

    if (mCurrTexType != TT_NONE)
    {
        mCurrTexture = 0;
        mCurrImage = (mCurrTexType == TT_TEXTURE) ? sWhiteImage.load() : nullptr;

        if (mCurrTexType == TT_TEXTURE)
        {
            glBindTexture(sGLTextureType[mCurrTexType], sWhiteTexture);
        }
        else
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
        // ONE EXCEPTION, and it is not theoretical -- it shipped as a driver warning before
        // being caught. This leaves the WHITE PLACEHOLDER on the slot, which is sampleable,
        // so a leftover COMPARE sampler is read through depth comparison on a non-depth
        // texture: undefined. Whoever selects a compare sampler must therefore release it
        // itself rather than relying on this; LLPipeline::bindShadowMaps and unbindShadowMaps
        // call bindSampler(0) on the channels they give up.
        //
        // It is not enough to track the channels and unbind the TEXTURE. That is what those
        // functions already did, and it is exactly what stopped being sufficient the moment
        // this stopped clearing the sampler as a side effect.
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
    activate();

    if (mCurrTexType != TT_NONE)
    {
        mCurrTexture = 0;
        mCurrImage = (mCurrTexType == TT_TEXTURE) ? sWhiteImage.load() : nullptr;

        if (mCurrTexType == TT_TEXTURE)
        {
            glBindTexture(sGLTextureType[mCurrTexType], sWhiteTexture);
        }
        else
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
    if (mIndex < 0) return;

    GLint activeTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    if ((GL_TEXTURE0 + mIndex) != activeTexture)
    {
        U32 set_unit = (activeTexture - GL_TEXTURE0);
        LL_WARNS() << "Incorrect texture slot!  Expected: " << set_unit << " Actual: " << mIndex << LL_ENDL;
    }
}
