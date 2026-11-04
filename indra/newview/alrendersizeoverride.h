/**
 * @file alrendersizeoverride.h
 * @brief The main view rendered at a size of its own for as long as a scope lasts.
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

/// The main view rendered at a size of its own while the scope lasts, before the resolution scale: a snapshot's. On
/// Darwin this records an exact native Metal capture size and never resizes GL resources. Unsupported platforms
/// decline the override; there is no legacy target fallback. No render-input application runs while it lasts. One at
/// a time.
class ALRenderSizeOverride
{
public:
    ALRenderSizeOverride(U32 width, U32 height);
    ~ALRenderSizeOverride();

    ALRenderSizeOverride(const ALRenderSizeOverride&)            = delete;
    ALRenderSizeOverride& operator=(const ALRenderSizeOverride&) = delete;

    /// Whether the requested size was accepted in full.
    bool applied() const { return mApplied; }

private:
    bool mApplied = false;
};
