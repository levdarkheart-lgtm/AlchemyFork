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

#include <boost/lexical_cast.hpp>

#include "alrendercapabilities.h"
#include "aluniformbuffer.h"
#include "hbxxh.h"
#include "llfeaturemanager.h"
#include "llviewershadermgr.h"

#include "alwatersurface.h"
#include "llviewercontrol.h"
#include "llversioninfo.h"

#include "llrender.h"
#include "llenvironment.h"
#include "llerrorcontrol.h"
#include "llworld.h"
#include "llsky.h"

#include "pipeline.h"

#include "llfile.h"

#include <filesystem>
#include <fmt/format.h>
#include "llviewerwindow.h"
#include "llwindow.h"

#include "lljoint.h"
#include "llskinningutil.h"

static LLStaticHashedString sTexture0("texture0");
static LLStaticHashedString sTexture1("texture1");
static LLStaticHashedString sTex0("tex0");
static LLStaticHashedString sTex1("tex1");
static LLStaticHashedString sDitherTex("dither_tex");

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
LLGLSLShader            gLensDirtGenProgram;

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
LLGLSLShader            gDeferredBufferVisualProgram;
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
// [RLVa:KB] - @setsphere
LLGLSLShader            gRlvSphereProgram;
// [/RLVa:KB]

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

// Digest of every shader source on disk, folded into the binary cache version.
//
// A program's own hash() covers its file PATHS, its defines and its features -- not the CONTENT
// of anything. Shared objects are worse off still: they are compiled once and attached by name,
// so nothing about them reaches the hash at all. Editing a shader therefore leaves every cached
// binary looking valid, and the only other input to the cache version is the viewer version,
// which does not move between local builds.
//
// Reading the tree costs one pass over a few hundred small files at startup, against reading
// them all again to compile anyway.
static std::string hash_shader_sources()
{
    LL_PROFILE_ZONE_SCOPED;

    std::vector<std::filesystem::path> files;
    std::error_code ec;
    const std::string root = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "shaders");

    for (std::filesystem::recursive_directory_iterator it(root, ec), end;
         !ec && it != end;
         it.increment(ec))
    {
        if (it->is_regular_file(ec))
        {
            files.push_back(it->path());
        }
    }

    if (ec || files.empty())
    {   // no digest is better than a wrong one: fall back to invalidating every run
        LL_WARNS("Shader") << "Could not enumerate " << root
                           << " to version the shader cache; treating it as stale" << LL_ENDL;
        return LLUUID::generateNewID().asString();
    }

    std::sort(files.begin(), files.end());  // directory iteration order is unspecified

    HBXXH128 hash_obj;
    for (const auto& file : files)
    {
        // relative, not absolute: the digest must not change with the install location
        hash_obj.update(file.lexically_relative(root).generic_string());

        llifstream in(file.string(), std::ios::binary);
        if (!in.is_open())
        {
            LL_WARNS("Shader") << "Could not read " << file.string()
                               << " to version the shader cache; treating it as stale" << LL_ENDL;
            return LLUUID::generateNewID().asString();
        }

        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        hash_obj.update(text);
    }
    return hash_obj.digest().asString();
}

