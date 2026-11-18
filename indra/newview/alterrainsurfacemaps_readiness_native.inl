/**
 * @file alterrainsurfacemaps_readiness_native.inl
 * @brief Actual terrain map publication and independent height readiness.
 * Copyright (C) 2026, Alchemy Viewer Project
 * SPDX-License-Identifier: LGPL-2.1-only
 */

void ALTerrainSurfaceMaps::markDirty()
{
    mDirtyWhole = true;
#if LL_DARWIN
    mMetalHeightCurrent = false;
#endif
    ++mVersion;
}

void ALTerrainSurfaceMaps::markDirty(const Rect& rect)
{
    const Rect changed = intersect(rect, wholeMap(mSurface.getGridsPerEdge()));
    mDirtyRect = merge(mDirtyRect, changed);
#if LL_DARWIN
    // Neighbor apron updates can clip wholly outside this map. Preserve the
    // current height publication when this call adds no effective dirty work,
    // including when only composition still awaits a successful upload.
    if (!changed.empty())
    {
        mMetalHeightCurrent = false;
    }
#endif
    ++mVersion;
}

void ALTerrainSurfaceMaps::ensureUploaded()
{
    if ((!mDirtyWhole && mDirtyRect.empty()) || !mSurface.hasZData())
    {
        return;
    }
    if (refresh())
    {
        mDirtyWhole = false;
        mDirtyRect = Rect();
    }
}

#if LL_DARWIN
bool ALTerrainSurfaceMaps::ensureMetalHeightUploaded()
{
    if (mMetalHeightCurrent && getMetalHeightTexture())
    {
        return true;
    }
    mMetalHeightCurrent = false;
    // A native image can disappear after an earlier successful publication.
    // Rebuild it even when no CPU height edit has marked a rectangle dirty.
    if (!mDirtyWhole && mDirtyRect.empty())
    {
        mDirtyWhole = true;
    }
    ensureUploaded();
    return mMetalHeightCurrent && getMetalHeightTexture();
}
#endif

bool ALTerrainSurfaceMaps::refresh()
{
    LL_PROFILE_ZONE_SCOPED;
    const S32 grids_per_edge = mSurface.getGridsPerEdge();
    const S32 size = grids_per_edge + 2 * APRON;
    // The first upload builds both maps whole, and so does one after the
    // surface changed size, which also lays the noise out again.
    if (size != mSize)
    {
        mNoiseReady = false;
    }
    const bool whole = mDirtyWhole || size != mSize || mHeight.isNull() || mComposition.isNull();
    const Rect rect = whole ? wholeMap(grids_per_edge) : mDirtyRect;
    mSize = size;

    bool has_neighbor[8];
    for (U32 dir = 0; dir < 8; ++dir)
    {
        has_neighbor[dir] = mSurface.mNeighbors[dir] != nullptr;
    }
    const auto height = [&](S32 gx, S32 gy, F32* texel)
    {
        texel[0] = mSurface.sampleZ(gx, gy, has_neighbor);
    };

    // The noise is a function of world position alone, so it is computed once
    // per region and kept in the staging buffer across refreshes.
    const bool noise_ready = mNoiseReady;
    const auto composition = [&](S32 gx, S32 gy, F32* texel)
    {
        texel[0] = compositionAt(gx, gy);
        if (!noise_ready)
        {
            texel[1] = noiseAt(gx, gy);
        }
    };
    if (whole)
    {
        fill(mHeightStaging, grids_per_edge, 1, height);
        fill(mCompositionStaging, grids_per_edge, 2, composition);
    }
    else
    {
        fillRect(mHeightStaging, grids_per_edge, 1, rect, height);
        fillRect(mCompositionStaging, grids_per_edge, 2, rect, composition);
    }
    mNoiseReady = true;

    const bool height_ok = upload(mHeight, GL_R32F, GL_RED, 1, mHeightStaging, rect, whole);
#if LL_DARWIN
    // Record height publication independently: composition can fail while
    // this exact height generation remains usable by shadow consumers.
    mMetalHeightCurrent = height_ok;
#endif
    const bool composition_ok = upload(mComposition, GL_RG16F, GL_RG, 2, mCompositionStaging, rect, whole);
    return height_ok && composition_ok;
}
