/**
 * @file alskycube.h
 * @brief WindLight's dome baked into a small cube, the environment map probe-less reflections read.
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
#include "v3math.h"

#include <bit>
#include <span>

/// The environment cube the shiny and probe-less reflections read where reflection probes are off (environmentMap, as
/// LLPipeline::bindDeferredShader and the bump pool's shiny pass bind it): WindLight's dome along each texel's direction,
/// drawn on the GPU (deferred/skyCubeF.glsl) when the sky changes, then mipped. Its readers look it up in the agent's
/// frame (east, north, up), through env_mat, the eye's turn back into it. No viewer state, so the sky and the GL tests
/// bake it alike.
namespace ALSkyCube
{

/// A face's edge, in texels: a power of two, which halves to 1.
constexpr U32 SIZE = 64;
static_assert(std::has_single_bit(SIZE), "a face's edge halves level by level to 1");

/// Its levels, from SIZE down to 1.
constexpr U32 LEVELS = (U32)std::bit_width(SIZE);

/// The width of the strip the six faces are drawn into side by side, in GL's face order, before each is copied into
/// its face.
constexpr U32 STRIP_WIDTH = 6 * SIZE;

/// What each colour channel of every face and level holds until the cube is first baked, and for as long as the bake
/// cannot run: mid grey as the readers take it, who double what the cube holds (applyLegacyEnv). Its alpha is 1.
constexpr U8 NEUTRAL = 64;

/// The directions GL's cube lookup selects face @a face (GL_TEXTURE_CUBE_MAP_POSITIVE_X + face) for, in the agent's
/// frame: the texel at s and t, each running -1 to 1 across the face from the edge of its first texel to the far edge
/// of its last, lies along mForward + s mRight + t mUp.
struct Face
{
    LLVector3 mForward;
    LLVector3 mRight;
    LLVector3 mUp;
};
Face face(U32 face);

/// The direction through the centre of texel (@a x, @a y) of face @a face, @a size a side, normalized, in the agent's
/// frame.
LLVector3 texelDirection(U32 face, U32 x, U32 y, U32 size = SIZE);

/// A direction in the agent's frame (east, north, up) in the dome's (north, up, east): ALFarPlane::skyFrame, as
/// LLEnvironment::toLightNorm turns the light into it.
LLVector3 toDome(const LLVector3& agent);

/// How far apart two values of a term the bake is worked from may lie, as a fraction of the larger, and still bake
/// alike: a sky that has drifted less than this from the one last baked differs from its bake by less than eight bits
/// keep, so a day cycle bakes again only once it has drifted further.
constexpr F32 TERM_TOLERANCE = 0.0005f;

/// Whether the terms of two skies, @a a and @a b, bake alike: as many of each, and every pair within TERM_TOLERANCE of
/// the larger of the two, or within F_APPROXIMATELY_ZERO of each other.
bool termsAlike(std::span<const F32> a, std::span<const F32> b);

} // namespace ALSkyCube
