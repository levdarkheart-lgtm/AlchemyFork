/**
 * @file alshadow_height_publication_metal_test.mm
 * @brief Height publication precedes consumption by an already-created command.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llgl.h"
#include "../llimagegl.h"
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalresource.h"

#import <Metal/Metal.h>

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <memory>

namespace
{
constexpr S32 side = 4;
constexpr std::size_t row_bytes = 256;
constexpr std::size_t plane_bytes = side * row_bytes;
using Heights = std::array<F32, side * side>;
constexpr Heights original_heights{{
    -4.f, -3.f, -2.f, -1.f,
     0.f,  1.f,  2.f,  3.f,
     4.f,  5.f,  6.f,  7.f,
     8.f,  9.f, 10.f, 11.f,
}};

struct TextureFixture
{
    std::string error;
    std::unique_ptr<ALMetalDevice> device;
    ALRenderCapabilities previous_capabilities = gRenderCapabilities;
    ALMetalDevice* previous_device = LLImageGL::getMetalDevice();
    bool previous_authority = LLImageGL::isMetalTextureAuthority();

    TextureFixture()
    {
        REQUIRE_FALSE(gGLManager.mInited);
        device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);
        gRenderCapabilities.initializeMetal(device->info());
        LLImageGL::setMetalDevice(device.get());
        LLImageGL::setMetalTextureAuthority(true);
    }

    ~TextureFixture()
    {
        LLImageGL::setMetalDevice(previous_device);
        LLImageGL::setMetalTextureAuthority(previous_authority);
        gRenderCapabilities = previous_capabilities;
    }

    LLPointer<LLImageGL> heightImage()
    {
        LLPointer<LLImageGL> image = new LLImageGL(side, side, 1, false);
        image->setNeedsAlphaAndPickMask(false);
        image->setExplicitFormat(GL_R32F, GL_RED, GL_FLOAT);
        REQUIRE(image->createMetalTexture(
            0, reinterpret_cast<const U8*>(original_heights.data())));
        REQUIRE(image->getMetalTexture());
        CHECK(image->getMetalTexture()->descriptor().format ==
              ALMetalPixelFormat::r32_float);
        CHECK_FALSE(image->getHasGLTexture());
        CHECK(image->getTexName() == 0);
        return image;
    }

    std::shared_ptr<ALMetalBuffer> readbackBuffer()
    {
        ALMetalBufferDescriptor descriptor;
        descriptor.length = 2 * plane_bytes;
        descriptor.storage = ALMetalStorageMode::shared;
        descriptor.label = "Terrain height generations readback";
        std::shared_ptr<ALMetalBuffer> buffer(
            ALMetalBuffer::create(*device, descriptor, &error));
        INFO(error);
        REQUIRE(buffer);
        return buffer;
    }

    Heights readPlane(const ALMetalBuffer& buffer, std::size_t offset)
    {
        Heights result{};
        for (S32 y = 0; y < side; ++y)
        {
            REQUIRE(buffer.read(offset + y * row_bytes,
                result.data() + y * side, side * sizeof(F32), &error));
        }
        return result;
    }
};

void encodeHeightReadback(ALMetalCommandBuffer& command,
                          const ALMetalTexture& texture,
                          const ALMetalBuffer& buffer,
                          std::size_t offset)
{
    id<MTLCommandBuffer> native =
        (__bridge id<MTLCommandBuffer>)command.nativeHandle();
    id<MTLTexture> source = (__bridge id<MTLTexture>)texture.nativeHandle();
    id<MTLBuffer> destination = (__bridge id<MTLBuffer>)buffer.nativeHandle();
    REQUIRE(native);
    REQUIRE(source);
    REQUIRE(destination);
    id<MTLBlitCommandEncoder> blit = [native blitCommandEncoder];
    REQUIRE(blit);
    [blit copyFromTexture:source
             sourceSlice:0
             sourceLevel:0
            sourceOrigin:MTLOriginMake(0, 0, 0)
              sourceSize:MTLSizeMake(side, side, 1)
                toBuffer:destination
       destinationOffset:offset
  destinationBytesPerRow:row_bytes
destinationBytesPerImage:plane_bytes
                 options:MTLBlitOptionNone];
    [blit endEncoding];
}
}

TEST_CASE("Terrain height upload precedes a command created before the dirty update",
          "[almetal][terrain-shadow-height-publication][texture-cow][queue-order]")
{
    @autoreleasepool
    {
        TextureFixture f;
        auto image = f.heightImage();
        auto old_height = image->getMetalTextureLease();
        REQUIRE(old_height);
        auto readback = f.readbackBuffer();

        // Shadow capture opens its command before a terrain patch supplies
        // inputs. Command creation does not put it ahead of a later upload's
        // commit. Do not insert a readback/wait between these operations.
        auto command = f.device->makeCommandBuffer(
            "Pre-created shadow height consumer", &f.error);
        INFO(f.error);
        REQUIRE(command);
        CHECK_FALSE(command->isCommitted());

        auto updated = original_heights;
        updated[5] = 101.25f;
        updated[6] = 102.5f;
        updated[9] = 103.75f;
        updated[10] = 104.f;
        const auto before = LLImageGL::metalPublicationStats();
        REQUIRE(image->uploadMetalSubImage(
            reinterpret_cast<const U8*>(updated.data()),
            side, side, 1, 1, 2, 2));
        const auto after = LLImageGL::metalPublicationStats();
        CHECK(after.cow_replacements == before.cow_replacements + 1);
        CHECK(after.immediate_upload_commits == before.immediate_upload_commits + 1);
        CHECK_FALSE(command->isCommitted());

        auto current_height = image->getMetalTextureLease();
        REQUIRE(current_height);
        CHECK(current_height.get() != old_height.get());
        CHECK(image->getMetalTexture() == current_height.get());
        const auto* current_identity = current_height.get();
        const auto* old_identity = old_height.get();
        std::weak_ptr<const ALMetalTexture> queued_current = current_height;
        std::weak_ptr<const ALMetalTexture> queued_old = old_height;
        REQUIRE(command->retainUntilCompletion(current_height, &f.error));
        REQUIRE(command->retainUntilCompletion(old_height, &f.error));
        REQUIRE(command->retainUntilCompletion(readback, &f.error));
        encodeHeightReadback(*command, *current_height, *readback, 0);
        encodeHeightReadback(*command, *old_height, *readback, plane_bytes);

        // Retire the image and explicit CPU holders. The consumer's completion
        // leases must cover both generations through its own submission, as
        // an independently retained shadow packet does.
        image = nullptr;
        current_height.reset();
        old_height.reset();
        REQUIRE_FALSE(queued_current.expired());
        REQUIRE_FALSE(queued_old.expired());
        CHECK(queued_current.lock().get() == current_identity);
        CHECK(queued_old.lock().get() == old_identity);

        auto completion = command->completionToken();
        REQUIRE(completion);
        INFO(f.error);
        REQUIRE(command->commitAndWait(&f.error));
        CHECK(completion->isComplete());
        CHECK(completion->status() == ALMetalCommandStatus::completed);
        CHECK(f.readPlane(*readback, 0) == updated);
        CHECK(f.readPlane(*readback, plane_bytes) == original_heights);
        CHECK_FALSE(gGLManager.mInited);
        // Completion callbacks may already have retired the leases. Their
        // release timing after completion is not part of this ordering test.
    }
}

TEST_CASE("Rejected terrain height subimage leaves the current publication unchanged",
          "[almetal][terrain-shadow-height-publication][rejected-subimage]")
{
    @autoreleasepool
    {
        TextureFixture f;
        auto image = f.heightImage();
        auto height = image->getMetalTextureLease();
        REQUIRE(height);
        const auto published = image->nativePublication().publishedRevision();
        const auto mutation = image->nativePublication().mutationRevision();
        const auto content = image->getContentStamp();
        const auto before = LLImageGL::metalPublicationStats();

        // Valid backing data with a positive rectangle that extends one texel
        // beyond the image exercises the API's ordinary bounds rejection.
        CHECK_FALSE(image->uploadMetalSubImage(
            reinterpret_cast<const U8*>(original_heights.data()),
            side, side, side - 1, 0, 2, 1));
        CHECK(image->getMetalTexture() == height.get());
        CHECK(image->getMetalTextureLease() == height);
        CHECK(image->nativePublication().publishedRevision() == published);
        CHECK(image->nativePublication().mutationRevision() == mutation);
        CHECK(image->getContentStamp() == content);
        const auto after = LLImageGL::metalPublicationStats();
        CHECK(after.cow_replacements == before.cow_replacements);
        CHECK(after.immediate_upload_commits == before.immediate_upload_commits);

        auto readback = f.readbackBuffer();
        auto command = f.device->makeCommandBuffer(
            "Rejected shadow height publication control", &f.error);
        REQUIRE(command);
        REQUIRE(command->retainUntilCompletion(height, &f.error));
        REQUIRE(command->retainUntilCompletion(readback, &f.error));
        encodeHeightReadback(*command, *height, *readback, 0);
        INFO(f.error);
        REQUIRE(command->commitAndWait(&f.error));
        CHECK(f.readPlane(*readback, 0) == original_heights);
        CHECK_FALSE(gGLManager.mInited);
    }
}
