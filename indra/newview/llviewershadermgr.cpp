/**
 * @file llviewershadermgr.cpp
 * @brief Viewer shader manager implementation.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright © 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */


#include "llviewerprecompiledheaders.h"

#include "alrendercapabilities.h"
#include "aluniformbuffer.h"
#include "llviewershadermgr.h"

#include "alwatersurface.h"
#include "llviewercontrol.h"

#include "llrender.h"
#include "llenvironment.h"
#include "llerrorcontrol.h"
#include "llworld.h"
#include "llsky.h"

#include "pipeline.h"

#include <fmt/format.h>
#include "llviewerwindow.h"
#include "llwindow.h"

#include "lljoint.h"
#include "llskinningutil.h"


// Lots of STL stuff in here, using namespace std to keep things more readable
using std::vector;
using std::pair;
using std::make_pair;
using std::string;

bool                LLViewerShaderMgr::sInitialized = false;

LLVector4           gShinyOrigin;

// The inputs the load running builds for.
static const ALShaderInputs& load_inputs()
{
    return LLViewerShaderMgr::instance()->inputs();
}

//utility shaders
LLGLSLShader    gOcclusionProgram;
LLGLSLShader    gOcclusionCubeProgram;
LLGLSLShader    gGlowCombineProgram;
LLGLSLShader    gTwoTextureCompareProgram;
LLGLSLShader    gOneTextureFilterProgram;
LLGLSLShader    gDebugProgram;
LLGLSLShader    gClipProgram;
LLGLSLShader    gAlphaMaskProgram;
LLGLSLShader    gBenchmarkProgram;
LLGLSLShader    gCopyProgram;
LLGLSLShader    gCopyDepthProgram;
LLGLSLShader    gDrawColorProgram;

//object shaders
LLGLSLShader        gObjectPreviewProgram;
LLGLSLShader        gPhysicsPreviewProgram;
LLGLSLShader        gObjectAlphaMaskNoColorProgram;

//avatar shader handles
LLGLSLShader        gAvatarProgram;
LLGLSLShader        gImpostorProgram;

// Effects Shaders
LLGLSLShader            gGlowProgram;
LLGLSLShader            gGlowExtractProgram;
LLGLSLShader            gBloomExtractProgram;
LLGLSLShader            gBloomDownsampleProgram;
LLGLSLShader            gBloomDownsampleFirstProgram;
LLGLSLShader            gBloomUpsampleProgram;
LLGLSLShader            gBloomCompositeProgram;
LLGLSLShader            gCrossFilterProgram;

// Deferred rendering shaders
LLGLSLShader            gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram;
LLGLSLShader            gDeferredTerrainProgram;
#if !LL_DARWIN
LLGLSLShader            gDeferredPostProgram;
LLGLSLShader            gDeferredPostProgramNoNear;
LLGLSLShader            gDeferredPostProgramShaped;
LLGLSLShader            gDeferredPostProgramNoNearShaped;
LLGLSLShader            gDeferredDoFCombineProgram;
LLGLSLShader            gDeferredDoFCombineProgramNoNear;
#endif
LLGLSLShader            gExposureProgram;
LLGLSLShader            gExposureProgramNoFade;
LLGLSLShader            gExposureHistogramProgram;
LLGLSLShader            gLuminanceProgram;
LLGLSLShader            gLensFlareStateProgram;
#if !LL_DARWIN
LLGLSLShader            gFXAAProgram[4];
LLGLSLShader            gSMAAEdgeDetectProgram[4];
LLGLSLShader            gSMAABlendWeightsProgram[4];
LLGLSLShader            gSMAANeighborhoodBlendProgram[4];
LLGLSLShader            gCASProgram;
#endif
LLGLSLShader            gDeferredPostNoDoFProgram;
LLGLSLShader            gBlitWithEffectsProgram;
#if !LL_DARWIN
LLGLSLShader            gBlitWithEffectsSharpenProgram;
#endif
LLGLSLShader            gCGGammaProgram;
LLGLSLShader            gCGLegacyGammaProgram;
LLGLSLShader            gCGTonemapProgram;
LLGLSLShader            gCGTonemapLegacyGammaProgram;
LLGLSLShader            gCGColorgradeGammaProgram;
LLGLSLShader            gCGColorgradeLegacyGammaProgram;
LLGLSLShader            gCGTonemapColorgradeProgram;
LLGLSLShader            gCGTonemapColorgradeLegacyGammaProgram;

LLGLSLShader            gHUDPBROpaqueProgram;
LLGLSLShader            gDeferredPBRTerrainProgram[TERRAIN_PAINT_TYPE_COUNT];
LLGLSLShader            gDeferredPBRTerrainAlphaMaskProgram[TERRAIN_PAINT_TYPE_COUNT];

// Mirror corners are only reachable during the hero-probe mirror pass, which RenderMirrors
// gates entirely; toggling it reloads the programs (handleMirrorsChanged), so
// building them while it is off would be dozens of programs nothing can bind.
static U32 mirror_variant()
{
    return load_inputs().mGlobal.mMirrors ? (U32)LLGLSLShader::VARIANT_MIRROR : 0u;
}


static void add_common_permutations(LLGLSLShader* shader)
{
    if (load_inputs().mGlobal.mEmissive)
    {
        shader->addPermutation("HAS_EMISSIVE", "1");
    }
}

// Every program that tessellates terrain. Without dynamic LOD the levels are pinned to the
// grid (lldrawpoolterrain sends density 0), and only equal spacing cuts an edge at level 16
// into 16 equal segments, on the grid's vertices; fractional odd spacing rounds 16 up to 17.
static void add_terrain_tess_permutations(LLGLSLShader* shader, bool pinned)
{
    if (pinned)
    {
        shader->addPermutation("TERRAIN_TESS_PINNED", "1");
    }
}

#ifdef SHOW_ASSERT
// Return true if no two live programs share a name. sInstances tracks every program that
// created successfully, so this covers the whole population without a registration list --
// including the rigged variants, which the old list-walk only reached through the bases
// someone had remembered to register.
static bool no_redundant_shaders()
{
    std::set<std::string> names;
    for (LLGLSLShader* shader : LLGLSLShader::sInstances)
    {
        if (!names.insert(shader->mName).second)
        {
            LL_WARNS("Shader") << "Redundant shader: " << shader->mName << LL_ENDL;
            return false;
        }
    }
    return true;
}
#endif


LLViewerShaderMgr::LLViewerShaderMgr() :
    mShaderLevel(SHADER_COUNT, 0),
    mMaxAvatarShaderLevel(0)
{
}

LLViewerShaderMgr::~LLViewerShaderMgr()
{
    mShaderLevel.clear();
}

void LLViewerShaderMgr::finalizeShaderList()
{
    // make sure there are no redundancies
    llassert(no_redundant_shaders());
}

// static
LLViewerShaderMgr * LLViewerShaderMgr::instance()
{
    if(NULL == sInstance)
    {
        sInstance = new LLViewerShaderMgr();
    }

    return static_cast<LLViewerShaderMgr*>(sInstance);
}

// static
void LLViewerShaderMgr::releaseInstance()
{
    if (sInstance != NULL)
    {
        delete sInstance;
        sInstance = NULL;
    }
}

void LLViewerShaderMgr::initAttribsAndUniforms(void)
{
    if (mReservedAttribs.empty())
    {
        LLShaderMgr::initAttribsAndUniforms();
    }
}


//============================================================================
// Set Levels

S32 LLViewerShaderMgr::getShaderLevel(S32 type)
{
    return mShaderLevel[type];
}

//============================================================================
// Shader Management

bool LLViewerShaderMgr::setShaders(const ALShaderInputs& inputs, const ALRenderApplication& application)
{
    LL_PROFILE_ZONE_SCOPED;
    static bool reentrance = false;
    if (!gPipeline.mInitialized || !sInitialized || reentrance)
    {
        return false;
    }
    if (!gRenderCapabilities.isMetal() || gGLManager.mInited)
    {
        LL_WARNS("ShaderLoading") << "Native Metal renderer is required" << LL_ENDL;
        return false;
    }

    // Latch one coherent input set before configuring pass identities and
    // reconciling the native resources which consume it.
    mInputs = inputs;
    ALUniformBuffer::sUpdateMode = ALUniformBuffer::clampUpdateMode(
        gSavedSettings.getS32("AlchemyRenderUBOUpdateMode"));
    LLPipeline::updateReverseZ(mInputs.mGlobal.mReverseZ);
    LLRender::sGBufferNormHDR = mInputs.mGlobal.mGBufferNormHDR;
    LLRender::sCompute = mInputs.mLocal.mCompute;
    LLGLSLShader::sIndexedTextureChannels = mInputs.mGlobal.mIndexedTextureChannels;
    LLGLSLShader::sIndexedGLTFChannels = mInputs.mLocal.mIndexedGLTFChannels;

    reentrance = true;
    const bool configured = setMetalShaders(application);
    reentrance = false;
    return configured;
}

