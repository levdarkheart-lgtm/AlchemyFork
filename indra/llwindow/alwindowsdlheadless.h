/**
 * @file alwindowsdlheadless.h
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

#pragma once

#include "llwindowheadless.h"
#include "SDL3/SDL.h"

#include <memory>
#if !LL_DARWIN
#include "llmutex.h"
#include <set>
#endif

#if LL_DARWIN
class ALMetalDevice;
class ALMetalFrameScheduler;
class ALMetalSurface;
#endif

// The headless backend for anything that renders: the appearance utility, a
// viewer with no desktop and renderer tests. An SDL window is created hidden
// and never shown. Darwin owns only an offscreen Metal device and frame
// scheduler; it deliberately creates no GL window or context. Other platforms
// retain their migration GL context until their platform cutover.
//
// Everything a window does besides own a context is LLWindowHeadless's
// virtual state: size, position, cursor, visibility. Input never arrives, so
// the keyboard is the headless one.
class ALWindowSDLHeadless final : public LLWindowHeadless
{
public:
    ALWindowSDLHeadless(LLWindowCallbacks* callbacks,
        const std::string& title, const std::string& name,
        S32 x, S32 y, S32 width, S32 height,
        U32 flags, bool fullscreen, bool clear_background,
        bool enable_vsync, bool ignore_pixel_depth, U32 fsaa_samples);
    ~ALWindowSDLHeadless() override;

    bool isValid() override
    {
#if LL_DARWIN
        return mWindow && mMetalDevice && mMetalFrames;
#else
        return mContext != nullptr;
#endif
    }

    // Tears the rendering backend and window down; LLWindowManager::destroyWindow
    // calls it before it quits SDL. The destructor finishes anything left.
    void close() override;

    void swapBuffers() override;
    void* getPlatformWindow() override { return mWindow; }

#if LL_DARWIN
    ALMetalDevice* getMetalDevice() const noexcept override { return mMetalDevice.get(); }
    ALMetalFrameScheduler* getMetalFrameScheduler() const noexcept override { return mMetalFrames.get(); }
    ALMetalSurface* getMetalSurface() const noexcept override { return nullptr; }
#endif

    // Worker-thread GL contexts remain available only on non-Darwin migration
    // builds. Metal uploads share the device/queue without a graphics context.
    void* createSharedContext() override;
    void makeContextCurrent(void* context) override;
    void destroySharedContext(void* context) override;

private:
    SDL_Window* mWindow = nullptr;
#if LL_DARWIN
    std::unique_ptr<ALMetalDevice> mMetalDevice;
    std::unique_ptr<ALMetalFrameScheduler> mMetalFrames;
#else
    SDL_GLContext mContext = nullptr;
    // Live shared-context handles, so any a worker failed to release can be
    // reclaimed before the main context goes.
    LLMutex mSharedCtxMutex;
    std::set<void*> mSharedContexts;
#endif
};
