/**
 * @file llviewermedia_streamingaudio.h
 * @author Tofu Linden
 * @brief Startup compatibility name for native parcel music.
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#ifndef LL_VIEWERMEDIA_STREAMINGAUDIO_H
#define LL_VIEWERMEDIA_STREAMINGAUDIO_H

#include "llstreamingaudio_coreaudio.h"

// Keep the startup caller's legacy name while startup preparation is being
// changed independently. Both the engine factory and this compatibility route
// use native parcel music; the old force-plugin preference cannot launch VLC.
// Video and browser media continue to use their separate plugin owners.
using LLStreamingAudio_MediaPlugins = LLStreamingAudio_CoreAudio;

#endif // LL_VIEWERMEDIA_STREAMINGAUDIO_H