bool LLViewerShaderMgr::setMetalShaders(
    const ALRenderApplication& application)
{
    const LLTimer timer;
    LLGLSLShader::sLinked = 0;
    LLGLSLShader::sKept = 0;
    mFailedPrograms.clear();
    ++LLGLSLShader::sLoadGeneration;

    initAttribsAndUniforms();

    const bool rebuild =
        application.mPrograms == ALRenderApplication::Programs::Rebuilt;
    if (rebuild)
    {
        unloadShaders();
    }
    else
    {
        for (LLGLSLShader* shader : LLGLSLShader::sConfigured)
        {
            if (!shader->mOwnedVariant)
            {
                shader->clearConfiguration();
            }
        }
    }

    LLPipeline::sRenderGlow = gSavedSettings.getBOOL("RenderGlow");
    if (gViewerWindow)
    {
        gViewerWindow->setCursor(UI_CURSOR_WAIT);
    }

    std::fill(mShaderLevel.begin(), mShaderLevel.end(), 0);
    mMaxAvatarShaderLevel = 0;
    mShaderLevel[SHADER_LIGHTING] = 3;
    mShaderLevel[SHADER_INTERFACE] = 2;
    mShaderLevel[SHADER_ENVIRONMENT] = 2;
    mShaderLevel[SHADER_WATER] = 3;
    mShaderLevel[SHADER_OBJECT] = 2;
    mShaderLevel[SHADER_EFFECT] = 2;
    mShaderLevel[SHADER_WINDLIGHT] = 2;
    mShaderLevel[SHADER_DEFERRED] = 3;
    mMaxAvatarShaderLevel = 3;
    mShaderLevel[SHADER_AVATAR] = 1;

    // Native MSL modules own their compile constants and bindings. Keep only
    // the latched input digest in the semantic configuration key; do not
    // enumerate, hash, load or compile the app_settings GLSL tree.
    LLGLSLShader::sGlobalDefines.clear();
    LLGLSLShader::sSharedObjectsKey = mInputs.mGlobal.digest();

    LL_INFOS("ShaderLoading")
        << "Configuring native Metal render-pass identities without GLSL"
        << LL_ENDL;

    loadShadersEffects();
    loadShadersInterface();
    loadShadersObject();
    loadShadersAvatar();
    loadShadersDeferred();

    std::vector<LLGLSLShader*> unasked;
    for (LLGLSLShader* shader : LLGLSLShader::sInstances)
    {
        if (!shader->mOwnedVariant &&
            shader->mLoadGeneration != LLGLSLShader::sLoadGeneration)
        {
            unasked.push_back(shader);
        }
    }
    for (LLGLSLShader* shader : unasked)
    {
        shader->unload();
    }

    gPipeline.mShadersLoaded = mFailedPrograms.empty();
    if (gPipeline.mShadersLoaded && gSky.mVOSkyp.notNull())
    {
        gSky.mVOSkyp->forceSkyUpdate();
    }

    if (!mFailedPrograms.empty())
    {
        std::string names;
        for (const std::string& name : mFailedPrograms)
        {
            names += names.empty() ? name : ", " + name;
        }
        LL_WARNS("ShaderLoading")
            << mFailedPrograms.size()
            << " native pass identities could not be configured: " << names
            << LL_ENDL;
    }

    if (gViewerWindow)
    {
        gViewerWindow->setCursor(UI_CURSOR_ARROW);
    }
    finalizeShaderList();

    LL_INFOS("ShaderLoading")
        << "Native Metal pass identities "
        << (rebuild ? "rebuilt" : "reloaded") << ": "
        << LLGLSLShader::sLinked << " configured, "
        << LLGLSLShader::sKept << " kept, " << unasked.size()
        << " retired; no GLSL source or cache touched, in "
        << fmt::format("{:.1f}",
                       timer.getElapsedTimeF64().value() * 1000.0)
        << " ms" << LL_ENDL;
    LL_PROFILE_PLOT("native passes configured",
                    static_cast<int64_t>(LLGLSLShader::sLinked));
    LL_PROFILE_PLOT("native passes kept",
                    static_cast<int64_t>(LLGLSLShader::sKept));
    return gPipeline.mShadersLoaded;
}

void LLViewerShaderMgr::unloadShaders()
{
    while (!LLGLSLShader::sInstances.empty())
    {
        LLGLSLShader* shader = *(LLGLSLShader::sInstances.begin());
        shader->unload();
    }

    mShaderLevel[SHADER_LIGHTING] = 0;
    mShaderLevel[SHADER_OBJECT] = 0;
    mShaderLevel[SHADER_AVATAR] = 0;
    mShaderLevel[SHADER_ENVIRONMENT] = 0;
    mShaderLevel[SHADER_WATER] = 0;
    mShaderLevel[SHADER_INTERFACE] = 0;
    mShaderLevel[SHADER_EFFECT] = 0;
    mShaderLevel[SHADER_WINDLIGHT] = 0;

    gPipeline.mShadersLoaded = false;
}


void LLViewerShaderMgr::loadShadersEffects()
{
    LL_PROFILE_ZONE_SCOPED;
    if (mShaderLevel[SHADER_EFFECT] == 0)
    {
        gGlowProgram.unload();
        gGlowExtractProgram.unload();
        gBloomExtractProgram.unload();
        gBloomDownsampleProgram.unload();
        gBloomDownsampleFirstProgram.unload();
        gBloomUpsampleProgram.unload();
        gBloomCompositeProgram.unload();
        gCrossFilterProgram.unload();
        return;
    }

    {
        gGlowProgram.mName = "Glow Shader (Post)";
        gGlowProgram.mShaderFiles.clear();
        gGlowProgram.mShaderFiles.push_back(make_pair("effects/glowV.glsl", GL_VERTEX_SHADER));
        gGlowProgram.mShaderFiles.push_back(make_pair("effects/glowF.glsl", GL_FRAGMENT_SHADER));
        gGlowProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        if (!built(gGlowProgram.createShader(), gGlowProgram))
        {
            LLPipeline::sRenderGlow = false;
        }
    }

    {
        const bool use_glow_noise = mInputs.mLocal.mGlowNoise;
        const std::string glow_noise_label = use_glow_noise ? " (+Noise)" : "";

        gGlowExtractProgram.mName = llformat("Glow Extract Shader (Post)%s", glow_noise_label.c_str());
        gGlowExtractProgram.mShaderFiles.clear();
        gGlowExtractProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gGlowExtractProgram.mShaderFiles.push_back(make_pair("effects/glowExtractF.glsl", GL_FRAGMENT_SHADER));
        gGlowExtractProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];

        if (use_glow_noise)
        {
            gGlowExtractProgram.addPermutation("HAS_NOISE", "1");
        }

        if (!built(gGlowExtractProgram.createShader(), gGlowExtractProgram))
        {
            LLPipeline::sRenderGlow = false;
        }
    }

    const bool bloom_halation = mInputs.mLocal.mBloomHalation;

    {
        gBloomExtractProgram.mName = "HDR Bloom Extract";
        gBloomExtractProgram.mShaderFiles.clear();
        gBloomExtractProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gBloomExtractProgram.mShaderFiles.push_back(make_pair("effects/bloomExtractF.glsl", GL_FRAGMENT_SHADER));
        gBloomExtractProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        if (bloom_halation)
        {
            gBloomExtractProgram.addPermutation("BLOOM_HALATION", "1");
        }
        built(gBloomExtractProgram.createShader(), gBloomExtractProgram);
    }

    {
        gBloomDownsampleFirstProgram.mName = "HDR Bloom Downsample (First)";
        gBloomDownsampleFirstProgram.mShaderFiles.clear();
        gBloomDownsampleFirstProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gBloomDownsampleFirstProgram.mShaderFiles.push_back(make_pair("effects/bloomDownsampleF.glsl", GL_FRAGMENT_SHADER));
        gBloomDownsampleFirstProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        gBloomDownsampleFirstProgram.addPermutation("FIRST_DOWNSAMPLE", "1");
        built(gBloomDownsampleFirstProgram.createShader(), gBloomDownsampleFirstProgram);
    }

    {
        gBloomDownsampleProgram.mName = "HDR Bloom Downsample";
        gBloomDownsampleProgram.mShaderFiles.clear();
        gBloomDownsampleProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gBloomDownsampleProgram.mShaderFiles.push_back(make_pair("effects/bloomDownsampleF.glsl", GL_FRAGMENT_SHADER));
        gBloomDownsampleProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        built(gBloomDownsampleProgram.createShader(), gBloomDownsampleProgram);
    }

    {
        gBloomUpsampleProgram.mName = "HDR Bloom Upsample";
        gBloomUpsampleProgram.mShaderFiles.clear();
        gBloomUpsampleProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gBloomUpsampleProgram.mShaderFiles.push_back(make_pair("effects/bloomUpsampleF.glsl", GL_FRAGMENT_SHADER));
        gBloomUpsampleProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        built(gBloomUpsampleProgram.createShader(), gBloomUpsampleProgram);
    }

    {
        gCrossFilterProgram.mName = "Cross Screen Filter";
        gCrossFilterProgram.mShaderFiles.clear();
        gCrossFilterProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gCrossFilterProgram.mShaderFiles.push_back(make_pair("effects/crossFilterF.glsl", GL_FRAGMENT_SHADER));
        gCrossFilterProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        gCrossFilterProgram.clearPermutations();
        gCrossFilterProgram.addPermutation("CROSS_TAPS", std::to_string(CROSS_FILTER_TAPS));
        built(gCrossFilterProgram.createShader(), gCrossFilterProgram);
    }

    {
        gBloomCompositeProgram.mName = "HDR Bloom Composite";
        gBloomCompositeProgram.mShaderFiles.clear();
        gBloomCompositeProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gBloomCompositeProgram.mShaderFiles.push_back(make_pair("effects/bloomCompositeF.glsl", GL_FRAGMENT_SHADER));
        gBloomCompositeProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        if (bloom_halation)
        {
            gBloomCompositeProgram.addPermutation("BLOOM_HALATION", "1");
        }
        built(gBloomCompositeProgram.createShader(), gBloomCompositeProgram);
    }
}

