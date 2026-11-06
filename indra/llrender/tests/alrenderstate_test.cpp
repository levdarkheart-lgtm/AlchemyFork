/**
 * @file alrenderstate_test.cpp
 * @brief Logical renderer-state tests that require no OpenGL context.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#include "catch2/catch_test_macros.hpp"

#include "alrendercapabilities.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llglstates.h"
#include "llimagegl.h"
#include "llrender.h"
#include "llrendertarget.h"

#if LL_DARWIN
#include "almetaldevice.h"
#include "almetalresource.h"
#endif

TEST_CASE("Renderer state remains semantic without an OpenGL context",
          "[alrenderstate][metal-cutover]")
{
    REQUIRE_FALSE(gGLManager.mInited);

    LLRender render;
    render.setLineWidth(3.f);
    render.setPolygonOffset(-2.f, -4.f);
    render.setPatchVertices(4);
    render.setPolygonMode(GL_LINE);
    render.setCullFace(GL_FRONT);
    render.setPointSize(5.f);
    render.setStencilFunc(GL_EQUAL, 7, 0x0fu);
    render.setStencilOp(GL_ZERO, GL_INCR, GL_REPLACE);
    render.setStencilMask(0xf0u);
    render.setBlendEquation(GL_FUNC_ADD, GL_MAX);
    render.setViewport(1, 2, 640, 480);
    render.setScissor(3, 4, 320, 240);
    render.setClearColor(LLColor4(0.25f, 0.5f, 0.75f, 1.f));

    CHECK(render.getLineWidth() == 3.f);
    CHECK(render.getPolygonOffsetFactor() == -2.f);
    CHECK(render.getPolygonOffsetUnits() == -4.f);
    CHECK(render.getPolygonMode() == GL_LINE);
    CHECK(render.getCullFace() == GL_FRONT);
    CHECK(render.getPointSize() == 5.f);
    CHECK(render.getStencilMask() == 0xf0u);
    CHECK(render.getClearColor() == LLColor4(0.25f, 0.5f, 0.75f, 1.f));

    U32 stencil_func = 0;
    S32 stencil_ref = 0;
    U32 stencil_read_mask = 0;
    render.getStencilFunc(stencil_func, stencil_ref, stencil_read_mask);
    CHECK(stencil_func == GL_EQUAL);
    CHECK(stencil_ref == 7);
    CHECK(stencil_read_mask == 0x0fu);

    U32 stencil_fail = 0;
    U32 depth_fail = 0;
    U32 stencil_pass = 0;
    render.getStencilOp(stencil_fail, depth_fail, stencil_pass);
    CHECK(stencil_fail == GL_ZERO);
    CHECK(depth_fail == GL_INCR);
    CHECK(stencil_pass == GL_REPLACE);

    U32 blend_rgb = 0;
    U32 blend_alpha = 0;
    render.getBlendEquation(blend_rgb, blend_alpha);
    CHECK(blend_rgb == GL_FUNC_ADD);
    CHECK(blend_alpha == GL_MAX);

    S32 viewport[4]{};
    render.getViewport(viewport);
    CHECK(viewport[0] == 1);
    CHECK(viewport[1] == 2);
    CHECK(viewport[2] == 640);
    CHECK(viewport[3] == 480);

    S32 scissor[4]{};
    render.getScissor(scissor);
    CHECK(scissor[0] == 3);
    CHECK(scissor[1] == 4);
    CHECK(scissor[2] == 320);
    CHECK(scissor[3] == 240);
}

TEST_CASE("Scoped render state remains balanced without an OpenGL context",
          "[alrenderstate][metal-cutover]")
{
    REQUIRE_FALSE(gGLManager.mInited);
    LLGLState::initClass();

    CHECK_FALSE(LLGLState::isEnabled(GL_BLEND));
    CHECK_FALSE(LLGLDepthTest::depthEnabled());
    CHECK(LLGLDepthTest::depthWriteEnabled());
    CHECK(LLGLDepthTest::depthFunction() == GL_LESS);

    {
        LLGLEnable blend(GL_BLEND);
        LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_GREATER);
        CHECK(LLGLState::isEnabled(GL_BLEND));
        CHECK(LLGLDepthTest::depthEnabled());
        CHECK_FALSE(LLGLDepthTest::depthWriteEnabled());
        CHECK(LLGLDepthTest::depthFunction() == GL_GREATER);
    }

    CHECK_FALSE(LLGLState::isEnabled(GL_BLEND));
    CHECK_FALSE(LLGLDepthTest::depthEnabled());
    CHECK(LLGLDepthTest::depthWriteEnabled());
    CHECK(LLGLDepthTest::depthFunction() == GL_LESS);
}

TEST_CASE("Sampler intent remains available without an OpenGL context",
          "[alrenderstate][metal-cutover]")
{
    REQUIRE_FALSE(gGLManager.mInited);

    struct SamplerReset
    {
        ~SamplerReset() { gGL.clearSamplers(); }
    } reset;

    gGL.clearSamplers();
    gGL.warmupSamplers();
    const U32 point = gGL.getSampler(ALSamplers::PointClamp);
    const U32 linear = gGL.getSampler(ALSamplers::BilinearClamp);
    CHECK(point != 0);
    CHECK(linear != 0);
    CHECK(point != linear);

    ALSamplerDesc point_desc;
    REQUIRE(gGL.describeSampler(point, point_desc));
    CHECK(point_desc.mMinFilter == ALSamplerFilter::nearest);
    CHECK(point_desc.mMagFilter == ALSamplerFilter::nearest);
    CHECK(point_desc.mMipFilter == ALSamplerMipFilter::nearest);
    CHECK(point_desc.mWrapS == ALSamplerAddressMode::clamp_to_edge);
    CHECK_FALSE(point_desc.mSRGBDecode);

    const ALSamplerDesc srgb =
        ALSamplerCache::makeDesc(ALSamplers::AnisoWrapSRGB);
    CHECK(srgb.mMinFilter == ALSamplerFilter::linear);
    CHECK(srgb.mMagFilter == ALSamplerFilter::linear);
    CHECK(srgb.mMipFilter == ALSamplerMipFilter::linear);
    CHECK(srgb.mWrapS == ALSamplerAddressMode::repeat);
    CHECK(srgb.mSRGBDecode);

    ALSamplerDesc custom = ALSamplerCache::makeDesc(ALSamplers::BilinearClamp);
    custom.mMagFilter = ALSamplerFilter::nearest;
    const U32 custom_name = gGL.getSampler(custom);
    CHECK(custom_name != 0);
    CHECK(custom_name != linear);
    ALSamplerDesc described_custom;
    REQUIRE(gGL.describeSampler(custom_name, described_custom));
    CHECK(described_custom == custom);

    ALTextureSlot* slot = gGL.getTextureSlot(31);
    ALTextureSlot::sSamplerBinds = 0;
    ALTextureSlot::sSamplerSkips = 0;
    slot->bindSampler(point);
    slot->bindSampler(point);
    CHECK(ALTextureSlot::sSamplerBinds == 1);
    CHECK(ALTextureSlot::sSamplerSkips == 1);
    slot->bindSampler(0);
}

#if LL_DARWIN
TEST_CASE("Texture slots retain native resource identity without OpenGL",
          "[alrenderstate][metal-cutover]")
{
    REQUIRE_FALSE(gGLManager.mInited);

    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);

    ALMetalTextureDescriptor descriptor;
    descriptor.type = ALMetalTextureType::texture_cube;
    descriptor.format = ALMetalPixelFormat::rgba8_unorm;
    descriptor.width = 4;
    descriptor.height = 4;
    descriptor.usage = ALMetalTextureUsage::shader_read;
    descriptor.label = "Texture slot native identity test";
    auto texture = ALMetalTexture::create(*device, descriptor, &error);
    INFO(error);
    REQUIRE(texture);

    struct SamplerReset
    {
        ~SamplerReset() { gGL.clearSamplers(); }
    } reset;
    gGL.clearSamplers();
    gGL.warmupSamplers();

    ALTextureSlot slot(7);
    REQUIRE(slot.bindNative(texture.get(), ALTextureSlot::TT_CUBE_MAP,
                            ALSamplers::AnisoClamp));
    CHECK(slot.getCurrTexture() != 0);
    CHECK(slot.getCurrType() == ALTextureSlot::TT_CUBE_MAP);
    CHECK(slot.getCurrImage() == nullptr);
    CHECK(slot.getCurrMetalTexture() == texture.get());

    CHECK_FALSE(slot.bindNative(texture.get(), ALTextureSlot::TT_TEXTURE,
                                ALSamplers::AnisoClamp));
    CHECK(slot.getCurrMetalTexture() == texture.get());

    // An arbitrary integer attachment name has no native owner and must not
    // displace a valid Metal binding or report a fallback success.
    CHECK_FALSE(slot.bindManual(ALTextureSlot::TT_TEXTURE, 0x1234u));
    CHECK(slot.getCurrType() == ALTextureSlot::TT_CUBE_MAP);
    CHECK(slot.getCurrMetalTexture() == texture.get());

    slot.unbind();
    CHECK(slot.getCurrTexture() == 0);
    CHECK(slot.getCurrType() == ALTextureSlot::TT_NONE);
    CHECK(slot.getCurrMetalTexture() == nullptr);
}

TEST_CASE("Render targets own, clear, mip and share native Metal attachments",
          "[alrenderstate][metal-cutover]")
{
    REQUIRE_FALSE(gGLManager.mInited);

    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error);
    REQUIRE(device);

    struct NativeStateReset
    {
        ~NativeStateReset()
        {
            while (LLRenderTarget::getCurrentBoundTarget())
            {
                LLRenderTarget::getCurrentBoundTarget()->flush();
            }
            gGL.clearSamplers();
            LLImageGL::setMetalDevice(nullptr);
        }
    } reset;
    LLImageGL::setMetalDevice(device.get());
    gGL.clearSamplers();
    gGL.warmupSamplers();

    const U32 bytes_before = LLRenderTarget::sBytesAllocated;
    LLRenderTarget owner;
    owner.setLabel("Native LLRenderTarget test owner");
    REQUIRE(owner.allocate(4, 4, GL_RGBA8, true, true,
                           ALTextureSlot::TT_TEXTURE,
                           LLRenderTarget::MIPS_AUTO,
                           LLRenderTarget::DEPTH_FMT_32F));
    REQUIRE(owner.addColorAttachment(GL_R32F));
    CHECK(owner.isComplete());
    CHECK(owner.getTexture(0) == 0);
    CHECK(owner.getDepth() == 0);
    CHECK(owner.getNumTextures() == 2);
    CHECK(owner.ownsDepthAttachment());
    REQUIRE(owner.getMetalColorTexture(0));
    REQUIRE(owner.getMetalColorTexture(1));
    REQUIRE(owner.getMetalDepthTexture());
    CHECK(owner.getMetalColorTexture(0)->descriptor().mipmap_level_count == 3);
    CHECK(owner.getMetalColorTexture(1)->descriptor().mipmap_level_count == 3);

    gGL.setClearColor(LLColor4(0.25f, 0.5f, 0.75f, 1.f));
    owner.bindTarget();
    owner.clear();
    owner.flush();

    std::array<U8, 4 * 4 * 4> pixels{};
    REQUIRE(owner.getMetalColorTexture(0)->readback(
        *device, 0, 0, {}, pixels.data(), 4 * 4, &error));
    INFO(error);
    CHECK(pixels[0] == 64);
    CHECK(pixels[1] == 128);
    CHECK(pixels[2] == 191);
    CHECK(pixels[3] == 255);

    std::array<U8, 4> last_mip{};
    REQUIRE(owner.getMetalColorTexture(0)->readback(
        *device, 2, 0, {}, last_mip.data(), 4, &error));
    INFO(error);
    CHECK(last_mip == std::array<U8, 4>{64, 128, 191, 255});

    LLRenderTarget borrower;
    borrower.setLabel("Native LLRenderTarget test borrower");
    REQUIRE(borrower.allocate(4, 4, GL_RGBA8));
    owner.shareDepthBuffer(borrower);
    CHECK(borrower.isComplete());
    CHECK(borrower.sharesDepth());
    CHECK_FALSE(borrower.ownsDepthAttachment());
    CHECK(borrower.getMetalDepthTexture() == owner.getMetalDepthTexture());

    ALTextureSlot slot(7);
    REQUIRE(slot.bind(&owner));
    CHECK(slot.getCurrMetalTexture() == owner.getMetalColorTexture(0));
    REQUIRE(slot.bind(&borrower, true));
    CHECK(slot.getCurrMetalTexture() == owner.getMetalDepthTexture());

    borrower.release();
    owner.release();
    CHECK(LLRenderTarget::sBytesAllocated == bytes_before);
}
#endif

TEST_CASE("Shader binding remains logical without an OpenGL context",
          "[alrenderstate][metal-cutover]")
{
    REQUIRE_FALSE(gGLManager.mInited);

    LLGLSLShader shader;
    shader.bind();
    CHECK(LLGLSLShader::sCurBoundShader == 0);
    CHECK(LLGLSLShader::sCurBoundShaderPtr == &shader);

    LLGLSLShader::unbind();
    CHECK(LLGLSLShader::sCurBoundShader == 0);
    CHECK(LLGLSLShader::sCurBoundShaderPtr == nullptr);
}

#if LL_DARWIN
TEST_CASE("Metal shader configuration retains pass variants without compiling GLSL",
          "[alrenderstate][metal-cutover]")
{
    REQUIRE_FALSE(gGLManager.mInited);

    struct CapabilitiesReset
    {
        ~CapabilitiesReset() { gRenderCapabilities.reset(); }
    } reset;
    ALMetalDeviceInfo info;
    info.name = "Test Apple GPU";
    info.registry_id = 7;
    info.highest_apple_family = 8;
    info.highest_mac_family = 2;
    gRenderCapabilities.initializeMetal(info);

    LLGLSLShader shader;
    shader.mName = "Native semantic pass";
    shader.mShaderFiles.emplace_back("not-opened-nativeV.glsl", GL_VERTEX_SHADER);
    shader.mShaderFiles.emplace_back("not-opened-nativeF.glsl", GL_FRAGMENT_SHADER);
    shader.mShaderLevel = 3;

    REQUIRE(shader.createShader(LLGLSLShader::VARIANT_RIGGED |
                                LLGLSLShader::VARIANT_MIRROR));
    CHECK_FALSE(shader.isComplete());
    CHECK(shader.isNativeConfigured());
    CHECK(shader.isAvailable());
    CHECK(shader.mProgramObject == 0);

    U32 variants = 0;
    shader.forEachVariant([&variants](LLGLSLShader& configured)
    {
        ++variants;
        CHECK(configured.isNativeConfigured());
        CHECK_FALSE(configured.isComplete());
        CHECK(configured.mProgramObject == 0);
    });
    CHECK(variants == 4);

    shader.bind();
    CHECK(LLGLSLShader::sCurBoundShaderPtr == &shader);
    LLGLSLShader::unbind();
    shader.unload();
    CHECK_FALSE(shader.isAvailable());
}
#endif
