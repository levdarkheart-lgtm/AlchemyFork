/**
 * @file allightmapblur.h
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

#pragma once

#include "v3math.h"

/// The native light-map blur's Gaussian and depth-aware tap contract. Metal consumes these values directly; no
/// application-owned GL shader, image binding or dispatch remains.
class ALLightMapBlur
{
public:
    static constexpr U32 MAX_APRON   = 16;

    /// Four weights out from the centre; the native shader samples halfway between adjacent steps too.
    struct Kernel
    {
        LLVector3 mTaps[4];      ///< kern: the shadow's weight (unused), the occlusion's, and the offset in steps
        F32       mScale = 0.f;  ///< kern_scale: a step in pixels, before the normal and the distance shorten it
        F32       mDistFactor = 0.f; ///< dist_factor: how much the distance shortens a step
    };

    /// The kernel from RenderShadowGaussian's two widths, RenderShadowBlurSize and RenderShadowBlurDistFactor.
    static Kernel kernel(const LLVector3& gaussian, F32 blur_size, F32 dist_factor);

    /// The rows above and below a tile that @a kernel's taps down can reach, or -1 past MAX_APRON.
    static S32 apron(const Kernel& kernel);
};
