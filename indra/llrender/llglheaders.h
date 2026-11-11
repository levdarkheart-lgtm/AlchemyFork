/**
 * @file llglheaders.h
 * @brief LLGL definitions
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

 // windows gl headers depend on things like APIENTRY, so include windows.
#include "llwin32headers.h"

// quotes so we get libraries/.../GL/ version
#include <GL/glcorearb.h>

#if LL_WINDOWS
#include <GL/wglext.h>
#endif

// GL_NVX_gpu_memory_info constants
#ifndef GL_NVX_gpu_memory_info
#define GL_NVX_gpu_memory_info
#define GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX          0x9047
#define GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    0x9048
#define GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049
#define GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX            0x904A
#define GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX            0x904B
#endif

// GL_ATI_meminfo constants
#ifndef GL_ATI_meminfo
#define GL_ATI_meminfo
#define GL_VBO_FREE_MEMORY_ATI                     0x87FB
#define GL_TEXTURE_FREE_MEMORY_ATI                 0x87FC
#define GL_RENDERBUFFER_FREE_MEMORY_ATI            0x87FD
#endif

#ifndef GL_EXT_texture_sRGB_decode
#define GL_EXT_texture_sRGB_decode 1
#define GL_TEXTURE_SRGB_DECODE_EXT        0x8A48
#define GL_DECODE_EXT                     0x8A49
#define GL_SKIP_DECODE_EXT                0x8A4A
#endif

// Not in glcorearb.h, though core contexts expose it where the hardware has it.
#ifndef GL_EXT_depth_bounds_test
#define GL_EXT_depth_bounds_test 1
#define GL_DEPTH_BOUNDS_TEST_EXT          0x8890
#define GL_DEPTH_BOUNDS_EXT               0x8891
typedef void (APIENTRYP PFNGLDEPTHBOUNDSEXTPROC) (GLdouble zmin, GLdouble zmax);
#endif

// GL_EXT_texture_sRGB constants
#ifndef GL_EXT_texture_sRGB
#define GL_EXT_texture_sRGB 1
#define GL_SRGB_EXT                       0x8C40
#define GL_SRGB8_EXT                      0x8C41
#define GL_SRGB_ALPHA_EXT                 0x8C42
#define GL_SRGB8_ALPHA8_EXT               0x8C43
#define GL_SLUMINANCE_ALPHA_EXT           0x8C44
#define GL_SLUMINANCE8_ALPHA8_EXT         0x8C45
#define GL_SLUMINANCE_EXT                 0x8C46
#define GL_SLUMINANCE8_EXT                0x8C47
#define GL_COMPRESSED_SRGB_EXT            0x8C48
#define GL_COMPRESSED_SRGB_ALPHA_EXT      0x8C49
#define GL_COMPRESSED_SLUMINANCE_EXT      0x8C4A
#define GL_COMPRESSED_SLUMINANCE_ALPHA_EXT 0x8C4B
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT  0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif /* GL_EXT_texture_sRGB */

// GL_ARB_vertex_buffer_object constants
#ifndef GL_ARB_vertex_buffer_object
#define GL_ARB_vertex_buffer_object 1
#define GL_STREAM_DRAW_ARB  0x88E0
#define GL_STREAM_READ_ARB  0x88E1
#define GL_STREAM_COPY_ARB  0x88E2
#define GL_STATIC_DRAW_ARB  0x88E4
#define GL_STATIC_READ_ARB  0x88E5
#define GL_STATIC_COPY_ARB  0x88E6
#define GL_DYNAMIC_DRAW_ARB 0x88E8
#define GL_DYNAMIC_READ_ARB 0x88E9
#define GL_DYNAMIC_COPY_ARB 0x88EA
#endif

// Deprecated OpenGL defines we still use
#define GL_COLOR_INDEX                    0x1900
#define GL_ALPHA                          0x1906
#define GL_ALPHA8                         0x803C
#define GL_LUMINANCE                      0x1909
#define GL_LUMINANCE_ALPHA                0x190A
#define GL_LUMINANCE8                     0x8040
#define GL_LUMINANCE8_ALPHA8              0x8045
#define GL_COMPRESSED_ALPHA               0x84E9
#define GL_COMPRESSED_LUMINANCE           0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA     0x84EB

// Remaining legacy calls are owned by the state/error helpers and the
// vertex/image consumers still being converted. Native device setup never
// resolves a GL entrypoint.
#if LL_WINDOWS
extern PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
extern PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
extern PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB;
extern PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
extern PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV;
extern PFNWGLDXCLOSEDEVICENVPROC wglDXCloseDeviceNV;
extern PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV;
extern PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV;
extern PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV;
extern PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV;
#endif

extern PFNGLTEXPARAMETERIPROC glTexParameteri;
extern PFNGLTEXPARAMETERIVPROC glTexParameteriv;
extern PFNGLDEPTHMASKPROC glDepthMask;
extern PFNGLDISABLEPROC glDisable;
extern PFNGLENABLEPROC glEnable;
extern PFNGLDEPTHFUNCPROC glDepthFunc;
extern PFNGLGETBOOLEANVPROC glGetBooleanv;
extern PFNGLGETERRORPROC glGetError;
extern PFNGLGETINTEGERVPROC glGetIntegerv;
extern PFNGLISENABLEDPROC glIsEnabled;
extern PFNGLDRAWARRAYSPROC glDrawArrays;
extern PFNGLDRAWELEMENTSPROC glDrawElements;
extern PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
extern PFNGLDELETETEXTURESPROC glDeleteTextures;
extern PFNGLGENTEXTURESPROC glGenTextures;
extern PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements;
extern PFNGLBINDBUFFERPROC glBindBuffer;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
extern PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer;
extern PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
extern PFNGLTEXSTORAGE2DPROC glTexStorage2D;
extern PFNGLOBJECTLABELPROC glObjectLabel;

#endif // LL_LLGLHEADERS_H
