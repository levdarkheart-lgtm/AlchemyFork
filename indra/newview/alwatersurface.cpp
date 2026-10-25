/**
 * @file alwatersurface.cpp
 * @brief The water's field for the frame: its layout around the camera and the map of cell water heights.
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

#include "alwatersurface.h"

#include "alfarplane.h"
#if LL_DARWIN
#include "../almetal/almetalworld.h"
#endif
#include "alwaterheightbake.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h"
#include "llcamera.h"
#include "lldrawpoolwater.h"
#include "llenvironment.h"
#include "llglslshader.h"
#include "llimage.h"
#include "llimagegl.h"
#include "llregionhandle.h"
#include "llrender.h"
#include "llshadermgr.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexture.h"
#include "llworld.h"
#include "pipeline.h"
#include "workqueue.h"

#include <cmath>
#include <cstring>
#include <memory>

ALWaterSurface::WaveSettings ALWaterSurface::waveSettings()
{
    static LLCachedControl<F32> wave_height(gSavedSettings, "AlchemyRenderWaterWaveHeight", 0.5f);
    static LLCachedControl<F32> tess_density(gSavedSettings, "AlchemyRenderWaterTessDensity", 32.f);
    WaveSettings out;
    out.mHeight = llclamp((F32)wave_height, 0.f, 4.f);
    out.mDensity = llclamp((F32)tess_density, 8.f, 256.f);
    return out;
}

bool ALWaterSurface::wavesWanted()
{
    return gPipeline.shaderInputs().mLocal.mWaterWaves;
}

void ALWaterSurface::update()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    static LLCachedControl<bool> render_water(gSavedSettings, "RenderWater", true);
    if (!render_water)
    {
        release();
        return;
    }

    LLViewerRegion* agent_region = gAgent.getRegion();
    if (!agent_region)
    {
        return;
    }

    mScratch.mRegions.clear();
    for (LLViewerRegion* region : LLWorld::getInstance()->getRegionList())
    {
        U32 x = 0;
        U32 y = 0;
        from_region_handle(region->getHandle(), &x, &y);
        const S32 cells = llmax(1, (S32)(region->getWidth() / REGION_WIDTH_METERS));
        mScratch.mRegions.push_back({ (S32)(x / REGION_WIDTH_U32), (S32)(y / REGION_WIDTH_U32), cells, region->getWaterHeight() });
    }
    const F32 default_height = agent_region->getWaterHeight();
    F32 lowest = default_height;
    F32 highest = default_height;
    for (const ALWaterField::Region& region : mScratch.mRegions)
    {
        lowest = llmin(lowest, region.mWaterHeight);
        highest = llmax(highest, region.mWaterHeight);
    }

    // The maps the pool binds this frame, followed before anything is drawn, so the waves' bound is the bound maps'.
    // Their means are read only by the tiled reads.
    const S32 hextile = gPipeline.shaderInputs().mGlobal.mWaterHextile;
    const bool waves = wavesWanted() && gWaterWavesProgram.mProgramObject != 0;
    if (LLDrawPoolWater* pool = static_cast<LLDrawPoolWater*>(gPipeline.findPool(LLDrawPool::POOL_WATER)))
    {
        trackMaps(pool->getNormalMap(0), pool->getNormalMap(1), hextile >= 1, waves);
    }

    // The waves' bound, A: each map's dial times its rise times the most its layers move the surface, a tiled layer's by
    // up to sqrt(3) more, the blend bands' variance gain at its peak; the larger of the two maps'.
    const WaveSettings settings = waveSettings();
    F32 max_displacement = 0.f;
    if (waves)
    {
        const F32 big_gain = hextile >= 1 ? F_SQRT3 : 1.f;
        const F32 mid_gain = hextile >= 2 ? F_SQRT3 : 1.f;
        for (const LLUUID& id : mTracked)
        {
            const MapStats* stats = findMapStats(id);
            if (stats && stats->mHeights.notNull())
            {
                const F32 rise = (F32)llclamp((LLFrameTimer::getTotalSeconds() - stats->mLanded) / RAMP_SECONDS, 0.0, 1.0);
                max_displacement = llmax(max_displacement, settings.mHeight * rise * (big_gain * stats->mMaxBig + mid_gain * stats->mMaxMid));
            }
        }
        // The heights are read from halves rounded to nearest, up to 2^-11 past the field they were baked from, and
        // worked in float from there: the culls test against A strictly, so it is let out by more than both.
        if (max_displacement > 0.f)
        {
            max_displacement = max_displacement * (1.f + 1.f / 1024.f) + 0.01f;
        }
    }

    // The reach the waves are tessellated to, across the plane: as far as a point lies within the reach of the camera
    // itself, so none is tessellated from a height where its edges would be whole.
    LLViewerCamera* camera = LLViewerCamera::getInstance();
    const LLVector3 eye_agent = camera->getOrigin();
    F64 reach = 0.0;
    if (waves && max_displacement > 0.f)
    {
        const F32 above = llmax(0.f, llmax(eye_agent.mV[VZ] - highest, lowest - eye_agent.mV[VZ]));
        reach = ALWaterField::reachAcross(ALWaterField::tessReach(settings.mDensity), (F64)above);
    }

    // The draw distance and the projection as the main view takes them this frame (display_update_camera): probe
    // captures and mirrors set the camera's own, and the field is laid out before they draw and before the main view's
    // are set.
    const LLVector3d eye = gAgent.getPosGlobalFromAgent(eye_agent);
    const F32 draw_distance = ALFarPlane::clampDrawDistance(gAgentCamera.mDrawDistance, LLRender::sReverseZ);
    static LLCachedControl<S32> force_projection(gSavedSettings, "AlchemyRenderFarPlaneForce", ALFarPlane::FORCE_AUTOMATIC);
    const bool infinite = ALFarPlane::isInfinite(
        ALFarPlane::projectionFar(LLRender::sReverseZ, false, force_projection(), gAgentCamera.mDrawDistance));
    const ALWaterField::Layout layout = ALWaterField::layout(eye.mdV[VX], eye.mdV[VY], REGION_WIDTH_METERS, draw_distance,
                                                             ALFarPlane::edgeWaterStretch(infinite), mScratch.mRegions, reach);
    if (layout.mCells == 0)
    {
        // A camera at no finite place lays out nothing; the last frame's field stays.
        return;
    }

    mLayout = layout;
    mEye = eye_agent;
    mVoidDrawn = gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_VOIDWATER);
    mMaxDisplacement = max_displacement;
    mWavesDrawn = reach > 0.0 && mLayout.mPatchMaxX >= mLayout.mPatchMinX && mLayout.mPatchMaxY >= mLayout.mPatchMinY;

    const LLVector3d agent_origin = agent_region->getOriginGlobal();
    mSquare = ALWaterField::cellSquare(mLayout, agent_origin.mdV[VX], agent_origin.mdV[VY]);
    mCentre.set(mSquare.mV[VX] + 0.5f * mSquare.mV[VW] * mSquare.mV[VZ], mSquare.mV[VY] + 0.5f * mSquare.mV[VW] * mSquare.mV[VZ]);
    mDefaultHeight = default_height;
    mLowest = lowest;
    mHighest = highest;

    mScratch.mCentreX = mLayout.mCentreX;
    mScratch.mCentreY = mLayout.mCentreY;
    mScratch.mCells = mLayout.mCells;
    mScratch.mDefaultHeight = mDefaultHeight;
    if (mCellMap.notNull() && mScratch == mKey)
    {
        return;
    }
    std::swap(mKey, mScratch);

    ALWaterField::fillCells(mLayout, mKey.mRegions, mKey.mDefaultHeight, mTexels);
    const U8* bytes = reinterpret_cast<const U8*>(mTexels.data());
    if (mCellMap.isNull())
    {
        mCellMap = new LLImageGL(ALWaterField::MAP_SIZE, ALWaterField::MAP_SIZE, 2, /*usemipmaps=*/false);
        mCellMap->setNeedsAlphaAndPickMask(false);
        mCellMap->setExplicitFormat(GL_RG32F, GL_RG, GL_FLOAT);
        mCellMap->createGLTexture(0, bytes);
    }
    else
    {
        mCellMap->setImage(bytes);
        gGL.getTextureSlot(0)->unbind();
    }
}

