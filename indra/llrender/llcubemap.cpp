/**
 * @file llcubemap.cpp
 * @brief LLCubeMap class implementation
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
#include "linden_common.h"

#include "llcubemap.h"

#include "v4coloru.h"
#include "v3math.h"
#include "v3dmath.h"
#include "m3math.h"
#include "m4math.h"

#include "llrender.h"
#include "llimagegl.h"

#if LL_DARWIN
#include "almetaldevice.h"
#include "almetalresource.h"
#endif

namespace {
    const U16 RESOLUTION = 64;
}

LLCubeMap::LLCubeMap()
    : mTextureStage(0),
      mMatrixStage(0)
{
}

LLCubeMap::~LLCubeMap()
{
}

void LLCubeMap::initRawData(const std::vector<LLPointer<LLImageRaw> >& rawimages)
{
    bool flip_x[6] =    { false, true,  false, false, true,  false };
    bool flip_y[6] =    { true,  true,  true,  false, true,  true  };
    bool transpose[6] = { false, false, false, false, true,  true  };

    // Yes, I know that this is inefficient! - djs 08/08/02
    for (int i = 0; i < 6; i++)
    {
        if (mRawImages[i].isNull())
        {
            mRawImages[i] = new LLImageRaw(RESOLUTION, RESOLUTION, 4);
        }
        LLImageDataCopyLock lock(rawimages[i], mRawImages[i]);

        const U8 *sd = rawimages[i]->getData();
        U8 *td = mRawImages[i]->getData();

        S32 offset = 0;
        S32 sx, sy, so;
        for (int y = 0; y < 64; y++)
        {
            for (int x = 0; x < 64; x++)
            {
                sx = x;
                sy = y;
                if (flip_y[i])
                {
                    sy = 63 - y;
                }
                if (flip_x[i])
                {
                    sx = 63 - x;
                }
                if (transpose[i])
                {
                    S32 temp = sx;
                    sx = sy;
                    sy = temp;
                }

                so = 64*sy + sx;
                so *= 4;
                *(td + offset++) = *(sd + so++);
                *(td + offset++) = *(sd + so++);
                *(td + offset++) = *(sd + so++);
                *(td + offset++) = *(sd + so++);
            }
        }
    }
#if LL_DARWIN
    mMetalDirty = true;
#endif
}

void LLCubeMap::init(const std::vector<LLPointer<LLImageRaw> >& rawimages)
{
    initRawData(rawimages);
#if LL_DARWIN
    std::string error;
    if (!getMetalTexture(&error))
    {
        LL_WARNS("Metal")
            << "Could not publish the legacy sky cube to Metal: "
            << error << LL_ENDL;
    }
#endif
}

#if LL_DARWIN
ALMetalTexture* LLCubeMap::getMetalTexture(std::string* error)
{
    ALMetalDevice* device = LLImageGL::getMetalDevice();
    if (!device)
    {
        if (error)
        {
            *error = "No Metal device is available for the legacy sky cube";
        }
        return nullptr;
    }
    if (!mMetalDirty && mMetalDevice == device && mMetalTexture)
    {
        return mMetalTexture.get();
    }
    for (const LLPointer<LLImageRaw>& image : mRawImages)
    {
        if (image.isNull() || image->getWidth() != RESOLUTION ||
            image->getHeight() != RESOLUTION ||
            image->getComponents() != 4)
        {
            if (error)
            {
                *error = "The legacy sky cube faces are incomplete";
            }
            return nullptr;
        }
    }

    ALMetalTextureDescriptor descriptor;
    descriptor.type = ALMetalTextureType::texture_cube;
    descriptor.format = ALMetalPixelFormat::rgba8_unorm;
    descriptor.width = RESOLUTION;
    descriptor.height = RESOLUTION;
    descriptor.storage = ALMetalStorageMode::private_gpu;
    descriptor.usage = ALMetalTextureUsage::shader_read;
    descriptor.label = "Alchemy legacy sky environment cube";
    auto replacement = ALMetalTexture::create(*device, descriptor, error);
    auto upload = replacement
        ? device->makeCommandBuffer(
              "Alchemy legacy sky environment cube upload", error)
        : nullptr;
    if (!upload)
    {
        return nullptr;
    }

    // LLCubeMap's historical source order is -X,+X,-Y,+Y,-Z,+Z.
    // OpenGL and Metal cube storage both use +X,-X,+Y,-Y,+Z,-Z, and the
    // already transformed mRawImages bytes have the GL-compatible in-face
    // orientation. The live reflection-probe bridge relies on the same
    // GL-to-Metal face-byte equivalence.
    constexpr U32 destination_slice[6] = {1, 0, 3, 2, 5, 4};
    bool uploaded = true;
    for (U32 source = 0; uploaded && source < 6; ++source)
    {
        LLImageDataSharedLock lock(mRawImages[source]);
        uploaded = replacement->upload(
            *upload, 0, destination_slice[source], {},
            mRawImages[source]->getData(),
            static_cast<std::size_t>(RESOLUTION) * 4, error);
    }
    uploaded = uploaded && upload->commit(error);
    if (!uploaded)
    {
        return nullptr;
    }

    mMetalTexture = std::move(replacement);
    mMetalDevice = device;
    mMetalDirty = false;
    return mMetalTexture.get();
}
#endif

void LLCubeMap::bind()
{
    // Clamp + anisotropic: what the environment map has always been sampled with, named
    // here now instead of written onto face 0 at load.
    gGL.getTextureSlot(mTextureStage)->bind(this, ALSamplers::AnisoClamp);
}

void LLCubeMap::enable(S32 stage)
{
    enableTexture(stage);
}

// Remembers which slot bind() will use. It used to also mark that slot "enabled for
// TT_CUBE_MAP" ahead of the bind that would set the target anyway.
void LLCubeMap::enableTexture(S32 stage)
{
    mTextureStage = stage;
}

void LLCubeMap::disable(void)
{
    disableTexture();
}

void LLCubeMap::disableTexture(void)
{
    if (mTextureStage >= 0)
    {
        // Followed by re-enabling slot 0 for TT_TEXTURE when the cube map had been on it --
        // restoring a target for the benefit of nothing, since the next bind names its own.
        gGL.getTextureSlot(mTextureStage)->unbind();
    }
}

void LLCubeMap::setMatrix(S32 stage, const LLMatrix4a& modelview_matrix)
{
    mMatrixStage = stage;

    if (mMatrixStage < 0) return;

    // No getTextureSlot(stage)->activate() here any more: it guarded a texture matrix stack
    // that MM_TEXTURE0 no longer selects through the active unit. See LLRender::eMatrixMode.

    const F32* modelview = modelview_matrix.getF32ptr();
    LLVector3 x(modelview+0);
    LLVector3 y(modelview+4);
    LLVector3 z(modelview+8);

    LLMatrix3 mat3;
    mat3.setRows(x,y,z);
    LLMatrix4 trans(mat3);
    trans.transpose();

    gGL.matrixMode(LLRender::MM_TEXTURE0);
    gGL.pushMatrix();
    gGL.loadMatrix((F32 *)trans.mMatrix);
    gGL.matrixMode(LLRender::MM_MODELVIEW);
}

void LLCubeMap::restoreMatrix()
{
    if (mMatrixStage < 0) return;

    gGL.matrixMode(LLRender::MM_TEXTURE0);
    gGL.popMatrix();
    gGL.matrixMode(LLRender::MM_MODELVIEW);
}
