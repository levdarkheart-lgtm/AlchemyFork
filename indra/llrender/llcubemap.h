/**
 * @file llcubemap.h
 * @brief LLCubeMap class definition
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

#ifndef LL_LLCUBEMAP_H
#define LL_LLCUBEMAP_H

#include "llimage.h"
#include "llpointer.h"
#include "llrefcount.h"

#include <memory>
#include <string>
#include <vector>

class LLVector3;
class LLMatrix4a;
#if LL_DARWIN
class ALMetalDevice;
class ALMetalTexture;
#endif

// Environment map hack!
class LLCubeMap : public LLRefCount
{
public:
    LLCubeMap();
    void init(const std::vector<LLPointer<LLImageRaw> >& rawimages);

    void initRawData(const std::vector<LLPointer<LLImageRaw> >& rawimages);

    void bind();
    void enable(S32 stage);

    void enableTexture(S32 stage);
    S32  getStage(void) { return mTextureStage; }

    void disable(void);
    void disableTexture(void);
    // Loads the rotation of the modelview into the texture matrix, so the
    // lookup direction is in world space
    void setMatrix(S32 stage, const LLMatrix4a& modelview);
    void restoreMatrix();

    U32 getResolution()
    {
        return mRawImages[0].notNull() ? mRawImages[0]->getWidth() : 0;
    }

#if LL_DARWIN
    // Return the coherent native cube containing the six transformed legacy
    // sky faces. It is rebuilt only after initRawData publishes a new sky
    // generation or the Metal device changes; a failed rebuild never exposes
    // the previous generation.
    ALMetalTexture* getMetalTexture(std::string* error = nullptr);
#endif

protected:
    ~LLCubeMap();
    LLPointer<LLImageRaw> mRawImages[6];
    S32 mTextureStage;
    S32 mMatrixStage;
#if LL_DARWIN
    std::unique_ptr<ALMetalTexture> mMetalTexture;
    ALMetalDevice* mMetalDevice = nullptr;
    bool mMetalDirty = true;
#endif
};

#endif
