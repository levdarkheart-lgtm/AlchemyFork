/**
 * @file llimagegl_upload_native.inl
 * @brief Private CPU staging for native LLImageGL uploads
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
// Included inside the caller's anonymous namespace. No graphics command or resource
// ownership lives here; converted bytes remain owned by the upload call until staging.
    enum class MetalRgbExpansion
    {
        none,
        rgb8,
        rgb16_float,
        rgb32_float,
    };

    enum class MetalScalarConversion
    {
        none,
        float32_to_float16,
    };

    struct MetalUploadFormat
    {
        ALMetalPixelFormat format = ALMetalPixelFormat::invalid;
        MetalRgbExpansion expansion = MetalRgbExpansion::none;
        MetalScalarConversion scalar_conversion =
            MetalScalarConversion::none;
    };

    MetalUploadFormat metal_upload_format(S32 internal_format, U32 primary_format, U32 type)
    {
        switch (primary_format)
        {
            case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
                return {ALMetalPixelFormat::bc1_rgba};
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
                return {ALMetalPixelFormat::bc1_rgba_srgb};
            case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
                return {ALMetalPixelFormat::bc2_rgba};
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
                return {ALMetalPixelFormat::bc2_rgba_srgb};
            case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                return {ALMetalPixelFormat::bc3_rgba};
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
                return {ALMetalPixelFormat::bc3_rgba_srgb};
            default:
                break;
        }

        if (primary_format == GL_BGRA &&
            (type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_INT_8_8_8_8_REV))
        {
            return {internal_format == GL_SRGB8_ALPHA8
                ? ALMetalPixelFormat::bgra8_unorm_srgb
                : ALMetalPixelFormat::bgra8_unorm,
                internal_format == GL_RGB8 && type == GL_UNSIGNED_BYTE
                    ? MetalRgbExpansion::rgb8 : MetalRgbExpansion::none};
        }

        switch (internal_format)
        {
            case GL_R8:                 return {ALMetalPixelFormat::r8_unorm};
            case GL_RG8:                return {ALMetalPixelFormat::rg8_unorm};
            case GL_RGB8:               return {ALMetalPixelFormat::rgba8_unorm, MetalRgbExpansion::rgb8};
            case GL_SRGB8:              return {ALMetalPixelFormat::rgba8_unorm_srgb, MetalRgbExpansion::rgb8};
            case GL_RGBA8:              return {ALMetalPixelFormat::rgba8_unorm};
            case GL_SRGB8_ALPHA8:       return {ALMetalPixelFormat::rgba8_unorm_srgb};
            case GL_R16F:
                return {ALMetalPixelFormat::r16_float,
                        MetalRgbExpansion::none,
                        type == GL_FLOAT
                            ? MetalScalarConversion::float32_to_float16
                            : MetalScalarConversion::none};
            case GL_RG16F:
                return {ALMetalPixelFormat::rg16_float,
                        MetalRgbExpansion::none,
                        type == GL_FLOAT
                            ? MetalScalarConversion::float32_to_float16
                            : MetalScalarConversion::none};
            case GL_RGB16F:
                return {ALMetalPixelFormat::rgba16_float,
                        MetalRgbExpansion::rgb16_float,
                        type == GL_FLOAT
                            ? MetalScalarConversion::float32_to_float16
                            : MetalScalarConversion::none};
            case GL_RGBA16F:
                return {ALMetalPixelFormat::rgba16_float,
                        MetalRgbExpansion::none,
                        type == GL_FLOAT
                            ? MetalScalarConversion::float32_to_float16
                            : MetalScalarConversion::none};
            case GL_RGBA16:             return {ALMetalPixelFormat::rgba16_unorm};
            case GL_R32F:               return {ALMetalPixelFormat::r32_float};
            case GL_RG32F:              return {ALMetalPixelFormat::rg32_float};
            case GL_RGB32F:             return {ALMetalPixelFormat::rgba32_float, MetalRgbExpansion::rgb32_float};
            case GL_RGBA32F:            return {ALMetalPixelFormat::rgba32_float};
            case GL_RGB10_A2:           return {ALMetalPixelFormat::rgb10a2_unorm};
            case GL_R11F_G11F_B10F:     return {ALMetalPixelFormat::rg11b10_float};
            case GL_R8I:                return {ALMetalPixelFormat::r8_sint};
            case GL_R8UI:               return {ALMetalPixelFormat::r8_uint};
            case GL_RG8I:               return {ALMetalPixelFormat::rg8_sint};
            case GL_RG8UI:              return {ALMetalPixelFormat::rg8_uint};
            case GL_RGBA8I:             return {ALMetalPixelFormat::rgba8_sint};
            case GL_RGBA8UI:            return {ALMetalPixelFormat::rgba8_uint};
            case GL_R16I:               return {ALMetalPixelFormat::r16_sint};
            case GL_R16UI:              return {ALMetalPixelFormat::r16_uint};
            case GL_RG16I:              return {ALMetalPixelFormat::rg16_sint};
            case GL_RG16UI:             return {ALMetalPixelFormat::rg16_uint};
            case GL_RGBA16I:            return {ALMetalPixelFormat::rgba16_sint};
            case GL_RGBA16UI:           return {ALMetalPixelFormat::rgba16_uint};
            case GL_R32I:               return {ALMetalPixelFormat::r32_sint};
            case GL_R32UI:              return {ALMetalPixelFormat::r32_uint};
            case GL_RG32I:              return {ALMetalPixelFormat::rg32_sint};
            case GL_RG32UI:             return {ALMetalPixelFormat::rg32_uint};
            case GL_RGBA32I:            return {ALMetalPixelFormat::rgba32_sint};
            case GL_RGBA32UI:           return {ALMetalPixelFormat::rgba32_uint};
            case GL_RGB10_A2UI:         return {ALMetalPixelFormat::rgb10a2_uint};
            default:                    return {};
        }
    }

    std::size_t metal_scalar_bytes(U32 type)
    {
        switch (type)
        {
            case GL_BYTE:
            case GL_UNSIGNED_BYTE:  return 1;
            case GL_SHORT:
            case GL_UNSIGNED_SHORT:
            case GL_HALF_FLOAT:     return 2;
            case GL_INT:
            case GL_UNSIGNED_INT:
            case GL_FLOAT:          return 4;
            default:                return 0;
        }
    }

    std::size_t metal_packed_bytes(U32 type)
    {
        switch (type)
        {
            case GL_UNSIGNED_SHORT_5_6_5:
            case GL_UNSIGNED_SHORT_5_6_5_REV:
            case GL_UNSIGNED_SHORT_4_4_4_4:
            case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            case GL_UNSIGNED_SHORT_5_5_5_1:
            case GL_UNSIGNED_SHORT_1_5_5_5_REV:
                return 2;
            case GL_UNSIGNED_INT_8_8_8_8:
            case GL_UNSIGNED_INT_8_8_8_8_REV:
            case GL_UNSIGNED_INT_10_10_10_2:
            case GL_UNSIGNED_INT_2_10_10_10_REV:
            case GL_UNSIGNED_INT_10F_11F_11F_REV:
            case GL_UNSIGNED_INT_5_9_9_9_REV:
                return 4;
            default:
                return 0;
        }
    }

    U16 metal_float_to_half(F32 value)
    {
        // Round to nearest, ties to even. Metal upload byte streams must
        // match their destination pixel format; OpenGL performs this
        // conversion implicitly when a GL_FLOAT source fills a *16F image.
        const U32 bits = std::bit_cast<U32>(value);
        const U32 sign = (bits >> 16) & 0x8000u;
        const U32 magnitude = bits & 0x7fffffffu;
        if (magnitude >= 0x7f800000u)
        {
            return static_cast<U16>(
                sign | 0x7c00u |
                (magnitude > 0x7f800000u ? 0x0200u : 0u));
        }
        if (magnitude >= 0x477ff000u)
        {
            return static_cast<U16>(sign | 0x7c00u);
        }
        if (magnitude < 0x38800000u)
        {
            if (magnitude < 0x33000000u)
            {
                return static_cast<U16>(sign);
            }
            const U32 exponent = magnitude >> 23;
            const U32 mantissa = (magnitude & 0x7fffffu) | 0x800000u;
            const U32 shift = 126u - exponent;
            U32 half = mantissa >> shift;
            const U32 rest = mantissa & ((1u << shift) - 1u);
            const U32 tie = 1u << (shift - 1u);
            if (rest > tie || (rest == tie && (half & 1u)))
            {
                ++half;
            }
            return static_cast<U16>(sign | half);
        }
        U32 half = (magnitude - 0x38000000u) >> 13;
        const U32 rest = magnitude & 0x1fffu;
        if (rest > 0x1000u || (rest == 0x1000u && (half & 1u)))
        {
            ++half;
        }
        return static_cast<U16>(sign | half);
    }

    bool prepare_metal_upload_pixels(const MetalUploadFormat& upload_format,
                                     U32 primary_format, U32 type, bool swap_bytes,
                                     U32 width, U32 height, const U8* pixels,
                                     std::size_t source_bytes_per_row,
                                     const U8*& upload_pixels,
                                     std::size_t& upload_bytes_per_row,
                                     std::vector<U8>& converted)
    {
        upload_pixels = pixels;
        upload_bytes_per_row = source_bytes_per_row;

        if (upload_format.scalar_conversion ==
            MetalScalarConversion::float32_to_float16)
        {
            const std::size_t source_components =
                LLImageGL::dataFormatComponents(primary_format);
            const bool expand_rgb =
                upload_format.expansion == MetalRgbExpansion::rgb16_float;
            const std::size_t destination_components =
                expand_rgb ? 4 : source_components;
            const std::size_t source_pixel_bytes =
                source_components * sizeof(F32);
            upload_bytes_per_row = static_cast<std::size_t>(width) *
                                   destination_components * sizeof(U16);
            converted.resize(upload_bytes_per_row * height);
            for (U32 row = 0; row < height; ++row)
            {
                const U8* source = pixels +
                    static_cast<std::size_t>(row) * source_bytes_per_row;
                U8* target = converted.data() +
                    static_cast<std::size_t>(row) * upload_bytes_per_row;
                for (U32 column = 0; column < width; ++column)
                {
                    for (std::size_t component = 0;
                         component < source_components; ++component)
                    {
                        F32 value = 0.f;
                        const U8* scalar =
                            source + component * sizeof(F32);
                        if (swap_bytes)
                        {
                            std::array<U8, sizeof(F32)> swapped{};
                            std::reverse_copy(
                                scalar, scalar + sizeof(F32), swapped.begin());
                            std::memcpy(&value, swapped.data(), sizeof(value));
                        }
                        else
                        {
                            std::memcpy(&value, scalar, sizeof(value));
                        }
                        const U16 half = metal_float_to_half(value);
                        std::memcpy(
                            target + component * sizeof(U16),
                            &half, sizeof(half));
                    }
                    if (expand_rgb)
                    {
                        const U16 one = 0x3c00u;
                        std::memcpy(
                            target + 3 * sizeof(U16), &one, sizeof(one));
                    }
                    source += source_pixel_bytes;
                    target += destination_components * sizeof(U16);
                }
            }
            upload_pixels = converted.data();
        }
        else if (upload_format.expansion != MetalRgbExpansion::none)
        {
            // Byte swapping has no effect on U8 components, including an RGB
            // storage expansion. Wider RGB expansions retain their existing guard.
            if (swap_bytes && !(upload_format.expansion == MetalRgbExpansion::rgb8 &&
                                type == GL_UNSIGNED_BYTE))
            {
                return false;
            }
            std::size_t source_pixel_bytes = 0;
            std::size_t destination_pixel_bytes = 0;
            switch (upload_format.expansion)
            {
                case MetalRgbExpansion::rgb8:
                    // Storage may discard an input alpha component: media can
                    // supply RGBA/BGRA U8 pixels for three-channel RGB8 storage.
                    source_pixel_bytes = type == GL_UNSIGNED_BYTE &&
                        (primary_format == GL_RGBA || primary_format == GL_BGRA) ? 4 : 3;
                    destination_pixel_bytes = 4;
                    break;
                case MetalRgbExpansion::rgb16_float:
                    source_pixel_bytes = 6;
                    destination_pixel_bytes = 8;
                    break;
                case MetalRgbExpansion::rgb32_float:
                    source_pixel_bytes = 12;
                    destination_pixel_bytes = 16;
                    break;
                case MetalRgbExpansion::none:
                    break;
            }
            upload_bytes_per_row = static_cast<std::size_t>(width) * destination_pixel_bytes;
            converted.resize(upload_bytes_per_row * height);
            for (U32 row = 0; row < height; ++row)
            {
                const U8* source = pixels + static_cast<std::size_t>(row) * source_bytes_per_row;
                U8* target = converted.data() + static_cast<std::size_t>(row) * upload_bytes_per_row;
                for (U32 column = 0; column < width; ++column)
                {
                    const std::size_t color_bytes =
                        upload_format.expansion == MetalRgbExpansion::rgb8 ? 3 : source_pixel_bytes;
                    std::memcpy(target, source, color_bytes);
                    if (upload_format.expansion == MetalRgbExpansion::rgb8)
                    {
                        target[3] = 0xff;
                    }
                    else if (upload_format.expansion == MetalRgbExpansion::rgb16_float)
                    {
                        const U16 one = 0x3c00;
                        std::memcpy(target + 6, &one, sizeof(one));
                    }
                    else
                    {
                        const F32 one = 1.0f;
                        std::memcpy(target + 12, &one, sizeof(one));
                    }
                    source += source_pixel_bytes;
                    target += destination_pixel_bytes;
                }
            }
            upload_pixels = converted.data();
        }
        else if (swap_bytes)
        {
            const std::size_t packed_bytes = metal_packed_bytes(type);
            const std::size_t scalar_bytes = packed_bytes ? packed_bytes : metal_scalar_bytes(type);
            const std::size_t scalars_per_pixel = packed_bytes ? 1 : LLImageGL::dataFormatComponents(primary_format);
            if (scalar_bytes <= 1)
            {
                // GL treats byte swapping as a no-op for one-byte elements.
            }
            else
            {
                const std::size_t source_pixel_bytes = scalar_bytes * scalars_per_pixel;
                upload_bytes_per_row = static_cast<std::size_t>(width) * source_pixel_bytes;
                converted.resize(upload_bytes_per_row * height);
                for (U32 row = 0; row < height; ++row)
                {
                    const U8* source = pixels + static_cast<std::size_t>(row) * source_bytes_per_row;
                    U8* target = converted.data() + static_cast<std::size_t>(row) * upload_bytes_per_row;
                    for (U32 column = 0; column < width; ++column)
                    {
                        for (std::size_t scalar = 0; scalar < scalars_per_pixel; ++scalar)
                        {
                            for (std::size_t byte = 0; byte < scalar_bytes; ++byte)
                            {
                                target[scalar * scalar_bytes + byte] =
                                    source[scalar * scalar_bytes + (scalar_bytes - byte - 1)];
                            }
                        }
                        source += source_pixel_bytes;
                        target += source_pixel_bytes;
                    }
                }
                upload_pixels = converted.data();
            }
        }

        return true;
    }
