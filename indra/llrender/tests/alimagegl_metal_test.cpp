/**
 * @file alimagegl_metal_test.cpp
 * @brief Native image readback follows the published upload generation.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llimagegl.h"
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstring>
#include <vector>

namespace
{
struct NativeImages
{
    ALRenderCapabilities previous_capabilities = gRenderCapabilities;
    ALMetalDevice* previous_device = LLImageGL::getMetalDevice();
    bool previous_authority = LLImageGL::isMetalTextureAuthority();
    std::unique_ptr<ALMetalDevice> device;
    std::string error;
    NativeImages()
    {
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        gRenderCapabilities.initializeMetal(device->info());
        LLImageGL::setMetalDevice(device.get());
        LLImageGL::setMetalTextureAuthority(true);
    }
    ~NativeImages()
    {
        LLImageGL::setMetalDevice(previous_device);
        LLImageGL::setMetalTextureAuthority(previous_authority);
        gRenderCapabilities = previous_capabilities;
    }
};

void checkReadback(LLImageGL& image, const std::vector<U8>& expected,
                   S32 width, S32 height, bool compressed = false)
{
    LLPointer<LLImageRaw> raw = new LLImageRaw;
    REQUIRE(image.readBackRaw(-1, raw, compressed));
    CHECK(raw->getWidth() == width);
    CHECK(raw->getHeight() == height);
    CHECK(raw->getComponents() == 4);
    REQUIRE(raw->getDataSize() == expected.size());
    CHECK(std::memcmp(raw->getData(), expected.data(), expected.size()) == 0);
}
}

TEST_CASE("Native image byte readback retains the published format during a typed replacement",
          "[alimagegl][almetal][publication]")
{
    NativeImages f;
    const U64 baseline = LLImageGL::getTextureBytesAllocated();
    std::vector<U8> old_pixels(8 * 8 * 4);
    for (std::size_t i = 0; i < old_pixels.size(); ++i)
        old_pixels[i] = static_cast<U8>(i * 31 + 7);
    LLPointer<LLImageGL> image = new LLImageGL(8, 8, 4, false);
    image->setNeedsAlphaAndPickMask(false);
    image->setExplicitFormat(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    REQUIRE(image->createMetalTexture(0, old_pixels.data()));
    ALMetalTexture* const old_texture = image->getMetalTexture();
    REQUIRE(old_texture);
    checkReadback(*image, old_pixels, 8, 8);
    CHECK(LLImageGL::getTextureBytesAllocated() == baseline + old_pixels.size());

    const std::array<float, 16> replacement{
        .0f,.25f,.5f,1.f, 1.f,.5f,.25f,.0f,
        .5f,.0f,1.f,.25f, .25f,1.f,.0f,.5f};
    for (bool publish : {false, true})
    {
        image->beginUpload();
        REQUIRE(image->setSize(2, 2, 4, 0));
        image->setExplicitFormat(GL_RGBA32F, GL_RGBA, GL_FLOAT);
        REQUIRE(image->createMetalTexture(0,
            reinterpret_cast<const U8*>(replacement.data())));
        // A real new RGBA32F resource exists, but published geometry, type,
        // memory and byte readback still belong to the old RGBA8 resource.
        CHECK(image->getStorageInternalFormat() == GL_RGBA32F); // Producer view.
        CHECK(image->getFormatType() == GL_UNSIGNED_BYTE);
        CHECK(image->getMetalTexture() == old_texture);
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline + old_pixels.size());
        checkReadback(*image, old_pixels, 8, 8);
        if (!publish)
        {
            image->cancelUpload();
            CHECK(image->getStorageInternalFormat() == GL_RGBA8);
            CHECK(image->getFormatType() == GL_UNSIGNED_BYTE);
            CHECK(image->getMetalTexture() == old_texture);
            checkReadback(*image, old_pixels, 8, 8);
        }
        else image->publishMetalTextureAuthoritative();
    }
    CHECK(image->getMetalTexture() != old_texture);
    CHECK(image->getWidth() == 2);
    CHECK(image->getFormatType() == GL_FLOAT);
    CHECK(LLImageGL::getTextureBytesAllocated() == baseline + sizeof(replacement));
    std::array<float, 16> actual{};
    REQUIRE(image->getMetalTexture()->readback(*f.device, 0, 0, {}, actual.data(),
        2 * 4 * sizeof(float), &f.error));
    CHECK(actual == replacement);
    // LLImageRaw has one byte per component; a published float texture is
    // intentionally rejected rather than truncated or written out of bounds.
    LLPointer<LLImageRaw> byte_result = new LLImageRaw;
    CHECK_FALSE(image->readBackRaw(-1, byte_result, false));
    image = nullptr;
    CHECK(LLImageGL::getTextureBytesAllocated() == baseline);
}

TEST_CASE("Native compressed readback retains published block bytes and row pitch",
          "[alimagegl][almetal][publication][compressed]")
{
    NativeImages f;
    const U64 baseline = LLImageGL::getTextureBytesAllocated();
    const std::vector<U8> bc1{
        0x00,0xf8,0xe0,0x07,0,0,0,0, 0x1f,0,0,0xf8,0x55,0x55,0x55,0x55,
        0xff,0xff,0,0,0xaa,0xaa,0xaa,0xaa, 0xe0,0x07,0x1f,0,0xff,0xff,0xff,0xff};
    const std::vector<U8> bc3{0xff,0,0,0,0,0,0,0, 0x00,0xf8,0xe0,0x07,0,0,0,0};
    LLPointer<LLImageGL> image = new LLImageGL(8, 8, 4, false);
    image->setNeedsAlphaAndPickMask(false);
    image->setExplicitFormat(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
        GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_UNSIGNED_BYTE);
    REQUIRE(image->createMetalTexture(0, bc1.data()));
    ALMetalTexture* const old_texture = image->getMetalTexture();
    REQUIRE(old_texture);
    CHECK(old_texture->descriptor().format == ALMetalPixelFormat::bc1_rgba);
    checkReadback(*image, bc1, 8, 8, true);
    image->beginUpload();
    REQUIRE(image->setSize(4, 4, 4, 0));
    image->setExplicitFormat(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
        GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_UNSIGNED_BYTE);
    REQUIRE(image->createMetalTexture(0, bc3.data()));
    CHECK(image->getStorageInternalFormat() == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
    CHECK(image->getPrimaryFormat() == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
    CHECK(image->getMetalTexture() == old_texture);
    CHECK(LLImageGL::getTextureBytesAllocated() == baseline + bc1.size());
    checkReadback(*image, bc1, 8, 8, true);
    image->publishMetalTextureAuthoritative();
    CHECK(image->getPrimaryFormat() == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
    CHECK(image->getMetalTexture()->descriptor().format == ALMetalPixelFormat::bc3_rgba);
    checkReadback(*image, bc3, 4, 4, true);
    CHECK(LLImageGL::getTextureBytesAllocated() == baseline + bc3.size());
    image = nullptr;
    CHECK(LLImageGL::getTextureBytesAllocated() == baseline);
}
