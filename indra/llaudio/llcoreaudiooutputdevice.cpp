/**
 * @file llcoreaudiooutputdevice.cpp
 * @brief Resolve FAudio's persisted output display name through Core Audio.
 *
 * Copyright (C) 2026 Alchemy Viewer contributors.
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#include "linden_common.h"
#include "llcoreaudiooutputdevice.h"

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <vector>

#include "llerror.h"

namespace
{
std::string deviceString(AudioObjectID device, AudioObjectPropertySelector selector,
                         AudioObjectPropertyScope scope)
{
    const AudioObjectPropertyAddress address{selector, scope, kAudioObjectPropertyElementMain};
    CFStringRef value = nullptr;
    UInt32 size = sizeof(value);
    if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, &value) != noErr || !value)
        return {};
    const CFIndex capacity = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value),
                                                              kCFStringEncodingUTF8) + 1;
    std::vector<char> bytes(static_cast<std::size_t>(capacity));
    const bool converted = CFStringGetCString(value, bytes.data(), capacity, kCFStringEncodingUTF8);
    CFRelease(value);
    return converted ? std::string(bytes.data()) : std::string();
}

bool hasOutput(AudioObjectID device)
{
    const AudioObjectPropertyAddress address{kAudioDevicePropertyStreamConfiguration,
                                            kAudioDevicePropertyScopeOutput,
                                            kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(device, &address, 0, nullptr, &size) != noErr ||
        size < sizeof(AudioBufferList)) return false;
    // AudioBufferList contains a variable-length buffer array.
    std::vector<UInt64> storage((size + sizeof(UInt64) - 1) / sizeof(UInt64));
    auto* buffers = reinterpret_cast<AudioBufferList*>(storage.data());
    if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, buffers) != noErr)
        return false;
    for (UInt32 i = 0; i < buffers->mNumberBuffers; ++i)
        if (buffers->mBuffers[i].mNumberChannels) return true;
    return false;
}
}

std::string llCoreAudioOutputDeviceUID(const std::string& output_name)
{
    if (output_name.empty()) return {};
    const AudioObjectPropertyAddress address{kAudioHardwarePropertyDevices,
                                            kAudioObjectPropertyScopeGlobal,
                                            kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &size) != noErr)
        return {};
    std::vector<AudioObjectID> devices(size / sizeof(AudioObjectID));
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr,
                                  &size, devices.data()) != noErr) return {};
    devices.resize(size / sizeof(AudioObjectID));
    std::string uid;
    unsigned matches = 0;
    for (const auto device : devices)
    {
        if (!hasOutput(device)) continue;
        std::string name = deviceString(device, kAudioObjectPropertyName,
                                        kAudioDevicePropertyScopeOutput);
        // SDL's Core Audio enumeration removes trailing spaces before FAudio
        // exposes the name as the stable preference ID.
        while (!name.empty() && name.back() == ' ') name.pop_back();
        if (name != output_name) continue;
        ++matches;
        uid = deviceString(device, kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal);
    }
    if (matches == 1 && !uid.empty()) return uid;
    LL_WARNS("AudioEngine") << "Native parcel music cannot uniquely resolve the selected output; "
                             "following the system output." << LL_ENDL;
    return {};
}
