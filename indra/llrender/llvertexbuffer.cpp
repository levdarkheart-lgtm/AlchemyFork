/**
 * @file llvertexbuffer.cpp
 * @brief LLVertexBuffer implementation
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
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

#include "llvertexbuffer.h"

#if LL_DARWIN
#include "almetaldevice.h"
#include "almetalresource.h"
#endif
#include "llglheaders.h"
#include "llrender.h"
#include "llvector4a.h"
#include "llglslshader.h"
#include "llimagegl.h"
#include "llmemory.h"

#include <algorithm>
#include <atomic>
#include <utility>

//Next Highest Power Of Two
//helper function, returns first number > v that is a power of 2, or v if v is already a power of 2
U32 nhpo2(U32 v)
{
    U32 r = 1;
    while (r < v) {
        r *= 2;
    }
    return r;
}

// The last content stamp handed out (LLVertexBuffer::getContentStamp).
static std::atomic<U64> sContentStamps{ 0 };

#if LL_DARWIN
namespace
{
    using MetalBufferGeneration =
        std::pair<std::unique_ptr<ALMetalBuffer>, U32>;
    using MetalBufferGenerations = std::vector<MetalBufferGeneration>;

    // A vertex buffer may disappear after its native pointer was captured but
    // before that frame's command buffer is encoded or completed. Keep those
    // wrappers alive independently of LLVertexBuffer until the frame timeline
    // says every possible reader is finished.
    MetalBufferGenerations sRetiredMetalBuffers;

    void collect_retired_metal_buffers(bool drain = false)
    {
        if (drain)
        {
            sRetiredMetalBuffers.clear();
            return;
        }

        const ALFrameFences& frames = gGL.frameFences();
        sRetiredMetalBuffers.erase(
            std::remove_if(
                sRetiredMetalBuffers.begin(), sRetiredMetalBuffers.end(),
                [&frames](const MetalBufferGeneration& generation)
                {
                    return frames.isComplete(generation.second);
                }),
            sRetiredMetalBuffers.end());
    }

    void retire_metal_buffer(std::unique_ptr<ALMetalBuffer> buffer,
                             U32 last_read_frame)
    {
        if (buffer)
        {
            sRetiredMetalBuffers.emplace_back(
                std::move(buffer), last_read_frame);
        }
    }

    std::unique_ptr<ALMetalBuffer> take_completed_metal_buffer(
        MetalBufferGenerations& generations)
    {
        const ALFrameFences& frames = gGL.frameFences();
        std::unique_ptr<ALMetalBuffer> reusable;
        for (auto generation = generations.begin();
             generation != generations.end();)
        {
            if (frames.isComplete(generation->second))
            {
                if (!reusable)
                {
                    reusable = std::move(generation->first);
                }
                generation = generations.erase(generation);
            }
            else
            {
                ++generation;
            }
        }
        return reusable;
    }
}
#endif


// The original Apple allocator staged aligned CPU stores without caching GL
// VBOs. Native geometry retains that staging policy; explicit Metal buffers
// and their completion-retired generations are owned by LLVertexBuffer.
class NativeVertexStorage final
{
public:
    U64 getBytesAllocated() const { return mAllocated; }

    void allocate(U32 size, U8*& data)
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_VERTEX;
        llassert(data == nullptr);
        llassert(size >= 2);
        data = static_cast<U8*>(ll_aligned_malloc_16(size));
        if (data) mAllocated += size;
    }

    void release(U32 size, U8* data)
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_VERTEX;
        llassert(mAllocated >= size);
        ll_aligned_free_16(data);
        mAllocated -= size;
    }

private:
    U64 mAllocated = 0;
};

static NativeVertexStorage* sNativeStorage = nullptr;

LLVertexBufferData::LLVertexBufferData()
    : mVB(nullptr),
      mMode(0),
      mCount(0),
      mTexImageOwner(nullptr),
      mTexImage(nullptr),
      mTextureBound(false),
      mProjection(LLMatrix4a::identity()),
      mModelView(LLMatrix4a::identity()),
      mTexture0(LLMatrix4a::identity())
{
}

LLVertexBufferData::LLVertexBufferData(
    LLVertexBuffer* buffer, U8 mode, U32 count,
    LLImageGL* tex_image, bool texture_bound,
    const ALSamplerDesc& sampler_desc,
    const LLMatrix4a& model_view, const LLMatrix4a& projection,
    const LLMatrix4a& texture0)
    : mVB(buffer),
      mMode(mode),
      mCount(count),
      mTexImageOwner(static_cast<LLThreadSafeRefCount*>(tex_image)),
      mTexImage(tex_image),
      mTextureBound(texture_bound),
      mSamplerDesc(sampler_desc),
      mProjection(projection),
      mModelView(model_view),
      mTexture0(texture0)
{
}

void LLVertexBufferData::drawWithMatrix()
{
    if (!mVB)
    {
        llassert(false);
        // Not supposed to happen, check buffer generation
        return;
    }

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.pushMatrix();
    gGL.loadMatrix(mModelView);
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.pushMatrix();
    gGL.loadMatrix(mProjection);
    gGL.matrixMode(LLRender::MM_TEXTURE0);
    gGL.pushMatrix();
    gGL.loadMatrix(mTexture0);

    // Retained UI is Metal-authoritative on the product platform. The
    // renderer records the exact failure reason and withholds an incomplete
    // native transaction; there is no raw-name/GL replay behind it.
    gGL.drawMetalUIBuffer(mVB.get(), mMode, mCount, mTexImage, mSamplerDesc,
                          mTextureBound);

    gGL.popMatrix();
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.popMatrix();
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.popMatrix();
}

void LLVertexBufferData::draw()
{
    if (!mVB)
    {
        llassert(false);
        // Not supposed to happen, check buffer generation
        return;
    }

    // See drawWithMatrix(): a missing native mirror is a visible native
    // defect, never permission to issue a legacy draw.
    gGL.drawMetalUIBuffer(mVB.get(), mMode, mCount, mTexImage, mSamplerDesc,
                          mTextureBound);
}

//============================================================================

//static
U64 LLVertexBuffer::getBytesAllocated()
{
    return sNativeStorage ? sNativeStorage->getBytesAllocated() : 0;
}

//============================================================================
//
//static
U32 LLVertexBuffer::sVertexCount = 0;


//NOTE: each component must be AT LEAST 4 bytes in size to avoid a performance penalty on AMD hardware
const U32 LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_MAX] =
{
    sizeof(LLVector4), // TYPE_VERTEX,
    sizeof(LLVector4), // TYPE_NORMAL,
    sizeof(LLVector2), // TYPE_TEXCOORD0,
    sizeof(LLVector2), // TYPE_TEXCOORD1,
    sizeof(LLVector2), // TYPE_TEXCOORD2,
    sizeof(LLVector2), // TYPE_TEXCOORD3,
    sizeof(LLColor4U), // TYPE_COLOR,
    sizeof(LLColor4U), // TYPE_EMISSIVE, only alpha is used currently
    sizeof(LLVector4), // TYPE_TANGENT,
    sizeof(F32),       // TYPE_WEIGHT,
    sizeof(LLVector4), // TYPE_WEIGHT4,
    sizeof(LLVector4), // TYPE_CLOTHWEIGHT,
    sizeof(U64),       // TYPE_JOINT,
    sizeof(LLVector4), // TYPE_TEXTURE_INDEX (actually exists as position.w), no extra data, but stride is 16 bytes
};

static const std::string vb_type_name[] =
{
    "TYPE_VERTEX",
    "TYPE_NORMAL",
    "TYPE_TEXCOORD0",
    "TYPE_TEXCOORD1",
    "TYPE_TEXCOORD2",
    "TYPE_TEXCOORD3",
    "TYPE_COLOR",
    "TYPE_EMISSIVE",
    "TYPE_TANGENT",
    "TYPE_WEIGHT",
    "TYPE_WEIGHT4",
    "TYPE_CLOTHWEIGHT",
    "TYPE_JOINT",
    "TYPE_TEXTURE_INDEX",
    "TYPE_MAX",
    "TYPE_INDEX",
};

//static
void LLVertexBuffer::drawArrays(U32 mode, const std::vector<LLVector3>& pos)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_VERTEX;
    gGL.begin(mode);
    for (auto& v : pos)
    {
        gGL.vertex3fv(v.mV);
    }
    gGL.end();
    gGL.flush();
}

//static
void LLVertexBuffer::drawElements(U32 mode, const LLVector4a* pos, const LLVector2* tc, U32 num_indices, const U16* indicesp)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_VERTEX;
    llassert(LLGLSLShader::sCurBoundShaderPtr != NULL);

    STOP_GLERROR;

    gGL.syncMatrices();

    unbind();

    gGL.begin(mode);

    if (tc != nullptr)
    {
        for (U32 i = 0; i < num_indices; ++i)
        {
            U16 idx = indicesp[i];
            gGL.texCoord2fv(tc[idx].mV);
            gGL.vertex3fv(pos[idx].getF32ptr());
        }
    }
    else
    {
        for (U32 i = 0; i < num_indices; ++i)
        {
            U16 idx = indicesp[i];
            gGL.vertex3fv(pos[idx].getF32ptr());
        }
    }
    gGL.end();
    gGL.flush();
}

bool LLVertexBuffer::canAppendRange(const DrawRange& current,
                                    const LLVertexBuffer& next_buffer,
                                    const DrawRange& next) const noexcept
{
    if (this != &next_buffer) return false;
    const auto fits = [this](const DrawRange& range)
    {
        return range.vertex_count != 0 && range.index_count != 0 &&
            range.first_vertex < mNumVerts &&
            range.vertex_count <= mNumVerts - range.first_vertex &&
            range.first_index < mNumIndices &&
            range.index_count <= mNumIndices - range.first_index;
    };
    return fits(current) && fits(next) &&
        next.first_vertex >= current.first_vertex &&
        next.first_vertex - current.first_vertex == current.vertex_count &&
        next.first_index >= current.first_index &&
        next.first_index - current.first_index == current.index_count;
}

bool LLVertexBuffer::validateRange(U32 start, U32 end, U32 count, U32 indices_offset) const
{
    const bool vertices_fit = start <= end && end < mNumVerts;
    const bool indices_fit = indices_offset <= mNumIndices &&
        count <= mNumIndices - indices_offset;
    if (gDebugGL && !vertices_fit)
    {
        LL_ERRS() << "Bad vertex buffer draw range: [" << start << ", " << end
                  << "] vs " << mNumVerts << LL_ENDL;
    }
    if (gDebugGL && !indices_fit)
    {
        LL_ERRS() << "Bad index buffer draw range: offset " << indices_offset
                  << ", count " << count << " vs " << mNumIndices << LL_ENDL;
    }
    return vertices_fit && indices_fit;
}

namespace
{
std::string bufferLabel(const std::string& label, const char* role)
{
    return label.empty() ? std::string() : label + role;
}
}

void LLVertexBuffer::setLabel(const char* label)
{
    const char* value = label ? label : "";
    if (mLabel == value) return;
    mLabel = value;
#if LL_DARWIN
    if (mMetalBuffer) mMetalBuffer->setLabel(bufferLabel(mLabel, " vertex data"));
    if (mMetalIndices) mMetalIndices->setLabel(bufferLabel(mLabel, " index data"));
#endif
}

void LLVertexBuffer::clone(LLVertexBuffer& target) const
{
    target.mTypeMask = mTypeMask;
    target.mIndicesType = mIndicesType;
    target.mIndicesStride = mIndicesStride;
    if (target.getNumVerts() != getNumVerts() ||
        target.getNumIndices() != getNumIndices())
    {
        target.allocateBuffer(getNumVerts(), getNumIndices());
    }
}

void LLVertexBuffer::drawRange(U32 mode, U32 start, U32 end, U32 count, U32 indices_offset) const
{
    if (count == 0) return;
    // LLRender::end keeps triangle/line/point streams pending. Its flush
    // admits through drawBuffer directly, so this cannot recurse into us.
    gGL.flush();
    if (!validateRange(start, end, count, indices_offset))
    {
        gGL.invalidateMetalUITextureCapture("Vertex buffer indexed draw exceeds its allocation");
        LL_WARNS_ONCE("Render") << "Skipping native indexed draw outside its vertex/index allocation" << LL_ENDL;
        return;
    }
    // World draw pools already admitted their typed native batches. Only an
    // active UI owner consumes this generic draw, using its current bindings.
    gGL.drawBoundMetalUIBuffer(const_cast<LLVertexBuffer*>(this), mode,
                              count, true, indices_offset);
}

void LLVertexBuffer::drawRangeFast(U32 mode, U32 start, U32 end, U32 count, U32 indices_offset) const
{
    drawRange(mode, start, end, count, indices_offset);
}


void LLVertexBuffer::draw(U32 mode, U32 count, U32 indices_offset) const
{
    drawRange(mode, 0, mNumVerts ? mNumVerts-1 : 0, count, indices_offset);
}


void LLVertexBuffer::drawArrays(U32 mode, U32 first, U32 count) const
{
    if (count == 0) return;
    gGL.flush();
    if (first >= mNumVerts || count > mNumVerts - first)
    {
        gGL.invalidateMetalUITextureCapture("Vertex buffer array draw exceeds its allocation");
        LL_WARNS_ONCE("Render") << "Skipping native array draw outside its vertex allocation" << LL_ENDL;
        return;
    }
    gGL.drawBoundMetalUIBuffer(const_cast<LLVertexBuffer*>(this), mode,
                              count, false, first);
}

//static
void LLVertexBuffer::initClass(LLWindow* window)
{
    llassert(sNativeStorage == nullptr);
    sNativeStorage = new NativeVertexStorage();
    (void)window;
}

//static
void LLVertexBuffer::unbind()
{
    // Every native encoder binds explicit resources; no ambient buffer exists.
}

// list of mapped buffers
// NOTE: must not be LLPointer<LLVertexBuffer> to avoid breaking non-ref-counted LLVertexBuffer instances
static std::vector<LLVertexBuffer*> sMappedBuffers;

//static
void LLVertexBuffer::cleanupClass()
{
    unbind();

#if LL_DARWIN
    // Shutdown discards unpublished CPU stores. The queue is non-owning;
    // disarm its entries before the backend and pool disappear so neither a
    // later flush nor a surviving buffer's destructor can retry them there.
    for (LLVertexBuffer* buffer : sMappedBuffers)
    {
        buffer->mMapped = false;
    }
    sMappedBuffers.clear();
#endif

    delete sNativeStorage;
    sNativeStorage = nullptr;

#if LL_DARWIN
    // gGL.shutdown() drains the scheduler before LLVertexBuffer cleanup, so
    // no native command can still reference a globally retired generation.
    collect_retired_metal_buffers(true);
#endif
}

//----------------------------------------------------------------------------

LLVertexBuffer::LLVertexBuffer(U32 typemask)
:   LLRefCount(),
    mTypeMask(typemask)
{
    //zero out offsets
    for (U32 i = 0; i < TYPE_MAX; i++)
    {
        mOffsets[i] = 0;
    }
}

void LLVertexBuffer::touchContents()
{
    mContentStamp = sContentStamps.fetch_add(1, std::memory_order_relaxed) + 1;
}


#if LL_DARWIN
static ALMetalDevice* sMetalDevice = nullptr;
static std::atomic<bool> sMetalMirrorReported{false};
static std::atomic<bool> sMetalBufferAuthority{false};

void LLVertexBuffer::setMetalDevice(ALMetalDevice* device)
{
    sMetalDevice = device;
}

void LLVertexBuffer::setMetalBufferAuthority(bool enabled) noexcept
{
    sMetalBufferAuthority.store(enabled, std::memory_order_release);
}

bool LLVertexBuffer::isMetalBufferAuthority() noexcept
{
    return sMetalBufferAuthority.load(std::memory_order_acquire);
}

static std::unique_ptr<ALMetalBuffer> allocate_metal_buffer(U32 size, const char* label)
{
    if (!sMetalDevice || size == 0)
    {
        return nullptr;
    }

    ALMetalBufferDescriptor descriptor;
    descriptor.length = size;
    descriptor.storage = ALMetalStorageMode::shared;
    descriptor.cpu_cache = ALMetalCpuCacheMode::write_combined;
    descriptor.label = label;
    std::string error;
    auto buffer = ALMetalBuffer::create(*sMetalDevice, descriptor, &error);
    if (!buffer)
    {
        LL_WARNS("Metal") << "Could not allocate " << label << ": " << error << LL_ENDL;
    }
    else
    {
        bool expected = false;
        if (sMetalMirrorReported.compare_exchange_strong(expected, true, std::memory_order_relaxed))
        {
            LL_INFOS("Metal") << "Production LLVertexBuffer Metal stores are active; first allocation "
                               << label << " is " << size << " bytes" << LL_ENDL;
        }
    }
    return buffer;
}

static bool upload_metal_buffer(
    std::unique_ptr<ALMetalBuffer>& current,
    MetalBufferGenerations& retired,
    bool& initialized,
    const void* bytes,
    U32 size,
    const char* label,
    std::string& error)
{
    if (!bytes || size == 0)
    {
        error = "Cannot upload an empty native vertex-buffer store";
        return false;
    }

    // The first upload cannot race a reader: no complete native generation
    // has been published yet. Every later upload gets another store, just as
    // the original Apple glBufferData path orphaned the name's old store.
    if (!initialized)
    {
        if (!current)
        {
            current = allocate_metal_buffer(size, label);
        }
        if (!current || !current->write(0, bytes, size, &error))
        {
            return false;
        }
        initialized = true;
        return true;
    }

    std::unique_ptr<ALMetalBuffer> next =
        take_completed_metal_buffer(retired);
    if (!next)
    {
        next = allocate_metal_buffer(size, label);
    }
    if (!next || !next->write(0, bytes, size, &error))
    {
        return false;
    }
    // A completed generation may have been labelled for an earlier pass.
    // Preserve the current logical diagnostic when it is reused.
    next->setLabel(label);

    retired.emplace_back(
        std::move(current), gGL.frameFences().frame());
    current = std::move(next);
    return true;
}
#endif

//static
void LLVertexBuffer::flushBuffers()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_VERTEX;
#if LL_DARWIN
    collect_retired_metal_buffers();
#endif
    // must only be called from main thread
    std::size_t retry_count = 0;
    for (LLVertexBuffer* buffer : sMappedBuffers)
    {
        buffer->_unmapBuffer();
#if LL_DARWIN
        // A failed native upload leaves its CPU store dirty. Keep the entry
        // queued so the next flush retries without requiring another write.
        // Destruction disarms mMapped before flushing: never requeue a dying
        // buffer even when its last publication failed.
        if (buffer->mMapped &&
            (buffer->mVerticesWritten || buffer->mIndicesWritten))
        {
            sMappedBuffers[retry_count++] = buffer;
            continue;
        }
#endif
        buffer->mMapped = false;
    }

    sMappedBuffers.resize(retry_count);
}

//static
U32 LLVertexBuffer::calcOffsets(const U32& typemask, U32* offsets, U32 num_vertices)
{
    U32 offset = 0;
    for (U32 i=0; i<TYPE_TEXTURE_INDEX; i++)
    {
        U32 mask = 1<<i;
        if (typemask & mask)
        {
            if (offsets && LLVertexBuffer::sTypeSize[i])
            {
                offsets[i] = offset;
                offset += LLVertexBuffer::sTypeSize[i]*num_vertices;
                offset = (offset + 0xF) & ~0xF;
            }
        }
    }

    offsets[TYPE_TEXTURE_INDEX] = offsets[TYPE_VERTEX] + 12;

    return offset;
}

//static
U32 LLVertexBuffer::calcVertexSize(const U32& typemask)
{
    U32 size = 0;
    for (U32 i = 0; i < TYPE_TEXTURE_INDEX; i++)
    {
        U32 mask = 1<<i;
        if (typemask & mask)
        {
            size += LLVertexBuffer::sTypeSize[i];
        }
    }

    return size;
}

// protected, use unref()
//virtual
LLVertexBuffer::~LLVertexBuffer()
{
    if (mMapped)
    { // is on the mapped buffer list but doesn't need to be flushed
        mMapped = false;
        unmapBuffer();
    }

    destroyGLBuffer();
    destroyGLIndices();

    if (mMappedData)
    {
        LL_ERRS() << "Failed to clear vertex buffer's vertices" << LL_ENDL;
    }
    if (mMappedIndexData)
    {
        LL_ERRS() << "Failed to clear vertex buffer's indices" << LL_ENDL;
    }
};

//----------------------------------------------------------------------------

void LLVertexBuffer::genBuffer(U32 size)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_VERTEX;
    llassert(sNativeStorage);

    if (sNativeStorage)
    {
        llassert(mSize == 0);
        llassert(mMappedData == nullptr);

        mSize = size;
        sNativeStorage->allocate(mSize, mMappedData);
    }
}

void LLVertexBuffer::genIndices(U32 size)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_VERTEX;
    llassert(sNativeStorage);

    if (sNativeStorage)
    {
        llassert(mIndicesSize == 0);
        llassert(mMappedIndexData == nullptr);
        mIndicesSize = size;
        sNativeStorage->allocate(mIndicesSize, mMappedIndexData);
    }
}

bool LLVertexBuffer::createGLBuffer(U32 size)
{
    if (mMappedData)
    {
        destroyGLBuffer();
    }

    if (size == 0)
    {
        return true;
    }

    bool success = true;

    genBuffer(size);

#if LL_DARWIN
    mMetalBuffer = allocate_metal_buffer(size, bufferLabel(mLabel, " vertex data").c_str());
    mMetalBufferInitialized = false;
    if (sMetalDevice && !mMetalBuffer)
    {
        success = false;
    }
#endif

    if (!mMappedData)
    {
        success = false;
    }
    return success;
}

bool LLVertexBuffer::createGLIndices(U32 size)
{
    if (mMappedIndexData)
    {
        destroyGLIndices();
    }

    if (size == 0)
    {
        return true;
    }

    bool success = true;

    genIndices(size);

#if LL_DARWIN
    mMetalIndices = allocate_metal_buffer(size, bufferLabel(mLabel, " index data").c_str());
    mMetalIndicesInitialized = false;
    if (sMetalDevice && !mMetalIndices)
    {
        success = false;
    }
#endif

    if (!mMappedIndexData)
    {
        success = false;
    }
    return success;
}

void LLVertexBuffer::destroyGLBuffer()
{
#if LL_DARWIN
    collect_retired_metal_buffers();
    for (MetalBufferGeneration& generation : mRetiredMetalBuffers)
    {
        retire_metal_buffer(
            std::move(generation.first), generation.second);
    }
    mRetiredMetalBuffers.clear();
    if (mMetalBufferInitialized)
    {
        retire_metal_buffer(
            std::move(mMetalBuffer), gGL.frameFences().frame());
    }
    else
    {
        mMetalBuffer.reset();
    }
    mMetalBufferInitialized = false;
#endif
    if (mMappedData)
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_VERTEX;
        if (sNativeStorage)
        {
            sNativeStorage->release(mSize, mMappedData);
        }

        mSize = 0;
        mMappedData = nullptr;
    }
}

void LLVertexBuffer::destroyGLIndices()
{
#if LL_DARWIN
    collect_retired_metal_buffers();
    for (MetalBufferGeneration& generation : mRetiredMetalIndices)
    {
        retire_metal_buffer(
            std::move(generation.first), generation.second);
    }
    mRetiredMetalIndices.clear();
    if (mMetalIndicesInitialized)
    {
        retire_metal_buffer(
            std::move(mMetalIndices), gGL.frameFences().frame());
    }
    else
    {
        mMetalIndices.reset();
    }
    mMetalIndicesInitialized = false;
#endif
    if (mMappedIndexData)
    {
        LL_PROFILE_ZONE_SCOPED_CATEGORY_VERTEX;
        if (sNativeStorage)
        {
            sNativeStorage->release(mIndicesSize, mMappedIndexData);
        }

        mIndicesSize = 0;
        mMappedIndexData = nullptr;
    }
}

bool LLVertexBuffer::updateNumVerts(U32 nverts)
{
    llassert(nverts >= 0);

    bool success = true;

    U32 needed_size = calcOffsets(mTypeMask, mOffsets, nverts);

    if (needed_size != mSize)
    {
        success &= createGLBuffer(needed_size);
    }

    llassert(mSize == needed_size);
    mNumVerts = nverts;
    return success;
}

bool LLVertexBuffer::updateNumIndices(U32 nindices)
{
    llassert(nindices >= 0);

    bool success = true;

    U32 needed_size = sizeof(U16) * nindices;

    if (needed_size != mIndicesSize)
    {
        success &= createGLIndices(needed_size);
    }

    llassert(mIndicesSize == needed_size);
    mNumIndices = nindices;
    return success;
}

bool LLVertexBuffer::allocateBuffer(U32 nverts, U32 nindices)
{
    if (nverts < 0 || nindices < 0)
    {
        LL_ERRS() << "Bad vertex buffer allocation: " << nverts << " : " << nindices << LL_ENDL;
    }

    bool success = true;

    success &= updateNumVerts(nverts);
    success &= updateNumIndices(nindices);
    touchContents();

    return success;
}

//----------------------------------------------------------------------------

// Mapping edits the CPU shadow and records which complete native store must
// be republished. Partial setters below modify that same shadow; no mapped
// GL range list or driver upload remains.
U8* LLVertexBuffer::mapVertexBuffer(LLVertexBuffer::AttributeType type, U32 index, S32)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_VERTEX;
    _mapBuffer();
    touchContents();
    mVerticesWritten = true;
    return mMappedData + mOffsets[type] + sTypeSize[type] * index;
}

U8* LLVertexBuffer::mapIndexBuffer(U32 index, S32)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_VERTEX;
    _mapBuffer();
    touchContents();
    mIndicesWritten = true;
    return mMappedIndexData + sizeof(U16) * index;
}

// Inclusive byte range, as used by the retained whole/partial data setters.
void LLVertexBuffer::flush_vbo(GLenum target, U32 start, U32 end, void* data, U8* dst)
{
    if (end + 1 == start) return;
    touchContents();
    _mapBuffer();
    if (target == GL_ELEMENT_ARRAY_BUFFER)
        mIndicesWritten = true;
    else
        mVerticesWritten = true;
    LL_PROFILE_ZONE_NAMED_CATEGORY_VERTEX("vb memcpy");
    memcpy(dst + start, data, end - start + 1);
}

void LLVertexBuffer::unmapBuffer()
{
    flushBuffers();
}

void LLVertexBuffer::_mapBuffer()
{
    if (!mMapped)
    {
        mMapped = true;
        sMappedBuffers.push_back(this);
    }
}

void LLVertexBuffer::_unmapBuffer()
{
    if (!mMapped) return;
#if LL_DARWIN
    // Preserve the original Apple whole-store orphaning contract: only a
    // written or never-initialized store is published. An index-only edit
    // must leave vertex bytes/generation intact, and conversely. Failed
    // publication retains its dirty flag for flushBuffers to retry.
    const bool upload_vertices = mVerticesWritten || !mMetalBufferInitialized;
    bool vertices_uploaded = true;
    if (mMappedData && upload_vertices)
    {
        if (sMetalDevice)
        {
            std::string error;
            vertices_uploaded = upload_metal_buffer(
                mMetalBuffer, mRetiredMetalBuffers, mMetalBufferInitialized,
                mMappedData, mSize, bufferLabel(mLabel, " vertex data").c_str(), error);
            if (!vertices_uploaded)
                LL_WARNS("Metal") << "Could not upload vertex data: " << error << LL_ENDL;
        }
        else
        {
            vertices_uploaded = false;
            LL_WARNS("Metal") << "Could not upload vertex data without a Metal device" << LL_ENDL;
        }
    }
    mVerticesWritten = upload_vertices && !vertices_uploaded;

    const bool upload_indices = mIndicesWritten || !mMetalIndicesInitialized;
    bool indices_uploaded = true;
    if (mMappedIndexData && upload_indices)
    {
        if (sMetalDevice)
        {
            std::string error;
            indices_uploaded = upload_metal_buffer(
                mMetalIndices, mRetiredMetalIndices, mMetalIndicesInitialized,
                mMappedIndexData, mIndicesSize, bufferLabel(mLabel, " index data").c_str(), error);
            if (!indices_uploaded)
                LL_WARNS("Metal") << "Could not upload index data: " << error << LL_ENDL;
        }
        else
        {
            indices_uploaded = false;
            LL_WARNS("Metal") << "Could not upload index data without a Metal device" << LL_ENDL;
        }
    }
    mIndicesWritten = upload_indices && !indices_uploaded;
#endif
}

//----------------------------------------------------------------------------

template <class T,LLVertexBuffer::AttributeType type> struct VertexBufferStrider
{
    typedef LLStrider<T> strider_t;
    static bool get(LLVertexBuffer& vbo,
                    strider_t& strider,
                    S32 index, S32 count)
    {
        if constexpr (type == LLVertexBuffer::TYPE_INDEX)
        {
            U8* ptr = vbo.mapIndexBuffer(index, count);

            if (ptr == NULL)
            {
                LL_WARNS() << "mapIndexBuffer failed!" << LL_ENDL;
                return false;
            }

            strider = (T*)ptr;
            strider.setStride(0);
            return true;
        }
        else if (vbo.hasDataType(type))
        {
            U32 stride = LLVertexBuffer::sTypeSize[type];

            U8* ptr = vbo.mapVertexBuffer(type, index, count);

            if (ptr == NULL)
            {
                LL_WARNS() << "mapVertexBuffer failed!" << LL_ENDL;
                return false;
            }

            strider = (T*)ptr;
            strider.setStride(stride);
            return true;
        }
        else
        {
            LL_ERRS() << "VertexBufferStrider could not find valid vertex data." << LL_ENDL;
        }
        return false;
    }
};

bool LLVertexBuffer::getVertexStrider(LLStrider<LLVector3>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLVector3,TYPE_VERTEX>::get(*this, strider, index, count);
}
bool LLVertexBuffer::getVertexStrider(LLStrider<LLVector4a>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLVector4a,TYPE_VERTEX>::get(*this, strider, index, count);
}
bool LLVertexBuffer::getIndexStrider(LLStrider<U16>& strider, U32 index, S32 count)
{
    llassert(mIndicesStride == 2); // cannot access 32-bit indices with U16 strider
    llassert(mIndicesType == GL_UNSIGNED_SHORT);
    return VertexBufferStrider<U16,TYPE_INDEX>::get(*this, strider, index, count);
}
bool LLVertexBuffer::getTexCoord0Strider(LLStrider<LLVector2>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLVector2,TYPE_TEXCOORD0>::get(*this, strider, index, count);
}
bool LLVertexBuffer::getTexCoord1Strider(LLStrider<LLVector2>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLVector2,TYPE_TEXCOORD1>::get(*this, strider, index, count);
}
bool LLVertexBuffer::getTexCoord2Strider(LLStrider<LLVector2>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLVector2,TYPE_TEXCOORD2>::get(*this, strider, index, count);
}
bool LLVertexBuffer::getNormalStrider(LLStrider<LLVector3>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLVector3,TYPE_NORMAL>::get(*this, strider, index, count);
}
bool LLVertexBuffer::getNormalStrider(LLStrider<LLVector4a>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLVector4a, TYPE_NORMAL>::get(*this, strider, index, count);
}
bool LLVertexBuffer::getTangentStrider(LLStrider<LLVector3>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLVector3,TYPE_TANGENT>::get(*this, strider, index, count);
}
bool LLVertexBuffer::getTangentStrider(LLStrider<LLVector4a>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLVector4a,TYPE_TANGENT>::get(*this, strider, index, count);
}
bool LLVertexBuffer::getColorStrider(LLStrider<LLColor4U>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLColor4U,TYPE_COLOR>::get(*this, strider, index, count);
}
bool LLVertexBuffer::getEmissiveStrider(LLStrider<LLColor4U>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLColor4U,TYPE_EMISSIVE>::get(*this, strider, index, count);
}
bool LLVertexBuffer::getWeightStrider(LLStrider<F32>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<F32,TYPE_WEIGHT>::get(*this, strider, index, count);
}

bool LLVertexBuffer::getWeight4Strider(LLStrider<LLVector4>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLVector4,TYPE_WEIGHT4>::get(*this, strider, index, count);
}

bool LLVertexBuffer::getClothWeightStrider(LLStrider<LLVector4>& strider, U32 index, S32 count)
{
    return VertexBufferStrider<LLVector4,TYPE_CLOTHWEIGHT>::get(*this, strider, index, count);
}

//----------------------------------------------------------------------------


// Publish pending native stores. Shader layout and resource bindings belong
// to the typed native draw/capture that consumes this buffer.
void LLVertexBuffer::setBuffer()
{
    if (mMapped)
    {
        LL_WARNS_ONCE() << "Missing call to unmapBuffer or flushBuffers" << LL_ENDL;
        _unmapBuffer();
    }
}

#if LL_DARWIN
bool LLVertexBuffer::prepareMetalBuffers(bool require_indices)
{
    setBuffer();
    return hasPublishedMetalVertexBuffer() &&
        (!require_indices || hasPublishedMetalIndexBuffer());
}
#endif

void LLVertexBuffer::setPositionData(const LLVector4a* data)
{
    flush_vbo(GL_ARRAY_BUFFER, 0, sizeof(LLVector4a) * getNumVerts()-1, (U8*) data, mMappedData);
}

void LLVertexBuffer::setTexCoord0Data(const LLVector2* data)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_TEXCOORD0], mOffsets[TYPE_TEXCOORD0] + sTypeSize[TYPE_TEXCOORD0] * getNumVerts() - 1, (U8*)data, mMappedData);
}

void LLVertexBuffer::setTexCoord1Data(const LLVector2* data)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_TEXCOORD1], mOffsets[TYPE_TEXCOORD1] + sTypeSize[TYPE_TEXCOORD1] * getNumVerts() - 1, (U8*)data, mMappedData);
}

void LLVertexBuffer::setColorData(const LLColor4U* data)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_COLOR], mOffsets[TYPE_COLOR] + sTypeSize[TYPE_COLOR] * getNumVerts() - 1, (U8*) data, mMappedData);
}

void LLVertexBuffer::setNormalData(const LLVector4a* data)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_NORMAL], mOffsets[TYPE_NORMAL] + sTypeSize[TYPE_NORMAL] * getNumVerts() - 1, (U8*) data, mMappedData);
}

void LLVertexBuffer::setTangentData(const LLVector4a* data)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_TANGENT], mOffsets[TYPE_TANGENT] + sTypeSize[TYPE_TANGENT] * getNumVerts() - 1, (U8*) data, mMappedData);
}

void LLVertexBuffer::setWeight4Data(const LLVector4a* data)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_WEIGHT4], mOffsets[TYPE_WEIGHT4] + sTypeSize[TYPE_WEIGHT4] * getNumVerts() - 1, (U8*) data, mMappedData);
}

void LLVertexBuffer::setJointData(const U64* data)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_JOINT], mOffsets[TYPE_JOINT] + sTypeSize[TYPE_JOINT] * getNumVerts() - 1, (U8*) data, mMappedData);
}

void LLVertexBuffer::setIndexData(const U16* data)
{
    flush_vbo(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(U16) * getNumIndices() - 1, (U8*) data, mMappedIndexData);
}

void LLVertexBuffer::setIndexData(const U32* data)
{
    if (mIndicesType != GL_UNSIGNED_INT)
    { // HACK -- vertex buffers are initialized as 16-bit indices, but can be switched to 32-bit indices
        mIndicesType = GL_UNSIGNED_INT;
        mIndicesStride = 4;
        mNumIndices /= 2;
    }
    flush_vbo(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(U32) * getNumIndices() - 1, (U8*)data, mMappedIndexData);
}

void LLVertexBuffer::setPositionData(const LLVector4a* data, U32 offset, U32 count)
{
    flush_vbo(GL_ARRAY_BUFFER, offset * sizeof(LLVector4a), (offset + count) * sizeof(LLVector4a) - 1, (U8*)data, mMappedData);
}

void LLVertexBuffer::setNormalData(const LLVector4a* data, U32 offset, U32 count)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_NORMAL] + offset * sTypeSize[TYPE_NORMAL], mOffsets[TYPE_NORMAL] + (offset + count) * sTypeSize[TYPE_NORMAL] - 1, (U8*)data, mMappedData);
}

void LLVertexBuffer::setTexCoord0Data(const LLVector2* data, U32 offset, U32 count)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_TEXCOORD0] + offset * sTypeSize[TYPE_TEXCOORD0], mOffsets[TYPE_TEXCOORD0] + (offset + count) * sTypeSize[TYPE_TEXCOORD0] - 1, (U8*)data, mMappedData);
}

void LLVertexBuffer::setTexCoord1Data(const LLVector2* data, U32 offset, U32 count)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_TEXCOORD1] + offset * sTypeSize[TYPE_TEXCOORD1], mOffsets[TYPE_TEXCOORD1] + (offset + count) * sTypeSize[TYPE_TEXCOORD1] - 1, (U8*)data, mMappedData);
}

void LLVertexBuffer::setColorData(const LLColor4U* data, U32 offset, U32 count)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_COLOR] + offset * sTypeSize[TYPE_COLOR], mOffsets[TYPE_COLOR] + (offset + count) * sTypeSize[TYPE_COLOR] - 1, (U8*)data, mMappedData);
}

void LLVertexBuffer::setTangentData(const LLVector4a* data, U32 offset, U32 count)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_TANGENT] + offset * sTypeSize[TYPE_TANGENT], mOffsets[TYPE_TANGENT] + (offset + count) * sTypeSize[TYPE_TANGENT] - 1, (U8*)data, mMappedData);
}

void LLVertexBuffer::setWeight4Data(const LLVector4a* data, U32 offset, U32 count)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_WEIGHT4] + offset * sTypeSize[TYPE_WEIGHT4], mOffsets[TYPE_WEIGHT4] + (offset + count) * sTypeSize[TYPE_WEIGHT4] - 1, (U8*)data, mMappedData);
}

void LLVertexBuffer::setJointData(const U64* data, U32 offset, U32 count)
{
    flush_vbo(GL_ARRAY_BUFFER, mOffsets[TYPE_JOINT] + offset * sTypeSize[TYPE_JOINT], mOffsets[TYPE_JOINT] + (offset + count) * sTypeSize[TYPE_JOINT] - 1, (U8*)data, mMappedData);
}

void LLVertexBuffer::setIndexData(const U16* data, U32 offset, U32 count)
{
    flush_vbo(GL_ELEMENT_ARRAY_BUFFER, offset * sizeof(U16), (offset + count) * sizeof(U16) - 1, (U8*)data, mMappedIndexData);
}

void LLVertexBuffer::setIndexData(const U32* data, U32 offset, U32 count)
{
    if (mIndicesType != GL_UNSIGNED_INT)
    { // HACK -- vertex buffers are initialized as 16-bit indices, but can be switched to 32-bit indices
        mIndicesType = GL_UNSIGNED_INT;
        mIndicesStride = 4;
        mNumIndices /= 2;
    }
    flush_vbo(GL_ELEMENT_ARRAY_BUFFER, offset * sizeof(U32), (offset + count) * sizeof(U32) - 1, (U8*)data, mMappedIndexData);
}




