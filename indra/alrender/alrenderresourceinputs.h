/**
 * @file alrenderresourceinputs.h
 * @brief Everything that decides what the pipeline's render resources are, latched in one place.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
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
 * $/LicenseInfo$
 */

#pragma once

#include "stdtypes.h"

#include <algorithm>
#include <bit>

/// Everything that decides what the pipeline's render resources are (ALRenderResourcePlan::make), as values. The
/// viewer builds one in one place from its settings and the latches set from them, so every resource is planned from
/// the same reading of the settings; a plan's test builds one by hand.
struct ALRenderResourceInputs
{
    /// The render size before RenderResolutionDivisor and RenderResolutionMultiplier: the world view's, or a
    /// snapshot's while an ALRenderSizeOverride holds one.
    U32 mWidth = 0;
    U32 mHeight = 0;
    U32 mResolutionDivisor = 1;
    F32 mResolutionMultiplier = 1.f;

    bool mHDR = true;            ///< RenderHDREnabled
    bool mEmissive = true;       ///< RenderEnableEmissiveBuffer
    bool mGBufferNormHDR = true; ///< the G-buffer normal layout the programs compile for (LLRender::sGBufferNormHDR)
    bool mReverseZ = true;       ///< the depth convention the programs compile for (LLRender::sReverseZ)
    bool mCompute = true;        ///< whether the session computes, as the programs are built for it (LLRender::sCompute)

    S32  mShadowDetail = 2;       ///< RenderShadowDetail: 0 none, 1 the sun's, 2 the sun's and two spot lights'
    F32  mShadowScale = 1.f;      ///< RenderShadowResolutionScale, of the main view's sun and spot shadows
    U32  mSpotShadowMaxResolution = 2048; ///< AlchemyRenderSpotShadowMaxResolution: the spot shadows' largest edge, 0 none
    bool mShadowDepth32F = false; ///< AlchemyRenderShadowDepth32F
    bool mSSAO = true;            ///< RenderDeferredSSAO
    bool mSSR = false;            ///< RenderScreenSpaceReflections

    bool mProbes = true;          ///< RenderReflectionsEnabled, where the feature table allows it
    U32  mProbeResolution = 128;  ///< a probe's face edge, as probeResolution() takes RenderReflectionProbeResolution
    U32  mProbeSuperSample = 2;   ///< how much larger a probe face renders each way, as probeSuperSample() takes it
    bool mProbeAllCascades = false; ///< AlchemyRenderProbeAllShadowCascades: a probe's face renders four sun cascades
    bool mMirrors = false;        ///< RenderMirrors
    U32  mHeroResolution = 1024;  ///< RenderHeroProbeResolution

    U32  mFSAAType = 0;          ///< RenderFSAAType: 0 none, 1 FXAA, 2 SMAA
    bool mSMAAStencil = true;    ///< RenderSMAAUseStencil
    bool mSMAAUseSample = false; ///< RenderSMAAUseSample: alternate edge-detection image
    S32  mBloomMips = 5;       ///< RenderBloomMipCount
    S32  mBloomScale = 0;      ///< RenderBloomResolutionScale: full, 3/4, 1/2, 1/4 or 1/8 of the render size
    bool mBloomHalation = false;
    S32  mGlowResolutionPow = 9; ///< RenderGlowResolutionPow: the legacy glow's height, as a power of two
    bool mGlowHDR = false;       ///< RenderGlowHDR
    F32  mDeferredNoise = 4.f;   ///< RenderDeferredNoise, which the deferred noise map's contents are made from

    static constexpr U32 BLOOM_MAX_MIPS = 7;
    struct BloomShape
    {
        U32 mWidth = 0;
        U32 mHeight = 0;
        U32 mMipCount = 0;
    };

    /// Requested bloom geometry for an already-scaled native post source.
    /// This is not allocation readiness. HDR/pack/post eligibility is the
    /// caller's policy, and the native renderer owns fallible GPU allocation.
    constexpr BloomShape bloomShape(U32 width, U32 height) const
    {
        if (width == 0 || height == 0) return {};
        constexpr U32 numerator[5] = { 1, 3, 1, 1, 1 };
        constexpr U32 denominator[5] = { 1, 4, 2, 4, 8 };
        const U32 scale = (U32)std::clamp(mBloomScale, 0, 4);
        BloomShape shape{
            std::max(1u, (U32)((U64)width * numerator[scale] / denominator[scale])),
            std::max(1u, (U32)((U64)height * numerator[scale] / denominator[scale])),
        };
        const U32 requested = (U32)std::clamp(mBloomMips, 3, (S32)BLOOM_MAX_MIPS);
        for (U32 mip = 0; mip < requested; ++mip)
        {
            ++shape.mMipCount;
            if (std::max(1u, shape.mWidth >> mip) == 1 &&
                std::max(1u, shape.mHeight >> mip) == 1)
                break;
        }
        return shape;
    }

    /// @a width by @a height as the main view renders it: divided by @a divisor where it divides both into more than
    /// one pixel, scaled by @a multiplier otherwise.
    static constexpr void scaleToRenderResolution(U32& width, U32& height, U32 divisor, F32 multiplier)
    {
        if (divisor > 1 && divisor < width && divisor < height)
        {
            width /= divisor;
            height /= divisor;
        }
        else if (multiplier > 0.f && multiplier != 1.f)
        {
            width = (U32)((F32)width * multiplier);
            height = (U32)((F32)height * multiplier);
        }
    }

    /// The size the main view renders at.
    constexpr void renderSize(U32& width, U32& height) const
    {
        width = mWidth;
        height = mHeight;
        scaleToRenderResolution(width, height, mResolutionDivisor, mResolutionMultiplier);
    }

    /// RenderReflectionProbeResolution as the probes take it: a power of two from 64 to 512.
    static constexpr U32 probeResolution(U32 setting) { return std::bit_ceil(std::clamp(setting, 64u, 512u)); }

    /// ALProbeSuperSample as the probes take it: 1, 2 or 4, since the blur and downsample from a face to its probe
    /// assume a power of two.
    static constexpr U32 probeSuperSample(S32 setting) { return setting >= 4 ? 4u : (setting >= 2 ? 2u : 1u); }

    friend bool operator==(const ALRenderResourceInputs&, const ALRenderResourceInputs&) = default;
};
