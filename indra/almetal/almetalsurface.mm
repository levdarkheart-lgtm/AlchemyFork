/**
 * @file almetalsurface.mm
 * @brief SDL-window Cocoa overlay and CAMetalLayer drawable lifecycle.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 * $/LicenseInfo$
 */

#include "almetalsurface.h"

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <utility>

// The product keeps SDL responsible for window/input lifecycle while this
// non-interactive Cocoa child view owns the sole drawable CAMetalLayer. Using
// the explicit view keeps layer format, opacity, drawable count and resize
// semantics under the native renderer without asking SDL to create a second
// graphics backend.
@interface ALMetalOverlayView : NSView
@end

@implementation ALMetalOverlayView

+ (Class)layerClass
{
    return [CAMetalLayer class];
}

- (BOOL)wantsUpdateLayer
{
    return YES;
}

- (CALayer*)makeBackingLayer
{
    return [CAMetalLayer layer];
}

- (NSView*)hitTest:(NSPoint)point
{
    return nil;
}

@end

namespace
{
    void setError(std::string* destination, const std::string& message)
    {
        if (destination)
        {
            *destination = message;
        }
    }

    bool requireMainThread(std::string* error)
    {
        if ([NSThread isMainThread])
        {
            return true;
        }
        setError(error, "SDL Metal surface mutation must run on the main thread");
        return false;
    }

    MTLPixelFormat pixelFormat(ALMetalSurfaceFormat format)
    {
        switch (format)
        {
            case ALMetalSurfaceFormat::bgra8_unorm:      return MTLPixelFormatBGRA8Unorm;
            case ALMetalSurfaceFormat::bgra8_unorm_srgb: return MTLPixelFormatBGRA8Unorm_sRGB;
            case ALMetalSurfaceFormat::bgr10_xr:         return MTLPixelFormatBGR10_XR;
            case ALMetalSurfaceFormat::rgba16_float:     return MTLPixelFormatRGBA16Float;
        }
        return MTLPixelFormatInvalid;
    }

    CGColorSpaceRef createColorSpace(const ALMetalSurfaceConfig& config)
    {
        if (config.extended_dynamic_range)
        {
            return CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearDisplayP3);
        }
        return CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    }
}

struct ALMetalDrawable::Impl
{
    id<CAMetalDrawable> drawable = nil;
    id<MTLTexture> texture = nil;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t generation = 0;
    bool presented = false;
};

struct ALMetalSurface::Impl
{
    SDL_Window* window = nullptr;
    NSView* view = nil;
    CAMetalLayer* layer = nil;
    id<MTLDevice> device = nil;
};

ALMetalDrawable::ALMetalDrawable(std::unique_ptr<Impl> impl)
: mImpl(std::move(impl))
{
}

ALMetalDrawable::~ALMetalDrawable() = default;

void* ALMetalDrawable::nativeHandle() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->drawable : nullptr;
}

void* ALMetalDrawable::nativeTexture() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->texture : nullptr;
}

std::uint32_t ALMetalDrawable::width() const noexcept
{
    return mImpl ? mImpl->width : 0;
}

std::uint32_t ALMetalDrawable::height() const noexcept
{
    return mImpl ? mImpl->height : 0;
}

std::uint64_t ALMetalDrawable::generation() const noexcept
{
    return mImpl ? mImpl->generation : 0;
}

bool ALMetalDrawable::wasPresented() const noexcept
{
    return mImpl && mImpl->presented;
}

ALMetalSurface::ALMetalSurface(std::unique_ptr<Impl> impl, ALMetalSurfaceInfo info)
: mImpl(std::move(impl)),
  mInfo(std::move(info))
{
}

ALMetalSurface::~ALMetalSurface()
{
    if (!mImpl || !mImpl->view)
    {
        return;
    }

    mImpl->layer = nil;
    NSView* view = mImpl->view;
    mImpl->view = nil;
    if ([NSThread isMainThread])
    {
        [view removeFromSuperview];
    }
    else
    {
        // AppKit view destruction is a main-thread operation. Viewer shutdown
        // normally already is there; keep exceptional teardown deterministic.
        dispatch_sync(dispatch_get_main_queue(), ^{
            [view removeFromSuperview];
        });
    }
}

