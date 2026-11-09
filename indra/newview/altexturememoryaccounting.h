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

    struct Pressure
    {
        float over_pct = 0.f;
        float free_mb = 0.f;
    };

    // Keep the original rounded-image/F32 arithmetic. Recovery requires both
    // known scopes to have headroom; absolute free MiB is only a UI estimate.
    constexpr Pressure evaluate(float rounded_image_used_mb,
                                float image_target_mb,
                                double global_used_mb,
                                double global_budget_mb) noexcept
    {
        const float image_free = image_target_mb - rounded_image_used_mb;
        const float image_over = image_target_mb > 0.f
            ? (rounded_image_used_mb - image_target_mb) / image_target_mb
            : 0.f;
        if (global_budget_mb <= 0.0)
        {
            return {image_over, image_free};
        }

        const float global_over = static_cast<float>(
            overageFraction(global_used_mb, global_budget_mb));
        const float global_free = static_cast<float>(
            global_budget_mb - global_used_mb);
        return {image_over > global_over ? image_over : global_over,
                image_free < global_free ? image_free : global_free};
    }
}