void LLViewerShaderMgr::loadShadersDeferred()
{
    LL_PROFILE_ZONE_SCOPED;
    if (mShaderLevel[SHADER_DEFERRED] == 0)
    {
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.unload();
        gDeferredTerrainProgram.unload();
#if !LL_DARWIN
        gDeferredPostProgram.unload();
        gDeferredPostProgramNoNear.unload();
        gDeferredPostProgramShaped.unload();
        gDeferredPostProgramNoNearShaped.unload();
        gDeferredDoFCombineProgram.unload();
        gDeferredDoFCombineProgramNoNear.unload();
#endif
        gExposureProgram.unload();
        gExposureProgramNoFade.unload();
        gExposureHistogramProgram.unload();
        gLuminanceProgram.unload();
        gLensFlareStateProgram.unload();

#if !LL_DARWIN
        for (auto i = 0; i < 4; ++i)
        {
            gFXAAProgram[i].unload();
            gSMAAEdgeDetectProgram[i].unload();
            gSMAABlendWeightsProgram[i].unload();
            gSMAANeighborhoodBlendProgram[i].unload();
        }
        gCASProgram.unload();
#endif

        LLGLSLShader::sIndexedLegacyMaterials = false;

        gHUDPBROpaqueProgram.unload();
        for (U32 paint_type = 0; paint_type < TERRAIN_PAINT_TYPE_COUNT; ++paint_type)
        {
            gDeferredPBRTerrainProgram[paint_type].unload();
            gDeferredPBRTerrainAlphaMaskProgram[paint_type].unload();
        }

        return;
    }

    {
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mName = "Deferred Diffuse Non-Indexed Alpha Mask No Color Shader";
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderFiles.clear();
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderFiles.push_back(make_pair("deferred/diffuseNoColorV.glsl", GL_VERTEX_SHADER));
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderFiles.push_back(make_pair("deferred/diffuseAlphaMaskNoColorF.glsl", GL_FRAGMENT_SHADER));
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        add_common_permutations(&gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram);
        built(gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.createShader(mirror_variant()), gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram);
    }

    // Native G-buffer and shadow capture accept indexed legacy batches
    // directly. Keep formation enabled when the shared slot width exists.
    LLGLSLShader::sIndexedLegacyMaterials =
        LLGLSLShader::sIndexedGLTFChannels >= 2;

    {
        gHUDPBROpaqueProgram.mName = "HUD PBR Opaque Shader";
        gHUDPBROpaqueProgram.mFeatures.hasSrgb = true;
        gHUDPBROpaqueProgram.mShaderFiles.clear();
        gHUDPBROpaqueProgram.mShaderFiles.push_back(make_pair("deferred/pbropaqueV.glsl", GL_VERTEX_SHADER));
        gHUDPBROpaqueProgram.mShaderFiles.push_back(make_pair("deferred/pbropaqueF.glsl", GL_FRAGMENT_SHADER));
        gHUDPBROpaqueProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gHUDPBROpaqueProgram.clearPermutations();
        gHUDPBROpaqueProgram.addPermutation("IS_HUD", "1");

        add_common_permutations(&gHUDPBROpaqueProgram);

        built(gHUDPBROpaqueProgram.createShader(), gHUDPBROpaqueProgram);

    }

    {
        const S32 detail = mInputs.mGlobal.mTerrainPBRDetail;
        const S32 mapping = mInputs.mGlobal.mTerrainPlanarSampleCount;
        // Faceted terrain shading is a fragment-stage choice, not a surface-data one: a
        // per-triangle normal has no per-vertex representation on a mesh whose vertices are
        // shared, so pbrterrainF derives it from position derivatives instead. See
        // terrain_geometric_normal().
        const bool flat_normals = mInputs.mLocal.mTerrainPBRNormals;
        // Read by pbrterrainUtilF, a shared object, so the define that matters is the global one
        // set above; the program's copy keeps its name and hash honest.
        const bool hex_tiling = mInputs.mGlobal.mTerrainHexTiling;
        // A region whose materials include a masked one is drawn by the program that can discard; every other by one
        // that cannot (LLDrawPoolTerrain::renderDeferred).
        for (U32 program = 0; program < 2 * TERRAIN_PAINT_TYPE_COUNT; ++program)
        {
            const U32 paint_type = program % TERRAIN_PAINT_TYPE_COUNT;
            const bool alpha_mask = program >= TERRAIN_PAINT_TYPE_COUNT;
            LLGLSLShader* shader = alpha_mask ? &gDeferredPBRTerrainAlphaMaskProgram[paint_type] : &gDeferredPBRTerrainProgram[paint_type];
            shader->mName = llformat("Deferred PBR Terrain Shader %d %s %s %s%s%s",
                    detail,
                    (paint_type == TERRAIN_PAINT_TYPE_PBR_PAINTMAP ? "paintmap" : "heightmap-with-noise"),
                    (mapping == 1 ? "flat" : "triplanar"),
                    (flat_normals ? "faceted" : "smooth"),
                    (hex_tiling ? " hex" : ""),
                    (alpha_mask ? " alpha mask" : ""));
            shader->mFeatures.hasSrgb = true;
            shader->mFeatures.isAlphaLighting = true;
            shader->mFeatures.calculatesAtmospherics = true;
            shader->mFeatures.hasAtmospherics = true;
            shader->mFeatures.hasGamma = true;
            shader->mFeatures.hasTransport = true;
            shader->mFeatures.isPBRTerrain = true;
            shader->mFeatures.hasTessellatedTerrain = true;

            shader->mShaderFiles.clear();
            shader->mShaderFiles.push_back(make_pair("deferred/terrainPatchV.glsl", GL_VERTEX_SHADER));
            shader->mShaderFiles.push_back(make_pair("deferred/terrainTC.glsl", GL_TESS_CONTROL_SHADER));
            shader->mShaderFiles.push_back(make_pair("deferred/pbrterrainTE.glsl", GL_TESS_EVALUATION_SHADER));
            shader->mShaderFiles.push_back(make_pair("deferred/pbrterrainF.glsl", GL_FRAGMENT_SHADER));
            shader->mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            shader->addPermutation("TERRAIN_PBR_DETAIL", llformat("%d", detail));
            shader->addPermutation("TERRAIN_PAINT_TYPE", llformat("%d", paint_type));
            shader->addPermutation("TERRAIN_PLANAR_TEXTURE_SAMPLE_COUNT", llformat("%d", mapping));
            if (flat_normals)
            {
                shader->addPermutation("TERRAIN_FLAT_NORMALS", "1");
            }
            if (hex_tiling)
            {
                shader->addPermutation("TERRAIN_HEX_TILING", "1");
            }
            if (alpha_mask)
            {
                shader->addPermutation("HAS_ALPHA_MASK", "1");
            }

            add_common_permutations(shader);
            add_terrain_tess_permutations(shader, mInputs.mLocal.mTerrainTessPinned);

            built(shader->createShader(mirror_variant()), *shader);
        }
    }

    {
        // The projection count and hex tiling apply to the four-texture terrain as they do to
        // the PBR one: terrainF blends its detail textures through pbrterrainUtilF's weights,
        // slices and cells, which isPBRTerrain links in.
        const S32 mapping = mInputs.mGlobal.mTerrainPlanarSampleCount;
        const bool hex_tiling = mInputs.mGlobal.mTerrainHexTiling;
        gDeferredTerrainProgram.mName = llformat("Deferred Terrain Shader %s%s",
                (mapping == 1 ? "flat" : "triplanar"),
                (hex_tiling ? " hex" : ""));
        gDeferredTerrainProgram.mFeatures.hasSrgb = true;
        gDeferredTerrainProgram.mFeatures.isAlphaLighting = true;
        gDeferredTerrainProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredTerrainProgram.mFeatures.hasAtmospherics = true;
        gDeferredTerrainProgram.mFeatures.hasGamma = true;
        gDeferredTerrainProgram.mFeatures.isPBRTerrain = true;

        gDeferredTerrainProgram.mFeatures.hasTessellatedTerrain = true;

        gDeferredTerrainProgram.mShaderFiles.clear();
        gDeferredTerrainProgram.mShaderFiles.push_back(make_pair("deferred/terrainPatchV.glsl", GL_VERTEX_SHADER));
        gDeferredTerrainProgram.mShaderFiles.push_back(make_pair("deferred/terrainTC.glsl", GL_TESS_CONTROL_SHADER));
        gDeferredTerrainProgram.mShaderFiles.push_back(make_pair("deferred/terrainTE.glsl", GL_TESS_EVALUATION_SHADER));
        gDeferredTerrainProgram.mShaderFiles.push_back(make_pair("deferred/terrainF.glsl", GL_FRAGMENT_SHADER));
        gDeferredTerrainProgram.clearPermutations();
        gDeferredTerrainProgram.addPermutation("TERRAIN_PLANAR_TEXTURE_SAMPLE_COUNT", llformat("%d", mapping));
        if (hex_tiling)
        {
            gDeferredTerrainProgram.addPermutation("TERRAIN_HEX_TILING", "1");
        }

        add_common_permutations(&gDeferredTerrainProgram);
        add_terrain_tess_permutations(&gDeferredTerrainProgram, mInputs.mLocal.mTerrainTessPinned);

        gDeferredTerrainProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gDeferredTerrainProgram.createShader(mirror_variant()), gDeferredTerrainProgram);
    }

    {
        gExposureProgram.mName = "Exposure";
        gExposureProgram.mFeatures.hasSrgb = true;
        gExposureProgram.mFeatures.isDeferred = true;
        gExposureProgram.mShaderFiles.clear();
        gExposureProgram.clearPermutations();
        gExposureProgram.addPermutation("USE_LAST_EXPOSURE", "1");
        gExposureProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gExposureProgram.mShaderFiles.push_back(make_pair("deferred/exposureF.glsl", GL_FRAGMENT_SHADER));
        gExposureProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gExposureProgram.createShader(), gExposureProgram);
    }

    {
        gExposureProgramNoFade.mName = "Exposure (no fade)";
        gExposureProgramNoFade.mFeatures.hasSrgb = true;
        gExposureProgramNoFade.mFeatures.isDeferred = true;
        gExposureProgramNoFade.mShaderFiles.clear();
        gExposureProgramNoFade.clearPermutations();
        gExposureProgramNoFade.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gExposureProgramNoFade.mShaderFiles.push_back(make_pair("deferred/exposureF.glsl", GL_FRAGMENT_SHADER));
        gExposureProgramNoFade.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gExposureProgramNoFade.createShader(), gExposureProgramNoFade);
    }

    {
        // The exposure meter's histogram: one point per part of a meter grid texel,
        // summed into 64 bins by additive blending.
        gExposureHistogramProgram.mName = "Exposure Histogram";
        gExposureHistogramProgram.mShaderFiles.clear();
        gExposureHistogramProgram.clearPermutations();
        gExposureHistogramProgram.mShaderFiles.push_back(make_pair("deferred/exposureHistogramV.glsl", GL_VERTEX_SHADER));
        gExposureHistogramProgram.mShaderFiles.push_back(make_pair("deferred/exposureHistogramF.glsl", GL_FRAGMENT_SHADER));
        gExposureHistogramProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gExposureHistogramProgram.createShader(), gExposureHistogramProgram);
    }

    {
        // Sun coverage, colour and the filtered flare drive, 2x1, read by the
        // colour-correct programs through uLensFlareStateMap.
        gLensFlareStateProgram.mName = "Lens Flare State";
        gLensFlareStateProgram.mShaderFiles.clear();
        gLensFlareStateProgram.clearPermutations();
        gLensFlareStateProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gLensFlareStateProgram.mShaderFiles.push_back(make_pair("alchemy/lensFlareStateF.glsl", GL_FRAGMENT_SHADER));
        gLensFlareStateProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gLensFlareStateProgram.createShader(), gLensFlareStateProgram);
    }

    {
        gLuminanceProgram.mName = "Exposure Meter Grid";
        gLuminanceProgram.mShaderFiles.clear();
        gLuminanceProgram.clearPermutations();
        gLuminanceProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gLuminanceProgram.mShaderFiles.push_back(make_pair("deferred/luminanceF.glsl", GL_FRAGMENT_SHADER));
        gLuminanceProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gLuminanceProgram.createShader(), gLuminanceProgram);
    }

