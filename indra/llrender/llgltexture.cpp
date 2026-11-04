/**
 * @file llgltexture.cpp
 * @brief Opengl texture implementation
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
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
#include "llgltexture.h"


LLGLTexture::LLGLTexture(bool usemipmaps)
{
    init();
    mUseMipMaps = usemipmaps;
}

LLGLTexture::LLGLTexture(const U32 width, const U32 height, const U8 components, bool usemipmaps)
{
    init();
    mFullWidth = width ;
    mFullHeight = height ;
    mUseMipMaps = usemipmaps;
    mComponents = components ;
    setTexelsPerImage();
}

LLGLTexture::LLGLTexture(const LLImageRaw* raw, bool usemipmaps)
{
    init();
    mUseMipMaps = usemipmaps ;
    // Create an empty image of the specified size and width
    mGLTexturep = new LLImageGL(raw, usemipmaps) ;
    mFullWidth = mGLTexturep->getWidth();
    mFullHeight = mGLTexturep->getHeight();
    mComponents = mGLTexturep->getComponents();
    setTexelsPerImage();
}

LLGLTexture::~LLGLTexture()
{
    cleanup();
}

void LLGLTexture::init()
{
    mBoostLevel = LLGLTexture::BOOST_NONE;

    mFullWidth = 0;
    mFullHeight = 0;
    mTexelsPerImage = 0 ;
    mUseMipMaps = false ;
    mComponents = 0 ;

    mTextureState = NO_DELETE ;
    mDontDiscard = false;
    mNeedsGLTexture = false ;
}

void LLGLTexture::cleanup()
{
    if(mGLTexturep)
    {
        mGLTexturep->cleanup();
    }
}

// virtual
void LLGLTexture::dump()
{
    if(mGLTexturep)
    {
        mGLTexturep->dump();
    }
}

void LLGLTexture::setBoostLevel(S32 level)
{
    if(mBoostLevel != level)
    {
        mBoostLevel = level ;
        if(mBoostLevel != LLGLTexture::BOOST_NONE
           && mBoostLevel != LLGLTexture::BOOST_ICON
           && mBoostLevel != LLGLTexture::BOOST_THUMBNAIL
           && mBoostLevel != LLGLTexture::BOOST_TERRAIN)
        {
            setNoDelete() ;
        }
    }
}

void LLGLTexture::forceActive()
{
    mTextureState = ACTIVE ;
}

void LLGLTexture::setActive()
{
    if(mTextureState != NO_DELETE)
    {
        mTextureState = ACTIVE ;
    }
}

//set the texture to stay in memory
void LLGLTexture::setNoDelete()
{
    mTextureState = NO_DELETE ;
}

void LLGLTexture::generateGLTexture()
{
    if(mGLTexturep.isNull())
    {
        mGLTexturep = new LLImageGL(mFullWidth, mFullHeight, mComponents, mUseMipMaps) ;
    }
}

LLImageGL* LLGLTexture::getGLTexture() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep ;
}

bool LLGLTexture::createGLTexture()
{
    if(mGLTexturep.isNull())
    {
        generateGLTexture() ;
    }

    return mGLTexturep->createGLTexture() ;
}

bool LLGLTexture::createGLTexture(S32 discard_level, const LLImageRaw* imageraw, S32 usename, bool to_create, S32 category, bool defer_copy, LLGLuint* tex_name)
{
    llassert(mGLTexturep.notNull());

    bool ret = mGLTexturep->createGLTexture(discard_level, imageraw, usename, to_create, category, defer_copy, tex_name) ;

    if(ret)
    {
        mFullWidth = mGLTexturep->getCurrentWidth() ;
        mFullHeight = mGLTexturep->getCurrentHeight() ;
        mComponents = mGLTexturep->getComponents() ;
        setTexelsPerImage();
    }

    return ret ;
}

bool LLGLTexture::setImageMetadata(S32 discard_level, const LLImageRaw* imageraw)
{
    llassert(mGLTexturep.notNull());

    const bool updated = mGLTexturep->setImageMetadata(discard_level, imageraw);
    if (updated)
    {
        mFullWidth = mGLTexturep->getCurrentWidth();
        mFullHeight = mGLTexturep->getCurrentHeight();
        mComponents = mGLTexturep->getComponents();
        setTexelsPerImage();
    }
    return updated;
}

#if LL_DARWIN
bool LLGLTexture::createMetalTexture(S32 discard_level, const LLImageRaw* imageraw,
                                     S32 category)
{
    if (mGLTexturep.isNull())
    {
        generateGLTexture();
    }

    bool ret = mGLTexturep->createMetalTexture(discard_level, imageraw, category);
    if (ret)
    {
        mFullWidth = mGLTexturep->getCurrentWidth();
        mFullHeight = mGLTexturep->getCurrentHeight();
        mComponents = mGLTexturep->getComponents();
        setTexelsPerImage();
    }
    return ret;
}
#endif

void LLGLTexture::setExplicitFormat(LLGLint internal_format, LLGLenum primary_format, LLGLenum type_format, bool swap_bytes)
{
    llassert(mGLTexturep.notNull()) ;

    mGLTexturep->setExplicitFormat(internal_format, primary_format, type_format, swap_bytes) ;
}

//virtual
S32 LLGLTexture::getWidth(S32 discard_level) const
{
    if (mGLTexturep.notNull())
    {
        return mGLTexturep->getWidth(discard_level) ;
    }

    // Native producers can create a logical texture first and publish its
    // Metal backing after the initial CPU image is complete.  Its constructor
    // has already recorded the dimensions, so callers such as LLNetMap must be
    // able to size their CPU layer before that first publication.
    S32 width = static_cast<S32>(mFullWidth);
    if (discard_level > 0)
    {
        width >>= discard_level;
    }
    return llmax(width, 1);
}

//virtual
S32 LLGLTexture::getHeight(S32 discard_level) const
{
    if (mGLTexturep.notNull())
    {
        return mGLTexturep->getHeight(discard_level) ;
    }

    S32 height = static_cast<S32>(mFullHeight);
    if (discard_level > 0)
    {
        height >>= discard_level;
    }
    return llmax(height, 1);
}

S32 LLGLTexture::getMaxDiscardLevel() const
{
    llassert(mGLTexturep.notNull()) ;
    return mGLTexturep->getMaxDiscardLevel() ;
}
S32 LLGLTexture::getDiscardLevel() const
{
    llassert(mGLTexturep.notNull()) ;
    return mGLTexturep->getDiscardLevel() ;
}
S8  LLGLTexture::getComponents() const
{
    return mGLTexturep.notNull() ? mGLTexturep->getComponents() : mComponents;
}

LLGLuint LLGLTexture::getTexName() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getTexName() ;
}

bool LLGLTexture::hasGLTexture() const
{
    if(mGLTexturep.notNull())
    {
        return mGLTexturep->getHasGLTexture() ;
    }
    return false ;
}

bool LLGLTexture::hasGPUTexture() const
{
    if (mGLTexturep.notNull())
    {
        return mGLTexturep->getHasGPUTexture();
    }
    return false;
}

bool LLGLTexture::getBoundRecently() const
{
    if(mGLTexturep.notNull())
    {
        return mGLTexturep->getBoundRecently() ;
    }
    return false ;
}

ALTextureSlot::eTextureType LLGLTexture::getTarget(void) const
{
    llassert(mGLTexturep.notNull()) ;
    return mGLTexturep->getTarget() ;
}

bool LLGLTexture::setSubImage(const LLImageRaw* imageraw, S32 x_pos, S32 y_pos, S32 width, S32 height, LLGLuint use_name)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->setSubImage(imageraw, x_pos, y_pos, width, height, 0, use_name) ;
}

bool LLGLTexture::setSubImage(const U8* datap, S32 data_width, S32 data_height, S32 x_pos, S32 y_pos, S32 width, S32 height, LLGLuint use_name)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_TEXTURE;
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->setSubImage(datap, data_width, data_height, x_pos, y_pos, width, height, 0, use_name) ;
}

void LLGLTexture::setGLTextureCreated (bool initialized)
{
    llassert(mGLTexturep.notNull()) ;

    mGLTexturep->setGLTextureCreated (initialized) ;
}

void  LLGLTexture::setCategory(S32 category)
{
    llassert(mGLTexturep.notNull()) ;

    mGLTexturep->setCategory(category) ;
}

void LLGLTexture::setTexName(LLGLuint texName)
{
    llassert(mGLTexturep.notNull());
    return mGLTexturep->setTexName(texName);
}

void LLGLTexture::setTarget(const LLGLenum target, const ALTextureSlot::eTextureType bind_target)
{
    llassert(mGLTexturep.notNull());
    return mGLTexturep->setTarget(target, bind_target);
}

S32Bytes LLGLTexture::getTextureMemory() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getTextureMemory();
}

LLGLenum LLGLTexture::getPrimaryFormat() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getPrimaryFormat() ;
}

bool LLGLTexture::isAlphaOnly() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->isAlphaOnly() ;
}

bool LLGLTexture::getIsAlphaMask() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getIsAlphaMask() ;
}

//bool LLGLTexture::getMask(const LLVector2 &tc)
// [RLVa:KB] - Checked: RLVa-2.2 (@setoverlay)
bool LLGLTexture::getMask(const LLVector2 &tc) const
// [/RLVa:KB]
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getMask(tc) ;
}

F32 LLGLTexture::getTimePassedSinceLastBound()
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getTimePassedSinceLastBound() ;
}
bool LLGLTexture::getMissed() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->getMissed() ;
}

bool LLGLTexture::isJustBound() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->isJustBound() ;
}

void LLGLTexture::forceUpdateBindStats(void) const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->forceUpdateBindStats() ;
}

bool LLGLTexture::isGLTextureCreated() const
{
    llassert(mGLTexturep.notNull()) ;

    return mGLTexturep->isGLTextureCreated() ;
}

void LLGLTexture::destroyGPUTexture()
{
    if (mGLTexturep.notNull() && mGLTexturep->getHasGPUTexture())
    {
        // LLImageGL owns both transitional GL storage and authoritative Metal
        // storage; its legacy-named destruction primitive releases whichever
        // resources are present and reconciles their accounting.
        mGLTexturep->destroyGLTexture();
        mTextureState = DELETED;
    }
}

void LLGLTexture::setTexelsPerImage()
{
    U32 fullwidth = llmin(mFullWidth,U32(MAX_IMAGE_SIZE_DEFAULT));
    U32 fullheight = llmin(mFullHeight,U32(MAX_IMAGE_SIZE_DEFAULT));
    mTexelsPerImage = (U32)fullwidth * fullheight;
}

static LLUUID sStubUUID;

const LLUUID& LLGLTexture::getID() const { return sStubUUID; }
