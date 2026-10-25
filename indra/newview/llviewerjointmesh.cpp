 /**
 * @file llviewerjointmesh.cpp
 * @brief Implementation of LLViewerJointMesh class
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

//-----------------------------------------------------------------------------
// Header Files
//-----------------------------------------------------------------------------
#include "llviewerprecompiledheaders.h"

#include "llrender.h"

#include "llbox.h"
#include "lldrawable.h"
#include "lldrawpoolavatar.h"
#include "lldrawpoolbump.h"
#include "lldynamictexture.h"
#include "llface.h"
#include "llglheaders.h"
#include "llviewertexlayer.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llviewerjointmesh.h"
#include "llvoavatar.h"
#include "llsky.h"
#include "pipeline.h"
#include "llviewershadermgr.h"
#include "llmath.h"
#include "v4math.h"
#include "m3math.h"
#include "m4math.h"
#include "llmatrix4a.h"
#include "llperfstats.h"

#include <algorithm>
#include <array>

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// LLViewerJointMesh
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// LLViewerJointMesh()
//-----------------------------------------------------------------------------
LLViewerJointMesh::LLViewerJointMesh()
    :
    LLAvatarJointMesh()
{
}


//-----------------------------------------------------------------------------
// ~LLViewerJointMesh()
// Class Destructor
//-----------------------------------------------------------------------------
LLViewerJointMesh::~LLViewerJointMesh()
{
}

const S32 NUM_AXES = 3;

// register layoud
// rotation X 0-n
// rotation Y 0-n
// rotation Z 0-n
// pivot parent 0-n -- child = n+1

bool LLViewerJointMesh::buildJointMatrixPalette(
    F32* destination,
    std::size_t float_count) const
{
    constexpr std::size_t PALETTE_FLOAT_COUNT =
        LL_CHARACTER_MAX_JOINTS_PER_MESH * NUM_AXES * 4;
    if (!destination || float_count < PALETTE_FLOAT_COUNT || !mMesh)
    {
        return false;
    }

    LLPolyMesh* reference_mesh = mMesh->getReferenceMesh();
    if (!reference_mesh)
    {
        return false;
    }

    std::fill_n(destination, PALETTE_FLOAT_COUNT, 0.f);
    std::array<LLMatrix4, LL_CHARACTER_MAX_JOINTS_PER_MESH> joint_matrices{};
    std::array<LLMatrix3, LL_CHARACTER_MAX_JOINTS_PER_MESH> joint_rotations{};
    std::array<LLVector4, LL_CHARACTER_MAX_JOINTS_PER_MESH> joint_pivots{};

    LLMatrix4a model_view;
    model_view.loadu(&LLDrawPoolAvatar::getModelView().mMatrix[0][0]);
    const std::size_t num_joints = llmin(
        reference_mesh->mJointRenderData.size(),
        static_cast<std::size_t>(LL_CHARACTER_MAX_JOINTS_PER_MESH));
    for (std::size_t joint_num = 0; joint_num < num_joints; ++joint_num)
    {
        LLMatrix4a joint_matrix;
        joint_matrix.setMulNoAlias(
            reference_mesh->mJointRenderData[joint_num]->mJoint->getWorldMatrix(),
            model_view);
        joint_matrices[joint_num] = joint_matrix.toMatrix4();
        joint_rotations[joint_num] = joint_matrices[joint_num].getMat3();
    }

    bool last_pivot_added = false;
    std::size_t pivot_count = 0;
    for (std::size_t joint_num = 0;
         joint_num < reference_mesh->mJointRenderData.size() &&
         pivot_count < joint_pivots.size();
         ++joint_num)
    {
        LLSkinJoint* skin_joint =
            reference_mesh->mJointRenderData[joint_num]->mSkinJoint;
        if (!skin_joint)
        {
            last_pivot_added = false;
            continue;
        }
        if (!last_pivot_added)
        {
            joint_pivots[pivot_count] =
                LLVector4(skin_joint->mRootToParentJointSkinOffset);
            joint_pivots[pivot_count].mV[VW] = 0.f;
            if (++pivot_count >= joint_pivots.size())
            {
                break;
            }
        }
        joint_pivots[pivot_count] =
            LLVector4(skin_joint->mRootToJointSkinOffset);
        joint_pivots[pivot_count].mV[VW] = 0.f;
        ++pivot_count;
        last_pivot_added = true;
    }

    for (std::size_t index = 0; index < pivot_count && index < num_joints;
         ++index)
    {
        LLVector3 pivot(joint_pivots[index]);
        pivot = pivot * joint_rotations[index];
        joint_matrices[index].translate(pivot);
    }

    for (std::size_t joint_num = 0; joint_num < num_joints; ++joint_num)
    {
        joint_matrices[joint_num].transpose();
        for (std::size_t axis = 0; axis < NUM_AXES; ++axis)
        {
            const F32* vector = joint_matrices[joint_num].mMatrix[axis];
            const std::size_t offset =
                (LL_CHARACTER_MAX_JOINTS_PER_MESH * axis + joint_num) * 4;
            std::memcpy(destination + offset, vector, sizeof(F32) * 4);
        }
    }
    return true;
}

//-----------------------------------------------------------------------------
// uploadJointMatrices()
//-----------------------------------------------------------------------------
void LLViewerJointMesh::uploadJointMatrices()
{
    std::array<GLfloat, LL_CHARACTER_MAX_JOINTS_PER_MESH * NUM_AXES * 4>
        matrix_palette{};
    if (!buildJointMatrixPalette(matrix_palette.data(), matrix_palette.size()))
    {
        return;
    }
    stop_glerror();
    if (LLGLSLShader::sCurBoundShaderPtr)
    {
        LLGLSLShader::sCurBoundShaderPtr->uniform4fv(
            LLViewerShaderMgr::AVATAR_MATRIX, 45, matrix_palette.data());
    }
    stop_glerror();
}

//--------------------------------------------------------------------
// DrawElementsBLEND and utility code
//--------------------------------------------------------------------

// compare_int is used by the qsort function to sort the index array
int compare_int(const void *a, const void *b)
{
    if (*(U32*)a < *(U32*)b)
    {
        return -1;
    }
    else if (*(U32*)a > *(U32*)b)
    {
        return 1;
    }
    else return 0;
}

//--------------------------------------------------------------------
// LLViewerJointMesh::drawShape()
//--------------------------------------------------------------------
U32 LLViewerJointMesh::drawShape( F32 pixelArea, bool first_pass, bool is_dummy)
{
    if (!mValid || !mMesh || !mFace || !mVisible ||
        !mFace->getVertexBuffer() ||
        mMesh->getNumFaces() == 0 ||
        mMesh->mFaceVertexCount == 0 ||
        LLGLSLShader::sCurBoundShaderPtr == NULL)
    {
        return 0;
    }

    U32 triangle_count = 0;

    S32 diffuse_channel = LLDrawPoolAvatar::sDiffuseChannel;

    stop_glerror();

    //----------------------------------------------------------------
    // setup current color
    //----------------------------------------------------------------
    if (is_dummy)
        gGL.diffuseColor4fv(LLVOAvatar::getDummyColor().mV);
    else
        gGL.diffuseColor4fv(mColor.mV);

    stop_glerror();

    //----------------------------------------------------------------
    // setup current texture
    //----------------------------------------------------------------
    llassert( !(mTexture.notNull() && mLayerSet) );  // mutually exclusive

    LLViewerTexLayerSet *layerset = dynamic_cast<LLViewerTexLayerSet*>(mLayerSet);

    // The avatar skin composite (and the stand-in diffuse below) is an sRGB texture. Decode
    // it on the sampler when the bound program shades in linear -- the deferred avatar writer
    // and the forward avatar-skin alpha both do (mLinearDiffuse), and both had their in-shader
    // decode removed, so the decode has to happen here or the skin reads too bright. The
    // deferred writer's gbuffer store re-encodes via FRAMEBUFFER_SRGB (see LLDrawPoolAvatar).
    const LLGLSLShader* bound = LLGLSLShader::sCurBoundShaderPtr;
    const ALSampler skin_key = (bound && bound->mLinearDiffuse)
                             ? ALSamplers::AnisoWrapSRGB
                             : ALSamplers::AnisoWrap;

    if (mTestImageName)
    {
        gGL.getTextureSlot(diffuse_channel)->bindManual(ALTextureSlot::TT_TEXTURE, mTestImageName);

        if (mIsTransparent)
        {
            gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
        }
        else
        {
            gGL.diffuseColor4f(0.7f, 0.6f, 0.3f, 1.f);
        }
    }
    else
    {
        LLGLTexture* diffuse = nullptr;
        if (!is_dummy && layerset)
        {
            diffuse = layerset->hasComposite() ? layerset->getViewerComposite() : nullptr;
        }
        else if (!is_dummy)
        {
            diffuse = mTexture.get();
        }
        if (!diffuse)
        {
            diffuse = LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT);
        }

        if (!LLDrawPoolAvatar::shadowDrawsBake(diffuse))
        {
            return 0;
        }

        gGL.getTextureSlot(diffuse_channel)->bindSampled(diffuse, skin_key);
    }

    U32 start = mMesh->mFaceVertexOffset;
    U32 end = start + mMesh->mFaceVertexCount - 1;
    U32 count = mMesh->mFaceIndexCount;
    U32 offset = mMesh->mFaceIndexOffset;

    LLVertexBuffer* buff = mFace->getVertexBuffer();

    if (mMesh->hasWeights())
    {
        if (first_pass)
        {
            uploadJointMatrices();
        }

        buff->setBuffer();
        buff->drawRange(LLRender::TRIANGLES, start, end, count, offset);
    }
    else
    {
        gGL.pushMatrix();
        LLMatrix4 jointToWorld = getWorldMatrix().toMatrix4();
        gGL.multMatrix((GLfloat*)jointToWorld.mMatrix);
        buff->setBuffer();
        buff->drawRange(LLRender::TRIANGLES, start, end, count, offset);
        gGL.popMatrix();
    }
    gPipeline.addTrianglesDrawn(count);

    triangle_count += count;

    return triangle_count;
}

//-----------------------------------------------------------------------------
// updateFaceSizes()
//-----------------------------------------------------------------------------
void LLViewerJointMesh::updateFaceSizes(U32 &num_vertices, U32& num_indices, F32 pixel_area)
{
    //bump num_vertices to next multiple of 4
    num_vertices = (num_vertices + 0x3) & ~0x3;

    // Do a pre-alloc pass to determine sizes of data.
    if (mMesh && mValid)
    {
        mMesh->mFaceVertexOffset = num_vertices;
        mMesh->mFaceVertexCount = mMesh->getNumVertices();
        mMesh->mFaceIndexOffset = num_indices;
        mMesh->mFaceIndexCount = mMesh->getSharedData()->mNumTriangleIndices;

        mMesh->getReferenceMesh()->mCurVertexCount = mMesh->mFaceVertexCount;

        num_vertices += mMesh->getNumVertices();
        num_indices += mMesh->mFaceIndexCount;
    }
}

//-----------------------------------------------------------------------------
// updateFaceData()
//-----------------------------------------------------------------------------

void LLViewerJointMesh::updateFaceData(LLFace *face, F32 pixel_area, bool damp_wind, bool terse_update)
{
    //IF THIS FUNCTION BREAKS, SEE LLPOLYMESH CONSTRUCTOR AND CHECK ALIGNMENT OF INPUT ARRAYS

    mFace = face;

    if (!mFace->getVertexBuffer())
    {
        return;
    }

    LL_PROFILE_ZONE_SCOPED;

    LLStrider<LLVector3> verticesp;
    LLStrider<LLVector3> normalsp;
    LLStrider<LLVector2> tex_coordsp;
    LLStrider<F32>       vertex_weightsp;
    LLStrider<LLVector4> clothing_weightsp;
    LLStrider<U16> indicesp;

    // Copy data into the faces from the polymesh data.
    if (mMesh && mValid)
    {
        const U32 num_verts = mMesh->getNumVertices();

        if (num_verts)
        {
            face->getVertexBuffer()->getIndexStrider(indicesp);
            face->getGeometryAvatar(verticesp, normalsp, tex_coordsp, vertex_weightsp, clothing_weightsp);

            verticesp += mMesh->mFaceVertexOffset;
            normalsp += mMesh->mFaceVertexOffset;

            F32* v = (F32*) verticesp.get();
            F32* n = (F32*) normalsp.get();

            U32 words = num_verts*4;

            LLVector4a::memcpyNonAliased16(v, (F32*) mMesh->getCoords(), words*sizeof(F32));
            LLVector4a::memcpyNonAliased16(n, (F32*) mMesh->getNormals(), words*sizeof(F32));


            if (!terse_update)
            {
                vertex_weightsp += mMesh->mFaceVertexOffset;
                clothing_weightsp += mMesh->mFaceVertexOffset;
                tex_coordsp += mMesh->mFaceVertexOffset;

                F32* tc = (F32*) tex_coordsp.get();
                F32* vw = (F32*) vertex_weightsp.get();
                F32* cw = (F32*) clothing_weightsp.get();

                //S32 tc_size = (num_verts*2*sizeof(F32)+0xF) & ~0xF;
                //LLVector4a::memcpyNonAliased16(tc, (F32*) mMesh->getTexCoords(), tc_size);
                //S32 vw_size = (num_verts*sizeof(F32)+0xF) & ~0xF;
                //LLVector4a::memcpyNonAliased16(vw, (F32*) mMesh->getWeights(), vw_size);

                // Both allocated in LLPolyMeshSharedData::allocateVertexData(unsigned int)

                memcpy(tc, mMesh->getTexCoords(), num_verts*2*sizeof(F32) );
                memcpy(vw, mMesh->getWeights(), num_verts*sizeof(F32) );

                LLVector4a::memcpyNonAliased16(cw, (F32*) mMesh->getClothingWeights(), num_verts*4*sizeof(F32));
            }

            const U32 idx_count = mMesh->getNumFaces()*3;

            indicesp += mMesh->mFaceIndexOffset;

            U16* __restrict idx = indicesp.get();
            S32* __restrict src_idx = (S32*) mMesh->getFaces();

            const S32 offset = (S32) mMesh->mFaceVertexOffset;

            for (U32 i = 0; i < idx_count; ++i)
            {
                *(idx++) = *(src_idx++)+offset;
            }
        }
    }
}



//-----------------------------------------------------------------------------
// updateLOD()
//-----------------------------------------------------------------------------
bool LLViewerJointMesh::updateLOD(F32 pixel_area, bool activate)
{
    bool valid = mValid;
    setValid(activate, true);
    return (valid != activate);
}

void LLViewerJointMesh::updateJointGeometry()
{
}

void LLViewerJointMesh::dump()
{
    if (mValid)
    {
        LL_INFOS() << "Usable LOD " << mName << LL_ENDL;
    }
}

// End
