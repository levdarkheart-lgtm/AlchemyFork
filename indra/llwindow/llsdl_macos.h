/**
 * @file llsdl_macos.h
 * @brief macOS-specific SDL helpers
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2024, Linden Research, Inc.
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

#pragma once

#include <string>

// Remove the Cmd+W key equivalent from the "Close" item in the default macOS
// menu bar that SDL3's Cocoa backend creates for us. See the implementation in
// llsdl_macos.mm for the full rationale. Safe to call once after the first
// SDL_INIT_VIDEO (when SDL has registered the app and built its menu).
void ll_sdl_macos_strip_default_close_shortcut();

// Make the given SDL window key again after a native file dialog closes.
struct SDL_Window;
void ll_sdl_macos_make_window_key_deferred(struct SDL_Window* window);

// A deliberately narrow accessibility bridge for the development viewer's
// custom-drawn login controls. The Cocoa accessibility tree cannot otherwise
// see LLUI controls because SDL presents the whole viewer as one NSView.
//
// Password reads are intentionally absent from this contract: AppKit exposes
// the password element as protected content and may only write a replacement
// value into it. The owner must uninstall the bridge before its context dies.
enum class LLSdlMacOSLoginAXControl
{
    USERNAME,
    PASSWORD,
    CONNECT
};

struct LLSdlMacOSNormalizedRect
{
    double x;
    double y;
    double width;
    double height;
};

struct LLSdlMacOSLoginAXCallbacks
{
    void* context = nullptr;
    std::string (*get_username)(void* context) = nullptr;
    void (*set_username)(void* context, const std::string& value) = nullptr;
    void (*set_password)(void* context, const std::string& value) = nullptr;
    bool (*get_control_rect)(void* context,
                             LLSdlMacOSLoginAXControl control,
                             LLSdlMacOSNormalizedRect* rect) = nullptr;
    bool (*is_connect_enabled)(void* context) = nullptr;
    void (*perform_connect)(void* context) = nullptr;
};

void ll_sdl_macos_install_login_accessibility(void* cocoa_window,
                                               const LLSdlMacOSLoginAXCallbacks& callbacks);
void ll_sdl_macos_uninstall_login_accessibility(void* context);
