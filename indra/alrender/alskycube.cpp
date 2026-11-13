/**
 * @file alskycube.cpp
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

#include "linden_common.h"

#include "alskycube.h"

#include "alfarplane.h"
#include <array>

namespace ALSkyCube
{

namespace
{
    // GL's cube lookup (the GL specification's table of cube map face selection): for the major axis' face, s runs
    // along sc and t along tc. Each face's right and up are those axes in the agent's frame.
    const std::array<Face, 6> FACES = { {
        { LLVector3(1.f, 0.f, 0.f), LLVector3(0.f, 0.f, -1.f), LLVector3(0.f, -1.f, 0.f) },  // +x: sc = -z, tc = -y
        { LLVector3(-1.f, 0.f, 0.f), LLVector3(0.f, 0.f, 1.f), LLVector3(0.f, -1.f, 0.f) },  // -x: sc = +z, tc = -y
        { LLVector3(0.f, 1.f, 0.f), LLVector3(1.f, 0.f, 0.f), LLVector3(0.f, 0.f, 1.f) },    // +y: sc = +x, tc = +z
        { LLVector3(0.f, -1.f, 0.f), LLVector3(1.f, 0.f, 0.f), LLVector3(0.f, 0.f, -1.f) },  // -y: sc = +x, tc = -z
        { LLVector3(0.f, 0.f, 1.f), LLVector3(1.f, 0.f, 0.f), LLVector3(0.f, -1.f, 0.f) },   // +z: sc = +x, tc = -y
        { LLVector3(0.f, 0.f, -1.f), LLVector3(-1.f, 0.f, 0.f), LLVector3(0.f, -1.f, 0.f) }, // -z: sc = -x, tc = -y
    } };
}

Face face(U32 face)
{
    return FACES[face];
}

LLVector3 texelDirection(U32 face, U32 x, U32 y, U32 size)
{
    const Face& basis = FACES[face];
    const F32   s     = (F32)(2 * x + 1) / (F32)size - 1.f;
    const F32   t     = (F32)(2 * y + 1) / (F32)size - 1.f;
    LLVector3   dir   = basis.mForward + basis.mRight * s + basis.mUp * t;
    dir.normalize();
    return dir;
}

LLVector3 toDome(const LLVector3& agent)
{
    return ALFarPlane::skyFrame(agent);
}

bool termsAlike(std::span<const F32> a, std::span<const F32> b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i)
    {
        const F32 apart = fabsf(a[i] - b[i]);
        if (!(apart < F_APPROXIMATELY_ZERO || apart < llmax(fabsf(a[i]), fabsf(b[i])) * TERM_TOLERANCE))
        {
            return false;
        }
    }
    return true;
}

} // namespace ALSkyCube
