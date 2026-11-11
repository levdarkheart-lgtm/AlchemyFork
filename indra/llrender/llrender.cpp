 /**
 * @file llrender.cpp
 * @brief LLRender implementation
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

#include "linden_common.h"

#include "llrender.h"

#include "algpubuffer.h"
#include "alsamplerstate.h"
#include "llvertexbuffer.h"
#include "llcubemap.h"
#include "llglslshader.h"
#include "llimagegl.h"
#include "llrendertarget.h"
#include "lltexture.h"
#include "llshadermgr.h"
#include "hbxxh.h"
#include "alprojection.h"
#include "llcamera.h"
#include "llthread.h"

#if LL_DARWIN
#include "almetaldevice.h"
#include "almetalresource.h"
#include "almetalsurface.h"
#include "almetaluirenderer.h"
#endif

#include <algorithm>
#include <cstdint>

thread_local LLRender gGL;

#if LL_DARWIN
U64 LLRender::metalCommandBuffersCommitted()
{
    return ALMetalDevice::committedCommandBufferCount();
}

void LLRender::setMetalCommandTimingEnabled(bool enabled)
{
    ALMetalDevice::setCommandTimingEnabled(enabled);
}

LLRender::MetalCommandTiming LLRender::metalCommandTiming()
{
    const ALMetalCommandTiming timing = ALMetalDevice::commandTiming();
    MetalCommandTiming result;
    result.submitted = timing.submitted;
    result.completed = timing.completed;
    result.timed = timing.timed;
    result.gpu_time_nanoseconds = timing.gpu_time_nanoseconds;
    return result;
}
#endif

#if LL_DARWIN
namespace
{
    U32 metal_ui_capture_cache_index(
        ALMetalPixelFormat format, bool linear_storage)
    {
        U32 index = 8;
        switch (format)
        {
            case ALMetalPixelFormat::rgba8_unorm:
                index = 0;
                break;
            case ALMetalPixelFormat::rgba8_unorm_srgb:
                index = 1;
                break;
            case ALMetalPixelFormat::bgra8_unorm:
                index = 2;
                break;
            case ALMetalPixelFormat::bgra8_unorm_srgb:
                index = 3;
                break;
            default:
                break;
        }
        return index < 4 && linear_storage ? index + 4 : index;
    }
}
#endif

S32 gGLViewport[4];


U32 LLRender::sUICalls = 0;
U32 LLRender::sUIVerts = 0;
U32 LLRender::sDrawCalls = 0;
U64 LLRender::sDrawElements = 0;
std::atomic<U32> ALTextureSlot::sWhiteTexture = 0;
std::atomic<LLImageGL*> ALTextureSlot::sWhiteImage = nullptr;
F32 LLRender::sAnisotropicFilteringLevel = 0.f;
// Read by the native Win32 window alone, which makes a compatibility context when it is false
// (RenderGLContextCoreProfile, which Intel's Windows driver is recommended off). Every other window,
// and anything that never sets it, as the appearance utility does not, is core.
bool LLRender::sGLCoreProfile = true;
bool LLRender::sNsightDebugSupport = false;
LLVector2 LLRender::sUIGLScaleFactor = LLVector2(1.f, 1.f);
bool LLRender::sClassicMode = false;
bool LLRender::sMirrorPass = false;
bool LLRender::sReverseZ = false;
bool LLRender::sCompute = false;
bool LLRender::sGBufferNormHDR = true;
bool LLRender::s10bitBackBuffer = false;


const U32 immediate_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_COLOR | LLVertexBuffer::MAP_TEXCOORD0;

#if LL_DARWIN
namespace
{
    enum class MetalUIFallbackReason : std::size_t
    {
        incomplete_batch,
        shader,
        primitive,
        mirror,
        viewport,
        encoder,
        count,
    };

    std::array<U64, static_cast<std::size_t>(MetalUIFallbackReason::count)>
        sMetalUIFallbackReasons{};

    const char* metal_ui_fallback_reason(MetalUIFallbackReason reason)
    {
        switch (reason)
        {
            case MetalUIFallbackReason::incomplete_batch:
                return "empty or incomplete immediate-mode batch";
            case MetalUIFallbackReason::shader:
                return "unsupported shader";
            case MetalUIFallbackReason::primitive:
                return "unsupported primitive or cull mode";
            case MetalUIFallbackReason::mirror:
                return "missing Metal buffer or texture mirror";
            case MetalUIFallbackReason::viewport:
                return "invalid viewport";
            case MetalUIFallbackReason::encoder:
                return "Metal encoder rejected the batch";
            case MetalUIFallbackReason::count:
                break;
        }
        return "unknown UI capture failure";
    }

    ALMetalBlendFactor metal_blend_factor(LLRender::eBlendFactor factor)
    {
        switch (factor)
        {
            case LLRender::BF_ZERO:                   return ALMetalBlendFactor::zero;
            case LLRender::BF_ONE:                    return ALMetalBlendFactor::one;
            case LLRender::BF_SOURCE_COLOR:           return ALMetalBlendFactor::source_color;
            case LLRender::BF_ONE_MINUS_SOURCE_COLOR: return ALMetalBlendFactor::one_minus_source_color;
            case LLRender::BF_SOURCE_ALPHA:           return ALMetalBlendFactor::source_alpha;
            case LLRender::BF_ONE_MINUS_SOURCE_ALPHA: return ALMetalBlendFactor::one_minus_source_alpha;
            case LLRender::BF_DEST_COLOR:             return ALMetalBlendFactor::destination_color;
            case LLRender::BF_ONE_MINUS_DEST_COLOR:   return ALMetalBlendFactor::one_minus_destination_color;
            case LLRender::BF_DEST_ALPHA:             return ALMetalBlendFactor::destination_alpha;
            case LLRender::BF_ONE_MINUS_DEST_ALPHA:   return ALMetalBlendFactor::one_minus_destination_alpha;
            case LLRender::BF_UNDEF:                  return ALMetalBlendFactor::one;
        }
        return ALMetalBlendFactor::one;
    }

    ALMetalBlendFactor metal_alpha_blend_factor(
        LLRender::eBlendFactor factor)
    {
        // OpenGL applies color factors component-wise, so GL_SOURCE_COLOR and
        // GL_DEST_COLOR select source/destination alpha for the alpha
        // component. Metal expresses that contract with its explicit alpha
        // factors; color factors are not valid for an alpha blend slot.
        switch (factor)
        {
            case LLRender::BF_SOURCE_COLOR:
                return ALMetalBlendFactor::source_alpha;
            case LLRender::BF_ONE_MINUS_SOURCE_COLOR:
                return ALMetalBlendFactor::one_minus_source_alpha;
            case LLRender::BF_DEST_COLOR:
                return ALMetalBlendFactor::destination_alpha;
            case LLRender::BF_ONE_MINUS_DEST_COLOR:
                return ALMetalBlendFactor::one_minus_destination_alpha;
            default:
                return metal_blend_factor(factor);
        }
    }

    ALMetalBlendOperation metal_blend_operation(U32 operation)
    {
        switch (operation)
        {
            case GL_FUNC_SUBTRACT:         return ALMetalBlendOperation::subtract;
            case GL_FUNC_REVERSE_SUBTRACT: return ALMetalBlendOperation::reverse_subtract;
            case GL_MIN:                   return ALMetalBlendOperation::minimum;
            case GL_MAX:                   return ALMetalBlendOperation::maximum;
            case GL_FUNC_ADD:
            default:                       return ALMetalBlendOperation::add;
        }
    }

    ALMetalCompareFunction metal_compare_function(U32 function)
    {
        switch (function)
        {
            case GL_NEVER:    return ALMetalCompareFunction::never;
            case GL_LESS:     return ALMetalCompareFunction::less;
            case GL_EQUAL:    return ALMetalCompareFunction::equal;
            case GL_LEQUAL:   return ALMetalCompareFunction::less_equal;
            case GL_GREATER:  return ALMetalCompareFunction::greater;
            case GL_NOTEQUAL: return ALMetalCompareFunction::not_equal;
            case GL_GEQUAL:   return ALMetalCompareFunction::greater_equal;
            case GL_ALWAYS:
            default:          return ALMetalCompareFunction::always;
        }
    }

    ALMetalSamplerFilter metal_sampler_filter(ALSamplerFilter filter)
    {
        return filter == ALSamplerFilter::nearest
            ? ALMetalSamplerFilter::nearest : ALMetalSamplerFilter::linear;
    }

    ALMetalSamplerAddressMode metal_sampler_address(ALSamplerAddressMode address)
    {
        switch (address)
        {
            case ALSamplerAddressMode::mirror_repeat:
                return ALMetalSamplerAddressMode::mirror_repeat;
            case ALSamplerAddressMode::clamp_to_zero:
                return ALMetalSamplerAddressMode::clamp_to_zero;
            case ALSamplerAddressMode::clamp_to_edge:
                return ALMetalSamplerAddressMode::clamp_to_edge;
            case ALSamplerAddressMode::repeat:
            default:
                return ALMetalSamplerAddressMode::repeat;
        }
    }

    ALMetalSamplerDescriptor metal_sampler_descriptor(const ALSamplerDesc& source)
    {
        ALMetalSamplerDescriptor result;
        result.min_filter = metal_sampler_filter(source.mMinFilter);
        switch (source.mMipFilter)
        {
            case ALSamplerMipFilter::none:
                result.mip_filter = ALMetalSamplerMipFilter::none;
                break;
            case ALSamplerMipFilter::nearest:
                result.mip_filter = ALMetalSamplerMipFilter::nearest;
                break;
            case ALSamplerMipFilter::linear:
            default:
                result.mip_filter = ALMetalSamplerMipFilter::linear;
                break;
        }
        result.mag_filter = metal_sampler_filter(source.mMagFilter);
        result.address_s = metal_sampler_address(source.mWrapS);
        result.address_t = metal_sampler_address(source.mWrapT);
        result.address_r = metal_sampler_address(source.mWrapR);
        result.max_anisotropy = static_cast<U32>(
            llclamp(ll_round(source.mMaxAnisotropy), 1, 16));
        result.label = "Alchemy production UI sampler";
        return result;
    }

    bool metal_ui_primitive(U32 mode, ALMetalUIPrimitive& result)
    {
        switch (mode)
        {
            case LLRender::POINTS:         result = ALMetalUIPrimitive::points; return true;
            case LLRender::LINES:          result = ALMetalUIPrimitive::lines; return true;
            case LLRender::LINE_STRIP:     result = ALMetalUIPrimitive::line_strip; return true;
            case LLRender::LINE_LOOP:      result = ALMetalUIPrimitive::line_loop; return true;
            case LLRender::TRIANGLES:      result = ALMetalUIPrimitive::triangles; return true;
            case LLRender::TRIANGLE_STRIP: result = ALMetalUIPrimitive::triangle_strip; return true;
            case LLRender::TRIANGLE_FAN:   result = ALMetalUIPrimitive::triangle_fan; return true;
            case LLRender::PATCHES:
            default:                       return false;
        }
    }
}
#endif

LLLightState::LLLightState(S32 index)
: mIndex(index),
  mEnabled(false),
  mConstantAtten(1.f),
  mLinearAtten(0.f),
  mQuadraticAtten(0.f),
  mSpotExponent(0.f),
  mSpotCutoff(180.f)
{
    if (mIndex == 0)
    {
        mDiffuse.set(1,1,1,1);
        mDiffuseB.set(0,0,0,0);
        mSpecular.set(1,1,1,1);
    }

    mSunIsPrimary = true;

    mAmbient.set(0,0,0,1);
    mPosition.set(0,0,1,0);
    mSpotDirection.set(0,0,-1);
}

void LLLightState::enable()
{
    mEnabled = true;
}

void LLLightState::disable()
{
    mEnabled = false;
}

void LLLightState::setDiffuse(const LLColor4& diffuse)
{
    if (mDiffuse != diffuse)
    {
        ++gGL.mLightHash;
        mDiffuse = diffuse;
    }
}

void LLLightState::setDiffuseB(const LLColor4& diffuse)
{
    if (mDiffuseB != diffuse)
    {
        ++gGL.mLightHash;
        mDiffuseB = diffuse;
    }
}

void LLLightState::setSunPrimary(bool v)
{
    if (mSunIsPrimary != v)
    {
        ++gGL.mLightHash;
        mSunIsPrimary = v;
    }
}

void LLLightState::setSize(F32 v)
{
    if (mSize != v)
    {
        ++gGL.mLightHash;
        mSize = v;
    }
}

void LLLightState::setFalloff(F32 v)
{
    if (mFalloff != v)
    {
        ++gGL.mLightHash;
        mFalloff = v;
    }
}

void LLLightState::setAmbient(const LLColor4& ambient)
{
    if (mAmbient != ambient)
    {
        ++gGL.mLightHash;
        mAmbient = ambient;
    }
}

void LLLightState::setSpecular(const LLColor4& specular)
{
    if (mSpecular != specular)
    {
        ++gGL.mLightHash;
        mSpecular = specular;
    }
}

void LLLightState::setPosition(const LLVector4& position)
{
    //always set position because modelview matrix may have changed
    ++gGL.mLightHash;
    mPosition = position;
    //transform position by current modelview matrix; w rides along, so a
    //directional light's zero stays zero
    LLVector4a pos;
    pos.loadua(position.mV);
    LLVector4a eye;
    gGL.getModelviewMatrix().transform4(pos, eye);
    mPosition.set(eye.getF32ptr());
}

void LLLightState::setConstantAttenuation(const F32& atten)
{
    if (mConstantAtten != atten)
    {
        mConstantAtten = atten;
        ++gGL.mLightHash;
    }
}

void LLLightState::setLinearAttenuation(const F32& atten)
{
    if (mLinearAtten != atten)
    {
        ++gGL.mLightHash;
        mLinearAtten = atten;
    }
}

void LLLightState::setQuadraticAttenuation(const F32& atten)
{
    if (mQuadraticAtten != atten)
    {
        ++gGL.mLightHash;
        mQuadraticAtten = atten;
    }
}

void LLLightState::setSpotExponent(const F32& exponent)
{
    if (mSpotExponent != exponent)
    {
        ++gGL.mLightHash;
        mSpotExponent = exponent;
    }
}

void LLLightState::setSpotCutoff(const F32& cutoff)
{
    if (mSpotCutoff != cutoff)
    {
        ++gGL.mLightHash;
        mSpotCutoff = cutoff;
    }
}

void LLLightState::setSpotDirection(const LLVector3& direction)
{
    //always set direction because modelview matrix may have changed
    ++gGL.mLightHash;

    //transform direction by current modelview matrix
    LLVector4a dir;
    dir.load3(direction.mV);
    LLVector4a eye;
    gGL.getModelviewMatrix().rotate(dir, eye);
    mSpotDirection.set(eye.getF32ptr());
}

LLRender::LLRender()
  : mDirty(false),
    mCount(0),
    mMode(LLRender::TRIANGLES),
    mCurrTextureUnitIndex(0)
{
    for (U32 i = 0; i < AL_NUM_TEXTURE_SLOTS; i++)
    {
        mTextureSlots[i].mIndex = i;
    }

    for (U32 i = 0; i < AL_NUM_IMAGE_SLOTS; ++i)
    {
        mImageSlots[i].mIndex = i;
    }

    for (U32 i = 0; i < LL_NUM_LIGHT_UNITS; ++i)
    {
        mLightState[i].mIndex = i;
    }

    for (U32 i = 0; i < 4; i++)
    {
        mCurrColorMask[i] = true;
    }

    mCurrBlendColorSFactor = BF_UNDEF;
    mCurrBlendAlphaSFactor = BF_UNDEF;
    mCurrBlendColorDFactor = BF_UNDEF;
    mCurrBlendAlphaDFactor = BF_UNDEF;

    mMatrixMode = LLRender::MM_MODELVIEW;

    for (U32 i = 0; i < NUM_MATRIX_MODES; ++i)
    {
        for (U32 j = 0; j < LL_MATRIX_STACK_DEPTH; ++j)
        {
            mMatrix[i][j].setIdentity();
        }
        mMatIdx[i] = 0;
        mMatHash[i] = 0;
        mCurMatHash[i] = 0xFFFFFFFF;
    }
    mCachedInvMdv.setIdentity();
    mCachedInvProj.setIdentity();
    mCachedMVP.setIdentity();

    mLightHash = 0;
}

LLRender::~LLRender()
{
    shutdown();
}

bool LLRender::init(bool needs_vertex_buffer)
{
    // Compatibility entry point for renderer-unit fixtures. The product calls
    // initMetal(), but both paths now establish only backend-neutral semantic
    // state; there is no hidden driver initialization or fallback context.
    resetCachedState();
    LLGLState::initClass();
    mSamplerCache.warmup();
    mCurrBlendColorSFactor = BF_SOURCE_ALPHA;
    mCurrBlendAlphaSFactor = BF_SOURCE_ALPHA;
    mCurrBlendColorDFactor = BF_ONE_MINUS_SOURCE_ALPHA;
    mCurrBlendAlphaDFactor = BF_ONE_MINUS_SOURCE_ALPHA;
    setAmbientLightColor(LLColor4::black);

    if (needs_vertex_buffer)
    {
        initVertexBuffer();
    }
    return true;
}

void LLRender::resetCachedState()
{
    // A fresh native renderer owns no live light or matrix binding.
    mLightsUBOBound   = false;
    mMatricesUBOBound = false;

    // Logical slot identities are renderer-local and must not survive a
    // backend restart. Unit zero remains the default semantic active slot.
    for (ALTextureSlot& slot : mTextureSlots)
    {
        slot.mCurrTexture = 0;
        slot.mCurrTexType = ALTextureSlot::TT_NONE;
        slot.mCurrSampler = 0;
        slot.mCurrImage = nullptr;
        slot.mCurrNativeTexture = nullptr;
    }
    mCurrTextureUnitIndex = 0;

    // The same for image units.
    for (ALImageSlot& slot : mImageSlots)
    {
        slot.reset();
    }

    // Start with all channels writable. Blend factors are marked unknown so
    // initialization establishes the requested alpha contract explicitly.
    for (bool& write : mCurrColorMask)
    {
        write = true;
    }
    mCurrBlendColorSFactor = BF_UNDEF;
    mCurrBlendAlphaSFactor = BF_UNDEF;
    mCurrBlendColorDFactor = BF_UNDEF;
    mCurrBlendAlphaDFactor = BF_UNDEF;

    // Established renderer defaults: unit line width, no polygon offset and
    // three control points per patch.
    mLineWidth           = 1.f;
    mPolygonOffsetFactor = 0.f;
    mPolygonOffsetUnits  = 0.f;
    mPatchVertices       = 3;

    // Target dimensions are unknown until the first native pass establishes
    // its viewport and scissor.
    mClearColor = LLColor4(0.f, 0.f, 0.f, 0.f);
    mScissor[0] = 0;
    mScissor[1] = 0;
    mScissor[2] = -1;
    mScissor[3] = -1;
    mViewport[0] = 0;
    mViewport[1] = 0;
    mViewport[2] = -1;
    mViewport[3] = -1;

    // Retain the established numeric vocabulary while native encoders consume
    // these values as explicit pipeline descriptions.
    mPolygonMode        = GL_FILL;
    mCullFace           = GL_BACK;
    mPointSize          = 1.f;
    mStencilFunc        = GL_ALWAYS;
    mStencilRef         = 0;
    mStencilReadMask    = ~0u;
    mStencilFail        = GL_KEEP;
    mStencilDepthFail   = GL_KEEP;
    mStencilPass        = GL_KEEP;
    mStencilWriteMask   = ~0u;
    mBlendEquationRGB   = GL_FUNC_ADD;
    mBlendEquationAlpha = GL_FUNC_ADD;
}

#if LL_DARWIN
bool LLRender::initMetal(bool needs_vertex_buffer)
{
    if (!mMetalDevice || !mMetalFrames)
    {
        LL_WARNS("Metal") << "Cannot initialize LLRender without the window-owned Metal backend"
                           << LL_ENDL;
        return false;
    }
    if (needs_vertex_buffer && !LLVertexBuffer::isMetalBufferAuthority())
    {
        LL_WARNS("Metal") << "Cannot initialize the native immediate buffer before Metal buffer authority"
                           << LL_ENDL;
        return false;
    }

    resetCachedState();
    LLGLState::initClass();
    mSamplerCache.warmup();

    mCurrBlendColorSFactor = BF_SOURCE_ALPHA;
    mCurrBlendAlphaSFactor = BF_SOURCE_ALPHA;
    mCurrBlendColorDFactor = BF_ONE_MINUS_SOURCE_ALPHA;
    mCurrBlendAlphaDFactor = BF_ONE_MINUS_SOURCE_ALPHA;
    setAmbientLightColor(LLColor4::black);

    if (needs_vertex_buffer)
    {
        initVertexBuffer();
    }
    LL_INFOS("Metal") << "Native LLRender state initialized"
                       << (needs_vertex_buffer ? " with immediate buffer" : "")
                       << LL_ENDL;
    return true;
}
#endif

void LLRender::initVertexBuffer()
{
    llassert_always(mBuffer.isNull());
    mBuffer = new LLVertexBuffer(immediate_mask);
    mBuffer->allocateBuffer(4096, 0);
    mBuffer->getVertexStrider(mVerticesp);
    mBuffer->getTexCoord0Strider(mTexcoordsp);
    mBuffer->getColorStrider(mColorsp);
}

void LLRender::resetVertexBuffer()
{
    mBuffer = nullptr;
    mBufferDataList = nullptr;
    mVBCache.clear();
}

void LLRender::shutdown()
{
    // gGL is thread_local, so this runs once per thread that touched it. Native
    // caches and buffers are therefore members rather than process statics.
#if LL_DARWIN
    if (mMetalUICaptureRenderer)
    {
        endMetalUITextureCapture(false);
    }
    releaseMetalUITextureCapturePixels();
    if (mMetalUICaptureRendererCreations != 0)
    {
        ALMetalUIRendererStats capture_stats{};
        for (const auto& cached : mMetalUICaptureCache)
        {
            if (!cached)
            {
                continue;
            }
            const ALMetalUIRendererStats stats = cached->stats();
            capture_stats.frames += stats.frames;
            capture_stats.batches += stats.batches;
            capture_stats.vertices += stats.vertices;
            capture_stats.pipeline_states += stats.pipeline_states;
            capture_stats.sampler_states += stats.sampler_states;
        }
        LL_INFOS("Metal")
            << "Offscreen native UI capture cache created "
            << mMetalUICaptureRendererCreations << " renderer(s), reused "
            << mMetalUICaptureRendererReuses << " renderer(s), and encoded "
            << capture_stats.frames << " frames / " << capture_stats.batches
            << " batches while retaining " << capture_stats.pipeline_states
            << " pipeline states and " << capture_stats.sampler_states
            << " sampler states" << LL_ENDL;
    }
    for (auto& cached : mMetalUICaptureCache)
    {
        cached.reset();
    }
    mMetalUICaptureRendererCreations = 0;
    mMetalUICaptureRendererReuses = 0;
    mMetalUICaptureCacheIndex = METAL_UI_CAPTURE_CACHE_SIZE;
    if (mMetalUIRenderer)
    {
        std::string error;
        if (mMetalUIRenderer->hasOpenFrame() && !mMetalUIRenderer->endFrame(&error))
        {
            LL_WARNS("Metal") << "Could not finish the native UI frame during shutdown: "
                                << error << LL_ENDL;
        }
        const ALMetalUIRendererStats stats = mMetalUIRenderer->stats();
        LL_INFOS("Metal") << "Native production UI session encoded "
                           << stats.frames << " frames, " << stats.batches
                           << " batches and " << stats.vertices << " vertices; cache holds "
                           << stats.pipeline_states << " pipeline states and "
                           << stats.sampler_states << " sampler states; "
                           << mMetalUIFallbackBatches << " of "
                           << mMetalUIAttemptedBatches
                           << " eligible UI batches were rejected" << LL_ENDL;
        LL_INFOS("Metal") << "Native UI rejection breakdown: incomplete="
                           << sMetalUIFallbackReasons[static_cast<std::size_t>(
                                  MetalUIFallbackReason::incomplete_batch)]
                           << ", shader="
                           << sMetalUIFallbackReasons[static_cast<std::size_t>(
                                  MetalUIFallbackReason::shader)]
                           << ", primitive="
                           << sMetalUIFallbackReasons[static_cast<std::size_t>(
                                  MetalUIFallbackReason::primitive)]
                           << ", mirror="
                           << sMetalUIFallbackReasons[static_cast<std::size_t>(
                                  MetalUIFallbackReason::mirror)]
                           << ", viewport="
                           << sMetalUIFallbackReasons[static_cast<std::size_t>(
                                  MetalUIFallbackReason::viewport)]
                           << ", encoder="
                           << sMetalUIFallbackReasons[static_cast<std::size_t>(
                                  MetalUIFallbackReason::encoder)] << LL_ENDL;
        mMetalUIRenderer.reset();
    }
#endif

    clearSamplers();

    resetVertexBuffer();

    // Drop the shared light block with the context that owns it. Forcing a re-pack (rather
    // than just clearing mLightsUBOBound) means a restarted context can never rebind a
    // buffer name from the dead one.
    mLightsUBO.release();
    mLightsUBOHash  = 0xFFFFFFFFu;
    mLightsUBOBound = false;

    // Same for the matrix block. syncMatrices re-initialises the shadow on the next use
    // because release() leaves it unallocated.
    mMatricesUBO.release();
    for (U32 i = 0; i < NUM_MATRIX_MODES; ++i)
    {
        mMatricesUBOHash[i] = 0xFFFFFFFFu;
    }
    mMatricesUBOBound = false;

    // Its queries and fences, which it deletes only while the context is up.
    mQueries.release();
    mFrameFences.release();
}

#if LL_DARWIN
bool LLRender::reloadMetalShaders(std::string* error)
{
    if (mMetalUICaptureRenderer ||
        (mMetalUIRenderer && mMetalUIRenderer->hasOpenFrame()))
    {
        if (error) *error = "A native UI transaction is active during shader reload";
        return false;
    }
    for (const auto& renderer : mMetalUICaptureCache)
    {
        if (renderer && renderer->hasOpenFrame())
        {
            if (error) *error = "A cached native UI transaction is still active";
            return false;
        }
    }
    if (mMetalUIRenderer && !mMetalUIRenderer->reloadShaders(error)) return false;
    for (const auto& renderer : mMetalUICaptureCache)
    {
        if (renderer && !renderer->reloadShaders(error)) return false;
    }
    return true;
}

void LLRender::setMetalBackend(ALMetalDevice* device, ALMetalFrameScheduler* scheduler,
                               ALMetalSurface* surface)
{
    if (mMetalUICaptureRenderer)
    {
        endMetalUITextureCapture(false);
    }
    releaseMetalUITextureCapturePixels();
    if (mMetalUIRenderer)
    {
        std::string error;
        if (mMetalUIRenderer->hasOpenFrame() && !mMetalUIRenderer->endFrame(&error))
        {
            LL_WARNS("Metal") << "Could not finish the native UI frame while changing surfaces: "
                                << error << LL_ENDL;
        }
    }
    for (auto& cached : mMetalUICaptureCache)
    {
        cached.reset();
    }
    mMetalUIRenderer.reset();
    mFrameFences.setMetalScheduler(nullptr);
    ALGPUBuffer::setMetalDevice(nullptr);
    mMetalDevice = nullptr;
    mMetalFrames = nullptr;
    mMetalUISurface = nullptr;
    mMetalUILoggedFrame = 0;
    mMetalUIFrameAttemptedBatches = 0;
    mMetalUIFrameFallbackBatches = 0;
    mMetalUICaptureRendererCreations = 0;
    mMetalUICaptureRendererReuses = 0;
    mMetalUICaptureCacheIndex = METAL_UI_CAPTURE_CACHE_SIZE;
    mMetalUICaptureUsesDepth = false;
    mMetalUICaptureDepthWidth = 0;
    mMetalUICaptureDepthHeight = 0;
    mMetalUICaptureFirstFailure.clear();
    mMetalUIReplacementEnabled = true;
    mMetalUISurfaceFrameSubmitted = false;
    if (!device || !scheduler)
    {
        return;
    }

    mMetalDevice = device;
    mMetalFrames = scheduler;
    ALGPUBuffer::setMetalDevice(device);
    mFrameFences.setMetalScheduler(scheduler);
    // Hidden windows intentionally have no presentation surface. Publishing
    // their device and scheduler is still sufficient for texture captures.
    if (!surface)
    {
        LL_INFOS("Metal") << "Native offscreen capture backend initialized"
                           << LL_ENDL;
        return;
    }
    std::string error;
    mMetalUIRenderer = ALMetalUIRenderer::createForSurface(
        *device, *scheduler, *surface, &error);
    if (!mMetalUIRenderer)
    {
        LL_WARNS("Metal") << "Native production UI renderer initialization failed: "
                           << error << LL_ENDL;
    }
    else
    {
        mMetalUISurface = surface;
        LL_INFOS("Metal") << "Native production UI drawable renderer initialized at "
                           << surface->info().width << "x" << surface->info().height
                           << LL_ENDL;
    }
}

bool LLRender::beginMetalUITextureCapture(
    ALMetalPixelFormat format,
    LLGLSLShader* alpha_mask_shader,
    const ALMetalTexture* background,
    const ALMetalTexture* depth,
    LLGLSLShader* uniform_color_shader,
    EMetalUICaptureMode mode,
    EMetalUICaptureStorage storage,
    std::unique_ptr<ALMetalTexture>* reusable_output)
{
    const bool authoritative =
        mode == EMetalUICaptureMode::NATIVE_AUTHORITATIVE;
    if (authoritative && !mMetalUICaptureRenderer)
    {
        // Do not let geometry queued for the live framebuffer cross into the
        // native-only transaction when the caller changes logical state.
        flush();
    }
    if (!mMetalDevice || !mMetalFrames || !alpha_mask_shader ||
        mMetalUICaptureRenderer)
    {
        return false;
    }
    mMetalUICapturePixel.reset();
    mMetalUICaptureUsesDepth = false;
    mMetalUICaptureDepthWidth = 0;
    mMetalUICaptureDepthHeight = 0;
    const bool linear_storage =
        storage == EMetalUICaptureStorage::LINEAR_STORAGE &&
        alMetalLinearFormat(format) != format;
    const U32 cache_index =
        metal_ui_capture_cache_index(format, linear_storage);
    if (cache_index >= mMetalUICaptureCache.size())
    {
        return false;
    }
    std::string error;
    if (mMetalUICaptureCache[cache_index])
    {
        mMetalUICaptureRenderer =
            std::move(mMetalUICaptureCache[cache_index]);
        ++mMetalUICaptureRendererReuses;
    }
    else
    {
        mMetalUICaptureRenderer = ALMetalUIRenderer::createForTexture(
            *mMetalDevice, *mMetalFrames, format, &error, linear_storage);
        if (mMetalUICaptureRenderer)
        {
            ++mMetalUICaptureRendererCreations;
        }
    }
    if (!mMetalUICaptureRenderer)
    {
        LL_DEBUGS("Metal") << "Could not begin an offscreen immediate-mode capture: "
                            << error << LL_ENDL;
        return false;
    }
    mMetalUICaptureCacheIndex = cache_index;
    if ((background && !mMetalUICaptureRenderer->setFrameBackground(
                           background, &error)) ||
        (depth && !mMetalUICaptureRenderer->setFrameDepth(depth, &error)))
    {
        LL_DEBUGS("Metal")
            << "Could not retain the offscreen composition backing: "
            << error << LL_ENDL;
        mMetalUICaptureRenderer.reset();
        mMetalUICaptureCacheIndex = METAL_UI_CAPTURE_CACHE_SIZE;
        return false;
    }
    if (reusable_output &&
        !mMetalUICaptureRenderer->setReusableOutputTexture(
            *reusable_output, &error))
    {
        LL_DEBUGS("Metal")
            << "Could not retain the preceding offscreen color backing: "
            << error << LL_ENDL;
        mMetalUICaptureRenderer.reset();
        mMetalUICaptureCacheIndex = METAL_UI_CAPTURE_CACHE_SIZE;
        return false;
    }
    mMetalUICaptureBatches = 0;
    mMetalUICaptureAlphaShader = alpha_mask_shader;
    mMetalUICaptureUniformColorShader = uniform_color_shader;
    mMetalUICaptureMinimumAlpha = alpha_mask_shader->getMinimumAlpha();
    mMetalUICaptureFirstFailure.clear();
    mMetalUICaptureComplete = true;
    mMetalUICaptureUsesDepth = depth != nullptr;
    if (depth)
    {
        mMetalUICaptureDepthWidth = depth->descriptor().width;
        mMetalUICaptureDepthHeight = depth->descriptor().height;
    }
    if (authoritative)
    {
        MetalUICaptureSavedState& saved = mMetalUICaptureSavedState;
        saved.shader = LLGLSLShader::sCurBoundShader;
        saved.shader_ptr = LLGLSLShader::sCurBoundShaderPtr;
        const ALTextureSlot& slot = mTextureSlots[0];
        saved.texture = slot.mCurrTexture;
        saved.texture_type = slot.mCurrTexType;
        saved.image = slot.mCurrImage;
        saved.native_texture = slot.mCurrNativeTexture;
        saved.sampler = slot.mCurrSampler;
        saved.blend_color_source = mCurrBlendColorSFactor;
        saved.blend_color_destination = mCurrBlendColorDFactor;
        saved.blend_alpha_source = mCurrBlendAlphaSFactor;
        saved.blend_alpha_destination = mCurrBlendAlphaDFactor;
        std::copy(std::begin(mCurrColorMask), std::end(mCurrColorMask),
                  std::begin(saved.color_mask));
        std::copy(std::begin(mViewport), std::end(mViewport),
                  std::begin(saved.viewport));
        if (mBuffer.notNull())
        {
            saved.color = mColorsp[0];
            saved.texcoord = mTexcoordsp[0];
        }
        saved.matrix_mode = mMatrixMode;
        saved.primitive_mode = mMode;
        saved.valid = true;

        mMetalUICaptureSampler =
            ALSamplerCache::makeDesc(ALSamplers::BilinearClamp);
        mMetalUICaptureTextureBound = false;
        mMetalUICaptureAuthoritative = true;
    }
    return true;
}

bool LLRender::bindMetalUITextureCapture(
    S32 slot_index, LLImageGL* image, ALSampler sampler)
{
    if (!mMetalUICaptureAuthoritative || !mMetalUICaptureRenderer)
    {
        return false;
    }
    if (slot_index != 0 || !image)
    {
        invalidateMetalUITextureCapture(
            "authoritative texture bind had no supported slot-zero image");
        return false;
    }

    // Drain the preceding batch under its old image/sampler before replacing
    // the logical binding consumed by drawMetalUIBuffer().
    flush();
    ALTextureSlot& slot = mTextureSlots[0];
    slot.mCurrTexture = 1; // logical native binding; never handed to GL
    slot.mCurrTexType = ALTextureSlot::TT_TEXTURE;
    slot.mCurrImage = image;
    slot.mCurrNativeTexture = nullptr;
    mMetalUICaptureSampler = ALSamplerCache::makeDesc(sampler);
    mMetalUICaptureTextureBound = true;
    return image->getMetalTexture() != nullptr;
}

bool LLRender::unbindMetalUITextureCapture(S32 slot_index)
{
    if (!mMetalUICaptureAuthoritative || !mMetalUICaptureRenderer ||
        slot_index != 0)
    {
        return false;
    }

    flush();
    ALTextureSlot& slot = mTextureSlots[0];
    slot.mCurrTexture = 0;
    slot.mCurrTexType = ALTextureSlot::TT_NONE;
    slot.mCurrImage = nullptr;
    slot.mCurrNativeTexture = nullptr;
    mMetalUICaptureTextureBound = false;
    return true;
}

void LLRender::setMetalUITextureCaptureMinimumAlpha(
    const LLGLSLShader* shader, F32 minimum)
{
    if (!mMetalUICaptureAuthoritative || !mMetalUICaptureRenderer)
    {
        return;
    }
    if (shader != mMetalUICaptureAlphaShader)
    {
        invalidateMetalUITextureCapture(
            "minimum-alpha state targeted an unsupported shader");
        return;
    }
    if (mMetalUICaptureMinimumAlpha == minimum)
    {
        return;
    }
    // Complete preceding geometry under the old threshold before changing
    // the capture-local state used by the next batch.
    flush();
    mMetalUICaptureMinimumAlpha = minimum;
}

bool LLRender::updateMetalUITextureCaptureDepth(
    S32 x, S32 y, S32 width, S32 height, bool restore, F32 depth)
{
    if (!mMetalUICaptureRenderer)
    {
        return false;
    }
    const auto reject = [this](const std::string& reason)
    {
        if (mMetalUICaptureFirstFailure.empty())
        {
            mMetalUICaptureFirstFailure =
                "retained-depth bracket: " + reason;
        }
        mMetalUICaptureComplete = false;
        LL_DEBUGS("Metal") << "Offscreen immediate-mode depth bracket failed: "
                            << reason << LL_ENDL;
        return false;
    };
    if (!mMetalUICaptureUsesDepth || mMetalUICaptureDepthWidth == 0 ||
        mMetalUICaptureDepthHeight == 0)
    {
        return reject("capture has no retained depth backing");
    }
    if (width <= 0 || height <= 0)
    {
        return reject("depth rectangle has non-positive area");
    }

    const std::int64_t target_width = mMetalUICaptureDepthWidth;
    const std::int64_t target_height = mMetalUICaptureDepthHeight;
    const std::int64_t source_left = x;
    const std::int64_t source_bottom = y;
    const std::int64_t source_right = source_left + width;
    const std::int64_t source_top = source_bottom + height;
    const std::int64_t left = std::clamp<std::int64_t>(
        source_left, 0, target_width);
    const std::int64_t right = std::clamp<std::int64_t>(
        source_right, 0, target_width);
    const std::int64_t bottom = std::clamp<std::int64_t>(
        source_bottom, 0, target_height);
    const std::int64_t top = std::clamp<std::int64_t>(
        source_top, 0, target_height);
    if (left >= right || bottom >= top)
    {
        // GL accepts an out-of-target scissor and affects no pixels.
        return true;
    }

    const ALMetalScissorRect rect{
        static_cast<U32>(left),
        static_cast<U32>(target_height - top),
        static_cast<U32>(right - left),
        static_cast<U32>(top - bottom),
    };
    std::string error;
    const bool encoded = restore
        ? mMetalUICaptureRenderer->restoreFrameDepthRects(
              &rect, 1,
              metal_compare_function(LLGLDepthTest::remap(GL_LESS)),
              &error)
        : mMetalUICaptureRenderer->clearFrameDepthRects(&rect, 1, depth, &error);
    return encoded ? true : reject(error);
}

bool LLRender::clearMetalUITextureCaptureDepth(
    S32 x, S32 y, S32 width, S32 height, F32 depth)
{
    return updateMetalUITextureCaptureDepth(
        x, y, width, height, false, depth);
}

bool LLRender::restoreMetalUITextureCaptureDepth(
    S32 x, S32 y, S32 width, S32 height)
{
    return updateMetalUITextureCaptureDepth(
        x, y, width, height, true, 0.f);
}

bool LLRender::drawMetalHUDIndexedBatch(
    const ALMetalHUDIndexedBatch& batch, ALSampler sampler_key)
{
    if (!mMetalUICaptureRenderer)
    {
        return false;
    }
    ALMetalHUDIndexedBatch resolved = batch;
    const ALSamplerDesc sampler = ALSamplerCache::makeDesc(sampler_key);
    resolved.sampler = metal_sampler_descriptor(sampler);
    resolved.blend.enabled = LLGLState::isEnabled(GL_BLEND);
    resolved.blend.source_rgb = metal_blend_factor(
        mCurrBlendColorSFactor == BF_UNDEF
            ? BF_SOURCE_ALPHA : mCurrBlendColorSFactor);
    resolved.blend.destination_rgb = metal_blend_factor(
        mCurrBlendColorDFactor == BF_UNDEF
            ? BF_ONE_MINUS_SOURCE_ALPHA : mCurrBlendColorDFactor);
    resolved.blend.source_alpha = metal_alpha_blend_factor(
        mCurrBlendAlphaSFactor == BF_UNDEF
            ? BF_SOURCE_ALPHA : mCurrBlendAlphaSFactor);
    resolved.blend.destination_alpha = metal_alpha_blend_factor(
        mCurrBlendAlphaDFactor == BF_UNDEF
            ? BF_ONE_MINUS_SOURCE_ALPHA : mCurrBlendAlphaDFactor);
    resolved.blend.rgb_operation = metal_blend_operation(mBlendEquationRGB);
    resolved.blend.alpha_operation = metal_blend_operation(mBlendEquationAlpha);
    U8 write_mask = 0;
    write_mask |= mCurrColorMask[0]
        ? static_cast<U8>(ALMetalColorWriteMask::red) : 0;
    write_mask |= mCurrColorMask[1]
        ? static_cast<U8>(ALMetalColorWriteMask::green) : 0;
    write_mask |= mCurrColorMask[2]
        ? static_cast<U8>(ALMetalColorWriteMask::blue) : 0;
    write_mask |= mCurrColorMask[3]
        ? static_cast<U8>(ALMetalColorWriteMask::alpha) : 0;
    resolved.blend.write_mask =
        static_cast<ALMetalColorWriteMask>(write_mask);

    const S32 viewport_width = mViewport[2];
    const S32 viewport_height = mViewport[3];
    if (viewport_width <= 0 || viewport_height <= 0)
    {
        if (mMetalUICaptureFirstFailure.empty())
        {
            mMetalUICaptureFirstFailure =
                "HUD attachment used an invalid viewport";
        }
        mMetalUICaptureComplete = false;
        return false;
    }
    resolved.target_width = mMetalUICaptureUsesDepth
        ? mMetalUICaptureDepthWidth
        : static_cast<U32>(
              llmax(viewport_width, mViewport[0] + viewport_width));
    resolved.target_height = mMetalUICaptureUsesDepth
        ? mMetalUICaptureDepthHeight
        : static_cast<U32>(
              llmax(viewport_height, mViewport[1] + viewport_height));
    resolved.viewport = {
        static_cast<double>(mViewport[0]),
        // Offscreen UI vertices are Y-flipped in the Metal shader so the
        // resulting texture keeps GL's lower-left row convention. Keep the
        // GL viewport origin in that same target space; applying Metal's
        // upper-left conversion here would move bounded HUD draws twice.
        static_cast<double>(mViewport[1]),
        static_cast<double>(viewport_width),
        static_cast<double>(viewport_height),
        0.0,
        1.0,
    };
    if (LLGLState::isEnabled(GL_SCISSOR_TEST) &&
        mScissor[2] >= 0 && mScissor[3] >= 0)
    {
        const S32 left = llclamp(
            mScissor[0], 0, static_cast<S32>(resolved.target_width));
        const S32 right = llclamp(
            mScissor[0] + mScissor[2], 0,
            static_cast<S32>(resolved.target_width));
        const S32 bottom = llclamp(
            mScissor[1], 0, static_cast<S32>(resolved.target_height));
        const S32 top = llclamp(
            mScissor[1] + mScissor[3], 0,
            static_cast<S32>(resolved.target_height));
        resolved.scissor = {
            static_cast<U32>(left),
            // The offscreen vertex flip already maps a GL top-edge draw to
            // the texture's lower rows. Its GL scissor must follow it there.
            static_cast<U32>(bottom),
            static_cast<U32>(llmax(0, right - left)),
            static_cast<U32>(llmax(0, top - bottom)),
        };
    }
    else
    {
        resolved.scissor = {
            0, 0, resolved.target_width, resolved.target_height};
    }
    if (mMetalUICaptureUsesDepth)
    {
        resolved.depth.enabled = LLGLDepthTest::depthEnabled();
        resolved.depth.write_enabled = resolved.depth.enabled &&
            LLGLDepthTest::depthWriteEnabled();
        resolved.depth.reverse_z = sReverseZ;
        resolved.depth.compare = metal_compare_function(
            LLGLDepthTest::remap(LLGLDepthTest::depthFunction()));
    }
    std::string error;
    if (!mMetalUICaptureRenderer->drawHUDIndexed(resolved, &error))
    {
        if (mMetalUICaptureFirstFailure.empty())
        {
            mMetalUICaptureFirstFailure =
                "HUD attachment encoder: " + error;
        }
        mMetalUICaptureComplete = false;
        LL_DEBUGS("Metal") << "Offscreen HUD attachment capture failed: "
                            << error << LL_ENDL;
        return false;
    }
    ++mMetalUICaptureBatches;
    return true;
}

bool LLRender::hasMetalUITextureCapture() const noexcept
{
    return mMetalUICaptureRenderer != nullptr && mMetalUICaptureComplete;
}

bool LLRender::requestMetalUITextureCapturePixel(S32 x, S32 y)
{
    if (!mMetalDevice || !mMetalUICaptureRenderer ||
        !mMetalUICaptureComplete || mMetalUICapturePixel || x < 0 || y < 0)
    {
        return false;
    }
    if (!mMetalUICapturePixelReads)
    {
        auto reads = std::make_unique<ALMetalTextureReadbackRing>();
        std::string error;
        if (!reads->allocate(
                *mMetalDevice, 3, "Alchemy ordered UI pixel readback", &error))
        {
            LL_WARNS("Metal")
                << "Could not allocate the native UI pixel readback: "
                << error << LL_ENDL;
            return false;
        }
        mMetalUICapturePixelReads = std::move(reads);
    }
    if (mMetalUICapturePixelReads->pending() >= 3)
    {
        return false;
    }

    // Materialize any immediate-mode vertices that precede the sample before
    // the native renderer splits its pass at this exact ordered boundary.
    flush();
    if (!mMetalUICaptureComplete)
    {
        return false;
    }
    std::string error;
    mMetalUICapturePixel =
        mMetalUICaptureRenderer->copyFrameColorPixel(
            static_cast<U32>(x), static_cast<U32>(y), &error);
    if (!mMetalUICapturePixel)
    {
        LL_DEBUGS("Metal")
            << "Native ordered UI pixel snapshot was skipped: "
            << error << LL_ENDL;
        return false;
    }
    mMetalUICapturePixelAt = {x, y};
    return true;
}

bool LLRender::collectMetalUITextureCapturePixel(
    S32& x, S32& y, LLColor4U& color)
{
    if (!mMetalUICapturePixelReads ||
        mMetalUICapturePixelReadAt.empty())
    {
        return false;
    }
    U8 bgra[4]{};
    std::string error;
    if (!mMetalUICapturePixelReads->collect(
            bgra, sizeof(bgra), false, &error))
    {
        if (!error.empty())
        {
            LL_WARNS("Metal")
                << "Could not collect the native UI pixel readback: "
                << error << LL_ENDL;
        }
        return false;
    }
    const auto at = mMetalUICapturePixelReadAt.front();
    mMetalUICapturePixelReadAt.pop_front();
    x = at.first;
    y = at.second;
    color.set(bgra[2], bgra[1], bgra[0], bgra[3]);
    return true;
}

bool LLRender::readMetalUITextureCaptureRGBA(
    S32 x, S32 y, S32 width, S32 height,
    U8* rgba, size_t bytes_per_row)
{
    if (!mMetalDevice || !mMetalUICaptureRenderer ||
        !mMetalUICaptureComplete || !rgba || x < 0 || y < 0 ||
        width <= 0 || height <= 0)
    {
        return false;
    }

    // Materialize every immediate draw before splitting this exceptional
    // synchronous boundary. The native renderer submits, reads and retains
    // that exact output as the next offscreen frame's background.
    flush();
    if (!mMetalUICaptureComplete)
    {
        return false;
    }
    std::string error;
    const bool complete = mMetalUICaptureRenderer->readFrameColorRGBA8(
        {static_cast<U32>(x), static_cast<U32>(y),
         static_cast<U32>(width), static_cast<U32>(height)},
        rgba, bytes_per_row, &error);
    if (!complete)
    {
        if (mMetalUICaptureFirstFailure.empty())
        {
            mMetalUICaptureFirstFailure =
                "ordered RGBA readback: " + error;
        }
        mMetalUICaptureComplete = false;
        LL_WARNS("Metal")
            << "Native ordered RGBA readback failed closed: "
            << error << LL_ENDL;
    }
    return complete;
}

void LLRender::releaseMetalUITextureCapturePixels()
{
    mMetalUICapturePixel.reset();
    mMetalUICapturePixelReadAt.clear();
    if (mMetalUICapturePixelReads)
    {
        mMetalUICapturePixelReads->release();
        mMetalUICapturePixelReads.reset();
    }
}

bool LLRender::drawMetalSelectionIDs(
    const ALMetalSelectionIDScene& scene)
{
    if (!mMetalUICaptureRenderer || !scene.batches ||
        scene.batch_count == 0)
    {
        return false;
    }

    std::vector<ALMetalSelectionIDBatch> batches(
        scene.batches, scene.batches + scene.batch_count);
    const ALSamplerDesc source_sampler =
        ALSamplerCache::makeDesc(ALSamplers::AnisoWrap);
    const ALMetalSamplerDescriptor sampler =
        metal_sampler_descriptor(source_sampler);
    for (ALMetalSelectionIDBatch& batch : batches)
    {
        batch.sampler = sampler;
    }
    ALMetalSelectionIDScene resolved = scene;
    resolved.batches = batches.data();

    std::string error;
    if (!mMetalUICaptureRenderer->drawSelectionIDs(resolved, &error))
    {
        if (mMetalUICaptureFirstFailure.empty())
        {
            mMetalUICaptureFirstFailure = "selection-id encoder: " + error;
        }
        mMetalUICaptureComplete = false;
        LL_DEBUGS("Metal")
            << "Offscreen selection-id capture failed: "
            << error << LL_ENDL;
        return false;
    }
    mMetalUICaptureBatches += scene.batch_count;
    return true;
}

bool LLRender::drawMetalSelectionContour(
    const ALMetalSelectionContourScene& scene)
{
    if (!mMetalUICaptureRenderer || !scene.palette_rgba8 ||
        scene.palette_bytes == 0)
    {
        return false;
    }

    std::vector<ALMetalSelectionIDBatch> wire_batches;
    ALMetalSelectionContourScene         resolved = scene;
    if (scene.wire_batch_count != 0)
    {
        if (!scene.wire_batches)
        {
            return false;
        }
        wire_batches.assign(scene.wire_batches, scene.wire_batches + scene.wire_batch_count);
        const ALSamplerDesc            source_sampler = ALSamplerCache::makeDesc(ALSamplers::AnisoWrap);
        const ALMetalSamplerDescriptor sampler        = metal_sampler_descriptor(source_sampler);
        for (ALMetalSelectionIDBatch& batch : wire_batches)
        {
            batch.sampler = sampler;
        }
        resolved.wire_batches = wire_batches.data();
    }

    std::string error;
    if (!mMetalUICaptureRenderer->drawSelectionContour(resolved, &error))
    {
        if (mMetalUICaptureFirstFailure.empty())
        {
            mMetalUICaptureFirstFailure =
                "selection-contour encoder: " + error;
        }
        mMetalUICaptureComplete = false;
        LL_DEBUGS("Metal") << "Offscreen selection-contour capture failed: " << error << LL_ENDL;
        return false;
    }
    mMetalUICaptureBatches += (scene.contour_enabled || scene.glow_radius != 0u ? 3u : 0u) + (scene.glow_radius != 0u ? 2u : 0u) +
                              (scene.contrast_shift && (scene.contour_enabled || scene.wire_batch_count != 0u) ? 1u : 0u) +
                              scene.wire_batch_count;
    if (!scene.wire_hud)
    {
        for (std::size_t index = 0; index < scene.wire_batch_count; ++index)
        {
            mMetalUICaptureBatches +=
                scene.wire_batches[index].show_hidden ? 1u : 0u;
        }
    }
    return true;
}

void LLRender::invalidateMetalUITextureCapture(const char* reason)
{
    if (!mMetalUICaptureRenderer || !mMetalUICaptureComplete)
    {
        return;
    }
    if (mMetalUICaptureFirstFailure.empty())
    {
        mMetalUICaptureFirstFailure = reason
            ? reason : "unsupported draw family";
    }
    mMetalUICaptureComplete = false;
    LL_DEBUGS("Metal")
        << "Offscreen UI capture invalidated by "
        << (reason ? reason : "an unsupported draw family")
        << LL_ENDL;
}

std::unique_ptr<ALMetalTexture> LLRender::endMetalUITextureCapture(
    bool source_success, bool present_surface, bool* presented,
    std::string* failure, EMetalUICaptureEndState* end_state)
{
    if (end_state)
    {
        *end_state = EMetalUICaptureEndState::INACTIVE;
    }
    if (failure)
    {
        failure->clear();
    }
    if (presented)
    {
        *presented = false;
    }
    if (!mMetalUICaptureRenderer)
    {
        if (failure)
        {
            *failure = "offscreen UI capture was not active";
        }
        return nullptr;
    }

    std::string error;
    // A retained-depth composition starts from an exact world color/depth
    // backing. Snapshot deliberately hides Interface and HUDs by default, so
    // its ordered overlay stream may be empty; that is a complete world-only
    // result, not a missing source. Color-only texture producers still need
    // at least one captured batch and therefore keep their old fail-closed
    // contract.
    const bool exact = source_success && mMetalUICaptureComplete &&
        (mMetalUICaptureBatches != 0 || mMetalUICaptureUsesDepth);
    bool submitted = false;
    bool surface_submitted = false;
    if (exact && present_surface && mMetalUISurface)
    {
        submitted = mMetalUICaptureRenderer->endFrameAndPresent(
            *mMetalUISurface, &error);
        surface_submitted = submitted;
        if (surface_submitted)
        {
            mMetalUISurfaceFrameSubmitted = true;
        }
    }
    else
    {
        submitted = mMetalUICaptureRenderer->endFrame(&error);
    }
    if (presented)
    {
        *presented = surface_submitted;
    }
    const bool complete = exact && submitted &&
                          (!present_surface || surface_submitted);
    std::unique_ptr<ALMetalTexture> output;
    if (complete)
    {
        output = mMetalUICaptureRenderer->takeOutputTexture();
        if (mMetalUICapturePixel && mMetalUICapturePixelReads)
        {
            std::string readback_error;
            if (mMetalUICapturePixelReads->request(
                    *mMetalUICapturePixel, 0, 0, {}, &readback_error))
            {
                mMetalUICapturePixelReadAt.push_back(
                    mMetalUICapturePixelAt);
            }
            else if (!readback_error.empty())
            {
                LL_WARNS("Metal")
                    << "Could not submit the native UI pixel readback: "
                    << readback_error << LL_ENDL;
            }
        }
    }
    else if (!error.empty())
    {
        LL_DEBUGS("Metal") << "Discarded an offscreen immediate-mode capture: "
                            << error << LL_ENDL;
    }
    if (!complete && failure)
    {
        if (!source_success)
        {
            *failure = "the source transaction was incomplete";
        }
        else if (!mMetalUICaptureComplete)
        {
            *failure = mMetalUICaptureFirstFailure.empty()
                ? "an unsupported UI draw invalidated the capture"
                : mMetalUICaptureFirstFailure;
        }
        else if (mMetalUICaptureBatches == 0 && !mMetalUICaptureUsesDepth)
        {
            *failure = "the UI stream encoded no batches";
        }
        else if (!error.empty())
        {
            *failure = "the Metal command submission failed: " + error;
        }
        else if (present_surface && !surface_submitted)
        {
            *failure = "the fused drawable submission did not complete";
        }
        else
        {
            *failure = "the offscreen UI transaction did not complete";
        }
    }
    if (end_state)
    {
        if (complete)
        {
            *end_state = EMetalUICaptureEndState::COMPLETE;
        }
        else if (!source_success)
        {
            *end_state = EMetalUICaptureEndState::SOURCE_INCOMPLETE;
        }
        else if (!mMetalUICaptureComplete)
        {
            *end_state = EMetalUICaptureEndState::CAPTURE_INCOMPLETE;
        }
        else if (mMetalUICaptureBatches == 0 && !mMetalUICaptureUsesDepth)
        {
            *end_state = EMetalUICaptureEndState::EMPTY;
        }
        else if (!error.empty())
        {
            *end_state = EMetalUICaptureEndState::COMMAND_SUBMISSION_FAILED;
        }
        else if (present_surface && !surface_submitted)
        {
            *end_state = EMetalUICaptureEndState::SURFACE_SUBMISSION_FAILED;
        }
        else
        {
            *end_state = EMetalUICaptureEndState::CAPTURE_INCOMPLETE;
        }
    }
    const U32 cache_index = mMetalUICaptureCacheIndex;
    mMetalUICaptureCacheIndex = METAL_UI_CAPTURE_CACHE_SIZE;
    if (submitted && cache_index < mMetalUICaptureCache.size() &&
        !mMetalUICaptureCache[cache_index])
    {
        mMetalUICaptureCache[cache_index] =
            std::move(mMetalUICaptureRenderer);
    }
    else
    {
        mMetalUICaptureRenderer.reset();
    }
    mMetalUICaptureBatches = 0;
    mMetalUICaptureAlphaShader = nullptr;
    mMetalUICaptureUniformColorShader = nullptr;
    mMetalUICaptureFirstFailure.clear();
    mMetalUICaptureComplete = false;
    mMetalUICaptureUsesDepth = false;
    mMetalUICaptureDepthWidth = 0;
    mMetalUICaptureDepthHeight = 0;
    mMetalUICapturePixel.reset();
    if (mMetalUICaptureAuthoritative)
    {
        MetalUICaptureSavedState& saved = mMetalUICaptureSavedState;
        llassert(saved.valid);
        LLGLSLShader::sCurBoundShader = saved.shader;
        LLGLSLShader::sCurBoundShaderPtr = saved.shader_ptr;
        ALTextureSlot& slot = mTextureSlots[0];
        slot.mCurrTexture = saved.texture;
        slot.mCurrTexType = saved.texture_type;
        slot.mCurrImage = saved.image;
        slot.mCurrNativeTexture = saved.native_texture;
        slot.mCurrSampler = saved.sampler;
        mCurrBlendColorSFactor = saved.blend_color_source;
        mCurrBlendColorDFactor = saved.blend_color_destination;
        mCurrBlendAlphaSFactor = saved.blend_alpha_source;
        mCurrBlendAlphaDFactor = saved.blend_alpha_destination;
        std::copy(std::begin(saved.color_mask), std::end(saved.color_mask),
                  std::begin(mCurrColorMask));
        std::copy(std::begin(saved.viewport), std::end(saved.viewport),
                  std::begin(mViewport));
        if (mBuffer.notNull())
        {
            mColorsp[0] = saved.color;
            mTexcoordsp[0] = saved.texcoord;
        }
        mMatrixMode = saved.matrix_mode;
        mMode = saved.primitive_mode;
        saved.valid = false;
    }
    mMetalUICaptureTextureBound = false;
    mMetalUICaptureAuthoritative = false;
    return output;
}

bool LLRender::metalUIFrameHasCompleteCoverage() const noexcept
{
    return mMetalUIRenderer && mMetalUIFrameAttemptedBatches != 0 &&
           mMetalUIFrameFallbackBatches == 0;
}

void LLRender::setMetalUIReplacementEnabled(bool enabled) noexcept
{
    mMetalUIReplacementEnabled = enabled;
}

bool LLRender::presentMetalTexture(const ALMetalTexture* texture)
{
    if (!mMetalUIRenderer || !mMetalUISurface || !texture ||
        mMetalUISurfaceFrameSubmitted || mMetalUIRenderer->hasOpenFrame())
    {
        return false;
    }

    std::string error;
    if (!mMetalUIRenderer->setFrameBackground(texture, &error) ||
        !mMetalUIRenderer->endFrame(&error))
    {
        LL_DEBUGS("Metal")
            << "Native retained world/UI drawable submission failed closed: "
            << error << LL_ENDL;
        return false;
    }
    mMetalUISurfaceFrameSubmitted = true;
    return true;
}

bool LLRender::setMetalPresentationOpaque(bool opaque)
{
    if (!mMetalUISurface)
    {
        return false;
    }
    std::string error;
    if (!mMetalUISurface->setOpaque(opaque, &error))
    {
        LL_WARNS("Metal") << "Could not set Metal presentation surface "
                           << (opaque ? "opaque: " : "transparent: ")
                           << error << LL_ENDL;
        return false;
    }
    return true;
}
#endif

void LLRender::endFrame(U32 frames_ahead)
{
#if LL_DARWIN
    if (mMetalUIRenderer)
    {
        std::string error;
        if (mMetalUISurfaceFrameSubmitted)
        {
            // presentMetalTexture already submitted this surface frame. A
            // second endFrame would acquire and present a transparent clear
            // over the completed world/UI drawable.
            mMetalUISurfaceFrameSubmitted = false;
        }
        // Native world presentation deliberately submits nothing when the
        // current authoritative world/UI transaction is incomplete.  Do not
        // manufacture an otherwise empty surface frame in that case: doing so
        // clears the drawable to black instead of retaining the last complete
        // world image.  Startup/login still owns direct surface replacement,
        // and an already-open direct frame must still be finished.
        else if ((mMetalUIReplacementEnabled ||
                  mMetalUIRenderer->hasOpenFrame()) &&
                 !mMetalUIRenderer->endFrame(&error))
        {
            LL_WARNS("Metal") << "Native production UI frame submission failed: "
                               << error << LL_ENDL;
        }
        else
        {
            const ALMetalUIRendererStats stats = mMetalUIRenderer->stats();
            if (mMetalUILoggedFrame == 0 && stats.frames != 0)
            {
                mMetalUILoggedFrame = stats.frames;
                LL_INFOS("Metal") << "Native production UI frame encoded "
                                   << stats.batches << " batches / " << stats.vertices
                                   << " vertices with " << stats.pipeline_states
                                   << " cached pipeline states and " << stats.sampler_states
                                   << " cached sampler states" << LL_ENDL;
            }
        }
    }
    mMetalUIFrameAttemptedBatches = 0;
    mMetalUIFrameFallbackBatches = 0;
#endif

    // The batch's results are written ahead of the fence that follows them. The batches the fences have just shown
    // finished are read here rather than before the next cull, the next batch's names are made here rather than by its
    // queries, and the fences of the uploads handed over by other contexts are polled here rather than in the next
    // idle: a threaded driver answers a call that returns a value only once its thread has caught up, and the fences'
    // own calls have just caught it up.
    if (gGLManager.mInited
#if LL_DARWIN
        || mMetalFrames
#endif
       )
    {
        mQueries.endFrame();
        mQueries.retire();
        mQueries.makeRoom();
    }
#if LL_DARWIN
    if (mMetalFrames)
    {
        mFrameFences.endFrame(frames_ahead);
    }
#endif
    LLImageGL::pollUploads();
    ALUniformBuffer::endFrame();
}

void LLRender::clearSamplers()
{
    mSamplerCache.clear();

    for (ALTextureSlot& unit : mTextureSlots)
    {
        unit.mCurrSampler = 0;
    }

}

void LLRender::warmupSamplers()
{
    mSamplerCache.warmup();
}

void LLRender::refreshState(void)
{
    // Native render passes snapshot complete explicit state at their encoding
    // boundary. There is no ambient driver state to rediscover or reissue;
    // only drain geometry queued under the prior semantic snapshot.
    mDirty = true;
    flush();
    mDirty = false;
}

// Pack the light arrays into the shared block. Returns true when the bytes moved.
bool LLRender::packLightsUBO()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;

    // Must match the std140 layout the shaders declare: five 16-byte-strided [8] arrays
    // (128 bytes each) then a float3 rounded up to the block's 16-byte multiple.
    static_assert(sizeof(LightsUBOData) == 656, "LightsUBOData must match std140 (656 bytes)");

    LightsUBOData d{};

    for (U32 i = 0; i < LL_NUM_LIGHT_UNITS; i++)
    {
        const LLLightState* light = &mLightState[i];

        // std140 pads every array element to 16 bytes whatever the element type, so each
        // row below is float[4] and the unused tail components stay zero.
        memcpy(d.light_position[i], light->mPosition.mV, sizeof(F32) * 4);
        memcpy(d.light_direction[i], light->mSpotDirection.mV, sizeof(F32) * 3);

        d.light_attenuation[i][0] = light->mLinearAtten;
        d.light_attenuation[i][1] = light->mQuadraticAtten;
        d.light_attenuation[i][2] = light->mSpecular.mV[2];
        d.light_attenuation[i][3] = light->mSpecular.mV[3];

        d.light_deferred_attenuation[i][0] = light->mSize;
        d.light_deferred_attenuation[i][1] = light->mFalloff;

        memcpy(d.light_diffuse[i], light->mDiffuse.mV, sizeof(F32) * 3);
    }

    memcpy(d.light_ambient, mAmbientLightColor.mV, sizeof(F32) * 3);

    if (memcmp(&d, &mLightsUBOData, sizeof(LightsUBOData)) == 0)
    {
        return false;
    }

    mLightsUBOData = d;
    return true;
}

void LLRender::syncLightState()
{
    // The light ARRAYS live in the shared UB_LIGHTS block: packed once when the light state
    // actually moves, not once per program. mLightHash is only a cheap trigger -- setPosition
    // and setSpotDirection bump it unconditionally (the modelview may have changed), so the
    // pack still byte-compares before spending an upload.
    if (mLightsUBOHash != mLightHash || !mLightsUBO.allocated())
    {
        mLightsUBOHash = mLightHash;
        if (packLightsUBO() || !mLightsUBO.allocated())
        {
            mLightsUBO.update(&mLightsUBOData, sizeof(LightsUBOData));
            mLightsUBOBound = false; // re-assert the binding against the new store
        }
    }

    if (!mLightsUBOBound)
    {
        mLightsUBO.bind(LLGLSLShader::UB_LIGHTS);
        mLightsUBOBound = true;
    }

    // Everything below is still a LOOSE per-program uniform, so it keeps the per-shader hash
    // gate. sun_up_factor has writers outside LLRender (the pipeline, the draw pools and
    // LLSettingsVO), so folding it into the shared block would let one writer stomp another.
    // The sun's, moon's and ambient light the atmospherics read are the Environment block's,
    // classic sky or not: the sky settings set them there, in one place for every program.
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;

    if (!shader || shader->mLightHash == mLightHash)
    {
        return;
    }

    shader->mLightHash = mLightHash;

    shader->uniform1i(LLShaderMgr::SUN_UP_FACTOR, mLightState[0].mSunIsPrimary ? 1 : 0);
}

void LLRender::packMatricesUBO()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;

    static_assert(sizeof(MatricesUBOData) == 368, "MatricesUBOData must match std140 (368 bytes)");

    const LLMatrix4a& mdv  = mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]];
    const LLMatrix4a& proj = mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]];
    const LLMatrix4a& tex  = mMatrix[MM_TEXTURE0][mMatIdx[MM_TEXTURE0]];

    const bool mdv_moved  = mMatHash[MM_MODELVIEW]  != mMatricesUBOHash[MM_MODELVIEW];
    const bool proj_moved = mMatHash[MM_PROJECTION] != mMatricesUBOHash[MM_PROJECTION];

    if (mdv_moved)
    {
        // The modelview is affine (view * model, no perspective), so the affine inverse is
        // exact and much cheaper than a general 4x4 inverse.
        mCachedInvMdv.setAffineInverse(mdv);
    }
    if (proj_moved)
    {
        // Projection is not affine -- general inverse required.
        mCachedInvProj.setInverse(proj);
    }
    if (mdv_moved || proj_moved)
    {
        // the modelview first, then the projection
        mCachedMVP.setMul(mdv, proj);
    }

    MatricesUBOData d;
    memcpy(d.modelview,            mdv.getF32ptr(),            sizeof(d.modelview));
    memcpy(d.projection,           proj.getF32ptr(),           sizeof(d.projection));
    memcpy(d.modelview_projection, mCachedMVP.getF32ptr(),     sizeof(d.modelview_projection));
    memcpy(d.inv_proj,             mCachedInvProj.getF32ptr(), sizeof(d.inv_proj));
    memcpy(d.texture0,             tex.getF32ptr(),            sizeof(d.texture0));

    // normal_matrix is the upper 3x3 of the inverse modelview transposed: column c of the
    // block is lane c of the inverse's first three rows, i.e. inv[4*k + c] for k = 0..2.
    // This reproduces exactly the nine floats the loose uniformMatrix3fv used to upload.
    const F32* inv = mCachedInvMdv.getF32ptr();
    for (U32 c = 0; c < 3; ++c)
    {
        d.normal[c][0] = inv[0 * 4 + c];
        d.normal[c][1] = inv[1 * 4 + c];
        d.normal[c][2] = inv[2 * 4 + c];
        d.normal[c][3] = 0.f;
    }

    U8* shadow = mMatricesUBO.beginWrite();
    if (memcmp(shadow, &d, sizeof(d)) != 0)
    {
        memcpy(shadow, &d, sizeof(d));
        mMatricesUBO.endWrite(0, sizeof(d));
    }
}

void LLRender::syncMatrices()
{
    STOP_GLERROR;
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;

    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;

    if (shader)
    {
        // The matrices live in the shared UB_MATRICES block: packed once per matrix EPOCH (any
        // mMatHash movement since the last pack) rather than once per program, so a program
        // bind costs no matrix work at all. The derived matrices (inverses, normal, MVP) are
        // recomputed per epoch under per-stack gates inside the pack, and unconditionally --
        // any program may read them from the block, so the old hasUniform() gates are gone.
        if (!mMatricesUBO.allocated())
        {
            // Fresh context (first use, or re-init after shutdown released the old block):
            // re-pack and re-bind everything.
            mMatricesUBO.initShadowed(sizeof(MatricesUBOData));
            for (U32 i = 0; i < NUM_MATRIX_MODES; ++i)
            {
                mMatricesUBOHash[i] = 0xFFFFFFFFu;
            }
            mMatricesUBOBound = false;
        }

        if (mMatHash[MM_MODELVIEW]  != mMatricesUBOHash[MM_MODELVIEW]  ||
            mMatHash[MM_PROJECTION] != mMatricesUBOHash[MM_PROJECTION] ||
            mMatHash[MM_TEXTURE0]   != mMatricesUBOHash[MM_TEXTURE0])
        {
            packMatricesUBO();
            for (U32 i = 0; i < NUM_MATRIX_MODES; ++i)
            {
                mMatricesUBOHash[i] = mMatHash[i];
            }
        }

        if (!mMatricesUBOBound)
        {
            // First attach on this context (or a refreshState() re-assert): bindCurrent both
            // uploads any pending bytes and (re)claims the binding point -- needed even when
            // clean, e.g. UPDATE_DIRECT where endWrite uploads without ever dirtying.
            mMatricesUBO.bindCurrent(LLGLSLShader::UB_MATRICES);
            mMatricesUBOBound = true;
        }
        else
        {
            // Per-draw steady state: upload when the pack dirtied the shadow, otherwise just
            // confirm the last flushed slice is still readable (streaming-ring reuse can
            // invalidate it). No GL calls at all on the clean-and-live path.
            mMatricesUBO.ensureCurrent(LLGLSLShader::UB_MATRICES);
        }

        if (shader->mFeatures.hasLighting || shader->mFeatures.calculatesLighting || shader->mFeatures.calculatesAtmospherics)
        { //also sync light state
            syncLightState();
        }
    }
    STOP_GLERROR;
}

void LLRender::translatef(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
    flush();

    {
        // the translation applies first: T then the stack's matrix
        LLMatrix4a& m = mMatrix[mMatrixMode][mMatIdx[mMatrixMode]];
        m.setMul(LLMatrix4a::translation(x, y, z), m);
        mMatHash[mMatrixMode]++;
    }
}

void LLRender::scalef(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
    flush();

    {
        LLMatrix4a& m = mMatrix[mMatrixMode][mMatIdx[mMatrixMode]];
        m.setMul(LLMatrix4a::scaling(x, y, z), m);
        mMatHash[mMatrixMode]++;
    }
}

void LLRender::ortho(F32 left, F32 right, F32 bottom, F32 top, F32 zNear, F32 zFar)
{
    flush();

    {
        // al_ortho emits reversed-ZO under reverse-Z (mapping legacy z in [-1,1] fully
        // inside the [0,1] clip volume, so 2D/UI content at z in [-1,0) is not clipped),
        // and the plain ortho otherwise. Converts all gGL.ortho() callers at once.
        LLMatrix4a& m = mMatrix[mMatrixMode][mMatIdx[mMatrixMode]];
        m.setMul(al_ortho(left, right, bottom, top, zNear, zFar), m);
        mMatHash[mMatrixMode]++;
    }
}

void LLRender::rotatef(const GLfloat& a, const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
    flush();

    {
        LLMatrix4a& m = mMatrix[mMatrixMode][mMatIdx[mMatrixMode]];
        m.setMul(LLMatrix4a::rotation(a * DEG_TO_RAD, LLVector4a(x, y, z)), m);
        mMatHash[mMatrixMode]++;
    }
}

void LLRender::pushMatrix()
{
    flush();

    {
        if (mMatIdx[mMatrixMode] < LL_MATRIX_STACK_DEPTH-1)
        {
            mMatrix[mMatrixMode][mMatIdx[mMatrixMode]+1] = mMatrix[mMatrixMode][mMatIdx[mMatrixMode]];
            ++mMatIdx[mMatrixMode];
        }
        else
        {
            LL_WARNS() << "Matrix stack overflow." << LL_ENDL;
        }
    }
}

void LLRender::popMatrix()
{
    flush();
    {
        if (mMatIdx[mMatrixMode] > 0)
        {
            --mMatIdx[mMatrixMode];
            mMatHash[mMatrixMode]++;
        }
        else
        {
            LL_WARNS() << "Matrix stack underflow." << LL_ENDL;
        }
    }
}

void LLRender::loadMatrix(const GLfloat* m)
{
    LLMatrix4a loaded;
    loaded.loadu(m);
    loadMatrix(loaded);
}

void LLRender::loadMatrix(const LLMatrix4a& m)
{
    flush();
    {
        mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = m;
        mMatHash[mMatrixMode]++;
    }
}

void LLRender::multMatrix(const GLfloat* m)
{
    LLMatrix4a factor;
    factor.loadu(m);
    multMatrix(factor);
}

void LLRender::multMatrix(const LLMatrix4a& m)
{
    flush();
    {
        // the new matrix applies first, then what the stack held
        LLMatrix4a& top = mMatrix[mMatrixMode][mMatIdx[mMatrixMode]];
        top.setMul(m, top);
        mMatHash[mMatrixMode]++;
    }
}

void LLRender::matrixMode(eMatrixMode mode)
{
    mMatrixMode = mode;
}

LLRender::eMatrixMode LLRender::getMatrixMode()
{
    return mMatrixMode;
}


void LLRender::loadIdentity()
{
    flush();

    {
        llassert_always(mMatrixMode < NUM_MATRIX_MODES) ;

        mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].setIdentity();
        mMatHash[mMatrixMode]++;
    }
}

const LLMatrix4a& LLRender::getModelviewMatrix()
{
    return mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]];
}

const LLMatrix4a& LLRender::getProjectionMatrix()
{
    return mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]];
}

const LLMatrix4a& LLRender::getTextureMatrix()
{
    return mMatrix[MM_TEXTURE0][mMatIdx[MM_TEXTURE0]];
}

void LLRender::translateUI(F32 x, F32 y, F32 z)
{
    if (mUIOffset.empty())
    {
        LL_ERRS() << "Need to push a UI translation frame before offsetting" << LL_ENDL;
    }

    mUIOffset.back().add(LLVector4a(x, y, z));
}

void LLRender::scaleUI(F32 x, F32 y, F32 z)
{
    if (mUIScale.empty())
    {
        LL_ERRS() << "Need to push a UI transformation frame before scaling." << LL_ENDL;
    }

    mUIScale.back().mul(LLVector4a(x, y, z));
}

void LLRender::pushUIMatrix()
{
    if (mUIOffset.empty())
    {
        mUIOffset.emplace_back(0.f);
    }
    else
    {
        mUIOffset.push_back(mUIOffset.back());
    }

    if (mUIScale.empty())
    {
        mUIScale.emplace_back(1.f);
    }
    else
    {
        mUIScale.push_back(mUIScale.back());
    }
}

void LLRender::popUIMatrix()
{
    if (mUIOffset.empty())
    {
        LL_ERRS() << "UI offset stack blown." << LL_ENDL;
    }
    mUIOffset.pop_back();
    mUIScale.pop_back();
}

LLVector3 LLRender::getUITranslation()
{
    if (mUIOffset.empty())
    {
        return LLVector3::zero;
    }

    return LLVector3(mUIOffset.back().getF32ptr());
}

LLVector3 LLRender::getUIScale()
{
    if (mUIScale.empty())
    {
        return LLVector3::all_one;
    }

    return LLVector3(mUIScale.back().getF32ptr());
}


void LLRender::loadUIIdentity()
{
    if (mUIOffset.empty())
    {
        LL_ERRS() << "Need to push UI translation frame before clearing offset." << LL_ENDL;
    }

    mUIOffset.back().clear();
    mUIScale.back().splat(1);
}

void LLRender::setColorMask(bool writeColor, bool writeAlpha)
{
    setColorMask(writeColor, writeColor, writeColor, writeAlpha);
}

void LLRender::setColorMask(bool writeColorR, bool writeColorG, bool writeColorB, bool writeAlpha)
{
    if (mCurrColorMask[0] != writeColorR ||
        mCurrColorMask[1] != writeColorG ||
        mCurrColorMask[2] != writeColorB ||
        mCurrColorMask[3] != writeAlpha)
    {
        flush();

        mCurrColorMask[0] = writeColorR;
        mCurrColorMask[1] = writeColorG;
        mCurrColorMask[2] = writeColorB;
        mCurrColorMask[3] = writeAlpha;

    }
}

void LLRender::setSceneBlendType(eBlendType type)
{
    switch (type)
    {
        case BT_ALPHA:
            blendFunc(BF_SOURCE_ALPHA, BF_ONE_MINUS_SOURCE_ALPHA);
            break;
        case BT_ADD:
            blendFunc(BF_ONE, BF_ONE);
            break;
        case BT_ADD_WITH_ALPHA:
            blendFunc(BF_SOURCE_ALPHA, BF_ONE);
            break;
        case BT_MULT:
            blendFunc(BF_DEST_COLOR, BF_ZERO);
            break;
        case BT_MULT_ALPHA:
            blendFunc(BF_DEST_ALPHA, BF_ZERO);
            break;
        case BT_MULT_X2:
            blendFunc(BF_DEST_COLOR, BF_SOURCE_COLOR);
            break;
        case BT_REPLACE:
            blendFunc(BF_ONE, BF_ZERO);
            break;
        default:
            LL_ERRS() << "Unknown Scene Blend Type: " << type << LL_ENDL;
            break;
    }
}

void LLRender::blendFunc(eBlendFactor sfactor, eBlendFactor dfactor)
{
    llassert(sfactor < BF_UNDEF);
    llassert(dfactor < BF_UNDEF);
    if (mCurrBlendColorSFactor != sfactor || mCurrBlendColorDFactor != dfactor ||
        mCurrBlendAlphaSFactor != sfactor || mCurrBlendAlphaDFactor != dfactor)
    {
        mCurrBlendColorSFactor = sfactor;
        mCurrBlendAlphaSFactor = sfactor;
        mCurrBlendColorDFactor = dfactor;
        mCurrBlendAlphaDFactor = dfactor;
        flush();
    }
}

void LLRender::blendFunc(eBlendFactor color_sfactor, eBlendFactor color_dfactor,
             eBlendFactor alpha_sfactor, eBlendFactor alpha_dfactor)
{
    llassert(color_sfactor < BF_UNDEF);
    llassert(color_dfactor < BF_UNDEF);
    llassert(alpha_sfactor < BF_UNDEF);
    llassert(alpha_dfactor < BF_UNDEF);

    if (mCurrBlendColorSFactor != color_sfactor || mCurrBlendColorDFactor != color_dfactor ||
        mCurrBlendAlphaSFactor != alpha_sfactor || mCurrBlendAlphaDFactor != alpha_dfactor)
    {
        mCurrBlendColorSFactor = color_sfactor;
        mCurrBlendAlphaSFactor = alpha_sfactor;
        mCurrBlendColorDFactor = color_dfactor;
        mCurrBlendAlphaDFactor = alpha_dfactor;
        flush();

    }
}

ALTextureSlot* LLRender::getTextureSlot(U32 index)
{
    if (index < mTextureSlots.size())
    {
        return &mTextureSlots[index];
    }
    else
    {
        LL_DEBUGS() << "Non-existing texture unit layer requested: " << index << LL_ENDL;
        return &mDummySlot;
    }
}

ALImageSlot* LLRender::getImageSlot(U32 index)
{
    if (index < mImageSlots.size())
    {
        return &mImageSlots[index];
    }

    LL_DEBUGS() << "Non-existing image unit requested: " << index << LL_ENDL;
    return &mDummyImageSlot;
}

void LLRender::forgetTextures(S32 count, const U32* names)
{
    const U32* end = names + count;
    for (ALTextureSlot& slot : mTextureSlots)
    {
        if (slot.mCurrTexture != 0 && std::find(names, end, slot.mCurrTexture) != end)
        {
            slot.mCurrTexture = 0;
        }
    }

    // Native image slots hold typed Metal resources, never GL names. Deleting
    // an unrelated compatibility name must not displace their bindings.
}

LLLightState* LLRender::getLight(U32 index)
{
    if (index < mLightState.size())
    {
        return &mLightState[index];
    }

    return NULL;
}

void LLRender::setAmbientLightColor(const LLColor4& color)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    if (color != mAmbientLightColor)
    {
        ++mLightHash;
        mAmbientLightColor = color;
    }
}

bool LLRender::verifyTexUnitActive(U32 unitToVerify)
{
    if (mCurrTextureUnitIndex == unitToVerify)
    {
        return true;
    }
    else
    {
        LL_WARNS() << "TexUnit currently active: " << mCurrTextureUnitIndex << " (expecting " << unitToVerify << ")" << LL_ENDL;
        return false;
    }
}

void LLRender::clearErrors()
{
    // Metal errors belong to command-buffer completion and are reported by
    // the owning encoder/scheduler. There is no thread-global error queue.
}

void LLRender::beginList(std::list<LLVertexBufferData> *list)
{
    if (mBufferDataList)
    {
        LL_ERRS() << "beginList called while another list is open." << LL_ENDL;
    }
    llassert(LLGLSLShader::sCurBoundShaderPtr == &gUIProgram);
    flush();
    mBufferDataList = list;
}

void LLRender::endList()
{
    if (mBufferDataList)
    {
        flush();
        mBufferDataList = nullptr;
    }
    else
    {
        llassert(false); // endList called without an open list
    }
}

void LLRender::begin(const GLuint& mode)
{
    if (mode != mMode)
    {
        if (mMode == LLRender::LINES ||
            mMode == LLRender::TRIANGLES ||
            mMode == LLRender::POINTS)
        {
            flush();
        }
        else if (mCount != 0)
        {
            LL_ERRS() << "gGL.begin() called redundantly." << LL_ENDL;
        }

        mMode = mode;
    }
}

void LLRender::end()
{
    if (mCount == 0)
    {
        return;
        //IMM_ERRS << "GL begin and end called with no vertices specified." << LL_ENDL;
    }

    if ((mMode != LLRender::LINES &&
        mMode != LLRender::TRIANGLES &&
        mMode != LLRender::POINTS) ||
        mCount > 2048)
    {
        flush();
    }
}

void LLRender::flush()
{
#if LL_DARWIN
    const bool native_capture = mMetalUICaptureAuthoritative;
#else
    constexpr bool native_capture = false;
#endif
    if (!native_capture)
    {
        STOP_GLERROR;
    }
    if (mCount > 0)
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
        llassert_always(LLGLSLShader::sCurBoundShaderPtr != nullptr);

        if (!mUIOffset.empty())
        {
            sUICalls++;
            sUIVerts += mCount;
        }

        //store mCount in a local variable to avoid re-entrance (drawArrays may call flush)
        U32 count = mCount;

        if (mMode == LLRender::TRIANGLES)
        {
            if (mCount%3 != 0)
            {
            count -= (mCount % 3);
            LL_WARNS() << "Incomplete triangle requested." << LL_ENDL;
            }
        }

        if (mMode == LLRender::LINES)
        {
            if (mCount%2 != 0)
            {
                count -= (mCount % 2);
                LL_WARNS() << "Incomplete line requested." << LL_ENDL;
            }
        }

        mCount = 0;

        if (mBuffer)
        {
            U32 attribute_mask = LLGLSLShader::sCurBoundShaderPtr->mAttributeMask;

            if (native_capture)
            {
                // Authoritative captures must not seed the retained VBO cache
                // with a buffer that deliberately has no GL name. A font
                // display list is different from that global hash cache: it
                // owns the Metal mirror and must retain it, or the text cache
                // marks an empty list valid and every later replay vanishes.
                LLPointer<LLVertexBuffer> native_buffer =
                    genBuffer(attribute_mask, count);
                if (mBufferDataList)
                {
                    ALTextureSlot* texture_slot = gGL.getTextureSlot(0);
                    mBufferDataList->emplace_back(
                        native_buffer,
                        mMode,
                        count,
                        texture_slot->mCurrImage,
                        mMetalUICaptureTextureBound,
                        mMetalUICaptureSampler,
                        mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]],
                        mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]],
                        mMatrix[MM_TEXTURE0][mMatIdx[MM_TEXTURE0]]);
                }
                drawBuffer(native_buffer, mMode, count);
            }
            else
            {
                LLVertexBuffer* vb = nullptr;
                if (mBufferDataList)
                {
                    vb = genBuffer(attribute_mask, count);
                    ALTextureSlot* texture_slot = gGL.getTextureSlot(0);
                    ALSamplerDesc sampler_desc = ALSamplerCache::makeDesc(ALSamplers::BilinearClamp);
                    mSamplerCache.describe(texture_slot->mCurrSampler, sampler_desc);
                    mBufferDataList->emplace_back(
                        vb,
                        mMode,
                        count,
                        texture_slot->mCurrImage,
                        texture_slot->mCurrTexture != 0,
                        sampler_desc,
                        mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]],
                        mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]],
                        mMatrix[MM_TEXTURE0][mMatIdx[MM_TEXTURE0]]
                        );
                }
                else
                {
                    vb = bufferfromCache(attribute_mask, count);
                }

                drawBuffer(vb, mMode, count);
            }
        }
        else
        {
            // mBuffer is present in main thread and not present in an image thread
            LL_ERRS() << "A flush call from outside main rendering thread" << LL_ENDL;
        }

        resetStriders(count);
    }
}

LLVertexBuffer* LLRender::bufferfromCache(U32 attribute_mask, U32 count)
{
    LLVertexBuffer *vb = nullptr;
    HBXXH64 hash;

    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_VERTEX("vb cache hash");

        hash.update((U8*)mVerticesp.get(), count * sizeof(LLVector4a));
        if (attribute_mask & LLVertexBuffer::MAP_TEXCOORD0)
        {
            hash.update((U8*)mTexcoordsp.get(), count * sizeof(LLVector2));
        }

        if (attribute_mask & LLVertexBuffer::MAP_COLOR)
        {
            hash.update((U8*)mColorsp.get(), count * sizeof(LLColor4U));
        }

        hash.finalize();
    }

    U64 vhash = hash.digest();

    // check the VB cache before making a new vertex buffer
    // This is a giant hack to deal with (mostly) our terrible UI rendering code
    // that was built on top of OpenGL immediate mode.  Huge performance wins
    // can be had by not uploading geometry to VRAM unless absolutely necessary.
    // Most of our usage of the "immediate mode" style draw calls is actually
    // sending the same geometry over and over again.
    // To leverage this, we maintain a running hash of the vertex stream being
    // built up before a flush, and then check that hash against a VB
    // cache just before creating a vertex buffer in VRAM
    boost::unordered_map<U64, LLVBCache>::iterator cache = mVBCache.find(vhash);
    if (cache != mVBCache.end())
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_VERTEX("vb cache hit");
        // cache hit, just use the cached buffer
        vb = cache->second.vb;
        cache->second.touched = std::chrono::steady_clock::now();
    }
    else
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_VERTEX("vb cache miss");
        vb = genBuffer(attribute_mask, count);

        mVBCache[vhash] = { vb , std::chrono::steady_clock::now() };

        static U32 miss_count = 0;
        miss_count++;
        if (miss_count > 1024)
        {
            LL_PROFILE_ZONE_NAMED_CATEGORY_VERTEX("vb cache clean");
            miss_count = 0;
            auto now = std::chrono::steady_clock::now();

            using namespace std::chrono_literals;
            // every 1024 misses, clean the cache of any VBs that haven't been touched in the last second
            for (boost::unordered_map<U64, LLVBCache>::iterator iter = mVBCache.begin(); iter != mVBCache.end();)
            {
                if (now - iter->second.touched > 1s)
                {
                    iter = mVBCache.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }
        }
    }
    return vb;
}

LLVertexBuffer* LLRender::genBuffer(U32 attribute_mask, S32 count)
{
    // Immediate-mode batches always contain positions. A native semantic
    // shader whose loader omitted that layout used to allocate a zero-byte
    // buffer and then memcpy the positions through a null mapped pointer.
    // Reject incomplete metadata here so the native renderer records a
    // failed batch instead of crashing the process.
    if (count <= 0 || !(attribute_mask & LLVertexBuffer::MAP_VERTEX))
    {
        LL_WARNS_ONCE("Render")
            << "Skipping immediate buffer with incomplete vertex layout: mask=0x"
            << std::hex << attribute_mask << std::dec
            << ", count=" << count << ", shader="
            << (LLGLSLShader::sCurBoundShaderPtr
                    ? LLGLSLShader::sCurBoundShaderPtr->mName
                    : std::string("<none>"))
            << LL_ENDL;
        return nullptr;
    }

    LLVertexBuffer * vb = new LLVertexBuffer(attribute_mask);
    if (!vb->allocateBuffer(count, 0))
    {
        LL_WARNS_ONCE("Render")
            << "Skipping immediate buffer after vertex allocation failed: mask=0x"
            << std::hex << attribute_mask << std::dec
            << ", count=" << count << ", shader="
            << (LLGLSLShader::sCurBoundShaderPtr
                    ? LLGLSLShader::sCurBoundShaderPtr->mName
                    : std::string("<none>"))
            << LL_ENDL;
        delete vb;
        return nullptr;
    }

    vb->setPositionData(mVerticesp.get());

    if (attribute_mask & LLVertexBuffer::MAP_TEXCOORD0)
    {
        vb->setTexCoord0Data(mTexcoordsp.get());
    }

    if (attribute_mask & LLVertexBuffer::MAP_COLOR)
    {
        vb->setColorData(mColorsp.get());
    }

    // The authoritative Darwin upload writes the Metal buffer. No parallel
    // GL VBO is created for a later replay.
    vb->unmapBuffer();
    vb->unbind();

    return vb;
}

void LLRender::drawBuffer(LLVertexBuffer* vb, U32 mode, S32 count)
{
    drawBoundMetalUIBuffer(vb, mode, count);
}

#if LL_DARWIN
bool LLRender::drawBoundMetalUIBuffer(
    LLVertexBuffer* buffer, U32 mode, U32 count, bool indexed, U32 first)
{
    ALTextureSlot* texture_slot = getTextureSlot(0);
    ALSamplerDesc sampler = mMetalUICaptureAuthoritative
        ? mMetalUICaptureSampler
        : ALSamplerCache::makeDesc(ALSamplers::BilinearClamp);
    if (texture_slot && !mMetalUICaptureAuthoritative)
    {
        mSamplerCache.describe(texture_slot->mCurrSampler, sampler);
    }
    // Metal is the only product renderer. A failed batch is accounted by the
    // native renderer and fails closed; the production buffer deliberately
    // has no GL storage to replay from.
    const bool capture = mMetalUICaptureRenderer != nullptr;
    const U64 captured_batches = mMetalUICaptureBatches;
    const bool replaced = drawMetalUIBuffer(
                      buffer, mode, indexed && buffer ? buffer->getNumVerts() : count,
                      texture_slot ? texture_slot->mCurrImage : nullptr,
                      sampler,
                      mMetalUICaptureAuthoritative
                          ? mMetalUICaptureTextureBound
                          : texture_slot && texture_slot->mCurrTexture != 0,
                      indexed, indexed ? count : 0, indexed ? first : 0,
                      indexed ? 0 : first);
    return capture ? mMetalUICaptureBatches == captured_batches + 1 : replaced;
}

bool LLRender::drawMetalUIBuffer(LLVertexBuffer* buffer, U32 mode, U32 count,
                                 LLImageGL* image, const ALSamplerDesc& sampler,
                                 bool texture_bound, bool indexed,
                                 U32 index_count, U32 indices_offset,
                                 U32 first_vertex)
{
    const bool capture = mMetalUICaptureRenderer != nullptr;
    const bool authoritative = capture && mMetalUICaptureAuthoritative;
    // An ordered offscreen capture owns the complete world/UI transaction.
    // Do not also open the direct surface renderer for the same UI batches:
    // the capture may publish its fused drawable at end-of-frame, while a
    // second direct frame would either contend for the surface or cover that
    // result. Outside a capture, the direct renderer is a transparent native
    // UI overlay and may replace each successfully encoded GL batch.
    const bool replace = !capture && mMetalUIRenderer &&
                         mMetalUIReplacementEnabled && !mUIOffset.empty();
    if (!capture && !replace)
    {
        return authoritative;
    }
    if (replace)
    {
        ++mMetalUIAttemptedBatches;
        ++mMetalUIFrameAttemptedBatches;
    }
    const auto fallback = [this, capture, replace, authoritative](
        MetalUIFallbackReason reason,
        const std::string& detail = std::string())
    {
        if (capture)
        {
            if (mMetalUICaptureFirstFailure.empty())
            {
                mMetalUICaptureFirstFailure = metal_ui_fallback_reason(reason);
                if (!detail.empty())
                {
                    mMetalUICaptureFirstFailure += ": " + detail;
                }
            }
            mMetalUICaptureComplete = false;
        }
        if (replace)
        {
            ++mMetalUIFallbackBatches;
            ++mMetalUIFrameFallbackBatches;
            ++sMetalUIFallbackReasons[static_cast<std::size_t>(reason)];
        }
        // Native-authoritative work fails closed: suppress the GL draw even
        // when this batch invalidated the Metal transaction.
        return authoritative;
    };
    if (!buffer || count == 0 || first_vertex > buffer->getNumVerts() ||
        count > buffer->getNumVerts() - first_vertex)
    {
        return fallback(MetalUIFallbackReason::incomplete_batch);
    }

    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    ALMetalUIShader metal_shader;
    if (shader == &gUIProgram)
    {
        if (!texture_bound && mUITextShadowMode != 0)
        {
            return fallback(MetalUIFallbackReason::shader);
        }
        metal_shader = texture_bound ? ALMetalUIShader::textured
                                     : ALMetalUIShader::vertex_color;
    }
    else if (shader == &gSolidColorProgram)
    {
        metal_shader = ALMetalUIShader::solid_color;
    }
    else if (capture && mMetalUICaptureUniformColorShader &&
             shader == mMetalUICaptureUniformColorShader)
    {
        metal_shader = ALMetalUIShader::uniform_color;
    }
    else if (capture && shader == mMetalUICaptureAlphaShader)
    {
        metal_shader = texture_bound ? ALMetalUIShader::alpha_mask_textured
                                     : ALMetalUIShader::alpha_mask_solid;
    }
    else
    {
        return fallback(
            MetalUIFallbackReason::shader,
            shader ? shader->mName : "no shader is bound");
    }

    ALMetalUIPrimitive primitive;
    if (!metal_ui_primitive(mode, primitive))
    {
        return fallback(MetalUIFallbackReason::primitive);
    }
    ALMetalBuffer* metal_buffer = buffer->getMetalVertexBuffer();
    ALMetalBuffer* metal_indices = indexed
        ? buffer->getMetalIndexBuffer() : nullptr;
    ALMetalTexture* metal_texture = image ? image->getMetalTexture() : nullptr;
    const bool needs_texture = metal_shader == ALMetalUIShader::textured ||
                               metal_shader == ALMetalUIShader::solid_color ||
                               metal_shader == ALMetalUIShader::alpha_mask_textured;
    const bool needs_color = metal_shader == ALMetalUIShader::textured ||
                             metal_shader == ALMetalUIShader::vertex_color ||
                             metal_shader == ALMetalUIShader::alpha_mask_textured ||
                             metal_shader == ALMetalUIShader::alpha_mask_solid;
    const bool needs_texcoord =
        metal_shader != ALMetalUIShader::uniform_color;
    const U32 required = LLVertexBuffer::MAP_VERTEX |
        (needs_texcoord ? LLVertexBuffer::MAP_TEXCOORD0 : 0) |
        (needs_color ? LLVertexBuffer::MAP_COLOR : 0);
    if (!metal_buffer || !buffer->hasPublishedMetalVertexBuffer() ||
        (indexed && (!metal_indices || !buffer->hasPublishedMetalIndexBuffer())) ||
        (needs_texture && !metal_texture) ||
        (buffer->getTypeMask() & required) != required)
    {
        static U32 warned = 0;
        if (warned++ < 4)
        {
            LL_WARNS("Metal") << "Skipping native UI batch without a complete Metal mirror: buffer="
                               << (metal_buffer ? "yes" : "no") << ", texture="
                               << (metal_texture ? "yes" : "no") << ", mask=0x"
                               << std::hex << buffer->getTypeMask() << std::dec << LL_ENDL;
        }
        return fallback(MetalUIFallbackReason::mirror);
    }

    const S32 viewport_width = mViewport[2];
    const S32 viewport_height = mViewport[3];
    if (viewport_width <= 0 || viewport_height <= 0)
    {
        return fallback(MetalUIFallbackReason::viewport);
    }
    const U32 target_width = static_cast<U32>(llmax(viewport_width, mViewport[0] + viewport_width));
    const U32 target_height = static_cast<U32>(llmax(viewport_height, mViewport[1] + viewport_height));

    ALMetalUIBatch batch;
    batch.vertices = metal_buffer;
    batch.position_offset = buffer->getOffset(LLVertexBuffer::TYPE_VERTEX) +
        static_cast<std::size_t>(first_vertex) * LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_VERTEX];
    batch.texcoord_offset = buffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD0) +
        static_cast<std::size_t>(first_vertex) * LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_TEXCOORD0];
    batch.color_offset = buffer->getOffset(LLVertexBuffer::TYPE_COLOR) +
        static_cast<std::size_t>(first_vertex) * LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_COLOR];
    batch.indices = metal_indices;
    if (indexed)
    {
        const U32 index_stride = buffer->getIndexStride();
        if (index_stride != sizeof(U16) && index_stride != sizeof(U32))
        {
            return fallback(MetalUIFallbackReason::mirror);
        }
        batch.index_offset =
            static_cast<std::size_t>(indices_offset) * index_stride;
        batch.index_type = index_stride == sizeof(U16)
            ? ALMetalIndexType::uint16 : ALMetalIndexType::uint32;
        batch.index_count = index_count;
    }
    // Immediate manipulator strips/fans obey the same cull state as indexed
    // geometry. Their topology does not change which faces the caller draws.
    if (LLGLState::isEnabled(GL_CULL_FACE))
    {
        if (getCullFace() == GL_BACK)
        {
            batch.cull_mode = ALMetalCullMode::back;
        }
        else if (getCullFace() == GL_FRONT)
        {
            batch.cull_mode = ALMetalCullMode::front;
        }
        else
        {
            return fallback(MetalUIFallbackReason::primitive);
        }
    }
    batch.texture = metal_texture;
    batch.sample_linear_storage = !sampler.mSRGBDecode;
    batch.sampler = metal_sampler_descriptor(sampler);
    batch.shader = metal_shader;
    batch.primitive = primitive;
    batch.vertex_count = count;
    batch.text_shadow_mode = mUITextShadowMode;
    batch.minimum_alpha = shader == mMetalUICaptureAlphaShader
        ? (authoritative ? mMetalUICaptureMinimumAlpha
                         : shader->getMinimumAlpha())
        : 0.004f;
    memcpy(batch.solid_color, mMetalSolidColor.mV, sizeof(batch.solid_color));
    batch.target_width = target_width;
    batch.target_height = target_height;

    const LLMatrix4a& modelview = mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]];
    const LLMatrix4a& projection = mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]];
    const LLMatrix4a& texture = mMatrix[MM_TEXTURE0][mMatIdx[MM_TEXTURE0]];
    LLMatrix4a modelview_projection;
    modelview_projection.setMul(modelview, projection);
    static_assert(sizeof(batch.matrices) == sizeof(MatricesUBOData));
    memcpy(batch.matrices.modelview, modelview.getF32ptr(), sizeof(batch.matrices.modelview));
    memcpy(batch.matrices.projection, projection.getF32ptr(), sizeof(batch.matrices.projection));
    memcpy(batch.matrices.modelview_projection, modelview_projection.getF32ptr(),
           sizeof(batch.matrices.modelview_projection));
    memcpy(batch.matrices.texture0, texture.getF32ptr(), sizeof(batch.matrices.texture0));

    batch.blend.enabled = LLGLState::isEnabled(GL_BLEND);
    batch.blend.source_rgb = metal_blend_factor(
        mCurrBlendColorSFactor == BF_UNDEF ? BF_SOURCE_ALPHA : mCurrBlendColorSFactor);
    batch.blend.destination_rgb = metal_blend_factor(
        mCurrBlendColorDFactor == BF_UNDEF ? BF_ONE_MINUS_SOURCE_ALPHA : mCurrBlendColorDFactor);
    batch.blend.source_alpha = metal_alpha_blend_factor(
        mCurrBlendAlphaSFactor == BF_UNDEF ? BF_SOURCE_ALPHA : mCurrBlendAlphaSFactor);
    batch.blend.destination_alpha = metal_alpha_blend_factor(
        mCurrBlendAlphaDFactor == BF_UNDEF ? BF_ONE_MINUS_SOURCE_ALPHA : mCurrBlendAlphaDFactor);
    batch.blend.rgb_operation = metal_blend_operation(mBlendEquationRGB);
    batch.blend.alpha_operation = metal_blend_operation(mBlendEquationAlpha);
    U8 write_mask = 0;
    write_mask |= mCurrColorMask[0] ? static_cast<U8>(ALMetalColorWriteMask::red) : 0;
    write_mask |= mCurrColorMask[1] ? static_cast<U8>(ALMetalColorWriteMask::green) : 0;
    write_mask |= mCurrColorMask[2] ? static_cast<U8>(ALMetalColorWriteMask::blue) : 0;
    write_mask |= mCurrColorMask[3] ? static_cast<U8>(ALMetalColorWriteMask::alpha) : 0;
    batch.blend.write_mask = static_cast<ALMetalColorWriteMask>(write_mask);

    // Surface replacement uses Metal's upper-left target coordinates. An
    // offscreen capture gets a copy below with GL's lower-left Y origin,
    // matching the vertex shader's capture-only Y flip.
    batch.viewport = {
        static_cast<double>(mViewport[0]),
        static_cast<double>(static_cast<S32>(target_height) - mViewport[1] - viewport_height),
        static_cast<double>(viewport_width),
        static_cast<double>(viewport_height),
        0.0,
        1.0,
    };

    if (LLGLState::isEnabled(GL_SCISSOR_TEST) && mScissor[2] >= 0 && mScissor[3] >= 0)
    {
        const S32 left = llclamp(mScissor[0], 0, static_cast<S32>(target_width));
        const S32 right = llclamp(mScissor[0] + mScissor[2], 0, static_cast<S32>(target_width));
        const S32 bottom = llclamp(mScissor[1], 0, static_cast<S32>(target_height));
        const S32 top = llclamp(mScissor[1] + mScissor[3], 0, static_cast<S32>(target_height));
        batch.scissor = {
            static_cast<U32>(left),
            static_cast<U32>(static_cast<S32>(target_height) - top),
            static_cast<U32>(llmax(0, right - left)),
            static_cast<U32>(llmax(0, top - bottom)),
        };
    }
    else
    {
        batch.scissor = {0, 0, target_width, target_height};
    }

    if (capture)
    {
        ALMetalUIBatch capture_batch = batch;
        capture_batch.viewport.y = static_cast<double>(mViewport[1]);
        capture_batch.scissor.y = LLGLState::isEnabled(GL_SCISSOR_TEST) &&
                                  mScissor[2] >= 0 && mScissor[3] >= 0
            ? static_cast<U32>(llclamp(
                  mScissor[1], 0, static_cast<S32>(target_height)))
            : 0u;
        if (mMetalUICaptureUsesDepth)
        {
            capture_batch.depth.enabled = LLGLDepthTest::depthEnabled();
            capture_batch.depth.write_enabled = capture_batch.depth.enabled &&
                LLGLDepthTest::depthWriteEnabled();
            capture_batch.depth.reverse_z = sReverseZ;
            capture_batch.depth.compare = metal_compare_function(
                LLGLDepthTest::remap(LLGLDepthTest::depthFunction()));
        }
        std::string error;
        if (!mMetalUICaptureRenderer->draw(capture_batch, &error))
        {
            if (mMetalUICaptureFirstFailure.empty())
            {
                mMetalUICaptureFirstFailure =
                    "immediate-mode encoder: " + error;
            }
            mMetalUICaptureComplete = false;
            LL_DEBUGS("Metal")
                << (authoritative
                        ? "Authoritative native immediate-mode capture failed: "
                        : "Offscreen immediate-mode capture failed: ")
                << error << LL_ENDL;
        }
        else
        {
            ++mMetalUICaptureBatches;
        }
    }
    if (!replace)
    {
        return authoritative;
    }

    std::string error;
    if (!mMetalUIRenderer->draw(batch, &error))
    {
        static U32 warned = 0;
        if (warned++ < 8)
        {
            LL_WARNS("Metal") << "Native production UI batch encoding failed: "
                               << error << LL_ENDL;
        }
        return fallback(MetalUIFallbackReason::encoder);
    }
    return true;
}

#endif

void LLRender::resetStriders(S32 count)
{
    mVerticesp[0] = mVerticesp[count];
    mTexcoordsp[0] = mTexcoordsp[count];
    mColorsp[0] = mColorsp[count];

    mCount = 0;
}

void LLRender::vertex3f(const GLfloat& x, const GLfloat& y, const GLfloat& z)
{
    //the range of mVerticesp, mColorsp and mTexcoordsp is [0, 4095]
    if (mCount > 2048)
    { //break when buffer gets reasonably full to keep GL command buffers happy and avoid overflow below
        switch (mMode)
        {
            case LLRender::POINTS: flush(); break;
            case LLRender::TRIANGLES: if (mCount%3==0) flush(); break;
            case LLRender::LINES: if (mCount%2 == 0) flush(); break;
        }
    }

    if (mCount > 4094)
    {
    //  LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
        return;
    }

    if (mUIOffset.empty())
    {
        mVerticesp[mCount].set(x,y,z);
    }
    else
    {
        LLVector4a vert(x, y, z);
        vert.add(mUIOffset.back());
        vert.mul(mUIScale.back());
        mVerticesp[mCount] = vert;
    }

    mCount++;
    mVerticesp[mCount] = mVerticesp[mCount-1];
    mColorsp[mCount] = mColorsp[mCount-1];
    mTexcoordsp[mCount] = mTexcoordsp[mCount-1];
}

void LLRender::vertexBatchPreTransformed(LLVector4a* verts, S32 vert_count)
{
    if (mCount + vert_count > 4094)
    {
        //  LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
        return;
    }

    for (S32 i = 0; i < vert_count; i++)
    {
        mVerticesp[mCount] = verts[i];

        mCount++;
        mTexcoordsp[mCount] = mTexcoordsp[mCount-1];
        mColorsp[mCount] = mColorsp[mCount-1];
    }

    if( mCount > 0 ) // ND: Guard against crashes if mCount is zero, yes it can happen
        mVerticesp[mCount] = mVerticesp[mCount-1];
}

void LLRender::vertexBatchPreTransformed(LLVector4a* verts, LLVector2* uvs, S32 vert_count)
{
    if (mCount + vert_count > 4094)
    {
        //  LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
        return;
    }

    for (S32 i = 0; i < vert_count; i++)
    {
        mVerticesp[mCount] = verts[i];
        mTexcoordsp[mCount] = uvs[i];

        mCount++;
        mColorsp[mCount] = mColorsp[mCount-1];
    }

    if (mCount > 0)
    {
        mVerticesp[mCount] = mVerticesp[mCount - 1];
        mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
    }
}

void LLRender::vertexBatchPreTransformed(LLVector4a* verts, LLVector2* uvs, LLColor4U* colors, S32 vert_count)
{
    if (mCount + vert_count > 4094)
    {
        //  LL_WARNS() << "GL immediate mode overflow.  Some geometry not drawn." << LL_ENDL;
        return;
    }

    for (S32 i = 0; i < vert_count; i++)
    {
        mVerticesp[mCount] = verts[i];
        mTexcoordsp[mCount] = uvs[i];
        mColorsp[mCount] = colors[i];

        mCount++;
    }

    if (mCount > 0)
    {
        mVerticesp[mCount] = mVerticesp[mCount - 1];
        mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
        mColorsp[mCount] = mColorsp[mCount - 1];
    }
}

void LLRender::vertex2i(const GLint& x, const GLint& y)
{
    vertex3f((GLfloat) x, (GLfloat) y, 0);
}

void LLRender::vertex2f(const GLfloat& x, const GLfloat& y)
{
    vertex3f(x,y,0);
}

void LLRender::vertex2fv(const GLfloat* v)
{
    vertex3f(v[0], v[1], 0);
}

void LLRender::vertex3fv(const GLfloat* v)
{
    vertex3f(v[0], v[1], v[2]);
}

void LLRender::texCoord2f(const GLfloat& x, const GLfloat& y)
{
    mTexcoordsp[mCount] = LLVector2(x,y);
}

void LLRender::texCoord2i(const GLint& x, const GLint& y)
{
    texCoord2f((GLfloat) x, (GLfloat) y);
}

void LLRender::texCoord2fv(const GLfloat* tc)
{
    texCoord2f(tc[0], tc[1]);
}

void LLRender::color4ub(const GLubyte& r, const GLubyte& g, const GLubyte& b, const GLubyte& a)
{
    if (!LLGLSLShader::sCurBoundShaderPtr || LLGLSLShader::sCurBoundShaderPtr->mAttributeMask & LLVertexBuffer::MAP_COLOR)
    {
        mColorsp[mCount] = LLColor4U(r,g,b,a);
    }
    else
    { //not using shaders or shader reads color from a uniform
        diffuseColor4ub(r,g,b,a);
    }
}
void LLRender::color4ubv(const GLubyte* c)
{
    color4ub(c[0], c[1], c[2], c[3]);
}

void LLRender::color4f(const GLfloat& r, const GLfloat& g, const GLfloat& b, const GLfloat& a)
{
    color4ub((GLubyte) (llclamp(r, 0.f, 1.f)*255),
        (GLubyte) (llclamp(g, 0.f, 1.f)*255),
        (GLubyte) (llclamp(b, 0.f, 1.f)*255),
        (GLubyte) (llclamp(a, 0.f, 1.f)*255));
}

void LLRender::color4fv(const GLfloat* c)
{
    color4f(c[0],c[1],c[2],c[3]);
}

void LLRender::color3f(const GLfloat& r, const GLfloat& g, const GLfloat& b)
{
    color4f(r,g,b,1);
}

void LLRender::color3fv(const GLfloat* c)
{
    color4f(c[0],c[1],c[2],1);
}

void LLRender::diffuseColor3f(F32 r, F32 g, F32 b)
{
    mMetalSolidColor.set(r, g, b, 1.f);
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    llassert(shader != NULL);

    if (shader)
    {
        shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r,g,b,1.f);
    }
}

void LLRender::diffuseColor3fv(const F32* c)
{
    mMetalSolidColor.set(c[0], c[1], c[2], 1.f);
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    llassert(shader != NULL);

    if (shader)
    {
        shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, c[0], c[1], c[2], 1.f);
    }
}

void LLRender::diffuseColor4f(F32 r, F32 g, F32 b, F32 a)
{
    mMetalSolidColor.set(r, g, b, a);
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    llassert(shader != NULL);

    if (shader)
    {
        shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r,g,b,a);
    }
}

void LLRender::diffuseColor4fv(const F32* c)
{
    mMetalSolidColor.set(c);
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    llassert(shader != NULL);

    if (shader)
    {
        shader->uniform4fv(LLShaderMgr::DIFFUSE_COLOR, 1, c);
    }
}

void LLRender::diffuseColor4ubv(const U8* c)
{
    mMetalSolidColor.set(c[0] / 255.f, c[1] / 255.f, c[2] / 255.f, c[3] / 255.f);
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    llassert(shader != NULL);

    if (shader)
    {
        shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, c[0]/255.f, c[1]/255.f, c[2]/255.f, c[3]/255.f);
    }
}

void LLRender::diffuseColor4ub(U8 r, U8 g, U8 b, U8 a)
{
    mMetalSolidColor.set(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    llassert(shader != NULL);

    if (shader)
    {
        shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r/255.f, g/255.f, b/255.f, a/255.f);
    }
}

void LLRender::setUITextShadowMode(U32 mode)
{
    llassert(mode <= 2);
    if (mUITextShadowMode != mode)
    {
        // The GL uniform writers call this before changing their program
        // value, so queued geometry is drained under the old constant.
        flush();
        mUITextShadowMode = mode;
    }
}

void LLRender::setLineWidth(F32 width)
{
    llassert(width > 0.f);
    if(mLineWidth != width)
    {
        flush();
        mLineWidth = width;
    }
}

void LLRender::setPolygonOffset(F32 factor, F32 units)
{
    if (mPolygonOffsetFactor != factor || mPolygonOffsetUnits != units)
    {
        mPolygonOffsetFactor = factor;
        mPolygonOffsetUnits = units;
        flush();

    }
}

void LLRender::setPatchVertices(U32 count)
{
    if (mPatchVertices != count)
    {
        mPatchVertices = count;
        flush();
    }
}

void LLRender::setClearColor(const LLColor4& color)
{
    if (mClearColor != color)
    {
        mClearColor = color;
    }
}

void LLRender::clear(U32 mask)
{
    (void)mask;
    flush();
}

void LLRender::setPolygonMode(U32 mode)
{
    llassert(mode == GL_FILL || mode == GL_LINE || mode == GL_POINT);
    if (mPolygonMode != mode)
    {
        flush();
        mPolygonMode = mode;
    }
}

void LLRender::setCullFace(U32 face)
{
    llassert(face == GL_BACK || face == GL_FRONT || face == GL_FRONT_AND_BACK);
    if (mCullFace != face)
    {
        flush();
        mCullFace = face;
    }
}

void LLRender::setPointSize(F32 size)
{
    llassert(size > 0.f);
    if (mPointSize != size)
    {
        flush();
        mPointSize = size;
    }
}

void LLRender::setStencilFunc(U32 func, S32 ref, U32 mask)
{
    if (mStencilFunc != func || mStencilRef != ref || mStencilReadMask != mask)
    {
        flush();
        mStencilFunc     = func;
        mStencilRef      = ref;
        mStencilReadMask = mask;
    }
}

void LLRender::setStencilOp(U32 stencil_fail, U32 depth_fail, U32 pass)
{
    if (mStencilFail != stencil_fail || mStencilDepthFail != depth_fail || mStencilPass != pass)
    {
        flush();
        mStencilFail      = stencil_fail;
        mStencilDepthFail = depth_fail;
        mStencilPass      = pass;
    }
}

void LLRender::setStencilMask(U32 mask)
{
    if (mStencilWriteMask != mask)
    {
        flush();
        mStencilWriteMask = mask;
    }
}

void LLRender::setBlendEquation(U32 rgb, U32 alpha)
{
    if (mBlendEquationRGB != rgb || mBlendEquationAlpha != alpha)
    {
        flush();
        mBlendEquationRGB   = rgb;
        mBlendEquationAlpha = alpha;
    }
}

void LLRender::setViewport(S32 x, S32 y, S32 width, S32 height)
{
    llassert(width >= 0 && height >= 0);
    if (mViewport[0] != x || mViewport[1] != y || mViewport[2] != width || mViewport[3] != height)
    {
        flush();
        mViewport[0] = x;
        mViewport[1] = y;
        mViewport[2] = width;
        mViewport[3] = height;
    }
}

// static
void LLRender::applyDepthConvention()
{
    // Native render-pass descriptors and projection helpers consume sReverseZ
    // directly. There is no ambient driver convention to mutate.
}

void LLRender::setScissor(S32 x, S32 y, S32 width, S32 height)
{
    llassert(width >= 0 && height >= 0);
    if (mScissor[0] != x || mScissor[1] != y || mScissor[2] != width || mScissor[3] != height)
    {
        flush();
        mScissor[0] = x;
        mScissor[1] = y;
        mScissor[2] = width;
        mScissor[3] = height;
    }
}

void LLRender::rebasePolygonOffset()
{
    flush();
}

void LLRender::debugTexUnits(void)
{
    LL_INFOS("TextureUnit") << "Active TexUnit: " << mCurrTextureUnitIndex << LL_ENDL;
    std::string active_enabled = "false";
    for (U32 i = 0; i < mTextureSlots.size(); i++)
    {
        if (getTextureSlot(i)->mCurrTexType != ALTextureSlot::TT_NONE)
        {
            if (i == mCurrTextureUnitIndex) active_enabled = "true";
            LL_INFOS("TextureUnit") << "TexUnit: " << i << " Enabled" << LL_ENDL;
            LL_INFOS("TextureUnit") << "Enabled As: " ;
            switch (getTextureSlot(i)->mCurrTexType)
            {
                case ALTextureSlot::TT_TEXTURE:
                    LL_CONT << "Texture 2D";
                    break;
                case ALTextureSlot::TT_RECT_TEXTURE:
                    LL_CONT << "Texture Rectangle";
                    break;
                case ALTextureSlot::TT_CUBE_MAP:
                    LL_CONT << "Cube Map";
                    break;
                default:
                    LL_CONT << "ARGH!!! NONE!";
                    break;
            }
            LL_CONT << ", Texture Bound: " << getTextureSlot(i)->mCurrTexture << LL_ENDL;
        }
    }
    LL_INFOS("TextureUnit") << "Active TexUnit Enabled : " << active_enabled << LL_ENDL;
}

LLMatrix4a al_reverse_z_transform(const LLMatrix4a& p)
{
    // z_ndc' = (1 - z_ndc)/2: the depth column becomes 0.5 * (w column - depth column),
    // x, y and w untouched.
    LLVector4a depth;
    depth.setSub(p.getColumn<3>(), p.getColumn<2>());
    depth.mul(0.5f);
    LLMatrix4a r = p;
    r.setColumn<2>(depth);
    return r;
}

LLMatrix4a al_perspective(F32 fovy_rad, F32 aspect, F32 z_near, F32 z_far)
{
    // An infinite far plane is exact only when reversed: forward 24-bit depth keeps the finite plane.
    if (std::isinf(z_far))
    {
        if (!LLRender::sReverseZ)
        {
            return LLMatrix4a::perspective(fovy_rad, aspect, z_near, FINITE_PROJECTION_FAR);
        }
        // Stored depth d = near / distance: the depth row is (0, 0, 0, near) against a w row of (0, 0, -1, 0).
        LLMatrix4a p = LLMatrix4a::perspective(fovy_rad, aspect, z_near, z_near * 2.f);
        p.setColumn<2>(LLVector4a(0.f, 0.f, 0.f, z_near));
        return p;
    }
    const LLMatrix4a p = LLMatrix4a::perspective(fovy_rad, aspect, z_near, z_far);
    return LLRender::sReverseZ ? al_reverse_z_transform(p) : p;
}

bool al_projection_is_infinite(const LLMatrix4a& proj)
{
    return LLRender::sReverseZ && proj.getColumn<2>()[2] == 0.f && proj.getColumn<3>()[2] != 0.f;
}

LLMatrix4a al_ortho(F32 left, F32 right, F32 bottom, F32 top, F32 z_near, F32 z_far)
{
    const LLMatrix4a p = LLMatrix4a::ortho(left, right, bottom, top, z_near, z_far);
    return LLRender::sReverseZ ? al_reverse_z_transform(p) : p;
}

LLVector4a al_project(const LLVector4a& obj, const LLMatrix4a& modelview, const LLMatrix4a& proj, const S32 viewport[4])
{
    // Under reverse-Z the projection already yields [0,1] window z, so the zero-to-one
    // form does not re-apply the [-1,1]->[0,1] remap.
    return LLRender::sReverseZ ? alprojection::project_zo(obj, modelview, proj, viewport)
                               : alprojection::project(obj, modelview, proj, viewport);
}

LLVector4a al_unproject(const LLVector4a& win, const LLMatrix4a& modelview, const LLMatrix4a& proj, const S32 viewport[4])
{
    return LLRender::sReverseZ ? alprojection::unproject_zo(win, modelview, proj, viewport)
                               : alprojection::unproject(win, modelview, proj, viewport);
}

LLVector4a al_unproject(const LLVector4a& win, const LLMatrix4a& inverse, const S32 viewport[4])
{
    return LLRender::sReverseZ ? alprojection::unproject_zo(win, inverse, viewport)
                               : alprojection::unproject(win, inverse, viewport);
}
