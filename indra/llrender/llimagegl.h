/**
 * @file llimagegl.h
 * @brief Object for managing images and their textures
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


#ifndef LL_LLIMAGEGL_H
#define LL_LLIMAGEGL_H

#include "llimage.h"

#include "llgltypes.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "v2math.h"
#include "llunits.h"
#include "llthreadsafequeue.h"
#include "llrender.h"
#include "threadpool.h"
#include "workqueue.h"
#include <boost/unordered_set.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <span>
#include <vector>

#define LL_IMAGEGL_THREAD_CHECK 0 //set to 1 to enable thread debugging for ImageGL

class LLWindow;
#if LL_DARWIN
class ALMetalCommandBuffer;
class ALMetalDevice;
class ALMetalTexture;
#endif

#define BYTES_TO_MEGA_BYTES(x) ((x) >> 20)
#define MEGA_BYTES_TO_BYTES(x) ((x) << 20)

//============================================================================
// NOTE: thread-safe refcount rather than plain LLRefCount because native upload
// publication takes a worker reference which is released on the main thread.
// Refcount traffic here is per-texture-lifetime (LLImageGL lives in member slots like
// LLGLTexture::mGLTexturep, it isn't copied through hot loops), so the atomic is not on
// any measured path.
class LLImageGL : public LLThreadSafeRefCount
{
    friend class ALTextureSlot;
public:

    // call once per frame
    static void updateClass();

    // Main thread: publish every off-thread native upload whose Metal batch has committed.
    // Returns how many still wait.
    static U32 publishUploads();

    // Main thread, at frame end: retained publication hook. Metal completion tokens need
    // no active poll; publishUploads observes them at the next publication point.
    static void pollUploads();

    // Main thread at shutdown: allow up to one second for native batches to commit and
    // publish; cleanup discards anything still pending.
    static void finishUploads();

    // Estimate logical bytes owned by published native images, including every
    // allocated mip, array slice, cube face and multisample plane.
    //
    // NOTE: viewer consumers (llviewertexture.cpp, lltextureview.cpp)
    // historically scaled this by 2x via a /512 divisor instead of /1024
    // when converting to MB. That fudge predates per-mip accounting and
    // also compensates for driver-side overhead (descriptor structs, page
    // alignment). Leave it alone until the budget code is reconciled
    // holistically.
    static U64 getTextureBytesAllocated();

    // Actual bytes currently allocated by the native Metal device, including
    // streamed images, render targets, buffers, staging and driver-visible
    // resources. Returns zero when no native device is active.
    static U64 getDeviceBytesAllocated();

    // Size calculation
    //
    // dataFormatBits/Bytes report the *tight host-side* layout — i.e. the
    // packed CPU buffer size for glTexImage2D/glTexSubImage2D uploads
    // (matching the pixel format, at a byte a component for an unsized
    // one). Use these for source-pointer arithmetic.
    //
    // dataFormatVRAMBits/Bytes report the *driver-side* allocation for
    // VRAM accounting. They differ from the host values for formats that
    // drivers pad (RGB8 -> RGBX, RGB16F -> RGBA16F, RGB32F -> RGBA32F,
    // DEPTH_COMPONENT24 -> 32-bit). For other formats they delegate to
    // dataFormatBits/Bytes.
    static S32 dataFormatBits(S32 dataformat);
    static S64 dataFormatBytes(S32 dataformat, S32 width, S32 height);
    static S32 dataFormatVRAMBits(S32 dataformat);
    static S64 dataFormatVRAMBytes(S32 dataformat, S32 width, S32 height);
    static S32 dataFormatComponents(S32 dataformat);

    // The sized integer colour formats: R32UI, RG16I, RGB10_A2UI and the rest. A shader reads their texels as
    // integers, through an isampler or a usampler, and GL neither filters them nor builds their mips: a filtering
    // sampler leaves such a texture incomplete, reading 0, and glGenerateMipmap refuses it.
    static bool isIntegerFormat(S32 internal_format);
    // Of those, the ones whose texels are signed, read through an isampler.
    static bool isSignedIntegerFormat(S32 internal_format);
    // The pixel format an integer internal format's host data is laid out in: GL_RED_INTEGER for R32UI,
    // GL_RGBA_INTEGER for RGBA16I. 0 for any other format.
    static U32 integerPixelFormat(S32 internal_format);

    bool updateBindStats() const ;
    F32 getTimePassedSinceLastBound();
    void forceUpdateBindStats(void) const;

    // needs to be called every frame
    static void updateStats(F64 current_time);

    // cleanup GL state
    static void destroyGL();

    static bool checkSize(S32 width, S32 height);

    // Number of mip levels in a full pyramid for these level-0 dimensions, counting
    // level 0 itself: 256x256 -> 9. This is a COUNT, which is what glTexStorage* takes.
    // Each dimension clamps at 1 independently, so 1024x16 is 11 levels, not 5.
    static S32 calcMipLevelCount(S32 width, S32 height);

    //for server side use only.
    // Not currently necessary for LLImageGL, but required in some derived classes,
    // so include for compatability
    static bool create(LLPointer<LLImageGL>& dest, bool usemipmaps = true);
    static bool create(LLPointer<LLImageGL>& dest, U32 width, U32 height, U8 components, bool usemipmaps = true);

public:
    LLImageGL(bool usemipmaps = true);
    LLImageGL(U32 width, U32 height, U8 components, bool usemipmaps = true);
    LLImageGL(const LLImageRaw* imageraw, bool usemipmaps = true);

    // What analyzeAlphaData finds in a level's alpha.
    struct AlphaAnalysis
    {
        // Whether it is near enough to 0 or 1 everywhere to be tested as a mask rather than blended.
        bool mIsMask = false;
        // The highest alpha of any texel.
        U8 mMaxAlpha = 255;
        // The lowest alpha of any texel. With mMaxAlpha's default, an analysis that read nothing bounds every alpha.
        U8 mMinAlpha = 0;
    };

protected:
    virtual ~LLImageGL();

    static AlphaAnalysis analyzeAlphaData(const void* data_in, U32 w, U32 h, S8 alpha_offset, S8 alpha_stride);
    // What a texture with no alpha channel analyses as: every texel opaque.
    static AlphaAnalysis opaqueAlphaAnalysis() { AlphaAnalysis analysis; analysis.mMinAlpha = 255; return analysis; }
    void analyzeAlpha(const void* data_in, U32 w, U32 h);
    void calcAlphaChannelOffsetAndStride();

    // Rewrite mFormatPrimary/mFormatInternal from deprecated forms
    // (GL_LUMINANCE / GL_ALPHA / GL_LUMINANCE_ALPHA) to their core-profile-
    // valid equivalents (GL_RED + R8 with a {R,R,R,1} swizzle, etc.). Records
    // the source semantics so native descriptors apply the matching swizzle.
    // No-op for non-deprecated formats.
    //
    // Must run AFTER calcAlphaChannelOffsetAndStride: the alpha layout
    // calc keys on the deprecated names (GL_LUMINANCE_ALPHA stride=2 etc).
    // After rewrite, mAlphaStride/mAlphaOffset are still correct for the
    // physical data layout (1 byte per channel either way), and subsequent
    // native uploads / readBackRaw / scaleDown read the live format names.
    void resolveDeprecatedFormat();

public:
    virtual void dump();    // debugging info to LL_INFOS()

    bool setSize(S32 width, S32 height, S32 ncomponents, S32 discard_level = -1);
    void setComponents(S32 ncomponents) { mComponents = (S8)ncomponents ;}

    // Publish decoded-image geometry, format and discard state without creating
    // GPU storage. Any obsolete native backing is released.
    bool setImageMetadata(S32 discard_level, const LLImageRaw* imageraw);

    // Main thread: run fn once the native worker upload has published, or
    // now if none is waiting. A caller finishing an off-thread upload goes through this, so
    // what it does next -- ending the snapshot, letting another upload start -- follows the
    // texture onto the screen rather than racing it.
    void afterPublish(std::function<void()> fn);
    // As above, with whether every queued native generation committed successfully.
    void afterPublish(std::function<void(bool)> fn);

    // Read back a raw image for this discard level, if it exists
    bool readBackRaw(S32 discard_level, LLImageRaw* imageraw, bool compressed_ok) const;
    void destroyGLTexture();
    void forceToInvalidateGLTexture();

    void setExplicitFormat(LLGLint internal_format, LLGLenum primary_format, LLGLenum type_format = 0, bool swap_bytes = false);
    void setComponents(S8 ncomponents) { mComponents = ncomponents; }

    // While an off-thread upload is in flight the members below describe the texture
    // being built, not the published GPU generation. Consumers pair these getters with
    // readBackRaw()/getHasGPUTexture(), so they must report the generation that remains
    // visible until publication completes.
    S32  getDiscardLevel() const        { return mUploadInFlight ? mPublished.mCurrentDiscardLevel : mCurrentDiscardLevel; }
    S32  getMaxDiscardLevel() const     { return mUploadInFlight ? mPublished.mMaxDiscardLevel : mMaxDiscardLevel; }

    // override the current discard level
    // should only be used for local textures where you know exactly what you're doing
    void setDiscardLevel(S32 level) { mCurrentDiscardLevel = level; }

    S32  getCurrentWidth() const { return mWidth ;}
    S32  getCurrentHeight() const { return mHeight ;}
    S32  getWidth(S32 discard_level = -1) const;
    S32  getHeight(S32 discard_level = -1) const;
    U8   getComponents() const { return (U8)(mUploadInFlight ? mPublished.mComponents : mComponents); }
    S64Bytes getTextureMemory() const { return mUploadInFlight ? mPublished.mTextureMemory : mTextureMemory; }
    S64  getBytes(S32 discard_level = -1) const;
    S64  getMipBytes(S32 discard_level = -1) const;
    bool getBoundRecently() const;
    bool isJustBound() const;
    bool getHasExplicitFormat() const { return mHasExplicitFormat; }
    LLGLenum getPrimaryFormat() const { return mUploadInFlight ? mPublished.mFormatPrimary : mFormatPrimary; }
    // The source was alpha only (GL_ALPHA). It is held as R8 under a {0,0,0,R}
    // swizzle, so getPrimaryFormat() cannot tell it from a luminance source;
    // the format it was asked for can.
    bool isAlphaOnly() const { return (mUploadInFlight ? mPublished.mDeprecatedSourceFormat : mDeprecatedSourceFormat) == GL_ALPHA; }
    LLGLenum getFormatType() const { return mUploadInFlight ? mPublished.mFormatType : mFormatType; }

    // Native storage-format metadata. Block-compressed textures keep their sized
    // format in mFormatPrimary rather than mFormatInternal.
    S32 getStorageInternalFormat() const;

    bool getHasGLTexture() const { return mTexName != 0; }
    // Backend-neutral residency used by lifecycle/pressure code. During the
    // transition an image may be GL-backed, dual-backed, or Metal-authoritative
    // after deliberately retiring its GL name.
    bool getHasGPUTexture() const
    {
#if LL_DARWIN
        return mTexName != 0 || mMetalTexture != nullptr;
#else
        return mTexName != 0;
#endif
    }
    LLGLuint getTexName() const { return mTexName; }
#if LL_DARWIN
    ALMetalTexture* getMetalTexture() const noexcept { return mMetalTexture.get(); }
    // Allocate a descriptor-compatible private render target for a GPU-owned
    // producer (for example a shared CEF IOSurface). The caller encodes and
    // commits the write before publishing it with adoptMetalRenderTexture().
    std::unique_ptr<ALMetalTexture> createMetalRenderTexture(
        std::string* error = nullptr) const;
    // Render passes that reproduce a GL-owned write create a separate native
    // texture, let GL finish its authoritative fallback, then publish the
    // native result here. The descriptor is checked against this image before
    // ownership changes.
    bool adoptMetalRenderTexture(std::unique_ptr<ALMetalTexture> texture);
    // A native producer publishes the rendered texture as the sole backing.
    // Descriptor validation is identical to adoptMetalRenderTexture(), but an
    // earlier GL name is retired and native memory/category metadata becomes
    // authoritative.
    bool adoptMetalRenderTextureAuthoritative(
        std::unique_ptr<ALMetalTexture> texture, S32 category);
    // A producer that updates an already-adopted render-target mirror in place
    // advances the same content generation as an ordinary upload.
    void markMetalRenderTextureUpdated() noexcept;
    // A successful GL write paired with a failed native write must not leave a
    // stale mirror eligible for an exact Metal frame.
    void discardMetalTexture() noexcept;
#endif
    // What the texture holds, as a value no texture has held before: taken again by every upload into it, in place or
    // not. A pass that keeps what it drew tells by it whether the pixels it sampled are as they were.
    U64 getContentStamp() const { return mContentStamp.load(std::memory_order_relaxed); }

    bool getIsAlphaMask() const;
    // The highest alpha of the level its alpha was last analysed at (analyzeAlpha), or 255 where it was not: no
    // alpha channel, an alpha it cannot read, or an analysis not yet back.
    U8 getMaxAlpha() const { return mMaxAlpha; }
    // The lowest alpha of that level, 255 where there is no alpha channel, or 0 where it was not analysed: an alpha
    // it cannot read, or an analysis not yet back.
    U8 getMinAlpha() const { return mMinAlpha; }

    bool getIsResident(bool test_now = false); // not const

    void setTarget(const LLGLenum target, const ALTextureSlot::eTextureType bind_target);

    ALTextureSlot::eTextureType getTarget(void) const { return mBindTarget; }
    bool isGLTextureCreated(void) const { return mGLTextureCreated.load(std::memory_order_relaxed); }
    void setGLTextureCreated (bool initialized) { mGLTextureCreated.store(initialized, std::memory_order_relaxed); }

    bool getUseMipMaps() const { return mUseMipMaps; }
    void setUseMipMaps(bool usemips) { mUseMipMaps = usemips; }
    void setHasMipMaps(bool hasmips) { mHasMipMaps = hasmips; }
    // Build the pick mask from level-0 RGBA8 data. During an upload in flight, or off the
    // main thread, the result waits for the texture it describes to publish.
    void updatePickMask(S32 width, S32 height, const U8* data_in);
// [RLVa:KB] - Checked: RLVa-2.2 (@setoverlay)
    bool getMask(const LLVector2 &tc) const;
// [/RLVa:KB]
//  bool getMask(const LLVector2 &tc);

    // NO sampling state here, and none coming back.
    //
    // A texture carries data plus facts about itself (dimensions, format, swizzle, whether it
    // has a mip chain). How it is READ belongs to the pass reading it: two materials can
    // reference one image and want different wrap modes -- glTF specifies sampler state per
    // texture reference for exactly that reason -- so an image that decided its own filtering
    // had to pick a winner and be wrong for everyone else. Name an ALSampler at the bind.
    LLGLenum getTexTarget()const { return mTarget; }

    void init(bool usemipmaps);
    virtual void cleanup(); // Clean up the LLImageGL so it can be reinitialized.  Be careful when using this in derived class destructors

    void setNeedsAlphaAndPickMask(bool need_mask);

#if LL_IMAGEGL_THREAD_CHECK
    // thread debugging
    std::thread::id mActiveThread;
    void checkActiveThread();
#endif

    // scale down to the desired discard level using GPU
    // returns true if texture was scaled down
    // desired discard will be clamped to max discard
    // if desired discard is less than or equal to current discard, no scaling will occur
    // only works for GL_TEXTURE_2D target
    bool scaleDown(S32 desired_discard);

public:
    // Various GL/Rendering options
    S64Bytes mTextureMemory;
    mutable F64  mLastBindTime; // last time this was bound, by discard level

private:
    void freePickMask();
    bool isCompressed() const;

    // Bytes of one level at this size, as getBytes and getMipBytes count it: an integer level by its pixel format AND
    // type -- an R32UI texel is four bytes, which GL_RED_INTEGER alone cannot say -- and any other by dataFormatBytes
    // of its pixel format, a byte a component. Rounded up to four, as dataFormatBytes rounds.
    S64 levelBytes(S32 width, S32 height) const;

    // The alpha facts an upload derives, routed through mPendingAlpha when stagesAlpha().
    // setPickMask takes ownership of mask, which may be null for "no pick mask".
    bool stagesAlpha() const;
    void setPickMask(U8* mask, U16 width, U16 height);
    void setAlphaAnalysis(const AlphaAnalysis& analysis);

    // Sets the published analysis to what a texture without a readable alpha holds: no mask, and opaque.
    void clearAlphaAnalysis();
    void publishPendingAlpha();
    void discardPendingAlpha();

    // Warn when an upload runs off the main thread without the snapshot beginUpload takes.
    void checkUploadBegun() const;

    LLPointer<LLImageRaw> mSaveData; // used for destroyGL/restoreGL
    LL::WorkQueue::weak_t mMainQueue;
    U8* mPickMask;  //downsampled bitmap approximation of alpha channel.  NULL if no alpha channel
    U16 mPickMaskWidth;
    U16 mPickMaskHeight;
    S8 mUseMipMaps;
    bool mHasExplicitFormat; // If false (default), GL format is f(mComponents)
    S8   mFormatComponents = -1; // the component count the format was worked out for, or -1
    void resolveComponentFormat(S8 components);
    bool mAutoGenMips = false;

    bool mIsMask;
    U8   mMaxAlpha;
    U8   mMinAlpha;
    bool mNeedsAlphaAndPickMask;
    LLAtomicU32 mAlphaAnalysisSerial; // for request tracking.
    S8   mAlphaStride ;
    S8   mAlphaOffset ;

    // The pick mask and alpha analysis of a texture that has not published yet.
    //
    // getMask and getIsAlphaMask answer for the texture mTexName names, and the main thread
    // asks them every frame (hover picking, the alpha pool). An off-thread upload building
    // the mask in place would free the buffer getMask was reading, so it builds here
    // instead and syncTexName swaps it in with the texture.
    struct PendingAlpha
    {
        U8*  mPickMask       = nullptr;
        U16  mPickMaskWidth  = 0;
        U16  mPickMaskHeight = 0;
        bool mHasPickMask    = false; // a result is waiting, possibly "no pick mask"
        AlphaAnalysis mAnalysis;
        bool mHasAnalysis    = false;
    };
    PendingAlpha mPendingAlpha;

    // Off-thread native uploads handed to the main thread and not yet published.
    using UploadCompletion = std::shared_ptr<std::atomic<U8>>;
    void queuePublish(UploadCompletion metal_completion = {});
    void finishPublish(bool metal_upload_succeeded = true);
    U32 mPublishesPending = 0;
    bool mPublishesSucceeded = true;
    std::vector<std::function<void(bool)>> mOnPublished;

    // Geometry of the currently published texture, captured when an off-thread
    // upload begins overwriting the members with the *next* texture's geometry.
    //
    // createMetalTexture runs on the LLImageGL thread and writes
    // mWidth/mHeight/mComponents (via setSize) and mCurrentDiscardLevel before
    // syncTexName publishes the new native generation on the main thread. Without this,
    // the image advertises the next generation's dimensions while consumers still sample
    // the old one. Only consulted while mUploadInFlight; the members are the truth otherwise.
    // Both are written on the main thread only; see beginUpload.
    struct PublishedGeom
    {
        S32 mWidth = 0;
        S32 mHeight = 0;
        S8  mComponents = 0;
        S8  mCurrentDiscardLevel = -1;
        S8  mMaxDiscardLevel = 0;
        // The format, which createMetalTexture works out again from mComponents for every
        // upload of an image without an explicit one.
        LLGLenum mFormatPrimary = 0;
        LLGLenum mDeprecatedSourceFormat = 0;
        LLGLint  mFormatInternal = -1;
        LLGLenum mFormatType = GL_UNSIGNED_BYTE;
        S8 mFormatComponents = -1;
        S8 mAlphaStride = 0;
        S8 mAlphaOffset = 0;
        bool mFormatSwapBytes = false;
        bool mHasExplicitFormat = false;
        bool mNeedsAlphaAndPickMask = true;
        bool mHasMipMaps = false;
        S32 mMipLevels = 0;
        S32 mCategory = -1;
        bool mGLTextureCreated = false;
        S64Bytes mTextureMemory = S64Bytes(0);
    };
    PublishedGeom mPublished;
    bool     mUploadInFlight = false;

    // Live geometry for the upload path. createMetalTexture runs while mUploadInFlight is
    // set and describes the texture being built. The getters above keep answering for the
    // published generation, so upload sizing must read these live members directly.
    S32 liveWidth(S32 discard_level) const
    {
        return llmax(mWidth >> llmax(discard_level, 0), 1);
    }
    S32 liveHeight(S32 discard_level) const
    {
        return llmax(mHeight >> llmax(discard_level, 0), 1);
    }

    // Written by the upload on LLImageGLThread and by the main thread that asks for the texture.
    std::atomic<bool> mGLTextureCreated;
    // getContentStamp, taken again by touchContents.
    std::atomic<U64> mContentStamp{ 0 };
    void touchContents();
    LLGLuint mTexName;
#if LL_DARWIN
    std::unique_ptr<ALMetalTexture> mMetalTexture;
    std::unique_ptr<ALMetalTexture> mPendingMetalTexture;
    U64 mAccountedMetalTextureBytes = 0;
#endif
    U16      mWidth;
    U16      mHeight;
    S8       mCurrentDiscardLevel;


protected:
    LLGLenum mTarget;       // Normally GL_TEXTURE2D, sometimes something else (ex. cube maps)
    ALTextureSlot::eTextureType mBindTarget;    // Normally TT_TEXTURE, sometimes something else (ex. cube maps)
    bool mHasMipMaps;
    // Number of mip levels the texture holds -- a COUNT, not a highest-index. 1 means
    // level 0 only; 0 means no texture yet. Matches LLRenderTarget::mMipLevels, and is
    // what glTexStorage2D takes. Computed once up front by calcMipLevelCount rather than
    // accumulated during upload, because allocation has to know it before any level is
    // written.
    S32 mMipLevels;

    LLGLboolean mIsResident;

    S8 mComponents;
    S8 mMaxDiscardLevel;

    LLGLint  mFormatInternal; // = GL internalformat
    LLGLenum mFormatPrimary;  // = GL format (pixel data format)
    LLGLenum mFormatType;
    bool     mFormatSwapBytes; // source byte-order metadata

    // The original (deprecated) source format the caller asked for, when
    // resolveDeprecatedFormat() rewrote it. Native descriptors apply the
    // corresponding channel swizzle. 0 = no rewrite happened.
    LLGLenum mDeprecatedSourceFormat;

    // STATICS
public:
    static boost::unordered_set<LLImageGL*> sImageList;
    static S32 sCount;
    static U32 sFrameCount;
    static F64 sLastFrameTime;

    // Global memory statistics
    static U32 sBindCount;                  // Tracks number of texture binds for current frame
    static U32 sUniqueCount;                // Tracks number of unique texture binds for current frame
    static LLImageGL* sDefaultGLTexture ;
    static bool sAutomatedTest;
#if DEBUG_MISS
    bool mMissed; // Missed on last bind?
    bool getMissed() const { return mMissed; };
#else
    bool getMissed() const { return false; };
#endif

public:
    static void initClass(LLWindow* window, S32 num_catagories, bool skip_analyze_alpha = false, bool thread_texture_loads = false, bool thread_media_updates = false);
    static void cleanupClass() ;
#if LL_DARWIN
    static void setMetalDevice(ALMetalDevice* device);
    static ALMetalDevice* getMetalDevice() noexcept;

    // Select the production texture owner independently of the legacy global
    // "renderer disabled" state. The native viewer keeps rendering enabled
    // while all ordinary image producers publish Metal-only storage.
    static void setMetalTextureAuthority(bool enabled) noexcept;
    static bool isMetalTextureAuthority() noexcept;

    struct MetalMipLevel
    {
        const U8* mData = nullptr;
        U32 mWidth = 0;
        U32 mHeight = 0;
    };

    // Create and populate a native-only 2D image. This establishes complete
    // geometry/format/accounting metadata, generates a native mip chain when this image
    // was constructed with usemipmaps, and leaves mTexName at zero.
    bool createMetalTexture(S32 discard_level, const LLImageRaw* image,
                            S32 category = sMaxCategories - 1);
    // As above, for typed source data whose byte size is described by the
    // image's explicit primary/type format rather than LLImageRaw's one byte
    // per component. The constructor/setSize and setExplicitFormat establish
    // the allocation geometry before this call.
    bool createMetalTexture(S32 discard_level, const U8* data,
                            S32 category = sMaxCategories - 1);

    // Create a native-only texture from a complete caller-authored mip pyramid. Unlike
    // the single-image overload this preserves every supplied level verbatim instead of
    // asking Metal to derive lower levels. The image geometry/format must already be
    // established, and the span must contain the exact full pyramid down to 1x1.
    bool createMetalTexture(S32 discard_level,
                            std::span<const MetalMipLevel> mip_levels,
                            S32 category = sMaxCategories - 1);

    // Upload one complete, non-mipmapped source image into the top-left of native storage
    // without creating or modifying a GL texture. The source may be smaller than the
    // allocation (media plugins expose an unpadded frame inside a power-of-two texture).
    // On the image worker this builds mPendingMetalTexture; call syncMetalToMainThread()
    // after success so it publishes only after the shared Metal batch commits. On the
    // main thread the committed write updates the live mirror.
    bool uploadMetalImage(const LLImageRaw* image);

    // Main-thread native subimage contract: (x, y, width, height) selects both
    // the source rectangle in the
    // full data_width x data_height image and its destination in this image.
    // No GL name is required or modified.
    bool uploadMetalSubImage(const U8* data, S32 data_width, S32 data_height,
                             S32 x, S32 y, S32 width, S32 height);

    // Off the main thread: hand a successful uploadMetalImage() to the main thread. This
    // has no GL fence or texture name; the shared Metal batch's commit token is its ordered
    // publication boundary. Returns whether that handoff reached the main queue.
    bool syncMetalToMainThread();

    // Main-thread publication boundary for a complete staged native texture.
    // Unlike a media-frame mirror update, this makes Metal authoritative and
    // retires any transitional GL object only after the native upload commits.
    void publishMetalTextureAuthoritative();
#endif

private:
#if LL_DARWIN
    bool prepareMetalTexture(std::unique_ptr<ALMetalCommandBuffer>& command_owner,
                             ALMetalCommandBuffer*& command_buffer,
                             bool will_upload_pixels);
    // Process-wide texture budgeting counts a transitional GL+Metal image once.
    // These helpers account native storage only while it is authoritative.
    void updateMetalTextureAccounting() noexcept;
    void clearMetalTextureAccounting() noexcept;
    bool uploadMetalTextureRegion(ALMetalCommandBuffer& command_buffer,
                                  U32 mip_level,
                                  U32 x,
                                  U32 y,
                                  U32 width,
                                  U32 height,
                                  const U8* pixels,
                                  std::size_t source_bytes_per_row);
    bool generateMetalMipmaps(ALMetalCommandBuffer& command_buffer);
    bool commitMetalUpload(std::unique_ptr<ALMetalCommandBuffer>& command_owner,
                           ALMetalCommandBuffer*& command_buffer);
    bool preserveMetalScaleDown(U32 source_mip_level, U32 width, U32 height);
    ALMetalTexture* metalTextureForUpload() noexcept;
    std::size_t metalSourceBytesPerRow(U32 width) const;
#endif

    static S32 sMaxCategories;
    static bool sSkipAnalyzeAlpha;

//
//****************************************************************************************************
//The below for texture auditing use only
//****************************************************************************************************
private:
    S32 mCategory ;
public:
    void setCategory(S32 category) {mCategory = category;}
    S32  getCategory()const {return mCategory;}

    void setTexName(GLuint texName) { mTexName = texName; }

    // Publish texname in place of mTexName, deleting the old one, along with what the
    // upload that built it staged (the pick mask), then end the upload. Main thread.
    void syncTexName(LLGLuint texname);

    // Bracket an upload handed to the LLImageGL thread, on the MAIN thread: begin before
    // posting the work, and end in its completion callback whatever the worker did
    // (syncTexName has already ended it if the texture published). In between, the
    // geometry getters answer for the texture mTexName still names.
    void beginUpload();
    // The same, first working out here the format an image without an explicit one takes
    // from the components it is about to upload: see resolveComponentFormat.
    void beginUpload(S8 components);
    // Abort an upload that changed live build metadata but never published.
    // Restores the texture view captured by beginUpload(), then discards
    // staged native/alpha state.
    void cancelUpload();
    void endUpload();

    //for debug use: show texture size distribution
    //----------------------------------------
    static S32 sCurTexSizeBar ;
    static S32 sCurTexPickSize ;

    static void setCurTexSizebar(S32 index, bool set_pick_size = true) ;
    static void resetCurTexSizebar();

//****************************************************************************************************
//End of definitions for texture auditing use only
//****************************************************************************************************

};

class LLImageGLThread : public LLSimpleton<LLImageGLThread>, LL::ThreadPool
{
public:
    // follows gSavedSettings "RenderGLMultiThreadedTextures"
    static bool sEnabledTextures;
    // follows gSavedSettings "RenderGLMultiThreadedMedia"
    static bool sEnabledMedia;

    LLImageGLThread(LLWindow* window);
    // Joins the worker before its final native upload batch is flushed.
    ~LLImageGLThread() override;

    // Finish the uploads posted and stop the thread, as the destructor does.
    using LL::ThreadPool::close;

    // post a function to be executed on the LLImageGL background thread
    template <typename CALLABLE>
    bool post(CALLABLE&& func)
    {
        return getQueue().post(std::forward<CALLABLE>(func));
    }

    void run() override;
};

#endif // LL_LLIMAGEGL_H
