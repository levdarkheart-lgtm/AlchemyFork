/**
 * @file llcoreaudiooutputdevice.h
 * @brief Resolve the viewer's sound output name to a native device UID.
 *
 * Copyright (C) 2026 Alchemy Viewer contributors.
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#ifndef LL_COREAUDIOOUTPUTDEVICE_H
#define LL_COREAUDIOOUTPUTDEVICE_H

#include <string>

// An empty preference follows the system output. Ambiguous or unavailable
// display names also return empty: guessing a duplicate device can send music
// to an unintended endpoint. This never changes the system or voice device.
std::string llCoreAudioOutputDeviceUID(const std::string& output_name);

#endif
