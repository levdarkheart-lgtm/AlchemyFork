/**
 * @file alshader_native_lifecycle_test.cpp
 * @brief Native pass identities retain the original loader lifecycle.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, Alchemy Viewer Project
 * Licensed under the GNU Lesser General Public License version 2.1 only.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "../alrendercapabilities.h"
#include "../llglslshader.h"
#include "../llshadermgr.h"
#include "../llvertexbuffer.h"
#include "../../almetal/almetaldevice.h"
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <set>

namespace
{
// Original1eb44 hashes loader-authored features/defines and keeps identical
// programs/variant pointers across loads. Native code must keep that contract
// without opening the provenance paths below or manufacturing GL programs.
struct NativeShaderScope final : LLShaderMgr
{
    LLShaderMgr* previous_manager = sInstance;
    ALRenderCapabilities previous_capabilities = gRenderCapabilities;
    LLGLSLShader* previous_bound = LLGLSLShader::sCurBoundShaderPtr;
    U32 previous_program = LLGLSLShader::sCurBoundShader;
    U32 generation = LLGLSLShader::sLoadGeneration;
    U32 linked = LLGLSLShader::sLinked, kept = LLGLSLShader::sKept;
    LLUUID shared_key = LLGLSLShader::sSharedObjectsKey;
    LLGLSLShader::defines_map_t global_defines = LLGLSLShader::sGlobalDefines;
    bool authority = LLVertexBuffer::isMetalBufferAuthority();
    bool classic = LLRender::sClassicMode, mirror = LLRender::sMirrorPass;
    bool reverse = LLRender::sReverseZ, normal_hdr = LLRender::sGBufferNormHDR;
    std::unique_ptr<ALMetalDevice> device;

    NativeShaderScope() { sInstance = this; }
    void init()
    {
        REQUIRE_FALSE(gGLManager.mInited);
        std::string error;
        device = ALMetalDevice::create(&error); INFO(error); REQUIRE(device);
        gRenderCapabilities.initializeMetal(device->info());
        LLVertexBuffer::setMetalBufferAuthority(true);
        LLRender::sClassicMode = LLRender::sMirrorPass = false;
        LLGLSLShader::sLinked = LLGLSLShader::sKept = 0;
        LLGLSLShader::sGlobalDefines.clear();
        LLGLSLShader::sSharedObjectsKey.setNull();
        ++LLGLSLShader::sLoadGeneration;
    }
    ~NativeShaderScope() override
    {
        LLGLSLShader::sCurBoundShaderPtr = previous_bound;
        LLGLSLShader::sCurBoundShader = previous_program;
        LLGLSLShader::sLoadGeneration = generation;
        LLGLSLShader::sLinked = linked; LLGLSLShader::sKept = kept;
        LLGLSLShader::sSharedObjectsKey = shared_key;
        LLGLSLShader::sGlobalDefines = global_defines;
        LLVertexBuffer::setMetalBufferAuthority(authority);
        LLRender::sClassicMode = classic; LLRender::sMirrorPass = mirror;
        LLRender::sReverseZ = reverse; LLRender::sGBufferNormHDR = normal_hdr;
        gRenderCapabilities = previous_capabilities;
        sInstance = previous_manager;
    }
    void updateShaderUniforms(LLGLSLShader*) override {}
};

constexpr U32 layout = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 |
                       LLVertexBuffer::MAP_COLOR;
constexpr U32 axes = LLGLSLShader::VARIANT_RIGGED | LLGLSLShader::VARIANT_CLASSIC |
                     LLGLSLShader::VARIANT_MIRROR;

void configure(LLGLSLShader& shader)
{
    shader.mName = "Native lifecycle identity";
    shader.mShaderLevel = 2;
    shader.mShaderFiles = {{"nonexistent/lifecycleV.glsl", GL_VERTEX_SHADER},
                          {"nonexistent/lifecycleF.glsl", GL_FRAGMENT_SHADER}};
    shader.setNativeAttributeMask(layout);
    shader.mFeatures.hasAlphaMask = true;
    shader.mFeatures.hasAtmospherics = true;
    shader.addPermutation("NATIVE_LIFECYCLE", "1");
}
}

TEST_CASE("Native shader reload preserves the original eight-corner variant graph",
          "[llrender][almetal][shader-lifecycle]")
{
    NativeShaderScope scope; scope.init();
    LLGLSLShader shader;
    configure(shader);
    REQUIRE(shader.createShader(axes));
    const auto original_hash = shader.hash();
    const auto original_links = shader.linkCount();
    std::set<LLGLSLShader*> corners;
    shader.forEachVariant([&](LLGLSLShader& corner)
    {
        CHECK(corner.isAvailable()); CHECK(corner.isNativeConfigured());
        CHECK(corner.mAttributeMask == layout);
        CHECK(corner.mShaderLevel == 2);
        CHECK(corner.mShaderFiles == shader.mShaderFiles);
        CHECK(corner.mFeatures.hasAlphaMask);
        CHECK(corner.mFeatures.hasAtmospherics);
        CHECK(corner.mDefines.at("NATIVE_LIFECYCLE") == "1");
        CHECK(corner.mLoadGeneration == LLGLSLShader::sLoadGeneration);
        CHECK(LLGLSLShader::sInstances.contains(&corner));
        CHECK(LLGLSLShader::sConfigured.contains(&corner));
        CHECK(corners.insert(&corner).second);
    });
    REQUIRE(corners.size() == 8);
    for (bool mirror : {false, true})
        for (bool classic : {false, true})
            for (bool rigged : {false, true})
            {
                LLRender::sMirrorPass = mirror; LLRender::sClassicMode = classic;
                LLGLSLShader* selected = shader.selectVariant();
                REQUIRE(selected);
                if (rigged) selected = selected->riggedVariant();
                CHECK(corners.contains(selected));
                CHECK(selected->mDefines.contains("MIRROR_CLIP") == mirror);
                CHECK(selected->mDefines.contains("CLASSIC_MODE") == classic);
                CHECK(selected->mFeatures.hasObjectSkinning == rigged);
                CHECK(selected->mDefines.contains("HAS_SKIN") == rigged);
            }
    LLRender::sClassicMode = LLRender::sMirrorPass = false;
    shader.setMinimumAlpha(.375f);
    shader.clearConfiguration();
    CHECK(shader.isAvailable()); // Configuration clearing is not resource retirement.
    configure(shader);
    ++LLGLSLShader::sLoadGeneration;
    REQUIRE(shader.createShader(axes));
    CHECK(shader.hash() == original_hash);
    CHECK(shader.linkCount() == original_links);
    CHECK(shader.mLoadGeneration == LLGLSLShader::sLoadGeneration);
    CHECK(shader.getMinimumAlpha() == .375f);
    CHECK(LLGLSLShader::sKept == 8);
    std::set<LLGLSLShader*> kept_corners;
    shader.forEachVariant([&](LLGLSLShader& corner) { kept_corners.insert(&corner); });
    CHECK(kept_corners == corners);
    shader.unload();
    CHECK_FALSE(shader.isAvailable());
    CHECK_FALSE(shader.mRiggedVariant); CHECK_FALSE(shader.mClassicVariant);
    CHECK_FALSE(shader.mMirrorVariant);
    CHECK(shader.mShaderFiles.empty()); CHECK(shader.mDefines.empty());
    for (auto* corner : corners) CHECK_FALSE(LLGLSLShader::sInstances.contains(corner));
    CHECK(LLGLSLShader::sConfigured.contains(&shader)); // Until object destruction.
}

TEST_CASE("Native configuration changes invalidate metadata and failed reloads retire bindings",
          "[llrender][almetal][shader-lifecycle]")
{
    NativeShaderScope scope; scope.init();
    LLGLSLShader shader, failed;
    configure(shader); REQUIRE(shader.createShader());
    shader.bind(); shader.setMinimumAlpha(.625f);
    CHECK(LLGLSLShader::sCurBoundShaderPtr == &shader);
    const auto hash = shader.hash();
    const auto links = shader.linkCount();
    shader.setNativeAttributeMask(LLVertexBuffer::MAP_VERTEX);
    CHECK(shader.hash() != hash);
    REQUIRE(shader.createShader());
    CHECK(shader.linkCount() == links + 1);
    CHECK(shader.mAttributeMask == LLVertexBuffer::MAP_VERTEX);
    CHECK(shader.getMinimumAlpha() == LLGLSLShader::MINIMUM_ALPHA_UNSET);
    CHECK(LLGLSLShader::sCurBoundShaderPtr == nullptr);
    shader.bind();

    // An unconfigured identity must replace the previously bound valid one;
    // native admission then sees its missing metadata instead of stale shader state.
    CHECK_FALSE(failed.createShader());
    CHECK(LLGLSLShader::sConfigured.contains(&failed));
    CHECK_FALSE(LLGLSLShader::sInstances.contains(&failed));
    failed.bind();
    CHECK(LLGLSLShader::sCurBoundShaderPtr != &shader);
    CHECK((!LLGLSLShader::sCurBoundShaderPtr ||
           !LLGLSLShader::sCurBoundShaderPtr->isAvailable()));
    failed.unload();
    CHECK(LLGLSLShader::sCurBoundShaderPtr == nullptr);

    shader.mShaderFiles.push_back({"nonexistent/invalidC.glsl", GL_COMPUTE_SHADER});
    CHECK_FALSE(shader.createShader(axes));
    CHECK_FALSE(shader.isAvailable());
    CHECK_FALSE(shader.mRiggedVariant); CHECK_FALSE(shader.mClassicVariant);
    CHECK_FALSE(shader.mMirrorVariant);
    CHECK_FALSE(LLGLSLShader::sInstances.contains(&shader));
    shader.unload(); shader.unload();
}

TEST_CASE("Native metadata hash includes original inputs and destruction clears every registry",
          "[llrender][almetal][shader-lifecycle]")
{
    NativeShaderScope scope; scope.init();
    const auto instances = LLGLSLShader::sInstances;
    const auto configured = LLGLSLShader::sConfigured;
    const auto submissions = ALMetalDevice::committedCommandBufferCount();
    {
        LLGLSLShader shader;
        configure(shader); REQUIRE(shader.createShader(axes));
        auto hash = shader.hash();
        shader.mFeatures.hasGamma = true; CHECK(shader.hash() != hash);
        shader.mFeatures.hasGamma = false; CHECK(shader.hash() == hash);
        shader.addPermutation("SECOND_DEFINE", "1"); CHECK(shader.hash() != hash);
        shader.mDefines.erase("SECOND_DEFINE"); CHECK(shader.hash() == hash);
        LLGLSLShader::sGlobalDefines["GLOBAL_DEFINE"] = "1"; CHECK(shader.hash() != hash);
        LLGLSLShader::sGlobalDefines.clear(); CHECK(shader.hash() == hash);
        LLGLSLShader::sSharedObjectsKey = LLUUID("963f13c6-7426-4289-8523-3874bb8ca4ad");
        CHECK(shader.hash() != hash);
        LLGLSLShader::sSharedObjectsKey.setNull(); CHECK(shader.hash() == hash);
        LLRender::sReverseZ = !scope.reverse; CHECK(shader.hash() != hash);
        LLRender::sReverseZ = scope.reverse; CHECK(shader.hash() == hash);
        LLRender::sGBufferNormHDR = !scope.normal_hdr; CHECK(shader.hash() != hash);
        LLRender::sGBufferNormHDR = scope.normal_hdr; CHECK(shader.hash() == hash);
        shader.bind(); // Destructor also clears a bound identity and its owned corners.
    }
    CHECK(LLGLSLShader::sCurBoundShaderPtr == nullptr);
    CHECK(LLGLSLShader::sInstances == instances);
    CHECK(LLGLSLShader::sConfigured == configured);
    CHECK(ALMetalDevice::committedCommandBufferCount() == submissions);
}

TEST_CASE("Reserved native producer metadata keeps enum and name alignment",
          "[llrender][shader-lifecycle][shader-metadata]")
{
    // This is CPU metadata only. The original GL suite asserted the water
    // patch ordering name at its reserved index; no graphics context is
    // needed to preserve that producer contract. The retained native table
    // has 14 attributes and 503 uniforms (older 1eb44 had 547 uniforms
    // before displaced consumers were retired).
    NativeShaderScope scope;
    scope.initAttribsAndUniforms();
    REQUIRE(scope.mReservedAttribs.size() == 14);
    CHECK(scope.mReservedAttribs.size() == LLVertexBuffer::TYPE_MAX);
    CHECK(scope.mReservedAttribs[LLVertexBuffer::TYPE_VERTEX] == "position");
    CHECK(scope.mReservedAttribs[LLVertexBuffer::TYPE_TEXTURE_INDEX] == "texture_index");
    REQUIRE(scope.mReservedUniforms.size() == 503);
    CHECK(scope.mReservedUniforms.size() == LLShaderMgr::END_RESERVED_UNIFORMS);
    CHECK(scope.mReservedUniforms[LLShaderMgr::ENVIRONMENT_MAP] == "environmentMap");
    CHECK(scope.mReservedUniforms[LLShaderMgr::REFLECTION_PROBES] == "reflectionProbes");
    CHECK(scope.mReservedUniforms[LLShaderMgr::DEFERRED_EMISSIVE] == "emissiveRect");
    CHECK(scope.mReservedUniforms[LLShaderMgr::DEFERRED_NOISE] == "noiseMap");
    CHECK(scope.mReservedUniforms[LLShaderMgr::SELECTION_SCENE_DEPTH] == "selection_scene_depth");
    CHECK(scope.mReservedUniforms[LLShaderMgr::WATER_PATCH_ORDER] == "waterPatchOrder");
}
