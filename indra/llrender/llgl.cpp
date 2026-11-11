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

// Retained CPU render-state helpers and native renderer diagnostics.

#include "linden_common.h"

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

// Remaining legacy calls are owned by the state/error helpers and the
// vertex/image consumers still being converted. Native device setup never
// resolves a GL entrypoint.
#if LL_WINDOWS
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB = nullptr;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = nullptr;
PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV = nullptr;
PFNWGLDXCLOSEDEVICENVPROC wglDXCloseDeviceNV = nullptr;
PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV = nullptr;
PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV = nullptr;
PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV = nullptr;
PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV = nullptr;
#endif

PFNGLTEXPARAMETERIPROC glTexParameteri = nullptr;
PFNGLTEXPARAMETERIVPROC glTexParameteriv = nullptr;
PFNGLDEPTHMASKPROC glDepthMask = nullptr;
PFNGLDISABLEPROC glDisable = nullptr;
PFNGLENABLEPROC glEnable = nullptr;
PFNGLDEPTHFUNCPROC glDepthFunc = nullptr;
PFNGLGETBOOLEANVPROC glGetBooleanv = nullptr;
PFNGLGETERRORPROC glGetError = nullptr;
PFNGLGETINTEGERVPROC glGetIntegerv = nullptr;
PFNGLISENABLEDPROC glIsEnabled = nullptr;
PFNGLDRAWARRAYSPROC glDrawArrays = nullptr;
PFNGLDRAWELEMENTSPROC glDrawElements = nullptr;
PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D = nullptr;
PFNGLDELETETEXTURESPROC glDeleteTextures = nullptr;
PFNGLGENTEXTURESPROC glGenTextures = nullptr;
PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements = nullptr;
PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;
PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer = nullptr;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap = nullptr;
PFNGLTEXSTORAGE2DPROC glTexStorage2D = nullptr;
PFNGLOBJECTLABELPROC glObjectLabel = nullptr;

LLGLManager gGLManager;

LLGLManager::LLGLManager() :
    mInited(false),
    mIsDisabled(false),
    mNumTextureImageUnits(1),
    mIsAMD(false),
    mIsNVIDIA(false),
    mIsIntel(false),
    mHasRequirements(true),
    mGLVersion(1.0f),
    mGLSLVersionMajor(0),
    mGLSLVersionMinor(0),
    mVRAM(0),
    mGLMaxVertexRange(0),
    mGLMaxIndexRange(0),
    mGLMaxTextureSize(0)
{
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
    // LLRender initializes the CPU state map and depth state before use,
    // including native Metal initialization without a GL context.
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

////////////////////////////////////////////////////////////////////////////////

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


