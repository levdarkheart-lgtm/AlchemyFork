/**
 * @file llgl.h
 * @brief LLGL definition
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

#ifndef LL_LLGL_H
#define LL_LLGL_H

// This file contains various stuff for handling gl extensions and other gl related stuff.

#include <functional>
#include <string>
#include <boost/unordered_map.hpp>
#include <list>

#include "llglheaders.h"

#include "llerror.h"
#include "v4color.h"
#include "llstring.h"
#include "stdtypes.h"
#include "v4math.h"
#include "llplane.h"
#include "llgltypes.h"
#include "llinstancetracker.h"

#include "llmatrix4a.h"

extern bool gDebugGL;
extern bool gDebugSession;
extern bool gDebugGLSession;
extern llofstream gFailLog;

void ll_init_fail_log(std::string filename);

void ll_fail(std::string msg);

void ll_close_fail_log();

class LLSD;

// Historical version convention retained by legacy callers and CPU tests.
// The original GL version was major + minor * 0.1f, biased down by 0.01 for
// floating-point comparisons. Native renderer admission belongs to the Metal
// device owner; native capabilities are published through ALRenderCapabilities.
const F32 GL_MINIMUM_VERSION = 4.09f;

// Retained CPU compatibility metadata and native diagnostic reporting.
class LLGLManager
{
public:
    LLGLManager();

    std::string getRawGLString(); // For sending to simulator

    // Apple's Metal-backed driver aborts on tessellated draws while a
    // GL_PRIMITIVES_GENERATED query is active without transform feedback.
    // Timer and sample queries, and primitive queries for other draws, work.
    bool canQueryPrimitives(bool tessellated) const
    {
        return !tessellated || !mIsApple;
    }

    // Original version-floor predicate, retained for existing CPU callers.
    static bool meetsVersionFloor(F32 gl_version);

    // Intel's drivers outside macOS short of GL 4.6: chiefly Haswell and Broadwell on Windows, which
    // report 4.3 or 4.4 and claim more than they hold to. The feature manager holds them to its
    // IntelLegacy list at every start, and the shader manager keeps them from compute and the shader
    // cache, which no list can hold.
    bool isIntelLegacy() const;

    bool mInited;
    bool mIsDisabled;

    // OpenGL limits
    S32 mNumTextureImageUnits;
    S32 mGLMaxVertexRange;
    S32 mGLMaxIndexRange;
    S32 mGLMaxTextureSize;
    F32 mMaxAnisotropy = 0.f;
    S32 mMaxVaryingVectors = 0;

    bool mHasAnisotropic = false;
    // Clip control (glClipControl for GL_ZERO_TO_ONE depth range). Core in 4.5, also
    // reachable as GL_ARB_clip_control. Gates the reverse-Z depth path; only true once
    // the entry point has actually resolved, so callers may trust it directly. Absent on
    // macOS GL 4.1, which stays forward-Z.
    bool mHasClipControl = false;

    // Vendor-specific extensions
    bool mHasAMDAssociations = false;
    bool mHasNVXGpuMemoryInfo = false;

    bool mIsAMD;
    bool mIsNVIDIA;
    bool mIsIntel;
    bool mIsApple = false;
    // True for any Mesa driver (radeonsi, iris, llvmpipe, zink, ...). Detected
    // from the GL_VERSION string; used to gate Mesa-specific workarounds.
    bool mIsMesa = false;
    // True when GL draws on the CPU: Mesa's llvmpipe, softpipe or swrast, or WARP behind
    // WSL's d3d12. What Mesa falls back to when the hardware driver does not load.
    // Detected from GL_RENDERER, on Linux only.
    bool mIsSoftwareRenderer = false;

    // hints to the render pipe
    U32 mDownScaleMethod = 0; // see settings.xml RenderDownScaleMethod

    // Compatibility requirement metadata still read by startup error handling.
    bool mHasRequirements;
    std::string mUnmetRequirement;

    F32 mGLVersion; // e.g = 1.4
    S32 mGLSLVersionMajor;
    S32 mGLSLVersionMinor;
    std::string mDriverVersionVendorString;
    std::string mGLVersionString;

    U32 mVRAM; // VRAM in MB

    std::string getGLInfoString();
    void printGLInfoString();
    void getGLInfo(LLSD& info);

    void asLLSD(LLSD& info);

    // In ALL CAPS
    std::string mGLVendor;
    std::string mGLVendorShort;

    // In ALL CAPS
    std::string mGLRenderer;

    // GL_VENDOR and GL_RENDERER in the driver's own case, as mGLVersionString is GL_VERSION.
    std::string mGLVendorString;
    std::string mGLRendererString;

    // Retained legacy driver identity for existing metadata consumers.
    // Native identity is reported from ALRenderCapabilities.
    std::string mGLIdentity;
};

extern LLGLManager gGLManager;

class LLQuaternion;
class LLMatrix4;

void rotate_quat(LLQuaternion& rotation);

// Empty legacy checkpoints retained until their callers are migrated. Native
// errors belong to typed Metal API results, command completion and the Metal
// API/shader validation layers; there is no global Metal error queue to poll.
void flush_glerror();
void assert_glerror();

# define stop_glerror() assert_glerror()
# define llglassertok() assert_glerror()

// Retained source compatibility for callers pending migration.
#if LL_DARWIN && !LL_RELEASE_FOR_DOWNLOAD
#define STOP_GLERROR stop_glerror()
#else
#define STOP_GLERROR
#endif

#define llglassertok_always() assert_glerror()

////////////////////////
//
// Note: U32's are GLEnum's...
//

// CPU render-state scopes consumed by native draw snapshots.

/*
    GL STATE MANAGEMENT DESCRIPTION

    LLGLState and its two subclasses, LLGLEnable and LLGLDisable, manage semantic
    enable/disable state. Native draw capture reads the CPU map when building
    explicit Metal pipeline/encoder descriptors.

    When a path temporarily enables a capability, make an instance of
    LLGLEnable with the state you want to set, and it will be restored to its
    original state when that instance of LLGLEnable is destroyed.  It is good practice
    to exploit stack frame controls for optimal setting/unsetting and readability of
    code.  In llglstates.h, there are a collection of helper classes that define groups
    of enables/disables that can cause multiple states to be set with the creation of
    one instance.

    Sample usage:

    //enable blending for rendering hud objects
    //INCORRECT USAGE
    LLGLEnable blend(GL_BLEND);
    renderHUD();
    LLGLDisable blend(GL_BLEND);

    //CORRECT USAGE
    {
        LLGLEnable blend(GL_BLEND);
        renderHUD();
    }

    If a state is to be set on a conditional, the following mechanism
    is useful:

    {
        LLGLEnable blend(blend_hud ? GL_BLEND: 0);
        renderHUD();
    }

    A LLGLState initialized with a parameter of 0 does nothing.

    LLGLState works by maintaining a map of the current semantic states, and ignoring redundant
    enables/disables.  If a redundant call is attempted, it becomes a noop, otherwise,
    it is set in the constructor and reset in the destructor.

*/

