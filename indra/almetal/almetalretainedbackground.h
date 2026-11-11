/**
 * @file almetalretainedbackground.h
 * @brief Native pre-UI world backing, independent of per-frame UI output.
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

#include "almetalresource.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>

class ALMetalRetainedBackground
{
public:
    ALMetalRetainedBackground();
    ~ALMetalRetainedBackground();
    struct Context
    {
        const ALMetalDevice* device = nullptr;
        const void* surface_identity = nullptr;
        std::uint64_t surface_generation = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool reverse_z = false;
        bool operator==(const Context&) const = default;
    };
    struct Record
    {
        Context context;
        // Reserved for a cleared UI backing, never a completed world frame.
        std::uint64_t source_frame = std::numeric_limits<std::uint64_t>::max();
        const ALMetalTexture* colorTexture() const noexcept { return color.get(); }
        const ALMetalTexture* depthTexture() const noexcept { return depth.get(); }
    private:
        std::shared_ptr<const ALMetalRenderTarget> color_owner, depth_owner;
        std::unique_ptr<ALMetalTexture> color, depth;
        friend class ALMetalRetainedBackground;
    };
    using Lease = std::shared_ptr<const Record>;

    // Caller has committed the coherent pre-UI world. Target leases pin pool
    // ownership; independent texture wrappers pin borrowed native storage.
    // Caller must exclude these native handles from future borrowed writes.
    bool publish(const Context& context, std::uint64_t source_frame,
                 std::shared_ptr<const ALMetalRenderTarget> color_owner,
                 std::shared_ptr<const ALMetalRenderTarget> depth_owner,
                 std::string* error = nullptr);
    Lease find(const Context& context) const noexcept;
    void clear() noexcept { mRecord.reset(); }
    std::uint64_t shaderReloadGeneration() const noexcept;

    // One current-size immutable clear target for UI/HUD while no compatible
    // world exists. Commits its clear on the same native queue, never waits,
    // and does not replace the cache's completed-world publication.
    static Lease makeCleared(const Context& context, std::string* error = nullptr);

private:
    struct ScaleState;
    Lease resample(const Context&, std::uint64_t,
                   std::shared_ptr<const ALMetalRenderTarget>,
                   std::shared_ptr<const ALMetalRenderTarget>, std::string*);
    static Lease retain(const Context&, std::uint64_t,
                        std::shared_ptr<const ALMetalRenderTarget>,
                        std::shared_ptr<const ALMetalRenderTarget>,
                        std::string*);
    Lease mRecord;
    std::unique_ptr<ScaleState> mScale;
};
