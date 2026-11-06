/**
 * @file llimagegl.cpp
 * @brief Generic GL image handler
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


// TODO: create 2 classes for images w/ and w/o discard levels?

#include "linden_common.h"

#include "llimagegl.h"

#include "llerror.h"
#include "llimage.h"

#if LL_DARWIN
#include "almetaldevice.h"
#include "almetalresource.h"
#include "almetaltextureops.h"
#endif

#include "llmath.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llrender.h"
#include "llrendertarget.h"
#include "llwindow.h"
#include "llframetimer.h"
#include <atomic>
#include <chrono>
#include <bit>
#include <thread>
#include <boost/unordered_map.hpp>

extern LL_COMMON_API bool on_main_thread();

#if !LL_IMAGEGL_THREAD_CHECK
#define checkActiveThread()
#endif

//----------------------------------------------------------------------------
const F32 MIN_TEXTURE_LIFETIME = 10.f;

constexpr int DELETE_DELAY = 3; // number of frames to wait before deleting textures
static std::vector<U32> sFreeList[DELETE_DELAY+1];
// The GL context the names in a thread's pool were generated on: each
// thread's pool is emptied when it is next asked after the context went,
// since a name of the old context is nobody's on the new one -- a core
// profile refuses to bind it -- and the pool is thread-local, so no one
// thread can empty the others'.
static std::atomic<U32> sTextureNameGeneration{ 1 };
constexpr U8 METAL_UPLOAD_PENDING = 0;
constexpr U8 METAL_UPLOAD_COMMITTED = 1;
constexpr U8 METAL_UPLOAD_FAILED = 2;
#if LL_DARWIN
static ALMetalDevice* sImageMetalDevice = nullptr;
static bool sMetalTextureAuthority = false;
static std::unique_ptr<ALMetalTextureResampler> sMetalTextureResampler;
static std::atomic<bool> sMetalTextureReported{false};
static std::atomic<bool> sMetalScaleDownReported{false};
static std::atomic<bool> sMetalResampleReported{false};

// The image thread is deliberately one worker. Textures decoded in one queue burst can
// therefore share a Metal command buffer without locking, while uploads outside this worker
// retain their immediate-submit behavior. A queued barrier commits the burst after the work
// that was already pending when it began; the count cap prevents a continuously replenished
// queue from retaining an unbounded number of staging buffers.
constexpr U32 METAL_UPLOAD_BATCH_LIMIT = 16;

struct WorkerMetalUploadBatch
{
    std::unique_ptr<ALMetalCommandBuffer> mCommand;
    std::shared_ptr<std::atomic<U8>> mCompletion;
    U32 mEncodedUploads = 0;
    bool mFlushQueued = false;
};

thread_local bool sOnImageGLThread = false;
thread_local WorkerMetalUploadBatch sWorkerMetalUploadBatch;

void flush_worker_metal_upload_batch()
{
    WorkerMetalUploadBatch& batch = sWorkerMetalUploadBatch;
    if (!batch.mCommand)
    {
        return;
    }

    std::string error;
    const bool committed = batch.mCommand->commit(&error);
    if (!committed)
    {
        LL_WARNS("Metal") << "Could not commit LLImageGL worker upload batch: "
                          << error << LL_ENDL;
    }
    batch.mCommand.reset();
    batch.mCompletion->store(committed ? METAL_UPLOAD_COMMITTED : METAL_UPLOAD_FAILED,
                             std::memory_order_release);
    batch.mCompletion.reset();
    batch.mEncodedUploads = 0;
}

ALMetalCommandBuffer* acquire_worker_metal_upload(ALMetalDevice& device, std::string* error)
{
    WorkerMetalUploadBatch& batch = sWorkerMetalUploadBatch;
    if (!batch.mCommand)
    {
        batch.mCommand = device.makeCommandBuffer("Alchemy image upload batch", error);
        if (!batch.mCommand)
        {
            return nullptr;
        }
        batch.mCompletion = std::make_shared<std::atomic<U8>>(METAL_UPLOAD_PENDING);

        // This barrier lands behind the queue burst already waiting. Work posted after it
        // starts a later batch. If shutdown has closed the queue, run() flushes explicitly.
        if (!batch.mFlushQueued && LLImageGLThread::instanceExists())
        {
            batch.mFlushQueued = LLImageGLThread::instance().post([]()
            {
                sWorkerMetalUploadBatch.mFlushQueued = false;
                flush_worker_metal_upload_batch();
            });
        }
    }
    return batch.mCommand.get();
}

std::shared_ptr<std::atomic<U8>> current_worker_metal_upload_completion()
{
    return sOnImageGLThread ? sWorkerMetalUploadBatch.mCompletion : nullptr;
}
#endif

namespace
{
    // An off-thread native upload handed to the main thread, oldest first.
    struct PendingPublish
    {
        LLPointer<LLImageGL> mImage;
        // Neighboring uploads may share a worker command buffer. Publication waits until
        // that native batch has committed; the single Metal queue preserves ordering.
        std::shared_ptr<std::atomic<U8>> mMetalCompletion;
    };
    std::vector<PendingPublish> sPendingPublishes;

    bool publication_ready(const PendingPublish& pending)
    {
        return !pending.mMetalCompletion ||
            pending.mMetalCompletion->load(std::memory_order_acquire) !=
                METAL_UPLOAD_PENDING;
    }
}

// Number of mip levels in a full pyramid for the given level-0 dimensions, counting
// level 0 itself: 256x256 -> 9 (256,128,...,1). This is a COUNT. Note llvertexbuffer's
// wpo2() returns log2, i.e. the highest mip *index*, which is one less -- it used to be
// assigned to mMipLevels here, and must not be substituted for this.
S32 LLImageGL::calcMipLevelCount(S32 width, S32 height)
{
    S32 levels = 1;
    while (width > 1 || height > 1)
    {
        width = llmax(1, width >> 1);
        height = llmax(1, height >> 1);
        ++levels;
    }
    return levels;
}


U32 LLImageGL::sFrameCount = 0;

// The last content stamp handed out (LLImageGL::getContentStamp).
static std::atomic<U64> sContentStamps{ 0 };

void LLImageGL::touchContents()
{
    mContentStamp.store(sContentStamps.fetch_add(1, std::memory_order_relaxed) + 1, std::memory_order_relaxed);
}


// texture memory accounting (for macOS)
static LLMutex sTexMemMutex;
static boost::unordered_map<U32, U64> sTextureAllocs;
// Changed under sTexMemMutex, and read by getTextureBytesAllocated without it.
static std::atomic<U64> sTextureBytes{0};
#if LL_DARWIN
// Native-authoritative LLImageGL storage is not represented by a GL name, so it
// has a separate process-wide contribution. A dual-backed migration resource is
// deliberately represented only by sTextureBytes until Metal becomes authoritative.
static std::atomic<U64> sMetalTextureBytes{0};

static U64 metal_texture_bytes(const ALMetalTexture& texture) noexcept
{
    const ALMetalTextureDescriptor& descriptor = texture.descriptor();
    U64 slices = 1;
    switch (descriptor.type)
    {
        case ALMetalTextureType::texture_2d_array:
            slices = descriptor.array_length;
            break;
        case ALMetalTextureType::texture_cube:
            slices = 6;
            break;
        case ALMetalTextureType::texture_cube_array:
            slices = static_cast<U64>(descriptor.array_length) * 6;
            break;
        default:
            break;
    }

    const U64 block_width = llmax<U64>(texture.blockWidth(), 1);
    const U64 block_height = llmax<U64>(texture.blockHeight(), 1);
    const U64 bytes_per_block = texture.bytesPerBlock();
    U64 bytes = 0;
    for (U32 mip = 0; mip < descriptor.mipmap_level_count; ++mip)
    {
        const U64 blocks_wide =
            (static_cast<U64>(texture.width(mip)) + block_width - 1) / block_width;
        const U64 blocks_high =
            (static_cast<U64>(texture.height(mip)) + block_height - 1) / block_height;
        bytes += blocks_wide * blocks_high * bytes_per_block;
    }
    return bytes * slices * llmax<U64>(descriptor.sample_count, 1);
}
#endif

// track a texture alloc on the currently bound texture.
// asserts that no currently tracked alloc exists
void LLImageGLMemory::alloc_tex_image(U32 width, U32 height, U32 intformat, U32 count, bool has_mips)
{
    U32 texUnit = gGL.getCurrentTexUnitIndex();
    llassert(texUnit == 0); // allocations should always be done on tex unit 0
    U32 texName = gGL.getTextureSlot(texUnit)->getCurrTexture();
    U64 size = LLImageGL::dataFormatVRAMBytes(intformat, width, height);
    if (has_mips)
    {
        // Sum the mip pyramid down to 1x1 the same way getMipBytes does,
        // so non-power-of-two and non-square cases stay exact rather than
        // relying on the 4/3 geometric-series approximation.
        S32 w = (S32)width;
        S32 h = (S32)height;
        while (w > 1 && h > 1)
        {
            w >>= 1; if (w == 0) w = 1;
            h >>= 1; if (h == 0) h = 1;
            size += LLImageGL::dataFormatVRAMBytes(intformat, w, h);
        }
    }
    size *= count;

    llassert(size >= 0);

    sTexMemMutex.lock();

    // it is a precondition that no existing allocation exists for this texture
    llassert(sTextureAllocs.find(texName) == sTextureAllocs.end());

    sTextureAllocs[texName] = size;
    sTextureBytes += size;

    sTexMemMutex.unlock();
}

// track texture free on given texName
void LLImageGLMemory::free_tex_image(U32 texName)
{
    sTexMemMutex.lock();
    auto iter = sTextureAllocs.find(texName);
    if (iter != sTextureAllocs.end()) // sometimes a texName will be "freed" before allocated (e.g. the first allocation on a given texName)
    {
        llassert(iter->second <= sTextureBytes); // sTextureBytes MUST NOT go below zero

        sTextureBytes -= iter->second;

        sTextureAllocs.erase(iter);
    }

    sTexMemMutex.unlock();
}

// track texture free on given texNames
void LLImageGLMemory::free_tex_images(U32 count, const U32* texNames)
{
    for (U32 i = 0; i < count; ++i)
    {
        free_tex_image(texNames[i]);
    }
}

// track texture free on currently bound texture
void LLImageGLMemory::free_cur_tex_image()
{
    U32 texUnit = gGL.getCurrentTexUnitIndex();
    llassert(texUnit == 0); // frees should always be done on tex unit 0
    U32 texName = gGL.getTextureSlot(texUnit)->getCurrTexture();
    free_tex_image(texName);
}

using namespace LLImageGLMemory;

// static
U64 LLImageGL::getTextureBytesAllocated()
{
    U64 bytes = sTextureBytes.load(std::memory_order_relaxed);
#if LL_DARWIN
    bytes += sMetalTextureBytes.load(std::memory_order_relaxed);
#endif
    return bytes;
}

//statics

U32 LLImageGL::sUniqueCount             = 0;
U32 LLImageGL::sBindCount               = 0;
S32 LLImageGL::sCount                   = 0;

F64 LLImageGL::sLastFrameTime           = 0.0;
LLImageGL* LLImageGL::sDefaultGLTexture = NULL ;
boost::unordered_set<LLImageGL*> LLImageGL::sImageList;


bool LLImageGLThread::sEnabledTextures = false;
bool LLImageGLThread::sEnabledMedia = false;

//****************************************************************************************************
//The below for texture auditing use only
//****************************************************************************************************
//-----------------------
//debug use
S32 LLImageGL::sCurTexSizeBar = -1 ;
S32 LLImageGL::sCurTexPickSize = -1 ;
S32 LLImageGL::sMaxCategories = 1 ;

//optimization for when we don't need to calculate mIsMask
bool LLImageGL::sSkipAnalyzeAlpha;


//------------------------
//****************************************************************************************************
//End for texture auditing use only
//****************************************************************************************************

//**************************************************************************************
//below are functions for debug use
//do not delete them even though they are not currently being used.

void LLImageGL::checkTexSize(bool forced) const
{
    if ((forced || gDebugGL) && mTarget == GL_TEXTURE_2D)
    {
        {
            //check viewport
            GLint vp[4] ;
            glGetIntegerv(GL_VIEWPORT, vp) ;
        }

        GLint texname;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texname);
        bool error = false;
        if (texname != mTexName)
        {
            LL_INFOS() << "Bound: " << texname << " Should bind: " << mTexName << " Default: " << (sDefaultGLTexture ? sDefaultGLTexture->getTexName() : 0 ) << LL_ENDL;

            error = true;
            if (gDebugSession)
            {
                gFailLog << "Invalid texture bound!" << std::endl;
            }
            else
            {
                LL_ERRS() << "Invalid texture bound!" << LL_ENDL;
            }
        }
        stop_glerror() ;
        // Immutable allocation records the dimensions at the only point they
        // can change. Use that owner instead of querying the GL object back.
        const LLGLint x = mStorage.mWidth;
        const LLGLint y = mStorage.mHeight;

        if(!mStorageAllocated || !x || !y)
        {
            return ;
        }
        // Clamp like getWidth/getHeight: mCurrentDiscardLevel can be -1
        // when no upload has happened yet; shifting by a negative value
        // is UB. Treat "no discard set" as full resolution.
        const S32 cur_discard = llmax<S32>(mCurrentDiscardLevel, 0);
        if(x != (mWidth >> cur_discard) || y != (mHeight >> cur_discard))
        {
            error = true;
            if (gDebugSession)
            {
                gFailLog << "wrong texture size and discard level!" <<
                    mWidth << " Height: " << mHeight << " Current Level: " << (S32)mCurrentDiscardLevel << std::endl;
            }
            else
            {
                LL_ERRS() << "wrong texture size and discard level: width: " <<
                    mWidth << " Height: " << mHeight << " Current Level: " << (S32)mCurrentDiscardLevel << LL_ENDL ;
            }
        }

        if (error)
        {
            ll_fail("LLImageGL::checkTexSize failed.");
        }
    }
}
//end of debug functions
//**************************************************************************************

//----------------------------------------------------------------------------
constexpr bool is_little_endian()
{
    return std::endian::native == std::endian::little;
}

#if LL_DARWIN
void LLImageGL::setMetalDevice(ALMetalDevice* device)
{
    if (device != sImageMetalDevice)
    {
        // The cached pipelines and sampler retain objects from their creating
        // device. Drop them before that borrowed device pointer can expire.
        sMetalTextureResampler.reset();
    }
    sImageMetalDevice = device;
}

ALMetalDevice* LLImageGL::getMetalDevice() noexcept
{
    return sImageMetalDevice;
}

void LLImageGL::setMetalTextureAuthority(bool enabled) noexcept
{
    sMetalTextureAuthority = enabled;
}

bool LLImageGL::isMetalTextureAuthority() noexcept
{
    return sMetalTextureAuthority;
}
#endif

//static
void LLImageGL::initClass(LLWindow* window, S32 num_catagories, bool skip_analyze_alpha /* = false */, bool thread_texture_loads /* = false */, bool thread_media_updates /* = false */)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    sSkipAnalyzeAlpha = skip_analyze_alpha;

    if (thread_texture_loads || thread_media_updates)
    {
        LLImageGLThread::sEnabledTextures = thread_texture_loads;
        LLImageGLThread::sEnabledMedia = thread_media_updates;
        // The legacy worker clears both again if it gets no shared context;
        // Metal-authoritative Darwin uploads need no GL context.
        LLImageGLThread::createInstance(window);
    }
}

//static
void LLImageGL::cleanupClass()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    LLImageGLThread::deleteSingleton();
    // The names pooled on this context are the context's; the next one
    // starts afresh.
    sTextureNameGeneration.fetch_add(1, std::memory_order_relaxed);

    const bool gl_alive = gGLManager.mInited;

    // Uploads handed over and never published. Their thread is gone, so discard the
    // staged native generation and its callbacks.
    for (PendingPublish& pending : sPendingPublishes)
    {
        pending.mImage->mOnPublished.clear();
        pending.mImage->mPublishesPending = 0;
        pending.mImage->endUpload();
    }
    sPendingPublishes.clear();

    // Drain the deferred-delete ring so leftover texture names don't dangle
    // in sTextureAllocs across re-init (the test fixture calls cleanupClass
    // between tests). Always clear the C++ bookkeeping; only issue
    // glDeleteTextures if GL is still around.
    for (S32 i = 0; i < DELETE_DELAY + 1; ++i)
    {
        if (!sFreeList[i].empty())
        {
            free_tex_images((GLsizei)sFreeList[i].size(), sFreeList[i].data());
            if (gl_alive)
            {
                glDeleteTextures((GLsizei)sFreeList[i].size(), sFreeList[i].data());
            }
            sFreeList[i].resize(0);
        }
    }

}


namespace
{
    // An integer internal format's pixel format and sign; a pixel format of 0 for any other format. isIntegerFormat,
    // isSignedIntegerFormat and integerPixelFormat all answer from it, so the three cannot disagree.
    struct IntegerFormatInfo
    {
        U32  mPixelFormat = 0;
        bool mSigned      = false;
    };

    IntegerFormatInfo integer_format_info(S32 internal_format)
    {
        switch (internal_format)
        {
        case GL_R8I:
        case GL_R16I:
        case GL_R32I:
            return { GL_RED_INTEGER, true };
        case GL_R8UI:
        case GL_R16UI:
        case GL_R32UI:
            return { GL_RED_INTEGER, false };
        case GL_RG8I:
        case GL_RG16I:
        case GL_RG32I:
            return { GL_RG_INTEGER, true };
        case GL_RG8UI:
        case GL_RG16UI:
        case GL_RG32UI:
            return { GL_RG_INTEGER, false };
        case GL_RGB8I:
        case GL_RGB16I:
        case GL_RGB32I:
            return { GL_RGB_INTEGER, true };
        case GL_RGB8UI:
        case GL_RGB16UI:
        case GL_RGB32UI:
            return { GL_RGB_INTEGER, false };
        case GL_RGBA8I:
        case GL_RGBA16I:
        case GL_RGBA32I:
            return { GL_RGBA_INTEGER, true };
        case GL_RGBA8UI:
        case GL_RGBA16UI:
        case GL_RGBA32UI:
        case GL_RGB10_A2UI:
            return { GL_RGBA_INTEGER, false };
        default:
            return {};
        }
    }
}

//static
bool LLImageGL::isIntegerFormat(S32 internal_format)
{
    return integer_format_info(internal_format).mPixelFormat != 0;
}

