/**
 * @file alfarplaneoverlay.cpp
 * @brief Develop overlay that draws column pairs out to 100 km with the main projection.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project.
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

#include "llviewerprecompiledheaders.h"

#include "alfarplaneoverlay.h"

#include "llviewercontrol.h"
#include "pipeline.h"

extern bool gCubeSnapshot;

namespace ALFarPlaneOverlay
{

void render()
{
    static LLCachedControl<bool> enabled(gSavedSettings, "AlchemyRenderFarPlaneOverlay", false);
    if (!enabled || gCubeSnapshot)
    {
        return;
    }

#if LL_DARWIN
    // Metal owns the private depth-tested column target and RGB-only composite.
    // Capture CPU geometry directly; no GL target or draw is required.
    gPipeline.captureMetalFarPlaneOverlay();
#endif
}

} // namespace ALFarPlaneOverlay
