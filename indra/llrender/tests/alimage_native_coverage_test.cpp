/**
 * @file alimage_native_coverage_test.cpp
 * @brief Native image lifecycle, pixels, sampling and worker-publication coverage.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../llcubemap.h"
#include "../llimagegl.h"
#include "../llgl.h"
#include "../llgltexture.h"
#include "../llrender.h"
#include "../alrendercapabilities.h"
#include "../../almetal/almetaldevice.h"
#include "../../almetal/almetalresource.h"
#include "llimage.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{
#include "../llimagegl_upload_native.inl"

    // The worker is joined and its native completion/publication queue drained before
    // any lambda-captured image, atomics or callback results leave the test scope.
    struct WorkerScope
    {
        ALMetalDevice& device;
        LL::WorkQueueBase& mainloop;
        std::function<void()> release;
        WorkerScope(ALMetalDevice& d, LL::WorkQueueBase& queue,
                    std::function<void()> unblock = {})
            : device(d), mainloop(queue), release(std::move(unblock))
        {
            LLImageGLThread::createInstance(nullptr);
        }
        ~WorkerScope()
        {
            if (release) release();
            LLImageGLThread::deleteSingleton();
            device.waitUntilIdle();
            mainloop.runPending();
            LLImageGL::finishUploads();
        }
    };

    struct llimage_native_data
    {
        ALRenderCapabilities previous_capabilities = gRenderCapabilities;
        ALMetalDevice* previous_device = LLImageGL::getMetalDevice();
        bool previous_authority = LLImageGL::isMetalTextureAuthority();
        bool previous_disabled = gGLManager.mIsDisabled;
        bool previous_thread_textures = LLImageGLThread::sEnabledTextures;
        bool previous_thread_media = LLImageGLThread::sEnabledMedia;
        LLImageGL* previous_default = LLImageGL::sDefaultGLTexture;
        LLImageGL* previous_white = ALTextureSlot::sWhiteImage.load();
        U32 previous_white_token = ALTextureSlot::sWhiteTexture.load();
        std::array<ALTextureSlot, AL_NUM_TEXTURE_SLOTS> previous_slots;
        std::unique_ptr<LL::WorkQueue> owned_main_queue;
        std::shared_ptr<LL::WorkQueueBase> main_queue;
        std::unique_ptr<ALMetalDevice> fixture_device;

        llimage_native_data()
        {
            REQUIRE_FALSE(LLImageGLThread::instanceExists());
            main_queue = LL::WorkQueue::getInstance("mainloop");
            if (!main_queue)
            {
                owned_main_queue = std::make_unique<LL::WorkQueue>("mainloop");
                main_queue = LL::WorkQueue::getInstance("mainloop");
            }
            REQUIRE(main_queue);
            std::string error;
            fixture_device = ALMetalDevice::create(&error);
            INFO(error);
            REQUIRE(fixture_device);
            for (U32 i = 0; i < AL_NUM_TEXTURE_SLOTS; ++i)
                previous_slots[i] = *gGL.getTextureSlot(i);
            gRenderCapabilities.initializeMetal(fixture_device->info());
            LLImageGL::setMetalDevice(fixture_device.get());
            LLImageGL::setMetalTextureAuthority(true);
            LLImageGLThread::sEnabledTextures = false;
            LLImageGLThread::sEnabledMedia = false;
        }
        ~llimage_native_data()
        {
            LLImageGL::cleanupClass();
            for (U32 i = 0; i < AL_NUM_TEXTURE_SLOTS; ++i)
                *gGL.getTextureSlot(i) = previous_slots[i];
            LLImageGL::sDefaultGLTexture = previous_default;
            ALTextureSlot::sWhiteImage.store(previous_white);
            ALTextureSlot::sWhiteTexture.store(previous_white_token);
            LLImageGLThread::sEnabledTextures = previous_thread_textures;
            LLImageGLThread::sEnabledMedia = previous_thread_media;
            LLImageGL::setMetalTextureAuthority(previous_authority);
            LLImageGL::setMetalDevice(previous_device);
            gGLManager.mIsDisabled = previous_disabled;
            gRenderCapabilities = previous_capabilities;
        }
        static LLPointer<LLImageRaw> makeRaw(U16 w, U16 h, S8 components, U8 fill)
        {
            LLPointer<LLImageRaw> raw = new LLImageRaw(w, h, components);
            std::memset(raw->getData(), fill, static_cast<std::size_t>(w) * h * components);
            return raw;
        }
        static LLPointer<LLImageRaw> loadViewerTexture(const char* filename)
        {
            const auto path = std::filesystem::path(AL_TEST_VIEWER_TEXTURE_DIR) / filename;
            LLPointer<LLImageFormatted> formatted = LLImageFormatted::createFromExtension(path.string());
            INFO(path.string());
            REQUIRE(formatted.notNull());
            REQUIRE(formatted->load(path.string()));
            LLPointer<LLImageRaw> raw = new LLImageRaw();
            REQUIRE(formatted->decode(raw, 0.f));
            REQUIRE_FALSE(raw->isBufferInvalid());
            return raw;
        }
        static U32 readTexelRGBA(const LLImageGL* image, S32 x, S32 y)
        {
            LLPointer<LLImageRaw> raw = new LLImageRaw();
            REQUIRE(image->readBackRaw(0, raw, false));
            REQUIRE(raw->getComponents() == 4);
            REQUIRE(x >= 0);
            REQUIRE(y >= 0);
            REQUIRE(x < raw->getWidth());
            REQUIRE(y < raw->getHeight());
            U32 texel = 0;
            std::memcpy(&texel, raw->getData() +
                (static_cast<std::size_t>(y) * raw->getWidth() + x) * 4, sizeof(texel));
            return texel;
        }
    };

    TEST_CASE_METHOD(llimage_native_data,
                     "LLImageGL: readBackRaw fails closed when its Metal mirror is missing",
                     "[LLImageGL][almetal][native-image-coverage]")
    {
        LLPointer<LLImageRaw> src = makeRaw(8, 8, 4, /*fill=*/0x5a);
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(img->createMetalTexture(0, src.get()));
        REQUIRE(img->getTexName() == 0);
        REQUIRE(img->getMetalTexture());

        img->discardMetalTexture();
        REQUIRE_FALSE(img->getMetalTexture());

        LLPointer<LLImageRaw> dst = new LLImageRaw();
        CHECK_FALSE(img->readBackRaw(0, dst.get(), /*compressed_ok=*/false));
        CHECK(dst->isBufferInvalid());
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: typed float uploads are converted to exact Metal half-float texels", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr U16 W = 4;
        constexpr U16 H = 2;
        constexpr std::size_t COMPONENTS = 2;
        std::array<F32, W * H * COMPONENTS> source{{
            0.f, 1.f, -2.f, 0.5f, 4.f, -0.25f, 65504.f, -0.f,
            1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f,
        }};
        const std::array<U16, W * H * COMPONENTS> source_half{{
            0x0000, 0x3c00, 0xc000, 0x3800,
            0x4400, 0xb400, 0x7bff, 0x8000,
            0x3c00, 0x4000, 0x4200, 0x4400,
            0x4500, 0x4600, 0x4700, 0x4800,
        }};

        LLPointer<LLImageGL> image =
            new LLImageGL(W, H, COMPONENTS, /*usemipmaps=*/false);
        image->setExplicitFormat(GL_RG16F, GL_RG, GL_FLOAT);
        REQUIRE(image->createMetalTexture(
            0, reinterpret_cast<const U8*>(source.data())));
        REQUIRE(image->getMetalTexture());
        CHECK(image->getMetalTexture()->descriptor().format ==
              ALMetalPixelFormat::rg16_float);

        std::array<U16, W * H * COMPONENTS> metal{};
        REQUIRE(image->getMetalTexture()->readback(
            *device, 0, 0, {}, metal.data(),
            W * COMPONENTS * sizeof(U16), &error));
        INFO(error);
        CHECK(metal == source_half);

        std::array<F32, W * H * COMPONENTS> patch;
        patch.fill(0.25f);
        REQUIRE(image->uploadMetalSubImage(
            reinterpret_cast<const U8*>(patch.data()), W, H,
            /*x_pos=*/1, /*y_pos=*/0, /*width=*/2, /*height=*/2));
        REQUIRE(image->getMetalTexture()->readback(
            *device, 0, 0, {}, metal.data(),
            W * COMPONENTS * sizeof(U16), &error));
        INFO(error);
        for (U32 y = 0; y < H; ++y)
        {
            for (U32 x = 0; x < W; ++x)
            {
                for (std::size_t component = 0;
                     component < COMPONENTS; ++component)
                {
                    const std::size_t index =
                        (static_cast<std::size_t>(y) * W + x) *
                            COMPONENTS +
                        component;
                    CHECK(metal[index] ==
                          (x >= 1 && x < 3 ? U16{0x3400}
                                           : source_half[index]));
                }
            }
        }

        constexpr U16 RGB_W = 2;
        constexpr U16 RGB_H = 1;
        constexpr std::array<F32, RGB_W * RGB_H * 3> rgb_source{{
            1.f, 0.5f, -2.f,
            0.25f, 4.f, -0.f,
        }};
        LLPointer<LLImageGL> rgb_image =
            new LLImageGL(RGB_W, RGB_H, 3, /*usemipmaps=*/false);
        rgb_image->setExplicitFormat(GL_RGB16F, GL_RGB, GL_FLOAT);
        REQUIRE(rgb_image->createMetalTexture(
            0, reinterpret_cast<const U8*>(rgb_source.data())));
        REQUIRE(rgb_image->getMetalTexture());
        CHECK(rgb_image->getMetalTexture()->descriptor().format ==
              ALMetalPixelFormat::rgba16_float);

        std::array<U16, RGB_W * RGB_H * 4> rgb_metal{};
        REQUIRE(rgb_image->getMetalTexture()->readback(
            *device, 0, 0, {}, rgb_metal.data(),
            RGB_W * 4 * sizeof(U16), &error));
        INFO(error);
        CHECK(rgb_metal == std::array<U16, RGB_W * RGB_H * 4>{{
            0x3c00, 0x3800, 0xc000, 0x3c00,
            0x3400, 0x4400, 0x8000, 0x3c00,
        }});

        constexpr std::array<U8, 2 * sizeof(F32)> swapped_source{{
            0x3f, 0x80, 0x00, 0x00,
            0xc0, 0x00, 0x00, 0x00,
        }};
        LLPointer<LLImageGL> swapped_image =
            new LLImageGL(1, 1, 2, /*usemipmaps=*/false);
        swapped_image->setExplicitFormat(
            GL_RG16F, GL_RG, GL_FLOAT, /*swap_bytes=*/true);
        REQUIRE(swapped_image->createMetalTexture(0, swapped_source.data()));
        REQUIRE(swapped_image->getMetalTexture());
        std::array<U16, 2> swapped_metal{};
        REQUIRE(swapped_image->getMetalTexture()->readback(
            *device, 0, 0, {}, swapped_metal.data(),
            2 * sizeof(U16), &error));
        INFO(error);
        CHECK(swapped_metal == std::array<U16, 2>{{0x3c00, 0xc000}});
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: readBackRaw uses its Metal mirror and collapses RGB storage", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr U16 W = 8;
        constexpr U16 H = 8;
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        LLPointer<LLImageRaw> gl_pixels = makeRaw(W, H, 3, 0x20);
        REQUIRE(img->createMetalTexture(0, gl_pixels.get()));
        REQUIRE(img->getMetalTexture());

        ALMetalTextureDescriptor descriptor = img->getMetalTexture()->descriptor();
        descriptor.usage = descriptor.usage | ALMetalTextureUsage::render_target;
        descriptor.label = "Alchemy LLImageGL Metal readback test";
        auto metal_pixels = ALMetalTexture::create(*device, descriptor, &error);
        INFO(error);
        REQUIRE(metal_pixels);

        std::array<U8, W * H * 4> rgba{};
        for (std::size_t pixel = 0; pixel < W * H; ++pixel)
        {
            rgba[pixel * 4 + 0] = static_cast<U8>(pixel);
            rgba[pixel * 4 + 1] = static_cast<U8>(pixel + 1);
            rgba[pixel * 4 + 2] = static_cast<U8>(pixel + 2);
            rgba[pixel * 4 + 3] = 0xff;
        }
        auto command = device->makeCommandBuffer("Alchemy LLImageGL Metal readback upload", &error);
        INFO(error);
        REQUIRE(command);
        REQUIRE(metal_pixels->upload(*command, 0, 0, {}, rgba.data(), W * 4, &error));
        INFO(error);
        REQUIRE(command->commitAndWait(&error));
        INFO(error);
        REQUIRE(img->adoptMetalRenderTexture(std::move(metal_pixels)));

        LLPointer<LLImageRaw> result = new LLImageRaw();
        REQUIRE(img->readBackRaw(0, result.get(), /*compressed_ok=*/false));
        REQUIRE(result->getWidth() == W);
        REQUIRE(result->getHeight() == H);
        REQUIRE(result->getComponents() == 3);
        const U8* bytes = result->getData();
        for (std::size_t pixel = 0; pixel < W * H; ++pixel)
        {
            CHECK(bytes[pixel * 3 + 0] == rgba[pixel * 4 + 0]);
            CHECK(bytes[pixel * 3 + 1] == rgba[pixel * 4 + 1]);
            CHECK(bytes[pixel * 3 + 2] == rgba[pixel * 4 + 2]);
        }
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: adopts a render-produced Metal mirror and reuses its superset usage", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr U16 W = 8;
        constexpr U16 H = 8;
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        img->setExplicitFormat(GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE);
        REQUIRE(img->createMetalTexture(0, makeRaw(W, H, 3, 0x20).get()));
        REQUIRE(img->getMetalTexture());

        const U64 initial_stamp = img->getContentStamp();
        auto rendered = img->createMetalRenderTexture(&error);
        INFO(error);
        REQUIRE(rendered);
        CHECK(alMetalHasUsage(rendered->descriptor().usage,
                              ALMetalTextureUsage::shader_read));
        CHECK(alMetalHasUsage(rendered->descriptor().usage,
                              ALMetalTextureUsage::render_target));
        ALMetalTexture* rendered_pointer = rendered.get();
        REQUIRE(img->adoptMetalRenderTexture(std::move(rendered)));
        CHECK(img->getMetalTexture() == rendered_pointer);
        CHECK(img->getContentStamp() != initial_stamp);

        const U64 adopted_stamp = img->getContentStamp();
        img->markMetalRenderTextureUpdated();
        CHECK(img->getContentStamp() != adopted_stamp);

        // A later upload asks only for shader-read usage. The adopted
        // shader-read|render-target allocation is a valid superset and must
        // be updated in place rather than replaced with undefined storage.
        REQUIRE(img->uploadMetalSubImage(makeRaw(W, H, 3, 0x60)->getData(),
                                         W, H, 0, 0, W, H));
        CHECK(img->getMetalTexture() == rendered_pointer);

        std::array<U8, W * H * 4> pixels{};
        REQUIRE(img->getMetalTexture()->readback(
            *device, 0, 0, {}, pixels.data(), W * 4, &error));
        INFO(error);
        for (std::size_t pixel = 0; pixel < W * H; ++pixel)
        {
            CHECK(pixels[pixel * 4 + 0] == 0x60);
            CHECK(pixels[pixel * 4 + 1] == 0x60);
            CHECK(pixels[pixel * 4 + 2] == 0x60);
            CHECK(pixels[pixel * 4 + 3] == 0xff);
        }

        const U64 uploaded_stamp = img->getContentStamp();
        img->discardMetalTexture();
        CHECK(img->getMetalTexture() == nullptr);
        CHECK(img->getContentStamp() != uploaded_stamp);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: authoritative Metal render publication retains native-only ownership", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr U16 W = 8;
        constexpr U16 H = 8;
        const U64 baseline_bytes = LLImageGL::getTextureBytesAllocated();
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/true);
        img->setExplicitFormat(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
        REQUIRE(img->createMetalTexture(0, makeRaw(W, H, 4, 0x20).get()));
        REQUIRE_FALSE(img->getHasGLTexture());
        REQUIRE(img->getMetalTexture());
        const U64 logical_bytes = static_cast<U64>(
            static_cast<S64>(img->getMipBytes(0)));
        CHECK(LLImageGL::getTextureBytesAllocated() ==
              baseline_bytes + logical_bytes);

        auto rendered = img->createMetalRenderTexture(&error);
        INFO(error);
        REQUIRE(rendered);
        REQUIRE(rendered->descriptor().mipmap_level_count == 4);

        std::array<U8, W * H * 4> rgba{};
        for (std::size_t pixel = 0; pixel < W * H; ++pixel)
        {
            rgba[pixel * 4 + 0] = static_cast<U8>(pixel);
            rgba[pixel * 4 + 1] = static_cast<U8>(pixel + 1);
            rgba[pixel * 4 + 2] = 0x80;
            rgba[pixel * 4 + 3] = 0xff;
        }
        auto command = device->makeCommandBuffer(
            "Alchemy authoritative Metal texture publication", &error);
        INFO(error);
        REQUIRE(command);
        REQUIRE(rendered->upload(
            *command, 0, 0, {}, rgba.data(), W * 4, &error));
        REQUIRE(rendered->generateMipmaps(*command, &error));
        REQUIRE(command->commitAndWait(&error));
        INFO(error);

        ALMetalTexture* rendered_pointer = rendered.get();
        REQUIRE(img->adoptMetalRenderTextureAuthoritative(
            std::move(rendered), LLGLTexture::LOCAL));
        CHECK_FALSE(img->getHasGLTexture());
        CHECK(img->getTexName() == 0);
        CHECK(img->getMetalTexture() == rendered_pointer);
        CHECK(img->getCategory() == LLGLTexture::LOCAL);
        CHECK(img->getTextureMemory() > S64Bytes(0));
        CHECK(LLImageGL::getTextureBytesAllocated() ==
              baseline_bytes + logical_bytes);

        std::array<U8, W * H * 4> result{};
        REQUIRE(img->getMetalTexture()->readback(
            *device, 0, 0, {}, result.data(), W * 4, &error));
        INFO(error);
        CHECK(result == rgba);

        img->destroyGLTexture();
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: scaleDown preserves its Metal mirror with a GPU blit", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr U16 W = 16;
        constexpr U16 H = 16;
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/true);
        REQUIRE(img->createMetalTexture(0, makeRaw(W, H, 4, 0x80).get()));
        REQUIRE(img->getMetalTexture());
        CHECK(img->getMetalTexture()->descriptor().width == W);
        CHECK(img->getMetalTexture()->descriptor().height == H);

        const bool scaled = img->scaleDown(1);

        REQUIRE(scaled);
        REQUIRE(img->getMetalTexture());
        CHECK(img->getMetalTexture()->descriptor().width == W / 2);
        CHECK(img->getMetalTexture()->descriptor().height == H / 2);
        CHECK(img->getMetalTexture()->descriptor().mipmap_level_count == 4);

        std::vector<U8> pixels((W / 2) * (H / 2) * 4, 0);
        REQUIRE(img->getMetalTexture()->readback(
            *device, 0, 0, {}, pixels.data(), (W / 2) * 4, &error));
        INFO(error);
        CHECK(std::all_of(pixels.begin(), pixels.end(), [](U8 value) { return value == 0x80; }));
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: scaleDown keeps a native-authoritative texture Metal-only", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr U16 W = 16;
        constexpr U16 H = 16;
        const U64 baseline_bytes = LLImageGL::getTextureBytesAllocated();
        LLPointer<LLImageGL> img = new LLImageGL(W, H, 4, /*usemipmaps=*/true);
        REQUIRE(img->createMetalTexture(
            0, makeRaw(W, H, 4, 0x6c).get(), LLGLTexture::LOCAL));
        REQUIRE(img->getMetalTexture());
        CHECK_FALSE(img->getHasGLTexture());
        CHECK(img->getTexName() == 0);

        const S64Bytes original_memory = img->getTextureMemory();
        REQUIRE(original_memory > S64Bytes(0));
        CHECK(LLImageGL::getTextureBytesAllocated() ==
              baseline_bytes + static_cast<U64>(static_cast<S64>(original_memory)));

        REQUIRE(img->scaleDown(1));

        REQUIRE(img->getMetalTexture());
        CHECK_FALSE(img->getHasGLTexture());
        CHECK(img->getTexName() == 0);
        CHECK(img->getDiscardLevel() == 1);
        CHECK(img->getMetalTexture()->descriptor().width == W / 2);
        CHECK(img->getMetalTexture()->descriptor().height == H / 2);
        CHECK(img->getMetalTexture()->descriptor().mipmap_level_count == 4);
        CHECK(img->getTextureMemory() == S64Bytes(img->getMipBytes(1)));
        CHECK(img->getTextureMemory() < original_memory);
        CHECK(LLImageGL::getTextureBytesAllocated() ==
              baseline_bytes + static_cast<U64>(
                  static_cast<S64>(img->getTextureMemory())));

        std::vector<U8> base((W / 2) * (H / 2) * 4, 0);
        REQUIRE(img->getMetalTexture()->readback(
            *device, 0, 0, {}, base.data(), (W / 2) * 4, &error));
        INFO(error);
        CHECK(std::all_of(base.begin(), base.end(), [](U8 value) { return value == 0x6c; }));

        std::array<U8, 4> tail{};
        REQUIRE(img->getMetalTexture()->readback(
            *device, 3, 0, {}, tail.data(), tail.size(), &error));
        INFO(error);
        CHECK(std::all_of(tail.begin(), tail.end(), [](U8 value) { return value == 0x6c; }));

        img->destroyGLTexture();
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: no-mip scaleDown resamples its Metal mirror on the GPU", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr U16 W = 16;
        constexpr U16 H = 16;
        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(img->createMetalTexture(0, makeRaw(W, H, 4, 0x80).get()));
        REQUIRE(img->getMetalTexture());

        // The native resampler owns its render target and pipeline.
        const bool scaled = img->scaleDown(1);

        REQUIRE(scaled);
        REQUIRE(img->getMetalTexture());
        CHECK(img->getMetalTexture()->descriptor().width == W / 2);
        CHECK(img->getMetalTexture()->descriptor().height == H / 2);
        CHECK(img->getMetalTexture()->descriptor().mipmap_level_count == 1);
        CHECK(alMetalHasUsage(img->getMetalTexture()->descriptor().usage,
                              ALMetalTextureUsage::render_target));

        std::vector<U8> pixels((W / 2) * (H / 2) * 4, 0);
        REQUIRE(img->getMetalTexture()->readback(
            *device, 0, 0, {}, pixels.data(), (W / 2) * 4, &error));
        INFO(error);
        CHECK(std::all_of(pixels.begin(), pixels.end(), [](U8 value) { return value == 0x80; }));
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: no-mip native-authoritative scaleDown never creates GL backing", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr U16 W = 16;
        constexpr U16 H = 16;
        const U64 baseline_bytes = LLImageGL::getTextureBytesAllocated();
        LLPointer<LLImageGL> img = new LLImageGL(W, H, 4, /*usemipmaps=*/false);
        REQUIRE(img->createMetalTexture(
            0, makeRaw(W, H, 4, 0x93).get(), LLGLTexture::LOCAL));
        REQUIRE(img->getMetalTexture());
        CHECK_FALSE(img->getHasGLTexture());

        CHECK(LLImageGL::getTextureBytesAllocated() ==
              baseline_bytes + static_cast<U64>(W) * H * 4);
        REQUIRE(img->scaleDown(1));

        REQUIRE(img->getMetalTexture());
        CHECK_FALSE(img->getHasGLTexture());
        CHECK(img->getTexName() == 0);
        CHECK(img->getDiscardLevel() == 1);
        CHECK(img->getMetalTexture()->descriptor().width == W / 2);
        CHECK(img->getMetalTexture()->descriptor().height == H / 2);
        CHECK(img->getMetalTexture()->descriptor().mipmap_level_count == 1);
        CHECK(alMetalHasUsage(img->getMetalTexture()->descriptor().usage,
                              ALMetalTextureUsage::render_target));
        CHECK(img->getTextureMemory() == S64Bytes(img->getBytes(1)));
        CHECK(LLImageGL::getTextureBytesAllocated() ==
              baseline_bytes + static_cast<U64>(W / 2) * (H / 2) * 4);

        std::vector<U8> pixels((W / 2) * (H / 2) * 4, 0);
        REQUIRE(img->getMetalTexture()->readback(
            *device, 0, 0, {}, pixels.data(), (W / 2) * 4, &error));
        INFO(error);
        CHECK(std::all_of(pixels.begin(), pixels.end(), [](U8 value) { return value == 0x93; }));

        img->destroyGLTexture();
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLCubeMap: publishes transformed faces only to one native cube", "[LLCubeMap][almetal][native-image-coverage]")
    {
        constexpr U32 resolution = 64;
        constexpr U32 components = 4;
        const bool flip_x[6] = {false, true, false, false, true, false};
        const bool flip_y[6] = {true, true, true, false, true, true};
        const bool transpose[6] = {false, false, false, false, true, true};
        constexpr U32 source_for_slice[6] = {1, 0, 3, 2, 5, 4};

        std::vector<LLPointer<LLImageRaw>> sources;
        sources.reserve(6);
        for (U32 face = 0; face < 6; ++face)
        {
            LLPointer<LLImageRaw> raw = new LLImageRaw(
                resolution, resolution, components);
            for (U32 y = 0; y < resolution; ++y)
            {
                for (U32 x = 0; x < resolution; ++x)
                {
                    U8* pixel = raw->getData() +
                        (static_cast<std::size_t>(y) * resolution + x) *
                            components;
                    pixel[0] = static_cast<U8>(face * 31 + x);
                    pixel[1] = static_cast<U8>(face * 17 + y);
                    pixel[2] = static_cast<U8>(x * 3 + y * 5 + face);
                    pixel[3] = static_cast<U8>(0x80 + face);
                }
            }
            sources.push_back(raw);
        }

        LLPointer<LLCubeMap> cube = new LLCubeMap();
        cube->init(sources);
        CHECK(cube->getResolution() == resolution);

        ALMetalTexture* texture = cube->getMetalTexture();
        REQUIRE(texture);
        CHECK(texture->descriptor().type == ALMetalTextureType::texture_cube);
        CHECK(texture->descriptor().format == ALMetalPixelFormat::rgba8_unorm);
        CHECK(texture->descriptor().width == resolution);
        CHECK(texture->descriptor().height == resolution);
        CHECK(texture->descriptor().mipmap_level_count == 1);

        ALTextureSlot* slot = gGL.getTextureSlot(7);
        REQUIRE(slot->bind(cube.get(), ALSamplers::AnisoClamp));
        CHECK(slot->getCurrType() == ALTextureSlot::TT_CUBE_MAP);
        CHECK(slot->getCurrTexture() != 0);
        CHECK(slot->getCurrImage() == nullptr);
        CHECK(slot->getCurrMetalTexture() == texture);
        slot->unbind();
        CHECK(slot->getCurrMetalTexture() == nullptr);

        ALMetalDevice* device = LLImageGL::getMetalDevice();
        REQUIRE(device);
        std::string error;
        std::vector<U8> actual(
            static_cast<std::size_t>(resolution) * resolution * components);
        std::vector<U8> expected(actual.size());
        for (U32 slice = 0; slice < 6; ++slice)
        {
            const U32 source = source_for_slice[slice];
            for (U32 y = 0; y < resolution; ++y)
            {
                for (U32 x = 0; x < resolution; ++x)
                {
                    U32 sx = flip_x[source] ? resolution - 1 - x : x;
                    U32 sy = flip_y[source] ? resolution - 1 - y : y;
                    if (transpose[source])
                    {
                        std::swap(sx, sy);
                    }
                    const U8* input = sources[source]->getData() +
                        (static_cast<std::size_t>(sy) * resolution + sx) *
                            components;
                    std::copy_n(
                        input, components,
                        expected.begin() +
                            (static_cast<std::size_t>(y) * resolution + x) *
                                components);
                }
            }
            REQUIRE(texture->readback(
                *device, 0, slice, {}, actual.data(),
                resolution * components, &error));
            INFO(error);
            CHECK(actual == expected);
        }

        // A generation prepared without a device remains unavailable instead
        // of exposing the previous cube, then publishes exactly on retry.
        struct DeviceRestore
        {
            ALMetalDevice* device;
            ~DeviceRestore() { LLImageGL::setMetalDevice(device); }
        } restore{device};
        LLImageGL::setMetalDevice(nullptr);
        sources[0]->getData()[0] ^= 0x5a;
        cube->init(sources);
        CHECK(cube->getMetalTexture() == nullptr);
        LLImageGL::setMetalDevice(device);
        ALMetalTexture* retried = cube->getMetalTexture(&error);
        REQUIRE(retried);
        INFO(error);
        REQUIRE(retried->readback(
            *device, 0, 1, {}, actual.data(), resolution * components,
            &error));
        INFO(error);
        const std::size_t changed_pixel =
            static_cast<std::size_t>(resolution - 1) * resolution * components;
        CHECK(actual[changed_pixel] == sources[0]->getData()[0]);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: native creation owns no GL texture", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        LLPointer<LLImageRaw> raw = makeRaw(12, 10, 4, 0x6d);
        LLPointer<LLImageGL> image = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(image->createMetalTexture(0, raw.get()));
        CHECK(image->getTexName() == 0);
        CHECK_FALSE(image->getHasGLTexture());
        CHECK(image->getWidth() == 12);
        CHECK(image->getHeight() == 10);
        REQUIRE(image->getMetalTexture());

        std::array<U8, 12 * 10 * 4> pixels{};
        REQUIRE(image->getMetalTexture()->readback(
            *device, 0, 0, {}, pixels.data(), 12 * 4, &error));
        INFO(error);
        CHECK(std::all_of(pixels.begin(), pixels.end(),
                          [](U8 byte) { return byte == 0x6d; }));

        LLPointer<LLImageRaw> updated = makeRaw(12, 10, 4, 0x6d);
        for (S32 y = 3; y < 7; ++y)
        {
            for (S32 x = 2; x < 8; ++x)
            {
                const std::size_t pixel =
                    (static_cast<std::size_t>(y) * 12 + x) * 4;
                updated->getData()[pixel + 0] = 0x10;
                updated->getData()[pixel + 1] = 0x20;
                updated->getData()[pixel + 2] = 0x30;
                updated->getData()[pixel + 3] = 0x40;
            }
        }
        REQUIRE(image->uploadMetalSubImage(
            updated->getData(), 12, 10, 2, 3, 6, 4));
        REQUIRE(image->getMetalTexture()->readback(
            *device, 0, 0, {}, pixels.data(), 12 * 4, &error));
        INFO(error);
        for (S32 y = 0; y < 10; ++y)
        {
            for (S32 x = 0; x < 12; ++x)
            {
                const std::size_t pixel =
                    (static_cast<std::size_t>(y) * 12 + x) * 4;
                const bool changed = x >= 2 && x < 8 && y >= 3 && y < 7;
                CHECK(pixels[pixel + 0] == (changed ? 0x10 : 0x6d));
                CHECK(pixels[pixel + 1] == (changed ? 0x20 : 0x6d));
                CHECK(pixels[pixel + 2] == (changed ? 0x30 : 0x6d));
                CHECK(pixels[pixel + 3] == (changed ? 0x40 : 0x6d));
            }
        }
        CHECK_FALSE(image->getHasGLTexture());

        LLPointer<LLImageRaw> linear_raw = makeRaw(4, 3, 4, 0x42);
        LLPointer<LLImageGL> linear = new LLImageGL(/*usemipmaps=*/false);
        linear->setExplicitFormat(GL_RGBA8, GL_RGBA);
        REQUIRE(linear->createMetalTexture(0, linear_raw.get()));
        REQUIRE(linear->getMetalTexture());
        CHECK(linear->getMetalTexture()->descriptor().format ==
              ALMetalPixelFormat::rgba8_unorm);
        CHECK_FALSE(linear->getHasGLTexture());
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: HDRI-style native creation preserves float pixels and mipmaps without GL", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr U16 W = 8;
        constexpr U16 H = 4;
        std::array<F32, W * H * 4> source{};
        for (std::size_t pixel = 0; pixel < W * H; ++pixel)
        {
            source[pixel * 4 + 0] = 0.25f;
            source[pixel * 4 + 1] = 0.5f;
            source[pixel * 4 + 2] = 1.f;
            source[pixel * 4 + 3] = 2.f;
        }

        const U64 baseline_bytes = LLImageGL::getTextureBytesAllocated();
        LLPointer<LLImageGL> image = new LLImageGL(W, H, 4, true);
        image->setExplicitFormat(GL_RGBA16F, GL_RGBA, GL_FLOAT);
        image->setHasMipMaps(true);
        image->setUseMipMaps(true);
        REQUIRE(image->createMetalTexture(
            0, reinterpret_cast<const U8*>(source.data())));

        CHECK(image->getTexName() == 0);
        CHECK_FALSE(image->getHasGLTexture());
        REQUIRE(image->getMetalTexture());
        CHECK(image->getMetalTexture()->descriptor().format ==
              ALMetalPixelFormat::rgba16_float);
        CHECK(image->getMetalTexture()->descriptor().width == W);
        CHECK(image->getMetalTexture()->descriptor().height == H);
        CHECK(image->getMetalTexture()->descriptor().mipmap_level_count == 4);
        CHECK(image->getTextureMemory() == S64Bytes(344));
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes + 344);

        constexpr std::array<U16, 4> expected = {
            0x3400u, 0x3800u, 0x3c00u, 0x4000u
        };
        std::array<U16, W * H * 4> base{};
        REQUIRE(image->getMetalTexture()->readback(
            *device, 0, 0, {}, base.data(), W * 4 * sizeof(U16), &error));
        INFO(error);
        for (std::size_t pixel = 0; pixel < W * H; ++pixel)
        {
            CHECK(std::equal(expected.begin(), expected.end(),
                             base.begin() + pixel * 4));
        }

        std::array<U16, 4> tail{};
        REQUIRE(image->getMetalTexture()->readback(
            *device, 3, 0, {}, tail.data(), tail.size() * sizeof(U16), &error));
        INFO(error);
        CHECK(tail == expected);

        image->destroyGLTexture();
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: staged native creation preserves then replaces its published generation", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        const U64 baseline_bytes = LLImageGL::getTextureBytesAllocated();
        LLPointer<LLImageGL> image = new LLImageGL(/*usemipmaps=*/true);
        REQUIRE(image->createMetalTexture(0, makeRaw(8, 8, 4, 0xff).get()));
        REQUIRE(image->getMetalTexture());
        const U32 old_name = image->getTexName();
        ALMetalTexture* old_metal = image->getMetalTexture();
        const U64 old_accounting = LLImageGL::getTextureBytesAllocated();
        const LLVector2 centre(0.5f, 0.5f);
        REQUIRE(image->getMask(centre));

        // A failed native attempt restores the complete published format view.
        image->beginUpload(3);
        LLImageGL::setMetalDevice(nullptr);
        CHECK_FALSE(image->createMetalTexture(
            0, makeRaw(16, 16, 3, 0x22).get(), LLGLTexture::BOOST_NONE));
        LLImageGL::setMetalDevice(device.get());
        image->cancelUpload();
        CHECK(image->getWidth() == 8);
        CHECK(image->getHeight() == 8);
        CHECK(image->getComponents() == 4);
        CHECK(image->getPrimaryFormat() == static_cast<LLGLenum>(GL_RGBA));
        CHECK(image->getTexName() == old_name);
        CHECK(image->getMetalTexture() == old_metal);
        CHECK(LLImageGL::getTextureBytesAllocated() == old_accounting);

        // A successful replacement remains pending until the explicit
        // authoritative publication boundary.
        LLPointer<LLImageRaw> clear = makeRaw(16, 16, 4, 0x00);
        image->beginUpload(4);
        REQUIRE(image->createMetalTexture(
            0, clear.get(), LLGLTexture::BOOST_NONE));
        CHECK(image->getWidth() == 8);
        CHECK(image->getHeight() == 8);
        CHECK(image->getTexName() == old_name);
        CHECK(image->getMetalTexture() == old_metal);
        CHECK(image->getMask(centre));
        CHECK(LLImageGL::getTextureBytesAllocated() == old_accounting);

        image->publishMetalTextureAuthoritative();

        CHECK(image->getWidth() == 16);
        CHECK(image->getHeight() == 16);
        CHECK(image->getTexName() == 0);
        CHECK_FALSE(image->getHasGLTexture());
        REQUIRE(image->getMetalTexture());
        CHECK(image->getMetalTexture() != old_metal);
        CHECK(image->getMetalTexture()->descriptor().mipmap_level_count == 5);
        CHECK_FALSE(image->getMask(centre));
        CHECK(LLImageGL::getTextureBytesAllocated() ==
              baseline_bytes + static_cast<U64>(
                  static_cast<S64>(image->getTextureMemory())));

        std::array<U8, 16 * 16 * 4> pixels{};
        REQUIRE(image->getMetalTexture()->readback(
            *device, 0, 0, {}, pixels.data(), 16 * 4, &error));
        INFO(error);
        CHECK(std::all_of(pixels.begin(), pixels.end(),
                          [](U8 byte) { return byte == 0x00; }));

        image->destroyGLTexture();
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: native-only residency follows actual Metal storage through discard and teardown", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr U16 W = 12;
        constexpr U16 H = 10;
        const U64 baseline_bytes = LLImageGL::getTextureBytesAllocated();
        LLPointer<LLImageGL> image = new LLImageGL(/*usemipmaps=*/false);

        // RGB8 source pixels expand to RGBA8 native storage. The residency
        // budget must describe the resource Metal allocated, not the smaller
        // host upload layout.
        REQUIRE(image->createMetalTexture(0, makeRaw(W, H, 3, 0x5a).get()));
        REQUIRE(image->getMetalTexture());
        CHECK(image->getMetalTexture()->bytesPerPixel() == 4);
        CHECK_FALSE(image->getHasGLTexture());
        CHECK(LLImageGL::getTextureBytesAllocated() ==
              baseline_bytes + static_cast<U64>(W) * H * 4);

        image->discardMetalTexture();
        CHECK(image->getMetalTexture() == nullptr);
        CHECK(image->getTextureMemory() == S64Bytes(0));
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes);

        REQUIRE(image->createMetalTexture(0, makeRaw(W, H, 3, 0x3c).get()));
        CHECK(LLImageGL::getTextureBytesAllocated() ==
              baseline_bytes + static_cast<U64>(W) * H * 4);
        image = nullptr;
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: typed native creation and region updates preserve float terrain texels", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr S32 W = 6;
        constexpr S32 H = 5;
        std::vector<F32> heights(static_cast<std::size_t>(W) * H);
        for (S32 y = 0; y < H; ++y)
        {
            for (S32 x = 0; x < W; ++x)
            {
                heights[static_cast<std::size_t>(y) * W + x] =
                    static_cast<F32>(y * W + x);
            }
        }

        LLPointer<LLImageGL> height =
            new LLImageGL(W, H, 1, /*usemipmaps=*/false);
        height->setExplicitFormat(GL_R32F, GL_RED, GL_FLOAT);
        REQUIRE(height->createMetalTexture(
            0, reinterpret_cast<const U8*>(heights.data())));
        CHECK_FALSE(height->getHasGLTexture());
        REQUIRE(height->getMetalTexture());

        std::vector<F32> height_readback(heights.size(), -1.f);
        REQUIRE(height->getMetalTexture()->readback(
            *device, 0, 0, {}, height_readback.data(), W * sizeof(F32),
            &error));
        INFO(error);
        CHECK(height_readback == heights);

        const U64 height_stamp = height->getContentStamp();
        std::vector<F32> updated = heights;
        for (S32 y = 1; y < 4; ++y)
        {
            for (S32 x = 2; x < 5; ++x)
            {
                updated[static_cast<std::size_t>(y) * W + x] = -42.f;
            }
        }
        REQUIRE(height->uploadMetalSubImage(
            reinterpret_cast<const U8*>(updated.data()), W, H,
            2, 1, 3, 3));
        CHECK(height->getContentStamp() != height_stamp);
        REQUIRE(height->getMetalTexture()->readback(
            *device, 0, 0, {}, height_readback.data(), W * sizeof(F32),
            &error));
        for (S32 y = 0; y < H; ++y)
        {
            for (S32 x = 0; x < W; ++x)
            {
                const F32 expected =
                    x >= 2 && x < 5 && y >= 1 && y < 4
                        ? -42.f
                        : heights[static_cast<std::size_t>(y) * W + x];
                CHECK(height_readback[static_cast<std::size_t>(y) * W + x]
                      == expected);
            }
        }

        std::vector<F32> composition(
            static_cast<std::size_t>(W) * H * 2);
        for (std::size_t texel = 0; texel < composition.size() / 2; ++texel)
        {
            composition[texel * 2] = 0.f;
            composition[texel * 2 + 1] = 1.f;
        }
        LLPointer<LLImageGL> composed =
            new LLImageGL(W, H, 2, /*usemipmaps=*/false);
        composed->setExplicitFormat(GL_RG16F, GL_RG, GL_FLOAT);
        REQUIRE(composed->createMetalTexture(
            0, reinterpret_cast<const U8*>(composition.data())));
        CHECK_FALSE(composed->getHasGLTexture());
        REQUIRE(composed->getMetalTexture());

        std::vector<U16> half_readback(composition.size(), 0xffffu);
        REQUIRE(composed->getMetalTexture()->readback(
            *device, 0, 0, {}, half_readback.data(),
            W * 2 * sizeof(U16), &error));
        for (std::size_t texel = 0; texel < composition.size() / 2; ++texel)
        {
            CHECK(half_readback[texel * 2] == 0x0000u);
            CHECK(half_readback[texel * 2 + 1] == 0x3c00u);
        }

        for (S32 y = 2; y < 4; ++y)
        {
            for (S32 x = 1; x < 4; ++x)
            {
                const std::size_t texel =
                    static_cast<std::size_t>(y) * W + x;
                composition[texel * 2] = 1.f;
                composition[texel * 2 + 1] = 0.f;
            }
        }
        REQUIRE(composed->uploadMetalSubImage(
            reinterpret_cast<const U8*>(composition.data()), W, H,
            1, 2, 3, 2));
        REQUIRE(composed->getMetalTexture()->readback(
            *device, 0, 0, {}, half_readback.data(),
            W * 2 * sizeof(U16), &error));
        for (S32 y = 0; y < H; ++y)
        {
            for (S32 x = 0; x < W; ++x)
            {
                const bool changed = x >= 1 && x < 4 && y >= 2 && y < 4;
                const std::size_t texel =
                    static_cast<std::size_t>(y) * W + x;
                CHECK(half_readback[texel * 2]
                      == (changed ? 0x3c00u : 0x0000u));
                CHECK(half_readback[texel * 2 + 1]
                      == (changed ? 0x0000u : 0x3c00u));
            }
        }
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: typed native creation generates an exact float mip chain", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr S32 W = 4;
        constexpr S32 H = 4;
        std::array<U16, W * H * 2> texels{};
        for (S32 y = 0; y < H; ++y)
        {
            for (S32 x = 0; x < W; ++x)
            {
                const bool one = ((x + y) & 1) != 0;
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * W + x) * 2;
                texels[offset] = one ? 0x3c00u : 0x0000u;
                texels[offset + 1] = one ? 0x0000u : 0x3c00u;
            }
        }

        LLPointer<LLImageGL> image =
            new LLImageGL(W, H, 2, /*usemipmaps=*/true);
        image->setNeedsAlphaAndPickMask(false);
        image->setExplicitFormat(GL_RG16F, GL_RG, GL_HALF_FLOAT);
        REQUIRE(image->createMetalTexture(
            0, reinterpret_cast<const U8*>(texels.data())));
        CHECK_FALSE(image->getHasGLTexture());
        REQUIRE(image->getMetalTexture());
        CHECK(image->getMetalTexture()->descriptor().mipmap_level_count == 3);

        std::array<U16, W * H * 2> base{};
        REQUIRE(image->getMetalTexture()->readback(
            *device, 0, 0, {}, base.data(), W * 2 * sizeof(U16), &error));
        INFO(error);
        CHECK(base == texels);

        std::array<U16, 2 * 2 * 2> middle{};
        REQUIRE(image->getMetalTexture()->readback(
            *device, 1, 0, {}, middle.data(), 2 * 2 * sizeof(U16), &error));
        INFO(error);
        CHECK(std::all_of(middle.begin(), middle.end(),
                          [](U16 value) { return value == 0x3800u; }));

        std::array<U16, 2> final{};
        REQUIRE(image->getMetalTexture()->readback(
            *device, 2, 0, {}, final.data(), 2 * sizeof(U16), &error));
        INFO(error);
        CHECK(final[0] == 0x3800u);
        CHECK(final[1] == 0x3800u);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: native authored mip chain preserves every level after source release", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        LLPointer<LLImageGL> incomplete =
            new LLImageGL(8, 8, 4, /*usemipmaps=*/true);
        std::array<U8, 8 * 8 * 4> incomplete_base{};
        const std::array<LLImageGL::MetalMipLevel, 1> incomplete_levels{{
            {incomplete_base.data(), 8, 8}
        }};
        CHECK_FALSE(incomplete->createMetalTexture(0, incomplete_levels));
        CHECK(incomplete->getMetalTexture() == nullptr);
        CHECK_FALSE(incomplete->getHasGLTexture());

        constexpr U32 WIDTH = 8;
        constexpr U32 HEIGHT = 8;
        constexpr U32 COMPONENTS = 4;
        const U32 level_count = static_cast<U32>(
            LLImageGL::calcMipLevelCount(WIDTH, HEIGHT));
        std::vector<std::vector<U8>> sources;
        std::vector<LLImageGL::MetalMipLevel> levels;
        sources.reserve(level_count);
        levels.reserve(level_count);

        U32 width = WIDTH;
        U32 height = HEIGHT;
        for (U32 mip = 0; mip < level_count; ++mip)
        {
            sources.emplace_back(
                static_cast<std::size_t>(width) * height * COMPONENTS);
            for (std::size_t byte = 0; byte < sources.back().size(); ++byte)
            {
                sources.back()[byte] = static_cast<U8>(
                    (mip * 0x31u + byte * 0x07u) & 0xffu);
            }
            levels.push_back({sources.back().data(), width, height});
            width = llmax<U32>(1, width >> 1);
            height = llmax<U32>(1, height >> 1);
        }

        LLPointer<LLImageGL> image =
            new LLImageGL(WIDTH, HEIGHT, COMPONENTS, /*usemipmaps=*/true);
        image->setNeedsAlphaAndPickMask(false);
        REQUIRE(image->createMetalTexture(0, levels));
        CHECK_FALSE(image->getHasGLTexture());
        REQUIRE(image->getMetalTexture());
        CHECK(image->getMetalTexture()->descriptor().mipmap_level_count ==
              level_count);

        // The upload owns the bytes once createMetalTexture returns; no mip may depend on
        // the caller retaining its CPU storage through a later command-buffer completion.
        sources.clear();
        sources.shrink_to_fit();
        levels.clear();
        levels.shrink_to_fit();

        width = WIDTH;
        height = HEIGHT;
        for (U32 mip = 0; mip < level_count; ++mip)
        {
            std::vector<U8> actual(
                static_cast<std::size_t>(width) * height * COMPONENTS);
            REQUIRE(image->getMetalTexture()->readback(
                *device, mip, 0, {}, actual.data(), width * COMPONENTS,
                &error));
            INFO(error);
            for (std::size_t byte = 0; byte < actual.size(); ++byte)
            {
                CAPTURE(mip, byte);
                CHECK(actual[byte] == static_cast<U8>(
                    (mip * 0x31u + byte * 0x07u) & 0xffu));
            }
            width = llmax<U32>(1, width >> 1);
            height = llmax<U32>(1, height >> 1);
        }
    }

    TEST_CASE_METHOD(llimage_native_data, "ALTextureSlot: native-only sampling preserves the Metal image identity", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct StateReset
        {
            ~StateReset()
            {
                ALTextureSlot::setWhiteImage(nullptr);
                LLImageGL::setMetalDevice(nullptr);
            }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        LLPointer<LLImageGL> white = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(white->createMetalTexture(0, makeRaw(1, 1, 4, 0xff).get()));
        ALTextureSlot::setWhiteImage(white.get());

        LLPointer<LLImageGL> native = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(native->createMetalTexture(0, makeRaw(8, 8, 4, 0x33).get()));
        REQUIRE(native->getTexName() == 0);

        ALTextureSlot* slot = gGL.getTextureSlot(0);
        REQUIRE(slot->bindSampled(native.get(), ALSamplers::BilinearClamp));
        CHECK(slot->getCurrImage() == native.get());
        CHECK(slot->getCurrMetalTexture() == native->getMetalTexture());
        CHECK(slot->getCurrTexture() == 1u);
        CHECK(slot->getCurrType() == ALTextureSlot::TT_TEXTURE);

        // The nonzero slot token is a CPU identity, never a graphics API name.
        CHECK(native->getTexName() == 0);
        CHECK_FALSE(native->getHasGLTexture());
    }

    TEST_CASE_METHOD(llimage_native_data, "ALTextureSlot: wrapped native-only images cross slow and fast sampled binds", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct StateReset
        {
            ~StateReset()
            {
                ALTextureSlot::setWhiteImage(nullptr);
                LLImageGL::setMetalDevice(nullptr);
            }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        LLPointer<LLImageGL> white = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(white->createMetalTexture(0, makeRaw(1, 1, 4, 0xff).get()));
        ALTextureSlot::setWhiteImage(white.get());

        LLPointer<LLGLTexture> texture = new LLGLTexture(/*usemipmaps=*/true);
        LLPointer<LLImageRaw> clear = makeRaw(8, 8, 4, 0x00);
        REQUIRE(texture->createMetalTexture(0, clear.get()));
        CHECK(texture->hasGPUTexture());
        CHECK_FALSE(texture->hasGLTexture());
        CHECK(texture->getFullWidth() == 8);
        CHECK(texture->getFullHeight() == 8);
        CHECK_FALSE(texture->getMask(LLVector2(0.5f, 0.5f)));
        LLImageGL* native = texture->getGLTexture();
        REQUIRE(native->getTexName() == 0);
        REQUIRE(native->getMetalTexture());
        CHECK(native->getMetalTexture()->descriptor().mipmap_level_count == 4);

        ALTextureSlot* slot = gGL.getTextureSlot(0);
        REQUIRE(slot->bindSampled(texture.get(), ALSamplers::BilinearClamp));
        CHECK(slot->getCurrImage() == native);
        CHECK(slot->getCurrMetalTexture() == native->getMetalTexture());
        CHECK(slot->getCurrTexture() == 1u);

        slot->bindFast(texture.get(), ALSamplers::PointClamp);
        CHECK(slot->getCurrImage() == native);
        CHECK(slot->getCurrMetalTexture() == native->getMetalTexture());
        CHECK(slot->getCurrTexture() == 1u);
        CHECK(slot->getCurrType() == ALTextureSlot::TT_TEXTURE);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLGLTexture: native-only destruction releases Metal residency", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        const U64 baseline_bytes = LLImageGL::getTextureBytesAllocated();
        LLPointer<LLGLTexture> texture = new LLGLTexture(/*usemipmaps=*/false);
        REQUIRE(texture->createMetalTexture(0, makeRaw(8, 8, 4, 0x55).get()));
        REQUIRE(texture->getGLTexture()->getMetalTexture());
        CHECK(texture->hasGPUTexture());
        CHECK_FALSE(texture->hasGLTexture());
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes + 8 * 8 * 4);

        texture->destroyGPUTexture();

        CHECK_FALSE(texture->hasGPUTexture());
        CHECK_FALSE(texture->hasGLTexture());
        CHECK(texture->getGLTexture()->getMetalTexture() == nullptr);
        CHECK(texture->getDiscardLevel() == -1);
        CHECK(texture->getTextureState() == LLGLTexture::DELETED);
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLGLTexture: logical metadata is queryable before native publication", "[LLImageGL][almetal][native-image-coverage]")
    {
        LLPointer<LLGLTexture> texture =
            new LLGLTexture(/*width=*/256, /*height=*/128, /*components=*/4,
                            /*usemipmaps=*/false);

        REQUIRE_FALSE(texture->hasGPUTexture());
        CHECK(texture->getWidth() == 256);
        CHECK(texture->getHeight() == 128);
        CHECK(texture->getWidth(/*discard_level=*/1) == 128);
        CHECK(texture->getHeight(/*discard_level=*/1) == 64);
        CHECK(texture->getComponents() == 4);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLGLTexture: native RGB screenshot preview expands and mipmaps without GL", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        constexpr U16 W = 16;
        constexpr U16 H = 8;
        const U64 baseline_bytes = LLImageGL::getTextureBytesAllocated();
        LLPointer<LLGLTexture> preview = new LLGLTexture(/*usemipmaps=*/true);
        REQUIRE(preview->createMetalTexture(
            0, makeRaw(W, H, 3, 0x5a).get(), LLGLTexture::OTHER));

        CHECK(preview->hasGPUTexture());
        CHECK_FALSE(preview->hasGLTexture());
        CHECK(preview->getFullWidth() == W);
        CHECK(preview->getFullHeight() == H);
        CHECK(preview->getComponents() == 3);

        LLImageGL* image = preview->getGLTexture();
        REQUIRE(image);
        CHECK(image->getTexName() == 0);
        REQUIRE(image->getMetalTexture());
        CHECK(image->getMetalTexture()->descriptor().format ==
              ALMetalPixelFormat::rgba8_unorm_srgb);
        CHECK(image->getMetalTexture()->descriptor().mipmap_level_count == 5);
        CHECK(LLImageGL::getTextureBytesAllocated() ==
              baseline_bytes + static_cast<U64>(
                  static_cast<S64>(image->getTextureMemory())));

        std::array<U8, W * H * 4> base{};
        REQUIRE(image->getMetalTexture()->readback(
            *device, 0, 0, {}, base.data(), W * 4, &error));
        INFO(error);
        for (std::size_t pixel = 0; pixel < W * H; ++pixel)
        {
            CHECK(base[pixel * 4 + 0] == 0x5a);
            CHECK(base[pixel * 4 + 1] == 0x5a);
            CHECK(base[pixel * 4 + 2] == 0x5a);
            CHECK(base[pixel * 4 + 3] == 0xff);
        }

        constexpr std::array<U8, 4> expected_tail{{0x5a, 0x5a, 0x5a, 0xff}};
        std::array<U8, 4> tail{};
        REQUIRE(image->getMetalTexture()->readback(
            *device, 4, 0, {}, tail.data(), tail.size(), &error));
        INFO(error);
        CHECK(tail == expected_tail);

        preview->destroyGPUTexture();
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLGLTexture: metadata-only publication needs no live GL manager or GPU backing", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct StateReset
        {
            bool mDisabled = gGLManager.mIsDisabled;
            ~StateReset()
            {
                gGLManager.mIsDisabled = mDisabled;
                LLImageGL::setMetalDevice(nullptr);
            }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        const U64 baseline_bytes = LLImageGL::getTextureBytesAllocated();
        LLPointer<LLGLTexture> texture = new LLGLTexture(/*usemipmaps=*/true);
        REQUIRE(texture->createMetalTexture(0, makeRaw(8, 8, 4, 0x55).get()));
        REQUIRE(texture->hasGPUTexture());
        REQUIRE(LLImageGL::getTextureBytesAllocated() > baseline_bytes);
        const LLGLTexture::LLGLTextureState state = texture->getTextureState();

        gGLManager.mIsDisabled = true;
        REQUIRE(texture->setImageMetadata(
            /*discard_level=*/2, makeRaw(4, 2, 3, 0x22).get()));

        CHECK_FALSE(texture->hasGPUTexture());
        CHECK_FALSE(texture->hasGLTexture());
        CHECK(texture->getGLTexture()->getMetalTexture() == nullptr);
        CHECK(texture->getDiscardLevel() == 2);
        CHECK(texture->getFullWidth() == 16);
        CHECK(texture->getFullHeight() == 8);
        CHECK(texture->getComponents() == 3);
        CHECK(texture->getTextureMemory() == S32Bytes(0));
        CHECK(texture->getTextureState() == state);
        CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes);
    }

    TEST_CASE_METHOD(llimage_native_data, "LLGLTexture: native system images preserve pixels and mips without GL", "[LLImageGL][almetal][native-image-coverage][system-textures]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct StateReset
        {
            bool mDisabled = gGLManager.mIsDisabled;
            ~StateReset()
            {
                gGLManager.mIsDisabled = mDisabled;
                LLImageGL::setMetalDevice(nullptr);
            }
        } reset;
        LLImageGL::setMetalDevice(device.get());
        gGLManager.mIsDisabled = true;

        struct SystemImage
        {
            U16 mWidth;
            U16 mHeight;
            U8 mUniform;
            bool mChecker;
            S32 mCategory;
            U32 mMipLevels;
        };
        constexpr std::array<SystemImage, 4> images{{
            {1, 1, 0x77, false, LLGLTexture::LOCAL, 1},
            {128, 128, 0x00, false, LLGLTexture::LOCAL, 8},
            {128, 128, 0x7f, false, LLGLTexture::OTHER, 8},
            {32, 32, 0x00, true, LLGLTexture::LOCAL, 6},
        }};

        const U64 baseline_bytes = LLImageGL::getTextureBytesAllocated();
        for (const SystemImage& system : images)
        {
            INFO(system.mWidth << "x" << system.mHeight
                               << (system.mChecker ? " checker" : " uniform"));
            LLPointer<LLImageRaw> raw =
                new LLImageRaw(system.mWidth, system.mHeight, 3);
            if (system.mChecker)
            {
                U8* pixels = raw->getData();
                for (U32 y = 0; y < system.mHeight; ++y)
                {
                    for (U32 x = 0; x < system.mWidth; ++x)
                    {
                        const U8 value = static_cast<U8>(
                            (((x / 16) + (y / 16)) % 2) * 255);
                        const std::size_t offset =
                            (static_cast<std::size_t>(y) * system.mWidth + x) * 3;
                        pixels[offset + 0] = value;
                        pixels[offset + 1] = value;
                        pixels[offset + 2] = value;
                    }
                }
            }
            else
            {
                std::memset(raw->getData(), system.mUniform,
                            static_cast<std::size_t>(system.mWidth) *
                                system.mHeight * 3);
            }

            LLPointer<LLGLTexture> texture = new LLGLTexture(/*usemipmaps=*/true);
            REQUIRE(texture->createMetalTexture(0, raw.get(), system.mCategory));
            REQUIRE(texture->getGLTexture()->getMetalTexture());
            CHECK_FALSE(texture->hasGLTexture());
            CHECK(texture->hasGPUTexture());
            CHECK(texture->getTexName() == 0);
            CHECK(texture->getFullWidth() == system.mWidth);
            CHECK(texture->getFullHeight() == system.mHeight);
            CHECK(texture->getComponents() == 3);
            CHECK(texture->getGLTexture()->getCategory() == system.mCategory);
            CHECK(texture->getGLTexture()->getMetalTexture()
                      ->descriptor().mipmap_level_count == system.mMipLevels);

            std::vector<U8> actual(
                static_cast<std::size_t>(system.mWidth) * system.mHeight * 4);
            REQUIRE(texture->getGLTexture()->getMetalTexture()->readback(
                *device, 0, 0, {}, actual.data(), system.mWidth * 4, &error));
            INFO(error);
            std::vector<U8> expected(actual.size());
            for (U32 y = 0; y < system.mHeight; ++y)
            {
                for (U32 x = 0; x < system.mWidth; ++x)
                {
                    const U8 value = system.mChecker
                        ? static_cast<U8>((((x / 16) + (y / 16)) % 2) * 255)
                        : system.mUniform;
                    const std::size_t offset =
                        (static_cast<std::size_t>(y) * system.mWidth + x) * 4;
                    expected[offset + 0] = value;
                    expected[offset + 1] = value;
                    expected[offset + 2] = value;
                    expected[offset + 3] = 0xff;
                }
            }
            CHECK(actual == expected);

            texture->destroyGPUTexture();
            CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes);
        }
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: shipped startup textures publish exact native pixels without GL", "[LLImageGL][almetal][native-image-coverage][startup-textures]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct StateReset
        {
            bool mDisabled = gGLManager.mIsDisabled;
            ~StateReset()
            {
                gGLManager.mIsDisabled = mDisabled;
                LLImageGL::setMetalDevice(nullptr);
            }
        } reset;
        LLImageGL::setMetalDevice(device.get());
        gGLManager.mIsDisabled = true;

        struct StartupTexture
        {
            const char* mFilename;
            U32 mWidth;
            U32 mHeight;
            S8 mComponents;
            bool mUseMipmaps;
            S32 mCategory;
            U32 mMipLevels;
            bool mAlphaOnly;
        };
        // Every shipped local asset requested explicitly by
        // LLViewerTextureList::doPreloadImages(). UI textures declared by
        // textures.xml share this publication path but form a separate list.
        constexpr std::array<StartupTexture, 11> textures{{
            {"missing_asset.tga", 64, 64, 3, false, LLGLTexture::BOOST_UI, 1, false},
            {"white.tga", 32, 32, 3, false, LLGLTexture::BOOST_UI, 1, false},
            {"pixiesmall.j2c", 128, 128, 4, true, LLGLTexture::BOOST_NONE, 8, false},
            {"flatnormal.tga", 4, 4, 3, false, LLGLTexture::BOOST_BUMP, 1, false},
            {"default_irradiance.png", 1024, 512, 3, true, LLGLTexture::BOOST_UI, 11, false},
            {"world/NoEntryLines.png", 256, 256, 4, true, LLGLTexture::BOOST_UI, 9, false},
            {"world/NoEntryPassLines.png", 256, 256, 4, true, LLGLTexture::BOOST_UI, 9, false},
            {"transparent.j2c", 32, 32, 4, true, LLGLTexture::BOOST_UI, 6, false},
            {"alpha_gradient.tga", 256, 1, 1, true, LLGLTexture::BOOST_UI, 9, true},
            {"alpha_gradient_2d.j2c", 256, 256, 1, true, LLGLTexture::BOOST_UI, 9, true},
            {"SoftDotNoBack.png", 1024, 1024, 4, true, LLGLTexture::BOOST_UI, 11, false},
        }};

        const U64 baseline_bytes = LLImageGL::getTextureBytesAllocated();
        for (const StartupTexture& startup : textures)
        {
            INFO(startup.mFilename);
            LLPointer<LLImageRaw> raw = loadViewerTexture(startup.mFilename);
            REQUIRE(raw->getWidth() == static_cast<S32>(startup.mWidth));
            REQUIRE(raw->getHeight() == static_cast<S32>(startup.mHeight));
            REQUIRE(raw->getComponents() == startup.mComponents);

            LLPointer<LLGLTexture> texture =
                new LLGLTexture(startup.mUseMipmaps);
            texture->generateGLTexture();
            if (startup.mAlphaOnly)
            {
                texture->setExplicitFormat(GL_ALPHA8, GL_ALPHA);
            }
            REQUIRE(texture->createMetalTexture(
                0, raw.get(), startup.mCategory));
            REQUIRE(texture->getGLTexture()->getMetalTexture());
            CHECK(texture->getTexName() == 0);
            CHECK_FALSE(texture->hasGLTexture());
            CHECK_FALSE(texture->isGLTextureCreated());
            CHECK(texture->hasGPUTexture());

            const ALMetalTextureDescriptor& descriptor =
                texture->getGLTexture()->getMetalTexture()->descriptor();
            CHECK(descriptor.width == startup.mWidth);
            CHECK(descriptor.height == startup.mHeight);
            CHECK(descriptor.mipmap_level_count == startup.mMipLevels);
            CHECK(texture->getGLTexture()->getCategory() == startup.mCategory);

            if (startup.mAlphaOnly)
            {
                CHECK(descriptor.format == ALMetalPixelFormat::r8_unorm);
                CHECK(descriptor.swizzle ==
                      std::array<ALMetalTextureSwizzle, 4>{
                          ALMetalTextureSwizzle::zero,
                          ALMetalTextureSwizzle::zero,
                          ALMetalTextureSwizzle::zero,
                          ALMetalTextureSwizzle::red});
                std::vector<U8> actual(
                    static_cast<std::size_t>(startup.mWidth) * startup.mHeight);
                REQUIRE(texture->getGLTexture()->getMetalTexture()->readback(
                    *device, 0, 0, {}, actual.data(), startup.mWidth, &error));
                INFO(error);
                CHECK(std::equal(actual.begin(), actual.end(), raw->getData()));
            }
            else
            {
                CHECK(descriptor.format == ALMetalPixelFormat::rgba8_unorm_srgb);
                std::vector<U8> actual(
                    static_cast<std::size_t>(startup.mWidth) * startup.mHeight * 4);
                REQUIRE(texture->getGLTexture()->getMetalTexture()->readback(
                    *device, 0, 0, {}, actual.data(), startup.mWidth * 4, &error));
                INFO(error);

                std::vector<U8> expected(actual.size(), 0xff);
                for (std::size_t pixel = 0;
                     pixel < static_cast<std::size_t>(startup.mWidth) * startup.mHeight;
                     ++pixel)
                {
                    std::memcpy(expected.data() + pixel * 4,
                                raw->getData() + pixel * startup.mComponents,
                                startup.mComponents);
                }
                CHECK(actual == expected);
            }

            texture = nullptr;
            CHECK(LLImageGL::getTextureBytesAllocated() == baseline_bytes);
        }
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: native media upload updates pixels and preserves allocation padding", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(img->createMetalTexture(0, makeRaw(16, 16, 4, 0x11).get()));
        REQUIRE(img->getMetalTexture());
        const U32 old_name = img->getTexName();
        const U64 old_stamp = img->getContentStamp();
        CHECK(readTexelRGBA(img, 5, 5) == 0x11111111u);

        // Media's valid frame can be smaller than its power-of-two allocation.
        // The update covers only the valid frame; allocation padding stays intact.
        REQUIRE(img->uploadMetalImage(makeRaw(12, 10, 4, 0x77).get()));
        CHECK(img->getTexName() == old_name);
        CHECK(readTexelRGBA(img, 5, 5) == 0x77777777u);
        CHECK(img->getContentStamp() != old_stamp);

        std::array<U8, 16 * 16 * 4> pixels{};
        REQUIRE(img->getMetalTexture()->readback(
            *device, 0, 0, {}, pixels.data(), 16 * 4, &error));
        INFO(error);
        for (U32 y = 0; y < 16; ++y)
        {
            for (U32 x = 0; x < 16; ++x)
            {
                const U8 expected = x < 12 && y < 10 ? 0x77 : 0x11;
                const std::size_t offset = (static_cast<std::size_t>(y) * 16 + x) * 4;
                CHECK(std::all_of(pixels.begin() + offset,
                                  pixels.begin() + offset + 4,
                                  [expected](U8 byte) { return byte == expected; }));
            }
        }
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: native worker upload publishes only after its Metal batch commits", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        auto& mainloop = *main_queue;

        LLPointer<LLImageGL> img = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(img->createMetalTexture(0, makeRaw(16, 16, 4, 0x11).get()));
        REQUIRE(img->getMetalTexture());
        ALMetalTexture* old_metal = img->getMetalTexture();
        const U32 old_name = img->getTexName();
        LLPointer<LLImageRaw> raw = makeRaw(16, 16, 4, 0x77);

        std::atomic<bool> release_first{ false };
        std::atomic<bool> release_second{ false };
        std::atomic<bool> upload_done{ false };
        std::atomic<bool> upload_ok{ false };
        bool completed = false;
        bool publication_succeeded = false;
        WorkerScope worker(*device, mainloop, [&]()
        {
            release_first.store(true, std::memory_order_release);
            release_second.store(true, std::memory_order_release);
        });

        // The upload queues its batch flush behind the second blocker. This gives the
        // main thread a deterministic interval in which queuePublish exists but its
        // Metal commit token is still pending.
        REQUIRE(LLImageGLThread::instance().post([&]()
        {
            while (!release_first.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
        }));
        img->beginUpload();
        REQUIRE(LLImageGLThread::instance().post([&]()
        {
            upload_ok = img->uploadMetalImage(raw.get());
            if (upload_ok.load(std::memory_order_acquire))
            {
                upload_ok = img->syncMetalToMainThread();
            }
            upload_done.store(true, std::memory_order_release);
        }));
        REQUIRE(LLImageGLThread::instance().post([&]()
        {
            while (!release_second.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
        }));

        release_first.store(true, std::memory_order_release);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!upload_done.load(std::memory_order_acquire) &&
               std::chrono::steady_clock::now() < deadline)
        {
            mainloop.runPending();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        mainloop.runPending();
        REQUIRE(upload_ok.load(std::memory_order_acquire));

        img->afterPublish([&](bool succeeded)
        {
            publication_succeeded = succeeded;
            completed = true;
        });
        CHECK(LLImageGL::publishUploads() == 1);
        CHECK_FALSE(completed);
        CHECK(img->getTexName() == old_name);
        CHECK(img->getMetalTexture() == old_metal);

        release_second.store(true, std::memory_order_release);
        while (!completed && std::chrono::steady_clock::now() < deadline)
        {
            mainloop.runPending();
            LLImageGL::publishUploads();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        REQUIRE(completed);
        CHECK(publication_succeeded);
        CHECK(img->getTexName() == old_name);
        REQUIRE(img->getMetalTexture());
        CHECK(img->getMetalTexture() != old_metal);
        std::array<U8, 16 * 16 * 4> pixels{};
        REQUIRE(img->getMetalTexture()->readback(
            *device, 0, 0, {}, pixels.data(), 16 * 4, &error));
        INFO(error);
        CHECK(std::all_of(pixels.begin(), pixels.end(),
                          [](U8 byte) { return byte == 0x77; }));
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: Metal-authoritative worker creates native-only textures", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct StateRestore
        {
            ALMetalDevice* mDevice = LLImageGL::getMetalDevice();
            bool mAuthority = LLImageGL::isMetalTextureAuthority();
            ~StateRestore()
            {
                LLImageGL::setMetalTextureAuthority(mAuthority);
                LLImageGL::setMetalDevice(mDevice);
            }
        } state_restore;
        LLImageGL::setMetalDevice(device.get());
        LLImageGL::setMetalTextureAuthority(true);
        REQUIRE(LLImageGL::isMetalTextureAuthority());

        auto& mainloop = *main_queue;

        LLPointer<LLImageGL> image = new LLImageGL(/*usemipmaps=*/true);
        LLPointer<LLImageRaw> raw = makeRaw(32, 16, 4, 0x6d);
        image->beginUpload(raw->getComponents());

        std::atomic<bool> worker_done{ false };
        std::atomic<bool> upload_ok{ false };
        bool published = false;
        bool publication_succeeded = false;
        WorkerScope worker(*device, mainloop);
        REQUIRE(LLImageGLThread::instance().post([&]()
        {
            bool created = image->createMetalTexture(
                0, raw.get(), LLGLTexture::OTHER);
            if (created)
            {
                created = image->syncMetalToMainThread();
            }
            upload_ok.store(created, std::memory_order_release);
            worker_done.store(true, std::memory_order_release);
        }));

        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!worker_done.load(std::memory_order_acquire) &&
               std::chrono::steady_clock::now() < deadline)
        {
            mainloop.runPending();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        mainloop.runPending();
        REQUIRE(upload_ok.load(std::memory_order_acquire));

        image->afterPublish([&](bool succeeded)
        {
            publication_succeeded = succeeded;
            published = true;
        });
        while (!published && std::chrono::steady_clock::now() < deadline)
        {
            mainloop.runPending();
            LLImageGL::publishUploads();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        REQUIRE(published);
        CHECK(publication_succeeded);
        CHECK(image->getTexName() == 0);
        CHECK_FALSE(image->getHasGLTexture());
        REQUIRE(image->getMetalTexture());
        CHECK(image->getMetalTexture()->descriptor().width == 32);
        CHECK(image->getMetalTexture()->descriptor().height == 16);
        CHECK(image->getMetalTexture()->descriptor().mipmap_level_count == 6);

        std::array<U8, 32 * 16 * 4> pixels{};
        REQUIRE(image->getMetalTexture()->readback(
            *device, 0, 0, {}, pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(std::all_of(pixels.begin(), pixels.end(),
                          [](U8 byte) { return byte == 0x6d; }));
    }

    TEST_CASE_METHOD(llimage_native_data, "LLImageGL: queued worker textures share one Metal upload submission", "[LLImageGL][almetal][native-image-coverage]")
    {
        std::string error;
        auto device = ALMetalDevice::create(&error);
        INFO(error);
        REQUIRE(device);

        struct MetalDeviceReset
        {
            ~MetalDeviceReset() { LLImageGL::setMetalDevice(nullptr); }
        } reset;
        LLImageGL::setMetalDevice(device.get());

        auto& mainloop = *main_queue;

        LLPointer<LLImageGL> first = new LLImageGL(/*usemipmaps=*/false);
        LLPointer<LLImageGL> second = new LLImageGL(/*usemipmaps=*/false);
        REQUIRE(first->createMetalTexture(0, makeRaw(16, 16, 4, 0x11).get()));
        REQUIRE(second->createMetalTexture(0, makeRaw(16, 16, 4, 0x22).get()));

        LLPointer<LLImageRaw> first_raw = makeRaw(32, 32, 4, 0x55);
        LLPointer<LLImageRaw> second_raw = makeRaw(32, 32, 4, 0xaa);
        first->beginUpload();
        second->beginUpload();

        // Hold the sole worker until both uploads are in its FIFO. The first upload then
        // appends the batch-commit barrier behind the second one, proving a real queue burst
        // rather than relying on timing between producer and consumer threads.
        std::atomic<bool> release_worker{ false };
        std::atomic<U32> workers_done{ 0 };
        std::atomic<bool> first_ok{ false };
        std::atomic<bool> second_ok{ false };
        bool first_published = false;
        bool second_published = false;
        WorkerScope worker(*device, mainloop, [&]()
        {
            release_worker.store(true, std::memory_order_release);
        });
        REQUIRE(LLImageGLThread::instance().post([&]()
        {
            while (!release_worker.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
        }));
        REQUIRE(LLImageGLThread::instance().post([&]()
        {
            first_ok = first->createMetalTexture(0, first_raw.get()) &&
                       first->syncMetalToMainThread();
            workers_done.fetch_add(1, std::memory_order_release);
        }));
        REQUIRE(LLImageGLThread::instance().post([&]()
        {
            second_ok = second->createMetalTexture(0, second_raw.get()) &&
                        second->syncMetalToMainThread();
            workers_done.fetch_add(1, std::memory_order_release);
        }));

        const std::uint64_t submissions_before =
            ALMetalDevice::committedCommandBufferCount();
        release_worker.store(true, std::memory_order_release);

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (workers_done.load(std::memory_order_acquire) != 2 &&
               std::chrono::steady_clock::now() < deadline)
        {
            mainloop.runPending();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        mainloop.runPending();
        REQUIRE(first_ok.load());
        REQUIRE(second_ok.load());

        first->afterPublish([&]() { first_published = true; });
        second->afterPublish([&]() { second_published = true; });
        while ((!first_published || !second_published) &&
               std::chrono::steady_clock::now() < deadline)
        {
            mainloop.runPending();
            LLImageGL::publishUploads();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        REQUIRE(first_published);
        REQUIRE(second_published);
        CHECK(ALMetalDevice::committedCommandBufferCount() == submissions_before + 1);
        REQUIRE(first->getMetalTexture());
        REQUIRE(second->getMetalTexture());

        std::array<U8, 32 * 32 * 4> first_pixels{};
        std::array<U8, 32 * 32 * 4> second_pixels{};
        REQUIRE(first->getMetalTexture()->readback(
            *device, 0, 0, {}, first_pixels.data(), 32 * 4, &error));
        INFO(error);
        REQUIRE(second->getMetalTexture()->readback(
            *device, 0, 0, {}, second_pixels.data(), 32 * 4, &error));
        INFO(error);
        CHECK(std::all_of(first_pixels.begin(), first_pixels.end(),
                          [](U8 byte) { return byte == 0x55; }));
        CHECK(std::all_of(second_pixels.begin(), second_pixels.end(),
                          [](U8 byte) { return byte == 0xaa; }));
    }

}

// These CPU cases execute the same private format/staging functions as the native
// upload call. They do not create a device, image, queue, window or renderer.
TEST_CASE("Native RGB8 upload honors source channels and opaque storage", "[image-upload-layout]")
{
    for (const U32 primary : {GL_RGB, GL_RGBA, GL_BGRA})
    for (const bool swap_bytes : {false, true})
    for (const auto size : {std::array<U32, 2>{3, 2}, {1, 1}, {2, 3}})
    for (const std::size_t padding : {std::size_t(0), std::size_t(5)})
    {
        CAPTURE(primary, swap_bytes, size[0], size[1], padding);
        const std::size_t components = primary == GL_RGB ? 3 : 4;
        const std::size_t row_bytes = size[0] * components + padding;
        // The leading pixel also models the pointer offset supplied for a subregion.
        std::vector<U8> source(components + row_bytes * size[1], 0xe7);
        U8* pixels = source.data() + components;
        std::vector<U8> expected;
        for (U32 row = 0; row < size[1]; ++row)
        for (U32 column = 0; column < size[0]; ++column)
        {
            const U8 red = static_cast<U8>(10 + row * 30 + column * 3);
            const U8 green = static_cast<U8>(red + 1);
            const U8 blue = static_cast<U8>(red + 2);
            U8* pixel = pixels + row * row_bytes + column * components;
            pixel[0] = primary == GL_BGRA ? blue : red;
            pixel[1] = green;
            pixel[2] = primary == GL_BGRA ? red : blue;
            if (components == 4) pixel[3] = static_cast<U8>(31 + row + column);
            // BGRA native storage retains the source BGR byte order, not an RGB shuffle.
            expected.insert(expected.end(), {pixel[0], pixel[1], pixel[2], 255});
        }
        const auto original = source;
        const auto format = metal_upload_format(GL_RGB8, primary, GL_UNSIGNED_BYTE);
        CHECK(format.format == (primary == GL_BGRA ? ALMetalPixelFormat::bgra8_unorm
                                                   : ALMetalPixelFormat::rgba8_unorm));
        const U8* upload = nullptr;
        std::size_t upload_row_bytes = 0;
        std::vector<U8> converted;
        const bool prepared = prepare_metal_upload_pixels(
            format, primary, GL_UNSIGNED_BYTE, swap_bytes, size[0], size[1], pixels,
            row_bytes, upload, upload_row_bytes, converted);
        CHECK(prepared);
        CHECK(source == original);
        if (!prepared) continue;
        REQUIRE(upload);
        CHECK(upload_row_bytes == size[0] * 4);
        for (U32 row = 0; row < size[1]; ++row)
        for (U32 byte = 0; byte < size[0] * 4; ++byte)
            CHECK(upload[row * upload_row_bytes + byte] == expected[row * size[0] * 4 + byte]);
    }
}

TEST_CASE("Native upload keeps alpha and existing scalar conversions", "[image-upload-layout]")
{
    // Accelerated CEF uses RGBA8 storage with BGRA bytes, unlike RGB8 software/media.
    const std::array<U8, 8> bgra{30, 20, 10, 7, 60, 50, 40, 99};
    for (const bool swap_bytes : {false, true})
    {
        const auto format = metal_upload_format(GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE);
        REQUIRE(format.format == ALMetalPixelFormat::bgra8_unorm);
        const U8* upload = nullptr;
        std::size_t row_bytes = 0;
        std::vector<U8> converted;
        REQUIRE(prepare_metal_upload_pixels(format, GL_BGRA, GL_UNSIGNED_BYTE,
            swap_bytes, 2, 1, bgra.data(), 8, upload, row_bytes, converted));
        CHECK(upload == bgra.data());
        CHECK(row_bytes == 8);
        CHECK(converted.empty());
        CHECK(std::equal(bgra.begin(), bgra.end(), upload));
    }
    // Packed BGRA and its existing whole-word swap must not become byte-swap no-ops.
    {
        const auto format = metal_upload_format(GL_RGB8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);
        const U8* upload = nullptr;
        std::size_t row_bytes = 0;
        std::vector<U8> converted;
        REQUIRE(prepare_metal_upload_pixels(format, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
            true, 2, 1, bgra.data(), 8, upload, row_bytes, converted));
        const std::vector<U8> expected{7, 10, 20, 30, 99, 40, 50, 60};
        CHECK(converted == expected);
        CHECK(row_bytes == 8);
    }
    // The existing float-to-half path handles source scalar swapping before conversion.
    for (const bool swap_bytes : {false, true})
    {
        const std::array<F32, 4> values{1.f, -2.f, .5f, 0.f};
        std::array<U8, sizeof(values)> bytes{};
        std::memcpy(bytes.data(), values.data(), bytes.size());
        if (swap_bytes)
            for (std::size_t offset = 0; offset < bytes.size(); offset += sizeof(F32))
                std::reverse(bytes.begin() + offset, bytes.begin() + offset + sizeof(F32));
        const auto format = metal_upload_format(GL_RG16F, GL_RG, GL_FLOAT);
        const U8* upload = nullptr;
        std::size_t row_bytes = 0;
        std::vector<U8> converted;
        REQUIRE(prepare_metal_upload_pixels(format, GL_RG, GL_FLOAT, swap_bytes,
            2, 1, bytes.data(), bytes.size(), upload, row_bytes, converted));
        const std::array<U16, 4> expected{0x3c00u, 0xc000u, 0x3800u, 0x0000u};
        REQUIRE(converted.size() == sizeof(expected));
        CHECK(std::memcmp(upload, expected.data(), sizeof(expected)) == 0);
        CHECK(row_bytes == sizeof(expected));
    }
    // Default three-channel sRGB images still use the same RGB expansion byte contract.
    {
        const auto format = metal_upload_format(GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE);
        REQUIRE(format.format == ALMetalPixelFormat::rgba8_unorm_srgb);
        const std::array<U8, 3> source{19, 87, 211};
        const U8* upload = nullptr;
        std::size_t row_bytes = 0;
        std::vector<U8> converted;
        REQUIRE(prepare_metal_upload_pixels(format, GL_RGB, GL_UNSIGNED_BYTE, false,
            1, 1, source.data(), 3, upload, row_bytes, converted));
        CHECK(converted == std::vector<U8>{19, 87, 211, 255});
        CHECK(row_bytes == 4);
    }
}
