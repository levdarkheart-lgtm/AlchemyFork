/**
 * @file alterrainsurfacemaps.cpp
 * @brief A region's terrain as textures: the height field, and the
 *        composition with its noise, each with an apron from the
 *        neighbouring regions.
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

#include "llviewerprecompiledheaders.h"

#include "alterrainsurfacemaps.h"

#include "llimagegl.h"
#include "llrender.h"
#include "llsurface.h"
#include "llsurfacepatch.h"
#include "llviewerregion.h"

ALTerrainSurfaceMaps::ALTerrainSurfaceMaps(LLSurface& surface) :
    mSurface(surface)
{
}

ALTerrainSurfaceMaps::~ALTerrainSurfaceMaps() = default;

#include "alterrainsurfacemaps_readiness_native.inl"

void ALTerrainSurfaceMaps::bind(S32 height_slot, S32 composition_slot)
{
    // A slot below zero is a program with no such sampler: the shadow stages
    // never read the composition.
    if (height_slot >= 0 && mHeight.notNull())
    {
        gGL.getTextureSlot(height_slot)->bindSampled(mHeight.get(), ALSamplers::PointClamp);
    }
    if (composition_slot >= 0 && mComposition.notNull())
    {
        gGL.getTextureSlot(composition_slot)->bindSampled(mComposition.get(), ALSamplers::BilinearClamp);
    }
}

#if LL_DARWIN
const ALMetalTexture* ALTerrainSurfaceMaps::getMetalHeightTexture() const
{
    return mHeight.notNull() ? mHeight->getMetalTexture() : nullptr;
}

const ALMetalTexture* ALTerrainSurfaceMaps::getMetalCompositionTexture() const
{
    return mComposition.notNull() ? mComposition->getMetalTexture() : nullptr;
}

std::shared_ptr<const ALMetalTexture> ALTerrainSurfaceMaps::getMetalHeightTextureLease() const
{
    return mHeight.notNull() ? mHeight->getMetalTextureLease() : nullptr;
}

std::shared_ptr<const ALMetalTexture> ALTerrainSurfaceMaps::getMetalCompositionTextureLease() const
{
    return mComposition.notNull() ? mComposition->getMetalTextureLease() : nullptr;
}
#endif

F32 ALTerrainSurfaceMaps::compositionAt(S32 gx, S32 gy) const
{
    // The composition has no apron of its own; the map is only ever sampled
    // inside the region, and getCompositionXY already reaches east and north
    // for the buffer column and row.
    const LLViewerRegion* region = mSurface.getRegion();
    if (!region)
    {
        return 0.f;
    }
    const S32 last = mSurface.getGridsPerEdge() - 1;
    return region->getCompositionXY(llclamp(gx, 0, last), llclamp(gy, 0, last));
}

F32 ALTerrainSurfaceMaps::noiseAt(S32 gx, S32 gy) const
{
    const LLVector3d& origin = mSurface.getOriginGlobal();
    const F32 meters_per_grid = mSurface.getMetersPerGrid();
    return terrain_composition_noise(origin.mdV[VX] + gx * meters_per_grid, origin.mdV[VY] + gy * meters_per_grid);
}

bool ALTerrainSurfaceMaps::upload(LLPointer<LLImageGL>& image, S32 internal_format, U32 primary_format, U8 components, const std::vector<F32>& data, const Rect& rect, bool whole)
{
    const U8* bytes = reinterpret_cast<const U8*>(data.data());
    bool needs_image = image.isNull() || image->getWidth() != mSize;
    needs_image = needs_image || (image.notNull() &&
        !image->getMetalTexture());
    if (needs_image)
    {
        image = new LLImageGL(mSize, mSize, components, /*usemipmaps=*/false);
        image->setExplicitFormat(internal_format, primary_format, GL_FLOAT);
        return image->createMetalTexture(0, bytes);
    }
    else if (whole)
    {
        return image->uploadMetalSubImage(bytes, mSize, mSize, 0, 0, mSize, mSize);
    }
    else
    {
        // Texel (u, v) holds grid (u - APRON, v - APRON); the staging buffer is the whole map.
        const S32 x = rect.x0 + APRON;
        const S32 y = rect.y0 + APRON;
        const S32 width = rect.x1 - rect.x0 + 1;
        const S32 height = rect.y1 - rect.y0 + 1;
        return image->uploadMetalSubImage(bytes, mSize, mSize, x, y, width, height);
    }
}
