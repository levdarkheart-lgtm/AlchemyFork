/**
 * @file llimagegl_test.cpp
 * @brief Upstream alpha-answer and large-readback regressions on native Metal.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llgl.h"
#include "../llgltexture.h"
#include "../llimagegl.h"
#include "../../almetal/almetaldevice.h"
#include "llimage.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstring>
#include <memory>

namespace
{
    struct NativeAlphaImages
    {
        ALRenderCapabilities previous_capabilities = gRenderCapabilities;
        ALMetalDevice* previous_device = LLImageGL::getMetalDevice();
        bool previous_authority = LLImageGL::isMetalTextureAuthority();
        bool previous_thread_textures = LLImageGLThread::sEnabledTextures;
        std::unique_ptr<LL::WorkQueue> owned_main;
        std::unique_ptr<LL::WorkQueue> owned_general;
        std::shared_ptr<LL::WorkQueueBase> main;
        std::shared_ptr<LL::WorkQueueBase> general;
        std::unique_ptr<ALMetalDevice> device;
        std::string error;

        NativeAlphaImages()
        {
            // LLImageGL captures the main queue when constructed. Drive the
            // same posted analysis/answer path without a live viewer thread.
            main = LL::WorkQueue::getInstance("mainloop");
            if (!main)
            {
                owned_main = std::make_unique<LL::WorkQueue>("mainloop");
                main = LL::WorkQueue::getInstance("mainloop");
            }
            general = LL::WorkQueue::getInstance("General");
            if (!general)
            {
                owned_general = std::make_unique<LL::WorkQueue>("General");
                general = LL::WorkQueue::getInstance("General");
            }
            REQUIRE(main);
            REQUIRE(general);
            device = ALMetalDevice::create(&error);
            INFO(error);
            REQUIRE(device);
            gRenderCapabilities.initializeMetal(device->info());
            LLImageGL::setMetalDevice(device.get());
            LLImageGL::setMetalTextureAuthority(true);
            LLImageGLThread::sEnabledTextures = false;
        }
        void settle()
        {
            for (U32 i = 0; i < 16; ++i)
            {
                general->runPending();
                main->runPending();
            }
        }
        ~NativeAlphaImages()
        {
            // Also drain after a failed assertion: analyses retain the image
            // until its main-queue callback drops their reference.
            settle();
            device->waitUntilIdle();
            LLImageGLThread::sEnabledTextures = previous_thread_textures;
            LLImageGL::setMetalTextureAuthority(previous_authority);
            LLImageGL::setMetalDevice(previous_device);
            gRenderCapabilities = previous_capabilities;
        }
    };

    struct MaskWatcher : LLGLTexture
    {
        MaskWatcher() : LLGLTexture(32, 32, 4, false) {}
        void onAlphaMaskChanged() override
        {
            CHECK(on_main_thread());
            ++changes;
        }
        S32 changes = 0;
    };

    LLPointer<LLImageRaw> alphaRaw(bool mask, U16 side = 32)
    {
        LLPointer<LLImageRaw> raw = new LLImageRaw(side, side, 4);
        for (U32 i = 0; i < U32(side) * side; ++i)
        {
            raw->getData()[4 * i + 0] = 0x80;
            raw->getData()[4 * i + 1] = 0x80;
            raw->getData()[4 * i + 2] = 0x80;
            raw->getData()[4 * i + 3] = mask ? (i < U32(side) * side / 2 ? 0 : 255) : 128;
        }
        return raw;
    }

    TEST_CASE_METHOD(NativeAlphaImages,
        "LLImageGL: native alpha answers notify owners in both directions and detach safely",
        "[LLImageGL][almetal][upstream-texture][alpha-answer]")
    {
        LLPointer<MaskWatcher> owner = new MaskWatcher();
        owner->generateGLTexture();
        LLPointer<LLImageGL> image = owner->getGLTexture();
        REQUIRE(image->getOwner() == owner.get());
        REQUIRE(image->createMetalTexture(0, alphaRaw(true).get()));
        CHECK(image->getTexName() == 0);
        CHECK(image->getIsAlphaMask());
        CHECK(owner->changes == 1);
        REQUIRE(image->createMetalTexture(0, alphaRaw(true).get()));
        CHECK(owner->changes == 1);
        REQUIRE(image->createMetalTexture(0, alphaRaw(false).get()));
        CHECK_FALSE(image->getIsAlphaMask());
        CHECK(owner->changes == 2);
        REQUIRE(image->createMetalTexture(0, alphaRaw(true).get()));
        CHECK(owner->changes == 3);
        image->setNeedsAlphaAndPickMask(false);
        CHECK_FALSE(image->getIsAlphaMask());
        CHECK(owner->changes == 4);
        owner = nullptr;
        CHECK(image->getOwner() == nullptr);
        image->setNeedsAlphaAndPickMask(true);
        REQUIRE(image->createMetalTexture(0, alphaRaw(true).get()));
        CHECK(image->getIsAlphaMask());
    }

    TEST_CASE_METHOD(NativeAlphaImages,
        "LLImageGL: deferred native alpha distinguishes unread and consumed provisional answers",
        "[LLImageGL][almetal][upstream-texture][alpha-answer]")
    {
        LLPointer<MaskWatcher> owner = new MaskWatcher();
        owner->generateGLTexture();
        LLPointer<LLImageGL> image = owner->getGLTexture();
        REQUIRE(image->createMetalTexture(0, alphaRaw(true, 64).get()));
        CHECK_FALSE(image->getIsAlphaMask());
        CHECK(owner->changes == 0);
        settle();
        CHECK(image->getIsAlphaMask());
        CHECK(owner->changes == 1);
        REQUIRE(image->createMetalTexture(0, alphaRaw(true, 64).get()));
        settle();
        CHECK(image->getIsAlphaMask());
        CHECK(owner->changes == 1);
        REQUIRE(image->createMetalTexture(0, alphaRaw(true, 64).get()));
        CHECK_FALSE(image->getIsAlphaMask());
        settle();
        CHECK(image->getIsAlphaMask());
        CHECK(owner->changes == 2);
        REQUIRE(image->createMetalTexture(0, alphaRaw(false, 64).get()));
        settle();
        CHECK_FALSE(image->getIsAlphaMask());
        CHECK(owner->changes == 3);

        // A queued answer retains the image, while the outer owner may go.
        REQUIRE(image->createMetalTexture(0, alphaRaw(true, 64).get()));
        owner = nullptr;
        CHECK(image->getOwner() == nullptr);
        settle();
        // Owner cleanup invalidates that old generation's analysis. A new
        // upload can still finish safely while no outer owner remains.
        CHECK_FALSE(image->getIsAlphaMask());
        REQUIRE(image->createMetalTexture(0, alphaRaw(true, 64).get()));
        settle();
        CHECK(image->getIsAlphaMask());
    }

    TEST_CASE_METHOD(NativeAlphaImages,
        "LLImageGL: staged native alpha changes only at successful publication",
        "[LLImageGL][almetal][upstream-texture][alpha-answer][publication]")
    {
        LLPointer<MaskWatcher> owner = new MaskWatcher();
        owner->generateGLTexture();
        LLPointer<LLImageGL> image = owner->getGLTexture();
        REQUIRE(image->createMetalTexture(0, alphaRaw(false).get()));
        CHECK(owner->changes == 0);
        image->beginUpload();
        REQUIRE(image->createMetalTexture(0, alphaRaw(true).get()));
        CHECK_FALSE(image->getIsAlphaMask());
        CHECK(owner->changes == 0);
        image->cancelUpload();
        CHECK_FALSE(image->getIsAlphaMask());
        CHECK(owner->changes == 0);
        image->beginUpload();
        REQUIRE(image->createMetalTexture(0, alphaRaw(true).get()));
        CHECK_FALSE(image->getIsAlphaMask());
        CHECK(owner->changes == 0);
        image->publishMetalTextureAuthoritative();
        CHECK(image->getIsAlphaMask());
        CHECK(owner->changes == 1);
    }

    TEST_CASE_METHOD(NativeAlphaImages,
        "LLImageGL: native readback accepts 4096 texels and refuses oversized requests nonfatally",
        "[LLImageGL][almetal][upstream-texture][readback]")
    {
        for (const auto width : { MAX_IMAGE_SIZE, MAX_IMAGE_SIZE * 2 })
        {
            LLPointer<LLImageRaw> raw = new LLImageRaw(width, 2, 4);
            for (S32 i = 0; i < raw->getDataSize(); ++i)
                raw->getData()[i] = U8(i * 17 + 3);
            LLPointer<LLImageGL> image = new LLImageGL(false);
            image->setNeedsAlphaAndPickMask(false);
            REQUIRE(image->createMetalTexture(0, raw.get()));
            REQUIRE(image->getMetalTexture());
            CHECK(image->getTexName() == 0);
            LLPointer<LLImageRaw> result = new LLImageRaw();
            if (width == MAX_IMAGE_SIZE)
            {
                REQUIRE(image->readBackRaw(0, result, false));
                CHECK(result->getWidth() == width);
                CHECK(result->getHeight() == 2);
                REQUIRE(result->getDataSize() == raw->getDataSize());
                CHECK(std::memcmp(result->getData(), raw->getData(), raw->getDataSize()) == 0);
            }
            else
            {
                CHECK_FALSE(image->readBackRaw(0, result, false));
                CHECK(result->isBufferInvalid());
            }
        }
    }
}