class LLGLState
{
public:
    static void initClass();

    // Seed externally initialized semantic state before scoped owners use it.
    // The map describes the main renderer; call on the main thread only.
    static void seedState(LLGLenum state, LLGLboolean enabled) { sStateMap[state] = enabled; }

    // Read the renderer's authoritative capability cache without a driver
    // round trip. Native draw capture snapshots this at the draw boundary.
    static bool isEnabled(LLGLenum state);

    // Empty legacy driver-state checkpoint; native validation occurs at the
    // explicit Metal resource/encoder boundary, never through global queries.
    static void checkStates(GLboolean writeAlpha = GL_TRUE);

protected:
    static boost::unordered_map<LLGLenum, LLGLboolean> sStateMap;

public:
    enum { CURRENT_STATE = -2, DISABLED_STATE = 0, ENABLED_STATE = 1 };
    LLGLState(LLGLenum state, S32 enabled = CURRENT_STATE);
    ~LLGLState();
    // A copy would restore the cap a second time.
    LLGLState(const LLGLState&) = delete;
    LLGLState& operator=(const LLGLState&) = delete;
    void setEnabled(S32 enabled);
    void enable() { setEnabled(ENABLED_STATE); }
    void disable() { setEnabled(DISABLED_STATE); }
protected:
    LLGLenum mState;
    bool mWasEnabled;
    bool mIsEnabled;
};

// Enable with functor
class LLGLEnableFunc : LLGLState
{
public:
    LLGLEnableFunc(LLGLenum state, bool enable, std::function<void()> func)
        : LLGLState(state, enable)
    {
        if (enable)
        {
            func();
        }
    }
};

/// TODO: Being deprecated.
class LLGLEnable : public LLGLState
{
public:
    LLGLEnable(LLGLenum state) : LLGLState(state, ENABLED_STATE) {}
};

/// TODO: Being deprecated.
class LLGLDisable : public LLGLState
{
public:
    LLGLDisable(LLGLenum state) : LLGLState(state, DISABLED_STATE) {}
};

/*
  Store and modify projection matrix to create an oblique
  projection that clips to the specified plane.  Oblique
  projections alter values in the depth buffer, so this
  class should not be used mid-renderpass.

  Restores projection matrix on destruction.
  GL_MODELVIEW_MATRIX is active whenever program execution
  leaves this class.
  Does not stack.
*/
class LLGLUserClipPlane
{
public:

    LLGLUserClipPlane(const LLPlane& plane, const LLMatrix4a& modelview, const LLMatrix4a& projection, bool apply = true);
    ~LLGLUserClipPlane();

    void setPlane(F32 a, F32 b, F32 c, F32 d);
    void disable();

private:
    bool mApply;

    LLMatrix4a mProjection;
    LLMatrix4a mModelview;
};

/*
    Interface for objects that need periodic GL updates applied to them.
    Used to synchronize GL updates with GL thread.
*/
class LLGLUpdate
{
public:

    static std::list<LLGLUpdate*> sGLQ;

    bool mInQ;
    LLGLUpdate()
        : mInQ(false)
    {
    }
    virtual ~LLGLUpdate()
    {
        if (mInQ)
        {
            std::list<LLGLUpdate*>::iterator iter = std::find(sGLQ.begin(), sGLQ.end(), this);
            if (iter != sGLQ.end())
            {
                sGLQ.erase(iter);
            }
        }
    }
    virtual void updateGL() = 0;
};

#include "llglstates.h"

extern bool gClothRipple;
extern bool gHeadlessClient;
extern bool gNonInteractive;
extern bool gGLActive;

#endif // LL_LLGL_H
