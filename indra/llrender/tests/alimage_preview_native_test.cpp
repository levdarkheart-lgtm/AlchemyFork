/**
 * @file alimage_preview_native_test.cpp
 * @brief Actual image-preview publication bodies retry real native resources.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llgl.h"
#include "../llglslshader.h"
#include "../llgltexture.h"
#include "../llimagegl.h"
#include "../llrender.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalframe.h"
#include "../../almetal/almetalresource.h"
#include "../../almetal/almetalsculptpreview.h"
#include "llcoord.h"
#include "v4color.h"
#include "llmatrix4a.h"
#include "llcamera.h"
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <map>
#include <type_traits>
#include <vector>

namespace
{
constexpr U32 width = 8, height = 8;
using Pixels = std::array<U8, width * height * 4>;

struct NativeImages
{
    ALRenderCapabilities old_capabilities = gRenderCapabilities;
    ALMetalDevice* old_device = LLImageGL::getMetalDevice();
    bool old_authority = LLImageGL::isMetalTextureAuthority();
    U64 old_image_bytes = LLImageGL::getTextureBytesAllocated();
    std::size_t old_image_count = LLImageGL::sImageList.size();
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    std::unique_ptr<ALMetalLitPreviewRenderer> renderer;
    std::unique_ptr<ALMetalBuffer> vertices;

    NativeImages()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        renderer = ALMetalLitPreviewRenderer::create(*device, &error);
        INFO(error); REQUIRE(renderer);
        const std::array<float, 9> positions{-1,-1,.5f, 3,-1,.5f, -1,3,.5f};
        ALMetalBufferDescriptor descriptor;
        descriptor.length = sizeof(positions);
        descriptor.storage = ALMetalStorageMode::shared;
        vertices = ALMetalBuffer::create(*device, descriptor, &error);
        INFO(error); REQUIRE(vertices);
        REQUIRE(vertices->write(0, positions.data(), sizeof(positions), &error));
        gRenderCapabilities.initializeMetal(device->info());
        LLImageGL::setMetalDevice(device.get());
        LLImageGL::setMetalTextureAuthority(true);
    }
    ~NativeImages()
    {
        CHECK(LLImageGL::getTextureBytesAllocated() == old_image_bytes);
        CHECK(LLImageGL::sImageList.size() == old_image_count);
        LLImageGL::setMetalDevice(old_device);
        LLImageGL::setMetalTextureAuthority(old_authority);
        gRenderCapabilities = old_capabilities;
    }
};

struct MissingImageBackend
{
    ALMetalDevice* previous = LLImageGL::getMetalDevice();
    MissingImageBackend() { LLImageGL::setMetalDevice(nullptr); }
    ~MissingImageBackend() { LLImageGL::setMetalDevice(previous); }
};

// The viewer identity and camera/scene construction are named inputs here.
// The real LLGLTexture owns dimensions/LLImageGL and both allocation methods;
// the included production bodies own generation, publication and dirty state.
// This does not construct a dummy avatar, sculpt volume, floater or camera.
using LLViewerTexture = LLGLTexture;
class LLViewerDynamicTexture : public LLViewerTexture
{
public:
    enum class ETargetlessRenderResult { PENDING, COMPLETED };
    explicit LLViewerDynamicTexture(U8 components = 3)
        : LLViewerTexture(width, height, components, false)
    {
        generateMetalTexture();
    }
    void generateMetalTexture();
    bool ensureMetalTexture();
protected:
    LLCoordGL mOrigin;
};

#include "../../newview/dynamic_texture_native.inl"

std::array<S32, 4> camera_input{11, 22, 33, 44};
struct LLScopedImagePreviewCameraState
{
    std::array<S32, 4> previous = camera_input;
    ~LLScopedImagePreviewCameraState() { camera_input = previous; }
};

enum class SceneInput { complete, render_rejected, publication_rejected };
namespace
{
bool prepareImagePreviewAvatarBuffer(LLVertexBuffer* buffer);
}
struct PreviewScene : LLViewerDynamicTexture
{
    NativeImages& native;
    SceneInput input = SceneInput::complete;
    std::array<float, 4> color{.25f, .5f, .75f, 1.f};
    bool mNeedsUpdate = true;
    unsigned scene_calls = 0;
    LLVertexBuffer* geometry_input = nullptr;
    std::unique_ptr<ALMetalTexture> mMetalPreview;
    std::unique_ptr<ALMetalTextureDescriptor> mMetalPreviewDescriptor;

    explicit PreviewScene(NativeImages& resources, U8 components = 3)
        : LLViewerDynamicTexture(components), native(resources) {}
    bool render()
    {
        ++scene_calls;
        camera_input.fill(99);
        mMetalPreview.reset();
        mMetalPreviewDescriptor.reset();
        std::string error;
        if (input == SceneInput::render_rejected)
        {
            // A real native admission failure, without changing the image.
            mMetalPreview = native.renderer->render(ALMetalLitPreviewScene{}, &error);
            CHECK_FALSE(mMetalPreview);
            CHECK_FALSE(error.empty());
            return false;
        }
        if (geometry_input && !prepareImagePreviewAvatarBuffer(geometry_input))
            return false;
        mMetalPreviewDescriptor = std::make_unique<ALMetalTextureDescriptor>(
            mGLTexturep->getMetalTexture()->descriptor());
        if (input == SceneInput::publication_rejected)
            ++mMetalPreviewDescriptor->width;
        ALMetalLitPreviewBatch batch;
        batch.geometry = ALMetalLitPreviewGeometry::unindexed_flat_triangles;
        batch.vertices = native.vertices.get();
        batch.vertex_count = 3;
        if (geometry_input)
        {
            batch.geometry = ALMetalLitPreviewGeometry::indexed_lit;
            batch.vertices = geometry_input->getMetalVertexBuffer();
            batch.position_offset = geometry_input->getOffset(LLVertexBuffer::TYPE_VERTEX);
            batch.normal_offset = geometry_input->getOffset(LLVertexBuffer::TYPE_NORMAL);
            batch.indices = geometry_input->getMetalIndexBuffer();
            batch.index_count = 3;
            for (unsigned axis = 0; axis < 3; ++axis)
                batch.normal_matrix[axis][axis] = 1.f;
            batch.light_direction[0][2] = 1.f;
            batch.light_diffuse[0][0] = 1.f;
            batch.light_diffuse[0][1] = 1.f;
            batch.light_diffuse[0][2] = 1.f;
        }
        std::copy(color.begin(), color.end(), batch.diffuse_color);
        for (unsigned axis = 0; axis < 4; ++axis)
            batch.modelview_projection[axis * 4 + axis] = 1.f;
        ALMetalLitPreviewScene scene;
        scene.batches = &batch;
        scene.batch_count = 1;
        scene.destination_descriptor = mMetalPreviewDescriptor.get();
        scene.linear_storage_view = true;
        mMetalPreview = native.renderer->render(scene, &error);
        INFO(error); REQUIRE(mMetalPreview);
        return true;
    }
};

class LLImagePreviewAvatar : public PreviewScene
{
public:
    using PreviewScene::PreviewScene;
    ETargetlessRenderResult renderWithoutTarget();
};
class LLImagePreviewSculpted : public PreviewScene
{
public:
    using PreviewScene::PreviewScene;
    ETargetlessRenderResult renderWithoutTarget();
};

// Named inputs for the avatar producer's scheduling boundary. The production
// helper below runs unchanged; these inputs do not construct a viewer avatar.
struct LLDrawable
{
    static constexpr U32 REBUILD_GEOMETRY = 1;
    bool rebuild = false;
    bool isState(U32 state) const { return state == REBUILD_GEOMETRY && rebuild; }
};
struct PreviewDrawableInput
{
    LLDrawable* value = nullptr;
    bool notNull() const { return value != nullptr; }
    LLDrawable* operator->() const { return value; }
};
class LLVOAvatar
{
public:
    PreviewDrawableInput mDrawable;
    S32 mDirtyMesh = 0;
    bool lod_refreshes = false;
    unsigned morph_revision = 2;
    unsigned vertex_revision = 1;
    std::vector<char> operations;
    U32 composite_updates = 0, visual_updates = 0, geometry_updates = 0;
    void updateComposites() { ++composite_updates; }
    void updateVisualParams() { ++visual_updates; }
    void updateGeometry(const PreviewDrawableInput&) { ++geometry_updates; }
    bool isMeshDirty() const { return mDirtyMesh > 0; }
    void updateLOD()
    {
        operations.push_back('L');
        if (lod_refreshes) updateMeshData();
    }
    void updateMeshData()
    {
        operations.push_back('M');
        vertex_revision = morph_revision;
        mDirtyMesh = 0;
        mDrawable->rebuild = false;
    }
};

#include "../../newview/image_preview_native.inl"

namespace ModelPreviewInputs
{
// Only the viewer's selected preview lights are named inputs. The actual
// model batch admission, transforms and packing below run unchanged.
struct LLPipeline
{
    inline static LLVector3 PreviewDirection0{0.f, 0.f, 1.f};
    inline static LLVector3 PreviewDirection1{0.f, 0.f, 1.f};
    inline static LLVector3 PreviewDirection2{0.f, 0.f, 1.f};
    inline static LLColor4 PreviewDiffuse0{1.f, 1.f, 1.f, 1.f};
    inline static LLColor4 PreviewDiffuse1{0.f, 0.f, 0.f, 1.f};
    inline static LLColor4 PreviewDiffuse2{0.f, 0.f, 0.f, 1.f};
};
const LLColor4 PREVIEW_EDGE_COL(0.4f, 0.4f, 0.4f, 1.0);
#include "../../newview/model_preview_native.inl"
}

// These names supply only viewer camera/scene inputs. The four complete
// publication methods below, native backing generator and LLImageGL owners
// are production code. No floater, viewer avatar or material scene is built.
using LLScopedModelPreviewCameraState = LLScopedImagePreviewCameraState;
using LLScopedAnimationPreviewCameraState = LLScopedImagePreviewCameraState;
class LLModelPreview : public PreviewScene
{
public:
    using PreviewScene::PreviewScene;
    std::map<std::string, bool> mViewOption;
    ETargetlessRenderResult renderWithoutTarget();
};
class LLPreviewAnimation : public PreviewScene
{
public:
    using PreviewScene::PreviewScene;
    ETargetlessRenderResult renderWithoutTarget();
};
class LLViewerCamera : public LLCamera
{
public:
    static LLViewerCamera* getInstance()
    { static LLViewerCamera camera; return &camera; }
    static const LLCamera& getCurrent() { return current(); }
    static void setCurrent(const LLCamera& camera) { current() = camera; }
    static void updateFrustumPlanes(LLCamera&) {}
    void setViewNoBroadcast(F32 view) { setView(view); }
private:
    static LLCamera& current() { static LLCamera camera; return camera; }
};
class LLVisualParamHint : public PreviewScene
{
public:
    using PreviewScene::PreviewScene;
    bool mIsVisible = false;
    bool clear_depth_input = true;
    void preRender(bool clear_depth) { clear_depth_input = clear_depth; }
    bool render()
    {
        LLViewerCamera::getInstance()->setOrigin(LLVector3(9.f, 8.f, 7.f));
        LLViewerCamera::setCurrent(*LLViewerCamera::getInstance());
        std::fill(std::begin(gGLViewport), std::end(gGLViewport), 99);
        return PreviewScene::render();
    }
    ETargetlessRenderResult renderWithoutTarget();
};
LLVOAvatar* gAgentAvatarp = nullptr;
class LLVisualParamReset : public LLViewerDynamicTexture
{
public:
    LLVisualParamReset() : LLViewerDynamicTexture(1) {}
    inline static bool sDirty = false;
    bool render();
    ETargetlessRenderResult renderWithoutTarget();
};
class LLGLTFPreviewTexture : public PreviewScene
{
public:
    explicit LLGLTFPreviewTexture(NativeImages& resources)
        : PreviewScene(resources, 4) {}
    enum class ENativeAttemptResult { NOT_ATTEMPTED, PENDING, FAILED, READY };
    bool mShouldRender = true;
    S32 mBestLoad = 5, mPendingLoad = 0;
    ENativeAttemptResult mNativeAttemptResult = ENativeAttemptResult::NOT_ATTEMPTED;
    void preRender() {}
    bool render()
    {
        // Named eligibility input matches the real render's missing native
        // destination descriptor branch. Full PBR scene construction is not run.
        if (!getGLTexture()->getMetalTexture())
        { mNativeAttemptResult = ENativeAttemptResult::FAILED; return false; }
        const bool rendered = PreviewScene::render();
        mNativeAttemptResult = rendered ? ENativeAttemptResult::READY
                                       : ENativeAttemptResult::FAILED;
        return rendered;
    }
    bool publishMetalPreview()
    {
        return mGLTexturep->adoptMetalRenderTexture(std::move(mMetalPreview));
    }
    ETargetlessRenderResult renderWithoutTarget();
};
#include "../../newview/model_preview_publication_native.inl"
#include "../../newview/animation_preview_native.inl"
#include "../../newview/appearance_hint_native.inl"
#include "../../newview/gltf_preview_native.inl"

// Layer content/readiness and projection bookkeeping are named inputs. The
// included production pre/post/targetless transaction uses actual gGL capture,
// the real native shader and actual LLImageGL adoption/metadata below.
struct LLTexLayerSetBuffer
{
    S32 projection_balance = 0;
    void preRenderTexLayerSet() { ++projection_balance; }
    void postRenderTexLayerSet(bool) { --projection_balance; }
};
LLGLSLShader* bake_alpha_input = nullptr;
class LLViewerTexLayerSetBuffer : public LLViewerDynamicTexture,
                                public LLTexLayerSetBuffer
{
public:
    LLViewerTexLayerSetBuffer() : LLViewerDynamicTexture(4) {}
    std::unique_ptr<ALMetalTexture> mMetalComposite;
    bool mMetalCaptureActive = false, mLastNativeRenderSucceeded = false;
    bool mNeedsUpdate = true, source_ready = true, reject_scene = false;
    bool scene_saw_backing = false;
    U32 update_calls = 0, scene_calls = 0;
    ALMetalTexture* white_input = nullptr;
    void preRenderTexLayerSet();
    void postRenderTexLayerSet(bool success);
    ETargetlessRenderResult renderWithoutTarget();
    bool isReadyToUpdate() const { return source_ready; }
    void doUpdate() { ++update_calls; mNeedsUpdate = false; }
    bool renderTexLayerSet(std::nullptr_t)
    {
        ++scene_calls;
        scene_saw_backing = getGLTexture()->getMetalTexture() != nullptr;
        if (reject_scene) return false;
        bake_alpha_input->bind();
        REQUIRE(gGL.getTextureSlot(0)->bindNative(
            white_input, ALTextureSlot::TT_TEXTURE, ALSamplers::PointClamp));
        gGL.color4ub(64, 128, 191, 255);
        gGL.begin(LLRender::TRIANGLES);
        gGL.texCoord2f(0.f, 0.f); gGL.vertex3f(-1.f, -1.f, 0.f);
        gGL.texCoord2f(2.f, 0.f); gGL.vertex3f(3.f, -1.f, 0.f);
        gGL.texCoord2f(0.f, 2.f); gGL.vertex3f(-1.f, 3.f, 0.f);
        gGL.end(); gGL.flush(); bake_alpha_input->unbind();
        return true;
    }
};
#pragma push_macro("gAlphaMaskProgram")
#define gAlphaMaskProgram (*bake_alpha_input)
#include "../../newview/texlayer_preview_native.inl"
#pragma pop_macro("gAlphaMaskProgram")

struct NativeComposite : NativeImages
{
    ALMetalDevice* old_vertex_device = LLImageGL::getMetalDevice();
    bool old_vertex_authority = LLVertexBuffer::isMetalBufferAuthority();
    LLGLSLShader* old_shader = LLGLSLShader::sCurBoundShaderPtr;
    U32 old_shader_id = LLGLSLShader::sCurBoundShader;
    U32 old_calls = LLRender::sDrawCalls;
    U64 old_elements = LLRender::sDrawElements;
    LLRender::eMatrixMode old_mode = gGL.getMatrixMode();
    LLGLSLShader alpha;
    LLGLSLShader* old_alpha_input = bake_alpha_input;
    std::unique_ptr<ALMetalFrameScheduler> frames;
    LLPointer<LLImageGL> white;
    bool initialized = false, matrices = false;
    NativeComposite()
    {
        REQUIRE_FALSE(gGLManager.mInited);
        // NativeImages has installed this fixture's device; the inherited
        // old_device is the actual pre-fixture renderer owner to restore.
        old_vertex_device = old_device;
        frames = ALMetalFrameScheduler::create(*device, 2, &error); REQUIRE(frames);
        LLVertexBuffer::setMetalDevice(device.get());
        LLVertexBuffer::setMetalBufferAuthority(true); LLVertexBuffer::initClass(nullptr);
        gGL.setMetalBackend(device.get(), frames.get(), nullptr); initialized = true;
        REQUIRE(gGL.initMetal(true));
        for (auto mode : {LLRender::MM_MODELVIEW, LLRender::MM_PROJECTION, LLRender::MM_TEXTURE0})
        { gGL.matrixMode(mode); gGL.pushMatrix(); gGL.loadIdentity(); }
        matrices = true; gGL.matrixMode(LLRender::MM_MODELVIEW);
        alpha.mName = "Native composite destination bootstrap input";
        alpha.mShaderFiles = {{"interface/alphamaskV.glsl", GL_VERTEX_SHADER},
                              {"interface/alphamaskF.glsl", GL_FRAGMENT_SHADER}};
        alpha.setNativeAttributeMask(LLVertexBuffer::MAP_VERTEX |
            LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR);
        REQUIRE(alpha.createShader()); bake_alpha_input = &alpha;
        LLPointer<LLImageRaw> raw = new LLImageRaw(1, 1, 4);
        std::memset(raw->getData(), 255, 4); white = new LLImageGL(raw, false);
        REQUIRE(white->getMetalTexture());
    }
    ~NativeComposite()
    {
        if (!initialized) return;
        if (gGL.hasMetalUITextureCapture()) gGL.endMetalUITextureCapture(false);
        if (LLGLSLShader::sCurBoundShaderPtr) LLGLSLShader::sCurBoundShaderPtr->unbind();
        if (matrices) for (auto mode : {LLRender::MM_TEXTURE0, LLRender::MM_PROJECTION, LLRender::MM_MODELVIEW})
        { gGL.matrixMode(mode); gGL.popMatrix(); }
        gGL.matrixMode(old_mode); CHECK(frames->waitUntilIdle(5000, &error));
        white = nullptr; gGL.shutdown(); gGL.setMetalBackend(nullptr, nullptr, nullptr);
        LLVertexBuffer::cleanupClass(); LLVertexBuffer::setMetalDevice(old_vertex_device);
        LLVertexBuffer::setMetalBufferAuthority(old_vertex_authority);
        LLGLSLShader::sCurBoundShader = old_shader_id;
        LLGLSLShader::sCurBoundShaderPtr = old_shader;
        bake_alpha_input = old_alpha_input;
        LLRender::sDrawCalls = old_calls; LLRender::sDrawElements = old_elements;
    }
};

Pixels pixels(NativeImages& native, const ALMetalTexture& texture)
{
    Pixels result{};
    REQUIRE(texture.width() == width);
    REQUIRE(texture.height() == height);
    REQUIRE(texture.bytesPerPixel() == 4);
    REQUIRE(texture.readback(*native.device, 0, 0, {}, result.data(), width * 4,
                            &native.error));
    return result;
}

void checkColor(const Pixels& actual, const std::array<U8, 4>& color)
{
    for (std::size_t offset = 0; offset < actual.size(); offset += 4)
        for (std::size_t channel = 0; channel < color.size(); ++channel)
            CHECK(actual[offset + channel] == color[channel]);
}
}

TEST_CASE("Avatar image preview refreshes ordinary dirty morph geometry before extraction",
          "[preview-geometry-policy]")
{
    LLDrawable drawable;
    LLVOAvatar avatar;
    avatar.mDrawable.value = &drawable;
    unsigned expected_updates = 0;
    SECTION("a warmed priority-one morph needs its final mesh refresh")
    {
        avatar.mDirtyMesh = 1;
        expected_updates = 1;
    }
    SECTION("an unchanged mesh keeps its current revision")
    {
        avatar.vertex_revision = avatar.morph_revision;
    }
    SECTION("LOD already refreshed priority-two geometry once")
    {
        avatar.mDirtyMesh = 2;
        avatar.lod_refreshes = true;
        expected_updates = 1;
    }
    SECTION("an unavailable drawable remains pending")
    {
        avatar.mDrawable.value = nullptr;
        avatar.mDirtyMesh = 1;
    }
    updateImagePreviewAvatarGeometry(&avatar);
    REQUIRE_FALSE(avatar.operations.empty());
    CHECK(avatar.operations.front() == 'L');
    CHECK(std::count(avatar.operations.begin(), avatar.operations.end(), 'M') == expected_updates);
    if (avatar.mDrawable.notNull())
    {
        CHECK(avatar.mDirtyMesh == 0);
        CHECK(avatar.vertex_revision == avatar.morph_revision);
    }
    else
    {
        CHECK(avatar.mDirtyMesh == 1);
        CHECK(avatar.vertex_revision == 1);
    }
}

TEST_CASE("Avatar image preview retries and admits only current indexed native stores",
          "[almetal][preview-geometry-publication]")
{
    NativeImages native;
    // This isolated fixture starts with no vertex owner. It exercises the
    // real LLVB publisher, not a boolean substitute for publication state.
    REQUIRE_FALSE(LLVertexBuffer::isMetalBufferAuthority());
    REQUIRE(LLVertexBuffer::getBytesAllocated() == 0);
    REQUIRE(native.old_device == nullptr);
    LLVertexBuffer::setMetalDevice(native.device.get());
    LLVertexBuffer::setMetalBufferAuthority(true);
    LLVertexBuffer::initClass(nullptr);
    struct VertexBackendScope
    {
        ~VertexBackendScope()
        {
            LLVertexBuffer::flushBuffers();
            CHECK(LLVertexBuffer::getBytesAllocated() == 0);
            LLVertexBuffer::cleanupClass();
            LLVertexBuffer::setMetalBufferAuthority(false);
            LLVertexBuffer::setMetalDevice(nullptr);
        }
    } backend;
    struct MissingVertexPublisher
    {
        ALMetalDevice* previous;
        explicit MissingVertexPublisher(ALMetalDevice* device) : previous(device)
        { LLVertexBuffer::setMetalDevice(nullptr); }
        ~MissingVertexPublisher() { LLVertexBuffer::setMetalDevice(previous); }
    };
    const auto read_store = [&native](const ALMetalBuffer* store)
    {
        REQUIRE(store);
        std::vector<U8> bytes(store->length());
        REQUIRE(store->read(0, bytes.data(), bytes.size(), &native.error));
        return bytes;
    };
    CHECK_FALSE(prepareImagePreviewAvatarBuffer(nullptr));
    LLPointer<LLVertexBuffer> buffer = new LLVertexBuffer(
        LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_NORMAL);
    REQUIRE(buffer->allocateBuffer(3, 3));
    REQUIRE(buffer->getMetalVertexBuffer());
    REQUIRE(buffer->getMetalIndexBuffer());
    CHECK_FALSE(prepareImagePreviewAvatarBuffer(buffer));
    std::memset(buffer->getMappedData(), 0, buffer->getSize());
    std::memset(buffer->getMappedIndices(), 0, buffer->getIndicesSize());
    LLStrider<LLVector3> positions, normals;
    REQUIRE(buffer->getVertexStrider(positions));
    positions[0].set(-1.f, -1.f, .5f);
    positions[1].set(3.f, -1.f, .5f);
    positions[2].set(-1.f, 3.f, .5f);
    REQUIRE(buffer->getNormalStrider(normals));
    for (unsigned vertex = 0; vertex < 3; ++vertex)
        normals[vertex].set(0.f, 0.f, 1.f);
    LLStrider<U16> indices;
    REQUIRE(buffer->getIndexStrider(indices));
    indices[0] = 0; indices[1] = 1; indices[2] = 2;
    buffer->unmapBuffer();
    REQUIRE(prepareImagePreviewAvatarBuffer(buffer));
    LLImagePreviewAvatar preview(native);
    preview.geometry_input = buffer;
    REQUIRE(preview.renderWithoutTarget() ==
            LLViewerDynamicTexture::ETargetlessRenderResult::COMPLETED);
    LLImageGL* image = preview.getGLTexture();
    const auto first_pixels = pixels(native, *image->getMetalTexture());
    const auto first_identity = image->getMetalTexture()->nativeHandle();
    const auto first_image_stamp = image->getContentStamp();
    auto* previous_vertices = buffer->getMetalVertexBuffer();
    auto* previous_indices = buffer->getMetalIndexBuffer();
    bool vertex_edit = false;
    SECTION("a warm vertex generation stays pending until its retry publishes")
    { vertex_edit = true; }
    SECTION("a warm index generation stays pending until its retry publishes")
    {}

    std::vector<U8> expected_vertices, expected_indices;
    U64 pending_stamp = 0;
    {
        MissingVertexPublisher missing(native.device.get());
        if (vertex_edit)
        {
            REQUIRE(buffer->getVertexStrider(positions));
            positions[1].set(0.f, -1.f, .5f);
            positions[2].set(-1.f, 0.f, .5f);
        }
        else
        {
            REQUIRE(buffer->getIndexStrider(indices));
            indices[1] = 0; indices[2] = 0;
        }
        expected_vertices.assign(buffer->getMappedData(),
                                 buffer->getMappedData() + buffer->getSize());
        expected_indices.assign(buffer->getMappedIndices(),
                                buffer->getMappedIndices() + buffer->getIndicesSize());
        buffer->unmapBuffer();
        pending_stamp = buffer->getContentStamp();
        CHECK(buffer->getMetalVertexBuffer() == previous_vertices);
        CHECK(buffer->getMetalIndexBuffer() == previous_indices);
        CHECK(buffer->hasPublishedMetalVertexBuffer() == !vertex_edit);
        CHECK(buffer->hasPublishedMetalIndexBuffer() == vertex_edit);
        preview.mNeedsUpdate = true;
        CHECK(preview.renderWithoutTarget() ==
              LLViewerDynamicTexture::ETargetlessRenderResult::PENDING);
        CHECK(preview.mNeedsUpdate);
        CHECK(image->getMetalTexture()->nativeHandle() == first_identity);
        CHECK(image->getContentStamp() == first_image_stamp);
        CHECK(pixels(native, *image->getMetalTexture()) == first_pixels);
    }
    // No mesh regeneration, map or CPU write occurs after the failure.
    // The original setBuffer draw boundary retried precisely this state.
    REQUIRE(preview.renderWithoutTarget() ==
            LLViewerDynamicTexture::ETargetlessRenderResult::COMPLETED);
    CHECK_FALSE(preview.mNeedsUpdate);
    CHECK(buffer->hasPublishedMetalVertexBuffer());
    CHECK(buffer->hasPublishedMetalIndexBuffer());
    CHECK(buffer->getContentStamp() == pending_stamp);
    CHECK(read_store(buffer->getMetalVertexBuffer()) == expected_vertices);
    CHECK(read_store(buffer->getMetalIndexBuffer()) == expected_indices);
    CHECK(pixels(native, *image->getMetalTexture()) != first_pixels);
    CHECK((vertex_edit ? buffer->getMetalIndexBuffer() == previous_indices
                       : buffer->getMetalVertexBuffer() == previous_vertices));
}

TEST_CASE("Model preview admits only published stores after its final upload retry",
          "[almetal][preview-indexed-publication]")
{
    NativeImages native;
    REQUIRE_FALSE(LLVertexBuffer::isMetalBufferAuthority());
    REQUIRE(LLVertexBuffer::getBytesAllocated() == 0);
    REQUIRE(native.old_device == nullptr);
    LLVertexBuffer::setMetalDevice(native.device.get());
    LLVertexBuffer::setMetalBufferAuthority(true);
    LLVertexBuffer::initClass(nullptr);
    struct VertexBackendScope
    {
        ~VertexBackendScope()
        {
            LLVertexBuffer::flushBuffers();
            CHECK(LLVertexBuffer::getBytesAllocated() == 0);
            LLVertexBuffer::cleanupClass();
            LLVertexBuffer::setMetalBufferAuthority(false);
            LLVertexBuffer::setMetalDevice(nullptr);
        }
    } backend;
    struct MissingVertexPublisher
    {
        ALMetalDevice* previous;
        explicit MissingVertexPublisher(ALMetalDevice* device) : previous(device)
        { LLVertexBuffer::setMetalDevice(nullptr); }
        ~MissingVertexPublisher() { LLVertexBuffer::setMetalDevice(previous); }
    };
    enum class Failure { first_upload, first_allocation, vertices, indices, flushed };
    Failure failure = Failure::first_upload;
    SECTION("an allocated first store is not a published store") {}
    SECTION("a missing first native allocation retries without another write")
    { failure = Failure::first_allocation; }
    SECTION("a warm vertex generation cannot substitute for a dirty store")
    { failure = Failure::vertices; }
    SECTION("a warm index generation cannot substitute for a dirty store")
    { failure = Failure::indices; }
    SECTION("a successful earlier global flush needs no duplicate publication")
    { failure = Failure::flushed; }
    const bool first = failure == Failure::first_upload ||
                       failure == Failure::first_allocation;
    LLPointer<LLVertexBuffer> buffer = new LLVertexBuffer(
        LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_NORMAL);
    std::unique_ptr<MissingVertexPublisher> missing;
    if (failure == Failure::first_allocation)
        missing = std::make_unique<MissingVertexPublisher>(native.device.get());
    REQUIRE(buffer->allocateBuffer(3, 3));
    std::memset(buffer->getMappedData(), 0, buffer->getSize());
    std::memset(buffer->getMappedIndices(), 0, buffer->getIndicesSize());
    LLStrider<LLVector3> positions, normals;
    REQUIRE(buffer->getVertexStrider(positions));
    positions[0].set(-1.f, -1.f, .5f);
    positions[1].set(3.f, -1.f, .5f);
    positions[2].set(-1.f, 3.f, .5f);
    REQUIRE(buffer->getNormalStrider(normals));
    for (U32 vertex = 0; vertex < 3; ++vertex)
        normals[vertex].set(0.f, 0.f, 1.f);
    LLStrider<U16> indices;
    REQUIRE(buffer->getIndexStrider(indices));
    indices[0] = 0; indices[1] = 1; indices[2] = 2;
    if (failure == Failure::first_upload)
        missing = std::make_unique<MissingVertexPublisher>(native.device.get());
    buffer->unmapBuffer();
    std::vector<ALMetalLitPreviewBatch> batches;
    const auto append = [&]
    {
        return ModelPreviewInputs::appendMetalModelPreviewBatch(
            *buffer, nullptr, LLColor4(.25f, .5f, .75f, 1.f), false,
            LLMatrix4a::identity(), LLMatrix4a::identity(), batches);
    };
    ALMetalTextureDescriptor output;
    output.width = width; output.height = height;
    output.format = ALMetalPixelFormat::rgba8_unorm_srgb;
    const auto render = [&]
    {
        ALMetalLitPreviewScene scene;
        scene.batches = batches.data(); scene.batch_count = batches.size();
        scene.destination_descriptor = &output;
        scene.linear_storage_view = true;
        auto texture = native.renderer->render(scene, &native.error);
        INFO(native.error); REQUIRE(texture);
        return texture;
    };
    std::unique_ptr<ALMetalTexture> previous_output;
    Pixels previous_pixels{};
    ALMetalBuffer* old_vertices = buffer->getMetalVertexBuffer();
    ALMetalBuffer* old_indices = buffer->getMetalIndexBuffer();
    if (!first)
    {
        REQUIRE(append()); REQUIRE(batches.size() == 1);
        previous_output = render();
        previous_pixels = pixels(native, *previous_output);
        checkColor(previous_pixels, {64, 128, 191, 255});
        batches.clear();
        missing = std::make_unique<MissingVertexPublisher>(native.device.get());
        if (failure == Failure::indices)
        {
            REQUIRE(buffer->getIndexStrider(indices));
            indices[1] = 0; indices[2] = 0;
        }
        else
        {
            REQUIRE(buffer->getVertexStrider(positions));
            positions[1].set(0.f, -1.f, .5f);
            positions[2].set(-1.f, 0.f, .5f);
        }
        buffer->unmapBuffer();
        CHECK(buffer->getMetalVertexBuffer() == old_vertices);
        CHECK(buffer->getMetalIndexBuffer() == old_indices);
    }
    const std::vector<U8> expected_vertices(buffer->getMappedData(),
        buffer->getMappedData() + buffer->getSize());
    const std::vector<U8> expected_indices(buffer->getMappedIndices(),
        buffer->getMappedIndices() + buffer->getIndicesSize());
    const U64 pending_stamp = buffer->getContentStamp();
    // Even repeated ordinary flushes cannot publish while the publisher is
    // unavailable. The final producer boundary must not admit old handles.
    LLVertexBuffer::flushBuffers(); LLVertexBuffer::flushBuffers();
    CHECK_FALSE(append()); CHECK(batches.empty()); batches.clear();
    if (previous_output)
        CHECK(pixels(native, *previous_output) == previous_pixels);
    missing.reset();
    if (failure == Failure::flushed)
    {
        LLVertexBuffer::flushBuffers();
        REQUIRE(buffer->hasPublishedMetalVertexBuffer());
        REQUIRE(buffer->hasPublishedMetalIndexBuffer());
    }
    auto* before_append_vertices = buffer->getMetalVertexBuffer();
    auto* before_append_indices = buffer->getMetalIndexBuffer();
    REQUIRE(append()); REQUIRE(batches.size() == 1);
    CHECK(buffer->hasPublishedMetalVertexBuffer());
    CHECK(buffer->hasPublishedMetalIndexBuffer());
    if (!buffer->hasPublishedMetalVertexBuffer() ||
        !buffer->hasPublishedMetalIndexBuffer())
        return; // Baseline must not send uninitialized indices to the GPU.
    CHECK(buffer->getContentStamp() == pending_stamp);
    const auto read_store = [&native](const ALMetalBuffer* store)
    {
        REQUIRE(store);
        std::vector<U8> bytes(store->length());
        REQUIRE(store->read(0, bytes.data(), bytes.size(), &native.error));
        return bytes;
    };
    CHECK(read_store(buffer->getMetalVertexBuffer()) == expected_vertices);
    CHECK(read_store(buffer->getMetalIndexBuffer()) == expected_indices);
    auto current_output = render();
    const auto current_pixels = pixels(native, *current_output);
    if (first)
        checkColor(current_pixels, {64, 128, 191, 255});
    else
        CHECK(current_pixels != previous_pixels);
    if (failure == Failure::flushed)
    {
        CHECK(buffer->getMetalVertexBuffer() == before_append_vertices);
        CHECK(buffer->getMetalIndexBuffer() == before_append_indices);
    }
    else if (!first)
        CHECK((failure == Failure::indices
            ? buffer->getMetalVertexBuffer() == old_vertices
            : buffer->getMetalIndexBuffer() == old_indices));
}

TEMPLATE_TEST_CASE("Image preview callers recover a missing initial native destination",
                   "[almetal][preview-publication]", LLImagePreviewAvatar,
                   LLImagePreviewSculpted)
{
    NativeImages native;
    std::unique_ptr<TestType> preview;
    {
        MissingImageBackend unavailable;
        preview = std::make_unique<TestType>(native);
        REQUIRE(preview->getGLTexture());
        CHECK_FALSE(preview->getGLTexture()->getMetalTexture());
        CHECK_FALSE(preview->getGLTexture()->getHasGLTexture());
        CHECK(preview->renderWithoutTarget() ==
              LLViewerDynamicTexture::ETargetlessRenderResult::PENDING);
        CHECK(preview->mNeedsUpdate);
        CHECK(preview->scene_calls == 0);
    }
    const auto camera_before = camera_input;
    REQUIRE(preview->renderWithoutTarget() ==
            LLViewerDynamicTexture::ETargetlessRenderResult::COMPLETED);
    CHECK_FALSE(preview->mNeedsUpdate);
    CHECK(preview->scene_calls == 1);
    CHECK(camera_input == camera_before);
    CHECK_FALSE(preview->mMetalPreview);
    CHECK_FALSE(preview->mMetalPreviewDescriptor);
    LLImageGL* image = preview->getGLTexture();
    REQUIRE(image->getMetalTexture());
    CHECK_FALSE(image->getHasGLTexture());
    CHECK(image->getCurrentWidth() == width);
    CHECK(image->getCurrentHeight() == height);
    CHECK(image->getComponents() == 3);
    checkColor(pixels(native, *image->getMetalTexture()), {64, 128, 191, 255});
}

TEMPLATE_TEST_CASE("Image preview callers retain published pixels across failed updates",
                   "[almetal][preview-publication]", LLImagePreviewAvatar,
                   LLImagePreviewSculpted)
{
    NativeImages native;
    TestType preview(native);
    REQUIRE(preview.renderWithoutTarget() ==
            LLViewerDynamicTexture::ETargetlessRenderResult::COMPLETED);
    LLImageGL* const image = preview.getGLTexture();
    REQUIRE(image->getMetalTexture());
    void* const first_identity = image->getMetalTexture()->nativeHandle();
    const auto first_pixels = pixels(native, *image->getMetalTexture());
    checkColor(first_pixels, {64, 128, 191, 255});
    const U64 first_accounting = LLImageGL::getTextureBytesAllocated();
    const auto first_stamp = image->getContentStamp();

    // This actual command encodes the old published source but is committed
    // only after successful replacement has destroyed its C++ texture owner.
    auto copy_descriptor = image->getMetalTexture()->descriptor();
    auto old_copy = ALMetalTexture::create(*native.device, copy_descriptor, &native.error);
    INFO(native.error); REQUIRE(old_copy);
    auto command = native.device->makeCommandBuffer("Preview prior image lease", &native.error);
    REQUIRE(command);
    REQUIRE(old_copy->copyFrom(*command, *image->getMetalTexture(), {}, &native.error));

    for (const auto rejected : {SceneInput::render_rejected, SceneInput::publication_rejected})
    {
        preview.input = rejected;
        preview.mNeedsUpdate = true;
        const auto camera_before = camera_input;
        CHECK(preview.renderWithoutTarget() ==
              LLViewerDynamicTexture::ETargetlessRenderResult::PENDING);
        CHECK(preview.mNeedsUpdate);
        CHECK(camera_input == camera_before);
        CHECK_FALSE(preview.mMetalPreview);
        CHECK_FALSE(preview.mMetalPreviewDescriptor);
        CHECK(preview.getGLTexture() == image);
        REQUIRE(image->getMetalTexture());
        CHECK(image->getMetalTexture()->nativeHandle() == first_identity);
        CHECK(image->getContentStamp() == first_stamp);
        CHECK(LLImageGL::getTextureBytesAllocated() == first_accounting);
        CHECK(pixels(native, *image->getMetalTexture()) == first_pixels);
    }

    preview.input = SceneInput::complete;
    preview.color = {1.f, 0.f, .5f, 1.f};
    REQUIRE(preview.renderWithoutTarget() ==
            LLViewerDynamicTexture::ETargetlessRenderResult::COMPLETED);
    CHECK_FALSE(preview.mNeedsUpdate);
    CHECK(preview.getGLTexture() == image);
    REQUIRE(image->getMetalTexture());
    CHECK(image->getMetalTexture()->nativeHandle() != first_identity);
    CHECK(image->getContentStamp() != first_stamp);
    CHECK(LLImageGL::getTextureBytesAllocated() == first_accounting);
    checkColor(pixels(native, *image->getMetalTexture()), {255, 0, 128, 255});
    REQUIRE(command->commitAndWait(&native.error));
    CHECK(pixels(native, *old_copy) == first_pixels);
}

namespace
{
struct ScopedPreviewInputs
{
    std::array<S32, 4> old_array = camera_input;
    LLCamera old_camera = *LLViewerCamera::getInstance();
    LLCamera old_current = LLViewerCamera::getCurrent();
    std::array<S32, 4> old_viewport{gGLViewport[0], gGLViewport[1], gGLViewport[2], gGLViewport[3]};
    ~ScopedPreviewInputs()
    {
        camera_input = old_array;
        static_cast<LLCamera&>(*LLViewerCamera::getInstance()) = old_camera;
        LLViewerCamera::setCurrent(old_current);
        std::copy(old_viewport.begin(), old_viewport.end(), gGLViewport);
    }
};
template<class Preview> bool requested(const Preview& preview)
{
    if constexpr (std::is_same_v<Preview, LLGLTFPreviewTexture>) return preview.mShouldRender;
    else return preview.mNeedsUpdate;
}
template<class Preview> void request(Preview& preview)
{
    if constexpr (std::is_same_v<Preview, LLGLTFPreviewTexture>) preview.mShouldRender = true;
    else preview.mNeedsUpdate = true;
}
}

TEST_CASE("Native dynamic backing retry preserves healthy image identity and pixels",
          "[almetal][dynamic-destination]")
{
    NativeImages native;
    std::unique_ptr<LLViewerDynamicTexture> image;
    {
        MissingImageBackend unavailable;
        image = std::make_unique<LLViewerDynamicTexture>();
        CHECK(image->getGLTexture()->getDiscardLevel() < 0);
        CHECK_FALSE(image->ensureMetalTexture());
        CHECK_FALSE(image->ensureMetalTexture());
        CHECK_FALSE(image->getGLTexture()->getMetalTexture());
    }
    REQUIRE(image->ensureMetalTexture());
    auto* const owner = image->getGLTexture();
    const auto* const backing = owner->getMetalTexture();
    const auto identity = backing->nativeHandle();
    const auto stamp = owner->getContentStamp();
    const auto contents = pixels(native, *backing);
    REQUIRE(image->ensureMetalTexture());
    CHECK(owner == image->getGLTexture());
    CHECK(owner->getMetalTexture()->nativeHandle() == identity);
    CHECK(owner->getContentStamp() == stamp);
    CHECK(pixels(native, *owner->getMetalTexture()) == contents);
    {
        MissingImageBackend unavailable;
        REQUIRE(image->ensureMetalTexture());
        CHECK(owner->getMetalTexture()->nativeHandle() == identity);
        CHECK(owner->getContentStamp() == stamp);
    }
}

TEST_CASE("Dynamic reset performs its CPU duties without image backing",
          "[dynamic-destination-policy]")
{
    LLVOAvatar avatar;
    struct RestoreInputs
    {
        LLVOAvatar* previous_avatar = gAgentAvatarp;
        bool previous_dirty = LLVisualParamReset::sDirty;
        ~RestoreInputs()
        { gAgentAvatarp = previous_avatar; LLVisualParamReset::sDirty = previous_dirty; }
    } restore;
    gAgentAvatarp = &avatar;
    MissingImageBackend unavailable;
    const auto submissions = ALMetalDevice::committedCommandBufferCount();
    LLVisualParamReset reset;
    CHECK_FALSE(reset.getGLTexture()->getMetalTexture());
    LLVisualParamReset::sDirty = true;
    CHECK(reset.renderWithoutTarget() == LLViewerDynamicTexture::ETargetlessRenderResult::COMPLETED);
    CHECK_FALSE(LLVisualParamReset::sDirty);
    CHECK(avatar.composite_updates == 1); CHECK(avatar.visual_updates == 1);
    CHECK(avatar.geometry_updates == 1);
    CHECK(reset.renderWithoutTarget() == LLViewerDynamicTexture::ETargetlessRenderResult::COMPLETED);
    CHECK(avatar.composite_updates == 1); CHECK(avatar.visual_updates == 1);
    CHECK(avatar.geometry_updates == 1);
    CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);
    CHECK_FALSE(reset.getGLTexture()->getMetalTexture());
}

TEMPLATE_TEST_CASE("Retained preview owners recover the same failed initial destination",
                   "[almetal][dynamic-destination]", LLModelPreview,
                   LLPreviewAnimation, LLVisualParamHint, LLGLTFPreviewTexture)
{
    NativeImages native; ScopedPreviewInputs camera_scope;
    std::unique_ptr<TestType> preview;
    {
        MissingImageBackend unavailable;
        preview = std::make_unique<TestType>(native);
        REQUIRE(preview->getGLTexture());
        CHECK_FALSE(preview->getGLTexture()->getMetalTexture());
        CHECK(preview->renderWithoutTarget() == LLViewerDynamicTexture::ETargetlessRenderResult::PENDING);
        CHECK(preview->renderWithoutTarget() == LLViewerDynamicTexture::ETargetlessRenderResult::PENDING);
        CHECK(requested(*preview));
        CHECK(preview->scene_calls == 0);
    }
    const auto old_origin = LLViewerCamera::getInstance()->getOrigin();
    const auto old_current_origin = LLViewerCamera::getCurrent().getOrigin();
    const auto old_viewport = std::array<S32, 4>{gGLViewport[0],gGLViewport[1],gGLViewport[2],gGLViewport[3]};
    REQUIRE(preview->renderWithoutTarget() == LLViewerDynamicTexture::ETargetlessRenderResult::COMPLETED);
    CHECK_FALSE(requested(*preview)); CHECK(preview->scene_calls == 1);
    REQUIRE(preview->getGLTexture()->getMetalTexture());
    CHECK_FALSE(preview->getGLTexture()->getHasGLTexture());
    CHECK(preview->getGLTexture()->getDiscardLevel() == 0);
    checkColor(pixels(native, *preview->getGLTexture()->getMetalTexture()), {64,128,191,255});
    if constexpr (std::is_same_v<TestType, LLVisualParamHint>)
    {
        CHECK_FALSE(preview->clear_depth_input); CHECK(preview->mIsVisible);
        CHECK(LLViewerCamera::getInstance()->getOrigin() == old_origin);
        CHECK(LLViewerCamera::getCurrent().getOrigin() == old_current_origin);
        CHECK(std::equal(old_viewport.begin(), old_viewport.end(), gGLViewport));
    }
    if constexpr (std::is_same_v<TestType, LLGLTFPreviewTexture>)
        CHECK(preview->mBestLoad == preview->mPendingLoad);
}

TEMPLATE_TEST_CASE("Retained preview retries never regenerate a published destination",
                   "[almetal][dynamic-destination]", LLModelPreview,
                   LLPreviewAnimation, LLVisualParamHint, LLGLTFPreviewTexture)
{
    NativeImages native; ScopedPreviewInputs camera_scope;
    TestType preview(native);
    REQUIRE(preview.renderWithoutTarget() == LLViewerDynamicTexture::ETargetlessRenderResult::COMPLETED);
    auto* const image = preview.getGLTexture();
    const auto identity = image->getMetalTexture()->nativeHandle();
    const auto stamp = image->getContentStamp();
    const auto previous_pixels = pixels(native, *image->getMetalTexture());
    for (auto failure : {SceneInput::render_rejected, SceneInput::publication_rejected})
    {
        preview.input = failure; request(preview);
        CHECK(preview.renderWithoutTarget() == LLViewerDynamicTexture::ETargetlessRenderResult::PENDING);
        CHECK(requested(preview)); CHECK(image == preview.getGLTexture());
        REQUIRE(image->getMetalTexture());
        CHECK(image->getMetalTexture()->nativeHandle() == identity);
        CHECK(image->getContentStamp() == stamp);
        CHECK(pixels(native, *image->getMetalTexture()) == previous_pixels);
    }
    preview.input = SceneInput::complete; preview.color = {1.f,0.f,.5f,1.f}; request(preview);
    REQUIRE(preview.renderWithoutTarget() == LLViewerDynamicTexture::ETargetlessRenderResult::COMPLETED);
    CHECK_FALSE(requested(preview)); CHECK(preview.getGLTexture() == image);
    CHECK(image->getContentStamp() != stamp);
    checkColor(pixels(native, *image->getMetalTexture()), {255,0,128,255});
}

TEST_CASE("Native composites bootstrap early metadata but can adopt without old backing",
          "[almetal][dynamic-destination][composite-destination]")
{
    NativeComposite native;
    LLGLDisable blend(GL_BLEND), cull(GL_CULL_FACE), scissor(GL_SCISSOR_TEST);
    LLGLDepthTest depth(GL_FALSE, GL_FALSE); LLGLSColorMask color_mask(true, true);
    std::unique_ptr<LLViewerTexLayerSetBuffer> composite;
    bool early = true;
    SECTION("creation failed before metadata initialization") {}
    SECTION("valid metadata needs no preliminary backing allocation") { early = false; }
    {
        MissingImageBackend unavailable;
        composite = std::make_unique<LLViewerTexLayerSetBuffer>();
        composite->white_input = native.white->getMetalTexture();
        CHECK(composite->getGLTexture()->getDiscardLevel() < 0);
        if (early)
        {
            CHECK(composite->renderWithoutTarget() == LLViewerDynamicTexture::ETargetlessRenderResult::PENDING);
            CHECK(composite->mNeedsUpdate); CHECK(composite->update_calls == 0);
            CHECK(composite->projection_balance == 0);
        }
    }
    if (!early)
    {
        LLPointer<LLImageRaw> raw = new LLImageRaw(width,height,4); raw->clear();
        REQUIRE(composite->setImageMetadata(0,raw));
        CHECK(composite->getGLTexture()->getDiscardLevel() == 0);
        CHECK_FALSE(composite->getGLTexture()->getMetalTexture());
    }
    REQUIRE(composite->renderWithoutTarget() == LLViewerDynamicTexture::ETargetlessRenderResult::COMPLETED);
    CHECK(composite->scene_saw_backing == early);
    CHECK_FALSE(composite->mNeedsUpdate); CHECK(composite->update_calls == 1);
    CHECK(composite->projection_balance == 0);
    REQUIRE(composite->getGLTexture()->getMetalTexture());
    checkColor(pixels(native,*composite->getGLTexture()->getMetalTexture()), {64,128,191,255});
    const auto identity = composite->getGLTexture()->getMetalTexture()->nativeHandle();
    const auto stamp = composite->getGLTexture()->getContentStamp();
    for (bool reject_scene : {true, false})
    {
        composite->reject_scene = reject_scene; composite->mNeedsUpdate = true;
        if (reject_scene)
        {
            CHECK(composite->renderWithoutTarget() == LLViewerDynamicTexture::ETargetlessRenderResult::PENDING);
        }
        else
        {
            // Complete real native capture, then reject actual image adoption
            // because its publisher is unavailable. Existing pixels survive.
            MissingImageBackend unavailable;
            CHECK(composite->renderWithoutTarget() == LLViewerDynamicTexture::ETargetlessRenderResult::PENDING);
        }
        CHECK(composite->mNeedsUpdate); CHECK(composite->update_calls == 1);
        CHECK(composite->projection_balance == 0);
        CHECK(composite->getGLTexture()->getContentStamp() == stamp);
        CHECK(composite->getGLTexture()->getMetalTexture()->nativeHandle() == identity);
        checkColor(pixels(native,*composite->getGLTexture()->getMetalTexture()), {64,128,191,255});
    }
}
