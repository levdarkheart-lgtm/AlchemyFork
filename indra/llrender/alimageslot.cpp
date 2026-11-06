/**
 * @file alimageslot.cpp
 * @brief One image unit: the texture level a shader loads from and stores to, tracked like a texture slot.
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

#include "linden_common.h"

#include "alimageslot.h"

#include "llgl.h"
#include "llglheaders.h"
#include "llimagegl.h"
#include "llrender.h"
#include "llrendertarget.h"
#if LL_DARWIN
#include "almetalresource.h"
#endif

U32 ALImageSlot::sImageBinds = 0;

namespace
{
#if LL_DARWIN
    bool is_native_image_format(ALMetalPixelFormat format)
    {
        switch (format)
        {
        case ALMetalPixelFormat::r8_unorm:
        case ALMetalPixelFormat::r8_sint:
        case ALMetalPixelFormat::r8_uint:
        case ALMetalPixelFormat::rg8_unorm:
        case ALMetalPixelFormat::rg8_sint:
        case ALMetalPixelFormat::rg8_uint:
        case ALMetalPixelFormat::rgba8_unorm:
        case ALMetalPixelFormat::rgba8_sint:
        case ALMetalPixelFormat::rgba8_uint:
        case ALMetalPixelFormat::bgra8_unorm:
        case ALMetalPixelFormat::r16_unorm:
        case ALMetalPixelFormat::r16_float:
        case ALMetalPixelFormat::r16_sint:
        case ALMetalPixelFormat::r16_uint:
        case ALMetalPixelFormat::rg16_float:
        case ALMetalPixelFormat::rg16_sint:
        case ALMetalPixelFormat::rg16_uint:
        case ALMetalPixelFormat::rgba16_unorm:
        case ALMetalPixelFormat::rgba16_float:
        case ALMetalPixelFormat::rgba16_sint:
        case ALMetalPixelFormat::rgba16_uint:
        case ALMetalPixelFormat::r32_float:
        case ALMetalPixelFormat::r32_sint:
        case ALMetalPixelFormat::r32_uint:
        case ALMetalPixelFormat::rg32_float:
        case ALMetalPixelFormat::rg32_sint:
        case ALMetalPixelFormat::rg32_uint:
        case ALMetalPixelFormat::rgba32_float:
        case ALMetalPixelFormat::rgba32_sint:
        case ALMetalPixelFormat::rgba32_uint:
        case ALMetalPixelFormat::rgb10a2_unorm:
        case ALMetalPixelFormat::rgb10a2_uint:
        case ALMetalPixelFormat::rg11b10_float:
            return true;
        default:
            return false;
        }
    }

    U32 layer_count(const ALMetalTextureDescriptor& descriptor)
    {
        switch (descriptor.type)
        {
        case ALMetalTextureType::texture_2d_array:
            return descriptor.array_length;
        case ALMetalTextureType::texture_cube:
            return 6;
        case ALMetalTextureType::texture_cube_array:
            return descriptor.array_length * 6;
        default:
            return 1;
        }
    }

    bool supports_access(const ALMetalTextureDescriptor& descriptor,
                         ALImageSlot::eAccess access)
    {
        const bool readable = alMetalHasUsage(
            descriptor.usage, ALMetalTextureUsage::shader_read);
        const bool writable = alMetalHasUsage(
            descriptor.usage, ALMetalTextureUsage::shader_write);
        switch (access)
        {
        case ALImageSlot::READ_ONLY:  return readable;
        case ALImageSlot::WRITE_ONLY: return writable;
        case ALImageSlot::READ_WRITE: return readable && writable;
        }
        return false;
    }
#endif
}

ALImageSlot::ALImageSlot(S32 index)
    : mIndex(index)
{
    llassert_always(index < (S32)AL_NUM_IMAGE_SLOTS);
    reset();
}

void ALImageSlot::reset()
{
    mCurrTexture = 0;
    mCurrMetalTexture = nullptr;
    mCurrLevel   = 0;
    mCurrLayer   = 0;
    mCurrFormat  = GL_R8;
    mCurrAccess  = READ_ONLY;
    mCurrLayered = false;
}

// static
bool ALImageSlot::isImageFormat(U32 format)
{
    switch (format)
    {
    case GL_RGBA32F:
    case GL_RGBA16F:
    case GL_RG32F:
    case GL_RG16F:
    case GL_R11F_G11F_B10F:
    case GL_R32F:
    case GL_R16F:
    case GL_RGBA32UI:
    case GL_RGBA16UI:
    case GL_RGB10_A2UI:
    case GL_RGBA8UI:
    case GL_RG32UI:
    case GL_RG16UI:
    case GL_RG8UI:
    case GL_R32UI:
    case GL_R16UI:
    case GL_R8UI:
    case GL_RGBA32I:
    case GL_RGBA16I:
    case GL_RGBA8I:
    case GL_RG32I:
    case GL_RG16I:
    case GL_RG8I:
    case GL_R32I:
    case GL_R16I:
    case GL_R8I:
    case GL_RGBA16:
    case GL_RGB10_A2:
    case GL_RGBA8:
    case GL_RG16:
    case GL_RG8:
    case GL_R16:
    case GL_R8:
    case GL_RGBA16_SNORM:
    case GL_RGBA8_SNORM:
    case GL_RG16_SNORM:
    case GL_RG8_SNORM:
    case GL_R16_SNORM:
    case GL_R8_SNORM:
        return true;
    default:
        return false;
    }
}

bool ALImageSlot::bind(LLRenderTarget* target, U32 attachment, eAccess access, U32 level)
{
    if (!target || attachment >= target->getNumTextures())
    {
        LL_WARNS("ImageSlot") << "Image slot " << mIndex << ": no colour attachment " << attachment << " to bind"
                              << LL_ENDL;
        llassert(false);
        return false;
    }
    return bindNative(target->getMetalColorTexture(attachment), level, false, 0,
                      access, target->getInternalFormat(attachment));
}

bool ALImageSlot::bind(LLImageGL* image, eAccess access, U32 level)
{
#if LL_DARWIN
    if (!image || !image->getMetalTexture())
    {
        LL_WARNS("ImageSlot") << "Image slot " << mIndex
                              << ": no native texture to bind" << LL_ENDL;
        llassert(false);
        return false;
    }
    const ALMetalTextureType type = image->getMetalTexture()->descriptor().type;
    const bool layered = type == ALMetalTextureType::texture_2d_array ||
                         type == ALMetalTextureType::texture_cube ||
                         type == ALMetalTextureType::texture_cube_array;
    return bindNative(image->getMetalTexture(), level, layered, 0, access,
                      static_cast<U32>(image->getStorageInternalFormat()));
#else
    (void)image;
    (void)access;
    (void)level;
    return false;
#endif
}

bool ALImageSlot::bindNative(const ALMetalTexture* texture, U32 level, bool layered,
                             S32 layer, eAccess access, U32 format)
{
#if LL_DARWIN
    if (mIndex < 0 || !texture)
    {
        return false;
    }
    const ALMetalTextureDescriptor& descriptor = texture->descriptor();
    const U32 layers = layer_count(descriptor);
    const bool layered_type = descriptor.type == ALMetalTextureType::texture_2d_array ||
                              descriptor.type == ALMetalTextureType::texture_cube ||
                              descriptor.type == ALMetalTextureType::texture_cube_array;
    if (!is_native_image_format(descriptor.format) ||
        (format && !isImageFormat(format)) ||
        level >= descriptor.mipmap_level_count ||
        descriptor.type == ALMetalTextureType::texture_2d_multisample ||
        (layered && !layered_type) || (!layered && (layer < 0 || static_cast<U32>(layer) >= layers)) ||
        !supports_access(descriptor, access))
    {
        LL_WARNS_ONCE("ImageSlot") << "Image slot " << mIndex
                                   << ": native texture is incompatible with level/layer/access or format 0x"
                                   << std::hex << format << std::dec << LL_ENDL;
        return false;
    }

    if (mCurrMetalTexture == texture && mCurrLevel == level && mCurrLayered == layered && mCurrLayer == layer
        && mCurrAccess == access && mCurrFormat == format)
    {
        return true;
    }

    // What is batched was meant to see the unit as it was.
    gGL.flush();

    ++sImageBinds;

    mCurrTexture = 1u;
    mCurrMetalTexture = texture;
    mCurrLevel   = level;
    mCurrLayered = layered;
    mCurrLayer   = layer;
    mCurrAccess  = access;
    mCurrFormat  = format;
    return true;
#else
    (void)texture;
    (void)level;
    (void)layered;
    (void)layer;
    (void)access;
    (void)format;
    return false;
#endif
}

bool ALImageSlot::bindManual(U32 texture, U32 level, bool layered, S32 layer,
                             eAccess access, U32 format)
{
    (void)texture;
    (void)level;
    (void)layered;
    (void)layer;
    (void)access;
    (void)format;
    LL_WARNS_ONCE("ImageSlot") << "Integer image identities have no Metal resource owner"
                               << LL_ENDL;
    return false;
}

void ALImageSlot::unbind()
{
    if (mIndex < 0 || !mCurrMetalTexture)
    {
        return;
    }

    gGL.flush();

    ++sImageBinds;
    reset();
}

void ALImageSlot::refreshState()
{
    // Metal encoders bind every resource explicitly. There is no driver image
    // unit to re-issue after unrelated native or legacy state changes.
}
