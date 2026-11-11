/**
 * @file llgl.cpp
 * @brief LLGL implementation
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

// This file sets some global GL parameters, and implements some
// useful functions for GL operations.

#include "linden_common.h"

#include "boost/tokenizer.hpp"

#include "llsys.h"

#include "llgl.h"
#include "alrendercapabilities.h"
#include "llglstates.h"
#include "llrender.h"

#include "llerror.h"
#include "llerrorcontrol.h"
#include "llquaternion.h"
#include "llmath.h"
#include "m4math.h"
#include "llstring.h"

#include "llglheaders.h"

#if LL_DARWIN
#  define LL_GET_PROC_ADDRESS(func) nullptr
#elif LL_WINDOWS
#  include "llwindowwin32.h"
#  define LL_GET_PROC_ADDRESS(func) LLWindowWin32::getProcAddress(func)
#endif

#if LL_WINDOWS
#include "lldxhardware.h"
#endif

#if LL_LINUX
#include <sys/utsname.h>
#endif

#ifdef _DEBUG
//#define GL_STATE_VERIFY
#endif

bool gDebugSession = false;
bool gDebugGLSession = false;
bool gHeadlessClient = false;
bool gNonInteractive = false;
bool gGLActive = false;

namespace
{
    bool issue_opengl_state() noexcept
    {
        if (!gGLManager.mInited)
        {
            return false;
        }
#if LL_DARWIN
        return !gGL.isMetalUITextureCaptureAuthoritative();
#else
        return true;
#endif
    }
}


llofstream gFailLog;

void parse_glsl_version(S32& major, S32& minor);

void ll_init_fail_log(std::string filename)
{
    gFailLog.open(filename.c_str());
}


void ll_fail(std::string msg)
{

    if (gDebugSession)
    {
        std::vector<std::string> lines;

        gFailLog << LLError::utcTime() << " " << msg << std::endl;

        gFailLog << "Stack Trace:" << std::endl;

        for(size_t i = 0; i < lines.size(); ++i)
        {
            gFailLog << lines[i] << std::endl;
        }

        gFailLog << "End of Stack Trace." << std::endl << std::endl;

        gFailLog.flush();
    }
};

void ll_close_fail_log()
{
    gFailLog.close();
}


std::list<LLGLUpdate*> LLGLUpdate::sGLQ;

#if LL_WINDOWS
// WGL_ARB_pixel_format
PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB = nullptr;
PFNWGLGETPIXELFORMATATTRIBFVARBPROC wglGetPixelFormatAttribfvARB = nullptr;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = nullptr;

// WGL_ARB_create_context
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;

// WGL_AMD_gpu_association
PFNWGLGETGPUIDSAMDPROC                          wglGetGPUIDsAMD = nullptr;
PFNWGLGETGPUINFOAMDPROC                         wglGetGPUInfoAMD = nullptr;
PFNWGLGETCONTEXTGPUIDAMDPROC                    wglGetContextGPUIDAMD = nullptr;
PFNWGLCREATEASSOCIATEDCONTEXTAMDPROC            wglCreateAssociatedContextAMD = nullptr;
PFNWGLCREATEASSOCIATEDCONTEXTATTRIBSAMDPROC     wglCreateAssociatedContextAttribsAMD = nullptr;
PFNWGLDELETEASSOCIATEDCONTEXTAMDPROC            wglDeleteAssociatedContextAMD = nullptr;
PFNWGLMAKEASSOCIATEDCONTEXTCURRENTAMDPROC       wglMakeAssociatedContextCurrentAMD = nullptr;
PFNWGLGETCURRENTASSOCIATEDCONTEXTAMDPROC        wglGetCurrentAssociatedContextAMD = nullptr;
PFNWGLBLITCONTEXTFRAMEBUFFERAMDPROC             wglBlitContextFramebufferAMD = nullptr;

// WGL_EXT_swap_control
PFNWGLSWAPINTERVALEXTPROC    wglSwapIntervalEXT = nullptr;
PFNWGLGETSWAPINTERVALEXTPROC wglGetSwapIntervalEXT = nullptr;

// WGL_NV_DX_interop / interop2
PFNWGLDXOPENDEVICENVPROC       wglDXOpenDeviceNV = nullptr;
PFNWGLDXCLOSEDEVICENVPROC      wglDXCloseDeviceNV = nullptr;
PFNWGLDXREGISTEROBJECTNVPROC   wglDXRegisterObjectNV = nullptr;
PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV = nullptr;
PFNWGLDXLOCKOBJECTSNVPROC      wglDXLockObjectsNV = nullptr;
PFNWGLDXUNLOCKOBJECTSNVPROC    wglDXUnlockObjectsNV = nullptr;
#endif

#if LL_LINUX
// EGL_VERSION_1_0
PFNEGLQUERYSTRINGPROC eglQueryString = nullptr;

// EGL_KHR_image
PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = nullptr;
PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = nullptr;

// GL_OES_EGL_image
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = nullptr;
#endif

// GL_VERSION_1_0
PFNGLCLEARCOLORPROC                  glClearColor = nullptr;
PFNGLCLEARPROC                       glClear = nullptr;
PFNGLCOLORMASKPROC                   glColorMask = nullptr;
PFNGLBLENDFUNCPROC                   glBlendFunc = nullptr;
PFNGLLOGICOPPROC                     glLogicOp = nullptr;
PFNGLFRONTFACEPROC                   glFrontFace = nullptr;
PFNGLCULLFACEPROC                    glCullFace = nullptr;
PFNGLPOINTSIZEPROC                   glPointSize = nullptr;
PFNGLLINEWIDTHPROC                   glLineWidth = nullptr;
PFNGLPOLYGONMODEPROC                 glPolygonMode = nullptr;
PFNGLSCISSORPROC                     glScissor = nullptr;
PFNGLDRAWBUFFERPROC                  glDrawBuffer = nullptr;
PFNGLREADBUFFERPROC                  glReadBuffer = nullptr;
PFNGLENABLEPROC                      glEnable = nullptr;
PFNGLDISABLEPROC                     glDisable = nullptr;
PFNGLISENABLEDPROC                   glIsEnabled = nullptr;
PFNGLGETBOOLEANVPROC                 glGetBooleanv = nullptr;
PFNGLGETDOUBLEVPROC                  glGetDoublev = nullptr;
PFNGLGETFLOATVPROC                   glGetFloatv = nullptr;
PFNGLGETINTEGERVPROC                 glGetIntegerv = nullptr;
PFNGLGETERRORPROC                    glGetError = nullptr;
PFNGLGETSTRINGPROC                   glGetString = nullptr;
PFNGLFINISHPROC                      glFinish = nullptr;
PFNGLFLUSHPROC                       glFlush = nullptr;
PFNGLHINTPROC                        glHint = nullptr;
PFNGLCLEARDEPTHPROC                  glClearDepth = nullptr;
PFNGLDEPTHFUNCPROC                   glDepthFunc = nullptr;
PFNGLDEPTHMASKPROC                   glDepthMask = nullptr;
PFNGLDEPTHRANGEPROC                  glDepthRange = nullptr;
PFNGLVIEWPORTPROC                    glViewport = nullptr;
PFNGLPIXELSTOREFPROC                 glPixelStoref = nullptr;
PFNGLPIXELSTOREIPROC                 glPixelStorei = nullptr;
PFNGLREADPIXELSPROC                  glReadPixels = nullptr;
PFNGLSTENCILFUNCPROC                 glStencilFunc = nullptr;
PFNGLSTENCILMASKPROC                 glStencilMask = nullptr;
PFNGLSTENCILOPPROC                   glStencilOp = nullptr;
PFNGLCLEARSTENCILPROC                glClearStencil = nullptr;
PFNGLTEXPARAMETERFPROC               glTexParameterf = nullptr;
PFNGLTEXPARAMETERIPROC               glTexParameteri = nullptr;
PFNGLTEXPARAMETERFVPROC              glTexParameterfv = nullptr;
PFNGLTEXPARAMETERIVPROC              glTexParameteriv = nullptr;
PFNGLGETTEXPARAMETERFVPROC           glGetTexParameterfv = nullptr;
PFNGLGETTEXPARAMETERIVPROC           glGetTexParameteriv = nullptr;
PFNGLGETTEXLEVELPARAMETERFVPROC      glGetTexLevelParameterfv = nullptr;
PFNGLGETTEXLEVELPARAMETERIVPROC      glGetTexLevelParameteriv = nullptr;
PFNGLTEXIMAGE1DPROC                  glTexImage1D = nullptr;
PFNGLTEXIMAGE2DPROC                  glTexImage2D = nullptr;
PFNGLGETTEXIMAGEPROC                 glGetTexImage = nullptr;

// GL_VERSION_1_1
PFNGLDRAWARRAYSPROC               glDrawArrays        = nullptr;
PFNGLDRAWELEMENTSPROC             glDrawElements      = nullptr;
PFNGLGETPOINTERVPROC              glGetPointerv       = nullptr;
PFNGLPOLYGONOFFSETPROC            glPolygonOffset     = nullptr;
PFNGLCOPYTEXIMAGE1DPROC           glCopyTexImage1D    = nullptr;
PFNGLCOPYTEXIMAGE2DPROC           glCopyTexImage2D    = nullptr;
PFNGLCOPYTEXSUBIMAGE1DPROC        glCopyTexSubImage1D = nullptr;
PFNGLCOPYTEXSUBIMAGE2DPROC        glCopyTexSubImage2D = nullptr;
PFNGLTEXSUBIMAGE1DPROC            glTexSubImage1D  = nullptr;
PFNGLTEXSUBIMAGE2DPROC            glTexSubImage2D  = nullptr;
PFNGLBINDTEXTUREPROC              glBindTexture = nullptr;
PFNGLDELETETEXTURESPROC           glDeleteTextures = nullptr;
PFNGLGENTEXTURESPROC              glGenTextures = nullptr;
PFNGLISTEXTUREPROC                glIsTexture = nullptr;


// GL_VERSION_1_2
PFNGLDRAWRANGEELEMENTSPROC           glDrawRangeElements = nullptr;
PFNGLTEXIMAGE3DPROC                  glTexImage3D = nullptr;
PFNGLTEXSUBIMAGE3DPROC               glTexSubImage3D = nullptr;
PFNGLCOPYTEXSUBIMAGE3DPROC           glCopyTexSubImage3D = nullptr;

// GL_VERSION_1_3
PFNGLACTIVETEXTUREPROC               glActiveTexture = nullptr;
PFNGLSAMPLECOVERAGEPROC              glSampleCoverage = nullptr;
PFNGLCOMPRESSEDTEXIMAGE3DPROC        glCompressedTexImage3D = nullptr;
PFNGLCOMPRESSEDTEXIMAGE2DPROC        glCompressedTexImage2D = nullptr;
PFNGLCOMPRESSEDTEXIMAGE1DPROC        glCompressedTexImage1D = nullptr;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC     glCompressedTexSubImage3D = nullptr;
PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC     glCompressedTexSubImage2D = nullptr;
PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC     glCompressedTexSubImage1D = nullptr;
PFNGLGETCOMPRESSEDTEXIMAGEPROC       glGetCompressedTexImage = nullptr;

// GL_VERSION_1_4
PFNGLBLENDFUNCSEPARATEPROC       glBlendFuncSeparate = nullptr;
PFNGLMULTIDRAWARRAYSPROC         glMultiDrawArrays = nullptr;
PFNGLMULTIDRAWELEMENTSPROC       glMultiDrawElements = nullptr;
PFNGLPOINTPARAMETERFPROC         glPointParameterf = nullptr;
PFNGLPOINTPARAMETERFVPROC        glPointParameterfv = nullptr;
PFNGLPOINTPARAMETERIPROC         glPointParameteri = nullptr;
PFNGLPOINTPARAMETERIVPROC        glPointParameteriv = nullptr;

// GL_VERSION_1_5
PFNGLBINDBUFFERPROC              glBindBuffer = nullptr;
PFNGLDELETEBUFFERSPROC           glDeleteBuffers = nullptr;
PFNGLGENBUFFERSPROC              glGenBuffers = nullptr;
PFNGLISBUFFERPROC                glIsBuffer = nullptr;
PFNGLBUFFERDATAPROC              glBufferData = nullptr;
PFNGLBUFFERSUBDATAPROC           glBufferSubData = nullptr;
PFNGLGETBUFFERSUBDATAPROC        glGetBufferSubData = nullptr;
PFNGLMAPBUFFERPROC               glMapBuffer = nullptr;
PFNGLUNMAPBUFFERPROC             glUnmapBuffer = nullptr;
PFNGLGETBUFFERPARAMETERIVPROC    glGetBufferParameteriv = nullptr;
PFNGLGETBUFFERPOINTERVPROC       glGetBufferPointerv = nullptr;

// GL_VERSION_2_0
PFNGLBLENDEQUATIONSEPARATEPROC           glBlendEquationSeparate = nullptr;
PFNGLDRAWBUFFERSPROC                     glDrawBuffers = nullptr;
PFNGLSTENCILOPSEPARATEPROC               glStencilOpSeparate = nullptr;
PFNGLSTENCILFUNCSEPARATEPROC             glStencilFuncSeparate = nullptr;
PFNGLSTENCILMASKSEPARATEPROC             glStencilMaskSeparate = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC        glDisableVertexAttribArray = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC         glEnableVertexAttribArray = nullptr;
PFNGLGETVERTEXATTRIBDVPROC               glGetVertexAttribdv = nullptr;
PFNGLGETVERTEXATTRIBFVPROC               glGetVertexAttribfv = nullptr;
PFNGLGETVERTEXATTRIBIVPROC               glGetVertexAttribiv = nullptr;
PFNGLGETVERTEXATTRIBPOINTERVPROC         glGetVertexAttribPointerv = nullptr;
PFNGLVERTEXATTRIB1DPROC                  glVertexAttrib1d = nullptr;
PFNGLVERTEXATTRIB1DVPROC                 glVertexAttrib1dv = nullptr;
PFNGLVERTEXATTRIB1FPROC                  glVertexAttrib1f = nullptr;
PFNGLVERTEXATTRIB1FVPROC                 glVertexAttrib1fv = nullptr;
PFNGLVERTEXATTRIB1SPROC                  glVertexAttrib1s = nullptr;
PFNGLVERTEXATTRIB1SVPROC                 glVertexAttrib1sv = nullptr;
PFNGLVERTEXATTRIB2DPROC                  glVertexAttrib2d = nullptr;
PFNGLVERTEXATTRIB2DVPROC                 glVertexAttrib2dv = nullptr;
PFNGLVERTEXATTRIB2FPROC                  glVertexAttrib2f = nullptr;
PFNGLVERTEXATTRIB2FVPROC                 glVertexAttrib2fv = nullptr;
PFNGLVERTEXATTRIB2SPROC                  glVertexAttrib2s = nullptr;
PFNGLVERTEXATTRIB2SVPROC                 glVertexAttrib2sv = nullptr;
PFNGLVERTEXATTRIB3DPROC                  glVertexAttrib3d = nullptr;
PFNGLVERTEXATTRIB3DVPROC                 glVertexAttrib3dv = nullptr;
PFNGLVERTEXATTRIB3FPROC                  glVertexAttrib3f = nullptr;
PFNGLVERTEXATTRIB3FVPROC                 glVertexAttrib3fv = nullptr;
PFNGLVERTEXATTRIB3SPROC                  glVertexAttrib3s = nullptr;
PFNGLVERTEXATTRIB3SVPROC                 glVertexAttrib3sv = nullptr;
PFNGLVERTEXATTRIB4NBVPROC                glVertexAttrib4Nbv = nullptr;
PFNGLVERTEXATTRIB4NIVPROC                glVertexAttrib4Niv = nullptr;
PFNGLVERTEXATTRIB4NSVPROC                glVertexAttrib4Nsv = nullptr;
PFNGLVERTEXATTRIB4NUBPROC                glVertexAttrib4Nub = nullptr;
PFNGLVERTEXATTRIB4NUBVPROC               glVertexAttrib4Nubv = nullptr;
PFNGLVERTEXATTRIB4NUIVPROC               glVertexAttrib4Nuiv = nullptr;
PFNGLVERTEXATTRIB4NUSVPROC               glVertexAttrib4Nusv = nullptr;
PFNGLVERTEXATTRIB4BVPROC                 glVertexAttrib4bv = nullptr;
PFNGLVERTEXATTRIB4DPROC                  glVertexAttrib4d = nullptr;
PFNGLVERTEXATTRIB4DVPROC                 glVertexAttrib4dv = nullptr;
PFNGLVERTEXATTRIB4FPROC                  glVertexAttrib4f = nullptr;
PFNGLVERTEXATTRIB4FVPROC                 glVertexAttrib4fv = nullptr;
PFNGLVERTEXATTRIB4IVPROC                 glVertexAttrib4iv = nullptr;
PFNGLVERTEXATTRIB4SPROC                  glVertexAttrib4s = nullptr;
PFNGLVERTEXATTRIB4SVPROC                 glVertexAttrib4sv = nullptr;
PFNGLVERTEXATTRIB4UBVPROC                glVertexAttrib4ubv = nullptr;
PFNGLVERTEXATTRIB4UIVPROC                glVertexAttrib4uiv = nullptr;
PFNGLVERTEXATTRIB4USVPROC                glVertexAttrib4usv = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC             glVertexAttribPointer = nullptr;

// GL_VERSION_2_1

// GL_VERSION_3_0
PFNGLCOLORMASKIPROC                              glColorMaski = nullptr;
PFNGLGETBOOLEANI_VPROC                           glGetBooleani_v = nullptr;
PFNGLGETINTEGERI_VPROC                           glGetIntegeri_v = nullptr;
PFNGLENABLEIPROC                                 glEnablei = nullptr;
PFNGLDISABLEIPROC                                glDisablei = nullptr;
PFNGLISENABLEDIPROC                              glIsEnabledi = nullptr;
PFNGLBEGINTRANSFORMFEEDBACKPROC                  glBeginTransformFeedback = nullptr;
PFNGLENDTRANSFORMFEEDBACKPROC                    glEndTransformFeedback = nullptr;
PFNGLBINDBUFFERRANGEPROC                         glBindBufferRange = nullptr;
PFNGLBINDBUFFERBASEPROC                          glBindBufferBase = nullptr;
PFNGLTRANSFORMFEEDBACKVARYINGSPROC               glTransformFeedbackVaryings = nullptr;
PFNGLGETTRANSFORMFEEDBACKVARYINGPROC             glGetTransformFeedbackVarying = nullptr;
PFNGLCLAMPCOLORPROC                              glClampColor = nullptr;
PFNGLBEGINCONDITIONALRENDERPROC                  glBeginConditionalRender = nullptr;
PFNGLENDCONDITIONALRENDERPROC                    glEndConditionalRender = nullptr;
PFNGLVERTEXATTRIBIPOINTERPROC                    glVertexAttribIPointer = nullptr;
PFNGLGETVERTEXATTRIBIIVPROC                      glGetVertexAttribIiv = nullptr;
PFNGLGETVERTEXATTRIBIUIVPROC                     glGetVertexAttribIuiv = nullptr;
PFNGLVERTEXATTRIBI1IPROC                         glVertexAttribI1i = nullptr;
PFNGLVERTEXATTRIBI2IPROC                         glVertexAttribI2i = nullptr;
PFNGLVERTEXATTRIBI3IPROC                         glVertexAttribI3i = nullptr;
PFNGLVERTEXATTRIBI4IPROC                         glVertexAttribI4i = nullptr;
PFNGLVERTEXATTRIBI1UIPROC                        glVertexAttribI1ui = nullptr;
PFNGLVERTEXATTRIBI2UIPROC                        glVertexAttribI2ui = nullptr;
PFNGLVERTEXATTRIBI3UIPROC                        glVertexAttribI3ui = nullptr;
PFNGLVERTEXATTRIBI4UIPROC                        glVertexAttribI4ui = nullptr;
PFNGLVERTEXATTRIBI1IVPROC                        glVertexAttribI1iv = nullptr;
PFNGLVERTEXATTRIBI2IVPROC                        glVertexAttribI2iv = nullptr;
PFNGLVERTEXATTRIBI3IVPROC                        glVertexAttribI3iv = nullptr;
PFNGLVERTEXATTRIBI4IVPROC                        glVertexAttribI4iv = nullptr;
PFNGLVERTEXATTRIBI1UIVPROC                       glVertexAttribI1uiv = nullptr;
PFNGLVERTEXATTRIBI2UIVPROC                       glVertexAttribI2uiv = nullptr;
PFNGLVERTEXATTRIBI3UIVPROC                       glVertexAttribI3uiv = nullptr;
PFNGLVERTEXATTRIBI4UIVPROC                       glVertexAttribI4uiv = nullptr;
PFNGLVERTEXATTRIBI4BVPROC                        glVertexAttribI4bv = nullptr;
PFNGLVERTEXATTRIBI4SVPROC                        glVertexAttribI4sv = nullptr;
PFNGLVERTEXATTRIBI4UBVPROC                       glVertexAttribI4ubv = nullptr;
PFNGLVERTEXATTRIBI4USVPROC                       glVertexAttribI4usv = nullptr;
PFNGLBINDFRAGDATALOCATIONPROC                    glBindFragDataLocation = nullptr;
PFNGLGETFRAGDATALOCATIONPROC                     glGetFragDataLocation = nullptr;
PFNGLTEXPARAMETERIIVPROC                         glTexParameterIiv = nullptr;
PFNGLTEXPARAMETERIUIVPROC                        glTexParameterIuiv = nullptr;
PFNGLGETTEXPARAMETERIIVPROC                      glGetTexParameterIiv = nullptr;
PFNGLGETTEXPARAMETERIUIVPROC                     glGetTexParameterIuiv = nullptr;
PFNGLCLEARBUFFERIVPROC                           glClearBufferiv = nullptr;
PFNGLCLEARBUFFERUIVPROC                          glClearBufferuiv = nullptr;
PFNGLCLEARBUFFERFVPROC                           glClearBufferfv = nullptr;
PFNGLCLEARBUFFERFIPROC                           glClearBufferfi = nullptr;
PFNGLGETSTRINGIPROC                              glGetStringi = nullptr;
PFNGLISRENDERBUFFERPROC                          glIsRenderbuffer = nullptr;
PFNGLBINDRENDERBUFFERPROC                        glBindRenderbuffer = nullptr;
PFNGLDELETERENDERBUFFERSPROC                     glDeleteRenderbuffers = nullptr;
PFNGLGENRENDERBUFFERSPROC                        glGenRenderbuffers = nullptr;
PFNGLRENDERBUFFERSTORAGEPROC                     glRenderbufferStorage = nullptr;
PFNGLGETRENDERBUFFERPARAMETERIVPROC              glGetRenderbufferParameteriv = nullptr;
PFNGLISFRAMEBUFFERPROC                           glIsFramebuffer = nullptr;
PFNGLBINDFRAMEBUFFERPROC                         glBindFramebuffer = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC                      glDeleteFramebuffers = nullptr;
PFNGLGENFRAMEBUFFERSPROC                         glGenFramebuffers = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC                  glCheckFramebufferStatus = nullptr;
PFNGLFRAMEBUFFERTEXTURE1DPROC                    glFramebufferTexture1D = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC                    glFramebufferTexture2D = nullptr;
PFNGLFRAMEBUFFERTEXTURE3DPROC                    glFramebufferTexture3D = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC                 glFramebufferRenderbuffer = nullptr;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC     glGetFramebufferAttachmentParameteriv = nullptr;
PFNGLGENERATEMIPMAPPROC                          glGenerateMipmap = nullptr;
PFNGLBLITFRAMEBUFFERPROC                         glBlitFramebuffer = nullptr;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC          glRenderbufferStorageMultisample = nullptr;
PFNGLFRAMEBUFFERTEXTURELAYERPROC                 glFramebufferTextureLayer = nullptr;
PFNGLMAPBUFFERRANGEPROC                          glMapBufferRange = nullptr;
PFNGLFLUSHMAPPEDBUFFERRANGEPROC                  glFlushMappedBufferRange = nullptr;
PFNGLBINDVERTEXARRAYPROC                         glBindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC                      glDeleteVertexArrays = nullptr;
PFNGLGENVERTEXARRAYSPROC                         glGenVertexArrays = nullptr;
PFNGLISVERTEXARRAYPROC                           glIsVertexArray = nullptr;

// GL_VERSION_3_1
PFNGLDRAWARRAYSINSTANCEDPROC         glDrawArraysInstanced = nullptr;
PFNGLDRAWELEMENTSINSTANCEDPROC       glDrawElementsInstanced = nullptr;
PFNGLTEXBUFFERPROC                   glTexBuffer = nullptr;
PFNGLPRIMITIVERESTARTINDEXPROC       glPrimitiveRestartIndex = nullptr;
PFNGLCOPYBUFFERSUBDATAPROC           glCopyBufferSubData = nullptr;

// GL_VERSION_3_2
PFNGLDRAWELEMENTSBASEVERTEXPROC          glDrawElementsBaseVertex = nullptr;
PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC     glDrawRangeElementsBaseVertex = nullptr;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC glDrawElementsInstancedBaseVertex = nullptr;
PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC     glMultiDrawElementsBaseVertex = nullptr;
PFNGLPROVOKINGVERTEXPROC                 glProvokingVertex = nullptr;
PFNGLFENCESYNCPROC                       glFenceSync = nullptr;
PFNGLISSYNCPROC                          glIsSync = nullptr;
PFNGLDELETESYNCPROC                      glDeleteSync = nullptr;
PFNGLCLIENTWAITSYNCPROC                  glClientWaitSync = nullptr;
PFNGLWAITSYNCPROC                        glWaitSync = nullptr;
PFNGLGETINTEGER64VPROC                   glGetInteger64v = nullptr;
PFNGLGETSYNCIVPROC                       glGetSynciv = nullptr;
PFNGLGETINTEGER64I_VPROC                 glGetInteger64i_v = nullptr;
PFNGLGETBUFFERPARAMETERI64VPROC          glGetBufferParameteri64v = nullptr;
PFNGLFRAMEBUFFERTEXTUREPROC              glFramebufferTexture = nullptr;
PFNGLTEXIMAGE2DMULTISAMPLEPROC           glTexImage2DMultisample = nullptr;
PFNGLTEXIMAGE3DMULTISAMPLEPROC           glTexImage3DMultisample = nullptr;
PFNGLGETMULTISAMPLEFVPROC                glGetMultisamplefv = nullptr;
PFNGLSAMPLEMASKIPROC                     glSampleMaski = nullptr;

// GL_VERSION_3_3
PFNGLBINDFRAGDATALOCATIONINDEXEDPROC  glBindFragDataLocationIndexed = nullptr;
PFNGLGETFRAGDATAINDEXPROC             glGetFragDataIndex = nullptr;
PFNGLGENSAMPLERSPROC                  glGenSamplers = nullptr;
PFNGLDELETESAMPLERSPROC               glDeleteSamplers = nullptr;
PFNGLISSAMPLERPROC                    glIsSampler = nullptr;
PFNGLBINDSAMPLERPROC                  glBindSampler = nullptr;
PFNGLSAMPLERPARAMETERIPROC            glSamplerParameteri = nullptr;
PFNGLSAMPLERPARAMETERIVPROC           glSamplerParameteriv = nullptr;
PFNGLSAMPLERPARAMETERFPROC            glSamplerParameterf = nullptr;
PFNGLSAMPLERPARAMETERFVPROC           glSamplerParameterfv = nullptr;
PFNGLSAMPLERPARAMETERIIVPROC          glSamplerParameterIiv = nullptr;
PFNGLSAMPLERPARAMETERIUIVPROC         glSamplerParameterIuiv = nullptr;
PFNGLGETSAMPLERPARAMETERIVPROC        glGetSamplerParameteriv = nullptr;
PFNGLGETSAMPLERPARAMETERIIVPROC       glGetSamplerParameterIiv = nullptr;
PFNGLGETSAMPLERPARAMETERFVPROC        glGetSamplerParameterfv = nullptr;
PFNGLGETSAMPLERPARAMETERIUIVPROC      glGetSamplerParameterIuiv = nullptr;
PFNGLVERTEXATTRIBDIVISORPROC          glVertexAttribDivisor = nullptr;
PFNGLVERTEXATTRIBP1UIPROC             glVertexAttribP1ui = nullptr;
PFNGLVERTEXATTRIBP1UIVPROC            glVertexAttribP1uiv = nullptr;
PFNGLVERTEXATTRIBP2UIPROC             glVertexAttribP2ui = nullptr;
PFNGLVERTEXATTRIBP2UIVPROC            glVertexAttribP2uiv = nullptr;
PFNGLVERTEXATTRIBP3UIPROC             glVertexAttribP3ui = nullptr;
PFNGLVERTEXATTRIBP3UIVPROC            glVertexAttribP3uiv = nullptr;
PFNGLVERTEXATTRIBP4UIPROC             glVertexAttribP4ui = nullptr;
PFNGLVERTEXATTRIBP4UIVPROC            glVertexAttribP4uiv = nullptr;

// GL_VERSION_4_0
PFNGLMINSAMPLESHADINGPROC                glMinSampleShading = nullptr;
PFNGLBLENDEQUATIONIPROC                  glBlendEquationi = nullptr;
PFNGLBLENDEQUATIONSEPARATEIPROC          glBlendEquationSeparatei = nullptr;
PFNGLBLENDFUNCIPROC                      glBlendFunci = nullptr;
PFNGLBLENDFUNCSEPARATEIPROC              glBlendFuncSeparatei = nullptr;
PFNGLDRAWARRAYSINDIRECTPROC              glDrawArraysIndirect = nullptr;
PFNGLDRAWELEMENTSINDIRECTPROC            glDrawElementsIndirect = nullptr;
PFNGLGETSUBROUTINEINDEXPROC              glGetSubroutineIndex = nullptr;
PFNGLGETACTIVESUBROUTINENAMEPROC         glGetActiveSubroutineName = nullptr;
PFNGLPATCHPARAMETERIPROC                 glPatchParameteri = nullptr;
PFNGLPATCHPARAMETERFVPROC                glPatchParameterfv = nullptr;
PFNGLBINDTRANSFORMFEEDBACKPROC           glBindTransformFeedback = nullptr;
PFNGLDELETETRANSFORMFEEDBACKSPROC        glDeleteTransformFeedbacks = nullptr;
PFNGLGENTRANSFORMFEEDBACKSPROC           glGenTransformFeedbacks = nullptr;
PFNGLISTRANSFORMFEEDBACKPROC             glIsTransformFeedback = nullptr;
PFNGLPAUSETRANSFORMFEEDBACKPROC          glPauseTransformFeedback = nullptr;
PFNGLRESUMETRANSFORMFEEDBACKPROC         glResumeTransformFeedback = nullptr;
PFNGLDRAWTRANSFORMFEEDBACKPROC           glDrawTransformFeedback = nullptr;
PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC     glDrawTransformFeedbackStream = nullptr;

// GL_VERSION_4_1
PFNGLDEPTHRANGEFPROC                     glDepthRangef = nullptr;
PFNGLCLEARDEPTHFPROC                     glClearDepthf = nullptr;
PFNGLVERTEXATTRIBL1DPROC                 glVertexAttribL1d = nullptr;
PFNGLVERTEXATTRIBL2DPROC                 glVertexAttribL2d = nullptr;
PFNGLVERTEXATTRIBL3DPROC                 glVertexAttribL3d = nullptr;
PFNGLVERTEXATTRIBL4DPROC                 glVertexAttribL4d = nullptr;
PFNGLVERTEXATTRIBL1DVPROC                glVertexAttribL1dv = nullptr;
PFNGLVERTEXATTRIBL2DVPROC                glVertexAttribL2dv = nullptr;
PFNGLVERTEXATTRIBL3DVPROC                glVertexAttribL3dv = nullptr;
PFNGLVERTEXATTRIBL4DVPROC                glVertexAttribL4dv = nullptr;
PFNGLVERTEXATTRIBLPOINTERPROC            glVertexAttribLPointer = nullptr;
PFNGLGETVERTEXATTRIBLDVPROC              glGetVertexAttribLdv = nullptr;
PFNGLVIEWPORTARRAYVPROC                  glViewportArrayv = nullptr;
PFNGLVIEWPORTINDEXEDFPROC                glViewportIndexedf = nullptr;
PFNGLVIEWPORTINDEXEDFVPROC               glViewportIndexedfv = nullptr;
PFNGLSCISSORARRAYVPROC                   glScissorArrayv = nullptr;
PFNGLSCISSORINDEXEDPROC                  glScissorIndexed = nullptr;
PFNGLSCISSORINDEXEDVPROC                 glScissorIndexedv = nullptr;
PFNGLDEPTHRANGEARRAYVPROC                glDepthRangeArrayv = nullptr;
PFNGLDEPTHRANGEINDEXEDPROC               glDepthRangeIndexed = nullptr;
PFNGLGETFLOATI_VPROC                     glGetFloati_v = nullptr;
PFNGLGETDOUBLEI_VPROC                    glGetDoublei_v = nullptr;

// GL_VERSION_4_2
PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC             glDrawArraysInstancedBaseInstance = nullptr;
PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC           glDrawElementsInstancedBaseInstance = nullptr;
PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC glDrawElementsInstancedBaseVertexBaseInstance = nullptr;
PFNGLGETINTERNALFORMATIVPROC                         glGetInternalformativ = nullptr;
PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC              glGetActiveAtomicCounterBufferiv = nullptr;
PFNGLBINDIMAGETEXTUREPROC                            glBindImageTexture = nullptr;
PFNGLMEMORYBARRIERPROC                               glMemoryBarrier = nullptr;
PFNGLTEXSTORAGE1DPROC                                glTexStorage1D = nullptr;
PFNGLTEXSTORAGE2DPROC                                glTexStorage2D = nullptr;
PFNGLTEXSTORAGE3DPROC                                glTexStorage3D = nullptr;
PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC              glDrawTransformFeedbackInstanced = nullptr;
PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC        glDrawTransformFeedbackStreamInstanced = nullptr;

// GL_VERSION_4_3
PFNGLCLEARBUFFERDATAPROC                             glClearBufferData = nullptr;
PFNGLCLEARBUFFERSUBDATAPROC                          glClearBufferSubData = nullptr;
PFNGLDISPATCHCOMPUTEPROC                             glDispatchCompute = nullptr;
PFNGLDISPATCHCOMPUTEINDIRECTPROC                     glDispatchComputeIndirect = nullptr;
PFNGLCOPYIMAGESUBDATAPROC                            glCopyImageSubData = nullptr;
PFNGLFRAMEBUFFERPARAMETERIPROC                       glFramebufferParameteri = nullptr;
PFNGLGETFRAMEBUFFERPARAMETERIVPROC                   glGetFramebufferParameteriv = nullptr;
PFNGLGETINTERNALFORMATI64VPROC                       glGetInternalformati64v = nullptr;
PFNGLINVALIDATETEXSUBIMAGEPROC                       glInvalidateTexSubImage = nullptr;
PFNGLINVALIDATETEXIMAGEPROC                          glInvalidateTexImage = nullptr;
PFNGLINVALIDATEBUFFERSUBDATAPROC                     glInvalidateBufferSubData = nullptr;
PFNGLINVALIDATEBUFFERDATAPROC                        glInvalidateBufferData = nullptr;
PFNGLINVALIDATEFRAMEBUFFERPROC                       glInvalidateFramebuffer = nullptr;
PFNGLINVALIDATESUBFRAMEBUFFERPROC                    glInvalidateSubFramebuffer = nullptr;
PFNGLMULTIDRAWARRAYSINDIRECTPROC                     glMultiDrawArraysIndirect = nullptr;
PFNGLMULTIDRAWELEMENTSINDIRECTPROC                   glMultiDrawElementsIndirect = nullptr;
PFNGLTEXBUFFERRANGEPROC                              glTexBufferRange = nullptr;
PFNGLTEXSTORAGE2DMULTISAMPLEPROC                     glTexStorage2DMultisample = nullptr;
PFNGLTEXSTORAGE3DMULTISAMPLEPROC                     glTexStorage3DMultisample = nullptr;
PFNGLTEXTUREVIEWPROC                                 glTextureView = nullptr;
PFNGLBINDVERTEXBUFFERPROC                            glBindVertexBuffer = nullptr;
PFNGLVERTEXATTRIBFORMATPROC                          glVertexAttribFormat = nullptr;
PFNGLVERTEXATTRIBIFORMATPROC                         glVertexAttribIFormat = nullptr;
PFNGLVERTEXATTRIBLFORMATPROC                         glVertexAttribLFormat = nullptr;
PFNGLVERTEXATTRIBBINDINGPROC                         glVertexAttribBinding = nullptr;
PFNGLVERTEXBINDINGDIVISORPROC                        glVertexBindingDivisor = nullptr;
PFNGLPUSHDEBUGGROUPPROC                              glPushDebugGroup = nullptr;
PFNGLPOPDEBUGGROUPPROC                               glPopDebugGroup = nullptr;
PFNGLOBJECTLABELPROC                                 glObjectLabel = nullptr;
PFNGLGETOBJECTLABELPROC                              glGetObjectLabel = nullptr;
PFNGLOBJECTPTRLABELPROC                              glObjectPtrLabel = nullptr;
PFNGLGETOBJECTPTRLABELPROC                           glGetObjectPtrLabel = nullptr;

// GL_VERSION_4_4
PFNGLBUFFERSTORAGEPROC       glBufferStorage = nullptr;
PFNGLCLEARTEXIMAGEPROC       glClearTexImage = nullptr;
PFNGLCLEARTEXSUBIMAGEPROC    glClearTexSubImage = nullptr;
PFNGLBINDBUFFERSBASEPROC     glBindBuffersBase = nullptr;
PFNGLBINDBUFFERSRANGEPROC    glBindBuffersRange = nullptr;
PFNGLBINDTEXTURESPROC        glBindTextures = nullptr;
PFNGLBINDSAMPLERSPROC        glBindSamplers = nullptr;
PFNGLBINDIMAGETEXTURESPROC   glBindImageTextures = nullptr;
PFNGLBINDVERTEXBUFFERSPROC   glBindVertexBuffers = nullptr;

// GL_VERSION_4_5
PFNGLCLIPCONTROLPROC                                     glClipControl = nullptr;
PFNGLCREATETRANSFORMFEEDBACKSPROC                        glCreateTransformFeedbacks = nullptr;
PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC                     glTransformFeedbackBufferBase = nullptr;
PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC                    glTransformFeedbackBufferRange = nullptr;
PFNGLGETTRANSFORMFEEDBACKIVPROC                          glGetTransformFeedbackiv = nullptr;
PFNGLGETTRANSFORMFEEDBACKI_VPROC                         glGetTransformFeedbacki_v = nullptr;
PFNGLGETTRANSFORMFEEDBACKI64_VPROC                       glGetTransformFeedbacki64_v = nullptr;
PFNGLCREATEBUFFERSPROC                                   glCreateBuffers = nullptr;
PFNGLNAMEDBUFFERSTORAGEPROC                              glNamedBufferStorage = nullptr;
PFNGLNAMEDBUFFERDATAPROC                                 glNamedBufferData = nullptr;
PFNGLNAMEDBUFFERSUBDATAPROC                              glNamedBufferSubData = nullptr;
PFNGLCOPYNAMEDBUFFERSUBDATAPROC                          glCopyNamedBufferSubData = nullptr;
PFNGLCLEARNAMEDBUFFERDATAPROC                            glClearNamedBufferData = nullptr;
PFNGLCLEARNAMEDBUFFERSUBDATAPROC                         glClearNamedBufferSubData = nullptr;
PFNGLMAPNAMEDBUFFERPROC                                  glMapNamedBuffer = nullptr;
PFNGLMAPNAMEDBUFFERRANGEPROC                             glMapNamedBufferRange = nullptr;
PFNGLUNMAPNAMEDBUFFERPROC                                glUnmapNamedBuffer = nullptr;
PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC                     glFlushMappedNamedBufferRange = nullptr;
PFNGLGETNAMEDBUFFERPARAMETERIVPROC                       glGetNamedBufferParameteriv = nullptr;
PFNGLGETNAMEDBUFFERPARAMETERI64VPROC                     glGetNamedBufferParameteri64v = nullptr;
PFNGLGETNAMEDBUFFERPOINTERVPROC                          glGetNamedBufferPointerv = nullptr;
PFNGLGETNAMEDBUFFERSUBDATAPROC                           glGetNamedBufferSubData = nullptr;
PFNGLCREATEFRAMEBUFFERSPROC                              glCreateFramebuffers = nullptr;
PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC                    glNamedFramebufferRenderbuffer = nullptr;
PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC                      glNamedFramebufferParameteri = nullptr;
PFNGLNAMEDFRAMEBUFFERTEXTUREPROC                         glNamedFramebufferTexture = nullptr;
PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC                    glNamedFramebufferTextureLayer = nullptr;
PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC                      glNamedFramebufferDrawBuffer = nullptr;
PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC                     glNamedFramebufferDrawBuffers = nullptr;
PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC                      glNamedFramebufferReadBuffer = nullptr;
PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC                  glInvalidateNamedFramebufferData = nullptr;
PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC               glInvalidateNamedFramebufferSubData = nullptr;
PFNGLCLEARNAMEDFRAMEBUFFERIVPROC                         glClearNamedFramebufferiv = nullptr;
PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC                        glClearNamedFramebufferuiv = nullptr;
PFNGLCLEARNAMEDFRAMEBUFFERFVPROC                         glClearNamedFramebufferfv = nullptr;
PFNGLCLEARNAMEDFRAMEBUFFERFIPROC                         glClearNamedFramebufferfi = nullptr;
PFNGLBLITNAMEDFRAMEBUFFERPROC                            glBlitNamedFramebuffer = nullptr;
PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC                     glCheckNamedFramebufferStatus = nullptr;
PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC                  glGetNamedFramebufferParameteriv = nullptr;
PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC        glGetNamedFramebufferAttachmentParameteriv = nullptr;
PFNGLCREATERENDERBUFFERSPROC                             glCreateRenderbuffers = nullptr;
PFNGLNAMEDRENDERBUFFERSTORAGEPROC                        glNamedRenderbufferStorage = nullptr;
PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC             glNamedRenderbufferStorageMultisample = nullptr;
PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC                 glGetNamedRenderbufferParameteriv = nullptr;
PFNGLCREATETEXTURESPROC                                  glCreateTextures = nullptr;
PFNGLTEXTUREBUFFERPROC                                   glTextureBuffer = nullptr;
PFNGLTEXTUREBUFFERRANGEPROC                              glTextureBufferRange = nullptr;
PFNGLTEXTURESTORAGE1DPROC                                glTextureStorage1D = nullptr;
PFNGLTEXTURESTORAGE2DPROC                                glTextureStorage2D = nullptr;
PFNGLTEXTURESTORAGE3DPROC                                glTextureStorage3D = nullptr;
PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC                     glTextureStorage2DMultisample = nullptr;
PFNGLTEXTURESTORAGE3DMULTISAMPLEPROC                     glTextureStorage3DMultisample = nullptr;
PFNGLTEXTURESUBIMAGE1DPROC                               glTextureSubImage1D = nullptr;
PFNGLTEXTURESUBIMAGE2DPROC                               glTextureSubImage2D = nullptr;
PFNGLTEXTURESUBIMAGE3DPROC                               glTextureSubImage3D = nullptr;
PFNGLCOMPRESSEDTEXTURESUBIMAGE1DPROC                     glCompressedTextureSubImage1D = nullptr;
PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC                     glCompressedTextureSubImage2D = nullptr;
PFNGLCOMPRESSEDTEXTURESUBIMAGE3DPROC                     glCompressedTextureSubImage3D = nullptr;
PFNGLCOPYTEXTURESUBIMAGE1DPROC                           glCopyTextureSubImage1D = nullptr;
PFNGLCOPYTEXTURESUBIMAGE2DPROC                           glCopyTextureSubImage2D = nullptr;
PFNGLCOPYTEXTURESUBIMAGE3DPROC                           glCopyTextureSubImage3D = nullptr;
PFNGLTEXTUREPARAMETERFPROC                               glTextureParameterf = nullptr;
PFNGLTEXTUREPARAMETERFVPROC                              glTextureParameterfv = nullptr;
PFNGLTEXTUREPARAMETERIPROC                               glTextureParameteri = nullptr;
PFNGLTEXTUREPARAMETERIIVPROC                             glTextureParameterIiv = nullptr;
PFNGLTEXTUREPARAMETERIUIVPROC                            glTextureParameterIuiv = nullptr;
PFNGLTEXTUREPARAMETERIVPROC                              glTextureParameteriv = nullptr;
PFNGLGENERATETEXTUREMIPMAPPROC                           glGenerateTextureMipmap = nullptr;
PFNGLBINDTEXTUREUNITPROC                                 glBindTextureUnit = nullptr;
PFNGLGETTEXTUREIMAGEPROC                                 glGetTextureImage = nullptr;
PFNGLGETCOMPRESSEDTEXTUREIMAGEPROC                       glGetCompressedTextureImage = nullptr;
PFNGLGETTEXTURELEVELPARAMETERFVPROC                      glGetTextureLevelParameterfv = nullptr;
PFNGLGETTEXTURELEVELPARAMETERIVPROC                      glGetTextureLevelParameteriv = nullptr;
PFNGLGETTEXTUREPARAMETERFVPROC                           glGetTextureParameterfv = nullptr;
PFNGLGETTEXTUREPARAMETERIIVPROC                          glGetTextureParameterIiv = nullptr;
PFNGLGETTEXTUREPARAMETERIUIVPROC                         glGetTextureParameterIuiv = nullptr;
PFNGLGETTEXTUREPARAMETERIVPROC                           glGetTextureParameteriv = nullptr;
PFNGLCREATEVERTEXARRAYSPROC                              glCreateVertexArrays = nullptr;
PFNGLDISABLEVERTEXARRAYATTRIBPROC                        glDisableVertexArrayAttrib = nullptr;
PFNGLENABLEVERTEXARRAYATTRIBPROC                         glEnableVertexArrayAttrib = nullptr;
PFNGLVERTEXARRAYELEMENTBUFFERPROC                        glVertexArrayElementBuffer = nullptr;
PFNGLVERTEXARRAYVERTEXBUFFERPROC                         glVertexArrayVertexBuffer = nullptr;
PFNGLVERTEXARRAYVERTEXBUFFERSPROC                        glVertexArrayVertexBuffers = nullptr;
PFNGLVERTEXARRAYATTRIBBINDINGPROC                        glVertexArrayAttribBinding = nullptr;
PFNGLVERTEXARRAYATTRIBFORMATPROC                         glVertexArrayAttribFormat = nullptr;
PFNGLVERTEXARRAYATTRIBIFORMATPROC                        glVertexArrayAttribIFormat = nullptr;
PFNGLVERTEXARRAYATTRIBLFORMATPROC                        glVertexArrayAttribLFormat = nullptr;
PFNGLVERTEXARRAYBINDINGDIVISORPROC                       glVertexArrayBindingDivisor = nullptr;
PFNGLGETVERTEXARRAYIVPROC                                glGetVertexArrayiv = nullptr;
PFNGLGETVERTEXARRAYINDEXEDIVPROC                         glGetVertexArrayIndexediv = nullptr;
PFNGLGETVERTEXARRAYINDEXED64IVPROC                       glGetVertexArrayIndexed64iv = nullptr;
PFNGLCREATESAMPLERSPROC                                  glCreateSamplers = nullptr;
PFNGLMEMORYBARRIERBYREGIONPROC                           glMemoryBarrierByRegion = nullptr;
PFNGLGETTEXTURESUBIMAGEPROC                              glGetTextureSubImage = nullptr;
PFNGLGETCOMPRESSEDTEXTURESUBIMAGEPROC                    glGetCompressedTextureSubImage = nullptr;
PFNGLGETGRAPHICSRESETSTATUSPROC                          glGetGraphicsResetStatus = nullptr;
PFNGLGETNCOMPRESSEDTEXIMAGEPROC                          glGetnCompressedTexImage = nullptr;
PFNGLGETNTEXIMAGEPROC                                    glGetnTexImage = nullptr;
PFNGLREADNPIXELSPROC                                     glReadnPixels = nullptr;
PFNGLTEXTUREBARRIERPROC                                  glTextureBarrier = nullptr;

// GL_VERSION_4_6
PFNGLMULTIDRAWARRAYSINDIRECTCOUNTPROC    glMultiDrawArraysIndirectCount = nullptr;
PFNGLMULTIDRAWELEMENTSINDIRECTCOUNTPROC  glMultiDrawElementsIndirectCount = nullptr;
PFNGLPOLYGONOFFSETCLAMPPROC              glPolygonOffsetClamp = nullptr;

// GL_EXT_depth_bounds_test
PFNGLDEPTHBOUNDSEXTPROC                glDepthBoundsEXT                = nullptr;

LLGLManager gGLManager;

LLGLManager::LLGLManager() :
    mInited(false),
    mIsDisabled(false),
    mMaxSamples(0),
    mNumTextureImageUnits(1),
    mMaxSampleMaskWords(0),
    mMaxColorTextureSamples(0),
    mMaxDepthTextureSamples(0),
    mMaxIntegerSamples(0),
    mIsAMD(false),
    mIsNVIDIA(false),
    mIsIntel(false),
    mHasRequirements(true),
    mDriverVersionMajor(1),
    mDriverVersionMinor(0),
    mDriverVersionRelease(0),
    mGLVersion(1.0f),
    mGLSLVersionMajor(0),
    mGLSLVersionMinor(0),
    mVRAM(0),
    mGLMaxVertexRange(0),
    mGLMaxIndexRange(0),
    mGLMaxTextureSize(0)
{
}

//---------------------------------------------------------------------
// Global initialization for GL
//---------------------------------------------------------------------
void LLGLManager::initWGL()
{
#if LL_WINDOWS
    reloadExtensionsString();

    if (mGLExtensions.contains("WGL_ARB_pixel_format"))
    {
        wglGetPixelFormatAttribivARB = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)LL_GET_PROC_ADDRESS("wglGetPixelFormatAttribivARB");
        wglGetPixelFormatAttribfvARB = (PFNWGLGETPIXELFORMATATTRIBFVARBPROC)LL_GET_PROC_ADDRESS("wglGetPixelFormatAttribfvARB");
        wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)LL_GET_PROC_ADDRESS("wglChoosePixelFormatARB");
    }
    else
    {
        LL_WARNS("RenderInit") << "No ARB pixel format extensions" << LL_ENDL;
    }

    // WGL_ARB_create_context
    if (mGLExtensions.contains("WGL_ARB_create_context"))
    {
        wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)LL_GET_PROC_ADDRESS("wglCreateContextAttribsARB");
    }
    else
    {
        LL_WARNS("RenderInit") << "No ARB create context extensions" << LL_ENDL;
    }

    // For retreiving information per AMD adapter,
    // because we can't trust curently selected/default one when there are multiple
    mHasAMDAssociations = mGLExtensions.contains("WGL_AMD_gpu_association");
    if (mHasAMDAssociations)
    {
        wglGetGPUIDsAMD = (PFNWGLGETGPUIDSAMDPROC)LL_GET_PROC_ADDRESS("wglGetGPUIDsAMD");
        wglGetGPUInfoAMD = (PFNWGLGETGPUINFOAMDPROC)LL_GET_PROC_ADDRESS("wglGetGPUInfoAMD");
        wglGetContextGPUIDAMD = (PFNWGLGETCONTEXTGPUIDAMDPROC)LL_GET_PROC_ADDRESS("wglGetContextGPUIDAMD");
        wglCreateAssociatedContextAMD = (PFNWGLCREATEASSOCIATEDCONTEXTAMDPROC)LL_GET_PROC_ADDRESS("wglCreateAssociatedContextAMD");
        wglCreateAssociatedContextAttribsAMD = (PFNWGLCREATEASSOCIATEDCONTEXTATTRIBSAMDPROC)LL_GET_PROC_ADDRESS("wglCreateAssociatedContextAttribsAMD");
        wglDeleteAssociatedContextAMD = (PFNWGLDELETEASSOCIATEDCONTEXTAMDPROC)LL_GET_PROC_ADDRESS("wglDeleteAssociatedContextAMD");
        wglMakeAssociatedContextCurrentAMD = (PFNWGLMAKEASSOCIATEDCONTEXTCURRENTAMDPROC)LL_GET_PROC_ADDRESS("wglMakeAssociatedContextCurrentAMD");
        wglGetCurrentAssociatedContextAMD = (PFNWGLGETCURRENTASSOCIATEDCONTEXTAMDPROC)LL_GET_PROC_ADDRESS("wglGetCurrentAssociatedContextAMD");
        wglBlitContextFramebufferAMD = (PFNWGLBLITCONTEXTFRAMEBUFFERAMDPROC)LL_GET_PROC_ADDRESS("wglBlitContextFramebufferAMD");

    }

    // WGL_EXT_swap_control
    if (mGLExtensions.contains("WGL_EXT_swap_control"))
    {
        wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)LL_GET_PROC_ADDRESS("wglSwapIntervalEXT");
        wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)LL_GET_PROC_ADDRESS("wglGetSwapIntervalEXT");
    }

    // WGL_NV_DX_interop2 (D3D<->GL sharing for zero-copy CEF media textures)
    if (mGLExtensions.contains("WGL_NV_DX_interop2") || mGLExtensions.contains("WGL_NV_DX_interop"))
    {
        wglDXOpenDeviceNV = (PFNWGLDXOPENDEVICENVPROC)LL_GET_PROC_ADDRESS("wglDXOpenDeviceNV");
        wglDXCloseDeviceNV = (PFNWGLDXCLOSEDEVICENVPROC)LL_GET_PROC_ADDRESS("wglDXCloseDeviceNV");
        wglDXRegisterObjectNV = (PFNWGLDXREGISTEROBJECTNVPROC)LL_GET_PROC_ADDRESS("wglDXRegisterObjectNV");
        wglDXUnregisterObjectNV = (PFNWGLDXUNREGISTEROBJECTNVPROC)LL_GET_PROC_ADDRESS("wglDXUnregisterObjectNV");
        wglDXLockObjectsNV = (PFNWGLDXLOCKOBJECTSNVPROC)LL_GET_PROC_ADDRESS("wglDXLockObjectsNV");
        wglDXUnlockObjectsNV = (PFNWGLDXUNLOCKOBJECTSNVPROC)LL_GET_PROC_ADDRESS("wglDXUnlockObjectsNV");
    }

    if(!mGLExtensions.contains("WGL_ARB_pbuffer"))
    {
        LL_WARNS("RenderInit") << "No ARB WGL PBuffer extensions" << LL_ENDL;
    }
#endif
}

void LLGLManager::initEGL()
{
#if LL_LINUX
    reloadExtensionsString();

    // EGL_VERSION_1_0
    eglQueryString = (PFNEGLQUERYSTRINGPROC)SDL_EGL_GetProcAddress("eglQueryString");

    // EGL_KHR_image
    eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)SDL_EGL_GetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)SDL_EGL_GetProcAddress("eglDestroyImageKHR");

    // GL_OES_EGL_image
    glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)SDL_EGL_GetProcAddress("glEGLImageTargetTexture2DOES");

    // SDL_EGL_GetProcAddress can return null for core EGL symbols (driver/version
    // dependent), so guard before calling through the pointer.
    if (eglQueryString)
    {
        LL_INFOS("RenderInit") << "EGL_VENDOR     " << ll_safe_string((const char *)eglQueryString(SDL_EGL_GetCurrentDisplay(), EGL_VENDOR)) << LL_ENDL;
        LL_INFOS("RenderInit") << "EGL_VERSION    " << ll_safe_string((const char *)eglQueryString(SDL_EGL_GetCurrentDisplay(), EGL_VERSION)) << LL_ENDL;
    }
    else
    {
        LL_WARNS("RenderInit") << "eglQueryString unavailable; skipping EGL vendor/version log" << LL_ENDL;
    }
#endif
}

#if LL_LINUX
namespace
{
    // radeonsi and r600 put the kernel's DRM interface version and release in GL_RENDERER:
    // "(radeonsi, navi31, ACO, DRM 3.64, 7.2.4-200.fc44.x86_64)", in older Mesa
    // "(POLARIS10, DRM 3.42.0, 5.15.0-58-generic, LLVM 13.0.1)", and r600
    // "(DRM 2.50.0 / 6.1.0-13-amd64, LLVM 15.0.6)". Both move with the kernel, not the driver.
    std::string strip_kernel_from_renderer(std::string renderer)
    {
        // With what joins it on, so that a release like "6.1.0" is not found in "LLVM 16.1.0".
        struct utsname uts;
        const std::string release = uname(&uts) == 0 ? uts.release : "";
        for (const std::string separator : { ", ", " / " })
        {
            if (size_t pos = renderer.find(separator + release); !release.empty() && pos != std::string::npos)
            {
                renderer.erase(pos, separator.size() + release.size());
                break;
            }
        }

        if (size_t pos = renderer.find("DRM "); pos != std::string::npos
            && pos + 4 < renderer.size() && isdigit((unsigned char)renderer[pos + 4]))
        {
            size_t end = pos + 4;
            while (end < renderer.size() && (isdigit((unsigned char)renderer[end]) || renderer[end] == '.'))
            {
                ++end;
            }
            if (pos >= 2 && renderer.compare(pos - 2, 2, ", ") == 0)
            {
                pos -= 2;   // "navi31, DRM 3.64"
            }
            else if (renderer.compare(end, 2, ", ") == 0)
            {
                end += 2;   // "(DRM 2.50.0, LLVM"
            }
            renderer.erase(pos, end - pos);
        }
        return renderer;
    }

    // Whole names only: EGL_EXT_device_drm is a prefix of EGL_EXT_device_drm_render_node.
    bool has_egl_extension(const char* extensions, const std::string& name)
    {
        return (" " + ll_safe_string(extensions) + " ").find(" " + name + " ") != std::string::npos;
    }

    // The DRM node of the EGL device behind the current display, e.g. /dev/dri/renderD128,
    // through EGL_EXT_device_query. Empty when the EGL cannot say, and for a software device,
    // which has none.
    std::string egl_device_drm_node()
    {
        EGLDisplay display = (EGLDisplay)SDL_EGL_GetCurrentDisplay();
        PFNEGLQUERYSTRINGPROC query_string = (PFNEGLQUERYSTRINGPROC)SDL_EGL_GetProcAddress("eglQueryString");
        if (!display || !query_string)
        {
            return {};
        }

        // Client extensions, so asked of no display.
        const char* client_extensions = query_string(EGL_NO_DISPLAY, EGL_EXTENSIONS);
        if (!has_egl_extension(client_extensions, "EGL_EXT_device_query")
            && !has_egl_extension(client_extensions, "EGL_EXT_device_base"))
        {
            return {};
        }

        PFNEGLQUERYDISPLAYATTRIBEXTPROC query_display_attrib =
            (PFNEGLQUERYDISPLAYATTRIBEXTPROC)SDL_EGL_GetProcAddress("eglQueryDisplayAttribEXT");
        PFNEGLQUERYDEVICESTRINGEXTPROC query_device_string =
            (PFNEGLQUERYDEVICESTRINGEXTPROC)SDL_EGL_GetProcAddress("eglQueryDeviceStringEXT");
        EGLAttrib device = 0;
        if (!query_display_attrib || !query_device_string
            || !query_display_attrib(display, EGL_DEVICE_EXT, &device) || !device)
        {
            return {};
        }

        const char* device_extensions = query_device_string((EGLDeviceEXT)device, EGL_EXTENSIONS);
        const char* node = nullptr;
        if (has_egl_extension(device_extensions, "EGL_EXT_device_drm_render_node"))
        {
            node = query_device_string((EGLDeviceEXT)device, EGL_DRM_RENDER_NODE_FILE_EXT);
        }
        if (!node && has_egl_extension(device_extensions, "EGL_EXT_device_drm"))
        {
            node = query_device_string((EGLDeviceEXT)device, EGL_DRM_DEVICE_FILE_EXT);
        }
        return ll_safe_string(node);
    }

    // The sysfs directory of the device GL draws with: the one EGL names, else the first
    // render node of GL's vendor. Empty when neither finds one.
    std::string drm_device_sysfs_dir(const std::string& vendor_short)
    {
        const std::string node = egl_device_drm_node();
        if (!node.empty())
        {
            // /dev/dri/renderD128 -> /sys/class/drm/renderD128/device
            return "/sys/class/drm/" + node.substr(node.rfind('/') + 1) + "/device";
        }

        const char* vendor_id = vendor_short == "AMD"    ? "0x1002"
                              : vendor_short == "INTEL"  ? "0x8086"
                              : vendor_short == "NVIDIA" ? "0x10de"
                              : nullptr;
        // Render nodes are numbered from 128.
        for (S32 minor = 128; vendor_id && minor < 192; ++minor)
        {
            const std::string dir = "/sys/class/drm/renderD" + std::to_string(minor) + "/device";
            llifstream file(dir + "/vendor");
            std::string id;
            if (file >> id && id == vendor_id)
            {
                return dir;
            }
        }
        return {};
    }

    // A size sysfs gives in bytes, in MB. 0 when the attribute is missing.
    U32 read_sysfs_mb(const std::string& path)
    {
        llifstream file(path);
        U64 bytes = 0;
        file >> bytes;
        return (U32)(bytes >> 20);
    }

    // VRAM for a GPU GL has no real figure for. amdgpu publishes its memory in sysfs. Memory
    // a GPU shares with the system -- Intel's, a software renderer's -- is a quarter of RAM,
    // which is what LLDXHardware allows Intel on Windows.
    U32 fallback_vram_mb(const std::string& vendor_short, bool shares_system_memory, std::string& source)
    {
        const U32 ram_share_mb = U32Megabytes(gSysMemory.getPhysicalMemoryKB()).value() / 4;

        const std::string device_dir = drm_device_sysfs_dir(vendor_short);
        const U32 vram_mb = device_dir.empty() ? 0 : read_sysfs_mb(device_dir + "/mem_info_vram_total");
        if (vram_mb >= 1024)
        {
            // A card's own memory, or a carve-out big enough to stand for it.
            source = "amdgpu VRAM";
            return vram_mb;
        }
        if (vram_mb > 0)
        {
            // An APU: its VRAM is the BIOS carve-out, and the rest it draws from system memory
            // through the GTT. The carve-out is not counted in system memory, so it comes on
            // top of the quarter, which the GTT's size bounds.
            source = "amdgpu carve-out and GTT";
            return vram_mb + llmin(read_sysfs_mb(device_dir + "/mem_info_gtt_total"), ram_share_mb);
        }
        if (shares_system_memory)
        {
            source = "system memory";
            return ram_share_mb;
        }
        return 0;
    }
}
#endif


// return false if unable (or unwilling due to old drivers) to init GL
bool LLGLManager::initGL()
{
    if (mInited)
    {
        LL_ERRS("RenderInit") << "Calling init on LLGLManager after already initialized!" << LL_ENDL;
    }

    // OpenGL 1.0 and 1.1 always exist
    // GL_VERSION_1_0
    glClearColor = (PFNGLCLEARCOLORPROC)LL_GET_PROC_ADDRESS("glClearColor");
    glClear = (PFNGLCLEARPROC)LL_GET_PROC_ADDRESS("glClear");
    glColorMask = (PFNGLCOLORMASKPROC)LL_GET_PROC_ADDRESS("glColorMask");
    glBlendFunc = (PFNGLBLENDFUNCPROC)LL_GET_PROC_ADDRESS("glBlendFunc");
    glLogicOp = (PFNGLLOGICOPPROC)LL_GET_PROC_ADDRESS("glLogicOp");
    glFrontFace = (PFNGLFRONTFACEPROC)LL_GET_PROC_ADDRESS("glFrontFace");
    glCullFace = (PFNGLCULLFACEPROC)LL_GET_PROC_ADDRESS("glCullFace");;
    glPointSize = (PFNGLPOINTSIZEPROC)LL_GET_PROC_ADDRESS("glPointSize");
    glLineWidth = (PFNGLLINEWIDTHPROC)LL_GET_PROC_ADDRESS("glLineWidth");
    glPolygonMode = (PFNGLPOLYGONMODEPROC)LL_GET_PROC_ADDRESS("glPolygonMode");
    glScissor = (PFNGLSCISSORPROC)LL_GET_PROC_ADDRESS("glScissor");
    glDrawBuffer = (PFNGLDRAWBUFFERPROC)LL_GET_PROC_ADDRESS("glDrawBuffer");
    glReadBuffer = (PFNGLREADBUFFERPROC)LL_GET_PROC_ADDRESS("glReadBuffer");
    glEnable = (PFNGLENABLEPROC)LL_GET_PROC_ADDRESS("glEnable");
    glDisable = (PFNGLDISABLEPROC)LL_GET_PROC_ADDRESS("glDisable");
    glIsEnabled = (PFNGLISENABLEDPROC)LL_GET_PROC_ADDRESS("glIsEnabled");
    glGetBooleanv = (PFNGLGETBOOLEANVPROC)LL_GET_PROC_ADDRESS("glGetBooleanv");
    glGetDoublev = (PFNGLGETDOUBLEVPROC)LL_GET_PROC_ADDRESS("glGetDoublev");
    glGetFloatv = (PFNGLGETFLOATVPROC)LL_GET_PROC_ADDRESS("glGetFloatv");
    glGetIntegerv = (PFNGLGETINTEGERVPROC)LL_GET_PROC_ADDRESS("glGetIntegerv");
    glGetError = (PFNGLGETERRORPROC)LL_GET_PROC_ADDRESS("glGetError");
    glGetString = (PFNGLGETSTRINGPROC)LL_GET_PROC_ADDRESS("glGetString");
    glFinish = (PFNGLFINISHPROC)LL_GET_PROC_ADDRESS("glFinish");
    glFlush = (PFNGLFLUSHPROC)LL_GET_PROC_ADDRESS("glFlush");
    glHint = (PFNGLHINTPROC)LL_GET_PROC_ADDRESS("glHint");
    glClearDepth = (PFNGLCLEARDEPTHPROC)LL_GET_PROC_ADDRESS("glClearDepth");
    glDepthFunc = (PFNGLDEPTHFUNCPROC)LL_GET_PROC_ADDRESS("glDepthFunc");
    glDepthMask = (PFNGLDEPTHMASKPROC)LL_GET_PROC_ADDRESS("glDepthMask");
    glDepthRange = (PFNGLDEPTHRANGEPROC)LL_GET_PROC_ADDRESS("glDepthRange");
    glViewport = (PFNGLVIEWPORTPROC)LL_GET_PROC_ADDRESS("glViewport");
    glPixelStoref = (PFNGLPIXELSTOREFPROC)LL_GET_PROC_ADDRESS("glPixelStoref");
    glPixelStorei = (PFNGLPIXELSTOREIPROC)LL_GET_PROC_ADDRESS("glPixelStorei");
    glReadPixels = (PFNGLREADPIXELSPROC)LL_GET_PROC_ADDRESS("glReadPixels");
    glStencilFunc = (PFNGLSTENCILFUNCPROC)LL_GET_PROC_ADDRESS("glStencilFunc");
    glStencilMask = (PFNGLSTENCILMASKPROC)LL_GET_PROC_ADDRESS("glStencilMask");
    glStencilOp = (PFNGLSTENCILOPPROC)LL_GET_PROC_ADDRESS("glStencilOp");
    glClearStencil = (PFNGLCLEARSTENCILPROC)LL_GET_PROC_ADDRESS("glClearStencil");
    glTexParameterf = (PFNGLTEXPARAMETERFPROC)LL_GET_PROC_ADDRESS("glTexParameterf");
    glTexParameteri = (PFNGLTEXPARAMETERIPROC)LL_GET_PROC_ADDRESS("glTexParameteri");
    glTexParameterfv = (PFNGLTEXPARAMETERFVPROC)LL_GET_PROC_ADDRESS("glTexParameterfv");
    glTexParameteriv = (PFNGLTEXPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glTexParameteriv");
    glGetTexParameterfv = (PFNGLGETTEXPARAMETERFVPROC)LL_GET_PROC_ADDRESS("glGetTexParameterfv");
    glGetTexParameteriv = (PFNGLGETTEXPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetTexParameteriv");
    glGetTexLevelParameterfv = (PFNGLGETTEXLEVELPARAMETERFVPROC)LL_GET_PROC_ADDRESS("glGetTexLevelParameterfv");
    glGetTexLevelParameteriv = (PFNGLGETTEXLEVELPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetTexLevelParameteriv");
    glTexImage1D = (PFNGLTEXIMAGE1DPROC)LL_GET_PROC_ADDRESS("glTexImage1D");
    glTexImage2D = (PFNGLTEXIMAGE2DPROC)LL_GET_PROC_ADDRESS("glTexImage2D");
    glGetTexImage = (PFNGLGETTEXIMAGEPROC)LL_GET_PROC_ADDRESS("glGetTexImage");

    // GL_VERSION_1_1
    glDrawArrays = (PFNGLDRAWARRAYSPROC)LL_GET_PROC_ADDRESS("glDrawArrays");
    glDrawElements = (PFNGLDRAWELEMENTSPROC)LL_GET_PROC_ADDRESS("glDrawElements");
    glGetPointerv = (PFNGLGETPOINTERVPROC)LL_GET_PROC_ADDRESS("glGetPointerv");
    glPolygonOffset = (PFNGLPOLYGONOFFSETPROC)LL_GET_PROC_ADDRESS("glPolygonOffset");
    glCopyTexImage1D = (PFNGLCOPYTEXIMAGE1DPROC)LL_GET_PROC_ADDRESS("glCopyTexImage1D");
    glCopyTexImage2D = (PFNGLCOPYTEXIMAGE2DPROC)LL_GET_PROC_ADDRESS("glCopyTexImage2D");
    glCopyTexSubImage1D = (PFNGLCOPYTEXSUBIMAGE1DPROC)LL_GET_PROC_ADDRESS("glCopyTexSubImage1D");
    glCopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC)LL_GET_PROC_ADDRESS("glCopyTexSubImage2D");
    glTexSubImage1D = (PFNGLTEXSUBIMAGE1DPROC)LL_GET_PROC_ADDRESS("glTexSubImage1D");
    glTexSubImage2D = (PFNGLTEXSUBIMAGE2DPROC)LL_GET_PROC_ADDRESS("glTexSubImage2D");
    glBindTexture = (PFNGLBINDTEXTUREPROC)LL_GET_PROC_ADDRESS("glBindTexture");
    glDeleteTextures = (PFNGLDELETETEXTURESPROC)LL_GET_PROC_ADDRESS("glDeleteTextures");
    glGenTextures = (PFNGLGENTEXTURESPROC)LL_GET_PROC_ADDRESS("glGenTextures");
    glIsTexture = (PFNGLISTEXTUREPROC)LL_GET_PROC_ADDRESS("glIsTexture");

    // Extract video card strings and convert to upper case to
    // work around driver-to-driver variation in capitalization.
    mGLVendorString = ll_safe_string((const char *)glGetString(GL_VENDOR));
    mGLVendor = mGLVendorString;
    LLStringUtil::toUpper(mGLVendor);

    mGLRendererString = ll_safe_string((const char *)glGetString(GL_RENDERER));
    mGLRenderer = mGLRendererString;
    LLStringUtil::toUpper(mGLRenderer);

    parse_gl_version( &mDriverVersionMajor,
        &mDriverVersionMinor,
        &mDriverVersionRelease,
        &mDriverVersionVendorString,
        &mGLVersionString);

    mGLVersion = mDriverVersionMajor + mDriverVersionMinor * .1f;

    {
        std::string renderer = ll_safe_string((const char *)glGetString(GL_RENDERER));
#if LL_LINUX
        renderer = strip_kernel_from_renderer(renderer);
#endif
        mGLIdentity = ll_safe_string((const char *)glGetString(GL_VENDOR)) + " " + renderer + " " + mGLVersionString;
    }

    // Every Mesa driver stamps "Mesa" into the GL_VERSION string
    // (e.g. "4.6 (Core Profile) Mesa 26.1.4"), regardless of the underlying
    // hardware backend. Detect it here so driver-specific workarounds can key
    // off mIsMesa. mGLVersionString is the raw (mixed-case) version string.
    {
        std::string ver_upper = mGLVersionString;
        LLStringUtil::toUpper(ver_upper);
        mIsMesa = ver_upper.find("MESA") != std::string::npos;
    }
    mIsD3D12 = mIsMesa && mGLRenderer.rfind("D3D12", 0) == 0;

#if LL_LINUX
    // When the hardware driver does not load -- NVIDIA's without its EGL platform library,
    // a missing Mesa driver, a VM or container without the GPU -- Mesa draws on the CPU
    // instead, offering GL 4.6 and every extension the viewer asks for, so nothing else
    // would tell. GL_RENDERER names what actually draws, and keeps the name when zink runs
    // over lavapipe or virgl over a host's llvmpipe.
    mIsSoftwareRenderer = false;
    for (const char* software : { "LLVMPIPE", "SOFTPIPE", "SWRAST", "SOFTWARE RASTERIZER",
                                  "MICROSOFT BASIC RENDER DRIVER" })
    {
        mIsSoftwareRenderer = mIsSoftwareRenderer || mGLRenderer.find(software) != std::string::npos;
    }
    if (mIsSoftwareRenderer)
    {
        LL_WARNS("RenderInit") << "GL renderer " << mGLRenderer << " draws on the CPU" << LL_ENDL;
    }
#endif

    // The floor (GL_MINIMUM_VERSION). Nothing past here adapts to a context below it, so it is
    // refused rather than run: the window tells the user what it lacks and the viewer quits.
    // mInited goes back to false: initExtensions sets it, and a window that drops the refused context
    // without shutdownGL would leave the next initGL taking itself for a second one.
    const auto refuse = [this](const char* missing)
    {
        mHasRequirements  = false;
        mUnmetRequirement = missing;
        mInited           = false;
        LL_WARNS("RenderInit") << "Refusing OpenGL " << mGLVersionString << " on " << mGLRenderer
                               << ": the viewer needs " << missing << LL_ENDL;
        return false;
    };

    if (!meetsVersionFloor(mGLVersion))
    {
        return refuse("OpenGL 4.1");
    }

    parse_glsl_version(mGLSLVersionMajor, mGLSLVersionMinor);

    // Trailing space necessary to keep "nVidia Corpor_ati_on" cards
    // from being recognized as ATI.
    // NOTE: AMD has been pretty good about not breaking this check, do not rename without good reason
    if (mGLVendor.substr(0,4) == "ATI "
#if LL_LINUX
         || mGLVendor.find("AMD") != std::string::npos
#endif //LL_LINUX
         )
    {
        mGLVendorShort = "AMD";
        // *TODO: Fix this?
        mIsAMD = true;
    }
    else if (mGLVendor.find("NVIDIA ") != std::string::npos)
    {
        mGLVendorShort = "NVIDIA";
        mIsNVIDIA = true;
    }
    else if (mGLVendor.find("INTEL") != std::string::npos
#if LL_LINUX
         // The Mesa-based drivers put this in the Renderer string,
         // not the Vendor string.
         || mGLRenderer.find("INTEL") != std::string::npos
#endif //LL_LINUX
         )
    {
        mGLVendorShort = "INTEL";
        mIsIntel = true;
    }
    else if (mGLVendor.find("APPLE") != std::string::npos)
    {
        mGLVendorShort = "APPLE";
        mIsApple = true;
    }
    else
    {
        mGLVendorShort = "MISC";
    }

    initExtensions();

    // The rest of the floor, once initExtensions has loaded what it could. Immutable texture
    // storage: every texture is allocated with glTexStorage* and written by sub-image thereafter,
    // which is the only model D3D11/12 and Vulkan have -- dimensions, format and mip count fixed at
    // creation. initExtensions resolves glTexStorage2D only where the context offers it.
    if (!glTexStorage2D)
    {
        return refuse("GL_ARB_texture_storage");
    }
    // sRGB decode: never promoted to core, so an extension at every version. Without it an
    // sRGB-format texture applies its transfer function on every read and there is no way to say
    // otherwise, which the renderer relies on being able to do -- see ALSampler::SRGBDecode.
    if (!mGLExtensions.contains("GL_EXT_texture_sRGB_decode"))
    {
        return refuse("GL_EXT_texture_sRGB_decode");
    }

    // A context that reports the floor and leaves one of its core calls unresolved is a broken
    // driver, not an old one, and nothing past here tests these before calling them. Refused with
    // nothing named, so the window says the driver did not install properly (MBVideoDrvErr).
    const std::pair<const char*, bool> core_calls[] = {
        { "glGenVertexArrays", glGenVertexArrays != nullptr },
        { "glPatchParameteri", glPatchParameteri != nullptr },
        { "glGenSamplers", glGenSamplers != nullptr },
        { "glSamplerParameteri", glSamplerParameteri != nullptr },
        { "glGetSamplerParameteriv", glGetSamplerParameteriv != nullptr },
        { "glBindSampler", glBindSampler != nullptr },
        { "glGetTexLevelParameteriv", glGetTexLevelParameteriv != nullptr },
        { "glGetInteger64v", glGetInteger64v != nullptr },
        { "glMapBufferRange", glMapBufferRange != nullptr },
        { "glFenceSync", glFenceSync != nullptr },
        { "glClientWaitSync", glClientWaitSync != nullptr },
        { "glDeleteSync", glDeleteSync != nullptr },
    };
    for (const auto& [name, resolved] : core_calls)
    {
        if (!resolved)
        {
            refuse(name);
            mUnmetRequirement.clear();
            return false;
        }
    }

    U32 old_vram = mVRAM;
    mVRAM = 0;

#if LL_WINDOWS
    if (mHasAMDAssociations)
    {
        GLuint gl_gpus_count = wglGetGPUIDsAMD(0, 0);
        if (gl_gpus_count > 0)
        {
            GLuint* ids = new GLuint[gl_gpus_count];
            wglGetGPUIDsAMD(gl_gpus_count, ids);

            GLuint mem_mb = 0;
            for (U32 i = 0; i < gl_gpus_count; i++)
            {
                wglGetGPUInfoAMD(ids[i],
                    WGL_GPU_RAM_AMD,
                    GL_UNSIGNED_INT,
                    sizeof(GLuint),
                    &mem_mb);
                if (mVRAM < mem_mb)
                {
                    // basically pick the best AMD and trust driver/OS to know to switch
                    mVRAM = mem_mb;
                }
            }
        }
        if (mVRAM != 0)
        {
            LL_INFOS("RenderInit") << "VRAM Detected (AMDAssociations):" << mVRAM << LL_ENDL;
        }
    } else
#endif

    // On Linux these two are what Mesa offers as well as the vendor drivers:
    // every gallium driver that can report memory exposes both, which is what
    // GLX_MESA_query_renderer used to be asked for on X11.
#if LL_WINDOWS || LL_LINUX
    {
        if (mHasNVXGpuMemoryInfo && mVRAM == 0)
        {
            GLint mem_kb = 0;
            glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &mem_kb);
            mVRAM = mem_kb / 1024;

            if (mVRAM != 0)
            {
                LL_INFOS("RenderInit") << "VRAM Detected (NVXGpuMemoryInfo):" << mVRAM << LL_ENDL;
            }
        }

        if (mHasATIMemInfo && mVRAM == 0)
        { //ask the gl how much vram is free at startup and attempt to use no more than half of that
            S32 meminfo[4];
            glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, meminfo);

            mVRAM = meminfo[0] / 1024;
            LL_INFOS("RenderInit") << "VRAM Detected (ATIMemInfo):" << mVRAM << LL_ENDL;
        }
    }
#endif

#if LL_LINUX
    // Neither extension counts memory a GPU shares with the system: Intel's and a software
    // renderer's report none, an APU's only its BIOS carve-out, often 512 MB -- so an AMD
    // figure under 1 GB is checked too. Windows asks DXGI instead.
    if (mVRAM < 256 || (mIsAMD && mVRAM < 1024))
    {
        std::string source;
        const U32 fallback_mb = fallback_vram_mb(mGLVendorShort, mIsIntel || mIsSoftwareRenderer, source);
        if (fallback_mb > mVRAM)
        {
            mVRAM = fallback_mb;
            LL_INFOS("RenderInit") << "VRAM Detected (" << source << "):" << mVRAM << LL_ENDL;
        }
    }
#endif

    if (mVRAM < 256 && old_vram > 0)
    {
        // fall back to old method
        mVRAM = old_vram;
    }

    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &mNumTextureImageUnits);
    glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &mMaxColorTextureSamples);
    glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &mMaxDepthTextureSamples);
    glGetIntegerv(GL_MAX_INTEGER_SAMPLES, &mMaxIntegerSamples);
    glGetIntegerv(GL_MAX_SAMPLE_MASK_WORDS, &mMaxSampleMaskWords);
    glGetIntegerv(GL_MAX_SAMPLES, &mMaxSamples);
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &mMaxVaryingVectors);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &mMaxUniformBlockSize);

    // If outside the allowed range, glLineWidth fails with "invalid value".
    glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, mAliasedLineRange.mV);

    // sanity clamp max uniform block size to 64k just in case
    // there's some implementation that reports a crazy value
    mMaxUniformBlockSize = llmin(mMaxUniformBlockSize, 65536);

    if (mHasAnisotropic)
    {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &mMaxAnisotropy);
    }

    initGLStates();

    return true;
}

void LLGLManager::getGLInfo(LLSD& info)
{
    info.erase("GLInfo");
    LLSD& renderer = info["RendererInfo"];
    renderer = LLSD::emptyMap();
    renderer["Backend"] = gRenderCapabilities.isMetal() ? "Metal" : "none";
    if (!gRenderCapabilities.isMetal())
    {
        return;
    }
    renderer["Vendor"] = gRenderCapabilities.vendor();
    renderer["Renderer"] = gRenderCapabilities.renderer();
    renderer["Version"] = gRenderCapabilities.version();
    renderer["Identity"] = gRenderCapabilities.identity();
    renderer["MemoryBudgetMB"] = LLSD::Integer(gRenderCapabilities.memoryBudgetMB());
    renderer["MaxTextureDimension"] = LLSD::Integer(gRenderCapabilities.maxTextureDimension());
}

std::string LLGLManager::getGLInfoString()
{
    if (!gRenderCapabilities.isMetal())
    {
        return "RENDERER_BACKEND none\n";
    }
    return "RENDERER_BACKEND Metal\nRENDERER_VENDOR " +
        gRenderCapabilities.vendor() + "\nRENDERER " +
        gRenderCapabilities.renderer() + "\nRENDERER_VERSION " +
        gRenderCapabilities.version() + "\nRENDERER_IDENTITY " +
        gRenderCapabilities.identity() + "\n";
}

void LLGLManager::printGLInfoString()
{
    LL_INFOS("RenderInit") << getGLInfoString() << LL_ENDL;
}

std::string LLGLManager::getRawGLString()
{
    return gRenderCapabilities.isMetal()
        ? gRenderCapabilities.identity() : "Renderer unavailable";
}

void LLGLManager::asLLSD(LLSD& info)
{
    // Do not carry driver-reported GL limits across a backend change or
    // label absent GL measurements as limits of the native Metal device.
    for (const char* key : {
             "opengl_version", "gl_renderer", "max_samples",
             "num_texture_image_units", "max_sample_mask_words",
             "max_color_texture_samples", "max_depth_texture_samples",
             "max_integer_samples", "max_vertex_range", "max_index_range",
             "has_compute", "max_compute_work_group_invocations",
             "max_compute_shared_memory_size", "max_image_units",
             "max_vertex_shader_storage_blocks",
             "max_fragment_shader_storage_blocks",
             "max_compute_shader_storage_blocks"})
    {
        info.erase(key);
    }
    info["renderer_backend"] = gRenderCapabilities.isMetal() ? "Metal" : "none";
    if (!gRenderCapabilities.isMetal())
    {
        for (const char* key : {
                 "gpu_vendor", "gpu_version", "renderer", "gpu_driver",
                 "vram", "max_texture_size", "is_ati", "is_nvidia", "is_intel"})
        {
            info.erase(key);
        }
        return;
    }
    info["gpu_vendor"] = gRenderCapabilities.vendor();
    info["gpu_version"] = gRenderCapabilities.version();
    info["renderer"] = gRenderCapabilities.renderer();
    info["gpu_driver"] = gRenderCapabilities.identity();
    info["vram"] = LLSD::Integer(gRenderCapabilities.memoryBudgetMB());
    info["max_texture_size"] = LLSD::Integer(gRenderCapabilities.maxTextureDimension());
    info["is_ati"] = gRenderCapabilities.isAMD();
    info["is_nvidia"] = gRenderCapabilities.isNVIDIA();
    info["is_intel"] = gRenderCapabilities.isIntel();
}

// static
bool LLGLManager::meetsVersionFloor(F32 gl_version)
{
    return gl_version >= GL_MINIMUM_VERSION;
}

bool LLGLManager::isIntelLegacy() const
{
#if LL_DARWIN
    return false;
#else
    return mIsIntel && mGLVersion < 4.59f;
#endif
}

void LLGLManager::shutdownGL()
{
    if (mInited)
    {
        glFinish();
        stop_glerror();
        mInited = false;
    }
}

// these are used to turn software blending on. They appear in the Debug/Avatar menu
// presence of vertex skinning/blending or vertex programs will set these to false by default.

void LLGLManager::reloadExtensionsString()
{
    mGLExtensions.clear();

    //reload extensions string
    // I am deeply sorry for how ugly this is. - Rye
    PFNGLGETSTRINGIPROC llglGetStringi = (PFNGLGETSTRINGIPROC)LL_GET_PROC_ADDRESS("glGetStringi");
    PFNGLGETINTEGERVPROC llglGetIntegerv = (PFNGLGETINTEGERVPROC)LL_GET_PROC_ADDRESS("glGetIntegerv");
    if (llglGetStringi && llglGetIntegerv)
    {
        GLint num_extensions = 0;
        llglGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
        for (GLint i = 0; i < num_extensions; ++i) {
            mGLExtensions.insert(ll_safe_string((char const*)llglGetStringi(GL_EXTENSIONS, i)));
        }
    }

#if LL_WINDOWS
    {
        PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)LL_GET_PROC_ADDRESS("wglGetExtensionsStringARB");
        if (wglGetExtensionsStringARB)
        {
            std::string wgl_exts = ll_safe_string((const char*)wglGetExtensionsStringARB(wglGetCurrentDC()));
            boost::char_separator<char> sep(" ");
            boost::tokenizer<boost::char_separator<char> > tok(wgl_exts, sep);
            for (boost::tokenizer<boost::char_separator<char> >::iterator i = tok.begin(); i != tok.end(); ++i)
            {
                mGLExtensions.insert(*i);
            }
        }
    }
#endif

#if LL_SDL_WINDOW && LL_LINUX
    {
        SDL_EGLDisplay egl_display = SDL_EGL_GetCurrentDisplay();
        if (egl_display)
        {
            PFNEGLQUERYSTRINGPROC lleglQueryString = (PFNEGLQUERYSTRINGPROC)SDL_EGL_GetProcAddress("eglQueryString");
            if (lleglQueryString)
            {
                std::string egl_exts = ll_safe_string((const char*)lleglQueryString((EGLDisplay)egl_display, EGL_EXTENSIONS));
                boost::char_separator<char> sep(" ");
                boost::tokenizer<boost::char_separator<char> > tok(egl_exts, sep);
                for (boost::tokenizer<boost::char_separator<char> >::iterator i = tok.begin(); i != tok.end(); ++i)
                {
                    mGLExtensions.insert(*i);
                }
            }
        }
    }
#endif
}

void LLGLManager::initExtensions()
{
    reloadExtensionsString();

    // NOTE: version checks against mGLVersion should bias down by 0.01 because of F32 errors

    // Past the floor: what a 4.1 context may or may not have.

    mHasAnisotropic = mGLVersion >= 4.59f;
    if(!mHasAnisotropic)
    {
        mHasAnisotropic = mGLExtensions.contains("GL_EXT_texture_filter_anisotropic");
    }

    // Core in 4.5; also an ARB extension. Downgraded below if the entry points don't resolve.
    mHasDirectStateAccess = mGLVersion >= 4.49f || mGLExtensions.contains("GL_ARB_direct_state_access");

    // Core in 4.5; also GL_ARB_clip_control. Gates reverse-Z. Downgraded below if glClipControl
    // does not resolve. macOS GL 4.1 lacks it entirely and stays forward-Z.
    mHasClipControl = mGLVersion >= 4.49f || mGLExtensions.contains("GL_ARB_clip_control");

    // Downgraded below if glDepthBoundsEXT does not resolve.
    mHasDepthBoundsTest = mGLExtensions.contains("GL_EXT_depth_bounds_test");

    mHasNVXGpuMemoryInfo = mGLExtensions.contains("GL_NVX_gpu_memory_info");
    mHasATIMemInfo = mGLExtensions.contains("GL_ATI_meminfo"); //Basic AMD method, also see mHasAMDAssociations

    // Misc
    glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, (GLint*) &mGLMaxVertexRange);
    glGetIntegerv(GL_MAX_ELEMENTS_INDICES, (GLint*) &mGLMaxIndexRange);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint*) &mGLMaxTextureSize);

    mInited = true;

    // Load the OpenGL API through GetProcAddress: all of it to 4.1, the floor, and leaving the
    // sections beyond mGLVersion unloaded.
    LL_DEBUGS("RenderInit") << "GL Probe: Getting symbols" << LL_ENDL;

    // Init EXT and ARB extensions FIRST, because they may be used in later GL versions

    // Immutable texture storage: core in 4.2, and GL_ARB_texture_storage on a 4.1 context (macOS,
    // GLOn12). Resolved here rather than left to the GL_VERSION_4_2 block below, which a 4.1
    // context returns before; on 4.2+ that block resolves the same pointers again, which is
    // harmless. Only where the context offers it, since initGL refuses one whose glTexStorage2D
    // stays null -- and cleared first, since a pointer outlives the context that resolved it.
    glTexStorage1D = nullptr;
    glTexStorage2D = nullptr;
    glTexStorage3D = nullptr;
    if (mGLVersion >= 4.19f || mGLExtensions.contains("GL_ARB_texture_storage"))
    {
        glTexStorage1D = (PFNGLTEXSTORAGE1DPROC)LL_GET_PROC_ADDRESS("glTexStorage1D");
        glTexStorage2D = (PFNGLTEXSTORAGE2DPROC)LL_GET_PROC_ADDRESS("glTexStorage2D");
        glTexStorage3D = (PFNGLTEXSTORAGE3DPROC)LL_GET_PROC_ADDRESS("glTexStorage3D");

        if (!glTexStorage2D)
        {
            LL_WARNS("RenderInit") << "Immutable texture storage advertised (GL " << mGLVersion
                                   << ") but glTexStorage2D did not resolve" << LL_ENDL;
        }
        else
        {
            LL_INFOS("RenderInit") << "Immutable texture storage available (GL " << mGLVersion
                                   << (mGLVersion >= 4.19f ? ", core)" : ", GL_ARB_texture_storage)")
                                   << LL_ENDL;
        }
    }

    // Immutable buffer storage. Resolved here for the same reason as glTexStorage above: the
    // GL_VERSION_4_4 ladder early-outs on a lower context, so a 4.1 driver exposing
    // GL_ARB_buffer_storage -- GLOn12 -- would otherwise leave glBufferStorage null, and every
    // persistently mapped uniform ring falls back to orphaning without it.
    if (mGLVersion >= 4.39f || mGLExtensions.contains("GL_ARB_buffer_storage"))
    {
        glBufferStorage = (PFNGLBUFFERSTORAGEPROC)LL_GET_PROC_ADDRESS("glBufferStorage");

        if (!glBufferStorage)
        {
            LL_WARNS("RenderInit") << "Buffer storage advertised (GL " << mGLVersion
                                   << ") but glBufferStorage did not resolve; uniform rings will orphan."
                                   << LL_ENDL;
        }
        else
        {
            LL_INFOS("RenderInit") << "Buffer storage available (GL " << mGLVersion
                                   << (mGLVersion >= 4.39f ? ", core)" : ", GL_ARB_buffer_storage)")
                                   << LL_ENDL;
        }
    }

    // Direct state access. Resolved here for the same reason as glTexStorage above: the
    // GL_VERSION_4_5 ladder early-outs on a lower context, so a 4.1/4.4 driver exposing
    // GL_ARB_direct_state_access would otherwise leave these null. Only the entry points
    // actually used are checked -- add to the check when more are adopted.
    if (mHasDirectStateAccess)
    {
        glBindTextureUnit = (PFNGLBINDTEXTUREUNITPROC)LL_GET_PROC_ADDRESS("glBindTextureUnit");
        glCreateSamplers  = (PFNGLCREATESAMPLERSPROC)LL_GET_PROC_ADDRESS("glCreateSamplers");

        // The named-buffer calls ALUniformBuffer makes. glNamedBufferStorage exists only
        // alongside buffer storage, so without it the pointer stays null and that class keeps
        // to the bind-target path.
        glCreateBuffers               = (PFNGLCREATEBUFFERSPROC)LL_GET_PROC_ADDRESS("glCreateBuffers");
        glNamedBufferData             = (PFNGLNAMEDBUFFERDATAPROC)LL_GET_PROC_ADDRESS("glNamedBufferData");
        glNamedBufferSubData          = (PFNGLNAMEDBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glNamedBufferSubData");
        glMapNamedBufferRange         = (PFNGLMAPNAMEDBUFFERRANGEPROC)LL_GET_PROC_ADDRESS("glMapNamedBufferRange");
        glUnmapNamedBuffer            = (PFNGLUNMAPNAMEDBUFFERPROC)LL_GET_PROC_ADDRESS("glUnmapNamedBuffer");
        glFlushMappedNamedBufferRange = (PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC)LL_GET_PROC_ADDRESS("glFlushMappedNamedBufferRange");
        if (glBufferStorage)
        {
            glNamedBufferStorage = (PFNGLNAMEDBUFFERSTORAGEPROC)LL_GET_PROC_ADDRESS("glNamedBufferStorage");
        }
        // And the ones ALGPUBuffer makes. Not part of the check below: glClearNamedBufferSubData exists only beside
        // GL_ARB_clear_buffer_object, and that class checks each before it uses it.
        glCopyNamedBufferSubData  = (PFNGLCOPYNAMEDBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glCopyNamedBufferSubData");
        glGetNamedBufferSubData   = (PFNGLGETNAMEDBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glGetNamedBufferSubData");
        glClearNamedBufferSubData = (PFNGLCLEARNAMEDBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glClearNamedBufferSubData");

        if (!glBindTextureUnit || !glCreateSamplers || !glCreateBuffers || !glNamedBufferData ||
            !glNamedBufferSubData || !glMapNamedBufferRange || !glUnmapNamedBuffer ||
            !glFlushMappedNamedBufferRange)
        {
            mHasDirectStateAccess = false;
            LL_WARNS("RenderInit") << "Direct state access advertised (GL " << mGLVersion
                                   << ") but its entry points did not resolve; using bind-to-edit."
                                   << LL_ENDL;
        }
        else
        {
            LL_INFOS("RenderInit") << "Direct state access available (GL " << mGLVersion
                                   << (mGLVersion >= 4.49f ? ", core)" : ", GL_ARB_direct_state_access)")
                                   << LL_ENDL;
        }
    }
    else
    {
        LL_INFOS("RenderInit") << "Direct state access unavailable (GL " << mGLVersion
                               << ", no GL_ARB_direct_state_access); using bind-to-edit." << LL_ENDL;
    }

    // Clip control. Resolved here for the same reason as glTexStorage/DSA above: the
    // GL_VERSION_4_5 ladder early-outs on a lower context, so a 4.1/4.4 driver exposing
    // GL_ARB_clip_control would otherwise leave glClipControl null.
    if (mHasClipControl)
    {
        glClipControl = (PFNGLCLIPCONTROLPROC)LL_GET_PROC_ADDRESS("glClipControl");

        if (!glClipControl)
        {
            mHasClipControl = false;
            LL_WARNS("RenderInit") << "Clip control advertised (GL " << mGLVersion
                                   << ") but glClipControl did not resolve; reverse-Z unavailable."
                                   << LL_ENDL;
        }
        else
        {
            LL_INFOS("RenderInit") << "Clip control available (GL " << mGLVersion
                                   << (mGLVersion >= 4.49f ? ", core)" : ", GL_ARB_clip_control)")
                                   << LL_ENDL;
        }
    }
    else
    {
        LL_INFOS("RenderInit") << "Clip control unavailable (GL " << mGLVersion
                               << ", no GL_ARB_clip_control); reverse-Z depth disabled." << LL_ENDL;
    }

    // Object labels. Resolved here for the same reason as glTexStorage
    // above: the GL_VERSION_4_3 ladder early-outs on a lower context, so a 4.1 driver
    // exposing GL_KHR_debug -- GLOn12 -- would otherwise leave these null.
    if (mGLVersion >= 4.29f || mGLExtensions.contains("GL_KHR_debug"))
    {
        glPushDebugGroup       = (PFNGLPUSHDEBUGGROUPPROC)LL_GET_PROC_ADDRESS("glPushDebugGroup");
        glPopDebugGroup        = (PFNGLPOPDEBUGGROUPPROC)LL_GET_PROC_ADDRESS("glPopDebugGroup");
        glObjectLabel          = (PFNGLOBJECTLABELPROC)LL_GET_PROC_ADDRESS("glObjectLabel");
        glGetObjectLabel       = (PFNGLGETOBJECTLABELPROC)LL_GET_PROC_ADDRESS("glGetObjectLabel");
        glObjectPtrLabel       = (PFNGLOBJECTPTRLABELPROC)LL_GET_PROC_ADDRESS("glObjectPtrLabel");
        glGetObjectPtrLabel    = (PFNGLGETOBJECTPTRLABELPROC)LL_GET_PROC_ADDRESS("glGetObjectPtrLabel");

    }

    // Reset status. Core in 4.5, and GL_KHR_robustness names it without a suffix on a lower
    // context, so it is resolved here for the GL_VERSION_4_5 ladder's early-out as above.
    // GL_ARB_robustness alone names it glGetGraphicsResetStatusARB, which the window layer
    // looks up itself.
    if (mGLVersion >= 4.49f || mGLExtensions.contains("GL_KHR_robustness"))
    {
        glGetGraphicsResetStatus = (PFNGLGETGRAPHICSRESETSTATUSPROC)LL_GET_PROC_ADDRESS("glGetGraphicsResetStatus");
        glReadnPixels            = (PFNGLREADNPIXELSPROC)LL_GET_PROC_ADDRESS("glReadnPixels");
    }

    // Texture barrier. Core in 4.5, and GL_ARB_texture_barrier names it without a suffix on a
    // lower context, so it is resolved here for the GL_VERSION_4_5 ladder's early-out as above.
    // GL_NV_texture_barrier, which ARB's took over unchanged, names it glTextureBarrierNV. Cleared first, since a
    // pointer outlives the context that resolved it, and mHasTextureBarrier says what this one offers.
    glTextureBarrier = nullptr;
    if (mGLVersion >= 4.49f || mGLExtensions.contains("GL_ARB_texture_barrier"))
    {
        glTextureBarrier = (PFNGLTEXTUREBARRIERPROC)LL_GET_PROC_ADDRESS("glTextureBarrier");
    }
    if (!glTextureBarrier && mGLExtensions.contains("GL_NV_texture_barrier"))
    {
        glTextureBarrier = (PFNGLTEXTUREBARRIERPROC)LL_GET_PROC_ADDRESS("glTextureBarrierNV");
    }
    mHasTextureBarrier = glTextureBarrier != nullptr;
    LL_INFOS("RenderInit") << "Texture barrier " << (mHasTextureBarrier ? "available" : "unavailable") << LL_ENDL;

    // GL_EXT_depth_bounds_test. Never core; a context has it or it does not.
    if (mHasDepthBoundsTest)
    {
        glDepthBoundsEXT = (PFNGLDEPTHBOUNDSEXTPROC)LL_GET_PROC_ADDRESS("glDepthBoundsEXT");
        if (!glDepthBoundsEXT)
        {
            mHasDepthBoundsTest = false;
            LL_WARNS("RenderInit") << "GL_EXT_depth_bounds_test advertised but glDepthBoundsEXT did not resolve" << LL_ENDL;
        }
    }
    LL_INFOS("RenderInit") << "Depth bounds test " << (mHasDepthBoundsTest ? "available" : "unavailable") << LL_ENDL;

    // GL_VERSION_1_2
    glDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC)LL_GET_PROC_ADDRESS("glDrawRangeElements");
    glTexImage3D = (PFNGLTEXIMAGE3DPROC)LL_GET_PROC_ADDRESS("glTexImage3D");
    glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC)LL_GET_PROC_ADDRESS("glTexSubImage3D");
    glCopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC)LL_GET_PROC_ADDRESS("glCopyTexSubImage3D");

    // GL_VERSION_1_3
    glActiveTexture = (PFNGLACTIVETEXTUREPROC)LL_GET_PROC_ADDRESS("glActiveTexture");
    glSampleCoverage = (PFNGLSAMPLECOVERAGEPROC)LL_GET_PROC_ADDRESS("glSampleCoverage");
    glCompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC)LL_GET_PROC_ADDRESS("glCompressedTexImage3D");
    glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)LL_GET_PROC_ADDRESS("glCompressedTexImage2D");
    glCompressedTexImage1D = (PFNGLCOMPRESSEDTEXIMAGE1DPROC)LL_GET_PROC_ADDRESS("glCompressedTexImage1D");
    glCompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)LL_GET_PROC_ADDRESS("glCompressedTexSubImage3D");
    glCompressedTexSubImage2D = (PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)LL_GET_PROC_ADDRESS("glCompressedTexSubImage2D");
    glCompressedTexSubImage1D = (PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)LL_GET_PROC_ADDRESS("glCompressedTexSubImage1D");
    glGetCompressedTexImage = (PFNGLGETCOMPRESSEDTEXIMAGEPROC)LL_GET_PROC_ADDRESS("glGetCompressedTexImage");

    // GL_VERSION_1_4
    glBlendFuncSeparate = (PFNGLBLENDFUNCSEPARATEPROC)LL_GET_PROC_ADDRESS("glBlendFuncSeparate");
    glMultiDrawArrays = (PFNGLMULTIDRAWARRAYSPROC)LL_GET_PROC_ADDRESS("glMultiDrawArrays");
    glMultiDrawElements = (PFNGLMULTIDRAWELEMENTSPROC)LL_GET_PROC_ADDRESS("glMultiDrawElements");
    glPointParameterf = (PFNGLPOINTPARAMETERFPROC)LL_GET_PROC_ADDRESS("glPointParameterf");
    glPointParameterfv = (PFNGLPOINTPARAMETERFVPROC)LL_GET_PROC_ADDRESS("glPointParameterfv");
    glPointParameteri = (PFNGLPOINTPARAMETERIPROC)LL_GET_PROC_ADDRESS("glPointParameteri");
    glPointParameteriv = (PFNGLPOINTPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glPointParameteriv");

    // GL_VERSION_1_5
    glBindBuffer = (PFNGLBINDBUFFERPROC)LL_GET_PROC_ADDRESS("glBindBuffer");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)LL_GET_PROC_ADDRESS("glDeleteBuffers");
    glGenBuffers = (PFNGLGENBUFFERSPROC)LL_GET_PROC_ADDRESS("glGenBuffers");
    glIsBuffer = (PFNGLISBUFFERPROC)LL_GET_PROC_ADDRESS("glIsBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)LL_GET_PROC_ADDRESS("glBufferData");
    glBufferSubData = (PFNGLBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glBufferSubData");
    glGetBufferSubData = (PFNGLGETBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glGetBufferSubData");
    glMapBuffer = (PFNGLMAPBUFFERPROC)LL_GET_PROC_ADDRESS("glMapBuffer");
    glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)LL_GET_PROC_ADDRESS("glUnmapBuffer");
    glGetBufferParameteriv = (PFNGLGETBUFFERPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetBufferParameteriv");
    glGetBufferPointerv = (PFNGLGETBUFFERPOINTERVPROC)LL_GET_PROC_ADDRESS("glGetBufferPointerv");

    // GL_VERSION_2_0
    glBlendEquationSeparate = (PFNGLBLENDEQUATIONSEPARATEPROC)LL_GET_PROC_ADDRESS("glBlendEquationSeparate");
    glDrawBuffers = (PFNGLDRAWBUFFERSPROC)LL_GET_PROC_ADDRESS("glDrawBuffers");
    glStencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC)LL_GET_PROC_ADDRESS("glStencilOpSeparate");
    glStencilFuncSeparate = (PFNGLSTENCILFUNCSEPARATEPROC)LL_GET_PROC_ADDRESS("glStencilFuncSeparate");
    glStencilMaskSeparate = (PFNGLSTENCILMASKSEPARATEPROC)LL_GET_PROC_ADDRESS("glStencilMaskSeparate");
    glDisableVertexAttribArray = (PFNGLDISABLEVERTEXATTRIBARRAYPROC)LL_GET_PROC_ADDRESS("glDisableVertexAttribArray");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)LL_GET_PROC_ADDRESS("glEnableVertexAttribArray");
    glGetVertexAttribdv = (PFNGLGETVERTEXATTRIBDVPROC)LL_GET_PROC_ADDRESS("glGetVertexAttribdv");
    glGetVertexAttribfv = (PFNGLGETVERTEXATTRIBFVPROC)LL_GET_PROC_ADDRESS("glGetVertexAttribfv");
    glGetVertexAttribiv = (PFNGLGETVERTEXATTRIBIVPROC)LL_GET_PROC_ADDRESS("glGetVertexAttribiv");
    glGetVertexAttribPointerv = (PFNGLGETVERTEXATTRIBPOINTERVPROC)LL_GET_PROC_ADDRESS("glGetVertexAttribPointerv");
    glVertexAttrib1d = (PFNGLVERTEXATTRIB1DPROC)LL_GET_PROC_ADDRESS("glVertexAttrib1d");
    glVertexAttrib1dv = (PFNGLVERTEXATTRIB1DVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib1dv");
    glVertexAttrib1f = (PFNGLVERTEXATTRIB1FPROC)LL_GET_PROC_ADDRESS("glVertexAttrib1f");
    glVertexAttrib1fv = (PFNGLVERTEXATTRIB1FVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib1fv");
    glVertexAttrib1s = (PFNGLVERTEXATTRIB1SPROC)LL_GET_PROC_ADDRESS("glVertexAttrib1s");
    glVertexAttrib1sv = (PFNGLVERTEXATTRIB1SVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib1sv");
    glVertexAttrib2d = (PFNGLVERTEXATTRIB2DPROC)LL_GET_PROC_ADDRESS("glVertexAttrib2d");
    glVertexAttrib2dv = (PFNGLVERTEXATTRIB2DVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib2dv");
    glVertexAttrib2f = (PFNGLVERTEXATTRIB2FPROC)LL_GET_PROC_ADDRESS("glVertexAttrib2f");
    glVertexAttrib2fv = (PFNGLVERTEXATTRIB2FVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib2fv");
    glVertexAttrib2s = (PFNGLVERTEXATTRIB2SPROC)LL_GET_PROC_ADDRESS("glVertexAttrib2s");
    glVertexAttrib2sv = (PFNGLVERTEXATTRIB2SVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib2sv");
    glVertexAttrib3d = (PFNGLVERTEXATTRIB3DPROC)LL_GET_PROC_ADDRESS("glVertexAttrib3d");
    glVertexAttrib3dv = (PFNGLVERTEXATTRIB3DVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib3dv");
    glVertexAttrib3f = (PFNGLVERTEXATTRIB3FPROC)LL_GET_PROC_ADDRESS("glVertexAttrib3f");
    glVertexAttrib3fv = (PFNGLVERTEXATTRIB3FVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib3fv");
    glVertexAttrib3s = (PFNGLVERTEXATTRIB3SPROC)LL_GET_PROC_ADDRESS("glVertexAttrib3s");
    glVertexAttrib3sv = (PFNGLVERTEXATTRIB3SVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib3sv");
    glVertexAttrib4Nbv = (PFNGLVERTEXATTRIB4NBVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4Nbv");
    glVertexAttrib4Niv = (PFNGLVERTEXATTRIB4NIVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4Niv");
    glVertexAttrib4Nsv = (PFNGLVERTEXATTRIB4NSVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4Nsv");
    glVertexAttrib4Nub = (PFNGLVERTEXATTRIB4NUBPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4Nub");
    glVertexAttrib4Nubv = (PFNGLVERTEXATTRIB4NUBVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4Nubv");
    glVertexAttrib4Nuiv = (PFNGLVERTEXATTRIB4NUIVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4Nuiv");
    glVertexAttrib4Nusv = (PFNGLVERTEXATTRIB4NUSVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4Nusv");
    glVertexAttrib4bv = (PFNGLVERTEXATTRIB4BVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4bv");
    glVertexAttrib4d = (PFNGLVERTEXATTRIB4DPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4d");
    glVertexAttrib4dv = (PFNGLVERTEXATTRIB4DVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4dv");
    glVertexAttrib4f = (PFNGLVERTEXATTRIB4FPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4f");
    glVertexAttrib4fv = (PFNGLVERTEXATTRIB4FVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4fv");
    glVertexAttrib4iv = (PFNGLVERTEXATTRIB4IVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4iv");
    glVertexAttrib4s = (PFNGLVERTEXATTRIB4SPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4s");
    glVertexAttrib4sv = (PFNGLVERTEXATTRIB4SVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4sv");
    glVertexAttrib4ubv = (PFNGLVERTEXATTRIB4UBVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4ubv");
    glVertexAttrib4uiv = (PFNGLVERTEXATTRIB4UIVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4uiv");
    glVertexAttrib4usv = (PFNGLVERTEXATTRIB4USVPROC)LL_GET_PROC_ADDRESS("glVertexAttrib4usv");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)LL_GET_PROC_ADDRESS("glVertexAttribPointer");

    // GL_VERSION_2_1

    // GL_VERSION_3_0
    glColorMaski = (PFNGLCOLORMASKIPROC)LL_GET_PROC_ADDRESS("glColorMaski");
    glGetBooleani_v = (PFNGLGETBOOLEANI_VPROC)LL_GET_PROC_ADDRESS("glGetBooleani_v");
    glGetIntegeri_v = (PFNGLGETINTEGERI_VPROC)LL_GET_PROC_ADDRESS("glGetIntegeri_v");
    glEnablei = (PFNGLENABLEIPROC)LL_GET_PROC_ADDRESS("glEnablei");
    glDisablei = (PFNGLDISABLEIPROC)LL_GET_PROC_ADDRESS("glDisablei");
    glIsEnabledi = (PFNGLISENABLEDIPROC)LL_GET_PROC_ADDRESS("glIsEnabledi");
    glBeginTransformFeedback = (PFNGLBEGINTRANSFORMFEEDBACKPROC)LL_GET_PROC_ADDRESS("glBeginTransformFeedback");
    glEndTransformFeedback = (PFNGLENDTRANSFORMFEEDBACKPROC)LL_GET_PROC_ADDRESS("glEndTransformFeedback");
    glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)LL_GET_PROC_ADDRESS("glBindBufferRange");
    glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)LL_GET_PROC_ADDRESS("glBindBufferBase");
    glTransformFeedbackVaryings = (PFNGLTRANSFORMFEEDBACKVARYINGSPROC)LL_GET_PROC_ADDRESS("glTransformFeedbackVaryings");
    glGetTransformFeedbackVarying = (PFNGLGETTRANSFORMFEEDBACKVARYINGPROC)LL_GET_PROC_ADDRESS("glGetTransformFeedbackVarying");
    glClampColor = (PFNGLCLAMPCOLORPROC)LL_GET_PROC_ADDRESS("glClampColor");
    glBeginConditionalRender = (PFNGLBEGINCONDITIONALRENDERPROC)LL_GET_PROC_ADDRESS("glBeginConditionalRender");
    glEndConditionalRender = (PFNGLENDCONDITIONALRENDERPROC)LL_GET_PROC_ADDRESS("glEndConditionalRender");
    glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC)LL_GET_PROC_ADDRESS("glVertexAttribIPointer");
    glGetVertexAttribIiv = (PFNGLGETVERTEXATTRIBIIVPROC)LL_GET_PROC_ADDRESS("glGetVertexAttribIiv");
    glGetVertexAttribIuiv = (PFNGLGETVERTEXATTRIBIUIVPROC)LL_GET_PROC_ADDRESS("glGetVertexAttribIuiv");
    glVertexAttribI1i = (PFNGLVERTEXATTRIBI1IPROC)LL_GET_PROC_ADDRESS("glVertexAttribI1i");
    glVertexAttribI2i = (PFNGLVERTEXATTRIBI2IPROC)LL_GET_PROC_ADDRESS("glVertexAttribI2i");
    glVertexAttribI3i = (PFNGLVERTEXATTRIBI3IPROC)LL_GET_PROC_ADDRESS("glVertexAttribI3i");
    glVertexAttribI4i = (PFNGLVERTEXATTRIBI4IPROC)LL_GET_PROC_ADDRESS("glVertexAttribI4i");
    glVertexAttribI1ui = (PFNGLVERTEXATTRIBI1UIPROC)LL_GET_PROC_ADDRESS("glVertexAttribI1ui");
    glVertexAttribI2ui = (PFNGLVERTEXATTRIBI2UIPROC)LL_GET_PROC_ADDRESS("glVertexAttribI2ui");
    glVertexAttribI3ui = (PFNGLVERTEXATTRIBI3UIPROC)LL_GET_PROC_ADDRESS("glVertexAttribI3ui");
    glVertexAttribI4ui = (PFNGLVERTEXATTRIBI4UIPROC)LL_GET_PROC_ADDRESS("glVertexAttribI4ui");
    glVertexAttribI1iv = (PFNGLVERTEXATTRIBI1IVPROC)LL_GET_PROC_ADDRESS("glVertexAttribI1iv");
    glVertexAttribI2iv = (PFNGLVERTEXATTRIBI2IVPROC)LL_GET_PROC_ADDRESS("glVertexAttribI2iv");
    glVertexAttribI3iv = (PFNGLVERTEXATTRIBI3IVPROC)LL_GET_PROC_ADDRESS("glVertexAttribI3iv");
    glVertexAttribI4iv = (PFNGLVERTEXATTRIBI4IVPROC)LL_GET_PROC_ADDRESS("glVertexAttribI4iv");
    glVertexAttribI1uiv = (PFNGLVERTEXATTRIBI1UIVPROC)LL_GET_PROC_ADDRESS("glVertexAttribI1uiv");
    glVertexAttribI2uiv = (PFNGLVERTEXATTRIBI2UIVPROC)LL_GET_PROC_ADDRESS("glVertexAttribI2uiv");
    glVertexAttribI3uiv = (PFNGLVERTEXATTRIBI3UIVPROC)LL_GET_PROC_ADDRESS("glVertexAttribI3uiv");
    glVertexAttribI4uiv = (PFNGLVERTEXATTRIBI4UIVPROC)LL_GET_PROC_ADDRESS("glVertexAttribI4uiv");
    glVertexAttribI4bv = (PFNGLVERTEXATTRIBI4BVPROC)LL_GET_PROC_ADDRESS("glVertexAttribI4bv");
    glVertexAttribI4sv = (PFNGLVERTEXATTRIBI4SVPROC)LL_GET_PROC_ADDRESS("glVertexAttribI4sv");
    glVertexAttribI4ubv = (PFNGLVERTEXATTRIBI4UBVPROC)LL_GET_PROC_ADDRESS("glVertexAttribI4ubv");
    glVertexAttribI4usv = (PFNGLVERTEXATTRIBI4USVPROC)LL_GET_PROC_ADDRESS("glVertexAttribI4usv");
    glBindFragDataLocation = (PFNGLBINDFRAGDATALOCATIONPROC)LL_GET_PROC_ADDRESS("glBindFragDataLocation");
    glGetFragDataLocation = (PFNGLGETFRAGDATALOCATIONPROC)LL_GET_PROC_ADDRESS("glGetFragDataLocation");
    glTexParameterIiv = (PFNGLTEXPARAMETERIIVPROC)LL_GET_PROC_ADDRESS("glTexParameterIiv");
    glTexParameterIuiv = (PFNGLTEXPARAMETERIUIVPROC)LL_GET_PROC_ADDRESS("glTexParameterIuiv");
    glGetTexParameterIiv = (PFNGLGETTEXPARAMETERIIVPROC)LL_GET_PROC_ADDRESS("glGetTexParameterIiv");
    glGetTexParameterIuiv = (PFNGLGETTEXPARAMETERIUIVPROC)LL_GET_PROC_ADDRESS("glGetTexParameterIuiv");
    glClearBufferiv = (PFNGLCLEARBUFFERIVPROC)LL_GET_PROC_ADDRESS("glClearBufferiv");
    glClearBufferuiv = (PFNGLCLEARBUFFERUIVPROC)LL_GET_PROC_ADDRESS("glClearBufferuiv");
    glClearBufferfv = (PFNGLCLEARBUFFERFVPROC)LL_GET_PROC_ADDRESS("glClearBufferfv");
    glClearBufferfi = (PFNGLCLEARBUFFERFIPROC)LL_GET_PROC_ADDRESS("glClearBufferfi");
    glGetStringi = (PFNGLGETSTRINGIPROC)LL_GET_PROC_ADDRESS("glGetStringi");
    glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC)LL_GET_PROC_ADDRESS("glIsRenderbuffer");
    glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC)LL_GET_PROC_ADDRESS("glBindRenderbuffer");
    glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC)LL_GET_PROC_ADDRESS("glDeleteRenderbuffers");
    glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC)LL_GET_PROC_ADDRESS("glGenRenderbuffers");
    glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC)LL_GET_PROC_ADDRESS("glRenderbufferStorage");
    glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetRenderbufferParameteriv");
    glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC)LL_GET_PROC_ADDRESS("glIsFramebuffer");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)LL_GET_PROC_ADDRESS("glBindFramebuffer");
    glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)LL_GET_PROC_ADDRESS("glDeleteFramebuffers");
    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)LL_GET_PROC_ADDRESS("glGenFramebuffers");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)LL_GET_PROC_ADDRESS("glCheckFramebufferStatus");
    glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DPROC)LL_GET_PROC_ADDRESS("glFramebufferTexture1D");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)LL_GET_PROC_ADDRESS("glFramebufferTexture2D");
    glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DPROC)LL_GET_PROC_ADDRESS("glFramebufferTexture3D");
    glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)LL_GET_PROC_ADDRESS("glFramebufferRenderbuffer");
    glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetFramebufferAttachmentParameteriv");
    glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)LL_GET_PROC_ADDRESS("glGenerateMipmap");
    glBlitFramebuffer = (PFNGLBLITFRAMEBUFFERPROC)LL_GET_PROC_ADDRESS("glBlitFramebuffer");
    glRenderbufferStorageMultisample = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC)LL_GET_PROC_ADDRESS("glRenderbufferStorageMultisample");
    glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC)LL_GET_PROC_ADDRESS("glFramebufferTextureLayer");
    glMapBufferRange = (PFNGLMAPBUFFERRANGEPROC)LL_GET_PROC_ADDRESS("glMapBufferRange");
    glFlushMappedBufferRange = (PFNGLFLUSHMAPPEDBUFFERRANGEPROC)LL_GET_PROC_ADDRESS("glFlushMappedBufferRange");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)LL_GET_PROC_ADDRESS("glBindVertexArray");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)LL_GET_PROC_ADDRESS("glDeleteVertexArrays");
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)LL_GET_PROC_ADDRESS("glGenVertexArrays");
    glIsVertexArray = (PFNGLISVERTEXARRAYPROC)LL_GET_PROC_ADDRESS("glIsVertexArray");

    // GL_VERSION_3_1
    glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)LL_GET_PROC_ADDRESS("glDrawArraysInstanced");
    glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC)LL_GET_PROC_ADDRESS("glDrawElementsInstanced");
    glTexBuffer = (PFNGLTEXBUFFERPROC)LL_GET_PROC_ADDRESS("glTexBuffer");
    glPrimitiveRestartIndex = (PFNGLPRIMITIVERESTARTINDEXPROC)LL_GET_PROC_ADDRESS("glPrimitiveRestartIndex");
    glCopyBufferSubData = (PFNGLCOPYBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glCopyBufferSubData");

    // GL_VERSION_3_2
    glDrawElementsBaseVertex = (PFNGLDRAWELEMENTSBASEVERTEXPROC)LL_GET_PROC_ADDRESS("glDrawElementsBaseVertex");
    glDrawRangeElementsBaseVertex = (PFNGLDRAWRANGEELEMENTSBASEVERTEXPROC)LL_GET_PROC_ADDRESS("glDrawRangeElementsBaseVertex");
    glDrawElementsInstancedBaseVertex = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXPROC)LL_GET_PROC_ADDRESS("glDrawElementsInstancedBaseVertex");
    glMultiDrawElementsBaseVertex = (PFNGLMULTIDRAWELEMENTSBASEVERTEXPROC)LL_GET_PROC_ADDRESS("glMultiDrawElementsBaseVertex");
    glProvokingVertex = (PFNGLPROVOKINGVERTEXPROC)LL_GET_PROC_ADDRESS("glProvokingVertex");
    glFenceSync = (PFNGLFENCESYNCPROC)LL_GET_PROC_ADDRESS("glFenceSync");
    glIsSync = (PFNGLISSYNCPROC)LL_GET_PROC_ADDRESS("glIsSync");
    glDeleteSync = (PFNGLDELETESYNCPROC)LL_GET_PROC_ADDRESS("glDeleteSync");
    glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC)LL_GET_PROC_ADDRESS("glClientWaitSync");
    glWaitSync = (PFNGLWAITSYNCPROC)LL_GET_PROC_ADDRESS("glWaitSync");
    glGetInteger64v = (PFNGLGETINTEGER64VPROC)LL_GET_PROC_ADDRESS("glGetInteger64v");
    glGetSynciv = (PFNGLGETSYNCIVPROC)LL_GET_PROC_ADDRESS("glGetSynciv");
    glGetInteger64i_v = (PFNGLGETINTEGER64I_VPROC)LL_GET_PROC_ADDRESS("glGetInteger64i_v");
    glGetBufferParameteri64v = (PFNGLGETBUFFERPARAMETERI64VPROC)LL_GET_PROC_ADDRESS("glGetBufferParameteri64v");
    glFramebufferTexture = (PFNGLFRAMEBUFFERTEXTUREPROC)LL_GET_PROC_ADDRESS("glFramebufferTexture");
    glTexImage2DMultisample = (PFNGLTEXIMAGE2DMULTISAMPLEPROC)LL_GET_PROC_ADDRESS("glTexImage2DMultisample");
    glTexImage3DMultisample = (PFNGLTEXIMAGE3DMULTISAMPLEPROC)LL_GET_PROC_ADDRESS("glTexImage3DMultisample");
    glGetMultisamplefv = (PFNGLGETMULTISAMPLEFVPROC)LL_GET_PROC_ADDRESS("glGetMultisamplefv");
    glSampleMaski = (PFNGLSAMPLEMASKIPROC)LL_GET_PROC_ADDRESS("glSampleMaski");

    // GL_VERSION_3_3
    glBindFragDataLocationIndexed = (PFNGLBINDFRAGDATALOCATIONINDEXEDPROC)LL_GET_PROC_ADDRESS("glBindFragDataLocationIndexed");
    glGetFragDataIndex = (PFNGLGETFRAGDATAINDEXPROC)LL_GET_PROC_ADDRESS("glGetFragDataIndex");
    glGenSamplers = (PFNGLGENSAMPLERSPROC)LL_GET_PROC_ADDRESS("glGenSamplers");
    glDeleteSamplers = (PFNGLDELETESAMPLERSPROC)LL_GET_PROC_ADDRESS("glDeleteSamplers");
    glIsSampler = (PFNGLISSAMPLERPROC)LL_GET_PROC_ADDRESS("glIsSampler");
    glBindSampler = (PFNGLBINDSAMPLERPROC)LL_GET_PROC_ADDRESS("glBindSampler");
    glSamplerParameteri = (PFNGLSAMPLERPARAMETERIPROC)LL_GET_PROC_ADDRESS("glSamplerParameteri");
    glSamplerParameteriv = (PFNGLSAMPLERPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glSamplerParameteriv");
    glSamplerParameterf = (PFNGLSAMPLERPARAMETERFPROC)LL_GET_PROC_ADDRESS("glSamplerParameterf");
    glSamplerParameterfv = (PFNGLSAMPLERPARAMETERFVPROC)LL_GET_PROC_ADDRESS("glSamplerParameterfv");
    glSamplerParameterIiv = (PFNGLSAMPLERPARAMETERIIVPROC)LL_GET_PROC_ADDRESS("glSamplerParameterIiv");
    glSamplerParameterIuiv = (PFNGLSAMPLERPARAMETERIUIVPROC)LL_GET_PROC_ADDRESS("glSamplerParameterIuiv");
    glGetSamplerParameteriv = (PFNGLGETSAMPLERPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetSamplerParameteriv");
    glGetSamplerParameterIiv = (PFNGLGETSAMPLERPARAMETERIIVPROC)LL_GET_PROC_ADDRESS("glGetSamplerParameterIiv");
    glGetSamplerParameterfv = (PFNGLGETSAMPLERPARAMETERFVPROC)LL_GET_PROC_ADDRESS("glGetSamplerParameterfv");
    glGetSamplerParameterIuiv = (PFNGLGETSAMPLERPARAMETERIUIVPROC)LL_GET_PROC_ADDRESS("glGetSamplerParameterIuiv");
    glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)LL_GET_PROC_ADDRESS("glVertexAttribDivisor");
    glVertexAttribP1ui = (PFNGLVERTEXATTRIBP1UIPROC)LL_GET_PROC_ADDRESS("glVertexAttribP1ui");
    glVertexAttribP1uiv = (PFNGLVERTEXATTRIBP1UIVPROC)LL_GET_PROC_ADDRESS("glVertexAttribP1uiv");
    glVertexAttribP2ui = (PFNGLVERTEXATTRIBP2UIPROC)LL_GET_PROC_ADDRESS("glVertexAttribP2ui");
    glVertexAttribP2uiv = (PFNGLVERTEXATTRIBP2UIVPROC)LL_GET_PROC_ADDRESS("glVertexAttribP2uiv");
    glVertexAttribP3ui = (PFNGLVERTEXATTRIBP3UIPROC)LL_GET_PROC_ADDRESS("glVertexAttribP3ui");
    glVertexAttribP3uiv = (PFNGLVERTEXATTRIBP3UIVPROC)LL_GET_PROC_ADDRESS("glVertexAttribP3uiv");
    glVertexAttribP4ui = (PFNGLVERTEXATTRIBP4UIPROC)LL_GET_PROC_ADDRESS("glVertexAttribP4ui");
    glVertexAttribP4uiv = (PFNGLVERTEXATTRIBP4UIVPROC)LL_GET_PROC_ADDRESS("glVertexAttribP4uiv");

    // GL_VERSION_4_0
    glMinSampleShading = (PFNGLMINSAMPLESHADINGPROC)LL_GET_PROC_ADDRESS("glMinSampleShading");
    glBlendEquationi = (PFNGLBLENDEQUATIONIPROC)LL_GET_PROC_ADDRESS("glBlendEquationi");
    glBlendEquationSeparatei = (PFNGLBLENDEQUATIONSEPARATEIPROC)LL_GET_PROC_ADDRESS("glBlendEquationSeparatei");
    glBlendFunci = (PFNGLBLENDFUNCIPROC)LL_GET_PROC_ADDRESS("glBlendFunci");
    glBlendFuncSeparatei = (PFNGLBLENDFUNCSEPARATEIPROC)LL_GET_PROC_ADDRESS("glBlendFuncSeparatei");
    glDrawArraysIndirect = (PFNGLDRAWARRAYSINDIRECTPROC)LL_GET_PROC_ADDRESS("glDrawArraysIndirect");
    glDrawElementsIndirect = (PFNGLDRAWELEMENTSINDIRECTPROC)LL_GET_PROC_ADDRESS("glDrawElementsIndirect");
    glGetSubroutineIndex = (PFNGLGETSUBROUTINEINDEXPROC)LL_GET_PROC_ADDRESS("glGetSubroutineIndex");
    glGetActiveSubroutineName = (PFNGLGETACTIVESUBROUTINENAMEPROC)LL_GET_PROC_ADDRESS("glGetActiveSubroutineName");
    glPatchParameteri = (PFNGLPATCHPARAMETERIPROC)LL_GET_PROC_ADDRESS("glPatchParameteri");
    glPatchParameterfv = (PFNGLPATCHPARAMETERFVPROC)LL_GET_PROC_ADDRESS("glPatchParameterfv");
    glBindTransformFeedback = (PFNGLBINDTRANSFORMFEEDBACKPROC)LL_GET_PROC_ADDRESS("glBindTransformFeedback");
    glDeleteTransformFeedbacks = (PFNGLDELETETRANSFORMFEEDBACKSPROC)LL_GET_PROC_ADDRESS("glDeleteTransformFeedbacks");
    glGenTransformFeedbacks = (PFNGLGENTRANSFORMFEEDBACKSPROC)LL_GET_PROC_ADDRESS("glGenTransformFeedbacks");
    glIsTransformFeedback = (PFNGLISTRANSFORMFEEDBACKPROC)LL_GET_PROC_ADDRESS("glIsTransformFeedback");
    glPauseTransformFeedback = (PFNGLPAUSETRANSFORMFEEDBACKPROC)LL_GET_PROC_ADDRESS("glPauseTransformFeedback");
    glResumeTransformFeedback = (PFNGLRESUMETRANSFORMFEEDBACKPROC)LL_GET_PROC_ADDRESS("glResumeTransformFeedback");
    glDrawTransformFeedback = (PFNGLDRAWTRANSFORMFEEDBACKPROC)LL_GET_PROC_ADDRESS("glDrawTransformFeedback");
    glDrawTransformFeedbackStream = (PFNGLDRAWTRANSFORMFEEDBACKSTREAMPROC)LL_GET_PROC_ADDRESS("glDrawTransformFeedbackStream");

    // GL_VERSION_4_1
    glDepthRangef = (PFNGLDEPTHRANGEFPROC)LL_GET_PROC_ADDRESS("glDepthRangef");
    glClearDepthf = (PFNGLCLEARDEPTHFPROC)LL_GET_PROC_ADDRESS("glClearDepthf");
    glVertexAttribL1d = (PFNGLVERTEXATTRIBL1DPROC)LL_GET_PROC_ADDRESS("glVertexAttribL1d");
    glVertexAttribL2d = (PFNGLVERTEXATTRIBL2DPROC)LL_GET_PROC_ADDRESS("glVertexAttribL2d");
    glVertexAttribL3d = (PFNGLVERTEXATTRIBL3DPROC)LL_GET_PROC_ADDRESS("glVertexAttribL3d");
    glVertexAttribL4d = (PFNGLVERTEXATTRIBL4DPROC)LL_GET_PROC_ADDRESS("glVertexAttribL4d");
    glVertexAttribL1dv = (PFNGLVERTEXATTRIBL1DVPROC)LL_GET_PROC_ADDRESS("glVertexAttribL1dv");
    glVertexAttribL2dv = (PFNGLVERTEXATTRIBL2DVPROC)LL_GET_PROC_ADDRESS("glVertexAttribL2dv");
    glVertexAttribL3dv = (PFNGLVERTEXATTRIBL3DVPROC)LL_GET_PROC_ADDRESS("glVertexAttribL3dv");
    glVertexAttribL4dv = (PFNGLVERTEXATTRIBL4DVPROC)LL_GET_PROC_ADDRESS("glVertexAttribL4dv");
    glVertexAttribLPointer = (PFNGLVERTEXATTRIBLPOINTERPROC)LL_GET_PROC_ADDRESS("glVertexAttribLPointer");
    glGetVertexAttribLdv = (PFNGLGETVERTEXATTRIBLDVPROC)LL_GET_PROC_ADDRESS("glGetVertexAttribLdv");
    glViewportArrayv = (PFNGLVIEWPORTARRAYVPROC)LL_GET_PROC_ADDRESS("glViewportArrayv");
    glViewportIndexedf = (PFNGLVIEWPORTINDEXEDFPROC)LL_GET_PROC_ADDRESS("glViewportIndexedf");
    glViewportIndexedfv = (PFNGLVIEWPORTINDEXEDFVPROC)LL_GET_PROC_ADDRESS("glViewportIndexedfv");
    glScissorArrayv = (PFNGLSCISSORARRAYVPROC)LL_GET_PROC_ADDRESS("glScissorArrayv");
    glScissorIndexed = (PFNGLSCISSORINDEXEDPROC)LL_GET_PROC_ADDRESS("glScissorIndexed");
    glScissorIndexedv = (PFNGLSCISSORINDEXEDVPROC)LL_GET_PROC_ADDRESS("glScissorIndexedv");
    glDepthRangeArrayv = (PFNGLDEPTHRANGEARRAYVPROC)LL_GET_PROC_ADDRESS("glDepthRangeArrayv");
    glDepthRangeIndexed = (PFNGLDEPTHRANGEINDEXEDPROC)LL_GET_PROC_ADDRESS("glDepthRangeIndexed");
    glGetFloati_v = (PFNGLGETFLOATI_VPROC)LL_GET_PROC_ADDRESS("glGetFloati_v");
    glGetDoublei_v = (PFNGLGETDOUBLEI_VPROC)LL_GET_PROC_ADDRESS("glGetDoublei_v");

    // GL_VERSION_4_2
    if (mGLVersion < 4.19f)
    {
        return;
    }
    glDrawArraysInstancedBaseInstance = (PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEPROC)LL_GET_PROC_ADDRESS("glDrawArraysInstancedBaseInstance");
    glDrawElementsInstancedBaseInstance = (PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEPROC)LL_GET_PROC_ADDRESS("glDrawElementsInstancedBaseInstance");
    glDrawElementsInstancedBaseVertexBaseInstance = (PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEPROC)LL_GET_PROC_ADDRESS("glDrawElementsInstancedBaseVertexBaseInstance");
    glGetInternalformativ = (PFNGLGETINTERNALFORMATIVPROC)LL_GET_PROC_ADDRESS("glGetInternalformativ");
    glGetActiveAtomicCounterBufferiv = (PFNGLGETACTIVEATOMICCOUNTERBUFFERIVPROC)LL_GET_PROC_ADDRESS("glGetActiveAtomicCounterBufferiv");
    glBindImageTexture = (PFNGLBINDIMAGETEXTUREPROC)LL_GET_PROC_ADDRESS("glBindImageTexture");
    glMemoryBarrier = (PFNGLMEMORYBARRIERPROC)LL_GET_PROC_ADDRESS("glMemoryBarrier");
    glTexStorage1D = (PFNGLTEXSTORAGE1DPROC)LL_GET_PROC_ADDRESS("glTexStorage1D");
    glTexStorage2D = (PFNGLTEXSTORAGE2DPROC)LL_GET_PROC_ADDRESS("glTexStorage2D");
    glTexStorage3D = (PFNGLTEXSTORAGE3DPROC)LL_GET_PROC_ADDRESS("glTexStorage3D");
    glDrawTransformFeedbackInstanced = (PFNGLDRAWTRANSFORMFEEDBACKINSTANCEDPROC)LL_GET_PROC_ADDRESS("glDrawTransformFeedbackInstanced");
    glDrawTransformFeedbackStreamInstanced = (PFNGLDRAWTRANSFORMFEEDBACKSTREAMINSTANCEDPROC)LL_GET_PROC_ADDRESS("glDrawTransformFeedbackStreamInstanced");

    // GL_VERSION_4_3
    if (mGLVersion < 4.29f)
    {
        return;
    }
    glClearBufferData = (PFNGLCLEARBUFFERDATAPROC)LL_GET_PROC_ADDRESS("glClearBufferData");
    glClearBufferSubData = (PFNGLCLEARBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glClearBufferSubData");
    glDispatchCompute = (PFNGLDISPATCHCOMPUTEPROC)LL_GET_PROC_ADDRESS("glDispatchCompute");
    glDispatchComputeIndirect = (PFNGLDISPATCHCOMPUTEINDIRECTPROC)LL_GET_PROC_ADDRESS("glDispatchComputeIndirect");
    glCopyImageSubData = (PFNGLCOPYIMAGESUBDATAPROC)LL_GET_PROC_ADDRESS("glCopyImageSubData");
    glFramebufferParameteri = (PFNGLFRAMEBUFFERPARAMETERIPROC)LL_GET_PROC_ADDRESS("glFramebufferParameteri");
    glGetFramebufferParameteriv = (PFNGLGETFRAMEBUFFERPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetFramebufferParameteriv");
    glGetInternalformati64v = (PFNGLGETINTERNALFORMATI64VPROC)LL_GET_PROC_ADDRESS("glGetInternalformati64v");
    glInvalidateTexSubImage = (PFNGLINVALIDATETEXSUBIMAGEPROC)LL_GET_PROC_ADDRESS("glInvalidateTexSubImage");
    glInvalidateTexImage = (PFNGLINVALIDATETEXIMAGEPROC)LL_GET_PROC_ADDRESS("glInvalidateTexImage");
    glInvalidateBufferSubData = (PFNGLINVALIDATEBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glInvalidateBufferSubData");
    glInvalidateBufferData = (PFNGLINVALIDATEBUFFERDATAPROC)LL_GET_PROC_ADDRESS("glInvalidateBufferData");
    glInvalidateFramebuffer = (PFNGLINVALIDATEFRAMEBUFFERPROC)LL_GET_PROC_ADDRESS("glInvalidateFramebuffer");
    glInvalidateSubFramebuffer = (PFNGLINVALIDATESUBFRAMEBUFFERPROC)LL_GET_PROC_ADDRESS("glInvalidateSubFramebuffer");
    glMultiDrawArraysIndirect = (PFNGLMULTIDRAWARRAYSINDIRECTPROC)LL_GET_PROC_ADDRESS("glMultiDrawArraysIndirect");
    glMultiDrawElementsIndirect = (PFNGLMULTIDRAWELEMENTSINDIRECTPROC)LL_GET_PROC_ADDRESS("glMultiDrawElementsIndirect");
    glTexBufferRange = (PFNGLTEXBUFFERRANGEPROC)LL_GET_PROC_ADDRESS("glTexBufferRange");
    glTexStorage2DMultisample = (PFNGLTEXSTORAGE2DMULTISAMPLEPROC)LL_GET_PROC_ADDRESS("glTexStorage2DMultisample");
    glTexStorage3DMultisample = (PFNGLTEXSTORAGE3DMULTISAMPLEPROC)LL_GET_PROC_ADDRESS("glTexStorage3DMultisample");
    glTextureView = (PFNGLTEXTUREVIEWPROC)LL_GET_PROC_ADDRESS("glTextureView");
    glBindVertexBuffer = (PFNGLBINDVERTEXBUFFERPROC)LL_GET_PROC_ADDRESS("glBindVertexBuffer");
    glVertexAttribFormat = (PFNGLVERTEXATTRIBFORMATPROC)LL_GET_PROC_ADDRESS("glVertexAttribFormat");
    glVertexAttribIFormat = (PFNGLVERTEXATTRIBIFORMATPROC)LL_GET_PROC_ADDRESS("glVertexAttribIFormat");
    glVertexAttribLFormat = (PFNGLVERTEXATTRIBLFORMATPROC)LL_GET_PROC_ADDRESS("glVertexAttribLFormat");
    glVertexAttribBinding = (PFNGLVERTEXATTRIBBINDINGPROC)LL_GET_PROC_ADDRESS("glVertexAttribBinding");
    glVertexBindingDivisor = (PFNGLVERTEXBINDINGDIVISORPROC)LL_GET_PROC_ADDRESS("glVertexBindingDivisor");
    glPushDebugGroup = (PFNGLPUSHDEBUGGROUPPROC)LL_GET_PROC_ADDRESS("glPushDebugGroup");
    glPopDebugGroup = (PFNGLPOPDEBUGGROUPPROC)LL_GET_PROC_ADDRESS("glPopDebugGroup");
    glObjectLabel = (PFNGLOBJECTLABELPROC)LL_GET_PROC_ADDRESS("glObjectLabel");
    glGetObjectLabel = (PFNGLGETOBJECTLABELPROC)LL_GET_PROC_ADDRESS("glGetObjectLabel");
    glObjectPtrLabel = (PFNGLOBJECTPTRLABELPROC)LL_GET_PROC_ADDRESS("glObjectPtrLabel");
    glGetObjectPtrLabel = (PFNGLGETOBJECTPTRLABELPROC)LL_GET_PROC_ADDRESS("glGetObjectPtrLabel");

    // GL_VERSION_4_4
    if (mGLVersion < 4.39f)
    {
        return;
    }
    glBufferStorage = (PFNGLBUFFERSTORAGEPROC)LL_GET_PROC_ADDRESS("glBufferStorage");
    glClearTexImage = (PFNGLCLEARTEXIMAGEPROC)LL_GET_PROC_ADDRESS("glClearTexImage");
    glClearTexSubImage = (PFNGLCLEARTEXSUBIMAGEPROC)LL_GET_PROC_ADDRESS("glClearTexSubImage");
    glBindBuffersBase = (PFNGLBINDBUFFERSBASEPROC)LL_GET_PROC_ADDRESS("glBindBuffersBase");
    glBindBuffersRange = (PFNGLBINDBUFFERSRANGEPROC)LL_GET_PROC_ADDRESS("glBindBuffersRange");
    glBindTextures = (PFNGLBINDTEXTURESPROC)LL_GET_PROC_ADDRESS("glBindTextures");
    glBindSamplers = (PFNGLBINDSAMPLERSPROC)LL_GET_PROC_ADDRESS("glBindSamplers");
    glBindImageTextures = (PFNGLBINDIMAGETEXTURESPROC)LL_GET_PROC_ADDRESS("glBindImageTextures");
    glBindVertexBuffers = (PFNGLBINDVERTEXBUFFERSPROC)LL_GET_PROC_ADDRESS("glBindVertexBuffers");

    // GL_VERSION_4_5
    if (mGLVersion < 4.49f)
    {
        return;
    }
    glClipControl = (PFNGLCLIPCONTROLPROC)LL_GET_PROC_ADDRESS("glClipControl");
    glCreateTransformFeedbacks = (PFNGLCREATETRANSFORMFEEDBACKSPROC)LL_GET_PROC_ADDRESS("glCreateTransformFeedbacks");
    glTransformFeedbackBufferBase = (PFNGLTRANSFORMFEEDBACKBUFFERBASEPROC)LL_GET_PROC_ADDRESS("glTransformFeedbackBufferBase");
    glTransformFeedbackBufferRange = (PFNGLTRANSFORMFEEDBACKBUFFERRANGEPROC)LL_GET_PROC_ADDRESS("glTransformFeedbackBufferRange");
    glGetTransformFeedbackiv = (PFNGLGETTRANSFORMFEEDBACKIVPROC)LL_GET_PROC_ADDRESS("glGetTransformFeedbackiv");
    glGetTransformFeedbacki_v = (PFNGLGETTRANSFORMFEEDBACKI_VPROC)LL_GET_PROC_ADDRESS("glGetTransformFeedbacki_v");
    glGetTransformFeedbacki64_v = (PFNGLGETTRANSFORMFEEDBACKI64_VPROC)LL_GET_PROC_ADDRESS("glGetTransformFeedbacki64_v");
    glCreateBuffers = (PFNGLCREATEBUFFERSPROC)LL_GET_PROC_ADDRESS("glCreateBuffers");
    glNamedBufferStorage = (PFNGLNAMEDBUFFERSTORAGEPROC)LL_GET_PROC_ADDRESS("glNamedBufferStorage");
    glNamedBufferData = (PFNGLNAMEDBUFFERDATAPROC)LL_GET_PROC_ADDRESS("glNamedBufferData");
    glNamedBufferSubData = (PFNGLNAMEDBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glNamedBufferSubData");
    glCopyNamedBufferSubData = (PFNGLCOPYNAMEDBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glCopyNamedBufferSubData");
    glClearNamedBufferData = (PFNGLCLEARNAMEDBUFFERDATAPROC)LL_GET_PROC_ADDRESS("glClearNamedBufferData");
    glClearNamedBufferSubData = (PFNGLCLEARNAMEDBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glClearNamedBufferSubData");
    glMapNamedBuffer = (PFNGLMAPNAMEDBUFFERPROC)LL_GET_PROC_ADDRESS("glMapNamedBuffer");
    glMapNamedBufferRange = (PFNGLMAPNAMEDBUFFERRANGEPROC)LL_GET_PROC_ADDRESS("glMapNamedBufferRange");
    glUnmapNamedBuffer = (PFNGLUNMAPNAMEDBUFFERPROC)LL_GET_PROC_ADDRESS("glUnmapNamedBuffer");
    glFlushMappedNamedBufferRange = (PFNGLFLUSHMAPPEDNAMEDBUFFERRANGEPROC)LL_GET_PROC_ADDRESS("glFlushMappedNamedBufferRange");
    glGetNamedBufferParameteriv = (PFNGLGETNAMEDBUFFERPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetNamedBufferParameteriv");
    glGetNamedBufferParameteri64v = (PFNGLGETNAMEDBUFFERPARAMETERI64VPROC)LL_GET_PROC_ADDRESS("glGetNamedBufferParameteri64v");
    glGetNamedBufferPointerv = (PFNGLGETNAMEDBUFFERPOINTERVPROC)LL_GET_PROC_ADDRESS("glGetNamedBufferPointerv");
    glGetNamedBufferSubData = (PFNGLGETNAMEDBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glGetNamedBufferSubData");
    glCreateFramebuffers = (PFNGLCREATEFRAMEBUFFERSPROC)LL_GET_PROC_ADDRESS("glCreateFramebuffers");
    glNamedFramebufferRenderbuffer = (PFNGLNAMEDFRAMEBUFFERRENDERBUFFERPROC)LL_GET_PROC_ADDRESS("glNamedFramebufferRenderbuffer");
    glNamedFramebufferParameteri = (PFNGLNAMEDFRAMEBUFFERPARAMETERIPROC)LL_GET_PROC_ADDRESS("glNamedFramebufferParameteri");
    glNamedFramebufferTexture = (PFNGLNAMEDFRAMEBUFFERTEXTUREPROC)LL_GET_PROC_ADDRESS("glNamedFramebufferTexture");
    glNamedFramebufferTextureLayer = (PFNGLNAMEDFRAMEBUFFERTEXTURELAYERPROC)LL_GET_PROC_ADDRESS("glNamedFramebufferTextureLayer");
    glNamedFramebufferDrawBuffer = (PFNGLNAMEDFRAMEBUFFERDRAWBUFFERPROC)LL_GET_PROC_ADDRESS("glNamedFramebufferDrawBuffer");
    glNamedFramebufferDrawBuffers = (PFNGLNAMEDFRAMEBUFFERDRAWBUFFERSPROC)LL_GET_PROC_ADDRESS("glNamedFramebufferDrawBuffers");
    glNamedFramebufferReadBuffer = (PFNGLNAMEDFRAMEBUFFERREADBUFFERPROC)LL_GET_PROC_ADDRESS("glNamedFramebufferReadBuffer");
    glInvalidateNamedFramebufferData = (PFNGLINVALIDATENAMEDFRAMEBUFFERDATAPROC)LL_GET_PROC_ADDRESS("glInvalidateNamedFramebufferData");
    glInvalidateNamedFramebufferSubData = (PFNGLINVALIDATENAMEDFRAMEBUFFERSUBDATAPROC)LL_GET_PROC_ADDRESS("glInvalidateNamedFramebufferSubData");
    glClearNamedFramebufferiv = (PFNGLCLEARNAMEDFRAMEBUFFERIVPROC)LL_GET_PROC_ADDRESS("glClearNamedFramebufferiv");
    glClearNamedFramebufferuiv = (PFNGLCLEARNAMEDFRAMEBUFFERUIVPROC)LL_GET_PROC_ADDRESS("glClearNamedFramebufferuiv");
    glClearNamedFramebufferfv = (PFNGLCLEARNAMEDFRAMEBUFFERFVPROC)LL_GET_PROC_ADDRESS("glClearNamedFramebufferfv");
    glClearNamedFramebufferfi = (PFNGLCLEARNAMEDFRAMEBUFFERFIPROC)LL_GET_PROC_ADDRESS("glClearNamedFramebufferfi");
    glBlitNamedFramebuffer = (PFNGLBLITNAMEDFRAMEBUFFERPROC)LL_GET_PROC_ADDRESS("glBlitNamedFramebuffer");
    glCheckNamedFramebufferStatus = (PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC)LL_GET_PROC_ADDRESS("glCheckNamedFramebufferStatus");
    glGetNamedFramebufferParameteriv = (PFNGLGETNAMEDFRAMEBUFFERPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetNamedFramebufferParameteriv");
    glGetNamedFramebufferAttachmentParameteriv = (PFNGLGETNAMEDFRAMEBUFFERATTACHMENTPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetNamedFramebufferAttachmentParameteriv");
    glCreateRenderbuffers = (PFNGLCREATERENDERBUFFERSPROC)LL_GET_PROC_ADDRESS("glCreateRenderbuffers");
    glNamedRenderbufferStorage = (PFNGLNAMEDRENDERBUFFERSTORAGEPROC)LL_GET_PROC_ADDRESS("glNamedRenderbufferStorage");
    glNamedRenderbufferStorageMultisample = (PFNGLNAMEDRENDERBUFFERSTORAGEMULTISAMPLEPROC)LL_GET_PROC_ADDRESS("glNamedRenderbufferStorageMultisample");
    glGetNamedRenderbufferParameteriv = (PFNGLGETNAMEDRENDERBUFFERPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetNamedRenderbufferParameteriv");
    glCreateTextures = (PFNGLCREATETEXTURESPROC)LL_GET_PROC_ADDRESS("glCreateTextures");
    glTextureBuffer = (PFNGLTEXTUREBUFFERPROC)LL_GET_PROC_ADDRESS("glTextureBuffer");
    glTextureBufferRange = (PFNGLTEXTUREBUFFERRANGEPROC)LL_GET_PROC_ADDRESS("glTextureBufferRange");
    glTextureStorage1D = (PFNGLTEXTURESTORAGE1DPROC)LL_GET_PROC_ADDRESS("glTextureStorage1D");
    glTextureStorage2D = (PFNGLTEXTURESTORAGE2DPROC)LL_GET_PROC_ADDRESS("glTextureStorage2D");
    glTextureStorage3D = (PFNGLTEXTURESTORAGE3DPROC)LL_GET_PROC_ADDRESS("glTextureStorage3D");
    glTextureStorage2DMultisample = (PFNGLTEXTURESTORAGE2DMULTISAMPLEPROC)LL_GET_PROC_ADDRESS("glTextureStorage2DMultisample");
    glTextureStorage3DMultisample = (PFNGLTEXTURESTORAGE3DMULTISAMPLEPROC)LL_GET_PROC_ADDRESS("glTextureStorage3DMultisample");
    glTextureSubImage1D = (PFNGLTEXTURESUBIMAGE1DPROC)LL_GET_PROC_ADDRESS("glTextureSubImage1D");
    glTextureSubImage2D = (PFNGLTEXTURESUBIMAGE2DPROC)LL_GET_PROC_ADDRESS("glTextureSubImage2D");
    glTextureSubImage3D = (PFNGLTEXTURESUBIMAGE3DPROC)LL_GET_PROC_ADDRESS("glTextureSubImage3D");
    glCompressedTextureSubImage1D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE1DPROC)LL_GET_PROC_ADDRESS("glCompressedTextureSubImage1D");
    glCompressedTextureSubImage2D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE2DPROC)LL_GET_PROC_ADDRESS("glCompressedTextureSubImage2D");
    glCompressedTextureSubImage3D = (PFNGLCOMPRESSEDTEXTURESUBIMAGE3DPROC)LL_GET_PROC_ADDRESS("glCompressedTextureSubImage3D");
    glCopyTextureSubImage1D = (PFNGLCOPYTEXTURESUBIMAGE1DPROC)LL_GET_PROC_ADDRESS("glCopyTextureSubImage1D");
    glCopyTextureSubImage2D = (PFNGLCOPYTEXTURESUBIMAGE2DPROC)LL_GET_PROC_ADDRESS("glCopyTextureSubImage2D");
    glCopyTextureSubImage3D = (PFNGLCOPYTEXTURESUBIMAGE3DPROC)LL_GET_PROC_ADDRESS("glCopyTextureSubImage3D");
    glTextureParameterf = (PFNGLTEXTUREPARAMETERFPROC)LL_GET_PROC_ADDRESS("glTextureParameterf");
    glTextureParameterfv = (PFNGLTEXTUREPARAMETERFVPROC)LL_GET_PROC_ADDRESS("glTextureParameterfv");
    glTextureParameteri = (PFNGLTEXTUREPARAMETERIPROC)LL_GET_PROC_ADDRESS("glTextureParameteri");
    glTextureParameterIiv = (PFNGLTEXTUREPARAMETERIIVPROC)LL_GET_PROC_ADDRESS("glTextureParameterIiv");
    glTextureParameterIuiv = (PFNGLTEXTUREPARAMETERIUIVPROC)LL_GET_PROC_ADDRESS("glTextureParameterIuiv");
    glTextureParameteriv = (PFNGLTEXTUREPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glTextureParameteriv");
    glGenerateTextureMipmap = (PFNGLGENERATETEXTUREMIPMAPPROC)LL_GET_PROC_ADDRESS("glGenerateTextureMipmap");
    glBindTextureUnit = (PFNGLBINDTEXTUREUNITPROC)LL_GET_PROC_ADDRESS("glBindTextureUnit");
    glGetTextureImage = (PFNGLGETTEXTUREIMAGEPROC)LL_GET_PROC_ADDRESS("glGetTextureImage");
    glGetCompressedTextureImage = (PFNGLGETCOMPRESSEDTEXTUREIMAGEPROC)LL_GET_PROC_ADDRESS("glGetCompressedTextureImage");
    glGetTextureLevelParameterfv = (PFNGLGETTEXTURELEVELPARAMETERFVPROC)LL_GET_PROC_ADDRESS("glGetTextureLevelParameterfv");
    glGetTextureLevelParameteriv = (PFNGLGETTEXTURELEVELPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetTextureLevelParameteriv");
    glGetTextureParameterfv = (PFNGLGETTEXTUREPARAMETERFVPROC)LL_GET_PROC_ADDRESS("glGetTextureParameterfv");
    glGetTextureParameterIiv = (PFNGLGETTEXTUREPARAMETERIIVPROC)LL_GET_PROC_ADDRESS("glGetTextureParameterIiv");
    glGetTextureParameterIuiv = (PFNGLGETTEXTUREPARAMETERIUIVPROC)LL_GET_PROC_ADDRESS("glGetTextureParameterIuiv");
    glGetTextureParameteriv = (PFNGLGETTEXTUREPARAMETERIVPROC)LL_GET_PROC_ADDRESS("glGetTextureParameteriv");
    glCreateVertexArrays = (PFNGLCREATEVERTEXARRAYSPROC)LL_GET_PROC_ADDRESS("glCreateVertexArrays");
    glDisableVertexArrayAttrib = (PFNGLDISABLEVERTEXARRAYATTRIBPROC)LL_GET_PROC_ADDRESS("glDisableVertexArrayAttrib");
    glEnableVertexArrayAttrib = (PFNGLENABLEVERTEXARRAYATTRIBPROC)LL_GET_PROC_ADDRESS("glEnableVertexArrayAttrib");
    glVertexArrayElementBuffer = (PFNGLVERTEXARRAYELEMENTBUFFERPROC)LL_GET_PROC_ADDRESS("glVertexArrayElementBuffer");
    glVertexArrayVertexBuffer = (PFNGLVERTEXARRAYVERTEXBUFFERPROC)LL_GET_PROC_ADDRESS("glVertexArrayVertexBuffer");
    glVertexArrayVertexBuffers = (PFNGLVERTEXARRAYVERTEXBUFFERSPROC)LL_GET_PROC_ADDRESS("glVertexArrayVertexBuffers");
    glVertexArrayAttribBinding = (PFNGLVERTEXARRAYATTRIBBINDINGPROC)LL_GET_PROC_ADDRESS("glVertexArrayAttribBinding");
    glVertexArrayAttribFormat = (PFNGLVERTEXARRAYATTRIBFORMATPROC)LL_GET_PROC_ADDRESS("glVertexArrayAttribFormat");
    glVertexArrayAttribIFormat = (PFNGLVERTEXARRAYATTRIBIFORMATPROC)LL_GET_PROC_ADDRESS("glVertexArrayAttribIFormat");
    glVertexArrayAttribLFormat = (PFNGLVERTEXARRAYATTRIBLFORMATPROC)LL_GET_PROC_ADDRESS("glVertexArrayAttribLFormat");
    glVertexArrayBindingDivisor = (PFNGLVERTEXARRAYBINDINGDIVISORPROC)LL_GET_PROC_ADDRESS("glVertexArrayBindingDivisor");
    glGetVertexArrayiv = (PFNGLGETVERTEXARRAYIVPROC)LL_GET_PROC_ADDRESS("glGetVertexArrayiv");
    glGetVertexArrayIndexediv = (PFNGLGETVERTEXARRAYINDEXEDIVPROC)LL_GET_PROC_ADDRESS("glGetVertexArrayIndexediv");
    glGetVertexArrayIndexed64iv = (PFNGLGETVERTEXARRAYINDEXED64IVPROC)LL_GET_PROC_ADDRESS("glGetVertexArrayIndexed64iv");
    glCreateSamplers = (PFNGLCREATESAMPLERSPROC)LL_GET_PROC_ADDRESS("glCreateSamplers");
    glMemoryBarrierByRegion = (PFNGLMEMORYBARRIERBYREGIONPROC)LL_GET_PROC_ADDRESS("glMemoryBarrierByRegion");
    glGetTextureSubImage = (PFNGLGETTEXTURESUBIMAGEPROC)LL_GET_PROC_ADDRESS("glGetTextureSubImage");
    glGetCompressedTextureSubImage = (PFNGLGETCOMPRESSEDTEXTURESUBIMAGEPROC)LL_GET_PROC_ADDRESS("glGetCompressedTextureSubImage");
    glGetGraphicsResetStatus = (PFNGLGETGRAPHICSRESETSTATUSPROC)LL_GET_PROC_ADDRESS("glGetGraphicsResetStatus");
    glGetnCompressedTexImage = (PFNGLGETNCOMPRESSEDTEXIMAGEPROC)LL_GET_PROC_ADDRESS("glGetnCompressedTexImage");
    glGetnTexImage = (PFNGLGETNTEXIMAGEPROC)LL_GET_PROC_ADDRESS("glGetnTexImage");
    glReadnPixels = (PFNGLREADNPIXELSPROC)LL_GET_PROC_ADDRESS("glReadnPixels");
    glTextureBarrier = (PFNGLTEXTUREBARRIERPROC)LL_GET_PROC_ADDRESS("glTextureBarrier");

    // GL_VERSION_4_6
    if (mGLVersion < 4.59f)
    {
        return;
    }
    glMultiDrawArraysIndirectCount = (PFNGLMULTIDRAWARRAYSINDIRECTCOUNTPROC)LL_GET_PROC_ADDRESS("glMultiDrawArraysIndirectCount");
    glMultiDrawElementsIndirectCount = (PFNGLMULTIDRAWELEMENTSINDIRECTCOUNTPROC)LL_GET_PROC_ADDRESS("glMultiDrawElementsIndirectCount");
    glPolygonOffsetClamp = (PFNGLPOLYGONOFFSETCLAMPPROC)LL_GET_PROC_ADDRESS("glPolygonOffsetClamp");
}

void rotate_quat(LLQuaternion& rotation)
{
    F32 angle_radians, x, y, z;
    rotation.getAngleAxis(&angle_radians, &x, &y, &z);
    gGL.rotatef(angle_radians * RAD_TO_DEG, x, y, z);
}

void flush_glerror()
{
    if (gGLManager.mInited)
    {
        glGetError();
    }
}

const std::string getGLErrorString(GLenum error)
{
    switch(error)
    {
    case GL_NO_ERROR:
        return "No Error";
    case GL_INVALID_ENUM:
        return "Invalid Enum";
    case GL_INVALID_VALUE:
        return "Invalid Value";
    case GL_INVALID_OPERATION:
        return "Invalid Operation";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "Invalid Framebuffer Operation";
    case GL_OUT_OF_MEMORY:
        return "Out of Memory";
#ifdef GL_STACK_UNDERFLOW
    case GL_STACK_UNDERFLOW:
        return "Stack Underflow";
#endif
#ifdef GL_STACK_OVERFLOW
    case GL_STACK_OVERFLOW:
        return "Stack Overflow";
#endif
#ifdef GL_TABLE_TOO_LARGE
    case GL_TABLE_TOO_LARGE:
        return "Table too large";
#endif
    default:
        return "UNKNOWN ERROR";
    }
}

//this function outputs gl error to the log file, does not crash the code.
void log_glerror()
{
    if (!gGLManager.mInited) [[unlikely]]
    {
        return ;
    }
    //  Create or update texture to be used with this data
    GLenum error;
    error = glGetError();
    while (error) [[unlikely]]
    {
        std::string gl_error_msg = getGLErrorString(error);
        LL_WARNS() << "GL Error: 0x" << std::hex << error << std::dec << " GL Error String: " << gl_error_msg << LL_ENDL;
        error = glGetError();
    }
}

void do_assert_glerror()
{
    //  Create or update texture to be used with this data
    GLenum error;
    error = glGetError();
    bool quit = false;
    if (error) [[unlikely]]
    {
        quit = true;
        std::string gl_error_msg = getGLErrorString(error);
        LL_WARNS("RenderState") << "GL Error: 0x" << std::hex << error << std::dec << LL_ENDL;
        LL_WARNS("RenderState") << "GL Error String: " << gl_error_msg << LL_ENDL;
        if (gDebugSession)
        {
            gFailLog << "GL Error: 0x" << std::hex << error << std::dec << " GL Error String: " << gl_error_msg << std::endl;
        }
    }

    if (quit)
    {
        if (gDebugSession)
        {
            ll_fail("assert_glerror failed");
        }
        else
        {
            LL_ERRS() << "One or more unhandled GL errors." << LL_ENDL;
        }
    }
}

void assert_glerror()
{
/*  if (!gGLActive)
    {
        //LL_WARNS() << "GL used while not active!" << LL_ENDL;

        if (gDebugSession)
        {
            //ll_fail("GL used while not active");
        }
    }
*/

    if (!gDebugGL || !gGLManager.mInited)
    {
        //funny looking if for branch prediction -- gDebugGL is almost always false and assert_glerror is called often
    }
    else
    {
        do_assert_glerror();
    }
}