void ALWaterSurface::release()
{
    mCellMap = nullptr;
    mKey = Key();
    mWavesDrawn = false;
    mMaxDisplacement = 0.f;
    // The maps' heights are GL textures; their texels stay, and are uploaded again when next wanted. A bake still
    // running lands on its map as before.
    for (MapStats& stats : mMaps)
    {
        stats.mHeights = nullptr;
    }
}

void ALWaterSurface::bind(LLGLSLShader& shader) const
{
    if (mCellMap.notNull())
    {
        const S32 channel = shader.enableTexture(LLShaderMgr::WATER_CELL_MAP);
        if (channel > -1)
        {
            gGL.getTextureSlot(channel)->bindSampled(mCellMap.get(), ALSamplers::PointClamp);
        }
        shader.uniform4fv(LLShaderMgr::WATER_CELL_SQUARE, 1, mSquare.mV);
    }
    else
    {
        // No cells: the square has none a side, so every point is past it.
        const LLVector4 none(0.f, 0.f, REGION_WIDTH_METERS, 0.f);
        shader.uniform4fv(LLShaderMgr::WATER_CELL_SQUARE, 1, none.mV);
    }
    shader.uniform1f(LLShaderMgr::WATER_DEFAULT_HEIGHT, mDefaultHeight);

    shader.uniform4f(LLShaderMgr::WATER_RINGS, (F32)mLayout.mHalf, (F32)mLayout.mRings, (F32)mLayout.mRim, mVoidDrawn ? 1.f : 0.f);
}

#if LL_DARWIN
bool ALWaterSurface::fillMetalWaterExclusionSurface(
    ALMetalWorldWaterExclusionSurface& surface,
    const LLViewerCamera& camera) const
{
    surface = {};
    if (mCellMap.isNull() || !mCellMap->getMetalTexture())
    {
        return false;
    }
    LLMatrix4a eye_to_agent;
    if (!eye_to_agent.setAffineInverse(camera.getModelview()))
    {
        return false;
    }
    surface.cell_map = mCellMap->getMetalTexture();
    std::memcpy(surface.eye_to_agent, eye_to_agent.getF32ptr(),
                sizeof(surface.eye_to_agent));
    std::memcpy(surface.cell_square, mSquare.mV,
                sizeof(surface.cell_square));
    const LLVector3& camera_position = camera.getOrigin();
    std::memcpy(surface.camera_default_height, camera_position.mV,
                sizeof(F32) * 3);
    surface.camera_default_height[3] = mDefaultHeight;
    return true;
}

