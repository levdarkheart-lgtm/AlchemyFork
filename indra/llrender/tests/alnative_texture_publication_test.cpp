/**
 * @file alnative_texture_publication_test.cpp
 * @brief Visible native texture tuples survive staged resize/cancel and withdrawal.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llgl.h"
#include "../llimagegl.h"
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalresource.h"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <vector>

namespace
{
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
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
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
};
struct TupleObserver final : ALNativePublication::Observer
{
    LLImageGL* image = nullptr;
    unsigned publications = 0, withdrawals = 0;
    bool coherent = true;
    void nativePublicationChanged(ALNativePublication::Phase phase) noexcept override
    {
        coherent = coherent && on_main_thread();
        if (phase == ALNativePublication::Phase::published)
        {
            ++publications;
            const auto* native = image->getMetalTexture();
            coherent = coherent && native && image->getWidth() == native->width() &&
                image->getHeight() == native->height() && image->getComponents() == 1 &&
                image->getPrimaryFormat() == GL_RED && image->getFormatType() == GL_FLOAT &&
                native->descriptor().format == ALMetalPixelFormat::r32_float;
        }
        else if (phase == ALNativePublication::Phase::withdrawn)
        {
            ++withdrawals;
            coherent = coherent && !image->getMetalTexture();
        }
    }
};
}

TEST_CASE("Native resize publishes a complete typed tuple while cancelled resize keeps the prior generation",
          "[almetal][native-publication][texture-tuple]")
{
    TextureFixture f;
    std::vector<U8> rgba(4 * 4 * 4, 255);
    LLPointer<LLImageGL> image = new LLImageGL(4, 4, 4, false);
    image->setNeedsAlphaAndPickMask(false);
    image->setExplicitFormat(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    REQUIRE(image->createMetalTexture(0, rgba.data()));
    auto old = image->getMetalTextureLease(); REQUIRE(old);
    const auto observer = std::make_shared<TupleObserver>();
    observer->image = image;
    image->nativePublication().observe(observer);
    const auto initial = image->nativePublication().publishedRevision();
    const auto initial_mutation = image->nativePublication().mutationRevision();

    image->beginUpload();
    REQUIRE(image->setSize(8, 8, 1));
    image->setExplicitFormat(GL_R32F, GL_RED, GL_FLOAT);
    std::vector<float> heights(8 * 8, 0.75f);
    REQUIRE(image->createMetalTexture(0, reinterpret_cast<const U8*>(heights.data())));
    CHECK(image->getMetalTextureLease() == old);
    CHECK(image->getWidth() == 4);
    CHECK(image->getComponents() == 4);
    CHECK(image->getPrimaryFormat() == GL_RGBA);
    CHECK(image->nativePublication().publishedRevision() == initial);
    CHECK(image->nativePublication().mutationRevision() == initial_mutation);
    image->publishMetalTextureAuthoritative();
    CHECK(observer->publications == 1);
    CHECK(observer->coherent);
    auto published = image->getMetalTextureLease(); REQUIRE(published);
    CHECK(published != old);
    std::vector<float> readback(heights.size());
    REQUIRE(published->readback(*f.device, 0, 0, {}, readback.data(), 8 * sizeof(float), &f.error));
    CHECK(readback == heights);

    const auto visible = image->nativePublication().publishedRevision();
    image->beginUpload();
    REQUIRE(image->setSize(2, 2, 4));
    image->setExplicitFormat(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    std::vector<U8> cancelled(2 * 2 * 4, 0);
    REQUIRE(image->createMetalTexture(0, cancelled.data()));
    image->cancelUpload();
    CHECK(image->getMetalTextureLease() == published);
    CHECK(image->getWidth() == 8);
    CHECK(image->getComponents() == 1);
    CHECK(image->getPrimaryFormat() == GL_RED);
    CHECK(image->getFormatType() == GL_FLOAT);
    CHECK(image->nativePublication().publishedRevision() == visible);
    CHECK(observer->publications == 1);

    image->discardMetalTexture();
    CHECK(observer->withdrawals == 1);
    CHECK(observer->coherent);
    CHECK(image->nativePublication().publishedRevision() > visible);
    std::vector<U8> retained(rgba.size());
    REQUIRE(old->readback(*f.device, 0, 0, {}, retained.data(), 4 * 4, &f.error));
    CHECK(retained == rgba);
}
