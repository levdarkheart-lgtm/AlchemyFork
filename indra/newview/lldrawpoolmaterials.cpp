/**
 * @file lldrawpoolmaterials.cpp
 * @brief LLDrawPoolMaterials class implementation
 * @author Jonathan "Geenz" Goodman
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2013, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation;
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

#include "lldrawpoolmaterials.h"
#include "pipeline.h"
#include "llspatialpartition.h"

LLDrawPoolMaterials::LLDrawPoolMaterials()
:  LLRenderPass(LLDrawPool::POOL_MATERIALS)
{
}

S32 LLDrawPoolMaterials::getNumDeferredPasses()
{
    // One pass per description for each of the non-rigged and rigged maps.
    return static_cast<S32>(DEFERRED_PASSES.size() * 2);
}

void LLDrawPoolMaterials::beginDeferredPass(S32)
{
}

void LLDrawPoolMaterials::endDeferredPass(S32 pass)
{
    LLRenderPass::endRenderPass(pass);
}

void LLDrawPoolMaterials::renderDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_MATERIAL;

    bool rigged = false;
    const S32 pass_count = static_cast<S32>(DEFERRED_PASSES.size());
    if (pass >= pass_count)
    {
        rigged = true;
        pass -= pass_count;
    }
    llassert(pass >= 0 && pass < pass_count);

    const DeferredPassDescription& description = DEFERRED_PASSES[pass];
    const U32 type = description.render_type + (rigged ? 1 : 0);

#if LL_DARWIN
    gPipeline.beginMetalWorldMaterialPass(
        rigged, description.alphaMode(),
        description.hasNormalMap(), description.hasSpecularMap());
    auto* at = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    while (at != end)
    {
        LLDrawInfo* info = *at;
        LLCullResult::increment_iterator(at, end);
        if (info)
        {
            gPipeline.captureMetalWorldMaterialBatch(
                *info, info->mMaterialSlotList.size() > 1);
        }
    }
    gPipeline.endMetalWorldGBufferPass();
#endif
}
