/**
 * @file altexturepressurequeue.h
 * @brief Stable native-first draining for the fetched-texture pressure queue.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

#ifndef AL_TEXTURE_PRESSURE_QUEUE_H
#define AL_TEXTURE_PRESSURE_QUEUE_H

#include <cstddef>
#include <utility>

namespace ALTexturePressureQueue
{
    enum class DrainResult
    {
        Complete,
        BudgetExhausted
    };

    // Inspect only the entries present at entry. Native entries are consumed;
    // retained and unexamined entries keep their original relative order even
    // when the budget expires part-way through the scan.
    template <typename Queue, typename IsNative, typename DrainNative,
              typename BudgetExpired>
    DrainResult drainNative(Queue& queue,
                            std::size_t minimum_scans,
                            IsNative&& is_native,
                            DrainNative&& drain_native,
                            BudgetExpired&& budget_expired)
    {
        Queue retained;
        const std::size_t initial_count = queue.size();
        std::size_t scanned = 0;
        DrainResult result = DrainResult::Complete;

        while (scanned < initial_count)
        {
            auto entry = std::move(queue.front());
            queue.pop();
            ++scanned;

            if (is_native(entry))
            {
                drain_native(entry);
            }
            else
            {
                retained.push(std::move(entry));
            }

            if (budget_expired() && scanned >= minimum_scans)
            {
                result = DrainResult::BudgetExhausted;
                break;
            }
        }

        while (!queue.empty())
        {
            retained.push(std::move(queue.front()));
            queue.pop();
        }
        queue = std::move(retained);
        return result;
    }
}

#endif // AL_TEXTURE_PRESSURE_QUEUE_H
