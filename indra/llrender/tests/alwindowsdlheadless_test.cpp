/**
 * @file alwindowsdlheadless_test.cpp
 * @brief Manual-only native hidden-window ownership and requested size.
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
#include "llwindow.h"
#include "llwindowcallbacks.h"
#include <catch2/catch_test_macros.hpp>

namespace
{
// This test really creates an SDL hidden window. It is intentionally absent
// from standard registered execution until window lifecycle testing is allowed.
// A hidden Catch tag alone must not be treated as approval to launch it.
struct WindowOwner
{
    LLWindow* previous=gWindowp;
    LLWindow* window=nullptr;
    bool close()
    {
        if (!window) return true;
        const bool result=LLWindowManager::destroyWindow(window);
        window=nullptr;
        // WindowManager historically leaves this borrowed global unchanged.
        // Restore the test's prior process state after deleting its own window.
        gWindowp=previous;
        return result;
    }
    ~WindowOwner() { close(); }
};

TEST_CASE("Native hidden SDL window retains requested size and offscreen Metal ownership",
          "[.native-window-lifecycle][almetal]")
{
    REQUIRE(gWindowp==nullptr);
    LLWindowCallbacks callbacks;
    WindowOwner owner;
    constexpr S32 width=256,height=256;
    owner.window=LLWindowManager::createWindow(&callbacks,
        "native-hidden-window-test","native-hidden-window-test",0,0,width,height,0,
        false,false,false,ALWindowBackend::Hidden);
    LLWindow* window=owner.window;
    REQUIRE(window!=nullptr);
    CHECK(LLWindowManager::getBackend()==ALWindowBackend::Hidden);
    CHECK(window->getPlatformWindow()!=nullptr);
    LLCoordWindow size;
    REQUIRE(window->getSize(&size));
    CHECK(size.mX==width);
    CHECK(size.mY==height);

    // The live hidden backend is used by appearance baking: it owns native
    // offscreen device/scheduler resources without a drawable presentation layer.
    CHECK(LLWindowManager::isWindowValid(window));
    CHECK(window->getMetalDevice()!=nullptr);
    CHECK(window->getMetalFrameScheduler()!=nullptr);
    CHECK(window->getMetalSurface()==nullptr);
    CHECK(owner.close());
    CHECK_FALSE(LLWindowManager::isWindowValid(window));
}
}
