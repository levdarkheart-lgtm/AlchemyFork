/**
 * @file alrendercapabilities.cpp
 * @brief Backend-neutral renderer identity and feature-selection capabilities.
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

#include "linden_common.h"

#include "alrendercapabilities.h"

#include "llgl.h"

#if LL_DARWIN
#include "almetaldevice.h"

#include <algorithm>
#include <sstream>
#endif

ALRenderCapabilities gRenderCapabilities;

void ALRenderCapabilities::reset()
{
    *this = ALRenderCapabilities();
}

void ALRenderCapabilities::initializeOpenGL(const LLGLManager& gl)
{
    reset();
    mBackend = ALRendererBackend::opengl;
    mVendor = gl.mGLVendorString;
    mRenderer = gl.mGLRendererString;
    mVersion = gl.mGLVersionString;
    mIdentity = gl.mGLIdentity;
    mIsApple = gl.mIsApple;
    mIsAMD = gl.mIsAMD;
    mIsNVIDIA = gl.mIsNVIDIA;
    mIsIntel = gl.mIsIntel;
    mIsSoftware = gl.mIsSoftwareRenderer;
    mMemoryBudgetMB = gl.mVRAM;
    // Preserve the original viewer's semantics exactly: gGLManager.mVRAM is
    // detected dedicated/video memory, which the texture manager subsequently
    // applies RenderTextureVRAMDivisor to.
    mTextureMemoryBudgetMB = gl.mVRAM;
    mMaxTextureDimension = static_cast<U32>(llmax(gl.mGLMaxTextureSize, 0));
    mHasAnisotropicFiltering = gl.mHasAnisotropic;
    mMaxAnisotropy = llmax(gl.mMaxAnisotropy, 1.f);
    mHasLegacyShaderLimits = true;
    mTextureImageUnits = gl.mNumTextureImageUnits;
    mMaxVaryingVectors = gl.mMaxVaryingVectors;
}

#if LL_DARWIN
namespace
{
// MTLDevice.recommendedMaxWorkingSetSize describes all GPU allocations on a
// unified-memory device. It is not the dedicated texture VRAM value consumed
// by the original OpenGL heuristic. Keep streamed images to a bounded share of
// that ceiling; manual RenderMaxVRAMBudget and RenderTextureVRAMDivisor remain
// available on top of this automatically detected capacity. This is a
// containment default, not a scene-derived performance optimum; paired
// scenario evidence is still required before changing the original streamer's
// control policy.
U32 metal_texture_memory_budget_mb(U64 recommended_working_set_size)
{
    constexpr U64 MIN_TEXTURE_CAPACITY_MB = 2ull * 1024ull;
    constexpr U64 MAX_TEXTURE_CAPACITY_MB = 8ull * 1024ull;
    constexpr U64 WORKING_SET_DIVISOR = 16ull;

    const U64 working_set_mb = recommended_working_set_size >> 20;
    if (working_set_mb == 0)
    {
        return 0;
    }

    const U64 scaled_mb = std::clamp(working_set_mb / WORKING_SET_DIVISOR,
                                     MIN_TEXTURE_CAPACITY_MB,
                                     MAX_TEXTURE_CAPACITY_MB);
    return static_cast<U32>(std::min(working_set_mb, scaled_mb));
}
}

void ALRenderCapabilities::initializeMetal(const ALMetalDeviceInfo& metal)
{
    reset();
    mBackend = ALRendererBackend::metal;
    mVendor = "Apple";
    mRenderer = metal.name;

    std::ostringstream version;
    version << "Metal Apple GPU family " << metal.highest_apple_family
            << ", Mac GPU family " << metal.highest_mac_family;
    mVersion = version.str();

    std::ostringstream identity;
    identity << mVendor << ' ' << mRenderer << " registry " << metal.registry_id
             << ' ' << mVersion;
    mIdentity = identity.str();

    mIsApple = true;
    mMemoryBudgetMB = static_cast<U32>(metal.recommended_working_set_size >> 20);
    mTextureMemoryBudgetMB =
        metal_texture_memory_budget_mb(metal.recommended_working_set_size);
    mMaxTextureDimension = metal.max_texture_2d_dimension;
    // Every supported Apple Silicon Metal family supports sampler anisotropy
    // through 16. The native sampler validator enforces the same 1...16 range.
    mHasAnisotropicFiltering = true;
    mMaxAnisotropy = 16.f;
    // Metal pipeline layouts are explicit and do not inherit GL's fragment
    // texture-unit or varying-vector limits.
    mHasLegacyShaderLimits = false;
}
#endif
