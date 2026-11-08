/**
 * @file altexturememoryaccounting.h
 * @brief Backend-specific resource scopes for texture pressure.
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

namespace ALTextureMemoryAccounting
{
    struct Usage
    {
        double texture_pressure_mb = 0.0;
        double global_pressure_mb = 0.0;
        double global_budget_mb = 0.0;
    };

    // The original GL controller deliberately combined its incomplete texture
    // and vertex estimates because both occupied one dedicated VRAM pool. On
    // unified-memory Metal, the automatically detected texture allowance is a
    // bounded streamed-image policy, while currentAllocatedSize and
    // recommendedMaxWorkingSetSize both describe the complete device resource
    // set. Keep those two scopes paired instead of comparing all Metal
    // resources with the smaller texture allowance.
    constexpr Usage classify(bool metal,
                             double texture_mb,
                             double legacy_vertex_mb,
                             double device_mb,
                             double recommended_device_mb) noexcept
    {
        return metal
            ? Usage{ texture_mb, device_mb, recommended_device_mb }
            : Usage{ texture_mb + legacy_vertex_mb, 0.0, 0.0 };
    }

    constexpr double overageFraction(double used_mb,
                                     double target_mb) noexcept
    {
        return target_mb > 0.0
            ? (used_mb - target_mb) / target_mb
            : 0.0;
    }
}
