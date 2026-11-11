/**
 * @file albaking_shader_metal_test.cpp
 * @brief The appearance utility's real loader configures Metal without GL.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llgl.h"
#include "../llglslshader.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetaldevice.h"
#include "../../llappearanceutility/llbakingshadermgr.h"
#include <catch2/catch_test_macros.hpp>

extern LLGLSLShader gAlphaMaskProgram;

namespace
{
struct BakingLoaderScope final : LLBakingShaderMgr
{
    LLShaderMgr* previous_manager = sInstance;
    ALRenderCapabilities previous_capabilities = gRenderCapabilities;
    bool previous_initialized = sInitialized;
    bool previous_gl = gGLManager.mInited;
    bool previous_authority = LLVertexBuffer::isMetalBufferAuthority();
    S32 previous_channels = LLGLSLShader::sIndexedTextureChannels;
    LLGLSLShader* previous_bound = LLGLSLShader::sCurBoundShaderPtr;
    U32 previous_program = LLGLSLShader::sCurBoundShader;
    bool previous_registered = LLGLSLShader::sConfigured.contains(&gAlphaMaskProgram);
    std::string previous_name = gAlphaMaskProgram.mName;
    S32 previous_level = gAlphaMaskProgram.mShaderLevel;
    LLUUID previous_hash = gAlphaMaskProgram.mShaderHash;

    BakingLoaderScope() { sInstance = this; }
    ~BakingLoaderScope() override
    {
        gGLManager.mInited = false;
        unloadShaders();
        gAlphaMaskProgram.mName = previous_name;
        gAlphaMaskProgram.mShaderLevel = previous_level;
        gAlphaMaskProgram.mShaderHash = previous_hash;
        if (!previous_registered) LLGLSLShader::sConfigured.erase(&gAlphaMaskProgram);
        LLGLSLShader::sCurBoundShaderPtr = previous_bound;
        LLGLSLShader::sCurBoundShader = previous_program;
        LLGLSLShader::sIndexedTextureChannels = previous_channels;
        LLVertexBuffer::setMetalBufferAuthority(previous_authority);
        gRenderCapabilities = previous_capabilities;
        gGLManager.mInited = previous_gl;
        sInitialized = previous_initialized;
        sInstance = previous_manager;
    }
};
}

TEST_CASE("Appearance shader loader configures native metadata without a GL compiler",
          "[llrender][almetal][baking-loader]")
{
    // Exact1eb44 utility used these two alpha-mask sources at interface level2.
    // Exercise the production loader, rather than reproducing its configuration.
    REQUIRE_FALSE(gGLManager.mInited);
    REQUIRE_FALSE(gAlphaMaskProgram.isAvailable());
    REQUIRE(gAlphaMaskProgram.mShaderFiles.empty());
    BakingLoaderScope manager;
    std::string error;
    auto device = ALMetalDevice::create(&error);
    INFO(error); REQUIRE(device);
    const auto submissions = ALMetalDevice::committedCommandBufferCount();
    LLVertexBuffer::setMetalBufferAuthority(true);
    LLGLSLShader::sIndexedTextureChannels = 7;

    LLBakingShaderMgr::sInitialized = true;
    gRenderCapabilities.reset();
    CHECK_FALSE(manager.setShaders());
    gRenderCapabilities.initializeOpenGL(gGLManager);
    CHECK_FALSE(manager.setShaders());
    gRenderCapabilities.initializeMetal(device->info());
    LLBakingShaderMgr::sInitialized = false;
    CHECK_FALSE(manager.setShaders());
    LLBakingShaderMgr::sInitialized = true;
    gGLManager.mInited = true; // Guard only: never create or invoke a GL context.
    CHECK_FALSE(manager.setShaders());
    gGLManager.mInited = false;
    CHECK_FALSE(gAlphaMaskProgram.isAvailable());
    CHECK(LLGLSLShader::sIndexedTextureChannels == 7);

    for (unsigned reload = 0; reload < 2; ++reload)
    {
        REQUIRE(manager.setShaders());
        CHECK(LLBakingShaderMgr::instance() == &manager);
        CHECK(gAlphaMaskProgram.isAvailable());
        CHECK(gAlphaMaskProgram.isNativeConfigured());
        CHECK_FALSE(gAlphaMaskProgram.isComplete());
        CHECK(gAlphaMaskProgram.mShaderLevel == 2);
        CHECK(gAlphaMaskProgram.mAttributeMask == (LLVertexBuffer::MAP_VERTEX |
              LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR));
        REQUIRE(gAlphaMaskProgram.mShaderFiles.size() == 2);
        CHECK(gAlphaMaskProgram.mShaderFiles[0].first == "interface/alphamaskV.glsl");
        CHECK(gAlphaMaskProgram.mShaderFiles[0].second == GL_VERTEX_SHADER);
        CHECK(gAlphaMaskProgram.mShaderFiles[1].first == "interface/alphamaskF.glsl");
        CHECK(gAlphaMaskProgram.mShaderFiles[1].second == GL_FRAGMENT_SHADER);
        CHECK(LLGLSLShader::sIndexedTextureChannels == 1);
        CHECK_FALSE(gGLManager.mInited);
    }
    manager.unloadShaders();
    CHECK_FALSE(gAlphaMaskProgram.isAvailable());
    CHECK(gAlphaMaskProgram.mAttributeMask == 0);
    manager.unloadShaders();
    CHECK_FALSE(gAlphaMaskProgram.isAvailable());
    REQUIRE(manager.setShaders()); // Reload after teardown also restores the layout.
    CHECK(gAlphaMaskProgram.mAttributeMask == (LLVertexBuffer::MAP_VERTEX |
          LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR));
    CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);
}