//static
bool LLImageGL::isSignedIntegerFormat(S32 internal_format)
{
    return integer_format_info(internal_format).mSigned;
}

//static
U32 LLImageGL::integerPixelFormat(S32 internal_format)
{
    return integer_format_info(internal_format).mPixelFormat;
}

// Whether an internal format is *sized*, i.e. legal for glTexStorage2D. Unsized
// formats (GL_RGBA, GL_RGB, GL_RED, the legacy GL_LUMINANCE family, generic
// GL_COMPRESSED_*) let the driver choose the actual storage, which immutable
// allocation cannot express -- glTexStorage2D rejects them with GL_INVALID_ENUM.
//
// Whitelist rather than blacklist: an unrecognised format trips the assertion below
// rather than being quietly handed to glTexStorage2D.
static bool isSizedInternalFormat(S32 intformat)
{
    switch (intformat)
    {
    case GL_R8:
    case GL_RG8:
    case GL_RGB8:
    case GL_RGBA8:
    case GL_SRGB8:
    case GL_SRGB8_ALPHA8:
    case GL_RGB10_A2:
    case GL_R11F_G11F_B10F:
    case GL_RGBA16:
    case GL_R16F:
    case GL_RG16F:
    case GL_RGB16F:
    case GL_RGBA16F:
    case GL_R32F:
    case GL_RG32F:
    case GL_RGB32F:
    case GL_RGBA32F:
    // Depth and depth-stencil, as used by render target attachments. GL_DEPTH_COMPONENT
    // on its own is unsized and is correctly rejected below.
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32:
    case GL_DEPTH_COMPONENT32F:
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
    // Block-compressed formats are sized and glTexStorage2D accepts them; their uploads
    // just have to go through glCompressedTexSubImage2D rather than glTexSubImage2D.
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        return true;
    default:
        // Every integer format is sized.
        return LLImageGL::isIntegerFormat(intformat);
    }
}

// The preconditions glTexStorage2D imposes, asserted rather than branched on.
//
// There is no mutable path any more. Immutable storage is required alongside GL 4.1 (see
// LLGLManager::initExtensions), so nothing here chooses between two ways of allocating --
// it catches a call site that would violate the requirement. Both cases below used to
// report and silently fall back to glTexImage2D; a session exercising the render paths hit
// neither, which is what made removing the fallback possible.
//
// Warned as well as asserted, because llassert compiles out unless SHOW_ASSERT is defined
// and a plain Release build does not define it -- so on the builds that actually ship, the
// assertion alone would say nothing and the only symptom would be a GL error and a texture
// that never got storage. The warning is once-per-site and costs nothing on the path where
// the invariant holds.
//
// The failure is deliberately not degraded: a texture that cannot be allocated immutably
// cannot be expressed in D3D11/12 or Vulkan either, so there is nothing to fall back to.
static void assertStorageAllocatable(U32 target, S32 intformat)
{
    // A non-2D target needs its own storage owner and must not arrive at the
    // generic 2D allocator.
    if (target != GL_TEXTURE_2D)
    {
        LL_WARNS_ONCE("RenderInit") << "Non-2D target 0x" << std::hex << target << std::dec
                                    << " reached the 2D allocator."
                                    << LL_ENDL;
        llassert(false);
    }

    // glTexStorage2D cannot express a format that lets the driver pick the storage, and
    // D3D11 has no unsized formats at all. Name a sized one (GL_RGBA8, not GL_RGBA).
    if (!isSizedInternalFormat(intformat))
    {
        LL_WARNS_ONCE("RenderInit") << "Internal format 0x" << std::hex << intformat << std::dec
                                    << " is unsized, so glTexStorage2D cannot express it. Name "
                                    << "a sized format (GL_RGBA8, not GL_RGBA)." << LL_ENDL;
        llassert(false);
    }
}


//static
S32 LLImageGL::dataFormatBits(S32 dataformat)
{
    switch (dataformat)
    {
    case GL_COMPRESSED_RED:                         return 8;
    case GL_COMPRESSED_RG:                          return 16;
    case GL_COMPRESSED_RGB:                         return 24;
    case GL_COMPRESSED_SRGB:                        return 24;
    case GL_COMPRESSED_RGBA:                        return 32;
    case GL_COMPRESSED_SRGB_ALPHA:                  return 32;
    case GL_COMPRESSED_LUMINANCE:                   return 8;
    case GL_COMPRESSED_LUMINANCE_ALPHA:             return 16;
    case GL_COMPRESSED_ALPHA:                       return 8;
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:          return 4;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:    return 4;
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:          return 8;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:    return 8;
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:          return 8;
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:    return 8;
    case GL_LUMINANCE:                              return 8;
    case GL_LUMINANCE8:                             return 8;
    case GL_ALPHA:                                  return 8;
    case GL_ALPHA8:                                 return 8;
    case GL_RED:                                    return 8;
    case GL_R8:                                     return 8;
    case GL_COLOR_INDEX:                            return 8;
    case GL_LUMINANCE_ALPHA:                        return 16;
    case GL_LUMINANCE8_ALPHA8:                      return 16;
    case GL_RG:                                     return 16;
    case GL_RG8:                                    return 16;
    case GL_RGB:                                    return 24;
    case GL_SRGB:                                   return 24;
    case GL_RGB8:                                   return 24;
    case GL_SRGB8:                                  return 24;
    case GL_R11F_G11F_B10F:                         return 32;
    case GL_RGBA:                                   return 32;
    case GL_RGBA8:                                  return 32;
    case GL_RGB10_A2:                               return 32;
    case GL_SRGB_ALPHA:                             return 32;
    case GL_SRGB8_ALPHA8:                           return 32;
    case GL_BGRA:                                   return 32;      // Used for QuickTime media textures on the Mac
    case GL_DEPTH_COMPONENT:                        return 24;
    case GL_DEPTH_COMPONENT24:                      return 24;
    case GL_DEPTH_COMPONENT32F:                     return 32;
    case GL_DEPTH24_STENCIL8:                       return 32;
    case GL_DEPTH32F_STENCIL8:                      return 64; // 32 for depth, 8 for stencil, stencil is still allocated as 24+8 internally GL_FLOAT_32_UNSIGNED_INT_24_8_REV
    case GL_RGBA16:                                 return 64;
    case GL_R16F:                                   return 16;
    case GL_RG16F:                                  return 32;
    case GL_RGB16F:                                 return 48;
    case GL_RGBA16F:                                return 64;
    case GL_R32F:                                   return 32;
    case GL_RG32F:                                  return 64;
    case GL_RGB32F:                                 return 96;
    case GL_RGBA32F:                                return 128;
    case GL_R8I:                                    return 8;
    case GL_R8UI:                                   return 8;
    case GL_R16I:                                   return 16;
    case GL_R16UI:                                  return 16;
    case GL_R32I:                                   return 32;
    case GL_R32UI:                                  return 32;
    case GL_RG8I:                                   return 16;
    case GL_RG8UI:                                  return 16;
    case GL_RG16I:                                  return 32;
    case GL_RG16UI:                                 return 32;
    case GL_RG32I:                                  return 64;
    case GL_RG32UI:                                 return 64;
    case GL_RGB8I:                                  return 24;
    case GL_RGB8UI:                                 return 24;
    case GL_RGB16I:                                 return 48;
    case GL_RGB16UI:                                return 48;
    case GL_RGB32I:                                 return 96;
    case GL_RGB32UI:                                return 96;
    case GL_RGBA8I:                                 return 32;
    case GL_RGBA8UI:                                return 32;
    case GL_RGBA16I:                                return 64;
    case GL_RGBA16UI:                               return 64;
    case GL_RGBA32I:                                return 128;
    case GL_RGBA32UI:                               return 128;
    case GL_RGB10_A2UI:                             return 32;
    default:
        LL_ERRS() << "LLImageGL::Unknown format: " << std::hex << dataformat << std::dec << LL_ENDL;
        return 0;
    }
}

//static
S64 LLImageGL::dataFormatBytes(S32 dataformat, S32 width, S32 height)
{
    switch (dataformat)
    {
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        if (width < 4) width = 4;
        if (height < 4) height = 4;
        break;
    default:
        break;
    }
    S64 bytes (((S64)width * (S64)height * (S64)dataFormatBits(dataformat)+7)>>3);
    S64 aligned = (bytes+3)&~3;
    return aligned;
}

//static
S32 LLImageGL::dataFormatVRAMBits(S32 dataformat)
{
    // For formats where driver-side storage diverges from the tight
    // host layout, return the padded width. Everything else delegates
    // to dataFormatBits so adding a new format to the host table also
    // covers VRAM accounting by default.
    switch (dataformat)
    {
    case GL_RGB8:                   return 32;  // padded to RGBX
    case GL_SRGB8:                  return 32;  // padded, as GL_RGB8
    case GL_RGB8I:                  return 32;  // padded, as GL_RGB8
    case GL_RGB8UI:                 return 32;  // padded, as GL_RGB8
    case GL_RGB16F:                 return 64;  // padded to RGBA16F
    case GL_RGB16I:                 return 64;  // padded, as GL_RGB16F
    case GL_RGB16UI:                return 64;  // padded, as GL_RGB16F
    case GL_RGB32F:                 return 128; // padded to RGBA32F
    case GL_RGB32I:                 return 128; // padded, as GL_RGB32F
    case GL_RGB32UI:                return 128; // padded, as GL_RGB32F
    case GL_DEPTH_COMPONENT24:      return 32;  // padded to 32-bit
    default:                        return dataFormatBits(dataformat);
    }
}

//static
S64 LLImageGL::dataFormatVRAMBytes(S32 dataformat, S32 width, S32 height)
{
    switch (dataformat)
    {
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        if (width < 4) width = 4;
        if (height < 4) height = 4;
        break;
    default:
        break;
    }
    S64 bytes (((S64)width * (S64)height * (S64)dataFormatVRAMBits(dataformat)+7)>>3);
    S64 aligned = (bytes+3)&~3;
    return aligned;
}

//static
S32 LLImageGL::dataFormatComponents(S32 dataformat)
{
    switch (dataformat)
    {
      case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:    return 3;
      case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT: return 3;
      case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:    return 4;
      case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT: return 4;
      case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:    return 4;
      case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: return 4;
      case GL_LUMINANCE:                        return 1;
      case GL_ALPHA:                            return 1;
      case GL_RED:                              return 1;
      case GL_COLOR_INDEX:                      return 1;
      case GL_LUMINANCE_ALPHA:                  return 2;
      case GL_RG:                               return 2;
      case GL_RGB:                              return 3;
      case GL_SRGB:                             return 3;
      case GL_RGBA:                             return 4;
      case GL_SRGB_ALPHA:                       return 4;
      case GL_BGRA:                             return 4;       // Used for QuickTime media textures on the Mac
      case GL_RED_INTEGER:                      return 1;
      case GL_RG_INTEGER:                       return 2;
      case GL_RGB_INTEGER:                      return 3;
      case GL_BGR_INTEGER:                      return 3;
      case GL_RGBA_INTEGER:                     return 4;
      case GL_BGRA_INTEGER:                     return 4;
      default:
        LL_ERRS() << "LLImageGL::Unknown format: " << std::hex << dataformat << std::dec << LL_ENDL;
        return 0;
    }
}

//----------------------------------------------------------------------------

// static
void LLImageGL::updateStats(F64 current_time)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    sLastFrameTime = current_time;

    // Per-frame counters. sBindCount/sUniqueCount are labelled per-frame and read that way by
    // the HUD, but went years without this reset -- they were lifetime totals.
    sBindCount   = 0;
    sUniqueCount = 0;

    ALTextureSlot::sSamplerBinds = 0;
    ALTextureSlot::sSamplerSkips = 0;
    ALTextureSlot::sTextureBinds = 0;
    ALTextureSlot::sSamplerBindsFlushed = 0;
    ALTextureSlot::sSamplerBindsSplitBatch = 0;
}

//----------------------------------------------------------------------------

//static
void LLImageGL::destroyGL()
{
    for (S32 stage = 0; stage < gGLManager.mNumTextureImageUnits; stage++)
    {
        gGL.getTextureSlot(stage)->unbind();
    }
    // What follows is a new context, whose names these were not.
    sTextureNameGeneration.fetch_add(1, std::memory_order_relaxed);
}

//static
//----------------------------------------------------------------------------

//for server side use only.
//static
bool LLImageGL::create(LLPointer<LLImageGL>& dest, bool usemipmaps)
{
    dest = new LLImageGL(usemipmaps);
    return true;
}

//for server side use only.
bool LLImageGL::create(LLPointer<LLImageGL>& dest, U32 width, U32 height, U8 components, bool usemipmaps)
{
    dest = new LLImageGL(width, height, components, usemipmaps);
    return true;
}

//----------------------------------------------------------------------------

LLImageGL::LLImageGL(bool usemipmaps/* = true*/)
:   mSaveData(0)
{
    init(usemipmaps);
    setSize(0, 0, 0);
    sImageList.insert(this);
    sCount++;
}

LLImageGL::LLImageGL(U32 width, U32 height, U8 components, bool usemipmaps/* = true*/)
:   mSaveData(0)
{
    llassert( components <= 4 );
    init(usemipmaps);
    setSize(width, height, components);
    sImageList.insert(this);
    sCount++;
}

LLImageGL::LLImageGL(const LLImageRaw* imageraw, bool usemipmaps/* = true*/)
:   mSaveData(0)
{
    init(usemipmaps);
    setSize(0, 0, 0);
    sImageList.insert(this);
    sCount++;

    if (!createMetalTexture(0, imageraw))
    {
        LL_WARNS("Metal") << "Could not publish constructor image to native texture storage"
                           << LL_ENDL;
    }
}

LLImageGL::~LLImageGL()
{
    // Always run the C++ bookkeeping (sImageList/sCount/freePickMask)
    // even if GL is gone. The previous gate on gGLManager.mInited left
    // stale `this` pointers in sImageList when an LLImageGL outlived GL
    // teardown — a later iteration (e.g. dirtyTexOptions) would UAF.
    // cleanup() is C++-safe on its own: destroyGLTexture is gated on
    // mIsDisabled, and deleteTextures no-ops when mInited is false.
    cleanup();
    sImageList.erase(this);
    freePickMask();
    sCount--;
}

void LLImageGL::init(bool usemipmaps)
{
#if LL_IMAGEGL_THREAD_CHECK
    mActiveThread = LLThread::currentID();
#endif

    // keep these members in the same order as declared in llimagehl.h
    // so that it is obvious by visual inspection if we forgot to
    // init a field.

    mTextureMemory = S64Bytes(0);
    mLastBindTime = 0.f;

    mPickMask = NULL;
    mPickMaskWidth = 0;
    mPickMaskHeight = 0;
    mUseMipMaps = usemipmaps;
    mHasExplicitFormat = false;
    mFormatComponents = -1;

    mIsMask = false;
    mMaxAlpha = 255;
    mMinAlpha = 0;
    mNeedsAlphaAndPickMask = true ;
    mAlphaAnalysisSerial = 0;
    mAlphaStride = 0 ;
    mAlphaOffset = 0 ;

    mGLTextureCreated = false ;
    mTexName = 0;
#if LL_DARWIN
    mMetalTexture.reset();
    mPendingMetalTexture.reset();
    mAccountedMetalTextureBytes = 0;
#endif
    mWidth = 0;
    mHeight = 0;
    mCurrentDiscardLevel = -1;

    mTarget = GL_TEXTURE_2D;
    mBindTarget = ALTextureSlot::TT_TEXTURE;
    mHasMipMaps = false;
    mMipLevels = 0;

    mIsResident = 0;

    mComponents = 0;
    mMaxDiscardLevel = MAX_DISCARD_LEVEL;

    mFormatInternal = -1;
    mFormatPrimary = (LLGLenum) 0;
    mFormatType = GL_UNSIGNED_BYTE;
    mFormatSwapBytes = false;

    mDeprecatedSourceFormat = 0;

#ifdef DEBUG_MISS
    mMissed = false;
#endif

    mCategory = -1;

    // Sometimes we have to post work for the main thread.
    mMainQueue = LL::WorkQueue::getInstance("mainloop");
}

void LLImageGL::cleanup()
{
    if (!gGLManager.mIsDisabled)
    {
        destroyGLTexture();
    }
#if LL_DARWIN
    clearMetalTextureAccounting();
    mMetalTexture.reset();
    mPendingMetalTexture.reset();
#endif
    freePickMask();
    discardPendingAlpha();

    mSaveData = NULL; // deletes data
}

//----------------------------------------------------------------------------

//static
bool LLImageGL::checkSize(S32 width, S32 height)
{
    if (width < 0 || height < 0)
    {
        return false;
    }
    return true;
}

bool LLImageGL::setSize(S32 width, S32 height, S32 ncomponents, S32 discard_level)
{
    if (width != mWidth || height != mHeight || ncomponents != mComponents)
    {
        // checkSize only rejects negative dimensions today (NPOT support
        // is universal in the GL versions we target); the comment used
        // to claim a power-of-two check that doesn't actually run.
        if (!checkSize(width, height))
        {
            LL_WARNS() << llformat("Texture has negative dimension: %dx%d",width,height) << LL_ENDL;
            return false;
        }

        mWidth = width;
        mHeight = height;
        mComponents = ncomponents;
        if (ncomponents > 0)
        {
            mMaxDiscardLevel = 0;
            while (width > 1 && height > 1 && mMaxDiscardLevel < MAX_DISCARD_LEVEL)
            {
                mMaxDiscardLevel++;
                width >>= 1;
                height >>= 1;
            }

            if(discard_level > 0)
            {
                mMaxDiscardLevel = llmax(mMaxDiscardLevel, (S8)discard_level);
            }
        }
        else
        {
            mMaxDiscardLevel = MAX_DISCARD_LEVEL;
        }
    }

    return true;
}

//----------------------------------------------------------------------------

// virtual
void LLImageGL::dump()
{
    LL_INFOS() << "mMaxDiscardLevel " << S32(mMaxDiscardLevel)
            << " mLastBindTime " << mLastBindTime
            << " mTarget " << S32(mTarget)
            << " mBindTarget " << S32(mBindTarget)
            << " mUseMipMaps " << S32(mUseMipMaps)
            << " mHasMipMaps " << S32(mHasMipMaps)
            << " mCurrentDiscardLevel " << S32(mCurrentDiscardLevel)
            << " mFormatInternal " << S32(mFormatInternal)
            << " mFormatPrimary " << S32(mFormatPrimary)
            << " mFormatType " << S32(mFormatType)
            << " mFormatSwapBytes " << S32(mFormatSwapBytes)
            << " mHasExplicitFormat " << S32(mHasExplicitFormat)
#if DEBUG_MISS
            << " mMissed " << mMissed
#endif
            << LL_ENDL;

    LL_INFOS() << " mTextureMemory " << mTextureMemory
            << " mTexNames " << mTexName
            << " mIsResident " << S32(mIsResident)
            << LL_ENDL;
}

