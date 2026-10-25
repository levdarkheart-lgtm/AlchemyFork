/**
 * @file lltoolmorph.cpp
 * @brief A tool to manipulate faces..
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

// File includes
#include "lltoolmorph.h"
#include "llrender.h"

// Library includes
#include "llaudioengine.h"
#include "llviewercontrol.h"
#include "llfontgl.h"
#include "llwearable.h"
#include "sound_ids.h"
#include "v3math.h"
#include "v3color.h"

// Viewer includes
#include "llagent.h"
#include "lldrawable.h"
#include "lldrawpool.h"
#include "lldrawpoolavatar.h"
#include "llface.h"
#include "llmorphview.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "lltexlayer.h"
#include "lltoolmgr.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"
#include "llviewerjointmesh.h"
#include "llviewerobject.h"
#include "llviewerwearable.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "noise.h"
#include "pipeline.h"

#if LL_DARWIN
#include "../almetal/almetalsculptpreview.h"
#endif

#include <array>
#include <vector>

//static
LLVisualParamHint::instance_list_t LLVisualParamHint::sInstances;
bool LLVisualParamReset::sDirty = false;

//-----------------------------------------------------------------------------
// LLVisualParamHint()
//-----------------------------------------------------------------------------

// static
LLVisualParamHint::LLVisualParamHint(
    S32 pos_x, S32 pos_y,
    S32 width, S32 height,
    LLViewerJointMesh *mesh,
    LLViewerVisualParam *param,
    LLWearable *wearable,
    F32 param_weight,
    LLJoint* jointp)
    :
    LLViewerDynamicTexture(width, height, 3, LLViewerDynamicTexture::ORDER_MIDDLE),
    mNeedsUpdate( true ),
    mIsVisible( false ),
    mJointMesh( mesh ),
    mVisualParam( param ),
    mWearablePtr( wearable ),
    mVisualParamWeight( param_weight ),
    mAllowsUpdates( true ),
    mDelayFrames( 0 ),
    mRect( pos_x, pos_y + height, pos_x + width, pos_y ),
    mLastParamWeight(0.f),
    mCamTargetJoint(jointp)
{
    LLVisualParamHint::sInstances.insert( this );
    mBackgroundp = LLUI::getUIImage("avatar_thumb_bkgrnd.png");
    if (mBackgroundp)
    {
        LLViewerFetchedTexture* tex = dynamic_cast<LLViewerFetchedTexture*>(mBackgroundp->getImage().get());
        if (tex)
        {
            tex->setKnownDrawSize(width, height);
        }

        mBackgroundLoadedConnection = mBackgroundp->addLoadedCallback([this]()
        {
            mNeedsUpdate = true;
            mDelayFrames = 0;
        });
    }

    llassert(width != 0);
    llassert(height != 0);
}

//-----------------------------------------------------------------------------
// ~LLVisualParamHint()
//-----------------------------------------------------------------------------
LLVisualParamHint::~LLVisualParamHint()
{
    LLVisualParamHint::sInstances.erase( this );
    mBackgroundLoadedConnection.disconnect();
}

//virtual
S8 LLVisualParamHint::getType() const
{
    return LLViewerDynamicTexture::LL_VISUAL_PARAM_HINT ;
}

//-----------------------------------------------------------------------------
// static
// requestHintUpdates()
// Requests updates for all instances (excluding two possible exceptions)  Grungy but efficient.
//-----------------------------------------------------------------------------
void LLVisualParamHint::requestHintUpdates( LLVisualParamHint* exception1, LLVisualParamHint* exception2 )
{
    S32 delay_frames = 0;
    for (instance_list_t::iterator iter = sInstances.begin();
         iter != sInstances.end(); ++iter)
    {
        LLVisualParamHint* instance = *iter;
        if( (instance != exception1) && (instance != exception2) )
        {
            if( instance->mAllowsUpdates )
            {
                instance->mNeedsUpdate = true;
                instance->mDelayFrames = delay_frames;
                delay_frames++;
            }
            else
            {
                instance->mNeedsUpdate = true;
                instance->mDelayFrames = 0;
            }
        }
    }
}

bool LLVisualParamHint::needsRender()
{
    // needsRender gets called once per frame from
    // LLViewerDynamicTexture::updateAllInstances()
    return mNeedsUpdate && mDelayFrames-- <= 0 && !gAgentAvatarp->getIsAppearanceAnimating() && mAllowsUpdates;
}

void LLVisualParamHint::preRender(bool clear_depth)
{
    LL_DEBUGS("ParamHint") << "preRender: param=" << mVisualParam->getName()
        << " id=" << mVisualParam->getID()
        << " weight=" << mVisualParamWeight
        << " dirtyMesh=" << gAgentAvatarp->mDirtyMesh
        << " pixelArea=" << gAgentAvatarp->mAdjustedPixelArea
        << " appearanceAnimating=" << gAgentAvatarp->getIsAppearanceAnimating()
        << LL_ENDL;
    LLViewerWearable* wearable = dynamic_cast<LLViewerWearable*>(mWearablePtr);
    if (wearable)
    {
        wearable->setVolatile(true);
    }
    mLastParamWeight = mVisualParam->getWeight();
    mWearablePtr->setVisualParamWeight(mVisualParam->getID(), mVisualParamWeight);
    gAgentAvatarp->setVisualParamWeight(mVisualParam->getID(), mVisualParamWeight);
    gAgentAvatarp->setVisualParamWeight("Blink_Left", 0.f);
    gAgentAvatarp->setVisualParamWeight("Blink_Right", 0.f);
    gAgentAvatarp->updateComposites();
    // Calling LLCharacter version, as we don't want position/height changes to cause the avatar to jump
    // up and down when we're doing preview renders. -Nyx
    gAgentAvatarp->LLCharacter::updateVisualParams();

    if (gAgentAvatarp->mDrawable.notNull())
    {
        gAgentAvatarp->updateGeometry(gAgentAvatarp->mDrawable);
        gAgentAvatarp->updateLOD();

        if (gAgentAvatarp->mDrawable->isState(LLDrawable::REBUILD_GEOMETRY)
            || gAgentAvatarp->mDirtyMesh > 0)
        {
            // updateLOD marks meshes for an update.
            LL_DEBUGS("ParamHint") << "preRender: updateMeshData"
                << " param=" << mVisualParam->getName()
                << " dirtyMesh=" << gAgentAvatarp->mDirtyMesh
                << " rebuildGeom=" << gAgentAvatarp->mDrawable->isState(LLDrawable::REBUILD_GEOMETRY)
                << LL_ENDL;
            // Clear states and force update so that renderSkinned inside generateImpostor
            // does not re-run updateMeshData() with a partially consistent mesh state,
            // which would reassign mFace pointers on some meshes while others are
            // mid-draw with stale vertex offsets.
            gAgentAvatarp->updateMeshData();
        }
    }
    else
    {
        LL_WARNS() << "Attempting to update avatar's geometry, but drawable doesn't exist yet" << LL_ENDL;
    }

    LLViewerDynamicTexture::preRender(clear_depth);
}

//-----------------------------------------------------------------------------
// render()
//-----------------------------------------------------------------------------
bool LLVisualParamHint::render()
{
    LLVisualParamReset::sDirty = true;
#if LL_DARWIN
    mMetalPreview.reset();
    mMetalPreviewDescriptor.reset();
    if (mGLTexturep.notNull() && mGLTexturep->getMetalTexture())
    {
        mMetalPreviewDescriptor = std::make_unique<ALMetalTextureDescriptor>(
            mGLTexturep->getMetalTexture()->descriptor());
    }
#endif

    gGL.pushUIMatrix();
    gGL.loadUIIdentity();

    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.pushMatrix();
    gGL.loadIdentity();
    gGL.ortho(0.0f, (F32)mFullWidth, 0.0f, (F32)mFullHeight, -1.0f, 1.0f);

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.pushMatrix();
    gGL.loadIdentity();

    gUIProgram.bind();

    LLGLSUIDefault gls_ui;
    //LLGLState::verify(true);
    mBackgroundp->draw(0, 0, mFullWidth, mFullHeight);

    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.popMatrix();

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.popMatrix();

    mNeedsUpdate = false;
    mIsVisible = true;

    LLQuaternion avatar_rotation;
    LLJoint* root_joint = gAgentAvatarp->getRootJoint();
    if( root_joint )
    {
        avatar_rotation = root_joint->getWorldRotation();
    }

    LLVector3 target_joint_pos = mCamTargetJoint->getWorldPosition();

    LLVector3 target_offset( 0, 0, mVisualParam->getCameraElevation() );
    LLVector3 target_pos = target_joint_pos + (target_offset * avatar_rotation);

    F32 cam_angle_radians = mVisualParam->getCameraAngle() * DEG_TO_RAD;

    static LLCachedControl<bool> auto_camera_position(gSavedSettings, "AppearanceCameraMovement");
    if (!auto_camera_position)
    {
        cam_angle_radians += F_PI;
    }

    LLVector3 camera_snapshot_offset(
        mVisualParam->getCameraDistance() * cosf( cam_angle_radians ),
        mVisualParam->getCameraDistance() * sinf( cam_angle_radians ),
        mVisualParam->getCameraElevation() );
    LLVector3 camera_pos = target_joint_pos + (camera_snapshot_offset * avatar_rotation);

    gGL.flush();

    LLViewerCamera::getInstance()->setAspect((F32)mFullWidth / (F32)mFullHeight);
    LLViewerCamera::getInstance()->lookAt(
        camera_pos,         // camera
        target_pos,         // point of interest
        LLVector3::z_axis); // up

    LLViewerCamera::getInstance()->setPerspective(false, mOrigin.mX, mOrigin.mY, mFullWidth, mFullHeight, false);

    llassert(!LLPipeline::sImpostorRender); // We are rendering this from UI, shouldn't be set
    if (gAgentAvatarp->mDrawable.notNull() && !LLPipeline::sImpostorRender)
    {
        LL_DEBUGS("ParamHint") << "render: generating impostor"
            << " param=" << mVisualParam->getName()
            << " drawableState=0x" << std::hex << gAgentAvatarp->mDrawable->getState() << std::dec
            << " dirtyMesh=" << gAgentAvatarp->mDirtyMesh
            << " mMeshValid=" << gAgentAvatarp->mMeshValid
            << LL_ENDL;
        LLGLDepthTest gls_depth(GL_TRUE, GL_TRUE);
        gGL.flush();
        gGL.setSceneBlendType(LLRender::BT_REPLACE);
        gPipeline.generateImpostor(gAgentAvatarp, true);
        gGL.setSceneBlendType(LLRender::BT_ALPHA);
        gGL.flush();

#if LL_DARWIN
        ALMetalDevice* metal_device = LLImageGL::getMetalDevice();
        LLImageGL* background_image = mBackgroundp && mBackgroundp->getImage()
            ? mBackgroundp->getImage()->getGLTexture() : nullptr;
        const ALMetalTexture* background_texture = background_image
            ? background_image->getMetalTexture() : nullptr;
        if (metal_device && mMetalPreviewDescriptor && background_texture)
        {
            static ALMetalDevice* renderer_device = nullptr;
            static std::unique_ptr<ALMetalLitPreviewRenderer> renderer;
            std::string error;
            if (renderer_device != metal_device)
            {
                renderer.reset();
                renderer_device = metal_device;
            }
            if (!renderer)
            {
                renderer = ALMetalLitPreviewRenderer::create(
                    *metal_device, &error);
            }

            bool complete = renderer != nullptr;
            std::vector<LLFace*> rigged_faces;
            auto collect_rigged_faces = [&rigged_faces](
                LLViewerObject* object)
            {
                if (!object || !object->isRiggedMesh())
                {
                    return true;
                }
                LLDrawable* drawable = object->mDrawable.get();
                if (!drawable)
                {
                    return false;
                }
                bool found = false;
                for (S32 face_index = 0;
                     face_index < drawable->getNumFaces(); ++face_index)
                {
                    LLFace* face = drawable->getFace(face_index);
                    if (!face || !face->isState(LLFace::RIGGED))
                    {
                        continue;
                    }
                    found = true;
                    if (!face->mDrawInfo || face->mAvatar != gAgentAvatarp ||
                        !face->mSkinInfo)
                    {
                        return false;
                    }
                    rigged_faces.push_back(face);
                }
                return found;
            };
            for (const auto& attachment_entry :
                 gAgentAvatarp->mAttachmentPoints)
            {
                LLViewerJointAttachment* attachment = attachment_entry.second;
                if (!attachment)
                {
                    continue;
                }
                for (const auto& attached : attachment->mAttachedObjects)
                {
                    LLViewerObject* object = attached.get();
                    complete = complete && collect_rigged_faces(object);
                    if (object)
                    {
                        for (LLViewerObject* child : object->getChildren())
                        {
                            complete = complete &&
                                collect_rigged_faces(child);
                        }
                    }
                }
            }

            using namespace LLAvatarAppearanceDefines;
            std::vector<EMeshIndex> required_meshes;
            required_meshes.reserve(8);
            const bool render_avatar = gAgent.needsRenderAvatar();
            const bool render_head = gAgent.needsRenderHead();
            const bool jelly =
                gAgentAvatarp->getOverallAppearance() ==
                    LLVOAvatar::AOA_JELLYDOLL &&
                !gAgentAvatarp->isControlAvatar();
            if (render_avatar && render_head &&
                (gAgentAvatarp->isTextureVisible(TEX_EYES_BAKED) || jelly ||
                 gAgentAvatarp->isUIAvatar()))
            {
                required_meshes.push_back(MESH_ID_EYEBALL_LEFT);
                required_meshes.push_back(MESH_ID_EYEBALL_RIGHT);
            }
            if (render_avatar &&
                (gAgentAvatarp->isTextureVisible(TEX_HEAD_BAKED) || jelly ||
                 gAgentAvatarp->isUIAvatar()))
            {
                required_meshes.push_back(MESH_ID_HEAD);
            }
            if (render_avatar &&
                (gAgentAvatarp->isTextureVisible(TEX_UPPER_BAKED) || jelly ||
                 gAgentAvatarp->isUIAvatar()))
            {
                required_meshes.push_back(MESH_ID_UPPER_BODY);
            }
            if (render_avatar &&
                (gAgentAvatarp->isTextureVisible(TEX_LOWER_BAKED) || jelly ||
                 gAgentAvatarp->isUIAvatar()))
            {
                required_meshes.push_back(MESH_ID_LOWER_BODY);
            }
            if (render_avatar && gAgentAvatarp->isWearingWearableType(
                    LLWearableType::WT_SKIRT) &&
                (gAgentAvatarp->isUIAvatar() ||
                 gAgentAvatarp->isTextureVisible(TEX_SKIRT_BAKED)))
            {
                required_meshes.push_back(MESH_ID_SKIRT);
            }
            if (render_avatar &&
                gAgentAvatarp->isTextureVisible(TEX_HEAD_BAKED))
            {
                required_meshes.push_back(MESH_ID_EYELASH);
            }
            if (render_avatar &&
                gAgentAvatarp->isTextureVisible(TEX_HAIR_BAKED) && !jelly)
            {
                required_meshes.push_back(MESH_ID_HAIR);
            }

            std::vector<LLViewerJointMesh*> preview_meshes;
            preview_meshes.reserve(required_meshes.size());
            for (EMeshIndex mesh_index : required_meshes)
            {
                auto* joint = dynamic_cast<LLAvatarJoint*>(
                    gAgentAvatarp->getViewerJoint(mesh_index));
                std::size_t active_parts = 0;
                if (joint)
                {
                    for (LLAvatarJointMesh* part : joint->mMeshParts)
                    {
                        auto* viewer_mesh =
                            dynamic_cast<LLViewerJointMesh*>(part);
                        LLFace* preview_face = nullptr;
                        LLPolyMesh* preview_mesh = nullptr;
                        if (viewer_mesh && viewer_mesh->getPreviewGeometry(
                                               preview_face, preview_mesh))
                        {
                            preview_meshes.push_back(viewer_mesh);
                            ++active_parts;
                        }
                    }
                }
                complete = complete && active_parts == 1;
            }

            constexpr std::size_t AVATAR_PALETTE_FLOATS =
                LL_CHARACTER_MAX_JOINTS_PER_MESH * 3 * 4;
            std::vector<std::array<F32, AVATAR_PALETTE_FLOATS>> palettes;
            std::vector<std::vector<F32>> rigged_palettes;
            std::vector<ALMetalLitPreviewBatch> batches;
            palettes.reserve(preview_meshes.size());
            rigged_palettes.reserve(rigged_faces.size());
            batches.reserve(preview_meshes.size() + rigged_faces.size());
            const LLMatrix4a modelview = gGL.getModelviewMatrix();
            const LLMatrix4a projection = gGL.getProjectionMatrix();
            LLMatrix4a rigged_modelview_projection;
            rigged_modelview_projection.setMul(modelview, projection);
            const bool avatar_cloth =
                gPipeline.shaderInputs().mLocal.mAvatarCloth;
            LLVector4 cloth_wind;
            LLVector4 cloth_sin_wave;
            LLVector4 cloth_gravity;
            LLVector4 cloth_depth(0.f, 0.f, 1.f, 0.f);
            if (avatar_cloth)
            {
                // Keep these values byte-for-byte aligned with
                // LLDrawPoolAvatar::set_cloth_uniforms(), which owns the GL
                // draw mirrored by this scene.
                const LLMatrix4& cloth_view =
                    LLDrawPoolAvatar::getModelView();
                cloth_wind.setVec(gAgentAvatarp->mWindVec);
                cloth_wind.mV[VW] = 0.f;
                cloth_wind = cloth_wind * cloth_view;
                cloth_wind.mV[VW] = gAgentAvatarp->mWindVec.mV[VW];

                const F32 phase = -gAgentAvatarp->mRipplePhase;
                const F32 frequency =
                    7.f + noise1(gAgentAvatarp->mRipplePhase) * 2.f;
                cloth_sin_wave.set(
                    frequency, frequency, frequency, phase);

                cloth_gravity.set(0.f, 0.f, -0.7f, 0.f);
                cloth_gravity = cloth_gravity * cloth_view;

                LLViewerCamera* main_camera =
                    LLViewerCamera::getInstance();
                if (&LLViewerCamera::getCurrent() != main_camera)
                {
                    LLMatrix4 main_view;
                    main_camera->getModelview().store(main_view);
                    LLMatrix4 to_main = cloth_view;
                    to_main.invert();
                    to_main *= main_view;
                    cloth_depth.set(
                        to_main.mMatrix[0][2], to_main.mMatrix[1][2],
                        to_main.mMatrix[2][2], to_main.mMatrix[3][2]);
                }
            }

            // The deferred geometry pass draws opaque and alpha-masked rigged
            // attachments before the avatar pool. Split texture-indexed GL
            // batches back into their exact face ranges so each native batch
            // can bind the face's real diffuse mirror without a texture
            // ladder. Other material/forward families remain fail-closed.
            constexpr U32 rigged_pools[] = {
                LLDrawPool::POOL_SIMPLE,
                LLDrawPool::POOL_ALPHA_MASK,
            };
            for (LLFace* face : rigged_faces)
            {
                complete = complete &&
                    (face->getPoolType() == LLDrawPool::POOL_SIMPLE ||
                     face->getPoolType() == LLDrawPool::POOL_ALPHA_MASK);
            }
            for (U32 pool : rigged_pools)
            {
                for (LLFace* face : rigged_faces)
                {
                    if (!complete || face->getPoolType() != pool)
                    {
                        continue;
                    }
                    LLDrawInfo* draw_info = face->mDrawInfo;
                    LLVertexBuffer* buffer = face->getVertexBuffer();
                    LLViewerTexture* diffuse = face->getTexture();
                    LLImageGL* diffuse_image = diffuse
                        ? diffuse->getGLTexture() : nullptr;
                    const ALMetalTexture* diffuse_texture = diffuse_image
                        ? diffuse_image->getMetalTexture() : nullptr;
                    const bool uses_texture_matrix = draw_info &&
                        draw_info->mTextureList.size() <= 1 &&
                        draw_info->mTextureMatrix;
                    complete = draw_info && buffer && diffuse_texture &&
                        !uses_texture_matrix && face->getIndicesStart() >= 0 &&
                        face->getIndicesCount() > 0 &&
                        buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) &&
                        buffer->hasDataType(LLVertexBuffer::TYPE_NORMAL) &&
                        buffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0) &&
                        buffer->hasDataType(LLVertexBuffer::TYPE_COLOR) &&
                        buffer->hasDataType(LLVertexBuffer::TYPE_WEIGHT4) &&
                        buffer->getMetalVertexBuffer() &&
                        buffer->getMetalIndexBuffer();
                    if (!complete)
                    {
                        break;
                    }

                    const LLVOAvatar::MatrixPaletteCache& palette =
                        gAgentAvatarp->updateSkinInfoMatrixPalette(
                            face->mSkinInfo);
                    const U32 joint_count = static_cast<U32>(
                        palette.mMatrixPalette.size());
                    complete = joint_count > 0 &&
                        palette.mGLMp.size() ==
                            4 + static_cast<std::size_t>(joint_count) * 12;
                    if (!complete)
                    {
                        break;
                    }
                    rigged_palettes.push_back(palette.mGLMp);

                    ALMetalLitPreviewBatch batch;
                    batch.vertices = buffer->getMetalVertexBuffer();
                    batch.position_offset = buffer->getOffset(
                        LLVertexBuffer::TYPE_VERTEX);
                    batch.normal_offset = buffer->getOffset(
                        LLVertexBuffer::TYPE_NORMAL);
                    batch.texcoord_offset = buffer->getOffset(
                        LLVertexBuffer::TYPE_TEXCOORD0);
                    batch.color_offset = buffer->getOffset(
                        LLVertexBuffer::TYPE_COLOR);
                    batch.weight_offset = buffer->getOffset(
                        LLVertexBuffer::TYPE_WEIGHT4);
                    batch.vertex_count = buffer->getNumVerts();
                    batch.indices = buffer->getMetalIndexBuffer();
                    batch.index_type = buffer->getIndexStride() == 4
                        ? ALMetalIndexType::uint32
                        : ALMetalIndexType::uint16;
                    batch.index_offset = static_cast<std::size_t>(
                        face->getIndicesStart()) * buffer->getIndexStride();
                    batch.index_count = face->getIndicesCount();
                    batch.texture = diffuse_texture;
                    batch.destination_descriptor =
                        mMetalPreviewDescriptor.get();
                    batch.skin_palette = rigged_palettes.back().data();
                    batch.skin_palette_float_count =
                        rigged_palettes.back().size();
                    batch.skin_joint_count = joint_count;
                    batch.deferred_rigged = true;
                    batch.alpha_cutoff =
                        pool == LLDrawPool::POOL_ALPHA_MASK
                        ? draw_info->mAlphaMaskCutoff : 0.f;
                    std::memcpy(batch.modelview_projection,
                                rigged_modelview_projection.getF32ptr(),
                                sizeof(batch.modelview_projection));
                    batches.push_back(batch);
                }
            }

            for (LLViewerJointMesh* viewer_mesh : preview_meshes)
            {
                LLFace* preview_face = nullptr;
                LLPolyMesh* preview_mesh = nullptr;
                LLGLTexture* diffuse = nullptr;
                complete = complete && viewer_mesh->getPreviewGeometry(
                    preview_face, preview_mesh) &&
                    viewer_mesh->getPreviewDiffuseTexture(diffuse);
                LLVertexBuffer* buffer = preview_face
                    ? preview_face->getVertexBuffer() : nullptr;
                LLImageGL* diffuse_image = diffuse
                    ? diffuse->getGLTexture() : nullptr;
                const ALMetalTexture* diffuse_texture = diffuse_image
                    ? diffuse_image->getMetalTexture() : nullptr;
                complete = complete && buffer && preview_mesh &&
                    diffuse_texture &&
                    buffer->hasDataType(LLVertexBuffer::TYPE_VERTEX) &&
                    buffer->hasDataType(LLVertexBuffer::TYPE_NORMAL) &&
                    buffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0) &&
                    buffer->getMetalVertexBuffer() &&
                    buffer->getMetalIndexBuffer();
                if (!complete)
                {
                    break;
                }

                ALMetalLitPreviewBatch batch;
                batch.vertices = buffer->getMetalVertexBuffer();
                batch.position_offset = buffer->getOffset(
                    LLVertexBuffer::TYPE_VERTEX);
                batch.normal_offset = buffer->getOffset(
                    LLVertexBuffer::TYPE_NORMAL);
                batch.texcoord_offset = buffer->getOffset(
                    LLVertexBuffer::TYPE_TEXCOORD0);
                batch.vertex_count = buffer->getNumVerts();
                batch.indices = buffer->getMetalIndexBuffer();
                batch.index_type = buffer->getIndexStride() == 4
                    ? ALMetalIndexType::uint32 : ALMetalIndexType::uint16;
                batch.index_offset = static_cast<std::size_t>(
                    preview_mesh->mFaceIndexOffset) *
                    buffer->getIndexStride();
                batch.index_count = preview_mesh->mFaceIndexCount;
                batch.texture = diffuse_texture;
                batch.destination_descriptor =
                    mMetalPreviewDescriptor.get();
                batch.avatar_unlit = true;

                if (preview_mesh->hasWeights())
                {
                    complete = buffer->hasDataType(
                        LLVertexBuffer::TYPE_WEIGHT) &&
                        (!avatar_cloth || buffer->hasDataType(
                            LLVertexBuffer::TYPE_CLOTHWEIGHT));
                    if (!complete)
                    {
                        break;
                    }
                    palettes.emplace_back();
                    complete = viewer_mesh->buildJointMatrixPalette(
                        palettes.back().data(), palettes.back().size());
                    if (!complete)
                    {
                        break;
                    }
                    batch.weight_offset = buffer->getOffset(
                        LLVertexBuffer::TYPE_WEIGHT);
                    batch.skin_palette = palettes.back().data();
                    batch.skin_palette_float_count = palettes.back().size();
                    batch.skin_joint_count =
                        LL_CHARACTER_MAX_JOINTS_PER_MESH;
                    batch.avatar_skinning = true;
                    if (avatar_cloth)
                    {
                        batch.cloth_offset = buffer->getOffset(
                            LLVertexBuffer::TYPE_CLOTHWEIGHT);
                        batch.avatar_cloth = true;
                        std::memcpy(batch.avatar_wind, cloth_wind.mV,
                                    sizeof(batch.avatar_wind));
                        std::memcpy(batch.avatar_sin_wave,
                                    cloth_sin_wave.mV,
                                    sizeof(batch.avatar_sin_wave));
                        std::memcpy(batch.avatar_gravity,
                                    cloth_gravity.mV,
                                    sizeof(batch.avatar_gravity));
                        std::memcpy(batch.avatar_cloth_depth,
                                    cloth_depth.mV,
                                    sizeof(batch.avatar_cloth_depth));
                    }
                    std::memcpy(batch.modelview_projection,
                                projection.getF32ptr(),
                                sizeof(batch.modelview_projection));
                }
                else
                {
                    const LLMatrix4a& joint_world =
                        viewer_mesh->getWorldMatrix();
                    LLMatrix4a rigid_modelview;
                    rigid_modelview.setMul(joint_world, modelview);
                    LLMatrix4a modelview_projection;
                    modelview_projection.setMul(
                        rigid_modelview, projection);
                    std::memcpy(batch.modelview_projection,
                                modelview_projection.getF32ptr(),
                                sizeof(batch.modelview_projection));
                }
                batches.push_back(batch);
            }

            if (complete && !batches.empty())
            {
                ALMetalLitPreviewScene scene;
                scene.batches = batches.data();
                scene.batch_count = batches.size();
                scene.background_texture = background_texture;
                scene.destination_descriptor =
                    mMetalPreviewDescriptor.get();
                scene.clear_color[0] = 0.f;
                scene.clear_color[1] = 0.f;
                scene.clear_color[2] = 0.f;
                scene.clear_color[3] = 0.f;
                mMetalPreview = renderer->render(scene, &error);
            }
            else if (error.empty())
            {
                error = "visual-parameter avatar scene is incomplete";
            }
            if (!mMetalPreview)
            {
                LL_DEBUGS("Metal")
                    << "Visual-parameter hint retained GL: "
                    << error << LL_ENDL;
            }
        }
        else
        {
            LL_DEBUGS("Metal")
                << "Visual-parameter hint retained GL because its native "
                   "background, device or destination is incomplete"
                << LL_ENDL;
        }
#endif
    }

    gAgentAvatarp->setVisualParamWeight(mVisualParam->getID(), mLastParamWeight);
    mWearablePtr->setVisualParamWeight(mVisualParam->getID(), mLastParamWeight);
    LLViewerWearable* wearable = dynamic_cast<LLViewerWearable*>(mWearablePtr);
    if (wearable)
    {
        wearable->setVolatile(false);
    }

    gAgentAvatarp->updateVisualParams();
    gGL.color4f(1,1,1,1);
    mGLTexturep->setGLTextureCreated(true);
    gGL.popUIMatrix();

    return true;
}

void LLVisualParamHint::postRender(bool success)
{
    LLViewerDynamicTexture::postRender(success);
#if LL_DARWIN
    if (lastPostRenderSucceeded() && mMetalPreview)
    {
        if (mGLTexturep->adoptMetalRenderTexture(std::move(mMetalPreview)))
        {
            LL_INFOS_ONCE("Metal")
                << "Visual-parameter hints own a native Metal render target"
                << LL_ENDL;
        }
        else
        {
            LL_WARNS("Metal")
                << "Could not publish the native visual-parameter hint"
                << LL_ENDL;
        }
    }
    mMetalPreview.reset();
#endif
}


//-----------------------------------------------------------------------------
// draw()
//-----------------------------------------------------------------------------
void LLVisualParamHint::draw(F32 alpha)
{
    if (!mIsVisible) return;

    // Clamp: the hint is a full [0,1] quad, and this dynamic texture asked for clamped
    // addressing (constructor clamp=true) back when that lived on the texture object --
    // wrap would blend the opposite border into the preview's edges.
    gGL.getTextureSlot(0)->bindSampled(this, ALSamplers::AnisoClamp);

    gGL.color4f(1.f, 1.f, 1.f, alpha);

    LLGLSUIDefault gls_ui;
    gGL.begin(LLRender::TRIANGLES);
    {
        gGL.texCoord2i(0, 1);
        gGL.vertex2i(0, mFullHeight);
        gGL.texCoord2i(0, 0);
        gGL.vertex2i(0, 0);
        gGL.texCoord2i(1, 0);
        gGL.vertex2i(mFullWidth, 0);

        gGL.texCoord2i(0, 1);
        gGL.vertex2i(0, mFullHeight);
        gGL.texCoord2i(1, 0);
        gGL.vertex2i(mFullWidth, 0);
        gGL.texCoord2i(1, 1);
        gGL.vertex2i(mFullWidth, mFullHeight);
    }
    gGL.end();

    gGL.getTextureSlot(0)->unbind();
}

//-----------------------------------------------------------------------------
// LLVisualParamReset()
//-----------------------------------------------------------------------------
LLVisualParamReset::LLVisualParamReset() : LLViewerDynamicTexture(1, 1, 1, ORDER_RESET)
{
}

//virtual
S8 LLVisualParamReset::getType() const
{
    return LLViewerDynamicTexture::LL_VISUAL_PARAM_RESET ;
}

//-----------------------------------------------------------------------------
// render()
//-----------------------------------------------------------------------------
bool LLVisualParamReset::render()
{
    if (sDirty)
    {
        gAgentAvatarp->updateComposites();
        gAgentAvatarp->updateVisualParams();
        gAgentAvatarp->updateGeometry(gAgentAvatarp->mDrawable);
        sDirty = false;
    }

    return false;
}
