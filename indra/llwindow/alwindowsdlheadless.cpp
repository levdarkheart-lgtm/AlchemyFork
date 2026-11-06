/**
 * @file alwindowsdlheadless.cpp
 * @brief A window never shown, with an offscreen rendering backend
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

#include "alwindowsdlheadless.h"

#include "llrender.h"
#include "almetaldevice.h"
#include "almetalframe.h"

ALWindowSDLHeadless::ALWindowSDLHeadless(LLWindowCallbacks* callbacks,
    const std::string& title, const std::string& name,
    S32 x, S32 y, S32 width, S32 height,
    U32 flags, bool fullscreen, bool clear_background,
    bool enable_vsync, bool ignore_pixel_depth, U32 fsaa_samples)
:   LLWindowHeadless(callbacks, title, name, x, y, width, height, flags,
                     fullscreen, clear_background, enable_vsync, true, ignore_pixel_depth)
{
    setFSAASamples(fsaa_samples);

    mWindow = SDL_CreateWindow(
        title.c_str(), width, height, SDL_WINDOW_HIDDEN);
    if (!mWindow)
    {
        LL_WARNS("Window") << "Hidden window creation failed: " << SDL_GetError() << LL_ENDL;
        return;
    }

    // The hidden backend is also the headless appearance-baking backend. It
    // has no drawable CAMetalLayer, but it still owns the same native device
    // and ordered frame scheduler required by offscreen Metal captures.
    std::string metal_error;
    mMetalDevice = ALMetalDevice::create(&metal_error);
    if (!mMetalDevice)
    {
        LL_WARNS("Metal") << "Hidden Metal device initialization failed: "
                           << metal_error << LL_ENDL;
        return;
    }
    mMetalFrames = ALMetalFrameScheduler::create(
        *mMetalDevice, 3, &metal_error);
    if (!mMetalFrames)
    {
        LL_WARNS("Metal") << "Hidden Metal frame scheduler initialization failed: "
                           << metal_error << LL_ENDL;
        mMetalDevice.reset();
        return;
    }
    LL_INFOS("Metal") << "Hidden offscreen Metal backend initialized on "
                       << mMetalDevice->info().name << LL_ENDL;

    LL_INFOS("Window") << "Hidden window " << width << "x" << height
                       << " on SDL video driver " << SDL_GetCurrentVideoDriver() << LL_ENDL;
}

ALWindowSDLHeadless::~ALWindowSDLHeadless()
{
    close();
}

void ALWindowSDLHeadless::close()
{
    LLWindowHeadless::close();

    // Render infrastructure only borrows these objects. Detach and drain it
    // before the window-owned scheduler and device disappear.
    gGL.setMetalBackend(nullptr, nullptr, nullptr);
    if (mMetalFrames)
    {
        std::string metal_error;
        if (!mMetalFrames->waitUntilIdle(5000, &metal_error))
        {
            LL_WARNS("Metal") << "Hidden Metal teardown did not reach idle: "
                               << metal_error << LL_ENDL;
        }
    }
    mMetalFrames.reset();
    mMetalDevice.reset();
    if (mWindow)
    {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }
}

void ALWindowSDLHeadless::swapBuffers()
{
    // Offscreen Metal work is submitted by its owning frame scheduler.
}

void* ALWindowSDLHeadless::createSharedContext()
{
    // The native upload path shares a Metal device and command queue.
    return nullptr;
}

void ALWindowSDLHeadless::makeContextCurrent(void* context)
{
    llassert(context == nullptr);
}

void ALWindowSDLHeadless::destroySharedContext(void* context)
{
    llassert(context == nullptr);
}
