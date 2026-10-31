/**
 * @file llsdl_macos.mm
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

#ifdef LL_DARWIN

#import <Cocoa/Cocoa.h>

#include "SDL3/SDL.h"

#include "llsdl_macos.h"

@interface LLLoginAccessibilityBridge : NSObject
@property(nonatomic, assign) LLSdlMacOSLoginAXCallbacks callbacks;
@property(nonatomic, strong) NSView* contentView;
@property(nonatomic, copy) NSArray* previousChildren;
@property(nonatomic, assign, getter=isActive) BOOL active;
@end

@implementation LLLoginAccessibilityBridge
@end

@interface LLLoginAccessibilityElement : NSAccessibilityElement
@property(nonatomic, strong) LLLoginAccessibilityBridge* bridge;
@property(nonatomic, assign) LLSdlMacOSLoginAXControl control;
@end

@implementation LLLoginAccessibilityElement

- (NSRect)accessibilityFrame
{
    LLLoginAccessibilityBridge* bridge = self.bridge;
    LLSdlMacOSNormalizedRect rect{};
    if (!bridge.isActive || !bridge.callbacks.get_control_rect ||
        !bridge.callbacks.get_control_rect(bridge.callbacks.context, self.control, &rect))
    {
        return NSZeroRect;
    }

    const NSRect bounds = bridge.contentView.bounds;
    const NSRect view_rect = NSMakeRect(
        bounds.origin.x + rect.x * bounds.size.width,
        bounds.origin.y + rect.y * bounds.size.height,
        rect.width * bounds.size.width,
        rect.height * bounds.size.height);
    return NSAccessibilityFrameInView(bridge.contentView, view_rect);
}

- (id)accessibilityValue
{
    LLLoginAccessibilityBridge* bridge = self.bridge;
    if (!bridge.isActive)
    {
        return @"";
    }

    if (self.control == LLSdlMacOSLoginAXControl::USERNAME && bridge.callbacks.get_username)
    {
        const std::string username = bridge.callbacks.get_username(bridge.callbacks.context);
        return [NSString stringWithUTF8String:username.c_str()];
    }

    // Never disclose password contents through the accessibility API.
    return @"";
}

- (void)setAccessibilityValue:(id)value
{
    LLLoginAccessibilityBridge* bridge = self.bridge;
    if (!bridge.isActive || ![value isKindOfClass:[NSString class]])
    {
        return;
    }

    const std::string text([(NSString*)value UTF8String]);
    const LLSdlMacOSLoginAXCallbacks callbacks = bridge.callbacks;
    const LLSdlMacOSLoginAXControl control = self.control;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!bridge.isActive)
        {
            return;
        }
        if (control == LLSdlMacOSLoginAXControl::USERNAME && callbacks.set_username)
        {
            callbacks.set_username(callbacks.context, text);
        }
        else if (control == LLSdlMacOSLoginAXControl::PASSWORD && callbacks.set_password)
        {
            callbacks.set_password(callbacks.context, text);
        }
    });
}

- (BOOL)isAccessibilityEnabled
{
    LLLoginAccessibilityBridge* bridge = self.bridge;
    if (!bridge.isActive)
    {
        return NO;
    }
    if (self.control != LLSdlMacOSLoginAXControl::CONNECT || !bridge.callbacks.is_connect_enabled)
    {
        return YES;
    }
    return bridge.callbacks.is_connect_enabled(bridge.callbacks.context);
}

- (BOOL)isAccessibilityProtectedContent
{
    return self.control == LLSdlMacOSLoginAXControl::PASSWORD;
}

- (BOOL)accessibilityPerformPress
{
    LLLoginAccessibilityBridge* bridge = self.bridge;
    if (!bridge.isActive || self.control != LLSdlMacOSLoginAXControl::CONNECT ||
        !bridge.callbacks.perform_connect || !self.isAccessibilityEnabled)
    {
        return NO;
    }

    const LLSdlMacOSLoginAXCallbacks callbacks = bridge.callbacks;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (bridge.isActive)
        {
            callbacks.perform_connect(callbacks.context);
        }
    });
    return YES;
}

@end

static LLLoginAccessibilityBridge* sLoginAccessibilityBridge = nil;

void ll_sdl_macos_strip_default_close_shortcut()
{
    @autoreleasepool
    {
        // SDL3's Cocoa backend auto-creates a default menu bar (it does this
        // whenever [NSApp mainMenu] is nil at registration) whose "Window"
        // menu contains a "Close" item bound to Cmd+W via -performClose:. That
        // native item swallows Cmd+W before it can reach the viewer's own
        // in-window "Close Window" accelerator (menu_viewer.xml -> control|W ->
        // File.CloseWindow, which just closes the frontmost floater). Worse,
        // -performClose: on the sole viewer window triggers
        // SDL_EVENT_WINDOW_CLOSE_REQUESTED, which LLWindowSDL turns into a full
        // application quit. Clear the key equivalent so Cmd+W falls through to
        // LLMenuGL instead of quitting the viewer. The menu item itself stays
        // (still clickable); only its keyboard shortcut is removed.
        // SDL parks "Close" in the menu it hands to -setWindowsMenu:, but scan
        // every top-level submenu rather than assuming that location, so this
        // keeps working if SDL ever rearranges its default bar.
        NSMenu* main_menu = [NSApp mainMenu];
        if (!main_menu)
        {
            return;
        }

        for (NSMenuItem* top_item in [main_menu itemArray])
        {
            NSMenu* submenu = [top_item submenu];
            if (!submenu)
            {
                continue;
            }

            for (NSMenuItem* item in [submenu itemArray])
            {
                if ([item action] == @selector(performClose:))
                {
                    [item setKeyEquivalent:@""];
                    [item setKeyEquivalentModifierMask:0];
                }
            }
        }
    }
}

void ll_sdl_macos_make_window_key_deferred(struct SDL_Window* window)
{
    if (!window)
    {
        return;
    }

    NSWindow* ns_window = (__bridge NSWindow*)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    if (!ns_window)
    {
        return;
    }

    // After a native dialog closes, SDL only reactivates the app, leaving no
    // key window: keystrokes hit no responder and AppKit beeps. Re-key the
    // window so it becomes key again and SDL emits FOCUS_GAINED, restoring input.
    // Defer to the next main-queue turn so this runs after SDL's own
    // ReactivateAfterDialog instead of being clobbered by it.
    dispatch_async(dispatch_get_main_queue(), ^{
        [NSApp activateIgnoringOtherApps:YES];
        [ns_window makeKeyAndOrderFront:nil];
    });
}

void ll_sdl_macos_install_login_accessibility(void* cocoa_window,
                                               const LLSdlMacOSLoginAXCallbacks& callbacks)
{
    if (!cocoa_window || !callbacks.context || sLoginAccessibilityBridge)
    {
        return;
    }

    NSWindow* window = (__bridge NSWindow*)cocoa_window;
    NSView* content_view = window.contentView;
    if (!content_view)
    {
        return;
    }

    LLLoginAccessibilityBridge* bridge = [LLLoginAccessibilityBridge new];
    bridge.callbacks = callbacks;
    bridge.contentView = content_view;
    bridge.previousChildren = content_view.accessibilityChildren;
    bridge.active = YES;

    LLLoginAccessibilityElement* username =
        [LLLoginAccessibilityElement accessibilityElementWithRole:NSAccessibilityTextFieldRole
                                                             frame:NSZeroRect
                                                             label:@"Username"
                                                            parent:content_view];
    username.bridge = bridge;
    username.control = LLSdlMacOSLoginAXControl::USERNAME;
    username.accessibilityIdentifier = @"alchemy.login.username";
    username.accessibilityPlaceholderValue = @"Username";

    LLLoginAccessibilityElement* password =
        [LLLoginAccessibilityElement accessibilityElementWithRole:NSAccessibilityTextFieldRole
                                                             frame:NSZeroRect
                                                             label:@"Password"
                                                            parent:content_view];
    password.bridge = bridge;
    password.control = LLSdlMacOSLoginAXControl::PASSWORD;
    password.accessibilityIdentifier = @"alchemy.login.password";
    password.accessibilityPlaceholderValue = @"Password";
    password.accessibilitySubrole = NSAccessibilitySecureTextFieldSubrole;
    password.accessibilityProtectedContent = YES;

    LLLoginAccessibilityElement* connect =
        [LLLoginAccessibilityElement accessibilityElementWithRole:NSAccessibilityButtonRole
                                                             frame:NSZeroRect
                                                             label:@"Log In"
                                                            parent:content_view];
    connect.bridge = bridge;
    connect.control = LLSdlMacOSLoginAXControl::CONNECT;
    connect.accessibilityIdentifier = @"alchemy.login.connect";

    NSMutableArray* children = bridge.previousChildren
        ? [bridge.previousChildren mutableCopy]
        : [NSMutableArray array];
    [children addObjectsFromArray:@[username, password, connect]];
    content_view.accessibilityChildren = children;
    content_view.accessibilityChildrenInNavigationOrder = children;
    NSAccessibilityPostNotification(content_view, NSAccessibilityLayoutChangedNotification);

    sLoginAccessibilityBridge = bridge;
}

void ll_sdl_macos_uninstall_login_accessibility(void* context)
{
    LLLoginAccessibilityBridge* bridge = sLoginAccessibilityBridge;
    if (!bridge || bridge.callbacks.context != context)
    {
        return;
    }

    bridge.active = NO;
    bridge.contentView.accessibilityChildren = bridge.previousChildren;
    bridge.contentView.accessibilityChildrenInNavigationOrder = bridge.previousChildren;
    NSAccessibilityPostNotification(bridge.contentView, NSAccessibilityLayoutChangedNotification);
    sLoginAccessibilityBridge = nil;
}

#endif // LL_DARWIN