//----------------------------------------------------------------------------
void LLImageGL::forceUpdateBindStats(void) const
{
    mLastBindTime = sLastFrameTime;
}

bool LLImageGL::updateBindStats() const
{
    if (mTexName != 0)
    {
#ifdef DEBUG_MISS
        mMissed = ! getIsResident(true);
#endif
        sBindCount++;
        if (mLastBindTime != sLastFrameTime)
        {
            // we haven't accounted for this texture yet this frame
            sUniqueCount++;
            mLastBindTime = sLastFrameTime;

            return true ;
        }
    }
    return false ;
}

F32 LLImageGL::getTimePassedSinceLastBound()
{
    return (F32)(sLastFrameTime - mLastBindTime);
}

void LLImageGL::setExplicitFormat( LLGLint internal_format, LLGLenum primary_format, LLGLenum type_format, bool swap_bytes )
{
    // Note: must be called before createTexture()
    // Note: it's up to the caller to ensure that the format matches the number of components.
    mHasExplicitFormat = true;
    mFormatComponents = -1;
    mFormatInternal = internal_format;
    mFormatPrimary = primary_format;
    if(type_format == 0)
        mFormatType = GL_UNSIGNED_BYTE;
    else
        mFormatType = type_format;
    mFormatSwapBytes = swap_bytes;

    // GL neither builds nor filters an integer texture's mips, and the box filter setImage builds them with by hand
    // would average the bits of its values: an integer texture holds level 0 alone.
    if (isIntegerFormat(internal_format))
    {
        mUseMipMaps = false;
    }

    // Order matters: alpha-stride/offset depends on the deprecated format
    // names (LUMINANCE_ALPHA → stride 2, etc.). Compute that first, then
    // rewrite to core-profile-valid forms.
    calcAlphaChannelOffsetAndStride() ;
    resolveDeprecatedFormat();
}

//----------------------------------------------------------------------------

static U32 pixel_bytes(U32 pixformat, U32 pixtype);

#if LL_DARWIN
bool LLImageGL::createMetalTexture(S32 discard_level, const LLImageRaw* image,
                                   S32 category)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    checkActiveThread();
    checkUploadBegun();

    const bool valid_upload_thread =
        on_main_thread() || (sOnImageGLThread && mUploadInFlight);
    if (!valid_upload_thread || !image || image->isBufferInvalid() || !sImageMetalDevice ||
        mTarget != GL_TEXTURE_2D)
    {
        return false;
    }

    if (discard_level < 0)
    {
        if (mCurrentDiscardLevel < 0)
        {
            return false;
        }
        discard_level = mCurrentDiscardLevel;
    }
    discard_level = llclamp(discard_level, 0, MAX_DISCARD_LEVEL);

    const S32 width = image->getWidth() << discard_level;
    const S32 height = image->getHeight() << discard_level;
    if (!setSize(width, height, image->getComponents(), discard_level))
    {
        LL_WARNS("Metal") << "Trying to create a native texture with incorrect dimensions"
                           << LL_ENDL;
        return false;
    }

    return createMetalTexture(discard_level, image->getData(), category);
}

bool LLImageGL::createMetalTexture(S32 discard_level, const U8* data,
                                   S32 category)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    checkActiveThread();
    checkUploadBegun();

    const bool valid_upload_thread =
        on_main_thread() || (sOnImageGLThread && mUploadInFlight);
    if (!valid_upload_thread || !data || !sImageMetalDevice ||
        mTarget != GL_TEXTURE_2D || mWidth <= 0 || mHeight <= 0 ||
        mComponents <= 0)
    {
        return false;
    }

    if (discard_level < 0)
    {
        if (mCurrentDiscardLevel < 0)
        {
            return false;
        }
        discard_level = mCurrentDiscardLevel;
    }
    discard_level = llclamp(discard_level, 0, (S32)mMaxDiscardLevel);
    discard_level = llmin(discard_level, MAX_DISCARD_LEVEL);
    const bool staged_upload = mUploadInFlight;

    if (mHasExplicitFormat &&
        ((mFormatPrimary == GL_RGBA && mComponents < 4) ||
         (mFormatPrimary == GL_RGB && mComponents < 3)))
    {
        LL_WARNS("Metal") << "Incorrect native texture format: " << std::hex
                           << mFormatPrimary << ", components: " << std::dec
                           << static_cast<U32>(mComponents) << LL_ENDL;
        mHasExplicitFormat = false;
    }
    if (!mHasExplicitFormat)
    {
        resolveComponentFormat(mComponents);
    }

    // Direct native producers retire their previous storage immediately. A fetched-image
    // replacement brackets this call with beginUpload(): build into pending native
    // storage and keep the lower-resolution GL/Metal generation authoritative until the
    // upload has committed and publishMetalTextureAuthoritative() performs the handoff.
    if (!staged_upload && mTexName != 0)
    {
        free_tex_image(mTexName);
    }
    if (!staged_upload)
    {
        destroyGLTexture();
        mStorageAllocated = false;
        mStorage = StorageDesc();
    }
    else
    {
        mPendingMetalTexture.reset();
    }
    mCurrentDiscardLevel = discard_level;
    mHasMipMaps = mUseMipMaps;
    const U32 width = static_cast<U32>(liveWidth(mCurrentDiscardLevel));
    const U32 height = static_cast<U32>(liveHeight(mCurrentDiscardLevel));
    mMipLevels = mUseMipMaps
        ? calcMipLevelCount(static_cast<S32>(width), static_cast<S32>(height))
        : 1;
    setCategory(category);

    std::unique_ptr<ALMetalCommandBuffer> command_owner;
    ALMetalCommandBuffer* command = nullptr;
    if (!prepareMetalTexture(command_owner, command, true) || !command ||
        !metalTextureForUpload() ||
        !uploadMetalTextureRegion(*command, 0, 0, 0, width, height,
                                  data, metalSourceBytesPerRow(width)) ||
        (mUseMipMaps && !generateMetalMipmaps(*command)) ||
        !commitMetalUpload(command_owner, command))
    {
        if (staged_upload)
        {
            mPendingMetalTexture.reset();
        }
        else
        {
            mMetalTexture.reset();
            clearMetalTextureAccounting();
            mTextureMemory = S64Bytes(0);
        }
        return false;
    }

    analyzeAlpha(data, width, height);
    updatePickMask(width, height, data);
    touchContents();
    ALMetalTexture* const created_texture = metalTextureForUpload();
    llassert(created_texture);
    mTextureMemory = S64Bytes(metal_texture_bytes(*created_texture));
    if (!staged_upload)
    {
        updateMetalTextureAccounting();
        mLastBindTime = sLastFrameTime;
        mGLTextureCreated = false;
        llassert(mTexName == 0);
    }
    return true;
}

bool LLImageGL::createMetalTexture(
    S32 discard_level,
    std::span<const MetalMipLevel> mip_levels,
    S32 category)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    checkActiveThread();
    checkUploadBegun();

    if (!on_main_thread() || !sImageMetalDevice ||
        mTarget != GL_TEXTURE_2D || !mUseMipMaps || mWidth <= 0 ||
        mHeight <= 0 || mComponents <= 0 || mip_levels.empty())
    {
        return false;
    }

    if (discard_level < 0)
    {
        if (mCurrentDiscardLevel < 0)
        {
            return false;
        }
        discard_level = mCurrentDiscardLevel;
    }
    discard_level = llclamp(discard_level, 0, (S32)mMaxDiscardLevel);
    discard_level = llmin(discard_level, MAX_DISCARD_LEVEL);

    if (mHasExplicitFormat &&
        ((mFormatPrimary == GL_RGBA && mComponents < 4) ||
         (mFormatPrimary == GL_RGB && mComponents < 3)))
    {
        LL_WARNS("Metal") << "Incorrect native texture format: " << std::hex
                           << mFormatPrimary << ", components: " << std::dec
                           << static_cast<U32>(mComponents) << LL_ENDL;
        mHasExplicitFormat = false;
    }
    if (!mHasExplicitFormat)
    {
        resolveComponentFormat(mComponents);
    }

    const U32 width = static_cast<U32>(liveWidth(discard_level));
    const U32 height = static_cast<U32>(liveHeight(discard_level));
    const S32 expected_levels = calcMipLevelCount(
        static_cast<S32>(width), static_cast<S32>(height));
    if (mip_levels.size() != static_cast<std::size_t>(expected_levels))
    {
        LL_WARNS("Metal") << "Native authored mip chain has "
                           << mip_levels.size() << " level(s), expected "
                           << expected_levels << LL_ENDL;
        return false;
    }

    U32 level_width = width;
    U32 level_height = height;
    for (const MetalMipLevel& level : mip_levels)
    {
        if (!level.mData || level.mWidth != level_width ||
            level.mHeight != level_height)
        {
            LL_WARNS("Metal") << "Native authored mip has invalid geometry; expected "
                               << level_width << 'x' << level_height << LL_ENDL;
            return false;
        }
        level_width = llmax<U32>(1, level_width >> 1);
        level_height = llmax<U32>(1, level_height >> 1);
    }

    // Publish only a complete native generation: stale GL/native storage is retired before
    // the replacement allocation, and any encode/commit failure clears the partial result.
    if (mTexName != 0)
    {
        free_tex_image(mTexName);
    }
    destroyGLTexture();
    mStorageAllocated = false;
    mStorage = StorageDesc();
    mCurrentDiscardLevel = discard_level;
    mHasMipMaps = true;
    mMipLevels = expected_levels;
    setCategory(category);

    std::unique_ptr<ALMetalCommandBuffer> command_owner;
    ALMetalCommandBuffer* command = nullptr;
    if (!prepareMetalTexture(command_owner, command, true) || !command ||
        !metalTextureForUpload())
    {
        mMetalTexture.reset();
        mPendingMetalTexture.reset();
        clearMetalTextureAccounting();
        mTextureMemory = S64Bytes(0);
        return false;
    }

    for (U32 mip = 0; mip < static_cast<U32>(mip_levels.size()); ++mip)
    {
        const MetalMipLevel& level = mip_levels[mip];
        if (!uploadMetalTextureRegion(
                *command, mip, 0, 0, level.mWidth, level.mHeight,
                level.mData, metalSourceBytesPerRow(level.mWidth)))
        {
            mMetalTexture.reset();
            mPendingMetalTexture.reset();
            clearMetalTextureAccounting();
            mTextureMemory = S64Bytes(0);
            return false;
        }
    }
    if (!commitMetalUpload(command_owner, command))
    {
        mMetalTexture.reset();
        mPendingMetalTexture.reset();
        clearMetalTextureAccounting();
        mTextureMemory = S64Bytes(0);
        return false;
    }

    const MetalMipLevel& base = mip_levels.front();
    analyzeAlpha(base.mData, base.mWidth, base.mHeight);
    updatePickMask(base.mWidth, base.mHeight, base.mData);
    touchContents();
    llassert(mMetalTexture);
    mTextureMemory = S64Bytes(metal_texture_bytes(*mMetalTexture));
    updateMetalTextureAccounting();
    mLastBindTime = sLastFrameTime;
    mGLTextureCreated = false;
    llassert(mTexName == 0);
    return true;
}

bool LLImageGL::uploadMetalSubImage(const U8* data, S32 data_width,
                                    S32 data_height, S32 x, S32 y,
                                    S32 width, S32 height)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    checkActiveThread();
    checkUploadBegun();

    if (width == 0 || height == 0)
    {
        return true;
    }
    if (!on_main_thread() || !data || !sImageMetalDevice ||
        !metalTextureForUpload() ||
        mTarget != GL_TEXTURE_2D || mUseMipMaps ||
        mCurrentDiscardLevel < 0 || data_width <= 0 || data_height <= 0 ||
        x < 0 || y < 0 || width < 0 || height < 0 ||
        x + width > data_width || y + height > data_height ||
        x + width > liveWidth(mCurrentDiscardLevel) ||
        y + height > liveHeight(mCurrentDiscardLevel))
    {
        return false;
    }

    const std::size_t source_pixel_bytes =
        pixel_bytes(mFormatPrimary, mFormatType);
    const U8* source = data +
        (static_cast<std::size_t>(y) * data_width + x) * source_pixel_bytes;
    std::string error;
    std::unique_ptr<ALMetalCommandBuffer> command =
        sImageMetalDevice->makeCommandBuffer(
            "Alchemy native image subimage upload", &error);
    if (!command)
    {
        LL_WARNS("Metal") << "Could not create LLImageGL native subimage command: "
                           << error << LL_ENDL;
        return false;
    }
    ALMetalCommandBuffer* borrowed = command.get();
    if (!uploadMetalTextureRegion(
            *borrowed, 0, static_cast<U32>(x), static_cast<U32>(y),
            static_cast<U32>(width), static_cast<U32>(height), source,
            metalSourceBytesPerRow(static_cast<U32>(data_width))) ||
        !commitMetalUpload(command, borrowed))
    {
        return false;
    }

    touchContents();
    mLastBindTime = sLastFrameTime;
    return true;
}

bool LLImageGL::uploadMetalImage(const LLImageRaw* image)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    checkActiveThread();
    checkUploadBegun();

    if (!image || image->isBufferInvalid() || !sImageMetalDevice ||
        mTarget != GL_TEXTURE_2D || mUseMipMaps ||
        image->getWidth() <= 0 || image->getHeight() <= 0 ||
        image->getWidth() > liveWidth(mCurrentDiscardLevel) ||
        image->getHeight() > liveHeight(mCurrentDiscardLevel) ||
        image->getComponents() != mComponents)
    {
        return false;
    }

    mHasMipMaps = false;
    mMipLevels = 1;
    std::unique_ptr<ALMetalCommandBuffer> command_owner;
    ALMetalCommandBuffer* command = nullptr;
    if (!prepareMetalTexture(command_owner, command, true) ||
        !command || !metalTextureForUpload())
    {
        return false;
    }

    const U8* pixels = image->getData();
    const U32 width = static_cast<U32>(image->getWidth());
    const U32 height = static_cast<U32>(image->getHeight());
    if (!uploadMetalTextureRegion(*command, 0, 0, 0, width, height,
                                  pixels, metalSourceBytesPerRow(width)) ||
        !commitMetalUpload(command_owner, command))
    {
        return false;
    }

    analyzeAlpha(pixels, width, height);
    updatePickMask(width, height, pixels);
    touchContents();
    if (on_main_thread())
    {
        mLastBindTime = sLastFrameTime;
    }
    return true;
}
#endif

// Bytes one pixel of client data occupies, for stepping through it a row at a time.
//
// A packed type holds the whole pixel in one value, whatever the format's component
// count: GStreamer hands its frames over as GL_BGRA / GL_UNSIGNED_INT_8_8_8_8_REV, which
// is four bytes a pixel, not four components of four bytes each.
static U32 pixel_bytes(U32 pixformat, U32 pixtype)
{
    switch (pixtype)
    {
    case GL_UNSIGNED_INT_8_8_8_8:
    case GL_UNSIGNED_INT_8_8_8_8_REV:
    case GL_UNSIGNED_INT_10_10_10_2:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
        return 4;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
        return 2;
    default:
        break;
    }

    const U32 components = LLImageGL::dataFormatComponents(pixformat);
    switch (pixtype)
    {
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
        return components;
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
    case GL_HALF_FLOAT:
        return components * 2;
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_FLOAT:
        return components * 4;
    default:
        LL_ERRS() << "Unknown type: " << pixtype << LL_ENDL;
        return 0;
    }
}

#if LL_DARWIN
namespace
{
    enum class MetalRgbExpansion
    {
        none,
        rgb8,
        rgb16_float,
        rgb32_float,
    };

    enum class MetalScalarConversion
    {
        none,
        float32_to_float16,
    };

    struct MetalUploadFormat
    {
        ALMetalPixelFormat format = ALMetalPixelFormat::invalid;
        MetalRgbExpansion expansion = MetalRgbExpansion::none;
        MetalScalarConversion scalar_conversion =
            MetalScalarConversion::none;
    };