#if !LL_DARWIN
    {
        std::vector<std::pair<std::string, std::string>> quality_levels = { {"12", "Low"},
                                                                             {"23", "Medium"},
                                                                             {"28", "High"},
                                                                             {"39", "Ultra"} };
        int i = 0;
        bool failed = false;
        for (const auto& quality_pair : quality_levels)
        {
            gFXAAProgram[i].mName = llformat("FXAA Shader (%s)", quality_pair.second.c_str());
            gFXAAProgram[i].mFeatures.isDeferred = true;
            gFXAAProgram[i].mShaderFiles.clear();
            gFXAAProgram[i].mShaderFiles.push_back(make_pair("deferred/postDeferredV.glsl", GL_VERTEX_SHADER));
            gFXAAProgram[i].mShaderFiles.push_back(make_pair("deferred/fxaaF.glsl", GL_FRAGMENT_SHADER));

            gFXAAProgram[i].clearPermutations();
            gFXAAProgram[i].addPermutation("FXAA_QUALITY_PRESET", quality_pair.first);
            gFXAAProgram[i].addPermutation("FXAA_GLSL_400", "1");

            gFXAAProgram[i].mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            if (!built(gFXAAProgram[i].createShader(), gFXAAProgram[i]))
            {
                LL_WARNS() << "Failed to create shader '" << gFXAAProgram[i].mName << "', disabling!" << LL_ENDL;
                // continue as if this shader never happened
                failed = true;
                break;
            }
            ++i;
        }

        // The pass runs only while the first of these is complete (LLPipeline), so a set short of one goes whole and
        // the pass is skipped, where a program that draws nothing would leave the frame unwritten.
        if (failed)
        {
            for (auto i = 0; i < 4; ++i)
            {
                gFXAAProgram[i].unload();
            }
        }
    }

    {
        std::vector<std::pair<std::string, std::string>> quality_levels = { {"SMAA_PRESET_LOW", "Low"},
                                                                             {"SMAA_PRESET_MEDIUM", "Medium"},
                                                                             {"SMAA_PRESET_HIGH", "High"},
                                                                          {"SMAA_PRESET_ULTRA", "Ultra"} };
        const bool smaa_predication = mInputs.mLocal.mSMAAPredication;
        int i = 0;
        bool failed = false;
        for (const auto& smaa_pair : quality_levels)
        {
            std::map<std::string, std::string> defines;
            defines.emplace("SMAA_GLSL_4", "1");
            defines.emplace("SMAA_PREDICATION", smaa_predication ? "1" : "0");
            defines.emplace("SMAA_REPROJECTION", "0");
            defines.emplace(smaa_pair.first, "1");

            gSMAAEdgeDetectProgram[i].mName = llformat("SMAA Edge Detection (%s)", smaa_pair.second.c_str());
            gSMAAEdgeDetectProgram[i].mFeatures.isDeferred = true;

            gSMAAEdgeDetectProgram[i].clearPermutations();
            gSMAAEdgeDetectProgram[i].addPermutations(defines);

            gSMAAEdgeDetectProgram[i].mShaderFiles.clear();
            gSMAAEdgeDetectProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAAEdgeDetectF.glsl", GL_FRAGMENT_SHADER));
            gSMAAEdgeDetectProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAAEdgeDetectV.glsl", GL_VERTEX_SHADER));
            gSMAAEdgeDetectProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAA.glsl", GL_FRAGMENT_SHADER));
            gSMAAEdgeDetectProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAA.glsl", GL_VERTEX_SHADER));
            gSMAAEdgeDetectProgram[i].mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            if (!built(gSMAAEdgeDetectProgram[i].createShader(), gSMAAEdgeDetectProgram[i]))
            {
                LL_WARNS() << "Failed to create shader '" << gSMAAEdgeDetectProgram[i].mName << "', disabling!" << LL_ENDL;
                // continue as if this shader never happened
                failed = true;
                break;
            }

            gSMAABlendWeightsProgram[i].mName = llformat("SMAA Blending Weights (%s)", smaa_pair.second.c_str());
            gSMAABlendWeightsProgram[i].mFeatures.isDeferred = true;

            gSMAABlendWeightsProgram[i].clearPermutations();
            gSMAABlendWeightsProgram[i].addPermutations(defines);

            gSMAABlendWeightsProgram[i].mShaderFiles.clear();
            gSMAABlendWeightsProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAABlendWeightsF.glsl", GL_FRAGMENT_SHADER));
            gSMAABlendWeightsProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAABlendWeightsV.glsl", GL_VERTEX_SHADER));
            gSMAABlendWeightsProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAA.glsl", GL_FRAGMENT_SHADER));
            gSMAABlendWeightsProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAA.glsl", GL_VERTEX_SHADER));
            gSMAABlendWeightsProgram[i].mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            if (!built(gSMAABlendWeightsProgram[i].createShader(), gSMAABlendWeightsProgram[i]))
            {
                LL_WARNS() << "Failed to create shader '" << gSMAABlendWeightsProgram[i].mName << "', disabling!" << LL_ENDL;
                // continue as if this shader never happened
                failed = true;
                break;
            }

            gSMAANeighborhoodBlendProgram[i].mName = llformat("SMAA Neighborhood Blending (%s)", smaa_pair.second.c_str());
            gSMAANeighborhoodBlendProgram[i].mFeatures.isDeferred = true;

            gSMAANeighborhoodBlendProgram[i].clearPermutations();
            gSMAANeighborhoodBlendProgram[i].addPermutations(defines);

            gSMAANeighborhoodBlendProgram[i].mShaderFiles.clear();
            gSMAANeighborhoodBlendProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAANeighborhoodBlendF.glsl", GL_FRAGMENT_SHADER));
            gSMAANeighborhoodBlendProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAANeighborhoodBlendV.glsl", GL_VERTEX_SHADER));
            gSMAANeighborhoodBlendProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAA.glsl", GL_FRAGMENT_SHADER));
            gSMAANeighborhoodBlendProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAA.glsl", GL_VERTEX_SHADER));
            gSMAANeighborhoodBlendProgram[i].mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            if (!built(gSMAANeighborhoodBlendProgram[i].createShader(), gSMAANeighborhoodBlendProgram[i]))
            {
                LL_WARNS() << "Failed to create shader '" << gSMAANeighborhoodBlendProgram[i].mName << "', disabling!" << LL_ENDL;
                // continue as if this shader never happened
                failed = true;
                break;
            }
            ++i;
        }

        // As FXAA's: the pass runs only while the first edge program is complete.
        if (failed)
        {
            for (auto i = 0; i < 4; ++i)
            {
                gSMAAEdgeDetectProgram[i].unload();
                gSMAABlendWeightsProgram[i].unload();
                gSMAANeighborhoodBlendProgram[i].unload();
            }
        }
    }

    {
        gCASProgram.mName = "Contrast Adaptive Sharpening Shader";
        gCASProgram.mFeatures.hasSrgb = true;
        gCASProgram.mShaderFiles.clear();
        gCASProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCASProgram.mShaderFiles.push_back(make_pair("deferred/CASF.glsl", GL_FRAGMENT_SHADER));
        gCASProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        if (!built(gCASProgram.createShader(), gCASProgram))
        {
            LL_WARNS() << "Failed to create shader '" << gCASProgram.mName << "', disabling!" << LL_ENDL;
        }
    }
