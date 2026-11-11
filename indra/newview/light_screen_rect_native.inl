/**
 * @file light_screen_rect_native.inl
 * @brief Rendering pipeline.
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

void setMetalLightScreenRect(
    const F32* position_radius, U32* screen_rect,
    const LLMatrix4a& projection, U32 width, U32 height)
{
    screen_rect[0] = 0;
    screen_rect[1] = 0;
    screen_rect[2] = width;
    screen_rect[3] = height;
    if (width == 0 || height == 0)
    {
        return;
    }

    // The original renderer rasterizes a light's bounding cube when the
    // camera is outside. Metal keeps the existing fragment math, but
    // constrains it to the conservative projection of that same volume. If
    // the volume crosses the eye plane, retain full-screen behavior.
    const F32 radius = position_radius[3];
    F32 minimum_x = std::numeric_limits<F32>::infinity();
    F32 minimum_y = std::numeric_limits<F32>::infinity();
    F32 maximum_x = -std::numeric_limits<F32>::infinity();
    F32 maximum_y = -std::numeric_limits<F32>::infinity();
    for (U32 corner = 0; corner < 8; ++corner)
    {
        LLVector4a eye_corner(
            position_radius[0] + ((corner & 1u) ? radius : -radius),
            position_radius[1] + ((corner & 2u) ? radius : -radius),
            position_radius[2] + ((corner & 4u) ? radius : -radius),
            1.f);
        LLVector4a clip;
        projection.transform4(eye_corner, clip);
        if (!std::isfinite(clip[0]) || !std::isfinite(clip[1]) ||
            !std::isfinite(clip[3]) || clip[3] <= 0.0001f)
        {
            return;
        }
        const F32 inverse_w = 1.f / clip[3];
        const F32 x = clip[0] * inverse_w;
        const F32 y = clip[1] * inverse_w;
        minimum_x = llmin(minimum_x, x);
        minimum_y = llmin(minimum_y, y);
        maximum_x = llmax(maximum_x, x);
        maximum_y = llmax(maximum_y, y);
    }

    const F32 width_f = static_cast<F32>(width);
    const F32 height_f = static_cast<F32>(height);
    const S32 left = llclamp(
        static_cast<S32>(std::floor((minimum_x * 0.5f + 0.5f) * width_f)) - 1,
        0, static_cast<S32>(width));
    const S32 right = llclamp(
        static_cast<S32>(std::ceil((maximum_x * 0.5f + 0.5f) * width_f)) + 1,
        0, static_cast<S32>(width));
    // World clip Y is flipped before rasterization, so offscreen row zero
    // retains GL's lower row. Bounds follow that storage, not presentation Y.
    const S32 top = llclamp(
        static_cast<S32>(std::floor((minimum_y * 0.5f + 0.5f) * height_f)) - 1,
        0, static_cast<S32>(height));
    const S32 bottom = llclamp(
        static_cast<S32>(std::ceil((maximum_y * 0.5f + 0.5f) * height_f)) + 1,
        0, static_cast<S32>(height));
    if (right <= left || bottom <= top)
    {
        return;
    }
    screen_rect[0] = static_cast<U32>(left);
    screen_rect[1] = static_cast<U32>(top);
    screen_rect[2] = static_cast<U32>(right - left);
    screen_rect[3] = static_cast<U32>(bottom - top);
}