    MetalUploadFormat metal_upload_format(S32 internal_format, U32 primary_format, U32 type)
    {
        switch (primary_format)
        {
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                return {ALMetalPixelFormat::bc1_rgba};
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
                return {ALMetalPixelFormat::bc1_rgba_srgb};
            case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
                return {ALMetalPixelFormat::bc2_rgba};
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
                return {ALMetalPixelFormat::bc2_rgba_srgb};
            case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                return {ALMetalPixelFormat::bc3_rgba};
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
                return {ALMetalPixelFormat::bc3_rgba_srgb};
            default:
                break;
        }

        if (primary_format == GL_BGRA &&
            (type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_INT_8_8_8_8_REV))
        {
            return {internal_format == GL_SRGB8_ALPHA8
                ? ALMetalPixelFormat::bgra8_unorm_srgb
                : ALMetalPixelFormat::bgra8_unorm};
        }

        switch (internal_format)
        {
            case GL_R8:                 return {ALMetalPixelFormat::r8_unorm};
            case GL_RG8:                return {ALMetalPixelFormat::rg8_unorm};
            case GL_RGB8:               return {ALMetalPixelFormat::rgba8_unorm, MetalRgbExpansion::rgb8};
            case GL_SRGB8:              return {ALMetalPixelFormat::rgba8_unorm_srgb, MetalRgbExpansion::rgb8};
            case GL_RGBA8:              return {ALMetalPixelFormat::rgba8_unorm};
            case GL_SRGB8_ALPHA8:       return {ALMetalPixelFormat::rgba8_unorm_srgb};
            case GL_R16F:
                return {ALMetalPixelFormat::r16_float,
                        MetalRgbExpansion::none,
                        type == GL_FLOAT
                            ? MetalScalarConversion::float32_to_float16
                            : MetalScalarConversion::none};
            case GL_RG16F:
                return {ALMetalPixelFormat::rg16_float,
                        MetalRgbExpansion::none,
                        type == GL_FLOAT
                            ? MetalScalarConversion::float32_to_float16
                            : MetalScalarConversion::none};
            case GL_RGB16F:
                return {ALMetalPixelFormat::rgba16_float,
                        MetalRgbExpansion::rgb16_float,
                        type == GL_FLOAT
                            ? MetalScalarConversion::float32_to_float16
                            : MetalScalarConversion::none};
            case GL_RGBA16F:
                return {ALMetalPixelFormat::rgba16_float,
                        MetalRgbExpansion::none,
                        type == GL_FLOAT
                            ? MetalScalarConversion::float32_to_float16
                            : MetalScalarConversion::none};
            case GL_RGBA16:             return {ALMetalPixelFormat::rgba16_unorm};
            case GL_R32F:               return {ALMetalPixelFormat::r32_float};
            case GL_RG32F:              return {ALMetalPixelFormat::rg32_float};
            case GL_RGB32F:             return {ALMetalPixelFormat::rgba32_float, MetalRgbExpansion::rgb32_float};
            case GL_RGBA32F:            return {ALMetalPixelFormat::rgba32_float};
            case GL_RGB10_A2:           return {ALMetalPixelFormat::rgb10a2_unorm};
            case GL_R11F_G11F_B10F:     return {ALMetalPixelFormat::rg11b10_float};
            case GL_R8I:                return {ALMetalPixelFormat::r8_sint};
            case GL_R8UI:               return {ALMetalPixelFormat::r8_uint};
            case GL_RG8I:               return {ALMetalPixelFormat::rg8_sint};
            case GL_RG8UI:              return {ALMetalPixelFormat::rg8_uint};
            case GL_RGBA8I:             return {ALMetalPixelFormat::rgba8_sint};
            case GL_RGBA8UI:            return {ALMetalPixelFormat::rgba8_uint};
            case GL_R16I:               return {ALMetalPixelFormat::r16_sint};
            case GL_R16UI:              return {ALMetalPixelFormat::r16_uint};
            case GL_RG16I:              return {ALMetalPixelFormat::rg16_sint};
            case GL_RG16UI:             return {ALMetalPixelFormat::rg16_uint};
            case GL_RGBA16I:            return {ALMetalPixelFormat::rgba16_sint};
            case GL_RGBA16UI:           return {ALMetalPixelFormat::rgba16_uint};
            case GL_R32I:               return {ALMetalPixelFormat::r32_sint};
            case GL_R32UI:              return {ALMetalPixelFormat::r32_uint};
            case GL_RG32I:              return {ALMetalPixelFormat::rg32_sint};
            case GL_RG32UI:             return {ALMetalPixelFormat::rg32_uint};
            case GL_RGBA32I:            return {ALMetalPixelFormat::rgba32_sint};
            case GL_RGBA32UI:           return {ALMetalPixelFormat::rgba32_uint};
            case GL_RGB10_A2UI:         return {ALMetalPixelFormat::rgb10a2_uint};
            default:                    return {};
        }
    }

    std::size_t metal_scalar_bytes(U32 type)
    {
        switch (type)
        {
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:  return 1;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
            case GL_HALF_FLOAT:     return 2;
            case GL_INT:
            case GL_UNSIGNED_INT:
            case GL_FLOAT:          return 4;
            default:                return 0;
        }
    }

    std::size_t metal_packed_bytes(U32 type)
    {
        switch (type)
        {
            case GL_UNSIGNED_SHORT_5_6_5:
            case GL_UNSIGNED_SHORT_5_6_5_REV:
            case GL_UNSIGNED_SHORT_4_4_4_4:
            case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            case GL_UNSIGNED_SHORT_5_5_5_1:
            case GL_UNSIGNED_SHORT_1_5_5_5_REV:
                return 2;
            case GL_UNSIGNED_INT_8_8_8_8:
            case GL_UNSIGNED_INT_8_8_8_8_REV:
            case GL_UNSIGNED_INT_10_10_10_2:
            case GL_UNSIGNED_INT_2_10_10_10_REV:
            case GL_UNSIGNED_INT_10F_11F_11F_REV:
            case GL_UNSIGNED_INT_5_9_9_9_REV:
                return 4;
            default:
                return 0;
        }
    }

    U16 metal_float_to_half(F32 value)
    {
        // Round to nearest, ties to even. Metal upload byte streams must
        // match their destination pixel format; OpenGL performs this
        // conversion implicitly when a GL_FLOAT source fills a *16F image.
        const U32 bits = std::bit_cast<U32>(value);
        const U32 sign = (bits >> 16) & 0x8000u;
        const U32 magnitude = bits & 0x7fffffffu;
        if (magnitude >= 0x7f800000u)
        {
            return static_cast<U16>(
                sign | 0x7c00u |
                (magnitude > 0x7f800000u ? 0x0200u : 0u));
        }
        if (magnitude >= 0x477ff000u)
        {
            return static_cast<U16>(sign | 0x7c00u);
        }
        if (magnitude < 0x38800000u)
        {
            if (magnitude < 0x33000000u)
            {
                return static_cast<U16>(sign);
            }
            const U32 exponent = magnitude >> 23;
            const U32 mantissa = (magnitude & 0x7fffffu) | 0x800000u;
            const U32 shift = 126u - exponent;
            U32 half = mantissa >> shift;
            const U32 rest = mantissa & ((1u << shift) - 1u);
            const U32 tie = 1u << (shift - 1u);
            if (rest > tie || (rest == tie && (half & 1u)))
            {
                ++half;
            }
            return static_cast<U16>(sign | half);
        }
        U32 half = (magnitude - 0x38000000u) >> 13;
        const U32 rest = magnitude & 0x1fffu;
        if (rest > 0x1000u || (rest == 0x1000u && (half & 1u)))
        {
            ++half;
        }
        return static_cast<U16>(sign | half);
    }

    bool same_metal_texture(const ALMetalTexture* texture,
                            const ALMetalTextureDescriptor& descriptor)
    {
        if (!texture)
        {
            return false;
        }
        const ALMetalTextureDescriptor& existing = texture->descriptor();
        return existing.type == descriptor.type &&
               existing.format == descriptor.format &&
               existing.width == descriptor.width &&
               existing.height == descriptor.height &&
               existing.array_length == descriptor.array_length &&
               existing.mipmap_level_count == descriptor.mipmap_level_count &&
               existing.sample_count == descriptor.sample_count &&
               existing.storage == descriptor.storage &&
               existing.hazard_tracking == descriptor.hazard_tracking &&
               (existing.usage & descriptor.usage) == descriptor.usage &&
               existing.swizzle == descriptor.swizzle;
    }

    void report_unsupported_metal_texture(S32 internal_format, U32 primary_format, U32 type)
    {
        LL_WARNS("Metal") << "LLImageGL format is not yet mirrored to Metal: internal 0x"
                           << std::hex << internal_format << ", primary 0x" << primary_format
                           << ", type 0x" << type << std::dec << LL_ENDL;
    }
}

bool LLImageGL::adoptMetalRenderTexture(std::unique_ptr<ALMetalTexture> texture)
{
    if (!sImageMetalDevice || !texture ||
        mTarget != GL_TEXTURE_2D || mUploadInFlight)
    {
        return false;
    }

    const MetalUploadFormat upload_format =
        metal_upload_format(getStorageInternalFormat(), mFormatPrimary, mFormatType);
    if (upload_format.format == ALMetalPixelFormat::invalid)
    {
        return false;
    }

    ALMetalTextureDescriptor expected;
    expected.type = ALMetalTextureType::texture_2d;
    expected.format = upload_format.format;
    expected.width = static_cast<U32>(liveWidth(mCurrentDiscardLevel));
    expected.height = static_cast<U32>(liveHeight(mCurrentDiscardLevel));
    expected.mipmap_level_count = static_cast<U32>(mMipLevels > 0
        ? mMipLevels
        : mUseMipMaps
            ? calcMipLevelCount(expected.width, expected.height)
            : 1);
    expected.storage = ALMetalStorageMode::private_gpu;
    expected.usage = ALMetalTextureUsage::shader_read;
    switch (mDeprecatedSourceFormat)
    {
        case GL_ALPHA:
            expected.swizzle = {
                ALMetalTextureSwizzle::zero, ALMetalTextureSwizzle::zero,
                ALMetalTextureSwizzle::zero, ALMetalTextureSwizzle::red};
            break;
        case GL_LUMINANCE:
            expected.swizzle = {
                ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::red,
                ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::one};
            break;
        case GL_LUMINANCE_ALPHA:
            expected.swizzle = {
                ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::red,
                ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::green};
            break;
        default:
            break;
    }

    if (!same_metal_texture(texture.get(), expected) ||
        !alMetalHasUsage(texture->descriptor().usage,
                         ALMetalTextureUsage::render_target))
    {
        LL_WARNS("Metal") << "Rejected a render-produced LLImageGL mirror with an "
                           << "incompatible descriptor" << LL_ENDL;
        return false;
    }

    mPendingMetalTexture.reset();
    mMetalTexture = std::move(texture);
    if (mTexName == 0)
    {
        mTextureMemory = S64Bytes(metal_texture_bytes(*mMetalTexture));
    }
    updateMetalTextureAccounting();
    touchContents();
    return true;
}

bool LLImageGL::adoptMetalRenderTextureAuthoritative(
    std::unique_ptr<ALMetalTexture> texture, S32 category)
{
    if (mCurrentDiscardLevel < 0 ||
        !adoptMetalRenderTexture(std::move(texture)))
    {
        return false;
    }

    const ALMetalTextureDescriptor descriptor = mMetalTexture->descriptor();
    if (mTexName != 0)
    {
        free_tex_image(mTexName);
        LLImageGL::deleteTextures(1, &mTexName);
        mTexName = 0;
    }

    mGLTextureCreated = false;
    mStorageAllocated = false;
    mStorage = StorageDesc();
    mHasMipMaps = descriptor.mipmap_level_count > 1;
    mMipLevels = static_cast<S32>(descriptor.mipmap_level_count);
    mTextureMemory = S64Bytes(metal_texture_bytes(*mMetalTexture));
    updateMetalTextureAccounting();
    mLastBindTime = sLastFrameTime;
    setCategory(category);
    clearAlphaAnalysis();
    llassert(!getHasGLTexture());
    return true;
}

std::unique_ptr<ALMetalTexture> LLImageGL::createMetalRenderTexture(
    std::string* error) const
{
    if (!sImageMetalDevice || mTarget != GL_TEXTURE_2D ||
        mUploadInFlight)
    {
        if (error)
        {
            *error = "LLImageGL cannot allocate a Metal render mirror in its current state";
        }
        return nullptr;
    }

    const MetalUploadFormat upload_format =
        metal_upload_format(getStorageInternalFormat(), mFormatPrimary, mFormatType);
    if (upload_format.format == ALMetalPixelFormat::invalid)
    {
        if (error)
        {
            *error = "LLImageGL format cannot be represented by a Metal render mirror";
        }
        return nullptr;
    }

    ALMetalTextureDescriptor descriptor;
    descriptor.type = ALMetalTextureType::texture_2d;
    descriptor.format = upload_format.format;
    descriptor.width = static_cast<U32>(liveWidth(mCurrentDiscardLevel));
    descriptor.height = static_cast<U32>(liveHeight(mCurrentDiscardLevel));
    descriptor.mipmap_level_count = static_cast<U32>(mMipLevels > 0
        ? mMipLevels
        : mUseMipMaps
            ? calcMipLevelCount(descriptor.width, descriptor.height)
            : 1);
    descriptor.storage = ALMetalStorageMode::private_gpu;
    descriptor.usage = ALMetalTextureUsage::shader_read |
                       ALMetalTextureUsage::render_target;
    descriptor.label = "Alchemy render-produced image texture";
    switch (mDeprecatedSourceFormat)
    {
        case GL_ALPHA:
            descriptor.swizzle = {
                ALMetalTextureSwizzle::zero, ALMetalTextureSwizzle::zero,
                ALMetalTextureSwizzle::zero, ALMetalTextureSwizzle::red};
            break;
        case GL_LUMINANCE:
            descriptor.swizzle = {
                ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::red,
                ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::one};
            break;
        case GL_LUMINANCE_ALPHA:
            descriptor.swizzle = {
                ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::red,
                ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::green};
            break;
        default:
            break;
    }
    return ALMetalTexture::create(*sImageMetalDevice, descriptor, error);
}

void LLImageGL::markMetalRenderTextureUpdated() noexcept
{
    if (mMetalTexture)
    {
        touchContents();
    }
}

void LLImageGL::discardMetalTexture() noexcept
{
    if (!mUploadInFlight)
    {
        clearMetalTextureAccounting();
        mMetalTexture.reset();
        mPendingMetalTexture.reset();
        if (mTexName == 0)
        {
            mTextureMemory = S64Bytes(0);
        }
        touchContents();
    }
}

void LLImageGL::updateMetalTextureAccounting() noexcept
{
    const U64 desired = mTexName == 0 && mMetalTexture
        ? metal_texture_bytes(*mMetalTexture)
        : 0;
    if (desired > mAccountedMetalTextureBytes)
    {
        sMetalTextureBytes.fetch_add(
            desired - mAccountedMetalTextureBytes, std::memory_order_relaxed);
    }
    else if (desired < mAccountedMetalTextureBytes)
    {
        sMetalTextureBytes.fetch_sub(
            mAccountedMetalTextureBytes - desired, std::memory_order_relaxed);
    }
    mAccountedMetalTextureBytes = desired;
}

void LLImageGL::clearMetalTextureAccounting() noexcept
{
    if (mAccountedMetalTextureBytes != 0)
    {
        sMetalTextureBytes.fetch_sub(
            mAccountedMetalTextureBytes, std::memory_order_relaxed);
        mAccountedMetalTextureBytes = 0;
    }
}

ALMetalTexture* LLImageGL::metalTextureForUpload() noexcept
{
    if (mUploadInFlight)
    {
        return mPendingMetalTexture.get();
    }
    return mMetalTexture.get();
}

std::size_t LLImageGL::metalSourceBytesPerRow(U32 width) const
{
    switch (mFormatPrimary)
    {
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
            return ((static_cast<std::size_t>(width) + 3) / 4) * 8;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            return ((static_cast<std::size_t>(width) + 3) / 4) * 16;
        default:
            return static_cast<std::size_t>(width) * pixel_bytes(mFormatPrimary, mFormatType);
    }
}

bool LLImageGL::prepareMetalTexture(std::unique_ptr<ALMetalCommandBuffer>& command_owner,
                                    ALMetalCommandBuffer*& command_buffer,
                                    bool will_upload_pixels)
{
    if (!sImageMetalDevice || mTarget != GL_TEXTURE_2D)
    {
        return true;
    }

    const MetalUploadFormat upload_format =
        metal_upload_format(getStorageInternalFormat(), mFormatPrimary, mFormatType);
    std::unique_ptr<ALMetalTexture>& destination =
        mUploadInFlight ? mPendingMetalTexture : mMetalTexture;
    if (upload_format.format == ALMetalPixelFormat::invalid)
    {
        destination.reset();
        if (&destination == &mMetalTexture)
        {
            if (mTexName == 0)
            {
                mTextureMemory = S64Bytes(0);
            }
            updateMetalTextureAccounting();
        }
        report_unsupported_metal_texture(getStorageInternalFormat(), mFormatPrimary, mFormatType);
        return true;
    }

    ALMetalTextureDescriptor descriptor;
    descriptor.type = ALMetalTextureType::texture_2d;
    descriptor.format = upload_format.format;
    descriptor.width = static_cast<U32>(liveWidth(mCurrentDiscardLevel));
    descriptor.height = static_cast<U32>(liveHeight(mCurrentDiscardLevel));
    descriptor.mipmap_level_count = static_cast<U32>(llmax(mMipLevels, 1));
    descriptor.storage = ALMetalStorageMode::private_gpu;
    descriptor.usage = ALMetalTextureUsage::shader_read;
    descriptor.label = "Alchemy image texture";
    switch (mDeprecatedSourceFormat)
    {
        case GL_ALPHA:
            descriptor.swizzle = {
                ALMetalTextureSwizzle::zero, ALMetalTextureSwizzle::zero,
                ALMetalTextureSwizzle::zero, ALMetalTextureSwizzle::red};
            break;
        case GL_LUMINANCE:
            descriptor.swizzle = {
                ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::red,
                ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::one};
            break;
        case GL_LUMINANCE_ALPHA:
            descriptor.swizzle = {
                ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::red,
                ALMetalTextureSwizzle::red, ALMetalTextureSwizzle::green};
            break;
        default:
            break;
    }

    if (!same_metal_texture(destination.get(), descriptor))
    {
        std::string error;
        destination = ALMetalTexture::create(*sImageMetalDevice, descriptor, &error);
        if (!destination)
        {
            LL_WARNS("Metal") << "Could not allocate LLImageGL Metal texture: " << error << LL_ENDL;
            return false;
        }
        bool expected = false;
        if (sMetalTextureReported.compare_exchange_strong(expected, true, std::memory_order_relaxed))
        {
            LL_INFOS("Metal") << "Production LLImageGL Metal mirrors are active; first texture is "
                               << descriptor.width << "x" << descriptor.height << ", "
                               << descriptor.mipmap_level_count << " mip level(s)" << LL_ENDL;
        }
    }

    if (will_upload_pixels)
    {
        std::string error;
        if (sOnImageGLThread && mUploadInFlight)
        {
            command_buffer = acquire_worker_metal_upload(*sImageMetalDevice, &error);
        }
        else
        {
            command_owner = sImageMetalDevice->makeCommandBuffer("Alchemy image upload", &error);
            command_buffer = command_owner.get();
        }
        if (!command_buffer)
        {
            LL_WARNS("Metal") << "Could not create LLImageGL upload command: " << error << LL_ENDL;
            return false;
        }
    }
    return true;
}

