/**
 * @file llrendertarget.cpp
 * @brief Native Metal implementation of Alchemy's offscreen target contract.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llrendertarget.h"

#include "alimageslot.h"
#include "almetalpipeline.h"
#include "almetalresource.h"
#include "almetaltextureops.h"
#include "alsamplerstate.h"
#include "altextureslot.h"
#include "llimagegl.h"
#include "llrender.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <utility>

LLRenderTarget* LLRenderTarget::sBoundTarget = nullptr;
U32 LLRenderTarget::sBytesAllocated = 0;
U64 LLRenderTarget::sLastDepthSerial = 0;
U32 LLRenderTarget::sCurFBO = 0;
U32 LLRenderTarget::sCurResX = 0;
U32 LLRenderTarget::sCurResY = 0;

extern S32 gGLViewport[4];

namespace
{
ALMetalPixelFormat metal_color_format(U32 format)
{
    switch (format)
    {
        case GL_R8:             return ALMetalPixelFormat::r8_unorm;
        case GL_R8I:            return ALMetalPixelFormat::r8_sint;
        case GL_R8UI:           return ALMetalPixelFormat::r8_uint;
        case GL_RG8:            return ALMetalPixelFormat::rg8_unorm;
        case GL_RG8I:           return ALMetalPixelFormat::rg8_sint;
        case GL_RG8UI:          return ALMetalPixelFormat::rg8_uint;
        // Metal has no three-component render-target formats. Alchemy's
        // native texture ingress uses the same padded RGBA representations.
        case GL_RGB8:
        case GL_RGBA8:          return ALMetalPixelFormat::rgba8_unorm;
        case GL_SRGB8:
        case GL_SRGB8_ALPHA8:   return ALMetalPixelFormat::rgba8_unorm_srgb;
        case GL_R16:            return ALMetalPixelFormat::r16_unorm;
        case GL_R16F:           return ALMetalPixelFormat::r16_float;
        case GL_R16I:           return ALMetalPixelFormat::r16_sint;
        case GL_R16UI:          return ALMetalPixelFormat::r16_uint;
        case GL_RG16F:          return ALMetalPixelFormat::rg16_float;
        case GL_RG16I:          return ALMetalPixelFormat::rg16_sint;
        case GL_RG16UI:         return ALMetalPixelFormat::rg16_uint;
        case GL_RGB16:
        case GL_RGBA16:         return ALMetalPixelFormat::rgba16_unorm;
        case GL_RGB16F:
        case GL_RGBA16F:        return ALMetalPixelFormat::rgba16_float;
        case GL_RGB16I:
        case GL_RGBA16I:        return ALMetalPixelFormat::rgba16_sint;
        case GL_RGB16UI:
        case GL_RGBA16UI:       return ALMetalPixelFormat::rgba16_uint;
        case GL_R32F:           return ALMetalPixelFormat::r32_float;
        case GL_R32I:           return ALMetalPixelFormat::r32_sint;
        case GL_R32UI:          return ALMetalPixelFormat::r32_uint;
        case GL_RG32F:          return ALMetalPixelFormat::rg32_float;
        case GL_RG32I:          return ALMetalPixelFormat::rg32_sint;
        case GL_RG32UI:         return ALMetalPixelFormat::rg32_uint;
        case GL_RGB32F:
        case GL_RGBA32F:        return ALMetalPixelFormat::rgba32_float;
        case GL_RGB32I:
        case GL_RGBA32I:        return ALMetalPixelFormat::rgba32_sint;
        case GL_RGB32UI:
        case GL_RGBA32UI:       return ALMetalPixelFormat::rgba32_uint;
        case GL_RGB10_A2:       return ALMetalPixelFormat::rgb10a2_unorm;
        case GL_RGB10_A2UI:     return ALMetalPixelFormat::rgb10a2_uint;
        case GL_R11F_G11F_B10F:return ALMetalPixelFormat::rg11b10_float;
        default:                return ALMetalPixelFormat::invalid;
    }
}

std::size_t metal_bytes_per_pixel(ALMetalPixelFormat format)
{
    switch (format)
    {
        case ALMetalPixelFormat::r8_unorm:
        case ALMetalPixelFormat::r8_sint:
        case ALMetalPixelFormat::r8_uint:
            return 1;
        case ALMetalPixelFormat::rg8_unorm:
        case ALMetalPixelFormat::rg8_sint:
        case ALMetalPixelFormat::rg8_uint:
        case ALMetalPixelFormat::r16_unorm:
        case ALMetalPixelFormat::r16_float:
        case ALMetalPixelFormat::r16_sint:
        case ALMetalPixelFormat::r16_uint:
            return 2;
        case ALMetalPixelFormat::rgba8_unorm:
        case ALMetalPixelFormat::rgba8_unorm_srgb:
        case ALMetalPixelFormat::bgra8_unorm:
        case ALMetalPixelFormat::bgra8_unorm_srgb:
        case ALMetalPixelFormat::rgba8_sint:
        case ALMetalPixelFormat::rgba8_uint:
        case ALMetalPixelFormat::rg16_float:
        case ALMetalPixelFormat::rg16_sint:
        case ALMetalPixelFormat::rg16_uint:
        case ALMetalPixelFormat::r32_float:
        case ALMetalPixelFormat::r32_sint:
        case ALMetalPixelFormat::r32_uint:
        case ALMetalPixelFormat::rgb10a2_unorm:
        case ALMetalPixelFormat::rgb10a2_uint:
        case ALMetalPixelFormat::rg11b10_float:
        case ALMetalPixelFormat::depth32_float:
            return 4;
        case ALMetalPixelFormat::rgba16_unorm:
        case ALMetalPixelFormat::rgba16_float:
        case ALMetalPixelFormat::rgba16_sint:
        case ALMetalPixelFormat::rgba16_uint:
        case ALMetalPixelFormat::rg32_float:
        case ALMetalPixelFormat::rg32_sint:
        case ALMetalPixelFormat::rg32_uint:
        case ALMetalPixelFormat::depth32_float_stencil8:
            return 8;
        case ALMetalPixelFormat::rgba32_float:
        case ALMetalPixelFormat::rgba32_sint:
        case ALMetalPixelFormat::rgba32_uint:
            return 16;
        default:
            return 0;
    }
}

U64 mip_pixels(U32 width, U32 height, U32 levels)
{
    U64 pixels = 0;
    for (U32 level = 0; level < levels; ++level)
    {
        pixels += static_cast<U64>(width) * height;
        width = llmax(1u, width >> 1u);
        height = llmax(1u, height >> 1u);
    }
    return pixels;
}

LLRenderTarget::eMipGeneration mip_generation(ALRenderTargetDesc::Mips mips)
{
    switch (mips)
    {
        case ALRenderTargetDesc::Mips::Auto:   return LLRenderTarget::MIPS_AUTO;
        case ALRenderTargetDesc::Mips::Manual: return LLRenderTarget::MIPS_MANUAL;
        case ALRenderTargetDesc::Mips::None:   return LLRenderTarget::MIPS_NONE;
    }
    return LLRenderTarget::MIPS_NONE;
}

ALRenderTargetDesc::Mips desc_mips(LLRenderTarget::eMipGeneration mips)
{
    switch (mips)
    {
        case LLRenderTarget::MIPS_AUTO:   return ALRenderTargetDesc::Mips::Auto;
        case LLRenderTarget::MIPS_MANUAL: return ALRenderTargetDesc::Mips::Manual;
        case LLRenderTarget::MIPS_NONE:   return ALRenderTargetDesc::Mips::None;
    }
    return ALRenderTargetDesc::Mips::None;
}

ALRenderTargetDesc held_to_device(ALRenderTargetDesc desc)
{
    if (const ALMetalDevice* device = LLImageGL::getMetalDevice())
    {
        const U32 limit = device->info().max_texture_2d_dimension;
        if (limit)
        {
            desc.width = llmin(desc.width, limit);
            desc.height = llmin(desc.height, limit);
        }
    }
    return desc;
}

const char* unbuildable(const ALRenderTargetDesc& desc, bool depth_shared)
{
    if (!desc.width || !desc.height)
    {
        return "it has no size";
    }
    const U32 colors = desc.colorCount();
    for (U32 index = colors; index < ALRenderTargetDesc::MAX_COLOR; ++index)
    {
        if (desc.color[index])
        {
            return "a colour format follows a zero in its colour list";
        }
    }
    if (!colors && desc.depth == ALRenderTargetDesc::Depth::None && !depth_shared)
    {
        return "it has no attachment";
    }
    if (desc.stencil && desc.depth == ALRenderTargetDesc::Depth::None)
    {
        return "it has stencil without depth";
    }
    if (desc.mips == ALRenderTargetDesc::Mips::Auto && !colors)
    {
        return "automatic mip generation has no colour attachment";
    }
    if (desc.mips == ALRenderTargetDesc::Mips::Auto &&
        LLImageGL::isIntegerFormat(desc.color[0]))
    {
        return "automatic mip generation cannot filter an integer attachment";
    }
    for (U32 index = 0; index < colors; ++index)
    {
        if (metal_color_format(desc.color[index]) == ALMetalPixelFormat::invalid)
        {
            return "a colour format has no native Metal representation";
        }
    }
    return nullptr;
}

const char* unshareable(const ALRenderTargetDesc& desc,
                        const LLRenderTarget& sharer,
                        const LLRenderTarget& owner)
{
    if (&sharer == &owner)
    {
        return "it would share its own depth";
    }
    if (!owner.ownsDepthAttachment())
    {
        return "the target it shares depth from has none of its own";
    }
    if (desc.depth != ALRenderTargetDesc::Depth::None || desc.stencil)
    {
        return "it describes depth of its own";
    }
    if (desc.width != owner.getWidth() || desc.height != owner.getHeight())
    {
        return "its size differs from the depth it shares";
    }
    return nullptr;
}

bool whole_surface(const LLRenderTarget& target,
                   S32 x0, S32 y0, S32 x1, S32 y1)
{
    return x0 == 0 && y0 == 0 &&
           x1 == static_cast<S32>(target.getWidth()) &&
           y1 == static_cast<S32>(target.getHeight());
}
}

LLRenderTarget::LLRenderTarget()
: mResX(0),
  mResY(0),
  mUseDepth(false),
  mStencil(false),
  mDepthFormat(DEPTH_FMT_24),
  mUsage(ALTextureSlot::TT_TEXTURE)
{
}

LLRenderTarget::~LLRenderTarget()
{
    release();
}

void LLRenderTarget::resize(U32 resx, U32 resy)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    llassert(!isBoundInStack());
    if (resx == mResX && resy == mResY)
    {
        return;
    }
    if (sharesDepth())
    {
        LL_WARNS_ONCE("RenderTarget")
            << "Refusing to resize a target with borrowed depth; rebuild it from its depth owner"
            << LL_ENDL;
        return;
    }
    ALRenderTargetDesc wanted = desc();
    wanted.width = resx;
    wanted.height = resy;
    if (ensure(wanted) == EnsureResult::Failed)
    {
        LL_WARNS("RenderTarget") << "Could not resize native target '" << mLabel << "'" << LL_ENDL;
    }
}

bool LLRenderTarget::allocate(U32 resx, U32 resy, U32 color_fmt,
                              bool depth, bool stencil,
                              ALTextureSlot::eTextureType usage,
                              eMipGeneration generateMipMaps,
                              eDepthFormat depth_fmt)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    llassert(usage == ALTextureSlot::TT_TEXTURE);
    llassert(!isBoundInStack());
    llassert(!stencil || depth);

    ALRenderTargetDesc wanted;
    wanted.width = resx;
    wanted.height = resy;
    wanted.color[0] = color_fmt;
    wanted.depth = depth ? (depth_fmt == DEPTH_FMT_32F
        ? ALRenderTargetDesc::Depth::D32F : ALRenderTargetDesc::Depth::D24)
        : ALRenderTargetDesc::Depth::None;
    wanted.stencil = depth && stencil;
    wanted.mips = desc_mips(generateMipMaps);
    wanted = held_to_device(wanted);

    if (const char* reason = unbuildable(wanted, false))
    {
        LL_WARNS("RenderTarget") << "Cannot allocate native target '" << mLabel
                                  << "': " << reason << LL_ENDL;
        release();
        return false;
    }
    return build(wanted);
}

U64 LLRenderTarget::bytesFor(const ALRenderTargetDesc& description)
{
    const U32 levels = description.mips == ALRenderTargetDesc::Mips::None
        ? 1u : static_cast<U32>(std::bit_width(llmax(description.width,
                                                     description.height)));
    U64 bytes = 0;
    for (U32 index = 0; index < description.colorCount(); ++index)
    {
        bytes += mip_pixels(description.width, description.height, levels) *
                 metal_bytes_per_pixel(metal_color_format(description.color[index]));
    }
    if (description.depth != ALRenderTargetDesc::Depth::None)
    {
        bytes += static_cast<U64>(description.width) * description.height *
                 (description.stencil ? 8u : 4u);
    }
    return bytes;
}

ALRenderTargetDesc LLRenderTarget::desc() const
{
    if (!mMetalTarget || mExternalColor)
    {
        return {};
    }
    ALRenderTargetDesc result{
        .width = mResX,
        .height = mResY,
        .mips = desc_mips(mGenerateMipMaps),
    };
    for (U32 index = 0; index < mInternalFormat.size() &&
                        index < ALRenderTargetDesc::MAX_COLOR; ++index)
    {
        result.color[index] = mInternalFormat[index];
    }
    if (mOwnDepth)
    {
        result.depth = mDepthFormat == DEPTH_FMT_32F
            ? ALRenderTargetDesc::Depth::D32F
            : ALRenderTargetDesc::Depth::D24;
        result.stencil = mStencil;
    }
    return result;
}

LLRenderTarget::EnsureResult LLRenderTarget::ensure(const ALRenderTargetDesc& wanted)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    llassert(!isBoundInStack());
    const ALRenderTargetDesc held = held_to_device(wanted);
    if (const char* reason = unbuildable(held, false))
    {
        LL_WARNS("RenderTarget") << "Target '" << mLabel
                                  << "' cannot be built: " << reason << LL_ENDL;
        release();
        return EnsureResult::Failed;
    }
    if (!sharesDepth() && !mExternalColor && held == desc())
    {
        return EnsureResult::Kept;
    }
    return build(held) ? EnsureResult::Built : EnsureResult::Failed;
}

LLRenderTarget::EnsureResult LLRenderTarget::ensure(
    const ALRenderTargetDesc& wanted, const LLRenderTarget& depth_from)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    llassert(!isBoundInStack());
    const ALRenderTargetDesc held = held_to_device(wanted);
    const char* reason = unbuildable(held, true);
    if (!reason)
    {
        reason = unshareable(held, *this, depth_from);
    }
    if (reason)
    {
        LL_WARNS("RenderTarget") << "Target '" << mLabel
                                  << "' cannot share depth: " << reason << LL_ENDL;
        release();
        return EnsureResult::Failed;
    }

    bool built = false;
    if (mExternalColor || held != desc())
    {
        if (!build(held))
        {
            return EnsureResult::Failed;
        }
        built = true;
    }
    if (!built && sharesDepth() && mSharedDepthOwner == &depth_from &&
        mSharedDepthSerial == depth_from.getDepthSerial())
    {
        return EnsureResult::Kept;
    }
    attachSharedDepth(depth_from);
    return mMetalTarget ? EnsureResult::Built : EnsureResult::Failed;
}

bool LLRenderTarget::build(const ALRenderTargetDesc& description)
{
    release();
    mResX = description.width;
    mResY = description.height;
    mUsage = ALTextureSlot::TT_TEXTURE;
    mGenerateMipMaps = mip_generation(description.mips);
    mMipLevels = mGenerateMipMaps == MIPS_NONE ? 1u
        : static_cast<U32>(std::bit_width(llmax(mResX, mResY)));
    for (U32 index = 0; index < description.colorCount(); ++index)
    {
        mInternalFormat.push_back(description.color[index]);
    }
    mUseDepth = description.depth != ALRenderTargetDesc::Depth::None;
    mOwnDepth = mUseDepth;
    mStencil = description.stencil;
    mDepthFormat = description.depth == ALRenderTargetDesc::Depth::D32F
        ? DEPTH_FMT_32F : DEPTH_FMT_24;
    if (!rebuildMetalTarget())
    {
        release();
        return false;
    }
    return true;
}

bool LLRenderTarget::rebuildMetalTarget()
{
    ALMetalDevice* device = LLImageGL::getMetalDevice();
    if (!device || (!mExternalColor && mInternalFormat.empty() && !mUseDepth))
    {
        LL_WARNS("RenderTarget") << "Native target '" << mLabel
                                  << "' has no Metal device or attachment" << LL_ENDL;
        return false;
    }

    ALMetalRenderTargetDescriptor native;
    native.width = mResX;
    native.height = mResY;
    native.mipmap_level_count = mExternalColor
        ? mExternalColor->descriptor().mipmap_level_count : llmax(1u, mMipLevels);
    native.color_attachment_count = getNumTextures();
    native.label = mLabel.empty() ? "Alchemy native render target" : mLabel;
    for (U32 index = 0; index < native.color_attachment_count; ++index)
    {
        const U32 format = getInternalFormat(index);
        native.colors[index].format = metal_color_format(format);
        native.colors[index].shader_read = true;
        native.colors[index].shader_write = ALImageSlot::isImageFormat(format);
        native.colors[index].texture = index == 0 ? mExternalColor : nullptr;
        native.colors[index].label = native.label + ".color" + std::to_string(index);
        if (native.colors[index].format == ALMetalPixelFormat::invalid)
        {
            LL_WARNS("RenderTarget") << "Target '" << mLabel
                                      << "' has unsupported colour format 0x"
                                      << std::hex << format << std::dec << LL_ENDL;
            return false;
        }
    }
    if (mUseDepth)
    {
        native.depth_stencil_format = mStencil
            ? ALMetalPixelFormat::depth32_float_stencil8
            : ALMetalPixelFormat::depth32_float;
        native.depth_stencil_shader_read = true;
        if (!mOwnDepth)
        {
            native.depth_stencil_texture = mSharedDepthOwner
                ? mSharedDepthOwner->getMetalDepthTexture() : nullptr;
            if (!native.depth_stencil_texture)
            {
                LL_WARNS("RenderTarget") << "Target '" << mLabel
                                          << "' has no native shared depth resource"
                                          << LL_ENDL;
                return false;
            }
        }
    }

    std::string error;
    auto replacement = ALMetalRenderTarget::create(*device, native, &error);
    if (!replacement)
    {
        LL_WARNS("RenderTarget") << "Could not build target '" << mLabel
                                  << "': " << error << LL_ENDL;
        return false;
    }

    if (mAllocatedBytes <= sBytesAllocated)
    {
        sBytesAllocated -= static_cast<U32>(mAllocatedBytes);
    }
    else
    {
        sBytesAllocated = 0;
    }
    mMetalTarget = std::move(replacement);
    mAllocatedBytes = allocatedBytes();
    const U64 available = std::numeric_limits<U32>::max() - sBytesAllocated;
    sBytesAllocated += static_cast<U32>(llmin(mAllocatedBytes, available));
    if (mOwnDepth)
    {
        mDepthSerial = ++sLastDepthSerial;
    }
    return true;
}

U64 LLRenderTarget::allocatedBytes() const
{
    if (mExternalColor)
    {
        return mOwnDepth ? static_cast<U64>(mResX) * mResY *
            (mStencil ? 8u : 4u) : 0u;
    }
    return bytesFor(desc());
}

void LLRenderTarget::setColorAttachment(LLImageGL* image)
{
    LL_PROFILE_ZONE_SCOPED;
    llassert(image);
    llassert(!isBoundInStack());
    release();
    if (!image || !image->getMetalTexture())
    {
        LL_WARNS("RenderTarget")
            << "External render attachment requires the LLImageGL's real Metal texture"
            << LL_ENDL;
        return;
    }
    mResX = image->getWidth();
    mResY = image->getHeight();
    mUsage = image->getTarget();
    mExternalColor = image->getMetalTexture();
    mExternalFormat = static_cast<U32>(image->getStorageInternalFormat());
    mMipLevels = mExternalColor->descriptor().mipmap_level_count;
    if (!rebuildMetalTarget())
    {
        release();
    }
}

void LLRenderTarget::releaseColorAttachment()
{
    llassert(!isBoundInStack());
    if (!mExternalColor)
    {
        LL_WARNS("RenderTarget") << "Target '" << mLabel
                                  << "' has no borrowed colour attachment" << LL_ENDL;
        return;
    }
    release();
}

bool LLRenderTarget::addColorAttachment(U32 color_fmt)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    llassert(!isBoundInStack());
    if (mExternalColor || mInternalFormat.size() >= ALRenderTargetDesc::MAX_COLOR ||
        metal_color_format(color_fmt) == ALMetalPixelFormat::invalid)
    {
        LL_WARNS("RenderTarget") << "Cannot add colour attachment 0x"
                                  << std::hex << color_fmt << std::dec
                                  << " to target '" << mLabel << "'" << LL_ENDL;
        return false;
    }
    mInternalFormat.push_back(color_fmt);
    if (!rebuildMetalTarget())
    {
        mInternalFormat.pop_back();
        return false;
    }
    return true;
}

bool LLRenderTarget::allocateDepth()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    llassert(!isBoundInStack());
    if (!mResX || !mResY || mExternalColor)
    {
        return false;
    }
    mUseDepth = true;
    mOwnDepth = true;
    mSharedDepthOwner = nullptr;
    mSharedDepthSerial = 0;
    if (!rebuildMetalTarget())
    {
        mUseDepth = false;
        mOwnDepth = false;
        return false;
    }
    return true;
}

void LLRenderTarget::shareDepthBuffer(LLRenderTarget& target)
{
    target.attachSharedDepth(*this);
    if (!target.mMetalTarget)
    {
        LL_WARNS("RenderTarget") << "Could not attach shared native depth" << LL_ENDL;
    }
}

void LLRenderTarget::attachSharedDepth(const LLRenderTarget& owner)
{
    llassert(!isBoundInStack());
    if (&owner == this || !owner.ownsDepthAttachment() ||
        owner.getWidth() != mResX || owner.getHeight() != mResY)
    {
        LL_WARNS("RenderTarget") << "Invalid native depth-sharing request" << LL_ENDL;
        release();
        return;
    }
    mUseDepth = true;
    mOwnDepth = false;
    mStencil = owner.mStencil;
    mDepthFormat = owner.mDepthFormat;
    mSharedDepthOwner = &owner;
    mSharedDepthSerial = owner.mDepthSerial;
    mDepthSerial = 0;
    if (!rebuildMetalTarget())
    {
        release();
    }
}

void LLRenderTarget::setLabel(std::string_view label)
{
    mLabel.assign(label);
}

void LLRenderTarget::release()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    llassert(!isBoundInStack());
    if (mAllocatedBytes <= sBytesAllocated)
    {
        sBytesAllocated -= static_cast<U32>(mAllocatedBytes);
    }
    else
    {
        sBytesAllocated = 0;
    }
    mAllocatedBytes = 0;
    mMetalTarget.reset();
    mExternalColor = nullptr;
    mExternalFormat = 0;
    mInternalFormat.clear();
    mSharedDepthOwner = nullptr;
    mDepthSerial = 0;
    mSharedDepthSerial = 0;
    mUseDepth = false;
    mOwnDepth = false;
    mStencil = false;
    mDepthFormat = DEPTH_FMT_24;
    mGenerateMipMaps = MIPS_NONE;
    mMipLevels = 0;
    mDrawBuffers.fill(-1);
    mPreviousRT = nullptr;
    mResX = 0;
    mResY = 0;
}

void LLRenderTarget::bindTarget()
{
    LL_PROFILE_GPU_ZONE("bindTarget");
    gGL.flush();
    llassert(mMetalTarget);
    llassert(!isBoundInStack());
    if (!mMetalTarget)
    {
        return;
    }
    mPreviousRT = sBoundTarget;
    sBoundTarget = this;
    sCurFBO = 0;
    sCurResX = mResX;
    sCurResY = mResY;
    for (U32 index = 0; index < mDrawBuffers.size(); ++index)
    {
        mDrawBuffers[index] = index < getNumTextures()
            ? static_cast<S8>(index) : static_cast<S8>(-1);
    }
    gGL.setViewport(0, 0, mResX, mResY);
}

bool LLRenderTarget::encodeClear(U32 mask, const LLColor4& color,
                                 F64 depth, U32 stencil, bool all_colors)
{
    ALMetalDevice* device = LLImageGL::getMetalDevice();
    if (!device || !mMetalTarget || !mask)
    {
        return mask == 0;
    }
    ALMetalRenderPassConfig config;
    for (U32 index = 0; index < AL_METAL_MAX_COLOR_ATTACHMENTS; ++index)
    {
        config.colors[index].bind = false;
    }
    if (mask & GL_COLOR_BUFFER_BIT)
    {
        if (all_colors)
        {
            for (U32 attachment = 0; attachment < getNumTextures(); ++attachment)
            {
                auto& selected = config.colors[attachment];
                selected.bind = true;
                selected.load = ALMetalLoadAction::clear;
                selected.store = ALMetalStoreAction::store;
                selected.clear = {color.mV[0], color.mV[1],
                                  color.mV[2], color.mV[3]};
            }
        }
        else
        {
            for (S8 attachment : mDrawBuffers)
            {
                if (attachment < 0 || attachment >= static_cast<S8>(getNumTextures()))
                {
                    continue;
                }
                auto& selected = config.colors[static_cast<U32>(attachment)];
                selected.bind = true;
                selected.load = ALMetalLoadAction::clear;
                selected.store = ALMetalStoreAction::store;
                selected.clear = {color.mV[0], color.mV[1],
                                  color.mV[2], color.mV[3]};
            }
        }
    }
    config.bind_depth_stencil = mUseDepth &&
        (mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    config.depth_load = mask & GL_DEPTH_BUFFER_BIT
        ? ALMetalLoadAction::clear : ALMetalLoadAction::load;
    config.depth_store = ALMetalStoreAction::store;
    config.clear_depth = depth;
    config.stencil_load = mask & GL_STENCIL_BUFFER_BIT
        ? ALMetalLoadAction::clear : ALMetalLoadAction::load;
    config.stencil_store = ALMetalStoreAction::store;
    config.clear_stencil = stencil;

    std::string error;
    auto command = device->makeCommandBuffer("Alchemy render-target clear", &error);
    auto pass = command ? mMetalTarget->makeRenderPass(config, &error) : nullptr;
    auto encoder = pass ? ALMetalRenderEncoder::begin(
        *command, *pass, "Alchemy render-target clear", &error) : nullptr;
    if (!encoder || !encoder->end(&error) || !command->commit(&error))
    {
        LL_WARNS("RenderTarget") << "Could not clear target '" << mLabel
                                  << "': " << error << LL_ENDL;
        return false;
    }
    return true;
}

void LLRenderTarget::clear(U32 mask_in)
{
    LL_PROFILE_GPU_ZONE("clear");
    gGL.flush();
    llassert(sBoundTarget == this);
    U32 mask = GL_COLOR_BUFFER_BIT;
    if (mUseDepth) mask |= GL_DEPTH_BUFFER_BIT;
    if (mStencil) mask |= GL_STENCIL_BUFFER_BIT;
    mask &= mask_in;
    encodeClear(mask, gGL.getClearColor(), LLRender::sReverseZ ? 0.0 : 1.0, 0);
}

bool LLRenderTarget::clearAttachments(U32 mask, const LLColor4& color,
                                      F64 depth, U32 stencil)
{
    LL_PROFILE_GPU_ZONE("clear attachments");
    gGL.flush();
    U32 available = getNumTextures() ? GL_COLOR_BUFFER_BIT : 0;
    if (mUseDepth)
    {
        available |= GL_DEPTH_BUFFER_BIT;
        if (mStencil)
        {
            available |= GL_STENCIL_BUFFER_BIT;
        }
    }
    return encodeClear(mask & available, color, depth, stencil, true);
}

U32 LLRenderTarget::getTexture(U32 attachment) const
{
    if (attachment >= getNumTextures())
    {
        LL_WARNS("RenderTarget") << "Invalid attachment " << attachment
                                  << " for target '" << mLabel << "'" << LL_ENDL;
        llassert(false);
    }
    // Native resources deliberately have no synthetic GL name.
    return 0;
}

U32 LLRenderTarget::getNumTextures() const
{
    return mExternalColor ? 1u : static_cast<U32>(mInternalFormat.size());
}

U32 LLRenderTarget::getInternalFormat(U32 attachment) const
{
    if (attachment < mInternalFormat.size())
    {
        return mInternalFormat[attachment];
    }
    return attachment == 0 && mExternalColor ? mExternalFormat : 0;
}

bool LLRenderTarget::hasDepthAttachment() const
{
    return mMetalTarget && mMetalTarget->depthStencilTexture();
}

const ALMetalTexture* LLRenderTarget::getMetalColorTexture(U32 attachment) const
{
    return mMetalTarget ? mMetalTarget->colorTexture(attachment) : nullptr;
}

const ALMetalTexture* LLRenderTarget::getMetalDepthTexture() const
{
    return mMetalTarget ? mMetalTarget->depthStencilTexture() : nullptr;
}

void LLRenderTarget::bindTexture(U32 index, S32 channel, ALSampler key)
{
    const U32 sampler = key == ALSamplers::TargetDefault
        ? getDefaultColorSampler(index) : gGL.getSampler(key);
    ALMetalTexture* texture = const_cast<ALMetalTexture*>(getMetalColorTexture(index));
    ALSampler resolved_key = key == ALSamplers::TargetDefault
        ? (index == 0 && !LLImageGL::isIntegerFormat(getInternalFormat(index))
            ? ALSamplers::BilinearMirror : ALSamplers::PointMirror)
        : key;
    if (!gGL.getTextureSlot(channel)->bindNative(texture, mUsage, resolved_key))
    {
        LL_WARNS("RenderTarget") << "Could not bind native attachment " << index
                                  << " from target '" << mLabel << "' (sampler "
                                  << sampler << ")" << LL_ENDL;
    }
}

U32 LLRenderTarget::getDefaultColorSampler(U32 attachment) const
{
    const bool filtered = attachment == 0 &&
        !LLImageGL::isIntegerFormat(getInternalFormat(attachment));
    return gGL.getSampler(filtered ? ALSamplers::BilinearMirror
                                   : ALSamplers::PointMirror);
}

U32 LLRenderTarget::getDefaultDepthSampler() const
{
    return gGL.getSampler(ALSamplers::PointClamp);
}

void LLRenderTarget::flush()
{
    LL_PROFILE_GPU_ZONE("rt flush");
    gGL.flush();
    llassert(sBoundTarget == this);
    if (mGenerateMipMaps == MIPS_AUTO)
    {
        ALMetalDevice* device = LLImageGL::getMetalDevice();
        ALMetalTexture* texture = const_cast<ALMetalTexture*>(getMetalColorTexture(0));
        std::string error;
        auto command = device ? device->makeCommandBuffer(
            "Alchemy render-target mip generation", &error) : nullptr;
        if (!command || !texture || !texture->generateMipmaps(*command, &error) ||
            !command->commit(&error))
        {
            LL_WARNS("RenderTarget") << "Could not generate target mips: "
                                      << error << LL_ENDL;
        }
    }

    LLRenderTarget* previous = mPreviousRT;
    mPreviousRT = nullptr;
    sBoundTarget = previous;
    sCurFBO = 0;
    if (previous)
    {
        sCurResX = previous->mResX;
        sCurResY = previous->mResY;
        gGL.setViewport(0, 0, sCurResX, sCurResY);
    }
    else
    {
        sCurResX = gGLViewport[2];
        sCurResY = gGLViewport[3];
        gGL.setViewport(gGLViewport[0], gGLViewport[1],
                        gGLViewport[2], gGLViewport[3]);
    }
}

void LLRenderTarget::copyContents(LLRenderTarget& source,
                                  S32 srcX0, S32 srcY0, S32 srcX1, S32 srcY1,
                                  S32 dstX0, S32 dstY0, S32 dstX1, S32 dstY1,
                                  U32 mask, U32 filter)
{
    LL_PROFILE_GPU_ZONE("LLRenderTarget::copyContents");
    gGL.flush();
    ALMetalDevice* device = LLImageGL::getMetalDevice();
    if (!device || !mMetalTarget || !source.mMetalTarget)
    {
        LL_WARNS("RenderTarget") << "Native target copy has no source or destination"
                                  << LL_ENDL;
        return;
    }
    std::string error;
    auto command = device->makeCommandBuffer("Alchemy render-target copy", &error);
    bool encoded = false;
    bool succeeded = true;

    if (command && (mask & GL_COLOR_BUFFER_BIT))
    {
        const ALMetalTexture* source_color = source.getMetalColorTexture(0);
        const ALMetalTexture* destination_color = getMetalColorTexture(0);
        const bool same_extent = (srcX1 - srcX0) == (dstX1 - dstX0) &&
                                 (srcY1 - srcY0) == (dstY1 - dstY0);
        if (source_color && destination_color && same_extent)
        {
            ALMetalTextureCopy copy;
            copy.source_region = {
                static_cast<U32>(llmax(0, srcX0)),
                static_cast<U32>(llmax(0, srcY0)),
                static_cast<U32>(llmax(0, srcX1 - srcX0)),
                static_cast<U32>(llmax(0, srcY1 - srcY0)),
            };
            copy.destination_x = static_cast<U32>(llmax(0, dstX0));
            copy.destination_y = static_cast<U32>(llmax(0, dstY0));
            const bool copied = mMetalTarget->copyColorFrom(
                *command, 0, *source_color, copy, &error);
            encoded |= copied;
            succeeded &= copied;
        }
        else if (source_color && destination_color &&
                 whole_surface(source, srcX0, srcY0, srcX1, srcY1) &&
                 whole_surface(*this, dstX0, dstY0, dstX1, dstY1) &&
                 (filter == GL_LINEAR || filter == GL_NEAREST))
        {
            auto resampler = ALMetalTextureResampler::create(*device, &error);
            const bool copied = resampler && resampler->encode(
                *command, *source_color, *destination_color, &error);
            encoded |= copied;
            succeeded &= copied;
        }
        else
        {
            error = "native colour copy requires compatible exact regions or full-surface resampling";
            succeeded = false;
        }
    }

    if (command && (mask & GL_DEPTH_BUFFER_BIT))
    {
        const ALMetalTexture* source_depth = source.getMetalDepthTexture();
        const bool same_extent = (srcX1 - srcX0) == (dstX1 - dstX0) &&
                                 (srcY1 - srcY0) == (dstY1 - dstY0);
        if (!source_depth || !getMetalDepthTexture() || !same_extent)
        {
            error = "native depth copy requires matching depth attachments and extents";
            succeeded = false;
        }
        else
        {
            ALMetalTextureCopy copy;
            copy.source_region = {
                static_cast<U32>(llmax(0, srcX0)),
                static_cast<U32>(llmax(0, srcY0)),
                static_cast<U32>(llmax(0, srcX1 - srcX0)),
                static_cast<U32>(llmax(0, srcY1 - srcY0)),
            };
            copy.destination_x = static_cast<U32>(llmax(0, dstX0));
            copy.destination_y = static_cast<U32>(llmax(0, dstY0));
            const bool copied = mMetalTarget->copyDepthStencilFrom(
                *command, *source_depth, copy, &error);
            encoded |= copied;
            succeeded &= copied;
        }
    }

    if (!command || !encoded || !succeeded || !command->commit(&error))
    {
        LL_WARNS("RenderTarget") << "Could not copy native target contents: "
                                  << error << LL_ENDL;
    }
}

void LLRenderTarget::copyContentsToFramebuffer(
    LLRenderTarget&, S32, S32, S32, S32, S32, S32, S32, S32, U32, U32)
{
    LL_WARNS_ONCE("RenderTarget")
        << "Default-framebuffer copies are unavailable; presentation must consume a Metal texture"
        << LL_ENDL;
}

void LLRenderTarget::copyFromBound(S32 srcX0, S32 srcY0, S32 srcX1, S32 srcY1,
                                   S32 dstX0, S32 dstY0, S32 dstX1, S32 dstY1,
                                   U32 filter)
{
    if (!sBoundTarget || sBoundTarget == this)
    {
        LL_WARNS("RenderTarget")
            << "No distinct native target is bound as a copy source" << LL_ENDL;
        return;
    }
    copyContents(*sBoundTarget, srcX0, srcY0, srcX1, srcY1,
                 dstX0, dstY0, dstX1, dstY1, GL_COLOR_BUFFER_BIT, filter);
}

void LLRenderTarget::copyFromWindow(S32, S32, S32, S32)
{
    LL_WARNS_ONCE("RenderTarget")
        << "Window framebuffer copies have no native resource; use the retained Metal presentation target"
        << LL_ENDL;
}

bool LLRenderTarget::isComplete() const
{
    return mMetalTarget && (getNumTextures() || hasDepthAttachment());
}

void LLRenderTarget::getViewport(S32* viewport)
{
    viewport[0] = 0;
    viewport[1] = 0;
    viewport[2] = mResX;
    viewport[3] = mResY;
}

void LLRenderTarget::setDrawBuffers(U32 count)
{
    llassert(sBoundTarget == this);
    const U32 attachments = getNumTextures();
    const U32 wanted = count ? llmin(count, attachments) : attachments;
    gGL.flush();
    for (U32 index = 0; index < mDrawBuffers.size(); ++index)
    {
        mDrawBuffers[index] = index < wanted
            ? static_cast<S8>(index) : static_cast<S8>(-1);
    }
}

void LLRenderTarget::setDrawOutput(U32 output)
{
    llassert(sBoundTarget == this);
    llassert(getNumTextures());
    output = llmin(output, static_cast<U32>(mDrawBuffers.size() - 1));
    gGL.flush();
    mDrawBuffers.fill(-1);
    mDrawBuffers[output] = 0;
}

bool LLRenderTarget::isBoundInStack() const
{
    LLRenderTarget* current = sBoundTarget;
    while (current && current != this)
    {
        current = current->mPreviousRT;
    }
    return current == this;
}

void LLRenderTarget::swapFBORefs(LLRenderTarget& other)
{
    llassert(!isBoundInStack());
    llassert(!other.isBoundInStack());
    llassert(mResX == other.mResX && mResY == other.mResY);
    llassert(mInternalFormat == other.mInternalFormat);
    llassert(mUseDepth == other.mUseDepth);
    llassert(mOwnDepth == other.mOwnDepth);
    llassert(mStencil == other.mStencil);
    llassert(mDepthFormat == other.mDepthFormat);
    llassert(mGenerateMipMaps == other.mGenerateMipMaps);
    llassert(mMipLevels == other.mMipLevels);
    llassert(mUsage == other.mUsage);
    llassert(!mExternalColor && !other.mExternalColor);

    std::swap(mMetalTarget, other.mMetalTarget);
    std::swap(mSharedDepthOwner, other.mSharedDepthOwner);
    std::swap(mDepthSerial, other.mDepthSerial);
    std::swap(mSharedDepthSerial, other.mSharedDepthSerial);
    std::swap(mAllocatedBytes, other.mAllocatedBytes);
}
