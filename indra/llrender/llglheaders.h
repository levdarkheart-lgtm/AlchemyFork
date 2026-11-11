/**
 * @file llglheaders.h
 * @brief CPU format and logical render-state vocabulary
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

#ifndef LL_LLGLHEADERS_H
#define LL_LLGLHEADERS_H

// Numeric compatibility vocabulary for decoded pixels, material metadata and
// logical render state. Native Metal owners consume these values explicitly;
// this header declares no graphics entry points or driver capabilities.
//
// Preserve the original viewer/Khronos scalar ABI and exact token literals,
// including their C++ integer types. This is CPU data, not an OpenGL backend.
using GLboolean = unsigned char;
using GLenum = unsigned int;
using GLfloat = float;
using GLint = int;
using GLubyte = unsigned char;
using GLuint = unsigned int;

#define GL_ALPHA                                         0x1906
#define GL_ALPHA8                                        0x803C
#define GL_ALWAYS                                        0x0207
#define GL_ARRAY_BUFFER                                  0x8892
#define GL_BACK                                          0x0405
#define GL_BGRA                                          0x80E1
#define GL_BGRA_INTEGER                                  0x8D9B
#define GL_BGR_INTEGER                                   0x8D9A
#define GL_BLEND                                         0x0BE2
#define GL_BYTE                                          0x1400
#define GL_COLOR_BUFFER_BIT                              0x00004000
#define GL_COLOR_INDEX                                   0x1900
#define GL_COMPRESSED_ALPHA                              0x84E9
#define GL_COMPRESSED_LUMINANCE                          0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA                    0x84EB
#define GL_COMPRESSED_RED                                0x8225
#define GL_COMPRESSED_RG                                 0x8226
#define GL_COMPRESSED_RGB                                0x84ED
#define GL_COMPRESSED_RGBA                               0x84EE
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT                 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT                 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT                 0x83F3
#define GL_COMPRESSED_SRGB                               0x8C48
#define GL_COMPRESSED_SRGB_ALPHA                         0x8C49
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT           0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT           0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT           0x8C4F
#define GL_COMPUTE_SHADER                                0x91B9
#define GL_CULL_FACE                                     0x0B44
#define GL_DEPTH24_STENCIL8                              0x88F0
#define GL_DEPTH32F_STENCIL8                             0x8CAD
#define GL_DEPTH_BUFFER_BIT                              0x00000100
#define GL_DEPTH_COMPONENT                               0x1902
#define GL_DEPTH_COMPONENT24                             0x81A6
#define GL_DEPTH_COMPONENT32F                            0x8CAC
#define GL_DITHER                                        0x0BD0
#define GL_ELEMENT_ARRAY_BUFFER                          0x8893
#define GL_EQUAL                                         0x0202
#define GL_FALSE                                         0
#define GL_FILL                                          0x1B02
#define GL_FLOAT                                         0x1406
#define GL_FRAGMENT_SHADER                               0x8B30
#define GL_FRAMEBUFFER_SRGB                              0x8DB9
#define GL_FRONT                                         0x0404
#define GL_FRONT_AND_BACK                                0x0408
#define GL_FUNC_ADD                                      0x8006
#define GL_FUNC_REVERSE_SUBTRACT                         0x800B
#define GL_FUNC_SUBTRACT                                 0x800A
#define GL_GEQUAL                                        0x0206
#define GL_GREATER                                       0x0204
#define GL_HALF_FLOAT                                    0x140B
#define GL_INCR                                          0x1E02
#define GL_INT                                           0x1404
#define GL_KEEP                                          0x1E00
#define GL_LEQUAL                                        0x0203
#define GL_LESS                                          0x0201
#define GL_LINE                                          0x1B01
#define GL_LINEAR                                        0x2601
#define GL_LUMINANCE                                     0x1909
#define GL_LUMINANCE8                                    0x8040
#define GL_LUMINANCE8_ALPHA8                             0x8045
#define GL_LUMINANCE_ALPHA                               0x190A
#define GL_MAX                                           0x8008
#define GL_MIN                                           0x8007
#define GL_MULTISAMPLE                                   0x809D
#define GL_NEAREST                                       0x2600
#define GL_NEVER                                         0x0200
#define GL_NOTEQUAL                                      0x0205
#define GL_POINT                                         0x1B00
#define GL_POLYGON_OFFSET_FILL                           0x8037
#define GL_R11F_G11F_B10F                                0x8C3A
#define GL_R16                                           0x822A
#define GL_R16F                                          0x822D
#define GL_R16I                                          0x8233
#define GL_R16UI                                         0x8234
#define GL_R16_SNORM                                     0x8F98
#define GL_R32F                                          0x822E
#define GL_R32I                                          0x8235
#define GL_R32UI                                         0x8236
#define GL_R8                                            0x8229
#define GL_R8I                                           0x8231
#define GL_R8UI                                          0x8232
#define GL_R8_SNORM                                      0x8F94
#define GL_RED                                           0x1903
#define GL_RED_INTEGER                                   0x8D94
#define GL_REPLACE                                       0x1E01
#define GL_RG                                            0x8227
#define GL_RG16                                          0x822C
#define GL_RG16F                                         0x822F
#define GL_RG16I                                         0x8239
#define GL_RG16UI                                        0x823A
#define GL_RG16_SNORM                                    0x8F99
#define GL_RG32F                                         0x8230
#define GL_RG32I                                         0x823B
#define GL_RG32UI                                        0x823C
#define GL_RG8                                           0x822B
#define GL_RG8I                                          0x8237
#define GL_RG8UI                                         0x8238
#define GL_RG8_SNORM                                     0x8F95
#define GL_RGB                                           0x1907
#define GL_RGB10_A2                                      0x8059
#define GL_RGB10_A2UI                                    0x906F
#define GL_RGB16                                         0x8054
#define GL_RGB16F                                        0x881B
#define GL_RGB16I                                        0x8D89
#define GL_RGB16UI                                       0x8D77
#define GL_RGB32F                                        0x8815
#define GL_RGB32I                                        0x8D83
#define GL_RGB32UI                                       0x8D71
#define GL_RGB8                                          0x8051
#define GL_RGB8I                                         0x8D8F
#define GL_RGB8UI                                        0x8D7D
#define GL_RGBA                                          0x1908
#define GL_RGBA16                                        0x805B
#define GL_RGBA16F                                       0x881A
#define GL_RGBA16I                                       0x8D88
#define GL_RGBA16UI                                      0x8D76
#define GL_RGBA16_SNORM                                  0x8F9B
#define GL_RGBA32F                                       0x8814
#define GL_RGBA32I                                       0x8D82
#define GL_RGBA32UI                                      0x8D70
#define GL_RGBA8                                         0x8058
#define GL_RGBA8I                                        0x8D8E
#define GL_RGBA8UI                                       0x8D7C
#define GL_RGBA8_SNORM                                   0x8F97
#define GL_RGBA_INTEGER                                  0x8D99
#define GL_RGB_INTEGER                                   0x8D98
#define GL_RG_INTEGER                                    0x8228
#define GL_SCISSOR_TEST                                  0x0C11
#define GL_SHORT                                         0x1402
#define GL_SRGB                                          0x8C40
#define GL_SRGB8                                         0x8C41
#define GL_SRGB8_ALPHA8                                  0x8C43
#define GL_SRGB_ALPHA                                    0x8C42
#define GL_STENCIL_BUFFER_BIT                            0x00000400
#define GL_STENCIL_TEST                                  0x0B90
#define GL_TESS_CONTROL_SHADER                           0x8E88
#define GL_TESS_EVALUATION_SHADER                        0x8E87
#define GL_TEXTURE_2D                                    0x0DE1
#define GL_TEXTURE_2D_MULTISAMPLE                        0x9100
#define GL_TEXTURE_3D                                    0x806F
#define GL_TEXTURE_CUBE_MAP                              0x8513
#define GL_TEXTURE_CUBE_MAP_ARRAY                        0x9009
#define GL_TEXTURE_RECTANGLE                             0x84F5
#define GL_TRUE                                          1
#define GL_UNSIGNED_BYTE                                 0x1401
#define GL_UNSIGNED_INT                                  0x1405
#define GL_UNSIGNED_INT_10F_11F_11F_REV                  0x8C3B
#define GL_UNSIGNED_INT_10_10_10_2                       0x8036
#define GL_UNSIGNED_INT_2_10_10_10_REV                   0x8368
#define GL_UNSIGNED_INT_5_9_9_9_REV                      0x8C3E
#define GL_UNSIGNED_INT_8_8_8_8                          0x8035
#define GL_UNSIGNED_INT_8_8_8_8_REV                      0x8367
#define GL_UNSIGNED_SHORT                                0x1403
#define GL_UNSIGNED_SHORT_1_5_5_5_REV                    0x8366
#define GL_UNSIGNED_SHORT_4_4_4_4                        0x8033
#define GL_UNSIGNED_SHORT_4_4_4_4_REV                    0x8365
#define GL_UNSIGNED_SHORT_5_5_5_1                        0x8034
#define GL_UNSIGNED_SHORT_5_6_5                          0x8363
#define GL_UNSIGNED_SHORT_5_6_5_REV                      0x8364
#define GL_VERTEX_SHADER                                 0x8B31
#define GL_ZERO                                          0

#endif // LL_LLGLHEADERS_H
