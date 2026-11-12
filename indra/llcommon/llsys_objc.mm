/**
 * @file llsys_objc.mm
 * @brief obj-c implementation of the system information functions
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2022, Linden Research, Inc.
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

#import "llsys_objc.h"
#import <Foundation/Foundation.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <mach/mach.h>
#include <pthread.h>
#include <sys/sysctl.h>
#include <time.h>
#include <utility>

namespace
{
bool readPositiveCPUCount(const char* name, uint32_t& value)
{
    int count = 0;
    size_t length = sizeof(count);
    if (sysctlbyname(name, &count, &length, nullptr, 0) != 0 ||
        length != sizeof(count) || count <= 0 || count > 1024)
    {
        return false;
    }
    value = static_cast<uint32_t>(count);
    return true;
}

bool readSystemCPUTicks(std::array<natural_t, CPU_STATE_MAX>& ticks)
{
    host_cpu_load_info_data_t info{};
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    const host_t host = mach_host_self();
    const kern_return_t status = host_statistics(
        host, HOST_CPU_LOAD_INFO, reinterpret_cast<host_info_t>(&info),
        &count);
    mach_port_deallocate(mach_task_self(), host);
    if (status != KERN_SUCCESS || count < HOST_CPU_LOAD_INFO_COUNT)
    {
        return false;
    }
    std::copy(std::begin(info.cpu_ticks), std::end(info.cpu_ticks),
              ticks.begin());
    return true;
}

bool readMainThreadCPUSeconds(double& seconds)
{
    thread_basic_info_data_t info{};
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    // The public sampler verifies this is the main thread before reaching here.
    const thread_t thread = mach_thread_self();
    const kern_return_t status = thread_info(
        thread, THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&info),
        &count);
    mach_port_deallocate(mach_task_self(), thread);
    if (status != KERN_SUCCESS || count < THREAD_BASIC_INFO_COUNT ||
        info.user_time.seconds < 0 || info.user_time.microseconds < 0 ||
        info.user_time.microseconds >= 1000000 ||
        info.system_time.seconds < 0 || info.system_time.microseconds < 0 ||
        info.system_time.microseconds >= 1000000)
    {
        return false;
    }
    seconds = static_cast<double>(info.user_time.seconds) +
        static_cast<double>(info.system_time.seconds) +
        (static_cast<double>(info.user_time.microseconds) +
         static_cast<double>(info.system_time.microseconds)) / 1000000.;
    return std::isfinite(seconds);
}

bool readMonotonicSeconds(double& seconds)
{
    timespec now{};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    {
        return false;
    }
    seconds = static_cast<double>(now.tv_sec) +
        static_cast<double>(now.tv_nsec) / 1000000000.;
    return std::isfinite(seconds);
}
}

bool LLGetDarwinOSInfo(int64_t &major, int64_t &minor, int64_t &patch)
{
        NSOperatingSystemVersion osVersion = [[NSProcessInfo processInfo] operatingSystemVersion];
        major = osVersion.majorVersion;
        minor = osVersion.minorVersion;
        patch = osVersion.patchVersion;

    return true;
}

LLDarwinCPUInfo LLGetDarwinCPUInfo()
{
    LLDarwinCPUInfo result;
    if (!readPositiveCPUCount("hw.logicalcpu", result.active_cores))
    {
        return result;
    }
    uint32_t level_count = 0;
    if (!readPositiveCPUCount("hw.nperflevels", level_count) ||
        level_count > 32 || level_count > result.active_cores)
    {
        return result;
    }

    std::vector<uint32_t> levels;
    levels.reserve(level_count);
    uint32_t total = 0;
    for (uint32_t level = 0; level < level_count; ++level)
    {
        char name[64];
        const int length = std::snprintf(
            name, sizeof(name), "hw.perflevel%u.physicalcpu", level);
        uint32_t cores = 0;
        if (length < 0 || static_cast<size_t>(length) >= sizeof(name) ||
            !readPositiveCPUCount(name, cores) ||
            cores > result.active_cores - total)
        {
            return result;
        }
        levels.push_back(cores);
        total += cores;
    }
    // This Apple Silicon policy needs a complete active-core partition.
    // Inconsistent/unavailable topology remains unknown, not an invented split.
    if (total == result.active_cores)
    {
        result.performance_levels = std::move(levels);
        result.topology_valid = true;
    }
    return result;
}

LLDarwinDecodePressure LLGetDarwinDecodePressure()
{
    LLDarwinDecodePressure result;
    if (!pthread_main_np())
    {
        return result;
    }
    @autoreleasepool
    {
        NSProcessInfo* process = [NSProcessInfo processInfo];
        if (@available(macOS 10.10.3, *))
        {
            const NSInteger state = process.thermalState;
            if (state >= NSProcessInfoThermalStateNominal &&
                state <= NSProcessInfoThermalStateCritical)
            {
                result.thermal_state = static_cast<int>(state);
            }
        }
        if (@available(macOS 12.0, *))
        {
            result.low_power = process.lowPowerModeEnabled;
        }
    }

    struct PreviousSample
    {
        std::array<natural_t, CPU_STATE_MAX> ticks{};
        double main_cpu_seconds = 0.;
        double wall_seconds = 0.;
        bool system_valid = false;
        bool main_valid = false;
    };
    // Constructed only by the first main-thread call; contains no ObjC objects.
    static PreviousSample previous;

    std::array<natural_t, CPU_STATE_MAX> ticks{};
    if (readSystemCPUTicks(ticks))
    {
        if (previous.system_valid)
        {
            uint64_t elapsed = 0;
            uint64_t busy = 0;
            for (size_t state = 0; state < ticks.size(); ++state)
            {
                // Mach's natural_t counters can wrap between samples.
                const natural_t delta = ticks[state] - previous.ticks[state];
                elapsed += delta;
                if (state != CPU_STATE_IDLE)
                {
                    busy += delta;
                }
            }
            if (elapsed != 0)
            {
                result.system_cpu_busy =
                    static_cast<double>(busy) / static_cast<double>(elapsed);
            }
        }
        previous.ticks = ticks;
        previous.system_valid = true;
    }
    else
    {
        previous.system_valid = false;
    }

    double wall_seconds = 0.;
    double cpu_seconds = 0.;
    if (readMonotonicSeconds(wall_seconds) &&
        readMainThreadCPUSeconds(cpu_seconds))
    {
        if (previous.main_valid && wall_seconds > previous.wall_seconds &&
            cpu_seconds >= previous.main_cpu_seconds)
        {
            result.main_thread_cpu_busy = std::clamp(
                (cpu_seconds - previous.main_cpu_seconds) /
                (wall_seconds - previous.wall_seconds), 0., 1.);
        }
        previous.wall_seconds = wall_seconds;
        previous.main_cpu_seconds = cpu_seconds;
        previous.main_valid = true;
    }
    else
    {
        previous.main_valid = false;
    }
    return result;
}