// Compile a second copy of a shared object under `suffix`, for programs carrying `define`.
//
// Two things this does not do naively. It uses the level the BASE copy RESOLVED to rather than
// the level that was asked for -- loadShaderFile walks down the class directories when a level
// is missing, and a copy built from a different class file than the base would link mismatched
// declarations into one program. And it skips the compile entirely when the file that level
// selects does not mention `define`: deferred/reflectionProbeF.glsl reads CLASSIC_MODE at class3
// and not at class2, so below class3 the second compile yields a byte-identical object under a
// different key -- a full compile of a large source at every setShaders().
// LLShaderMgr::variantObjectKey falls back to the base object when no copy exists, so the skip
// is invisible to attach.
//
// The single-file test is sound because the loader has no #include: a shared source cannot pull
// the define in from anywhere else.
static bool load_axis_copy(LLViewerShaderMgr& mgr,
                           const std::vector<std::pair<std::string, S32> >& loaded,
                           const std::string& path, GLenum stage,
                           std::map<std::string, std::string>& defines,
                           const char* define, const char* suffix)
{
    auto it = std::find_if(loaded.begin(), loaded.end(),
                           [&path](const std::pair<std::string, S32>& e) { return e.first == path; });
    if (it == loaded.end())
    {   // the base pass must have loaded it, or there is nothing for this to be a copy OF
        LL_WARNS("Shader") << "No base object for " << path << "; cannot build its " << suffix
                           << " copy" << LL_ENDL;
        return false;
    }

    S32 level = it->second;

    bool varies = false;
    for (S32 gpu_class = level; gpu_class > 0; --gpu_class)
    {
        const std::string full = mgr.getShaderDirPrefix() + std::to_string(gpu_class)
                               + gDirUtilp->getDirDelimiter() + path;
        llifstream in(full, std::ios::binary);
        if (!in.is_open())
        {
            continue;
        }
        const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        varies = text.find(define) != std::string::npos;
        break;  // the first file that exists is the one loadShaderFile picked
    }

    if (!varies)
    {
        return true;
    }

    return mgr.loadShaderFile(path, level, stage, &defines, -1, path + suffix) != 0;
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

    const bool native_metal =
        gRenderCapabilities.isMetal() && !gGLManager.mInited;
    if (!native_metal && !gGLManager.mHasRequirements)
    {
        // The window refused the context and told the user why
        LL_INFOS("ShaderLoading") << "Not supported hardware/software" << LL_ENDL;
        return false;
    }

    // Every loader below reads these, and nothing else of the settings.
    mInputs = inputs;

    ALUniformBuffer::sUpdateMode = ALUniformBuffer::clampUpdateMode(gSavedSettings.getS32("AlchemyRenderUBOUpdateMode"));

    // Latch reverse-Z before anything downstream reads it: every shader compile below injects
    // REVERSE_Z from the latched state and the cache key folds it in, and the reconcile that
    // follows in the same application builds the depth format (mainDepthFormat) from it, so
    // clip control, clear depth, depth format and the shader define agree for the whole next
    // frame.
    LLPipeline::updateReverseZ(mInputs.mGlobal.mReverseZ);

    // The same for the G-buffer's normal layout: every compile below injects GBUFFER_NORM_HDR from this latch, the
    // cache key folds it in, and the resources planned from the same inputs give the normal attachment its format.
    LLRender::sGBufferNormHDR = mInputs.mGlobal.mGBufferNormHDR;

    // The same for compute: the shared objects compiled for its stage and every program built on it follow the switch
    // as it stands for this load.
    LLRender::sCompute = mInputs.mLocal.mCompute;

    // The texture ladder still forms viewer draw batches on Metal, but its
    // width comes from the native layout rather than a GL fragment-unit
    // query. These values are consumed by draw-pool formation and native
    // capture even though no GLSL program exists.
    LLGLSLShader::sIndexedTextureChannels =
        mInputs.mGlobal.mIndexedTextureChannels;
    LLGLSLShader::sIndexedGLTFChannels =
        mInputs.mLocal.mIndexedGLTFChannels;

    if (native_metal)
    {
        reentrance = true;
        const bool configured = setMetalShaders(application);
        reentrance = false;
        return configured;
    }

    {
        static LLCachedControl<bool> shader_cache_enabled(gSavedSettings, "RenderShaderCacheEnabled", true);
        static LLUUID old_cache_version;
        static LLUUID current_cache_version;
        if (current_cache_version.isNull())
        {
            HBXXH128 hash_obj;
            hash_obj.update(LLVersionInfo::instance().getVersion());
            hash_obj.update(hash_shader_sources());
            current_cache_version = hash_obj.digest();

            old_cache_version = LLUUID(gSavedSettings.getString("RenderShaderCacheVersion"));
            gSavedSettings.setString("RenderShaderCacheVersion", current_cache_version.asString());
        }

        // Intel's legacy drivers build every program from source (LLGLManager::isIntelLegacy).
        initShaderCache(
            shader_cache_enabled && !gGLManager.isIntelLegacy(),
            old_cache_version,
            current_cache_version,
            LLAppViewer::instance()->isSecondInstance());
    }

    // The texture units an indexed draw spreads its textures over, and the indexed material programs' slots, which the
    // loaders below lower where a program cannot be built. Zero slots, as on GLOn12, is the state a failed load leaves,
    // and the pools fall back to their scalar paths.
    if (gGLManager.mIsD3D12)
    {
        LL_INFOS("ShaderLoading") << "Indexed material batching disabled on GLOn12." << LL_ENDL;
    }

    reentrance = true;

    const LLTimer timer;
    LLGLSLShader::sLinked = 0;
    LLGLSLShader::sKept   = 0;
    mFailedPrograms.clear();
    ++LLGLSLShader::sLoadGeneration;

    initAttribsAndUniforms();

    const bool rebuild        = application.mPrograms == ALRenderApplication::Programs::Rebuilt;
    const bool shared_objects = rebuild || application.mSharedObjects;
    if (rebuild)
    {
        // Every program built from its sources, and the shared objects with them.
        unloadShaders();
    }
    else
    {
        // Every program configured again from nothing, as an unload leaves it, and still linked: createShader keeps
        // each its loader configures as it linked. A variant is its program's, kept or built again with it.
        for (LLGLSLShader* shader : LLGLSLShader::sConfigured)
        {
            if (!shader->mOwnedVariant)
            {
                shader->clearConfiguration();
            }
        }
        if (shared_objects)
        {
            clearShaderObjects();
        }
    }

    LLPipeline::sRenderGlow = gSavedSettings.getBOOL("RenderGlow");

    if (gViewerWindow)
    {
        gViewerWindow->setCursor(UI_CURSOR_WAIT);
    }

    // Shaders
    LL_INFOS("ShaderLoading") << "\n~~~~~~~~~~~~~~~~~~\n Loading Shaders:\n~~~~~~~~~~~~~~~~~~" << LL_ENDL;
    LL_INFOS("ShaderLoading") << llformat("Using GLSL %d.%d", gGLManager.mGLSLVersionMajor, gGLManager.mGLSLVersionMinor) << LL_ENDL;

    for (S32 i = 0; i < SHADER_COUNT; i++)
    {
        mShaderLevel[i] = 0;
    }
    mMaxAvatarShaderLevel = 0;

    LLVertexBuffer::unbind();


    S32 light_class = 3;
    S32 interface_class = 2;
    S32 env_class = 2;
    S32 obj_class = 2;
    S32 effect_class = 2;
    S32 wl_class = 2;
    S32 water_class = 3;
    S32 deferred_class = 3;

    // Load lighting shaders
    mShaderLevel[SHADER_LIGHTING] = light_class;
    mShaderLevel[SHADER_INTERFACE] = interface_class;
    mShaderLevel[SHADER_ENVIRONMENT] = env_class;
    mShaderLevel[SHADER_WATER] = water_class;
    mShaderLevel[SHADER_OBJECT] = obj_class;
    mShaderLevel[SHADER_EFFECT] = effect_class;
    mShaderLevel[SHADER_WINDLIGHT] = wl_class;
    mShaderLevel[SHADER_DEFERRED] = deferred_class;

    std::string shader_name = shared_objects ? loadBasicShaders() : std::string();
    if (!shared_objects)
    {
        LL_INFOS("Shader") << "Kept the basic shaders." << LL_ENDL;
    }
    else if (shader_name.empty())
    {
        LL_INFOS("Shader") << "Loaded basic shaders." << LL_ENDL;
    }
    else
    {
        // "ShaderLoading" and "Shader" need to be logged
        LL_WARNS("Shader") << "Failed loading basic shaders.  Retrying with increased log level..." << LL_ENDL;

        LLError::ELevel lvl = LLError::getDefaultLevel();
        LLError::setDefaultLevel(LLError::LEVEL_DEBUG);
        loadBasicShaders();
        LLError::setDefaultLevel(lvl);
        gGLManager.printGLInfoString();
        // The programs attaching it fail to build and draw nothing; the rest draw.
        LL_WARNS("Shader") << "Unable to load basic shader " << shader_name
                           << ", verify graphics driver installed and current." << LL_ENDL;
    }

    gPipeline.mShadersLoaded = true;

    // Every loader runs, whatever the others did: a program that does not build is recorded (built) and draws nothing
    // (LLGLSLShader::bind), and the rest of the viewer draws on.
    loadShadersEffects();
    loadShadersInterface();

    // The avatar programs are built at class 1; mMaxAvatarShaderLevel is the highest they may take.
    mMaxAvatarShaderLevel = 3;
    mShaderLevel[SHADER_AVATAR] = 1;
    loadShadersObject();
    loadShadersAvatar();
    loadShadersDeferred();

    // A program this load's loaders configured nothing for was not asked for: a feature switched off, or a loader
    // stopped short at a failure. It goes, as a load from nothing would leave it.
    std::vector<LLGLSLShader*> unasked;
    for (LLGLSLShader* shader : LLGLSLShader::sInstances)
    {
        if (!shader->mOwnedVariant && shader->mLoadGeneration != LLGLSLShader::sLoadGeneration)
        {
            unasked.push_back(shader);
        }
    }
    for (LLGLSLShader* shader : unasked)
    {
        shader->unload();
    }

    // The fallback sky cube map is drawn with these programs, and again by the sky once it is told to.
    if (LLGLSLShader::sLinked > 0 && gSky.mVOSkyp.notNull())
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
        LL_WARNS("ShaderLoading") << mFailedPrograms.size() << " programs did not build and draw nothing: " << names
                                  << LL_ENDL;
    }

    // A program that did not link saved no binary, so the metadata names only programs that did.
    if (LLGLSLShader::sLinked > 0 && !LLAppViewer::instance()->isSecondInstance())
    {
        persistShaderCacheMetadata();
    }

    if (gViewerWindow)
    {
        gViewerWindow->setCursor(UI_CURSOR_ARROW);
    }

    finalizeShaderList();

    LL_INFOS("ShaderLoading") << "Programs " << (rebuild ? "built from their sources" : "reloaded") << ": "
                              << LLGLSLShader::sLinked << " linked, " << LLGLSLShader::sKept << " kept, "
                              << unasked.size() << " unloaded; shared objects "
                              << (shared_objects ? "compiled" : "kept") << ", in "
                              << fmt::format("{:.1f}", timer.getElapsedTimeF64().value() * 1000.0) << " ms" << LL_ENDL;
    LL_PROFILE_PLOT("programs linked", (int64_t)LLGLSLShader::sLinked);
    LL_PROFILE_PLOT("programs kept", (int64_t)LLGLSLShader::sKept);

    reentrance = false;
    return true;
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
        if (application.mSharedObjects)
        {
            // A context released during migration owns any old GL names;
            // clearShaderObjects now only retires the CPU-side registry.
            clearShaderObjects();
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
    // The shared objects are the manager's, so they go here rather than with the programs that attached them.
    clearShaderObjects();
    LLGLSLShader::releaseStandIn();

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

std::string LLViewerShaderMgr::loadBasicShaders()
{
    // Load basic dependency shaders first

    // A shared object that does not compile is reported, and the rest compiled: only the programs attaching it fail.
    std::string first_failure;
    const auto failed = [&first_failure](const std::string& file)
    {
        if (first_failure.empty())
        {
            first_failure = file;
        }
    };

    // The feature table masks out the light level to use.
    const S32 sum_lights_class = mInputs.mGlobal.mLightingClass;

    // Load the Basic Vertex Shaders at the appropriate level.
    // (in order of shader function call depth for reference purposes, deepest level first)

    vector< pair<string, S32> > shaders;
    shaders.push_back( make_pair( "windlight/atmosphericsVarsV.glsl",       mShaderLevel[SHADER_WINDLIGHT] ) );
    shaders.push_back( make_pair( "windlight/atmosphericsHelpersV.glsl",    mShaderLevel[SHADER_WINDLIGHT] ) );
    shaders.push_back( make_pair( "lighting/lightFuncV.glsl",               mShaderLevel[SHADER_LIGHTING] ) );
    shaders.push_back( make_pair( "lighting/sumLightsV.glsl",               sum_lights_class ) );
    shaders.push_back( make_pair( "lighting/lightV.glsl",                   mShaderLevel[SHADER_LIGHTING] ) );
    shaders.push_back( make_pair( "lighting/lightFuncSpecularV.glsl",       mShaderLevel[SHADER_LIGHTING] ) );
    shaders.push_back( make_pair( "lighting/sumLightsSpecularV.glsl",       sum_lights_class ) );
    shaders.push_back( make_pair( "lighting/lightSpecularV.glsl",           mShaderLevel[SHADER_LIGHTING] ) );
    shaders.push_back( make_pair( "windlight/atmosphericsFuncs.glsl",       mShaderLevel[SHADER_WINDLIGHT] ) );
    shaders.push_back( make_pair( "windlight/atmosphericsV.glsl",           mShaderLevel[SHADER_WINDLIGHT] ) );
    shaders.push_back( make_pair( "environment/srgbF.glsl",                 1 ) );
    shaders.push_back( make_pair( "avatar/avatarSkinV.glsl",                1 ) );
    shaders.push_back( make_pair( "avatar/objectSkinV.glsl",                1 ) );
    shaders.push_back( make_pair( "deferred/textureUtilV.glsl",             1 ) );
    shaders.push_back( make_pair( "environment/waterSurface.glsl",          1 ) );
    shaders.push_back( make_pair( "objects/indexedTextureV.glsl",           1 ) );
    shaders.push_back( make_pair( "objects/nonindexedTextureV.glsl",        1 ) );

    std::map<std::string, std::string> attribs;
    attribs["MAX_JOINTS_PER_MESH_OBJECT"] =
        std::to_string(LLSkinningUtil::getMaxJointCount());

    const ALShaderInputs::Global& in = mInputs.mGlobal;

    if (in.mEmissive)
    {
        attribs["HAS_EMISSIVE"] = "1";
    }

    const bool ssr = in.mSSR;

    const bool mirrors = in.mMirrors;

    const bool has_reflection_probes = in.mProbes;

    const S32 probe_level = in.mProbeLevel;

    if (ssr)
    {
        attribs["SSR"] = "1";
    }

    if (has_reflection_probes)
    {
        attribs["REFMAP_LEVEL"] = std::to_string(probe_level);
        attribs["REF_SAMPLE_COUNT"] = "32";
    }

    if (mirrors)
    {
        attribs["HERO_PROBES"] = "1";
    }

    // NOTE: REVERSE_Z is deliberately NOT here. This map reaches only the shared objects
    // compiled below -- a program's own mShaderFiles carry their permutations and nothing else
    // -- and reverse-Z is read by program-owned sources too. LLShaderMgr::loadShaderFile()
    // injects it for every compile instead, and LLGLSLShader::hash() folds LLRender::sReverseZ
    // in directly so a toggle still invalidates cached binaries.

    { // PBR terrain
        attribs["TERRAIN_PLANAR_TEXTURE_SAMPLE_COUNT"] = llformat("%d", in.mTerrainPlanarSampleCount);
        attribs["TERRAIN_PBR_DETAIL"] = llformat("%d", in.mTerrainPBRDetail);
        if (in.mTerrainHexTiling)
        {
            attribs["TERRAIN_HEX_TILING"] = "1";
        }
    }

    // The water's wave layers that hex-tile, read in waterSurface.glsl, a shared object.
    attribs["WATER_HEXTILE"] = std::to_string(in.mWaterHextile);

    LLGLSLShader::sGlobalDefines = attribs;
    // What the objects below compile from, which every program's hash follows.
    LLGLSLShader::sSharedObjectsKey = in.digest();

    // Shared objects are compiled once and attached by name, so a per-program permutation
    // cannot reach them. Sources that read classic_mode therefore get a second copy compiled
    // with CLASSIC_MODE=1, stored under a suffixed key; attachShaderFeatures() picks the copy
    // matching each program's own defines (see LLShaderMgr::variantObjectKey).
    std::map<std::string, std::string> classic_attribs = attribs;
    classic_attribs["CLASSIC_MODE"] = "1";

    // We no longer have to bind the shaders to global glhandles, they are automatically added to a map now.
    for (U32 i = 0; i < shaders.size(); i++)
    {
        // Note usage of GL_VERTEX_SHADER
        if (loadShaderFile(shaders[i].first, shaders[i].second, GL_VERTEX_SHADER, &attribs) == 0)
        {
            LL_WARNS("Shader") << "Failed to load basic vertex shader " << i << ": " << shaders[i].first << LL_ENDL;
            failed(shaders[i].first);
        }
    }

    if (!load_axis_copy(*this, shaders, "windlight/atmosphericsFuncs.glsl", GL_VERTEX_SHADER,
                        classic_attribs, "CLASSIC_MODE", LLShaderMgr::CLASSIC_OBJECT_SUFFIX))
    {
        failed("windlight/atmosphericsFuncs.glsl");
    }

    // The terrain's evaluation-stage objects. GLSL links per stage, so the objects the terrain
    // evaluation calls are compiled here for that stage, under the same keys in the stage's
    // own map.
    shaders.clear();
    shaders.push_back( make_pair( "deferred/terrainSurface.glsl",           1 ) );
    shaders.push_back( make_pair( "windlight/atmosphericsVarsV.glsl",       mShaderLevel[SHADER_WINDLIGHT] ) );
    for (U32 i = 0; i < shaders.size(); i++)
    {
        if (loadShaderFile(shaders[i].first, shaders[i].second, GL_TESS_EVALUATION_SHADER, &attribs) == 0)
        {
            LL_WARNS("Shader") << "Failed to load basic tessellation evaluation shader " << i << ": " << shaders[i].first << LL_ENDL;
            failed(shaders[i].first);
        }
    }

    // Load the Basic Fragment Shaders at the appropriate level.
    // (in order of shader function call depth for reference purposes, deepest level first)

    shaders.clear();
    S32 ch = llmax(LLGLSLShader::sIndexedTextureChannels, 1);


    std::vector<S32> index_channels;
    index_channels.push_back(-1);    shaders.push_back( make_pair( "windlight/atmosphericsVarsF.glsl",      mShaderLevel[SHADER_WINDLIGHT] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "windlight/atmosphericsHelpersF.glsl",       mShaderLevel[SHADER_WINDLIGHT] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "windlight/atmosphericsFuncs.glsl",       mShaderLevel[SHADER_WINDLIGHT] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "windlight/atmosphericsF.glsl",          mShaderLevel[SHADER_WINDLIGHT] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "environment/waterFogF.glsl",                mShaderLevel[SHADER_WATER] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "environment/srgbF.glsl",                    mShaderLevel[SHADER_ENVIRONMENT] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/deferredUtil.glsl",                    1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/gbufferUtil.glsl",                    1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/globalF.glsl",                          1));
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/aoUtil.glsl",                          1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/terrainSurface.glsl",                  1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "environment/waterSurface.glsl",                 1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/pbrterrainUtilF.glsl",                 1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/hexLatticeUtil.glsl",                  1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/tonemapUtilF.glsl",                    1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "alchemy/colorGradeUtilF.glsl",                 1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "alchemy/postEffectUtilsF.glsl",                 1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/reflectionProbeF.glsl",                has_reflection_probes ? 3 : 2) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/screenSpaceReflUtil.glsl",             ssr ? 3 : 1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "lighting/lightNonIndexedF.glsl",                    mShaderLevel[SHADER_LIGHTING] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "lighting/lightAlphaMaskNonIndexedF.glsl",                   mShaderLevel[SHADER_LIGHTING] ) );
    index_channels.push_back(ch);    shaders.push_back( make_pair( "lighting/lightF.glsl",                  mShaderLevel[SHADER_LIGHTING] ) );
    index_channels.push_back(ch);    shaders.push_back( make_pair( "lighting/lightAlphaMaskF.glsl",                 mShaderLevel[SHADER_LIGHTING] ) );

    for (U32 i = 0; i < shaders.size(); i++)
    {
        // Note usage of GL_FRAGMENT_SHADER
        if (loadShaderFile(shaders[i].first, shaders[i].second, GL_FRAGMENT_SHADER, &attribs, index_channels[i]) == 0)
        {
            LL_WARNS("Shader") << "Failed to load fragment shader " << shaders[i].first << LL_ENDL;
            failed(shaders[i].first);
        }
    }

    if (mirrors)
    {
        // globalF's mirrorClip() is the only shared source that varies on MIRROR_CLIP. Only
        // compiled when RenderMirrors is on, matching the gate on the variants themselves --
        // nothing can attach this copy otherwise.
        std::map<std::string, std::string> mirror_attribs = attribs;
        mirror_attribs["MIRROR_CLIP"] = "1";

        if (!load_axis_copy(*this, shaders, "deferred/globalF.glsl", GL_FRAGMENT_SHADER,
                            mirror_attribs, "MIRROR_CLIP", LLShaderMgr::MIRROR_OBJECT_SUFFIX))
        {
            failed("deferred/globalF.glsl");
        }
    }

    for (const char* path : { "windlight/atmosphericsFuncs.glsl",
                              "deferred/deferredUtil.glsl",
                              "deferred/reflectionProbeF.glsl" })
    {
        if (!load_axis_copy(*this, shaders, path, GL_FRAGMENT_SHADER,
                            classic_attribs, "CLASSIC_MODE", LLShaderMgr::CLASSIC_OBJECT_SUFFIX))
        {
            failed(path);
        }
    }

    return first_failure;
}

bool LLViewerShaderMgr::built(bool linked, const LLGLSLShader& shader)
{
    if (!linked)
    {
        LL_WARNS("ShaderLoading") << shader.mName << " did not build; it draws nothing, and the load goes on" << LL_ENDL;
        mFailedPrograms.push_back(shader.mName);
    }
    return linked;
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
        gLensDirtGenProgram.unload();
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
        gLensDirtGenProgram.mName = "Lens Dirt Generator";
        gLensDirtGenProgram.mShaderFiles.clear();
        gLensDirtGenProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gLensDirtGenProgram.mShaderFiles.push_back(make_pair("effects/lensDirtGenF.glsl", GL_FRAGMENT_SHADER));
        gLensDirtGenProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        gLensDirtGenProgram.clearPermutations();
        gLensDirtGenProgram.addPermutation("DIRT_MAX_LINES", std::to_string(LENS_DIRT_MAX_LINES));
        built(gLensDirtGenProgram.createShader(), gLensDirtGenProgram);
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
        gDeferredBufferVisualProgram.unload();

        LLGLSLShader::sIndexedLegacyMaterials = false;

        gHUDPBROpaqueProgram.unload();
        for (U32 paint_type = 0; paint_type < TERRAIN_PAINT_TYPE_COUNT; ++paint_type)
        {
            gDeferredPBRTerrainProgram[paint_type].unload();
            gDeferredPBRTerrainAlphaMaskProgram[paint_type].unload();
        }

// [RLVa:KB] - @setsphere
        gRlvSphereProgram.unload();
// [/RLVa:KB]

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
        gDeferredBufferVisualProgram.mName = "Deferred Buffer Visualization Shader";
        gDeferredBufferVisualProgram.mShaderFiles.clear();
        gDeferredBufferVisualProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredBufferVisualProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredVisualizeBuffers.glsl", GL_FRAGMENT_SHADER));
        gDeferredBufferVisualProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        add_common_permutations(&gDeferredBufferVisualProgram);

        built(gDeferredBufferVisualProgram.createShader(), gDeferredBufferVisualProgram);
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

    // [RLVa:KB] - @setsphere
    {
        gRlvSphereProgram.mName = "RLVa Sphere Post Processing Shader";
        gRlvSphereProgram.mFeatures.isDeferred = true;
        gRlvSphereProgram.mShaderFiles.clear();
        gRlvSphereProgram.mShaderFiles.push_back(make_pair("deferred/rlvV.glsl", GL_VERTEX_SHADER));
        gRlvSphereProgram.mShaderFiles.push_back(make_pair("deferred/rlvF.glsl", GL_FRAGMENT_SHADER));
        gRlvSphereProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        built(gRlvSphereProgram.createShader(), gRlvSphereProgram);
    }
    // [/RLV:KB]
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
        // Initialize the shadow-path uniform to passthrough so non-text UI
        // and NO_SHADOW text take the early-return branch in uiF.glsl. GLSL
        // already zero-initializes uniforms, but pushing an explicit default
        // documents the contract and protects against driver quirks.
        // textShadowMode is the shader's only shadow uniform — atlas texel size
        // and channel layout derive from the bound texture in uiF.glsl.
        gUIProgram.mOnLink = [](LLGLSLShader& s)
        {
            s.bind();
            s.uniform1i(LLShaderMgr::TEXT_SHADOW_MODE, 0);
            s.unbind();
        };
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
        gTwoTextureCompareProgram.mOnLink = [](LLGLSLShader& s)
        {
            s.bind();
            s.uniform1i(sTex0, 0);
            s.uniform1i(sTex1, 1);
            s.uniform1i(sDitherTex, 2);
            s.unbind();
        };
        built(gTwoTextureCompareProgram.createShader(), gTwoTextureCompareProgram);
    }

    {
        gOneTextureFilterProgram.mName = "One Texture Filter Shader";
        gOneTextureFilterProgram.mShaderFiles.clear();
        gOneTextureFilterProgram.mShaderFiles.push_back(make_pair("interface/onetexturefilterV.glsl", GL_VERTEX_SHADER));
        gOneTextureFilterProgram.mShaderFiles.push_back(make_pair("interface/onetexturefilterF.glsl", GL_FRAGMENT_SHADER));
        gOneTextureFilterProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        gOneTextureFilterProgram.mOnLink = [](LLGLSLShader& s)
        {
            s.bind();
            s.uniform1i(sTex0, 0);
            s.unbind();
        };
        built(gOneTextureFilterProgram.createShader(), gOneTextureFilterProgram);
    }
#endif

    {
        gSolidColorProgram.mName = "Solid Color Shader";
        gSolidColorProgram.mShaderFiles.clear();
        gSolidColorProgram.mShaderFiles.push_back(make_pair("interface/solidcolorV.glsl", GL_VERTEX_SHADER));
        gSolidColorProgram.mShaderFiles.push_back(make_pair("interface/solidcolorF.glsl", GL_FRAGMENT_SHADER));
        gSolidColorProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        gSolidColorProgram.mOnLink = [](LLGLSLShader& s)
        {
            s.bind();
            s.uniform1i(sTex0, 0);
            s.unbind();
        };
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
        built(gAlphaMaskProgram.createShader(), gAlphaMaskProgram);
    }

}


std::string LLViewerShaderMgr::getShaderDirPrefix(void)
{
    return gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "shaders", "class");
}

void LLViewerShaderMgr::updateShaderUniforms(LLGLSLShader * shader)
{
    if (LLEnvironment::instanceExists())
    {
        LLEnvironment::instance().updateShaderUniforms(shader);
    }
}