#endif

#if !LL_DARWIN
    // Without HDR, retained LLPipeline::renderDoF's blur is in the post
    // chain's RGBA8, so the gather and combine programs keep it square-root
    // encoded.
    const bool dof_sqrt_encoded = !mInputs.mLocal.mHDR;

    {
        // Four DoF gather variants across two orthogonal axes. Both are value
        // tests in the shader, so the "off" build defines the symbol as "0"
        // rather than omitting it -- omitting it would make #if FRONT_BLUR a
        // compile error rather than a false branch.
        struct PostVariant
        {
            LLGLSLShader* shader;
            const char*   name;
            const char*   front_blur;
            const char*   dof_shaped;
        };

        const PostVariant post_variants[] =
        {
            { &gDeferredPostProgram,             "Deferred Post Shader",                            "1", "0" },
            { &gDeferredPostProgramNoNear,       "Deferred Post Shader No Near Blur",               "0", "0" },
            { &gDeferredPostProgramShaped,       "Deferred Post Shader Shaped",                     "1", "1" },
            { &gDeferredPostProgramNoNearShaped, "Deferred Post Shader No Near Blur Shaped",        "0", "1" },
        };

        for (const PostVariant& variant : post_variants)
        {
            variant.shader->mName = variant.name;
            variant.shader->mFeatures.isDeferred = true;
            variant.shader->mShaderFiles.clear();
            variant.shader->mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
            variant.shader->mShaderFiles.push_back(make_pair("deferred/postDeferredF.glsl", GL_FRAGMENT_SHADER));
            variant.shader->mShaderFiles.push_back(make_pair("deferred/dofCoFUtilF.glsl", GL_FRAGMENT_SHADER));
            variant.shader->mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            variant.shader->clearPermutations();
            variant.shader->addPermutation("FRONT_BLUR", variant.front_blur);
            variant.shader->addPermutation("DOF_SHAPED", variant.dof_shaped);
            if (dof_sqrt_encoded)
            {
                variant.shader->addPermutation("DOF_SQRT_ENCODED", "1");
            }

            built(variant.shader->createShader(), *variant.shader);
        }
    }

    {
        gDeferredDoFCombineProgram.mName = "Deferred DoFCombine Shader";
        gDeferredDoFCombineProgram.mFeatures.isDeferred = true;
        gDeferredDoFCombineProgram.mShaderFiles.clear();
        gDeferredDoFCombineProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredDoFCombineProgram.mShaderFiles.push_back(make_pair("deferred/dofCombineF.glsl", GL_FRAGMENT_SHADER));
        gDeferredDoFCombineProgram.mShaderFiles.push_back(make_pair("deferred/dofCoFUtilF.glsl", GL_FRAGMENT_SHADER));
        gDeferredDoFCombineProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredDoFCombineProgram.clearPermutations();
        gDeferredDoFCombineProgram.addPermutation("FRONT_BLUR", "1");
        if (dof_sqrt_encoded)
        {
            gDeferredDoFCombineProgram.addPermutation("DOF_SQRT_ENCODED", "1");
        }
        built(gDeferredDoFCombineProgram.createShader(), gDeferredDoFCombineProgram);
    }

    {
        gDeferredDoFCombineProgramNoNear.mName = "Deferred DoFCombine Shader No Near Blur";
        gDeferredDoFCombineProgramNoNear.mFeatures.isDeferred = true;
        gDeferredDoFCombineProgramNoNear.mShaderFiles.clear();
        gDeferredDoFCombineProgramNoNear.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredDoFCombineProgramNoNear.mShaderFiles.push_back(make_pair("deferred/dofCombineF.glsl", GL_FRAGMENT_SHADER));
        gDeferredDoFCombineProgramNoNear.mShaderFiles.push_back(make_pair("deferred/dofCoFUtilF.glsl", GL_FRAGMENT_SHADER));
        gDeferredDoFCombineProgramNoNear.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredDoFCombineProgramNoNear.clearPermutations();
        gDeferredDoFCombineProgramNoNear.addPermutation("FRONT_BLUR", "0");
        if (dof_sqrt_encoded)
        {
            gDeferredDoFCombineProgramNoNear.addPermutation("DOF_SQRT_ENCODED", "1");
        }
        built(gDeferredDoFCombineProgramNoNear.createShader(), gDeferredDoFCombineProgramNoNear);
    }
