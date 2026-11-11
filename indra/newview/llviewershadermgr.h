/**
 * @file llviewershadermgr.h
 * @brief Viewer Shader Manager
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_VIEWER_SHADER_MGR_H
#define LL_VIEWER_SHADER_MGR_H

#include "alrenderinputrequests.h"
#include "alshaderinputs.h"
#include "llshadermgr.h"
#include "llmaterial.h"

class ALMetalDevice;

class LLViewerShaderMgr: public LLShaderMgr
{
public:
    static bool sInitialized;

    LLViewerShaderMgr();
    /* virtual */ ~LLViewerShaderMgr();

    // Post-load sanity pass; asserts on duplicate program names in debug builds
    // (checked over LLGLSLShader::sInstances -- no registration needed).
    void finalizeShaderList();

    // singleton pattern implementation
    static LLViewerShaderMgr * instance();
    static void releaseInstance();

    // Apply the app-owned native cache policy before initial PSOs or reload.
    // Cache failures remain optional and never suppress native rendering.
    static void configureNativePipelineCache(ALMetalDevice& device);

    void initAttribsAndUniforms(void) override;
    /// Configures native pass identities and variants from the pipeline's latched inputs.
    /// Unchanged configurations keep their metadata; a forced rebuild replaces it, and
    /// a pass no loader requests is retired. The pipeline reloads actual Metal libraries
    /// and pipelines separately at the same frame boundary. Requires initialized Metal.
    bool setShaders(const ALShaderInputs& inputs, const ALRenderApplication& application);
    /// The inputs the programs were last loaded for, which every loader reads in place of the settings.
    const ALShaderInputs& inputs() const { return mInputs; }
    void unloadShaders();
    S32  getShaderLevel(S32 type);

    void loadShadersEffects();
    void loadShadersDeferred();
    void loadShadersObject();
    void loadShadersAvatar();
    void loadShadersInterface();

    std::vector<S32> mShaderLevel;
    S32 mMaxAvatarShaderLevel;

    enum EShaderClass
    {
        SHADER_LIGHTING,
        SHADER_OBJECT,
        SHADER_AVATAR,
        SHADER_ENVIRONMENT,
        SHADER_INTERFACE,
        SHADER_EFFECT,
        SHADER_WINDLIGHT,
        SHADER_WATER,
        SHADER_DEFERRED,
        SHADER_COUNT
    };

    void updateShaderUniforms(LLGLSLShader * shader) override;

private:
    // Configure the retained traversal's semantic pass/variant graph for the
    // native Metal pipeline without compiling or caching GLSL.
    bool setMetalShaders(const ALRenderApplication& application);

    // A program a loader asked for, recorded where it did not build; whether it built. The loader goes on to the next
    // either way, and a program that did not build draws nothing (LLGLSLShader::bind).
    bool built(bool linked, const LLGLSLShader& shader);

    ALShaderInputs mInputs;
    // The programs the load running could not build, by name.
    std::vector<std::string> mFailedPrograms;
}; //LLViewerShaderMgr

extern LLVector4            gShinyOrigin;

//utility shaders
extern LLGLSLShader         gOcclusionProgram;
extern LLGLSLShader         gOcclusionCubeProgram;
extern LLGLSLShader         gDebugProgram;
extern LLGLSLShader         gCopyProgram;
extern LLGLSLShader         gCopyDepthProgram;


//object shaders
extern LLGLSLShader     gObjectAlphaMaskNoColorProgram;

//environment shaders
extern LLGLSLShader         gGlowProgram;
extern LLGLSLShader         gGlowExtractProgram;
extern LLGLSLShader         gBloomExtractProgram;
extern LLGLSLShader         gBloomDownsampleProgram;
extern LLGLSLShader         gBloomDownsampleFirstProgram;
extern LLGLSLShader         gBloomUpsampleProgram;
// Taps per cross-filter pass. Injected into crossFilterF.glsl as CROSS_TAPS
// and used by pipeline.cpp for the pass strides and the falloff remap, so the
// chain's exact base-N tiling has one source of truth instead of three sites
// that each hard-coded 4 or 63.
constexpr S32               CROSS_FILTER_TAPS = 4;
// Shared by the generation gate and the composite, which read the same setting
// in two different files and must agree, or the effect changes brightness
// between "is it on" and "how bright".
constexpr F32               CROSS_FILTER_MAX_STRENGTH = 32.f;
extern LLGLSLShader         gCrossFilterProgram;
// Upper bound on the native generator's segment loop and the CPU control.
constexpr S32               LENS_DIRT_MAX_LINES = 32;

// avatar shader handles
extern LLGLSLShader         gAvatarProgram;
extern LLGLSLShader         gImpostorProgram;

// Deferred rendering shaders
extern LLGLSLShader         gDeferredTerrainProgram;
#if !LL_DARWIN
// Retained non-Darwin DoF gather blur, four variants over two orthogonal
// compile-time axes: FRONT_BLUR (RenderDepthOfFieldNearBlur) and DOF_SHAPED
// (any shaped aperture, cat's-eye or defocus fringe being active). The shaped
// code sits in the innermost sample loop, so it is compiled out rather than
// branched over.
extern LLGLSLShader         gDeferredPostProgram;
extern LLGLSLShader         gDeferredPostProgramNoNear;
extern LLGLSLShader         gDeferredPostProgramShaped;
extern LLGLSLShader         gDeferredPostProgramNoNearShaped;
extern LLGLSLShader         gDeferredDoFCombineProgram;
extern LLGLSLShader         gDeferredDoFCombineProgramNoNear;
extern LLGLSLShader         gFXAAProgram[4];
extern LLGLSLShader         gSMAAEdgeDetectProgram[4];
#endif
extern LLGLSLShader         gExposureProgram;
extern LLGLSLShader         gExposureProgramNoFade;
extern LLGLSLShader         gExposureHistogramProgram;
extern LLGLSLShader         gLuminanceProgram;
extern LLGLSLShader         gLensFlareStateProgram;
extern LLGLSLShader         gBlitWithEffectsProgram;
extern LLGLSLShader         gCGGammaProgram;
extern LLGLSLShader         gCGLegacyGammaProgram;
extern LLGLSLShader         gCGTonemapProgram;
extern LLGLSLShader         gCGTonemapLegacyGammaProgram;
extern LLGLSLShader         gCGColorgradeGammaProgram;
extern LLGLSLShader         gCGColorgradeLegacyGammaProgram;
extern LLGLSLShader         gCGTonemapColorgradeProgram;
extern LLGLSLShader         gCGTonemapColorgradeLegacyGammaProgram;

enum TerrainPaintType : U32
{
    // Use LLVLComposition::mDatap (heightmap) generated by generateHeights, plus noise from TERRAIN_ALPHARAMP
    TERRAIN_PAINT_TYPE_HEIGHTMAP_WITH_NOISE = 0,
    // Use paint map if PBR terrain, otherwise fall back to TERRAIN_PAINT_TYPE_HEIGHTMAP_WITH_NOISE
    TERRAIN_PAINT_TYPE_PBR_PAINTMAP         = 1,
    TERRAIN_PAINT_TYPE_COUNT                = 2,
};
extern LLGLSLShader         gDeferredPBRTerrainProgram[TERRAIN_PAINT_TYPE_COUNT];
extern LLGLSLShader         gDeferredPBRTerrainAlphaMaskProgram[TERRAIN_PAINT_TYPE_COUNT]; // a region with a masked material
#endif