bool ALWaterSurface::fillMetalWaterHaze(
    ALMetalWorldWaterHaze& haze, const LLViewerCamera& camera,
    bool above_water) const
{
    haze = {};
    haze.above_water = above_water;

    LLMatrix4a inverse_projection;
    inverse_projection.setInverse(camera.getProjection());
    std::memcpy(haze.inverse_projection,
                inverse_projection.getF32ptr(),
                sizeof(haze.inverse_projection));
    std::memcpy(haze.water_plane, LLDrawPoolAlpha::sWaterPlane.mV,
                sizeof(haze.water_plane));

    const LLEnvironment::EnvironmentUBOData& environment =
        LLEnvironment::instance().getEnvironmentUBOData();
    std::memcpy(haze.fog_color, environment.waterFogColor,
                sizeof(haze.fog_color));
    haze.fog[0] = environment.waterFogKS;
    haze.fog[1] = environment.waterFogDensity;

    if (!above_water)
    {
        return true;
    }
    if (mCellMap.isNull() || !mCellMap->getMetalTexture())
    {
        return false;
    }

    LLMatrix4a modelview_projection;
    modelview_projection.setMul(camera.getModelview(),
                                camera.getProjection());
    std::memcpy(haze.modelview_projection,
                modelview_projection.getF32ptr(),
                sizeof(haze.modelview_projection));
    haze.cell_map = mCellMap->getMetalTexture();
    std::memcpy(haze.cell_square, mSquare.mV,
                sizeof(haze.cell_square));
    haze.default_height[0] = mDefaultHeight;
    haze.rings[0] = static_cast<F32>(mLayout.mHalf);
    haze.rings[1] = static_cast<F32>(mLayout.mRings);
    haze.rings[2] = static_cast<F32>(mLayout.mRim);
    haze.rings[3] = mVoidDrawn ? 1.f : 0.f;

    const ALWaterField::FieldDraws field = ALWaterField::fieldDraws(
        mLayout, ALWaterField::PatchRect());
    for (const ALWaterField::Draw& draw : field)
    {
        if (draw.mPart == ALWaterField::Draw::FRINGE ||
            draw.mVertices <= 0 || draw.mInstances <= 0 ||
            haze.draw_count >= haze.draws.size())
        {
            continue;
        }
        ALMetalWorldWaterFieldDraw& destination =
            haze.draws[haze.draw_count++];
        destination.part = static_cast<U32>(draw.mPart);
        destination.vertex_count = static_cast<U32>(draw.mVertices);
        destination.instance_count = static_cast<U32>(draw.mInstances);
        destination.triangle_strip = draw.mStrip;
    }
    return haze.draw_count == 3;
}