bool LLImageGL::uploadMetalTextureRegion(ALMetalCommandBuffer& command_buffer,
                                         U32 mip_level,
                                         U32 x,
                                         U32 y,
                                         U32 width,
                                         U32 height,
                                         const U8* pixels,
                                         std::size_t source_bytes_per_row)
{
    ALMetalTexture* texture = metalTextureForUpload();
    if (!texture)
    {
        return true;
    }

    const MetalUploadFormat upload_format =
        metal_upload_format(getStorageInternalFormat(), mFormatPrimary, mFormatType);
    const U8* upload_pixels = pixels;
    std::size_t upload_bytes_per_row = source_bytes_per_row;
    std::vector<U8> converted;

    if (upload_format.scalar_conversion ==
        MetalScalarConversion::float32_to_float16)
    {
        const std::size_t source_components =
            dataFormatComponents(mFormatPrimary);
        const bool expand_rgb =
            upload_format.expansion == MetalRgbExpansion::rgb16_float;
        const std::size_t destination_components =
            expand_rgb ? 4 : source_components;
        const std::size_t source_pixel_bytes =
            source_components * sizeof(F32);
        upload_bytes_per_row = static_cast<std::size_t>(width) *
                               destination_components * sizeof(U16);
        converted.resize(upload_bytes_per_row * height);
        for (U32 row = 0; row < height; ++row)
        {
            const U8* source = pixels +
                static_cast<std::size_t>(row) * source_bytes_per_row;
            U8* target = converted.data() +
                static_cast<std::size_t>(row) * upload_bytes_per_row;
            for (U32 column = 0; column < width; ++column)
            {
                for (std::size_t component = 0;
                     component < source_components; ++component)
                {
                    F32 value = 0.f;
                    const U8* scalar =
                        source + component * sizeof(F32);
                    if (mFormatSwapBytes)
                    {
                        std::array<U8, sizeof(F32)> swapped{};
                        std::reverse_copy(
                            scalar, scalar + sizeof(F32), swapped.begin());
                        std::memcpy(&value, swapped.data(), sizeof(value));
                    }
                    else
                    {
                        std::memcpy(&value, scalar, sizeof(value));
                    }
                    const U16 half = metal_float_to_half(value);
                    std::memcpy(
                        target + component * sizeof(U16),
                        &half, sizeof(half));
                }
                if (expand_rgb)
                {
                    const U16 one = 0x3c00u;
                    std::memcpy(
                        target + 3 * sizeof(U16), &one, sizeof(one));
                }
                source += source_pixel_bytes;
                target += destination_components * sizeof(U16);
            }
        }
        upload_pixels = converted.data();
    }
    else if (upload_format.expansion != MetalRgbExpansion::none)
    {
        if (mFormatSwapBytes)
        {
            LL_WARNS("Metal") << "Byte-swapped RGB expansion is not supported for Metal upload" << LL_ENDL;
            return false;
        }
        std::size_t source_pixel_bytes = 0;
        std::size_t destination_pixel_bytes = 0;
        switch (upload_format.expansion)
        {
            case MetalRgbExpansion::rgb8:
                source_pixel_bytes = 3;
                destination_pixel_bytes = 4;
                break;
            case MetalRgbExpansion::rgb16_float:
                source_pixel_bytes = 6;
                destination_pixel_bytes = 8;
                break;
            case MetalRgbExpansion::rgb32_float:
                source_pixel_bytes = 12;
                destination_pixel_bytes = 16;
                break;
            case MetalRgbExpansion::none:
                break;
        }
        upload_bytes_per_row = static_cast<std::size_t>(width) * destination_pixel_bytes;
        converted.resize(upload_bytes_per_row * height);
        for (U32 row = 0; row < height; ++row)
        {
            const U8* source = pixels + static_cast<std::size_t>(row) * source_bytes_per_row;
            U8* target = converted.data() + static_cast<std::size_t>(row) * upload_bytes_per_row;
            for (U32 column = 0; column < width; ++column)
            {
                std::memcpy(target, source, source_pixel_bytes);
                if (upload_format.expansion == MetalRgbExpansion::rgb8)
                {
                    target[3] = 0xff;
                }
                else if (upload_format.expansion == MetalRgbExpansion::rgb16_float)
                {
                    const U16 one = 0x3c00;
                    std::memcpy(target + 6, &one, sizeof(one));
                }
                else
                {
                    const F32 one = 1.0f;
                    std::memcpy(target + 12, &one, sizeof(one));
                }
                source += source_pixel_bytes;
                target += destination_pixel_bytes;
            }
        }
        upload_pixels = converted.data();
    }
    else if (mFormatSwapBytes)
    {
        const std::size_t packed_bytes = metal_packed_bytes(mFormatType);
        const std::size_t scalar_bytes = packed_bytes ? packed_bytes : metal_scalar_bytes(mFormatType);
        const std::size_t scalars_per_pixel = packed_bytes ? 1 : dataFormatComponents(mFormatPrimary);
        if (scalar_bytes <= 1)
        {
            // GL treats byte swapping as a no-op for one-byte elements.
        }
        else
        {
            const std::size_t source_pixel_bytes = scalar_bytes * scalars_per_pixel;
            upload_bytes_per_row = static_cast<std::size_t>(width) * source_pixel_bytes;
            converted.resize(upload_bytes_per_row * height);
            for (U32 row = 0; row < height; ++row)
            {
                const U8* source = pixels + static_cast<std::size_t>(row) * source_bytes_per_row;
                U8* target = converted.data() + static_cast<std::size_t>(row) * upload_bytes_per_row;
                for (U32 column = 0; column < width; ++column)
                {
                    for (std::size_t scalar = 0; scalar < scalars_per_pixel; ++scalar)
                    {
                        for (std::size_t byte = 0; byte < scalar_bytes; ++byte)
                        {
                            target[scalar * scalar_bytes + byte] =
                                source[scalar * scalar_bytes + (scalar_bytes - byte - 1)];
                        }
                    }
                    source += source_pixel_bytes;
                    target += source_pixel_bytes;
                }
            }
            upload_pixels = converted.data();
        }
    }

    std::string error;
    if (!texture->upload(command_buffer, mip_level, 0,
                         {x, y, width, height}, upload_pixels,
                         upload_bytes_per_row, &error))
    {
        LL_WARNS("Metal") << "Could not upload LLImageGL Metal texture: " << error << LL_ENDL;
        return false;
    }
    return true;
}

bool LLImageGL::generateMetalMipmaps(ALMetalCommandBuffer& command_buffer)
{
    ALMetalTexture* texture = metalTextureForUpload();
    if (!texture || texture->descriptor().mipmap_level_count <= 1)
    {
        return true;
    }
    std::string error;
    if (!texture->generateMipmaps(command_buffer, &error))
    {
        LL_WARNS("Metal") << "Could not generate LLImageGL Metal mipmaps: " << error << LL_ENDL;
        return false;
    }
    return true;
}

bool LLImageGL::commitMetalUpload(std::unique_ptr<ALMetalCommandBuffer>& command_owner,
                                  ALMetalCommandBuffer*& command_buffer)
{
    if (!command_buffer)
    {
        return true;
    }
    if (!command_owner)
    {
        // Borrowed from the image worker's burst. syncMetalToMainThread() attaches the
        // batch completion token to this texture's pending publication.
        ++sWorkerMetalUploadBatch.mEncodedUploads;
        command_buffer = nullptr;
        return true;
    }

    std::string error;
    if (!command_owner->commit(&error))
    {
        LL_WARNS("Metal") << "Could not commit LLImageGL Metal upload: " << error << LL_ENDL;
        return false;
    }
    command_owner.reset();
    command_buffer = nullptr;
    return true;
}

bool LLImageGL::preserveMetalScaleDown(U32 source_mip_level, U32 width, U32 height)
{
    if (!sImageMetalDevice || !mMetalTexture)
    {
        return false;
    }

    const ALMetalTextureDescriptor& source_descriptor = mMetalTexture->descriptor();
    const bool can_blit = source_mip_level < source_descriptor.mipmap_level_count &&
                          mMetalTexture->width(source_mip_level) == width &&
                          mMetalTexture->height(source_mip_level) == height;
    const bool can_resample = !mHasMipMaps &&
                              ALMetalTextureResampler::supports(source_descriptor.format);
    if (!can_blit && !can_resample)
    {
        return false;
    }

    ALMetalTextureDescriptor destination_descriptor = source_descriptor;
    destination_descriptor.width = width;
    destination_descriptor.height = height;
    destination_descriptor.mipmap_level_count = mHasMipMaps
        ? static_cast<U32>(std::bit_width(std::max(width, height))) : 1;
    if (can_resample)
    {
        destination_descriptor.usage = destination_descriptor.usage |
                                       ALMetalTextureUsage::render_target;
    }
    destination_descriptor.label = "Alchemy downscaled image texture";

    std::string error;
    auto destination = ALMetalTexture::create(
        *sImageMetalDevice, destination_descriptor, &error);
    if (!destination)
    {
        LL_WARNS("Metal") << "Could not allocate downscaled LLImageGL Metal texture: "
                           << error << LL_ENDL;
        return false;
    }

    auto command = sImageMetalDevice->makeCommandBuffer(
        can_blit ? "Alchemy image scale-down blit"
                 : "Alchemy image scale-down bilinear render", &error);
    if (!command)
    {
        LL_WARNS("Metal") << "Could not create LLImageGL scale-down command: "
                           << error << LL_ENDL;
        return false;
    }

    bool encoded = false;
    if (can_blit)
    {
        ALMetalTextureCopy copy;
        copy.source_mip_level = source_mip_level;
        encoded = destination->copyFrom(*command, *mMetalTexture, copy, &error) &&
                  (destination_descriptor.mipmap_level_count <= 1 ||
                   destination->generateMipmaps(*command, &error));
    }
    else
    {
        if (!sMetalTextureResampler)
        {
            sMetalTextureResampler = ALMetalTextureResampler::create(
                *sImageMetalDevice, &error);
        }
        encoded = sMetalTextureResampler &&
                  sMetalTextureResampler->encode(
                      *command, *mMetalTexture, *destination, &error);
    }

    if (!encoded || !command->commit(&error))
    {
        LL_WARNS("Metal") << "Could not preserve LLImageGL scale-down in Metal: "
                           << error << LL_ENDL;
        return false;
    }

    mMetalTexture = std::move(destination);
    std::atomic<bool>& report = can_blit
        ? sMetalScaleDownReported : sMetalResampleReported;
    bool expected = false;
    if (report.compare_exchange_strong(expected, true, std::memory_order_relaxed))
    {
        LL_INFOS("Metal") << "Production LLImageGL Metal scale-down "
                           << (can_blit ? "blits" : "bilinear renders")
                           << " are active; first result is " << width << "x" << height
                           << (can_blit ? " from source mip " : " from level-zero source ")
                           << (can_blit ? source_mip_level : 0) << LL_ENDL;
    }
    return true;
}
#endif

// Whether to break an upload into sub_image_lines slices. This is latency smoothing,
// not throughput: it only ever applies on the main thread, where one large
// glTexSubImage2D can stall long enough to cost a frame. Off-thread uploads issue a
// single call.
//
// The compressed guard is structural, not a driver workaround: sub_image_lines slices
// by scanline stride, which is meaningless for block-compressed data. (The comment that
// used to be here blamed an NVIDIA/Win10 glTexSubImage2D bug, long since fixed -- but the
// guard would still be required without it.) Raw-name callers can pass a genuinely
// block-compressed texture; the allocation paths always pass false, since driver-side
// generic compression is gone.
static bool should_stagger_image_set(bool compressed)
{
#if LL_LINUX
    // Only NVIDIA's own driver slices. AMD and Intel were taken out on purpose
    // (2dcb7ae22c, March 2025), and the drivers that report neither -- nouveau, zink
    // on NVK, llvmpipe -- have not sliced since 1727c4d264. featuretable_linux.txt
    // gives all of those threaded texture uploads instead, so a large upload leaves
    // the main thread rather than stalling it.
    return !compressed && on_main_thread() && gGLManager.mIsNVIDIA;
#elif LL_DARWIN
    return !compressed && on_main_thread() && gGLManager.mIsAMD;
#else
    // Setting media textures off-thread seems faster when not using sub_image_lines (Nvidia/Windows 10) -Cosmic,2023-03-31
    return !compressed && on_main_thread() && !gGLManager.mIsIntel;
#endif
}

// Equivalent to calling glSetSubImage2D(target, miplevel, x_offset, y_offset, width, height, pixformat, pixtype, src), assuming the total width of the image is data_width
// However, instead there are multiple calls to glSetSubImage2D on smaller slices of the image
static void sub_image_lines(U32 target, S32 miplevel, S32 x_offset, S32 y_offset, S32 width, S32 height, U32 pixformat, U32 pixtype, const U8* src, S32 data_width)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;

    LL_PROFILE_ZONE_NUM(width);
    LL_PROFILE_ZONE_NUM(height);

    const U32 line_width = data_width * pixel_bytes(pixformat, pixtype);
    const U32 y_offset_end = y_offset + height;

    if (width == data_width && height % 32 == 0)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_TEXTURE("subimage - batched lines");

        // full width, batch multiple lines at a time
        // set batch size based on width
        U32 batch_size = 32;

        if (width > 1024)
        {
            batch_size = 8;
        }
        else if (width > 512)
        {
            batch_size = 16;
        }

        // full width texture, do 32 lines at a time
        for (U32 y_pos = y_offset; y_pos < y_offset_end; y_pos += batch_size)
        {
            // If this keeps crashing, pass down data_size, looks like it is using
            // imageraw->getData(); for data, but goes way over allocated size limit
            glTexSubImage2D(target, miplevel, x_offset, y_pos, width, batch_size, pixformat, pixtype, src);
            src += line_width * batch_size;
        }
    }
    else
    {
        // partial width or strange height
        for (U32 y_pos = y_offset; y_pos < y_offset_end; y_pos += 1)
        {
            // If this keeps crashing, pass down data_size, looks like it is using
            // imageraw->getData(); for data, but goes way over allocated size limit
            glTexSubImage2D(target, miplevel, x_offset, y_pos, width, 1, pixformat, pixtype, src);
            src += line_width;
        }
    }
}

// static
void LLImageGL::generateTextures(S32 numTextures, U32 *textures)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    static constexpr U32 pool_size = 1024;
    // pool of texture names, on the heap: as a thread_local array it was 4 KB of static TLS, which
    // glibc takes out of the stack of every thread, those that never make a texture too
    static thread_local std::unique_ptr<U32[]> name_pool;
    static thread_local U32 name_count = 0; // number of available names in the pool
    static thread_local U32 name_generation = 0; // the context the pool was filled on

    const U32 generation = sTextureNameGeneration.load(std::memory_order_relaxed);
    if (name_generation != generation)
    {
        name_count      = 0;
        name_generation = generation;
    }
    if (name_count == 0)
    {
        LL_PROFILE_ZONE_NAMED("iglgt - reup pool");
        // pool is emtpy, refill it
        if (!name_pool)
        {
            name_pool = std::make_unique<U32[]>(pool_size);
        }
        glGenTextures(pool_size, name_pool.get());
        name_count = pool_size;
    }

    if ((U32)numTextures <= name_count)
    {
        //copy teture names off the end of the pool
        memcpy(textures, name_pool.get() + name_count - numTextures, sizeof(U32) * numTextures);
        name_count -= numTextures;
    }
    else
    {
        LL_PROFILE_ZONE_NAMED("iglgt - pool miss");
        glGenTextures(numTextures, textures);
    }
}

// static
void LLImageGL::updateClass()
{
    sFrameCount++;

    // wait a few frames before actually deleting the textures to avoid
    // synchronization issues with the GPU
    U32 idx = (sFrameCount+DELETE_DELAY) % (DELETE_DELAY+1);

    if (!sFreeList[idx].empty())
    {
        gGL.forgetTextures((S32)sFreeList[idx].size(), sFreeList[idx].data());
        free_tex_images((GLsizei) sFreeList[idx].size(), sFreeList[idx].data());
        glDeleteTextures((GLsizei)sFreeList[idx].size(), sFreeList[idx].data());
        sFreeList[idx].resize(0);
    }

    publishUploads();
}

// static
void LLImageGL::deleteTextures(S32 numTextures, const U32 *textures)
{
    if (gGLManager.mInited)
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
        U32 idx = sFrameCount % (DELETE_DELAY+1);
        for (S32 i = 0; i < numTextures; ++i)
        {
            sFreeList[idx].push_back(textures[i]);
        }
    }
}

// static
void LLImageGL::resolveUploadFormat(S32& intformat, U32& pixformat, U32& pixtype,
                                    const void*& pixels, S32 width, S32 height)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    // Deprecated source formats are re-expressed as R8 / RG8 and read back through the
    // texture's swizzle attribute.
    //
    // The upload paths do not write GL_TEXTURE_SWIZZLE_RGBA themselves — overwriting it is
    // destructive when the bound texture already has a custom swizzle (e.g. an LLImageGL
    // whose resolveDeprecatedFormat recorded a specific source mask,
    // or any caller that set up its own swizzle before this call). The format rewrite
    // here still happens so the allocation gets the right backing storage; applying the
    // swizzle is the caller's responsibility. LLImageGL applies it to every name it creates,
    // from mDeprecatedSourceFormat. Direct callers that pass GL_ALPHA / GL_LUMINANCE /
    // GL_LUMINANCE_ALPHA must apply the matching swizzle themselves before uploading.
    if (pixformat == GL_ALPHA)
    { //GL_ALPHA → R8; caller-set {0,0,0,R} swizzle is required for {0,0,0,A} sample semantics
        pixformat = GL_RED;
        intformat = GL_R8;
    }

    if (pixformat == GL_LUMINANCE)
    { //GL_LUMINANCE → R8; caller-set {R,R,R,1} swizzle is required for {L,L,L,1} sample semantics
        pixformat = GL_RED;
        intformat = GL_R8;
    }

    if (pixformat == GL_LUMINANCE_ALPHA)
    { //GL_LUMINANCE_ALPHA → RG8; caller-set {R,R,R,G} swizzle is required for {L,L,L,A} sample semantics
        pixformat = GL_RG;
        intformat = GL_RG8;
    }
}

// Upload one level into a texture that already has storage. Same format resolution as the
// allocating paths, but a sub-image write, which is what makes it legal on immutable
// storage. Does no VRAM accounting -- allocateTextureStorage recorded the whole object
// when it created the storage.
void LLImageGL::setManualSubImage(U32 target, S32 miplevel, S32 width, S32 height, U32 pixformat, U32 pixtype, const void* pixels)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;

    if (pixels == nullptr)
    {
        // storage already exists and there is nothing to write into it
        return;
    }

    S32 intformat = 0; // resolved but unused; storage was fixed at allocation
    resolveUploadFormat(intformat, pixformat, pixtype, pixels, width, height);

    stop_glerror();
    {
        LL_PROFILE_ZONE_NAMED("glTexSubImage2D");
        LL_PROFILE_ZONE_NUM(width);
        LL_PROFILE_ZONE_NUM(height);

        if (should_stagger_image_set(false))
        {
            sub_image_lines(target, miplevel, 0, 0, width, height, pixformat, pixtype, (const U8*)pixels, width);
        }
        else
        {
            glTexSubImage2D(target, miplevel, 0, 0, width, height, pixformat, pixtype, pixels);
        }
    }
    stop_glerror();
}