#endif

    {
        gDeferredPostNoDoFProgram.mName = "Deferred Post NoDoF Shader";
        gDeferredPostNoDoFProgram.mFeatures.isDeferred = true;
        gDeferredPostNoDoFProgram.mShaderFiles.clear();
        gDeferredPostNoDoFProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredPostNoDoFProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoDoFF.glsl", GL_FRAGMENT_SHADER));
        gDeferredPostNoDoFProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gDeferredPostNoDoFProgram.createShader(), gDeferredPostNoDoFProgram);
    }

    {
        gBlitWithEffectsProgram.mName = "Blit With Post Effects Shader";
        gBlitWithEffectsProgram.mFeatures.isDeferred = true;
        gBlitWithEffectsProgram.mFeatures.hasPostEffects = true;
        gBlitWithEffectsProgram.mShaderFiles.clear();
        gBlitWithEffectsProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gBlitWithEffectsProgram.mShaderFiles.push_back(make_pair("alchemy/blitWithEffectsF.glsl", GL_FRAGMENT_SHADER));
        gBlitWithEffectsProgram.clearPermutations();
        if (mInputs.mLocal.mHDR)
        {
            gBlitWithEffectsProgram.addPermutation("DITHER", "1");
        }
        gBlitWithEffectsProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gBlitWithEffectsProgram.createShader(), gBlitWithEffectsProgram);
    }

#if !LL_DARWIN
    {
        // The same blit sharpening the frame as it samples it, CASF.glsl's filter linked in. While it is incomplete
        // the frame is sharpened by CAS's own pass.
        gBlitWithEffectsSharpenProgram.mName = "Blit With Post Effects Shader (Sharpened)";
        gBlitWithEffectsSharpenProgram.mFeatures.isDeferred = true;
        gBlitWithEffectsSharpenProgram.mFeatures.hasPostEffects = true;
        gBlitWithEffectsSharpenProgram.mShaderFiles.clear();
        gBlitWithEffectsSharpenProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gBlitWithEffectsSharpenProgram.mShaderFiles.push_back(make_pair("alchemy/blitWithEffectsF.glsl", GL_FRAGMENT_SHADER));
        gBlitWithEffectsSharpenProgram.mShaderFiles.push_back(make_pair("deferred/CASF.glsl", GL_FRAGMENT_SHADER));
        gBlitWithEffectsSharpenProgram.clearPermutations();
        gBlitWithEffectsSharpenProgram.addPermutation("CAS_LIBRARY", "1");
        if (mInputs.mLocal.mHDR)
        {
            gBlitWithEffectsSharpenProgram.addPermutation("DITHER", "1");
        }
        gBlitWithEffectsSharpenProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gBlitWithEffectsSharpenProgram.createShader(), gBlitWithEffectsSharpenProgram);
    }
#endif

    // HDR-only: the bloom pyramid is allocated in the HDR path, so the tonemap
    // shader variants fold the bloom composite inline. Halation rides in the
    // bloom alpha channel when RenderBloomHalation is on — both settings trigger
    // shader rebuilds, so reading them at compile time stays in sync with the
    // bloom pyramid format.
    const bool hdr_enabled         = mInputs.mLocal.mHDR;
    const bool bloom_halation_perm = mInputs.mLocal.mBloomHalation;

    {
        gCGGammaProgram.mName = "CG Gamma Shader";
        gCGGammaProgram.mFeatures.isDeferred = true;
        gCGGammaProgram.mFeatures.hasPostEffects = true;
        gCGGammaProgram.mShaderFiles.clear();
        gCGGammaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGGammaProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGGammaProgram.clearPermutations();
        gCGGammaProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGGammaProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGGammaProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGGammaProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGGammaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gCGGammaProgram.createShader(), gCGGammaProgram);
    }

    {
        gCGLegacyGammaProgram.mName = "CG Legacy Gamma Shader";
        gCGLegacyGammaProgram.mFeatures.isDeferred = true;
        gCGLegacyGammaProgram.mFeatures.hasPostEffects = true;
        gCGLegacyGammaProgram.mShaderFiles.clear();
        gCGLegacyGammaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGLegacyGammaProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGLegacyGammaProgram.clearPermutations();
        gCGLegacyGammaProgram.addPermutation("LEGACY_GAMMA", "1");
        gCGLegacyGammaProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGLegacyGammaProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGLegacyGammaProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGLegacyGammaProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGLegacyGammaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gCGLegacyGammaProgram.createShader(), gCGLegacyGammaProgram);
    }

    {
        gCGColorgradeGammaProgram.mName = "CG Color Grade Gamma Shader";
        gCGColorgradeGammaProgram.mFeatures.isDeferred = true;
        gCGColorgradeGammaProgram.mFeatures.hasColorGrade = true;
        gCGColorgradeGammaProgram.mFeatures.hasPostEffects = true;
        gCGColorgradeGammaProgram.mShaderFiles.clear();
        gCGColorgradeGammaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGColorgradeGammaProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGColorgradeGammaProgram.clearPermutations();
        gCGColorgradeGammaProgram.addPermutation("COLOR_GRADE", "1");
        gCGColorgradeGammaProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGColorgradeGammaProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGColorgradeGammaProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGColorgradeGammaProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGColorgradeGammaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gCGColorgradeGammaProgram.createShader(), gCGColorgradeGammaProgram);
    }

    {
        gCGColorgradeLegacyGammaProgram.mName = "CG Color Grade Legacy Gamma Shader";
        gCGColorgradeLegacyGammaProgram.mFeatures.isDeferred = true;
        gCGColorgradeLegacyGammaProgram.mFeatures.hasColorGrade = true;
        gCGColorgradeLegacyGammaProgram.mFeatures.hasPostEffects = true;
        gCGColorgradeLegacyGammaProgram.mShaderFiles.clear();
        gCGColorgradeLegacyGammaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGColorgradeLegacyGammaProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGColorgradeLegacyGammaProgram.clearPermutations();
        gCGColorgradeLegacyGammaProgram.addPermutation("COLOR_GRADE", "1");
        gCGColorgradeLegacyGammaProgram.addPermutation("LEGACY_GAMMA", "1");
        gCGColorgradeLegacyGammaProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGColorgradeLegacyGammaProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGColorgradeLegacyGammaProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGColorgradeLegacyGammaProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGColorgradeLegacyGammaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gCGColorgradeLegacyGammaProgram.createShader(), gCGColorgradeLegacyGammaProgram);
    }

    {
        gCGTonemapProgram.mName = "CG Tonemap Shader";
        gCGTonemapProgram.mFeatures.isDeferred = true;
        gCGTonemapProgram.mFeatures.hasTonemap = true;
        gCGTonemapProgram.mFeatures.hasPostEffects = true;
        gCGTonemapProgram.mShaderFiles.clear();
        gCGTonemapProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGTonemapProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGTonemapProgram.clearPermutations();
        gCGTonemapProgram.addPermutation("TONEMAP", "1");
        gCGTonemapProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGTonemapProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGTonemapProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGTonemapProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGTonemapProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gCGTonemapProgram.createShader(), gCGTonemapProgram);
    }

    {
        gCGTonemapLegacyGammaProgram.mName = "CG Tonemap Legacy Gamma Shader";
        gCGTonemapLegacyGammaProgram.mFeatures.isDeferred = true;
        gCGTonemapLegacyGammaProgram.mFeatures.hasTonemap = true;
        gCGTonemapLegacyGammaProgram.mFeatures.hasPostEffects = true;
        gCGTonemapLegacyGammaProgram.mShaderFiles.clear();
        gCGTonemapLegacyGammaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGTonemapLegacyGammaProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGTonemapLegacyGammaProgram.clearPermutations();
        gCGTonemapLegacyGammaProgram.addPermutation("LEGACY_GAMMA", "1");
        gCGTonemapLegacyGammaProgram.addPermutation("TONEMAP", "1");
        gCGTonemapLegacyGammaProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGTonemapLegacyGammaProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGTonemapLegacyGammaProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGTonemapLegacyGammaProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGTonemapLegacyGammaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gCGTonemapLegacyGammaProgram.createShader(), gCGTonemapLegacyGammaProgram);
    }

    {
        gCGTonemapColorgradeProgram.mName = "CG Tonemap Color Grade Shader";
        gCGTonemapColorgradeProgram.mFeatures.isDeferred = true;
        gCGTonemapColorgradeProgram.mFeatures.hasTonemap = true;
        gCGTonemapColorgradeProgram.mFeatures.hasColorGrade = true;
        gCGTonemapColorgradeProgram.mFeatures.hasPostEffects = true;
        gCGTonemapColorgradeProgram.mShaderFiles.clear();
        gCGTonemapColorgradeProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGTonemapColorgradeProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGTonemapColorgradeProgram.clearPermutations();
        gCGTonemapColorgradeProgram.addPermutation("COLOR_GRADE", "1");
        gCGTonemapColorgradeProgram.addPermutation("TONEMAP", "1");
        gCGTonemapColorgradeProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGTonemapColorgradeProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGTonemapColorgradeProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGTonemapColorgradeProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGTonemapColorgradeProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gCGTonemapColorgradeProgram.createShader(), gCGTonemapColorgradeProgram);
    }

    {
        gCGTonemapColorgradeLegacyGammaProgram.mName = "CG Tonemap Color Grade Legacy Gamma Shader";
        gCGTonemapColorgradeLegacyGammaProgram.mFeatures.isDeferred = true;
        gCGTonemapColorgradeLegacyGammaProgram.mFeatures.hasTonemap = true;
        gCGTonemapColorgradeLegacyGammaProgram.mFeatures.hasColorGrade = true;
        gCGTonemapColorgradeLegacyGammaProgram.mFeatures.hasPostEffects = true;
        gCGTonemapColorgradeLegacyGammaProgram.mShaderFiles.clear();
        gCGTonemapColorgradeLegacyGammaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGTonemapColorgradeLegacyGammaProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGTonemapColorgradeLegacyGammaProgram.clearPermutations();
        gCGTonemapColorgradeLegacyGammaProgram.addPermutation("COLOR_GRADE", "1");
        gCGTonemapColorgradeLegacyGammaProgram.addPermutation("LEGACY_GAMMA", "1");
        gCGTonemapColorgradeLegacyGammaProgram.addPermutation("TONEMAP", "1");
        gCGTonemapColorgradeLegacyGammaProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGTonemapColorgradeLegacyGammaProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGTonemapColorgradeLegacyGammaProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGTonemapColorgradeLegacyGammaProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGTonemapColorgradeLegacyGammaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gCGTonemapColorgradeLegacyGammaProgram.createShader(), gCGTonemapColorgradeLegacyGammaProgram);
    }

}