bool ALWaterSurface::fillMetalWaterWaves(
    ALMetalWorldWaterSurface& surface,
    const LLViewerCamera& camera,
    const LLViewerTexture* current,
    const LLViewerTexture* next,
    bool occlusion) const
{
    if (!mWavesDrawn || occlusion || mCellMap.isNull() ||
        mTexels.size() < static_cast<size_t>(
            ALWaterField::MAP_SIZE * ALWaterField::MAP_SIZE * 2))
    {
        return false;
    }

    const Heights heights[2] = {
        mapHeights(current), mapHeights(next)
    };
    const ALMetalTexture* height_maps[2] = {
        heights[0].mTexture ? heights[0].mTexture->getMetalTexture()
                            : nullptr,
        heights[1].mTexture ? heights[1].mTexture->getMetalTexture()
                            : nullptr,
    };
    const ALMetalTexture* fallback = height_maps[0]
        ? height_maps[0] : height_maps[1];
    if (!fallback)
    {
        return false;
    }
    surface.height_maps[0] = height_maps[0] ? height_maps[0] : fallback;
    surface.height_maps[1] = height_maps[1] ? height_maps[1] : fallback;

    const WaveSettings settings = waveSettings();
    surface.wave_eye_density[0] = mEye.mV[VX];
    surface.wave_eye_density[1] = mEye.mV[VY];
    surface.wave_eye_density[2] = mEye.mV[VZ];
    surface.wave_eye_density[3] = settings.mDensity;
    surface.wave_reach_amplitude[0] = static_cast<F32>(mLayout.mReach);
    surface.wave_reach_amplitude[1] = mMaxDisplacement;
    surface.wave_reach_amplitude[2] = height_maps[0]
        ? settings.mHeight * heights[0].mRise : 0.f;
    surface.wave_reach_amplitude[3] = height_maps[1]
        ? settings.mHeight * heights[1].mRise : 0.f;

    const ALWaterField::PatchRect rect =
        ALWaterField::patchRect(mLayout);
    if (rect.empty())
    {
        return false;
    }
    surface.patch_rect[0] = rect.mMinX;
    surface.patch_rect[1] = rect.mMinY;
    surface.patch_rect[2] = rect.mEndX;
    surface.patch_rect[3] = rect.mEndY;
    const LLVector3 at = camera.getAtAxis();
    const ALWaterField::PatchOrder order =
        ALWaterField::patchOrder(at.mV[VX], at.mV[VY]);
    std::memcpy(surface.patch_order, order.mOrder,
                sizeof(surface.patch_order));

    surface.field.draws = {};
    surface.field.draw_count = 0;
    for (const ALWaterField::Draw& draw :
         ALWaterField::fieldDraws(mLayout, rect))
    {
        if (draw.mVertices <= 0 || draw.mInstances <= 0 ||
            surface.field.draw_count >= surface.field.draws.size())
        {
            continue;
        }
        ALMetalWorldWaterFieldDraw& destination =
            surface.field.draws[surface.field.draw_count++];
        destination.part = static_cast<U32>(draw.mPart);
        destination.vertex_count = static_cast<U32>(draw.mVertices);
        destination.instance_count = static_cast<U32>(draw.mInstances);
        destination.triangle_strip = draw.mStrip;
    }
    if (surface.field.draw_count != 4)
    {
        return false;
    }

    constexpr S32 cull_plane_count = 5;
    const S32 plane_indices[cull_plane_count] = {
        LLCamera::AGENT_PLANE_LEFT,
        LLCamera::AGENT_PLANE_RIGHT,
        LLCamera::AGENT_PLANE_NEAR,
        LLCamera::AGENT_PLANE_BOTTOM,
        LLCamera::AGENT_PLANE_TOP,
    };
    F32 planes[cull_plane_count][4]{};
    for (S32 plane_index = 0; plane_index < cull_plane_count;
         ++plane_index)
    {
        const LLPlane& plane = camera.getAgentPlane(
            plane_indices[plane_index]);
        for (S32 component = 0; component < 4; ++component)
        {
            planes[plane_index][component] = plane[component];
        }
    }

    constexpr S32 patches_per_cell =
        ALWaterField::PATCHES_PER_CELL;
    const S32 patches_x =
        (rect.mEndX - rect.mMinX) * patches_per_cell;
    const S32 patches_y =
        (rect.mEndY - rect.mMinY) * patches_per_cell;
    const S32 patch_count = patches_x * patches_y;
    if (patch_count <= 0 || patch_count != patchCount())
    {
        return false;
    }
    surface.patch_count = static_cast<U32>(patch_count);
    surface.tessellation_factors.assign(
        static_cast<size_t>(patch_count) * 6, 0);

    struct PatchCorner
    {
        F32 x;
        F32 y;
        F32 z;
    };
    const F32 side = mSquare.mV[VW];
    const F32 width = mSquare.mV[VZ];
    const F32 centre_x = mSquare.mV[VX] + 0.5f * side * width;
    const F32 centre_y = mSquare.mV[VY] + 0.5f * side * width;
    const LLVector3 eye = camera.getOrigin();
    const auto edge_level = [&](const PatchCorner& a,
                                const PatchCorner& b)
    {
        const F32 ab_x = b.x - a.x;
        const F32 ab_y = b.y - a.y;
        const F32 ab_length_sq = ab_x * ab_x + ab_y * ab_y;
        const F32 eye_x = mEye.mV[VX] - a.x;
        const F32 eye_y = mEye.mV[VY] - a.y;
        const F32 along = llclamp(
            (eye_x * ab_x + eye_y * ab_y) /
                llmax(ab_length_sq, 1e-6f),
            0.f, 1.f);
        const F32 reach_x = a.x + along * ab_x - mEye.mV[VX];
        const F32 reach_y = a.y + along * ab_y - mEye.mV[VY];
        if (sqrtf(reach_x * reach_x + reach_y * reach_y) >=
            surface.wave_reach_amplitude[0])
        {
            return 1.f;
        }
        const F32 ab_z = b.z - a.z;
        const F32 length = sqrtf(
            ab_x * ab_x + ab_y * ab_y + ab_z * ab_z);
        const F32 mid_x = 0.5f * (a.x + b.x) - eye.mV[VX];
        const F32 mid_y = 0.5f * (a.y + b.y) - eye.mV[VY];
        const F32 mid_z = 0.5f * (a.z + b.z) - eye.mV[VZ];
        const F32 distance = sqrtf(
            mid_x * mid_x + mid_y * mid_y + mid_z * mid_z);
        return llclamp(
            length * settings.mDensity / llmax(distance, 1.f),
            1.f, 64.f);
    };

    for (S32 instance = 0; instance < patch_count; ++instance)
    {
        const bool slow_x = order.mOrder[0] != 0;
        const S32 run = slow_x ? patches_y : patches_x;
        S32 slow = instance / run;
        S32 fast = instance - slow * run;
        slow = order.mOrder[1] != 0
            ? (slow_x ? patches_x : patches_y) - 1 - slow : slow;
        fast = order.mOrder[2] != 0 ? run - 1 - fast : fast;
        const S32 at_x = slow_x ? slow : fast;
        const S32 at_y = slow_x ? fast : slow;
        const S32 cell_x = rect.mMinX + at_x / patches_per_cell;
        const S32 cell_y = rect.mMinY + at_y / patches_per_cell;
        const S32 square_x = at_x % patches_per_cell;
        const S32 square_y = at_y % patches_per_cell;
        const size_t texel_index = static_cast<size_t>(
            (cell_y * ALWaterField::MAP_SIZE + cell_x) * 2);
        const F32 water_height = mTexels[texel_index];
        const bool drawn = mVoidDrawn ||
            (static_cast<S32>(mTexels[texel_index + 1]) & 1) != 0;

        std::array<PatchCorner, 4> corners{};
        constexpr S32 corner_x[4] = { 0, 1, 1, 0 };
        constexpr S32 corner_y[4] = { 0, 0, 1, 1 };
        for (S32 corner = 0; corner < 4; ++corner)
        {
            const S32 dx = drawn ? corner_x[corner] : 0;
            const S32 dy = drawn ? corner_y[corner] : 0;
            const F32 local_x = static_cast<F32>(cell_x) +
                static_cast<F32>(square_x + dx) /
                    static_cast<F32>(patches_per_cell);
            const F32 local_y = static_cast<F32>(cell_y) +
                static_cast<F32>(square_y + dy) /
                    static_cast<F32>(patches_per_cell);
            corners[corner] = {
                centre_x + (local_x - 0.5f * side) * width,
                centre_y + (local_y - 0.5f * side) * width,
                water_height,
            };
        }

        bool visible = drawn &&
            (corners[0].x != corners[2].x ||
             corners[0].y != corners[2].y);
        if (visible)
        {
            F32 lo[3] = { corners[0].x, corners[0].y,
                          water_height - mMaxDisplacement };
            F32 hi[3] = { corners[0].x, corners[0].y,
                          water_height + mMaxDisplacement };
            for (const PatchCorner& corner : corners)
            {
                lo[0] = llmin(lo[0], corner.x);
                lo[1] = llmin(lo[1], corner.y);
                hi[0] = llmax(hi[0], corner.x);
                hi[1] = llmax(hi[1], corner.y);
            }
            const F32 centre[3] = {
                0.5f * (lo[0] + hi[0]),
                0.5f * (lo[1] + hi[1]),
                0.5f * (lo[2] + hi[2]),
            };
            const F32 radius[3] = {
                0.5f * (hi[0] - lo[0]),
                0.5f * (hi[1] - lo[1]),
                0.5f * (hi[2] - lo[2]),
            };
            for (S32 plane_index = 0;
                 visible && plane_index < cull_plane_count;
                 ++plane_index)
            {
                F32 distance = planes[plane_index][3];
                for (S32 component = 0; component < 3; ++component)
                {
                    const F32 normal =
                        planes[plane_index][component];
                    const F32 inner = centre[component] -
                        radius[component] *
                            (normal > 0.f ? 1.f
                             : normal < 0.f ? -1.f : 0.f);
                    distance += normal * inner;
                }
                visible = distance <= 0.f;
            }
        }
        if (!visible)
        {
            continue;
        }

        const F32 outer[4] = {
            edge_level(corners[0], corners[3]),
            edge_level(corners[0], corners[1]),
            edge_level(corners[1], corners[2]),
            edge_level(corners[3], corners[2]),
        };
        const F32 inner[2] = {
            llmax(outer[1], outer[3]),
            llmax(outer[0], outer[2]),
        };
        const size_t factor = static_cast<size_t>(instance) * 6;
        for (S32 edge = 0; edge < 4; ++edge)
        {
            surface.tessellation_factors[factor + edge] =
                ALWaterHeightBake::halfOf(outer[edge]);
        }
        surface.tessellation_factors[factor + 4] =
            ALWaterHeightBake::halfOf(inner[0]);
        surface.tessellation_factors[factor + 5] =
            ALWaterHeightBake::halfOf(inner[1]);
    }
    surface.waves = true;
    return true;
}
#endif