void clear_glerror()
{
    if (gGLManager.mInited)
    {
        glGetError();
        glGetError();
    }
}

///////////////////////////////////////////////////////////////
//
// LLGLState
//

// Static members
boost::unordered_map<LLGLenum, LLGLboolean> LLGLState::sStateMap;

GLboolean LLGLDepthTest::sDepthEnabled = GL_FALSE; // OpenGL default
GLenum LLGLDepthTest::sDepthFunc = GL_LESS; // OpenGL default
GLboolean LLGLDepthTest::sWriteEnabled = GL_TRUE; // OpenGL default

//static
void LLGLState::initClass()
{
    // Runs per context, from initGL: the map and the depth state describe this context, and
    // GL starts every context at its defaults.
    sStateMap.clear();
    LLGLDepthTest::initClass();

    sStateMap[GL_DITHER] = GL_TRUE;
    // sStateMap[GL_TEXTURE_2D] = GL_TRUE;

    //make sure multisample defaults to disabled
    sStateMap[GL_MULTISAMPLE] = GL_FALSE;
    if (gGLManager.mInited)
    {
        glDisable(GL_MULTISAMPLE);
    }

    // Ensure SRGB framebuffer is disabled by default
    sStateMap[GL_FRAMEBUFFER_SRGB] = GL_FALSE;
    if (gGLManager.mInited)
    {
        glDisable(GL_FRAMEBUFFER_SRGB);
    }
}

