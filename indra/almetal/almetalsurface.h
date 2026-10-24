/**
 * @file almetalsurface.h
 * @brief SDL Metal view and CAMetalLayer drawable lifecycle.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#pragma once

#include "almetaldevice.h"

#include <cstdint>
#include <memory>
#include <string>

struct SDL_Window;

enum class ALMetalSurfaceFormat
{
    bgra8_unorm,
    bgra8_unorm_srgb,
    bgr10_xr,
    rgba16_float,
};

struct ALMetalSurfaceConfig
{
    ALMetalSurfaceFormat format = ALMetalSurfaceFormat::bgra8_unorm;
    std::uint32_t maximum_drawable_count = 3;
    bool display_sync_enabled = true;
    bool framebuffer_only = true;
    bool allows_next_drawable_timeout = true;
    bool extended_dynamic_range = false;
};

struct ALMetalSurfaceInfo
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t generation = 0;
    ALMetalSurfaceFormat format = ALMetalSurfaceFormat::bgra8_unorm;
    std::uint32_t maximum_drawable_count = 0;
    bool display_sync_enabled = true;
    bool framebuffer_only = true;
    bool extended_dynamic_range = false;
};

class ALMetalDrawable final
{
public:
    ALMetalDrawable(const ALMetalDrawable&) = delete;
    ALMetalDrawable& operator=(const ALMetalDrawable&) = delete;
    ALMetalDrawable(ALMetalDrawable&&) = delete;
    ALMetalDrawable& operator=(ALMetalDrawable&&) = delete;
    ~ALMetalDrawable();

    // Non-owning Objective-C handles, valid for this drawable's lifetime.
    void* nativeHandle() const noexcept;
    void* nativeTexture() const noexcept;
    std::uint32_t width() const noexcept;
    std::uint32_t height() const noexcept;
    std::uint64_t generation() const noexcept;
    bool wasPresented() const noexcept;

private:
    struct Impl;
    explicit ALMetalDrawable(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> mImpl;

    friend class ALMetalSurface;
};

class ALMetalSurface final
{
public:
    ALMetalSurface(const ALMetalSurface&) = delete;
    ALMetalSurface& operator=(const ALMetalSurface&) = delete;
    ALMetalSurface(ALMetalSurface&&) = delete;
    ALMetalSurface& operator=(ALMetalSurface&&) = delete;
    ~ALMetalSurface();

    // Creation, mutation and destruction of the SDL view happen on the main
    // thread. Drawable acquisition and command encoding may happen elsewhere.
    static std::unique_ptr<ALMetalSurface> create(
        SDL_Window* window,
        const ALMetalDevice& device,
        const ALMetalSurfaceConfig& config = {},
        std::string* error = nullptr);

    const ALMetalSurfaceInfo& info() const noexcept;
    void* nativeLayer() const noexcept;

    bool resizeFromWindow(std::string* error = nullptr);
    bool resize(std::uint32_t width, std::uint32_t height, std::string* error = nullptr);
    bool setDisplaySyncEnabled(bool enabled, std::string* error = nullptr);

    std::unique_ptr<ALMetalDrawable> nextDrawable(std::string* error = nullptr);

    // Encode presentation before committing. The command buffer and drawable
    // remain owned by the caller so completion/error state can be observed.
    bool present(
        ALMetalCommandBuffer& command_buffer,
        ALMetalDrawable& drawable,
        std::string* error = nullptr);

private:
    struct Impl;
    explicit ALMetalSurface(std::unique_ptr<Impl> impl, ALMetalSurfaceInfo info);
    std::unique_ptr<Impl> mImpl;
    ALMetalSurfaceInfo mInfo;
};