void ALWaterSurface::draw(LLGLSLShader& shader, bool patched) const
{
    if (mCellMap.isNull())
    {
        return;
    }
    // The fringe only where the patches are drawn with the field, bindWaves having set their rectangle.
    const ALWaterField::PatchRect rect = patched && mWavesDrawn ? ALWaterField::patchRect(mLayout) : ALWaterField::PatchRect();
    for (const ALWaterField::Draw& draw : ALWaterField::fieldDraws(mLayout, rect))
    {
        if (draw.mInstances <= 0)
        {
            continue;
        }
        shader.uniform1i(LLShaderMgr::WATER_LAYOUT, (S32)draw.mPart);
        gGL.drawArraysInstanced(draw.mStrip ? LLRender::TRIANGLE_STRIP : LLRender::TRIANGLES, 0, (U32)draw.mVertices,
                                (U32)draw.mInstances);
        ++gPipeline.mWaterDraws;
    }
}

bool ALWaterSurface::rim(const LLVector3& eye, LLVector2& lo, LLVector2& hi) const
{
    if (mCellMap.isNull())
    {
        return false;
    }
    const F32 rim = (F32)mLayout.mRim;
    lo.set(mCentre.mV[VX] - rim - eye.mV[VX], mCentre.mV[VY] - rim - eye.mV[VY]);
    hi.set(mCentre.mV[VX] + rim - eye.mV[VX], mCentre.mV[VY] + rim - eye.mV[VY]);
    return true;
}

bool ALWaterSurface::inView(LLCamera& camera) const
{
    if (mCellMap.isNull())
    {
        return false;
    }
    // The waves reach A above the highest water and below the lowest.
    const F32 rim = (F32)mLayout.mRim;
    const LLVector4a centre(mCentre.mV[VX], mCentre.mV[VY], 0.5f * (mLowest + mHighest));
    const LLVector4a radius(rim, rim, 0.5f * (mHighest - mLowest) + maxDisplacement() + 0.01f);
    return camera.AABBInFrustumNoFarClip(centre, radius) != 0;
}

bool ALWaterSurface::drawnFrom(LLViewerCamera& camera) const
{
    return exists() &&
           ALFarPlane::waterVisibleFrom(camera.getOrigin().mV[VZ], LLEnvironment::instance().getWaterHeight(), camera.getProjectionFar()) &&
           inView(camera);
}

bool ALWaterSurface::patchesInView(LLCamera& camera) const
{
    if (!mWavesDrawn || mCellMap.isNull())
    {
        return false;
    }
    const ALWaterField::PatchRect rect = ALWaterField::patchRect(mLayout);
    const F32 width = mSquare.mV[VZ];
    const F32 x0 = mSquare.mV[VX] + (F32)rect.mMinX * width;
    const F32 y0 = mSquare.mV[VY] + (F32)rect.mMinY * width;
    const F32 x1 = mSquare.mV[VX] + (F32)rect.mEndX * width;
    const F32 y1 = mSquare.mV[VY] + (F32)rect.mEndY * width;
    const LLVector4a centre(0.5f * (x0 + x1), 0.5f * (y0 + y1), 0.5f * (mLowest + mHighest));
    const LLVector4a radius(0.5f * (x1 - x0), 0.5f * (y1 - y0), 0.5f * (mHighest - mLowest) + mMaxDisplacement + 0.01f);
    return camera.AABBInFrustumNoFarClip(centre, radius) != 0;
}