void LLViewerShaderMgr::loadShadersObject()
{
    LL_PROFILE_ZONE_SCOPED;
    {
        gObjectAlphaMaskNoColorProgram.mName = "No color alpha mask Shader";
        gObjectAlphaMaskNoColorProgram.mFeatures.calculatesLighting = true;
        gObjectAlphaMaskNoColorProgram.mFeatures.calculatesAtmospherics = true;
        gObjectAlphaMaskNoColorProgram.mFeatures.hasGamma = true;
        gObjectAlphaMaskNoColorProgram.mFeatures.hasAtmospherics = true;
        gObjectAlphaMaskNoColorProgram.mFeatures.hasLighting = true;
        gObjectAlphaMaskNoColorProgram.mFeatures.hasAlphaMask = true;
        gObjectAlphaMaskNoColorProgram.mShaderFiles.clear();
        gObjectAlphaMaskNoColorProgram.mShaderFiles.push_back(make_pair("objects/simpleNoColorV.glsl", GL_VERTEX_SHADER));
        gObjectAlphaMaskNoColorProgram.mShaderFiles.push_back(make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER));
        gObjectAlphaMaskNoColorProgram.mShaderLevel = mShaderLevel[SHADER_OBJECT];
        built(gObjectAlphaMaskNoColorProgram.createShader(), gObjectAlphaMaskNoColorProgram);
    }

    {
        gImpostorProgram.mName = "Impostor Shader";
        gImpostorProgram.mFeatures.hasSrgb = true;
        gImpostorProgram.mShaderFiles.clear();
        gImpostorProgram.mShaderFiles.push_back(make_pair("objects/impostorV.glsl", GL_VERTEX_SHADER));
        gImpostorProgram.mShaderFiles.push_back(make_pair("objects/impostorF.glsl", GL_FRAGMENT_SHADER));
        gImpostorProgram.mShaderLevel = mShaderLevel[SHADER_OBJECT];
        built(gImpostorProgram.createShader(), gImpostorProgram);
    }

    {
        gObjectPreviewProgram.mName = "Object Preview Shader";
        gObjectPreviewProgram.mShaderFiles.clear();
        gObjectPreviewProgram.mShaderFiles.push_back(make_pair("objects/previewV.glsl", GL_VERTEX_SHADER));
        gObjectPreviewProgram.mShaderFiles.push_back(make_pair("objects/previewF.glsl", GL_FRAGMENT_SHADER));
        gObjectPreviewProgram.mShaderLevel = mShaderLevel[SHADER_OBJECT];
        built(gObjectPreviewProgram.createShader(LLGLSLShader::VARIANT_RIGGED), gObjectPreviewProgram);
        gObjectPreviewProgram.forEachVariant([](LLGLSLShader& s) { s.mFeatures.hasLighting = true; });
    }

    {
        gPhysicsPreviewProgram.mName = "Preview Physics Shader";
        gPhysicsPreviewProgram.mFeatures.calculatesLighting = false;
        gPhysicsPreviewProgram.mFeatures.calculatesAtmospherics = false;
        gPhysicsPreviewProgram.mFeatures.hasGamma = false;
        gPhysicsPreviewProgram.mFeatures.hasAtmospherics = false;
        gPhysicsPreviewProgram.mFeatures.hasLighting = false;
        gPhysicsPreviewProgram.mShaderFiles.clear();
        gPhysicsPreviewProgram.mShaderFiles.push_back(make_pair("objects/previewPhysicsV.glsl", GL_VERTEX_SHADER));
        gPhysicsPreviewProgram.mShaderFiles.push_back(make_pair("objects/previewPhysicsF.glsl", GL_FRAGMENT_SHADER));
        gPhysicsPreviewProgram.mShaderLevel = mShaderLevel[SHADER_OBJECT];
        built(gPhysicsPreviewProgram.createShader(), gPhysicsPreviewProgram);
        gPhysicsPreviewProgram.mFeatures.hasLighting = false;
    }
}

void LLViewerShaderMgr::loadShadersAvatar()
{
    LL_PROFILE_ZONE_SCOPED;
#if 1 // DEPRECATED -- forward rendering is deprecated
    {
        gAvatarProgram.mName = "Avatar Shader";
        gAvatarProgram.mFeatures.hasSkinning = true;
        gAvatarProgram.mFeatures.calculatesAtmospherics = true;
        gAvatarProgram.mFeatures.calculatesLighting = true;
        gAvatarProgram.mFeatures.hasGamma = true;
        gAvatarProgram.mFeatures.hasAtmospherics = true;
        gAvatarProgram.mFeatures.hasLighting = true;
        gAvatarProgram.mFeatures.hasAlphaMask = true;
        gAvatarProgram.mShaderFiles.clear();
        gAvatarProgram.mShaderFiles.push_back(make_pair("avatar/avatarV.glsl", GL_VERTEX_SHADER));
        gAvatarProgram.mShaderFiles.push_back(make_pair("avatar/avatarF.glsl", GL_FRAGMENT_SHADER));
        gAvatarProgram.mShaderLevel = mShaderLevel[SHADER_AVATAR];
        built(gAvatarProgram.createShader(), gAvatarProgram);

        /// Keep track of avatar levels
        if (gAvatarProgram.mShaderLevel != mShaderLevel[SHADER_AVATAR])
        {
            mMaxAvatarShaderLevel = mShaderLevel[SHADER_AVATAR] = gAvatarProgram.mShaderLevel;
        }
    }
#endif
}

