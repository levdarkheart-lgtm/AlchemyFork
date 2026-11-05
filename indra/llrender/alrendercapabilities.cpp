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
    mMaxTextureDimension = static_cast<U32>(llmax(gl.mGLMaxTextureSize, 0));
    mHasAnisotropicFiltering = gl.mHasAnisotropic;
    mMaxAnisotropy = llmax(gl.mMaxAnisotropy, 1.f);
    mHasLegacyShaderLimits = true;
    mTextureImageUnits = gl.mNumTextureImageUnits;
    mMaxVaryingVectors = gl.mMaxVaryingVectors;
}

#if LL_DARWIN
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