void ALWaterSurface::addExtents(LLVector3& min, LLVector3& max) const
{
    if (mCellMap.isNull())
    {
        return;
    }
    const F32 rim = (F32)mLayout.mRim;
    min.mV[VX] = llmin(min.mV[VX], mCentre.mV[VX] - rim);
    min.mV[VY] = llmin(min.mV[VY], mCentre.mV[VY] - rim);
    min.mV[VZ] = llmin(min.mV[VZ], mLowest - maxDisplacement());
    max.mV[VX] = llmax(max.mV[VX], mCentre.mV[VX] + rim);
    max.mV[VY] = llmax(max.mV[VY], mCentre.mV[VY] + rim);
    max.mV[VZ] = llmax(max.mV[VZ], mHighest + maxDisplacement());
}

void ALWaterSurface::bindWaves(LLGLSLShader& shader, bool drawn, bool patches, const LLViewerTexture* a, const LLViewerTexture* b) const
{
    drawn = drawn && mWavesDrawn;
    // The patch rectangle in the cell map's cells, its max past the end; all 0, empty, without waves.
    const ALWaterField::PatchRect rect = drawn ? ALWaterField::patchRect(mLayout) : ALWaterField::PatchRect();
    const S32 rect_uniform[4] = { rect.mMinX, rect.mMinY, rect.mEndX, rect.mEndY };
    shader.uniform4iv(LLShaderMgr::WATER_PATCH_RECT, 1, rect_uniform);
    if (!patches || !drawn)
    {
        return;
    }

    const WaveSettings settings = waveSettings();
    shader.uniform1f(LLShaderMgr::WATER_TESS_DENSITY, settings.mDensity);
    shader.uniform1f(LLShaderMgr::WATER_MAX_DISPLACEMENT, mMaxDisplacement);
    shader.uniform2f(LLShaderMgr::WATER_WAVE_REACH, (F32)mLayout.mReach, mMaxDisplacement);
    shader.uniform3fv(LLShaderMgr::WATER_WAVE_EYE, 1, mEye.mV);

    // The view's planes but the far one: the patches lie within the reach, which the draw distance does not bound.
    // Read at bind, from whichever camera this view is drawn from.
    LLViewerCamera* camera = LLViewerCamera::getInstance();
    constexpr S32 CULL_PLANES = 5;
    const S32 planes[CULL_PLANES] = { LLCamera::AGENT_PLANE_LEFT, LLCamera::AGENT_PLANE_RIGHT, LLCamera::AGENT_PLANE_NEAR,
                                 LLCamera::AGENT_PLANE_BOTTOM, LLCamera::AGENT_PLANE_TOP };
    F32 cull[CULL_PLANES * 4];
    for (S32 i = 0; i < CULL_PLANES; ++i)
    {
        const LLPlane& plane = camera->getAgentPlane(planes[i]);
        for (S32 c = 0; c < 4; ++c)
        {
            cull[i * 4 + c] = plane[c];
        }
    }
    shader.uniform4fv(LLShaderMgr::WATER_CULL_PLANES, CULL_PLANES, cull);

    // The patches front to back, as nearly as rows of them allow. The water shades only what the depth test lets
    // through, so a crest drawn first spares the troughs it hides behind it.
    const LLVector3 at = camera->getAtAxis();
    const ALWaterField::PatchOrder order = ALWaterField::patchOrder(at.mV[VX], at.mV[VY]);
    shader.uniform4iv(LLShaderMgr::WATER_PATCH_ORDER, 1, order.mOrder);

    // Each map's heights on their own trilinear sampler, whatever RenderWaterMipNormal puts the normal maps on, and
    // its wave height; a map not baked has none, and its sampler reads black rather than whatever the unit held.
    F32 amp[2] = { 0.f, 0.f };
    const U32 units[2] = { LLShaderMgr::WATER_HEIGHT_MAP, LLShaderMgr::WATER_HEIGHT_MAP2 };
    const LLViewerTexture* maps[2] = { a, b };
    for (S32 i = 0; i < 2; ++i)
    {
        const Heights heights = mapHeights(maps[i]);
        const S32 channel = shader.enableTexture(units[i]);
        if (channel < 0)
        {
            continue;
        }
        if (heights.mTexture)
        {
            gGL.getTextureSlot(channel)->bindSampled(heights.mTexture, ALSamplers::TrilinearWrap);
            amp[i] = settings.mHeight * heights.mRise;
        }
        else if (LLViewerTexture::sBlackImagep.notNull() && LLViewerTexture::sBlackImagep->getGLTexture())
        {
            gGL.getTextureSlot(channel)->bindSampled(LLViewerTexture::sBlackImagep->getGLTexture(), ALSamplers::PointWrap);
        }
    }
    shader.uniform2f(LLShaderMgr::WATER_WAVE_AMP, amp[0], amp[1]);
}

S32 ALWaterSurface::patchCount() const
{
    if (!mWavesDrawn)
    {
        return 0;
    }
    const S32 cells = (mLayout.mPatchMaxX - mLayout.mPatchMinX + 1) * (mLayout.mPatchMaxY - mLayout.mPatchMinY + 1);
    return cells * ALWaterField::PATCHES_PER_CELL * ALWaterField::PATCHES_PER_CELL;
}

void ALWaterSurface::drawPatches(LLGLSLShader& shader) const
{
    if (!mWavesDrawn || mCellMap.isNull())
    {
        return;
    }
    gGL.setPatchVertices(4);
    gGL.drawArraysInstanced(LLRender::PATCHES, 0, 4, (U32)patchCount());
    gPipeline.mWaterDraws += 1;
}