void LLViewerShaderMgr::loadShadersInterface()
{
    LL_PROFILE_ZONE_SCOPED;
    {
        gUIProgram.mName = "UI Shader";
        gUIProgram.mShaderFiles.clear();
        gUIProgram.mShaderFiles.push_back(make_pair("interface/uiV.glsl", GL_VERTEX_SHADER));
        gUIProgram.mShaderFiles.push_back(make_pair("interface/uiF.glsl", GL_FRAGMENT_SHADER));
        gUIProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        gUIProgram.setNativeAttributeMask(LLVertexBuffer::MAP_VERTEX |
                                          LLVertexBuffer::MAP_TEXCOORD0 |
                                          LLVertexBuffer::MAP_COLOR);
        // Native text shadow state is carried by LLRender's ordered UI batch.
        built(gUIProgram.createShader(), gUIProgram);
    }

    {
        gGlowCombineProgram.mName = "Glow Combine Shader";
        gGlowCombineProgram.mShaderFiles.clear();
        gGlowCombineProgram.mShaderFiles.push_back(make_pair("interface/glowcombineV.glsl", GL_VERTEX_SHADER));
        gGlowCombineProgram.mShaderFiles.push_back(make_pair("interface/glowcombineF.glsl", GL_FRAGMENT_SHADER));
        gGlowCombineProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        built(gGlowCombineProgram.createShader(), gGlowCombineProgram);
    }

#ifdef LL_WINDOWS
    {
        gTwoTextureCompareProgram.mName = "Two Texture Compare Shader";
        gTwoTextureCompareProgram.mShaderFiles.clear();
        gTwoTextureCompareProgram.mShaderFiles.push_back(make_pair("interface/twotexturecompareV.glsl", GL_VERTEX_SHADER));
        gTwoTextureCompareProgram.mShaderFiles.push_back(make_pair("interface/twotexturecompareF.glsl", GL_FRAGMENT_SHADER));
        gTwoTextureCompareProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        built(gTwoTextureCompareProgram.createShader(), gTwoTextureCompareProgram);
    }

    {
        gOneTextureFilterProgram.mName = "One Texture Filter Shader";
        gOneTextureFilterProgram.mShaderFiles.clear();
        gOneTextureFilterProgram.mShaderFiles.push_back(make_pair("interface/onetexturefilterV.glsl", GL_VERTEX_SHADER));
        gOneTextureFilterProgram.mShaderFiles.push_back(make_pair("interface/onetexturefilterF.glsl", GL_FRAGMENT_SHADER));
        gOneTextureFilterProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        built(gOneTextureFilterProgram.createShader(), gOneTextureFilterProgram);
    }
#endif

    {
        gSolidColorProgram.mName = "Solid Color Shader";
        gSolidColorProgram.mShaderFiles.clear();
        gSolidColorProgram.mShaderFiles.push_back(make_pair("interface/solidcolorV.glsl", GL_VERTEX_SHADER));
        gSolidColorProgram.mShaderFiles.push_back(make_pair("interface/solidcolorF.glsl", GL_FRAGMENT_SHADER));
        gSolidColorProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        gSolidColorProgram.setNativeAttributeMask(LLVertexBuffer::MAP_VERTEX |
                                                  LLVertexBuffer::MAP_TEXCOORD0);
        built(gSolidColorProgram.createShader(), gSolidColorProgram);
    }

    {
        gOcclusionProgram.mName = "Occlusion Shader";
        gOcclusionProgram.mShaderFiles.clear();
        gOcclusionProgram.mShaderFiles.push_back(make_pair("interface/occlusionV.glsl", GL_VERTEX_SHADER));
        gOcclusionProgram.mShaderFiles.push_back(make_pair("interface/occlusionF.glsl", GL_FRAGMENT_SHADER));
        gOcclusionProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        add_common_permutations(&gOcclusionProgram);
        built(gOcclusionProgram.createShader(LLGLSLShader::VARIANT_RIGGED), gOcclusionProgram);
    }

    {
        gOcclusionCubeProgram.mName = "Occlusion Cube Shader";
        gOcclusionCubeProgram.mShaderFiles.clear();
        gOcclusionCubeProgram.mShaderFiles.push_back(make_pair("interface/occlusionCubeV.glsl", GL_VERTEX_SHADER));
        gOcclusionCubeProgram.mShaderFiles.push_back(make_pair("interface/occlusionF.glsl", GL_FRAGMENT_SHADER));
        gOcclusionCubeProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        add_common_permutations(&gOcclusionCubeProgram);
        built(gOcclusionCubeProgram.createShader(), gOcclusionCubeProgram);
    }

    {
        gDebugProgram.mName = "Debug Shader";
        // Native setup cannot reflect debugV.glsl's position attribute.
        // Immediate edit-tool rings use this position-only uniform-color pass.
        gDebugProgram.setNativeAttributeMask(LLVertexBuffer::MAP_VERTEX);
        gDebugProgram.mShaderFiles.clear();
        gDebugProgram.mShaderFiles.push_back(make_pair("interface/debugV.glsl", GL_VERTEX_SHADER));
        gDebugProgram.mShaderFiles.push_back(make_pair("interface/debugF.glsl", GL_FRAGMENT_SHADER));
        gDebugProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        built(gDebugProgram.createShader(LLGLSLShader::VARIANT_RIGGED), gDebugProgram);
    }

    {
        gClipProgram.mName = "Clip Shader";
        gClipProgram.mShaderFiles.clear();
        gClipProgram.mShaderFiles.push_back(make_pair("interface/clipV.glsl", GL_VERTEX_SHADER));
        gClipProgram.mShaderFiles.push_back(make_pair("interface/clipF.glsl", GL_FRAGMENT_SHADER));
        gClipProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        built(gClipProgram.createShader(), gClipProgram);
    }

    {
        gBenchmarkProgram.mName = "Benchmark Shader";
        gBenchmarkProgram.mShaderFiles.clear();
        gBenchmarkProgram.mShaderFiles.push_back(make_pair("interface/benchmarkV.glsl", GL_VERTEX_SHADER));
        gBenchmarkProgram.mShaderFiles.push_back(make_pair("interface/benchmarkF.glsl", GL_FRAGMENT_SHADER));
        gBenchmarkProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        built(gBenchmarkProgram.createShader(), gBenchmarkProgram);
    }

    {
        gCopyProgram.mName = "Copy Shader";
        gCopyProgram.mShaderFiles.clear();
        gCopyProgram.mShaderFiles.push_back(make_pair("interface/copyV.glsl", GL_VERTEX_SHADER));
        gCopyProgram.mShaderFiles.push_back(make_pair("interface/copyF.glsl", GL_FRAGMENT_SHADER));
        gCopyProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        built(gCopyProgram.createShader(), gCopyProgram);
    }

    {
        gCopyDepthProgram.mName = "Copy Depth Shader";
        gCopyDepthProgram.mShaderFiles.clear();
        gCopyDepthProgram.mShaderFiles.push_back(make_pair("interface/copyV.glsl", GL_VERTEX_SHADER));
        gCopyDepthProgram.mShaderFiles.push_back(make_pair("interface/copyF.glsl", GL_FRAGMENT_SHADER));
        gCopyDepthProgram.clearPermutations();
        gCopyDepthProgram.addPermutation("COPY_DEPTH", "1");
        gCopyDepthProgram.addPermutation("DEPTH_ONLY", "1");
        gCopyDepthProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        built(gCopyDepthProgram.createShader(), gCopyDepthProgram);
    }

    {
        gDrawColorProgram.mName = "Draw Color Shader";
        gDrawColorProgram.mShaderFiles.clear();
        gDrawColorProgram.mShaderFiles.push_back(make_pair("objects/simpleNoAtmosV.glsl", GL_VERTEX_SHADER));
        gDrawColorProgram.mShaderFiles.push_back(make_pair("objects/simpleColorF.glsl", GL_FRAGMENT_SHADER));
        gDrawColorProgram.clearPermutations();
        gDrawColorProgram.mShaderLevel = mShaderLevel[SHADER_OBJECT];
        built(gDrawColorProgram.createShader(), gDrawColorProgram);
    }

    {
        gAlphaMaskProgram.mName = "Alpha Mask Shader";
        gAlphaMaskProgram.mShaderFiles.clear();
        gAlphaMaskProgram.mShaderFiles.push_back(make_pair("interface/alphamaskV.glsl", GL_VERTEX_SHADER));
        gAlphaMaskProgram.mShaderFiles.push_back(make_pair("interface/alphamaskF.glsl", GL_FRAGMENT_SHADER));
        gAlphaMaskProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        // Native configuration has no GL reflection step.  The appearance
        // compositor and ordered world/UI capture both stream the same
        // position/UV/color layout declared by alphamaskV.glsl.
        gAlphaMaskProgram.setNativeAttributeMask(
            LLVertexBuffer::MAP_VERTEX |
            LLVertexBuffer::MAP_TEXCOORD0 |
            LLVertexBuffer::MAP_COLOR);
        built(gAlphaMaskProgram.createShader(), gAlphaMaskProgram);
    }

}



void LLViewerShaderMgr::updateShaderUniforms(LLGLSLShader * shader)
{
    if (LLEnvironment::instanceExists())
    {
        LLEnvironment::instance().updateShaderUniforms(shader);
    }
}

