/**
 * @file llcefaccelinterop.cpp
 * @brief Viewer-side consumer of the CEF accelerated-paint shared texture.
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

#include "llviewerprecompiledheaders.h"

#include "llcefaccelinterop.h"
#include "llviewerwindow.h"

#include "almetalresource.h"
#include "almetaltextureops.h"
#include "llimagegl.h"
#include <IOSurface/IOSurface.h>

namespace
{
    struct MacAccel
    {
        IOSurfaceRef surface = nullptr;
        const ALMetalDevice* metal_device = nullptr;
        std::unique_ptr<ALMetalTexture> metal_source;
        std::unique_ptr<ALMetalTextureResampler> metal_resampler;
    };
}

LLCEFAccelInterop::~LLCEFAccelInterop()
{
    shutdown();
}

bool LLCEFAccelInterop::init()
{
    MacAccel* m = new MacAccel();
    m->metal_device = getViewerMetalDevice();
    if (!m->metal_device)
    {
        LL_WARNS("Media")
            << "accelerated paint: no published Metal device; using CPU paint"
            << LL_ENDL;
        delete m;
        return false;
    }
    std::string error;
    m->metal_resampler = ALMetalTextureResampler::create(
        *m->metal_device, &error);
    if (!m->metal_resampler)
    {
        LL_WARNS("Media")
            << "accelerated paint: native Metal media copier is unavailable; "
               "using CPU paint: " << error << LL_ENDL;
        delete m;
        return false;
    }
    mImpl = m;
    mValid = true;
    return true;
}

void LLCEFAccelInterop::shutdown()
{
    if (!mImpl)
    {
        return;
    }
    MacAccel* m = (MacAccel*)mImpl;
    m->metal_source.reset();
    m->metal_resampler.reset();
    if (m->surface) { CFRelease(m->surface); }
    delete m;
    mImpl = nullptr;
    mValid = false;
}

bool LLCEFAccelInterop::setStableTexture(unsigned long long handle, int width, int height,
                                         int format, unsigned int stride,
                                         unsigned long long offset, unsigned long long modifier, int src_pid,
                                         int plane_count, const unsigned long long* plane_fds,
                                         const unsigned int* plane_strides, const unsigned long long* plane_offsets)
{
    (void)width; (void)height; (void)format; (void)stride;
    (void)offset; (void)modifier; (void)src_pid;
    (void)plane_count; (void)plane_fds; (void)plane_strides; (void)plane_offsets;
    if (!mValid || !handle)
    {
        return false;
    }
    MacAccel* m = (MacAccel*)mImpl;

    // The "handle" is an already-resolved IOSurfaceRef (+1 retained) handed over
    // by the mach receiver (LLCEFSurfaceReceiver) - CEF shares its IOSurface via a
    // mach port, which can't be resolved from a cross-process global id, so the
    // surface arrives out-of-band rather than as an IOSurfaceID. We take ownership.
    IOSurfaceRef surf = (IOSurfaceRef)(uintptr_t)handle;
    if (!surf)
    {
        return false;
    }
    // Only swap in the new surface once its native texture exists, so a failed
    // import leaves the previous complete source intact.
    const int new_width = (int)IOSurfaceGetWidth(surf);
    const int new_height = (int)IOSurfaceGetHeight(surf);

    if (!m->metal_device || !m->metal_resampler ||
        new_width <= 0 || new_height <= 0)
    {
        CFRelease(surf);
        return false;
    }

    ALMetalTextureDescriptor descriptor;
    descriptor.format = ALMetalPixelFormat::bgra8_unorm;
    descriptor.width = static_cast<U32>(new_width);
    descriptor.height = static_cast<U32>(new_height);
    descriptor.storage = ALMetalStorageMode::shared;
    descriptor.usage = ALMetalTextureUsage::shader_read;
    descriptor.label = "Alchemy accelerated CEF IOSurface";
    std::string error;
    std::unique_ptr<ALMetalTexture> metal_source =
        ALMetalTexture::createFromIOSurface(
            *m->metal_device, surf, 0, descriptor, &error);
    if (!metal_source)
    {
        LL_WARNS("Media")
            << "accelerated paint: IOSurface Metal import failed; "
               "exact native frames will fail closed: "
            << error << LL_ENDL;
        CFRelease(surf);
        return false;
    }

    IOSurfaceRef previous_surface = m->surface;
    m->metal_source = std::move(metal_source);
    m->surface = surf;
    if (previous_surface) { CFRelease(previous_surface); }
    return true;
}

bool LLCEFAccelInterop::blitToMetal(ALMetalTexture& destination,
                                    std::string* error)
{
    if (!mValid || !mImpl)
    {
        if (error)
        {
            *error = "Accelerated media interop is unavailable";
        }
        return false;
    }
    MacAccel* m = static_cast<MacAccel*>(mImpl);
    if (!m->metal_device || !m->metal_source || !m->metal_resampler)
    {
        if (error)
        {
            *error = "Accelerated media IOSurface has no Metal import";
        }
        return false;
    }
    const ALMetalTextureDescriptor& source = m->metal_source->descriptor();
    const ALMetalTextureDescriptor& output = destination.descriptor();
    if (source.width != output.width || source.height != output.height ||
        source.format != output.format ||
        !alMetalHasUsage(output.usage, ALMetalTextureUsage::shader_read) ||
        !alMetalHasUsage(output.usage, ALMetalTextureUsage::render_target))
    {
        if (error)
        {
            *error = "Accelerated media Metal source and destination are incompatible";
        }
        return false;
    }

    auto command = m->metal_device->makeCommandBuffer(
        "Alchemy accelerated CEF Metal media copy", error);
    return command &&
           m->metal_resampler->encode(
               *command, *m->metal_source, destination, true, error) &&
           command->commit(error);
}

// Static capability preflight. Probe once so callers request accelerated
// paint only when the native IOSurface consumer is ready.
bool LLCEFAccelInterop::isSupported()
{
    static bool s_probed = false;
    static bool s_supported = false;
    if (!s_probed)
    {
        LLCEFAccelInterop probe;
        s_supported = probe.init();
        probe.shutdown();
        s_probed = true;
    }
    return s_supported;
}