void ALWaterSurface::trackMaps(LLViewerTexture* current, LLViewerTexture* next, bool means, bool heights)
{
    mTracked[0] = current ? current->getID() : LLUUID::null;
    mTracked[1] = next ? next->getID() : LLUUID::null;
    for (LLViewerTexture* map : { current, next })
    {
        if (!map)
        {
            continue;
        }
        MapStats& stats = mapStats(map->getID());
        stats.mUsed = gFrameCount;
        if (heights && stats.mTexels && stats.mHeights.isNull())
        {
            uploadHeights(stats);
        }
        const bool wanted = (means && !stats.mKnown) || (heights && !stats.mHeightsBaked);
        if (!wanted || stats.mBaking)
        {
            continue;
        }
        LLViewerFetchedTexture* fetched = LLViewerTextureManager::staticCastToFetchedTexture(map);
        if (!fetched)
        {
            continue;
        }
        // The full-resolution pixels, saved by the fetch when asked for, read where they are. The bake holds its own
        // reference, so the texture's is let go at once rather than kept to the texture list's sweep, unless another
        // user waits on the texture's loads or asked to keep them (LLViewerFetchedTexture::destroySavedRawImage).
        if (fetched->hasSavedRawImage() && fetched->getSavedRawImageLevel() == 0)
        {
            bakeMap(stats, fetched->getSavedRawImage(), heights);
            if (!fetched->hasCallbacks())
            {
                fetched->destroySavedRawImage();
            }
        }
        else
        {
            fetched->forceToSaveRawImage(0);
        }
    }
}

struct ALWaterSurface::Baked
{
    LLVector2                               mMean;
    bool                                    mHeightsWanted = false;
    std::shared_ptr<const std::vector<U16>> mTexels;
    S32                                     mWidth = 0;
    S32                                     mHeight = 0;
    F32                                     mMaxBig = 0.f;
    F32                                     mMaxMid = 0.f;
    F64                                     mRmsBig = 0.0;
    F64                                     mRmsMid = 0.0;
    F64                                     mSeconds = 0.0;
};

void ALWaterSurface::bakeMap(MapStats& stats, LLPointer<LLImageRaw> raw, bool heights)
{
    if (raw.isNull())
    {
        return;
    }
    // The saved pixels are the texture's own copy, never written once saved, and counted thread-safely: the worker
    // reads them under the image's shared lock, and nothing is copied here.
    // Anything thrown on the worker would be thrown again on the main thread as it lands (LL::WorkQueue::postTo), so
    // nothing leaves: ALWaterHeightBake::bake catches its own, and an allocation failing around it lands as a failure.
    const auto bake = [raw, heights]() -> std::shared_ptr<Baked>
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_TEXTURE("water height bake");
        try
        {
            auto baked = std::make_shared<Baked>();
            LLTimer timer;
            LLImageDataSharedLock lock(raw);
            const U8* data = raw->getData();
            const S32 width = raw->getWidth();
            const S32 height = raw->getHeight();
            const S32 components = raw->getComponents();
            baked->mMean = ALWaterField::meanNormal(data, width, height, components);
            baked->mHeightsWanted = heights;
            if (heights)
            {
                // A map past the bake's size is halved until it fits, first.
                std::vector<U8> fitted;
                const U8* pixels = data;
                size_t size = (size_t)raw->getDataSize();
                S32 bake_width = width;
                S32 bake_height = height;
                if ((width > ALWaterHeightBake::MAX_BAKE_SIZE || height > ALWaterHeightBake::MAX_BAKE_SIZE) &&
                    ALWaterHeightBake::fitToSize(data, width, height, components, ALWaterHeightBake::MAX_BAKE_SIZE, fitted,
                                                 bake_width, bake_height))
                {
                    pixels = fitted.data();
                    size = fitted.size();
                }
                const ALWaterHeightBake::Heights field = ALWaterHeightBake::bake(pixels, size, bake_width, bake_height, components);
                if (field.valid())
                {
                    baked->mTexels = std::make_shared<const std::vector<U16>>(ALWaterHeightBake::halfTexels(field));
                    baked->mWidth = field.mWidth;
                    baked->mHeight = field.mHeight;
                    baked->mMaxBig = field.mMaxBig;
                    baked->mMaxMid = field.mMaxMid;
                    const size_t count = (size_t)field.mWidth * (size_t)field.mHeight;
                    F64 square[2] = {};
                    for (size_t n = 0; n < count; ++n)
                    {
                        square[0] += (F64)field.mBig[n] * field.mBig[n];
                        square[1] += (F64)field.mMid[n] * field.mMid[n];
                    }
                    baked->mRmsBig = ALWaterHeightBake::BIG_METRES * std::sqrt(square[0] / (F64)count);
                    baked->mRmsMid = ALWaterHeightBake::MID_METRES * std::sqrt(square[1] / (F64)count);
                }
            }
            baked->mSeconds = timer.getElapsedTimeF64();
            return baked;
        }
        catch (const std::exception& e)
        {
            LL_WARNS("Water") << "A wave height bake failed: " << e.what() << LL_ENDL;
        }
        return nullptr;
    };

    stats.mBaking = true;
    const LLUUID id = stats.mID;
    LL::WorkQueue::ptr_t main_queue = LL::WorkQueue::getInstance("mainloop");
    LL::WorkQueue::ptr_t general_queue = LL::WorkQueue::getInstance("General");
    // Landing looks the map up again: the cache may have let it go meanwhile.
    if (!main_queue || !general_queue ||
        !main_queue->postTo(general_queue, bake, [id](std::shared_ptr<Baked> baked) { gPipeline.mWaterSurface.land(id, baked.get()); }))
    {
        // No queue to take it, as the viewer shuts down: the map is left as it was.
        stats.mBaking = false;
    }
}