// Opt a freshly-allocated texture out of the sRGB transfer function.
//
// GL applies it on every read of an sRGB-format texture unless told otherwise, so without
// this the decode is decided by the internal format and nothing else -- see
// ALSampler::SRGBDecode for why that is the wrong way round for this renderer.
//
// The samplers already skip it, and a sampler object overrides the texture when one is
// bound, so this is what makes a bind with sampler 0 behave the same way rather than
// quietly linearising. Harmless on a non-sRGB format: the state exists but nothing reads it.
static void skipSRGBDecode(U32 target)
{
    glTexParameteri(target, GL_TEXTURE_SRGB_DECODE_EXT, GL_SKIP_DECODE_EXT);
}

// Allocate a 2D texture on the currently-bound name and upload level 0.
//
// This owns the whole texture: call it exactly once per texture object. The result cannot
// then be resized or reformatted -- build a new texture instead. That is the contract
// glTexStorage2D imposes, and the one D3D11 requires of every resource.
//
// levels is the mip level COUNT (see calcMipLevelCount). Storage is allocated for all of
// them, so the caller can fill levels 1+ with setManualSubImage.
//
// pixels may be null to allocate without uploading.
void LLImageGL::allocateTexture2D(U32 target, S32 intformat, S32 width, S32 height,
                                  U32 pixformat, U32 pixtype, const void* pixels, S32 levels)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;

    levels = llmax(1, levels);

    resolveUploadFormat(intformat, pixformat, pixtype, pixels, width, height);

    free_cur_tex_image();

    assertStorageAllocatable(target, intformat);

    {
        LL_PROFILE_ZONE_NAMED("glTexStorage2D");
        glTexStorage2D(target, levels, intformat, width, height);
        skipSRGBDecode(target);

        if (pixels != nullptr)
        {
            if (should_stagger_image_set(false))
            {
                sub_image_lines(target, 0, 0, 0, width, height, pixformat, pixtype, (const U8*)pixels, width);
            }
            else
            {
                glTexSubImage2D(target, 0, 0, 0, width, height, pixformat, pixtype, pixels);
            }
        }
    }

    alloc_tex_image(width, height, intformat, 1, levels > 1);
    stop_glerror();
}

//create an empty GL texture: just create a texture name
//the texture is assiciate with some image by calling glTexImage outside LLImageGL
bool LLImageGL::setImageMetadata(S32 discard_level, const LLImageRaw* imageraw)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    checkActiveThread();

    if (!imageraw || imageraw->isBufferInvalid())
    {
        LL_WARNS() << "Trying to publish metadata from invalid image data"
                   << LL_ENDL;
        mGLTextureCreated = false;
        return false;
    }

    if (discard_level < 0)
    {
        llassert(mCurrentDiscardLevel >= 0);
        discard_level = mCurrentDiscardLevel;
    }
    discard_level = llmin(discard_level, MAX_DISCARD_LEVEL);

    const S32 width = imageraw->getWidth() << discard_level;
    const S32 height = imageraw->getHeight() << discard_level;

    checkUploadBegun();
    if (!setSize(width, height, imageraw->getComponents(), discard_level))
    {
        LL_WARNS() << "Trying to publish image metadata with incorrect dimensions!"
                   << LL_ENDL;
        mGLTextureCreated = false;
        return false;
    }

    if (mHasExplicitFormat &&
        ((mFormatPrimary == GL_RGBA && mComponents < 4) ||
         (mFormatPrimary == GL_RGB && mComponents < 3)))
    {
        LL_WARNS() << "Incorrect format: " << std::hex << mFormatPrimary
                   << " components: " << (U32)mComponents << LL_ENDL;
        mHasExplicitFormat = false;
    }

    if (!mHasExplicitFormat)
    {
        resolveComponentFormat(mComponents);
    }

    // A metadata-only image has no GPU owner. This releases either kind of
    // transitional backing without requiring a live GL manager.
    destroyGLTexture();
    mCurrentDiscardLevel = discard_level;
    mLastBindTime = sLastFrameTime;
    mGLTextureCreated = false;
    return true;
}


#if LL_DARWIN
bool LLImageGL::syncMetalToMainThread()
{
    LL_PROFILE_ZONE_SCOPED;
    llassert(!on_main_thread());
    llassert(mUploadInFlight);
    llassert(mPendingMetalTexture);

    UploadCompletion metal_completion = current_worker_metal_upload_completion();
    ref();
    const bool posted = LL::WorkQueue::postMaybe(
            mMainQueue,
            [this, metal_completion = std::move(metal_completion)]() mutable
            {
                LL_PROFILE_ZONE_NAMED("native image - queue publish");
                queuePublish(std::move(metal_completion));
                unref();
            });
    if (!posted)
    {
        unref();
    }

    if (sWorkerMetalUploadBatch.mCommand &&
        sWorkerMetalUploadBatch.mEncodedUploads >= METAL_UPLOAD_BATCH_LIMIT)
    {
        flush_worker_metal_upload_batch();
    }
    return posted;
}

void LLImageGL::publishMetalTextureAuthoritative()
{
    llassert(on_main_thread());
    llassert(mUploadInFlight);
    llassert(mPendingMetalTexture);

    // The pending native generation is complete. Retire the transitional GL
    // object only at this publication boundary so allocation/encoding failure
    // leaves the previously visible generation intact.
    if (mTexName != 0)
    {
        free_tex_image(mTexName);
        LLImageGL::deleteTextures(1, &mTexName);
        mTexName = 0;
    }
    mStorageAllocated = false;
    mStorage = StorageDesc();
    mGLTextureCreated = false;
    syncTexName(0);
}
#endif

void LLImageGL::queuePublish(UploadCompletion metal_completion)
{
    llassert(on_main_thread());
    if (mPublishesPending == 0)
    {
        mPublishesSucceeded = true;
    }
    ++mPublishesPending;
    sPendingPublishes.push_back(
        { LLPointer<LLImageGL>(this), std::move(metal_completion) });
}

void LLImageGL::finishPublish(bool metal_upload_succeeded)
{
    mPublishesSucceeded = mPublishesSucceeded && metal_upload_succeeded;
#if LL_DARWIN
    if (!metal_upload_succeeded)
    {
        // Keep the previously published native generation authoritative.
        mPendingMetalTexture.reset();
        cancelUpload();
    }
    else
    {
        syncTexName(0);
    }
#else
    llassert(false);
#endif

    llassert(mPublishesPending > 0);
    if (--mPublishesPending == 0 && !mOnPublished.empty())
    {
        // Moved out first: a callback may ask for another one.
        std::vector<std::function<void(bool)>> callbacks;
        callbacks.swap(mOnPublished);
        const bool succeeded = mPublishesSucceeded;
        for (std::function<void(bool)>& callback : callbacks)
        {
            callback(succeeded);
        }
    }
}

void LLImageGL::afterPublish(std::function<void()> fn)
{
    afterPublish([fn = std::move(fn)](bool) { fn(); });
}

void LLImageGL::afterPublish(std::function<void(bool)> fn)
{
    llassert(on_main_thread());
    if (mPublishesPending == 0)
    {
        fn(true);
        return;
    }
    mOnPublished.push_back(std::move(fn));
}

// static
void LLImageGL::finishUploads()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    llassert(on_main_thread());
    const auto give_up = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (publishUploads() > 0 && std::chrono::steady_clock::now() < give_up)
    {
        std::this_thread::yield();
    }
}