//static
bool LLGLState::isEnabled(LLGLenum state)
{
    const auto found = sStateMap.find(state);
    return found != sStateMap.end() && found->second == GL_TRUE;
}

//static
// Really shouldn't be needed, but seems we sometimes do.
void LLGLState::resetTextureStates()
{
    gGL.flush();
    GLint maxTextureUnits;

    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
    for (S32 j = maxTextureUnits-1; j >=0; j--)
    {
        // Slot 0 used to be spared and left "enabled for TT_TEXTURE" instead, which reset
        // nothing -- it only set the target field. A reset releases every slot.
        gGL.getTextureSlot(j)->unbind();
    }
}

void LLGLState::dumpStates()
{
    LL_INFOS("RenderState") << "GL States:" << LL_ENDL;
    for (boost::unordered_map<LLGLenum, LLGLboolean>::iterator iter = sStateMap.begin();
         iter != sStateMap.end(); ++iter)
    {
        LL_INFOS("RenderState") << llformat(" 0x%04x : %s",(S32)iter->first,iter->second?"true":"false") << LL_ENDL;
    }
}

void LLGLState::checkStates(GLboolean writeAlpha)
{
    if (!gDebugGL || !issue_opengl_state())
    {
        return;
    }

    GLint srcRGB, dstRGB, srcAlpha, dstAlpha;
    glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
    llassert_always(srcRGB == GL_SRC_ALPHA);
    llassert_always(srcAlpha == GL_SRC_ALPHA);
    llassert_always(dstRGB == GL_ONE_MINUS_SRC_ALPHA);
    llassert_always(dstAlpha == GL_ONE_MINUS_SRC_ALPHA);

    // disable for now until usage is consistent
    //GLboolean colorMask[4];
    //glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    //llassert_always(colorMask[0]);
    //llassert_always(colorMask[1]);
    //llassert_always(colorMask[2]);
    // llassert_always(colorMask[3] == writeAlpha);

    for (boost::unordered_map<LLGLenum, LLGLboolean>::iterator iter = sStateMap.begin();
         iter != sStateMap.end(); ++iter)
    {
        LLGLenum state = iter->first;
        LLGLboolean cur_state = iter->second;
        LLGLboolean gl_state = glIsEnabled(state);
        if(cur_state != gl_state)
        {
            dumpStates();
            LL_GL_ERRS << llformat("LLGLState error. State: 0x%04x",state) << LL_ENDL;
        }
    }
}