void ALWaterSurface::land(const LLUUID& id, const Baked* baked)
{
    MapStats* stats = findMapStats(id);
    if (!stats || !stats->mBaking)
    {
        return;
    }
    stats->mBaking = false;
    stats->mKnown = true;
    if (!baked)
    {
        // Not tried again: the map's mean stays neutral and its waves flat.
        stats->mMean.setZero();
        stats->mHeightsBaked = true;
        return;
    }
    stats->mMean = baked->mMean;
    if (!baked->mHeightsWanted)
    {
        return;
    }
    // Baked or not, the map has given what heights it has; without, its waves stay flat.
    stats->mHeightsBaked = true;
    if (!baked->mTexels)
    {
        return;
    }
    // Uploaded at the next track, on the main thread with the context current.
    stats->mTexels = baked->mTexels;
    stats->mTexelsWidth = baked->mWidth;
    stats->mTexelsHeight = baked->mHeight;
    stats->mHeights = nullptr;
    stats->mMaxBig = baked->mMaxBig;
    stats->mMaxMid = baked->mMaxMid;
    stats->mLanded = LLFrameTimer::getTotalSeconds();
    LL_INFOS("Water") << "Wave heights of " << id << ", " << baked->mWidth << "x" << baked->mHeight << ", baked in "
                      << baked->mSeconds * 1000.0 << " ms: big layer rms " << baked->mRmsBig << " m, max " << baked->mMaxBig
                      << " m; mid layer rms " << baked->mRmsMid << " m, max " << baked->mMaxMid << " m" << LL_ENDL;
}

void ALWaterSurface::uploadHeights(MapStats& stats)
{
    if (!stats.mTexels || stats.mTexelsWidth <= 0 || stats.mTexelsHeight <= 0)
    {
        return;
    }
    // The two fields side by side in a texel, and every mip, box-averaged as GL generates them: a layer seen far off
    // reads its heights averaged toward the plane.
    LLPointer<LLImageGL> texture = new LLImageGL(stats.mTexelsWidth, stats.mTexelsHeight, 2, true);
    texture->setNeedsAlphaAndPickMask(false);
    texture->setExplicitFormat(GL_RG16F, GL_RG, GL_HALF_FLOAT);
    if (!texture->createGLTexture(0, reinterpret_cast<const U8*>(stats.mTexels->data())))
    {
        // Not tried again: the map's waves stay flat.
        LL_WARNS("Water") << "The wave heights of " << stats.mID << " did not upload" << LL_ENDL;
        stats.mTexels.reset();
        return;
    }
    stats.mHeights = texture;
}

ALWaterSurface::Heights ALWaterSurface::mapHeights(const LLViewerTexture* map) const
{
    Heights out;
    const MapStats* stats = map ? findMapStats(map->getID()) : nullptr;
    if (stats && stats->mHeights.notNull())
    {
        out.mTexture = stats->mHeights;
        out.mMaxBig = stats->mMaxBig;
        out.mMaxMid = stats->mMaxMid;
        out.mRise = (F32)llclamp((LLFrameTimer::getTotalSeconds() - stats->mLanded) / RAMP_SECONDS, 0.0, 1.0);
    }
    return out;
}

LLVector4 ALWaterSurface::mapMeans(const LLViewerTexture* a, const LLViewerTexture* b) const
{
    LLVector4 means(0.f, 0.f, 0.f, 0.f);
    const LLViewerTexture* maps[2] = { a, b };
    for (S32 i = 0; i < 2; ++i)
    {
        const MapStats* stats = maps[i] ? findMapStats(maps[i]->getID()) : nullptr;
        if (stats && stats->mKnown)
        {
            means.mV[2 * i] = stats->mMean.mV[VX];
            means.mV[2 * i + 1] = stats->mMean.mV[VY];
        }
    }
    return means;
}

ALWaterSurface::MapStats& ALWaterSurface::mapStats(const LLUUID& id)
{
    for (MapStats& stats : mMaps)
    {
        if (stats.mID == id)
        {
            return stats;
        }
    }
    if (mMaps.size() < MAP_CACHE)
    {
        mMaps.emplace_back();
        mMaps.back().mID = id;
        return mMaps.back();
    }
    // The least recently tracked map makes way, but not one still baking while another can: tracked again before its
    // bake landed, it would be baked a second time and one of the two bakes thrown away.
    MapStats* oldest = nullptr;
    for (MapStats& stats : mMaps)
    {
        if (!oldest || (oldest->mBaking && !stats.mBaking) ||
            (oldest->mBaking == stats.mBaking && stats.mUsed < oldest->mUsed))
        {
            oldest = &stats;
        }
    }
    *oldest = MapStats();
    oldest->mID = id;
    return *oldest;
}

ALWaterSurface::MapStats* ALWaterSurface::findMapStats(const LLUUID& id)
{
    for (MapStats& stats : mMaps)
    {
        if (stats.mID == id)
        {
            return &stats;
        }
    }
    return nullptr;
}

const ALWaterSurface::MapStats* ALWaterSurface::findMapStats(const LLUUID& id) const
{
    for (const MapStats& stats : mMaps)
    {
        if (stats.mID == id)
        {
            return &stats;
        }
    }
    return nullptr;
}
