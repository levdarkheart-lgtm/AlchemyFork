/**
 * @file lldrawpool.cpp
 * @brief LLDrawPoolMaterials class implementation
 * @author Jonathan "Geenz" Goodman
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2013, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "lldrawpoolwaterexclusion.h"

#include "pipeline.h"

LLDrawPoolWaterExclusion::LLDrawPoolWaterExclusion() : LLRenderPass(LLDrawPool::POOL_WATEREXCLUSION)
{
    LL_INFOS("DPInvisible") << "Creating water exclusion draw pool" << LL_ENDL;
}


// The PASS_INVISIBLE prims into the native screen target's stencil: the
// exclude bit wherever one lies in front of the scene and on the camera's
// side of the procedural water surface. No colour and no depth.
void LLDrawPoolWaterExclusion::render(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    (void)pass;
#if LL_DARWIN
    gPipeline.beginMetalWorldWaterExclusionPass();
    auto* at = gPipeline.beginRenderMap(LLRenderPass::PASS_INVISIBLE);
    auto* end = gPipeline.endRenderMap(LLRenderPass::PASS_INVISIBLE);
    while (at != end)
    {
        LLDrawInfo* info = *at;
        LLCullResult::increment_iterator(at, end);
        if (info)
        {
            gPipeline.captureMetalWorldGBufferBatch(*info, false);
        }
    }
    gPipeline.endMetalWorldGBufferPass();
#endif
}
