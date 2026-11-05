/**
 * @file alrendercapabilities.h
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

#pragma once

#include "stdtypes.h"

#include <string>

class LLGLManager;
#if LL_DARWIN
struct ALMetalDeviceInfo;
#endif

enum class ALRendererBackend
{
    none,
    opengl,
    metal,
};

// This is intentionally separate from LLGLManager. LLGLManager::mInited means
// that GL entry points and a live context exist; it must never be kept true to
// carry product-renderer identity or settings after the Darwin context is cut.
class ALRenderCapabilities final
{
public:
    void reset();
    void initializeOpenGL(const LLGLManager& gl);
#if LL_DARWIN
    void initializeMetal(const ALMetalDeviceInfo& metal);
#endif

    bool isInitialized() const noexcept { return mBackend != ALRendererBackend::none; }
    bool isMetal() const noexcept { return mBackend == ALRendererBackend::metal; }
    ALRendererBackend backend() const noexcept { return mBackend; }

    const std::string& vendor() const noexcept { return mVendor; }
    const std::string& renderer() const noexcept { return mRenderer; }
    const std::string& version() const noexcept { return mVersion; }
    const std::string& identity() const noexcept { return mIdentity; }

    bool isApple() const noexcept { return mIsApple; }
    bool isAMD() const noexcept { return mIsAMD; }
    bool isNVIDIA() const noexcept { return mIsNVIDIA; }
    bool isIntel() const noexcept { return mIsIntel; }
    bool isSoftware() const noexcept { return mIsSoftware; }

    U32 memoryBudgetMB() const noexcept { return mMemoryBudgetMB; }
    U32 maxTextureDimension() const noexcept { return mMaxTextureDimension; }
    bool hasAnisotropicFiltering() const noexcept { return mHasAnisotropicFiltering; }
    F32 maxAnisotropy() const noexcept { return mMaxAnisotropy; }

    // These two masks describe limits of the retained GL shader model. They
    // are not meaningful for native Metal pipeline layouts and are therefore
    // applied only when this returns true.
    bool hasLegacyShaderLimits() const noexcept { return mHasLegacyShaderLimits; }
    S32 textureImageUnits() const noexcept { return mTextureImageUnits; }
    S32 maxVaryingVectors() const noexcept { return mMaxVaryingVectors; }

private:
    ALRendererBackend mBackend = ALRendererBackend::none;
    std::string mVendor;
    std::string mRenderer;
    std::string mVersion;
    std::string mIdentity;
    bool mIsApple = false;
    bool mIsAMD = false;
    bool mIsNVIDIA = false;
    bool mIsIntel = false;
    bool mIsSoftware = false;
    U32 mMemoryBudgetMB = 0;
    U32 mMaxTextureDimension = 0;
    bool mHasAnisotropicFiltering = false;
    F32 mMaxAnisotropy = 1.f;
    bool mHasLegacyShaderLimits = false;
    S32 mTextureImageUnits = 0;
    S32 mMaxVaryingVectors = 0;
};

extern ALRenderCapabilities gRenderCapabilities;