// static
void LLImageGL::pollUploads()
{
    if (sPendingPublishes.empty())
    {
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    llassert(on_main_thread());

    // Metal exposes batch commitment through a lock-free completion token. Unlike the
    // removed GL fence path, frame end has no active poll to issue; publishUploads observes
    // the token at the next publication point.
}

U32 LLImageGL::publishUploads()
{
    if (sPendingPublishes.empty())
    {
        return 0;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    llassert(on_main_thread());

    // The worker queues native batches in submission order. The first still-pending batch
    // leaves every later publication pending. Remove ready entries before callbacks run.
    std::vector<PendingPublish> ready;
    for (PendingPublish& pending : sPendingPublishes)
    {
        if (!publication_ready(pending))
        {
            break;
        }
        ready.push_back(std::move(pending));
    }
    sPendingPublishes.erase(sPendingPublishes.begin(), sPendingPublishes.begin() + ready.size());

    for (PendingPublish& pending : ready)
    {
        const bool metal_upload_succeeded =
            !pending.mMetalCompletion ||
            pending.mMetalCompletion->load(std::memory_order_acquire) != METAL_UPLOAD_FAILED;
        pending.mImage->finishPublish(metal_upload_succeeded);
    }

    return (U32)sPendingPublishes.size();
}


// Capture the currently published generation before an off-thread native upload starts
// overwriting members with the geometry of the texture it is about to build.
//
// Main thread only, and both ends of it: whoever hands the upload to the LLImageGL thread
// begins it before posting, syncTexName ends it when the new texture publishes, and the
// poster's completion callback ends it whatever happened (a no-op after syncTexName).
// That is what lets the main thread read mUploadInFlight and the snapshot with no
// synchronisation -- the worker never writes them, and reads them only after the post
// that ordered them. Were the worker to raise the flag itself, a getter could see it go
// up before the snapshot behind it was written, or test it just before it went up and
// then read members the worker had started to overwrite.
void LLImageGL::beginUpload()
{
    llassert(on_main_thread());
    if (mUploadInFlight)
    {
        return;
    }

    mPublished.mWidth               = mWidth;
    mPublished.mHeight              = mHeight;
    mPublished.mComponents          = mComponents;
    mPublished.mCurrentDiscardLevel = mCurrentDiscardLevel;
    mPublished.mMaxDiscardLevel     = mMaxDiscardLevel;
    mPublished.mFormatPrimary       = mFormatPrimary;
    mPublished.mDeprecatedSourceFormat = mDeprecatedSourceFormat;
    mPublished.mFormatInternal      = mFormatInternal;
    mPublished.mFormatType          = mFormatType;
    mPublished.mFormatComponents    = mFormatComponents;
    mPublished.mAlphaStride         = mAlphaStride;
    mPublished.mAlphaOffset         = mAlphaOffset;
    mPublished.mFormatSwapBytes     = mFormatSwapBytes;
    mPublished.mHasExplicitFormat   = mHasExplicitFormat;
    mPublished.mNeedsAlphaAndPickMask = mNeedsAlphaAndPickMask;
    mPublished.mHasMipMaps          = mHasMipMaps;
    mPublished.mMipLevels           = mMipLevels;
    mPublished.mCategory            = mCategory;
    mPublished.mGLTextureCreated    = mGLTextureCreated.load(std::memory_order_relaxed);
    mPublished.mTextureMemory       = mTextureMemory;
    mUploadInFlight = true;
}

void LLImageGL::beginUpload(S8 components)
{
    llassert(on_main_thread());
    // With no upload of this image under way, nothing reads the format while it is set. It
    // is set once the snapshot has the format of the texture mTexName still names, which the
    // getters answer with until the new one publishes.
    const bool resolve = !mUploadInFlight && !mHasExplicitFormat;
    beginUpload();
    if (resolve)
    {
        resolveComponentFormat(components);
    }
}

void LLImageGL::cancelUpload()
{
    llassert(on_main_thread());
    if (!mUploadInFlight)
    {
        return;
    }

    mWidth = static_cast<U16>(mPublished.mWidth);
    mHeight = static_cast<U16>(mPublished.mHeight);
    mComponents = mPublished.mComponents;
    mCurrentDiscardLevel = mPublished.mCurrentDiscardLevel;
    mMaxDiscardLevel = mPublished.mMaxDiscardLevel;
    mFormatPrimary = mPublished.mFormatPrimary;
    mDeprecatedSourceFormat = mPublished.mDeprecatedSourceFormat;
    mFormatInternal = mPublished.mFormatInternal;
    mFormatType = mPublished.mFormatType;
    mFormatComponents = mPublished.mFormatComponents;
    mAlphaStride = mPublished.mAlphaStride;
    mAlphaOffset = mPublished.mAlphaOffset;
    mFormatSwapBytes = mPublished.mFormatSwapBytes;
    mHasExplicitFormat = mPublished.mHasExplicitFormat;
    mNeedsAlphaAndPickMask = mPublished.mNeedsAlphaAndPickMask;
    mHasMipMaps = mPublished.mHasMipMaps;
    mMipLevels = mPublished.mMipLevels;
    mCategory = mPublished.mCategory;
    mGLTextureCreated.store(mPublished.mGLTextureCreated,
                            std::memory_order_relaxed);
    mTextureMemory = mPublished.mTextureMemory;
    endUpload();
}

void LLImageGL::endUpload()
{
    llassert(on_main_thread());
    mUploadInFlight = false;

    // Whatever the upload staged and did not publish describes a texture that never was.
    discardPendingAlpha();
#if LL_DARWIN
    mPendingMetalTexture.reset();
#endif
}

void LLImageGL::checkUploadBegun() const
{
    if (!on_main_thread() && !mUploadInFlight)
    {
        LL_WARNS_ONCE("LLImageGL") << "Texture upload off the main thread with no beginUpload(): "
                                   << "the image reports the new texture's size before that "
                                   << "texture is published." << LL_ENDL;
        llassert(false);
    }
}

void LLImageGL::syncTexName(LLGLuint texname)
{
    bool publish = texname != 0;
#if LL_DARWIN
    publish = publish || mPendingMetalTexture != nullptr;
#endif
    if (publish)
    {
        if (texname != 0 && mTexName != 0 && mTexName != texname)
        {
            LLImageGL::deleteTextures(1, &mTexName);
        }
        if (texname != 0)
        {
            mTexName = texname;
        }

        // Marked bound as it publishes, so it is not thrown out before anything draws it.
        mLastBindTime = sLastFrameTime;

        // The alpha facts derived from the new texture publish with it.
        publishPendingAlpha();
#if LL_DARWIN
        if (mPendingMetalTexture)
        {
            mMetalTexture = std::move(mPendingMetalTexture);
        }
        if (mTexName == 0 && mMetalTexture)
        {
            mTextureMemory = S64Bytes(metal_texture_bytes(*mMetalTexture));
        }
        updateMetalTextureAccounting();
#endif
    }

    // Members and mTexName describe the same texture again.
    endUpload();
}

bool LLImageGL::stagesAlpha() const
{
    // Off the main thread, getMask and getIsAlphaMask may be reading the published values
    // at this very moment; on it, they answer for mTexName, which an upload in flight has
    // not replaced yet.
    return mUploadInFlight || !on_main_thread();
}

void LLImageGL::setPickMask(U8* mask, U16 width, U16 height)
{
    if (stagesAlpha())
    {
        delete[] mPendingAlpha.mPickMask;
        mPendingAlpha.mPickMask       = mask;
        mPendingAlpha.mPickMaskWidth  = width;
        mPendingAlpha.mPickMaskHeight = height;
        mPendingAlpha.mHasPickMask    = true;
        return;
    }

    freePickMask();
    mPickMask       = mask;
    mPickMaskWidth  = width;
    mPickMaskHeight = height;
}

void LLImageGL::setAlphaAnalysis(const AlphaAnalysis& analysis)
{
    if (stagesAlpha())
    {
        mPendingAlpha.mAnalysis    = analysis;
        mPendingAlpha.mHasAnalysis = true;
        return;
    }

    mIsMask = analysis.mIsMask;
    mMaxAlpha = analysis.mMaxAlpha;
    mMinAlpha = analysis.mMinAlpha;
}

void LLImageGL::clearAlphaAnalysis()
{
    mIsMask = false;
    mMaxAlpha = 255;
    mMinAlpha = 0;
}

void LLImageGL::publishPendingAlpha()
{
    if (mPendingAlpha.mHasPickMask)
    {
        freePickMask();
        mPickMask       = mPendingAlpha.mPickMask;
        mPickMaskWidth  = mPendingAlpha.mPickMaskWidth;
        mPickMaskHeight = mPendingAlpha.mPickMaskHeight;
        mPendingAlpha.mPickMask = nullptr;
    }
    if (mPendingAlpha.mHasAnalysis)
    {
        mIsMask = mPendingAlpha.mAnalysis.mIsMask;
        mMaxAlpha = mPendingAlpha.mAnalysis.mMaxAlpha;
        mMinAlpha = mPendingAlpha.mAnalysis.mMinAlpha;
    }
    mPendingAlpha = PendingAlpha();
}

void LLImageGL::discardPendingAlpha()
{
    delete[] mPendingAlpha.mPickMask;
    mPendingAlpha = PendingAlpha();
}

bool LLImageGL::readBackRaw(S32 discard_level, LLImageRaw* imageraw, bool compressed_ok) const
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;

    if (!imageraw)
    {
        return false;
    }

    // The published view stays on the old texture while an upload is in flight.
    // Pair its discard, dimensions and format with the old published Metal mirror;
    // mPendingMetalTexture belongs to the upload that has not published yet.
    const S32 cur_discard = getDiscardLevel();
    const S32 max_discard = getMaxDiscardLevel();

    if (discard_level < 0)
    {
        discard_level = cur_discard;
    }

    if (discard_level < cur_discard || discard_level > max_discard)
    {
        return false;
    }

    const U32 mip_level = static_cast<U32>(discard_level - cur_discard);

    const S32 width = getWidth(discard_level);
    const S32 height = getHeight(discard_level);
    const S32 ncomponents = getComponents();
    if (ncomponents == 0)
    {
        return false;
    }

    if (width <= 0 || width > 2048 || height <= 0 || height > 2048 || ncomponents < 1 || ncomponents > 4)
    {
        LL_ERRS() << llformat("LLImageGL::readBackRaw: bogus params: %d x %d x %d",width,height,ncomponents) << LL_ENDL;
    }

#if LL_DARWIN
    // Darwin readback is native-only. Fetched textures own the separately
    // published Metal mirror populated by their upload. A missing or incompatible
    // mirror is an incomplete resource, not permission to synchronously import the
    // deprecated GL texture. That makes saved fetched/sculpt images fail closed.
    if (!sImageMetalDevice || !mMetalTexture)
    {
        LL_WARNS("Metal") << "Cannot read back LLImageGL without its published Metal mirror"
                           << LL_ENDL;
        return false;
    }
    const ALMetalTextureDescriptor& descriptor = mMetalTexture->descriptor();
    if (mip_level >= descriptor.mipmap_level_count ||
        mMetalTexture->width(mip_level) != static_cast<U32>(width) ||
        mMetalTexture->height(mip_level) != static_cast<U32>(height))
    {
        LL_WARNS("Metal") << "Cannot read back LLImageGL from a mismatched Metal mip"
                           << LL_ENDL;
        return false;
    }

    // Preserve the old compressed_ok contract without a GL metadata query. The
    // Metal descriptor is authoritative, and BC block layout is known from the
    // published image format.
    if (mMetalTexture->isCompressed())
    {
        if (!compressed_ok)
        {
            LL_WARNS("Metal") << "LLImageGL compressed readback requires an explicit "
                                  "uncompressed conversion owner"
                               << LL_ENDL;
            return false;
        }
        const S64 storage_bytes = dataFormatBytes(mFormatPrimary, width, height);
        if (storage_bytes <= 0 ||
            storage_bytes > static_cast<S64>(std::numeric_limits<S32>::max()))
        {
            LL_WARNS("Metal") << "Invalid compressed LLImageGL readback size: "
                               << storage_bytes << LL_ENDL;
            return false;
        }
        LLImageDataLock lock(imageraw);
        if (!imageraw->allocateDataSize(
                width, height, ncomponents, static_cast<S32>(storage_bytes)))
        {
            LL_WARNS("Metal") << "Memory allocation failed for compressed Metal texture readback"
                               << LL_ENDL;
            return false;
        }
        std::string metal_error;
        if (!mMetalTexture->readback(
                *sImageMetalDevice, mip_level, 0, {}, imageraw->getData(),
                metalSourceBytesPerRow(static_cast<U32>(width)), &metal_error))
        {
            LL_WARNS("Metal") << "Could not read back compressed LLImageGL Metal mirror: "
                               << metal_error << LL_ENDL;
            imageraw->deleteData();
            return false;
        }
        return true;
    }

    const LLGLenum primary_format = getPrimaryFormat();
    const U32 host_pixel_bytes = pixel_bytes(primary_format, mFormatType);
    if (host_pixel_bytes != static_cast<U32>(ncomponents))
    {
        LL_WARNS("Metal") << "LLImageGL byte readback cannot represent format 0x"
                           << std::hex << primary_format << " type 0x" << mFormatType
                           << std::dec << " in " << ncomponents << " byte components"
                           << LL_ENDL;
        return false;
    }
    const std::size_t metal_pixel_bytes = mMetalTexture->bytesPerPixel();
    const bool direct = metal_pixel_bytes == static_cast<std::size_t>(ncomponents);
    const bool expanded_rgb = ncomponents == 3 &&
                              primary_format == GL_RGB &&
                              mFormatType == GL_UNSIGNED_BYTE &&
                              metal_pixel_bytes == 4;
    if (!direct && !expanded_rgb)
    {
        LL_WARNS("Metal") << "LLImageGL Metal storage does not match its host byte layout"
                           << LL_ENDL;
        return false;
    }

    LLImageDataLock lock(imageraw);
    if (!imageraw->allocateDataSize(width, height, ncomponents))
    {
        constexpr F32 MAX_IMAGE_SIZE = 2048 * 2048;
        const F32 size = static_cast<F32>(width) * static_cast<F32>(height) *
                         static_cast<F32>(ncomponents);
        if (size > 0 && size <= MAX_IMAGE_SIZE)
        {
            LLError::LLUserWarningMsg::showOutOfMemory();
            LL_ERRS() << "Memory allocation failed for reading back Metal texture. Data size: "
                      << size << LL_ENDL;
        }
        LL_WARNS() << "Memory allocation failed for reading back Metal texture." << LL_ENDL;
        return false;
    }

    std::vector<U8> expanded;
    void* destination = imageraw->getData();
    std::size_t destination_row_bytes = static_cast<std::size_t>(width) * ncomponents;
    if (expanded_rgb)
    {
        destination_row_bytes = static_cast<std::size_t>(width) * metal_pixel_bytes;
        expanded.resize(destination_row_bytes * static_cast<std::size_t>(height));
        destination = expanded.data();
    }

    std::string metal_error;
    if (!mMetalTexture->readback(
            *sImageMetalDevice, mip_level, 0, {}, destination,
            destination_row_bytes, &metal_error))
    {
        LL_WARNS("Metal") << "Could not read back LLImageGL Metal mirror: "
                           << metal_error << LL_ENDL;
        imageraw->deleteData();
        return false;
    }

    if (expanded_rgb)
    {
        U8* output = imageraw->getData();
        const U8* input = expanded.data();
        const std::size_t pixels = static_cast<std::size_t>(width) * height;
        for (std::size_t pixel = 0; pixel < pixels; ++pixel)
        {
            std::memcpy(output + pixel * 3, input + pixel * 4, 3);
        }
    }
    return true;
#else
    (void)compressed_ok;
    LL_WARNS_ONCE("Metal") << "LLImageGL::readBackRaw has no non-Metal fallback"
                            << LL_ENDL;
    return false;
#endif
}

void LLImageGL::destroyGLTexture()
{
    checkActiveThread();

#if LL_DARWIN
    const bool had_native_texture = mMetalTexture || mPendingMetalTexture;
    clearMetalTextureAccounting();
    mMetalTexture.reset();
    mPendingMetalTexture.reset();
#else
    const bool had_native_texture = false;
#endif

    if (mTexName != 0)
    {
        if(mTextureMemory != S64Bytes(0))
        {
            mTextureMemory = (S64Bytes)0;
        }

        LLImageGL::deleteTextures(1, &mTexName);
        mCurrentDiscardLevel = -1 ; //invalidate mCurrentDiscardLevel.
        mTexName = 0;
        mGLTextureCreated = false ;
    }
    else if (had_native_texture)
    {
        mTextureMemory = S64Bytes(0);
        mCurrentDiscardLevel = -1;
        mGLTextureCreated = false;
    }

    // Invalidate pending jobs
    ++mAlphaAnalysisSerial;
}

//force to invalidate the gl texture, most likely a sculpty texture
void LLImageGL::forceToInvalidateGLTexture()
{
    checkActiveThread();
    if (mTexName != 0)
    {
        destroyGLTexture();
    }
    else
    {
        mCurrentDiscardLevel = -1 ; //invalidate mCurrentDiscardLevel.
    }
}

//----------------------------------------------------------------------------

bool LLImageGL::getIsResident(bool test_now)
{
    if (test_now)
    {
        if (mTexName != 0)
        {
            mIsResident = true;
        }
        else
        {
            mIsResident = false;
        }
    }

    return mIsResident;
}

S32 LLImageGL::getHeight(S32 discard_level) const
{
    const S32 base = mUploadInFlight ? mPublished.mHeight : mHeight;
    if (discard_level < 0)
    {
        // mCurrentDiscardLevel can still be -1 if no discard has been set;
        // treat that as "full resolution" rather than shifting by a negative.
        discard_level = llmax<S32>(mUploadInFlight ? mPublished.mCurrentDiscardLevel
                                                   : mCurrentDiscardLevel, 0);
    }
    S32 height = base >> discard_level;
    if (height < 1) height = 1;
    return height;
}

S32 LLImageGL::getWidth(S32 discard_level) const
{
    const S32 base = mUploadInFlight ? mPublished.mWidth : mWidth;
    if (discard_level < 0)
    {
        discard_level = llmax<S32>(mUploadInFlight ? mPublished.mCurrentDiscardLevel
                                                   : mCurrentDiscardLevel, 0);
    }
    S32 width = base >> discard_level;
    if (width < 1) width = 1;
    return width;
}

S64 LLImageGL::getBytes(S32 discard_level) const
{
    if (discard_level < 0)
    {
        discard_level = llmax<S32>(mCurrentDiscardLevel, 0);
    }
    S32 w = mWidth>>discard_level;
    S32 h = mHeight>>discard_level;
    if (w == 0) w = 1;
    if (h == 0) h = 1;
    return levelBytes(w, h);
}

S64 LLImageGL::getMipBytes(S32 discard_level) const
{
    if (discard_level < 0)
    {
        // Match getWidth/getHeight/getBytes: mCurrentDiscardLevel can still
        // be -1 if no discard has been set; treat that as "full resolution"
        // rather than shifting by a negative (which is UB).
        discard_level = llmax<S32>(mCurrentDiscardLevel, 0);
    }
    S32 w = mWidth>>discard_level;
    S32 h = mHeight>>discard_level;
    S64 res = levelBytes(w, h);
    if (mUseMipMaps)
    {
        while (w > 1 && h > 1)
        {
            w >>= 1; if (w == 0) w = 1;
            h >>= 1; if (h == 0) h = 1;
            res += levelBytes(w, h);
        }
    }
    return res;
}

S64 LLImageGL::levelBytes(S32 width, S32 height) const
{
    if (!isIntegerFormat(mFormatInternal))
    {
        return dataFormatBytes(mFormatPrimary, width, height);
    }
    const S64 bytes = (S64)width * (S64)height * (S64)pixel_bytes(mFormatPrimary, mFormatType);
    return (bytes + 3) & ~3;
}

bool LLImageGL::isJustBound() const
{
    return sLastFrameTime - mLastBindTime < 0.5f;
}

bool LLImageGL::getBoundRecently() const
{
    return (bool)(sLastFrameTime - mLastBindTime < MIN_TEXTURE_LIFETIME);
}

bool LLImageGL::getIsAlphaMask() const
{
    llassert_always(!sSkipAnalyzeAlpha);
    return mIsMask;
}

void LLImageGL::setTarget(const LLGLenum target, const ALTextureSlot::eTextureType bind_target)
{
    mTarget = target;
    mBindTarget = bind_target;
}

const S8 INVALID_OFFSET = -99 ;
void LLImageGL::setNeedsAlphaAndPickMask(bool need_mask)
{
    if(mNeedsAlphaAndPickMask != need_mask)
    {
        mNeedsAlphaAndPickMask = need_mask;
        // The alpha layout is worked out with the format, so the next upload works both out again.
        mFormatComponents = -1;

        if(mNeedsAlphaAndPickMask)
        {
            mAlphaOffset = 0 ;
        }
        else //do not need alpha mask
        {
            mAlphaOffset = INVALID_OFFSET ;
            clearAlphaAnalysis();

            // Invalidate pending jobs
            ++mAlphaAnalysisSerial;
        }
    }
}

void LLImageGL::calcAlphaChannelOffsetAndStride()
{
    if(mAlphaOffset == INVALID_OFFSET)//do not need alpha mask
    {
        return ;
    }

    mAlphaStride = -1 ;
    switch (mFormatPrimary)
    {
    case GL_LUMINANCE:
    case GL_ALPHA:
        mAlphaStride = 1;
        break;
    case GL_LUMINANCE_ALPHA:
        mAlphaStride = 2;
        break;
    case GL_RED:
    case GL_RGB:
    case GL_SRGB:
        // The texture the format is for has no alpha. Staged while an upload is in flight:
        // the texture on screen keeps its own until the new one publishes.
        mNeedsAlphaAndPickMask = false;
        setAlphaAnalysis(opaqueAlphaAnalysis());
        return; //no alpha channel.
    case GL_RED_INTEGER:
    case GL_RG_INTEGER:
    case GL_RGB_INTEGER:
    case GL_BGR_INTEGER:
    case GL_RGBA_INTEGER:
    case GL_BGRA_INTEGER:
        // Integers: even a fourth channel is a value, not coverage.
        mNeedsAlphaAndPickMask = false;
        setAlphaAnalysis(opaqueAlphaAnalysis());
        return;
    case GL_RGBA:
    case GL_SRGB_ALPHA:
        mAlphaStride = 4;
        break;
    case GL_BGRA:
        mAlphaStride = 4;
        break;
    default:
        break;
    }

    mAlphaOffset = -1 ;
    if (mFormatType == GL_UNSIGNED_BYTE)
    {
        mAlphaOffset = mAlphaStride - 1 ;
    }
    else if(is_little_endian())
    {
        if (mFormatType == GL_UNSIGNED_INT_8_8_8_8)
        {
            mAlphaOffset = 0 ;
        }
        else if (mFormatType == GL_UNSIGNED_INT_8_8_8_8_REV)
        {
            mAlphaOffset = 3 ;
        }
    }
    else //big endian
    {
        if (mFormatType == GL_UNSIGNED_INT_8_8_8_8)
        {
            mAlphaOffset = 3 ;
        }
        else if (mFormatType == GL_UNSIGNED_INT_8_8_8_8_REV)
        {
            mAlphaOffset = 0 ;
        }
    }

    if( mAlphaStride < 1 || //unsupported format
        mAlphaOffset < 0 || //unsupported type
        (mFormatPrimary == GL_BGRA && mFormatType != GL_UNSIGNED_BYTE)) //unknown situation
    {
        LL_WARNS() << "Cannot analyze alpha for image with format type " << std::hex << mFormatType << std::dec << LL_ENDL;

        mNeedsAlphaAndPickMask = false ;
        setAlphaAnalysis(AlphaAnalysis());
    }
}

// static
void LLImageGL::applySwizzleForDeprecatedFormat(ALTextureSlot::eTextureType type, U32 original_format)
{
    // Swizzle table is owned here; legacy raw-name producers
    // identify the format they originally asked for and let this routine
    // pick the matching mask. Keeps GL_TEXTURE_SWIZZLE_RGBA and the
    // mask-component constants out of the call sites.
    LLGLint mask[4];
    switch (original_format)
    {
    case GL_ALPHA:
        // Original meaning: byte goes to alpha, (R,G,B) read as (0,0,0).
        mask[0] = GL_ZERO; mask[1] = GL_ZERO; mask[2] = GL_ZERO; mask[3] = GL_RED;
        break;
    case GL_LUMINANCE:
        // Original meaning: byte replicated to RGB, alpha = 1.
        mask[0] = GL_RED;  mask[1] = GL_RED;  mask[2] = GL_RED;  mask[3] = GL_ONE;
        break;
    case GL_LUMINANCE_ALPHA:
        // Original meaning: first byte replicated to RGB, second byte → alpha.
        mask[0] = GL_RED;  mask[1] = GL_RED;  mask[2] = GL_RED;  mask[3] = GL_GREEN;
        break;
    default:
        return;  // not a format we re-express via swizzle
    }
    glTexParameteriv(ALTextureSlot::getInternalType(type), GL_TEXTURE_SWIZZLE_RGBA, mask);
}

// The format of an image without an explicit one, from its component count. Worked out
// once a count: beginUpload does it on the main thread before an upload is posted, so the
// upload on LLImageGLThread finds it done and writes none of what the main thread reads.
void LLImageGL::resolveComponentFormat(S8 components)
{
    if (components == mFormatComponents)
    {
        return;
    }

    switch (components)
    {
    case 1:
        // Single-channel -- used by font glyph maps, but the path is generic for
        // any 1-component upload. resolveDeprecatedFormat below stores it as R8,
        // read back through a gray-replicate swizzle.
        mFormatInternal = GL_LUMINANCE8;
        mFormatPrimary = GL_LUMINANCE;
        mFormatType = GL_UNSIGNED_BYTE;
        break;
    case 2:
        // Two-channel (luminance + alpha): RG8 under the matching swizzle, the
        // same way.
        mFormatInternal = GL_LUMINANCE8_ALPHA8;
        mFormatPrimary = GL_LUMINANCE_ALPHA;
        mFormatType = GL_UNSIGNED_BYTE;
        break;
    case 3:
        // sRGB, not linear. The bits are identical either way -- an 8-bit texture
        // uploaded from a JPEG2000/PNG/TGA asset already holds sRGB-encoded values --
        // so this changes nothing about what is stored, only whether GL is willing to
        // decode it. Sampling is unaffected unless a bind asks for the decode with
        // ALSampler::SRGBDecode, and nothing does by default.
        //
        // Worth stating why it cannot be narrowed to colour textures: the same
        // LLViewerFetchedTexture object serves whichever glTF slot references its UUID
        // (see fetch_texture), so one image can be base colour for one material and a
        // normal map for another. The format cannot know; only the bind can. Which is
        // exactly the split the sampler work established.
        mFormatInternal = GL_SRGB8;
        mFormatPrimary = GL_RGB;
        mFormatType = GL_UNSIGNED_BYTE;
        break;
    case 4:
        mFormatInternal = GL_SRGB8_ALPHA8;
        mFormatPrimary = GL_RGBA;
        mFormatType = GL_UNSIGNED_BYTE;
        break;
    default:
        LL_ERRS() << "Bad number of components for texture: " << (U32)components << LL_ENDL;
    }

    // Calc alpha layout first (keys on the deprecated names), then
    // rewrite the format to core-profile-valid forms.
    calcAlphaChannelOffsetAndStride() ;
    resolveDeprecatedFormat();
    mFormatComponents = components;
}

void LLImageGL::resolveDeprecatedFormat()
{
    // resolveUploadFormat rewrites the deprecated source/internal formats locally (so the
    // allocation gets the right backing storage), but its rewrites don't propagate to
    // LLImageGL's mFormatPrimary / mFormatInternal members. Subsequent native uploads,
    // readBackRaw, and scaleDown read those members, so rewrite them at format-resolution
    // time and remember the original format for the matching native or legacy swizzle.
    //
    switch (mFormatPrimary)
    {
    case GL_ALPHA:
        mDeprecatedSourceFormat = mFormatPrimary;
        mFormatPrimary = GL_RED;
        mFormatInternal = GL_R8;
        break;
    case GL_LUMINANCE:
        mDeprecatedSourceFormat = mFormatPrimary;
        mFormatPrimary = GL_RED;
        mFormatInternal = GL_R8;
        break;
    case GL_LUMINANCE_ALPHA:
        mDeprecatedSourceFormat = mFormatPrimary;
        mFormatPrimary = GL_RG;
        mFormatInternal = GL_RG8;
        break;
    default:
        // Reset so a stale value from a prior format doesn't make
        // a later upload apply the wrong swizzle on a fresh non-deprecated
        // format (e.g. setExplicitFormat(LUMINANCE) followed by
        // setExplicitFormat(RGBA), or an LLImageGL whose component count
        // changed across native upload calls).
        mDeprecatedSourceFormat = 0;
        break;
    }
}

LLImageGL::AlphaAnalysis LLImageGL::analyzeAlphaData(
    const void* data_in,
    U32 w,
    U32 h,
    S8 alpha_offset,
    S8 alpha_stride)
{
    if (!data_in || alpha_stride < 1)
    {
        return AlphaAnalysis();
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;

    U32 length = w * h;
    U32 alphatotal = 0;
    U32 max_alpha = 0;
    U32 min_alpha = 255;

    U32 sample[16];
    memset(sample, 0, sizeof(U32) * 16);

    // generate histogram of quantized alpha.
    // also add-in the histogram of a 2x2 box-sampled version.  The idea is
    // this will mid-skew the data (and thus increase the chances of not
    // being used as a mask) from high-frequency alpha maps which
    // suffer the worst from aliasing when used as alpha masks.
    if (w >= 2 && h >= 2)
    {
        // Walk only complete 2x2 quads. Odd dimensions previously hit
        // asserts in debug and read OOB on the trailing row/column in
        // release (`current[w * alpha_stride]` indexes one row down,
        // which doesn't exist for the last odd row). Trailing odd row
        // and column are dropped from the histogram; the remaining
        // sample is still representative for the mask classifier.
        const U32 paired_w = w & ~1u;
        const U32 paired_h = h & ~1u;
        const GLubyte* rowstart = ((const GLubyte*)data_in) + alpha_offset;
        for (U32 y = 0; y < paired_h; y += 2)
        {
            const GLubyte* current = rowstart;
            for (U32 x = 0; x < paired_w; x += 2)
            {
                const U32 s1 = current[0];
                alphatotal += s1;
                const U32 s2 = current[w * alpha_stride];
                alphatotal += s2;
                current += alpha_stride;
                const U32 s3 = current[0];
                alphatotal += s3;
                const U32 s4 = current[w * alpha_stride];
                alphatotal += s4;
                current += alpha_stride;

                ++sample[s1 / 16];
                ++sample[s2 / 16];
                ++sample[s3 / 16];
                ++sample[s4 / 16];

                const U32 asum = (s1 + s2 + s3 + s4);
                alphatotal += asum;
                sample[asum / (16 * 4)] += 4;

                max_alpha = llmax(max_alpha, llmax(llmax(s1, s2), llmax(s3, s4)));
                min_alpha = llmin(min_alpha, llmin(llmin(s1, s2), llmin(s3, s4)));
            }

            rowstart += 2 * w * alpha_stride;
        }
        // Two histogram entries per source texel over the paired region.
        length = 2 * paired_w * paired_h;

        // The trailing odd row and column are left out of the histogram, but one of their texels may be the only
        // one with any alpha, or the only one without.
        const GLubyte* alpha = ((const GLubyte*)data_in) + alpha_offset;
        if (paired_w < w)
        {
            for (U32 y = 0; y < h; ++y)
            {
                const U32 a = alpha[((size_t)y * w + paired_w) * alpha_stride];
                max_alpha = llmax(max_alpha, a);
                min_alpha = llmin(min_alpha, a);
            }
        }
        if (paired_h < h)
        {
            for (U32 x = 0; x < w; ++x)
            {
                const U32 a = alpha[((size_t)paired_h * w + x) * alpha_stride];
                max_alpha = llmax(max_alpha, a);
                min_alpha = llmin(min_alpha, a);
            }
        }
    }
    else
    {
        const unsigned char* current = ((const unsigned char*)data_in) + alpha_offset;
        for (U32 i = 0; i < length; i++)
        {
            const U32 s1 = *current;
            alphatotal += s1;
            ++sample[s1 / 16];
            max_alpha = llmax(max_alpha, s1);
            min_alpha = llmin(min_alpha, s1);
            current += alpha_stride;
        }
    }

    // if more than 1/16th of alpha samples are mid-range, this
    // shouldn't be treated as a 1-bit mask

    // also, if all of the alpha samples are clumped on one half
    // of the range (but not at an absolute extreme), then consider
    // this to be an intentional effect and don't treat as a mask.

    U32 midrangetotal = 0;
    for (U32 i = 2; i < 13; i++)
    {
        midrangetotal += sample[i];
    }
    U32 lowerhalftotal = 0;
    for (U32 i = 0; i < 8; i++)
    {
        lowerhalftotal += sample[i];
    }
    U32 upperhalftotal = 0;
    for (U32 i = 8; i < 16; i++)
    {
        upperhalftotal += sample[i];
    }

    AlphaAnalysis analysis;
    analysis.mMaxAlpha = (U8)max_alpha;
    analysis.mMinAlpha = (U8)min_alpha;
    if (midrangetotal > length / 48 ||
        (lowerhalftotal == length && alphatotal != 0) ||
        (upperhalftotal == length && alphatotal != 255 * length))
    {
        analysis.mIsMask = false; // not suitable for masking
    }
    else
    {
        analysis.mIsMask = true; // is a mask
    }
    return analysis;
}

void LLImageGL::analyzeAlpha(const void* data_in, U32 w, U32 h)
{
    // if mNeedsAlphaAndPickMask is true, then offset and stride are supposed to be valid.
    if (!data_in || sSkipAnalyzeAlpha || !mNeedsAlphaAndPickMask)
        return;

    // Already on a worker thread or a small image - analyze immediately
    if (!on_main_thread() || (w < 64 && h < 64))
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
        // Should have been incremented by destroyGLTexture,
        // but increment either way, for extra safety.
        ++mAlphaAnalysisSerial;

        setAlphaAnalysis(analyzeAlphaData(data_in, w, h, mAlphaOffset, mAlphaStride));
        return;
    }

    // On main thread - defer to worker thread

    // Capture context
    const S8 alpha_offset = mAlphaOffset;
    const S8 alpha_stride = mAlphaStride;
    const U32 request_serial = ++mAlphaAnalysisSerial;

    // Copy data for worker thread
    const size_t data_size = size_t(w) * size_t(h) * size_t(alpha_stride);
    U8* data_copy = new (std::nothrow) U8[data_size];

    if (!data_copy)
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
        setAlphaAnalysis(analyzeAlphaData(data_in, w, h, alpha_offset, alpha_stride));
        return;
    }
    memcpy(data_copy, static_cast<const U8*>(data_in), data_size);

    // Viewer can rapidly switch between lods, which would invalidate
    // previous analysis results.
    // Use a serial number to filter out obsolete analysis results.

    auto mainq = mMainQueue.lock();

    // Get the GL thread queue
    auto workerq = LLImageGLThread::sEnabledTextures ?
        LL::WorkQueue::getInstance("LLImageGL") : // Use the image processing queue if available
        LL::WorkQueue::getInstance("General"); // Fallback to general

    bool posted_job = false;
    if (mainq && workerq)
    {
        posted_job = mainq->postTo(
            workerq,
            // Worker thread: analyze alpha
            [data_copy, w, h, alpha_offset, alpha_stride]() -> AlphaAnalysis
        {
            LL_PROFILE_ZONE_NAMED("Deffered alpha mask analysis");
            const AlphaAnalysis analysis = LLImageGL::analyzeAlphaData(data_copy, w, h, alpha_offset, alpha_stride);
            delete[] data_copy;
            return analysis;
        },
            // Main thread: apply result
            [this, request_serial](AlphaAnalysis analysis)
        {
            // Only apply if no newer analysis has been requested
            if (mAlphaAnalysisSerial == request_serial)
            {
                mIsMask = analysis.mIsMask;
                mMaxAlpha = analysis.mMaxAlpha;
                mMinAlpha = analysis.mMinAlpha;
            }
            unref();
        }
        );

        if (posted_job)
        {
            // Keep the texture alive until the completion callback runs, which
            // is what drops this reference again. Taken only once the post has
            // succeeded: the callback runs on this same queue and so cannot
            // execute before we return, whereas a reference taken up front has
            // to be dropped again when the post fails -- and that balanced pair
            // deletes the texture outright while its reference count is still
            // zero, as it is whenever native creation runs from the
            // LLImageGL(LLImageRaw*) constructor.
            ref();

            // Conservative default until analysis completes
            clearAlphaAnalysis();
        }
    }
    if (!posted_job)
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
        // Queues not available - fall back to synchronous analysis
        delete[] data_copy;
        setAlphaAnalysis(analyzeAlphaData(data_in, w, h, mAlphaOffset, mAlphaStride));
    }
}

