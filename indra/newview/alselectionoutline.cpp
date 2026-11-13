/**
 * @file alselectionoutline.cpp
 * @brief Outlines of selected world objects, drawn on the GPU from an id target in the UI stage.
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

#include "alselectionoutline.h"

#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "llface.h"
#include "llfetchedgltfmaterial.h"
#include "lltextureentry.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewertexture.h"
#include "llvovolume.h"
#include "llvoavatar.h"
#include "pipeline.h"
#include "rlvactions.h"

#if LL_DARWIN
#include "../almetal/almetaluirenderer.h"
#endif

#include <iterator>

namespace
{
    // Whether a texture's alpha can leave anything out, as LLPipeline::getPoolTypeFromTE reads it: media is drawn
    // opaque whatever its format.
    bool texture_has_alpha(const LLViewerTexture* texture)
    {
        const S8 components = texture->getComponents();
        return (components == 4 && texture->getType() != LLViewerTexture::MEDIA_TEXTURE) || components == 2;
    }

    // The highest alpha of what a face samples, 255 where it is not known (LLImageGL::getMaxAlpha).
    U8 texture_max_alpha(const LLViewerTexture* texture)
    {
        const LLImageGL* image = texture->getGLTexture();
        return image ? image->getMaxAlpha() : 255;
    }

    // Which faces a view draws whole (View::mWholeFaces, View::mWholeGlow): Highlight Transparent, which RLVa already
    // gates, draws them so in the scene, and AlchemySelectionOutlineGeometry asks for them where RLVa would allow it.
    void set_whole_faces(ALSelectionOutline::View& view)
    {
        static LLCachedControl<bool> outline_geometry(gSavedSettings, "AlchemySelectionOutlineGeometry", true);
        const bool debug_alpha = LLDrawPoolAlpha::sShowDebugAlpha;
        view.mWholeFaces = debug_alpha || (outline_geometry && RlvActions::canHighlightTransparent());
        view.mWholeGlow = debug_alpha;
    }

    // The contour width setting in force (ALSelectionOutline::contourSetting).
    F32 contour_setting()
    {
        static LLCachedControl<bool> wireframe(gSavedSettings, "AlchemySelectionWireframe", false);
        static LLCachedControl<F32> width(gSavedSettings, "AlchemySelectionOutlineWidth",
                                          ALSelectionOutline::DEFAULT_CONTOUR_WIDTH);
        static LLCachedControl<F32> wireframe_width(gSavedSettings, "AlchemySelectionOutlineWireframeWidth",
                                                    ALSelectionOutline::DEFAULT_WIREFRAME_CONTOUR_WIDTH);
        return ALSelectionOutline::contourSetting(wireframe, width, wireframe_width);
    }

    // The wireframe's width setting (AlchemySelectionWireframeWidth).
    F32 wire_setting()
    {
        static LLCachedControl<F32> wire_width(gSavedSettings, "AlchemySelectionWireframeWidth",
                                               ALSelectionOutline::DEFAULT_WIRE_WIDTH);
        return wire_width;
    }

    // What every view takes from the settings beyond its matrices and widths.
    void set_view_settings(ALSelectionOutline::View& view)
    {
        static LLCachedControl<S32> contrast(gSavedSettings, "AlchemySelectionOutlineContrast",
                                             ALSelectionOutline::CONTRAST_RIM);
        set_whole_faces(view);
        view.mContrast = ALSelectionOutline::contrastMode(contrast);
    }
}

ALSelectionOutline::ALSelectionOutline() = default;

ALSelectionOutline::View ALSelectionOutline::worldView()
{
    View view = makeView(LLViewerCamera::getCurrent().getModelview(), gGL.getProjectionMatrix(), contour_setting(),
                         LLUI::getScaleFactor().mV[VX], false, LLPipeline::RenderHighlightThickness,
                         LLPipeline::RenderHighlightBrightness, wire_setting());
    set_view_settings(view);
    return view;
}

bool ALSelectionOutline::hudView(View& view)
{
    LLMatrix4a projection;
    LLMatrix4a modelview;
    if (!get_hud_matrices(projection, modelview))
    {
        return false;
    }
    view = makeView(modelview, projection, contour_setting(), LLUI::getScaleFactor().mV[VX], true,
                    LLPipeline::RenderHighlightThickness, LLPipeline::RenderHighlightBrightness, wire_setting());
    set_view_settings(view);
    return true;
}

void ALSelectionOutline::add(LLViewerObject* object, U32 te_mask, const LLColor4& colour, bool show_hidden,
                             EPriority priority, U32 parts)
{
    if (!object)
    {
        return;
    }

    // What glows is drawn with nothing else, and only where the scene shows it.
    const Entry entry = { object, te_mask, colour, show_hidden && priority != PRIORITY_HOVER, priority,
                          partsFor(priority, parts) };

    const auto found = mEntryIndex.find(object);
    if (found != mEntryIndex.end())
    {
        Entry& added = mEntries[found->second];
        if (replaces(added.mPriority, priority))
        {
            added = entry;
        }
        return;
    }

    if (mEntries.size() >= MAX_IDS)
    {
        return;
    }
    mEntryIndex.emplace(object, (U32)mEntries.size());
    mEntries.push_back(entry);
}

void ALSelectionOutline::clearEntries()
{
    mEntries.clear();
    mEntryIndex.clear();
}

void ALSelectionOutline::release()
{
    clearEntries();
}

void ALSelectionOutline::render(const View& view)
{
    if (mEntries.empty())
    {
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    // The window's viewport: the final blit filled it from the scene, the HUD is drawn over it, and the outlines
    // are laid over both.
    const S32 width = gGLViewport[2];
    const S32 height = gGLViewport[3];

    Wants wants;
    for (const Entry& entry : mEntries)
    {
        wants.mContour = wants.mContour || (entry.mParts & PART_CONTOUR);
        wants.mWireframe = wants.mWireframe || (entry.mParts & PART_WIREFRAME);
        wants.mGlow = wants.mGlow || entry.mPriority == PRIORITY_HOVER;
    }
    const Passes passes = passesFor(wants, view.mHUD, true);
    if (!passes.any() || width <= 0 || height <= 0 || !gPipeline.mRT)
    {
        clearEntries();
        return;
    }

    LLMatrix4a mvp;
    mvp.setMul(view.mModelview, view.mProjection);

    // Ids in priority order: id i + 1 is mEntries[mOrder[i]].
    const U32 count = (U32)mEntries.size();
    mPriorities.resize(count);
    for (U32 i = 0; i < count; ++i)
    {
        mPriorities[i] = mEntries[i].mPriority;
    }
    priorityOrder(mPriorities, mOrder);

    // What this frame draws on screen, and where. The HUD's wireframe tests the id pass's depth, so it needs its
    // faces there; the world's needs nothing of it.
    collectFaces(view, width, height, mvp);
    const bool ids = passes.mIds && !mRects.empty();
    const bool edges = passes.mEdges && ids;
    const bool contour = edges && wants.mContour;
    const bool wireframe = passes.mWireframe && !mLineRects.empty() && (!view.mHUD || ids);
    const bool glow = edges && wants.mGlow;
    const bool backdrop = readsBackdrop(view, contour, wireframe);
    if (!ids && !wireframe)
    {
        clearEntries();
        return;
    }

    LL_PROFILE_GPU_ZONE("selection outline");

    const U32 rows = paletteRows(count);
    mPaletteTexels.assign((size_t)PALETTE_WIDTH * rows * 4, 0);
    for (U32 i = 0; i < count; ++i)
    {
        const Entry& entry = mEntries[mOrder[i]];
        writePalette(mPaletteTexels, i + 1, entry.mColour, entry.mShowHidden);
    }

#if LL_DARWIN
    const bool native_edges      = edges;
    F32        native_wire_slope = 0.f;
    if (wireframe)
    {
        F32 resample = 1.f;
        if (!view.mHUD && gPipeline.mRT->deferredScreen.getWidth() > 0)
        {
            resample = (F32)width / (F32)gPipeline.mRT->deferredScreen.getWidth();
        }
        native_wire_slope = wireOffsetFactor(resample);
    }
    if ((ids || wireframe) && gGL.hasMetalUITextureCapture() &&
        !drawMetalIds(view, width, height, native_edges && contour, native_edges && glow, wireframe,
                      view.mContrast == CONTRAST_SHIFT && ((native_edges && contour) || wireframe), native_wire_slope))
    {
        gGL.invalidateMetalUITextureCapture("an incomplete native selection edge transaction");
    }
#endif

    clearEntries();
}

void ALSelectionOutline::collectFaces(const View& view, S32 width, S32 height, const LLMatrix4a& mvp)
{
    mFaces.clear();
    mRects.clear();
    mLineRects.clear();

    const S32 contour_reach = contourReach(view);
    const S32 glow_reach = lineReach(view.mGlowRadius);
    const S32 wire_reach = lineReach(view.mWireWidth);
    const LLRect target(0, height, width, 0);

    for (U32 i = 0; i < (U32)mEntries.size(); ++i)
    {
        const Entry& entry = mEntries[mOrder[i]];
        const bool in_ids = inIdPass(entry.mParts, entry.mPriority, view.mHUD);
        const bool wire = (entry.mParts & PART_WIREFRAME) != 0;
        if (!in_ids && !wire)
        {
            continue;
        }

        LLViewerObject* object = entry.mObject;
        LLDrawable* drawable = object->mDrawable;
        if (!drawable || drawable->isDead() || !drawable->getVOVolume())
        {
            continue;
        }

        // The frame each face's vertex buffer is in, as LLVolumeGeometryManager::registerFace places it. Rigged
        // faces are skinned into agent space and take none.
        const LLMatrix4a* model = nullptr;
        bool agent_extents = false;
        if (drawable->isState(LLDrawable::ANIMATED_CHILD))
        {
            model = &drawable->getWorldMatrix();
        }
        else if (drawable->isActive())
        {
            model = &drawable->getRenderMatrix();
        }
        else if (drawable->getRegion())
        {
            model = &drawable->getRegion()->mRenderMatrix;
            // A static face's box already has the region's origin in it (LLFace::genVolumeBBoxes).
            agent_extents = true;
        }

        FaceBox box;
        const size_t first = mFaces.size();
        // The TE mask holds 32 faces, more than a volume has.
        const S32 faces = llmin(llmin((S32)object->getNumTEs(), drawable->getNumFaces()), 32);
        for (S32 te = 0; te < faces; ++te)
        {
            if (!(entry.mTEMask & (1u << te)))
            {
                continue;
            }

            LLFace* face = drawable->getFace(te);
            if (!face)
            {
                continue;
            }
            // The HUD's matrices have no place for a rigged face, posed about its avatar in the world.
            const bool rigged = face->isState(LLFace::RIGGED);
            if ((rigged && view.mHUD) || (!rigged && !model))
            {
                continue;
            }
            LLVertexBuffer* buffer = face->getVertexBuffer();
            if (!buffer || face->getGeomCount() == 0 || face->getIndicesCount() == 0)
            {
                continue;
            }

            LLVector4a extents[2];
            if (rigged)
            {
                if (!face->mAvatar || !face->mSkinInfo)
                {
                    continue;
                }
                // Joint boxes, refreshed while the face is in view (LLFace::calcPixelArea); empty until they are.
                extents[0] = face->mRiggedExtents[0];
                extents[1] = face->mRiggedExtents[1];
                LLVector4a size;
                size.setSub(extents[1], extents[0]);
                if (!(size[0] > 0.f && size[1] > 0.f && size[2] > 0.f))
                {
                    box.mBounded = false;
                }
            }
            else if (agent_extents)
            {
                extents[0] = face->mExtents[0];
                extents[1] = face->mExtents[1];
            }
            else
            {
                matMulBoundBox(*model, face->mExtents, extents);
            }
            box.mLo.setMin(box.mLo, extents[0]);
            box.mHi.setMax(box.mHi, extents[1]);

            mFaces.push_back({ face, rigged ? nullptr : model, i, entry.mPriority, rigged });
        }
        if (mFaces.size() == first)
        {
            continue;
        }

        // What reaches past the faces: the glow, the contour, or the wireframe's lines alone.
        const S32 reach = entry.mPriority == PRIORITY_HOVER ? glow_reach
                          : (entry.mParts & PART_CONTOUR)   ? llmax(contour_reach, wire_reach)
                                                            : wire_reach;
        LLRect rect = target;
        if (box.mBounded)
        {
            const LLVector4a extents[2] = { box.mLo, box.mHi };
            if (!scissorRect(mvp, extents, width, height, reach, rect))
            {
                // Nothing of it can reach the window.
                mFaces.resize(first);
                continue;
            }
        }
        if (in_ids)
        {
            mRects.push_back(rect);
        }
        if (entry.mPriority != PRIORITY_HOVER)
        {
            mLineRects.push_back(rect);
        }
    }

    mergeRects(mRects);
    mergeRects(mLineRects);

    // The rigged apart, as they take another program; the lowest priority first, so that where surfaces the scene
    // does not show lie at the same depth, the highest priority's is drawn last and keeps the texel, and its lines
    // are laid over the others'; then by what the draws share.
    std::sort(mFaces.begin(), mFaces.end(), [](const FaceDraw& a, const FaceDraw& b)
    {
        if (a.mRigged != b.mRigged)
        {
            return !a.mRigged;
        }
        if (a.mPriority != b.mPriority)
        {
            return a.mPriority > b.mPriority;
        }
        if (a.mRigged)
        {
            if (a.mFace->mAvatar != b.mFace->mAvatar)
            {
                return std::less<const LLVOAvatar*>()(a.mFace->mAvatar, b.mFace->mAvatar);
            }
            if (a.mFace->mSkinInfo != b.mFace->mSkinInfo)
            {
                return std::less<const LLMeshSkinInfo*>()(a.mFace->mSkinInfo, b.mFace->mSkinInfo);
            }
        }
        else if (a.mModel != b.mModel)
        {
            return std::less<const LLMatrix4a*>()(a.mModel, b.mModel);
        }
        const LLVertexBuffer* a_buffer = a.mFace->getVertexBuffer();
        const LLVertexBuffer* b_buffer = b.mFace->getVertexBuffer();
        if (a_buffer != b_buffer)
        {
            return std::less<const LLVertexBuffer*>()(a_buffer, b_buffer);
        }
        if (a.mIndex != b.mIndex)
        {
            return a.mIndex < b.mIndex;
        }
        return a.mFace->getGeomIndex() < b.mFace->getGeomIndex();
    });
}

#if LL_DARWIN
bool ALSelectionOutline::drawMetalIds(const View& view, S32 width, S32 height, bool contour_enabled, bool glow_enabled,
                                      bool wireframe_enabled, bool contrast_shift, F32 wire_depth_slope)
{
    LLMatrix4a inverse_projection;
    if (!inverse_projection.setInverse(view.mProjection))
    {
        return false;
    }

    std::vector<ALMetalSelectionIDBatch> batches;
    std::vector<ALMetalSelectionIDBatch> wire_batches;
    batches.reserve(mFaces.size());
    wire_batches.reserve(mFaces.size());
    for (const FaceDraw& draw : mFaces)
    {
        const Entry& entry   = mEntries[mOrder[draw.mIndex]];
        const bool   in_ids  = inIdPass(entry.mParts, entry.mPriority, view.mHUD);
        const bool   in_wire = wireframe_enabled && inWirePass(entry.mParts, entry.mShowHidden, WIRE_VISIBLE);
        if (!in_ids && !in_wire)
        {
            continue;
        }

        LLFace*         face   = draw.mFace;
        LLVertexBuffer* buffer = face ? face->getVertexBuffer() : nullptr;
        if (!buffer || !buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) || !buffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0) ||
            !buffer->prepareMetalBuffers(true))
        {
            return false;
        }

        ALMetalSelectionIDBatch batch;
        batch.vertices        = buffer->getMetalVertexBuffer();
        batch.position_offset = buffer->getOffset(LLVertexBuffer::TYPE_VERTEX);
        batch.texcoord_offset = buffer->getOffset(LLVertexBuffer::TYPE_TEXCOORD0);
        batch.vertex_count    = buffer->getNumVerts();
        batch.indices         = buffer->getMetalIndexBuffer();
        batch.index_offset    = static_cast<std::size_t>(face->getIndicesStart()) * buffer->getIndexStride();
        batch.index_type      = buffer->getIndexStride() == 4 ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
        batch.index_count     = face->getIndicesCount();
        batch.id              = draw.mIndex + 1;
        batch.priority        = entry.mPriority;
        batch.show_hidden     = entry.mShowHidden;
        batch.outlined        = outlined(entry.mParts, entry.mPriority);

        LLMatrix4a modelview = view.mModelview;
        if (draw.mModel)
        {
            modelview.setMul(*draw.mModel, modelview);
        }
        LLMatrix4a modelview_projection;
        modelview_projection.setMul(modelview, view.mProjection);
        std::memcpy(batch.modelview_projection, modelview_projection.getF32ptr(), sizeof(batch.modelview_projection));

        const LLTextureEntry*                  te      = face->getTextureEntry();
        LLViewerTexture*                       texture = face->getTexture();
        F32                                    cutoff  = NO_ALPHA_TEST;
        LLUUID                                 texture_id;
        U8                                     max_alpha = 255;
        LLGLTFMaterial::TextureTransform::Pack transform;
        std::copy(std::begin(IDENTITY_TRANSFORM), std::end(IDENTITY_TRANSFORM), std::begin(transform));
        auto* gltf = te ? static_cast<LLFetchedGLTFMaterial*>(te->getGLTFRenderMaterial()) : nullptr;
        if (gltf)
        {
            batch.double_sided = gltf->mDoubleSided;
            if (!face->hasMedia() || !texture || texture->getType() != LLViewerTexture::MEDIA_TEXTURE)
            {
                texture = gltf->getRenderTexture(LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR);
            }
            if (texture && texture_has_alpha(texture))
            {
                cutoff     = gltfAlphaCutoff(gltf->mAlphaMode, gltf->mAlphaCutoff);
                texture_id = texture->getID();
                max_alpha  = texture_max_alpha(texture);
            }
            gltf->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR].getPacked(transform);
        }
        else if (te && texture)
        {
            const LLMaterial* material = te->getMaterialParams().get();
            cutoff                     = legacyAlphaCutoff(material != nullptr,
                                       material ? material->getDiffuseAlphaMode() : 0,
                                       material ? material->getAlphaMaskCutoff() : 0,
                                                           texture_has_alpha(texture));
            texture_id                 = te->getID();
            max_alpha                  = texture_max_alpha(texture);
        }

        const AlphaTest test =
            faceAlphaTest(texture_id, max_alpha, cutoff, wholeFaces(view, draw.mPriority), LLVOVolume::isInvisibleFace(te));
        batch.alpha_cutoff = test.mCutoff;
        batch.keep_cut     = test.mKeepCut;
        std::copy(std::begin(transform), std::end(transform), std::begin(batch.base_color_transform));
        if (test.mCutoff > 0.f && test.mCutoff <= 1.f)
        {
            LLImageGL* image = texture ? texture->getGLTexture() : nullptr;
            batch.texture    = image ? image->getMetalTexture() : nullptr;
            if (!batch.texture)
            {
                return false;
            }
        }
        if (face->mTextureMatrix && face->isState(LLFace::TEXTURE_ANIM) && face->getVirtualSize() > MIN_TEX_ANIM_SIZE)
        {
            std::memcpy(batch.texture_matrix, face->mTextureMatrix->mMatrix, sizeof(batch.texture_matrix));
        }

        if (draw.mRigged)
        {
            if (!face->mAvatar || !face->mSkinInfo || !buffer->hasDataType(LLVertexBuffer::TYPE_WEIGHT4))
            {
                return false;
            }
            const LLVOAvatar::MatrixPaletteCache& palette     = face->mAvatar->updateSkinInfoMatrixPalette(face->mSkinInfo);
            const U32                             joint_count = static_cast<U32>(palette.mMatrixPalette.size());
            if (joint_count == 0 || palette.mGLMp.size() != 4u + static_cast<std::size_t>(joint_count) * 12u)
            {
                return false;
            }
            batch.weight_offset            = buffer->getOffset(LLVertexBuffer::TYPE_WEIGHT4);
            batch.skin_palette             = palette.mGLMp.data();
            batch.skin_palette_float_count = palette.mGLMp.size();
            batch.skin_joint_count         = joint_count;
        }
        if (in_ids)
        {
            batches.push_back(batch);
        }
        if (in_wire)
        {
            wire_batches.push_back(batch);
        }
    }

    // A world selection made solely of wireframes has no legacy id pass, but
    // the native transaction still needs a private depth target for a uniform
    // ordered lifetime. Its color is never consumed and these faces are not
    // allowed to occlude a simultaneous contour id pass.
    if (batches.empty() && !wire_batches.empty())
    {
        batches = wire_batches;
        for (ALMetalSelectionIDBatch& batch : batches)
        {
            batch.outlined = false;
        }
    }
    if (batches.empty())
    {
        return false;
    }
    ALMetalSelectionIDScene scene;
    scene.batches     = batches.data();
    scene.batch_count = batches.size();
    std::memcpy(scene.inverse_projection, inverse_projection.getF32ptr(), sizeof(scene.inverse_projection));
    scene.width           = static_cast<U32>(width);
    scene.height          = static_cast<U32>(height);
    scene.reverse_z       = LLRender::sReverseZ;
    scene.use_scene_depth = !view.mHUD;
    if (!gGL.drawMetalSelectionIDs(scene) || (!contour_enabled && !glow_enabled && wire_batches.empty()))
    {
        return false;
    }

    ALMetalSelectionContourScene contour;
    contour.palette_rgba8  = mPaletteTexels.data();
    contour.palette_bytes  = mPaletteTexels.size();
    contour.palette_width  = PALETTE_WIDTH;
    contour.palette_height = paletteRows(static_cast<U32>(mEntries.size()));
    std::memcpy(contour.inverse_projection, inverse_projection.getF32ptr(), sizeof(contour.inverse_projection));
    contour.width            = static_cast<U32>(width);
    contour.height           = static_cast<U32>(height);
    contour.outline_width    = static_cast<U32>(view.mWidth);
    contour.inner_width      = static_cast<U32>(view.mInnerWidth);
    contour.rim_width        = view.mContrast == CONTRAST_RIM ? static_cast<U32>(view.mRimWidth) : 0u;
    contour.glow_radius      = glow_enabled ? static_cast<U32>(view.mGlowRadius) : 0u;
    contour.glow_brightness  = glow_enabled ? view.mGlowBrightness : 0.f;
    contour.wire_batches     = wire_batches.data();
    contour.wire_batch_count = wire_batches.size();
    contour.wire_width       = wireframe_enabled ? static_cast<U32>(view.mWireWidth) : 0u;
    contour.wire_depth_slope = wire_depth_slope;
    contour.wire_depth_units = WIRE_OFFSET_UNITS;
    contour.wire_hud         = view.mHUD;
    contour.contour_enabled  = contour_enabled;
    contour.contrast_shift   = contrast_shift;
    contour.reverse_z        = LLRender::sReverseZ;
    return gGL.drawMetalSelectionContour(contour);
}
#endif