std::unique_ptr<ALMetalSurface> ALMetalSurface::create(
    SDL_Window* window,
    const ALMetalDevice& device,
    const ALMetalSurfaceConfig& config,
    std::string* error)
{
    @autoreleasepool
    {
        if (!requireMainThread(error))
        {
            return nullptr;
        }
        if (!window)
        {
            setError(error, "Cannot create a Metal surface for a null SDL window");
            return nullptr;
        }
        if (!device.nativeHandle())
        {
            setError(error, "Cannot create a Metal surface without a Metal device");
            return nullptr;
        }
        if (config.maximum_drawable_count < 2 || config.maximum_drawable_count > 3)
        {
            setError(error, "CAMetalLayer maximumDrawableCount must be 2 or 3");
            return nullptr;
        }
        const MTLPixelFormat format = pixelFormat(config.format);
        if (format == MTLPixelFormatInvalid)
        {
            setError(error, "Unsupported Metal surface pixel format");
            return nullptr;
        }

        SDL_PropertiesID window_properties = SDL_GetWindowProperties(window);
        NSWindow* native_window = (__bridge NSWindow*)SDL_GetPointerProperty(
            window_properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
        NSView* container = native_window.contentView;
        if (!native_window || !container)
        {
            setError(error, "SDL window does not expose its Cocoa window and content view");
            return nullptr;
        }

        ALMetalOverlayView* view = [[ALMetalOverlayView alloc] initWithFrame:container.bounds];
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        view.wantsLayer = YES;
        CAMetalLayer* layer = (CAMetalLayer*)view.layer;
        if (!layer)
        {
            setError(error, "Could not create the Cocoa CAMetalLayer overlay");
            return nullptr;
        }
        [container addSubview:view];

        id<MTLDevice> native_device = (__bridge id<MTLDevice>)device.nativeHandle();
        layer.device = native_device;
        layer.pixelFormat = format;
        layer.framebufferOnly = config.framebuffer_only;
        layer.maximumDrawableCount = config.maximum_drawable_count;
        layer.displaySyncEnabled = config.display_sync_enabled;
        layer.allowsNextDrawableTimeout = config.allows_next_drawable_timeout;
        layer.presentsWithTransaction = NO;
        layer.opaque = config.opaque;
        layer.wantsExtendedDynamicRangeContent = config.extended_dynamic_range;

        CGColorSpaceRef color_space = createColorSpace(config);
        if (color_space)
        {
            layer.colorspace = color_space;
            CGColorSpaceRelease(color_space);
        }

        auto impl = std::make_unique<Impl>();
        impl->window = window;
        impl->view = view;
        impl->layer = layer;
        impl->device = native_device;

        ALMetalSurfaceInfo info;
        info.format = config.format;
        info.maximum_drawable_count = config.maximum_drawable_count;
        info.display_sync_enabled = config.display_sync_enabled;
        info.framebuffer_only = config.framebuffer_only;
        info.extended_dynamic_range = config.extended_dynamic_range;
        info.opaque = config.opaque;

        auto surface = std::unique_ptr<ALMetalSurface>(new ALMetalSurface(std::move(impl), info));
        if (!surface->resizeFromWindow(error))
        {
            return nullptr;
        }
        return surface;
    }
}

const ALMetalSurfaceInfo& ALMetalSurface::info() const noexcept
{
    return mInfo;
}

void* ALMetalSurface::nativeLayer() const noexcept
{
    return mImpl ? (__bridge void*)mImpl->layer : nullptr;
}

bool ALMetalSurface::resizeFromWindow(std::string* error)
{
    if (!requireMainThread(error))
    {
        return false;
    }
    if (!mImpl || !mImpl->window)
    {
        setError(error, "Metal surface has no SDL window");
        return false;
    }

    int width = 0;
    int height = 0;
    if (!SDL_GetWindowSizeInPixels(mImpl->window, &width, &height))
    {
        setError(error, std::string("SDL_GetWindowSizeInPixels failed: ") + SDL_GetError());
        return false;
    }
    return resize(static_cast<std::uint32_t>(std::max(width, 1)),
                  static_cast<std::uint32_t>(std::max(height, 1)), error);
}

bool ALMetalSurface::resize(std::uint32_t width, std::uint32_t height, std::string* error)
{
    if (!requireMainThread(error))
    {
        return false;
    }
    if (!mImpl || !mImpl->layer)
    {
        setError(error, "Metal surface has no CAMetalLayer");
        return false;
    }
    if (width == 0 || height == 0)
    {
        setError(error, "Metal drawable size must be non-zero");
        return false;
    }

    // NSView does not guarantee that a custom backing layer follows a window
    // across Retina/non-Retina displays. drawableSize remains explicit pixels;
    // contentsScale keeps Core Animation's logical composition in agreement.
    const CGFloat backing_scale = mImpl->view.window.backingScaleFactor;
    mImpl->layer.contentsScale = backing_scale > 0.0 ? backing_scale : 1.0;
    if (mInfo.width == width && mInfo.height == height)
    {
        return true;
    }

    mImpl->layer.drawableSize = CGSizeMake(width, height);
    mInfo.width = width;
    mInfo.height = height;
    ++mInfo.generation;
    return true;
}

bool ALMetalSurface::setDisplaySyncEnabled(bool enabled, std::string* error)
{
    if (!requireMainThread(error))
    {
        return false;
    }
    if (!mImpl || !mImpl->layer)
    {
        setError(error, "Metal surface has no CAMetalLayer");
        return false;
    }
    mImpl->layer.displaySyncEnabled = enabled;
    mInfo.display_sync_enabled = enabled;
    return true;
}

bool ALMetalSurface::setOpaque(bool opaque, std::string* error)
{
    if (!requireMainThread(error))
    {
        return false;
    }
    if (!mImpl || !mImpl->layer)
    {
        setError(error, "Metal surface has no CAMetalLayer");
        return false;
    }
    if (mInfo.opaque == opaque)
    {
        return true;
    }
    mImpl->layer.opaque = opaque;
    mInfo.opaque = opaque;
    return true;
}

std::unique_ptr<ALMetalDrawable> ALMetalSurface::nextDrawable(std::string* error)
{
    @autoreleasepool
    {
        if (!mImpl || !mImpl->layer)
        {
            setError(error, "Metal surface has no CAMetalLayer");
            return nullptr;
        }
        id<CAMetalDrawable> drawable = [mImpl->layer nextDrawable];
        if (!drawable)
        {
            setError(error, "CAMetalLayer returned no drawable (window may be occluded or drawable-starved)");
            return nullptr;
        }

        auto impl = std::make_unique<ALMetalDrawable::Impl>();
        impl->drawable = drawable;
        impl->texture = drawable.texture;
        impl->width = static_cast<std::uint32_t>(drawable.texture.width);
        impl->height = static_cast<std::uint32_t>(drawable.texture.height);
        impl->generation = mInfo.generation;
        return std::unique_ptr<ALMetalDrawable>(new ALMetalDrawable(std::move(impl)));
    }
}

bool ALMetalSurface::present(
    ALMetalCommandBuffer& command_buffer,
    ALMetalDrawable& drawable,
    std::string* error)
{
    if (!mImpl || !mImpl->layer || !mImpl->device)
    {
        setError(error, "Metal surface is unavailable");
        return false;
    }
    if (!drawable.mImpl || !drawable.mImpl->drawable)
    {
        setError(error, "Cannot present an empty Metal drawable");
        return false;
    }
    if (drawable.mImpl->presented)
    {
        setError(error, "Metal drawable was already presented");
        return false;
    }
    if (drawable.mImpl->generation != mInfo.generation)
    {
        setError(error, "Metal drawable belongs to an obsolete surface generation");
        return false;
    }
    if (command_buffer.isCommitted())
    {
        setError(error, "Cannot attach presentation to an already committed command buffer");
        return false;
    }
    if (command_buffer.nativeDevice() != (__bridge void*)mImpl->device)
    {
        setError(error, "Metal command buffer and drawable use different devices");
        return false;
    }

    id<MTLCommandBuffer> native_command = (__bridge id<MTLCommandBuffer>)command_buffer.nativeHandle();
    if (!native_command)
    {
        setError(error, "Metal command buffer has no native handle");
        return false;
    }
    [native_command presentDrawable:drawable.mImpl->drawable];
    drawable.mImpl->presented = true;
    return true;
}
