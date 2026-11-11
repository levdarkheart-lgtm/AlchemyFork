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
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalresource.h"
#include "../../almetal/almetalsculptpreview.h"
#include "llcoord.h"
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
struct PreviewScene : LLViewerDynamicTexture
{
    NativeImages& native;
    SceneInput input = SceneInput::complete;
    std::array<float, 4> color{.25f, .5f, .75f, 1.f};
    bool mNeedsUpdate = true;
    unsigned scene_calls = 0;
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
        mMetalPreviewDescriptor = std::make_unique<ALMetalTextureDescriptor>(
            mGLTexturep->getMetalTexture()->descriptor());
        if (input == SceneInput::publication_rejected)
            ++mMetalPreviewDescriptor->width;
        ALMetalLitPreviewBatch batch;
        batch.geometry = ALMetalLitPreviewGeometry::unindexed_flat_triangles;
        batch.vertices = native.vertices.get();
        batch.vertex_count = 3;
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