//----------------------------------------------------------------------------
void LLImageGL::freePickMask()
{
    if (mPickMask != NULL)
    {
        delete [] mPickMask;
    }
    mPickMask = NULL;
    mPickMaskWidth = mPickMaskHeight = 0;
}

bool LLImageGL::isCompressed() const
{
    llassert(mFormatPrimary != 0);
    // *NOTE: Not all compressed formats are included here.
    bool is_compressed = false;
    switch (mFormatPrimary)
    {
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
        is_compressed = true;
        break;
    default:
        break;
    }
    return is_compressed;
}

//----------------------------------------------------------------------------
void LLImageGL::updatePickMask(S32 width, S32 height, const U8* data_in)
{
    if(!mNeedsAlphaAndPickMask)
    {
        return ;
    }

    if (mFormatType != GL_UNSIGNED_BYTE ||
        ((mFormatPrimary != GL_RGBA)
      && (mFormatPrimary != GL_SRGB_ALPHA)))
    {
        //cannot generate a pick mask for this texture
        setPickMask(nullptr, 0, 0);
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;

    // Built aside and handed over whole: setPickMask holds it back until the texture it
    // was made from is the one mTexName names, and getMask may be reading the published
    // mask from the main thread while this runs on the LLImageGL thread.
    //
    // The walk below steps two texels at a time, so a row holds ceil(width/2) cells and
    // getMask has to use that stride, or odd widths read from the wrong row.
    const U32 stride_w = (U32)((width + 1) / 2);
    const U32 stride_h = (U32)((height + 1) / 2);
    const U32 size = (stride_w * stride_h + 7) / 8; // pixelcount-to-bits
    U8* mask = new U8[size];
    memset(mask, 0, size);

    U32 pick_bit = 0;

    for (S32 y = 0; y < height; y += 2)
    {
        for (S32 x = 0; x < width; x += 2)
        {
            U8 alpha = data_in[(y*width+x)*4+3];

            if (alpha > 32)
            {
                U32 pick_idx = pick_bit/8;
                U32 pick_offset = pick_bit%8;
                llassert(pick_idx < size);

                mask[pick_idx] |= 1 << pick_offset;
            }

            ++pick_bit;
        }
    }

    setPickMask(mask, (U16)stride_w, (U16)stride_h);
}

//bool LLImageGL::getMask(const LLVector2 &tc)
// [RLVa:KB] - Checked: RLVa-2.2 (@setoverlay)
bool LLImageGL::getMask(const LLVector2 &tc) const
// [/RLVa:KB]
{
    bool res = true;

    if (mPickMask)
    {
        F32 u,v;
        if (tc.isFinite()) [[likely]]
        {
            u = tc.mV[0] - floorf(tc.mV[0]);
            v = tc.mV[1] - floorf(tc.mV[1]);
        }
        else
        {
            LL_WARNS_ONCE("render") << "Ugh, non-finite u/v in mask pick" << LL_ENDL;
            u = v = 0.f;
            // removing assert per EXT-4388
            // llassert(false);
        }

        if (u < 0.f || u > 1.f ||
                v < 0.f || v > 1.f) [[unlikely]]
        {
            LL_WARNS_ONCE("render") << "Ugh, u/v out of range in image mask pick" << LL_ENDL;
            u = v = 0.f;
            // removing assert per EXT-4388
            // llassert(false);
        }

        S32 x = llfloor(u * mPickMaskWidth);
        S32 y = llfloor(v * mPickMaskHeight);

        // Defensive clamp to the last valid index. The previous version
        // tested `> mPickMaskWidth` and clamped to `mPickMaskWidth` itself,
        // which is one past the valid range and addresses the start of the
        // next row's bits. Use `>=` and clamp to width-1 / height-1.
        if (x >= mPickMaskWidth) [[unlikely]]
        {
            LL_WARNS_ONCE("render") << "Ooh, width overrun on pick mask read, that coulda been bad." << LL_ENDL;
            x = mPickMaskWidth > 0 ? mPickMaskWidth - 1 : 0;
        }
        if (y >= mPickMaskHeight) [[unlikely]]
        {
            LL_WARNS_ONCE("render") << "Ooh, height overrun on pick mask read, that woulda been bad." << LL_ENDL;
            y = mPickMaskHeight > 0 ? mPickMaskHeight - 1 : 0;
        }

        S32 idx = y*mPickMaskWidth+x;
        S32 offset = idx%8;

        res = (mPickMask[idx/8] & (1 << offset)) != 0;
    }

    return res;
}

void LLImageGL::setCurTexSizebar(S32 index, bool set_pick_size)
{
    sCurTexSizeBar = index ;

    if(set_pick_size)
    {
        sCurTexPickSize = (1 << index) ;
    }
    else
    {
        sCurTexPickSize = -1 ;
    }
}
void LLImageGL::resetCurTexSizebar()
{
    sCurTexSizeBar = -1 ;
    sCurTexPickSize = -1 ;
}

// The internal format glTexStorage* should be handed. Block-compressed textures carry
// their (sized) compressed format in mFormatPrimary rather than mFormatInternal, which is
// the convention the glCompressedTexImage2D calls already follow.
S32 LLImageGL::getStorageInternalFormat() const
{
    return isCompressed() ? mFormatPrimary : mFormatInternal;
}

// Allocate backing storage for the currently-bound texture at the given level-0 size,
// and record the VRAM accounting for it. Callers replacing an existing texture are
// responsible for releasing the old accounting (free_tex_image) themselves.
void LLImageGL::allocateTextureStorage(S32 width, S32 height, bool has_mips)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;

    mMipLevels = has_mips ? calcMipLevelCount(width, height) : 1;

    assertStorageAllocatable(mTarget, getStorageInternalFormat());

    {
        LL_PROFILE_ZONE_NAMED("glTexStorage2D");
        // Dimensions, format and level count are fixed for the lifetime of this texture
        // object. Every later write must be a sub-image, and any change of size or format
        // must create a new texture -- see scaleDown.
        glTexStorage2D(mTarget, mMipLevels, getStorageInternalFormat(), width, height);
        skipSRGBDecode(mTarget);
        mStorageAllocated = true;
        mStorage.mWidth  = width;
        mStorage.mHeight = height;
        mStorage.mLevels = mMipLevels;
        mStorage.mFormat = getStorageInternalFormat();
    }

    alloc_tex_image(width, height, mFormatInternal, 1, has_mips);
#if LL_DARWIN
    // While both resources exist during migration, the GL allocation is the
    // image's one logical budget entry.
    clearMetalTextureAccounting();
#endif
}

bool LLImageGL::storageFits(S32 width, S32 height) const
{
    // Nothing allocated yet: setImage allocates it at this size.
    if (!mStorageAllocated || mStorage.mLevels == 0)
    {
        return true;
    }

    return mStorage.mWidth == width
        && mStorage.mHeight == height
        && mStorage.mFormat == getStorageInternalFormat()
        && mStorage.mLevels == (mUseMipMaps ? calcMipLevelCount(width, height) : 1);
}

// static
void LLImageGL::generateMipmaps(U32 target)
{
    // See the header comment: clearing the sampler makes the texture object's own
    // SKIP_DECODE (written at allocation) govern the mip filter's colour space, instead of
    // whatever sampler the last draw left on the slot. bindSampler dedups, so this is free
    // when the slot is already sampler-less.
    gGL.getTextureSlot(0)->bindSampler(0);
    glGenerateMipmap(target);
}

bool LLImageGL::scaleDown(S32 desired_discard)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;

    if (mTarget != GL_TEXTURE_2D
        || mFormatInternal == -1
        || isCompressed()
        || isIntegerFormat(mFormatInternal))
    {
        return false;
    }

    // The image worker may be replacing the resource and discard metadata.
    // Retry on a later frame instead of publishing over that transaction.
    if (mUploadInFlight)
    {
        return false;
    }

    desired_discard = llmin(desired_discard, mMaxDiscardLevel);
    if (desired_discard <= mCurrentDiscardLevel)
    {
        return false;
    }

#if !LL_DARWIN
    return false;
#else
    // This fork has no GL texture-copy fallback. A scale-down is valid only
    // for the authoritative native texture and is published transactionally
    // after the Metal copy/resample command has committed.
    if (!mMetalTexture || mTexName != 0)
    {
        return false;
    }

    const S32 source_mip = desired_discard - mCurrentDiscardLevel;
    const S32 desired_width = getWidth(desired_discard);
    const S32 desired_height = getHeight(desired_discard);
    if (!preserveMetalScaleDown(static_cast<U32>(source_mip),
                                static_cast<U32>(desired_width),
                                static_cast<U32>(desired_height)))
    {
        return false;
    }

    mPendingMetalTexture.reset();
    mCurrentDiscardLevel = desired_discard;
    mMipLevels = static_cast<S32>(
        mMetalTexture->descriptor().mipmap_level_count);
    mTextureMemory = S64Bytes(metal_texture_bytes(*mMetalTexture));
    updateMetalTextureAccounting();
    mLastBindTime = sLastFrameTime;
    llassert(!getHasGLTexture());
    return true;
#endif
}

#if LL_IMAGEGL_THREAD_CHECK
void LLImageGL::checkActiveThread()
{
    llassert(mActiveThread == LLThread::currentID());
}
#endif

//----------------------------------------------------------------------------


LLImageGLThread::LLImageGLThread(LLWindow* window)
    // We want exactly one thread.
    : LL::ThreadPool("LLImageGL", 1)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    (void)window;

    if (!LLImageGL::isMetalTextureAuthority())
    {
        LL_WARNS("LLImageGL")
            << "Native image worker requires authoritative Metal textures; "
               "asynchronous texture and media uploads are disabled"
            << LL_ENDL;
        sEnabledTextures = false;
        sEnabledMedia = false;
        return;
    }

    // Native image uploads need the worker queue, not a per-thread graphics
    // context. Preserve asynchronous decode/upload and Metal command batching.
    LL::ThreadPool::start();
}

LLImageGLThread::~LLImageGLThread()
{
    close();
}

void LLImageGLThread::run()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    sOnImageGLThread = true;
    LL::ThreadPool::run();

    // The queue may have been closed while an upload task was executing, preventing its
    // commit barrier from being posted. Complete that last native batch before
    // the worker and its pending publications are torn down.
    sWorkerMetalUploadBatch.mFlushQueued = false;
    flush_worker_metal_upload_batch();
    sOnImageGLThread = false;
}

