/**
 * @file llterrainpaintmap.cpp
 * @brief Utilities for managing terrain paint maps
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2024, Linden Research, Inc.
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

#include "llterrainpaintmap.h"

#include "llviewerprecompiledheaders.h"

#if LL_DARWIN
#include "../almetal/almetalterrainpaint.h"
#include "../almetal/almetalresource.h"
#endif

#include "llrender.h"
#include "llsurface.h"
#include "llsurfacepatch.h"
#include "llviewercamera.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexture.h"

// static
bool LLTerrainPaintMap::bakeHeightNoiseIntoPBRPaintMapRGB(
    const LLViewerRegion& region, LLViewerTexture& tex)
{
#if !LL_DARWIN
    LL_WARNS() << "Native terrain paintmap baking requires Metal" << LL_ENDL;
    return false;
#else
    llassert(tex.getComponents() == 3);
    llassert(tex.getWidth() > 0 && tex.getHeight() > 0);
    llassert(tex.getWidth() == tex.getHeight());
    llassert(tex.getPrimaryFormat() == GL_RGB);
    llassert(tex.getGLTexture());

    ALMetalDevice* metal_device = LLImageGL::getMetalDevice();
    LLImageGL* destination_image = tex.getGLTexture();
    ALMetalTexture* destination_template = destination_image
        ? destination_image->getMetalTexture() : nullptr;
    if (!metal_device || !destination_template)
    {
        LL_WARNS("Metal")
            << "Cannot bake terrain paintmap without a native destination"
            << LL_ENDL;
        return false;
    }

    const LLSurface& surface = region.getLand();
    const U32 patch_count = surface.getPatchesPerEdge();

    // *TODO: mHeightsGenerated isn't guaranteed to be true. Assume terrain is
    // loaded for now. Would be nice to fix the loading issue or find a better
    // heuristic to determine that the terrain is sufficiently loaded.
#if 0
    for (U32 rj = 0; rj < patch_count; ++rj)
    {
        for (U32 ri = 0; ri < patch_count; ++ri)
        {
            const LLSurfacePatch* patch = surface.getPatch(ri, rj);
            if (!patch->isHeightsGenerated())
            {
                LL_WARNS() << "Region heightmap not fully loaded" << LL_ENDL;
                return false;
            }
        }
    }
#endif

    // Position the bake camera directly over the region. The direct matrix
    // construction preserves the previous projection without touching shared
    // GL matrix, viewport or target state.
    const F32 region_width = region.getWidth();
    const F32 region_half_width = region_width / 2.0f;
    const F32 region_camera_height = surface.getMaxZ() + DEFAULT_NEAR_PLANE;
    LLViewerCamera camera;
    const LLVector3 region_center =
        LLVector3(region_half_width, region_half_width, 0.0f) +
        region.getOriginAgent();
    const LLVector3 camera_origin =
        LLVector3(0.0f, 0.0f, region_camera_height) + region_center;
    camera.lookAt(camera_origin, region_center, LLVector3::y_axis);
    camera.setAspect(1.f);
    const LLMatrix4a modelview = camera.frameModelview();
    llassert(camera_origin.mV[VZ] >= surface.getMaxZ());
    const F32 region_high_near =
        camera_origin.mV[VZ] - surface.getMaxZ();
    constexpr F32 far_plane_delta = 0.25f;
    const F32 region_low_far =
        camera_origin.mV[VZ] - surface.getMinZ() + far_plane_delta;
    const LLMatrix4a projection = al_ortho(
        -region_half_width, region_half_width,
        -region_half_width, region_half_width,
        region_high_near, region_low_far);
    LLMatrix4a modelview_projection;
    modelview_projection.setMul(modelview, projection);

    // Build one vertex per terrain grid point so the native bake retains the
    // full-resolution composition coordinates used by the original feature.
    const U32 patch_size = (U32)surface.getGridsPerPatchEdge();
    constexpr U32 stride = 1;
    const U32 vert_size = (patch_size / stride) + 1;
    const U32 vertices_per_patch = vert_size * vert_size;
    const U32 indices_per_patch =
        6 * (vert_size - 1) * (vert_size - 1);
    const U32 region_vertices =
        vertices_per_patch * patch_count * patch_count;
    const U32 region_indices =
        indices_per_patch * patch_count * patch_count;
    if (patch_count == 0 || patch_size == 0 ||
        region_vertices == 0 || region_indices == 0)
    {
        LL_WARNS("Metal") << "Terrain paintmap geometry is empty" << LL_ENDL;
        return false;
    }

    std::vector<U32> index_array(region_indices);
    static_assert(sizeof(LLVector4a) == sizeof(F32) * 4);
    static_assert(sizeof(LLVector2) == sizeof(F32) * 2);
    std::vector<LLVector4a> positions(region_vertices);
    std::vector<LLVector2> texcoords(region_vertices);
    U32 vertex_total = 0;
    auto index_iterator = index_array.begin();
    auto position_iterator = positions.begin();
    auto texcoord_iterator = texcoords.begin();
    for (U32 rj = 0; rj < patch_count; ++rj)
    {
        for (U32 ri = 0; ri < patch_count; ++ri)
        {
            const U32 index_offset = vertex_total;
            for (U32 j = 0; j < vert_size - 1; ++j)
            {
                for (U32 i = 0; i < vert_size - 1; ++i)
                {
                    const U32 vert0 = index_offset + i + (j * vert_size);
                    *index_iterator++ = vert0;
                    *index_iterator++ = vert0 + 1;
                    *index_iterator++ = vert0 + vert_size;
                    *index_iterator++ = vert0 + 1;
                    *index_iterator++ = vert0 + vert_size + 1;
                    *index_iterator++ = vert0 + vert_size;
                }
            }

            const LLSurfacePatch* patch = surface.getPatch(ri, rj);
            for (U32 j = 0; j < vert_size; ++j)
            {
                for (U32 i = 0; i < vert_size; ++i)
                {
                    LLVector3 position;
                    LLVector2 composition;
                    patch->eval(i, j, &position, &composition);
                    (*position_iterator++).set(
                        position.mV[VX], position.mV[VY], position.mV[VZ]);
                    *texcoord_iterator++ = composition;
                    ++vertex_total;
                }
            }
        }
    }

    LLPointer<LLViewerTexture> alpha_ramp_texture =
        LLViewerTextureManager::getFetchedTexture(IMG_ALPHA_GRAD_2D);
    ALMetalTexture* alpha_ramp_metal =
        alpha_ramp_texture.notNull() && alpha_ramp_texture->getGLTexture()
            ? alpha_ramp_texture->getGLTexture()->getMetalTexture()
            : nullptr;
    const U32 terrain_bit_depth =
        LLViewerShaderMgr::instance()->inputs().mLocal.mTerrainPaintBitDepth;
    if (!alpha_ramp_metal || terrain_bit_depth < 1 || terrain_bit_depth > 8)
    {
        LL_WARNS("Metal")
            << "Cannot bake terrain paintmap: native alpha ramp or precision "
               "is unavailable"
            << LL_ENDL;
        return false;
    }

    // The production geometry is short-lived and created only on an explicit
    // terrain-editor command, so shared buffers avoid a second staging pass.
    std::string error;
    const std::size_t position_bytes =
        positions.size() * sizeof(LLVector4a);
    const std::size_t texcoord_bytes =
        texcoords.size() * sizeof(LLVector2);
    ALMetalBufferDescriptor vertex_descriptor;
    vertex_descriptor.length = position_bytes + texcoord_bytes;
    vertex_descriptor.storage = ALMetalStorageMode::shared;
    vertex_descriptor.label = "Alchemy terrain paintmap vertices";
    auto metal_vertices = ALMetalBuffer::create(
        *metal_device, vertex_descriptor, &error);
    if (!metal_vertices ||
        !metal_vertices->write(
            0, positions.data(), position_bytes, &error) ||
        !metal_vertices->write(
            position_bytes, texcoords.data(), texcoord_bytes, &error))
    {
        LL_WARNS("Metal") << "Could not upload terrain paint vertices: "
                           << error << LL_ENDL;
        return false;
    }

    ALMetalBufferDescriptor index_descriptor;
    index_descriptor.length = index_array.size() * sizeof(U32);
    index_descriptor.storage = ALMetalStorageMode::shared;
    index_descriptor.label = "Alchemy terrain paintmap indices";
    auto metal_indices = ALMetalBuffer::create(
        *metal_device, index_descriptor, &error);
    if (!metal_indices ||
        !metal_indices->write(
            0, index_array.data(), index_descriptor.length, &error))
    {
        LL_WARNS("Metal") << "Could not upload terrain paint indices: "
                           << error << LL_ENDL;
        return false;
    }

    static ALMetalDevice* renderer_device = nullptr;
    static std::unique_ptr<ALMetalTerrainPaintRenderer> renderer;
    if (renderer_device != metal_device)
    {
        renderer.reset();
        renderer_device = metal_device;
    }
    if (!renderer)
    {
        renderer = ALMetalTerrainPaintRenderer::create(*metal_device, &error);
    }
    if (!renderer)
    {
        LL_WARNS("Metal") << "Could not create terrain paint renderer: "
                           << error << LL_ENDL;
        return false;
    }

    ALMetalTerrainPaintBatch batch;
    batch.vertices = metal_vertices.get();
    batch.position_offset = 0;
    batch.texcoord_offset = position_bytes;
    batch.indices = metal_indices.get();
    batch.index_type = ALMetalIndexType::uint32;
    batch.index_count = region_indices;
    batch.alpha_ramp = alpha_ramp_metal;
    batch.destination_template = destination_template;
    memcpy(batch.modelview_projection, modelview_projection.getF32ptr(),
           sizeof(batch.modelview_projection));
    batch.precision = static_cast<F32>((1u << terrain_bit_depth) - 1u);

    std::unique_ptr<ALMetalTexture> metal_paintmap =
        renderer->render(batch, &error);
    if (!metal_paintmap)
    {
        LL_WARNS("Metal") << "Could not bake terrain paintmap: "
                           << error << LL_ENDL;
        return false;
    }
    if (!destination_image->adoptMetalRenderTextureAuthoritative(
            std::move(metal_paintmap), LLGLTexture::LOCAL))
    {
        LL_WARNS("Metal") << "Could not publish native terrain paintmap"
                           << LL_ENDL;
        return false;
    }

    LL_INFOS_ONCE("Metal")
        << "Production terrain paintmap bakes are native Metal-only"
        << LL_ENDL;
    return true;
#endif
}
