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
#include "../llgltexture.h"
#include "../llimagegl.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalresource.h"
#include "../../almetal/almetalsculptpreview.h"
#include "llcoord.h"
#include "v4color.h"
#include "llmatrix4a.h"
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
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
    LLViewerDynamicTexture() : LLViewerTexture(width, height, 3, false)
    {
        generateMetalTexture();
    }
    void generateMetalTexture();
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

    explicit PreviewScene(NativeImages& resources) : native(resources) {}
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
