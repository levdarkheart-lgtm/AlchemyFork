/**
 * @file llrender.h
 * @brief LLRender definition
 *
 *  This class acts as a wrapper for OpenGL calls.
 *  The goal of this class is to minimize the number of api calls due to legacy rendering
 *  code, to define an interface for a multiple rendering API abstraction of the UI
 *  rendering, and to abstract out direct rendering calls in a way that is cleaner and easier to maintain.
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

#ifndef LL_LLGLRENDER_H
#define LL_LLGLRENDER_H

//#include "linden_common.h"

#include "v2math.h"
#include "v3math.h"
#include "v4coloru.h"
#include "v4math.h"
#include "llstrider.h"
#include "llpointer.h"
#include "llglheaders.h"
#include "llmatrix4a.h"
#include "alsamplerstate.h"  // mSamplerCache -- this context's sampler objects
#include "algpubuffer.h"     // AL_NUM_STORAGE_BINDINGS -- the storage binding points this context keeps
#include "alframefences.h"
#include "alimageslot.h"
#include "alquerypool.h"
#include "altextureslot.h"
#include "aluniformbuffer.h"

#include <boost/unordered_map.hpp>

#include <array>
#include <chrono>
#include <deque>
#include <list>
#include <memory>
#include <string>
#include <vector>

class LLVertexBuffer;
class LLCubeMap;
class LLImageGL;
class LLGLSLShader;
class LLRenderTarget;
class LLTexture;
class LLVertexBufferData;
#if LL_DARWIN
class ALMetalDevice;
class ALMetalFrameScheduler;
class ALMetalSurface;
class ALMetalTexture;
class ALMetalTextureReadbackRing;
class ALMetalUIRenderer;
struct ALMetalSelectionIDScene;
struct ALMetalSelectionContourScene;
struct ALMetalHUDIndexedBatch;
enum class ALMetalPixelFormat;
#endif

namespace ll_test { struct RenderProbe; }

#define LL_MATRIX_STACK_DEPTH 32

// AL_NUM_TEXTURE_SLOTS lives in altextureslot.h alongside the units it sizes.
constexpr U32 LL_NUM_LIGHT_UNITS = 8;

class LLLightState
{
public:
    LLLightState(S32 index = -1);

    void enable();
    void disable();
    void setDiffuse(const LLColor4& diffuse);
    void setDiffuseB(const LLColor4& diffuse);
    void setAmbient(const LLColor4& ambient);
    void setSpecular(const LLColor4& specular);
    void setPosition(const LLVector4& position);
    void setConstantAttenuation(const F32& atten);
    void setLinearAttenuation(const F32& atten);
    void setQuadraticAttenuation(const F32& atten);
    void setSpotExponent(const F32& exponent);
    void setSpotCutoff(const F32& cutoff);
    void setSpotDirection(const LLVector3& direction);
    void setSunPrimary(bool v);
    void setSize(F32 size);
    void setFalloff(F32 falloff);

protected:
    friend class LLRender;

    S32 mIndex;
    bool mEnabled;
    LLColor4 mDiffuse;
    LLColor4 mDiffuseB;
    bool     mSunIsPrimary;
    LLColor4 mAmbient;
    LLColor4 mSpecular;
    LLVector4 mPosition;
    LLVector3 mSpotDirection;

    F32 mConstantAtten;
    F32 mLinearAtten;
    F32 mQuadraticAtten;

    F32 mSpotExponent;
    F32 mSpotCutoff;
    F32 mSize = 0.f;
    F32 mFalloff = 0.f;
};

class LLRender
{
    friend class ALTextureSlot;
public:

    enum eTexIndex : U8
    {
        // Channels for material textures
        DIFFUSE_MAP            = 0,
        ALTERNATE_DIFFUSE_MAP  = 1,
        NORMAL_MAP             = 1,
        SPECULAR_MAP           = 2,
        // Channels for PBR textures
        BASECOLOR_MAP          = 3,
        METALLIC_ROUGHNESS_MAP = 4,
        GLTF_NORMAL_MAP        = 5,
        EMISSIVE_MAP           = 6,
        // Total number of channels
        NUM_TEXTURE_CHANNELS   = 7,
    };

    enum eVolumeTexIndex : U8
    {
        LIGHT_TEX = 0,
        SCULPT_TEX,
        NUM_VOLUME_TEXTURE_CHANNELS,
    };

    enum eGeomModes : U8
    {
        TRIANGLES = 0,
        TRIANGLE_STRIP,
        TRIANGLE_FAN,
        POINTS,
        LINES,
        LINE_STRIP,
        LINE_LOOP,
        PATCHES,    // tessellation input; vertices per patch set with glPatchParameteri
        NUM_MODES
    };

    enum eCompareFunc : U8
    {
        CF_NEVER = 0,
        CF_ALWAYS,
        CF_LESS,
        CF_LESS_EQUAL,
        CF_EQUAL,
        CF_NOT_EQUAL,
        CF_GREATER_EQUAL,
        CF_GREATER,
        CF_DEFAULT
    };

    enum eBlendType : U8
    {
        BT_ALPHA = 0,
        BT_ADD,
        BT_ADD_WITH_ALPHA,  // Additive blend modulated by the fragment's alpha.
        BT_MULT,
        BT_MULT_ALPHA,
        BT_MULT_X2,
        BT_REPLACE
    };

    // WARNING:  this MUST match the LL_PART_BF enum in LLPartData, so set values explicitly in case someone
    // decides to add more or reorder them
    enum eBlendFactor : U8
    {
        BF_ONE = 0,
        BF_ZERO = 1,
        BF_DEST_COLOR = 2,
        BF_SOURCE_COLOR = 3,
        BF_ONE_MINUS_DEST_COLOR = 4,
        BF_ONE_MINUS_SOURCE_COLOR = 5,
        BF_DEST_ALPHA = 6,
        BF_SOURCE_ALPHA = 7,
        BF_ONE_MINUS_DEST_ALPHA = 8,
        BF_ONE_MINUS_SOURCE_ALPHA = 9,
        BF_UNDEF
    };

    // One texture matrix, not four, and no MM_TEXTURE.
    //
    // MM_TEXTURE used to resolve to MM_TEXTURE0 + getCurrentTexUnitIndex(), making the active
    // texture unit an implicit argument that selected which stack a caller wrote. That was a
    // silent trap rather than a feature: no shader in the tree declares texture_matrix1/2/3
    // (they were dropped during the Slang port), so a resolution landing anywhere but stack 0
    // wrote a matrix nothing would ever read -- the texture transform simply vanished, with no
    // error. Call sites defended against it by issuing getTextureSlot(0)->activate() first, which
    // is why that call appeared next to matrixMode all over the draw paths.
    //
    // Measured before removing: 132 of 132 resolutions in a frame landed on unit 0, so the
    // dynamic form had no users at all, and the guarding activate() calls came to 0.8% of
    // issued glActiveTexture. So this is not a performance change -- it deletes a failure mode
    // that could only ever cost correctness.
    //
    // LLShaderMgr::TEXTURE_MATRIX1/2/3 are deliberately left in place: those enum values index
    // mReservedUniforms, and renumbering it would invalidate the on-disk shader reflection
    // cache. They are simply never referenced now.
    enum eMatrixMode : U8
    {
        MM_MODELVIEW = 0,
        MM_PROJECTION,
        MM_TEXTURE0,
        NUM_MATRIX_MODES
    };

    // Memory barrier bits, GL's own values, so a caller states exactly which later reads must see
    // earlier shader writes.
    enum eBarrier : U32
    {
        BARRIER_VERTEX_ATTRIB      = GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT,
        BARRIER_ELEMENT_ARRAY      = GL_ELEMENT_ARRAY_BARRIER_BIT,
        BARRIER_UNIFORM            = GL_UNIFORM_BARRIER_BIT,
        BARRIER_TEXTURE_FETCH      = GL_TEXTURE_FETCH_BARRIER_BIT,
        BARRIER_IMAGE              = GL_SHADER_IMAGE_ACCESS_BARRIER_BIT,
        BARRIER_COMMAND            = GL_COMMAND_BARRIER_BIT,
        BARRIER_PIXEL_BUFFER       = GL_PIXEL_BUFFER_BARRIER_BIT,
        BARRIER_TEXTURE_UPDATE     = GL_TEXTURE_UPDATE_BARRIER_BIT,
        BARRIER_BUFFER_UPDATE      = GL_BUFFER_UPDATE_BARRIER_BIT,
        BARRIER_FRAMEBUFFER        = GL_FRAMEBUFFER_BARRIER_BIT,
        BARRIER_TRANSFORM_FEEDBACK = GL_TRANSFORM_FEEDBACK_BARRIER_BIT,
        BARRIER_ATOMIC_COUNTER     = GL_ATOMIC_COUNTER_BARRIER_BIT,
        BARRIER_STORAGE            = GL_SHADER_STORAGE_BARRIER_BIT,
        BARRIER_CLIENT_MAPPED      = GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT,
        BARRIER_QUERY_BUFFER       = GL_QUERY_BUFFER_BARRIER_BIT,
        BARRIER_ALL                = GL_ALL_BARRIER_BITS,
    };

    LLRender();
    ~LLRender();
    bool init(bool needs_vertex_buffer);
#if LL_DARWIN
    // Initialize the native renderer's logical state and immediate buffer
    // without issuing an OpenGL call or preparing a fallback context.
    bool initMetal(bool needs_vertex_buffer);
#endif
    void initVertexBuffer();
    void resetVertexBuffer();
    void shutdown();

#if LL_DARWIN
    enum class EMetalUICaptureMode
    {
        // Retain the existing GL producer and mirror every supported batch.
        GL_MIRROR,
        // The Metal transaction is the sole producer. Unsupported work fails
        // the transaction closed and never falls through to a GL draw.
        NATIVE_AUTHORITATIVE
    };
    enum class EMetalUICaptureStorage
    {
        FORMAT_NATIVE,
        // Render and blend through a view-compatible UNORM attachment while
        // retaining the sRGB texture's storage descriptor. This matches GL
        // with GL_FRAMEBUFFER_SRGB disabled.
        LINEAR_STORAGE
    };
    enum class EMetalUICaptureEndState
    {
        INACTIVE,
        SOURCE_INCOMPLETE,
        CAPTURE_INCOMPLETE,
        EMPTY,
        COMMAND_SUBMISSION_FAILED,
        SURFACE_SUBMISSION_FAILED,
        COMPLETE
    };

    // Install the window-owned native device and bounded frame scheduler on
    // the main renderer. The renderer borrows both; the window outlives gGL's
    // explicit shutdown.
    void setMetalBackend(ALMetalDevice* device, ALMetalFrameScheduler* scheduler,
                         ALMetalSurface* surface);

    // Explicit shader reload at a frame boundary. Keep capture textures and
    // backend ownership; never interrupt an active UI transaction.
    bool reloadMetalShaders(std::string* error = nullptr);

    // Mirror one already-materialized immediate/captured UI draw. Public only
    // because LLVertexBufferData replays outside LLRender::drawBuffer.
    bool drawMetalUIBuffer(LLVertexBuffer* buffer, U32 mode, U32 count,
                           LLImageGL* image, const ALSamplerDesc& sampler,
                           bool texture_bound, bool indexed = false,
                           U32 index_count = 0, U32 indices_offset = 0,
                           U32 first_vertex = 0);
    // Submit with the current texture/sampler and shader state. For indexed
    // draws count/first are index elements; otherwise they are vertex elements.
    // Returns actual native admission, never authority/fallback suppression.
    bool drawBoundMetalUIBuffer(LLVertexBuffer* buffer, U32 mode, U32 count,
                                bool indexed = false, U32 first = 0);
    // Indexed sphere capture; uses the same current native binding state.
    bool captureMetalUIIndexedBuffer(LLVertexBuffer* buffer, U32 mode,
                                     U32 index_count, U32 indices_offset);

    // Parallel offscreen capture for GL-owned immediate-mode render-texture
    // producers by default. A single unsupported batch fails the complete
    // target closed and end returns no texture. NATIVE_AUTHORITATIVE is the
    // cutover form: supported calls update only the logical state consumed by
    // Metal, captured draws suppress GL, and failure never invokes a fallback.
    // Optional retained color/depth inputs turn this into a world/UI
    // composition capture. They are borrowed until end capture returns. The
    // live surface is never affected and existing texture producers pass no
    // inputs, preserving their color-only behavior.
    bool beginMetalUITextureCapture(
        ALMetalPixelFormat format,
        LLGLSLShader* alpha_mask_shader,
        const ALMetalTexture* background = nullptr,
        const ALMetalTexture* depth = nullptr,
        LLGLSLShader* uniform_color_shader = nullptr,
        EMetalUICaptureMode mode = EMetalUICaptureMode::GL_MIRROR,
        EMetalUICaptureStorage storage =
            EMetalUICaptureStorage::FORMAT_NATIVE,
        std::unique_ptr<ALMetalTexture>* reusable_output = nullptr);
    bool isMetalUITextureCaptureAuthoritative() const noexcept
    {
        return mMetalUICaptureAuthoritative;
    }
    // Record slot-zero sampled-image state without allocating or binding a GL
    // texture/sampler. Used only while an authoritative capture is active.
    bool bindMetalUITextureCapture(S32 slot, LLImageGL* image, ALSampler sampler);
    bool unbindMetalUITextureCapture(S32 slot);
    // Keep capture-only alpha-test state out of LLGLSLShader's retained GL
    // uniform cache so the native transaction cannot desynchronize it from
    // the physical program.
    void setMetalUITextureCaptureMinimumAlpha(
        const LLGLSLShader* shader, F32 minimum);
    // Mirror GL's localized HUD depth bracket into an active retained-depth
    // capture. Coordinates are GL lower-left target pixels and are clipped to
    // the retained target before conversion to Metal's upper-left scissor.
    bool clearMetalUITextureCaptureDepth(
        S32 x, S32 y, S32 width, S32 height, F32 depth);
    bool restoreMetalUITextureCaptureDepth(
        S32 x, S32 y, S32 width, S32 height);
    // Append one indexed HUD-attachment volume draw to an active offscreen
    // capture, using the draw family's sampler rather than an unrelated UI
    // texture slot. Failure invalidates the complete native transaction.
    bool drawMetalHUDIndexedBatch(
        const ALMetalHUDIndexedBatch& batch,
        ALSampler sampler = ALSamplers::AnisoWrap);
    bool hasMetalUITextureCapture() const noexcept;
    // Snapshot and asynchronously read one BGRA8 pixel at the current ordered
    // offscreen UI boundary. Request coordinates use the GL-compatible
    // lower-left row convention retained by the native world/UI texture.
    // Collection returns only samples whose complete source transaction was
    // submitted; neither call waits in steady state.
    bool requestMetalUITextureCapturePixel(S32 x, S32 y);
    bool collectMetalUITextureCapturePixel(
        S32& x, S32& y, LLColor4U& color);
    void releaseMetalUITextureCapturePixels();
    // Synchronously checkpoint the active ordered RGBA8 offscreen capture,
    // return the selected GL-row rectangle, and resume later draws from that
    // complete native output. Reserved for exceptional appearance CPU
    // consumers whose legacy GL path already stalled at this boundary.
    bool readMetalUITextureCaptureRGBA(
        S32 x, S32 y, S32 width, S32 height,
        U8* rgba, size_t bytes_per_row);
    // Encode the selection owner's private face-id/depth pass into the active
    // ordered world/UI capture. GL remains authoritative until the later
    // contour/glow passes complete the transaction.
    bool drawMetalSelectionIDs(const ALMetalSelectionIDScene& scene);
    // Consume the current native selection ids into the default optimized
    // contour/rim and append it to the ordered offscreen transaction.
    bool drawMetalSelectionContour(
        const ALMetalSelectionContourScene& scene);
    // Record one GL-owned draw family that an active offscreen transaction
    // did not reproduce. This is a no-op outside a capture; the authoritative
    // GL draw continues and end capture discards the incomplete native frame.
    void invalidateMetalUITextureCapture(const char* reason);
    // When present_surface is true, a complete BGRA8 capture appends its
    // drawable copy to the still-open offscreen command buffer and commits
    // once. presented is true only after that fused command was submitted.
    std::unique_ptr<ALMetalTexture> endMetalUITextureCapture(
        bool source_success,
        bool present_surface = false,
        bool* presented = nullptr,
        std::string* failure = nullptr,
        EMetalUICaptureEndState* end_state = nullptr);

    // A startup frame can replace the GL window presentation only when every
    // eligible UI batch was encoded. The surface stays transparent for any
    // incomplete frame so the GL fallback remains visible underneath it.
    bool metalUIFrameHasCompleteCoverage() const noexcept;
    // Enable direct native UI replacement on the presentation surface. During
    // world fallback this is a transparent Metal overlay above the GL scene;
    // an active ordered offscreen capture suspends direct replacement so that
    // exactly one renderer owns the surface submission for that frame.
    void setMetalUIReplacementEnabled(bool enabled) noexcept;
    // Submit one already-complete BGRA8 world/UI texture to the native surface.
    // Success means the drawable command was committed, so the caller may
    // suppress exactly this frame's GL swap. The ordinary endFrame call still
    // performs renderer housekeeping but does not submit a second clear
    // drawable over this frame.
    bool presentMetalTexture(const ALMetalTexture* texture);
    bool setMetalPresentationOpaque(bool opaque);
    // Window-owned surface identity and resize generation. Borrowed only;
    // callers must not retain it beyond the installed backend lifetime.
    const ALMetalSurface* getMetalPresentationSurface() const noexcept
    {
        return mMetalUISurface;
    }
#endif

    // Refreshes renderer state to the cached values
    // Needed when the render context has changed and invalidated the current state
    void refreshState(void);

    void translatef(const GLfloat& x, const GLfloat& y, const GLfloat& z);
    void scalef(const GLfloat& x, const GLfloat& y, const GLfloat& z);
    void rotatef(const GLfloat& a, const GLfloat& x, const GLfloat& y, const GLfloat& z);
    void ortho(F32 left, F32 right, F32 bottom, F32 top, F32 zNear, F32 zFar);

    void pushMatrix();
    void popMatrix();
    void loadMatrix(const GLfloat* m);
    void loadMatrix(const LLMatrix4a& m);
    void loadIdentity();
    void multMatrix(const GLfloat* m);
    void multMatrix(const LLMatrix4a& m);
    void matrixMode(eMatrixMode mode);
    eMatrixMode getMatrixMode();

    const LLMatrix4a& getModelviewMatrix();
    const LLMatrix4a& getProjectionMatrix();
    const LLMatrix4a& getTextureMatrix();

    void syncMatrices();
    void syncLightState();

    // ---- Shared forward-lighting uniform block (UB_LIGHTS) --------------------------------
    // The fixed-function light arrays every forward/alpha program reads. They used to be
    // pushed as LOOSE uniforms into each program on its first bind after mLightHash moved --
    // the same ~672 bytes re-derived and re-written once per program. Packed once and bound at
    // a fixed engine point instead, so a light-state change costs one upload total.
    //
    // Holds ONLY what LLRender owns exclusively. sun_up_factor stays LOOSE: it is written from
    // many sites across the pipeline, draw pools and LLSettingsVO, and a shared block would let
    // one writer silently stomp another's value. The sun's, moon's and ambient light the
    // atmospherics read are the Environment block's (LLSettingsVOSky::applySpecial).
    //
    // std140 pads every array element to 16 bytes regardless of the element type, hence the
    // float[4] rows for the vec3/vec2 arrays -- the C++ mirror carries that padding explicitly
    // so the struct IS the layout. Public so the debug layout registration can offsetof it.
    struct alignas(16) LightsUBOData
    {
        F32 light_position[LL_NUM_LIGHT_UNITS][4];
        F32 light_direction[LL_NUM_LIGHT_UNITS][4];             // .xyz used, .w padding
        F32 light_attenuation[LL_NUM_LIGHT_UNITS][4];
        F32 light_deferred_attenuation[LL_NUM_LIGHT_UNITS][4];  // .xy used (size, falloff)
        F32 light_diffuse[LL_NUM_LIGHT_UNITS][4];               // .rgb used, .a padding
        F32 light_ambient[3];  F32 _tail_pad;
    };

    // ---- Shared matrix uniform block (UB_MATRICES) ----------------------------------------
    // The matrix stack plus the matrices derived from it. Like the Lights block above, these
    // used to be LOOSE uniforms re-pushed per program -- and they were the worst offenders:
    // every (modelview, projection) epoch (camera set, shadow cascade, probe face, mirror
    // pass, each UI translate/scale) re-taxed every program bound after it with up to six
    // matrix writes. Packed once per epoch and bound at a fixed engine point, a matrix change
    // costs one upload total and a program bind costs no matrix work at all.
    //
    // Matrices go up as they lie: LLMatrix4a's rows are the columns std140 reads by default.
    // normal is a mat3, which std140 strides at 16 bytes per column, hence float[3][4].
    struct alignas(16) MatricesUBOData
    {
        F32 modelview[16];
        F32 projection[16];
        F32 modelview_projection[16];
        F32 inv_proj[16];
        F32 texture0[16];
        F32 normal[3][4];   // .xyz used per column, .w std140 padding
    };

    void translateUI(F32 x, F32 y, F32 z);
    void scaleUI(F32 x, F32 y, F32 z);
    void pushUIMatrix();
    void popUIMatrix();
    void loadUIIdentity();
    LLVector3 getUITranslation();
    LLVector3 getUIScale();

    void flush();

    // if list is set, will store buffers in list for later use, if list isn't set, will use cache
    void beginList(std::list<LLVertexBufferData> *list);
    void endList();

    void begin(const GLuint& mode);
    void end();

    U8 getMode() const { return mMode; }

    void vertex2i(const GLint& x, const GLint& y);
    void vertex2f(const GLfloat& x, const GLfloat& y);
    void vertex3f(const GLfloat& x, const GLfloat& y, const GLfloat& z);
    void vertex2fv(const GLfloat* v);
    void vertex3fv(const GLfloat* v);

    void texCoord2i(const GLint& x, const GLint& y);
    void texCoord2f(const GLfloat& x, const GLfloat& y);
    void texCoord2fv(const GLfloat* tc);

    void color4ub(const GLubyte& r, const GLubyte& g, const GLubyte& b, const GLubyte& a);
    void color4f(const GLfloat& r, const GLfloat& g, const GLfloat& b, const GLfloat& a);
    void color4fv(const GLfloat* c);
    void color3f(const GLfloat& r, const GLfloat& g, const GLfloat& b);
    void color3fv(const GLfloat* c);
    void color4ubv(const GLubyte* c);

    void diffuseColor3f(F32 r, F32 g, F32 b);
    void diffuseColor3fv(const F32* c);
    void diffuseColor4f(F32 r, F32 g, F32 b, F32 a);
    void diffuseColor4fv(const F32* c);
    void diffuseColor4ubv(const U8* c);
    void diffuseColor4ub(U8 r, U8 g, U8 b, U8 a);
    // CPU mirror of the uniform solid colour last submitted through the
    // diffuseColor* family. Native sidecar captures use it at exact generated-
    // geometry boundaries without issuing a GL query or changing the shader.
    const LLColor4& getDiffuseColor() const { return mMetalSolidColor; }

    // The UI text shader's only per-pass constant. The GL uniform remains
    // owned by its shader; this shadows the value at the batch boundary for
    // the native encoder.
    void setUITextShadowMode(U32 mode);

    void vertexBatchPreTransformed(LLVector4a* verts, S32 vert_count);
    void vertexBatchPreTransformed(LLVector4a* verts, LLVector2* uvs, S32 vert_count);
    void vertexBatchPreTransformed(LLVector4a* verts, LLVector2* uvs, LLColor4U*, S32 vert_count);

    void setColorMask(bool writeColor, bool writeAlpha);
    void setColorMask(bool writeColorR, bool writeColorG, bool writeColorB, bool writeAlpha);
    // The mask currently in force, so a caller can restore what it found instead of
    // assuming a convention. Channels are R, G, B, A. See LLGLSColorMask.
    void getColorMask(bool (&mask)[4]) const
    {
        mask[0] = mCurrColorMask[0];
        mask[1] = mCurrColorMask[1];
        mask[2] = mCurrColorMask[2];
        mask[3] = mCurrColorMask[3];
    }
    void setSceneBlendType(eBlendType type);

    // applies blend func to both color and alpha
    void blendFunc(eBlendFactor sfactor, eBlendFactor dfactor);
    // applies separate blend functions to color and alpha
    void blendFunc(eBlendFactor color_sfactor, eBlendFactor color_dfactor,
               eBlendFactor alpha_sfactor, eBlendFactor alpha_dfactor);
    void getBlendFunc(eBlendFactor& color_sfactor,
                      eBlendFactor& color_dfactor,
                      eBlendFactor& alpha_sfactor,
                      eBlendFactor& alpha_dfactor) const
    {
        color_sfactor = mCurrBlendColorSFactor;
        color_dfactor = mCurrBlendColorDFactor;
        alpha_sfactor = mCurrBlendAlphaSFactor;
        alpha_dfactor = mCurrBlendAlphaDFactor;
    }

    LLLightState* getLight(U32 index);
    // Snapshot the exact CPU mirror consumed by UB_LIGHTS. This updates only
    // the packed bytes; it performs no GL upload or bind, so native sidecar
    // encoders can capture the forward-light contract at a draw boundary.
    const LightsUBOData& getLightsUBOData()
    {
        packLightsUBO();
        return mLightsUBOData;
    }
    void setAmbientLightColor(const LLColor4& color);

    void setLineWidth(F32 width);
    // The semantic width consumed by native pipeline descriptions.
    F32  getLineWidth() const { return mLineWidth; }

    // Depth offset in the forward/semantic convention: a negative factor or
    // unit pulls a fragment toward the viewer. Native pipeline construction
    // translates it for the active depth convention. Redundant sets are
    // dropped without a flush.
    void setPolygonOffset(F32 factor, F32 units);
    F32 getPolygonOffsetFactor() const { return mPolygonOffsetFactor; }
    F32 getPolygonOffsetUnits() const { return mPolygonOffsetUnits; }
    // End work encoded under the previous depth convention after sReverseZ
    // changes. The cached semantic values remain unchanged.
    void rebasePolygonOffset();

    // Control points per PATCHES primitive, read by the tessellation control stage.
    // Redundant sets are dropped without a flush.
    void setPatchVertices(U32 count);

    // How polygons rasterize, using the retained GL_FILL/GL_LINE/GL_POINT
    // numeric vocabulary. Native pipeline creation maps the value.
    void setPolygonMode(U32 mode);
    U32  getPolygonMode() const { return mPolygonMode; }

    // Faces culled, using the retained GL_BACK/GL_FRONT/GL_FRONT_AND_BACK
    // numeric vocabulary. A change flushes queued geometry.
    void setCullFace(U32 face);
    U32  getCullFace() const { return mCullFace; }

    // Point diameter in pixels. A change flushes queued geometry; redundant
    // sets are dropped without one.
    void setPointSize(F32 size);
    F32  getPointSize() const { return mPointSize; }

    // The stencil test GL_STENCIL_TEST (an LLGLState) runs, front and back alike: its function, reference and the
    // bits it compares, what each outcome does to the stencil, and the bits a draw or a clear may write. Each setter
    // flushes what is batched on a change and drops a redundant set without one. ALGLStencil scopes all three.
    void setStencilFunc(U32 func, S32 ref, U32 mask);
    void setStencilOp(U32 stencil_fail, U32 depth_fail, U32 pass);
    void setStencilMask(U32 mask);
    void getStencilFunc(U32& func, S32& ref, U32& mask) const
    {
        func = mStencilFunc;
        ref  = mStencilRef;
        mask = mStencilReadMask;
    }
    void getStencilOp(U32& stencil_fail, U32& depth_fail, U32& pass) const
    {
        stencil_fail = mStencilFail;
        depth_fail   = mStencilDepthFail;
        pass         = mStencilPass;
    }
    U32  getStencilMask() const { return mStencilWriteMask; }

    // How blending combines source and destination, the colour by `rgb` and alpha by `alpha` (GL_FUNC_ADD, GL_MAX
    // and the rest), for every draw buffer. A change flushes what is batched; redundant sets are dropped without one.
    void setBlendEquation(U32 rgb, U32 alpha);
    void getBlendEquation(U32& rgb, U32& alpha) const
    {
        rgb   = mBlendEquationRGB;
        alpha = mBlendEquationAlpha;
    }

    // The viewport, in pixels of the bound framebuffer. Every writer goes through here, LLRenderTarget's binds
    // included; a change flushes what is batched under the old viewport, and redundant sets are dropped without one.
    void setViewport(S32 x, S32 y, S32 width, S32 height);
    // The viewport in force: x, y, width, height. Width is negative until a
    // native pass owner sets it.
    void getViewport(S32 (&box)[4]) const
    {
        box[0] = mViewport[0];
        box[1] = mViewport[1];
        box[2] = mViewport[2];
        box[3] = mViewport[3];
    }

    // Compatibility boundary after sReverseZ changes. Native render-pass
    // descriptors and projection helpers consume sReverseZ directly.
    static void applyDepthConvention();

    // The colour glClear fills colour buffers with. Every writer in the tree goes through
    // here, so the cache is the state. Redundant sets are dropped; nothing batched reads it,
    // so a change needs no flush.
    void setClearColor(const LLColor4& color);
    const LLColor4& getClearColor() const { return mClearColor; }

    // Compatibility boundary for retained call sites. It drains queued work;
    // native target owners encode attachment clears in their pass descriptors.
    void clear(U32 mask);

    // Retained instanced-draw entry point. Until its native encoder owner is
    // supplied, it drains queued work and fails closed with a diagnostic.
    void drawArraysInstanced(eGeomModes mode, U32 first, U32 count, U32 instances);

    // Retained indirect-draw entry point. The command is validated, then the
    // operation fails closed until a native encoder owner is supplied.
    void drawArraysIndirect(eGeomModes mode, const ALGPUBuffer& buffer, size_t offset);

    // End queued work at a texture dependency boundary. Native encoder/pass
    // ownership expresses the resource dependency itself.
    void textureBarrier();

    // ---- Compute ---------------------------------------------------------------------------------------------------
    // Everything here needs sCompute and asserts it.

    // Make later reads of the kinds in `barriers` (eBarrier bits, or-ed) see what shaders wrote before it -- storage
    // buffers, images and atomics are not ordered against anything else. What is batched is drawn first.
    void memoryBarrier(U32 barriers);

    // Retained compute entry point. Zero groups do nothing; non-zero work
    // fails closed until a native compute encoder owner is supplied.
    void dispatchCompute(U32 groups_x, U32 groups_y, U32 groups_z);

    // Retained indirect-compute entry point. The command is validated, then
    // the operation fails closed until a native compute owner is supplied.
    void dispatchComputeIndirect(const ALGPUBuffer& buffer, size_t offset);

    // ---- Frames ----------------------------------------------------------------------------------------------------

    // This context's queries: the viewer's display() retires them before its first cull reads them. Released with the
    // context in shutdown().
    ALQueryPool& queries() { return mQueries; }

    // Viewer frames completed by the ordered Metal scheduler submission
    // timeline. Released with the native backend in shutdown().
    ALFrameFences& frameFences() { return mFrameFences; }

    // Ends frame-scoped work: legacy queries close only while their migration
    // owner exists, the Metal submission serial is recorded, and resource-ring
    // counters close. `frames_ahead`, above 0, paces the CPU against the
    // scheduler timeline.
    void endFrame(U32 frames_ahead = 0);

    // Native scissor box in target pixels. Every writer goes through here; a
    // change flushes work queued under the old box.
    void setScissor(S32 x, S32 y, S32 width, S32 height);
    // The box in force: x, y, width, height. Width is negative until set.
    void getScissor(S32 (&box)[4]) const
    {
        box[0] = mScissor[0];
        box[1] = mScissor[1];
        box[2] = mScissor[2];
        box[3] = mScissor[3];
    }

    ALTextureSlot* getTextureSlot(U32 index);

    // The image unit `index`, or a slot that binds nothing past AL_NUM_IMAGE_SLOTS.
    ALImageSlot* getImageSlot(U32 index);

    // Drop these texture names, about to be deleted in this context, from every slot's
    // bind cache, the image slots' included. Deleting a bound texture returns that binding to 0, and GL may hand the
    // name out again; a slot still caching it would take the new texture for bound already
    // and skip the bind.
    void forgetTextures(S32 count, const U32* names);

    U32 getCurrentTexUnitIndex(void) const { return mCurrTextureUnitIndex; }

    // Resolve a stable sampler identity belonging to this renderer. Sampling intent only --
    // see ALSampler. Compose with |, or use a named composition in ALSamplers.
    U32 getSampler(ALSampler key) { return mSamplerCache.get(key); }

    // For sampling modes a mask cannot express; see ALSamplerCache::get(desc).
    U32 getSampler(const ALSamplerDesc& desc) { return mSamplerCache.get(desc); }
    bool describeSampler(U32 identity, ALSamplerDesc& desc) const
    {
        return mSamplerCache.describe(identity, desc);
    }

    // Changes whenever logical identities are invalidated, allowing hot paths to cache one.
    U32 getSamplerGeneration() const { return mSamplerCache.getGeneration(); }

    // Invalidate identities and matching slot caches. The anisotropy preference caller warms
    // them again after changing the descriptor inputs; shutdown leaves them empty.
    void clearSamplers();

    // Build every logical sampler identity. get() has no lazy path.
    void warmupSamplers();

    bool verifyTexUnitActive(U32 unitToVerify);

    void debugTexUnits(void);

    void clearErrors();

    struct Vertex
    {
        GLfloat v[3];
        GLubyte c[4];
        GLfloat uv[2];
    };

public:
    static U32 sUICalls;
    static U32 sUIVerts;
    // The draws issued, through LLVertexBuffer and LLRender's own, and the vertices or indices they took, since the
    // frame's statistics were last reset (LLPipeline::resetFrameStats).
    static U32 sDrawCalls;
    static U64 sDrawElements;
#if LL_DARWIN
    // Process-wide command-buffer commits issued through the native Metal
    // wrapper. Frame-to-frame deltas are submission counts, not GPU time.
    static U64 metalCommandBuffersCommitted();
    struct MetalCommandTiming
    {
        U64 submitted = 0;
        U64 completed = 0;
        U64 timed = 0;
        U64 gpu_time_nanoseconds = 0;
    };
    static void setMetalCommandTimingEnabled(bool enabled);
    static MetalCommandTiming metalCommandTiming();
#endif
    static F32 sAnisotropicFilteringLevel;
    static bool sGLCoreProfile;
    static bool sNsightDebugSupport;
    static LLVector2 sUIGLScaleFactor;
    static bool sClassicMode; // classic sky mode active
    static bool sMirrorPass;  // hero-probe planar-reflection (mirror clip) pass active
    static bool s10bitBackBuffer;
    // Reverse-Z depth active this session: Metal's [0,1] depth, reversed
    // projections (near=1, far=0), depth cleared to 0, default GREATER comparison.
    // Newview latches the setting before selecting native resources/constants.
    static bool sReverseZ;

    // Compute is selected for the native pass plan and has not been disabled
    // by the user. Newview latches it with the other shader inputs.
    static bool sCompute;

    // The GBuffer normal attachment is GL_RGBA16 when the deferred targets are HDR and
    // GL_RGB10_A2 when they are not, which changes how many bits its blue channel has to spend
    // on the packed geometric normal -- 16 versus 10. Latched by newview at shader load, before
    // anything compiles (llrender cannot read settings), and the attachment's format is then picked
    // from it, because a shader compiled against the wrong assumption unpacks noise.
    static bool sGBufferNormHDR;

private:
    friend class LLLightState;
    // The tests' state guard reads how deep each matrix stack is.
    friend struct ll_test::RenderProbe;

#if LL_DARWIN
    bool updateMetalUITextureCaptureDepth(
        S32 x, S32 y, S32 width, S32 height, bool restore, F32 depth);
#endif

    LLVertexBuffer* bufferfromCache(U32 attribute_mask, U32 count);
    LLVertexBuffer* genBuffer(U32 attribute_mask, S32 count);
    void drawBuffer(LLVertexBuffer* vb, U32 mode, S32 count);
    void resetStriders(S32 count);
    void resetCachedState();

    eMatrixMode mMatrixMode;
    U32 mMatIdx[NUM_MATRIX_MODES];
    U32 mMatHash[NUM_MATRIX_MODES];
    LLMatrix4a mMatrix[NUM_MATRIX_MODES][LL_MATRIX_STACK_DEPTH];
    U32 mCurMatHash[NUM_MATRIX_MODES];
    U32 mLightHash;
    LLColor4 mAmbientLightColor;

    // Pack the light state into mLightsUBOData. Returns true if the bytes actually changed
    // (so the caller uploads). mLightHash is a conservative trigger -- setPosition and
    // setSpotDirection bump it unconditionally because the modelview may have moved, so a
    // frame's worth of "changes" are frequently byte-identical -- hence the compare.
    bool packLightsUBO();

    LightsUBOData   mLightsUBOData{};
    ALUniformBuffer mLightsUBO;
    U32             mLightsUBOHash  = 0xFFFFFFFFu;
    bool            mLightsUBOBound = false;

    // Rebuild the matrix block from the current stacks into the buffer's shadow. The derived
    // matrices are recomputed only for the stack that actually moved.
    void packMatricesUBO();

    ALUniformBuffer mMatricesUBO;
    U32             mMatricesUBOHash[NUM_MATRIX_MODES] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
    bool            mMatricesUBOBound = false;

    ALFrameFences mFrameFences;
    ALQueryPool   mQueries{ mFrameFences };

    // Derived matrices, cached across epochs so a stack that did not move is not re-derived.
    // Members rather than function statics: they belong to this context, and shutdown() has to
    // be able to invalidate them along with the block.
    LLMatrix4a      mCachedInvMdv;
    LLMatrix4a      mCachedInvProj;
    LLMatrix4a      mCachedMVP;

    bool            mDirty;
    U32             mCount;
    U32             mMode;
    U32             mCurrTextureUnitIndex;
    bool            mCurrColorMask[4];
    F32             mLineWidth = 1.f;
    // Semantic (forward-convention) polygon offset.
    F32             mPolygonOffsetFactor = 0.f;
    F32             mPolygonOffsetUnits = 0.f;
    U32             mPatchVertices = 3;
    LLColor4        mClearColor = LLColor4(0.f, 0.f, 0.f, 0.f);
    S32             mScissor[4] = { 0, 0, -1, -1 };              // unknown until set (see getScissor)
    S32             mViewport[4] = { 0, 0, -1, -1 };             // unknown until set (see getViewport)
    // Established semantic defaults consumed by native pipeline descriptions.
    U32             mPolygonMode = GL_FILL;
    U32             mCullFace = GL_BACK;
    F32             mPointSize = 1.f;
    U32             mStencilFunc = GL_ALWAYS;
    S32             mStencilRef = 0;
    U32             mStencilReadMask = ~0u;
    U32             mStencilFail = GL_KEEP;
    U32             mStencilDepthFail = GL_KEEP;
    U32             mStencilPass = GL_KEEP;
    U32             mStencilWriteMask = ~0u;
    U32             mBlendEquationRGB = GL_FUNC_ADD;
    U32             mBlendEquationAlpha = GL_FUNC_ADD;

    LLPointer<LLVertexBuffer>   mBuffer;
    LLStrider<LLVector4a>       mVerticesp;
    LLStrider<LLVector2>        mTexcoordsp;
    LLStrider<LLColor4U>        mColorsp;
    std::array<ALTextureSlot, AL_NUM_TEXTURE_SLOTS> mTextureSlots;
    // This context's sampler objects. Sits beside mTextureSlots deliberately: the units hold
    // the bindings, this holds the objects those bindings name, and the two have to be
    // torn down together and by the same thread.
    ALSamplerCache mSamplerCache;
    ALTextureSlot           mDummySlot;
    std::array<ALImageSlot, AL_NUM_IMAGE_SLOTS> mImageSlots;
    ALImageSlot             mDummyImageSlot;
    std::array<LLLightState, LL_NUM_LIGHT_UNITS> mLightState;

    eBlendFactor mCurrBlendColorSFactor;
    eBlendFactor mCurrBlendColorDFactor;
    eBlendFactor mCurrBlendAlphaSFactor;
    eBlendFactor mCurrBlendAlphaDFactor;

    U32      mUITextShadowMode = 0;
    LLColor4 mMetalSolidColor = LLColor4(1.f, 1.f, 1.f, 1.f);

#if LL_DARWIN
    static constexpr U32 METAL_UI_CAPTURE_CACHE_SIZE = 8;
    std::unique_ptr<ALMetalUIRenderer> mMetalUIRenderer;
    std::unique_ptr<ALMetalUIRenderer> mMetalUICaptureRenderer;
    std::unique_ptr<ALMetalTexture> mMetalUICapturePixel;
    std::unique_ptr<ALMetalTextureReadbackRing> mMetalUICapturePixelReads;
    std::deque<std::pair<S32, S32>> mMetalUICapturePixelReadAt;
    std::pair<S32, S32> mMetalUICapturePixelAt{};
    // Offscreen capture output textures are transferred to their callers, but
    // the format-specific renderer owns reusable shader libraries, pipeline
    // states, samplers and intermediate selection targets. Keep one idle
    // renderer per supported output-format/storage-view pair so continuous
    // world/UI capture does not rebuild that state every frame.
    std::array<std::unique_ptr<ALMetalUIRenderer>,
               METAL_UI_CAPTURE_CACHE_SIZE> mMetalUICaptureCache;
    ALMetalDevice* mMetalDevice = nullptr;
    ALMetalFrameScheduler* mMetalFrames = nullptr;
    ALMetalSurface* mMetalUISurface = nullptr;
    U64 mMetalUILoggedFrame = 0;
    U64 mMetalUIAttemptedBatches = 0;
    U64 mMetalUIFallbackBatches = 0;
    U64 mMetalUIFrameAttemptedBatches = 0;
    U64 mMetalUIFrameFallbackBatches = 0;
    U64 mMetalUICaptureBatches = 0;
    U64 mMetalUICaptureRendererCreations = 0;
    U64 mMetalUICaptureRendererReuses = 0;
    U32 mMetalUICaptureCacheIndex = METAL_UI_CAPTURE_CACHE_SIZE;
    U32 mMetalUICaptureDepthWidth = 0;
    U32 mMetalUICaptureDepthHeight = 0;
    LLGLSLShader* mMetalUICaptureAlphaShader = nullptr;
    LLGLSLShader* mMetalUICaptureUniformColorShader = nullptr;
    F32 mMetalUICaptureMinimumAlpha = 0.004f;
    ALSamplerDesc mMetalUICaptureSampler =
        ALSamplerCache::makeDesc(ALSamplers::BilinearClamp);
    bool mMetalUICaptureTextureBound = false;

    struct MetalUICaptureSavedState
    {
        GLuint shader = 0;
        LLGLSLShader* shader_ptr = nullptr;
        U32 texture = 0;
        ALTextureSlot::eTextureType texture_type = ALTextureSlot::TT_NONE;
        LLImageGL* image = nullptr;
        ALMetalTexture* native_texture = nullptr;
        U32 sampler = 0;
        eBlendFactor blend_color_source = BF_UNDEF;
        eBlendFactor blend_color_destination = BF_UNDEF;
        eBlendFactor blend_alpha_source = BF_UNDEF;
        eBlendFactor blend_alpha_destination = BF_UNDEF;
        bool color_mask[4] = {true, true, true, true};
        S32 viewport[4] = {0, 0, -1, -1};
        LLColor4U color{};
        LLVector2 texcoord{};
        eMatrixMode matrix_mode = MM_MODELVIEW;
        U32 primitive_mode = TRIANGLES;
        bool valid = false;
    };
    MetalUICaptureSavedState mMetalUICaptureSavedState;
    // First exactness failure in the current offscreen transaction. Keeping
    // only the first owner makes a discarded snapshot or drawable probe
    // diagnosable without producing per-batch log noise.
    std::string mMetalUICaptureFirstFailure;
    bool mMetalUICaptureComplete = false;
    bool mMetalUICaptureUsesDepth = false;
    bool mMetalUICaptureAuthoritative = false;
    bool mMetalUIReplacementEnabled = true;
    bool mMetalUISurfaceFrameSubmitted = false;
#endif

    std::vector<LLVector4a> mUIOffset;
    std::vector<LLVector4a> mUIScale;

    struct LLVBCache
    {
        LLPointer<LLVertexBuffer> vb;
        std::chrono::steady_clock::time_point touched;
    };

    boost::unordered_map<U64, LLVBCache> mVBCache;
    std::list<LLVertexBufferData>* mBufferDataList = nullptr;
};

extern S32 gGLViewport[4];

extern thread_local LLRender gGL;

// --- Reverse-Z projection helpers (gated on LLRender::sReverseZ) --------------
// Rewrite a forward [-1,1] projection into reversed zero-to-one (near->1, far->0):
// z_ndc' = (1 - z_ndc)/2, i.e. the depth column becomes 0.5*(w column - depth column).
// Unconditional math; valid for any forward projection, including the hand-rolled
// shadow perspective whose depth rides an off-diagonal clip component.
LLMatrix4a al_reverse_z_transform(const LLMatrix4a& forward_proj);
// Perspective / ortho that emit reversed-ZO when sReverseZ, else the plain forward
// matrix: LLMatrix4a::perspective / ortho with the convention applied.
LLMatrix4a al_perspective(F32 fovy_rad, F32 aspect, F32 z_near, F32 z_far);
// z_far may be +infinity, which under reverse-Z gives the infinite projection (stored depth = near /
// distance, nothing clipped far). True for such a matrix, pick or zoom applied or not.
bool al_projection_is_infinite(const LLMatrix4a& proj);
LLMatrix4a al_ortho(F32 left, F32 right, F32 bottom, F32 top, F32 z_near, F32 z_far);
// project / unproject honoring the active convention: the zero-to-one forms under
// reverse-Z, since the projection already outputs [0,1] window z. The results carry
// w = 1.
LLVector4a al_project(const LLVector4a& obj, const LLMatrix4a& modelview, const LLMatrix4a& proj, const S32 viewport[4]);
LLVector4a al_unproject(const LLVector4a& win, const LLMatrix4a& modelview, const LLMatrix4a& proj, const S32 viewport[4]);
// The same through the inverse of modelview then projection, for a caller with several
// points against one camera.
LLVector4a al_unproject(const LLVector4a& win, const LLMatrix4a& inverse, const S32 viewport[4]);
// Window depth of the near / far plane under the active convention.
inline F32 al_window_near() { return LLRender::sReverseZ ? 1.f : 0.f; }
inline F32 al_window_far()  { return LLRender::sReverseZ ? 0.f : 1.f; }

#define LL_SHADER_LOADING_WARNS(...) LL_WARNS()

#endif
