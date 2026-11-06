/**
 * @file llcefaccelinterop.h
 * @brief Viewer-side consumer of the CEF accelerated-paint shared texture.
 *
 * Imports the plugin's IOSurface directly into Metal without a CPU round trip.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Alchemy Viewer Project.
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
 * $/LicenseInfo$
 */

#ifndef LL_LLCEFACCELINTEROP_H
#define LL_LLCEFACCELINTEROP_H

#include <string>

class ALMetalTexture;

// Bridges the plugin's shared GPU texture to the viewer's native media texture
// with no CPU copy.
class LLCEFAccelInterop
{
public:
    LLCEFAccelInterop() = default;
    ~LLCEFAccelInterop();

    // Create the native import/copy state. False if unsupported (caller should
    // fall back to the CPU paint path).
    bool init();
    void shutdown();
    bool valid() const { return mValid; }

    // True when the published Metal device can consume the plugin's shared
    // texture. Callers request accelerated paint only after this probe passes.
    static bool isSupported();

    // (Re)bind the plugin's retained IOSurfaceRef. The remaining parameters
    // preserve the media-plugin callback ABI and are ignored by this consumer.
    bool setStableTexture(unsigned long long handle, int width, int height,
                          int format = 0, unsigned int stride = 0,
                          unsigned long long offset = 0, unsigned long long modifier = 0,
                          int src_pid = 0, int plane_count = 1,
                          const unsigned long long* plane_fds = nullptr,
                          const unsigned int* plane_strides = nullptr,
                          const unsigned long long* plane_offsets = nullptr);

    // Flip the imported IOSurface's top-down rows into
    // the viewer's bottom-up media-texture convention. The destination must be
    // a compatible shader-read/render-target texture.
    bool blitToMetal(ALMetalTexture& destination,
                     std::string* error = nullptr);

private:
    bool mValid = false;
    void* mImpl = nullptr;   // platform import/copy state
};

#endif // LL_LLCEFACCELINTEROP_H
