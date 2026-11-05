/**
 * @file allightmapblur.cpp
 * @brief Backend-neutral constants for the native light-map ambient-occlusion blur.
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

#include "linden_common.h"

#include "allightmapblur.h"

#include "llmath.h"

#include <cmath>

// static
ALLightMapBlur::Kernel ALLightMapBlur::kernel(const LLVector3& gaussian, F32 blur_size, F32 dist_factor)
{
    // Sampled symmetrically, the middle sample falling exactly on 0.
    Kernel kernel;
    for (U32 i = 0; i < 4; ++i)
    {
        const F32 x = (F32)i;
        kernel.mTaps[i].set(llgaussian(x, gaussian.mV[VX]), llgaussian(x, gaussian.mV[VY]), x);
    }
    kernel.mScale      = blur_size * (4 / 2.f - 0.5f);
    kernel.mDistFactor = dist_factor;
    return kernel;
}

// static
S32 ALLightMapBlur::apron(const Kernel& kernel)
{
    // A tap lies at its offset times two steps out from an origin the checkerboard moves by a quarter of the second
    // weight's offset, a step no longer than the scale: the normal and the distance only shorten it.
    F32 offset = 0.f;
    for (const LLVector3& tap : kernel.mTaps)
    {
        offset = llmax(offset, std::fabs(tap.mV[VZ]));
    }
    const F32 reach = std::fabs(kernel.mScale) * (2.f * offset + 0.25f * std::fabs(kernel.mTaps[1].mV[VZ]));
    if (!std::isfinite(reach) || reach >= (F32)(MAX_APRON - 1))
    {
        return -1;
    }
    // A tap reads the row its offset lands in, which is at most ceil(reach) away; one more for the arithmetic.
    return (S32)std::ceil(reach) + 1;
}
