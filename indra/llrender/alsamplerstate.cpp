/**
 * @file alsamplerstate.cpp
 * @brief ALSamplerCache implementation
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
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

#include "linden_common.h"

#include "alsamplerstate.h"

#include "alrendercapabilities.h"
#include "llrender.h" // LLRender::sAnisotropicFilteringLevel

namespace
{
    // Indexed by the addressing field, already shifted down. The enum's numbering is chosen
    // to index this directly -- see ALSampler::Wrap/Mirror/Clamp.
    constexpr ALSamplerAddressMode sAddressMode[] =
    {
        ALSamplerAddressMode::repeat,
        ALSamplerAddressMode::mirror_repeat,
        ALSamplerAddressMode::clamp_to_edge,
    };

    // A contextless native renderer still needs stable sampler identities in
    // the retained slot caches. These values are never handed to GL; the
    // cache maps them back to the same descriptors used by Metal encoders.
    constexpr U32 LOGICAL_SAMPLER_BASE = 0x80000000u;
    constexpr U32 LOGICAL_CUSTOM_BASE  = 0xc0000000u;

    U32 logical_sampler(U32 slot)
    {
        return LOGICAL_SAMPLER_BASE | (slot + 1u);
    }
}

// static
ALSamplerDesc ALSamplerCache::makeDesc(ALSampler key)
{
    // Unpack the mask back into its fields. This is the only place that does: the mask is the
    // interface everywhere else, and the only reason the pieces exist here is that GL wants
    // them as separate parameters.
    const U16  filter  = alSamplerFilter(key);
    const U16  address = alSamplerAddress(key);
    const bool decode  = alSamplerHas(key, ALSampler::SRGBDecode);

    // The unreachable quarter of the address field. Reaching it means a mask was built by
    // arithmetic rather than by composing ALSampler values.
    llassert(address <= AL_SAMPLER_ADDRESS_MAX);

    ALSamplerDesc desc;

    desc.mMagFilter = filter == static_cast<U16>(ALSampler::Point)
        ? ALSamplerFilter::nearest : ALSamplerFilter::linear;

    // Trilinear and above get a linear mip filter, Bilinear a nearest one, Point stays nearest
    // in both. The asymmetry at Bilinear is deliberate and long-standing.
    //
    // Unconditional now. These used to collapse to the non-mipmapped filter when the
    // resource had no mip chain, because GL calls that combination incomplete and samples
    // black. Every texture carries immutable storage, which is mipmap-complete by
    // construction, so a mip filter on a one-level texture selects level 0 and needs no
    // reconciling -- see the note where ALSampler::HasMips used to be.
    if (filter >= static_cast<U16>(ALSampler::Trilinear))
    {
        desc.mMinFilter = ALSamplerFilter::linear;
        desc.mMipFilter = ALSamplerMipFilter::linear;
    }
    else if (filter >= static_cast<U16>(ALSampler::Bilinear))
    {
        desc.mMinFilter = ALSamplerFilter::linear;
        desc.mMipFilter = ALSamplerMipFilter::nearest;
    }
    else
    {
        desc.mMinFilter = ALSamplerFilter::nearest;
        desc.mMipFilter = ALSamplerMipFilter::nearest;
    }

    desc.mWrapS = desc.mWrapT = desc.mWrapR = sAddressMode[address];

    const bool has_anisotropy = gRenderCapabilities.isInitialized() &&
        gRenderCapabilities.hasAnisotropicFiltering();
    const F32 max_anisotropy = gRenderCapabilities.isInitialized()
        ? gRenderCapabilities.maxAnisotropy() : 1.f;
    if (has_anisotropy && filter == static_cast<U16>(ALSampler::Anisotropic)
        && LLRender::sAnisotropicFilteringLevel > 1.f)
    {
        desc.mMaxAnisotropy = llclamp(LLRender::sAnisotropicFilteringLevel, 1.f,
                                      max_anisotropy);
    }

    desc.mSRGBDecode = decode;

    return desc;
}

U32 ALSamplerCache::createSlot(ALSampler key)
{
    return logical_sampler(static_cast<U16>(key) & KEY_MASK);
}

U32 ALSamplerCache::get(const ALSamplerDesc& desc)
{
    for (const auto& entry : mCustomSamplers)
    {
        if (entry.first == desc)
        {
            return entry.second;
        }
    }

    const U32 name = LOGICAL_CUSTOM_BASE |
        (static_cast<U32>(mCustomSamplers.size()) + 1u);
    if (name)
    {
        mCustomSamplers.emplace_back(desc, name);
    }
    return name;
}

bool ALSamplerCache::describe(U32 name, ALSamplerDesc& desc) const
{
    if (!name)
    {
        return false;
    }

    for (U32 i = 0; i < NUM_SAMPLERS; ++i)
    {
        if (mSamplers[i] == name)
        {
            desc = makeDesc(static_cast<ALSampler>(i));
            return true;
        }
    }
    for (const auto& entry : mCustomSamplers)
    {
        if (entry.second == name)
        {
            desc = entry.first;
            return true;
        }
    }
    return false;
}

void ALSamplerCache::warmup()
{
    for (U32 i = 0; i < NUM_SAMPLERS; ++i)
    {
        // The address field has four encodings and only three meanings. Slots with the fourth
        // are unreachable by composing ALSampler values and stay empty; get() asserts on them,
        // which is how a mask built by arithmetic gets caught.
        if (alSamplerAddress(static_cast<ALSampler>(i)) > AL_SAMPLER_ADDRESS_MAX)
        {
            continue;
        }

        if (mSamplers[i] == 0)
        {
            mSamplers[i] = createSlot(static_cast<ALSampler>(i));
        }
    }
}

void ALSamplerCache::clear()
{
    mSamplers.fill(0);
    mCustomSamplers.clear();

    // Invalidate every cached resolution of a name from this cache. Skipping 0 keeps it
    // usable as the "never resolved" marker.
    if (++mGeneration == 0)
    {
        mGeneration = 1;
    }

    // NOTE: any per-unit binding that referenced these names is stale now. Releasing those
    // is LLRender::clearSamplers()' job -- it owns both sides.
}
