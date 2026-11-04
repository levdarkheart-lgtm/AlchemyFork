/**
 * @file lldrawpooltree.cpp
 * @brief LLDrawPoolTree class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#include "lldrawpooltree.h"

#include "lldrawable.h"
#include "llface.h"
#include "pipeline.h"
#include "llviewerregion.h"

LLDrawPoolTree::LLDrawPoolTree(LLViewerTexture *texturep) :
    LLFacePool(POOL_TREE),
    mTexturep(texturep)
{
}

//============================================
// deferred implementation
//============================================
void LLDrawPoolTree::beginDeferredPass(S32)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
}

void LLDrawPoolTree::renderDeferred(S32)
{
    LL_PROFILE_ZONE_SCOPED;

    if (mDrawFace.empty())
    {
        return;
    }

// [SL:KB] - Patch: Render-TextureToggle (Catznip-4.0)
    LLViewerTexture* pTexture = (LLPipeline::sRenderTextures) ? mTexturep.get() : LLViewerFetchedTexture::sDefaultDiffuseImagep.get();
// [/SL:KB]
    mTexturep->addTextureStats(1024.f * 1024.f); // <=== keep Linden tree textures at full res

#if LL_DARWIN
    gPipeline.beginMetalWorldGBufferPass(false, true);
#endif

    for (LLFace* face : mDrawFace)
    {
        if (face && face->getVertexBuffer())
        {
#if LL_DARWIN
            gPipeline.captureMetalWorldTreeBatch(face, pTexture);
#endif
        }
    }

#if LL_DARWIN
    gPipeline.endMetalWorldGBufferPass();
#endif
}

void LLDrawPoolTree::endDeferredPass(S32)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
}

//============================================
// shadow implementation
//============================================
void LLDrawPoolTree::beginShadowPass(S32)
{
    LL_PROFILE_ZONE_SCOPED;
}

void LLDrawPoolTree::renderShadow(S32)
{
}

void LLDrawPoolTree::captureMetalDirectionalShadow()
{
#if LL_DARWIN
    LLViewerTexture* texture = LLPipeline::sRenderTextures
        ? mTexturep.get()
        : LLViewerFetchedTexture::sDefaultDiffuseImagep.get();
    for (LLFace* face : mDrawFace)
    {
        if (!face || !face->getDrawable() ||
            !face->getDrawable()->getRegion())
        {
            continue;
        }
        LLVertexBuffer* buffer = face->getVertexBuffer();
        if (!buffer)
        {
            continue;
        }
        gPipeline.captureMetalSpotShadowTreeBatch(
            buffer, texture,
            &face->getDrawable()->getRegion()->mRenderMatrix);
    }
#endif
}

// virtual
void LLDrawPoolTree::addShadowCasters(ALSpotShadowCache::Casters& casters)
{
    // What renderDeferred draws each face from, with the texture it alpha tests by.
    const LLViewerTexture* texture = LLPipeline::sRenderTextures ? mTexturep.get() : LLViewerFetchedTexture::sDefaultDiffuseImagep.get();
    ALSpotShadowCache::Placements placements;
    for (LLFace* face : mDrawFace)
    {
        const LLVertexBuffer* buffer = face->getVertexBuffer();
        if (!buffer)
        {
            continue;
        }
        ALSpotShadowCache::Hash draw;
        ALSpotShadowCache::foldGeometry(draw, buffer, 0, buffer->getNumVerts() - 1, buffer->getNumIndices(), 0,
                                        placements.of(&face->getDrawable()->getRegion()->mRenderMatrix));
        add_shadow_texture(casters, draw, texture);
        casters.add(draw);
    }
}

void LLDrawPoolTree::endShadowPass(S32)
{
    LL_PROFILE_ZONE_SCOPED;
}

bool LLDrawPoolTree::verify() const
{
    return true;
}

LLViewerTexture *LLDrawPoolTree::getTexture()
{
    return mTexturep;
}

LLViewerTexture *LLDrawPoolTree::getDebugTexture()
{
    return mTexturep;
}


LLColor3 LLDrawPoolTree::getDebugColor() const
{
    return LLColor3(1.f, 0.f, 1.f);
}

