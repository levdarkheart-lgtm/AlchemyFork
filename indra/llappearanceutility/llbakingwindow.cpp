/**
 * @file llbakingwindow.cpp
 * @brief Implementation of LLBakingWindow class.
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
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

// linden includes
#include "linden_common.h"

// project includes
#include "llappappearanceutility.h"
#include "llbakingshadermgr.h"
#include "llbakingwindow.h"
#include "llgl.h"
#include "llgltexture.h"
#include "llvertexbuffer.h"

LLBakingWindow::LLBakingWindow(S32 width, S32 height, ALWindowBackend backend)
{
    const S32 WINDOW_ORIGIN_X = 0;
    const S32 WINDOW_ORIGIN_Y = 0;
    const U32 FLAGS = 32; // *TODO: Why did mapserver use this?  mFlags looks unused.
    const bool NO_FULLSCREEN = false;
    const bool NO_CLEAR_BG = false;
    const bool NO_DISABLE_VSYNC = false;
    const bool NO_IGNORE_PIXEL_DEPTH = false;
    mWindow = LLWindowManager::createWindow(this,
        "appearanceutility", "Appearance Utility",
        WINDOW_ORIGIN_X, WINDOW_ORIGIN_Y,
        width, height,
        FLAGS,
        NO_FULLSCREEN,
        NO_CLEAR_BG,
        NO_DISABLE_VSYNC, //gSavedSettings.getBOOL("DisableVerticalSync"),
        backend,
        NO_IGNORE_PIXEL_DEPTH); //gIgnorePixelDepth = false

    if (nullptr == mWindow)
    {
        throw LLAppException(RV_UNABLE_TO_INIT_GL);
    }

    try
    {
        // Validate the window-owned native backend before bringing up render
        // singletons. The catch below also reclaims a window whose renderer
        // or shader initialization fails after borrowed pointers are installed.
        if (!mWindow->getMetalDevice() || !mWindow->getMetalFrameScheduler())
        {
            throw LLAppException(
                RV_UNABLE_TO_INIT_GL,
                " Native Metal appearance backend is unavailable.");
        }
        LLVertexBuffer::setMetalDevice(mWindow->getMetalDevice());
        LLVertexBuffer::setMetalBufferAuthority(true);
        LLImageGL::setMetalDevice(mWindow->getMetalDevice());
        LLImageGL::setMetalTextureAuthority(true);
        gGL.setMetalBackend(
            mWindow->getMetalDevice(),
            mWindow->getMetalFrameScheduler(),
            mWindow->getMetalSurface());
        LLVertexBuffer::initClass(mWindow);

        if (!gGL.initMetal(true))
        {
            throw LLAppException(
                RV_UNABLE_TO_INIT_GL,
                " Native Metal renderer initialization failed.");
        }
        if (!LLBakingShaderMgr::sInitialized)
        { //immediately initialize shaders
            LLBakingShaderMgr* shader_mgr = LLBakingShaderMgr::instance();
            LLBakingShaderMgr::sInitialized = true;
            if (!shader_mgr->setShaders())
            {
                throw LLAppException(
                    RV_UNABLE_TO_INIT_GL,
                    " Native Metal appearance shader initialization failed.");
            }
        }

        LLImageGL::initClass(mWindow, LLGLTexture::MAX_GL_IMAGE_CATEGORY, true, false);
    }
    catch (...)
    {
        cleanup();
        throw;
    }

    // mWindow->show();
    // mWindow->bringToFront();
    // mWindow->focusClient();
    // mWindow->gatherInput();
}

LLBakingWindow::~LLBakingWindow()
{
    cleanup();
}

void LLBakingWindow::cleanup()
{
    if (LLBakingShaderMgr::sInitialized)
    {
        LLBakingShaderMgr::instance()->unloadShaders();
        LLBakingShaderMgr::sInitialized = false;
    }
    gGL.setMetalBackend(nullptr, nullptr, nullptr);
    LLImageGL::setMetalTextureAuthority(false);
    LLImageGL::setMetalDevice(nullptr);
    LLVertexBuffer::setMetalBufferAuthority(false);
    LLVertexBuffer::setMetalDevice(nullptr);
    if (mWindow)
    {
        LLWindowManager::destroyWindow(mWindow);
    }
    mWindow = nullptr;
}

void LLBakingWindow::swapBuffers()
{
    mWindow->swapBuffers();
}