///////////////////////////////////////////////////////////////////////

LLGLState::LLGLState(LLGLenum state, S32 enabled) :
    mState(state), mWasEnabled(false), mIsEnabled(false)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    if (mState)
    {
        mWasEnabled = sStateMap[state];
        setEnabled(enabled);
    }
}

void LLGLState::setEnabled(S32 enabled)
{
    if (!mState)
    {
        return;
    }
    if (enabled == CURRENT_STATE)
    {
        enabled = sStateMap[mState] == GL_TRUE ? ENABLED_STATE : DISABLED_STATE;
    }
    else if (enabled == ENABLED_STATE && sStateMap[mState] != GL_TRUE)
    {
        gGL.flush();
        if (issue_opengl_state())
        {
            glEnable(mState);
        }
        sStateMap[mState] = GL_TRUE;
    }
    else if (enabled == DISABLED_STATE && sStateMap[mState] != GL_FALSE)
    {
        gGL.flush();
        if (issue_opengl_state())
        {
            glDisable(mState);
        }
        sStateMap[mState] = GL_FALSE;
    }
    mIsEnabled = enabled;
}

LLGLState::~LLGLState()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    if (mState)
    {
        if (gDebugGL && issue_opengl_state())
        {
            if (!gDebugSession)
            {
                llassert_always(sStateMap[mState] == glIsEnabled(mState));
            }
            else
            {
                if (sStateMap[mState] != glIsEnabled(mState))
                {
                    ll_fail("GL enabled state does not match expected");
                }
            }
        }

        if (mIsEnabled != mWasEnabled)
        {
            gGL.flush();
            if (mWasEnabled)
            {
                if (issue_opengl_state())
                {
                    glEnable(mState);
                }
                sStateMap[mState] = GL_TRUE;
            }
            else
            {
                if (issue_opengl_state())
                {
                    glDisable(mState);
                }
                sStateMap[mState] = GL_FALSE;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

void LLGLManager::initGLStates()
{
    //gl states moved to classes in llglstates.h
    LLGLState::initClass();
}

////////////////////////////////////////////////////////////////////////////////

void parse_gl_version( S32* major, S32* minor, S32* release, std::string* vendor_specific, std::string* version_string )
{
    // GL_VERSION returns a null-terminated string with the format:
    // <major>.<minor>[.<release>] [<vendor specific>]

    const char* version = (const char*) glGetString(GL_VERSION);
    *major = 0;
    *minor = 0;
    *release = 0;
    vendor_specific->assign("");

    if( !version )
    {
        return;
    }
    LL_INFOS() << "GL: "  << version << LL_ENDL;

    version_string->assign(version);

    std::string ver_copy( version );
    S32 len = (S32)strlen( version );   /* Flawfinder: ignore */
    S32 i = 0;
    S32 start;
    // Find the major version
    start = i;
    for( ; i < len; i++ )
    {
        if( '.' == version[i] )
        {
            break;
        }
    }
    std::string major_str = ver_copy.substr(start,i-start);
    LLStringUtil::convertToS32(major_str, *major);

    if( '.' == version[i] )
    {
        i++;
    }

    // Find the minor version
    start = i;
    for( ; i < len; i++ )
    {
        if( ('.' == version[i]) || isspace(version[i]) )
        {
            break;
        }
    }
    std::string minor_str = ver_copy.substr(start,i-start);
    LLStringUtil::convertToS32(minor_str, *minor);

    // Find the release number (optional)
    if( '.' == version[i] )
    {
        i++;

        start = i;
        for( ; i < len; i++ )
        {
            if( isspace(version[i]) )
            {
                break;
            }
        }

        std::string release_str = ver_copy.substr(start,i-start);
        LLStringUtil::convertToS32(release_str, *release);
    }

    // Skip over any white space
    while( version[i] && isspace( version[i] ) )
    {
        i++;
    }

    // Copy the vendor-specific string (optional)
    if( version[i] )
    {
        vendor_specific->assign( version + i );
    }
}


void parse_glsl_version(S32& major, S32& minor)
{
    // GL_SHADING_LANGUAGE_VERSION returns a null-terminated string with the format:
    // <major>.<minor>[.<release>] [<vendor specific>]

    const char* version = (const char*) glGetString(GL_SHADING_LANGUAGE_VERSION);
    major = 0;
    minor = 0;

    if( !version )
    {
        return;
    }

    std::string ver_copy( version );
    S32 len = (S32)strlen( version );   /* Flawfinder: ignore */
    S32 i = 0;
    S32 start;
    // Find the major version
    start = i;
    for( ; i < len; i++ )
    {
        if( '.' == version[i] )
        {
            break;
        }
    }
    std::string major_str = ver_copy.substr(start,i-start);
    LLStringUtil::convertToS32(major_str, major);

    if( '.' == version[i] )
    {
        i++;
    }

    // Find the minor version
    start = i;
    for( ; i < len; i++ )
    {
        if( ('.' == version[i]) || isspace(version[i]) )
        {
            break;
        }
    }
    std::string minor_str = ver_copy.substr(start,i-start);
    LLStringUtil::convertToS32(minor_str, minor);
}

LLGLUserClipPlane::LLGLUserClipPlane(const LLPlane& p, const LLMatrix4a& modelview, const LLMatrix4a& projection, bool apply)
{
    mApply = apply;

    if (mApply)
    {
        mModelview = modelview;
        mProjection = projection;

        //flip incoming LLPlane to get consistent behavior compared to frustum culling
        setPlane(-p[0], -p[1], -p[2], -p[3]);
    }
}

void LLGLUserClipPlane::disable()
{
    if (mApply)
    {
        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.popMatrix();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
    mApply = false;
}

// NOTE: dead code -- LLGLUserClipPlane is not constructed anywhere in the tree (water/
// mirror clipping is done by shader discard). The oblique-clip math below hardcodes the
// [-1,1] NDC convention (cplane[3] -= 1). If this is ever revived it MUST be made
// reverse-Z aware (the ZERO_TO_ONE near-plane constant differs) before use.
void LLGLUserClipPlane::setPlane(F32 a, F32 b, F32 c, F32 d)
{
    // the plane into clip space: through the inverse transpose of the
    // modelview then projection
    LLMatrix4a invtrans_MVP;
    invtrans_MVP.setMul(mModelview, mProjection);
    invtrans_MVP.invert();
    invtrans_MVP.transpose();
    LLVector4a cplane;
    invtrans_MVP.transform4(LLVector4a(a, b, c, d), cplane);

    cplane.mul(1.f / fabsf(cplane[2])); // normalize such that depth is not scaled
    cplane.getF32ptr()[3] -= 1.f;

    if (cplane[2] < 0.f)
    {
        cplane.negate();
    }

    // the projection with its depth column replaced by the plane
    LLMatrix4a suffix;
    suffix.setIdentity();
    suffix.setColumn<2>(cplane);
    LLMatrix4a newP;
    newP.setMul(mProjection, suffix);
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.pushMatrix();
    gGL.loadMatrix(newP);
    gGL.matrixMode(LLRender::MM_MODELVIEW);
}

LLGLUserClipPlane::~LLGLUserClipPlane()
{
    disable();
}

GLenum LLGLDepthTest::remap(GLenum func)
{
    if (!LLRender::sReverseZ)
    {
        return func;
    }
    switch (func)
    {
        case GL_LESS:     return GL_GREATER;
        case GL_GREATER:  return GL_LESS;
        case GL_LEQUAL:   return GL_GEQUAL;
        case GL_GEQUAL:   return GL_LEQUAL;
        // EQUAL/NOTEQUAL/ALWAYS/NEVER are direction-independent under a monotone
        // depth remap and must NOT flip.
        default:          return func;
    }
}

//static
void LLGLDepthTest::initClass()
{
    sDepthEnabled = GL_FALSE;
    sDepthFunc = GL_LESS;
    sWriteEnabled = GL_TRUE;
    if (gGLManager.mInited)
    {
        glDepthFunc(remap(sDepthFunc));
    }
}

void LLGLDepthTest::rebase()
{
    // Force the physical depth func to agree with the current translation of the tracked
    // (semantic) func. Used by the reverse-Z latch after LLRender::sReverseZ toggles.
    gGL.flush();
    if (issue_opengl_state())
    {
        glDepthFunc(remap(sDepthFunc));
        stop_glerror();
    }
}

LLGLDepthTest::LLGLDepthTest(GLboolean depth_enabled, GLboolean write_enabled, GLenum depth_func)
: mPrevDepthEnabled(sDepthEnabled), mPrevDepthFunc(sDepthFunc), mPrevWriteEnabled(sWriteEnabled)
{
    const bool issue_gl = issue_opengl_state();
    if (issue_gl)
    {
        stop_glerror();
    }
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    if (issue_gl)
    {
        checkState();
    }

    if (!depth_enabled)
    { // always disable depth writes if depth testing is disabled
      // GL spec defines this as a requirement, but some implementations allow depth writes with testing disabled
      // The proper way to write to depth buffer with testing disabled is to enable testing and use a depth_func of GL_ALWAYS
        write_enabled = GL_FALSE;
    }

    if (depth_enabled != sDepthEnabled)
    {
        gGL.flush();
        if (issue_gl)
        {
            if (depth_enabled) glEnable(GL_DEPTH_TEST);
            else glDisable(GL_DEPTH_TEST);
        }
        sDepthEnabled = depth_enabled;
    }
    if (depth_func != sDepthFunc)
    {
        gGL.flush();
        if (issue_gl)
        {
            glDepthFunc(remap(depth_func));
        }
        sDepthFunc = depth_func;
    }
    if (write_enabled != sWriteEnabled)
    {
        gGL.flush();
        if (issue_gl)
        {
            glDepthMask(write_enabled);
        }
        sWriteEnabled = write_enabled;
    }
}

LLGLDepthTest::~LLGLDepthTest()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    const bool issue_gl = issue_opengl_state();
    if (issue_gl)
    {
        checkState();
    }
    if (sDepthEnabled != mPrevDepthEnabled )
    {
        gGL.flush();
        if (issue_gl)
        {
            if (mPrevDepthEnabled) glEnable(GL_DEPTH_TEST);
            else glDisable(GL_DEPTH_TEST);
        }
        sDepthEnabled = mPrevDepthEnabled;
    }
    if (sDepthFunc != mPrevDepthFunc)
    {
        gGL.flush();
        if (issue_gl)
        {
            glDepthFunc(remap(mPrevDepthFunc));
        }
        sDepthFunc = mPrevDepthFunc;
    }
    if (sWriteEnabled != mPrevWriteEnabled )
    {
        gGL.flush();
        if (issue_gl)
        {
            glDepthMask(mPrevWriteEnabled);
        }
        sWriteEnabled = mPrevWriteEnabled;
    }
}

void LLGLDepthTest::checkState()
{
    if (gDebugGL && issue_opengl_state())
    {
        GLint func = 0;
        GLboolean mask = GL_FALSE;

        glGetIntegerv(GL_DEPTH_FUNC, &func);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);

        if (glIsEnabled(GL_DEPTH_TEST) != sDepthEnabled ||
            sWriteEnabled != mask ||
            remap(sDepthFunc) != (GLenum)func)
        {
            if (gDebugSession)
            {
                gFailLog << "Unexpected depth testing state." << std::endl;
            }
            else
            {
                LL_GL_ERRS << "Unexpected depth testing state." << LL_ENDL;
            }
        }
    }
}



#if LL_WINDOWS
// Expose desired use of high-performance graphics processor to Optimus driver and to AMD driver
// https://docs.nvidia.com/gameworks/content/technologies/desktop/optimus.htm
extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif


